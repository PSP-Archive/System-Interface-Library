/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sound/decode.c: Tests for the audio decoding framework.
 */

#include "src/base.h"
#include "src/memory.h"
#include "src/sound.h"
#include "src/sound/decode.h"
#include "src/sysdep.h"
#include "src/test/base.h"
#include "src/test/sound/wavegen.h"
#include "src/thread.h"

#if !defined(SIL_PLATFORM_PSP)
# define USING_IOQUEUE
# include "src/sysdep/misc/ioqueue.h"
#endif

/*************************************************************************/
/************************** Dummy decode module **************************/
/*************************************************************************/

/* Flag indicating whether decode_close() was called. */
static uint8_t decode_close_called;

/*-----------------------------------------------------------------------*/

static int decode_get_pcm(SoundDecodeHandle *this, int16_t *pcm_buffer,
                          int pcm_len, int *loop_offset_ret)
{
    *loop_offset_ret = 0;
    if (this->internal->data_type == SOUND_DECODE_CUSTOM) {
        SquareState *state = (SquareState *)this->custom_data;
        if (this->stereo) {
            return stereo_square_gen(state, pcm_buffer, pcm_len) ? pcm_len : 0;
        } else {
            return square_gen(state, pcm_buffer, pcm_len) ? pcm_len : 0;
        }
    } else {
        int samples_read = 0;
        uintptr_t offset = (uintptr_t)this->private;
        while (samples_read < pcm_len) {
            const uint8_t *data;
            const uint32_t data_size =
                decode_get_data(this, offset, pcm_len, &data);
            if (data_size == 0) {
                break;
            }
            for (uint32_t i = 0; i < data_size; i++) {
                if (this->stereo) {
                    pcm_buffer[samples_read*2+0] = data[i];
                    pcm_buffer[samples_read*2+1] = -data[i];
                } else {
                    pcm_buffer[samples_read] = data[i];
                }
                samples_read++;
                offset++;
                if (this->internal->loop) {
                    const uintptr_t loop_end =
                        this->loop_start + this->loop_length;
                    if (offset == loop_end) {
                        offset = this->loop_start;
                        *loop_offset_ret += this->loop_length;
                        break;
                    }
                }
            }
        }
        this->private = (void *)offset;
        return samples_read;
    }
}

/*-----------------------------------------------------------------------*/

static void decode_close(UNUSED SoundDecodeHandle *this)
{
    decode_close_called = 1;
}

/*-----------------------------------------------------------------------*/

/* Control flags for decode_open().  Each flag is reset to zero after
 * being used. */
static uint8_t decode_open_force_failure;
static uint8_t decode_open_return_stereo;
static uint8_t decode_open_return_freq_0;

static int decode_open(SoundDecodeHandle *this)
{
    if (this->internal->data_type == SOUND_DECODE_CUSTOM) {
        if (!this->custom_data) {
            return 0;
        }
    } else {
        if (decode_open_force_failure) {
            decode_open_force_failure = 0;
            return 0;
        }
    }

    this->get_pcm     = decode_get_pcm;
    this->close       = decode_close;
    this->stereo      = (decode_open_return_stereo != 0);
    this->native_freq = (decode_open_return_freq_0 ? 0 : 4);
    this->loop_start  = 0;
    this->loop_length = 0;
    this->private     = (void *)0;  // Sample counter for memory/file decodes.

    decode_open_return_stereo = 0;
    decode_open_return_freq_0 = 0;

    return 1;
}

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

/**
 * wrap_sys_file_open:  Call sys_file_open(), converting the given path to
 * an absolute path by prepending the resource path prefix.
 */
static SysFile *wrap_sys_file_open(const char *path)
{
    char abs_path[10000];
    ASSERT(sys_get_resource_path_prefix(abs_path, sizeof(abs_path))
           < (int)sizeof(abs_path));
    ASSERT(strformat_check(abs_path+strlen(abs_path),
                           sizeof(abs_path)-strlen(abs_path), "%s", path));
    return sys_file_open(abs_path);
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_sound_decode)

/*-----------------------------------------------------------------------*/

TEST_INIT(init)
{
    /* Override the default WAV handler so we have a format code we can
     * use for our tests. */
    sound_decode_set_handler(SOUND_FORMAT_WAV, decode_open);
    CHECK_TRUE(sound_decode_has_handler(SOUND_FORMAT_WAV));

    CHECK_TRUE(thread_init());
    CHECK_TRUE(sys_file_init());
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
    /* Restore the original WAV decoder before returning. */
    sound_decode_set_handler(SOUND_FORMAT_WAV, decode_wav_open);

    sys_file_cleanup();
    thread_cleanup();
    return 1;
}

/*************************************************************************/
/************************* Basic decoding tests **************************/
/*************************************************************************/

TEST(test_handler)
{
    sound_decode_set_handler(SOUND_FORMAT_AUTODETECT, decode_open); // Invalid.
    CHECK_FALSE(sound_decode_has_handler(SOUND_FORMAT_AUTODETECT));
    sound_decode_set_handler(SOUND_FORMAT_WAV, NULL);
    CHECK_FALSE(sound_decode_has_handler(SOUND_FORMAT_WAV));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_decode_memory)
{
    static const uint8_t decode_data[] = {100, 100, 40, 40};
    SoundDecodeHandle *decoder;
    int16_t pcm[3];

    CHECK_TRUE(decoder = sound_decode_open(SOUND_FORMAT_WAV, decode_data,
                                           lenof(decode_data), 0, 1));
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4);

    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 3));
    CHECK_FLOATEQUAL(sound_decode_get_position(decoder), 0.75);
    CHECK_INTEQUAL(pcm[0], 100);
    CHECK_INTEQUAL(pcm[1], 100);
    CHECK_INTEQUAL(pcm[2], 40);

    decode_close_called = 0;
    sound_decode_close(decoder);
    CHECK_TRUE(decode_close_called);

    CHECK_TRUE(decoder = sound_decode_open(SOUND_FORMAT_WAV, decode_data,
                                           lenof(decode_data), 0, 1));
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 3));
    CHECK_FLOATEQUAL(sound_decode_get_position(decoder), 0.75);
    CHECK_INTEQUAL(pcm[0], 100);
    CHECK_INTEQUAL(pcm[1], 100);
    CHECK_INTEQUAL(pcm[2], 40);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 2));
    CHECK_FLOATEQUAL(sound_decode_get_position(decoder), 1.0);
    CHECK_INTEQUAL(pcm[0], 40);
    CHECK_INTEQUAL(pcm[1], 0);
    CHECK_FALSE(sound_decode_get_pcm(decoder, pcm, 1));
    CHECK_FLOATEQUAL(sound_decode_get_position(decoder), 1.0);
    sound_decode_close(decoder);

    CHECK_TRUE(decoder = sound_decode_open(SOUND_FORMAT_WAV, decode_data,
                                           lenof(decode_data), 0, 1));
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 3));
    CHECK_FLOATEQUAL(sound_decode_get_position(decoder), 0.75);
    CHECK_INTEQUAL(pcm[0], 100);
    CHECK_INTEQUAL(pcm[1], 100);
    CHECK_INTEQUAL(pcm[2], 40);
    sound_decode_close(decoder);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_decode_memory_memory_failure)
{
    static const uint8_t decode_data[] = {100, 100, 40, 40};

    SoundDecodeHandle *decoder;
    CHECK_MEMORY_FAILURES(
        decoder = sound_decode_open(SOUND_FORMAT_WAV, decode_data,
                                    lenof(decode_data), 0, 1));
    int16_t pcm[3];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 3));
    CHECK_FLOATEQUAL(sound_decode_get_position(decoder), 0.75);
    CHECK_INTEQUAL(pcm[0], 100);
    CHECK_INTEQUAL(pcm[1], 100);
    CHECK_INTEQUAL(pcm[2], 40);
    sound_decode_close(decoder);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_decode_file)
{
    SysFile *file;
    SoundDecodeHandle *decoder;
    int16_t pcm[3];

    ASSERT(file = wrap_sys_file_open("testdata/sound/square.dat"));
    CHECK_TRUE(decoder = sound_decode_open_from_file(SOUND_FORMAT_WAV, file,
                                                     1, 4, 0, 1));
    sys_file_close(file);
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4);

    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 3));
    CHECK_FLOATEQUAL(sound_decode_get_position(decoder), 0.75);
    CHECK_INTEQUAL(pcm[0], 100);
    CHECK_INTEQUAL(pcm[1], 100);
    CHECK_INTEQUAL(pcm[2], 40);

    decode_close_called = 0;
    sound_decode_close(decoder);
    CHECK_TRUE(decode_close_called);

    ASSERT(file = wrap_sys_file_open("testdata/sound/square.dat"));
    CHECK_TRUE(decoder = sound_decode_open_from_file(SOUND_FORMAT_WAV, file,
                                                     1, 4, 0, 1));
    sys_file_close(file);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 3));
    CHECK_FLOATEQUAL(sound_decode_get_position(decoder), 0.75);
    CHECK_INTEQUAL(pcm[0], 100);
    CHECK_INTEQUAL(pcm[1], 100);
    CHECK_INTEQUAL(pcm[2], 40);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 2));
    CHECK_FLOATEQUAL(sound_decode_get_position(decoder), 1.0);
    CHECK_INTEQUAL(pcm[0], 40);
    CHECK_INTEQUAL(pcm[1], 0);
    CHECK_FALSE(sound_decode_get_pcm(decoder, pcm, 1));
    CHECK_FLOATEQUAL(sound_decode_get_position(decoder), 1.0);
    sound_decode_close(decoder);

    ASSERT(file = wrap_sys_file_open("testdata/sound/square.dat"));
    CHECK_TRUE(decoder = sound_decode_open_from_file(SOUND_FORMAT_WAV, file,
                                                     1, 4, 0, 1));
    sys_file_close(file);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 3));
    CHECK_FLOATEQUAL(sound_decode_get_position(decoder), 0.75);
    CHECK_INTEQUAL(pcm[0], 100);
    CHECK_INTEQUAL(pcm[1], 100);
    CHECK_INTEQUAL(pcm[2], 40);
    sound_decode_close(decoder);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_decode_file_memory_failure)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square.dat"));

    SoundDecodeHandle *decoder;
    CHECK_MEMORY_FAILURES(
        decoder = sound_decode_open_from_file(SOUND_FORMAT_WAV, file,
                                              1, 4, 0, 1));
    sys_file_close(file);
    int16_t pcm[3];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 3));
    CHECK_FLOATEQUAL(sound_decode_get_position(decoder), 0.75);
    CHECK_INTEQUAL(pcm[0], 100);
    CHECK_INTEQUAL(pcm[1], 100);
    CHECK_INTEQUAL(pcm[2], 40);
    sound_decode_close(decoder);

    return 1;
}

/*-----------------------------------------------------------------------*/

#ifdef USING_IOQUEUE
TEST(test_decode_file_read_permfail)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square.dat"));

    /* Permanent failure should cause the open to fail. */
    TEST_misc_ioqueue_permfail_next_read(1);
    CHECK_FALSE(sound_decode_open_from_file(SOUND_FORMAT_WAV, file,
                                            1, 4, 0, 1));

    sys_file_close(file);
    return 1;
}
#endif

/*-----------------------------------------------------------------------*/

#ifdef USING_IOQUEUE
TEST(test_decode_file_read_tempfail)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square.dat"));
    SoundDecodeHandle *decoder;
    int16_t pcm[3];

    /* Transient failure should still allow the open to succeed. */
    TEST_misc_ioqueue_tempfail_next_read(1);
    CHECK_TRUE(decoder = sound_decode_open_from_file(SOUND_FORMAT_WAV, file,
                                                     1, 4, 0, 1));
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 3));
    CHECK_FLOATEQUAL(sound_decode_get_position(decoder), 0.75);
    CHECK_INTEQUAL(pcm[0], 100);
    CHECK_INTEQUAL(pcm[1], 100);
    CHECK_INTEQUAL(pcm[2], 40);
    sound_decode_close(decoder);

    sys_file_close(file);
    return 1;
}
#endif

/*-----------------------------------------------------------------------*/

#ifdef USING_IOQUEUE
TEST(test_decode_file_read_tempfail_invalid_format)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square.dat"));

    TEST_misc_ioqueue_tempfail_next_read(1);
    CHECK_FALSE(sound_decode_open_from_file(SOUND_FORMAT_AUTODETECT, file,
                                            1, 4, 0, 1));

    sys_file_close(file);
    return 1;
}
#endif

/*-----------------------------------------------------------------------*/

#ifdef USING_IOQUEUE
TEST(test_decode_file_read_error)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square.dat"));
    SoundDecodeHandle *decoder;
    int16_t pcm[3];

    /* I/O failure on the read should trigger an immediate read when
     * getting data. */
    TEST_misc_ioqueue_iofail_next_read(1);
    CHECK_TRUE(decoder = sound_decode_open_from_file(SOUND_FORMAT_WAV, file,
                                                     1, 4, 0, 1));
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 3));
    CHECK_FLOATEQUAL(sound_decode_get_position(decoder), 0.75);
    CHECK_INTEQUAL(pcm[0], 100);
    CHECK_INTEQUAL(pcm[1], 100);
    CHECK_INTEQUAL(pcm[2], 40);
    sound_decode_close(decoder);

    sys_file_close(file);
    return 1;
}
#endif

/*-----------------------------------------------------------------------*/

TEST(test_decode_custom)
{
    SquareState state = {.period = 4, .num_cycles = 1};
    SoundDecodeHandle *decoder;

    state.samples_out = 0;
    CHECK_TRUE(decoder = sound_decode_open_custom(decode_open, &state, 1));
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4);

    int16_t pcm[3];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 3));
    CHECK_FLOATEQUAL(sound_decode_get_position(decoder), 0.75);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], -10000);

    decode_close_called = 0;
    sound_decode_close(decoder);
    CHECK_TRUE(decode_close_called);

    state.samples_out = 0;
    CHECK_TRUE(decoder = sound_decode_open_custom(decode_open, &state, 1));
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 3));
    CHECK_FLOATEQUAL(sound_decode_get_position(decoder), 0.75);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], -10000);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 2));
    CHECK_FLOATEQUAL(sound_decode_get_position(decoder), 1.25);
    CHECK_INTEQUAL(pcm[0], -10000);
    CHECK_INTEQUAL(pcm[1], 0);
    CHECK_FALSE(sound_decode_get_pcm(decoder, pcm, 1));
    CHECK_FLOATEQUAL(sound_decode_get_position(decoder), 1.25);
    sound_decode_close(decoder);

    state.samples_out = 0;
    CHECK_TRUE(decoder = sound_decode_open_custom(decode_open, &state, 1));
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 3));
    CHECK_FLOATEQUAL(sound_decode_get_position(decoder), 0.75);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], -10000);
    sound_decode_close(decoder);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_decode_custom_memory_failure)
{
    SquareState state = {.period = 4, .num_cycles = 1, .samples_out = 0};
    SoundDecodeHandle *decoder;
    CHECK_MEMORY_FAILURES(
        decoder = sound_decode_open_custom(decode_open, &state, 1));
    int16_t pcm[3];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 3));
    CHECK_FLOATEQUAL(sound_decode_get_position(decoder), 0.75);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], -10000);
    sound_decode_close(decoder);

    return 1;
}

/*************************************************************************/
/*************************** Resampling tests ****************************/
/*************************************************************************/

TEST(test_resample)
{
    static const uint8_t decode_data[] = {100, 100, 40, 40};
    SoundDecodeHandle *decoder;
    int16_t pcm[8000];

    /* Test downsampling. */
    CHECK_TRUE(decoder = sound_decode_open(SOUND_FORMAT_WAV, decode_data,
                                           lenof(decode_data), 0, 1));
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4);
    sound_decode_set_output_freq(decoder, 2);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 3));
    CHECK_INTEQUAL(pcm[0], 100);
    CHECK_INTEQUAL(pcm[1], 40);
    CHECK_INTEQUAL(pcm[2], 0);
    sound_decode_close(decoder);

    /* Test set_output_freq() to the native frequency. */
    CHECK_TRUE(decoder = sound_decode_open(SOUND_FORMAT_WAV, decode_data,
                                           lenof(decode_data), 0, 1));
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4);
    sound_decode_set_output_freq(decoder, 4);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 5));
    CHECK_INTEQUAL(pcm[0], 100);
    CHECK_INTEQUAL(pcm[1], 100);
    CHECK_INTEQUAL(pcm[2], 40);
    CHECK_INTEQUAL(pcm[3], 40);
    CHECK_INTEQUAL(pcm[4], 0);
    sound_decode_close(decoder);

    /* Test upsampling. */
    CHECK_TRUE(decoder = sound_decode_open(SOUND_FORMAT_WAV, decode_data,
                                           lenof(decode_data), 0, 1));
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4);
    sound_decode_set_output_freq(decoder, 5);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 6));
    CHECK_INTEQUAL(pcm[0], 100);
    CHECK_INTEQUAL(pcm[1], 100);
    CHECK_INTEQUAL(pcm[2], 64);
    CHECK_INTEQUAL(pcm[3], 40);
    CHECK_INTEQUAL(pcm[4], 32);
    CHECK_INTEQUAL(pcm[5], 0);
    sound_decode_close(decoder);

    /* Test resampling in stereo. */
    decode_open_return_stereo = 1;
    CHECK_TRUE(decoder = sound_decode_open(SOUND_FORMAT_WAV, decode_data,
                                           lenof(decode_data), 1, 1));
    CHECK_TRUE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4);
    sound_decode_set_output_freq(decoder, 5);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 6));
    CHECK_INTEQUAL(pcm[0*2+0], 100);
    CHECK_INTEQUAL(pcm[0*2+1], -100);
    CHECK_INTEQUAL(pcm[1*2+0], 100);
    CHECK_INTEQUAL(pcm[1*2+1], -100);
    CHECK_INTEQUAL(pcm[2*2+0], 64);
    CHECK_INTEQUAL(pcm[2*2+1], -64);
    CHECK_INTEQUAL(pcm[3*2+0], 40);
    CHECK_INTEQUAL(pcm[3*2+1], -40);
    CHECK_INTEQUAL(pcm[4*2+0], 32);
    CHECK_INTEQUAL(pcm[4*2+1], -32);
    CHECK_INTEQUAL(pcm[5*2+0], 0);
    CHECK_INTEQUAL(pcm[5*2+1], 0);
    sound_decode_close(decoder);

    /* Test handling of resample buffer reloads for large files. */
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/long.dat"));
    CHECK_TRUE(decoder = sound_decode_open_from_file(SOUND_FORMAT_WAV, file,
                                                     1, 39996, 0, 1));
    sys_file_close(file);
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4);
    sound_decode_set_output_freq(decoder, 1);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 8000));
    CHECK_INTEQUAL(pcm[0], 0);
    CHECK_INTEQUAL(pcm[7999], 15998 % 256);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 8000));
    CHECK_INTEQUAL(pcm[0], 16000 % 256);
    CHECK_INTEQUAL(pcm[1998], 19996 % 256);
    CHECK_INTEQUAL(pcm[1999], 0);
    CHECK_INTEQUAL(pcm[7999], 0);
    sound_decode_close(decoder);

    /* Test handling of a stream with no PCM data. */
    SquareState state = {.period = 4, .num_cycles = 0, .samples_out = 0};
    CHECK_TRUE(decoder = sound_decode_open_custom(decode_open, &state, 1));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4);
    sound_decode_set_output_freq(decoder, 2);
    CHECK_FALSE(sound_decode_get_pcm(decoder, pcm, 3));
    sound_decode_close(decoder);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_resample_change_rate)
{
    static const uint8_t decode_data[] = {100, 100, 40, 40};
    SoundDecodeHandle *decoder;
    int16_t pcm[5];

    /* Test changing from non-native to native rate. */
    CHECK_TRUE(decoder = sound_decode_open(SOUND_FORMAT_WAV, decode_data,
                                           lenof(decode_data), 0, 1));
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4);
    sound_decode_set_output_freq(decoder, 2);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 1));
    CHECK_INTEQUAL(pcm[0], 100);
    sound_decode_set_output_freq(decoder, 4);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 3));
    CHECK_INTEQUAL(pcm[0], 40);
    CHECK_INTEQUAL(pcm[1], 40);
    CHECK_INTEQUAL(pcm[2], 0);
    sound_decode_close(decoder);

    /* Test changing from native to non-native rate. */
    CHECK_TRUE(decoder = sound_decode_open(SOUND_FORMAT_WAV, decode_data,
                                           lenof(decode_data), 0, 1));
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4);
    sound_decode_set_output_freq(decoder, 4);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 2));
    CHECK_INTEQUAL(pcm[0], 100);
    CHECK_INTEQUAL(pcm[1], 100);
    sound_decode_set_output_freq(decoder, 8);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 5));
    CHECK_INTEQUAL(pcm[0], 40);
    CHECK_INTEQUAL(pcm[1], 40);
    CHECK_INTEQUAL(pcm[2], 40);
    CHECK_INTEQUAL(pcm[3], 20);
    CHECK_INTEQUAL(pcm[4], 0);
    sound_decode_close(decoder);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_resample_no_interpolate)
{
    static const uint8_t decode_data[] = {100, 100, 40, 40};
    SoundDecodeHandle *decoder;
    int16_t pcm[12];

    /* Test upsampling. */
    CHECK_TRUE(decoder = sound_decode_open(SOUND_FORMAT_WAV, decode_data,
                                           lenof(decode_data), 0, 0));
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4);
    sound_decode_set_output_freq(decoder, 5);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 6));
    CHECK_INTEQUAL(pcm[0], 100);
    CHECK_INTEQUAL(pcm[1], 100);
    CHECK_INTEQUAL(pcm[2], 100);
    CHECK_INTEQUAL(pcm[3], 40);
    CHECK_INTEQUAL(pcm[4], 40);
    CHECK_INTEQUAL(pcm[5], 0);
    sound_decode_close(decoder);

    /* Test resampling in stereo. */
    decode_open_return_stereo = 1;
    CHECK_TRUE(decoder = sound_decode_open(SOUND_FORMAT_WAV, decode_data,
                                           lenof(decode_data), 1, 0));
    CHECK_TRUE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4);
    sound_decode_set_output_freq(decoder, 5);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 6));
    CHECK_INTEQUAL(pcm[0*2+0], 100);
    CHECK_INTEQUAL(pcm[0*2+1], -100);
    CHECK_INTEQUAL(pcm[1*2+0], 100);
    CHECK_INTEQUAL(pcm[1*2+1], -100);
    CHECK_INTEQUAL(pcm[2*2+0], 100);
    CHECK_INTEQUAL(pcm[2*2+1], -100);
    CHECK_INTEQUAL(pcm[3*2+0], 40);
    CHECK_INTEQUAL(pcm[3*2+1], -40);
    CHECK_INTEQUAL(pcm[4*2+0], 40);
    CHECK_INTEQUAL(pcm[4*2+1], -40);
    CHECK_INTEQUAL(pcm[5*2+0], 0);
    CHECK_INTEQUAL(pcm[5*2+1], 0);
    sound_decode_close(decoder);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_resample_memory_failure)
{
    static const uint8_t decode_data[] = {100, 100, 40, 40};
    SoundDecodeHandle *decoder;
    int16_t pcm[3];

    /* This is a bit complicated for handling via CHECK_MEMORY_FAILURES()
     * because memory allocation doesn't occur until we actually start
     * decoding, so we check manually. */
    int i;
    for (i = 0; i < 100; i++) {
        CHECK_TRUE(decoder = sound_decode_open(SOUND_FORMAT_WAV, decode_data,
                                               lenof(decode_data), 0, 1));
        CHECK_FALSE(sound_decode_is_stereo(decoder));
        CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4);
        TEST_mem_fail_after(i, 1, 0);
        sound_decode_set_output_freq(decoder, 2);
        const int result = sound_decode_get_pcm(decoder, pcm, 3);
        TEST_mem_fail_after(-1, 0, 0);
        if (result) {
            if (i == 0) {
                FAIL("sound_decode_set_output_freq(decoder, 2) did not fail"
                     " on a memory allocation failure");
            }
            break;
        }
        sound_decode_close(decoder);
    }
    if (i == 100) {
        FAIL("sound_decode_set_output_freq(decoder, 2) did not succeed"
             " after 100 iterations");
    }
    CHECK_INTEQUAL(pcm[0], 100);
    CHECK_INTEQUAL(pcm[1], 40);
    CHECK_INTEQUAL(pcm[2], 0);
    sound_decode_close(decoder);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_decode_freq)
{
    static const uint8_t decode_data[] = {100, 100, 40, 40};
    SoundDecodeHandle *decoder;
    int16_t pcm[9];

    CHECK_TRUE(decoder = sound_decode_open(SOUND_FORMAT_WAV, decode_data,
                                           lenof(decode_data), 0, 1));
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4);
    sound_decode_set_decode_freq(decoder, 2);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 9));
    CHECK_INTEQUAL(pcm[0], 100);
    CHECK_INTEQUAL(pcm[1], 100);
    CHECK_INTEQUAL(pcm[2], 100);
    CHECK_INTEQUAL(pcm[3], 70);
    CHECK_INTEQUAL(pcm[4], 40);
    CHECK_INTEQUAL(pcm[5], 40);
    CHECK_INTEQUAL(pcm[6], 40);
    CHECK_INTEQUAL(pcm[7], 20);
    CHECK_INTEQUAL(pcm[8], 0);
    sound_decode_close(decoder);

    CHECK_TRUE(decoder = sound_decode_open(SOUND_FORMAT_WAV, decode_data,
                                           lenof(decode_data), 0, 1));
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4);
    sound_decode_set_decode_freq(decoder, 8);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 3));
    CHECK_INTEQUAL(pcm[0], 100);
    CHECK_INTEQUAL(pcm[1], 40);
    CHECK_INTEQUAL(pcm[2], 0);
    sound_decode_close(decoder);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_decode_freq_change)
{
    static const uint8_t decode_data[] = {100, 100, 40, 40};
    SoundDecodeHandle *decoder;
    int16_t pcm[5];

    CHECK_TRUE(decoder = sound_decode_open(SOUND_FORMAT_WAV, decode_data,
                                           lenof(decode_data), 0, 1));
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4);
    sound_decode_set_decode_freq(decoder, 2);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 5));
    CHECK_INTEQUAL(pcm[0], 100);
    CHECK_INTEQUAL(pcm[1], 100);
    CHECK_INTEQUAL(pcm[2], 100);
    CHECK_INTEQUAL(pcm[3], 70);
    CHECK_INTEQUAL(pcm[4], 40);
    /* The decoder is now pointing between two samples.  If we change back
     * to the original frequency, the fractional part of the position
     * should be retained. */
    sound_decode_set_decode_freq(decoder, 4);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 3));
    CHECK_INTEQUAL(pcm[0], 40);
    CHECK_INTEQUAL(pcm[1], 20);
    CHECK_INTEQUAL(pcm[2], 0);
    sound_decode_close(decoder);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_decode_freq_zero)
{
    static const uint8_t decode_data[] = {100, 100, 40, 40};
    SoundDecodeHandle *decoder;
    int16_t pcm[6];

    CHECK_TRUE(decoder = sound_decode_open(SOUND_FORMAT_WAV, decode_data,
                                           lenof(decode_data), 0, 1));
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4);
    sound_decode_set_decode_freq(decoder, 2);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 3));
    CHECK_INTEQUAL(pcm[0], 100);
    CHECK_INTEQUAL(pcm[1], 100);
    CHECK_INTEQUAL(pcm[2], 100);
    /* Setting the decode frequency to zero should hold the current sample.
     * In this case, the current sample is the not-yet-output intermediate
     * sample between 100 and 40. */
    sound_decode_set_decode_freq(decoder, 0);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 2));
    CHECK_INTEQUAL(pcm[0], 70);
    CHECK_INTEQUAL(pcm[1], 70);
    /* Setting the decode frequency back to nonzero should resume decoding
     * from the point at which it was stopped. */
    sound_decode_set_decode_freq(decoder, 2);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 6));
    CHECK_INTEQUAL(pcm[0], 70);
    CHECK_INTEQUAL(pcm[1], 40);
    CHECK_INTEQUAL(pcm[2], 40);
    CHECK_INTEQUAL(pcm[3], 40);
    CHECK_INTEQUAL(pcm[4], 20);
    CHECK_INTEQUAL(pcm[5], 0);
    sound_decode_close(decoder);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_decode_invalid)
{
    static const uint8_t decode_data[] = {100, 100, 40, 40};
    SoundDecodeHandle *decoder;
    int16_t pcm[5];

    CHECK_TRUE(decoder = sound_decode_open(SOUND_FORMAT_WAV, decode_data,
                                           lenof(decode_data), 0, 1));
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    sound_decode_set_decode_freq(decoder, -1);  // No effect.
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 5));
    CHECK_INTEQUAL(pcm[0], 100);
    CHECK_INTEQUAL(pcm[1], 100);
    CHECK_INTEQUAL(pcm[2], 40);
    CHECK_INTEQUAL(pcm[3], 40);
    CHECK_INTEQUAL(pcm[4], 0);
    sound_decode_close(decoder);

    return 1;
}

/*************************************************************************/
/**************************** File read tests ****************************/
/*************************************************************************/

TEST(test_file_read)
{
#if READ_BUFFER_SIZE < 16000 || READ_BUFFER_SIZE >= 20000
# error Need to rewrite test for current READ_BUFFER_SIZE setting.
#endif

    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/long.dat"));

    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(SOUND_FORMAT_WAV, file,
                                                     0, 40000, 0, 1));
    sys_file_close(file);

    const uint8_t *data;

    /* Should be bounded to READ_BUFFER_SIZE. */
    CHECK_INTEQUAL(decode_get_data(decoder, 0, 20000, &data),
                   READ_BUFFER_SIZE);
    CHECK_INTEQUAL(data[0]<<8 | data[1], 0);
    CHECK_INTEQUAL(data[READ_BUFFER_SIZE-2]<<8 | data[READ_BUFFER_SIZE-1],
                   READ_BUFFER_SIZE/2 - 1);

    /* Completely separate part of the file. */
    CHECK_INTEQUAL(decode_get_data(decoder, 20000, READ_BUFFER_SIZE, &data),
                   READ_BUFFER_SIZE);
    CHECK_INTEQUAL(data[0]<<8 | data[1], 10000);
    CHECK_INTEQUAL(data[READ_BUFFER_SIZE-2]<<8 | data[READ_BUFFER_SIZE-1],
                   10000 + (READ_BUFFER_SIZE/2 - 1));

    /* Partially overlapping with the beginning of the buffered data. */
    CHECK_INTEQUAL(decode_get_data(decoder, 6000, READ_BUFFER_SIZE, &data),
                   READ_BUFFER_SIZE);
    CHECK_INTEQUAL(data[0]<<8 | data[1], 3000);
    CHECK_INTEQUAL(data[READ_BUFFER_SIZE-2]<<8 | data[READ_BUFFER_SIZE-1],
                   3000 + (READ_BUFFER_SIZE/2 - 1));

    /* Partially overlapping with the end of the buffered data. */
    CHECK_INTEQUAL(decode_get_data(decoder, 22000, READ_BUFFER_SIZE, &data),
                   READ_BUFFER_SIZE);
    CHECK_INTEQUAL(data[0]<<8 | data[1], 11000);
    CHECK_INTEQUAL(data[READ_BUFFER_SIZE-2]<<8 | data[READ_BUFFER_SIZE-1],
                   11000 + (READ_BUFFER_SIZE/2 - 1));

    /* Entirely within the end of the buffered data. */
    CHECK_INTEQUAL(decode_get_data(decoder, 36000, 2000, &data), 2000);
    CHECK_INTEQUAL(data[0]<<8 | data[1], 18000);
    CHECK_INTEQUAL(data[1998]<<8 | data[1999], 18999);

    /* Small read (triggering read-ahead). */
    CHECK_INTEQUAL(decode_get_data(decoder, 0, 2000, &data), 2000);
    CHECK_INTEQUAL(data[0]<<8 | data[1], 0);
    CHECK_INTEQUAL(data[1998]<<8 | data[1999], 999);

    /* Small read from read-ahead data. */
    CHECK_INTEQUAL(decode_get_data(decoder, 4000, 2000, &data), 2000);
    CHECK_INTEQUAL(data[0]<<8 | data[1], 2000);
    CHECK_INTEQUAL(data[1998]<<8 | data[1999], 2999);

#ifdef USING_IOQUEUE
    /* Delayed I/O for read-ahead.  To trigger this, we block I/O and then
     * read from the end of the read buffer, triggering a read-ahead. */
    TEST_misc_ioqueue_block_io_thread(1);
    CHECK_INTEQUAL(decode_get_data(decoder, 10000, 2000, &data), 2000);
    CHECK_INTEQUAL(data[0]<<8 | data[1], 5000);
    CHECK_INTEQUAL(data[1998]<<8 | data[1999], 5999);
    /* The read request will be detected as still pending here. */
    CHECK_INTEQUAL(decode_get_data(decoder, 12000, 2000, &data), 2000);
    CHECK_INTEQUAL(data[0]<<8 | data[1], 6000);
    CHECK_INTEQUAL(data[1998]<<8 | data[1999], 6999);

    /* Small read from a different part of the file, cancelling the
     * pending read operation. */
    TEST_misc_ioqueue_unblock_on_wait(1);
    CHECK_INTEQUAL(decode_get_data(decoder, 36000, 2000, &data), 2000);
    CHECK_INTEQUAL(data[0]<<8 | data[1], 18000);
    CHECK_INTEQUAL(data[1998]<<8 | data[1999], 18999);

    /* I/O error on immediate read (only testable on POSIX systems, which
     * route immediate reads through ioqueue). */
# if defined(SIL_PLATFORM_ANDROID) || defined(SIL_PLATFORM_IOS) || defined(SIL_PLATFORM_LINUX) || defined(SIL_PLATFORM_MACOSX)
    TEST_misc_ioqueue_step_io_thread();  // Let the read-ahead finish.
    TEST_misc_ioqueue_iofail_next_read(1);
    TEST_misc_ioqueue_unblock_on_wait(0);
    TEST_misc_ioqueue_block_io_thread(0);
    CHECK_FALSE(decode_get_data(decoder, 0, 2000, &data));
# endif
#endif  // USING_IOQUEUE

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_read_no_async_leak)
{
#if READ_BUFFER_SIZE < 12000 || READ_BUFFER_SIZE >= 20000
# error Need to rewrite test for current READ_BUFFER_SIZE setting.
#endif

    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/long.dat"));

    /* Make sure that closing a decoder with an open async read request
     * doesn't leak the request.  We do this by filling up the async read
     * table except for 2 entries, operating on the decoder, and checking
     * that we can still create 2 new async requests after closing the
     * decoder. */

    char buf[1];
    int reqlist[1000];
    ASSERT(reqlist[0] = sys_file_read_async(file, buf, 1, 0, -1));
    ASSERT(reqlist[1] = sys_file_read_async(file, buf, 1, 0, -1));
    int i;
    for (i = 2; i < lenof(reqlist); i++) {
        if (!(reqlist[i] = sys_file_read_async(file, buf, 1, 0, -1))) {
            break;
        }
    }
    if (i >= lenof(reqlist)) {
        FAIL("Unable to force sys_file_read_async() failure by running out"
             " of async read handles");
    }
    ASSERT(sys_file_wait_async(reqlist[--i]) == 1);
    ASSERT(sys_file_wait_async(reqlist[--i]) == 1);

    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(SOUND_FORMAT_WAV, file,
                                                     0, 40000, 0, 1));

    /* Wait for the first async read to complete. */
    const uint8_t *data;
    CHECK_INTEQUAL(decode_get_data(decoder, 0, 2000, &data), 2000);
    CHECK_INTEQUAL(data[0]<<8 | data[1], 0);
    CHECK_INTEQUAL(data[1998]<<8 | data[1999], 999);

    /* Trigger a read-ahead. */
    CHECK_INTEQUAL(decode_get_data(decoder, 10000, 2000, &data), 2000);
    CHECK_INTEQUAL(data[0]<<8 | data[1], 5000);
    CHECK_INTEQUAL(data[1998]<<8 | data[1999], 5999);

    sound_decode_close(decoder);
    CHECK_TRUE(reqlist[i] = sys_file_read_async(file, buf, 1, 0, -1));
    CHECK_TRUE(reqlist[i+1] = sys_file_read_async(file, buf, 1, 0, -1));
    CHECK_INTEQUAL(sys_file_wait_async(reqlist[i]), 1);
    CHECK_INTEQUAL(sys_file_wait_async(reqlist[i+1]), 1);

    for (i--; i >= 0; i--) {
        ASSERT(sys_file_wait_async(reqlist[i]) == 1);
    }

    sys_file_close(file);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_file_read_async_request_error)
{
#if READ_BUFFER_SIZE < 8000 || READ_BUFFER_SIZE >= 38000
# error Need to rewrite test for current READ_BUFFER_SIZE setting.
#endif

    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/long.dat"));

    char buf[1];
    int reqlist[1000];
    int i;
    for (i = 0; i < lenof(reqlist); i++) {
        if (!(reqlist[i] = sys_file_read_async(file, buf, 1, 0, -1))) {
            break;
        }
    }
    if (i >= lenof(reqlist)) {
        FAIL("Unable to force sys_file_read_async() failure by running out"
             " of async read handles");
    }

    /* This will be unable to create an initial read-ahead request but
     * should still succeed. */
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(SOUND_FORMAT_WAV, file,
                                                     0, 40000, 0, 1));

    const uint8_t *data;

    /* Trigger a read-ahead attempt, which will fail. */
    CHECK_INTEQUAL(decode_get_data(decoder, 0, 2000, &data), 2000);
    CHECK_INTEQUAL(data[0]<<8 | data[1], 0);
    CHECK_INTEQUAL(data[1998]<<8 | data[1999], 999);

    /* Clear out the async request table. */
    for (i--; i >= 0; i--) {
        ASSERT(sys_file_wait_async(reqlist[i]) == 1);
    }

    /* Trigger another read-ahead attempt, which will succeed. */
    CHECK_INTEQUAL(decode_get_data(decoder, 2000, 2000, &data), 2000);
    CHECK_INTEQUAL(data[0]<<8 | data[1], 1000);
    CHECK_INTEQUAL(data[1998]<<8 | data[1999], 1999);

    /* If the first read-ahead attempt failed and the second succeeded as
     * expected, the read buffer now starts at 2000 and will be full
     * (length READ_BUFFER_SIZE) when the read completes.  If we're using
     * ioqueue, we can verify this by blocking I/O on a request which
     * should fit within the expected size of the read buffer; if the
     * read-ahead behavior was incorrect, the test program will block
     * indefinitely. */

#ifdef USING_IOQUEUE
    /* Wait for the read-ahead to finish.  (This range would be in the
     * read-ahead buffer in any case.) */
    CHECK_INTEQUAL(decode_get_data(decoder, 4000, 2000, &data), 2000);
    CHECK_INTEQUAL(data[0]<<8 | data[1], 2000);
    CHECK_INTEQUAL(data[1998]<<8 | data[1999], 2999);

    /* Block I/O and read from the end of the read-ahead buffer.  If the
     * first read-ahead attempt did not fail, this will fall outside the
     * read-ahead buffer contents and the read will block indefinitely. */
    TEST_misc_ioqueue_block_io_thread(1);
    CHECK_INTEQUAL(decode_get_data(decoder, READ_BUFFER_SIZE, 2000, &data),
                   2000);
    TEST_misc_ioqueue_block_io_thread(0);
    CHECK_INTEQUAL(data[0]<<8 | data[1], READ_BUFFER_SIZE/2);
    CHECK_INTEQUAL(data[1998]<<8 | data[1999], READ_BUFFER_SIZE/2 + 999);
#endif

    sound_decode_close(decoder);
    sys_file_close(file);
    return 1;
}

/*************************************************************************/
/************************** Loop-related tests ***************************/
/*************************************************************************/

TEST(test_enable_loop)
{
    SquareState state = {.period = 4, .num_cycles = 1, .samples_out = 0};
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_custom(decode_open, &state, 1));
    CHECK_INTEQUAL(decoder->internal->loop, 0);

    sound_decode_enable_loop(decoder, 1);
    CHECK_INTEQUAL(decoder->internal->loop, 1);

    sound_decode_enable_loop(decoder, 0);
    CHECK_INTEQUAL(decoder->internal->loop, 0);

    /* Any nonzero value should be treated as true (even if the low byte
     * of the value is zero). */
    sound_decode_enable_loop(decoder, INT_MIN);
    CHECK_INTEQUAL(decoder->internal->loop, 1);

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_loop_points)
{
    SquareState state = {.period = 4, .num_cycles = 1, .samples_out = 0};
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_custom(decode_open, &state, 1));
    CHECK_INTEQUAL(decoder->loop_start, 0);
    CHECK_INTEQUAL(decoder->loop_length, 0);

    sound_decode_set_loop_points(decoder, 1, 2);
    CHECK_INTEQUAL(decoder->loop_start, 1);
    CHECK_INTEQUAL(decoder->loop_length, 2);

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_resample_loop)
{
    static const uint8_t decode_data[] = {100, 100, 40, 40, 20};
    SoundDecodeHandle *decoder;
    int16_t pcm[10];

    CHECK_TRUE(decoder = sound_decode_open(SOUND_FORMAT_WAV, decode_data,
                                           lenof(decode_data), 0, 1));
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4);
    sound_decode_set_output_freq(decoder, 5);
    sound_decode_enable_loop(decoder, 1);
    sound_decode_set_loop_points(decoder, 1, 3);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 10));
    CHECK_INTEQUAL(pcm[0], 100);
    CHECK_INTEQUAL(pcm[1], 100);
    CHECK_INTEQUAL(pcm[2], 64);
    CHECK_INTEQUAL(pcm[3], 40);
    CHECK_INTEQUAL(pcm[4], 52);
    CHECK_INTEQUAL(pcm[5], 100);
    CHECK_INTEQUAL(pcm[6], 52);
    CHECK_INTEQUAL(pcm[7], 40);
    CHECK_INTEQUAL(pcm[8], 64);
    CHECK_INTEQUAL(pcm[9], 88);
    sound_decode_close(decoder);

    return 1;
}

/*************************************************************************/
/************************* Error handling tests **************************/
/*************************************************************************/

TEST(test_open_memory_invalid)
{
    static const uint8_t decode_data[] = {100};

    CHECK_FALSE(sound_decode_open(
                    SOUND_FORMAT_AUTODETECT, decode_data, 1, 0, 1));
    CHECK_FALSE(sound_decode_open(SOUND_FORMAT_MP3, decode_data, 1, 0, 1));
    CHECK_FALSE(sound_decode_open(SOUND_FORMAT_WAV, NULL, 1, 0, 1));
    CHECK_FALSE(sound_decode_open(SOUND_FORMAT_WAV, decode_data, 0, 0, 1));
    decode_open_force_failure = 1;
    CHECK_FALSE(sound_decode_open(SOUND_FORMAT_WAV, decode_data, 1, 0, 1));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_file_invalid)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square.dat"));

    CHECK_FALSE(sound_decode_open_from_file(
                    SOUND_FORMAT_AUTODETECT, file, 0, 1, 0, 1));
    CHECK_FALSE(sound_decode_open_from_file(
                    SOUND_FORMAT_MP3, file, 0, 1, 0, 1));
    CHECK_FALSE(sound_decode_open_from_file(
                    SOUND_FORMAT_WAV, NULL, 0, 1, 0, 1));
    CHECK_FALSE(sound_decode_open_from_file(
                    SOUND_FORMAT_WAV, file, 0, 0, 0, 1));
    decode_open_force_failure = 1;
    CHECK_FALSE(sound_decode_open_from_file(
                    SOUND_FORMAT_WAV, file, 0, 1, 1, 1));

    sys_file_close(file);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_custom_invalid)
{
    SquareState state = {.period = 4, .num_cycles = 1, .samples_out = 0};
    CHECK_FALSE(sound_decode_open_custom(NULL, &state, 1));
    CHECK_FALSE(sound_decode_open_custom(decode_open, NULL, 1));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_invalid_freq)
{
    static const uint8_t decode_data[] = {100};

    decode_open_return_freq_0 = 1;
    CHECK_FALSE(sound_decode_open(SOUND_FORMAT_WAV, decode_data, 1, 1, 1));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_loop_points_invalid)
{
    SquareState state = {.period = 4, .num_cycles = 1, .samples_out = 0};
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_custom(decode_open, &state, 1));
    CHECK_INTEQUAL(decoder->loop_start, 0);
    CHECK_INTEQUAL(decoder->loop_length, 0);

    sound_decode_set_loop_points(decoder, -1, 2);
    CHECK_INTEQUAL(decoder->loop_start, 0);
    CHECK_INTEQUAL(decoder->loop_length, 0);

    sound_decode_set_loop_points(decoder, 1, -2);
    CHECK_INTEQUAL(decoder->loop_start, 0);
    CHECK_INTEQUAL(decoder->loop_length, 0);

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_output_freq_invalid)
{
    SquareState state = {.period = 4, .num_cycles = 1, .samples_out = 0};
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_custom(decode_open, &state, 1));

    sound_decode_set_output_freq(decoder, 0);

    int16_t pcm[3];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 3));
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], -10000);

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_pcm_invalid)
{
    SquareState state = {.period = 4, .num_cycles = 1, .samples_out = 0};
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_custom(decode_open, &state, 1));

    int16_t pcm[1];
    CHECK_FALSE(sound_decode_get_pcm(decoder, NULL, 1));
    CHECK_FALSE(sound_decode_get_pcm(decoder, pcm, 0));

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_data_bad_type)
{
    SquareState state = {.period = 4, .num_cycles = 1, .samples_out = 0};
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_custom(decode_open, &state, 1));

    /* Muck with the data_type field to cover the default code path for
     * the switch (this->internal->data_type) in decode_get_data().  (This
     * path should never be hit outside testing; we run it only to improve
     * coverage.) */
    ASSERT(decoder->internal->data_type == SOUND_DECODE_CUSTOM);
    decoder->internal->data_type = SOUND_DECODE_BUFFER - 1;  // Invalid valid.
    decoder->internal->datalen = 1;
    const uint8_t *data;
    CHECK_INTEQUAL(decode_get_data(decoder, 0, 1, &data), 1);
    decoder->internal->datalen = 0;
    decoder->internal->data_type = SOUND_DECODE_CUSTOM;

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_data_invalid)
{
    SquareState state = {.period = 4, .num_cycles = 1, .samples_out = 0};
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_custom(decode_open, &state, 1));

    const uint8_t *data;
    CHECK_FALSE(decode_get_data(decoder, 0, 1, &data));

    decoder->internal->datalen = 1;
    CHECK_FALSE(decode_get_data(decoder, 0, 1, &data));
    decoder->internal->datalen = 0;

    sound_decode_close(decoder);
    return 1;
}

/*************************************************************************/
/*************************************************************************/
