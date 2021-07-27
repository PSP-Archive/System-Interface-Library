/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/mutex.h: Header for mutual-exclusion lock (mutex) functions.
 */

#ifndef SIL_MUTEX_H
#define SIL_MUTEX_H

EXTERN_C_BEGIN

/*************************************************************************/
/************************* Constant definitions **************************/
/*************************************************************************/

/* Constants representing mutex behavior types, used with mutex_create(). */
enum MutexType {
    /* A simple, non-recursive mutex. */
    MUTEX_SIMPLE = 0,
    /* A mutex allowing recursive locking. */
    MUTEX_RECURSIVE = 1,
};
typedef enum MutexType MutexType;

/* Constants representing the state of a mutex, used with mutex_create(). */
enum MutexState {
    MUTEX_UNLOCKED = 0,
    MUTEX_LOCKED = 1,
};
typedef enum MutexState MutexState;

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

/**
 * mutex_create:  Create a mutex.
 *
 * [Parameters]
 *     type: Mutex recursion type (MUTEX_SIMPLE or MUTEX_RECURSIVE).
 *     initial_state: Initial state of mutex (MUTEX_LOCKED or MUTEX_UNLOCKED).
 * [Return value]
 *     New mutex handle, or zero on error.
 */
extern int mutex_create(MutexType type, MutexState initial_state);

/**
 * mutex_destroy:  Destroy a mutex.  Does nothing if mutex == 0.
 *
 * [Parameters]
 *     mutex: Mutex to destroy.
 */
extern void mutex_destroy(int mutex);

/**
 * mutex_lock:  Lock a mutex.  If the mutex is already locked, wait
 * indefinitely for it to be unlocked.
 *
 * If multiple threads attempt to lock a locked mutex, then when the mutex
 * is unlocked, exactly one of the threads will lock and obtain it.  Which
 * thread locks the mutex is unspecified.
 *
 * Behavior when a mutex of type MUTEX_SIMPLE is locked by the same thread
 * multiple times is undefined.  If a MUTEX_RECURSIVE mutex is locked
 * multiple times, it must be unlocked the same number of times before
 * another thread is allowed to lock it.
 *
 * This function includes an implicit memory barrier (like BARRIER()).
 *
 * [Parameters]
 *     mutex: Mutex to lock.
 */
extern void mutex_lock(int mutex);

/**
 * mutex_lock_timeout:  Attempt to lock a mutex like mutex_lock(), but
 * give up after the specified period of time if the mutex does not become
 * available.  A timeout of zero causes the function to return immediately
 * if the mutex cannot be locked.
 *
 * On some systems, timeout measurement may be fairly coarse.  Callers
 * should not rely on this function for precise timing; use time_delay()
 * instead.
 *
 * This function includes an implicit memory barrier (like BARRIER()).
 *
 * [Parameters]
 *     mutex: Mutex to lock.
 *     timeout: Maximum time to wait for the mutex, in seconds.
 * [Return value]
 *     True if the lock operation succeeded, false if not.
 */
extern int mutex_lock_timeout(int mutex, float timeout);

/**
 * mutex_unlock:  Unlock a mutex.  The mutex must be held by the calling
 * thread; if the caller does not hold the mutex, behavior is undefined.
 *
 * This function includes an implicit memory barrier (like BARRIER()).
 *
 * [Parameters]
 *     mutex: Mutex to unlock.
 */
extern void mutex_unlock(int mutex);

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_MUTEX_H
