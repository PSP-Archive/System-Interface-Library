/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/utility/dds.c: Utility routines for reading the DDS texture file format.
 */

#include "src/base.h"
#include "src/endian.h"
#include "src/texture.h"  // For texture format codes.
#include "src/utility/dds.h"

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int dds_get_info(const void *data_, uint32_t size, DDSInfo *dds_ret)
{
    const uint8_t *data = data_;
    if (UNLIKELY(!data) || UNLIKELY(!dds_ret)) {
        DLOG("Invalid parameters: %p %u %p", data, size, dds_ret);
        return 0;
    }
    if (size < 128 || memcmp(data, "DDS \x7C\x00\x00\x00", 8) != 0) {
        DLOG("Not a DDS file");
        return 0;
    }

    DDSInfo dds;

    uint32_t flags  = data[  8] | data[  9]<<8 | data[ 10]<<16 | data[ 11]<<24;
    dds.height      = data[ 12] | data[ 13]<<8 | data[ 14]<<16 | data[ 15]<<24;
    dds.width       = data[ 16] | data[ 17]<<8 | data[ 18]<<16 | data[ 19]<<24;
    dds.stride      = !(flags & 0x8) ? dds.width :
                      data[ 20] | data[ 21]<<8 | data[ 22]<<16 | data[ 23]<<24;
    uint32_t levels = !(flags & 0x20000) ? 0 :
                      data[ 28] | data[ 29]<<8 | data[ 30]<<16 | data[ 31]<<24;
    uint32_t flags2 = data[ 80] | data[ 81]<<8 | data[ 82]<<16 | data[ 83]<<24;
    const uint8_t *compression = &data[84];
    uint32_t bpp    = data[ 88] | data[ 89]<<8 | data[ 90]<<16 | data[ 91]<<24;
    uint32_t r_mask = data[ 92] | data[ 93]<<8 | data[ 94]<<16 | data[ 95]<<24;
    uint32_t g_mask = data[ 96] | data[ 97]<<8 | data[ 98]<<16 | data[ 99]<<24;
    uint32_t b_mask = data[100] | data[101]<<8 | data[102]<<16 | data[103]<<24;
    uint32_t a_mask = data[104] | data[105]<<8 | data[106]<<16 | data[107]<<24;

    if (dds.width <= 0 || dds.height <= 0) {
        DLOG("Invalid width/height: %dx%d", dds.width, dds.height);
        return 0;
    }
    if (dds.stride < 0) {
        DLOG("Invalid stride: %d", dds.stride);
        return 0;
    }

    if (levels > (uint32_t)lenof(dds.data)) {
        DLOG("Too many mipmap levels (%u), truncating to %d", levels,
             lenof(dds.data));
        levels = lenof(dds.data);
    }
    dds.mipmaps = (levels > 0 ? levels-1 : 0);

    int min_width, min_height;
    if (flags2 & 0x4) {
        if (flags & 0x8) {  // Stride is meaningless for compressed formats.
            DLOG("Stride given for compressed format (invalid)");
            return 0;
        }
        if (memcmp(compression, "DXT1", 4) == 0) {
            if (bpp != 4) {
                DLOG("Invalid bpp %u for DXT1", bpp);
                return 0;
            }
            dds.format = TEX_FORMAT_S3TC_DXT1;
            min_width = min_height = 4;
        } else if (memcmp(compression, "DXT3", 4) == 0) {
            if (bpp != 8) {
                DLOG("Invalid bpp %u for DXT3", bpp);
                return 0;
            }
            dds.format = TEX_FORMAT_S3TC_DXT3;
            min_width = min_height = 4;
        } else if (memcmp(compression, "DXT5", 4) == 0) {
            if (bpp != 8) {
                DLOG("Invalid bpp %u for DXT5", bpp);
                return 0;
            }
            dds.format = TEX_FORMAT_S3TC_DXT5;
            min_width = min_height = 4;
        } else {
            DLOG("Unrecognized compression format: %.4s", compression);
            return 0;
        }
    } else {
        static const struct {
            unsigned int bpp;
            uint32_t r_mask, g_mask, b_mask, a_mask;
            uint8_t format;
        } raw_formats[] = {
            {32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000,
             TEX_FORMAT_RGBA8888},
            {32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000,
             TEX_FORMAT_BGRA8888},
            {16, 0x001F, 0x07E0, 0xF800, 0x0000, TEX_FORMAT_RGB565},
            {16, 0x001F, 0x03E0, 0x7C00, 0x8000, TEX_FORMAT_RGBA5551},
            {16, 0x000F, 0x00F0, 0x0F00, 0xF000, TEX_FORMAT_RGBA4444},
            {16, 0xF800, 0x07E0, 0x001F, 0x0000, TEX_FORMAT_BGR565},
            {16, 0x7C00, 0x03E0, 0x001F, 0x8000, TEX_FORMAT_BGRA5551},
            {16, 0x0F00, 0x00F0, 0x000F, 0xF000, TEX_FORMAT_BGRA4444},
            { 8, 0x00, 0x00, 0x00, 0xFF, TEX_FORMAT_A8},
        };
        int found = 0;
        for (int i = 0; i < lenof(raw_formats); i++) {
            if (bpp    == raw_formats[i].bpp
             && r_mask == raw_formats[i].r_mask
             && g_mask == raw_formats[i].g_mask
             && b_mask == raw_formats[i].b_mask
             && a_mask == raw_formats[i].a_mask) {
                dds.format = raw_formats[i].format;
                found = 1;
                break;
            }
        }
        if (!found) {
            DLOG("Unrecognized bpp/mask combination: %u r=%08X g=%08X"
                 " b=%08X a=%08X", bpp, r_mask, g_mask, b_mask, a_mask);
            return 0;
        }
        min_width = min_height = 1;
    }

    const uint8_t *pixels = (const uint8_t *)data + 128;
    size -= 128;
    unsigned int level;
    for (level = 0; level <= dds.mipmaps; level++) {
        const int height = lbound(dds.height >> level, min_height);
        const int stride = lbound(dds.stride >> level, min_width);
        uint32_t level_size = (stride * height * bpp) / 8;
        if (level_size > size) {
            DLOG("Not enough data for level %u (need %u bytes for %ux%ux%u,"
                 " have %u)", level, level_size,
                 lbound(dds.width >> level, min_width), height, bpp, size);
            return 0;
        }
        dds.data[level] = pixels;
        pixels += level_size;
        size -= level_size;
    }
    for (; level < lenof(dds.data); level++) {
        /* Avoid a spurious warning on mips64 and x86_64. */
#if defined(__GNUC__) && (defined(__amd64__) || defined(__mips64))
# pragma GCC diagnostic ignored "-Warray-bounds"
#endif
        dds.data[level] = NULL;
    }

    *dds_ret = dds;
    return 1;
}

/*************************************************************************/
/*************************************************************************/
