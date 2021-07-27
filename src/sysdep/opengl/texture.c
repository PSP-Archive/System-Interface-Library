/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/opengl/texture.c: Texture manipulation functionality for
 * OpenGL-based platforms.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/graphics.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/opengl/internal.h"
#include "src/sysdep/opengl/opengl.h"
#include "src/texture.h"
#include "src/utility/pixformat.h"

/*************************************************************************/
/****************** Global data (only used for testing) ******************/
/*************************************************************************/

#ifdef SIL_INCLUDE_TESTS
uint8_t TEST_opengl_always_convert_texture_data = 0;
#endif

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Local routine declarations. */

/**
 * create_gl_texture:  Create an OpenGL texture object corresponding to
 * the given texture configuration.  The parameters correspond to the
 * SysTexture fields of the same names.
 *
 * [Parameters]
 *     width, height: Texture size, in pixels.
 *     color_type: Texture color type (TEXCOLOR_*).
 *     has_mipmaps: True to allocate mipmap storage for the texture.
 *     repeat_u: True if the texture should repeat horizontally.
 *     repeat_v: True if the texture should repeat vertically.
 *     antialias: True if the texture should be antialiased.
 * [Return value]
 *     Newly created OpenGL texture, or 0 on error.
 */
static GLuint create_gl_texture(
    int width, int height, int color_type, int has_mipmaps,
    int repeat_u, int repeat_v, int antialias);

/**
 * read_texture_via_framebuffer:  Read the given texture's data by
 * attaching the texture to a framebuffer and calling glReadPixels() on
 * the framebuffer.
 *
 * [Parameters]
 *     texture: Texture to read from.
 *     x, y, w, h: Coordinates of region to read, in pixels.
 *     buffer: Output pixel buffer (RGBA format).
 * [Return value]
 *     True on success, false on error.
 */
static int read_texture_via_framebuffer(
    SysTexture *texture, int x, int y, int w, int h, void *buffer);

/**
 * update_texture:  Update the OpenGL texture object for the given texture
 * using the data in the given pixel buffer, generating mipmaps if the
 * texture's auto_mipmaps flag is set.
 *
 * The input pixel buffer is assumed to have been allocated with
 * mem_alloc(), and will be freed with mem_free() before return.  While
 * this does complicate the caller's logic, it reduces peak memory usage
 * by releasing the local copy of the higher-resolution data before
 * sending the lower-resolution data to the GL.
 *
 * [Parameters]
 *     texture: Texture to update.
 *     pixels: Pixel data to store; will be freed before return.
 */
static void update_texture(SysTexture *texture, uint8_t *pixels);

/**
 * generate_mipmaps:  Generate mipmaps for the given texture (down to size
 * 1x1) and register them with the OpenGL driver.
 *
 * [Parameters]
 *     texture_id: OpenGL texture ID.
 *     pixels: Pixel data buffer, in RGBA format; will be freed before return.
 *     offset: Byte offset from "pixels" to start of pixel data.
 *     width: Width of current texture image, in pixels.
 *     height: Height of current texture image, in pixels.
 *     level: Mipmap level of current texture image (0 = primary image).
 */
static void generate_mipmaps(GLuint texture_id, uint8_t *pixels,
                             int32_t offset, int width, int height, int level);

/*************************************************************************/
/*************** Interface: Texture creation and deletion ****************/
/*************************************************************************/

SysTexture *sys_(texture_create)(
    int width, int height, TextureFormat data_format, int num_levels,
    void *data, int stride, const int32_t *level_offsets,
    const int32_t *level_sizes, int mipmaps, int mem_flags, int reuse)
{
    /* Determine the GL format parameters for the texture. */

    GLenum gl_internalformat, gl_format, gl_type;
    int color_type, bpp, input_bpp = 0;
    int is_palette = 0, is_compressed = 0;
    PixelConvertFunc *convert_func = NULL;

#ifdef SIL_INCLUDE_TESTS
    const int force_convert = TEST_opengl_always_convert_texture_data;
#else
    const int force_convert = 0;
#endif

    /* This is used to detect an invalid format without needing a "default"
     * case (which would break the compiler's missing-case check). */
    bpp = 0;

    switch (data_format) {

      case TEX_FORMAT_RGBA8888:
        /* Note that the GLES 2.0 spec doesn't allow sized formats as the
         * internalformat argument to glTexImage2D(), but the only two GLES
         * platforms we support (Android and iOS) both allow them, so we
         * deliberately deviate from the spec to reduce conditional
         * compilation in some cases and explicitly request narrow pixels
         * (to reduce memory usage) in others. */
        gl_internalformat = GL_RGBA8;
        gl_format = GL_RGBA;
        gl_type = GL_UNSIGNED_BYTE;
        color_type = TEXCOLOR_RGBA;
        bpp = 32;
        break;

      case TEX_FORMAT_RGB565:
        /* Desktop OpenGL doesn't define a 5-6-5 internalformat, so go with
         * 8-8-8 in that case. */
#ifdef SIL_OPENGL_ES
        gl_internalformat = GL_RGB565;
#else
        gl_internalformat = GL_RGB8;
#endif
        gl_format = GL_RGB;
        /* _REV because we label components from the low bits, but OpenGL
         * labels from the high bits. */
        if (opengl_has_formats(OPENGL_FORMAT_BITREV) && !force_convert) {
            gl_type = GL_UNSIGNED_SHORT_5_6_5_REV;
        } else {
            convert_func = pixel_convert_rgb565_bgr565;
            gl_type = GL_UNSIGNED_SHORT_5_6_5;
        }
        color_type = TEXCOLOR_RGB;
        bpp = 16;
        break;

      case TEX_FORMAT_RGBA5551:
        gl_internalformat = GL_RGB5_A1;
        gl_format = GL_RGBA;
        if (opengl_has_formats(OPENGL_FORMAT_BITREV) && !force_convert) {
            gl_type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
        } else {
            convert_func = pixel_convert_rgba5551_abgr1555;
            gl_type = GL_UNSIGNED_SHORT_5_5_5_1;
        }
        color_type = TEXCOLOR_RGBA;
        bpp = 16;
        break;

      case TEX_FORMAT_RGBA4444:
        gl_internalformat = GL_RGBA4;
        gl_format = GL_RGBA;
        if (opengl_has_formats(OPENGL_FORMAT_BITREV) && !force_convert) {
            gl_type = GL_UNSIGNED_SHORT_4_4_4_4_REV;
        } else {
            convert_func = pixel_convert_rgba4444_abgr4444;
            gl_type = GL_UNSIGNED_SHORT_4_4_4_4;
        }
        color_type = TEXCOLOR_RGBA;
        bpp = 16;
        break;

      case TEX_FORMAT_BGRA8888:
        gl_internalformat = GL_RGBA8;
        if (opengl_has_formats(OPENGL_FORMAT_BGRA) && !force_convert) {
            gl_format = GL_BGRA;
        } else {
            convert_func = pixel_convert_bgra8888_rgba8888;
            gl_format = GL_RGBA;
        }
        gl_type = GL_UNSIGNED_BYTE;
        color_type = TEXCOLOR_RGBA;
        bpp = 32;
        break;

      case TEX_FORMAT_BGR565:
#ifdef SIL_OPENGL_ES
        gl_internalformat = GL_RGB565;
#else
        gl_internalformat = GL_RGB8;
#endif
        gl_format = GL_RGB;
        gl_type = GL_UNSIGNED_SHORT_5_6_5;
        color_type = TEXCOLOR_RGB;
        bpp = 16;
        break;

      case TEX_FORMAT_BGRA5551:
        gl_internalformat = GL_RGB5_A1;
        if (opengl_has_formats(OPENGL_FORMAT_BGRA | OPENGL_FORMAT_BITREV)
         && !force_convert) {
            gl_format = GL_BGRA;
            gl_type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
        } else {
            convert_func = pixel_convert_bgra5551_abgr1555;
            gl_format = GL_RGBA;
            gl_type = GL_UNSIGNED_SHORT_5_5_5_1;
        }
        color_type = TEXCOLOR_RGBA;
        bpp = 16;
        break;

      case TEX_FORMAT_BGRA4444:
        gl_internalformat = GL_RGBA4;
        if (opengl_has_formats(OPENGL_FORMAT_BGRA | OPENGL_FORMAT_BITREV)
         && !force_convert) {
            gl_format = GL_BGRA;
            gl_type = GL_UNSIGNED_SHORT_4_4_4_4_REV;
        } else {
            convert_func = pixel_convert_bgra4444_abgr4444;
            gl_format = GL_RGBA;
            gl_type = GL_UNSIGNED_SHORT_4_4_4_4;
        }
        color_type = TEXCOLOR_RGBA;
        bpp = 16;
        break;

      case TEX_FORMAT_A8:
        if (opengl_has_formats(OPENGL_FORMAT_RG)) {
            gl_internalformat = GL_R8;
            gl_format = GL_RED;
            gl_type = GL_UNSIGNED_BYTE;
            bpp = 8;
        } else {
            /* Use a format with the alpha value in the red component,
             * so shaders don't need to figure out where the value is.
             * We've explicitly stated (in <SIL/shader.h>) that the
             * GBA components of single-channel textures are undefined,
             * so luminance format will do nicely. */
#ifdef SIL_OPENGL_ES
            /* GLES doesn't define GL_LUMINANCE8. */
            gl_internalformat = GL_LUMINANCE;
            gl_format = GL_LUMINANCE;
            gl_type = GL_UNSIGNED_BYTE;
            bpp = 8;
#else
            /* For desktop OpenGL, some drivers don't handle LUMINANCE
             * textures correctly, so we just convert to RGBA8888,
             * reusing the L8 converter for convenience.  Legacy
             * systems (pre-OpenGL 3.0 without ARB_texture_rg) should
             * be sufficiently rare thes days that it's not worth
             * worrying about the extra time and memory cost. */
            convert_func = pixel_convert_l8_rgba8888;
            gl_internalformat = GL_RGBA8;
            gl_format = GL_RGBA;
            gl_type = GL_UNSIGNED_BYTE;
            bpp = 32;
            input_bpp = 8;
#endif
        }
        color_type = TEXCOLOR_A;
        break;

      case TEX_FORMAT_L8:
        if (opengl_has_formats(OPENGL_FORMAT_RG)) {
            gl_internalformat = GL_R8;
            gl_format = GL_RED;
            gl_type = GL_UNSIGNED_BYTE;
            bpp = 8;
        } else {
#ifdef SIL_OPENGL_ES
            gl_internalformat = GL_LUMINANCE;
            gl_format = GL_LUMINANCE;
            gl_type = GL_UNSIGNED_BYTE;
            bpp = 8;
#else
            /* As above. */
            convert_func = pixel_convert_l8_rgba8888;
            gl_internalformat = GL_RGBA8;
            gl_format = GL_RGBA;
            gl_type = GL_UNSIGNED_BYTE;
            bpp = 32;
            input_bpp = 8;
#endif
        }
        color_type = TEXCOLOR_L;
        break;

      case TEX_FORMAT_PSP_RGBA8888:
      case TEX_FORMAT_PSP_RGB565:
      case TEX_FORMAT_PSP_RGBA5551:
      case TEX_FORMAT_PSP_RGBA4444:
      case TEX_FORMAT_PSP_A8:
      case TEX_FORMAT_PSP_L8:
      case TEX_FORMAT_PSP_PALETTE8_RGBA8888:
      case TEX_FORMAT_PSP_RGBA8888_SWIZZLED:
      case TEX_FORMAT_PSP_RGB565_SWIZZLED:
      case TEX_FORMAT_PSP_RGBA5551_SWIZZLED:
      case TEX_FORMAT_PSP_RGBA4444_SWIZZLED:
      case TEX_FORMAT_PSP_A8_SWIZZLED:
      case TEX_FORMAT_PSP_L8_SWIZZLED:
      case TEX_FORMAT_PSP_PALETTE8_RGBA8888_SWIZZLED:
        DLOG("Pixel format %u unsupported", data_format);
        goto error_return;

      case TEX_FORMAT_PALETTE8_RGBA8888:
        gl_internalformat = GL_RGBA8;
        gl_format = GL_RGBA;
        gl_type = GL_UNSIGNED_BYTE;
        color_type = TEXCOLOR_RGBA;
        bpp = 8;
        is_palette = 1;
        break;

      case TEX_FORMAT_S3TC_DXT1:
        gl_internalformat = gl_format = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
        gl_type = GL_INVALID_ENUM;  // Not used.
        color_type = TEXCOLOR_RGB;
        bpp = 4;
        is_compressed = 1;
        break;

      case TEX_FORMAT_S3TC_DXT3:
        gl_internalformat = gl_format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
        gl_type = GL_INVALID_ENUM;  // Not used.
        color_type = TEXCOLOR_RGBA;
        bpp = 8;
        is_compressed = 1;
        break;

      case TEX_FORMAT_S3TC_DXT5:
        gl_internalformat = gl_format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
        gl_type = GL_INVALID_ENUM;  // Not used.
        color_type = TEXCOLOR_RGBA;
        bpp = 8;
        is_compressed = 1;
        break;

      case TEX_FORMAT_PVRTC2_RGBA:
        gl_internalformat = gl_format = GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG;
        gl_type = GL_INVALID_ENUM;  // Not used.
        color_type = TEXCOLOR_RGBA;
        bpp = 2;
        is_compressed = 1;
        break;

      case TEX_FORMAT_PVRTC2_RGB:
        gl_internalformat = gl_format = GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG;
        gl_type = GL_INVALID_ENUM;  // Not used.
        color_type = TEXCOLOR_RGB;
        bpp = 2;
        is_compressed = 1;
        break;

      case TEX_FORMAT_PVRTC4_RGBA:
        gl_internalformat = gl_format = GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG;
        gl_type = GL_INVALID_ENUM;  // Not used.
        color_type = TEXCOLOR_RGBA;
        bpp = 4;
        is_compressed = 1;
        break;

      case TEX_FORMAT_PVRTC4_RGB:
        gl_internalformat = gl_format = GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG;
        gl_type = GL_INVALID_ENUM;  // Not used.
        color_type = TEXCOLOR_RGB;
        bpp = 4;
        is_compressed = 1;
        break;

    }  // switch (data_format)

    if (UNLIKELY(bpp == 0)) {
        DLOG("Pixel format %u unknown", data_format);
        goto error_return;
    }

    if (input_bpp == 0) {
        input_bpp = bpp;
    }

#ifdef SIL_OPENGL_ES
    if (gl_format == GL_BGRA) {
        /* Apple, you are stupid.  Go with the standards already! */
        if (opengl_has_extension("GL_APPLE_texture_format_BGRA8888")) {
            gl_internalformat = GL_BGRA8_EXT;
        } else {
            gl_internalformat = GL_BGRA_EXT;
        }
    }
#endif

    /* Allocate and set up the SysTexture structure. */

    SysTexture *texture = debug_mem_alloc(
        sizeof(*texture), 0, mem_flags & ~MEM_ALLOC_CLEAR,
        __FILE__, __LINE__, MEM_INFO_TEXTURE);
    if (UNLIKELY(!texture)) {
        DLOG("Failed to allocate SysTexture");
        goto error_return;
    }

    opengl_clear_error();
    texture->id = 0;
    glCreateTextures(GL_TEXTURE_2D, 1, &texture->id);
    if (UNLIKELY(!texture->id)) {
        DLOG("Failed to generate an OpenGL texture ID: 0x%04X", glGetError());
        goto error_free_texture;
    }

    texture->generation = opengl_device_generation;
    texture->width      = width;
    texture->height     = height;
    texture->color_type = color_type;
    texture->repeat_u   = 1;
    texture->repeat_v   = 1;
    texture->antialias  = 1;
    texture->empty      = (num_levels == 0);
    texture->readable   = (!is_compressed && gl_format == GL_RGBA);
    texture->lock_buf   = NULL;

    if (mipmaps) {
        /* Core OpenGL (non-ES) allows mipmaps on textures of any size;
         * OpenGL ES only allows them for power-of-two-sized textures.
         * Our custom generate_mipmaps() routine also only handles
         * power-of-two textures, and is limited to 32bpp textures. */
        if (opengl_has_features(OPENGL_FEATURE_GENERATEMIPMAP)) {
#ifdef SIL_OPENGL_ES
            texture->auto_mipmaps = ((width  & (width -1)) == 0
                                  && (height & (height-1)) == 0);
#else
            texture->auto_mipmaps = 1;
#endif
        } else {
            texture->auto_mipmaps = (bpp == 32
                                  && (width  & (width -1)) == 0
                                  && (height & (height-1)) == 0);
        }
    } else {
        texture->auto_mipmaps = 0;
    }
    if (texture->auto_mipmaps && num_levels > 1) {
        num_levels = 1; // Ignore provided mipmap data since we'll generate it.
    }
    texture->has_mipmaps = texture->auto_mipmaps || (num_levels > 1);

    /* Allocate GL-side storage for the texture. */

    uint32_t palette[256];

#ifdef SIL_OPENGL_ES
    /* The minification filter defaults to NEAREST_MIPMAP_LINEAR, which
     * in GLES prevents the creation of non-power-of-two textures, so
     * temporarily change it here.  We'll set it for real later on. */
    glTextureParameteri(texture->id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
#endif

    int total_levels = 1;
    if (texture->has_mipmaps) {
        int w = width, h = height;
        while (w > 1 || h > 1) {
            w = lbound(w/2, 1);
            h = lbound(h/2, 1);
            total_levels++;
        }
    }
    if (opengl_version_is_at_least(3,0)) {
        glTextureStorage2D(texture->id, total_levels, gl_internalformat,
                           width, height);
    } else {
        /* The glTextureStorage2D() non-DSA wrapper (see dsa.c) assumes
         * behavior for glTexImage2D() as specified by GL/GLES 3.0.  This
         * doesn't work in earlier versions, so we need to allocate
         * storage manually.  For compressed formats, we can't even do
         * that, so we fall back to using glCompressedTexImage() below. */
        GLuint current_texture;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, (GLint *)&current_texture);
        glBindTexture(GL_TEXTURE_2D, texture->id);
        if (!is_compressed) {
            gl_internalformat = gl_format;
            /* GLES and plain GL differ in the expected "format" value for
             * BGRA data.  To make things worse, Apple deviates from the
             * GLES standard (EXT_texture_format_BGRA8888) and uses the
             * non-ES style. */
            if (gl_format == GL_BGRA
#ifdef SIL_OPENGL_ES
                && opengl_has_extension("GL_APPLE_texture_format_BGRA8888")
#endif
            ) {
                gl_internalformat = GL_RGBA;
            }
            int w = width, h = height;
            for (int level = 0; level < total_levels; level++) {
                glTexImage2D(GL_TEXTURE_2D, level, gl_internalformat, w, h, 0,
                             gl_format, gl_type, NULL);
                w = lbound(w/2, 1);
                h = lbound(h/2, 1);
            }
        }
        glBindTexture(GL_TEXTURE_2D, current_texture);
    }

    /* Load the texture data (if any). */

    if (num_levels > 0) {

        for (int level = 0; level < num_levels; level++) {
            const int level_w = lbound(width >> level, 1);
            const int level_h = lbound(height >> level, 1);
            const int level_s = lbound(stride >> level, 1);
            uint8_t *level_data = (uint8_t *)data + level_offsets[level];
            int32_t level_size = level_sizes[level];

            if (is_compressed) {
                if (opengl_version_is_at_least(3,0)) {  // See above.
                    glCompressedTextureSubImage2D(
                        texture->id, level, 0, 0, level_w, level_h,
                        gl_format, level_size, level_data);
                } else {
                    GLuint current_texture;
                    glGetIntegerv(GL_TEXTURE_BINDING_2D,
                                  (GLint *)&current_texture);
                    glBindTexture(GL_TEXTURE_2D, texture->id);
                    glCompressedTexImage2D(
                        GL_TEXTURE_2D, level, gl_format, level_w, level_h, 0,
                        level_size, level_data);
                    glBindTexture(GL_TEXTURE_2D, current_texture);
                }

            } else if (is_palette) {
                if (level == 0) {
                    memcpy(palette, level_data, sizeof(palette));
                    level_data += sizeof(palette);
                    level_size -= sizeof(palette);
                }
                uint32_t *temp_data = mem_alloc(level_size*4, 0,
                                                MEM_ALLOC_TEMP);
                if (!temp_data) {
                    DLOG("Failed to get memory for level %d palette fixup"
                         " (%d bytes)", level, level_size*4);
                    goto error_delete_gltex;
                }
                for (int32_t i = 0; i < level_size; i++) {
                    temp_data[i] = palette[level_data[i]];
                }
                glTextureSubImage2D(texture->id, level, 0, 0, level_w, level_h,
                                    gl_format, gl_type, temp_data);
                mem_free(temp_data);

            } else if ((level_s != level_w || bpp != input_bpp || convert_func)
                       && (!reuse || level_w > level_s || bpp > input_bpp)) {
                /* We need a temporary buffer for format conversion, either
                 * because we're not reusing the input buffer or because
                 * the output format requires more memory than the input
                 * layout provides. */
                const int32_t temp_size = (level_w * level_h * bpp) / 8;
                /* Ensure 4-byte alignment for OpenGL (see below). */
                uint8_t *temp_data = mem_alloc(temp_size, 4, MEM_ALLOC_TEMP);
                if (!temp_data) {
                    DLOG("Failed to get memory for level %d data conversion"
                         " (%d bytes)", level, temp_size);
                    goto error_delete_gltex;
                }
                if (level_s != level_w) {
                    if (convert_func) {
                        for (int y = 0; y < level_h; y++) {
                            (*convert_func)(temp_data + y*((level_w*bpp)/8),
                                            level_data + y*((level_s*input_bpp)/8),
                                            level_w);
                        }
                    } else {
                        for (int y = 0; y < level_h; y++) {
                            memcpy(temp_data + y*((level_w*bpp)/8),
                                   level_data + y*((level_s*bpp)/8),
                                   (level_w*bpp)/8);
                        }
                    }
                } else {  // level_s == level_w
                    (*convert_func)(temp_data, level_data, level_w * level_h);
                }
                /* The default data alignment is 4 bytes, so we need to
                 * adjust if the width for this image level is not a
                 * multiple of that (we ensure data buffer alignment above).
                 * Otherwise, we leave the alignment alone so as not to
                 * trigger the use of a slower copy algorithm in the
                 * graphics driver.  We assume each pixel is aligned on
                 * a natural boundary. */
                const int is_unaligned = ((bpp == 8 && level_w % 4 != 0)
                                       || (bpp == 16 && level_w % 2 != 0));
                if (is_unaligned) {
                    glPixelStorei(GL_UNPACK_ALIGNMENT, bpp/8);
                }
                glTextureSubImage2D(texture->id, level, 0, 0, level_w, level_h,
                                    gl_format, gl_type, temp_data);
                if (is_unaligned) {
                    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
                }
                mem_free(temp_data);

            } else {
                /* We're reusing the data and the data size won't grow,
                 * so we can write over the input buffer. */
                if (level_s != level_w) {
                    if (convert_func) {
                        for (int y = 0; y < level_h; y++) {
                            (*convert_func)(level_data + y*((level_w*bpp)/8),
                                            level_data + y*((level_s*input_bpp)/8),
                                            level_w);
                        }
                    } else {
                        for (int y = 1; y < level_h; y++) {
                            memcpy(level_data + y*((level_w*bpp)/8),
                                   level_data + y*((level_s*bpp)/8),
                                   (level_w*bpp)/8);
                        }
                    }
                } else if (convert_func) {
                    (*convert_func)(level_data, level_data, level_w * level_h);
                }
                const int is_unaligned =
                    ((bpp == 8 && ((uintptr_t)level_data % 4 != 0
                                   || level_w % 4 != 0))
                     || (bpp == 16 && ((uintptr_t)level_data % 2 != 0
                                       || level_w % 2 != 0)));
                if (is_unaligned) {
                    glPixelStorei(GL_UNPACK_ALIGNMENT, bpp/8);
                }
                glTextureSubImage2D(texture->id, level, 0, 0, level_w, level_h,
                                    gl_format, gl_type, level_data);
                if (is_unaligned) {
                    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
                }

            }
        }  // for (int level = 0; level < num_levels; level++) {

        if (texture->auto_mipmaps) {
            if (opengl_has_features(OPENGL_FEATURE_GENERATEMIPMAP)) {
                glGenerateTextureMipmap(texture->id);
            } else {
                int32_t offset;
                if (reuse) {
                    offset = level_offsets[0];
                } else {
                    void *new_data = mem_alloc(level_sizes[0], 0,
                                               MEM_ALLOC_TEMP);
                    if (!new_data) {
                        DLOG("No memory for mipmap generation, skipping");
                        texture->has_mipmaps = 0;
                        texture->auto_mipmaps = 0;
                        goto no_mipmaps;
                    } else {
                        memcpy(new_data,
                               (const uint8_t *)data + level_offsets[0],
                               level_sizes[0]);
                        data = new_data;
                    }
                    offset = 0;
                }
                generate_mipmaps(texture->id, data, offset,
                                 texture->width, texture->height, 0);
                data = NULL;  // Freed by generate_mipmaps().
            }
        }

    } else {  // num_levels == 0

        if (mem_flags & MEM_ALLOC_CLEAR) {
            void *buffer = mem_alloc((width * height * bpp + 7) / 8, 0,
                                     MEM_ALLOC_TEMP | MEM_ALLOC_CLEAR);
            if (UNLIKELY(!buffer)) {
                DLOG("No memory for temporary buffer, can't zero-clear"
                     " texture data");
            } else {
                glTextureSubImage2D(texture->id, 0, 0, 0, width, height,
                                    gl_format, gl_type, buffer);
                if (texture->auto_mipmaps) {
                    int level = 0, level_w = width, level_h = height;
                    while (level_w > 1 || level_h > 1) {
                        level++;
                        level_w = lbound(level_w/2, 1);
                        level_h = lbound(level_h/2, 1);
                        glTextureSubImage2D(texture->id, level,
                                            0, 0, level_w, level_h,
                                            gl_format, gl_type, buffer);
                    }
                }
            }
            mem_free(buffer);
        }

    }

    if (texture->has_mipmaps) {
        glTextureParameteri(texture->id, GL_TEXTURE_MIN_FILTER,
                            GL_LINEAR_MIPMAP_LINEAR);
    } else {
      no_mipmaps:
        glTextureParameteri(texture->id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    glTextureParameteri(texture->id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    const int error = glGetError();
    if (UNLIKELY(error != GL_NO_ERROR)) {
        DLOG("Failed to initialize texture data (%dx%d): 0x%04X",
             width, height, error);
        goto error_delete_gltex;
    }

    /* Success! */

    if (reuse) {
        mem_free(data);
    }
    return texture;

    /* Error handling. */

  error_delete_gltex:
    opengl_delete_texture(texture->id);
  error_free_texture:
    mem_free(texture);
  error_return:
    if (reuse) {
        mem_free(data);
    }
    return NULL;
}

/*-----------------------------------------------------------------------*/

void sys_(texture_destroy)(SysTexture *texture)
{
    if (texture->generation == opengl_device_generation) {
        if (opengl_current_texture_id == texture->id) {
            glBindTextureUnit(0, 0);
            opengl_current_texture_id = 0;
        }
        if (opengl_current_texture == texture) {
            opengl_current_texture = NULL;
        }
        opengl_delete_texture(texture->id);
    }

    mem_free(texture->lock_buf);
    mem_free(texture);
}

/*************************************************************************/
/*************** Interface: Texture information retrieval ****************/
/*************************************************************************/

int sys_(texture_width)(SysTexture *texture)
{
    return texture->width;
}

/*-----------------------------------------------------------------------*/

int sys_(texture_height)(SysTexture *texture)
{
    return texture->height;
}

/*-----------------------------------------------------------------------*/

int sys_(texture_has_mipmaps)(SysTexture *texture)
{
    return texture->has_mipmaps;
}

/*************************************************************************/
/****************** Interface: Pixel data manipulation *******************/
/*************************************************************************/

SysTexture *sys_(texture_grab)(int x, int y, int w, int h,
                               UNUSED int readable, int mipmaps, int mem_flags)
{
    SysTexture *texture = mem_alloc(sizeof(*texture), 0, mem_flags);
    if (UNLIKELY(!texture)) {
        DLOG("Failed to allocate SysTexture");
        return NULL;
    }
    mem_debug_set_info(texture, MEM_INFO_TEXTURE);
    texture->generation   = opengl_device_generation;
    texture->id           = 0;
    texture->width        = w;
    texture->height       = h;
    texture->color_type   = TEXCOLOR_RGB;
    texture->auto_mipmaps = (mipmaps != 0);
    texture->has_mipmaps  = 0;
    texture->repeat_u     = 1;
    texture->repeat_v     = 1;
    texture->antialias    = 1;
    texture->empty        = 1;
    texture->readable     = 1;
    texture->lock_buf     = NULL;

    /* As with sys_graphics_read_pixels(), keep ourselves safe from broken
     * GL implementations that don't handle out-of-range source coordinates
     * correctly. */
    int framebuffer_w, framebuffer_h;
    SysFramebuffer *framebuffer = opengl_current_framebuffer();
    if (framebuffer) {
        framebuffer_w = framebuffer->width;
        framebuffer_h = framebuffer->height;
    } else {
        framebuffer_w = opengl_window_width;
        framebuffer_h = opengl_window_height;
    }
    const int is_offscreen = (x < 0
                           || y < 0
                           || w > framebuffer_w - x
                           || h > framebuffer_h - y);
    if (!is_offscreen
     && !opengl_has_features(OPENGL_FEATURE_BROKEN_COPYTEXIMAGE)
     && !(texture->auto_mipmaps
          && !opengl_has_features(OPENGL_FEATURE_GENERATEMIPMAP)))
    {

        /* Fast case: perform the copy entirely within GL.  The texture
         * might already have been created as an immutable texture
         * (glTextureStorage2D()), so we need to delete and recreate it. */
        const GLuint new_texture_id = create_gl_texture(
            texture->width, texture->height, TEXCOLOR_RGB,
            texture->auto_mipmaps, texture->repeat_u, texture->repeat_v,
            texture->antialias);
        if (UNLIKELY(!new_texture_id)) {
            mem_free(texture);
            return NULL;
        }
        texture->id = new_texture_id;
        texture->color_type = TEXCOLOR_RGB;
        glCopyTextureSubImage2D(texture->id, 0, 0, 0, x, y, w, h);
        if (texture->auto_mipmaps) {
            glGenerateTextureMipmap(texture->id);
            texture->has_mipmaps = 1;
        } else {
            texture->has_mipmaps = 0;
        }
        texture->empty = 0;
        texture->readable = 1;

    } else {

        /* Slow case: partial copy or manual mipmaps needed. */
        int dest_x = 0, dest_y = 0;
        if (x < 0) {
            dest_x += (-x);
            w -= (-x);
            x = 0;
        }
        if (y < 0) {
            dest_y += (-y);
            h -= (-y);
            y = 0;
        }
        if (w > framebuffer_w - x) {
            w = framebuffer_w - x;
        }
        if (h > framebuffer_h - y) {
            h = framebuffer_h - y;
        }
        if (w > 0 && h > 0) {
            uint8_t *pixels = mem_alloc(texture->width * texture->height * 4,
                                        0, MEM_ALLOC_TEMP);
            if (UNLIKELY(!pixels)) {
                DLOG("Failed to allocate pixel buffer, can't grab");
                mem_free(texture);
                return NULL;
            }
            uint8_t *dest = pixels + ((dest_y * texture->width + dest_x) * 4);
            glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, dest);
            for (int i = 0; i < w*h; i++) {
                dest[i*4+3] = 0xFF;
            }
            if (w < texture->width) {
                for (int line = h-1; line > 0; line--) {
                    memmove(dest + line*texture->width*4, dest + line*w*4, w*4);
                }
            }
            update_texture(texture, pixels);
        }

    }  // fast vs. slow case

    return texture;
}

/*-----------------------------------------------------------------------*/

void *sys_(texture_lock)(SysTexture *texture, SysTextureLockMode lock_mode,
                         int x, int y, int w, int h)
{
    if (UNLIKELY(texture->generation != opengl_device_generation)) {
        DLOG("Attempt to use invalidated texture %p", texture);
        return NULL;
    }

    const uint32_t size = w * h * 4;
    texture->lock_buf = mem_alloc(size, SIL_OPENGL_TEXTURE_BUFFER_ALIGNMENT,
                                  MEM_ALLOC_TEMP);
    if (UNLIKELY(!texture->lock_buf)) {
        DLOG("lock(%p): Failed to get a lock buffer (%u bytes)",
             texture, size);
        return NULL;
    }

    if (lock_mode == SYS_TEXTURE_LOCK_DISCARD) {
        /* Nothing else to do. */

    } else if (texture->empty) {
        mem_clear(texture->lock_buf, size);

    } else if (opengl_has_features(OPENGL_FEATURE_GETTEXIMAGE)) {
        uint32_t *texture_buf;
        if (w == texture->width && h == texture->height) {
            texture_buf = (uint32_t *)texture->lock_buf;
        } else {
            /* There's no glGetTextureSubImage(), so we'll need to fetch
             * the entire texture data and copy out of it. */
            const uint32_t full_size = texture->width * texture->height * 4;
            texture_buf = mem_alloc(full_size, 0, MEM_ALLOC_TEMP);
            if (UNLIKELY(!texture_buf)) {
                DLOG("lock(%p): Failed to get a texture image buffer (%u"
                     " bytes)", texture, full_size);
                mem_free(texture->lock_buf);
                texture->lock_buf = NULL;
                return NULL;
            }
        }

        opengl_clear_error();
        glGetTextureImage(texture->id, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                          texture->width * texture->height * 4, texture_buf);
        const int error = glGetError();
        if (UNLIKELY(error != GL_NO_ERROR)) {
            DLOG("lock(%p): Failed to retrieve texture data (%dx%d): 0x%04X",
                 texture, texture->width, texture->height, error);
            if (texture_buf != texture->lock_buf) {
                mem_free(texture_buf);
            }
            mem_free(texture->lock_buf);
            texture->lock_buf = NULL;
            return NULL;
        }

        if (texture_buf != texture->lock_buf) {
            const uint32_t *src = texture_buf + y*texture->width + x;
            uint32_t *dest = (uint32_t *)texture->lock_buf;
            if (w == texture->width) {
                memcpy(dest, src, h * texture->width * 4);
            } else {
                for (int yy = 0; yy < h; yy++, src += texture->width, dest += w) {
                    memcpy(dest, src, w*4);
                }
            }
            mem_free(texture_buf);
        }

        if (texture->color_type == TEXCOLOR_A) {
            /* Alpha textures are loaded as R8, so move the data around
             * appropriately. */
            uint8_t *pixels = texture->lock_buf;
            for (int i = 0; i < w*h; i++) {
                pixels[i*4+3] = pixels[i*4+0];
                pixels[i*4+0] = 255;
                pixels[i*4+1] = 255;
                pixels[i*4+2] = 255;
            }
        } else if (texture->color_type == TEXCOLOR_L) {
            uint8_t *pixels = texture->lock_buf;
            for (int i = 0; i < w*h; i++) {
                pixels[i*4+1] = pixels[i*4+0];
                pixels[i*4+2] = pixels[i*4+0];
            }
        }

    } else if (texture->readable) {
        if (!read_texture_via_framebuffer(texture, x, y, w, h,
                                          texture->lock_buf)) {
            mem_free(texture->lock_buf);
            texture->lock_buf = NULL;
            return NULL;
        }

    } else {
        DLOG("Can't read texture data on this platform");
        mem_free(texture->lock_buf);
        texture->lock_buf = NULL;
        return NULL;
    }

    texture->lock_mode = lock_mode;
    return texture->lock_buf;
}

/*-----------------------------------------------------------------------*/

void sys_(texture_unlock)(SysTexture *texture, int update)
{
    if (UNLIKELY(texture->generation != opengl_device_generation)) {
        DLOG("Attempt to use invalidated texture %p", texture);
        return;
    }

    if (update) {
        update_texture(texture, texture->lock_buf);
    } else {
        mem_free(texture->lock_buf);
    }
    texture->lock_buf = NULL;
}

/*-----------------------------------------------------------------------*/

void sys_(texture_flush)(SysTexture *texture)
{
    if (UNLIKELY(texture->generation != opengl_device_generation)) {
        DLOG("Attempt to use invalidated texture %p", texture);
        return;
    }

    /* Nothing to do for OpenGL.  We assume that if the caller has made
     * any direct OpenGL calls on the texture, the caller has also taken
     * responsibility for keeping the SysTexture structure and GL texture
     * state consistent. */
}

/*************************************************************************/
/********************* Interface: Rendering control **********************/
/*************************************************************************/

void sys_(texture_set_repeat)(SysTexture *texture, int repeat_u, int repeat_v)
{
    if (UNLIKELY(texture->generation != opengl_device_generation)) {
        DLOG("Attempt to use invalidated texture %p", texture);
        return;
    }

    int need_update = 0;
    if ((repeat_u != 0) != texture->repeat_u) {
        texture->repeat_u = (repeat_u != 0);
        need_update = 1;
    }
    if ((repeat_v != 0) != texture->repeat_v) {
        texture->repeat_v = (repeat_v != 0);
        need_update = 1;
    }
    if (need_update) {
        glTextureParameteri(texture->id, GL_TEXTURE_WRAP_S,
                            texture->repeat_u ? GL_REPEAT : GL_CLAMP_TO_EDGE);
        glTextureParameteri(texture->id, GL_TEXTURE_WRAP_T,
                            texture->repeat_v ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    }
}

/*-----------------------------------------------------------------------*/

void sys_(texture_set_antialias)(SysTexture *texture, int on)
{
    if (UNLIKELY(texture->generation != opengl_device_generation)) {
        DLOG("Attempt to use invalidated texture %p", texture);
        return;
    }

    if (on && !texture->antialias) {
        if (texture->has_mipmaps) {
            glTextureParameteri(texture->id, GL_TEXTURE_MIN_FILTER,
                                GL_LINEAR_MIPMAP_LINEAR);
        } else {
            glTextureParameteri(texture->id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        }
        glTextureParameteri(texture->id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        texture->antialias = 1;
    } else if (!on && texture->antialias) {
        if (texture->has_mipmaps) {
            glTextureParameteri(texture->id, GL_TEXTURE_MIN_FILTER,
                                GL_NEAREST_MIPMAP_NEAREST);
        } else {
            glTextureParameteri(texture->id, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        }
        glTextureParameteri(texture->id, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        texture->antialias = 0;
    }
}

/*-----------------------------------------------------------------------*/

void sys_(texture_apply)(int unit, SysTexture *texture)
{
    if (texture && UNLIKELY(texture->generation != opengl_device_generation)) {
        DLOG("Attempt to use invalidated texture %p", texture);
        return;
    }

    opengl_clear_error();
    if (unit > 0) {
        glBindTextureUnit(unit, texture ? texture->id : 0);
    } else {
        opengl_bind_texture(GL_TEXTURE_2D, texture ? texture->id : 0);
        opengl_current_texture = texture;
    }
    const GLenum error = glGetError();
    if (UNLIKELY(error != GL_NO_ERROR)) {
        DLOG("Failed to bind texture %u to unit %d: 0x%04X",
             texture ? texture->id : 0, unit, error);
    }
}

/*-----------------------------------------------------------------------*/

int sys_(texture_num_units)(void)
{
    GLint num_units = -1;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &num_units);
    ASSERT(num_units >= 1, num_units = 1);
    return num_units;
}

/*************************************************************************/
/*********************** Exported utility routines ***********************/
/*************************************************************************/

int opengl_texture_id(const SysTexture *texture)
{
    PRECOND(texture != NULL, return 0);
    return texture->id;
}

/*************************************************************************/
/******************* Local routines: Texture creation ********************/
/*************************************************************************/

static GLuint create_gl_texture(
    int width, int height, int color_type, int has_mipmaps,
    int repeat_u, int repeat_v, int antialias)
{
    GLuint texture_id;

    opengl_clear_error();
    glCreateTextures(GL_TEXTURE_2D, 1, &texture_id);
    if (UNLIKELY(!texture_id)) {
        DLOG("Failed to create a new OpenGL texture");
    }
#ifdef SIL_OPENGL_ES
    /* The minification filter defaults to NEAREST_MIPMAP_LINEAR, which
     * in GLES prevents the creation of non-power-of-two textures, so
     * temporarily change it here.  We'll set it for real later on. */
    glTextureParameteri(texture_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
#endif

    int levels = 1;
    if (has_mipmaps) {
        int w = width, h = height;
        while (w > 1 || h > 1) {
            w = lbound(w/2, 1);
            h = lbound(h/2, 1);
            levels++;
        }
    }
    const GLenum format = (
        color_type==TEXCOLOR_A || color_type==TEXCOLOR_L ? GL_R8 :
        color_type==TEXCOLOR_RGB ? GL_RGB8 : GL_RGBA8);
    glTextureStorage2D(texture_id, levels, format, width, height);

    glTextureParameteri(texture_id, GL_TEXTURE_WRAP_S,
                        repeat_u ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    glTextureParameteri(texture_id, GL_TEXTURE_WRAP_T,
                        repeat_v ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    if (antialias) {
        if (has_mipmaps) {
            glTextureParameteri(texture_id, GL_TEXTURE_MIN_FILTER,
                                GL_LINEAR_MIPMAP_LINEAR);
        } else {
            glTextureParameteri(texture_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        }
        glTextureParameteri(texture_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        if (has_mipmaps) {
            glTextureParameteri(texture_id, GL_TEXTURE_MIN_FILTER,
                                GL_NEAREST_MIPMAP_NEAREST);
        } else {
            glTextureParameteri(texture_id, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        }
        glTextureParameteri(texture_id, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    const GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        DLOG("Failed to configure new OpenGL texture: 0x%04X", error);
        opengl_delete_texture(texture_id);
        return 0;
    }

    return texture_id;
}

/*************************************************************************/
/****************** Local routines: Texture data access ******************/
/*************************************************************************/

static int read_texture_via_framebuffer(
    SysTexture *texture, int x, int y, int w, int h, void *buffer)
{
    PRECOND(texture != NULL, return 0);
    PRECOND(buffer != NULL, return 0);

    int result = 1;

    GLuint framebuffer = 0;
    glGenFramebuffers(1, &framebuffer);
    if (UNLIKELY(!framebuffer)) {
        DLOG("Failed to generate a framebuffer ID");
        result = 0;
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, texture->id, 0);
        const int status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status == GL_FRAMEBUFFER_COMPLETE) {
            glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
        } else {
            if (status == GL_FRAMEBUFFER_UNSUPPORTED) {
                DLOG("Framebuffer not supported by system (size %dx%d)", w, h);
            } else {
                DLOG("Unknown error 0x%X while setting up framebuffer (size"
                     " %dx%d)", status, w, h);
            }
            result = 0;
        }
        glDeleteFramebuffers(1, &framebuffer);
    }

    SysFramebuffer *current_framebuffer = opengl_current_framebuffer();
    if (current_framebuffer) {
        glBindFramebuffer(GL_FRAMEBUFFER, current_framebuffer->framebuffer);
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, opengl_get_default_framebuffer());
    }
    opengl_apply_viewport();
    opengl_apply_clip_region();
    return result;
}

/*-----------------------------------------------------------------------*/

static void update_texture(SysTexture *texture, uint8_t *pixels)
{
    PRECOND(pixels != NULL, return);
    PRECOND(texture != NULL, mem_free(pixels); return);

    /* If the texture was not in RGBA format, we have to recreate it. */
    if (texture->color_type != TEXCOLOR_RGBA) {
        const GLuint new_texture = create_gl_texture(
            texture->width, texture->height, TEXCOLOR_RGBA,
            texture->auto_mipmaps, texture->repeat_u, texture->repeat_v,
            texture->antialias);
        if (UNLIKELY(!new_texture)) {
            DLOG("update(%p): Failed to create new RGBA texture, some"
                 " channels will be discarded", texture);
        } else {
            if (texture->id) {
                opengl_delete_texture(texture->id);
            }
            texture->id = new_texture;
            texture->color_type = TEXCOLOR_RGBA;
        }
    }

    glTextureSubImage2D(texture->id, 0, 0, 0, texture->width, texture->height,
                        GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    if (texture->auto_mipmaps) {
        if (opengl_has_features(OPENGL_FEATURE_GENERATEMIPMAP)) {
            glGenerateTextureMipmap(texture->id);
        } else {
            generate_mipmaps(texture->id, pixels, 0,
                             texture->width, texture->height, 0);
            pixels = NULL;  // Prevent double free below.
        }
        texture->has_mipmaps = 1;
    }

    texture->empty = 0;
    texture->readable = 1;
    mem_free(pixels);  // In case we didn't call generate_mipmaps().
}

/*-----------------------------------------------------------------------*/

static void generate_mipmaps(GLuint texture_id, uint8_t *pixels,
                             int32_t offset, int width, int height, int level)
{
    PRECOND(pixels != NULL, return);
    PRECOND(width != 0, mem_free(pixels); return);
    PRECOND(height != 0, mem_free(pixels); return);

    if (width == 1 && height == 1) {
        /* No more mipmaps to generate. */
        mem_free(pixels);
        return;
    }

    uint8_t *pixels_base = pixels;
    pixels += offset;

    /* Shrink the texture by half in place, using simple per-component
     * linear averaging.  This algorithm does not handle the case of an
     * odd width or height (other than 1), and mipmaps for such textures
     * will be slightly offset from the primary image. */

    const int new_width  = lbound(width /2, 1);
    const int new_height = lbound(height/2, 1);
    uint8_t *new_pixels = pixels_base;

    if (width == 1 || height == 1) {
        /* Whether horizontal or vertical, the data is still a single
         * array of pixels, and we can treat both cases the same way. */
        for (int i = 0; i < new_width * new_height; i++) {
            for (int c = 0; c < 4; c++) {
                new_pixels[i*4+c] = (pixels[(i*2+0)*4+c]
                                   + pixels[(i*2+1)*4+c] + 1) / 2;
            }
        }
    } else {  // width != 1 && height != 1
        for (int y = 0; y < new_height; y++) {
            const uint8_t *in0 = &pixels[(y*2+0) * (width*4)];
            const uint8_t *in1 = &pixels[(y*2+1) * (width*4)];
            uint8_t *out = &new_pixels[y * (new_width*4)];
            for (int x = 0; x < new_width; x++) {
                for (int c = 0; c < 4; c++) {
                    out[x*4+c] = (in0[(x*2+0)*4+c] + in0[(x*2+1)*4+c]
                                + in1[(x*2+0)*4+c] + in1[(x*2+1)*4+c] + 2) / 4;
                }
            }
        }
    }

    /* Shrink the pixel buffer to reduce memory pressure. */

    const uint32_t new_size = new_width * new_height * 4;
    ASSERT(pixels = mem_realloc(new_pixels, new_size, MEM_ALLOC_TEMP),
           pixels = new_pixels);

    /* Register the mipmap with OpenGL. */

    glTextureSubImage2D(texture_id, level+1, 0, 0, new_width, new_height,
                        GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    /* Recursively generate mipmaps for smaller sizes.  (The initial size
     * check will stop the recursion when we hit 1x1.) */

    generate_mipmaps(texture_id, pixels, 0, new_width, new_height, level+1);
}

/*************************************************************************/
/*************************************************************************/
