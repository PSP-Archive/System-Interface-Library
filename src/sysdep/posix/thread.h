/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/posix/thread.h: POSIX-specific thread management header.
 */

#ifndef SIL_SRC_SYSDEP_POSIX_THREAD_H
#define SIL_SRC_SYSDEP_POSIX_THREAD_H

#include <pthread.h>  // For pthread_t definition.

/*************************************************************************/
/******************** Thread data structure routines *********************/
/*************************************************************************/

/* Structure holding thread data. */

typedef struct SysThread SysThread;
struct SysThread {
    /* Thread handle. */
    pthread_t handle;

    /* Thread name from caller's ThreadAttributes structure. */
    const char *name;

    /* Function to call, and its parameter. */
    int (*function)(void *param);
    void *param;

#ifdef __linux__
    /* Initial priority for this thread (will be applied at thread startup). */
    int initial_priority;
    /* Initial CPU affinity mask (will be applied at thread startup). */
    uint64_t initial_affinity;
#endif

    /* Flag indicating whether the thread has terminated.  We need this
     * because pthreads has no equivalent to sys_thread_is_running(). */
    uint8_t finished;
};

/*************************************************************************/
/********************* SIL-internal utility routines *********************/
/*************************************************************************/

/**
 * posix_thread_create_detached:  Create and start a thread executing the
 * given function.  The function should accept a single pointer parameter
 * and return nothing.
 *
 * Unlike sys_thread_create(), the thread will be destroyed as soon as the
 * function returns; there is no way for the caller to determine when the
 * thread has exited.
 *
 * [Parameters]
 *     function: Function to execute.
 *     param: Arbitrary parameter to pass to the function.
 * [Return value]
 *     True on success, false on error.
 */
extern int posix_thread_create_detached(void (*function)(void *), void *param);

/*************************************************************************/
/********************** Platform-specific functions **********************/
/*************************************************************************/

/*
 * These two functions must be defined by platform-specific code.  The
 * functions can be empty if there is no platform-specific initialization
 * or cleanup required.
 */

/**
 * posix_thread_runner_init:  Called immediately after a new thread is
 * created.  This function should perform any platform-specific
 * initialization required for new threads created through the
 * sys_thread interface.
 *
 * [Parameters]
 *     thread: SysThread structure for the thread.
 */
extern void posix_thread_runner_init(SysThread *thread);

/**
 * posix_thread_runner_cleanup:  Called immediately before a thread
 * terminates, whether by returning from the thread function or by calling
 * thread_exit().  This function should perform any platform-specific
 * cleanup required for threads created through the sys_thread interface.
 *
 * [Parameters]
 *     thread: SysThread structure for the thread.
 */
extern void posix_thread_runner_cleanup(SysThread *thread);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_POSIX_THREAD_H
