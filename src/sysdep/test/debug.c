/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/test/debug.c: Testing implementation of the system-specific
 * debugging utility functions.
 */

#ifdef DEBUG  // To the end of the file.

#define IN_SYSDEP_TEST

#include "src/base.h"
#include "src/sysdep.h"
#include "src/sysdep/test.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Values to return from sys_debug_get_memory_stats(). */
static int64_t total, self, avail;

/* Flag: Fail the next sys_debug_get_memory_stats() call? */
static uint8_t fail_next_memory_stats_call;

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int sys_debug_get_memory_stats(
    int64_t *total_ret, int64_t *self_ret, int64_t *avail_ret)
{
    if (fail_next_memory_stats_call) {
        fail_next_memory_stats_call = 0;
        return 0;
    }

    *total_ret = total;
    *self_ret = self;
    *avail_ret = avail;
    return 1;
}

/*************************************************************************/
/************************* Test control routines *************************/
/*************************************************************************/

void sys_test_debug_set_memory_stats(
    int64_t total_, int64_t self_, int64_t avail_)
{
    total = total_;
    self = self_;
    avail = avail_;
}

/*-----------------------------------------------------------------------*/

void sys_test_debug_fail_memory_stats(void)
{
    fail_next_memory_stats_call = 1;
}

/*************************************************************************/
/*************************************************************************/

#endif  // DEBUG
