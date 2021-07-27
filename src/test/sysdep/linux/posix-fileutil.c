/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/linux/posix-fileutil.c: Additional tests for the POSIX
 * file read/write utility functions which use Linux-specific system call
 * wrappers to inject failures.
 */

#include "src/base.h"
#include "src/memory.h"
#include "src/sysdep/posix/fileutil.h"
#include "src/sysdep/posix/path_max.h"
#include "src/test/base.h"
#include "src/test/sysdep/linux/wrap-io.h"
#include "src/test/sysdep/posix/internal.h"
#include "src/thread.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Temporary directory to be cleaned up after each test. */
static char tempdir[1000];

/*************************************************************************/
/************************* System call overrides *************************/
/*************************************************************************/

/**
 * short_read:  read() replacement which injects an end-of-file condition.
 */
static ssize_t short_read(UNUSED int fd, UNUSED void *buf, UNUSED size_t count)
{
    return 0;
}

/*-----------------------------------------------------------------------*/

/**
 * half_write:  write() replacement which injects an interrupt condition
 * after writing half the requested number of bytes (rounded up).
 */
static ssize_t half_write(int fd, const void *buf, size_t count)
{
    ASSERT(count > 0);
    return trampoline_write(fd, buf, (count+1)/2);
}

/*-----------------------------------------------------------------------*/

/**
 * fail_write:  write() replacement which injects an EIO error.
 */
static ssize_t fail_write(UNUSED int fd, UNUSED const void *buf,
                          UNUSED size_t count)
{
    errno = EIO;
    return -1;
}

/*-----------------------------------------------------------------------*/

/**
 * fail_close:  close() replacement which injects an EIO error.
 */
static int fail_close(int fd)
{
    /* Actually close the FD so we don't leak it. */
    trampoline_close(fd);
    errno = EIO;
    return -1;
}

/*-----------------------------------------------------------------------*/

/**
 * fail_fdatasync:  fdatasync() replacement which injects an EIO error.
 */
static int fail_fdatasync(UNUSED int fd)
{
    errno = EIO;
    return -1;
}

/*-----------------------------------------------------------------------*/

/**
 * fail_utime:  utime() replacement which injects an EIO error.
 */
static int fail_utime(UNUSED const char *filename,
                      UNUSED const struct utimbuf *times)
{
    errno = EIO;
    return -1;
}

/*-----------------------------------------------------------------------*/

/**
 * mkdir_racer_1:  mkdir() replacement which creates the target path as a
 * regular file and returns an EEXIST error, as if it lost a race with
 * another process on the same path.
 */
static int mkdir_racer_1(const char *pathname, UNUSED mode_t mode)
{
    ASSERT(open(pathname, O_WRONLY | O_CREAT | O_EXCL, 0666) >= 0);
    errno = EEXIST;
    return -1;
}

/*-----------------------------------------------------------------------*/

/**
 * mkdir_racer_2:  mkdir() replacement which leaves the pathname as a
 * nonexistent file and returns an EEXIST error, as if it lost a race with
 * another process which created and then deleted a file at the same path.
 */
static int mkdir_racer_2(const char *pathname, UNUSED mode_t mode)
{
    ASSERT(access(pathname, F_OK) == -1);
    errno = EEXIST;
    return -1;
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_linux_posix_fileutil)

/*-----------------------------------------------------------------------*/

TEST_INIT(init)
{
    clear_io_wrapper_variables();
    CHECK_TRUE(thread_init());
    CHECK_TRUE(posix_create_temporary_dir("test-posix-fileutil",
                                          tempdir, sizeof(tempdir)));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
    clear_io_wrapper_variables();

    int had_temp_files;
    if (!posix_remove_temporary_dir(tempdir, &had_temp_files)) {
        FAIL("Failed to remove temporary directory %s", tempdir);
    }
    CHECK_FALSE(had_temp_files);

    thread_cleanup();
    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_read_file_short_read)
{
    char path_1[1002];
    ASSERT(strformat_check(path_1, sizeof(path_1), "%s/1", tempdir));
    ASSERT(posix_write_file(path_1, "1", 1, 0));

    override_read = short_read;
    CHECK_FALSE(posix_read_file(path_1, (ssize_t[1]){0}, 0));
    override_read = NULL;

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_write_file_multiple_write)
{
    void *data;
    ssize_t size = -1;

    char path_2[1002];
    ASSERT(strformat_check(path_2, sizeof(path_2), "%s/2", tempdir));

    override_write = half_write;
    CHECK_TRUE(posix_write_file(path_2, "22", 2, 0));
    override_write = NULL;

    CHECK_TRUE(data = posix_read_file(path_2, &size, 0));
    CHECK_INTEQUAL(size, 2);
    CHECK_MEMEQUAL(data, "22", 2);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_write_file_fail_write)
{
    char path_1[1002], path_1_[1003];
    ASSERT(strformat_check(path_1, sizeof(path_1), "%s/1", tempdir));
    ASSERT(strformat_check(path_1_, sizeof(path_1_), "%s/1~", tempdir));

    override_write = fail_write;
    CHECK_FALSE(posix_write_file(path_1, "1", 1, 0));
    override_write = NULL;

    CHECK_FALSE(posix_read_file(path_1, (ssize_t[1]){0}, 0));
    CHECK_FALSE(posix_read_file(path_1_, (ssize_t[1]){0}, 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_write_file_fail_sync)
{
    char path_1[1002], path_1_[1003];
    ASSERT(strformat_check(path_1, sizeof(path_1), "%s/1", tempdir));
    ASSERT(strformat_check(path_1_, sizeof(path_1_), "%s/1~", tempdir));

    override_fdatasync = fail_fdatasync;
    CHECK_FALSE(posix_write_file(path_1, "1", 1, 1));
    override_fdatasync = NULL;

    CHECK_FALSE(posix_read_file(path_1, (ssize_t[1]){0}, 0));
    CHECK_FALSE(posix_read_file(path_1_, (ssize_t[1]){0}, 0));

    /* If we set the sync parameter to false, fdatasync() should not be
     * called at all. */
    override_fdatasync = fail_fdatasync;
    CHECK_TRUE(posix_write_file(path_1, "1", 1, 0));
    override_fdatasync = NULL;
    void *data;
    ssize_t size = -1;
    CHECK_TRUE(data = posix_read_file(path_1, &size, 0));
    CHECK_INTEQUAL(size, 1);
    CHECK_MEMEQUAL(data, "1", 1);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_write_file_fail_close)
{
    char path_1[1002];
    ASSERT(strformat_check(path_1, sizeof(path_1), "%s/1", tempdir));

    override_close = fail_close;
    CHECK_FALSE(posix_write_file(path_1, "1", 1, 0));
    override_close = NULL;

    CHECK_FALSE(posix_read_file(path_1, (ssize_t[1]){0}, 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_copy_file_multiple_write)
{
    void *data;
    ssize_t size = -1;

    char path_B[1002], path_2[1002];
    ASSERT(strformat_check(path_B, sizeof(path_B), "%s/B", tempdir));
    ASSERT(strformat_check(path_2, sizeof(path_2), "%s/2", tempdir));
    ASSERT(posix_write_file(path_B, "22", 2, 0));

    override_write = half_write;
    CHECK_TRUE(posix_copy_file(path_B, path_2, 0, 0));
    override_write = NULL;

    CHECK_TRUE(data = posix_read_file(path_2, &size, 0));
    CHECK_INTEQUAL(size, 2);
    CHECK_MEMEQUAL(data, "22", 2);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_copy_file_fail_write)
{
    char path_B[1002], path_2[1002];
    ASSERT(strformat_check(path_B, sizeof(path_B), "%s/B", tempdir));
    ASSERT(strformat_check(path_2, sizeof(path_2), "%s/2", tempdir));
    ASSERT(posix_write_file(path_B, "22", 2, 0));

    override_write = fail_write;
    CHECK_FALSE(posix_copy_file(path_B, path_2, 0, 0));
    override_write = NULL;

    CHECK_FALSE(posix_read_file(path_2, (ssize_t[1]){0}, 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_copy_file_fail_close)
{
    char path_B[1002], path_2[1002];
    ASSERT(strformat_check(path_B, sizeof(path_B), "%s/B", tempdir));
    ASSERT(strformat_check(path_2, sizeof(path_2), "%s/2", tempdir));
    ASSERT(posix_write_file(path_B, "22", 2, 0));

    override_close = fail_close;
    CHECK_FALSE(posix_copy_file(path_B, path_2, 0, 0));
    override_close = NULL;

    CHECK_FALSE(posix_read_file(path_2, (ssize_t[1]){0}, 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_copy_file_fail_utime)
{
    void *data;
    ssize_t size = -1;

    char path_B[1002], path_2[1002];
    ASSERT(strformat_check(path_B, sizeof(path_B), "%s/B", tempdir));
    ASSERT(strformat_check(path_2, sizeof(path_2), "%s/2", tempdir));
    ASSERT(posix_write_file(path_B, "22", 2, 0));

    override_utime = fail_utime;
    CHECK_FALSE(posix_copy_file(path_B, path_2, 1, 0));
    override_utime = NULL;
    CHECK_FALSE(posix_read_file(path_2, (ssize_t[1]){0}, 0));

    /* utime() should not be called at all when not preserving times. */
    override_utime = fail_utime;
    CHECK_TRUE(posix_copy_file(path_B, path_2, 0, 0));
    override_utime = NULL;
    CHECK_TRUE(data = posix_read_file(path_2, &size, 0));
    CHECK_INTEQUAL(size, 2);
    CHECK_MEMEQUAL(data, "22", 2);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mkdir_p_mkdir_race)
{
    char path_A[1002];
    ASSERT(strformat_check(path_A, sizeof(path_A), "%s/A", tempdir));

    override_mkdir = mkdir_racer_1;
    CHECK_FALSE(posix_mkdir_p(path_A));
    override_mkdir = NULL;
    struct stat st;
    CHECK_TRUE(stat(path_A, &st) == 0);
    CHECK_TRUE(S_ISREG(st.st_mode));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mkdir_p_stat_race)
{
    char path_A[1002];
    ASSERT(strformat_check(path_A, sizeof(path_A), "%s/A", tempdir));

    override_mkdir = mkdir_racer_2;
    CHECK_FALSE(posix_mkdir_p(path_A));
    override_mkdir = NULL;
    CHECK_FALSE(access(path_A, F_OK) == 0);

    return 1;
}

/*************************************************************************/
/*************************************************************************/
