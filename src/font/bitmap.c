/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/font/bitmap.c: Bitmap font implementation.
 */

#include "src/base.h"
#include "src/font.h"
#include "src/font/internal.h"
#include "src/graphics.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/texture.h"
#include "src/utility/font-file.h"
#include "src/utility/utf8.h"

#ifdef SIL_PLATFORM_PSP
# include "src/endian.h"
# include "src/utility/tex-file.h"
#endif

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/**
 * MAX_CHAR_VALUE:  Maximum character value (Unicode codepoint) that we
 * support.  This is used to avoid trying to allocate large amounts of
 * memory when reading corrupt files.  However, we only allocate memory
 * for portions of the code space actually used, so there is no inherent
 * penalty for setting this to a large value.
 */
#define MAX_CHAR_VALUE  0x10FFFF

/*-----------------------------------------------------------------------*/

typedef struct CharInfo CharInfo;  // Used in the FontPrivate structure.

/* Private data structure for a bitmap font. */
struct FontPrivate {
    /* Native font height, in pixels. */
    int height;
    /* Baseline position, in pixels. */
    int baseline;
    /* Maximum ascent and descent, in pixels. */
    int ascent, descent;

    /* Texture containing character glyphs. */
    int texture;

    /* Character data, indexed by Unicode codepoint.  Since many fonts only
     * use limited portions of the code space, we arrange this data as a
     * two-level array; each element N of charinfo[] either points to a
     * 256-element CharInfo array, covering the 256 codepoints from U+<N>00
     * through U+<N>FF, or is NULL, indicating that no codepoints in that
     * range are defined in the font. */
    int num_charinfo_pages;
    CharInfo **charinfo;
};

/* Data structure for a single character. */
struct CharInfo {
    /* Upper-left and lower-right texture coordinates. */
    Vector2f uv0, uv1;
    /* Character width, ascent, and descent, in pixels. */
    int width, ascent, descent;
    /* Pre- and post-kern offsets, in (possibly fractional) pixels. */
    float prekern, postkern;
};

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * init_bitmap_font:  Fill in the bitmap font method pointers in a Font
 * object.
 *
 * [Parameters]
 *     font: Font object to fill in.
 */
static void init_bitmap_font(Font *font);

/**
 * get_charinfo:  Return the CharInfo structure for the given character
 * (Unicode codepoint) in the given font, or NULL if the character is not
 * defined in the font.
 *
 * [Parameters]
 *     private: Private data structure for font in which to look up character.
 *     ch: Character to look up.
 * [Return value]
 *     Pointer to character's CharInfo structure, or NULL if none.
 */
static const CharInfo *get_charinfo(const FontPrivate *private, int32_t ch);

/*************************************************************************/
/********************** Font method implementations **********************/
/*************************************************************************/

int font_bitmap_init(Font *this, void *data, int len, int mem_flags,
                     int reuse)
{
    mem_flags &= ~MEM_ALLOC_CLEAR;

    /* Parse the file data into structures. */
    FontFileHeader header;
    if (!font_parse_header(data, len, &header)) {
        DLOG("Failed to parse font file");
        goto error_return;
    }

    FontFileCharInfo *charinfo_in = debug_mem_alloc(
        sizeof(FontFileCharInfo) * header.charinfo_count, 0,
        /* These flags ensure that even if temporary memory isn't available,
         * we don't cause fragmentation near where the final font buffer
         * will be allocated. */
        (mem_flags & MEM_ALLOC_TOP) ^ MEM_ALLOC_TEMP,
        __FILE__, __LINE__, MEM_INFO_FONT);
    if (UNLIKELY(!charinfo_in)) {
        DLOG("No memory for character info (%u bytes)",
             (unsigned int)(sizeof(FontFileCharInfo) * header.charinfo_count));
        goto error_return;
    }
    if (!font_parse_charinfo((const char *)data + header.charinfo_offset,
                             header.charinfo_count, header.version,
                             charinfo_in)) {
        DLOG("Failed to parse character info");
        goto error_free_charinfo_in;
    }

    /* Create a texture from the font's image data.  For systems which
     * implement input memory reuse for textures, we shift the image data
     * down to the beginning of the input data block and pass the block
     * onto the texture routines if our reuse flag is true. */
    int32_t tex_offset = header.texture_offset;
    int32_t tex_size = header.texture_size;
    int tex_reuse = 0;
#ifdef SIL_PLATFORM_PSP
    if (reuse) {
        TexFileHeader tex_header;
        if (tex_parse_header((char *)data + header.texture_offset,
                             header.texture_size, &tex_header)) {
            /* Moving the entire buffer may be expensive, so just shift the
             * TEX header and rewrite the pixels_offset field. */
            memmove(data, (char *)data + header.texture_offset,
                    sizeof(TexFileHeader));
            TexFileHeader *new_header = (TexFileHeader *)data;
            new_header->pixels_offset =
                u32_to_be(header.texture_offset + tex_header.pixels_offset);
            tex_offset = 0;
            tex_size += header.texture_offset;
            tex_reuse = 1;
            reuse = 0;  // Don't attempt to free it ourselves.
        }
    }
#endif
    int texture = texture_parse((char *)data + tex_offset, tex_size,
                                mem_flags, 1, tex_reuse);
    if (!texture) {
        DLOG("Failed to parse font texture");
        goto error_free_charinfo_in;
    }

    /* Work out which Unicode pages are used. */
    const int max_pages = (MAX_CHAR_VALUE >> 8) + 1;
    uint32_t *pages = debug_mem_alloc(((max_pages+31) / 32) * 4, 4,
                                      MEM_ALLOC_TEMP | MEM_ALLOC_CLEAR,
                                      __FILE__, __LINE__, MEM_INFO_FONT);
    if (UNLIKELY(!pages)) {
        DLOG("No memory for page flags (%u bytes)", (max_pages+31) / 32 * 32);
        goto error_destroy_texture;
    }
    int max_page_used = -1;
    for (unsigned int i = 0; i < header.charinfo_count; i++) {
        ASSERT(charinfo_in[i].ch >= 0, continue);
        const int page = charinfo_in[i].ch >> 8;
        if (LIKELY(page < max_pages)) {
            pages[page/32] |= 1U << (page%32);
            if (page > max_page_used) {
                max_page_used = page;
            }
        } else {
            DLOG("Warning: Character U+%04X in font but out of supported"
                 " range", charinfo_in[i].ch);
        }
    }
    int pages_used = 0;
    for (int i = 0; i < (max_pages+31) / 32; i++) {
        static const uint8_t popcount[256] = {
            0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4,1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,
            1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
            1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
            2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
            1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,
            2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
            2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,
            3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8,
        };
        const uint32_t pages_word = pages[i];
        pages_used += popcount[pages_word>>24 & 0xFF]
                    + popcount[pages_word>>16 & 0xFF]
                    + popcount[pages_word>> 8 & 0xFF]
                    + popcount[pages_word>> 0 & 0xFF];
    }
    if (!pages_used) {
        DLOG("Font contains no characters!");
        goto error_free_pages;
    }

    /* Allocate the private data structure as a single block (for ease of
     * freeing later), and set up pointers appropriately. */
    const size_t struct_size = (sizeof(FontPrivate)
                                + (max_page_used + 1) * sizeof(CharInfo *)
                                + pages_used * (256*sizeof(CharInfo)));
    FontPrivate *private = debug_mem_alloc(struct_size, 0, mem_flags,
                                           __FILE__, __LINE__, MEM_INFO_FONT);
    if (UNLIKELY(!private)) {
        DLOG("No memory for private data structure");
        goto error_free_pages;
    }
    private->num_charinfo_pages = max_page_used + 1;
    private->charinfo = (CharInfo **)&private[1];
    CharInfo *pageptr = (CharInfo *)&private->charinfo[private->num_charinfo_pages];
    for (int i = 0; i < private->num_charinfo_pages; i++) {
        if (pages[i/32] & (1U << (i%32))) {
            private->charinfo[i] = pageptr;
            mem_clear(pageptr, 256 * sizeof(*pageptr));
            pageptr += 256;
        } else {
            private->charinfo[i] = NULL;
        }
    }
    ASSERT((uintptr_t)pageptr == (uintptr_t)private + struct_size);

    /* Set up internal per-character data. */
    int global_ascent = 0, global_descent = 0;
    const int tex_width = texture_width(texture);
    const int tex_height = texture_height(texture);
    const float texw_mult = 1.0f / tex_width;
    const float texh_mult = 1.0f / tex_height;
    for (unsigned int i = 0; i < header.charinfo_count; i++) {
        ASSERT(charinfo_in[i].ch >= 0, continue);
        if (charinfo_in[i].ch > MAX_CHAR_VALUE) {
            continue;
        }
        ASSERT(private->charinfo[charinfo_in[i].ch >> 8] != NULL, continue);
        CharInfo *charinfo_out =
            &private->charinfo[charinfo_in[i].ch >> 8][charinfo_in[i].ch & 255];
        const int x = charinfo_in[i].x;
        const int y = charinfo_in[i].y;
        const int w = charinfo_in[i].w;
        const int h = charinfo_in[i].h;
        const int ascent = charinfo_in[i].ascent;
        const int descent = h - charinfo_in[i].ascent;
        const int prekern = charinfo_in[i].prekern;
        const int postkern = charinfo_in[i].postkern;
        if (UNLIKELY(x + w > tex_width) || UNLIKELY(y + h > tex_height)) {
            DLOG("Warning: Character U+%04X glyph out of texture bounds"
                 " (%d,%d-%d,%d on %dx%d texture)", charinfo_in[i].ch,
                 x, y, x+w, y+h, tex_width, tex_height);
        }
        charinfo_out->uv0.x = x * texw_mult;
        charinfo_out->uv0.y = y * texh_mult;
        charinfo_out->uv1.x = (x + w) * texw_mult;
        charinfo_out->uv1.y = (y + h) * texh_mult;
        charinfo_out->width = w;
        charinfo_out->ascent = ascent;
        charinfo_out->descent = descent;
        charinfo_out->prekern = prekern * (1.0f/256.0f);
        charinfo_out->postkern = postkern * (1.0f/256.0f);
        if (ascent > global_ascent) {
            global_ascent = ascent;
        }
        if (descent > global_descent) {
            global_descent = descent;
        }
    }

    /* Initialize fontwide data fields. */
    private->height   = header.height;
    private->baseline = header.baseline;
    private->ascent   = global_ascent;
    private->descent  = global_descent;
    private->texture  = texture;

    /* Free temporary buffers (and the incoming data buffer if requested),
     * set up the Font method pointers, and return success. */
    mem_free(pages);
    mem_free(charinfo_in);
    if (reuse) {
        mem_free(data);
    }
    init_bitmap_font(this);
    this->private = private;
    return 1;

  error_free_pages:
    mem_free(pages);
  error_destroy_texture:
    texture_destroy(texture);
  error_free_charinfo_in:
    mem_free(charinfo_in);
  error_return:
    if (reuse) {
        mem_free(data);
    }
    return 0;
}

/*-----------------------------------------------------------------------*/

static int font_bitmap_native_size(Font *this)
{
    ASSERT(this->private != NULL);
    return this->private->height;
}

/*-----------------------------------------------------------------------*/

static void font_bitmap_get_metrics(Font *this, float size, float *height_ret,
                                    float *baseline_ret, float *ascent_ret,
                                    float *descent_ret)
{
    ASSERT(this->private != NULL);

    const float size_mult = size / this->private->height;
    *height_ret   = size;
    *baseline_ret = this->private->baseline * size_mult;
    *ascent_ret   = this->private->ascent   * size_mult;
    *descent_ret  = this->private->descent  * size_mult;
}

/*-----------------------------------------------------------------------*/

static float font_bitmap_char_advance(Font *this, int32_t ch, float size)
{
    ASSERT(this->private != NULL);

    const CharInfo *charinfo = get_charinfo(this->private, ch);
    if (!charinfo) {
        return 0;
    }
    const float advance =
        charinfo->prekern + charinfo->width + charinfo->postkern;
    return advance * (size / this->private->height);
}

/*-----------------------------------------------------------------------*/

static float font_bitmap_text_advance(Font *this, const char *str, float size)
{
    ASSERT(this->private != NULL);

    const float size_mult = size / this->private->height;
    float advance = 0;
    int32_t ch;
    while ((ch = utf8_read(&str)) != 0) {
        if (ch == -1) {
            continue;
        }
        const CharInfo *charinfo = get_charinfo(this->private, ch);
        if (charinfo) {
            const float char_advance =
                charinfo->prekern + charinfo->width + charinfo->postkern;
            advance += char_advance * size_mult;
        }
    }
    return advance;
}

/*-----------------------------------------------------------------------*/

static void font_bitmap_get_text_bounds(
    Font *this, const char *str, float size, float *left_ret, float *right_ret)
{
    ASSERT(this->private != NULL);

    const float size_mult = size / this->private->height;
    *left_ret = *right_ret = 0;
    float x = 0;
    int32_t ch;
    while ((ch = utf8_read(&str)) != 0) {
        if (ch == -1) {
            continue;
        }
        const CharInfo *charinfo = get_charinfo(this->private, ch);
        if (charinfo) {
            const float char_left = x + charinfo->prekern * size_mult;
            const float char_right = char_left + charinfo->width * size_mult;
            if (char_left < *left_ret) {
                *left_ret = char_left;
            }
            if (char_right > *right_ret) {
                *right_ret = char_right;
            }
            const float char_advance =
                charinfo->prekern + charinfo->width + charinfo->postkern;
            x += char_advance * size_mult;
        }
    }
}

/*-----------------------------------------------------------------------*/

static Text *font_bitmap_render(Font *this, const char *str, float size,
                                const Vector3f *origin, int v_flip)
{
    ASSERT(this->private != NULL);

    const float size_mult = size / this->private->height;
    const float ascent_mult = v_flip ? -size_mult : size_mult;
    Vector3f pos = *origin;

    struct Vertex {float x, y, z, u, v;};
    static const uint32_t vertex_format[] = {
        GRAPHICS_VERTEX_FORMAT(POSITION_3F, offsetof(struct Vertex,x)),
        GRAPHICS_VERTEX_FORMAT(TEXCOORD_2F, offsetof(struct Vertex,u)),
        0
    };
    struct Vertex *vertices;
    /* We can never have more than 4 vertices (1 quad) per byte, so
     * allocate that much memory for the vertex buffer.  We only keep it
     * long enough to create the graphics primitive for rendering, so it's
     * not critical to keep it small. */
    const int vertices_size = 4 * lbound((int)strlen(str), 1);
    vertices = debug_mem_alloc(sizeof(*vertices) * vertices_size, 4,
                               MEM_ALLOC_TEMP,
                               __FILE__, __LINE__, MEM_INFO_FONT);
    if (UNLIKELY(!vertices)) {
        return NULL;
    }

    int num_vertices = 0;
    int32_t ch;
    while ((ch = utf8_read(&str)) != 0) {
        if (ch == -1) {
            continue;
        }
        const CharInfo *charinfo = get_charinfo(this->private, ch);
        if (!charinfo) {
            continue;
        }
        const float char_x = pos.x + charinfo->prekern * size_mult;
        if (charinfo->width > 0) {
            ASSERT(num_vertices < vertices_size, break);
            vertices[num_vertices+0].x = char_x;
            vertices[num_vertices+0].y = pos.y + charinfo->ascent*ascent_mult;
            vertices[num_vertices+0].z = pos.z;
            vertices[num_vertices+0].u = charinfo->uv0.x;
            vertices[num_vertices+0].v = charinfo->uv0.y;
            vertices[num_vertices+1].x = char_x + charinfo->width*size_mult;
            vertices[num_vertices+1].y = pos.y + charinfo->ascent*ascent_mult;
            vertices[num_vertices+1].z = pos.z;
            vertices[num_vertices+1].u = charinfo->uv1.x;
            vertices[num_vertices+1].v = charinfo->uv0.y;
            vertices[num_vertices+2].x = char_x + charinfo->width*size_mult;
            vertices[num_vertices+2].y = pos.y - charinfo->descent*ascent_mult;
            vertices[num_vertices+2].z = pos.z;
            vertices[num_vertices+2].u = charinfo->uv1.x;
            vertices[num_vertices+2].v = charinfo->uv1.y;
            vertices[num_vertices+3].x = char_x;
            vertices[num_vertices+3].y = pos.y - charinfo->descent*ascent_mult;
            vertices[num_vertices+3].z = pos.z;
            vertices[num_vertices+3].u = charinfo->uv0.x;
            vertices[num_vertices+3].v = charinfo->uv1.y;
            num_vertices += 4;
        }
        const float char_advance =
            charinfo->prekern + charinfo->width + charinfo->postkern;
        pos.x += char_advance * size_mult;
    }

    int primitive;
    if (num_vertices > 0) {
        primitive = graphics_create_primitive(
            GRAPHICS_PRIMITIVE_QUADS, vertices, vertex_format,
            sizeof(*vertices), num_vertices);
    } else {
        primitive = 0;
    }
    mem_free(vertices);
    if (num_vertices > 0 && UNLIKELY(!primitive)) {
        DLOG("Failed to create graphics primitive for text");
        return NULL;
    }

    Text *text = debug_mem_alloc(sizeof(*text), 0, 0,
                                 __FILE__, __LINE__, MEM_INFO_FONT);
    if (UNLIKELY(!text)) {
        DLOG("No memory for text primitive");
        graphics_destroy_primitive(primitive);
        return NULL;
    }
    text->texture_is_oneshot = 0;
    text->texture = this->private->texture;
    text->primitive = primitive;
    text->advance = pos.x - origin->x;
    return text;
}

/*-----------------------------------------------------------------------*/

static void font_bitmap_destroy(Font *this)
{
    ASSERT(this->private != NULL);

    texture_destroy(this->private->texture);
    mem_free(this->private);
    this->private = NULL;
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static void init_bitmap_font(Font *font)
{
    font->native_size     = font_bitmap_native_size;
    font->get_metrics     = font_bitmap_get_metrics;
    font->char_advance    = font_bitmap_char_advance;
    font->text_advance    = font_bitmap_text_advance;
    font->get_text_bounds = font_bitmap_get_text_bounds;
    font->render          = font_bitmap_render;
    font->destroy         = font_bitmap_destroy;
}

/*-----------------------------------------------------------------------*/

static const CharInfo *get_charinfo(const FontPrivate *private, int32_t ch)
{
    PRECOND(private != NULL, return NULL);

    const int page = ch >> 8;
    if (page >= private->num_charinfo_pages || !private->charinfo[page]) {
        return NULL;
    }
    return &private->charinfo[page][ch & 255];
}

/*************************************************************************/
/*************************************************************************/
