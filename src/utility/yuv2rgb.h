/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/utility/yuv2rgb.h: Header for YUV/RGB colorspace conversion.
 */

#ifndef SIL_SRC_UTILITY_YUV2RGB_H
#define SIL_SRC_UTILITY_YUV2RGB_H

EXTERN_C_BEGIN

/*************************************************************************/
/*************************************************************************/

/**
 * yuv2rgb:  Convert YUV-colorspace video data to 32bpp RGBA image data.
 *
 * [Parameters]
 *     src: Pointers to input Y, U, and V planes.
 *     src_stride: Line lengths of each source plane, in pixels.
 *     dest: Buffer in which to store RGBA output data.
 *     dest_stride: Line length of output buffer, in pixels.
 *     width: Width of image, in pixels.
 *     height: Height of image, in pixels.
 *     smooth_uv: True to linearly interpolate U and V across the Y plane
 *         (slower); false to use nearest-point U/V sampling (faster).
 *         Interpolation assumes MPEG-2 chroma sampling locations: cosited
 *         with the first of each two luma samples horizontally and sited
 *         between each two luma samples vertically.
 */
extern void yuv2rgb(const uint8_t **src, const int *src_stride,
                    uint8_t *dest, const int dest_stride,
                    const int width, const int height, int smooth_uv);

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_SRC_UTILITY_YUV2RGB_H
