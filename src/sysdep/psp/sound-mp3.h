/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/sound-mp3.h: PSP MP3 decoding module header.
 */

#ifndef SIL_SRC_SYSDEP_PSP_SOUND_MP3_H
#define SIL_SRC_SYSDEP_PSP_SOUND_MP3_H

struct SoundDecodeHandle;

/*************************************************************************/
/*************************************************************************/

/**
 * psp_decode_mp3_open:  Sound decoder method for initializing MP3 decoding.
 *
 * [Return value]
 *     True on success, false on error.
 */
extern int psp_decode_mp3_open(struct SoundDecodeHandle *this);

/**
 * psp_clean_mp3_garbage:  Destroy all private MP3 decoding data which is
 * no longer in use by any decoding threads.  Must be called periodically
 * from the main thread.
 *
 * [Parameters]
 *     wait: True to wait for threads belonging to closed decoders to
 *         terminate; false to not wait for anything.
 */
extern void psp_clean_mp3_garbage(int wait);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_PSP_SOUND_MP3_H
