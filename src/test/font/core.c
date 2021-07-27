/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/font/core.c: Tests for general font handling.
 */

/*
 * This file mostly tests the behavior of font functions when given invalid
 * font ID arguments.  Note that these tests assume that the bitmap font
 * tests have passed.
 */

#include "src/base.h"
#include "src/font.h"
#include "src/graphics.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/test/base.h"
#include "src/test/font/internal.h"
#include "src/test/graphics/internal.h"  // Borrow the graphics test helpers.
#include "src/texture.h"
#include "src/utility/font-file.h"

/*************************************************************************/
/******************************* Test data *******************************/
/*************************************************************************/

/* Font and text primitive IDs guaranteed to be invalid across all tests. */
#define INVALID_FONT  10000
#define INVALID_TEXT  10000

/*-----------------------------------------------------------------------*/

/* Simple font for testing (the same as in font-bitmap.c). */
static const ALIGNED(4) uint8_t font_data[] = {
    'F','O','N','T',  1, 10,  8,  0,  0,  0,  0, 24,  0,  5,  0, 16,
      0,  0,  0,104,  0,  0,  0,160,

      0,  0,  0,' ',  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  4,  0,
      0,  0,  0,'A',  0,  0,  0,  0,  5,  7,  7,  0,  0,  0,  1,  0,
      0,  0,  0,'B',  0,  5,  0,  0,  6,  7,  7,  0,255,192,  0,128,
      0,  0,  0,'p',  0, 11,  0,  0,  5,  6,  5,  0,  0,  0,  1,  0,
      0,  0, 32, 10,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 64,

    'T','E','X', 10,  2, 64,  0,  0,  0, 16,  0,  8,  0,  1,  0,  0,
      0,  0,  0, 32,  0,  0,  0,128,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,255,  0,  0, 64,255,255,255,128,  0,255,  0,255,255,  0,
      0,255,  0,255,  0,  0,255,  0,  0,255,128,255,255,  0,  0,255,
    255,  0,  0,  0,255,  0,255,  0,  0,255,128,255,  0,  0,  0,255,
    255,255,255,255,255,  0,255,255,255,128,  0,255,255,  0,  0,255,
    255,  0,  0,  0,255,  0,255,  0,  0,255,128,255,  0,255,255,  0,
    255,  0,  0,  0,255,  0,255,  0,  0,255,128,255,  0,  0,  0,  0,
    255,  0,  0,  0,255, 64,255,255,255,128,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,255,
};
/* String exercising all characters in the test font. */
static const char render_test_input[] = "A p\xE2\x80\x8A""B";

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

static int do_test_font_core(void);
int test_font_core(void)
{
    return run_tests_in_window(do_test_font_core);
}

DEFINE_GENERIC_TEST_RUNNER(do_test_font_core)

TEST_INIT(init)
{
    graphics_start_frame();
    return 1;
}

TEST_CLEANUP(cleanup)
{
    graphics_finish_frame();

    /* Clean up memory to prove there are no leaks. */
    graphics_flush_resources();
    return 1;
}

/*************************************************************************/
/***************** Basic font creation/management tests ******************/
/*************************************************************************/

TEST(test_id_reuse)
{
    int font1, font2;

    CHECK_TRUE(font1 = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                         0, 0));
    CHECK_TRUE(font2 = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                         0, 0));
    font_destroy(font1);
    CHECK_INTEQUAL(font_parse_bitmap((void *)font_data, sizeof(font_data),
                                     0, 0), font1);

    font_destroy(font1);
    font_destroy(font2);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_destroy_invalid)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                         0, 0));

    font_destroy(INVALID_FONT);  // Should log an error but not crash.
    font_destroy(0);  // Should not crash (defined as a no-op).
    font_destroy(-1);  // Should log an error but not crash.

    font_destroy(font);
    font_destroy(font);  // Should log an error but not crash.

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_native_size_invalid)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                         0, 0));
    font_destroy(font);

    CHECK_INTEQUAL(font_native_size(0), 0);
    CHECK_INTEQUAL(font_native_size(font), 0);
    CHECK_INTEQUAL(font_native_size(INVALID_FONT), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_height_invalid)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                         0, 0));

    CHECK_INTEQUAL(font_height(font, -10), 0);
    font_destroy(font);
    CHECK_INTEQUAL(font_height(0, 10), 0);
    CHECK_INTEQUAL(font_height(font, 10), 0);
    CHECK_INTEQUAL(font_height(INVALID_FONT, 10), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_baseline_invalid)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                         0, 0));

    CHECK_FLOATEQUAL(font_baseline(font, -10), 0);
    font_destroy(font);
    CHECK_FLOATEQUAL(font_baseline(0, 10), 0);
    CHECK_FLOATEQUAL(font_baseline(font, 10), 0);
    CHECK_FLOATEQUAL(font_baseline(INVALID_FONT, 10), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_ascent_invalid)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                         0, 0));

    CHECK_FLOATEQUAL(font_ascent(font, -10), 0);
    font_destroy(font);
    CHECK_FLOATEQUAL(font_ascent(0, 10), 0);
    CHECK_FLOATEQUAL(font_ascent(font, 10), 0);
    CHECK_FLOATEQUAL(font_ascent(INVALID_FONT, 10), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_descent_invalid)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                         0, 0));

    CHECK_FLOATEQUAL(font_descent(font, -10), 0);
    font_destroy(font);
    CHECK_FLOATEQUAL(font_descent(0, 10), 0);
    CHECK_FLOATEQUAL(font_descent(font, 10), 0);
    CHECK_FLOATEQUAL(font_descent(INVALID_FONT, 10), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_charwidth_invalid)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                         0, 0));

    CHECK_FLOATEQUAL(font_char_advance(font, -1, 10), 0);

    font_destroy(font);
    CHECK_FLOATEQUAL(font_char_advance(0, 'A', 10), 0);
    CHECK_FLOATEQUAL(font_char_advance(font, 'A', 10), 0);
    CHECK_FLOATEQUAL(font_char_advance(INVALID_FONT, 'A', 10), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_textwidth_invalid)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                         0, 0));

    CHECK_FLOATEQUAL(font_text_width(font, NULL, 10), 0);

    font_destroy(font);
    CHECK_FLOATEQUAL(font_text_width(0, "A", 10), 0);
    CHECK_FLOATEQUAL(font_text_width(font, "A", 10), 0);
    CHECK_FLOATEQUAL(font_text_width(INVALID_FONT, "A", 10), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_font_array_hole)
{
    int font1, font2, font3;

    CHECK_TRUE(font1 = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                         0, 0));
    CHECK_TRUE(font2 = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                         0, 0));
    CHECK_TRUE(font2 > font1);

    font_destroy(font1);
    CHECK_INTEQUAL(font_native_size(font1), 0);
    CHECK_INTEQUAL(font_native_size(font2), 10);

    CHECK_INTEQUAL(font_parse_bitmap((void *)font_data, sizeof(font_data),
                                     0, 0), font1);
    CHECK_TRUE(font3 = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                         0, 0));
    CHECK_TRUE(font3 > font2);
    CHECK_INTEQUAL(font_native_size(font1), 10);
    CHECK_INTEQUAL(font_native_size(font2), 10);
    CHECK_INTEQUAL(font_native_size(font3), 10);

    font_destroy(font1);
    font_destroy(font2);
    font_destroy(font3);
    return 1;
}

/*************************************************************************/
/************************* Font rendering tests **************************/
/*************************************************************************/

TEST(test_render_invalid)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                         0, 0));
    render_setup(0);

    CHECK_FLOATEQUAL(font_render_text(font, NULL, 10,
                                      &(Vector3f){10, 10, 0}, FONT_ALIGN_LEFT,
                                      0), 10);
    CHECK_FLOATEQUAL(font_render_text(font, render_test_input, 10,
                                      NULL, FONT_ALIGN_LEFT, 0), 0);
    CHECK_FLOATEQUAL(font_render_text(font, render_test_input, 10,
                                      &(Vector3f){10, 10, 0},
                                      FONT_ALIGN_LEFT-1, 0), 10);
    CHECK_FLOATEQUAL(font_render_text(0, render_test_input, 10,
                                      &(Vector3f){10, 10, 0}, FONT_ALIGN_LEFT,
                                      0), 10);
    CHECK_FLOATEQUAL(font_render_text(INVALID_FONT, render_test_input, 10,
                                      &(Vector3f){10, 10, 0}, FONT_ALIGN_LEFT,
                                      0), 10);

    font_destroy(font);
    CHECK_FLOATEQUAL(font_render_text(font, render_test_input, 10,
                                      &(Vector3f){10, 10, 0}, FONT_ALIGN_LEFT,
                                      0), 10);

    CHECK_TRUE(check_render_result(0, 0, 0, 0, 1, ""));
    return 1;
}

/*************************************************************************/
/************************* Text primitive tests **************************/
/*************************************************************************/

TEST(test_create_text_invalid)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                         0, 0));

    CHECK_FALSE(font_create_text(font, NULL, 10,
                                 &(Vector3f){10, 10, 0}, FONT_ALIGN_LEFT, 0));
    CHECK_FALSE(font_create_text(font, render_test_input, 10,
                                 NULL, FONT_ALIGN_LEFT, 0));
    CHECK_FALSE(font_create_text(font, render_test_input, 10,
                                 &(Vector3f){10, 10, 0}, FONT_ALIGN_LEFT-1, 0));
    CHECK_FALSE(font_create_text(0, render_test_input, 10,
                                 &(Vector3f){10, 10, 0}, FONT_ALIGN_LEFT, 0));
    CHECK_FALSE(font_create_text(INVALID_FONT, render_test_input, 10,
                                 &(Vector3f){10, 10, 0}, FONT_ALIGN_LEFT, 0));

    font_destroy(font);
    CHECK_FALSE(font_create_text(font, render_test_input, 10,
                                 &(Vector3f){10, 10, 0}, FONT_ALIGN_LEFT, 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_text_primitive_invalid)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                         0, 0));
    render_setup(0);

    int text;
    CHECK_TRUE(text = font_create_text(font, render_test_input, 10,
                                       &(Vector3f){10, 10, 0}, FONT_ALIGN_LEFT,
                                       0));

    CHECK_FLOATEQUAL(text_advance(0), 0);
    CHECK_FLOATEQUAL(text_advance(INVALID_TEXT), 0);

    text_render(0);
    text_render(INVALID_TEXT);
    CHECK_TRUE(check_render_result(0, 0, 0, 0, 1, ""));

    text_destroy(0);
    text_destroy(INVALID_TEXT);
    text_destroy(text);
    text_destroy(text);  // Make sure double-free doesn't break anything.

    font_destroy(font);
    return 1;
}

/*************************************************************************/
/*************************************************************************/
