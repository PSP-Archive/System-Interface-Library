/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/time.c: PSP timekeeping functions.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/math.h"
#include "src/sysdep.h"
#include "src/sysdep/psp/internal.h"
#include "src/time.h"

/* Work around a missing "void" in the unofficial PSPSDK headers. */
#define sceRtcGetTickResolution(...) sceRtcGetTickResolution(void)
#include <psprtc.h>
#undef sceRtcGetTickResolution

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

void sys_time_init(void)
{
    /* Nothing to do. */
}

/*-----------------------------------------------------------------------*/

uint64_t sys_time_unit(void)
{
    return 1000000;
}

/*-----------------------------------------------------------------------*/

uint64_t sys_time_now(void)
{
    return sceKernelGetSystemTimeWide();
}

/*-----------------------------------------------------------------------*/

void sys_time_delay(int64_t time)
{
    if (time > 0) {
        sceKernelDelayThread(time);
    }
}

/*-----------------------------------------------------------------------*/

int sys_time_get_utc(struct DateTime *time_ret)
{
    pspTime tm;
    sceRtcGetCurrentClock(&tm, 0);
    time_ret->year = tm.year;
    time_ret->month = tm.month;
    time_ret->day = tm.day;
    time_ret->weekday = sceRtcGetDayOfWeek(tm.year, tm.month, tm.day);
    time_ret->hour = tm.hour;
    time_ret->minute = tm.minutes;
    time_ret->second = tm.seconds;
    time_ret->nsec = tm.microseconds * 1000;

    const uint64_t resolution = sceRtcGetTickResolution();
    uint64_t utc_offset = 86400 * resolution;
    sceRtcConvertUtcToLocalTime((uint64_t[]){utc_offset}, &utc_offset);
    return ((int)(utc_offset / resolution) - 86400) / 60;
}

/*************************************************************************/
/*************************************************************************/
