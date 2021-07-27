/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/debug.c: Tests for the debug interface.
 */

#include "src/base.h"
#include "src/debug.h"
#include "src/graphics.h"
#include "src/input.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/test.h"
#include "src/test/base.h"
#include "src/test/graphics/internal.h"
#include "src/texture.h"
#include "src/time.h"

/*************************************************************************/
/******************************* Test data *******************************/
/*************************************************************************/

/* Allowable delta for text pixel comparisons. */
#ifdef SIL_PLATFORM_PSP
# define NEAR  24  // The PSP is really bad at antialiasing, I guess...
#else
# define NEAR  4
#endif

static const uint8_t cpu_text[8][18] = {
    {  0,  2, 18, 28, 24,  3, 29, 28, 19,  1, 19,  6,  0,  5, 25,  0,  0,  0},
    {  5, 78,152,145,163, 29,205,166,176, 61,141, 48,  0, 35,189,  0,  0,  0},
    { 36,186, 56, 21, 31,  6,231, 69,206,136,170, 58,  0, 42,226,  0, 56, 80},
    {103,159, 10,  0,  0,  0,232, 99,209, 74,170, 58,  0, 42,226,  0,110,149},
    {100,171, 14,  0,  0,  0,243,165, 87, 14,170, 65,  0, 46,223,  0, 17, 11},
    { 42,215, 86,  0, 17,  6,230, 58,  1,  9,153,138,  1, 92,185,  0, 29, 40},
    {  4, 93,210,200,199, 34,203, 43,  0,  0, 63,203,199,190, 47,  0,108,147},
    {  0,  0, 18, 38, 19,  1, 36,  1,  0,  0,  0, 17, 38, 16,  0,  0, 14,  9},
};


static const uint8_t pct_0_text[8][20] = {
    {  0,  6, 26, 21,  2,  0,  0,  0, 10, 27, 16,  0, 14, 34, 20,  0,  2, 23,  1,  0},
    {  3,114,164,166, 57,  0,  0,  5,140,163,168, 40,122,135,168, 27, 71,124,  3,  0},
    { 46,217, 33,118,174,  0,  0, 76,207, 31,179,151,162, 33,183, 96,180, 23,  0,  0},
    { 83,204,  0, 76,230,  0,  0,138,176,  0,126,208,165, 60,199,163,146,153,103,  8},
    { 81,213,  0, 78,217,  0,  0,135,186,  0,129,192, 80,132,142,154,225, 69,175, 36},
    { 44,225, 53,117,150, 10, 47, 78,220, 37,187,113,  3, 14,169, 82,223, 35,159, 38},
    {  0,120,216,180, 28, 37,175, 22,156,213,161,  3,  0, 92,115,  1,150,177,136, 13},
    {  0,  0, 31, 15,  0,  5, 17,  1,  5, 30,  6,  0,  0, 24,  3,  0,  8, 47, 15,  0},
};

static const uint8_t pct_100_text[8][28] = {
    {  1, 13, 27,  0,  0,  6, 26, 21,  2,  1, 17, 28, 12,  0,  0,  0,  3, 23, 26,  7,  1, 27, 30,  5,  0, 15,  6,  0},
    { 30,151,205,  0,  3,114,164,166, 57, 28,149,163,146, 17,  0,  1, 70,159,164,102, 62,167,159, 77, 15,116, 30,  0},
    { 10, 88,234,  0, 46,217, 33,118,174,116,138, 28,203, 83,  0, 15,166, 70, 58,189,148,137, 99,147, 80,116,  4,  0},
    {  0, 56,231,  0, 83,204,  0, 76,230,173,106,  0,168,123,  0, 28,189, 36, 26,199,132,170,123,133,196,143,156, 41},
    {  0, 56,231,  0, 81,213,  0, 78,217,172,112,  0,168,115,  0, 27,193, 38, 26,192, 54,138,132,177,144,150,123,178},
    {  0, 56,231,  0, 44,225, 53,117,150,117,157, 21,207, 68, 48, 43,171, 94, 48,178, 23,  7,100,114, 96,139, 82,186},
    {  0, 50,201,  0,  0,120,216,180, 28, 24,181,210,127,  2,181,111, 72,205,199, 78,  1, 19,125, 23, 32,177,189, 66},
    {  0,  8, 26,  0,  0,  0, 31, 15,  0,  0, 16, 30,  0,  0, 24,  4,  0, 26, 25,  0,  0,  5, 16,  1,  2, 30, 34,  0},
};

static const uint8_t pct_inf_text[8][22] = {
    {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1, 27, 30,  5,  0, 15,  6,  0},
    {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 57,167,159, 77, 15,116, 30,  0},
    {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,135,137, 99,147, 80,116,  4,  0},
    {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,109,170,123,133,196,143,156, 41},
    { 87,123, 59, 87,123, 59, 87,123, 59,  0,  0, 66,123, 85, 25,138,132,177,144,150,123,178},
    { 80,112, 35, 80,112, 35, 80,112, 35, 10, 47, 66,112, 66,  3,  7,100,114, 96,139, 82,186},
    {  0,  0,  0,  0,  0,  0,  0,  0,  0, 37,175, 22,  0,  0,  0, 19,125, 23, 32,177,189, 66},
    {  0,  0,  0,  0,  0,  0,  0,  0,  0,  5, 17,  1,  0,  0,  0,  5, 16,  1,  2, 30, 34,  0},
};

static const uint8_t system_text[9][68] = {
    {  0,  1, 15, 28, 17,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1, 24, 29, 17,  1,  6, 25, 24,  3,  0,  0, 10, 14,  0,  6, 25, 24,  3,  2, 21, 27, 10,  0,  8, 26, 21,  1,  3, 23, 26,  7, 12, 35,  0,  0,  0},
    {  0, 21,146,152,138, 10,  0,  0,  0,  0,  0,  0,  0,101, 20,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 27,179,147, 81,  4,115,154,187, 92,  0,  6,142,106,  0,115,154,187, 92, 60,145,149,122, 10,126,146,178, 51, 70,159,164,102, 75,210,  0,  0,  0},
    {  0, 41,202, 45, 29, 52, 78,  0, 43, 75, 97,125,106,238,143, 18, 60,135, 66,  6, 79, 96,136, 76,107,127, 36,  0,106, 37,  0,  0, 78,178, 21, 10,  0, 46, 27,152,197,  1,127,229,127,  0, 46, 27,152,197,130,114, 56,190, 33,221, 53,199,128,166, 70, 58,189, 91,210, 24, 94,  0},
    {  0, 21,179,177, 28, 81,217, 15,130,125,230,105, 91,240,116, 76,222,132,227, 33,170,160,139,238,127,171,149,  8,206, 67,  0,  0, 84,206,186, 64,  2,  0, 18,192,112, 83,169,130,130,  1,  0, 18,192,111, 98,222,182,129,  6,201,201,216, 53,189, 36, 26,199,101,222,144,104,  0},
    {  0,  0, 33,164,183, 34,207,122,152, 48,175,163, 19,226, 42,137,208,127,196, 54,170, 48, 31,177, 29, 51,169, 14, 29,  3,  0,  0,  0, 10,111,185, 20, 12,134,135, 33,203,174,200,188, 20, 12,134,135, 12,116,143,108,167, 47,190,112,170,136,193, 38, 26,192, 99,250,189, 24,  0},
    {  0, 24, 25, 58,222, 31,120,230,125, 12, 38,165, 42,227, 47,116,211, 43, 35, 19,170, 44, 29,169, 28, 43,169, 14, 55, 18,  0,  4, 27,  4, 73,185, 20,129,141, 15,  6, 41, 43,146,145,  6,129,141, 15, 13,166,105, 22,179, 92,225, 37,107,194,171, 94, 48,178, 88,232,186,120,  0},
    {  0, 96,207,208,107,  0, 23,228, 57, 76,208,181, 29,180,184, 40,183,196,191, 32,148, 39, 25,148, 25, 38,148, 12,203, 67,  0, 20,168,199,201, 69, 69,225,211,207,165,  0,  0,109,111, 69,225,211,207,166, 91,212,199,132, 18,180,209,201, 51, 72,205,199, 78, 64,182, 69,206,  0},
    {  0,  3, 28, 24,  0,  4, 77,162,  6,  0, 29, 18,  0, 12, 26,  3,  7, 35, 12,  2, 21,  5,  5, 21,  2,  7, 21,  0, 25,  2,  0,  0, 14, 38, 18,  0, 15, 42, 42, 42, 25,  0,  0, 17, 14, 15, 42, 42, 42, 25,  1, 25, 30,  4,  0,  6, 32, 12,  0,  0, 26, 25,  0, 12, 23,  3, 27,  0},
    {  0,  0,  0,  0,  4,116,168, 24,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},
};

static const uint8_t avail_text[8][59] = {
    {  0,  5, 29, 29, 24,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 17, 28, 15,  0,  2, 19, 24,  0,  0,  0,  3, 25,  0,  1, 24, 29, 17,  1, 27, 29, 29, 24,  2, 19, 28, 14,  1,  8, 26, 21,  3, 29,  7,  0,  0,  0,  0},
    {  0, 36,190,144,109,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,135,159,166, 18, 71,162,177,  0,  0,  1, 68,198,  0, 27,179,147, 81,  4,138,148,181,163, 54,146,161,152, 18,126,146,178, 61,168, 42,  0,  0,  0,  0},
    {  0, 43,183, 34, 19, 78,116, 96, 24,118,123, 26,  0, 60,135, 66,  0, 56, 80,  0,  0,  0, 29, 48,225, 55, 24,115,212,  0,  0, 27,188,250,  0, 78,178, 21, 10,  0, 20, 32,205, 92, 24, 33, 49,207, 48,221, 53,199,127,168, 42, 57, 52,  0,  0},
    {  0, 43,202, 91, 70,170,175,104,162,137,165,134, 70,222,132,227, 20,110,149,  0,  0,  0, 20,165,197, 12,  0, 94,212,  0, 12,156,139,230,  4, 84,206,186, 64,  2,  1, 92,176,  0,  0,  0, 67,178, 22,201,201,216, 40,169,102,194, 41,  0,  0},
    {  0, 43,216,136,105,169, 47, 28,195,142,141,130,141,208,127,196, 42, 17, 11,  0,  0,  0, 14, 96,193,122,  0, 94,212,  0,132,233,174,244,101,  0, 10,111,185, 20, 21,167, 59,  0,  0, 60,183, 65, 26,190,112,170,129,169,240,117,  0,  0,  0},
    {  0, 43,178, 17, 14,168, 42, 23,191, 90, 29, 22,117,211, 43, 35,  5, 29, 40,  0,  0,  0,  4,  2,165,169,  0, 94,212,  0, 32, 43, 83,233, 34, 27,  4, 73,185, 20,101,146,  9,  0, 56,198, 57,  2, 64,225, 37,107,194,169,151,215, 48,  0,  0},
    {  0, 38,156, 14, 13,147, 37,  3, 97,208,180,110, 18,183,196,191, 20,108,147,  0,  0,  0,195,203,193, 30,  0, 83,184,  0,  0,  0, 43,199, 20,168,199,201, 69,  1,181, 56,  0, 14,155,217,207,203, 40,180,209,201, 61,147, 37,153,110,  0,  0},
    {  0,  7, 22,  0,  2, 21,  5,  0,  1, 27, 32,  3,  0,  7, 35, 12,  0, 14,  9,  0,  0,  0, 16, 35,  9,  0,  0, 14, 23,  0,  0,  0,  7, 26,  0, 14, 38, 18,  0,  0, 26,  0,  0,  3, 30, 42, 42, 36,  5,  6, 32, 12,  2, 21,  5, 10, 16,  0,  0},
};

static const uint8_t self_text[8][61] = {
    {  0, 13, 15,  0,  1, 19,  5,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  7, 38,  0,  0,  0,  0,  0,  0,  6, 25, 24,  3,  3, 23, 26,  7,  0,  8, 26, 23,  2, 13, 28, 29, 29,  5,  8, 25,  5,  0,  1, 24, 29, 17,  1,  6, 25, 24,  5, 29,  7,  0,  0,  0},
    {  0, 94,114,  2, 12,141, 35,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 42,226,  0,  0,  0,  0,  0,  0,115,154,187, 93, 70,159,164,102, 15,137,154,175, 80, 68,146,155,192, 39,139,190, 35,  0, 27,179,147, 81,  4,115,154,187,101,168, 42,  0,  0,  0},
    {  0,114,136,  3, 14,169, 43, 15,114,116,  8, 89,136, 43,  0, 38,134,139,236,  0, 56, 80,  0,  0,  0, 46, 27,152,200,166, 70, 58,189, 81,221, 45,138,198,  9, 21, 76,174, 20, 61,192, 43,  0, 78,178, 21, 10,  0, 46, 27,152,200,168, 42, 57, 52,  0},
    {  0,114,136,  3, 14,169, 43, 98,183,102,108,179,143,206, 42,210,111,165,250,  0,110,149,  0,  0,  0,  0, 18,192,127,189, 36, 26,199, 78,225, 82,167,188,  0,  7,176, 88,  0, 19,184, 43,  0, 84,206,186, 64,  2,  0, 18,192,119,169,102,194, 41,  0},
    {  0,113,141,  4, 16,171, 42, 51,188,134,175,175,128,183, 97,196,  0, 58,232,  0, 17, 11,  0,  0,  0, 12,134,135, 33,193, 38, 26,192, 39,112,161,235, 98,  0,104,179, 14,  0, 19,184, 43,  0,  0, 10,111,185, 20, 12,134,135, 21,169,240,117,  0,  0},
    {  0, 86,185, 19, 34,182, 31, 12, 55,220,156,150, 33, 32, 73,222, 13, 80,239,  0, 29, 40,  0,  0,  0,129,141, 15, 15,171, 94, 48,178, 23, 10,128,156,  8, 37,212, 45,  0,  0, 19,184, 43,  4, 27,  4, 73,185, 20,129,141, 15, 14,169,151,215, 48,  0},
    {  0, 21,172,202,200, 99,  3,112,202,174, 44,196,184,166, 23,173,190,179,217,  0,108,147,  0,  0, 69,225,211,207,165, 72,205,199, 78,  1,173,141, 14,  0, 98,126,  0,  0,  0, 17,160, 37, 20,168,199,201, 69, 69,225,211,207,170,147, 37,153,110,  0},
    {  0,  0, 10, 37, 26,  1,  0,  5, 31,  7,  0, 17, 36,  6,  0, 12, 50, 27, 26,  0, 14,  9,  0,  0, 15, 42, 42, 42, 25,  0, 26, 25,  0,  0, 22,  2,  0,  0, 16,  6,  0,  0,  0,  3, 22,  5,  0, 14, 38, 18,  0, 15, 42, 42, 42, 27, 21,  5, 10, 16,  0},
};

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

/**
 * text_scale:  Return the expected text size for the given base (unscaled)
 * size.
 *
 * [Parameters]
 *     size: Unscaled text size (width or height), in pixels.
 * [Return value]
 *     Expected scaled text size, in pixels.
 */
static int text_scale(int size)
{
    const float scale = lbound(graphics_display_height() / 720.0f, 0.75f);
    return iroundf(size * scale);
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

static int do_test_debug(void);
int test_debug(void)
{
#ifdef PSP
    return run_tests_in_sized_window(do_test_debug, 480, 272);
#else
    return run_tests_in_sized_window(do_test_debug, 800, 360);
#endif
}

DEFINE_GENERIC_TEST_RUNNER(do_test_debug)

/*-----------------------------------------------------------------------*/

TEST_INIT(init)
{
    time_init();
    debug_init();
    input_init();
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
    input_cleanup();
    debug_cleanup();
    graphics_flush_resources();
    return 1;
}

/*************************************************************************/
/*************** Test routines: Debug rendering primitives ***************/
/*************************************************************************/

TEST(test_fill_box)
{
    const int width = 64;
    const int height = 64;
    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(width * height * 4, 0, MEM_ALLOC_TEMP));

    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    debug_fill_box(16, graphics_display_height()-48, 32, 32,
                   &(Vector4f){1, 0, 1, 0.6});
    TEST_debug_capture_frame(0, 0, width, height, pixels);
    graphics_finish_frame();
    for (int i = 0; i < width*height*4; i += 4) {
        const int x = (i/4) % width, y = (i/4) / width;
        const int p = (x >= 16 && x < 48 && y >= 16 && y < 48) ? 153 : 0;
        CHECK_PIXEL(&pixels[i], p,0,p,255, x, y);
    }

    mem_free(pixels);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fill_box_zero_size)
{
    const int width = 64;
    const int height = 64;
    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(width * height * 4, 0, MEM_ALLOC_TEMP));

    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    debug_fill_box(16, graphics_display_height()-48, 32, 32,
                   &(Vector4f){1, 0, 1, 0.6});
    debug_fill_box(32, graphics_display_height()-32, 0, 32,
                   &(Vector4f){1, 0, 1, 0.6});
    debug_fill_box(32, graphics_display_height()-32, 32, 0,
                   &(Vector4f){1, 0, 1, 0.6});
    TEST_debug_capture_frame(0, 0, width, height, pixels);
    graphics_finish_frame();
    for (int i = 0; i < width*height*4; i += 4) {
        const int x = (i/4) % width, y = (i/4) / width;
        const int p = (x >= 16 && x < 48 && y >= 16 && y < 48) ? 153 : 0;
        CHECK_PIXEL(&pixels[i], p,0,p,255, x, y);
    }

    mem_free(pixels);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fill_box_overflow)
{
    const int width = 64;
    const int height = 64;
    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(width * height * 4, 0, MEM_ALLOC_TEMP));

    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    for (int y = 16; y < 49; y += 3) {
        for (int x = 16; x < 49; x += 3) {
            debug_fill_box(x, graphics_display_height()-(y+3), 3, 3,
                           &(Vector4f){1, 0, 1, 0.6});
        }
    }
    TEST_debug_capture_frame(0, 0, width, height, pixels);
    graphics_finish_frame();
    for (int i = 0; i < width*height*4; i += 4) {
        const int x = (i/4) % width, y = (i/4) / width;
        const int p = (x >= 16 && x < 49 && y >= 16 && y < 49) ? 153 : 0;
        CHECK_PIXEL(&pixels[i], p,0,p,255, x, y);
    }

    mem_free(pixels);
    return 1;
}

/*-----------------------------------------------------------------------*/

#ifdef IMMEDIATE_RENDER_ALLOCS_MEMORY

TEST(test_fill_box_memory_failure)
{
    const int width = 64;
    const int height = 64;
    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(width * height * 4, 0, MEM_ALLOC_TEMP));

    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    debug_fill_box(16, graphics_display_height()-48, 32, 32,
                   &(Vector4f){1, 0, 1, 0.6});
    TEST_debug_capture_frame(0, 0, width, height, pixels);
    TEST_mem_fail_after(0, 1, 0);
    graphics_finish_frame();
    TEST_mem_fail_after(-1, 0, 0);
    for (int i = 0; i < width*height*4; i += 4) {
        const int x = (i/4) % width, y = (i/4) / width;
        CHECK_PIXEL(&pixels[i], 0,0,0,255, x, y);
    }

    mem_free(pixels);
    return 1;
}

#endif  // IMMEDIATE_RENDER_ALLOCS_MEMORY

/*-----------------------------------------------------------------------*/

TEST(test_fill_box_not_initted)
{
    const int width = 64;
    const int height = 64;
    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(width * height * 4, 0, MEM_ALLOC_TEMP));

    debug_cleanup();

    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    debug_fill_box(16, graphics_display_height()-48, 32, 32,
                   &(Vector4f){1, 0, 1, 0.6});
    TEST_debug_capture_frame(0, 0, width, height, pixels);
    graphics_finish_frame();
    for (int i = 0; i < width*height*4; i += 4) {
        const int x = (i/4) % width, y = (i/4) / width;
        CHECK_PIXEL(&pixels[i], 0,0,0,255, x, y);
    }

    mem_free(pixels);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_draw_text)
{
    const int width = 24;
    const int height = 12;
    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(width * height * 4, 0, MEM_ALLOC_TEMP));

    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_INTEQUAL(debug_draw_text(4, graphics_display_height()-10, 1,
                                   &(Vector4f){1, 0, 1, 0.6}, "CPU:"),
                   text_scale(24));
    TEST_debug_capture_frame(0, 0, width, height, pixels);
    graphics_finish_frame();
    if (text_scale(100) == 75) {
        for (int i = 0; i < width*height*4; i += 4) {
            /* Use top-to-bottom Y coordinates here for clarity. */
            const int x = (i/4) % width, y = (height-1) - ((i/4) / width);
            if ((x >= 4 && x < 22) && (y >= 2 && y < 10)) {
                const int p = iroundf(cpu_text[y-2][x-4] * 0.6f);
                CHECK_PIXEL_NEAR(&pixels[i], p,0,p,255, NEAR, x, y);
            } else {
                CHECK_PIXEL(&pixels[i], 0,0,0,255, x, y);
            }
        }
    } else {
        DLOG("Skipping pixel comparison (display size too large)");
    }

    mem_free(pixels);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_draw_text_format)
{
    const int width = 24;
    const int height = 12;
    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(width * height * 4, 0, MEM_ALLOC_TEMP));

    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_INTEQUAL(debug_draw_text(4, graphics_display_height()-10, 1,
                                   &(Vector4f){1, 0, 1, 0.6}, "%s:", "CPU"),
                   text_scale(24));
    TEST_debug_capture_frame(0, 0, width, height, pixels);
    graphics_finish_frame();
    if (text_scale(100) == 75) {
        for (int i = 0; i < width*height*4; i += 4) {
            const int x = (i/4) % width, y = (height-1) - ((i/4) / width);
            if ((x >= 4 && x < 22) && (y >= 2 && y < 10)) {
                const int p = iroundf(cpu_text[y-2][x-4] * 0.6f);
                CHECK_PIXEL_NEAR(&pixels[i], p,0,p,255, NEAR, x, y);
            } else {
                CHECK_PIXEL(&pixels[i], 0,0,0,255, x, y);
            }
        }
    } else {
        DLOG("Skipping pixel comparison (display size too large)");
    }

    mem_free(pixels);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_draw_text_space)
{
    const int width = 32;
    const int height = 12;
    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(width * height * 4, 0, MEM_ALLOC_TEMP));

    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_INTEQUAL(debug_draw_text(4, graphics_display_height()-10, 1,
                                   &(Vector4f){1, 0, 1, 0.6}, "CPU: "),
                   text_scale(27));
    TEST_debug_capture_frame(0, 0, width, height, pixels);
    graphics_finish_frame();
    if (text_scale(100) == 75) {
        for (int i = 0; i < width*height*4; i += 4) {
            const int x = (i/4) % width, y = (height-1) - ((i/4) / width);
            if ((x >= 4 && x < 22) && (y >= 2 && y < 10)) {
                const int p = iroundf(cpu_text[y-2][x-4] * 0.6f);
                CHECK_PIXEL_NEAR(&pixels[i], p,0,p,255, NEAR, x, y);
            } else {
                CHECK_PIXEL(&pixels[i], 0,0,0,255, x, y);
            }
        }
    } else {
        DLOG("Skipping pixel comparison (display size too large)");
    }

    mem_free(pixels);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_draw_text_alignment)
{
    const int width = 48;
    const int height = 12;
    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(width * height * 4, 0, MEM_ALLOC_TEMP));

    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_INTEQUAL(debug_draw_text(13, graphics_display_height()-10, 0,
                                   &(Vector4f){1, 0, 1, 0.6}, "CPU:"),
                   text_scale(24));
    TEST_debug_capture_frame(0, 0, width, height, pixels);
    graphics_finish_frame();
    if (text_scale(100) == 75) {
        for (int i = 0; i < width*height*4; i += 4) {
            const int x = (i/4) % width, y = (height-1) - ((i/4) / width);
            if ((x >= 4 && x < 22) && (y >= 2 && y < 10)) {
                const int p = iroundf(cpu_text[y-2][x-4] * 0.6f);
                CHECK_PIXEL_NEAR(&pixels[i], p,0,p,255, NEAR, x, y);
            } else {
                CHECK_PIXEL(&pixels[i], 0,0,0,255, x, y);
            }
        }
    } else {
        DLOG("Skipping pixel comparison (display size too large)");
    }

    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_INTEQUAL(debug_draw_text(22, graphics_display_height()-10, -1,
                                   &(Vector4f){1, 0, 1, 0.6}, "CPU:"),
                   text_scale(24));
    TEST_debug_capture_frame(0, 0, width, height, pixels);
    graphics_finish_frame();
    if (text_scale(100) == 75) {
        for (int i = 0; i < width*height*4; i += 4) {
            const int x = (i/4) % width, y = (height-1) - ((i/4) / width);
            if ((x >= 4 && x < 22) && (y >= 2 && y < 10)) {
                const int p = iroundf(cpu_text[y-2][x-4] * 0.6f);
                CHECK_PIXEL_NEAR(&pixels[i], p,0,p,255, NEAR, x, y);
            } else {
                CHECK_PIXEL(&pixels[i], 0,0,0,255, x, y);
            }
        }
    } else {
        DLOG("Skipping pixel comparison (display size too large)");
    }

    mem_free(pixels);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_draw_text_empty_char)
{
    const int width = 24;
    const int height = 12;
    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(width * height * 4, 0, MEM_ALLOC_TEMP));

    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_INTEQUAL(debug_draw_text(4, graphics_display_height()-10, 1,
                                   &(Vector4f){1, 0, 1, 0.6}, "CPU\1:"),
                   text_scale(24));
    TEST_debug_capture_frame(0, 0, width, height, pixels);
    graphics_finish_frame();
    if (text_scale(100) == 75) {
        for (int i = 0; i < width*height*4; i += 4) {
            const int x = (i/4) % width, y = (height-1) - ((i/4) / width);
            if ((x >= 4 && x < 22) && (y >= 2 && y < 10)) {
                const int p = iroundf(cpu_text[y-2][x-4] * 0.6f);
                CHECK_PIXEL_NEAR(&pixels[i], p,0,p,255, NEAR, x, y);
            } else {
                CHECK_PIXEL(&pixels[i], 0,0,0,255, x, y);
            }
        }
    } else {
        DLOG("Skipping pixel comparison (display size too large)");
    }

    mem_free(pixels);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_draw_text_out_of_range_char)
{
    const int width = 24;
    const int height = 12;
    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(width * height * 4, 0, MEM_ALLOC_TEMP));

    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_INTEQUAL(debug_draw_text(4, graphics_display_height()-10, 1,
                                   &(Vector4f){1, 0, 1, 0.6},
                                   "CPU\xE2\x80\x94:"), text_scale(24));
    TEST_debug_capture_frame(0, 0, width, height, pixels);
    graphics_finish_frame();
    if (text_scale(100) == 75) {
        for (int i = 0; i < width*height*4; i += 4) {
            const int x = (i/4) % width, y = (height-1) - ((i/4) / width);
            if ((x >= 4 && x < 22) && (y >= 2 && y < 10)) {
                const int p = iroundf(cpu_text[y-2][x-4] * 0.6f);
                CHECK_PIXEL_NEAR(&pixels[i], p,0,p,255, NEAR, x, y);
            } else {
                CHECK_PIXEL(&pixels[i], 0,0,0,255, x, y);
            }
        }
    } else {
        DLOG("Skipping pixel comparison (display size too large)");
    }

    mem_free(pixels);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_draw_text_invalid_char)
{
    const int width = 24;
    const int height = 12;
    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(width * height * 4, 0, MEM_ALLOC_TEMP));

    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_INTEQUAL(debug_draw_text(4, graphics_display_height()-10, 1,
                                   &(Vector4f){1, 0, 1, 0.6}, "CPU\x80:"),
                   text_scale(24));
    TEST_debug_capture_frame(0, 0, width, height, pixels);
    graphics_finish_frame();
    if (text_scale(100) == 75) {
        for (int i = 0; i < width*height*4; i += 4) {
            const int x = (i/4) % width, y = (height-1) - ((i/4) / width);
            if ((x >= 4 && x < 22) && (y >= 2 && y < 10)) {
                const int p = iroundf(cpu_text[y-2][x-4] * 0.6f);
                CHECK_PIXEL_NEAR(&pixels[i], p,0,p,255, NEAR, x, y);
            } else {
                CHECK_PIXEL(&pixels[i], 0,0,0,255, x, y);
            }
        }
    } else {
        DLOG("Skipping pixel comparison (display size too large)");
    }

    mem_free(pixels);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_draw_text_overflow)
{
    const int width = 24;
    const int height = 12;
    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(width * height * 4, 0, MEM_ALLOC_TEMP));

    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    for (int i = 0; i < 256; i++) {
        CHECK_INTEQUAL(debug_draw_text(4, graphics_display_height()-10, 1,
                                       &(Vector4f){1, 0, 1, 0.6}, "CPU:"),
                       text_scale(24));
    }
    TEST_debug_capture_frame(0, 0, width, height, pixels);
    graphics_finish_frame();
    if (text_scale(100) == 75) {
        for (int i = 0; i < width*height*4; i += 4) {
            const int x = (i/4) % width, y = (height-1) - ((i/4) / width);
            if ((x >= 4 && x < 22) && (y >= 2 && y < 10)) {
                const int p = cpu_text[y-2][x-4];
                if (p == 0) {
                    CHECK_PIXEL(&pixels[i], 0,0,0,255, x, y);
                } else if (p >= 10) {
                    CHECK_PIXEL_NEAR(&pixels[i], 255,0,255,255, 64, x, y);
                } else {
                    /* For nearly-but-not-completely-transparent pixels,
                     * rounding means we could get pretty much anything, so
                     * skip the check. */
                }
            } else {
                CHECK_PIXEL(&pixels[i], 0,0,0,255, x, y);
            }
        }
    } else {
        DLOG("Skipping pixel comparison (display size too large)");
    }

    mem_free(pixels);
    return 1;
}

/*-----------------------------------------------------------------------*/

#ifdef IMMEDIATE_RENDER_ALLOCS_MEMORY

TEST(test_draw_text_memory_failure)
{
    const int width = 24;
    const int height = 12;
    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(width * height * 4, 0, MEM_ALLOC_TEMP));

    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_INTEQUAL(debug_draw_text(4, graphics_display_height()-10, 1,
                                   &(Vector4f){1, 0, 1, 0.6}, "CPU:"),
                   text_scale(24));
    TEST_debug_capture_frame(0, 0, width, height, pixels);
    TEST_mem_fail_after(0, 1, 0);
    graphics_finish_frame();
    TEST_mem_fail_after(-1, 0, 0);
    for (int i = 0; i < width*height*4; i += 4) {
        const int x = (i/4) % width, y = (i/4) / width;
        CHECK_PIXEL(&pixels[i], 0,0,0,255, x, y);
    }

    mem_free(pixels);
    return 1;
}

#endif  // IMMEDIATE_RENDER_ALLOCS_MEMORY

/*-----------------------------------------------------------------------*/

TEST(test_draw_text_not_initted)
{
    const int width = 24;
    const int height = 12;
    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(width * height * 4, 0, MEM_ALLOC_TEMP));

    debug_cleanup();

    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_INTEQUAL(debug_draw_text(4, graphics_display_height()-10, 1,
                                   &(Vector4f){1, 0, 1, 0.6}, "CPU:"), 0);
    TEST_debug_capture_frame(0, 0, width, height, pixels);
    graphics_finish_frame();
    for (int i = 0; i < width*height*4; i += 4) {
        const int x = (i/4) % width, y = (i/4) / width;
        CHECK_PIXEL(&pixels[i], 0,0,0,255, x, y);
    }

    mem_free(pixels);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_text_width)
{
    CHECK_INTEQUAL(debug_text_width("CPU:", 0), text_scale(24));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_text_width_len)
{
    CHECK_INTEQUAL(debug_text_width("CPU:", 3), text_scale(21));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_text_width_empty_char)
{
    CHECK_INTEQUAL(debug_text_width("CPU\1:", 0), text_scale(24));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_text_width_out_of_range_char)
{
    CHECK_INTEQUAL(debug_text_width("CPU\xE2\x80\x94:", 0), text_scale(24));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_text_width_invalid_char)
{
    CHECK_INTEQUAL(debug_text_width("CPU\x80:", 0), text_scale(24));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_text_width_not_initted)
{
    debug_cleanup();
    CHECK_INTEQUAL(debug_text_width("CPU:", 0), 0);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_text_height)
{
    CHECK_INTEQUAL(debug_text_height(), text_scale(12));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_text_height_large_window)
{
    if (!graphics_has_windowed_mode()) {
        SKIP("No windowed mode on this platform.");
    }

    if (!graphics_set_display_mode(1440, 900, 0)) {
        SKIP("Failed to set a large window size.");
    }
    graphics_start_frame();
    CHECK_INTEQUAL(debug_text_height(), 15);
    graphics_finish_frame();

    ASSERT(graphics_set_display_mode(800, 360, 0));
    return 1;
}

/*************************************************************************/
/*************** Test routines: Debug interface rendering ****************/
/*************************************************************************/

TEST(test_debug_interface_activate)
{
    const int width = graphics_display_width();
    const int height = graphics_display_height();
    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(width * height * 4, 0, MEM_ALLOC_TEMP));

    debug_interface_activate(1);
    CHECK_TRUE(debug_interface_is_active());
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    TEST_debug_capture_frame(0, 0, width, height, pixels);
    graphics_finish_frame();
    for (int i = 0; i < width*height*4; i += 4) {
        /* Calculate Y with the origin in the upper-left, like the debug
         * interface assumes. */
        const int x = (i/4) % width, y = (height-1) - (i/4) / width;
        if (y < iroundf(height*0.79f)) {
            CHECK_PIXEL(&pixels[i], 85,0,0,255, x, y);
        } else if (y < ifloorf(height*0.80f) || y > iceilf(height*0.97f)) {
            CHECK_PIXEL(&pixels[i], 21,0,0,255, x, y);
        }
    }

    debug_interface_activate(0);
    CHECK_FALSE(debug_interface_is_active());
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    TEST_debug_capture_frame(0, 0, width, height, pixels);
    graphics_finish_frame();
    for (int i = 0; i < width*height*4; i += 4) {
        const int x = (i/4) % width, y = (i/4) / width;
        CHECK_PIXEL(&pixels[i], 0,0,0,255, x, y);
    }

    debug_cleanup();
    debug_interface_activate(1);
    CHECK_FALSE(debug_interface_is_active());

    mem_free(pixels);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_cpu_usage)
{
    static const struct {
        int start;      // Frame start time, in system timer ticks.
        int end;        // Frame end time, in system timer ticks.
        int expect_pct; // Expected CPU % shown (0, 100, or -1 for "---.-%").
    } test_frames[] = {
        {  0,  1,   0},
        {  4,  5,   0},
        {  8,  9,   0},
        { 12, 13,   0},
        { 16, 17,   0},
        { 20, 21,   0},
        { 24, 25,   0},
        { 28, 29,   0},
        { 32, 33, 100},
        { 36, 37, 100},
        { 40, 41, 100},
        { 44, 45, 100},
        { 48, 49, 100},
        { 52, 53, 100},
        { 56, 57, 100},
        { 60, 61, 100},
        { 64,104, 100},
        {104,144,  -1},
    };

    const int space = text_scale(2);
    const int width = graphics_display_width();
    const int height = 2*space + text_scale(12) + 2;
    const int bar_x = 2*space + text_scale(67);

    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(width * height * 4, 0, MEM_ALLOC_TEMP));

    ASSERT(sys_time_unit() == 64); // Tests are written to this value.
    const float time_unit = 1.0f / sys_time_unit();
    const float frame_period = 4 * time_unit;
    TEST_debug_set_frame_period(frame_period);
    float cpu_average[2];  // Red/green levels we expect to see on the meter.

    debug_show_cpu_usage(1);
    CHECK_TRUE(debug_cpu_usage_is_on());
    sys_test_time_set(0);
    graphics_start_frame();
    graphics_clear(0.8, 0, 0.8, 0, 1, 0);
    TEST_debug_capture_frame(0, 0, width, height, pixels);
    sys_test_time_set(1);
    graphics_finish_frame();
    cpu_average[0] = cpu_average[1] = 0;
    for (int i = 0; i < width*height*4; i += 4) {
        /* Nothing will be drawn for the first frame since time_now()==0. */
        const int x = (i/4) % width, y = (i/4) / width;
        CHECK_PIXEL(&pixels[i], 204,0,204,255, x, y);
    }

    for (int frame = 1; frame < lenof(test_frames); frame++) {
        sys_test_time_set(test_frames[frame].start);
        graphics_start_frame();
        graphics_clear(0.8, 0, 0.8, 0, 1, 0);
        TEST_debug_capture_frame(0, 0, width, height, pixels);
        sys_test_time_set(test_frames[frame].end);
        graphics_finish_frame();
        /* Each frame, we update our expected CPU meter levels based on
         * what we expect the debug interface to compute for the previous
         * frame (since the debug interface itself lags by one frame when
         * calculating CPU time).  However, the first frame is ignored for
         * CPU time averaging, so we likewise ignore the first frame in
         * our calculations. */
        if (frame > 1) {
            const float frame_time =
                (test_frames[frame].start - test_frames[frame-1].start)
                * time_unit;
            const float render_time =
                (test_frames[frame-1].end - test_frames[frame-1].start)
                * time_unit;
            const float process_time = frame_time - render_time;
            float usage[2] = {render_time / frame_period,
                              process_time / frame_period};
            for (int i = 0; i < 2; i++) {
                float factor = 1 - powf(0.2f, frame_time);
                if (usage[i] > cpu_average[i]*1.5f) {
                    factor *= lbound((cpu_average[i]*1.5f) / usage[i], 0.5f);
                }
                cpu_average[i] = cpu_average[i]*(1-factor) + usage[i]*factor;
            }
        }
        for (int i = 0; i < width*height*4; i += 4) {
            const int x = (i/4) % width;
            /* Make Y relative to bottom of display. */
            const int y = ((height-1) - ((i/4) / width)) - height;
            if (y < -(2*space + text_scale(12))) {
                CHECK_PIXEL(&pixels[i], 204,0,204,255, x, y);
            } else if (y < -(space + text_scale(12)) || y >= -space) {
                CHECK_PIXEL(&pixels[i], 51,0,51,255, x, y);
            } else if (x >= bar_x) {
                int is_tick = 0;
                for (int j = 1; j <= 9; j++) {
                    if (x == bar_x + ((width-bar_x)*j+5)/10) {
                        is_tick = 1;
                        break;
                    }
                }
                if (is_tick) {
                    CHECK_PIXEL(&pixels[i], 0,0,0,255, x, y);
                } else if (x-bar_x < iroundf(cpu_average[0] * (width-bar_x))) {
                    CHECK_PIXEL(&pixels[i], 255,0,0,255, x, y);
                } else if (x-bar_x < iroundf((cpu_average[0] + cpu_average[1])
                                             * (width-bar_x))) {
                    CHECK_PIXEL(&pixels[i], 0,255,0,255, x, y);
                } else {
                    CHECK_PIXEL(&pixels[i], 85,85,85,255, x, y);
                }
            } else if (text_scale(100) != 75) {
                /* Skip text check on large displays. */
            } else if ((x >= 2 && x < 20) && (y >= -11 && y < -3)) {
                const int level = cpu_text[y+11][x-2];
                const int level2 = iroundf(51 + level*0.8f);
                CHECK_PIXEL_NEAR(&pixels[i], level2,level,level2,255, NEAR,
                                 x, y);
            } else if (test_frames[frame].expect_pct == 0
                       && (x >= 33 && x < 53) && (y >= -11 && y < -3)) {
                const int level = pct_0_text[y+11][x-33];
                const int level2 = iroundf(51 + level*0.8f);
                CHECK_PIXEL_NEAR(&pixels[i], level2,level,level2,255, NEAR,
                                 x, y);
            } else if (test_frames[frame].expect_pct == 100
                       && (x >= 24 && x < 52) && (y >= -11 && y < -3)) {
                const int level = pct_100_text[y+11][x-24];
                const int level2 = iroundf(51 + level*0.8f);
                CHECK_PIXEL_NEAR(&pixels[i], level2,level,level2,255, NEAR,
                                 x, y);
            } else if (test_frames[frame].expect_pct == -1
                       && (x >= 30 && x < 52) && (y >= -11 && y < -3)) {
                const int level = pct_inf_text[y+11][x-30];
                const int level2 = iroundf(51 + level*0.8f);
                CHECK_PIXEL_NEAR(&pixels[i], level2,level,level2,255, NEAR,
                                 x, y);
            } else {
                CHECK_PIXEL(&pixels[i], 51,0,51,255, x, y);
            }
        }  // for each pixel
    }  // for each frame

    debug_show_cpu_usage(0);
    CHECK_FALSE(debug_cpu_usage_is_on());
    graphics_start_frame();
    graphics_clear(0.8, 0, 0.8, 0, 1, 0);
    TEST_debug_capture_frame(0, 0, width, height, pixels);
    graphics_finish_frame();
    for (int i = 0; i < width*height*4; i += 4) {
        const int x = (i/4) % width, y = (i/4) / width;
        CHECK_PIXEL(&pixels[i], 204,0,204,255, x, y);
    }

    debug_cleanup();
    debug_show_cpu_usage(1);
    CHECK_FALSE(debug_cpu_usage_is_on());

    mem_free(pixels);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_memory_usage)
{
    const int space = text_scale(2);
    const int width = graphics_display_width();
    const int height = 2*space + text_scale(12) + 2;

    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(width * height * 4, 0, MEM_ALLOC_TEMP));

    debug_show_memory_usage(1);
    CHECK_TRUE(debug_memory_usage_is_on());
    sys_test_debug_set_memory_stats(
        (int64_t)10 << 30, (int64_t)2 << 30, (int64_t)3 << 30);
    graphics_start_frame();
    graphics_clear(0.8, 0, 0.8, 0, 1, 0);
    TEST_debug_capture_frame(
        0, graphics_display_height()-height, width, height, pixels);
    graphics_finish_frame();
    const int system_width = debug_text_width("System: 5242880k", 0);
    const int avail_width = debug_text_width("Avail: 3145728k", 0);
    const int self_width = debug_text_width("Used: 2097152k", 0);
    const int avail_x = width/2 + ((width*3+5)/10)/2 - avail_width/2;
    const int self_x = width - (space + self_width);
    for (int i = 0; i < width*height*4; i += 4) {
        const int x = (i/4) % width, y = (height-1) - ((i/4) / width);
        if (y >= 2*space + text_scale(12)) {
            CHECK_PIXEL(&pixels[i], 204,0,204,255, x, y);
        } else if (y < space || y >= space + text_scale(12)) {
            if (text_scale(100) != 75
             && (y == space-1 || y == space + text_scale(12))) {
                continue;  // Could be leakage from scaled text on these lines.
            }
            CHECK_PIXEL(&pixels[i], 51,0,51,255, x, y);
        } else if (x >= space - 1 && x < space + system_width + 1) {
            if (text_scale(100) == 75) {
                const int level = system_text[y-space][x-(space-1)];
                const int level2 = iroundf(204 + level*0.2f);
                CHECK_PIXEL_NEAR(&pixels[i], level,level,level2,255, NEAR,
                                 x, y);
            }
        } else if (x >= avail_x - 1 && x < avail_x + avail_width + 1) {
            if (text_scale(100) == 75) {
                const int level =
                    y==10 ? 0 : avail_text[y-space][x-(avail_x-1)];
                const int level2 = iroundf(51 + level*0.8f);
                CHECK_PIXEL_NEAR(&pixels[i], level2,level,level2,255, NEAR,
                                 x, y);
            }
        } else if (x >= self_x - 1 && x < self_x + self_width + 1) {
            if (text_scale(100) == 75) {
                const int b = y==10 ? 0 : self_text[y-space][x-(self_x-1)];
                const int r = iroundf(153 + b*0.4f);
                const int g = iroundf(102 + b*0.6f);
                CHECK_PIXEL_NEAR(&pixels[i], r,g,b,255, NEAR, x, y);
            }
        } else if (x < (width+1)/2) {
            CHECK_PIXEL(&pixels[i], 0,0,204,255, x, y);
        } else if (x >= (width*8+5)/10) {
            CHECK_PIXEL(&pixels[i], 153,102,0,255, x, y);
        } else {
            CHECK_PIXEL(&pixels[i], 51,0,51,255, x, y);
        }
    }

    sys_test_debug_set_memory_stats(
        (int64_t)5 << 30, (int64_t)2 << 30, (int64_t)3 << 30);
    graphics_start_frame();
    graphics_clear(0.8, 0, 0.8, 0, 1, 0);
    TEST_debug_capture_frame(
        0, graphics_display_height()-height, width, height, pixels);
    graphics_finish_frame();
    for (int i = 0; i < width*height*4; i += 4) {
        const int x = (i/4) % width, y = (height-1) - ((i/4) / width);
        if (y >= 2*space + text_scale(12)) {
            CHECK_PIXEL(&pixels[i], 204,0,204,255, x, y);
        } else if (y < space || y >= space + text_scale(12)) {
            if (text_scale(100) != 75
             && (y == space-1 || y == space + text_scale(12))) {
                continue;  // Could be leakage from scaled text on these lines.
            }
            CHECK_PIXEL(&pixels[i], 51,0,51,255, x, y);
        } else if (x >= space - 1 && x < space + avail_width + 1) {
            if (text_scale(100) == 75) {
                const int level = y==10 ? 0 : avail_text[y-space][x-(space-1)];
                const int level2 = iroundf(51 + level*0.8f);
                CHECK_PIXEL_NEAR(&pixels[i], level2,level,level2,255, NEAR,
                                 x, y);
            }
        } else if (x >= self_x - 1 && x < self_x + self_width + 1) {
            if (text_scale(100) == 75) {
                const int b = y==10 ? 0 : self_text[y-space][x-(self_x-1)];
                const int r = iroundf(153 + b*0.4f);
                const int g = iroundf(102 + b*0.6f);
                CHECK_PIXEL_NEAR(&pixels[i], r,g,b,255, NEAR, x, y);
            }
        } else if (x >= iroundf(width*0.6f)) {
            CHECK_PIXEL(&pixels[i], 153,102,0,255, x, y);
        } else {
            CHECK_PIXEL(&pixels[i], 51,0,51,255, x, y);
        }
    }

    sys_test_debug_fail_memory_stats();
    graphics_start_frame();
    graphics_clear(0.8, 0, 0.8, 0, 1, 0);
    TEST_debug_capture_frame(0, 0, width, height, pixels);
    graphics_finish_frame();
    for (int i = 0; i < width*height*4; i += 4) {
        const int x = (i/4) % width, y = (height-1) - ((i/4) / width);
        CHECK_PIXEL(&pixels[i], 204,0,204,255, x, y);
    }

    debug_show_memory_usage(0);
    CHECK_FALSE(debug_memory_usage_is_on());
    graphics_start_frame();
    graphics_clear(0.8, 0, 0.8, 0, 1, 0);
    TEST_debug_capture_frame(0, 0, width, height, pixels);
    graphics_finish_frame();
    for (int i = 0; i < width*height*4; i += 4) {
        const int x = (i/4) % width, y = (height-1) - ((i/4) / width);
        CHECK_PIXEL(&pixels[i], 204,0,204,255, x, y);
    }

    debug_cleanup();
    debug_show_memory_usage(1);
    CHECK_FALSE(debug_memory_usage_is_on());

    mem_free(pixels);
    return 1;
}

/*************************************************************************/
/************** Test routines: Debug interface interaction ***************/
/*************************************************************************/

TEST(test_toggle_debug_interface_touch)
{
    /* Increment the time by one tick per frame so the debug interface
     * detects the passage of time. */
    int time = 0;

    /* By default, touch input should not trigger the debug interface. */
    sys_test_input_touch_down(0, 0, 0);
    sys_test_input_touch_down(1, 0, 1);
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_FALSE(debug_interface_is_active());
    sys_test_input_touch_up(0);
    sys_test_input_touch_up(1);
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_FALSE(debug_interface_is_active());

    debug_interface_enable_auto(1);

    /* Touches in the upper-left and lower-left corners should now activate
     * the debug interface. */
    sys_test_input_touch_down(0, 0, 0);
    sys_test_input_touch_down(1, 0, 1);
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_TRUE(debug_interface_is_active());

    /* Lifting one touch should leave the interface active. */
    sys_test_input_touch_up(0);
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_TRUE(debug_interface_is_active());

    /* Moving the remaining touch out of the corner should leave the
     * interface active. */
    sys_test_input_touch_move_to(1, 0.5, 0.5);
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_TRUE(debug_interface_is_active());

    /* Adding a third touch and lifting the second touch should leave the
     * interface active. */
    sys_test_input_touch_down(2, 0, 0.5);
    sys_test_input_touch_up(1);
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_TRUE(debug_interface_is_active());

    /* Lifting the last touch should deactivate the interface. */
    sys_test_input_touch_up(2);
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_FALSE(debug_interface_is_active());

    /* Touches in the upper-left and lower-left corners should activate
     * the debug interface even if they don't occur at the same time. */
    sys_test_input_touch_down(0, 0, 0);
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_FALSE(debug_interface_is_active());
    sys_test_input_touch_down(1, 0, 1);
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_TRUE(debug_interface_is_active());

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_toggle_debug_interface_mouse)
{
    int time = 0;

    input_mouse_set_position(0.5, 0.5);

    /* By default, mouse input should not trigger the debug interface. */
    sys_test_input_press_mouse_buttons(1, 1, 1);
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_FALSE(debug_interface_is_active());
    sys_test_input_release_mouse_buttons(1, 1, 1);
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_FALSE(debug_interface_is_active());

    debug_interface_enable_auto(1);

    /* Clicking all three buttons should now toggle the debug interface. */
    sys_test_input_press_mouse_buttons(1, 1, 1);
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_TRUE(debug_interface_is_active());

    /* Leaving the mouse buttons pressed should not change anything. */
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_TRUE(debug_interface_is_active());

    /* Releasing the mouse buttons should leave the debug interface active. */
    sys_test_input_release_mouse_buttons(1, 1, 1);
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_TRUE(debug_interface_is_active());

    /* Clicking all three buttons should toggle the debug interface even
     * if the clicks don't happen at the same time. */
    sys_test_input_press_mouse_buttons(1, 0, 0);
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_TRUE(debug_interface_is_active());
    sys_test_input_press_mouse_buttons(0, 1, 0);
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_TRUE(debug_interface_is_active());
    sys_test_input_press_mouse_buttons(0, 0, 1);
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_FALSE(debug_interface_is_active());

    return 1;
}

/*-----------------------------------------------------------------------*/

/* This test checks general touch input handling along with basic behavior
 * of the memory usage meter button. */
TEST(test_button_touch)
{
    int time = 0;
    debug_interface_enable_auto(1);

    sys_test_input_touch_down(0, 0, 0);
    sys_test_input_touch_down(1, 0, 1);
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_TRUE(debug_interface_is_active());
    CHECK_FALSE(debug_memory_usage_is_on());

    /* Tapping a button should immediately activate it. */
    sys_test_input_touch_down(2, 0.87, 0.84);
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_TRUE(debug_interface_is_active());
    CHECK_TRUE(debug_memory_usage_is_on());

    /* Moving the touch within the button should have no effect. */
    sys_test_input_touch_move_to(2, 0.86, 0.83);
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_TRUE(debug_interface_is_active());
    CHECK_TRUE(debug_memory_usage_is_on());

    /* Moving the touch outside the button should have no effect. */
    sys_test_input_touch_move_to(2, 0.70, 0.70);
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_TRUE(debug_interface_is_active());
    CHECK_TRUE(debug_memory_usage_is_on());

    /* Releasing the touch should have no effect. */
    sys_test_input_touch_up(2);
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_TRUE(debug_interface_is_active());
    CHECK_TRUE(debug_memory_usage_is_on());

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_button_touch_overflow)
{
    int time = 0;
    debug_interface_enable_auto(1);

    sys_test_input_touch_down(0, 0, 0);
    sys_test_input_touch_down(1, 0, 1);
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_TRUE(debug_interface_is_active());
    CHECK_FALSE(debug_memory_usage_is_on());

    for (int i = 2; i < INPUT_MAX_TOUCHES; i++) {
        sys_test_input_touch_down(i, 0.5, 0.5);
    }
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_TRUE(debug_interface_is_active());
    CHECK_FALSE(debug_memory_usage_is_on());

    for (int i = 1; i < INPUT_MAX_TOUCHES; i++) {
        sys_test_input_touch_up(i);
    }
    /* Since the debug.c touch array is the same size as the global touch
     * state array in input.c, we need to run two input_update() cycles to
     * trigger an overflow in debug.c: first we clear all existing touches
     * (except one) to free up slots in the global touch table, then we add
     * a new touch which will be detected but discarded by the debug
     * interface (since removed touches are not deleted until after new
     * touches have been checked). */
    input_update();
    sys_test_input_touch_down(INPUT_MAX_TOUCHES, 0.87, 0.84);
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_TRUE(debug_interface_is_active());
    CHECK_FALSE(debug_memory_usage_is_on());

    /* The debug interface checks state only, not events, so it should
     * pick up the button touch this time around without any new events. */
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_TRUE(debug_interface_is_active());
    CHECK_TRUE(debug_memory_usage_is_on());

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_button_mouse)
{
    int time = 0;
    debug_interface_enable_auto(1);

    input_mouse_set_position(0.5, 0.5);
    sys_test_input_press_mouse_buttons(1, 1, 1);
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    sys_test_input_release_mouse_buttons(1, 1, 1);
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_TRUE(debug_interface_is_active());
    CHECK_FALSE(debug_memory_usage_is_on());

    /* Clicking on a button should immediately activate it. */
    input_mouse_set_position(0.87, 0.84);
    sys_test_input_press_mouse_buttons(1, 0, 0);
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_TRUE(debug_interface_is_active());
    CHECK_TRUE(debug_memory_usage_is_on());

    /* Clicking a non-left button should have no effect. */
    sys_test_input_press_mouse_buttons(0, 1, 0);
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_TRUE(debug_interface_is_active());
    CHECK_TRUE(debug_memory_usage_is_on());
    sys_test_input_release_mouse_buttons(0, 1, 0);
    sys_test_input_press_mouse_buttons(0, 0, 1);
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_TRUE(debug_interface_is_active());
    CHECK_TRUE(debug_memory_usage_is_on());

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_memory_button)
{
    int time = 0;
    debug_interface_enable_auto(1);

    sys_test_input_touch_down(0, 0, 0);
    sys_test_input_touch_down(1, 0, 1);
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_TRUE(debug_interface_is_active());
    CHECK_FALSE(debug_memory_usage_is_on());

    /* Tapping the memory button should enable the memory meter. */
    sys_test_input_touch_down(2, 0.87, 0.84);
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_TRUE(debug_interface_is_active());
    CHECK_TRUE(debug_memory_usage_is_on());

    /* Holding the button down should have no effect. */
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_TRUE(debug_interface_is_active());
    CHECK_TRUE(debug_memory_usage_is_on());

    /* Releasing the button should have no effect. */
    sys_test_input_touch_up(2);
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_TRUE(debug_interface_is_active());
    CHECK_TRUE(debug_memory_usage_is_on());

    /* Tapping the memory button again should disable the memory meter. */
    sys_test_input_touch_down(2, 0.87, 0.84);
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_TRUE(debug_interface_is_active());
    CHECK_FALSE(debug_memory_usage_is_on());

    sys_test_input_touch_up(2);
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_TRUE(debug_interface_is_active());
    CHECK_FALSE(debug_memory_usage_is_on());

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_cpu_button)
{
    int time = 0;
    debug_interface_enable_auto(1);

    TEST_debug_set_frame_period(1/16.0f);

    sys_test_input_touch_down(0, 0, 0);
    sys_test_input_touch_down(1, 0, 1);
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_TRUE(debug_interface_is_active());
    CHECK_FALSE(debug_cpu_usage_is_on());

    /* Tapping the CPU button should enable the CPU meter. */
    sys_test_input_touch_down(2, 0.87, 0.93);
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_TRUE(debug_interface_is_active());
    CHECK_TRUE(debug_cpu_usage_is_on());

    /* Holding the button down should have no effect. */
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_TRUE(debug_interface_is_active());
    CHECK_TRUE(debug_cpu_usage_is_on());

    /* Releasing the button should have no effect. */
    sys_test_input_touch_up(2);
    input_update();
    sys_test_time_set(++time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_TRUE(debug_interface_is_active());
    CHECK_TRUE(debug_cpu_usage_is_on());

    /* Tapping the CPU button again should disable the CPU meter but
     * enable logging of CPU usage via DLOG(). */
    sys_test_input_touch_down(2, 0.87, 0.93);
    input_update();
    /* Set a known absolute time so we have predictable log message test. */
    time = 8 * sys_time_unit();
    sys_test_time_set(time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    /* From here on down we add 2 seconds per frame (1 second render and
     * 1 second processing) to ensure that a log message is written on
     * every frame when logging is enabled. */
    time += sys_time_unit();
    sys_test_time_set(time);
    sys_test_time_set(time);
    graphics_finish_frame();
    CHECK_TRUE(debug_interface_is_active());
    CHECK_FALSE(debug_cpu_usage_is_on());

    DLOG("test message");
    sys_test_input_touch_up(2);
    input_update();
    time += sys_time_unit();
    sys_test_time_set(time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    time += sys_time_unit();
    sys_test_time_set(time);
    graphics_finish_frame();
    CHECK_TRUE(debug_interface_is_active());
    CHECK_FALSE(debug_cpu_usage_is_on());
    CHECK_DLOG_TEXT("[11.000] Usage (%%): render=1600.0 debug=0.0"
                    " process=1600.0 GPU=0.0 total=3200.0");

    /* Tapping the CPU button a third time leave the CPU meter disabled
     * and also disable logging of CPU usage. */
    sys_test_input_touch_down(2, 0.87, 0.93);
    input_update();
    time += sys_time_unit();
    sys_test_time_set(time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    time += sys_time_unit();
    sys_test_time_set(time);
    graphics_finish_frame();
    CHECK_TRUE(debug_interface_is_active());
    CHECK_FALSE(debug_cpu_usage_is_on());

    DLOG("test message");
    sys_test_input_touch_up(2);
    input_update();
    time += sys_time_unit();
    sys_test_time_set(time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    time += sys_time_unit();
    sys_test_time_set(time);
    graphics_finish_frame();
    CHECK_TRUE(debug_interface_is_active());
    CHECK_FALSE(debug_cpu_usage_is_on());
    CHECK_DLOG_TEXT("test message");

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_cpu_range)
{
    int time = 0;
    debug_interface_enable_auto(1);

    const int space = text_scale(2);
    const int width = graphics_display_width();
    const int height = 2*space + text_scale(12) + 2;
    const int bar_x = 2*space + text_scale(67);

    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(width * height * 4, 0, MEM_ALLOC_TEMP));

    const float time_unit = 1.0f / sys_time_unit();
    /* Normally this will be exact, but for this test we don't need
     * precise values, so don't worry about what the exact time unit is. */
    const int frame_ticks = iroundf(1/16.0f / time_unit);
    const float frame_period = frame_ticks * time_unit;
    TEST_debug_set_frame_period(frame_period);

    /* Turn on the debug interface and CPU meter.  This takes three frames,
     * so we'll need to mix two frames' time (100% usage) into the average. */
    sys_test_input_touch_down(0, 0, 0);
    sys_test_input_touch_down(1, 0, 1);
    input_update();
    time += frame_ticks;
    sys_test_time_set(time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();

    sys_test_input_touch_down(2, 0.87, 0.93);
    input_update();
    time += frame_ticks;
    sys_test_time_set(time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();

    sys_test_input_touch_up(2);
    input_update();
    time += frame_ticks;
    sys_test_time_set(time);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    CHECK_TRUE(debug_interface_is_active());
    CHECK_TRUE(debug_cpu_usage_is_on());

    const float base_factor = 0.5f * (1 - powf(0.2f, frame_period));
    float cpu_average = 2*base_factor - base_factor*base_factor;

    static const struct {float x, y; int range;} taps[] = {
        {0.53, 0.93,  2},
        {0.53, 0.93,  3},
        {0.53, 0.93,  4},
        {0.53, 0.93,  5},
        {0.53, 0.93,  6},
        {0.53, 0.93,  7},
        {0.53, 0.93,  8},
        {0.53, 0.93,  9},
        {0.53, 0.93, 10},
        {0.53, 0.93, 10},
        {0.27, 0.93,  9},
        {0.27, 0.93,  8},
        {0.27, 0.93,  7},
        {0.27, 0.93,  6},
        {0.27, 0.93,  5},
        {0.27, 0.93,  4},
        {0.27, 0.93,  3},
        {0.27, 0.93,  2},
        {0.27, 0.93,  1},
        {0.27, 0.93,  1},
    };

    for (int tap = 0; tap < lenof(taps); tap++) {
        /* We need to tap, hold (to check for no repeat) and then release,
         * which takes three frames. */
        for (int frame = 0; frame < 3; frame++) {
            DLOG("tap %d frame %d", tap, frame);
            if (frame == 0) {
                sys_test_input_touch_down(2, taps[tap].x, taps[tap].y);
            } else if (frame == 2) {
                sys_test_input_touch_up(2);
            }
            input_update();
            time += frame_ticks;
            sys_test_time_set(time);
            graphics_start_frame();
            graphics_clear(0, 0, 0, 0, 1, 0);
            TEST_debug_capture_frame(0, 0, width, height, pixels);
            graphics_finish_frame();

            const float usage = 1.0f;
            float factor = 1 - powf(0.2f, frame_period);
            if (usage > cpu_average*1.5f) {
                factor *= lbound((cpu_average*1.5f) / usage, 0.5f);
            }
            cpu_average = cpu_average*(1-factor) + usage*factor;
            const float adjusted_cpu = cpu_average / taps[tap].range;

            for (int i = 0; i < width*height*4; i += 4) {
                const int x = (i/4) % width;
                const int y = ((height-1) - ((i/4) / width)) - height;
                if (y < -(space+text_scale(12)) || y >= -space || x < bar_x) {
                    continue;  // We're only interested in the bar.
                }
                int is_tick = 0;
                for (int j = 1; j <= 9; j++) {
                    if (x == bar_x + ((width-bar_x)*j+5)/10) {
                        is_tick = 1;
                        break;
                    }
                }
                if (is_tick) {
                    CHECK_PIXEL(&pixels[i], 0,0,0,255, x, y);
                } else if (x-bar_x < iroundf(adjusted_cpu * (width-bar_x))) {
                    CHECK_PIXEL(&pixels[i], 0,255,0,255, x, y);
                } else {
                    CHECK_PIXEL(&pixels[i], 85,85,85,255, x, y);
                }
            }
        }  // for each pixel
    }

    mem_free(pixels);
    return 1;
}

/*************************************************************************/
/*************************************************************************/
