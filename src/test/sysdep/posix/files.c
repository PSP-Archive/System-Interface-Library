/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/posix/files.c: Tests specific to the POSIX implementation
 * of the system-level file and directory access functions.
 */

#include "src/base.h"
#include "src/sysdep.h"
#include "src/sysdep/misc/ioqueue.h"
#include "src/sysdep/posix/files.h"
#include "src/sysdep/posix/fileutil.h"
#include "src/sysdep/posix/path_max.h"
#include "src/test/base.h"
#include "src/test/sysdep/posix/internal.h"
#include "src/thread.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Temporary directory to be cleaned up after each test. */
static char tempdir[1000];

/* Flag: Is the temporary directory on a case-sensitive filesystem? */
static uint8_t filesystem_is_case_sensitive;

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_posix_files)

/*-----------------------------------------------------------------------*/

TEST_INIT(init)
{
    CHECK_TRUE(sys_file_init());
    CHECK_TRUE(thread_init());

    /* Create a temporary directory into which we can safely write files. */
    CHECK_TRUE(posix_create_temporary_dir("test-posix-files",
                                          tempdir, sizeof(tempdir)));

    /* Check whether the filesystem is case-sensitive.  (In theory, we only
     * need to do this check once, but it doesn't add a significant amount
     * of overhead either way.) */
    char buf[1002];
    int fd;
    ASSERT(strformat_check(buf, sizeof(buf), "%s/a", tempdir));
    ASSERT((fd = open(buf, O_WRONLY | O_CREAT | O_EXCL, 0600)) >= 0);
    close(fd);
    ASSERT(strformat_check(buf, sizeof(buf), "%s/A", tempdir));
    fd = open(buf, O_RDONLY);
    if (fd >= 0) {
        filesystem_is_case_sensitive = 0;
        close(fd);
    } else {
        filesystem_is_case_sensitive = 1;
    }
    ASSERT(strformat_check(buf, sizeof(buf), "%s/a", tempdir));
    ASSERT(unlink(buf) == 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
    /* Delete the temporary directory. */
    if (!posix_rmdir_r(tempdir)) {
        FAIL("Failed to remove temporary directory %s", tempdir);
    }
    thread_cleanup();
    sys_file_cleanup();
    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_file_open_relative)
{
#ifdef SIL_PLATFORM_ANDROID
    /* No raw resource file access on Android. */
    SKIP("Can't run this test on Android.");
#endif

    /* Make sure to restore the initial working directory even on failure. */
    #undef FAIL_ACTION
    #define FAIL_ACTION  goto fail

    char path[PATH_MAX];
    int pathlen = sys_get_resource_path_prefix(path, sizeof(path));
    ASSERT(pathlen < (int)sizeof(path));
    ASSERT(strformat_check(path+pathlen, sizeof(path)-pathlen,
                           "testdata/DIR1/dir2"));

    char cwd[PATH_MAX];
    ASSERT(getcwd(cwd, sizeof(cwd)));
    ASSERT(chdir(path) == 0);

    SysFile *fh;
    CHECK_TRUE(fh = sys_file_open("File.Txt"));
    char buf[8];
    CHECK_INTEQUAL(sys_file_read(fh, buf, 8), 7);
    CHECK_MEMEQUAL(buf, "hello\0\1", 7);
    sys_file_close(fh);

    ASSERT(chdir(cwd) == 0);
    return 1;

  fail:
    ASSERT(chdir(cwd) == 0);
    return 0;

    #undef FAIL_ACTION
    #define FAIL_ACTION  return 0
}

/*-----------------------------------------------------------------------*/

TEST(test_file_open_pipe)
{
    char path[1005];
    ASSERT(strformat_check(path, sizeof(path), "%s/pipe", tempdir));
    ASSERT(mkfifo(path, S_IRUSR | S_IWUSR) == 0);

    int writer_thread;
    CHECK_TRUE(writer_thread = thread_create(posix_pipe_writer, path));
    const SysFile *sys_file_open_result = sys_file_open(path);
    thread_wait(writer_thread);
    CHECK_FALSE(sys_file_open_result);
    CHECK_INTEQUAL(sys_last_error(), SYSERR_UNKNOWN_ERROR);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_open_unreadable_file)
{
    char path[1002];

    ASSERT(strformat_check(path, sizeof(path), "%s/a", tempdir));
    int fd;
    ASSERT((fd = open(path, O_WRONLY | O_CREAT, 0)) >= 0);
    close(fd);

    CHECK_FALSE(sys_file_open(path));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_FILE_ACCESS_DENIED);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_open_unreadable_dir)
{
    char path[1002];

    ASSERT(strformat_check(path, sizeof(path), "%s/a", tempdir));
    int fd;
    ASSERT((fd = open(path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR)) >= 0);
    close(fd);
    ASSERT(chmod(tempdir, S_IWUSR | S_IXUSR) == 0);

    /* An exact filename match should succeed since we don't need to scan
     * the directory. */
    SysFile *fh;
    CHECK_TRUE(fh = sys_file_open(path));
    sys_file_close(fh);

    /* On case-sensitive filesystems, a case mismatch should cause the
     * open to fail since we can't read the directory to scan filenames. */
    ASSERT(strformat_check(path, sizeof(path), "%s/A", tempdir));
    if (filesystem_is_case_sensitive) {
        CHECK_FALSE(sys_file_open(path));
        CHECK_INTEQUAL(sys_last_error(), SYSERR_FILE_ACCESS_DENIED);
    } else {
        CHECK_TRUE(fh = sys_file_open(path));
        sys_file_close(fh);
    }

    /* Make the directory writable again so it can get cleaned up. */
    CHECK_INTEQUAL(chmod(tempdir, S_IRWXU), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_open_empty_path_component)
{
    char path[PATH_MAX];
    ASSERT(sys_get_resource_path_prefix(path, sizeof(path))
           < (int)sizeof(path));
    ASSERT(strformat_check(path+strlen(path), sizeof(path)-strlen(path),
                           "testdata//DIR1/dir2/File.Txt"));
    CHECK_FALSE(sys_file_open(path));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_FILE_NOT_FOUND);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_open_path_too_long)
{
    const char *testpath = "testdir/test.txt";
    char path[PATH_MAX+4];
    ASSERT(strformat(path, sizeof(path), "%s/%s", tempdir, testpath));
    ASSERT(posix_write_file(path, "foo", 3, 0));

    size_t i = strformat(path, sizeof(path), "%s/", tempdir);
    for (; i+2 <= sizeof(path) - (strlen(testpath)+1); i += 2) {
        path[i+0] = '.';
        path[i+1] = '/';
    }
    strcpy(path+i, testpath);  // Guaranteed safe by above loop.

    CHECK_FALSE(sys_file_open(path));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_BUFFER_OVERFLOW);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_open_path_component_too_long)
{
    char path[PATH_MAX+1];
    memset(path, 'a', sizeof(path)-1);
    path[sizeof(path)-1] = 0;

    CHECK_FALSE(sys_file_open(path));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_BUFFER_OVERFLOW);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_read_permfail_on_async)
{
    SysFile *fh;
    char path[PATH_MAX];
    ASSERT(sys_get_resource_path_prefix(path, sizeof(path))
           < (int)sizeof(path));
    ASSERT(strformat_check(path+strlen(path), sizeof(path)-strlen(path),
                           "testdata/dir1/dir2/file.txt"));
    ASSERT(fh = sys_file_open(path));

    char buf[8];
    memset(buf, 3, sizeof(buf));

    CHECK_TRUE(sys_file_seek(fh, 0, FILE_SEEK_SET));
    TEST_misc_ioqueue_permfail_next_read(1);
    CHECK_INTEQUAL(sys_file_read(fh, buf, 7), -1);
    CHECK_INTEQUAL(sys_last_error(), SYSERR_OUT_OF_MEMORY);
    CHECK_MEMEQUAL(buf, "\3\3\3\3\3\3\3\3", 8);

    TEST_misc_ioqueue_permfail_next_read(1);
    CHECK_INTEQUAL(sys_file_read_at(fh, buf, 7, 2), -1);
    CHECK_INTEQUAL(sys_last_error(), SYSERR_OUT_OF_MEMORY);
    CHECK_MEMEQUAL(buf, "\3\3\3\3\3\3\3\3", 8);

    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_read_tempfail_on_async)
{
    SysFile *fh;
    char path[PATH_MAX];
    ASSERT(sys_get_resource_path_prefix(path, sizeof(path))
           < (int)sizeof(path));
    ASSERT(strformat_check(path+strlen(path), sizeof(path)-strlen(path),
                           "testdata/dir1/dir2/file.txt"));
    ASSERT(fh = sys_file_open(path));

    char buf[8];
    memset(buf, 3, sizeof(buf));

    CHECK_TRUE(sys_file_seek(fh, 0, FILE_SEEK_SET));
    TEST_misc_ioqueue_tempfail_next_read(1);
    CHECK_INTEQUAL(sys_file_read(fh, buf, 7), -1);
    CHECK_INTEQUAL(sys_last_error(), SYSERR_TRANSIENT_FAILURE);
    CHECK_MEMEQUAL(buf, "\3\3\3\3\3\3\3\3", 8);

    TEST_misc_ioqueue_tempfail_next_read(1);
    CHECK_INTEQUAL(sys_file_read_at(fh, buf, 7, 2), -1);
    CHECK_INTEQUAL(sys_last_error(), SYSERR_TRANSIENT_FAILURE);
    CHECK_MEMEQUAL(buf, "\3\3\3\3\3\3\3\3", 8);

    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_read_io_error)
{
    SysFile *fh;
    char path[PATH_MAX];
    ASSERT(sys_get_resource_path_prefix(path, sizeof(path))
           < (int)sizeof(path));
    ASSERT(strformat_check(path+strlen(path), sizeof(path)-strlen(path),
                           "testdata/dir1/dir2/file.txt"));
    ASSERT(fh = sys_file_open(path));

    char buf[8];
    memset(buf, 3, sizeof(buf));

    CHECK_TRUE(sys_file_seek(fh, 0, FILE_SEEK_SET));
    TEST_misc_ioqueue_iofail_next_read(1);
    CHECK_INTEQUAL(sys_file_read(fh, buf, 7), -1);
    CHECK_INTEQUAL(sys_last_error(), SYSERR_UNKNOWN_ERROR);
    CHECK_MEMEQUAL(buf, "\3\3\3\3\3\3\3\3", 8);

    TEST_misc_ioqueue_iofail_next_read(1);
    CHECK_INTEQUAL(sys_file_read_at(fh, buf, 7, 2), -1);
    CHECK_INTEQUAL(sys_last_error(), SYSERR_UNKNOWN_ERROR);
    CHECK_MEMEQUAL(buf, "\3\3\3\3\3\3\3\3", 8);

    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* Identical to the platform-agnostic sys_files test of the same name,
 * except that we use ioqueue blocking to verify unequivocally that
 * sys_file_poll_async() returns the proper values. */

TEST(test_file_async)
{
    SysFile *fh;
    char path[PATH_MAX];
    ASSERT(sys_get_resource_path_prefix(path, sizeof(path))
           < (int)sizeof(path));
    ASSERT(strformat_check(path+strlen(path), sizeof(path)-strlen(path),
                           "testdata/dir1/dir2/file.txt"));
    ASSERT(fh = sys_file_open(path));

    char buf[8];
    int req;
    memset(buf, 3, sizeof(buf));
    TEST_misc_ioqueue_block_io_thread(1);
    CHECK_TRUE(req = sys_file_read_async(fh, buf, 7, 0, -1));
    CHECK_FALSE(sys_file_poll_async(req));
    TEST_misc_ioqueue_block_io_thread(0);
    while (!sys_file_poll_async(req)) {
        thread_yield();
    }
    CHECK_INTEQUAL(sys_file_wait_async(req), 7);
    CHECK_MEMEQUAL(buf, "hello\0\1\3", 8);

    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_read_when_async_full)
{
    SysFile *fh;
    char path[PATH_MAX];
    ASSERT(sys_get_resource_path_prefix(path, sizeof(path))
           < (int)sizeof(path));
    ASSERT(strformat_check(path+strlen(path), sizeof(path)-strlen(path),
                           "testdata/dir1/dir2/file.txt"));
    ASSERT(fh = sys_file_open(path));
    char buf[8], buf2[1];

    int reqlist[1000];
    int i;
    for (i = 0; i < lenof(reqlist); i++) {
        if (!(reqlist[i] = sys_file_read_async(fh, buf2, 1, 0, -1))) {
            break;
        }
    }
    if (i >= lenof(reqlist)) {
        FAIL("Unable to force sys_file_read_async() failure by running out"
             " of async read handles");
    }

    memset(buf, 3, sizeof(buf));
    CHECK_TRUE(sys_file_seek(fh, 0, FILE_SEEK_SET));
    CHECK_INTEQUAL(sys_file_read(fh, buf, 7), 7);
    CHECK_MEMEQUAL(buf, "hello\0\1\3", 8);

    for (i--; i >= 0; i--) {
        ASSERT(sys_file_wait_async(reqlist[i]) == 1);
    }

    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fileno)
{
    /* It'd be simpler to just open() one of our test data files, but we
     * can't do that on Android. (grumble) */
    char path[1002];
    ASSERT(strformat_check(path, sizeof(path), "%s/a", tempdir));
    int fd;
    ASSERT((fd = open(path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR)) >= 0);
    close(fd);

    SysFile *fh;
    char path2[PATH_MAX];
    ASSERT(sys_get_resource_path_prefix(path2, sizeof(path2))
           < (int)sizeof(path2));
    ASSERT(strformat_check(path2+strlen(path2), sizeof(path2)-strlen(path2),
                           "testdata/DIR1/dir2/File.Txt"));
    ASSERT(fh = sys_file_open(path2));
    CHECK_INTEQUAL(posix_fileno(fh), fd);

    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_path)
{
    SysFile *fh;
    char path[PATH_MAX];
    ASSERT(sys_get_resource_path_prefix(path, sizeof(path))
           < (int)sizeof(path));
    ASSERT(strformat_check(path+strlen(path), sizeof(path)-strlen(path),
                           "testdata/DIR1/dir2/File.Txt"));
    ASSERT(fh = sys_file_open(path));
    CHECK_STREQUAL(posix_file_path(fh), path);

    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_path_case_change)
{
    if (filesystem_is_case_sensitive) {
        SysFile *fh;
        char path[PATH_MAX];
        int prefix_len = sys_get_resource_path_prefix(path, sizeof(path));
        ASSERT(prefix_len < (int)sizeof(path));
        ASSERT(strformat_check(path+prefix_len, sizeof(path)-prefix_len,
                               "testdata/dir1/dir2/file.txt"));
        ASSERT(fh = sys_file_open(path));
        ASSERT(strformat_check(path+prefix_len, sizeof(path)-prefix_len,
                               "testdata/DIR1/dir2/File.Txt"));
        CHECK_STREQUAL(posix_file_path(fh), path);
        sys_file_close(fh);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_dir_open_posix_absolute)
{
    SysDir *d;

    CHECK_TRUE(d = sys_dir_open("/"));
    sys_dir_close(d);

    const char *tmpdir = posix_get_tmpdir();
    CHECK_TRUE(d = sys_dir_open(tmpdir));
    sys_dir_close(d);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_dir_read_pipe)
{
    char path[1005];
    ASSERT(strformat_check(path, sizeof(path), "%s/pipe", tempdir));
    ASSERT(mkfifo(path, S_IRUSR | S_IWUSR) == 0);

    SysDir *d;
    CHECK_TRUE(d = sys_dir_open(tempdir));
    int is_subdir;
    CHECK_FALSE(sys_dir_read(d, &is_subdir));
    sys_dir_close(d);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_dir_read_broken_symlink)
{
    char path[1002];
    ASSERT(strformat_check(path, sizeof(path), "%s/a", tempdir));
    ASSERT(symlink(path, path) == 0);
    struct stat st;
    ASSERT(stat(path, &st) == -1);
    ASSERT(errno == ELOOP);

    SysDir *d;
    CHECK_TRUE(d = sys_dir_open(tempdir));
    int is_subdir;
    CHECK_FALSE(sys_dir_read(d, &is_subdir));
    sys_dir_close(d);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_dir_read_path_too_long)
{
    const char *testdir = "testdir/dir1/dir2", *testfile = "file.txt";
    char path[PATH_MAX+4];
    ASSERT(strformat(path, sizeof(path),
                     "%s/%s/%s", tempdir, testdir, testfile));
    ASSERT(posix_write_file(path, "foo", 3, 0));

    const size_t testpath_len = strlen(testdir) + 1 + strlen(testfile);
    size_t i = strformat(path, sizeof(path), "%s/", tempdir);
    for (; i+2 <= sizeof(path) - (testpath_len+1); i += 2) {
        path[i+0] = '.';
        path[i+1] = '/';
    }
    strcpy(path+i, testdir);  // Guaranteed safe by above loop.

    SysDir *d;
    CHECK_TRUE(d = sys_dir_open(path));
    /* The directory contains file.txt, but we can't read it due to buffer
     * overflow. */
    int is_subdir;
    CHECK_FALSE(sys_dir_read(d, &is_subdir));
    sys_dir_close(d);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_dir_read_three_dots)
{
    char path[1004];
    ASSERT(strformat_check(path, sizeof(path), "%s/...", tempdir));
    int fd;
    ASSERT((fd = open(path, O_WRONLY|O_CREAT, 0666)) >= 0);

    SysDir *d;
    CHECK_TRUE(d = sys_dir_open(tempdir));
    int is_subdir;
    CHECK_STREQUAL(sys_dir_read(d, &is_subdir), "...");
    CHECK_FALSE(is_subdir);
    CHECK_STREQUAL(sys_dir_read(d, &is_subdir), NULL);
    sys_dir_close(d);

    return 1;
}

/*************************************************************************/
/*************************************************************************/
