/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/linux/graphics/fs-mode.c: Linux-specific graphics tests
 * covering X11 event handling.
 */

#include "src/base.h"
#include "src/graphics.h"
#include "src/sysdep.h"
#include "src/sysdep/linux/internal.h"
#include "src/test/base.h"
#include "src/test/graphics/internal.h"
#include "src/test/sysdep/linux/graphics/internal.h"
#include "src/test/sysdep/linux/wrap-x11.h"

#include <time.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xinerama.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/xf86vmode.h>

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_linux_graphics_x11_events)

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

TEST(test_fullscreen_iconify)
{
    if (!check_xrandr()) {
        SKIP("XRandR not found or too old.");
    }

    int mode_width, mode_height;
    if (!get_alternate_video_mode(&mode_width, &mode_height)) {
        SKIP("No alternate video mode available.");
    }

    int saved_x, saved_y;
    get_mouse_position(&saved_x, &saved_y);

    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(mode_width, mode_height, NULL));
    CHECK_FALSE(graphics_display_is_window());
    CHECK_INTEQUAL(graphics_display_width(), mode_width);
    CHECK_INTEQUAL(graphics_display_height(), mode_height);

    int xrandr_width, xrandr_height;
    xrandr_get_current_resolution(&xrandr_width, &xrandr_height);
    CHECK_INTEQUAL(xrandr_width, mode_width);
    CHECK_INTEQUAL(xrandr_height, mode_height);

    /* Iconifying the window should reset to the default screen mode. */
    Display *display = linux_x11_display();
    Window window = linux_x11_window();
    int screen = linux_x11_screen();
    if (!XIconifyWindow(display, window, screen)) {
        SKIP("Window manager does not support iconification.");
    }
    XSync(display, False);
    /* Give the window manager a chance to process the message. */
    nanosleep(&(struct timespec){0, 100*1000*1000}, NULL);
    while (XPending(display)) {
        XEvent unused_event;
        (void) linux_get_window_event(&unused_event);
    }
    xrandr_get_current_resolution(&xrandr_width, &xrandr_height);
    CHECK_INTEQUAL(xrandr_width, graphics_device_width());
    CHECK_INTEQUAL(xrandr_height, graphics_device_height());

    /* Restoring the window should restore the custom screen mode. */
    XMapRaised(display, window);
    XSync(display, False);
    nanosleep(&(struct timespec){0, 100*1000*1000}, NULL);
    while (XPending(display)) {
        XEvent unused_event;
        (void) linux_get_window_event(&unused_event);
    }
    xrandr_get_current_resolution(&xrandr_width, &xrandr_height);
    CHECK_INTEQUAL(xrandr_width, mode_width);
    CHECK_INTEQUAL(xrandr_height, mode_height);

    CHECK_TRUE(graphics_set_display_attr("window", 1));
    CHECK_TRUE(graphics_set_display_mode(mode_width, mode_height, NULL));
    set_mouse_position(saved_x, saved_y);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fullscreen_iconify_set_mode_failure)
{
    if (!check_xrandr()) {
        SKIP("XRandR not found or too old.");
    }

    int mode_width, mode_height;
    if (!get_alternate_video_mode(&mode_width, &mode_height)) {
        SKIP("No alternate video mode available.");
    }

    int saved_x, saved_y;
    get_mouse_position(&saved_x, &saved_y);

    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(mode_width, mode_height, NULL));
    CHECK_FALSE(graphics_display_is_window());
    CHECK_INTEQUAL(graphics_display_width(), mode_width);
    CHECK_INTEQUAL(graphics_display_height(), mode_height);

    int xrandr_width, xrandr_height;
    xrandr_get_current_resolution(&xrandr_width, &xrandr_height);
    CHECK_INTEQUAL(xrandr_width, mode_width);
    CHECK_INTEQUAL(xrandr_height, mode_height);

    /* Iconifying the window should reset to the default screen mode. */
    Display *display = linux_x11_display();
    Window window = linux_x11_window();
    int screen = linux_x11_screen();
    if (!XIconifyWindow(display, window, screen)) {
        SKIP("Window manager does not support iconification.");
    }
    XSync(display, False);
    /* Give the window manager a chance to process the message. */
    nanosleep(&(struct timespec){0, 100*1000*1000}, NULL);
    while (XPending(display)) {
        XEvent unused_event;
        (void) linux_get_window_event(&unused_event);
    }
    xrandr_get_current_resolution(&xrandr_width, &xrandr_height);
    CHECK_INTEQUAL(xrandr_width, graphics_device_width());
    CHECK_INTEQUAL(xrandr_height, graphics_device_height());

    /* Force a failure of set_video_mode() when the window is restored. */
    disable_XRRGetCrtcInfo = 1;
    XMapRaised(display, window);
    XSync(display, False);
    nanosleep(&(struct timespec){0, 100*1000*1000}, NULL);
    while (XPending(display)) {
        XEvent unused_event;
        (void) linux_get_window_event(&unused_event);
    }
    disable_XRRGetCrtcInfo = 0;
    xrandr_get_current_resolution(&xrandr_width, &xrandr_height);
    CHECK_INTEQUAL(xrandr_width, graphics_device_width());
    CHECK_INTEQUAL(xrandr_height, graphics_device_height());
    /* The window should have been changed to non-fullscreen. */
    CHECK_TRUE(graphics_display_is_window());

    set_mouse_position(saved_x, saved_y);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_wm_ping)
{
    Display *display = linux_x11_display();
    Atom wm_protocols = XInternAtom(display, "WM_PROTOCOLS", True);
    Atom net_wm_ping = XInternAtom(display, "_NET_WM_PING", True);
    if (!wm_protocols || !net_wm_ping) {
        SKIP("WM_PROTOCOLS or _NET_WM_PING atom not found.");
    }

    CHECK_TRUE(graphics_set_display_mode(TESTW, TESTH, NULL));
    Window window = linux_x11_window();
    Window root = RootWindow(display, linux_x11_screen());
    const long timestamp = 12345;
    XEvent event = {.xclient = {
        .type = ClientMessage,
        .display = display,
        .window = window,
        .message_type = wm_protocols,
        .format = 32,
        .data = {.l = {net_wm_ping, timestamp, window, 0, 0}},
    }};
    CHECK_TRUE(XSendEvent(display, window, False, 0, &event));
    XSync(display, False);
    while (XPending(display)) {
        XEvent unused_event;
        (void) linux_get_window_event(&unused_event);
    }
    CHECK_PTREQUAL(last_event_display, display);
    CHECK_INTEQUAL(last_event_window, root);
    CHECK_FALSE(last_event_propagate);
    CHECK_INTEQUAL(last_event_mask,
                   SubstructureNotifyMask | SubstructureRedirectMask);
    CHECK_INTEQUAL(last_event.type, ClientMessage);
    CHECK_PTREQUAL(last_event.xclient.display, display);
    CHECK_INTEQUAL(last_event.xclient.window, root);
    CHECK_INTEQUAL(last_event.xclient.message_type, wm_protocols);
    CHECK_INTEQUAL(last_event.xclient.format, 32);
    CHECK_INTEQUAL(last_event.xclient.data.l[0], net_wm_ping);
    CHECK_INTEQUAL(last_event.xclient.data.l[1], timestamp);
    CHECK_INTEQUAL(last_event.xclient.data.l[2], window);
    CHECK_INTEQUAL(last_event.xclient.data.l[3], 0);
    CHECK_INTEQUAL(last_event.xclient.data.l[4], 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_unknown_client_message)
{
    Display *display = linux_x11_display();
    Atom wm_protocols = XInternAtom(display, "WM_PROTOCOLS", True);
    Atom net_wm_ping = XInternAtom(display, "_NET_WM_PING", True);

    CHECK_TRUE(graphics_set_display_mode(TESTW, TESTH, NULL));
    Window window = linux_x11_window();
    XEvent event = {.xclient = {
        .type = ClientMessage,
        .display = display,
        .window = window,
        .message_type = wm_protocols,
        .format = 32,
        .data = {.l = {0}},
    }};

    CHECK_TRUE(XSendEvent(display, window, False, 0, &event));
    last_event_display = NULL;
    XSync(display, False);
    while (XPending(display)) {
        XEvent unused_event;
        (void) linux_get_window_event(&unused_event);
    }
    CHECK_FALSE(last_event_display);  // No event should have been generated.

    event.xclient.format = 8;
    event.xclient.data.l[0] = net_wm_ping;
    CHECK_TRUE(XSendEvent(display, window, False, 0, &event));
    last_event_display = NULL;
    XSync(display, False);
    while (XPending(display)) {
        XEvent unused_event;
        (void) linux_get_window_event(&unused_event);
    }
    CHECK_FALSE(last_event_display);

    event.xclient.format = 32;
    event.xclient.message_type = 0;
    CHECK_TRUE(XSendEvent(display, window, False, 0, &event));
    last_event_display = NULL;
    XSync(display, False);
    while (XPending(display)) {
        XEvent unused_event;
        (void) linux_get_window_event(&unused_event);
    }
    CHECK_FALSE(last_event_display);

    return 1;
}

/*************************************************************************/
/*************************************************************************/
