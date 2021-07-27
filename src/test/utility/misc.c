/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/utility/misc.c: Tests for miscellaneous utility functions.
 */

#include "src/base.h"
#include "src/memory.h"
#include "src/sysdep/test.h"
#include "src/test/base.h"
#include "src/utility/misc.h"

/*************************************************************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_utility_misc)

/*-----------------------------------------------------------------------*/

TEST(test_console_printf)
{
    console_printf("This is a %s message", "test");
    CHECK_STREQUAL(sys_test_get_last_console_output(),
                   "This is a test message");
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_display_error)
{
    display_error("This is a %s error", "test");
    CHECK_STREQUAL(sys_test_get_last_displayed_error(),
                   "This is a test error");
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_system_language)
{
    const char *language, *dialect;

    sys_test_clear_languages();
    language = (const char *)1;
    dialect = (const char *)2;
    CHECK_FALSE(get_system_language(0, &language, &dialect));
    CHECK_PTREQUAL(language, (const char *)1);
    CHECK_PTREQUAL(dialect, (const char *)2);

    sys_test_set_language(0, "en", "");
    CHECK_TRUE(get_system_language(0, &language, &dialect));
    CHECK_STREQUAL(language, "en");
    CHECK_STREQUAL(dialect, "");
    CHECK_FALSE(get_system_language(1, &language, &dialect));
    /* Make sure NULLs work too. */
    CHECK_TRUE(get_system_language(0, NULL, NULL));

    sys_test_set_language(1, "fr", "FR");
    CHECK_TRUE(get_system_language(0, &language, &dialect));
    CHECK_STREQUAL(language, "en");
    CHECK_STREQUAL(dialect, "");
    CHECK_TRUE(get_system_language(1, &language, &dialect));
    CHECK_STREQUAL(language, "fr");
    CHECK_STREQUAL(dialect, "FR");
    CHECK_FALSE(get_system_language(2, &language, &dialect));

    sys_test_set_language(0, "ja", "JP");
    CHECK_TRUE(get_system_language(0, &language, &dialect));
    CHECK_STREQUAL(language, "ja");
    CHECK_STREQUAL(dialect, "JP");
    CHECK_TRUE(get_system_language(1, &language, &dialect));
    CHECK_STREQUAL(language, "fr");
    CHECK_STREQUAL(dialect, "FR");
    CHECK_FALSE(get_system_language(2, &language, &dialect));

    sys_test_clear_languages();
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_default_dialect_for_language)
{
    CHECK_STREQUAL(default_dialect_for_language("en"), "US");
    CHECK_STREQUAL(default_dialect_for_language("??"), "");
    CHECK_STREQUAL(default_dialect_for_language(""), "");
    CHECK_STREQUAL(default_dialect_for_language(NULL), "");
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_reset_idle_timer)
{
    sys_test_clear_idle_reset_flag();
    reset_idle_timer();
    CHECK_TRUE(sys_test_get_idle_reset_flag());
    sys_test_clear_idle_reset_flag();
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_performance_level)
{
    CHECK_TRUE(set_performance_level(0));  // Should always succeed.
    CHECK_FALSE(set_performance_level(PERFORMANCE_LEVEL_LOW - 1));  // Invalid.
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_file_url)
{
    CHECK_TRUE(can_open_file());
    CHECK_TRUE(can_open_url());
    CHECK_TRUE(open_file("/tmp/123"));
    CHECK_STREQUAL(sys_test_get_last_external_open_path(), "/tmp/123");
    CHECK_TRUE(open_url("http://456/"));
    CHECK_STREQUAL(sys_test_get_last_external_open_path(), "http://456/");
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_split_args)
{
    char buffer[100];
    int argc;
    char **argv;

    strcpy(buffer, "name arg1 arg2");  // Safe.
    CHECK_TRUE(split_args(buffer, 0, &argc, &argv));
    CHECK_INTEQUAL(argc, 3);
    CHECK_STREQUAL(argv[0], "name");
    CHECK_STREQUAL(argv[1], "arg1");
    CHECK_STREQUAL(argv[2], "arg2");
    CHECK_FALSE(argv[3]);
    mem_free(argv);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_split_args_insert_dummy)
{
    char buffer[100];
    int argc;
    char **argv;

    strcpy(buffer, "name arg1 arg2");  // Safe.
    CHECK_TRUE(split_args(buffer, 1, &argc, &argv));
    CHECK_INTEQUAL(argc, 4);
    CHECK_STREQUAL(argv[0], "");
    CHECK_STREQUAL(argv[1], "name");
    CHECK_STREQUAL(argv[2], "arg1");
    CHECK_STREQUAL(argv[3], "arg2");
    CHECK_FALSE(argv[4]);
    mem_free(argv);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_split_args_whitespace)
{
    char buffer[100];
    int argc;
    char **argv;

    strcpy(buffer, "\t name\n arg1\targ2\r \n");  // Safe.
    CHECK_TRUE(split_args(buffer, 0, &argc, &argv));
    CHECK_INTEQUAL(argc, 3);
    CHECK_STREQUAL(argv[0], "name");
    CHECK_STREQUAL(argv[1], "arg1");
    CHECK_STREQUAL(argv[2], "arg2");
    CHECK_FALSE(argv[3]);
    mem_free(argv);

    strcpy(buffer, "\t name\n arg1\targ2\r \n ");  // Safe.
    CHECK_TRUE(split_args(buffer, 1, &argc, &argv));
    CHECK_INTEQUAL(argc, 4);
    CHECK_STREQUAL(argv[0], "");
    CHECK_STREQUAL(argv[1], "name");
    CHECK_STREQUAL(argv[2], "arg1");
    CHECK_STREQUAL(argv[3], "arg2");
    CHECK_FALSE(argv[4]);
    mem_free(argv);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_split_args_backslash)
{
    char buffer[100];
    int argc;
    char **argv;

    strcpy(buffer, "name arg1\\ arg2");  // Safe.
    CHECK_TRUE(split_args(buffer, 0, &argc, &argv));
    CHECK_INTEQUAL(argc, 2);
    CHECK_STREQUAL(argv[0], "name");
    CHECK_STREQUAL(argv[1], "arg1 arg2");
    CHECK_FALSE(argv[2]);
    mem_free(argv);

    strcpy(buffer, "name ar\\g1 arg2");  // Safe.
    CHECK_TRUE(split_args(buffer, 0, &argc, &argv));
    CHECK_INTEQUAL(argc, 3);
    CHECK_STREQUAL(argv[0], "name");
    CHECK_STREQUAL(argv[1], "arg1");
    CHECK_STREQUAL(argv[2], "arg2");
    CHECK_FALSE(argv[3]);
    mem_free(argv);

    strcpy(buffer, "name arg1\\\narg2");  // Safe.
    CHECK_TRUE(split_args(buffer, 0, &argc, &argv));
    CHECK_INTEQUAL(argc, 2);
    CHECK_STREQUAL(argv[0], "name");
    CHECK_STREQUAL(argv[1], "arg1arg2");
    CHECK_FALSE(argv[2]);
    mem_free(argv);

    strcpy(buffer, "name arg1\\\r\narg2");  // Safe.
    CHECK_TRUE(split_args(buffer, 0, &argc, &argv));
    CHECK_INTEQUAL(argc, 2);
    CHECK_STREQUAL(argv[0], "name");
    CHECK_STREQUAL(argv[1], "arg1arg2");
    CHECK_FALSE(argv[2]);
    mem_free(argv);

    strcpy(buffer, "name arg1\\\n\rarg2");  // Safe.
    CHECK_TRUE(split_args(buffer, 0, &argc, &argv));
    CHECK_INTEQUAL(argc, 3);
    CHECK_STREQUAL(argv[0], "name");
    CHECK_STREQUAL(argv[1], "arg1");
    CHECK_STREQUAL(argv[2], "arg2");
    CHECK_FALSE(argv[3]);
    mem_free(argv);

    strcpy(buffer, "name arg1\\\rarg2");  // Safe.
    CHECK_TRUE(split_args(buffer, 0, &argc, &argv));
    CHECK_INTEQUAL(argc, 2);
    CHECK_STREQUAL(argv[0], "name");
    CHECK_STREQUAL(argv[1], "arg1\rarg2");
    CHECK_FALSE(argv[2]);
    mem_free(argv);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_split_args_single_quotes)
{
    char buffer[100];
    int argc;
    char **argv;

    strcpy(buffer, "name 'arg'1 arg'2'");  // Safe.
    CHECK_TRUE(split_args(buffer, 0, &argc, &argv));
    CHECK_INTEQUAL(argc, 3);
    CHECK_STREQUAL(argv[0], "name");
    CHECK_STREQUAL(argv[1], "arg1");
    CHECK_STREQUAL(argv[2], "arg2");
    CHECK_FALSE(argv[3]);
    mem_free(argv);

    strcpy(buffer, "name 'arg1 arg2'");  // Safe.
    CHECK_TRUE(split_args(buffer, 0, &argc, &argv));
    CHECK_INTEQUAL(argc, 2);
    CHECK_STREQUAL(argv[0], "name");
    CHECK_STREQUAL(argv[1], "arg1 arg2");
    CHECK_FALSE(argv[2]);
    mem_free(argv);

    strcpy(buffer, "name 'arg\\'1 arg2'");  // Safe.
    CHECK_TRUE(split_args(buffer, 0, &argc, &argv));
    CHECK_INTEQUAL(argc, 3);
    CHECK_STREQUAL(argv[0], "name");
    CHECK_STREQUAL(argv[1], "arg\\1");
    CHECK_STREQUAL(argv[2], "arg2");
    CHECK_FALSE(argv[3]);
    mem_free(argv);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_split_args_double_quotes)
{
    char buffer[100];
    int argc;
    char **argv;

    strcpy(buffer, "name \"arg\"1 arg\"2\"");  // Safe.
    CHECK_TRUE(split_args(buffer, 0, &argc, &argv));
    CHECK_INTEQUAL(argc, 3);
    CHECK_STREQUAL(argv[0], "name");
    CHECK_STREQUAL(argv[1], "arg1");
    CHECK_STREQUAL(argv[2], "arg2");
    CHECK_FALSE(argv[3]);
    mem_free(argv);

    strcpy(buffer, "name \"arg1 arg2\"");  // Safe.
    CHECK_TRUE(split_args(buffer, 0, &argc, &argv));
    CHECK_INTEQUAL(argc, 2);
    CHECK_STREQUAL(argv[0], "name");
    CHECK_STREQUAL(argv[1], "arg1 arg2");
    CHECK_FALSE(argv[2]);
    mem_free(argv);

    strcpy(buffer, "name \"arg\\1 arg2\"");  // Safe.
    CHECK_TRUE(split_args(buffer, 0, &argc, &argv));
    CHECK_INTEQUAL(argc, 2);
    CHECK_STREQUAL(argv[0], "name");
    CHECK_STREQUAL(argv[1], "arg\\1 arg2");
    CHECK_FALSE(argv[2]);
    mem_free(argv);

    strcpy(buffer, "name \"arg\\\"1 arg2\"");  // Safe.
    CHECK_TRUE(split_args(buffer, 0, &argc, &argv));
    CHECK_INTEQUAL(argc, 2);
    CHECK_STREQUAL(argv[0], "name");
    CHECK_STREQUAL(argv[1], "arg\"1 arg2");
    CHECK_FALSE(argv[2]);
    mem_free(argv);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_split_args_mixed_quotes)
{
    char buffer[100];
    int argc;
    char **argv;

    strcpy(buffer, "name 'arg\"1 \\\"arg'2 arg3\"");  // Safe.
    CHECK_TRUE(split_args(buffer, 0, &argc, &argv));
    CHECK_INTEQUAL(argc, 3);
    CHECK_STREQUAL(argv[0], "name");
    CHECK_STREQUAL(argv[1], "arg\"1 \\\"arg2");
    CHECK_STREQUAL(argv[2], "arg3");
    CHECK_FALSE(argv[3]);
    mem_free(argv);

    strcpy(buffer, "name \"arg'1 \\\"arg\"2 arg3'");  // Safe.
    CHECK_TRUE(split_args(buffer, 0, &argc, &argv));
    CHECK_INTEQUAL(argc, 3);
    CHECK_STREQUAL(argv[0], "name");
    CHECK_STREQUAL(argv[1], "arg'1 \"arg2");
    CHECK_STREQUAL(argv[2], "arg3");
    CHECK_FALSE(argv[3]);
    mem_free(argv);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_split_args_empty)
{
    char buffer[100];
    int argc;
    char **argv;

    strcpy(buffer, "");  // Safe.
    CHECK_TRUE(split_args(buffer, 0, &argc, &argv));
    CHECK_INTEQUAL(argc, 0);
    CHECK_FALSE(argv[0]);
    mem_free(argv);

    strcpy(buffer, "");  // Safe.
    CHECK_TRUE(split_args(buffer, 1, &argc, &argv));
    CHECK_INTEQUAL(argc, 1);
    CHECK_STREQUAL(argv[0], "");
    CHECK_FALSE(argv[1]);
    mem_free(argv);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_split_args_memory_failure)
{
    char buffer[100];
    int argc;
    char **argv;

    CHECK_MEMORY_FAILURES(
        (strcpy(buffer, "name arg1 arg2"),  // Safe.
         split_args(buffer, 1, &argc, &argv)));
    CHECK_INTEQUAL(argc, 4);
    CHECK_STREQUAL(argv[0], "");
    CHECK_STREQUAL(argv[1], "name");
    CHECK_STREQUAL(argv[2], "arg1");
    CHECK_STREQUAL(argv[3], "arg2");
    CHECK_FALSE(argv[4]);
    mem_free(argv);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_split_args_invalid)
{
    char buffer[] = "1 2 3";
    int argc;
    char **argv;

    argc = -123;
    argv = (char **)123;
    CHECK_FALSE(split_args(NULL, 0, &argc, &argv));
    CHECK_INTEQUAL(argc, -123);
    CHECK_PTREQUAL(argv, (char **)123);
    CHECK_FALSE(split_args(buffer, 0, NULL, &argv));
    CHECK_PTREQUAL(argv, (char **)123);
    CHECK_FALSE(split_args(buffer, 0, &argc, NULL));
    CHECK_INTEQUAL(argc, -123);

    return 1;
}

/*************************************************************************/
/*************************************************************************/
