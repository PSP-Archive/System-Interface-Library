/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/utility/compress.h: Header for data compression/decompression
 * functions.
 */

/*
 * Note that the one-step compression and decompression routines provided
 * here (compress(), decompress(), and decompress_to()) are not thread-safe.
 * For thread-safe decompression, use the decompress_partial() interface;
 * there is currently no interface for thread-safe compression.
 */

#ifndef SIL_UTILITY_COMPRESS_H
#define SIL_UTILITY_COMPRESS_H

EXTERN_C_BEGIN

/*************************************************************************/
/*************************************************************************/

/**
 * compress:  Compress a data buffer using the "deflate" algorithm,
 * returning a newly allocated (using mem_alloc()) buffer containing the
 * compressed data.  The buffer should be freed with mem_free() when no
 * longer needed.
 *
 * This function will fail if the zlib library has not been linked in.
 *
 * [Parameters]
 *     in_data: Input data buffer.
 *     in_size: Input data buffer size, in bytes.
 *     out_size_ret: Pointer to variable to receive output data size, in bytes.
 *     mem_flags: Memory allocation flags (MEM_ALLOC_*).
 *     level: Compression level (0-9: 0=uncompressed, 1=fast, 9=best, or -1
 *         for the zlib default).
 * [Return value]
 *     Newly allocated buffer containing the compressed data, or NULL on error.
 */
extern void *compress(const void *in_data, int32_t in_size,
                      int32_t *out_size_ret, int mem_flags, int level);

/**
 * decompress:  Decompress a data buffer compressed with the "deflate"
 * algorithm, returning a newly allocated (using mem_alloc()) buffer
 * containing the decompressed data.  The buffer should be freed with
 * mem_free() when no longer needed.
 *
 * If the zlib library has been linked in, this and the other decompression
 * functions will use it; otherwise, they will use the slower but less
 * memory-hungry tinflate library.
 *
 * [Parameters]
 *     in_data: Input data buffer.
 *     in_size: Input data buffer size, in bytes.
 *     out_size_ret: Pointer to variable to receive output data size, in bytes.
 *     mem_flags: Memory allocation flags (MEM_ALLOC_*).
 * [Return value]
 *     Newly allocated buffer containing the decompressed data, or NULL on
 *     error.
 */
extern void *decompress(const void *in_data, int32_t in_size,
                        int32_t *out_size_ret, int mem_flags);

/**
 * decompress_to:  Decompress a data buffer compressed with the "deflate"
 * algorithm, storing the decompressed data in the specified buffer.
 *
 * [Parameters]
 *     in_data: Input data buffer.
 *     in_size: Input data buffer size, in bytes.
 *     out_buffer: Output data buffer.
 *     out_bufsize: Output data buffer size, in bytes.
 *     out_size_ret: Pointer to variable to receive output data size, in
 *         bytes.  May be NULL if this value is not needed.
 * [Return value]
 *     True on success, false on buffer overflow or other error.
 */
extern int decompress_to(const void *in_data, int32_t in_size,
                         void *out_buffer, int32_t out_bufsize,
                         int32_t *out_size_ret);

/**
 * decompress_create_state:  Create a state block for decompress_partial().
 *
 * [Return value]
 *     New state block pointer, or NULL on error.
 */
extern void *decompress_create_state(void);

/**
 * decompress_partial:  Decompress part of a data stream compressed with
 * the "deflate" algorithm, storing the decompressed data in the specified
 * buffer.  A separate state block must be provided for each distinct
 * stream to be decompressed.
 *
 * This routine may also be used for one-step decompression when there is
 * a possibility that another thread may be calling decompress() or
 * decompress_to(), since this routine is thread-safe.
 *
 * *out_size_ret is set after each non-error return to indicate the amount
 * of decompressed data output so far.  Note that depending on the size and
 * content of the input data block passed to this routine, a successful
 * return might not add any bytes to the output.
 *
 * [Parameters]
 *     state: State block.
 *     in_data: Input data buffer.
 *     in_size: Input data buffer size, in bytes.
 *     out_buffer: Output data buffer.
 *     out_bufsize: Output data buffer size, in bytes.
 *     out_size_ret: Pointer to variable to receive output data size, in bytes.
 * [Return value]
 *     +1 on successful completion; -1 if no errors have occurred but the
 *     data stream is incomplete; 0 on buffer overflow or other error.
 */
extern int decompress_partial(void *state,
                              const void *in_data, int32_t in_size,
                              void *out_buffer, int32_t out_bufsize,
                              int32_t *out_size_ret);

/**
 * decompress_destroy_state:  Destroy a state block created with
 * decompress_create_state().  Does nothing if state == NULL.
 *
 * [Parameters]
 *     state: State block to destroy.
 */
extern void decompress_destroy_state(void *state);

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_UTILITY_COMPRESS_H
