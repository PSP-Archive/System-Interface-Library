/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/movie/webm.h: Header for WebM decoding routines.
 */

#ifndef SIL_SRC_MOVIE_WEBM_H
#define SIL_SRC_MOVIE_WEBM_H

struct SysFile;

/*************************************************************************/
/*************************************************************************/

/* Handle data type for software WebM decoding. */
typedef struct WebMDecodeHandle WebMDecodeHandle;

/*-----------------------------------------------------------------------*/

/**
 * movie_webm_open:  Create a new decoding handle for a WebM stream read
 * from the given file.  On success, the decoder takes ownership of the
 * file handle, and the file handle will be closed when the decoder is
 * closed.  On failure, the caller retains ownership of the file handle.
 *
 * If WebM support is not built in (SIL_MOVIE_INCLUDE_WEBM is not defined),
 * this function will always fail.
 *
 * [Parameters]
 *     fh: File handle to read from.
 *     offset: Byte offset of stream data within file.
 *     length: Length of stream data, in bytes.
 *     smooth_chroma: True to linearly interpolate U/V planes when upsampling.
 * [Return value]
 *     New decoder handle, or NULL on error.
 */
extern WebMDecodeHandle *movie_webm_open(struct SysFile *fh, int64_t offset,
                                         int64_t length, int smooth_chroma);

/**
 * movie_webm_close:  Close a WebM stream decoding handle.
 *
 * [Parameters]
 *     handle: Handle to close.
 */
extern void movie_webm_close(WebMDecodeHandle *handle);

/**
 * movie_webm_framerate:  Return the video frame rate of the given stream.
 *
 * [Parameters]
 *     handle: Handle to operate on.
 * [Return value]
 *     Video frame rate, in frames per second, or zero if unknown.
 */
extern double movie_webm_framerate(WebMDecodeHandle *handle);

/**
 * movie_webm_width:  Return the video frame width of the given stream.
 *
 * [Parameters]
 *     handle: Handle to operate on.
 * [Return value]
 *     Video frame width, in pixels.
 */
extern int movie_webm_width(WebMDecodeHandle *handle);

/**
 * movie_webm_height:  Return the video frame height of the given stream.
 *
 * [Parameters]
 *     handle: Handle to operate on.
 * [Return value]
 *     Video frame height, in pixels.
 */
extern int movie_webm_height(WebMDecodeHandle *handle);

/**
 * movie_webm_audio_channels:  Return the number of audio channels in the
 * given stream.
 *
 * [Parameters]
 *     handle: Handle to operate on.
 * [Return value]
 *     Number of audio channels, or zero if the stream has no audio.
 */
extern int movie_webm_audio_channels(WebMDecodeHandle *handle);

/**
 * movie_webm_audio_rate:  Return the audio sampling rate of the given
 * stream.
 *
 * [Parameters]
 *     handle: Handle to operate on.
 * [Return value]
 *     Audio sampling rate, or zero if the stream has no audio.
 */
extern int movie_webm_audio_rate(WebMDecodeHandle *handle);

/**
 * movie_webm_get_video:  Retrieve the next video frame and store it in
 * the given buffer using RGBA pixel format.  The buffer must be large
 * enough to hold an image of movie_webm_width() * movie_webm_height()
 * 32-bit pixels.
 *
 * [Parameters]
 *     handle: Handle to operate on.
 *     buffer: Pixel buffer in which to store the video frame.
 * [Return value]
 *     True if a frame was returned, false if not.
 */
extern int movie_webm_get_video(WebMDecodeHandle *handle, void *buffer);

/**
 * movie_webm_get_audio:  Retrieve the next num_samples audio samples and
 * store them in the given buffer as 16-bit signed integer values.  For
 * multi-channel audio, the channels are interleaved.  The buffer must be
 * large enough to hold num_samples * movie_webm_audio_channels() 16-bit
 * values.
 *
 * [Parameters]
 *     handle: Handle to operate on.
 *     buffer: Buffer in which to store the audio samples.
 *     num_samples: Number of samples to retrieve.
 * [Return value]
 *     Number of samples actually retrieved.  Zero indicates the end of
 *     the audio stream.
 */
extern int movie_webm_get_audio(WebMDecodeHandle *handle, void *buffer,
                                int num_samples);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_MOVIE_WEBM_H
