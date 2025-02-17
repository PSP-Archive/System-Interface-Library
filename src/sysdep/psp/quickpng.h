/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/quickpng.h: Header for quickpng.c PNG generator.
 */

#ifndef SIL_SRC_SYSDEP_PSP_QUICKPNG_H
#define SIL_SRC_SYSDEP_PSP_QUICKPNG_H

/*************************************************************************/
/*************************************************************************/

/**
 * quickpng_from_rgb32:  Generate a PNG file from a 32-bit-per-pixel RGB
 * image (with 8 bits for each color component and 8 alpha or unused bits).
 *
 * Parameters:
 *       source_data: A pointer to the first byte of the image data.
 *             width: The width of the image, in pixels.
 *            height: The height of the image, in pixels.
 *            stride: The length of a pixel row's data, in pixels.
 *     output_buffer: A pointer to the first byte of the buffer into which
 *                       the generated PNG data will be stored.
 *       output_size: The number of bytes available in the output buffer.
 *         bgr_order: Nonzero if the pixel data is in BGR format; zero if
 *                       in RGB format.
 *       alpha_first: Nonzero if the alpha or unused byte of the pixel data
 *                       precedes the color bytes, zero if it follows the
 *                       color bytes.
 *         use_alpha: Nonzero to include the image's alpha channel in the
 *                       PNG file, zero to create an RGB-only PNG file.
 * Return value:
 *     The resultant PNG data length (nonzero, in bytes) on success, zero
 *     on failure.
 * Preconditions:
 *     If source_data is not NULL and both stride and height are positive,
 *        source_data points to at least (stride * height * 4) bytes of
 *        readable memory.
 *     If output_buffer is not NULL and output_size is positive,
 *        output_buffer points to at least output_size bytes of writable
 *        memory.
 * Postconditions:
 *     The function succeeds iff all of the following are true on entry:
 *        - source_data != NULL
 *        - width > 0 && width <= 10000
 *        - height > 0 && height <= 10000
 *        - stride >= width
 *        - output_buffer != NULL
 *        - output_size >= quickpng_rgb32_size(width, height)
 */
extern long quickpng_from_rgb32(const void *source_data,
                                int width, int height, int stride,
                                void *output_buffer, long output_size,
                                int bgr_order, int alpha_first, int use_alpha);

/**
 * quickpng_rgb32_size:  Return the size of the buffer necessary to store
 * the PNG data generated by quickpng_from_rgb32() for the specified image
 * size.
 *
 * It is an error to specify a nonpositive value or a value greater than
 * 10,000 for either width or height.
 *
 * Parameters:
 *         width: The width of the image, in pixels.
 *        height: The height of the image, in pixels.
 *     use_alpha: Whether or not to include an alpha channel.  (Use the same
 *                   value you intend to pass to quickpng_from_rgb32().)
 * Return value:
 *     The number of bytes required to hold the resulting PNG data, or an
 *     unspecified negative value on error.
 * Postconditions:
 *     If both width and height are within the range [1,10000], then the
 *     return value is positive.
 */
extern long quickpng_rgb32_size(int width, int height, int use_alpha);

/**
 * quickpng_append_chunk:  Append an arbitrary chunk to a PNG file,
 * inserting the chunk immediately before the trailing IEND chunk.  Fails
 * if there is not enough space in the PNG data buffer to insert the chunk
 * (there must be at least 12 bytes free in addition to the space required
 * by the chunk data itself).
 *
 * If the chunk to append will not contain any data (i.e., chunk_len == 0),
 * then chunk_data may be specified as NULL.
 *
 * Parameters:
 *          chunk_type: Pointer to a buffer containing the 4-byte chunk type.
 *          chunk_data: Pointer to the data for the chunk to append.
 *           chunk_len: Length of the chunk data, in bytes.
 *            png_data: Pointer to the PNG data buffer.
 *             png_len: Length of the current PNG data, in bytes.
 *     png_buffer_size: Size of the PNG data buffer, in bytes.
 * Return value:
 *     The resultant PNG data length (nonzero, in bytes) on success, zero
 *     on failure.
 * Preconditions:
 *     If chunk_type is not NULL, chunk_type points to at least 4 bytes of
 *        readable memory.
 *     If chunk_data is not NULL and chunk_len is positive, chunk_data
 *        points to at least chunk_len bytes of readable memory.
 *     If chunk_len is positive, its value can be represented in 32 bits.
 *     If png_data is not NULL and png_buffer_size is positive, png_data
 *        points to at least png_buffer_size bytes of readable and writable
 *        memory.
 */
extern long quickpng_append_chunk(const void *chunk_type,
                                  const void *chunk_data, long chunk_len,
                                  void *png_data, long png_len,
                                  long png_buffer_size);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_PSP_QUICKPNG_H
