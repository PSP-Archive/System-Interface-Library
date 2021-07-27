/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/font/internal.h: Data and function declarations used internally by
 * font code.
 */

#ifndef SIL_SRC_FONT_INTERNAL_H
#define SIL_SRC_FONT_INTERNAL_H

struct Vector3f;

/*************************************************************************/
/**************************** Data structures ****************************/
/*************************************************************************/

/* Structure type which can be defined by individual font implementations
 * to hold private data. */
typedef struct FontPrivate FontPrivate;

/* Structure type for a text primitive (defined below). */
typedef struct Text Text;

/* Common structure for a font object. */
typedef struct Font Font;
struct Font {
    /*
     * Font creation functions must fill in all method pointers below.
     * All methods take a "this" pointer to the Font object on which the
     * methods are being called, though "this" is omitted from function
     * documentation for brevity.
     */

    /**
     * native_size:  Return the "native" size of this font -- in other
     * words, the size at which it is optimized for rendering.  For fonts
     * without a particular optimal or "native" size, return zero.
     *
     * [Return value]
     *     Native size, in pixels, or zero if none.
     */
    int (*native_size)(Font *this);

    /**
     * get_metrics:  Return this font's line height, baseline position,
     * ascent, and descent for the given font size.  All values are in
     * units of pixels and need not be integral.
     *
     * The font size is guaranteed to be positive, and all return pointers
     * are guaranteed to be non-NULL.
     *
     * [Parameters]
     *     size: Font size (typically character height).
     *     height_ret: Pointer to variable to receive the line height
     *         (distance between baselines of adjacent lines).
     *     baseline_ret: Pointer to variable to receive the baseline position
     *         (distance from the top of the line to the baseline).
     *     ascent_ret: Pointer to variable to receive the font ascent
     *         (maximum distance above the baseline of any character glyph).
     *     descent_ret: Pointer to variable to receive the font descent
     *         (maximum distance below the baseline of any character glyph).
     */
    void (*get_metrics)(Font *this, float size, float *height_ret,
                        float *baseline_ret, float *ascent_ret,
                        float *descent_ret);

    /**
     * char_advance:  Return the horizontal advance of the given Unicode
     * character (i.e., the distance from the rendering origin of the
     * character to the origin of a hypothetical next character) at the
     * given size.  If the character is not supported by the font, the
     * return value should indicate the value for the glyph (if any) that
     * would be drawn if that character was encountered in a string when
     * rendering.
     *
     * [Parameters]
     *     ch: Unicode character (codepoint, guaranteed to be nonnegative).
     *     size: Font size, in pixels (guaranteed to be positive).
     * [Return value]
     *     Horizontal advance, in pixels.
     */
    float (*char_advance)(Font *this, int32_t ch, float size);

    /**
     * text_advance:  Return the horizontal advance of the given text
     * string as it would be rendered at the given size.
     *
     * [Parameters]
     *     str: UTF-8 text string (guaranteed to be non-NULL).
     *     size: Font size, in pixels (guaranteed to be positive).
     * [Return value]
     *     Horizontal advance, in pixels.
     */
    float (*text_advance)(Font *this, const char *str, float size);

    /**
     * get_text_bounds:  Return the horizontal advance and left/right
     * bounds of the given text string as it would be rendered at the given
     * size.  All pointers are guaranteed to be non-NULL.
     *
     * [Parameters]
     *     str: UTF-8 text string (guaranteed to be non-NULL).
     *     size: Font size, in pixels (guaranteed to be positive).
     *     left_ret: Pointer to variable to receive the distance from the
     *         origin to the left edge of the text, in pixels.
     *     right_ret: Pointer to variable to receive the distance from the
     *         origin to the right edge of the text, in pixels.
     */
    void (*get_text_bounds)(Font *this, const char *str, float size,
                            float *left_ret, float *right_ret);

    /**
     * render:  Create and return a Text object to render the given text
     * string at the given origin.
     *
     * [Parameters]
     *     str: UTF-8 text string (guaranteed to be non-NULL).
     *     size: Font size, in pixels (guaranteed to be positive).
     *     origin: Origin coordinates (the Y coordinate corresponds to the
     *         font baseline).
     *     v_flip: True if Y coordinates increase going down; false if Y
     *         coordinates increase going up.
     * [Return value]
     *     New Text object, or NULL on error.
     */
    Text *(*render)(Font *this, const char *str, float size,
                    const struct Vector3f *origin, int v_flip);

    /**
     * destroy:  Destroy all resources associated with this font (but not
     * the Font object itself).
     */
    void (*destroy)(Font *this);

    /* Private data pointer for the font implementation (the implementation
     * is free to set this or not as it wishes). */
    FontPrivate *private;

    /*----------------------------------------------------------------------*/
    /* The font implementation should not touch any fields below this line. */
    /*----------------------------------------------------------------------*/

    /* Reference count for this font, used to keep the font in memory for
     * text primitives even after font_destroy() is called. */
    int ref_count;

    /* Cached metrics from the most recent get_metrics() call. */
    float cached_size;
    float cached_height, cached_baseline, cached_ascent, cached_descent;

    /* Antialias state for rendering. */
    uint8_t antialias;
};

/*-----------------------------------------------------------------------*/

/* Data structure for a text primitive. */
struct Text {
    /* Font used by this text primitive.  (This is set by the font core
     * functions and may be left alone by the font's render() method.) */
    Font *font;

    /* Flag indicating whether the texture should be destroyed along with
     * the Text object (the primitive is always destroyed). */
    uint8_t texture_is_oneshot;

    /* Texture ID to use, or zero if none is needed (e.g., for vector
     * fonts). */
    int texture;

    /* Graphics primitive ID to render, or zero if none (such as for a
     * whitespace-only string). */
    int primitive;

    /* Horizontal advance for this text. */
    float advance;
};

/*************************************************************************/
/******************* Font initializer implementations ********************/
/*************************************************************************/

/**
 * font_bitmap_init:  Initialize a new Font structure for a bitmap font.
 *
 * [Parameters]
 *     data: File data buffer.
 *     len: File length, in bytes.
 *     mem_flags: Memory allocation flags (MEM_ALLOC_*).
 *     reuse: True to reuse the data buffer, false to allocate new memory
 *         for the font.
 * [Return value]
 *     True on success, false on error.
 */
extern int font_bitmap_init(Font *this, void *data, int len, int mem_flags,
                            int reuse);

/**
 * font_freetype_init:  Initialize a new Font structure for a
 * FreeType-rendered font.
 *
 * [Parameters]
 *     data: File data buffer.
 *     len: File length, in bytes.
 *     mem_flags: Memory allocation flags (MEM_ALLOC_*).
 *     reuse: True to reuse the data buffer, false to allocate new memory
 *         for the font.
 * [Return value]
 *     True on success, false on error.
 */
extern int font_freetype_init(Font *this, void *data, int len, int mem_flags,
                              int reuse);

/**
 * font_sysfont_init:  Initialize a new Font structure for a system-provided
 * font.
 *
 * [Parameters]
 *     name: System-dependent font name, or the empty string for the
 *         default font.
 *     size: Desired font size, in pixels.
 *     mem_flags: Memory allocation flags (MEM_ALLOC_*).
 * [Return value]
 *     True on success, false on error.
 */
extern int font_sysfont_init(Font *this, const char *name, float size,
                             int mem_flags);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_FONT_INTERNAL_H
