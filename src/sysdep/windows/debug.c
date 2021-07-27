/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/windows/debug.c: Windows-specific debugging utility functions.
 */

#ifdef DEBUG  // To the end of the file.

#define IN_SYSDEP

#include "src/base.h"
#include "src/sysdep.h"
#include "src/sysdep/windows/internal.h"

#define PSAPI_VERSION 1 // Ensure pre-Win7 compatibility when building on Win7.
#include <psapi.h>

/*************************************************************************/
/*************************************************************************/

int sys_debug_get_memory_stats(
    int64_t *total_ret, int64_t *self_ret, int64_t *avail_ret)
{
#if WINVER >= WINDOWS_VERSION_XP
    MEMORYSTATUSEX memstat;
    memstat.dwLength = sizeof(memstat);
    if (UNLIKELY(!GlobalMemoryStatusEx(&memstat))) {
        DLOG("Failed to get memory information: %s",
             windows_strerror(GetLastError()));
        return 0;
    }
    *total_ret = memstat.ullTotalPhys + memstat.ullTotalPageFile;
    *avail_ret = memstat.ullAvailPhys + memstat.ullAvailPageFile;

    PROCESS_MEMORY_COUNTERS procmemstat;
    if (UNLIKELY(!GetProcessMemoryInfo(GetCurrentProcess(),
                                       &procmemstat, sizeof(procmemstat)))) {
        DLOG("Failed to get process memory information: %s",
             windows_strerror(GetLastError()));
        return 0;
    }
    *self_ret = procmemstat.WorkingSetSize;

    return 1;
#else
    static int warned = 0;
    if (!warned) {
        DLOG("Unable to get memory information on pre-WinXP systems");
        warned = 1;
    }
    return 0;
#endif
}

/*************************************************************************/
/*************************************************************************/

#endif  // DEBUG
