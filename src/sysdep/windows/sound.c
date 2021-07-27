/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/windows/sound.c: Windows audio output interface.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sound/mixer.h"
#include "src/sysdep.h"
#include "src/sysdep/windows/internal.h"
#include "src/thread.h"
#include "src/time.h"

/*************************************************************************/
/************************* Configuration options *************************/
/*************************************************************************/

/**
 * PLAYBACK_THREAD_PRIORITY:  Thread priority for the playback thread.
 */
#define PLAYBACK_THREAD_PRIORITY  2

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Have we been initialized? */
static uint8_t sound_initted;

/* Driver for selected audio interface. */
static AudioDriver *driver;

/* avrt.dll handle for AvSetMmThreadCharacteristicsW(). */
static HANDLE avrt_dll;

/* Audio device sampling rate. */
static int sound_rate;

/* Thread ID of playback thread, and flag used to tell it to stop. */
static int playback_thread_id;
static uint8_t playback_thread_stop;

/* Requested new latency, passed from sys_sound_set_latency() to the
 * playback thread. */
static float requested_latency;
/* Semaphore signalled by sys_sound_set_latency() to indicate that
 * requested_num_buffers is valid. */
static SysSemaphoreID latency_change_requested;
/* Semaphore signalled by the playback thread to indicate that the latency
 * change request has been processed. */
static SysSemaphoreID latency_change_complete;

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

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

    /* Choose a sound interface. */
    driver = &windows_winmm_driver;
    avrt_dll = NULL;
    if (windows_version_is_at_least(WINDOWS_VERSION_VISTA)) {
        if (windows_wasapi_init()) {
            driver = &windows_wasapi_driver;
            avrt_dll = LoadLibrary("avrt.dll");
        }
    }
    ASSERT(driver != NULL, goto error_return);

    /* Create synchronization objects. */
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

    /* Open and initialize the audio device. */
    sound_rate = driver->open(device_name);
    if (!sound_rate && driver == &windows_wasapi_driver) {
        /* Fall back to WinMM if WASAPI fails for whatever reason. */
        DLOG("Failed to open device using WASAPI, trying WinMM");
        driver = &windows_winmm_driver;
        sound_rate = driver->open(device_name);
    }
    if (!sound_rate) {
        goto error_destroy_latency_change_complete;
    }

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
    driver->close();
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
    return sound_rate;
}

/*-----------------------------------------------------------------------*/

float sys_sound_set_latency(float latency)
{
    if (latency > 0) {
        requested_latency = latency;
        sys_semaphore_signal(latency_change_requested);
        sys_semaphore_wait(latency_change_complete, -1);
    }

    return driver->get_latency();
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

    if (avrt_dll) {
        FreeLibrary(avrt_dll);
        avrt_dll = NULL;
    }

    ASSERT(driver, goto no_driver);
    driver->close();
    driver = NULL;
  no_driver:

    sys_semaphore_destroy(latency_change_complete);
    latency_change_complete = 0;
    sys_semaphore_destroy(latency_change_requested);
    latency_change_requested = 0;

    sound_initted = 0;
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static int playback_thread(UNUSED void *userdata)
{
    HANDLE (WINAPI *p_AvSetMmThreadCharacteristicsW)(LPCWSTR TaskName, LPDWORD TaskIndex) = NULL;
    void (WINAPI *p_AvRevertMmThreadCharacteristics)(HANDLE AvrtHandle) = NULL;
    if (avrt_dll) {
        p_AvSetMmThreadCharacteristicsW =
            (void *)GetProcAddress(avrt_dll, "AvSetMmThreadCharacteristicsW");
        p_AvRevertMmThreadCharacteristics =
            (void *)GetProcAddress(avrt_dll, "AvRevertMmThreadCharacteristics");
    }

    HANDLE avrt_handle = NULL;
    if (p_AvSetMmThreadCharacteristicsW && p_AvRevertMmThreadCharacteristics) {
        /* The string "Pro Audio" doesn't seem to be explicitly documented
         * anywhere, but the audio playback example on MSDN at
         * <https://msdn.microsoft.com/en-us/library/windows/desktop/dd370844(v=vs.85).aspx>
         * uses that string with a comment suggesting that it boosts the
         * thread priority. */
        static const WCHAR name[] = {'P','r','o',' ','A','u','d','i','o', 0};
        avrt_handle = (*p_AvSetMmThreadCharacteristicsW)(name, (DWORD[1]){0});
        if (UNLIKELY(!avrt_handle)) {
            DLOG("AvSetMmThreadCharacteristicsW() failed: %s",
                 windows_strerror(GetLastError()));
        }
    }

    while (!playback_thread_stop) {
        if (sys_semaphore_wait(latency_change_requested, 0)) {
            driver->set_latency(requested_latency);
            sys_semaphore_signal(latency_change_complete);
        }
        int16_t *buffer;
        int mix_len;
        const int result = driver->get_buffer(0.1, &buffer, &mix_len);
        if (result < 0) {
            /* Consume and discard data from the mixer as though we were
             * processing it, so that client code waiting (e.g.) for a
             * sound to finish playing doesn't end up waiting forever. */
            int16_t tempbuf[256*2];
            time_delay((float)(lenof(tempbuf)/2) / (float)sound_rate);
            sound_mixer_get_pcm(tempbuf, lenof(tempbuf)/2);
        } else if (result > 0) {
            sound_mixer_get_pcm(buffer, mix_len);
            driver->submit_buffer();
        }
    }  // while (!playback_thread_stop)

    if (avrt_handle) {
        (*p_AvRevertMmThreadCharacteristics)(avrt_handle);
    }

    return 0;
}

/*************************************************************************/
/*************************************************************************/
