/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/psp/files.c: Tests specific to the PSP implementation
 * of the system-level file and directory access functions.
 */

#include "src/base.h"
#include "src/sysdep.h"
#include "src/sysdep/psp/file-read.h"
#include "src/sysdep/psp/internal.h"
#include "src/sysdep/psp/thread.h"
#include "src/test/base.h"

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_psp_files)

/*-----------------------------------------------------------------------*/

TEST_INIT(init)
{
    CHECK_TRUE(sys_file_init());
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
    sys_file_cleanup();
    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_open_async)
{
    SysFile *fh;
    int req;

    fh = NULL;
    CHECK_TRUE(req = psp_file_open_async("testdata/DIR1/dir2/File.Txt", &fh));
    while (!sys_file_poll_async(req)) {
        sys_thread_yield();
    }
    CHECK_TRUE(sys_file_wait_async(req));
    CHECK_TRUE(fh);
    CHECK_INTEQUAL(sys_file_size(fh), 7);
    char buf[8];
    memset(buf, 3, sizeof(buf));
    CHECK_INTEQUAL(sys_file_read(fh, buf, 7), 7);
    CHECK_MEMEQUAL(buf, "hello\0\1\3", 8);
    sys_file_close(fh);

    CHECK_TRUE(req = psp_file_open_async("no_such_file", &fh));
    CHECK_FALSE(sys_file_wait_async(req));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_async_too_many_files)
{
    SysFile *fh[100];

    CHECK_TRUE(fh[0] = sys_file_open("testdata/test.txt"));
    int i;
    for (i = 1; i < lenof(fh); i++) {
        psp_errno = 0;
        if (!(fh[i] = sys_file_open("testdata/test.txt"))) {
            CHECK_INTEQUAL(sys_last_error(), SYSERR_OUT_OF_MEMORY);
            break;
        }
    }
    ASSERT(i < lenof(fh));

    psp_errno = 0;
    CHECK_FALSE(psp_file_open_async("testdata/test.txt", &fh[i]));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_OUT_OF_MEMORY);

    while (--i >= 0) {
        sys_file_close(fh[i]);
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_async_table_full)
{
    SysFile *fh;
    CHECK_TRUE(fh = sys_file_open("testdata/test.txt"));

    char buf[1];
    int req[MAX_ASYNC_READS+1];
    CHECK_TRUE(req[0] = sys_file_read_async(fh, buf, 1, 0, -1));
    int i;
    for (i = 1; i < lenof(req); i++) {
        if (!(req[i] = sys_file_read_async(fh, buf, 1, 0, -1))) {
            CHECK_INTEQUAL(sys_last_error(), SYSERR_FILE_ASYNC_FULL);
            break;
        }
    }
    ASSERT(i < lenof(req));

    SysFile *fh2;
    psp_errno = 0;
    CHECK_FALSE(psp_file_open_async("testdata/test.txt", &fh2));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_FILE_ASYNC_FULL);

    while (--i >= 0) {
        CHECK_INTEQUAL(sys_file_wait_async(req[--i]), 1);
    }
    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_async_invalid)
{
    SysFile *fh;

    psp_errno = 0;
    CHECK_FALSE(psp_file_open_async(NULL, &fh));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_INVALID_PARAMETER);

    psp_errno = 0;
    CHECK_FALSE(psp_file_open_async("testdata/test.txt", NULL));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_INVALID_PARAMETER);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_colon_in_path)
{
    const char *basepath = psp_executable_dir();
    ASSERT(strchr(basepath, ':'));

    char dirpath[256];
    ASSERT(strformat_check(dirpath, sizeof(dirpath), "%s%s", basepath,
                           basepath[strlen(basepath)-1]==':' ? "/" : ""));
    SysDir *dir;
    CHECK_TRUE(dir = sys_dir_open(dirpath));
    sys_dir_close(dir);

    char filepath[256];
    ASSERT(strformat_check(filepath, sizeof(filepath),
                           "%s/testdata/test.txt", basepath));
    SysFile *fh;
    CHECK_TRUE(fh = sys_file_open(filepath));
    sys_file_close(fh);

    int req;
    fh = NULL;
    CHECK_TRUE(req = psp_file_open_async(filepath, &fh));
    CHECK_TRUE(sys_file_wait_async(req));
    CHECK_TRUE(fh);
    char buf[6];
    CHECK_INTEQUAL(sys_file_size(fh), 5);
    memset(buf, 3, sizeof(buf));
    CHECK_INTEQUAL(sys_file_read(fh, buf, 5), 5);
    CHECK_MEMEQUAL(buf, "hello\3", 6);
    sys_file_close(fh);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_path_buffer_overflow)
{
    char path[260];
    memset(path, 'a', sizeof(path)-1);
    path[sizeof(path)-1] = '\0';

    psp_errno = 0;
    CHECK_FALSE(sys_file_open(path));
    CHECK_INTEQUAL(psp_errno, PSP_ENAMETOOLONG);

    psp_errno = 0;
    CHECK_FALSE(sys_dir_open(path));
    CHECK_INTEQUAL(psp_errno, PSP_ENAMETOOLONG);

    SysFile *fh;
    psp_errno = 0;
    CHECK_FALSE(psp_file_open_async(path, &fh));
    CHECK_INTEQUAL(psp_errno, PSP_ENAMETOOLONG);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_async_abort_with_locked_threads)
{
    SysFile *fh;
    CHECK_TRUE(fh = sys_file_open("testdata/dir1/dir2/file.txt"));

    char buf[8];
    int req;
    memset(buf, 3, sizeof(buf));

    psp_threads_lock();
    {
        CHECK_TRUE(req = sys_file_read_async(fh, buf, 7, 0, -1));
        CHECK_TRUE(sys_file_abort_async(req));
    }
    psp_threads_unlock();

    const int result = sys_file_wait_async(req);
    if (result == -1) {
        CHECK_INTEQUAL(sys_last_error(), SYSERR_FILE_ASYNC_ABORTED);
    } else {
        CHECK_INTEQUAL(result, 7);
        CHECK_MEMEQUAL(buf, "hello\0\1\3", 8);
    }

    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_read_async_deadline)
{
    SysFile *fh;
    CHECK_TRUE(fh = sys_file_open("testdata/test.txt"));

    int req;
    char buf[5];
    memset(buf, 3, sizeof(buf));
    CHECK_TRUE(req = sys_file_read_async(fh, buf, 5, 1, 0));
    CHECK_INTEQUAL(sys_file_wait_async(req), 4);
    CHECK_MEMEQUAL(buf, "ello\3", 5);

    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_read_async_deadline_priority)
{
    SysFile *fh;
    CHECK_TRUE(fh = sys_file_open("testdata/sound/long.dat"));

    int req[11];
    char buf[11*10];

    /* Check that an untimed request takes priority over timed requests
     * whose deadlines have not yet expired, and timed requests are
     * processed in deadline order.  To make sure the read thread doesn't
     * grab requests as soon as we pass them in, we freeze threads until
     * we finish submitting all the requests. */
    memset(buf, -1, sizeof(buf));
    psp_threads_lock();
    {
        for (int i = 0; i < 10; i++) {
            CHECK_TRUE(req[i] = sys_file_read_async(
                           fh, &buf[i*10], 10, i*10, (10-i)*0.1f));
        }
        CHECK_TRUE(req[10] = sys_file_read_async(fh, &buf[100], 10, 100, -1));
    }
    psp_threads_unlock();
    CHECK_INTEQUAL(sys_file_wait_async(req[10]), 10);
    CHECK_FALSE(sys_file_poll_async(req[0]));
    CHECK_INTEQUAL(sys_file_wait_async(req[0]), 10);
    for (int i = 1; i < 10; i++) {
        CHECK_TRUE(sys_file_poll_async(req[i]));
        CHECK_INTEQUAL(sys_file_wait_async(req[i]), 10);
    }
    for (int i = 0; i < lenof(buf); i++) {
        CHECK_INTEQUAL(buf[i], i%2==0 ? 0 : i/2);
    }

    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_dir_open_trailing_dot)
{
    char path[1000];
    ASSERT(strformat_check(path, sizeof(path), "%s/testdata/psp/.",
                           psp_executable_dir()));

    SysDir *dir;
    CHECK_TRUE(dir = sys_dir_open(path));
    int is_subdir;
    CHECK_STREQUAL(sys_dir_read(dir, &is_subdir), "ICON0.PNG");
    CHECK_FALSE(is_subdir);
    CHECK_STREQUAL(sys_dir_read(dir, &is_subdir), NULL);
    sys_dir_close(dir);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_pause)
{
    SysFile *fh;
    CHECK_TRUE(fh = sys_file_open("testdata/test.txt"));

    int req1, req2;
    char buf1[3], buf2[3];
    CHECK_TRUE(req1 = sys_file_read_async(fh, buf1, 3, 0, -1));
    CHECK_TRUE(req2 = sys_file_read_async(fh, buf2, 3, 2, -1));
    CHECK_INTEQUAL(sys_file_wait_async(req1), 3);

    char *name[32];  // Expand as needed if more files are added to testdata.
    int subdir[32];
    SysDir *dir;
    CHECK_TRUE(dir = sys_dir_open("testdata"));
    int num_entries = 0;
    const char *this_name;
    int this_subdir;
    while ((this_name = sys_dir_read(dir, &this_subdir)) != NULL) {
        ASSERT(num_entries < lenof(name));
        ASSERT(num_entries < lenof(subdir));
        ASSERT(name[num_entries] = mem_strdup(this_name, 0));
        subdir[num_entries] = this_subdir;
        num_entries++;
    }
    sys_dir_close(dir);
    CHECK_TRUE(dir = sys_dir_open("testdata"));
    for (int i = 0; i < num_entries-1; i++) {
        CHECK_STREQUAL(sys_dir_read(dir, &this_subdir), name[i]);
        CHECK_INTEQUAL(this_subdir, subdir[i]);
        mem_free(name[i]);
    }

    psp_file_pause();
    psp_file_unpause();

    CHECK_INTEQUAL(sys_file_wait_async(req2), 3);
    CHECK_MEMEQUAL(buf1, "hel", 3);
    CHECK_MEMEQUAL(buf2, "llo", 3);
    CHECK_STREQUAL(sys_dir_read(dir, &this_subdir), name[num_entries-1]);
    CHECK_INTEQUAL(this_subdir, subdir[num_entries-1]);
    mem_free(name[num_entries-1]);
    CHECK_STREQUAL(sys_dir_read(dir, &this_subdir), NULL);

    sys_dir_close(dir);
    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_read_submit_table_full)
{
    SysFile *fh;
    CHECK_TRUE(fh = sys_file_open("testdata/test.txt"));
    char path[1000];
    ASSERT(strformat_check(path, sizeof(path), "%s/testdata/test.txt",
                           psp_executable_dir()));
    int fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    CHECK_INTRANGE(fd, 0, INT_MAX);

    char buf[1];
    int req[MAX_ASYNC_READS+11];
    CHECK_TRUE(req[0] = psp_file_read_submit(fd, 0, 1, buf, 0, 0));
    int i;
    for (i = 1; i < lenof(req); i++) {
        if (!(req[i] = psp_file_read_submit(fd, 0, 1, buf, 0, 0))) {
            break;
        }
    }
    ASSERT(i < lenof(req));

    psp_errno = 0;
    CHECK_INTEQUAL(sys_file_read(fh, buf, 1), -1);
    CHECK_INTEQUAL(psp_errno, PSP_EIO);

    psp_errno = 0;
    CHECK_INTEQUAL(sys_file_read_at(fh, buf, 1, 0), -1);
    CHECK_INTEQUAL(psp_errno, PSP_EIO);

    psp_errno = 0;
    CHECK_FALSE(sys_file_read_async(fh, buf, 1, 0, -1));
    CHECK_INTEQUAL(psp_errno, PSP_EIO);

    while (--i >= 0) {
        CHECK_INTEQUAL(psp_file_read_wait(req[i]), 1);
    }
    sceIoClose(fd);
    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_read_invalid)
{
    char path[1000];
    ASSERT(strformat_check(path, sizeof(path), "%s/testdata/test.txt",
                           psp_executable_dir()));
    int fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    CHECK_INTRANGE(fd, 0, INT_MAX);

    char buf[1];
    int req;
    CHECK_TRUE(req = psp_file_read_submit(fd, 0, 1, buf, 0, 0));
    CHECK_INTEQUAL(psp_file_read_wait(req), 1);

    CHECK_INTEQUAL(psp_file_read_check(req), -1);
    CHECK_INTEQUAL(psp_file_read_check(INT_MAX), -1);
    CHECK_INTEQUAL(psp_file_read_wait(req), (int)PSP_EINVAL);
    CHECK_INTEQUAL(psp_file_read_wait(INT_MAX), (int)PSP_EINVAL);
    CHECK_INTEQUAL(psp_file_read_abort(req), 0);
    CHECK_INTEQUAL(psp_file_read_abort(INT_MAX), 0);

    sceIoClose(fd);
    return 1;
}

/*************************************************************************/
/*************************************************************************/
