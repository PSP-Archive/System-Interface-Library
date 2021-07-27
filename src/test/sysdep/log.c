/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/log.c: Tests for the system-level log writing functions
 * and the core log_to_file() function.
 */

#include "src/base.h"
#include "src/sysdep.h"
#include "src/test/base.h"
#include "src/thread.h"

#if defined(SIL_PLATFORM_ANDROID)
# include "src/sysdep/android/internal.h"
# include <unistd.h>
#elif defined(SIL_PLATFORM_IOS)
# include "src/sysdep/ios/util.h"
# include <unistd.h>
#elif defined(SIL_PLATFORM_PSP)
# include "src/sysdep/psp/internal.h"
#elif defined(SIL_PLATFORM_LINUX) || defined(SIL_PLATFORM_MACOSX)
# include "src/sysdep/test.h"
# include "src/userdata.h"
# include <unistd.h>
#elif defined(SIL_PLATFORM_WINDOWS)
# include "src/sysdep/test.h"
# include "src/sysdep/windows/internal.h"
# include "src/userdata.h"
#endif

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

/**
 * log_directory:  Return the pathname of the directory to which log files
 * are written.
 */
static const char *log_directory(void)
{
#if defined(SIL_PLATFORM_ANDROID)
    return android_external_data_path;
#elif defined(SIL_PLATFORM_IOS)
    return ios_get_application_support_path();
#elif defined(SIL_PLATFORM_PSP)
    return psp_executable_dir();
#elif defined(SIL_PLATFORM_LINUX) || defined(SIL_PLATFORM_MACOSX) || defined(SIL_PLATFORM_WINDOWS)
    const char *dir = userdata_get_data_path();
    ASSERT(dir);
    return dir;
#else
# error Log directory unknown on this system!
#endif
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_sys_log)

/*-----------------------------------------------------------------------*/

TEST_INIT(init)
{
    CHECK_TRUE(thread_init());
    CHECK_TRUE(sys_file_init());

#if defined(SIL_PLATFORM_LINUX) || defined(SIL_PLATFORM_MACOSX) || defined(SIL_PLATFORM_WINDOWS)
    sys_test_userdata_use_live_routines = 1;
    CHECK_TRUE(userdata_init());
    userdata_set_program_name("SIL-log-test");
#endif

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
    const char *dir = log_directory();
    char pathbuf[1000];
    ASSERT(strformat_check(pathbuf, sizeof(pathbuf), "%s%stest.log",
                           dir, dir[strlen(dir)-1]=='/' ? "" : "/"));
#if defined(SIL_PLATFORM_ANDROID) || defined(SIL_PLATFORM_IOS) || defined(SIL_PLATFORM_LINUX) || defined(SIL_PLATFORM_MACOSX)
    unlink(pathbuf);
    rmdir(dir);
#elif defined(SIL_PLATFORM_PSP)
    sceIoRemove(pathbuf);
#elif defined(SIL_PLATFORM_WINDOWS)
    for (char *s = pathbuf; *s; s++) {
        if (*s == '/') {
            *s = '\\';
        }
    }
    DeleteFile(pathbuf);
    pathbuf[strlen(pathbuf)-9] = '\0';  // Also delete the trailing backslash.
    RemoveDirectory(pathbuf);
#else
# error Cleanup code missing for this system!
#endif

#if defined(SIL_PLATFORM_LINUX) || defined(SIL_PLATFORM_MACOSX) || defined(SIL_PLATFORM_WINDOWS)
    userdata_cleanup();
    sys_test_userdata_use_live_routines = 0;
#endif

    sys_file_cleanup();
    thread_cleanup();

    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_log_write)
{
    void *logfh;
    CHECK_TRUE(logfh = sys_log_open("test.log"));
    sys_log_write(logfh, "test 123\n", 8);
    sys_log_close(logfh);

    const char *dir = log_directory();
    char pathbuf[1000];
    ASSERT(strformat_check(pathbuf, sizeof(pathbuf), "%s%stest.log",
                           dir, dir[strlen(dir)-1]=='/' ? "" : "/"));
    SysFile *fh;
    CHECK_TRUE(fh = sys_file_open(pathbuf));
#ifdef SIL_PLATFORM_WINDOWS
    char buf[11];
#else
    char buf[10];
#endif
    CHECK_INTEQUAL(sys_file_read(fh, buf, sizeof(buf)), sizeof(buf)-1);
    buf[sizeof(buf)-1] = '\0';
#ifdef SIL_PLATFORM_WINDOWS
    CHECK_STREQUAL(buf, "test 123\r\n");
#else
    CHECK_STREQUAL(buf, "test 123\n");
#endif
    sys_file_close(fh);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_log_write_empty)
{
    void *logfh;
    CHECK_TRUE(logfh = sys_log_open("test.log"));
    sys_log_write(logfh, "\n", 0);
    sys_log_close(logfh);

    const char *dir = log_directory();
    char pathbuf[1000];
    ASSERT(strformat_check(pathbuf, sizeof(pathbuf), "%s%stest.log",
                           dir, dir[strlen(dir)-1]=='/' ? "" : "/"));
    SysFile *fh;
    CHECK_TRUE(fh = sys_file_open(pathbuf));
#ifdef SIL_PLATFORM_WINDOWS
    char buf[3];
#else
    char buf[2];
#endif
    CHECK_INTEQUAL(sys_file_read(fh, buf, sizeof(buf)), sizeof(buf)-1);
    buf[sizeof(buf)-1] = '\0';
#ifdef SIL_PLATFORM_WINDOWS
    CHECK_STREQUAL(buf, "\r\n");
#else
    CHECK_STREQUAL(buf, "\n");
#endif
    sys_file_close(fh);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_log_to_file)
{
    /* Fill up the log buffer so we get the "some messages lost" line.
     * Note that we still get the message even if the buffer was exactly
     * filled without overflowing (a simplification since the case is
     * unlikely to occur in the real world). */
    char fillbuf[SIL_DLOG_MAX_SIZE];
    memset(fillbuf, 'a', sizeof(fillbuf)-1);
    for (int i = 0; i < 16384; i += sizeof(fillbuf)) {
        const int linelen = ubound((int)sizeof(fillbuf), 16384 - i);
        fillbuf[linelen-1] = '\0';
        do_DLOG(NULL, 0, NULL, "%s", fillbuf);
    }

    log_to_file("test.log");
    do_DLOG(NULL, 0, NULL, "test line");
    do_DLOG(NULL, 0, NULL, "test two");
    log_to_file(NULL);

#ifdef SIL_PLATFORM_WINDOWS
    const char *expected = "[some log messages lost]\r\ntest line\r\ntest two\r\n";
#else
    const char *expected = "[some log messages lost]\ntest line\ntest two\n";
#endif
    const char *dir = log_directory();
    char pathbuf[1000];
    ASSERT(strformat_check(pathbuf, sizeof(pathbuf), "%s%stest.log",
                           dir, dir[strlen(dir)-1]=='/' ? "" : "/"));
    SysFile *fh;
    CHECK_TRUE(fh = sys_file_open(pathbuf));
    char buf[32816];  // 16384*2 (for LF->CRLF expansion) + expected text (with Windows CRLF) + 1
    const int len = sys_file_read(fh, buf, sizeof(buf));
    sys_file_close(fh);
    CHECK_TRUE(len >= (int)strlen(expected));
    CHECK_TRUE(len < (int)sizeof(buf));
    buf[len] = '\0';  // So we can do a string compare.
    CHECK_STREQUAL(buf + len - strlen(expected), expected);

    /* Reopening the file should leave it empty, though we need to log a
     * line before it's actually opened. */
    log_to_file("test.log");
    do_DLOG(NULL, 0, NULL, "test line");
    log_to_file(NULL);
    CHECK_TRUE(fh = sys_file_open(pathbuf));
#ifdef SIL_PLATFORM_WINDOWS
    CHECK_INTEQUAL(sys_file_read(fh, buf, sizeof(buf)), 11);
    buf[11] = '\0';
    CHECK_STREQUAL(buf, "test line\r\n");
#else
    CHECK_INTEQUAL(sys_file_read(fh, buf, sizeof(buf)), 10);
    buf[10] = '\0';
    CHECK_STREQUAL(buf, "test line\n");
#endif
    sys_file_close(fh);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_log_to_file_open_error)
{
    log_to_file(".");  // Will fail.
    do_DLOG(NULL, 0, NULL, "test line");  // Should be stored, not discarded.

    log_to_file("test.log");
    do_DLOG(NULL, 0, NULL, "test two");
    log_to_file(NULL);

#ifdef SIL_PLATFORM_WINDOWS
    const char *expected = "test line\r\ntest two\r\n";
#else
    const char *expected = "test line\ntest two\n";
#endif
    const char *dir = log_directory();
    char pathbuf[1000];
    ASSERT(strformat_check(pathbuf, sizeof(pathbuf), "%s%stest.log",
                           dir, dir[strlen(dir)-1]=='/' ? "" : "/"));
    SysFile *fh;
    CHECK_TRUE(fh = sys_file_open(pathbuf));
    /* There will probably be error messages related to failing to open
     * the file, so leave room for them. */
    char buf[10000];
    const int len = sys_file_read(fh, buf, sizeof(buf));
    sys_file_close(fh);
    CHECK_TRUE(len >= (int)strlen(expected));
    CHECK_TRUE(len < (int)sizeof(buf));
    buf[len] = '\0';
    CHECK_STREQUAL(buf + len - strlen(expected), expected);

    return 1;
}

/*************************************************************************/
/*************************************************************************/
