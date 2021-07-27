/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/opengl/primitive.c: OpenGL-specific graphics primitive
 * tests.
 */

#include "src/base.h"
#undef SIL_OPENGL_NO_SYS_FUNCS  // Avoid type renaming.
#include "src/graphics.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep.h"  // Needed by src/sysdep/opengl/internal.h.
#include "src/sysdep/opengl/internal.h"
#include "src/sysdep/opengl/opengl.h"
#include "src/test/base.h"
#include "src/test/graphics/internal.h"
#include "src/texture.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

typedef struct BasicVertex {float x,y,z;} BasicVertex;
static const uint32_t basic_vertex_format[] = {
    GRAPHICS_VERTEX_FORMAT(POSITION_3F, offsetof(BasicVertex,x)),
    0
};

static const BasicVertex square_vertices[] = {
    {0,0,0}, {0,1,0}, {1,1,0}, {1,0,0}
};
static const uint16_t square_indices[] = {0, 1, 2, 3};

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

/**
 * shader_source_fail, shader_key_fail:  Stub shader generators which
 * always fail.  Used to force primitive rendering failure when using
 * shaders.
 */
static char *shader_source_fail(
    UNUSED GraphicsPrimitiveType primitive_type, UNUSED int position_count,
    UNUSED int texcoord_count, UNUSED GraphicsTextureColorType texcolor_type,
    UNUSED int tex_offset, UNUSED int color_count, UNUSED int color_uniform,
    UNUSED int fog, UNUSED int alpha_test,
    UNUSED GraphicsComparisonType alpha_comparison)
{
    return NULL;
}

static uint32_t shader_key_fail(
    UNUSED GraphicsPrimitiveType primitive_type, UNUSED int position_count,
    UNUSED int texcoord_count, UNUSED GraphicsTextureColorType texcolor_type,
    UNUSED int tex_offset, UNUSED int color_count, UNUSED int color_uniform,
    UNUSED int fog, UNUSED int alpha_test,
    UNUSED GraphicsComparisonType alpha_comparison)
{
    return INVALID_SHADER_KEY;
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

static int do_test_opengl_primitive(void);
int test_opengl_primitive(void)
{
    return run_tests_in_window(do_test_opengl_primitive);
}

DEFINE_GENERIC_TEST_RUNNER(do_test_opengl_primitive)

TEST_INIT(init)
{
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    return 1;
}

TEST_CLEANUP(cleanup)
{
    graphics_set_blend(GRAPHICS_BLEND_ADD,
                       GRAPHICS_BLEND_SRC_ALPHA, GRAPHICS_BLEND_INV_SRC_ALPHA);
    graphics_set_fixed_color(&(Vector4f){1, 1, 1, 1});

    graphics_finish_frame();
    graphics_flush_resources();
    return 1;
}

/*************************************************************************/
/***************************** General tests *****************************/
/*************************************************************************/

TEST(test_reuse_immediate_vertex_buffer)
{
    static const BasicVertex offscreen_vertices[] = {
        {2,2,0}, {2,3,0}, {3,3,0}, {3,2,0}
    };
    uint8_t *pixels;

    opengl_primitive_cleanup();  // Destroy existing immediate-mode buffers.

    for (int i = 0; i < SIL_OPENGL_IMMEDIATE_VERTEX_BUFFERS; i++) {
        graphics_draw_vertices(
            GRAPHICS_PRIMITIVE_QUADS, offscreen_vertices, basic_vertex_format,
            sizeof(*offscreen_vertices), lenof(offscreen_vertices));
    }
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, square_vertices, basic_vertex_format,
        sizeof(*square_vertices), lenof(square_vertices));
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x >= TESTW/2 && y >= TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_reuse_immediate_index_buffer)
{
    static const BasicVertex offscreen_vertices[] = {
        {2,2,0}, {2,3,0}, {3,3,0}, {3,2,0}
    };
    uint8_t *pixels;

    graphics_set_fixed_color(&(Vector4f){1, 1, 1, 0.2});
    graphics_set_blend(GRAPHICS_BLEND_ADD,
                       GRAPHICS_BLEND_SRC_ALPHA, GRAPHICS_BLEND_ONE);

    opengl_primitive_cleanup();  // Destroy existing immediate-mode buffers.

    for (int i = 0; i < SIL_OPENGL_IMMEDIATE_VERTEX_BUFFERS / 2; i++) {
        graphics_draw_indexed_vertices(
            GRAPHICS_PRIMITIVE_QUADS, offscreen_vertices, basic_vertex_format,
            sizeof(*offscreen_vertices), lenof(offscreen_vertices),
            square_indices, sizeof(*square_indices), lenof(square_indices));
    }
    graphics_draw_indexed_vertices(
        GRAPHICS_PRIMITIVE_QUADS, square_vertices, basic_vertex_format,
        sizeof(*square_vertices), lenof(square_vertices),
        square_indices, sizeof(*square_indices), lenof(square_indices));
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x >= TESTW/2 && y >= TESTH/2) ? 51 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* load_primitive_data() rolls over on the opposite parity, so cycle
     * through once more. */
    for (int i = 2; i < SIL_OPENGL_IMMEDIATE_VERTEX_BUFFERS - 1; i++) {
        graphics_draw_vertices(
            GRAPHICS_PRIMITIVE_QUADS, offscreen_vertices, basic_vertex_format,
            sizeof(*offscreen_vertices), lenof(offscreen_vertices));
    }
    graphics_draw_indexed_vertices(
        GRAPHICS_PRIMITIVE_QUADS, square_vertices, basic_vertex_format,
        sizeof(*square_vertices), lenof(square_vertices),
        square_indices, sizeof(*square_indices), lenof(square_indices));
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x >= TESTW/2 && y >= TESTH/2) ? 102 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_draw_primitive_shader_failure)
{
    int primitive;
    CHECK_TRUE(primitive = graphics_create_primitive(
        GRAPHICS_PRIMITIVE_QUADS, square_vertices, basic_vertex_format,
        sizeof(*square_vertices), lenof(square_vertices)));
    graphics_set_shader_generator(shader_source_fail, shader_source_fail,
                                  shader_key_fail, 0, 1);
    graphics_draw_primitive(primitive);
    graphics_set_shader_generator(NULL, NULL, NULL, 0, 0);
    graphics_destroy_primitive(primitive);
    uint8_t *pixels;
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        CHECK_PIXEL(&pixels[i], 0,0,0,255, x, y);
    }
    mem_free(pixels);

    return 1;
}

/*************************************************************************/
/***************** Tests for !NATIVE_QUADS special cases *****************/
/*************************************************************************/

TEST(test_short_indexed_quad)
{
    graphics_draw_indexed_vertices(
        GRAPHICS_PRIMITIVE_QUADS, square_vertices, basic_vertex_format,
        sizeof(*square_vertices), lenof(square_vertices),
        square_indices, sizeof(*square_indices), lenof(square_indices) - 1);

    uint8_t *pixels;
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        CHECK_PIXEL(&pixels[i], 0,0,0,255, x, y);
    }
    mem_free(pixels);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_short_indexed_quad_strip)
{
    static const BasicVertex vertices[] = {
        {0,0,0}, {0,1,0}, {1,0,0}, {1,1,0}, {0.5,0.5,0}, {0.5,1,0}
    };
    static const uint16_t indices[] = {0, 1, 2, 3, 4, 5};
    uint8_t *pixels;

    graphics_draw_indexed_vertices(
        GRAPHICS_PRIMITIVE_QUAD_STRIP,
        vertices, basic_vertex_format, sizeof(*vertices), lenof(vertices),
        indices, sizeof(*indices), lenof(indices) - 3);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        CHECK_PIXEL(&pixels[i], 0,0,0,255, x, y);
    }
    mem_free(pixels);

    graphics_set_fixed_color(&(Vector4f){1, 1, 1, 0.2});
    graphics_draw_indexed_vertices(
        GRAPHICS_PRIMITIVE_QUAD_STRIP,
        vertices, basic_vertex_format, sizeof(*vertices), lenof(vertices),
        indices, sizeof(*indices), lenof(indices) - 1);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x >= TESTW/2 && y >= TESTH/2 ? 51 : 0);
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_single_quad_buffer)
{
    int primitive;
    uint8_t *pixels;

    CHECK_TRUE(primitive = graphics_create_primitive(
        GRAPHICS_PRIMITIVE_QUADS, square_vertices, basic_vertex_format,
        sizeof(*square_vertices), lenof(square_vertices)));
    graphics_draw_primitive(primitive);
    graphics_destroy_primitive(primitive);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x >= TESTW/2 && y >= TESTH/2 ? 255 : 0);
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_single_quad_buffer_memory_failure)
{
    int primitive;
    uint8_t *pixels;

    opengl_primitive_cleanup();  // Destroy any existing single quad buffer.

    CHECK_MEMORY_FAILURES(primitive = graphics_create_primitive(
        GRAPHICS_PRIMITIVE_QUADS, square_vertices, basic_vertex_format,
        sizeof(*square_vertices), lenof(square_vertices)));
    graphics_draw_primitive(primitive);
    graphics_destroy_primitive(primitive);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x >= TESTW/2 && y >= TESTH/2 ? 255 : 0);
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_many_quads)
{
    static const BasicVertex offscreen_vertices[4] = {
        {2,2,0}, {2,3,0}, {3,3,0}, {3,2,0}
    };

    const int quads_for_32bit = 65536/4 + 1;
    BasicVertex *vertices;
    ASSERT(vertices = mem_alloc((quads_for_32bit * 4) * sizeof(*vertices),
                                0, MEM_ALLOC_TEMP));
    int primitive;
    uint8_t *pixels;

    graphics_set_fixed_color(&(Vector4f){1, 1, 1, 0.2});
    graphics_set_blend(GRAPHICS_BLEND_ADD,
                       GRAPHICS_BLEND_SRC_ALPHA, GRAPHICS_BLEND_ONE);

    memcpy(&vertices[0], square_vertices, sizeof(square_vertices));
    for (int i = 1; i < quads_for_32bit - 1; i++) {
        memcpy(&vertices[i*4], offscreen_vertices, sizeof(offscreen_vertices));
    }
    memcpy(&vertices[(quads_for_32bit - 1) * 4],
           square_vertices, sizeof(square_vertices));
    if (opengl_has_formats(OPENGL_FORMAT_INDEX32)) {
        CHECK_TRUE(primitive = graphics_create_primitive(
                       GRAPHICS_PRIMITIVE_QUADS, vertices, basic_vertex_format,
                       sizeof(*vertices), quads_for_32bit * 4));
    } else {
        CHECK_FALSE(primitive = graphics_create_primitive(
                        GRAPHICS_PRIMITIVE_QUADS, vertices,
                        basic_vertex_format, sizeof(*vertices),
                        quads_for_32bit * 4));
        memcpy(&vertices[(quads_for_32bit - 2) * 4],
               square_vertices, sizeof(square_vertices));
        CHECK_TRUE(primitive = graphics_create_primitive(
                       GRAPHICS_PRIMITIVE_QUADS, vertices, basic_vertex_format,
                       sizeof(*vertices), (quads_for_32bit-1) * 4));
    }
    graphics_draw_primitive(primitive);
    graphics_destroy_primitive(primitive);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p =  (x >= TESTW/2 && y >= TESTH/2 ? 102 : 0);
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    mem_free(vertices);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_quad_index_memory_failure)
{
    static const BasicVertex vertices[] = {
        {0,0,0}, {0,1,0}, {1,1,0}, {1,0,0},
        {-1,-1,0}, {-1,0,0}, {0,0,0}, {0,-1,0},
    };
    int primitive;
    uint8_t *pixels;

    CHECK_MEMORY_FAILURES(primitive = graphics_create_primitive(
        GRAPHICS_PRIMITIVE_QUADS,
        vertices, basic_vertex_format, sizeof(*vertices), lenof(vertices)));
    graphics_draw_primitive(primitive);
    graphics_destroy_primitive(primitive);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = ((x >= TESTW/2 && y >= TESTH/2)
                       || (x < TESTW/2 && y < TESTH/2) ? 255 : 0);
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    return 1;
}

/*************************************************************************/
/*************************************************************************/
