/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/utility/png.h: Header for PNG file manipulation functions.
 */

#ifndef SIL_UTILITY_PNG_H
#define SIL_UTILITY_PNG_H

EXTERN_C_BEGIN

/*************************************************************************/
/*************************************************************************/

/**
 * png_parse:  Parse a PNG file and return its pixel data in RGBA format
 * (8-bit unsigned values in R, G, B, A order, with no padding between
 * pixel rows).  The pixel buffer is allocated with mem_alloc(), passing
 * the given mem_flags (but ignoring MEM_ALLOC_CLEAR).
 *
 * This function supports most common PNG image formats, but does _not_
 * support interlaced images or images with a bit depth greater than 8.
 *
 * If PNG support is not built into the executable, this function will
 * always fail.
 *
 * [Parameters]
 *     data: PNG file data.
 *     size: PNG file size, in bytes.
 *     mem_flags: Memory allocation flags (MEM_ALLOC_*).
 *     width_ret: Pointer to variable to receive image width, in pixels.
 *     height_ret: Pointer to variable to receive image height, in pixels.
 * [Return value]
 *     Newly-allocated buffer containing pixel data, or NULL on error.
 */
extern uint8_t *png_parse(const void *data, uint32_t size, int mem_flags,
                          int *width_ret, int *height_ret);

/**
 * png_create:  Generate a PNG file from the given RGBA pixel data (in the
 * same format as that returned by png_parse()).  The returned data buffer
 * is allocated with mem_alloc(), passing the given mem_flags (but ignoring
 * MEM_ALLOC_CLEAR).
 *
 * If PNG support is not built into the executable, this function will
 * always fail.
 *
 * [Parameters]
 *     pixels: Pixel data for image.
 *     width: Image width, in pixels.
 *     height: Image height, in pixels.
 *     keep_alpha: True to save the alpha channel in the pixel data, false
 *         to discard it and create an opaque image.
 *     comp_level: Compression level (0-9 where 0 = no compression and 9 =
 *         best compression, or -1 for the zlib default).
 *     flush_interval: Interval in rows at which to flush the compressed
 *         output stream, or zero to never flush.  Set to zero for the best
 *         compression.
 *     mem_flags: Memory allocation flags (MEM_ALLOC_*).
 *     size_ret: Pointer to variable to receive PNG file size, in bytes.
 * [Return value]
 *     Newly-allocated buffer containing PNG file data, or NULL on error.
 */
extern void *png_create(const void *pixels, int width, int height,
                        int keep_alpha, int comp_level, int flush_interval,
                        int mem_flags, uint32_t *size_ret);

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_UTILITY_PNG_H
