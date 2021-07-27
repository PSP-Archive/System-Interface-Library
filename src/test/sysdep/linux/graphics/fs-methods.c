/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/linux/graphics/fs-methods.c: Linux-specific graphics
 * tests covering the various methods for entering fullscreen mode.
 */

#include "src/base.h"
#include "src/graphics.h"
#include "src/sysdep.h"
#include "src/sysdep/linux/internal.h"
#include "src/test/base.h"
#include "src/test/graphics/internal.h"
#include "src/test/sysdep/linux/graphics/internal.h"
#include "src/test/sysdep/linux/wrap-x11.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xinerama.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/xf86vmode.h>

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_linux_graphics_fs_methods)

/*-----------------------------------------------------------------------*/

TEST_INIT(init)
{
    clear_variables();
    if (!strstr(CURRENT_TEST_NAME(), "_NOINIT")) {
        CHECK_TRUE(graphics_init());
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
    if (!strstr(CURRENT_TEST_NAME(), "_NOCLEANUP")) {
        graphics_cleanup();
    }
    CHECK_INTEQUAL(linux_x11_get_error(), 0);
    clear_variables();
    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_env_fullscreen_method_XMoveWindow)
{
    Display *display = linux_x11_display();
    Atom motif_wm_hints = XInternAtom(display, "_MOTIF_WM_HINTS", True);

    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop;
    Status result;

    ASSERT(setenv("SIL_X11_FULLSCREEN_METHOD", "XMOVEWINDOW", 1) == 0);
    ASSERT(setenv("SIL_X11_CREATE_FULLSCREEN", "1", 1) == 0);

    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(graphics_device_width(),
                                         graphics_device_height(), NULL));
    Window window = linux_x11_window();
    CHECK_INTEQUAL(called_XMoveWindow, 0);
    if (motif_wm_hints) {
        result = XGetWindowProperty(display, window, motif_wm_hints, 0, 5,
                                    False, AnyPropertyType, &actual_type,
                                    &actual_format, &nitems, &bytes_after,
                                    &prop);
        CHECK_INTEQUAL(result, Success);
        CHECK_INTEQUAL(actual_type, motif_wm_hints);
        CHECK_INTEQUAL(actual_format, 32);
        CHECK_INTEQUAL(nitems, 5);
        CHECK_INTEQUAL(bytes_after, 0);
        CHECK_INTEQUAL(((long *)(void *)prop)[0], 1 << 1);
        CHECK_INTEQUAL(((long *)(void *)prop)[1], 0);
        CHECK_INTEQUAL(((long *)(void *)prop)[2], 0);
        CHECK_INTEQUAL(((long *)(void *)prop)[3], 0);
        CHECK_INTEQUAL(((long *)(void *)prop)[4], 0);
        XFree(prop);
    } else {
        result = XGetWindowProperty(display, window, XA_WM_TRANSIENT_FOR, 0, 1,
                                    False, AnyPropertyType, &actual_type,
                                    &actual_format, &nitems, &bytes_after,
                                    &prop);
        CHECK_INTEQUAL(result, Success);
        CHECK_INTEQUAL(actual_type, XA_WINDOW);
        CHECK_INTEQUAL(actual_format, 32);
        CHECK_INTEQUAL(nitems, 1);
        CHECK_INTEQUAL(bytes_after, 0);
        CHECK_INTEQUAL(*(long *)(void *)prop,
                       RootWindow(display, linux_x11_screen()));
        XFree(prop);
    }

    called_XCreateWindow = 0;
    CHECK_TRUE(graphics_set_display_attr("window", 1));
    CHECK_TRUE(graphics_set_display_mode(graphics_device_width(),
                                         graphics_device_height(), NULL));
    CHECK_INTEQUAL(called_XCreateWindow, 0);
    CHECK_INTEQUAL(called_XMoveWindow, 1);
    if (motif_wm_hints) {
        result = XGetWindowProperty(display, window, motif_wm_hints, 0, 5,
                                    False, AnyPropertyType, &actual_type,
                                    &actual_format, &nitems, &bytes_after,
                                    &prop);
        CHECK_INTEQUAL(result, Success);
        CHECK_INTEQUAL(actual_type, motif_wm_hints);
        CHECK_INTEQUAL(actual_format, 32);
        CHECK_INTEQUAL(nitems, 5);
        CHECK_INTEQUAL(bytes_after, 0);
        CHECK_INTEQUAL(((long *)(void *)prop)[0], 1 << 1);
        CHECK_INTEQUAL(((long *)(void *)prop)[1], 0);
        CHECK_INTEQUAL(((long *)(void *)prop)[2], 1);
        CHECK_INTEQUAL(((long *)(void *)prop)[3], 0);
        CHECK_INTEQUAL(((long *)(void *)prop)[4], 0);
        XFree(prop);
    } else {
        result = XGetWindowProperty(display, window, XA_WM_TRANSIENT_FOR, 0, 1,
                                    False, AnyPropertyType, &actual_type,
                                    &actual_format, &nitems, &bytes_after,
                                    &prop);
        CHECK_TRUE(result != Success || actual_type == 0 || *(long *)(void *)prop == 0);
    }

    called_XCreateWindow = 0;
    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(graphics_device_width(),
                                         graphics_device_height(), NULL));
    CHECK_INTEQUAL(called_XCreateWindow, 0);
    CHECK_INTEQUAL(called_XMoveWindow, 2);
    if (motif_wm_hints) {
        result = XGetWindowProperty(display, window, motif_wm_hints, 0, 5,
                                    False, AnyPropertyType, &actual_type,
                                    &actual_format, &nitems, &bytes_after,
                                    &prop);
        CHECK_INTEQUAL(result, Success);
        CHECK_INTEQUAL(actual_type, motif_wm_hints);
        CHECK_INTEQUAL(actual_format, 32);
        CHECK_INTEQUAL(nitems, 5);
        CHECK_INTEQUAL(bytes_after, 0);
        CHECK_INTEQUAL(((long *)(void *)prop)[0], 1 << 1);
        CHECK_INTEQUAL(((long *)(void *)prop)[1], 0);
        CHECK_INTEQUAL(((long *)(void *)prop)[2], 0);
        CHECK_INTEQUAL(((long *)(void *)prop)[3], 0);
        CHECK_INTEQUAL(((long *)(void *)prop)[4], 0);
        XFree(prop);
    } else {
        result = XGetWindowProperty(display, window, XA_WM_TRANSIENT_FOR, 0, 1,
                                    False, AnyPropertyType, &actual_type,
                                    &actual_format, &nitems, &bytes_after,
                                    &prop);
        CHECK_INTEQUAL(result, Success);
        CHECK_INTEQUAL(actual_type, XA_WINDOW);
        CHECK_INTEQUAL(actual_format, 32);
        CHECK_INTEQUAL(nitems, 1);
        CHECK_INTEQUAL(bytes_after, 0);
        CHECK_INTEQUAL(*(long *)(void *)prop,
                       RootWindow(display, linux_x11_screen()));
        XFree(prop);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_env_fullscreen_method_ewmh_fullscreen)
{
    Display *display = linux_x11_display();
    Atom motif_wm_hints = XInternAtom(display, "_MOTIF_WM_HINTS", True);
    Atom net_wm_state = XInternAtom(display, "_NET_WM_STATE", True);
    const long net_wm_state_add = 1;
    const long net_wm_state_remove = 0;
    Atom net_wm_state_fullscreen =
        XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", True);
    if (!net_wm_state || !net_wm_state_fullscreen) {
        SKIP("_NET_WM_STATE or _NET_WM_STATE_FULLSCREEN atom not found.");
    }

    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop;
    Status result;

    ASSERT(setenv("SIL_X11_FULLSCREEN_METHOD", "EWMH_FULLSCREEN", 1) == 0);
    ASSERT(setenv("SIL_X11_CREATE_FULLSCREEN", "1", 1) == 0);

    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(graphics_device_width(),
                                         graphics_device_height(), NULL));
    Window window = linux_x11_window();
    Window root = RootWindow(display, linux_x11_screen());
    result = XGetWindowProperty(display, window, net_wm_state, 0, 1, False,
                                AnyPropertyType, &actual_type, &actual_format,
                                &nitems, &bytes_after, &prop);
    CHECK_TRUE(result == Success);
    CHECK_INTEQUAL(actual_type, XA_ATOM);
    CHECK_INTEQUAL(actual_format, 32);
    CHECK_INTEQUAL(nitems, 1);
    CHECK_INTEQUAL(bytes_after, 0);
    CHECK_INTEQUAL(*(long *)(void *)prop, net_wm_state_fullscreen);
    XFree(prop);
    if (motif_wm_hints) {
        result = XGetWindowProperty(display, window, motif_wm_hints, 0, 5,
                                    False, AnyPropertyType, &actual_type,
                                    &actual_format, &nitems, &bytes_after,
                                    &prop);
        CHECK_TRUE(result != Success || actual_type == 0);
    }
    result = XGetWindowProperty(display, window, XA_WM_TRANSIENT_FOR, 0, 1,
                                False, AnyPropertyType, &actual_type,
                                &actual_format, &nitems, &bytes_after, &prop);
    CHECK_TRUE(result != Success || actual_type == 0);

    called_XCreateWindow = 0;
    CHECK_TRUE(graphics_set_display_attr("window", 1));
    CHECK_TRUE(graphics_set_display_mode(graphics_device_width(),
                                         graphics_device_height(), NULL));
    CHECK_INTEQUAL(called_XCreateWindow, 0);
    CHECK_PTREQUAL(last_event_display, display);
    CHECK_INTEQUAL(last_event_window, root);
    CHECK_FALSE(last_event_propagate);
    CHECK_INTEQUAL(last_event_mask,
                   SubstructureNotifyMask | SubstructureRedirectMask);
    CHECK_INTEQUAL(last_event.type, ClientMessage);
    CHECK_PTREQUAL(last_event.xclient.display, display);
    CHECK_INTEQUAL(last_event.xclient.window, window);
    CHECK_INTEQUAL(last_event.xclient.message_type, net_wm_state);
    CHECK_INTEQUAL(last_event.xclient.format, 32);
    CHECK_INTEQUAL(last_event.xclient.data.l[0], net_wm_state_remove);
    CHECK_INTEQUAL(last_event.xclient.data.l[1], net_wm_state_fullscreen);
    CHECK_INTEQUAL(last_event.xclient.data.l[2], 0);
    CHECK_INTEQUAL(last_event.xclient.data.l[3], 1);
    if (motif_wm_hints) {
        result = XGetWindowProperty(display, window, motif_wm_hints, 0, 5,
                                    False, AnyPropertyType, &actual_type,
                                    &actual_format, &nitems, &bytes_after,
                                    &prop);
        CHECK_TRUE(result != Success || actual_type == 0);
    }
    result = XGetWindowProperty(display, window, XA_WM_TRANSIENT_FOR, 0, 1,
                                False, AnyPropertyType, &actual_type,
                                &actual_format, &nitems, &bytes_after, &prop);
    CHECK_TRUE(result != Success || actual_type == 0);

    called_XCreateWindow = 0;
    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(graphics_device_width(),
                                         graphics_device_height(), NULL));
    CHECK_INTEQUAL(called_XCreateWindow, 0);
    CHECK_PTREQUAL(last_event_display, display);
    CHECK_INTEQUAL(last_event_window, root);
    CHECK_FALSE(last_event_propagate);
    CHECK_INTEQUAL(last_event_mask,
                   SubstructureNotifyMask | SubstructureRedirectMask);
    CHECK_INTEQUAL(last_event.type, ClientMessage);
    CHECK_PTREQUAL(last_event.xclient.display, display);
    CHECK_INTEQUAL(last_event.xclient.window, window);
    CHECK_INTEQUAL(last_event.xclient.message_type, net_wm_state);
    CHECK_INTEQUAL(last_event.xclient.format, 32);
    CHECK_INTEQUAL(last_event.xclient.data.l[0], net_wm_state_add);
    CHECK_INTEQUAL(last_event.xclient.data.l[1], net_wm_state_fullscreen);
    CHECK_INTEQUAL(last_event.xclient.data.l[2], 0);
    CHECK_INTEQUAL(last_event.xclient.data.l[3], 1);
    if (motif_wm_hints) {
        result = XGetWindowProperty(display, window, motif_wm_hints, 0, 5,
                                    False, AnyPropertyType, &actual_type,
                                    &actual_format, &nitems, &bytes_after,
                                    &prop);
        CHECK_TRUE(result != Success || actual_type == 0);
    }
    result = XGetWindowProperty(display, window, XA_WM_TRANSIENT_FOR, 0, 1,
                                False, AnyPropertyType, &actual_type,
                                &actual_format, &nitems, &bytes_after, &prop);
    CHECK_TRUE(result != Success || actual_type == 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_env_fullscreen_method_invalid_empty)
{
    ASSERT(!getenv("SIL_X11_FULLSCREEN_METHOD"));

    Display *display = linux_x11_display();
    Atom motif_wm_hints = XInternAtom(display, "_MOTIF_WM_HINTS", True);
    Atom net_wm_state = XInternAtom(display, "_NET_WM_STATE", True);
    Atom net_wm_state_fullscreen =
        XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", True);

    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop;
    Status result;

    ASSERT(setenv("SIL_X11_CREATE_FULLSCREEN", "1", 1) == 0);
    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(graphics_device_width(),
                                         graphics_device_height(), NULL));
    int is_ewmh_fullscreen = 0;
    if (net_wm_state && net_wm_state_fullscreen) {
        Window window = linux_x11_window();
        result = XGetWindowProperty(display, window, net_wm_state, 0, 1,
                                    False, AnyPropertyType, &actual_type,
                                    &actual_format, &nitems, &bytes_after,
                                    &prop);
        if (result == Success && actual_type != 0) {
            is_ewmh_fullscreen = 1;
            XFree(prop);
        }
    }

    for (int do_empty = 0; do_empty < 2; do_empty++) {
        ASSERT(setenv("SIL_X11_FULLSCREEN_METHOD",
                      do_empty ? "" : "foo", 1) == 0);

        linux_close_window();
        CHECK_TRUE(graphics_set_display_mode(graphics_device_width(),
                                             graphics_device_height(), NULL));
        Window window = linux_x11_window();
        if (is_ewmh_fullscreen) {
            result = XGetWindowProperty(display, window, net_wm_state, 0, 1,
                                        False, AnyPropertyType, &actual_type,
                                        &actual_format, &nitems, &bytes_after,
                                        &prop);
            CHECK_TRUE(result == Success);
            CHECK_INTEQUAL(actual_type, XA_ATOM);
            CHECK_INTEQUAL(actual_format, 32);
            CHECK_INTEQUAL(nitems, 1);
            CHECK_INTEQUAL(bytes_after, 0);
            CHECK_INTEQUAL(*(long *)(void *)prop, net_wm_state_fullscreen);
            XFree(prop);
            if (motif_wm_hints) {
                result = XGetWindowProperty(display, window, motif_wm_hints,
                                            0, 5, False, AnyPropertyType,
                                            &actual_type, &actual_format,
                                            &nitems, &bytes_after, &prop);
                CHECK_TRUE(result != Success || actual_type == 0);
            }
            result = XGetWindowProperty(display, window, XA_WM_TRANSIENT_FOR,
                                        0, 1, False, AnyPropertyType,
                                        &actual_type, &actual_format, &nitems,
                                        &bytes_after, &prop);
            CHECK_TRUE(result != Success || actual_type == 0);
        } else {  // !ewmh_fullscreen
            if (motif_wm_hints) {
                result = XGetWindowProperty(display, window, motif_wm_hints,
                                            0, 5, False, AnyPropertyType,
                                            &actual_type, &actual_format,
                                            &nitems, &bytes_after, &prop);
                CHECK_INTEQUAL(result, Success);
                CHECK_INTEQUAL(actual_type, motif_wm_hints);
                CHECK_INTEQUAL(actual_format, 32);
                CHECK_INTEQUAL(nitems, 5);
                CHECK_INTEQUAL(bytes_after, 0);
                CHECK_INTEQUAL(((long *)(void *)prop)[0], 1 << 1);
                CHECK_INTEQUAL(((long *)(void *)prop)[1], 0);
                CHECK_INTEQUAL(((long *)(void *)prop)[2], 0);
                CHECK_INTEQUAL(((long *)(void *)prop)[3], 0);
                CHECK_INTEQUAL(((long *)(void *)prop)[4], 0);
            } else {
                result = XGetWindowProperty(
                    display, window, XA_WM_TRANSIENT_FOR, 0, 1, False,
                    AnyPropertyType, &actual_type, &actual_format, &nitems,
                    &bytes_after, &prop);
                CHECK_INTEQUAL(result, Success);
                CHECK_INTEQUAL(actual_type, XA_WINDOW);
                CHECK_INTEQUAL(actual_format, 32);
                CHECK_INTEQUAL(nitems, 1);
                CHECK_INTEQUAL(bytes_after, 0);
                CHECK_INTEQUAL(*(long *)(void *)prop,
                               RootWindow(display, linux_x11_screen()));
            }
        }
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_env_move_before_true_vidmode_NOINIT)
{
    disable_XRRQueryExtension = 1;
    CHECK_TRUE(graphics_init());
    disable_XRRQueryExtension = 0;

    if (!check_vidmode()) {
        SKIP("XF86VidMode not found.");
    }

    Display *display = linux_x11_display();
    Atom net_wm_state = XInternAtom(display, "_NET_WM_STATE", True);
    Atom net_wm_state_fullscreen =
        XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", True);
    if (!net_wm_state || !net_wm_state_fullscreen) {
        SKIP("_NET_WM_STATE or _NET_WM_STATE_FULLSCREEN atom not found.");
    }

    ASSERT(setenv("SIL_X11_FULLSCREEN_METHOD", "EWMH_FULLSCREEN", 1) == 0);
    /* We set CREATE_FULLSCREEN=1 for these tests to avoid a _NET_WM_STATE
     * message being sent to the root window and overwriting the saved
     * event (or lack thereof) from setting _NET_WM_FULLSCREEN_MONITORS. */
    ASSERT(setenv("SIL_X11_CREATE_FULLSCREEN", "1", 1) == 0);
    ASSERT(setenv("SIL_X11_EWMH_FULLSCREEN_MOVE_BEFORE", "1", 1) == 0);

    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(graphics_device_width(),
                                         graphics_device_height(), NULL));
    CHECK_INTEQUAL(called_XMoveWindow, 1);
    CHECK_FALSE(last_event_display);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_env_move_before_true_xrandr)
{
    if (!check_xrandr()) {
        SKIP("XRandR not found.");
    }

    if (graphics_num_devices() == 1) {
        SKIP("Only one display device present.");
    }

    Display *display = linux_x11_display();
    if (ScreenCount(display) > 1) {
        SKIP("X11 server not in a single-screen, multi-head configuration.");
    }

    Atom net_wm_state = XInternAtom(display, "_NET_WM_STATE", True);
    Atom net_wm_state_fullscreen =
        XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", True);
    if (!net_wm_state || !net_wm_state_fullscreen) {
        SKIP("_NET_WM_STATE or _NET_WM_STATE_FULLSCREEN atom not found.");
    }

    ASSERT(setenv("SIL_X11_FULLSCREEN_METHOD", "EWMH_FULLSCREEN", 1) == 0);
    ASSERT(setenv("SIL_X11_CREATE_FULLSCREEN", "1", 1) == 0);
    ASSERT(setenv("SIL_X11_EWMH_FULLSCREEN_MOVE_BEFORE", "1", 1) == 0);

    CHECK_TRUE(graphics_set_display_attr("device", 1));
    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(graphics_device_width(),
                                         graphics_device_height(), NULL));
    CHECK_INTEQUAL(called_XMoveWindow, 1);
    CHECK_FALSE(last_event_display);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_env_move_before_false_vidmode_NOINIT)
{
    disable_XRRQueryExtension = 1;
    CHECK_TRUE(graphics_init());
    disable_XRRQueryExtension = 0;

    if (!check_vidmode()) {
        SKIP("XF86VidMode not found.");
    }

    Display *display = linux_x11_display();
    Atom net_wm_state = XInternAtom(display, "_NET_WM_STATE", True);
    Atom net_wm_state_fullscreen =
        XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", True);
    Atom net_wm_fullscreen_monitors =
        XInternAtom(display, "_NET_WM_FULLSCREEN_MONITORS", True);
    if (!net_wm_state || !net_wm_state_fullscreen
     || !net_wm_fullscreen_monitors) {
        SKIP("_NET_WM_STATE, _NET_WM_STATE_FULLSCREEN, or"
             " _NET_WM_FULLSCREEN_MONITORS atom not found.");
    }

    ASSERT(setenv("SIL_X11_FULLSCREEN_METHOD", "EWMH_FULLSCREEN", 1) == 0);
    ASSERT(setenv("SIL_X11_CREATE_FULLSCREEN", "1", 1) == 0);
    ASSERT(setenv("SIL_X11_EWMH_FULLSCREEN_MOVE_BEFORE", "0", 1) == 0);

    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(graphics_device_width(),
                                         graphics_device_height(), NULL));
    Window window = linux_x11_window();
    Window root = RootWindow(display, linux_x11_screen());
    CHECK_INTEQUAL(called_XMoveWindow, 0);
    CHECK_PTREQUAL(last_event_display, display);
    CHECK_INTEQUAL(last_event_window, root);
    CHECK_FALSE(last_event_propagate);
    CHECK_INTEQUAL(last_event_mask,
                   SubstructureNotifyMask | SubstructureRedirectMask);
    CHECK_INTEQUAL(last_event.type, ClientMessage);
    CHECK_PTREQUAL(last_event.xclient.display, display);
    CHECK_INTEQUAL(last_event.xclient.window, window);
    CHECK_INTEQUAL(last_event.xclient.message_type,
                   net_wm_fullscreen_monitors);
    CHECK_INTEQUAL(last_event.xclient.format, 32);
    CHECK_INTEQUAL(last_event.xclient.data.l[0], 0);
    CHECK_INTEQUAL(last_event.xclient.data.l[1], 0);
    CHECK_INTEQUAL(last_event.xclient.data.l[2], 0);
    CHECK_INTEQUAL(last_event.xclient.data.l[3], 0);
    CHECK_INTEQUAL(last_event.xclient.data.l[4], 1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_env_move_before_false_xrandr)
{
    if (!check_xrandr()) {
        SKIP("XRandR not found.");
    }

    if (graphics_num_devices() == 1) {
        SKIP("Only one display device present.");
    }

    Display *display = linux_x11_display();
    if (ScreenCount(display) > 1) {
        SKIP("X11 server not in a single-screen, multi-head configuration.");
    }

    Atom net_wm_state = XInternAtom(display, "_NET_WM_STATE", True);
    Atom net_wm_state_fullscreen =
        XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", True);
    Atom net_wm_fullscreen_monitors =
        XInternAtom(display, "_NET_WM_FULLSCREEN_MONITORS", True);
    if (!net_wm_state || !net_wm_state_fullscreen
     || !net_wm_fullscreen_monitors) {
        SKIP("_NET_WM_STATE, _NET_WM_STATE_FULLSCREEN, or"
             " _NET_WM_FULLSCREEN_MONITORS atom not found.");
    }

    ASSERT(setenv("SIL_X11_FULLSCREEN_METHOD", "EWMH_FULLSCREEN", 1) == 0);
    ASSERT(setenv("SIL_X11_CREATE_FULLSCREEN", "1", 1) == 0);
    ASSERT(setenv("SIL_X11_EWMH_FULLSCREEN_MOVE_BEFORE", "0", 1) == 0);

    CHECK_TRUE(graphics_set_display_attr("device", 1));
    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(graphics_device_width(),
                                         graphics_device_height(), NULL));
    Window window = linux_x11_window();
    Window root = RootWindow(display, linux_x11_screen());
    CHECK_INTEQUAL(called_XMoveWindow, 0);
    CHECK_PTREQUAL(last_event_display, display);
    CHECK_INTEQUAL(last_event_window, root);
    CHECK_FALSE(last_event_propagate);
    CHECK_INTEQUAL(last_event_mask,
                   SubstructureNotifyMask | SubstructureRedirectMask);
    CHECK_INTEQUAL(last_event.type, ClientMessage);
    CHECK_PTREQUAL(last_event.xclient.display, display);
    CHECK_INTEQUAL(last_event.xclient.window, window);
    CHECK_INTEQUAL(last_event.xclient.message_type,
                   net_wm_fullscreen_monitors);
    CHECK_INTEQUAL(last_event.xclient.format, 32);
    CHECK_INTEQUAL(last_event.xclient.data.l[0], 1);
    CHECK_INTEQUAL(last_event.xclient.data.l[1], 1);
    CHECK_INTEQUAL(last_event.xclient.data.l[2], 1);
    CHECK_INTEQUAL(last_event.xclient.data.l[3], 1);
    CHECK_INTEQUAL(last_event.xclient.data.l[4], 1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_env_move_before_invalid_empty)
{
    Display *display = linux_x11_display();
    int device;
    if (check_xrandr()
     && graphics_num_devices() > 1
     && ScreenCount(display) == 1) {
        device = 1;
    } else {
        graphics_cleanup();
        disable_XRRQueryExtension = 1;
        CHECK_TRUE(graphics_init());
        disable_XRRQueryExtension = 0;
        if (!check_vidmode()) {
            SKIP("XF86VidMode not found.");
        }
        device = 0;
    }

    Atom net_wm_state = XInternAtom(display, "_NET_WM_STATE", True);
    Atom net_wm_state_fullscreen =
        XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", True);
    Atom net_wm_fullscreen_monitors =
        XInternAtom(display, "_NET_WM_FULLSCREEN_MONITORS", True);
    if (!net_wm_state || !net_wm_state_fullscreen) {
        SKIP("_NET_WM_STATE or _NET_WM_STATE_FULLSCREEN atom not found.");
    }

    ASSERT(setenv("SIL_X11_FULLSCREEN_METHOD", "EWMH_FULLSCREEN", 1) == 0);
    ASSERT(setenv("SIL_X11_CREATE_FULLSCREEN", "1", 1) == 0);
    ASSERT(!getenv("SIL_X11_EWMH_FULLSCREEN_MOVE_BEFORE"));

    CHECK_TRUE(graphics_set_display_attr("device", device));
    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(graphics_device_width(),
                                         graphics_device_height(), NULL));
    const int is_move_before = (called_XMoveWindow == 1);

    for (int do_empty = 0; do_empty < 2; do_empty++) {
        ASSERT(setenv("SIL_X11_EWMH_FULLSCREEN_MOVE_BEFORE",
                      do_empty ? "" : "foo", 1) == 0);

        called_XMoveWindow = 0;
        last_event_display = NULL;
        linux_close_window();
        CHECK_TRUE(graphics_set_display_attr("device", device));
        CHECK_TRUE(graphics_set_display_mode(graphics_device_width(),
                                             graphics_device_height(), NULL));
        if (is_move_before) {
            CHECK_INTEQUAL(called_XMoveWindow, 1);
        } else {
            CHECK_TRUE(net_wm_fullscreen_monitors);
            Window window = linux_x11_window();
            Window root = RootWindow(display, linux_x11_screen());
            CHECK_INTEQUAL(called_XMoveWindow, 0);
            CHECK_PTREQUAL(last_event_display, display);
            CHECK_INTEQUAL(last_event_window, root);
            CHECK_FALSE(last_event_propagate);
            CHECK_INTEQUAL(last_event_mask,
                           SubstructureNotifyMask | SubstructureRedirectMask);
            CHECK_INTEQUAL(last_event.type, ClientMessage);
            CHECK_PTREQUAL(last_event.xclient.display, display);
            CHECK_INTEQUAL(last_event.xclient.window, window);
            CHECK_INTEQUAL(last_event.xclient.message_type,
                           net_wm_fullscreen_monitors);
            CHECK_INTEQUAL(last_event.xclient.format, 32);
            CHECK_INTEQUAL(last_event.xclient.data.l[0], 1);
            CHECK_INTEQUAL(last_event.xclient.data.l[1], 1);
            CHECK_INTEQUAL(last_event.xclient.data.l[2], 1);
            CHECK_INTEQUAL(last_event.xclient.data.l[3], 1);
            CHECK_INTEQUAL(last_event.xclient.data.l[4], 1);
        }
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_env_resize_after_true)
{
    Display *display = linux_x11_display();
    Atom net_wm_state = XInternAtom(display, "_NET_WM_STATE", True);
    Atom net_wm_state_fullscreen =
        XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", True);
    if (!net_wm_state || !net_wm_state_fullscreen) {
        SKIP("_NET_WM_STATE or _NET_WM_STATE_FULLSCREEN atom not found.");
    }

    ASSERT(setenv("SIL_X11_FULLSCREEN_METHOD", "EWMH_FULLSCREEN", 1) == 0);
    ASSERT(setenv("SIL_X11_EWMH_FULLSCREEN_RESIZE_AFTER", "1", 1) == 0);

    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(graphics_device_width(),
                                         graphics_device_height(), NULL));
    Window window = linux_x11_window();
    CHECK_INTEQUAL(called_XMoveResizeWindow, 1);
    XSizeHints hints;
    CHECK_TRUE(XGetWMNormalHints(display, window, &hints, (long[1]){0}));
    CHECK_TRUE(hints.flags & PMinSize);
    CHECK_TRUE(hints.flags & PMaxSize);
    CHECK_INTEQUAL(hints.min_width, graphics_device_width());
    CHECK_INTEQUAL(hints.max_width, graphics_device_width());
    CHECK_INTEQUAL(hints.min_height, graphics_device_height());
    CHECK_INTEQUAL(hints.max_height, graphics_device_height());

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_env_resize_after_false)
{
    Display *display = linux_x11_display();
    Atom net_wm_state = XInternAtom(display, "_NET_WM_STATE", True);
    Atom net_wm_state_fullscreen =
        XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", True);
    if (!net_wm_state || !net_wm_state_fullscreen) {
        SKIP("_NET_WM_STATE or _NET_WM_STATE_FULLSCREEN atom not found.");
    }

    ASSERT(setenv("SIL_X11_FULLSCREEN_METHOD", "EWMH_FULLSCREEN", 1) == 0);
    ASSERT(setenv("SIL_X11_EWMH_FULLSCREEN_RESIZE_AFTER", "0", 1) == 0);

    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(graphics_device_width(),
                                         graphics_device_height(), NULL));
    Window window = linux_x11_window();
    CHECK_INTEQUAL(called_XMoveResizeWindow, 0);
    XSizeHints hints;
    CHECK_TRUE(XGetWMNormalHints(display, window, &hints, (long[1]){0}));
    CHECK_FALSE(hints.flags & (PMinSize | PMaxSize));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_env_resize_after_invalid_empty)
{
    Display *display = linux_x11_display();
    Atom net_wm_state = XInternAtom(display, "_NET_WM_STATE", True);
    Atom net_wm_state_fullscreen =
        XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", True);
    if (!net_wm_state || !net_wm_state_fullscreen) {
        SKIP("_NET_WM_STATE or _NET_WM_STATE_FULLSCREEN atom not found.");
    }

    ASSERT(setenv("SIL_X11_FULLSCREEN_METHOD", "EWMH_FULLSCREEN", 1) == 0);
    ASSERT(setenv("SIL_X11_EWMH_FULLSCREEN_RESIZE_AFTER", "1", 1) == 0);

    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(graphics_device_width(),
                                         graphics_device_height(), NULL));
    const int is_resize_after = (called_XMoveResizeWindow == 1);

    for (int do_empty = 0; do_empty < 2; do_empty++) {
        ASSERT(setenv("SIL_X11_EWMH_FULLSCREEN_RESIZE_AFTER",
                      do_empty ? "" : "foo", 1) == 0);

        called_XMoveResizeWindow = 0;
        linux_close_window();
        CHECK_TRUE(graphics_set_display_mode(graphics_device_width(),
                                             graphics_device_height(), NULL));
        if (is_resize_after) {
            CHECK_INTEQUAL(called_XMoveResizeWindow, 1);
        } else {
            Window window = linux_x11_window();
            XSizeHints hints;
            CHECK_TRUE(XGetWMNormalHints(display, window, &hints, (long[1]){0}));
            CHECK_TRUE(hints.flags & PMinSize);
            CHECK_TRUE(hints.flags & PMaxSize);
            CHECK_INTEQUAL(hints.min_width, graphics_device_width());
            CHECK_INTEQUAL(hints.max_width, graphics_device_width());
            CHECK_INTEQUAL(hints.min_height, graphics_device_height());
            CHECK_INTEQUAL(hints.max_height, graphics_device_height());
        }
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_env_use_transient_for_true)
{
    Display *display = linux_x11_display();

    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop;
    Status result;

    ASSERT(setenv("SIL_X11_FULLSCREEN_METHOD", "XMOVEWINDOW", 1) == 0);
    ASSERT(setenv("SIL_X11_USE_TRANSIENT_FOR_HINT", "1", 1) == 0);

    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(graphics_device_width(),
                                         graphics_device_height(), NULL));
    Window window = linux_x11_window();
    result = XGetWindowProperty(display, window, XA_WM_TRANSIENT_FOR, 0, 1,
                                False, AnyPropertyType, &actual_type,
                                &actual_format, &nitems, &bytes_after, &prop);
    CHECK_INTEQUAL(result, Success);
    CHECK_INTEQUAL(actual_type, XA_WINDOW);
    CHECK_INTEQUAL(actual_format, 32);
    CHECK_INTEQUAL(nitems, 1);
    CHECK_INTEQUAL(bytes_after, 0);
    CHECK_INTEQUAL(*(long *)(void *)prop, RootWindow(display, linux_x11_screen()));
    XFree(prop);

    called_XCreateWindow = 0;
    CHECK_TRUE(graphics_set_display_attr("window", 1));
    CHECK_TRUE(graphics_set_display_mode(graphics_device_width(),
                                         graphics_device_height(), NULL));
    CHECK_INTEQUAL(called_XCreateWindow, 0);
    result = XGetWindowProperty(display, window, XA_WM_TRANSIENT_FOR, 0, 1,
                                False, AnyPropertyType, &actual_type,
                                &actual_format, &nitems, &bytes_after, &prop);
    CHECK_TRUE(result != Success || actual_type == 0 || *(long *)(void *)prop == 0);
    if (result == Success && actual_type != 0) {
        XFree(prop);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_env_use_transient_for_false)
{
    Display *display = linux_x11_display();
    Atom motif_wm_hints = XInternAtom(display, "_MOTIF_WM_HINTS", True);
    if (!motif_wm_hints) {
        SKIP("_MOTIF_WM_HINTS atom not found.");
    }

    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop;
    Status result;

    ASSERT(setenv("SIL_X11_FULLSCREEN_METHOD", "XMOVEWINDOW", 1) == 0);
    ASSERT(setenv("SIL_X11_USE_TRANSIENT_FOR_HINT", "0", 1) == 0);

    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(graphics_device_width(),
                                         graphics_device_height(), NULL));
    Window window = linux_x11_window();
    result = XGetWindowProperty(display, window, motif_wm_hints, 0, 5,
                                False, AnyPropertyType, &actual_type,
                                &actual_format, &nitems, &bytes_after, &prop);
    CHECK_INTEQUAL(result, Success);
    CHECK_INTEQUAL(actual_type, motif_wm_hints);
    CHECK_INTEQUAL(actual_format, 32);
    CHECK_INTEQUAL(nitems, 5);
    CHECK_INTEQUAL(bytes_after, 0);
    CHECK_INTEQUAL(((long *)(void *)prop)[0], 1 << 1);
    CHECK_INTEQUAL(((long *)(void *)prop)[1], 0);
    CHECK_INTEQUAL(((long *)(void *)prop)[2], 0);
    CHECK_INTEQUAL(((long *)(void *)prop)[3], 0);
    CHECK_INTEQUAL(((long *)(void *)prop)[4], 0);
    XFree(prop);

    called_XCreateWindow = 0;
    CHECK_TRUE(graphics_set_display_attr("window", 1));
    CHECK_TRUE(graphics_set_display_mode(graphics_device_width(),
                                         graphics_device_height(), NULL));
    CHECK_INTEQUAL(called_XCreateWindow, 0);
    result = XGetWindowProperty(display, window, motif_wm_hints, 0, 5,
                                False, AnyPropertyType, &actual_type,
                                &actual_format, &nitems, &bytes_after, &prop);
    CHECK_INTEQUAL(result, Success);
    CHECK_INTEQUAL(actual_type, motif_wm_hints);
    CHECK_INTEQUAL(actual_format, 32);
    CHECK_INTEQUAL(nitems, 5);
    CHECK_INTEQUAL(bytes_after, 0);
    CHECK_INTEQUAL(((long *)(void *)prop)[0], 1 << 1);
    CHECK_INTEQUAL(((long *)(void *)prop)[1], 0);
    CHECK_INTEQUAL(((long *)(void *)prop)[2], 1);
    CHECK_INTEQUAL(((long *)(void *)prop)[3], 0);
    CHECK_INTEQUAL(((long *)(void *)prop)[4], 0);
    XFree(prop);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_env_use_transient_for_invalid_empty)
{
    Display *display = linux_x11_display();
    Atom motif_wm_hints = XInternAtom(display, "_MOTIF_WM_HINTS", True);

    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop;
    Status result;

    ASSERT(setenv("SIL_X11_FULLSCREEN_METHOD", "XMOVEWINDOW", 1) == 0);

    for (int do_empty = 0; do_empty < 2; do_empty++) {
        ASSERT(setenv("SIL_X11_USE_TRANSIENT_FOR_HINT",
                      do_empty ? "" : "foo", 1) == 0);

        linux_close_window();
        CHECK_TRUE(graphics_set_display_attr("window", 0));
        CHECK_TRUE(graphics_set_display_mode(graphics_device_width(),
                                             graphics_device_height(), NULL));
        Window window = linux_x11_window();

        if (motif_wm_hints) {
            result = XGetWindowProperty(display, window, motif_wm_hints, 0, 5,
                                        False, AnyPropertyType, &actual_type,
                                        &actual_format, &nitems, &bytes_after,
                                        &prop);
            CHECK_INTEQUAL(result, Success);
            CHECK_INTEQUAL(actual_type, motif_wm_hints);
            CHECK_INTEQUAL(actual_format, 32);
            CHECK_INTEQUAL(nitems, 5);
            CHECK_INTEQUAL(bytes_after, 0);
            CHECK_INTEQUAL(((long *)(void *)prop)[0], 1 << 1);
            CHECK_INTEQUAL(((long *)(void *)prop)[1], 0);
            CHECK_INTEQUAL(((long *)(void *)prop)[2], 0);
            CHECK_INTEQUAL(((long *)(void *)prop)[3], 0);
            CHECK_INTEQUAL(((long *)(void *)prop)[4], 0);
            XFree(prop);
        } else {
            result = XGetWindowProperty(display, window, XA_WM_TRANSIENT_FOR,
                                        0, 1, False, AnyPropertyType,
                                        &actual_type, &actual_format, &nitems,
                                        &bytes_after, &prop);
            CHECK_INTEQUAL(result, Success);
            CHECK_INTEQUAL(actual_type, XA_WINDOW);
            CHECK_INTEQUAL(actual_format, 32);
            CHECK_INTEQUAL(nitems, 1);
            CHECK_INTEQUAL(bytes_after, 0);
            CHECK_INTEQUAL(*(long *)(void *)prop,
                           RootWindow(display, linux_x11_screen()));
            XFree(prop);
        }
    }

    return 1;
}

/*************************************************************************/
/*************************************************************************/
