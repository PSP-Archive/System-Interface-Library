/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/userdata.c: Tests for the high-level user data access functions.
 * These tests are also designed to cover all low-level code paths.
 */

#include "src/base.h"
#include "src/graphics.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/test.h"
#include "src/test/base.h"
#include "src/test/graphics/internal.h"
#include "src/test/userdata.h"
#include "src/texture.h"
#include "src/thread.h"
#include "src/userdata.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Does the sysdep variety being tested support userdata_get_data_path()? */
static uint8_t sysdep_has_data_path;

/* Helper functions for the sysdep variety being tested, as passed to
 * run_userdata_tests(). */
static int (*sysdep_init)(void);
static int (*sysdep_cleanup)(void);
static void *(*sysdep_get_screenshot)(int index,
                                      int *width_ret, int *height_ret);
static void (*sysdep_make_data_unwritable)(void);

/*************************************************************************/
/*************************** Helper functions ****************************/
/*************************************************************************/

/**
 * userdata_init_allocates_memory:  Return whether userdata_init()
 * allocates memory (and therefore should be tested for memory allocation
 * failures).
 *
 * [Return value]
 *     True if userdata_init() or any function it calls allocates memory,
 *     false if not.
 */
static int userdata_init_allocates_memory(void)
{
#if defined(SIL_PLATFORM_PSP) || defined(SIL_PLATFORM_WINDOWS)
    return 0;
#else
    return 1;
#endif
}

/*-----------------------------------------------------------------------*/

/**
 * gen_override_path:  Generate an override pathname appropriate to the
 * current environment.  Used to prepend system-specific prefixes when
 * using live userdata routines.
 *
 * The returned string may be stored in a static buffer, in which case it
 * will be overwritten by the next call to this function.
 *
 * [Parameters]
 *     path: Base pathname (relative to top of save data tree).
 * [Return value]
 *     Pathname to pass to userdata_override_file_path().
 */
static const char *gen_override_path(const char *path)
{
    PRECOND(path != NULL, return NULL);

    if (sys_test_userdata_use_live_routines) {
        /* No special cases currently needed. */
    }

    return path;
}

/*-----------------------------------------------------------------------*/

/**
 * get_screenshot_for_sys_test:  System-specific function to get saved
 * screenshot data for the stub implementation of the user data access
 * functions.
 */
static void *get_screenshot_for_sys_test(UNUSED int index,
                                         int *width_ret, int *height_ret)
{
    const void *image_pixels =
        sys_test_userdata_get_screenshot(width_ret, height_ret);
    if (!image_pixels) {
        return NULL;
    }
    void *copy;
    ASSERT(copy = mem_alloc((*width_ret) * (*height_ret) * 4, 0, 0));
    memcpy(copy, image_pixels, (*width_ret) * (*height_ret) * 4);
    return copy;
}

/*-----------------------------------------------------------------------*/

/**
 * make_data_unwritable_for_sys_test:  System-specific function to force
 * failure of write operations for the stub implementation of the user data
 * access functions.
 */
static void make_data_unwritable_for_sys_test(void)
{
    sys_test_userdata_writable = 0;
}

/*************************************************************************/
/***************** Test runner and init/cleanup routines *****************/
/*************************************************************************/

static int wrap_run_userdata_tests(void);
static int do_run_userdata_tests(void);

int test_userdata(void)
{
    /* These have to be run with the graphics engine initialized because
     * we make use of textures. */
    return run_tests_in_window(wrap_run_userdata_tests);
}

static int wrap_run_userdata_tests(void)
{
    return run_userdata_tests(
        0, NULL, NULL, get_screenshot_for_sys_test,
        make_data_unwritable_for_sys_test);
}

int run_userdata_tests(
    int has_data_path,
    int (*init_func)(void),
    int (*cleanup_func)(void),
    void *(*get_screenshot_func)(int index, int *width_ret, int *height_ret),
    void (*make_data_unwritable_func)(void))
{
    sysdep_has_data_path = (has_data_path != 0);
    sysdep_init = init_func;
    sysdep_cleanup = cleanup_func;
    sysdep_get_screenshot = get_screenshot_func;
    sysdep_make_data_unwritable = make_data_unwritable_func;
    return do_run_userdata_tests();
}

DEFINE_GENERIC_TEST_RUNNER(do_run_userdata_tests)

/*-----------------------------------------------------------------------*/

TEST_INIT(init)
{
    if (sysdep_init) {
        CHECK_TRUE((*sysdep_init)());
    }
    CHECK_TRUE(userdata_init());
    userdata_set_program_name("test");
    userdata_set_program_title("Userdata Test");
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
    graphics_flush_resources();
    userdata_cleanup();
    if (sysdep_cleanup) {
        CHECK_TRUE((*sysdep_cleanup)());
    }
    return 1;
}

/*************************************************************************/
/**************************** Save data tests ****************************/
/*************************************************************************/

TEST(test_savefile)
{
    int id;
    void *data;
    uint32_t size;
    int image;

    CHECK_TRUE(id = userdata_save_savefile(0, "111", 3, "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_status(id));  // Just to be sure it works.
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_load_savefile(0, &data, &size, NULL));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    mem_free(data);

    CHECK_TRUE(id = userdata_load_savefile(0, &data, &size, &image));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    CHECK_INTEQUAL(image, 0);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_image)
{
#ifndef SIL_UTILITY_INCLUDE_PNG
    /* At present, all systems use PNG format for save file images. */
    SKIP("PNG support not compiled in.");
#endif

    int texture;
    ASSERT(texture = texture_create(4, 3, 0, 0));
    uint8_t *pixels;
    ASSERT(pixels = texture_lock(texture));
    for (int i = 0; i < 4*3; i++) {
        const int y = i/4;
        pixels[i*4+0] = 0x11 * (y+1);
        pixels[i*4+1] = 0x22 * (y+1);
        pixels[i*4+2] = 0x33 * (y+1);
        pixels[i*4+3] = 0xFF;
    }
    texture_unlock(texture);

    int id;
    void *data;
    uint32_t size;
    int image;

    CHECK_TRUE(id = userdata_save_savefile(0, "111", 3, "title", "desc",
                                           texture));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    texture_destroy(texture);

    CHECK_TRUE(id = userdata_load_savefile(0, &data, &size, NULL));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    mem_free(data);

    CHECK_TRUE(id = userdata_load_savefile(0, &data, &size, &image));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    CHECK_TRUE(image);
    CHECK_INTEQUAL(texture_width(image), 4);
    CHECK_INTEQUAL(texture_height(image), 3);
    CHECK_FLOATEQUAL(texture_scale(image), 1);
    const uint8_t *image_pixels;
    CHECK_TRUE(image_pixels = (const uint8_t *)texture_lock_readonly(image));
    for (int i = 0; i < 4*3; i++) {
        const int y = i/4;
        CHECK_INTEQUAL(image_pixels[i*4+0], 0x11 * (y+1));
        CHECK_INTEQUAL(image_pixels[i*4+1], 0x22 * (y+1));
        CHECK_INTEQUAL(image_pixels[i*4+2], 0x33 * (y+1));
        CHECK_INTEQUAL(image_pixels[i*4+3], 0xFF);
    }
    mem_free(data);
    texture_destroy(image);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_image_flip)
{
#ifndef SIL_UTILITY_INCLUDE_PNG
    SKIP("PNG support not compiled in.");
#endif

    int texture;
    ASSERT(texture = texture_create(4, 3, 0, 0));
    uint8_t *pixels;
    ASSERT(pixels = texture_lock(texture));
    for (int i = 0; i < 4*3; i++) {
        const int y = i/4;
        pixels[i*4+0] = 0x11 * (y+1);
        pixels[i*4+1] = 0x22 * (y+1);
        pixels[i*4+2] = 0x33 * (y+1);
        pixels[i*4+3] = 0xFF;
    }
    texture_unlock(texture);

    int id;
    void *data;
    uint32_t size;
    int image;
    const uint8_t *image_pixels;

    userdata_set_flip_image_for_save(1);
    CHECK_TRUE(id = userdata_save_savefile(0, "111", 3, "title", "desc",
                                           texture));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    userdata_set_flip_image_for_save(0);
    texture_destroy(texture);

    CHECK_TRUE(id = userdata_load_savefile(0, &data, &size, NULL));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    mem_free(data);

    CHECK_TRUE(id = userdata_load_savefile(0, &data, &size, &image));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    CHECK_TRUE(image);
    CHECK_INTEQUAL(texture_width(image), 4);
    CHECK_INTEQUAL(texture_height(image), 3);
    CHECK_FLOATEQUAL(texture_scale(image), 1);
    CHECK_TRUE(image_pixels = (const uint8_t *)texture_lock_readonly(image));
    for (int i = 0; i < 4*3; i++) {
        const int y = i/4;
        CHECK_INTEQUAL(image_pixels[i*4+0], 0x11 * (3-y));
        CHECK_INTEQUAL(image_pixels[i*4+1], 0x22 * (3-y));
        CHECK_INTEQUAL(image_pixels[i*4+2], 0x33 * (3-y));
        CHECK_INTEQUAL(image_pixels[i*4+3], 0xFF);
    }
    mem_free(data);
    texture_destroy(image);

    /* Setting flip_image_for_save should not affect loaded images. */
    userdata_set_flip_image_for_save(1);
    CHECK_TRUE(id = userdata_load_savefile(0, &data, &size, &image));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    userdata_set_flip_image_for_save(0);
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    CHECK_TRUE(image);
    CHECK_INTEQUAL(texture_width(image), 4);
    CHECK_INTEQUAL(texture_height(image), 3);
    CHECK_FLOATEQUAL(texture_scale(image), 1);
    CHECK_TRUE(image_pixels = (const uint8_t *)texture_lock_readonly(image));
    for (int i = 0; i < 4*3; i++) {
        const int y = i/4;
        CHECK_INTEQUAL(image_pixels[i*4+0], 0x11 * (3-y));
        CHECK_INTEQUAL(image_pixels[i*4+1], 0x22 * (3-y));
        CHECK_INTEQUAL(image_pixels[i*4+2], 0x33 * (3-y));
        CHECK_INTEQUAL(image_pixels[i*4+3], 0xFF);
    }
    mem_free(data);
    texture_destroy(image);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_image_lock_failure)
{
    /* Check that failing to access texture data for a save file screenshot
     * does not prevent the file itself from being saved.  We don't protect
     * this test with a sysdep_get_screenshot check since we don't actually
     * write any image data. */

    int texture;
    ASSERT(texture = texture_create(4, 3, 0, 0));
    uint8_t *pixels;
    ASSERT(pixels = texture_lock(texture));
    for (int i = 0; i < 4*3; i++) {
        const int y = i/4;
        pixels[i*4+0] = 0x11 * (y+1);
        pixels[i*4+1] = 0x22 * (y+1);
        pixels[i*4+2] = 0x33 * (y+1);
        pixels[i*4+3] = 0xFF;
    }
    /* Do _not_ unlock the texture -- leave it locked so the lock call in
     * userdata_save_savefile() fails. */

    int id;
    void *data;
    uint32_t size;
    int image;

    CHECK_TRUE(id = userdata_save_savefile(0, "111", 3, "title", "desc",
                                           texture));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    texture_destroy(texture);

    CHECK_TRUE(id = userdata_load_savefile(0, &data, &size, &image));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    CHECK_FALSE(image);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_overwrite)
{
    int id;
    void *data;
    uint32_t size;

    CHECK_TRUE(id = userdata_save_savefile(0, "111", 3, "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_save_savefile(0, "!!!!", 4, "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_load_savefile(0, &data, &size, NULL));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 4);
    CHECK_MEMEQUAL(data, "!!!!", 4);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_overwrite_image)
{
#ifndef SIL_UTILITY_INCLUDE_PNG
    SKIP("PNG support not compiled in.");
#endif

    int texture;
    ASSERT(texture = texture_create(4, 3, 0, 0));
    uint8_t *pixels;
    ASSERT(pixels = texture_lock(texture));
    for (int i = 0; i < 4*3; i++) {
        pixels[i*4+0] = 0x11;
        pixels[i*4+1] = 0x22;
        pixels[i*4+2] = 0x33;
        pixels[i*4+3] = 0xFF;
    }
    texture_unlock(texture);

    int id;
    void *data;
    uint32_t size;
    int image;

    CHECK_TRUE(id = userdata_save_savefile(0, "111", 3, "title", "desc",
                                           texture));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    texture_destroy(texture);

    CHECK_TRUE(id = userdata_save_savefile(0, "!!!!", 4, "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_load_savefile(0, &data, &size, &image));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 4);
    CHECK_MEMEQUAL(data, "!!!!", 4);
    CHECK_INTEQUAL(image, 0);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_delete)
{
    int id;
    void *data;
    uint32_t size;

    CHECK_TRUE(id = userdata_save_savefile(0, "111", 3, "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_delete_savefile(0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_load_savefile(0, &data, &size, NULL));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_override_path)
{
    int id;
    void *data;
    uint32_t size;

    /* Check that a path-overridden save doesn't kill the original.  This
     * will fail either if the original is overwritten, or if the override
     * is not properly cancelled after the operation. */
    CHECK_TRUE(id = userdata_save_savefile(0, "111", 3, "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_TRUE(userdata_override_file_path(gen_override_path("foo")));
    CHECK_TRUE(id = userdata_save_savefile(0, "quux", 4, "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_TRUE(id = userdata_load_savefile(0, &data, &size, NULL));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    mem_free(data);

    /* Check that the load path can also be overridden. */
    CHECK_TRUE(userdata_override_file_path(gen_override_path("foo")));
    CHECK_TRUE(id = userdata_load_savefile(0, &data, &size, NULL));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 4);
    CHECK_MEMEQUAL(data, "quux", 4);
    mem_free(data);

    /* Check that a nonexistent path causes failure. */
    CHECK_TRUE(userdata_override_file_path(gen_override_path("bar")));
    CHECK_TRUE(id = userdata_load_savefile(0, &data, &size, NULL));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));

    /* Check that a path override can be cancelled before the operation
     * is started. */
    CHECK_TRUE(userdata_override_file_path(gen_override_path("foo")));
    CHECK_TRUE(userdata_override_file_path(NULL));
    CHECK_TRUE(id = userdata_load_savefile(0, &data, &size, NULL));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_override_path_memory_failure)
{
    int id;
    void *data;
    uint32_t size;

    /* Perform an initial operation to confirm the path override takes
     * place. */
    CHECK_TRUE(id = userdata_save_savefile(0, "111", 3, "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_TRUE(id = userdata_load_savefile(0, &data, &size, NULL));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    mem_free(data);

    CHECK_MEMORY_FAILURES(
        (userdata_override_file_path(gen_override_path("foo"))
         && (id = userdata_save_savefile(0, "quux", 4, "title", "desc", 0))
         && (userdata_wait(id), userdata_get_result(id))));
    CHECK_TRUE(userdata_override_file_path(gen_override_path("foo")));
    CHECK_TRUE(id = userdata_load_savefile(0, &data, &size, NULL));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 4);
    CHECK_MEMEQUAL(data, "quux", 4);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_override_path_image)
{
#ifndef SIL_UTILITY_INCLUDE_PNG
    SKIP("PNG support not compiled in.");
#endif

    int texture, texture2;
    uint8_t *pixels;
    ASSERT(texture = texture_create(4, 3, 0, 0));
    ASSERT(pixels = texture_lock(texture));
    for (int i = 0; i < 4*3; i++) {
        pixels[i*4+0] = 0x11;
        pixels[i*4+1] = 0x22;
        pixels[i*4+2] = 0x33;
        pixels[i*4+3] = 0xFF;
    }
    texture_unlock(texture);
    ASSERT(texture2 = texture_create(3, 4, 0, 0));
    ASSERT(pixels = texture_lock(texture2));
    for (int i = 0; i < 3*4; i++) {
        pixels[i*4+0] = 0xAA;
        pixels[i*4+1] = 0xBB;
        pixels[i*4+2] = 0xCC;
        pixels[i*4+3] = 0xFF;
    }
    texture_unlock(texture2);

    int id;
    void *data;
    uint32_t size;
    int image;

    /* Check that a path-overridden save still overwrites the original image
     * when appropriate (i.e., always during testing, or if the pathname
     * excluding file extension matches for the live POSIX implementation). */
    CHECK_TRUE(id = userdata_save_savefile(0, "111", 3, "title", "desc",
                                           texture));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    const char *override_path = "save/save-0000.foo";
    if (sys_test_userdata_use_live_routines) {
        /* No special cases currently needed. */
    }
    CHECK_TRUE(userdata_override_file_path(override_path));
    CHECK_TRUE(id = userdata_save_savefile(0, "quux", 4, "title", "desc",
                                           texture2));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_TRUE(id = userdata_load_savefile(0, &data, &size, &image));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    CHECK_TRUE(image);
    CHECK_INTEQUAL(texture_width(image), 3);
    CHECK_INTEQUAL(texture_height(image), 4);
    CHECK_FLOATEQUAL(texture_scale(image), 1);
    const uint8_t *image_pixels;
    CHECK_TRUE(image_pixels = (const uint8_t *)texture_lock_readonly(image));
    for (int i = 0; i < 3*4; i++) {
        CHECK_INTEQUAL(image_pixels[i*4+0], 0xAA);
        CHECK_INTEQUAL(image_pixels[i*4+1], 0xBB);
        CHECK_INTEQUAL(image_pixels[i*4+2], 0xCC);
        CHECK_INTEQUAL(image_pixels[i*4+3], 0xFF);
    }
    mem_free(data);
    texture_destroy(image);

    /* Check that loading a path-overridden save without an appropriate
     * path override properly fails (and doesn't crash) if there is no
     * pre-existing data for that save file in the standard location. */
    CHECK_TRUE(userdata_override_file_path(gen_override_path("foo")));
    CHECK_TRUE(id = userdata_save_savefile(1, "quux", 4, "title", "desc",
                                           texture2));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_TRUE(id = userdata_load_savefile(1, &data, &size, NULL));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));

    texture_destroy(texture);
    texture_destroy(texture2);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_override_path_delete)
{
    int id;
    void *data;
    uint32_t size;

    CHECK_TRUE(userdata_override_file_path(gen_override_path("foo")));
    CHECK_TRUE(id = userdata_save_savefile(0, "111", 3, "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(userdata_override_file_path(gen_override_path("foo")));
    CHECK_TRUE(id = userdata_delete_savefile(0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(userdata_override_file_path(gen_override_path("foo")));
    CHECK_TRUE(id = userdata_load_savefile(0, &data, &size, NULL));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_multiple)
{
    int id;
    void *data;
    uint32_t size;

    CHECK_TRUE(id = userdata_save_savefile(0, "111", 3, "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_save_savefile(1, "2222", 4, "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_load_savefile(0, &data, &size, NULL));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    mem_free(data);

    CHECK_TRUE(id = userdata_load_savefile(1, &data, &size, NULL));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 4);
    CHECK_MEMEQUAL(data, "2222", 4);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_scan)
{
    int id;
    uint8_t *data;

    CHECK_TRUE(id = userdata_save_savefile(0, "111", 3, "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_save_savefile(2, "2222", 4, "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_save_savefile(3, "33333", 5, "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    data = NULL;
    CHECK_TRUE(id = userdata_scan_savefiles(0, 4, &data));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_TRUE(data);
    CHECK_TRUE(data[0]);
    CHECK_FALSE(data[1]);
    CHECK_TRUE(data[2]);
    CHECK_TRUE(data[3]);
    mem_free(data);

    data = NULL;
    CHECK_TRUE(id = userdata_scan_savefiles(1, 2, &data));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_TRUE(data);
    CHECK_FALSE(data[0]);
    CHECK_TRUE(data[1]);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_save_memory_failure)
{
    /* We don't skip this image test even if !SIL_UTILITY_INCLUDE_PNG
     * because we allow saving of the image to fail while still returning
     * success, so it's okay if libpng isn't available -- we still want to
     * exercise the code path. */
    int texture;
    ASSERT(texture = texture_create(4, 3, 0, 0));
    uint8_t *pixels;
    ASSERT(pixels = texture_lock(texture));
    for (int i = 0; i < 4*3; i++) {
        pixels[i*4+0] = 0x11;
        pixels[i*4+1] = 0x22;
        pixels[i*4+2] = 0x33;
        pixels[i*4+3] = 0xFF;
    }
    texture_unlock(texture);

    int id;
    void *data;
    uint32_t size;
    int image;

    CHECK_USERDATA_MEMORY_FAILURES(
        userdata_save_savefile(0, "111", 3, "title", "desc", texture));
    texture_destroy(texture);

    CHECK_TRUE(id = userdata_load_savefile(0, &data, &size, &image));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    texture_destroy(image);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_load_memory_failure)
{
    int texture;
    ASSERT(texture = texture_create(4, 3, 0, 0));
    uint8_t *pixels;
    ASSERT(pixels = texture_lock(texture));
    for (int i = 0; i < 4*3; i++) {
        pixels[i*4+0] = 0x11;
        pixels[i*4+1] = 0x22;
        pixels[i*4+2] = 0x33;
        pixels[i*4+3] = 0xFF;
    }
    texture_unlock(texture);

    int id;
    void *data;
    uint32_t size;
    int image;

    CHECK_TRUE(id = userdata_save_savefile(0, "111", 3, "title", "desc",
                                           texture));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    texture_destroy(texture);

    CHECK_USERDATA_MEMORY_FAILURES(
        userdata_load_savefile(0, &data, &size, &image));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    texture_destroy(image);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_delete_memory_failure)
{
    int id;
    void *data;
    uint32_t size;

    CHECK_TRUE(id = userdata_save_savefile(0, "111", 3, "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_USERDATA_MEMORY_FAILURES(userdata_delete_savefile(0));

    CHECK_TRUE(id = userdata_load_savefile(0, &data, &size, NULL));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_scan_memory_failure)
{
    int id;
    uint8_t *data;

    CHECK_TRUE(id = userdata_save_savefile(0, "111", 3, "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_save_savefile(2, "2222", 4, "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    data = NULL;
    CHECK_USERDATA_MEMORY_FAILURES(userdata_scan_savefiles(0, 4, &data));
    CHECK_TRUE(data);
    CHECK_TRUE(data[0]);
    CHECK_FALSE(data[1]);
    CHECK_TRUE(data[2]);
    CHECK_FALSE(data[3]);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_load_missing)
{
    int id;
    void *data;
    uint32_t size;

    CHECK_TRUE(id = userdata_load_savefile(0, &data, &size, NULL));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_delete_nonexistent)
{
    int id;

    CHECK_TRUE(id = userdata_delete_savefile(0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_write_failure)
{
    if (!sysdep_make_data_unwritable) {
        return 1;
    }

    int id;
    void *data;
    uint32_t size;

    CHECK_TRUE(id = userdata_save_savefile(0, "111", 3, "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    (*sysdep_make_data_unwritable)();

    CHECK_TRUE(id = userdata_save_savefile(1, "111", 3, "title", "desc", 0));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_load_savefile(1, &data, &size, NULL));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_delete_savefile(1));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_delete_savefile(0));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_load_savefile(0, &data, &size, NULL));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_savefile_invalid)
{
    int id;
    void *data;
    uint32_t size;

    CHECK_FALSE(userdata_save_savefile(-1, "111", 3, "title", "desc", 0));
    CHECK_FALSE(userdata_save_savefile(0, NULL, 3, "title", "desc", 0));
    CHECK_FALSE(userdata_save_savefile(0, "111", 3, NULL, "desc", 0));
    CHECK_FALSE(userdata_save_savefile(0, "111", 3, "title", NULL, 0));

    CHECK_TRUE(id = userdata_save_savefile(0, "111", 3, "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_TRUE(id = userdata_load_savefile(0, &data, &size, NULL));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    mem_free(data);
    CHECK_FALSE(userdata_load_savefile(-1, &data, &size, NULL));
    CHECK_FALSE(userdata_load_savefile(0, NULL, &size, NULL));
    CHECK_FALSE(userdata_load_savefile(0, &data, NULL, NULL));
    CHECK_FALSE(userdata_scan_savefiles(-1, 2, (uint8_t **)&data));
    CHECK_FALSE(userdata_scan_savefiles(0, 0, (uint8_t **)&data));
    CHECK_FALSE(userdata_scan_savefiles(0, 1, NULL));

    CHECK_FALSE(userdata_delete_savefile(-1));

    return 1;
}

/*************************************************************************/
/************************** Settings data tests **************************/
/*************************************************************************/

TEST(test_settings)
{
    int id;
    void *data;
    uint32_t size;

    CHECK_TRUE(id = userdata_save_settings("sss", 3, "title", "desc"));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_load_settings(&data, &size));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "sss", 3);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_settings_memory_failure)
{
    int id;
    void *data;
    uint32_t size;

    CHECK_USERDATA_MEMORY_FAILURES(
        userdata_save_settings("sss", 3, "title", "desc"));
    CHECK_USERDATA_MEMORY_FAILURES(userdata_load_settings(&data, &size));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "sss", 3);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_settings_overwrite)
{
    int id;
    void *data;
    uint32_t size;

    CHECK_TRUE(id = userdata_save_settings("sss", 3, "title", "desc"));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_save_settings("SSSS", 4, "title", "desc"));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_load_settings(&data, &size));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 4);
    CHECK_MEMEQUAL(data, "SSSS", 4);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_settings_override_path)
{
    int id;
    void *data;
    uint32_t size;

    /* Check that a path-overridden save doesn't kill the original.  This
     * will fail either if the original is overwritten, or if the override
     * is not properly cancelled after the operation. */
    CHECK_TRUE(id = userdata_save_settings("sss", 3, "title", "desc"));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_TRUE(userdata_override_file_path(gen_override_path("foo")));
    CHECK_TRUE(id = userdata_save_settings("quux", 4, "title", "desc"));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_TRUE(id = userdata_load_settings(&data, &size));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "sss", 3);
    mem_free(data);

    /* Check that the load path can also be overridden. */
    CHECK_TRUE(userdata_override_file_path(gen_override_path("foo")));
    CHECK_TRUE(id = userdata_load_settings(&data, &size));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 4);
    CHECK_MEMEQUAL(data, "quux", 4);
    mem_free(data);

    /* Check that a path override can be cancelled before the operation
     * is started. */
    CHECK_TRUE(userdata_override_file_path(gen_override_path("foo")));
    CHECK_TRUE(userdata_override_file_path(NULL));
    CHECK_TRUE(id = userdata_load_settings(&data, &size));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "sss", 3);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_settings_load_missing)
{
    int id;
    void *data;
    uint32_t size;

    CHECK_TRUE(id = userdata_load_settings(&data, &size));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_settings_write_failure)
{
    if (!sysdep_make_data_unwritable) {
        return 1;
    }

    int id;
    void *data;
    uint32_t size;

    (*sysdep_make_data_unwritable)();

    CHECK_TRUE(id = userdata_save_settings("sss", 3, "title", "desc"));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_load_settings(&data, &size));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_settings_invalid)
{
    int id;
    void *data;
    uint32_t size;

    CHECK_FALSE(userdata_save_settings(NULL, 3, "title", "desc"));
    CHECK_FALSE(userdata_save_settings("sss", 3, NULL, "desc"));
    CHECK_FALSE(userdata_save_settings("sss", 3, "title", NULL));

    CHECK_TRUE(id = userdata_save_settings("sss", 3, "title", "desc"));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_TRUE(id = userdata_load_settings(&data, &size));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    mem_free(data);
    CHECK_FALSE(userdata_load_settings(NULL, &size));
    CHECK_FALSE(userdata_load_settings(&data, NULL));

    return 1;
}

/*************************************************************************/
/*************************** Screenshot tests ****************************/
/*************************************************************************/

TEST(test_screenshot)
{
    if (!sysdep_get_screenshot) {
        return 1;
    }

    int texture;
    ASSERT(texture = texture_create(2, 4, 0, 0));
    uint8_t *pixels;
    ASSERT(pixels = texture_lock(texture));
    for (int i = 0; i < 2*4; i++) {
        const int y = i/2;
        pixels[i*4+0] = 0xA0 + y;
        pixels[i*4+1] = 0xB0 + y;
        pixels[i*4+2] = 0xC0 + y;
        pixels[i*4+3] = 0xFF;
    }
    texture_unlock(texture);

    int id;
    CHECK_TRUE(id = userdata_save_screenshot(texture));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    texture_destroy(texture);

    int image_width, image_height;
    uint8_t *image_pixels;
    CHECK_TRUE(image_pixels =
                   (*sysdep_get_screenshot)(0, &image_width, &image_height));
    CHECK_INTEQUAL(image_width, 2);
    CHECK_INTEQUAL(image_height, 4);
    for (int i = 0; i < 2*4; i++) {
        const int y = i/2;
        CHECK_INTEQUAL(image_pixels[i*4+0], 0xA0 + y);
        CHECK_INTEQUAL(image_pixels[i*4+1], 0xB0 + y);
        CHECK_INTEQUAL(image_pixels[i*4+2], 0xC0 + y);
        CHECK_INTEQUAL(image_pixels[i*4+3], 0xFF);
    }
    mem_free(image_pixels);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_screenshot_flip)
{
    if (!sysdep_get_screenshot) {
        return 1;
    }

    int texture;
    ASSERT(texture = texture_create(2, 4, 0, 0));
    uint8_t *pixels;
    ASSERT(pixels = texture_lock(texture));
    for (int i = 0; i < 2*4; i++) {
        const int y = i/2;
        pixels[i*4+0] = 0xA0 + y;
        pixels[i*4+1] = 0xB0 + y;
        pixels[i*4+2] = 0xC0 + y;
        pixels[i*4+3] = 0xFF;
    }
    texture_unlock(texture);

    int id;
    userdata_set_flip_image_for_save(1);
    CHECK_TRUE(id = userdata_save_screenshot(texture));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    userdata_set_flip_image_for_save(0);
    texture_destroy(texture);

    int image_width, image_height;
    uint8_t *image_pixels;
    CHECK_TRUE(image_pixels =
                   (*sysdep_get_screenshot)(0, &image_width, &image_height));
    CHECK_INTEQUAL(image_width, 2);
    CHECK_INTEQUAL(image_height, 4);
    for (int i = 0; i < 2*4; i++) {
        const int y = i/2;
        CHECK_INTEQUAL(image_pixels[i*4+0], 0xA0 + (3-y));
        CHECK_INTEQUAL(image_pixels[i*4+1], 0xB0 + (3-y));
        CHECK_INTEQUAL(image_pixels[i*4+2], 0xC0 + (3-y));
        CHECK_INTEQUAL(image_pixels[i*4+3], 0xFF);
    }
    mem_free(image_pixels);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_screenshot_2)
{
    if (!sysdep_get_screenshot) {
        return 1;
    }

    int texture;
    ASSERT(texture = texture_create(2, 4, 0, 0));
    uint8_t *pixels;
    ASSERT(pixels = texture_lock(texture));
    for (int i = 0; i < 2*4; i++) {
        pixels[i*4+0] = 0xAA;
        pixels[i*4+1] = 0xBB;
        pixels[i*4+2] = 0xCC;
        pixels[i*4+3] = 0xFF;
    }
    texture_unlock(texture);

    int id;
    CHECK_TRUE(id = userdata_save_screenshot(texture));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    texture_destroy(texture);

    ASSERT(texture = texture_create(2, 4, 0, 0));
    ASSERT(pixels = texture_lock(texture));
    for (int i = 0; i < 2*4; i++) {
        pixels[i*4+0] = 0x44;
        pixels[i*4+1] = 0x66;
        pixels[i*4+2] = 0x99;
        pixels[i*4+3] = 0xFF;
    }
    texture_unlock(texture);

    CHECK_TRUE(id = userdata_save_screenshot(texture));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    texture_destroy(texture);

    int image_width, image_height;
    uint8_t *image_pixels;
    CHECK_TRUE(image_pixels =
                   (*sysdep_get_screenshot)(1, &image_width, &image_height));
    CHECK_INTEQUAL(image_width, 2);
    CHECK_INTEQUAL(image_height, 4);
    for (int i = 0; i < 2*4; i++) {
        CHECK_INTEQUAL(image_pixels[i*4+0], 0x44);
        CHECK_INTEQUAL(image_pixels[i*4+1], 0x66);
        CHECK_INTEQUAL(image_pixels[i*4+2], 0x99);
        CHECK_INTEQUAL(image_pixels[i*4+3], 0xFF);
    }
    mem_free(image_pixels);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_screenshot_memory_failure)
{
    if (!sysdep_get_screenshot) {
        return 1;
    }

    int texture;
    ASSERT(texture = texture_create(2, 4, 0, 0));
    uint8_t *pixels;
    ASSERT(pixels = texture_lock(texture));
    for (int i = 0; i < 2*4; i++) {
        pixels[i*4+0] = 0xAA;
        pixels[i*4+1] = 0xBB;
        pixels[i*4+2] = 0xCC;
        pixels[i*4+3] = 0xFF;
    }
    texture_unlock(texture);

    int id;

    CHECK_USERDATA_MEMORY_FAILURES(userdata_save_screenshot(texture));
    texture_destroy(texture);

    int image_width, image_height;
    uint8_t *image_pixels;
    CHECK_TRUE(image_pixels =
                   (*sysdep_get_screenshot)(0, &image_width, &image_height));
    CHECK_INTEQUAL(image_width, 2);
    CHECK_INTEQUAL(image_height, 4);
    for (int i = 0; i < 2*4; i++) {
        CHECK_INTEQUAL(image_pixels[i*4+0], 0xAA);
        CHECK_INTEQUAL(image_pixels[i*4+1], 0xBB);
        CHECK_INTEQUAL(image_pixels[i*4+2], 0xCC);
        CHECK_INTEQUAL(image_pixels[i*4+3], 0xFF);
    }
    mem_free(image_pixels);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_screenshot_write_failure)
{
    if (!sysdep_get_screenshot) {
        return 1;
    }
    if (!sysdep_make_data_unwritable) {
        return 1;
    }

    int texture;
    ASSERT(texture = texture_create(2, 4, 0, 0));

    (*sysdep_make_data_unwritable)();

    int id;
    CHECK_TRUE(id = userdata_save_screenshot(texture));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));
    texture_destroy(texture);

    int image_width, image_height;
    CHECK_FALSE((*sysdep_get_screenshot)(0, &image_width, &image_height));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_screenshot_invalid)
{
    CHECK_FALSE(userdata_save_screenshot(0));
    return 1;
}

/*************************************************************************/
/**************************** Data file tests ****************************/
/*************************************************************************/

TEST(test_data)
{
    int id;
    void *data;
    uint32_t size;

    CHECK_TRUE(id = userdata_save_data("foo", "foo", 3));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_load_data("foo", &data, &size));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "foo", 3);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_data_memory_failure)
{
    int id;
    void *data;
    uint32_t size;

    CHECK_USERDATA_MEMORY_FAILURES(userdata_save_data("foo", "foo", 3));
    CHECK_USERDATA_MEMORY_FAILURES(userdata_load_data("foo", &data, &size));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "foo", 3);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_data_overwrite)
{
    int id;
    void *data;
    uint32_t size;

    CHECK_TRUE(id = userdata_save_data("foo", "foo", 3));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_save_data("foo", "quux", 4));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_load_data("foo", &data, &size));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 4);
    CHECK_MEMEQUAL(data, "quux", 4);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_data_delete)
{
    int id;
    void *data;
    uint32_t size;

    CHECK_TRUE(id = userdata_save_data("foo", "foo", 3));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_delete_data("foo"));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_load_data("foo", &data, &size));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_data_delete_memory_failure)
{
    int id;
    void *data;
    uint32_t size;

    CHECK_TRUE(id = userdata_save_data("foo", "foo", 3));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_USERDATA_MEMORY_FAILURES(userdata_delete_data("foo"));

    CHECK_TRUE(id = userdata_load_data("foo", &data, &size));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_data_override_path)
{
    int id;
    void *data;
    uint32_t size;

    CHECK_TRUE(userdata_override_file_path(gen_override_path("bar")));
    CHECK_TRUE(id = userdata_save_data("foo", "foo", 3));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_load_data("bar", &data, &size));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "foo", 3);
    mem_free(data);

    CHECK_TRUE(userdata_override_file_path(gen_override_path("bar")));
    CHECK_TRUE(id = userdata_delete_data("foo"));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_load_data("bar", &data, &size));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_data_multiple)
{
    int id;
    void *data;
    uint32_t size;

    CHECK_TRUE(id = userdata_save_data("foo", "foo", 3));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_save_data("bar", "quux", 4));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_load_data("foo", &data, &size));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "foo", 3);
    mem_free(data);

    CHECK_TRUE(id = userdata_load_data("bar", &data, &size));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 4);
    CHECK_MEMEQUAL(data, "quux", 4);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_data_load_missing)
{
    int id;
    void *data;
    uint32_t size;

    CHECK_TRUE(id = userdata_load_data("foo", &data, &size));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_data_delete_nonexistent)
{
    int id;

    CHECK_TRUE(id = userdata_delete_data("foo"));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_data_write_failure)
{
    if (!sysdep_make_data_unwritable) {
        return 1;
    }

    int id;
    void *data;
    uint32_t size;

    CHECK_TRUE(id = userdata_save_data("foo", "foo", 3));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    (*sysdep_make_data_unwritable)();

    CHECK_TRUE(id = userdata_save_data("bar", "quux", 4));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_load_data("bar", &data, &size));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_delete_data("bar"));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_delete_data("foo"));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_load_data("foo", &data, &size));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "foo", 3);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_data_invalid)
{
    int id;
    void *data;
    uint32_t size;

    CHECK_FALSE(userdata_save_data(NULL, "foo", 3));
    CHECK_FALSE(userdata_save_data("foo", NULL, 3));

    CHECK_TRUE(id = userdata_save_data("foo", "foo", 3));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_TRUE(id = userdata_load_data("foo", &data, &size));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    mem_free(data);
    CHECK_FALSE(userdata_load_data(NULL, &data, &size));
    CHECK_FALSE(userdata_load_data("foo", NULL, &size));
    CHECK_FALSE(userdata_load_data("foo", &data, NULL));

    CHECK_FALSE(userdata_delete_data(NULL));

    return 1;
}

/*************************************************************************/
/*************************** Statistics tests ****************************/
/*************************************************************************/

TEST(test_stats)
{
    static const struct UserStatInfo stats[] = {
        {.id = 0, .type = USERDATA_STAT_FLAG},
        {.id = 2, .type = USERDATA_STAT_UINT32},
        {.id = 3, .type = USERDATA_STAT_DOUBLE},
        {.id = 5, .type = USERDATA_STAT_UINT32_MAX},
        {.id = 7, .type = USERDATA_STAT_DOUBLE_MAX},
    };
    int id;

    /* Register stats, and check that they're all initialized to zero. */
    CHECK_TRUE(id = userdata_register_stats(stats, lenof(stats)));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(2), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(3), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(5), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(7), 0);

    /* Check that clearing a flag that is already clear does nothing. */
    CHECK_TRUE(userdata_set_stat(0, 0));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 0);

    /* Set values, and check that they can be read again. */
    CHECK_TRUE(userdata_set_stat(0, -999));  // Flag, should turn into 1.
    CHECK_TRUE(userdata_set_stat(2, 100));
    CHECK_TRUE(userdata_set_stat(3, 1e10));
    CHECK_TRUE(userdata_set_stat(5, 100));
    CHECK_TRUE(userdata_set_stat(7, 1e10));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 1);
    CHECK_DOUBLEEQUAL(userdata_get_stat(2), 100);
    CHECK_DOUBLEEQUAL(userdata_get_stat(3), 1e10);
    CHECK_DOUBLEEQUAL(userdata_get_stat(5), 100);
    CHECK_DOUBLEEQUAL(userdata_get_stat(7), 1e10);

    /* Check that attempts to reset flags or lower _MAX values fail. */
    CHECK_TRUE(userdata_set_stat(0, 0));
    CHECK_TRUE(userdata_set_stat(2, 10));
    CHECK_TRUE(userdata_set_stat(3, 1e5));
    CHECK_TRUE(userdata_set_stat(5, 10));
    CHECK_TRUE(userdata_set_stat(7, 1e5));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 1);
    CHECK_DOUBLEEQUAL(userdata_get_stat(2), 10);
    CHECK_DOUBLEEQUAL(userdata_get_stat(3), 1e5);
    CHECK_DOUBLEEQUAL(userdata_get_stat(5), 100);
    CHECK_DOUBLEEQUAL(userdata_get_stat(7), 1e10);

    /* Check that setting a flag that is already set does nothing. */
    CHECK_TRUE(userdata_set_stat(0, 1));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 1);

    /* Check that updating persistent storage succeeds and does not modify
     * any values. */
    CHECK_TRUE(id = userdata_update_stats());
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 1);
    CHECK_DOUBLEEQUAL(userdata_get_stat(2), 10);
    CHECK_DOUBLEEQUAL(userdata_get_stat(3), 1e5);
    CHECK_DOUBLEEQUAL(userdata_get_stat(5), 100);
    CHECK_DOUBLEEQUAL(userdata_get_stat(7), 1e10);

    /* Check that an update call when no changes have been made returns
     * false. */
    CHECK_FALSE(userdata_update_stats());

    /* Check that non-uint32 values are rejected for UINT32 stats. */
    CHECK_FALSE(userdata_set_stat(2, 123.4));
    CHECK_FALSE(userdata_set_stat(5, 1e10));
    CHECK_FALSE(userdata_set_stat(2, -1));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 1);
    CHECK_DOUBLEEQUAL(userdata_get_stat(2), 10);
    CHECK_DOUBLEEQUAL(userdata_get_stat(3), 1e5);
    CHECK_DOUBLEEQUAL(userdata_get_stat(5), 100);
    CHECK_DOUBLEEQUAL(userdata_get_stat(7), 1e10);

    /* Check that clear() clears everything, including flags and _MAX stats. */
    CHECK_TRUE(id = userdata_clear_stats());
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(2), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(3), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(5), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(7), 0);

    /* Check that a second clear() does nothing (and doesn't break). */
    CHECK_TRUE(id = userdata_clear_stats());
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(2), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(3), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(5), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(7), 0);

    /* Check that a second register succeeds and preserves values. */
    CHECK_TRUE(userdata_set_stat(0, 1));
    CHECK_TRUE(userdata_set_stat(2, 100));
    CHECK_TRUE(userdata_set_stat(3, 1e10));
    CHECK_TRUE(userdata_set_stat(5, 100));
    CHECK_TRUE(userdata_set_stat(7, 1e10));
    CHECK_TRUE(id = userdata_update_stats());
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_TRUE(id = userdata_register_stats(stats, lenof(stats)));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 1);
    CHECK_DOUBLEEQUAL(userdata_get_stat(2), 100);
    CHECK_DOUBLEEQUAL(userdata_get_stat(3), 1e10);
    CHECK_DOUBLEEQUAL(userdata_get_stat(5), 100);
    CHECK_DOUBLEEQUAL(userdata_get_stat(7), 1e10);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_stats_memory_failure)
{
    /* Same as (portions of) test_stats(), except that we bracket all
     * userdata calls with CHECK_USERDATA_MEMORY_FAILURES(). */

    static const struct UserStatInfo stats[] = {
        {.id = 0, .type = USERDATA_STAT_FLAG},
        {.id = 2, .type = USERDATA_STAT_UINT32},
        {.id = 3, .type = USERDATA_STAT_DOUBLE},
        {.id = 5, .type = USERDATA_STAT_UINT32_MAX},
        {.id = 7, .type = USERDATA_STAT_DOUBLE_MAX},
    };
    int id;

    /* Re-initialize so we can run a memory failure test on userdata_init(). */
    if (userdata_init_allocates_memory()) {
        userdata_cleanup();
        CHECK_MEMORY_FAILURES(userdata_init());
    }
    userdata_set_program_name("test");
    userdata_set_program_title("Userdata Test");

    /* Register stats, and check that they're all initialized to zero. */
    CHECK_USERDATA_MEMORY_FAILURES(
        userdata_register_stats(stats, lenof(stats)));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(2), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(3), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(5), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(7), 0);

    /* Check that memory failures while updating don't cause the updates
     * to be lost from permanent storage. */
    CHECK_TRUE(userdata_set_stat(0, 1));
    CHECK_TRUE(userdata_set_stat(2, 100));
    CHECK_TRUE(userdata_set_stat(3, 1e10));
    CHECK_TRUE(userdata_set_stat(5, 100));
    CHECK_TRUE(userdata_set_stat(7, 1e10));
    CHECK_USERDATA_MEMORY_FAILURES(userdata_update_stats());
    CHECK_USERDATA_MEMORY_FAILURES(
        userdata_register_stats(stats, lenof(stats)));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 1);
    CHECK_DOUBLEEQUAL(userdata_get_stat(2), 100);
    CHECK_DOUBLEEQUAL(userdata_get_stat(3), 1e10);
    CHECK_DOUBLEEQUAL(userdata_get_stat(5), 100);
    CHECK_DOUBLEEQUAL(userdata_get_stat(7), 1e10);

    /* Check memory failures while clearing. */
    CHECK_TRUE(userdata_set_stat(0, 1));
    CHECK_TRUE(userdata_set_stat(2, 100));
    CHECK_TRUE(userdata_set_stat(3, 1e10));
    CHECK_TRUE(userdata_set_stat(5, 100));
    CHECK_TRUE(userdata_set_stat(7, 1e10));
    CHECK_USERDATA_MEMORY_FAILURES(userdata_clear_stats());
    CHECK_USERDATA_MEMORY_FAILURES(
        userdata_register_stats(stats, lenof(stats)));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(2), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(3), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(5), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(7), 0);

    /* Check that a second register succeeds and preserves values. */
    CHECK_TRUE(userdata_set_stat(0, 1));
    CHECK_TRUE(userdata_set_stat(2, 100));
    CHECK_TRUE(userdata_set_stat(3, 1e10));
    CHECK_TRUE(userdata_set_stat(5, 100));
    CHECK_TRUE(userdata_set_stat(7, 1e10));
    CHECK_USERDATA_MEMORY_FAILURES(userdata_update_stats());
    CHECK_USERDATA_MEMORY_FAILURES(
        userdata_register_stats(stats, lenof(stats)));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 1);
    CHECK_DOUBLEEQUAL(userdata_get_stat(2), 100);
    CHECK_DOUBLEEQUAL(userdata_get_stat(3), 1e10);
    CHECK_DOUBLEEQUAL(userdata_get_stat(5), 100);
    CHECK_DOUBLEEQUAL(userdata_get_stat(7), 1e10);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_stats_parallel)
{
    static const struct UserStatInfo stats[] = {
        {.id = 0, .type = USERDATA_STAT_FLAG},
        {.id = 2, .type = USERDATA_STAT_UINT32},
    };
    int id;

    /* Set stat 2, but don't let the operation complete yet. */
    CHECK_TRUE(id = userdata_register_stats(stats, lenof(stats)));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_TRUE(userdata_set_stat(2, 100));
    CHECK_TRUE(id = userdata_update_stats());

    /* Set both stats, then let the earlier update complete. */
    CHECK_TRUE(userdata_set_stat(0, 1));
    CHECK_TRUE(userdata_set_stat(2, 200));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    /* Run a second update, then reregister the stats and ensure that all
     * changes were committed to persistent storage. */
    CHECK_TRUE(id = userdata_update_stats());
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_TRUE(id = userdata_register_stats(stats, lenof(stats)));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 1);
    CHECK_DOUBLEEQUAL(userdata_get_stat(2), 200);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_stats_wrong_id)
{
    static const struct UserStatInfo stats[] = {
        {.id = 0, .type = USERDATA_STAT_FLAG},
    };
    int id;

    CHECK_TRUE(id = userdata_register_stats(stats, lenof(stats)));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(userdata_set_stat(0, 1));
    CHECK_FALSE(userdata_set_stat(1, 1));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 1);
    CHECK_DOUBLEEQUAL(userdata_get_stat(1), 0);

    CHECK_TRUE(id = userdata_update_stats());
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_TRUE(id = userdata_register_stats(stats, lenof(stats)));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 1);
    CHECK_DOUBLEEQUAL(userdata_get_stat(1), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_stats_nan)
{
    static const struct UserStatInfo stats[] = {
        {.id = 0, .type = USERDATA_STAT_FLAG},
        {.id = 1, .type = USERDATA_STAT_DOUBLE},
    };
    int id;

    CHECK_TRUE(id = userdata_register_stats(stats, lenof(stats)));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    CHECK_TRUE(userdata_set_stat(0, 1));
    CHECK_FALSE(userdata_set_stat(1, DOUBLE_NAN()));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 1);
    CHECK_DOUBLEEQUAL(userdata_get_stat(1), 0);

    CHECK_TRUE(id = userdata_update_stats());
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_TRUE(id = userdata_register_stats(stats, lenof(stats)));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 1);
    CHECK_DOUBLEEQUAL(userdata_get_stat(1), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_stats_invalid)
{
    static const struct UserStatInfo stats_invalid_type[] = {
        {.id = 0, .type = -1},
        {.id = 0, .type = USERDATA_STAT_UINT32},
    };
    static const struct UserStatInfo stats_duplicate_id[] = {
        {.id = 0, .type = USERDATA_STAT_FLAG},
        {.id = 0, .type = USERDATA_STAT_UINT32},
    };
    static const struct UserStatInfo stats[] = {
        {.id = 0, .type = USERDATA_STAT_FLAG},
        {.id = 2, .type = USERDATA_STAT_UINT32},
        {.id = 3, .type = USERDATA_STAT_DOUBLE},
        {.id = 5, .type = USERDATA_STAT_UINT32_MAX},
        {.id = 7, .type = USERDATA_STAT_DOUBLE_MAX},
    };
    int id;

    /* Check invalid calls to register_stats(). */
    CHECK_FALSE(userdata_register_stats(NULL, lenof(stats)));
    CHECK_FALSE(userdata_register_stats(stats, 0));

    /* Check invalid statistic arrays. */
    CHECK_FALSE(userdata_register_stats(stats_invalid_type,
                                        lenof(stats_invalid_type)));
    CHECK_FALSE(userdata_register_stats(stats_duplicate_id,
                                        lenof(stats_duplicate_id)));

    /* Check calls to get/set/update/clear when stats aren't registered. */
    CHECK_FALSE(userdata_set_stat(0, 1));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 0.0);
    CHECK_FALSE(userdata_update_stats());
    CHECK_FALSE(userdata_clear_stats());

    /* Check that register succeeds even after an earlier failed register. */
    CHECK_TRUE(id = userdata_register_stats(stats, lenof(stats)));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_TRUE(userdata_set_stat(0, 1));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 1);

    return 1;
}

/*************************************************************************/
/************************* Miscellaneous tests ***************************/
/*************************************************************************/

TEST(test_set_program_name_memory_failure)
{
    /* No return value, so just make this doesn't crash. */
    for (int i = 0; i < 10; i++) {
        TEST_mem_fail_after(i, 1, 0);
        userdata_set_program_name("test");
        TEST_mem_fail_after(-1, 0, 0);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_program_name_invalid)
{
    /* No return value, so just make sure this doesn't crash. */
    userdata_set_program_name(NULL);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_program_title_memory_failure)
{
    /* No return value, so just make this doesn't crash. */
    for (int i = 0; i < 10; i++) {
        TEST_mem_fail_after(i, 1, 0);
        userdata_set_program_title("Userdata Test");
        TEST_mem_fail_after(-1, 0, 0);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_program_title_invalid)
{
    /* No return value, so just make sure this doesn't crash. */
    userdata_set_program_title(NULL);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_data_path)
{
    if (sysdep_has_data_path) {
        CHECK_TRUE(userdata_get_data_path());
    } else {
        CHECK_FALSE(userdata_get_data_path());
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_status)
{
    int id;

    CHECK_TRUE(id = userdata_save_data("foo", "foo", 3));
    while (!userdata_get_status(id)) {
        thread_yield();
    }
    CHECK_TRUE(userdata_get_result(id));  // Should succeed without wait().

    CHECK_TRUE(id = userdata_save_data("bar", "bar", 3));
    userdata_wait(id);
    userdata_wait(id);  // Should not change anything.
    CHECK_TRUE(userdata_get_result(id));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_misc_invalid)
{
    int id;

    CHECK_TRUE(userdata_get_status(0));
    userdata_wait(0);  // Make sure it doesn't get stuck or crash.
    CHECK_FALSE(userdata_get_result(0));

    /* Assume INT_MAX will never be returned as a valid ID. */
    CHECK_TRUE(userdata_get_status(INT_MAX));
    userdata_wait(INT_MAX);  // Make sure it doesn't get stuck or crash.
    CHECK_FALSE(userdata_get_result(INT_MAX));

    CHECK_TRUE(id = userdata_save_data("foo", "foo", 3));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_TRUE(userdata_get_status(id));
    userdata_wait(id);  // Make sure it doesn't get stuck or crash.
    CHECK_FALSE(userdata_get_result(id));

    return 1;
}

/*************************************************************************/
/*************************************************************************/
