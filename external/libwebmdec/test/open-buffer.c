/*
 * libwebmdec: a decoder library for WebM audio/video streams
 * Copyright (c) 2014-2019 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include "test/test.h"

#include <stdio.h>
#include <stdlib.h>

/*************************************************************************/
/*********************** Individual test routines ************************/
/*************************************************************************/

static int test_open_buffer_basic(void)
{
    FILE *fp;
    assert_true(fp = fopen("test/data/stereo.webm", "rb"));
    assert_true(fseek(fp, 0, SEEK_END) == 0);
    const long size = ftell(fp);
    assert_true(size > 0);
    assert_true(fseek(fp, 0, SEEK_SET) == 0);

    void *buffer;
    assert_true(buffer = malloc(size));
    assert_equal(fread(buffer, 1, size, fp), size);
    fclose(fp);

    webmdec_error_t error = (webmdec_error_t)-1;
    webmdec_t *handle;
    assert_true(handle = webmdec_open_from_buffer(buffer, size,
                                                  WEBMDEC_OPEN_ANY, &error));
    assert_equal(error, WEBMDEC_NO_ERROR);
    webmdec_close(handle);
    free(buffer);

    return 1;
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

int test_open_buffer(void)
{
    int pass = 1;

    pass &= test_open_buffer_basic();

    return pass;
}

/*************************************************************************/
/*************************************************************************/
