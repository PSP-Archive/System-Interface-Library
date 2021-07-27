/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/misc/log-stdio.c: Tests for the stdio-based log writing
 * implementation.  Currently, this implementation is only used on POSIX
 * systems, so we make use of POSIX functions in the tests.
 */

#include "src/base.h"
#include "src/sysdep.h"
#include "src/sysdep/posix/fileutil.h"
#include "src/sysdep/test.h"
#include "src/test/base.h"
#include "src/thread.h"
#include "src/userdata.h"

#include <stdio.h>

#ifdef _POSIX_C_SOURCE
# include <sys/stat.h>
#endif

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_misc_log_stdio)

/*-----------------------------------------------------------------------*/

TEST_INIT(init)
{
    CHECK_TRUE(thread_init());
    CHECK_TRUE(sys_file_init());
    sys_test_userdata_use_live_routines = 1;
    CHECK_TRUE(userdata_init());
    userdata_set_program_name("SIL-log-test");

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
    const char *dir = userdata_get_data_path();
    if (dir) {
        remove(dir);
    }

    userdata_cleanup();
    sys_test_userdata_use_live_routines = 0;
    sys_file_cleanup();
    thread_cleanup();

    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_log_open_no_user_data_path)
{
    /* Resetting the core userdata code to an uninitialized state will
     * cause userdata_get_data_path() to always return NULL, so
     * sys_log_open() should fail. */
    userdata_cleanup();
    ASSERT(!userdata_get_data_path());
    CHECK_FALSE(sys_log_open("test.log"));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_log_open_memory_faliure)
{
    void *fh;
    CHECK_MEMORY_FAILURES(fh = sys_log_open("test.log"));
    sys_log_close(fh);

    char namebuf[1000];
    ASSERT(strformat_check(namebuf, sizeof(namebuf),
                           "%stest.log", userdata_get_data_path()));
    ASSERT(remove(namebuf) == 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_log_open_buffer_overflow)
{
    char namebuf[1002];
    for (int i = 0; i < 1000; i += 2) {
        namebuf[i] = '.';
        namebuf[i+1] = '/';
    }
    namebuf[1000] = 'a';
    namebuf[1001] = '\0';
    CHECK_FALSE(sys_log_open(namebuf));

    /* The file may have been created by userdata_save_data(), so remove it. */
    ASSERT(strformat_check(namebuf, sizeof(namebuf),
                           "%sa", userdata_get_data_path()));
    errno = 0;
    ASSERT(remove(namebuf) == 0 || errno == ENOENT);

    return 1;
}

/*-----------------------------------------------------------------------*/

#ifdef _POSIX_C_SOURCE  // Requires chmod().

TEST(test_log_open_unwritable_dir)
{
    const char *dir;
    ASSERT(dir = userdata_get_data_path());

    char pathbuf[1000];
    int len = strformat(pathbuf, sizeof(pathbuf), "%sdir", dir);
    ASSERT(len < (int)sizeof(pathbuf));
    ASSERT(posix_mkdir_p(pathbuf));
    ASSERT(chmod(pathbuf, 0555) == 0);

    CHECK_FALSE(sys_log_open("dir/test.log"));

    ASSERT(remove(pathbuf) == 0);
    return 1;
}

#endif  // _POSIX_C_SOURCE

/*************************************************************************/
/*************************************************************************/
