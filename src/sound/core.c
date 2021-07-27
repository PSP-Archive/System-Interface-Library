/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sound/core.c: Core sound playback routines.
 */

#include "src/base.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/mutex.h"
#include "src/sound.h"
#include "src/sound/decode.h"
#include "src/sound/filter.h"
#include "src/sound/mixer.h"
#include "src/sysdep.h"
#include "src/thread.h"

/* Disable the memory debugging macros from the bottom of sound.h.  Also
 * define convenience macros for declaring and passing debug parameters, to
 * avoid unneeded messiness in the actual code. */
#ifdef DEBUG
# undef sound_create
# undef sound_create_stream
# undef sound_destroy
# undef sound_play
# undef sound_play_decoder
# undef sound_cut
# undef sound_fade
# define __DEBUG_PARAMS      , const char *file, int line
# define __DEBUG_ARGS        , file, line
# define __LOCAL_DEBUG_ARGS  , __FILE__, __LINE__
#else
# define __DEBUG_PARAMS      /*nothing*/
# define __DEBUG_ARGS        /*nothing*/
# define __LOCAL_DEBUG_ARGS  /*nothing*/
#endif

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Sound structure definition. */

struct Sound {
    /* Usage counter, to prevent sound_destroy() from freeing in-use sounds. */
    uint16_t usage_counter;

    /* Flag indicating whether this object should be freed when the last
     * channel it's used by is stopped. */
    uint8_t free_on_stop;

    /* Flag indicating whether the data source is a memory buffer or a
     * file handle. */
    uint8_t is_file;

    /* Sound data format. */
    SoundFormat format;

    /* Data source, either a buffer pointer or a file handle depending on
     * the value of "is_file". */
    union {
        void *data;
        SysFile *fh;
    };

    /* Data offset (only used for file sources) and length. */
    int64_t dataofs;
    int datalen;

    /* Loop start and length set with sound_set_loop(), or -1 if no
     * explicit loop has been defined. */
    int loopstart;
    int looplen;

    /* Audio parameters. */
    uint8_t have_audio_params;  // Flag: have we looked these up yet?
    uint8_t stereo;             // Stereo (true) or monaural (false)?
    int native_freq;            // Native playback frequency (Hz).
};

/*-----------------------------------------------------------------------*/

/* Flag: Have we been initialized? */
static uint8_t initted;

/* Flag: Has an audio output device been opened? */
static uint8_t device_opened;

/* Flag: Enable interpolation for resampled sounds? */
static uint8_t enable_interpolation;

/* Flag: Enable headphone disconnect handling? */
static uint8_t enable_headphone_disconnect;

/* Flag: Is playback globally paused (via sound_pause_all())? */
static uint8_t global_pause;

/* Total number of sound channels available for playback. */
static int num_channels;

/* Playback sampling rate. */
static int playback_rate;

/*----------------------------------*/

/* Data for each channel.  We number channels starting with 1, so entry 0
 * in this array is unused, but the waste of a mere 16-24 bytes is a small
 * price to pay for improved code clarity. */

typedef struct Channel Channel;
struct Channel {
    /* Channel ID for use with sys_sound_*() functions. */
    int id;

    /* Is the channel reserved (cannot be dynamically allocated)? */
    uint8_t reserved;

    /* Is the channel currently in use (playing something)? */
    uint8_t in_use;

    /* Is the channel currently paused (whether due to a global pause or
     * a channel-specific pause)? */
    uint8_t paused;

    /* Has this specific channel been paused with sound_pause()? */
    uint8_t channel_pause;

    /* Sound object in use by this channel. */
    Sound *sound;

    /* Decoder instance for this channel. */
    SoundDecodeHandle *decoder;

    /* Filter instance for this channel, or NULL if no filter is active. */
    SoundFilterHandle *filter;

    /* Locking flags used for changing the active filter.  These flags and
     * the associated code implement Peterson's mutual exclusion algorithm
     * to protect updates to the "filter" field, required to ensure that
     * the main thread does not free a filter being used by a separate
     * decoding thread.  We implement locking ourselves rather than using
     * system synchronization primitives because the system primitives on
     * certain platforms (yes, Apple, I'm looking at you) are too slow to
     * be usable at the potentially high frequency required here, and in
     * normal use there will almost never be any lock contention. */
    uint8_t filter_lock_main, filter_lock_decode;
    uint8_t filter_lock_turn_is_main;
};

static Channel *channels;

/* Mutex for allocating channels for playback. */
static int allocate_channel_mutex;

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * sound_decode_callback:  Callback function for providing decoded PCM data
 * to the system sound interface.  Decodes the next "pcm_len" samples of
 * audio data to S16LE PCM and stores them in "pcm_buffer".  If at least
 * one sample but less than "pcm_len" samples are available, the remaining
 * portion of the buffer is cleared to zero.
 *
 * The output buffer is guaranteed to have enough room for "pcm_len" stereo
 * samples, i.e. 4*pcm_len bytes.
 *
 * [Parameters]
 *     handle: Audio data handle (opaque pointer).
 *     pcm_buffer: Output buffer for PCM samples.
 *     pcm_len: Number of samples to retrieve.
 * [Return value]
 *     True on success, false on end of stream or error.
 */
static int sound_decode_callback(void *handle, void *pcm_buffer, int pcm_len);

/**
 * autodetect_format:  Attempt to determine the format of the given audio data.
 *
 * [Parameters]
 *     data: Buffer containing the audio data (or an initial portion thereof).
 *     datalen: Length of data, in bytes.
 * [Return value]
 *     Audio data format (SOUND_FORMAT_*, nonzero), or zero if unknown.
 */
static SoundFormat autodetect_format(const uint8_t *data, int datalen);

/**
 * find_audio_params:  Look up the audio parameters for a Sound object by
 * analyzing the audio data.  Does nothing if the parameters have already
 * been successfully looked up.
 *
 * [Parameters]
 *     sound: Sound object.
 * [Return value]
 *     True if parameters were successfully looked up (or already known),
 *     false on error.
 */
static int find_audio_params(Sound *sound);

/**
 * open_decoder:  Open a decoder for the given Sound object.
 *
 * [Parameters]
 *     sound: Sound object for which to open a decoder.
 *     loop: True to loop, false to play once and stop.
 * [Return value]
 *     Decoder handle, or NULL on error.
 */
static SoundDecodeHandle *open_decoder(Sound *sound, int loop);

/**
 * allocate_channel:  Allocate an unused channel, with thread safety.
 *
 * [Parameters]
 *     reserve: True to mark the new channel as reserved, false to mark it
 *         as in-use (dynamically allocated for playback).
 * [Return value]
 *     Allocated channel, or 0 on failure.
 */
static int allocate_channel(int reserve);

/**
 * lock_filter, unlock_filter:  Lock or unlock the given channel for filter
 * operations.
 *
 * A memory barrier is executed before returning from each of these functions.
 *
 * [Parameters]
 *     channel: Pointer to channel to lock or unlock.
 *     from_main: 1 if called from the main interface functions; 0 if called
 *         from the decode callback.
 */
static void lock_filter(Channel *channel, const int from_main);
static void unlock_filter(Channel *channel, const int from_main);

/**
 * reset_channel:  Reset the given channel and free associated resources.
 *
 * [Parameters]
 *     channel: Channel to reset.
 */
static void reset_channel(int channel __DEBUG_PARAMS);

/**
 * free_sound:  Free resources associated with a Sound object.  The object
 * is assumed to not be in use by any channels.
 *
 * [Parameters]
 *     sound: Sound object to free.
 */
static void free_sound(Sound *sound __DEBUG_PARAMS);

/*************************************************************************/
/******************** Interface: Global sound control ********************/
/*************************************************************************/

void sound_init(void)
{
    if (initted) {
        return;
    }

    enable_interpolation = 1;
    enable_headphone_disconnect = 0;

    /* sys_sound_init() and other possibly-failing calls are deferred to
     * sound_open_device(). */

    initted = 1;
}

/*-----------------------------------------------------------------------*/

int sound_open_device(const char *device_name, int num_channels_)
{
    if (device_opened) {
        return 1;
    }

    if (UNLIKELY(num_channels_ <= 0)) {
        DLOG("Invalid parameters: %p[%s] %d", device_name,
             device_name ? device_name : "", num_channels_);
        goto error_return;
    }
    if (!device_name) {
        device_name = "";
    }

    /* Set up local data structures. */
    num_channels = num_channels_;
    channels =
        mem_alloc(sizeof(*channels) * (num_channels+1), 0, MEM_ALLOC_CLEAR);
    if (UNLIKELY(!channels)) {
        DLOG("No memory for channel array (%d channels)", num_channels);
        goto error_return;
    }
    for (int i = 1; i <= num_channels; i++) {
        channels[i].id = i;
    }
    allocate_channel_mutex = mutex_create(MUTEX_SIMPLE, MUTEX_UNLOCKED);
    if (UNLIKELY(!allocate_channel_mutex)) {
        goto error_free_channels;
    }

    /* Open the requested output device. */
    if (UNLIKELY(!sys_sound_init(device_name))) {
        goto error_destroy_allocate_channel_mutex;
    }
    const int sys_playback_rate = sys_sound_playback_rate();
    if (UNLIKELY(sys_playback_rate <= 0)) {
        DLOG("sys_sound_playback_rate() returned invalid value %d",
             sys_playback_rate);
        goto error_close_device;
    }
    playback_rate = sys_playback_rate;

    /* Set up the software mixer. */
    if (!sound_mixer_init(num_channels, playback_rate)) {
        DLOG("Mixer initialization failed");
        goto error_close_device;
    }

    sys_sound_enable_headphone_disconnect_check(enable_headphone_disconnect);

    enable_interpolation = 1;
    device_opened = 1;
    return 1;

  error_close_device:
    sys_sound_cleanup();
  error_destroy_allocate_channel_mutex:
    mutex_destroy(allocate_channel_mutex);
  error_free_channels:
    mem_free(channels);
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

void sound_set_interpolate(int enable)
{
    if (!device_opened) {
        return;
    }

    enable_interpolation = (enable != 0);
}

/*-----------------------------------------------------------------------*/

extern float sound_get_latency(void)
{
    if (!device_opened) {
        return 0;
    }

    return sys_sound_set_latency(0);
}

/*-----------------------------------------------------------------------*/

float sound_set_latency(float latency)
{
    if (!device_opened) {
        return 0;
    }

    if (UNLIKELY(latency <= 0)) {
        DLOG("Invalid parameters: %g", latency);
        return sound_get_latency();
    }
    return sys_sound_set_latency(latency);
}

/*-----------------------------------------------------------------------*/

int sound_check_format(enum SoundFormat format)
{
    if (!device_opened) {
        return 0;
    }

    return sound_decode_has_handler(format);
}

/*-----------------------------------------------------------------------*/

void sound_set_global_volume(float volume)
{
    if (!device_opened) {
        return;
    }

    if (UNLIKELY(!(volume >= 0 && volume <= 15))) {
        DLOG("Invalid volume: %g", volume);
        return;
    }
    sound_mixer_set_base_volume(volume);
}

/*-----------------------------------------------------------------------*/

void sound_update(void)
{
    if (!device_opened) {
        return;
    }

    for (int channel = 1; channel <= num_channels; channel++) {
        if (channels[channel].in_use
         && !channels[channel].paused
         && !sound_mixer_status(channels[channel].id)) {
            reset_channel(channel __LOCAL_DEBUG_ARGS);
        }
    }
}

/*-----------------------------------------------------------------------*/

void sound_pause_all(void)
{
    if (!device_opened) {
        return;
    }

    for (int channel = 1; channel <= num_channels; channel++) {
        if (channels[channel].in_use) {
            channels[channel].paused = 1;
            sound_mixer_stop(channels[channel].id);
        }
    }
    global_pause = 1;
}

/*-----------------------------------------------------------------------*/

void sound_resume_all(void)
{
    if (!device_opened) {
        return;
    }

    global_pause = 0;
    for (int channel = 1; channel <= num_channels; channel++) {
        if (channels[channel].in_use
         && channels[channel].paused
         && !channels[channel].channel_pause) {
            sound_mixer_start(channels[channel].id);
            channels[channel].paused = 0;
        }
    }
}

/*-----------------------------------------------------------------------*/

void sound_enable_headphone_disconnect_check(void)
{
    if (!device_opened) {
        return;
    }

    enable_headphone_disconnect = 1;
    sys_sound_enable_headphone_disconnect_check(1);
}

/*-----------------------------------------------------------------------*/

int sound_check_headphone_disconnect(void)
{
    if (!device_opened) {
        return 0;
    }

    return sys_sound_check_headphone_disconnect();
}

/*-----------------------------------------------------------------------*/

void sound_acknowledge_headphone_disconnect(void)
{
    if (!device_opened) {
        return;
    }

    sys_sound_acknowledge_headphone_disconnect();
}

/*-----------------------------------------------------------------------*/

void sound_cleanup(void)
{
    if (!initted) {
        return;
    }

    if (device_opened) {
        for (int channel = 1; channel <= num_channels; channel++) {
            if (channels[channel].in_use) {
                reset_channel(channel __LOCAL_DEBUG_ARGS);
            }
        }
        sys_sound_cleanup();
        sound_mixer_cleanup();
        mutex_destroy(allocate_channel_mutex);
        allocate_channel_mutex = 0;
        mem_free(channels);
        channels = NULL;
        num_channels = 0;
        device_opened = 0;
    }

    initted = 0;
}

/*************************************************************************/
/****************** Interface: Sound object management *******************/
/*************************************************************************/

Sound *sound_create(void *data, int datalen, SoundFormat format, int reuse
                    __DEBUG_PARAMS)
{
    if (UNLIKELY(data == NULL)
     || UNLIKELY(datalen == 0)) {
        DLOG("Invalid parameters: %p %d 0x%X", data, datalen, format);
        goto error_return;
    }

    if (format == SOUND_FORMAT_AUTODETECT) {
        if (!(format = autodetect_format(data, datalen))) {
            goto error_return;
        }
    }

    Sound *sound = debug_mem_alloc(sizeof(*sound), 0, 0,
                                   file, line, MEM_INFO_SOUND);
    if (!sound) {
        DLOG("Out of memory for sound object");
        goto error_return;
    }

    if (reuse) {
        sound->data = data;
        mem_debug_set_info(data, MEM_INFO_SOUND);
    } else {
        sound->data = debug_mem_alloc(datalen, 0, 0,
                                      file, line, MEM_INFO_SOUND);
        if (!sound->data) {
            DLOG("Out of memory for sound data (%u bytes)", datalen);
            debug_mem_free(sound, file, line);
            goto error_return;
        }
        memcpy(sound->data, data, datalen);
    }

    sound->usage_counter     = 0;
    sound->free_on_stop      = 0;
    sound->is_file           = 0;
    sound->format            = format;
    sound->datalen           = datalen;
    sound->loopstart         = -1;
    sound->looplen           = -1;
    sound->have_audio_params = 0;
    return sound;

  error_return:
    if (reuse) {
        debug_mem_free(data, file, line);
    }
    return NULL;
}

/*-----------------------------------------------------------------------*/

Sound *sound_create_stream(SysFile *fh, int64_t dataofs, int datalen,
                           SoundFormat format __DEBUG_PARAMS)
{
    if (UNLIKELY(fh == NULL)
     || UNLIKELY(datalen == 0)) {
        DLOG("Invalid parameters: %p %lld %d 0x%X",
             fh, (long long)dataofs, datalen, format);
        return 0;
    }

    if (format == SOUND_FORMAT_AUTODETECT) {
        uint8_t readbuf[2048];
        sys_file_seek(fh, dataofs, FILE_SEEK_SET);
        int toread = ubound(datalen, (int)sizeof(readbuf));
        int nread = sys_file_read(fh, readbuf, toread);
        if (nread < 0) {
            DLOG("sys_file_read(%p, %d) failed: %s", fh, toread,
                 sys_last_errstr());
            return NULL;
        } else if (nread != toread) {
            DLOG("sys_file_read(%p, %d) failed: Premature EOF", fh, toread);
            return NULL;
        }
        if (!(format = autodetect_format(readbuf, nread))) {
            return NULL;
        }
    }

    Sound *sound = debug_mem_alloc(sizeof(*sound), 0, 0,
                                   file, line, MEM_INFO_SOUND);
    if (!sound) {
        DLOG("Out of memory for sound object");
        return NULL;
    }

    sound->usage_counter     = 0;
    sound->free_on_stop      = 0;
    sound->is_file           = 1;
    sound->format            = format;
    sound->fh                = fh;
    sound->dataofs           = dataofs;
    sound->datalen           = datalen;
    sound->loopstart         = -1;
    sound->looplen           = -1;
    sound->have_audio_params = 0;
    return sound;
}

/*-----------------------------------------------------------------------*/

int sound_is_stereo(Sound *sound)
{
    if (UNLIKELY(!sound)) {
        DLOG("sound == NULL");
        return 0;
    }

    if (!find_audio_params(sound)) {
        return 0;
    }
    return sound->stereo;
}

/*-----------------------------------------------------------------------*/

int sound_native_freq(Sound *sound)
{
    if (UNLIKELY(!sound)) {
        DLOG("sound == NULL");
        return 0;
    }

    if (!find_audio_params(sound)) {
        return 0;
    }
    return sound->native_freq;
}

/*-----------------------------------------------------------------------*/

void sound_set_loop(Sound *sound, int start, int len)
{
    if (UNLIKELY(!sound)
     || UNLIKELY(start < 0)
     || UNLIKELY(len < 0)) {
        DLOG("Invalid parameters: %p %d %d", sound, start, len);
        return;
    }

    sound->loopstart = start;
    sound->looplen   = len;
}

/*-----------------------------------------------------------------------*/

void sound_destroy(Sound *sound __DEBUG_PARAMS)
{
    if (sound) {
        if (sound->usage_counter > 0) {
            sound->free_on_stop = 1;
        } else {
            free_sound(sound __DEBUG_ARGS);
        }
    }
}

/*************************************************************************/
/******************* Interface: Sound channel control ********************/
/*************************************************************************/

int sound_reserve_channel(void)
{
    if (!device_opened) {
        return 0;
    }

    const int channel = allocate_channel(1);
    if (!channel) {
        DLOG("No channels available for reservation");
    }
    return channel;
}

/*-----------------------------------------------------------------------*/

void sound_free_channel(int channel)
{
    if (!device_opened) {
        return;
    }
    if (UNLIKELY(channel < 0 || channel > num_channels)) {
        DLOG("Invalid parameter: %d", channel);
        return;
    }

    if (channel) {
        if (UNLIKELY(!channels[channel].reserved)) {
            DLOG("Channel %d was not reserved", channel);
            return;
        }
        channels[channel].reserved = 0;
    }
}

/*-----------------------------------------------------------------------*/

int sound_play(Sound *sound, int channel, float volume, float pan, int loop
               __DEBUG_PARAMS)
{
    if (!device_opened) {
        return 0;
    }
    if (UNLIKELY(sound == NULL)
     || UNLIKELY(channel < 0) || UNLIKELY(channel > num_channels)
     || UNLIKELY(volume < 0)
     || UNLIKELY(pan < -1 || pan > 1)) {
        DLOG("Invalid parameters: %p %d %g %g %d",
             sound, channel, volume, pan, loop);
        return 0;
    }

    SoundDecodeHandle *decoder = open_decoder(sound, loop);
    if (!decoder) {
        DLOG("Failed to get a decode handle");
        return 0;
    }

    channel = sound_play_decoder(decoder, channel, volume, pan __DEBUG_ARGS);
    if (channel != 0) {
        channels[channel].sound = sound;
        sound->usage_counter++;
    } else {
        sound_decode_close(decoder);
    }
    return channel;
}

/*-----------------------------------------------------------------------*/

int sound_play_decoder(struct SoundDecodeHandle *decoder, int channel,
                       float volume, float pan __DEBUG_PARAMS)
{
    if (!device_opened) {
        return 0;
    }
    if (UNLIKELY(decoder == NULL)
     || UNLIKELY(channel < 0) || UNLIKELY(channel > num_channels)
     || UNLIKELY(volume < 0)
     || UNLIKELY(pan < -1 || pan > 1)) {
        DLOG("Invalid parameters: %p %d %g %g",
             decoder, channel, volume, pan);
        return 0;
    }

    if (channel != 0) {
        mutex_lock(allocate_channel_mutex);
        if (!channels[channel].reserved) {
            DLOG("Channel %d has not been reserved", channel);
            mutex_unlock(allocate_channel_mutex);
            return 0;
        }
        if (channels[channel].in_use) {
            reset_channel(channel __DEBUG_ARGS);
        }
        channels[channel].in_use = 1;
        mutex_unlock(allocate_channel_mutex);
    } else {
        channel = allocate_channel(0);
        if (channel == 0) {
            DLOG("Failed to allocate a sound channel");
            return 0;
        }
    }

    sound_decode_set_output_freq(decoder, playback_rate);

    sound_mixer_setvol(channels[channel].id, volume);
    sound_mixer_setpan(channels[channel].id, pan);
    channels[channel].paused = 0;
    channels[channel].channel_pause = 0;
    channels[channel].decoder = decoder;
    channels[channel].filter = NULL;

    /* These must come last -- the low-level playback routines may call
     * sound_decode_callback() at any time once sound_mixer_setdata() has
     * been called. */
    ASSERT(sound_mixer_setdata(channels[channel].id,
                               sound_decode_callback, &channels[channel],
                               sound_decode_is_stereo(decoder)),
           return 0);
    sound_mixer_start(channels[channel].id);

    return channel;
}

/*-----------------------------------------------------------------------*/

void sound_pause(int channel)
{
    if (!device_opened) {
        return;
    }
    if (channel < 1 || channel > num_channels) {
        DLOG("Invalid parameters: %d", channel);
        return;
    }
    if (!channels[channel].in_use) {
        return;
    }

    sound_mixer_stop(channels[channel].id);
    channels[channel].paused = 1;
    channels[channel].channel_pause = 1;
}

/*-----------------------------------------------------------------------*/

void sound_resume(int channel)
{
    if (!device_opened) {
        return;
    }
    if (channel < 1 || channel > num_channels) {
        DLOG("Invalid parameters: %d", channel);
        return;
    }
    if (!channels[channel].in_use) {
        return;
    }

    channels[channel].channel_pause = 0;
    if (channels[channel].paused && !global_pause) {
        sound_mixer_start(channels[channel].id);
        channels[channel].paused = 0;
    }
}

/*-----------------------------------------------------------------------*/

void sound_cut(int channel __DEBUG_PARAMS)
{
    if (!device_opened) {
        return;
    }
    if (channel < 1 || channel > num_channels) {
        DLOG("Invalid parameters: %d", channel);
        return;
    }
    if (!channels[channel].in_use) {
        return;
    }

    reset_channel(channel __DEBUG_ARGS);
}

/*-----------------------------------------------------------------------*/

void sound_fade(int channel, float time __DEBUG_PARAMS)
{
    if (!device_opened) {
        return;
    }
    if (channel < 1 || channel > num_channels) {
        DLOG("Invalid parameters: %d %g", channel, time);
        return;
    }
    if (!channels[channel].in_use) {
        return;
    }

    if (time == 0) {
        reset_channel(channel __DEBUG_ARGS);
    } else {
        sound_mixer_setfade(channels[channel].id, 0.0f, time, 1);
    }
}

/*-----------------------------------------------------------------------*/

void sound_adjust_volume(int channel, float new_volume, float time)
{
    if (!device_opened) {
        return;
    }
    if (channel < 1 || channel > num_channels
     || new_volume < 0 || time < 0) {
        DLOG("Invalid parameters: %d %g %g", channel, new_volume, time);
        return;
    }
    if (!channels[channel].in_use) {
        return;
    }

    if (time == 0) {
        sound_mixer_setvol(channels[channel].id, new_volume);
    } else {
        sound_mixer_setfade(channels[channel].id, new_volume, time, 0);
    }
}

/*-----------------------------------------------------------------------*/

void sound_set_pan(int channel, float new_pan)
{
    if (!device_opened) {
        return;
    }
    if (channel < 1 || channel > num_channels
     || new_pan < -1 || new_pan > 1) {
        DLOG("Invalid parameters: %d %g", channel, new_pan);
        return;
    }
    if (!channels[channel].in_use) {
        return;
    }

    sound_mixer_setpan(channels[channel].id, new_pan);
}

/*-----------------------------------------------------------------------*/

void sound_set_playback_rate(int channel, float new_rate)
{
    if (!device_opened) {
        return;
    }
    if (channel < 1 || channel > num_channels || new_rate < 0) {
        DLOG("Invalid parameters: %d %g", channel, new_rate);
        return;
    }
    if (!channels[channel].in_use) {
        return;
    }

    const int native_freq = sound_decode_native_freq(channels[channel].decoder);
    sound_decode_set_decode_freq(channels[channel].decoder,
                                 iroundf(new_rate * native_freq));
}

/*-----------------------------------------------------------------------*/

void sound_set_flange(int channel, int enable, float period, float depth)
{
    if (!device_opened) {
        return;
    }
    if (channel < 1 || channel > num_channels
     || (enable && (period <= 0 || depth < 0))) {
        DLOG("Invalid parameters: %d %d %g %g",
             channel, enable, period, depth);
        return;
    }
    if (!channels[channel].in_use) {
        return;
    }

    lock_filter(&channels[channel], 1);

    if (channels[channel].filter) {
        sound_filter_close(channels[channel].filter);
    }
    if (enable) {
        channels[channel].filter = sound_filter_open_flange(
            sound_decode_is_stereo(channels[channel].decoder), playback_rate,
            period, depth);
        if (!channels[channel].filter) {
            DLOG("Failed to create flange filter");
        }
    } else {  // !enable
        channels[channel].filter = NULL;
    }

    unlock_filter(&channels[channel], 1);
}

/*-----------------------------------------------------------------------*/

void sound_set_filter(int channel, SoundFilterHandle *filter)
{
    if (!device_opened) {
        return;
    }
    if (channel < 1 || channel > num_channels) {
        DLOG("Invalid parameters: %d %p", channel, filter);
        sound_filter_close(filter);
        return;
    }
    if (!channels[channel].in_use) {
        sound_filter_close(filter);
        return;
    }

    lock_filter(&channels[channel], 1);

    if (channels[channel].filter) {
        sound_filter_close(channels[channel].filter);
    }
    channels[channel].filter = filter;

    unlock_filter(&channels[channel], 1);
}

/*-----------------------------------------------------------------------*/

void sound_enable_loop(int channel, int loop)
{
    if (!device_opened) {
        return;
    }
    if (channel < 1 || channel > num_channels) {
        DLOG("Invalid parameters: %d %d", channel, loop);
        return;
    }
    if (!channels[channel].in_use) {
        return;
    }

    sound_decode_enable_loop(channels[channel].decoder, loop);
}

/*-----------------------------------------------------------------------*/

int sound_is_playing(int channel)
{
    if (!device_opened) {
        return 0;
    }
    if (channel < 1 || channel > num_channels) {
        DLOG("Invalid parameters: %d", channel);
        return 0;
    }

    return channels[channel].in_use;
}

/*-----------------------------------------------------------------------*/

float sound_playback_pos(int channel)
{
    if (!device_opened) {
        return 0;
    }
    if (channel < 1 || channel > num_channels) {
        DLOG("Invalid parameters: %d", channel);
        return 0;
    }
    if (!channels[channel].in_use) {
        return 0;
    }

    return sound_decode_get_position(channels[channel].decoder);
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static int sound_decode_callback(void *handle, void *pcm_buffer, int pcm_len)
{
    Channel *channel = (Channel *)handle;

    if (!sound_decode_get_pcm(channel->decoder, pcm_buffer, pcm_len)) {
        return 0;
    }

    /* We deliberately read channel->filter before locking to avoid the
     * lock sequence if no filter is active.  If we read a NULL, then
     * either no filter was ever active or a previous filter has already
     * been cleared, so we'll end up doing the right thing without locking. */
    if (channel->filter) {
        lock_filter(channel, 0);
        int result = sound_filter_filter(channel->filter, pcm_buffer, pcm_len);
        unlock_filter(channel, 0);
        if (!result) {
            return 0;
        }
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

static SoundFormat autodetect_format(const uint8_t *data, int datalen)
{
    if (datalen >= 12
     && memcmp(data, "RIFF", 4) == 0
     && memcmp(data+8, "WAVE", 4) == 0) {
        return SOUND_FORMAT_WAV;
    } else if (datalen >= 2 && data[0] == 0xFF && (data[1] & 0xE0) == 0xE0) {
        return SOUND_FORMAT_MP3;
    } else if (datalen >= 4 && memcmp(data, "OggS", 4) == 0) {
        return SOUND_FORMAT_OGG;
    } else {
        DLOG("Unknown audio data format");
        return (SoundFormat)0;
    }
}

/*-----------------------------------------------------------------------*/

static int find_audio_params(Sound *sound)
{
    PRECOND(sound != NULL, return 0);
    if (!sound->have_audio_params) {
        SoundDecodeHandle *decoder = open_decoder(sound, 0);
        if (!decoder) {
            DLOG("Failed to get a decode handle");
            return 0;
        }
        sound->have_audio_params = 1;
        sound->stereo            = sound_decode_is_stereo(decoder);
        sound->native_freq       = sound_decode_native_freq(decoder);
        sound_decode_close(decoder);
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

static SoundDecodeHandle *open_decoder(Sound *sound, int loop)
{
    SoundDecodeHandle *decoder;
    if (sound->is_file) {
        decoder = sound_decode_open_from_file(
            sound->format, sound->fh, sound->dataofs, sound->datalen, loop,
            enable_interpolation);
    } else {
        decoder = sound_decode_open(
            sound->format, sound->data, sound->datalen, loop,
            enable_interpolation);
    }
    if (sound->looplen >= 0) {
        sound_decode_set_loop_points(decoder,
                                     sound->loopstart, sound->looplen);
    }
    return decoder;
}

/*-----------------------------------------------------------------------*/

static int allocate_channel(int reserve)
{
    mutex_lock(allocate_channel_mutex);

    int channel = 0;
    for (int i = 1; i <= num_channels; i++) {
        if (!channels[i].reserved && !channels[i].in_use) {
            channel = i;
            break;
        }
    }

    if (channel) {
        if (reserve) {
            channels[channel].reserved = 1;
        } else {
            channels[channel].in_use = 1;
        }
    }

    mutex_unlock(allocate_channel_mutex);
    return channel;
}

/*-----------------------------------------------------------------------*/

static void lock_filter(Channel *channel, const int from_main)
{
    if (from_main) {
        channel->filter_lock_main = 1;
        BARRIER();
        channel->filter_lock_turn_is_main = 0;
    } else {
        channel->filter_lock_decode = 1;
        BARRIER();
        channel->filter_lock_turn_is_main = 1;
    }
    BARRIER();
    while ((from_main ? channel->filter_lock_decode : channel->filter_lock_main)
           /* Note that this second condition only plays a role when two
            * threads try to take the lock at the same time:
            *    (main)--- filter_lock_main = 1;
            *              filter_lock_decode = 1; ---------(decode)
            *    (main)--- filter_lock_turn_is_main = 0;
            *              filter_lock_turn_is_main = 1; ---(decode)
            * After the above sequence, the main thread (in this case) will
            * detect filter_lock_decode set but will win the lock anyway
            * because of this filter_lock_turn_is_main check. */
           && channel->filter_lock_turn_is_main != from_main)
    {
#ifdef SIL_INCLUDE_TESTS
        extern uint8_t sound_core_blocked_on_filter_lock;
        sound_core_blocked_on_filter_lock = 1;
#endif
        thread_yield();
    }
}

/*-----------------------------------------------------------------------*/

static void unlock_filter(Channel *channel, const int from_main)
{
    if (from_main) {
        channel->filter_lock_main = 0;
    } else {
        channel->filter_lock_decode = 0;
    }
    BARRIER();
}

/*-----------------------------------------------------------------------*/

static void reset_channel(int channel __DEBUG_PARAMS)
{
    PRECOND(channel >= 1 && channel <= num_channels, return);
    PRECOND(channels[channel].in_use != 0, return);
    PRECOND(channels[channel].decoder != NULL, return);

    sound_mixer_reset(channels[channel].id);

    if (channels[channel].filter) {
        /* Playback is guaranteed to have stopped, so we don't need to lock. */
        sound_filter_close(channels[channel].filter);
    }
    sound_decode_close(channels[channel].decoder);

    Sound *sound = channels[channel].sound;
    if (sound) {
        ASSERT(sound->usage_counter > 0, sound->usage_counter = 1);
        sound->usage_counter--;
        if (sound->usage_counter == 0 && sound->free_on_stop) {
            free_sound(sound __DEBUG_ARGS);
        }
    }

    channels[channel].decoder = NULL;
    channels[channel].filter = NULL;
    channels[channel].sound = NULL;
    channels[channel].paused = 0;
    channels[channel].channel_pause = 0;
    /* Clearing in_use will let allocate_channel() grab the channel (if
     * it's not reserved), so make sure this is the last store. */
    BARRIER();
    channels[channel].in_use = 0;
}

/*-----------------------------------------------------------------------*/

static void free_sound(Sound *sound __DEBUG_PARAMS)
{
    PRECOND(sound != NULL, return);

    if (sound->is_file) {
        sys_file_close(sound->fh);
    } else {
        debug_mem_free(sound->data, file, line);
    }
    debug_mem_free(sound, file, line);
}

/*************************************************************************/
/*************************************************************************/
