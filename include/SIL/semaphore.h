/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/semaphore.h: Header for semaphore functions.
 */

#ifndef SIL_SEMAPHORE_H
#define SIL_SEMAPHORE_H

EXTERN_C_BEGIN

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

/**
 * semaphore_create:  Create a semaphore.
 *
 * The value of required_max should be the greatest value which the
 * semaphore is expected to have; it must be positive and no less than the
 * initial value of the semaphore.  If the semaphore is successfully
 * created, it is guaranteed to be able to take on values up to and
 * including required_max.  However, the result of attempting to increment
 * the semaphore (with semaphore_signal()) beyond required_max is undefined.
 *
 * [Parameters]
 *     initial_value: Initial value for the semaphore (must be nonnegative).
 *     required_max: Required maximum value for the semaphore (see above).
 * [Return value]
 *     New semaphore handle, or zero on error.
 */
extern int semaphore_create(int initial_value, int required_max);

/**
 * semaphore_destroy:  Destroy a semaphore.  Does nothing if semaphore == 0.
 *
 * [Parameters]
 *     semaphore: Semaphore to destroy.
 */
extern void semaphore_destroy(int semaphore);

/**
 * semaphore_wait:  Wait on a semaphore (the P operation in the traditional
 * definition).  This function waits for the semaphore's value to become
 * nonzero, then decrements it by 1 and returns.
 *
 * This function includes an implicit memory barrier (like BARRIER()).
 *
 * [Parameters]
 *     semaphore: Semaphore on which to wait.
 */
extern void semaphore_wait(int semaphore);

/**
 * semaphore_wait_timeout:  Wait on a semaphore like semaphore_wait(), but
 * return after the specified period of time if the semaphore's value does
 * not become nonzero within that time.
 *
 * On some systems, timeout measurement may be fairly coarse.  Callers
 * should not rely on this function for precise timing; use time_delay()
 * instead.
 *
 * This function includes an implicit memory barrier (like BARRIER()).
 *
 * [Parameters]
 *     semaphore: Semaphore on which to wait.
 *     timeout: Maximum time to wait for the semaphore, in seconds.
 * [Return value]
 *     True if the semaphore was signalled before the timeout expired,
 *     false otherwise.
 */
extern int semaphore_wait_timeout(int semaphore, float timeout);

/**
 * semaphore_signal:  Signal a semaphore (the V operation in the traditional
 * definition).  This function increments the value of the semaphore by 1;
 * if any threads are waiting on the semaphore, exactly one will be woken up.
 *
 * This function includes an implicit memory barrier (like BARRIER()).
 *
 * [Parameters]
 *     semaphore: Semaphore to signal.
 */
extern void semaphore_signal(int semaphore);

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_SEMAPHORE_H
