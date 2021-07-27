/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/utility/font-file.c: Utility routines for reading the SIL bitmap
 * font file format.
 */

#include "src/base.h"
#include "src/endian.h"
#include "src/utility/font-file.h"

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int font_parse_header(const void *data, int size, FontFileHeader *header_ret)
{
    if (UNLIKELY(!data) || UNLIKELY(size < 0) || UNLIKELY(!header_ret)) {
        DLOG("Invalid parameters: %p %d %p", data, size, header_ret);
        return 0;
    }
    if (UNLIKELY((uintptr_t)data % 4 != 0)) {
        DLOG("Input pointer %p is not 4-byte aligned", data);
        return 0;
    }

    const FontFileHeader *header_in = (const FontFileHeader *)data;

    if (size < (int)sizeof(FontFileHeader)) {
        DLOG("File too small for FontFileHeader (%d < %d)", size,
             (int)sizeof(FontFileHeader));
        return 0;
    }
    if (memcmp(header_in->magic, FONT_FILE_MAGIC,
               sizeof(header_in->magic)) != 0) {
        DLOG("FNT signature not found");
        return 0;
    }

    uint8_t height;
    uint8_t baseline;
    int32_t charinfo_offset;
    int16_t charinfo_count;
    int16_t charinfo_size;
    int32_t texture_offset;
    int32_t texture_size;
    if (header_in->version == FONT_FILE_VERSION) {
        height          = header_in->height;
        baseline        = header_in->baseline;
        charinfo_offset = be_to_s32(header_in->charinfo_offset);
        charinfo_count  = be_to_s16(header_in->charinfo_count);
        charinfo_size   = be_to_s16(header_in->charinfo_size);
        texture_offset  = be_to_s32(header_in->texture_offset);
        texture_size    = be_to_s32(header_in->texture_size);
        if (charinfo_size != (int)sizeof(FontFileCharInfo)) {
            DLOG("Character info size is wrong (%d, should be %zu)",
                 charinfo_size, sizeof(FontFileCharInfo));
            return 0;
        }
    } else {
        DLOG("Invalid version %u", header_in->version);
        return 0;
    }

    if (charinfo_offset < 0) {
        DLOG("Character info offset is negative (%d)", charinfo_offset);
        return 0;
    }
    if (charinfo_offset > size) {
        DLOG("Character info starts beyond end of file (%d > %d)",
             charinfo_offset, size);
        return 0;
    }
    const int32_t charinfo_bytesize = charinfo_count * charinfo_size;
    /* Note the format of this comparison!  This avoids misbehavior in the
     * case that adding charinfo_offset + charinfo_bytesize causes overflow. */
    if (charinfo_bytesize > size - charinfo_offset) {
        DLOG("Character info extends past end of file (%d + %d > %d)",
             charinfo_offset, charinfo_bytesize, size);
        return 0;
    }

    if (texture_offset < 0) {
        DLOG("Texture offset is negative (%d)", texture_offset);
        return 0;
    }
    if (texture_offset > size) {
        DLOG("Font texture data starts beyond end of file (%d > %d)",
             texture_offset, size);
        return 0;
    }
    if (texture_size > size - texture_offset) {
        DLOG("Font texture data extends past end of file (%d + %d > %d)",
             texture_offset, texture_size, size);
        return 0;
    }

    memcpy(header_ret->magic, FONT_FILE_MAGIC, sizeof(header_ret->magic));
    header_ret->version         = header_in->version;
    header_ret->height          = height;
    header_ret->baseline        = baseline;
    header_ret->charinfo_offset = charinfo_offset;
    header_ret->charinfo_count  = charinfo_count;
    header_ret->charinfo_size   = charinfo_size;
    header_ret->texture_offset  = texture_offset;
    header_ret->texture_size    = texture_size;
    return 1;
}

/*-----------------------------------------------------------------------*/

int font_parse_charinfo(const void *data, int count, int version,
                        FontFileCharInfo *charinfo_ret)
{
    if (UNLIKELY(!data) || UNLIKELY(!charinfo_ret)) {
        DLOG("Invalid parameters: %p %d %d %p", data, count, version,
             charinfo_ret);
        return 0;
    }
    if (UNLIKELY((uintptr_t)data % 4 != 0)) {
        DLOG("Input pointer %p is not 4-byte aligned", data);
        return 0;
    }
    if (UNLIKELY(version < 1 || version > FONT_FILE_VERSION)) {
        DLOG("Invalid data version: %d", version);
        return 0;
    }

    const FontFileCharInfo *charinfo_in = (const FontFileCharInfo *)data;

    for (int i = 0; i < count; i++) {
        const int32_t ch = be_to_s32(charinfo_in[i].ch);
        const int16_t x  = be_to_s16(charinfo_in[i].x);
        const int16_t y  = be_to_s16(charinfo_in[i].y);
        if (ch < 0) {
            DLOG("charinfo[%d]: Invalid character %d", i, ch);
            return 0;
        }
        if (x < 0 || y < 0) {
            DLOG("charinfo[%d] (U+%04X): Invalid coordinates %d,%d",
                 i, ch, x, y);
            return 0;
        }
    }

    for (int i = 0; i < count; i++) {
        const int32_t ch       = be_to_s32(charinfo_in[i].ch);
        const int16_t x        = be_to_s16(charinfo_in[i].x);
        const int16_t y        = be_to_s16(charinfo_in[i].y);
        const uint8_t w        = charinfo_in[i].w;
        const uint8_t h        = charinfo_in[i].h;
        const int8_t ascent    = charinfo_in[i].ascent;
        const int16_t prekern  = be_to_s16(charinfo_in[i].prekern);
        const int16_t postkern = be_to_s16(charinfo_in[i].postkern);

        charinfo_ret[i].ch       = ch;
        charinfo_ret[i].x        = x;
        charinfo_ret[i].y        = y;
        charinfo_ret[i].w        = w;
        charinfo_ret[i].h        = h;
        charinfo_ret[i].ascent   = ascent;
        charinfo_ret[i].prekern  = prekern;
        charinfo_ret[i].postkern = postkern;
    }

    return 1;
}

/*************************************************************************/
/*************************************************************************/
