/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/sound.c: PSP audio output interface.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sound.h"
#include "src/sound/decode.h"  // For sound_decode_set_handler().
#include "src/sound/mixer.h"
#include "src/sysdep.h"
#include "src/sysdep/psp/internal.h"
#include "src/sysdep/psp/sound-low.h"
#include "src/sysdep/psp/sound-mp3.h"

/*************************************************************************/
/************************* Configuration options *************************/
/*************************************************************************/

/**
 * SOUND_BUFLEN:  The playback buffer length, in samples.  Should be the
 * same as the mixer buffer length (in sound/mixer.c) for best performance.
 */
#define SOUND_BUFLEN  1024

/**
 * SOUNDGEN_STACK_SIZE:  The stack size for the sound generator/mixer
 * thread, in bytes.
 */
#define SOUNDGEN_STACK_SIZE  16384

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Have we been initialized? */
static int sound_initted;

/* Hardware channel allocated for playback. */
static int psp_sound_channel = -1;

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * sound_callback:  Sound generator callback.  Called by the low-level
 * sound driver when the hardware is ready for the next audio buffer.
 *
 * [Parameters]
 *     blocksize: Number of samples to generate.
 *     volume_ret: Pointer to variable to receive volume (0...PSP_VOLUME_MAX).
 *     userdata: Opaque pointer passed to psp_sound_start_channel() (unused).
 * [Return value]
 *     Buffer containing audio data for playback, or NULL for silence.
 */
static const void *sound_callback(int blocksize, int *volume_ret,
                                  void *userdata);

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int sys_sound_init(const char *device_name)
{
    PRECOND(device_name != NULL, return 0);
    if (sound_initted) {
        DLOG("Tried to init twice!");
        return 0;
    }

    /* We ignore the device name, since there's no concept of multiple
     * output devices on the PSP. */

    sound_decode_set_handler(SOUND_FORMAT_MP3, psp_decode_mp3_open);

    psp_sound_channel = psp_sound_start_channel(SOUND_BUFLEN, sound_callback,
                                                NULL, SOUNDGEN_STACK_SIZE);
    if (psp_sound_channel < 0) {
        DLOG("Failed to allocate primary audio channel");
        return 0;
    }

    sound_initted = 1;
    return 1;
}

/*-----------------------------------------------------------------------*/

int sys_sound_playback_rate(void)
{
    return SOUND_RATE;
}

/*-----------------------------------------------------------------------*/

float sys_sound_set_latency(UNUSED float latency)
{
    /* We don't support changing the latency. */
    return (float)(SOUND_BUFLEN*3/2) / (float)SOUND_RATE;
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

    psp_sound_stop_channel(psp_sound_channel);
    psp_sound_channel = -1;
    sound_initted = 0;
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static const void *sound_callback(UNUSED int blocksize, int *volume_ret,
                                  UNUSED void *userdata)
{
    PRECOND(volume_ret != NULL, return NULL);

    static int16_t audiobuf[2][SOUND_BUFLEN*2];
    static unsigned int buffer = 0;

    sound_mixer_get_pcm(audiobuf[buffer], SOUND_BUFLEN);

    *volume_ret = PSP_VOLUME_MAX;
    const void *retval = audiobuf[buffer];
    buffer = (buffer+1) % lenof(audiobuf);
    return retval;
}

/*************************************************************************/
/*************************************************************************/
