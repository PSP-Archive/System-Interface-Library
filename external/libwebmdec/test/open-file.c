/*
 * libwebmdec: a decoder library for WebM audio/video streams
 * Copyright (c) 2014-2019 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include "test/test.h"

#include <errno.h>

/*************************************************************************/
/*********************** Individual test routines ************************/
/*************************************************************************/

static int test_open_file_basic(void)
{
    webmdec_t *handle;
    webmdec_error_t error = (webmdec_error_t)-1;
#ifdef USE_STDIO
    assert_true(handle = webmdec_open_from_file("test/data/stereo.webm",
                                                WEBMDEC_OPEN_ANY, &error));
    assert_equal(error, WEBMDEC_NO_ERROR);
    webmdec_close(handle);
#else
    assert_false(handle = webmdec_open_from_file("test/data/stereo.webm",
                                                 WEBMDEC_OPEN_ANY, &error));
    assert_equal(error, WEBMDEC_ERROR_DISABLED_FUNCTION);
#endif

    return 1;
}

/*-----------------------------------------------------------------------*/

static int test_open_file_errno(void)
{
#ifdef USE_STDIO
    webmdec_error_t error = (webmdec_error_t)-1;
    errno = 0;
    assert_false(webmdec_open_from_file("test/data/no-such-file",
                                        WEBMDEC_OPEN_ANY, &error));
    assert_equal(error, WEBMDEC_ERROR_FILE_OPEN_FAILED);
    assert_equal(errno, ENOENT);
#endif

    return 1;
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

int test_open_file(void)
{
    int pass = 1;

    pass &= test_open_file_basic();
    pass &= test_open_file_errno();

    return pass;
}

/*************************************************************************/
/*************************************************************************/
