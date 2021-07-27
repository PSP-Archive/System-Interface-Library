/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/macosx/dialog.h: Header for Mac OS X dialog box routine.
 */

#ifndef SIL_SRC_SYSDEP_MACOSX_DIALOG_H
#define SIL_SRC_SYSDEP_MACOSX_DIALOG_H

#include <CoreFoundation/CoreFoundation.h>

/*************************************************************************/
/*************************************************************************/

/**
 * macosx_dialog:  Display a simple dialog box with the given title and
 * message text, and wait for the user to click "OK".
 *
 * [Parameters]
 *     title: Dialog title.
 *     text: Dialog text.
 */
extern void macosx_dialog(CFStringRef title, CFStringRef text);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_MACOSX_DIALOG_H
