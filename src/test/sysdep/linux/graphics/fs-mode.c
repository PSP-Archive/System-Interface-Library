/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/linux/graphics/fs-mode.c: Linux-specific graphics tests
 * covering display mode changing for fullscreen windows.
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

DEFINE_GENERIC_TEST_RUNNER(test_linux_graphics_fs_mode)

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

TEST(test_fullscreen_vidmode_NOINIT)
{
    disable_XRRQueryExtension = 1;
    CHECK_TRUE(graphics_init());
    disable_XRRQueryExtension = 0;

    if (!check_vidmode()) {
        SKIP("XF86VidMode not found.");
    }

    int saved_x, saved_y;
    get_mouse_position(&saved_x, &saved_y);

    int mode_width, mode_height;
    if (!get_alternate_video_mode(&mode_width, &mode_height)) {
        SKIP("No alternate video mode available.");
    }

    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(mode_width, mode_height, NULL));
    CHECK_FALSE(graphics_display_is_window());
    CHECK_INTEQUAL(graphics_display_width(), mode_width);
    CHECK_INTEQUAL(graphics_display_height(), mode_height);
    CHECK_INTEQUAL(called_XRRSetCrtcConfig, 0);
    CHECK_INTEQUAL(called_XRRGetPanning, 0);

    CHECK_TRUE(graphics_set_display_attr("window", 1));
    CHECK_TRUE(graphics_set_display_mode(TESTW, TESTH, NULL));
    set_mouse_position(saved_x, saved_y);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fullscreen_xrandr_NOCLEANUP)
{
    int saved_x, saved_y;
    get_mouse_position(&saved_x, &saved_y);

    if (!check_xrandr()) {
        graphics_cleanup();
        SKIP("XRandR not found or too old.");
    }

    int mode_width, mode_height;
    if (!get_alternate_video_mode(&mode_width, &mode_height)) {
        graphics_cleanup();
        SKIP("No alternate video mode available.");
    }

    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(mode_width, mode_height, NULL));
    CHECK_FALSE(graphics_display_is_window());
    CHECK_INTEQUAL(graphics_display_width(), mode_width);
    CHECK_INTEQUAL(graphics_display_height(), mode_height);
    CHECK_TRUE(called_XRRSetCrtcConfig > 0);
    CHECK_TRUE(called_XRRGetPanning > 0);

    graphics_cleanup();
    set_mouse_position(saved_x, saved_y);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fullscreen_xrandr_memory_failure_NOCLEANUP)
{
    int saved_x, saved_y;
    get_mouse_position(&saved_x, &saved_y);

    if (!check_xrandr()) {
        graphics_cleanup();
        SKIP("XRandR not found or too old.");
    }

    int mode_width, mode_height;
    if (!get_alternate_video_mode(&mode_width, &mode_height)) {
        graphics_cleanup();
        SKIP("No alternate video mode available.");
    }

    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_MEMORY_FAILURES(graphics_set_display_mode(mode_width, mode_height,
                                                    NULL));
    CHECK_FALSE(graphics_display_is_window());
    CHECK_INTEQUAL(graphics_display_width(), mode_width);
    CHECK_INTEQUAL(graphics_display_height(), mode_height);
    CHECK_TRUE(called_XRRSetCrtcConfig > 0);
    CHECK_INTEQUAL(called_XRRGetPanning, 0);

    graphics_cleanup();
    set_mouse_position(saved_x, saved_y);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fullscreen_xrandr_XRRGetScreenResources_failure)
{
    if (!check_xrandr()) {
        SKIP("XRandR not found or too old.");
    }

    int mode_width, mode_height;
    if (!get_alternate_video_mode(&mode_width, &mode_height)) {
        SKIP("No alternate video mode available.");
    }

    CHECK_TRUE(graphics_set_display_attr("window", 0));
    disable_XRRGetScreenResources = 1;
    GraphicsError error;
    CHECK_FALSE(graphics_set_display_mode(mode_width, mode_height, &error));
    disable_XRRGetScreenResources = 0;
    CHECK_INTEQUAL(error, GRAPHICS_ERROR_MODE_NOT_SUPPORTED);
    CHECK_INTEQUAL(called_XRRSetCrtcConfig, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fullscreen_xrandr_XRRGetOutputInfo_failure)
{
    if (!check_xrandr()) {
        SKIP("XRandR not found or too old.");
    }

    int mode_width, mode_height;
    if (!get_alternate_video_mode(&mode_width, &mode_height)) {
        SKIP("No alternate video mode available.");
    }

    CHECK_TRUE(graphics_set_display_attr("window", 0));
    disable_XRRGetOutputInfo = 1;
    GraphicsError error;
    CHECK_FALSE(graphics_set_display_mode(mode_width, mode_height, &error));
    disable_XRRGetOutputInfo = 0;
    CHECK_INTEQUAL(error, GRAPHICS_ERROR_MODE_NOT_SUPPORTED);
    CHECK_INTEQUAL(called_XRRSetCrtcConfig, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fullscreen_xrandr_XRRGetCrtcInfo_failure)
{
    if (!check_xrandr()) {
        SKIP("XRandR not found or too old.");
    }

    int mode_width, mode_height;
    if (!get_alternate_video_mode(&mode_width, &mode_height)) {
        SKIP("No alternate video mode available.");
    }

    CHECK_TRUE(graphics_set_display_attr("window", 0));
    disable_XRRGetCrtcInfo = 1;
    GraphicsError error;
    CHECK_FALSE(graphics_set_display_mode(mode_width, mode_height, &error));
    disable_XRRGetCrtcInfo = 0;
    CHECK_INTEQUAL(error, GRAPHICS_ERROR_MODE_NOT_SUPPORTED);
    CHECK_INTEQUAL(called_XRRSetCrtcConfig, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fullscreen_xrandr_v1_2_no_panning_NOINIT_NOCLEANUP)
{
    xrandr_version_major = 1;
    xrandr_version_minor = 2;
    CHECK_TRUE(graphics_init());
    xrandr_version_major = 0;
    xrandr_version_minor = 0;

    if (!check_xrandr()) {
        graphics_cleanup();
        SKIP("XRandR not found or too old.");
    }

    int saved_x, saved_y;
    get_mouse_position(&saved_x, &saved_y);

    int mode_width, mode_height;
    if (!get_alternate_video_mode(&mode_width, &mode_height)) {
        graphics_cleanup();
        SKIP("No alternate video mode available.");
    }

    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(mode_width, mode_height, NULL));
    CHECK_FALSE(graphics_display_is_window());
    CHECK_INTEQUAL(graphics_display_width(), mode_width);
    CHECK_INTEQUAL(graphics_display_height(), mode_height);
    CHECK_INTRANGE(called_XRRSetCrtcConfig, 1, 2);
    CHECK_INTEQUAL(called_XRRGetPanning, 0);

    graphics_cleanup();
    set_mouse_position(saved_x, saved_y);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fullscreen_switch_failure)
{
    if (!check_xrandr()) {
        SKIP("XRandR not found or too old.");
    }

    int mode_width, mode_height;
    if (!get_alternate_video_mode(&mode_width, &mode_height)) {
        SKIP("No alternate video mode available.");
    }

    CHECK_TRUE(graphics_set_display_attr("window", 1));
    CHECK_TRUE(graphics_set_display_mode(mode_width, mode_height, NULL));
    CHECK_TRUE(graphics_display_is_window());

    CHECK_TRUE(graphics_set_display_attr("window", 0));
    disable_XRRGetCrtcInfo = 1;
    GraphicsError error;
    CHECK_FALSE(graphics_set_display_mode(mode_width, mode_height, &error));
    disable_XRRGetCrtcInfo = 0;
    CHECK_INTEQUAL(error, GRAPHICS_ERROR_MODE_NOT_SUPPORTED);
    /* The old window should still be open. */
    CHECK_TRUE(graphics_display_is_window());

    return 1;
}

/*************************************************************************/
/*************************************************************************/
