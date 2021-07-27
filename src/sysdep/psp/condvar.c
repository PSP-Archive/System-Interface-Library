/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/condvar.c: System-level condition variable routines for
 * the PSP (based on the Windows implementation).
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/psp/internal.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Data structure for a condition variable. */
typedef struct SysCondVar SysCondVar;
struct SysCondVar {
    /* Number of threads waiting on this condition variable. */
    int num_waiters;
    /* Lock protecting access to num_waiters. */
    SceUID num_waiters_lock;
    /* Semaphore used to wake waiters. */
    SceUID wait_sem;
    /* Event object used to signal when all threads have received a
     * broadcast event. */
    SceUID waiters_done;
    /* Flag: Was the last signal operation a broadcast? */
    uint8_t was_broadcast;
};

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

SysCondVarID sys_condvar_create(void)
{
    char namebuf[32];

    SysCondVar *condvar = mem_alloc(sizeof(*condvar), 0, 0);
    if (!condvar) {
        DLOG("No memory for condition variable");
        goto error_return;
    }

    strformat(namebuf, sizeof(namebuf), "SysCondVarLock_%08X", (int)condvar);
    condvar->num_waiters_lock = sceKernelCreateSema(namebuf, 0, 1, 1, NULL);
    if (condvar->num_waiters_lock < 0) {
        DLOG("Failed to create lock for condition variable: %s",
             psp_strerror(condvar->num_waiters_lock));
        goto error_free_condvar;
    }

    strformat(namebuf, sizeof(namebuf), "SysCondVarSem_%08X", (int)condvar);
    condvar->wait_sem = sceKernelCreateSema(namebuf, 0, 0, 0x7FFFFFFF, NULL);
    if (condvar->wait_sem < 0) {
        DLOG("Failed to create semaphore for condition variable: %s",
             psp_strerror(condvar->wait_sem));
        goto error_destroy_num_waiters_lock;
    }

    strformat(namebuf, sizeof(namebuf), "SysCondVarEvent_%08X", (int)condvar);
    condvar->waiters_done = sceKernelCreateEventFlag(namebuf, 0, 0, 0);
    if (condvar->waiters_done < 0) {
        DLOG("Failed to create event object for condition variable: %s",
             psp_strerror(condvar->waiters_done));
        goto error_destroy_wait_sem;
    }

    condvar->num_waiters = 0;
    condvar->was_broadcast = 0;
    return (SysCondVarID)condvar;

  error_destroy_wait_sem:
    sceKernelDeleteSema(condvar->wait_sem);
  error_destroy_num_waiters_lock:
    sceKernelDeleteSema(condvar->num_waiters_lock);
  error_free_condvar:
    mem_free(condvar);
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

void sys_condvar_destroy(SysCondVarID condvar_)
{
    SysCondVar *condvar = (SysCondVar *)condvar_;
    sceKernelDeleteEventFlag(condvar->waiters_done);
    sceKernelDeleteSema(condvar->wait_sem);
    sceKernelDeleteSema(condvar->num_waiters_lock);
    mem_free(condvar);
}

/*-----------------------------------------------------------------------*/

int sys_condvar_wait(SysCondVarID condvar_, SysMutexID mutex, float timeout)
{
    SysCondVar *condvar = (SysCondVar *)condvar_;

    /* Add this thread to the set of waiters for broadcasts. */
    sceKernelWaitSema(condvar->num_waiters_lock, 1, NULL);
    condvar->num_waiters++;
    sceKernelSignalSema(condvar->num_waiters_lock, 1);

    /* Normally, a condition variable should release the mutex and enter a
     * wait state as a single atomic operation.  The PSP doesn't have such
     * an atomic operation, but as in the Windows implementation, the use
     * of a counting semaphore means we still function correctly. */
    sys_mutex_unlock(mutex);
    /* wait_sem wasn't created with sys_semaphore_create(), but the
     * sys_semaphore implementation just returns the raw SceUID as the
     * semaphore ID, so we can safely call sys_semaphore_wait() to make use
     * of the timeout handling logic there. */
    const int retval =
        sys_semaphore_wait((SysSemaphoreID)condvar->wait_sem, timeout);

    /* Remove this thread from the waiter set.  Also check whether we
     * need to signal completion to an in-progress broadcast operation. */
    sceKernelWaitSema(condvar->num_waiters_lock, 1, NULL);
    condvar->num_waiters--;
    const int last_waiter = condvar->was_broadcast && condvar->num_waiters==0;
    sceKernelSignalSema(condvar->num_waiters_lock, 1);

    /* If this thread was the last one to wake from a broadcast operation,
     * signal that the broadcast is complete.  To guarantee fairness of
     * scheduling, this would need to be an atomic operation that both
     * signalled the waiters_done event and waited on the caller's mutex
     * (thus ensuring that this thread gets onto the mutex wait list before
     * any other waiter has a chance to resume), but unfortunately we can't
     * do that on the PSP. */
    if (last_waiter) {
        sceKernelSetEventFlag(condvar->waiters_done, 1);
    }

    /* Relock the caller's mutex before returning. */
    sys_mutex_lock(mutex, -1);
    return retval;
}

/*-----------------------------------------------------------------------*/

void sys_condvar_signal(SysCondVarID condvar_, int broadcast)
{
    SysCondVar *condvar = (SysCondVar *)condvar_;

    sceKernelWaitSema(condvar->num_waiters_lock, 1, NULL);
    const int num_waiters = condvar->num_waiters;
    if (!num_waiters) {
        /* No threads waiting, so nothing to do. */
        sceKernelSignalSema(condvar->num_waiters_lock, 1);
        return;
    }

    if (broadcast) {
        condvar->was_broadcast = 1;
        /* The lock is still held here, so no other threads can enter
         * sys_condvar_wait() until after the sceKernelSignalSema() call.
         * (And in any case, the condition variable API requires the mutex
         * to be held for signal and broadcast as well as wait operations,
         * so no thread can call sys_condvar_wait() while this function is
         * executing anyway.) */
        sceKernelSignalSema(condvar->wait_sem, num_waiters);
        sceKernelSignalSema(condvar->num_waiters_lock, 1);
        sceKernelWaitEventFlag(condvar->waiters_done, 1,
                           PSP_EVENT_WAITCLEAR, NULL, NULL);
        /* The semaphore value may still be nonzero here, if any threads
         * timed out on the semaphore wait but we read num_waiters before
         * the timing-out thread decremented it, so clear out any leftover
         * value.  We rely on the API requirement to hold the mutex when
         * calling this function in order to ensure correctness. */
        for (int i = 0; i < num_waiters; i++) {
            unsigned int zero = 0;
            if (sceKernelWaitSema(condvar->wait_sem, 1, &zero) != 0) {
                break;
            }
        }
        /* This is safe without locking the critical section because of
         * the API requirement to hold the mutex. */
        condvar->was_broadcast = 0;
    } else {
        sceKernelSignalSema(condvar->num_waiters_lock, 1);
        sceKernelSignalSema(condvar->wait_sem, 1);
    }
}

/*************************************************************************/
/*************************************************************************/
