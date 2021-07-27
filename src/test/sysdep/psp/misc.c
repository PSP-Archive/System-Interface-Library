/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/psp/misc.c: Miscellaneous PSP-specific tests.
 */

#include "src/base.h"
#define IN_SYSDEP  // So we get the real functions instead of the diversions.
#include "src/sysdep.h"
#include "src/sysdep/psp/internal.h"
#include "src/sysdep/psp/thread.h"
#include "src/test/base.h"
#include "src/utility/misc.h"

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_psp_misc)

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_get_language)
{
    char language[3], dialect[3];
    CHECK_TRUE(sys_get_language(0, language, dialect));
    CHECK_INTEQUAL(strlen(language), 2);

    CHECK_FALSE(sys_get_language(1, language, dialect));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_file_url)
{
    CHECK_FALSE(sys_open_file(NULL));
    CHECK_FALSE(sys_open_url(NULL));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_reset_idle_timer)
{
    sys_reset_idle_timer();  // Just make sure it doesn't crash.
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_performance)
{
    unsigned int time_low, time_high, time_166, time_default;
    unsigned int start, end;

    CHECK_TRUE(sys_set_performance_level(PERFORMANCE_LEVEL_LOW));
    psp_threads_lock();
    start = sceKernelGetSystemTimeLow();
    {
        for (int i = 0; i < 10000; i++) {
            BARRIER();
        }
    }
    end = sceKernelGetSystemTimeLow();
    psp_threads_unlock();
    time_low = end - start;

    CHECK_TRUE(sys_set_performance_level(PERFORMANCE_LEVEL_HIGH));
    psp_threads_lock();
    start = sceKernelGetSystemTimeLow();
    {
        for (int i = 0; i < 10000; i++) {
            BARRIER();
        }
    }
    end = sceKernelGetSystemTimeLow();
    psp_threads_unlock();
    time_high = end - start;

    CHECK_TRUE(sys_set_performance_level(166));
    psp_threads_lock();
    start = sceKernelGetSystemTimeLow();
    {
        for (int i = 0; i < 10000; i++) {
            BARRIER();
        }
    }
    end = sceKernelGetSystemTimeLow();
    psp_threads_unlock();
    time_166 = end - start;

    CHECK_TRUE(sys_set_performance_level(PERFORMANCE_LEVEL_DEFAULT));
    psp_threads_lock();
    start = sceKernelGetSystemTimeLow();
    {
        for (int i = 0; i < 10000; i++) {
            BARRIER();
        }
    }
    end = sceKernelGetSystemTimeLow();
    psp_threads_unlock();
    time_default = end - start;

    CHECK_TRUE(time_low > time_166);
    CHECK_TRUE(time_166 > time_default);
    CHECK_TRUE(time_default > time_high);

    CHECK_FALSE(sys_set_performance_level(1));
    CHECK_FALSE(sys_set_performance_level(INT_MAX));

    return 1;
}

/*************************************************************************/
/*************************************************************************/
