/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/macosx/util.c: Tests for Mac OS X utility functions.
 */

#include "src/base.h"
#include "src/test/base.h"
#include "src/sysdep/macosx/util.h"

/*************************************************************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_macosx_util)

/*-----------------------------------------------------------------------*/

TEST(test_version_major)
{
    CHECK_TRUE(macosx_version_major() >= 10);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_version_minor)
{
    CHECK_TRUE(macosx_version_minor() >= 0);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_version_bugfix)
{
    CHECK_TRUE(macosx_version_bugfix() >= 0);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_version_is_at_least)
{
    const int x = macosx_version_major();
    const int y = macosx_version_minor();
    const int z = macosx_version_bugfix();

    /* Comments indicate the version number that would be tested for a
     * runtime OS version of 2.5.8, where "*" = 999 (assumed to be larger
     * than any real version). */
    CHECK_TRUE (macosx_version_is_at_least(x-1, 999, 999));  // 1.*.*
    CHECK_TRUE (macosx_version_is_at_least(x+0, y-1, 999));  // 2.4.*
    CHECK_TRUE (macosx_version_is_at_least(x+0, y+0, z-1));  // 2.5.7
    CHECK_TRUE (macosx_version_is_at_least(x+0, y+0, z+0));  // 2.5.8
    CHECK_FALSE(macosx_version_is_at_least(x+0, y+0, z+1));  // 2.5.9
    CHECK_FALSE(macosx_version_is_at_least(x+0, y+1, 0  ));  // 2.6.0
    CHECK_FALSE(macosx_version_is_at_least(x+1, 0,   0  ));  // 3.0.0

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_application_support)
{
    char expected[4096];
    ASSERT(strformat_check(expected, sizeof(expected),
                           "%s/Library/Application Support", getenv("HOME")));
    CHECK_STREQUAL(macosx_get_application_support_path(), expected);
    return 1;
}

/*************************************************************************/
/*************************************************************************/
