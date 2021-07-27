/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/linux/wrap-x11.h: Control interface for the X11 wrappers.
 */

#ifndef SIL_SRC_TEST_SYSDEP_LINUX_WRAP_X11_H
#define SIL_SRC_TEST_SYSDEP_LINUX_WRAP_X11_H

#include <X11/Xlib.h>

/*************************************************************************/
/*************************************************************************/

/* Flags for causing wrapped calls to fail. */
extern uint8_t disable_XCreateBitmapFromData;
extern uint8_t disable_XCreateColormap;
extern uint8_t disable_XCreateGC;
extern uint8_t disable_XCreateIC;
extern uint8_t disable_XCreatePixmap;
extern uint8_t disable_XCreatePixmapCursor;
extern uint8_t disable_XCreateWindow;
extern uint8_t disable_XGetImage;
extern uint8_t disable_XLoadQueryFont;
extern uint8_t disable_XQueryPointer;
extern uint8_t disable_XF86VidModeQueryExtension;
extern uint8_t disable_XF86VidModeGetAllModeLines;
extern uint8_t disable_XF86VidModeGetModeLine;
extern uint8_t disable_XIQueryVersion;
extern uint8_t disable_XRRQueryExtension;
extern uint8_t disable_XRRQueryVersion;
extern uint8_t disable_XRRGetCrtcInfo;
extern uint8_t disable_XRRGetOutputInfo;
extern uint8_t disable_XRRGetPanning;
extern uint8_t disable_XRRGetScreenResources;
extern uint8_t disable_XineramaQueryExtension;
extern uint8_t disable_XineramaIsActive;
extern uint8_t disable_glXQueryExtension;
extern uint8_t disable_glXCreateWindow;
extern uint8_t disable_glXCreateNewContext;
extern uint8_t disable_glXMakeContextCurrent;

/* Number of calls after which to disable specific wrapped calls. */
extern uint8_t disable_XCreateGC_after;

/* Flags for generating X11 errors from wrapped calls. */
extern uint8_t error_XCreateIC;
extern uint8_t error_XCreateWindow;
extern uint8_t error_XMoveResizeWindow;

/* Counters indicating the number of times certain functions were called. */
extern int called_XCreateWindow;
extern int called_XIconifyWindow;
extern int called_XMoveResizeWindow;
extern int called_XMoveWindow;
extern int called_XResetScreenSaver;
extern int called_XResizeWindow;
extern int called_XF86VidModeGetAllModeLines;
extern int called_XF86VidModeGetModeLine;
extern int called_XRRGetCrtcInfo;
extern int called_XRRGetOutputInfo;
extern int called_XRRGetPanning;
extern int called_XRRGetScreenResources;
extern int called_XRRSetCrtcConfig;
extern int called_XineramaIsActive;
extern int called_XineramaQueryScreens;

/* A copy of the last event sent via XSendEvent() and associated parameters. */
extern XEvent last_event;
extern Display *last_event_display;
extern Window last_event_window;
extern Bool last_event_propagate;
extern long last_event_mask;

/* Override return string for Xutf8LookupString() (NULL = no override). */
extern const char *Xutf8LookupString_override;

/* Versions to report from XIQueryVersion() and XRRQueryVersion(),
 * overriding what the system returns.  Only used if at least one of the
 * relevant {major,minor} pair is nonzero. */
extern uint8_t xinput_version_major;
extern uint8_t xinput_version_minor;
extern uint8_t xrandr_version_major;
extern uint8_t xrandr_version_minor;

/* Client version reported in the most recent call to XIQueryVersion(). */
extern int xinput_client_major;
extern int xinput_client_minor;

/* Flag indicating whether to indicate the presence of a touchscreen in
 * XIQueryDevice(): 1 to indicate that a touchscreen is present, 0 to
 * indicate that no touchscreen is present, or -1 to leave the device list
 * returned by the system unmodified. */
extern int8_t xinput_simulate_touchscreen;

/*-----------------------------------------------------------------------*/

/**
 * clear_x11_wrapper_variables:  Reset all X11 wrapper variables to their
 * initial state (no modifications to behavior and all counters set to zero).
 */
extern void clear_x11_wrapper_variables(void);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_TEST_SYSDEP_LINUX_WRAP_X11_H
