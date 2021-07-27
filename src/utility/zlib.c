/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/utility/zlib.c: Functions for compressing and decompressing data
 * using the zlib library.
 */

#ifdef SIL_UTILITY_INCLUDE_ZLIB  // To the end of the file.

#include "src/base.h"
#include "src/memory.h"
#include "src/utility/zlib.h"

#include <zlib.h>

/*************************************************************************/
/********************** Callback routines for zlib ***********************/
/*************************************************************************/

/* Memory allocation routine. */
static void *zalloc(UNUSED voidpf opaque, uInt items, uInt size) {
    return mem_alloc((size_t)items * (size_t)size, 0, MEM_ALLOC_TEMP);
}

/* Memory freeing routine. */
static void zfree(UNUSED voidpf opaque, voidpf address) {
    mem_free(address);
}

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

void *zlib_compress(const void *in_data, int32_t in_size,
                    int32_t *out_size_ret, int mem_flags, int level)
{
    PRECOND(in_data != NULL, return NULL);
    PRECOND(out_size_ret != NULL, return NULL);
    PRECOND(level >= -1 && level <= 9, level = bound(level,-1,9));
    mem_flags &= ~MEM_ALLOC_CLEAR;
    if (level < 0) {
        level = Z_DEFAULT_COMPRESSION;
    }

    /* Allocate an initial buffer for compression.  We use the size of the
     * input data as the initial output buffer size; in most cases this
     * will be more than enough, but we can expand it later if necessary. */

    int32_t out_size = (in_size < 1 ? 1 : in_size);
    void *out_data = mem_alloc(out_size, 0, mem_flags);
    if (!out_data) {
        DLOG("Out of memory for compressed data buffer (unable to allocate"
             " %d bytes)", out_size);
        goto fail;
    }

    /* Initialize the zlib compression state block. */

    z_stream deflater = {
        .next_in = in_data,
        .avail_in = in_size,
        .next_out = out_data,
        .avail_out = out_size,
        .zalloc = zalloc,
        .zfree = zfree,
        .opaque = Z_NULL
    };

    if (deflateInit(&deflater, level) != Z_OK) {
        DLOG("deflateInit() failed: %s", deflater.msg);
        goto fail;
    }

    /* Perform the actual compression, looping until zlib reports that
     * compression is complete. */

    int result;
    while ((result = deflate(&deflater, Z_FINISH)) != Z_STREAM_END) {

        /* Z_STREAM_ERROR only occurs on an invalid state structure, and
         * Z_BUF_ERROR only occurs if we try calling deflate() without
         * giving it anything to do.  Neither can happen without a bug in
         * this routine. */
        ASSERT(result == Z_OK, goto fail_close_deflater);

        /* Output buffer is full, so expand it.  We default to doubling
         * the output buffer size up to 64k and increasing 64k at a time
         * thereafter, but fall back to smaller increments if necessary. */
        int32_t size_to_add;
        if (out_size < 0x10000) {
            size_to_add = out_size;
        } else {
            size_to_add = 0x10000;
        }
        /* Watch out for overflow! */
        ASSERT((uint32_t)out_size + (uint32_t)size_to_add <= 0x7FFFFFFFU,
               goto fail_close_deflater);
        void *new_out;
        while (!(new_out = mem_realloc(out_data, out_size + size_to_add,
                                       mem_flags))) {
            size_to_add /= 2;
            if (size_to_add == 0) {
                DLOG("Out of memory expanding compressed data buffer from"
                     " %d bytes", out_size);
                goto fail_close_deflater;
            }
        }
        out_data = new_out;
        out_size += size_to_add;
        deflater.next_out = (Byte *)out_data + deflater.total_out;
        deflater.avail_out += size_to_add;

    }  // while ((result = deflate(&deflater, Z_FINISH)) != Z_STREAM_END)

    /* Success!  Free zlib resources, and shrink the output buffer (if
     * possible) before returning. */

    deflateEnd(&deflater);

    void *shrink = mem_realloc(out_data, deflater.total_out, mem_flags);
    if (shrink) {  // Shrinking should always succeed, but check just in case.
        out_data = shrink;
    }

    *out_size_ret = deflater.total_out;
    return out_data;

    /* Error cases jump down here. */

  fail_close_deflater:
    deflateEnd(&deflater);
  fail:
    mem_free(out_data);
    return 0;
}

/*-----------------------------------------------------------------------*/

void *zlib_decompress(const void *in_data, int32_t in_size,
                      int32_t *out_size_ret, int mem_flags)
{
    PRECOND(in_data != NULL, return NULL);
    PRECOND(out_size_ret != NULL, return NULL);
    mem_flags &= ~MEM_ALLOC_CLEAR;

    /* Allocate an initial buffer for decompression.  We use the size of
     * the input data as the initial output buffer size, and expand it as
     * necessary while decompressing. */

    int32_t out_size = (in_size < 1 ? 1 : in_size);
    void *out_data = mem_alloc(out_size, 0, mem_flags);
    if (!out_data) {
        DLOG("Out of memory for decompressed data buffer (unable to allocate"
             " %d bytes)", out_size);
        goto fail;
    }

    /* Initialize the zlib decompression state block. */

    z_stream inflater = {
        .next_in = in_data,
        .avail_in = in_size,
        .next_out = out_data,
        .avail_out = out_size,
        .zalloc = zalloc,
        .zfree = zfree,
        .opaque = Z_NULL
    };

    if (inflateInit(&inflater) != Z_OK) {
        DLOG("inflateInit() failed: %s", inflater.msg);
        goto fail;
    }

    /* Perform the actual decompression, looping until zlib reports that
     * decompression is complete. */

    int result;
    while ((result = inflate(&inflater, 0)) != Z_STREAM_END) {

        if (result == Z_OK && inflater.avail_out == 0) {

            /* Output buffer is full, so expand it.  As with
             * zlib_compress(), we default to doubling the output buffer
             * size up to 64k and adding 64k at a time thereafter, but
             * fall back to smaller increments if necessary. */
            int32_t size_to_add;
            if (out_size < 0x10000) {
                size_to_add = out_size;
            } else {
                size_to_add = 0x10000;
            }
            /* Watch out for overflow! */
            ASSERT((uint32_t)out_size + (uint32_t)size_to_add <= 0x7FFFFFFFU,
                   goto fail_close_inflater);
            void *new_out;
            while (!(new_out = mem_realloc(out_data, out_size + size_to_add,
                                           mem_flags))) {
                size_to_add /= 2;
                if (size_to_add == 0) {
                    DLOG("Out of memory expanding decompressed data buffer"
                         " from %d bytes", out_size);
                    goto fail_close_inflater;
                }
            }
            out_data = new_out;
            out_size += size_to_add;
            inflater.next_out = (Byte *)out_data + inflater.total_out;
            inflater.avail_out += size_to_add;

        } else {  // Some sort of error.

            if (result == Z_OK || result == Z_BUF_ERROR) {
                DLOG("Premature end of compressed data");
            } else {
                DLOG("inflate() failed: %s", inflater.msg);
            }
            goto fail_close_inflater;

        }
    }  // while ((result = inflate(&inflater, 0)) != Z_STREAM_END)

    /* Success!  Free zlib resources, and shrink the output buffer (if
     * possible) before returning. */

    inflateEnd(&inflater);

    void *shrink =
        mem_realloc(out_data, lbound(inflater.total_out, 1), mem_flags);
    if (shrink) {  // Shrinking should always succeed, but check just in case.
        out_data = shrink;
    }

    *out_size_ret = inflater.total_out;
    return out_data;

    /* Error cases jump down here. */

  fail_close_inflater:
    inflateEnd(&inflater);
  fail:
    mem_free(out_data);
    return NULL;
}

/*-----------------------------------------------------------------------*/

int zlib_decompress_to(const void *in_data, int32_t in_size,
                       void *out_buffer, int32_t out_bufsize,
                       int32_t *out_size_ret)
{
    PRECOND(in_data != NULL, return 0);
    PRECOND(out_buffer != NULL || out_bufsize == 0, return 0);

    /* Initialize the zlib decompression state block. */

    z_stream inflater = {
        .next_in = in_data,
        .avail_in = in_size,
        .next_out = out_buffer,
        .avail_out = out_bufsize,
        .zalloc = zalloc,
        .zfree = zfree,
        .opaque = Z_NULL
    };

    if (inflateInit(&inflater) != Z_OK) {
        DLOG("inflateInit() failed: %s", inflater.msg);
        return 0;
    }

    /* Perform the actual decompression.  Since we have a fixed output
     * buffer, we can do this in a single step. */

    const int result = inflate(&inflater, Z_FINISH);
    if (result != Z_STREAM_END) {
        if (result == Z_BUF_ERROR) {
            if (inflater.avail_in == 0) {
                if (inflater.avail_out == 0) {
                    DLOG("Premature end of compressed data (or output buffer"
                         " overflow)");
                } else {
                    DLOG("Premature end of compressed data");
                }
            } else {
                DLOG("Buffer overflow during decompression");
            }
        } else {
            DLOG("inflate() failed: %s", inflater.msg);
        }
        inflateEnd(&inflater);
        return 0;
    }

    if (out_size_ret) {
        *out_size_ret = inflater.total_out;
    }
    inflateEnd(&inflater);
    return 1;
}

/*-----------------------------------------------------------------------*/

void *zlib_create_state(void)
{
    z_stream *state = mem_alloc(sizeof(*state), 0, MEM_ALLOC_TEMP);
    if (UNLIKELY(!state)) {
        DLOG("Out of memory!");
        return NULL;
    }

    state->next_in = Z_NULL;
    state->avail_in = 0;
    state->next_out = Z_NULL;
    state->avail_out = 0;
    state->zalloc = zalloc;
    state->zfree = zfree;
    state->opaque = (void *)0;  // We store the current output size here.

    if (inflateInit(state) != Z_OK) {
        DLOG("inflateInit() failed: %s", state->msg);
        mem_free(state);
        return NULL;
    }

    return state;
}

/*-----------------------------------------------------------------------*/

int zlib_decompress_partial(void *state,
                            const void *in_data, int32_t in_size,
                            void *out_buffer, int32_t out_bufsize,
                            int32_t *out_size_ret)
{
    PRECOND(state != NULL, return 0);
    PRECOND(in_data != NULL, return 0);
    PRECOND(out_buffer != NULL || out_bufsize == 0, return 0);

    z_stream *inflater = (z_stream *)state;
    const intptr_t out_so_far = (intptr_t)inflater->opaque;
    if (out_bufsize < out_so_far) {
        DLOG("Output buffer shrunk below current output size");
        return 0;
    }

    inflater->next_in = in_data;
    inflater->avail_in = in_size;
    inflater->next_out = (void *)((char *)out_buffer + out_so_far);
    inflater->avail_out = out_bufsize - out_so_far;

    int result;
    while ((result = inflate(inflater, 0)) != Z_STREAM_END) {
        /* Note that for our purposes, Z_OK and Z_BUF_ERROR mean the same
         * thing -- in both cases, the inflater ran out of either input
         * data or output buffer space. */
        if (result == Z_OK || result == Z_BUF_ERROR) {
            if (inflater->avail_in == 0) {
                const int32_t out_size =
                    (char *)inflater->next_out - (char *)out_buffer;
                inflater->opaque = (void *)(intptr_t)out_size;
                if (out_size_ret) {
                    *out_size_ret = out_size;
                }
                return -1;
            } else {
                DLOG("Buffer overflow during decompression");
                return 0;
            }
        } else {
            DLOG("inflate() failed: %s", inflater->msg);
            return 0;
        }
    }  // while ((result = inflate(inflater, 0)) != Z_STREAM_END)

    if (out_size_ret) {
        *out_size_ret = (char *)inflater->next_out - (char *)out_buffer;
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

void zlib_destroy_state(void *state)
{
    inflateEnd(state);
    mem_free(state);
}

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_UTILITY_INCLUDE_ZLIB
