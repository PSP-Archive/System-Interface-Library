/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/linux/thread.c: POSIX thread helpers for Linux.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/sysdep.h"
#include "src/sysdep/linux/internal.h"
#include "src/sysdep/posix/thread.h"
#include "src/thread.h"

#include <sys/resource.h>

/*************************************************************************/
/*************************************************************************/

void posix_thread_runner_init(SysThread *thread)
{
    if (UNLIKELY(setpriority(PRIO_PROCESS, 0, thread->initial_priority) != 0)) {
        DLOG("Failed to set thread priority to %d: %s",
             thread->initial_priority, strerror(errno));
    }

    if (thread->initial_affinity) {
        if (UNLIKELY(!thread_set_affinity(thread->initial_affinity))) {
            DLOG("Failed to set thread affinity mask to 0x%llX",
                 (long long)thread->initial_affinity);
        }
    }
}

/*-----------------------------------------------------------------------*/

void posix_thread_runner_cleanup(UNUSED SysThread *thread)
{
    /* Nothing to do. */
}

/*************************************************************************/
/*************************************************************************/
