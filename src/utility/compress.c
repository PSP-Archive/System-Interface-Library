/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/utility/compress.c: Functions for compressing and decompressing data.
 */

#include "src/base.h"
#include "src/memory.h"
#include "src/utility/compress.h"
#include "src/utility/tinflate.h"
#include "src/utility/zlib.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* State block structure for decompress_partial(). */
typedef struct DecompressState DecompressState;
struct DecompressState {
    void *state;   // Library-specific data block.
    int32_t size;  // Size of data decompressed so far.
};

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

#ifdef SIL_UTILITY_INCLUDE_ZLIB
# define USED_ZLIB  /*nothing*/
#else
# define USED_ZLIB  UNUSED
#endif

void *compress(const void *in_data, int32_t in_size, int32_t *out_size_ret,
               USED_ZLIB int mem_flags, USED_ZLIB int level)
{
    PRECOND(in_data != NULL, return NULL);
    PRECOND(in_size >= 0, return NULL);
    PRECOND(out_size_ret != NULL, return NULL);

#ifdef SIL_UTILITY_INCLUDE_ZLIB
    void *out_data = zlib_compress(in_data, in_size, out_size_ret,
                                   mem_flags, level);
    if (!out_data) {
        DLOG("Compression failed");
    }
    return out_data;
#else
    DLOG("zlib not compiled in, can't compress data!");
    return NULL;
#endif
}

/*-----------------------------------------------------------------------*/

void *decompress(const void *in_data, int32_t in_size,
                 int32_t *out_size_ret, int mem_flags)
{
    PRECOND(in_data != NULL, return NULL);
    PRECOND(in_size >= 0, return NULL);
    PRECOND(out_size_ret != NULL, return NULL);

#ifdef SIL_UTILITY_INCLUDE_ZLIB
    void *out_data = zlib_decompress(in_data, in_size, out_size_ret,
                                     mem_flags);
    if (!out_data) {
        DLOG("Decompression failed");
    }
    return out_data;
#else
    const long out_size = tinflate(in_data, in_size, NULL, 0, NULL);
    if (out_size < 0) {
        DLOG("Compressed data is corrupt");
        return NULL;
    }
    void *out_data = mem_alloc(out_size, 0, mem_flags & ~MEM_ALLOC_CLEAR);
    if (!out_data) {
        DLOG("Out of memory for decompressed data buffer (unable to allocate"
             " %ld bytes)", out_size);
        return NULL;
    }
    ASSERT(tinflate(in_data, in_size, out_data, out_size, NULL) == out_size);
    if (out_size_ret) {
        *out_size_ret = out_size;
    }
    return out_data;
#endif
}

/*-----------------------------------------------------------------------*/

int decompress_to(const void *in_data, int32_t in_size,
                  void *out_buffer, int32_t out_bufsize,
                  int32_t *out_size_ret)
{
    PRECOND(in_data != NULL, return 0);
    PRECOND(in_size >= 0, return 0);
    PRECOND(out_buffer != NULL || out_bufsize == 0, return 0);
    PRECOND(out_bufsize >= 0, return 0);

#ifdef SIL_UTILITY_INCLUDE_ZLIB
    const int result = zlib_decompress_to(in_data, in_size, out_buffer,
                                          out_bufsize, out_size_ret);
    if (!result) {
        DLOG("Decompression failed");
    }
    return result;
#else
    const long out_size = tinflate(in_data, in_size, out_buffer, out_bufsize,
                                   NULL);
    if (out_size < 0) {
        DLOG("Compressed data is corrupt");
        return 0;
    } else {
        if (out_size_ret) {
            *out_size_ret = out_size;
        }
        if (out_size > out_bufsize) {
            DLOG("Buffer overflow during decompression");
            return 0;
        } else {
            return 1;
        }
    }
#endif
}

/*-----------------------------------------------------------------------*/

void *decompress_create_state(void)
{
    DecompressState *state = mem_alloc(sizeof(*state), 0, MEM_ALLOC_TEMP);
    if (UNLIKELY(!state)) {
        DLOG("Failed to allocate state block");
        return NULL;
    }
#ifdef SIL_UTILITY_INCLUDE_ZLIB
    state->state = zlib_create_state();
#else
    state->state = mem_alloc(tinflate_state_size(), 0,
                             MEM_ALLOC_TEMP | MEM_ALLOC_CLEAR);
#endif
    if (UNLIKELY(!state->state)) {
        DLOG("Failed to allocate low-level state block");
        mem_free(state);
        return NULL;
    }
    state->size = 0;
    return state;
}

/*-----------------------------------------------------------------------*/

int decompress_partial(void *state_,
                       const void *in_data, int32_t in_size,
                       void *out_buffer, int32_t out_bufsize,
                       int32_t *out_size_ret)
{
    PRECOND(state_ != NULL, return 0);
    PRECOND(in_data != NULL, return 0);
    PRECOND(in_size >= 0, return 0);
    PRECOND(out_buffer != NULL || out_bufsize == 0, return 0);
    PRECOND(out_bufsize >= 0, return 0);

    DecompressState *state = (DecompressState *)state_;

#ifdef SIL_UTILITY_INCLUDE_ZLIB
    const int result = zlib_decompress_partial(
        state->state, in_data, in_size, out_buffer, out_bufsize, &state->size);
    if (out_size_ret) {
        *out_size_ret = state->size;
    }
    return result<0 ? -1 : result>0 ? 1 : 0;
#else
    unsigned long size = state->size;
    const int result = tinflate_partial(
        in_data, in_size, out_buffer, out_bufsize, &size, NULL,
        state->state, tinflate_state_size());
    state->size = size;
    if (out_size_ret) {
        *out_size_ret = state->size;
    }
    if (result < 0) {
        DLOG("Compressed data is corrupt");
        return 0;
    } else if (state->size > out_bufsize) {
        DLOG("Buffer overflow during decompression");
        return 0;
    } else {
        return result==0 ? 1 : -1;
    }
#endif
}

/*-----------------------------------------------------------------------*/

void decompress_destroy_state(void *state_)
{
    DecompressState *state = (DecompressState *)state_;

    if (state) {
#ifdef SIL_UTILITY_INCLUDE_ZLIB
        zlib_destroy_state(state->state);
#else
        mem_free(state->state);
#endif
        mem_free(state);
    }
}

/*************************************************************************/
/*************************************************************************/
