/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/windows/mutex.c: Mutex routines for Windows.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/windows/internal.h"

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

SysMutexID sys_mutex_create(UNUSED int recursive, int initially_locked)
{
    CRITICAL_SECTION *mutex = mem_alloc(sizeof(*mutex), 0, 0);
    if (UNLIKELY(!mutex)) {
        DLOG("Failed to allocate memory for mutex");
        return NULL;
    }
    InitializeCriticalSection(mutex);
    if (initially_locked) {
        EnterCriticalSection(mutex);
    }
    return (SysMutexID)mutex;
}

/*-----------------------------------------------------------------------*/

void sys_mutex_destroy(SysMutexID mutex)
{
    mem_free((CRITICAL_SECTION *)mutex);
}

/*-----------------------------------------------------------------------*/

int sys_mutex_lock(SysMutexID mutex, float timeout)
{
    if (timeout < 0) {
        EnterCriticalSection((CRITICAL_SECTION *)mutex);
        return 1;
    } else {
        if (timeout > 0) {
            if (TryEnterCriticalSection((CRITICAL_SECTION *)mutex)) {
                return 1;
            }
            /* Both timeGetTime() and GetTickCount() return the current
             * time in milliseconds, but timeGetTime() appears to be more
             * precise in at least some versions of Windows.  See:
             * https://randomascii.wordpress.com/2013/05/09/timegettime-versus-gettickcount/ */
            DWORD limit = timeGetTime() + timeout_to_ms(timeout);
            Sleep(1);
            while ((int32_t)(limit - timeGetTime()) > 0) {
                if (TryEnterCriticalSection((CRITICAL_SECTION *)mutex)) {
                    return 1;
                }
                Sleep(1);
            }
        }
        return TryEnterCriticalSection((CRITICAL_SECTION *)mutex);
    }
}

/*-----------------------------------------------------------------------*/

void sys_mutex_unlock(SysMutexID mutex)
{
    LeaveCriticalSection((CRITICAL_SECTION *)mutex);
}

/*************************************************************************/
/*************************************************************************/
