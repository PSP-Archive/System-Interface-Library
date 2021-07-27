/*
 * libwebmdec: a decoder library for WebM audio/video streams
 * Copyright (c) 2014-2019 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include "test/test.h"

/*************************************************************************/
/*********************** Individual test routines ************************/
/*************************************************************************/

static int test_close_null(void)
{
    /* Make sure this doesn't crash. */
    webmdec_close(NULL);

    return 1;
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

int test_close(void)
{
    int pass = 1;

    pass &= test_close_null();

    return pass;
}

/*************************************************************************/
/*************************************************************************/
