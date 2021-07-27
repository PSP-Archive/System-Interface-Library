/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/thread.h: Header for thread handling routines.
 */

#ifndef SIL_THREAD_H
#define SIL_THREAD_H

EXTERN_C_BEGIN

/*************************************************************************/
/****************************** Data types *******************************/
/*************************************************************************/

/**
 * ThreadAttributes:  Structure describing attributes for a new thread.
 */
typedef struct ThreadAttributes ThreadAttributes;
struct ThreadAttributes {
    /* Thread priority.  Equivalent to the "priority" parameter to
     * thread_create_with_priority(). */
    int priority;
    /* Stack size, in bytes, or zero for the default stack size.  Note that
     * different systems have different stack size requirements; on some
     * systems, system libraries may expect a certain minimum stack size
     * and crash if not enough stack space is available.  Non-default stack
     * sizes should be used with care. */
    int stack_size;
    /* CPU affinity set (see thread_set_affinity()).  A value of zero
     * causes the new thread to inherit the current thread's affinity
     * mask.  To allow the thread to run on all available cores, set this
     * to ~UINT64_C(0) (all bits set). */
    uint64_t affinity;
    /* Thread name.  The name is a string identifying the thread for use
     * in platform-specific debugging functionality; SIL does not expose
     * an interface for obtaining the name of a thread.  If NULL, the
     * name may or may not be set to an arbitrary string based on
     * platform requirements.  On some platforms, the name may be
     * truncated to a certain length (for example, Linux limits thread
     * names to 15 bytes).  The result of attempting to create a thread
     * with the same name as an existing thread is platform-dependent. */
    const char *name;
};

/**
 * ThreadFunction:  Type for thread functions passed to thread_create()
 * and friends.  These functions take a single pointer parameter, which
 * takes the value passed along with the function pointer when the thread
 * is created.
 *
 * NOTE: Do not try to return a pointer from a thread function!  Pointers
 * are larger than the "int" type in some environments, and any pointer
 * value returned from a thread function will be corrupted in such
 * environments.
 *
 * [Parameters]
 *     opaque: Opaque data pointer passed to thread_create*().
 * [Return value]
 *     Result value to return to the caller via thread_wait() or
 *     thread_wait2().
 */
typedef int ThreadFunction(void *opaque);

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

/**
 * thread_get_num_cores:  Return the number of processing cores (logical
 * CPUs) available for threads.  Cores reserved for the system are not
 * included in the returned count.  If the number of available cores
 * cannot be determined, it is taken to be 1.
 *
 * On systems with multiple cores, explicitly assigning threads which run
 * simultaneously to different cores (see thread_set_affinity()) can
 * improve performance by ensuring that the threads do not interrupt each
 * other.  The value returned by this function can be used as a hint in
 * deciding how to assign threads to cores.
 *
 * [Return value]
 *     Number of processing cores available (at least 1).
 */
extern int thread_get_num_cores(void);

/**
 * thread_create:  Create and start a thread executing the given function
 * at the same priority as the current thread.
 *
 * This function includes an implicit memory barrier (like BARRIER()).
 *
 * [Parameters]
 *     function: Function to execute.
 *     param: Arbitrary parameter to pass to the function.
 * [Return value]
 *     New thread ID, or zero on error.
 */
extern int thread_create(ThreadFunction *function, void *param);

/**
 * thread_create_with_priority:  Create and start a thread executing the
 * given function at the specified priority.  The precise meaning of the
 * priority values is system-dependent, except that:
 *    - A priority of zero is the same priority as the program's initial
 *         thread.
 *    - Positive values have a higher priority than the initial thread
 *         (the new thread will receive at least the same amount of
 *         system resources as, and typically more resources than, the
 *         initial thread).
 *    - Negative values have a lower priority than the initial thread.
 *
 * The only priority value guaranteed to be supported is zero.  If the
 * requested priority value is not supported, the nearest supported value
 * is used instead.  (On systems which do not support setting the thread
 * priority, all priority values will be treated as zero.)
 *
 * This function includes an implicit memory barrier (like BARRIER()).
 *
 * [Parameters]
 *     priority: Thread priority.
 *     function: Function to execute.
 *     param: Arbitrary parameter to pass to the function.
 * [Return value]
 *     New thread ID, or zero on error.
 */
extern int thread_create_with_priority(
    int priority, ThreadFunction *function, void *param);

/**
 * thread_create_with_attr:  Create and start a thread executing the given
 * function with the specified attributes.
 *
 * This function includes an implicit memory barrier (like BARRIER()).
 *
 * [Parameters]
 *     attr: Thread attributes.
 *     function: Function to execute.
 *     param: Arbitrary parameter to pass to the function.
 * [Return value]
 *     New thread ID, or zero on error.
 */
extern int thread_create_with_attr(
    const ThreadAttributes *attr, ThreadFunction *function, void *param);

/**
 * thread_exit:  Terminate the current thread, as if the thread function
 * had returned.
 *
 * This function does not return.
 *
 * [Parameters]
 *     exit_code: Value to return via thread_wait() as the thread's exit code.
 */
extern NORETURN void thread_exit(int exit_code);

/**
 * thread_get_id:  Return the ID of the current thread.  If the current
 * thread was not created with this interface (such as the main thread of
 * the program, on some platforms), this function returns zero.
 *
 * [Return value]
 *     ID of current thread.
 */
extern int thread_get_id(void);

/**
 * thread_get_priority:  Return the priority of the current thread.  The
 * value returned is such that a new thread created with the same value
 * passed to thread_create_with_priority() will create a thread with the
 * same priority.  If the current thread was not created with this
 * interface (such as the main thread of the program, which by definition
 * has priority zero), this function returns zero.
 *
 * [Return value]
 *     Priority of current thread.
 */
extern int thread_get_priority(void);

/**
 * thread_set_affinity:  Modify the set of processing cores on which the
 * current thread should run.
 *
 * The affinity parameter is a bitmask in which each bit indicates whether
 * the thread should be allowed to run on the corresponding processing core.
 * The least-significant bit (value 0x1) corresponds to the first core in a
 * system-defined (constant) order; the next bit (value 0x2) corresponds to
 * the second core, and so on.  In environments with more than 64 cores,
 * only the first 64 cores can be referenced in an affinity set.
 *
 * An affinity mask with no bits set (integer value zero), or a mask in
 * which the only set bits do not correspond to valid cores, is equivalent
 * to a mask with all bits set, allowing the thread to run on any core.
 *
 * The effect of changing the affinity of a thread when the thread is
 * running on a core not included in the new affinity mask is
 * system-dependent; the thread may be rescheduled immediately on one of
 * the cores in the new affinity mask, or it may continue running on its
 * current core until its scheduling quantum expires or it otherwise stops
 * executing.
 *
 * Note that this function may fail even with an otherwise valid affinity
 * mask.  In particular, Mac OS X and iOS do not support core affinity for
 * threads, so this function will always fail on those platforms.
 *
 * By default, new threads inherit the affinity set of the creating thread
 * (see thread_create_with_attr() and ThreadAttributes.affinity for how to
 * change the initial affinity set of a new thread).  The default affinity
 * set of the main thread of the program is system-dependent.
 *
 * [Parameters]
 *     affinity: Affinity mask.
 * [Return value]
 *     True if the affinity mask was set; false on error.
 */
extern int thread_set_affinity(uint64_t affinity);

/**
 * thread_get_affinity:  Return the set of processing cores on which the
 * current thread will run.
 *
 * See thread_set_affinity() for the meaning of the return value.
 *
 * [Return value]
 *     Current affinity mask.
 */
extern uint64_t thread_get_affinity(void);

/**
 * thread_is_running:  Return whether the given thread is still running
 * (i.e., has not yet terminated).
 *
 * Note that even if this routine returns false, meaning the thread has
 * terminated, the caller must still call thread_wait() or thread_wait2()
 * to clean up the thread.
 *
 * [Parameters]
 *     thread: ID of thread to check.
 * [Return value]
 *     True if the thread is still running; false if the thread has
 *     terminated or the thread ID is invalid.
 */
extern int thread_is_running(int thread);

/**
 * thread_wait:  Wait for the given thread to terminate, and return its
 * result value (the value returned by the thread function).
 *
 * This function always succeeds except under the following conditions:
 *    - The thread ID is invalid; this is an error.
 *    - The thread ID specifies the current thread; this is an error.
 *    - Another thread is already waiting for the specified thread; in
 *      this case, behavior is undefined for all relevant threads (the
 *      current thread, the previously waiting thread, and the thread
 *      being waited on).
 *
 * This function includes an implicit memory barrier (like BARRIER()).
 *
 * [Parameters]
 *     thread: ID of thread to wait for.
 * [Return value]
 *     Thread result value, or zero on error.
 */
extern int thread_wait(int thread);

/**
 * thread_wait2:  Wait for the given thread to terminate, and store its
 * result value at the specified location.  On failure, the contents of
 * the specified location are undefined.
 *
 * This function can be used in place of thread_wait() to differentiate
 * between a successful call with a thread result of 0 and a failed call
 * if the thread ID is not known to be valid.  For example, this allows a
 * caller to wait for a thread which might not have been successfully
 * created without needing an explicit test for thread==0 (since this
 * function will just return failure in that case).
 *
 * This function includes an implicit memory barrier (like BARRIER()).
 *
 * [Parameters]
 *     thread: ID of thread to wait for.
 *     result_ret: Pointer to variable to receive the thread result value.
 * [Return value]
 *     True on success, false on error.
 */
extern int thread_wait2(int thread, int *result_ret);

/**
 * thread_yield:  Yield the CPU to another thread.  If no other threads are
 * ready to run, this function returns immediately.
 *
 * This function includes an implicit memory barrier (like BARRIER()).
 */
extern void thread_yield(void);

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_THREAD_H
