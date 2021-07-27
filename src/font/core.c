/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/font/core.c: Core font management and text rendering routines.
 */

#include "src/base.h"
#include "src/font.h"
#include "src/font/internal.h"
#include "src/graphics.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/texture.h"
#include "src/utility/id-array.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Array of allocated fonts. */
static IDArray fonts = ID_ARRAY_INITIALIZER(10);

/**
 * VALIDATE_FONT:  Validate the font ID passed to a font routine, and store
 * the corresponding pointer in the variable "font".  If the font ID is
 * invalid, the "error_return" statement is executed; this may consist of
 * multiple statements, but must include a "return" to exit the function.
 */
#define VALIDATE_FONT(id,font,error_return) \
    ID_ARRAY_VALIDATE(&fonts, (id), Font *, font, \
                      DLOG("Font ID %d is invalid", _id); error_return)

/*-----------------------------------------------------------------------*/

/* Array of text primitives. */
static IDArray texts = ID_ARRAY_INITIALIZER(100);

/**
 * VALIDATE_TEXT:  Validate the text ID passed to a text routine, and store
 * the corresponding pointer in the variable "text".  If the text ID is
 * invalid, the "error_return" statement is executed; this may consist of
 * multiple statements, but must include a "return" to exit the function.
 */
#define VALIDATE_TEXT(id,text,error_return) \
    ID_ARRAY_VALIDATE(&texts, (id), Text *, text, \
                      DLOG("Text ID %d is invalid", _id); error_return)

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * font_parse_common:  Common processing for the font_parse_*() interface
 * functions.
 *
 * [Parameters]
 *     data: File data buffer.
 *     len: File length, in bytes.
 *     mem_flags: Memory allocation flags (MEM_ALLOC_*).
 *     reuse: True to reuse the data buffer, false to allocate new memory
 *         for the font.
 *     init: Font instance initializer function pointer.
 * [Return value]
 *     ID of newly created font (nonzero), or zero on error.
 */
static int font_parse_common(void *data, int len, int mem_flags, int reuse,
                             int (*init)(Font *, void *, int, int, int));

/**
 * new_font:  Allocate a new Font instance.  The new instance will have a
 * reference count of 0.
 *
 * [Parameters]
 *     mem_flags: Memory allocation flags (MEM_ALLOC_*).
 * [Return value]
 *     Newly allocated Font instance, or NULL on error.
 */
static Font *new_font(int mem_flags);

/**
 * validate_font:  Check that all method pointers for the given Font
 * instance have been initialized.
 *
 * [Parameters]
 *     font: Font to check.
 * [Return value]
 *     True if all method pointers have been initialized, false if not.
 */
static int validate_font(const Font *font);

/**
 * register_font:  Register the given font in the global font ID table and
 * increment the font's reference count.
 *
 * [Parameters]
 *     font: Font to register.
 * [Return value]
 *     Font ID assigned to the font, or zero on error.
 */
static int register_font(Font *font);

/**
 * ref_font:  Increment the reference count of the given font.
 *
 * [Parameters]
 *     font: Font object pointer.
 */
static void ref_font(Font *font);

/**
 * unref_font:  Decrement the reference count of the given font.  If the
 * reference count reaches zero, the font's resources are freed.
 *
 * [Parameters]
 *     font: Font object pointer.
 */
static void unref_font(Font *font);

/**
 * cache_metrics:  Cache the metrics for the given font at the given size
 * if they are not already cached.
 *
 * [Parameters]
 *     font: Font for which to cache metrics.
 *     size: Font size for which to cache metrics, in pixels.
 */
static void cache_metrics(Font *font, float size);

/**
 * destroy_text:  Destroy a Text object given its pointer.
 *
 * [Parameters]
 *     text: Text object pointer.
 */
static void destroy_text(Text *text);

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int font_parse_bitmap(void *data, int len, int mem_flags, int reuse)
{
    return font_parse_common(data, len, mem_flags, reuse, font_bitmap_init);
}

/*-----------------------------------------------------------------------*/

int font_parse_freetype(void *data, int len, int mem_flags, int reuse)
{
    return font_parse_common(data, len, mem_flags, reuse, font_freetype_init);
}

/*-----------------------------------------------------------------------*/

int font_create_sysfont(const char *name, float size, int mem_flags)
{
    if (UNLIKELY(!name)) {
        DLOG("name == NULL");
        return 0;
    }
    if (UNLIKELY(size <= 0)) {
        DLOG("Invalid size %g (must be positive)", size);
        return 0;
    }

    Font *font = new_font(mem_flags);
    if (UNLIKELY(!font)) {
        return 0;
    }
    if (!font_sysfont_init(font, name, size, mem_flags)) {
        DLOG("Failed to create font");
        mem_free(font);
        return 0;
    }
    ASSERT(validate_font(font), return 0);
    return register_font(font);
}

/*-----------------------------------------------------------------------*/

void font_destroy(int font_id)
{
    if (font_id) {
        Font *font;
        VALIDATE_FONT(font_id, font, return);
        unref_font(font);
        id_array_release(&fonts, font_id);
    }
}

/*-----------------------------------------------------------------------*/

int font_native_size(int font_id)
{
    Font *font;
    VALIDATE_FONT(font_id, font, return 0);
    const int size = font->native_size(font);
    if (size > 0) {
        return size;
    } else {
        return 12;  // Arbitrary nonzero value.
    }
}

/*-----------------------------------------------------------------------*/

float font_height(int font_id, float size)
{
    Font *font;
    VALIDATE_FONT(font_id, font, return 0);
    if (UNLIKELY(size <= 0)) {
        DLOG("Invalid size: %g", size);
        return 0;
    }

    cache_metrics(font, size);
    return font->cached_height;
}

/*-----------------------------------------------------------------------*/

float font_baseline(int font_id, float size)
{
    Font *font;
    VALIDATE_FONT(font_id, font, return 0);
    if (UNLIKELY(size <= 0)) {
        DLOG("Invalid size: %g", size);
        return 0;
    }

    cache_metrics(font, size);
    return font->cached_baseline;
}

/*-----------------------------------------------------------------------*/

float font_ascent(int font_id, float size)
{
    Font *font;
    VALIDATE_FONT(font_id, font, return 0);
    if (UNLIKELY(size <= 0)) {
        DLOG("Invalid size: %g", size);
        return 0;
    }

    cache_metrics(font, size);
    return font->cached_ascent;
}

/*-----------------------------------------------------------------------*/

float font_descent(int font_id, float size)
{
    Font *font;
    VALIDATE_FONT(font_id, font, return 0);
    if (UNLIKELY(size <= 0)) {
        DLOG("Invalid size: %g", size);
        return 0;
    }

    cache_metrics(font, size);
    return font->cached_descent;
}

/*-----------------------------------------------------------------------*/

float font_char_advance(int font_id, int32_t ch, float size)
{
    Font *font;
    VALIDATE_FONT(font_id, font, return 0);
    if (UNLIKELY(ch < 0)) {
        DLOG("Invalid character %d", ch);
        return 0;
    }
    return font->char_advance(font, ch, size);
}

/*-----------------------------------------------------------------------*/

float font_text_width(int font_id, const char *str, float size)
{
    Font *font;
    VALIDATE_FONT(font_id, font, return 0);
    if (UNLIKELY(!str)) {
        DLOG("str == NULL");
        return 0;
    }
    float left, right;
    font->get_text_bounds(font, str, size, &left, &right);
    return right - left;
}

/*-----------------------------------------------------------------------*/

void font_set_antialias(int font_id, int antialias)
{
    Font *font;
    VALIDATE_FONT(font_id, font, return);
    font->antialias = (antialias != 0);
}

/*-----------------------------------------------------------------------*/

int font_create_text(int font_id, const char *str, float size,
                     const Vector3f *origin, FontAlignment align, int v_flip)
{
    Font *font;
    VALIDATE_FONT(font_id, font, return 0);
    if (UNLIKELY(!str)) {
        DLOG("str == NULL");
        return 0;
    } else if (UNLIKELY(!origin)) {
        DLOG("origin == NULL");
        return 0;
    } else if (UNLIKELY(align != FONT_ALIGN_LEFT && align != FONT_ALIGN_CENTER
                        && align != FONT_ALIGN_RIGHT)) {
        DLOG("Invalid value for align: %d (must be %d, %d, or %d)",
             align, FONT_ALIGN_LEFT, FONT_ALIGN_CENTER, FONT_ALIGN_RIGHT);
        return 0;
    }

    Vector3f pos = *origin;
    if (align == FONT_ALIGN_CENTER) {
        float left, right;
        font->get_text_bounds(font, str, size, &left, &right);
        pos.x -= (right + left) / 2;
    } else if (align == FONT_ALIGN_RIGHT) {
        pos.x -= font->text_advance(font, str, size);
    }

    Text *text = font->render(font, str, size, &pos, v_flip);
    if (!text) {
        return 0;
    }
    texture_set_antialias(text->texture, font->antialias);
    text->font = font;
    ref_font(font);

    if (align == FONT_ALIGN_RIGHT) {
        text->advance = 0;  // Avoid rounding error.
    } else if (align == FONT_ALIGN_CENTER) {
        text->advance -= origin->x - pos.x;
    }

    const int text_id = id_array_register(&texts, text);
    if (UNLIKELY(!text_id)) {
        DLOG("Failed to register text primitive");
        destroy_text(text);
        return 0;
    }

    return text_id;
}

/*-----------------------------------------------------------------------*/

float font_render_text(int font_id, const char *str, float size,
                       const Vector3f *origin, FontAlignment align, int v_flip)
{
    if (UNLIKELY(!origin)) {
        DLOG("origin == NULL");
        return 0;
    }
    Font *font;
    VALIDATE_FONT(font_id, font, return origin->x);
    if (UNLIKELY(!str)) {
        DLOG("str == NULL");
        return origin->x;
    } else if (UNLIKELY(align != FONT_ALIGN_LEFT && align != FONT_ALIGN_CENTER
                        && align != FONT_ALIGN_RIGHT)) {
        DLOG("Invalid value for align: %d (must be %d, %d, or %d)",
             align, FONT_ALIGN_LEFT, FONT_ALIGN_CENTER, FONT_ALIGN_RIGHT);
        return origin->x;
    }

    if (!*str) {
        return origin->x;
    }

    const int text_id =
        font_create_text(font_id, str, size, origin, align, v_flip);
    if (UNLIKELY(!text_id)) {
        return origin->x;
    }
    text_render(text_id);
    const float new_x = origin->x + text_advance(text_id);
    text_destroy(text_id);
    return new_x;
}

/*************************************************************************/
/****************** Interface: Text primitive routines *******************/
/*************************************************************************/

void text_render(int text_id)
{
    const Text *text;
    VALIDATE_TEXT(text_id, text, return);
    if (text->primitive) {
        texture_apply(0, text->texture);
        graphics_set_texture_offset(&(Vector2f){0, 0});
        graphics_draw_primitive(text->primitive);
    }
}

/*-----------------------------------------------------------------------*/

float text_advance(int text_id)
{
    const Text *text;
    VALIDATE_TEXT(text_id, text, return 0);
    return text->advance;
}

/*-----------------------------------------------------------------------*/

void text_destroy(int text_id)
{
    Text *text;
    VALIDATE_TEXT(text_id, text, return);
    destroy_text(text);
    id_array_release(&texts, text_id);
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static int font_parse_common(void *data, int len, int mem_flags, int reuse,
                             int (*init)(Font *, void *, int, int, int))
{
    if (UNLIKELY(!data)) {
        DLOG("data == NULL");
        return 0;
    }

    Font *font = new_font(mem_flags);
    if (UNLIKELY(!font)) {
        if (reuse) {
            mem_free(data);
        }
        return 0;
    }
    if (!(*init)(font, data, len, mem_flags, reuse)) {
        DLOG("Failed to create font");
        mem_free(font);
        return 0;
    }
    ASSERT(validate_font(font), return 0);
    return register_font(font);
}

/*-----------------------------------------------------------------------*/

static Font *new_font(int mem_flags)
{
    Font *font = debug_mem_alloc(sizeof(*font), 0, mem_flags | MEM_ALLOC_CLEAR,
                                 __FILE__, __LINE__, MEM_INFO_FONT);
    if (UNLIKELY(!font)) {
        DLOG("No memory for font object");
        return NULL;
    }
    font->antialias = 1;
    return font;
}

/*-----------------------------------------------------------------------*/

static int validate_font(const Font *font)
{
    ASSERT(font->native_size     != NULL, return 0);
    ASSERT(font->get_metrics     != NULL, return 0);
    ASSERT(font->char_advance    != NULL, return 0);
    ASSERT(font->text_advance    != NULL, return 0);
    ASSERT(font->get_text_bounds != NULL, return 0);
    ASSERT(font->render          != NULL, return 0);
    ASSERT(font->destroy         != NULL, return 0);
    return 1;
}

/*-----------------------------------------------------------------------*/

static int register_font(Font *font)
{
    const int id = id_array_register(&fonts, font);
    if (UNLIKELY(!id)) {
        font->destroy(font);
        mem_free(font);
        return 0;
    }
    ref_font(font);
    return id;
}

/*-----------------------------------------------------------------------*/

static void ref_font(Font *font)
{
    font->ref_count++;
}

/*-----------------------------------------------------------------------*/

static void unref_font(Font *font)
{
    font->ref_count--;
    if (font->ref_count <= 0) {
        font->destroy(font);
        mem_free(font);
    }
}

/*-----------------------------------------------------------------------*/

static void cache_metrics(Font *font, float size)
{
    PRECOND(font != NULL, return);
    PRECOND(size > 0, return);

    if (font->cached_size != size) {
        font->cached_size = size;
        font->get_metrics(
            font, size, &font->cached_height, &font->cached_baseline,
            &font->cached_ascent, &font->cached_descent);
    }
}

/*-----------------------------------------------------------------------*/

static void destroy_text(Text *text)
{
    PRECOND(text != NULL, return);

    graphics_destroy_primitive(text->primitive);
    if (text->texture_is_oneshot) {
        texture_destroy(text->texture);
    }
    unref_font(text->font);
    mem_free(text);
}

/*************************************************************************/
/*************************************************************************/
