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

static int test_read_basic(void)
{
    webmdec_t *handle;
    assert_true(handle = open_test_file("test/data/mono.webm"));

    const void *video_data, *audio_data;
    int video_length, audio_length;
    double video_time, audio_time;
    for (unsigned int i = 0; timestamps[i][0]>=0 || timestamps[i][1]>=0; i++) {
        video_data = audio_data = (void *)1;
        assert_true(webmdec_read_frame(
                        handle,
                        &video_data, &video_length, &video_time,
                        &audio_data, &audio_length, &audio_time));
        assert_near(video_time, timestamps[i][0], 0.001);
        assert_near(audio_time, timestamps[i][1], 0.001);
        if (video_time >= 0) {
            assert_true(video_data != NULL);
            assert_true(video_length > 0);
        } else {
            assert_true(video_data == NULL);
            assert_equal(video_length, 0);
        }
        if (audio_time >= 0) {
            assert_true(audio_data != NULL);
            assert_true(audio_length > 0);
        } else {
            assert_true(audio_data == NULL);
            assert_equal(audio_length, 0);
        }
    }
    assert_false(webmdec_read_frame(
                     handle,
                     &video_data, &video_length, &video_time,
                     &audio_data, &audio_length, &audio_time));
    assert_equal(webmdec_last_error(handle), WEBMDEC_ERROR_STREAM_END);

    webmdec_close(handle);
    return 1;
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

int test_read(void)
{
    int pass = 1;

    pass &= test_read_basic();

    return pass;
}

/*************************************************************************/
/*************************************************************************/
