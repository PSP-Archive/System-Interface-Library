/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/macosx/input.h: Header for input-related declarations internal
 * to Mac OS X code.
 */

#ifndef SIL_SRC_SYSDEP_MACOSX_INPUT_H
#define SIL_SRC_SYSDEP_MACOSX_INPUT_H

/*************************************************************************/
/*************************************************************************/

/**
 * macosx_quit_requested:  Flag indicating whether a quit was requested by
 * the operating system.  If a quit is requested, this flag is set by the
 * application delegate; the flag is never cleared once set.
 */
extern uint8_t macosx_quit_requested;

/**
 * macosx_update_mouse_grab:  Set whether the mouse pointer should be
 * confined to the current window.
 *
 * [Parameters]
 *     grab: 1 to confine the mouse pointer to the current window; 0 to
 *         allow the mouse pointer to move freely; -1 to use the current
 *         state set by input_grab().
 */
extern void macosx_update_mouse_grab(int grab);

/**
 * macosx_handle_mouse_event:  Process a mouse event.
 *
 * [Parameters]
 *     cocoa_event: Event to process (NSEvent *).
 */
extern void macosx_handle_mouse_event(void *cocoa_event);

/**
 * macosx_handle_scroll_event:  Process a mouse scroll event.
 *
 * [Parameters]
 *     cocoa_event: Event to process (NSEvent *).
 */
extern void macosx_handle_scroll_event(void *cocoa_event);

/**
 * macosx_handle_key_event:  Process a keyboard event.
 *
 * [Parameters]
 *     cocoa_event: Event to process (NSEvent *).
 */
extern void macosx_handle_key_event(void *cocoa_event);

/**
 * macosx_clear_window_input_state:  Generate release-type events for all
 * inputs which are received through the window and which are currently in
 * a non-released or non-neutral state.  Used to avoid input state desync
 * caused by dropped input events during window reconfiguration.
 */
extern void macosx_clear_window_input_state(void);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_MACOSX_INPUT_H
