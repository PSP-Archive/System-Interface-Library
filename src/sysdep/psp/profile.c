/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/profile.c: Simple profiling routines for profiling a
 * limited number of selected functions.
 */

#ifdef DEBUG  // To the end of the file.

#include "src/base.h"
#include "src/sysdep/psp/internal.h"
#include "src/sysdep/psp/profile.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Maximum number of functions to profile. */
#define PROFILE_MAX  640

/* Profiling data. */
static struct {
    const char *funcname;  // Name of this function
    uint32_t recurse_level;// Current recursion level
    uint32_t calls;        // Number of calls made to function
    uint32_t usec;         // Total microseconds spent in function
    uint32_t start_time;   // Timestamp at last _profile_start() call
} profile_data[PROFILE_MAX];
static int profile_used = 0;

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

void _profile_start(const char *funcname, int *index_ptr)
{
    if (UNLIKELY(!*index_ptr)) {
        if (profile_used >= lenof(profile_data)) {
            DLOG("Out of profile slots for function %s\n", funcname);
            *index_ptr = -1;
            return;
        } else {
            *index_ptr = ++profile_used;
            profile_data[*index_ptr - 1].funcname = funcname;
        }
    }

    const int index = *index_ptr - 1;
    if (LIKELY(index >= 0)) {
        profile_data[index].recurse_level++;
        if (profile_data[index].recurse_level == 1) {
            /* Push the system call down as far as possible to minimize the
             * amount of overhead we record. */
            uint32_t *start_time_ptr = &profile_data[index].start_time;
            *start_time_ptr = sceKernelGetSystemTimeLow();
        }
    }
}

/*-----------------------------------------------------------------------*/

void _profile_end(int *index_ptr)
{
    const uint32_t now = sceKernelGetSystemTimeLow();
    const int index = *index_ptr - 1;
    if (LIKELY(index >= 0)) {
        profile_data[index].calls++;
        profile_data[index].recurse_level--;
        if (profile_data[index].recurse_level == 0) {
            profile_data[index].usec += now - profile_data[index].start_time;
        }
    }
}

/*-----------------------------------------------------------------------*/

void _profile_pause(int *index_ptr)
{
    const uint32_t now = sceKernelGetSystemTimeLow();
    if (LIKELY(*index_ptr > 0)) {
        profile_data[*index_ptr - 1].usec +=
            now - profile_data[*index_ptr - 1].start_time;
    }
}

/*-----------------------------------------------------------------------*/

void _profile_resume(int *index_ptr)
{
    if (LIKELY(*index_ptr > 0)) {
        uint32_t *start_time_ptr = &profile_data[*index_ptr - 1].start_time;
        *start_time_ptr = sceKernelGetSystemTimeLow();
    }
}

/*-----------------------------------------------------------------------*/

void _profile_dump(void)
{
    static const char header[] =
        "  Calls     usec  usec/call  Function\n"
        "-------  -------  ---------  --------\n";
    sceIoWrite(1, header, sizeof(header)-1);

    /* Dump all functions with at least one call, in order from most to
     * least time taken.  This isn't a time-critical function, so we don't
     * bother trying to cleverly sort or anything like that. */

    for (;;) {
        int best = -1;
        for (int i = 0; i < profile_used; i++) {
            if (profile_data[i].calls > 0) {
                if (best < 0 || profile_data[i].usec > profile_data[best].usec) {
                    best = i;
                }
            }
        }
        if (best < 0) {
            break;
        }
        char buf[1000];
        strformat(buf, sizeof(buf), "%7d  %7d  %9.2f  %s\n",
                  profile_data[best].calls, profile_data[best].usec,
                  (double)profile_data[best].usec / profile_data[best].calls,
                  profile_data[best].funcname);
        sceIoWrite(1, buf, strlen(buf));
        profile_data[best].calls = 0;
        profile_data[best].usec = 0;
    }
}

/*************************************************************************/
/*************************************************************************/

#endif  // DEBUG
