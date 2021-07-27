/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/userdata.h: Helper declarations and definitions for the user
 * data tests.
 */

#ifndef SIL_SRC_TEST_USERDATA_H
#define SIL_SRC_TEST_USERDATA_H

/*************************************************************************/
/*************************************************************************/

/**
 * CHECK_USERDATA_MEMORY_FAILURES:  Check that the given userdata
 * operation handles memory allocation failures properly.  Similar to the
 * CHECK_MEMORY_FAILURES() macro, except that this macro waits for the
 * operation to complete.
 *
 * Call like:
 *    CHECK_USERDATA_MEMORY_FAILURES(userdata_save_savefile(...))
 * (the parameter should return an operation ID).
 */
#define CHECK_USERDATA_MEMORY_FAILURES(operation) \
    CHECK_MEMORY_FAILURES( \
        (id = (operation)) && (userdata_wait(id), userdata_get_result(id)))

/*-----------------------------------------------------------------------*/

/**
 * run_userdata_tests:  Run the common userdata tests using the given
 * helper functions.
 *
 * [Parameters]
 *     has_data_path: True if userdata_get_data_path() should return non-NULL.
 *     init_func: Function to call before running a test, or NULL if none.
 *         Should return true on success, false on error.
 *     cleanup_func: Function to call before running a test, or NULL if none.
 *         Should return true on success, false on error.
 *     get_screenshot_func: Function to call to get the data for the most
 *         recently saved screenshot, or NULL if this is not possible.
 *         The caller passes in the screenshot number (counting from zero).
 *         Should store the width and height in the passed-in return
 *         variables and return the pixel data in a newly allocated buffer.
 *     make_data_unwritable_func: Function to call to cause subsequent
 *         write operations to fail until the cleanup function is called,
 *         or NULL if this is not possible.
 * [Return value]
 *     True if all tests succeeded, false if some tests failed.
 */
extern int run_userdata_tests(
    int has_data_path,
    int (*init_func)(void),
    int (*cleanup_func)(void),
    void *(*get_screenshot_func)(int index, int *width_ret, int *height_ret),
    void (*make_data_unwritable_func)(void));

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_TEST_USERDATA_H
