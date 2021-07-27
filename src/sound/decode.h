/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sound/decode.h: Internal header for audio decoding handlers.
 */

#ifndef SIL_SRC_SOUND_DECODE_H
#define SIL_SRC_SOUND_DECODE_H

#include "SIL/sound/decode.h"

struct SysFile;

/*************************************************************************/
/************************** Internal constants ***************************/
/*************************************************************************/

/**
 * READ_BUFFER_SIZE:  Size, in bytes, of the buffer used for reading from
 * files.
 */
#define READ_BUFFER_SIZE  16384

/**
 * RESAMPLE_BUFLEN:  Size, in samples, of the buffer used for resampling.
 */
#define RESAMPLE_BUFLEN  1024

/*************************************************************************/
/****************************** Data types *******************************/
/*************************************************************************/

/* Internal data structure, declared but not defined in the public header. */

struct SoundDecodeInternal {
    /* Parameters to the open_*() function. */
    int64_t dataofs;            // Offset of audio data in data source (bytes).
    int datalen;                // Length of audio data (bytes).
    uint8_t loop;               // True if looping is enabled.

    /* Data source types and associated pointers.  The data pointer for
     * CUSTOM-type decoders is exported in the SoundDecodeHandle structure. */
    enum {SOUND_DECODE_BUFFER, SOUND_DECODE_FILE, SOUND_DECODE_CUSTOM}
        data_type;
    const uint8_t *data;        // Memory buffer for type BUFFER.
    struct SysFile *fh;         // File handle for type FILE.

    /* Read buffer and related data for reading from a file. */
    uint8_t *read_buffer;       // Read buffer.
    int64_t read_buffer_pos;    // File offset of the first byte in read_buffer
                                //    (bytes).
    int read_buffer_len;        // Length of valid data in the buffer (bytes).
    int read_async_req;         // Asynchronous read request ID.
    int read_async_ofs;         // Offset into the buffer at which the current
                                //    async read is taking place (bytes).

    /* Number of samples retrieved by the caller (for get_position()).  When
     * resampling is in use, this is based on the native sampling rate. */
    int samples_gotten;

    /* Data for resampling. */
    int decode_freq;            // Decoding rate (Hz).
    int output_freq;            // Output sampling rate (Hz).
    uint8_t need_resample;      // True if resampling is required.
    uint8_t resample_active;    // True if resampling is active.
    uint8_t do_interpolate;     // True to enable inter-sample interpolation.
    uint8_t resample_eof;       // True if the end of the stream was reached.
    int resample_len;           // Number of samples in the reampling buffer.
    int resample_pos;           // Current resampling position (samples).
    int pos_frac;               // Fractional part of reasmple_pos
                                //    (0...output_freq-1).
    int resample_loopofs;       // Backward loop offset to apply on reaching
                                //    the end of the resample buffer.
    int16_t *resample_buf;      // Temporary PCM buffer for resampling
                                //    (allocated only if necessary).
    int16_t last_l, last_r;     // Previous left/right input sample (for
                                //    interpolation).
};

/*************************************************************************/
/************************* SIL-internal routines *************************/
/*************************************************************************/

/*----------------- Audio format decoder configuration ------------------*/

/**
 * sound_decode_set_handler:  Set the decoder to be used with the given
 * audio data format.
 *
 * [Parameters]
 *     format: Audio data format constant (SOUND_FORMAT_*).
 *     open_func: Pointer to the open() function for the decoder to use,
 *         or NULL for the default handler.
 */
extern void sound_decode_set_handler(SoundFormat format,
                                     SoundDecodeOpenFunc *open_func);

/**
 * sound_decode_has_handler:  Return whether a decoder has been registered
 * for the given audio data format.
 *
 * [Parameters]
 *     format: Audio data format constant (SOUND_FORMAT_*).
 * [Return value]
 *     True if a decoder has been registered, false if not.
 */
extern int sound_decode_has_handler(SoundFormat format);

/*---- Decoder instance creation functions for memory and file data -----*/

/**
 * sound_decode_open:  Start decoding audio data stored in a memory buffer.
 * The memory buffer remains owned by the caller, and must remain valid as
 * long as the decoder instance exists.
 *
 * [Parameters]
 *     format: Audio data format constant (SOUND_FORMAT_*).
 *     data: Audio data buffer.
 *     datalen: Audio data length, in bytes.
 *     loop: True to enable looping, false to disable looping.  (Looping
 *         can also be toggled on or off after the decoder is created.)
 *     interpolate: True to enable interpolation of resampled sounds.
 *         Has no effect if resampling is not required.
 * [Return value]
 *     Decoder instance handle, or NULL on error.
 */
extern SoundDecodeHandle *sound_decode_open(
    SoundFormat format, const void *data, int datalen, int loop,
    int interpolate);

/**
 * sound_decode_open_from_file:  Start decoding audio data stored in a file.
 * The file handle remains owned by the caller; the handle is duplicated
 * for use by the decoder, and will not be touched by further decode
 * operations (thus the caller is free to close the file handle as soon as
 * this function returns).
 *
 * [Parameters]
 *     format: Audio data format constant (SOUND_FORMAT_*).
 *     fh: Audio data file handle.
 *     dataofs: Offset of audio data within the file, in bytes.
 *     datalen: Audio data length, in bytes.
 *     loop: True to enable looping, false to disable looping.  (Looping
 *         can also be toggled on or off after the decoder is created.)
 *     interpolate: True to enable interpolation of resampled sounds.
 *         Has no effect if resampling is not required.
 * [Return value]
 *     Decoder instance handle, or NULL on error.
 */
extern SoundDecodeHandle *sound_decode_open_from_file(
    SoundFormat format, struct SysFile *fh, int64_t dataofs, int datalen,
    int loop, int interpolate);

/*---------------------- Other interface functions ----------------------*/

/* Note: the "this" parameter is implied (but not documented) for all
 * functions below. */

/**
 * sound_decode_is_stereo:  Return whether the decoded audio stream is
 * stereo or monaural.
 *
 * [Return value]
 *     True if the decoded audio stream is stereo, false if monaural.
 */
extern int sound_decode_is_stereo(SoundDecodeHandle *this);

/**
 * sound_decode_native_freq:  Return the native sampling rate of the
 * decoded audio stream.
 *
 * [Return value]
 *     Native sampling rate, in Hz.
 */
extern int sound_decode_native_freq(SoundDecodeHandle *this);

/**
 * sound_decode_set_decode_freq:  Set the frequency to use as the
 * sampling rate for decoded audio data.  Setting this lower than the
 * stream's native sampling rate will cause the stream to be played back
 * slower and at a lower pitch; setting it higher will cause the stream to
 * be played back faster and at a higher pitch.
 *
 * The frequency can be set to zero, which causes the decoder to hold the
 * current sample until the frequency is changed again.  Negative
 * frequencies are not allowed.
 *
 * By default, audio data is decoded at the stream's native sampling rate.
 *
 * [Parameters]
 *     freq: Decoding rate, in Hz.
 */
extern void sound_decode_set_decode_freq(SoundDecodeHandle *this, int freq);

/**
 * sound_decode_set_output_freq:  Set the sampling rate for audio data
 * returned by sound_decode_get_pcm().  If not equal to the decoding rate,
 * the input data will be resampled to the requested rate.
 *
 * By default, audio data is output at the native sampling rate (even if
 * the decoding rate is subsequently changed by calling
 * sound_decode_set_decode_freq()).
 *
 * [Parameters]
 *     freq: Output sampling rate, in Hz.
 */
extern void sound_decode_set_output_freq(SoundDecodeHandle *this, int freq);

/**
 * sound_decode_set_loop_points:  Set the loop start point and length for
 * the decoder.
 *
 * [Parameters]
 *     start: Loop start point, in samples.
 *     length: Loop length, in samples, or 0 to use the end of the stream
 *         as the end of the loop.
 */
extern void sound_decode_set_loop_points(SoundDecodeHandle *this,
                                         int start, int length);

/**
 * sound_decode_enable_loop:  Enable or disable looping for the decoder.
 * Attempting to enable looping when the decode position is already past
 * the loop endpoint will have no effect.
 *
 * This function does nothing if the decoder was created with a loop length
 * of zero.
 *
 * [Parameters]
 *     loop: True to enable looping, false to disable looping.
 */
extern void sound_decode_enable_loop(SoundDecodeHandle *this, int loop);

/**
 * sound_decode_get_pcm:  Return PCM audio data from the current decode
 * position in the audio stream, and advance the decode position
 * accordingly.  If the end of the stream is reached before the buffer is
 * full, the remaining space in the buffer will be filled with zero-value
 * samples.
 *
 * [Parameters]
 *     pcm_buffer: Buffer into which to store PCM (signed 16-bit) data.
 *     pcm_len: Number of samples to retrieve.
 * [Return value]
 *     True on success, false if no data could be retrieved (end of stream
 *     already reached or unrecoverable decoding error).
 */
extern int sound_decode_get_pcm(SoundDecodeHandle *this, int16_t *pcm_buffer,
                                int pcm_len);

/**
 * sound_decode_get_position:  Return the time offset from the beginning
 * of the audio stream of the decode position (i.e., the position of the
 * next sample that would be returned by sound_decode_get_pcm()).
 *
 * The returned value is not affected by any changes to the decoding
 * frequency.  For example, setting a decoding frequency of half the
 * native sampling rate causes the value returned by this function to
 * increase by only 0.5 seconds for every 1 second of audio data generated
 * by the decoder.
 *
 * [Return value]
 *     Decode position, in seconds.
 */
extern float sound_decode_get_position(SoundDecodeHandle *this);

/*************************************************************************/
/***************** Utility functions for decoder modules *****************/
/*************************************************************************/

/**
 * decode_get_data:  Retrieve data from a memory or file data source.
 * Only up to READ_BUFFER_SIZE (defined in decode.c) bytes of data can be
 * read in a single call.
 *
 * This function may not be used (and will always return zero) for custom
 * decoders.
 *
 * [Parameters]
 *     pos: Offset into the audio data at which to read, in bytes.
 *     len: Number of bytes to read.
 *     ptr_ret: Buffer into which to read data.
 * [Return value]
 *     Number of bytes read.
 */
extern int decode_get_data(SoundDecodeHandle *this, int pos, int len,
                           const uint8_t **ptr_ret);

/*************************************************************************/
/**************** Standard decoder module open() methods *****************/
/*************************************************************************/

/* RIFF WAVE-format PCM audio decoder (more like an "extractor"). */
extern int decode_wav_open(SoundDecodeHandle *this);

/* Ogg Vorbis decoder. */
#ifdef SIL_SOUND_INCLUDE_OGG
extern int decode_ogg_open(SoundDecodeHandle *this);
#endif

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SOUND_DECODE_H
