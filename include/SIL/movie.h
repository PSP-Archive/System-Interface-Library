/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/movie.h: Movie playback functionality header.
 */

#ifndef SIL_MOVIE_H
#define SIL_MOVIE_H

EXTERN_C_BEGIN

struct Vector2f;

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

/**
 * movie_open:  Open the given movie file and prepare it for playback.
 *
 * If add_extension is true, a filename extension is appended to the given
 * pathname depending on the system:
 *    - Any system, if SIL_MOVIE_INCLUDE_WEBM is defined: ".webm"
 *    - PSP: ".str"
 *    - All others: ".webm"
 * If add_extension is false, the given pathname is used as is.
 *
 * [Parameters]
 *     path: Pathname (or resource name) of movie file to open.
 *     add_extension: True to add a default file extension; false to use
 *         the pathname as is.
 *     smooth_chroma: For movie formats with subsampled chroma data, true
 *         to linearly interpolate the chroma data when upsampling (slower
 *         but higher quality), false to use point sampling of chroma data
 *         (faster but lower quality).
 * [Return value]
 *     Movie ID (nonzero) on success, zero on error.
 */
extern int movie_open(const char *path, int add_extension, int smooth_chroma);

/**
 * movie_close:  Close the given movie, freeing all associated resources.
 * If the movie is playing, it is automatically stopped.  Does nothing if
 * movie_id == 0.
 *
 * [Parameters]
 *     movie_id: Movie ID.
 */
extern void movie_close(int movie_id);

/**
 * movie_framerate:  Return the frame rate of the given movie.
 *
 * [Parameters]
 *     movie_id: Movie ID.
 * [Return value]
 *     Frame rate, in frames per second, or zero if unknown.
 */
extern double movie_framerate(int movie_id);

/**
 * movie_set_volume:  Set the audio playback volume for the given movie.
 *
 * [Parameters]
 *     movie_id: Movie ID.
 *     volume: Audio playback volume (0...∞, 0 = silent, 1 = as recorded).
 */
extern void movie_set_volume(int movie_id, float volume);

/**
 * movie_play:  Begin or resume playback of the given movie.  After calling
 * this function, the movie audio immediately begins playing from the
 * current playback position, and movie_update() will select frames as
 * appropriate to keep the video in sync with the audio.
 *
 * This function does nothing if the movie is already playing.
 *
 * This function may fail even after successfully opening a movie if, for
 * example, a required system resource is no longer available.
 *
 * Note that some systems are unable to return audio at the very beginning
 * of a movie stream.  SIL handles audio/video synchronization when
 * movie_update() and movie_draw() are used for playback, but you should
 * ensure that at least the first 0.25 seconds of the movie's audio stream
 * is silent in order to avoid audio glitches.
 *
 * [Parameters]
 *     movie_id: Movie ID.
 * [Return value]
 *     True on success, false on error.
 */
extern int movie_play(int movie_id);

/**
 * movie_stop:  Stop playback of the given movie at the current position.
 * Playback can be resumed by calling movie_play().  Does nothing if the
 * movie is not playing.
 *
 * [Parameters]
 *     movie_id: Movie ID.
 */
extern void movie_stop(int movie_id);

/**
 * movie_is_playing:  Return whether the given movie is currently playing.
 * The movie is considered to be playing during the period between a
 * successful call to movie_play() and the earlier of (1) the next call to
 * movie_stop() or (2) the next call to movie_next_frame() or movie_update()
 * which returns false.
 *
 * [Parameters]
 *     movie_id: Movie ID.
 * [Return value]
 *     True if the movie is still playing; false if the movie has stopped.
 */
extern int movie_is_playing(int movie_id);

/**
 * movie_get_texture:  Return the ID of a texture containing the video
 * image.  This texture is updated by calling either movie_next_frame() or
 * movie_update().  The caller must not attempt to modify the texture data.
 *
 * Depending on the system and the movie format, it may not be possible to
 * read the texture data directly (using texture_lock_readonly()).
 *
 * Note that the texture is _not_ initialized by movie_play(); always call
 * movie_update() at least once before using the texture.
 *
 * This function always succeeds for a valid movie ID.
 *
 * [Parameters]
 *     movie_id: Movie ID.
 *     left_ret: Pointer to variable to receive the U coordinate of the
 *         left edge of the image.
 *     right_ret: Pointer to variable to receive the U coordinate of the
 *         right edge of the image.
 *     top_ret: Pointer to variable to receive the V coordinate of the
 *         top edge of the image.
 *     bottom_ret: Pointer to variable to receive the V coordinate of the
 *         bottom edge of the image.
 * [Return value]
 *     Texture ID.
 */
extern int movie_get_texture(
    int movie_id,
    float *left_ret, float *right_ret, float *top_ret, float *bottom_ret);

/**
 * movie_next_frame:  Read the next video frame from the movie and store it
 * in the movie's video texture.  The "next" video frame is the frame
 * following the frame currently rendered to the video texture, or the
 * first frame in the movie if neither this function nor movie_update()
 * have yet been called on the movie.
 *
 * [Parameters]
 *     movie_id: Movie ID.
 * [Return value]
 *     True if the movie is still playing; false if the movie has finished.
 */
extern int movie_next_frame(int movie_id);

/**
 * movie_update:  Update the movie's video texture by advancing to the
 * current frame (see below for how "current" is determined) and rendering
 * that frame to the texture.  If the current frame is already stored in
 * the texture, this function returns without doing anything.
 *
 * For the purposes of this function, the "current" frame is determined
 * by multiplying the number of seconds since movie_play() was called (the
 * "current time") by the frame rate returned by movie_framerate() and
 * truncating the result to an integer.  However, if graphics_frame_period()
 * returns a nonzero value and the current time is less than half that
 * value before the next frame, the frame number is instead rounded up;
 * this reduces the risk of jitter caused by timing fluctuations.
 *
 * If movie_framerate() returns zero, this function behaves like
 * movie_next_frame().
 *
 * [Parameters]
 *     movie_id: Movie ID.
 * [Return value]
 *     True if the movie is still playing; false if the movie has finished.
 */
extern int movie_update(int movie_id);

/**
 * movie_draw:  Draw the currently loaded frame of the given movie to the
 * display.  The caller is responsible for setting appropriate coordinate
 * transformation matrices such that the frame can be rendered with its
 * upper-left corner at (0,0) and its lower-right corner at (width,height).
 * If shader objects are enabled, the caller must install an appropriate
 * shader with the standard vertex attributes bound to vertex shader inputs
 * (however, the vertex color attribute is not used by this function and
 * does not need to be bound).
 *
 * This function may modify all aspects of rendering state except the
 * current framebuffer and coordinate transformation matrices.
 *
 * This function is provided only for convenience; the same result can be
 * obtained by retrieving the movie texture and rendering it on an
 * appropriately-sized quad.
 *
 * [Parameters]
 *     movie_id: Movie ID.
 */
extern void movie_draw(int movie_id);

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_MOVIE_H
