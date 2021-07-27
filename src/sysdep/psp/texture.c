/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/texture.c: Texture manipulation functionality for the PSP.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/endian.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/psp/ge-util.h"
#include "src/sysdep/psp/internal.h"
#include "src/sysdep/psp/texture.h"
#include "src/texture.h"
#include "src/utility/tex-file.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Currently-applied texture. */
static SysTexture *current_texture;

/* Texture whose parameters are currently loaded into GE texture registers.
 * We track this to avoid unnecessary updating of texture registers and
 * clearing of the GE's texture cache. */
static SysTexture *loaded_texture;
/* Current state of texture registers. */
static const void *loaded_texture_palette;
static int16_t loaded_texture_scale_x;
static int16_t loaded_texture_scale_y;
static uint8_t loaded_texture_antialias;
static uint8_t loaded_texture_mipmaps;  // Boolean flag, not count.
static uint8_t loaded_texture_repeat_u;
static uint8_t loaded_texture_repeat_v;

/* List of textures whose deletion has been deferred (see
 * sys_texture_destroy() for details). */
static SysTexture *deferred_destroy_list;

/*-----------------------------------------------------------------------*/

/* Static palettes for alpha-only and luminance images. */

static const ALIGNED(64) uint32_t alpha_palette[256] = {
    0x00FFFFFF, 0x01FFFFFF, 0x02FFFFFF, 0x03FFFFFF,
    0x04FFFFFF, 0x05FFFFFF, 0x06FFFFFF, 0x07FFFFFF,
    0x08FFFFFF, 0x09FFFFFF, 0x0AFFFFFF, 0x0BFFFFFF,
    0x0CFFFFFF, 0x0DFFFFFF, 0x0EFFFFFF, 0x0FFFFFFF,
    0x10FFFFFF, 0x11FFFFFF, 0x12FFFFFF, 0x13FFFFFF,
    0x14FFFFFF, 0x15FFFFFF, 0x16FFFFFF, 0x17FFFFFF,
    0x18FFFFFF, 0x19FFFFFF, 0x1AFFFFFF, 0x1BFFFFFF,
    0x1CFFFFFF, 0x1DFFFFFF, 0x1EFFFFFF, 0x1FFFFFFF,
    0x20FFFFFF, 0x21FFFFFF, 0x22FFFFFF, 0x23FFFFFF,
    0x24FFFFFF, 0x25FFFFFF, 0x26FFFFFF, 0x27FFFFFF,
    0x28FFFFFF, 0x29FFFFFF, 0x2AFFFFFF, 0x2BFFFFFF,
    0x2CFFFFFF, 0x2DFFFFFF, 0x2EFFFFFF, 0x2FFFFFFF,
    0x30FFFFFF, 0x31FFFFFF, 0x32FFFFFF, 0x33FFFFFF,
    0x34FFFFFF, 0x35FFFFFF, 0x36FFFFFF, 0x37FFFFFF,
    0x38FFFFFF, 0x39FFFFFF, 0x3AFFFFFF, 0x3BFFFFFF,
    0x3CFFFFFF, 0x3DFFFFFF, 0x3EFFFFFF, 0x3FFFFFFF,
    0x40FFFFFF, 0x41FFFFFF, 0x42FFFFFF, 0x43FFFFFF,
    0x44FFFFFF, 0x45FFFFFF, 0x46FFFFFF, 0x47FFFFFF,
    0x48FFFFFF, 0x49FFFFFF, 0x4AFFFFFF, 0x4BFFFFFF,
    0x4CFFFFFF, 0x4DFFFFFF, 0x4EFFFFFF, 0x4FFFFFFF,
    0x50FFFFFF, 0x51FFFFFF, 0x52FFFFFF, 0x53FFFFFF,
    0x54FFFFFF, 0x55FFFFFF, 0x56FFFFFF, 0x57FFFFFF,
    0x58FFFFFF, 0x59FFFFFF, 0x5AFFFFFF, 0x5BFFFFFF,
    0x5CFFFFFF, 0x5DFFFFFF, 0x5EFFFFFF, 0x5FFFFFFF,
    0x60FFFFFF, 0x61FFFFFF, 0x62FFFFFF, 0x63FFFFFF,
    0x64FFFFFF, 0x65FFFFFF, 0x66FFFFFF, 0x67FFFFFF,
    0x68FFFFFF, 0x69FFFFFF, 0x6AFFFFFF, 0x6BFFFFFF,
    0x6CFFFFFF, 0x6DFFFFFF, 0x6EFFFFFF, 0x6FFFFFFF,
    0x70FFFFFF, 0x71FFFFFF, 0x72FFFFFF, 0x73FFFFFF,
    0x74FFFFFF, 0x75FFFFFF, 0x76FFFFFF, 0x77FFFFFF,
    0x78FFFFFF, 0x79FFFFFF, 0x7AFFFFFF, 0x7BFFFFFF,
    0x7CFFFFFF, 0x7DFFFFFF, 0x7EFFFFFF, 0x7FFFFFFF,
    0x80FFFFFF, 0x81FFFFFF, 0x82FFFFFF, 0x83FFFFFF,
    0x84FFFFFF, 0x85FFFFFF, 0x86FFFFFF, 0x87FFFFFF,
    0x88FFFFFF, 0x89FFFFFF, 0x8AFFFFFF, 0x8BFFFFFF,
    0x8CFFFFFF, 0x8DFFFFFF, 0x8EFFFFFF, 0x8FFFFFFF,
    0x90FFFFFF, 0x91FFFFFF, 0x92FFFFFF, 0x93FFFFFF,
    0x94FFFFFF, 0x95FFFFFF, 0x96FFFFFF, 0x97FFFFFF,
    0x98FFFFFF, 0x99FFFFFF, 0x9AFFFFFF, 0x9BFFFFFF,
    0x9CFFFFFF, 0x9DFFFFFF, 0x9EFFFFFF, 0x9FFFFFFF,
    0xA0FFFFFF, 0xA1FFFFFF, 0xA2FFFFFF, 0xA3FFFFFF,
    0xA4FFFFFF, 0xA5FFFFFF, 0xA6FFFFFF, 0xA7FFFFFF,
    0xA8FFFFFF, 0xA9FFFFFF, 0xAAFFFFFF, 0xABFFFFFF,
    0xACFFFFFF, 0xADFFFFFF, 0xAEFFFFFF, 0xAFFFFFFF,
    0xB0FFFFFF, 0xB1FFFFFF, 0xB2FFFFFF, 0xB3FFFFFF,
    0xB4FFFFFF, 0xB5FFFFFF, 0xB6FFFFFF, 0xB7FFFFFF,
    0xB8FFFFFF, 0xB9FFFFFF, 0xBAFFFFFF, 0xBBFFFFFF,
    0xBCFFFFFF, 0xBDFFFFFF, 0xBEFFFFFF, 0xBFFFFFFF,
    0xC0FFFFFF, 0xC1FFFFFF, 0xC2FFFFFF, 0xC3FFFFFF,
    0xC4FFFFFF, 0xC5FFFFFF, 0xC6FFFFFF, 0xC7FFFFFF,
    0xC8FFFFFF, 0xC9FFFFFF, 0xCAFFFFFF, 0xCBFFFFFF,
    0xCCFFFFFF, 0xCDFFFFFF, 0xCEFFFFFF, 0xCFFFFFFF,
    0xD0FFFFFF, 0xD1FFFFFF, 0xD2FFFFFF, 0xD3FFFFFF,
    0xD4FFFFFF, 0xD5FFFFFF, 0xD6FFFFFF, 0xD7FFFFFF,
    0xD8FFFFFF, 0xD9FFFFFF, 0xDAFFFFFF, 0xDBFFFFFF,
    0xDCFFFFFF, 0xDDFFFFFF, 0xDEFFFFFF, 0xDFFFFFFF,
    0xE0FFFFFF, 0xE1FFFFFF, 0xE2FFFFFF, 0xE3FFFFFF,
    0xE4FFFFFF, 0xE5FFFFFF, 0xE6FFFFFF, 0xE7FFFFFF,
    0xE8FFFFFF, 0xE9FFFFFF, 0xEAFFFFFF, 0xEBFFFFFF,
    0xECFFFFFF, 0xEDFFFFFF, 0xEEFFFFFF, 0xEFFFFFFF,
    0xF0FFFFFF, 0xF1FFFFFF, 0xF2FFFFFF, 0xF3FFFFFF,
    0xF4FFFFFF, 0xF5FFFFFF, 0xF6FFFFFF, 0xF7FFFFFF,
    0xF8FFFFFF, 0xF9FFFFFF, 0xFAFFFFFF, 0xFBFFFFFF,
    0xFCFFFFFF, 0xFDFFFFFF, 0xFEFFFFFF, 0xFFFFFFFF,
};

static const ALIGNED(64) uint32_t luminance_palette[256] = {
    0xFF000000, 0xFF010101, 0xFF020202, 0xFF030303,
    0xFF040404, 0xFF050505, 0xFF060606, 0xFF070707,
    0xFF080808, 0xFF090909, 0xFF0A0A0A, 0xFF0B0B0B,
    0xFF0C0C0C, 0xFF0D0D0D, 0xFF0E0E0E, 0xFF0F0F0F,
    0xFF101010, 0xFF111111, 0xFF121212, 0xFF131313,
    0xFF141414, 0xFF151515, 0xFF161616, 0xFF171717,
    0xFF181818, 0xFF191919, 0xFF1A1A1A, 0xFF1B1B1B,
    0xFF1C1C1C, 0xFF1D1D1D, 0xFF1E1E1E, 0xFF1F1F1F,
    0xFF202020, 0xFF212121, 0xFF222222, 0xFF232323,
    0xFF242424, 0xFF252525, 0xFF262626, 0xFF272727,
    0xFF282828, 0xFF292929, 0xFF2A2A2A, 0xFF2B2B2B,
    0xFF2C2C2C, 0xFF2D2D2D, 0xFF2E2E2E, 0xFF2F2F2F,
    0xFF303030, 0xFF313131, 0xFF323232, 0xFF333333,
    0xFF343434, 0xFF353535, 0xFF363636, 0xFF373737,
    0xFF383838, 0xFF393939, 0xFF3A3A3A, 0xFF3B3B3B,
    0xFF3C3C3C, 0xFF3D3D3D, 0xFF3E3E3E, 0xFF3F3F3F,
    0xFF404040, 0xFF414141, 0xFF424242, 0xFF434343,
    0xFF444444, 0xFF454545, 0xFF464646, 0xFF474747,
    0xFF484848, 0xFF494949, 0xFF4A4A4A, 0xFF4B4B4B,
    0xFF4C4C4C, 0xFF4D4D4D, 0xFF4E4E4E, 0xFF4F4F4F,
    0xFF505050, 0xFF515151, 0xFF525252, 0xFF535353,
    0xFF545454, 0xFF555555, 0xFF565656, 0xFF575757,
    0xFF585858, 0xFF595959, 0xFF5A5A5A, 0xFF5B5B5B,
    0xFF5C5C5C, 0xFF5D5D5D, 0xFF5E5E5E, 0xFF5F5F5F,
    0xFF606060, 0xFF616161, 0xFF626262, 0xFF636363,
    0xFF646464, 0xFF656565, 0xFF666666, 0xFF676767,
    0xFF686868, 0xFF696969, 0xFF6A6A6A, 0xFF6B6B6B,
    0xFF6C6C6C, 0xFF6D6D6D, 0xFF6E6E6E, 0xFF6F6F6F,
    0xFF707070, 0xFF717171, 0xFF727272, 0xFF737373,
    0xFF747474, 0xFF757575, 0xFF767676, 0xFF777777,
    0xFF787878, 0xFF797979, 0xFF7A7A7A, 0xFF7B7B7B,
    0xFF7C7C7C, 0xFF7D7D7D, 0xFF7E7E7E, 0xFF7F7F7F,
    0xFF808080, 0xFF818181, 0xFF828282, 0xFF838383,
    0xFF848484, 0xFF858585, 0xFF868686, 0xFF878787,
    0xFF888888, 0xFF898989, 0xFF8A8A8A, 0xFF8B8B8B,
    0xFF8C8C8C, 0xFF8D8D8D, 0xFF8E8E8E, 0xFF8F8F8F,
    0xFF909090, 0xFF919191, 0xFF929292, 0xFF939393,
    0xFF949494, 0xFF959595, 0xFF969696, 0xFF979797,
    0xFF989898, 0xFF999999, 0xFF9A9A9A, 0xFF9B9B9B,
    0xFF9C9C9C, 0xFF9D9D9D, 0xFF9E9E9E, 0xFF9F9F9F,
    0xFFA0A0A0, 0xFFA1A1A1, 0xFFA2A2A2, 0xFFA3A3A3,
    0xFFA4A4A4, 0xFFA5A5A5, 0xFFA6A6A6, 0xFFA7A7A7,
    0xFFA8A8A8, 0xFFA9A9A9, 0xFFAAAAAA, 0xFFABABAB,
    0xFFACACAC, 0xFFADADAD, 0xFFAEAEAE, 0xFFAFAFAF,
    0xFFB0B0B0, 0xFFB1B1B1, 0xFFB2B2B2, 0xFFB3B3B3,
    0xFFB4B4B4, 0xFFB5B5B5, 0xFFB6B6B6, 0xFFB7B7B7,
    0xFFB8B8B8, 0xFFB9B9B9, 0xFFBABABA, 0xFFBBBBBB,
    0xFFBCBCBC, 0xFFBDBDBD, 0xFFBEBEBE, 0xFFBFBFBF,
    0xFFC0C0C0, 0xFFC1C1C1, 0xFFC2C2C2, 0xFFC3C3C3,
    0xFFC4C4C4, 0xFFC5C5C5, 0xFFC6C6C6, 0xFFC7C7C7,
    0xFFC8C8C8, 0xFFC9C9C9, 0xFFCACACA, 0xFFCBCBCB,
    0xFFCCCCCC, 0xFFCDCDCD, 0xFFCECECE, 0xFFCFCFCF,
    0xFFD0D0D0, 0xFFD1D1D1, 0xFFD2D2D2, 0xFFD3D3D3,
    0xFFD4D4D4, 0xFFD5D5D5, 0xFFD6D6D6, 0xFFD7D7D7,
    0xFFD8D8D8, 0xFFD9D9D9, 0xFFDADADA, 0xFFDBDBDB,
    0xFFDCDCDC, 0xFFDDDDDD, 0xFFDEDEDE, 0xFFDFDFDF,
    0xFFE0E0E0, 0xFFE1E1E1, 0xFFE2E2E2, 0xFFE3E3E3,
    0xFFE4E4E4, 0xFFE5E5E5, 0xFFE6E6E6, 0xFFE7E7E7,
    0xFFE8E8E8, 0xFFE9E9E9, 0xFFEAEAEA, 0xFFEBEBEB,
    0xFFECECEC, 0xFFEDEDED, 0xFFEEEEEE, 0xFFEFEFEF,
    0xFFF0F0F0, 0xFFF1F1F1, 0xFFF2F2F2, 0xFFF3F3F3,
    0xFFF4F4F4, 0xFFF5F5F5, 0xFFF6F6F6, 0xFFF7F7F7,
    0xFFF8F8F8, 0xFFF9F9F9, 0xFFFAFAFA, 0xFFFBFBFB,
    0xFFFCFCFC, 0xFFFDFDFD, 0xFFFEFEFE, 0xFFFFFFFF,
};

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * bpp_for_ge_format:  Return the number of bits per pixel for the given
 * GE pixel format.
 *
 * [Parameters]
 *     format: GE pixel format (GE_PIXFMT_* or GE_TEXFMT_*).
 * [Return value]
 *     Number of bits per pixel.
 */
static int bpp_for_ge_format(int format);

/**
 * texture_to_RGBA:  Copy texture pixel data into an RGBA buffer.
 *
 * [Parameters]
 *     texture: Texture to copy from.
 *     dest: Destination buffer.
 */
static void texture_to_RGBA(SysTexture *texture, uint32_t *dest);

/**
 * palette_to_RGBA_aligned:  Copy 8bpp palettized image data into an RGBA
 * buffer.  Both the source pointer and the width must be a multiple of 4,
 * and the width must not be zero.
 *
 * [Parameters]
 *     src: Source buffer.
 *     stride: Source buffer line length, in pixels.
 *     palette: Palette data.
 *     dest: Destination buffer.
 *     width, height: Size of region to copy, in pixels.  The destination
 *         buffer is assumed to be exactly this size.
 */
static inline void palette_to_RGBA_aligned(
    const uint8_t *src, int stride, const uint32_t *palette, uint32_t *dest,
    int width, int height);

/**
 * pixel_16bpp_to_rgba:  Convert a 16-bit pixel value to a 32-bit RGBA value
 * (a 32-bit value which will be in RGBA byte order when stored in memory).
 *
 * [Parameters]
 *     format: Input pixel format (one of GE_TEXFMT_5650, GE_TEXFMT_5551,
 *         or GE_TEXFMT_4444).
 *     pixel: Input pixel value.
 * [Return value]
 *     Equivalent 32-bit pixel value.
 */
static inline uint32_t pixel_16bpp_to_RGBA(
    const int format, const uint16_t pixel);

/**
 * RGBA_to_texture:  Copy RGBA pixel data into a texture.
 *
 * [Parameters]
 *     src: Source buffer.
 *     src_stride: Source buffer line length, in pixels.
 *     width, height: Size of region to copy.
 *     has_alpha: True if the source alpha byte is valid, false if not.
 *     texture: Destination texture.
 *     u0, v0: Base coordinate in texture of destination rectangle.
 */
static void RGBA_to_texture(const uint32_t *src, int src_stride,
                            int width, int height, int has_alpha,
                            SysTexture *texture, int u0, int v0);

/**
 * fb_to_texture:  Copy framebuffer data into a texture.
 *
 * [Parameters]
 *     x0, y0: Coordinates of first pixel to copy.
 *     width, height: Size of region to copy.
 *     texture: Destination texture.
 *     u0, v0: Texture coordinates at which to store pixel (x0,y0).
 */
static void fb_to_texture(int x0, int y0, int width, int height,
                          SysTexture *texture, int u0, int v0);

/**
 * set_texture_state_repeat:  Set GE registers to reflect the coordinate
 * wrapping state of the given texture.
 */
static void set_texture_state_repeat(const SysTexture *texture);

/**
 * set_texture_state_antialias:  Set GE registers to reflect the antialias
 * state of the given texture.
 */
static void set_texture_state_antialias(const SysTexture *texture);

/*************************************************************************/
/*************** Interface: Texture creation and deletion ****************/
/*************************************************************************/

SysTexture *sys_texture_create(
    int width, int height, TextureFormat data_format, int num_levels,
    void *data, int stride, const int32_t *level_offsets,
    const int32_t *level_sizes, UNUSED int mipmaps, int mem_flags, int reuse)
{
    /* First off, drop any excess image levels we can't use. */

    if (num_levels > 8) {
        DLOG("Warning: dropping unusable mipmap levels (%d > 8)", num_levels);
        num_levels = 8;
    }

    /* Check the data format and make sure it's something we can handle. */

    int format;
    int is_alpha = 0, is_luminance = 0, is_aligned = 0, is_swizzled = 0;
    switch (data_format) {
      case TEX_FORMAT_PSP_RGBA8888_SWIZZLED:
        is_swizzled = 1;
        /* Fall through. */
      case TEX_FORMAT_PSP_RGBA8888:
        is_aligned = 1;
        /* Fall through. */
      case TEX_FORMAT_RGBA8888:
        format = GE_TEXFMT_8888;
        break;

      case TEX_FORMAT_PSP_RGB565_SWIZZLED:
        is_swizzled = 1;
        /* Fall through. */
      case TEX_FORMAT_PSP_RGB565:
        is_aligned = 1;
        /* Fall through. */
      case TEX_FORMAT_RGB565:
        format = GE_TEXFMT_5650;
        break;

      case TEX_FORMAT_PSP_RGBA5551_SWIZZLED:
        is_swizzled = 1;
        /* Fall through. */
      case TEX_FORMAT_PSP_RGBA5551:
        is_aligned = 1;
        /* Fall through. */
      case TEX_FORMAT_RGBA5551:
        format = GE_TEXFMT_5551;
        break;

      case TEX_FORMAT_PSP_RGBA4444_SWIZZLED:
        is_swizzled = 1;
        /* Fall through. */
      case TEX_FORMAT_PSP_RGBA4444:
        is_aligned = 1;
        /* Fall through. */
      case TEX_FORMAT_RGBA4444:
        format = GE_TEXFMT_4444;
        break;

      case TEX_FORMAT_PSP_A8_SWIZZLED:
        is_swizzled = 1;
        /* Fall through. */
      case TEX_FORMAT_PSP_A8:
        is_aligned = 1;
        /* Fall through. */
      case TEX_FORMAT_A8:
        format = GE_TEXFMT_T8;
        is_alpha = 1;
        break;

      case TEX_FORMAT_PSP_L8_SWIZZLED:
        is_swizzled = 1;
        /* Fall through. */
      case TEX_FORMAT_PSP_L8:
        is_aligned = 1;
        /* Fall through. */
      case TEX_FORMAT_L8:
        format = GE_TEXFMT_T8;
        is_luminance = 1;
        break;

      case TEX_FORMAT_PSP_PALETTE8_RGBA8888_SWIZZLED:
        is_swizzled = 1;
        /* Fall through. */
      case TEX_FORMAT_PSP_PALETTE8_RGBA8888:
        is_aligned = 1;
        /* Fall through. */
      case TEX_FORMAT_PALETTE8_RGBA8888:
        format = GE_TEXFMT_T8;
        break;

      default:
        DLOG("Pixel format %u unsupported", data_format);
        goto error_return;
    }

    const int bpp = bpp_for_ge_format(format);

    /* If data reuse was requested, check whether we can in fact reuse the
     * data buffer. */

    int do_reuse = 0;
    if (num_levels > 0 && reuse) {
        if (!is_aligned) {
            DLOG("Not reusing data: not in PSP format");
        } else if ((uintptr_t)data % 64 != 0) {
            DLOG("Not reusing data: buffer not correctly aligned");
        } else if (level_offsets[0] < (int)sizeof(SysTexture)) {
            DLOG("Not reusing data: not enough space at beginning of buffer"
                 " (%zu, need %zu)", level_offsets[0], sizeof(SysTexture));
        } else {
            do_reuse = 1;
        }
    }

    /* If the data was not in a PSP-specific format (which here implies
     * !is_aligned) and is of a properly aligned width and height, swizzle
     * it on the assumption that textures with initial data will not be
     * written to later.  (If the texture was in a non-swizzled
     * PSP-specific format, we assume it was deliberately stored unswizzled
     * and don't attempt to swizzle it here.) */
    const int swizzle = !is_aligned && width%(128/bpp) == 0 && height%8 == 0;

    /* Set up a SysTexture structure for the texture.  If we're not reusing
     * the input data buffer, allocate the SysTexture structure and texture
     * data as a single memory block for simplicity. */

    SysTexture *texture;
    uint8_t *texture_limit;  // For sceKernelDcacheWritebackRange().
    const uint32_t *palette =
        is_alpha ? alpha_palette : is_luminance ? luminance_palette : NULL;

    if (do_reuse) {

        mem_debug_set_info(data, MEM_INFO_TEXTURE);
        texture = (SysTexture *)data;
        texture_limit = NULL;  // Not used.
        texture->stride = stride;
        for (int level = 0; level < num_levels; level++) {
            texture->pixels[level] = (uint8_t *)data + level_offsets[level];
            sceKernelDcacheWritebackRange(texture->pixels[level],
                                          level_sizes[level]);
        }
        if (bpp == 8 && !(is_alpha || is_luminance)) {
            palette = (uint32_t *)(void *)(texture->pixels[0]);
            sceKernelDcacheWritebackRange(palette, 256*4);
            texture->pixels[0] += 256*4;
        }
        reuse = 0;  // Don't free the buffer on return.

    } else {  // !do_reuse

        uint32_t my_level_sizes[8];
        const int32_t struct_size = align_up(sizeof(SysTexture), 64);
        int32_t total_size = struct_size;
        if (bpp == 8 && !(is_alpha || is_luminance)) {
            total_size += 256*4;
        }
        int level_h = height;
        if (swizzle) {
            level_h = align_up(level_h, 8);
        }
        int level_s = align_up(width, 128/bpp);
        for (int level = 0; level < lbound(num_levels,1); level++) {
            const int level_size = (level_h * level_s * bpp) / 8;
            my_level_sizes[level] = align_up(level_size, 64);
            total_size += my_level_sizes[level];
            level_h = lbound(level_h/2, swizzle ? 8 : 1);
            level_s = align_up(level_s/2, 128/bpp);
        }

        texture = debug_mem_alloc(total_size, 64, mem_flags,
                                  __FILE__, __LINE__, MEM_INFO_TEXTURE);
        if (UNLIKELY(!texture)) {
            DLOG("Failed to allocate %dx%d texture (%d levels, %zu bytes)",
                 width, height, lbound(num_levels,1), total_size);
            goto error_return;
        }
        texture_limit = (uint8_t *)texture + total_size;
        texture->stride = align_up(width, 128/bpp);
        uint8_t *pixels = (uint8_t *)texture + struct_size;
        if (bpp == 8 && !(is_alpha || is_luminance)) {
            /* Do the copy here so we don't have to de-const below. */
            if (num_levels > 0) {
                memcpy(pixels, (uint8_t *)data + level_offsets[0], 256*4);
            } else {
                DLOG("Warning: indexed texture without data, assuming"
                     " luminance palette");
                memcpy(pixels, luminance_palette, 256*4);
            }
            sceKernelDcacheWritebackRange(pixels, 256*4);
            /* Cast through void * to suppress cast-align warning. */
            palette = (const uint32_t *)(void *)pixels;
            pixels += 256*4;
        }
        for (int level = 0; level < lbound(num_levels,1); level++) {
            texture->pixels[level] = pixels;
            pixels += my_level_sizes[level];
        }

    }  // do_reuse

    texture->width        = width;
    texture->height       = height;
    texture->format       = format;
    texture->swizzled     = is_swizzled | swizzle;
    texture->vram         = 0;
    texture->mipmaps      = lbound(num_levels,1) - 1;
    texture->palette      = palette;
    texture->orig_palette = palette;
    texture->antialias    = 1;
    texture->repeat_u     = 1;
    texture->repeat_v     = 1;
    texture->lock_buf     = NULL;

    /* If texture data was given but we're not reusing the input buffer,
     * copy the texture data into the newly allocated buffer.  (Palettes
     * for 8-bit indexed data were copied above.) */

    if (!do_reuse && num_levels > 0) {
        int level_w = width;
        int level_h = height;
        if (swizzle) {
            level_h = align_up(level_h, 8);
        }
        int in_stride = stride;
        for (int level = 0; level < num_levels; level++) {
            if (is_aligned) {
                in_stride = align_up(in_stride, 128/bpp);
            }
            const int out_stride = align_up(level_w, 128/bpp);
            const uint8_t *pixels =
                (const uint8_t *)data + level_offsets[level];
            if (bpp == 8 && !(is_alpha || is_luminance)) {
                pixels += 256*4;  // Skip the palette data.
            }
            if (swizzle) {
                uint8_t *dest = texture->pixels[level];
                for (int y = 0; y < level_h; y += 8) {
                    const uint8_t *src = pixels + y * (in_stride*bpp)/8;
                    for (int x = 0; x < out_stride; x += 128/bpp, src += 16) {
                        const unsigned int copy_bytes =
                            ubound(((level_w - x) * bpp + 7) / 8, 16);
                        const uint8_t *line_src = src;
                        const int num_lines = ubound(level_h - y, 8);
                        for (int line = 0; line < num_lines;
                             line++, line_src += (in_stride*bpp)/8, dest += 16)
                        {
                            memcpy(dest, line_src, copy_bytes);
                            if (copy_bytes < 16) {
                                mem_clear(dest + copy_bytes, 16 - copy_bytes);
                            }
                        }
                    }
                }
            } else if (out_stride == in_stride) {
                memcpy(texture->pixels[level], pixels,
                       level_h * in_stride * (bpp/8));
            } else {
                const uint8_t *src = pixels;
                uint8_t *dest = texture->pixels[level];
                for (int y = 0; y < level_h;
                     y++, src += (in_stride*bpp)/8, dest += (out_stride*bpp)/8)
                {
                    memcpy(dest, src, (level_w * bpp + 7) / 8);
                }
            }
            level_w = lbound(level_w/2, 1);
            level_h = lbound(level_h/2, swizzle ? 8 : 1);
            in_stride = lbound(in_stride/2, 1);
        }
        sceKernelDcacheWritebackRange(texture->pixels[0],
                                      texture_limit - texture->pixels[0]);
    }

    /* All done! */

    if (reuse) {
        mem_free(data);
    }
    return texture;

  error_return:
    if (reuse) {
        mem_free(data);
    }
    return NULL;
}

/*-----------------------------------------------------------------------*/

void sys_texture_destroy(SysTexture *texture)
{
    PRECOND(texture != NULL, return);

    if (current_texture == texture) {
        current_texture = NULL;
    }
    if (loaded_texture == texture) {
        psp_set_texture_state(1);
    }

    /* We allocate textures as a single memory block, so we just need to
     * free that one block (plus the lock buffer if necessary).  However,
     * if we're in the middle of a frame, the GE might still need to draw
     * from this texture, so in that case we defer the final free operation
     * until the end of the frame.  Deferred destroy operations are managed
     * through a simple linked list, using the first word of the SysTexture
     * structure memory as the link pointer (since we don't need the
     * contents of the structure anymore). */
    mem_free(texture->lock_buf);
    if (psp_is_ge_busy()) {
        *(SysTexture **)texture = deferred_destroy_list;
        deferred_destroy_list = texture;
    } else {
        if (texture->vram) {
            psp_vram_free(texture->pixels[0]);
        }
        mem_free(texture);
    }
}

/*************************************************************************/
/*************** Interface: Texture information retrieval ****************/
/*************************************************************************/

int sys_texture_width(SysTexture *texture)
{
    return texture->width;
}

/*-----------------------------------------------------------------------*/

int sys_texture_height(SysTexture *texture)
{
    return texture->height;
}

/*-----------------------------------------------------------------------*/

int sys_texture_has_mipmaps(SysTexture *texture)
{
    return texture->mipmaps > 0;
}

/*************************************************************************/
/****************** Interface: Pixel data manipulation *******************/
/*************************************************************************/

SysTexture *sys_texture_grab(int x, int y, int w, int h, int readable,
                             UNUSED int mipmaps, int mem_flags)
{
    const int format = (!readable && w%4 == 0 && h%8 == 0
                        ? TEX_FORMAT_PSP_RGBA8888_SWIZZLED
                        : TEX_FORMAT_PSP_RGBA8888);
    SysTexture *texture = sys_texture_create(
        w, h, format, 0, NULL, 0, NULL, NULL, 0, 0, mem_flags);
    if (UNLIKELY(!texture)) {
        DLOG("Failed to create texture for grab");
        return NULL;
    }

    int u = 0, v = 0;
    if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) {
        goto out;
    }
    if (x < 0) {
        w -= -x;
        u += -x;
        x = 0;
    }
    if (y < 0) {
        h -= -y;
        v += -y;
        y = 0;
    }
    if (w > DISPLAY_WIDTH - x) {
        w = DISPLAY_WIDTH - x;
    }
    if (h > DISPLAY_HEIGHT - y) {
        h = DISPLAY_HEIGHT - y;
    }
    if (w <= 0 || h <= 0) {
        goto out;
    }

    /* Swizzle the texture if feasible. */
    if (u == 0 && v == 0 && w == texture->width && h == texture->height
     && w%4 == 0 && h%8 == 0) {
        texture->swizzled = (readable == 0);
    }

    /* Do the actual copy. */
    fb_to_texture(x, y, w, h, texture, u, v);

  out:
    return texture;
}

/*-----------------------------------------------------------------------*/

void *sys_texture_lock(SysTexture *texture, SysTextureLockMode lock_mode,
                       int x, int y, int w, int h)
{
    PRECOND(texture != NULL, return NULL);
    PRECOND(x >= 0, return NULL);
    PRECOND(y >= 0, return NULL);
    PRECOND(w > 0, return NULL);
    PRECOND(h > 0, return NULL);
    PRECOND(x+w <= texture->width, return NULL);
    PRECOND(y+h <= texture->height, return NULL);

    if (texture->swizzled) {
        /* Safety check -- we never create unaligned-size swizzled textures
         * anyway, but if we try unswizzling with an unaligned height or
         * stride, we'll overrun the lock buffer. */
        const int bpp = bpp_for_ge_format(texture->format);
        ASSERT(texture->stride % (128/bpp) == 0, return NULL);
        ASSERT(texture->height % 8 == 0, return NULL);
    }

    const uint32_t size = w * h * 4;
    texture->lock_buf = mem_alloc(size, 0, MEM_ALLOC_TEMP);
    if (UNLIKELY(!texture->lock_buf)) {
        DLOG("lock(%p): Failed to get a lock buffer (%u bytes)",
             texture, size);
        return NULL;
    }

    if (lock_mode == SYS_TEXTURE_LOCK_NORMAL) {

        if (w == texture->stride && h == texture->height) {
            texture_to_RGBA(texture, texture->lock_buf);

        } else if (!texture->swizzled) {
            uint32_t *dest = texture->lock_buf;
            const int stride = texture->stride;
            if (texture->format == GE_TEXFMT_T8) {
                const uint32_t * const palette = texture->palette;
                const uint8_t *src = texture->pixels[0] + y*stride + x;
                if ((uintptr_t)src%4 == 0 && w%4 == 0) {
                    palette_to_RGBA_aligned(src, stride, palette, dest, w, h);
                } else {
                    for (int yy = 0; yy < h; yy++, src += stride - w) {
                        const uint8_t *top = src + w;
                        for (; (uintptr_t)src%4 != 0 && src < top;
                             src++, dest++) {
                            *dest = palette[*src];
                        }
                        for (; src+4 <= top; src += 4, dest += 4) {
                            /* Cast through void * to suppress alignment
                             * warning. */
                            uint32_t pixels = *(uint32_t *)(void *)src;
                            dest[0] = palette[pixels>> 0 & 0xFF];
                            dest[1] = palette[pixels>> 8 & 0xFF];
                            dest[2] = palette[pixels>>16 & 0xFF];
                            dest[3] = palette[pixels>>24 & 0xFF];
                        }
                        for (; src < top; src++, dest++) {
                            *dest = palette[*src];
                        }
                    }
                }
            } else if (texture->format == GE_TEXFMT_8888) {
                const uint32_t *src =
                    (const uint32_t *)((void *)texture->pixels[0])
                    + y*stride + x;
                for (int yy = 0; yy < h; yy++, src += stride, dest += w) {
                    memcpy(dest, src, w*4);
                }
            } else {  // 16bpp
                const int format = texture->format;
                const uint16_t *src =
                    (const uint16_t *)((void *)texture->pixels[0])
                    + y*stride + x;
                for (int yy = 0; yy < h; yy++, src += stride - w) {
                    for (int xx = 0; xx < w; xx++, src++, dest++) {
                        *dest = pixel_16bpp_to_RGBA(format, *src);
                    }
                }
            }

        } else {  // texture->swizzled
            /* For normal use this should be a rare case, so for simplicity,
             * we just deswizzle the entire texture and extract the desired
             * portion from the unswizzled data. */
            const uint32_t full_size = texture->stride * texture->height * 4;
            uint32_t *texture_buf = mem_alloc(full_size, 0, MEM_ALLOC_TEMP);
            if (UNLIKELY(!texture_buf)) {
                DLOG("lock(%p): Failed to get a texture image buffer (%u"
                     " bytes)", texture, full_size);
                mem_free(texture->lock_buf);
                texture->lock_buf = NULL;
                return NULL;
            }
            texture_to_RGBA(texture, texture_buf);
            const uint32_t *src = texture_buf + y*texture->width + x;
            uint32_t *dest = texture->lock_buf;
            if (w == texture->width) {
                memcpy(dest, src, h * texture->width * 4);
            } else {
                for (int yy = 0; yy < h;
                     yy++, src += texture->stride, dest += w)
                {
                    memcpy(dest, src, w*4);
                }
            }
            mem_free(texture_buf);
        }

    }  // if (lock_mode == SYS_TEXTURE_LOCK_NORMAL)

    return (uint8_t *)texture->lock_buf;
}

/*-----------------------------------------------------------------------*/

void sys_texture_unlock(SysTexture *texture, int update)
{
    PRECOND(texture != NULL, return);
    PRECOND(texture->lock_buf != NULL, return);

    if (update) {
        RGBA_to_texture(texture->lock_buf, texture->width,
                        texture->width, texture->height, 1, texture, 0, 0);
        sys_texture_flush(texture);
    }

    mem_free(texture->lock_buf);
    texture->lock_buf = NULL;
}

/*-----------------------------------------------------------------------*/

void sys_texture_flush(SysTexture *texture)
{
    PRECOND(texture != NULL, return);

    sceKernelDcacheWritebackRange(texture->pixels[0],
                                  texture->stride * texture->height * 4);
    if (loaded_texture == texture) {
        ge_flush_texture_cache();
    }
}

/*************************************************************************/
/********************* Interface: Rendering control **********************/
/*************************************************************************/

void sys_texture_set_repeat(SysTexture *texture, int repeat_u, int repeat_v)
{
    texture->repeat_u = repeat_u;
    texture->repeat_v = repeat_v;
    if (loaded_texture == texture) {
        set_texture_state_repeat(texture);
    }
}

/*-----------------------------------------------------------------------*/

void sys_texture_set_antialias(SysTexture *texture, int on)
{
    texture->antialias = on;
    if (loaded_texture == texture) {
        set_texture_state_antialias(texture);
    }
}

/*-----------------------------------------------------------------------*/

void sys_texture_apply(int unit, SysTexture *texture)
{
    if (UNLIKELY(unit != 0)) {
        DLOG("Invalid unit %d", unit);
        return;
    }
    current_texture = texture;
}

/*-----------------------------------------------------------------------*/

int sys_texture_num_units(void)
{
    return 1;
}

/*************************************************************************/
/*********************** Exported utility routines ***********************/
/*************************************************************************/

void *psp_texture_get_pixel_data(SysTexture *texture)
{
    PRECOND(texture != NULL, return NULL);
    return texture->pixels[0];
}

/*-----------------------------------------------------------------------*/

const void *psp_texture_get_palette(SysTexture *texture)
{
    PRECOND(texture != NULL, return NULL);
    return texture->palette;
}

/*-----------------------------------------------------------------------*/

void psp_texture_set_palette(SysTexture *texture, const void *palette)
{
    PRECOND(texture != NULL, return);

    if (palette) {
        sceKernelDcacheWritebackRange(palette, 256*4);
    }
    texture->palette = palette ? palette : texture->orig_palette;
    if (loaded_texture == texture) {
        ge_set_colortable(texture->palette, 256, GE_PIXFMT_8888, 0, 0xFF);
        loaded_texture_palette = texture->palette;
    }
}

/*************************************************************************/
/******************** PSP-specific interface routines ********************/
/*************************************************************************/

void psp_texture_init(void)
{
    current_texture = NULL;
    loaded_texture = NULL;
    loaded_texture_palette = NULL;
    loaded_texture_scale_x = 512;
    loaded_texture_scale_y = 512;
    loaded_texture_antialias = 0;
    loaded_texture_repeat_u = 1;
    loaded_texture_repeat_v = 1;
}

/*-----------------------------------------------------------------------*/

void psp_set_texture_state(int force)
{
    if (!force && current_texture == loaded_texture) {
        return;
    }

    const int had_loaded_texture = (loaded_texture != NULL);
    loaded_texture = current_texture;

    if (current_texture) {

        SysTexture *texture = current_texture;  // For brevity.

        if (texture->format == GE_TEXFMT_T8) {
            if (force || texture->palette != loaded_texture_palette) {
                ge_set_colortable(texture->palette, 256, GE_PIXFMT_8888, 0,
                                  0xFF);
                loaded_texture_palette = texture->palette;
            }
        } else {
            /* A previously loaded palette might be freed and the address
             * subsequently allocated to a different palette, so make sure
             * to reload the palette in that case. */
            loaded_texture_palette = NULL;
        }

        int width = texture->width, height = texture->height;
        int stride = texture->stride;
        const int format = texture->format;
        const int bpp = bpp_for_ge_format(format);
        for (unsigned int level = 0; level <= texture->mipmaps; level++) {
            ge_set_texture_data(level, texture->pixels[level],
                                width, height, stride);
            width  = lbound(width/2, 1);
            height = lbound(height/2, 1);
            stride = align_up(stride/2, 128/bpp);
        }

        ge_set_texture_format(texture->mipmaps + 1, texture->swizzled, format);
        ge_set_texture_draw_mode(GE_TEXDRAWMODE_MODULATE, 1);

        /* Use texture coordinate scaling to adjust texture coordinates
         * when the texture width or height is not a power of 2. */
        const int effective_w = ubound(texture->width, 512);
        const int effective_h = ubound(texture->height, 512);
        const int log2_width  = texture->width==1 ? 0 :
            ubound(32 - __builtin_clz(effective_w-1), 9);
        const int log2_height = texture->height==1 ? 0 :
            ubound(32 - __builtin_clz(effective_h-1), 9);
        const int scale_x = effective_w * (512 >> log2_width);
        const int scale_y = effective_h * (512 >> log2_height);
        if (force || scale_x != loaded_texture_scale_x
                  || scale_y != loaded_texture_scale_y) {
            ge_set_texture_scale(scale_x / 512.0f, scale_y / 512.0f);
            loaded_texture_scale_x = scale_x;
            loaded_texture_scale_y = scale_y;
        }

        if (force || texture->antialias != loaded_texture_antialias
                  || (texture->mipmaps != 0) != loaded_texture_mipmaps) {
            set_texture_state_antialias(texture);
        }

        if (force || texture->repeat_u != loaded_texture_repeat_u
                  || texture->repeat_v != loaded_texture_repeat_v) {
            set_texture_state_repeat(texture);
        }

        if (force || !had_loaded_texture) {
            ge_enable(GE_STATE_TEXTURE);
        }

    } else {  // current_texture == NULL

        loaded_texture_palette = NULL;
        /* If we get here, either force is true or loaded_texture was
         * non-NULL, so we always need to send a disable-texture command. */
        ge_disable(GE_STATE_TEXTURE);

    }
}

/*-----------------------------------------------------------------------*/

SysTexture *psp_current_texture(void)
{
    return current_texture;
}

/*-----------------------------------------------------------------------*/

void psp_texture_flush_deferred_destroy_list(void)
{
    for (SysTexture *texture = deferred_destroy_list, *next;
         texture != NULL; texture = next)
    {
        next = *(SysTexture **)texture;
        if (texture->vram) {
            psp_vram_free(texture->pixels[0]);
        }
        mem_free(texture);
    }
    deferred_destroy_list = NULL;
}

/*-----------------------------------------------------------------------*/

SysTexture *psp_create_vram_texture(int width, int height)
{
    if (UNLIKELY(width <= 0) || UNLIKELY(height <= 0)) {
        DLOG("Invalid parameters: %d %d", width, height);
        return NULL;
    }

    const int stride = align_up(width, 4);

    void *pixels = psp_vram_alloc(stride * height * 4, 64);
    if (UNLIKELY(!pixels)) {
        DLOG("No VRAM available for %dx%d pixel buffer", stride, height);
        return NULL;
    }

    SysTexture *texture = mem_alloc(sizeof(*texture), 0, MEM_ALLOC_TEMP);
    if (UNLIKELY(!texture)) {
        DLOG("Failed to allocate texture");
        psp_vram_free(pixels);
        return NULL;
    }
    mem_debug_set_info(texture, MEM_INFO_TEXTURE);

    texture->width     = width;
    texture->height    = height;
    texture->stride    = stride;
    texture->format    = GE_TEXFMT_8888;
    texture->swizzled  = 0;
    texture->vram      = 1;
    texture->mipmaps   = 0;
    texture->pixels[0] = pixels;
    texture->palette   = NULL;
    texture->antialias = 1;
    texture->repeat_u  = 1;
    texture->repeat_v  = 1;
    texture->lock_buf  = NULL;
    return texture;
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static int bpp_for_ge_format(int format)
{
    static const uint8_t bpp_table[] = {
        [GE_TEXFMT_5650] = 16,
        [GE_TEXFMT_5551] = 16,
        [GE_TEXFMT_4444] = 16,
        [GE_TEXFMT_8888] = 32,
        [GE_TEXFMT_T4]   =  4,
        [GE_TEXFMT_T8]   =  8,
        [GE_TEXFMT_T16]  = 16,
        [GE_TEXFMT_T32]  = 32,
        [GE_TEXFMT_DXT1] =  4,
        [GE_TEXFMT_DXT3] =  8,
        [GE_TEXFMT_DXT5] =  8,
    };
    ASSERT(format >= 0 && format < lenof(bpp_table), return 32);
    return bpp_table[format];
}

/*-----------------------------------------------------------------------*/

static void texture_to_RGBA(SysTexture *texture, uint32_t *dest)
{
    PRECOND(texture != NULL, return);
    PRECOND(dest != NULL, return);

    const int width = texture->width;
    const int height = texture->height;
    const int stride = texture->stride;
    const uint32_t * const palette = texture->palette;

    if (texture->swizzled) {

        if (texture->format == GE_TEXFMT_T8) {

            const uint8_t *src = texture->pixels[0];
            for (int y = 0; y < height; y += 8, dest += stride*7) {
                for (int x = 0; x < width; x += 16, dest += 16) {
                    uint32_t *destline = dest;
                    for (int line = 0; line < 8; line++, destline += width) {
                        for (int i = 0; i < 16; i += 4, src += 4) {
                            const uint8_t pixel0 = src[0];
                            const uint8_t pixel1 = src[1];
                            const uint8_t pixel2 = src[2];
                            const uint8_t pixel3 = src[3];
                            destline[i+0] = palette[pixel0];
                            destline[i+1] = palette[pixel1];
                            destline[i+2] = palette[pixel2];
                            destline[i+3] = palette[pixel3];
                        }
                    }
                }
            }

        } else if (texture->format == GE_TEXFMT_8888) {

            const uint32_t *src =
                (const uint32_t *)((void *)texture->pixels[0]);
            for (int y = 0; y < height; y += 8, dest += stride*7) {
                for (int x = 0; x < width; x += 4, dest += 4) {
                    uint32_t *destline = dest;
                    for (int line = 0; line < 8;
                         line++, src += 4, destline += stride)
                    {
                        const uint32_t pixel0 = src[0];
                        const uint32_t pixel1 = src[1];
                        const uint32_t pixel2 = src[2];
                        const uint32_t pixel3 = src[3];
                        destline[0] = pixel0;
                        destline[1] = pixel1;
                        destline[2] = pixel2;
                        destline[3] = pixel3;
                    }
                }
            }

        } else {  // 16bpp

            const int format = texture->format;
            const uint16_t *src =
                (const uint16_t *)((void *)texture->pixels[0]);
            for (int y = 0; y < height; y += 8, dest += stride*7) {
                for (int x = 0; x < width; x += 8, dest += 8) {
                    uint32_t *destline = dest;
                    for (int line = 0; line < 8; line++, destline += stride) {
                        for (int i = 0; i < 8; i++, src++) {
                            destline[i] = pixel_16bpp_to_RGBA(format, *src);
                        }
                    }
                }
            }

        }

    } else {  // !swizzled

        if (texture->format == GE_TEXFMT_T8) {
            const uint8_t *src = texture->pixels[0];
            for (int y = 0; y < height; y++, src += stride - width) {
                for (int x = 0; x < width; x++, src++, dest++) {
                    *dest = palette[*src];
                }
            }
        } else if (texture->format == GE_TEXFMT_8888) {
            const uint32_t *src =
                (const uint32_t *)((void *)texture->pixels[0]);
            for (int y = 0; y < height; y++, src += stride, dest += width) {
                memcpy(dest, src, width*4);
            }
        } else {  // 16bpp
            const int format = texture->format;
            const uint16_t *src =
                (const uint16_t *)((void *)texture->pixels[0]);
            for (int y = 0; y < height; y++, src += stride - width) {
                for (int x = 0; x < width; x++, src++, dest++) {
                    *dest = pixel_16bpp_to_RGBA(format, *src);
                }
            }
        }

    }  // if (swizzled)
}

/*-----------------------------------------------------------------------*/

static inline void palette_to_RGBA_aligned(
    const uint8_t *src, int stride, const uint32_t *palette, uint32_t *dest,
    int width, int height)
{
    for (int y = 0; y < height; y++, src += stride - width) {
#if 1  // ~29 cycles/loop
            __asm__(
                ".set push; .set noreorder\n"
                "0:"
                "lw $t7, 0(%[src])              \n"
                "addi %[src], %[src], 4         \n"
                "addi %[dest], %[dest], 16      \n"
                /* The PSP's R4000 implementation appears to forward
                 * results from EX[n] to EX[n+1] but not to EX[n+2] or
                 * later (those end up stalling until WB[n]), so it's
                 * faster to perform a multi-step computation in
                 * consecutive instructions rather than interleaving
                 * multiple computations. */
                "andi $t4, $t7, 0xFF            \n"
                "sll $t4, $t4, 2                \n"
                "add $t4, $t4, %[palette]       \n"
                "lw $t4, 0($t4)                 \n"
                "ext $t5, $t7, 8, 8             \n"
                "sll $t5, $t5, 2                \n"
                "add $t5, $t5, %[palette]       \n"
                "lw $t5, 0($t5)                 \n"
                "sw $t4, -16(%[dest])           \n"
                "ext $t6, $t7, 16, 8            \n"
                "sll $t6, $t6, 2                \n"
                "add $t6, $t6, %[palette]       \n"
                "lw $t6, 0($t6)                 \n"
                "sw $t5, -12(%[dest])           \n"
                "srl $t7, $t7, 24               \n"
                "sll $t7, $t7, 2                \n"
                "add $t7, $t7, %[palette]       \n"
                "lw $t7, 0($t7)                 \n"
                "sw $t6, -8(%[dest])            \n"
                "sltu $t8, %[src], %[top]       \n"
                "bnez $t8, 0b                   \n"
                "sw $t7, -4(%[dest])            \n"
                ".set pop                       \n"
                : [src] "=r" (src), [dest] "=r" (dest), "=m" (*dest)
                : "0" (src), "1" (dest), [top] "r" (src + width),
                  [palette] "r" (palette)
                : "t4", "t5", "t6", "t7", "t8", "memory"
            );
#else  // GCC 4.5: ~34 cycles/loop (15% slower)
        for (const uint8_t *top = src+width; src < top; src += 4, dest += 4) {
            /* Cast through void * to suppress alignment warning. */
            uint32_t pixels = *(uint32_t *)(void *)src;
            dest[0] = palette[pixels>> 0 & 0xFF];
            dest[1] = palette[pixels>> 8 & 0xFF];
            dest[2] = palette[pixels>>16 & 0xFF];
            dest[3] = palette[pixels>>24 & 0xFF];
        }
#endif
    }
}

/*-----------------------------------------------------------------------*/

static inline uint32_t pixel_16bpp_to_RGBA(
    const int format, const uint16_t pixel)
{
    if (format == GE_TEXFMT_5650) {
        const uint32_t b = pixel>>11 & 0x1F;
        const uint32_t g = pixel>> 5 & 0x3F;
        const uint32_t r = pixel>> 0 & 0x1F;
        return 0xFF000000
             | (b<<3 | b>>2) << 16
             | (g<<2 | g>>4) <<  8
             | (r<<3 | r>>2) <<  0;
    } else if (format == GE_TEXFMT_5551) {
        const uint32_t a = pixel>>15 & 0x01;
        const uint32_t b = pixel>>10 & 0x1F;
        const uint32_t g = pixel>> 5 & 0x1F;
        const uint32_t r = pixel>> 0 & 0x1F;
        return (a ? 0xFF000000 : 0)
             | (b<<3 | b>>2) << 16
             | (g<<3 | g>>2) <<  8
             | (r<<3 | r>>2) <<  0;
    } else {  // format == GE_TEXFMT_4444
        const uint32_t a = pixel>>12 & 0x0F;
        const uint32_t b = pixel>> 8 & 0x0F;
        const uint32_t g = pixel>> 4 & 0x0F;
        const uint32_t r = pixel>> 0 & 0x0F;
        return (a<<4 | a) << 24
             | (b<<4 | b) << 16
             | (g<<4 | g) <<  8
             | (r<<4 | r) <<  0;
    }
}

/*-----------------------------------------------------------------------*/

static void RGBA_to_texture(const uint32_t *src, int src_stride,
                            int width, int height, int has_alpha,
                            SysTexture *texture, int u0, int v0)
{
    PRECOND(src != NULL, return);
    PRECOND(texture != NULL, return);
    PRECOND(width <= texture->width, return);
    PRECOND(height <= texture->height, return);
    PRECOND(!texture->swizzled
            || (width%4 == 0 && height%8 == 0 && u0 == 0 && v0 == 0), return);

    const uint32_t alpha_mod = has_alpha ? 0 : 0xFF000000;
    uint32_t *dest = (uint32_t *)((void *)texture->pixels[0]);
    uint32_t dest_stride = texture->stride;
    dest += v0*dest_stride + u0;

    if (texture->swizzled) {
        for (int y = 0; y < height;
             y += 8, src += src_stride*8, dest += dest_stride*8)
        {
            uint32_t *dest_row = dest;
            for (int x = 0; x < width; x += 4) {
                const uint32_t *src_line = &src[x];
                for (int line = 0; line < 8;
                     line++, src_line += src_stride, dest_row += 4)
                {
                    const uint32_t pixel0 = src_line[0];
                    const uint32_t pixel1 = src_line[1];
                    const uint32_t pixel2 = src_line[2];
                    const uint32_t pixel3 = src_line[3];
                    dest_row[0] = pixel0 | alpha_mod;
                    dest_row[1] = pixel1 | alpha_mod;
                    dest_row[2] = pixel2 | alpha_mod;
                    dest_row[3] = pixel3 | alpha_mod;
                }
            }
        }
    } else if (width % 4 == 0) {
        for (int y = 0; y < height;
             y++, src += src_stride - width, dest += dest_stride - width)
        {
            for (int x = 0; x < width; x += 4, src += 4, dest += 4) {
                const uint32_t pixel0 = src[0];
                const uint32_t pixel1 = src[1];
                const uint32_t pixel2 = src[2];
                const uint32_t pixel3 = src[3];
                dest[0] = pixel0 | alpha_mod;
                dest[1] = pixel1 | alpha_mod;
                dest[2] = pixel2 | alpha_mod;
                dest[3] = pixel3 | alpha_mod;
            }
        }
    } else {
        for (int y = 0; y < height;
             y++, src += src_stride - width, dest += dest_stride - width)
        {
            for (int x = 0; x < width; x++, src++, dest++) {
                *dest = *src | alpha_mod;
            }
        }
    }
}

/*-----------------------------------------------------------------------*/

static void fb_to_texture(int x0, int y0, int width, int height,
                          SysTexture *texture, int u0, int v0)
{
    PRECOND(x0 + width <= DISPLAY_WIDTH, return);
    PRECOND(y0 + height <= DISPLAY_HEIGHT, return);
    PRECOND(texture != NULL, return);
    PRECOND(width <= texture->stride, return);
    PRECOND(height <= texture->height, return);

    psp_sync_framebuffer(x0, y0, width, height);

    const int fb_stride = psp_framebuffer_stride();
    const uint32_t *src;
    int src_stride;
    if (psp_current_framebuffer()) {
        src = psp_fb_pixel_address(x0, y0);
        src_stride = fb_stride;
    } else {
        /* Tweak source parameters because we need to start from the bottom
         * rather than the top when reading from the display buffer. */
        src = psp_fb_pixel_address(x0, (psp_framebuffer_height()-1) - y0);
        src_stride = -fb_stride;
    }

    RGBA_to_texture(src, src_stride, width, height, 0, texture, u0, v0);
}

/*-----------------------------------------------------------------------*/

static void set_texture_state_repeat(const SysTexture *texture)
{
    ge_set_texture_wrap_mode(
        texture->repeat_u ? GE_TEXWRAPMODE_REPEAT : GE_TEXWRAPMODE_CLAMP,
        texture->repeat_v ? GE_TEXWRAPMODE_REPEAT : GE_TEXWRAPMODE_CLAMP);
    loaded_texture_repeat_u = texture->repeat_u;
    loaded_texture_repeat_v = texture->repeat_v;
}

/*-----------------------------------------------------------------------*/

static void set_texture_state_antialias(const SysTexture *texture)
{
    if (texture->antialias) {
        ge_set_texture_filter(GE_TEXFILTER_LINEAR, GE_TEXFILTER_LINEAR,
                              texture->mipmaps ? GE_TEXMIPFILTER_LINEAR
                                               : GE_TEXMIPFILTER_NONE);
    } else {
        ge_set_texture_filter(GE_TEXFILTER_NEAREST, GE_TEXFILTER_NEAREST,
                              texture->mipmaps ? GE_TEXMIPFILTER_NEAREST
                                               : GE_TEXMIPFILTER_NONE);
    }
    loaded_texture_antialias = texture->antialias;
    loaded_texture_mipmaps = (texture->mipmaps > 0);
}

/*************************************************************************/
/*************************************************************************/
