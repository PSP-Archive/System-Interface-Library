/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sound/decode-ogg.c: Tests for decoding of Ogg Vorbis audio data.
 */

#include "src/base.h"
#include "src/memory.h"
#include "src/sound.h"
#include "src/sound/decode.h"
#include "src/sound/decode-ogg.h"
#include "src/sysdep.h"
#include "src/test/base.h"
#include "src/thread.h"

#ifndef SIL_SOUND_INCLUDE_OGG
int test_sound_decode_ogg(void) {
    DLOG("Ogg Vorbis support disabled, nothing to test.");
    return 1;
}
#else  // To the end of the file.

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Expected PCM output from the Ogg Vorbis test files. */

static const int16_t mono_pcm[] = {
    9763, 9445, -9591, -9749, 9573, 9868,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   // These aren't checked.
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    -10150, -9443
};

static const int16_t stereo_pcm[] = {
    9401, -9313, 9059, -9043, 9128, -9275,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // These aren't checked.
    -10661
};

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

DEFINE_GENERIC_TEST_RUNNER(test_sound_decode_ogg)

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
    ASSERT(file = wrap_sys_file_open("testdata/sound/square.ogg"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_OGG, file, 0, sys_file_size(file), 0, 1));
    sys_file_close(file);
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);

    int16_t pcm[35];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 3));
    CHECK_INTEQUAL(pcm[0], mono_pcm[0]);
    CHECK_INTEQUAL(pcm[1], mono_pcm[1]);
    CHECK_INTEQUAL(pcm[2], mono_pcm[2]);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 3));
    CHECK_INTEQUAL(pcm[0], mono_pcm[3]);
    CHECK_INTEQUAL(pcm[1], mono_pcm[4]);
    CHECK_INTEQUAL(pcm[2], mono_pcm[5]);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 35));
    CHECK_INTEQUAL(pcm[33], mono_pcm[39]);
    CHECK_INTEQUAL(pcm[34], 0);

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_decode_memory_failure)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square.ogg"));
    const int64_t datalen = sys_file_size(file);
    void *data;
    ASSERT(data = mem_alloc(datalen, 0, 0));
    ASSERT(sys_file_read(file, data, datalen) == datalen);
    sys_file_close(file);

    SoundDecodeHandle *decoder;
    CHECK_MEMORY_FAILURES_TO(
        200,
        decoder = sound_decode_open(SOUND_FORMAT_OGG, data, datalen, 0, 1));
    int16_t pcm[3];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 3));
    CHECK_INTEQUAL(pcm[0], mono_pcm[0]);
    CHECK_INTEQUAL(pcm[1], mono_pcm[1]);
    CHECK_INTEQUAL(pcm[2], mono_pcm[2]);
    sound_decode_close(decoder);

    mem_free(data);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_decode_stereo)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square-stereo.ogg"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_OGG, file, 0, sys_file_size(file), 0, 1));
    sys_file_close(file);
    CHECK_TRUE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);

    int16_t pcm[30];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 3));
    CHECK_INTEQUAL(pcm[0], stereo_pcm[0]);
    CHECK_INTEQUAL(pcm[1], stereo_pcm[0]);
    CHECK_INTEQUAL(pcm[2], stereo_pcm[1]);
    CHECK_INTEQUAL(pcm[3], stereo_pcm[1]);
    CHECK_INTEQUAL(pcm[4], stereo_pcm[2]);
    CHECK_INTEQUAL(pcm[5], stereo_pcm[2]);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 3));
    CHECK_INTEQUAL(pcm[0], stereo_pcm[3]);
    CHECK_INTEQUAL(pcm[1], stereo_pcm[3]);
    CHECK_INTEQUAL(pcm[2], stereo_pcm[4]);
    CHECK_INTEQUAL(pcm[3], stereo_pcm[4]);
    CHECK_INTEQUAL(pcm[4], stereo_pcm[5]);
    CHECK_INTEQUAL(pcm[5], stereo_pcm[5]);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 15));
    CHECK_INTEQUAL(pcm[26], stereo_pcm[19]);
    CHECK_INTEQUAL(pcm[27], stereo_pcm[19]);
    CHECK_INTEQUAL(pcm[28], 0);
    CHECK_INTEQUAL(pcm[29], 0);

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_loop)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square.ogg"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_OGG, file, 0, sys_file_size(file), 1, 1));
    sys_file_close(file);
    sound_decode_set_loop_points(decoder, 2, 3);
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);

    int16_t pcm[9];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 9));
    CHECK_INTEQUAL(pcm[0], mono_pcm[0]);
    CHECK_INTEQUAL(pcm[1], mono_pcm[1]);
    CHECK_INTEQUAL(pcm[2], mono_pcm[2]);
    CHECK_INTEQUAL(pcm[3], mono_pcm[3]);
    CHECK_INTEQUAL(pcm[4], mono_pcm[4]);
    CHECK_INTEQUAL(pcm[5], mono_pcm[2]);
    CHECK_INTEQUAL(pcm[6], mono_pcm[3]);
    CHECK_INTEQUAL(pcm[7], mono_pcm[4]);
    CHECK_INTEQUAL(pcm[8], mono_pcm[2]);
    CHECK_FLOATRANGE(sound_decode_get_position(decoder), 2.5f/4000, 3.5f/4000);

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_loop_to_end)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square.ogg"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_OGG, file, 0, sys_file_size(file), 1, 1));
    sys_file_close(file);
    sound_decode_set_loop_points(decoder, 2, 0);
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);

    int16_t pcm[43];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 43));
    CHECK_INTEQUAL(pcm[0], mono_pcm[0]);
    CHECK_INTEQUAL(pcm[1], mono_pcm[1]);
    CHECK_INTEQUAL(pcm[2], mono_pcm[2]);
    CHECK_INTEQUAL(pcm[3], mono_pcm[3]);
    CHECK_INTEQUAL(pcm[4], mono_pcm[4]);
    CHECK_INTEQUAL(pcm[5], mono_pcm[5]);
    CHECK_INTEQUAL(pcm[38], mono_pcm[38]);
    CHECK_INTEQUAL(pcm[39], mono_pcm[39]);
    CHECK_INTEQUAL(pcm[40], mono_pcm[2]);
    CHECK_INTEQUAL(pcm[41], mono_pcm[3]);
    CHECK_INTEQUAL(pcm[42], mono_pcm[4]);

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_loop_past_end)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square.ogg"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_OGG, file, 0, sys_file_size(file), 1, 1));
    sys_file_close(file);
    sound_decode_set_loop_points(decoder, 2, 42);
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);

    int16_t pcm[43];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 43));
    CHECK_INTEQUAL(pcm[0], mono_pcm[0]);
    CHECK_INTEQUAL(pcm[1], mono_pcm[1]);
    CHECK_INTEQUAL(pcm[2], mono_pcm[2]);
    CHECK_INTEQUAL(pcm[3], mono_pcm[3]);
    CHECK_INTEQUAL(pcm[4], mono_pcm[4]);
    CHECK_INTEQUAL(pcm[5], mono_pcm[5]);
    CHECK_INTEQUAL(pcm[38], mono_pcm[38]);
    CHECK_INTEQUAL(pcm[39], mono_pcm[39]);
    CHECK_INTEQUAL(pcm[40], mono_pcm[2]);
    CHECK_INTEQUAL(pcm[41], mono_pcm[3]);
    CHECK_INTEQUAL(pcm[42], mono_pcm[4]);

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_loop_starts_at_end)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square.ogg"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_OGG, file, 0, sys_file_size(file), 1, 1));
    sys_file_close(file);
    sound_decode_set_loop_points(decoder, 40, 10);
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);

    int16_t pcm[40];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 40));
    CHECK_INTEQUAL(pcm[0], mono_pcm[0]);
    CHECK_INTEQUAL(pcm[1], mono_pcm[1]);
    CHECK_INTEQUAL(pcm[2], mono_pcm[2]);
    CHECK_INTEQUAL(pcm[3], mono_pcm[3]);
    CHECK_INTEQUAL(pcm[4], mono_pcm[4]);
    CHECK_INTEQUAL(pcm[5], mono_pcm[5]);
    CHECK_INTEQUAL(pcm[38], mono_pcm[38]);
    CHECK_INTEQUAL(pcm[39], mono_pcm[39]);
    CHECK_FALSE(sound_decode_get_pcm(decoder, pcm, 1));

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_loop_starts_past_end)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square.ogg"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_OGG, file, 0, sys_file_size(file), 1, 1));
    sys_file_close(file);
    sound_decode_set_loop_points(decoder, 42, 10);
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);

    int16_t pcm[40];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 40));
    CHECK_INTEQUAL(pcm[0], mono_pcm[0]);
    CHECK_INTEQUAL(pcm[1], mono_pcm[1]);
    CHECK_INTEQUAL(pcm[2], mono_pcm[2]);
    CHECK_INTEQUAL(pcm[3], mono_pcm[3]);
    CHECK_INTEQUAL(pcm[4], mono_pcm[4]);
    CHECK_INTEQUAL(pcm[5], mono_pcm[5]);
    CHECK_INTEQUAL(pcm[38], mono_pcm[38]);
    CHECK_INTEQUAL(pcm[39], mono_pcm[39]);
    CHECK_FALSE(sound_decode_get_pcm(decoder, pcm, 1));

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_loop_from_comments)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square-loop.ogg"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_OGG, file, 0, sys_file_size(file), 1, 1));
    sys_file_close(file);
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);

    int16_t pcm[41];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 41));
    CHECK_INTEQUAL(pcm[0], mono_pcm[0]);
    CHECK_INTEQUAL(pcm[1], mono_pcm[1]);
    CHECK_INTEQUAL(pcm[2], mono_pcm[2]);
    CHECK_INTEQUAL(pcm[38], mono_pcm[38]);
    CHECK_INTEQUAL(pcm[39], mono_pcm[1]);
    CHECK_INTEQUAL(pcm[40], mono_pcm[2]);
    CHECK_FLOATRANGE(sound_decode_get_position(decoder), 2.5f/4000, 3.5f/4000);

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_loop_from_comments_value_termination)
{
    /* This file has a comment of length 48 right after the LOOPSTART
     * comment, so if the comment value is not properly null-terminated,
     * the loop start point will be read as "10" instead of "1". */
    SysFile *file;
    ASSERT(file = wrap_sys_file_open(
               "testdata/sound/square-loop-value-termination.ogg"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_OGG, file, 0, sys_file_size(file), 1, 1));
    sys_file_close(file);
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);

    int16_t pcm[41];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 41));
    CHECK_INTEQUAL(pcm[0], mono_pcm[0]);
    CHECK_INTEQUAL(pcm[1], mono_pcm[1]);
    CHECK_INTEQUAL(pcm[2], mono_pcm[2]);
    CHECK_INTEQUAL(pcm[38], mono_pcm[38]);
    CHECK_INTEQUAL(pcm[39], mono_pcm[1]);
    CHECK_INTEQUAL(pcm[40], mono_pcm[2]);
    CHECK_FLOATRANGE(sound_decode_get_position(decoder), 2.5f/4000, 3.5f/4000);

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_loop_from_comments_truncated)
{
    /* This file has a dummy comment which pads the comment block to 1 byte
     * longer than the read size used when searching for loop comments, so
     * if the truncation is not properly detected, the length comment will
     * be read as "3" rather than discarded. */
    SysFile *file;
    ASSERT(file = wrap_sys_file_open(
               "testdata/sound/square-loop-read-truncation.ogg"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_OGG, file, 0, sys_file_size(file), 1, 1));
    sys_file_close(file);
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);

    int16_t pcm[41];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 41));
    CHECK_INTEQUAL(pcm[0], mono_pcm[0]);
    CHECK_INTEQUAL(pcm[1], mono_pcm[1]);
    CHECK_INTEQUAL(pcm[2], mono_pcm[2]);
    CHECK_INTEQUAL(pcm[38], mono_pcm[38]);
    CHECK_INTEQUAL(pcm[39], mono_pcm[1]);
    CHECK_INTEQUAL(pcm[40], mono_pcm[2]);
    CHECK_FLOATRANGE(sound_decode_get_position(decoder), 2.5f/4000, 3.5f/4000);

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_loop_from_comments_empty_start)
{
    /* libnogg doesn't care if the Ogg CRC doesn't match, so just load the
     * file and tweak the data a bit. */
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square-loop.ogg"));
    const int64_t datalen = sys_file_size(file);
    uint8_t *data;
    ASSERT(data = mem_alloc(datalen, 0, 0));
    ASSERT(sys_file_read(file, data, datalen) == datalen);
    sys_file_close(file);

    ASSERT(data[0x9C] == 0x18);  // Add 1 byte to the "Comment=..." comment.
    data[0x9C]++;
    memcpy(&data[0xB9], "\x0A\0\0\0LOOPSTART=", 14);

    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open(
                   SOUND_FORMAT_OGG, data, datalen, 1, 1));
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);

    int16_t pcm[42];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 42));
    CHECK_INTEQUAL(pcm[0], mono_pcm[0]);
    CHECK_INTEQUAL(pcm[1], mono_pcm[1]);
    CHECK_INTEQUAL(pcm[2], mono_pcm[2]);
    CHECK_INTEQUAL(pcm[39], mono_pcm[39]);
    CHECK_INTEQUAL(pcm[40], mono_pcm[0]);
    CHECK_INTEQUAL(pcm[41], mono_pcm[1]);
    CHECK_FLOATRANGE(sound_decode_get_position(decoder), 1.5f/4000, 2.5f/4000);

    sound_decode_close(decoder);
    mem_free(data);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_loop_from_comments_invalid_start)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square-loop.ogg"));
    const int64_t datalen = sys_file_size(file);
    uint8_t *data;
    ASSERT(data = mem_alloc(datalen, 0, 0));
    ASSERT(sys_file_read(file, data, datalen) == datalen);
    sys_file_close(file);

    ASSERT(data[0xC6] == '1');
    data[0xC6] = 'z';

    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open(
                   SOUND_FORMAT_OGG, data, datalen, 1, 1));
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);

    int16_t pcm[42];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 42));
    CHECK_INTEQUAL(pcm[0], mono_pcm[0]);
    CHECK_INTEQUAL(pcm[1], mono_pcm[1]);
    CHECK_INTEQUAL(pcm[2], mono_pcm[2]);
    CHECK_INTEQUAL(pcm[39], mono_pcm[39]);
    CHECK_INTEQUAL(pcm[40], mono_pcm[0]);
    CHECK_INTEQUAL(pcm[41], mono_pcm[1]);
    CHECK_FLOATRANGE(sound_decode_get_position(decoder), 1.5f/4000, 2.5f/4000);

    sound_decode_close(decoder);
    mem_free(data);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_loop_from_comments_empty_length)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square-loop.ogg"));
    const int64_t datalen = sys_file_size(file);
    uint8_t *data;
    ASSERT(data = mem_alloc(datalen, 0, 0));
    ASSERT(sys_file_read(file, data, datalen) == datalen);
    sys_file_close(file);

    ASSERT(data[0xB8] == 0x0B);  // Add 2 bytes to the "LOOPSTART=..." comment.
    data[0xB8] += 2;
    memcpy(&data[0xC6], "001\x0B\0\0\0LOOPLENGTH=", 18);

    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open(
                   SOUND_FORMAT_OGG, data, datalen, 1, 1));
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);

    int16_t pcm[42];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 42));
    CHECK_INTEQUAL(pcm[0], mono_pcm[0]);
    CHECK_INTEQUAL(pcm[1], mono_pcm[1]);
    CHECK_INTEQUAL(pcm[2], mono_pcm[2]);
    CHECK_INTEQUAL(pcm[39], mono_pcm[39]);
    CHECK_INTEQUAL(pcm[40], mono_pcm[0]);
    CHECK_INTEQUAL(pcm[41], mono_pcm[1]);
    CHECK_FLOATRANGE(sound_decode_get_position(decoder), 1.5f/4000, 2.5f/4000);

    sound_decode_close(decoder);
    mem_free(data);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_loop_from_comments_invalid_length)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square-loop.ogg"));
    const int64_t datalen = sys_file_size(file);
    uint8_t *data;
    ASSERT(data = mem_alloc(datalen, 0, 0));
    ASSERT(sys_file_read(file, data, datalen) == datalen);
    sys_file_close(file);

    ASSERT(data[0xD6] == '3');
    data[0xD6] = 'z';

    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open(
                   SOUND_FORMAT_OGG, data, datalen, 1, 1));
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);

    int16_t pcm[42];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 42));
    CHECK_INTEQUAL(pcm[0], mono_pcm[0]);
    CHECK_INTEQUAL(pcm[1], mono_pcm[1]);
    CHECK_INTEQUAL(pcm[2], mono_pcm[2]);
    CHECK_INTEQUAL(pcm[39], mono_pcm[39]);
    CHECK_INTEQUAL(pcm[40], mono_pcm[0]);
    CHECK_INTEQUAL(pcm[41], mono_pcm[1]);
    CHECK_FLOATRANGE(sound_decode_get_position(decoder), 1.5f/4000, 2.5f/4000);

    sound_decode_close(decoder);
    mem_free(data);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_enable_loop)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square.ogg"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_OGG, file, 0, sys_file_size(file), 0, 1));
    sys_file_close(file);
    sound_decode_set_loop_points(decoder, 2, 3);
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);

    int16_t pcm[34];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 3));
    CHECK_INTEQUAL(pcm[0], mono_pcm[0]);
    CHECK_INTEQUAL(pcm[1], mono_pcm[1]);
    CHECK_INTEQUAL(pcm[2], mono_pcm[2]);

    /* We should be able to enable the loop before we hit its endpoint. */
    sound_decode_enable_loop(decoder, 1);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 4));
    CHECK_INTEQUAL(pcm[0], mono_pcm[3]);
    CHECK_INTEQUAL(pcm[1], mono_pcm[4]);
    CHECK_INTEQUAL(pcm[2], mono_pcm[2]);
    CHECK_INTEQUAL(pcm[3], mono_pcm[3]);

    /* We should be able to disable the loop and play past its endpoint. */
    sound_decode_enable_loop(decoder, 0);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 2));
    CHECK_INTEQUAL(pcm[0], mono_pcm[4]);
    CHECK_INTEQUAL(pcm[1], mono_pcm[5]);

    /* Attempting to enable the loop when past its endpoint should not
     * cause an immediate loop. */
    sound_decode_enable_loop(decoder, 1);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 34));
    CHECK_INTEQUAL(pcm[32], mono_pcm[38]);
    CHECK_INTEQUAL(pcm[33], mono_pcm[39]);
    CHECK_FALSE(sound_decode_get_pcm(decoder, pcm, 1));

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_invalid_format)
{
    static const char * const files[] = {
        "testdata/sound/bad/empty-file.ogg",
        "testdata/sound/bad/not-vorbis.ogg",
        "testdata/sound/bad/4-channels.ogg",
        "testdata/sound/bad/max-sample-rate.ogg",
    };
    for (int i = 0; i < lenof(files); i++) {
        SysFile *file;
        if (!(file = wrap_sys_file_open(files[i]))) {
            FAIL("wrap_sys_file_open(%s) failed: %s", files[i],
                 sys_last_errstr());
        }
        if (sound_decode_open_from_file(
                SOUND_FORMAT_OGG, file, 0, sys_file_size(file), 0, 1)) {
            FAIL("sound_decode_open_from_file() for %s was not false as"
                 " expected", files[i]);
        }
        sys_file_close(file);
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_corrupt_data)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/bad/corrupt-data.ogg"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_OGG, file, 0, sys_file_size(file), 0, 1));
    sys_file_close(file);
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);

    int16_t pcm[1];
    CHECK_FALSE(sound_decode_get_pcm(decoder, pcm, 1));

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_corrupt_data_looped)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/bad/corrupt-data.ogg"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_OGG, file, 0, sys_file_size(file), 1, 1));
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

TEST(test_hole_in_data)
{
    SysFile *file;
    SoundDecodeHandle *decoder;
    int16_t pcm[1000];
    const int32_t hole_pos = 49664;

    /* We need >1 audio data packet to trigger a recoverable error, so we
     * use a 30-second square wave.  First make sure we have the correct
     * sample value for the original file. */
    ASSERT(file = wrap_sys_file_open("testdata/sound/square-long.ogg"));
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_OGG, file, 0, sys_file_size(file), 0, 1));
    sys_file_close(file);
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);
    for (int32_t pos = 0; pos < hole_pos; pos += lenof(pcm)) {
        const int toread = ubound(hole_pos - pos, lenof(pcm));
        CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, toread));
    }
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 1));
    CHECK_INTEQUAL(pcm[0], 9487);
    sound_decode_close(decoder);

    ASSERT(file = wrap_sys_file_open("testdata/sound/bad/holey-data.ogg"));
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_OGG, file, 0, sys_file_size(file), 0, 1));
    sys_file_close(file);
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);
    for (int32_t pos = 0; pos < hole_pos; pos += lenof(pcm)) {
        const int toread = ubound(hole_pos - pos, lenof(pcm));
        CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, toread));
    }
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 1));
    CHECK_INTEQUAL(pcm[0], 9487);
    sound_decode_close(decoder);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_decode_position_mismatch)
{
    SysFile *file;
    SoundDecodeHandle *decoder;
    int16_t pcm[1000];
    const int len = 120064;

    ASSERT(file = wrap_sys_file_open("testdata/sound/bad/granulepos-moves-back.ogg"));
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_OGG, file, 0, sys_file_size(file), 0, 1));
    sys_file_close(file);
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);
    for (int32_t pos = 0; pos < len; pos += lenof(pcm)) {
        const int toread = ubound(len - pos, lenof(pcm));
        CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, toread));
    }
    CHECK_FALSE(sound_decode_get_pcm(decoder, pcm, 1));
    /* Make sure an appropriate warning is output as well. */
    CHECK_TRUE(strstr(test_DLOG_last_message, "corrupt"));

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_decode_error)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square.ogg"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_OGG, file, 0, sys_file_size(file), 1, 1));
    sys_file_close(file);
    sound_decode_set_loop_points(decoder, 2, 3);
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);

    int16_t pcm[1];

    /* Seek back a byte so libnogg fails to detect the start of a page. */
    struct {  // Copied from SoundDecodePrivate in src/sound/decode-ogg.c.
        struct vorbis_t *vorbis;
        int filepos;
        uint8_t error;
    } *private = (void *)decoder->private;
    ASSERT(private->filepos > 0);
    const int saved_filepos = private->filepos;
    private->filepos--;
    CHECK_FALSE(sound_decode_get_pcm(decoder, pcm, 1));
    /* Once the stream hits a hard error, it should not try to decode any
     * more data. */
    private->filepos = saved_filepos;
    CHECK_FALSE(sound_decode_get_pcm(decoder, pcm, 1));

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_decode_loop_seek_error)
{
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square.ogg"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_OGG, file, 0, sys_file_size(file), 1, 1));
    sys_file_close(file);
    sound_decode_set_loop_points(decoder, 2, 3);
    CHECK_FALSE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 4000);

    int16_t pcm[39];

    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 4));
    CHECK_INTEQUAL(pcm[0], mono_pcm[0]);
    CHECK_INTEQUAL(pcm[1], mono_pcm[1]);
    CHECK_INTEQUAL(pcm[2], mono_pcm[2]);
    CHECK_INTEQUAL(pcm[3], mono_pcm[3]);
    /* Fail on end-of-loop seek. */
    sound_decode_ogg_test_fail_next_read();
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 1));
    CHECK_INTEQUAL(pcm[0], mono_pcm[4]);
    CHECK_FALSE(sound_decode_get_pcm(decoder, pcm, 1));
    sound_decode_close(decoder);

    /* Fail on loop seek when the loop endpoint is past the end of the file. */
    ASSERT(file = wrap_sys_file_open("testdata/sound/square.ogg"));
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_OGG, file, 0, sys_file_size(file), 1, 1));
    sys_file_close(file);
    sound_decode_set_loop_points(decoder, 2, 40);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 39));
    sound_decode_ogg_test_fail_next_read();
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 1));
    CHECK_FALSE(sound_decode_get_pcm(decoder, pcm, 1));
    sound_decode_close(decoder);

    return 1;
}

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SOUND_INCLUDE_OGG
