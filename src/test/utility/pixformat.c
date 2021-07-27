/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/utility/misc.c: Tests for miscellaneous utility functions.
 */

#include "src/base.h"
#include "src/test/base.h"
#include "src/utility/pixformat.h"

/*************************************************************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_utility_pixformat)

/*-----------------------------------------------------------------------*/

TEST(test_bgra8888_rgba8888)
{
    static const uint8_t in[3][4] = {{1,2,3,4}, {5,6,7,8}, {9,10,11,12}};
    static const uint8_t expected[3][4] = {{3,2,1,4}, {7,6,5,8}, {0,0,0,0}};
    uint8_t out[3][4];
    mem_clear(out, sizeof(out));
    pixel_convert_bgra8888_rgba8888(out, in, lenof(in)-1);
    CHECK_MEMEQUAL(out, expected, sizeof(expected));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_rgb565_bgr565)
{
    static const uint16_t in[3] = {0x1841, 0x30A4, 0x4907};
    static const uint16_t expected[3] = {0x0843, 0x20A6, 0};
    uint16_t out[3];
    mem_clear(out, sizeof(out));
    pixel_convert_rgb565_bgr565(out, in, lenof(in)-1);
    CHECK_MEMEQUAL(out, expected, sizeof(expected));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_rgba5551_abgr1555)
{
    static const uint16_t in[3] = {0x0C41, 0x98A4, 0x2507};
    static const uint16_t expected[3] = {0x0886, 0x214D, 0};
    uint16_t out[3];
    mem_clear(out, sizeof(out));
    pixel_convert_rgba5551_abgr1555(out, in, lenof(in)-1);
    CHECK_MEMEQUAL(out, expected, sizeof(expected));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_bgra5551_abgr1555)
{
    static const uint16_t in[3] = {0x0443, 0x90A6, 0x1D09};
    static const uint16_t expected[3] = {0x0886, 0x214D, 0};
    uint16_t out[3];
    mem_clear(out, sizeof(out));
    pixel_convert_bgra5551_abgr1555(out, in, lenof(in)-1);
    CHECK_MEMEQUAL(out, expected, sizeof(expected));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_rgba5551_bgra5551)
{
    static const uint16_t in[3] = {0x0C41, 0x98A4, 0x2507};
    static const uint16_t expected[3] = {0x0443, 0x90A6, 0};
    uint16_t out[3];
    mem_clear(out, sizeof(out));
    pixel_convert_rgba5551_bgra5551(out, in, lenof(in)-1);
    CHECK_MEMEQUAL(out, expected, sizeof(expected));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_rgba4444_abgr4444)
{
    static const uint16_t in[3] = {0x4321, 0x8765, 0xCBA9};
    static const uint16_t expected[3] = {0x1234, 0x5678, 0};
    uint16_t out[3];
    mem_clear(out, sizeof(out));
    pixel_convert_rgba4444_abgr4444(out, in, lenof(in)-1);
    CHECK_MEMEQUAL(out, expected, sizeof(expected));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_bgra4444_abgr4444)
{
    static const uint16_t in[3] = {0x4123, 0x8567, 0xC9AB};
    static const uint16_t expected[3] = {0x1234, 0x5678, 0};
    uint16_t out[3];
    mem_clear(out, sizeof(out));
    pixel_convert_bgra4444_abgr4444(out, in, lenof(in)-1);
    CHECK_MEMEQUAL(out, expected, sizeof(expected));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_rgba4444_bgra4444)
{
    static const uint16_t in[3] = {0x4321, 0x8765, 0xCBA9};
    static const uint16_t expected[3] = {0x4123, 0x8567, 0};
    uint16_t out[3];
    mem_clear(out, sizeof(out));
    pixel_convert_rgba4444_bgra4444(out, in, lenof(in)-1);
    CHECK_MEMEQUAL(out, expected, sizeof(expected));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_rgb565_rgba8888)
{
    static const uint16_t in[3] = {0x1841, 0x30A4, 0x4907};
    static const uint8_t expected[3][4] = {{8,8,24,255}, {33,20,49,255},
                                           {0,0,0,0}};
    uint8_t out[3][4];
    mem_clear(out, sizeof(out));
    pixel_convert_rgb565_rgba8888(out, in, lenof(in)-1);
    CHECK_MEMEQUAL(out, expected, sizeof(expected));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_bgr565_rgba8888)
{
    static const uint16_t in[3] = {0x0843, 0x20A6, 0x3909};
    static const uint8_t expected[3][4] = {{8,8,24,255}, {33,20,49,255},
                                           {0,0,0,0}};
    uint8_t out[3][4];
    mem_clear(out, sizeof(out));
    pixel_convert_bgr565_rgba8888(out, in, lenof(in)-1);
    CHECK_MEMEQUAL(out, expected, sizeof(expected));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_rgba5551_rgba8888)
{
    static const uint16_t in[3] = {0x0C41, 0x98A4, 0x2507};
    static const uint8_t expected[3][4] = {{8,16,24,0}, {33,41,49,255},
                                           {0,0,0,0}};
    uint8_t out[3][4];
    mem_clear(out, sizeof(out));
    pixel_convert_rgba5551_rgba8888(out, in, lenof(in)-1);
    CHECK_MEMEQUAL(out, expected, sizeof(expected));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_bgra5551_rgba8888)
{
    static const uint16_t in[3] = {0x0443, 0x90A6, 0x1D09};
    static const uint8_t expected[3][4] = {{8,16,24,0}, {33,41,49,255},
                                           {0,0,0,0}};
    uint8_t out[3][4];
    mem_clear(out, sizeof(out));
    pixel_convert_bgra5551_rgba8888(out, in, lenof(in)-1);
    CHECK_MEMEQUAL(out, expected, sizeof(expected));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_rgba4444_rgba8888)
{
    static const uint16_t in[3] = {0x4321, 0x8765, 0xCBA9};
    static const uint8_t expected[3][4] = {{0x11,0x22,0x33,0x44},
                                           {0x55,0x66,0x77,0x88}, {0,0,0,0}};
    uint8_t out[3][4];
    mem_clear(out, sizeof(out));
    pixel_convert_rgba4444_rgba8888(out, in, lenof(in)-1);
    CHECK_MEMEQUAL(out, expected, sizeof(expected));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_bgra4444_rgba8888)
{
    static const uint16_t in[3] = {0x4123, 0x8567, 0xC9AB};
    static const uint8_t expected[3][4] = {{0x11,0x22,0x33,0x44},
                                           {0x55,0x66,0x77,0x88}, {0,0,0,0}};
    uint8_t out[3][4];
    mem_clear(out, sizeof(out));
    pixel_convert_bgra4444_rgba8888(out, in, lenof(in)-1);
    CHECK_MEMEQUAL(out, expected, sizeof(expected));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_l8_rgba8888)
{
    static const uint8_t in[3] = {1, 2, 3};
    static const uint8_t expected[3][4] = {{1,1,1,255}, {2,2,2,255},
                                           {0,0,0,0}};
    uint8_t out[3][4];
    mem_clear(out, sizeof(out));
    pixel_convert_l8_rgba8888(out, in, lenof(in)-1);
    CHECK_MEMEQUAL(out, expected, sizeof(expected));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_a8_rgba8888)
{
    static const uint8_t in[3] = {1, 2, 3};
    static const uint8_t expected[3][4] = {{255,255,255,1}, {255,255,255,2},
                                           {0,0,0,0}};
    uint8_t out[3][4];
    mem_clear(out, sizeof(out));
    pixel_convert_a8_rgba8888(out, in, lenof(in)-1);
    CHECK_MEMEQUAL(out, expected, sizeof(expected));
    return 1;
}

/*************************************************************************/
/*************************************************************************/
