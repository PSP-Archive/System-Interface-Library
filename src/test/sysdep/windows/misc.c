/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/windows/misc.c: Miscellaneous Windows-specific tests.
 */

#include "src/base.h"
#define IN_SYSDEP  // So we get the real functions instead of the diversions.
#include "src/sysdep.h"
#include "src/test/base.h"

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_windows_misc)

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_language)
{
    char language[3], dialect[3];

    CHECK_TRUE(sys_get_language(0, language, dialect));
    CHECK_INTRANGE(language[0], 'a', 'z');
    CHECK_INTRANGE(language[1], 'a', 'z');
    CHECK_INTEQUAL(language[2], '\0');
    if (dialect[0]) {
        CHECK_INTRANGE(dialect[0], 'A', 'Z');
        CHECK_INTRANGE(dialect[1], 'A', 'Z');
        CHECK_INTEQUAL(dialect[2], '\0');
    }

    CHECK_TRUE(sys_get_language(1, language, dialect));
    CHECK_INTRANGE(language[0], 'a', 'z');
    CHECK_INTRANGE(language[1], 'a', 'z');
    CHECK_INTEQUAL(language[2], '\0');
    if (dialect[0]) {
        CHECK_INTRANGE(dialect[0], 'A', 'Z');
        CHECK_INTRANGE(dialect[1], 'A', 'Z');
        CHECK_INTEQUAL(dialect[2], '\0');
    }

    CHECK_FALSE(sys_get_language(2, language, dialect));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_file_null)
{
    CHECK_TRUE(sys_open_file(NULL));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_file_fail)
{
    CHECK_FALSE(sys_open_file("testdata/no_such_file"));
    CHECK_INTEQUAL(sys_last_error(), SYSERR_FILE_NOT_FOUND);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_url_null)
{
    CHECK_TRUE(sys_open_url(NULL));
    return 1;
}

/*************************************************************************/
/*************************************************************************/
