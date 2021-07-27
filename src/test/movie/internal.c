/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/movie/internal.c: Shared helper routines for movie playback tests.
 */

#include "src/base.h"
#include "src/graphics.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sound.h"
#include "src/sound/mixer.h"  // For sound_mixer_get_pcm().
#include "src/test/base.h"
#include "src/test/movie/internal.h"

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

/**
 * check_audio_sample:  Return whether the given audio sample is reasonably
 * close to the expected value.
 *
 * [Parameters]
 *     sample: Sample value (signed 16-bit).
 *     index: Sample index.
 *     is_right: True if this is the right channel (for failure messages).
 *     period: Period of sine wave.
 *     amplitude: Amplitude of sine wave.
 * [Return value]
 *     True if the check passes, false if it fails.
 */
static int check_audio_sample(int sample, int index, int is_right, int period,
                              int amplitude)
{
    const int phase = index % period;
    const int expected = iroundf(amplitude * dsinf(phase*360.0f/period));
    if (sample < expected - amplitude/8 || sample > expected + amplitude/8) {
        FAIL("Audio sample %d (%s) was %d but should have been near %d", index,
             is_right ? "right" : "left", sample, expected);
    }
    return 1;
}

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

/**
 * check_video_frame:  Return whether the display contains the expected
 * image data for the given movie frame on a black background.
 *
 * [Parameters]
 *     frame: Frame index (0-14, or -1 to check for a black frame).
 *     full: True to check the entire display, false to just check the
 *         portion containing the movie.
 *     smooth_chroma: True if linear interpolation was enabled for chroma
 *         upsampling, false if not.
 * [Return value]
 *     True if the check passes, false if it fails.
 */
int check_video_frame(int frame, int full, int smooth_chroma)
{
    PRECOND(frame < 15);

    const uint8_t rgb[3][2][2][3] = {
        { {{0x1F,0x1F,0x1F}, {0x5F,0x5F,0x5F}},
          {{0x9F,0x9F,0x9F}, {0xDF,0xDF,0xDF}} },
        { {{0x5F,0x00,0xBF}, {0x5F,0x00,0xBF}},
          {{0x5F,0x00,0xBF}, {0x5F,0x00,0xBF}} },
        { {{0x4F,0xA9,0x42}, {0x4F,0x90,0xC3}},
          {{0x82,0x8F,0x42}, {0x82,0x76,0xC3}} },
    };
    const int r00 = frame<0 ? 0 : rgb[frame/5][0][0][0];
    const int g00 = frame<0 ? 0 : rgb[frame/5][0][0][1];
    const int b00 = frame<0 ? 0 : rgb[frame/5][0][0][2];
    const int r01 = frame<0 ? 0 : rgb[frame/5][0][1][0];
    const int g01 = frame<0 ? 0 : rgb[frame/5][0][1][1];
    const int b01 = frame<0 ? 0 : rgb[frame/5][0][1][2];
    const int r10 = frame<0 ? 0 : rgb[frame/5][1][0][0];
    const int g10 = frame<0 ? 0 : rgb[frame/5][1][0][1];
    const int b10 = frame<0 ? 0 : rgb[frame/5][1][0][2];
    const int r11 = frame<0 ? 0 : rgb[frame/5][1][1][0];
    const int g11 = frame<0 ? 0 : rgb[frame/5][1][1][1];
    const int b11 = frame<0 ? 0 : rgb[frame/5][1][1][2];

    const int disp_w = graphics_display_width();
    const int disp_h = graphics_display_height();
    const int width = full ? disp_w : MOVIE_WIDTH;
    const int height = full ? disp_h : MOVIE_HEIGHT;

    uint8_t *pixels = mem_alloc(width*height*4, 0, MEM_ALLOC_TEMP);
    if (!pixels) {
        DLOG("Failed to allocate memory");
        return 0;
    }
    if (!graphics_read_pixels(disp_w/2 - width/2, disp_h/2 - height/2,
                              width, height, pixels)) {
        DLOG("Failed to read pixels");
        mem_free(pixels);
        return 0;
    }
    /* Note that pixels are read from the bottom up! */
    for (int i = 0; i < width*height*4; i += 4) {
        const int x = (i/4) % width;
        const int y = (i/4) / width;
        if (smooth_chroma && frame/5 == 2
         && x >= width/2-MOVIE_WIDTH/2 && x < width/2+MOVIE_WIDTH/2
         && y >= height/2-MOVIE_HEIGHT/2 && y < height/2+MOVIE_HEIGHT/2
         && (x == width/2-1 || y == height/2-1 || y == height/2)) {
            const int weight_left = (x <= width/2-1) + (x < width/2-1);
            const int weight_right = (x >= width/2-1) + (x > width/2-1);
            const int weight_bottom =
                (y <= height/2) + 2*(y <= height/2-1) + (y < height/2-1);
            const int weight_top =
                (y >= height/2-1) + 2*(y >= height/2) + (y > height/2);
            ASSERT(weight_left + weight_right == 2);
            ASSERT(weight_top + weight_bottom == 4);
            /* These are deliberately not rounded because not rounding
             * produces closer results to the actual RGB values without
             * cluttering the code with YUV-to-RGB conversions. */
            const int r = (r00 * weight_left * weight_top +
                           r01 * weight_right * weight_top +
                           r10 * weight_left * weight_bottom +
                           r11 * weight_right * weight_bottom) / 8;
            const int g = (g00 * weight_left * weight_top +
                           g01 * weight_right * weight_top +
                           g10 * weight_left * weight_bottom +
                           g11 * weight_right * weight_bottom) / 8;
            const int b = (b00 * weight_left * weight_top +
                           b01 * weight_right * weight_top +
                           b10 * weight_left * weight_bottom +
                           b11 * weight_right * weight_bottom) / 8;
            CHECK_PIXEL_NEAR(&pixels[i], r,g,b,255, 2, x, y);
        } else if (x >= width/2-MOVIE_WIDTH/2 && x < width/2
                && y >= height/2-MOVIE_HEIGHT/2 && y < height/2) {
            CHECK_PIXEL_NEAR(&pixels[i], r10,g10,b10,255, 2, x, y);
        } else if (x >= width/2 && x < width/2+MOVIE_WIDTH/2
                && y >= height/2-MOVIE_HEIGHT/2 && y < height/2) {
            CHECK_PIXEL_NEAR(&pixels[i], r11,g11,b11,255, 2, x, y);
        } else if (x >= width/2-MOVIE_WIDTH/2 && x < width/2
                && y >= height/2 && y < height/2+MOVIE_HEIGHT/2) {
            CHECK_PIXEL_NEAR(&pixels[i], r00,g00,b00,255, 2, x, y);
        } else if (x >= width/2 && x < width/2+MOVIE_WIDTH/2
                && y >= height/2 && y < height/2+MOVIE_HEIGHT/2) {
            CHECK_PIXEL_NEAR(&pixels[i], r01,g01,b01,255, 2, x, y);
        } else {
            CHECK_PIXEL(&pixels[i], 0,0,0,255, x, y);
        }
    }
    mem_free(pixels);
    return 1;
}

/*-----------------------------------------------------------------------*/

int check_audio_frame(int frame, int stereo, float volume, int skip_samples)
{
    int16_t pcm[MOVIE_SAMPLES_PER_FRAME*2];
    const int num_samples = lenof(pcm) / 2;
    sound_update();
    sound_mixer_get_pcm(pcm, num_samples);

    /* Left channel: sine wave of period 100 samples and amplitude 10000. */
    const int left_period = 100;
    const int left_amplitude = iroundf(10000*volume);
    /* Right channel: sine wave of period 50 samples and amplitude 6000. */
    const int right_period = (stereo ? 50 : left_period);
    const int right_amplitude =
        (stereo ? iroundf(6000*volume) : left_amplitude);

    int audio_pos = frame * MOVIE_SAMPLES_PER_FRAME;
    for (int i = 0; i < num_samples; i++, audio_pos++) {
        if (audio_pos < skip_samples) {
            continue;
        }
        if (!check_audio_sample(pcm[i*2+0], audio_pos, 0,
                                left_period, left_amplitude)) {
            FAIL("check_audio_sample() failed for frame %d", frame);
        }
        if (!check_audio_sample(pcm[i*2+1], audio_pos, 1,
                                right_period, right_amplitude)) {
            FAIL("check_audio_sample() failed for frame %d", frame);
        }
    }

    return 1;
}

/*************************************************************************/
/*************************************************************************/
