/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/debug.c: System-level debug functionality tests.
 */

#include "src/base.h"
#define IN_SYSDEP  // So we get the real functions instead of the diversions.
#include "src/sysdep.h"
#include "src/test/base.h"

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_sys_debug)

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_get_memory_stats)
{
    int64_t total, self, avail;
    CHECK_TRUE(sys_debug_get_memory_stats(&total, &self, &avail));

    /* We have no idea what the "correct" values should be, so just make
     * sure they're sane. */
    CHECK_TRUE(total >= self + avail);

    /* Make sure the values we got back are in units of bytes and not
     * kB or some such.  For this check, we assume the process size is
     * at least 1MB but less than 1GB. */
    CHECK_TRUE(self >= 1000000);

    return 1;
}

/*************************************************************************/
/*************************************************************************/
