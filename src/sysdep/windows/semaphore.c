/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/windows/semaphore.c: System-level semaphore routines for Windows.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/math.h"
#include "src/sysdep.h"
#include "src/sysdep/windows/internal.h"

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
    HANDLE semaphore =
        CreateSemaphore(NULL, initial_value, required_max, NULL);
    if (semaphore) {
        return (SysSemaphoreID)semaphore;
    } else {
        DLOG("Failed to create semaphore: %s",
             windows_strerror(GetLastError()));
        return 0;
    }
}

/*-----------------------------------------------------------------------*/

void sys_semaphore_destroy(SysSemaphoreID semaphore)
{
    CloseHandle((HANDLE)semaphore);
}

/*-----------------------------------------------------------------------*/

int sys_semaphore_wait(SysSemaphoreID semaphore, float timeout)
{
    return WaitForSingleObject((HANDLE)semaphore, timeout_to_ms(timeout))
        == WAIT_OBJECT_0;
}

/*-----------------------------------------------------------------------*/

void sys_semaphore_signal(SysSemaphoreID semaphore)
{
    ReleaseSemaphore((HANDLE)semaphore, 1, NULL);
}

/*************************************************************************/
/*************************************************************************/
