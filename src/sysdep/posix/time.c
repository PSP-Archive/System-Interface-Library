/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/posix/time.c: Timekeeping functions for POSIX-compatible systems.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/math.h"
#include "src/sysdep.h"
#include "src/sysdep/posix/time.h"
#include "src/time.h"

#include <sys/time.h>
#include <time.h>

/*************************************************************************/
/****************** Global data (only used for testing) ******************/
/*************************************************************************/

#ifndef SIL_INCLUDE_TESTS
static const
#endif
    uint8_t TEST_sys_posix_disable_clock_gettime = 0;

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Clock ID to use for clock_gettime().  We prefer CLOCK_MONOTONIC if
 * available, since it's not affected by time-of-day changes.  (Linux also
 * has CLOCK_MONOTONIC_RAW, which is also isolated from NTP time slew, but
 * we accept slew over short periods in order to provide a timestamp closer
 * to real time over longer periods.) */
static clockid_t clock_id;

/* Flag: Use clock_gettime() (true) or gettimeofday() (false)? */
static uint8_t use_clock_gettime;

/* Flag: Has epoch been set yet? */
static uint8_t epoch_set;

/* time_now() epoch in sys_time_now() time units. */
static uint64_t epoch;

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

void sys_time_init(void)
{
    epoch_set = 0;

    use_clock_gettime = 0;
    struct timespec ts;

    if (!TEST_sys_posix_disable_clock_gettime) {
#ifdef DEBUG
# define CLOCK_PREFERENCE(id) {id, #id}
#else
# define CLOCK_PREFERENCE(id) {id}
#endif
        static const struct {
            clockid_t id;
#ifdef DEBUG
            const char *name;
#endif
        } clock_preference[] = {
#ifdef CLOCK_MONOTONIC
            CLOCK_PREFERENCE(CLOCK_MONOTONIC),
#endif
            CLOCK_PREFERENCE(CLOCK_REALTIME),
        };
#undef CLOCK_PREFERENCE

        for (int i = 0; i < lenof(clock_preference); i++) {
            if (clock_gettime(clock_preference[i].id, &ts) == 0) {
                DLOG("Using %s as time source", clock_preference[i].name);
                clock_id = CLOCK_MONOTONIC;
                use_clock_gettime = 1;
                break;
            }
        }
    }

    if (!use_clock_gettime) {
        DLOG("clock_gettime() unavailable, using gettimeofday()");
    }
}

/*-----------------------------------------------------------------------*/

uint64_t sys_time_unit(void)
{
    return use_clock_gettime ? 1000000000 : 1000000;
}

/*-----------------------------------------------------------------------*/

uint64_t sys_time_now(void)
{
    uint64_t time;
    if (use_clock_gettime) {
        struct timespec ts;
        clock_gettime(clock_id, &ts);
        time = ((uint64_t)ts.tv_sec)*1000000000 + ts.tv_nsec;
    } else {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        time = ((uint64_t)tv.tv_sec)*1000000 + tv.tv_usec;
    }
    if (UNLIKELY(!epoch_set)) {
        epoch_set = 1;
        epoch = time;
    }
    return time;
}

/*-----------------------------------------------------------------------*/

void sys_time_delay(int64_t delay)  // Param renamed to avoid time() collision.
{
    if (!use_clock_gettime) {
        delay *= 1000;  // usec -> nsec
    }
    struct timespec ts;
    ts.tv_sec  = delay / 1000000000;
    ts.tv_nsec = delay % 1000000000;
    while (nanosleep(&ts, &ts) != 0) {
        /* nanosleep() can only fail for EINTR, EFAULT, or EINVAL due to
         * tv_nsec out of range.  The latter two should be impossible, but
         * ASSERT() just in case. */
        ASSERT(errno == EINTR, break);
    }
}

/*-----------------------------------------------------------------------*/

int sys_time_get_utc(struct DateTime *time_ret)
{
    time_t sec;
    if (use_clock_gettime) {
        struct timespec ts;
        ASSERT(clock_gettime(CLOCK_REALTIME, &ts) == 0);  // Cannot fail.
        sec = ts.tv_sec;
        time_ret->nsec = ts.tv_nsec;
    } else {
        struct timeval tv;
        ASSERT(gettimeofday(&tv, NULL) == 0);  // Cannot fail.
        sec = tv.tv_sec;
        time_ret->nsec = tv.tv_usec * 1000;
    }

    struct tm utc;
    ASSERT(gmtime_r(&sec, &utc) == &utc);  // Cannot fail.
    time_ret->year = utc.tm_year + 1900;
    time_ret->month = utc.tm_mon + 1;
    time_ret->day = utc.tm_mday;
    time_ret->weekday = utc.tm_wday;
    time_ret->hour = utc.tm_hour;
    time_ret->minute = utc.tm_min;
    time_ret->second = utc.tm_sec;
    time_ret->nsec = 0;

    struct tm local;
    ASSERT(localtime_r(&sec, &local) == &local);  // Cannot fail.
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
/******************* POSIX-specific interface routines *******************/
/*************************************************************************/

uint64_t sys_posix_time_epoch(void)
{
    return epoch;
}

/*-----------------------------------------------------------------------*/

int sys_posix_time_clock(void)
{
    return use_clock_gettime ? clock_id : CLOCK_REALTIME;
}

/*************************************************************************/
/*************************************************************************/
