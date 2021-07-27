/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/thread.h: Internal header for thread handling.
 */

#ifndef SIL_SRC_THREAD_H
#define SIL_SRC_THREAD_H

#include "SIL/thread.h"  // Include the public header.

/*************************************************************************/
/************************** Internal interface ***************************/
/*************************************************************************/

/**
 * thread_init:  Initialize the thread and synchronization primitive manager.
 *
 * [Return value]
 *     True on success, false on error.
 */
extern int thread_init(void);

/**
 * thread_cleanup:  Shut down the thread and synchronization primitive manager.
 */
extern void thread_cleanup(void);

/*************************************************************************/
/************************* Test control routines *************************/
/*************************************************************************/

#ifdef SIL_INCLUDE_TESTS

/**
 * TEST_thread_fail_create:  Enable or disable forced failure of all thread
 * creation requests through any of the thread_create*() functions.
 *
 * [Parameters]
 *     fail: True to enable forced failure, false to disable.
 */
extern void TEST_thread_fail_create(int fail);

#endif  // SIL_INCLUDE_TESTS

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_THREAD_H
