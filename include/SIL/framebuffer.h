/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/framebuffer.h: Header for framebuffer management.
 */

/*
 * Framebuffers provide offscreen targets for rendering graphics, and can
 * be used for purposes such as applying visual effects to rendered images
 * or capturing a copy of the dispaly before rendering UI or other overlay
 * elements.
 *
 * Call framebuffer_create() to create a new framebuffer of a given size.
 * The framebuffer can then be passed to framebuffer_bind() to cause all
 * render operations to target that framebuffer instead of the display;
 * passing 0 (which is never a valid framebuffer ID) to framebuffer_bind()
 * will cancel the binding and cause render operations to target the
 * display again.
 *
 * Once graphics have been rendered into a framebuffer, the framebuffer can
 * then be used as a texture for drawing to the display (or another
 * framebuffer); call framebuffer_get_texture() to retrieve a texture ID
 * for the framebuffer, which can then be applied like any other texture
 * with texture_apply().  The texture associated with a framebuffer is
 * treated as read-only with respect to texture functions, so its
 * properties cannot be modified; to toggle antialiasing on a framebuffer
 * texture, call framebuffer_set_antialias() instead.  (Framebuffers are
 * always treated as non-repeating with respect to texture coordinates.)
 *
 * Note that SIL does not currently support alpha channels in framebuffers;
 * attempting to reference the alpha channel of a framebuffer (from a
 * shader or with the DEST_ALPHA blend factor, for example) results in
 * undefined behavior.
 */

#ifndef SIL_FRAMEBUFFER_H
#define SIL_FRAMEBUFFER_H

EXTERN_C_BEGIN

/*************************************************************************/
/*************************************************************************/

/**
 * FramebufferColorType:  Constants for framebuffer color formats, used
 * with framebuffer_create().
 */
typedef enum FramebufferColorType {
    FBCOLOR_RGB8 = 1,   // 8 bits per channel, no alpha
    FBCOLOR_RGBA8,      // 8 bits per channel, with alpha
} FramebufferColorType;

/*-----------------------------------------------------------------------*/

/**
 * framebuffer_supported:  Return whether the platform supports offscreen
 * framebuffers.
 *
 * [Return value]
 *     True if offscreen framebuffers are supported, false if not.
 */
extern int framebuffer_supported(void);

/**
 * framebuffer_create:  Create a new offscreen framebuffer.
 *
 * depth_bits and stencil_bits give the minimum number of depth and
 * stencil bits per pixel to allocate in the framebuffer, similar to the
 * same-named display attributes used with graphics_set_display_attr().
 * The actual depth or stencil buffer resolution may be greater than that
 * requested, but will never be less; the create operation will fail if
 * the requested number of bits is not supported.  If either value is
 * zero, the system may choose not to create the corresponding buffer, in
 * which case any operations involving that buffer are undefined.
 *
 * [Parameters]
 *     width, height: Framebuffer size, in pixels.
 *     color_type: Framebuffer color type (FBCOLOR_*).
 *     depth_bits: Number of bits per pixel in the depth buffer.
 *     stencil_bits: Number of bits per pixel in the stencil buffer.
 * [Return value]
 *     Framebuffer ID (nonzero), or zero on error.
 */
extern int framebuffer_create(
    int width, int height, FramebufferColorType color_type,
    int depth_bits, int stencil_bits);

/**
 * framebuffer_destroy:  Destroy an offscreen framebuffer.  Does nothing
 * if framebuffer_id == 0.
 *
 * Destroying the currently bound (with framebuffer_bind()) framebuffer
 * cancels the binding, as if framebuffer_bind(0) had been called.  The
 * effect of destroying a framebuffer whose texture is applied mirrors
 * the behavior of texture_destroy() on an applied texture.
 *
 * [Parameters]
 *     framebuffer_id: ID of framebuffer to destroy.
 */
extern void framebuffer_destroy(int framebuffer_id);

/**
 * framebuffer_width, framebuffer_height:  Return the given framebuffer's
 * width or height.
 *
 * These functions may be called from any thread.
 *
 * [Parameters]
 *     framebuffer_id: ID of framebuffer for which to retrieve size.
 * [Return value]
 *     Width or height of framebuffer, in pixels.
 */
extern PURE_FUNCTION int framebuffer_width(int framebuffer_id);
extern PURE_FUNCTION int framebuffer_height(int framebuffer_id);

/**
 * framebuffer_bind:  Bind the given framebuffer to the current rendering
 * target.  If framebuffer_id == 0, cancel any framebuffer binding so that
 * the display is bound to the rendering target.
 *
 * Note that it is usually necessary to reset the render viewport with
 * graphics_set_viewport() after changing to a framebuffer of a different
 * size.
 *
 * The behavior of this function is undefined if it is not called between
 * graphics_start_frame() and graphics_finish_frame().
 *
 * [Parameters]
 *     framebuffer_id: ID of framebuffer to bind, or zero to cancel binding.
 */
extern void framebuffer_bind(int framebuffer_id);

/**
 * framebuffer_get_texture:  Return a texture ID which can be used with
 * rendering functions to refer to the given framebuffer's data.  The
 * returned ID remains owned by the framebuffer and must not be modified
 * by the caller.
 *
 * [Parameters]
 *     framebuffer_id: ID of framebuffer to return texture for.
 * [Return value]
 *     Texture ID for framebuffer.
 */
extern int framebuffer_get_texture(int framebuffer_id);

/**
 * framebuffer_set_antialias:  Set whether the framebuffer should be
 * antialiased when used as a texture.  Defaults to true (antialiasing
 * enabled).
 *
 * [Parameters]
 *     framebuffer_id: ID of framebuffer to modify.
 *     on: True to enable antialiasing, false to disable.
 */
extern void framebuffer_set_antialias(int framebuffer_id, int on);

/**
 * framebuffer_discard_data:  Discard all pixel data in the given
 * framebuffer.  This function serves as a hint to the graphics hardware
 * that the given framebuffer's contents do not need to be stored to
 * memory.  On some systems, calling this function when a framebuffer is
 * no longer needed (for example, after drawing the framebuffer to the
 * display) can improve performance.
 *
 * Calling this function has no effect on renderer output as long as no
 * attempt is made to read from the framebuffer before the next time it is
 * written to.  In particular, the framebuffer does not need to be unbound
 * before calling this function.
 *
 * [Parameters]
 *     framebuffer_id: ID of framebuffer whose contents may be discarded.
 */
extern void framebuffer_discard_data(int framebuffer_id);

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_FRAMEBUFFER_H
