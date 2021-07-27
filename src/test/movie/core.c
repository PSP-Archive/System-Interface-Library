/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/movie/core.c: Tests for core movie playback functionality.
 */

#include "src/base.h"
#include "src/graphics.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/movie.h"
#include "src/resource.h"
#include "src/sound.h"
#include "src/sound/mixer.h"  // For sound_mixer_get_pcm().
#include "src/sysdep.h"
#include "src/sysdep/test.h"
#include "src/test/base.h"
#include "src/test/graphics/internal.h"
#include "src/test/movie/internal.h"
#include "src/texture.h"
#include "src/time.h"

/*************************************************************************/
/******************************* Test data *******************************/
/*************************************************************************/

/* Movie ID guaranteed to be invalid across all tests. */
#define INVALID_MOVIE  10000

/* Filename extension to use with movie pathnames (overrides the system
 * default).  Used for testing system-specific movie support when WebM
 * software decoding is also enabled; see test_movie_core_with_extension(). */
static const char *override_extension;

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

/**
 * movie_extension:  Return the filename extension (.webm, etc.) to use
 * for movie files in the current runtime environment.
 */
static const char *movie_extension(void)
{
    if (override_extension) {
        return override_extension;
    }
#if defined(SIL_MOVIE_INCLUDE_WEBM)
    return ".webm";
#elif defined(SIL_PLATFORM_PSP)
    return ".str";
#else
    return ".webm";
#endif
}

/*-----------------------------------------------------------------------*/

/**
 * wrap_movie_open:  Wrap the movie_open() call by rewriting calls with
 * add_extension set if an extension override is in place.
 */
static int wrap_movie_open(const char *path, int add_extension,
                           int smooth_chroma)
{
    if (add_extension && override_extension) {
        char buf[100];
        ASSERT(strformat_check(buf, sizeof(buf),
                               "%s%s", path, override_extension));
        return movie_open(buf, 0, smooth_chroma);
    } else {
        return movie_open(path, add_extension, smooth_chroma);
    }
}

/*-----------------------------------------------------------------------*/

/**
 * using_internal_webm:  Return whether movie playback will use the
 * built-in software WebM decoder.
 */
static int using_internal_webm(void)
{
#ifdef SIL_MOVIE_INCLUDE_WEBM
    return !override_extension || strcmp(override_extension, ".webm") == 0;
#else
    return 0;
#endif
}

/*-----------------------------------------------------------------------*/

/**
 * can_play_mono_movies:  Return whether the current runtime environment
 * supports movie files with single-channel (monaural) audio.
 */
static int can_play_mono_movies(void)
{
    if (using_internal_webm()) {
        return 1;
    }
#if defined(SIL_PLATFORM_PSP)
    return 0;
#else
    return 1;
#endif
}

/*-----------------------------------------------------------------------*/

/**
 * can_play_silent_movies:  Return whether the current runtime environment
 * supports movie files with no audio.
 */
static int can_play_silent_movies(void)
{
    if (using_internal_webm()) {
        return 1;
    }
#if defined(SIL_PLATFORM_PSP)
    return 0;
#else
    return 1;
#endif
}

/*-----------------------------------------------------------------------*/

/**
 * can_smooth_chroma:  Return whether the current runtime environment
 * supports linear interpolation when upsampling chroma data.
 */
static int can_smooth_chroma(void)
{
    if (using_internal_webm()) {
        return 1;
    }
#if defined(SIL_PLATFORM_PSP)
    return 0;
#else
    return 1;
#endif
}

/*-----------------------------------------------------------------------*/

/**
 * num_invalid_audio_samples:  Return the number of audio samples at the
 * beginning of a movie which do not contain valid data.
 */
static int num_invalid_audio_samples(void)
{
    if (using_internal_webm()) {
        return 0;
    }
#if defined(SIL_PLATFORM_LINUX)
    return 1088;  // Needed for AAC audio.
#else
    return 0;
#endif
}

/*-----------------------------------------------------------------------*/

/**
 * draw_from_texture:  Draw a movie frame by retrieving its texture and
 * rendering the texture to the screen.
 *
 * [Parameters]
 *     movie: Movie ID.
 * [Return value]
 *     True on success, false on error.
 */
static int draw_from_texture(int movie)
{
    float tex_left, tex_right, tex_top, tex_bottom;
    const int texture = movie_get_texture(
        movie, &tex_left, &tex_right, &tex_top, &tex_bottom);
    if (!texture) {
        DLOG("movie_get_texture() didn't return a texture");
        return 0;
    }

    texture_set_repeat(texture, 0, 0);
    texture_set_antialias(texture, 0);
    texture_apply(0, texture);
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    graphics_add_vertex(
        &(Vector3f){0, 0, 0},
        &(Vector2f){tex_left, tex_top}, NULL);
    graphics_add_vertex(
        &(Vector3f){MOVIE_WIDTH, 0, 0},
        &(Vector2f){tex_right, tex_top}, NULL);
    graphics_add_vertex(
        &(Vector3f){MOVIE_WIDTH, MOVIE_HEIGHT, 0},
        &(Vector2f){tex_right, tex_bottom}, NULL);
    graphics_add_vertex(
        &(Vector3f){0, MOVIE_HEIGHT, 0},
        &(Vector2f){tex_left, tex_bottom}, NULL);
    ASSERT(graphics_end_and_draw_primitive());
    texture_apply(0, 0);

    return 1;
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

static int do_test_movie_core(void);
static int wrap_test_movie_core(void);

int test_movie_core(void)
{
    override_extension = NULL;
    return run_tests_in_window(wrap_test_movie_core);
}

int test_movie_core_with_extension(const char *extension)
{
    override_extension = extension;
    const int result = wrap_test_movie_core();
    override_extension = NULL;
    return result;
}

static int wrap_test_movie_core(void)
{
    if (!using_internal_webm()) {
#if defined(SIL_PLATFORM_ANDROID) || (defined(SIL_PLATFORM_LINUX) && !defined(SIL_PLATFORM_LINUX_USE_FFMPEG)) || defined(SIL_PLATFORM_WINDOWS)
        SKIP("Movie support not available.");
#endif
    }
    return do_test_movie_core();
}

DEFINE_GENERIC_TEST_RUNNER(do_test_movie_core)

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

    /* run_tests_in_window() sets up a TESTW x TESTH viewport, but
     * movie_draw() expects to draw an image centered in the entire display,
     * so reset the viewport. */
    graphics_set_viewport(
        0, 0, graphics_display_width(), graphics_display_height());

    graphics_set_parallel_projection(
        0, graphics_display_width(), graphics_display_height(), 0, -1, 1);
    Matrix4f view = mat4_identity;
    view._41 = graphics_display_width()/2 - MOVIE_WIDTH/2;
    view._42 = graphics_display_height()/2 - MOVIE_HEIGHT/2;
    graphics_set_view_matrix(&view);
    graphics_set_model_matrix(&mat4_identity);

    graphics_start_frame();
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
    graphics_finish_frame();
    graphics_flush_resources();
    sound_cleanup();
    sys_file_cleanup();
    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_play_movie)
{
    char path[100];
    ASSERT(strformat_check(path, sizeof(path),
                           "testdata/movie/test%s", movie_extension()));

    int movie;
    CHECK_TRUE(movie = wrap_movie_open(path, 0, 0));
    CHECK_DOUBLEEQUAL(movie_framerate(movie), MOVIE_FRAMERATE);

    CHECK_TRUE(movie_play(movie));
    for (int frame = 0; frame < MOVIE_FRAMES; frame++) {
        graphics_clear(0, 0, 0, 0, 1, 0);
        if (!movie_is_playing(movie)) {
            FAIL("movie_is_playing() was not true for frame %d", frame);
        }
        if (!movie_next_frame(movie)) {
            FAIL("movie_next_frame() failed for frame %d", frame);
        }
        if (!draw_from_texture(movie)) {
            FAIL("draw_from_movie() failed for frame %d", frame);
        }
        if (frame < 15 && !check_video_frame(frame, 1, 0)) {
            FAIL("check_video_frame() failed for frame %d", frame);
        }
        CHECK_TRUE(check_audio_frame(frame, 1, 1.0f,
                                     num_invalid_audio_samples()));
        graphics_finish_frame();
        graphics_start_frame();
    }
    CHECK_TRUE(movie_is_playing(movie));
    /* We reached the end of the movie. */
    CHECK_FALSE(movie_next_frame(movie));
    CHECK_FALSE(movie_is_playing(movie));
    /* Check that things don't break if we try to continue playing anyway. */
    CHECK_FALSE(movie_next_frame(movie));
    CHECK_FALSE(movie_is_playing(movie));

    movie_close(movie);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_play_movie_auto_extension)
{
    int movie;
    CHECK_TRUE(movie = wrap_movie_open("testdata/movie/test", 1, 0));
    CHECK_DOUBLEEQUAL(movie_framerate(movie), MOVIE_FRAMERATE);

    CHECK_TRUE(movie_play(movie));
    for (int frame = 0; frame < MOVIE_FRAMES; frame++) {
        graphics_clear(0, 0, 0, 0, 1, 0);
        if (!movie_is_playing(movie)) {
            FAIL("movie_is_playing() was not true for frame %d", frame);
        }
        if (!movie_next_frame(movie)) {
            FAIL("movie_next_frame() failed for frame %d", frame);
        }
        if (!draw_from_texture(movie)) {
            FAIL("draw_from_movie() failed for frame %d", frame);
        }
        if (frame < 15 && !check_video_frame(frame, 0, 0)) {
            FAIL("check_video_frame(0) failed for frame %d", frame);
        }
        CHECK_TRUE(check_audio_frame(frame, 1, 1.0f,
                                     num_invalid_audio_samples()));
        graphics_finish_frame();
        graphics_start_frame();
    }
    CHECK_TRUE(movie_is_playing(movie));
    CHECK_FALSE(movie_next_frame(movie));
    CHECK_FALSE(movie_is_playing(movie));

    movie_close(movie);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_play_movie_memory_failure)
{
    int movie;
    /* When using the software WebM decoder, the demuxer/decoder libraries
     * will attempt to make many (>1000) allocations, so we need to iterate
     * farther than usual. */
    CHECK_MEMORY_FAILURES_TO(
        2000, ((movie = wrap_movie_open("testdata/movie/test", 1, 0))
               || (graphics_flush_resources(), 0)));
    CHECK_DOUBLEEQUAL(movie_framerate(movie), MOVIE_FRAMERATE);

    for (int frame = 0; frame < MOVIE_FRAMES; frame++) {
        graphics_clear(0, 0, 0, 0, 1, 0);
        if (frame == 0) {
            CHECK_MEMORY_FAILURES(
                (movie_play(movie) && movie_next_frame(movie))
                || (TEST_mem_fail_after(-1, 0, 0),
                    movie_close(movie),
                    graphics_flush_resources(),
                    movie = wrap_movie_open("testdata/movie/test", 1, 0),
                    0));
        } else {
            if (!movie_is_playing(movie)) {
                FAIL("movie_is_playing() was not true for frame %d", frame);
            }
            if (!movie_next_frame(movie)) {
                FAIL("movie_next_frame() failed for frame %d", frame);
            }
        }
        if (!draw_from_texture(movie)) {
            FAIL("draw_from_movie() failed for frame %d", frame);
        }
        if (frame < 15 && !check_video_frame(frame, 0, 0)) {
            FAIL("check_video_frame(0) failed for frame %d", frame);
        }
        CHECK_TRUE(check_audio_frame(frame, 1, 1.0f,
                                     num_invalid_audio_samples()));
        graphics_finish_frame();
        graphics_start_frame();
    }
    CHECK_TRUE(movie_is_playing(movie));
    CHECK_FALSE(movie_next_frame(movie));
    CHECK_FALSE(movie_is_playing(movie));

    movie_close(movie);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_no_sound_channel)
{
    while (sound_reserve_channel()) {
        /* spin */;
    }

    int movie;
    CHECK_FALSE((movie = wrap_movie_open("testdata/movie/test", 1, 0))
                && movie_play(movie));

    movie_close(movie);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_nonexistent)
{
    CHECK_FALSE(wrap_movie_open("testdata/movie/test", 0, 0));
    CHECK_FALSE(wrap_movie_open("testdata/movie/nonexistent", 1, 0));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_empty_pathname)
{
    CHECK_FALSE(wrap_movie_open("", 0, 0));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_bad_data)
{
    CHECK_FALSE(wrap_movie_open("testdata/test.txt", 0, 0));
    CHECK_FALSE(wrap_movie_open("testdata/sound/long.dat", 0, 0));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_invalid)
{
    CHECK_FALSE(wrap_movie_open(NULL, 0, 0));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_close_null)
{
    movie_close(0);  // Just make sure it doesn't crash.
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_close_invalid)
{
    int movie;
    CHECK_TRUE(movie = wrap_movie_open("testdata/movie/test", 1, 0));
    movie_close(movie);

    movie_close(movie);
    movie_close(INVALID_MOVIE);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_framerate_invalid)
{
    int movie;
    CHECK_TRUE(movie = wrap_movie_open("testdata/movie/test", 1, 0));
    movie_close(movie);

    CHECK_DOUBLEEQUAL(movie_framerate(0), 0);
    CHECK_DOUBLEEQUAL(movie_framerate(movie), 0);
    CHECK_DOUBLEEQUAL(movie_framerate(INVALID_MOVIE), 0);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_volume)
{
    int movie;
    CHECK_TRUE(movie = wrap_movie_open("testdata/movie/test", 1, 0));

    movie_set_volume(movie, 0.5);
    CHECK_TRUE(movie_play(movie));
    for (int frame = 0; frame < MOVIE_FRAMES; frame++) {
        graphics_clear(0, 0, 0, 0, 1, 0);
        if (!movie_next_frame(movie)) {
            FAIL("movie_next_frame() failed for frame %d", frame);
        }
        CHECK_TRUE(check_audio_frame(frame, 1, 0.5f,
                                     num_invalid_audio_samples()));
    }
    CHECK_FALSE(movie_next_frame(movie));

    movie_close(movie);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_volume_invalid)
{
    int movie;
    CHECK_TRUE(movie = wrap_movie_open("testdata/movie/test", 1, 0));

    movie_set_volume(movie, -1);
    movie_set_volume(0, 0.5);
    movie_set_volume(INVALID_MOVIE, 0.5);
    CHECK_TRUE(movie_play(movie));
    for (int frame = 0; frame < MOVIE_FRAMES; frame++) {
        if (!movie_next_frame(movie)) {
            FAIL("movie_next_frame() failed for frame %d", frame);
        }
        CHECK_TRUE(check_audio_frame(frame, 1, 1.0f,
                                     num_invalid_audio_samples()));
    }
    CHECK_FALSE(movie_next_frame(movie));

    movie_close(movie);
    movie_set_volume(movie, 0.5);  // Didn't check this case above.
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_play_while_playing)
{
    int movie;
    CHECK_TRUE(movie = wrap_movie_open("testdata/movie/test", 1, 0));

    CHECK_TRUE(movie_play(movie));
    for (int frame = 0; frame < MOVIE_FRAMES; frame++) {
        if (frame == 5) {
            CHECK_TRUE(movie_play(movie));  // Should have no effect.
        }
        graphics_clear(0, 0, 0, 0, 1, 0);
        if (!movie_next_frame(movie)) {
            FAIL("movie_next_frame() failed for frame %d", frame);
        }
        if (!draw_from_texture(movie)) {
            FAIL("draw_from_movie() failed for frame %d", frame);
        }
        if (frame < 15 && !check_video_frame(frame, 0, 0)) {
            FAIL("check_video_frame(0) failed for frame %d", frame);
        }
        CHECK_TRUE(check_audio_frame(frame, 1, 1.0f,
                                     num_invalid_audio_samples()));
        graphics_finish_frame();
        graphics_start_frame();
    }
    CHECK_FALSE(movie_next_frame(movie));

    movie_close(movie);
    movie_stop(movie);  // Didn't check this case above.
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_play_invalid)
{
    int movie;
    CHECK_TRUE(movie = wrap_movie_open("testdata/movie/test", 1, 0));
    movie_close(movie);

    CHECK_FALSE(movie_play(0));
    CHECK_FALSE(movie_play(movie));
    CHECK_FALSE(movie_play(INVALID_MOVIE));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_stop)
{
    int16_t pcm[MOVIE_SAMPLES_PER_FRAME*2];

    int movie;
    CHECK_TRUE(movie = wrap_movie_open("testdata/movie/test", 1, 0));

    CHECK_TRUE(movie_play(movie));

    for (int frame = 0; frame < 5; frame++) {
        graphics_clear(0, 0, 0, 0, 1, 0);
        if (!movie_next_frame(movie)) {
            FAIL("movie_next_frame() failed for frame %d", frame);
        }
        if (!draw_from_texture(movie)) {
            FAIL("draw_from_movie() failed for frame %d", frame);
        }
        if (!check_video_frame(frame, 0, 0)) {
            FAIL("check_video_frame(0) failed for frame %d", frame);
        }
        CHECK_TRUE(check_audio_frame(frame, 1, 1.0f,
                                     num_invalid_audio_samples()));
        graphics_finish_frame();
        graphics_start_frame();
    }

    movie_stop(movie);
    CHECK_FALSE(movie_next_frame(movie));
    sound_update();
    sound_mixer_get_pcm(pcm, lenof(pcm)/2);
    for (int i = 0; i < lenof(pcm); i++) {
        CHECK_INTEQUAL(pcm[i], 0);
    }

    movie_stop(movie);  // A second call should have no effect.
    CHECK_FALSE(movie_next_frame(movie));
    sound_update();
    sound_mixer_get_pcm(pcm, lenof(pcm)/2);
    for (int i = 0; i < lenof(pcm); i++) {
        CHECK_INTEQUAL(pcm[i], 0);
    }

    CHECK_TRUE(movie_play(movie));
    for (int frame = 5; frame < MOVIE_FRAMES; frame++) {
        graphics_clear(0, 0, 0, 0, 1, 0);
        if (!movie_next_frame(movie)) {
            FAIL("movie_next_frame() failed for frame %d", frame);
        }
        if (!draw_from_texture(movie)) {
            FAIL("draw_from_movie() failed for frame %d", frame);
        }
        if (frame < 15 && !check_video_frame(frame, 0, 0)) {
            FAIL("check_video_frame(0) failed for frame %d", frame);
        }
        CHECK_TRUE(check_audio_frame(frame, 1, 1.0f,
                                     num_invalid_audio_samples()));
        graphics_finish_frame();
        graphics_start_frame();
    }
    CHECK_FALSE(movie_next_frame(movie));

    movie_close(movie);
    movie_stop(movie);  // Didn't check this case above.
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_stop_on_close)
{
    /* Test for a former bug in which movie_close() would leave the sound
     * channel in "playing" status, leading to use-after-free if the movie
     * was closed while audio was still playing. */

    /* Can't reliably check this unless we're using the WebM decoder. */
    if (!using_internal_webm()) {
        SKIP("Test not supported for this configuration.");
    }

    int movie;
    CHECK_TRUE(movie = wrap_movie_open("testdata/movie/test", 1, 0));

    CHECK_TRUE(movie_play(movie));
    CHECK_TRUE(sound_is_playing(1));

    movie_close(movie);
    CHECK_FALSE(sound_is_playing(1));
    int channel;
    CHECK_INTEQUAL(channel = sound_reserve_channel(), 1);
    sound_free_channel(channel);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_stop_invalid)
{
    int movie;
    CHECK_TRUE(movie = wrap_movie_open("testdata/movie/test", 1, 0));

    CHECK_TRUE(movie_play(movie));
    for (int frame = 0; frame < MOVIE_FRAMES; frame++) {
        if (frame == 5) {
            movie_stop(0);
            movie_stop(INVALID_MOVIE);
        }
        graphics_clear(0, 0, 0, 0, 1, 0);
        if (!movie_next_frame(movie)) {
            FAIL("movie_next_frame() failed for frame %d", frame);
        }
        if (!draw_from_texture(movie)) {
            FAIL("draw_from_movie() failed for frame %d", frame);
        }
        if (frame < 15 && !check_video_frame(frame, 0, 0)) {
            FAIL("check_video_frame(0) failed for frame %d", frame);
        }
        CHECK_TRUE(check_audio_frame(frame, 1, 1.0f,
                                     num_invalid_audio_samples()));
        graphics_finish_frame();
        graphics_start_frame();
    }
    CHECK_FALSE(movie_next_frame(movie));

    movie_close(movie);
    movie_stop(movie);  // Didn't check this case above.
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_is_playing_invalid)
{
    int movie;
    CHECK_TRUE(movie = wrap_movie_open("testdata/movie/test", 1, 0));

    movie_play(movie);
    ASSERT(movie_is_playing(movie));
    CHECK_FALSE(movie_is_playing(0));
    CHECK_FALSE(movie_is_playing(INVALID_MOVIE));

    movie_close(movie);
    CHECK_FALSE(movie_is_playing(movie));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_texture_invalid)
{
    int movie;
    CHECK_TRUE(movie = wrap_movie_open("testdata/movie/test", 1, 0));

    float dummy = -1;
    CHECK_FALSE(movie_get_texture(movie, NULL, &dummy, &dummy, &dummy));
    CHECK_FALSE(movie_get_texture(movie, &dummy, NULL, &dummy, &dummy));
    CHECK_FALSE(movie_get_texture(movie, &dummy, &dummy, NULL, &dummy));
    CHECK_FALSE(movie_get_texture(movie, &dummy, &dummy, &dummy, NULL));
    CHECK_FALSE(movie_get_texture(0, &dummy, &dummy, &dummy, &dummy));
    CHECK_FALSE(movie_get_texture(INVALID_MOVIE,
                                  &dummy, &dummy, &dummy, &dummy));
    movie_close(movie);
    CHECK_FALSE(movie_get_texture(movie, &dummy, &dummy, &dummy, &dummy));
    CHECK_FLOATEQUAL(dummy, -1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_next_frame_invalid)
{
    int movie;
    CHECK_TRUE(movie = wrap_movie_open("testdata/movie/test", 1, 0));
    movie_close(movie);

    CHECK_FALSE(movie_next_frame(0));
    CHECK_FALSE(movie_next_frame(movie));
    CHECK_FALSE(movie_next_frame(INVALID_MOVIE));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_update)
{
    int movie;
    CHECK_TRUE(movie = wrap_movie_open("testdata/movie/test", 1, 0));

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
        if (!draw_from_texture(movie)) {
            FAIL("draw_from_movie() failed for frame %d", frame);
        }
        if (frame < 15 && !check_video_frame(frame, 0, 0)) {
            FAIL("check_video_frame() failed for frame %d", frame);
        }
        CHECK_TRUE(check_audio_frame(frame, 1, 1.0f,
                                     num_invalid_audio_samples()));
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

TEST(test_update_while_stopped)
{
    int movie;
    CHECK_TRUE(movie = wrap_movie_open("testdata/movie/test", 1, 0));

    sys_test_time_set(0);
    CHECK_TRUE(movie_play(movie));
    double next_frame_time = 0;
    for (int frame = 0; frame < 5;
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
        if (!draw_from_texture(movie)) {
            FAIL("draw_from_movie() failed for frame %d", frame);
        }
        if (frame < 15 && !check_video_frame(frame, 0, 0)) {
            FAIL("check_video_frame() failed for frame %d", frame);
        }
        CHECK_TRUE(check_audio_frame(frame, 1, 1.0f,
                                     num_invalid_audio_samples()));
        graphics_finish_frame();
        graphics_start_frame();
    }

    movie_stop(movie);
    sys_test_time_set(iceilf(next_frame_time * sys_time_unit()));
    CHECK_FALSE(movie_update(movie));
    sound_update();
    int16_t pcm[MOVIE_SAMPLES_PER_FRAME*2];
    sound_mixer_get_pcm(pcm, lenof(pcm)/2);
    for (int i = 0; i < lenof(pcm); i++) {
        CHECK_INTEQUAL(pcm[i], 0);
    }

    movie_play(movie);
    for (int frame = 5; frame < MOVIE_FRAMES;
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
        if (!draw_from_texture(movie)) {
            FAIL("draw_from_movie() failed for frame %d", frame);
        }
        if (frame < 15 && !check_video_frame(frame, 0, 0)) {
            FAIL("check_video_frame() failed for frame %d", frame);
        }
        CHECK_TRUE(check_audio_frame(frame, 1, 1.0f,
                                     num_invalid_audio_samples()));
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

TEST(test_update_same_frame)
{
    int movie;
    CHECK_TRUE(movie = wrap_movie_open("testdata/movie/test", 1, 0));

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
        /* A second call should succeed but should not advance the playback
         * position. */
        if (!movie_update(movie)) {
            FAIL("movie_update() failed for frame %d (second call)", frame);
        }
        if (!draw_from_texture(movie)) {
            FAIL("draw_from_movie() failed for frame %d", frame);
        }
        if (frame < 15 && !check_video_frame(frame, 0, 0)) {
            FAIL("check_video_frame() failed for frame %d", frame);
        }
        CHECK_TRUE(check_audio_frame(frame, 1, 1.0f,
                                     num_invalid_audio_samples()));
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

TEST(test_update_skip_frames)
{
    int movie;
    CHECK_TRUE(movie = wrap_movie_open("testdata/movie/test", 1, 0));

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
        if (frame % 2 == 0) {
            if (!movie_update(movie)) {
                FAIL("movie_update() failed for frame %d", frame);
            }
            if (!draw_from_texture(movie)) {
                FAIL("draw_from_movie() failed for frame %d", frame);
            }
            if (frame < 15 && !check_video_frame(frame, 0, 0)) {
                FAIL("check_video_frame() failed for frame %d", frame);
            }
        }
        CHECK_TRUE(check_audio_frame(frame, 1, 1.0f,
                                     num_invalid_audio_samples()));
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

TEST(test_update_display_framerate_rounding)
{
    if (graphics_frame_period() == 0) {
        SKIP("Display framerate is unknown.");
    }

    int movie;
    CHECK_TRUE(movie = wrap_movie_open("testdata/movie/test", 1, 0));

    sys_test_time_set(0);
    CHECK_TRUE(movie_play(movie));
    double next_frame_time = -0.4f * graphics_frame_period();
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
        if (!draw_from_texture(movie)) {
            FAIL("draw_from_movie() failed for frame %d", frame);
        }
        if (frame < 15 && !check_video_frame(frame, 0, 0)) {
            FAIL("check_video_frame() failed for frame %d", frame);
        }
        CHECK_TRUE(check_audio_frame(frame, 1, 1.0f,
                                     num_invalid_audio_samples()));
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

TEST(test_update_invalid)
{
    int movie;
    CHECK_TRUE(movie = wrap_movie_open("testdata/movie/test", 1, 0));
    movie_close(movie);

    CHECK_FALSE(movie_update(0));
    CHECK_FALSE(movie_update(movie));
    CHECK_FALSE(movie_update(INVALID_MOVIE));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_draw)
{
    int movie;
    CHECK_TRUE(movie = wrap_movie_open("testdata/movie/test", 1, 0));

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
        movie_draw(movie);
        if (frame < 15 && !check_video_frame(frame, 1, 0)) {
            FAIL("check_video_frame() failed for frame %d", frame);
        }
        CHECK_TRUE(check_audio_frame(frame, 1, 1.0f,
                                     num_invalid_audio_samples()));
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

TEST(test_draw_invalid)
{
    int movie;
    CHECK_TRUE(movie = wrap_movie_open("testdata/movie/test", 1, 0));
    movie_close(movie);

    graphics_clear(0, 0, 0, 0, 1, 0);
    /* These calls should all be no-ops. */
    movie_draw(0);
    movie_draw(movie);
    movie_draw(INVALID_MOVIE);
    CHECK_TRUE(check_video_frame(-1, 0, 0));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_smooth_chroma)
{
    if (!can_smooth_chroma()) {
        SKIP("Chroma smoothing not available.");
    }

    int movie;
    CHECK_TRUE(movie = wrap_movie_open("testdata/movie/test", 1, 1));

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
        movie_draw(movie);
        if (frame < 15 && !check_video_frame(frame, 0, 1)) {
            FAIL("check_video_frame() failed for frame %d", frame);
        }
        CHECK_TRUE(check_audio_frame(frame, 1, 1.0f,
                                     num_invalid_audio_samples()));
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

TEST(test_zero_framerate)
{
    if (strcmp(movie_extension(), ".webm") != 0) {
        return 1;
    }

    int movie;
    CHECK_TRUE(movie = wrap_movie_open(
                   "testdata/movie/framerate-0.webm", 0, 0));
    CHECK_DOUBLEEQUAL(movie_framerate(movie), 0);

    CHECK_TRUE(movie_play(movie));
    for (int frame = 0; frame < MOVIE_FRAMES; frame++) {
        graphics_clear(0, 0, 0, 0, 1, 0);
        if (!movie_is_playing(movie)) {
            FAIL("movie_is_playing() was not true for frame %d", frame);
        }
        /* For a framerate-zero movie, movie_update() should advance 1
         * frame per call even without a timestamp change. */
        if (!movie_update(movie)) {
            FAIL("movie_update() failed for frame %d", frame);
        }
        movie_draw(movie);
        if (frame < 15 && !check_video_frame(frame, 1, 0)) {
            FAIL("check_video_frame() failed for frame %d", frame);
        }
        CHECK_TRUE(check_audio_frame(frame, 1, 1.0f,
                                     num_invalid_audio_samples()));
        graphics_finish_frame();
        graphics_start_frame();
    }
    CHECK_TRUE(movie_is_playing(movie));
    CHECK_FALSE(movie_update(movie));
    CHECK_FALSE(movie_is_playing(movie));

    movie_close(movie);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mono_audio)
{
    if (!can_play_mono_movies()) {
        SKIP("Mono audio not supported.");
    }

    int movie;
    CHECK_TRUE(movie = wrap_movie_open("testdata/movie/test-mono", 1, 0));

    CHECK_TRUE(movie_play(movie));
    for (int frame = 0; frame < MOVIE_FRAMES; frame++) {
        graphics_clear(0, 0, 0, 0, 1, 0);
        if (!movie_next_frame(movie)) {
            FAIL("movie_next_frame() failed for frame %d", frame);
        }
        CHECK_TRUE(check_audio_frame(frame, 0, 1.0f,
                                     num_invalid_audio_samples()));
        graphics_finish_frame();
        graphics_start_frame();
    }
    CHECK_FALSE(movie_next_frame(movie));

    movie_close(movie);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_no_audio)
{
    if (!can_play_silent_movies()) {
        SKIP("Silent movies not supported.");
    }

    int movie;
    CHECK_TRUE(movie = wrap_movie_open("testdata/movie/test-nosound", 1, 0));

    CHECK_TRUE(movie_play(movie));
    for (int frame = 0; frame < MOVIE_FRAMES; frame++) {
        graphics_clear(0, 0, 0, 0, 1, 0);
        if (!movie_next_frame(movie)) {
            FAIL("movie_next_frame() failed for frame %d", frame);
        }
        CHECK_TRUE(check_audio_frame(frame, 0, 0.0f,
                                     num_invalid_audio_samples()));
        graphics_finish_frame();
        graphics_start_frame();
    }
    CHECK_FALSE(movie_next_frame(movie));

    movie_close(movie);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_import)
{
    char path[100];
    ASSERT(strformat_check(path, sizeof(path),
                           "testdata/movie/test%s", movie_extension()));

    int64_t offset;
    int size;
    SysFile *fh;
    ASSERT(fh = resource_internal_open_file(path, &offset, &size));

    SysMovieHandle *handle = sys_movie_open(fh, offset, size, 0);
    if (!handle) {
        SKIP("No native movie playback support on this system.");
    }

    int movie;
    CHECK_TRUE(movie = movie_import(handle));
    CHECK_DOUBLEEQUAL(movie_framerate(movie), MOVIE_FRAMERATE);

    CHECK_TRUE(movie_play(movie));
    for (int frame = 0; frame < MOVIE_FRAMES; frame++) {
        graphics_clear(0, 0, 0, 0, 1, 0);
        if (!movie_is_playing(movie)) {
            FAIL("movie_is_playing() was not true for frame %d", frame);
        }
        if (!movie_next_frame(movie)) {
            FAIL("movie_next_frame() failed for frame %d", frame);
        }
        if (!draw_from_texture(movie)) {
            FAIL("draw_from_movie() failed for frame %d", frame);
        }
        if (frame < 15 && !check_video_frame(frame, 1, 0)) {
            FAIL("check_video_frame() failed for frame %d", frame);
        }
        CHECK_TRUE(check_audio_frame(frame, 1, 1.0f,
                                     num_invalid_audio_samples()));
        graphics_finish_frame();
        graphics_start_frame();
    }
    CHECK_TRUE(movie_is_playing(movie));
    /* We reached the end of the movie. */
    CHECK_FALSE(movie_next_frame(movie));
    CHECK_FALSE(movie_is_playing(movie));
    /* Check that things don't break if we try to continue playing anyway. */
    CHECK_FALSE(movie_next_frame(movie));
    CHECK_FALSE(movie_is_playing(movie));

    movie_close(movie);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_import_memory_failure)
{
    char path[100];
    ASSERT(strformat_check(path, sizeof(path),
                           "testdata/movie/test%s", movie_extension()));

    int64_t offset;
    int size;
    SysFile *fh;
    ASSERT(fh = resource_internal_open_file(path, &offset, &size));

    SysMovieHandle *handle = sys_movie_open(fh, offset, size, 0);
    if (!handle) {
        SKIP("No native movie playback support on this system.");
    }

    int movie;
    CHECK_MEMORY_FAILURES(movie = movie_import(handle));
    CHECK_DOUBLEEQUAL(movie_framerate(movie), MOVIE_FRAMERATE);

    CHECK_TRUE(movie_play(movie));
    for (int frame = 0; frame < MOVIE_FRAMES; frame++) {
        graphics_clear(0, 0, 0, 0, 1, 0);
        if (!movie_is_playing(movie)) {
            FAIL("movie_is_playing() was not true for frame %d", frame);
        }
        if (!movie_next_frame(movie)) {
            FAIL("movie_next_frame() failed for frame %d", frame);
        }
        if (!draw_from_texture(movie)) {
            FAIL("draw_from_movie() failed for frame %d", frame);
        }
        if (frame < 15 && !check_video_frame(frame, 1, 0)) {
            FAIL("check_video_frame() failed for frame %d", frame);
        }
        CHECK_TRUE(check_audio_frame(frame, 1, 1.0f,
                                     num_invalid_audio_samples()));
        graphics_finish_frame();
        graphics_start_frame();
    }
    CHECK_TRUE(movie_is_playing(movie));
    /* We reached the end of the movie. */
    CHECK_FALSE(movie_next_frame(movie));
    CHECK_FALSE(movie_is_playing(movie));
    /* Check that things don't break if we try to continue playing anyway. */
    CHECK_FALSE(movie_next_frame(movie));
    CHECK_FALSE(movie_is_playing(movie));

    movie_close(movie);
    return 1;
}

/*************************************************************************/
/*************************************************************************/
