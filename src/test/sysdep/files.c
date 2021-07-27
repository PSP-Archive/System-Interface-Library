/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/files.c: Tests for the system-level file and directory
 * access functions.
 */

#include "src/base.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/test/base.h"
#include "src/thread.h"

#if !defined(SIL_PLATFORM_PSP)
# define USING_IOQUEUE
# include "src/sysdep/misc/ioqueue.h"
#endif

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

/**
 * wrap_sys_file_open, wrap_sys_dir_open:  Call sys_file_open() or
 * sys_dir_open() respectively, converting the given path to an absolute
 * path by prepending the resource path prefix.
 */
static SysFile *wrap_sys_file_open(const char *path)
{
    char abs_path[10000];
    ASSERT(sys_get_resource_path_prefix(abs_path, sizeof(abs_path))
           < (int)sizeof(abs_path));
    ASSERT(strformat_check(abs_path+strlen(abs_path),
                           sizeof(abs_path)-strlen(abs_path), "%s", path));
    return sys_file_open(abs_path);
}

static SysDir *wrap_sys_dir_open(const char *path)
{
    char abs_path[10000];
    ASSERT(sys_get_resource_path_prefix(abs_path, sizeof(abs_path))
           < (int)sizeof(abs_path));
    ASSERT(strformat_check(abs_path+strlen(abs_path),
                           sizeof(abs_path)-strlen(abs_path), "%s", path));
    return sys_dir_open(abs_path);
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_sys_files)

/*-----------------------------------------------------------------------*/

TEST_INIT(init)
{
    CHECK_TRUE(thread_init());
    CHECK_TRUE(sys_file_init());
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
    sys_file_cleanup();
    thread_cleanup();
    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_file_open)
{
    SysFile *fh;

    CHECK_TRUE(fh = wrap_sys_file_open("testdata/DIR1/dir2/File.Txt"));
    sys_file_close(fh);
    CHECK_FALSE(wrap_sys_file_open("no_such_file"));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_open_case_insensitive)
{
    SysFile *fh;

    CHECK_TRUE(fh = wrap_sys_file_open("testdata/dir1/DIR2/fILE.tXT"));
    sys_file_close(fh);
    CHECK_TRUE(fh = wrap_sys_file_open("testdata/dir1/dir2/file.txt"));
    sys_file_close(fh);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_open_nonexistent_file)
{
    CHECK_FALSE(wrap_sys_file_open("testdata/no_such_file"));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_FILE_NOT_FOUND);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_open_case_insensitive_partial_match)
{
    CHECK_FALSE(wrap_sys_file_open("testdat/dir1/DIR2/fILE.tXT"));
    CHECK_FALSE(wrap_sys_file_open("testdata/dir1/DIR2/fILE.tX"));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_FILE_NOT_FOUND);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_open_nonexistent_dir)
{
    CHECK_FALSE(wrap_sys_file_open("testdata/no/such/file"));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_FILE_NOT_FOUND);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_open_memory_failure)
{
    SysFile *fh;

    /* CHECK_MEMORY_FAILURES() requires the call under test to fail at least
     * once, but the underlying sysdep implementation may not need to allocate
     * memory, so we check first and only use CHECK_MEMORY_FAILURES() if
     * appropriate. */
    TEST_mem_fail_after(0, 1, 0);
    const int need_mem_check =
        !(fh = wrap_sys_file_open("testdata/DIR1/dir2/File.Txt"));
    TEST_mem_fail_after(-1, 0, 0);
    if (need_mem_check) {
        CHECK_INTEQUAL(sys_last_error(), SYSERR_OUT_OF_MEMORY);
        CHECK_MEMORY_FAILURES(
            fh = wrap_sys_file_open("testdata/DIR1/dir2/File.Txt"));
    }
    sys_file_close(fh);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_open_fill_handle_table)
{
#ifdef SIL_PLATFORM_WINDOWS
    SKIP("No file handle limit on Windows.");
#endif

    static SysFile *fhlist[10000];
    unsigned int i;
    for (i = 0; i < lenof(fhlist)-1; i++) {
        fhlist[i] = wrap_sys_file_open("testdata/dir1/dir2/file.txt");
        if (!fhlist[i]) {
            break;
        }
    }
    if (i >= lenof(fhlist)-1) {
        FAIL("Unable to force sys_file_open() failure by running out of"
             " file handles");
    }
    CHECK_INTEQUAL(sys_last_error(), SYSERR_OUT_OF_MEMORY);

    for (i = 0; fhlist[i] != NULL; i++) {
        sys_file_close(fhlist[i]);
        fhlist[i] = NULL;
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_open_dir)
{
    CHECK_FALSE(wrap_sys_file_open("testdata/DIR1/dir2"));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_FILE_WRONG_TYPE);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_open_invalid)
{
    CHECK_FALSE(sys_file_open(NULL));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_INVALID_PARAMETER);

    CHECK_FALSE(sys_file_open(""));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_FILE_NOT_FOUND);

    sys_file_close(NULL);  // Just make sure it doesn't crash.

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_dup)
{
    SysFile *fh, *fh2;

    CHECK_TRUE(fh = wrap_sys_file_open("testdata/dir1/dir2/file.txt"));
    CHECK_TRUE(fh2 = sys_file_dup(fh));

    sys_file_close(fh);
    sys_file_close(fh2);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_dup_independent_positions)
{
    SysFile *fh, *fh2;
    CHECK_TRUE(fh = wrap_sys_file_open("testdata/dir1/dir2/file.txt"));
    CHECK_TRUE(fh2 = sys_file_dup(fh));

    char buf[4], buf2[5];
    CHECK_INTEQUAL(sys_file_read(fh, buf, 4), 4);
    CHECK_MEMEQUAL(buf, "hell", 4);
    CHECK_INTEQUAL(sys_file_read(fh2, buf2, 5), 5);
    CHECK_MEMEQUAL(buf2, "hello", 5);
    CHECK_TRUE(sys_file_seek(fh, 0, FILE_SEEK_SET));
    CHECK_INTEQUAL(sys_file_read(fh2, buf2, 4), 2);
    CHECK_MEMEQUAL(buf2, "\0\1llo", 5);

    sys_file_close(fh);
    sys_file_close(fh2);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_dup_and_close)
{
    SysFile *fh, *fh2;
    CHECK_TRUE(fh = wrap_sys_file_open("testdata/dir1/dir2/file.txt"));
    CHECK_TRUE(fh2 = sys_file_dup(fh));

    char buf[4];
    sys_file_close(fh);
    CHECK_INTEQUAL(sys_file_read(fh2, buf, 4), 4);
    CHECK_MEMEQUAL(buf, "hell", 4);
    sys_file_close(fh2);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_dup_memory_failure)
{
    SysFile *fh, *fh2;
    CHECK_TRUE(fh = wrap_sys_file_open("testdata/dir1/dir2/file.txt"));

    TEST_mem_fail_after(0, 1, 0);
    const int need_mem_check = !(fh2 = sys_file_dup(fh));
    TEST_mem_fail_after(-1, 0, 0);
    if (need_mem_check) {
        CHECK_INTEQUAL(sys_last_error(), SYSERR_OUT_OF_MEMORY);
        CHECK_MEMORY_FAILURES(fh2 = sys_file_dup(fh));
    }
    sys_file_close(fh2);

    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_dup_fill_handle_table)
{
#ifdef SIL_PLATFORM_WINDOWS
    SKIP("No file handle limit on Windows.");
#endif

    SysFile *fh;
    CHECK_TRUE(fh = wrap_sys_file_open("testdata/dir1/dir2/file.txt"));
    CHECK_INTEQUAL(sys_file_tell(fh), 0);

    static SysFile *fhlist[10000];
    unsigned int i;
    for (i = 0; i < lenof(fhlist)-1; i++) {
        fhlist[i] = sys_file_dup(fh);
        if (!fhlist[i]) {
            break;
        }
    }
    if (i >= lenof(fhlist)-1) {
        FAIL("Unable to force sys_file_dup() failure by running out of"
             " file handles");
    }
    CHECK_INTEQUAL(sys_last_error(), SYSERR_OUT_OF_MEMORY);

    for (i = 0; fhlist[i] != NULL; i++) {
        sys_file_close(fhlist[i]);
        fhlist[i] = NULL;
    }

    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_dup_invalid)
{
    CHECK_FALSE(sys_file_dup(NULL));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_INVALID_PARAMETER);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_pos)
{
    SysFile *fh;
    CHECK_TRUE(fh = wrap_sys_file_open("testdata/dir1/dir2/file.txt"));

    CHECK_INTEQUAL(sys_file_size(fh), 7);
    CHECK_INTEQUAL(sys_file_tell(fh), 0);

    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_seek_set)
{
    SysFile *fh;
    CHECK_TRUE(fh = wrap_sys_file_open("testdata/dir1/dir2/file.txt"));

    CHECK_TRUE(sys_file_seek(fh, 3, FILE_SEEK_SET));
    CHECK_INTEQUAL(sys_file_tell(fh), 3);
    /* Also make sure we can't seek before the beginning of the file.
     * (tell() behavior is undefined for seeking past the end of the
     * file, and it doesn't actually matter as long as reads return zero
     * bytes, which we check later.) */
    CHECK_TRUE(sys_file_seek(fh, -1, FILE_SEEK_SET));
    CHECK_INTEQUAL(sys_file_tell(fh), 0);

    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_seek_cur)
{
    SysFile *fh;
    CHECK_TRUE(fh = wrap_sys_file_open("testdata/dir1/dir2/file.txt"));

    CHECK_TRUE(sys_file_seek(fh, 3, FILE_SEEK_SET));
    CHECK_INTEQUAL(sys_file_tell(fh), 3);
    CHECK_TRUE(sys_file_seek(fh, -2, FILE_SEEK_CUR));
    CHECK_INTEQUAL(sys_file_tell(fh), 1);
    CHECK_TRUE(sys_file_seek(fh, -2, FILE_SEEK_CUR));
    CHECK_INTEQUAL(sys_file_tell(fh), 0);
    CHECK_TRUE(sys_file_seek(fh, 2, FILE_SEEK_CUR));
    CHECK_INTEQUAL(sys_file_tell(fh), 2);

    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_seek_end)
{
    SysFile *fh;
    CHECK_TRUE(fh = wrap_sys_file_open("testdata/dir1/dir2/file.txt"));

    CHECK_TRUE(sys_file_seek(fh, 0, FILE_SEEK_END));
    CHECK_INTEQUAL(sys_file_tell(fh), 7);
    CHECK_TRUE(sys_file_seek(fh, -1, FILE_SEEK_END));
    CHECK_INTEQUAL(sys_file_tell(fh), 6);
    CHECK_TRUE(sys_file_seek(fh, -8, FILE_SEEK_END));
    CHECK_INTEQUAL(sys_file_tell(fh), 0);

    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_pos_invalid)
{
    SysFile *fh;
    CHECK_TRUE(fh = wrap_sys_file_open("testdata/dir1/dir2/file.txt"));

    CHECK_FALSE(sys_file_size(NULL));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_INVALID_PARAMETER);

    CHECK_FALSE(sys_file_seek(NULL, 0, FILE_SEEK_SET));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_INVALID_PARAMETER);

    CHECK_FALSE(sys_file_seek(fh, 0, FILE_SEEK_END + 1));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_INVALID_PARAMETER);

    CHECK_FALSE(sys_file_tell(NULL));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_INVALID_PARAMETER);

    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_read_consecutive)
{
    SysFile *fh;
    CHECK_TRUE(fh = wrap_sys_file_open("testdata/dir1/dir2/file.txt"));

    char buf[8];
    memset(buf, 3, sizeof(buf));
    CHECK_TRUE(sys_file_seek(fh, 0, FILE_SEEK_SET));
    CHECK_INTEQUAL(sys_file_read(fh, buf, 3), 3);
    CHECK_MEMEQUAL(buf, "hel\3\3\3\3\3", 8);
    CHECK_INTEQUAL(sys_file_tell(fh), 3);
    CHECK_INTEQUAL(sys_file_read(fh, buf, 3), 3);
    CHECK_MEMEQUAL(buf, "lo\0\3\3\3\3\3", 8);
    CHECK_INTEQUAL(sys_file_tell(fh), 6);

    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_read_after_seek)
{
    SysFile *fh;
    CHECK_TRUE(fh = wrap_sys_file_open("testdata/dir1/dir2/file.txt"));

    char buf[8];
    memset(buf, 3, sizeof(buf));
    CHECK_TRUE(sys_file_seek(fh, 4, FILE_SEEK_SET));
    CHECK_INTEQUAL(sys_file_read(fh, buf, 3), 3);
    CHECK_MEMEQUAL(buf, "o\0\1\3\3\3\3\3", 8);

    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_read_from_eof)
{
    SysFile *fh;
    CHECK_TRUE(fh = wrap_sys_file_open("testdata/dir1/dir2/file.txt"));

    char buf[8];
    memset(buf, 3, sizeof(buf));
    CHECK_TRUE(sys_file_seek(fh, 0, FILE_SEEK_END));
    CHECK_INTEQUAL(sys_file_read(fh, buf, 8), 0);
    CHECK_MEMEQUAL(buf, "\3\3\3\3\3\3\3\3", 8);
    CHECK_TRUE(sys_file_seek(fh, 10, FILE_SEEK_SET));
    CHECK_INTEQUAL(sys_file_read(fh, buf, 8), 0);
    CHECK_MEMEQUAL(buf, "\3\3\3\3\3\3\3\3", 8);

    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_read_zero_size)
{
    SysFile *fh;
    CHECK_TRUE(fh = wrap_sys_file_open("testdata/dir1/dir2/file.txt"));

    char buf[8];
    memset(buf, 3, sizeof(buf));
    CHECK_TRUE(sys_file_seek(fh, 0, FILE_SEEK_SET));
    CHECK_INTEQUAL(sys_file_read(fh, buf, 0), 0);
    CHECK_MEMEQUAL(buf, "\3\3\3\3\3\3\3\3", 8);

    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_read_memory_failure)
{
    SysFile *fh;
    CHECK_TRUE(fh = wrap_sys_file_open("testdata/dir1/dir2/file.txt"));

    char buf[8];
    memset(buf, 3, sizeof(buf));
    CHECK_TRUE(sys_file_seek(fh, 0, FILE_SEEK_SET));
    TEST_mem_fail_after(0, 1, 0);
    const int need_mem_check = !(sys_file_read(fh, buf, 7) == 7);
    TEST_mem_fail_after(-1, 0, 0);
    if (need_mem_check) {
        CHECK_MEMORY_FAILURES(sys_file_read(fh, buf, 7) == 7);
    }
    CHECK_MEMEQUAL(buf, "hello\0\1\3", 8);

    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_read_invalid)
{
    SysFile *fh;
    CHECK_TRUE(fh = wrap_sys_file_open("testdata/dir1/dir2/file.txt"));

    char buf[8];
    CHECK_INTEQUAL(sys_file_read(NULL, buf, 1), -1);
    CHECK_INTEQUAL(sys_file_read(fh, NULL, 1), -1);
    CHECK_INTEQUAL(sys_file_read(fh, buf, -1), -1);

    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_read_at)
{
    SysFile *fh;
    CHECK_TRUE(fh = wrap_sys_file_open("testdata/dir1/dir2/file.txt"));

    char buf[8];
    memset(buf, 3, sizeof(buf));
    CHECK_TRUE(sys_file_seek(fh, 0, FILE_SEEK_SET));
    CHECK_INTEQUAL(sys_file_read_at(fh, buf, 7, 2), 5);
    CHECK_MEMEQUAL(buf, "llo\0\1\3\3\3", 8);
    CHECK_INTEQUAL(sys_file_tell(fh), 0);

    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_read_at_past_eof)
{
    SysFile *fh;
    CHECK_TRUE(fh = wrap_sys_file_open("testdata/dir1/dir2/file.txt"));

    char buf[8];
    memset(buf, 3, sizeof(buf));
    CHECK_INTEQUAL(sys_file_read_at(fh, buf, 8, 8), 0);
    CHECK_MEMEQUAL(buf, "\3\3\3\3\3\3\3\3", 8);

    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_read_at_zero_size)
{
    SysFile *fh;
    CHECK_TRUE(fh = wrap_sys_file_open("testdata/dir1/dir2/file.txt"));

    char buf[8];
    memset(buf, 3, sizeof(buf));
    CHECK_INTEQUAL(sys_file_read_at(fh, buf, 0, 2), 0);
    CHECK_MEMEQUAL(buf, "\3\3\3\3\3\3\3\3", 8);

    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_read_at_memory_failure)
{
    SysFile *fh;
    CHECK_TRUE(fh = wrap_sys_file_open("testdata/dir1/dir2/file.txt"));

    char buf[8];
    memset(buf, 3, sizeof(buf));
    CHECK_TRUE(sys_file_seek(fh, 0, FILE_SEEK_SET));
    TEST_mem_fail_after(0, 1, 0);
    const int need_mem_check = !(sys_file_read_at(fh, buf, 7, 2) == 5);
    TEST_mem_fail_after(-1, 0, 0);
    if (need_mem_check) {
        CHECK_MEMORY_FAILURES(sys_file_read_at(fh, buf, 7, 2) == 5);
    }
    CHECK_MEMEQUAL(buf, "llo\0\1\3\3\3", 8);

    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_read_at_invalid)
{
    SysFile *fh;
    CHECK_TRUE(fh = wrap_sys_file_open("testdata/dir1/dir2/file.txt"));

    char buf[8];
    CHECK_INTEQUAL(sys_file_read_at(NULL, buf, 1, 1), -1);
    CHECK_INTEQUAL(sys_file_read_at(fh, NULL, 1, 1), -1);
    CHECK_INTEQUAL(sys_file_read_at(fh, buf, -1, 1), -1);
    CHECK_INTEQUAL(sys_file_read_at(fh, buf, 1, -1), -1);

    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_async)
{
    SysFile *fh;
    CHECK_TRUE(fh = wrap_sys_file_open("testdata/dir1/dir2/file.txt"));

    char buf[8];
    int req;
    memset(buf, 3, sizeof(buf));
    CHECK_TRUE(req = sys_file_read_async(fh, buf, 7, 0, -1));
    while (!sys_file_poll_async(req)) {
        thread_yield();
    }
    CHECK_TRUE(sys_file_poll_async(req));  // Should stay true.
    CHECK_INTEQUAL(sys_file_wait_async(req), 7);
    CHECK_MEMEQUAL(buf, "hello\0\1\3", 8);

    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_async_read_past_eof)
{
    SysFile *fh;
    CHECK_TRUE(fh = wrap_sys_file_open("testdata/dir1/dir2/file.txt"));

    char buf[8];
    int req;
    memset(buf, 3, sizeof(buf));
    CHECK_TRUE(req = sys_file_read_async(fh, buf, 8, 0, -1));
    CHECK_INTEQUAL(sys_file_wait_async(req), 7);
    CHECK_MEMEQUAL(buf, "hello\0\1\3", 8);

    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_async_read_position)
{
    SysFile *fh;
    CHECK_TRUE(fh = wrap_sys_file_open("testdata/dir1/dir2/file.txt"));

    char buf[8];
    int req;
    memset(buf, 3, sizeof(buf));
    CHECK_TRUE(req = sys_file_read_async(fh, buf, 7, 3, -1));
    CHECK_INTEQUAL(sys_file_wait_async(req), 4);
    CHECK_MEMEQUAL(buf, "lo\0\1\3\3\3\3", 8);

    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_async_and_sync_position)
{
    SysFile *fh;
    CHECK_TRUE(fh = wrap_sys_file_open("testdata/dir1/dir2/file.txt"));

    char buf[8];
    int req;
    memset(buf, 3, sizeof(buf));
    CHECK_TRUE(sys_file_seek(fh, 1, FILE_SEEK_SET));
    CHECK_INTEQUAL(sys_file_tell(fh), 1);
    CHECK_TRUE(req = sys_file_read_async(fh, buf, 3, 0, -1));
    CHECK_INTEQUAL(sys_file_wait_async(req), 3);
    CHECK_MEMEQUAL(buf, "hel\3\3\3\3\3", 8);
    CHECK_INTEQUAL(sys_file_tell(fh), 1);

    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_async_abort)
{
    SysFile *fh;
    CHECK_TRUE(fh = wrap_sys_file_open("testdata/dir1/dir2/file.txt"));

    char buf[8];
    int req;
    memset(buf, 3, sizeof(buf));
    CHECK_TRUE(req = sys_file_read_async(fh, buf, 7, 0, -1));
    CHECK_TRUE(sys_file_abort_async(req));
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

TEST(test_file_async_parallel)
{
    SysFile *fh;
    CHECK_TRUE(fh = wrap_sys_file_open("testdata/dir1/dir2/file.txt"));

    char buf[8], buf2[8];
    int req, req2;
    memset(buf, 3, sizeof(buf));
    memset(buf2, 3, sizeof(buf2));
    CHECK_TRUE(req = sys_file_read_async(fh, buf, 3, 0, -1));
    CHECK_TRUE(req2 = sys_file_read_async(fh, buf2, 3, 2, -1));
    CHECK_INTEQUAL(sys_file_wait_async(req), 3);
    CHECK_INTEQUAL(sys_file_wait_async(req2), 3);
    CHECK_MEMEQUAL(buf, "hel\3\3\3\3\3", 8);
    CHECK_MEMEQUAL(buf2, "llo\3\3\3\3\3", 8);

    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_async_parallel_max)
{
    SysFile *fh;
    CHECK_TRUE(fh = wrap_sys_file_open("testdata/dir1/dir2/file.txt"));

    char reqlist_buf[MAX_ASYNC_READS];
    int reqlist[MAX_ASYNC_READS];
    memset(reqlist_buf, 3, sizeof(reqlist_buf));
    for (unsigned int i = 0; i < MAX_ASYNC_READS; i++) {
        if (!(reqlist[i] = sys_file_read_async(fh, &reqlist_buf[i], 1, i%7,
                                               -1))) {
            FAIL("sys_file_read_async() failed for simultaneous read #%u", i);
        }
    }
    for (unsigned int i = 0; i < MAX_ASYNC_READS; i++) {
        const int result = sys_file_wait_async(reqlist[i]);
        if (result != 1) {
            FAIL("sys_file_wait_async() failed for simultaneous read #%u"
                 " (expected 1, got %d)", i, result);
        }
    }
    for (unsigned int i = 0; i < MAX_ASYNC_READS; i++) {
        if (reqlist_buf[i] != "hello\0\1"[i%7]) {
            FAIL("Simultaneous read #%u returned wrong byte (expected %d,"
                 " got %d)", i, "hello\0\1"[i%7], reqlist_buf[i]);
        }
    }

    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_async_parallel_overflow)
{
    SysFile *fh;
    CHECK_TRUE(fh = wrap_sys_file_open("testdata/dir1/dir2/file.txt"));

    char reqlist_buf[1000];
    int reqlist[1000];
    memset(reqlist_buf, 3, sizeof(reqlist_buf));

    int i;
    for (i = 0; i < lenof(reqlist)-1; i++) {
        if (!(reqlist[i] = sys_file_read_async(fh, &reqlist_buf[i], 1, i%7,
                                               -1))) {
            break;
        }
    }
    if (i >= lenof(reqlist)-1) {
        /* As with test_file_dup_fill_handle_table(), make this a hard
         * failure unless and until there are systems with no preset limit. */
        FAIL("Unable to force sys_file_read_async() failure by running out"
             " of async read handles");
    }
    CHECK_INTEQUAL(sys_last_error(), SYSERR_FILE_ASYNC_FULL);

    for (i = 0; reqlist[i] != 0; i++) {
        const int result = sys_file_wait_async(reqlist[i]);
        if (result != 1) {
            FAIL("sys_file_wait_async() failed for simultaneous read #%d"
                 " (expected 1, got %d)", i, result);
        }
        if (reqlist_buf[i] != "hello\0\1"[i%7]) {
            FAIL("Simultaneous read #%d returned wrong byte (expected %d,"
                 " got %d)", i, "hello\0\1"[i%7], reqlist_buf[i]);
        }
    }

    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

#ifdef USING_IOQUEUE
TEST(test_file_async_deadline)
{
    SysFile *fh;
    CHECK_TRUE(fh = wrap_sys_file_open("testdata/dir1/dir2/file.txt"));

    char buf[8], buf2[8];
    int req, req2;
    memset(buf, 3, sizeof(buf));
    memset(buf2, 3, sizeof(buf2));
    TEST_misc_ioqueue_block_io_thread(1);
    TEST_misc_ioqueue_unblock_on_wait(1);
    CHECK_TRUE(req = sys_file_read_async(fh, buf, 3, 0, 1));
    /* This request should be prioritized since its deadline is immediate. */
    CHECK_TRUE(req2 = sys_file_read_async(fh, buf2, 3, 2, 0));
    CHECK_INTEQUAL(sys_file_wait_async(req2), 3);
    CHECK_MEMEQUAL(buf2, "llo\3\3\3\3\3", 8);
    /* The first requests should have been deferred. */
    CHECK_FALSE(ioq_poll(req));
    CHECK_INTEQUAL(sys_file_wait_async(req), 3);
    CHECK_MEMEQUAL(buf, "hel\3\3\3\3\3", 8);
    TEST_misc_ioqueue_block_io_thread(0);
    TEST_misc_ioqueue_unblock_on_wait(0);

    sys_file_close(fh);
    return 1;
}
#endif

/*-----------------------------------------------------------------------*/

TEST(test_file_async_close_while_reading)
{
    SysFile *fh;
    CHECK_TRUE(fh = wrap_sys_file_open("testdata/dir1/dir2/file.txt"));

    char buf[8];
    int req;
    CHECK_TRUE(sys_file_seek(fh, 0, FILE_SEEK_SET));
    CHECK_TRUE(req = sys_file_read_async(fh, buf, 3, 0, -1));
    sys_file_close(fh);
    CHECK_INTEQUAL(sys_file_wait_async(req), -1);
    CHECK_INTEQUAL(sys_last_error(), SYSERR_FILE_ASYNC_ABORTED);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_async_memory_failure)
{
    SysFile *fh;
    CHECK_TRUE(fh = wrap_sys_file_open("testdata/dir1/dir2/file.txt"));

    char buf[8];
    int req;
    memset(buf, 3, sizeof(buf));
    TEST_mem_fail_after(0, 1, 0);
    const int need_mem_check = !(req = sys_file_read_async(fh, buf, 7, 0, -1))
                            || !(sys_file_wait_async(req) == 7);
    TEST_mem_fail_after(-1, 0, 0);
    if (need_mem_check) {
        CHECK_MEMORY_FAILURES((req = sys_file_read_async(fh, buf, 7, 0, -1))
                              && sys_file_wait_async(req) == 7);
    }
    CHECK_MEMEQUAL(buf, "hello\0\1\3", 8);

    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

#ifdef USING_IOQUEUE
TEST(test_file_async_io_tempfail)
{
    SysFile *fh;
    CHECK_TRUE(fh = wrap_sys_file_open("testdata/dir1/dir2/file.txt"));

    char buf[8];
    TEST_misc_ioqueue_tempfail_next_read(1);
    CHECK_FALSE(sys_file_read_async(fh, buf, 7, 0, -1));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_TRANSIENT_FAILURE);

    sys_file_close(fh);
    return 1;
}
#endif

/*-----------------------------------------------------------------------*/

TEST(test_file_async_invalid)
{
    SysFile *fh;
    CHECK_TRUE(fh = wrap_sys_file_open("testdata/dir1/dir2/file.txt"));

    char buf[8];
    int req;

    CHECK_FALSE(sys_file_read_async(NULL, buf, 1, 0, -1));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_INVALID_PARAMETER);

    //FIXME: could have false success here, need to reset sys_last_error
    CHECK_FALSE(sys_file_read_async(fh, NULL, 1, 0, -1));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_INVALID_PARAMETER);

    CHECK_FALSE(sys_file_read_async(fh, buf, -1, 0, -1));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_INVALID_PARAMETER);

    CHECK_FALSE(sys_file_read_async(fh, buf, 1, -1, -1));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_INVALID_PARAMETER);

    CHECK_TRUE(sys_file_poll_async(0));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_FILE_ASYNC_INVALID);

    CHECK_TRUE(sys_file_poll_async(INT_MAX));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_FILE_ASYNC_INVALID);

    CHECK_INTEQUAL(sys_file_wait_async(0), -1);
    CHECK_INTEQUAL(sys_last_error(), SYSERR_FILE_ASYNC_INVALID);

    CHECK_INTEQUAL(sys_file_wait_async(INT_MAX), -1);
    CHECK_INTEQUAL(sys_last_error(), SYSERR_FILE_ASYNC_INVALID);

    CHECK_FALSE(sys_file_abort_async(0));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_FILE_ASYNC_INVALID);

    CHECK_FALSE(sys_file_abort_async(INT_MAX));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_FILE_ASYNC_INVALID);

    memset(buf, 3, sizeof(buf));
    CHECK_TRUE(req = sys_file_read_async(fh, buf, 7, 0, -1));
    CHECK_INTEQUAL(sys_file_wait_async(req), 7);
    CHECK_MEMEQUAL(buf, "hello\0\1\3", 8);
    CHECK_TRUE(sys_file_poll_async(req));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_FILE_ASYNC_INVALID);
    CHECK_INTEQUAL(sys_file_wait_async(req), -1);
    CHECK_INTEQUAL(sys_last_error(), SYSERR_FILE_ASYNC_INVALID);
    CHECK_FALSE(sys_file_abort_async(req));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_FILE_ASYNC_INVALID);

    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_sync_read_while_async_full)
{
    SysFile *fh;
    CHECK_TRUE(fh = wrap_sys_file_open("testdata/dir1/dir2/file.txt"));

    char reqlist_buf[1000];
    int reqlist[1000];
    memset(reqlist_buf, 3, sizeof(reqlist_buf));

    int i;
    for (i = 0; i < lenof(reqlist)-1; i++) {
        if (!(reqlist[i] = sys_file_read_async(fh, &reqlist_buf[i], 1, i%7,
                                               -1))) {
            break;
        }
    }
    if (i >= lenof(reqlist)-1) {
        FAIL("Unable to force sys_file_read_async() failure by running out"
             " of async read handles");
    }

    char buf[8];
    memset(buf, 3, sizeof(buf));
    CHECK_INTEQUAL(sys_file_read(fh, buf, 3), 3);
    CHECK_MEMEQUAL(buf, "hel\3\3\3\3\3", 8);
    CHECK_INTEQUAL(sys_file_read_at(fh, buf, 3, 2), 3);
    CHECK_MEMEQUAL(buf, "llo\3\3\3\3\3", 8);

    for (i = 0; reqlist[i] != 0; i++) {
        const int result = sys_file_wait_async(reqlist[i]);
        if (result != 1) {
            FAIL("sys_file_wait_async() failed for simultaneous read #%d"
                 " (expected 1, got %d)", i, result);
        }
        if (reqlist_buf[i] != "hello\0\1"[i%7]) {
            FAIL("Simultaneous read #%d returned wrong byte (expected %d,"
                 " got %d)", i, "hello\0\1"[i%7], reqlist_buf[i]);
        }
    }

    sys_file_close(fh);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_dir_open)
{
    SysDir *d;

    CHECK_TRUE(d = wrap_sys_dir_open("testdata/DIR1"));
    sys_dir_close(d);
    CHECK_TRUE(d = wrap_sys_dir_open("testdata/DIR1/dir2"));
    sys_dir_close(d);
    CHECK_TRUE(d = wrap_sys_dir_open("testdata/dir1"));
    sys_dir_close(d);
    CHECK_TRUE(d = wrap_sys_dir_open("testdata/dir1/DIR2"));
    sys_dir_close(d);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_dir_open_memory_failure)
{
    SysDir *d;

    TEST_mem_fail_after(0, 1, 0);
    const int need_mem_check = !(d = wrap_sys_dir_open("testdata/DIR1"));
    TEST_mem_fail_after(-1, 0, 0);
    if (need_mem_check) {
        CHECK_INTEQUAL(sys_last_error(), SYSERR_OUT_OF_MEMORY);
        CHECK_MEMORY_FAILURES(d = wrap_sys_dir_open("testdata/DIR1"));
    }
    sys_dir_close(d);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_dir_open_nonexistent)
{

    CHECK_FALSE(wrap_sys_dir_open("no_such_dir"));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_FILE_NOT_FOUND);

    CHECK_FALSE(wrap_sys_dir_open("no_such_dir/testdata/DIR1"));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_FILE_NOT_FOUND);

    CHECK_FALSE(wrap_sys_dir_open("testdata/DIR1/no_such_dir"));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_FILE_NOT_FOUND);

    CHECK_FALSE(wrap_sys_dir_open("testdata/DIR1/dir2/no_such_dir"));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_FILE_NOT_FOUND);

    CHECK_FALSE(wrap_sys_dir_open("testdata/no/such/dir"));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_FILE_NOT_FOUND);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_dir_open_file)
{
    CHECK_FALSE(wrap_sys_dir_open("testdata/DIR1/dir2/File.Txt"));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_FILE_WRONG_TYPE);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_dir_read)
{
    SysDir *d;
    int is_subdir;

#ifndef SIL_PLATFORM_ANDROID  // Android can't read subdirectories.
    CHECK_TRUE(d = wrap_sys_dir_open("testdata/DIR1"));
    CHECK_STREQUAL(sys_dir_read(d, &is_subdir), "dir2");
    CHECK_TRUE(is_subdir);
    CHECK_STREQUAL(sys_dir_read(d, &is_subdir), NULL);
    sys_dir_close(d);
#endif

    CHECK_TRUE(d = wrap_sys_dir_open("testdata/DIR1/dir2"));
    CHECK_STREQUAL(sys_dir_read(d, &is_subdir), "File.Txt");
    CHECK_FALSE(is_subdir);
    CHECK_STREQUAL(sys_dir_read(d, &is_subdir), NULL);
    sys_dir_close(d);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_dir_trailing_slash)
{
    SysDir *d;
    int is_subdir;

    CHECK_TRUE(d = wrap_sys_dir_open("testdata/DIR1/dir2/"));
    CHECK_STREQUAL(sys_dir_read(d, &is_subdir), "File.Txt");
    CHECK_FALSE(is_subdir);
    CHECK_STREQUAL(sys_dir_read(d, &is_subdir), NULL);
    sys_dir_close(d);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_dir_memory_failure)
{
    SysDir *d;
    int is_subdir;

    TEST_mem_fail_after(0, 1, 0);
    const int need_mem_check = !((d = wrap_sys_dir_open("testdata/DIR1/dir2"))
                                 && sys_dir_read(d, &is_subdir));
    TEST_mem_fail_after(-1, 0, 0);
    if (need_mem_check) {
        CHECK_MEMORY_FAILURES((d = wrap_sys_dir_open("testdata/DIR1/dir2"))
                              && (sys_dir_read(d, &is_subdir)
                                  || (sys_dir_close(d), 0)));
    }
    sys_dir_close(d);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_dir_invalid)
{
    CHECK_FALSE(sys_dir_open(NULL));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_INVALID_PARAMETER);

    CHECK_FALSE(sys_dir_open(""));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_FILE_NOT_FOUND);

    int is_subdir;
    CHECK_FALSE(sys_dir_read(NULL, &is_subdir));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_INVALID_PARAMETER);

    CHECK_FALSE(sys_dir_open(""));  // Just to change the last error code.
    CHECK_INTEQUAL(sys_last_error(), SYSERR_FILE_NOT_FOUND);

    SysDir *d;
    CHECK_TRUE(d = wrap_sys_dir_open("testdata/DIR1/dir2"));
    CHECK_FALSE(sys_dir_read(d, NULL));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_INVALID_PARAMETER);
    sys_dir_close(d);

    sys_dir_close(NULL);  // Just make sure it doesn't crash.

    return 1;
}

/*************************************************************************/
/*************************************************************************/
