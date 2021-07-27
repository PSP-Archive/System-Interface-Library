/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/semaphore.h: Internal header for semaphore routines.
 */

#ifndef SIL_SRC_SEMAPHORE_H
#define SIL_SRC_SEMAPHORE_H

#include "SIL/semaphore.h"  // Include the public header.

/*************************************************************************/
/************************** Internal interface ***************************/
/*************************************************************************/

/**
 * semaphore_max_value:  Return the maximum supported value for a
 * semaphore.  Attempting to create a semaphore with required_max greater
 * than this value will fail.
 */
extern int semaphore_max_value(void);

/*************************************************************************/
/************************* Test control routines *************************/
/*************************************************************************/

#ifdef SIL_INCLUDE_TESTS

/**
 * TEST_semaphore_fail_create:  Enable or disable forced failure of all
 * semaphore creation requests through the semaphore_create() function.
 *
 * [Parameters]
 *     fail: True to enable forced failure, false to disable.
 */
extern void TEST_semaphore_fail_create(int fail);

#endif  // SIL_INCLUDE_TESTS

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SEMAPHORE_H
