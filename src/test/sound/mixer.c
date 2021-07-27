/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sound/mixer.c: Tests for the software audio mixer.
 */

#include "src/base.h"
#include "src/memory.h"
#include "src/sound.h"
#include "src/sound/mixer.h"
#include "src/test/base.h"
#include "src/test/sound/wavegen.h"
#include "src/thread.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Sampling rate to use for tests. */
#define MIX_RATE  16

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_sound_mixer)

TEST_INIT(init)
{
    CHECK_TRUE(thread_init());
    CHECK_TRUE(sound_mixer_init(2, MIX_RATE));
    return 1;
}

TEST_CLEANUP(cleanup)
{
    sound_mixer_cleanup();
    thread_cleanup();
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_init_cleanup)
{
    /* The mixer has already been initialized.  Check that double
     * initialization fails. */
    CHECK_FALSE(sound_mixer_init(2, MIX_RATE));

    /* Check that the mixer can be closed and reinitialized. */
    sound_mixer_cleanup();
    CHECK_TRUE(sound_mixer_init(2, MIX_RATE));

    /* Check that double cleanup does not crash. */
    sound_mixer_cleanup();
    sound_mixer_cleanup();

    /* Check that invalid initialization parameters are handled properly. */
    CHECK_FALSE(sound_mixer_init(0, 44100));
    CHECK_FALSE(sound_mixer_init(-1, 44100));
    CHECK_FALSE(sound_mixer_init(2, 0));
    CHECK_FALSE(sound_mixer_init(3, -44100));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_init_memory_failure)
{
    sound_mixer_cleanup();
    CHECK_MEMORY_FAILURES(sound_mixer_init(2, MIX_RATE));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_pcm)
{
    int16_t buf[2] = {12345, 23456};

    sound_mixer_get_pcm(NULL, 1);  // Just make sure it doesn't crash.
    sound_mixer_get_pcm(buf, 0);
    CHECK_INTEQUAL(buf[0], 12345);
    CHECK_INTEQUAL(buf[1], 23456);
    sound_mixer_get_pcm(buf, -1);
    CHECK_INTEQUAL(buf[0], 12345);
    CHECK_INTEQUAL(buf[1], 23456);
    sound_mixer_get_pcm(buf, 1);
    CHECK_INTEQUAL(buf[0], 0);
    CHECK_INTEQUAL(buf[1], 0);

    /* Check that sound_mixer_get_pcm() doesn't crash if called when the
     * mixer is not initialized. */
    sound_mixer_cleanup();
    buf[0] = buf[1] = 1;
    sound_mixer_get_pcm(buf, 1);
    CHECK_INTEQUAL(buf[0], 0);
    CHECK_INTEQUAL(buf[1], 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_1ch_mono)
{
    SquareState state = {.period = 4, .num_cycles = 1, .samples_out = 0};
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_start(1);

    int16_t pcm[5*2];
    sound_mixer_get_pcm(pcm, 5);
    CHECK_INTEQUAL(pcm[0*2+0], 10000);
    CHECK_INTEQUAL(pcm[0*2+1], 10000);
    CHECK_INTEQUAL(pcm[1*2+0], 10000);
    CHECK_INTEQUAL(pcm[1*2+1], 10000);
    CHECK_INTEQUAL(pcm[2*2+0], -10000);
    CHECK_INTEQUAL(pcm[2*2+1], -10000);
    CHECK_INTEQUAL(pcm[3*2+0], -10000);
    CHECK_INTEQUAL(pcm[3*2+1], -10000);
    CHECK_INTEQUAL(pcm[4*2+0], 0);
    CHECK_INTEQUAL(pcm[4*2+1], 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_1ch_stereo)
{
    SquareState state = {.period = 2, .num_cycles = 2, .samples_out = 0};
    sound_mixer_setdata(1, stereo_square_gen, &state, 1);
    sound_mixer_start(1);

    int16_t pcm[5*2];
    sound_mixer_get_pcm(pcm, 5);
    CHECK_INTEQUAL(pcm[0*2+0], 10000);
    CHECK_INTEQUAL(pcm[0*2+1], 10000);
    CHECK_INTEQUAL(pcm[1*2+0], -10000);
    CHECK_INTEQUAL(pcm[1*2+1], 10000);
    CHECK_INTEQUAL(pcm[2*2+0], 10000);
    CHECK_INTEQUAL(pcm[2*2+1], -10000);
    CHECK_INTEQUAL(pcm[3*2+0], -10000);
    CHECK_INTEQUAL(pcm[3*2+1], -10000);
    CHECK_INTEQUAL(pcm[4*2+0], 0);
    CHECK_INTEQUAL(pcm[4*2+1], 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_2ch_mono)
{
    SquareState state1 = {.period = 4, .num_cycles = 1, .samples_out = 0};
    sound_mixer_setdata(1, square_gen, &state1, 0);
    sound_mixer_start(1);

    SquareState state2 = {.period = 2, .num_cycles = 2, .samples_out = 0};
    sound_mixer_setdata(2, square_gen, &state2, 0);
    sound_mixer_start(2);

    int16_t pcm[5*2];
    sound_mixer_get_pcm(pcm, 5);
    CHECK_INTEQUAL(pcm[0*2+0], 20000);
    CHECK_INTEQUAL(pcm[0*2+1], 20000);
    CHECK_INTEQUAL(pcm[1*2+0], 0);
    CHECK_INTEQUAL(pcm[1*2+1], 0);
    CHECK_INTEQUAL(pcm[2*2+0], 0);
    CHECK_INTEQUAL(pcm[2*2+1], 0);
    CHECK_INTEQUAL(pcm[3*2+0], -20000);
    CHECK_INTEQUAL(pcm[3*2+1], -20000);
    CHECK_INTEQUAL(pcm[4*2+0], 0);
    CHECK_INTEQUAL(pcm[4*2+1], 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_2ch_stereo)
{
    SquareState state1 = {.period = 4, .num_cycles = 1, .samples_out = 0};
    sound_mixer_setdata(1, stereo_square_gen, &state1, 1);
    sound_mixer_start(1);

    SquareState state2 = {.period = 2, .num_cycles = 2, .samples_out = 0};
    sound_mixer_setdata(2, stereo_square_gen, &state2, 1);
    sound_mixer_start(2);

    int16_t pcm[5*2];
    sound_mixer_get_pcm(pcm, 5);
    CHECK_INTEQUAL(pcm[0*2+0], 20000);
    CHECK_INTEQUAL(pcm[0*2+1], 20000);
    CHECK_INTEQUAL(pcm[1*2+0], 0);
    CHECK_INTEQUAL(pcm[1*2+1], 20000);
    CHECK_INTEQUAL(pcm[2*2+0], 0);
    CHECK_INTEQUAL(pcm[2*2+1], 0);
    CHECK_INTEQUAL(pcm[3*2+0], -20000);
    CHECK_INTEQUAL(pcm[3*2+1], 0);
    CHECK_INTEQUAL(pcm[4*2+0], 0);
    CHECK_INTEQUAL(pcm[4*2+1], 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_2ch_mix)
{
    SquareState state1 = {.period = 4, .num_cycles = 1, .samples_out = 0};
    sound_mixer_setdata(1, square_gen, &state1, 0);
    sound_mixer_start(1);

    SquareState state2 = {.period = 2, .num_cycles = 2, .samples_out = 0};
    sound_mixer_setdata(2, stereo_square_gen, &state2, 1);
    sound_mixer_start(2);

    int16_t pcm[5*2];
    sound_mixer_get_pcm(pcm, 5);
    CHECK_INTEQUAL(pcm[0*2+0], 20000);
    CHECK_INTEQUAL(pcm[0*2+1], 20000);
    CHECK_INTEQUAL(pcm[1*2+0], 0);
    CHECK_INTEQUAL(pcm[1*2+1], 20000);
    CHECK_INTEQUAL(pcm[2*2+0], 0);
    CHECK_INTEQUAL(pcm[2*2+1], -20000);
    CHECK_INTEQUAL(pcm[3*2+0], -20000);
    CHECK_INTEQUAL(pcm[3*2+1], -20000);
    CHECK_INTEQUAL(pcm[4*2+0], 0);
    CHECK_INTEQUAL(pcm[4*2+1], 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_2ch_one_silent)
{
    SquareState state = {.period = 4, .num_cycles = 1, .samples_out = 0};
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_start(1);

    SquareState state2 = {.period = 2, .num_cycles = 2, .samples_out = 0};
    sound_mixer_setdata(2, square_gen, &state2, 0);
    sound_mixer_setvol(2, 0);
    sound_mixer_start(2);

    int16_t pcm[5*2];
    sound_mixer_get_pcm(pcm, 5);
    CHECK_INTEQUAL(pcm[0*2+0], 10000);
    CHECK_INTEQUAL(pcm[0*2+1], 10000);
    CHECK_INTEQUAL(pcm[1*2+0], 10000);
    CHECK_INTEQUAL(pcm[1*2+1], 10000);
    CHECK_INTEQUAL(pcm[2*2+0], -10000);
    CHECK_INTEQUAL(pcm[2*2+1], -10000);
    CHECK_INTEQUAL(pcm[3*2+0], -10000);
    CHECK_INTEQUAL(pcm[3*2+1], -10000);
    CHECK_INTEQUAL(pcm[4*2+0], 0);
    CHECK_INTEQUAL(pcm[4*2+1], 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_empty_stream)
{
    SquareState state1 = {.period = 4, .num_cycles = 1, .samples_out = 0};
    sound_mixer_setdata(1, square_gen, &state1, 0);
    sound_mixer_start(1);

    SquareState state2 = {.period = 2, .num_cycles = 0, .samples_out = 0};
    sound_mixer_setdata(2, square_gen, &state2, 0);
    sound_mixer_start(2);

    int16_t pcm[5*2];
    sound_mixer_get_pcm(pcm, 5);
    CHECK_INTEQUAL(pcm[0*2+0], 10000);
    CHECK_INTEQUAL(pcm[0*2+1], 10000);
    CHECK_INTEQUAL(pcm[1*2+0], 10000);
    CHECK_INTEQUAL(pcm[1*2+1], 10000);
    CHECK_INTEQUAL(pcm[2*2+0], -10000);
    CHECK_INTEQUAL(pcm[2*2+1], -10000);
    CHECK_INTEQUAL(pcm[3*2+0], -10000);
    CHECK_INTEQUAL(pcm[3*2+1], -10000);
    CHECK_INTEQUAL(pcm[4*2+0], 0);
    CHECK_INTEQUAL(pcm[4*2+1], 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_resolution)
{
    int16_t state = 0;
    int16_t *pcm;
    ASSERT(pcm = mem_alloc(65536*4, 4, MEM_ALLOC_TEMP));

    sound_mixer_setdata(1, sawtooth_gen, &state, 0);
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 65536);

    for (int i = 0; i < 65536; i++) {
        if (pcm[i*2+0] != (int16_t)i || pcm[i*2+1] != (int16_t)i) {
            FAIL("Lost sample resolution at %d: output = %d/%d (should be %d)",
                 i, pcm[i*2+0], pcm[i*2+1], i);
        }
    }

    mem_free(pcm);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* Test various buffer sizes to check that optimized code doesn't break
 * under particular conditions. */
TEST(test_pcm_buffer_size_mono)
{
    int16_t state = 0;
    int16_t *pcm;
    const int max_len = 256;
    ASSERT(pcm = mem_alloc((max_len+2)*4, 8, MEM_ALLOC_TEMP));

    sound_mixer_setdata(1, sawtooth_gen, &state, 0);
    sound_mixer_setvol(1, 2);
    sound_mixer_setpan(1, 0.5);
    sound_mixer_start(1);

    uint16_t current = 0;
    for (int len = 1; len <= max_len; len++) {
        mem_fill32(pcm, ((current-1)<<16) | (current-1), (max_len+2)*4);
        sound_mixer_get_pcm(pcm, len);
        for (int i = 0; i < max_len+2; i++) {
            const int16_t expected_l =
                (int16_t)((i < len) ? current+i : current-1);
            const int16_t expected_r =
                ((i < len)
                 ? bound((int16_t)(current+i) * 3, -32768, 32767)
                 : (int16_t)(current-1));
            if (pcm[i*2+0] != expected_l || pcm[i*2+1] != expected_r) {
                FAIL("Wrong sample value at %d for length %d (aligned):"
                     " output = %d/%d (should be %d/%d)", i, len,
                     pcm[i*2+0], pcm[i*2+1], expected_l, expected_r);
            }
        }
        current += len;

        mem_fill32(pcm, ((current-1)<<16) | (current-1), (max_len+2)*4);
        sound_mixer_get_pcm(&pcm[2], len);
        for (int i = 0; i < max_len+2; i++) {
            const int16_t expected_l =
                (int16_t)((i >= 1 && i <= len) ? current+(i-1) : current-1);
            const int16_t expected_r =
                ((i >= 1 && i <= len)
                 ? bound((int16_t)(current+(i-1)) * 3, -32768, 32767)
                 : (int16_t)(current-1));
            if (pcm[i*2+0] != expected_l || pcm[i*2+1] != expected_r) {
                FAIL("Wrong sample value at %d for length %d (unaligned):"
                     " output = %d/%d (should be %d/%d)", i, len,
                     pcm[i*2+0], pcm[i*2+1], expected_l, expected_r);
            }
        }
        current += len;
    }

    mem_free(pcm);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_pcm_buffer_size_stereo)
{
    int16_t state = 0;
    int16_t *pcm;
    const int max_len = 256;
    ASSERT(pcm = mem_alloc((max_len+2)*4, 8, MEM_ALLOC_TEMP));

    sound_mixer_setdata(1, sawtooth_stereo_gen, &state, 1);
    sound_mixer_setvol(1, 1);
    sound_mixer_start(1);

    uint16_t current = 0;
    for (int len = 1; len <= max_len; len++) {
        mem_fill32(pcm, ((current-1)<<16) | (current-1), (max_len+2)*4);
        sound_mixer_get_pcm(pcm, len);
        for (int i = 0; i < max_len+2; i++) {
            const int16_t expected_l =
                (int16_t)((i < len) ? current+(2*i+0) : current-1);
            const int16_t expected_r =
                (int16_t)((i < len) ? current+(2*i+1) : current-1);
            if (pcm[i*2+0] != expected_l || pcm[i*2+1] != expected_r) {
                FAIL("Wrong sample value at %d for length %d (aligned):"
                     " output = %d/%d (should be %d/%d)", i, len,
                     pcm[i*2+0], pcm[i*2+1], expected_l, expected_r);
            }
        }
        current += len*2;

        mem_fill32(pcm, ((current-1)<<16) | (current-1), (max_len+2)*4);
        sound_mixer_get_pcm(&pcm[2], len);
        for (int i = 0; i < max_len+2; i++) {
            const int16_t expected_l =
                (int16_t)((i >= 1 && i <= len) ? current+(2*i-2) : current-1);
            const int16_t expected_r =
                (int16_t)((i >= 1 && i <= len) ? current+(2*i-1) : current-1);
            if (pcm[i*2+0] != expected_l || pcm[i*2+1] != expected_r) {
                FAIL("Wrong sample value at %d for length %d (unaligned):"
                     " output = %d/%d (should be %d/%d)", i, len,
                     pcm[i*2+0], pcm[i*2+1], expected_l, expected_r);
            }
        }
        current += len*2;
    }

    mem_free(pcm);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_base_volume)
{
    sound_mixer_set_base_volume(1);

    SquareState state = {.period = 4, .num_cycles = 2, .samples_out = 0};
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_start(1);

    int16_t pcm[4*2];
    sound_mixer_get_pcm(pcm, 4);
    CHECK_INTEQUAL(pcm[0*2+0], 10000);
    CHECK_INTEQUAL(pcm[0*2+1], 10000);
    CHECK_INTEQUAL(pcm[1*2+0], 10000);
    CHECK_INTEQUAL(pcm[1*2+1], 10000);
    CHECK_INTEQUAL(pcm[2*2+0], -10000);
    CHECK_INTEQUAL(pcm[2*2+1], -10000);
    CHECK_INTEQUAL(pcm[3*2+0], -10000);
    CHECK_INTEQUAL(pcm[3*2+1], -10000);

    /* Changing the base volume should immediately take effect. */
    sound_mixer_set_base_volume(0.5);
    sound_mixer_get_pcm(pcm, 4);
    CHECK_INTEQUAL(pcm[0*2+0], 5000);
    CHECK_INTEQUAL(pcm[0*2+1], 5000);
    CHECK_INTEQUAL(pcm[1*2+0], 5000);
    CHECK_INTEQUAL(pcm[1*2+1], 5000);
    CHECK_INTEQUAL(pcm[2*2+0], -5000);
    CHECK_INTEQUAL(pcm[2*2+1], -5000);
    CHECK_INTEQUAL(pcm[3*2+0], -5000);
    CHECK_INTEQUAL(pcm[3*2+1], -5000);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_base_volume_range)
{
    SquareState state = {.period = 4, .num_cycles = 2, .samples_out = 0};
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_setvol(1, 0.125);  // Avoid overflow at base_volume=15.
    sound_mixer_start(1);

    int16_t pcm[4*2];
    sound_mixer_set_base_volume(-1);
    sound_mixer_get_pcm(pcm, 4);
    CHECK_INTEQUAL(pcm[0*2+0], 0);
    CHECK_INTEQUAL(pcm[0*2+1], 0);
    CHECK_INTEQUAL(pcm[1*2+0], 0);
    CHECK_INTEQUAL(pcm[1*2+1], 0);
    CHECK_INTEQUAL(pcm[2*2+0], 0);
    CHECK_INTEQUAL(pcm[2*2+1], 0);
    CHECK_INTEQUAL(pcm[3*2+0], 0);
    CHECK_INTEQUAL(pcm[3*2+1], 0);

    sound_mixer_set_base_volume(16);
    sound_mixer_get_pcm(pcm, 4);
    CHECK_INTEQUAL(pcm[0*2+0], 18750);
    CHECK_INTEQUAL(pcm[0*2+1], 18750);
    CHECK_INTEQUAL(pcm[1*2+0], 18750);
    CHECK_INTEQUAL(pcm[1*2+1], 18750);
    CHECK_INTEQUAL(pcm[2*2+0], -18750);
    CHECK_INTEQUAL(pcm[2*2+1], -18750);
    CHECK_INTEQUAL(pcm[3*2+0], -18750);
    CHECK_INTEQUAL(pcm[3*2+1], -18750);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_setdata_invalid)
{
    SquareState state1 = {.period = 2, .num_cycles = 1, .samples_out = 0};
    SquareState state2 = {.period = 4, .num_cycles = 1, .samples_out = 0};
    int16_t buf[6];

    sound_mixer_setdata(0, square_gen, &state1, 0);
    buf[0] = buf[1] = 1;
    sound_mixer_get_pcm(buf, 1);
    CHECK_INTEQUAL(buf[0], 0);
    CHECK_INTEQUAL(buf[1], 0);

    sound_mixer_setdata(3, square_gen, &state1, 0);
    buf[0] = buf[1] = 1;
    sound_mixer_get_pcm(buf, 1);
    CHECK_INTEQUAL(buf[0], 0);
    CHECK_INTEQUAL(buf[1], 0);

    sound_mixer_setdata(1, NULL, &state1, 0);
    buf[0] = buf[1] = 1;
    sound_mixer_get_pcm(buf, 1);
    CHECK_INTEQUAL(buf[0], 0);
    CHECK_INTEQUAL(buf[1], 0);

    sound_mixer_setdata(1, square_gen, &state1, 0);
    sound_mixer_setdata(1, square_gen, &state2, 0);  // Error (channel in use).
    sound_mixer_start(1);
    buf[0] = buf[1] = buf[2] = buf[3] = buf[4] = buf[5] = 1;
    sound_mixer_get_pcm(buf, 3);
    CHECK_INTEQUAL(buf[0*2+0], 10000);
    CHECK_INTEQUAL(buf[0*2+1], 10000);
    CHECK_INTEQUAL(buf[1*2+0], -10000);
    CHECK_INTEQUAL(buf[1*2+1], -10000);
    CHECK_INTEQUAL(buf[2*2+0], 0);
    CHECK_INTEQUAL(buf[2*2+1], 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_setvol)
{
    int16_t pcm[3*2];
    SquareState state = {.period = 2, .num_cycles = 1};

    /* Check normal volume (1.0). */
    state.samples_out = 0;
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_setvol(1, 1);
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 3);
    CHECK_INTEQUAL(pcm[0*2+0], 10000);
    CHECK_INTEQUAL(pcm[0*2+1], 10000);
    CHECK_INTEQUAL(pcm[1*2+0], -10000);
    CHECK_INTEQUAL(pcm[1*2+1], -10000);
    CHECK_INTEQUAL(pcm[2*2+0], 0);
    CHECK_INTEQUAL(pcm[2*2+1], 0);
    sound_mixer_reset(1);

    /* Check lower than normal volume. */
    state.samples_out = 0;
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_setvol(1, 0.5);
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 3);
    CHECK_INTEQUAL(pcm[0*2+0], 5000);
    CHECK_INTEQUAL(pcm[0*2+1], 5000);
    CHECK_INTEQUAL(pcm[1*2+0], -5000);
    CHECK_INTEQUAL(pcm[1*2+1], -5000);
    CHECK_INTEQUAL(pcm[2*2+0], 0);
    CHECK_INTEQUAL(pcm[2*2+1], 0);
    sound_mixer_reset(1);

    /* Check higher than normal volume. */
    state.samples_out = 0;
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_setvol(1, 2.5);
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 3);
    CHECK_INTEQUAL(pcm[0*2+0], 25000);
    CHECK_INTEQUAL(pcm[0*2+1], 25000);
    CHECK_INTEQUAL(pcm[1*2+0], -25000);
    CHECK_INTEQUAL(pcm[1*2+1], -25000);
    CHECK_INTEQUAL(pcm[2*2+0], 0);
    CHECK_INTEQUAL(pcm[2*2+1], 0);
    sound_mixer_reset(1);

    /* Check zero volume. */
    state.samples_out = 0;
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_setvol(1, 0);
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 3);
    CHECK_INTEQUAL(pcm[0*2+0], 0);
    CHECK_INTEQUAL(pcm[0*2+1], 0);
    CHECK_INTEQUAL(pcm[1*2+0], 0);
    CHECK_INTEQUAL(pcm[1*2+1], 0);
    CHECK_INTEQUAL(pcm[2*2+0], 0);
    CHECK_INTEQUAL(pcm[2*2+1], 0);
    sound_mixer_reset(1);

    /* Check out-of-range volume and output clipping. */
    state.samples_out = 0;
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_setvol(1, -1);  // Should be bounded to 0.
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 3);
    CHECK_INTEQUAL(pcm[0*2+0], 0);
    CHECK_INTEQUAL(pcm[0*2+1], 0);
    CHECK_INTEQUAL(pcm[1*2+0], 0);
    CHECK_INTEQUAL(pcm[1*2+1], 0);
    CHECK_INTEQUAL(pcm[2*2+0], 0);
    CHECK_INTEQUAL(pcm[2*2+1], 0);
    sound_mixer_reset(1);
    state.samples_out = 0;
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_setvol(1, 1e10); // Should be bounded to MAX_VOLUME (and clip).
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 3);
    CHECK_INTEQUAL(pcm[0*2+0], 32767);
    CHECK_INTEQUAL(pcm[0*2+1], 32767);
    CHECK_INTEQUAL(pcm[1*2+0], -32768);
    CHECK_INTEQUAL(pcm[1*2+1], -32768);
    CHECK_INTEQUAL(pcm[2*2+0], 0);
    CHECK_INTEQUAL(pcm[2*2+1], 0);
    sound_mixer_reset(1);

    /* Check handling of invalid parameters. */
    state.samples_out = 0;
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_setvol(0, 0.5);  // Invalid channel, should have no effect.
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 3);
    CHECK_INTEQUAL(pcm[0*2+0], 10000);
    CHECK_INTEQUAL(pcm[0*2+1], 10000);
    CHECK_INTEQUAL(pcm[1*2+0], -10000);
    CHECK_INTEQUAL(pcm[1*2+1], -10000);
    CHECK_INTEQUAL(pcm[2*2+0], 0);
    CHECK_INTEQUAL(pcm[2*2+1], 0);
    sound_mixer_reset(1);
    state.samples_out = 0;
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_setvol(3, 0.5); // Out-of-range channel, should have no effect.
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 3);
    CHECK_INTEQUAL(pcm[0*2+0], 10000);
    CHECK_INTEQUAL(pcm[0*2+1], 10000);
    CHECK_INTEQUAL(pcm[1*2+0], -10000);
    CHECK_INTEQUAL(pcm[1*2+1], -10000);
    CHECK_INTEQUAL(pcm[2*2+0], 0);
    CHECK_INTEQUAL(pcm[2*2+1], 0);
    sound_mixer_reset(1);
    state.samples_out = 0;
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_setvol(2, 0.5);  // Not-in-use channel, should have no effect.
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 3);
    CHECK_INTEQUAL(pcm[0*2+0], 10000);
    CHECK_INTEQUAL(pcm[0*2+1], 10000);
    CHECK_INTEQUAL(pcm[1*2+0], -10000);
    CHECK_INTEQUAL(pcm[1*2+1], -10000);
    CHECK_INTEQUAL(pcm[2*2+0], 0);
    CHECK_INTEQUAL(pcm[2*2+1], 0);
    sound_mixer_reset(1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_setpan)
{
    int16_t pcm[3*2];
    SquareState state = {.period = 2, .num_cycles = 1};

    /* Check regular (center) pan. */
    state.samples_out = 0;
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_setpan(1, 0);
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 3);
    CHECK_INTEQUAL(pcm[0*2+0], 10000);
    CHECK_INTEQUAL(pcm[0*2+1], 10000);
    CHECK_INTEQUAL(pcm[1*2+0], -10000);
    CHECK_INTEQUAL(pcm[1*2+1], -10000);
    CHECK_INTEQUAL(pcm[2*2+0], 0);
    CHECK_INTEQUAL(pcm[2*2+1], 0);
    sound_mixer_reset(1);
    state.samples_out = 0;
    sound_mixer_setdata(1, stereo_square_gen, &state, 1);
    sound_mixer_setpan(1, 0);
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 3);
    CHECK_INTEQUAL(pcm[0*2+0], 10000);
    CHECK_INTEQUAL(pcm[0*2+1], 10000);
    CHECK_INTEQUAL(pcm[1*2+0], -10000);
    CHECK_INTEQUAL(pcm[1*2+1], 10000);
    CHECK_INTEQUAL(pcm[2*2+0], 0);
    CHECK_INTEQUAL(pcm[2*2+1], 0);
    sound_mixer_reset(1);

    /* Check partial pan to the left. */
    state.samples_out = 0;
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_setpan(1, -0.5);
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 3);
    CHECK_INTEQUAL(pcm[0*2+0], 15000);
    CHECK_INTEQUAL(pcm[0*2+1], 5000);
    CHECK_INTEQUAL(pcm[1*2+0], -15000);
    CHECK_INTEQUAL(pcm[1*2+1], -5000);
    CHECK_INTEQUAL(pcm[2*2+0], 0);
    CHECK_INTEQUAL(pcm[2*2+1], 0);
    sound_mixer_reset(1);
    state.samples_out = 0;
    sound_mixer_setdata(1, stereo_square_gen, &state, 1);
    sound_mixer_setpan(1, -1.0f/3.0f);
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 3);
    CHECK_INTEQUAL(pcm[0*2+0], 10000);
    CHECK_INTEQUAL(pcm[0*2+1], 5000);
    CHECK_INTEQUAL(pcm[1*2+0], -10000);
    CHECK_INTEQUAL(pcm[1*2+1], 5000);
    CHECK_INTEQUAL(pcm[2*2+0], 0);
    CHECK_INTEQUAL(pcm[2*2+1], 0);
    sound_mixer_reset(1);


    /* Check partial pan to the right. */
    state.samples_out = 0;
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_setpan(1, 0.5);
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 3);
    CHECK_INTEQUAL(pcm[0*2+0], 5000);
    CHECK_INTEQUAL(pcm[0*2+1], 15000);
    CHECK_INTEQUAL(pcm[1*2+0], -5000);
    CHECK_INTEQUAL(pcm[1*2+1], -15000);
    CHECK_INTEQUAL(pcm[2*2+0], 0);
    CHECK_INTEQUAL(pcm[2*2+1], 0);
    sound_mixer_reset(1);
    state.samples_out = 0;
    sound_mixer_setdata(1, stereo_square_gen, &state, 1);
    sound_mixer_setpan(1, 1.0f/3.0f);
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 3);
    CHECK_INTEQUAL(pcm[0*2+0], 5000);
    CHECK_INTEQUAL(pcm[0*2+1], 10000);
    CHECK_INTEQUAL(pcm[1*2+0], -5000);
    CHECK_INTEQUAL(pcm[1*2+1], 10000);
    CHECK_INTEQUAL(pcm[2*2+0], 0);
    CHECK_INTEQUAL(pcm[2*2+1], 0);
    sound_mixer_reset(1);

    /* Check out-of-range pan to the left. */
    state.samples_out = 0;
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_setpan(1, -1e10);  // Should be bounded to -1.
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 3);
    CHECK_INTEQUAL(pcm[0*2+0], 20000);
    CHECK_INTEQUAL(pcm[0*2+1], 0);
    CHECK_INTEQUAL(pcm[1*2+0], -20000);
    CHECK_INTEQUAL(pcm[1*2+1], 0);
    CHECK_INTEQUAL(pcm[2*2+0], 0);
    CHECK_INTEQUAL(pcm[2*2+1], 0);
    sound_mixer_reset(1);
    state.samples_out = 0;
    sound_mixer_setdata(1, stereo_square_gen, &state, 1);
    sound_mixer_setpan(1, -1e10);
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 3);
    CHECK_INTEQUAL(pcm[0*2+0], 10000);
    CHECK_INTEQUAL(pcm[0*2+1], 0);
    CHECK_INTEQUAL(pcm[1*2+0], -10000);
    CHECK_INTEQUAL(pcm[1*2+1], 0);
    CHECK_INTEQUAL(pcm[2*2+0], 0);
    CHECK_INTEQUAL(pcm[2*2+1], 0);
    sound_mixer_reset(1);

    /* Check out-of-range pan to the right. */
    state.samples_out = 0;
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_setpan(1, 1e10);  // Should be bounded to +1.
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 3);
    CHECK_INTEQUAL(pcm[0*2+0], 0);
    CHECK_INTEQUAL(pcm[0*2+1], 20000);
    CHECK_INTEQUAL(pcm[1*2+0], 0);
    CHECK_INTEQUAL(pcm[1*2+1], -20000);
    CHECK_INTEQUAL(pcm[2*2+0], 0);
    CHECK_INTEQUAL(pcm[2*2+1], 0);
    sound_mixer_reset(1);
    state.samples_out = 0;
    sound_mixer_setdata(1, stereo_square_gen, &state, 1);
    sound_mixer_setpan(1, 1e10);
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 3);
    CHECK_INTEQUAL(pcm[0*2+0], 0);
    CHECK_INTEQUAL(pcm[0*2+1], 10000);
    CHECK_INTEQUAL(pcm[1*2+0], 0);
    CHECK_INTEQUAL(pcm[1*2+1], 10000);
    CHECK_INTEQUAL(pcm[2*2+0], 0);
    CHECK_INTEQUAL(pcm[2*2+1], 0);
    sound_mixer_reset(1);

    /* Check that there's no overflow with maximum volume and pan levels. */
    state.samples_out = 0;
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_setvol(1, 1e10);
    sound_mixer_setpan(1, -1e10);
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 3);
    CHECK_INTEQUAL(pcm[0*2+0], 32767);
    CHECK_INTEQUAL(pcm[0*2+1], 0);
    CHECK_INTEQUAL(pcm[1*2+0], -32768);
    CHECK_INTEQUAL(pcm[1*2+1], 0);
    CHECK_INTEQUAL(pcm[2*2+0], 0);
    CHECK_INTEQUAL(pcm[2*2+1], 0);
    sound_mixer_reset(1);
    state.samples_out = 0;
    sound_mixer_setdata(1, stereo_square_gen, &state, 1);
    sound_mixer_setvol(1, 1e10);
    sound_mixer_setpan(1, -1e10);
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 3);
    CHECK_INTEQUAL(pcm[0*2+0], 32767);
    CHECK_INTEQUAL(pcm[0*2+1], 0);
    CHECK_INTEQUAL(pcm[1*2+0], -32768);
    CHECK_INTEQUAL(pcm[1*2+1], 0);
    CHECK_INTEQUAL(pcm[2*2+0], 0);
    CHECK_INTEQUAL(pcm[2*2+1], 0);
    sound_mixer_reset(1);
    state.samples_out = 0;
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_setvol(1, 1e10);
    sound_mixer_setpan(1, 1e10);
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 3);
    CHECK_INTEQUAL(pcm[0*2+0], 0);
    CHECK_INTEQUAL(pcm[0*2+1], 32767);
    CHECK_INTEQUAL(pcm[1*2+0], 0);
    CHECK_INTEQUAL(pcm[1*2+1], -32768);
    CHECK_INTEQUAL(pcm[2*2+0], 0);
    CHECK_INTEQUAL(pcm[2*2+1], 0);
    sound_mixer_reset(1);
    state.samples_out = 0;
    sound_mixer_setdata(1, stereo_square_gen, &state, 1);
    sound_mixer_setvol(1, 1e10);
    sound_mixer_setpan(1, 1e10);
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 3);
    CHECK_INTEQUAL(pcm[0*2+0], 0);
    CHECK_INTEQUAL(pcm[0*2+1], 32767);
    CHECK_INTEQUAL(pcm[1*2+0], 0);
    CHECK_INTEQUAL(pcm[1*2+1], 32767);
    CHECK_INTEQUAL(pcm[2*2+0], 0);
    CHECK_INTEQUAL(pcm[2*2+1], 0);
    sound_mixer_reset(1);

    /* Check handling of invalid parameters. */
    state.samples_out = 0;
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_setpan(0, 0.5);  // Invalid channel.
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 3);
    CHECK_INTEQUAL(pcm[0*2+0], 10000);
    CHECK_INTEQUAL(pcm[0*2+1], 10000);
    CHECK_INTEQUAL(pcm[1*2+0], -10000);
    CHECK_INTEQUAL(pcm[1*2+1], -10000);
    CHECK_INTEQUAL(pcm[2*2+0], 0);
    CHECK_INTEQUAL(pcm[2*2+1], 0);
    sound_mixer_reset(1);
    state.samples_out = 0;
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_setpan(3, 0.5);  // Out-of-range channel.
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 3);
    CHECK_INTEQUAL(pcm[0*2+0], 10000);
    CHECK_INTEQUAL(pcm[0*2+1], 10000);
    CHECK_INTEQUAL(pcm[1*2+0], -10000);
    CHECK_INTEQUAL(pcm[1*2+1], -10000);
    CHECK_INTEQUAL(pcm[2*2+0], 0);
    CHECK_INTEQUAL(pcm[2*2+1], 0);
    sound_mixer_reset(1);
    state.samples_out = 0;
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_setpan(2, 0.5);  // Not-in-use channel.
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 3);
    CHECK_INTEQUAL(pcm[0*2+0], 10000);
    CHECK_INTEQUAL(pcm[0*2+1], 10000);
    CHECK_INTEQUAL(pcm[1*2+0], -10000);
    CHECK_INTEQUAL(pcm[1*2+1], -10000);
    CHECK_INTEQUAL(pcm[2*2+0], 0);
    CHECK_INTEQUAL(pcm[2*2+1], 0);
    sound_mixer_reset(1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_setfade)
{
    int16_t pcm[4*2];
    SquareState state = {.period = 2, .num_cycles = 4};

    /* Check fading to silence with cut==0. */
    state.samples_out = 0;
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_setfade(1, 0, 0.25, 0);
    sound_mixer_start(1);
    /* Note that we have to get these one sample at a time because fading
     * is only performed at the beginning of each output buffer. */
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], 7500);
    CHECK_INTEQUAL(pcm[0*2+1], 7500);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], -5000);
    CHECK_INTEQUAL(pcm[0*2+1], -5000);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], 2500);
    CHECK_INTEQUAL(pcm[0*2+1], 2500);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], 0);
    CHECK_INTEQUAL(pcm[0*2+1], 0);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], 0);
    CHECK_INTEQUAL(pcm[0*2+1], 0);
    /* Check that a non-cut fade is still playing. */
    sound_mixer_setvol(1, 1);
    sound_mixer_get_pcm(pcm, 4);
    CHECK_INTEQUAL(pcm[0*2+0], -10000);
    CHECK_INTEQUAL(pcm[0*2+1], -10000);
    CHECK_INTEQUAL(pcm[1*2+0], 10000);
    CHECK_INTEQUAL(pcm[1*2+1], 10000);
    CHECK_INTEQUAL(pcm[2*2+0], -10000);
    CHECK_INTEQUAL(pcm[2*2+1], -10000);
    CHECK_INTEQUAL(pcm[3*2+0], 0);
    CHECK_INTEQUAL(pcm[3*2+1], 0);
    sound_mixer_reset(1);

    /* Check fading to silence with cut==1. */
    state.samples_out = 0;
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_setfade(1, 0, 0.25, 1);
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], 7500);
    CHECK_INTEQUAL(pcm[0*2+1], 7500);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], -5000);
    CHECK_INTEQUAL(pcm[0*2+1], -5000);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], 2500);
    CHECK_INTEQUAL(pcm[0*2+1], 2500);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], 0);
    CHECK_INTEQUAL(pcm[0*2+1], 0);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], 0);
    CHECK_INTEQUAL(pcm[0*2+1], 0);
    /* Check that a cut fade is no longer playing. */
    sound_mixer_setvol(1, 1);
    sound_mixer_get_pcm(pcm, 4);
    CHECK_INTEQUAL(pcm[0*2+0], 0);
    CHECK_INTEQUAL(pcm[0*2+1], 0);
    CHECK_INTEQUAL(pcm[1*2+0], 0);
    CHECK_INTEQUAL(pcm[1*2+1], 0);
    CHECK_INTEQUAL(pcm[2*2+0], 0);
    CHECK_INTEQUAL(pcm[2*2+1], 0);
    CHECK_INTEQUAL(pcm[3*2+0], 0);
    CHECK_INTEQUAL(pcm[3*2+1], 0);
    sound_mixer_reset(1);

    /* Check fading to a lower volume (but not silence). */
    state.samples_out = 0;
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_setfade(1, 0.5, 0.25, 0);
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], 8750);
    CHECK_INTEQUAL(pcm[0*2+1], 8750);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], -7500);
    CHECK_INTEQUAL(pcm[0*2+1], -7500);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], 6250);
    CHECK_INTEQUAL(pcm[0*2+1], 6250);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], -5000);
    CHECK_INTEQUAL(pcm[0*2+1], -5000);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], 5000);
    CHECK_INTEQUAL(pcm[0*2+1], 5000);
    sound_mixer_reset(1);

    /* Check fading to a higher volume. */
    state.samples_out = 0;
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_setfade(1, 1.5, 0.25, 0);
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], 11250);
    CHECK_INTEQUAL(pcm[0*2+1], 11250);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], -12500);
    CHECK_INTEQUAL(pcm[0*2+1], -12500);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], 13750);
    CHECK_INTEQUAL(pcm[0*2+1], 13750);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], -15000);
    CHECK_INTEQUAL(pcm[0*2+1], -15000);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], 15000);
    CHECK_INTEQUAL(pcm[0*2+1], 15000);
    sound_mixer_reset(1);

    /* Check fading with an extremely short time (1/10 of a sample). */
    state.samples_out = 0;
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_setfade(1, 0.5, 0.1/MIX_RATE, 0);
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], 5000);
    CHECK_INTEQUAL(pcm[0*2+1], 5000);
    sound_mixer_reset(1);

    /* Check fading to out-of-bounds volume values. */
    state.samples_out = 0;
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_setfade(1, -1, 0.25, 0);
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], 7500);
    CHECK_INTEQUAL(pcm[0*2+1], 7500);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], -5000);
    CHECK_INTEQUAL(pcm[0*2+1], -5000);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], 2500);
    CHECK_INTEQUAL(pcm[0*2+1], 2500);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], 0);
    CHECK_INTEQUAL(pcm[0*2+1], 0);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], 0);
    CHECK_INTEQUAL(pcm[0*2+1], 0);
    sound_mixer_reset(1);
    state.samples_out = 0;
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_setfade(1, 1e10, 0.25, 0);
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 4);
    CHECK_INTEQUAL(pcm[3*2+0], -32768);
    CHECK_INTEQUAL(pcm[3*2+1], -32768);
    sound_mixer_reset(1);

    /* Check that a new fade properly overwrites an old one. */
    state.samples_out = 0;
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_setfade(1, 0, 0.125, 1);
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], 5000);
    CHECK_INTEQUAL(pcm[0*2+1], 5000);
    sound_mixer_setfade(1, 0, 0.25, 1);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], -3750);
    CHECK_INTEQUAL(pcm[0*2+1], -3750);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], 2500);
    CHECK_INTEQUAL(pcm[0*2+1], 2500);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], -1250);
    CHECK_INTEQUAL(pcm[0*2+1], -1250);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], 0);
    CHECK_INTEQUAL(pcm[0*2+1], 0);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], 0);
    CHECK_INTEQUAL(pcm[0*2+1], 0);
    sound_mixer_setvol(1, 1);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], 0);
    CHECK_INTEQUAL(pcm[0*2+1], 0);
    sound_mixer_reset(1);

    /* Check that a length of zero properly cancels a running fade. */
    state.samples_out = 0;
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_setfade(1, 0, 0.25, 1);
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], 7500);
    CHECK_INTEQUAL(pcm[0*2+1], 7500);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], -5000);
    CHECK_INTEQUAL(pcm[0*2+1], -5000);
    sound_mixer_setfade(1, 0, 0, 1);
    sound_mixer_get_pcm(pcm, 4);
    CHECK_INTEQUAL(pcm[0*2+0], 5000);
    CHECK_INTEQUAL(pcm[0*2+1], 5000);
    CHECK_INTEQUAL(pcm[1*2+0], -5000);
    CHECK_INTEQUAL(pcm[1*2+1], -5000);
    CHECK_INTEQUAL(pcm[2*2+0], 5000);
    CHECK_INTEQUAL(pcm[2*2+1], 5000);
    CHECK_INTEQUAL(pcm[3*2+0], -5000);
    CHECK_INTEQUAL(pcm[3*2+1], -5000);
    sound_mixer_reset(1);

    /* Check that a setvol() call properly cancels any running fade. */
    state.samples_out = 0;
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_setfade(1, 0, 0.25, 1);
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], 7500);
    CHECK_INTEQUAL(pcm[0*2+1], 7500);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], -5000);
    CHECK_INTEQUAL(pcm[0*2+1], -5000);
    sound_mixer_setvol(1, 1);
    sound_mixer_get_pcm(pcm, 4);
    CHECK_INTEQUAL(pcm[0*2+0], 10000);
    CHECK_INTEQUAL(pcm[0*2+1], 10000);
    CHECK_INTEQUAL(pcm[1*2+0], -10000);
    CHECK_INTEQUAL(pcm[1*2+1], -10000);
    CHECK_INTEQUAL(pcm[2*2+0], 10000);
    CHECK_INTEQUAL(pcm[2*2+1], 10000);
    CHECK_INTEQUAL(pcm[3*2+0], -10000);
    CHECK_INTEQUAL(pcm[3*2+1], -10000);
    sound_mixer_reset(1);

    /* Check handling of invalid parameters. */
    state.samples_out = 0;
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_setfade(0, 0, 0.25, 0);  // Invalid channel.
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], 10000);
    CHECK_INTEQUAL(pcm[0*2+1], 10000);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], -10000);
    CHECK_INTEQUAL(pcm[0*2+1], -10000);
    sound_mixer_reset(1);
    state.samples_out = 0;
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_setfade(3, 0, 0.25, 0);  // Out-of-range channel.
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], 10000);
    CHECK_INTEQUAL(pcm[0*2+1], 10000);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], -10000);
    CHECK_INTEQUAL(pcm[0*2+1], -10000);
    sound_mixer_reset(1);
    state.samples_out = 0;
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_setfade(2, 0, 0.25, 0);  // Not-in-use channel.
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], 10000);
    CHECK_INTEQUAL(pcm[0*2+1], 10000);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], -10000);
    CHECK_INTEQUAL(pcm[0*2+1], -10000);
    sound_mixer_reset(1);
    state.samples_out = 0;
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_setfade(1, 0, -1, 0);  // Negative length.
    sound_mixer_start(1);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], 10000);
    CHECK_INTEQUAL(pcm[0*2+1], 10000);
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], -10000);
    CHECK_INTEQUAL(pcm[0*2+1], -10000);
    sound_mixer_reset(1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_start_stop)
{
    int16_t pcm[5*2];
    SquareState state = {.period = 2, .num_cycles = 2};

    /* Check that a channel can be stopped and restarted. */
    state.samples_out = 0;
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_setvol(1, 1);
    sound_mixer_start(1);
    CHECK_TRUE(sound_mixer_status(1));
    sound_mixer_get_pcm(pcm, 2);
    CHECK_INTEQUAL(pcm[0*2+0], 10000);
    CHECK_INTEQUAL(pcm[0*2+1], 10000);
    CHECK_INTEQUAL(pcm[1*2+0], -10000);
    CHECK_INTEQUAL(pcm[1*2+1], -10000);
    CHECK_TRUE(sound_mixer_status(1));
    sound_mixer_stop(1);
    CHECK_FALSE(sound_mixer_status(1));
    sound_mixer_get_pcm(pcm, 2);
    CHECK_INTEQUAL(pcm[0*2+0], 0);
    CHECK_INTEQUAL(pcm[0*2+1], 0);
    CHECK_INTEQUAL(pcm[1*2+0], 0);
    CHECK_INTEQUAL(pcm[1*2+1], 0);
    CHECK_FALSE(sound_mixer_status(1));
    sound_mixer_start(1);
    CHECK_TRUE(sound_mixer_status(1));
    sound_mixer_get_pcm(pcm, 3);
    CHECK_INTEQUAL(pcm[0*2+0], 10000);
    CHECK_INTEQUAL(pcm[0*2+1], 10000);
    CHECK_INTEQUAL(pcm[1*2+0], -10000);
    CHECK_INTEQUAL(pcm[1*2+1], -10000);
    CHECK_INTEQUAL(pcm[2*2+0], 0);
    CHECK_INTEQUAL(pcm[2*2+1], 0);
    /* End of stream is not detected until the next mix() call. */
    CHECK_TRUE(sound_mixer_status(1));
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0*2+0], 0);
    CHECK_INTEQUAL(pcm[0*2+1], 0);
    CHECK_FALSE(sound_mixer_status(1));
    sound_mixer_reset(1);

    /* Check that a reset channel can't be restarted. */
    state.samples_out = 0;
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_setvol(1, 1);
    sound_mixer_start(1);
    CHECK_TRUE(sound_mixer_status(1));
    sound_mixer_get_pcm(pcm, 2);
    CHECK_INTEQUAL(pcm[0*2+0], 10000);
    CHECK_INTEQUAL(pcm[0*2+1], 10000);
    CHECK_INTEQUAL(pcm[1*2+0], -10000);
    CHECK_INTEQUAL(pcm[1*2+1], -10000);
    CHECK_TRUE(sound_mixer_status(1));
    sound_mixer_reset(1);
    CHECK_FALSE(sound_mixer_status(1));
    sound_mixer_get_pcm(pcm, 2);
    CHECK_INTEQUAL(pcm[0*2+0], 0);
    CHECK_INTEQUAL(pcm[0*2+1], 0);
    CHECK_INTEQUAL(pcm[1*2+0], 0);
    CHECK_INTEQUAL(pcm[1*2+1], 0);
    CHECK_FALSE(sound_mixer_status(1));
    sound_mixer_start(1);
    CHECK_FALSE(sound_mixer_status(1));
    sound_mixer_get_pcm(pcm, 3);
    CHECK_INTEQUAL(pcm[0*2+0], 0);
    CHECK_INTEQUAL(pcm[0*2+1], 0);
    CHECK_INTEQUAL(pcm[1*2+0], 0);
    CHECK_INTEQUAL(pcm[1*2+1], 0);
    CHECK_INTEQUAL(pcm[2*2+0], 0);
    CHECK_INTEQUAL(pcm[2*2+1], 0);
    CHECK_FALSE(sound_mixer_status(1));
    sound_mixer_reset(1);

    /* Check handling of invalid parameters. */
    state.samples_out = 0;
    sound_mixer_setdata(1, square_gen, &state, 0);
    sound_mixer_setvol(1, 1);
    sound_mixer_start(0);
    sound_mixer_start(2);
    sound_mixer_start(3);
    CHECK_FALSE(sound_mixer_status(0));
    CHECK_FALSE(sound_mixer_status(2));
    CHECK_FALSE(sound_mixer_status(3));
    sound_mixer_get_pcm(pcm, 2);
    CHECK_INTEQUAL(pcm[0*2+0], 0);
    CHECK_INTEQUAL(pcm[0*2+1], 0);
    CHECK_INTEQUAL(pcm[1*2+0], 0);
    CHECK_INTEQUAL(pcm[1*2+1], 0);
    sound_mixer_start(1);
    sound_mixer_stop(0);
    sound_mixer_stop(2);
    sound_mixer_stop(3);
    sound_mixer_get_pcm(pcm, 2);
    CHECK_INTEQUAL(pcm[0*2+0], 10000);
    CHECK_INTEQUAL(pcm[0*2+1], 10000);
    CHECK_INTEQUAL(pcm[1*2+0], -10000);
    CHECK_INTEQUAL(pcm[1*2+1], -10000);
    sound_mixer_reset(0);
    sound_mixer_reset(2);
    sound_mixer_reset(3);
    sound_mixer_get_pcm(pcm, 2);
    CHECK_INTEQUAL(pcm[0*2+0], 10000);
    CHECK_INTEQUAL(pcm[0*2+1], 10000);
    CHECK_INTEQUAL(pcm[1*2+0], -10000);
    CHECK_INTEQUAL(pcm[1*2+1], -10000);
    sound_mixer_reset(1);

    return 1;
}

/*************************************************************************/
/*************************************************************************/
