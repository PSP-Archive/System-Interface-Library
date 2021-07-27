/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * tools/quantize.h: Header for color quantization routine.
 */

#ifndef SIL_TOOLS_QUANTIZE_H
#define SIL_TOOLS_QUANTIZE_H

/*************************************************************************/
/*************************************************************************/

/**
 * quantize_image:  Quantize a 32bpp image down to 8bpp.  Dithering is not
 * applied.
 *
 * The palette buffer ("palette" parameter) must be supplied, but the
 * colors themselves can be chosen automatically.  The caller may opt to
 * force certain colors to be included in the palette; in this case, the
 * fixed_colors parameter should be greater than zero, and the colors to
 * include should be stored in palette[0...fixed_colors-1].
 *
 * src and dest may point to the same location, but they may not otherwise
 * overlap.
 *
 * Note that this function allocates a temporary buffer of size
 * width*height*8 bytes.
 *
 * [Parameters]
 *     src: 32bpp image data.
 *     src_stride: Line stride for src, in pixels.
 *     dest: Output buffer for 8bpp image data.
 *     dest_stride: Line stride for dest, in pixels.
 *     width, height: Image size, in pixels.
 *     palette: Array buffer for generated palette colors.
 *     fixed_colors: Number of colors in palette[] which are preset.
 * [Return value]
 *     True on success, false on error.
 */
extern int quantize_image(const uint32_t *src, const int32_t src_stride,
                          uint8_t *dest, const int32_t dest_stride,
                          const int32_t width, const int32_t height,
                          uint32_t *palette, const unsigned int fixed_colors);

/**
 * generate_palette:  Generate an optimal 256-color palette for the given
 * image data.  Uses the median cut algorthm described in Paul Heckbert's
 * "Color Image Quantization for Frame Buffer Display", except that alpha
 * values are taken into account when measuring color distance.
 *
 * If callback is not NULL, it will be called at approximately 1-second
 * intervals until this function returns.
 *
 * [Parameters]
 *     imageptr: Image data (must be in 0xAARRGGBB or 0xAABBGGRR format).
 *     width, height: Image size, in pixels.
 *     stride: Image line stride, in pixels.
 *     palette: Array buffer for generated palette colors.
 *     fixed_colors: Number of colors in palette[] which are preset.
 *     callback: Callback function to be called periodically (may be NULL).
 */
extern void generate_palette(const uint32_t *imageptr, uint32_t width,
                             uint32_t height, uint32_t stride,
                             uint32_t *palette, unsigned int fixed_colors,
                             void (*callback)(void));

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_TOOLS_QUANTIZE_H
