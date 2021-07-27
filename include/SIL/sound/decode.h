/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/sound/decode.h: Header for audio decoding handlers.
 */

/*
 * This file defines the interface through which custom audio decoders
 * provide audio data to the sound core.  A decoder handle can be created
 * by calling sound_decode_open_custom(), passing it the open() method of
 * the custom decoder; the handle can then be used with sound_play_decoder()
 * to play audio data output by the decoder.
 *
 * Note that the term "decoder" here is used in a general sense to mean
 * anything that outputs raw audio data, including both decoders in the
 * usual sense of the word and data generators (such as tone generators)
 * which create output without processing any input.
 */

#ifndef SIL_SOUND_DECODE_H
#define SIL_SOUND_DECODE_H

EXTERN_C_BEGIN
#ifdef __cplusplus
# define this this_  // Avoid errors when included from C++ source.
# define private private_
#endif

/*************************************************************************/
/****************************** Data types *******************************/
/*************************************************************************/

/**
 * SoundDecodePrivate:  Structure type provided for use by individual
 * decoder modules.  By defining a structure of this type, modules can use
 * the handle->private field to store per-instance data.  The sound core
 * treats this field as opaque.
 */
typedef struct SoundDecodePrivate SoundDecodePrivate;

/**
 * SoundDecodeInternal:  Structure type used to encapsulate data internal
 * to the sound core.
 */
typedef struct SoundDecodeInternal SoundDecodeInternal;

/**
 * SoundDecodeHandle:  Object type used for decoding a bitstream into
 * PCM audio data.  Use sound_decode_open_custom() to create a decoder
 * instance with a user-specified initialization function; use
 * sound_decode_close() to destroy an existing decoder instance.
 */
typedef struct SoundDecodeHandle SoundDecodeHandle;
struct SoundDecodeHandle {
    /**
     * get_pcm:  Retrieve signed 16-bit PCM audio samples from the
     * audio stream.
     *
     * [Parameters]
     *     pcm_buffer: Buffer into which to store PCM (signed 16-bit) data.
     *     pcm_len: Number of samples to retrieve.
     *     loop_offset_ret: Pointer to variable to receive the number of
     *         samples skipped backward due to looping (used in reporting
     *         playback position).
     * [Return value]
     *     Number of samples stored in pcm_buffer.
     */
    int (*get_pcm)(SoundDecodeHandle *this, int16_t *pcm_buffer,
                   int pcm_len, int *loop_offset_ret);

    /**
     * close:  Terminate decoding and clean up any resources allocated by
     * the decoder's open() method.
     */
    void (*close)(SoundDecodeHandle *this);

    /* Audio parameters. */
    int stereo;       // True if stereo, false if monaural.
    int native_freq;  // PCM sampling rate, in samples per second.
    int bitrate;      // Nominal data rate, in bits per second.
    int loop_start;   // Start of loop, in samples.
    int loop_length;  // Length of loop, or 0 to mean "loop to end of file".

    /* Data pointer passed to the sound_decode_open_custom() function.
     * Set by the sound core before calling the decoder's open() method. */
    void *custom_data;

    /* Private data for the decoder implementation, ignored by the sound
     * core. */
    SoundDecodePrivate *private;

    /* Data used internally by the sound core. */
    SoundDecodeInternal *internal;
};

/**
 * SoundDecodeOpenFunc:  Audio decoder open() method type, used with
 * sound_decode_set_handler() and sound_decode_open_custom().  This
 * function must set the method pointers as well as the audio parameter
 * fields (stereo, native_freq, bitrate, loop_start, loop_length) in the
 * instance handle.
 *
 * [Parameters]
 *     this: Decoder instance handle.
 * [Return value]
 *     True on success, false on error.
 */
typedef int SoundDecodeOpenFunc(SoundDecodeHandle *this);

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

/**
 * sound_decode_open_custom:  Start decoding an audio stream using a custom
 * decoder module.  (The decoder implementation need not be a "decoder" per
 * se; it may be anything which returns audio data, such as a waveform
 * generator.)
 *
 * [Parameters]
 *     open_func: Pointer to the open() method for the decoder.
 *     data: Arbitrary data pointer (stored in the handle's custom_data field).
 *     interpolate: True to enable interpolation of resampled sounds.  Has
 *         no effect if resampling is not required.
 * [Return value]
 *     Decoder instance handle, or NULL on error.
 */
extern SoundDecodeHandle *sound_decode_open_custom(
    SoundDecodeOpenFunc *open_func, void *data, int interpolate);

/**
 * sound_decode_close:  Terminate decoding and destroy the decoder instance.
 *
 * [Parameters]
 *     this: Decoder handle.
 */
extern void sound_decode_close(SoundDecodeHandle *this);

/*************************************************************************/
/*************************************************************************/

#ifdef __cplusplus
# undef this
# undef private
#endif
EXTERN_C_END

#endif  // SIL_SRC_SOUND_DECODE_H
