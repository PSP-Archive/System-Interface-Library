/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/linux/graphics/fs-minimize.c: Linux-specific graphics
 * tests covering auto-minimization of fullscreen windows.
 */

#include "src/base.h"
#include "src/graphics.h"
#include "src/sysdep.h"
#include "src/sysdep/linux/internal.h"
#include "src/sysdep/test.h"
#include "src/test/base.h"
#include "src/test/graphics/internal.h"
#include "src/test/sysdep/linux/graphics/internal.h"
#include "src/test/sysdep/linux/wrap-x11.h"
#include "src/time.h"

#include <time.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xinerama.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/xf86vmode.h>

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_linux_graphics_fs_minimize)

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

/* XIfEvent() helper for create_focus_window(). */
static Window match_window_value;
static Bool match_window(UNUSED Display *display, XEvent *event,
                         UNUSED char *userdata)
    {return event->xany.window == match_window_value;}

/**
 * create_focus_window:  Create and focus a dummy X11 window to trigger a
 * FocusOut event on the SIL window.  The dummy window is closed on return.
 *
 * [Return value]
 *     True on success, false if an error occurred.
 */
static int create_focus_window(void)
{
    struct timespec ts;
    ASSERT(clock_gettime(CLOCK_MONOTONIC, &ts) == 0);
    const double start = ts.tv_sec + ts.tv_nsec*1e-9;

    Display *display = linux_x11_display();
    Window root = DefaultRootWindow(display);
    XVisualInfo template;
    mem_clear(&template, sizeof(template));
    template.class = TrueColor;
    XVisualInfo *visi;
    ASSERT(visi = XGetVisualInfo(display, VisualClassMask,
                                 &template, (int[1]){0}));
    Colormap cmap;
    ASSERT(cmap = XCreateColormap(display, root, visi->visual, AllocNone));
    Window window;
    ASSERT(window = XCreateWindow(
               display, root, 0, 0, TESTW, TESTH, 0, visi->depth, InputOutput,
               visi->visual, CWColormap | CWEventMask,
               &(XSetWindowAttributes){
                   .colormap = cmap,
                   .event_mask = FocusChangeMask | StructureNotifyMask}));
    XFree(visi);

    XMapWindow(display, window);
    XEvent event;
    match_window_value = window;
    do {
        clock_gettime(CLOCK_MONOTONIC, &ts);
        const double now = ts.tv_sec + ts.tv_nsec*1e-9;
        if (now >= start+1) {
            DLOG("Timeout while mapping dummy window");
            XDestroyWindow(display, window);
            XFreeColormap(display, cmap);
            return 0;
        }
        XIfEvent(display, &event, match_window, NULL);
    } while (event.type != MapNotify);

    XSetInputFocus(display, window, RevertToNone,
                   CurrentTime);
    do {
        clock_gettime(CLOCK_MONOTONIC, &ts);
        const double now = ts.tv_sec + ts.tv_nsec*1e-9;
        if (now >= start+1) {
            DLOG("Timeout while mapping dummy window");
            XDestroyWindow(display, window);
            XFreeColormap(display, cmap);
            return 0;
        }
        XIfEvent(display, &event, match_window, NULL);
    } while (event.type != FocusIn);

    /* Give the SIL window a chance to notice the focus change before we
     * destroy the dummy window. */
    while (linux_get_window_event(&event)) { /*spin*/ }

    XDestroyWindow(display, window);
    XFreeColormap(display, cmap);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fullscreen_auto_minimize)
{
    int mode_width, mode_height;
    if (!get_alternate_video_mode(&mode_width, &mode_height)) {
        SKIP("No alternate video mode available.");
    }

    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(mode_width, mode_height, NULL));
    CHECK_FALSE(graphics_display_is_window());

    CHECK_TRUE(create_focus_window());

    CHECK_FALSE(graphics_has_focus());
    CHECK_INTEQUAL(called_XIconifyWindow, 1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fullscreen_auto_minimize_focus_in_out)
{
    int mode_width, mode_height;
    if (!get_alternate_video_mode(&mode_width, &mode_height)) {
        SKIP("No alternate video mode available.");
    }

    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(mode_width, mode_height, NULL));
    CHECK_FALSE(graphics_display_is_window());

    /* We can't guarantee that creating and then immediately destroying a
     * window will return focus to the SIL window, so for this test we
     * just synthesize FocusOut and FocusIn events directly. */
    XEvent event = {.xfocus = {
        .type = FocusOut,
        .display = linux_x11_display(),
        .window = linux_x11_window(),
        .mode = NotifyNormal,
        .detail = NotifyDetailNone,
    }};
    CHECK_TRUE(XSendEvent(event.xfocus.display, event.xfocus.window, False, 0,
                          (XEvent *)&event));
    event.xfocus.type = FocusIn;
    CHECK_TRUE(XSendEvent(event.xfocus.display, event.xfocus.window, False, 0,
                          (XEvent *)&event));
    XSync(event.xfocus.display, False);

    while (linux_get_window_event(&event)) { /*spin*/ }
    CHECK_TRUE(graphics_has_focus());
    CHECK_INTEQUAL(called_XIconifyWindow, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fullscreen_auto_minimize_same_mode)
{
    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(graphics_device_width(),
                                         graphics_device_height(), NULL));
    CHECK_FALSE(graphics_display_is_window());

    CHECK_TRUE(create_focus_window());

    CHECK_FALSE(graphics_has_focus());
    /* The window should not auto-minimize if it's using the default mode. */
    CHECK_INTEQUAL(called_XIconifyWindow, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fullscreen_auto_minimize_focus_follows_mouse)
{
    int mode_width, mode_height;
    if (!get_alternate_video_mode(&mode_width, &mode_height)) {
        SKIP("No alternate video mode available.");
    }

    CHECK_TRUE(graphics_set_display_attr("window", 1));
    CHECK_TRUE(graphics_set_display_mode(mode_width, mode_height, NULL));
    CHECK_TRUE(graphics_display_is_window());

    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(mode_width, mode_height, NULL));
    CHECK_FALSE(graphics_display_is_window());

    CHECK_TRUE(create_focus_window());

    CHECK_FALSE(graphics_has_focus());
    /* The window should not have been minimized because it lost focus
     * immediately after a switch to fullscreen mode, which we treat as
     * probably a transient loss of focus due to focus-follows-mouse rules. */
    CHECK_INTEQUAL(called_XIconifyWindow, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fullscreen_auto_minimize_explicit_disable)
{
    int mode_width, mode_height;
    if (!get_alternate_video_mode(&mode_width, &mode_height)) {
        SKIP("No alternate video mode available.");
    }

    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(mode_width, mode_height, NULL));
    CHECK_FALSE(graphics_display_is_window());

    CHECK_TRUE(graphics_set_display_attr("fullscreen_minimize_on_focus_loss", 0));
    CHECK_TRUE(create_focus_window());

    CHECK_FALSE(graphics_has_focus());
    CHECK_INTEQUAL(called_XIconifyWindow, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fullscreen_auto_minimize_explicit_enable)
{
    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(graphics_device_width(),
                                         graphics_device_height(), NULL));
    CHECK_FALSE(graphics_display_is_window());

    CHECK_TRUE(graphics_set_display_attr("fullscreen_minimize_on_focus_loss", 1));
    CHECK_TRUE(create_focus_window());

    CHECK_FALSE(graphics_has_focus());
    CHECK_INTEQUAL(called_XIconifyWindow, 1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fullscreen_auto_minimize_SDL_fallback_false)
{
    int mode_width, mode_height;
    if (!get_alternate_video_mode(&mode_width, &mode_height)) {
        SKIP("No alternate video mode available.");
    }

    static const char * const false_strings[] = {"0", "false", "FALSE"};
    for (int i = 0; i < lenof(false_strings); i++) {
        ASSERT(setenv("SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS",
                      false_strings[i], 1) == 0);

        CHECK_TRUE(graphics_set_display_attr("window", 0));
        CHECK_TRUE(graphics_set_display_mode(mode_width, mode_height, NULL));
        CHECK_FALSE(graphics_display_is_window());

        CHECK_TRUE(create_focus_window());

        CHECK_FALSE(graphics_has_focus());
        CHECK_INTEQUAL(called_XIconifyWindow, 0);

        linux_close_window();
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fullscreen_auto_minimize_SDL_fallback_true)
{
    static const char * const false_strings[] = {"1", "true", "0 ", "false "};
    for (int i = 0; i < lenof(false_strings); i++) {
        ASSERT(setenv("SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS",
                      false_strings[i], 1) == 0);
        called_XIconifyWindow = 0;

        CHECK_TRUE(graphics_set_display_attr("window", 0));
        CHECK_TRUE(graphics_set_display_mode(graphics_device_width(),
                                             graphics_device_height(), NULL));
        CHECK_FALSE(graphics_display_is_window());

        CHECK_TRUE(create_focus_window());

        CHECK_FALSE(graphics_has_focus());
        CHECK_INTEQUAL(called_XIconifyWindow, 1);

        linux_close_window();
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fullscreen_auto_minimize_SDL_fallback_empty)
{
    int mode_width, mode_height;
    if (!get_alternate_video_mode(&mode_width, &mode_height)) {
        SKIP("No alternate video mode available.");
    }

    ASSERT(setenv("SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS", "", 1) == 0);
    CHECK_TRUE(graphics_set_display_attr("window", 0));

    CHECK_TRUE(graphics_set_display_mode(mode_width, mode_height, NULL));
    CHECK_FALSE(graphics_display_is_window());
    CHECK_TRUE(create_focus_window());
    CHECK_FALSE(graphics_has_focus());
    CHECK_INTEQUAL(called_XIconifyWindow, 1);
    linux_close_window();

    called_XIconifyWindow = 0;
    CHECK_TRUE(graphics_set_display_mode(graphics_device_width(),
                                         graphics_device_height(), NULL));
    CHECK_FALSE(graphics_display_is_window());
    CHECK_TRUE(create_focus_window());
    CHECK_FALSE(graphics_has_focus());
    CHECK_INTEQUAL(called_XIconifyWindow, 0);

    return 1;
}

/*************************************************************************/
/*************************************************************************/
