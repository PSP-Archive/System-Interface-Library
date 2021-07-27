/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/condvar.h: Header for condition variable functions.
 */

#ifndef SIL_CONDVAR_H
#define SIL_CONDVAR_H

EXTERN_C_BEGIN

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

/**
 * condvar_create:  Create a condition variable.
 *
 * Note that the use of condition variables can incur significant overhead
 * on some systems.  For simple signalling between two threads, semaphores
 * provide equivalent synchronization guarantees with better performance.
 *
 * [Return value]
 *     New condition variable handle, or zero on error.
 */
extern int condvar_create(void);

/**
 * condvar_destroy:  Destroy a condition variable.  Does nothing if
 * condvar == 0.
 *
 * [Parameters]
 *     condvar: Condition variable to destroy.
 */
extern void condvar_destroy(int condvar);

/**
 * condvar_wait:  Wait on a condition variable.
 *
 * This function must be called with the given mutex locked; if the mutex
 * is recursive, it must not be locked recursively (that is, a single
 * unlock operation must fully unlock the mutex).  The calling thread will
 * be put to sleep until another thread calls condvar_signal() or
 * condvar_broadcast(), with the mutex unlocked for the duration of the
 * sleep.  On return, the mutex will once again be locked.
 *
 * Returning from this function does _not_ imply that the shared data
 * protected by the mutex is in any particular state; the caller must
 * explicitly check for the expected state and loop on this function until
 * it is detected.
 *
 * Calling this function without the mutex locked, or using more than one
 * mutex with a given condition variable, results in undefined behavior.
 *
 * This function includes an implicit memory barrier (like BARRIER()).
 *
 * [Parameters]
 *     condvar: Condition variable to wait on.
 *     mutex: Mutex associated with condition variable (must be locked).
 */
extern void condvar_wait(int condvar, int mutex);

/**
 * condvar_wait_timeout:  Attempt to wait on a condition variable like
 * condvar_wait(), but give up after the specified period of time if the
 * condition variable is not signalled.
 *
 * Note that the timeout does not apply to relocking the mutex on return,
 * so this function has the potential to block indefinitely regardless of
 * the specified timeout.
 *
 * On some systems, timeout measurement may be fairly coarse.  Callers
 * should not rely on this function for precise timing; use time_delay()
 * instead.
 *
 * This function includes an implicit memory barrier (like BARRIER()).
 *
 * [Parameters]
 *     condvar: Condition variable to wait on.
 *     mutex: Mutex associated with condition variable (must be locked).
 *     timeout: Maximum time to wait on the condition variable, in seconds.
 * [Return value]
 *     True if the condition variable was signalled; false if the timeout
 *     expired.
 */
extern int condvar_wait_timeout(int condvar, int mutex, float timeout);

/**
 * condvar_signal:  Signal a condition variable, allowing at least one
 * thread waiting on the condition variable to proceed.
 *
 * Behavior is undefined if the caller does not hold the mutex associated
 * with the condition variable (the mutex passed to condvar_wait() or
 * condvar_wait_timeout() for the condition variable).  The caller should
 * hold the mutex until after this function returns, at which point it may
 * safely unlock the mutex.
 *
 * Note that this function is defined as allowing "at least one" thread to
 * resume instead of "exactly one" thread because it is nontrivial (and
 * therefore expensive) to ensure that exactly one thread is awoken in
 * environments with true concurrent execution, such as multi-core
 * processors.  For this reason, even if all users of a condition variable
 * call this function and not condvar_broadcast(), waiters on the condition
 * variable must explicitly check the state of the shared data after
 * resuming from the wait.
 *
 * This function includes an implicit memory barrier (like BARRIER()).
 *
 * [Parameters]
 *     condvar: Condition variable to signal.
 */
extern void condvar_signal(int condvar);

/**
 * condvar_broadcast:  Signal a condition variable, allowing all threads
 * waiting on the condition variable to proceed.
 *
 * Behavior is undefined if the caller does not hold the mutex associated
 * with the condition variable (the mutex passed to condvar_wait() or
 * condvar_wait_timeout() for the condition variable).
 *
 * This function includes an implicit memory barrier (like BARRIER()).
 *
 * [Parameters]
 *     condvar: Condition variable to signal.
 */
extern void condvar_broadcast(int condvar);

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_CONDVAR_H
