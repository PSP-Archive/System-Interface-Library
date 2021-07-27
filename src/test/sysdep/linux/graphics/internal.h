/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/linux/graphics/internal.h: Declarations of helper
 * functions for Linux-specific graphics tests.
 */

#ifndef SIL_SRC_TEST_SYSDEP_LINUX_GRAPHICS_INTERNAL_H
#define SIL_SRC_TEST_SYSDEP_LINUX_GRAPHICS_INTERNAL_H

/*************************************************************************/
/*************************************************************************/

/**
 * check_vidmode:  Return whether the XF86VidMode extension is available.
 */
extern int check_vidmode(void);

/**
 * check_xinerama:  Return whether the Xinerama extension is available and
 * active.
 */
extern int check_xinerama(void);

/**
 * check_xrandr:  Return whether XRandR 1.2+ is available.
 */
extern int check_xrandr(void);

/**
 * clear_variables:  Reset all environment and X11 wrapper variables to
 * their initial state (unset / zero).
 */
extern void clear_variables(void);

/**
 * xrandr_get_current_resolution:  Return the current screen resolution for
 * the screen containing the current window.  XRandR must be available, and
 * the window must have been opened on SIL device index 0.
 *
 * [Parameters]
 *     width_ret: Pointer to variable to receive the screen width, in pixels.
 *     height_ret: Pointer to variable to receive the screen height, in pixels.
 */
extern void xrandr_get_current_resolution(int *width_ret, int *height_ret);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_TEST_SYSDEP_LINUX_GRAPHICS_INTERNAL_H
