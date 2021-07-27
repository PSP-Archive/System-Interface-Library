/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/posix/userdata.c: Tests for the POSIX implementation of
 * the user data access functions.
 */

#include "src/base.h"
#include "src/sysdep.h"
#include "src/sysdep/windows/internal.h"
#include "src/test/base.h"
#include "src/test/graphics/internal.h"

/*************************************************************************/
/*************************************************************************/

int test_windows_condvar(void)
{
    DLOG("=== Testing emulated condition variables ===");
    TEST_windows_condvar_disable_native = 1;
    const int result = test_condvar();
    TEST_windows_condvar_disable_native = 0;
    if (!result) {
        DLOG("=== Preceding failure(s) occured with emulated condition"
             " variables ===");
    }

    return result;
}

/*************************************************************************/
/*************************************************************************/
