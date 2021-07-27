/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/font/sysfont.c: Font implementation for system-provided fonts.
 */

#include "src/base.h"
#include "src/font.h"
#include "src/font/internal.h"
#include "src/graphics.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/texture.h"
#include "src/utility/utf8.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Local routine declarations. */

/**
 * init_sysfont_font:  Fill in the system-provided font method pointers in
 * a Font object.
 *
 * [Parameters]
 *     font: Font object to fill in.
 */
static void init_sysfont_font(Font *font);

/*************************************************************************/
/********************** Font method implementations **********************/
/*************************************************************************/

int font_sysfont_init(Font *this, const char *name, float size, int mem_flags)
{
    this->private = (FontPrivate *)sys_sysfont_create(name, size, mem_flags);
    if (!this->private) {
        return 0;
    }
    init_sysfont_font(this);
    return 1;
}

/*-----------------------------------------------------------------------*/

static int font_sysfont_native_size(Font *this)
{
    ASSERT(this->private != NULL);
    return sys_sysfont_native_size((SysFont *)(this->private));
}

/*-----------------------------------------------------------------------*/

static void font_sysfont_get_metrics(Font *this, float size, float *height_ret,
                                     float *baseline_ret, float *ascent_ret,
                                     float *descent_ret)
{
    ASSERT(this->private != NULL, return);
    sys_sysfont_get_metrics((SysFont *)(this->private), size, height_ret,
                            baseline_ret, ascent_ret, descent_ret);
}

/*-----------------------------------------------------------------------*/

static float font_sysfont_char_advance(Font *this, int32_t ch, float size)
{
    ASSERT(this->private != NULL, return 0);
    return sys_sysfont_char_advance((SysFont *)(this->private), ch, size);
}

/*-----------------------------------------------------------------------*/

static float font_sysfont_text_advance(Font *this, const char *str, float size)
{
    ASSERT(this->private != NULL, return 0);
    return sys_sysfont_text_advance((SysFont *)(this->private), str, size);
}

/*-----------------------------------------------------------------------*/

static void font_sysfont_get_text_bounds(
    Font *this, const char *str, float size, float *left_ret, float *right_ret)
{
    ASSERT(this->private != NULL, return);
    sys_sysfont_get_text_bounds((SysFont *)(this->private), str, size,
                                left_ret, right_ret);
}

/*-----------------------------------------------------------------------*/

static Text *font_sysfont_render(Font *this, const char *str, float size,
                                 const Vector3f *origin, int v_flip)
{
    ASSERT(this->private != NULL, goto error_return);

    Vector2f tex_origin = {0, 0};
    float advance = 0, scale = 1;
    SysTexture *systex = sys_sysfont_render(
        (SysFont *)(this->private), str, size, &tex_origin.x, &tex_origin.y,
        &advance, &scale);
    if (!systex) {
        DLOG("Failed to render text");
        goto error_return;
    }
    tex_origin.x *= scale;
    tex_origin.y *= scale;
    advance *= scale;
    const float tex_width = sys_texture_width(systex) * scale;
    const float tex_height = sys_texture_height(systex) * scale;

    const int texture = texture_import(systex, 0);
    if (UNLIKELY(!texture)) {
        DLOG("Failed to register texture");
        sys_texture_destroy(systex);
        goto error_return;
    }
    texture_set_repeat(texture, 0, 0);

    struct Vertex {float x, y, z, u, v;};
    static const uint32_t vertex_format[] = {
        GRAPHICS_VERTEX_FORMAT(POSITION_3F, offsetof(struct Vertex,x)),
        GRAPHICS_VERTEX_FORMAT(TEXCOORD_2F, offsetof(struct Vertex,u)),
        0
    };
    struct Vertex vertices[4];
    const float left = origin->x - tex_origin.x;
    const float right = left + tex_width;
    const float top = origin->y - (v_flip ? -1 : +1) * tex_origin.y;
    const float bottom = top + (v_flip ? -1 : +1) * tex_height;
    vertices[0].x = left;
    vertices[0].y = top;
    vertices[0].z = origin->z;
    vertices[0].u = 0;
    vertices[0].v = 0;
    vertices[1].x = right;
    vertices[1].y = top;
    vertices[1].z = origin->z;
    vertices[1].u = 1;
    vertices[1].v = 0;
    vertices[2].x = right;
    vertices[2].y = bottom;
    vertices[2].z = origin->z;
    vertices[2].u = 1;
    vertices[2].v = 1;
    vertices[3].x = left;
    vertices[3].y = bottom;
    vertices[3].z = origin->z;
    vertices[3].u = 0;
    vertices[3].v = 1;
    const int primitive = graphics_create_primitive(
        GRAPHICS_PRIMITIVE_QUADS, vertices, vertex_format,
        sizeof(*vertices), lenof(vertices));
    if (UNLIKELY(!primitive)) {
        DLOG("Failed to create graphics primitive for text");
        goto error_destroy_texture;
    }

    Text *text = debug_mem_alloc(sizeof(*text), 0, 0,
                                 __FILE__, __LINE__, MEM_INFO_FONT);
    if (UNLIKELY(!text)) {
        DLOG("No memory for text primitive");
        goto error_destroy_primitive;
    }

    text->texture_is_oneshot = 1;
    text->texture = texture;
    text->primitive = primitive;
    text->advance = advance;
    return text;

  error_destroy_primitive:
    graphics_destroy_primitive(primitive);
  error_destroy_texture:
    texture_destroy(texture);
  error_return:
    return NULL;
}

/*-----------------------------------------------------------------------*/

static void font_sysfont_destroy(Font *this)
{
    sys_sysfont_destroy((SysFont *)(this->private));
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static void init_sysfont_font(Font *font)
{
    font->native_size     = font_sysfont_native_size;
    font->get_metrics     = font_sysfont_get_metrics;
    font->char_advance    = font_sysfont_char_advance;
    font->text_advance    = font_sysfont_text_advance;
    font->get_text_bounds = font_sysfont_get_text_bounds;
    font->render          = font_sysfont_render;
    font->destroy         = font_sysfont_destroy;
}

/*************************************************************************/
/*************************************************************************/
