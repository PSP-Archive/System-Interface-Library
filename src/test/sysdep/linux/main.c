/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/linux/main.c: Tests for the Linux main() function.
 */

#include "src/base.h"
#include "src/main.h"
#include "src/sysdep/linux/internal.h"
#include "src/sysdep/posix/path_max.h"
#include "src/test/base.h"
#include "src/test/sysdep/linux/wrap-io.h"

#include <unistd.h>

/* Technically this should be "char **", but "const char **" is more
 * convenient for our purposes. */
extern int main(int argc, const char **argv);

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Path to return from readlink(), or NULL to return EACCES. */
static const char *readlink_path;

/* Number of times stub_main() was called. */
static int main_called = 0;
/* Arguments to the most recent invocation of stub_main(). */
static int main_argc = 0;
static const char *main_argv0 = NULL;
static const char *main_argv1 = NULL;

/*************************************************************************/
/***************************** I/O overrides *****************************/
/*************************************************************************/

static ssize_t test_main_readlink(UNUSED const char *pathname, char *buf,
                                  size_t bufsiz)
{
    if (readlink_path) {
        const int len = ubound(strlen(readlink_path), bufsiz);
        memcpy(buf, readlink_path, len);
        return len;
    } else {
        errno = EACCES;
        return -1;
    }
}

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

/**
 * stub_main:  Stub replacement for sil_main() which records the fact that
 * it was called and saves argc and argv[0..1] for checking by tests.
 */
static int stub_main(int argc, const char **argv)
{
    main_called++;
    main_argc = argc;
    main_argv0 = NULL;
    main_argv1 = NULL;
    if (argv) {
        main_argv0 = argv[0];
        if (argc > 0) {
            main_argv1 = argv[1];
        }
    }
    return EXIT_SUCCESS;
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_linux_main)

/*-----------------------------------------------------------------------*/

TEST_INIT(init)
{
    clear_io_wrapper_variables();

    TEST_override_sil_main(stub_main);
    main_called = 0;
    main_argc = 0;
    main_argv0 = NULL;
    main_argv1 = NULL;

    ASSERT(linux_x11_display());
    linux_close_display();

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
    clear_io_wrapper_variables();

    TEST_override_sil_main(NULL);

    if (!linux_x11_display()) {
        /* Do this first so we don't get crashes later if the test fails. */
        ASSERT(linux_open_display());
    }

    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_display_closed_on_exit)
{
    const int exitcode = main(1, (const char *[]){"program", NULL});
    CHECK_INTEQUAL(exitcode, 0);
    CHECK_INTEQUAL(main_called, 1);
    CHECK_INTEQUAL(main_argc, 1);
    CHECK_STREQUAL(main_argv0, "program");
    CHECK_STREQUAL(main_argv1, NULL);
    CHECK_FALSE(linux_x11_display());

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_no_DISPLAY)
{
    const char *saved_DISPLAY = getenv("DISPLAY");
    ASSERT(unsetenv("DISPLAY") == 0);
    const int exitcode = main(1, (const char *[]){"program", NULL});
    ASSERT(setenv("DISPLAY", saved_DISPLAY, 1) == 0);
    CHECK_INTEQUAL(exitcode, 2);
    CHECK_INTEQUAL(main_called, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_readlink)
{
    override_readlink = test_main_readlink;
    readlink_path = "/absolute/path/to/program";
    const int exitcode = main(1, (const char *[]){"program", NULL});
    CHECK_STREQUAL(linux_executable_dir(), "/absolute/path/to");
    CHECK_INTEQUAL(exitcode, 0);
    CHECK_INTEQUAL(main_called, 1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_readlink_failure)
{
    override_readlink = test_main_readlink;
    readlink_path = NULL;
    const int exitcode = main(1, (const char *[]){"program", NULL});
    CHECK_STREQUAL(linux_executable_dir(), ".");
    CHECK_INTEQUAL(exitcode, 0);
    CHECK_INTEQUAL(main_called, 1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_readlink_overlong_name)
{
    char buf[PATH_MAX+2];  // "/a/aaa...aaa"
    memset(buf, 'a', sizeof(buf));
    buf[0] = '/';
    buf[2] = '/';
    buf[sizeof(buf)-1] = '\0';
    override_readlink = test_main_readlink;
    readlink_path = buf;
    const int exitcode = main(1, (const char *[]){"program", NULL});
    CHECK_STREQUAL(linux_executable_dir(), ".");
    CHECK_INTEQUAL(exitcode, 0);
    CHECK_INTEQUAL(main_called, 1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_readlink_not_absolute)
{
    override_readlink = test_main_readlink;
    readlink_path = "dir/file";
    const int exitcode = main(1, (const char *[]){"program", NULL});
    CHECK_STREQUAL(linux_executable_dir(), ".");
    CHECK_INTEQUAL(exitcode, 0);
    CHECK_INTEQUAL(main_called, 1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_argc_zero)
{
    const int exitcode = main(0, (const char *[]){NULL});
    CHECK_INTEQUAL(exitcode, 0);
    CHECK_INTEQUAL(main_called, 1);
    CHECK_INTEQUAL(main_argc, 1);
    CHECK_TRUE(main_argv0);
    CHECK_STREQUAL(main_argv1, NULL);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_argv0_NULL)
{
    const int exitcode = main(1, (const char *[]){NULL, NULL});
    CHECK_INTEQUAL(exitcode, 0);
    CHECK_INTEQUAL(main_called, 1);
    CHECK_INTEQUAL(main_argc, 1);
    CHECK_TRUE(main_argv0);
    CHECK_STREQUAL(main_argv1, NULL);

    return 1;
}

/*************************************************************************/
/*************************************************************************/
