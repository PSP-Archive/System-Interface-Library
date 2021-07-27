/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/utility/zlib.h: Header for zlib compression functions.
 */

/*
 * Note that the one-step compression and decompression routines provided
 * here (zlib_compress(), zlib_decompress(), and zlib_decompress_to()) are
 * not multithread-safe.  For multithread-safe decompression, use the
 * zlib_decompress_partial() interface; there is currently no interface
 * for multithread-safe compression.
 */

#ifndef SIL_SRC_UTILITY_ZLIB_H
#define SIL_SRC_UTILITY_ZLIB_H

#ifdef SIL_UTILITY_INCLUDE_ZLIB  // To the end of the file.

EXTERN_C_BEGIN

/*************************************************************************/
/*************************************************************************/

/**
 * zlib_compress:  Compress a data buffer using zlib, returning a newly
 * allocated buffer (using mem_alloc()) containing the compressed data.
 * The buffer should be freed with mem_free() when no longer needed.
 *
 * [Parameters]
 *     in_data: Input data buffer.
 *     in_size: Input data buffer size, in bytes.
 *     out_size_ret: Pointer to variable to receive output data size, in bytes.
 *     mem_flags: Memory allocation flags (MEM_ALLOC_*).
 *     level: Compression level (0-9: 0=uncompressed, 1=fast, 9=best, or -1
 *         for the zlib default.
 * [Return value]
 *     Newly allocated buffer containing the compressed data, or NULL on error.
 */
extern void *zlib_compress(const void *in_data, int32_t in_size,
                           int32_t *out_size_ret, int mem_flags, int level);

/**
 * zlib_decompress:  Decompress a zlib-compressed data buffer using zlib,
 * returning a newly allocated buffer (using mem_alloc()) containing the
 * decompressed data.  The buffer should be freed with mem_free() when no
 * longer needed.
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
extern void *zlib_decompress(const void *in_data, int32_t in_size,
                             int32_t *out_size_ret, int mem_flags);

/**
 * zlib_decompress_to:  Decompress a zlib-compressed data buffer using
 * zlib, storing the decompressed data in the specified buffer.
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
extern int zlib_decompress_to(const void *in_data, int32_t in_size,
                              void *out_buffer, int32_t out_bufsize,
                              int32_t *out_size_ret);

/**
 * zlib_create_state:  Create a state block for zlib_decompress_partial().
 *
 * [Return value]
 *     State block pointer, or NULL on error.
 */
extern void *zlib_create_state(void);

/**
 * zlib_decompress_partial:  Decompress part of a zlib-compressed data
 * stream, storing the decompressed data in the specified buffer.  A state
 * block must be provided for each distinct stream to be decompressed.
 *
 * This routine may also be used for one-step decompression when there is
 * a possibility that another thread may be calling zlib_decompress(),
 * since this routine is multithread-safe.
 *
 * *out_size_ret is set after each non-error return to indicate the amount
 * of decompressed data output so far.  Note that depending on the size of
 * the input data blocks passed to this routine, a successful return may
 * not add any bytes to the output.
 *
 * [Parameters]
 *     state: State block.
 *     in_data: Input data buffer.
 *     in_size: Input data buffer size, in bytes.
 *     out_buffer: Output data buffer.
 *     out_bufsize: Output data buffer size, in bytes.
 *     out_size_ret: Pointer to variable to receive output data size, in bytes.
 * [Return value]
 *     Positive on successful completion; negative if no errors have
 *     occurred but the data stream is incomplete; zero on buffer overflow
 *     or other error.
 */
extern int zlib_decompress_partial(void *state,
                                   const void *in_data, int32_t in_size,
                                   void *out_buffer, int32_t out_bufsize,
                                   int32_t *out_size_ret);

/**
 * zlib_destroy_state:  Destroy a state block created with zlib_create_state().
 * Does nothing if state == NULL.
 *
 * [Parameters]
 *     state: State block to destroy.
 */
extern void zlib_destroy_state(void *state);

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_UTILITY_INCLUDE_ZLIB
#endif  // SIL_SRC_UTILITY_ZLIB_H
