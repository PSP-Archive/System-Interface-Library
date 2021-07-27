/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/utility/log.c: Tests for the DLOG() interface.  log_to_file()
 * is tested separately in the sys_log tests since it depends on other
 * library components (notably userdata on PC-type platforms).
 */

#include "src/base.h"
#include "src/test/base.h"
#include "src/utility/log.h"

/*************************************************************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_utility_log)

/*-----------------------------------------------------------------------*/

TEST(test_file_path_stripping)
{
#ifdef SIL_DLOG_STRIP_PATH
    do_DLOG("file.c", 1, "function", "test");
    CHECK_STREQUAL(test_DLOG_last_message, "file.c:1(function): test");

    do_DLOG(SIL_DLOG_STRIP_PATH "file.c", 1, "function", "test");
    CHECK_STREQUAL(test_DLOG_last_message, "file.c:1(function): test");

    do_DLOG(SIL_DLOG_STRIP_PATH "dir/file.c", 1, "function", "test");
    CHECK_STREQUAL(test_DLOG_last_message, "dir/file.c:1(function): test");

    do_DLOG("/dir" SIL_DLOG_STRIP_PATH "dir2/file.c", 1, "function", "test");
    CHECK_STREQUAL(test_DLOG_last_message,
                   "/dir" SIL_DLOG_STRIP_PATH "dir2/file.c:1(function): test");
#endif

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_format_code_in_line_prefix)
{
    do_DLOG("%s.c", 0, "function", "test");
    CHECK_STREQUAL(test_DLOG_last_message, "%s.c:0(function): test");

    do_DLOG("file.%", 0, "function", "test");
    CHECK_STREQUAL(test_DLOG_last_message, "file.%:0(function): test");

    do_DLOG("file.c", 0, "%s", "test");
    CHECK_STREQUAL(test_DLOG_last_message, "file.c:0(%s): test");

    do_DLOG("file.c", 0, "%", "test");
    CHECK_STREQUAL(test_DLOG_last_message, "file.c:0(%): test");

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_no_file)
{
    do_DLOG(NULL, 1, "function", "test");
    CHECK_STREQUAL(test_DLOG_last_message, "test");

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_strip_newline)
{
    do_DLOG("file.c", 1, "function", "test\n");
    CHECK_STREQUAL(test_DLOG_last_message, "file.c:1(function): test");

    /* Only one newline should be stripped. */
    do_DLOG("file.c", 1, "function", "test\n\n");
    CHECK_STREQUAL(test_DLOG_last_message, "file.c:1(function): test\n");

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_strip_newline_empty_message)
{
    /* Make sure the newline check doesn't try to access outside the
     * buffer bounds when given an empty log string.  For this check, we
     * need a bit of hackery to avoid various format string warnings. */
    volatile char format[1];
    format[0] = '\0';
    do_DLOG(NULL, 0, NULL, (const char *)format, "");
    CHECK_STREQUAL(test_DLOG_last_message, "");

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_long_message)
{
    /* On error, this will still only overwrite a single byte somewhere
     * past the end of the output buffer (and then only if the byte has
     * value 0x0A).  Try and set up our own stack so that it'll catch the
     * bug. */
    struct {
        char test[1000];  // Should appear right above the do_DLOG() frame.
        char buf[SIL_DLOG_MAX_SIZE + 1000];
    } s;
    memset(s.buf, 'a', sizeof(s.buf)-2);
    s.buf[sizeof(s.buf)-2] = '\n';
    s.buf[sizeof(s.buf)-1] = '\0';
    memset(s.test, '\n', sizeof(s.test));
    /* Dummy format argument used to suppress -Wformat warning. */
    do_DLOG(NULL, 0, NULL, s.buf, "dummy");
    s.buf[SIL_DLOG_MAX_SIZE-1] = 0;
    CHECK_STREQUAL(test_DLOG_last_message, s.buf);
    for (int i = 0; i < (int)sizeof(s.test); i++) {
        if (s.test[i] != '\n') {
            FAIL("Byte at offset %d corrupted", i);
        }
    }

    return 1;
}

/*************************************************************************/
/*************************************************************************/
