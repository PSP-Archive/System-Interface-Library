/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/linux/meminfo.c: Tests for Linux memory information
 * collection functions.
 */

#include "src/base.h"
#include "src/sysdep/linux/meminfo.h"
#include "src/test/base.h"
#include "src/test/sysdep/linux/wrap-io.h"

#include <fcntl.h>
#include <unistd.h>

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Path on which our simulated file should appear. */
static const char *open_path;
/* Simulated file contents to return from read(), as a string. */
static const char *read_data;
/* Errors to return from open() and read(), respectively, or 0 for normal
 * behavior. */
static int open_errno;
static int read_errno;

/* File descriptor for the simulated file, or -1 if not open. */
static int opened_fd;

/*************************************************************************/
/***************************** I/O overrides *****************************/
/*************************************************************************/

static int test_meminfo_open(const char *pathname, int flags, ...)
{
    if (strcmp(pathname, open_path) != 0) {
        va_list args;
        va_start(args, flags);
        int mode = va_arg(args, int);
        return trampoline_open(pathname, flags, mode);
    }

    ASSERT(opened_fd < 0);
    if (open_errno) {
        errno = open_errno;
        return -1;
    }
    ASSERT((opened_fd = open("/dev/null", O_RDWR)) >= 0);
    return opened_fd;
}

/*-----------------------------------------------------------------------*/

static int test_meminfo_close(int fd)
{
    if (fd == opened_fd) {
        /* Save the FD aside and reset opened_fd before calling close()
         * to avoid infinite recursion without having to manually call the
         * wrapped function pointer. */
        const int fd_to_close = opened_fd;
        opened_fd = -1;
        close(fd_to_close);
        return 0;
    } else {
        return trampoline_close(fd);
    }
}

/*-----------------------------------------------------------------------*/

static ssize_t test_meminfo_read(int fd, void *buf, size_t count)
{
    if (fd == opened_fd) {
        if (read_errno) {
            errno = read_errno;
            return -1;
        }
        const size_t data_len = strlen(read_data);
        const size_t read_len = ubound(count, data_len);
        memcpy(buf, read_data, read_len);
        return read_len;
    } else {
        return trampoline_read(fd, buf, count);
    }
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_linux_meminfo)

/*-----------------------------------------------------------------------*/

TEST_INIT(init)
{
    clear_io_wrapper_variables();
    override_open = test_meminfo_open;
    override_close = test_meminfo_close;
    override_read = test_meminfo_read;
    open_path = "";
    read_data = "";
    open_errno = 0;
    read_errno = 0;
    opened_fd = -1;

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
    clear_io_wrapper_variables();
    CHECK_INTEQUAL(opened_fd, -1);

    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_get_total_memory)
{
    open_path = "/proc/meminfo";
    /* Normally MemTotal comes first, but we put it second to exercise the
     * code to process multiple lines. */
    read_data = ("MemFree:  2000000 kB\n"
                 "MemTotal: 8000000 kB\n"
                 "Buffers:  1000000 kB\n"
                 "Cached:   3000000 kB\n");
    CHECK_INTEQUAL(linux_get_total_memory(), INT64_C(8192000000));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_total_memory_open_error)
{
    open_path = "/proc/meminfo";
    open_errno = EACCES;
    CHECK_INTEQUAL(linux_get_total_memory(), 0);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_total_memory_read_error)
{
    open_path = "/proc/meminfo";
    read_errno = EINVAL;
    CHECK_INTEQUAL(linux_get_total_memory(), 0);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_total_memory_missing_memtotal)
{
    open_path = "/proc/meminfo";
    read_data = ("MemFree:  2000000 kB\n"
                 "Buffers:  1000000 kB\n"
                 "Cached:   3000000 kB\n"
                 "SwapCached:  1000 kB\n");
    CHECK_INTEQUAL(linux_get_total_memory(), 0);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_total_memory_truncated_line)
{
    char buf[1024+6];
    ASSERT(strformat_check(buf, sizeof(buf), "MemTotal: %*d kB\n",
                           sizeof(buf)-10-5, 8000000));
    open_path = "/proc/meminfo";
    read_data = buf;
    CHECK_INTEQUAL(linux_get_total_memory(), 0);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_process_size)
{
    open_path = "/proc/self/status";
    read_data = ("Name:  foo\n"
                 "VmRSS: 5000000 kB\n");
    CHECK_INTEQUAL(linux_get_process_size(), INT64_C(5120000000));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_process_size_open_error)
{
    open_path = "/proc/self/status";
    open_errno = EACCES;
    CHECK_INTEQUAL(linux_get_process_size(), 0);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_process_size_read_error)
{
    open_path = "/proc/self/status";
    read_errno = EINVAL;
    CHECK_INTEQUAL(linux_get_process_size(), 0);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_process_size_missing_vmrss)
{
    open_path = "/proc/self/status";
    read_data = "Name: foo\n";
    CHECK_INTEQUAL(linux_get_process_size(), 0);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_process_size_truncated_line)
{
    char buf[1024+6];
    ASSERT(strformat_check(buf, sizeof(buf), "VmRss: %*d kB\n",
                           sizeof(buf)-7-5, 5000000));
    open_path = "/proc/self/status";
    read_data = buf;
    CHECK_INTEQUAL(linux_get_process_size(), 0);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_free_memory)
{
    open_path = "/proc/meminfo";
    read_data = ("MemTotal: 8000000 kB\n"
                 "MemFree:  2000000 kB\n"
                 "Buffers:  1000000 kB\n"
                 "Cached:   3000000 kB\n"
                 "SwapCached:  1000 kB\n");
    CHECK_INTEQUAL(linux_get_free_memory(), INT64_C(6144000000));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_free_memory_open_error)
{
    open_path = "/proc/meminfo";
    open_errno = EACCES;
    CHECK_INTEQUAL(linux_get_free_memory(), 0);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_free_memory_read_error)
{
    open_path = "/proc/meminfo";
    read_errno = EINVAL;
    CHECK_INTEQUAL(linux_get_free_memory(), 0);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_free_memory_missing_memfree)
{
    open_path = "/proc/meminfo";
    read_data = ("MemTotal: 8000000 kB\n"
                 "Buffers:  1000000 kB\n"
                 "Cached:   3000000 kB\n"
                 "SwapCached:  1000 kB\n");
    CHECK_INTEQUAL(linux_get_free_memory(), 0);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_free_memory_missing_buffers)
{
    open_path = "/proc/meminfo";
    read_data = ("MemTotal: 8000000 kB\n"
                 "MemFree:  2000000 kB\n"
                 "Cached:   3000000 kB\n"
                 "SwapCached:  1000 kB\n");
    CHECK_INTEQUAL(linux_get_free_memory(), 0);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_free_memory_missing_cached)
{
    open_path = "/proc/meminfo";
    read_data = ("MemTotal: 8000000 kB\n"
                 "MemFree:  2000000 kB\n"
                 "Buffers:  1000000 kB\n"
                 "SwapCached:  1000 kB\n");
    CHECK_INTEQUAL(linux_get_free_memory(), 0);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_free_memory_truncated_line)
{
    char buf[1024+6];
    ASSERT(strformat_check(buf, sizeof(buf), ("MemTotal: 8000000 kB\n"
                                              "MemFree:  2000000 kB\n"
                                              "Buffers:  1000000 kB\n"
                                              "Cached:   %*d kB\n"),
                           sizeof(buf)-73-5, 3000000));
    open_path = "/proc/meminfo";
    read_data = buf;
    CHECK_INTEQUAL(linux_get_free_memory(), 0);
    return 1;
}

/*************************************************************************/
/*************************************************************************/
