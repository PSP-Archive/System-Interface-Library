/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sound/core.c: Tests for the sound core.
 */

#include "src/base.h"
#include "src/memory.h"
#include "src/mutex.h"
#include "src/sound.h"
#include "src/sound/decode.h"
#include "src/sound/filter.h"
#include "src/sound/mixer.h"  // For sound_mixer_get_pcm().
#include "src/sysdep.h"
#include "src/sysdep/test.h"
#include "src/test/base.h"
#include "src/thread.h"

#if !defined(SIL_PLATFORM_PSP)
# define USING_IOQUEUE
# include "src/sysdep/misc/ioqueue.h"
#endif

/*************************************************************************/
/***************************** Exported data *****************************/
/*************************************************************************/

/* Flag set by lock_filter() in sound/core.c when blocking on a lock.  We
 * use this to detect whether the lock is working as expected. */
uint8_t sound_core_blocked_on_filter_lock;

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Flag set by the dummy MP3 decoder indicating that the open routine was
 * called. */
static uint8_t mp3_opened = 0;

/* Flag set by the dummy filter while running. */
static uint8_t filter_running = 0;

/* Flag: Should the dummy filter's close function lock the mutex?  (The
 * mutex is kept locked until this flag is set back to false.) */
static uint8_t filter_mutex_lock_on_close = 0;

/* Flag set by the dummy filter's close function while it holds the mutex. */
static uint8_t filter_mutex_locked_by_close = 0;

/* Mutex locked by the dummy filter if nonzero. */
static int filter_mutex = 0;

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

/**
 * dummy_mp3_open:  Stub function used as an MP3 decoder open method, to
 * check that MP3 data was correctly detected as MP3 format.
 *
 * [Parameters]
 *     this: Sound decoder instance.
 * [Return value]
 *     Always false.
 */
static int dummy_mp3_open(UNUSED SoundDecodeHandle *this)
{
    mp3_opened = 1;
    return 0;
}

/*-----------------------------------------------------------------------*/

/**
 * dummy_filter_filter, dummy_filter_close, dummy_filter_open:  Functions
 * implementing a stub filter which returns 10 samples from 100 to 1000 in
 * steps of 100, then fails.  If filter_mutex is nonzero, it is locked on
 * entry to dummy_filter_filter() and unlocked on exit.
 *
 * See sound/filter.h for function details.
 */
static int dummy_filter_filter(SoundFilterHandle *this, int16_t *pcm_buffer,
                               uint32_t pcm_len)
{
    PRECOND(this != NULL, return 0);

    filter_running = 1;
    if (filter_mutex) {
        mutex_lock(filter_mutex);
    }

    uintptr_t samples_out = (uintptr_t)this->private;
    if (samples_out >= 10) {
        return 0;
    }

    uint32_t i = 0;
    while (samples_out < 10 && i < pcm_len) {
        samples_out++;
        pcm_buffer[i++] = samples_out * 100;
    }
    for (; i < pcm_len; i++) {
        pcm_buffer[i] = 0;
    }
    this->private = (SoundFilterPrivate *)samples_out;

    filter_running = 0;
    if (filter_mutex) {
        mutex_unlock(filter_mutex);
    }
    return 1;
}


static void dummy_filter_close(UNUSED SoundFilterHandle *this)
{
    if (filter_mutex_lock_on_close) {
        mutex_lock(filter_mutex);
        filter_mutex_locked_by_close = 1;
        while (filter_mutex_lock_on_close) {
            thread_yield();
        }
        filter_mutex_locked_by_close = 0;
        mutex_unlock(filter_mutex);
    }
}


static SoundFilterHandle *dummy_filter_open(void)
{
    SoundFilterHandle *this;
    ASSERT(this = mem_alloc(sizeof(*this), 0, 0));
    this->filter  = dummy_filter_filter;
    this->close   = dummy_filter_close;
    this->stereo  = 0;  // We don't use this for anything.
    this->freq    = 1;  // We don't use this for anything.
    this->private = (SoundFilterPrivate *)0;  // Output sample counter.
    return this;
}

/*-----------------------------------------------------------------------*/

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

/*-----------------------------------------------------------------------*/

/**
 * load_sound:  Load a file into memory and create a Sound object from it.
 *
 * [Parameters]
 *     path: File pathname.
 *     format: Audio data format (SOUND_FORMAT_*, or 0 to autodetect).
 * [Return value]
 *     Sound object, or NULL on error.
 */
static Sound *load_sound(const char *path, SoundFormat format)
{
    PRECOND(path != NULL, return NULL);

    SysFile *file = wrap_sys_file_open(path);
    if (!file) {
        DLOG("%s: %s", path, sys_last_errstr());
        return NULL;
    }
    const int64_t datalen = sys_file_size(file);
    void *data = mem_alloc(datalen, 0, 0);
    if (!data) {
        DLOG("%s: Out of memory (need %lld bytes)", path, (long long)datalen);
        sys_file_close(file);
        return NULL;
    }
    const int32_t nread = sys_file_read(file, data, datalen);
    sys_file_close(file);
    if (nread != datalen) {
        DLOG("%s: Read error", path);
        mem_free(data);
        return NULL;
    }
    Sound *sound = sound_create(data, datalen, format, 1);
    if (!sound) {
        DLOG("%s: Failed to create Sound object", path);
        return NULL;
    }
    return sound;
}

/*-----------------------------------------------------------------------*/

/**
 * get_pcm:  Retrieve output samples, and update the sound core state.
 *
 * [Parameters]
 *     buffer: Buffer for PCM samples (stereo 16-bit).
 *     num_samples: Number of samples to retrieve.
 */
static void get_pcm(int16_t *buffer, int num_samples)
{
    sound_mixer_get_pcm(buffer, num_samples);
    sound_update();
}

/*-----------------------------------------------------------------------*/

/**
 * get_pcm_thread:  Simple thread routine to call get_pcm() to retrieve one
 * sample.
 *
 * [Parameters]
 *     param: Thread parameter (4-byte buffer for one stereo PCM sample).
 * [Return value]
 *     1
 */
static int get_pcm_thread(void *param)
{
    int16_t *buffer = (int16_t *)param;
    get_pcm(buffer, 1);
    return 1;
}

/*-----------------------------------------------------------------------*/

/**
 * set_filter_thread:  Simple thread routine to call
 * sound_set_filter(dummy_filter_open()) for the given channel.
 *
 * [Parameters]
 *     param: Thread parameter (integer: channel number).
 * [Return value]
 *     1
 */
static int set_filter_thread(void *param)
{
    int channel = (int)(intptr_t)param;
    sound_set_filter(channel, dummy_filter_open());
    return 1;
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_sound_core)

TEST_INIT(init)
{
    CHECK_TRUE(thread_init());
    CHECK_TRUE(sys_file_init());

    sys_test_sound_set_output_rate(4000);
    sound_init();
    CHECK_TRUE(sound_open_device("", 3));

    return 1;
}

TEST_CLEANUP(cleanup)
{
    sound_cleanup();
    sys_test_sound_set_output_rate(4000);

    sys_file_cleanup();
    thread_cleanup();
    return 1;
}

/*************************************************************************/
/****************************** Basic tests ******************************/
/*************************************************************************/

TEST(test_sound_init_cleanup)
{
    /* The sound framework wil have already been initialized at this point.
     * Check that double initialization does nothing. */
    sound_init();
    CHECK_TRUE(sound_open_device("", 3));

    /* Check that the sound core can be closed and reinitialized. */
    sound_cleanup();
    sound_init();
    CHECK_TRUE(sound_open_device("", 2));

    /* Check that NULL is accepted as well as the empty string. */
    sound_cleanup();
    sound_init();
    CHECK_TRUE(sound_open_device(NULL, 2));

    /* Check that double cleanup does not crash. */
    sound_cleanup();
    sound_cleanup();

    /* Check that cleanup does not crash if no device has been opened. */
    sound_init();
    sound_cleanup();

    /* Check that memory allocation errors are properly handled. */
    sound_init();
    CHECK_MEMORY_FAILURES(sound_open_device("", 2));
    sound_cleanup();

    /* Check that invalid initialization parameters are handled properly. */
    sound_init();
    CHECK_FALSE(sound_open_device("", 0));
    CHECK_FALSE(sound_open_device(NULL, 0));

    /* Check that sys_sound_init() failure is handled properly. */
    CHECK_FALSE(sound_open_device("FAIL", 3));

    /* Check that a zero or negative output rate from
     * sys_sound_playback_rate() is handled properly. */
    CHECK_FALSE(sound_open_device("ZERO", 3));
    CHECK_FALSE(sound_open_device("NEGA", 3));
    sys_test_sound_set_output_rate(4000);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_get_latency)
{
    /* The test implementation takes whatever value we give it and rounds
     * it to the nearest integral number of sample periods. */
    const float target = 0.25;
    const float latency = target - 0.4f/4000.0f;
    CHECK_FLOATEQUAL(sound_set_latency(latency), target);
    CHECK_FLOATEQUAL(sound_get_latency(), target);

    /* A zero or negative value (invalid) should leave the current latency
     * unchanged. */
    CHECK_FLOATEQUAL(sound_set_latency(0), target);
    CHECK_FLOATEQUAL(sound_set_latency(-1), target);

    /* Check that very small latency values are not rounded to zero. */
    const float small_latency = 0.1f/4000.0f;
    const float small_target = 1.0f/4000.0f;
    CHECK_FLOATEQUAL(sound_set_latency(small_latency), small_target);
    CHECK_FLOATEQUAL(sound_get_latency(), small_target);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_check_format)
{
    CHECK_FALSE(sound_check_format(SOUND_FORMAT_MP3));
#ifdef SIL_SOUND_INCLUDE_OGG
    CHECK_TRUE(sound_check_format(SOUND_FORMAT_OGG));
#else
    CHECK_FALSE(sound_check_format(SOUND_FORMAT_OGG));
#endif
    CHECK_TRUE(sound_check_format(SOUND_FORMAT_WAV));
    CHECK_FALSE(sound_check_format((SoundFormat)0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_headphone_disconnect)
{
    sound_enable_headphone_disconnect_check();

    /* The stub implementation should report no disconnect by default. */
    CHECK_FALSE(sound_check_headphone_disconnect());

    /* Check that the flag can be set using the test interface. */
    sys_test_sound_set_headphone_disconnect();
    CHECK_TRUE(sound_check_headphone_disconnect());

    /* Check that the flag is sticky. */
    CHECK_TRUE(sound_check_headphone_disconnect());

    /* Check that the flag can be cleared. */
    sound_acknowledge_headphone_disconnect();
    CHECK_FALSE(sound_check_headphone_disconnect());

    /* Check that the flag stays clear. */
    CHECK_FALSE(sound_check_headphone_disconnect());

    /* Check that a second clear operation is a no-op. */
    sound_acknowledge_headphone_disconnect();
    CHECK_FALSE(sound_check_headphone_disconnect());

    return 1;
}

/*************************************************************************/
/********************* Decoder-based playback tests **********************/
/*************************************************************************/

/* Before we mess with Sound objects, check that playing from a raw
 * decoder handle works.  (We assume the WAV decoder works.) */

/*-----------------------------------------------------------------------*/

TEST(test_play_decoder)
{
    SoundDecodeHandle *decoder;
    int16_t pcm[82];

    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square.wav"));
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_WAV, file, 0, sys_file_size(file), 0, 1));
    sys_file_close(file);
    CHECK_TRUE(sound_play_decoder(decoder, 0, 1, 0));
    get_pcm(pcm, 41);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], -10000);
    CHECK_INTEQUAL(pcm[5], -10000);
    CHECK_INTEQUAL(pcm[6], -10000);
    CHECK_INTEQUAL(pcm[7], -10000);
    CHECK_INTEQUAL(pcm[8], 10000);
    CHECK_INTEQUAL(pcm[9], 10000);
    CHECK_INTEQUAL(pcm[78], -10000);
    CHECK_INTEQUAL(pcm[79], -10000);
    CHECK_INTEQUAL(pcm[80], 0);
    CHECK_INTEQUAL(pcm[81], 0);

    /* Check invalid parameter handling. */
    ASSERT(file = wrap_sys_file_open("testdata/sound/square.wav"));
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_WAV, file, 0, sys_file_size(file), 0, 1));
    sys_file_close(file);
    CHECK_FALSE(sound_play_decoder(NULL, 0, 1, 0));
    CHECK_FALSE(sound_play_decoder(decoder, -1, 1, 0));
    CHECK_FALSE(sound_play_decoder(decoder, 4, 1, 0));
    CHECK_FALSE(sound_play_decoder(decoder, 0, -1, 0));
    CHECK_FALSE(sound_play_decoder(decoder, 0, 1, -2));
    CHECK_FALSE(sound_play_decoder(decoder, 0, 1, 2));
    sound_decode_close(decoder);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_update)
{
    SoundDecodeHandle *decoder;
    int channel;
    int16_t pcm[82];

    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square.wav"));
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_WAV, file, 0, sys_file_size(file), 0, 1));
    sys_file_close(file);

    CHECK_TRUE(channel = sound_play_decoder(decoder, 0, 1, 0));
    CHECK_TRUE(sound_is_playing(channel));

    /* Read past the end of the stream, but don't call sound_update() yet. */
    sound_mixer_get_pcm(pcm, 41);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], -10000);
    CHECK_INTEQUAL(pcm[5], -10000);
    CHECK_INTEQUAL(pcm[6], -10000);
    CHECK_INTEQUAL(pcm[7], -10000);
    CHECK_INTEQUAL(pcm[8], 10000);
    CHECK_INTEQUAL(pcm[9], 10000);
    CHECK_INTEQUAL(pcm[78], -10000);
    CHECK_INTEQUAL(pcm[79], -10000);
    CHECK_INTEQUAL(pcm[80], 0);
    CHECK_INTEQUAL(pcm[81], 0);

    /* Should still be true because we haven't called sound_update() yet. */
    CHECK_TRUE(sound_is_playing(channel));

    /* Now call sound_update() and check that end-of-stream is detected.
     * Note that the software mixer doesn't detect end-of-stream until it
     * tries to read samples in a mix() call and none are available, so we
     * need one extra sound_mixer_get_pcm() call to properly detect that
     * the stream has ended.  Similarly in many test functions below. */
    sound_mixer_get_pcm(pcm, 1);
    sound_update();
    CHECK_FALSE(sound_is_playing(channel));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_playback_status)
{
    int channel;
    int16_t pcm[82];

    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square.wav"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_WAV, file, 0, sys_file_size(file), 0, 1));
    sys_file_close(file);
    /* Float, not int!  Used as the denominator for calculating playback
     * position. */
    const float freq = sound_decode_native_freq(decoder);

    CHECK_TRUE(channel = sound_play_decoder(decoder, 0, 1, 0));
    CHECK_TRUE(sound_is_playing(channel));
    CHECK_FLOATEQUAL(sound_playback_pos(channel), 0/freq);

    get_pcm(pcm, 4);
    CHECK_FLOATEQUAL(sound_playback_pos(channel), 4/freq);
    get_pcm(pcm, 35);
    CHECK_FLOATEQUAL(sound_playback_pos(channel), 39/freq);
    get_pcm(pcm, 1);
    CHECK_FLOATEQUAL(sound_playback_pos(channel), 40/freq);
    get_pcm(pcm, 1);
    CHECK_FALSE(sound_is_playing(channel));
    CHECK_FLOATEQUAL(sound_playback_pos(channel), 0);

    CHECK_FALSE(sound_is_playing(0));
    CHECK_FALSE(sound_is_playing(4));
    CHECK_FLOATEQUAL(sound_playback_pos(0), 0);
    CHECK_FLOATEQUAL(sound_playback_pos(4), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_loop)
{
    SysFile *file;
    SoundDecodeHandle *decoder;
    int16_t pcm[82];

    ASSERT(file = wrap_sys_file_open("testdata/sound/square.wav"));
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_WAV, file, 0, sys_file_size(file), 1, 1));
    sys_file_close(file);
    sound_decode_set_loop_points(decoder, 1, 2);
    CHECK_TRUE(sound_play_decoder(decoder, 0, 1, 0));
    get_pcm(pcm, 41);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], -10000);
    CHECK_INTEQUAL(pcm[5], -10000);
    CHECK_INTEQUAL(pcm[6], 10000);
    CHECK_INTEQUAL(pcm[7], 10000);
    CHECK_INTEQUAL(pcm[8], -10000);
    CHECK_INTEQUAL(pcm[9], -10000);
    CHECK_INTEQUAL(pcm[80], -10000);
    CHECK_INTEQUAL(pcm[81], -10000);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_loop_to_end)
{
    SysFile *file;
    SoundDecodeHandle *decoder;
    int16_t pcm[84];

    ASSERT(file = wrap_sys_file_open("testdata/sound/square.wav"));
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_WAV, file, 0, sys_file_size(file), 1, 1));
    sys_file_close(file);
    sound_decode_set_loop_points(decoder, 37, 0);
    CHECK_TRUE(sound_play_decoder(decoder, 0, 1, 0));
    get_pcm(pcm, 42);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], -10000);
    CHECK_INTEQUAL(pcm[5], -10000);
    CHECK_INTEQUAL(pcm[6], -10000);
    CHECK_INTEQUAL(pcm[7], -10000);
    CHECK_INTEQUAL(pcm[78], -10000);
    CHECK_INTEQUAL(pcm[79], -10000);
    CHECK_INTEQUAL(pcm[80], 10000);
    CHECK_INTEQUAL(pcm[81], 10000);
    CHECK_INTEQUAL(pcm[82], -10000);
    CHECK_INTEQUAL(pcm[83], -10000);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_playback_pos_loop)
{
    int channel;
    int16_t pcm[82];

    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square.wav"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_WAV, file, 0, sys_file_size(file), 1, 1));
    sys_file_close(file);
    sound_decode_set_loop_points(decoder, 10, 10);
    const float freq = sound_decode_native_freq(decoder);

    CHECK_TRUE(channel = sound_play_decoder(decoder, 0, 1, 0));
    CHECK_FLOATEQUAL(sound_playback_pos(channel), 0/freq);

    get_pcm(pcm, 5);
    CHECK_FLOATEQUAL(sound_playback_pos(channel), 5/freq);
    get_pcm(pcm, 14);
    CHECK_FLOATEQUAL(sound_playback_pos(channel), 19/freq);
    get_pcm(pcm, 1);
    CHECK_FLOATEQUAL(sound_playback_pos(channel), 10/freq);
    get_pcm(pcm, 25);
    CHECK_FLOATEQUAL(sound_playback_pos(channel), 15/freq);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_playback_pos_resample)
{
    int channel;
    int16_t pcm[82];

    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square-8k.wav"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_WAV, file, 0, sys_file_size(file), 1, 1));
    sys_file_close(file);
    sound_decode_set_loop_points(decoder, 10, 10);
    const float freq = sound_decode_native_freq(decoder);

    CHECK_TRUE(channel = sound_play_decoder(decoder, 0, 1, 0));
    CHECK_FLOATEQUAL(sound_playback_pos(channel), 0/freq);
    get_pcm(pcm, 7);
    CHECK_FLOATEQUAL(sound_playback_pos(channel), 14/freq);
    get_pcm(pcm, 6);
    CHECK_FLOATEQUAL(sound_playback_pos(channel), 16/freq);
    for (int i = 13; i < 1024/2; i += 5) {  // 1024 == RESAMPLE_BUFLEN
        get_pcm(pcm, 5);
    }
    get_pcm(pcm, 12);
    CHECK_FLOATEQUAL(sound_playback_pos(channel), 10/freq);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_playback_pos_resample_loop_to_end)
{
    int channel;
    int16_t pcm[82];

    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square-8k.wav"));
    SoundDecodeHandle *decoder;
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_WAV, file, 0, sys_file_size(file), 1, 1));
    sys_file_close(file);
    sound_decode_set_loop_points(decoder, 10, 0);
    const float freq = sound_decode_native_freq(decoder);

    CHECK_TRUE(channel = sound_play_decoder(decoder, 0, 1, 0));
    CHECK_FLOATEQUAL(sound_playback_pos(channel), 0/freq);
    get_pcm(pcm, 7);
    CHECK_FLOATEQUAL(sound_playback_pos(channel), 14/freq);
    /* When looping to the end of the stream, the position won't get reset
     * until the end of the current resampling buffer. */
    get_pcm(pcm, 10);
    for (int i = 17; i < 1024/2; i += 15) {  // 1024 == RESAMPLE_BUFLEN
        get_pcm(pcm, 15);
    }
    get_pcm(pcm, 2);
    CHECK_FLOATEQUAL(sound_playback_pos(channel), 38/freq);

    return 1;
}

/*************************************************************************/
/************************** Sound object tests ***************************/
/*************************************************************************/

/* This routine also tests sound_create*() and sound_destroy() since we
 * have no other way to check the validity of created Sound objects. */
TEST(test_play_sound)
{
    Sound *sound;
    int16_t pcm[82];

    /* Check playing of a memory-based sound. */
    CHECK_TRUE(sound = load_sound("testdata/sound/square.wav",
                                  SOUND_FORMAT_WAV));
    CHECK_TRUE(sound_play(sound, 0, 1, 0, 0));
    get_pcm(pcm, 41);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[78], -10000);
    CHECK_INTEQUAL(pcm[79], -10000);
    CHECK_INTEQUAL(pcm[80], 0);
    CHECK_INTEQUAL(pcm[81], 0);
    get_pcm(pcm, 1);  // Detect end-of-stream.
    /* Leave "sound" allocated since we reuse it below. */

    /* Check playing of a memory-based sound without reusing the buffer. */
    static const char wav_data[48] =
        ("RIFF\x28\0\0\0WAVE"
         "fmt \x10\0\0\0\1\0\1\0\xA0\x0F\0\0\x40\x1F\0\0\2\0\x10\0"
         "data\4\0\0\0\x10\x27\xF0\xD8");
    Sound *sound_memory;
    CHECK_TRUE(sound_memory = sound_create((void *)wav_data, sizeof(wav_data),
                                           SOUND_FORMAT_WAV, 0));
    CHECK_TRUE(sound_play(sound_memory, 0, 1, 0, 0));
    get_pcm(pcm, 3);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], -10000);
    CHECK_INTEQUAL(pcm[3], -10000);
    CHECK_INTEQUAL(pcm[4], 0);
    CHECK_INTEQUAL(pcm[5], 0);
    get_pcm(pcm, 1);  // Detect end-of-stream.
    sound_destroy(sound_memory);

    /* Check playing of a file-based sound. */
    Sound *sound_file;
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square.wav"));
    CHECK_TRUE(sound_file = sound_create_stream(
                   file, 0, sys_file_size(file), SOUND_FORMAT_WAV));
    CHECK_TRUE(sound_play(sound_file, 0, 1, 0, 0));
    get_pcm(pcm, 41);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[78], -10000);
    CHECK_INTEQUAL(pcm[79], -10000);
    CHECK_INTEQUAL(pcm[80], 0);
    CHECK_INTEQUAL(pcm[81], 0);
    get_pcm(pcm, 1);  // Detect end-of-stream.
    sound_destroy(sound_file);

    /* Check invalid parameter handling. */
    CHECK_FALSE(sound_create(NULL, 1, 0, 0));
    CHECK_FALSE(sound_create(pcm, 0, 0, 0));
    CHECK_FALSE(sound_create(pcm, 1, 0, 0));
    CHECK_FALSE(sound_create(pcm, 1, 0, 0));
    CHECK_FALSE(sound_create_stream(NULL, 0, 1, 0));
    ASSERT(file = wrap_sys_file_open("testdata/sound/square.wav"));
    CHECK_FALSE(sound_create_stream(file, 0, 0, 0));
    CHECK_FALSE(sound_create_stream(file, 0, 1, 0));
    CHECK_FALSE(sound_create_stream(file, 0, 1, 0));
    sys_file_close(file);
    CHECK_FALSE(sound_play(NULL, 0, 1, 0, 0));
    CHECK_FALSE(sound_play(sound, -1, 1, 0, 0));
    CHECK_FALSE(sound_play(sound, 4, 1, 0, 0));
    CHECK_FALSE(sound_play(sound, 0, -1, 0, 0));
    CHECK_FALSE(sound_play(sound, 0, 1, -2, 0));
    CHECK_FALSE(sound_play(sound, 0, 1, 2, 0));

    /* Check that attempting to play on a specific channel without first
     * reserving that channel fails. */
    CHECK_FALSE(sound_play(sound, 1, 1, 0, 0));

    /* Check handling of a corrupt stream (in this case we pass WAV data to
     * the Ogg Vorbis decoder to force a decoder open error). */
    Sound *sound_ogg;
    CHECK_TRUE(sound_ogg = load_sound("testdata/sound/square.wav",
                                      SOUND_FORMAT_OGG));
    CHECK_FALSE(sound_play(sound_ogg, 0, 1, 0, 0));
    sound_destroy(sound_ogg);

    /* Check handling of no-channels-available errors.  We do this last to
     * detect whether any of the previous calls left channels allocated. */
    Sound *sound1, *sound2, *sound3;
    CHECK_TRUE(sound1 = load_sound("testdata/sound/square.wav",
                                   SOUND_FORMAT_WAV));
    CHECK_TRUE(sound2 = load_sound("testdata/sound/square.wav",
                                   SOUND_FORMAT_WAV));
    CHECK_TRUE(sound3 = load_sound("testdata/sound/square.wav",
                                   SOUND_FORMAT_WAV));
    CHECK_TRUE(sound_play(sound1, 0, 1, 0, 0));
    CHECK_TRUE(sound_play(sound2, 0, 1, 0, 0));
    CHECK_TRUE(sound_play(sound3, 0, 1, 0, 0));
    CHECK_FALSE(sound_play(sound, 0, 1, 0, 0));
    get_pcm(pcm, 41);
    get_pcm(pcm, 1);  // Detect end-of-stream.
    sound_destroy(sound1);
    sound_destroy(sound2);
    sound_destroy(sound3);

    sound_destroy(sound);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_sound_memory_failure)
{
    SysFile *file;
    int64_t datalen;
    void *data;
    Sound *sound;
    int16_t pcm[82];

    ASSERT(file = wrap_sys_file_open("testdata/sound/square.wav"));
    datalen = sys_file_size(file);

    /* Check creation and playing of a memory-based sound. */
    ASSERT(data = mem_alloc(datalen, 0, 0));
    ASSERT(sys_file_read(file, data, datalen) == datalen);
    CHECK_MEMORY_FAILURES(
        (sound = sound_create(data, datalen, SOUND_FORMAT_WAV, 0))
        && (sound_play(sound, 0, 1, 0, 0) || (sound_destroy(sound), 0)));
    mem_free(data);
    get_pcm(pcm, 41);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[78], -10000);
    CHECK_INTEQUAL(pcm[79], -10000);
    CHECK_INTEQUAL(pcm[80], 0);
    CHECK_INTEQUAL(pcm[81], 0);
    get_pcm(pcm, 1);  // Detect end-of-stream.
    sound_destroy(sound);

    /* Check creation of a file-based sound. */
    CHECK_MEMORY_FAILURES(
        sound = sound_create_stream(file, 0, datalen, SOUND_FORMAT_WAV));
    CHECK_TRUE(sound_play(sound, 0, 1, 0, 0));
    get_pcm(pcm, 41);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[78], -10000);
    CHECK_INTEQUAL(pcm[79], -10000);
    CHECK_INTEQUAL(pcm[80], 0);
    CHECK_INTEQUAL(pcm[81], 0);
    get_pcm(pcm, 1);  // Detect end-of-stream.
    sound_destroy(sound);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_format_autodetect)
{
    SysFile *file;
    Sound *sound;
    int16_t pcm[82];

    /* Check detection of real formats. */

    sound_decode_set_handler(SOUND_FORMAT_MP3, dummy_mp3_open);
    CHECK_TRUE(sound = load_sound("testdata/sound/squares.mp3", 0));
    mp3_opened = 0;
    CHECK_FALSE(sound_play(sound, 0, 1, 0, 0));
    CHECK_TRUE(mp3_opened);
    sound_destroy(sound);
    sound_decode_set_handler(SOUND_FORMAT_MP3, NULL);

#ifdef SIL_SOUND_INCLUDE_OGG
    CHECK_TRUE(sound = load_sound("testdata/sound/square.ogg", 0));
    CHECK_TRUE(sound_play(sound, 0, 1, 0, 0));
    get_pcm(pcm, 41);
    CHECK_INTEQUAL(pcm[0], 9763);
    CHECK_INTEQUAL(pcm[1], 9763);
    CHECK_INTEQUAL(pcm[78], -9443);
    CHECK_INTEQUAL(pcm[79], -9443);
    CHECK_INTEQUAL(pcm[80], 0);
    CHECK_INTEQUAL(pcm[81], 0);
    get_pcm(pcm, 1);  // Detect end-of-stream.
    sound_destroy(sound);
#else
    sound_decode_set_handler(SOUND_FORMAT_OGG, dummy_mp3_open);
    CHECK_TRUE(sound = load_sound("testdata/sound/square.ogg", 0));
    mp3_opened = 0;
    CHECK_FALSE(sound_play(sound, 0, 1, 0, 0));
    sound_destroy(sound);
    sound_decode_set_handler(SOUND_FORMAT_OGG, NULL);
#endif

    CHECK_TRUE(sound = load_sound("testdata/sound/square.wav", 0));
    CHECK_TRUE(sound_play(sound, 0, 1, 0, 0));
    get_pcm(pcm, 41);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[78], -10000);
    CHECK_INTEQUAL(pcm[79], -10000);
    CHECK_INTEQUAL(pcm[80], 0);
    CHECK_INTEQUAL(pcm[81], 0);
    get_pcm(pcm, 1);  // Detect end-of-stream.
    sound_destroy(sound);

    /* Check that detection works for files too. */
    ASSERT(file = wrap_sys_file_open("testdata/sound/square.wav"));
    CHECK_TRUE(sound = sound_create_stream(file, 0, sys_file_size(file), 0));
    CHECK_TRUE(sound_play(sound, 0, 1, 0, 0));
    get_pcm(pcm, 41);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[78], -10000);
    CHECK_INTEQUAL(pcm[79], -10000);
    CHECK_INTEQUAL(pcm[80], 0);
    CHECK_INTEQUAL(pcm[81], 0);
    get_pcm(pcm, 1);  // Detect end-of-stream.
    sound_destroy(sound);

    /* Check detection failure on invalid data. */
    CHECK_FALSE(load_sound("testdata/test.txt", 0));
    ASSERT(file = wrap_sys_file_open("testdata/test.txt"));
    CHECK_FALSE(sound_create_stream(file, 0, sys_file_size(file), 0));
    sys_file_close(file);

    /* Check detection failure on partial matches or short data.  We use
     * deliberately-sized buffers to trigger memory access errors on
     * overrun when testing with a memory-checking tool like Valgrind. */
    uint8_t *size1, *size2, *size4, *size9, *size12;
    ASSERT(size1 = mem_alloc(1, 0, 0));
    ASSERT(size2 = mem_alloc(2, 0, 0));
    ASSERT(size4 = mem_alloc(4, 0, 0));
    ASSERT(size9 = mem_alloc(9, 0, 0));
    ASSERT(size12 = mem_alloc(12, 0, 0));
    size1[0] = 0xFF;  // First byte of an MP3 header.
    CHECK_FALSE(sound_create(size1, 1, 0, 0));
    size1[0] = 'O';  // "OggS"[0]
    CHECK_FALSE(sound_create(size1, 1, 0, 0));
    size1[0] = 'R';  // "RIFF"[0]
    CHECK_FALSE(sound_create(size1, 1, 0, 0));
    memcpy(size2, "\xFF\x00", 2);  // Enough bytes for MP3 detection.
    CHECK_FALSE(sound_create(size2, 2, 0, 0));
    memcpy(size4, "RIFF", 4);  // Enough bytes for RIFF detection.
    CHECK_FALSE(sound_create(size4, 4, 0, 0));
    memcpy(size9, "RIFF\1\0\0\0W", 9);  // "WAVE"[0]
    CHECK_FALSE(sound_create(size9, 9, 0, 0));
    memcpy(size12, "RIFF\1\0\0\0WHOP", 12);  // RIFF but not WAVE.
    CHECK_FALSE(sound_create(size12, 12, 0, 0));
    mem_free(size1);
    mem_free(size2);
    mem_free(size4);
    mem_free(size9);
    mem_free(size12);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_stream_autodetect_short_file)
{
    SysFile *file;

    ASSERT(file = wrap_sys_file_open("testdata/sound/square.wav"));
    CHECK_FALSE(sound_create_stream(file, 0, sys_file_size(file)+1, 0));
    sys_file_close(file);

    return 1;
}

/*-----------------------------------------------------------------------*/

#if defined(USING_IOQUEUE) && !defined(SIL_PLATFORM_WINDOWS)
/* Windows doesn't use ioqueue for synchronous reads, so this test won't
 * work. */
TEST(test_stream_autodetect_read_error)
{
    SysFile *file;

    ASSERT(file = wrap_sys_file_open("testdata/sound/square.wav"));
    TEST_misc_ioqueue_iofail_next_read(1);
    CHECK_FALSE(sound_create_stream(file, 0, sys_file_size(file), 0));
    sys_file_close(file);

    return 1;
}
#endif

/*-----------------------------------------------------------------------*/

TEST(test_sound_loop)
{
    Sound *sound;
    int16_t pcm[84];

    /* Check looping of a memory-based sound. */
    CHECK_TRUE(sound = load_sound("testdata/sound/square.wav", 0));
    sound_set_loop(sound, 1, 2);
    CHECK_TRUE(sound_play(sound, 0, 1, 0, 1));
    get_pcm(pcm, 41);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], -10000);
    CHECK_INTEQUAL(pcm[5], -10000);
    CHECK_INTEQUAL(pcm[6], 10000);
    CHECK_INTEQUAL(pcm[7], 10000);
    CHECK_INTEQUAL(pcm[8], -10000);
    CHECK_INTEQUAL(pcm[9], -10000);
    CHECK_INTEQUAL(pcm[80], -10000);
    CHECK_INTEQUAL(pcm[81], -10000);
    sound_destroy(sound);

    sound_cleanup();
    sound_init();
    CHECK_TRUE(sound_open_device("", 3));

    /* Check looping of a file-based sound. */
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square.wav"));
    CHECK_TRUE(sound = sound_create_stream(file, 0, sys_file_size(file), 0));
    sound_set_loop(sound, 1, 2);
    CHECK_TRUE(sound_play(sound, 0, 1, 0, 1));
    get_pcm(pcm, 41);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], -10000);
    CHECK_INTEQUAL(pcm[5], -10000);
    CHECK_INTEQUAL(pcm[6], 10000);
    CHECK_INTEQUAL(pcm[7], 10000);
    CHECK_INTEQUAL(pcm[8], -10000);
    CHECK_INTEQUAL(pcm[9], -10000);
    CHECK_INTEQUAL(pcm[80], -10000);
    CHECK_INTEQUAL(pcm[81], -10000);
    sound_destroy(sound);

    sound_cleanup();
    sound_init();
    CHECK_TRUE(sound_open_device("", 3));

    /* Check that a loop length of 0 loops until the end of the file. */
    CHECK_TRUE(sound = load_sound("testdata/sound/square.wav", 0));
    sound_set_loop(sound, 37, 0);
    CHECK_TRUE(sound_play(sound, 0, 1, 0, 1));
    get_pcm(pcm, 42);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], -10000);
    CHECK_INTEQUAL(pcm[5], -10000);
    CHECK_INTEQUAL(pcm[6], -10000);
    CHECK_INTEQUAL(pcm[7], -10000);
    CHECK_INTEQUAL(pcm[8], 10000);
    CHECK_INTEQUAL(pcm[9], 10000);
    CHECK_INTEQUAL(pcm[78], -10000);
    CHECK_INTEQUAL(pcm[79], -10000);
    CHECK_INTEQUAL(pcm[80], 10000);
    CHECK_INTEQUAL(pcm[81], 10000);
    CHECK_INTEQUAL(pcm[82], -10000);
    CHECK_INTEQUAL(pcm[83], -10000);
    sound_destroy(sound);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_sound_is_stereo)
{
    Sound *sound;

    CHECK_TRUE(sound = load_sound("testdata/sound/square.wav", 0));
    CHECK_FALSE(sound_is_stereo(sound));
    CHECK_FALSE(sound_is_stereo(sound));  // Different code path on 2nd lookup.
    sound_destroy(sound);

    CHECK_TRUE(sound = load_sound("testdata/sound/square-stereo.wav", 0));
    CHECK_TRUE(sound_is_stereo(sound));
    CHECK_TRUE(sound_is_stereo(sound));
    sound_destroy(sound);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_sound_is_stereo_invalid)
{
    Sound *sound;

    CHECK_FALSE(sound_is_stereo(NULL));

    CHECK_TRUE(sound = load_sound("testdata/sound/squares.mp3", 0));
    CHECK_FALSE(sound_is_stereo(sound));
    sound_destroy(sound);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_sound_native_freq)
{
    Sound *sound;

    CHECK_TRUE(sound = load_sound("testdata/sound/square.wav", 0));
    /* Call twice because the first call caches the value, so the second call
     * will take a different code path. */
    CHECK_INTEQUAL(sound_native_freq(sound), 4000);
    CHECK_INTEQUAL(sound_native_freq(sound), 4000);
    sound_destroy(sound);

    CHECK_TRUE(sound = load_sound("testdata/sound/square-8k.wav", 0));
    CHECK_INTEQUAL(sound_native_freq(sound), 8000);
    CHECK_INTEQUAL(sound_native_freq(sound), 8000);
    sound_destroy(sound);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_sound_native_freq_invalid)
{
    Sound *sound;

    CHECK_INTEQUAL(sound_native_freq(NULL), 0);

    CHECK_TRUE(sound = load_sound("testdata/sound/squares.mp3", 0));
    CHECK_INTEQUAL(sound_native_freq(sound), 0);
    sound_destroy(sound);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_sound_set_loop)
{
    Sound *sound;
    int16_t pcm[82];

    CHECK_TRUE(sound = load_sound("testdata/sound/square.wav", 0));
    sound_set_loop(sound, 1, 2);
    CHECK_TRUE(sound_play(sound, 0, 1, 0, 1));
    get_pcm(pcm, 41);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], -10000);
    CHECK_INTEQUAL(pcm[5], -10000);
    CHECK_INTEQUAL(pcm[6], 10000);
    CHECK_INTEQUAL(pcm[7], 10000);
    CHECK_INTEQUAL(pcm[8], -10000);
    CHECK_INTEQUAL(pcm[9], -10000);
    CHECK_INTEQUAL(pcm[80], -10000);
    CHECK_INTEQUAL(pcm[81], -10000);
    sound_destroy(sound);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_sound_set_loop_to_end)
{
    Sound *sound;
    int16_t pcm[84];

    CHECK_TRUE(sound = load_sound("testdata/sound/square.wav", 0));
    sound_set_loop(sound, 37, 0);
    CHECK_TRUE(sound_play(sound, 0, 1, 0, 1));
    get_pcm(pcm, 42);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], -10000);
    CHECK_INTEQUAL(pcm[5], -10000);
    CHECK_INTEQUAL(pcm[6], -10000);
    CHECK_INTEQUAL(pcm[7], -10000);
    CHECK_INTEQUAL(pcm[8], 10000);
    CHECK_INTEQUAL(pcm[9], 10000);
    CHECK_INTEQUAL(pcm[78], -10000);
    CHECK_INTEQUAL(pcm[79], -10000);
    CHECK_INTEQUAL(pcm[80], 10000);
    CHECK_INTEQUAL(pcm[81], 10000);
    CHECK_INTEQUAL(pcm[82], -10000);
    CHECK_INTEQUAL(pcm[83], -10000);
    sound_destroy(sound);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_sound_set_loop_invalid)
{
    Sound *sound;
    int16_t pcm[84];

    CHECK_TRUE(sound = load_sound("testdata/sound/square.wav", 0));
    sound_set_loop(sound, 37, 0);
    sound_set_loop(sound, -1, 1);
    sound_set_loop(sound, 1, -1);
    CHECK_TRUE(sound_play(sound, 0, 1, 0, 1));
    get_pcm(pcm, 42);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], -10000);
    CHECK_INTEQUAL(pcm[5], -10000);
    CHECK_INTEQUAL(pcm[6], -10000);
    CHECK_INTEQUAL(pcm[7], -10000);
    CHECK_INTEQUAL(pcm[8], 10000);
    CHECK_INTEQUAL(pcm[9], 10000);
    CHECK_INTEQUAL(pcm[78], -10000);
    CHECK_INTEQUAL(pcm[79], -10000);
    CHECK_INTEQUAL(pcm[80], 10000);
    CHECK_INTEQUAL(pcm[81], 10000);
    CHECK_INTEQUAL(pcm[82], -10000);
    CHECK_INTEQUAL(pcm[83], -10000);
    sound_destroy(sound);

    sound_set_loop(NULL, 0, 0);  // Make sure this doesn't crash.

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_sound_destroy)
{
    Sound *sound;
    int16_t pcm[82];

    /* Check that a Sound object can be destroyed while it is playing
     * without causing a crash. */
    CHECK_TRUE(sound = load_sound("testdata/sound/square.wav", 0));
    int channel;
    CHECK_TRUE(channel = sound_play(sound, 0, 1, 0, 0));
    get_pcm(pcm, 22);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[42], 10000);
    CHECK_INTEQUAL(pcm[43], 10000);
    sound_destroy(sound);
    get_pcm(pcm, 19);
    CHECK_INTEQUAL(pcm[0], -10000);
    CHECK_INTEQUAL(pcm[1], -10000);
    CHECK_INTEQUAL(pcm[34], -10000);
    CHECK_INTEQUAL(pcm[35], -10000);
    CHECK_INTEQUAL(pcm[36], 0);
    CHECK_INTEQUAL(pcm[37], 0);
    get_pcm(pcm, 1);  // Detect end-of-stream.

    /* We have no way to check that it really was properly freed (aside
     * from the memory leak checks); just make sure we can start another
     * sound and it goes to the same channel. */
    CHECK_TRUE(sound = load_sound("testdata/sound/square.wav", 0));
    CHECK_INTEQUAL(sound_play(sound, 0, 1, 0, 0), channel);
    get_pcm(pcm, 41);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[78], -10000);
    CHECK_INTEQUAL(pcm[79], -10000);
    CHECK_INTEQUAL(pcm[80], 0);
    CHECK_INTEQUAL(pcm[81], 0);
    get_pcm(pcm, 1);  // Detect end-of-stream.
    sound_destroy(sound);

    /* Check that sound_destroy(NULL) doesn't crash (documented as a no-op). */
    sound_destroy(NULL);

    return 1;
}

/*************************************************************************/
/***************************** Filter tests ******************************/
/*************************************************************************/

TEST(test_flange)
{
    Sound *sound;
    int channel;
    int16_t pcm[8];

    CHECK_TRUE(sound = load_sound("testdata/sound/square.wav", 0));
    CHECK_TRUE(channel = sound_play(sound, 0, 1, 0, 1));

    /* Check that the flange filter is applied to input audio data. */
    sound_set_flange(channel, 1, 0.1f, 1.5f/4000.0f);
    get_pcm(pcm, 4);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], -9916);
    CHECK_INTEQUAL(pcm[5], -9916);
    CHECK_INTEQUAL(pcm[6], -10000);
    CHECK_INTEQUAL(pcm[7], -10000);

    /* Check that flanging can be turned off and on or reset while a sound
     * is playing. */
    sound_set_flange(channel, 0, 0, 0);
    get_pcm(pcm, 4);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], -10000);
    CHECK_INTEQUAL(pcm[5], -10000);
    CHECK_INTEQUAL(pcm[6], -10000);
    CHECK_INTEQUAL(pcm[7], -10000);
    sound_set_flange(channel, 1, 0.1f, 1.5f/4000.0f);
    get_pcm(pcm, 4);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], -9916);
    CHECK_INTEQUAL(pcm[5], -9916);
    CHECK_INTEQUAL(pcm[6], -10000);
    CHECK_INTEQUAL(pcm[7], -10000);
    sound_set_flange(channel, 1, 0.1f, 4.0f/4000.0f);
    get_pcm(pcm, 4);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], -9776);
    CHECK_INTEQUAL(pcm[5], -9776);
    CHECK_INTEQUAL(pcm[6], -10000);
    CHECK_INTEQUAL(pcm[7], -10000);
    sound_set_flange(channel, 0, 0, 0);
    get_pcm(pcm, 4);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], -10000);
    CHECK_INTEQUAL(pcm[5], -10000);
    CHECK_INTEQUAL(pcm[6], -10000);
    CHECK_INTEQUAL(pcm[7], -10000);

    /* Check invalid parameter handling. */

    sound_set_flange(0, 1, 0.1f, 1.5f/4000.0f);
    get_pcm(pcm, 4);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], -10000);
    CHECK_INTEQUAL(pcm[5], -10000);
    CHECK_INTEQUAL(pcm[6], -10000);
    CHECK_INTEQUAL(pcm[7], -10000);

    sound_set_flange(4, 1, 0.1f, 1.5f/4000.0f);
    get_pcm(pcm, 4);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], -10000);
    CHECK_INTEQUAL(pcm[5], -10000);
    CHECK_INTEQUAL(pcm[6], -10000);
    CHECK_INTEQUAL(pcm[7], -10000);

    sound_set_flange(channel==1 ? 2 : 1, 1, 0.1f, 1.5f/4000.0f);
    get_pcm(pcm, 4);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], -10000);
    CHECK_INTEQUAL(pcm[5], -10000);
    CHECK_INTEQUAL(pcm[6], -10000);
    CHECK_INTEQUAL(pcm[7], -10000);

    sound_set_flange(channel, 1, 0, 1.5f/4000.0f);
    get_pcm(pcm, 4);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], -10000);
    CHECK_INTEQUAL(pcm[5], -10000);
    CHECK_INTEQUAL(pcm[6], -10000);
    CHECK_INTEQUAL(pcm[7], -10000);

    sound_set_flange(channel, 1, 0.1f, -1.5f/4000.0f);
    get_pcm(pcm, 4);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], -10000);
    CHECK_INTEQUAL(pcm[5], -10000);
    CHECK_INTEQUAL(pcm[6], -10000);
    CHECK_INTEQUAL(pcm[7], -10000);

    sound_set_flange(channel, 1, 0.1f, 65536.0f);
    get_pcm(pcm, 4);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], -10000);
    CHECK_INTEQUAL(pcm[5], -10000);
    CHECK_INTEQUAL(pcm[6], -10000);
    CHECK_INTEQUAL(pcm[7], -10000);

    sound_destroy(sound);
    return 1;
}

/*************************************************************************/
/****************************** Other tests ******************************/
/*************************************************************************/

TEST(test_no_device_opened)
{
    sound_cleanup();
    sound_init();

    Sound *sound;
    CHECK_TRUE(sound = load_sound("testdata/sound/square.wav", 0));

    SoundDecodeHandle *decoder;
    SysFile *file;
    ASSERT(file = wrap_sys_file_open("testdata/sound/square-8k.wav"));
    CHECK_TRUE(decoder = sound_decode_open_from_file(
                   SOUND_FORMAT_WAV, file, 0, sys_file_size(file), 1, 1));
    sys_file_close(file);

    /* Most of these return no value; we just call them to make sure they
     * don't crash due to NULL dereference or the like. */
    sound_set_interpolate(0);
    CHECK_FALSE(sound_get_latency());
    CHECK_FALSE(sound_set_latency(0));
    CHECK_FALSE(sound_check_format(SOUND_FORMAT_WAV));
    sound_set_global_volume(0);
    sound_update();
    sound_pause_all();
    sound_resume_all();
    sound_enable_headphone_disconnect_check();
    CHECK_FALSE(sound_check_headphone_disconnect());
    sound_acknowledge_headphone_disconnect();
    CHECK_FALSE(sound_reserve_channel());
    sound_free_channel(1);
    CHECK_FALSE(sound_play(sound, 0, 1, 0, 0));
    CHECK_FALSE(sound_play_decoder(decoder, 0, 1, 0));
    sound_pause(1);
    sound_resume(1);
    sound_cut(1);
    sound_fade(1, 0);
    sound_adjust_volume(1, 1, 0);
    sound_set_pan(1, 0);
    sound_set_playback_rate(1, 4000);
    sound_set_flange(1, 1, 1, 1);
    sound_set_filter(1, NULL);
    sound_enable_loop(1, 1);
    CHECK_FALSE(sound_is_playing(1));
    CHECK_FALSE(sound_playback_pos(1));

    sound_destroy(sound);
    sound_decode_close(decoder);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_global_volume)
{
    Sound *sound;
    int16_t pcm[82];

    sound_set_global_volume(1.5);

    CHECK_TRUE(sound = load_sound("testdata/sound/square.wav", 0));
    CHECK_TRUE(sound_play(sound, 0, 1, 0, 0));
    get_pcm(pcm, 41);
    CHECK_INTEQUAL(pcm[0], 15000);
    CHECK_INTEQUAL(pcm[1], 15000);
    CHECK_INTEQUAL(pcm[78], -15000);
    CHECK_INTEQUAL(pcm[79], -15000);
    CHECK_INTEQUAL(pcm[80], 0);
    CHECK_INTEQUAL(pcm[81], 0);
    get_pcm(pcm, 1);  // Detect end-of-stream.
    sound_destroy(sound);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_global_volume_invalid)
{
    Sound *sound;
    int16_t pcm[82];

    sound_set_global_volume(-1);  // Should be ignored.
    sound_set_global_volume(16);  // Should be ignored.

    CHECK_TRUE(sound = load_sound("testdata/sound/square.wav", 0));
    CHECK_TRUE(sound_play(sound, 0, 1, 0, 0));
    get_pcm(pcm, 41);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[78], -10000);
    CHECK_INTEQUAL(pcm[79], -10000);
    CHECK_INTEQUAL(pcm[80], 0);
    CHECK_INTEQUAL(pcm[81], 0);
    get_pcm(pcm, 1);  // Detect end-of-stream.
    sound_destroy(sound);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_sound_abort)
{
    Sound *sound;
    int16_t pcm[82];

    CHECK_TRUE(sound = load_sound("testdata/sound/square.wav", 0));

    /* Check that sounds being played back are properly aborted when
     * sound_cleanup() is called. */
    int channel;
    CHECK_TRUE(channel = sound_play(sound, 0, 1, 0, 0));
    get_pcm(pcm, 22);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[42], 10000);
    CHECK_INTEQUAL(pcm[43], 10000);
    sound_cleanup();

    /* Again, we have no way to check what happened after sound_cleanup(),
     * so just reinit, play the sound again, and make sure nothing breaks. */
    sound_init();
    CHECK_TRUE(sound_open_device("", 3));
    CHECK_INTEQUAL(sound_play(sound, 0, 1, 0, 0), channel);
    get_pcm(pcm, 41);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[78], -10000);
    CHECK_INTEQUAL(pcm[79], -10000);
    CHECK_INTEQUAL(pcm[80], 0);
    CHECK_INTEQUAL(pcm[81], 0);
    get_pcm(pcm, 1);  // Detect end-of-stream.

    sound_destroy(sound);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_interpolate)
{
    Sound *sound;
    int channel;
    int16_t pcm[82];

    CHECK_TRUE(sound = load_sound("testdata/sound/square.wav", 0));

    /* Use an output rate of 5kHz so we get upsampling. */
    sound_cleanup();
    sys_test_sound_set_output_rate(5000);
    sound_init();
    CHECK_TRUE(sound_open_device("", 3));

    /* Interpolation should be enabled by default. */
    CHECK_TRUE(channel = sound_play(sound, 0, 1, 0, 0));
    get_pcm(pcm, 6);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], -2000);
    CHECK_INTEQUAL(pcm[5], -2000);
    CHECK_INTEQUAL(pcm[6], -10000);
    CHECK_INTEQUAL(pcm[7], -10000);
    CHECK_INTEQUAL(pcm[8], -6000);
    CHECK_INTEQUAL(pcm[9], -6000);
    CHECK_INTEQUAL(pcm[10], 10000);
    CHECK_INTEQUAL(pcm[11], 10000);

    /* sound_set_interpolate() should not change the state of a running
     * sound. */
    sound_set_interpolate(0);
    get_pcm(pcm, 4);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], -2000);
    CHECK_INTEQUAL(pcm[3], -2000);
    CHECK_INTEQUAL(pcm[4], -10000);
    CHECK_INTEQUAL(pcm[5], -10000);
    CHECK_INTEQUAL(pcm[6], -6000);
    CHECK_INTEQUAL(pcm[7], -6000);

    sound_cut(channel);

    /* sound_set_interpolate() should affect newly-started sounds. */
    CHECK_TRUE(channel = sound_play(sound, 0, 1, 0, 0));
    get_pcm(pcm, 6);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], 10000);
    CHECK_INTEQUAL(pcm[5], 10000);
    CHECK_INTEQUAL(pcm[6], -10000);
    CHECK_INTEQUAL(pcm[7], -10000);
    CHECK_INTEQUAL(pcm[8], -10000);
    CHECK_INTEQUAL(pcm[9], -10000);
    CHECK_INTEQUAL(pcm[10], 10000);
    CHECK_INTEQUAL(pcm[11], 10000);

    /* sound_init() should reset the interpolation flag to true. */
    sound_cleanup();
    sound_init();
    CHECK_TRUE(sound_open_device("", 3));
    CHECK_TRUE(channel = sound_play(sound, 0, 1, 0, 0));
    get_pcm(pcm, 6);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], -2000);
    CHECK_INTEQUAL(pcm[5], -2000);
    CHECK_INTEQUAL(pcm[6], -10000);
    CHECK_INTEQUAL(pcm[7], -10000);
    CHECK_INTEQUAL(pcm[8], -6000);
    CHECK_INTEQUAL(pcm[9], -6000);
    CHECK_INTEQUAL(pcm[10], 10000);
    CHECK_INTEQUAL(pcm[11], 10000);

    sound_destroy(sound);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_pause_resume_all)
{
    Sound *sound;
    int16_t pcm[4];

    CHECK_TRUE(sound = load_sound("testdata/sound/square.wav", 0));

    /* Check that sounds being played back are properly stopped when
     * sound_pause_all() is called, and resume from the stopped position
     * when sound_resume_all() is called. */
    CHECK_TRUE(sound_play(sound, 0, 1, 0, 0));
    get_pcm(pcm, 2);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    sound_pause_all();
    get_pcm(pcm, 2);
    CHECK_INTEQUAL(pcm[0], 0);
    CHECK_INTEQUAL(pcm[1], 0);
    CHECK_INTEQUAL(pcm[2], 0);
    CHECK_INTEQUAL(pcm[3], 0);
    sound_resume_all();
    get_pcm(pcm, 2);
    CHECK_INTEQUAL(pcm[0], -10000);
    CHECK_INTEQUAL(pcm[1], -10000);
    CHECK_INTEQUAL(pcm[2], -10000);
    CHECK_INTEQUAL(pcm[3], -10000);

    /* Check that multiple sound_pause_all() or sound_resume_all() calls
     * do not stack. */

    get_pcm(pcm, 2);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    sound_pause_all();
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], 0);
    CHECK_INTEQUAL(pcm[1], 0);
    sound_pause_all();
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], 0);
    CHECK_INTEQUAL(pcm[1], 0);
    sound_resume_all();
    get_pcm(pcm, 2);
    CHECK_INTEQUAL(pcm[0], -10000);
    CHECK_INTEQUAL(pcm[1], -10000);
    CHECK_INTEQUAL(pcm[2], -10000);
    CHECK_INTEQUAL(pcm[3], -10000);
    sound_resume_all();
    get_pcm(pcm, 2);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);

    sound_destroy(sound);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_reserve_channel)
{
    Sound *sound;
    int channel, channel2, channel3;
    int16_t pcm[82];

    CHECK_TRUE(sound = load_sound("testdata/sound/square.wav", 0));

    /* Check that a channel can be reserved and a sound played on that
     * channel. */
    CHECK_TRUE(channel = sound_reserve_channel());
    CHECK_INTEQUAL(sound_play(sound, channel, 1, 0, 0), channel);
    get_pcm(pcm, 41);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[78], -10000);
    CHECK_INTEQUAL(pcm[79], -10000);
    CHECK_INTEQUAL(pcm[80], 0);
    CHECK_INTEQUAL(pcm[81], 0);
    get_pcm(pcm, 1);  // Detect end-of-stream.

    /* Check that playing a sound on a reserved channel will abort any
     * running playback on that channel. */
    CHECK_INTEQUAL(sound_play(sound, channel, 1, 0, 0), channel);
    get_pcm(pcm, 3);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[4], -10000);
    CHECK_INTEQUAL(pcm[5], -10000);
    CHECK_INTEQUAL(sound_play(sound, channel, 1, 0, 0), channel);
    get_pcm(pcm, 41);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[78], -10000);
    CHECK_INTEQUAL(pcm[79], -10000);
    CHECK_INTEQUAL(pcm[80], 0);
    CHECK_INTEQUAL(pcm[81], 0);
    get_pcm(pcm, 1);  // Detect end-of-stream.

    /* Check that more channels can't be reserved than actually exist,
     * and make sure that the same channel isn't reserved twice. */
    CHECK_TRUE(channel2 = sound_reserve_channel());
    CHECK_TRUE(channel3 = sound_reserve_channel());
    CHECK_FALSE(sound_reserve_channel());
    CHECK_FALSE(channel == channel2);
    CHECK_FALSE(channel == channel3);
    CHECK_FALSE(channel2 == channel3);

    /* Check that sound won't play if there are no unreserved channels. */
    CHECK_FALSE(sound_play(sound, 0, 1, 0, 0));
    sound_free_channel(channel3);
    CHECK_INTEQUAL(sound_play(sound, 0, 1, 0, 0), channel3);
    CHECK_FALSE(sound_play(sound, 0, 1, 0, 0));
    get_pcm(pcm, 41);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[78], -10000);
    CHECK_INTEQUAL(pcm[79], -10000);
    CHECK_INTEQUAL(pcm[80], 0);
    CHECK_INTEQUAL(pcm[81], 0);
    get_pcm(pcm, 1);  // Detect end-of-stream.

    /* Check that an unreserved channel dynamically allocated for playback
     * can't be reserved while the sound is playing. */
    CHECK_INTEQUAL(sound_play(sound, 0, 1, 0, 0), channel3);
    CHECK_FALSE(sound_reserve_channel());
    get_pcm(pcm, 41);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[78], -10000);
    CHECK_INTEQUAL(pcm[79], -10000);
    CHECK_INTEQUAL(pcm[80], 0);
    CHECK_INTEQUAL(pcm[81], 0);
    get_pcm(pcm, 1);  // Detect end-of-stream.

    /* Check invalid/no-op calls to sound_free_channel(). */
    sound_free_channel(-1);
    sound_free_channel(0);  // Defined to be a no-op (no error message).
    sound_free_channel(channel3);
    sound_free_channel(4);
    CHECK_INTEQUAL(sound_play(sound, 0, 1, 0, 0), channel3);
    CHECK_FALSE(sound_play(sound, 0, 1, 0, 0));
    get_pcm(pcm, 41);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[78], -10000);
    CHECK_INTEQUAL(pcm[79], -10000);
    CHECK_INTEQUAL(pcm[80], 0);
    CHECK_INTEQUAL(pcm[81], 0);
    get_pcm(pcm, 1);  // Detect end-of-stream.

    sound_destroy(sound);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_pause_resume)
{
    Sound *sound;
    int channel1, channel2, channel3;
    int16_t pcm[82];

    CHECK_TRUE(sound = load_sound("testdata/sound/square.wav", 0));

    /* Check pause and resume of a playing sound. */
    CHECK_TRUE(channel1 = sound_play(sound, 0, 1, 0, 0));
    CHECK_TRUE(channel2 = sound_play(sound, 0, 1, 0, 0));
    get_pcm(pcm, 2);
    CHECK_INTEQUAL(pcm[0], 20000);
    CHECK_INTEQUAL(pcm[1], 20000);
    CHECK_INTEQUAL(pcm[2], 20000);
    CHECK_INTEQUAL(pcm[3], 20000);
    sound_pause(channel1);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], -10000);
    CHECK_INTEQUAL(pcm[1], -10000);
    sound_resume(channel1);  // channel1 now lags channel2 by 1 sample.
    get_pcm(pcm, 39);
    CHECK_INTEQUAL(pcm[0], -20000);
    CHECK_INTEQUAL(pcm[1], -20000);
    CHECK_INTEQUAL(pcm[2], 0);
    CHECK_INTEQUAL(pcm[3], 0);
    CHECK_INTEQUAL(pcm[4], 20000);
    CHECK_INTEQUAL(pcm[5], 20000);
    CHECK_INTEQUAL(pcm[6], 0);
    CHECK_INTEQUAL(pcm[7], 0);
    CHECK_INTEQUAL(pcm[8], -20000);
    CHECK_INTEQUAL(pcm[9], -20000);
    CHECK_INTEQUAL(pcm[72], -20000);
    CHECK_INTEQUAL(pcm[73], -20000);
    CHECK_INTEQUAL(pcm[74], -10000);
    CHECK_INTEQUAL(pcm[75], -10000);
    CHECK_INTEQUAL(pcm[76], 0);
    CHECK_INTEQUAL(pcm[77], 0);
    get_pcm(pcm, 1);  // Detect end-of-stream.

    /* Check that multiple calls do not stack. */
    CHECK_TRUE(channel1 = sound_play(sound, 0, 1, 0, 0));
    CHECK_TRUE(channel2 = sound_play(sound, 0, 1, 0, 0));
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], 20000);
    CHECK_INTEQUAL(pcm[1], 20000);
    sound_pause(channel1);
    sound_pause(channel1);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    sound_resume(channel1);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], 0);
    CHECK_INTEQUAL(pcm[1], 0);
    sound_resume(channel1);
    get_pcm(pcm, 39);
    CHECK_INTEQUAL(pcm[0], -20000);
    CHECK_INTEQUAL(pcm[1], -20000);
    CHECK_INTEQUAL(pcm[2], 0);
    CHECK_INTEQUAL(pcm[3], 0);
    CHECK_INTEQUAL(pcm[4], 20000);
    CHECK_INTEQUAL(pcm[5], 20000);
    CHECK_INTEQUAL(pcm[6], 0);
    CHECK_INTEQUAL(pcm[7], 0);
    CHECK_INTEQUAL(pcm[8], -20000);
    CHECK_INTEQUAL(pcm[9], -20000);
    CHECK_INTEQUAL(pcm[72], -20000);
    CHECK_INTEQUAL(pcm[73], -20000);
    CHECK_INTEQUAL(pcm[74], -10000);
    CHECK_INTEQUAL(pcm[75], -10000);
    CHECK_INTEQUAL(pcm[76], 0);
    CHECK_INTEQUAL(pcm[77], 0);
    get_pcm(pcm, 1);  // Detect end-of-stream.

    /* Check invalid parameter handling (just that they don't misbehave). */
    CHECK_TRUE(channel1 = sound_play(sound, 0, 1, 0, 0));
    CHECK_TRUE(channel2 = sound_play(sound, 0, 1, 0, 0));
    /* Normally channel1 == 1 and channel2 == 2, but make this work no matter
     * which channels are used. */
    channel3 = 6 - channel1 - channel2;
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], 20000);
    CHECK_INTEQUAL(pcm[1], 20000);
    sound_pause(0);
    sound_pause(channel3);
    sound_pause(4);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], 20000);
    CHECK_INTEQUAL(pcm[1], 20000);
    sound_pause(channel1);
    sound_resume(0);
    sound_resume(channel3);
    sound_resume(4);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], -10000);
    CHECK_INTEQUAL(pcm[1], -10000);
    sound_resume(channel1);
    get_pcm(pcm, 39);
    CHECK_INTEQUAL(pcm[0], -20000);
    CHECK_INTEQUAL(pcm[1], -20000);
    CHECK_INTEQUAL(pcm[2], 0);
    CHECK_INTEQUAL(pcm[3], 0);
    CHECK_INTEQUAL(pcm[72], -20000);
    CHECK_INTEQUAL(pcm[73], -20000);
    CHECK_INTEQUAL(pcm[74], -10000);
    CHECK_INTEQUAL(pcm[75], -10000);
    CHECK_INTEQUAL(pcm[76], 0);
    CHECK_INTEQUAL(pcm[77], 0);
    get_pcm(pcm, 1);  // Detect end-of-stream.

    sound_destroy(sound);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_pause_resume_global_and_local)
{
    Sound *sound;
    int channel1, channel2;
    int16_t pcm[82];

    CHECK_TRUE(sound = load_sound("testdata/sound/square.wav", 0));

    /* Check that global pause and resume does not unpause a locally
     * paused sound. */
    CHECK_TRUE(channel1 = sound_play(sound, 0, 1, 0, 0));
    CHECK_TRUE(channel2 = sound_play(sound, 0, 1, 0, 0));
    get_pcm(pcm, 2);
    CHECK_INTEQUAL(pcm[0], 20000);
    CHECK_INTEQUAL(pcm[1], 20000);
    CHECK_INTEQUAL(pcm[2], 20000);
    CHECK_INTEQUAL(pcm[3], 20000);
    sound_pause(channel1);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], -10000);
    CHECK_INTEQUAL(pcm[1], -10000);
    sound_pause_all();
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], 0);
    CHECK_INTEQUAL(pcm[1], 0);
    sound_resume_all();
    get_pcm(pcm, 2);
    CHECK_INTEQUAL(pcm[0], -10000);
    CHECK_INTEQUAL(pcm[1], -10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    sound_resume(channel1);  // channel1 now lags channel2 by 3 samples.
    get_pcm(pcm, 39);
    CHECK_INTEQUAL(pcm[0], 0);
    CHECK_INTEQUAL(pcm[1], 0);
    CHECK_INTEQUAL(pcm[2], -20000);
    CHECK_INTEQUAL(pcm[3], -20000);
    CHECK_INTEQUAL(pcm[4], 0);
    CHECK_INTEQUAL(pcm[5], 0);
    CHECK_INTEQUAL(pcm[6], 20000);
    CHECK_INTEQUAL(pcm[7], 20000);
    CHECK_INTEQUAL(pcm[74], -10000);
    CHECK_INTEQUAL(pcm[75], -10000);
    CHECK_INTEQUAL(pcm[76], 0);
    CHECK_INTEQUAL(pcm[77], 0);
    get_pcm(pcm, 1);  // Detect end-of-stream.

    /* Check that sound_pause() is honored during global pause. */
    CHECK_TRUE(channel1 = sound_play(sound, 0, 1, 0, 0));
    CHECK_TRUE(channel2 = sound_play(sound, 0, 1, 0, 0));
    get_pcm(pcm, 2);
    CHECK_INTEQUAL(pcm[0], 20000);
    CHECK_INTEQUAL(pcm[1], 20000);
    CHECK_INTEQUAL(pcm[2], 20000);
    CHECK_INTEQUAL(pcm[3], 20000);
    sound_pause_all();
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], 0);
    CHECK_INTEQUAL(pcm[1], 0);
    sound_pause(channel1);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], 0);
    CHECK_INTEQUAL(pcm[1], 0);
    sound_resume_all();
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], -10000);
    CHECK_INTEQUAL(pcm[1], -10000);
    sound_resume(channel1);  // channel1 now lags channel2 by 1 sample.
    get_pcm(pcm, 39);
    CHECK_INTEQUAL(pcm[0], -20000);
    CHECK_INTEQUAL(pcm[1], -20000);
    CHECK_INTEQUAL(pcm[2], 0);
    CHECK_INTEQUAL(pcm[3], 0);
    CHECK_INTEQUAL(pcm[74], -10000);
    CHECK_INTEQUAL(pcm[75], -10000);
    CHECK_INTEQUAL(pcm[76], 0);
    CHECK_INTEQUAL(pcm[77], 0);
    get_pcm(pcm, 1);  // Detect end-of-stream.

    /* Check that sound_resume() is honored during global pause but not
     * applied until sound_resume_all() is called. */
    CHECK_TRUE(channel1 = sound_play(sound, 0, 1, 0, 0));
    CHECK_TRUE(channel2 = sound_play(sound, 0, 1, 0, 0));
    get_pcm(pcm, 2);
    CHECK_INTEQUAL(pcm[0], 20000);
    CHECK_INTEQUAL(pcm[1], 20000);
    CHECK_INTEQUAL(pcm[2], 20000);
    CHECK_INTEQUAL(pcm[3], 20000);
    sound_pause(channel1);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], -10000);
    CHECK_INTEQUAL(pcm[1], -10000);
    sound_pause_all();
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], 0);
    CHECK_INTEQUAL(pcm[1], 0);
    sound_resume(channel1);  // channel1 now lags channel2 by 1 sample.
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], 0);
    CHECK_INTEQUAL(pcm[1], 0);
    sound_resume_all();
    get_pcm(pcm, 39);
    CHECK_INTEQUAL(pcm[0], -20000);
    CHECK_INTEQUAL(pcm[1], -20000);
    CHECK_INTEQUAL(pcm[2], 0);
    CHECK_INTEQUAL(pcm[3], 0);
    CHECK_INTEQUAL(pcm[74], -10000);
    CHECK_INTEQUAL(pcm[75], -10000);
    CHECK_INTEQUAL(pcm[76], 0);
    CHECK_INTEQUAL(pcm[77], 0);
    get_pcm(pcm, 1);  // Detect end-of-stream.

    sound_destroy(sound);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_cut)
{
    Sound *sound;
    int channel1, channel2, channel3;
    int16_t pcm[82];

    CHECK_TRUE(sound = load_sound("testdata/sound/square.wav", 0));

    /* Check stopping of a playing sound. */
    CHECK_TRUE(channel1 = sound_play(sound, 0, 1, 0, 0));
    CHECK_TRUE(channel2 = sound_play(sound, 0, 1, 0, 0));
    get_pcm(pcm, 2);
    CHECK_INTEQUAL(pcm[0], 20000);
    CHECK_INTEQUAL(pcm[1], 20000);
    CHECK_INTEQUAL(pcm[2], 20000);
    CHECK_INTEQUAL(pcm[3], 20000);
    sound_cut(channel1);
    get_pcm(pcm, 39);
    CHECK_INTEQUAL(pcm[0], -10000);
    CHECK_INTEQUAL(pcm[1], -10000);
    CHECK_INTEQUAL(pcm[74], -10000);
    CHECK_INTEQUAL(pcm[75], -10000);
    CHECK_INTEQUAL(pcm[76], 0);
    CHECK_INTEQUAL(pcm[77], 0);
    get_pcm(pcm, 1);  // Detect end-of-stream.

    /* Check invalid parameter handling (just that they don't misbehave). */
    CHECK_TRUE(channel1 = sound_play(sound, 0, 1, 0, 0));
    CHECK_TRUE(channel2 = sound_play(sound, 0, 1, 0, 0));
    /* Normally channel1 == 1 and channel2 == 2, but make this work no matter
     * which channels are used. */
    channel3 = 6 - channel1 - channel2;
    get_pcm(pcm, 2);
    CHECK_INTEQUAL(pcm[0], 20000);
    CHECK_INTEQUAL(pcm[1], 20000);
    CHECK_INTEQUAL(pcm[2], 20000);
    CHECK_INTEQUAL(pcm[3], 20000);
    sound_cut(0);
    sound_cut(channel3);
    sound_cut(4);
    get_pcm(pcm, 39);
    CHECK_INTEQUAL(pcm[0], -20000);
    CHECK_INTEQUAL(pcm[1], -20000);
    CHECK_INTEQUAL(pcm[74], -20000);
    CHECK_INTEQUAL(pcm[75], -20000);
    CHECK_INTEQUAL(pcm[76], 0);
    CHECK_INTEQUAL(pcm[77], 0);
    get_pcm(pcm, 1);  // Detect end-of-stream.

    sound_destroy(sound);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fade)
{
    Sound *sound;
    int channel1, channel2, channel3;
    int16_t pcm[82];

    CHECK_TRUE(sound = load_sound("testdata/sound/square.wav", 0));

    /* Check fading of a playing sound. */
    CHECK_TRUE(channel1 = sound_play(sound, 0, 1, 0, 0));
    CHECK_TRUE(channel2 = sound_play(sound, 0, 1, 0, 0));
    get_pcm(pcm, 2);
    CHECK_INTEQUAL(pcm[0], 20000);
    CHECK_INTEQUAL(pcm[1], 20000);
    CHECK_INTEQUAL(pcm[2], 20000);
    CHECK_INTEQUAL(pcm[3], 20000);
    sound_fade(channel1, 4.0f/4000.0f);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], -17500);
    CHECK_INTEQUAL(pcm[1], -17500);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], -15000);
    CHECK_INTEQUAL(pcm[1], -15000);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], 12500);
    CHECK_INTEQUAL(pcm[1], 12500);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    get_pcm(pcm, 35);
    CHECK_INTEQUAL(pcm[0], -10000);
    CHECK_INTEQUAL(pcm[1], -10000);
    CHECK_INTEQUAL(pcm[66], -10000);
    CHECK_INTEQUAL(pcm[67], -10000);
    CHECK_INTEQUAL(pcm[68], 0);
    CHECK_INTEQUAL(pcm[69], 0);
    get_pcm(pcm, 1);  // Detect end-of-stream.

    /* Check termination of a playing sound using a fade length of 0. */
    CHECK_TRUE(channel1 = sound_play(sound, 0, 1, 0, 0));
    CHECK_TRUE(channel2 = sound_play(sound, 0, 1, 0, 0));
    get_pcm(pcm, 2);
    CHECK_INTEQUAL(pcm[0], 20000);
    CHECK_INTEQUAL(pcm[1], 20000);
    CHECK_INTEQUAL(pcm[2], 20000);
    CHECK_INTEQUAL(pcm[3], 20000);
    sound_fade(channel1, 0);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], -10000);
    CHECK_INTEQUAL(pcm[1], -10000);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], -10000);
    CHECK_INTEQUAL(pcm[1], -10000);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    get_pcm(pcm, 35);
    CHECK_INTEQUAL(pcm[0], -10000);
    CHECK_INTEQUAL(pcm[1], -10000);
    CHECK_INTEQUAL(pcm[66], -10000);
    CHECK_INTEQUAL(pcm[67], -10000);
    CHECK_INTEQUAL(pcm[68], 0);
    CHECK_INTEQUAL(pcm[69], 0);
    get_pcm(pcm, 1);  // Detect end-of-stream.

    /* Check invalid parameter handling (just that they don't misbehave). */
    CHECK_TRUE(channel1 = sound_play(sound, 0, 1, 0, 0));
    CHECK_TRUE(channel2 = sound_play(sound, 0, 1, 0, 0));
    /* Normally channel1 == 1 and channel2 == 2, but make this work no matter
     * which channels are used. */
    channel3 = 6 - channel1 - channel2;
    get_pcm(pcm, 2);
    CHECK_INTEQUAL(pcm[0], 20000);
    CHECK_INTEQUAL(pcm[1], 20000);
    CHECK_INTEQUAL(pcm[2], 20000);
    CHECK_INTEQUAL(pcm[3], 20000);
    sound_fade(0, 4.0f/4000.0f);
    sound_fade(channel3, 4.0f/4000.0f);
    sound_fade(4, 4.0f/4000.0f);
    get_pcm(pcm, 39);
    CHECK_INTEQUAL(pcm[0], -20000);
    CHECK_INTEQUAL(pcm[1], -20000);
    CHECK_INTEQUAL(pcm[74], -20000);
    CHECK_INTEQUAL(pcm[75], -20000);
    CHECK_INTEQUAL(pcm[76], 0);
    CHECK_INTEQUAL(pcm[77], 0);
    get_pcm(pcm, 1);  // Detect end-of-stream.

    sound_destroy(sound);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_adjust_volume)
{
    Sound *sound;
    int channel1, channel2, channel3;
    int16_t pcm[82];

    CHECK_TRUE(sound = load_sound("testdata/sound/square.wav", 0));

    /* Check volume adjustment of a playing sound. */
    CHECK_TRUE(channel1 = sound_play(sound, 0, 1, 0, 0));
    CHECK_TRUE(channel2 = sound_play(sound, 0, 1, 0, 0));
    get_pcm(pcm, 2);
    CHECK_INTEQUAL(pcm[0], 20000);
    CHECK_INTEQUAL(pcm[1], 20000);
    CHECK_INTEQUAL(pcm[2], 20000);
    CHECK_INTEQUAL(pcm[3], 20000);
    sound_adjust_volume(channel1, 0.2f, 4.0f/4000.0f);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], -18000);
    CHECK_INTEQUAL(pcm[1], -18000);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], -16000);
    CHECK_INTEQUAL(pcm[1], -16000);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], 14000);
    CHECK_INTEQUAL(pcm[1], 14000);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], 12000);
    CHECK_INTEQUAL(pcm[1], 12000);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], -12000);
    CHECK_INTEQUAL(pcm[1], -12000);
    sound_adjust_volume(channel2, 2.0f, 2.0f/4000.0f);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], -17000);
    CHECK_INTEQUAL(pcm[1], -17000);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], 22000);
    CHECK_INTEQUAL(pcm[1], 22000);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], 22000);
    CHECK_INTEQUAL(pcm[1], 22000);
    sound_adjust_volume(channel1, 1.2f, 0);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], -32000);
    CHECK_INTEQUAL(pcm[1], -32000);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], -32000);
    CHECK_INTEQUAL(pcm[1], -32000);
    get_pcm(pcm, 29);
    CHECK_INTEQUAL(pcm[0], 32000);
    CHECK_INTEQUAL(pcm[1], 32000);
    CHECK_INTEQUAL(pcm[54], -32000);
    CHECK_INTEQUAL(pcm[55], -32000);
    CHECK_INTEQUAL(pcm[56], 0);
    CHECK_INTEQUAL(pcm[57], 0);
    get_pcm(pcm, 1);  // Detect end-of-stream.

    /* Check that volume adjustments aren't carried over between sounds on
     * the same channel. */
    CHECK_INTEQUAL(sound_play(sound, 0, 1, 0, 0), channel1);
    CHECK_INTEQUAL(sound_play(sound, 0, 1, 0, 0), channel2);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], 20000);
    CHECK_INTEQUAL(pcm[1], 20000);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], 20000);
    CHECK_INTEQUAL(pcm[1], 20000);

    /* Check that volume adjustments on cut or faded channels have no
     * effect. */
    sound_cut(channel1);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], -10000);
    CHECK_INTEQUAL(pcm[1], -10000);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], -10000);
    CHECK_INTEQUAL(pcm[1], -10000);
    sound_adjust_volume(channel1, 1, 0);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    sound_fade(channel2, 2.0f/4000.0f);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], -5000);
    CHECK_INTEQUAL(pcm[1], -5000);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], 0);
    CHECK_INTEQUAL(pcm[1], 0);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], 0);
    CHECK_INTEQUAL(pcm[1], 0);
    sound_adjust_volume(channel2, 1, 0);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], 0);
    CHECK_INTEQUAL(pcm[1], 0);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], 0);
    CHECK_INTEQUAL(pcm[1], 0);

    /* Check invalid parameter handling (just that they don't misbehave). */
    CHECK_TRUE(channel1 = sound_play(sound, 0, 1, 0, 0));
    CHECK_TRUE(channel2 = sound_play(sound, 0, 1, 0, 0));
    /* Normally channel1 == 1 and channel2 == 2, but make this work no matter
     * which channels are used. */
    channel3 = 6 - channel1 - channel2;
    get_pcm(pcm, 2);
    CHECK_INTEQUAL(pcm[0], 20000);
    CHECK_INTEQUAL(pcm[1], 20000);
    CHECK_INTEQUAL(pcm[2], 20000);
    CHECK_INTEQUAL(pcm[3], 20000);
    sound_adjust_volume(0, 0, 0);
    sound_adjust_volume(channel3, 0, 0);
    sound_adjust_volume(4, 0, 0);
    sound_adjust_volume(channel1, -1, 0);
    sound_adjust_volume(channel2, 0, -1);
    get_pcm(pcm, 39);
    CHECK_INTEQUAL(pcm[0], -20000);
    CHECK_INTEQUAL(pcm[1], -20000);
    CHECK_INTEQUAL(pcm[74], -20000);
    CHECK_INTEQUAL(pcm[75], -20000);
    CHECK_INTEQUAL(pcm[76], 0);
    CHECK_INTEQUAL(pcm[77], 0);
    get_pcm(pcm, 1);  // Detect end-of-stream.

    sound_destroy(sound);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_pan)
{
    Sound *sound;
    int channel1, channel2, channel3;
    int16_t pcm[8];

    /* Check panning of a monaural sound. */
    CHECK_TRUE(sound = load_sound("testdata/sound/square.wav", 0));
    CHECK_TRUE(channel1 = sound_play(sound, 0, 1, 0, 0));
    get_pcm(pcm, 2);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    sound_set_pan(channel1, -0.5f);
    get_pcm(pcm, 4);
    CHECK_INTEQUAL(pcm[0], -15000);
    CHECK_INTEQUAL(pcm[1], -5000);
    CHECK_INTEQUAL(pcm[6], 15000);
    CHECK_INTEQUAL(pcm[7], 5000);
    sound_set_pan(channel1, +0.5f);
    get_pcm(pcm, 4);
    CHECK_INTEQUAL(pcm[0], -5000);
    CHECK_INTEQUAL(pcm[1], -15000);
    CHECK_INTEQUAL(pcm[6], 5000);
    CHECK_INTEQUAL(pcm[7], 15000);
    sound_set_pan(channel1, 0);
    get_pcm(pcm, 4);
    CHECK_INTEQUAL(pcm[0], -10000);
    CHECK_INTEQUAL(pcm[1], -10000);
    CHECK_INTEQUAL(pcm[6], 10000);
    CHECK_INTEQUAL(pcm[7], 10000);
    sound_cut(channel1);
    sound_destroy(sound);

    /* Check panning of a stereo sound. */
    CHECK_TRUE(sound = load_sound("testdata/sound/square-stereo.wav", 0));
    CHECK_TRUE(channel1 = sound_play(sound, 0, 1, 0, 0));
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    sound_set_pan(channel1, -0.6f);
    get_pcm(pcm, 2);
    CHECK_INTEQUAL(pcm[0], -10000);
    CHECK_INTEQUAL(pcm[1], -2500);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 2500);
    sound_set_pan(channel1, +0.6f);
    get_pcm(pcm, 2);
    CHECK_INTEQUAL(pcm[0], -2500);
    CHECK_INTEQUAL(pcm[1], -10000);
    CHECK_INTEQUAL(pcm[2], 2500);
    CHECK_INTEQUAL(pcm[3], 10000);
    sound_set_pan(channel1, 0);
    get_pcm(pcm, 2);
    CHECK_INTEQUAL(pcm[0], -10000);
    CHECK_INTEQUAL(pcm[1], -10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    sound_cut(channel1);
    sound_destroy(sound);

    /* Check invalid parameter handling (just that they don't misbehave). */
    CHECK_TRUE(sound = load_sound("testdata/sound/square.wav", 0));
    CHECK_TRUE(channel1 = sound_play(sound, 0, 1, 0, 0));
    CHECK_TRUE(channel2 = sound_play(sound, 0, 1, 0, 0));
    /* Normally channel1 == 1 and channel2 == 2, but make this work no matter
     * which channels are used. */
    channel3 = 6 - channel1 - channel2;
    get_pcm(pcm, 2);
    CHECK_INTEQUAL(pcm[0], 20000);
    CHECK_INTEQUAL(pcm[1], 20000);
    CHECK_INTEQUAL(pcm[2], 20000);
    CHECK_INTEQUAL(pcm[3], 20000);
    sound_set_pan(0, -0.5f);
    sound_set_pan(channel3, -0.5f);
    sound_set_pan(4, -0.5f);
    sound_set_pan(channel1, -1.1f);
    sound_set_pan(channel2, +1.1f);
    get_pcm(pcm, 4);
    CHECK_INTEQUAL(pcm[0], -20000);
    CHECK_INTEQUAL(pcm[1], -20000);
    CHECK_INTEQUAL(pcm[6], 20000);
    CHECK_INTEQUAL(pcm[7], 20000);
    sound_cut(channel1);
    sound_cut(channel2);
    sound_destroy(sound);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_playback_rate)
{
    Sound *sound;
    int channel;
    int16_t pcm[10];

    CHECK_TRUE(sound = load_sound("testdata/sound/square.wav", 0));
    CHECK_TRUE(channel = sound_play(sound, 0, 1, 0, 0));
    sound_set_playback_rate(channel, 0.5);
    get_pcm(pcm, 5);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], 10000);
    CHECK_INTEQUAL(pcm[5], 10000);
    CHECK_INTEQUAL(pcm[6], 0);
    CHECK_INTEQUAL(pcm[7], 0);
    CHECK_INTEQUAL(pcm[8], -10000);
    CHECK_INTEQUAL(pcm[9], -10000);
    sound_cut(channel);
    sound_destroy(sound);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_playback_rate_invalid)
{
    Sound *sound;
    int channel;
    int16_t pcm[10];

    CHECK_TRUE(sound = load_sound("testdata/sound/square.wav", 0));
    CHECK_INTEQUAL(sound_native_freq(sound), 4000);
    CHECK_TRUE(channel = sound_play(sound, 0, 1, 0, 0));
    sound_set_playback_rate(0, 4000);  // No effect.
    sound_set_playback_rate(channel, -1);  // No effect.
    sound_set_playback_rate(channel+1, 4000);  // No effect.
    sound_set_playback_rate(INT_MAX, 4000);  // No effect.
    get_pcm(pcm, 5);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], -10000);
    CHECK_INTEQUAL(pcm[5], -10000);
    CHECK_INTEQUAL(pcm[6], -10000);
    CHECK_INTEQUAL(pcm[7], -10000);
    CHECK_INTEQUAL(pcm[8], 10000);
    CHECK_INTEQUAL(pcm[9], 10000);
    sound_cut(channel);
    sound_destroy(sound);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_enable_loop)
{
    Sound *sound;
    int channel1, channel2, channel3;
    int16_t pcm[78];

    CHECK_TRUE(sound = load_sound("testdata/sound/square.wav", 0));
    sound_set_loop(sound, 1, 2);
    CHECK_TRUE(channel1 = sound_play(sound, 0, 1, 0, 0));

    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);

    sound_enable_loop(channel1, 1);
    get_pcm(pcm, 3);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], -10000);
    CHECK_INTEQUAL(pcm[3], -10000);
    CHECK_INTEQUAL(pcm[4], 10000);
    CHECK_INTEQUAL(pcm[5], 10000);

    sound_enable_loop(channel1, 0);
    get_pcm(pcm, 2);
    CHECK_INTEQUAL(pcm[0], -10000);
    CHECK_INTEQUAL(pcm[1], -10000);
    CHECK_INTEQUAL(pcm[2], -10000);
    CHECK_INTEQUAL(pcm[3], -10000);

    sound_enable_loop(channel1, 1);
    get_pcm(pcm, 36);
    CHECK_INTEQUAL(pcm[0], 10000);
    CHECK_INTEQUAL(pcm[1], 10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], -10000);
    CHECK_INTEQUAL(pcm[5], -10000);
    CHECK_INTEQUAL(pcm[6], -10000);
    CHECK_INTEQUAL(pcm[7], -10000);
    CHECK_INTEQUAL(pcm[8], 10000);
    CHECK_INTEQUAL(pcm[9], 10000);
    CHECK_INTEQUAL(pcm[70], -10000);
    CHECK_INTEQUAL(pcm[71], -10000);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], 0);
    CHECK_INTEQUAL(pcm[1], 0);
    CHECK_FALSE(sound_is_playing(channel1));

    /* Check invalid parameter handling (just that they don't misbehave). */
    CHECK_TRUE(channel1 = sound_play(sound, 0, 1, 0, 0));
    CHECK_TRUE(channel2 = sound_play(sound, 0, 1, 0, 0));
    /* Normally channel1 == 1 and channel2 == 2, but make this work no matter
     * which channels are used. */
    channel3 = 6 - channel1 - channel2;
    get_pcm(pcm, 2);
    CHECK_INTEQUAL(pcm[0], 20000);
    CHECK_INTEQUAL(pcm[1], 20000);
    CHECK_INTEQUAL(pcm[2], 20000);
    CHECK_INTEQUAL(pcm[3], 20000);
    sound_enable_loop(0, 1);
    sound_enable_loop(channel3, 1);
    sound_enable_loop(4, 1);
    get_pcm(pcm, 39);
    CHECK_INTEQUAL(pcm[0], -20000);
    CHECK_INTEQUAL(pcm[1], -20000);
    CHECK_INTEQUAL(pcm[74], -20000);
    CHECK_INTEQUAL(pcm[75], -20000);
    CHECK_INTEQUAL(pcm[76], 0);
    CHECK_INTEQUAL(pcm[77], 0);
    get_pcm(pcm, 1);  // Detect end-of-stream.

    sound_destroy(sound);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_filter)
{
    Sound *sound;
    int channel;
    int16_t pcm[12];

    CHECK_TRUE(sound = load_sound("testdata/sound/square.wav", 0));
    CHECK_TRUE(channel = sound_play(sound, 0, 1, 0, 1));

    /* Check that filters are applied to input audio data. */
    sound_set_filter(channel, dummy_filter_open());
    get_pcm(pcm, 3);
    CHECK_INTEQUAL(pcm[0], 100);
    CHECK_INTEQUAL(pcm[1], 100);
    CHECK_INTEQUAL(pcm[2], 200);
    CHECK_INTEQUAL(pcm[3], 200);
    CHECK_INTEQUAL(pcm[4], 300);
    CHECK_INTEQUAL(pcm[5], 300);

    /* Check that filters can be removed or replaced while a sound is
     * playing. */
    sound_set_filter(channel, NULL);
    get_pcm(pcm, 4);
    CHECK_INTEQUAL(pcm[0], -10000);
    CHECK_INTEQUAL(pcm[1], -10000);
    CHECK_INTEQUAL(pcm[2], 10000);
    CHECK_INTEQUAL(pcm[3], 10000);
    CHECK_INTEQUAL(pcm[4], 10000);
    CHECK_INTEQUAL(pcm[5], 10000);
    CHECK_INTEQUAL(pcm[6], -10000);
    CHECK_INTEQUAL(pcm[7], -10000);
    sound_set_filter(channel, dummy_filter_open());
    get_pcm(pcm, 4);
    CHECK_INTEQUAL(pcm[0], 100);
    CHECK_INTEQUAL(pcm[1], 100);
    CHECK_INTEQUAL(pcm[2], 200);
    CHECK_INTEQUAL(pcm[3], 200);
    CHECK_INTEQUAL(pcm[4], 300);
    CHECK_INTEQUAL(pcm[5], 300);
    CHECK_INTEQUAL(pcm[6], 400);
    CHECK_INTEQUAL(pcm[7], 400);
    sound_set_filter(channel, dummy_filter_open());
    get_pcm(pcm, 4);
    CHECK_INTEQUAL(pcm[0], 100);
    CHECK_INTEQUAL(pcm[1], 100);
    CHECK_INTEQUAL(pcm[2], 200);
    CHECK_INTEQUAL(pcm[3], 200);
    CHECK_INTEQUAL(pcm[4], 300);
    CHECK_INTEQUAL(pcm[5], 300);
    CHECK_INTEQUAL(pcm[6], 400);
    CHECK_INTEQUAL(pcm[7], 400);

    /* Check invalid parameter handling. */
    sound_set_filter(0, dummy_filter_open());
    sound_set_filter(channel==1 ? 2 : 1, dummy_filter_open());
    sound_set_filter(4, dummy_filter_open());

    /* Check that end-of-data from a filter is handled properly. */
    get_pcm(pcm, 6);
    CHECK_INTEQUAL(pcm[10], 1000);
    CHECK_INTEQUAL(pcm[11], 1000);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], 0);
    CHECK_INTEQUAL(pcm[1], 0);
    CHECK_FALSE(sound_is_playing(channel));

    sound_destroy(sound);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_filter_locking)
{
    Sound *sound;
    int channel;
    int16_t pcm[2];
    int filter_thread, set_thread;

    ASSERT(filter_mutex = mutex_create(MUTEX_SIMPLE, MUTEX_UNLOCKED));
    CHECK_TRUE(sound = load_sound("testdata/sound/square.wav", 0));
    CHECK_TRUE(channel = sound_play(sound, 0, 1, 0, 1));
    filter_running = 0;
    sound_set_filter(channel, dummy_filter_open());

    /* Check that the filter function deals with the mutex properly
     * (just a self-test). */
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], 100);
    CHECK_INTEQUAL(pcm[1], 100);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], 200);
    CHECK_INTEQUAL(pcm[1], 200);

    /* Check that the filter function blocks sound_set_filter() calls. */
    mutex_lock(filter_mutex);
    ASSERT(filter_thread = thread_create(get_pcm_thread, pcm));
    while (!filter_running) {
        thread_yield();
    }
    sound_core_blocked_on_filter_lock = 0;
    if (!(set_thread = thread_create(set_filter_thread,
                                     (void *)(intptr_t)channel))) {
        /* Clean up properly here to avoid leaving a stuck thread. */
        mutex_unlock(filter_mutex);
        thread_wait(filter_thread);
        FAIL("Failed to create set_filter thread");
    }
    /* If the locking logic is broken, this may never terminate. */
    while (!sound_core_blocked_on_filter_lock) {
        thread_yield();
    }
    mutex_unlock(filter_mutex);
    thread_wait(filter_thread);
    thread_wait(set_thread);
    /* The old filter was in place when the filter routine was called, so
     * this sample will used the old filter's data. */
    CHECK_INTEQUAL(pcm[0], 300);
    CHECK_INTEQUAL(pcm[1], 300);
    /* But the next sample should use the new filter. */
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], 100);
    CHECK_INTEQUAL(pcm[1], 100);

    /* Check that sound_set_filter() blocks the filter function. */
    filter_mutex_lock_on_close = 1;
    filter_mutex_locked_by_close = 0;
    ASSERT(set_thread = thread_create(set_filter_thread,
                                      (void *)(intptr_t)channel));
    while (!filter_mutex_locked_by_close) {
        thread_yield();
    }
    sound_core_blocked_on_filter_lock = 0;
    if (!(filter_thread = thread_create(get_pcm_thread, pcm))) {
        /* As above, clean up properly to avoid leaving a stuck thread. */
        mutex_unlock(filter_mutex);
        thread_wait(filter_thread);
        FAIL("Failed to create set_filter thread");
    }
    /* As above, if the locking logic is broken, this may never terminate. */
    while (!sound_core_blocked_on_filter_lock) {
        thread_yield();
    }
    filter_mutex_lock_on_close = 0;
    thread_wait(set_thread);
    thread_wait(filter_thread);
    filter_mutex_lock_on_close = 0;
    /* The set-filter thread grabbed the lock first, so the filter should
     * have already been changed before the second thread reads any samples. */
    CHECK_INTEQUAL(pcm[0], 100);
    CHECK_INTEQUAL(pcm[1], 100);
    get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], 200);
    CHECK_INTEQUAL(pcm[1], 200);

    sound_destroy(sound);
    mutex_destroy(filter_mutex);
    filter_mutex = 0;
    return 1;
}

/*************************************************************************/
/*************************************************************************/
