/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/utility/tex-file.h: Header for the SIL custom texture file format.
 */

#ifndef SIL_SRC_UTILITY_TEX_FILE_H
#define SIL_SRC_UTILITY_TEX_FILE_H

EXTERN_C_BEGIN

/*************************************************************************/
/*************************************************************************/

/*
 * File header for custom-format texture files.  All integer values are
 * stored in big-endian format, and the floating-point "scale" value is
 * stored as a 16.16 fixed-point integer.  For indexed-color data
 * (TEX_FORMAT_PALETTE8_RGBA8888), the palette color values precede the
 * actual image data, and are counted as part of the pixel data for the
 * pixels_{offset,size} fields.
 */

typedef struct TexFileHeader TexFileHeader;
struct TexFileHeader {
    char magic[4];          // File identifier (TEX_FILE_MAGIC).
    uint8_t version;        // Version identifier (TEX_FILE_VERSION).
    uint8_t format;         // Pixel format (TEX_FORMAT_* from texture.h).
    uint8_t mipmaps;        // Number of mipmap levels, _not_ including
                            //    primary texture data; mipmaps are only
                            //    valid for textures with power-of-2 sizes.
    uint8_t opaque_bitmap;  // True if an opaque bitmap is present after
                            //    the texture data.  The bitmap is stored
                            //    with pixel 0 of each row in bit 0 (LSB),
                            //    pixel 1 in bit 1, etc.; rows are padded
                            //    to a multiple of 8 bits if needed.
    int16_t width, height;  // Texture size (pixels).  Each mipmap level
                            //    has half the width and height of the
                            //    preceding level, rounded down.
    union {
        float scale;        // Texture scale factor.
        int32_t scale_int;  // Same, as a 16.16 fixed-point integer (in files).
    };
    int32_t pixels_offset;  // Offset to pixel data.  Mipmaps are stored
                            //    in order immediately following the base
                            //    image data, with no padding.
    int32_t pixels_size;    // Size of pixel data, in bytes.
    int32_t bitmap_offset;  // Offset to opaque bitmap data (0 if not present).
    int32_t bitmap_size;    // Size of opaque bitmap data (0 if not present).
};

#define TEX_FILE_MAGIC    "TEX\012"
#define TEX_FILE_VERSION  2

/*-----------------------------------------------------------------------*/

/**
 * tex_parse_header:  Parse the given data into a TexFileHeader structure.
 *
 * On success, the function guarantees that header_ret->pixels_offset and
 * header_ret->bitmap_offset point to regions which fit within the
 * specified data size, and that if an opaque bitmap is present, its size
 * matches the texture size; in other words, the following relations hold:
 *    header_ret->pixels_offset + header_ret->pixels_size <= size
 *    header_ret->bitmap_offset + header_ret->bitmap_size <= size
 *    if opaque_bitmap != 0: bitmap_size == ((width+7)/8) * height
 * On failure, *header_ret will be unchanged.
 *
 * header_ret may point to the same memory buffer as data.
 *
 * [Parameters]
 *     data: Data to parse (must be 4-byte aligned).
 *     size: Size of data, in bytes.
 *     header_ret: Pointer to TexFileHeader buffer in which to store result.
 * [Return value]
 *     True on success, false on failure.
 */
extern int tex_parse_header(const void *data, int size,
                            TexFileHeader *header_ret);

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_SRC_UTILITY_TEX_FILE_H
