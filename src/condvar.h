/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/condvar.h: Internal header for condition variable routines.
 */

#ifndef SIL_SRC_CONDVAR_H
#define SIL_SRC_CONDVAR_H

#include "SIL/condvar.h"  // Include the public header.

/*************************************************************************/
/************************* Test control routines *************************/
/*************************************************************************/

#ifdef SIL_INCLUDE_TESTS

/**
 * TEST_condvar_fail_create:  Enable or disable forced failure of all
 * condition variable creation requests through the condvar_create()
 * function.
 *
 * [Parameters]
 *     fail: True to enable forced failure, false to disable.
 */
extern void TEST_condvar_fail_create(int fail);

#endif  // SIL_INCLUDE_TESTS

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_CONDVAR_H
