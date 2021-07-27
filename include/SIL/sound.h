/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/sound.h: Sound-related functionality header.
 */

/*
 * This file declares functionality associated with playback of audio
 * streams.
 *
 * ======== Initializing audio output ========
 *
 * Before attempting to output any audio, the program must open an audio
 * device by calling sound_open_device().  Callers will typically pass the
 * empty string as the device name, though on platforms such as PCs which
 * can have multiple output devices, a user-specified device may be passed
 * instead.  (There is currently no way to enumerate valid device names.)
 *
 * sound_open_device() also takes a requested channel count, which is the
 * maximum number of simultaneous sounds the program expects to play.
 * Setting a large value does not in itself have any significant
 * performance impact, but if too many channels are in active use, SIL may
 * not be able to process audio fast enough for realtime output, resulting
 * in audio "stuttering" as the output device is starved for data.  Using
 * a lower value here can help avoid this problem by letting SIL reject
 * playback requests beyond a certain number of simultaneous sounds.
 *
 * Once the device has been opened, output parameters can be configured
 * using sound_set_interpolate() and sound_set_latency().  The estimated
 * output latency can also be retrieved with sound_get_latency(), such as
 * for displaying in a configuration UI.
 *
 * It is not currently possible to close the output device or change to a
 * different output device after calling sound_open_device().
 *
 * As the program runs, it should call sound_update() periodically to
 * update SIL's image of the current playback state.  This call can be
 * made as part of the program's overall update loop, for example.
 *
 * ======== Managing audio data ========
 *
 * The base type for audio data is Sound.  Sound instances can be created
 * with sound_create() or sound_create_stream(), though more commonly
 * they will be created via a resource manager (see resource_load_sound()
 * and resource_open_sound() in resource.h).
 *
 * Typically, audio data will be loaded or streamed from an audio file in
 * a standard format.  SIL supports RIFF WAVE (linear PCM) and Ogg Vorbis
 * files on all platforms, as well as some platform-specific types (for
 * example, MP3 is supported on the PSP).
 *
 * In addition to the audio data itself, Sound instances encapsulate loop
 * points, used when a sound is played with looping enabled.  If loop
 * endpoints have been set with the sound_set_loop() function, then when
 * looping is enabled for a sound, SIL will loop only the specified
 * portion of the audio data rather than the entire stream.  The sound
 * creation functions will also read loop information from file formats
 * which support it (see the sound_set_loop() documentation for details),
 * allowing loop points to be encoded directly into the audio file rather
 * than having to specify them manually in the program code.
 *
 * ======== Playing audio data ========
 *
 * Once a Sound instance has been created, it can be played by passing it
 * to sound_play().  The volume, pan, playback rate, and loop enable
 * settings can be changed while the sound is playing by passing the
 * channel returned by sound_play() to the appropriate function.
 *
 * To temporarily stop playback of a sound, call sound_pause().  A
 * subsequent call to sound_resume() on the same channel will resume
 * playback where it stopped.
 *
 * To terminate playback of a sound, call sound_cut() or sound_fade().
 * When the sound terminates, the channel and associated playback resources
 * will be freed.  If using sound_fade(), sound_is_playing() will indicate
 * whether the fade is still in progress.
 *
 * The current playback position in the audio stream (notionally, the
 * "playback head") can be retrieved with sound_playback_pos().  This can
 * be used to synchronize events with specific points in the audio stream,
 * or to display the current playback time in a sound player, for example.
 *
 * By default, sound_play() and related functions will allocate an unused
 * channel for the sound to be played, and release that channel when the
 * sound ends or is stopped.  It is also possible to reserve channels with
 * sound_reserve_channel(), removing them from the pool used for dynamic
 * allocation and guaranteeing that the reserved channel will always be
 * available for playback.  This can be used to ensure that high-priority
 * sounds, such as background music or voices, can always be played back
 * regardless of how many other sounds are active.  When a reserved channel
 * is no longer needed, call sound_free_channel() to return it to the
 * dynamic allocation pool.
 *
 * In addition to manipulating individual channels, all channels can be
 * paused or resumed at once by calling sound_pause_all() or
 * sound_resume_all().  These functions can be used, for example, to stop
 * and restart audio around a suspend event or when (on PCs) input focus
 * is lost.
 *
 * ======== Applying filters ========
 *
 * Audio data can be passed through a filter before being sent to the
 * output mixer, which is useful for applying effects such as echo or
 * flanging.  Filters are applied per channel, so different effects can
 * be applied to different filters.
 *
 * SIL currently includes one builtin audio filter which provides a
 * flanging effect, enabled by calling sound_set_flange().  SIL also
 * provides the sound_set_filter() function for applying custom filter
 * functions to channels, though the interface is not currently public; see
 * the file src/sound/filter.h in the SIL source code for details.
 *
 * It is not currently possible to apply multiple filters to a single
 * channel, though a custom filter could take another filter as a parameter
 * and call that filter manually to create a simple filter chain.  It is
 * also not currently possible to apply a single filter to the final mixed
 * data.
 *
 * ======== Custom audio decoders and generators ========
 *
 * The sound subsystem implements audio stream decoding through a decoder
 * interface, and it is possible to write custom decoders to handle special
 * data formats.  "Decode" here is used in a very broad sense; the
 * interface does not require that a decoder process input data in any
 * particular fashion, and it is entirely conceivable to, for example,
 * create a "decoder" which plays MOD-format music modules, effectively
 * turning the "decode" function into a "generate" function.  Indeed,
 * several of SIL's built-in tests use such generators to feed data into
 * the sound subsystem for testing.
 *
 * The sound_decode_open_custom() function creates a decoder instance for
 * a particular decoder type and input data.  The handle returned from
 * that function can then be passed to sound_play_decoder(), which is the
 * equivalent of sound_play() for decoder instances.  sound_play_decoder()
 * takes ownership of the decoder instance, so the caller does not need to
 * worry about freeing it when playback completes.  It is also possible to
 * destroy a decoder instance without playing it, by calling
 * sound_decode_close().
 *
 * See <SIL/sound/decode.h> for details of the decoder interface.
 *
 * ======== Handling headphone disconnect events ========
 *
 * On certain platforms (currently Android and iOS), the system will notify
 * programs if the user has headphones connected to the device and the
 * connection is lost, for example because the user unplugged the
 * headphones.  By calling sound_enable_headphone_disconnect_check(), SIL
 * will respond to these events by immediately muting audio output, so that
 * audio which was previously directed to the headphones is not played out
 * loud through the device's speakers.
 *
 * To detect whether such an event has occurred, the program should call
 * sound_check_headphone_disconnect() periodically.  This function will
 * return true after an event has been received; the program can then call
 * sound_acknowledge_headphone_disconnect() to re-enable audio output,
 * typically after waiting for user confirmation.
 *
 * Note that the headphone disconnect check is disabled by default, so that
 * programs which are not prepared to handle these events are not silenced
 * when such an event occurs.
 *
 * ======== Sound and multithreading ========
 *
 * Most sound-related functions are thread-safe.  Individual channels are
 * not locked against changes by multiple threads, so it is not safe to
 * call sound_stop() for a channel from one thread while calling other
 * channel-related functions on the same channel from another thread.
 * (Note that this includes the implicit sound_stop() performed by
 * sound_update() when a non-looped channel finishes playing or when
 * sound_play() or sound_play_decoder() is called on a statically
 * allocated channel which is currently playing a sound.)  It _is_ safe
 * to make simultaneous calls to channel-related functions other than
 * sound_stop(), though no guarantees are made about the order in which
 * the operations will take effect.
 *
 * If using a custom decoder, note that sound processing is performed on a
 * separate thread from the main program, so the decoder should be careful
 * not to call thread-unsafe functions without synchronizing with the main
 * thread.
 */

#ifndef SIL_SOUND_H
#define SIL_SOUND_H

EXTERN_C_BEGIN

struct SoundDecodeHandle;
struct SoundFilterHandle;
struct SysFile;

/*************************************************************************/
/*************************************************************************/

/*----------------------------- Data types ------------------------------*/

/**
 * Sound:  Data type representing an audio sample.  Instances of this type
 * are created with sound_create() or sound_create_stream(), played with
 * sound_play(), and destroyed with sound_destroy().
 */
typedef struct Sound Sound;

/**
 * SoundFormat:  Identifiers for particular types of audio data.  (The
 * values are taken from the 16-bit identifiers in general use to identify
 * audio types, e.g. in the "fmt" header of a RIFF WAVE file.)
 */
enum SoundFormat {
    /* Value used to request autodetection of the audio data format.
     * This is guaranteed to be zero in all future versions of the library. */
    SOUND_FORMAT_AUTODETECT = 0,
    /* 16-bit little-endian linear PCM data in a RIFF WAVE container. */
    SOUND_FORMAT_WAV = 0x0001,
    /* Classic MPEG audio (layer I, II, or III; version 1, 2, or 2.5).
     * Note that this format is not supported on all platforms. */
    SOUND_FORMAT_MP3 = 0x0055,
    /* Vorbis audio in an Ogg container. */
    SOUND_FORMAT_OGG = 0x674F,
};
typedef enum SoundFormat SoundFormat;

/*------------------------ Global sound control -------------------------*/

/**
 * sound_open_device:  Open the given audio device for playback, allocating
 * the given number of playback channels.  The format of the device name is
 * system-dependent, except that an empty string or NULL always means "the
 * default device".  On systems which do not support multiple audio output
 * devices, the empty string is the only valid device name.
 *
 * This function may only be called once over the lifetime of the program.
 * Attempting to call any sound functions other than the Sound instance
 * management functions (sound_create(), etc.) without first successfully
 * calling this function will have no effect.
 *
 * [Parameters]
 *     device_name: System-dependent device name for audio output.
 *     num_channels: Number of playback channels to use.
 * [Return value]
 *     True on success, false on error.
 */
extern int sound_open_device(const char *device_name, int num_channels);

/**
 * sound_set_interpolate:  Set whether to interpolate between samples when
 * resampling sounds for playback.  Interpolation improves sound quality
 * but requires more CPU time for playback, potentially reducing overall
 * performance.  The default is to enable interpolation.
 *
 * This setting takes effect for all subsequent calls to sound_play(), but
 * does not affect any sounds already being played back.
 *
 * This setting has no effect on sounds which are not resampled (i.e.,
 * which have the same native sampling rate as the system's audio output
 * interface).
 *
 * [Parameters]
 *     enable: True to enable interpolation, false to disable.
 */
extern void sound_set_interpolate(int enable);

/**
 * sound_get_latency:  Return the estimated output latency for audio.
 * This is the approximate amount of time that will elapse between a call
 * to sound_play() to start playing a sound and the time when that sound
 * starts being emitted from the physical output device (such as speakers).
 *
 * [Return value]
 *     Estimated audio output latency, in seconds.
 */
extern float sound_get_latency(void);

/**
 * sound_set_latency:  Request a specific audio output latency.  The system
 * will attempt to reconfigure audio output to give a latency as close as
 * possible to the requested value, and the value actually chosen will be
 * returned.  The return value is the same value that would be returned by
 * a subsequent call to sound_get_latency().
 *
 * This function may block for a short time, and it may cause a short
 * interruption in audio output.
 *
 * [Parameters]
 *     latency: Requested audio output latency, in seconds.
 * [Return value]
 *     New estimated audio output latency, in seconds.
 */
extern float sound_set_latency(float latency);

/**
 * sound_check_format:  Return whether the given audio data format is
 * supported.
 *
 * [Parameters]
 *     format: Audio data format (SOUND_FORMAT_*).
 * [Return value]
 *     True if the format is supported, false if not.
 */
extern int sound_check_format(enum SoundFormat format);

/**
 * sound_set_global_volume:  Set the global volume scale factor.  The
 * default is 1.0.  Lowering this value may help avoid clipping when
 * multiple sounds are played at the same time.
 *
 * This one goes up to 15, because sometimes 11 just isn't enough.
 *
 * [Parameters]
 *     volume: Global volume scale factor (0-15, 1 = no change to output
 *         samples).
 */
extern void sound_set_global_volume(float volume);

/**
 * sound_update:  Periodic update routine.  This should be called once per
 * frame or other convenient period.
 */
extern void sound_update(void);

/**
 * sound_pause_all:  Suspend playback on all channels.
 */
extern void sound_pause_all(void);

/**
 * sound_resume_all:  Resume playback on all channels.  Channels which
 * have been independently paused with sound_pause() will remain paused.
 */
extern void sound_resume_all(void);

/**
 * sound_enable_headphone_disconnect_check:  Enable logic for detecting
 * headphone disconnect events on platforms which support such events.
 * After calling this function, if the system reports that a headphone or
 * similarly private device has been disconnected, SIL will automatically
 * mute all sound output (independently of other volume settings) so that
 * it is not sent to speakers or a similarly "noisy" device, and report the
 * disconnect event via sound_check_headphone_disconnect().  The caller is
 * responsible for calling sound_acknowledge_headphone_disconnect() to
 * re-enable sound output after such an event.
 *
 * On platforms which do not support headphone disconnect detection, this
 * functions do nothing.
 */
extern void sound_enable_headphone_disconnect_check(void);

/**
 * sound_check_headphone_disconnect:  Return true if the audio output route
 * has changed from headphones (or some other private device) to speakers
 * (or some other "noisy" route).  On systems that cannot detect when
 * headphones have been connected or disconnected, this function always
 * returns false.
 *
 * The flag returned by this function is sticky until reset by
 * sound_acknowledge_headphone_disconnect().
 *
 * If headphone disconnect detection has not been enabled with
 * sound_enable_headphone_disconnect_check() or the platform does not
 * support headphone disconnect detection, this function always returns
 * false.
 *
 * [Return value]
 *     True if a headphone disconnect has been detected, false if not.
 */
extern int sound_check_headphone_disconnect(void);

/**
 * sound_acknowledge_headphone_disconnect:  Reset the headphone-disconnect
 * flag returned by sound_check_headphone_disconnect(), and re-enable sound
 * output if it was muted due to a headphone disconnect event.  If no
 * headphone disconnect has been detected, this function does nothing.
 */
extern void sound_acknowledge_headphone_disconnect(void);

/*---------------------- Sound instance management ----------------------*/

/**
 * sound_create:  Create a new Sound instance from a memory buffer.  If
 * reuse != 0, then the memory buffer (which must have been allocated using
 * mem_alloc()) will be reused if possible and freed if not.  Otherwise,
 * an internal copy of the data buffer will be created for the new instance.
 *
 * [Parameters]
 *     data: Audio data buffer.
 *     datalen: Length of audio data, in bytes.
 *     format: Audio data format (SOUND_FORMAT_*).
 *     reuse: True to reuse the audio data buffer, false to allocate new
 *         memory for the data.
 * [Return value]
 *     Newly created Sound instance, or NULL on error.
 */
extern Sound *sound_create(void *data, int datalen, SoundFormat format,
                           int reuse
#ifdef DEBUG
                           , const char *file, int line
#endif
);

/**
 * sound_create_stream:  Create a new Sound instance which will stream
 * audio data from a file.  The file handle will be closed when the Sound
 * instance is destroyed.
 *
 * If the function fails, the file handle is left untouched.
 *
 * [Parameters]
 *     fh: File handle for streaming.
 *     dataofs: Offset of audio data within file, in bytes.
 *     datalen: Length of audio data, in bytes.
 *     format: Audio data format (SOUND_FORMAT_*).
 * [Return value]
 *     Newly created Sound instance, or NULL on error.
 */
extern Sound *sound_create_stream(struct SysFile *fh, int64_t dataofs,
                                  int datalen, SoundFormat format
#ifdef DEBUG
                                  , const char *file, int line
#endif
);

/**
 * sound_is_stereo:  Return whether the given Sound instance contains
 * stereo audio data.
 *
 * [Parameters]
 *     sound: Sound instance.
 * [Return value]
 *     True if the audio data is stereo, false if the audio data is monaural.
 */
extern int sound_is_stereo(Sound *sound);

/**
 * sound_native_freq:  Return the native playback frequency of the given
 * Sound instance.
 *
 * [Parameters]
 *     sound: Sound instance.
 * [Return value]
 *     Native playback frequency (Hz).
 */
extern int sound_native_freq(Sound *sound);

/**
 * sound_set_loop:  Set the loop start point and length of the given Sound
 * instance.
 *
 * The default loop start and end points (used if this function is not
 * called for a particular Sound instance) are determined by the decoder:
 *
 * - For RIFF WAVE files, if a "smpl" chunk is present, the start and end
 *   points of the first loop entry (offsets 44 and 48 in the chunk data)
 *   are taken as the loop points.  Note that other data in the chunk,
 *   such as loop type and play count, is ignored.
 *
 * - For Ogg Vorbis files, if LOOPSTART and LOOPLENGTH fields are present
 *   in the file comments, they are taken as the loop points.
 *
 * If the decoder does not set explicit loop points, the entire stream is
 * used for looping.
 *
 * [Parameters]
 *     sound: Sound instance.
 *     start: Start position of loop, in samples.
 *     len: Length of loop, in samples (0 means "all remaining samples").
 */
extern void sound_set_loop(Sound *sound, int start, int len);

/**
 * sound_destroy:  Destroy a Sound instance.  If the instance is not
 * currently being played on any channels, it is destroyed immediately.
 * Otherwise, the instance is marked for deletion, but its associated
 * resources will not actually be freed until the sound finishes playing.
 *
 * [Parameters]
 *     sound: Sound instance to destroy.
 */
extern void sound_destroy(Sound *sound
#ifdef DEBUG
                          , const char *file, int line
#endif
);

/*------------------------ Sound channel control ------------------------*/

/**
 * sound_reserve_channel:  Reserve a sound channel for use by the caller.
 * The returned channel number will never be dynamically allocated by
 * sound_play(), so the caller will always be able to play sounds on the
 * channel and can avoid interfering with other sounds.
 *
 * [Return value]
 *     Reserved channel number, or zero on error.
 */
extern int sound_reserve_channel(void);

/**
 * sound_free_channel:  Free a channel previously reserved with
 * sound_reserve_channel().  If a sound is currently playing on the
 * channel, it will continue playing normally, and the channel will become
 * available for dynamic allocation when the sound finishes or is stopped.
 *
 * This function does nothing if channel == 0.
 *
 * [Parameters]
 *     channel: Channel number to free.
 */
extern void sound_free_channel(int channel);

/**
 * sound_play:  Play audio data.
 *
 * If a nonzero channel number is passed in the channel parameter, the
 * sound will be played on that channel; if a sound is already playing on
 * that channel, it will be stopped as though sound_cut() had been called.
 * The channel number must have been previously returned by
 * sound_reserve_channel().
 *
 * If true is passed for the loop parameter, the sound will automatically
 * loop back to the loop start point when the playback position reaches the
 * loop end point.  See sound_set_loop() for details on how the loop start
 * and end points are determined.
 *
 * [Parameters]
 *     sound: Sound instance to play.
 *     channel: Channel to use for playback, or 0 to allocate one dynamically.
 *     volume: Playback volume (0...∞, 0 = silent, 1 = as recorded).
 *     pan: Pan position (-1 = left, 0 = center, +1 = right).
 *     loop: True to loop, false to play once and stop.
 * [Return value]
 *     Channel number for playback (nonzero) on success, zero on error.
 */
extern int sound_play(Sound *sound, int channel, float volume, float pan,
                      int loop
#ifdef DEBUG
                      , const char *file, int line
#endif
);

/**
 * sound_play_decoder:  Play audio data using a custom audio decoding handle
 * (see sound_decode_open_custom() in sound/decode.h).
 *
 * On success, the sound core takes ownership of the decoding handle and
 * closes it when playback terminates.  On failure, the caller retains
 * ownership of the decoding handle.
 *
 * [Parameters]
 *     decoder: Audio decoding handle.
 *     channel: Channel to use for playback, or 0 to allocate one dynamically.
 *     volume: Playback volume (0...∞, 0 = silent, 1 = as recorded).
 *     pan: Pan position (-1 = left, 0 = center, +1 = right).
 * [Return value]
 *     Channel number for playback (nonzero) on success, zero on error.
 */
extern int sound_play_decoder(struct SoundDecodeHandle *decoder, int channel,
                              float volume, float pan
#ifdef DEBUG
                              , const char *file, int line
#endif
);

/**
 * sound_pause:  Suspend playback on the given channel.  Does nothing if
 * the channel is already paused via this function.
 *
 * The suspend state controlled by this function and sound_resume() is
 * tracked independently from the global suspend state toggled by
 * sound_pause_all() and sound_resume_all().  If this function is called
 * while playback is globally paused, the channel remains paused but will
 * no longer resume when sound_resume_all() is called.  Similarly, if
 * sound_resume() is called for a paused channel while playback is
 * globally paused, the channel will not resume playback until
 * sound_resume_all() is called.
 *
 * [Parameters]
 *     channel: Sound channel.
 */
extern void sound_pause(int channel);

/**
 * sound_resume:  Resume playback on the given channel.  Does nothing if
 * the channel was not paused with sound_pause().
 *
 * See the documentation of sound_pause() for an explanation of how this
 * function interacts with sound_pause_all() and sound_resume_all().
 *
 * [Parameters]
 *     channel: Sound channel.
 */
extern void sound_resume(int channel);

/**
 * sound_cut:  Immediately stop playback on the given channel.  Playback
 * cannot be resumed after this call.  Does nothing if the channel is not
 * active.
 *
 * [Parameters]
 *     channel: Sound channel.
 */
extern void sound_cut(int channel
#ifdef DEBUG
                      , const char *file, int line
#endif
);

/**
 * sound_fade:  Fade the given channel out to silence, terminating playback
 * when the fade completes.  Playback cannot be resumed after the fade
 * completes.  (It is possible to abort the fade using sound_adjust_volume()
 * before the fade completes, but this is not recommended as it can result
 * in unstable behavior.  In such cases, use sound_adjust_volume() to fade
 * to a volume of zero, and later fade in or cut the channel as needed.)
 *
 * Does nothing if the channel is not active.
 *
 * Note that sound_fade(channel,0) is equivalent to sound_cut(channel).
 *
 * [Parameters]
 *     channel: Sound channel.
 *     time: Fade time, in seconds.
 */
extern void sound_fade(int channel, float time
#ifdef DEBUG
                       , const char *file, int line
#endif
);

/**
 * sound_adjust_volume:  Adjust the given channel's volume, optionally
 * fading over a period of time.  If the volume goes to zero, the channel
 * becomes silent but continues processing input data as usual.  Does
 * nothing if the channel is not active.
 *
 * [Parameters]
 *     channel: Sound channel.
 *     new_volume: New playback volume (0...∞, 0 = silent, 1 = as recorded).
 *     time: Fade time, in seconds, or zero to change the volume immediately.
 */
extern void sound_adjust_volume(int channel, float new_volume, float time);

/**
 * sound_set_pan:  Set the given channel's pan position.  Does nothing if
 * the channel is not active.
 *
 * [Parameters]
 *     channel: Sound channel.
 *     new_pan: New pan position (-1 = left, 0 = center, +1 = right).
 */
extern void sound_set_pan(int channel, float new_pan);

/**
 * sound_set_playback_rate:  Set the playback rate of the sound on the
 * given channel.  A value greater than 1 causes the sound to be played
 * back faster and at a higher pitch than usual; a value less than one
 * results in slower and lower-pitched playback.  Zero can be used to
 * pause decoding; this has a similar effect to sound_pause(), but the
 * current sample value is maintained, resulting in a DC bias in the
 * mixed output.  Negative rates are not allowed.
 *
 * [Parameters]
 *     channel: Sound channel.
 *     new_rate: New playback rate (1 = as recorded).
 */
extern void sound_set_playback_rate(int channel, float new_rate);

/**
 * sound_set_flange:  Enable or disable flanging on the given channel.  The
 * period and depth parameters are ignored if enable is false.  This
 * replaces any existing filter on the channel.
 *
 * [Parameters]
 *     channel: Sound channel.
 *     enable: True to enable flanging, false to disable.
 *     period: Flange period (delay cycle period, in seconds).
 *     depth: Flange depth (maximum playback offset, in seconds).
 */
extern void sound_set_flange(int channel, int enable, float period, float depth);

/**
 * sound_set_filter:  Enable or disable filtering with an arbitrary filter
 * on the given channel.  This replaces any existing filter on the channel.
 *
 * The passed-in filter is taken over by the sound core and will be freed
 * automatically when no longer needed.
 *
 * [Parameters]
 *     channel: Sound channel.
 *     filter: Filter to apply, or NULL to remove any existing filter.
 */
extern void sound_set_filter(int channel, struct SoundFilterHandle *filter);

/**
 * sound_enable_loop:  Set whether to loop the sound on the given channel.
 * Attempting to enable looping when the playback position is already past
 * the loop endpoint will have no effect.
 *
 * Note that for sounds in Ogg Vorbis format, enabling looping after the
 * sound has started playing may result in a short dropout the first time
 * the sound loops due to one-time seek overhead.  (This does not occur if
 * looping is enabled when the sound is started, since in that case the
 * decoder performs a dummy seek before starting to avoid overhead during
 * playback.)
 *
 * [Parameters]
 *     channel: Sound channel.
 *     loop: True to enable looping, false to disable looping.
 */
extern void sound_enable_loop(int channel, int loop);

/**
 * sound_is_playing:  Return whether a sound is currently playing on the
 * given channel.  Paused sounds are treated as "currently playing" for the
 * purposes of this function.
 *
 * [Parameters]
 *     channel: Sound channel.
 * [Return value]
 *     True if a sound is currently playing on the given channel, false if not.
 */
extern int sound_is_playing(int channel);

/**
 * sound_playback_pos:  Return the current playback position on the given
 * channel.  For looped sounds, the returned value is the position within
 * the audio stream (taking loops into account) rather than the length of
 * time the channel has been playing.  Inactive channels are treated as
 * having a playback position of zero.
 *
 * [Parameters]
 *     channel: Sound channel.
 * [Return value]
 *     Current playback position, in seconds.
 */
extern float sound_playback_pos(int channel);

/*------------------------- Debugging wrappers --------------------------*/

/*
 * When debugging is enabled, we wrap allocating calls with these macros
 * which pass down the source file and line at which the call was made
 * (as for resource control functions in resource.h).
 */

#ifdef DEBUG
# define sound_create(data,datalen,format,reuse) \
    sound_create((data), (datalen), (format), (reuse), __FILE__, __LINE__)
# define sound_create_stream(fh,dataofs,datalen,format) \
    sound_create_stream((fh), (dataofs), (datalen), (format), \
                        __FILE__, __LINE__)
# define sound_destroy(sound) \
    sound_destroy((sound), __FILE__, __LINE__)
# define sound_play(sound,channel,volume,pan,loop) \
    sound_play((sound), (channel), (volume), (pan), (loop), __FILE__, __LINE__)
# define sound_play_decoder(decoder,channel,volume,pan) \
    sound_play_decoder((decoder), (channel), (volume), (pan), \
                       __FILE__, __LINE__)
# define sound_cut(channel) \
    sound_cut((channel), __FILE__, __LINE__)
# define sound_fade(channel,time) \
    sound_fade((channel), (time), __FILE__, __LINE__)
#endif

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_SOUND_H
