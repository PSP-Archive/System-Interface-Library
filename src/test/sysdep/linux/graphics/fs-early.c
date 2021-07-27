/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/linux/graphics/fs-early.c: Linux-specific graphics tests
 * covering fullscreen window creation ($SIL_X11_CREATE_FULLSCREEN).
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

DEFINE_GENERIC_TEST_RUNNER(test_linux_graphics_fs_early)

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

TEST(test_env_create_fullscreen_true)
{
    Display *display = linux_x11_display();
    Atom motif_wm_hints = XInternAtom(display, "_MOTIF_WM_HINTS", True);
    Atom net_wm_state = XInternAtom(display, "_NET_WM_STATE", True);
    Atom net_wm_state_fullscreen =
        XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", True);

    ASSERT(setenv("SIL_X11_CREATE_FULLSCREEN", "1", 1) == 0);
    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(graphics_device_width(),
                                         graphics_device_height(), NULL));
    Window window = linux_x11_window();

    /* XMoveWindow() and XSendEvent() are only used for post-create
     * fullscreen. */
    CHECK_INTEQUAL(called_XMoveWindow, 0);
    CHECK_FALSE(last_event_display);

    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop;
    Status result = BadMatch;  // Arbitrary non-"Success" value.

    if (net_wm_state && net_wm_state_fullscreen) {
        result = XGetWindowProperty(display, window, net_wm_state, 0, 1,
                                    False, AnyPropertyType, &actual_type,
                                    &actual_format, &nitems, &bytes_after,
                                    &prop);
        if (result == Success && actual_type == 0) {
            result = BadMatch;
        }
    }
    if (result == Success) {
        CHECK_INTEQUAL(actual_type, XA_ATOM);
        CHECK_INTEQUAL(actual_format, 32);
        CHECK_INTEQUAL(nitems, 1);
        CHECK_INTEQUAL(bytes_after, 0);
        CHECK_INTEQUAL(*(long *)(void *)prop, net_wm_state_fullscreen);
        XFree(prop);
    } else {
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

/*-----------------------------------------------------------------------*/

TEST(test_env_create_fullscreen_false)
{
    Display *display = linux_x11_display();
    Atom motif_wm_hints = XInternAtom(display, "_MOTIF_WM_HINTS", True);
    Atom net_wm_state = XInternAtom(display, "_NET_WM_STATE", True);
    const long net_wm_state_add = 1;  // Not an atom, despite the spelling.
    Atom net_wm_state_fullscreen =
        XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", True);

    ASSERT(setenv("SIL_X11_CREATE_FULLSCREEN", "0", 1) == 0);
    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(graphics_device_width(),
                                         graphics_device_height(), NULL));
    Window window = linux_x11_window();

    if (last_event_display) {
        CHECK_TRUE(net_wm_state);
        CHECK_TRUE(net_wm_state_fullscreen);
        Window root = RootWindow(display, linux_x11_screen());
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
    } else {
        CHECK_INTEQUAL(called_XMoveWindow, 1);
        Atom actual_type;
        int actual_format;
        unsigned long nitems, bytes_after;
        unsigned char *prop;
        Status result;
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

/*-----------------------------------------------------------------------*/

/* Both invalid and empty values for $SIL_CREATE_FULLSCREEN should have the
 * same effect (and the logic to test them is similarly identical), so we
 * do them both here in a loop. */
TEST(test_env_create_fullscreen_invalid_empty)
{
    ASSERT(!getenv("SIL_CREATE_FULLSCREEN"));

    Display *display = linux_x11_display();
    Atom motif_wm_hints = XInternAtom(display, "_MOTIF_WM_HINTS", True);
    Atom net_wm_state = XInternAtom(display, "_NET_WM_STATE", True);
    const long net_wm_state_add = 1;
    Atom net_wm_state_fullscreen =
        XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", True);

    /* Figure out the default fullscreen timing and method. */
    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(graphics_device_width(),
                                         graphics_device_height(), NULL));
    const int is_early_fullscreen = !called_XMoveWindow && !last_event_display;
    int is_ewmh_fullscreen = 0;
    if (last_event_display) {
        CHECK_TRUE(net_wm_state);
        CHECK_TRUE(net_wm_state_fullscreen);
        is_ewmh_fullscreen = 1;
    } else if (net_wm_state && net_wm_state_fullscreen) {
        Window window = linux_x11_window();
        Atom actual_type;
        int actual_format;
        unsigned long nitems, bytes_after;
        unsigned char *prop;
        Status result;
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
        /* This setting should be ignored. */
        ASSERT(setenv("SIL_X11_CREATE_FULLSCREEN",
                      do_empty ? "" : "foo", 1) == 0);
        linux_close_window();
        CHECK_TRUE(graphics_set_display_mode(graphics_device_width(),
                                             graphics_device_height(), NULL));
        Window window = linux_x11_window();

        if (is_ewmh_fullscreen) {

            CHECK_TRUE(net_wm_state);
            CHECK_TRUE(net_wm_state_fullscreen);
            if (is_early_fullscreen) {
                CHECK_FALSE(last_event_display);
                Atom actual_type;
                int actual_format;
                unsigned long nitems, bytes_after;
                unsigned char *prop;
                Status result = XGetWindowProperty(
                    display, window, net_wm_state, 0, 1, False,
                    AnyPropertyType, &actual_type, &actual_format,
                    &nitems, &bytes_after, &prop);
                CHECK_INTEQUAL(result, Success);
                CHECK_INTEQUAL(actual_type, XA_ATOM);
                CHECK_INTEQUAL(actual_format, 32);
                CHECK_INTEQUAL(nitems, 1);
                CHECK_INTEQUAL(bytes_after, 0);
                CHECK_INTEQUAL(*(long *)(void *)prop, net_wm_state_fullscreen);
                XFree(prop);
            } else {
                Window root = RootWindow(display, linux_x11_screen());
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
            }

        } else {  // !is_ewmh_fullscreen

            CHECK_INTEQUAL(called_XMoveWindow, is_early_fullscreen ? 0 : 1);
            Atom actual_type;
            int actual_format;
            unsigned long nitems, bytes_after;
            unsigned char *prop;
            Status result;
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
                XFree(prop);
            }

        }  // if (is_ewmh_fullscreen)
    }  // for (int do_empty = 0; do_empty < 2; do_empty++)

    return 1;
}

/*************************************************************************/
/*************************************************************************/
