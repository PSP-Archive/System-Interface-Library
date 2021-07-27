/*
 * libwebmdec: a decoder library for WebM audio/video streams
 * Copyright (c) 2014-2019 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include "test/test.h"

#include <math.h>
#define TWO_PIf  6.2831853f

/*************************************************************************/
/*********************** Individual test routines ************************/
/*************************************************************************/

static int test_decode_basic(void)
{
    webmdec_t *handle;
    assert_true(handle = open_test_file("test/data/mono.webm"));
    assert_equal(webmdec_video_width(handle), 64);
    assert_equal(webmdec_video_height(handle), 32);
    assert_equal(webmdec_audio_channels(handle), 1);
    assert_equal(webmdec_audio_rate(handle), 44100);

    const void *video_data;
    const float *audio_data;
    int audio_length;
    double video_time, audio_time;

#if defined(DECODE_AUDIO) && defined(DECODE_VIDEO)

    int video_frame = 0, audio_sample = 0;

    for (unsigned int i = 0; timestamps[i][0]>=0 || timestamps[i][1]>=0; i++) {
        video_data = audio_data = (void *)1;
        assert_true(webmdec_decode_frame(
                        handle, &video_data, &video_time,
                        &audio_data, &audio_length, &audio_time));
        assert_near(video_time, timestamps[i][0], 0.001);
        assert_near(audio_time, timestamps[i][1], 0.001);

        if (video_time >= 0) {
            assert_true(video_data != NULL);
            const unsigned char *data = video_data;
            if (video_frame < 5) {
                for (int y = 0; y < 32; y++) {
                    for (int x = 0; x < 64; x++) {
                        int value;
                        if (y < 16) {
                            value = (x<32 ? 0x2B : 0x62);
                        } else {
                            value = (x<32 ? 0x99 : 0xD0);
                        }
                        assert_element_equal(data, y*64+x, value);
                    }
                }
                for (int j = 0; j < (64/2)*(32/2)*2; j++) {
                    assert_element_equal(data, 64*32+j, 0x80);
                }
            } else {
                for (int j = 0; j < 64*32 + (64/2)*(32/2)*2; j++) {
                    int value;
                    if (j < 64*32) {
                        value = 0x3B;
                    } else if (j < 64*32 + (64/2)*(32/2)) {
                        value = 0xC6;
                    } else {
                        value = 0x9C;
                    }
                    assert_element_equal(data, j, value);
                }
            }
            video_frame++;
        } else {
            assert_true(video_data == NULL);
        }
        if (audio_time >= 0) {
            assert_true(audio_data != NULL);
            assert_true(audio_length >= 0);
            /* Munge the sample pointer so we can use audio_sample as the
             * array index in assert_element_near(). */
            audio_data -= audio_sample;
            for (int j = 0; j < audio_length && audio_sample < 14700;
                 j++, audio_sample++)
            {
                const float amplitude = 10000.0f/32767.0f;
                const int period = 100;
                const int phase = audio_sample % period;
                const float expected = amplitude * sinf(phase*TWO_PIf/period);
                assert_element_near(audio_data, audio_sample, expected,
                                    amplitude/20);
            }
        } else {
            assert_true(audio_data == NULL);
            assert_equal(audio_length, 0);
        }
    }

    assert_false(webmdec_decode_frame(
                     handle, &video_data, &video_time,
                     &audio_data, &audio_length, &audio_time));
    assert_equal(webmdec_last_error(handle), WEBMDEC_ERROR_STREAM_END);

#else  // !DECODE_AUDIO || !DECODE_VIDEO

    assert_false(webmdec_decode_frame(
                     handle, &video_data, &video_time,
                     &audio_data, &audio_length, &audio_time));
    assert_equal(webmdec_last_error(handle), WEBMDEC_ERROR_DISABLED_FUNCTION);

#endif

    webmdec_close(handle);
    return 1;
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

int test_decode(void)
{
    int pass = 1;

    pass &= test_decode_basic();

    return pass;
}

/*************************************************************************/
/*************************************************************************/
