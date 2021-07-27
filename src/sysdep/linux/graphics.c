/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/linux/graphics.c: Graphics and rendering functionality for Linux.
 */

/*
 * Much benefit was derived from Christophe Tronche's HTMLized X11
 * documentation: http://tronche.com/gui/x/
 */

/*
 * A note on window managers and SIL
 * ---------------------------------
 *
 * The behavior of X11 window managers varies drastically, particularly
 * with respect to windows intended to be shown in fullscreen mode.  The
 * table below summarizes how various window managers behave with respect
 * to windows created by SIL.
 *
 * As shown in the table, SIL alters some aspects of its own behavior
 * based on the window manager detected to be running.  Since window
 * managers may themselves change behavior over time, SIL allows users to
 * override its choices using several environment variables (see
 * README-linux.txt in the top directory of the SIL distribution).
 *
 * The set of window managers tested was taken from the Gentoo Linux
 * distribution, which includes a fairly wide variety of window manager
 * packages, though many of the programs are fairly specialized and only
 * likely to be used by a small number of users.  A more comprehensive
 * list, including window managers which are incomplete or do not run on
 * Linux systems, can be found at <https://www.gilesorr.com/wm/table.html>.
 *
 * Legend:
 *    Package = Gentoo Linux package name
 *    _NET_WM_NAME = window manager name as exposed by _NET_WM_NAME in the
 *                      _NET_SUPPORTING_WM_CHECK window, or "---" if none
 *    XMW = enter fullscreen by XMoveWindow() to the screen origin
 *    NET = enter fullscreen using _NET_WM_STATE_FULLSCREEN (EWMH hint)
 *     *  = method is used by SIL for fullscreen windows
 *     =  = method is used by SIL for fullscreen under multi-head Xinerama
 *     !  = (NET only) method is supported by window manager but broken
 *    Early FS? = whether window manager properly handles windows created in
 *                   fullscreen mode (only applies if WM_NAME is available)
 *
 *                        |                 | FS method |Early|
 * Package                | _NET_WM_NAME    | XMW | NET | FS? | Notes
 * -----------------------+-----------------+-----+-----+-----+------------
 * gnome-base/gnome-shell | GNOME Shell     |     |  *  |  no |
 * kde-base/kwin          | KWin            |     |  *  |  no |
 * x11-libs/motif         | (*)             |  *  |     | yes | 2
 * x11-wm/aewm++          | ---             |  *  |     | --- | 1, 2
 * x11-wm/amiwm           | ---             |  *  |     | --- |
 * x11-wm/awesome         | awesome         |  *  |     |  no | 3, 5
 * x11-wm/blackbox        | Blackbox        |  *  |  !  | yes | 2, 6
 * x11-wm/bspwm           | bspwm           |  *  |     | yes | 3, 4
 * x11-wm/ctwm            | ---             |  *  |     | --- | 8
 * x11-wm/cwm             | CWM             |  *  |     | yes | 2
 * x11-wm/dwm             | ---             |  *  |     | --- | 3, 7
 * x11-wm/echinus         | echinus         |  *  |     | yes | 3, 9
 * x11-wm/enlightenment   | e16, e17, ...   |     |  *  | yes |
 * x11-wm/evilwm          | ---             |  *  |     | --- | 2
 * x11-wm/fluxbox         | Fluxbox         |     |  *  | yes |
 * x11-wm/fvwm            | FVWM            |     |  *  |  no | 17
 * x11-wm/goomwwm         | GoomwW (**)     |  *  |  !  | yes | 3, 10
 * x11-wm/herbstluftwm    | herbstluftwm    |  *  |     | yes | 4, 11
 * x11-wm/i3              | i3              |  *  |     |  no |
 * x11-wm/icewm           | IceWM <version> |  *  |     | yes |
 * x11-wm/jwm             | JWM             |  *  |     | yes | 12, 13
 * x11-wm/larswm          | ---             |  *  |     | --- | 9
 * x11-wm/lwm             | lwm             |  *  |     |  no | 2, 7
 * x11-wm/marco           | Marco           |     |  *  |  no |
 * x11-wm/matwm2          | matwm2          |  *  |  !  | yes | 6, 10
 * x11-wm/metacity        | Metacity        |     |  *  |  no |
 * x11-wm/muffin          | Mutter (Muffin) |     |  *  |  no |
 * x11-wm/musca           | musca           |  *  |     | yes | 5, 7, 11
 * x11-wm/mutter          | Mutter          |     |  *  | yes |
 * x11-wm/notion          | notion          |  *  |     | yes | 14
 * x11-wm/openbox         | Openbox         |  =  |  *  | yes |
 * x11-wm/oroborus        | oroborus        |  *  |     | yes | 2, 15
 * x11-wm/pekwm           | pekwm           |     |  *  | yes | 2
 * x11-wm/plwm            | ---             |  *  |     | --- |
 * x11-wm/qtile           | qtile           |  *  |     |  no | 11
 * x11-wm/ratpoison       | ---             |  *  |     | --- | 3, 7
 * x11-wm/sawfish         | Sawfish         |  *  |     | yes | 3
 * x11-wm/sithwm          | ---             |  *  |     | --- | 9
 * x11-wm/spectrwm        | LG3D            |     |  *  |  no | 3, 5, 9, 11
 * x11-wm/subtle          | subtle          |  *  |     |  no | 3, 7
 * x11-wm/twm             | ---             |  *  |     | --- | 8
 * x11-wm/windowlab       | ---             |  *  |     | --- | 16
 * x11-wm/windowmaker     | (*)             |     |  *  |  no |
 * x11-wm/wm2             | ---             |  *  |     | --- | (***)
 * x11-wm/wmii            | wmii            |     |  *  |  no |
 * x11-wm/xmonad          | ---             |  *  |     | --- | 2, 11
 *
 * The following window managers were not tested:
 *    - AfterStep (goes into an infinite loop on startup)
 *    - Compiz (fails to build)
 *
 * (*) x11-libs/motif (MWM) and x11-wm/windowmaker (Window Maker) do not
 *     set _NET_WM_NAME, but they do set custom properties which can be
 *     used to detect their presence.
 *
 * (**) x11-wm/goomwwm's listed _NET_WM_NAME of "GoomwW" is not a typo;
 *      the code incorrectly passes a string length of 6 instead of 7 when
 *      setting the value.
 *
 * (***) Despite dating from March 1997, wm2 handles both fullscreen and
 *       multi-head perfectly.  There's something to be said for simplicity!
 *
 * Notes:
 *  (1) Fullscreen windows lose input focus if the window manager is
 *         configured in click-to-focus mode (the default).
 *  (2) Fullscreen windows may be placed on the wrong monitor or across
 *         multiple monitors when using a multi-head, single-X11-screen
 *         configuration.
 *  (3) Fullscreen windows may be placed on the wrong monitor if a
 *         non-default monitor is requested when using a multi-head,
 *         single-X11-screen configuration.
 *  (4) XSetTransientForHint() is used to disable borders in fullscreen
 *         mode even if the _MOTIF_WM_HINTS atom is present because the
 *         window manager does not parse _MOTIF_WM_HINTS.  (The atom might
 *         be present due to a badly-behaved X11 program blindly creating
 *         the atom instead of checking for its existence.)
 *  (5) The window manager does not parse _MOTIF_WM_HINTS, but it also does
 *         not behave any differently when the transient-for hint is set,
 *         so the XSetTransientForHint() override is not used.
 *  (6) _NET_WM_STATE_FULLSCREEN is not used because the WM fails to remove
 *         window borders in fullscreen mode.
 *  (7) Fullscreen windows still have borders.
 *  (8) Fullscreen windows must be manually placed.
 *  (9) Fullscreen windows lose input focus.
 * (10) _NET_WM_STATE_FULLSCREEN is not used because fullscreen windows at
 *         non-default screen resolutions are placed offscreen.
 * (11) The window manager must be configured to use floating mode for the
 *         window.
 * (12) The mouse pointer is always shown even if the program disables it.
 * (13) The WM-provided window border is positioned incorrectly after
 *         leaving fullscreen mode.
 * (14) Windows disappear when leaving fullscreen mode.
 * (15) Window borders are not restored when leaving fullscreen mode.
 * (16) The program freezes when entering fullscreen mode (this appears to
 *         be a bug in the window manager).
 * (17) FVWM 2.6.6 through (at least) 2.6.9 cause the program to hang when
 *         creating a fullscreen window due to a bug in the window manager.
 *         See: https://github.com/fvwmorg/fvwm/issues/82
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/graphics.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/linux/internal.h"
#include "src/sysdep/opengl/opengl.h"
#include "src/sysdep/posix/path_max.h"
#include "src/time.h"

#include <dlfcn.h>
#include <locale.h>
#include <stdio.h>
#include <unistd.h>
#include <GL/glx.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/Xinerama.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/xf86vmode.h>

/* Constant missing from dlfcn.h: (well, technically it's defined, but it's
 * protected by _GNU_SOURCE and we don't want to enable that globally) */
#define RTLD_DEFAULT  NULL

/* Flags missing from Xrandr.h and xf86vmode.h: */
#define V_PHSYNC     0x0001
#define V_NHSYNC     0x0002
#define V_PVSYNC     0x0004
#define V_NVSYNC     0x0008
#define V_INTERLACE  0x0010
#define V_DBLSCAN    0x0020
#define V_CSYNC      0x0040
#define V_PCSYNC     0x0080
#define V_NCSYNC     0x0100
#define V_HSKEW      0x0200
#define V_BCAST      0x0400
#define V_PIXMUX     0x1000
#define V_DBLCLK     0x2000
#define V_CLKDIV2    0x4000

/*************************************************************************/
/***************************** Exported data *****************************/
/*************************************************************************/

#ifdef SIL_INCLUDE_TESTS

uint8_t TEST_linux_graphics_duplicate_mode;

#endif

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/*---------------------------- General data -----------------------------*/

/* Graphics capability structure returned to high-level code.  The display
 * device count and display mode list are filled in at initialization time. */
static SysGraphicsInfo graphics_info = {
    .has_windowed_mode = 1,
    .num_devices = 0,
    .num_modes = 0,
    .modes = NULL,
};

/* Have we been initialized? */
static uint8_t initted;

/* Have we looked up OpenGL and GLX symbols? */
static uint8_t did_opengl_lookup;
static uint8_t did_glx_lookup;

/* Should we enable OpenGL debugging? */
static uint8_t use_opengl_debug;

/* Should we show the mouse pointer? */
static uint8_t show_mouse_pointer;

/* Should we minimize a fullscreen window on focus loss? (1 = yes, 0 = no,
 * -1 = unspecified by client code) */
static int8_t minimize_fullscreen;

/* Timeout (relative to time_now()) after entering fullscreen until which
 * we suppress automatic minimization on focus loss.  This is needed to
 * avoid minimizing in response to transient focus loss during fullscreen
 * transition in focus-follows-mouse environments. */
static double minimize_fs_timeout;

/* Requested OpenGL version (0 if not set). */
static int desired_opengl_major, desired_opengl_minor;

/*------------------------ Current window state -------------------------*/

/* Current window size. */
static int window_width, window_height;

/* Most recent window position when window was changed to fullscreen mode. */
static int window_last_x, window_last_y;

/* Does the current window have the input focus? */
static uint8_t window_focused;

/* Is the current window in fullscreen mode? */
static uint8_t window_fullscreen;

/* Is input grabbed for the window? */
static uint8_t window_grabbed;

/* Is the window currently iconified? */
static uint8_t window_iconified;

/* Is the window currently mapped? */
static uint8_t window_mapped;

/*--------------------------- Window settings ---------------------------*/

/* Should the window (in windowed mode) be centered when opened? */
static uint8_t center_window;

/* Is fullscreen mode selected (for the next mode change)? */
static uint8_t fullscreen;

/* Display device (screen) to use for the next mode change. */
static int screen_to_use;

/* Should the window be resizable? */
static uint8_t window_resizable;

/* Is input grabbing requested at window creation time? */
static uint8_t grab_requested;

/* Is multisampling enabled, and with how many samples? */
static uint8_t multisample;
static int multisample_samples;

/* Requested refresh rate. */
static float refresh_rate;

/* Is vertical sync enabled? */
static uint8_t vsync;

/* Requested depth and stencil sizes. */
static int depth_bits;
static int stencil_bits;

/* Title to use with new windows. */
static char *window_title;

/* Icon data (in _NET_WM_ICON format) to use with new windows.  Note that
 * as with all X11 properties, the 32-bit format stores each 32-bit data
 * element in a native "long" value, so on systems where sizeof(long) > 4,
 * there is padding between each set of four data bytes. */
static long *window_icon;
/* Length (in longs) of the icon data. */
static int window_icon_len;

/* XSizeHints to use with newly created windows. */
static XSizeHints window_size_hints;

/*--------------------------- Video mode data ---------------------------*/

/* Interface to use for setting screen modes. */
static enum {
    NONE,
    VIDMODE,
    XRANDR,
} video_mode_interface;

/* Is Xinerama active on the display? */
static uint8_t use_xinerama;

/* Data for each known video mode. */
typedef struct VideoModeInfo VideoModeInfo;
struct VideoModeInfo {
    /* Logical screen to which this mode applies. */
    int screen;
    /* Corresponding X11 screen number (always 0 with Xinerama). */
    int x11_screen;
    /* Display resolution. */
    int width, height;
    /* Display offset. */
    int x, y;
    /* Refresh rate (as a fraction), or 0/1 if unknown. */
    int refresh_n, refresh_d;
    /* Interface-specific data. */
    union {
        XF86VidModeModeInfo vidmode_info;
        struct {
            /* XRandR output and mode IDs. */
            RROutput output;
            RRMode mode;
            /* Corresponding Xinerama screen, or -1 if unknown.  (This is
             * a screen-specific rather than mode-specific field, but it
             * is included as part of the mode data for convenience.) */
            int xinerama_screen;
        } xrandr_info;
    };
};
static VideoModeInfo *video_modes;
static int video_modes_size;
static int num_video_modes;

/* XRandR only: Original screen dimensions (in pixels) before any video
 * mode changes. */
static int xrandr_original_width, xrandr_original_height;
/* XRandR 1.3+ only: Panning areas for each output, saved when entering
 * fullscreen and restored when leaving fullscreen.  The pointer is NULL
 * if panning areas have not been saved; otherwise it points to an array
 * of num_xrandr_pan_areas elements. */
static struct {
    uint8_t is_changed;  // Is this output in a non-default mode?
    RROutput output;
    XRRPanning *panning;
} *xrandr_pan_areas;
static int num_xrandr_pan_areas;

/* Original video mode (index into video_modes[]) on each screen. */
static int *original_video_mode;

/* Current video mode.  This is statically initialized to avoid confusing
 * linux_reset_video_mode() if we error out during setup. */
static int current_video_mode = -1;

/* Video mode in use when window was iconified.  Used to restore the
 * desired mode when the window is brought back from icon state. */
static int saved_video_mode = -1;

/*------------- X11/GLX function pointers and atom values ---------------*/

/* Pointers to X11 extension functions that may or may not be available.
 * We look these up in linux_open_display(). */
static __typeof__(XIFreeDeviceInfo)           *p_XIFreeDeviceInfo;
static __typeof__(XIQueryDevice)              *p_XIQueryDevice;
static __typeof__(XIQueryVersion)             *p_XIQueryVersion;
static __typeof__(XISelectEvents)             *p_XISelectEvents;
static __typeof__(XineramaIsActive)           *p_XineramaIsActive;
static __typeof__(XineramaQueryExtension)     *p_XineramaQueryExtension;
static __typeof__(XineramaQueryScreens)       *p_XineramaQueryScreens;
static __typeof__(XRRFreeCrtcInfo)            *p_XRRFreeCrtcInfo;
static __typeof__(XRRFreeOutputInfo)          *p_XRRFreeOutputInfo;
static __typeof__(XRRFreePanning)             *p_XRRFreePanning;
static __typeof__(XRRFreeScreenResources)     *p_XRRFreeScreenResources;
static __typeof__(XRRGetCrtcInfo)             *p_XRRGetCrtcInfo;
static __typeof__(XRRGetOutputInfo)           *p_XRRGetOutputInfo;
static __typeof__(XRRGetPanning)              *p_XRRGetPanning;
static __typeof__(XRRGetScreenResources)      *p_XRRGetScreenResources;
static __typeof__(XRRGetScreenSizeRange)      *p_XRRGetScreenSizeRange;
static __typeof__(XRRQueryExtension)          *p_XRRQueryExtension;
static __typeof__(XRRQueryVersion)            *p_XRRQueryVersion;
static __typeof__(XRRSetCrtcConfig)           *p_XRRSetCrtcConfig;
static __typeof__(XRRSetPanning)              *p_XRRSetPanning;
static __typeof__(XRRSetScreenSize)           *p_XRRSetScreenSize;
static __typeof__(XF86VidModeGetAllModeLines) *p_XF86VidModeGetAllModeLines;
static __typeof__(XF86VidModeGetModeLine)     *p_XF86VidModeGetModeLine;
static __typeof__(XF86VidModeQueryExtension)  *p_XF86VidModeQueryExtension;
static __typeof__(XF86VidModeSwitchToMode)    *p_XF86VidModeSwitchToMode;
#define XIFreeDeviceInfo           p_XIFreeDeviceInfo
#define XIQueryDevice              p_XIQueryDevice
#define XIQueryVersion             p_XIQueryVersion
#define XISelectEvents             p_XISelectEvents
#define XineramaIsActive           p_XineramaIsActive
#define XineramaQueryExtension     p_XineramaQueryExtension
#define XineramaQueryScreens       p_XineramaQueryScreens
#define XRRFreeCrtcInfo            p_XRRFreeCrtcInfo
#define XRRFreeOutputInfo          p_XRRFreeOutputInfo
#define XRRFreePanning             p_XRRFreePanning
#define XRRFreeScreenResources     p_XRRFreeScreenResources
#define XRRGetCrtcInfo             p_XRRGetCrtcInfo
#define XRRGetOutputInfo           p_XRRGetOutputInfo
#define XRRGetPanning              p_XRRGetPanning
#define XRRGetScreenResources      p_XRRGetScreenResources
#define XRRGetScreenSizeRange      p_XRRGetScreenSizeRange
#define XRRQueryExtension          p_XRRQueryExtension
#define XRRQueryVersion            p_XRRQueryVersion
#define XRRSetCrtcConfig           p_XRRSetCrtcConfig
#define XRRSetPanning              p_XRRSetPanning
#define XRRSetScreenSize           p_XRRSetScreenSize
#define XF86VidModeGetAllModeLines p_XF86VidModeGetAllModeLines
#define XF86VidModeGetModeLine     p_XF86VidModeGetModeLine
#define XF86VidModeQueryExtension  p_XF86VidModeQueryExtension
#define XF86VidModeSwitchToMode    p_XF86VidModeSwitchToMode

/* Pointer to glXGetProcAddress function.  May be NULL if the GL library
 * does not implement the function. */
static PFNGLXGETPROCADDRESSPROC p_glXGetProcAddress;
/* Pointers to optional GLX functions. */
static PFNGLXCREATECONTEXTATTRIBSARBPROC p_glXCreateContextAttribsARB;
static PFNGLXSWAPINTERVALEXTPROC p_glXSwapIntervalEXT;
static PFNGLXSWAPINTERVALMESAPROC p_glXSwapIntervalMESA;

/* X11 window manager atoms.  For details, see:
 * http://www.x.org/docs/ICCCM/icccm.pdf
 * http://standards.freedesktop.org/wm-spec/latest/ */
static Atom wm_delete_window;
static Atom wm_protocols;
static Atom wm_state;
static Atom motif_wm_hints;
static Atom net_supporting_wm_check;
static Atom net_wm_action_fullscreen;
static Atom net_wm_allowed_actions;
static Atom net_wm_bypass_compositor;
static Atom net_wm_fullscreen_monitors;
static Atom net_wm_icon;
static Atom net_wm_name;
static Atom net_wm_pid;
static Atom net_wm_ping;
static Atom net_wm_state;
static Atom net_wm_state_fullscreen;
static Atom net_wm_window_type;
static Atom net_wm_window_type_normal;
static Atom utf8_string;
/* Constants for _NET_WM_STATE messages.  These are spelled like atoms, but
 * they're actually simple constants. */
static const long net_wm_state_add = 1;     // _NET_WM_STATE_ADD
static const long net_wm_state_remove = 0;  // _NET_WM_STATE_REMOVE
/* Name-to-variable mapping used for initialization. */
static const struct {const char *name; Atom *atom_ptr;} atom_lookup_list[] = {
    {"WM_DELETE_WINDOW",            &wm_delete_window},
    {"WM_PROTOCOLS",                &wm_protocols},
    {"WM_STATE",                    &wm_state},
    {"_MOTIF_WM_HINTS",             &motif_wm_hints},
    {"_NET_SUPPORTING_WM_CHECK",    &net_supporting_wm_check},
    {"_NET_WM_ACTION_FULLSCREEN",   &net_wm_action_fullscreen},
    {"_NET_WM_ALLOWED_ACTIONS",     &net_wm_allowed_actions},
    {"_NET_WM_BYPASS_COMPOSITOR",   &net_wm_bypass_compositor},
    {"_NET_WM_FULLSCREEN_MONITORS", &net_wm_fullscreen_monitors},
    {"_NET_WM_ICON",                &net_wm_icon},
    {"_NET_WM_NAME",                &net_wm_name},
    {"_NET_WM_PID",                 &net_wm_pid},
    {"_NET_WM_PING",                &net_wm_ping},
    {"_NET_WM_STATE",               &net_wm_state},
    {"_NET_WM_STATE_FULLSCREEN",    &net_wm_state_fullscreen},
    {"_NET_WM_WINDOW_TYPE",         &net_wm_window_type},
    {"_NET_WM_WINDOW_TYPE_NORMAL",  &net_wm_window_type_normal},
    {"UTF8_STRING",                 &utf8_string},
};

/*-------------- Window manager detection and related data --------------*/

/* Currently detected window manager. */
static WindowManagerID window_manager = WM_UNNAMED;

/* Method for entering fullscreen mode. */
static enum {
    /* Remove the window decorations and move the window to the screen
     * origin with XMoveWindow().  This is the simplest method, but some
     * tiling or other automatic-layout window managers can relocate
     * windows independently of the program, which in the case of a
     * lower-than-default resolution video mode can cause the window to
     * end up outside the visible portion of the screen. */
    XMOVEWINDOW,
    /* Use the _NET_WM_STATE window manager message to ask the window
     * manager to make our window fullscreen.  This is the best method in
     * terms of cooperating with the rest of the system, but it requires
     * specific window manager support which is missing from many older
     * WMs, and even some newer WMs don't implement it correctly. */
    EWMH_FULLSCREEN,
} fullscreen_method;

/* Should we create windows in fullscreen mode? */
static uint8_t allow_early_fullscreen;

/* For EWMH_FULLSCREEN, should we move the window to its target monitor
 * before entering fullscreen mode?  (Only applies to Xinerama configs.) */
static uint8_t ewmh_fullscreen_move_before;

/* For EWMH_FULLSCREEN, should we resize the window to its proper size
 * after entering fullscreen mode? */
static uint8_t ewmh_fullscreen_resize_after;

/* Should we always use XSetTransientForHint() to disable window borders? */
static uint8_t force_XSetTransientForHint;

/* List of window managers which behave correctly when setting a window as
 * fullscreen before mapping it. */
static const uint8_t early_fullscreen_whitelist[WM__NUM] = {
    [WM_BLACKBOX]      = 1,
    [WM_BSPWM]         = 1,
    [WM_CWM]           = 1,
    [WM_ECHINUS]       = 1,
    [WM_ENLIGHTENMENT] = 1,
    [WM_FLUXBOX]       = 1,
    [WM_GOOMWWM]       = 1,
    [WM_HERBSTLUFTWM]  = 1,
    [WM_ICEWM]         = 1,
    [WM_JWM]           = 1,
    [WM_MATWM2]        = 1,
    [WM_MUSCA]         = 1,
    [WM_MUTTER]        = 1,
    [WM_NOTION]        = 1,
    [WM_OPENBOX]       = 1,
    [WM_OROBORUS]      = 1,
    [WM_PEKWM]         = 1,
    [WM_SAWFISH]       = 1,
};

/* List of window managers which are known to implement EWMH-style
 * fullscreen incorrectly. */
static const uint8_t ewmh_fullscreen_blacklist[WM__NUM] = {
    [WM_BLACKBOX] = 1,
    [WM_GOOMWWM]  = 1,
    [WM_MATWM2]   = 1,
};

/* List of window managers which handle EWMH fullscreen properly in
 * multi-head (Xinerama) configurations. */
static const uint8_t ewmh_fullscreen_xinerama_whitelist[WM__NUM] = {
    [WM_ENLIGHTENMENT] = 1,
    [WM_FLUXBOX]       = 1,
    [WM_FVWM]          = 1,
    [WM_GNOME_SHELL]   = 1,
    [WM_KWIN]          = 1,
    [WM_MARCO]         = 1,
    [WM_METACITY]      = 1,
    [WM_MUFFIN]        = 1,
    [WM_MUTTER]        = 1,
    [WM_WINDOWMAKER]   = 1,
    [WM_WMII]          = 1,
    /* These WMs don't support multi-head, but they also break when using
     * the traditional (XMoveWindow) fullscreen method, so we let them use
     * the EWMH method as it causes less user pain. */
    [WM_PEKWM]         = 1,
    [WM_SPECTRWM]      = 1,
};

/* List of window managers which support _NET_WM_FULLSCREEN_MONITORS and
 * are known to implement it correctly (a subset of the Xinerama whitelist). */
static const uint8_t ewmh_fullscreen_monitors_whitelist[WM__NUM] = {
    [WM_GNOME_SHELL]   = 1,
    [WM_MARCO]         = 1,
    [WM_METACITY]      = 1,
    [WM_MUFFIN]        = 1,
    [WM_MUTTER]        = 1,
};

/* List of window managers for which we use EWMH but need to manually
 * resize the window after going fullscreen. */
static const uint8_t ewmh_fullscreen_resize_after_list[WM__NUM] = {
    [WM_FLUXBOX]     = 1,
    [WM_FVWM]        = 1,
    [WM_WINDOWMAKER] = 1,
};

/* List of window managers for which we blacklist the use of _MOTIF_WM_HINTS
 * because the window manager doesn't recognize it. */
static const uint8_t motif_wm_hints_blacklist[WM__NUM] = {
    [WM_BSPWM]        = 1,
    [WM_HERBSTLUFTWM] = 1,
};

/*------------------- Other X11 and GLX runtime data --------------------*/

/* X11 display resources. */
static Display *x11_display;
static Cursor x11_empty_cursor;
static XIM x11_im;
static int x11_num_screens;
static int x11_default_screen;

/* X11 input device information. */
static uint8_t xi2_touchscreen_present;  // Is a touchscreen present?

/* X11 window resources. */
static Window x11_window;  // None (0) if no window is open.
/* The following are only valid if x11_window is not None. */
static int x11_screen;     // X11 screen index (always 0 under Xinerama).
static int real_screen;    // Actual screen, taking Xinerama into account.
static Window x11_root;
static XVisualInfo *x11_visual_info;
static Colormap x11_colormap;
static XIC x11_ic;

/* Extension list, used by the error handler. */
static char **x11_extensions_raw;  // Array returned from XListExtensions().
static const char *x11_extensions[128];  // Indexed by major_code-128.

/* Error code saved by the error handler. */
static int x11_error;

/* OpenGL resources. */
static GLXFBConfig glx_config;
static GLXWindow glx_window;
static GLXContext glx_context;

/*--------------------- Local routine declarations ----------------------*/

/*-------- Low-level X11 helpers --------*/

/**
 * x11_error_handler:  Handler for X11 errors.  We record the error and
 * return control to the caller (instead of terminating the program as the
 * default error handler does).
 *
 * [Parameters]
 *     display: Display on which the error occurred.
 *     error: Error event.
 * [Return value]
 *     0
 */
static int x11_error_handler(Display *display, XErrorEvent *error);

/**
 * get_resource_class:  Return the X11 resource class name to use for this
 * program.  If the class name does not fit in the buffer, it will be
 * silently truncated.
 *
 * [Parameters]
 *     buf: Buffer in which to store the X11 resource class name.
 *     bufsize: Size of buf, in bytes.
 */
static void get_resource_class(char *buf, int bufsize);

/**
 * wait_for_event:  Wait until the given window receives an event of the
 * given type.
 *
 * [Parameters]
 *     window: Window which will receive the event.
 *     event_type: Type of event to wait for.
 *     event_ret: Pointer to event structure to receive the matching event.
 *         The pointed-to variable must be of type XEvent, not of the type
 *         of event to be received.  If NULL, the event is discarded.
 */
static void wait_for_event(Window window, int event_type, XEvent *event_ret);

/**
 * wait_for_event_predicate:  XIfEvent() predicate function used by
 * wait_for_event().
 *
 * [Parameters]
 *     display: X11 display connection.
 *     event: Event to check.
 *     arg: Predicate argument (pointer to internal structure type).
 * [Return value]
 *     True if the event matches the requested window and type, false if not.
 */
static int wait_for_event_predicate(Display *display, XEvent *event,
                                    XPointer arg);

/**
 * get_property:  Return the value of the given ATOM or CARD32 property for
 * the given window.  If the property has more than one value, the first
 * value is returned.
 *
 * [Parameters]
 *     window: Window to retrieve property from.
 *     property: Property to look up.
 * [Return value]
 *     The value of the property, or 0 if the property is not found or is
 *     not an ATOM or CARD32 value.
 */
static Atom get_property(Window window, Atom property);

/*-------- Initialization helpers --------*/

/**
 * scan_for_touchscreen:  Scan all devices reported by the XInput2
 * extension to see if any touchscreens are present.
 *
 * [Return value]
 *     True if any touchscreens are present (and in use), false if not.
 */
static int scan_for_touchscreen(void);

/**
 * detect_window_manager:  Determine the type of window manager (if any)
 * managing the given X11 screen.  Sets the global variable window_manager
 * to the detected type.
 */
static void detect_window_manager(int screen);

/**
 * choose_fullscreen_method:  Choose a method for setting windows to
 * fullscreen mode based on the detected window manager.  Sets the global
 * variable fullscreen_method to the chosen method.
 */
static void choose_fullscreen_method(void);

/*-------- Video mode management --------*/

/**
 * add_video_modes_none, add_video_modes_vidmode, add_video_modes_xrandr:
 * Add video modes for the given screen to the global mode list.  Helper
 * functions for sys_graphics_init().
 *
 * [Parameters]
 *     screen: Screen index (either X11 screen or Xinerama/XRandR output,
 *         depending on configuration).
 *     xrandr_screen0_res: XRandR screen resources for X11 screen 0.
 *     xinerama_screen_info: Xinerama screen information array.
 *     num_xinerama_screens: Length of xinerama_screen_info[].
 * [Return value]
 *     True on success, false if an out-of-memory condition occurs.
 */
static int add_video_modes_none(
    int screen, const XineramaScreenInfo *xinerama_screen_info);
static int add_video_modes_vidmode(
    int screen, const XineramaScreenInfo *xinerama_screen_info);
static int add_video_modes_xrandr(
    int screen, XRRScreenResources *xrandr_screen0_res,
    const XineramaScreenInfo *xinerama_screen_info, int num_xinerama_screens);

/**
 * vidmode_modeline_to_modeinfo:  Copy data from an XF86VidModeModeLine
 * structure into an XF86VidModeModeInfo structure.
 *
 * [Parameters]
 *     modeline: XF86VidModeModeLine structure to copy from.
 *     dotclock: Dot clock corresponding to modeline.
 *     modeinfo_ret: Pointer to XF86VidModeModeInfo structure to receive
 *         the copied data.
 */
static void vidmode_modeline_to_modeinfo(
    const XF86VidModeModeLine *modeline, int dotclock,
    XF86VidModeModeInfo *modeinfo_ret);

/**
 * vidmode_modeinfo_to_mode:  Copy data from an XF86VidModeModeInfo
 * structure into the common VideoModeInfo structure.
 *
 * [Parameters]
 *     modeinfo: XF86VidModeModeInfo structure to copy from.
 *     mode_ret: Pointer to VideoModeInfo structure to receive the copied data.
 */
static void vidmode_modeinfo_to_mode(const XF86VidModeModeInfo *modeinfo,
                                     VideoModeInfo *mode_ret);

/**
 * add_video_mode:  Append a video mode to the video mode list.
 *
 * [Parameters]
 *     info: Mode descriptor.
 * [Return value]
 *     True on success, false on error (out of memory).
 */
static int add_video_mode(const VideoModeInfo *info);

/**
 * set_video_mode:  Switch the display to the given video mode.
 *
 * [Parameters]
 *     mode: Mode to switch to (index into video_modes[]).
 * [Return value]
 *     True on success, false on error.
 */
static int set_video_mode(int mode);

/**
 * xrandr_set_crtc_mode:  Wrapper for XRRSetCrtcConfig() which also
 * ensures that the panning parameters are set correctly for all outputs,
 * as a workaround for at least some drivers which arbitrarily change
 * panning areas on all outputs when one output's mode changes.
 *
 * [Parameters]
 *     res: XRRScreenResources for the relevant screen.
 *     output_info: XRROutputInfo for the relevant output.
 *     crtc: XRRCrtcInfo for the relevant output.
 *     mode: Mode to set (video_modes[] index).
 *     save: True to save current panning regions; false to restore saved
 *         panning regions.
 * [Return value]
 *     Return value of XRRSetCrtcConfig().
 */
static Status xrandr_set_crtc_mode(
    XRRScreenResources *res, XRROutputInfo *output_info, XRRCrtcInfo *crtc,
    int mode, int save);

/*-------- Window management --------*/

/**
 * create_window:  Create the X11 window and associated resources for the
 * requested display mode.  Helper for sys_graphics_set_display_mode().
 *
 * [Parameters]
 *     x, y: Requested window position (INT_MIN = don't care).
 *     width, height: Requested window size.
 *     config: Selected GLX configuration ID.
 * [Return value]
 *     True on success, false on error.
 */
static int create_window(int x, int y, int width, int height,
                         GLXFBConfig config);

/**
 * close_window:  Close the currently open window.
 */
static void close_window(void);

/**
 * resize_window:  Resize the currently open window to the given size.
 *
 * [Parameters]
 *     width, height: Requested window size.
 * [Return value]
 *     True on success, false on error.
 */
static int resize_window(int width, int height);

/**
 * save_window_position:  Save the current position of the window in the
 * window_last_x and window_last_y variables.
 */
static void save_window_position(void);

/**
 * scroll_to_window:  Scroll the screen's viewport to ensure that the
 * window is visible.  Used when setting up a fullscreen window.
 *
 * [Parameters]
 *     pointer_x, pointer_y: Desired final pointer coordinates within the
 *         window.
 */
static void scroll_to_window(int pointer_x, int pointer_y);

/**
 * set_window_border:  Set whether the given X11 window should have a
 * window manager border.
 *
 * [Parameters]
 *     window: Window to operate on.
 *     border: True to include a border around the window, false for no border.
 */
static void set_window_border(Window window, int border);

/**
 * set_window_fullscreen:  Set whether the currently open window should
 * be displayed in fullscreen, and optionally resize the window if
 * switching away from fullscreen.
 *
 * This function does nothing if is_new is false and the window is already
 * in the desired fullscreen mode (even if width and height specify a
 * different size than the window is currently set to).
 *
 * [Parameters]
 *     full: True for fullscreen display, false for normal window display.
 *     is_new: True if this is a new window, false if the window was
 *         already open.
 *     width: New width for window.
 *     height: New height for window.
 * [Return value]
 *     True on success, false on error.
 */
static int set_window_fullscreen(int full, int is_new, int width, int height);

/**
 * should_minimize_fullscreen:  Return whether the current window should
 * be minimized on focus loss if in fullscreen mode.
 */
static int should_minimize_fullscreen(void);

/*-------- OpenGL management --------*/

/**
 * setup_opengl:  Initialize the OpenGL subsystem.  This must be called
 * after a display surface has been created.
 *
 * [Parameters]
 *     width, height: Display size, in pixels.
 */
static int setup_opengl(int width, int height);

/**
 * create_glx_context:  Create a new GLXContext for the given GLX config.
 *
 * [Parameters]
 *     config: GLXFBConfig to use with the context.
 *     direct: True if direct rendering is desired, false if direct
 *         rendering is not needed.
 * [Return value]
 *     Newly created GLXContext, or None (0) on error.
 */
static GLXContext create_glx_context(GLXFBConfig config, Bool direct);

/**
 * create_gl_shader_compilation_context:  Create and make current a new
 * GLXContext for the current thread which can be used to compile shaders.
 *
 * [Return value]
 *     True on success or if the current thread already has a GL context,
 *     false on error.
 */
static int create_gl_shader_compilation_context(void);

/**
 * glx_has_extension:  Return whether the given GLX extension is supported.
 *
 * [Parameters]
 *     screen: X11 screen on which to check.
 *     name: GLX extension name.
 * [Return value]
 *     True if the extension is supported, false if not.
 */
static int glx_has_extension(int screen, const char *name);

/**
 * glx_choose_config:  Return an appropriate GLXFBConfig for the currently
 * set display attributes.
 *
 * [Parameters]
 *     screen: X11 screen index on which to look up configurations.
 *     config_ret: Pointer to variable to receive the chosen configuration.
 * [Return value]
 *     True on success, false on error.
 */
static int glx_choose_config(int screen, GLXFBConfig *config_ret);

/**
 * linux_glXGetProcAddress:  Wrapper for glXGetProcAddress() which falls
 * back to dlsym() if glXProcAddress is not available.
 *
 * [Parameters]
 *     name: Name of function to look up.
 * [Return value]
 *     Function pointer, or NULL if the function is not found.
 */
static void *linux_glXGetProcAddress(const char *name);

/*************************************************************************/
/***************** Interface: Basic graphics operations ******************/
/*************************************************************************/

const SysGraphicsInfo *sys_graphics_init(void)
{
    PRECOND(!initted, goto error_return);
    PRECOND(x11_display != NULL, goto error_return);

    /* Determine which interface to use for setting fullscreen modes. */
    int major, minor;
    if (XRRQueryExtension
            && XRRQueryExtension(x11_display, (int[1]){0}, (int[1]){0})
            && XRRQueryVersion(x11_display, &major, &minor)
            && (major >= 2 || (major == 1 && minor >= 2))) {
        video_mode_interface = XRANDR;
        /* If the client library is version 1.3+ but the server is only
         * version 1.2, don't try to use the panning functions. */
        if (major == 1 && minor == 2) {
            XRRFreePanning = NULL;
            XRRGetPanning = NULL;
            XRRSetPanning = NULL;
        }
    } else if (XF86VidModeQueryExtension
            && XF86VidModeQueryExtension(x11_display,
                                         (int[1]){0}, (int[1]){0})) {
        video_mode_interface = VIDMODE;
    } else {
        DLOG("No video mode interface found!");
        video_mode_interface = NONE;
    }

    /* Allow the user to override our detected video mode interface, but
     * only if the requested interface is actually available. */
    const char *vmi_override = getenv("SIL_X11_VIDEO_MODE_INTERFACE");
    if (vmi_override && *vmi_override) {
        if (strcmp(vmi_override, "NONE") == 0) {
            video_mode_interface = NONE;
        } else if (strcmp(vmi_override, "VIDMODE") == 0) {
            if (XF86VidModeQueryExtension
             && XF86VidModeQueryExtension(x11_display,
                                          (int[1]){0}, (int[1]){0})) {
                video_mode_interface = VIDMODE;
            } else {
                fprintf(stderr, "Warning: Video mode interface VIDMODE"
                        " requested but not available, ignoring override.\n");
            }
        } else if (strcmp(vmi_override, "XRANDR") == 0) {
            /* XRANDR will always be chosen if available. */
            if (video_mode_interface != XRANDR) {
                fprintf(stderr, "Warning: Video mode interface XRANDR"
                        " requested but not available, ignoring override.\n");
            }
        } else {
            fprintf(stderr, "Warning: Ignoring unrecognized value for"
                    " SIL_X11_VIDEO_MODE_INTERFACE: %s\n", vmi_override);
        }
    }

    /* Check whether Xinerama is available.  We don't use Xinerama
     * directly for changing video modes (since it only provides screen
     * position and size information), but we use it both to properly
     * position windows when XRandR is not available and to set the
     * _NET_WM_FULLSCREEN_MONITORS property when entering fullscreen mode
     * on compliant window managers. */
    XineramaScreenInfo *xinerama_screen_info = NULL;
    int num_xinerama_screens = 0;
    if (XineramaQueryExtension
     && XineramaQueryExtension(x11_display, (int[1]){0}, (int[1]){0})
     && XineramaIsActive(x11_display)) {
        use_xinerama = (video_mode_interface != XRANDR);
        xinerama_screen_info =
            XineramaQueryScreens(x11_display, &num_xinerama_screens);
        ASSERT(xinerama_screen_info != NULL, use_xinerama = 0);
        ASSERT(num_xinerama_screens > 0, use_xinerama = 0);
    } else {
        use_xinerama = 0;
    }

    /* Find the number of display devices available. */
    XRRScreenResources *xrandr_screen0_res = NULL;
    if (video_mode_interface == XRANDR) {
        if (ScreenCount(x11_display) > 1) {
            /* Assume one monitor per X11 screen in this case.  We don't
             * support multiple X11 screens with multiple monitors per
             * screen, but such configurations should be rare. */
            x11_num_screens = ScreenCount(x11_display);
            x11_default_screen = DefaultScreen(x11_display);
        } else {
            xrandr_screen0_res =
                XRRGetScreenResources(x11_display, RootWindow(x11_display, 0));
            if (UNLIKELY(!xrandr_screen0_res)) {
                DLOG("XRRGetScreenResources() failed, mode switching"
                     " disabled");
                video_mode_interface = NONE;
                x11_num_screens = 1;
            } else {
                x11_num_screens = 0;
                for (int i = 0; i < xrandr_screen0_res->noutput; i++) {
                    XRROutputInfo *oi = XRRGetOutputInfo(
                        x11_display, xrandr_screen0_res,
                        xrandr_screen0_res->outputs[i]);
                    if (oi) {
                        if (oi->crtc && oi->connection != RR_Disconnected) {
                            x11_num_screens++;
                        }
                        XRRFreeOutputInfo(oi);
                    }
                }
                if (x11_num_screens == 0) {
                    DLOG("Unable to find any connected display devices,"
                         " mode switching disabled");
                    video_mode_interface = NONE;
                    x11_num_screens = 1;
                }
            }
            x11_default_screen = 0;
        }
    } else if (use_xinerama) {
        x11_num_screens = num_xinerama_screens;
        x11_default_screen = 0;
    } else {
        x11_num_screens = ScreenCount(x11_display);
        x11_default_screen = DefaultScreen(x11_display);
    }
    ASSERT(x11_num_screens > 0, goto error_free_x11_resources);

    /* Collect available screen modes. */
    original_video_mode = mem_alloc(
        sizeof(*original_video_mode) * x11_num_screens, 0, 0);
    if (UNLIKELY(!original_video_mode)) {
        goto error_free_x11_resources;
    }
    for (int screen = 0; screen < x11_num_screens; screen++) {
        switch (video_mode_interface) {
          case NONE:
            if (!add_video_modes_none(screen, xinerama_screen_info)) {
                goto error_free_video_modes;
            }
            break;
          case VIDMODE:
            if (!add_video_modes_vidmode(screen, xinerama_screen_info)) {
                goto error_free_video_modes;
            }
            break;
          case XRANDR:
            if (!add_video_modes_xrandr(screen, xrandr_screen0_res,
                                        xinerama_screen_info,
                                        num_xinerama_screens)) {
                goto error_free_video_modes;
            }
            break;
        }
    }
    ASSERT(num_video_modes > 0, goto error_free_video_modes);
    current_video_mode = original_video_mode[x11_default_screen];

    if (xinerama_screen_info) {
        XFree(xinerama_screen_info);
        xinerama_screen_info = NULL;
    }
    if (xrandr_screen0_res) {
        XRRFreeScreenResources(xrandr_screen0_res);
        xrandr_screen0_res = NULL;
    }

    /* Initialize the SysGraphicsInfo structure to return to the caller. */
    GraphicsDisplayModeEntry *modes =
        mem_alloc(sizeof(*graphics_info.modes) * num_video_modes, 0, 0);
    if (UNLIKELY(!modes)) {
        DLOG("Out of memory creating display mode list");
        goto error_free_video_modes;
    }
    graphics_info.num_devices = x11_num_screens;
    graphics_info.num_modes = num_video_modes;
    graphics_info.modes = modes;
    for (int i = 0; i < num_video_modes; i++) {
        modes[i].device = video_modes[i].screen;
        modes[i].device_name = NULL;
        modes[i].width = video_modes[i].width;
        modes[i].height = video_modes[i].height;
        modes[i].refresh =
            (float)video_modes[i].refresh_n / (float)video_modes[i].refresh_d;
    }

    /* Create a blank cursor (mouse pointer) image so we can hide the
     * mouse pointer when requested. */
    x11_empty_cursor = None;  // Fallback in case of error.
    Pixmap pixmap = XCreateBitmapFromData(
        x11_display, DefaultRootWindow(x11_display), (char[]){0}, 1, 1);
    if (pixmap) {
        XColor color = {.red = 0, .green = 0, .blue = 0};
        x11_empty_cursor = XCreatePixmapCursor(x11_display, pixmap, pixmap,
                                               &color, &color, 0, 0);
        XFreePixmap(x11_display, pixmap);
        if (!x11_empty_cursor) {
            DLOG("Failed to create empty cursor,"
                 " graphics_show_mouse_pointer(0) will fail");
        }
    } else {
        DLOG("Failed to create pixmap for empty cursor,"
             " graphics_show_mouse_pointer(0) will fail");
    }

    /* Open a connection to the X11 input manager, if available. */
    setlocale(LC_ALL, "");
    XSetLocaleModifiers("");
    x11_im = XOpenIM(x11_display, NULL, NULL, NULL);

    /* Check whether any touchscreen devices are present.  For XInput2
     * docs, see: http://who-t.blogspot.com/search/label/xi2 */
    major = 2;
    minor = 2;
    x11_error = 0;
    if (XIQueryVersion
     && XQueryExtension(x11_display, "XInputExtension",
                        (int[1]){0}, (int[1]){0}, (int[1]){0})
     && XIQueryVersion(x11_display, &major, &minor) == Success
     && !x11_error
     && (major > 2 || (major == 2 && minor >= 2))) {
        xi2_touchscreen_present = scan_for_touchscreen();
    }

    /* Look up atoms used by window manager properties. */
    for (int i = 0; i < lenof(atom_lookup_list); i++) {
        *(atom_lookup_list[i].atom_ptr) =
            XInternAtom(x11_display, atom_lookup_list[i].name, True);
    }

    /* Initialize other internal data. */
    center_window = 0;
    depth_bits = 16;
    desired_opengl_major = 0;
    desired_opengl_minor = 0;
    did_opengl_lookup = 0;
    did_glx_lookup = 0;
    fullscreen = 0;
    glx_context = None;
    glx_window = None;
    grab_requested = 0;
    minimize_fs_timeout = 0;
    minimize_fullscreen = -1;
    multisample = 0;
    multisample_samples = 1;
    screen_to_use = x11_default_screen;
    show_mouse_pointer = (x11_empty_cursor ? 0 : 1);
    stencil_bits = 0;
    use_opengl_debug = 0;
    vsync = 1;
    window_icon = NULL;
    window_icon_len = 0;
    window_last_x = INT_MIN;
    window_last_y = INT_MIN;
    window_resizable = 0;
    mem_clear(&window_size_hints, sizeof(window_size_hints));
    window_title = NULL;
    x11_window = None;

    initted = 1;
    return &graphics_info;

  error_free_video_modes:
    mem_free(video_modes);
    video_modes = NULL;
    video_modes_size = 0;
    num_video_modes = 0;
    mem_free(original_video_mode);
    original_video_mode = NULL;
  error_free_x11_resources:
    if (xinerama_screen_info) {
        XFree(xinerama_screen_info);
        xinerama_screen_info = NULL;
    }
    if (xrandr_screen0_res) {
        XRRFreeScreenResources(xrandr_screen0_res);
        xrandr_screen0_res = NULL;
    }
  error_return:
    return NULL;
}

/*-----------------------------------------------------------------------*/

void sys_graphics_cleanup(void)
{
    PRECOND(initted, return);

    /* Reset the video mode before closing the window so the monitor is
     * already switching modes when the window disappears. */
    linux_reset_video_mode();

    if (x11_window) {
        close_window();
    }
    mem_free(window_title);
    window_title = NULL;
    mem_free(window_icon);
    window_icon = NULL;

    if (x11_im) {
        XCloseIM(x11_im);
        x11_im = None;
    }

    if (x11_empty_cursor) {
        XFreeCursor(x11_display, x11_empty_cursor);
        x11_empty_cursor = None;
    }

    mem_free((void *)graphics_info.modes);
    graphics_info.num_modes = 0;
    graphics_info.modes = NULL;

    current_video_mode = -1;
    mem_free(video_modes);
    video_modes = NULL;
    video_modes_size = 0;
    num_video_modes = 0;
    mem_free(original_video_mode);
    original_video_mode = NULL;

    /* These should always be freed/cleared after linux_reset_video_mode(). */
    ASSERT(xrandr_pan_areas == NULL, xrandr_pan_areas = NULL);
    ASSERT(xrandr_original_width == 0, xrandr_original_width = 0);
    ASSERT(xrandr_original_height == 0, xrandr_original_height = 0);

    initted = 0;
}

/*-----------------------------------------------------------------------*/

int sys_graphics_device_width(void)
{
    ASSERT(original_video_mode != NULL, return 0);
    const int screen = x11_window ? real_screen : screen_to_use;
    const int mode = original_video_mode[screen];
    return video_modes[mode].width;
}

/*-----------------------------------------------------------------------*/

int sys_graphics_device_height(void)
{
    ASSERT(original_video_mode != NULL, return 0);
    const int screen = x11_window ? real_screen : screen_to_use;
    const int mode = original_video_mode[screen];
    return video_modes[mode].height;
}

/*-----------------------------------------------------------------------*/

int sys_graphics_set_display_attr(const char *name, va_list args)
{
    if (strcmp(name, "center_window") == 0) {
        const int value = va_arg(args, int);
        center_window = (value != 0);
        return 1;
    }

    if (strcmp(name, "depth_bits") == 0) {
        const int value = va_arg(args, int);
        if (value < 0) {
            DLOG("Invalid value for attribute %s: %d", name, value);
            return 0;
        }
        depth_bits = value;
        return 1;
    }

    if (strcmp(name, "device") == 0) {
        const int value = va_arg(args, int);
        if (value < 0 || value >= x11_num_screens) {
            DLOG("Invalid value for attribute %s: %d", name, value);
            return 0;
        }
        screen_to_use = value;
        return 1;
    }

    if (strcmp(name, "fullscreen_minimize_on_focus_loss") == 0) {
        const int value = va_arg(args, int);
        minimize_fullscreen = (value != 0);
        return 1;
    }

    if (strcmp(name, "multisample") == 0) {
        const int value = va_arg(args, int);
        if (value <= 0) {
            DLOG("Invalid value for attribute %s: %d", name, value);
            return 0;
        }
        multisample = (value > 1);
        multisample_samples = value;
        return 1;
    }

    if (strcmp(name, "opengl_debug") == 0) {
        use_opengl_debug = (va_arg(args, int) != 0);
        if (x11_window != None) {
            opengl_enable_debug(use_opengl_debug);
        }
        return 1;
    }

    if (strcmp(name, "opengl_version") == 0) {
        desired_opengl_major = va_arg(args, int);
        desired_opengl_minor = va_arg(args, int);
        return 1;
    }

    if (strcmp(name, "refresh_rate") == 0) {
        const float value = (float)va_arg(args, double);
        if (!(value >= 0)) {
            DLOG("Invalid value for attribute %s: %g", name, value);
            return 0;
        }
        refresh_rate = value;
        return 1;
    }

    if (strcmp(name, "stencil_bits") == 0) {
        const int value = va_arg(args, int);
        if (value < 0) {
            DLOG("Invalid value for attribute %s: %d", name, value);
            return 0;
        }
        stencil_bits = value;
        return 1;
    }

    if (strcmp(name, "vsync") == 0) {
        vsync = (va_arg(args, int) != 0);
        if (glx_window) {
            if (p_glXSwapIntervalEXT) {
                (*p_glXSwapIntervalEXT)(x11_display, glx_window, vsync);
            } else if (p_glXSwapIntervalMESA) {
                (*p_glXSwapIntervalMESA)(vsync);
            }
        }
        return 1;
    }

    if (strcmp(name, "window") == 0) {
        fullscreen = (va_arg(args, int) == 0);
        return 1;
    }

    if (strcmp(name, "window_resizable") == 0) {
        window_resizable = (va_arg(args, int) != 0);
        if (x11_window && !fullscreen) {
            if (window_resizable) {
                XSetWMNormalHints(x11_display, x11_window, &window_size_hints);
            } else {
                XSizeHints size_hints = {
                    .flags = PMinSize | PMaxSize,
                    .min_width = window_width, .max_width = window_width,
                    .min_height = window_height, .max_height = window_height,
                };
                XSetWMNormalHints(x11_display, x11_window, &size_hints);
            }
        }
        return 1;
    }

    return 0;
}

/*-----------------------------------------------------------------------*/

GraphicsError sys_graphics_set_display_mode(int width, int height)
{
    /* Map the requested logical screen to an X11 screen. */
    const int x11_screen_to_use =
        (use_xinerama || ScreenCount(x11_display) == 1) ? 0 : screen_to_use;

    /* Make sure GLX is available before we try using it, and look up
     * functions if necessary. */
    if (!glXQueryExtension(x11_display, (int[1]){0}, (int[1]){0})) {
        DLOG("GLX not available");
        return GRAPHICS_ERROR_BACKEND_NOT_FOUND;
    }
    if (!did_glx_lookup) {
        /* We link directly with libGL, so we assume that at least GLX 1.3
         * (spec published in 1998) is statically available.
         * glXGetProcAddress was not defined as mandatory until GLX 1.4
         * (2005), and it's easy to work around if missing, so we look
         * it up dynamically.  We also look up extension functions which
         * are not part of GLX proper. */
        p_glXGetProcAddress = dlsym(RTLD_DEFAULT, "glXGetProcAddress");
        if (glx_has_extension(x11_screen_to_use, "GLX_ARB_create_context")) {
            p_glXCreateContextAttribsARB =
                dlsym(RTLD_DEFAULT, "glXCreateContextAttribsARB");
        } else {
            p_glXCreateContextAttribsARB = NULL;
        }
        if (glx_has_extension(x11_screen_to_use, "GLX_EXT_swap_control")) {
            p_glXSwapIntervalEXT = dlsym(RTLD_DEFAULT, "glXSwapIntervalEXT");
        } else {
            p_glXSwapIntervalEXT = NULL;
        }
        if (glx_has_extension(x11_screen_to_use, "GLX_MESA_swap_control")) {
            p_glXSwapIntervalMESA = dlsym(RTLD_DEFAULT, "glXSwapIntervalMESA");
        } else {
            p_glXSwapIntervalMESA = NULL;
        }
        did_glx_lookup = 1;
    }

    /* Pick a GL framebuffer configuration. */
    GLXFBConfig config;
    if (!glx_choose_config(x11_screen_to_use, &config)) {
        return GRAPHICS_ERROR_MODE_NOT_SUPPORTED;
    }
    XVisualInfo *visual_info = glXGetVisualFromFBConfig(x11_display, config);
    ASSERT(visual_info, return GRAPHICS_ERROR_UNKNOWN);

    /* Detect the window manager type for this screen and select an
     * appropriate method for entering fullscreen mode.  Note that we need
     * to choose the fullscreen method before changing the video mode,
     * since the method used to change modes depends in part on the
     * fullscreen method -- in particular, using XRandR to set the screen
     * size can break window managers that don't support EWMH-style
     * fullscreen. */
    detect_window_manager(x11_screen_to_use);
    choose_fullscreen_method();

    /* Look up the video mode corresponding to the requested display size
     * and screen. */
    int fullscreen_video_mode = -1;
    if (fullscreen) {
        if (refresh_rate == 0
         && video_modes[current_video_mode].width == width
         && video_modes[current_video_mode].height == height) {
            /* Avoid unnecessary mode changes. */
            fullscreen_video_mode = current_video_mode;
        } else {
            float best_refresh;
            for (int i = 0; i < num_video_modes; i++) {
                if (video_modes[i].width == width
                 && video_modes[i].height == height
                 && video_modes[i].screen == screen_to_use) {
                    const float refresh = (float)video_modes[i].refresh_n
                                        / (float)video_modes[i].refresh_d;
                    int better;
                    if (fullscreen_video_mode < 0) {
                        better = 1;
                    } else if (refresh_rate > 0) {
                        better = (fabsf(refresh - refresh_rate)
                                  < fabsf(best_refresh - refresh_rate));
                    } else {  // refresh_rate == 0
                        better = (refresh > best_refresh);
                    }
                    if (better) {
                        fullscreen_video_mode = i;
                        best_refresh = refresh;
                    }
                }
            }
        }
        if (fullscreen_video_mode < 0) {
            DLOG("No video mode matching %dx%d on screen %d", width, height,
                 screen_to_use);
            return GRAPHICS_ERROR_MODE_NOT_SUPPORTED;
        }
    }
    const int new_video_mode =
        fullscreen ? fullscreen_video_mode : original_video_mode[screen_to_use];
    ASSERT(new_video_mode >= 0);

    /* If we're changing screens, restore the old screen's mode and select
     * the new screen's default mode as current (to ensure the centering
     * calculation below works correctly). */
    if (video_modes[current_video_mode].screen != screen_to_use) {
        ASSERT(set_video_mode(original_video_mode[screen_to_use]));
    }

    /* Switch video modes if requested. */
    const int old_video_mode = current_video_mode;
    if (current_video_mode != new_video_mode) {
        if (UNLIKELY(!set_video_mode(new_video_mode))) {
            return GRAPHICS_ERROR_MODE_NOT_SUPPORTED;
        }
    }

    /* X11 can occasionally drop input events during window reconfiguration
     * even if the window remains focused, so force-clear all input state
     * to avoid things like Alt being reported as still held down after an
     * Alt+Enter fullscreen switch. */
    linux_clear_window_input_state();

    /* Handle cases that don't require closing and reopening the window. */
    if (x11_window
     && config == glx_config
     && video_modes[new_video_mode].x11_screen == x11_screen_to_use) {
        int ok;
        if (fullscreen) {
            ok = set_window_fullscreen(1, 0, width, height);
            minimize_fs_timeout = time_now() + 0.25;
        } else if (window_fullscreen) {
            ok = set_window_fullscreen(0, 0, width, height);
        } else if (width != window_width || height != window_height) {
            ok = resize_window(width, height);
        } else {
            ok = 1;
        }
        if (ok) {
            /* With at least the NVIDIA driver, we need to swap buffers
             * once to get the driver's buffer pointers updated; if we
             * don't, things get rendered to the wrong part of the window
             * for the first frame. */
            glXSwapBuffers(x11_display, glx_window);
            window_width = width;
            window_height = height;
            opengl_set_display_size(width, height);
            return GRAPHICS_ERROR_SUCCESS;
        }
    }

    /* If we already had a window, destroy it and the associated GL context. */
    const int window_was_open = (x11_window != None);
    if (window_was_open) {
        close_window();
    }

    /* Set basic window parameters. */
    real_screen = screen_to_use;
    x11_screen = x11_screen_to_use;
    x11_root = RootWindow(x11_display, x11_screen);
    x11_visual_info = visual_info;
    int window_x, window_y;
    if (fullscreen) {
        window_x = video_modes[new_video_mode].x;
        window_y = video_modes[new_video_mode].y;
    } else if (center_window) {
        window_x = (video_modes[new_video_mode].width - width) / 2;
        window_y = (video_modes[new_video_mode].height - height) / 2;
    } else if (window_last_x != INT_MIN && window_last_y != INT_MIN) {
        window_x = window_last_x;
        window_y = window_last_y;
    } else {  // Leave it to the WM.
        window_x = INT_MIN;
        window_y = INT_MIN;
    }
    window_width = width;
    window_height = height;
    window_focused = 1;
    window_fullscreen = 0;
    window_grabbed = 0;
    window_iconified = 0;
    window_mapped = 0;
    saved_video_mode = -1;

    /* Create the new X11 window and associated resources. */
    if (!create_window(window_x, window_y, width, height, config)) {
        if (current_video_mode != old_video_mode) {
            set_video_mode(old_video_mode);
        }
        return GRAPHICS_ERROR_MODE_NOT_SUPPORTED;
    }

    /* Enable input events for the window. */
    long x11_events = 0;
    if (x11_ic) {
        XGetICValues(x11_ic, XNFilterEvents, &x11_events, NULL);
    }
    x11_events |= (KeyPressMask | KeyReleaseMask | ButtonPressMask
                   | ButtonReleaseMask | EnterWindowMask | PointerMotionMask
                   | FocusChangeMask | PropertyChangeMask | StructureNotifyMask
                   | VisibilityChangeMask);
    if (xi2_touchscreen_present) {
        unsigned char mask[XIMaskLen(XI_LASTEVENT)];
        mem_clear(mask, sizeof(mask));
        XISetMask(mask, XI_Motion);
        XISetMask(mask, XI_ButtonPress);
        XISetMask(mask, XI_ButtonRelease);
        XISetMask(mask, XI_TouchBegin);
        XISetMask(mask, XI_TouchUpdate);
        XISetMask(mask, XI_TouchEnd);
        XIEventMask event_mask = {.deviceid = XIAllMasterDevices,
                                  .mask_len = sizeof(mask), .mask = mask};
        XISelectEvents(x11_display, x11_window, &event_mask, 1);
        x11_events &= ~(ButtonPressMask | ButtonReleaseMask | EnterWindowMask
                        | PointerMotionMask);
    }
    XSelectInput(x11_display, x11_window, x11_events);

    /* Configure the window for fullscreen display if requested, but only
     * if the window manager is known to handle initially-fullscreen
     * windows correctly. */
    if (fullscreen && allow_early_fullscreen) {
        set_window_fullscreen(1, 1, width, height);
    }

    /* Hide the mouse pointer if requested. */
    sys_graphics_show_mouse_pointer(show_mouse_pointer);

    /* Show the new window and wait for it to become visible. */
    XMapRaised(x11_display, x11_window);
    wait_for_event(x11_window, MapNotify, NULL);
    window_mapped = 1;
    wait_for_event(x11_window, VisibilityNotify, NULL);

    /* Handle deferred fullscreen (see above). */
    if (fullscreen && !allow_early_fullscreen) {
        set_window_fullscreen(1, 0, width, height);
    }

    /* Grab input if requested.  (This must be done after the window is
     * mapped.) */
    linux_set_window_grab(grab_requested);

    /* Set up the OpenGL manager now that we have a context to work with. */
    if (!did_opengl_lookup) {
        opengl_lookup_functions(linux_glXGetProcAddress);
        did_opengl_lookup = 1;
    }
    opengl_enable_debug(opengl_debug_is_enabled());
    if (!setup_opengl(width, height)) {
        return GRAPHICS_ERROR_BACKEND_TOO_OLD;
    }

    /* Enable multisampling if requested. */
    if (multisample) {
        glEnable(GL_MULTISAMPLE);
    } else {
        glDisable(GL_MULTISAMPLE);
    }

    return window_was_open ? GRAPHICS_ERROR_STATE_LOST
                           : GRAPHICS_ERROR_SUCCESS;
}

/*-----------------------------------------------------------------------*/

int sys_graphics_display_is_window(void)
{
    return x11_window && !window_fullscreen;
}

/*-----------------------------------------------------------------------*/

void sys_graphics_set_window_title(const char *title)
{
    mem_free(window_title);
    window_title = mem_strdup(title, 0);
    if (UNLIKELY(!window_title)) {
        DLOG("No memory to copy window title");
    }
    if (x11_window) {
        Xutf8SetWMProperties(x11_display, x11_window, window_title,
                             window_title, NULL, 0, NULL, NULL, NULL);
    }
}

/*-----------------------------------------------------------------------*/

void sys_graphics_set_window_icon(SysTexture *texture)
{
    const int width = sys_texture_width(texture);
    const int height = sys_texture_height(texture);

    uint8_t *pixels = sys_texture_lock(texture, SYS_TEXTURE_LOCK_NORMAL,
                                       0, 0, width, height);
    if (UNLIKELY(!pixels)) {
        DLOG("Failed to lock texture for icon");
        return;
    }

    /* This is deliberately "long" instead of "int32_t" even though it's
     * 32-bit data, because that's what X11 apparently requires. */
    long *icon_data = mem_alloc((2 + width*height)*sizeof(long),
                                sizeof(long), 0);
    if (UNLIKELY(!icon_data)) {
        DLOG("No memory for copy of icon data (%d bytes)", width*height*4);
        sys_texture_unlock(texture, 0);
        return;
    }
    icon_data[0] = width;
    icon_data[1] = height;
    for (int i = 0; i < width*height; i++) {
        const int r = pixels[i*4+0];
        const int g = pixels[i*4+1];
        const int b = pixels[i*4+2];
        const int a = pixels[i*4+3];
        icon_data[2+i] = a<<24 | r<<16 | g<<8 | b;
    }
    sys_texture_unlock(texture, 0);

    mem_free(window_icon);
    window_icon = icon_data;
    window_icon_len = 2 + width*height;

    /* We have to have a window open in order to create textures, but
     * check anyway in case we decide to change the interface later. */
    if (x11_window && net_wm_icon) {
        XChangeProperty(x11_display, x11_window, net_wm_icon, XA_CARDINAL, 32,
                        PropModeReplace, (void *)icon_data, window_icon_len);
    }
}

/*-----------------------------------------------------------------------*/

void sys_graphics_set_window_resize_limits(
    int min_width, int min_height, int max_width, int max_height,
    int min_aspect_x, int min_aspect_y, int max_aspect_x, int max_aspect_y)
{
    if (min_width > 0 && min_height > 0) {
        window_size_hints.flags |= PMinSize;
        window_size_hints.min_width = min_width;
        window_size_hints.min_height = min_height;
    } else {
        window_size_hints.flags &= ~PMinSize;
    }

    if (max_width > 0 && max_height > 0) {
        window_size_hints.flags |= PMaxSize;
        window_size_hints.max_width = max_width;
        window_size_hints.max_height = max_height;
    } else {
        window_size_hints.flags &= ~PMaxSize;
    }

    /* XSizeHints requires either both or none of the minimum and maximum
     * aspect ratios to be set, so we have to hack around a bit to allow
     * just one to be set. */
    const int have_min_aspect = (min_aspect_x > 0 && min_aspect_y > 0);

    if (max_aspect_x > 0 && max_aspect_y > 0) {
        window_size_hints.flags |= PAspect;
        if (have_min_aspect) {
            window_size_hints.min_aspect.x = min_aspect_x;
            window_size_hints.min_aspect.y = min_aspect_y;
        } else {
            window_size_hints.min_aspect.x = 1;
            window_size_hints.min_aspect.y = 0x7FFFFFFF;
        }
        window_size_hints.max_aspect.x = max_aspect_x;
        window_size_hints.max_aspect.y = max_aspect_y;
    } else {
        if (have_min_aspect) {
            window_size_hints.flags |= PAspect;
            window_size_hints.min_aspect.x = min_aspect_x;
            window_size_hints.min_aspect.y = min_aspect_y;
            window_size_hints.max_aspect.x = 0x7FFFFFFF;
            window_size_hints.max_aspect.y = 1;
        } else {
            window_size_hints.flags &= ~PAspect;
        }
    }

    if (x11_window && !fullscreen && window_resizable) {
        XSetWMNormalHints(x11_display, x11_window, &window_size_hints);
    }
}

/*-----------------------------------------------------------------------*/

void sys_graphics_show_mouse_pointer(int on)
{
    if (x11_empty_cursor) {
        show_mouse_pointer = (on != 0);
        if (x11_window) {
            XDefineCursor(x11_display, x11_window,
                          on ? None : x11_empty_cursor);
        }
    }
}

/*-----------------------------------------------------------------------*/

int sys_graphics_get_mouse_pointer_state(void)
{
    return show_mouse_pointer;
}

/*-----------------------------------------------------------------------*/

void sys_graphics_get_frame_period(int *numerator_ret, int *denominator_ret)
{
    if (video_modes[current_video_mode].refresh_n > 0) {
        *numerator_ret = video_modes[current_video_mode].refresh_d;
        *denominator_ret = video_modes[current_video_mode].refresh_n;
    } else {
        *numerator_ret = 0;
        *denominator_ret = 1;
    }
}

/*-----------------------------------------------------------------------*/

int sys_graphics_has_focus(void)
{
    return x11_window && window_focused;
}

/*-----------------------------------------------------------------------*/

void sys_graphics_start_frame(int *width_ret, int *height_ret)
{
    *width_ret = window_width;
    *height_ret = window_height;
    opengl_start_frame();
}

/*-----------------------------------------------------------------------*/

void sys_graphics_finish_frame(void)
{
    glXSwapBuffers(x11_display, glx_window);
}

/*-----------------------------------------------------------------------*/

void sys_graphics_sync(int flush)
{
    opengl_sync();
    if (flush) {
        opengl_free_dead_resources(1);
    }
    glXWaitGL();
    glXWaitX();
}

/*************************************************************************/
/******************* Linux-internal exported routines ********************/
/*************************************************************************/

int linux_open_display(void)
{
    /* Look up symbols from optional X11 extension libraries. */
    const char *libname;
    int missing_symbol;
    #define LOAD(name)  ( \
        missing_symbol = 0, \
        libname = (name), \
        dlopen(libname, RTLD_NOW | RTLD_GLOBAL) \
    )
    /* We could use the return value of dlopen() instead of RTLD_DEFAULT
     * here, but RTLD_DEFAULT allows user override via LD_PRELOAD (and
     * also enables the overrides in src/test/sysdep/linux/graphics.c). */
    #define LOOKUP(sym)  do { \
        if (UNLIKELY(!((sym) = dlsym(RTLD_DEFAULT, #sym)))) { \
            fprintf(stderr, "%s: Symbol %s not found\n", libname, #sym); \
            missing_symbol = 1; \
        } \
    } while (0)
    if (LOAD("libXi.so.6")) {
        LOOKUP(XIFreeDeviceInfo);
        LOOKUP(XIQueryDevice);
        LOOKUP(XIQueryVersion);
        LOOKUP(XISelectEvents);
        if (UNLIKELY(missing_symbol)) {
            XIQueryVersion = NULL;
        }
    }
    if (LOAD("libXinerama.so.1")) {
        LOOKUP(XineramaIsActive);
        LOOKUP(XineramaQueryExtension);
        LOOKUP(XineramaQueryScreens);
        if (UNLIKELY(missing_symbol)) {
            XineramaQueryExtension = NULL;
        }
    }
    if (LOAD("libXrandr.so.2")) {
        LOOKUP(XRRFreeCrtcInfo);
        LOOKUP(XRRFreeOutputInfo);
        LOOKUP(XRRFreeScreenResources);
        LOOKUP(XRRGetCrtcInfo);
        LOOKUP(XRRGetOutputInfo);
        LOOKUP(XRRGetScreenResources);
        LOOKUP(XRRGetScreenSizeRange);
        LOOKUP(XRRQueryExtension);
        LOOKUP(XRRQueryVersion);
        LOOKUP(XRRSetCrtcConfig);
        LOOKUP(XRRSetScreenSize);
        if (UNLIKELY(missing_symbol)) {
            XRRQueryExtension = NULL;
        }
        LOOKUP(XRRFreePanning);
        LOOKUP(XRRGetPanning);
        LOOKUP(XRRSetPanning);
        if (missing_symbol) {
            XRRFreePanning = NULL;
            XRRGetPanning = NULL;
            XRRSetPanning = NULL;
        }
    }
    if (LOAD("libXxf86vm.so.1")) {
        LOOKUP(XF86VidModeGetAllModeLines);
        LOOKUP(XF86VidModeGetModeLine);
        LOOKUP(XF86VidModeQueryExtension);
        LOOKUP(XF86VidModeSwitchToMode);
        if (UNLIKELY(missing_symbol)) {
            XF86VidModeQueryExtension = NULL;
        }
    }
    #undef LOAD
    #undef LOOKUP

    /* Normally only one thread should ever do graphics calls, but since
     * we shouldn't be making frequent X calls the overhead won't hurt,
     * so play it safe. */
    XInitThreads();

    /* Open the display. */
    const char *display_name = getenv("DISPLAY");
    if (!display_name || !*display_name) {
        DLOG("No DISPLAY variable found");
        return 0;
    }
    x11_display = XOpenDisplay(display_name);
    if (!x11_display) {
        DLOG("Failed to open display");
        return 0;
    }

    /* Look up extension names for use in the error handler (since we can't
     * do queries from the error handler itself). */
    for (int i = 0; i < lenof(x11_extensions); i++) {
        x11_extensions[i] = NULL;
    }
    int num_extensions = 0;
    x11_extensions_raw = XListExtensions(x11_display, &num_extensions);
    for (int i = 0; i < num_extensions; i++) {
        int extension_major;
        ASSERT(XQueryExtension(x11_display, x11_extensions_raw[i],
                               &extension_major,
                               (int[1]){0}, (int[1]){0}), continue);
        ASSERT(extension_major >= 128 && extension_major < 256, continue);
        x11_extensions[extension_major-128] = x11_extensions_raw[i];
    }

    /* The default X11 error handler terminates the program, which is
     * rather user-unfriendly, so we immediately set up an error handler
     * to catch and record errors without terminating.  (This doesn't
     * prevent Xlib from terminating the program if the connection to the
     * X server is lost, but that probably means the user logged out or
     * otherwise killed the server, so termination is appropriate in that
     * case.) */
    XSetErrorHandler(x11_error_handler);

    return 1;
}

/*-----------------------------------------------------------------------*/

void linux_close_display(void)
{
    XFreeExtensionList(x11_extensions_raw);
    x11_extensions_raw = NULL;
    XCloseDisplay(x11_display);
    x11_display = NULL;
}

/*-----------------------------------------------------------------------*/

void linux_close_window(void)
{
    if (x11_window) {
        close_window();
    }
}

/*-----------------------------------------------------------------------*/

void linux_reset_video_mode(void)
{
    if (original_video_mode != NULL && current_video_mode >= 0) {
        const int mode =
            original_video_mode[video_modes[current_video_mode].screen];
        if (current_video_mode != mode) {
            set_video_mode(mode);
        }
    }
}

/*-----------------------------------------------------------------------*/

Display *linux_x11_display(void)
{
    return x11_display;
}

/*-----------------------------------------------------------------------*/

Window linux_x11_window(void)
{
    return x11_window;
}

/*-----------------------------------------------------------------------*/

int linux_x11_window_width(void)
{
    return window_width;
}

/*-----------------------------------------------------------------------*/

int linux_x11_window_height(void)
{
    return window_height;
}

/*-----------------------------------------------------------------------*/

int linux_x11_screen(void)
{
    return x11_screen;
}

/*-----------------------------------------------------------------------*/

XIC linux_x11_ic(void)
{
    return x11_ic;
}

/*-----------------------------------------------------------------------*/

WindowManagerID linux_window_manager(void)
{
    return window_manager;
}

/*-----------------------------------------------------------------------*/

int linux_get_window_event(XEvent *event_ret)
{
    PRECOND(event_ret != NULL, return 0);
    PRECOND(x11_window != None, return 0);

    int retval = 0;
    const int window_was_focused = window_focused;

    while (XPending(x11_display)) {
        XNextEvent(x11_display, event_ret);
        if (XFilterEvent(event_ret, None)) {
            continue;
        }
        if (event_ret->type != GenericEvent
         && event_ret->xany.window != x11_window) {
            /* Ignore events not directed at our window. */
        } else if (wm_protocols && wm_delete_window
                   && event_ret->type == ClientMessage
                   && event_ret->xclient.message_type == wm_protocols
                   && event_ret->xclient.format == 32
                   && (Atom)event_ret->xclient.data.l[0] == wm_delete_window) {
            linux_set_quit_requested();
        } else if (wm_protocols && net_wm_ping
                   && event_ret->type == ClientMessage
                   && event_ret->xclient.message_type == wm_protocols
                   && event_ret->xclient.format == 32
                   && (Atom)event_ret->xclient.data.l[0] == net_wm_ping) {
            event_ret->xclient.window = x11_root;
            XSendEvent(x11_display, x11_root, False,
                       SubstructureRedirectMask | SubstructureNotifyMask,
                       event_ret);
        } else if (event_ret->type == ConfigureNotify) {
            window_width = event_ret->xconfigure.width;
            window_height = event_ret->xconfigure.height;
            opengl_set_display_size(window_width, window_height);
        } else if (event_ret->type == FocusIn) {
            window_focused = 1;
        } else if (event_ret->type == FocusOut) {
            window_focused = 0;
        } else if (event_ret->type == MapNotify) {
            window_mapped = 1;
        } else if (event_ret->type == UnmapNotify) {
            window_mapped = 0;
        } else if (wm_state
                   && event_ret->type == PropertyNotify
                   && event_ret->xproperty.atom == wm_state) {
            const Atom state = get_property(x11_window, wm_state);
            window_iconified = (state == IconicState);
            if (state == IconicState && window_fullscreen) {
                saved_video_mode = current_video_mode;
                linux_reset_video_mode();
            } else if (state == NormalState && saved_video_mode >= 0) {
                if (window_fullscreen) {
                    if (set_video_mode(saved_video_mode)) {
                        scroll_to_window(window_width/2, window_height/2);
                    } else {
                        set_window_fullscreen(0, 0,
                                              window_width, window_height);
                    }
                }
                saved_video_mode = -1;
            }
            linux_set_window_grab(grab_requested);
        } else {
            retval = 1;
            break;
        }
    }

    /* We delay this check until after the loop so we don't respond
     * unnecessarily to a FocusIn/FocusOut pair which cancel each other out. */
    if (window_was_focused != window_focused) {
        linux_set_window_grab(grab_requested);
        if (!window_focused && window_fullscreen
         && should_minimize_fullscreen()) {
            XIconifyWindow(x11_display, x11_window, x11_screen);
        }
    }

    return retval;
}

/*-----------------------------------------------------------------------*/

void linux_set_window_grab(int grab)
{
    grab_requested = grab;

    if (!x11_window) {
        return;
    }

    /* Grab input while in fullscreen if necessary to prevent scrolling.
     * We do _not_ need to grab when using XRandR and EWMH-style fullscreen
     * unless all of the following hold:
     *    - The server does not support XRandR 1.3 (panning control).
     *    - There are multiple monitors configured as a single X11 screen.
     *    - The current video mode is a non-default mode.
     * (This assumes that in such a multi-monitor setup, no monitors would
     * scroll if all monitors were in their default mode, which is the case
     * for typical configurations.) */
    if (window_fullscreen) {
        if (video_mode_interface != XRANDR) {
            grab = 1;
        } else if (fullscreen_method != EWMH_FULLSCREEN) {
            grab = 1;
        } else if (!XRRGetPanning
                   && x11_num_screens > ScreenCount(x11_display)
                   && current_video_mode != original_video_mode[real_screen]) {
            grab = 1;
        }
    }
    /* Never grab input while iconified or not focused. */
    if (!window_focused || window_iconified) {
        grab = 0;
    }

    if (grab && !window_grabbed) {
        int error = XGrabPointer(x11_display, x11_window, True, 0,
                                 GrabModeAsync, GrabModeAsync, x11_window,
                                 None, CurrentTime);
        if (error) {
            DLOG("Failed to grab pointer (%s)",
                 error == GrabNotViewable ? "window not visible" :
                 error == AlreadyGrabbed ? "pointer already grabbed" :
                 error == GrabFrozen ? "pointer frozen by other grab" :
                 error == GrabInvalidTime ? "pointer grabbed by other client" :
                 "unknown error");
            return;
        }
        window_grabbed = 1;
    } else if (!grab && window_grabbed) {
        XUngrabPointer(x11_display, CurrentTime);
        window_grabbed = 0;
    }
}

/*-----------------------------------------------------------------------*/

int linux_get_window_grab(void)
{
    return window_grabbed;
}

/*-----------------------------------------------------------------------*/

int linux_x11_get_error(void)
{
    const int error = x11_error;
    x11_error = 0;
    return error;
}

/*-----------------------------------------------------------------------*/

int linux_x11_touchscreen_present(void)
{
    return xi2_touchscreen_present;
}

/*************************************************************************/
/***************** Local routines: Low-level X11 helpers *****************/
/*************************************************************************/

static int x11_error_handler(Display *display, XErrorEvent *error)
{
    if (x11_error) {
        /* Don't report subsequent errors (since they were probably caused
         * by the first error). */
        return 0;
    }
    x11_error = error->error_code;

    char error_name[1000];
    XGetErrorText(display, error->error_code, error_name,
                  sizeof(error_name));

    char request_code[8];
    char request_name[1000];
    if (error->request_code < 128) {
        ASSERT(strformat_check(request_code, sizeof(request_code), "%d",
                               error->request_code));
        XGetErrorDatabaseText(display, "XRequest", request_code,
                              "???", request_name, sizeof(request_name));
    } else {
        ASSERT(strformat_check(request_code, sizeof(request_code), "%d.%d",
                               error->request_code, error->minor_code));
        char extended_code[1000];
        ASSERT(strformat_check(extended_code, sizeof(extended_code), "%s.%d",
                               x11_extensions[error->request_code - 128],
                               error->minor_code));
        XGetErrorDatabaseText(display, "XRequest", extended_code,
                              "???", request_name, sizeof(request_name));
    }

    DLOG("X11 error %d (%s) in request %s (%s)",
         error->error_code, error_name, request_code, request_name);
    return 0;
}

/*-----------------------------------------------------------------------*/

static void get_resource_class(char *buf, int bufsize)
{
    /* Allow the user to override the resource class. */
    const char *override = getenv("SIL_X11_RESOURCE_CLASS");
    if (override && *override) {
        strformat(buf, bufsize, "%s", override);
        return;
    }

    /* Pull the name of the executable from /proc and use that, if possible. */
    char exec_path[PATH_MAX+1];
    const int exec_len = readlink("/proc/self/exe",
                                  exec_path, sizeof(exec_path)-1);
    if (exec_len > 0) {
        exec_path[exec_len] = 0;
        const char *s = strrchr(exec_path, '/');
        if (s) {
            s++;
        } else {
            s = exec_path;
        }
        strformat(buf, bufsize, "%s", s);
        return;
    }

    /* No way to find out who we are, so just use a default. */
    strformat(buf, bufsize, "SIL");
}

/*-----------------------------------------------------------------------*/

typedef struct WaitForEventData WaitForEventData;
struct WaitForEventData {
    Window window;
    int event_type;
};

static void wait_for_event(Window window, int event_type, XEvent *event_ret)
{
    WaitForEventData data = {.window = window, .event_type = event_type};
    XEvent event;
    XIfEvent(x11_display, &event, wait_for_event_predicate, (XPointer)&data);
    if (event_ret) {
        *event_ret = event;
    }
}

static int wait_for_event_predicate(UNUSED Display *display, XEvent *event,
                                    XPointer arg)
{
    /* XPointer is "char *" instead of "void *" for some reason, so we
     * need casts to work around cast-align warnings. */
    WaitForEventData *data = (WaitForEventData *)(void *)arg;
    return event->type == data->event_type
        && event->xany.window == data->window;
}

/*-----------------------------------------------------------------------*/

static Atom get_property(Window window, Atom property)
{
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop;
    int result = XGetWindowProperty(
        x11_display, window, property, 0, 1, False, AnyPropertyType,
        &actual_type, &actual_format, &nitems, &bytes_after, &prop);
    if (result == Success && actual_format == 32 && nitems >= 1) {
        Atom value = *(Atom *)(void *)prop;
        XFree(prop);
        return value;
    }
    return 0;
}

/*************************************************************************/
/**************** Local routines: Initialization helpers *****************/
/*************************************************************************/

static int scan_for_touchscreen(void)
{
    int num_input_devices = 0;
    XIDeviceInfo *input_devices = XIQueryDevice(
        x11_display, XIAllMasterDevices, &num_input_devices);
    if (!input_devices) {
        return 0;
    }

    int found = 0;
    for (int i = 0; i < num_input_devices; i++) {
        for (int j = 0; j < input_devices[i].num_classes; j++) {
            XIAnyClassInfo *info = input_devices[i].classes[j];
            if (info->type == XITouchClass) {
                XITouchClassInfo *touch_info = (XITouchClassInfo *)info;
                if (touch_info->mode == XIDirectTouch) {
                    found = 1;
                    break;
                }
            }
        }
    }

    XIFreeDeviceInfo(input_devices);
    return found;
}

/*-----------------------------------------------------------------------*/

static void detect_window_manager(int screen)
{
    window_manager = WM_UNNAMED;

    if (!net_supporting_wm_check) {
        return;
    }

    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop;

    int result = XGetWindowProperty(
        x11_display, RootWindow(x11_display, screen), net_supporting_wm_check,
        0, 1, False, XA_WINDOW, &actual_type, &actual_format, &nitems,
        &bytes_after, &prop);
    if (result != Success || actual_type == 0) {
        /* Could be MWM. */
        Atom motif_wm_info = XInternAtom(x11_display, "_MOTIF_WM_INFO", True);
        if (motif_wm_info) {
            result = XGetWindowProperty(
                x11_display, RootWindow(x11_display, screen), motif_wm_info,
                0, 1, False, XA_WINDOW, &actual_type, &actual_format, &nitems,
                &bytes_after, &prop);
            if (result == Success && actual_type != 0) {
                if (actual_type == motif_wm_info) {
                    window_manager = WM_MWM;
                }
                XFree(prop);
            }
        }
        return;
    }
    if (actual_type != XA_WINDOW) {
        XFree(prop);
        return;
    }
    ASSERT(actual_format == 32);
    ASSERT(nitems >= 1);
    Window supporting_window = *(long *)(void *)prop;
    XFree(prop);

    char name[101];
    *name = '\0';
    const int name_max = sizeof(name) - 1;
    result = XGetWindowProperty(
        x11_display, supporting_window, net_wm_name, 0, name_max/4,
        False, AnyPropertyType, &actual_type, &actual_format, &nitems,
        &bytes_after, &prop);
    if (result == Success && actual_type == 0) {  // i.e. does not exist
        result = BadValue;
    }
    if (result == Success
     && actual_type != utf8_string
     && actual_type != XA_STRING) {
        DLOG("Bad type %ld for _NET_WM_NAME", actual_type);
        XFree(prop);
        result = BadMatch;
    }
    if (result == Success) {
        ASSERT(actual_format == 8, nitems = 0);
        memcpy(name, prop, nitems);
        name[nitems] = '\0';
        XFree(prop);
        if (strncmp(name, "IceWM ", 6) == 0) {
            window_manager = WM_ICEWM;
        } else if (*name == 'e'
                   && name[1]
                   && strchr("123456789", name[1])
                   && !name[2 + strspn(name+2, "0123456789")]) {
            window_manager = WM_ENLIGHTENMENT;  // eNN
        } else {
            window_manager = WM_UNKNOWN;
            static const struct {const char *name; int id;} wm_list[] = {
                {"awesome",         WM_AWESOME},
                {"Blackbox",        WM_BLACKBOX},
                {"bspwm",           WM_BSPWM},
                {"CWM",             WM_CWM},
                {"echinus",         WM_ECHINUS},
                {"Fluxbox",         WM_FLUXBOX},
                {"FVWM",            WM_FVWM},
                {"GNOME Shell",     WM_GNOME_SHELL},
                {"GoomwW",          WM_GOOMWWM},
                {"herbstluftwm",    WM_HERBSTLUFTWM},
                {"i3",              WM_I3},
                {"JWM",             WM_JWM},
                {"KWin",            WM_KWIN},
                {"LG3D",            WM_SPECTRWM},
                {"lwm",             WM_LWM},
                {"Marco",           WM_MARCO},
                {"matwm2",          WM_MATWM2},
                {"Metacity",        WM_METACITY},
                {"musca",           WM_MUSCA},
                {"Mutter",          WM_MUTTER},
                {"Mutter (Muffin)", WM_MUFFIN},
                {"notion",          WM_NOTION},
                {"Openbox",         WM_OPENBOX},
                {"oroborus",        WM_OROBORUS},
                {"pekwm",           WM_PEKWM},
                {"qtile",           WM_QTILE},
                {"Sawfish",         WM_SAWFISH},
                {"wmii",            WM_WMII},
            };
            for (int i = 0; i < lenof(wm_list); i++) {
                if (strcmp(name, wm_list[i].name) == 0) {
                    window_manager = wm_list[i].id;
                    break;
                }
            }
        }
    } else {  // _NET_WM_NAME not found
        /* Window Maker (http://windowmaker.org/) doesn't set _NET_WM_NAME
         * (or even WM_NAME) on its supporting window, but we can detect
         * it by the presence of other properties. */
        Atom windowmaker_noticeboard =
            XInternAtom(x11_display, "_WINDOWMAKER_NOTICEBOARD", True);
        if (windowmaker_noticeboard) {
            result = XGetWindowProperty(
                x11_display, supporting_window, windowmaker_noticeboard,
                0, 1, False, XA_WINDOW, &actual_type, &actual_format,
                &nitems, &bytes_after, &prop);
            if (result == Success && actual_type != 0) {
                if (actual_type == XA_WINDOW && nitems == 1) {
                    window_manager = WM_WINDOWMAKER;
                }
                XFree(prop);
            }
        }
    }
}

/*-----------------------------------------------------------------------*/

static void choose_fullscreen_method(void)
{
    /* Allow the user to override our choice with an environment variable. */
    const char *override = getenv("SIL_X11_FULLSCREEN_METHOD");
    if (override && strcmp(override, "XMOVEWINDOW") == 0) {
            fullscreen_method = XMOVEWINDOW;
    } else if (override && strcmp(override, "EWMH_FULLSCREEN") == 0) {
        fullscreen_method = EWMH_FULLSCREEN;
    } else {
        if (override && *override) {
            fprintf(stderr, "Warning: Ignoring unrecognized value for"
                    " SIL_X11_FULLSCREEN_METHOD: %s\n", override);
        }

        /* See if the WM supports EWMH-style fullscreen switching.  The
         * "proper" method to do this requires an existing window, but we need
         * to know whether we can use this method before creating the window
         * because the logic for changing screen modes depends in part on the
         * fullscreen method we choose here, and for this method we need to
         * change the screen mode before mapping the window in order for the
         * window to be set to the correct size.  (Also, the "proper" detection
         * method is unreliable because it potentially requires changing window
         * attributes and waiting an indefinite time for the WM to update the
         * _NET_WM_ALLOWED_ACTIONS property.)  So we only check whether the
         * relevant atoms are present, assuming that if they are, the window
         * manager must have added them and it therefore supports them. */
        int can_wm_fullscreen = 0;
        if (net_wm_state && net_wm_state_fullscreen
         && net_wm_allowed_actions && net_wm_action_fullscreen) {
            can_wm_fullscreen = 1;
        }

        /* Blacklist WMs that are known to get WM-based fullscreen wrong. */
        if (can_wm_fullscreen) {
            ASSERT(window_manager < lenof(ewmh_fullscreen_blacklist),
                   window_manager = WM_UNNAMED);
            if (ewmh_fullscreen_blacklist[window_manager]) {
                can_wm_fullscreen = 0;
            }
        }

        /* Some WMs which support EWMH-style fullscreen get confused by
         * multi-head, single-X11-screen configurations, so only whitelist
         * those which we know behave correctly. */
        if (can_wm_fullscreen && ((video_mode_interface == XRANDR
                                   && x11_num_screens > 1
                                   && ScreenCount(x11_display) == 1)
                                  || use_xinerama)) {
            ASSERT(window_manager < lenof(ewmh_fullscreen_xinerama_whitelist),
                   window_manager = WM_UNNAMED);
            if (!ewmh_fullscreen_xinerama_whitelist[window_manager]) {
                can_wm_fullscreen = 0;
            }
        }

        /* Choose a fullscreen method. */
        if (can_wm_fullscreen) {
            fullscreen_method = EWMH_FULLSCREEN;
        } else {
            fullscreen_method = XMOVEWINDOW;
        }
    }

    /* Decide whether to set new fullscreen windows as fullscreen
     * immediately or after showing them. */
    override = getenv("SIL_X11_CREATE_FULLSCREEN");
    if (override && strcmp(override, "0") == 0) {
        allow_early_fullscreen = 0;
    } else if (override && strcmp(override, "1") == 0) {
        allow_early_fullscreen = 1;
    } else {
        if (override && *override) {
            fprintf(stderr, "Warning: Ignoring unrecognized value for"
                    " SIL_X11_CREATE_FULLSCREEN: %s\n", override);
        }
        ASSERT(window_manager < lenof(early_fullscreen_whitelist),
               window_manager = WM_UNNAMED);
        allow_early_fullscreen = early_fullscreen_whitelist[window_manager];
    }

    /* For the EWMH method, figure out whether we need any special hacks
     * to make the window manager do the Right Thing. */
    if (fullscreen_method == EWMH_FULLSCREEN) {
        override = getenv("SIL_X11_EWMH_FULLSCREEN_MOVE_BEFORE");
        if (override && strcmp(override, "0") == 0) {
            ewmh_fullscreen_move_before = 0;
        } else if (override && strcmp(override, "1") == 0) {
            ewmh_fullscreen_move_before = 1;
        } else {
            if (override && *override) {
                fprintf(stderr, "Warning: Ignoring unrecognized value for"
                        " SIL_X11_EWMH_FULLSCREEN_MOVE_BEFORE: %s\n",
                        override);
            }
            ASSERT(window_manager < lenof(ewmh_fullscreen_monitors_whitelist),
                   window_manager = WM_UNNAMED);
            ewmh_fullscreen_move_before =
                !ewmh_fullscreen_monitors_whitelist[window_manager];
        }

        override = getenv("SIL_X11_EWMH_FULLSCREEN_RESIZE_AFTER");
        if (override && strcmp(override, "0") == 0) {
            ewmh_fullscreen_resize_after = 0;
        } else if (override && strcmp(override, "1") == 0) {
            ewmh_fullscreen_resize_after = 1;
        } else {
            if (override && *override) {
                fprintf(stderr, "Warning: Ignoring unrecognized value for"
                        " SIL_X11_EWMH_FULLSCREEN_RESIZE_AFTER: %s\n",
                        override);
            }
            ASSERT(window_manager < lenof(ewmh_fullscreen_resize_after_list),
                   window_manager = WM_UNNAMED);
            ewmh_fullscreen_resize_after =
                ewmh_fullscreen_resize_after_list[window_manager];
        }
    }

    /* For non-EWMH fullscreen, check whether we should ignore the
     * presence of the _MOTIF_WM_HINTS atom. */
    if (fullscreen_method == XMOVEWINDOW) {
        override = getenv("SIL_X11_USE_TRANSIENT_FOR_HINT");
        if (override && strcmp(override, "0") == 0) {
            force_XSetTransientForHint = 0;
        } else if (override && strcmp(override, "1") == 0) {
            force_XSetTransientForHint = 1;
        } else {
            if (override && *override) {
                fprintf(stderr, "Warning: Ignoring unrecognized value for"
                        " SIL_X11_USE_TRANSIENT_FOR_HINT: %s\n", override);
            }
            ASSERT(window_manager < lenof(motif_wm_hints_blacklist),
                   window_manager = WM_UNNAMED);
            force_XSetTransientForHint =
                motif_wm_hints_blacklist[window_manager];
        }
    }
}

/*************************************************************************/
/***************** Local routines: Video mode management *****************/
/*************************************************************************/

static int add_video_modes_none(
    int screen, const XineramaScreenInfo *xinerama_screen_info)
{
    VideoModeInfo info;
    info.screen = screen;
    info.x11_screen = use_xinerama ? 0 : screen;

    if (use_xinerama) {
        info.width = xinerama_screen_info[screen].width;
        info.height = xinerama_screen_info[screen].height;
        info.x = xinerama_screen_info[screen].x_org;
        info.y = xinerama_screen_info[screen].y_org;
    } else {
        Screen *screen_p = ScreenOfDisplay(x11_display, screen);
        info.width = WidthOfScreen(screen_p);
        info.height = HeightOfScreen(screen_p);
        info.x = 0;
        info.y = 0;
    }
    info.refresh_n = 0;
    info.refresh_d = 1;
    if (UNLIKELY(!add_video_mode(&info))) {
        DLOG("Out of memory initializing video modes");
        return 0;
    }
    original_video_mode[screen] = num_video_modes - 1;
    return 1;
}

/*-----------------------------------------------------------------------*/

static int add_video_modes_vidmode(
    int screen, const XineramaScreenInfo *xinerama_screen_info)
{
    VideoModeInfo info;
    info.screen = screen;
    info.x11_screen = use_xinerama ? 0 : screen;

    if (use_xinerama && screen != 0) {
        /* The VidMode extension can only change modes on the first monitor. */
        goto error_add_fallback_mode;
    }
    info.x = 0;
    info.y = 0;
    XF86VidModeModeLine modeline;
    mem_clear(&modeline, sizeof(modeline));
    XF86VidModeModeInfo **modes;
    int dotclock, num_modes;
    if (UNLIKELY(!XF86VidModeGetModeLine(x11_display, screen,
                                         &dotclock, &modeline))) {
        DLOG("XF86VidModeGetModeLine() failed for screen %d", screen);
        goto error_add_fallback_mode;
    }
    XF86VidModeModeInfo default_mode;
    vidmode_modeline_to_modeinfo(&modeline, dotclock, &default_mode);
    vidmode_modeinfo_to_mode(&default_mode, &info);
    if (UNLIKELY(!add_video_mode(&info))) {
        DLOG("Out of memory initializing video modes");
        return 0;
    }
    original_video_mode[screen] = num_video_modes - 1;

    if (UNLIKELY(!XF86VidModeGetAllModeLines(
                     x11_display, screen, &num_modes, &modes))) {
        DLOG("XF86VidModeGetAllModeLines() failed for screen %d", screen);
        return 1;
    }
    for (int i = 0; i < num_modes; i++) {
        if (modes[i]->hdisplay == default_mode.hdisplay
         && modes[i]->vdisplay == default_mode.vdisplay
         && modes[i]->htotal == default_mode.htotal
         && modes[i]->vtotal == default_mode.vtotal
         && modes[i]->dotclock == default_mode.dotclock) {
            continue;  // This is the current display mode.
        }
        vidmode_modeinfo_to_mode(modes[i], &info);
        if (UNLIKELY(!add_video_mode(&info))) {
            DLOG("Out of memory initializing video modes");
            XFree(modes);
            return 0;
        }
    }
    XFree(modes);

    return 1;

  error_add_fallback_mode:
    if (use_xinerama) {
        info.width = xinerama_screen_info[screen].width;
        info.height = xinerama_screen_info[screen].height;
        info.x = xinerama_screen_info[screen].x_org;
        info.y = xinerama_screen_info[screen].y_org;
    } else {
        Screen *screen_p = ScreenOfDisplay(x11_display, screen);
        info.width = WidthOfScreen(screen_p);
        info.height = HeightOfScreen(screen_p);
        info.x = 0;
        info.y = 0;
    }
    info.refresh_n = 0;
    info.refresh_d = 1;
    if (UNLIKELY(!add_video_mode(&info))) {
        DLOG("Out of memory initializing video modes");
        return 0;
    }
    original_video_mode[screen] = num_video_modes - 1;
    return 1;
}

/*-----------------------------------------------------------------------*/

static int add_video_modes_xrandr(
    int screen, XRRScreenResources *xrandr_screen0_res,
    const XineramaScreenInfo *xinerama_screen_info, int num_xinerama_screens)
{
    const int screen_for_x11 = (ScreenCount(x11_display) > 1 ? screen : 0);
    VideoModeInfo info;
    info.screen = screen;
    info.x11_screen = screen_for_x11;
    info.xrandr_info.xinerama_screen = -1;

    XRRScreenResources *res;
    XRRScreenResources *alloced_res = NULL;
    if (ScreenCount(x11_display) > 1) {
        alloced_res = XRRGetScreenResources(
            x11_display, RootWindow(x11_display, screen_for_x11));
        res = alloced_res;
    } else {
        res = xrandr_screen0_res;
    }
    if (UNLIKELY(!res)) {
        DLOG("Unable to get XRandR screen resources for screen %d,"
             " generating fallback video mode", screen);
        goto error_add_fallback_mode;
    }

    int output_index = (ScreenCount(x11_display) > 1 ? 0 : screen);
    XRROutputInfo *output_info = NULL;
    for (int i = 0; i < res->noutput; i++) {
        XRROutputInfo *oi =
            XRRGetOutputInfo(x11_display, res, res->outputs[i]);
        if (oi) {
            if (oi->crtc && oi->connection != RR_Disconnected) {
                if (output_index == 0) {
                    info.xrandr_info.output = res->outputs[i];
                    output_info = oi;
                    break;
                } else {
                    output_index--;
                }
            }
            XRRFreeOutputInfo(oi);
        }
    }
    if (UNLIKELY(!output_info)) {
        DLOG("Unable to find XRandR output for screen %d, generating"
             " fallback video mode", screen);
        goto error_add_fallback_mode;
    }

    XRRCrtcInfo *crtc = XRRGetCrtcInfo(x11_display, res, output_info->crtc);
    if (UNLIKELY(!crtc)) {
        DLOG("Unable to retrieve XRandR CRTC info for screen %d,"
             " generating fallback video mode", screen);
        XRRFreeOutputInfo(output_info);
        goto error_add_fallback_mode;
    }
    const int rotated = (crtc->rotation & (RR_Rotate_90 | RR_Rotate_270)) != 0;

    if (ScreenCount(x11_display) == 1 && xinerama_screen_info) {
        for (int i = 0; i < num_xinerama_screens; i++) {
            if (xinerama_screen_info[i].x_org == crtc->x
             && xinerama_screen_info[i].y_org == crtc->y) {
                info.xrandr_info.xinerama_screen = i;
                break;
            }
        }
    }

    /* Add modes, but also make sure the current mode actually exists in
     * the mode list; if not, we'll delete the XRandR modes and use the
     * fallback instead. */
    const int old_num_modes = num_video_modes;
    original_video_mode[screen] = -1;
    for (int i = 0; i < output_info->nmode; i++) {
        const XRRModeInfo *mode = NULL;
        for (int j = 0; j < res->nmode; j++) {
            if (res->modes[j].id == output_info->modes[i]) {
                mode = &res->modes[j];
                break;
            }
        }
        if (UNLIKELY(!mode)) {
            DLOG("Mode %ld on screen %d (output %.*s) missing from mode list"
                 " -- XRandR bug?", output_info->modes[i], screen,
                 output_info->nameLen, output_info->name);
            continue;
        }
        info.xrandr_info.mode = mode->id;
        info.x = crtc->x;
        info.y = crtc->y;
        if (rotated) {
            info.width = mode->height;
            info.height = mode->width;
        } else {
            info.width = mode->width;
            info.height = mode->height;
        }
        info.refresh_n = mode->dotClock;
        info.refresh_d = mode->hTotal * mode->vTotal;
        if (mode->modeFlags & V_DBLSCAN) {
            info.refresh_d *= 2;
        }
        if (UNLIKELY(!add_video_mode(&info))) {
            DLOG("Out of memory initializing video modes");
            XRRFreeCrtcInfo(crtc);
            XRRFreeOutputInfo(output_info);
            if (alloced_res) {
                XRRFreeScreenResources(alloced_res);
            }
            return 0;
        }
        if (crtc->mode == mode->id) {
            original_video_mode[screen] = num_video_modes - 1;
        }
    }

    XRRFreeCrtcInfo(crtc);
    XRRFreeOutputInfo(output_info);
    if (alloced_res) {
        XRRFreeScreenResources(alloced_res);
        alloced_res = NULL;
    }
    if (original_video_mode[screen] < 0) {
        DLOG("Failed to find current XRandR mode for screen %d,"
             " generating fallback video mode", screen);
        num_video_modes = old_num_modes;
        goto error_add_fallback_mode;
    }

    return 1;

  error_add_fallback_mode:
    if (alloced_res) {
        XRRFreeScreenResources(alloced_res);
        alloced_res = NULL;
    }
    Screen *screen_p = ScreenOfDisplay(x11_display, screen_for_x11);
    info.x = 0;
    info.y = 0;
    info.width = WidthOfScreen(screen_p);
    info.height = HeightOfScreen(screen_p);
    info.refresh_n = 0;
    info.refresh_d = 1;
    /* We don't set up XRandR-specific data here, but since this will be
     * the only valid mode for this screen, the XRandR-specific fields
     * will never be referenced. */
    if (UNLIKELY(!add_video_mode(&info))) {
        DLOG("Out of memory initializing video modes");
        return 0;
    }
    original_video_mode[screen] = num_video_modes - 1;
    return 1;
}

/*-----------------------------------------------------------------------*/

static void vidmode_modeline_to_modeinfo(
    const XF86VidModeModeLine *modeline, int dotclock,
    XF86VidModeModeInfo *modeinfo_ret)
{
    PRECOND(modeline != NULL, return);
    PRECOND(modeinfo_ret != NULL, return);

    modeinfo_ret->dotclock = dotclock;
    modeinfo_ret->hdisplay = modeline->hdisplay;
    modeinfo_ret->hsyncstart = modeline->hsyncstart;
    modeinfo_ret->hsyncend = modeline->hsyncend;
    modeinfo_ret->htotal = modeline->htotal;
    modeinfo_ret->hskew = modeline->hskew;
    modeinfo_ret->vdisplay = modeline->vdisplay;
    modeinfo_ret->vsyncstart = modeline->vsyncstart;
    modeinfo_ret->vsyncend = modeline->vsyncend;
    modeinfo_ret->vtotal = modeline->vtotal;
    modeinfo_ret->flags = modeline->flags;
    modeinfo_ret->privsize = modeline->privsize;
    modeinfo_ret->private = modeline->private;
}

/*-----------------------------------------------------------------------*/

static void vidmode_modeinfo_to_mode(const XF86VidModeModeInfo *modeinfo,
                                     VideoModeInfo *mode_ret)
{
    PRECOND(modeinfo != NULL, return);
    PRECOND(mode_ret != NULL, return);

    mode_ret->vidmode_info = *modeinfo;
    mode_ret->width = modeinfo->hdisplay;
    mode_ret->height = modeinfo->vdisplay;
    mode_ret->refresh_n = modeinfo->dotclock * 1000;
    mode_ret->refresh_d = modeinfo->htotal * modeinfo->vtotal;
    if (mode_ret->vidmode_info.flags & V_DBLSCAN) {
        mode_ret->refresh_d *= 2;
    }
}

/*-----------------------------------------------------------------------*/

static int add_video_mode(const VideoModeInfo *info)
{
    PRECOND(info != NULL, return 0);

    if (num_video_modes >= video_modes_size) {
        const int new_size = num_video_modes + 100;
        VideoModeInfo *new_modes = mem_realloc(
            video_modes, sizeof(*video_modes) * new_size, 0);
        if (UNLIKELY(!new_modes)) {
            return 0;
        }
        video_modes = new_modes;
        video_modes_size = new_size;
    }
    video_modes[num_video_modes++] = *info;
#ifdef SIL_INCLUDE_TESTS
    if (TEST_linux_graphics_duplicate_mode) {
        TEST_linux_graphics_duplicate_mode = 0;
        return add_video_mode(info);
    }
#endif
    return 1;
}

/*-----------------------------------------------------------------------*/

static int set_video_mode(int mode)
{
    PRECOND(mode >= 0 && mode < num_video_modes, return 0);

    const int screen = video_modes[mode].screen;
    if (screen != video_modes[current_video_mode].screen) {
        linux_reset_video_mode();
        current_video_mode = original_video_mode[screen];
    }

    if (mode == current_video_mode) {
        return 1;
    }

    XSync(x11_display, False);
    (void) linux_x11_get_error();

    switch (video_mode_interface) {
      case NONE:
        /* We can't get here because each screen has only one mode. */
        ASSERT(!"impossible"); goto error;  // NOTREACHED

      case VIDMODE:
        if (!XF86VidModeSwitchToMode(x11_display, video_modes[mode].x11_screen,
                                     &video_modes[mode].vidmode_info)) {
            DLOG("XV86VidModeSwitchToMode() failed for mode %dx%d",
                 video_modes[mode].width, video_modes[mode].height);
            goto error;
        }
        break;

      case XRANDR: {
        Window root = RootWindow(x11_display, video_modes[mode].x11_screen);
        XRRScreenResources *res = XRRGetScreenResources(x11_display, root);
        if (!res) {
            DLOG("XRRGetScreenResources() failed");
            goto xrr_error_return;
        }
        XRROutputInfo *output_info = XRRGetOutputInfo(
            x11_display, res, video_modes[mode].xrandr_info.output);
        if (!output_info) {
            DLOG("XRRGetOutputInfo() failed");
            goto xrr_error_free_res;
        }
        if (output_info->connection == RR_Disconnected) {
            DLOG("Display device is disconnected, can't change modes");
            goto xrr_error_free_output_info;
        }
        XRRCrtcInfo *crtc = XRRGetCrtcInfo(x11_display, res, output_info->crtc);
        if (!crtc) {
            DLOG("XRRGetCrtcInfo() failed");
            goto xrr_error_free_output_info;
        }
        const Status status =
            xrandr_set_crtc_mode(res, output_info, crtc, mode,
                                 (mode != original_video_mode[screen]));
        if (status == RRSetConfigInvalidTime
         || status == RRSetConfigInvalidConfigTime) {
            /* Somebody else just changed the config!  Try again. */
            XRRFreeCrtcInfo(crtc);
            XRRFreeOutputInfo(output_info);
            XRRFreeScreenResources(res);
            return set_video_mode(mode);
        } else if (status != RRSetConfigSuccess) {
            DLOG("Failed to set mode %dx%d@%d",
                 video_modes[mode].width, video_modes[mode].height,
                 (video_modes[mode].refresh_n + video_modes[mode].refresh_d/2)
                     / video_modes[mode].refresh_d);
            goto xrr_error_free_crtc;
        }
        XRRFreeCrtcInfo(crtc);
        XRRFreeOutputInfo(output_info);
        XRRFreeScreenResources(res);
        break;

      xrr_error_free_crtc:
        XRRFreeCrtcInfo(crtc);
      xrr_error_free_output_info:
        XRRFreeOutputInfo(output_info);
      xrr_error_free_res:
        XRRFreeScreenResources(res);
      xrr_error_return:
        goto error;
      }  // case XRANDR
    }

    XSync(x11_display, False);
    if (linux_x11_get_error()) {
        DLOG("X11 error detected while changing video mode");
        return 0;
    }

    current_video_mode = mode;
    return 1;

  error:
    XSync(x11_display, False);
    (void) linux_x11_get_error();
    return 0;
}

/*-----------------------------------------------------------------------*/

static Status xrandr_set_crtc_mode(
    XRRScreenResources *res, XRROutputInfo *output_info, XRRCrtcInfo *crtc,
    int mode, int save)
{
    PRECOND(res != NULL, return RRSetConfigFailed);
    PRECOND(mode >= 0 && mode < num_video_modes, return RRSetConfigFailed);

    Status status = RRSetConfigSuccess;

    const int screen = video_modes[mode].x11_screen;
    const Window root = RootWindow(x11_display, screen);

    /* This is not an atomic operation, so take the server lock while we
     * work to avoid window managers or other clients getting confused by
     * intermediate states. */
    XGrabServer(x11_display);

    /* Variables to hold the current screen size in case we need to restore
     * it on failure.  Zero indicates the screen size was not changed. */
    int saved_screen_width = 0, saved_screen_height = 0;

    /* Look up all outputs, CRTCs, and modes ahead of time. */
    struct {
        XRROutputInfo *output;
        XRRCrtcInfo *crtc;
        XRRModeInfo *mode;
        uint8_t disabled;  // Did we disable this CRTC for screen size change?
    } *outputs = mem_alloc(sizeof(*outputs) * res->noutput, 0,
                           MEM_ALLOC_TEMP | MEM_ALLOC_CLEAR);
    if (UNLIKELY(!outputs)) {
        XUngrabServer(x11_display);
        return RRSetConfigFailed;
    }
    for (int i = 0; i < res->noutput; i++) {
        outputs[i].output = XRRGetOutputInfo(x11_display, res, res->outputs[i]);
        if (outputs[i].output) {
            if (outputs[i].output->connection != RR_Disconnected) {
                outputs[i].crtc = XRRGetCrtcInfo(x11_display, res,
                                                 outputs[i].output->crtc);
                if (outputs[i].crtc) {
                    outputs[i].mode = NULL;
                    for (int j = 0; j < res->nmode; j++) {
                        if (res->modes[j].id == outputs[i].crtc->mode) {
                            outputs[i].mode = &res->modes[j];
                            break;
                        }
                    }
                    if (outputs[i].mode) {
                        continue;
                    }
                    DLOG("Mode %ld (output %.*s) missing from mode list --"
                         " XRandR bug?", output_info->modes[i],
                         output_info->nameLen, output_info->name);
                    XRRFreeCrtcInfo(outputs[i].crtc);
                    outputs[i].crtc = NULL;
                } else {
                    DLOG("Failed to get CRTC info for output %.*s",
                         outputs[i].output->nameLen, outputs[i].output->name);
                }
            }
        } else {
            DLOG("Failed to get output info for output %d", i);
        }
    }

    /* If changing away from the default mode, save the current screen size
     * and panning data so it can be restored when we return to the default
     * mode. */
    int saved_original_size = 0;
    if (save && !xrandr_original_width) {
        xrandr_original_width = DisplayWidth(x11_display, screen);
        xrandr_original_height = DisplayHeight(x11_display, screen);
        saved_original_size = 1;
    }

    /* Keep track of whether we allocated xrandr_pan_areas in this call so
     * we can free it on error. */
    int allocated_pan_areas = 0;
    if (save && !xrandr_pan_areas && XRRGetPanning) {
        ASSERT(XRRFreePanning);
        num_xrandr_pan_areas = res->noutput;
        xrandr_pan_areas =
            mem_alloc(sizeof(*xrandr_pan_areas) * res->noutput, 0, 0);
        if (xrandr_pan_areas) {
            XSync(x11_display, False);
            (void) linux_x11_get_error();
            for (int i = 0; i < num_xrandr_pan_areas; i++) {
                xrandr_pan_areas[i].output = res->outputs[i];
                XRROutputInfo *oi = outputs[i].output;
                if (oi && oi->connection != RR_Disconnected) {
                    xrandr_pan_areas[i].is_changed =
                        (oi->crtc == output_info->crtc);
                    xrandr_pan_areas[i].panning =
                        XRRGetPanning(x11_display, res, oi->crtc);
                } else {
                    xrandr_pan_areas[i].is_changed = 0;
                    xrandr_pan_areas[i].panning = NULL;
                }
                if (xrandr_pan_areas[i].panning) {
                    /* Some drivers seem to return X/Y as zero when panning
                     * is disabled, but calling XRRSetPanning() with those
                     * values resets the screen origin to (0,0), so we need
                     * to look up the current values ourselves.  We also
                     * need to fill in the actual width and height for
                     * screen size computation. */
                    XRRCrtcInfo *ci = outputs[i].crtc;
                    const XRRModeInfo *mi = outputs[i].mode;
                    if (xrandr_pan_areas[i].panning->width == 0) {
                        xrandr_pan_areas[i].panning->left = ci->x;
                        xrandr_pan_areas[i].panning->width = mi->width;
                    }
                    if (xrandr_pan_areas[i].panning->height == 0) {
                        xrandr_pan_areas[i].panning->top = ci->y;
                        xrandr_pan_areas[i].panning->height = mi->height;
                    }
                }
            }
        }
        allocated_pan_areas = (xrandr_pan_areas != NULL);
    }

    /* Change the screen size to match the post-mode-change configuration,
     * but only if we use the EWMH fullscreen method -- otherwise we may
     * need to scroll the viewport to find the window. */
    if (fullscreen_method == EWMH_FULLSCREEN) {

        /* Determine the new size of the screen.  When setting a custom
         * mode, we disable panning on all monitors (see notes below), so
         * this is just the bounding box of all monitor areas. */
        int screen_w, screen_h;
        if (!save) {
            screen_w = xrandr_original_width;
            screen_h = xrandr_original_height;
        } else {
            int xmin = INT_MAX, xmax = 0;
            int ymin = INT_MAX, ymax = 0;
            for (int i = 0; i < res->noutput; i++) {
                if (!outputs[i].output || !outputs[i].crtc) {
                    continue;
                }
                if (outputs[i].output->crtc == output_info->crtc) {
                    /* Use the mode we're about to set, not the current
                     * mode. */
                    xmin = min(xmin, outputs[i].crtc->x);
                    xmax = max(xmax, outputs[i].crtc->x
                                     + video_modes[mode].width);
                    ymin = min(ymin, outputs[i].crtc->y);
                    ymax = max(ymax, outputs[i].crtc->y
                                     + video_modes[mode].height);
                } else {
                    xmin = min(xmin, outputs[i].crtc->x);
                    xmax = max(xmax, outputs[i].crtc->x
                                     + (int)outputs[i].mode->width);
                    ymin = min(ymin, outputs[i].crtc->y);
                    ymax = max(ymax, outputs[i].crtc->y
                                     + (int)outputs[i].mode->height);
                }
            }
            screen_w = xmax - xmin;
            screen_h = ymax - ymin;
            ASSERT(screen_w > 0, screen_w = 1);
            ASSERT(screen_h > 0, screen_h = 1);
        }
        int screen_wmin, screen_wmax, screen_hmin, screen_hmax;
        if (XRRGetScreenSizeRange(x11_display, root, &screen_wmin,
                                  &screen_hmin, &screen_wmax, &screen_hmax)) {
            screen_w = bound(screen_w, screen_wmin, screen_wmax);
            screen_h = bound(screen_h, screen_hmin, screen_hmax);
        }

        /* If any monitors would not fit within the new screen size at
         * their current positions and resolutions, we need to disable the
         * associated CRTCs before setting the screen size.  This should
         * only impact the monitor we're actually operating on, but we loop
         * over all outputs just to be on the safe side. */
        for (int i = 0; i < res->noutput && status == RRSetConfigSuccess; i++) {
            if (!outputs[i].output || !outputs[i].crtc) {
                continue;
            }
            int xmin = outputs[i].crtc->x;
            int ymin = outputs[i].crtc->y;
            int xmax = xmin + outputs[i].mode->width;
            int ymax = ymin + outputs[i].mode->height;
            if (!save && xrandr_pan_areas) {
                /* The set of outputs could have changed since we saved the
                 * panning data (due to monitor hotplug, for example), so
                 * we need to manually match up outputs. */
                int j;
                for (j = 0; j < num_xrandr_pan_areas; j++) {
                    if (xrandr_pan_areas[j].output == res->outputs[i]) {
                        break;
                    }
                }
                if (j < num_xrandr_pan_areas && xrandr_pan_areas[j].panning) {
                    xmin = xrandr_pan_areas[j].panning->left;
                    ymin = xrandr_pan_areas[j].panning->top;
                    xmax = xmin + xrandr_pan_areas[j].panning->width;
                    ymax = ymin + xrandr_pan_areas[j].panning->height;
                }
            }
            if (xmax > screen_w || ymax > screen_h) {
                status = XRRSetCrtcConfig(
                    x11_display, res, outputs[i].output->crtc, CurrentTime,
                    0, 0, None, RR_Rotate_0, NULL, 0);
                XSync(x11_display, False);
                const int error = linux_x11_get_error();
                if (UNLIKELY(status != RRSetConfigSuccess || error)) {
                    DLOG("Failed to disable CRTC for output %.*s: %s %d",
                         outputs[i].output->nameLen, outputs[i].output->name,
                         status != RRSetConfigSuccess ? "status" : "X11 error",
                         status != RRSetConfigSuccess ? status : error);
                } else {
                    outputs[i].disabled = 1;
                }
            }
        }

        /* Change the screen size to fit the new mode (plus any other
         * monitors). */
        if (status == RRSetConfigSuccess) {
            saved_screen_width = DisplayWidth(x11_display, screen);
            saved_screen_height = DisplayHeight(x11_display, screen);
            XRRSetScreenSize(x11_display, root, screen_w, screen_h,
                             DisplayWidthMM(x11_display, screen),
                             DisplayHeightMM(x11_display, screen));
            XSync(x11_display, False);
            const int error = linux_x11_get_error();
            if (UNLIKELY(error)) {
                DLOG("Failed to set screen size to %dx%d: X11 error %d",
                     screen_w, screen_h, error);
                status = RRSetConfigFailed;
            }
        }

    }  // if (fullscreen_method == EWMH_FULLSCREEN)

    /* Actually change the video mode, and re-enable any other CRTCs we had
     * to disable above. */
    for (int i = 0; i < res->noutput && status == RRSetConfigSuccess; i++) {
        if (!outputs[i].output) {
            continue;
        }
        if (outputs[i].output->crtc == output_info->crtc) {
            status = XRRSetCrtcConfig(
                x11_display, res, output_info->crtc, CurrentTime,
                video_modes[mode].x, video_modes[mode].y,
                video_modes[mode].xrandr_info.mode, crtc->rotation,
                &video_modes[mode].xrandr_info.output, 1);
        } else if (outputs[i].disabled) {
            status = XRRSetCrtcConfig(
                x11_display, res, outputs[i].output->crtc, CurrentTime,
                outputs[i].crtc->x, outputs[i].crtc->y,
                outputs[i].crtc->mode, outputs[i].crtc->rotation,
                outputs[i].crtc->outputs, outputs[i].crtc->noutput);
        }
        XSync(x11_display, False);
        const int error = linux_x11_get_error();
        if (UNLIKELY(status != RRSetConfigSuccess || error)) {
            DLOG("Failed to configure CRTC for output %.*s: %s %d",
                 outputs[i].output->nameLen, outputs[i].output->name,
                 status != RRSetConfigSuccess ? "status" : "X11 error",
                 status != RRSetConfigSuccess ? status : error);
        }
    }

    /* If we failed at some point up to now, the display may be in an
     * inconsistent state.  Try to restore things to the way they were. */
    if (status != RRSetConfigSuccess) {
        if (saved_screen_width) {
            /* For the same reason as above, we may need to disable CRTCs
             * before changing the screen size.  Here, just disable
             * everything so we have the best chance of success. */
            for (int i = 0; i < res->noutput; i++) {
                if (!outputs[i].output || !outputs[i].crtc) {
                    continue;
                }
                const Status s = XRRSetCrtcConfig(
                    x11_display, res, outputs[i].output->crtc, CurrentTime,
                    0, 0, None, RR_Rotate_0, NULL, 0);
                XSync(x11_display, False);
                const int error = linux_x11_get_error();
                if (UNLIKELY(s != RRSetConfigSuccess || error)) {
                    DLOG("[recovery] Failed to disable CRTC for output %.*s:"
                         " %s %d",
                         outputs[i].output->nameLen, outputs[i].output->name,
                         s != RRSetConfigSuccess ? "status" : "X11 error",
                         s != RRSetConfigSuccess ? s : error);
                }
            }
            XRRSetScreenSize(x11_display, root,
                             saved_screen_width, saved_screen_height,
                             DisplayWidthMM(x11_display, screen),
                             DisplayHeightMM(x11_display, screen));
            XSync(x11_display, False);
            const int error = linux_x11_get_error();
            if (UNLIKELY(error)) {
                DLOG("[recovery] Failed to set screen size to %dx%d: X11"
                     " error %d", saved_screen_width, saved_screen_height,
                     error);
            }
        }
        for (int i = 0; i < res->noutput; i++) {
            if (!outputs[i].output || !outputs[i].crtc) {
                continue;
            }
            const Status s = XRRSetCrtcConfig(
                x11_display, res, outputs[i].output->crtc, CurrentTime,
                outputs[i].crtc->x, outputs[i].crtc->y,
                outputs[i].crtc->mode, outputs[i].crtc->rotation,
                outputs[i].crtc->outputs, outputs[i].crtc->noutput);
            XSync(x11_display, False);
            const int error = linux_x11_get_error();
            if (UNLIKELY(s != RRSetConfigSuccess || error)) {
                DLOG("[recovery] Failed to restore CRTC for output %.*s:"
                     " %s %d",
                     outputs[i].output->nameLen, outputs[i].output->name,
                     s != RRSetConfigSuccess ? "status" : "X11 error",
                     s != RRSetConfigSuccess ? s : error);
            }
        }
        if (saved_original_size) {
            xrandr_original_width = 0;
            xrandr_original_height = 0;
        }
    }

    /* If everything has gone well so far, update panning areas.  If we're
     * restoring the default mode, we reload all panning values we saved
     * above.  Otherwise we're setting a custom mode, and we just disable
     * panning on all monitors.  The rationale for this is that if only a
     * single monitor is in use, it's the monitor we just set a fullscreen
     * mode on and we shouldn't pan even if the user has a larger root
     * window; otherwise, the user has multiple monitors which are
     * presumably set up to show a single workspace, so none of them should
     * have panning enabled anyway. */
    for (int i = 0; i < res->noutput && status == RRSetConfigSuccess; i++) {
        if (!XRRSetPanning || !XRRFreePanning) {
            break;
        }
        if (!outputs[i].output) {
            continue;
        }
        XRRPanning panning = {
            .timestamp     = CurrentTime,
            .track_left    = 0,
            .track_top     = 0,
            .track_width   = 0,
            .track_height  = 0,
            .border_left   = 0,
            .border_top    = 0,
            .border_right  = 0,
            .border_bottom = 0,
        };
        if (outputs[i].output->crtc == output_info->crtc) {
            panning.left   = video_modes[mode].x;
            panning.top    = video_modes[mode].y;
            panning.width  = video_modes[mode].width;
            panning.height = video_modes[mode].height;
        } else {
            if (!outputs[i].crtc) {
                continue;
            }
            panning.left   = outputs[i].crtc->x;
            panning.top    = outputs[i].crtc->y;
            panning.width  = outputs[i].mode->width;
            panning.height = outputs[i].mode->height;
        }
        if (!save && xrandr_pan_areas) {
            int j;
            for (j = 0; j < num_xrandr_pan_areas; j++) {
                if (xrandr_pan_areas[j].output == res->outputs[i]) {
                    break;
                }
            }
            if (j < num_xrandr_pan_areas && xrandr_pan_areas[j].panning) {
                panning = *xrandr_pan_areas[j].panning;
            }
        }
        XRRSetPanning(x11_display, res, outputs[i].output->crtc, &panning);
    }

    /* We're done with our local copies of the XRandR output data. */
    for (int i = 0; i < res->noutput && status == RRSetConfigSuccess; i++) {
        if (outputs[i].output) {
            XRRFreeOutputInfo(outputs[i].output);
        }
        if (outputs[i].crtc) {
            XRRFreeCrtcInfo(outputs[i].crtc);
        }
    }
    mem_free(outputs);

    /* If we just restored the original video mode, or if we failed while
     * trying to switch away from the original video mode, free the saved
     * panning data as well. */
    if (xrandr_pan_areas
        && (!save || (status != RRSetConfigSuccess && allocated_pan_areas)))
    {
        for (int i = 0; i < num_xrandr_pan_areas; i++) {
            if (xrandr_pan_areas[i].panning) {
                XRRFreePanning(xrandr_pan_areas[i].panning);
            }
        }
        mem_free(xrandr_pan_areas);
        xrandr_pan_areas = NULL;
    }

    /* Clear saved width/height when resetting the mode so cleanup() can
     * verify that we did in fact reset the mode. */
    if (!save) {
        xrandr_original_width = 0;
        xrandr_original_height = 0;
    }

    /* All done! */
    XUngrabServer(x11_display);
    return status;
}

/*************************************************************************/
/******************* Local routines: Window management *******************/
/*************************************************************************/

static int create_window(int x, int y, int width, int height,
                         GLXFBConfig config)
{
    XSync(x11_display, False);
    (void) linux_x11_get_error();

    int force_position;
    if (x != INT_MIN && y != INT_MIN) {
        force_position = 1;
    } else {
        force_position = 0;
        /* Set a default position for the XCreateWindow() call. */
        x = video_modes[current_video_mode].x;
        y = video_modes[current_video_mode].y;
    }

    /* Create a colormap because XCreateWindow requires one even for
     * TrueColor displays.  (Why...?) */
    x11_colormap = XCreateColormap(x11_display, x11_root,
                                   x11_visual_info->visual, AllocNone);
    if (UNLIKELY(!x11_colormap)) {
        goto error_return;
    }

    /* Create the window itself. */
    XSetWindowAttributes cw_attributes = {
        .override_redirect = False,
        .background_pixmap = None,
        .border_pixel = BlackPixel(x11_display, x11_screen),
        .colormap = x11_colormap,
    };
    x11_window = XCreateWindow(
        x11_display, x11_root, x, y, width, height, 0,
        x11_visual_info->depth, InputOutput, x11_visual_info->visual,
        CWOverrideRedirect | CWBackPixmap | CWBorderPixel | CWColormap,
        &cw_attributes);
    if (UNLIKELY(!x11_window)) {
        DLOG("Failed to create X11 window");
        goto error_free_colormap;
    }

    /* Create GLX resources for the window. */
    glx_config = config;
    glx_window = glXCreateWindow(x11_display, config, x11_window, NULL);
    if (UNLIKELY(!glx_window)) {
        DLOG("Failed to create GL window object");
        goto error_destroy_x11_window;
    }
    glx_context = create_glx_context(config, True);
    if (UNLIKELY(!glx_context)) {
        DLOG("Failed to create GL context");
        goto error_destroy_glx_window;
    }
    if (UNLIKELY(!glXMakeContextCurrent(x11_display, glx_window, glx_window,
                                        glx_context))) {
        DLOG("Failed to make GL context current");
        goto error_destroy_glx_context;
    }

    /* Check for any asynchronously reported errors from X11. */
    XSync(x11_display, False);
    if (UNLIKELY(linux_x11_get_error())) {
        DLOG("X11 error occurred while creating window");
        goto error_clear_glx_context;
    }

    /* Create an input context if possible. */
    if (x11_im) {
        x11_ic = XCreateIC(x11_im,
                           XNClientWindow, x11_window,
                           XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                           NULL);
        XSync(x11_display, False);
        if (UNLIKELY(linux_x11_get_error())) {
            XDestroyIC(x11_ic);
            x11_ic = None;
        }
        if (LIKELY(x11_ic)) {
            XSetICFocus(x11_ic);
        } else {
            DLOG("Failed to create input context (continuing anyway)");
        }
    }

    /* Set up various window properties. */
    XSizeHints size_hints = {.flags = PMinSize | PMaxSize,
                             .min_width = width, .max_width = width,
                             .min_height = height, .max_height = height};
    if (!fullscreen && window_resizable) {
        size_hints = window_size_hints;
    }
    if (force_position) {
        size_hints.flags |= USPosition;
        size_hints.x = x;
        size_hints.y = y;
    }
    XWMHints wm_hints = {
        .flags = InputHint,
        .input = True,
    };
    char class_name[1000];
    get_resource_class(class_name, sizeof(class_name));
    XClassHint class_hint = {
        .res_name = class_name,
        .res_class = class_name,
    };
    Xutf8SetWMProperties(x11_display, x11_window, window_title, window_title,
                         NULL, 0, &size_hints, &wm_hints, &class_hint);
    if (wm_protocols && wm_delete_window && net_wm_ping) {
        Atom protocols[] = {
            wm_delete_window,  // Enable window deletion by the WM.
            net_wm_ping,       // Let the WM detect if we freeze.
        };
        XSetWMProtocols(x11_display, x11_window, protocols, lenof(protocols));
    }
    if (net_wm_bypass_compositor) {
        int bypass_compositor_hint = 1;  // 1 = suggest disabling compositing
        XChangeProperty(x11_display, x11_window,
                        net_wm_bypass_compositor, XA_CARDINAL, 32,
                        PropModeReplace, (void *)&bypass_compositor_hint, 1);
    }
    if (net_wm_icon && window_icon) {
        XChangeProperty(x11_display, x11_window,
                        net_wm_icon, XA_CARDINAL, 32, PropModeReplace,
                        (void *)window_icon, window_icon_len);
    }
    if (net_wm_pid) {
        pid_t pid = getpid();
        XChangeProperty(x11_display, x11_window,
                        net_wm_pid, XA_CARDINAL, 32, PropModeReplace,
                        (void *)&pid, 1);
    }
    if (net_wm_window_type && net_wm_window_type_normal) {
        XChangeProperty(x11_display, x11_window,
                        net_wm_window_type, XA_ATOM, 32, PropModeReplace,
                        (void *)&net_wm_window_type_normal, 1);
    }

    /* All done! */
    return 1;

  error_clear_glx_context:
    glXMakeContextCurrent(x11_display, None, None, None);
  error_destroy_glx_context:
    glXDestroyContext(x11_display, glx_context);
    glx_context = None;
  error_destroy_glx_window:
    glXDestroyWindow(x11_display, glx_window);
    glx_window = None;
  error_destroy_x11_window:
    XDestroyWindow(x11_display, x11_window);
    x11_window = None;
  error_free_colormap:
    XFreeColormap(x11_display, x11_colormap);
    x11_colormap = None;
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

static void close_window(void)
{
    PRECOND(x11_window, return);

    /* Save the window position if not in fullscreen, so if we're about to
     * reopen it, we can do so at the same location. */
    if (!window_fullscreen) {
        save_window_position();
    }

    opengl_cleanup();

    if (window_grabbed) {
        XUngrabPointer(x11_display, CurrentTime);
    }

    /* Unmap the window first, and wait for the unmap event to ensure that
     * the server has processed the request -- otherwise the window may be
     * left visible until the next window is opened or the program exits. */
    if (window_mapped) {
        XUnmapWindow(x11_display, x11_window);
        wait_for_event(x11_window, UnmapNotify, NULL);
    }

    if (x11_ic) {
        XDestroyIC(x11_ic);
        x11_ic = None;
    }
    glXMakeContextCurrent(x11_display, None, None, None);
    glXDestroyContext(x11_display, glx_context);
    glx_context = None;
    glXDestroyWindow(x11_display, glx_window);
    glx_window = None;
    XDestroyWindow(x11_display, x11_window);
    x11_window = None;
    XFreeColormap(x11_display, x11_colormap);
    x11_colormap = None;
    XFree(x11_visual_info);
    x11_visual_info = NULL;
}

/*-----------------------------------------------------------------------*/

static int resize_window(int width, int height)
{
    (void) linux_x11_get_error();

    if (window_fullscreen || !window_resizable) {
        XSizeHints size_hints = {
            .flags = PMinSize | PMaxSize,
            .min_width = width,
            .max_width = width,
            .min_height = height,
            .max_height = height,
        };
        XSetWMNormalHints(x11_display, x11_window, &size_hints);
    }
    if (center_window) {
        int x, y;
        x = video_modes[current_video_mode].x
            + (video_modes[current_video_mode].width - width) / 2;
        y = video_modes[current_video_mode].y
            + (video_modes[current_video_mode].height - height) / 2;
        XMoveResizeWindow(x11_display, x11_window, x, y, width, height);
    } else {
        XResizeWindow(x11_display, x11_window, width, height);
    }

    XSync(x11_display, False);
    if (UNLIKELY(linux_x11_get_error())) {
        DLOG("Failed to resize window to %dx%d", width, height);
        return 0;
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

static void save_window_position(void)
{
    PRECOND(x11_window);

    /* We can't simply call XTranslateCoordinates on our window, because
     * with most window managers, our window will be contained in a
     * decoration window, and it's the coordinates of that window which we
     * need for XCreateWindow()/XMoveWindow() calls. */
    Window container;
    for (Window parent = x11_window; parent != None && parent != x11_root; ) {
        container = parent;
        Window *children;  // Not needed, but we have to free it manually.
        if (UNLIKELY(!XQueryTree(x11_display, container, (Window[1]){0},
                                 &parent, &children, (unsigned int[1]){0}))) {
            XSync(x11_display, False);
            DLOG("Unexpected XQueryTree() failure");
            break;
        }
        if (children) {
            XFree(children);
        }
    }
    ASSERT(XTranslateCoordinates(x11_display, container, x11_root, 0, 0,
                                 &window_last_x, &window_last_y,
                                 (Window[1]){0}));
}

/*-----------------------------------------------------------------------*/

static void scroll_to_window(int pointer_x, int pointer_y)
{
    /* Make sure all pointer motion events here are unique, so the
     * position override below doesn't get cancelled early. */
    if (pointer_x != window_width-1 || pointer_y != window_height-1) {
        XWarpPointer(x11_display, None, x11_window, 0, 0, 0, 0,
                     window_width-1, window_height-1);
        /* Flush to avoid libX11 merging the warp calls. */
        XFlush(x11_display);
    }
    XWarpPointer(x11_display, None, x11_window, 0, 0, 0, 0, 0, 0);
    XFlush(x11_display);
    if (pointer_x != 0 || pointer_y != 0) {
        XWarpPointer(x11_display, None, x11_window, 0, 0, 0, 0,
                     pointer_x, pointer_y);
        XFlush(x11_display);
    }
    linux_override_mouse_position(pointer_x, pointer_y);
}

/*-----------------------------------------------------------------------*/

static void set_window_border(Window window, int border)
{
    PRECOND(window, return);

    if (!force_XSetTransientForHint && motif_wm_hints) {
        struct {
            unsigned long flags, functions, decorations;
            long input_mode;
            unsigned long status;
        } hints = {.flags = 1 << 1, .functions = 0,
                   .decorations = border ? 1 : 0, .input_mode = 0,
                   .status = 0};
        XChangeProperty(x11_display, window, motif_wm_hints,
                        motif_wm_hints, 32, PropModeReplace, (void *)&hints,
                        sizeof(hints) / sizeof(long));
    } else {
        /* Use the "transient window" hint to try and toggle borders. */
        XSetTransientForHint(x11_display, window, border ? None : x11_root);
    }
}

/*-----------------------------------------------------------------------*/

static int set_window_fullscreen(int full, int is_new, int width, int height)
{
    if ((full != 0) == window_fullscreen) {
        return 1;  // Nothing to do.
    }

    /* Save the current pointer position within the window so we can
     * restore it after the window is (potentially) moved. */
    int pointer_x = 0, pointer_y = 0;
    {
        Window unused_r, unused_c;
        int unused_rx, unused_ry;
        unsigned int unused_mask;
        XQueryPointer(x11_display, x11_window, &unused_r, &unused_c,
                      &unused_rx, &unused_ry, &pointer_x, &pointer_y,
                      &unused_mask);
    }
    pointer_x = bound(pointer_x, 0, window_width-1);
    pointer_y = bound(pointer_y, 0, window_height-1);

    /* If a window is already open and we're changing to fullscreen mode,
     * save the window's position for possibly changing back later. */
    if (!is_new && full) {
        save_window_position();
    }

    (void) linux_x11_get_error();

    /*
     * Some window managers (at least FVWM 2.6.5 and Marco 1.8.0) disallow
     * the EWMH fullscreen action if the window is not resizable, even if
     * no actual resize would take place.  To placate these WMs, we clear
     * the min/max size settings on the window while in fullscreen mode
     * when using the EWMH method (but see the ewmh_fullscreen_resize_after
     * test below for an exception).
     *
     * Note that if we checked _NET_WM_ALLOWED_ACTIONS here, we could be
     * tripped up by some WMs (notably FVWM) which fail to update the
     * allowed action list when the size hints change, so we'd have to
     * wait until after mapping the window to set the size hints.
     */
    if (fullscreen_method == EWMH_FULLSCREEN) {
        if (full) {
            XSizeHints size_hints = {.flags = 0};
            XSetWMNormalHints(x11_display, x11_window, &size_hints);
        }
    }

    /* If we're in a multi-head, single-X11-screen setup and we're using
     * the EWMH method, we need to tell the WM which monitor to use for
     * fullscreen positioning. */
    if (full && fullscreen_method == EWMH_FULLSCREEN
        && ((video_mode_interface == XRANDR
             && x11_num_screens > 1
             && ScreenCount(x11_display) == 1)
            || use_xinerama))
    {
        int fullscreen_monitor;
        if (use_xinerama) {
            fullscreen_monitor = video_modes[current_video_mode].screen;
        } else {
            ASSERT(video_mode_interface == XRANDR);
            fullscreen_monitor =
                video_modes[current_video_mode].xrandr_info.xinerama_screen;
        }
        /* Some WMs don't support _NET_WM_FULLSCREEN_MONITORS but can still
         * be convinced to put the window on the proper monitor by moving
         * the window to that monitor in advance. */
        if (ewmh_fullscreen_move_before) {
            const int x = video_modes[current_video_mode].x;
            const int y = video_modes[current_video_mode].y;
            XMoveWindow(x11_display, x11_window, x, y);
        } else {
            XEvent event;
            mem_clear(&event, sizeof(event));
            event.type = ClientMessage;
            event.xclient.display = x11_display;
            event.xclient.window = x11_window;
            event.xclient.message_type = net_wm_fullscreen_monitors;
            event.xclient.format = 32;
            event.xclient.data.l[0] = fullscreen_monitor;
            event.xclient.data.l[1] = fullscreen_monitor;
            event.xclient.data.l[2] = fullscreen_monitor;
            event.xclient.data.l[3] = fullscreen_monitor;
            event.xclient.data.l[4] = 1;
            XSendEvent(x11_display, x11_root, False,
                       SubstructureRedirectMask | SubstructureNotifyMask,
                       &event);
        }
    }

    /* Determine where to place the window after the switch. */
    int new_x, new_y;
    if (full) {
        new_x = video_modes[current_video_mode].x;
        new_y = video_modes[current_video_mode].y;
    } else if (window_last_x != INT_MIN && window_last_y != INT_MIN) {
        new_x = window_last_x;
        new_y = window_last_y;
    } else {
        /* Default to centering the window when switching out of fullscreen
         * mode.  The WM may decide to place the window on its own, which
         * is fine too. */
        new_x = video_modes[current_video_mode].x
            + (video_modes[current_video_mode].width - width) / 2;
        new_y = video_modes[current_video_mode].y
            + (video_modes[current_video_mode].height - height) / 2;
        if (use_xinerama) {
            /* Use Xinerama instead of the mode table since the default
             * mode may be a multi-monitor one. */
            int num_screens;
            XineramaScreenInfo *screen_info =
                XineramaQueryScreens(x11_display, &num_screens);
            if (screen_info) {
                int screen = real_screen;
                ASSERT(screen < num_screens, screen = 0);
                new_x = screen_info[screen].x_org
                    + (screen_info[screen].width - width) / 2;
                new_y = screen_info[screen].y_org
                    + (screen_info[screen].height - height) / 2;
                XFree(screen_info);
            }
        }
    }

    /* Do the actual fullscreen/window switch. */
    if (is_new) {

        ASSERT(full, return 0);
        if (fullscreen_method == EWMH_FULLSCREEN) {
            XChangeProperty(x11_display, x11_window,
                            net_wm_state, XA_ATOM, 32, PropModeReplace,
                            (void *)&net_wm_state_fullscreen, 1);
        } else {
            set_window_border(x11_window, 0);
        }

    } else if (fullscreen_method == EWMH_FULLSCREEN) {

        /* In case we're in a low-resolution mode, warp to the upper-left
         * corner of the screen to try and help ensure that the window goes
         * in the expected place. */
        const int x = video_modes[current_video_mode].x;
        const int y = video_modes[current_video_mode].y;
        XWarpPointer(x11_display, None, x11_root, 0, 0, 0, 0, x, y);

        XEvent event;
        mem_clear(&event, sizeof(event));
        event.type = ClientMessage;
        event.xclient.display = x11_display;
        event.xclient.window = x11_window;
        event.xclient.message_type = net_wm_state;
        event.xclient.format = 32;
        event.xclient.data.l[0] = full ? net_wm_state_add : net_wm_state_remove;
        event.xclient.data.l[1] = net_wm_state_fullscreen;
        event.xclient.data.l[3] = 1;
        XSendEvent(x11_display, x11_root, False,
                   SubstructureRedirectMask | SubstructureNotifyMask, &event);

        /* Wait for the WM to process our fullscreen request. */
        do {
            wait_for_event(x11_window, PropertyNotify, &event);
        } while (event.xproperty.atom != net_wm_state);

    } else {  // !is_new && fullscreen_method != EWMH_FULLSCREEN

        if (full) {
            set_window_border(x11_window, 0);
            /* Unmap and remap the window because some WMs only change
             * decoration state at map time. */
            XUnmapWindow(x11_display, x11_window);
            XMoveWindow(x11_display, x11_window, new_x, new_y);
            XMapRaised(x11_display, x11_window);
        } else {  // !full
            /* Unmap and remap the window for two reasons: it gives the WM
             * a chance to place the window in its previous (or some other
             * reasonable) location, and it provides an event we can wait
             * for -- XMoveWindow() by itself might not generate a
             * ConfigureNotify event (if the window is already at the
             * target location, or if the WM rejects the move request),
             * and then we'd be stuck waiting forever. */
            XUnmapWindow(x11_display, x11_window);
            /* This wait is required to avoid locking up under JWM. */
            if (window_mapped) {
                wait_for_event(x11_window, UnmapNotify, NULL);
                window_mapped = 0;
            }
            XMoveWindow(x11_display, x11_window, new_x, new_y);
            set_window_border(x11_window, 1);
            XMapRaised(x11_display, x11_window);
        }  // if (full)

        /* Make sure the window is visible before we continue.  We check
         * for MapNotify before VisibilityNotify in case there were any
         * intermediate visibility events. */
        window_mapped = 0;
        while (!window_mapped) {
            XEvent event;
            XWindowEvent(x11_display, x11_window,
                         StructureNotifyMask | VisibilityChangeMask, &event);
            if (event.type == MapNotify) {
                window_mapped = 1;
            }  // Else it's UnmapNotify or VisibilityNotify, so discard.
        }
        wait_for_event(x11_window, VisibilityNotify, NULL);

    }  // if (can_wm_fullscreen)

    /* Undo the EWMH fullscreen resize hack, if appropriate.  If we went
     * fullscreen via the WM, we normally leave the hint unset until we
     * return to windowed mode because re-enabling it right away will cause
     * some WMs (at least Marco 1.8.0) to fail to reposition the window.
     * On the flip side, some window managers size the window based on the
     * original screen size rather than the current size, so we undo the
     * hack here and force the window back to the proper size.
     *
     * If the hack was not needed but a window resize has been requested,
     * perform that here as well.
     */
    if (fullscreen_method == EWMH_FULLSCREEN
     && (!full || ewmh_fullscreen_resize_after)) {
        if (full || !window_resizable) {
            XSizeHints size_hints = {
                .flags = PMinSize | PMaxSize,
                .min_width = width,
                .max_width = width,
                .min_height = height,
                .max_height = height,
            };
            XSetWMNormalHints(x11_display, x11_window, &size_hints);
        } else {
            XSetWMNormalHints(x11_display, x11_window, &window_size_hints);
        }
        /* Make sure the window is still the correct size and in the
         * correct position. */
        XMoveResizeWindow(x11_display, x11_window,
                          new_x, new_y, width, height);
    } else {  // Hack was not used; check for resize.
        if (!full && (width != window_width || height != window_height)) {
            if (window_resizable) {
                XSetWMNormalHints(x11_display, x11_window, &window_size_hints);
            } else {
                XSizeHints size_hints = {
                    .flags = PMinSize | PMaxSize,
                    .min_width = width,
                    .max_width = width,
                    .min_height = height,
                    .max_height = height,
                };
                XSetWMNormalHints(x11_display, x11_window, &size_hints);
            }
            XResizeWindow(x11_display, x11_window, width, height);
        }
    }

    XSync(x11_display, False);
    if (UNLIKELY(linux_x11_get_error())) {
        DLOG("Failed to set window fullscreen state: %d, %d, %dx%d",
             full, is_new, width, height);
        return 0;
    }

    /* Restore the original pointer position.  If entering fullscreen mode,
     * rather than just setting the pointer position, we warp the pointer
     * to the upper-left and lower-right corners of the window to ensure
     * that the window is fully displayed on the screen (in case we've
     * switched to a lower resolution than the root window size). */
    if (full) {
        scroll_to_window(pointer_x, pointer_y);
    } else {  // !full
        XWarpPointer(x11_display, None, x11_window, 0, 0, 0, 0,
                     pointer_x ? pointer_x-1 : 1, pointer_y ? pointer_y-1 : 1);
        XFlush(x11_display);
        XWarpPointer(x11_display, None, x11_window, 0, 0, 0, 0,
                     pointer_x, pointer_y);
        XFlush(x11_display);
        linux_override_mouse_position(pointer_x, pointer_y);
    }

    window_fullscreen = (full != 0);
    linux_set_window_grab(grab_requested);

    return 1;
}

/*-----------------------------------------------------------------------*/

static int should_minimize_fullscreen(void)
{
    ASSERT(x11_window != None, return 0);

    if (time_now() < minimize_fs_timeout) {
        return 0;
    }

    if (minimize_fullscreen >= 0) {
        return minimize_fullscreen;
    }

    /* If the SDL hint variable is present, use it to override default
     * behavior. */
    const char *sdl_hint = getenv("SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS");
    if (sdl_hint && *sdl_hint) {
        return strcmp(sdl_hint, "0") != 0 && stricmp(sdl_hint, "false") != 0;
    }

    /* Otherwise, minimize only if in a non-default video mode. */
    return current_video_mode != original_video_mode[x11_screen];
}

/*************************************************************************/
/******************* Local routines: OpenGL management *******************/
/*************************************************************************/

static int setup_opengl(int width, int height)
{
    const uint32_t gl_flags = OPENGL_FEATURE_FAST_STATIC_VBO
                            | OPENGL_FEATURE_FAST_DYNAMIC_VBO
                            | OPENGL_FEATURE_NATIVE_QUADS
                            | OPENGL_FEATURE_GENERATEMIPMAP;
    if (UNLIKELY(!opengl_init(width, height, gl_flags))) {
        DLOG("Failed to set up OpenGL!");
        return 0;
    }

    opengl_set_compile_context_callback(create_gl_shader_compilation_context);

    return 1;
}

/*-----------------------------------------------------------------------*/

static GLXContext create_glx_context(GLXFBConfig config, Bool direct)
{
    /* HACK: glXCreateNewContext() fails on at least the NVIDIA proprietary
     * driver if direct is false, so force it on; this should be harmless,
     * since we won't actually render anything if direct is false. */
    direct = True;

    if (p_glXCreateContextAttribsARB) {
        int attribs[11];
        int index = 0;
        attribs[index++] = GLX_RENDER_TYPE;
        attribs[index++] = GLX_RGBA_TYPE;
        if (desired_opengl_major >= 3) {
            attribs[index++] = GLX_CONTEXT_MAJOR_VERSION_ARB;
            attribs[index++] = desired_opengl_major;
            attribs[index++] = GLX_CONTEXT_MINOR_VERSION_ARB;
            attribs[index++] = desired_opengl_minor;
            attribs[index++] = GLX_CONTEXT_PROFILE_MASK_ARB;
            attribs[index++] = GLX_CONTEXT_CORE_PROFILE_BIT_ARB;
        }
        if (use_opengl_debug) {
            attribs[index++] = GLX_CONTEXT_FLAGS_ARB;
            attribs[index++] = GLX_CONTEXT_DEBUG_BIT_ARB;
        }
        ASSERT(index < lenof(attribs), return 0);
        attribs[index] = None;
        return (*p_glXCreateContextAttribsARB)(
            x11_display, config, None, direct, attribs);
    } else {
        /* If OpenGL 3.0+ is requested, assume it's not available due to
         * lack of glXCreateContextAttribsARB(). */
        if (desired_opengl_major >= 3) {
            DLOG("glXCreateContextAttribsARB() not available, can't create"
                 " OpenGL %d.%d context", desired_opengl_major,
                 desired_opengl_minor);
            return None;
        }
        return glXCreateNewContext(x11_display, config,
                                   GLX_RGBA_TYPE, None, direct);
    }
}

/*-----------------------------------------------------------------------*/

static int create_gl_shader_compilation_context(void)
{
    if (glXGetCurrentContext()) {
        return 1;
    }

    if (!glx_window) {
        DLOG("No window open, can't create a shader compilation context");
        return 0;
    }
    GLXContext context = create_glx_context(glx_config, False);
    if (UNLIKELY(!context)) {
        DLOG("Failed to create shader compilation context");
        return 0;
    }
    /* We don't actually need to draw to the window for this context, but
     * we have to give glXMakeContextCurrent() _some_ valid GLXDrawable or
     * it will fail. */
    if (UNLIKELY(!glXMakeContextCurrent(x11_display, glx_window, glx_window,
                                        context))) {
        DLOG("Failed to activate shader compilation context");
        glXDestroyContext(x11_display, context);
        return 0;
    }

    /* Destroy the context now so we don't leak it when the thread exits. */
    glXDestroyContext(x11_display, context);

    return 1;
}

/*-----------------------------------------------------------------------*/

static int glx_has_extension(int screen, const char *name)
{
    const char *extensions = glXQueryExtensionsString(x11_display, screen);
    ASSERT(extensions, extensions = "");
    const int namelen = strlen(name);
    const char *s = extensions;
    while ((s = strstr(s, name)) != NULL) {
        if ((s == extensions || s[-1] == ' ')
         && (s[namelen] == 0 || s[namelen] == ' ')) {
            return 1;
        }
        s += namelen;
        s += strcspn(s, " ");
    }
    return 0;
}

/*-----------------------------------------------------------------------*/

static int glx_choose_config(int screen, GLXFBConfig *config_ret)
{
    PRECOND(config_ret != NULL, return 0);

    /* Pick a GL framebuffer configuration. */
    int attributes[] = {
        GLX_DOUBLEBUFFER,   True,
        GLX_RED_SIZE,       8,
        GLX_GREEN_SIZE,     8,
        GLX_BLUE_SIZE,      8,
        GLX_DEPTH_SIZE,     depth_bits,
        GLX_STENCIL_SIZE,   stencil_bits,
        GLX_SAMPLE_BUFFERS, multisample,
        GLX_SAMPLES,        multisample ? multisample_samples
                                        : (int)(GLX_DONT_CARE),
        GLX_X_VISUAL_TYPE,  GLX_TRUE_COLOR,
        None
    };
    int num_configs;
    GLXFBConfig *configs = glXChooseFBConfig(x11_display, screen,
                                             attributes, &num_configs);
    if (!configs || !num_configs) {
        DLOG("Couldn't find any matching GLX configs");
        return 0;
    }

    /* Return the first ("best" by the GLX sort order) matching
     * configuration. */
    *config_ret = configs[0];
    XFree(configs);
    return 1;
}

/*-----------------------------------------------------------------------*/

static void *linux_glXGetProcAddress(const char *name)
{
    if (p_glXGetProcAddress) {
        return (*p_glXGetProcAddress)((const GLubyte *)name);
    } else {
        return dlsym(RTLD_DEFAULT, name);
    }
}

/*************************************************************************/
/*************************************************************************/
