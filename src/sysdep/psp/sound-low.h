/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/sound-low.h: Low-level PSP sound driver header.
 */

#ifndef SIL_SRC_SYSDEP_PSP_SOUND_LOW_H
#define SIL_SRC_SYSDEP_PSP_SOUND_LOW_H

/*************************************************************************/
/*************************************************************************/

/* Maximum volume for sceAudio calls. */
#define PSP_VOLUME_MAX  0xFFFF

/*-----------------------------------------------------------------------*/

/**
 * PSPSoundCallback:  Type of a playback callback function.  Takes the
 * number of samples to generate, and returns a buffer pointer and volume.
 *
 * The callback function is called from its own thread, so any accesses to
 * shared data must be appropriately protected.
 *
 * [Parameters]
 *     blocksize: Number of samples to be returned.
 *     volume_ret: Pointer to channel volume (0...PSP_VOLUME_MAX).  Set to
 *         the current channel volume on entry; contains the new channel
 *         volume on return.
 *     userdata: User data pointer passed to psp_sound_start_channel().
 * [Return value]
 *     Pointer to buffer containing the requested number of samples, or
 *     NULL for silence.
 */
typedef const void *(*PSPSoundCallback)(int blocksize, int *volume_ret,
                                        void *userdata);

/*-----------------------------------------------------------------------*/

/**
 * psp_sound_start_channel:  Allocate a hardware channel, and start
 * playback using the given callback function.
 *
 * [Parameters]
 *     blocksize: Playback block size, in samples.  Silently capped at 32704.
 *     callback: Playback callback function.
 *     userdata: Opaque user data pointer passed to the callback function.
 *     stacksize: Stack size for the playback thread, in bytes.
 * [Return value]
 *     Hardware channel number (>=0) on success, negative on error.
 */
extern int psp_sound_start_channel(int blocksize, PSPSoundCallback callback,
                                   void *userdata, int stacksize);

/**
 * psp_sound_stop_channel:  Stop playback on the given channel and free it.
 * The channel number passed in must have been a channel returned from
 * psp_sound_start_channel().
 *
 * [Parameters]
 *     channel: Channel to stop.
 */
extern void psp_sound_stop_channel(int channel);

/**
 * psp_sound_low_pause:  Suspend audio output.  Called when processing a
 * system suspend event.
 */
extern void psp_sound_low_pause(void);

/**
 * psp_sound_low_unpause:  Resume audio output.  Called when processing a
 * system resume event.
 */
extern void psp_sound_low_unpause(void);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_PSP_SOUND_LOW_H
