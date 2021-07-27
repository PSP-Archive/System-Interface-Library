/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/graphics/primitive.c: Graphics primitive rendering and management.
 */

#include "src/base.h"
#include "src/graphics.h"
#include "src/graphics/internal.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/utility/id-array.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Array of primitive objects. */
static IDArray primitives = ID_ARRAY_INITIALIZER(100);

/* Are we currently constructing a primitive?  (This flag is always set
 * between graphics_begin_primitive() and graphics_end_primitive(), even if
 * an error occurs.) */
static uint8_t in_primitive;

/* Has an error occurred during construction of the current primitive? */
static uint8_t primitive_error;

/* What data is included in vertices?  (These are set when the first vertex
 * is registered.) */
static uint8_t primitive_has_texcoord;
static uint8_t primitive_has_color;

/* Data for the current primitive.  To avoid unnecessary memory allocation,
 * we use a static array for the format data, since we can never have more
 * than three data elements (position, texture coordinate, color). */
static GraphicsPrimitiveType primitive_type;
static float *primitive_data;         // All values are 32-bit floats.
static uint32_t primitive_format[4];  // 3 + 1 for the zero terminator.
static int primitive_size;            // Vertex data size in bytes.
static int primitive_count;           // Number of vertices registered.

/* Current size of the primitive data buffer, in bytes. */
static int primitive_data_bufsize;

/* Size (in bytes) by which we expand the primitive data buffer when needed.
 * This also serves as the size of the static buffer defined below. */
#define PRIMITIVE_DATA_EXPAND  1024

/* Static buffer used as the initial vertex data buffer, to avoid memory
 * allocation operations for small primitives. */
static float primitive_data_static_buffer[PRIMITIVE_DATA_EXPAND/4];

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * do_end_primitive:  Common processing for graphics_end_primitive() and
 * graphics_end_and_draw_primitive().
 *
 * [Parameters]
 *     immediate: True to draw immediately, false to create a primitive object.
 * [Return value]
 *     Value to return to caller's caller.
 */
static int do_end_primitive(int immediate);

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int graphics_begin_primitive(GraphicsPrimitiveType type)
{
    if (UNLIKELY(in_primitive)) {
        DLOG("Already creating a primitive!");
        /* Abort the current primitive as well as failing this one, in case
         * the caller ignores this failure and starts dumping vertex data
         * on us. */
        primitive_error = 1;
        return 0;
    }

    int ok = 0;
    switch (type) {
      case GRAPHICS_PRIMITIVE_POINTS:
      case GRAPHICS_PRIMITIVE_LINES:
      case GRAPHICS_PRIMITIVE_LINE_STRIP:
      case GRAPHICS_PRIMITIVE_TRIANGLES:
      case GRAPHICS_PRIMITIVE_TRIANGLE_STRIP:
      case GRAPHICS_PRIMITIVE_QUADS:
      case GRAPHICS_PRIMITIVE_QUAD_STRIP:
        ok = 1;
        break;
    }
    if (UNLIKELY(!ok)) {
        DLOG("Invalid primitive type %d", type);
        return 0;
    }

    in_primitive = 1;
    primitive_error = 0;
    primitive_type = type;
    primitive_count = 0;
    primitive_data = primitive_data_static_buffer;
    primitive_data_bufsize = sizeof(primitive_data_static_buffer);
    return 1;
}

/*-----------------------------------------------------------------------*/

int graphics_add_vertex(const Vector3f *position,
                        const Vector2f *texcoord,
                        const Vector4f *color)
{
    if (UNLIKELY(!in_primitive)) {
        DLOG("Not constructing a primitive");
        return 0;
    }
    if (UNLIKELY(primitive_error)) {
        return 0;
    }

    if (UNLIKELY(!position)) {
        DLOG("Vertex position missing");
        primitive_error = 1;
        return 0;
    }

    if (primitive_count == 0) {

        /* This is the first vertex, so generate the vertex format data. */

        primitive_has_texcoord = (texcoord != NULL);
        primitive_has_color = (color != NULL);

        int i = 0;
        primitive_size = 0;
        primitive_format[i++] =
            GRAPHICS_VERTEX_FORMAT(POSITION_3F, primitive_size);
        primitive_size += 4*3;
        if (primitive_has_texcoord) {
            primitive_format[i++] =
                GRAPHICS_VERTEX_FORMAT(TEXCOORD_2F, primitive_size);
            primitive_size += 4*2;
        }
        if (primitive_has_color) {
            primitive_format[i++] =
                GRAPHICS_VERTEX_FORMAT(COLOR_4F, primitive_size);
            primitive_size += 4*4;
        }
        primitive_format[i] = 0;

    } else {

        /* This is the second or a later vertex, so verify that the data
         * we were given matches the vertex format. */

        if (primitive_has_texcoord) {
            if (UNLIKELY(!texcoord)) {
                DLOG("Texture coordinate missing");
                primitive_error = 1;
                return 0;
            }
        } else {
            if (UNLIKELY(texcoord)) {
                DLOG("Texture coordinate given, but not in vertex format");
                primitive_error = 1;
                return 0;
            }
        }
        if (primitive_has_color) {
            if (UNLIKELY(!color)) {
                DLOG("Color missing");
                primitive_error = 1;
                return 0;
            }
        } else {
            if (UNLIKELY(color)) {
                DLOG("Color given, but not in vertex format");
                primitive_error = 1;
                return 0;
            }
        }

    }

    /* Expand the vertex data buffer if necessary. */

    int new_bufsize = (primitive_count + 1) * primitive_size;
    if (new_bufsize > primitive_data_bufsize) {
        new_bufsize += PRIMITIVE_DATA_EXPAND;
        float *new_data;
        int need_copy;
        if (primitive_data == primitive_data_static_buffer) {
            new_data = mem_alloc(new_bufsize, 4, MEM_ALLOC_TEMP);
            need_copy = 1;
        } else {
            new_data = mem_realloc(primitive_data, new_bufsize, MEM_ALLOC_TEMP);
            need_copy = 0;
        }
        if (UNLIKELY(!new_data)) {
            DLOG("Out of memory for vertex buffer at %u vertices * %u bytes",
                 primitive_count + 1, primitive_size);
            primitive_error = 1;
            return 0;
        }
        if (need_copy) {
            memcpy(new_data, primitive_data, primitive_data_bufsize);
        }
        primitive_data = new_data;
        primitive_data_bufsize = new_bufsize;
    }

    /* Append the vertex data to the data buffer. */

    float *ptr = primitive_data + (primitive_count * primitive_size/4);
    *ptr++ = position->x;
    *ptr++ = position->y;
    *ptr++ = position->z;
    if (texcoord) {
        *ptr++ = texcoord->x;
        *ptr++ = texcoord->y;
    }
    if (color) {
        *ptr++ = color->x;
        *ptr++ = color->y;
        *ptr++ = color->z;
        *ptr++ = color->w;
    }
    primitive_count++;

    return 1;
}

/*-----------------------------------------------------------------------*/

int graphics_end_primitive(void)
{
    return do_end_primitive(0);
}

/*-----------------------------------------------------------------------*/

int graphics_end_and_draw_primitive(void)
{
    return do_end_primitive(1);
}

/*-----------------------------------------------------------------------*/

int graphics_create_primitive(
    GraphicsPrimitiveType type, const void *data, const uint32_t *format,
    int size, int count)
{
    int ok = 0;
    switch (type) {
      case GRAPHICS_PRIMITIVE_POINTS:
      case GRAPHICS_PRIMITIVE_LINES:
      case GRAPHICS_PRIMITIVE_LINE_STRIP:
      case GRAPHICS_PRIMITIVE_TRIANGLES:
      case GRAPHICS_PRIMITIVE_TRIANGLE_STRIP:
      case GRAPHICS_PRIMITIVE_QUADS:
      case GRAPHICS_PRIMITIVE_QUAD_STRIP:
        ok = 1;
        break;
    }
    if (UNLIKELY(!ok)
     || UNLIKELY(!data)
     || UNLIKELY(!format)
     || UNLIKELY(size == 0)
     || UNLIKELY(count == 0)) {
        DLOG("Invalid parameters: %d %p %p %u %u", type, data, format,
             size, count);
        return 0;
    }

    SysPrimitive *primitive = sys_graphics_create_primitive(
        type, data, format, size, count, NULL, 0, 0, 0);
    if (UNLIKELY(!primitive)) {
        DLOG("Failed to create primitive object");
        return 0;
    }

    const int id = id_array_register(&primitives, primitive);
    if (UNLIKELY(!id)) {
        DLOG("Failed to register primitive object");
        sys_graphics_destroy_primitive(primitive);
        return 0;
    }

    return id;
}

/*-----------------------------------------------------------------------*/

int graphics_create_indexed_primitive(
    GraphicsPrimitiveType type, const void *vertex_data,
    const uint32_t *vertex_format, int vertex_size, int vertex_count,
    const void *index_data, int index_size, int index_count)
{
    int ok = 0;
    switch (type) {
      case GRAPHICS_PRIMITIVE_POINTS:
      case GRAPHICS_PRIMITIVE_LINES:
      case GRAPHICS_PRIMITIVE_LINE_STRIP:
      case GRAPHICS_PRIMITIVE_TRIANGLES:
      case GRAPHICS_PRIMITIVE_TRIANGLE_STRIP:
      case GRAPHICS_PRIMITIVE_QUADS:
      case GRAPHICS_PRIMITIVE_QUAD_STRIP:
        ok = 1;
        break;
    }
    if (UNLIKELY(!ok)
     || UNLIKELY(!vertex_data)
     || UNLIKELY(!vertex_format)
     || UNLIKELY(vertex_size == 0)
     || UNLIKELY(vertex_count == 0)
     || UNLIKELY(!index_data)
     || UNLIKELY(index_size != 1 && index_size != 2 && index_size != 4)
     || UNLIKELY(index_count == 0)) {
        DLOG("Invalid parameters: %d %p %p %u %u %p %u %u",
             type, vertex_data, vertex_format, vertex_size, vertex_count,
             index_data, index_size, index_count);
        return 0;
    }

    SysPrimitive *primitive = sys_graphics_create_primitive(
        type, vertex_data, vertex_format, vertex_size, vertex_count,
        index_data, index_size, index_count, 0);
    if (UNLIKELY(!primitive)) {
        DLOG("Failed to create primitive object");
        return 0;
    }

    const int id = id_array_register(&primitives, primitive);
    if (UNLIKELY(!id)) {
        DLOG("Failed to register primitive object");
        sys_graphics_destroy_primitive(primitive);
        return 0;
    }

    return id;
}

/*-----------------------------------------------------------------------*/

void graphics_draw_primitive(int primitive)
{
    SysPrimitive *sys_primitive = id_array_get(&primitives, primitive);
    if (UNLIKELY(!sys_primitive)) {
        DLOG("Invalid primitive ID %d", primitive);
        return;
    }

    sys_graphics_draw_primitive(sys_primitive, 0, -1);
}

/*-----------------------------------------------------------------------*/

void graphics_draw_primitive_partial(int primitive, int start, int count)
{
    SysPrimitive *sys_primitive = id_array_get(&primitives, primitive);
    if (UNLIKELY(!sys_primitive)) {
        DLOG("Invalid primitive ID %d", primitive);
        return;
    }
    if (UNLIKELY(start < 0)) {
        DLOG("Invalid start index %d", start);
        return;
    }

    sys_graphics_draw_primitive(sys_primitive, start, count);
}

/*-----------------------------------------------------------------------*/

void graphics_draw_vertices(
    GraphicsPrimitiveType type, const void *data, const uint32_t *format,
    int size, int count)
{
    int ok = 0;
    switch (type) {
      case GRAPHICS_PRIMITIVE_POINTS:
      case GRAPHICS_PRIMITIVE_LINES:
      case GRAPHICS_PRIMITIVE_LINE_STRIP:
      case GRAPHICS_PRIMITIVE_TRIANGLES:
      case GRAPHICS_PRIMITIVE_TRIANGLE_STRIP:
      case GRAPHICS_PRIMITIVE_QUADS:
      case GRAPHICS_PRIMITIVE_QUAD_STRIP:
        ok = 1;
        break;
    }
    if (UNLIKELY(!ok)
     || UNLIKELY(!data)
     || UNLIKELY(!format)
     || UNLIKELY(size == 0)
     || UNLIKELY(count == 0)) {
        DLOG("Invalid parameters: %d %p %p %u %u", type, data, format,
             size, count);
        return;
    }

    SysPrimitive *primitive = sys_graphics_create_primitive(
        type, data, format, size, count, NULL, 0, 0, 1);
    if (UNLIKELY(!primitive)) {
        DLOG("Failed to create primitive object");
        return;
    }
    sys_graphics_draw_primitive(primitive, 0, -1);
    sys_graphics_destroy_primitive(primitive);
}

/*-----------------------------------------------------------------------*/

void graphics_draw_indexed_vertices(
    GraphicsPrimitiveType type, const void *vertex_data,
    const uint32_t *vertex_format, int vertex_size, int vertex_count,
    const void *index_data, int index_size, int index_count)
{
    int ok = 0;
    switch (type) {
      case GRAPHICS_PRIMITIVE_POINTS:
      case GRAPHICS_PRIMITIVE_LINES:
      case GRAPHICS_PRIMITIVE_LINE_STRIP:
      case GRAPHICS_PRIMITIVE_TRIANGLES:
      case GRAPHICS_PRIMITIVE_TRIANGLE_STRIP:
      case GRAPHICS_PRIMITIVE_QUADS:
      case GRAPHICS_PRIMITIVE_QUAD_STRIP:
        ok = 1;
        break;
    }
    if (UNLIKELY(!ok)
     || UNLIKELY(!vertex_data)
     || UNLIKELY(!vertex_format)
     || UNLIKELY(vertex_size == 0)
     || UNLIKELY(vertex_count == 0)
     || UNLIKELY(!index_data)
     || UNLIKELY(index_size != 1 && index_size != 2 && index_size != 4)
     || UNLIKELY(index_count == 0)) {
        DLOG("Invalid parameters: %d %p %p %u %u %p %u %u",
             type, vertex_data, vertex_format, vertex_size, vertex_count,
             index_data, index_size, index_count);
        return;
    }

    SysPrimitive *primitive = sys_graphics_create_primitive(
        type, vertex_data, vertex_format, vertex_size, vertex_count,
        index_data, index_size, index_count, 1);
    if (UNLIKELY(!primitive)) {
        DLOG("Failed to create primitive object");
        return;
    }
    sys_graphics_draw_primitive(primitive, 0, -1);
    sys_graphics_destroy_primitive(primitive);
}

/*-----------------------------------------------------------------------*/

void graphics_destroy_primitive(int primitive)
{
    if (primitive == 0) {
        return;
    }

    SysPrimitive *sys_primitive = id_array_get(&primitives, primitive);
    if (UNLIKELY(!sys_primitive)) {
        DLOG("Invalid primitive ID %d", primitive);
        return;
    }

    sys_graphics_destroy_primitive(sys_primitive);
    id_array_release(&primitives, primitive);
}

/*************************************************************************/
/*********************** Library-internal routines ***********************/
/*************************************************************************/

void primitive_cleanup(void)
{
    for (int i = 1; i < id_array_size(&primitives); i++) {
        if (id_array_get(&primitives, i)) {
            graphics_destroy_primitive(i);
        }
    }
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static int do_end_primitive(int immediate)
{
    int retval = 0;

    if (UNLIKELY(!in_primitive)) {
        DLOG("Not constructing a primitive");
        goto out;
    }
    if (UNLIKELY(primitive_error)) {
        goto out;
    }
    if (primitive_count == 0) {
        DLOG("No vertices given!");
        goto out;
    }

    if (immediate) {
        SysPrimitive * const primitive = sys_graphics_create_primitive(
            primitive_type, primitive_data, primitive_format,
            primitive_size, primitive_count, NULL, 0, 0, 1);
        if (UNLIKELY(!primitive)) {
            DLOG("Failed to create primitive object");
            retval = 0;
        } else {
            sys_graphics_draw_primitive(primitive, 0, -1);
            sys_graphics_destroy_primitive(primitive);
            retval = 1;
        }
    } else {
        retval = graphics_create_primitive(
            primitive_type, primitive_data, primitive_format,
            primitive_size, primitive_count);
    }

  out:
    if (primitive_data != primitive_data_static_buffer) {
        mem_free(primitive_data);
    }
    primitive_data = NULL;
    in_primitive = 0;
    return retval;
}

/*************************************************************************/
/*************************************************************************/
