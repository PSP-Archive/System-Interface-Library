/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/ios/input.h: Header for internal iOS input-related routines.
 */

#ifndef SIL_SRC_SYSDEP_IOS_INPUT_H
#define SIL_SRC_SYSDEP_IOS_INPUT_H

/*************************************************************************/
/*************************************************************************/

/**
 * ios_forward_input_event:  Pass the given InputEvent to the callback
 * function registered with sys_input_init().
 *
 * [Parameters]
 *     event: Event to forward.
 */
extern void ios_forward_input_event(const InputEvent *event);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_IOS_INPUT_H
