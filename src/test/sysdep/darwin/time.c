/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/darwin/time.c: Tests for Darwin (OSX/iOS) timekeeping
 * functions.
 */

#include "src/base.h"
#define IN_SYSDEP  // So we get the real functions instead of the diversions.
#include "src/sysdep.h"
#include "src/sysdep/darwin/time.h"
#include "src/test/base.h"
#include "src/thread.h"
#include "src/time.h"

#undef thread_create
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <time.h>
#define thread_create sil_thread_create

/* This stuff is technically Carbon, not Darwin, but since we only use
 * Darwin code with OSX and iOS, it's not a problem. */
#include <CoreFoundation/CFCalendar.h>
#include <CoreFoundation/CFTimeZone.h>

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_darwin_time)

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_time_unit)
{
    sys_time_init();

    mach_timebase_info_data_t timebase_info;
    mach_timebase_info(&timebase_info);
    uint64_t ticks_per_sec = ((1000000000 * (uint64_t)timebase_info.denom
                               + (uint64_t)(timebase_info.numer / 2))
                              / (uint64_t)timebase_info.numer);
    CHECK_INTEQUAL(sys_time_unit(), ticks_per_sec);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_delay)
{
    sys_time_init();

    const uint64_t ticks_per_sec = sys_time_unit();
    const unsigned int ticks_per_csec = ticks_per_sec / 100;
    uint64_t start, end;

    start = mach_absolute_time();
    end = mach_absolute_time();
    CHECK_INTRANGE(end - start, 0, ticks_per_csec/2);

    start = mach_absolute_time();
    sys_time_delay(ticks_per_sec/100);
    end = mach_absolute_time();
    CHECK_INTRANGE(end - start, ticks_per_csec, ticks_per_csec*2);

    start = mach_absolute_time();
    sys_time_delay(0);
    end = mach_absolute_time();
    CHECK_INTRANGE(end - start, 0, ticks_per_csec/2);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_epoch)
{
    sys_time_init();

    const uint64_t time_unit = sys_time_unit();
    const uint64_t now = sys_time_now();
    CHECK_DOUBLEEQUAL(darwin_time_epoch(), (double)now / (double)time_unit);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_time_from_timestamp)
{
    sys_time_init();

    const uint64_t now = sys_time_now();
    CHECK_DOUBLEEQUAL(darwin_time_from_timestamp(now), 0.0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_utc)
{
    /* Wait until the beginning of a new second to reduce the chance of
     * spurious failure due to crossing to a new second. */
    DLOG("Waiting for next second...");
    const time_t now = time(NULL);
    while (time(NULL) == now) {
        thread_yield();
    }

    const CFAbsoluteTime cf_now = CFAbsoluteTimeGetCurrent();
    const CFTimeZoneRef zone = CFTimeZoneCopyDefault();
    ASSERT(zone);
    const int zone_offset_sec = CFTimeZoneGetSecondsFromGMT(zone, cf_now);
    CFRelease(zone);
    ASSERT(zone_offset_sec % 60 == 0);
    const int zone_offset = zone_offset_sec / 60;

    const CFCalendarRef calendar = CFCalendarCopyCurrent();
    ASSERT(calendar);
    const CFTimeZoneRef utc_zone =
        CFTimeZoneCreateWithTimeIntervalFromGMT(NULL, 0);
    ASSERT(utc_zone);
    CFCalendarSetTimeZone(calendar, utc_zone);
    /* This function also supports weekday extraction with "e", but the
     * value doesn't seem to be well-defined (1 is Sunday on some systems,
     * Monday on others), so we ignore it. */
    int year, month, day, hour, minute, second;
    ASSERT(CFCalendarDecomposeAbsoluteTime(
               calendar, cf_now, "yMdHms",
               &year, &month, &day, &hour, &minute, &second));
    CFRelease(calendar);
    CFRelease(utc_zone);

    DateTime utc_time;
    const int utc_offset = sys_time_get_utc(&utc_time);
    CHECK_INTEQUAL(utc_time.year, year);
    CHECK_INTEQUAL(utc_time.month, month);
    CHECK_INTEQUAL(utc_time.day, day);
    CHECK_TRUE(utc_time.weekday >= 0 && utc_time.weekday < 7);
    CHECK_INTEQUAL(utc_time.hour, hour);
    CHECK_INTEQUAL(utc_time.minute, minute);
    CHECK_INTEQUAL(utc_time.second, second);
    CHECK_TRUE(utc_time.nsec >= 0 && utc_time.nsec < 1000000000);
    CHECK_INTEQUAL(utc_offset, zone_offset);

    return 1;
}

/*************************************************************************/
/*************************************************************************/
