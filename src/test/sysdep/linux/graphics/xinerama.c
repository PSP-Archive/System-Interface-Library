/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/linux/graphics/fs-minimize.c: Linux-specific graphics
 * tests covering Xinerama-related functionality.
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

DEFINE_GENERIC_TEST_RUNNER(test_linux_graphics_xinerama)

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

TEST(test_xinerama_disabled_NOINIT)
{
    disable_XineramaQueryExtension = 1;
    CHECK_TRUE(graphics_init());
    disable_XineramaQueryExtension = 0;

    Display *display = linux_x11_display();
    CHECK_INTEQUAL(graphics_num_devices(), ScreenCount(display));
    CHECK_INTEQUAL(called_XineramaIsActive, 0);
    CHECK_INTEQUAL(called_XineramaQueryScreens, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_xinerama_not_active_NOINIT)
{
    disable_XineramaIsActive = 1;
    CHECK_TRUE(graphics_init());
    disable_XineramaIsActive = 0;

    Display *display = linux_x11_display();
    CHECK_INTEQUAL(graphics_num_devices(), ScreenCount(display));
    CHECK_INTEQUAL(called_XineramaIsActive, 1);
    CHECK_INTEQUAL(called_XineramaQueryScreens, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_xinerama_disabled_vidmode_fallback_NOINIT)
{
    disable_XineramaQueryExtension = 1;
    disable_XRRQueryExtension = 1;
    disable_XF86VidModeGetModeLine = 1;
    CHECK_TRUE(graphics_init());
    disable_XineramaQueryExtension = 0;
    disable_XRRQueryExtension = 0;
    disable_XF86VidModeGetModeLine = 0;

    if (!check_vidmode()) {
        SKIP("XF86VidMode not found.");
    }

    Display *display = linux_x11_display();
    CHECK_INTEQUAL(graphics_num_devices(), ScreenCount(display));
    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(1));
    CHECK_INTEQUAL(mode_list->num_modes, graphics_num_devices());

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_xinerama_fullscreen_XMoveWindow_NOINIT)
{
    disable_XRRQueryExtension = 1;
    CHECK_TRUE(graphics_init());
    disable_XRRQueryExtension = 0;

    if (!check_xinerama()) {
        SKIP("Xinerama not found.");
    }

    ASSERT(setenv("SIL_X11_FULLSCREEN_METHOD", "XMOVEWINDOW", 1) == 0);
    ASSERT(setenv("SIL_X11_CREATE_FULLSCREEN", "1", 1) == 0);

    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(graphics_device_width(),
                                         graphics_device_height(), NULL));

    called_XCreateWindow = 0;
    called_XMoveWindow = 0;
    called_XineramaQueryScreens = 0;
    CHECK_TRUE(graphics_set_display_attr("window", 1));
    CHECK_TRUE(graphics_set_display_mode(graphics_device_width(),
                                         graphics_device_height(), NULL));
    CHECK_INTEQUAL(called_XCreateWindow, 0);
    CHECK_INTEQUAL(called_XMoveWindow, 1);
    CHECK_INTEQUAL(called_XineramaQueryScreens, 1);

    return 1;
}

/*************************************************************************/
/*************************************************************************/
