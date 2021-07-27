/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/font/freetype.c: Tests for FreeType-based font rendering.
 */

#include "src/base.h"
#include "src/font.h"
#include "src/graphics.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/test/base.h"
#include "src/test/font/internal.h"
#include "src/test/graphics/internal.h"  // Borrow the graphics test helpers.
#include "src/texture.h"
#include "src/thread.h"

/*************************************************************************/
/******************************* Test data *******************************/
/*************************************************************************/

/* Font and text primitive IDs guaranteed to be invalid across all tests. */
#define INVALID_FONT  10000
#define INVALID_TEXT  10000

/*-----------------------------------------------------------------------*/

/* Font data and file size (data is loaded when the test is run). */
void *font_data;
int font_data_size;

/* String exercising all characters in the test font, and ensuring they
 * all start on an integral pixel coordinate when left-aligned. */
static const char render_test_input[] = "L-\xC2\xA0j";
/* Simple ASCII rendering of render_test_input, which will be parsed by
 * check_render_result(). */
static const char render_test_output[] =
    "#                 "
    "#                 "
    "#               # "
    "#                 "
    "#      ###:     # "
    "#               # "
    "#               # "
    "#####           # "
    "                # "
    "                  ";

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

/**
 * load_file:  Load a file into memory.  The returned buffer should be
 * freed with mem_free() when no longer necessary.
 *
 * [Parameters]
 *     path: File pathname, relative to the root of the resource files.
 *     size_ret: Pointer to varaible to receive the file size, in bytes.
 * [Return value]
 *     Newly allocated file data buffer, or NULL on error.
 */
static void *load_file(const char *path, int *size_ret)
{
    PRECOND(path != NULL, return NULL);
    PRECOND(size_ret != NULL, return NULL);

    char abs_path[10000];
    ASSERT(sys_get_resource_path_prefix(abs_path, sizeof(abs_path))
           < (int)sizeof(abs_path));
    ASSERT(strformat_check(abs_path+strlen(abs_path),
                           sizeof(abs_path)-strlen(abs_path), "%s", path));
    SysFile *file = sys_file_open(abs_path);
    if (!file) {
        DLOG("%s (%s): %s", path, abs_path, sys_last_errstr());
        return NULL;
    }
    const int datalen = sys_file_size(file);
    if (sys_file_size(file) > 0x7FFFFFFF) {
        DLOG("%s: File too large", path);
        sys_file_close(file);
        return NULL;
    }
    void *data = mem_alloc(datalen, 0, 0);
    if (!data) {
        DLOG("%s: Out of memory (need %d bytes)", path, datalen);
        sys_file_close(file);
        return NULL;
    }
    const int32_t nread = sys_file_read(file, data, datalen);
    sys_file_close(file);
    if (nread != datalen) {
        DLOG("%s: Read error", path);
        mem_free(data);
        return NULL;
    }
    *size_ret = datalen;
    return data;
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

static int do_test_font_freetype(void);
int test_font_freetype(void)
{
#ifndef SIL_FONT_INCLUDE_FREETYPE
    SKIP("FreeType font support not compiled in.");
#endif

    CHECK_TRUE(thread_init());
    CHECK_TRUE(sys_file_init());
    ASSERT(font_data = load_file(
               "testdata/font/SILTestFont.ttf", &font_data_size));
    sys_file_cleanup();
    thread_cleanup();
    const int result = run_tests_in_window(do_test_font_freetype);
    mem_free(font_data);
    font_data = NULL;
    font_data_size = 0;
    return result;
}

DEFINE_GENERIC_TEST_RUNNER(do_test_font_freetype)

TEST_INIT(init)
{
    CHECK_TRUE(sys_file_init());
    graphics_start_frame();
    return 1;
}

TEST_CLEANUP(cleanup)
{
    graphics_finish_frame();
    /* Clean up memory to prove there are no leaks. */
    graphics_flush_resources();

    sys_file_cleanup();
    return 1;
}

/*************************************************************************/
/****************** Font creation and information tests ******************/
/*************************************************************************/

TEST(test_parse)
{
    int font;
    CHECK_TRUE(font = font_parse_freetype(font_data, font_data_size, 0, 0));
    CHECK_FLOATEQUAL(font_height(font, 10), 10);
    CHECK_FLOATEQUAL(font_baseline(font, 10), 8);
    CHECK_FLOATEQUAL(font_ascent(font, 10), 8);
    CHECK_FLOATEQUAL(font_descent(font, 10), 1);
    CHECK_FLOATEQUAL(font_char_advance(font, 'L', 10), 6);
    CHECK_FLOATEQUAL(font_char_advance(font, '-', 10), 5.5);
    CHECK_FLOATEQUAL(font_char_advance(font, 'j', 10), 3);
    CHECK_FLOATEQUAL(font_char_advance(font, 0xA0, 10), 3.5);

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_parse_memory_failure)
{
    int font;
    CHECK_MEMORY_FAILURES(
        font = font_parse_freetype(font_data, font_data_size, 0, 0));
    CHECK_FLOATEQUAL(font_height(font, 10), 10);
    CHECK_FLOATEQUAL(font_baseline(font, 10), 8);
    CHECK_FLOATEQUAL(font_ascent(font, 10), 8);
    CHECK_FLOATEQUAL(font_descent(font, 10), 1);
    CHECK_FLOATEQUAL(font_char_advance(font, 'L', 10), 6);
    CHECK_FLOATEQUAL(font_char_advance(font, '-', 10), 5.5);
    CHECK_FLOATEQUAL(font_char_advance(font, 'j', 10), 3);
    CHECK_FLOATEQUAL(font_char_advance(font, 0xA0, 10), 3.5);

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_parse_bad_data)
{
    CHECK_FALSE(font_parse_freetype((void *)"abc", 3, 0, 0));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_parse_invalid)
{
    CHECK_FALSE(font_parse_freetype(NULL, font_data_size, 0, 0));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_native_size)
{
    int font;
    CHECK_TRUE(font = font_parse_freetype(font_data, font_data_size, 0, 0));

    /* Scalable fonts have no "native" size, but font_native_size() should
     * always return a nonzero value. */
    CHECK_TRUE(font_native_size(font) > 0);

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_height)
{
    int font;
    CHECK_TRUE(font = font_parse_freetype(font_data, font_data_size, 0, 0));

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
    CHECK_TRUE(font = font_parse_freetype(font_data, font_data_size, 0, 0));

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
    CHECK_TRUE(font = font_parse_freetype(font_data, font_data_size, 0, 0));

    CHECK_FLOATEQUAL(font_ascent(font, 10), 8);
    CHECK_FLOATEQUAL(font_ascent(font, 20), 16);
    CHECK_FLOATEQUAL(font_ascent(font, 0.625), 0.5);

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_descent)
{
    int font;
    CHECK_TRUE(font = font_parse_freetype(font_data, font_data_size, 0, 0));

    CHECK_FLOATEQUAL(font_descent(font, 10), 1);
    CHECK_FLOATEQUAL(font_descent(font, 20), 2);
    CHECK_FLOATEQUAL(font_descent(font, 0.625), 0.0625);

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_charwidth)
{
    int font;
    CHECK_TRUE(font = font_parse_freetype(font_data, font_data_size, 0, 0));

    CHECK_FLOATEQUAL(font_char_advance(font, '-', 10), 5.5);
    CHECK_FLOATEQUAL(font_char_advance(font, '-', 20), 11);
    CHECK_FLOATEQUAL(font_char_advance(font, '-', 5), 2.75);

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_charwidth_missing_char)
{
    int font;
    CHECK_TRUE(font = font_parse_freetype(font_data, font_data_size, 0, 0));

    CHECK_FLOATEQUAL(font_char_advance(font, 'C', 10), 0);

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_textwidth)
{
    int font;
    CHECK_TRUE(font = font_parse_freetype(font_data, font_data_size, 0, 0));

    CHECK_FLOATEQUAL(font_text_width(font, "-", 10), 5);
    CHECK_FLOATEQUAL(font_text_width(font, "-", 20), 9);
    CHECK_FLOATEQUAL(font_text_width(font, "-", 5), 3);

    CHECK_FLOATEQUAL(font_text_width(font, "\xC2\xA0", 10), 0);

    CHECK_FLOATEQUAL(font_text_width(font, "L-\xC2\xA0j", 10), 17);
    CHECK_FLOATEQUAL(font_text_width(font, "L-\xC2\xA0j", 20), 34);
    CHECK_FLOATEQUAL(font_text_width(font, "L-\xC2\xA0j", 5), 9.5);

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_textwidth_empty_string)
{
    int font;
    CHECK_TRUE(font = font_parse_freetype(font_data, font_data_size, 0, 0));

    CHECK_FLOATEQUAL(font_text_width(font, "", 10), 0);

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_textwidth_missing_char)
{
    int font;
    CHECK_TRUE(font = font_parse_freetype(font_data, font_data_size, 0, 0));

    CHECK_FLOATEQUAL(font_text_width(font, "LC-\xC2\xA0j", 10), 17);

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_textwidth_bad_utf8)
{
    int font;
    CHECK_TRUE(font = font_parse_freetype(font_data, font_data_size, 0, 0));

    CHECK_FLOATEQUAL(font_text_width(font, "L-\xC2\xC2\xA0j", 10), 17);

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_textwidth_kerning)
{
    int font;
    CHECK_TRUE(font = font_parse_freetype(font_data, font_data_size, 0, 0));

    /* --- should have -2 pixels of kerning between each pair of dashes. */
    CHECK_FLOATEQUAL(font_text_width(font, "L---\xC2\xA0j", 10), 24);

    /* L' should have the ' inside the L. */
    CHECK_FLOATEQUAL(font_text_width(font, "L'", 10), 5);

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_textwidth_kerning_nokern_font)
{
    /* This is just to check the !FT_HAS_KERNING(face) code path. */

    int size;
    void *data;
    ASSERT(data = load_file("testdata/font/SILTestFont-NoKern.ttf", &size));
    int font;
    CHECK_TRUE(font = font_parse_freetype(data, size, 0, 1));

    CHECK_FLOATEQUAL(font_text_width(font, "L'", 10), 8);

    font_destroy(font);
    return 1;
}

/*************************************************************************/
/************************* Font rendering tests **************************/
/*************************************************************************/

TEST(test_render)
{
    int font;
    CHECK_TRUE(font = font_parse_freetype(font_data, font_data_size, 0, 0));

    render_setup(0);
    CHECK_FLOATEQUAL(font_render_text(font, render_test_input, 10,
                                      &(Vector3f){10, 10, 0}, FONT_ALIGN_LEFT,
                                      0), 28);
    CHECK_TRUE(check_render_result(10, 8, 18, 10, 0, render_test_output));

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_render_memory_failure)
{
    int font;

    render_setup(0);
    float new_x;
    /* FreeType may create additional persistent data structures when
     * rendering, so we need to free and recreate the font on each pass to
     * avoid false positives from the leak checker. */
    CHECK_TEXTURE_MEMORY_FAILURES(
        ((font = font_parse_freetype(font_data, font_data_size, 0, 0)) != 0
         && ((new_x = font_render_text(font, render_test_input, 10,
                                       &(Vector3f){10, 10, 0}, FONT_ALIGN_LEFT,
                                       0)) > 10))
        || (font_destroy(font), 0));
    CHECK_FLOATEQUAL(new_x, 28);
    CHECK_TRUE(check_render_result(10, 8, 18, 10, 0, render_test_output));

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_render_align_center)
{
    int font;
    CHECK_TRUE(font = font_parse_freetype(font_data, font_data_size, 0, 0));

    render_setup(0);
    CHECK_FLOATEQUAL(font_render_text(font, render_test_input, 10,
                                      &(Vector3f){19.5, 10, 0},
                                      FONT_ALIGN_CENTER, 0), 29);
    CHECK_TRUE(check_render_result(11, 8, 18, 10, 0, render_test_output));

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_render_align_right)
{
    int font;
    CHECK_TRUE(font = font_parse_freetype(font_data, font_data_size, 0, 0));

    render_setup(0);
    CHECK_FLOATEQUAL(font_render_text(font, render_test_input, 10,
                                      &(Vector3f){30, 10, 0},
                                      FONT_ALIGN_RIGHT, 0), 30);
    CHECK_TRUE(check_render_result(12, 8, 18, 10, 0, render_test_output));

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_render_flipped)
{
    int font;
    CHECK_TRUE(font = font_parse_freetype(font_data, font_data_size, 0, 0));

    render_setup(1);
    CHECK_FLOATEQUAL(font_render_text(font, render_test_input, 10,
                                      &(Vector3f){10, 10, 0}, FONT_ALIGN_LEFT,
                                      1), 28);
    CHECK_TRUE(check_render_result(10, TESTH-12, 18, 10, 0, render_test_output));

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_render_empty_string)
{
    int font;
    CHECK_TRUE(font = font_parse_freetype(font_data, font_data_size, 0, 0));
    render_setup(0);

    CHECK_FLOATEQUAL(font_render_text(font, "", 10, &(Vector3f){10, 10, 0},
                                      FONT_ALIGN_LEFT, 0), 10);
    CHECK_TRUE(check_render_result(0, 0, 0, 0, 1, ""));

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_render_missing_char)
{
    int font;
    CHECK_TRUE(font = font_parse_freetype(font_data, font_data_size, 0, 0));

    render_setup(0);
    CHECK_FLOATEQUAL(font_render_text(font, "LC-\xC2\xA0j", 10,
                                      &(Vector3f){10, 10, 0}, FONT_ALIGN_LEFT,
                                      0), 28);
    CHECK_TRUE(check_render_result(10, 8, 18, 10, 0, render_test_output));

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_render_bad_utf8)
{
    int font;
    CHECK_TRUE(font = font_parse_freetype(font_data, font_data_size, 0, 0));
    render_setup(0);

    /* Use FONT_ALIGN_CENTER so we exercise get_text_bounds() too. */
    CHECK_FLOATEQUAL(font_render_text(font, "L-\xC2\xC2\xA0j", 10,
                                      &(Vector3f){19.5, 10, 0},
                                      FONT_ALIGN_CENTER, 0), 29);
    CHECK_TRUE(check_render_result(11, 8, 18, 10, 0, render_test_output));

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_render_kerning)
{
    int font;
    CHECK_TRUE(font = font_parse_freetype(font_data, font_data_size, 0, 0));
    render_setup(0);

    CHECK_FLOATEQUAL(font_render_text(font, "L---\xC2\xA0j", 10,
                                      &(Vector3f){10, 10, 0}, FONT_ALIGN_LEFT,
                                      0), 35);
    CHECK_TRUE(check_render_result(10, 8, 25, 10, 0,
                                   "#                        "
                                   "#                        "
                                   "#                      # "
                                   "#                        "
                                   "#      ##########:     # "
                                   "#                      # "
                                   "#                      # "
                                   "#####                  # "
                                   "                       # "
                                   "                         "));

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_render_kerning_2)
{
    int font;
    CHECK_TRUE(font = font_parse_freetype(font_data, font_data_size, 0, 0));
    render_setup(0);

    CHECK_FLOATEQUAL(font_render_text(font, "L'L", 10,
                                      &(Vector3f){10, 10, 0}, FONT_ALIGN_LEFT,
                                      0), 21);
    CHECK_TRUE(check_render_result(10, 8, 11, 10, 0,
                                   "#  # #     "
                                   "#  # #     "
                                   "#    #     "
                                   "#    #     "
                                   "#    #     "
                                   "#    #     "
                                   "#    #     "
                                   "########## "
                                   "           "
                                   "           "));

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_render_blending)
{
    int font;
    CHECK_TRUE(font = font_parse_freetype(font_data, font_data_size, 0, 0));
    render_setup(0);

    CHECK_FLOATEQUAL(font_render_text(font, "-:", 10,
                                      &(Vector3f){10, 10, 0}, FONT_ALIGN_LEFT,
                                      0), 14);
    CHECK_TRUE(check_render_result(10, 8, 5, 10, 0,
                                   "     "
                                   "     "
                                   "  #  "
                                   "     "
                                   " ###:"
                                   "     "
                                   "  #  "
                                   "     "
                                   "     "
                                   "     "));

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_render_broken_font)
{
    void *broken_font_data;
    int broken_font_data_size;
    ASSERT(broken_font_data = load_file(
               "testdata/font/SILTestFont-broken-glyph.ttf",
               &broken_font_data_size));

    int font;
    CHECK_TRUE(font = font_parse_freetype(
                   broken_font_data, broken_font_data_size, 0, 1));

    render_setup(0);
    CHECK_FLOATEQUAL(font_render_text(font, render_test_input, 10,
                                      &(Vector3f){10, 10, 0}, FONT_ALIGN_LEFT,
                                      0), 28);
    CHECK_TRUE(check_render_result(10, 8, 18, 10, 0,
                                   "                  "
                                   "                  "
                                   "                # "
                                   "                  "
                                   "       ###:     # "
                                   "                # "
                                   "                # "
                                   "                # "
                                   "                # "
                                   "                  "));

    font_destroy(font);
    return 1;
}

/*************************************************************************/
/************************* Text primitive tests **************************/
/*************************************************************************/

TEST(test_create_text_and_render)
{
    int font;
    CHECK_TRUE(font = font_parse_freetype(font_data, font_data_size, 0, 0));
    render_setup(0);

    int text;
    CHECK_TRUE(text = font_create_text(font, render_test_input, 10,
                                       &(Vector3f){10, 10, 0}, FONT_ALIGN_LEFT,
                                       0));
    CHECK_FLOATEQUAL(text_advance(text), 18);
    text_render(text);
    text_destroy(text);
    CHECK_TRUE(check_render_result(10, 8, 18, 10, 0, render_test_output));

    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_text_render_multiple)
{
    int font;
    CHECK_TRUE(font = font_parse_freetype(font_data, font_data_size, 0, 0));
    render_setup(0);

    int text;
    CHECK_TRUE(text = font_create_text(font, render_test_input, 10,
                                       &(Vector3f){10, 10, 0}, FONT_ALIGN_LEFT,
                                       0));
    text_render(text);
    CHECK_TRUE(check_render_result(10, 8, 18, 10, 0, render_test_output));

    static const Matrix4f shifted_model =
        {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,20,0,1};
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_set_model_matrix(&shifted_model);
    text_render(text);
    graphics_set_model_matrix(&mat4_identity);
    CHECK_TRUE(check_render_result(10, 28, 18, 10, 0, render_test_output));

    text_destroy(text);
    font_destroy(font);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_text_render_whitespace)
{
    int font;
    CHECK_TRUE(font = font_parse_freetype(font_data, font_data_size, 0, 0));
    render_setup(0);

    int text;
    CHECK_TRUE(text = font_create_text(font, "\xC2\xA0\xC2\xA0\xC2\xA0", 10,
                                       &(Vector3f){10, 10, 0},
                                       FONT_ALIGN_LEFT, 0));
    CHECK_FLOATEQUAL(text_advance(text), 10.5);
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
    CHECK_TRUE(font = font_parse_freetype(font_data, font_data_size, 0, 0));
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
    CHECK_TRUE(font = font_parse_freetype(font_data, font_data_size, 0, 0));
    render_setup(0);

    int text;
    CHECK_TRUE(text = font_create_text(font, render_test_input, 10,
                                       &(Vector3f){10, 10, 0}, FONT_ALIGN_LEFT,
                                       0));

    font_destroy(font);
    CHECK_FLOATEQUAL(text_advance(text), 18);
    text_render(text);
    text_destroy(text);
    CHECK_TRUE(check_render_result(10, 8, 18, 10, 0, render_test_output));

    return 1;
}

/*************************************************************************/
/*************************************************************************/
