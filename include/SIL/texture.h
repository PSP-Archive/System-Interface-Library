/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/texture.h: Header for texture manipulation routines.
 */

/*
 * As documented in graphics.h, all texture functions must be called from
 * the main thread unelss otherwise specified.
 */

#ifndef SIL_TEXTURE_H
#define SIL_TEXTURE_H

EXTERN_C_BEGIN
#ifdef __cplusplus
# define private private_  // Avoid errors when included from C++ code.
#endif

struct SysTexture;

/*************************************************************************/
/**************************** Texture formats ****************************/
/*************************************************************************/

/**
 * TEX_FORMAT_*:  Codes for texture data formats.  For packed 16-bit data
 * formats (RGB565, RGBA1555, and so on), the data must be in the byte
 * order expected by the graphics driver/hardware; there are no separate
 * big/little endian format codes.
 *
 * The only formats guaranteed to be supported by all systems (though
 * possibly through internal data conversion) are RGBA8888, A8, and
 * PALETTE8_RGBA8888.
 *
 * Note: These codes are used in TEX-format texture files, so DO NOT CHANGE
 * any of these values unless you want to break existing files!
 */
typedef enum TextureFormat {
    /* RGB/RGBA formats.  16-bit RGB[A] formats place R in the low bits of
     * the pixel value; 16-bit BGR[A] formats place B in the low bits. */
    TEX_FORMAT_RGBA8888                 = 0x00,
    TEX_FORMAT_RGB565                   = 0x01,
    TEX_FORMAT_RGBA5551                 = 0x02,
    TEX_FORMAT_RGBA4444                 = 0x03,
    TEX_FORMAT_BGRA8888                 = 0x08,
    TEX_FORMAT_BGR565                   = 0x09,
    TEX_FORMAT_BGRA5551                 = 0x0A,
    TEX_FORMAT_BGRA4444                 = 0x0B,

    /* Single-channel formats.  Note that when using custom shaders, both
     * alpha and luminance types are treated identically, with the single
     * component copied to the red ("r") field of the color vector.  The
     * separate types are used to tell the default SIL rendering pipeline
     * how to interpret the texture data. */
    TEX_FORMAT_A8                       = 0x40,  // Alpha only
    TEX_FORMAT_L8                       = 0x41,  // Luminance (grey)

    /* PSP-specific formats.  These guarantee that all images are located
     * at 64-byte-aligned offsets and have a line stride which is a
     * multiple of 16 bytes; the _SWIZZLED formats also have data swizzled
     * in the PSP style. */
    TEX_FORMAT_PSP_RGBA8888             = 0x70,
    TEX_FORMAT_PSP_RGB565               = 0x71,
    TEX_FORMAT_PSP_RGBA5551             = 0x72,
    TEX_FORMAT_PSP_RGBA4444             = 0x73,
    TEX_FORMAT_PSP_A8                   = 0x74,
    TEX_FORMAT_PSP_PALETTE8_RGBA8888    = 0x75,
    TEX_FORMAT_PSP_L8                   = 0x76,
    TEX_FORMAT_PSP_RGBA8888_SWIZZLED    = 0x78,
    TEX_FORMAT_PSP_RGB565_SWIZZLED      = 0x79,
    TEX_FORMAT_PSP_RGBA5551_SWIZZLED    = 0x7A,
    TEX_FORMAT_PSP_RGBA4444_SWIZZLED    = 0x7B,
    TEX_FORMAT_PSP_A8_SWIZZLED          = 0x7C,
    TEX_FORMAT_PSP_PALETTE8_RGBA8888_SWIZZLED = 0x7D,
    TEX_FORMAT_PSP_L8_SWIZZLED          = 0x7E,

    /* Compressed formats. */
    TEX_FORMAT_PALETTE8_RGBA8888        = 0x80,
    TEX_FORMAT_S3TC_DXT1                = 0x81,
    TEX_FORMAT_S3TC_DXT3                = 0x82,
    TEX_FORMAT_S3TC_DXT5                = 0x83,
    TEX_FORMAT_PVRTC2_RGBA              = 0x84,
    TEX_FORMAT_PVRTC4_RGBA              = 0x85,
    TEX_FORMAT_PVRTC2_RGB               = 0x86,
    TEX_FORMAT_PVRTC4_RGB               = 0x87,

    /* Values 0xE0 through 0xFF are available for use by client programs
     * to indicate custom texture formats.  All other values are reserved
     * for use by future versions of SIL. */
} TextureFormat;

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

/*-------------------- Texture creation and deletion --------------------*/

/**
 * texture_create:  Create a new 32-bit-color texture.  The width and
 * height may be any positive values, but the texture may not be suitable
 * for some rendering operations if the width and height are not both
 * powers of two.
 *
 * If MEM_ALLOC_CLEAR is specified in mem_flags, the texture is cleared to
 * transparent black (all components zero).  Otherwise, the pixel data is
 * left undefined.
 *
 * [Parameters]
 *     width: Texture width, in pixels.
 *     height: Texture height, in pixels.
 *     mem_flags: Memory allocation flags (MEM_ALLOC_*).
 *     mipmaps: True to enable automatic mipmap generation for this texture
 *         (if applicable to the system), false to prevent mipmap generation.
 * [Return value]
 *     ID of new texture (nonzero), or zero on error.
 */
extern int texture_create(int width, int height, int mem_flags, int mipmaps);

/**
 * texture_create_with_data:  Create a new texture, initializing it from an
 * existing pixel buffer.  The width and height may be any positive values,
 * but the texture may not be suitable for some rendering operations if the
 * width and height are not both powers of two.
 *
 * [Parameters]
 *     width: Texture width, in pixels.
 *     height: Texture height, in pixels.
 *     data: Pixel data.
 *     format: Pixel data format (TEX_FORMAT_*).
 *     stride: Pixel data line size, in pixels.  Ignored if not applicable
 *         to the format.
 *     mem_flags: Memory allocation flags (MEM_ALLOC_*; MEM_ALLOC_CLEAR is
 *         ignored).
 *     mipmaps: True to enable automatic mipmap generation for this texture
 *         (if applicable to the system), false to prevent mipmap generation.
 * [Return value]
 *     ID of new texture (nonzero), or zero on error.
 */
extern int texture_create_with_data(
    int width, int height, const void *data, TextureFormat format,
    int stride, int mem_flags, int mipmaps);

/**
 * texture_create_from_display:  Create a new texture containing pixel data
 * from a portion of the display.  The display data is copied to the new
 * texture such that pixel coordinate (x,y) is copied to texture coordinate
 * (0,0).
 *
 * If a framebuffer is currently bound (with framebuffer_bind()), pixel
 * data is read from that framebuffer.  However, it is usually more
 * efficient to simply use the framebuffer's texture directly; see
 * framebuffer_get_texture().
 *
 * This function must be called between graphics_start_frame() and
 * graphics_finish_frame(), or the contents of the new texture are
 * undefined.
 *
 * Due to platform-specific constraints, some OpenGL ES systems may be
 * unable to return texture data when using a size not equal to the
 * display size, even if readable is set to true.  Setting width and
 * height to multiples of large powers of two (16 or greater) will
 * generally reduce the risk of encountering this problem.
 *
 * [Parameters]
 *     x, y: Base display coordinates of region to copy, in pixels.
 *     w, h: Size of region to copy, in pixels.
 *     readable: False if the texture is not required to be readable (this
 *         may improve performance if the pixel data will never be read out).
 *     mem_flags: Memory allocation flags (MEM_ALLOC_*).
 *     mipmaps: True to enable automatic mipmap generation for this texture
 *         (if applicable to the system), false to prevent mipmap generation.
 * [Return value]
 *     ID of new texture (nonzero), or zero on error.
 */
extern int texture_create_from_display(
    int x, int y, int w, int h, int readable, int mem_flags, int mipmaps);

/**
 * texture_parse:  Parse the contents of a texture data file loaded into
 * memory and return a new texture.
 *
 * If the "reuse" flag is true, then this function takes ownership of the
 * data buffer (which must have been allocated using mem_alloc()).  If
 * possible, the data buffer will be reused for storing the texture data;
 * if reuse is not possible or if the function fails, the data buffer will
 * be freed.
 *
 * For TEX-format textures, if an opaque bitmap for the texture is stored
 * in the file, it will be loaded and used for texture_lock_readonly() and
 * texture_lock_readonly_partial() calls.  This allows program code to
 * take actions based on texture opacity data even on platforms where the
 * texture data itself cannot be read back into program memory.
 *
 * The MEM_ALLOC_CLEAR flag for mem_flags is ignored.
 *
 * [Parameters]
 *     data: File data buffer.
 *     len: File length, in bytes.
 *     mem_flags: Memory allocation flags (MEM_ALLOC_*).
 *     mipmaps: True to enable automatic mipmap generation for this texture
 *         (if applicable to the system), false to prevent mipmap generation.
 *     reuse: True to reuse the texture data buffer, false to allocate new
 *         memory for the data.
 * [Return value]
 *     ID of newly created texture (nonzero), or zero on error.
 */
extern int texture_parse(void *data, int32_t len, int mem_flags, int mipmaps,
                         int reuse);

/**
 * texture_destroy:  Destroy a texture.  Does nothing if texture_id is zero.
 *
 * Destroying a texture which is currently bound to rendering state (with
 * texture_apply()) is safe, but the effect is system-dependent: either
 * the texture will be immediately unbound and destroyed, or it will be
 * marked for destruction and destroyed when it is next unbound.
 *
 * [Parameters]
 *     texture_id: ID of texture to destroy.
 */
extern void texture_destroy(int texture_id);

/*-------------------- Texture information retrieval --------------------*/

/**
 * texture_width, texture_height:  Return the given texture's width or
 * height.
 *
 * These functions may be called from any thread.
 *
 * [Parameters]
 *     texture_id: ID of texture for which to retrieve size.
 * [Return value]
 *     Width or height of texture, in pixels.
 */
extern PURE_FUNCTION int texture_width(int texture_id);
extern PURE_FUNCTION int texture_height(int texture_id);

/**
 * texture_scale:  Return the size of this texture relative to the size at
 * which it is intended to be displayed.  Typically this is 1.0, indicating
 * that the texture resolution matches the intended display resolution, but
 * (for example) if a texture is stored at half-size in a data file, the
 * scale for that texture would be 0.5.
 *
 * This function may be called from any thread.
 *
 * [Parameters]
 *     texture_id: ID of texture for which to retrieve scale.
 * [Return value]
 *     Texture scale (size relative to intended display size).
 */
extern PURE_FUNCTION float texture_scale(int texture_id);

/**
 * texture_has_mipmaps:  Return whether the given texture has stored
 * mipmaps in addition to the base image.
 *
 * This function may be called from any thread.
 *
 * [Parameters]
 *     texture_id: ID of texture for which to get mipmap state.
 * [Return value]
 *     True if the texture has stored mipmaps, false if not.
 */
extern PURE_FUNCTION int texture_has_mipmaps(int texture_id);

/*----------------------- Pixel data manipulation -----------------------*/

/**
 * texture_lock:  Lock the texture's image data into memory, and return a
 * pointer to an array of 32-bit pixels in RGBA format (R, G, B, and A
 * bytes in that order).  The pixel data may be modified freely, but the
 * texture may not be used for drawing until the image data has been
 * released with texture_unlock().
 *
 * If the texture is currently bound to any texture unit in the rendering
 * pipeline (with texture_apply()), the state of those texture units
 * becomes undefined when the texture is locked, regardless of the type
 * of the lock operation.  The texture must be explicitly rebound with
 * texture_apply() after it has been unlocked.
 *
 * Accessing texture data from the CPU is a slow operation on many systems,
 * and it may force a sync operation (like graphics_sync()) in order to
 * copy the data, so it should be avoided when possible.  In particular,
 * locking a texture with a pixel format other than 32-bit RGBA in
 * read-write mode typically forces the texture to be recreated in RGBA
 * mode, requiring several round trips to the graphics hardware.  When
 * possible, use texture_lock_readonly() or texture_lock_writeonly() in
 * preference to this function.
 *
 * If an opaque bitmap is associated with this texture, this function
 * always fails; use texture_lock_readonly() or texture_lock_writeonly()
 * instead.
 *
 * [Parameters]
 *     texture_id: ID of texture to lock.
 * [Return value]
 *     Pointer to pixel data, or NULL on error.
 */
extern void *texture_lock(int texture_id);

/**
 * texture_lock_readonly:  Lock the texture's image data into memory, and
 * return a pointer to an array of 32-bit pixels in RGBA format.  Unlike
 * texture_lock(), the pixel data may _not_ be modified.  The texture may
 * not be used for drawing until the image data has been released with
 * texture_unlock().
 *
 * If an opaque bitmap is associated with this texture, this function
 * returns the data from the opaque bitmap instead of from the texture
 * itself (regardless of whether the system would allow the texture data
 * to be read).  Opaque pixels in the bitmap are returned as opaque white
 * (0xFFFFFFFF), and transparent pixels are returned as transparent black
 * (0x00000000).  When used on such a texture, this function does not incur
 * any delay with respect to the graphics hardware.
 *
 * [Parameters]
 *     texture_id: ID of texture to lock.
 * [Return value]
 *     Pointer to pixel data, or NULL on error.
 */
extern const void *texture_lock_readonly(int texture_id);

/**
 * texture_lock_readonly_partial:  Lock a portion the texture's image data
 * into memory, and return a pointer to an array of 32-bit pixels in RGBA
 * format.
 *
 * Aside from the region of data returned, this function behaves the same
 * as texture_lock_readonly().
 *
 * [Parameters]
 *     texture_id: ID of texture to lock.
 *     x, y, w, h: Coordinates and size of region to lock, in pixels.
 * [Return value]
 *     Pointer to pixel data, or NULL on error.
 */
extern const void *texture_lock_readonly_partial(
    int texture_id, int x, int y, int w, int h);

/**
 * texture_lock_writeonly:  Lock the texture's image data into memory, and
 * return a pointer to a buffer for storing 32-bit pixels in RGBA format.
 * The contents of the buffer are undefined.  The texture may not be used
 * for drawing until the image data has been released with texture_unlock().
 *
 * If the entire texture image is to be rewritten without reference to the
 * original pixel data, this function is typically faster than
 * texture_lock(), since it does not require any data copy or pixel
 * conversion steps.
 *
 * If an opaque bitmap is associated with this texture, this function
 * discards the opaque bitmap.  The texture will subsequently behave the
 * same as an ordinary texture created with texture_create().
 *
 * [Parameters]
 *     texture_id: ID of texture to lock.
 * [Return value]
 *     Pointer to pixel data buffer, or NULL on error.
 */
extern void *texture_lock_writeonly(int texture_id);

/**
 * texture_lock_raw:  Return a pointer to the internal SysTexture structure
 * for this texture.  The texture will be locked against changes as with
 * other texture_lock* functions, but no guarantees are made about the
 * state of the texture's image data.  The texture may not be used for
 * drawing until the data has been released with texture_unlock().
 *
 * This function is primarily intended for system-specific optimizations
 * of texture data manipulation routines.
 *
 * [Parameters]
 *     texture_id: ID of texture to lock.
 * [Return value]
 *     Pointer to corresponding SysTexture structure, or NULL on error.
 */
extern struct SysTexture *texture_lock_raw(int texture_id);

/**
 * texture_unlock:  Unlock a texture locked with any of the texture_lock...()
 * functions.  Does nothing if the texture is not locked.
 *
 * If the texture was locked for writing (from texture_lock() or
 * texture_lock_writeonly()), this function blocks until the new texture
 * data has been submitted to the graphics hardware.  If the system's
 * graphics API does not support 32-bit RGBA-format textures, this function
 * may take a significant amount of time to complete due to pixel format
 * conversion.
 *
 * [Parameters]
 *     texture_id: ID of texture to unlock.
 */
extern void texture_unlock(int texture_id);

/*-------------------------- Rendering control --------------------------*/

/**
 * texture_set_repeat:  Set whether texture coordinates should wrap around
 * the texture (thus repeating the texture over the texture coordinate
 * plane) or be clamped to the range [0,1].  Both parameters default to
 * true if not set for the particular texture.
 *
 * [Parameters]
 *     texture_id: ID of texture to modify.
 *     repeat_u: True to repeat horizontally, false to clamp.
 *     repeat_v: True to repeat vertically, false to clamp.
 */
extern void texture_set_repeat(int texture_id, int repeat_u, int repeat_v);

/**
 * texture_set_antialias:  Set whether the texture should be antialiased
 * when rendered.  Defaults to true (antialiasing enabled).
 *
 * [Parameters]
 *     texture_id: ID of texture to modify.
 *     on: True to enable antialiasing, false to disable.
 */
extern void texture_set_antialias(int texture_id, int on);

/**
 * texture_apply:  Set the given texture as the texture to be used for
 * subsequent rendering operations.
 *
 * unit selects which of (possibly multiple) texture units to operate on.
 * By default, SIL only uses texture unit 0; other units can be used for
 * multitextured rendering in custom shaders.  See also texture_num_units().
 *
 * If unit or texture_id is invalid, this function has no effect.
 *
 * [Parameters]
 *     unit: Texture unit to modify.
 *     texture_id: ID of texture to set, or zero to clear any previously
 *         set texture.
 */
extern void texture_apply(int unit, int texture_id);

/**
 * texture_num_units:  Return the number of texture units available for
 * rendering.  Valid texture unit IDs for texture_apply() range from zero
 * through one less than the value returned by this function.
 *
 * [Return value]
 *     Number of texture units available (always at least 1).
 */
extern int texture_num_units(void);

/*************************************************************************/
/*************************************************************************/

#ifdef __cplusplus
# undef private
#endif
EXTERN_C_END

#endif  // SIL_TEXTURE_H
