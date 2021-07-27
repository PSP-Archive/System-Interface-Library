/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/graphics/clear-grab.c: Tests for graphics_clear() and
 * graphics_read_pixels().  These are split out from base.c so they can be
 * run in a single window instead of having to open and close the window
 * for each test (which can be slow).
 */

#include "src/base.h"
#include "src/graphics.h"
#include "src/memory.h"
#include "src/test/base.h"
#include "src/test/graphics/internal.h"

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

static int do_test_graphics_clear_grab(void);
int test_graphics_clear_grab(void)
{
    return run_tests_in_window(do_test_graphics_clear_grab);
}

DEFINE_GENERIC_TEST_RUNNER(do_test_graphics_clear_grab)

TEST_INIT(init)
{
    graphics_start_frame();
    return 1;
}

TEST_CLEANUP(cleanup)
{
    graphics_finish_frame();
    graphics_flush_resources();
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_clear_sync_grab)
{
    const int width = graphics_display_width();
    const int height = graphics_display_height();
    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(width * height * 4, 0, MEM_ALLOC_TEMP));

    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_sync();  // Call it once just to verify that it doesn't break.
    CHECK_TRUE(graphics_read_pixels(0, 0, width, height, pixels));
    for (int i = 0; i < width*height*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 0,0,0,255, (i/4) % width, (i/4) / width);
    }

    mem_free(pixels);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_clear_with_color)
{
    #define TESTCASE(r,g,b,pr,pg,pb)  {__LINE__, r, g, b, {pr, pg, pb, 255}}
    static const struct {int line; float r, g, b; uint8_t pixel[4];} tests[] ={
        TESTCASE( 1.0, 0.0, 0.0,  255,  0,  0),
        TESTCASE( 0.0, 1.0, 0.0,    0,255,  0),
        TESTCASE( 0.0, 0.0, 1.0,    0,  0,255),
        TESTCASE( 1.0, 1.0, 1.0,  255,255,255),
        TESTCASE( 0.2, 0.4, 0.6,   51,102,153),
        TESTCASE(-1.0,-1.0,-1.0,    0,  0,  0),
        TESTCASE( 2.0, 2.0, 2.0,  255,255,255),
    };
    #undef TESTCASE

    const int width = graphics_display_width();
    const int height = graphics_display_height();
    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(width * height * 4, 0, MEM_ALLOC_TEMP));

    for (int test = 0; test < lenof(tests); test++) {
        if (test > 0) {
            /* Don't leave a red screen displayed for a long time if these
             * tests are slow. */
            graphics_finish_frame();
            graphics_start_frame();
        }
        graphics_clear(tests[test].r, tests[test].g, tests[test].b, 0, 1, 0);
        CHECK_TRUE(graphics_read_pixels(0, 0, width, height, pixels));
        for (int i = 0; i < width*height*4; i += 4) {
            CHECK_PIXEL(&pixels[i],
                        tests[test].pixel[0], tests[test].pixel[1],
                        tests[test].pixel[2], tests[test].pixel[3],
                        (i/4) % width, (i/4) / height);
        }
    }

    mem_free(pixels);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_grab_bounds_negative)
{
    graphics_clear(0.2, 0.4, 0.6, 0, 1, 0);
    uint8_t pixels[8*8*4];
    /* graphics_read_pixels() says that areas outside the window are
     * undefined, but we rely on current behavior that pixels with X or Y
     * coordinate less than zero are unchanged. */
    memset(pixels, 1, sizeof(pixels));
    CHECK_TRUE(graphics_read_pixels(-2, -6, 8, 8, pixels));
    for (int i = 0; i < 8*8*4; i += 4) {
        const int x = (i/4) % 8;
        const int y = (i/4) / 8;
        const int r = (x >= 2 && y >= 6 ? 51 : 1);
        const int g = (x >= 2 && y >= 6 ? 102 : 1);
        const int b = (x >= 2 && y >= 6 ? 153 : 1);
        const int a = (x >= 2 && y >= 6 ? 255 : 1);
        CHECK_PIXEL(&pixels[i], r,g,b,a, x, y);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_grab_bounds_negative_2)
{
    graphics_clear(0.2, 0.4, 0.6, 0, 1, 0);
    uint8_t pixels[8*8*4];
    memset(pixels, 1, sizeof(pixels));
    CHECK_TRUE(graphics_read_pixels(-8, -6, 8, 8, pixels));
    CHECK_TRUE(graphics_read_pixels(-6, -8, 8, 8, pixels));
    for (int i = 0; i < 8*8*4; i += 4) {
        const int x = (i/4) % 8;
        const int y = (i/4) / 8;
        CHECK_PIXEL(&pixels[i], 1,1,1,1, x, y);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_grab_bounds_positive)
{
    const int width = graphics_display_width();
    const int height = graphics_display_height();

    graphics_clear(0.2, 0.4, 0.6, 0, 1, 0);
    uint8_t pixels[8*8*4];
    memset(pixels, 1, sizeof(pixels));
    CHECK_TRUE(graphics_read_pixels(width-2, height-6, 8, 8, pixels));
    for (int i = 0; i < 8*8*4; i += 4) {
        const int x = (i/4) % 8;
        const int y = (i/4) / 8;
        if (x < 2 && y < 6) {
            CHECK_PIXEL(&pixels[i], 51,102,153,255, x, y);
        }
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_grab_bounds_positive_2)
{
    const int width = graphics_display_width();
    const int height = graphics_display_height();

    graphics_clear(0.2, 0.4, 0.6, 0, 1, 0);
    uint8_t pixels[8*8*4];
    /* Just check that the calls succeed, since the data is undefined. */
    CHECK_TRUE(graphics_read_pixels(width, 0, 8, 8, pixels));
    CHECK_TRUE(graphics_read_pixels(0, height+4, 8, 8, pixels));
    CHECK_TRUE(graphics_read_pixels(width, height+4, 8, 8, pixels));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_grab_invalid)
{
    CHECK_FALSE(graphics_read_pixels(0, 0, 8, 8, NULL));
    return 1;
}

/*-----------------------------------------------------------------------*/

/* This test is here rather than with the other attribute tests since we
 * render a couple of frames to ensure that toggling V-sync doesn't cause
 * graphics driver problems. */
TEST(test_set_vsync_while_open)
{
    const int width = graphics_display_width();
    const int height = graphics_display_height();
    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(width * height * 4, 0, MEM_ALLOC_TEMP));

    graphics_clear(0, 1, 1, 0, 1, 0);
    CHECK_TRUE(graphics_read_pixels(0, 0, width, height, pixels));
    for (int i = 0; i < width*height*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 0,255,255,255, (i/4) % width, (i/4) / width);
    }

    if (!graphics_set_display_attr("vsync", 0)) {
        mem_free(pixels);
        SKIP("System does not support toggling V-sync.");
    }

    graphics_finish_frame();
    graphics_start_frame();
    graphics_clear(0, 1, 0, 0, 1, 0);
    CHECK_TRUE(graphics_read_pixels(0, 0, width, height, pixels));
    for (int i = 0; i < width*height*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 0,255,0,255, (i/4) % width, (i/4) / width);
    }

    if (!graphics_set_display_attr("vsync", 1)) {
        mem_free(pixels);
        SKIP("System does not support toggling V-sync.");
    }

    graphics_finish_frame();
    graphics_start_frame();
    graphics_clear(0, 0, 1, 0, 1, 0);
    CHECK_TRUE(graphics_read_pixels(0, 0, width, height, pixels));
    for (int i = 0; i < width*height*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 0,0,255,255, (i/4) % width, (i/4) / width);
    }

    mem_free(pixels);
    return 1;
}

/*************************************************************************/
/*************************************************************************/
