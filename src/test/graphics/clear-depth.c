/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/graphics/clear-depth.c: Tests for graphics_clear() and related
 * functions which depend on depth testing working correctly.
 */

#include "src/base.h"
#include "src/graphics.h"
#include "src/memory.h"
#include "src/test/base.h"
#include "src/test/graphics/internal.h"

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

static int do_test_graphics_clear_depth(void);
int test_graphics_clear_depth(void)
{
    return run_tests_in_window(do_test_graphics_clear_depth);
}

DEFINE_GENERIC_TEST_RUNNER(do_test_graphics_clear_depth)

TEST_INIT(init)
{
    graphics_start_frame();
    graphics_set_viewport(0, 0, 64, 64);
    return 1;
}

TEST_CLEANUP(cleanup)
{
    graphics_enable_depth_test(0);
    graphics_set_depth_test_comparison(GRAPHICS_COMPARISON_LESS);
    graphics_enable_stencil_test(0);
    graphics_finish_frame();
    graphics_flush_resources();
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_clear_alternate_depth_value)
{
    graphics_enable_depth_test(1);

    /* Z=0.5 translates to depth 0.75, so this square should be displayed
     * if we clear the depth buffer to 1 (the default value). */
    graphics_clear(0, 0, 0, 0, 1, 0);
    draw_square(0.5, 0,1,0,1);
    CHECK_SQUARE(0,1,0);

    /* Clearing the depth buffer to a value less than 0.5 should prevent
     * a square at Z=0 (depth 0.5) from being drawn, even though it would
     * be in front of the previous square. */
    graphics_clear(0, 0, 0, 0, 0.25, 0);
    draw_square(0, 1,0,0,1);
    CHECK_SQUARE(0,0,0);
    draw_square(-1, 0,0,1,1);
    CHECK_SQUARE(0,0,1);

    /* We should be able to clear to a value greater than the current depth. */
    graphics_clear(0, 0, 0, 0, 0.75, 0);
    draw_square(0, 1,1,1,1);
    CHECK_SQUARE(1,1,1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_clear_invalid_depth_value)
{
    graphics_enable_depth_test(1);

    /* Depth values out of the range [0,1] should be clamped to that range. */

    graphics_clear(0, 0, 0, 0, -1, 0);
    graphics_set_depth_test_comparison(GRAPHICS_COMPARISON_LESS_EQUAL);
    draw_square(-1, 0,0,1,1);
    CHECK_SQUARE(0,0,1);

    graphics_clear(0, 0, 0, 0, 2, 0);
    graphics_set_depth_test_comparison(GRAPHICS_COMPARISON_LESS);
    draw_square(1, 1,0,0,1);
    CHECK_SQUARE(0,0,0);
    graphics_set_depth_test_comparison(GRAPHICS_COMPARISON_LESS_EQUAL);
    draw_square(1, 0,1,0,1);
    CHECK_SQUARE(0,1,0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_clear_alternate_stencil_value)
{
    graphics_enable_stencil_test(1);

    /* Check that clearing to zero (the default value) works. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_EQUAL, 0, 255);
    draw_square(0, 0,1,0,1);
    CHECK_SQUARE(0,1,0);

    /* Check that clearing to an alternate value works. */
    graphics_clear(0, 0, 0, 0, 0.25, 200);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_EQUAL, 0, 255);
    draw_square(0, 1,0,0,1);
    CHECK_SQUARE(0,0,0);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_EQUAL, 200, 255);
    draw_square(-1, 0,0,1,1);
    CHECK_SQUARE(0,0,1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_clear_color)
{
    graphics_enable_depth_test(1);
    graphics_enable_stencil_test(1);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_EQUAL, 0, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_INCR);

    graphics_clear(0, 0, 0, 0, 1, 0);
    draw_square(0, 1,1,1,1);
    CHECK_SQUARE(1,1,1);

    graphics_clear_color(0, 0, 0, 0);
    CHECK_SQUARE(0,0,0);

    /* This should not be drawn because it fails the stencil test. */
    draw_square(-1, 1,0,0,1);
    CHECK_SQUARE(0,0,0);

    /* This should not be drawn because it fails the depth test. */
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_EQUAL, 1, 255);
    draw_square(0, 1,0,0,1);
    CHECK_SQUARE(0,0,0);

    /* This should be drawn. */
    draw_square(-1, 0,1,0,1);
    CHECK_SQUARE(0,1,0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_clear_color_bounds)
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
            graphics_finish_frame();
            graphics_start_frame();
        }
        graphics_clear_color(tests[test].r, tests[test].g, tests[test].b, 0);
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

TEST(test_clear_depth)
{
    graphics_enable_depth_test(1);
    graphics_enable_stencil_test(1);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_EQUAL, 0, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_INCR);

    graphics_clear(0, 0, 0, 0, 1, 0);
    draw_square(0, 1,1,1,1);
    CHECK_SQUARE(1,1,1);

    /* This should not be drawn because it fails the depth and stencil
     * tests. */
    draw_square(0, 1,0,0,1);
    CHECK_SQUARE(1,1,1);

    /* This should not affect the color buffer. */
    graphics_clear_depth(1, 0);
    CHECK_SQUARE(1,1,1);

    /* This should be now drawn due to the graphics_clear_depth() call. */
    draw_square(0, 0,1,0,1);
    CHECK_SQUARE(0,1,0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_clear_depth_bounds)
{
    graphics_clear(0, 0, 0, 0, 0.5, 0);
    graphics_enable_depth_test(1);

    graphics_clear_depth(-1, 0);
    graphics_set_depth_test_comparison(GRAPHICS_COMPARISON_LESS_EQUAL);
    draw_square(-1, 0,0,1,1);
    CHECK_SQUARE(0,0,1);

    graphics_clear_depth(2, 0);
    graphics_set_depth_test_comparison(GRAPHICS_COMPARISON_LESS);
    draw_square(1, 1,0,0,1);
    CHECK_SQUARE(0,0,1);
    graphics_set_depth_test_comparison(GRAPHICS_COMPARISON_LESS_EQUAL);
    draw_square(1, 0,1,0,1);
    CHECK_SQUARE(0,1,0);

    return 1;
}

/*************************************************************************/
/*************************************************************************/
