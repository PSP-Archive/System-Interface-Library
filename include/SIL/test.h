/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/test.h: Header for internal test interface.
 */

#ifndef SIL_TEST_H
#define SIL_TEST_H

EXTERN_C_BEGIN

/*************************************************************************/
/*************************************************************************/

/**
 * run_internal_tests:  Run all or a subset of the tests built into the
 * SIL library.  See the definition of tests[] in src/test/test-harness.c
 * for a list of test names that can be passed to this function.
 *
 * Prefixing a test name with "=" causes the dependencies of that test to
 * be skipped; otherwise, all dependencies are tested before the specified
 * test is run.
 *
 * If SIL was not compiled with SIL_INCLUDE_TESTS defined, this function will
 * always return true without doing anything.
 *
 * [Parameters]
 *     tests_to_run: Comma-separated list of tests to run, or the empty
 *         string to run all tests.
 * [Return value]
 *     True if no tests were run or all tests passed; false if any tests
 *     failed.
 */
extern int run_internal_tests(const char *tests_to_run);

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_TEST_H
