/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/windows/time.c: Tests for Windows-specific timekeeping
 * functions.
 */

#include "src/base.h"
#define IN_SYSDEP  // So we get the real functions instead of the diversions.
#include "src/sysdep.h"
#include "src/sysdep/windows/internal.h"
#include "src/test/base.h"
#include "src/thread.h"
#include "src/time.h"

#include <time.h>

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_windows_time)

TEST_INIT(init)
{
    sys_time_init();

    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    CHECK_INTEQUAL(sys_time_unit(), frequency.QuadPart);

    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_delay)
{
    LARGE_INTEGER frequency_buf;
    QueryPerformanceFrequency(&frequency_buf);
    const uint64_t frequency = frequency_buf.QuadPart;
    const uint64_t ticks_per_csec = frequency / 100;

    LARGE_INTEGER start, end;

    QueryPerformanceCounter(&start);
    QueryPerformanceCounter(&end);
    CHECK_INTRANGE(end.QuadPart - start.QuadPart, 0, ticks_per_csec/2);

    QueryPerformanceCounter(&start);
    sys_time_delay(10*ticks_per_csec);
    QueryPerformanceCounter(&end);
    /* Sleep() seems to sometimes wake up slightly before the requested
     * amount of time has passed. */
    CHECK_INTRANGE(end.QuadPart - start.QuadPart,
                   9*ticks_per_csec, 12*ticks_per_csec);

    QueryPerformanceCounter(&start);
    sys_time_delay(0);
    QueryPerformanceCounter(&end);
    CHECK_INTRANGE(end.QuadPart - start.QuadPart, 0, ticks_per_csec/2);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_utc)
{
    /* The Windows implementation of sys_time_get_utc() calls the Win32
     * kernel functions to obtain the current time and timezone offset.
     * We test using the CRT equivalents so we're not just repeating the
     * code under test. */

    /* Wait until the beginning of a new second to reduce the chance of
     * spurious failure due to crossing to a new second. */
    DLOG("Waiting for next second...");
    __time64_t now = _time64(NULL);
    while (_time64(NULL) == now) {
        thread_yield();
    }
    now++;

    struct tm utc, local;
    _gmtime64_s(&utc, &now);
    _localtime64_s(&local, &now);
    int utc_offset =
        (local.tm_hour*60 + local.tm_min) - (utc.tm_hour*60 + utc.tm_min);
    if (local.tm_wday != utc.tm_wday) {
        if (((local.tm_wday+7) - utc.tm_wday) % 7 == 1) {
            ASSERT(utc_offset < 0);
            utc_offset += 1440;
        } else {
            ASSERT(utc_offset > 0);
            utc_offset -= 1440;
        }
    }

    DateTime utc_time;
    CHECK_INTEQUAL(sys_time_get_utc(&utc_time), utc_offset);
    CHECK_INTEQUAL(utc_time.year, utc.tm_year + 1900);
    CHECK_INTEQUAL(utc_time.month, utc.tm_mon + 1);
    CHECK_INTEQUAL(utc_time.day, utc.tm_mday);
    CHECK_INTEQUAL(utc_time.weekday, utc.tm_wday);
    CHECK_INTEQUAL(utc_time.hour, utc.tm_hour);
    CHECK_INTEQUAL(utc_time.minute, utc.tm_min);
    CHECK_INTEQUAL(utc_time.second, utc.tm_sec);
    CHECK_TRUE(utc_time.nsec >= 0 && utc_time.nsec < 1000000000);

    return 1;
}

/*************************************************************************/
/*************************************************************************/
