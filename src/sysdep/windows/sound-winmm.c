/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/windows/sound-winmm.c: Windows audio output implementation
 * using the waveOut API.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sound/mixer.h"
#include "src/sysdep.h"
#include "src/sysdep/windows/internal.h"
#include "src/thread.h"

/* Some old MinGW installations don't have these. */
#ifndef WAVE_FORMAT_48S16
# define WAVE_FORMAT_48S16  (1 << 15)
# define WAVE_FORMAT_96S16  (1 << 19)
#endif

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Audio device handle. */
static HWAVEOUT wave_device;

/* Event object used to wait for buffer playback completion. */
static HANDLE completion_event;

/* Audio device sampling rate. */
static int sound_rate;

/* Number of samples per audio buffer. */
static int buffer_len;
/* Number of audio buffers in use. */
static int num_buffers;
/* Audio buffer array. */
typedef struct AudioBuffer AudioBuffer;
struct AudioBuffer {
    WAVEHDR *header;
    int16_t *data;
};
static AudioBuffer *buffers;
/* Next buffer to fill. */
int next_buffer;

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * adjust_num_buffers:  Change the number of buffers used for audio output.
 * On entry, buffers[0] should be the next buffer to be filled with audio
 * data for output.
 *
 * If reducing the number of buffers, this function will block until the
 * number of queued buffers is less than or equal to new_num_buffers.
 *
 * [Parameters]
 *     new_num_buffers: Desired number of buffers.
 * [Return value]
 *     True on success, false on error.
 */
static int adjust_num_buffers(int new_num_buffers);

/**
 * init_buffer:  Initialize a new AudioBuffer instance.
 *
 * [Parameters]
 *     buffer: Instance to initialize.
 * [Return value]
 *     True on success, false on error.
 */
static int init_buffer(AudioBuffer *buffer);

/**
 * deinit_buffer:  Free resources associated with an AudioBuffer instance.
 *
 * [Parameters]
 *     buffer: Instance to deinitialize.
 */
static void deinit_buffer(AudioBuffer *buffer);

/*************************************************************************/
/*********************** Driver interface routines ***********************/
/*************************************************************************/

static int winmm_open(const char *device_name)
{
    /* Create the buffer synchronization event object.  We use this in
     * auto-reset style, but the waveOut docs explicitly specify a
     * manual-reset object, so that's what we do. */
    completion_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (UNLIKELY(!completion_event)) {
        DLOG("Failed to create completion event object: %s",
             windows_strerror(GetLastError()));
        goto error_return;
    }

    /* Validate and open the audio device. */
    MMRESULT result;
    WAVEOUTCAPS caps;
    const int num_devices = waveOutGetNumDevs();
    int device_index = -1;
    for (int i = 0; i < num_devices; i++) {
        result = waveOutGetDevCaps(i, &caps, sizeof(caps));
        if (UNLIKELY(result != MMSYSERR_NOERROR)) {
            DLOG("waveOutGetDevCaps(%d) failed: %d", i, result);
        } else if (!*device_name || strcmp(caps.szPname, device_name) == 0) {
            device_index = i;
            break;
        }
    }
    if (device_index < 0) {
        if (*device_name) {
            DLOG("Device \"%s\" not found", device_name);
        } else {
            DLOG("No audio output devices found");
        }
        goto error_destroy_completion_event;
    }
    DLOG("Device %d:\n"
         "        Name: %s\n"
         "     Version: %d.%d\n"
         "     Formats: 0x%X\n"
         "    Channels: %d\n"
         "     Support: 0x%X",
         device_index, caps.szPname, HIBYTE(caps.vDriverVersion),
         LOBYTE(caps.vDriverVersion), caps.dwFormats, caps.wChannels,
         caps.dwSupport);
    if (caps.dwFormats & WAVE_FORMAT_48S16) {
        sound_rate = 48000;
        /* A buffer length of 512 at 48kHz (~10ms) results in stuttering in
         * some environments even if the playback thread is keeping up, so
         * we use a base buffer period of ~20ms. */
        buffer_len = 1024;
    } else if (caps.dwFormats & WAVE_FORMAT_4S16) {
        sound_rate = 44100;
        buffer_len = 1024;
    } else if (caps.dwFormats & WAVE_FORMAT_96S16) {
        sound_rate = 96000;
        buffer_len = 2048;
    } else if (caps.dwFormats & WAVE_FORMAT_2S16) {
        sound_rate = 22050;
        buffer_len = 512;
    } else if (caps.dwFormats & WAVE_FORMAT_1S16) {
        sound_rate = 11025;
        buffer_len = 256;
    } else {
        DLOG("No supported sample rate/type found");
        goto error_destroy_completion_event;
    }
    DLOG("Using output rate %d Hz, buffer size %d", sound_rate, buffer_len);
    result = waveOutOpen(&wave_device, device_index,
                         &(WAVEFORMATEX){.wFormatTag = WAVE_FORMAT_PCM,
                                         .nChannels = 2,
                                         .nSamplesPerSec = sound_rate,
                                         .nAvgBytesPerSec = sound_rate * 4,
                                         .nBlockAlign = 4,
                                         .wBitsPerSample = 16,
                                         .cbSize = 0},
                         (uintptr_t)completion_event, 0,
                         WAVE_MAPPED | CALLBACK_EVENT);
    if (UNLIKELY(result != MMSYSERR_NOERROR)) {
        DLOG("Failed to open device %d: %d", device_index, result);
        goto error_destroy_completion_event;
    }

    /* Allocate the initial mixing buffers. */
    buffers = NULL;
    num_buffers = 0;
    next_buffer = 0;
    const int initial_num_buffers = 3;  // 2.5 * 20ms = average latency of 50ms
    if (!adjust_num_buffers(initial_num_buffers)) {
        DLOG("No memory for %d mixing buffers", num_buffers);
        goto error_close_winmm_device;
    }

    ResetEvent(completion_event);  // Might not be needed?
    return sound_rate;

  error_close_winmm_device:
    ASSERT(waveOutClose(wave_device) == MMSYSERR_NOERROR);
    wave_device = NULL;
  error_destroy_completion_event:
    CloseHandle(completion_event);
    completion_event = NULL;
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

static void winmm_close(void)
{
    PRECOND(wave_device != NULL, return);

    ASSERT(waveOutReset(wave_device) == MMSYSERR_NOERROR);
    for (int i = 0; i < num_buffers; i++) {
        ASSERT(waveOutUnprepareHeader(wave_device, buffers[i].header,
                                      sizeof(*buffers[i].header))
               == MMSYSERR_NOERROR);
    }
    ASSERT(waveOutClose(wave_device) == MMSYSERR_NOERROR);
    wave_device = NULL;

    for (int i = 0; i < num_buffers; i++) {
        mem_free(buffers[i].header);
        mem_free(buffers[i].data);
    }
    mem_free(buffers);
    buffers = NULL;

    CloseHandle(completion_event);
    completion_event = NULL;
}

/*-----------------------------------------------------------------------*/

static float winmm_get_latency(void)
{
    return ((float)(num_buffers * buffer_len - buffer_len/2)
            / (float)sound_rate);
}

/*-----------------------------------------------------------------------*/

static void winmm_set_latency(float latency)
{
    /* We calculate latency as (num_buffers - 0.5) * buffer_period, so
     * reverse the calculation before converting to samples. */
    const float new_buffer_time = latency + (float)(buffer_len/2) / buffer_len;
    const int new_buffer_len = iroundf(new_buffer_time * sound_rate);
    const int new_num_buffers =
        lbound((new_buffer_len + buffer_len/2) / buffer_len, 2);

    /* Rotate the current buffer list so the next buffer to fill is buffer 0.
     * adjust_num_buffers() requires this in order to simplify the
     * grow/shrink logic. */
    for (; next_buffer > 0; next_buffer--) {
        AudioBuffer temp = buffers[0];
        memmove(&buffers[0], &buffers[1], sizeof(*buffers) * (num_buffers-1));
        buffers[num_buffers-1] = temp;
    }
    if (new_num_buffers > num_buffers) {
        /* adjust_num_buffers() will insert the new buffers at the
         * beginning of the array. */
        next_buffer += num_buffers - new_num_buffers;
    }
    adjust_num_buffers(new_num_buffers);
}

/*-----------------------------------------------------------------------*/

static int winmm_get_buffer(float timeout, int16_t **buffer_ret, int *size_ret)
{
    if (!(buffers[next_buffer].header->dwFlags & WHDR_DONE)) {
        const DWORD result =
            WaitForSingleObject(completion_event, iceilf(timeout*1000));
        if (result == WAIT_OBJECT_0) {
            ResetEvent(completion_event);
        } else if (result == WAIT_TIMEOUT) {
            return 0;
        } else {
            DLOG("Completion event wait failed: %s",
                 result==WAIT_ABANDONED ? "Wait abandoned"
                                        : windows_strerror(GetLastError()));
            return -1;
        }
    }
    *buffer_ret = buffers[next_buffer].data;
    *size_ret = buffer_len;
    return 1;
}

/*-----------------------------------------------------------------------*/

static void winmm_submit_buffer(void)
{
    buffers[next_buffer].header->dwFlags &= ~WHDR_DONE;
    MMRESULT result = waveOutWrite(wave_device,
                                   buffers[next_buffer].header,
                                   sizeof(*buffers[next_buffer].header));
    if (UNLIKELY(result != MMSYSERR_NOERROR)) {
        DLOG("Failed to write audio: %d", result);
        buffers[next_buffer].header->dwFlags |= WHDR_DONE;
    }

    next_buffer = (next_buffer + 1) % num_buffers;
}

/*-----------------------------------------------------------------------*/

AudioDriver windows_winmm_driver = {
    .open = winmm_open,
    .close = winmm_close,
    .get_latency = winmm_get_latency,
    .set_latency = winmm_set_latency,
    .get_buffer = winmm_get_buffer,
    .submit_buffer = winmm_submit_buffer,
};

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static int adjust_num_buffers(int new_num_buffers)
{
    PRECOND(new_num_buffers > 0, return 0);

    AudioBuffer *new_buffers =
        mem_alloc(sizeof(*buffers) * new_num_buffers, 0, 0);
    if (UNLIKELY(!new_buffers)) {
        DLOG("No memory for new buffer list (length %d)", new_num_buffers);
        goto error_return;
    }

    if (new_num_buffers < num_buffers) {
        const int num_deleted_buffers = num_buffers - new_num_buffers;
        for (int i = 0; i < num_deleted_buffers; i++) {
            while (!(buffers[i].header->dwFlags & WHDR_DONE)) {
                WaitForSingleObject(completion_event, INFINITE);
                ResetEvent(completion_event);
            }
            deinit_buffer(&buffers[i]);
        }
        memcpy(new_buffers, &buffers[num_deleted_buffers],
               sizeof(*buffers) * new_num_buffers);
    } else {
        const int num_inserted_buffers = new_num_buffers - num_buffers;
        for (int i = 0; i < num_inserted_buffers; i++) {
            if (UNLIKELY(!init_buffer(&new_buffers[i]))) {
                for (i--; i >= 0; i--) {
                    deinit_buffer(&new_buffers[i]);
                }
                goto error_free_new_buffers;
            }
        }
        memcpy(&new_buffers[num_inserted_buffers], buffers,
               sizeof(*buffers) * num_buffers);
    }

    buffers = new_buffers;
    num_buffers = new_num_buffers;
    return 1;

  error_free_new_buffers:
    mem_free(new_buffers);
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

static int init_buffer(AudioBuffer *buffer)
{
    PRECOND(buffer != NULL, return 0);

    buffer->header = mem_alloc(sizeof(*buffer->header), 0, MEM_ALLOC_CLEAR);
    /* Aligning the PCM buffers may or may not help performance, but it
     * certainly shouldn't hurt. */
    const int buffer_size = buffer_len * 4;
    buffer->data = mem_alloc(buffer_size, 64, 0);
    if (UNLIKELY(!buffer->header) || UNLIKELY(!buffer->data)) {
        DLOG("No memory for new buffer (%d bytes)",
             sizeof(*buffer->header) + buffer_size);
        goto error_free;
    }

    buffer->header->lpData = (void *)buffer->data;
    buffer->header->dwBufferLength = buffer_size;
    const MMRESULT result = waveOutPrepareHeader(wave_device, buffer->header,
                                                 sizeof(*buffer->header));
    if (UNLIKELY(result != MMSYSERR_NOERROR)) {
        DLOG("Failed to prepare buffer: %d", result);
        goto error_free;
    }

    buffer->header->dwFlags |= WHDR_DONE;
    return 1;

  error_free:
    mem_free(buffer->header);
    mem_free(buffer->data);
    return 0;
}

/*-----------------------------------------------------------------------*/

static void deinit_buffer(AudioBuffer *buffer)
{
    PRECOND(buffer != NULL, return);

    ASSERT(waveOutUnprepareHeader(wave_device, buffer->header,
                                  sizeof(*buffer->header))
           == MMSYSERR_NOERROR);
    mem_free(buffer->header);
    mem_free(buffer->data);
}

/*************************************************************************/
/*************************************************************************/
