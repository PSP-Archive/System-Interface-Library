/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/windows/userdata.c: Tests for the Windows implementation
 * of the user data access functions.
 */

#include "src/base.h"
#include "src/graphics.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/test.h"
#include "src/sysdep/windows/internal.h"
#include "src/sysdep/windows/userdata.h"
#include "src/test/base.h"
#include "src/test/graphics/internal.h"
#include "src/test/userdata.h"
#include "src/texture.h"
#include "src/userdata.h"
#include "src/utility/utf8.h"

/*************************************************************************/
/*************************** Helper functions ****************************/
/*************************************************************************/

/**
 * read_file:  Read in the data from the file at the given path.  The
 * returned buffer should be freed with mem_free() when no longer needed.
 *
 * [Parameters]
 *     path: Pathname of file to read.
 *     len_ret: Pointer to variable to receive the length of the file data,
 *         in bytes.
 * [Return value]
 *     Newly allocated buffer containing the read-in data, or NULL on error.
 */
static void *read_file(const char *path, uint32_t *len_ret)
{
    const HANDLE fh = CreateFile(path, FILE_READ_DATA, 0, NULL,
                                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (fh == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    DWORD size_high = 0;
    uint32_t size = GetFileSize(fh, &size_high);
    CHECK_TRUE(size != INVALID_FILE_SIZE);
    CHECK_INTEQUAL(size_high, 0);

    void *buffer;
    CHECK_TRUE(buffer = mem_alloc(size > 0 ? size : 1, 0, 0));

    DWORD bytes_read;
    CHECK_TRUE(ReadFile(fh, buffer, size, &bytes_read, NULL));
    CHECK_INTEQUAL(bytes_read, size);

    CloseHandle(fh);
    *len_ret = size;
    return buffer;
}

/*-----------------------------------------------------------------------*/

/**
 * write_file:  Create a file at the given path with the given data.  The
 * containing directory must already exist.
 *
 * [Parameters]
 *     path: Pathname of file to create.
 *     data: Data to write to file.
 *     len: Length of data, in bytes.
 * [Return value]
 *     True on success, false on error.
 */
static int write_file(const char *path, const void *data, int len)
{
    const HANDLE fh = CreateFile(path, FILE_WRITE_DATA, 0, NULL,
                                 CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    CHECK_TRUE(fh != INVALID_HANDLE_VALUE);
    DWORD bytes_written;
    CHECK_TRUE(WriteFile(fh, data, len, &bytes_written, NULL));
    CHECK_INTEQUAL(bytes_written, len);
    CloseHandle(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

/**
 * remove_dir:  Remove a directory and all files contained therein.
 * Subdirectories are not removed, and the function will fail if any
 * subdirectories are present.
 *
 * [Parameters]
 *     path: Pathname of directory to remove.
 * [Return value]
 *     True on success or if the directory does not exist, false on error.
 */
static int remove_dir(const char *path)
{
    PRECOND(path != NULL);
    SysDir *dir = sys_dir_open(path);
    if (dir) {
        const char *name;
        int is_subdir;
        while ((name = sys_dir_read(dir, &is_subdir)) != NULL) {
            /* While MSDN is silent on the subject, it seems to be
             * generally regarded as safe to delete files in a
             * FindNextFile() loop; it is also safe in Wine after
             * http://bugs.winehq.org/show_bug.cgi?id=31987 was fixed. */
            char file_path[MAX_PATH*3];
            ASSERT(strformat_check(
                       file_path, sizeof(file_path), "%s%s%s", path,
                       path[strlen(path)-1]=='\\' ? "" : "\\", name));
            SetFileAttributes(
                file_path, GetFileAttributes(path) & ~FILE_ATTRIBUTE_READONLY);
            if (is_subdir) {
                if (UNLIKELY(!RemoveDirectory(file_path))) {
                    DLOG("Failed to delete directory %s: %s", file_path,
                         windows_strerror(GetLastError()));
                    return 0;
                }
            } else {
                if (UNLIKELY(!DeleteFile(file_path))) {
                    DLOG("Failed to delete %s: %s", file_path,
                         windows_strerror(GetLastError()));
                    return 0;
                }
            }
        }
        sys_dir_close(dir);
        if (UNLIKELY(!RemoveDirectory(path))) {
            DLOG("Failed to delete %s: %s", path,
                 windows_strerror(GetLastError()));
            return 0;
        }
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

/**
 * windows_init:  Initialization for userdata tests specific to the Windows
 * implementation.
 */
static int windows_init(void)
{
    static char tempdir[4 + (MAX_PATH+1)*3];
    /* Use a raw path so we can have pathnames longer than MAX_PATH
     * characters -- this is needed so we can create paths with just under
     * MAX_PATH*3 bytes (i.e., more than MAX_PATH characters) in the buffer
     * overflow tests. */
    memcpy(tempdir, "\\\\?\\", 4);
    unsigned int templen = GetTempPath(sizeof(tempdir)-4, tempdir+4);
    if (UNLIKELY(templen == 0)) {
        DLOG("GetTempPath() failed: %s", windows_strerror(GetLastError()));
        return 0;
    }
    ASSERT(templen <= (MAX_PATH+1)*3);  // Guaranteed by API (modulo UTF-8).
    templen += 4;  // For raw path prefix.
    /* Make sure the final path for all files will fit in MAX_PATH*3
     * bytes: <tempdir>\SIL-1234567890\screenshots\screen0.png */
    if (templen > (MAX_PATH*3)-40) {
        DLOG("Temporary directory pathname %s is too long", tempdir+4);
        return 0;
    }
    ASSERT(strformat_check(tempdir+templen, sizeof(tempdir)-templen,
                           "SIL-%u\\", (int)GetCurrentProcessId()));
    TEST_windows_userdata_path = tempdir;
    sys_test_userdata_use_live_routines = 1;
    return 1;
}

/*-----------------------------------------------------------------------*/

/**
 * windows_cleanup:  Cleanup for userdata tests specific to the Windows
 * implementation.
 */
static int windows_cleanup(void)
{
    sys_test_userdata_use_live_routines = 0;

    char save_path[MAX_PATH*3];
    ASSERT(strformat_check(save_path, sizeof(save_path), "%ssave",
                           TEST_windows_userdata_path));
    char screenshot_path[MAX_PATH*3];
    ASSERT(strformat_check(screenshot_path, sizeof(screenshot_path),
                           "%sscreenshots", TEST_windows_userdata_path));

    CHECK_TRUE(sys_file_init());
    CHECK_TRUE(remove_dir(save_path));
    CHECK_TRUE(remove_dir(screenshot_path));
    CHECK_TRUE(remove_dir(TEST_windows_userdata_path));
    sys_file_cleanup();

    TEST_windows_userdata_path = NULL;
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
    char png_path[MAX_PATH*3];
    ASSERT(strformat_check(png_path, sizeof(png_path),
                           "%sscreenshots\\screen%d.png",
                           TEST_windows_userdata_path, index));
    SysFile *fh = sys_file_open(png_path);
    if (!fh) {
        return NULL;
    }
    const int png_size = sys_file_size(fh);
    void *png_data;
    ASSERT(png_data = mem_alloc(png_size, 0, MEM_ALLOC_TEMP));
    if (sys_file_read(fh, png_data, png_size) != png_size) {
        DLOG("Error reading from %s", png_path);
        mem_free(png_data);
        sys_file_close(fh);
        return NULL;
    }
    sys_file_close(fh);

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

/*************************************************************************/
/***************** Test runner and init/cleanup routines *****************/
/*************************************************************************/

static int do_test_windows_userdata(void);
int test_windows_userdata(void)
{
    return run_tests_in_window(do_test_windows_userdata);
}

DEFINE_GENERIC_TEST_RUNNER(do_test_windows_userdata)

TEST_INIT(init)
{
    CHECK_TRUE(windows_init());
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
    CHECK_TRUE(windows_cleanup());
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

    /* Re-run the common userdata tests using the Windows implementation. */
    DLOG("Running common userdata tests for Windows...");
    if (!run_userdata_tests(
            1, windows_init, windows_cleanup,
#ifdef SIL_UTILITY_INCLUDE_PNG
            get_screenshot,
#else
            NULL,
#endif
            NULL))
    {
        FAIL("Preceding failure(s) occurred while testing Windows userdata"
             " functions");
    }

    DLOG("Common userdata tests for Windows succeeded.");
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_windows_create_directory_nonraw_absolute_path)
{
    char path[MAX_PATH];
    int id;

    ASSERT(memcmp(TEST_windows_userdata_path, "\\\\?\\", 4) == 0);
    ASSERT(strformat_check(path, sizeof(path), "%stest",
                           TEST_windows_userdata_path + 4));
    {
        const char *saved_test_path = TEST_windows_userdata_path;
        TEST_windows_userdata_path = NULL;
        const int override_result = userdata_override_file_path(path);
        const int save_result =
            override_result && (id = userdata_save_data("test", "foo", 3));
        userdata_wait(id);
        const int operation_result = save_result && userdata_get_result(id);
        TEST_windows_userdata_path = saved_test_path;
        CHECK_TRUE(override_result);
        CHECK_TRUE(save_result);
        CHECK_TRUE(operation_result);
    }
    ASSERT(strformat_check(path, sizeof(path), "%stest",
                           TEST_windows_userdata_path));
    uint32_t size;
    void *data;
    CHECK_TRUE(data = read_file(path, &size));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "foo", 3);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_windows_override_path_overflow)
{
    char long_path[MAX_PATH*3+1];
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
        const char *saved_test_path = TEST_windows_userdata_path;
        TEST_windows_userdata_path = NULL;
        const int override_result = userdata_override_file_path(long_path);
        const int save_result =
            override_result && (id = userdata_save_data("test", "foo", 3));
        userdata_wait(id);
        const int operation_result = save_result && userdata_get_result(id);
        TEST_windows_userdata_path = saved_test_path;
        CHECK_TRUE(override_result);
        CHECK_TRUE(save_result);
        CHECK_FALSE(operation_result);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_windows_generate_path_overflow)
{
    char long_path[MAX_PATH*3+1];
    int id;

    /* Overflow on the base path. */
    memset(long_path, 'a', sizeof(long_path)-1);
    long_path[sizeof(long_path)-1] = '\0';
    {
        const char *saved_test_path = TEST_windows_userdata_path;
        TEST_windows_userdata_path = long_path;
        const int save_result = (id = userdata_save_data("test", "foo", 3));
        userdata_wait(id);
        const int operation_result = save_result && userdata_get_result(id);
        TEST_windows_userdata_path = saved_test_path;
        CHECK_TRUE(save_result);
        CHECK_FALSE(operation_result);
    }

    /* Overflow on the final path. */
    memset(long_path, 'a', MAX_PATH*3-2);
    long_path[MAX_PATH*3-2] = '\0';
    {
        const char *saved_test_path = TEST_windows_userdata_path;
        TEST_windows_userdata_path = long_path;
        const int save_result = (id = userdata_save_data("test", "foo", 3));
        userdata_wait(id);
        const int operation_result = save_result && userdata_get_result(id);
        TEST_windows_userdata_path = saved_test_path;
        CHECK_TRUE(save_result);
        CHECK_FALSE(operation_result);
    }

    /* Overflow using the live userdata directory pathname. */
    char *userdata_path;
    CHECK_TRUE(userdata_path = sys_userdata_get_data_path("test"));
    const unsigned int userdata_pathlen = strlen(userdata_path);
    mem_free(userdata_path);
    if (userdata_pathlen < MAX_PATH*3) {
        memset(long_path, 'a', MAX_PATH*3 - userdata_pathlen);
        long_path[MAX_PATH*3 - userdata_pathlen] = '\0';
        const char *saved_test_path = TEST_windows_userdata_path;
        TEST_windows_userdata_path = NULL;
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
        TEST_windows_userdata_path = saved_test_path;
        CHECK_TRUE(save_result);
        CHECK_FALSE(operation_result);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_windows_temporary_path_overflow)
{
    char path[MAX_PATH*3];
    int id;

    ASSERT(strlen(TEST_windows_userdata_path) < sizeof(path)-1);
    const int namelen =
        (sizeof(path)-1) - strlen(TEST_windows_userdata_path);
    memset(path, 'a', namelen);
    path[namelen] = '\0';
    CHECK_TRUE(id = userdata_save_data(path, "foo", 3));
    userdata_wait(id);
    CHECK_FALSE(userdata_get_result(id));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_windows_overwrite_unwritable_savefile)
{
    int id;
    void *data;
    uint32_t size;

    CHECK_TRUE(id = userdata_save_savefile(0, "111", 3, "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    char path[MAX_PATH];
    ASSERT(strformat_check(path, sizeof(path), "%ssave\\save-0000.bin",
                           TEST_windows_userdata_path));
    const DWORD attributes = GetFileAttributes(path);
    ASSERT(attributes != INVALID_FILE_ATTRIBUTES);
    ASSERT(!(attributes & FILE_ATTRIBUTE_READONLY));
    CHECK_TRUE(SetFileAttributes(path, attributes | FILE_ATTRIBUTE_READONLY));

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

TEST(test_windows_savefile_scan_directory)
{
    int id;

    CHECK_TRUE(id = userdata_save_savefile(0, "111", 3, "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    char path[PATH_MAX];
    ASSERT(strformat_check(path, sizeof(path), "%ssave\\save-0001.bin",
                           TEST_windows_userdata_path));
    CHECK_TRUE(CreateDirectory(path, NULL));

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

TEST(test_windows_save_screenshot_path)
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
    uint32_t size;
    char path[MAX_PATH*3];

    /* No filename extension. */
    CHECK_TRUE(userdata_override_file_path("test1"));
    CHECK_TRUE(id = userdata_save_savefile(0, "111", 3, "title", "desc",
                                           texture));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    ASSERT(strformat_check(path, sizeof(path), "%stest1",
                           TEST_windows_userdata_path));
    CHECK_TRUE(data = read_file(path, &size));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    mem_free(data);
    ASSERT(strformat_check(path, sizeof(path), "%stest1.png",
                           TEST_windows_userdata_path));
    CHECK_TRUE(data = read_file(path, &size));
    mem_free(data);

    /* Filename extension the same length as "png". */
    CHECK_TRUE(userdata_override_file_path("test2.abc"));
    CHECK_TRUE(id = userdata_save_savefile(0, "222", 3, "title", "desc",
                                           texture));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    ASSERT(strformat_check(path, sizeof(path), "%stest2.abc",
                           TEST_windows_userdata_path));
    CHECK_TRUE(data = read_file(path, &size));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "222", 3);
    mem_free(data);
    ASSERT(strformat_check(path, sizeof(path), "%stest2.png",
                           TEST_windows_userdata_path));
    CHECK_TRUE(data = read_file(path, &size));
    mem_free(data);

    /* Filename extension shorter than "png". */
    CHECK_TRUE(userdata_override_file_path("test3.x"));
    CHECK_TRUE(id = userdata_save_savefile(0, "333", 3, "title", "desc",
                                           texture));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    ASSERT(strformat_check(path, sizeof(path), "%stest3.x",
                           TEST_windows_userdata_path));
    CHECK_TRUE(data = read_file(path, &size));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "333", 3);
    mem_free(data);
    ASSERT(strformat_check(path, sizeof(path), "%stest3.png",
                           TEST_windows_userdata_path));
    CHECK_TRUE(data = read_file(path, &size));
    mem_free(data);

    /* Filename extension longer than "png". */
    CHECK_TRUE(userdata_override_file_path("test4.lmnop"));
    CHECK_TRUE(id = userdata_save_savefile(0, "444", 3, "title", "desc",
                                           texture));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    ASSERT(strformat_check(path, sizeof(path), "%stest4.lmnop",
                           TEST_windows_userdata_path));
    CHECK_TRUE(data = read_file(path, &size));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "444", 3);
    mem_free(data);
    ASSERT(strformat_check(path, sizeof(path), "%stest4.png",
                           TEST_windows_userdata_path));
    CHECK_TRUE(data = read_file(path, &size));
    mem_free(data);

    /* No filename extension, but period in previous path element. */
    CHECK_TRUE(userdata_override_file_path("test5.dir\\file"));
    CHECK_TRUE(id = userdata_save_savefile(0, "555", 3, "title", "desc",
                                           texture));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    ASSERT(strformat_check(path, sizeof(path), "%stest5.dir\\file",
                           TEST_windows_userdata_path));
    CHECK_TRUE(data = read_file(path, &size));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "555", 3);
    mem_free(data);
    ASSERT(strformat_check(path, sizeof(path), "%stest5.dir\\file.png",
                           TEST_windows_userdata_path));
    CHECK_TRUE(data = read_file(path, &size));
    mem_free(data);
    /* remove_dir() can't handle subdirectories, so we need to remove this
     * on our own. */
    ASSERT(strformat_check(path, sizeof(path), "%stest5.dir",
                           TEST_windows_userdata_path));
    remove_dir(path);

    /* Period in relative pathname with no directory components.  For this
     * test, we need to temporarily clear the global path override so it
     * doesn't get prepended to the filename, and we need to chdir() into
     * the temporary directory so we don't splatter test files all over the
     * filesystem. */
    CHECK_TRUE(userdata_override_file_path("test6.bin"));
    {
        const char *saved_tempdir = TEST_windows_userdata_path;
        TEST_windows_userdata_path = NULL;
        char saved_cwd[MAX_PATH*3];
        DWORD gcd_result = GetCurrentDirectory(sizeof(saved_cwd), saved_cwd);
        ASSERT(gcd_result > 0 && gcd_result < sizeof(saved_cwd));
        ASSERT(SetCurrentDirectory(saved_tempdir));

        id = userdata_save_savefile(0, "555", 3, "title", "desc", texture);
        userdata_wait(id);

        ASSERT(SetCurrentDirectory(saved_cwd));
        TEST_windows_userdata_path = saved_tempdir;
    }
    CHECK_TRUE(id);
    CHECK_TRUE(userdata_get_result(id));
    ASSERT(strformat_check(path, sizeof(path), "%stest6.bin",
                           TEST_windows_userdata_path));
    CHECK_TRUE(data = read_file(path, &size));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "555", 3);
    mem_free(data);
    ASSERT(strformat_check(path, sizeof(path), "%stest6.png",
                           TEST_windows_userdata_path));
    CHECK_TRUE(data = read_file(path, &size));
    mem_free(data);

    texture_destroy(texture);

    return 1;
}

#endif  // SIL_UTILITY_INCLUDE_PNG

/*-----------------------------------------------------------------------*/

#ifdef SIL_UTILITY_INCLUDE_PNG

TEST(test_windows_save_screenshot_path_overflow)
{
    /* Generate a save filename such that the final path itself (and the
     * temporary file's pathname, which is one byte longer) fits within a
     * MAX_PATH*3 sized buffer, but rewriting the filename extension to .png
     * will overflow the buffer.  Also break the path down into components
     * at 240-byte intervals so we don't hit any filename length limits. */
    char override_path[MAX_PATH*3];
    char savefile_path[MAX_PATH*3-1], png_path[MAX_PATH*3+1];
    const int tempdir_pathlen = strlen(TEST_windows_userdata_path);
    ASSERT(tempdir_pathlen < MAX_PATH*3-3);
    const int override_pathlen = (MAX_PATH*3-2) - tempdir_pathlen;
    memset(override_path, 'a', override_pathlen);
    override_path[override_pathlen - 2] = '.';
    override_path[override_pathlen] = '\0';
    for (int i = 240; i < override_pathlen - 3; i += 240) {
        override_path[i] = '\\';
    }
    ASSERT(strformat_check(savefile_path, sizeof(savefile_path), "%s%s",
                           TEST_windows_userdata_path, override_path));
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
    uint32_t size;

    CHECK_TRUE(userdata_override_file_path(override_path));
    CHECK_TRUE(id = userdata_save_savefile(0, "111", 3, "title", "desc",
                                           texture));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    texture_destroy(texture);

    CHECK_TRUE(userdata_override_file_path(override_path));
    CHECK_TRUE(id = userdata_load_savefile(0, &data, &size, &texture));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    CHECK_FALSE(texture);
    mem_free(data);

    CHECK_TRUE(data = read_file(savefile_path, &size));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "111", 3);
    mem_free(data);

    CHECK_FALSE(read_file(png_path, &size));

    /* Manually delete the subdirectories since remove_dir() will choke on
     * them. */
    CHECK_TRUE(DeleteFile(savefile_path));
    char *s;
    while ((s = strrchr(savefile_path, '\\'))
            > (savefile_path + strlen(TEST_windows_userdata_path))) {
        *s = '\0';
        CHECK_TRUE(RemoveDirectory(savefile_path));
    }

    return 1;
}

#endif  // SIL_UTILITY_INCLUDE_PNG

/*-----------------------------------------------------------------------*/

#ifdef SIL_UTILITY_INCLUDE_PNG

TEST(test_windows_save_screenshot_corrupt)
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

    char path[MAX_PATH*3];
    int id;
    void *data;
    uint32_t size;
    int image;

    CHECK_TRUE(id = userdata_save_savefile(0, "111", 3, "title", "desc",
                                           texture));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    texture_destroy(texture);

    ASSERT(strformat_check(path, lenof(path), "%ssave\\save-0000.png",
                           TEST_windows_userdata_path));
    ASSERT(write_file(path, "foo", 3));

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

TEST(test_windows_save_screenshot_overwrite_unwritable)
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

    char path[MAX_PATH*3];
    ASSERT(CreateDirectory(TEST_windows_userdata_path, NULL));
    ASSERT(strformat_check(path, lenof(path), "%ssave",
                           TEST_windows_userdata_path));
    ASSERT(CreateDirectory(path, NULL));
    ASSERT(strformat_check(path, lenof(path), "%ssave\\save-0000.png",
                           TEST_windows_userdata_path));
    ASSERT(write_file(path, "foo", 3));
    const DWORD attributes = GetFileAttributes(path);
    ASSERT(attributes != INVALID_FILE_ATTRIBUTES);
    ASSERT(!(attributes & FILE_ATTRIBUTE_READONLY));
    CHECK_TRUE(SetFileAttributes(path, attributes | FILE_ATTRIBUTE_READONLY));

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

TEST(test_windows_scan_savefiles_other_files)
{
    char path[MAX_PATH*3];
    int id;

    CHECK_TRUE(id = userdata_save_savefile(1, "111", 3, "title", "desc", 0));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    ASSERT(strformat_check(path, lenof(path), "%ssave\\save-0002.png",
                           TEST_windows_userdata_path));
    ASSERT(write_file(path, "foo", 3));
    ASSERT(strformat_check(path, lenof(path), "%ssave\\foo",
                           TEST_windows_userdata_path));
    ASSERT(write_file(path, "foo", 3));

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

TEST(test_windows_overwrite_unwritable_data)
{
    int id;
    void *data;
    uint32_t size;

    CHECK_TRUE(id = userdata_save_data("foo", "111", 3));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));

    char path[MAX_PATH];
    ASSERT(strformat_check(path, sizeof(path), "%sfoo",
                           TEST_windows_userdata_path));
    const DWORD attributes = GetFileAttributes(path);
    ASSERT(attributes != INVALID_FILE_ATTRIBUTES);
    ASSERT(!(attributes & FILE_ATTRIBUTE_READONLY));
    CHECK_TRUE(SetFileAttributes(path, attributes | FILE_ATTRIBUTE_READONLY));

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

TEST(test_windows_stats_truncated_file_for_flag)
{
    static const struct UserStatInfo stats[] = {
        {.id = 0, .type = USERDATA_STAT_UINT32},
        {.id = 1, .type = USERDATA_STAT_FLAG},
    };
    int id;

    char buf[1000];
    ASSERT(strformat_check(buf, sizeof(buf), "%sstats.bin",
                           TEST_windows_userdata_path));
    ASSERT(CreateDirectory(TEST_windows_userdata_path, NULL));
    ASSERT(write_file(buf, "\0\0\0\x2A", 4));

    CHECK_TRUE(id = userdata_register_stats(stats, lenof(stats)));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 42);
    CHECK_DOUBLEEQUAL(userdata_get_stat(1), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_windows_stats_corrupt_data_for_flag)
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
                           TEST_windows_userdata_path));
    ASSERT(CreateDirectory(TEST_windows_userdata_path, NULL));
    ASSERT(write_file(buf, "\0\1\x2A", 5));

    CHECK_TRUE(id = userdata_register_stats(stats, lenof(stats)));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 0);
    CHECK_DOUBLEEQUAL(userdata_get_stat(1), 1);
    CHECK_DOUBLEEQUAL(values_out[2].value, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_windows_stats_truncated_file_for_uint32)
{
    static const struct UserStatInfo stats[] = {
        {.id = 0, .type = USERDATA_STAT_UINT32},
        {.id = 1, .type = USERDATA_STAT_UINT32},
    };
    int id;

    char buf[1000];
    ASSERT(strformat_check(buf, sizeof(buf), "%sstats.bin",
                           TEST_windows_userdata_path));
    ASSERT(CreateDirectory(TEST_windows_userdata_path, NULL));
    ASSERT(write_file(buf, "\0\0\0\x2A", 4));

    CHECK_TRUE(id = userdata_register_stats(stats, lenof(stats)));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 42);
    CHECK_DOUBLEEQUAL(userdata_get_stat(1), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_windows_stats_truncated_file_for_double)
{
    static const struct UserStatInfo stats[] = {
        {.id = 0, .type = USERDATA_STAT_UINT32},
        {.id = 1, .type = USERDATA_STAT_DOUBLE},
    };
    int id;

    char buf[1000];
    ASSERT(strformat_check(buf, sizeof(buf), "%sstats.bin",
                           TEST_windows_userdata_path));
    ASSERT(CreateDirectory(TEST_windows_userdata_path, NULL));
    ASSERT(write_file(buf, "\0\0\0\x2A", 4));

    CHECK_TRUE(id = userdata_register_stats(stats, lenof(stats)));
    userdata_wait(id);
    CHECK_TRUE(userdata_get_result(id));
    CHECK_DOUBLEEQUAL(userdata_get_stat(0), 42);
    CHECK_DOUBLEEQUAL(userdata_get_stat(1), 0);

    return 1;
}

/*************************************************************************/
/*************************************************************************/
