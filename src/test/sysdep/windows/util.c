/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/windows/util.c: Tests for Windows-specific utility
 * functions.
 */

#include "src/base.h"
#include "src/sysdep/windows/internal.h"
#include "src/test/base.h"

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_windows_util)

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_strcmp_16)
{
    static const uint16_t a[] = {'a', 0};
    static const uint16_t b[] = {'b', 0};
    static const uint16_t c[] = {0xFF43, 0};
    static const uint16_t empty[] = {0};

    CHECK_INTEQUAL(strcmp_16(a, a), 0);
    CHECK_INTEQUAL(strcmp_16(c, c), 0);
    CHECK_INTEQUAL(strcmp_16(empty, empty), 0);

    CHECK_TRUE(strcmp_16(a, b) < 0);
    CHECK_TRUE(strcmp_16(b, a) > 0);
    CHECK_TRUE(strcmp_16(a, c) < 0);
    CHECK_TRUE(strcmp_16(c, a) > 0);
    CHECK_TRUE(strcmp_16(a, empty) > 0);
    CHECK_TRUE(strcmp_16(empty, a) < 0);
    CHECK_TRUE(strcmp_16(c, empty) > 0);
    CHECK_TRUE(strcmp_16(empty, c) < 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_strdup_16)
{
    uint16_t *copy;

    static const uint16_t s16[] = {'T', 0xC9, 'S', 0x4E01, 0xFF01, 0};
    CHECK_TRUE(copy = strdup_16(s16));
    CHECK_MEMEQUAL(copy, s16, sizeof(s16));
    mem_free(copy);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_strdup_16to8)
{
    char *s;

    static const uint16_t s16[] = {'T', 0xC9, 'S', 0x4E01, 0xFF01, 0};
    CHECK_TRUE(s = strdup_16to8(s16));
    CHECK_STREQUAL(s, "TÉS丁！");
    mem_free(s);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_strdup_16to8_memory_failure)
{
    char *s;

    static const uint16_t s16[] = {'T', 0xC9, 'S', 0x4E01, 0xFF01, 0};
    CHECK_MEMORY_FAILURES(s = strdup_16to8(s16));
    CHECK_STREQUAL(s, "TÉS丁！");
    mem_free(s);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_strdup_16to8_surrogates)
{
    char *s;

    static const uint16_t s16[] = {'A', 0xDBC8, 0xDF45, 'B', 0};  // U+102345
    CHECK_TRUE(s = strdup_16to8(s16));
    CHECK_STREQUAL(s, "A\xF4\x82\x8D\x85""B");
    mem_free(s);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_strdup_16to8_surrogates_invalid)
{
    char *s;

    static const uint16_t s16[] = {'A', 0xDBC8, 'B', 0xDF45, 0xD800, 0};
    CHECK_TRUE(s = strdup_16to8(s16));
    CHECK_STREQUAL(s, "A\xEF\xBF\xBD""B\xEF\xBF\xBD\xEF\xBF\xBD");
    mem_free(s);

    static const uint16_t s16_2[] = {'A', 0xDBC8, 0xFF01, 0xDF45, 0xD800, 0};
    CHECK_TRUE(s = strdup_16to8(s16_2));
    CHECK_STREQUAL(s, "A\xEF\xBF\xBD\xEF\xBC\x81\xEF\xBF\xBD\xEF\xBF\xBD");
    mem_free(s);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_strdup_16to8_empty)
{
    char *s;

    static const uint16_t s16[] = {0};
    CHECK_TRUE(s = strdup_16to8(s16));
    CHECK_STREQUAL(s, "");
    mem_free(s);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_strdup_8to16)
{
    uint16_t *s;

    static const uint16_t expect[] = {'T', 0xC9, 'S', 0x4E01, 0xFF01, 0};
    CHECK_TRUE(s = strdup_8to16("TÉS丁！"));
    CHECK_MEMEQUAL(s, expect, sizeof(expect));
    mem_free(s);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_strdup_8to16_memory_failure)
{
    uint16_t *s;

    static const uint16_t expect[] = {'T', 0xC9, 'S', 0x4E01, 0xFF01, 0};
    CHECK_MEMORY_FAILURES(s = strdup_8to16("TÉS丁！"));
    CHECK_MEMEQUAL(s, expect, sizeof(expect));
    mem_free(s);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_strdup_8to16_surrogates)
{
    uint16_t *s;

    static const uint16_t expect[] = {'A', 0xDBC8, 0xDF45, 'B', 0};
    CHECK_TRUE(s = strdup_8to16("A\xF4\x82\x8D\x85""B"));
    CHECK_MEMEQUAL(s, expect, sizeof(expect));
    mem_free(s);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_strdup_8to16_surrogates_in_utf8)
{
    uint16_t *s;

    static const uint16_t expect[] = {'A', 0xFFFD, 0xFFFD, 'B', 0};
    CHECK_TRUE(s = strdup_8to16("A\xED\xAF\x88\xED\xBD\x85""B"));
    CHECK_MEMEQUAL(s, expect, sizeof(expect));
    mem_free(s);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_strdup_8to16_out_of_range_utf8)
{
    uint16_t *s;

    static const uint16_t expect[] = {'A', 0xFFFD, 'B', 0};
    CHECK_TRUE(s = strdup_8to16("A\xF5\x80\x80\x80""B"));
    CHECK_MEMEQUAL(s, expect, sizeof(expect));
    mem_free(s);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_strdup_8to16_invalid_utf8)
{
    uint16_t *s;

    static const uint16_t expect[] = {'A', 'B', 0};
    CHECK_TRUE(s = strdup_8to16("A\xF4""B"));
    CHECK_MEMEQUAL(s, expect, sizeof(expect));
    mem_free(s);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_strdup_8to16_empty)
{
    uint16_t *s;

    static const uint16_t expect[] = {0};
    CHECK_TRUE(s = strdup_8to16(""));
    CHECK_MEMEQUAL(s, expect, sizeof(expect));
    mem_free(s);

    return 1;
}

/*-----------------------------------------------------------------------*/

/* windows_getenv() is tested in utf8_wrappers.c to avoid a circular test
 * dependency. */

/*-----------------------------------------------------------------------*/

TEST(test_windows_strerror)
{
    CHECK_TRUE(strncmp(windows_strerror(ERROR_OUTOFMEMORY),
                       "0000000E: ", 10) == 0);
    CHECK_STREQUAL(windows_strerror(1<<29), "20000000");
    return 1;
}

/*************************************************************************/
/*************************************************************************/
