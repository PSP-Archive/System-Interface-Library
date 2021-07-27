/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/linux/internal.h: Common header for declarations internal to
 * Linux-specific code.
 */

#ifndef SIL_SRC_SYSDEP_LINUX_INTERNAL_H
#define SIL_SRC_SYSDEP_LINUX_INTERNAL_H

#include <X11/Xlib.h>  // For Display, Window declarations.

/*************************************************************************/
/******************* Internal constants and data types *******************/
/*************************************************************************/

/**
 * WindowManagerID:  Enumerated type identifying the window manager under
 * which the program is running, if known.  Note that many of these
 * enumerants are unused in the code itself, but we include them and the
 * associated detection code (see detect_window_manager()) to document how
 * each WM can be identified.
 */
typedef enum WindowManagerID WindowManagerID;
enum WindowManagerID {
    WM_UNNAMED = 0,  // Pre-EWMH, or does not supply _NET_WM_SUPPORTING_CHECK.
    WM_UNKNOWN,      // EWMH but with an unknown name.
    WM_AWESOME,
    WM_BLACKBOX,
    WM_BSPWM,
    WM_CWM,
    WM_ECHINUS,
    WM_ENLIGHTENMENT,
    WM_FLUXBOX,
    WM_FVWM,
    WM_GNOME_SHELL,
    WM_GOOMWWM,
    WM_HERBSTLUFTWM,
    WM_I3,
    WM_ICEWM,
    WM_JWM,
    WM_KWIN,
    WM_LWM,
    WM_MARCO,
    WM_MATWM2,
    WM_METACITY,
    WM_MUSCA,
    WM_MUFFIN,
    WM_MUTTER,
    WM_MWM,
    WM_NOTION,
    WM_OPENBOX,
    WM_OROBORUS,
    WM_PEKWM,
    WM_QTILE,
    WM_SAWFISH,
    WM_SPECTRWM,
    WM_WINDOWMAKER,
    WM_WMII,
    WM__NUM
};

/*************************************************************************/
/***************** Internal library routine declarations *****************/
/*************************************************************************/

/******** graphics.c ********/

/**
 * linux_open_display:  Open a connection to the user's I/O interface
 * (e.g. X11 server).
 *
 * [Return value]
 *     True on success, false on error.
 */
extern int linux_open_display(void);

/**
 * linux_close_display:  Close the connection to the user's I/O interface.
 */
extern void linux_close_display(void);

/**
 * linux_close_window:  Close the currently open window, if any.  Intended
 * primarily for use by test routines.
 */
extern void linux_close_window(void);

/**
 * linux_reset_video_mode:  If the video mode of the display device has
 * been changed, reset it to the original mode.
 */
extern void linux_reset_video_mode(void);

/**
 * linux_x11_display:  Return the X11 display pointer.  This will not
 * change during the life of the program.
 */
extern PURE_FUNCTION Display *linux_x11_display(void);

/**
 * linux_x11_window:  Return the X11 window ID for the current window, or
 * None (0) if no window is open.
 */
extern PURE_FUNCTION Window linux_x11_window(void);

/**
 * linux_x11_window_width, linux_x11_window_height:  Return the width or
 * height of the current window, or 0 if no window is open.
 */
extern PURE_FUNCTION int linux_x11_window_width(void);
extern PURE_FUNCTION int linux_x11_window_height(void);

/**
 * linux_x11_screen:  Return the X11 screen ID for the current window.
 * If no window is open, the return value is undefined.
 */
extern PURE_FUNCTION int linux_x11_screen(void);

/**
 * linux_x11_ic:  Return the X11 input context for the current window.
 * If no window is open, the return value is undefined.
 */
extern PURE_FUNCTION XIC linux_x11_ic(void);

/**
 * linux_window_manager:  Return the window manager detected for the
 * current window.
 */
extern PURE_FUNCTION WindowManagerID linux_window_manager(void);

/**
 * linux_get_window_event:  Retrieve the next event for the current window
 * if any events are pending.  Focus events and other internal X11 events
 * are processed directly by this function and not returned.
 *
 * Note that returned events are not filtered with XFilterEvent(); the
 * caller should perform this filtering.
 *
 * [Parameters]
 *     event_ret: Pointer to XEvent structure to receive the event data.
 * [Return value]
 *     True if an event was returned, false if not.
 */
extern int linux_get_window_event(XEvent *event_ret);

/**
 * linux_set_window_grab:  Set whether input should be grabbed for the
 * current window.  Note that this may be overridden depending on the
 * window's fullscreen state.
 *
 * [Parameters]
 *     grab: True to grab input, false to ungrab.
 */
extern void linux_set_window_grab(int grab);

/**
 * linux_get_window_grab:  Return whether input is grabbed for the current
 * window.
 *
 * [Return value]
 *     True if input is grabbed, false if not.
 */
extern int linux_get_window_grab(void);

/**
 * linux_x11_get_error:  Clear and return the error code, if any, recorded
 * by the X11 error handler.  If multiple X11 errors have occurred since
 * the last call to this function (or the start of the program, if this
 * function has not yet been called), all but the first are discarded.
 *
 * [Return value]
 *     X11 error code, or 0 if no errors have been recorded since the
 *     last call to this function.
 */
extern int linux_x11_get_error(void);

/**
 * linux_x11_touchscreen_present:  Return whether a touchscreen device is
 * present.  If and only if this function returns true, mouse events will
 * be received in XInput2 events rather than core X11 events.
 */
extern int linux_x11_touchscreen_present(void);


/******** input.c ********/

/**
 * linux_clear_window_input_state:  Generate release-type events for all
 * inputs which are received through the X11 window and which are currently
 * in a non-released or non-neutral state.  Used to avoid input state
 * desync caused by dropped X11 events during window reconfiguration.
 */
extern void linux_clear_window_input_state(void);

/**
 * linux_override_mouse_position:  Override the mouse position reported by
 * SIL to the given coordinates until a MotionNotify event with those
 * coordinates is received.
 *
 * [Parameters]
 *     x, y: Window-relative mouse coordinates.
 */
extern void linux_override_mouse_position(int x, int y);

/**
 * linux_set_quit_requested:  Set the quit-requested flag.
 */
extern void linux_set_quit_requested(void);


/******** main.c ********/

/**
 * linux_executable_dir:  Return the pathname of the directory containing
 * the executable file used to start the program, or "." if the directory
 * is unknown.
 */
extern const char *linux_executable_dir(void);

/*************************************************************************/
/************************ Test control variables *************************/
/*************************************************************************/

#ifdef SIL_INCLUDE_TESTS

/**
 * TEST_linux_graphics_duplicate_mode:  If set to true, the first mode in
 * the display mode list will be duplicated and this variable will be
 * cleared when sys_graphics_init() is called.  Used to test removal of
 * duplicate modes in core graphics code.
 */
extern uint8_t TEST_linux_graphics_duplicate_mode;

#endif

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_LINUX_INTERNAL_H
