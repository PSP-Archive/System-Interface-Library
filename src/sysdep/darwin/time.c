/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/darwin/time.c: Timekeeping functions for Darwin-based systems
 * (Mac/iOS).
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/math.h"
#include "src/sysdep.h"
#include "src/sysdep/darwin/time.h"
#include "src/time.h"

#include <mach/mach.h>
#include <mach/mach_time.h>
#include <sys/time.h>

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* sys_time_now() time units per second, as returned from sys_time_unit(). */
static uint64_t ticks_per_sec;

/* time_now() epoch in the mach_absolute_now() time base (expressed in
 * seconds). */
static double epoch = 0;

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

void sys_time_init(void)
{
    mach_timebase_info_data_t timebase_info;
    mach_timebase_info(&timebase_info);
    ticks_per_sec = ((1000000000 * (uint64_t)timebase_info.denom
                      + (uint64_t)(timebase_info.numer / 2))
                     / (uint64_t)timebase_info.numer);
    epoch = 0;
}

/*-----------------------------------------------------------------------*/

uint64_t sys_time_unit(void)
{
    return ticks_per_sec;
}

/*-----------------------------------------------------------------------*/

uint64_t sys_time_now(void)
{
    const uint64_t now = mach_absolute_time();
    if (UNLIKELY(epoch == 0)) {
        epoch = (double)now / (double)ticks_per_sec;
    }
    return now;
}

/*-----------------------------------------------------------------------*/

void sys_time_delay(int64_t time)
{
    const uint64_t start = mach_absolute_time();
    const uint64_t target = start + time;
    if (start > target) {
        mach_wait_until(0);
    }
    uint64_t now;
    while ((now = mach_absolute_time()) >= start && now < target) {
        mach_wait_until(target);
    }
}

/*-----------------------------------------------------------------------*/

int sys_time_get_utc(struct DateTime *time_ret)
{
    /*
     * Mach provides clock_get_time() to get calendar time:
     *     mach_timespec_t ts;
     *     clock_serv_t clock;
     *     host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &clock);
     *     clock_get_time(clock, &ts);
     *     mach_port_deallocate(mach_task_self(), clock);
     * but it's reported to be very slow (on the order of 10 usec, see
     * <https://stackoverflow.com/questions/5167269>), so we don't use it.
     * clock_gettime() wasn't added to OSX until 10.12 and iOS 10, so we
     * just fall back to gettimeofday().
     */
    struct timeval tv;
    ASSERT(gettimeofday(&tv, NULL) == 0);  // Cannot fail.
    time_ret->nsec = tv.tv_usec * 1000;

    struct tm utc;
    ASSERT(gmtime_r(&tv.tv_sec, &utc) == &utc);  // Cannot fail.
    time_ret->year = utc.tm_year + 1900;
    time_ret->month = utc.tm_mon + 1;
    time_ret->day = utc.tm_mday;
    time_ret->weekday = utc.tm_wday;
    time_ret->hour = utc.tm_hour;
    time_ret->minute = utc.tm_min;
    time_ret->second = utc.tm_sec;
    time_ret->nsec = 0;

    struct tm local;
    ASSERT(localtime_r(&tv.tv_sec, &local) == &local);  // Cannot fail.
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
    return utc_offset;
}

/*************************************************************************/
/*********************** Library-internal routines ***********************/
/*************************************************************************/

double darwin_time_epoch(void)
{
    return epoch;
}

/*-----------------------------------------------------------------------*/

double darwin_time_from_timestamp(uint64_t timestamp)
{
    return (double)timestamp / (double)ticks_per_sec - epoch;
}

/*************************************************************************/
/*************************************************************************/
