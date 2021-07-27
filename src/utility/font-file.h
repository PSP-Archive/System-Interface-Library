/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/utility/font-file.h: Header for the SIL bitmap font file format.
 */

#ifndef SIL_SRC_UTILITY_FONT_FILE_H
#define SIL_SRC_UTILITY_FONT_FILE_H

EXTERN_C_BEGIN

/*************************************************************************/
/*************************************************************************/

/*
 * File header for bitmap font files.  All integer values are stored in
 * big-endian format.
 */

typedef struct FontFileHeader FontFileHeader;
struct FontFileHeader {
    char magic[4];              // File identifier (FONT_FILE_MAGIC).
    uint8_t version;            // Version identifier (FONT_FILE_VERSION).
    uint8_t height;             // Native font height, in pixels.
    uint8_t baseline;           // Font baseline position, in pixels
                                //    (0 = top edge of character box).
    uint8_t pad;
    int32_t charinfo_offset;    // File offset to character information.
    uint16_t charinfo_count;    // Number of character information entries.
                                //    Unsigned to allow >32767 entries.
    int16_t charinfo_size;      // Size of one character information entry
                                //    (must be == sizeof(FontFileCharInfo)).
    int32_t texture_offset;     // File offset to font texture data (may be
                                //    any supported data format).
    int32_t texture_size;       // Size of font texture data.
};

#define FONT_FILE_MAGIC    "FONT"
#define FONT_FILE_VERSION  1

/*
 * Data structure for information about a single character (glyph) in the
 * font.  FontFileHeader.charinfo_offset points to an array of these.
 */

typedef struct FontFileCharInfo FontFileCharInfo;
struct FontFileCharInfo {
    int32_t ch;         // Unicode codepoint of this character.
    int16_t x, y;       // Upper-left corner of the glyph, in pixels (with the
                        //    origin at the upper-left corner of the texture).
    uint8_t w, h;       // Size of the glyph, in pixels.
    int8_t ascent;      // Height above the baseline (in pixels) at which to
                        //    draw the upper-left pixel of the glyph.
    uint8_t pad;
    int16_t prekern;    // Offset to add to the current X coordinate before
                        //    drawing, in 256ths of a pixel.
    int16_t postkern;   // Offset to add to the current X coordinate after
                        //    drawing, in 256ths of a pixel (excluding the
                        //    width of the glyph itself).
};

/*-----------------------------------------------------------------------*/

/**
 * font_parse_header:  Parse the given data into a FontFileHeader structure.
 *
 * On success, the function guarantees that all subsidiary data structures
 * are fully contained within the specified data size; in other words, the
 * following relations hold:
 *    header_ret->charinfo_offset + (header_ret->charinfo_count
 *                                   * header_ret->charinfo_size) <= size
 *    header_ret->texture_offset + header_ret->texture_size <= size
 * The function also guarantees that header_ret->version is a recognized
 * format version number and that header_ret->charinfo_size is the correct
 * value for the indicated format version.  On failure, *header_ret will be
 * unchanged.
 *
 * header_ret may point to the same memory buffer as data.
 *
 * [Parameters]
 *     data: Data to parse (must be 4-byte aligned).
 *     size: Size of data, in bytes.
 *     header_ret: Pointer to FontFileHeader buffer in which to store result.
 * [Return value]
 *     True on success, false on failure.
 */
extern int font_parse_header(const void *data, int size,
                             FontFileHeader *header_ret);

/**
 * font_parse_charinfo:  Parse the given data into a FontFileCharInfo array.
 *
 * On success, the function guarantees that all character values
 * (charinfo_ret[].ch) are nonnegative.  On failure, the array pointed to
 * by charinfo_ret will be unchanged.
 *
 * charinfo_ret may point to the same memory address as data (to parse the
 * data in place) if the input data version is FONT_FILE_VERSION; otherwise,
 * the charinfo_ret buffer should not overlap the input data.
 *
 * [Parameters]
 *     data: Data to parse (must be 4-byte aligned).
 *     count: Number of character entries.
 *     version: File data version (from FontFileHeader.version).
 *     charinfo_ret: Pointer to FontFileHeader buffer in which to store result.
 * [Return value]
 *     True on success, false on failure.
 */
extern int font_parse_charinfo(const void *data, int count, int version,
                               FontFileCharInfo *charinfo_ret);

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_SRC_UTILITY_FONT_FILE_H
