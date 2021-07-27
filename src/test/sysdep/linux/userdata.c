/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/linux/movie.c: Tests for the FFmpeg-based Linux
 * implementation of the system-level movie playback interface.
 */

#include "src/base.h"
#include "src/sysdep/test.h"
#include "src/test/base.h"
#include "src/thread.h"
#include "src/userdata.h"

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

static int do_test_linux_userdata(void);
int test_linux_userdata(void)
{
    char *saved_HOME, *saved_XDG_DATA_HOME;
    if (getenv("HOME")) {
        ASSERT(saved_HOME = mem_strdup(getenv("HOME"), 0));
        ASSERT(unsetenv("HOME") == 0);
    } else {
        saved_HOME = NULL;
    }
    if (getenv("XDG_DATA_HOME")) {
        ASSERT(saved_XDG_DATA_HOME = mem_strdup(getenv("XDG_DATA_HOME"), 0));
        ASSERT(unsetenv("XDG_DATA_HOME") == 0);
    } else {
        saved_XDG_DATA_HOME = NULL;
    }

    const int result = do_test_linux_userdata();

    if (saved_HOME) {
        ASSERT(setenv("HOME", saved_HOME, 1) == 0);
        mem_free(saved_HOME);
    } else {
        ASSERT(unsetenv("HOME") == 0);
    }
    if (saved_XDG_DATA_HOME) {
        ASSERT(setenv("XDG_DATA_HOME", saved_XDG_DATA_HOME, 1) == 0);
        mem_free(saved_XDG_DATA_HOME);
    } else {
        ASSERT(unsetenv("XDG_DATA_HOME") == 0);
    }

    return result;
}

DEFINE_GENERIC_TEST_RUNNER(do_test_linux_userdata)

/*-----------------------------------------------------------------------*/

TEST_INIT(init)
{
    ASSERT(unsetenv("HOME") == 0);
    ASSERT(unsetenv("XDG_DATA_HOME") == 0);

    sys_test_userdata_use_live_routines = 1;
    CHECK_TRUE(thread_init());
    CHECK_TRUE(userdata_init());
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
    userdata_cleanup();
    thread_cleanup();
    sys_test_userdata_use_live_routines = 0;
    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_no_env_vars)
{
    ASSERT(!getenv("HOME"));
    ASSERT(!getenv("XDG_DATA_HOME"));

    userdata_set_program_name("test");
    CHECK_STREQUAL(userdata_get_data_path(), "./test/");

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_HOME_only)
{
    ASSERT(setenv("HOME", "/home/foo", 1) == 0);
    ASSERT(!getenv("XDG_DATA_HOME"));

    userdata_set_program_name("test");
    CHECK_STREQUAL(userdata_get_data_path(), "/home/foo/.local/share/test/");

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_HOME_empty)
{
    ASSERT(setenv("HOME", "", 1) == 0);
    ASSERT(!getenv("XDG_DATA_HOME"));

    userdata_set_program_name("test");
    CHECK_STREQUAL(userdata_get_data_path(), "./test/");

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_XDG_DATA_HOME_only)
{
    ASSERT(!getenv("HOME"));
    ASSERT(setenv("XDG_DATA_HOME", "/xdg/data/home", 1) == 0);

    userdata_set_program_name("test");
    CHECK_STREQUAL(userdata_get_data_path(), "/xdg/data/home/test/");

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_XDG_DATA_HOME_empty)
{
    ASSERT(!getenv("HOME"));
    ASSERT(setenv("XDG_DATA_HOME", "", 1) == 0);

    userdata_set_program_name("test");
    CHECK_STREQUAL(userdata_get_data_path(), "./test/");

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_XDG_DATA_HOME_and_HOME)
{
    ASSERT(setenv("HOME", "/home/foo", 1) == 0);
    ASSERT(setenv("XDG_DATA_HOME", "/xdg/data/home", 1) == 0);

    userdata_set_program_name("test");
    CHECK_STREQUAL(userdata_get_data_path(), "/xdg/data/home/test/");

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_XDG_DATA_HOME_empty_and_HOME)
{
    ASSERT(setenv("HOME", "/home/foo", 1) == 0);
    ASSERT(setenv("XDG_DATA_HOME", "", 1) == 0);

    userdata_set_program_name("test");
    CHECK_STREQUAL(userdata_get_data_path(), "/home/foo/.local/share/test/");

    return 1;
}

/*************************************************************************/
/*************************************************************************/
