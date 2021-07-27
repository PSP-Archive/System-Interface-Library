/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/darwin/semaphore.c: System-level semaphore routines for Darwin
 * (OSX/iOS).
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/math.h"
#include "src/sysdep.h"

/* Undo the function renaming from {macosx,ios}/common.h so we can access
 * the semaphore syscalls. */
#undef semaphore_create
#undef semaphore_destroy
#undef semaphore_wait
#undef semaphore_wait_timeout
#undef semaphore_signal

#include <mach/mach_error.h>
#include <mach/mach_init.h>
#include <mach/semaphore.h>
#include <mach/task.h>

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int sys_semaphore_max_value(void)
{
    /* There doesn't seem to be any documentation on the allowed maximum
     * value for a Mach semaphore; we assume that any int-sized value is
     * allowed. */
    return INT_MAX;
}

/*-----------------------------------------------------------------------*/

SysSemaphoreID sys_semaphore_create(int initial_value, UNUSED int required_max)
{
    semaphore_t sem;
    kern_return_t result;
    result = semaphore_create(mach_task_self(), &sem, SYNC_POLICY_FIFO,
                              initial_value);
    if (UNLIKELY(result != KERN_SUCCESS)) {
        DLOG("Failed to create semaphore: %d: %s",
             result, mach_error_string(result));
        return 0;
    }
    return (SysSemaphoreID)(uintptr_t)sem;
}

/*-----------------------------------------------------------------------*/

void sys_semaphore_destroy(SysSemaphoreID semaphore_)
{
    semaphore_t semaphore = (semaphore_t)(uintptr_t)semaphore_;
    semaphore_destroy(mach_task_self(), semaphore);
}

/*-----------------------------------------------------------------------*/

int sys_semaphore_wait(SysSemaphoreID semaphore_, float timeout)
{
    semaphore_t semaphore = (semaphore_t)(uintptr_t)semaphore_;
    if (timeout < 0) {
        semaphore_wait(semaphore);
        return 1;
    } else {
        const int32_t sec = (int32_t)floorf(timeout);
        const int32_t nsec = (int32_t)floorf(fmodf(timeout,1) * 1e9f);
        kern_return_t result = semaphore_timedwait(
            semaphore, (struct mach_timespec){sec, nsec});
        return result == KERN_SUCCESS;
    }
}

/*-----------------------------------------------------------------------*/

void sys_semaphore_signal(SysSemaphoreID semaphore_)
{
    semaphore_t semaphore = (semaphore_t)(uintptr_t)semaphore_;
    semaphore_signal(semaphore);
}

/*************************************************************************/
/*************************************************************************/
