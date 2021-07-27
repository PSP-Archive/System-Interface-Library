/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/font.h: Header for font management.
 */

/*
 * The functions declared in this file implement text rendering.  The
 * following types of fonts are supported:
 *
 * - Bitmap fonts, in which each glyph (character) is part of a single
 *   texture.  Bitmap fonts are the fastest to render, but they may appear
 *   blurry if not drawn at their native size as encoded in the texture.
 *   Bitmap font files can be created with the "makefont" tool in the
 *   tools directory of the SIL source code.
 *
 * - Any font type supported by the FreeType library, such as TrueType and
 *   OpenType fonts.  These fonts are slower to render, since the glyph
 *   data must be converted to pixel data on each render call.  However,
 *   the "text primitive" interface (font_create_text() and text_render())
 *   allows most of this overhead to be avoided for static text by caching
 *   the pixel data.
 *
 * - A system-supplied font (currently available only on Android, iOS, and
 *   Linux).  These have the same overhead of converting to pixel data as
 *   FreeType fonts.
 *
 * Right-to-left text is not currently supported.
 *
 * ======== Font object management ========
 *
 * Font objects are created using the function appropriate to the font
 * type: font_parse_bitmap() for bitmap fonts, font_parse_freetype() for
 * FreeType-rendered fonts, or font_create_sysfont() for system-provided
 * fonts.  These functions return a font ID, an integer used to refer to
 * the font in subsequent function calls.  Note that client code will
 * typically create bitmap or FreeType font objects using the
 * resource_load_*_font() functions rather than calling these creation
 * functions directly; see resource.h for details.
 *
 * Font objects can be destroyed by calling font_destroy() on the font ID.
 * Fonts loaded via a resource manager should be destroyed by calling
 * resource_free() on the associated resource ID.
 *
 * All metric- and rendering-related functions take a "size" parameter in
 * addition to the font ID itself.  Roughly speaking, this "size" is the
 * desired vertical size of rendered glyphs, in pixels; for bitmap fonts,
 * it corresponds to the height encoded in the font file, and for TrueType
 * and similar fonts, it corresponds to the point size of the font.  For
 * bitmap fonts, font_native_size() returns the size value at which the
 * font will be rendered without scaling.  (font_native_size() can be used
 * on other font types as well, but for scalable fonts, the value it
 * returns has no special meaning.)
 *
 * Vertical positioning information is provided by the font_height(),
 * font_baseline(), font_ascent(), and font_descent() functions.  In
 * particular, font_height() returns the intended vertical offset between
 * consecutive lines, which may be different from the size value.
 *
 * Horizontal positioning information is provided by the font_char_advance()
 * and font_text_width() functions.  As the names indicate, these functions
 * return slightly different values: font_char_advance() returns the
 * _horizontal advance_ for the requested character, while font_text_width()
 * returns the _rendered width_ of the requested string.
 *
 * All of the above metrics functions return values in the same units as
 * the size value, namely pixels of rendered text.  These correspond to
 * display coordinates only if the rendering pipeline has been configured
 * with a parallel projection matrix (see graphics_set_parallel_projection())
 * whose size is equal to the display size.
 *
 * Antialiasing of rendered text can also be controlled by calling
 * font_set_antialias().  This is primarily useful for bitmap fonts which
 * are intended to be rendered with a pixelated look.
 *
 * ======== Text rendering ========
 *
 * Simple text rendering can be done by calling font_render_text().  This
 * function takes the string to be rendered, the font size, and three
 * positioning arguments: an origin coordinate, an alignment specifier,
 * and a flag indicating whether Y coordinates increase in the "up" or
 * "down" direction (to handle both lower-left-origin and upper-left-origin
 * coordinate systems).
 *
 * For non-bitmap fonts, calling font_render_text() entails rendering the
 * requested string to a texture on each call.  To avoid this overhead,
 * instead call font_create_text() with the same arguments, which returns
 * a text primitive ID.  The text primitive includes the rendered texture
 * for such fonts, so drawing it to the display (with text_render()) has
 * no more overhead than drawing any other simple graphics primitive.  The
 * horizontal advance for a text primitive can also be retrieved, by
 * calling text_advance().  When a text primitive is no longer needed, it
 * can be destroyed with text_destroy().
 *
 * When shader objects are in use, the caller must ensure that an
 * appropriate shader pipeline has been installed before calling
 * font_render_text() or text_render().  The vertex attributes vary
 * depending on the font type:
 *    - Bitmap fonts use a 3-float position and 2-float texture
 *      coordinate, with the font texture (of whatever format was stored
 *      in the font file) in texture unit 0.
 *    - FreeType fonts use a 3-float position and 2-float texture
 *      coordinate, with a TEX_FORMAT_A8 texture in texture unit 0.
 *    - System fonts use a 3-float position and 2-float texture
 *      coordinate, with a texture in texture unit 0.  The texture
 *      format is system-dependent, though all currently supported
 *      systems use TEX_FORMAT_A8.
 * In all cases above, the client must bind the relevant SIL vertex
 * attributes to appropriate attributes in the vertex shader using
 * shader_bind_standard_attribute().
 */

#ifndef SIL_FONT_H
#define SIL_FONT_H

EXTERN_C_BEGIN

struct Vector3f;

/*************************************************************************/
/*************************************************************************/

/*----------------------------- Data types ------------------------------*/

/**
 * FontAlignment:  Constants indicating how rendered text should be
 * aligned with respect to the origin coordinates.
 */
typedef enum FontAlignment {
    /* Text is drawn from the origin. */
    FONT_ALIGN_LEFT,
    /* Text is drawn such that the origin X coordinate is in the center of
     * the rendered string. */
    FONT_ALIGN_CENTER,
    /* Text is drawn such that the origin of a hypothetical character
     * immediately following the string would coincide with the rendering
     * origin. */
    FONT_ALIGN_RIGHT,
} FontAlignment;

/*---------------------- Font management routines -----------------------*/

/**
 * font_parse_bitmap:  Parse the contents of a bitmap font data file loaded
 * into memory and return a font ID for the font.
 *
 * If the "reuse" flag is true, then this function takes ownership of the
 * data buffer (which must have been allocated using mem_alloc()).  If
 * possible, the data buffer will be reused for storing the font data; if
 * reuse is not possible or if the function fails, the data buffer will be
 * freed.
 *
 * This function may only be called from the main thread.
 *
 * [Parameters]
 *     data: File data buffer.
 *     len: File length, in bytes.
 *     mem_flags: Memory allocation flags (MEM_ALLOC_*).
 *     reuse: True to reuse the data buffer, false to allocate new memory
 *         for the font.
 * [Return value]
 *     ID of newly created font (nonzero), or zero on error.
 */
extern int font_parse_bitmap(void *data, int len, int mem_flags, int reuse);

/**
 * font_parse_freetype:  Parse the contents of a FreeType-supported font
 * file loaded into memory and return a font ID for the font.
 *
 * If the "reuse" flag is true, then this function takes ownership of the
 * data buffer (which must have been allocated using mem_alloc()).  If
 * possible, the data buffer will be reused for storing the font data; if
 * reuse is not possible or if the function fails, the data buffer will be
 * freed.
 *
 * This function may only be called from the main thread.
 *
 * [Parameters]
 *     data: File data buffer.
 *     len: File length, in bytes.
 *     mem_flags: Memory allocation flags (MEM_ALLOC_*).
 *     reuse: True to reuse the data buffer, false to allocate new memory
 *         for the font.
 * [Return value]
 *     ID of newly created font (nonzero), or zero on error.
 */
extern int font_parse_freetype(void *data, int len, int mem_flags, int reuse);

/**
 * font_create_sysfont:  Create and return a font ID for a system-provided
 * font.
 *
 * This function may only be called from the main thread.
 *
 * [Parameters]
 *     name: System-dependent font name, or the empty string for the
 *         default font.
 *     size: Desired font size, in pixels.
 *     mem_flags: Memory allocation flags (MEM_ALLOC_*).
 * [Return value]
 *     ID of newly created font (nonzero), or zero on error.
 */
extern int font_create_sysfont(const char *name, float size, int mem_flags);

/**
 * font_destroy:  Destroy a font object.  Does nothing if font_id is zero.
 *
 * This function may only be called from the main thread.
 *
 * [Parameters]
 *     font_id: Font to destroy.
 */
extern void font_destroy(int font_id);

/**
 * font_native_size:  Return the "native" size of the given font -- in
 * other words, the size at which it is optimized for rendering.  For a
 * bitmap font, this is the size at which character glyphs are rendered
 * without scaling.
 *
 * [Parameters]
 *     font_id: Font to use.
 * [Return value]
 *     Native font size, in pixels.
 */
extern int font_native_size(int font_id);

/**
 * font_height:  Return the line height in pixels for text drawn in the
 * given font at the given font size.
 *
 * The "size" used here and in other font functions is, roughly speaking,
 * the height in pixels of a line of text rendered using that font.  The
 * precise mapping between size and the line height value returned by this
 * function depends on the specific font; for example, if a font is
 * normally rendered with extra space between lines, that space will
 * typically be included in this value but not in the size value.
 *
 * Note that font sizes do not need to be integral, and the return value of
 * this and other size-type functions may likewise not be integral.
 *
 * [Parameters]
 *     font_id: Font to use.
 *     size: Font size, in pixels.
 * [Return value]
 *     Line height, in pixels.
 */
extern float font_height(int font_id, float size);

/**
 * font_baseline:  Return the Y offset in pixels from the top of the line
 * to the font's baseline for the given font at the given size.
 *
 * [Parameters]
 *     font_id: Font to use.
 *     size: Font size, in pixels.
 * [Return value]
 *     Font baseline, in pixels.
 */
extern float font_baseline(int font_id, float size);

/**
 * font_ascent:  Return the maximum height in pixels above the baseline of
 * any character (glyph) in the given font at the given size.
 *
 * [Parameters]
 *     font_id: Font to use.
 *     size: Font size, in pixels.
 * [Return value]
 *     Font ascent, in pixels.
 */
extern float font_ascent(int font_id, float size);

/**
 * font_descent:  Return the maximum height in pixels below the baseline of
 * any character (glyph) in the given font at the given size.
 *
 * [Parameters]
 *     font_id: Font to use.
 *     size: Font size, in pixels.
 * [Return value]
 *     Font descent, in pixels.
 */
extern float font_descent(int font_id, float size);

/**
 * font_char_advance:  Return the horizontal advance of the given character
 * (the distance from the origin of this character to the origin of the
 * hypothetical next character) in the given font at the given size.
 *
 * [Parameters]
 *     font_id: Font to use.
 *     ch: Unicode character (codepoint).
 *     size: Font size, in pixels.
 * [Return value]
 *     Character width, in pixels.
 */
extern float font_char_advance(int font_id, int32_t ch, float size);

/**
 * font_text_width:  Return the width of the given text string if rendered
 * in the given font at the given size.
 *
 * [Parameters]
 *     font_id: Font to use.
 *     text: Text to calculate width of (UTF-8 encoded).
 *     size: Font size, in pixels.
 * [Return value]
 *     Text width, in pixels.
 */
extern float font_text_width(int font_id, const char *text, float size);

/**
 * font_set_antialias:  Set whether the given font should be antialiased
 * when rendered.  Antialiasing is enabled by default.
 *
 * [Parameters]
 *     font_id: Font to operate on.
 *     antialias: True to antialias rendered text, false to not antialias.
 */
extern void font_set_antialias(int font_id, int antialias);

/**
 * font_create_text:  Create a text primitive (analagous to a graphics
 * primitive such as lines or triangles) which can be used to render the
 * the given text to the display.
 *
 * This function may only be called from the main thread.
 *
 * [Parameters]
 *     font_id: Font to use.
 *     str: Text string to render (UTF-8 encoded).
 *     size: Font size, in pixels.
 *     origin: Origin coordinates.  The X coordinate is the left edge,
 *         center, or right edge of the text, depending on the selected
 *         alignment; the Y coordinate corresponds to the font baseline.
 *     align: Text alignment with respect to the origin (FONT_ALIGN_*).
 *     v_flip: True to flip the text vertically (useful when the display
 *         coordinate system has the origin in the upper-left corner).
 * [Return value]
 *     Text primitive ID, or zero on error.
 */
extern int font_create_text(
    int font_id, const char *str, float size,
    const struct Vector3f *origin, FontAlignment align, int v_flip);

/**
 * font_render_text:  Render the given text to the display.  Equivalent to:
 *    text_render(text_id = font_create_text(...));
 *    text_destroy(text_id);
 *
 * After calling this function, the current texture state (applied texture
 * and texture offsets) is undefined.
 *
 * This function may only be called from the main thread.
 *
 * [Parameters]
 *     font_id: Font to use.
 *     str: Text string to render (UTF-8 encoded).
 *     size: Font size, in pixels.
 *     origin: Origin coordinates.  The X coordinate is the left edge,
 *         center, or right edge of the text, depending on the selected
 *         alignment; the Y coordinate corresponds to the font baseline.
 *     align: Text alignment with respect to the origin (FONT_ALIGN_*).
 *     v_flip: True to flip the text vertically (useful when the display
 *         coordinate system has the origin in the upper-left corner).
 * [Return value]
 *     X coordinate of the right edge of the text.
 */
extern float font_render_text(
    int font_id, const char *str, float size,
    const struct Vector3f *origin, FontAlignment align, int v_flip);

/*----------------------- Text primitive routines -----------------------*/

/**
 * text_render:  Render a text primitive created with font_create_text().
 *
 * After calling this function, the current texture state (applied texture
 * and texture offsets) is undefined.
 *
 * This function may only be called from the main thread.
 *
 * [Parameters]
 *     text_id: Text primitive to render.
 */
extern void text_render(int text_id);

/**
 * text_advance:  Return the distance from the rendering origin to the right
 * edge of the text.  (For right-aligned text, this will always be zero.)
 *
 * [Parameters]
 *     text_id: Text primitive to render.
 * [Return value]
 *     Horizontal advance for text, in pixels.
 */
extern float text_advance(int text_id);

/**
 * text_destroy:  Destroy a text primitive.  Does nothing if text_id is zero.
 *
 * This function may only be called from the main thread.
 *
 * [Parameters]
 *     text_id: Text primitive to destroy.
 */
extern void text_destroy(int text_id);

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_FONT_H
