/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/posix/userdata.c: Tests for the POSIX implementation of
 * the user data access functions.
 */

#include "src/base.h"
#include "src/graphics.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/posix/fileutil.h"
#include "src/sysdep/posix/path_max.h"
#include "src/sysdep/posix/userdata.h"
#include "src/sysdep/test.h"
#include "src/test/base.h"
#include "src/test/graphics/internal.h"
#include "src/test/sysdep/posix/internal.h"
#include "src/test/userdata.h"
#include "src/texture.h"
#include "src/userdata.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

/*************************************************************************/
/*************************** Helper functions ****************************/
/*************************************************************************/

/**
 * posix_init:  Initialization for userdata tests specific to the POSIX
 * implementation.
 */
static int posix_init(void)
{
    static char tempdir[PATH_MAX];
    CHECK_TRUE(posix_create_temporary_dir("test-userdata",
                                          tempdir, sizeof(tempdir)-1));
    strcat(tempdir, "/");  // Guaranteed safe by sizeof(tempdir)-1 above.
    TEST_posix_userdata_path = tempdir;
    sys_test_userdata_use_live_routines = 1;
    return 1;
}

/*-----------------------------------------------------------------------*/

/**
 * posix_cleanup:  Cleanup for userdata tests specific to the POSIX
 * implementation.
 */
static int posix_cleanup(void)
{
    sys_test_userdata_use_live_routines = 0;
    CHECK_INTEQUAL(chmod(TEST_posix_userdata_path, S_IRWXU), 0);
    char save_path[PATH_MAX];
    ASSERT(strformat_check(save_path, sizeof(save_path), "%s/save",
                           TEST_posix_userdata_path));
    if (access(save_path, F_OK) == 0) {
        CHECK_INTEQUAL(chmod(save_path, S_IRWXU), 0);
    }
    if (!posix_rmdir_r(TEST_posix_userdata_path)) {
        FAIL("Failed to remove temporary directory %s",
             TEST_posix_userdata_path);
    }
    TEST_posix_userdata_path = NULL;
    return 1;
}

/*-----------------------------------------------------------------------*/

#ifdef SIL_UTILITY_INCLUDE_PNG

/**
 * get_screenshot:  System-specific function to get saved
 * screenshot data for the stub implementation of the user data access
 * functions.
 */
static void *get_screenshot(int index, int *width_ret, int *height_ret)
{
    char png_path[PATH_MAX+1];
    ASSERT(strformat_check(png_path, sizeof(png_path),
                           "%sscreenshots/screen%d.png",
                           TEST_posix_userdata_path, index));
    ssize_t png_size;
    void *png_data;
    if (!(png_data = posix_read_file(png_path, &png_size, 0))) {
        return NULL;
    }

    int image;
    CHECK_TRUE(image = texture_parse(png_data, png_size, 0, 0, 1));
    const void *image_pixels = texture_lock_readonly(image);
    if (!image_pixels) {
        texture_destroy(image);
        return NULL;
    }
    *width_ret = texture_width(image);
    *height_ret = texture_height(image);

    void *copy;
    ASSERT(copy = mem_alloc((*width_ret) * (*height_ret) * 4, 0, 0));
    memcpy(copy, image_pixels, (*width_ret) * (*height_ret) * 4);
    texture_destroy(image);

    return copy;
}

#endif  // SIL_UTILITY_INCLUDE_PNG

/*-----------------------------------------------------------------------*/

/**
 * make_data_unwritable:  System-specific function to force
 * failure of write operations for the stub implementation of the user data
 * access functions.
 */
static void make_data_unwritable(void)
{
    ASSERT(chmod(TEST_posix_userdata_path, S_IRUSR|S_IXUSR) == 0);
    char save_path[PATH_MAX];
    ASSERT(strformat_check(save_path, sizeof(save_path), "%s/save",
                           TEST_posix_userdata_path));
    if (access(save_path, F_OK) == 0) {
        ASSERT(chmod(save_path, S_IRUSR|S_IXUSR) == 0);
    }
}

/*************************************************************************/
/***************** Test runner and init/cleanup routines *****************/
/*************************************************************************/

static int do_test_posix_userdata(void);
int test_posix_userdata(void)
{
#if defined(SIL_PLATFORM_IOS) && defined(SIL_PLATFORM_IOS_USE_GAMEKIT)
    SKIP("Skipping to avoid interacting with the Game Center server.");
#endif

    return run_tests_in_window(do_test_posix_userdata);
}

DEFINE_GENERIC_TEST_RUNNER(do_test_posix_userdata)

TEST_INIT(init)
{
    CHECK_TRUE(posix_init());
    CHECK_TRUE(userdata_init());
    userdata_set_program_name("test");
    return 1;
}

TEST_CLEANUP(cleanup)
{
    if (!sys_test_userdata_use_live_routines) {
        return 1;  // Outer call for nested tests.
    }

    graphics_flush_resources();
    userdata_cleanup();
    CHECK_TRUE(posix_cleanup());
    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_common)
{
    /* If we try to make nested calls to init() and cleanup(), path buffers
     * will be overwritten and things will get generally confused, so we
     * call cleanup() for this test now.  The test at the top of cleanup()
     * will ensure that it doesn't get run when this test returns. */
    ASSERT(cleanup());

    /* Re-run the common userdata tests using the POSIX implementation. */
    DLOG("Running common userdata tests for POSIX...");
    if (!run_userdata_tests(
            1, posix_init, posix_cleanup,
#ifdef SIL_UTILITY_INCLUDE_PNG
            get_screenshot,
#else
            NULL,
#endif
            make_data_unwritable))
    {
        FAIL("Preceding failure(s) occurred while testing POSIX userdata"
             " functions");
    }

    DLOG("Common userdata tests for POSIX succeeded.");
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_posix_override_path_overflow)
{
    char long_path[PATH_MAX+1];
    memset(long_path, 'a', sizeof(long_path)-1);
    long_path[sizeof(long_path)-1] = '\0';

    int id;

    CHECK_TRUE(userdata_override_file_path(long_path));
    CHECK_TRUE(id = userdata_save_data("test", "foo", 3));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));

    /* Also test with the live userdata directory (different code path).
     * Make sure to restore it before returning, even on failure! */
    {
        const char *saved_test_path = TEST_posix_userdata_path;
        TEST_posix_userdata_path = NULL;
        const int override_result = userdata_override_file_path(long_path);
        const int save_result =
            override_result && (id = userdata_save_data("test", "foo", 3));
        userdata_wait(id);
        const int operation_result = save_result && userdata_get_result(id);
        TEST_posix_userdata_path = saved_test_path;
        CHECK_TRUE(override_result);
        CHECK_TRUE(save_result);
        CHECK_FALSE(operation_result);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_posix_generate_path_overflow)
{
    char long_path[PATH_MAX+1];
    int id;

    /* Overflow on the base path. */
    memset(long_path, 'a', sizeof(long_path)-1);
    long_path[sizeof(long_path)-1] = '\0';
    {
        const char *saved_test_path = TEST_posix_userdata_path;
        TEST_posix_userdata_path = long_path;
        const int save_result = (id = userdata_save_data("test", "foo", 3));
        userdata_wait(id);
        const int operation_result = save_result && userdata_get_result(id);
        TEST_posix_userdata_path = saved_test_path;
        CHECK_TRUE(save_result);
        CHECK_FALSE(operation_result);
    }

    /* Overflow on the final path. */
    memset(long_path, 'a', PATH_MAX-2);
    long_path[PATH_MAX-2] = '\0';
    {
        const char *saved_test_path = TEST_posix_userdata_path;
        TEST_posix_userdata_path = long_path;
        const int save_result = (id = userdata_save_data("test", "foo", 3));
        userdata_wait(id);
        const int operation_result = save_result && userdata_get_result(id);
        TEST_posix_userdata_path = saved_test_path;
        CHECK_TRUE(save_result);
        CHECK_FALSE(operation_result);
    }

    /* Overflow using the live userdata directory pathname. */
    char *userdata_path;
    CHECK_TRUE(userdata_path = sys_userdata_get_data_path("test"));
    const unsigned int userdata_pathlen = strlen(userdata_path);
    mem_free(userdata_path);
    if (userdata_pathlen < PATH_MAX) {
        memset(long_path, 'a', PATH_MAX - userdata_pathlen);
        long_path[PATH_MAX - userdata_pathlen] = '\0';
        const char *saved_test_path = TEST_posix_userdata_path;
        TEST_posix_userdata_path = NULL;
        /* This can never succeed anyway because the file doesn't exist
         * (and the filesystem may also choke on the huge filename), but
         * we want to avoid even the chance of accidentally overwriting
         * the user's data.  This test just serves to make sure the
         * code doesn't overflow any buffers while working on the path. */
        void *data;
        uint32_t size;
        const int save_result =
            (id = userdata_load_data(long_path, &data, &size));
        userdata_wait(id);
        const int operation_result = save_result && userdata_get_result(id);
        TEST_posix_userdata_path = saved_test_path;
        CHECK_TRUE(save_result);
        CHECK_FALSE(operation_result);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_posix_overwrite_unwritable_savefile)
{
    int id;
    void *data;
    uint32_t size;

    CHECK_TRUE(id = userdata_save_savefile(0, "111", 3, "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    char path[PATH_MAX];
    ASSERT(strformat_check(path, sizeof(path), "%ssave/save-0000.bin",
                           TEST_posix_userdata_path));
    CHECK_TRUE(chmod(path, 0444) == 0);

    CHECK_TRUE(id = userdata_save_savefile(0, "2222", 4, "title", "desc", 0));
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

TEST(test_posix_savefile_scan_directory)
{
    int id;

    CHECK_TRUE(id = userdata_save_savefile(0, "111", 3, "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    char path[PATH_MAX];
    ASSERT(strformat_check(path, sizeof(path), "%ssave/save-0001.bin",
                           TEST_posix_userdata_path));
    CHECK_TRUE(mkdir(path, 0777) == 0);

    CHECK_TRUE(id = userdata_save_savefile(2, "333", 3, "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    uint8_t *data = NULL;
    CHECK_TRUE(id = userdata_scan_savefiles(0, 4, &data));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_TRUE(data);
    CHECK_TRUE(data[0]);
    CHECK_FALSE(data[1]);
    CHECK_TRUE(data[2]);
    CHECK_FALSE(data[3]);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

#ifdef SIL_UTILITY_INCLUDE_PNG

TEST(test_posix_save_screenshot_path)
{
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
    ssize_t size;
    char path[PATH_MAX];

    /* No filename extension. */
    CHECK_TRUE(userdata_override_file_path("test1"));
    CHECK_TRUE(id = userdata_save_savefile(0, "111", 3, "title", "desc",
                                           texture));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    ASSERT(strformat_check(path, sizeof(path), "%stest1",
                           TEST_posix_userdata_path));
    CHECK_TRUE(data = posix_read_file(path, &size, 0));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    mem_free(data);
    ASSERT(strformat_check(path, sizeof(path), "%stest1.png",
                           TEST_posix_userdata_path));
    CHECK_TRUE(data = posix_read_file(path, &size, 0));
    mem_free(data);

    /* Filename extension the same length as "png". */
    CHECK_TRUE(userdata_override_file_path("test2.abc"));
    CHECK_TRUE(id = userdata_save_savefile(0, "222", 3, "title", "desc",
                                           texture));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    ASSERT(strformat_check(path, sizeof(path), "%stest2.abc",
                           TEST_posix_userdata_path));
    CHECK_TRUE(data = posix_read_file(path, &size, 0));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "222", 3);
    mem_free(data);
    ASSERT(strformat_check(path, sizeof(path), "%stest2.png",
                           TEST_posix_userdata_path));
    CHECK_TRUE(data = posix_read_file(path, &size, 0));
    mem_free(data);

    /* Filename extension shorter than "png". */
    CHECK_TRUE(userdata_override_file_path("test3.x"));
    CHECK_TRUE(id = userdata_save_savefile(0, "333", 3, "title", "desc",
                                           texture));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    ASSERT(strformat_check(path, sizeof(path), "%stest3.x",
                           TEST_posix_userdata_path));
    CHECK_TRUE(data = posix_read_file(path, &size, 0));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "333", 3);
    mem_free(data);
    ASSERT(strformat_check(path, sizeof(path), "%stest3.png",
                           TEST_posix_userdata_path));
    CHECK_TRUE(data = posix_read_file(path, &size, 0));
    mem_free(data);

    /* Filename extension longer than "png". */
    CHECK_TRUE(userdata_override_file_path("test4.lmnop"));
    CHECK_TRUE(id = userdata_save_savefile(0, "444", 3, "title", "desc",
                                           texture));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    ASSERT(strformat_check(path, sizeof(path), "%stest4.lmnop",
                           TEST_posix_userdata_path));
    CHECK_TRUE(data = posix_read_file(path, &size, 0));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "444", 3);
    mem_free(data);
    ASSERT(strformat_check(path, sizeof(path), "%stest4.png",
                           TEST_posix_userdata_path));
    CHECK_TRUE(data = posix_read_file(path, &size, 0));
    mem_free(data);

    /* No filename extension, but period in previous path element. */
    CHECK_TRUE(userdata_override_file_path("test5.dir/file"));
    CHECK_TRUE(id = userdata_save_savefile(0, "555", 3, "title", "desc",
                                           texture));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    ASSERT(strformat_check(path, sizeof(path), "%stest5.dir/file",
                           TEST_posix_userdata_path));
    CHECK_TRUE(data = posix_read_file(path, &size, 0));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "555", 3);
    mem_free(data);
    ASSERT(strformat_check(path, sizeof(path), "%stest5.dir/file.png",
                           TEST_posix_userdata_path));
    CHECK_TRUE(data = posix_read_file(path, &size, 0));
    mem_free(data);

    /* Period in relative pathname with no directory components.  For this
     * test, we need to temporarily clear the global path override so it
     * doesn't get prepended to the filename, and we need to chdir() into
     * the temporary directory so we don't splatter test files all over the
     * filesystem. */
    CHECK_TRUE(userdata_override_file_path("test6.bin"));
    {
        const char *saved_tempdir = TEST_posix_userdata_path;
        TEST_posix_userdata_path = NULL;
        int cwd_fd;
        ASSERT((cwd_fd = open(".", O_RDONLY)) != -1);
        ASSERT(chdir(saved_tempdir) == 0);

        id = userdata_save_savefile(0, "555", 3, "title", "desc", texture);
        userdata_wait(id);

        ASSERT(fchdir(cwd_fd) == 0);
        close(cwd_fd);
        TEST_posix_userdata_path = saved_tempdir;
    }
    CHECK_TRUE(id);
    CHECK_TRUE(userdata_get_result(id));
    ASSERT(strformat_check(path, sizeof(path), "%stest6.bin",
                           TEST_posix_userdata_path));
    CHECK_TRUE(data = posix_read_file(path, &size, 0));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "555", 3);
    mem_free(data);
    ASSERT(strformat_check(path, sizeof(path), "%stest6.png",
                           TEST_posix_userdata_path));
    CHECK_TRUE(data = posix_read_file(path, &size, 0));
    mem_free(data);

    texture_destroy(texture);

    return 1;
}

#endif  // SIL_UTILITY_INCLUDE_PNG

/*-----------------------------------------------------------------------*/

#ifdef SIL_UTILITY_INCLUDE_PNG

TEST(test_posix_save_screenshot_path_overflow)
{
    /* Generate a save filename such that the final path itself (and the
     * temporary file's pathname, which is one byte longer) fits within a
     * PATH_MAX sized buffer, but rewriting the filename extension to .png
     * will overflow the buffer.  Also break the path down into components
     * at 240-byte intervals so we don't hit any filename length limits. */
    char override_path[PATH_MAX];
    char savefile_path[PATH_MAX-1], png_path[PATH_MAX+1];
    const int tempdir_pathlen = strlen(TEST_posix_userdata_path);
    ASSERT(tempdir_pathlen < PATH_MAX-3);
    const int override_pathlen = (PATH_MAX-2) - tempdir_pathlen;
    memset(override_path, 'a', override_pathlen);
    override_path[override_pathlen - 2] = '.';
    override_path[override_pathlen] = '\0';
    for (int i = 240; i < override_pathlen - 3; i += 240) {
        override_path[i] = '/';
    }
    ASSERT(strformat_check(savefile_path, sizeof(savefile_path), "%s%s",
                           TEST_posix_userdata_path, override_path));
    ASSERT(strformat_check(png_path, sizeof(png_path), "%.*s.png",
                           strlen(savefile_path) - 2, savefile_path));

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
    ssize_t size;
    uint32_t size32;

    CHECK_TRUE(userdata_override_file_path(override_path));
    CHECK_TRUE(id = userdata_save_savefile(0, "111", 3, "title", "desc",
                                           texture));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    texture_destroy(texture);

    CHECK_TRUE(userdata_override_file_path(override_path));
    CHECK_TRUE(id = userdata_load_savefile(0, &data, &size32, &texture));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size32, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    CHECK_FALSE(texture);
    mem_free(data);

    CHECK_TRUE(data = posix_read_file(savefile_path, &size, 0));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    mem_free(data);

    CHECK_FALSE(posix_read_file(png_path, &size, 0));

    return 1;
}

#endif  // SIL_UTILITY_INCLUDE_PNG

/*-----------------------------------------------------------------------*/

#ifdef SIL_UTILITY_INCLUDE_PNG

TEST(test_posix_save_screenshot_corrupt)
{
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

    char path[PATH_MAX];
    int id;
    void *data;
    uint32_t size;
    int image;

    CHECK_TRUE(id = userdata_save_savefile(0, "111", 3, "title", "desc",
                                           texture));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    texture_destroy(texture);

    ASSERT(strformat_check(path, lenof(path), "%ssave/save-0000.png",
                           TEST_posix_userdata_path));
    ASSERT(posix_write_file(path, "foo", 3, 0));

    CHECK_TRUE(id = userdata_load_savefile(0, &data, &size, &image));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    mem_free(data);
    CHECK_FALSE(image);

    return 1;
}

#endif  // SIL_UTILITY_INCLUDE_PNG

/*-----------------------------------------------------------------------*/

#ifdef SIL_UTILITY_INCLUDE_PNG

TEST(test_posix_save_screenshot_overwrite_unwritable)
{
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

    char path[PATH_MAX];
    ASSERT(strformat_check(path, lenof(path), "%ssave/save-0000.png",
                           TEST_posix_userdata_path));
    ASSERT(posix_write_file(path, "foo", 3, 0));
    ASSERT(chmod(path, 0444) == 0);

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
    mem_free(data);
    CHECK_FALSE(image);

    return 1;
}

#endif  // SIL_UTILITY_INCLUDE_PNG

/*-----------------------------------------------------------------------*/

#ifdef SIL_UTILITY_INCLUDE_PNG

TEST(test_posix_save_screenshot_remove_failure)
{
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

    char path[PATH_MAX];
    int id;
    void *data;
    uint32_t size;
    int image;

    CHECK_TRUE(id = userdata_save_savefile(0, "111", 3, "title", "desc",
                                           texture));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    texture_destroy(texture);

    ASSERT(strformat_check(path, lenof(path), "%ssave/save-0000.png",
                           TEST_posix_userdata_path));
    ASSERT(posix_write_file(path, "foo", 3, 0));

    CHECK_TRUE(id = userdata_load_savefile(0, &data, &size, &image));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    mem_free(data);
    CHECK_FALSE(image);

    return 1;
}

#endif  // SIL_UTILITY_INCLUDE_PNG

/*-----------------------------------------------------------------------*/

TEST(test_posix_scan_savefiles_other_files)
{
    char path[PATH_MAX];
    int id;

    CHECK_TRUE(id = userdata_save_savefile(1, "111", 3, "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    ASSERT(strformat_check(path, lenof(path), "%ssave/save-0002.png",
                           TEST_posix_userdata_path));
    ASSERT(posix_write_file(path, "foo", 3, 0));
    ASSERT(strformat_check(path, lenof(path), "%ssave/foo",
                           TEST_posix_userdata_path));
    ASSERT(posix_write_file(path, "foo", 3, 0));

    uint8_t *data = NULL;
    CHECK_TRUE(id = userdata_scan_savefiles(0, 4, &data));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_TRUE(data);
    CHECK_FALSE(data[0]);
    CHECK_TRUE(data[1]);
    CHECK_FALSE(data[2]);
    CHECK_FALSE(data[3]);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_posix_overwrite_unwritable_data)
{
    int id;
    void *data;
    uint32_t size;

    CHECK_TRUE(id = userdata_save_data("foo", "111", 3));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    char path[PATH_MAX];
    ASSERT(strformat_check(path, sizeof(path), "%sfoo",
                           TEST_posix_userdata_path));
    CHECK_TRUE(chmod(path, 0444) == 0);

    CHECK_TRUE(id = userdata_save_data("foo", "2222", 4));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));

    CHECK_TRUE(id = userdata_load_data("foo", &data, &size));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_posix_stats_truncated_file_for_flag)
{
    static const struct UserStatInfo stats[] = {
        {.id = 0, .type = USERDATA_STAT_UINT32},
        {.id = 1, .type = USERDATA_STAT_FLAG},
    };
    int id;

    char buf[1000];
    ASSERT(strformat_check(buf, sizeof(buf), "%sstats.bin",
                           TEST_posix_userdata_path));
    ASSERT(posix_write_file(buf, "\0\0\0\x2A", 4, 0));

    CHECK_TRUE(id = userdata_register_stats(stats, lenof(stats)));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 42);
    CHECK_DOUBLEEQUAL(userdata_get_stat(1), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_posix_stats_corrupt_data_for_flag)
{
    static const struct UserStatInfo stats[] = {
        {.id = 0, .type = USERDATA_STAT_FLAG},
        {.id = 1, .type = USERDATA_STAT_FLAG},
        {.id = 2, .type = USERDATA_STAT_FLAG},
    };
    struct UserStatValue values_out[] = {
        {.id = 0},
        {.id = 1},
        {.id = 2},
    };
    int id;

    char buf[1000];
    ASSERT(strformat_check(buf, sizeof(buf), "%sstats.bin",
                           TEST_posix_userdata_path));
    ASSERT(posix_write_file(buf, "\0\1\x2A", 4, 0));

    CHECK_TRUE(id = userdata_register_stats(stats, lenof(stats)));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(1), 1);
    CHECK_DOUBLEEQUAL(values_out[2].value, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_posix_stats_truncated_file_for_uint32)
{
    static const struct UserStatInfo stats[] = {
        {.id = 0, .type = USERDATA_STAT_UINT32},
        {.id = 1, .type = USERDATA_STAT_UINT32},
    };
    int id;

    char buf[1000];
    ASSERT(strformat_check(buf, sizeof(buf), "%sstats.bin",
                           TEST_posix_userdata_path));
    ASSERT(posix_write_file(buf, "\0\0\0\x2A", 4, 0));

    CHECK_TRUE(id = userdata_register_stats(stats, lenof(stats)));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 42);
    CHECK_DOUBLEEQUAL(userdata_get_stat(1), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_posix_stats_truncated_file_for_double)
{
    static const struct UserStatInfo stats[] = {
        {.id = 0, .type = USERDATA_STAT_UINT32},
        {.id = 1, .type = USERDATA_STAT_DOUBLE},
    };
    int id;

    char buf[1000];
    ASSERT(strformat_check(buf, sizeof(buf), "%sstats.bin",
                           TEST_posix_userdata_path));
    ASSERT(posix_write_file(buf, "\0\0\0\x2A", 4, 0));

    CHECK_TRUE(id = userdata_register_stats(stats, lenof(stats)));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 42);
    CHECK_DOUBLEEQUAL(userdata_get_stat(1), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_posix_stats_save_error)
{
    static const struct UserStatInfo stats[] = {
        {.id = 0, .type = USERDATA_STAT_UINT32},
        {.id = 1, .type = USERDATA_STAT_DOUBLE},
    };
    char path[PATH_MAX];
    int id;

    CHECK_TRUE(id = userdata_register_stats(stats, lenof(stats)));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    ASSERT(strformat_check(path, lenof(path), "%sfoo",
                           TEST_posix_userdata_path));
    ASSERT(posix_write_file(path, "foo", 3, 0));
    CHECK_TRUE(userdata_set_stat(0, 1));
    CHECK_TRUE(userdata_set_stat(1, 2));
    CHECK_TRUE(userdata_override_file_path("foo/bar"));
    CHECK_TRUE(id = userdata_update_stats());
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_posix_stats_clear_error)
{
    static const struct UserStatInfo stats[] = {
        {.id = 0, .type = USERDATA_STAT_UINT32},
        {.id = 1, .type = USERDATA_STAT_DOUBLE},
    };
    char path[PATH_MAX];
    int id;

    CHECK_TRUE(id = userdata_register_stats(stats, lenof(stats)));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_TRUE(userdata_set_stat(0, 1));
    CHECK_TRUE(userdata_set_stat(1, 2));
    CHECK_TRUE(userdata_override_file_path("foo/bar"));
    CHECK_TRUE(id = userdata_update_stats());
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    ASSERT(strformat_check(path, lenof(path), "%sfoo",
                           TEST_posix_userdata_path));
    ASSERT(chmod(path, S_IRUSR | S_IXUSR) == 0);
    CHECK_TRUE(userdata_override_file_path("foo/bar"));
    CHECK_TRUE(id = userdata_clear_stats());
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));
    ASSERT(strformat_check(path, lenof(path), "%sfoo",
                           TEST_posix_userdata_path));
    ASSERT(access(path, R_OK) == 0);

    ASSERT(strformat_check(path, lenof(path), "%sfoo",
                           TEST_posix_userdata_path));
    CHECK_INTEQUAL(chmod(path, S_IRWXU), 0);
    return 1;
}

/*************************************************************************/
/*************************************************************************/
