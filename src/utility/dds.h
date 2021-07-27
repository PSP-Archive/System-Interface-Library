/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/utility/dds.h: Header for DDS texture file processing routines.
 */

#ifndef SIL_SRC_UTILITY_DDS_H
#define SIL_SRC_UTILITY_DDS_H

/*************************************************************************/
/*************************************************************************/

/*
 * Info structure for DDS files, used by dds_parse().  This structure
 * supports up to 16 mipmap levels (including the base texture data),
 * which should be plenty.
 */

typedef struct DDSInfo DDSInfo;
struct DDSInfo {
    int width, height;        // Size of texture.
    int stride;               // Line stride, in pixels.
    uint8_t format;           // Texture data format (TEX_FORMAT_*).
    uint8_t mipmaps;          // Number of additional mipmap levels.
    const uint8_t *data[16];  // Texture data per mipmap level.
};

/**
 * dds_get_info:  Parse a DDS-format file and return information about it.
 *
 * On success, the function guarantees that all fields have valid data.
 * In particular:
 *    - dds_ret->width and dds_ret->height will be greater than zero.
 *    - dds_ret->stride will be zero if dds_ret->format is a compressed
 *      format, and nonnegative otherwise.
 *    - dds_ret->data[0] through dds_ret->data[dds_ret->mipmaps] will
 *      point to regions which fit within the input data given the size of
 *      each mipmap level's data, and remaining dds_ret->data[] pointers
 *      will be set to NULL.
 *
 * On failure, *dds_ret will be unchanged.
 *
 * [Parameters]
 *     data: Data to parse.
 *     size: Size of data, in bytes.
 *     dds_ret: Pointer to DDSInfo buffer in which to store result.
 * [Return value]
 *     True on success, false on failure.
 */
extern int dds_get_info(const void *data, uint32_t size, DDSInfo *dds_ret);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_UTILITY_DDS_H
