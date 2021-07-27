/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/linux/graphics/modes.c: Linux-specific graphics tests
 * covering display mode management.
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

DEFINE_GENERIC_TEST_RUNNER(test_linux_graphics_modes)

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

/* This doesn't test anything per se, but it logs a skip message if only
 * one screen is available so the user knows some code will not be
 * functionally tested. */
TEST(test_multiple_screens)
{
    if (graphics_num_devices() == 1) {
        SKIP("Only one display device is available; some tests in this"
             " file may spuriously pass.");
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_no_video_mode_interface_NOINIT)
{
    disable_XF86VidModeQueryExtension = 1;
    disable_XRRQueryExtension = 1;
    CHECK_TRUE(graphics_init());
    disable_XF86VidModeQueryExtension = 0;
    disable_XRRQueryExtension = 0;

    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(1));
    CHECK_TRUE(mode_list->num_modes > 0);
    DLOG("Available display modes (VidMode/XRandR disabled):");
    const GraphicsDisplayModeEntry *modes = mode_list->modes;
    for (int i = 0; i < mode_list->num_modes; i++) {
        DLOG("   Display %d (%s): %dx%d (%.4gHz)", modes[i].device,
             modes[i].device_name, modes[i].width, modes[i].height,
             modes[i].refresh);
        /* All modes should have an unknown refresh rate. */
        CHECK_FLOATEQUAL(modes[i].refresh, 0);
        if (i >= 1) {
            /* There should only be one mode (the current mode) per screen. */
            CHECK_TRUE(modes[i].device == modes[i-1].device + 1);
        }
    }
    CHECK_INTEQUAL(mode_list->num_modes, graphics_num_devices());
    CHECK_INTEQUAL(called_XF86VidModeGetModeLine, 0);
    CHECK_INTEQUAL(called_XF86VidModeGetAllModeLines, 0);
    CHECK_INTEQUAL(called_XRRGetScreenResources, 0);
    CHECK_INTEQUAL(called_XRRGetOutputInfo, 0);
    CHECK_INTEQUAL(called_XRRGetCrtcInfo, 0);

    /* graphics_frame_period() should return zero if we have no video mode
     * interface. */
    CHECK_TRUE(graphics_set_display_mode(TESTW, TESTH, NULL));
    CHECK_FLOATEQUAL(graphics_frame_period(), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_no_video_mode_interface_memory_failure_NOINIT)
{
    disable_XF86VidModeQueryExtension = 1;
    disable_XRRQueryExtension = 1;
    CHECK_MEMORY_FAILURES(graphics_init());
    disable_XF86VidModeQueryExtension = 0;
    disable_XRRQueryExtension = 0;

    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(1));
    CHECK_TRUE(mode_list->num_modes > 0);
    DLOG("Available display modes (interface=NONE):");
    const GraphicsDisplayModeEntry *modes = mode_list->modes;
    for (int i = 0; i < mode_list->num_modes; i++) {
        DLOG("   Display %d (%s): %dx%d (%.4gHz)", modes[i].device,
             modes[i].device_name, modes[i].width, modes[i].height,
             modes[i].refresh);
        if (i >= 1) {
            CHECK_TRUE(modes[i].device == modes[i-1].device + 1);
        }
    }
    CHECK_INTEQUAL(mode_list->num_modes, graphics_num_devices());
    CHECK_INTEQUAL(called_XF86VidModeGetModeLine, 0);
    CHECK_INTEQUAL(called_XF86VidModeGetAllModeLines, 0);
    CHECK_INTEQUAL(called_XRRGetScreenResources, 0);
    CHECK_INTEQUAL(called_XRRGetOutputInfo, 0);
    CHECK_INTEQUAL(called_XRRGetCrtcInfo, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_vidmode_NOINIT)
{
    disable_XRRQueryExtension = 1;
    CHECK_TRUE(graphics_init());
    disable_XRRQueryExtension = 0;

    if (!check_vidmode()) {
        SKIP("XF86VidMode not found.");
    }

    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(1));
    CHECK_TRUE(mode_list->num_modes > 0);
    DLOG("Available display modes (XF86VidMode):");
    const GraphicsDisplayModeEntry *modes = mode_list->modes;
    int screen_1_mode = -1;
    for (int i = 0; i < mode_list->num_modes; i++) {
        DLOG("   Display %d (%s): %dx%d (%.4gHz)", modes[i].device,
             modes[i].device_name, modes[i].width, modes[i].height,
             modes[i].refresh);
        if (modes[i].device > 0) {
            /* There should only be one mode (the current mode) for the
             * second and later screens. */
            if (modes[i].device == 1) {
                CHECK_TRUE(screen_1_mode == -1);
                screen_1_mode = i;
            }
            CHECK_TRUE(modes[i].device == modes[i-1].device + 1);
        }
    }
    if (screen_1_mode >= 0) {
        CHECK_INTEQUAL(mode_list->num_modes,
                       screen_1_mode + (graphics_num_devices() - 1));
    }
    CHECK_TRUE(called_XF86VidModeGetModeLine > 0);
    CHECK_TRUE(called_XF86VidModeGetAllModeLines > 0);
    CHECK_INTEQUAL(called_XRRGetScreenResources, 0);
    CHECK_INTEQUAL(called_XRRGetOutputInfo, 0);
    CHECK_INTEQUAL(called_XRRGetCrtcInfo, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_vidmode_memory_failure_NOINIT)
{
    disable_XRRQueryExtension = 1;
    CHECK_TRUE(graphics_init());
    disable_XRRQueryExtension = 0;

    if (!check_vidmode()) {
        SKIP("XF86VidMode not found.");
    }

    GraphicsDisplayModeEntry *orig_modes;
    int orig_num_modes;
    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(1));
    CHECK_TRUE(mode_list->num_modes > 0);
    orig_num_modes = mode_list->num_modes;
    ASSERT(orig_modes = mem_alloc(sizeof(*orig_modes) * orig_num_modes, 0, 0));
    memcpy(orig_modes, mode_list->modes, sizeof(*orig_modes) * orig_num_modes);

    graphics_cleanup();
    disable_XRRQueryExtension = 1;
    CHECK_MEMORY_FAILURES(graphics_init());
    disable_XRRQueryExtension = 0;

    CHECK_TRUE(mode_list = graphics_list_display_modes(1));
    DLOG("Available display modes (XF86VidMode, memory failures):");
    const GraphicsDisplayModeEntry *modes = mode_list->modes;
    for (int i = 0; i < mode_list->num_modes; i++) {
        DLOG("   Display %d (%s): %dx%d (%.4gHz)", modes[i].device,
             modes[i].device_name, modes[i].width, modes[i].height,
             modes[i].refresh);
        CHECK_INTEQUAL(modes[i].device, orig_modes[i].device);
        CHECK_INTEQUAL(modes[i].width, orig_modes[i].width);
        CHECK_INTEQUAL(modes[i].height, orig_modes[i].height);
        CHECK_FLOATEQUAL(modes[i].refresh, orig_modes[i].refresh);
    }
    CHECK_INTEQUAL(mode_list->num_modes, orig_num_modes);
    CHECK_TRUE(called_XF86VidModeGetModeLine > 0);
    CHECK_TRUE(called_XF86VidModeGetAllModeLines > 0);
    CHECK_INTEQUAL(called_XRRGetScreenResources, 0);
    CHECK_INTEQUAL(called_XRRGetOutputInfo, 0);
    CHECK_INTEQUAL(called_XRRGetCrtcInfo, 0);

    mem_free(orig_modes);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_XF86VidModeGetModeLine_failure_NOINIT)
{
    disable_XRRQueryExtension = 1;
    disable_XF86VidModeGetModeLine = 1;
    CHECK_TRUE(graphics_init());
    disable_XRRQueryExtension = 0;
    disable_XF86VidModeGetModeLine = 0;

    if (!check_vidmode()) {
        SKIP("XF86VidMode not found.");
    }

    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(1));
    CHECK_INTEQUAL(mode_list->num_modes, graphics_num_devices());
    CHECK_TRUE(called_XF86VidModeGetModeLine > 0);
    CHECK_INTEQUAL(called_XF86VidModeGetAllModeLines, 0);
    CHECK_INTEQUAL(called_XRRGetScreenResources, 0);
    CHECK_INTEQUAL(called_XRRGetOutputInfo, 0);
    CHECK_INTEQUAL(called_XRRGetCrtcInfo, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_XF86VidModeGetAllModeLines_failure_NOINIT)
{
    disable_XRRQueryExtension = 1;
    disable_XF86VidModeGetAllModeLines = 1;
    CHECK_TRUE(graphics_init());
    disable_XRRQueryExtension = 0;
    disable_XF86VidModeGetAllModeLines = 0;

    if (!check_vidmode()) {
        SKIP("XF86VidMode not found.");
    }

    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(1));
    CHECK_INTEQUAL(mode_list->num_modes, graphics_num_devices());
    CHECK_TRUE(called_XF86VidModeGetModeLine > 0);
    CHECK_INTEQUAL(called_XF86VidModeGetAllModeLines,
                   called_XF86VidModeGetModeLine);
    CHECK_INTEQUAL(called_XRRGetScreenResources, 0);
    CHECK_INTEQUAL(called_XRRGetOutputInfo, 0);
    CHECK_INTEQUAL(called_XRRGetCrtcInfo, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_vidmode_fallback_memory_failure_NOINIT)
{
    disable_XRRQueryExtension = 1;
    disable_XF86VidModeGetModeLine = 1;
    CHECK_MEMORY_FAILURES(graphics_init());
    disable_XRRQueryExtension = 0;
    disable_XF86VidModeGetModeLine = 0;

    if (!check_vidmode()) {
        SKIP("XF86VidMode not found.");
    }

    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(1));
    CHECK_INTEQUAL(mode_list->num_modes, graphics_num_devices());
    CHECK_TRUE(called_XF86VidModeGetModeLine > 0);
    CHECK_INTEQUAL(called_XF86VidModeGetAllModeLines, 0);
    CHECK_INTEQUAL(called_XRRGetScreenResources, 0);
    CHECK_INTEQUAL(called_XRRGetOutputInfo, 0);
    CHECK_INTEQUAL(called_XRRGetCrtcInfo, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_xrandr)
{
    if (!check_xrandr()) {
        SKIP("XRandR not found or too old.");
    }

    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(1));
    CHECK_TRUE(mode_list->num_modes > 0);
    DLOG("Available display modes (XRandR):");
    const GraphicsDisplayModeEntry *modes = mode_list->modes;
    CHECK_TRUE(mode_list->num_modes > 0);
    for (int i = 0; i < mode_list->num_modes; i++) {
        DLOG("   Display %d (%s): %dx%d (%.4gHz)", modes[i].device,
             modes[i].device_name, modes[i].width, modes[i].height,
             modes[i].refresh);
    }
    CHECK_INTEQUAL(called_XF86VidModeGetModeLine, 0);
    CHECK_INTEQUAL(called_XF86VidModeGetAllModeLines, 0);
    CHECK_TRUE(called_XRRGetScreenResources > 0);
    CHECK_TRUE(called_XRRGetOutputInfo > 0);
    CHECK_TRUE(called_XRRGetCrtcInfo > 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_xrandr_memory_failure)
{
    if (!check_xrandr()) {
        SKIP("XRandR not found or too old.");
    }

    GraphicsDisplayModeEntry *orig_modes;
    int orig_num_modes;
    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(1));
    CHECK_TRUE(mode_list->num_modes > 0);
    orig_num_modes = mode_list->num_modes;
    ASSERT(orig_modes = mem_alloc(sizeof(*orig_modes) * orig_num_modes, 0, 0));
    memcpy(orig_modes, mode_list->modes, sizeof(*orig_modes) * orig_num_modes);

    graphics_cleanup();
    CHECK_MEMORY_FAILURES(graphics_init());

    CHECK_TRUE(mode_list = graphics_list_display_modes(1));
    CHECK_TRUE(mode_list->num_modes > 0);
    DLOG("Available display modes (XRandR, memory failures):");
    const GraphicsDisplayModeEntry *modes = mode_list->modes;
    for (int i = 0; i < mode_list->num_modes; i++) {
        DLOG("   Display %d (%s): %dx%d (%.4gHz)", modes[i].device,
             modes[i].device_name, modes[i].width, modes[i].height,
             modes[i].refresh);
        CHECK_INTEQUAL(modes[i].device, orig_modes[i].device);
        CHECK_INTEQUAL(modes[i].width, orig_modes[i].width);
        CHECK_INTEQUAL(modes[i].height, orig_modes[i].height);
        CHECK_FLOATEQUAL(modes[i].refresh, orig_modes[i].refresh);
    }
    CHECK_INTEQUAL(mode_list->num_modes, orig_num_modes);
    CHECK_INTEQUAL(called_XF86VidModeGetModeLine, 0);
    CHECK_INTEQUAL(called_XF86VidModeGetAllModeLines, 0);
    CHECK_TRUE(called_XRRGetScreenResources > 0);
    CHECK_TRUE(called_XRRGetOutputInfo > 0);
    CHECK_TRUE(called_XRRGetCrtcInfo > 0);

    mem_free(orig_modes);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_XRRQueryVersion_failure_NOINIT)
{
    disable_XF86VidModeQueryExtension = 1;
    disable_XRRQueryVersion = 1;
    CHECK_TRUE(graphics_init());
    disable_XF86VidModeQueryExtension = 0;
    disable_XRRQueryVersion = 0;

    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(1));
    CHECK_INTEQUAL(mode_list->num_modes, graphics_num_devices());
    CHECK_INTEQUAL(called_XF86VidModeGetModeLine, 0);
    CHECK_INTEQUAL(called_XF86VidModeGetAllModeLines, 0);
    CHECK_INTEQUAL(called_XRRGetScreenResources, 0);
    CHECK_INTEQUAL(called_XRRGetOutputInfo, 0);
    CHECK_INTEQUAL(called_XRRGetCrtcInfo, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_xrandr_version_too_old_NOINIT)
{
    disable_XF86VidModeQueryExtension = 1;
    xrandr_version_major = 0;
    xrandr_version_minor = 9;
    CHECK_TRUE(graphics_init());
    disable_XF86VidModeQueryExtension = 0;
    xrandr_version_major = 0;
    xrandr_version_minor = 0;

    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(1));
    CHECK_INTEQUAL(mode_list->num_modes, graphics_num_devices());
    CHECK_INTEQUAL(called_XF86VidModeGetModeLine, 0);
    CHECK_INTEQUAL(called_XF86VidModeGetAllModeLines, 0);
    CHECK_INTEQUAL(called_XRRGetScreenResources, 0);
    CHECK_INTEQUAL(called_XRRGetOutputInfo, 0);
    CHECK_INTEQUAL(called_XRRGetCrtcInfo, 0);

    graphics_cleanup();
    disable_XF86VidModeQueryExtension = 1;
    xrandr_version_major = 1;
    xrandr_version_minor = 1;
    CHECK_TRUE(graphics_init());
    disable_XF86VidModeQueryExtension = 0;
    xrandr_version_major = 0;
    xrandr_version_minor = 0;

    CHECK_TRUE(mode_list = graphics_list_display_modes(1));
    CHECK_INTEQUAL(mode_list->num_modes, graphics_num_devices());
    CHECK_INTEQUAL(called_XF86VidModeGetModeLine, 0);
    CHECK_INTEQUAL(called_XF86VidModeGetAllModeLines, 0);
    CHECK_INTEQUAL(called_XRRGetScreenResources, 0);
    CHECK_INTEQUAL(called_XRRGetOutputInfo, 0);
    CHECK_INTEQUAL(called_XRRGetCrtcInfo, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_XRRGetScreenResources_failure_NOINIT)
{
    disable_XRRGetScreenResources = 1;
    CHECK_TRUE(graphics_init());
    disable_XRRGetScreenResources = 0;

    if (!check_xrandr()) {
        SKIP("XRandR not found or too old.");
    }

    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(1));
    CHECK_INTEQUAL(mode_list->num_modes, graphics_num_devices());
    CHECK_INTEQUAL(called_XF86VidModeGetModeLine, 0);
    CHECK_INTEQUAL(called_XF86VidModeGetAllModeLines, 0);
    CHECK_TRUE(called_XRRGetScreenResources > 0);
    CHECK_INTEQUAL(called_XRRGetOutputInfo, 0);
    CHECK_INTEQUAL(called_XRRGetCrtcInfo, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_XRRGetOutputInfo_failure_NOINIT)
{
    disable_XRRGetOutputInfo = 1;
    CHECK_TRUE(graphics_init());
    disable_XRRGetOutputInfo = 0;

    if (!check_xrandr()) {
        SKIP("XRandR not found or too old.");
    }

    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(1));
    CHECK_INTEQUAL(mode_list->num_modes, graphics_num_devices());
    CHECK_INTEQUAL(called_XF86VidModeGetModeLine, 0);
    CHECK_INTEQUAL(called_XF86VidModeGetAllModeLines, 0);
    CHECK_TRUE(called_XRRGetScreenResources > 0);
    CHECK_TRUE(called_XRRGetOutputInfo > 0);
    CHECK_INTEQUAL(called_XRRGetCrtcInfo, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_XRRGetCrtcInfo_failure_NOINIT)
{
    disable_XRRGetCrtcInfo = 1;
    CHECK_TRUE(graphics_init());
    disable_XRRGetCrtcInfo = 0;

    if (!check_xrandr()) {
        SKIP("XRandR not found or too old.");
    }

    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(1));
    CHECK_INTEQUAL(mode_list->num_modes, graphics_num_devices());
    CHECK_INTEQUAL(called_XF86VidModeGetModeLine, 0);
    CHECK_INTEQUAL(called_XF86VidModeGetAllModeLines, 0);
    CHECK_TRUE(called_XRRGetScreenResources > 0);
    CHECK_TRUE(called_XRRGetOutputInfo > 0);
    CHECK_TRUE(called_XRRGetCrtcInfo > 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_xrandr_fallback_memory_failure_NOINIT)
{
    disable_XRRGetCrtcInfo = 1;
    CHECK_MEMORY_FAILURES(graphics_init());
    disable_XRRGetCrtcInfo = 0;

    if (!check_xrandr()) {
        SKIP("XRandR not found or too old.");
    }

    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(1));
    CHECK_INTEQUAL(mode_list->num_modes, graphics_num_devices());
    CHECK_INTEQUAL(called_XF86VidModeGetModeLine, 0);
    CHECK_INTEQUAL(called_XF86VidModeGetAllModeLines, 0);
    CHECK_TRUE(called_XRRGetScreenResources > 0);
    CHECK_TRUE(called_XRRGetOutputInfo > 0);
    CHECK_TRUE(called_XRRGetCrtcInfo > 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_env_video_mode_none_NOINIT)
{
    ASSERT(setenv("SIL_X11_VIDEO_MODE_INTERFACE", "NONE", 1) == 0);
    CHECK_TRUE(graphics_init());

    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(1));
    CHECK_TRUE(mode_list->num_modes > 0);
    DLOG("Available display modes (interface=NONE):");
    const GraphicsDisplayModeEntry *modes = mode_list->modes;
    for (int i = 0; i < mode_list->num_modes; i++) {
        DLOG("   Display %d (%s): %dx%d (%.4gHz)", modes[i].device,
             modes[i].device_name, modes[i].width, modes[i].height,
             modes[i].refresh);
        if (i >= 1) {
            CHECK_TRUE(modes[i].device == modes[i-1].device + 1);
        }
    }
    CHECK_INTEQUAL(mode_list->num_modes, graphics_num_devices());
    CHECK_INTEQUAL(called_XF86VidModeGetModeLine, 0);
    CHECK_INTEQUAL(called_XF86VidModeGetAllModeLines, 0);
    CHECK_INTEQUAL(called_XRRGetScreenResources, 0);
    CHECK_INTEQUAL(called_XRRGetOutputInfo, 0);
    CHECK_INTEQUAL(called_XRRGetCrtcInfo, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_env_video_mode_vidmode_NOINIT)
{
    disable_XRRQueryExtension = 1;
    CHECK_TRUE(graphics_init());
    disable_XRRQueryExtension = 0;

    if (!check_vidmode()) {
        SKIP("XF86VidMode not found.");
    }

    GraphicsDisplayModeEntry *orig_modes;
    int orig_num_modes;
    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(1));
    CHECK_TRUE(mode_list->num_modes > 0);
    orig_num_modes = mode_list->num_modes;
    ASSERT(orig_modes = mem_alloc(sizeof(*orig_modes) * orig_num_modes, 0, 0));
    memcpy(orig_modes, mode_list->modes, sizeof(*orig_modes) * orig_num_modes);

    graphics_cleanup();
    ASSERT(setenv("SIL_X11_VIDEO_MODE_INTERFACE", "VIDMODE", 1) == 0);
    CHECK_TRUE(graphics_init());

    CHECK_TRUE(mode_list = graphics_list_display_modes(1));
    DLOG("Available display modes (interface=VIDMODE):");
    const GraphicsDisplayModeEntry *modes = mode_list->modes;
    for (int i = 0; i < mode_list->num_modes; i++) {
        DLOG("   Display %d (%s): %dx%d (%.4gHz)", modes[i].device,
             modes[i].device_name, modes[i].width, modes[i].height,
             modes[i].refresh);
        CHECK_INTEQUAL(modes[i].device, orig_modes[i].device);
        CHECK_INTEQUAL(modes[i].width, orig_modes[i].width);
        CHECK_INTEQUAL(modes[i].height, orig_modes[i].height);
        CHECK_FLOATEQUAL(modes[i].refresh, orig_modes[i].refresh);
    }
    CHECK_INTEQUAL(mode_list->num_modes, orig_num_modes);
    CHECK_TRUE(called_XF86VidModeGetModeLine > 0);
    CHECK_TRUE(called_XF86VidModeGetAllModeLines > 0);
    CHECK_INTEQUAL(called_XRRGetScreenResources, 0);
    CHECK_INTEQUAL(called_XRRGetOutputInfo, 0);
    CHECK_INTEQUAL(called_XRRGetCrtcInfo, 0);

    mem_free(orig_modes);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_env_video_mode_xrandr)
{
    ASSERT(!getenv("SIL_X11_VIDEO_MODE_INTERFACE"));
    if (!check_xrandr()) {
        SKIP("XRandR not found or too old.");
    }

    GraphicsDisplayModeEntry *orig_modes;
    int orig_num_modes;
    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(1));
    CHECK_TRUE(mode_list->num_modes > 0);
    orig_num_modes = mode_list->num_modes;
    ASSERT(orig_modes = mem_alloc(sizeof(*orig_modes) * orig_num_modes, 0, 0));
    memcpy(orig_modes, mode_list->modes, sizeof(*orig_modes) * orig_num_modes);

    graphics_cleanup();
    ASSERT(setenv("SIL_X11_VIDEO_MODE_INTERFACE", "XRANDR", 1) == 0);
    CHECK_TRUE(graphics_init());

    CHECK_TRUE(mode_list = graphics_list_display_modes(1));
    CHECK_TRUE(mode_list->num_modes > 0);
    DLOG("Available display modes (interface=XRANDR):");
    const GraphicsDisplayModeEntry *modes = mode_list->modes;
    for (int i = 0; i < mode_list->num_modes; i++) {
        DLOG("   Display %d (%s): %dx%d (%.4gHz)", modes[i].device,
             modes[i].device_name, modes[i].width, modes[i].height,
             modes[i].refresh);
        CHECK_INTEQUAL(modes[i].device, orig_modes[i].device);
        CHECK_INTEQUAL(modes[i].width, orig_modes[i].width);
        CHECK_INTEQUAL(modes[i].height, orig_modes[i].height);
        CHECK_FLOATEQUAL(modes[i].refresh, orig_modes[i].refresh);
    }
    CHECK_INTEQUAL(mode_list->num_modes, orig_num_modes);
    CHECK_INTEQUAL(called_XF86VidModeGetModeLine, 0);
    CHECK_INTEQUAL(called_XF86VidModeGetAllModeLines, 0);
    CHECK_TRUE(called_XRRGetScreenResources > 0);
    CHECK_TRUE(called_XRRGetOutputInfo > 0);
    CHECK_TRUE(called_XRRGetCrtcInfo > 0);

    mem_free(orig_modes);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_env_video_mode_empty)
{
    GraphicsDisplayModeEntry *orig_modes;
    int orig_num_modes;
    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(1));
    CHECK_TRUE(mode_list->num_modes > 0);
    orig_num_modes = mode_list->num_modes;
    ASSERT(orig_modes = mem_alloc(sizeof(*orig_modes) * orig_num_modes, 0, 0));
    memcpy(orig_modes, mode_list->modes, sizeof(*orig_modes) * orig_num_modes);

    graphics_cleanup();
    ASSERT(setenv("SIL_X11_VIDEO_MODE_INTERFACE", "", 1) == 0);
    CHECK_TRUE(graphics_init());

    CHECK_TRUE(mode_list = graphics_list_display_modes(1));
    CHECK_TRUE(mode_list->num_modes > 0);
    DLOG("Available display modes (interface=<invalid>):");
    const GraphicsDisplayModeEntry *modes = mode_list->modes;
    for (int i = 0; i < mode_list->num_modes; i++) {
        DLOG("   Display %d (%s): %dx%d (%.4gHz)", modes[i].device,
             modes[i].device_name, modes[i].width, modes[i].height,
             modes[i].refresh);
        CHECK_INTEQUAL(modes[i].device, orig_modes[i].device);
        CHECK_INTEQUAL(modes[i].width, orig_modes[i].width);
        CHECK_INTEQUAL(modes[i].height, orig_modes[i].height);
        CHECK_FLOATEQUAL(modes[i].refresh, orig_modes[i].refresh);
    }
    CHECK_INTEQUAL(mode_list->num_modes, orig_num_modes);

    mem_free(orig_modes);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_env_video_mode_invalid)
{
    GraphicsDisplayModeEntry *orig_modes;
    int orig_num_modes;
    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(1));
    CHECK_TRUE(mode_list->num_modes > 0);
    orig_num_modes = mode_list->num_modes;
    ASSERT(orig_modes = mem_alloc(sizeof(*orig_modes) * orig_num_modes, 0, 0));
    memcpy(orig_modes, mode_list->modes, sizeof(*orig_modes) * orig_num_modes);

    graphics_cleanup();
    /* This (invalid) setting should be ignored. */
    ASSERT(setenv("SIL_X11_VIDEO_MODE_INTERFACE", "<invalid>", 1) == 0);
    CHECK_TRUE(graphics_init());

    CHECK_TRUE(mode_list = graphics_list_display_modes(1));
    CHECK_TRUE(mode_list->num_modes > 0);
    DLOG("Available display modes (interface=<invalid>):");
    const GraphicsDisplayModeEntry *modes = mode_list->modes;
    for (int i = 0; i < mode_list->num_modes; i++) {
        DLOG("   Display %d (%s): %dx%d (%.4gHz)", modes[i].device,
             modes[i].device_name, modes[i].width, modes[i].height,
             modes[i].refresh);
        CHECK_INTEQUAL(modes[i].device, orig_modes[i].device);
        CHECK_INTEQUAL(modes[i].width, orig_modes[i].width);
        CHECK_INTEQUAL(modes[i].height, orig_modes[i].height);
        CHECK_FLOATEQUAL(modes[i].refresh, orig_modes[i].refresh);
    }
    CHECK_INTEQUAL(mode_list->num_modes, orig_num_modes);

    mem_free(orig_modes);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* Check that explicitly requesting XF86VidMode or XRandR doesn't enable
 * the interface if it doesn't exist on the system. */
TEST(test_env_video_mode_disabled_NOINIT)
{
    disable_XF86VidModeQueryExtension = 1;
    disable_XRRQueryExtension = 1;
    CHECK_TRUE(graphics_init());
    disable_XF86VidModeQueryExtension = 0;
    disable_XRRQueryExtension = 0;

    GraphicsDisplayModeEntry *orig_modes;
    int orig_num_modes;
    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(1));
    CHECK_TRUE(mode_list->num_modes > 0);
    orig_num_modes = mode_list->num_modes;
    ASSERT(orig_modes = mem_alloc(sizeof(*orig_modes) * orig_num_modes, 0, 0));
    memcpy(orig_modes, mode_list->modes, sizeof(*orig_modes) * orig_num_modes);

    graphics_cleanup();
    ASSERT(setenv("SIL_X11_VIDEO_MODE_INTERFACE", "VIDMODE", 1) == 0);
    disable_XF86VidModeQueryExtension = 1;
    disable_XRRQueryExtension = 1;
    CHECK_TRUE(graphics_init());
    disable_XF86VidModeQueryExtension = 0;
    disable_XRRQueryExtension = 0;

    CHECK_TRUE(mode_list = graphics_list_display_modes(1));
    CHECK_TRUE(mode_list->num_modes > 0);
    DLOG("Available display modes (interface=VIDMODE, disabled):");
    const GraphicsDisplayModeEntry *modes = mode_list->modes;
    for (int i = 0; i < mode_list->num_modes; i++) {
        DLOG("   Display %d (%s): %dx%d (%.4gHz)", modes[i].device,
             modes[i].device_name, modes[i].width, modes[i].height,
             modes[i].refresh);
        CHECK_INTEQUAL(modes[i].device, orig_modes[i].device);
        CHECK_INTEQUAL(modes[i].width, orig_modes[i].width);
        CHECK_INTEQUAL(modes[i].height, orig_modes[i].height);
        CHECK_FLOATEQUAL(modes[i].refresh, orig_modes[i].refresh);
    }
    CHECK_INTEQUAL(mode_list->num_modes, orig_num_modes);

    graphics_cleanup();
    ASSERT(setenv("SIL_X11_VIDEO_MODE_INTERFACE", "XRANDR", 1) == 0);
    disable_XF86VidModeQueryExtension = 1;
    disable_XRRQueryExtension = 1;
    CHECK_TRUE(graphics_init());
    disable_XF86VidModeQueryExtension = 0;
    disable_XRRQueryExtension = 0;

    CHECK_TRUE(mode_list = graphics_list_display_modes(1));
    CHECK_TRUE(mode_list->num_modes > 0);
    DLOG("Available display modes (interface=XRANDR, disabled):");
    modes = mode_list->modes;
    for (int i = 0; i < mode_list->num_modes; i++) {
        DLOG("   Display %d (%s): %dx%d (%.4gHz)", modes[i].device,
             modes[i].device_name, modes[i].width, modes[i].height,
             modes[i].refresh);
        CHECK_INTEQUAL(modes[i].device, orig_modes[i].device);
        CHECK_INTEQUAL(modes[i].width, orig_modes[i].width);
        CHECK_INTEQUAL(modes[i].height, orig_modes[i].height);
        CHECK_FLOATEQUAL(modes[i].refresh, orig_modes[i].refresh);
    }
    CHECK_INTEQUAL(mode_list->num_modes, orig_num_modes);

    mem_free(orig_modes);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* linux_reset_video_mode() is called by the abort handler, so make sure
 * it doesn't crash even if the graphics subsystem is not initialized. */
TEST(test_reset_video_mode_uninitted_NOINIT_NOCLEANUP)
{
    linux_reset_video_mode();
    return 1;
}

/*************************************************************************/
/*************************************************************************/
