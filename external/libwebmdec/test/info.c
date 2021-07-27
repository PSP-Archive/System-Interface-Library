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

static int test_info_basic(void)
{
    webmdec_t *handle;
    assert_true(handle = open_test_file("test/data/stereo.webm"));

    assert_equal(webmdec_video_width(handle), 64);
    assert_equal(webmdec_video_height(handle), 32);
    assert_near(webmdec_video_rate(handle), 30, 0.0);  /* Should be exact. */
    assert_equal(webmdec_audio_channels(handle), 2);
    assert_equal(webmdec_audio_rate(handle), 44100);

    webmdec_close(handle);
    return 1;
}

/*-----------------------------------------------------------------------*/

static int test_info_no_audio(void)
{
    webmdec_t *handle;
    assert_true(handle = open_test_file("test/data/no-audio.webm"));

    assert_equal(webmdec_video_width(handle), 64);
    assert_equal(webmdec_video_height(handle), 32);
    assert_near(webmdec_video_rate(handle), 30, 0.0);
    assert_equal(webmdec_audio_channels(handle), 0);
    assert_equal(webmdec_audio_rate(handle), 0);

    webmdec_close(handle);
    return 1;
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

int test_info(void)
{
    int pass = 1;

    pass &= test_info_basic();
    pass &= test_info_no_audio();

    return pass;
}

/*************************************************************************/
/*************************************************************************/
