/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/utility/tex-file.c: Utility routines for reading the SIL custom
 * texture file format.
 */

#include "src/base.h"
#include "src/endian.h"
#include "src/texture.h"
#include "src/utility/tex-file.h"

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int tex_parse_header(const void *data, int size, TexFileHeader *header_ret)
{
    if (UNLIKELY(!data) || UNLIKELY(size < 0) || UNLIKELY(!header_ret)) {
        DLOG("Invalid parameters: %p %d %p", data, size, header_ret);
        return 0;
    }
    if (UNLIKELY((uintptr_t)data % 4 != 0)) {
        DLOG("Input pointer %p is not 4-byte aligned", data);
        return 0;
    }

    const TexFileHeader *header_in = (const TexFileHeader *)data;

    if (size < (int)sizeof(TexFileHeader)) {
        DLOG("File too small for TexFileHeader (%d < %d)", size,
             (int)sizeof(TexFileHeader));
        return 0;
    }
    if (memcmp(header_in->magic, TEX_FILE_MAGIC,
               sizeof(header_in->magic)) != 0) {
        DLOG("TEX signature not found");
        return 0;
    }

    uint8_t format;
    uint8_t mipmaps;
    uint8_t opaque_bitmap;
    int16_t width;
    int16_t height;
    float scale;
    int32_t pixels_offset;
    int32_t pixels_size;
    int32_t bitmap_offset;
    int32_t bitmap_size;
    if (header_in->version == TEX_FILE_VERSION) {
        format        = header_in->format;
        mipmaps       = header_in->mipmaps;
        opaque_bitmap = header_in->opaque_bitmap;
        width         = be_to_s16(header_in->width);
        height        = be_to_s16(header_in->height);
        scale         = be_to_s32(header_in->scale_int) / 65536.0f;
        pixels_offset = be_to_u32(header_in->pixels_offset);
        pixels_size   = be_to_u32(header_in->pixels_size);
        bitmap_offset = be_to_u32(header_in->bitmap_offset);
        bitmap_size   = be_to_u32(header_in->bitmap_size);
    } else if (header_in->version == 1) {
        typedef struct TexFileHeaderV1 TexFileHeaderV1;
        struct TexFileHeaderV1 {
            char magic[4];
            uint8_t version;
            uint8_t pad1[3];
            int16_t width, height;
            uint8_t scale;
            uint8_t format;
            uint8_t mipmaps;
            uint8_t opaque_bitmap;
            int32_t pixels_offset;
            int32_t pixels_size;
            int32_t bitmap_offset;
            int32_t bitmap_size;
        };
        const TexFileHeaderV1 *header_v1 = (const TexFileHeaderV1 *)data;
        width         = be_to_s16(header_v1->width);
        height        = be_to_s16(header_v1->height);
        scale         = header_v1->scale / 16.0f;
        format        = header_v1->format;
        mipmaps       = header_v1->mipmaps;
        opaque_bitmap = header_v1->opaque_bitmap;
        pixels_offset = be_to_s32(header_v1->pixels_offset);
        pixels_size   = be_to_s32(header_v1->pixels_size);
        bitmap_offset = be_to_s32(header_v1->bitmap_offset);
        bitmap_size   = be_to_s32(header_v1->bitmap_size);
        switch (format) {
            case 0x01: format = TEX_FORMAT_PALETTE8_RGBA8888; break;
            case 0x02: format = TEX_FORMAT_A8;                break;
            case 0x80: format = TEX_FORMAT_PVRTC2_RGB;        break;
            case 0x81: format = TEX_FORMAT_PVRTC4_RGB;        break;
            case 0x82: format = TEX_FORMAT_PVRTC2_RGBA;       break;
            case 0x83: format = TEX_FORMAT_PVRTC4_RGBA;       break;
        }
    } else {
        DLOG("Invalid version %u", header_in->version);
        return 0;
    }

    if (opaque_bitmap && bitmap_size != (width+7)/8 * height) {
        DLOG("Opaque bitmap data is wrong size (%d, should be %d for %dx%d"
             " texture)", bitmap_size, ((width+7)/8) * height, width, height);
        return 0;
    }

    if (pixels_offset < 0) {
        DLOG("Pixel data offset is negative (%d)", pixels_offset);
        return 0;
    }
    if (pixels_offset > size) {
        DLOG("Pixel data starts beyond end of file (%d > %d)",
             pixels_offset, size);
        return 0;
    }
    /* Note the format of this comparison!  This avoids misbehavior in the
     * case that adding pixels_offset + pixels_size causes overflow. */
    if (pixels_size > size - pixels_offset) {
        DLOG("Pixel data extends past end of file (%d + %d > %d)",
             pixels_offset, pixels_size, size);
        return 0;
    }

    if (bitmap_offset < 0) {
        DLOG("Bitmap data offset is negative (%d)", pixels_offset);
        return 0;
    }
    if (bitmap_offset > size) {
        DLOG("Opaque bitmap data starts beyond end of file (%d > %d)",
             bitmap_offset, size);
        return 0;
    }
    if (bitmap_size > size - bitmap_offset) {
        DLOG("Opaque bitmap data extends past end of file (%d + %d > %d)",
             bitmap_offset, bitmap_size, size);
        return 0;
    }

    memcpy(header_ret->magic, TEX_FILE_MAGIC, sizeof(header_ret->magic));
    header_ret->version       = TEX_FILE_VERSION;
    header_ret->format        = format;
    header_ret->mipmaps       = mipmaps;
    header_ret->opaque_bitmap = opaque_bitmap;
    header_ret->width         = width;
    header_ret->height        = height;
    header_ret->scale         = scale;
    header_ret->pixels_offset = pixels_offset;
    header_ret->pixels_size   = pixels_size;
    header_ret->bitmap_offset = bitmap_offset;
    header_ret->bitmap_size   = bitmap_size;
    return 1;
}

/*************************************************************************/
/*************************************************************************/
