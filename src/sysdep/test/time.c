/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/test/time.c: Testing implementation of the system-level
 * timekeeping functions.
 */

/*
 * This file implements an overlay of the sys_time_*() functions for
 * testing time-related functionality.  The "current time" reported by
 * sys_time_now() can be set arbitrarily by calling sys_test_time_set().
 */

#define IN_SYSDEP_TEST

#include "src/base.h"
#include "src/math.h"
#include "src/sysdep.h"
#include "src/sysdep/test.h"
#include "src/time.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

#define TIME_UNITS_PER_SEC  64

static uint64_t current_time;   // Current time reported by sys_time_now().
static DateTime current_utc;    // Current time reported by sys_time_get_utc().
static int current_utc_offset;  // Timezone offset reported by sys_time_get_utc().;

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

void sys_time_init(void)
{
    current_time = 0;
    mem_clear(&current_utc, sizeof(current_utc));
    current_utc.year = 2000;
    current_utc.month = 1;
    current_utc.day = 1;
    current_utc.weekday = 6;
    current_utc_offset = 0;
}

/*-----------------------------------------------------------------------*/

uint64_t sys_time_unit(void)
{
    return TIME_UNITS_PER_SEC;
}

/*-----------------------------------------------------------------------*/

uint64_t sys_time_now(void)
{
    return current_time;
}

/*-----------------------------------------------------------------------*/

void sys_time_delay(int64_t time)
{
    current_time += time;
}

/*-----------------------------------------------------------------------*/

int sys_time_get_utc(struct DateTime *time_ret)
{
    *time_ret = current_utc;
    return current_utc_offset;
}

/*************************************************************************/
/************************* Test control routines *************************/
/*************************************************************************/

void sys_test_time_set(uint64_t time)
{
    current_time = time;
}

/*-----------------------------------------------------------------------*/

void sys_test_time_set_seconds(double time)
{
    current_time = (uint64_t)(time * TIME_UNITS_PER_SEC);
}

/*-----------------------------------------------------------------------*/

void sys_test_time_set_utc(const DateTime *utc, int utc_offset)
{
    PRECOND(utc != NULL, return);
    PRECOND(utc_offset > -1440 && utc_offset < 1440, return);
    current_utc = *utc;
    current_utc_offset = utc_offset;
}

/*************************************************************************/
/*************************************************************************/
