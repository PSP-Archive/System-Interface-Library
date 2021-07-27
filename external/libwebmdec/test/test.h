/*
 * libwebmdec: a decoder library for WebM audio/video streams
 * Copyright (c) 2014-2019 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#ifndef LIBWEBMDEC_TEST_H
#define LIBWEBMDEC_TEST_H

/* Make sure we use the copy of webmdec.h in this source tree rather than
 * one (of a possibly different version) installed on the system. */
#include "include/webmdec.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

/*************************************************************************/
/*************************** Assertion macros ****************************/
/*************************************************************************/

/*
 * The macros below can be used to verify that a certain condition holds,
 * for example that a variable has a given value.  If the assertion fails,
 * a message will be logged to standard error and the function will be
 * aborted with a return value of failure.
 */

/**
 * assert_true:  Assert that the given expression is true.
 */
#define assert_true(expression)  do { \
    if (!(expression)) { \
        fprintf(stderr, "%s:%d: %s was not true as expected\n", \
                __FILE__, __LINE__, #expression); \
        return 0; \
    } \
} while (0)

/**
 * assert_false:  Assert that the given expression is false.
 */
#define assert_false(expression)  do { \
    if ((expression)) { \
        fprintf(stderr, "%s:%d: %s was not false as expected\n", \
                __FILE__, __LINE__, #expression); \
        return 0; \
    } \
} while (0)

/**
 * assert_equal:  Assert that an integer expression has a given value.
 */
#define assert_equal(expression,value)  do { \
    const intmax_t __expression = (expression); \
    const intmax_t __value = (value); \
    if (__expression != __value) { \
        fprintf(stderr, "%s:%d: %s had the value %jd but should have been" \
                " %jd\n", __FILE__, __LINE__, #expression, __expression, \
                __value); \
        return 0; \
    } \
} while (0)

/**
 * assert_element_equal:  Assert that an element of an integer array has a
 * given value.
 */
#define assert_element_equal(array,index,value)  do { \
    const intmax_t __index = (index); \
    const intmax_t __value = (value); \
    const intmax_t __actual = (array)[__index]; \
    if (__actual != __value) { \
        fprintf(stderr, "%s:%d: %s[%jd] had the value %jd but should have" \
                " been %jd\n", __FILE__, __LINE__, #array, __index, \
                __actual, __value); \
        return 0; \
    } \
} while (0)

/**
 * assert_near:  Assert that a floating-point expression has a value within
 * a given epsilon of a given value.
 */
#define assert_near(expression,value,epsilon)  do { \
    const double __expression = (expression); \
    const double __value = (value); \
    if (!(fabs(__expression - __value) <= (epsilon))) { \
        fprintf(stderr, "%s:%d: %s had the value %g but should have been" \
                " near %g\n", __FILE__, __LINE__, #expression, __expression, \
                __value); \
        return 0; \
    } \
} while (0)

/**
 * assert_not_near:  Assert that a floating-point expression has a value
 * not within a given epsilon of a given value.
 */
#define assert_not_near(expression,value,epsilon)  do { \
    const double __expression = (expression); \
    const double __value = (value); \
    if (fabs(__expression - __value) <= (epsilon)) { \
        fprintf(stderr, "%s:%d: %s had the value %g but should not have been" \
                " near %g\n", __FILE__, __LINE__, #expression, __expression, \
                __value); \
        return 0; \
    } \
} while (0)

/**
 * assert_element_near:  Assert that an element of a floating-point array
 * has a value within a given epsilon of a given value.
 */
#define assert_element_near(array,index,value,epsilon)  do { \
    const intmax_t __index = (index); \
    const double __value = (value); \
    const double __actual = (array)[__index]; \
    if (fabs(__actual - __value) > (epsilon)) { \
        fprintf(stderr, "%s:%d: %s[%jd] had the value %g but should have" \
                " been near %g\n", __FILE__, __LINE__, #array, __index, \
                __actual, __value); \
        return 0; \
    } \
} while (0)

/*************************************************************************/
/********************* Shared test data declarations *********************/
/*************************************************************************/

/* Frame timestamps for the mono.webm and stereo.webm data files.  Each
 * element is a (video-timestamp, audio-timestamp) pair; one value will
 * always be -1.0, for the type of frame not decoded in that iteration.
 * The list is terminated by an element with both values set to -1.0. */
extern const double timestamps[][2];

/* Frame timestamps for the no-audio.webm data file.  The list is
 * terminated by a value of -1.0. */
extern const double timestamps_no_audio[];

/*************************************************************************/
/****************** Helper function/macro declarations *******************/
/*************************************************************************/

/**
 * lenof:  Return the length of an array, in elements.
 */
#define lenof(array)  (sizeof((array)) / sizeof(*(array)))

/**
 * open_test_file:  Open a libwebmdec handle for a test data file.  This
 * function works regardless of whether stdio support is built into the
 * library.
 *
 * [Parameters]
 *     path: Pathname of file to open (typically "test/data/...").
 * [Return value]
 *     New stream handle, or NULL on error.
 */
extern webmdec_t *open_test_file(const char *path);

/**
 * open_test_file_unseekable:  Open a libwebmdec handle for a test data
 * file in unseekable mode.  This function works regardless of whether
 * stdio support is built into the library.
 *
 * [Parameters]
 *     path: Pathname of file to open (typically "test/data/...").
 * [Return value]
 *     New stream handle, or NULL on error.
 */
extern webmdec_t *open_test_file_unseekable(const char *path);

/*************************************************************************/
/*********************** Test runner declarations ************************/
/*************************************************************************/

/*
 * All test runner functions return a boolean value indicating whether the
 * tests run by that function all succeeded (true) or some tests failed
 * (false).
 */

extern int test_close(void);
extern int test_decode(void);
extern int test_info(void);
extern int test_open_buffer(void);
extern int test_open_callbacks(void);
extern int test_open_file(void);
extern int test_read(void);
extern int test_rewind(void);
extern int test_seek(void);
extern int test_tell(void);

/*************************************************************************/
/*************************************************************************/

#endif  /* LIBWEBMDEC_TEST_H */
