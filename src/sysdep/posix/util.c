/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/posix/util.c: Internal utility functions for POSIX systems.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/math.h"
#include "src/sysdep.h"
#include "src/sysdep/posix/util.h"

#include <time.h>

/* Darwin doesn't have clock_gettime(), so we need to use gettimeofday()
 * instead. */
#if defined(SIL_PLATFORM_IOS) || defined(SIL_PLATFORM_MACOSX)
# include <sys/time.h>
#endif

/*************************************************************************/
/*************************************************************************/

struct timespec timeout_to_ts(float timeout)
{
    /* In theory we should use ceilf() when rounding to integer, but since
     * that's slightly more complex when the value is split into two parts
     * (we have to check for overflow of the low part) and the difference
     * is negligible at nanosecond resolution, we just go with floorf(). */
    const int32_t sec = ifloorf(timeout);
    const int32_t nsec = ifloorf(fmodf(timeout,1) * 1e9f);
    struct timespec ts;
#if defined(SIL_PLATFORM_IOS) || defined(SIL_PLATFORM_MACOSX)
    struct timeval tv;
    gettimeofday(&tv, NULL);
    ts.tv_sec = tv.tv_sec;
    ts.tv_nsec = tv.tv_usec * 1000;
#else
    clock_gettime(CLOCK_REALTIME, &ts);
#endif
    ts.tv_nsec += nsec;
    ts.tv_sec += sec + ts.tv_nsec/1000000000;
    ts.tv_nsec %= 1000000000;
    return ts;
}

/*************************************************************************/
/*************************************************************************/
