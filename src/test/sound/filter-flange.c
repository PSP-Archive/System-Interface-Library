/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sound/filter-flange.c: Tests for the flange audio filter.
 */

#include "src/base.h"
#include "src/sound.h"
#include "src/sound/filter.h"
#include "src/test/base.h"
#include "src/test/sound/wavegen.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Expected output data for one loop of a flanged square wave starting at
 * phase multiples of 30 degrees.  We use a square wave period of 8
 * samples, a flange depth of 6 samples (slightly less than one cycle),
 * and a flange period of 12*256 samples (which gives depth lookup table
 * values at 30-degree phase increments).  For simplicity of calculation,
 * we set the frequency to 1Hz; the frequency is not used within the
 * filter except to convert input values from seconds to samples. */

static const int16_t expected_samples_8[12][8] = {
    {10000, 10000, 10000, 10000, -9945, -10000, -10000, -10000},// 0x0
    {6483, 10000, 10000, 10000, -6333, -10000, -10000, -10000}, // 0x0.66E4
    {1250, 5574, 10000, 10000, -1250, -5369, -10000, -10000},   // 0x1.8000
    {1250, 1250, 1250, 9846, -1250, -1250, -1250, -9641},       // 0x3.0000
    {5625, 1250, 1250, 1250, -5775, -1250, -1250, -1250},       // 0x4.8000
    {10000, 6497, 1250, 1250, -10000, -6552, -1250, -1250},     // 0x5.991C
    {10000, 9986, 1250, 1250, -10000, -9931, -1250, -1250},     // 0x6.0000
    {10000, 6446, 1250, 1250, -10000, -6296, -1250, -1250},     // 0x5.991C
    {5625, 1250, 1250, 1250, -5420, -1250, -1250, -1250},       // 0x4.8000
    {1250, 1250, 1352, 10000, -1250, -1250, -1558, -10000},     // 0x3.0000
    {1250, 5663, 10000, 10000, -1250, -5813, -10000, -10000},   // 0x1.8000
    {6483, 10000, 10000, 10000, -6538, -10000, -10000, -10000}, // 0x0.66E4
};

static const int16_t expected_samples_16[12][16] = {
    {10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000,
     -9890, -10000, -10000, -10000, -10000, -10000, -10000, -10000},
    {6483, 10000, 10000, 10000, 10000, 10000, 10000, 10000,
     -6183, -10000, -10000, -10000, -10000, -10000, -10000, -10000},
    {1250, 5574, 10000, 10000, 10000, 10000, 10000, 10000,
     -1250, -5163, -10000, -10000, -10000, -10000, -10000, -10000},
    {1250, 1250, 1250, 9846, 10000, 10000, 10000, 10000,
     -1250, -1250, -1250, -9436, -10000, -10000, -10000, -10000},
    {1250, 1250, 1250, 1250, 5475, 10000, 10000, 10000,
     -1250, -1250, -1250, -1250, -5175, -10000, -10000, -10000},
    {1250, 1250, 1250, 1250, 1250, 4698, 10000, 10000,
     -1250, -1250, -1250, -1250, -1250, -4588, -10000, -10000},
    {1250, 1250, 1250, 1250, 1250, 1319, 10000, 10000,
     -1250, -1250, -1250, -1250, -1250, -1428, -10000, -10000},
    {1250, 1250, 1250, 1250, 1250, 4954, 10000, 10000,
     -1250, -1250, -1250, -1250, -1250, -5254, -10000, -10000},
    {1250, 1250, 1250, 1250, 5830, 10000, 10000, 10000,
     -1250, -1250, -1250, -1250, -6240, -10000, -10000, -10000},
    {1250, 1250, 1352, 10000, 10000, 10000, 10000, 10000,
     -1250, -1250, -1763, -10000, -10000, -10000, -10000, -10000},
    {1250, 5663, 10000, 10000, 10000, 10000, 10000, 10000,
     -1250, -5963, -10000, -10000, -10000, -10000, -10000, -10000},
    {6483, 10000, 10000, 10000, 10000, 10000, 10000, 10000,
     -6593, -10000, -10000, -10000, -10000, -10000, -10000, -10000},
};

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_sound_filter_flange)

/*-----------------------------------------------------------------------*/

TEST(test_mono)
{
    SoundFilterHandle *filter;
    int16_t pcm[512];

    CHECK_TRUE(filter = sound_filter_open_flange(0, 1, 12*256, 6));
    for (int i = 0; i < 12+2; i++) {
        SquareState state = {.period = 8, .num_cycles = 10000,
                             .samples_out = 0};
        square_gen(&state, pcm, 256);
        sound_filter_filter(filter, pcm, 256);
        for (int j = 0; j < 8; j++) {
            if (pcm[j] != expected_samples_8[i%12][j]) {
                FAIL("Bad filter output at step %d sample %d: got %d,"
                     " expected %d",
                     i, j, pcm[j], expected_samples_8[i%12][j]);
            }
        }
    }
    sound_filter_close(filter);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_stereo)
{
    SoundFilterHandle *filter;
    int16_t pcm[512];

    CHECK_TRUE(filter = sound_filter_open_flange(1, 1, 12*256, 6));
    for (int i = 0; i < 12+2; i++) {
        SquareState state = {.period = 8, .num_cycles = 10000,
                             .samples_out = 0};
        stereo_square_gen(&state, pcm, 256);
        sound_filter_filter(filter, pcm, 256);
        for (int j = 0; j < 8; j++) {
            if (pcm[j*2+0] != expected_samples_8[i%12][j]) {
                FAIL("Bad filter output at step %d sample %d/L: got %d,"
                     " expected %d",
                     i, j, pcm[j*2+0], expected_samples_8[i%12][j]);
            }
        }
        for (int j = 0; j < 16; j++) {
            if (pcm[j*2+1] != expected_samples_16[i%12][j]) {
                FAIL("Bad filter output at step %d sample %d/R: got %d,"
                     " expected %d",
                     i, j, pcm[j*2+1], expected_samples_16[i%12][j]);
            }
        }
    }
    sound_filter_close(filter);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_memory_failures)
{
    SoundFilterHandle *filter;
    int16_t pcm[512];

    CHECK_MEMORY_FAILURES(filter = sound_filter_open_flange(0, 1, 12*256, 6));
    for (int i = 0; i < 12+2; i++) {
        SquareState state = {.period = 8, .num_cycles = 10000,
                             .samples_out = 0};
        square_gen(&state, pcm, 256);
        sound_filter_filter(filter, pcm, 256);
        for (int j = 0; j < 8; j++) {
            if (pcm[j] != expected_samples_8[i%12][j]) {
                FAIL("Bad filter output at step %d sample %d: got %d,"
                     " expected %d",
                     i, j, pcm[j], expected_samples_8[i%12][j]);
            }
        }
    }
    sound_filter_close(filter);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_invalid)
{
    CHECK_FALSE(sound_filter_open_flange(0, 0, 1, 1));
    CHECK_FALSE(sound_filter_open_flange(0, 1, 0, 1));
    CHECK_FALSE(sound_filter_open_flange(0, 1, 1, -1));
    CHECK_FALSE(sound_filter_open_flange(0, 32768, 131072, 1));
    CHECK_FALSE(sound_filter_open_flange(0, 32768, 1, 2));

    return 1;
}

/*************************************************************************/
/*************************************************************************/
