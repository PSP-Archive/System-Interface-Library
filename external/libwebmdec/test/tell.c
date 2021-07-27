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

static int test_tell_basic(void)
{
    webmdec_t *handle;
    assert_true(handle = open_test_file("test/data/mono.webm"));

    assert_near(webmdec_tell(handle), 0.0, 0.0);

    for (unsigned int i = 0; timestamps[i][0]>=0 || timestamps[i][1]>=0; i++) {
        const void *video_data, *audio_data;
        int video_length, audio_length;
        double video_time, audio_time;
        assert_true(webmdec_read_frame(
                        handle,
                        &video_data, &video_length, &video_time,
                        &audio_data, &audio_length, &audio_time));
        if (video_time >= 0) {
            assert_near(webmdec_tell(handle), video_time, 0.001);
        } else {
            assert_near(webmdec_tell(handle), audio_time, 0.001);
        }
    }

    webmdec_close(handle);
    return 1;
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

int test_tell(void)
{
    int pass = 1;

    pass &= test_tell_basic();

    return pass;
}

/*************************************************************************/
/*************************************************************************/
