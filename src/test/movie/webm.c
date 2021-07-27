/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/movie/webm.c: Movie functionality tests specific to WebM
 * software decoding.
 */

#include "src/base.h"
#include "src/graphics.h"
#include "src/math.h"
#include "src/movie.h"
#include "src/movie/webm.h"
#include "src/sound.h"
#include "src/sound/mixer.h"  // For sound_mixer_get_pcm().
#include "src/sysdep.h"
#include "src/sysdep/test.h"
#include "src/test/base.h"
#include "src/test/graphics/internal.h"
#include "src/test/movie/internal.h"
#include "src/texture.h"
#include "src/time.h"

#ifndef SIL_MOVIE_INCLUDE_WEBM
int test_movie_webm(void) {
    DLOG("WebM support disabled, nothing to test.");
    return 1;
}
#else  // To the end of the file.

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

static int do_test_movie_webm(void);
int test_movie_webm(void)
{
    return run_tests_in_window(do_test_movie_webm);
}

DEFINE_GENERIC_TEST_RUNNER(do_test_movie_webm)

TEST_INIT(init)
{
    CHECK_TRUE(sys_file_init());
    sys_test_sound_set_output_rate(44100);
    sound_init();
    CHECK_TRUE(sound_open_device("", 2));

    /* For movie_draw(). */
    time_init();
    graphics_set_viewport(
        0, 0, graphics_display_width(), graphics_display_height());
    graphics_set_parallel_projection(
        0, graphics_display_width(), graphics_display_height(), 0, -1, 1);
    Matrix4f view = mat4_identity;
    view._41 = graphics_display_width()/2 - MOVIE_WIDTH/2;
    view._42 = graphics_display_height()/2 - MOVIE_HEIGHT/2;
    graphics_set_view_matrix(&view);
    graphics_set_model_matrix(&mat4_identity);

    return 1;
}

TEST_CLEANUP(cleanup)
{
    graphics_flush_resources();
    sound_cleanup();
    sys_file_cleanup();
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_play_sound_decoder_failure)
{
    int movie;
    CHECK_TRUE(movie = movie_open("testdata/movie/test.webm", 0, 0));

    /* The movie will have reserved sound channel 1 for playback.  We free
     * it here so the sound_play_decoder() call in core.c/movie_play()
     * will fail. */
    sound_free_channel(1);
    CHECK_FALSE(movie_play(movie));

    movie_close(movie);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_draw_texture_lock_failure)
{
    int movie;
    CHECK_TRUE(movie = movie_open("testdata/movie/test.webm", 0, 0));
    CHECK_TRUE(movie_play(movie));

    /* The movie will have created a texture with ID 1 for the video image.
     * We lock it here so the lock call in core.c/movie_update() will fail. */
    ASSERT(texture_lock(1));
    CHECK_FALSE(movie_update(movie));

    movie_close(movie);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_play_audio_end_of_stream)
{
    int movie;
    CHECK_TRUE(movie = movie_open("testdata/movie/test.webm", 0, 0));

    CHECK_TRUE(movie_play(movie));
    const int audio_length = (44100/30) * 20
        + 1000;  // There will be a few extra samples at the end of the stream.
    int16_t pcm[1024*2];
    for (int samples = 0; samples < audio_length; samples += lenof(pcm)/2) {
        sound_mixer_get_pcm(pcm, lenof(pcm)/2);
    }
    /* We can't directly check whether audio playback has terminated, so
     * here we just check that the audio output is silent, and we rely on
     * branch coverage data to determine that the end-of-stream branch
     * has actually been taken. */
    sound_mixer_get_pcm(pcm, 1);
    CHECK_INTEQUAL(pcm[0], 0);
    CHECK_INTEQUAL(pcm[1], 0);

    /* Video playback should keep going even after the audio is done. */
    CHECK_TRUE(movie_is_playing(movie));

    movie_close(movie);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_play_audio_float_overflow)
{
    int movie;
    CHECK_TRUE(movie = movie_open("testdata/movie/test-overflow.webm", 0, 0));

    CHECK_TRUE(movie_play(movie));
    const int audio_length = (44100/30) * 20;
    int16_t pcm[100*2];
    for (int samples = 0; samples < audio_length; samples += lenof(pcm)/2) {
        sound_mixer_get_pcm(pcm, lenof(pcm)/2);
        for (int i = 0; i < lenof(pcm); i++) {
            if (i < lenof(pcm)/2) {
                if (pcm[i] <= 0) {
                    FAIL("Audio sample %d was %d but should have been"
                         " positive", samples + i/2, pcm[i]);
                }
            } else {
                if (pcm[i] >= 0) {
                    FAIL("Audio sample %d was %d but should have been"
                         " negative", samples + i/2, pcm[i]);
                }
            }
        }
    }

    movie_close(movie);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_play_broken_video)
{
    int movie;
    CHECK_TRUE(movie = movie_open("testdata/movie/broken-video.webm", 0, 0));

    CHECK_TRUE(movie_play(movie));
    CHECK_FALSE(movie_next_frame(movie));  // Frame data is corrupt.
    CHECK_FALSE(movie_is_playing(movie));  // Error should terminate playback.

    movie_close(movie);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_play_audio_memory_failures)
{
    WebMDecodeHandle *handle;
    int16_t pcm[2];

    /* We don't use CHECK_MEMORY_FAILURES here because a memory failure
     * during audio decoding will trigger end-of-stream, so we need to
     * set up and tear down the movie object on every iteration, and that
     * would be fairly awkward to write in a single macro.  Instead, we
     * loop on the get_audio call until success and rely on the leak check
     * for the test itself to catch memory leaks. */
    for (int try = 0; ; try++) {
        if (try >= 100) {
            FAIL("movie_webm_get_audio(handle, pcm, lenof(pcm)) did not"
                 " succeed after %d iterations", try);
        }
        SysFile *fh;
        char path[4096+30];
        ASSERT(sys_get_resource_path_prefix(path, sizeof(path))
               < (int)sizeof(path));
        ASSERT(strformat_check(path+strlen(path), sizeof(path)-strlen(path),
                               "testdata/movie/test-mono.webm"));
        ASSERT(fh = sys_file_open(path));
        CHECK_TRUE(handle = movie_webm_open(fh, 0, sys_file_size(fh), 0));
        CHECK_INTEQUAL(movie_webm_audio_channels(handle), 1);

        TEST_mem_fail_after(try, 1, 0);
        const int result = movie_webm_get_audio(handle, pcm, lenof(pcm));
        TEST_mem_fail_after(-1, 0, 0);
        if (result) {
            break;
        }
        movie_webm_close(handle);
    }

    CHECK_INTRANGE(pcm[0], 0x40, 0x50);
    CHECK_INTRANGE(pcm[1], 0x2AC, 0x2BC);

    /* A subsequent get_audio call should still succeed, though it will
     * have skipped data. */
    CHECK_INTEQUAL(movie_webm_get_audio(handle, pcm, lenof(pcm)), lenof(pcm));

    movie_webm_close(handle);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_play_vp9)
{
#ifdef SIL_ARCH_ARM
    /* The ARM NEON-optimized VP9 decoder in libvpx-1.4.0 and later
     * (through at least libvpx-1.5.0) crashes due to unaligned data access,
     * so it's disabled and this open should fail.  See also the notes in
     * build/common/sources.mk and build/common/rules.mk. */
    CHECK_FALSE(movie_open("testdata/movie/test-vp9-nosound.webm", 0, 0));
    return 1;
#endif

    int movie;
    CHECK_TRUE(movie = movie_open("testdata/movie/test-vp9-nosound.webm",
                                  0, 0));
    CHECK_DOUBLEEQUAL(movie_framerate(movie), MOVIE_FRAMERATE);

    sys_test_time_set(0);
    CHECK_TRUE(movie_play(movie));
    double next_frame_time = 0;
    for (int frame = 0; frame < MOVIE_FRAMES;
         frame++, next_frame_time += 1.0 / MOVIE_FRAMERATE)
    {
        graphics_clear(0, 0, 0, 0, 1, 0);
        if (!movie_is_playing(movie)) {
            FAIL("movie_is_playing() was not true for frame %d", frame);
        }
        sys_test_time_set(iceilf(next_frame_time * sys_time_unit()));
        if (!movie_update(movie)) {
            FAIL("movie_draw() failed for frame %d", frame);
        }
        movie_draw(movie);
        if (frame < 15 && !check_video_frame(frame, 1, 0)) {
            FAIL("check_video_frame() failed for frame %d", frame);
        }
        graphics_finish_frame();
        graphics_start_frame();
    }
    CHECK_TRUE(movie_is_playing(movie));
    next_frame_time += 1.0 / MOVIE_FRAMERATE;
    sys_test_time_set(iceilf(next_frame_time * sys_time_unit()));
    CHECK_FALSE(movie_update(movie));
    CHECK_FALSE(movie_is_playing(movie));

    movie_close(movie);
    return 1;
}

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_MOVIE_INCLUDE_WEBM
