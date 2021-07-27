/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/font/sysfont.c: Tests for system-provided font rendering.
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

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

/* Check whether system-provided fonts are available.  On some systems,
 * this requires the graphics subsystem to be initialized, so we call this
 * with a window open using run_tests_in_window(). */
static int check_has_sysfont(void)
{
    int test = font_create_sysfont("", 10, 0);
    font_destroy(test);
    return test != 0;
}

static int do_test_font_sysfont(void);
int test_font_sysfont(void)
{
    if (!run_tests_in_window(check_has_sysfont)) {
        SKIP("System-provided fonts not supported.");
    }

    return run_tests_in_window(do_test_font_sysfont);
}

DEFINE_GENERIC_TEST_RUNNER(do_test_font_sysfont)

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
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_create_memory_failure)
{
    int font;
    CHECK_MEMORY_FAILURES(font = font_create_sysfont("", 10, 0));
    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_invalid)
{
    CHECK_FALSE(font_create_sysfont(NULL, 10, 0));
    CHECK_FALSE(font_create_sysfont("", 0, 0));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_native_size)
{
    int font;
    CHECK_TRUE(font = font_create_sysfont("", 10, 0));
    CHECK_TRUE(font_native_size(font) > 0);

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_metrics_valid)
{
    int font;
    CHECK_TRUE(font = font_create_sysfont("", 10, 0));

    CHECK_TRUE(font_height(font, 10) > 0);
    CHECK_TRUE(font_ascent(font, 10) + font_descent(font, 10) > 0);

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_height)
{
    int font;
    CHECK_TRUE(font = font_create_sysfont("", 10, 0));

    const float height10 = font_height(font, 10);
    const float height20 = font_height(font, 20);
    if (fabsf(height20/height10 - 2.0f) > 0.1f) {
        FAIL("font_height(20) != 2*font_height(10):"
             " height(10)=%g height(20)=%g", height10, height20);
    }

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_baseline)
{
    int font;
    CHECK_TRUE(font = font_create_sysfont("", 10, 0));

    const float baseline10 = font_baseline(font, 10);
    const float baseline20 = font_baseline(font, 20);
    if (baseline20 < baseline10) {
        FAIL("font_baseline(20) < font_baseline(10):"
             " baseline(10)=%g baseline(20)=%g", baseline10, baseline20);
    }

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_ascent)
{
    int font;
    CHECK_TRUE(font = font_create_sysfont("", 10, 0));

    const float ascent10 = font_ascent(font, 10);
    const float ascent20 = font_ascent(font, 20);
    if (ascent20 < ascent10) {
        FAIL("font_ascent(20) < font_ascent(10):"
             " ascent(10)=%g ascent(20)=%g", ascent10, ascent20);
    }

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_descent)
{
    int font;
    CHECK_TRUE(font = font_create_sysfont("", 10, 0));

    const float descent10 = font_descent(font, 10);
    const float descent20 = font_descent(font, 20);
    if (descent20 < descent10) {
        FAIL("font_descent(20) < font_descent(10):"
             " descent(10)=%g descent(20)=%g", descent10, descent20);
    }

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_charwidth)
{
    int font;
    CHECK_TRUE(font = font_create_sysfont("", 10, 0));

    for (char ch = ' '; ch <= '~'; ch++) {
        if (!(font_char_advance(font, ch, 10) > 0)) {
            FAIL("font_char_advance(font, %d, 10) > 0 was not true as"
                 " expected", ch);
        }
    }

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_textwidth_single)
{
    int font;
    CHECK_TRUE(font = font_create_sysfont("", 10, 0));

    CHECK_FLOATEQUAL(font_text_width(font, " ", 10), 0);

    for (char ch = '!'; ch <= '~'; ch++) {
        char buf[2] = {ch, 0};
        CHECK_TRUE(font_text_width(font, buf, 10) > 0);
    }

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_textwidth_multiple)
{
    int font;
    CHECK_TRUE(font = font_create_sysfont("", 10, 0));

    float advance_by_char = 0;
    for (char ch = ' '; ch <= '~'; ch++) {
        advance_by_char += font_char_advance(font, ch, 10);
    }

    char buf[(int)'~' - (int)' ' + 1];
    for (char ch = ' ' /*32*/; ch <= '~' /*126*/; ch++) {
        buf[ch - ' '] = ch;
    }
    buf[sizeof(buf)-1] = 0;
    /* Allow up to 10% variation due to rounding, kerning, width of "~" etc. */
    CHECK_FLOATRANGE(font_text_width(font, buf, 10),
                     advance_by_char*0.9f, advance_by_char*1.1f);

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_textwidth_memory_failure)
{
    int font;
    CHECK_TRUE(font = font_create_sysfont("", 10, 0));

    float test;
    /* Not all platforms need to allocate memory here. */
    TEST_mem_fail_after(0, 1, 0);
    test = font_text_width(font, "a", 10);
    TEST_mem_fail_after(-1, 0, 0);
    if (!test) {
        graphics_flush_resources();
        CHECK_TEXTURE_MEMORY_FAILURES(test = font_text_width(font, "a", 10));
    }
    CHECK_FLOATEQUAL(test, font_text_width(font, "a", 10));

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_textwidth_empty)
{
    int font;
    CHECK_TRUE(font = font_create_sysfont("", 10, 0));

    CHECK_FLOATEQUAL(font_text_width(font, "", 10), 0);

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_render)
{
    int font;
    /* Use a reasonably big font size so we don't get any characters that
     * end up less than a pixel wide. */
    CHECK_TRUE(font = font_create_sysfont("", 20, 0));
    render_setup(0);

    const int ascent = iceilf(font_ascent(font, 20));
    const int descent = iceilf(font_descent(font, 20));
    for (char ch = '!'; ch <= '~'; ch++) {
        char buf[2] = {ch, 0};
        int text;
        CHECK_TRUE(text = font_create_text(
                       font, buf, 20, &(Vector3f){10, 10, 0},
                       FONT_ALIGN_CENTER, 0));
        CHECK_TRUE(text_advance(text) > 0);
        graphics_clear(0, 0, 0, 0, 1, 0);
        text_render(text);
        text_destroy(text);
        uint8_t pixels[40*40*4];
        graphics_read_pixels(0, 0, 40, 40, pixels);
        int found_nonzero = 0;
        for (int y = 0; y < 40; y++) {
            for (int x = 0; x < 40; x++) {
                if (pixels[(y*40+x)*4]) {
                    if (y-10 >= ascent || 10-y > descent) {
                        FAIL("Character %d [%c] was rendered outside"
                             " vertical bounds (ascent=%d, descent=%d, but"
                            " pixel at %d,%d))",
                             ch, ch, ascent, descent, x-10, y-10);
                    } else {
                        found_nonzero = 1;
                    }
                }
            }
        }
        if (!found_nonzero) {
            FAIL("Character %d [%c] did not produce any output", ch, ch);
        }
    }

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_render_v_flip)
{
    int font;
    CHECK_TRUE(font = font_create_sysfont("", 20, 0));
    render_setup(1);

    int text;
    CHECK_TRUE(text = font_create_text(
                   font, "a", 20, &(Vector3f){10, 30, 0},
                   FONT_ALIGN_CENTER, 1));
    CHECK_TRUE(text_advance(text) > 0);
    graphics_clear(0, 0, 0, 0, 1, 0);
    text_render(text);
    text_destroy(text);
    uint8_t pixels[20*25*4];
    graphics_read_pixels(0, TESTH-35, 20, 25, pixels);
    int found_nonzero = 0;
    for (int y = 0; y < 25; y++) {
        for (int x = 0; x < 20; x++) {
            found_nonzero |= pixels[(y*20+x)*4];
        }
    }
    if (!found_nonzero) {
        FAIL("'a' did not produce any output");
    }

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_render_memory_failure)
{
    int font;
    CHECK_TRUE(font = font_create_sysfont("", 20, 0));
    render_setup(0);

    int text;
    CHECK_TEXTURE_MEMORY_FAILURES(text = font_create_text(
                                      font, "a", 20, &(Vector3f){10, 10, 0},
                                      FONT_ALIGN_CENTER, 0));
    CHECK_TRUE(text_advance(text) > 0);
    graphics_clear(0, 0, 0, 0, 1, 0);
    text_render(text);
    text_destroy(text);
    uint8_t pixels[20*25*4];
    graphics_read_pixels(0, 5, 20, 25, pixels);
    int found_nonzero = 0;
    for (int y = 0; y < 25; y++) {
        for (int x = 0; x < 20; x++) {
            found_nonzero |= pixels[(y*20+x)*4];
        }
    }
    if (!found_nonzero) {
        FAIL("'a' did not produce any output");
    }

    font_destroy(font);
    return 1;
}

/*************************************************************************/
/*************************************************************************/
