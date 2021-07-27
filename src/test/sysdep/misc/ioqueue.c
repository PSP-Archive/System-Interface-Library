/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/misc/ioqueue.c: Tests for the ioqueue library.
 */

#include "src/base.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/misc/ioqueue.h"
#include "src/test/base.h"
#include "src/thread.h"

#if defined(SIL_PLATFORM_ANDROID) || defined(SIL_PLATFORM_IOS) || defined(SIL_PLATFORM_LINUX) || defined(SIL_PLATFORM_MACOSX)
# define IS_POSIX  // For convenience.
#endif

#if defined(IS_POSIX)
# include "src/sysdep/posix/path_max.h"
# include <fcntl.h>
# include <time.h>
# include <unistd.h>
# define CANCEL_ERROR_CODE  ECANCELED
# define IO_ERROR_CODE      EIO
#elif defined(SIL_PLATFORM_WINDOWS)
# include "src/sysdep/windows/internal.h"
# include "src/sysdep/windows/utf8-wrappers.h"
# define O_RDONLY  0
# define O_WRONLY  1
# define O_RDWR    2
# define CANCEL_ERROR_CODE  ERROR_OPERATION_ABORTED
# define IO_ERROR_CODE      ERROR_GEN_FAILURE
#endif

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

/**
 * req_waiter:  Thread wrapper for ioq_wait().
 *
 * [Parameters]
 *     req: Request ID (cast to a pointer).
 * [Return value]
 *     ioq_wait(req)
 */
static int req_waiter(void *req)
{
    return ioq_wait((int)(intptr_t)req, NULL);
}

/*-----------------------------------------------------------------------*/

/**
 * msleep:  Sleep for the given number of milliseconds.
 */
static void msleep(int milliseconds)
{
#if defined(IS_POSIX)
    nanosleep(&(struct timespec){milliseconds / 1000,
                                 (milliseconds % 1000) * 1000000}, NULL);
#elif defined(SIL_PLATFORM_WINDOWS)
    Sleep(milliseconds);
#endif
}

/*-----------------------------------------------------------------------*/

/**
 * real_open, real_read, real_close:  POSIX-like wrappers for file-related
 * system calls.
 */
static IOQHandle real_open(const char *path, int flags)
{
#if defined(IS_POSIX)
    return open(path, flags);
#elif defined(SIL_PLATFORM_WINDOWS)
    (void) flags;
    return CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                      OPEN_EXISTING, 0, NULL);
#endif
}

static int64_t real_read(IOQHandle fd, void *buf, int size)
{
#if defined(IS_POSIX)
    return read(fd, buf, size);
#elif defined(SIL_PLATFORM_WINDOWS)
    DWORD nread;
    return ReadFile(fd, buf, size, &nread, NULL) ? nread : 0;
#endif
}

static void real_close(IOQHandle fd)
{
#if defined(IS_POSIX)
    close(fd);
#elif defined(SIL_PLATFORM_WINDOWS)
    CloseHandle(fd);
#endif
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

static int do_test_misc_ioqueue(void);
int test_misc_ioqueue(void)
{
#ifdef SIL_PLATFORM_ANDROID
    /* All the data files are stuck in an archive, so open() will fail. */
    SKIP("Skipping ioqueue test on Android (open() not available).");
#endif

#ifdef SIL_PLATFORM_WINDOWS
    char original_cwd[MAX_PATH];
    char resource_dir[MAX_PATH];
#else
    char original_cwd[PATH_MAX];
    char resource_dir[PATH_MAX];
#endif
    ASSERT(sys_get_resource_path_prefix(resource_dir, sizeof(resource_dir))
           < (int)sizeof(resource_dir));
#if defined(IS_POSIX)
    ASSERT(getcwd(original_cwd, sizeof(original_cwd)));
    ASSERT(chdir(resource_dir) == 0);
#elif defined(SIL_PLATFORM_WINDOWS)
    ASSERT(GetCurrentDirectory(sizeof(original_cwd), original_cwd)
           < sizeof(original_cwd));
    ASSERT(SetCurrentDirectory(resource_dir));
#endif

    const int result = do_test_misc_ioqueue();

#if defined(IS_POSIX)
    ASSERT(chdir(original_cwd) == 0);
#elif defined(SIL_PLATFORM_WINDOWS)
    ASSERT(SetCurrentDirectory(original_cwd));
#endif

    return result;
}

/*-----------------------------------------------------------------------*/

DEFINE_GENERIC_TEST_RUNNER(do_test_misc_ioqueue)

/*-----------------------------------------------------------------------*/

TEST_INIT(init)
{
    CHECK_TRUE(thread_init());
    CHECK_TRUE(ioq_init());
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
    ioq_reset();
    thread_cleanup();
    return 1;
}

/*************************************************************************/
/********************** Test routines: Basic tests ***********************/
/*************************************************************************/

/* Check that files can be opened (on both the first and subsequent
 * requests). */
TEST(test_open)
{
    int req, error;
    IOQHandle fd, fd2;
    char buf[5];

    CHECK_TRUE(req = ioq_open("testdata/test.txt", O_RDONLY, -1));
    error = -1;
    CHECK_TRUE((fd = RESULT_TO_IOQHANDLE(ioq_wait(req, &error)))
               != IOQHANDLE_INVALID);
    CHECK_INTEQUAL(error, 0);

    CHECK_TRUE(req = ioq_open("testdata/test.txt", O_RDONLY, -1));
    error = -1;
    CHECK_TRUE((fd2 = RESULT_TO_IOQHANDLE(ioq_wait(req, &error)))
               != IOQHANDLE_INVALID);
    CHECK_INTEQUAL(error, 0);

    mem_clear(buf, sizeof(buf));
    CHECK_INTEQUAL(real_read(fd, buf, 5), 5);
    CHECK_MEMEQUAL(buf, "hello", 5);
    mem_clear(buf, sizeof(buf));
    CHECK_INTEQUAL(real_read(fd2, buf, 5), 5);
    CHECK_MEMEQUAL(buf, "hello", 5);
    real_close(fd);
    real_close(fd2);

    return 1;
}

/*-----------------------------------------------------------------------*/

/* Check that memory allocation errors while opening files are handled
 * properly. */
TEST(test_open_memory_failure)
{
#ifdef SIL_PLATFORM_WINDOWS
    /* The test fails due to a spurious memory leak report because the
     * request array is left expanded when the operation itself fails due
     * to memory allocation failure in CreateFileU(). */
    SKIP("Spurious failure on Windows.");
#endif

    int req;
    IOQHandle fd;
    char buf[5];

    CHECK_MEMORY_FAILURES(
        (req = ioq_open("testdata/test.txt", O_RDONLY, -1))
        && (fd = RESULT_TO_IOQHANDLE(ioq_wait(req, NULL)))
           != IOQHANDLE_INVALID);
    mem_clear(buf, sizeof(buf));
    CHECK_INTEQUAL(real_read(fd, buf, 5), 5);
    CHECK_MEMEQUAL(buf, "hello", 5);
    real_close(fd);

    return 1;
}

/*-----------------------------------------------------------------------*/

/* Check that a NULL path is rejected (as opposed to crashing). */
TEST(test_open_null)
{
    CHECK_FALSE(ioq_open(NULL, O_RDONLY, -1));

    return 1;
}

/*-----------------------------------------------------------------------*/

/* Check that O_WRONLY and O_RDWR are rejected.  Use /dev/null to avoid
 * accidentally overwriting existing files (but make sure it exists first). */
TEST(test_open_write)
{
#ifdef SIL_PLATFORM_WINDOWS
    SKIP("Not applicable to Windows.");
#endif

    int req, error;
    IOQHandle fd;

    CHECK_TRUE(req = ioq_open("/dev/null", O_RDONLY, -1));
    error = -1;
    CHECK_TRUE((fd = RESULT_TO_IOQHANDLE(ioq_wait(req, &error)))
               != IOQHANDLE_INVALID);
    CHECK_INTEQUAL(error, 0);
    real_close(fd);
    CHECK_FALSE(ioq_open("/dev/null", O_WRONLY, -1));
    CHECK_FALSE(ioq_open("/dev/null", O_RDWR, -1));

    return 1;
}

/*-----------------------------------------------------------------------*/

/* Check that attempting to open a nonexistent file fails with ENOENT. */
TEST(test_open_noent)
{
    int req, error;

    CHECK_TRUE(req = ioq_open("testdata/no_such_file", O_RDONLY, -1));
    error = -1;
    CHECK_INTEQUAL(ioq_wait(req, &error), -1);
#if defined(IS_POSIX)
    CHECK_INTEQUAL(error, ENOENT);
#elif defined(SIL_PLATFORM_WINDOWS)
    CHECK_INTEQUAL(error, ERROR_FILE_NOT_FOUND);
#endif

    return 1;
}

/*-----------------------------------------------------------------------*/

/* Check that files can be read (on both the first and subsequent requests). */
TEST(test_read)
{
    IOQHandle fd, fd2;
    int req, error;
    char buf[5];

    CHECK_TRUE((fd = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);
    CHECK_TRUE((fd2 = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);

    mem_clear(buf, sizeof(buf));
    error = -1;
    CHECK_TRUE(req = ioq_read(fd, buf, 5, 0, -1));
    CHECK_INTEQUAL(ioq_wait(req, &error), 5);
    CHECK_INTEQUAL(error, 0);
    CHECK_MEMEQUAL(buf, "hello", 5);

    mem_clear(buf, sizeof(buf));
    error = -1;
    CHECK_TRUE(req = ioq_read(fd2, buf, 5, 0, -1));
    CHECK_INTEQUAL(ioq_wait(req, &error), 5);
    CHECK_INTEQUAL(error, 0);
    CHECK_MEMEQUAL(buf, "hello", 5);

    real_close(fd);
    real_close(fd2);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* Check ioq_poll() behavior. */
TEST(test_poll)
{
    IOQHandle fd;
    int req;
    char buf[5];

    CHECK_TRUE((fd = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);

    /* Check that ioq_poll() returns true at some point for a valid request
     * (which is about the best we can do for testing). */
    CHECK_TRUE(req = ioq_read(fd, buf, 5, 0, -1));
    while (!ioq_poll(req)) {
        thread_yield();
    }
    CHECK_INTEQUAL(ioq_wait(req, NULL), 5);
    CHECK_MEMEQUAL(buf, "hello", 5);

    /* Check that ioq_poll() returns true for invalid request IDs. */
    CHECK_TRUE(ioq_poll(0));
    CHECK_TRUE(ioq_poll(INT_MAX));
    CHECK_TRUE(ioq_poll(req));

    real_close(fd);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* Check ioq_wait() invalid cases. */
TEST(test_wait)
{
    IOQHandle fd;
    int req, error;
    char buf[5];

    CHECK_TRUE((fd = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);
    CHECK_TRUE(req = ioq_read(fd, buf, 5, 0, -1));
    CHECK_INTEQUAL(ioq_wait(req, NULL), 5);

    errno = 0;
    CHECK_INTEQUAL(ioq_wait(0, NULL), -1);
    CHECK_INTEQUAL(errno, ESRCH);
    errno = 0;
    CHECK_INTEQUAL(ioq_wait(INT_MAX, NULL), -1);
    CHECK_INTEQUAL(errno, ESRCH);
    errno = 0;
    error = -1;
    CHECK_INTEQUAL(ioq_wait(req, &error), -1);
    CHECK_INTEQUAL(errno, ESRCH);
    CHECK_INTEQUAL(error, 0);

    real_close(fd);
    return 1;
}

/*************************************************************************/
/***************** Test routines: Test control functions *****************/
/*************************************************************************/

/* Check that the testing-specific call TEST_misc_ioqueue_block_io_thread()
 * properly blocks I/O. */
TEST(test_block_io_thread)
{
    IOQHandle fd;
    int req;
    char buf[5];

    CHECK_TRUE((fd = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);

    mem_clear(buf, sizeof(buf));
    TEST_misc_ioqueue_block_io_thread(1);
    CHECK_TRUE(req = ioq_read(fd, buf, 5, 0, -1));
    /* Wait long enough that we can be reasonably sure the data would have
     * been loaded if the thread was running. */
    msleep(100);
    CHECK_FALSE(ioq_poll(req));
    TEST_misc_ioqueue_block_io_thread(0);
    CHECK_INTEQUAL(ioq_wait(req, NULL), 5);
    CHECK_MEMEQUAL(buf, "hello", 5);

    real_close(fd);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* Check that the testing-specific call TEST_misc_ioqueue_unblock_on_wait()
 * properly unblocks I/O on an ioq_wait() call. */
TEST(test_unblock_on_wait)
{
    IOQHandle fd;
    int req;
    char buf[5];

    CHECK_TRUE((fd = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);

    mem_clear(buf, sizeof(buf));
    TEST_misc_ioqueue_block_io_thread(1);
    TEST_misc_ioqueue_unblock_on_wait(1);
    CHECK_TRUE(req = ioq_read(fd, buf, 5, 0, -1));
    CHECK_INTEQUAL(ioq_wait(req, NULL), 5);  // Will freeze on test failure.
    CHECK_MEMEQUAL(buf, "hello", 5);
    TEST_misc_ioqueue_unblock_on_wait(0);

    real_close(fd);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* Check that the testing-specific call TEST_misc_ioqueue_step_io_thread()
 * properly runs exactly one loop of the I/O thread. */
TEST(test_step_io_thread)
{
    IOQHandle fd;
    int req, req2;
    char buf[5];

    CHECK_TRUE((fd = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);
    TEST_misc_ioqueue_block_io_thread(1);
    CHECK_TRUE(req = ioq_read(fd, buf, 5, 0, -1));
    CHECK_TRUE(req2 = ioq_read(fd, buf, 5, 0, -1));

    mem_clear(buf, sizeof(buf));
    TEST_misc_ioqueue_step_io_thread();
    CHECK_TRUE(ioq_poll(req));
    CHECK_INTEQUAL(ioq_wait(req, NULL), 5);
    CHECK_MEMEQUAL(buf, "hello", 5);
    CHECK_FALSE(ioq_poll(req2));

    mem_clear(buf, sizeof(buf));
    TEST_misc_ioqueue_step_io_thread();
    CHECK_TRUE(ioq_poll(req2));
    CHECK_INTEQUAL(ioq_wait(req2, NULL), 5);
    CHECK_MEMEQUAL(buf, "hello", 5);

    TEST_misc_ioqueue_unblock_on_wait(0);
    real_close(fd);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* Check that the testing-specific call TEST_misc_ioqueue_permfail_next_read()
 * properly fails a single read request. */
TEST(test_permfail_next_read)
{
    IOQHandle fd, fd2;
    int req;
    char buf[5];

    CHECK_TRUE((fd = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);
    CHECK_TRUE((fd2 = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);

    TEST_misc_ioqueue_permfail_next_read(1);
    mem_clear(buf, sizeof(buf));
    CHECK_FALSE(req = ioq_read(fd, buf, 5, 0, -1));
    CHECK_INTEQUAL(errno, ENOMEM);

    mem_clear(buf, sizeof(buf));
    CHECK_TRUE(req = ioq_read(fd2, buf, 5, 0, -1));
    CHECK_INTEQUAL(ioq_wait(req, NULL), 5);
    CHECK_MEMEQUAL(buf, "hello", 5);

    real_close(fd);
    real_close(fd2);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* Check that the testing-specific call TEST_misc_ioqueue_tempfail_next_read()
 * properly fails a single read request. */
TEST(test_tempfail_next_read)
{
    IOQHandle fd, fd2;
    int req;
    char buf[5];

    CHECK_TRUE((fd = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);
    CHECK_TRUE((fd2 = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);

    TEST_misc_ioqueue_tempfail_next_read(1);
    mem_clear(buf, sizeof(buf));
    CHECK_FALSE(req = ioq_read(fd, buf, 5, 0, -1));
    CHECK_INTEQUAL(errno, EAGAIN);

    mem_clear(buf, sizeof(buf));
    CHECK_TRUE(req = ioq_read(fd2, buf, 5, 0, -1));
    CHECK_INTEQUAL(ioq_wait(req, NULL), 5);
    CHECK_MEMEQUAL(buf, "hello", 5);

    real_close(fd);
    real_close(fd2);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* Check that the testing-specific call TEST_misc_ioqueue_iofail_next_read()
 * properly fails a single read request. */
TEST(test_iofail_next_read)
{
    IOQHandle fd, fd2;
    int req, error;
    char buf[5];

    CHECK_TRUE((fd = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);
    CHECK_TRUE((fd2 = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);

    TEST_misc_ioqueue_iofail_next_read(1);
    mem_clear(buf, sizeof(buf));
    CHECK_TRUE(req = ioq_read(fd, buf, 5, 0, -1));
    CHECK_INTEQUAL(ioq_wait(req, &error), -1);
    CHECK_INTEQUAL(error, IO_ERROR_CODE);
    CHECK_MEMEQUAL(buf, "\0\0\0\0\0", 5);

    mem_clear(buf, sizeof(buf));
    CHECK_TRUE(req = ioq_read(fd2, buf, 5, 0, -1));
    CHECK_INTEQUAL(ioq_wait(req, NULL), 5);
    CHECK_MEMEQUAL(buf, "hello", 5);

    real_close(fd);
    real_close(fd2);
    return 1;
}

/*************************************************************************/
/***************** Test routines: Detailed read behavior *****************/
/*************************************************************************/

/* Check that memory allocation errors while reading files are handled
 * properly. */
TEST(test_read_memory_failure)
{
    IOQHandle fd;
    int req, res;
    char buf[5];

    CHECK_TRUE((fd = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);

    mem_clear(buf, sizeof(buf));
    CHECK_MEMORY_FAILURES(
        (req = ioq_read(fd, buf, 5, 0, -1))
        && (res = ioq_wait(req, NULL)) >= 0);
    CHECK_INTEQUAL(res, 5);
    CHECK_MEMEQUAL(buf, "hello", 5);

    real_close(fd);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* Check that reading from a position at or past the end of the file gives
 * the proper result (no bytes read, but no error). */
TEST(test_read_eof)
{
    IOQHandle fd;
    int req, error;
    char buf[1];

    CHECK_TRUE((fd = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);

    buf[0] = 1;
    CHECK_TRUE(req = ioq_read(fd, buf, 1, 5, -1));
    error = -1;
    CHECK_INTEQUAL(ioq_wait(req, &error), 0);
    CHECK_INTEQUAL(error, 0);
    CHECK_INTEQUAL(buf[0], 1);

    buf[0] = 2;
    CHECK_TRUE(req = ioq_read(fd, buf, 1, 6, -1));
    error = -1;
    CHECK_INTEQUAL(ioq_wait(req, &error), 0);
    CHECK_INTEQUAL(error, 0);
    CHECK_INTEQUAL(buf[0], 2);

    real_close(fd);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* Check that invalid read parameters are rejected. */
TEST(test_read_invalid)
{
    IOQHandle fd;
    char buf[5];

    CHECK_TRUE((fd = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);

    CHECK_FALSE(ioq_read(IOQHANDLE_INVALID, buf, 5, 0, -1));
    CHECK_FALSE(ioq_read(fd, NULL, 5, 0, -1));
    CHECK_FALSE(ioq_read(fd, buf, -1, 0, -1));
    CHECK_FALSE(ioq_read(fd, buf, 5, -1, -1));

    real_close(fd);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* Check that reads executed in parallel work properly. */
TEST(test_read_parallel)
{
    IOQHandle fd, fd2;
    int req, req2;
    char buf[5], buf2[5];

    CHECK_TRUE((fd = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);
    CHECK_TRUE((fd2 = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);

    mem_clear(buf, sizeof(buf));
    mem_clear(buf2, sizeof(buf2));
    CHECK_TRUE(req = ioq_read(fd, buf, 5, 0, -1));
    CHECK_TRUE(req2 = ioq_read(fd2, buf2, 5, 0, -1));
    CHECK_INTEQUAL(ioq_wait(req, NULL), 5);
    CHECK_MEMEQUAL(buf, "hello", 5);
    CHECK_INTEQUAL(ioq_wait(req2, NULL), 5);
    CHECK_MEMEQUAL(buf2, "hello", 5);

    real_close(fd);
    real_close(fd2);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* Check handling of memory allocation failures for parallel reads. */
TEST(test_read_parallel_memory_failure)
{
    IOQHandle fd, fd2;
    int req, req2, res2;
    char buf[5], buf2[5];

    CHECK_TRUE((fd = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);
    CHECK_TRUE((fd2 = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);

    mem_clear(buf, sizeof(buf));
    mem_clear(buf2, sizeof(buf2));
    CHECK_TRUE(req = ioq_read(fd, buf, 5, 0, -1));
    CHECK_MEMORY_FAILURES(
        (req2 = ioq_read(fd2, buf2, 5, 0, -1))
        && (res2 = ioq_wait(req2, NULL)) >= 0);
    CHECK_INTEQUAL(ioq_wait(req, NULL), 5);
    CHECK_MEMEQUAL(buf, "hello", 5);
    CHECK_INTEQUAL(res2, 5);
    CHECK_MEMEQUAL(buf2, "hello", 5);

    real_close(fd);
    real_close(fd2);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* Check handling of memory allocation failures for parallel reads when
 * a shrinking realloc() call fails. */
TEST(test_read_parallel_realloc_failure)
{
    IOQHandle fd, fd2;
    int req, req2;
    char buf[5], buf2[5];

    CHECK_TRUE((fd = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);
    CHECK_TRUE((fd2 = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);

    mem_clear(buf, sizeof(buf));
    mem_clear(buf2, sizeof(buf2));
    CHECK_TRUE(req = ioq_read(fd, buf, 5, 0, -1));
    /* CHECK_MEMORY_FAILURES() would report a leak when the shrinking
     * realloc call fails, so we do this manually and rely on the
     * automatic leak check when the test completes. */
    for (int i = 0; ; i++) {
        if (i >= 100) {
            FAIL("ioq_read(fd2, buf2, 5, 0, -1) did not succeed after 100"
                 " iterations");
        }
        TEST_mem_fail_after(i, 1, 1);
        req2 = ioq_read(fd2, buf2, 5, 0, -1);
        TEST_mem_fail_after(-1, 0, 0);
        if (req2 != 0) {
            break;
        }
    }
    CHECK_INTEQUAL(ioq_wait(req2, NULL), 5);
    CHECK_INTEQUAL(ioq_wait(req, NULL), 5);
    CHECK_MEMEQUAL(buf, "hello", 5);
    CHECK_MEMEQUAL(buf2, "hello", 5);

    real_close(fd);
    real_close(fd2);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* Check that overlapping requests work if the request array is expanded
 * while a request is being waited on. */
TEST(test_read_parallel_2)
{
    IOQHandle fd, fd2;
    int req, req2;
    char buf[5], buf2[5];
    int thread;

    CHECK_TRUE((fd = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);
    CHECK_TRUE((fd2 = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);

    TEST_misc_ioqueue_move_on_realloc(1);
    TEST_misc_ioqueue_block_io_thread(1);
    mem_clear(buf, sizeof(buf));
    mem_clear(buf2, sizeof(buf2));
    CHECK_TRUE(req = ioq_read(fd, buf, 5, 0, -1));
    ASSERT(thread = thread_create(req_waiter, (void *)(intptr_t)req));
    /* Wait long enough that we can be reasonably sure the thread is waiting
     * on the request. */
    msleep(1);
    CHECK_TRUE(req2 = ioq_read(fd2, buf2, 5, 0, -1));
    TEST_misc_ioqueue_block_io_thread(0);
    /* Check req2 first; if it's finished, then req is guaranteed to have
     * finished as well, and (in theory) we shouldn't have to wait below. */
    CHECK_INTEQUAL(ioq_wait(req2, NULL), 5);
    CHECK_MEMEQUAL(buf2, "hello", 5);
    int wait_ms = 1000;
    while (thread_is_running(thread)) {
        if (wait_ms <= 0) {
            FAIL("ioq_wait(req) did not return");
        }
        msleep(1);
        wait_ms--;
    }
    CHECK_INTEQUAL(thread_wait(thread), 5);
    CHECK_MEMEQUAL(buf, "hello", 5);

    TEST_misc_ioqueue_move_on_realloc(0);
    real_close(fd);
    real_close(fd2);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* Check that a read with a deadline gets priority over one without. */
TEST(test_read_deadline)
{
    IOQHandle fd, fd2;
    int req, req2;
    char buf[5], buf2[5];

    CHECK_TRUE((fd = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);
    CHECK_TRUE((fd2 = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);
    mem_clear(buf, sizeof(buf));
    mem_clear(buf2, sizeof(buf2));

    TEST_misc_ioqueue_block_io_thread(1);
    TEST_misc_ioqueue_unblock_on_wait(1);
    CHECK_TRUE(req = ioq_read(fd, buf, 5, 0, -1));
    CHECK_TRUE(req2 = ioq_read(fd2, buf2, 5, 0, 1000000000));
    CHECK_INTEQUAL(ioq_wait(req2, NULL), 5);
    CHECK_MEMEQUAL(buf2, "hello", 5);
    CHECK_FALSE(ioq_poll(req));
    CHECK_INTEQUAL(ioq_wait(req, NULL), 5);
    CHECK_MEMEQUAL(buf, "hello", 5);
    TEST_misc_ioqueue_block_io_thread(0);
    TEST_misc_ioqueue_unblock_on_wait(0);

    real_close(fd);
    real_close(fd2);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* Check that a read with an earlier deadline gets priority over one with
 * a later deadline. */
TEST(test_read_deadline_order)
{
    IOQHandle fd, fd2, fd3;
    int req, req2, req3;
    char buf[5], buf2[5], buf3[5];

    CHECK_TRUE((fd = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);
    CHECK_TRUE((fd2 = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);
    CHECK_TRUE((fd3 = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);
    mem_clear(buf, sizeof(buf));
    mem_clear(buf2, sizeof(buf2));
    mem_clear(buf3, sizeof(buf3));

    TEST_misc_ioqueue_block_io_thread(1);
    TEST_misc_ioqueue_unblock_on_wait(1);
    CHECK_TRUE(req = ioq_read(fd, buf, 5, 0, 1500000000));
    CHECK_TRUE(req2 = ioq_read(fd2, buf2, 5, 0, 1000000000));
    CHECK_TRUE(req3 = ioq_read(fd3, buf3, 5, 0, 2000000000));
    CHECK_INTEQUAL(ioq_wait(req2, NULL), 5);
    CHECK_MEMEQUAL(buf2, "hello", 5);
    CHECK_FALSE(ioq_poll(req));
    CHECK_FALSE(ioq_poll(req3));
    CHECK_INTEQUAL(ioq_wait(req, NULL), 5);
    CHECK_MEMEQUAL(buf, "hello", 5);
    CHECK_FALSE(ioq_poll(req3));
    CHECK_INTEQUAL(ioq_wait(req3, NULL), 5);
    CHECK_MEMEQUAL(buf3, "hello", 5);
    TEST_misc_ioqueue_block_io_thread(0);
    TEST_misc_ioqueue_unblock_on_wait(0);

    real_close(fd);
    real_close(fd2);
    real_close(fd3);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* Check that a read with an expired deadline gets priority over one with
 * a non-expired deadline. */
TEST(test_read_deadline_expired)
{
    IOQHandle fd, fd2;
    int req, req2;
    char buf[5], buf2[5];

    CHECK_TRUE((fd = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);
    CHECK_TRUE((fd2 = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);
    mem_clear(buf, sizeof(buf));
    mem_clear(buf2, sizeof(buf2));

    TEST_misc_ioqueue_block_io_thread(1);
    TEST_misc_ioqueue_unblock_on_wait(1);
    CHECK_TRUE(req = ioq_read(fd, buf, 5, 0, 1000000000));
    CHECK_TRUE(req2 = ioq_read(fd2, buf2, 5, 0, 0));  // Expires immediately.
    CHECK_INTEQUAL(ioq_wait(req2, NULL), 5);
    CHECK_MEMEQUAL(buf2, "hello", 5);
    CHECK_FALSE(ioq_poll(req));
    CHECK_INTEQUAL(ioq_wait(req, NULL), 5);
    CHECK_MEMEQUAL(buf, "hello", 5);
    TEST_misc_ioqueue_block_io_thread(0);
    TEST_misc_ioqueue_unblock_on_wait(0);

    real_close(fd);
    real_close(fd2);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* Check the logic to split large read requests into smaller blocks. */
TEST(test_read_limit)
{
    IOQHandle fd;
    int req, req2;
    char buf[6], buf2[2];

    ioq_set_read_limit(2);
    TEST_misc_ioqueue_block_io_thread(1);
    CHECK_TRUE((fd = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);

    mem_clear(buf, sizeof(buf));
    mem_clear(buf2, sizeof(buf2));
    CHECK_TRUE(req = ioq_read(fd, buf, 6, 0, -1));
    CHECK_TRUE(req2 = ioq_read(fd, buf2, 2, 1, -1));

    TEST_misc_ioqueue_step_io_thread();
    CHECK_FALSE(ioq_poll(req));
    CHECK_FALSE(ioq_poll(req2));
    CHECK_MEMEQUAL(buf, "he\0\0\0\0", 6);
    CHECK_MEMEQUAL(buf2, "\0\0", 2);

    TEST_misc_ioqueue_step_io_thread();
    CHECK_FALSE(ioq_poll(req));
    CHECK_FALSE(ioq_poll(req2));
    CHECK_MEMEQUAL(buf, "hell\0\0", 6);
    CHECK_MEMEQUAL(buf2, "\0\0", 2);

    TEST_misc_ioqueue_step_io_thread();
    CHECK_TRUE(ioq_poll(req));
    CHECK_FALSE(ioq_poll(req2));
    CHECK_INTEQUAL(ioq_wait(req, NULL), 5);
    CHECK_MEMEQUAL(buf, "hello\0", 6);
    CHECK_MEMEQUAL(buf2, "\0\0", 2);

    TEST_misc_ioqueue_step_io_thread();
    CHECK_TRUE(ioq_poll(req2));
    CHECK_INTEQUAL(ioq_wait(req2, NULL), 2);
    CHECK_MEMEQUAL(buf, "hello\0", 6);
    CHECK_MEMEQUAL(buf2, "el", 2);

    real_close(fd);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* Check that a split read is fully processed if the I/O thread cannot be
 * started due to (for example) memory allocation errors. */
TEST(test_read_limit_memory_failure)
{
    IOQHandle fd;
    int req, res;
    char buf[5];

    ioq_set_read_limit(2);
    CHECK_TRUE((fd = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);

    mem_clear(buf, sizeof(buf));
    CHECK_MEMORY_FAILURES(
        (req = ioq_read(fd, buf, 5, 0, -1))
        && (res = ioq_wait(req, NULL)) >= 0);
    CHECK_INTEQUAL(res, 5);
    CHECK_MEMEQUAL(buf, "hello", 5);

    real_close(fd);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* Check that a split read which fails after reading some data returns the
 * amount of data read up to the failure. */
TEST(test_read_limit_read_failure)
{
    IOQHandle fd;
    int req;
    char buf[5];

    ioq_set_read_limit(2);
    TEST_misc_ioqueue_block_io_thread(1);
    CHECK_TRUE((fd = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);

    mem_clear(buf, sizeof(buf));
    CHECK_TRUE(req = ioq_read(fd, buf, 5, 0, -1));
    TEST_misc_ioqueue_step_io_thread();
    CHECK_FALSE(ioq_poll(req));
    CHECK_MEMEQUAL(buf, "he\0\0\0", 5);

    TEST_misc_ioqueue_iofail_next_read(1);
    TEST_misc_ioqueue_step_io_thread();
    CHECK_TRUE(ioq_poll(req));
    CHECK_INTEQUAL(ioq_wait(req, NULL), 2);
    CHECK_MEMEQUAL(buf, "he\0\0\0", 5);

    real_close(fd);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* Check that a split read which is cancelled after reading some data
 * returns the amount of data read up to the cancellation. */
TEST(test_read_limit_cancel)
{
    IOQHandle fd;
    int req, error;
    char buf[5];

    ioq_set_read_limit(2);
    TEST_misc_ioqueue_block_io_thread(1);
    CHECK_TRUE((fd = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);

    mem_clear(buf, sizeof(buf));
    CHECK_TRUE(req = ioq_read(fd, buf, 5, 0, -1));
    TEST_misc_ioqueue_step_io_thread();
    CHECK_FALSE(ioq_poll(req));
    CHECK_MEMEQUAL(buf, "he\0\0\0", 5);

    ioq_cancel(req);
    TEST_misc_ioqueue_step_io_thread();
    CHECK_TRUE(ioq_poll(req));
    error = 0;
    CHECK_INTEQUAL(ioq_wait(req, &error), -1);
    CHECK_INTEQUAL(error, CANCEL_ERROR_CODE);
    CHECK_MEMEQUAL(buf, "he\0\0\0", 5);

    real_close(fd);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* Check that a split read request can be interrupted by a higher-priority
 * request. */
TEST(test_read_limit_priority)
{
    IOQHandle fd;
    int req, req2;
    char buf[5], buf2[3];

    ioq_set_read_limit(3);
    TEST_misc_ioqueue_block_io_thread(1);
    CHECK_TRUE((fd = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);

    mem_clear(buf, sizeof(buf));
    CHECK_TRUE(req = ioq_read(fd, buf, 5, 0, -1));
    TEST_misc_ioqueue_step_io_thread();
    CHECK_FALSE(ioq_poll(req));
    CHECK_MEMEQUAL(buf, "hel\0\0", 5);

    mem_clear(buf2, sizeof(buf2));
    CHECK_TRUE(req2 = ioq_read(fd, buf2, 3, 2, 0));
    TEST_misc_ioqueue_step_io_thread();
    CHECK_FALSE(ioq_poll(req));
    CHECK_TRUE(ioq_poll(req2));
    CHECK_INTEQUAL(ioq_wait(req2, NULL), 3);
    CHECK_MEMEQUAL(buf, "hel\0\0", 5);
    CHECK_MEMEQUAL(buf2, "llo", 3);

    TEST_misc_ioqueue_step_io_thread();
    CHECK_TRUE(ioq_poll(req));
    CHECK_INTEQUAL(ioq_wait(req, NULL), 5);
    CHECK_MEMEQUAL(buf, "hello", 5);

    real_close(fd);
    return 1;
}

/*************************************************************************/
/***************** Test routines: Cancel/reset behavior ******************/
/*************************************************************************/

/* Check that ioq_cancel() properly cancels requests. */
TEST(test_cancel)
{
    IOQHandle fd;
    int req, req2, error;
    char buf[5];

    CHECK_TRUE((fd = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);

    CHECK_TRUE(req = ioq_open("testdata/nonexistent.txt", O_RDONLY, -1));
    CHECK_TRUE(req2 = ioq_read(fd, buf, 5, 0, -1));
    while (!ioq_poll(req2)) {
        thread_yield();
    }
    ioq_cancel(req);
    ioq_cancel(req2);
    error = 0;
    CHECK_INTEQUAL(ioq_wait(req, &error), -1);
    CHECK_INTEQUAL(error, CANCEL_ERROR_CODE);
    error = 0;
    CHECK_INTEQUAL(ioq_wait(req2, &error), -1);
    CHECK_INTEQUAL(error, CANCEL_ERROR_CODE);

    /* Check that ioq_cancel() doesn't crash on an invalid request ID. */
    ioq_cancel(0);
    ioq_cancel(req);

    real_close(fd);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* Check that ioq_cancel() properly cancels a request that has not yet
 * begun. */
TEST(test_cancel_pending_request)
{
    IOQHandle fd;
    int req, error;
    char buf[5];

    CHECK_TRUE((fd = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);
    mem_clear(buf, 5);

    TEST_misc_ioqueue_block_io_thread(1);
    CHECK_TRUE(req = ioq_read(fd, buf, 5, 0, -1));
    ioq_cancel(req);
    TEST_misc_ioqueue_block_io_thread(0);
    error = 0;
    CHECK_INTEQUAL(ioq_wait(req, &error), -1);
    CHECK_INTEQUAL(error, CANCEL_ERROR_CODE);
    CHECK_MEMEQUAL(buf, "\0\0\0\0\0", 5);

    real_close(fd);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* Check that ioq_cancel() on a completed open operation closes the FD. */
TEST(test_cancel_open)
{
#ifdef SIL_PLATFORM_WINDOWS
    SKIP("File handle values are not consistent on Windows.");
#endif

    IOQHandle fd;
    int req, error;

    /* We assume that repeated sequences of open() followed by close() will
     * always return the same FD, and see if there's any change in FD from
     * a raw open() call before and after a cancelled open operation. */

    CHECK_TRUE((fd = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);
    real_close(fd);
    CHECK_TRUE(real_open("testdata/test.txt", O_RDONLY) == fd);
    real_close(fd);
    CHECK_TRUE(req = ioq_open("testdata/test.txt", O_RDONLY, -1));
    while (!ioq_poll(req)) {
        thread_yield();
    }
    ioq_cancel(req);
    error = 0;
    CHECK_INTEQUAL(ioq_wait(req, &error), -1);
    CHECK_INTEQUAL(error, CANCEL_ERROR_CODE);
    /* Explicit cast to intptr_t so the code compiles on Windows, even
     * though it's not run. */
    CHECK_INTEQUAL((intptr_t)real_open("testdata/test.txt", O_RDONLY),
                   (intptr_t)fd);

    real_close(fd);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* Check that ioq_cancel_fd() properly cancels requests for the given file
 * (and not for other files). */
TEST(test_cancel_fd)
{
    IOQHandle fd, fd2, fd3;
    int req, req2, req3, error;
    char buf[5], buf2[5];

    CHECK_TRUE((fd = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);
    CHECK_TRUE((fd2 = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);

    TEST_misc_ioqueue_block_io_thread(1);
    mem_clear(buf, sizeof(buf));
    mem_clear(buf2, sizeof(buf2));
    CHECK_TRUE(req = ioq_read(fd, buf, 5, 0, -1));
    CHECK_TRUE(req2 = ioq_read(fd2, buf2, 5, 0, -1));
    CHECK_TRUE(req3 = ioq_open("testdata/test.txt", O_RDONLY, -1));
    ioq_cancel_fd(fd);
    TEST_misc_ioqueue_block_io_thread(0);
    error = 0;
    CHECK_INTEQUAL(ioq_wait(req, &error), -1);
    CHECK_INTEQUAL(error, CANCEL_ERROR_CODE);
    CHECK_MEMEQUAL(buf, "\0\0\0\0\0", 5);
    error = -1;
    CHECK_INTEQUAL(ioq_wait(req2, &error), 5);
    CHECK_INTEQUAL(error, 0);
    CHECK_MEMEQUAL(buf2, "hello", 5);
    error = -1;
    CHECK_TRUE((fd3 = RESULT_TO_IOQHANDLE(ioq_wait(req3, &error)))
               != IOQHANDLE_INVALID);
    CHECK_INTEQUAL(error, 0);
    real_close(fd3);

    /* Cancelled requests should return an error code even if they had
     * completed before being cancelled. */
    mem_clear(buf, sizeof(buf));
    CHECK_TRUE(req = ioq_read(fd, buf, 5, 0, -1));
    while (!ioq_poll(req)) {
        thread_yield();
    }
    ioq_cancel_fd(fd);
    error = 0;
    CHECK_INTEQUAL(ioq_wait(req, &error), -1);
    CHECK_INTEQUAL(error, CANCEL_ERROR_CODE);
    CHECK_MEMEQUAL(buf, "hello", 5);

    real_close(fd);
    real_close(fd2);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* Check that ioq_reset() properly cancels pending requests.  We can't
 * check a cancelled request's status directly, so we use the same test as
 * for cancelled open operations. */
TEST(test_reset)
{
#ifdef SIL_PLATFORM_WINDOWS
    SKIP("File handle values are not consistent on Windows.");
#endif

    IOQHandle fd, fd2;
    int req, req2;
    char buf[5];

    CHECK_TRUE((fd = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);
    CHECK_TRUE((fd2 = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);
    real_close(fd2);
    CHECK_TRUE(req = ioq_open("testdata/nonexistent.txt", O_RDONLY, -1));
    CHECK_TRUE(req2 = ioq_open("testdata/test.txt", O_RDONLY, -1));
    while (!ioq_poll(req2)) {
        thread_yield();
    }
    TEST_misc_ioqueue_block_io_thread(1);
    CHECK_TRUE(ioq_read(fd, buf, 5, 0, -1));
    CHECK_TRUE(ioq_open("testdata/nonexistent.txt", O_RDONLY, -1));
    TEST_misc_ioqueue_block_io_thread(0);
    ioq_reset();
    CHECK_INTEQUAL((intptr_t)real_open("testdata/test.txt", O_RDONLY),
                   (intptr_t)fd2);

    real_close(fd);
    real_close(fd2);
    return 1;
}

/*************************************************************************/
/**************** Test routines: Request array management ****************/
/*************************************************************************/

/* Check that the request array is shrunk when there are a lot of unused
 * entries.  Note that sporadic failures of this or the next test may
 * indicate that the I/O thread is improperly modifying the requests
 * array while not holding the mutex.  (Such failures typically result
 * from the array being reallocated at the same instant a request is
 * being processed.) */
TEST(test_shrink_request_array)
{
    IOQHandle fd;
    int req[20];
    char buf[5];

    CHECK_TRUE((fd = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);
    const uint64_t used1 = mem_debug_bytes_allocated();

    for (int i = 0; i < lenof(req); i++) {
        CHECK_TRUE(req[i] = ioq_read(fd, buf, 5, 0, -1));
    }
    const uint64_t used2 = mem_debug_bytes_allocated();

    for (int i = lenof(req) - 1; i >= 0; i--) {
        CHECK_INTEQUAL(ioq_wait(req[i], NULL), 5);
    }
    const uint64_t used3 = mem_debug_bytes_allocated();

    CHECK_TRUE(used3 < used2);
    CHECK_TRUE(used3 > used1); // There should still be some entries allocated.

    real_close(fd);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* Check memory reallocation failure when shrinking the request array. */
TEST(test_shrink_request_array_memory_failure)
{
    IOQHandle fd;
    int req[20];
    char buf[5];

    CHECK_TRUE((fd = real_open("testdata/test.txt", O_RDONLY))
               != IOQHANDLE_INVALID);

    for (int i = 0; i < lenof(req); i++) {
        CHECK_TRUE(req[i] = ioq_read(fd, buf, 5, 0, -1));
    }
    for (int i = lenof(req) - 1; i >= 0; i--) {
        CHECK_INTEQUAL(ioq_wait(req[i], NULL), 5);
    }
    const uint64_t used1 = mem_debug_bytes_allocated();

    for (int i = 0; i < lenof(req); i++) {
        CHECK_TRUE(req[i] = ioq_read(fd, buf, 5, 0, -1));
    }
    for (int i = lenof(req) - 1; i >= 0; i--) {
        TEST_mem_fail_after(0, 1, 1);
        const int res = ioq_wait(req[i], NULL);
        TEST_mem_fail_after(-1, 0, 0);
        CHECK_INTEQUAL(res, 5);
    }
    const uint64_t used2 = mem_debug_bytes_allocated();

    CHECK_TRUE(used2 > used1);

    real_close(fd);
    return 1;
}

/*************************************************************************/
/*************************************************************************/
