/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/ios/dialog.h: Header for displaying dialog boxes on iOS.
 */

#ifndef SIL_SRC_SYSDEP_IOS_DIALOG_H
#define SIL_SRC_SYSDEP_IOS_DIALOG_H

#include <CoreFoundation/CoreFoundation.h>

/*************************************************************************/
/*************************************************************************/

/**
 * ios_dialog:  Display a simple dialog box with the given title and
 * message text, and wait for the user to click "OK".
 *
 * [Parameters]
 *     title: Dialog title (as a CFString).
 *     text: Dialog text (as a CFString).
 */
extern void ios_dialog(CFStringRef title, CFStringRef text);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_IOS_DIALOG_H
