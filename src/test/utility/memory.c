/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/utility/memory.c: Tests for memory manipulation utility functions.
 */

#include "src/base.h"
#include "src/memory.h"
#include "src/test/base.h"
#include "src/utility/memory.h"

/*-----------------------------------------------------------------------*/

static int test_mem_transpose32_one(int rows, int cols, int align);

/*************************************************************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_utility_memory)

/*-----------------------------------------------------------------------*/

TEST(test_mem_fill32)
{
    uint32_t buf[4];

    /* Normal test. */
    buf[0] = buf[1] = buf[2] = buf[3] = 1;
    mem_fill32(buf, 0x12345678, 4);
    CHECK_INTEQUAL(buf[0], 0x12345678);
    CHECK_INTEQUAL(buf[1], 1);
    CHECK_INTEQUAL(buf[2], 1);
    CHECK_INTEQUAL(buf[3], 1);

    /* Sub-word remainder of size should be ignored. */
    buf[0] = buf[1] = buf[2] = buf[3] = 1;
    mem_fill32(buf, 0x87654321, 15);
    CHECK_INTEQUAL(buf[0], 0x87654321);
    CHECK_INTEQUAL(buf[1], 0x87654321);
    CHECK_INTEQUAL(buf[2], 0x87654321);
    CHECK_INTEQUAL(buf[3], 1);

    /* Zero-size fill should do nothing. */
    buf[0] = 1;
    mem_fill32(buf, 0x15263748, 0);
    CHECK_INTEQUAL(buf[0], 1);
    mem_fill32(buf, 0x51627384, 3);
    CHECK_INTEQUAL(buf[0], 1);

    /* Invalid parameters. */
    mem_fill32(NULL, 0x11223344, 1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mem_fill32_x86)
{
    /* For x86, we use optimized SSE2 fills for 16-byte-aligned address and
     * size when the size is > 128 bytes. */

    uint32_t buf[72];
    /* Set buflen such that:
     *    max(buf_aligned+buflen, buf_unaligned+buflen) <= buf+buflen */
    const int buflen = lenof(buf) - 3;
    uint32_t *buf_aligned = buf;
    while (((uintptr_t)buf_aligned & 15) != 0) {
        buf_aligned++;
    }
    uint32_t *buf_unaligned = (buf_aligned == buf ? buf+1 : buf);
    int i;

    /* Fill with aligned pointer but unaligned size. */
    mem_clear(buf_aligned, 4*buflen);
    mem_fill32(buf_aligned, 0x12345678, 148);
    for (i = 0; i < 148/4; i++) {
        CHECK_TRUE(buf_aligned[i] == 0x12345678);
    }
    for (; i < buflen; i++) {
        CHECK_TRUE(buf_aligned[i] == 0);
    }

    /* Fill with unaligned pointer but aligned size. */
    mem_clear(buf_unaligned, 4*buflen);
    mem_fill32(buf_unaligned, 0x12345678, 144);
    for (i = 0; i < 144/4; i++) {
        CHECK_TRUE(buf_unaligned[i] == 0x12345678);
    }
    for (; i < buflen; i++) {
        CHECK_TRUE(buf_unaligned[i] == 0);
    }

    /* Fill with aligned pointer and size but below the limit. */
    mem_clear(buf_aligned, 4*buflen);
    mem_fill32(buf_aligned, 0x12345678, 128);
    for (i = 0; i < 128/4; i++) {
        CHECK_TRUE(buf_aligned[i] == 0x12345678);
    }
    for (; i < buflen; i++) {
        CHECK_TRUE(buf_aligned[i] == 0);
    }

    /* Fill with aligned pointer and size. */
    mem_clear(buf_aligned, 4*buflen);
    mem_fill32(buf_aligned, 0x12345678, 144);
    for (i = 0; i < 144/4; i++) {
        CHECK_TRUE(buf_aligned[i] == 0x12345678);
    }
    for (; i < buflen; i++) {
        CHECK_TRUE(buf_aligned[i] == 0);
    }

    /* Fill with 1 large loop + 7 small loops. */
    mem_clear(buf_aligned, 4*buflen);
    mem_fill32(buf_aligned, 0x12345678, 240);
    for (i = 0; i < 240/4; i++) {
        CHECK_TRUE(buf_aligned[i] == 0x12345678);
    }
    for (; i < buflen; i++) {
        CHECK_TRUE(buf_aligned[i] == 0);
    }

    /* Fill with 2 large loops + 0 small loops. */
    mem_clear(buf_aligned, 4*buflen);
    mem_fill32(buf_aligned, 0x12345678, 256);
    for (i = 0; i < 256/4; i++) {
        CHECK_TRUE(buf_aligned[i] == 0x12345678);
    }
    for (; i < buflen; i++) {
        CHECK_TRUE(buf_aligned[i] == 0);
    }

    /* Fill with 2 large loops + 1 small loop. */
    mem_clear(buf_aligned, 4*buflen);
    mem_fill32(buf_aligned, 0x12345678, 272);
    for (i = 0; i < 272/4; i++) {
        CHECK_TRUE(buf_aligned[i] == 0x12345678);
    }
    for (; i < buflen; i++) {
        CHECK_TRUE(buf_aligned[i] == 0);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mem_fill32_arm)
{
    /* For ARM, we use optimized fills when the size is >= 64 bytes.  In a
     * 64-bit build, there is additional handling depending on whether the
     * target address is 8-byte aligned or not. */

    uint32_t buf[96/4];

    for (unsigned int size = 60; size < sizeof(buf); size++) {
        mem_clear(buf, sizeof(buf));
        mem_fill32(buf, 0x12345678, size);
        unsigned int i;
        for (i = 0; i < size/4; i++) {
            if (buf[i] != 0x12345678) {
                FAIL("buf[%u] not filled with 0x12345678 for size=%u aligned",
                     i, size);
            }
        }
        for (; i < sizeof(buf)/4; i++) {
            if (buf[i] != 0) {
                FAIL("buf[%u] not left at zero for size=%u aligned", i, size);
            }
        }

        mem_clear(buf, sizeof(buf));
        mem_fill32(buf+1, 0x12345678, size-4);
        if (buf[0] != 0) {
            FAIL("buf[0] not left at zero for size=%u unaligned", size);
        }
        for (i = 1; i < size/4; i++) {
            if (buf[i] != 0x12345678) {
                FAIL("buf[%u] not filled with 0x12345678 for size=%u aligned",
                     i, size);
            }
        }
        for (; i < sizeof(buf)/4; i++) {
            if (buf[i] != 0) {
                FAIL("buf[%u] not left at zero for size=%u unaligned",
                     i, size);
            }
        }
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mem_transpose32)
{
#ifdef SIL_UTILITY_MEMORY_TRANSPOSE_BLOCK_SIZE
    const int BLOCK_SIZE = SIL_UTILITY_MEMORY_TRANSPOSE_BLOCK_SIZE;
#else
    const int BLOCK_SIZE = 16;
#endif

    CHECK_TRUE(test_mem_transpose32_one(10, 10, 1));
    CHECK_TRUE(test_mem_transpose32_one(10, 10, 16));
    CHECK_TRUE(test_mem_transpose32_one(BLOCK_SIZE, 2*BLOCK_SIZE, 1));
    CHECK_TRUE(test_mem_transpose32_one(BLOCK_SIZE, 2*BLOCK_SIZE, 16));
    CHECK_TRUE(test_mem_transpose32_one(BLOCK_SIZE+1, 2*BLOCK_SIZE, 1));
    CHECK_TRUE(test_mem_transpose32_one(BLOCK_SIZE+1, 2*BLOCK_SIZE, 16));
    CHECK_TRUE(test_mem_transpose32_one(BLOCK_SIZE+1, 2*BLOCK_SIZE+1, 1));
    CHECK_TRUE(test_mem_transpose32_one(BLOCK_SIZE+1, 2*BLOCK_SIZE+1, 16));

    /* Make sure these don't crash. */
    uint32_t buf[1];
    mem_transpose32(NULL, buf, 1, 1, 1, 1);
    mem_transpose32(buf, NULL, 1, 1, 1, 1);

    return 1;
}

/*-----------------------------------------------------------------------*/

static int test_mem_transpose32_one(int rows, int cols, int align)
{
    uint32_t *in, *out;

    const int src_stride = align_up(cols, align);
    const int dest_stride = align_up(rows, align);

    ASSERT(in = mem_alloc(4*(rows*src_stride), 0, MEM_ALLOC_TEMP));
    for (int i = 0; i < rows; i++) {
        int j;
        for (j = 0; j < cols; j++) {
            in[i*src_stride+j] = i*cols+j;
        }
        for (; j < src_stride; j++) {
            in[i*src_stride+j] = 0xDEADBEEF;
        }
    }

    ASSERT(out = mem_alloc(4*(cols*dest_stride), 0, MEM_ALLOC_TEMP));
    for (int i = 0; i < cols; i++) {
        int j;
        for (j = 0; j < dest_stride; j++) {
            out[i*dest_stride+j] = 0xC0D1F1ED;
        }
    }

    mem_transpose32(out, in, cols, rows, src_stride, dest_stride);
    mem_free(in);

    for (int i = 0; i < cols; i++) {
        int j;
        for (j = 0; j < rows; j++) {
            if (out[i*dest_stride+j] != (uint32_t)(j*cols+i)) {
                const uint32_t val = out[i*dest_stride+j];
                mem_free(out);
                FAIL("rows=%d cols=%d align=%d: row %d col %d should have"
                     " been %d but was %d", rows, cols, align, i, j,
                     j*cols+i, val);
            }
        }
        for (; j < dest_stride; j++) {
            if (out[i*dest_stride+j] != 0xC0D1F1ED) {
                const uint32_t val = out[i*dest_stride+j];
                mem_free(out);
                FAIL("rows=%d cols=%d align=%d: row %d col %d should not"
                     " have been modified but was %d", rows, cols, align,
                     i, j, val);
            }
        }
    }

    mem_free(out);
    return 1;
}

/*************************************************************************/
/*************************************************************************/
