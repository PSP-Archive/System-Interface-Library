/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sound/decode-wav.c: Tests for decoding of RIFF WAVE audio data.
 */

#include "src/base.h"
#include "src/memory.h"
#include "src/sound.h"
#include "src/sound/decode.h"
#include "src/sysdep.h"
#include "src/test/base.h"
#include "src/thread.h"

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

DEFINE_GENERIC_TEST_RUNNER(test_sound_decode_wav)

/*-----------------------------------------------------------------------*/

TEST_INIT(init)
{
    CHECK_TRUE(thread_init());
    CHECK_TRUE(sys_file_init());
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
    sys_file_cleanup();
    thread_cleanup();
    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_decode)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square.wav"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_WAV, file, 0, sys_file_size(file), 0, 1));
    sys_file_close(file);
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);

    int16_t pcm[35];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 3));
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], -10000);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 3));
    CHECK_INTEQUAL(pcm[0], -10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 35));
    CHECK_INTEQUAL(pcm[33], -10000);
    CHECK_INTEQUAL(pcm[34], 0);

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_decode_memory_failure)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square.wav"));
    const int64_t datalen = sys_file_size(file);
    void *data;
    ASSERT(data = mem_alloc(datalen, 0, 0));
    ASSERT(sys_file_read(file, data, datalen) == datalen);
    sys_file_close(file);

    SoundDecodeHandle *decoder;
    CHECK_MEMORY_FAILURES(
        decoder = sound_decode_open(SOUND_FORMAT_WAV, data, datalen, 0, 1));
    int16_t pcm[3];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 3));
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], -10000);
    sound_decode_close(decoder);

    mem_free(data);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_odd_chunk_size)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open(
               "testdata/sound/square-odd-chunk-size.wav"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_WAV, file, 0, sys_file_size(file), 0, 1));
    sys_file_close(file);
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);

    int16_t pcm[38];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 3));
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], -10000);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 38));
    CHECK_INTEQUAL(pcm[36], -10000);
    CHECK_INTEQUAL(pcm[37], 0);

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_loop)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square.wav"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_WAV, file, 0, sys_file_size(file), 1, 1));
    sys_file_close(file);
    sound_decode_set_loop_points(decoder, 2, 3);
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);

    int16_t pcm[9];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 9));
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], -10000);
    CHECK_INTEQUAL(pcm[3], -10000);
    CHECK_INTEQUAL(pcm[4], 10000);
    CHECK_INTEQUAL(pcm[5], -10000);
    CHECK_INTEQUAL(pcm[6], -10000);
    CHECK_INTEQUAL(pcm[7], 10000);
    CHECK_INTEQUAL(pcm[8], -10000);
    CHECK_FLOATRANGE(sound_decode_get_position(decoder), 2.5f/4000, 3.5f/4000);

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_loop_to_end)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square.wav"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_WAV, file, 0, sys_file_size(file), 1, 1));
    sys_file_close(file);
    sound_decode_set_loop_points(decoder, 2, 0);
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);

    int16_t pcm[43];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 43));
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], -10000);
    CHECK_INTEQUAL(pcm[3], -10000);
    CHECK_INTEQUAL(pcm[4], 10000);
    CHECK_INTEQUAL(pcm[5], 10000);
    CHECK_INTEQUAL(pcm[38], -10000);
    CHECK_INTEQUAL(pcm[39], -10000);
    CHECK_INTEQUAL(pcm[40], -10000);
    CHECK_INTEQUAL(pcm[41], -10000);
    CHECK_INTEQUAL(pcm[42], 10000);

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_loop_past_end)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square.wav"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_WAV, file, 0, sys_file_size(file), 1, 1));
    sys_file_close(file);
    sound_decode_set_loop_points(decoder, 2, 43);
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);

    int16_t pcm[43];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 43));
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], -10000);
    CHECK_INTEQUAL(pcm[3], -10000);
    CHECK_INTEQUAL(pcm[4], 10000);
    CHECK_INTEQUAL(pcm[5], 10000);
    CHECK_INTEQUAL(pcm[38], -10000);
    CHECK_INTEQUAL(pcm[39], -10000);
    CHECK_INTEQUAL(pcm[40], -10000);
    CHECK_INTEQUAL(pcm[41], -10000);
    CHECK_INTEQUAL(pcm[42], 10000);

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_loop_from_smpl)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square-loop.wav"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_WAV, file, 0, sys_file_size(file), 1, 1));
    sys_file_close(file);
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);

    int16_t pcm[9];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 9));
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], -10000);
    CHECK_INTEQUAL(pcm[3], -10000);
    CHECK_INTEQUAL(pcm[4], 10000);
    CHECK_INTEQUAL(pcm[5], -10000);
    CHECK_INTEQUAL(pcm[6], -10000);
    CHECK_INTEQUAL(pcm[7], 10000);
    CHECK_INTEQUAL(pcm[8], -10000);
    CHECK_FLOATRANGE(sound_decode_get_position(decoder), 2.5f/4000, 3.5f/4000);

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_loop_from_smpl_zero_loops)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square-loop.wav"));
    const int64_t datalen = sys_file_size(file);
    uint8_t *data;
    ASSERT(data = mem_alloc(datalen, 0, 0));
    ASSERT(sys_file_read(file, data, datalen) == datalen);
    sys_file_close(file);

    ASSERT(data[0x48] == 1);
    data[0x48] = 0;

    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open(
                   SOUND_FORMAT_WAV, data, datalen, 1, 1));
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);

    int16_t pcm[42];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 42));
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], -10000);
    CHECK_INTEQUAL(pcm[39], -10000);
    CHECK_INTEQUAL(pcm[40], 10000);
    CHECK_INTEQUAL(pcm[41], 10000);
    CHECK_FLOATRANGE(sound_decode_get_position(decoder), 1.5f/4000, 2.5f/4000);

    sound_decode_close(decoder);
    mem_free(data);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_loop_from_smpl_bad_endpoints)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square-loop.wav"));
    const int64_t datalen = sys_file_size(file);
    uint8_t *data;
    ASSERT(data = mem_alloc(datalen, 0, 0));
    ASSERT(sys_file_read(file, data, datalen) == datalen);
    sys_file_close(file);

    ASSERT(data[0x5C] == 4);
    data[0x5C] = data[0x58] - 1;  // Set the loop to have length 0.

    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open(
                   SOUND_FORMAT_WAV, data, datalen, 1, 1));
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);

    int16_t pcm[42];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 42));
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], -10000);
    CHECK_INTEQUAL(pcm[39], -10000);
    CHECK_INTEQUAL(pcm[40], 10000);
    CHECK_INTEQUAL(pcm[41], 10000);
    CHECK_FLOATRANGE(sound_decode_get_position(decoder), 1.5f/4000, 2.5f/4000);

    sound_decode_close(decoder);
    mem_free(data);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_loop_from_smpl_too_long)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square-loop.wav"));
    const int64_t datalen = sys_file_size(file);
    uint8_t *data;
    ASSERT(data = mem_alloc(datalen, 0, 0));
    ASSERT(sys_file_read(file, data, datalen) == datalen);
    sys_file_close(file);

    ASSERT(data[0x5C] == 4);
    data[0x5C] = 40;  // Past the end of the file.

    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open(
                   SOUND_FORMAT_WAV, data, datalen, 1, 1));
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);

    int16_t pcm[42];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 42));
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], -10000);
    CHECK_INTEQUAL(pcm[39], -10000);
    CHECK_INTEQUAL(pcm[40], 10000);
    CHECK_INTEQUAL(pcm[41], 10000);
    CHECK_FLOATRANGE(sound_decode_get_position(decoder), 1.5f/4000, 2.5f/4000);

    sound_decode_close(decoder);
    mem_free(data);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_loop_from_smpl_short_chunk)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square-loop.wav"));
    const int64_t datalen = sys_file_size(file);
    uint8_t *data;
    ASSERT(data = mem_alloc(datalen, 0, 0));
    ASSERT(sys_file_read(file, data, datalen) == datalen);
    sys_file_close(file);

    /* Shrink the smpl chunk by 1 byte so it's considered too short.
     * We don't need to move any data around since the following chunk
     * will be 2-byte aligned anyway, leaving it in the same place. */
    ASSERT(data[0x28] == 0x3C);
    data[0x28]--;

    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open(
                   SOUND_FORMAT_WAV, data, datalen, 1, 1));
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);

    int16_t pcm[42];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 42));
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], -10000);
    CHECK_INTEQUAL(pcm[39], -10000);
    CHECK_INTEQUAL(pcm[40], 10000);
    CHECK_INTEQUAL(pcm[41], 10000);
    CHECK_FLOATRANGE(sound_decode_get_position(decoder), 1.5f/4000, 2.5f/4000);

    sound_decode_close(decoder);
    mem_free(data);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_enable_loop)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square.wav"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_WAV, file, 0, sys_file_size(file), 0, 1));
    sys_file_close(file);
    sound_decode_set_loop_points(decoder, 2, 3);
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);

    int16_t pcm[33];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 3));
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], -10000);

    /* We should be able to enable the loop before we hit its endpoint. */
    sound_decode_enable_loop(decoder, 1);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 4));
    CHECK_INTEQUAL(pcm[0], -10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], -10000);
    CHECK_INTEQUAL(pcm[3], -10000);

    /* We should be able to disable the loop and play past its endpoint. */
    sound_decode_enable_loop(decoder, 0);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 3));
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], -10000);

    /* Attempting to enable the loop when past its endpoint should not
     * cause an immediate loop. */
    sound_decode_enable_loop(decoder, 1);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 33));
    CHECK_INTEQUAL(pcm[0], -10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], -10000);
    CHECK_INTEQUAL(pcm[32], -10000);
    CHECK_FALSE(sound_decode_get_pcm(decoder, pcm, 1));

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_short_read)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square.wav"));
    const int64_t size = sys_file_size(file);
    uint8_t *data;
    ASSERT(data = mem_alloc(size, 0, 0));
    ASSERT(sys_file_read(file, data, size) == size);
    sys_file_close(file);

    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open(SOUND_FORMAT_WAV, data, size, 0, 1));
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);

    /* Hack the decoder handle to force truncation after 3 samples + 1 byte. */
    decoder->internal->datalen = 51;

    int16_t pcm[4];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 4));
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], -10000);
    CHECK_INTEQUAL(pcm[3], 0);

    sound_decode_close(decoder);
    mem_free(data);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_invalid_format)
{
    static const char * const files[] = {
        "testdata/sound/bad/empty-file.wav",
        "testdata/sound/bad/not-riff.wav",
        "testdata/sound/bad/not-wave.wav",
        "testdata/sound/bad/large-chunk.wav",
        "testdata/sound/bad/missing-fmt.wav",
        "testdata/sound/bad/wrong-fmt-size.wav",
        "testdata/sound/bad/wrong-codec.wav",
        "testdata/sound/bad/no-channels.wav",
        "testdata/sound/bad/not-16bits.wav",
        "testdata/sound/bad/zero-freq.wav",
        "testdata/sound/bad/large-freq.wav",
        "testdata/sound/bad/large-bitrate.wav",
        "testdata/sound/bad/missing-data.wav",
    };
    for (int i = 0; i < lenof(files); i++) {
        SysFile *file;
        if (!(file = wrap_sys_file_open(files[i]))) {
            FAIL("wrap_sys_file_open(%s) failed: %s", files[i],
                 sys_last_errstr());
        }
        if (sound_decode_open_from_file(
                SOUND_FORMAT_WAV, file, 0, sys_file_size(file), 0, 1)) {
            FAIL("sound_decode_open_from_file() for %s was not false as"
                 " expected", files[i]);
        }
        sys_file_close(file);
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_invalid_format_no_async_leak)
{
    /* This is actually a test of the core sound_decode_open_from_file()
     * function, similar to test_read_file_no_async_leak() in decode.c, but
     * in order to exercise the proper code path we need a standard decoder
     * module to return failure, so we run the test here. */

    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/bad/not-riff.wav"));

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

    CHECK_FALSE(sound_decode_open_from_file(
                    SOUND_FORMAT_WAV, file, 0, sys_file_size(file), 0, 1));
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

TEST(test_no_data)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/bad/no-data.wav"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_WAV, file, 0, sys_file_size(file), 0, 1));
    sys_file_close(file);
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);

    int16_t pcm[1];
    CHECK_FALSE(sound_decode_get_pcm(decoder, pcm, 1));

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_no_data_looped)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/bad/no-data.wav"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_WAV, file, 0, sys_file_size(file), 1, 1));
    sys_file_close(file);
    sound_decode_set_loop_points(decoder, 0, 1);
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);

    int16_t pcm[1];
    /* Make sure this doesn't go into an infinite loop trying to loop over
     * a zero-length file. */
    CHECK_FALSE(sound_decode_get_pcm(decoder, pcm, 1));

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_truncated_data)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/bad/truncated-data.wav"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_WAV, file, 0, sys_file_size(file), 0, 1));
    sys_file_close(file);
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);

    int16_t pcm[4];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 4));
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], -10000);
    CHECK_INTEQUAL(pcm[3], 0);

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_truncated_stereo)
{
    SysFile *file;
    ASSERT(file =
               wrap_sys_file_open("testdata/sound/bad/truncated-stereo.wav"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_WAV, file, 0, sys_file_size(file), 0, 1));
    sys_file_close(file);
    CHECK_TRUE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);

    int16_t pcm[4];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 2));
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    /* File data/length is the same as truncated-data.wav, but for stereo,
     * we should discard the final left channel sample since the right
     * channel sample is truncated. */
    CHECK_INTEQUAL(pcm[2], 0);
    CHECK_INTEQUAL(pcm[3], 0);

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_extra_data)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/bad/extra-data.wav"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_WAV, file, 0, sys_file_size(file), 0, 1));
    sys_file_close(file);
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);

    int16_t pcm[41];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 41));
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], -10000);
    CHECK_INTEQUAL(pcm[39], -10000);
    CHECK_INTEQUAL(pcm[40], 0);

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_zero_data_size)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/bad/zero-data-size.wav"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_WAV, file, 0, sys_file_size(file), 0, 1));
    sys_file_close(file);
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);

    int16_t pcm[41];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 41));
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], -10000);
    CHECK_INTEQUAL(pcm[39], -10000);
    CHECK_INTEQUAL(pcm[40], 0);

    sound_decode_close(decoder);
    return 1;
}

/*************************************************************************/
/*************************************************************************/
