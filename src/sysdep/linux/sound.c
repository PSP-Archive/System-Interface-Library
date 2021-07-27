/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/linux/sound.c: Linux audio output interface.  Uses the ALSA
 * library.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/math.h"
#include "src/sound/mixer.h"
#include "src/sysdep.h"
#include "src/thread.h"
#include "src/time.h"

#include <alloca.h>
#include <alsa/asoundlib.h>

/*************************************************************************/
/************************* Configuration options *************************/
/*************************************************************************/

/**
 * MIX_BUFSIZE:  Maximum number of samples to mix and send to the hardware
 * in a single output call.  Should be the same as the mixer buffer length
 * (in sound/mixer.c) for best performance.
 */
#define MIX_BUFSIZE  1024

/**
 * DEFAULT_PCM_BUFSIZE:  Default PCM device buffer size, in samples.
 * Latency is equal to approximately (buffer size * 5/8 / sampling rate),
 * so a buffer size of 1024 samples with a 48kHz output rate gives a
 * latency of around 13 milliseconds.
 */
#define DEFAULT_PCM_BUFSIZE  1024

/**
 * DEFAULT_SOUND_RATE:  Default output sampling rate to use.
 */
#define DEFAULT_SOUND_RATE  48000

/**
 * PLAYBACK_THREAD_PRIORITY:  Thread priority for the playback thread.
 */
#define PLAYBACK_THREAD_PRIORITY  10

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Have we been initialized? */
static int sound_initted;

/* ALSA sound device handle. */
static snd_pcm_t *pcm_handle;
/* Actual playback sampling rate in use. */
static int hw_sound_rate;
/* Number of samples in the internal audio buffer. */
static int hw_buffer_size;
/* Number of samples in the optimum transfer block size. */
static int hw_period_size;

/* Thread ID of playback thread, and flag used to tell it to stop. */
static int playback_thread_id;
static uint8_t playback_thread_stop;

/* Requested new buffer size, passed from sys_sound_set_latency() to the
 * playback thread to avoid having to wrap ALSA calls in locks (since the
 * ALSA library is not thread-safe). */
static int requested_buffer_size;
/* Semaphore signalled by sys_sound_set_latency() to indicate that
 * requested_buffer_size is valid. */
static SysSemaphoreID latency_change_requested;
/* Semaphore signalled by the playback thread to indicate that the latency
 * change request has been processed. */
static SysSemaphoreID latency_change_complete;

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * configure_pcm:  Configure the PCM playback device.  On success, the
 * global variables hw_buffer_size and hw_period_size (and, if force_rate
 * is false, hw_sound_rate) are updated with the actual values set on the
 * device.
 *
 * The caller must hold the alsa_semaphore lock if appropriate.
 *
 * [Parameters]
 *     buffer_size: Playback buffer size to use, in samples.
 *     force_rate: True to force the output rate to be hw_sound_rate;
 *         false to allow the hardware to select a rate.
 * [Return value]
 *     True on success, false on error.
 */
static int configure_pcm(int buffer_size, int force_rate);

/**
 * playback_thread:  Thread which mixes audio data and sends it to the
 * audio output device.
 *
 * [Parameters]
 *     userdata: Opaque user data pointer (unused).
 * [Return value]
 *     0
 */
static int playback_thread(void *userdata);

/**
 * handle_latency_change_request:  Process a latency change request from
 * sys_sound_set_latency().  Helper function for playback_thread().
 *
 * All pending samples must have been drained from the PCM device before
 * calling this function.
 *
 * [Parameters]
 *     new_buffer_size: Requested ALSA buffer size, in samples.
 */
static void handle_latency_change_request(int new_buffer_size);

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int sys_sound_init(const char *device_name)
{
    PRECOND(device_name != NULL, goto error_return);
    if (UNLIKELY(sound_initted)) {
        DLOG("Tried to init twice!");
        goto error_return;
    }

    /* Create latency change semaphores. */
    latency_change_requested = sys_semaphore_create(0, 1);
    if (UNLIKELY(!latency_change_requested)) {
        DLOG("Failed to create latency change request semaphore");
        goto error_return;
    }
    latency_change_complete = sys_semaphore_create(0, 1);
    if (UNLIKELY(!latency_change_complete)) {
        DLOG("Failed to create latency change completion semaphore");
        goto error_destroy_latency_change_requested;
    }

    /* Open the ALSA device. */
    if (!*device_name) {
        device_name = "default";
    }
    pcm_handle = NULL;
    int result = snd_pcm_open(&pcm_handle, device_name,
                              SND_PCM_STREAM_PLAYBACK, 0);
    if (UNLIKELY(result < 0)) {
        DLOG("Failed to open device %s: %s", device_name,
             snd_strerror(result));
        goto error_destroy_latency_change_complete;
    }
    if (UNLIKELY(!configure_pcm(DEFAULT_PCM_BUFSIZE, 0))) {
        DLOG("Failed to configure device %s", device_name);
        goto error_close_device;
    }
    DLOG("Audio output rate: %d Hz, buffer size: %d, period: %d",
         hw_sound_rate, hw_buffer_size, hw_period_size);

    /* Start playback. */
    playback_thread_stop = 0;
    playback_thread_id = thread_create_with_priority(
        PLAYBACK_THREAD_PRIORITY, playback_thread, NULL);
    if (!playback_thread_id) {
        DLOG("Failed to create playback thread for mixer output: %s",
             sys_last_errstr());
        goto error_close_device;
    }

    /* All done. */
    sound_initted = 1;
    return 1;

  error_close_device:
    snd_pcm_close(pcm_handle);
    pcm_handle = NULL;
  error_destroy_latency_change_complete:
    sys_semaphore_destroy(latency_change_complete);
    latency_change_complete = 0;
  error_destroy_latency_change_requested:
    sys_semaphore_destroy(latency_change_requested);
    latency_change_requested = 0;
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

int sys_sound_playback_rate(void)
{
    return hw_sound_rate;
}

/*-----------------------------------------------------------------------*/

float sys_sound_set_latency(float latency)
{
    if (latency > 0) {
        /* We calculate latency as 1/2 buffer + 1/2 period (where period
         * is normally 1/4 buffer), so reverse the calculation before
         * converting to samples. */
        const float new_buffer_time = latency * 1.6f;
        requested_buffer_size =
            lbound(iroundf(new_buffer_time * hw_sound_rate), 1);
        sys_semaphore_signal(latency_change_requested);
        sys_semaphore_wait(latency_change_complete, -1);
    }

    return (float)(hw_buffer_size/2 + hw_period_size/2) / (float)hw_sound_rate;
}

/*-----------------------------------------------------------------------*/

void sys_sound_enable_headphone_disconnect_check(UNUSED int enable)
{
    /* Not supported. */
}

/*-----------------------------------------------------------------------*/

int sys_sound_check_headphone_disconnect(void)
{
    return 0;  // Not supported.
}

/*-----------------------------------------------------------------------*/

void sys_sound_acknowledge_headphone_disconnect(void)
{
    /* Not supported. */
}

/*-----------------------------------------------------------------------*/

void sys_sound_cleanup(void)
{
    if (!sound_initted) {
        return;
    }

    playback_thread_stop = 1;
    thread_wait(playback_thread_id);
    playback_thread_id = 0;

    snd_pcm_close(pcm_handle);
    pcm_handle = NULL;

    sys_semaphore_destroy(latency_change_complete);
    latency_change_complete = 0;
    sys_semaphore_destroy(latency_change_requested);
    latency_change_requested = 0;

    sound_initted = 0;
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static int configure_pcm(int buffer_size, int force_rate)
{
    /* Convenience macro for checking ALSA function return codes. */
    #define CHECK_ALSA(call)  do {                       \
        int result;                                      \
        if (UNLIKELY((result = (call)) < 0)) {           \
            DLOG("%s: %s", #call, snd_strerror(result)); \
            return 0;                                    \
        }                                                \
    } while (0)

    snd_pcm_hw_params_t *hwparams;
    snd_pcm_hw_params_alloca(&hwparams);
    snd_pcm_sw_params_t *swparams;
    snd_pcm_sw_params_alloca(&swparams);

    CHECK_ALSA(snd_pcm_hw_params_any(pcm_handle, hwparams));
    CHECK_ALSA(snd_pcm_hw_params_set_access(
                   pcm_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED));
    CHECK_ALSA(snd_pcm_hw_params_set_format(
                   pcm_handle, hwparams, SND_PCM_FORMAT_S16_LE));
    CHECK_ALSA(snd_pcm_hw_params_set_channels(pcm_handle, hwparams, 2));
    CHECK_ALSA(snd_pcm_hw_params_set_rate_resample(pcm_handle, hwparams,
                                                   0));
    if (force_rate) {
        CHECK_ALSA(snd_pcm_hw_params_set_rate(pcm_handle, hwparams,
                                              hw_sound_rate, 0));
    } else {
        CHECK_ALSA(snd_pcm_hw_params_set_rate_near(
                       pcm_handle, hwparams,
                       (unsigned int[]){DEFAULT_SOUND_RATE}, 0));
    }
    CHECK_ALSA(snd_pcm_hw_params_set_buffer_size_near(
             pcm_handle, hwparams, (snd_pcm_uframes_t[]){buffer_size}));
    CHECK_ALSA(snd_pcm_hw_params_set_period_size_near(
             pcm_handle, hwparams, (snd_pcm_uframes_t[]){buffer_size / 4}, 0));
    CHECK_ALSA(snd_pcm_hw_params(pcm_handle, hwparams));

    unsigned int actual_sound_rate;
    snd_pcm_uframes_t actual_buffer_size, actual_period_size;
    CHECK_ALSA(snd_pcm_hw_params_get_rate(hwparams, &actual_sound_rate, NULL));
    CHECK_ALSA(snd_pcm_hw_params_get_buffer_size(hwparams,
                                                 &actual_buffer_size));
    CHECK_ALSA(snd_pcm_hw_params_get_period_size(hwparams,
                                                 &actual_period_size, NULL));

    /* If the period size got rounded up, increase the buffer size to at
     * least 4 periods to avoid stutter.  Since we can't adjust a
     * parameter set we've already sent to the driver, we just redo the
     * configuration all over again. */
    if (actual_buffer_size < actual_period_size * 4) {
        return configure_pcm(
            max(actual_period_size * 4, buffer_size + actual_period_size),
            force_rate);
    }

    CHECK_ALSA(snd_pcm_sw_params_current(pcm_handle, swparams));
    CHECK_ALSA(snd_pcm_sw_params_set_avail_min(
                   pcm_handle, swparams, actual_buffer_size / 2));
    CHECK_ALSA(snd_pcm_sw_params(pcm_handle, swparams));

    if (force_rate) {
        ASSERT((int)actual_sound_rate == hw_sound_rate);
    } else {
        hw_sound_rate = actual_sound_rate;
    }
    hw_buffer_size = actual_buffer_size;
    hw_period_size = actual_period_size;
    return 1;

    #undef CHECK_ALSA
}

/*-----------------------------------------------------------------------*/

static int playback_thread(UNUSED void *userdata)
{
    /* Mixer output buffer. */
    int16_t pcm_buffer[MIX_BUFSIZE*2];
    /* Current output position in pcm_buffer[]. */
    int pcm_buffer_offset = 0;
    /* Number of unconsumed samples in pcm_buffer[]. */
    int pcm_buffer_avail = 0;

    /* Sample output counter and counter of silent samples for ALSA bug
     * workaround (see below). */
    int samples_played = 0;
    int silent_samples_played = 0;

    /* EPIPE counter for detecting drivers that allow setting period_size
     * or avail_min smaller than they actually support (see below). */
    int check_epipe = 1;
    double check_epipe_limit = time_now() + 1.0;
    int epipe_count = 0;


    while (!playback_thread_stop) {

        /* Process any pending latency change request. */
        if (sys_semaphore_wait(latency_change_requested, 0)) {
            int result;
            if ((result = snd_pcm_drain(pcm_handle)) < 0) {
                DLOG("snd_pcm_drain() failed: %s", snd_strerror(result));
            }
            handle_latency_change_request(requested_buffer_size);
            sys_semaphore_signal(latency_change_complete);
        }

        /* Wait for the device to be ready to receive data. */
        for (int result = -1; result < 0; ) {
            static int warned_timeout = 0;
            const int timeout = (1000 * hw_buffer_size) / hw_sound_rate;
            result = snd_pcm_wait(pcm_handle, timeout);
            if (result < 0 && snd_pcm_recover(pcm_handle, result, 1) != 0) {
                DLOG("Failed waiting for audio device: %s",
                     snd_strerror(errno));
                /* Wait a little while just so we don't spend lots of CPU
                 * time spinning on failing write calls. */
                nanosleep(&(struct timespec){
                              .tv_sec = 0,
                              .tv_nsec = (1000000000/hw_period_size) / 2},
                          NULL);
                break;
            } else if (result == 0) {
                if (!warned_timeout) {
                    DLOG("Audio wait timeout, device may be broken");
                    warned_timeout = 1;
                }
            } else if (result > 0) {
                warned_timeout = 0;
            }
        }

        /* Get some data from the mixer if necessary. */
        if (pcm_buffer_avail <= 0) {
            const int mix_len = ubound(hw_period_size, MIX_BUFSIZE);
            sound_mixer_get_pcm(pcm_buffer, mix_len);
            pcm_buffer_offset = 0;
            pcm_buffer_avail = mix_len;

            /* Work around an ALSA bug on 32-bit systems. */
            if (sizeof(long) == 4) {
                /*
                 * ALSA keeps an internal counter of samples played, which
                 * wraps around at 2^(long_bits-2) -- on 32-bit systems, this
                 * is 0x40000000, or 6+ hours at 48kHz.  Due to ALSA bug 5190,
                 * the library does not compute the available buffer space
                 * properly when the counter is close to the wraparound point;
                 * depending on the ratio of playback to hardware sample rate
                 * or of internal buffer sizes, this can cause the audio stream
                 * to start skipping or looping on a single hardware buffer,
                 * and the condition will not be resolved until a reset
                 * operation is performed on the device.
                 *
                 * To work around this, SIL will (when running on a 32-bit
                 * system) monitor the output audio stream for periods of
                 * extended silence.  When we find such a period within the
                 * last 1/4 of the counter space (2^30 samples), we trigger a
                 * reset on the audio device, under the assumption the silence
                 * will continue for a while longer; this allows us to reset
                 * ALSA's internal counters without any audible impact.  If we
                 * get very close to the counter limit without seeing such a
                 * period of silence, we likewise trigger a reset, accepting
                 * the momentary audio glitch in order to prevent extended
                 * problems later on.
                 *
                 * Of course, this problem can theoretically happen on 64-bit
                 * systems as well, but since it would take about three million
                 * years to occur, we don't worry about it.  If this decision
                 * causes difficulties in Y3M, I'll be happy to take the blame.
                 */
                int is_silent = 1;
                const uint32_t *ptr32 = (const uint32_t *)(void *)pcm_buffer;
                for (int i = 0; i < pcm_buffer_avail; i++) {
                    if (ptr32[i]) {
                        is_silent = 0;
                        break;
                    }
                }
                int do_reset = 0;
                if (is_silent) {
                    silent_samples_played += pcm_buffer_avail;
                    /* Reset after 1 second of silence if we're in the last
                     * quarter of the counter space. */
                    do_reset = (samples_played >= 0x30000000)
                        && (silent_samples_played >= hw_sound_rate);
                } else {
                    silent_samples_played = 0;
                }
                samples_played += pcm_buffer_avail;
                if (samples_played >= 0x3FF00000 && !do_reset) {
                    DLOG("Close to wraparound, resetting audio for ALSA bug"
                         " 5190");
                    do_reset = 1;
                }
                if (do_reset) {
                    int result;
                    if ((result = snd_pcm_drain(pcm_handle)) < 0) {
                        DLOG("snd_pcm_drain() failed: %s",
                             snd_strerror(result));
                    }
                    if ((result = snd_pcm_prepare(pcm_handle)) < 0) {
                        DLOG("snd_pcm_prepare() failed: %s",
                             snd_strerror(result));
                    }
                    silent_samples_played = 0;
                    samples_played = 0;
                }
            }  // if (sizeof(long) == 4)
        }  // if (pcm_buffer_avail <= 0)

        /* Write the data to the device. */
        const uint8_t *ptr =
            ((const uint8_t *)pcm_buffer + pcm_buffer_offset * 4);
        ssize_t out;
        for (;;) {
            snd_pcm_nonblock(pcm_handle, 1);
            out = snd_pcm_writei(pcm_handle, ptr, pcm_buffer_avail);
            snd_pcm_nonblock(pcm_handle, 0);
            if (out >= 0) {
                break;
            } else if (out == -EAGAIN) {
                out = 0;
                break;
            }
            int result = snd_pcm_recover(pcm_handle, out, 1);
            if (result < 0) {
                DLOG("snd_pcm_writei(): %s", snd_strerror(result));
                out = 0;
                break;
            }
            if (check_epipe && time_now() > check_epipe_limit) {
                check_epipe = 0;
            } else if (check_epipe && out == -EPIPE) {
                /*
                 * EPIPE indicates a buffer underrun, which generally means
                 * the audio mixing thread is not outputting data fast
                 * enough for uninterrupted audio playback.  However, EPIPE
                 * can also indicate _over_run of the ALSA-internal
                 * playback buffer if the driver or plugin backing the
                 * audio device claims to be ready to accept input when it
                 * in fact is not ready; this is typically observed as
                 * abnormally fast and stuttery audio, caused by the driver
                 * cancelling playback of existing buffers as it accepts
                 * new ones from the snd_pcm_writei() interface.
                 *
                 * To work around such buggy drivers, we watch for early
                 * EPIPE errors from snd_pcm_writei(), and if we see five
                 * such errors in the space of one second, we assume the
                 * driver is suffering from this issue and increase the
                 * buffer size.  Conveniently, this also addresses
                 * underruns, which are the nominal cause of EPIPE errors.
                 * (This naturally increases latency, but that's better
                 * than unusable audio.)
                 */
                epipe_count++;
                if (epipe_count >= 5) {
                    if (hw_buffer_size >= 16384) {
                        DLOG("Audio driver is stuck in an under/overrun loop"
                             " but buffer size is already %d, giving up",
                             hw_buffer_size);
                        check_epipe = 0;
                        continue;
                    }
                    const int new_buffer_size = hw_buffer_size * 2;
                    DLOG("Audio driver is stuck in an under/overrun loop,"
                         " increasing buffer size to %d...", new_buffer_size);
                    result = snd_pcm_drop(pcm_handle);
                    if (UNLIKELY(result < 0)) {
                        DLOG("snd_pcm_drop() failed: %s",
                             snd_strerror(result));
                    }
                    if (UNLIKELY(!configure_pcm(new_buffer_size, 1))) {
                        DLOG("Failed to reconfigure playback device");
                    } else {
                        DLOG("Audio reconfigured to buffer size: %d,"
                             " period: %d", hw_buffer_size, hw_period_size);
                    }
                    check_epipe_limit = time_now() + 1.0;
                    epipe_count = 0;
                }  // if (epipe_count >= 3)
            }  // if (out == EPIPE)
        }  // until write succeeds
        pcm_buffer_offset += out;
        pcm_buffer_avail -= out;

        BARRIER();
    }  // while (!playback_thread_stop)

    return 0;
}

/*-----------------------------------------------------------------------*/

static void handle_latency_change_request(int new_buffer_size)
{
    int min_size, max_size;
    snd_pcm_hw_params_t *hwparams;
    snd_pcm_hw_params_alloca(&hwparams);
    snd_pcm_uframes_t buffer_size_min, buffer_size_max;
    snd_pcm_uframes_t period_size_min, period_size_max;
    int unused_dir;
    if (snd_pcm_hw_params_any(pcm_handle, hwparams) == 0
     && snd_pcm_hw_params_get_buffer_size_min(hwparams, &buffer_size_min) == 0
     && snd_pcm_hw_params_get_buffer_size_max(hwparams, &buffer_size_max) == 0
     && snd_pcm_hw_params_get_period_size_min(hwparams, &period_size_min,
                                              &unused_dir) == 0
     && snd_pcm_hw_params_get_period_size_max(hwparams, &period_size_max,
                                              &unused_dir) == 0)
    {
        min_size = max(buffer_size_min, period_size_min * 4);
        max_size = min(buffer_size_max, period_size_max * 4);
        if (UNLIKELY(max_size < min_size)) {
            DLOG("ALSA returned bogus buffer size limits: min %d (%d/%d),"
                 " max %d (%d/%d)",
                 min_size, (int)buffer_size_min, (int)period_size_min,
                 max_size, (int)buffer_size_max, (int)period_size_max);
            max_size = min_size;
        }
        if (UNLIKELY(min_size < 4)) {
            DLOG("ALSA returned bogus minimum buffer size %d (%d/%d)",
                 min_size, (int)buffer_size_min, (int)period_size_min);
            min_size = 4;
            max_size = lbound(max_size, 4);
        }
    } else {
        DLOG("Failed to get min/max buffer size, using defaults");
        min_size = 1024;
        max_size = 16384;
    }
    const int final_buffer_size =
        bound(new_buffer_size, min_size, max_size);
    if (final_buffer_size != hw_buffer_size) {
        DLOG("Reconfiguring audio for requested buffer size %d",
             new_buffer_size);
        if (UNLIKELY(!configure_pcm(new_buffer_size, 1))) {
            DLOG("Failed to reconfigure playback device");
        } else {
            DLOG("Audio reconfigured to buffer size: %d,"
                 " period: %d", hw_buffer_size, hw_period_size);
        }
    }
}

/*************************************************************************/
/*************************************************************************/
