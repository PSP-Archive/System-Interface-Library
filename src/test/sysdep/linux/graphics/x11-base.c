/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/linux/graphics/x11-base.c: Linux-specific graphics tests
 * covering basic X11 behavior.
 */

#include "src/base.h"
#include "src/graphics.h"
#include "src/sysdep.h"
#include "src/sysdep/linux/internal.h"
#include "src/test/base.h"
#include "src/test/graphics/internal.h"
#include "src/test/sysdep/linux/graphics/internal.h"
#include "src/test/sysdep/linux/wrap-x11.h"

#include <dlfcn.h>
#define RTLD_DEFAULT  NULL

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xinerama.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/xf86vmode.h>

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_linux_graphics_x11_base)

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

/* Note that we make heavy use of ASSERT() in some of these tests to ensure
 * that subsequent tests don't end up running in a broken environment. */

TEST(test_no_display_variable_NOINIT_NOCLEANUP)
{
    const char *saved_DISPLAY = getenv("DISPLAY");
    ASSERT(saved_DISPLAY);
    ASSERT(*saved_DISPLAY);

    linux_close_display();
    /* As long as we're here, check that the display pointer is cleared on
     * close to avoid any risk of an invalid pointer dereference (even
     * though the display will never be closed while the program is running
     * under normal circumstances). */
    CHECK_FALSE(linux_x11_display());

    ASSERT(unsetenv("DISPLAY") == 0);
    CHECK_FALSE(linux_open_display());

    ASSERT(setenv("DISPLAY", saved_DISPLAY, 1) == 0);
    ASSERT(linux_open_display());
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_empty_display_variable_NOINIT_NOCLEANUP)
{
    const char *saved_DISPLAY = getenv("DISPLAY");
    ASSERT(saved_DISPLAY);
    ASSERT(*saved_DISPLAY);

    linux_close_display();
    ASSERT(setenv("DISPLAY", "", 1) == 0);
    CHECK_FALSE(linux_open_display());

    ASSERT(setenv("DISPLAY", saved_DISPLAY, 1) == 0);
    ASSERT(linux_open_display());
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_bad_display_variable_NOINIT_NOCLEANUP)
{
    const char *saved_DISPLAY = getenv("DISPLAY");
    ASSERT(saved_DISPLAY);
    ASSERT(*saved_DISPLAY);

    linux_close_display();
    ASSERT(setenv("DISPLAY", ":-1", 1) == 0);
    CHECK_FALSE(linux_open_display());

    ASSERT(setenv("DISPLAY", saved_DISPLAY, 1) == 0);
    ASSERT(linux_open_display());
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_x11_error_handler)
{
    Display *display = linux_x11_display();

    XMapWindow(display, None);
    XSync(display, False);
    CHECK_INTEQUAL(linux_x11_get_error(), BadWindow);
    const char *log = strstr(test_DLOG_last_message, ": ");
    ASSERT(log);
    log += 2;
    CHECK_STRSTARTS(log, "X11 error 3 (BadWindow");
    CHECK_STRENDS(log, ") in request 8 (X_MapWindow)");

    if (check_xrandr()) {
        __typeof__(XRRSetScreenSize) *p_XRRSetScreenSize =
            dlsym(RTLD_DEFAULT, "XRRSetScreenSize");
        ASSERT(p_XRRSetScreenSize != NULL);
        (*p_XRRSetScreenSize)(display, DefaultRootWindow(display), 0, 0, 0, 0);
        XSync(display, False);
        CHECK_INTEQUAL(linux_x11_get_error(), BadValue);
        log = strstr(test_DLOG_last_message, ": ");
        ASSERT(log);
        log += 2;
        CHECK_STRSTARTS(log, "X11 error 2 (BadValue");
        CHECK_STRENDS(log, ".7 (RRSetScreenSize)");
    }

    XGetAtomName(display, (Atom)-1);
    XSync(display, False);
    XMapWindow(display, None);  // Error should be discarded.
    XSync(display, False);
    CHECK_INTEQUAL(linux_x11_get_error(), BadAtom);

    /* The saved error code should have been cleared by the previous call. */
    CHECK_INTEQUAL(linux_x11_get_error(), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_resource_class)
{
    ASSERT(!getenv("SIL_X11_RESOURCE_CLASS"));

    Display *display = linux_x11_display();
    Atom wm_class;
    CHECK_TRUE(wm_class = XInternAtom(display, "WM_CLASS", True));
    Atom utf8_string = XInternAtom(display, "UTF8_STRING", False);

    Window window;
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop;
    Status result;

    CHECK_TRUE(graphics_set_display_mode(TESTW, TESTH, NULL));
    window = linux_x11_window();
    result = XGetWindowProperty(
        display, window, wm_class, 0, 1000, False, AnyPropertyType,
        &actual_type, &actual_format, &nitems, &bytes_after, &prop);
    CHECK_INTEQUAL(result, Success);
    if (utf8_string && actual_type != utf8_string) {
        CHECK_INTEQUAL(actual_type, XA_STRING);
    }
    CHECK_INTEQUAL(actual_format, 8);
    CHECK_TRUE(nitems > 0);
    CHECK_TRUE(nitems > strlen((char *)prop) + 1);  // Should be 2 strings.
    XFree(prop);

    static const char test_class[] = "test_class";
    ASSERT(setenv("SIL_X11_RESOURCE_CLASS", test_class, 1) == 0);
    linux_close_window();
    CHECK_TRUE(graphics_set_display_mode(TESTW, TESTH, NULL));
    window = linux_x11_window();
    result = XGetWindowProperty(
        display, window, wm_class, 0, 1000, False, AnyPropertyType,
        &actual_type, &actual_format, &nitems, &bytes_after, &prop);
    CHECK_INTEQUAL(result, Success);
    /* The class name is an ASCII string so this should always be XA_STRING,
     * not UTF8_STRING. */
    CHECK_INTEQUAL(actual_type, XA_STRING);
    CHECK_INTEQUAL(actual_format, 8);
    CHECK_INTEQUAL(nitems, 2 * (strlen(test_class) + 1));
    CHECK_INTEQUAL(bytes_after, 0);
    CHECK_STREQUAL((char *)prop, test_class);
    CHECK_STREQUAL((char *)(prop + strlen(test_class) + 1), test_class);
    XFree(prop);

    /* An empty string should revert to the default class name. */
    ASSERT(setenv("SIL_X11_RESOURCE_CLASS", "", 1) == 0);
    linux_close_window();
    CHECK_TRUE(graphics_set_display_mode(TESTW, TESTH, NULL));
    window = linux_x11_window();
    result = XGetWindowProperty(
        display, window, wm_class, 0, 1000, False, AnyPropertyType,
        &actual_type, &actual_format, &nitems, &bytes_after, &prop);
    CHECK_INTEQUAL(result, Success);
    if (utf8_string && actual_type != utf8_string) {
        CHECK_INTEQUAL(actual_type, XA_STRING);
    }
    CHECK_INTEQUAL(actual_format, 8);
    CHECK_TRUE(nitems > 2);  // Should not be two empty strings.
    XFree(prop);

    return 1;
}

/*-----------------------------------------------------------------------*/

/* Failure here could leave the window manager itself in an inconsistent
 * state, so be sure to clean up. */
#undef FAIL_ACTION
#define FAIL_ACTION  goto error

TEST(test_detect_window_manager)
{
    Display *display = linux_x11_display();
    Window root = RootWindow(display, 0);
    Atom motif_wm_info, net_supporting_wm_check, net_wm_name, utf8_string;
    Atom windowmaker_noticeboard;
    ASSERT(motif_wm_info = XInternAtom(display, "_MOTIF_WM_INFO", False));
    ASSERT(net_supporting_wm_check =
               XInternAtom(display, "_NET_SUPPORTING_WM_CHECK", False));
    ASSERT(net_wm_name = XInternAtom(display, "_NET_WM_NAME", False));
    ASSERT(utf8_string = XInternAtom(display, "UTF8_STRING", False));
    ASSERT(windowmaker_noticeboard =
               XInternAtom(display, "_WINDOWMAKER_NOTICEBOARD", False));

    Window supporting_window;
    Colormap colormap =
        XCreateColormap(display, root, DefaultVisual(display,0), AllocNone);
    XSetWindowAttributes cw_attributes = {
        .override_redirect = False,
        .background_pixmap = None,
        .border_pixel = 0,
        .colormap = colormap,
    };
    ASSERT(supporting_window = XCreateWindow(
               display, root, -1, -1, 1, 1, 0, DefaultDepth(display,0),
               InputOutput, DefaultVisual(display,0),
               CWOverrideRedirect | CWBackPixmap | CWBorderPixel | CWColormap,
               &cw_attributes));
    XSync(display, False);
    ASSERT(linux_x11_get_error() == 0);

    Window old_supporting_window = None;
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop;
    int result = XGetWindowProperty(
        display, root, net_supporting_wm_check, 0, 1, False, XA_WINDOW,
        &actual_type, &actual_format, &nitems, &bytes_after, &prop);
    if (result == Success && actual_type != 0) {
        /* It's not beyond the realm of possibility that a strange WM could
         * set a non-Window type value or multiple Window values on this
         * property, but since our cleanup code only handles single Window
         * values, we abort in that case to avoid corrupting WM state. */
        ASSERT(actual_type == XA_WINDOW);
        ASSERT(actual_format == 32);
        ASSERT(nitems == 1);
        old_supporting_window = *(long *)(void *)prop;
        XFree(prop);
    }

    void *old_motif_wm_info = NULL;
    int old_motif_wm_info_len = 0;
    result = XGetWindowProperty(
        display, root, motif_wm_info, 0, 1000, False, motif_wm_info,
        &actual_type, &actual_format, &nitems, &bytes_after, &prop);
    if (result == Success && actual_type != 0) {
        ASSERT(actual_type == motif_wm_info);
        ASSERT(actual_format == 32);
        ASSERT(bytes_after == 0);
        old_motif_wm_info = prop;
        old_motif_wm_info_len = nitems;
    }

    /* Clear _NET_WM_SUPPORTING_CHECK and _MOTIF_WM_INFO and check that we
     * detect the lack of a window manager. */
    CHECK_TRUE(XDeleteProperty(display, root, net_supporting_wm_check));
    CHECK_TRUE(XDeleteProperty(display, root, motif_wm_info));
    XSync(display, False);
    CHECK_INTEQUAL(linux_x11_get_error(), 0);
    linux_close_window();
    graphics_set_display_mode(TESTW, TESTH, NULL);
    CHECK_INTEQUAL(linux_window_manager(), WM_UNNAMED);

    /* Test MWM detection. */
    CHECK_TRUE(XChangeProperty(display, root, motif_wm_info, motif_wm_info, 32,
                               PropModeReplace, (void *)((long[]){2, 0}), 1));
    linux_close_window();
    graphics_set_display_mode(TESTW, TESTH, NULL);
    CHECK_INTEQUAL(linux_window_manager(), WM_MWM);
    /* A wrong property type should cause detection to fail. */
    CHECK_TRUE(XChangeProperty(display, root, motif_wm_info, XA_STRING, 8,
                               PropModeReplace, (void *)"foo", 3));
    linux_close_window();
    graphics_set_display_mode(TESTW, TESTH, NULL);
    CHECK_INTEQUAL(linux_window_manager(), WM_UNNAMED);
    /* Delete the property for subsequent tests. */
    CHECK_TRUE(XDeleteProperty(display, root, motif_wm_info));

    /* Install our dummy window as _NET_WM_SUPPORTING_CHECK for subsequent
     * tests.  Currently, our dummy supporting window has no _NET_WM_NAME
     * property, so we should be detected as an unnamed window manager. */
    CHECK_TRUE(XChangeProperty(display, root, net_supporting_wm_check,
                               XA_WINDOW, 32, PropModeReplace,
                               (void *)((long[]){supporting_window}), 1));
    XSync(display, False);
    CHECK_INTEQUAL(linux_x11_get_error(), 0);
    linux_close_window();
    graphics_set_display_mode(TESTW, TESTH, NULL);
    CHECK_INTEQUAL(linux_window_manager(), WM_UNNAMED);

    /* Test Window Maker detection. */
    CHECK_TRUE(XChangeProperty(display, supporting_window,
                               windowmaker_noticeboard,
                               XA_WINDOW, 32, PropModeReplace,
                               (void *)((long[]){supporting_window}), 1));
    XSync(display, False);
    CHECK_INTEQUAL(linux_x11_get_error(), 0);
    linux_close_window();
    graphics_set_display_mode(TESTW, TESTH, NULL);
    CHECK_INTEQUAL(linux_window_manager(), WM_WINDOWMAKER);
    /* An empty property should cause detection to fail. */
    CHECK_TRUE(XChangeProperty(display, supporting_window,
                               windowmaker_noticeboard,
                               XA_WINDOW, 32, PropModeReplace, NULL, 0));
    XSync(display, False);
    CHECK_INTEQUAL(linux_x11_get_error(), 0);
    linux_close_window();
    graphics_set_display_mode(TESTW, TESTH, NULL);
    CHECK_INTEQUAL(linux_window_manager(), WM_UNNAMED);
    /* Delete the property for subsequent tests. */
    CHECK_TRUE(XDeleteProperty(display, supporting_window,
                               windowmaker_noticeboard));
    XSync(display, False);
    CHECK_INTEQUAL(linux_x11_get_error(), 0);

    /* Test generic WM detection. */
    const char *name = "FVWM";
    CHECK_TRUE(XChangeProperty(display, supporting_window, net_wm_name,
                               utf8_string, 8, PropModeReplace,
                               (void *)name, strlen(name)));
    linux_close_window();
    graphics_set_display_mode(TESTW, TESTH, NULL);
    CHECK_INTEQUAL(linux_window_manager(), WM_FVWM);

    /* Test detection using XA_STRING instead of utf8_string. */
    name = "Fluxbox";
    CHECK_TRUE(XChangeProperty(display, supporting_window, net_wm_name,
                               XA_STRING, 8, PropModeReplace,
                               (void *)name, strlen(name)));
    linux_close_window();
    graphics_set_display_mode(TESTW, TESTH, NULL);
    CHECK_INTEQUAL(linux_window_manager(), WM_FLUXBOX);

    /* Test detection of an unknown name. */
    name = "<unknown name>";
    CHECK_TRUE(XChangeProperty(display, supporting_window, net_wm_name,
                               utf8_string, 8, PropModeReplace,
                               (void *)name, strlen(name)));
    linux_close_window();
    graphics_set_display_mode(TESTW, TESTH, NULL);
    CHECK_INTEQUAL(linux_window_manager(), WM_UNKNOWN);

    /* Test Enlightenment detection. */
    name = "e16";
    CHECK_TRUE(XChangeProperty(display, supporting_window, net_wm_name,
                               utf8_string, 8, PropModeReplace,
                               (void *)name, strlen(name)));
    linux_close_window();
    graphics_set_display_mode(TESTW, TESTH, NULL);
    CHECK_INTEQUAL(linux_window_manager(), WM_ENLIGHTENMENT);
    /* These should not be detected as Enlightenment. */
    name = "e";
    CHECK_TRUE(XChangeProperty(display, supporting_window, net_wm_name,
                               utf8_string, 8, PropModeReplace,
                               (void *)name, strlen(name)));
    linux_close_window();
    graphics_set_display_mode(TESTW, TESTH, NULL);
    CHECK_INTEQUAL(linux_window_manager(), WM_UNKNOWN);
    name = "e0";
    CHECK_TRUE(XChangeProperty(display, supporting_window, net_wm_name,
                               utf8_string, 8, PropModeReplace,
                               (void *)name, strlen(name)));
    linux_close_window();
    graphics_set_display_mode(TESTW, TESTH, NULL);
    CHECK_INTEQUAL(linux_window_manager(), WM_UNKNOWN);
    name = "e16 maybe?";
    CHECK_TRUE(XChangeProperty(display, supporting_window, net_wm_name,
                               utf8_string, 8, PropModeReplace,
                               (void *)name, strlen(name)));
    linux_close_window();
    graphics_set_display_mode(TESTW, TESTH, NULL);
    CHECK_INTEQUAL(linux_window_manager(), WM_UNKNOWN);

    /* Test IceWM detection. */
    name = "IceWM 0.0.0";
    CHECK_TRUE(XChangeProperty(display, supporting_window, net_wm_name,
                               utf8_string, 8, PropModeReplace,
                               (void *)name, strlen(name)));
    linux_close_window();
    graphics_set_display_mode(TESTW, TESTH, NULL);
    CHECK_INTEQUAL(linux_window_manager(), WM_ICEWM);

    /* Test handling of a wrong type in _NET_WM_NAME. */
    CHECK_TRUE(XChangeProperty(display, supporting_window, net_wm_name,
                               XA_WINDOW, 32, PropModeReplace,
                               (void *)((long[]){supporting_window}), 1));
    linux_close_window();
    graphics_set_display_mode(TESTW, TESTH, NULL);
    CHECK_INTEQUAL(linux_window_manager(), WM_UNNAMED);

    /* Test handling of a wrong type for _NET_WM_SUPPORTING_CHECK. */
    CHECK_TRUE(XChangeProperty(display, root, net_supporting_wm_check,
                               XA_STRING, 8, PropModeReplace,
                               (void *)"foo", 3));
    XSync(display, False);
    CHECK_INTEQUAL(linux_x11_get_error(), 0);
    linux_close_window();
    graphics_set_display_mode(TESTW, TESTH, NULL);
    CHECK_INTEQUAL(linux_window_manager(), WM_UNNAMED);

    /* Clean up after ourselves. */
    if (old_supporting_window) {
        ASSERT(XChangeProperty(display, root, net_supporting_wm_check,
                               XA_WINDOW, 32, PropModeReplace,
                               (void *)((long[]){old_supporting_window}), 1));
    } else {
        ASSERT(XDeleteProperty(display, root, net_supporting_wm_check));
    }
    XSync(display, False);
    ASSERT(linux_x11_get_error() == 0);
    return 1;

  error:
    if (old_supporting_window) {
        ASSERT(XChangeProperty(display, root, net_supporting_wm_check,
                               XA_WINDOW, 32, PropModeReplace,
                               (void *)((long[]){old_supporting_window}), 1));
    } else {
        ASSERT(XDeleteProperty(display, root, net_supporting_wm_check));
    }
    if (old_motif_wm_info) {
        ASSERT(XChangeProperty(display, root, motif_wm_info, motif_wm_info, 32,
                               PropModeReplace, old_motif_wm_info,
                               old_motif_wm_info_len));
        XFree(old_motif_wm_info);
    } else {
        ASSERT(XDeleteProperty(display, root, motif_wm_info));
    }
    XSync(display, False);
    ASSERT(linux_x11_get_error() == 0);
    return 0;
}

#undef FAIL_ACTION
#define FAIL_ACTION  return 0

/*************************************************************************/
/*************************************************************************/
