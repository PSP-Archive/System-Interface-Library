/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/windows/main.c: Tests for the Windows WinMain() function.
 */

#include "src/base.h"
#include "src/main.h"
#include "src/memory.h"
#include "src/sysdep/windows/internal.h"
#include "src/test/base.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* String to return as the command line, or NULL to use the actual command
 * line from the system. */
uint16_t *command_line = NULL;

/* Number of times stub_main() was called. */
static int main_called = 0;
/* Arguments to the most recent invocation of stub_main(). */
static int main_argc = 0;
static char *main_argv0 = NULL;  // These are locally-allocated copies.
static char *main_argv1 = NULL;
static char *main_argv2 = NULL;
static char *main_argv3 = NULL;

/*************************************************************************/
/*********************** GetCommandLineW() wrapper ***********************/
/*************************************************************************/

__declspec(dllexport) LPWSTR WINAPI GetCommandLineW(void)
{
    LPWSTR (WINAPI *p_GetCommandLineW)(void);
    HMODULE kernel32 = GetModuleHandle("kernel32.dll");
    ASSERT(kernel32);
    p_GetCommandLineW = (void *)GetProcAddress(kernel32, "GetCommandLineW");
    ASSERT(p_GetCommandLineW);

    if (command_line) {
        return command_line;
    } else {
        return (*p_GetCommandLineW)();
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
    mem_free(main_argv0);
    main_argv0 = NULL;
    mem_free(main_argv1);
    main_argv1 = NULL;
    mem_free(main_argv2);
    main_argv2 = NULL;
    mem_free(main_argv3);
    main_argv3 = NULL;
    TEST_mem_fail_after(-1, 0, 0);
    if (argv) {
        main_argv0 = argv[0] ? mem_strdup(argv[0], 0) : NULL;
        if (argc > 0) {
            main_argv1 = argv[1] ? mem_strdup(argv[1], 0) : NULL;
            if (argc > 1) {
                main_argv2 = argv[2] ? mem_strdup(argv[2], 0) : NULL;
                if (argc > 2) {
                    main_argv3 = argv[3] ? mem_strdup(argv[3], 0) : NULL;
                }
            }
        }
    }
    return EXIT_SUCCESS;
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_windows_main)

/*-----------------------------------------------------------------------*/

TEST_INIT(init)
{
    command_line = NULL;

    TEST_windows_no_main_abort = 1;
    TEST_override_sil_main(stub_main);
    main_called = 0;
    main_argc = 0;
    main_argv0 = NULL;
    main_argv1 = NULL;
    main_argv2 = NULL;
    main_argv3 = NULL;

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
    TEST_windows_no_main_abort = 0;
    TEST_override_sil_main(NULL);
    mem_free(main_argv0);
    main_argv0 = NULL;
    mem_free(main_argv1);
    main_argv1 = NULL;
    mem_free(main_argv2);
    main_argv2 = NULL;
    mem_free(main_argv3);
    main_argv3 = NULL;

    command_line = NULL;

    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_unicode_command_line)
{
    command_line = (uint16_t[]){'t','e','s','t',L'。','\0'};
    /* Our WinMain() implementation doesn't actually use any of the
     * parameters, so we just pass zeroes for simplicity. */
    const int exitcode = WinMain(0, 0, 0, 0);
    CHECK_INTEQUAL(exitcode, 0);
    CHECK_INTEQUAL(main_called, 1);
    CHECK_INTEQUAL(main_argc, 1);
    CHECK_STREQUAL(main_argv0, "test。");
    CHECK_STREQUAL(main_argv1, NULL);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_argv_splitting)
{
    command_line = (uint16_t[]){'t','e','s','t',' ',
                                '"','o','n','e',' ','t','w','o','"',' ',
                                't','h','r','e','e','\0'};
    const int exitcode = WinMain(0, 0, 0, 0);
    CHECK_INTEQUAL(exitcode, 0);
    CHECK_INTEQUAL(main_called, 1);
    CHECK_INTEQUAL(main_argc, 3);
    CHECK_STREQUAL(main_argv0, "test");
    CHECK_STREQUAL(main_argv1, "one two");
    CHECK_STREQUAL(main_argv2, "three");
    CHECK_STREQUAL(main_argv3, NULL);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_argv_memory_failure)
{
    command_line = (uint16_t[]){'t','e','s','t',' ','a','r','g','\0'};
    int exitcode;

    TEST_mem_fail_after(  // Fails the argv allocation.
        windows_version_is_at_least(WINDOWS_VERSION_8_1) ? 3 : 4, 0, 0);
    exitcode = WinMain(0, 0, 0, 0);
    TEST_mem_fail_after(-1, 0, 0);
    CHECK_INTEQUAL(exitcode, 0);
    CHECK_INTEQUAL(main_called, 1);
    CHECK_INTEQUAL(main_argc, 1);
    CHECK_STREQUAL(main_argv0, "SIL");
    CHECK_STREQUAL(main_argv1, NULL);

    /* init_all() might need a lot of iterations. */
    CHECK_MEMORY_FAILURES_TO(1000,
                             (main_called = 0, WinMain(0, 0, 0, 0),
                              main_argv0 && strcmp(main_argv0, "test") == 0));
    CHECK_INTEQUAL(exitcode, 0);
    CHECK_INTEQUAL(main_called, 1);
    CHECK_INTEQUAL(main_argc, 2);
    CHECK_STREQUAL(main_argv1, "arg");

    return 1;
}

/*************************************************************************/
/*************************************************************************/
