/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/font/bitmap.c: Tests for general font handling and bitmap font
 * rendering.
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

/* Simple font for testing, with 5 characters: ' ' 'A' 'B' 'p' U+200A */
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

/* Versions of the font with unaligned character or texture data. */
static const ALIGNED(4) uint8_t font_data_unaligned_charinfo[] = {
    'F','O','N','T',  1, 10,  8,  0,  0,  0,  0, 27,  0,  5,  0, 16,
      0,  0,  0,108,  0,  0,  0,160,
      0,  0,  0,

      0,  0,  0,' ',  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  4,  0,
      0,  0,  0,'A',  0,  0,  0,  0,  5,  7,  7,  0,  0,  0,  1,  0,
      0,  0,  0,'B',  0,  5,  0,  0,  6,  7,  7,  0,255,192,  0,128,
      0,  0,  0,'p',  0, 11,  0,  0,  5,  6,  5,  0,  0,  0,  1,  0,
      0,  0, 32, 10,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 64,

      0,
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

/* Versions of the font with unaligned character or texture data. */
static const ALIGNED(4) uint8_t font_data_unaligned_texture[] = {
    'F','O','N','T',  1, 10,  8,  0,  0,  0,  0, 24,  0,  5,  0, 16,
      0,  0,  0,105,  0,  0,  0,160,

      0,  0,  0,' ',  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  4,  0,
      0,  0,  0,'A',  0,  0,  0,  0,  5,  7,  7,  0,  0,  0,  1,  0,
      0,  0,  0,'B',  0,  5,  0,  0,  6,  7,  7,  0,255,192,  0,128,
      0,  0,  0,'p',  0, 11,  0,  0,  5,  6,  5,  0,  0,  0,  1,  0,
      0,  0, 32, 10,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 64,

      0,
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

/* String exercising all characters in the test font, and ensuring they
 * all start on an integral pixel coordinate when left-aligned. */
static const char render_test_input[] = "A p\xE2\x80\x8A""B";
/* Simple ASCII rendering of render_test_input, which will be parsed by
 * check_render_result(). */
static const char render_test_output[] =
    "  #             .###: "
    " # #             #  #:"
    "#   #     # ##   #  #:"
    "#####     ##  #  ###: "
    "#   #     #   #  #  #:"
    "#   #     ##  #  #  #:"
    "#   #     # ##  .###: "
    "          #           ";
/* The same thing at double width, without antialiasing. */
static const char render_test_output_2x_aliased[] =
    "    ##                          ..######::  "
    "  ##  ##                          ##    ##::"
    "##      ##          ##  ####      ##    ##::"
    "##########          ####    ##    ######::  "
    "##      ##          ##      ##    ##    ##::"
    "##      ##          ####    ##    ##    ##::"
    "##      ##          ##  ####    ..######::  "
    "                    ##                      ";

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

/**
 * check_render_result_mismatch:  Check that the result of rendering does
 * _not_ exactly match the given data.  Equivalent to
 * !check_render_result(...) with exact set to true, but does not log
 * failure messages on pixel mismatch.
 *
 * Helper for the font_set_antialias() tests.
 *
 * [Parameters]
 *     x0, y0: Base coordinates (origin at lower-left) of rectangle to check.
 *     w, h: Size of rectangle to check.
 *     data: Alpha data (w*h chars) to compare against (origin at upper-left;
 *         characters are ' '==0, '.'==64, ':'==128, '#'==255).
 * [Return value]
 *     True if the display data does _not_ match the given data and all
 *     other portions of the display are empty, false otherwise.
 */
static int check_render_result_mismatch(int x0, int y0, int w, int h,
                                        const char *data)
{
    int texture;
    CHECK_TRUE(
        texture = texture_create_from_display(0, 0, TESTW, TESTH, 1, 0, 0));
    const uint8_t *pixels;
    CHECK_TRUE(pixels = texture_lock_readonly(texture));

    int all_match = 1;
    for (int i = 0; i < TESTW*TESTH*4 && all_match; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        int value = 0;
        if (x >= x0 && x < x0+w && y >= y0 && y < y0+h) {
            const char ch = data[((h-1)-(y-y0))*w + (x-x0)];
            ASSERT(ch == ' ' || ch == '.' || ch == ':' || ch == '#');
            switch (ch) {
                case '.': value =  64; break;
                case ':': value = 128; break;
                case '#': value = 255; break;
            }
            all_match &= (pixels[i+0] == value &&
                          pixels[i+1] == value &&
                          pixels[i+2] == value &&
                          pixels[i+3] == 255);
        } else {
            CHECK_PIXEL(&pixels[i], 0,0,0,255, x, y);
        }
    }

    texture_destroy(texture);
    return 1;
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

static int do_test_font_bitmap(void);
int test_font_bitmap(void)
{
    return run_tests_in_window(do_test_font_bitmap);
}

DEFINE_GENERIC_TEST_RUNNER(do_test_font_bitmap)

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

TEST(test_parse)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                        0, 0));
    CHECK_INTEQUAL(font_native_size(font), 10);
    CHECK_FLOATEQUAL(font_height(font, 10), 10);
    CHECK_FLOATEQUAL(font_baseline(font, 10), 8);
    CHECK_FLOATEQUAL(font_ascent(font, 10), 7);
    CHECK_FLOATEQUAL(font_descent(font, 10), 1);
    CHECK_FLOATEQUAL(font_char_advance(font, ' ', 10), 4);
    CHECK_FLOATEQUAL(font_char_advance(font, 'A', 10), 6);
    CHECK_FLOATEQUAL(font_char_advance(font, 'B', 10), 6.25);
    CHECK_FLOATEQUAL(font_char_advance(font, 'p', 10), 6);
    CHECK_FLOATEQUAL(font_char_advance(font, 0x200A, 10), 0.25);

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_parse_memory_failure)
{
    int font;
    CHECK_TEXTURE_MEMORY_FAILURES(
        font = font_parse_bitmap((void *)font_data, sizeof(font_data), 0, 0));
    CHECK_INTEQUAL(font_native_size(font), 10);
    CHECK_FLOATEQUAL(font_height(font, 10), 10);
    CHECK_FLOATEQUAL(font_baseline(font, 10), 8);
    CHECK_FLOATEQUAL(font_ascent(font, 10), 7);
    CHECK_FLOATEQUAL(font_descent(font, 10), 1);
    CHECK_FLOATEQUAL(font_char_advance(font, ' ', 10), 4);
    CHECK_FLOATEQUAL(font_char_advance(font, 'A', 10), 6);
    CHECK_FLOATEQUAL(font_char_advance(font, 'B', 10), 6.25);
    CHECK_FLOATEQUAL(font_char_advance(font, 'p', 10), 6);
    CHECK_FLOATEQUAL(font_text_width(font, "ABp ", 10), 18.25);

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_parse_reuse)
{
    uint8_t *data;
    ASSERT(data = mem_alloc(sizeof(font_data), 0, MEM_ALLOC_TEMP));
    memcpy(data, font_data, sizeof(font_data));

    int font;
    CHECK_TRUE(font = font_parse_bitmap(data, sizeof(font_data), 0, 1));
    CHECK_INTEQUAL(font_native_size(font), 10);
    CHECK_FLOATEQUAL(font_height(font, 10), 10);
    CHECK_FLOATEQUAL(font_baseline(font, 10), 8);
    CHECK_FLOATEQUAL(font_ascent(font, 10), 7);
    CHECK_FLOATEQUAL(font_descent(font, 10), 1);
    CHECK_FLOATEQUAL(font_char_advance(font, ' ', 10), 4);
    CHECK_FLOATEQUAL(font_char_advance(font, 'A', 10), 6);
    CHECK_FLOATEQUAL(font_char_advance(font, 'B', 10), 6.25);
    CHECK_FLOATEQUAL(font_char_advance(font, 'p', 10), 6);
    CHECK_FLOATEQUAL(font_char_advance(font, 0x200A, 10), 0.25);

    font_destroy(font);  // Also frees the buffer we allocated above.
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_parse_reuse_memory_failure)
{
    uint8_t *data;
    int font;
    CHECK_TEXTURE_MEMORY_FAILURES(
        /* We need to reallocate the data buffer each time around. */
        ((data = mem_alloc(sizeof(font_data), 0, MEM_ALLOC_TEMP)) != NULL &&
         memcpy(data, font_data, sizeof(font_data)) &&
         (font = font_parse_bitmap(data, sizeof(font_data), 0, 1)) != 0));
    CHECK_INTEQUAL(font_native_size(font), 10);
    CHECK_FLOATEQUAL(font_height(font, 10), 10);
    CHECK_FLOATEQUAL(font_baseline(font, 10), 8);
    CHECK_FLOATEQUAL(font_ascent(font, 10), 7);
    CHECK_FLOATEQUAL(font_descent(font, 10), 1);
    CHECK_FLOATEQUAL(font_char_advance(font, ' ', 10), 4);
    CHECK_FLOATEQUAL(font_char_advance(font, 'A', 10), 6);
    CHECK_FLOATEQUAL(font_char_advance(font, 'B', 10), 6.25);
    CHECK_FLOATEQUAL(font_char_advance(font, 'p', 10), 6);
    CHECK_FLOATEQUAL(font_text_width(font, "ABp ", 10), 18.25);

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_parse_short_data)
{
    CHECK_FALSE(font_parse_bitmap((void *)font_data, 0, 0, 0));
    CHECK_FALSE(font_parse_bitmap((void *)font_data,
                                  sizeof(FontFileHeader) - 1, 0, 0));
    CHECK_FALSE(font_parse_bitmap((void *)font_data, sizeof(font_data) - 1,
                                  0, 0));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_parse_bad_charinfo)
{
    uint8_t *bad_data;
    ASSERT(bad_data = mem_alloc(sizeof(font_data), 0, MEM_ALLOC_TEMP));
    memcpy(bad_data, font_data, sizeof(font_data));
    bad_data[24] = bad_data[25] = bad_data[26] = bad_data[27] = 255;

    CHECK_FALSE(font_parse_bitmap(bad_data, sizeof(font_data), 0, 0));

    mem_free(bad_data);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_parse_out_of_range_char)
{
    uint8_t *bad_data;
    ASSERT(bad_data = mem_alloc(sizeof(font_data), 0, MEM_ALLOC_TEMP));
    memcpy(bad_data, font_data, sizeof(font_data));
    bad_data[24] = bad_data[25] = bad_data[26] = bad_data[27] = 127;

    int font;
    CHECK_TRUE(font = font_parse_bitmap(bad_data, sizeof(font_data), 0, 0));
    CHECK_INTEQUAL(font_native_size(font), 10);
    CHECK_FLOATEQUAL(font_height(font, 10), 10);
    CHECK_FLOATEQUAL(font_baseline(font, 10), 8);
    CHECK_FLOATEQUAL(font_ascent(font, 10), 7);
    CHECK_FLOATEQUAL(font_descent(font, 10), 1);
    CHECK_FLOATEQUAL(font_char_advance(font, ' ', 10), 0);  // Overwritten.
    CHECK_FLOATEQUAL(font_char_advance(font, 'A', 10), 6);
    CHECK_FLOATEQUAL(font_char_advance(font, 'B', 10), 6.25);
    CHECK_FLOATEQUAL(font_char_advance(font, 'p', 10), 6);
    CHECK_FLOATEQUAL(font_char_advance(font, 0x200A, 10), 0.25);
    CHECK_FLOATEQUAL(font_char_advance(font, 0x7F7F7F7F, 10), 0); // Out of range.

    font_destroy(font);
    mem_free(bad_data);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_parse_no_valid_chars)
{
    uint8_t *bad_data;
    ASSERT(bad_data = mem_alloc(sizeof(font_data), 0, MEM_ALLOC_TEMP));
    memcpy(bad_data, font_data, sizeof(font_data));
    bad_data[24] = bad_data[25] = bad_data[26] = bad_data[27] = 123;
    bad_data[40] = bad_data[41] = bad_data[42] = bad_data[43] = 124;
    bad_data[56] = bad_data[57] = bad_data[58] = bad_data[59] = 125;
    bad_data[72] = bad_data[73] = bad_data[74] = bad_data[75] = 126;
    bad_data[88] = bad_data[89] = bad_data[90] = bad_data[91] = 127;

    CHECK_FALSE(font_parse_bitmap(bad_data, sizeof(font_data), 0, 0));

    mem_free(bad_data);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_parse_unaligned_charinfo)
{
    CHECK_FALSE(font_parse_bitmap((void *)font_data_unaligned_charinfo,
                                  sizeof(font_data_unaligned_charinfo), 0, 0));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_parse_unaligned_texture)
{
    CHECK_FALSE(font_parse_bitmap((void *)font_data_unaligned_texture,
                                  sizeof(font_data_unaligned_texture), 0, 0));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_parse_invalid)
{
    CHECK_FALSE(font_parse_bitmap(NULL, sizeof(font_data), 0, 0));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_height)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                        0, 0));

    CHECK_FLOATEQUAL(font_height(font, 10), 10);
    CHECK_FLOATEQUAL(font_height(font, 20), 20);
    CHECK_FLOATEQUAL(font_height(font, 2.5), 2.5);

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_baseline)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                        0, 0));

    CHECK_FLOATEQUAL(font_baseline(font, 10), 8);
    CHECK_FLOATEQUAL(font_baseline(font, 20), 16);
    CHECK_FLOATEQUAL(font_baseline(font, 0.625), 0.5);

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_ascent)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                        0, 0));

    CHECK_FLOATEQUAL(font_ascent(font, 10), 7);
    CHECK_FLOATEQUAL(font_ascent(font, 20), 14);
    CHECK_FLOATEQUAL(font_ascent(font, 2.5), 1.75);

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_descent)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                        0, 0));

    CHECK_FLOATEQUAL(font_descent(font, 10), 1);
    CHECK_FLOATEQUAL(font_descent(font, 20), 2);
    CHECK_FLOATEQUAL(font_descent(font, 2.5), 0.25);

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_charwidth)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                        0, 0));

    CHECK_FLOATEQUAL(font_char_advance(font, 'B', 10), 6.25);
    CHECK_FLOATEQUAL(font_char_advance(font, 'B', 20), 12.5);
    CHECK_FLOATEQUAL(font_char_advance(font, 'B', 5), 3.125);

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_charwidth_missing_char_on_valid_page)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                        0, 0));

    CHECK_FLOATEQUAL(font_char_advance(font, 'C', 10), 0);

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_charwidth_missing_page_in_range)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                        0, 0));

    CHECK_FLOATEQUAL(font_char_advance(font, 0x0100, 10), 0);

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_charwidth_missing_page_out_of_range)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                        0, 0));

    CHECK_FLOATEQUAL(font_char_advance(font, 0xFF21, 10), 0);

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_textwidth)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                        0, 0));

    CHECK_FLOATEQUAL(font_text_width(font, "B", 10), 6);
    CHECK_FLOATEQUAL(font_text_width(font, "B", 20), 12);
    CHECK_FLOATEQUAL(font_text_width(font, "B", 5), 3);

    CHECK_FLOATEQUAL(font_text_width(font, "\xE2\x80\x8A", 10), 0);

    CHECK_FLOATEQUAL(font_text_width(font, "A p\xE2\x80\x8A""B", 10), 22);
    CHECK_FLOATEQUAL(font_text_width(font, "A p\xE2\x80\x8A""B", 20), 44);
    CHECK_FLOATEQUAL(font_text_width(font, "A p\xE2\x80\x8A""B", 5), 11);

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_textwidth_missing_char_on_valid_page)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                        0, 0));

    CHECK_FLOATEQUAL(font_text_width(font, "AC p\xE2\x80\x8A""B", 10), 22);

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_textwidth_missing_page)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                        0, 0));

    CHECK_FLOATEQUAL(font_text_width(font, "A\xEF\xBC\xA1 p\xE2\x80\x8A""B",
                                    10), 22);

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_textwidth_bad_utf8)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                        0, 0));

    CHECK_FLOATEQUAL(font_text_width(font, "A p\xE2\xE2\x80\x8A""B", 10), 22);

    font_destroy(font);
    return 1;
}

/*************************************************************************/
/************************* Font rendering tests **************************/
/*************************************************************************/

TEST(test_render)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                        0, 0));

    render_setup(0);
    CHECK_FLOATEQUAL(font_render_text(font, render_test_input, 10,
                                      &(Vector3f){10, 10, 0}, FONT_ALIGN_LEFT,
                                      0), 32.5);
    CHECK_TRUE(check_render_result(10, 9, 22, 8, 0, render_test_output));

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_render_memory_failure)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                        0, 0));

    render_setup(0);
    float new_x;
    CHECK_TEXTURE_MEMORY_FAILURES(
        (new_x = font_render_text(font, render_test_input, 10,
                                   &(Vector3f){10, 10, 0}, FONT_ALIGN_LEFT, 0))
        > 10);
    CHECK_FLOATEQUAL(new_x, 32.5);
    CHECK_TRUE(check_render_result(10, 9, 22, 8, 0, render_test_output));

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_render_align_center)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                        0, 0));

    render_setup(0);
    CHECK_FLOATEQUAL(font_render_text(font, render_test_input, 10,
                                      &(Vector3f){20, 10, 0},
                                      FONT_ALIGN_CENTER, 0), 31.5);
    CHECK_TRUE(check_render_result(9, 9, 22, 8, 0, render_test_output));

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_render_align_center_prekern)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                        0, 0));

    render_setup(0);
    CHECK_FLOATEQUAL(font_render_text(font, "B", 10,
                                      &(Vector3f){13, 10, 0},
                                      FONT_ALIGN_CENTER, 0), 16.5);
    CHECK_TRUE(check_render_result(10, 9, 6, 8, 0,
                                   ".###: "
                                   " #  #:"
                                   " #  #:"
                                   " ###: "
                                   " #  #:"
                                   " #  #:"
                                   ".###: "
                                   "      "));

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_render_align_right)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                        0, 0));

    render_setup(0);
    CHECK_FLOATEQUAL(font_render_text(font, render_test_input, 10,
                                      &(Vector3f){30.5, 10, 0},
                                      FONT_ALIGN_RIGHT, 0), 30.5);
    CHECK_TRUE(check_render_result(8, 9, 22, 8, 0, render_test_output));

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_render_flipped)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                        0, 0));

    render_setup(1);
    CHECK_FLOATEQUAL(font_render_text(font, render_test_input, 10,
                                      &(Vector3f){10, 10, 0}, FONT_ALIGN_LEFT,
                                      1), 32.5);
    CHECK_TRUE(check_render_result(10, TESTH-11, 22, 8, 0, render_test_output));

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_render_texture_offset_set)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                        0, 0));

    render_setup(0);
    /* Setting the texture offset should not change the result of rendering. */
    graphics_set_texture_offset(&(Vector2f){0.5, 0.5});
    CHECK_FLOATEQUAL(font_render_text(font, render_test_input, 10,
                                      &(Vector3f){10, 10, 0}, FONT_ALIGN_LEFT,
                                      0), 32.5);
    CHECK_TRUE(check_render_result(10, 9, 22, 8, 0, render_test_output));

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_render_bad_glyph_coords)
{
    uint8_t *bad_data;
    ASSERT(bad_data = mem_alloc(sizeof(font_data), 0, MEM_ALLOC_TEMP));
    memcpy(bad_data, font_data, sizeof(font_data));
    bad_data[45] = 14;  // 'A'
    bad_data[47] = 6;
    bad_data[48] = 3;
    bad_data[49] = 2;
    bad_data[50] = 2;
    bad_data[52] = 0;
    bad_data[53] = 0;
    bad_data[54] = 1;
    bad_data[55] = 0;
    bad_data[61] = 14;  // 'B'
    bad_data[63] = 6;
    bad_data[64] = 2;
    bad_data[65] = 3;
    bad_data[66] = 3;
    bad_data[68] = 0;
    bad_data[69] = 0;
    bad_data[70] = 2;
    bad_data[71] = 0;

    int font;
    CHECK_TRUE(font = font_parse_bitmap(bad_data, sizeof(font_data), 0, 0));
    render_setup(0);

    CHECK_FLOATEQUAL(font_render_text(font, "AB", 10, &(Vector3f){10, 10, 0},
                                      FONT_ALIGN_LEFT, 0), 18);
    /* Textures default to wraparound, so we should get pixels from the
     * left/top sides. */
    CHECK_TRUE(check_render_result(10, 10, 6, 3, 0,
                                   "      "
                                   "  #  #"
                                   " #  # "));

    font_destroy(font);
    mem_free(bad_data);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_render_empty_string)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                        0, 0));
    render_setup(0);

    CHECK_FLOATEQUAL(font_render_text(font, "", 10, &(Vector3f){10, 10, 0},
                                      FONT_ALIGN_LEFT, 0), 10);
    CHECK_TRUE(check_render_result(0, 0, 0, 0, 1, ""));

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_render_missing_char_on_valid_page)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                        0, 0));
    render_setup(0);

    /* Use FONT_ALIGN_CENTER so we exercise get_text_bounds() too. */
    CHECK_FLOATEQUAL(font_render_text(font, "AC p\xE2\x80\x8A""B", 10,
                                      &(Vector3f){20, 10, 0},
                                      FONT_ALIGN_CENTER, 0), 31.5);
    CHECK_TRUE(check_render_result(9, 9, 22, 8, 0, render_test_output));

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_render_missing_char_on_missing_page)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                        0, 0));
    render_setup(0);

    /* Use FONT_ALIGN_CENTER so we exercise get_text_bounds() too. */
    CHECK_FLOATEQUAL(font_render_text(font, "A\xC4\x80 p\xE2\x80\x8A""B", 10,
                                      &(Vector3f){20, 10, 0},
                                      FONT_ALIGN_CENTER, 0), 31.5);
    CHECK_TRUE(check_render_result(9, 9, 22, 8, 0, render_test_output));

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_render_bad_utf8)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                        0, 0));
    render_setup(0);

    /* Use FONT_ALIGN_CENTER so we exercise get_text_bounds() too. */
    CHECK_FLOATEQUAL(font_render_text(font, "A p\xE2\xE2\x80\x8A""B", 10,
                                      &(Vector3f){20, 10, 0},
                                      FONT_ALIGN_CENTER, 0), 31.5);
    CHECK_TRUE(check_render_result(9, 9, 22, 8, 0, render_test_output));

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_antialias)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                        0, 0));
    render_setup(0);
    graphics_set_view_matrix(&(Matrix4f){2,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1});

    /* The default state should be antialiased.  It's hard to guess exactly
     * what the renderer will do, so we just check that the output _doesn't_
     * match the expected aliased output, and assume that if the aliased
     * output matches below, then the antialiased output was also correct. */
    CHECK_FLOATEQUAL(font_render_text(font, render_test_input, 10,
                                      &(Vector3f){10, 10, 0}, FONT_ALIGN_LEFT,
                                      0), 32.5);
    CHECK_TRUE(check_render_result_mismatch(20, 9, 44, 8,
                                            render_test_output_2x_aliased));

    font_set_antialias(font, 0);
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_FLOATEQUAL(font_render_text(font, render_test_input, 10,
                                      &(Vector3f){10, 10, 0}, FONT_ALIGN_LEFT,
                                      0), 32.5);
    CHECK_TRUE(check_render_result(20, 9, 44, 8, 1,
                                   render_test_output_2x_aliased));

    font_set_antialias(font, 1);
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_FLOATEQUAL(font_render_text(font, render_test_input, 10,
                                      &(Vector3f){10, 10, 0}, FONT_ALIGN_LEFT,
                                      0), 32.5);
    CHECK_TRUE(check_render_result_mismatch(20, 9, 44, 8,
                                            render_test_output_2x_aliased));

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_antialias_invalid)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                        0, 0));
    render_setup(0);
    graphics_set_view_matrix(&(Matrix4f){2,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1});

    font_set_antialias(0, 0);
    font_set_antialias(INVALID_FONT, 0);
    CHECK_FLOATEQUAL(font_render_text(font, render_test_input, 10,
                                      &(Vector3f){10, 10, 0}, FONT_ALIGN_LEFT,
                                      0), 32.5);
    /* See test_set_antialias above for the meaning of this check. */
    CHECK_TRUE(check_render_result_mismatch(20, 9, 44, 8,
                                            render_test_output_2x_aliased));

    font_set_antialias(font, 0);
    font_set_antialias(0, 1);
    font_set_antialias(INVALID_FONT, 1);
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_FLOATEQUAL(font_render_text(font, render_test_input, 10,
                                      &(Vector3f){10, 10, 0}, FONT_ALIGN_LEFT,
                                      0), 32.5);
    CHECK_TRUE(check_render_result(20, 9, 44, 8, 1,
                                   render_test_output_2x_aliased));

    font_destroy(font);
    return 1;
}

/*************************************************************************/
/************************* Text primitive tests **************************/
/*************************************************************************/

TEST(test_create_text_and_render)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                        0, 0));
    render_setup(0);

    int text;
    CHECK_TRUE(text = font_create_text(font, render_test_input, 10,
                                       &(Vector3f){10, 10, 0}, FONT_ALIGN_LEFT,
                                       0));
    CHECK_FLOATEQUAL(text_advance(text), 22.5);
    text_render(text);
    text_destroy(text);
    CHECK_TRUE(check_render_result(10, 9, 22, 8, 0, render_test_output));

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_text_memory_failure)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                        0, 0));
    render_setup(0);

    int text;
    CHECK_TEXTURE_MEMORY_FAILURES(
        text = font_create_text(font, render_test_input, 10,
                                &(Vector3f){10, 10, 0}, FONT_ALIGN_LEFT, 0));
    CHECK_FLOATEQUAL(text_advance(text), 22.5);
    text_render(text);
    text_destroy(text);
    CHECK_TRUE(check_render_result(10, 9, 22, 8, 0, render_test_output));

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_text_render_multiple)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                        0, 0));
    render_setup(0);

    int text;
    CHECK_TRUE(text = font_create_text(font, render_test_input, 10,
                                       &(Vector3f){10, 10, 0}, FONT_ALIGN_LEFT,
                                       0));
    text_render(text);
    CHECK_TRUE(check_render_result(10, 9, 22, 8, 0, render_test_output));

    static const Matrix4f shifted_model =
        {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,20,0,1};
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_set_model_matrix(&shifted_model);
    text_render(text);
    graphics_set_model_matrix(&mat4_identity);
    CHECK_TRUE(check_render_result(10, 29, 22, 8, 0, render_test_output));

    text_destroy(text);
    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_text_render_whitespace)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                        0, 0));
    render_setup(0);

    int text;
    CHECK_TRUE(text = font_create_text(font, "   ", 10, &(Vector3f){10, 10, 0},
                                       FONT_ALIGN_LEFT, 0));
    CHECK_FLOATEQUAL(text_advance(text), 12);
    text_render(text);
    text_destroy(text);
    CHECK_TRUE(check_render_result(0, 0, 0, 0, 1, ""));

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_text_render_empty)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                        0, 0));
    render_setup(0);

    int text;
    CHECK_TRUE(text = font_create_text(font, "", 10, &(Vector3f){10, 10, 0},
                                       FONT_ALIGN_LEFT, 0));
    CHECK_FLOATEQUAL(text_advance(text), 0);
    text_render(text);
    text_destroy(text);
    CHECK_TRUE(check_render_result(0, 0, 0, 0, 1, ""));

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_text_render_after_font_destroyed)
{
    int font;
    CHECK_TRUE(font = font_parse_bitmap((void *)font_data, sizeof(font_data),
                                        0, 0));
    render_setup(0);

    int text;
    CHECK_TRUE(text = font_create_text(font, render_test_input, 10,
                                       &(Vector3f){10, 10, 0}, FONT_ALIGN_LEFT,
                                       0));

    font_destroy(font);
    CHECK_FLOATEQUAL(text_advance(text), 22.5);
    text_render(text);
    text_destroy(text);
    CHECK_TRUE(check_render_result(10, 9, 22, 8, 0, render_test_output));

    return 1;
}

/*************************************************************************/
/*************************************************************************/
