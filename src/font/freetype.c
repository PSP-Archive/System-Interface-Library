/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/font/freetype.c: FreeType-based font implementation.
 */

#include "src/base.h"
#include "src/font.h"
#include "src/font/internal.h"
#include "src/memory.h"

#ifndef SIL_FONT_INCLUDE_FREETYPE

/* Stub function so linking doesn't fail. */

int font_freetype_init(UNUSED Font *this, void *data, UNUSED int len,
                       UNUSED int mem_flags, int reuse)
{
    DLOG("FreeType support not compiled in");
    if (reuse) {
        mem_free(data);
    }
    return 0;
}

#else  // defined(SIL_FONT_INCLUDE_FREETYPE), to the end of the file.

#include "src/graphics.h"
#include "src/math.h"
#include "src/texture.h"
#include "src/utility/utf8.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ADVANCES_H
#include FT_GLYPH_H
#include FT_MODULE_H
#include FT_OUTLINE_H

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Private data structure for a FreeType font. */
struct FontPrivate {
    /* Font data (will be freed when the font is destroyed). */
    void *data;
    /* FreeType library objects. */
    FT_Library library;
    FT_Face face;
};

/*-----------------------------------------------------------------------*/

/* FreeType memory allocator handle and associated function declarations. */

static void *freetype_alloc(FT_Memory memory, long size);
static void *freetype_realloc(FT_Memory memory, long cur_size, long new_size,
                              void *block);
static void freetype_free(FT_Memory memory, void *block);

static struct FT_MemoryRec_ freetype_allocator = {
    .alloc   = freetype_alloc,
    .realloc = freetype_realloc,
    .free    = freetype_free,
};

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * init_freetype_font:  Fill in the FreeType font method pointers in a Font
 * object.
 *
 * [Parameters]
 *     font: Font object to fill in.
 */
static void init_freetype_font(Font *font);

/**
 * set_size:  Set the current size for the given font.
 *
 * [Parameters]
 *     face: FT_Face instance for which to set size.
 *     size: Size to set (nominal height), in pixels.
 */
static void set_size(FT_Face face, float size);

/**
 * get_advance:  Return the horizontal advance for the given glyph as a
 * floating-point value.  The returned value is rounded to the nearest
 * 1/4096 to mask floating-point inaccuracies in the FreeType library.
 *
 * [Parameters]
 *     face: FT_Face instance from which to retrieve data.
 *     glyph_index: Glyph index.
 * [Return value]
 *     Horizontal advance for glyph, in pixels.
 */
static float get_advance(FT_Face face, FT_UInt glyph_index);

/**
 * get_kerning:  Return the horizontal kerning for the given glyph pair
 * as a floating-point value.
 *
 * [Parameters]
 *     face: FT_Face instance from which to retrieve data.
 *     prev_glyph: Glyph index of previous glyph.
 *     cur_glyph: Glyph index of current glyph.
 * [Return value]
 *     Horizontal origin offset for current glyph, in pixels.
 */
static float get_kerning(FT_Face face, FT_UInt prev_glyph, FT_UInt cur_glyph);

#ifdef DEBUG
/**
 * freetype_strerror:  Return an error message corresponding to the given
 * FreeType error code.
 *
 * This function is only defined when debug mode is enabled.
 *
 * [Parameters]
 *     error: FreeType error code.
 * [Return value]
 *     Corresponding error message.
 */
static const char *freetype_strerror(FT_Error error);
#endif

/*************************************************************************/
/********************** Font method implementations **********************/
/*************************************************************************/

int font_freetype_init(Font *this, void *data, int len, int mem_flags,
                       int reuse)
{
    mem_flags &= ~MEM_ALLOC_CLEAR;

    FT_Error error;

    /* If we're not reusing the incoming data buffer, make a copy of it,
     * since the data has to remain valid for the lifetime of the FreeType
     * font object. */
    if (!reuse) {
        const void *original = data;
        data = debug_mem_alloc(len, 0, mem_flags,
                               __FILE__, __LINE__, MEM_INFO_FONT);
        if (UNLIKELY(!data)) {
            DLOG("Failed to allocate %d bytes for copy of font data", len);
            goto error_return;
        }
        memcpy(data, original, len);
    }

    /* Allocate a private data structure for the font. */
    FontPrivate *private = debug_mem_alloc(sizeof(*private), 0, mem_flags,
                                           __FILE__, __LINE__, MEM_INFO_FONT);
    if (UNLIKELY(!private)) {
        DLOG("No memory for private data");
        goto error_free_data;
    }
    private->data = data;

    /* Initialize a FreeType library instance for this font. */
    error = FT_New_Library(&freetype_allocator, &private->library);
    if (UNLIKELY(error != FT_Err_Ok)) {
        DLOG("Failed to create FreeType library instance: %s",
             freetype_strerror(error));
        goto error_free_private;
    }
    FT_Add_Default_Modules(private->library);

    /* Parse the given data file. */
    error = FT_New_Memory_Face(private->library, data, len, 0, &private->face);
    if (error != FT_Err_Ok) {
        DLOG("Failed to parse font data: %s", freetype_strerror(error));
        goto error_free_library;
    }

    /* We currently only support scalable fonts.  We don't build in the
     * FreeType modules for non-scalable font formats, so any font that's
     * successfully loaded should be scalable. */
    ASSERT(FT_IS_SCALABLE(private->face));

    /* Set up the font instance and return. */
    init_freetype_font(this);
    this->private = private;
    return 1;

  error_free_library:
    FT_Done_Library(private->library);
  error_free_private:
    mem_free(private);
  error_free_data:
    mem_free(data);
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

static int font_freetype_native_size(UNUSED Font *this)
{
    return 0;  // Assume we're using a scalable font.
}

/*-----------------------------------------------------------------------*/

static void font_freetype_get_metrics(
    Font *this, float size, float *height_ret, float *baseline_ret,
    float *ascent_ret, float *descent_ret)
{
    ASSERT(this->private != NULL);
    ASSERT(this->private->face != NULL);
    FT_Face face = this->private->face;

    *height_ret   = size * face->height / face->units_per_EM;
    *baseline_ret = size * face->ascender / face->units_per_EM;
    *ascent_ret   = size * face->bbox.yMax / face->units_per_EM;
    *descent_ret  = -(size * face->bbox.yMin / face->units_per_EM);
}

/*-----------------------------------------------------------------------*/

static float font_freetype_char_advance(Font *this, int32_t ch, float size)
{
    ASSERT(this->private != NULL);
    ASSERT(this->private->face != NULL);
    FT_Face face = this->private->face;

    set_size(face, size);

    const FT_UInt glyph_index = FT_Get_Char_Index(face, ch);
    if (!glyph_index) {
        return 0;
    }
    return get_advance(face, glyph_index);
}

/*-----------------------------------------------------------------------*/

static float font_freetype_text_advance(Font *this, const char *str,
                                        float size)
{
    ASSERT(this->private != NULL);
    ASSERT(this->private->face != NULL);
    FT_Face face = this->private->face;

    set_size(face, size);

    float total_advance = 0;
    FT_UInt prev_glyph = 0;
    int32_t ch;
    while ((ch = utf8_read(&str)) != 0) {
        if (ch == -1) {
            continue;
        }
        const FT_UInt glyph_index = FT_Get_Char_Index(face, ch);
        if (glyph_index) {
            total_advance += get_kerning(face, prev_glyph, glyph_index);
            total_advance += get_advance(face, glyph_index);
        }
        prev_glyph = glyph_index;
    }
    return total_advance;
}

/*-----------------------------------------------------------------------*/

static void font_freetype_get_text_bounds(
    Font *this, const char *str, float size, float *left_ret, float *right_ret)
{
    ASSERT(this->private != NULL);
    ASSERT(this->private->face != NULL);
    FT_Face face = this->private->face;

    set_size(face, size);

    *left_ret = 0;
    *right_ret = 0;
    float pos = 0;
    FT_UInt prev_glyph = 0;
    int32_t ch;
    while ((ch = utf8_read(&str)) != 0) {
        if (ch == -1) {
            continue;
        }
        const FT_UInt glyph_index = FT_Get_Char_Index(face, ch);
        if (glyph_index) {
            pos += get_kerning(face, prev_glyph, glyph_index);
            FT_Error error =
                FT_Load_Glyph(face, glyph_index, FT_LOAD_NO_BITMAP);
            if (UNLIKELY(error != FT_Err_Ok)) {
                DLOG("Failed to get glyph for U+%04X: %s", ch,
                     freetype_strerror(error));
            } else {
                const int pos_frac = iroundf(fracf(pos) * 64);
                if (pos_frac != 0) {
                    FT_Outline_Translate(&face->glyph->outline, pos_frac, 0);
                }
                error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
                if (UNLIKELY(error != FT_Err_Ok)) {
                    DLOG("Failed to render glyph for U+%04X: %s", ch,
                         freetype_strerror(error));
                } else {
                    const float left = pos + face->glyph->bitmap_left;
                    *left_ret = min(*left_ret, left);
                    const float right = left + face->glyph->bitmap.width;
                    *right_ret = max(*right_ret, right);
                }
            }
            pos += get_advance(face, glyph_index);
        }
        prev_glyph = glyph_index;
    }
}

/*-----------------------------------------------------------------------*/

static Text *font_freetype_render(Font *this, const char *str, float size,
                                  const Vector3f *origin, int v_flip)
{
    ASSERT(this->private != NULL);
    ASSERT(this->private->face != NULL);
    FT_Face face = this->private->face;

    /* Note that we don't need to call set_size() because the get_tex_bounds()
     * call below will do it for us. */

    const int baseline = iceilf(size * face->ascender / face->units_per_EM);
    const int descent = -(iceilf(size * face->descender / face->units_per_EM));
    float left_bound, right_bound;
    font_freetype_get_text_bounds(this, str, size, &left_bound, &right_bound);
    /* Add a 1-pixel border so pixels at the edges of the text are properly
     * antialiased if the text is scaled up. */
    const int tex_width = 2 + (-ifloorf(left_bound)) + iceilf(right_bound);
    const int tex_height = 2 + baseline + descent;
    uint8_t *pixels = debug_mem_alloc(tex_width * tex_height, 0,
                                      MEM_ALLOC_CLEAR,
                                      __FILE__, __LINE__, MEM_INFO_FONT);
    if (UNLIKELY(!pixels)) {
        DLOG("No memory for texture (%dx%d)", tex_width, tex_height);
        goto error_return;
    }
    const int tex_origin_x = 1 + (-ifloorf(left_bound));
    const int tex_origin_y = 1 + baseline;

    float pos = 0;
    FT_UInt prev_glyph = 0;
    int32_t ch;
    while ((ch = utf8_read(&str)) != 0) {
        if (ch == -1) {
            continue;
        }

        const FT_UInt glyph_index = FT_Get_Char_Index(face, ch);
        if (!glyph_index) {
            continue;
        }

        pos += get_kerning(face, prev_glyph, glyph_index);
        const int this_pos = ifloorf(pos);
        const int pos_frac = iroundf(fracf(pos) * 64);
        pos += get_advance(face, glyph_index);
        prev_glyph = glyph_index;

        FT_Error error = FT_Load_Glyph(face, glyph_index, FT_LOAD_NO_BITMAP);
        if (UNLIKELY(error != FT_Err_Ok)) {
            DLOG("Failed to get glyph for U+%04X: %s", ch,
                 freetype_strerror(error));
            continue;
        }
        if (pos_frac != 0) {
            FT_Outline_Translate(&face->glyph->outline, pos_frac, 0);
        }
        error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
        if (UNLIKELY(error != FT_Err_Ok)) {
            DLOG("Failed to render glyph for U+%04X: %s", ch,
                 freetype_strerror(error));
            continue;
        }

        const int glyph_width = face->glyph->bitmap.width;
        const int glyph_height = face->glyph->bitmap.rows;
        const int glyph_stride = face->glyph->bitmap.pitch;
        int x = tex_origin_x + this_pos + face->glyph->bitmap_left;
        ASSERT(x >= 0, x = 0);
        ASSERT(x <= tex_width - (glyph_width+1), // +1 for antialiasing
               x = tex_width - (glyph_width+1));
        int y = tex_origin_y - face->glyph->bitmap_top;
        ASSERT(y >= 0, y = 0);
        ASSERT(y <= tex_height - glyph_height,
               y = tex_height - glyph_height);
        const uint8_t *src = face->glyph->bitmap.buffer;
        uint8_t *dest = pixels + (y * tex_width + x);
        for (int line = 0; line < glyph_height;
             line++, src += glyph_stride, dest += tex_width)
        {
            for (int pixel = 0; pixel < glyph_width; pixel++) {
                if (src[pixel] != 0) {
                    if (dest[pixel] == 0) {
                        dest[pixel] = src[pixel];
                    } else {
                        dest[pixel] = ubound(dest[pixel] + src[pixel], 255);
                    }
                }
            }
        }
    }

    const int texture = texture_create_with_data(
        tex_width, tex_height, pixels, TEX_FORMAT_A8, tex_width, 0, 0);
    mem_free(pixels);
    if (UNLIKELY(!texture)) {
        DLOG("Failed to create texture");
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
    const float left = origin->x - tex_origin_x;
    const float right = left + tex_width;
    const float top = origin->y - (v_flip ? +1 : -1) * tex_origin_y;
    const float bottom = top + (v_flip ? +1 : -1) * tex_height;
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
    text->advance = pos;
    return text;

  error_destroy_primitive:
    graphics_destroy_primitive(primitive);
  error_destroy_texture:
    texture_destroy(texture);
  error_return:
    return NULL;
}

/*-----------------------------------------------------------------------*/

static void font_freetype_destroy(Font *this)
{
    ASSERT(this->private != NULL);

    FT_Done_Face(this->private->face);
    FT_Done_Library(this->private->library);
    mem_free(this->private->data);
    mem_free(this->private);
    this->private = NULL;
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

#ifdef DEBUG
# ifdef FT_DEBUG_MEMORY
extern const char *_ft_debug_file;
extern long _ft_debug_lineno;
# else
#  define _ft_debug_file "(freetype)"
#  define _ft_debug_lineno 0
# endif
#endif

static void *freetype_alloc(UNUSED FT_Memory memory, long size)
{
    return debug_mem_alloc(size, 0, 0, _ft_debug_file, _ft_debug_lineno,
                           MEM_INFO_FONT);
}

static void *freetype_realloc(UNUSED FT_Memory memory, UNUSED long cur_size,
                              long new_size, void *block)
{
    return debug_mem_realloc(block, new_size, 0, _ft_debug_file,
                             _ft_debug_lineno, MEM_INFO_FONT);
}

static void freetype_free(UNUSED FT_Memory memory, void *block)
{
    debug_mem_free(block, _ft_debug_file, _ft_debug_lineno);
}

/*-----------------------------------------------------------------------*/

static void init_freetype_font(Font *font)
{
    font->native_size     = font_freetype_native_size;
    font->get_metrics     = font_freetype_get_metrics;
    font->char_advance    = font_freetype_char_advance;
    font->text_advance    = font_freetype_text_advance;
    font->get_text_bounds = font_freetype_get_text_bounds;
    font->render          = font_freetype_render;
    font->destroy         = font_freetype_destroy;
}

/*-----------------------------------------------------------------------*/

static void set_size(FT_Face face, float size)
{
    const FT_Error error = FT_Set_Char_Size(face, 0, iroundf(size*64), 0, 72);
    ASSERT(error == FT_Err_Ok);
}

/*-----------------------------------------------------------------------*/

static float get_advance(FT_Face face, FT_UInt glyph_index)
{
    FT_Fixed advance = 0;
    FT_Get_Advance(face, glyph_index, FT_LOAD_NO_HINTING, &advance);
    const float f = advance / 65536.0f;
    return roundf(f*4096) / 4096;
}

/*-----------------------------------------------------------------------*/

static float get_kerning(FT_Face face, FT_UInt prev_glyph, FT_UInt cur_glyph)
{
    PRECOND(cur_glyph != 0, return 0);

    if (!FT_HAS_KERNING(face) || !prev_glyph) {
        return 0;
    }
    FT_Vector kerning;
    FT_Get_Kerning(face, prev_glyph, cur_glyph, FT_KERNING_UNFITTED, &kerning);
    return kerning.x / 64.0f;
}

/*-----------------------------------------------------------------------*/

#ifdef DEBUG

static const char *freetype_strerror(FT_Error error)
{
#undef __FTERRORS_H__
#define FT_ERRORDEF(id,value,message)  if (error == (id)) return (message);
#include FT_ERRORS_H
    return "unknown error";  // NOTREACHED
}

#endif  // DEBUG

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_FONT_INCLUDE_FREETYPE
