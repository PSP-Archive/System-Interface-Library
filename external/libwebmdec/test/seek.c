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

static int test_seek_basic(void)
{
    webmdec_t *handle;
    assert_true(handle = open_test_file("test/data/no-audio.webm"));

    void *sixth_frame;
    int sixth_frame_length;
    double first_frame_time, sixth_frame_time;

    const void *video_data;
    int video_length;
    double video_time;
    assert_true(webmdec_read_frame(handle, &video_data, &video_length,
                                   &video_time, NULL, NULL, NULL));
    first_frame_time = video_time;

    /* Skip a few frames so we can seek to a keyframe. */
    assert_true(webmdec_read_frame(handle, &video_data, &video_length, NULL,
                                   NULL, NULL, NULL));
    assert_true(webmdec_read_frame(handle, &video_data, &video_length, NULL,
                                   NULL, NULL, NULL));
    assert_true(webmdec_read_frame(handle, &video_data, &video_length, NULL,
                                   NULL, NULL, NULL));
    assert_true(webmdec_read_frame(handle, &video_data, &video_length, NULL,
                                   NULL, NULL, NULL));

    assert_true(webmdec_read_frame(handle, &video_data, &video_length,
                                   &video_time, NULL, NULL, NULL));
    assert_not_near(video_time, first_frame_time, 0.001);

    assert_true(sixth_frame = malloc(video_length));
    memcpy(sixth_frame, video_data, video_length);
    sixth_frame_length = video_length;
    sixth_frame_time = video_time;

    assert_true(webmdec_read_frame(handle, &video_data, &video_length,
                                   &video_time, NULL, NULL, NULL));
    assert_not_near(video_time, sixth_frame_time, 0.001);
    assert_true(video_length != sixth_frame_length
                || memcmp(video_data, sixth_frame, video_length) != 0);

    assert_true(webmdec_seek(handle, sixth_frame_time - 0.002));
    assert_near(webmdec_tell(handle), sixth_frame_time - 0.002, 0.001);

#if 0  /* FIXME: nestegg can't seem to seek to this keyframe */
    assert_true(webmdec_read_frame(handle, &video_data, &video_length,
                                   &video_time, NULL, NULL, NULL));
    assert_near(video_time, sixth_frame_time, 0.001);
    assert_true(video_length == sixth_frame_length);
    assert_true(memcmp(video_data, sixth_frame, video_length) == 0);
#endif

    webmdec_close(handle);
    return 1;
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

int test_seek(void)
{
    int pass = 1;

    pass &= test_seek_basic();

    return pass;
}

/*************************************************************************/
/*************************************************************************/
