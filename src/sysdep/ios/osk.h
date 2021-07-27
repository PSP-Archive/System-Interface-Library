/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/ios/osk.h: Interface to the iOS on-screen keyboard.
 */

#ifndef SIL_SRC_SYSDEP_IOS_OSK_H
#define SIL_SRC_SYSDEP_IOS_OSK_H

/*************************************************************************/
/*************************************************************************/

/**
 * ios_osk_open:  Display the on-screen keyboard, along with an input field
 * and optional prompt text at the top of the screen, and begin accepting
 * user input.  Fails if the keyboard is already displayed.
 *
 * [Parameters]
 *     text: Default text to display in the input field.
 *     prompt: Prompt text to display, or NULL for none.
 */
extern void ios_osk_open(const char *text, const char *prompt);

/**
 * ios_osk_is_running:  Returns whether the on-screen keyboard is currently
 * open and processing input from the user (that is, the user has not yet
 * confirmed the input string).
 *
 * [Return value]
 *     True if the OSK is open and running, false if not.
 */
extern int ios_osk_is_running(void);

/**
 * ios_osk_get_text:  Return the text entered by the user as a sequence of
 * Unicode character values terminated by the value zero.  This function
 * may only be called after the user has entered a string and before the
 * OSK is closed.
 *
 * [Return value]
 *     Array of characters representing entered text, or NULL if the OSK is
 *     not open or still processing input.
 */
extern const int *ios_osk_get_text(void);

/**
 * ios_osk_close:  Hide the on-screen keyboard if it is displayed.
 */
extern void ios_osk_close(void);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_IOS_OSK_H
