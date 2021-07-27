/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/utility/compress.c: Tests for compression/decompression utility
 * functions.
 */

/*
 * Since we test the individual library interfaces separately, this source
 * file only checks that the compress() and decompress*() wrappers function
 * as advertised.
 */

#include "src/base.h"
#include "src/memory.h"
#include "src/test/base.h"
#include "src/utility/compress.h"
#include "src/utility/tinflate.h"

/*************************************************************************/
/******************************* Test data *******************************/
/*************************************************************************/

/* Input string for compression tests. */
static const char original_data[] = "test";

/* Input compressed data for decompression tests. */
static const uint8_t compressed_data[] = {
    0x78, 0x01, 0x2B, 0x49, 0x2D, 0x2E, 0x61, 0x00, 0x00,
#ifdef SIL_UTILITY_INCLUDE_ZLIB  // tinflate doesn't process these bytes.
    0x06, 0x1E, 0x01, 0xC1,
#endif
};

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_utility_compress)

/*-----------------------------------------------------------------------*/

TEST(test_compress)
{
    void *compressed;
    int32_t comp_size;

#ifdef SIL_UTILITY_INCLUDE_ZLIB
    char decompressed[5];
    long decomp_size;

    CHECK_TRUE(compressed = compress(original_data, sizeof(original_data),
                                     &comp_size, 0, -1));
    decomp_size = tinflate(compressed, comp_size,
                           decompressed, sizeof(decompressed), NULL);
    mem_free(compressed);
    CHECK_INTEQUAL(decomp_size, sizeof(original_data));
    CHECK_STREQUAL(decompressed, original_data);
#else
    CHECK_FALSE(compressed = compress(original_data, sizeof(original_data),
                                      &comp_size, 0, -1));
#endif

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_compress_memory_failure)
{
#ifdef SIL_UTILITY_INCLUDE_ZLIB
    void *compressed;
    int32_t comp_size;
    char decompressed[5];
    long decomp_size;

    CHECK_MEMORY_FAILURES(compressed = compress(
                              original_data, sizeof(original_data),
                              &comp_size, 0, -1));
    decomp_size = tinflate(compressed, comp_size,
                           decompressed, sizeof(decompressed), NULL);
    mem_free(compressed);
    CHECK_INTEQUAL(decomp_size, sizeof(original_data));
    CHECK_STREQUAL(decompressed, original_data);
#endif

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_decompress)
{
    const uint8_t compressed[] =
        "\x78\x01\x2B\x49\x2D\x2E\x61\x00\x00\x06\x1E\x01\xC1";
    char *decompressed;
    int32_t decomp_size;

    CHECK_TRUE(decompressed = decompress(compressed, sizeof(compressed),
                                         &decomp_size, 0));
    CHECK_INTEQUAL(decomp_size, sizeof(original_data));
    CHECK_STREQUAL(decompressed, original_data);
    mem_free(decompressed);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_decompress_memory_failures)
{
    const uint8_t compressed[] =
        "\x78\x01\x2B\x49\x2D\x2E\x61\x00\x00\x06\x1E\x01\xC1";
    char *decompressed;
    int32_t decomp_size;

    CHECK_MEMORY_FAILURES(decompressed = decompress(
                              compressed, sizeof(compressed), &decomp_size, 0));
    CHECK_INTEQUAL(decomp_size, sizeof(original_data));
    CHECK_STREQUAL(decompressed, original_data);
    mem_free(decompressed);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_decompress_to)
{
    char decompressed[5];
    int32_t decomp_size;

    CHECK_TRUE(decompress_to(compressed_data, sizeof(compressed_data),
                             decompressed, sizeof(decompressed),
                             &decomp_size));
    CHECK_INTEQUAL(decomp_size, sizeof(original_data));
    CHECK_STREQUAL(decompressed, original_data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_decompress_to_failure)
{
    char decompressed[5];

    CHECK_FALSE(decompress_to(compressed_data, 1,
                              decompressed, sizeof(decompressed), NULL));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_decompress_to_buffer_overflow)
{
    char decompressed[2] = {'x', 'x'};

    CHECK_FALSE(decompress_to(compressed_data, sizeof(compressed_data),
                              decompressed, 1, NULL));
    CHECK_INTEQUAL(decompressed[0], original_data[0]);
    CHECK_INTEQUAL(decompressed[1], 'x');

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_decompress_partial)
{
    char decompressed[5];
    int32_t decomp_size;
    void *state;

    CHECK_TRUE(state = decompress_create_state());
    for (int i = 0; i < (int)sizeof(compressed_data) - 1; i++) {
        const int result = decompress_partial(
            state, &compressed_data[i], 1, decompressed, sizeof(decompressed),
            &decomp_size);
        if (result != -1) {
            FAIL("decompress_partial() for byte %d was %d but should have"
                 " been -1", i, result);
        }
    }
    CHECK_INTEQUAL(decompress_partial(
                       state, &compressed_data[sizeof(compressed_data)-1], 1,
                       decompressed, sizeof(decompressed), &decomp_size), 1);
    decompress_destroy_state(state);
    CHECK_INTEQUAL(decomp_size, sizeof(original_data));
    CHECK_STREQUAL(decompressed, original_data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_decompress_partial_memory_failure)
{
    char decompressed[5];
    int32_t decomp_size;
    void *state;

    CHECK_MEMORY_FAILURES(state = decompress_create_state());
    for (int i = 0; i < (int)sizeof(compressed_data) - 1; i++) {
        const int result = decompress_partial(
            state, &compressed_data[i], 1, decompressed, sizeof(decompressed),
            &decomp_size);
        if (result != -1) {
            FAIL("decompress_partial() for byte %d was %d but should have"
                 " been -1", i, result);
        }
    }
    CHECK_INTEQUAL(decompress_partial(
                       state, &compressed_data[sizeof(compressed_data)-1], 1,
                       decompressed, sizeof(decompressed), &decomp_size), 1);
    decompress_destroy_state(state);
    CHECK_INTEQUAL(decomp_size, sizeof(original_data));
    CHECK_STREQUAL(decompressed, original_data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_decompress_partial_no_size)
{
    char decompressed[5];
    void *state;

    CHECK_TRUE(state = decompress_create_state());
    for (int i = 0; i < (int)sizeof(compressed_data) - 1; i++) {
        const int result = decompress_partial(
            state, &compressed_data[i], 1, decompressed, sizeof(decompressed), NULL);
        if (result != -1) {
            FAIL("decompress_partial() for byte %d was %d but should have"
                 " been -1", i, result);
        }
    }
    CHECK_INTEQUAL(decompress_partial(
                       state, &compressed_data[sizeof(compressed_data)-1], 1,
                       decompressed, sizeof(decompressed), NULL), 1);
    decompress_destroy_state(state);
    CHECK_STREQUAL(decompressed, original_data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_decompress_partial_no_size_buffer_overflow)
{
    char decompressed[2] = {'x', 'x'};
    void *state;

    CHECK_TRUE(state = decompress_create_state());
    for (int i = 0; i < (int)sizeof(compressed_data) - 1; i++) {
        const int result = decompress_partial(
            state, &compressed_data[i], 1, decompressed, 1, NULL);
        if (result == 1) {
            FAIL("decompress_partial() for byte %d was 1 but should have"
                 " been 0 or -1", i);
        }
    }
    CHECK_INTEQUAL(decompress_partial(
                       state, &compressed_data[sizeof(compressed_data)-1], 1,
                       decompressed, 1, NULL), 0);
    decompress_destroy_state(state);
    CHECK_INTEQUAL(decompressed[0], original_data[0]);
    CHECK_INTEQUAL(decompressed[1], 'x');

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_decompress_destroy_state_null)
{
    decompress_destroy_state(NULL);
    return 1;
}

/*************************************************************************/
/*************************************************************************/
