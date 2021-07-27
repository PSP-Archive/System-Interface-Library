/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/psp/sound-mp3.c: Tests for MP3 audio decoding on the PSP.
 */

#include "src/base.h"
#include "src/sound.h"
#include "src/sound/decode.h"
#include "src/sysdep.h"
#include "src/sysdep/psp/internal.h"
#include "src/sysdep/psp/sound-mp3.h"
#include "src/sysdep/psp/thread.h"
#include "src/test/base.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Expected PCM output from the MP3 test files.  This is generally equal
 * to output from PC decoding software such as LAME, except that the PSP
 * appears to truncate rather than round fractional results. */

static const int16_t mono_pcm_head[] = {
    10758, 9631, -10212, -9668, 10111, 9290,
};
static const int16_t mono_pcm_tail[] = {
    9828, -9878, -10040,
};

static const int16_t monocbr_pcm_head[] = {
    9531, 7129, -8783, -9016, 8063, 7930,
};
static const int16_t monocbr_pcm_mid[] = {  // Samples 600 through 605.
    9376, 9453, -9377, -9455, 9379, 9458,
};
static const int16_t monocbr_pcm_tail[] = {
    8025, -8493, -8800,
};

static const int16_t mono16_pcm_head[] = {
    9881, 9739, 10561, 9609, -9828, -10348,
};
static const int16_t mono16_pcm_tail[] = {
    10118, 9891, -9917, -10044, -9974, -9989,
};

static const int16_t mono32_pcm_head[] = {
    10385, 9471, 10623, 9565, -9795, -9781,
};
static const int16_t mono32_pcm_tail[] = {
    10040, 9975, -9977, -10010, -10003, -9993,
};

static const int16_t stereo_pcm_head[] = {
    10758, 6605, 9631, 5772, -10212, -6287,
    -9668, -5719, 10111, 6335, 9290, 5544,
};
static const int16_t stereo_pcm_tail[] = {
    9812, 5913, -9962, -5920, -9983, -6029
};

/*-----------------------------------------------------------------------*/

/**
 * CHECK_SAMPLE_NEAR:  Check that a PCM sample is near a target value.
 *
 * [Parameters]
 *     index: Sample index (for error messages).
 *     sample: Sample value.
 *     target: Expected value of sample.
 */
#define CHECK_SAMPLE_NEAR(index,sample,target)  do {            \
    const int _index = (index);                                 \
    const int16_t _sample = (sample);                           \
    const int16_t _target = (target);                           \
    if (abs(_sample - _target) > abs(_target)/8) {              \
        FAIL("Sample %d was %d but should have been near %d",   \
             _index, _sample, _target);                         \
    }                                                           \
} while (0)

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_psp_sound_mp3)

TEST_INIT(init)
{
    CHECK_TRUE(sys_file_init());
    sound_decode_set_handler(SOUND_FORMAT_MP3, psp_decode_mp3_open);
    return 1;
}

TEST_CLEANUP(cleanup)
{
    sound_decode_set_handler(SOUND_FORMAT_MP3, NULL);
    psp_clean_mp3_garbage(1);
    sys_file_cleanup();
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_decode)
{
    SysFile *file;
    ASSERT(file = sys_file_open("testdata/sound/square-8k.mp3"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_MP3, file, 0, sys_file_size(file), 0, 0));
    sys_file_close(file);
    /* This is a mono file, but the PSP decoder always outputs in stereo. */
    CHECK_TRUE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 8000);

    int16_t pcm[200];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 3));
    CHECK_INTEQUAL(pcm[0], mono_pcm_head[0]);
    CHECK_INTEQUAL(pcm[1], mono_pcm_head[0]);
    CHECK_INTEQUAL(pcm[2], mono_pcm_head[1]);
    CHECK_INTEQUAL(pcm[3], mono_pcm_head[1]);
    CHECK_INTEQUAL(pcm[4], mono_pcm_head[2]);
    CHECK_INTEQUAL(pcm[5], mono_pcm_head[2]);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 3));
    CHECK_INTEQUAL(pcm[0], mono_pcm_head[3]);
    CHECK_INTEQUAL(pcm[1], mono_pcm_head[3]);
    CHECK_INTEQUAL(pcm[2], mono_pcm_head[4]);
    CHECK_INTEQUAL(pcm[3], mono_pcm_head[4]);
    CHECK_INTEQUAL(pcm[4], mono_pcm_head[5]);
    CHECK_INTEQUAL(pcm[5], mono_pcm_head[5]);
    for (int i = 0; i < 119; i++) {
        CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 100));
        for (int j = 0; j < 100; j++) {
            CHECK_SAMPLE_NEAR(i*100+j+6, pcm[j*2],
                              (j+6)%4 < 2 ? 10000 : -10000);
            CHECK_INTEQUAL(pcm[j*2+1], pcm[j*2]);
        }
    }
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 95));
    CHECK_INTEQUAL(pcm[182], mono_pcm_tail[0]);
    CHECK_INTEQUAL(pcm[183], mono_pcm_tail[0]);
    CHECK_INTEQUAL(pcm[184], mono_pcm_tail[1]);
    CHECK_INTEQUAL(pcm[185], mono_pcm_tail[1]);
    CHECK_INTEQUAL(pcm[186], mono_pcm_tail[2]);
    CHECK_INTEQUAL(pcm[187], mono_pcm_tail[2]);
    CHECK_INTEQUAL(pcm[188], 0);
    CHECK_INTEQUAL(pcm[189], 0);
    CHECK_FALSE(sound_decode_get_pcm(decoder, pcm, 1));

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_decode_cbr)
{
    SysFile *file;
    ASSERT(file = sys_file_open("testdata/sound/square-8k-cbr.mp3"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_MP3, file, 0, sys_file_size(file), 0, 0));
    sys_file_close(file);
    CHECK_TRUE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 8000);

    int16_t pcm[200];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 100));
    CHECK_INTEQUAL(pcm[0], monocbr_pcm_head[0]);
    CHECK_INTEQUAL(pcm[1], monocbr_pcm_head[0]);
    CHECK_INTEQUAL(pcm[2], monocbr_pcm_head[1]);
    CHECK_INTEQUAL(pcm[3], monocbr_pcm_head[1]);
    CHECK_INTEQUAL(pcm[4], monocbr_pcm_head[2]);
    CHECK_INTEQUAL(pcm[5], monocbr_pcm_head[2]);
    CHECK_INTEQUAL(pcm[6], monocbr_pcm_head[3]);
    CHECK_INTEQUAL(pcm[7], monocbr_pcm_head[3]);
    CHECK_INTEQUAL(pcm[8], monocbr_pcm_head[4]);
    CHECK_INTEQUAL(pcm[9], monocbr_pcm_head[4]);
    CHECK_INTEQUAL(pcm[10], monocbr_pcm_head[5]);
    CHECK_INTEQUAL(pcm[11], monocbr_pcm_head[5]);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 100));
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 100));
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 100));
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 100));
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 100));
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 100));
    CHECK_INTEQUAL(pcm[0], monocbr_pcm_mid[0]);
    CHECK_INTEQUAL(pcm[1], monocbr_pcm_mid[0]);
    CHECK_INTEQUAL(pcm[2], monocbr_pcm_mid[1]);
    CHECK_INTEQUAL(pcm[3], monocbr_pcm_mid[1]);
    CHECK_INTEQUAL(pcm[4], monocbr_pcm_mid[2]);
    CHECK_INTEQUAL(pcm[5], monocbr_pcm_mid[2]);
    CHECK_INTEQUAL(pcm[6], monocbr_pcm_mid[3]);
    CHECK_INTEQUAL(pcm[7], monocbr_pcm_mid[3]);
    CHECK_INTEQUAL(pcm[8], monocbr_pcm_mid[4]);
    CHECK_INTEQUAL(pcm[9], monocbr_pcm_mid[4]);
    CHECK_INTEQUAL(pcm[10], monocbr_pcm_mid[5]);
    CHECK_INTEQUAL(pcm[11], monocbr_pcm_mid[5]);
    for (int i = 7; i < 119; i++) {
        CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 100));
        for (int j = 0; j < 100; j++) {
            CHECK_SAMPLE_NEAR(i*100+j, pcm[j*2], j%4 < 2 ? 10000 : -10000);
            CHECK_INTEQUAL(pcm[j*2+1], pcm[j*2]);
        }
    }
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 100));
    CHECK_INTEQUAL(pcm[194], monocbr_pcm_tail[0]);
    CHECK_INTEQUAL(pcm[195], monocbr_pcm_tail[0]);
    CHECK_INTEQUAL(pcm[196], monocbr_pcm_tail[1]);
    CHECK_INTEQUAL(pcm[197], monocbr_pcm_tail[1]);
    CHECK_INTEQUAL(pcm[198], monocbr_pcm_tail[2]);
    CHECK_INTEQUAL(pcm[199], monocbr_pcm_tail[2]);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 100));
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 42));
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 1));
    CHECK_FALSE(sound_decode_get_pcm(decoder, pcm, 1));

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_decode_16k)
{
    SysFile *file;
    ASSERT(file = sys_file_open("testdata/sound/square-16k.mp3"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_MP3, file, 0, sys_file_size(file), 0, 0));
    sys_file_close(file);
    CHECK_TRUE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 16000);

    int16_t pcm[400];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 200));
    CHECK_INTEQUAL(pcm[0], mono16_pcm_head[0]);
    CHECK_INTEQUAL(pcm[1], mono16_pcm_head[0]);
    CHECK_INTEQUAL(pcm[2], mono16_pcm_head[1]);
    CHECK_INTEQUAL(pcm[3], mono16_pcm_head[1]);
    CHECK_INTEQUAL(pcm[4], mono16_pcm_head[2]);
    CHECK_INTEQUAL(pcm[5], mono16_pcm_head[2]);
    CHECK_INTEQUAL(pcm[6], mono16_pcm_head[3]);
    CHECK_INTEQUAL(pcm[7], mono16_pcm_head[3]);
    CHECK_INTEQUAL(pcm[8], mono16_pcm_head[4]);
    CHECK_INTEQUAL(pcm[9], mono16_pcm_head[4]);
    CHECK_INTEQUAL(pcm[10], mono16_pcm_head[5]);
    CHECK_INTEQUAL(pcm[11], mono16_pcm_head[5]);
    for (int i = 1; i < 59; i++) {
        CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 200));
        for (int j = 0; j < 200; j++) {
            CHECK_SAMPLE_NEAR(i*200+j, pcm[j*2], j%8 < 4 ? 10000 : -10000);
            CHECK_INTEQUAL(pcm[j*2+1], pcm[j*2]);
        }
    }
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 200));
    CHECK_INTEQUAL(pcm[388], mono16_pcm_tail[0]);
    CHECK_INTEQUAL(pcm[389], mono16_pcm_tail[0]);
    CHECK_INTEQUAL(pcm[390], mono16_pcm_tail[1]);
    CHECK_INTEQUAL(pcm[391], mono16_pcm_tail[1]);
    CHECK_INTEQUAL(pcm[392], mono16_pcm_tail[2]);
    CHECK_INTEQUAL(pcm[393], mono16_pcm_tail[2]);
    CHECK_INTEQUAL(pcm[394], mono16_pcm_tail[3]);
    CHECK_INTEQUAL(pcm[395], mono16_pcm_tail[3]);
    CHECK_INTEQUAL(pcm[396], mono16_pcm_tail[4]);
    CHECK_INTEQUAL(pcm[397], mono16_pcm_tail[4]);
    CHECK_INTEQUAL(pcm[398], mono16_pcm_tail[5]);
    CHECK_INTEQUAL(pcm[399], mono16_pcm_tail[5]);
    CHECK_FALSE(sound_decode_get_pcm(decoder, pcm, 1));

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_decode_32k)
{
    SysFile *file;
    ASSERT(file = sys_file_open("testdata/sound/square-32k.mp3"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_MP3, file, 0, sys_file_size(file), 0, 0));
    sys_file_close(file);
    CHECK_TRUE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 32000);

    int16_t pcm[400];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 200));
    CHECK_INTEQUAL(pcm[0], mono32_pcm_head[0]);
    CHECK_INTEQUAL(pcm[1], mono32_pcm_head[0]);
    CHECK_INTEQUAL(pcm[2], mono32_pcm_head[1]);
    CHECK_INTEQUAL(pcm[3], mono32_pcm_head[1]);
    CHECK_INTEQUAL(pcm[4], mono32_pcm_head[2]);
    CHECK_INTEQUAL(pcm[5], mono32_pcm_head[2]);
    CHECK_INTEQUAL(pcm[6], mono32_pcm_head[3]);
    CHECK_INTEQUAL(pcm[7], mono32_pcm_head[3]);
    CHECK_INTEQUAL(pcm[8], mono32_pcm_head[4]);
    CHECK_INTEQUAL(pcm[9], mono32_pcm_head[4]);
    CHECK_INTEQUAL(pcm[10], mono32_pcm_head[5]);
    CHECK_INTEQUAL(pcm[11], mono32_pcm_head[5]);
    for (int i = 1; i < 59; i++) {
        CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 200));
        for (int j = 0; j < 200; j++) {
            CHECK_SAMPLE_NEAR(i*200+j, pcm[j*2], j%8 < 4 ? 10000 : -10000);
            CHECK_INTEQUAL(pcm[j*2+1], pcm[j*2]);
        }
    }
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 200));
    CHECK_INTEQUAL(pcm[388], mono32_pcm_tail[0]);
    CHECK_INTEQUAL(pcm[389], mono32_pcm_tail[0]);
    CHECK_INTEQUAL(pcm[390], mono32_pcm_tail[1]);
    CHECK_INTEQUAL(pcm[391], mono32_pcm_tail[1]);
    CHECK_INTEQUAL(pcm[392], mono32_pcm_tail[2]);
    CHECK_INTEQUAL(pcm[393], mono32_pcm_tail[2]);
    CHECK_INTEQUAL(pcm[394], mono32_pcm_tail[3]);
    CHECK_INTEQUAL(pcm[395], mono32_pcm_tail[3]);
    CHECK_INTEQUAL(pcm[396], mono32_pcm_tail[4]);
    CHECK_INTEQUAL(pcm[397], mono32_pcm_tail[4]);
    CHECK_INTEQUAL(pcm[398], mono32_pcm_tail[5]);
    CHECK_INTEQUAL(pcm[399], mono32_pcm_tail[5]);
    CHECK_FALSE(sound_decode_get_pcm(decoder, pcm, 1));

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_decode_memory_failure)
{
    SysFile *file;
    ASSERT(file = sys_file_open("testdata/sound/square-8k.mp3"));
    const int64_t datalen = sys_file_size(file);
    void *data;
    ASSERT(data = mem_alloc(datalen, 0, 0));
    ASSERT(sys_file_read(file, data, datalen) == datalen);
    sys_file_close(file);

    SoundDecodeHandle *decoder;
    CHECK_MEMORY_FAILURES(
        decoder = sound_decode_open(SOUND_FORMAT_MP3, data, datalen, 0, 0));
    int16_t pcm[6];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 3));
    CHECK_INTEQUAL(pcm[0], mono_pcm_head[0]);
    CHECK_INTEQUAL(pcm[1], mono_pcm_head[0]);
    CHECK_INTEQUAL(pcm[2], mono_pcm_head[1]);
    CHECK_INTEQUAL(pcm[3], mono_pcm_head[1]);
    CHECK_INTEQUAL(pcm[4], mono_pcm_head[2]);
    CHECK_INTEQUAL(pcm[5], mono_pcm_head[2]);
    sound_decode_close(decoder);

    mem_free(data);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_decode_stereo)
{
    SysFile *file;
    ASSERT(file = sys_file_open("testdata/sound/squares.mp3"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_MP3, file, 0, sys_file_size(file), 0, 0));
    sys_file_close(file);
    CHECK_TRUE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 8000);

    int16_t pcm[200];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 3));
    CHECK_INTEQUAL(pcm[0], stereo_pcm_head[0]);
    CHECK_INTEQUAL(pcm[1], stereo_pcm_head[1]);
    CHECK_INTEQUAL(pcm[2], stereo_pcm_head[2]);
    CHECK_INTEQUAL(pcm[3], stereo_pcm_head[3]);
    CHECK_INTEQUAL(pcm[4], stereo_pcm_head[4]);
    CHECK_INTEQUAL(pcm[5], stereo_pcm_head[5]);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 3));
    CHECK_INTEQUAL(pcm[0], stereo_pcm_head[6]);
    CHECK_INTEQUAL(pcm[1], stereo_pcm_head[7]);
    CHECK_INTEQUAL(pcm[2], stereo_pcm_head[8]);
    CHECK_INTEQUAL(pcm[3], stereo_pcm_head[9]);
    CHECK_INTEQUAL(pcm[4], stereo_pcm_head[10]);
    CHECK_INTEQUAL(pcm[5], stereo_pcm_head[11]);
    for (int i = 0; i < 59; i++) {
        CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 100));
        for (int j = 0; j < 100; j++) {
            CHECK_SAMPLE_NEAR(i*100+j+3, pcm[j*2+0],
                              (j+6)%4 < 2 ? 10000 : -10000);
            CHECK_SAMPLE_NEAR(i*100+j+3, pcm[j*2+1],
                              (j+6)%4 < 2 ? 6000 : -6000);
        }
    }
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 95));
    CHECK_INTEQUAL(pcm[182], stereo_pcm_tail[0]);
    CHECK_INTEQUAL(pcm[183], stereo_pcm_tail[1]);
    CHECK_INTEQUAL(pcm[184], stereo_pcm_tail[2]);
    CHECK_INTEQUAL(pcm[185], stereo_pcm_tail[3]);
    CHECK_INTEQUAL(pcm[186], stereo_pcm_tail[4]);
    CHECK_INTEQUAL(pcm[187], stereo_pcm_tail[5]);
    CHECK_INTEQUAL(pcm[188], 0);
    CHECK_INTEQUAL(pcm[189], 0);
    CHECK_FALSE(sound_decode_get_pcm(decoder, pcm, 1));

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_loop)
{
    SysFile *file;
    ASSERT(file = sys_file_open("testdata/sound/square-8k.mp3"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_MP3, file, 0, sys_file_size(file), 1, 1));
    sys_file_close(file);
    sound_decode_set_loop_points(decoder, 8001, 2002);
    CHECK_TRUE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 8000);

    int16_t pcm[200];
    for (int i = 0; i < 100; i++) {
        CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 100));
    }
    CHECK_FLOATRANGE(sound_decode_get_position(decoder),
                                               9999.5f/8000, 10000.5f/8000);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 5));
    CHECK_INTEQUAL(pcm[0], 10001);
    CHECK_INTEQUAL(pcm[1], 10001);
    CHECK_INTEQUAL(pcm[2], 9998);
    CHECK_INTEQUAL(pcm[3], 9998);
    CHECK_INTEQUAL(pcm[4], -10001);
    CHECK_INTEQUAL(pcm[5], -10001);
    CHECK_INTEQUAL(pcm[6], 9998);
    CHECK_INTEQUAL(pcm[7], 9998);
    CHECK_INTEQUAL(pcm[8], -10001);
    CHECK_INTEQUAL(pcm[9], -10001);
    CHECK_FLOATRANGE(sound_decode_get_position(decoder),
                                               8002.5f/8000, 8003.5f/8000);

    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 97));
    for (int i = 1; i < 20; i++) {
        CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 100));
    }
    CHECK_FLOATRANGE(sound_decode_get_position(decoder),
                                               9999.5f/8000, 10000.5f/8000);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 5));
    CHECK_INTEQUAL(pcm[0], 10001);
    CHECK_INTEQUAL(pcm[1], 10001);
    CHECK_INTEQUAL(pcm[2], 9998);
    CHECK_INTEQUAL(pcm[3], 9998);
    CHECK_INTEQUAL(pcm[4], -10001);
    CHECK_INTEQUAL(pcm[5], -10001);
    CHECK_INTEQUAL(pcm[6], 9998);
    CHECK_INTEQUAL(pcm[7], 9998);
    CHECK_INTEQUAL(pcm[8], -10001);
    CHECK_INTEQUAL(pcm[9], -10001);
    CHECK_FLOATRANGE(sound_decode_get_position(decoder),
                                               8002.5f/8000, 8003.5f/8000);

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_loop_too_short)
{
    SysFile *file;
    ASSERT(file = sys_file_open("testdata/sound/square-8k.mp3"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_MP3, file, 0, sys_file_size(file), 1, 1));
    sys_file_close(file);
    /* This should cause the loop to be disabled when the file is played. */
    sound_decode_set_loop_points(decoder, 2, 3);
    CHECK_TRUE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 8000);

    int16_t pcm[202];
    for (int i = 0; i < 119; i++) {
        CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 100));
    }
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 101));
    CHECK_INTEQUAL(pcm[194], mono_pcm_tail[0]);
    CHECK_INTEQUAL(pcm[195], mono_pcm_tail[0]);
    CHECK_INTEQUAL(pcm[196], mono_pcm_tail[1]);
    CHECK_INTEQUAL(pcm[197], mono_pcm_tail[1]);
    CHECK_INTEQUAL(pcm[198], mono_pcm_tail[2]);
    CHECK_INTEQUAL(pcm[199], mono_pcm_tail[2]);
    CHECK_INTEQUAL(pcm[200], 0);
    CHECK_INTEQUAL(pcm[201], 0);
    CHECK_FALSE(sound_decode_get_pcm(decoder, pcm, 1));

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_loop_early_start)
{
    SysFile *file;
    ASSERT(file = sys_file_open("testdata/sound/square-8k.mp3"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_MP3, file, 0, sys_file_size(file), 1, 1));
    sys_file_close(file);
    sound_decode_set_loop_points(decoder, 2, 2001);
    CHECK_TRUE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 8000);

    int16_t pcm[4012];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 2006));
    CHECK_INTEQUAL(pcm[0], mono_pcm_head[0]);
    CHECK_INTEQUAL(pcm[2], mono_pcm_head[1]);
    CHECK_INTEQUAL(pcm[4], mono_pcm_head[2]);
    CHECK_INTEQUAL(pcm[6], mono_pcm_head[3]);
    CHECK_INTEQUAL(pcm[8], mono_pcm_head[4]);
    CHECK_INTEQUAL(pcm[10], mono_pcm_head[5]);
    CHECK_INTEQUAL(pcm[4006], mono_pcm_head[2]);
    CHECK_INTEQUAL(pcm[4008], mono_pcm_head[3]);
    CHECK_INTEQUAL(pcm[4010], mono_pcm_head[4]);

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_loop_to_end)
{
    SysFile *file;
    ASSERT(file = sys_file_open("testdata/sound/square-8k.mp3"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_MP3, file, 0, sys_file_size(file), 1, 1));
    sys_file_close(file);
    sound_decode_set_loop_points(decoder, 2, 0);
    CHECK_TRUE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 8000);

    int16_t pcm[200];
    for (int i = 0; i < 119; i++) {
        CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 100));
    }
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 97));
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 5));
    CHECK_INTEQUAL(pcm[0], mono_pcm_tail[0]);
    CHECK_INTEQUAL(pcm[2], mono_pcm_tail[1]);
    CHECK_INTEQUAL(pcm[4], mono_pcm_tail[2]);
    CHECK_INTEQUAL(pcm[6], mono_pcm_head[2]);
    CHECK_INTEQUAL(pcm[8], mono_pcm_head[3]);

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_loop_past_end)
{
    SysFile *file;
    ASSERT(file = sys_file_open("testdata/sound/square-8k.mp3"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_MP3, file, 0, sys_file_size(file), 1, 1));
    sys_file_close(file);
    sound_decode_set_loop_points(decoder, 2, 13000);
    CHECK_TRUE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 8000);

    int16_t pcm[200];
    for (int i = 0; i < 119; i++) {
        CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 100));
    }
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 97));
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 5));
    CHECK_INTEQUAL(pcm[0], mono_pcm_tail[0]);
    CHECK_INTEQUAL(pcm[2], mono_pcm_tail[1]);
    CHECK_INTEQUAL(pcm[4], mono_pcm_tail[2]);
    CHECK_INTEQUAL(pcm[6], mono_pcm_head[2]);
    CHECK_INTEQUAL(pcm[8], mono_pcm_head[3]);

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_loop_starts_at_end)
{
    SysFile *file;
    ASSERT(file = sys_file_open("testdata/sound/square-8k.mp3"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_MP3, file, 0, sys_file_size(file), 1, 1));
    sys_file_close(file);
    sound_decode_set_loop_points(decoder, 12000, 1000);
    CHECK_TRUE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 8000);

    int16_t pcm[200];
    for (int i = 0; i < 120; i++) {
        CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 100));
    }
    CHECK_INTEQUAL(pcm[194], mono_pcm_tail[0]);
    CHECK_INTEQUAL(pcm[196], mono_pcm_tail[1]);
    CHECK_INTEQUAL(pcm[198], mono_pcm_tail[2]);
    CHECK_FALSE(sound_decode_get_pcm(decoder, pcm, 1));

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_loop_no_xing)
{
    SysFile *file;
    ASSERT(file = sys_file_open("testdata/sound/square-8k-noxing.mp3"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_MP3, file, 0, sys_file_size(file), 1, 1));
    sys_file_close(file);
    sound_decode_set_loop_points(decoder, 8001, 2002);
    CHECK_TRUE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 8000);

    int16_t pcm[200];
    for (int i = 0; i < 100; i++) {
        CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 100));
    }
    CHECK_FLOATRANGE(sound_decode_get_position(decoder),
                                               9999.5f/8000, 10000.5f/8000);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 5));
    CHECK_INTEQUAL(pcm[0], 10001);
    CHECK_INTEQUAL(pcm[1], 10001);
    CHECK_INTEQUAL(pcm[2], 9998);
    CHECK_INTEQUAL(pcm[3], 9998);
    CHECK_INTEQUAL(pcm[4], -10001);
    CHECK_INTEQUAL(pcm[5], -10001);
    CHECK_INTEQUAL(pcm[6], 9998);
    CHECK_INTEQUAL(pcm[7], 9998);
    CHECK_INTEQUAL(pcm[8], -10001);
    CHECK_INTEQUAL(pcm[9], -10001);
    CHECK_FLOATRANGE(sound_decode_get_position(decoder),
                                               8002.5f/8000, 8003.5f/8000);

    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 97));
    for (int i = 1; i < 20; i++) {
        CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 100));
    }
    CHECK_FLOATRANGE(sound_decode_get_position(decoder),
                                               9999.5f/8000, 10000.5f/8000);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 5));
    CHECK_INTEQUAL(pcm[0], 10001);
    CHECK_INTEQUAL(pcm[1], 10001);
    CHECK_INTEQUAL(pcm[2], 9998);
    CHECK_INTEQUAL(pcm[3], 9998);
    CHECK_INTEQUAL(pcm[4], -10001);
    CHECK_INTEQUAL(pcm[5], -10001);
    CHECK_INTEQUAL(pcm[6], 9998);
    CHECK_INTEQUAL(pcm[7], 9998);
    CHECK_INTEQUAL(pcm[8], -10001);
    CHECK_INTEQUAL(pcm[9], -10001);
    CHECK_FLOATRANGE(sound_decode_get_position(decoder),
                                               8002.5f/8000, 8003.5f/8000);

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_move_loop_start_forward)
{
    SysFile *file;
    ASSERT(file = sys_file_open("testdata/sound/square-8k.mp3"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_MP3, file, 0, sys_file_size(file), 1, 1));
    sys_file_close(file);
    CHECK_TRUE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 8000);

    int16_t pcm[200];
    /* Read enough data to get past the new loop start point, but not so
     * much that the decoder reads up to the end of the stream. */
    for (int i = 0; i < 90; i++) {
        CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 100));
    }

    sound_decode_set_loop_points(decoder, 8001, 12000);

    for (int i = 0; i < 29; i++) {
        CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 100));
    }
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 97));
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 5));
    CHECK_INTEQUAL(pcm[0], mono_pcm_tail[0]);
    CHECK_INTEQUAL(pcm[2], mono_pcm_tail[1]);
    CHECK_INTEQUAL(pcm[4], mono_pcm_tail[2]);
    CHECK_INTEQUAL(pcm[6], 9998);
    CHECK_INTEQUAL(pcm[8], -10001);

    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 97));
    for (int i = 1; i < 39; i++) {
        CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 100));
    }
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 97));
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 5));
    CHECK_INTEQUAL(pcm[0], mono_pcm_tail[0]);
    CHECK_INTEQUAL(pcm[2], mono_pcm_tail[1]);
    CHECK_INTEQUAL(pcm[4], mono_pcm_tail[2]);
    CHECK_INTEQUAL(pcm[6], 9998);
    CHECK_INTEQUAL(pcm[8], -10001);

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_move_loop_start_backward)
{
    SysFile *file;
    ASSERT(file = sys_file_open("testdata/sound/square-8k.mp3"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_MP3, file, 0, sys_file_size(file), 1, 1));
    sys_file_close(file);
    sound_decode_set_loop_points(decoder, 8001, 12000);
    CHECK_TRUE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 8000);

    int16_t pcm[200];
    /* Read enough data to get past the current loop start point, but not
     * so much that the decoder reads up to the end of the stream. */
    for (int i = 0; i < 90; i++) {
        CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 100));
    }

    sound_decode_set_loop_points(decoder, 1001, 12000);

    for (int i = 0; i < 29; i++) {
        CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 100));
    }
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 97));
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 5));
    CHECK_INTEQUAL(pcm[0], mono_pcm_tail[0]);
    CHECK_INTEQUAL(pcm[2], mono_pcm_tail[1]);
    CHECK_INTEQUAL(pcm[4], mono_pcm_tail[2]);
    CHECK_INTEQUAL(pcm[6], 10004);
    CHECK_INTEQUAL(pcm[8], -9998);

    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 97));
    for (int i = 1; i < 109; i++) {
        CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 100));
    }
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 97));
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 5));
    CHECK_INTEQUAL(pcm[0], mono_pcm_tail[0]);
    CHECK_INTEQUAL(pcm[2], mono_pcm_tail[1]);
    CHECK_INTEQUAL(pcm[4], mono_pcm_tail[2]);
    CHECK_INTEQUAL(pcm[6], 10004);
    CHECK_INTEQUAL(pcm[8], -9998);

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_enable_loop)
{
    SysFile *file;
    ASSERT(file = sys_file_open("testdata/sound/square-8k.mp3"));
    SoundDecodeHandle *decoder;
    /* The PSP MP3 decoder runs in the background, so we can't toggle the
     * loop flag on a sample-by-sample basis, and consequently we can't
     * easily check that turning on the loop flag works.  We assume it's
     * okay if everything else passes. */
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_MP3, file, 0, sys_file_size(file), 1, 1));
    sys_file_close(file);
    sound_decode_set_loop_points(decoder, 2, 2001);
    CHECK_TRUE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 8000);

    int16_t pcm[4012];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 1));
    CHECK_INTEQUAL(pcm[0], mono_pcm_head[0]);

    /* We should be able to enable the loop before we hit its endpoint. */
    sound_decode_enable_loop(decoder, 1);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 2005));
    CHECK_INTEQUAL(pcm[0], mono_pcm_head[1]);
    CHECK_INTEQUAL(pcm[2], mono_pcm_head[2]);
    CHECK_INTEQUAL(pcm[4], mono_pcm_head[3]);
    CHECK_INTEQUAL(pcm[6], mono_pcm_head[4]);
    CHECK_INTEQUAL(pcm[8], mono_pcm_head[5]);
    CHECK_INTEQUAL(pcm[4004], mono_pcm_head[2]);
    CHECK_INTEQUAL(pcm[4006], mono_pcm_head[3]);
    CHECK_INTEQUAL(pcm[4008], mono_pcm_head[4]);

    /* We should be able to disable the loop and play past its endpoint.
     * For the PSP, we don't know how many loops will be decoded until the
     * loop flag change is detected, so we repeatedly read one loop's worth
     * of samples and wait for the tail end of the buffer to change. */
    sound_decode_enable_loop(decoder, 0);
    do {
        CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 2000));
        CHECK_INTEQUAL(pcm[0], mono_pcm_head[5]);
    } while (pcm[3998] == mono_pcm_head[4]);

    /* Attempting to enable the loop when past its endpoint should not
     * cause an immediate loop. */
    sound_decode_enable_loop(decoder, 1);
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 95));
    for (int i = 21; i < 120; i++) {
        CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 100));
    }
    CHECK_INTEQUAL(pcm[194], mono_pcm_tail[0]);
    CHECK_INTEQUAL(pcm[196], mono_pcm_tail[1]);
    CHECK_INTEQUAL(pcm[198], mono_pcm_tail[2]);
    CHECK_FALSE(sound_decode_get_pcm(decoder, pcm, 1));

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_decode_thread_buffers_full)
{
    SysFile *file;
    ASSERT(file = sys_file_open("testdata/sound/square-8k.mp3"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_MP3, file, 0, sys_file_size(file), 0, 0));
    sys_file_close(file);
    CHECK_TRUE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 8000);

    sceKernelDelayThread(30000);

    int16_t pcm[200];
    for (int i = 0; i < 120; i++) {
        CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 100));
    }
    CHECK_INTEQUAL(pcm[194], mono_pcm_tail[0]);
    CHECK_INTEQUAL(pcm[196], mono_pcm_tail[1]);
    CHECK_INTEQUAL(pcm[198], mono_pcm_tail[2]);
    CHECK_FALSE(sound_decode_get_pcm(decoder, pcm, 1));

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_close_when_decode_thread_buffers_full)
{
    SysFile *file;
    ASSERT(file = sys_file_open("testdata/sound/square-8k.mp3"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_MP3, file, 0, sys_file_size(file), 0, 0));
    sys_file_close(file);
    CHECK_TRUE(sound_decode_is_stereo(decoder));
    CHECK_INTEQUAL(sound_decode_native_freq(decoder), 8000);

    sceKernelDelayThread(30000);

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_invalid_format)
{
    static const char * const files[] = {
        "testdata/sound/bad/empty-file.mp3",
        "testdata/sound/bad/short-header.mp3",
        "testdata/sound/bad/bad-sync.mp3",
        "testdata/sound/bad/bad-version.mp3",
        "testdata/sound/bad/bad-layer.mp3",
        "testdata/sound/bad/bad-freq.mp3",
        "testdata/sound/bad/bad-bitrate.mp3",
        "testdata/sound/bad/free-bitrate.mp3",
    };
    for (int i = 0; i < lenof(files); i++) {
        SysFile *file;
        if (!(file = sys_file_open(files[i]))) {
            FAIL("sys_file_open(%s) failed: %s", files[i], sys_last_errstr());
        }
        if (sound_decode_open_from_file(
                SOUND_FORMAT_MP3, file, 0, sys_file_size(file), 0, 0)) {
            FAIL("sound_decode_open_from_file() for %s was not false as"
                 " expected", files[i]);
        }
        sys_file_close(file);
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_bad_xing_header)
{
    SysFile *file;
    ASSERT(file = sys_file_open("testdata/sound/bad/xing-no-frame-count.mp3"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_MP3, file, 0, sys_file_size(file), 0, 0));
    sys_file_close(file);

    /* The broken Xing header means we'll get an extra frame (576 samples)
     * of junk at the beginning of the stream. */
    int16_t pcm[576*2];
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 576));
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 100));
    CHECK_INTEQUAL(pcm[0], mono_pcm_head[0]);
    CHECK_INTEQUAL(pcm[2], mono_pcm_head[1]);
    CHECK_INTEQUAL(pcm[4], mono_pcm_head[2]);
    CHECK_INTEQUAL(pcm[6], mono_pcm_head[3]);
    CHECK_INTEQUAL(pcm[8], mono_pcm_head[4]);
    CHECK_INTEQUAL(pcm[10], mono_pcm_head[5]);
    for (int i = 1; i < 120; i++) {
        CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 100));
    }
    CHECK_INTEQUAL(pcm[194], mono_pcm_tail[0]);
    CHECK_INTEQUAL(pcm[196], mono_pcm_tail[1]);
    CHECK_INTEQUAL(pcm[198], mono_pcm_tail[2]);
    /* The broken Xing header also means the decoder won't automatically cut
     * off the padding (23*576-1105 = 143 samples) at the end of the stream,
     * so check for it. */
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 142));
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 1));
    CHECK_FALSE(sound_decode_get_pcm(decoder, pcm, 1));

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_bad_xing_padding)
{
    SysFile *file;
    ASSERT(file = sys_file_open("testdata/sound/bad/xing-short-padding.mp3"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_MP3, file, 0, sys_file_size(file), 0, 0));
    sys_file_close(file);

    int16_t pcm[142*2];
    /* We should have 500 samples of initial junk that didn't get skipped. */
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 100));
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 100));
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 100));
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 100));
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 100));
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 100));
    CHECK_INTEQUAL(pcm[0], mono_pcm_head[0]);
    CHECK_INTEQUAL(pcm[2], mono_pcm_head[1]);
    CHECK_INTEQUAL(pcm[4], mono_pcm_head[2]);
    CHECK_INTEQUAL(pcm[6], mono_pcm_head[3]);
    CHECK_INTEQUAL(pcm[8], mono_pcm_head[4]);
    CHECK_INTEQUAL(pcm[10], mono_pcm_head[5]);
    for (int i = 1; i < 120; i++) {
        CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 100));
    }
    CHECK_INTEQUAL(pcm[194], mono_pcm_tail[0]);
    CHECK_INTEQUAL(pcm[196], mono_pcm_tail[1]);
    CHECK_INTEQUAL(pcm[198], mono_pcm_tail[2]);
    /* We'll also have the extra 143 samples of padding at the end. */
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 142));
    CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, 1));
    CHECK_FALSE(sound_decode_get_pcm(decoder, pcm, 1));

    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_bad_data_in_decode)
{
    static const char * const files[] = {
        "testdata/sound/bad/truncated-header.mp3",
        "testdata/sound/bad/truncated-data.mp3",
        "testdata/sound/bad/broken-header.mp3",
        "testdata/sound/bad/broken-data.mp3",
    };
    for (int i = 0; i < lenof(files); i++) {
        DLOG("Testing %s", files[i]);
        SysFile *file;
        ASSERT(file = sys_file_open(files[i]));
        SoundDecodeHandle *decoder;
        CHECK_TRUE(decoder = sound_decode_open_from_file(
                       SOUND_FORMAT_MP3, file, 0, sys_file_size(file), 0, 0));
        sys_file_close(file);
        int16_t pcm[(576*3-1105)*2];
        CHECK_TRUE(sound_decode_get_pcm(decoder, pcm, lenof(pcm)/2));
        if (strcmp(files[i], "testdata/sound/bad/broken-data.mp3") == 0) {
            ASSERT(lenof(pcm) >= 576*2);
            sound_decode_get_pcm(decoder, pcm, 576);
            for (int j = 0; j < 576*2; j++) {
                CHECK_INTEQUAL(pcm[j], 0);
            }
            sound_decode_get_pcm(decoder, pcm, 1);
            for (int j = 12; j < 120; j++) {
                sound_decode_get_pcm(decoder, pcm, 100);
            }
        }
        CHECK_FALSE(sound_decode_get_pcm(decoder, pcm, 1));
        sound_decode_close(decoder);
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_clean_garbage)
{
    SysFile *file;
    ASSERT(file = sys_file_open("testdata/sound/square-8k.mp3"));
    SoundDecodeHandle *decoder1, *decoder2;
    CHECK_TRUE(decoder1 = sound_decode_open_from_file(
                   SOUND_FORMAT_MP3, file, 0, sys_file_size(file), 0, 0));
    CHECK_TRUE(decoder2 = sound_decode_open_from_file(
                   SOUND_FORMAT_MP3, file, 0, sys_file_size(file), 0, 0));
    sys_file_close(file);

    psp_threads_lock();
    {
        sound_decode_close(decoder1);
        /* This call should do nothing, since the decode thread is frozen
         * and won't have had a chance to detect the stop request. */
        psp_clean_mp3_garbage(0);
    }
    psp_threads_unlock();
    /* This call should free the first decoder but leave the second alone. */
    psp_clean_mp3_garbage(1);

    int16_t pcm[200];
    for (int i = 0; i < 120; i++) {
        CHECK_TRUE(sound_decode_get_pcm(decoder2, pcm, 100));
    }
    CHECK_FALSE(sound_decode_get_pcm(decoder2, pcm, 1));
    sound_decode_close(decoder2);
    /* This call should free the second decoder immediately, since we
     * reached the end of the stream. */
    psp_clean_mp3_garbage(0);

    return 1;
}

/*************************************************************************/
/*************************************************************************/
