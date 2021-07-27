/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sound/mixer.h: Header for software mixer implementation.
 */

#ifndef SIL_SRC_SOUND_MIXER_H
#define SIL_SRC_SOUND_MIXER_H

/*
 * This file, along with the associated implementation in mixer.c, defines
 * a simple software mixer supporting an arbitrary number of playback
 * channels, each of which can play 1- or 2-channel audio streams (surround
 * is not currently supported).
 *
 * Most functions in this file are only for use by the sound core.
 * However, system-specific audio code should call sound_mixer_get_pcm()
 * from its audio output callback or equivalent function to retrieve PCM
 * audio data for output.  This function may be called from any thread.
 */

/*************************************************************************/
/*************************************************************************/

/**
 * SoundDecodeCallback:  Function type for the audio data decoding callback
 * function passed to sys_sound_setdata().  Decodes the next "pcm_len"
 * samples of audio data to S16LE PCM and stores them in "pcm_buffer".
 * If at least one sample but less than "pcm_len" samples are available,
 * the remaining portion of the buffer is cleared to zero.
 *
 * The output buffer must have enough room for "pcm_len" stereo samples,
 * i.e. 4*pcm_len bytes.
 *
 * Note that this function may be called in a different thread than that
 * which originally called sys_sound_setdata().
 *
 * [Parameters]
 *     handle: Audio data handle (opaque pointer).
 *     pcm_buffer: Output buffer for PCM samples.
 *     pcm_len: Number of samples to retrieve.
 * [Return value]
 *     True on success, false on end of stream or error.
 */
typedef int (*SoundDecodeCallback)(void *handle, void *pcm_buffer, int pcm_len);

/*---------------------- General control functions ----------------------*/

/**
 * sound_mixer_init:  Initialize the mixer, allocating the specified number
 * of channels for playback.  Channels are indexed starting from 1 (not 0!),
 * like the high-level sound routines.
 *
 * This function may only be called if the mixer has never been initialized
 * or after a call to sound_mixer_cleanup().
 *
 * [Parameters]
 *     num_channels: Number of playback channels to allocate.
 *     mix_rate: Sampling rate to use for mixing.
 * [Return value]
 *     True on success, false on failure.
 */
extern int sound_mixer_init(int num_channels, int mix_rate);

/**
 * sound_mixer_set_base_volume:  Set the base volume factor applied to
 * all channels.  The volume must be between 0 and 15 inclusive.  The
 * default is 0.5, which reduces the volume of all samples by half (to
 * avoid clipping when multiple samples are played simultaneously).
 *
 * [Parameters]
 *     volume: Base volume (1.0 = samples played at channel volume 1.0 are
 *         output unchanged).
 */
extern void sound_mixer_set_base_volume(float volume);

/**
 * sound_mixer_get_pcm:  Read PCM data from the mixer.
 *
 * [Parameters]
 *     buffer: Buffer into which to store mixed audio data (interleaved
 *         stereo 16-bit signed integer PCM).
 *     samples: Number of samples to read.
 */
extern void sound_mixer_get_pcm(void *buffer, int samples);

/**
 * sound_mixer_cleanup:  Shut down the mixer and release all associated
 * resources.
 */
extern void sound_mixer_cleanup(void);

/*---------------- Sound playback/manipulation functions ----------------*/

/**
 * sound_mixer_setdata:  Register an audio data stream on a mixer channel.
 * This function fails if a stream has already been registered on the
 * channel; to reuse a channel, first call sound_mixer_reset() to clear the
 * existing stream.
 *
 * If the channel number is valid, decode_func is not NULL, and no stream
 * is registered on the channel, this function always succeeds.
 *
 * The decode callback function must return data with the same sampling
 * rate as the mixing rate passed to sound_mixer_init().
 *
 * [Parameters]
 *     channel: Channel number for playback.
 *     decode_func: Decode callback function for audio data.
 *     handle: Audio data handle passed to decode function.
 *     is_stereo: True if the audio data is stereo, false if monaural.
 * [Return value]
 *     True on success, false on error.
 */
extern int sound_mixer_setdata(int channel, SoundDecodeCallback decode_func,
                               void *handle, int is_stereo);

/**
 * sound_mixer_setvol:  Set the playback volume on the given channel.  Any
 * fade effect on the channel is cancelled by this call.
 *
 * This function may be called whether data has been registered on the
 * channel or not.
 *
 * [Parameters]
 *     channel: Channel number.
 *     volume: Playback volume (0...∞, 0 = silent, 1 = as recorded).
 */
extern void sound_mixer_setvol(int channel, float volume);

/**
 * sound_mixer_setpan:  Set the pan position for the given channel.  For
 * stereo samples, the channel opposite the pan direction (e.g., the right
 * channel when panning left) is shifted in the direction of the pan, and
 * the overall volume is scaled down by a factor of 1+|pan|.
 *
 * This function may be called whether data has been registered on the
 * channel or not.
 *
 * [Parameters]
 *     channel: Channel number.
 *     pan: Pan position (-1 = left, 0 = center, +1 = right).
 */
extern void sound_mixer_setpan(int channel, float pan);

/**
 * sound_mixer_setfade:  Begin a volume fade on the given channel.
 *
 * [Parameters]
 *     channel: Channel number.
 *     target: Target volume (0...∞, 0 = silent, 1 = as recorded).
 *     length: Fade length, in seconds, or zero to cancel any current fade.
 *     cut: True to terminate playback when the volume reaches zero; false
 *         to continue playback regardless of volume.
 */
extern void sound_mixer_setfade(int channel, float target, float length, int cut);

/**
 * sound_mixer_start:  Begin playing the given channel's audio data.  If
 * playback was previously stopped with sound_mixer_stop(), playback resumes
 * from the point at which it was stopped.  This function does nothing if
 * no audio data has been registered on the channel.
 *
 * [Parameters]
 *     channel: Channel number.
 */
extern void sound_mixer_start(int channel);

/**
 * sound_mixer_stop:  Stop playback on the given channel.  This function
 * does nothing if the channel is not currently playing.
 *
 * [Parameters]
 *     channel: Channel number.
 */
extern void sound_mixer_stop(int channel);

/**
 * sound_mixer_reset:  Stop playback on the given channel and clear any
 * registered audio data, waiting until playback has actually stopped
 * before returning.
 *
 * [Parameters]
 *     channel: Channel number.
 */
extern void sound_mixer_reset(int channel);

/**
 * sound_mixer_status:  Return whether the given channel is currently playing.
 *
 * [Parameters]
 *     channel: Channel number.
 * [Return value]
 *     True if the channel is playing, false if not.
 */
extern int sound_mixer_status(int channel);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SOUND_MIXER_H
