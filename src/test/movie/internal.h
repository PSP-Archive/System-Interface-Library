/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/movie/internal.h: Declarations of helper functions for the
 * movie playback subsystem tests.
 */

#ifndef SIL_SRC_TEST_MOVIE_INTERNAL_H
#define SIL_SRC_TEST_MOVIE_INTERNAL_H

/*************************************************************************/
/*************************************************************************/

/* Video/audio parameters for the test movie. */
#define MOVIE_FRAMES             20
#define MOVIE_FRAMERATE          30
#define MOVIE_SAMPLES_PER_FRAME  (44100 / MOVIE_FRAMERATE)
#define MOVIE_WIDTH              64
#define MOVIE_HEIGHT             32

/*-----------------------------------------------------------------------*/

/**
 * test_movie_core_with_extension:  Run the core movie tests with the given
 * extension as the default movie filename extension.  A display mode must
 * be set before calling this function.
 *
 * Pass NULL to run the tests without overriding the default extension.
 *
 * [Parameters]
 *     extension: Default filename extension to use.
 * [Return value]
 *     True if all tests passed, false if some tests failed.
 */
extern int test_movie_core_with_extension(const char *extension);

/**
 * check_video_frame:  Return whether the display contains the expected
 * image data for the given frame of the test movie on a black background.
 *
 * [Parameters]
 *     frame: Frame index (0-14, or -1 to check for a black frame).
 *     full: True to check the entire display, false to just check the
 *         portion containing the movie.
 *     smooth_chroma: True if linear interpolation was enabled for chroma
 *         upsampling, false if not.
 * [Return value]
 *     True if the check passes, false if it fails.
 */
extern int check_video_frame(int frame, int full, int smooth_chroma);

/**
 * check_audio_frame:  Read one frame's worth of audio from the software
 * mixer and check whether the audio data is reasonably close to the
 * expected waveform.
 *
 * [Parameters]
 *     frame: Video frame number.
 *     stereo: True for stereo input, false for monaural input.
 *     volume: Volume of the input audio.
 *     skip_samples: Number of samples at the beginning of the stream to
 *         ignore (to work around decoder limitations).
 * [Return value]
 *     True if the check passes, false if it fails.
 */
extern int check_audio_frame(int frame, int stereo, float volume,
                             int skip_samples);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_TEST_MOVIE_INTERNAL_H
