/*
 * libwebmdec: a decoder library for WebM audio/video streams
 * Copyright (c) 2014-2019 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include "test/test.h"

#include <stdlib.h>
#include <string.h>

/*************************************************************************/
/*********************** Individual test routines ************************/
/*************************************************************************/

static int test_rewind_basic(void)
{
    webmdec_t *handle;
    assert_true(handle = open_test_file("test/data/no-audio.webm"));

    void *first_frame;
    int first_frame_length;

    const void *video_data;
    int video_length;
    assert_true(webmdec_read_frame(handle, &video_data, &video_length, NULL,
                                   NULL, NULL, NULL));

    assert_true(first_frame = malloc(video_length));
    memcpy(first_frame, video_data, video_length);
    first_frame_length = video_length;

    assert_true(webmdec_read_frame(handle, &video_data, &video_length, NULL,
                                   NULL, NULL, NULL));
    assert_true(video_length != first_frame_length
                || memcmp(video_data, first_frame, video_length) != 0);

    assert_true(webmdec_rewind(handle));
    assert_near(webmdec_tell(handle), 0.0, 0.0);

    assert_true(webmdec_read_frame(handle, &video_data, &video_length, NULL,
                                   NULL, NULL, NULL));
    assert_true(video_length == first_frame_length);
    assert_true(memcmp(video_data, first_frame, video_length) == 0);

    webmdec_close(handle);
    return 1;
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

int test_rewind(void)
{
    int pass = 1;

    pass &= test_rewind_basic();

    return pass;
}

/*************************************************************************/
/*************************************************************************/
