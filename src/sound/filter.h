/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sound/filter.h: Internal header for audio filters.
 */

#ifndef SIL_SRC_SOUND_FILTER_H
#define SIL_SRC_SOUND_FILTER_H

/*************************************************************************/
/****************************** Data types *******************************/
/*************************************************************************/

/**
 * SoundFilterPrivate:  Structure type provided for use by individual
 * filter modules.  By defining a structure of this type, modules can use
 * the handle->private field to store per-instance data.  This declaration
 * also serves to hide the contents of the structure from callers.
 */
typedef struct SoundFilterPrivate SoundFilterPrivate;

/**
 * SoundFilterHandle:  Object type used for performing a filtering
 * operation on a PCM audio stream.  Use sound_filter_open_*() to create
 * an instance of a particular filter; use sound_filter_close() to destroy
 * an existing instance.
 */
typedef struct SoundFilterHandle SoundFilterHandle;
struct SoundFilterHandle {
    /* Method implementations (see sound_filter_*() for documentation). */
    int (*filter)(SoundFilterHandle *this, int16_t *pcm_buffer,
                  uint32_t pcm_len);
    void (*close)(SoundFilterHandle *this);

    /* Audio parameters. */
    int stereo;     // True if stereo, false if monaural.
    uint32_t freq;  // PCM sampling rate.

    /* Private data for the filter implementation. */
    SoundFilterPrivate *private;
};

/*************************************************************************/
/************************* Interface declaration *************************/
/*************************************************************************/

/*--------------- Creation functions for each filter type ---------------*/

/**
 * sound_filter_open_flange:  Create a new flange filter instance.  The
 * filter output at a given time t (in seconds) is the mean of the input
 * samples at times t and t - depth*((1-cos(2πt/period))/2.
 *
 * For the filter to work properly, the input values must satisfy the
 * following inequalities:
 *    - roundf(freq * period) < 4294967296.0f
 *    - roundf(freq * depth)  < 65536.0f
 *
 * [Parameters]
 *     stereo: True if the input is a stereo stream, false if monaural.
 *     freq: PCM data sampling rate, in Hz.
 *     period: Flange period (cosine wave period), in seconds.
 *     depth: Flange depth (maximum time offset), in seconds.
 * [Return value]
 *     New filter instance handle, or NULL on error.
 */
extern SoundFilterHandle *sound_filter_open_flange(
    int stereo, uint32_t freq, float period, float depth);

/*---------------------- Other interface functions ----------------------*/

/* Note: the "this" parameter is implied (but not documented) for all
 * functions below. */

/**
 * sound_filter_filter:  Filter the audio data in pcm_buffer, overwriting
 * the buffer with the filtered data.
 *
 * [Parameters]
 *     pcm_buffer: Buffer containing PCM audio data (16-bit signed) to filter.
 *     pcm_len: Number of samples in pcm_buffer.
 * [Return value]
 *     True on success, false on error.
 */
extern int sound_filter_filter(SoundFilterHandle *this, int16_t *pcm_buffer,
                               uint32_t pcm_len);

/**
 * sound_filter_close:  Destroy the given filter instance.
 */
extern void sound_filter_close(SoundFilterHandle *this);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SOUND_FILTER_H
