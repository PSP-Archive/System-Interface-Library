/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/utility/tinflate.h: Header for "deflate"-method decompression functions.
 */

#ifndef SIL_SRC_UTILITY_TINFLATE_H
#define SIL_SRC_UTILITY_TINFLATE_H

/*************************************************************************/
/*************************************************************************/

/**
 * tinflate_state_size:  Return the size of the state buffer required by
 * tinflate_partial().
 *
 * Parameters:
 *     None.
 * Return value:
 *     The number of bytes required by the tinflate_partial() state buffer.
 */
extern int tinflate_state_size(void);

/**
 * tinflate:  Decompress a complete stream of data compressed with the
 * "deflate" algorithm.
 *
 * Parameters:
 *     compressed_data: Pointer to the compressed data.
 *     compressed_size: Number of bytes of compressed data.
 *       output_buffer: Pointer to the buffer to receive uncompressed data.
 *         output_size: Size of the output buffer, in bytes.
 *             crc_ret: Pointer to variable to receive the CRC-32 of the
 *                         uncompressed data, or NULL if the CRC is not
 *                         needed.
 * Return value:
 *     The number of bytes occupied by the decompressed data (a nonnegative
 *     value), regardless of the size of the output buffer, or an
 *     unspecified negative value if an error occurs.  (A full output
 *     buffer is not considered an error.)
 * Notes:
 *     The returned CRC value is only valid when the entire stream of
 *     decompressed data fits within the output buffer, i.e. when the
 *     function's return value is no greater than "output_size".
 */
extern long tinflate(const void *compressed_data, long compressed_size,
                     void *output_buffer, long output_size,
                     unsigned long *crc_ret);

/**
 * tinflate_partial:  Decompress a portion of a stream of data compressed
 * with the "deflate" algorithm.
 *
 * Parameters:
 *       compressed_data: Pointer to the portion of the compressed data to
 *                           process.
 *       compressed_size: Number of bytes of compressed data.
 *         output_buffer: Pointer to the buffer to receive uncompressed data.
 *           output_size: Size of the output buffer, in bytes.
 *              size_ret: Pointer to variable to receive the size of the
 *                           uncompressed data, or NULL if the size is not
 *                           needed.
 *               crc_ret: Pointer to variable to receive the CRC-32 of the
 *                           uncompressed data, or NULL if the CRC is not
 *                           needed.
 *          state_buffer: Pointer to a buffer to hold state information,
 *                           which must be zeroed before the first call for
 *                           the stream.
 *     state_buffer_size: Size of the state buffer, in bytes.  Must be no
 *                           less than the value returned by
 *                           tinflate_state_size().
 * Return value:
 *     Zero when the data has been completely decompressed; an unspecified
 *     positive value if the end of the input data is reached before
 *     decompression is complete; or an unspecified negative value if an
 *     error occurs.  (A full output buffer is not considered an error.)
 * Notes:
 *     The returned CRC value is only valid after the entire stream of data
 *     has been decompressed, and only when the decompressed data fits
 *     within the output buffer, i.e. when the value stored in *size_ret is
 *     no greater than the value of output_size.
 */
extern int tinflate_partial(const void *compressed_data, long compressed_size,
                            void *output_buffer, long output_size,
                            unsigned long *size_ret, unsigned long *crc_ret,
                            void *state_buffer, long state_buffer_size);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_UTILITY_TINFLATE_H
