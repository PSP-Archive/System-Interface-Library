/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/windows/condvar.c: System-level condition variable routines
 * for Windows.
 */

/*
 * The Windows XP implementation is based on "Strategies for Implementing
 * POSIX Condition Variables on Win32" by Douglas C. Schmidt and Irfan
 * Pyarali (http://www.cs.wustl.edu/~schmidt/win32-cv-1.html).  That paper
 * asserts that the method is fair in part because SignalObjectAndWait()'s
 * atomicity ensures that all threads have a chance to respond to a signal
 * before any other thread can lock the mutex.  Unfortunately, it turns out
 * that SignalObjectAndWait() is not in fact atomic (making it a singularly
 * useless function), so this implementation has the potential for unfair
 * scheduling behavior.
 *
 * On Windows Vista and later, we use native condition variables, which
 * (at least hopefully) don't suffer from that problem.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/windows/internal.h"

/*************************************************************************/
/*************************** Test control data ***************************/
/*************************************************************************/

#ifdef SIL_INCLUDE_TESTS

uint8_t TEST_windows_condvar_disable_native = 0;

#endif

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Data structure for a condition variable, encapsulating both native
 * condition variables and the XP workaround. */
typedef struct SysCondVar SysCondVar;
struct SysCondVar {
    uint8_t is_native;
    union {
        CONDITION_VARIABLE cv;
        struct {
            /* Number of threads waiting on this condition variable. */
            int num_waiters;
            /* Lock protecting access to num_waiters. */
            CRITICAL_SECTION num_waiters_lock;
            /* Semaphore used to wake waiters. */
            HANDLE wait_sem;
            /* Event object used to signal when all threads have received a
             * broadcast event. */
            HANDLE waiters_done;
            /* Flag: Was the last signal operation a broadcast? */
            uint8_t was_broadcast;
        } s;
    };
};

/* Function pointers for the Vista-and-later native condition variable API. */
VOID (WINAPI *p_InitializeConditionVariable)(
    PCONDITION_VARIABLE ConditionVariable);
BOOL (WINAPI *p_SleepConditionVariableCS)(
    PCONDITION_VARIABLE ConditionVariable, PCRITICAL_SECTION CriticalSection,
    DWORD dwMilliseconds);
VOID (WINAPI *p_WakeAllConditionVariable)(
    PCONDITION_VARIABLE ConditionVariable);
VOID (WINAPI *p_WakeConditionVariable)(
    PCONDITION_VARIABLE ConditionVariable);
#define InitializeConditionVariable (*p_InitializeConditionVariable)
#define SleepConditionVariableCS    (*p_SleepConditionVariableCS)
#define WakeAllConditionVariable    (*p_WakeAllConditionVariable)
#define WakeConditionVariable       (*p_WakeConditionVariable)

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

SysCondVarID sys_condvar_create(void)
{
    if (UNLIKELY(!p_InitializeConditionVariable)) {
        static uint8_t checked = 0;
        if (!checked) {
            checked = 1;
            const HMODULE kernel32 = GetModuleHandle("kernel32.dll");
            p_InitializeConditionVariable = (void *)GetProcAddress(
                kernel32, "InitializeConditionVariable");
            p_SleepConditionVariableCS = (void *)GetProcAddress(
                kernel32, "SleepConditionVariableCS");
            p_WakeAllConditionVariable = (void *)GetProcAddress(
                kernel32, "WakeAllConditionVariable");
            p_WakeConditionVariable = (void *)GetProcAddress(
                kernel32, "WakeConditionVariable");
            if (p_InitializeConditionVariable
             && p_SleepConditionVariableCS
             && p_WakeAllConditionVariable
             && p_WakeConditionVariable) {
                DLOG("Using native condition variables");
            } else {
                DLOG("Using emulated condition variables because native"
                     " functions missing: Init=%p Sleep=%p WakeAll=%p Wake=%p",
                     p_InitializeConditionVariable,
                     p_SleepConditionVariableCS,
                     p_WakeAllConditionVariable,
                     p_WakeConditionVariable);
            }
        }
    }

    SysCondVar *condvar = mem_alloc(sizeof(*condvar), 0, 0);
    if (!condvar) {
        DLOG("No memory for condition variable");
        goto error_return;
    }

#ifdef SIL_INCLUDE_TESTS
    if (!TEST_windows_condvar_disable_native)
#endif
    if (p_InitializeConditionVariable) {
        condvar->is_native = 1;
        InitializeConditionVariable(&condvar->cv);
        return (SysCondVarID)condvar;
    }

    condvar->is_native = 0;

    condvar->s.wait_sem = CreateSemaphore(NULL, 0, 0x7FFFFFFF, NULL);
    if (!condvar->s.wait_sem) {
        DLOG("Failed to create semaphore for condition variable: %s",
             windows_strerror(GetLastError()));
        goto error_free_condvar;
    }

    condvar->s.waiters_done = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!condvar->s.waiters_done) {
        DLOG("Failed to create event object for condition variable: %s",
             windows_strerror(GetLastError()));
        goto error_destroy_wait_sem;
    }

    condvar->s.num_waiters = 0;
    InitializeCriticalSection(&condvar->s.num_waiters_lock);
    condvar->s.was_broadcast = 0;
    return (SysCondVarID)condvar;

  error_destroy_wait_sem:
    CloseHandle(condvar->s.wait_sem);
  error_free_condvar:
    mem_free(condvar);
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

void sys_condvar_destroy(SysCondVarID condvar_)
{
    SysCondVar *condvar = (SysCondVar *)condvar_;

    if (!condvar->is_native) {
        CloseHandle(condvar->s.waiters_done);
        CloseHandle(condvar->s.wait_sem);
    }
    mem_free(condvar);
}

/*-----------------------------------------------------------------------*/

int sys_condvar_wait(SysCondVarID condvar_, SysMutexID mutex_, float timeout)
{
    SysCondVar *condvar = (SysCondVar *)condvar_;
    CRITICAL_SECTION *mutex = (CRITICAL_SECTION *)mutex_;

    if (condvar->is_native) {
        return SleepConditionVariableCS(&condvar->cv, mutex,
                                        timeout_to_ms(timeout));
    }

    /* Add this thread to the set of waiters for broadcasts. */
    EnterCriticalSection(&condvar->s.num_waiters_lock);
    condvar->s.num_waiters++;
    LeaveCriticalSection(&condvar->s.num_waiters_lock);

    /* Normally, a condition variable should release the mutex and enter a
     * wait state as a single atomic operation.  Windows doesn't have such
     * an atomic operation (SignalObjectAndWait() looks like it should be
     * that operation but in fact is not atomic), so it's possible for
     * another thread to squeeze between these two calls, take the mutex,
     * and signal the condition variable.  However, since we use a counting
     * semaphore rather than a boolean event flag for the wait operation,
     * there's no loss of correctness; the WaitForSingleObject() call will
     * just return immediately instead of waiting, and since we increment
     * the waiter count before unlocking the mutex, broadcast operations
     * will always include this thread in the semaphore release count. */
    LeaveCriticalSection(mutex);
    const int retval =
        (WaitForSingleObject(condvar->s.wait_sem, timeout_to_ms(timeout))
         == WAIT_OBJECT_0);

    /* Remove this thread from the waiter set.  Also check whether we
     * need to signal completion to an in-progress broadcast operation. */
    EnterCriticalSection(&condvar->s.num_waiters_lock);
    condvar->s.num_waiters--;
    const int last_waiter =
        condvar->s.was_broadcast && condvar->s.num_waiters == 0;
    LeaveCriticalSection(&condvar->s.num_waiters_lock);

    /* If this thread was the last one to wake from a broadcast operation,
     * signal that the broadcast is complete.  To guarantee fairness of
     * scheduling, this would need to be an atomic operation that both
     * signalled the waiters_done event and waited on the caller's mutex
     * (thus ensuring that this thread gets onto the mutex wait list before
     * any other waiter has a chance to resume), but unfortunately we can't
     * do that in Windows. */
    if (last_waiter) {
        SetEvent(condvar->s.waiters_done);
    }

    /* Relock the caller's mutex before returning. */
    EnterCriticalSection(mutex);
    return retval;
}

/*-----------------------------------------------------------------------*/

void sys_condvar_signal(SysCondVarID condvar_, int broadcast)
{
    SysCondVar *condvar = (SysCondVar *)condvar_;

    if (condvar->is_native) {
        if (broadcast) {
            WakeAllConditionVariable(&condvar->cv);
        } else {
            WakeConditionVariable(&condvar->cv);
        }
        return;
    }

    EnterCriticalSection(&condvar->s.num_waiters_lock);
    const int num_waiters = condvar->s.num_waiters;
    if (!num_waiters) {
        /* No threads waiting, so nothing to do. */
        LeaveCriticalSection(&condvar->s.num_waiters_lock);
        return;
    }

    if (broadcast) {
        condvar->s.was_broadcast = 1;
        /* The critical section is still held here, so no other threads
         * can enter sys_condvar_wait() until after the ReleaseSemaphore()
         * call.  (And in any case, the condition variable API requires
         * the mutex to be held for signal and broadcast as well as wait
         * operations, so no thread can call sys_condvar_wait() while this
         * function is executing anyway.) */
        ReleaseSemaphore(condvar->s.wait_sem, num_waiters, NULL);
        LeaveCriticalSection(&condvar->s.num_waiters_lock);
        WaitForSingleObject(condvar->s.waiters_done, INFINITE);
        /* The semaphore value may still be nonzero here, if any threads
         * timed out on the semaphore wait but we read num_waiters before
         * the timing-out thread decremented it, so clear out any leftover
         * value.  We rely on the API requirement to hold the mutex when
         * calling this function in order to ensure correctness. */
        for (int i = 0; i < num_waiters; i++) {
            if (WaitForSingleObject(condvar->s.wait_sem, 0) != WAIT_OBJECT_0) {
                break;
            }
        }
        /* This is safe without locking the critical section because of
         * the API requirement to hold the mutex. */
        condvar->s.was_broadcast = 0;
    } else {
        LeaveCriticalSection(&condvar->s.num_waiters_lock);
        ReleaseSemaphore(condvar->s.wait_sem, 1, NULL);
    }
}

/*************************************************************************/
/*************************************************************************/
