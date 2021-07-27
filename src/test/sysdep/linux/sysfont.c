/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/linux/movie.c: Tests for the FFmpeg-based Linux
 * implementation of the system-level movie playback interface.
 */

#include "src/base.h"
#include "src/font.h"
#include "src/graphics.h"
#include "src/math.h"
#include "src/sysdep.h"
#include "src/sysdep/linux/internal.h"
#include "src/test/base.h"
#include "src/test/font/internal.h"
#include "src/test/graphics/internal.h"
#include "src/test/sysdep/linux/wrap-x11.h"

#include <X11/Xlib.h>

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

static int do_test_linux_sysfont(void);
int test_linux_sysfont(void)
{
    return run_tests_in_window(do_test_linux_sysfont);
}

DEFINE_GENERIC_TEST_RUNNER(do_test_linux_sysfont)

TEST_INIT(init)
{
    clear_x11_wrapper_variables();
    graphics_start_frame();
    return 1;
}

TEST_CLEANUP(cleanup)
{
    graphics_finish_frame();
    graphics_flush_resources();
    CHECK_FALSE(linux_x11_get_error());
    clear_x11_wrapper_variables();
    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_font_load_failure)
{
    disable_XLoadQueryFont = 1;
    CHECK_FALSE(font_create_sysfont("", 12, 0));
    disable_XLoadQueryFont = 0;

    disable_XCreateGC = 1;
    CHECK_FALSE(font_create_sysfont("", 12, 0));
    disable_XCreateGC = 0;

    disable_XCreateGC_after = 1;
    CHECK_FALSE(font_create_sysfont("", 12, 0));
    disable_XCreateGC = 0;

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_font_render_failure)
{
    int font;
    CHECK_TRUE(font = font_create_sysfont("fixed", 12, 0));

    disable_XCreatePixmap = 1;
    CHECK_FALSE(font_create_text(font, "a", 12, &(Vector3f){10, 22, 0},
                                 FONT_ALIGN_CENTER, 1));
    disable_XCreatePixmap = 0;

    disable_XGetImage = 1;
    CHECK_FALSE(font_create_text(font, "a", 12, &(Vector3f){10, 22, 0},
                                 FONT_ALIGN_CENTER, 1));
    disable_XGetImage = 0;

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_font_name)
{
    int font;
    CHECK_TRUE(font = font_create_sysfont("fixed", 12, 0));
    render_setup(1);

    graphics_clear(0, 0, 0, 0, 1, 0);
    font_render_text(font, "a", 12, &(Vector3f){10, 22, 0},
                     FONT_ALIGN_CENTER, 1);
    uint8_t pixels[20*17*4];
    graphics_read_pixels(0, TESTH-27, 20, 17, pixels);
    int found_nonzero = 0;
    for (int y = 0; y < 17; y++) {
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

TEST(test_font_name_expanded)
{
    Display *display = linux_x11_display();
    const char *name = "-*-helvetica-*-r-*-*-12-*-*-*-*-*-*-*";
    XFontStruct *xfont = XLoadQueryFont(display, name);
    if (!xfont) {
        SKIP("Font \"%s\" not found.", name);
    }
    XFreeFont(display, xfont);

    int font;
    CHECK_TRUE(font = font_create_sysfont("helvetica", 12, 0));
    render_setup(1);

    graphics_clear(0, 0, 0, 0, 1, 0);
    font_render_text(font, "a", 12, &(Vector3f){10, 22, 0},
                     FONT_ALIGN_CENTER, 1);
    uint8_t pixels[20*17*4];
    graphics_read_pixels(0, TESTH-27, 20, 17, pixels);
    int found_nonzero = 0;
    for (int y = 0; y < 17; y++) {
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

TEST(test_font_name_nonexistent)
{
    int font, fixed_font;
    CHECK_TRUE(font = font_create_sysfont("_NO_SUCH_FONT_", 12, 0));
    CHECK_TRUE(fixed_font = font_create_sysfont("", 12, 0));
    render_setup(1);

    graphics_clear(0, 0, 0, 0, 1, 0);
    font_render_text(fixed_font, "a", 12, &(Vector3f){10, 22, 0},
                     FONT_ALIGN_CENTER, 1);
    uint8_t fixed_pixels[20*17*4];
    graphics_read_pixels(0, TESTH-27, 20, 17, fixed_pixels);

    graphics_clear(0, 0, 0, 0, 1, 0);
    font_render_text(font, "a", 12, &(Vector3f){10, 22, 0},
                     FONT_ALIGN_CENTER, 1);
    uint8_t pixels[20*17*4];
    graphics_read_pixels(0, TESTH-27, 20, 17, pixels);
    for (int y = 0; y < 17; y++) {
        for (int x = 0; x < 20; x++) {
            CHECK_PIXEL(&pixels[(y*20+x)*4],
                        fixed_pixels[(y*20+x)*4+0],
                        fixed_pixels[(y*20+x)*4+1],
                        fixed_pixels[(y*20+x)*4+2],
                        fixed_pixels[(y*20+x)*4+3],
                        x, y);
        }
    }

    font_destroy(font);
    font_destroy(fixed_font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_font_name_buffer_overflow)
{
    int font, fixed_font;
    char longbuf[1001];
    memset(longbuf, 'a', sizeof(longbuf)-1);
    longbuf[sizeof(longbuf)-1] = '\0';
    CHECK_TRUE(font = font_create_sysfont(longbuf, 12, 0));
    CHECK_TRUE(fixed_font = font_create_sysfont("", 12, 0));
    render_setup(1);

    graphics_clear(0, 0, 0, 0, 1, 0);
    font_render_text(fixed_font, "a", 12, &(Vector3f){10, 22, 0},
                     FONT_ALIGN_CENTER, 1);
    uint8_t expected[20*17*4];
    graphics_read_pixels(0, TESTH-27, 20, 17, expected);

    graphics_clear(0, 0, 0, 0, 1, 0);
    font_render_text(font, "a", 12, &(Vector3f){10, 22, 0},
                     FONT_ALIGN_CENTER, 1);
    uint8_t pixels[20*17*4];
    graphics_read_pixels(0, TESTH-27, 20, 17, pixels);
    for (int y = 0; y < 17; y++) {
        for (int x = 0; x < 20; x++) {
            CHECK_PIXEL(&pixels[(y*20+x)*4],
                        expected[(y*20+x)*4+0],
                        expected[(y*20+x)*4+1],
                        expected[(y*20+x)*4+2],
                        expected[(y*20+x)*4+3],
                        x, y);
        }
    }

    font_destroy(font);
    font_destroy(fixed_font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_high_unicode)
{
    int font;
    CHECK_TRUE(font = font_create_sysfont("", 12, 0));
    render_setup(1);

    CHECK_INTEQUAL(font_char_advance(font, 0x10000, 12), 0);

    graphics_clear(0, 0, 0, 0, 1, 0);
    font_render_text(font, "a", 12, &(Vector3f){10, 22, 0},
                     FONT_ALIGN_CENTER, 1);
    uint8_t expected[20*17*4];
    graphics_read_pixels(0, TESTH-27, 20, 17, expected);

    graphics_clear(0, 0, 0, 0, 1, 0);
    font_render_text(font, "\xf0\x90\x80\x80""a", 12, &(Vector3f){10, 22, 0},
                     FONT_ALIGN_CENTER, 1);
    uint8_t pixels[20*17*4];
    graphics_read_pixels(0, TESTH-27, 20, 17, pixels);
    for (int y = 0; y < 17; y++) {
        for (int x = 0; x < 20; x++) {
            CHECK_PIXEL(&pixels[(y*20+x)*4],
                        expected[(y*20+x)*4+0],
                        expected[(y*20+x)*4+1],
                        expected[(y*20+x)*4+2],
                        expected[(y*20+x)*4+3],
                        x, y);
        }
    }

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_invalid_unicode)
{
    int font;
    CHECK_TRUE(font = font_create_sysfont("", 12, 0));
    render_setup(1);

    graphics_clear(0, 0, 0, 0, 1, 0);
    font_render_text(font, "a", 12, &(Vector3f){10, 22, 0},
                     FONT_ALIGN_CENTER, 1);
    uint8_t expected[20*17*4];
    graphics_read_pixels(0, TESTH-27, 20, 17, expected);

    graphics_clear(0, 0, 0, 0, 1, 0);
    font_render_text(font, "\x80""a", 12, &(Vector3f){10, 22, 0},
                     FONT_ALIGN_CENTER, 1);
    uint8_t pixels[20*17*4];
    graphics_read_pixels(0, TESTH-27, 20, 17, pixels);
    for (int y = 0; y < 17; y++) {
        for (int x = 0; x < 20; x++) {
            CHECK_PIXEL(&pixels[(y*20+x)*4],
                        expected[(y*20+x)*4+0],
                        expected[(y*20+x)*4+1],
                        expected[(y*20+x)*4+2],
                        expected[(y*20+x)*4+3],
                        x, y);
        }
    }

    font_destroy(font);
    return 1;
}

/*************************************************************************/
/*************************************************************************/
