/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/posix/fileutil.c: Tests for the POSIX file read/write
 * utility functions.
 */

#include "src/base.h"
#include "src/memory.h"
#include "src/sysdep/posix/fileutil.h"
#include "src/sysdep/posix/path_max.h"
#include "src/test/base.h"
#include "src/test/sysdep/posix/internal.h"
#include "src/thread.h"

/* iOS fails if the absolute path of a file is longer than PATH_MAX, even
 * when using a relative reference shorter than PATH_MAX, so don't try to
 * create long nested paths on that platform. */
#ifndef SIL_PLATFORM_IOS
# define CAN_CREATE_LONG_NESTED_PATHS
#endif

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Temporary directory to be cleaned up after each test. */
static char tempdir[1000];

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

#ifdef CAN_CREATE_LONG_NESTED_PATHS

/**
 * create_long_nested_path:  Generate a directory structure inside the
 * given directory with a single file under nested directories, such that
 * no path component has a length longer than 63 bytes but the total path
 * length is greater than PATH_MAX.  Does not clean up on failure (assuming
 * the entire tree will be zapped during cleanup anyway).
 *
 * [Parameters]
 *     base_path: Pathname of base directory to use.
 * [Return value]
 *     Open file descriptor to the lowest (most-nested) directory, or -1
 *     on error.
 */
static int create_long_nested_path(const char *base_path)
{
    PRECOND(base_path != NULL, return 0);

    const char *a49 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    const char *a63 =
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

    int cwd_fd;
    ASSERT((cwd_fd = open(".", O_RDONLY)) >= 0);
    ASSERT(chdir(base_path) == 0);

    const int num_dirs_needed = ((PATH_MAX-1) - (int)strlen(base_path)) / 50;
    for (int i = 0; i < num_dirs_needed; i++) {
        if (mkdir(a49, S_IRWXU) != 0) {
            DLOG("mkdir() for dir %d/%d: %s", i+1, num_dirs_needed,
                 strerror(errno));
            goto error;
        }
        if (chmod(a49, S_IRWXU) != 0) {
            DLOG("chmod() for dir %d/%d: %s", i+1, num_dirs_needed,
                 strerror(errno));
            goto error;
        }
        if (chdir(a49) != 0) {
            DLOG("chdir() for dir %d/%d: %s", i+1, num_dirs_needed,
                 strerror(errno));
            goto error;
        }
    }

    const int dir_fd = open(".", O_RDONLY);
    if (dir_fd < 0) {
        DLOG("open() for lowest directory: %s", strerror(errno));
        goto error;
    }

    int fd = open(a63, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        DLOG("open(O_CREAT) for final file: %s", strerror(errno));
        goto error;
    }

    CHECK_INTEQUAL(fchdir(cwd_fd), 0);
    CHECK_INTEQUAL(close(cwd_fd), 0);
    return dir_fd;

  error:
    CHECK_INTEQUAL(fchdir(cwd_fd), 0);
    CHECK_INTEQUAL(close(cwd_fd), 0);
    return -1;
}

#endif  // CAN_CREATE_LONG_NESTED_PATHS

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_posix_fileutil)

/*-----------------------------------------------------------------------*/

TEST_INIT(init)
{
    CHECK_TRUE(thread_init());

    CHECK_TRUE(posix_create_temporary_dir("test-posix-fileutil",
                                          tempdir, sizeof(tempdir)));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
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

TEST(test_read_file)
{
    void *data;
    ssize_t size = -1;

    char path_0[1002], path_1[1002], path_2[1002];
    ASSERT(strformat_check(path_0, sizeof(path_0), "%s/0", tempdir));
    ASSERT(strformat_check(path_1, sizeof(path_1), "%s/1", tempdir));
    ASSERT(strformat_check(path_2, sizeof(path_2), "%s/2", tempdir));
    int fd;
    ASSERT((fd = open(path_0, O_WRONLY | O_CREAT | O_EXCL, 0666)) >= 0);
    ASSERT(close(fd) == 0);
    ASSERT((fd = open(path_1, O_WRONLY | O_CREAT | O_EXCL, 0666)) >= 0);
    ASSERT(write(fd, "1", 1) == 1);
    ASSERT(close(fd) == 0);
    ASSERT((fd = open(path_2, O_WRONLY | O_CREAT | O_EXCL, 0666)) >= 0);
    ASSERT(write(fd, "22", 2) == 2);
    ASSERT(close(fd) == 0);

    CHECK_TRUE(data = posix_read_file(path_0, &size, 0));
    CHECK_INTEQUAL(size, 0);
    mem_free(data);
    CHECK_TRUE(data = posix_read_file(path_1, &size, 0));
    CHECK_INTEQUAL(size, 1);
    CHECK_MEMEQUAL(data, "1", 1);
    mem_free(data);
    CHECK_TRUE(data = posix_read_file(path_2, &size, 0));
    CHECK_INTEQUAL(size, 2);
    CHECK_MEMEQUAL(data, "22", 2);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_read_file_unseekable)
{
    void *data;
    ssize_t size = -1;

    char path_0[1002];
    ASSERT(strformat_check(path_0, sizeof(path_0), "%s/0", tempdir));
    CHECK_INTEQUAL(mkfifo(path_0, S_IRUSR | S_IWUSR), 0);

    int writer_thread;
    ASSERT(writer_thread = thread_create(posix_pipe_writer, path_0));
    data = posix_read_file(path_0, &size, 0);
    thread_wait(writer_thread);
    CHECK_FALSE(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_read_file_dir)
{
    ssize_t size = -1;

    char path_0[1002];
    ASSERT(strformat_check(path_0, sizeof(path_0), "%s/0", tempdir));

    CHECK_INTEQUAL(mkdir(path_0, S_IRWXU), 0);
    /* Explicitly chmod in case (umask & S_IRWXU) != 0. */
    CHECK_INTEQUAL(chmod(path_0, S_IRWXU), 0);
    CHECK_FALSE(posix_read_file(path_0, &size, 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_read_file_memory_failures)
{
    void *data;
    ssize_t size = -1;

    char path_0[1002], path_1[1002], path_2[1002];
    ASSERT(strformat_check(path_0, sizeof(path_0), "%s/0", tempdir));
    ASSERT(strformat_check(path_1, sizeof(path_1), "%s/1", tempdir));
    ASSERT(strformat_check(path_2, sizeof(path_2), "%s/2", tempdir));
    int fd;
    ASSERT((fd = open(path_0, O_WRONLY | O_CREAT | O_EXCL, 0666)) >= 0);
    ASSERT(close(fd) == 0);
    ASSERT((fd = open(path_1, O_WRONLY | O_CREAT | O_EXCL, 0666)) >= 0);
    ASSERT(write(fd, "1", 1) == 1);
    ASSERT(close(fd) == 0);
    ASSERT((fd = open(path_2, O_WRONLY | O_CREAT | O_EXCL, 0666)) >= 0);
    ASSERT(write(fd, "22", 2) == 2);
    ASSERT(close(fd) == 0);

    CHECK_MEMORY_FAILURES(data = posix_read_file(path_0, &size, 0));
    CHECK_INTEQUAL(size, 0);
    mem_free(data);
    CHECK_MEMORY_FAILURES(data = posix_read_file(path_1, &size, 0));
    CHECK_INTEQUAL(size, 1);
    CHECK_MEMEQUAL(data, "1", 1);
    mem_free(data);
    CHECK_MEMORY_FAILURES(data = posix_read_file(path_2, &size, 0));
    CHECK_INTEQUAL(size, 2);
    CHECK_MEMEQUAL(data, "22", 2);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_read_file_nonexistent)
{
    char path_A[1002];
    ASSERT(strformat_check(path_A, sizeof(path_A), "%s/A", tempdir));

    ssize_t size = 123456789;
    CHECK_FALSE(posix_read_file(path_A, &size, 0));
    CHECK_INTEQUAL(size, 123456789);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_write_file)
{
    void *data;
    ssize_t size = -1;

    char path_A[1002], path_B[1002], path_C[1002];
    char path_A_[1003], path_B_[1003], path_C_[1003];
    ASSERT(strformat_check(path_A, sizeof(path_A), "%s/A", tempdir));
    ASSERT(strformat_check(path_B, sizeof(path_B), "%s/B", tempdir));
    ASSERT(strformat_check(path_C, sizeof(path_C), "%s/C", tempdir));
    ASSERT(strformat_check(path_A_, sizeof(path_A_), "%s/A~", tempdir));
    ASSERT(strformat_check(path_B_, sizeof(path_B_), "%s/B~", tempdir));
    ASSERT(strformat_check(path_C_, sizeof(path_C_), "%s/C~", tempdir));

    CHECK_TRUE(posix_write_file(path_A, "", 0, 0));
    CHECK_TRUE(data = posix_read_file(path_A, &size, 0));
    CHECK_INTEQUAL(size, 0);
    mem_free(data);
    CHECK_TRUE(posix_write_file(path_B, "B", 1, 0));
    CHECK_TRUE(data = posix_read_file(path_B, &size, 0));
    CHECK_INTEQUAL(size, 1);
    CHECK_MEMEQUAL(data, "B", 1);
    mem_free(data);
    CHECK_TRUE(posix_write_file(path_C, "CC", 2, 0));
    CHECK_TRUE(data = posix_read_file(path_C, &size, 0));
    CHECK_INTEQUAL(size, 2);
    CHECK_MEMEQUAL(data, "CC", 2);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_write_file_sync)
{
    void *data;
    ssize_t size = -1;

    char path_A[1002];
    ASSERT(strformat_check(path_A, sizeof(path_A), "%s/A", tempdir));

    /* We can't (at least portably/consistently) test whether the sync was
     * actually performed, so just make sure the call doesn't fail. */
    CHECK_TRUE(posix_write_file(path_A, "1", 1, 1));
    CHECK_TRUE(data = posix_read_file(path_A, &size, 0));
    CHECK_INTEQUAL(size, 1);
    CHECK_MEMEQUAL(data, "1", 1);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_write_file_overwrite)
{
    void *data;
    ssize_t size = -1;

    char path_A[1002];
    ASSERT(strformat_check(path_A, sizeof(path_A), "%s/A", tempdir));

    CHECK_TRUE(posix_write_file(path_A, "1", 1, 0));
    CHECK_TRUE(data = posix_read_file(path_A, &size, 0));
    CHECK_INTEQUAL(size, 1);
    CHECK_MEMEQUAL(data, "1", 1);
    mem_free(data);

    CHECK_TRUE(posix_write_file(path_A, "22", 2, 0));
    CHECK_TRUE(data = posix_read_file(path_A, &size, 0));
    CHECK_INTEQUAL(size, 2);
    CHECK_MEMEQUAL(data, "22", 2);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_write_file_overwrite_unwritable)
{
    void *data;
    ssize_t size = -1;

    char path_A[1002];
    ASSERT(strformat_check(path_A, sizeof(path_A), "%s/A", tempdir));

    CHECK_TRUE(posix_write_file(path_A, "1", 1, 0));
    CHECK_TRUE(data = posix_read_file(path_A, &size, 0));
    CHECK_INTEQUAL(size, 1);
    CHECK_MEMEQUAL(data, "1", 1);
    mem_free(data);

    CHECK_TRUE(chmod(path_A, 0444) == 0);

    CHECK_FALSE(posix_write_file(path_A, "22", 2, 0));
    CHECK_TRUE(data = posix_read_file(path_A, &size, 0));
    CHECK_INTEQUAL(size, 1);
    CHECK_MEMEQUAL(data, "1", 1);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_write_file_parent_directories)
{
    void *data;
    ssize_t size = -1;

    char path_D_y[1004], path_E_F_z[1006];
    ASSERT(strformat_check(path_D_y, sizeof(path_D_y), "%s/D/y", tempdir));
    ASSERT(strformat_check(path_E_F_z, sizeof(path_E_F_z), "%s/E/F/z",
                           tempdir));

    CHECK_TRUE(posix_write_file(path_D_y, "333", 3, 0));
    CHECK_TRUE(data = posix_read_file(path_D_y, &size, 0));
    CHECK_INTEQUAL(size, 3);
    CHECK_MEMEQUAL(data, "333", 3);
    mem_free(data);

    CHECK_TRUE(posix_write_file(path_E_F_z, "4444", 4, 0));
    CHECK_TRUE(data = posix_read_file(path_E_F_z, &size, 0));
    CHECK_INTEQUAL(size, 4);
    CHECK_MEMEQUAL(data, "4444", 4);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_write_file_current_directory)
{
    void *data;
    ssize_t size = -1;

    char path_A[1002];
    ASSERT(strformat_check(path_A, sizeof(path_A), "%s/A", tempdir));

    char cwd[PATH_MAX];
    CHECK_TRUE(getcwd(cwd, sizeof(cwd)));
    ASSERT(chdir(tempdir) == 0);
    const int result = posix_write_file("A", "1", 1, 0);
    ASSERT(chdir(cwd) == 0);
    CHECK_TRUE(result);
    CHECK_TRUE(data = posix_read_file(path_A, &size, 0));
    CHECK_INTEQUAL(size, 1);
    CHECK_MEMEQUAL(data, "1", 1);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_write_file_unwritable_dir)
{
    char path_0[1002], path_0_1[1004];
    ASSERT(strformat_check(path_0, sizeof(path_0), "%s/0", tempdir));
    ASSERT(strformat_check(path_0_1, sizeof(path_0_1), "%s/0/1", tempdir));

    CHECK_INTEQUAL(mkdir(path_0, S_IRUSR | S_IXUSR), 0);
    CHECK_INTEQUAL(chmod(path_0, S_IRUSR | S_IXUSR), 0);
    CHECK_INTEQUAL(access(path_0, F_OK), 0);
    CHECK_FALSE(posix_write_file(path_0_1, "x", 1, 0));
    CHECK_INTEQUAL(chmod(path_0, S_IRWXU), 0);
    CHECK_INTEQUAL(access(path_0_1, F_OK), -1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_write_file_colliding_file_and_dir_names)
{
    void *data;
    ssize_t size = -1;

    char path_A[1002], path_B[1002], path_C[1002], path_D[1002];
    char path_A_p_q[1006], path_C_x[1004], path_D_y[1004];
    ASSERT(strformat_check(path_A, sizeof(path_A), "%s/A", tempdir));
    ASSERT(strformat_check(path_B, sizeof(path_B), "%s/B", tempdir));
    ASSERT(strformat_check(path_C, sizeof(path_C), "%s/C", tempdir));
    ASSERT(strformat_check(path_D, sizeof(path_D), "%s/D", tempdir));
    ASSERT(strformat_check(path_A_p_q, sizeof(path_A_p_q), "%s/A/p/q",
                           tempdir));
    ASSERT(strformat_check(path_C_x, sizeof(path_C_x), "%s/C/x", tempdir));
    ASSERT(strformat_check(path_D_y, sizeof(path_D_y), "%s/D/y", tempdir));

    CHECK_TRUE(posix_write_file(path_A, "", 0, 0));
    CHECK_TRUE(data = posix_read_file(path_A, &size, 0));
    mem_free(data);
    CHECK_TRUE(posix_write_file(path_B, "B", 1, 0));
    CHECK_TRUE(data = posix_read_file(path_B, &size, 0));
    mem_free(data);
    CHECK_TRUE(posix_write_file(path_C, "CC", 2, 0));
    CHECK_TRUE(data = posix_read_file(path_C, &size, 0));
    mem_free(data);
    CHECK_TRUE(posix_write_file(path_D_y, "333", 3, 0));
    CHECK_TRUE(data = posix_read_file(path_D_y, &size, 0));
    mem_free(data);

    CHECK_FALSE(posix_write_file(path_D, "x", 1, 0));
    CHECK_FALSE(posix_read_file(path_D, &size, 0));
    CHECK_FALSE(posix_write_file(path_C_x, "x", 1, 0));
    CHECK_FALSE(posix_read_file(path_C_x, &size, 0));
    CHECK_FALSE(posix_write_file(path_A_p_q, "x", 1, 0));
    CHECK_FALSE(posix_read_file(path_A_p_q, &size, 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_write_file_overlength_path)
{
    ssize_t size;

    char path_long[PATH_MAX];
    ASSERT(strformat_check(path_long, sizeof(path_long), "%s/", tempdir));
    memset(path_long + strlen(path_long), 'a',
           (sizeof(path_long)-1) - strlen(path_long));
    path_long[sizeof(path_long)-1] = 0;

    CHECK_FALSE(posix_write_file(path_long, "x", 1, 0));
    CHECK_FALSE(posix_read_file(path_long, &size, 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_copy_file)
{
    void *data;
    ssize_t size = -1;

    char path_0[1002], path_1[1002], path_2[1002];
    char path_A[1002], path_D_y[1004], path_E_F_z[1006];
    ASSERT(strformat_check(path_0, sizeof(path_0), "%s/0", tempdir));
    ASSERT(strformat_check(path_1, sizeof(path_1), "%s/1", tempdir));
    ASSERT(strformat_check(path_2, sizeof(path_2), "%s/2", tempdir));
    ASSERT(strformat_check(path_A, sizeof(path_A), "%s/A", tempdir));
    ASSERT(strformat_check(path_D_y, sizeof(path_D_y), "%s/D/y", tempdir));
    ASSERT(strformat_check(path_E_F_z, sizeof(path_E_F_z), "%s/E/F/z",
                           tempdir));
    CHECK_TRUE(posix_write_file(path_0, "", 0, 0));
    CHECK_TRUE(posix_write_file(path_1, "1", 1, 0));
    CHECK_TRUE(posix_write_file(path_2, "22", 2, 0));

    CHECK_TRUE(posix_copy_file(path_0, path_A, 0, 0));
    CHECK_TRUE(data = posix_read_file(path_A, &size, 0));
    CHECK_INTEQUAL(size, 0);
    mem_free(data);
    CHECK_TRUE(posix_copy_file(path_1, path_D_y, 0, 0));
    CHECK_TRUE(data = posix_read_file(path_D_y, &size, 0));
    CHECK_INTEQUAL(size, 1);
    CHECK_MEMEQUAL(data, "1", 1);
    mem_free(data);
    CHECK_TRUE(posix_copy_file(path_2, path_E_F_z, 0, 0));
    CHECK_TRUE(data = posix_read_file(path_E_F_z, &size, 0));
    CHECK_INTEQUAL(size, 2);
    CHECK_MEMEQUAL(data, "22", 2);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_copy_file_overwrite_unwritable)
{
    void *data;
    ssize_t size = -1;

    char path_1[1002], path_A[1002];
    ASSERT(strformat_check(path_1, sizeof(path_1), "%s/1", tempdir));
    ASSERT(strformat_check(path_A, sizeof(path_A), "%s/A", tempdir));
    CHECK_TRUE(posix_write_file(path_1, "1", 1, 0));
    CHECK_TRUE(posix_write_file(path_A, "AA", 2, 0));

    CHECK_TRUE(chmod(path_A, 0444) == 0);

    CHECK_FALSE(posix_copy_file(path_1, path_A, 0, 0));
    CHECK_TRUE(data = posix_read_file(path_A, &size, 0));
    CHECK_INTEQUAL(size, 2);
    CHECK_MEMEQUAL(data, "AA", 2);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_copy_file_memory_failures)
{
    void *data;
    ssize_t size = -1;

    char path_1[1002], path_A[1002];
    ASSERT(strformat_check(path_1, sizeof(path_1), "%s/1", tempdir));
    ASSERT(strformat_check(path_A, sizeof(path_A), "%s/A", tempdir));
    CHECK_TRUE(posix_write_file(path_1, "1", 1, 0));

    CHECK_MEMORY_FAILURES(posix_copy_file(path_1, path_A, 0, 0));
    CHECK_TRUE(data = posix_read_file(path_A, &size, 0));
    CHECK_INTEQUAL(size, 1);
    CHECK_MEMEQUAL(data, "1", 1);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_copy_file_small_buffer)
{
    void *data;
    ssize_t size = -1;

    char path_1[1002], path_A[1002];
    ASSERT(strformat_check(path_1, sizeof(path_1), "%s/1", tempdir));
    ASSERT(strformat_check(path_A, sizeof(path_A), "%s/A", tempdir));
    CHECK_TRUE(posix_write_file(path_1, "0123456789", 10, 0));

    CHECK_MEMORY_FAILURES(posix_copy_file(path_1, path_A, 0, 3));
    CHECK_TRUE(data = posix_read_file(path_A, &size, 0));
    CHECK_INTEQUAL(size, 10);
    CHECK_MEMEQUAL(data, "0123456789", 10);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_copy_file_current_directory)
{
    void *data;
    ssize_t size = -1;

    char path_1[1002], path_A[1002];
    ASSERT(strformat_check(path_1, sizeof(path_1), "%s/1", tempdir));
    ASSERT(strformat_check(path_A, sizeof(path_A), "%s/A", tempdir));
    CHECK_TRUE(posix_write_file(path_1, "1", 1, 0));

    char cwd[PATH_MAX];
    CHECK_TRUE(getcwd(cwd, sizeof(cwd)));
    ASSERT(chdir(tempdir) == 0);
    const int result = posix_copy_file("1", "A", 0, 0);
    ASSERT(chdir(cwd) == 0);
    CHECK_TRUE(result);
    CHECK_TRUE(data = posix_read_file(path_A, &size, 0));
    CHECK_INTEQUAL(size, 1);
    CHECK_MEMEQUAL(data, "1", 1);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_copy_file_nonexistent_source)
{
    void *data;
    ssize_t size = -1;

    char path_0[1002], path_1[1002];
    ASSERT(strformat_check(path_0, sizeof(path_0), "%s/0", tempdir));
    ASSERT(strformat_check(path_1, sizeof(path_1), "%s/1", tempdir));

    CHECK_TRUE(posix_write_file(path_1, "1", 1, 0));
    CHECK_FALSE(posix_copy_file(path_0, path_1, 0, 0));
    CHECK_TRUE(data = posix_read_file(path_1, &size, 0));
    CHECK_INTEQUAL(size, 1);
    CHECK_MEMEQUAL(data, "1", 1);
    mem_free(data);

    CHECK_TRUE(posix_write_file(path_1, "1", 1, 0));
    CHECK_FALSE(posix_copy_file(path_0, path_1, 1, 0));
    CHECK_TRUE(data = posix_read_file(path_1, &size, 0));
    CHECK_INTEQUAL(size, 1);
    CHECK_MEMEQUAL(data, "1", 1);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_copy_file_dir_source)
{
    void *data;
    ssize_t size = -1;

    char path_0[1002], path_1[1002];
    ASSERT(strformat_check(path_0, sizeof(path_0), "%s/0", tempdir));
    ASSERT(strformat_check(path_1, sizeof(path_1), "%s/1", tempdir));

    CHECK_INTEQUAL(mkdir(path_0, S_IRWXU), 0);
    CHECK_INTEQUAL(chmod(path_0, S_IRWXU), 0);
    CHECK_TRUE(posix_write_file(path_1, "1", 1, 0));
    CHECK_FALSE(posix_copy_file(path_0, path_1, 0, 0));
    CHECK_TRUE(data = posix_read_file(path_1, &size, 0));
    CHECK_INTEQUAL(size, 1);
    CHECK_MEMEQUAL(data, "1", 1);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_copy_file_unreadable_source)
{
    void *data;
    ssize_t size = -1;

    char path_0[1002], path_1[1002];
    ASSERT(strformat_check(path_0, sizeof(path_0), "%s/0", tempdir));
    ASSERT(strformat_check(path_1, sizeof(path_1), "%s/1", tempdir));

    CHECK_TRUE(posix_write_file(path_0, "", 0, 0));
    CHECK_INTEQUAL(chmod(path_0, 0), 0);
    CHECK_TRUE(posix_write_file(path_1, "1", 1, 0));
    CHECK_FALSE(posix_copy_file(path_0, path_1, 0, 0));
    CHECK_TRUE(data = posix_read_file(path_1, &size, 0));
    CHECK_INTEQUAL(size, 1);
    CHECK_MEMEQUAL(data, "1", 1);
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

/* Check that timestamps are preserved when (and only when) requested.
 * For this test, we assume (but also check) that the test data files
 * used by other tests have different timestamps than newly-created
 * files will have. */
TEST(test_copy_file_timestamps)
{
    char path_A[1002], path_B[1002];
    ASSERT(strformat_check(path_A, sizeof(path_A), "%s/A", tempdir));
    ASSERT(strformat_check(path_B, sizeof(path_B), "%s/B", tempdir));

    struct stat st1, st2;

    CHECK_TRUE(posix_write_file(path_A, "A", 1, 0));
    CHECK_INTEQUAL(stat(path_A, &st1), 0);
    DLOG("Waiting for mtime change...");
    for (int try = 0; try < 21; try++) {
        nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = 100000000}, NULL);
        CHECK_TRUE(posix_write_file(path_B, "B", 1, 0));
        CHECK_INTEQUAL(stat(path_B, &st2), 0);
        if (st2.st_mtime != st1.st_mtime) {
            break;
        }
    }
    /* 21 tries (2.1 seconds) is enough for an mtime change even on FAT
     * filesystems. */
    ASSERT(st2.st_mtime != st1.st_mtime);

    CHECK_TRUE(posix_copy_file(path_A, path_B, 0, 0));
    CHECK_INTEQUAL(stat(path_B, &st2), 0);
    CHECK_FALSE(st1.st_mtime == st2.st_mtime);
    CHECK_TRUE(posix_copy_file(path_A, path_B, 1, 0));
    CHECK_INTEQUAL(stat(path_B, &st2), 0);
    CHECK_TRUE(st1.st_mtime == st2.st_mtime);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_copy_file_unwritable_dir)
{
    char path_A[1002], path_0[1002], path_0_1[1004];
    ASSERT(strformat_check(path_A, sizeof(path_A), "%s/A", tempdir));
    ASSERT(strformat_check(path_0, sizeof(path_0), "%s/0", tempdir));
    ASSERT(strformat_check(path_0_1, sizeof(path_0_1), "%s/0/1", tempdir));

    CHECK_TRUE(posix_write_file(path_A, "A", 1, 0));
    CHECK_INTEQUAL(mkdir(path_0, S_IRUSR | S_IXUSR), 0);
    CHECK_INTEQUAL(chmod(path_0, S_IRUSR | S_IXUSR), 0);
    CHECK_INTEQUAL(access(path_0, F_OK), 0);
    CHECK_FALSE(posix_copy_file(path_A, path_0_1, 0, 0));
    CHECK_INTEQUAL(chmod(path_0, S_IRWXU), 0);
    CHECK_INTEQUAL(access(path_0_1, F_OK), -1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_copy_file_colliding_file_and_dir_names)
{
    ssize_t size = -1;

    char path_0[1002], path_A[1002], path_D[1002];
    char path_A_p_q[1006], path_A_x[1004], path_D_y[1004];
    ASSERT(strformat_check(path_0, sizeof(path_0), "%s/0", tempdir));
    ASSERT(strformat_check(path_A, sizeof(path_A), "%s/A", tempdir));
    ASSERT(strformat_check(path_D, sizeof(path_D), "%s/D", tempdir));
    ASSERT(strformat_check(path_A_p_q, sizeof(path_A_p_q), "%s/A/p/q",
                           tempdir));
    ASSERT(strformat_check(path_A_x, sizeof(path_A_x), "%s/A/x", tempdir));
    ASSERT(strformat_check(path_D_y, sizeof(path_D_y), "%s/D/y", tempdir));
    CHECK_TRUE(posix_write_file(path_0, "", 0, 0));
    CHECK_TRUE(posix_write_file(path_A, "1", 1, 0));
    CHECK_TRUE(posix_write_file(path_D_y, "22", 2, 0));

    CHECK_FALSE(posix_copy_file(path_0, path_D, 0, 0));
    CHECK_FALSE(posix_read_file(path_D, &size, 0));
    CHECK_FALSE(posix_copy_file(path_0, path_A_x, 0, 0));
    CHECK_FALSE(posix_read_file(path_A_x, &size, 0));
    CHECK_FALSE(posix_copy_file(path_0, path_A_p_q, 0, 0));
    CHECK_FALSE(posix_read_file(path_A_p_q, &size, 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_copy_file_overlength_path)
{
    ssize_t size;

    char path_0[1002], path_long[PATH_MAX];
    ASSERT(strformat_check(path_0, sizeof(path_0), "%s/0", tempdir));
    ASSERT(strformat_check(path_long, sizeof(path_long), "%s/", tempdir));
    memset(path_long + strlen(path_long), 'a',
           (sizeof(path_long)-1) - strlen(path_long));
    path_long[sizeof(path_long)-1] = 0;

    CHECK_TRUE(posix_write_file(path_0, "", 0, 0));
    CHECK_FALSE(posix_copy_file(path_0, path_long, 0, 0));
    CHECK_FALSE(posix_read_file(path_long, &size, 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mkdir_p)
{
    char path_0[1002];
    ASSERT(strformat_check(path_0, sizeof(path_0), "%s/0", tempdir));

    CHECK_TRUE(posix_mkdir_p(path_0));

    struct stat st;
    CHECK_TRUE(stat(path_0, &st) == 0);
    CHECK_TRUE(S_ISDIR(st.st_mode));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mkdir_p_exists)
{
    struct stat st;
    CHECK_TRUE(stat(tempdir, &st) == 0);
    CHECK_TRUE(S_ISDIR(st.st_mode));

    CHECK_TRUE(posix_mkdir_p(tempdir));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mkdir_p_trailing_dot_component)
{
    char path_0[1002], path_0_dot[1004];
    ASSERT(strformat_check(path_0, sizeof(path_0), "%s/0", tempdir));
    ASSERT(strformat_check(path_0_dot, sizeof(path_0_dot), "%s/0/.", tempdir));

    CHECK_TRUE(posix_mkdir_p(path_0_dot));

    struct stat st;
    CHECK_TRUE(stat(path_0, &st) == 0);
    CHECK_TRUE(S_ISDIR(st.st_mode));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mkdir_p_over_file)
{
    char path_0[1002];
    ASSERT(strformat_check(path_0, sizeof(path_0), "%s/0", tempdir));

    CHECK_TRUE(posix_write_file(path_0, "", 0, 0));
    CHECK_FALSE(posix_mkdir_p(path_0));
    CHECK_INTEQUAL(errno, EEXIST);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mkdir_p_subdir_over_file)
{
    char path_0[1002], path_dot_0[1004];
    ASSERT(strformat_check(path_0, sizeof(path_0), "%s/0", tempdir));
    ASSERT(strformat_check(path_dot_0, sizeof(path_0), "%s/./0", tempdir));

    CHECK_TRUE(posix_write_file(path_0, "", 0, 0));
    CHECK_FALSE(posix_mkdir_p(path_dot_0));
    CHECK_INTEQUAL(errno, EEXIST);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mkdir_p_parent_over_file)
{
    char path_0[1002], path_0_1[1004];
    ASSERT(strformat_check(path_0, sizeof(path_0), "%s/0", tempdir));
    ASSERT(strformat_check(path_0_1, sizeof(path_0_1), "%s/0/1", tempdir));

    CHECK_TRUE(posix_write_file(path_0, "", 0, 0));
    CHECK_FALSE(posix_mkdir_p(path_0_1));
    CHECK_INTEQUAL(errno, ENOTDIR);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mkdir_p_in_unwritable_dir)
{
    char path_0[1002], path_0_1[1004];
    ASSERT(strformat_check(path_0, sizeof(path_0), "%s/0", tempdir));
    ASSERT(strformat_check(path_0_1, sizeof(path_0_1), "%s/0/1", tempdir));

    CHECK_TRUE(mkdir(path_0, 0555) == 0);
    CHECK_FALSE(posix_mkdir_p(path_0_1));
    CHECK_INTEQUAL(errno, EACCES);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mkdir_p_parent_in_unwritable_dir)
{
    char path_0[1002], path_0_1_2[1004];
    ASSERT(strformat_check(path_0, sizeof(path_0), "%s/0", tempdir));
    ASSERT(strformat_check(path_0_1_2, sizeof(path_0_1_2),
                           "%s/0/1/2", tempdir));

    CHECK_TRUE(mkdir(path_0, 0555) == 0);
    CHECK_FALSE(posix_mkdir_p(path_0_1_2));
    CHECK_INTEQUAL(errno, EACCES);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mkdir_p_buffer_overflow)
{
    char long_path[PATH_MAX+2];
    int len = strformat(long_path, sizeof(long_path), "%s", tempdir);
    ASSERT(len < (int)sizeof(long_path));
    while (len < PATH_MAX) {
        long_path[len++] = '/';
        long_path[len++] = '.';
    }
    long_path[len-1] = '0';
    long_path[len] = '\0';

    CHECK_FALSE(posix_mkdir_p(long_path));
    CHECK_INTEQUAL(errno, ENAMETOOLONG);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_rmdir_r_nonexistent)
{
    char path_0[1002];
    ASSERT(strformat_check(path_0, sizeof(path_0), "%s/0", tempdir));

    CHECK_FALSE(posix_rmdir_r(path_0));
    CHECK_INTEQUAL(errno, ENOENT);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_rmdir_r_empty_dir)
{
    char path_0[1002];
    ASSERT(strformat_check(path_0, sizeof(path_0), "%s/0", tempdir));

    CHECK_INTEQUAL(mkdir(path_0, S_IRWXU), 0);
    CHECK_INTEQUAL(chmod(path_0, S_IRWXU), 0);
    CHECK_INTEQUAL(access(path_0, F_OK), 0);
    CHECK_TRUE(posix_rmdir_r(path_0));
    CHECK_INTEQUAL(access(path_0, F_OK), -1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_rmdir_r_unwritable_empty_dir)
{
    char path_0[1002], path_0_1[1004];
    ASSERT(strformat_check(path_0, sizeof(path_0), "%s/0", tempdir));
    ASSERT(strformat_check(path_0_1, sizeof(path_0_1), "%s/0/1", tempdir));

    /* Check that an empty, unwritable directory can be removed.  (We don't
     * actually depend on this; this is just to verify expected OS behavior
     * vis-a-vis the test_rmdir_r_unwritable_dir() test below.) */
    CHECK_INTEQUAL(mkdir(path_0, S_IRUSR | S_IXUSR), 0);
    CHECK_INTEQUAL(chmod(path_0, S_IRUSR | S_IXUSR), 0);
    CHECK_INTEQUAL(access(path_0, F_OK), 0);
    CHECK_FALSE(posix_write_file(path_0_1, "x", 1, 0));
    CHECK_INTEQUAL(access(path_0_1, F_OK), -1);
    CHECK_TRUE(posix_rmdir_r(path_0));
    CHECK_INTEQUAL(access(path_0, F_OK), -1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_rmdir_r_nonempty_dir)
{
    char path_0[1002], path_0_1[1004], path_0_2[1004];
    ASSERT(strformat_check(path_0, sizeof(path_0), "%s/0", tempdir));
    ASSERT(strformat_check(path_0_1, sizeof(path_0_1), "%s/0/1", tempdir));
    ASSERT(strformat_check(path_0_2, sizeof(path_0_2), "%s/0/2", tempdir));

    CHECK_INTEQUAL(mkdir(path_0, S_IRWXU), 0);
    CHECK_INTEQUAL(chmod(path_0, S_IRWXU), 0);
    CHECK_INTEQUAL(access(path_0, F_OK), 0);
    CHECK_TRUE(posix_write_file(path_0_1, "x", 1, 0));
    CHECK_INTEQUAL(access(path_0_1, F_OK), 0);
    CHECK_INTEQUAL(mkdir(path_0_2, S_IRWXU), 0);
    CHECK_INTEQUAL(chmod(path_0_2, S_IRWXU), 0);
    CHECK_INTEQUAL(access(path_0_2, F_OK), 0);
    CHECK_TRUE(posix_rmdir_r(path_0));
    CHECK_INTEQUAL(access(path_0, F_OK), -1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_rmdir_r_broken_symlink)
{
    char path_0[1002], path_0_1[1004], path_0_2[1004], path_0_3[1004];
    ASSERT(strformat_check(path_0, sizeof(path_0), "%s/0", tempdir));
    ASSERT(strformat_check(path_0_1, sizeof(path_0_1), "%s/0/1", tempdir));
    ASSERT(strformat_check(path_0_2, sizeof(path_0_2), "%s/0/2", tempdir));
    ASSERT(strformat_check(path_0_3, sizeof(path_0_3), "%s/0/3", tempdir));

    CHECK_INTEQUAL(mkdir(path_0, S_IRWXU), 0);
    CHECK_INTEQUAL(chmod(path_0, S_IRWXU), 0);
    CHECK_INTEQUAL(access(path_0, F_OK), 0);
    CHECK_TRUE(posix_write_file(path_0_1, "x", 1, 0));
    CHECK_INTEQUAL(access(path_0_1, F_OK), 0);
    CHECK_INTEQUAL(mkdir(path_0_2, S_IRWXU), 0);
    CHECK_INTEQUAL(chmod(path_0_2, S_IRWXU), 0);
    CHECK_INTEQUAL(access(path_0_2, F_OK), 0);
    CHECK_INTEQUAL(symlink(path_0_3, path_0_3), 0);
    CHECK_TRUE(posix_rmdir_r(path_0));
    CHECK_INTEQUAL(access(path_0, F_OK), -1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_rmdir_r_symlink_to_dir)
{
    char path_0[1002], path_0_1[1004], path_0_2[1004], path_0_3[1004];
    char path_1[1002];
    ASSERT(strformat_check(path_0, sizeof(path_0), "%s/0", tempdir));
    ASSERT(strformat_check(path_0_1, sizeof(path_0_1), "%s/0/1", tempdir));
    ASSERT(strformat_check(path_0_2, sizeof(path_0_2), "%s/0/2", tempdir));
    ASSERT(strformat_check(path_0_3, sizeof(path_0_3), "%s/0/3", tempdir));
    ASSERT(strformat_check(path_1, sizeof(path_1), "%s/1", tempdir));

    CHECK_INTEQUAL(mkdir(path_0, S_IRWXU), 0);
    CHECK_INTEQUAL(chmod(path_0, S_IRWXU), 0);
    CHECK_INTEQUAL(access(path_0, F_OK), 0);
    CHECK_TRUE(posix_write_file(path_0_1, "x", 1, 0));
    CHECK_INTEQUAL(access(path_0_1, F_OK), 0);
    CHECK_INTEQUAL(mkdir(path_0_2, S_IRWXU), 0);
    CHECK_INTEQUAL(chmod(path_0_2, S_IRWXU), 0);
    CHECK_INTEQUAL(access(path_0_2, F_OK), 0);
    CHECK_INTEQUAL(mkdir(path_1, S_IRWXU), 0);
    CHECK_INTEQUAL(chmod(path_1, S_IRWXU), 0);
    CHECK_INTEQUAL(access(path_1, F_OK), 0);
    CHECK_INTEQUAL(symlink(path_1, path_0_3), 0);
    CHECK_INTEQUAL(access(path_0_3, F_OK), 0);
    CHECK_TRUE(posix_rmdir_r(path_0));
    CHECK_INTEQUAL(access(path_0, F_OK), -1);
    CHECK_INTEQUAL(access(path_1, F_OK), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_rmdir_r_unwritable_dir)
{
    char path_0[1002], path_0_1[1004], path_0_2[1004], path_0_3[1004];
    ASSERT(strformat_check(path_0, sizeof(path_0), "%s/0", tempdir));
    ASSERT(strformat_check(path_0_1, sizeof(path_0_1), "%s/0/1", tempdir));
    ASSERT(strformat_check(path_0_2, sizeof(path_0_2), "%s/0/2", tempdir));
    ASSERT(strformat_check(path_0_3, sizeof(path_0_3), "%s/0/3", tempdir));

    CHECK_INTEQUAL(mkdir(path_0, S_IRWXU), 0);
    CHECK_INTEQUAL(chmod(path_0, S_IRWXU), 0);
    CHECK_INTEQUAL(access(path_0, F_OK), 0);
    CHECK_TRUE(posix_write_file(path_0_1, "x", 1, 0));
    CHECK_INTEQUAL(access(path_0_1, F_OK), 0);
    CHECK_TRUE(posix_write_file(path_0_3, "x", 1, 0));
    CHECK_INTEQUAL(access(path_0_3, F_OK), 0);
    CHECK_INTEQUAL(chmod(path_0, S_IRUSR | S_IXUSR), 0);
    CHECK_FALSE(posix_write_file(path_0_2, "x", 1, 0));
    CHECK_INTEQUAL(access(path_0_2, F_OK), -1);
    CHECK_FALSE(posix_rmdir_r(path_0));
    CHECK_INTEQUAL(errno, EACCES);
    CHECK_INTEQUAL(access(path_0, F_OK), 0);
    /* Now make it writable and remove so it's not in the way. */
    CHECK_INTEQUAL(chmod(path_0, S_IRWXU), 0);
    CHECK_TRUE(posix_rmdir_r(path_0));
    CHECK_INTEQUAL(access(path_0, F_OK), -1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_rmdir_r_recursive_on_subdirs)
{
    char path_0[1002], path_0_1[1004], path_0_2[1004], path_0_3[1004];
    char path_0_2_a[1006], path_0_2_b[1006], path_0_2_b_x[1008];
    ASSERT(strformat_check(path_0, sizeof(path_0), "%s/0", tempdir));
    ASSERT(strformat_check(path_0_1, sizeof(path_0_1), "%s/0/1", tempdir));
    ASSERT(strformat_check(path_0_2, sizeof(path_0_2), "%s/0/2", tempdir));
    ASSERT(strformat_check(path_0_3, sizeof(path_0_3), "%s/0/3", tempdir));
    ASSERT(strformat_check(path_0_2_a, sizeof(path_0_2_a), "%s/0/2/a",
                           tempdir));
    ASSERT(strformat_check(path_0_2_b, sizeof(path_0_2_b), "%s/0/2/b",
                           tempdir));
    ASSERT(strformat_check(path_0_2_b_x, sizeof(path_0_2_b_x), "%s/0/2/b/x",
                           tempdir));

    CHECK_INTEQUAL(mkdir(path_0, S_IRWXU), 0);
    CHECK_INTEQUAL(chmod(path_0, S_IRWXU), 0);
    CHECK_INTEQUAL(access(path_0, F_OK), 0);
    CHECK_TRUE(posix_write_file(path_0_1, "x", 1, 0));
    CHECK_INTEQUAL(access(path_0_1, F_OK), 0);
    CHECK_INTEQUAL(mkdir(path_0_2, S_IRWXU), 0);
    CHECK_INTEQUAL(chmod(path_0_2, S_IRWXU), 0);
    CHECK_INTEQUAL(access(path_0_2, F_OK), 0);
    CHECK_TRUE(posix_write_file(path_0_2_a, "x", 1, 0));
    CHECK_INTEQUAL(access(path_0_2_a, F_OK), 0);
    CHECK_INTEQUAL(mkdir(path_0_2_b, S_IRWXU), 0);
    CHECK_INTEQUAL(chmod(path_0_2_b, S_IRWXU), 0);
    CHECK_INTEQUAL(access(path_0_2_b, F_OK), 0);
    CHECK_TRUE(posix_write_file(path_0_2_b_x, "x", 1, 0));
    CHECK_INTEQUAL(access(path_0_2_b_x, F_OK), 0);
    CHECK_TRUE(posix_write_file(path_0_3, "x", 1, 0));
    CHECK_INTEQUAL(access(path_0_3, F_OK), 0);
    CHECK_TRUE(posix_rmdir_r(path_0));
    CHECK_INTEQUAL(access(path_0, F_OK), -1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_rmdir_r_unwritable_subdir)
{
    char path_0[1002], path_0_1[1004], path_0_2[1004], path_0_3[1004];
    char path_0_2_a[1006], path_0_2_b[1006], path_0_2_b_x[1008];
    char path_0_2_b_y[1008];
    ASSERT(strformat_check(path_0, sizeof(path_0), "%s/0", tempdir));
    ASSERT(strformat_check(path_0_1, sizeof(path_0_1), "%s/0/1", tempdir));
    ASSERT(strformat_check(path_0_2, sizeof(path_0_2), "%s/0/2", tempdir));
    ASSERT(strformat_check(path_0_3, sizeof(path_0_3), "%s/0/3", tempdir));
    ASSERT(strformat_check(path_0_2_a, sizeof(path_0_2_a), "%s/0/2/a",
                           tempdir));
    ASSERT(strformat_check(path_0_2_b, sizeof(path_0_2_b), "%s/0/2/b",
                           tempdir));
    ASSERT(strformat_check(path_0_2_b_x, sizeof(path_0_2_b_x), "%s/0/2/b/x",
                           tempdir));
    ASSERT(strformat_check(path_0_2_b_y, sizeof(path_0_2_b_y), "%s/0/2/b/y",
                           tempdir));

    CHECK_INTEQUAL(mkdir(path_0, S_IRWXU), 0);
    CHECK_INTEQUAL(chmod(path_0, S_IRWXU), 0);
    CHECK_INTEQUAL(access(path_0, F_OK), 0);
    CHECK_TRUE(posix_write_file(path_0_1, "x", 1, 0));
    CHECK_INTEQUAL(access(path_0_1, F_OK), 0);
    CHECK_INTEQUAL(mkdir(path_0_2, S_IRWXU), 0);
    CHECK_INTEQUAL(chmod(path_0_2, S_IRWXU), 0);
    CHECK_INTEQUAL(access(path_0_2, F_OK), 0);
    CHECK_TRUE(posix_write_file(path_0_2_a, "x", 1, 0));
    CHECK_INTEQUAL(access(path_0_2_a, F_OK), 0);
    CHECK_INTEQUAL(mkdir(path_0_2_b, S_IRWXU), 0);
    CHECK_INTEQUAL(chmod(path_0_2_b, S_IRWXU), 0);
    CHECK_INTEQUAL(access(path_0_2_b, F_OK), 0);
    CHECK_TRUE(posix_write_file(path_0_2_b_x, "x", 1, 0));
    CHECK_INTEQUAL(access(path_0_2_b_x, F_OK), 0);
    CHECK_TRUE(posix_write_file(path_0_3, "x", 1, 0));
    CHECK_INTEQUAL(access(path_0_3, F_OK), 0);
    CHECK_INTEQUAL(chmod(path_0_2_b, S_IRUSR | S_IXUSR), 0);
    CHECK_FALSE(posix_write_file(path_0_2_b_y, "x", 1, 0));
    CHECK_INTEQUAL(access(path_0_2_b_y, F_OK), -1);
    CHECK_FALSE(posix_rmdir_r(path_0));
    CHECK_INTEQUAL(errno, EACCES);
    CHECK_INTEQUAL(access(path_0, F_OK), 0);
    /* Now make it writable and remove so it's not in the way. */
    CHECK_INTEQUAL(chmod(path_0_2_b, S_IRWXU), 0);
    CHECK_TRUE(posix_rmdir_r(path_0));
    CHECK_INTEQUAL(access(path_0, F_OK), -1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_rmdir_r_unwritable_dir_writable_subdir)
{
    char path_0[1002], path_0_1[1004], path_0_2[1004];
    char path_0_2_a[1006], path_0_2_b[1006], path_0_2_b_x[1008];
    ASSERT(strformat_check(path_0, sizeof(path_0), "%s/0", tempdir));
    ASSERT(strformat_check(path_0_1, sizeof(path_0_1), "%s/0/1", tempdir));
    ASSERT(strformat_check(path_0_2, sizeof(path_0_2), "%s/0/2", tempdir));
    ASSERT(strformat_check(path_0_2_a, sizeof(path_0_2_a), "%s/0/2/a",
                           tempdir));
    ASSERT(strformat_check(path_0_2_b, sizeof(path_0_2_b), "%s/0/2/b",
                           tempdir));
    ASSERT(strformat_check(path_0_2_b_x, sizeof(path_0_2_b_x), "%s/0/2/b/x",
                           tempdir));

    CHECK_INTEQUAL(mkdir(path_0, S_IRWXU), 0);
    CHECK_INTEQUAL(chmod(path_0, S_IRWXU), 0);
    CHECK_INTEQUAL(access(path_0, F_OK), 0);
    CHECK_INTEQUAL(mkdir(path_0_2, S_IRWXU), 0);
    CHECK_INTEQUAL(chmod(path_0_2, S_IRWXU), 0);
    CHECK_INTEQUAL(access(path_0_2, F_OK), 0);
    CHECK_TRUE(posix_write_file(path_0_2_a, "x", 1, 0));
    CHECK_INTEQUAL(access(path_0_2_a, F_OK), 0);
    CHECK_INTEQUAL(mkdir(path_0_2_b, S_IRWXU), 0);
    CHECK_INTEQUAL(chmod(path_0_2_b, S_IRWXU), 0);
    CHECK_INTEQUAL(access(path_0_2_b, F_OK), 0);
    CHECK_TRUE(posix_write_file(path_0_2_b_x, "x", 1, 0));
    CHECK_INTEQUAL(access(path_0_2_b_x, F_OK), 0);
    CHECK_INTEQUAL(chmod(path_0, S_IRUSR | S_IXUSR), 0);
    CHECK_FALSE(posix_write_file(path_0_1, "x", 1, 0));
    CHECK_INTEQUAL(access(path_0_1, F_OK), -1);
    CHECK_FALSE(posix_rmdir_r(path_0_2));
    CHECK_INTEQUAL(errno, EACCES);
    CHECK_INTEQUAL(access(path_0_2, F_OK), 0);
    CHECK_INTEQUAL(access(path_0_2_b, F_OK), -1);
    /* Now make it writable and remove so it's not in the way. */
    CHECK_INTEQUAL(chmod(path_0, S_IRWXU), 0);
    CHECK_TRUE(posix_rmdir_r(path_0));
    CHECK_INTEQUAL(access(path_0, F_OK), -1);

    return 1;
}

/*-----------------------------------------------------------------------*/

#ifdef CAN_CREATE_LONG_NESTED_PATHS

TEST(test_rmdir_r_long_nested_path)
{
    char path_0[1002];
    ASSERT(strformat_check(path_0, sizeof(path_0), "%s/0", tempdir));

    int dir_fd, cwd_fd;

    CHECK_INTEQUAL(mkdir(path_0, S_IRWXU), 0);
    CHECK_INTEQUAL(chmod(path_0, S_IRWXU), 0);
    CHECK_INTEQUAL(access(path_0, F_OK), 0);
    CHECK_TRUE((dir_fd = create_long_nested_path(path_0)) >= 0);
    CHECK_FALSE(posix_rmdir_r(path_0));
    CHECK_INTEQUAL(errno, ENAMETOOLONG);
    CHECK_INTEQUAL(access(path_0, F_OK), 0);

    CHECK_TRUE((cwd_fd = open(".", O_RDONLY)) >= 0);
    CHECK_INTEQUAL(fchdir(dir_fd), 0);
    const int unlink_result = unlink(
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    CHECK_INTEQUAL(fchdir(cwd_fd), 0);
    CHECK_INTEQUAL(unlink_result, 0);
    CHECK_INTEQUAL(close(dir_fd), 0);
    CHECK_INTEQUAL(close(cwd_fd), 0);
    CHECK_TRUE(posix_rmdir_r(path_0));

    return 1;
}

#endif  // CAN_CREATE_LONG_NESTED_PATHS

/*************************************************************************/
/*************************************************************************/
