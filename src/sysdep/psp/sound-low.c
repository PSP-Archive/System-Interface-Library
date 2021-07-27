/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/sound-low.c: Low-level PSP sound driver.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/sysdep.h"
#include "src/sysdep/psp/internal.h"
#include "src/sysdep/psp/sound-low.h"
#include "src/sysdep/psp/thread.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Maximum number of threads to create (set equal to the number of
 * hardware channels). */
#define MAX_THREADS  8

/* Thread data structure. */
typedef struct SoundThreadInfo_ {
    SceUID handle;        // Thread handle (0 = entry is unused)
    SceUID suspend_sema;  // Semaphore used to suspend thread
    int8_t channel;       // Hardware channel number (0-7)
    uint8_t stop;         // Stop request flag (set by main thread)
    int16_t blocksize;    // Samples per block
    PSPSoundCallback callback;  // Playback callback
    void *userdata;       // Data pointer to pass to callback
} SoundThreadInfo;
static SoundThreadInfo threads[MAX_THREADS];

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * stop_channel:  Stop the given playback thread.
 *
 * [Parameters]
 *     info: SoundThreadInfo pointer for thread to stop.
 */
static void stop_channel(SoundThreadInfo *info);

/**
 * sound_thread:  Playback thread for a single channel.  Loops continuously
 * over the callback function, retrieving a block of data and passing it to
 * the hardware, until the channel's stop flag is set.
 *
 * [Parameters]
 *     args: Argument block size (always 4).
 *     argp: Argument pointer (contains a pointer to the SoundThreadInfo
 *         for the thread).
 * [Return value]
 *     0
 */
static int sound_thread(SceSize args, void *argp);

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int psp_sound_start_channel(int blocksize, PSPSoundCallback callback,
                            void *userdata, int stacksize)
{
    if (UNLIKELY(blocksize < 0)
     || UNLIKELY(!callback)
     || UNLIKELY(stacksize <= 0)
    ) {
        DLOG("Invalid parameters: %d %p %p %d",
             blocksize, callback, userdata, stacksize);
        goto error_return;
    }

    if (blocksize > 32768-64) {
        blocksize = 32768-64;
    }

    /* Look for an unused slot in the thread table. */
    unsigned int index;
    for (index = 0; index < lenof(threads); index++) {
        if (!threads[index].handle) {
            break;
        }
    }
    if (index >= lenof(threads)) {
        DLOG("No thread slots available for blocksize %d callback %p",
             blocksize, callback);
        goto error_return;
    }

    /* Allocate a free hardware channel. */
    int channel = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, blocksize,
                                    PSP_AUDIO_FORMAT_STEREO);
    if (UNLIKELY(channel < 0)) {
        DLOG("Failed to allocate channel: %s", psp_strerror(channel));
        goto error_return;
    }

    /* Start a playback thread on the channel. */
    char namebuf[100];
    strformat(namebuf, sizeof(namebuf), "SoundCh%dSema", channel);
    threads[index].suspend_sema = sceKernelCreateSema(namebuf, 0, 1, 1, NULL);
    if (UNLIKELY(threads[index].suspend_sema < 0)) {
        DLOG("Failed to create suspend semaphore: %s",
             psp_strerror(threads[index].suspend_sema));
        goto error_free_sound_channel;
    }
    strformat(namebuf, sizeof(namebuf), "SoundCh%d", channel);
    threads[index].channel   = channel;
    threads[index].blocksize = blocksize;
    threads[index].callback  = callback;
    threads[index].userdata  = userdata;
    threads[index].stop      = 0;
    SoundThreadInfo *infoptr = &threads[index];
    int handle = psp_start_thread(namebuf, sound_thread,
                                  THREADPRI_SOUND, stacksize,
                                  sizeof(infoptr), &infoptr);
    if (UNLIKELY(handle < 0)) {
        DLOG("Failed to create thread: %s", psp_strerror(handle));
        goto error_destroy_sema;
    }

    /* Success! */
    threads[index].handle = handle;
    return channel;

  error_destroy_sema:
    sceKernelDeleteSema(threads[index].suspend_sema);
  error_free_sound_channel:
    sceAudioChRelease(channel);
  error_return:
    return -1;
}

/*-----------------------------------------------------------------------*/

void psp_sound_stop_channel(int channel)
{
    unsigned int index;
    for (index = 0; index < lenof(threads); index++) {
        if (threads[index].channel == channel && threads[index].handle != 0) {
            break;
        }
    }
    if (index >= lenof(threads)) {
        DLOG("No thread found for channel %d", channel);
        return;
    }
    stop_channel(&threads[index]);
}

/*-----------------------------------------------------------------------*/

void psp_sound_low_pause(void)
{
    for (int i = 0; i < lenof(threads); i++) {
        if (threads[i].handle) {
            sceKernelWaitSema(threads[i].suspend_sema, 1, NULL);
        }
    }
}

/*-----------------------------------------------------------------------*/

void psp_sound_low_unpause(void)
{
    for (int i = 0; i < lenof(threads); i++) {
        if (threads[i].handle) {
            sceKernelSignalSema(threads[i].suspend_sema, 1);
        }
    }
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static void stop_channel(SoundThreadInfo *info)
{
    info->stop = 1;
    BARRIER();
    while (!psp_delete_thread_if_stopped(info->handle, NULL)) {
        sceKernelDelayThread(100);
    }
    info->handle = 0;
    sceKernelDeleteSema(info->suspend_sema);
    sceAudioChRelease(info->channel);
}

/*-----------------------------------------------------------------------*/

static int sound_thread(UNUSED SceSize args, void *argp)
{
    SoundThreadInfo * const info = *(SoundThreadInfo **)argp;

    while (!info->stop) {
        sceKernelWaitSema(info->suspend_sema, 1, NULL);
        int volume = -1;
        const void *data = (*info->callback)(info->blocksize, &volume,
                                             info->userdata);
        if (data) {
            sceAudioOutputBlocking(info->channel, volume, data);
        } else {
            sceKernelDelayThread(10000);
        }
        sceKernelSignalSema(info->suspend_sema, 1);
    }

    return 0;
}

/*************************************************************************/
/*************************************************************************/
