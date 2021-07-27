/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/linux/sound.c: Tests for Linux sound output.  The ALSA
 * loopback device (kernel module snd-aloop) must be installed for these
 * tests to work.
 */

#include "src/base.h"
#include "src/math.h"
#include "src/sound.h"
#include "src/sound/decode.h"
#include "src/sysdep.h"
#include "src/sysdep/test.h"
#include "src/thread.h"
#include "src/test/base.h"
#include "src/test/sound/wavegen.h"

#include <alloca.h>
#include <alsa/asoundlib.h>

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Default ALSA period size, equal to 1/4 of DEFAULT_PCM_BUFSIZE from
 * src/sysdep/linux/sound.c. */
#define PERIOD_SIZE  256

/* Loopback device names. */
#define LOOPBACK_PLAYBACK_DEVICE  "hw:Loopback,0"
#define LOOPBACK_CAPTURE_DEVICE   "hw:Loopback,1"

/* ALSA PCM device for capturing looped-back audio samples. */
static snd_pcm_t *pcm_handle = NULL;
/* Buffer size for PCM capture device. */
static int pcm_buffer_size;
/* Sampling rate of captured PCM data. */
static int pcm_rate;

/*************************************************************************/
/************************** Sound decode module **************************/
/*************************************************************************/

static int decode_get_pcm(SoundDecodeHandle *this, int16_t *pcm_buffer,
                          int pcm_len, int *loop_offset_ret)
{
    *loop_offset_ret = 0;
    SquareState *state = (SquareState *)this->custom_data;
    return square_gen(state, pcm_buffer, pcm_len) ? pcm_len : 0;
}

/*-----------------------------------------------------------------------*/

static void decode_close(UNUSED SoundDecodeHandle *this)
{
}

/*-----------------------------------------------------------------------*/

static int decode_open(SoundDecodeHandle *this)
{
    this->get_pcm     = decode_get_pcm;
    this->close       = decode_close;
    this->stereo      = 0;
    this->native_freq = pcm_rate;
    this->loop_start  = 0;
    this->loop_length = 0;
    return 1;
}

/*************************************************************************/
/************************* Other helper routines *************************/
/*************************************************************************/

/**
 * start_capture:  Start capturing PCM data on the ALSA loopback device.
 *
 * [Return value]
 *     True on success, false on failure.
 */
static int start_capture(void)
{
    /* Convenience macro for checking ALSA function return codes. */
    #define CHECK_ALSA(call)  do {                       \
        int result;                                      \
        if ((result = (call)) < 0) {                     \
            FAIL("%s: %s", #call, snd_strerror(result)); \
        }                                                \
    } while (0)

    /* Open the playback side of the loopback device.  This is done first
     * so that the loopback device takes its parameters from those set by
     * the playback code. */
    CHECK_TRUE(sound_open_device(LOOPBACK_PLAYBACK_DEVICE, 1));

    /* Make sure failures close the capture device. */
    #undef FAIL_ACTION
    #define FAIL_ACTION  goto error

    /* Open the capture side of the loopback device. */
    CHECK_ALSA(snd_pcm_open(&pcm_handle, LOOPBACK_CAPTURE_DEVICE,
                            SND_PCM_STREAM_CAPTURE, 0));
    snd_pcm_hw_params_t *hwparams;
    snd_pcm_hw_params_alloca(&hwparams);
    CHECK_ALSA(snd_pcm_hw_params_any(pcm_handle, hwparams));
    CHECK_ALSA(snd_pcm_hw_params_set_access(
                   pcm_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED));
    CHECK_ALSA(snd_pcm_hw_params_set_format(
                   pcm_handle, hwparams, SND_PCM_FORMAT_S16_LE));
    CHECK_ALSA(snd_pcm_hw_params_set_channels(pcm_handle, hwparams, 2));
    CHECK_ALSA(snd_pcm_hw_params(pcm_handle, hwparams));
    int dummy;
    CHECK_ALSA(snd_pcm_hw_params_get_rate(
                   hwparams, (unsigned int *)&pcm_rate, &dummy));
    snd_pcm_uframes_t buffer_size;
    CHECK_ALSA(snd_pcm_hw_params_get_buffer_size(hwparams, &buffer_size));
    pcm_buffer_size = (int)buffer_size;

    return 1;

  error:
    if (pcm_handle) {
        snd_pcm_close(pcm_handle);
        pcm_handle = NULL;
    }
    sys_sound_cleanup();
    return 0;

    #undef CHECK_ALSA
    #undef FAIL_ACTION
    #define FAIL_ACTION  return 0
}

/*-----------------------------------------------------------------------*/

/**
 * read_pcm:  Read the given number of 16-bit stereo samples from the
 * loopback PCM capture device.
 *
 * [Parameters]
 *     buffer: Buffer into which to read PCM samples.
 *     count: Number of samples to read.
 * [Return value]
 *     True on success, false on failure (including short read).
 */
static int read_pcm(int16_t *buffer, int count)
{
    uint8_t *ptr = (uint8_t *)buffer;
    int left = count;
    while (left > 0) {
        int to_read = ubound(left, pcm_buffer_size);
        ssize_t result;
        while ((result = snd_pcm_readi(pcm_handle, ptr, to_read)) < 0) {
            DLOG("snd_pcm_readi(): %s", snd_strerror(result));
            if (result == -EPIPE) {
                result = snd_pcm_prepare(pcm_handle);
                if (result != 0) {
                    DLOG("snd_pcm_readi(): overrun recovery failed: %s",
                         snd_strerror(result));
                    return 0;
                }
            } else {
                result = snd_pcm_recover(pcm_handle, result, 0);
                if (result != 0) {
                    DLOG("snd_pcm_readi(): recover: %s", snd_strerror(result));
                    return 0;
                }
            }
        }
        ptr += result * 4;
        left -= result;
    }
    ASSERT(ptr == (uint8_t *)buffer + count*4);
    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

static int do_test_linux_sound(void);
int test_linux_sound(void)
{
    /* Make sure the loopback device is available before running any tests. */
    if (!sys_sound_init(LOOPBACK_PLAYBACK_DEVICE)) {
        WARN("ALSA loopback device does not seem to be available; skipping"
             " Linux sound tests.  Ensure the snd-aloop kernel module is"
             " loaded before running these tests.  This can typically be"
             " done by running the command: sudo modprobe snd-aloop");
        return 1;
    }
    sys_sound_cleanup();
    return do_test_linux_sound();
}

DEFINE_GENERIC_TEST_RUNNER(do_test_linux_sound)

TEST_INIT(init)
{
    sys_test_sound_use_live_routines = 1;
    CHECK_TRUE(thread_init());
    sound_init();
    return 1;
}

TEST_CLEANUP(cleanup)
{
    if (pcm_handle) {
        snd_pcm_close(pcm_handle);
        pcm_handle = NULL;
    }
    sound_cleanup();
    thread_cleanup();
    sys_test_sound_use_live_routines = 0;
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_init)
{
    CHECK_TRUE(sys_sound_init(LOOPBACK_PLAYBACK_DEVICE));
    sys_sound_cleanup();
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_init_default)
{
    if (!sys_sound_init("")) {
        FAIL("sys_sound_init(\"\") was not true as expected (this test may"
             " fail if another process has the system's audio device locked)");
    }
    sys_sound_cleanup();
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_init_failure)
{
    CHECK_FALSE(sys_sound_init("hw:-1,0"));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_init_memory_failure)
{
    CHECK_MEMORY_FAILURES(sys_sound_init(LOOPBACK_PLAYBACK_DEVICE));
    sys_sound_cleanup();
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_multiple_init)
{
    CHECK_TRUE(sys_sound_init(LOOPBACK_PLAYBACK_DEVICE));
    CHECK_FALSE(sys_sound_init(LOOPBACK_PLAYBACK_DEVICE));
    sys_sound_cleanup();
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_capture)
{
    CHECK_TRUE(start_capture());

    /* We should be able to read silence from the capture device if nothing
     * is playing. */
    int16_t buffer[1024*2];
    /* In theory, this memset should be unnecessary, but without it Valgrind
     * (at least through version 3.9.0) reports uninitialized value errors
     * on every array access.  Maybe Valgrind doesn't handle whatever system
     * call is involved in snd_pcm_readi()?  In any case, this helps check
     * that snd_pcm_readi() does in fact give us valid data. */
    memset(buffer, -1, sizeof(buffer));
    CHECK_TRUE(read_pcm(buffer, lenof(buffer)/2));
    for (int i = 0; i < lenof(buffer); i++) {
        CHECK_INTEQUAL(buffer[i], 0);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_capture_after_init_failure)
{
    CHECK_FALSE(sys_sound_init("hw:-1,0"));
    CHECK_TRUE(start_capture());

    int16_t buffer[1024*2];
    memset(buffer, -1, sizeof(buffer));
    CHECK_TRUE(read_pcm(buffer, lenof(buffer)/2));
    for (int i = 0; i < lenof(buffer); i++) {
        CHECK_INTEQUAL(buffer[i], 0);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_playback_rate)
{
    CHECK_TRUE(start_capture());
    CHECK_INTEQUAL(sys_sound_playback_rate(), pcm_rate);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_latency)
{
    CHECK_TRUE(start_capture());
    CHECK_INTEQUAL(sys_sound_set_latency(0), 1024.0f/pcm_rate);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_headphone_disconnect)
{
    /* This isn't supported on linux, so the check should always return
     * false. */
    CHECK_FALSE(sys_sound_check_headphone_disconnect());
    sys_sound_acknowledge_headphone_disconnect();  // Should do nothing.
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_playback)
{
    int16_t buffer[1024*2];
    memset(buffer, -1, sizeof(buffer));

    CHECK_TRUE(start_capture());

    /* Read (and discard) a buffer's worth of samples to get the playback
     * loop running. */
    read_pcm(buffer, lenof(buffer)/2);

    SquareState state = {.period = pcm_rate/300, .num_cycles = 30,
                         .samples_out = 0};
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_custom(decode_open, &state, 1));
    int channel;
    CHECK_TRUE(channel = sound_play_decoder(decoder, 0, 0.5, -1));

    /* Watch for the beginning and end of the square wave, but don't wait
     * longer than 1 second or 0.2 seconds past the starting point (the
     * square wave is only 0.1 seconds long). */
    int current_sample = 0, square_start = -1, square_end = -1;
    while (square_end < 0
           && current_sample < max(square_start + pcm_rate/5, pcm_rate)) {
        CHECK_TRUE(read_pcm(buffer, lenof(buffer)/2));
        for (int i = 0; i < lenof(buffer); i += 2, current_sample++) {
            if (buffer[i] != 0) {
                if (square_start < 0) {
                    square_start = current_sample;
                }
                const int offset = current_sample - square_start;
                if (offset % state.period < state.period / 2) {
                    CHECK_INTEQUAL(buffer[i], 10000);
                } else {
                    CHECK_INTEQUAL(buffer[i], -10000);
                }
            } else {
                if (square_start > 0 && square_end < 0) {
                    square_end = current_sample;
                }
            }
            CHECK_INTEQUAL(buffer[i+1], 0);
        }
    }
    if (square_start < 0) {
        FAIL("Did not see test sample in audio stream");
    } else if (square_end < 0) {
        FAIL("Did not see end of test sample in audio stream");
    } else {
        DLOG("Test sample start: %5d (%.3fs)", square_start,
            (float)square_start / (float)pcm_rate);
        DLOG("Test sample end:   %5d (%.3fs)", square_end,
            (float)square_end / (float)pcm_rate);
        if (square_end - square_start != state.period * state.num_cycles) {
            FAIL("Test sample had wrong length (actual = %d, expected = %d)",
                 square_end - square_start, state.period * state.num_cycles);
        } else if (square_start > pcm_rate/10) {
            FAIL("Test sample was delayed too long (check buffer size setup)");
        }
    }

    /* The sound should also be reported as stopped by the interface. */
    sound_update();
    CHECK_FALSE(sound_is_playing(channel));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fade)
{
    int16_t buffer[1024*2];
    memset(buffer, -1, sizeof(buffer));

    CHECK_TRUE(start_capture());
    read_pcm(buffer, lenof(buffer)/2);

    SquareState state = {.period = pcm_rate/300, .num_cycles = 30,
                         .samples_out = 0};
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_custom(decode_open, &state, 1));
    int channel;
    CHECK_TRUE(channel = sound_play_decoder(decoder, 0, 1, 1));
    sound_fade(channel, 0.05);

    int current_sample = 0, square_start = -1, square_end = -1;
    while (square_end < 0
           && current_sample < max(square_start + pcm_rate/5, pcm_rate)) {
        CHECK_TRUE(read_pcm(buffer, lenof(buffer)/2));
        for (int i = 0; i < lenof(buffer); i += 2, current_sample++) {
            CHECK_INTEQUAL(buffer[i], 0);
            if (buffer[i+1] != 0) {
                if (square_start < 0) {
                    square_start = current_sample;
                }
                const int offset = current_sample - square_start;
                const int end_of_buffer_offset =
                    align_up(offset+1, PERIOD_SIZE);
                const float amplitude = lbound(
                    1 - ((float)end_of_buffer_offset / (float)(pcm_rate/20)),
                    0);
                const int expected_sample = iroundf(20000 * amplitude);
                if (offset % state.period < state.period / 2) {
                    CHECK_INTRANGE(buffer[i+1], expected_sample - 1,
                                                expected_sample + 1);
                } else {
                    CHECK_INTRANGE(buffer[i+1], -expected_sample - 1,
                                                -expected_sample + 1);
                }
            } else {
                if (square_start > 0 && square_end < 0) {
                    square_end = current_sample;
                }
            }
        }
    }
    if (square_start < 0) {
        FAIL("Did not see test sample in audio stream");
    } else if (square_end < 0) {
        FAIL("Did not see end of test sample in audio stream");
    } else {
        DLOG("Test sample start: %5d (%.3fs)", square_start,
            (float)square_start / (float)pcm_rate);
        DLOG("Test sample end:   %5d (%.3fs)", square_end,
            (float)square_end / (float)pcm_rate);
        const int expected_length = align_down(pcm_rate/20 - 1, PERIOD_SIZE);
        if (square_end - square_start != expected_length) {
            FAIL("Test sample had wrong length (actual = %d, expected = %d)",
                 square_end - square_start, expected_length);
        }
    }

    sound_update();
    CHECK_FALSE(sound_is_playing(channel));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_cut)
{
    int16_t buffer[1024*2];
    memset(buffer, -1, sizeof(buffer));

    CHECK_TRUE(start_capture());
    read_pcm(buffer, lenof(buffer)/2);

    SquareState state = {.period = pcm_rate/300, .num_cycles = 30,
                         .samples_out = 0};
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_custom(decode_open, &state, 1));
    int channel;
    CHECK_TRUE(channel = sound_play_decoder(decoder, 0, 0.5, -1));

    const float period_length = (float)PERIOD_SIZE / (float)pcm_rate;
    nanosleep(&(struct timespec){0, iroundf(period_length*1.0e9f)}, NULL);
    sound_cut(channel);

    int current_sample = 0, square_start = -1, square_end = -1;
    while (square_end < 0
           && current_sample < max(square_start + pcm_rate/5, pcm_rate)) {
        CHECK_TRUE(read_pcm(buffer, lenof(buffer)/2));
        for (int i = 0; i < lenof(buffer); i += 2, current_sample++) {
            if (buffer[i] != 0) {
                if (square_start < 0) {
                    square_start = current_sample;
                }
                const int offset = current_sample - square_start;
                if (offset % state.period < state.period / 2) {
                    CHECK_INTEQUAL(buffer[i], 10000);
                } else {
                    CHECK_INTEQUAL(buffer[i], -10000);
                }
            } else {
                if (square_start > 0 && square_end < 0) {
                    square_end = current_sample;
                }
            }
            CHECK_INTEQUAL(buffer[i+1], 0);
        }
    }
    if (square_start < 0) {
        FAIL("Did not see test sample in audio stream");
    } else if (square_end < 0) {
        FAIL("Did not see end of test sample in audio stream");
    } else {
        DLOG("Test sample start: %5d (%.3fs)", square_start,
            (float)square_start / (float)pcm_rate);
        DLOG("Test sample end:   %5d (%.3fs)", square_end,
            (float)square_end / (float)pcm_rate);
        if (square_end - square_start != PERIOD_SIZE
         && square_end - square_start != PERIOD_SIZE*2) {
            FAIL("Test sample had wrong length (actual = %d, expected = %d"
                 " or %d)", square_end - square_start, PERIOD_SIZE,
                 PERIOD_SIZE*2);
        }
    }

    sound_update();
    CHECK_FALSE(sound_is_playing(channel));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_pause)
{
    int16_t buffer[1024*2];
    memset(buffer, -1, sizeof(buffer));

    CHECK_TRUE(start_capture());
    read_pcm(buffer, lenof(buffer)/2);

    SquareState state = {.period = pcm_rate/300, .num_cycles = 30,
                         .samples_out = 0};
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_custom(decode_open, &state, 1));
    int channel;
    CHECK_TRUE(channel = sound_play_decoder(decoder, 0, 0.5, -1));

    const float period_length = (float)PERIOD_SIZE / (float)pcm_rate;
    nanosleep(&(struct timespec){0, iroundf(period_length*1.0e9f)}, NULL);
    sound_pause(channel);

    int current_sample = 0, square_start = -1, square_end = -1;
    while (square_end < 0
           && current_sample < max(square_start + pcm_rate/5, pcm_rate)) {
        CHECK_TRUE(read_pcm(buffer, lenof(buffer)/2));
        for (int i = 0; i < lenof(buffer); i += 2, current_sample++) {
            if (buffer[i] != 0) {
                if (square_start < 0) {
                    square_start = current_sample;
                }
                const int offset = current_sample - square_start;
                if (offset % state.period < state.period / 2) {
                    CHECK_INTEQUAL(buffer[i], 10000);
                } else {
                    CHECK_INTEQUAL(buffer[i], -10000);
                }
            } else {
                if (square_start > 0 && square_end < 0) {
                    square_end = current_sample;
                }
            }
            CHECK_INTEQUAL(buffer[i+1], 0);
        }
    }
    if (square_start < 0) {
        FAIL("Did not see test sample in audio stream");
    } else if (square_end < 0) {
        FAIL("Did not see end of test sample in audio stream");
    } else {
        DLOG("Test sample start: %5d (%.3fs)", square_start,
            (float)square_start / (float)pcm_rate);
        DLOG("Test sample pause: %5d (%.3fs)", square_end,
            (float)square_end / (float)pcm_rate);
        if (square_end - square_start != PERIOD_SIZE
         && square_end - square_start != PERIOD_SIZE*2) {
            FAIL("Test sample had wrong length before pause (actual = %d,"
                 " expected = %d or %d)", square_end - square_start,
                 PERIOD_SIZE, PERIOD_SIZE*2);
        }
    }

    sound_resume(channel);
    const int length_before_pause = square_end - square_start;
    square_start = square_end = -1;
    while (square_end < 0
           && current_sample < max(square_start + pcm_rate/5, pcm_rate)) {
        CHECK_TRUE(read_pcm(buffer, lenof(buffer)/2));
        for (int i = 0; i < lenof(buffer); i += 2, current_sample++) {
            if (buffer[i] != 0) {
                if (square_start < 0) {
                    square_start = current_sample;
                }
                const int offset =
                    (current_sample - square_start) + length_before_pause;
                if (offset % state.period < state.period / 2) {
                    CHECK_INTEQUAL(buffer[i], 10000);
                } else {
                    CHECK_INTEQUAL(buffer[i], -10000);
                }
            } else {
                if (square_start > 0 && square_end < 0) {
                    square_end = current_sample;
                }
            }
            CHECK_INTEQUAL(buffer[i+1], 0);
        }
    }
    if (square_start < 0) {
        FAIL("Did not see test sample in audio stream");
    } else if (square_end < 0) {
        FAIL("Did not see end of test sample in audio stream");
    } else {
        DLOG("Test sample resume:%5d (%.3fs)", square_start,
            (float)square_start / (float)pcm_rate);
        DLOG("Test sample end:   %5d (%.3fs)", square_end,
            (float)square_end / (float)pcm_rate);
        const int total_length =
            (square_end - square_start) + length_before_pause;
        if (total_length != state.period * state.num_cycles) {
            FAIL("Test sample had wrong length (actual = %d (%d+%d),"
                 " expected = %d)", total_length, length_before_pause,
                 square_end - square_start, state.period * state.num_cycles);
        }
    }

    sound_update();
    CHECK_FALSE(sound_is_playing(channel));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_latency)
{
    int16_t buffer[1024*2];
    memset(buffer, -1, sizeof(buffer));

    CHECK_TRUE(start_capture());

    /* Check that latency changes don't cause a crash if done while the
     * playback thread is running. */
    read_pcm(buffer, lenof(buffer)/2);
    sys_sound_set_latency(sys_sound_set_latency(0) * 2);

    SquareState state = {.period = pcm_rate/300, .num_cycles = 30,
                         .samples_out = 0};
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_custom(decode_open, &state, 1));
    int channel;
    CHECK_TRUE(channel = sound_play_decoder(decoder, 0, 0.5, -1));

    int current_sample = 0, square_start = -1, square_end = -1;
    while (square_end < 0
           && current_sample < max(square_start + pcm_rate/5, pcm_rate)) {
        CHECK_TRUE(read_pcm(buffer, lenof(buffer)/2));
        for (int i = 0; i < lenof(buffer); i += 2, current_sample++) {
            if (buffer[i] != 0) {
                if (square_start < 0) {
                    square_start = current_sample;
                }
                const int offset = current_sample - square_start;
                if (offset % state.period < state.period / 2) {
                    CHECK_INTEQUAL(buffer[i], 10000);
                } else {
                    CHECK_INTEQUAL(buffer[i], -10000);
                }
            } else {
                if (square_start > 0 && square_end < 0) {
                    square_end = current_sample;
                }
            }
            CHECK_INTEQUAL(buffer[i+1], 0);
        }
    }
    if (square_start < 0) {
        FAIL("Did not see test sample in audio stream");
    } else if (square_end < 0) {
        FAIL("Did not see end of test sample in audio stream");
    } else {
        DLOG("Test sample start: %5d (%.3fs)", square_start,
            (float)square_start / (float)pcm_rate);
        DLOG("Test sample end:   %5d (%.3fs)", square_end,
            (float)square_end / (float)pcm_rate);
        if (square_end - square_start != state.period * state.num_cycles) {
            FAIL("Test sample had wrong length (actual = %d, expected = %d)",
                 square_end - square_start, state.period * state.num_cycles);
        } else if (square_start > pcm_rate/10) {
            FAIL("Test sample was delayed too long (check buffer size setup)");
        }
    }

    sound_update();
    CHECK_FALSE(sound_is_playing(channel));

    return 1;
}

/*************************************************************************/
/*************************************************************************/
