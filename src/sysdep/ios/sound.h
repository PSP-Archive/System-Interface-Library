/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/ios/sound.h: Internal header for the iOS sound driver.
 */

#ifndef SIL_SRC_SYSDEP_IOS_SOUND_H
#define SIL_SRC_SYSDEP_IOS_SOUND_H

/*************************************************************************/
/*************************************************************************/

/**
 * ios_ignore_audio_route_change_delay:  If nonzero, indicates the
 * time_now() timestamp until which audio route changes should be ignored.
 * This is a workaround for an iOS bug/misfeature (reported to Apple as
 * bug 9677380) which sends outdated route change events to the app when
 * it resumes from suspend.
 */
extern double ios_ignore_audio_route_change_until;

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_IOS_SOUND_H
