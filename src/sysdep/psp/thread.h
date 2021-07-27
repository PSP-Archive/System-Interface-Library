/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/thread.h: Thread management header for the PSP.
 */

#ifndef SIL_SRC_SYSDEP_PSP_THREAD_H
#define SIL_SRC_SYSDEP_PSP_THREAD_H

EXTERN_C_BEGIN

/*************************************************************************/
/*************************************************************************/

/**
 * psp_thread_create_named:  Create and start a new thread with a given
 * name, returning a thread handle usable with the sys_thread_*() functions.
 *
 * This function does not bound the thread priority.
 *
 * [Parameters]
 *     name: Thread name.
 *     stack_size: Stack size, in bytes, or 0 for the default stack size
 *         (must be nonnegative).
 *     priority: Thread priority.
 *     function: Function to execute (must be non-NULL).
 *     param: Arbitrary parameter to pass to the function.
 * [Return value]
 *     New thread handle, or zero on error.
 */
extern SysThreadID psp_thread_create_named(
    const char *name, int priority, int stack_size, int (*function)(void *),
    void *param);

/**
 * psp_start_thread:  Create and start a new thread, returning the kernel
 * thread ID.
 *
 * [Parameters]
 *     name: Thread name.
 *     entry: Thread start address (pointer to function to call).
 *     priority: Thread priority.
 *     stacksize: Thread stack size, in bytes.
 *     args: Thread argument size, in bytes.
 *     argp: Thread argument pointer.
 * [Return value]
 *     Kernel thread ID, or negative on error.
 */
extern SceUID psp_start_thread(const char *name, int (*entry)(SceSize, void *),
                               int priority, int stacksize, SceSize args, void *argp);

/**
 * psp_delete_thread_if_stopped:  Check whether the given thread is
 * stopped, and delete it if so.
 *
 * [Parameters]
 *     thid: Thread handle.
 *     status_ret: Pointer to variable to receive thread exit code
 *         (negative indicates error or abnormal termination); may be NULL.
 * [Return value]
 *     True if thread was stopped, false if the thread is still running.
 */
extern int psp_delete_thread_if_stopped(SceUID thid, int *status_ret);

/**
 * psp_threads_lock:  Prevent all other threads from running.
 *
 * Calls to this function nest, so that other threads will not be able to
 * run until psp_threads_unlock() has been called the same number of times
 * as psp_threads_lock().
 */
extern void psp_threads_lock(void);

/**
 * psp_threads_unlock:  Allow other threads to run.  Does nothing if
 * threads are not locked.
 */
extern void psp_threads_unlock(void);

/**
 * psp_threads_locked:  Return whether other threads are currently locked
 * from running.
 *
 * [Return value]
 *     True if thread switching is locked, false if not.
 */
extern int psp_threads_locked(void);

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_SRC_SYSDEP_PSP_THREAD_H
