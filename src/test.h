/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test.h: Header file for testing functions.
 */

#ifndef SIL_SRC_TEST_H
#define SIL_SRC_TEST_H

#include "SIL/test.h"  // Include the public header.

EXTERN_C_BEGIN

/*************************************************************************/
/*************************************************************************/

#ifdef SIL_INCLUDE_TESTS

/**
 * is_running_tests:  Return whether tests are currently being run.
 *
 * [Return value]
 *     True if tests are currently being run, false otherwise.
 */
extern int is_running_tests(void);

#endif  // SIL_INCLUDE_TESTS

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_SRC_TEST_H
