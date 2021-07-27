/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/workqueue.h: Header for work queues.
 */

/*
 * "Work queues" allow a caller to submit a unit of work (specifically, a
 * function call) to be executed asynchronously on another thread, while
 * limiting the number of work units which can execute concurrently.
 *
 * In this implementation, a caller first creates a work queue by calling
 * workqueue_create(), then submits work units using workqueue_submit()
 * detects work unit completion using workqueue_poll(), and retrieves the
 * work unit's result with workqueue_wait().  Each work unit consists of a
 * call to a user-specified function, passing in an opaque data pointer and
 * returning an integer result.
 *
 * All functions other than workqueue_create() and workqueue_destroy() are
 * thread-safe, and in particular multiple threads may submit work units to
 * the same queue.  However, multiple threads must not attempt to wait on
 * the same work unit.
 */

#ifndef SIL_WORKQUEUE_H
#define SIL_WORKQUEUE_H

EXTERN_C_BEGIN

/*************************************************************************/
/****************************** Data types *******************************/
/*************************************************************************/

/**
 * WorkUnitFunction:  Type for functions to be executed as work units in
 * a work queue.  These functions take a single pointer parameter, which
 * takes the value passed along with the function pointer when the work
 * unit is submitted.
 *
 * NOTE: Do not try to return a pointer from a work unit function!
 * Pointers are larger than the "int" type in some environments, and any
 * pointer value returned from a work unit will be corrupted in such
 * environments.  If you need to return a pointer, have the caller pass
 * a pointer to a return variable and write into that pointer.
 *
 * [Parameters]
 *     opaque: Opaque data pointer passed to workqueue_submit().
 * [Return value]
 *     Result value to return to the caller via workqueue_wait().
 */
typedef int WorkUnitFunction(void *opaque);

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

/**
 * workqueue_create:  Create a new work queue.
 *
 * This function is thread-hostile: calling any other workqueue_*()
 * function in parallel with this function has undefined behavior.
 *
 * [Parameters]
 *     max_concurrency: Maximum number of work units which can execute
 *         concurrently.
 * [Return value]
 *     New work queue ID (nonzero), or zero on error.
 */
extern int workqueue_create(int max_concurrency);

/**
 * workqueue_destroy:  Destroy a work queue.  If there is any work pending
 * in the work queue, work units which have not been started are discarded,
 * and this function blocks until any currently executing work is complete.
 *
 * If the work queue is being used by multiple threads, it should be
 * considered invalidated at the moment this function is called; any
 * subsequent attempt to operate on the queue, and any wait operation
 * which has not completed when this function is called, has undefined
 * behavior.
 *
 * This function does nothing if wq == 0.
 *
 * This function is thread-hostile if wq != 0: calling any other
 * workqueue_*() function in parallel with this function has undefined
 * behavior.
 *
 * [Parameters]
 *     workqueue: ID of work queue to destroy.
 */
extern void workqueue_destroy(int workqueue);

/**
 * workqueue_is_busy:  Return whether the given work queue is busy (has at
 * least one work unit currently executing or pending execution).
 *
 * Note that the result of this function is only valid for the specific
 * instant in time at which the check was performed.  Even if this
 * function returns false (the queue is idle), if another thread submits
 * work on the queue while this function is running, the queue may no
 * longer be idle by the time the caller receives the result.
 *
 * [Parameters]
 *     workqueue: ID of work queue to check.
 * [Return value]
 *     True if the work queue is busy, false otherwise.
 */
extern int workqueue_is_busy(int workqueue);

/**
 * workqueue_wait_all:  Wait until the given work queue becomes empty (all
 * pending work units have been processed).  If another thread submits new
 * work units to the work queue before the queue becomes empty, this
 * function will wait for those work units as well.
 *
 * The results of any work units waited for by this function are discarded.
 *
 * [Parameters]
 *     workqueue: ID of work queue to wait for.
 */
extern void workqueue_wait_all(int workqueue);

/**
 * workqueue_submit:  Submit a new work unit (a function to call and an
 * opaque pointer value to pass to the function) to the given work queue.
 *
 * The pointer value passed in "argument" is treated as opaque by this
 * function and is passed directly through to the work unit function.
 *
 * [Parameters]
 *     workqueue: Work queue ID.
 *     function: Function to call.
 *     argument: Opaque argument to function.
 * [Return value]
 *     Work unit ID (nonzero), or zero on error.
 */
extern int workqueue_submit(int workqueue, WorkUnitFunction *function,
                            void *argument);

/**
 * workqueue_poll:  Return whether the given work unit has finished
 * processing.
 *
 * Calling this function does not affect the state of the work unit.  In
 * particular, if this function returns true (indicating that the work
 * unit has finished processing), the caller must still call
 * workqueue_wait() to remove the work unit from the queue.
 *
 * [Parameters]
 *     workqueue: Work queue ID.
 *     unit: Work unit ID.
 * [Return value]
 *     False if the work unit is currently executing or waiting to execute,
 *     true if the work unit has finished executing or either ID is invalid.
 */
extern int workqueue_poll(int workqueue, int unit);

/**
 * workqueue_wait:  Wait until the given work unit finishes processing,
 * remove the work unit from its work queue, and return the work unit's
 * result (the return value of the function called).
 *
 * [Parameters]
 *     workqueue: Work queue ID.
 *     unit: Work unit ID.
 * [Return value]
 *     Work unit result, or zero if either ID is invalid.
 */
extern int workqueue_wait(int workqueue, int unit);

/**
 * workqueue_cancel:  Cancel the given work unit, if possible.
 *
 * A work unit may only be cancelled if it has not started execution.
 * If the work unit has started (or has already finished) execution, this
 * function will fail.  Since a work unit can potentially be started at
 * any time after it has been submitted, callers should not rely on being
 * able to cancel a work unit in order to behave correctly.  This function
 * is intended only to help reduce the time required to abort an operation
 * involving multiple work units by skipping execution of work units whose
 * result is known to no longer be needed.
 *
 * [Parameters]
 *     workqueue: Work queue ID.
 *     unit: Work unit ID.
 * [Return value]
 *     True if the work unit was successfully cancelled, false otherwise.
 */
extern int workqueue_cancel(int workqueue, int unit);

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_WORKQUEUE_H
