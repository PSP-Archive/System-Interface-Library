/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/main.c: Tests for the sil__main() program entry point.
 */

#include "src/base.h"
#include "src/main.h"
#include "src/sysdep/test.h"
#include "src/test/base.h"

#ifdef _POSIX_C_SOURCE
# include "src/sysdep/posix/files.h"
#endif

/*************************************************************************/
/*********************** Helper routines and data ************************/
/*************************************************************************/

/* Count of calls to stub_main(). */
static int main_call_count;

/* Last arguments passed to stub_main(). */
static int last_argc;
static const char **last_argv;

/*-----------------------------------------------------------------------*/

/**
 * stub_main:  Stub replacement for sil_main() which records its arguments
 * and updates the call counter.
 *
 * [Parameters]
 *     As for sil_main().
 * [Return value]
 *     EXIT_FAILURE if argv[1] is "-fail"; EXIT_SUCCESS otherwise.
 */
static int stub_main(int argc, const char **argv)
{
    last_argc = argc;
    last_argv = argv;
    main_call_count++;
    if (argc >= 2 && strcmp(argv[1], "-fail") == 0) {
        return EXIT_FAILURE;
    } else {
        return EXIT_SUCCESS;
    }
}

/*-----------------------------------------------------------------------*/

/**
 * call_main:  Call sil__main() with the given arguments.
 *
 * [Parameters]
 *     argv0: argv[0] string for sil__main().
 *     ...: Command line argument strings for sil__main(), terminated
 *         with NULL.  A maximum of 3 non-NULL strings is allowed.
 * [Return value]
 *     Return value of sil__main().
 */
static int call_main(const char *argv0, ...)
{
    /* argv is declared static so test routines can read from it (via
     * last_argv) after this function returns. */
    static const char *argv[5];
    int argc = 0;

    argv[argc++] = argv0;
    va_list args;
    va_start(args, argv0);
    do {
        ASSERT(argc < lenof(argv));
        argv[argc++] = va_arg(args, const char *);
    } while (argv[argc-1] != NULL);

    TEST_override_sil_main(stub_main);
    const int retval = sil__main(argc-1, argv);
    TEST_override_sil_main(NULL);
    return retval;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_main)

TEST_INIT(init)
{
    main_call_count = 0;
    last_argc = 0;
    last_argv = NULL;
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_call_main)
{
    CHECK_INTEQUAL(call_main("SIL", NULL), 0);
    CHECK_INTEQUAL(main_call_count, 1);
    CHECK_INTEQUAL(last_argc, 1);
    CHECK_STREQUAL(last_argv[0], "SIL");
    CHECK_STREQUAL(last_argv[1], NULL);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_call_main_with_args)
{
    CHECK_INTEQUAL(call_main("SIL", "abc", "123", NULL), 0);
    CHECK_INTEQUAL(main_call_count, 1);
    CHECK_INTEQUAL(last_argc, 3);
    CHECK_STREQUAL(last_argv[0], "SIL");
    CHECK_STREQUAL(last_argv[1], "abc");
    CHECK_STREQUAL(last_argv[2], "123");
    CHECK_STREQUAL(last_argv[3], NULL);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_init_failure)
{
    int res;

    /* Bump up the iteration count mainly for graphics_init(), as in
     * src/test/graphics/base.c. */
    CHECK_MEMORY_FAILURES_TO(1000, (res = call_main("SIL", NULL)) != 2);
    CHECK_INTEQUAL(res, 0);

    CHECK_MEMORY_FAILURES_TO(1000,
                             (res = call_main("SIL", "-fail", NULL)) != 2);
    CHECK_INTEQUAL(res, 1);

    /* input_init() using the test sysdep stubs doesn't allocate memory,
     * so we have to check it separately. */
    sys_test_input_fail_init();
    CHECK_INTEQUAL(call_main("SIL", NULL), 2);

    /* Similarly, we currently have no sys_file implementation that
     * allocates memory, so we force a failure (only implemented for POSIX). */
#ifdef _POSIX_C_SOURCE
    TEST_posix_file_fail_init = 1;
    CHECK_INTEQUAL(call_main("SIL", NULL), 2);
#endif

    return 1;
}

/*************************************************************************/
/*************************************************************************/
