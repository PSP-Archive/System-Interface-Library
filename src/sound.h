/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sound.h: Internal header for sound-related functionality.
 */

#ifndef SIL_SRC_SOUND_H
#define SIL_SRC_SOUND_H

#include "SIL/sound.h"  // Include the public header.

/*************************************************************************/
/*************************************************************************/

/**
 * sound_init:  Initialize the sound playback functionality.  Playback
 * cannot actually be performed until an output device has been opened
 * with sound_open_device().
 *
 * It is invalid to call any other sound functions except sound_open_device()
 * without first calling sound_init().
 */
extern void sound_init(void);

/**
 * sound_cleanup:  Shut down the sound playback functionality.  Does
 * nothing if sound_init() has not been successfully called.
 */
extern void sound_cleanup(void);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SOUND_H
