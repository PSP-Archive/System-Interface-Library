/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/psp/time.c: Tests for PSP-specific timekeeping functions.
 */

#include "src/base.h"
#define IN_SYSDEP  // So we get the real functions instead of the diversions.
#include "src/sysdep.h"
#include "src/sysdep/psp/internal.h"
#include "src/test/base.h"

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_psp_time)

TEST_INIT(init)
{
    sys_time_init();
    CHECK_INTEQUAL(sys_time_unit(), 1000000);
    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_delay)
{
    uint32_t start, end;

    start = sceKernelGetSystemTimeLow();
    CHECK_INTRANGE(sceKernelGetSystemTimeLow() - start, 0, 5000);

    start = sceKernelGetSystemTimeLow();
    sys_time_delay(10000);
    end = sceKernelGetSystemTimeLow();
    CHECK_INTRANGE(end - start, 10000, 20000);

    start = sceKernelGetSystemTimeLow();
    sys_time_delay(0);
    CHECK_INTRANGE(sceKernelGetSystemTimeLow() - start, 0, 5000);

    return 1;
}

/*************************************************************************/
/*************************************************************************/
