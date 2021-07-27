/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/utility/pixformat.h: Header for pixel format conversion routines.
 */

#ifndef SIL_SRC_UTILITY_PIXFORMAT_H
#define SIL_SRC_UTILITY_PIXFORMAT_H

EXTERN_C_BEGIN

/*************************************************************************/
/*************************************************************************/

/**
 * PixelConvertFunc, pixel_convert_*:  Function type and individual
 * functions for converting between different pixel formats.  All functions
 * below have the same signature, so that a caller can assign any of them
 * to a PixelConvertFunc* pointer and safely call it through that pointer.
 *
 * The actual conversion functions are named "pixel_convert_<from>_<to>",
 * where <from> and <to> are the source and destination pixel formats.
 * Formats are described as the list of components followed by the list of
 * bit widths; "8888" formats are ordered from low to high memory addresses,
 * while other formats are ordered from LSB to MSB of the overall pixel
 * value.  For example, "rgba8888" refers to a pixel containing 1-byte
 * R, G, B, and A values with R at the lowest byte address, and "bgra5551"
 * refers to a 16-bit pixel unit with the following bit layout:
 *
 *     MSB -------------------------- LSB
 *     A R R R  R R G G  G G G B  B B B B
 *
 * Behavior is undefined if the source and destination buffers overlap;
 * however, src == dest is permitted when the pixel sizes of both source
 * and destination formats are equal (both 32 bits, for example).
 *
 * [Parameters]
 *     dest: Output data pointer.  Must be aligned to a pixel unit.
 *     src: Input data pointer.  Must be aligned to a pixel unit.
 *     len: Number of pixels to process.
 */
typedef void PixelConvertFunc(void *dest, const void *src, size_t len);
extern void pixel_convert_bgra8888_rgba8888(void *dest, const void *src,
                                            size_t len);
extern void pixel_convert_rgb565_bgr565(void *dest, const void *src,
                                        size_t len);
extern void pixel_convert_rgba5551_abgr1555(void *dest, const void *src,
                                               size_t len);
extern void pixel_convert_bgra5551_abgr1555(void *dest, const void *src,
                                               size_t len);
extern void pixel_convert_rgba5551_bgra5551(void *dest, const void *src,
                                               size_t len);
extern void pixel_convert_rgba4444_abgr4444(void *dest, const void *src,
                                               size_t len);
extern void pixel_convert_bgra4444_abgr4444(void *dest, const void *src,
                                               size_t len);
extern void pixel_convert_rgba4444_bgra4444(void *dest_, const void *src_,
                                            size_t len);
extern void pixel_convert_rgb565_rgba8888(void *dest, const void *src,
                                          size_t len);
extern void pixel_convert_bgr565_rgba8888(void *dest, const void *src,
                                          size_t len);
extern void pixel_convert_rgba5551_rgba8888(void *dest, const void *src,
                                               size_t len);
extern void pixel_convert_bgra5551_rgba8888(void *dest, const void *src,
                                               size_t len);
extern void pixel_convert_rgba4444_rgba8888(void *dest, const void *src,
                                               size_t len);
extern void pixel_convert_bgra4444_rgba8888(void *dest, const void *src,
                                               size_t len);
extern void pixel_convert_l8_rgba8888(void *dest, const void *src, size_t len);
extern void pixel_convert_a8_rgba8888(void *dest, const void *src, size_t len);

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_SRC_UTILITY_PIXFORMAT_H
