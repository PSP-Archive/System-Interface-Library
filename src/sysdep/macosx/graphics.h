/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/macosx/graphics.h: Header for graphics functions internal to
 * Mac OS X code.
 */

#ifndef SIL_SRC_SYSDEP_MACOSX_GRAPHICS_H
#define SIL_SRC_SYSDEP_MACOSX_GRAPHICS_H

#include "src/sysdep/macosx/osx-headers.h"

/* Work around name collisions with Mach headers (included indirectly by
 * CoreGraphics.h). */
#undef semaphore_create
#undef semaphore_destroy
#undef semaphore_wait
#undef semaphore_signal
#undef thread_create

#include <CoreGraphics/CoreGraphics.h>

#define semaphore_create   sil_semaphore_create
#define semaphore_destroy  sil_semaphore_destroy
#define semaphore_wait     sil_semaphore_wait
#define semaphore_signal   sil_semaphore_signal
#define thread_create      sil_thread_create

/*************************************************************************/
/*************************************************************************/

/**
 * macosx_display_id:  Return the CoreGraphics display ID for the given
 * display device.
 */
extern PURE_FUNCTION CGDirectDisplayID macosx_display_id(int display);

/**
 * macosx_window:  Return the currently open window, or NULL if none.
 *
 * The return value is a pointer to an instance of type SILWindow (a
 * subclass of NSWindow), but it is declared as "void *" to avoid errors
 * when included in C code.
 */
extern PURE_FUNCTION void *macosx_window(void);

/**
 * macosx_window_x, macosx_window_y:  Return the X or Y pixel coordinate
 * of the current window's origin, or 0 if no window is open.
 */
extern int macosx_window_x(void);
extern int macosx_window_y(void);

/**
 * macosx_window_width, macosx_window_height:  Return the width or height
 * of the current window, in pixels, or 0 if no window is open.
 */
extern int macosx_window_width(void);
extern int macosx_window_height(void);

/**
 * macosx_get_window_title:  Return the title of the current window, or
 * NULL if no window is open.  The returned string should be freed with
 * mem_free() when no longer needed.
 *
 * [Return value]
 *     Title of currently open window, or NULL if no window is open.
 */
extern char *macosx_get_window_title(void);

/**
 * macosx_close_window:  Close the currently open window, if any.  Intended
 * primarily for use by test routines.
 */
extern void macosx_close_window(void);

/**
 * macosx_handle_focus_change:  Perform any needed operations on a focus
 * state change.
 *
 * [Parameters]
 *     focus: True if the window just gained focus; false if the window
 *         just lost focus.
 */
extern void macosx_handle_focus_change(int focus);

/**
 * macosx_reset_video_mode:  If the video mode of the display device has
 * been changed, reset it to the original mode.
 */
extern void macosx_reset_video_mode(void);

/**
 * macosx_window_changed_fullscreen:  Called from window functions to
 * indicate that the window has changed between windowed and fullscreen
 * mode via OS interfaces (such as the fullscreen button in the title bar).
 *
 * [Parameters]
 *     window: Window which changed to fullscreen mode.
 *     fullscreen: True if the window is now in fullscreen mode; false if
 *         the window is now in windowed mode.
 */
extern void macosx_window_changed_fullscreen(void *window, int fullscreen);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_MACOSX_GRAPHICS_H
