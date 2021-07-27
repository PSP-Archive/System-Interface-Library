/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/semaphore.c: Semaphore routines for the PSP.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/math.h"
#include "src/sysdep.h"
#include "src/sysdep/psp/internal.h"
#include "src/sysdep/psp/thread.h"

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int sys_semaphore_max_value(void)
{
    return INT_MAX;
}

/*-----------------------------------------------------------------------*/

SysSemaphoreID sys_semaphore_create(int initial_value, int required_max)
{
    char namebuf[32];
    static uint32_t name_counter;
    strformat(namebuf, sizeof(namebuf), "SysSemaphore%u", name_counter++);

    int32_t semaphore = sceKernelCreateSema(namebuf, 0, initial_value,
                                            required_max, NULL);
    if (semaphore < 0) {
        DLOG("Failed to create semaphore: %s", psp_strerror(semaphore));
        return 0;
    } else {
        return (SysSemaphoreID)semaphore;
    }
}

/*-----------------------------------------------------------------------*/

void sys_semaphore_destroy(SysSemaphoreID semaphore)
{
    sceKernelDeleteSema((SceUID)semaphore);
}

/*-----------------------------------------------------------------------*/

int sys_semaphore_wait(SysSemaphoreID semaphore, float timeout)
{
    if (timeout < 0) {
        sceKernelWaitSema((SceUID)semaphore, 1, NULL);
        return 1;
    } else {
        do {
            unsigned int timeout_usec;
            if (timeout > 1000) {
                timeout_usec = 1000000000;
                timeout -= 1000;
            } else {
                timeout_usec = iceilf(timeout*1000000);
                timeout = 0;
            }
            if (sceKernelWaitSema((SceUID)semaphore, 1, &timeout_usec) == 0) {
                return 1;
            }
        } while (timeout > 0);
        return 0;
    }
}

/*-----------------------------------------------------------------------*/

void sys_semaphore_signal(SysSemaphoreID semaphore)
{
    sceKernelSignalSema((SceUID)semaphore, 1);
}

/*************************************************************************/
/*************************************************************************/
