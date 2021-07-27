/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/mutex.c: Mutex routines for the PSP.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/psp/internal.h"
#include "src/sysdep/psp/thread.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Data structure for a recursive mutex. */
typedef struct RecursiveMutex RecursiveMutex;
struct RecursiveMutex {
    SysSemaphoreID semaphore;
    int lock_count;
    SysThreadID owner;  // Owner's thread ID if currently locked, 0 otherwise.
};

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

SysMutexID sys_mutex_create(int recursive, int initially_locked)
{
    /* The PSP doesn't have native mutexes, so we fake them with semaphores. */
    const int initial_value = (initially_locked ? 0 : 1);
    const SysSemaphoreID semaphore = sys_semaphore_create(initial_value, 1);
    if (UNLIKELY(!semaphore)) {
        return 0;
    }
    if (!recursive) {
        return semaphore;
    }
    RecursiveMutex *mutex = mem_alloc(sizeof(*mutex), 0, 0);
    if (UNLIKELY(!mutex)) {
        DLOG("No memory for recursive mutex structure");
        sys_semaphore_destroy(semaphore);
        return 0;
    }
    mutex->semaphore = semaphore;
    mutex->lock_count = initially_locked ? 1 : 0;
    mutex->owner = initially_locked ? sys_thread_get_id() : 0;
    /* Use the high bit (which can never be set on a regular semaphore ID)
     * to indicate that this is a recursive mutex. */
    return (SysMutexID)((SceUID)mutex | 0x80000000U);
}

/*-----------------------------------------------------------------------*/

void sys_mutex_destroy(SysMutexID mutex_)
{
    SceUID mutex = (SceUID)mutex_;
    if (mutex & 0x80000000U) {
        mutex &= 0x7FFFFFFF;
        RecursiveMutex *mutex_struct = (RecursiveMutex *)mutex;
        sys_semaphore_destroy(mutex_struct->semaphore);
        mem_free(mutex_struct);
    } else {
        sys_semaphore_destroy((SysSemaphoreID)mutex);
    }
}

/*-----------------------------------------------------------------------*/

int sys_mutex_lock(SysMutexID mutex_, float timeout)
{
    SceUID mutex = (SceUID)mutex_;
    if (mutex & 0x80000000U) {
        mutex &= 0x7FFFFFFF;
        RecursiveMutex *mutex_struct = (RecursiveMutex *)mutex;
        const SysThreadID self = sys_thread_get_id();
        if (mutex_struct->owner == self && mutex_struct->lock_count > 0) {
            mutex_struct->lock_count++;
            return 1;
        }
        if (!sys_semaphore_wait(mutex_struct->semaphore, timeout)) {
            return 0;
        }
        mutex_struct->lock_count = 1;
        mutex_struct->owner = self;
        return 1;
    } else {
        return sys_semaphore_wait((SysSemaphoreID)mutex, timeout);
    }
}

/*-----------------------------------------------------------------------*/

void sys_mutex_unlock(SysMutexID mutex_)
{
    SceUID mutex = (SceUID)mutex_;
    if (mutex & 0x80000000U) {
        mutex &= 0x7FFFFFFF;
        RecursiveMutex *mutex_struct = (RecursiveMutex *)mutex;
        if (mutex_struct->lock_count > 1) {
            mutex_struct->lock_count--;
            return;
        }
        mutex_struct->lock_count = 0;
        mutex_struct->owner = 0;
        sys_semaphore_signal(mutex_struct->semaphore);
    } else {
        sys_semaphore_signal((SysSemaphoreID)mutex);
    }
}

/*************************************************************************/
/*************************************************************************/
