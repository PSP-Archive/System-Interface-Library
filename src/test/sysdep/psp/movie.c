/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/psp/movie.c: Tests for PSP-specific movie playback code.
 */

#include "src/base.h"
#include "src/graphics.h"
#include "src/math.h"
#include "src/movie.h"
#include "src/sound.h"
#include "src/sound/mixer.h"  // For sound_mixer_get_pcm().
#include "src/sysdep.h"
#include "src/sysdep/test.h"
#include "src/test/base.h"
#include "src/test/graphics/internal.h"
#include "src/test/movie/internal.h"
#include "src/time.h"

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

static int do_test_psp_movie(void);

int test_psp_movie(void)
{
    return run_tests_in_window(do_test_psp_movie);
}

DEFINE_GENERIC_TEST_RUNNER(do_test_psp_movie)

/*-----------------------------------------------------------------------*/

TEST_INIT(init)
{
    CHECK_TRUE(sys_file_init());
    /* Prime any statically-allocated low-level resources (e.g. ioqueue)
     * so CHECK_MEMORY_FAILURES doesn't report spurious leaks. */
    SysFile *fh;
    char path[4096+20];
    ASSERT(sys_get_resource_path_prefix(path, sizeof(path))
           < (int)sizeof(path));
    ASSERT(strformat_check(path+strlen(path), sizeof(path)-strlen(path),
                           "testdata/test.txt"));
    ASSERT(fh = sys_file_open(path));
    char buf[1];
    int req;
    ASSERT(req = sys_file_read_async(fh, buf, 1, 0, -1));
    ASSERT(sys_file_wait_async(req) == 1);
    sys_file_close(fh);

    sys_test_sound_set_output_rate(44100);
    sound_init();
    CHECK_TRUE(sound_open_device("", 2));

    time_init();

    graphics_start_frame();
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
    if (strcmp(CURRENT_TEST_NAME(), "test_core") == 0) {
        return 1;
    }

    graphics_finish_frame();
    graphics_flush_resources();
    sound_cleanup();
    sys_file_cleanup();
    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_direct_render)
{
    int movie;
    CHECK_TRUE(movie = psp_movie_open_direct("testdata/movie/test.str", 0));

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
            FAIL("movie_update() failed for frame %d", frame);
        }
        if (frame < 15 && !check_video_frame(frame, 1, 0)) {
            FAIL("check_video_frame() failed for frame %d", frame);
        }
        CHECK_TRUE(check_audio_frame(frame, 1, 1.0f, 0));
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

/*-----------------------------------------------------------------------*/

TEST(test_direct_render_memory_failure)
{
    int movie;
    CHECK_MEMORY_FAILURES(
        movie = psp_movie_open_direct("testdata/movie/test.str", 0));

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
            FAIL("movie_update() failed for frame %d", frame);
        }
        if (frame < 15 && !check_video_frame(frame, 1, 0)) {
            FAIL("check_video_frame() failed for frame %d", frame);
        }
        CHECK_TRUE(check_audio_frame(frame, 1, 1.0f, 0));
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
