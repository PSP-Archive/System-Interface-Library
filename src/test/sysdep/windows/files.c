/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/windows/files.c: Windows-specific file tests.
 */

#include "src/base.h"
#include "src/semaphore.h"
#include "src/sysdep.h"
#include "src/sysdep/windows/internal.h"
#include "src/test/base.h"
#include "src/thread.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Data structure for passing to pipe_thread(). */
typedef struct PipeThreadData PipeThreadData;
struct PipeThreadData {
    char path[100];   // Pathname of the named pipe.
    int create_sema;  // Semaphore signalled after the pipe is created.
    int write_sema;   // Semaphore on which the writer waits before writing.
    int done_sema;    // Semaphore signalled after the write completes.
    int close_sema;   // Semaphore on which writer waits before closing pipe.
};

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

/*-----------------------------------------------------------------------*/

/**
 * pipe_thread:  Thread to create a pipe, accept a single connection, wait
 * on a semaphore, then write a single byte ('a') through the pipe.
 *
 * The pipe pathname is taken from the file-scope variable pipe_path.
 *
 * [Parameters]
 *     data: Thread data (PipeThreadData *).
 * [Return value]
 *     True if all operations completed successfully, false on error.
 */
static int pipe_thread(void *data_)
{
    PipeThreadData *data = (PipeThreadData *)data_;

    HANDLE handle = CreateNamedPipe(
        data->path,
        PIPE_ACCESS_OUTBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1, 1, 0, 0, NULL);
    semaphore_signal(data->create_sema);
    if (handle == INVALID_HANDLE_VALUE) {
        DLOG("Failed to create pipe: %s", windows_strerror(GetLastError()));
        return 0;
    }

    /* ConnectNamedPipe() generates ERROR_PIPE_CONNECTED if the remote side
     * connected before the call, but at least for our purposes, this isn't
     * an error condition. */
    if (!ConnectNamedPipe(handle, NULL)
     && GetLastError() != ERROR_PIPE_CONNECTED) {
        DLOG("Failed to wait for pipe reader: %s",
             windows_strerror(GetLastError()));
        CloseHandle(handle);
        return 0;
    }

    semaphore_wait(data->write_sema);
    DWORD num_written;
    const int write_result = WriteFile(handle, "a", 1, &num_written, NULL);
    semaphore_signal(data->done_sema);
    if (!write_result) {
        DLOG("Failed to write to pipe: %s", windows_strerror(GetLastError()));
        CloseHandle(handle);
        return 0;
    } else if (num_written == 0) {
        DLOG("Failed to write to pipe: num_written = 0");
        CloseHandle(handle);
        return 0;
    }

    semaphore_wait(data->close_sema);
    CloseHandle(handle);
    return 1;
}

/*-----------------------------------------------------------------------*/

/**
 * read_at_thread:  Thread which calls sys_file_read_at() to read one byte
 * from the beginning of the given file.
 *
 * [Parameters]
 *     fh: File handle to read from (SysFile *).
 * [Return value]
 *     Value of byte read, or -1 on error.
 */
static int read_at_thread(void *fh_)
{
    SysFile *fh = (SysFile *)fh_;

    unsigned char ch;
    if (sys_file_read_at(fh, &ch, 1, 0) != 1) {
        DLOG("sys_file_read_at() failed: %s", sys_last_errstr());
        return -1;
    }
    return ch;
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_windows_files)

TEST_INIT(init)
{
    CHECK_TRUE(sys_file_init());
    CHECK_TRUE(thread_init());
    return 1;
}

TEST_CLEANUP(cleanup)
{
    thread_cleanup();
    sys_file_cleanup();
    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_open_multiple_slashes)
{
    SysFile *fh;

    CHECK_TRUE(fh = wrap_sys_file_open("testdata////////////test.txt"));
    sys_file_close(fh);
    CHECK_TRUE(fh = wrap_sys_file_open("testdata////////DIR1//dir2//File.Txt"));
    sys_file_close(fh);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_buffer_overflow)
{
    char buf[4100];  // Larger than the buffer in sys_file_open().
    strcpy(buf, "testdata/");  // Safe.
    for (size_t i = 9; i < sizeof(buf)-20; i += 2) {
        strcat(buf, "./");  // Safe (if slow).
    }
    strcat(buf, "test.txt");  // Safe.
    CHECK_FALSE(wrap_sys_file_open(buf));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_wait_delayed)
{
    PipeThreadData data;
    ASSERT(strformat_check(data.path, sizeof(data.path),
                           "\\\\.\\pipe\\SIL-%u", GetCurrentProcessId()));
    ASSERT(data.create_sema = semaphore_create(0, 1));
    ASSERT(data.write_sema = semaphore_create(0, 1));
    ASSERT(data.done_sema = semaphore_create(0, 1));
    ASSERT(data.close_sema = semaphore_create(0, 1));

    int piper;
    ASSERT(piper = thread_create(pipe_thread, &data));
    semaphore_wait(data.create_sema);

    SysFile *fh;
    CHECK_TRUE(fh = sys_file_open(data.path));

    int req;
    char ch;
    CHECK_TRUE(req = sys_file_read_async(fh, &ch, 1, 0, -1));
    CHECK_FALSE(sys_file_poll_async(req));

    semaphore_signal(data.write_sema);
    CHECK_INTEQUAL(sys_file_wait_async(req), 1);
    CHECK_INTEQUAL(ch, 'a');

    sys_file_close(fh);
    semaphore_signal(data.close_sema);
    CHECK_TRUE(thread_wait(piper));

    semaphore_destroy(data.create_sema);
    semaphore_destroy(data.write_sema);
    semaphore_destroy(data.done_sema);
    semaphore_destroy(data.close_sema);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_poll_delayed)
{
    PipeThreadData data;
    ASSERT(strformat_check(data.path, sizeof(data.path),
                           "\\\\.\\pipe\\SIL-%u", GetCurrentProcessId()));
    ASSERT(data.create_sema = semaphore_create(0, 1));
    ASSERT(data.write_sema = semaphore_create(0, 1));
    ASSERT(data.done_sema = semaphore_create(0, 1));
    ASSERT(data.close_sema = semaphore_create(0, 1));

    int piper;
    ASSERT(piper = thread_create(pipe_thread, &data));
    semaphore_wait(data.create_sema);

    SysFile *fh;
    CHECK_TRUE(fh = sys_file_open(data.path));

    int req;
    char ch;
    CHECK_TRUE(req = sys_file_read_async(fh, &ch, 1, 0, -1));
    CHECK_FALSE(sys_file_poll_async(req));

    semaphore_signal(data.write_sema);
    for (int try = 0; !sys_file_poll_async(req); try++) {
        if (try >= 10000) {
            FAIL("sys_file_poll_async(req) was not true after %d iterations",
                 try);
        }
        sys_thread_yield();
    }

    CHECK_INTEQUAL(sys_file_wait_async(req), 1);
    CHECK_INTEQUAL(ch, 'a');

    sys_file_close(fh);
    semaphore_signal(data.close_sema);
    CHECK_TRUE(thread_wait(piper));

    semaphore_destroy(data.create_sema);
    semaphore_destroy(data.write_sema);
    semaphore_destroy(data.done_sema);
    semaphore_destroy(data.close_sema);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_read_at_delayed)
{
    PipeThreadData data;
    ASSERT(strformat_check(data.path, sizeof(data.path),
                           "\\\\.\\pipe\\SIL-%u", GetCurrentProcessId()));
    ASSERT(data.create_sema = semaphore_create(0, 1));
    ASSERT(data.write_sema = semaphore_create(0, 1));
    ASSERT(data.done_sema = semaphore_create(0, 1));
    ASSERT(data.close_sema = semaphore_create(0, 1));

    int piper;
    ASSERT(piper = thread_create(pipe_thread, &data));
    semaphore_wait(data.create_sema);
    Sleep(10);  // In case the thread needs more time to hit the connect call.

    SysFile *fh;
    CHECK_TRUE(fh = sys_file_open(data.path));

    int reader;
    ASSERT(reader = thread_create(read_at_thread, fh));
    Sleep(10);  // Give the read thread a chance to start the read operation.

    semaphore_signal(data.write_sema);
    CHECK_INTEQUAL(thread_wait(reader), 'a');

    sys_file_close(fh);
    semaphore_signal(data.close_sema);
    CHECK_TRUE(thread_wait(piper));

    semaphore_destroy(data.create_sema);
    semaphore_destroy(data.write_sema);
    semaphore_destroy(data.done_sema);
    semaphore_destroy(data.close_sema);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_dir_open_multiple_slashes)
{
    SysDir *d;

    CHECK_TRUE(d = wrap_sys_dir_open("testdata////////DIR1"));
    sys_dir_close(d);
    CHECK_TRUE(d = wrap_sys_dir_open("testdata////////DIR1//dir2"));
    sys_dir_close(d);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_dir_open_trailing_slash)
{
    SysDir *d;

    CHECK_TRUE(d = wrap_sys_dir_open("testdata/DIR1/"));
    sys_dir_close(d);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_dir_open_trailing_multiple_slashes)
{
    SysDir *d;

    CHECK_TRUE(d = wrap_sys_dir_open("testdata////////"));
    sys_dir_close(d);

    return 1;
}

/*************************************************************************/
/*************************************************************************/
