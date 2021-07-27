/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/windows/time.c: Timekeeping functions for Windows.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/math.h"
#include "src/sysdep.h"
#include "src/sysdep/windows/internal.h"
#include "src/time.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Tick frequency returned from sys_time_unit().  Equal to the frequency
 * returned from QueryPerformanceFrequency(). */
static uint64_t ticks_per_sec;

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

void sys_time_init(void)
{
    LARGE_INTEGER frequency_buf;
    QueryPerformanceFrequency(&frequency_buf);
    ticks_per_sec = frequency_buf.QuadPart;
}

/*-----------------------------------------------------------------------*/

uint64_t sys_time_unit(void)
{
    return ticks_per_sec;
}

/*-----------------------------------------------------------------------*/

uint64_t sys_time_now(void)
{
    LARGE_INTEGER now_buf;
    QueryPerformanceCounter(&now_buf);
    return now_buf.QuadPart;
}

/*-----------------------------------------------------------------------*/

void sys_time_delay(int64_t time)
{
    const double sec = (double)time / (double)ticks_per_sec;
    /* Round up so that the value passed to Sleep() is not less than the
     * original request, but try to avoid incrementing the millisecond
     * count solely due to rounding error. */
    const int msec = iceil((sec * 1000) - 0.001);
    Sleep(msec);
}

/*-----------------------------------------------------------------------*/

int sys_time_get_utc(struct DateTime *time_ret)
{
    SYSTEMTIME time;
    GetSystemTime(&time);
    time_ret->year = time.wYear;
    time_ret->month = time.wMonth;
    time_ret->day = time.wDay;
    time_ret->weekday = time.wDayOfWeek;
    time_ret->hour = time.wHour;
    time_ret->minute = time.wMinute;
    time_ret->second = time.wSecond;
    time_ret->nsec = time.wMilliseconds * 1000000;

    TIME_ZONE_INFORMATION tzi;
    switch (GetTimeZoneInformation(&tzi)) {
      case TIME_ZONE_ID_UNKNOWN:
        return -(tzi.Bias);
      case TIME_ZONE_ID_STANDARD:
        return -(tzi.Bias + tzi.StandardBias);
      case TIME_ZONE_ID_DAYLIGHT:
        return -(tzi.Bias + tzi.DaylightBias);
      default: {
        static uint8_t warned = 0;
        if (!warned) {
            DLOG("Failed to get time zone information: %s",
                 windows_strerror(GetLastError()));
            warned = 1;
        }
        return 0;
      }
    }
}

/*************************************************************************/
/*************************************************************************/
