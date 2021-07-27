/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/texture.h: Texture data structure for the PSP.
 */

#ifndef SIL_SRC_SYSDEP_PSP_TEXTURE_H
#define SIL_SRC_SYSDEP_PSP_TEXTURE_H

/*************************************************************************/
/*************************************************************************/

/* PSP-internal data structure used for textures. */

struct SysTexture {
    /* Basic texture data. */
    int16_t width, height;      // Texture size (in pixels).
    int16_t stride;             // Texture line stride (in pixels, always a
                                //    multiple of 16 bytes).
    uint8_t format:4,           // Texture format (GE_TEXFMT_*).
            swizzled:1,         // 1 = pixel data is swizzled.
            vram:1;             // 1 = pixel data is stored in VRAM.
    uint8_t mipmaps;            // Number of mipmap levels, _not_ including
                                //    primary texture data (0-7).  Odd
                                //    sizes are rounded down when halving
                                //    to compute mipmap width/height.
    uint8_t *pixels[8];         // Pixel data for each image level.
    const uint32_t *palette;    // Color palette (for indexed-color images).
    const uint32_t *orig_palette; //Palette originally associated with texture.

    /* Runtime data. */
    uint8_t repeat_u, repeat_v; // Texture coordinate wrap flags.
    uint8_t antialias;          // Texture antialiasing flag.
    uint32_t *lock_buf;         // RGBA buffer for texture locking (NULL if
                                //    not locked).
};

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_PSP_TEXTURE_H
