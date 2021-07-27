/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/graphics/base.c: Tests for basic graphics functions.
 */

#include "src/base.h"
#include "src/graphics.h"
#include "src/memory.h"
#include "src/test/base.h"
#include "src/test/graphics/internal.h"
#include "src/texture.h"
#include "src/thread.h"

#if defined(SIL_PLATFORM_LINUX)
# include "src/sysdep/linux/internal.h"
# include <X11/Xatom.h>
# include <X11/Xlib.h>
#elif defined(SIL_PLATFORM_MACOSX)
# include "src/sysdep/macosx/graphics.h"
#elif defined(SIL_PLATFORM_WINDOWS)
# include "src/sysdep/windows/internal.h"
#endif

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_graphics_base)

/*-----------------------------------------------------------------------*/

TEST_INIT(init)
{
#ifdef SIL_PLATFORM_LINUX
    TEST_linux_graphics_duplicate_mode = 1;
#endif
    CHECK_TRUE(thread_init());
    return graphics_init();
}

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
    graphics_cleanup();
    thread_cleanup();
    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_init_memory_failure)
{
    graphics_cleanup();

    /* The init function might need to do lots of allocations, e.g. for
     * recording video modes, so increase the iteration limit. */
    CHECK_MEMORY_FAILURES_TO(1000, graphics_init());

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_init_invalid)
{
    CHECK_FALSE(graphics_init());  // Double init should fail.

    graphics_cleanup();
    graphics_cleanup();  // Double cleanup should be a no-op.

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_device_info)
{
    /* We have no idea what values we should get here, so just check that
     * they're all positive. */
    CHECK_TRUE(graphics_num_devices() > 0);
    CHECK_TRUE(graphics_device_width() > 0);
    CHECK_TRUE(graphics_device_height() > 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_modes)
{
    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(1));
    CHECK_TRUE(mode_list->num_modes > 0);

    DLOG("Available display modes:");
    const GraphicsDisplayModeEntry *modes = mode_list->modes;
    for (int i = 0; i < mode_list->num_modes; i++) {
        DLOG("   Display %d (%s): %dx%d (%.4gHz)", modes[i].device,
             modes[i].device_name, modes[i].width, modes[i].height,
             modes[i].refresh);
        if (i >= 1) {
            CHECK_TRUE(modes[i].device > modes[i-1].device
                       || (modes[i].device == modes[i-1].device
                           && (modes[i].width * modes[i].height
                                   > modes[i-1].width * modes[i-1].height
                               || (modes[i].width * modes[i].height
                                       == modes[i-1].width * modes[i-1].height
                                   && (modes[i].width > modes[i-1].width
                                       || (modes[i].width == modes[i-1].width
                                           && modes[i].refresh > modes[i-1].refresh))))));
        }
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_modes_no_refresh)
{
    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(0));
    CHECK_TRUE(mode_list->num_modes > 0);

    DLOG("Available display modes (ignoring refresh):");
    const GraphicsDisplayModeEntry *modes = mode_list->modes;
    for (int i = 0; i < mode_list->num_modes; i++) {
        DLOG("   Display %d (%s): %dx%d", modes[i].device,
             modes[i].device_name, modes[i].width, modes[i].height);
        CHECK_FLOATEQUAL(modes[i].refresh, 0);
        if (i >= 1) {
            CHECK_TRUE(modes[i].device > modes[i-1].device
                       || (modes[i].device == modes[i-1].device
                           && (modes[i].width * modes[i].height
                                   > modes[i-1].width * modes[i-1].height
                               || (modes[i].width * modes[i].height
                                       == modes[i-1].width * modes[i-1].height
                                   && modes[i].width > modes[i-1].width))));
        }
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_fullscreen)
{
    const int width = graphics_device_width();
    const int height = graphics_device_height();

    CHECK_TRUE(graphics_set_display_attr("window", 0));
    GraphicsError error;
    CHECK_TRUE(graphics_set_display_mode(width, height, &error));
    CHECK_INTEQUAL(error, GRAPHICS_ERROR_SUCCESS);
    CHECK_FALSE(graphics_display_is_window());
    CHECK_INTEQUAL(graphics_display_width(), width);
    CHECK_INTEQUAL(graphics_display_height(), height);
    /* graphics_display_width() will always return the requested size on
     * first call; start a new frame to ensure that the system also reports
     * the same size. */
    graphics_start_frame();
    CHECK_INTEQUAL(graphics_display_width(), width);
    CHECK_INTEQUAL(graphics_display_height(), height);
    graphics_finish_frame();
    /* We should always have input focus after setting fullscreen mode. */
    CHECK_TRUE(graphics_has_focus());

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_window)
{
    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(0));
    CHECK_TRUE(mode_list->num_modes > 0);
    const int width = mode_list->modes[0].width;
    const int height = mode_list->modes[0].height;

    if (graphics_has_windowed_mode()) {
        CHECK_TRUE(graphics_set_display_attr("window", 1));
    } else {
        CHECK_FALSE(graphics_set_display_attr("window", 1));
    }
    GraphicsError error;
    CHECK_TRUE(graphics_set_display_mode(width, height, &error));
    CHECK_INTEQUAL(error, GRAPHICS_ERROR_SUCCESS);
    if (graphics_has_windowed_mode()) {
        CHECK_TRUE(graphics_display_is_window());
    } else {
        CHECK_FALSE(graphics_display_is_window());
    }
    CHECK_INTEQUAL(graphics_display_width(), width);
    CHECK_INTEQUAL(graphics_display_height(), height);
    graphics_start_frame();
    CHECK_INTEQUAL(graphics_display_width(), width);
    CHECK_INTEQUAL(graphics_display_height(), height);
    graphics_finish_frame();

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_switch_to_fullscreen)
{
    if (!graphics_has_windowed_mode()) {
        SKIP("Not applicable to this platform.");
    }

    const int width = graphics_device_width();
    const int height = graphics_device_height();
    GraphicsError error;

    CHECK_TRUE(graphics_set_display_attr("window", 1));
    CHECK_TRUE(graphics_set_display_mode(TESTW, TESTH, &error));
    CHECK_INTEQUAL(error, GRAPHICS_ERROR_SUCCESS);
    CHECK_TRUE(graphics_display_is_window());
    CHECK_INTEQUAL(graphics_display_width(), TESTW);
    CHECK_INTEQUAL(graphics_display_height(), TESTH);
    graphics_start_frame();
    CHECK_INTEQUAL(graphics_display_width(), TESTW);
    CHECK_INTEQUAL(graphics_display_height(), TESTH);
    graphics_finish_frame();

    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(width, height, NULL));
    CHECK_FALSE(graphics_display_is_window());
    CHECK_INTEQUAL(graphics_display_width(), width);
    CHECK_INTEQUAL(graphics_display_height(), height);
    graphics_start_frame();
    CHECK_INTEQUAL(graphics_display_width(), width);
    CHECK_INTEQUAL(graphics_display_height(), height);
    graphics_finish_frame();

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_switch_to_window)
{
    if (!graphics_has_windowed_mode()) {
        SKIP("Not applicable to this platform.");
    }

    const int width = graphics_device_width();
    const int height = graphics_device_height();
    GraphicsError error;

    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(width, height, &error));
    CHECK_INTEQUAL(error, GRAPHICS_ERROR_SUCCESS);
    CHECK_FALSE(graphics_display_is_window());
    CHECK_INTEQUAL(graphics_display_width(), width);
    CHECK_INTEQUAL(graphics_display_height(), height);
    graphics_start_frame();
    CHECK_INTEQUAL(graphics_display_width(), width);
    CHECK_INTEQUAL(graphics_display_height(), height);
    graphics_finish_frame();

    CHECK_TRUE(graphics_set_display_attr("window", 1));
    CHECK_TRUE(graphics_set_display_mode(TESTW, TESTH, NULL));
    CHECK_TRUE(graphics_display_is_window());
    CHECK_INTEQUAL(graphics_display_width(), TESTW);
    CHECK_INTEQUAL(graphics_display_height(), TESTH);
    graphics_start_frame();
    CHECK_INTEQUAL(graphics_display_width(), TESTW);
    CHECK_INTEQUAL(graphics_display_height(), TESTH);
    graphics_finish_frame();

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_is_window_no_display)
{
    CHECK_FALSE(graphics_display_is_window());
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_alternate_device_info)
{
    const int num_devices = graphics_num_devices();
    if (num_devices == 1) {
        SKIP("Only one display device available.");
    }

    const int width_0 = graphics_device_width();
    const int height_0 = graphics_device_height();
    /* With no display mode set, graphics_device_width() and _height()
     * should return the parameters of the display device selected by the
     * "device" attribute.  However, even if we get the same width and
     * height as for the default device, that's not necessarily a failure;
     * it could just mean that both displays have the same size. */
    int i, width_i, height_i;
    for (i = 1; i < num_devices; i++) {
        CHECK_TRUE(graphics_set_display_attr("device", i));
        width_i = graphics_device_width();
        height_i = graphics_device_height();
        if (width_i != width_0 || height_i != height_0) {
            break;
        }
    }
    if (i >= num_devices) {
        SKIP("No display device found with a different resolution than the"
             " default device (%dx%d).", width_0, height_0);
    }

    /* After a display mode has been opened, graphics_device_width() and
     * _height() should return the parameters of the display device on
     * which the mode was set, regardless of the value of the "device"
     * attribute. */
    CHECK_TRUE(graphics_set_display_attr("window",
                                         graphics_has_windowed_mode()));
    GraphicsError error;
    CHECK_TRUE(graphics_set_display_mode(width_i, height_i, &error));
    CHECK_INTEQUAL(error, GRAPHICS_ERROR_SUCCESS);
    CHECK_INTEQUAL(graphics_device_width(), width_i);
    CHECK_INTEQUAL(graphics_device_height(), height_i);
    CHECK_TRUE(graphics_set_display_attr("device", 0));
    CHECK_INTEQUAL(graphics_device_width(), width_i);
    CHECK_INTEQUAL(graphics_device_height(), height_i);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_window_centered)
{
    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(0));
    CHECK_TRUE(mode_list->num_modes > 0);
    const int width = mode_list->modes[0].width;
    const int height = mode_list->modes[0].height;

    if (graphics_has_windowed_mode()) {
        CHECK_TRUE(graphics_set_display_attr("window", 1));
    } else {
        CHECK_FALSE(graphics_set_display_attr("window", 1));
    }
    CHECK_TRUE(graphics_set_display_attr("center_window", 1));
    GraphicsError error;
    CHECK_TRUE(graphics_set_display_mode(width, height, &error));
    CHECK_INTEQUAL(error, GRAPHICS_ERROR_SUCCESS);

    if (!graphics_has_windowed_mode()) {
        return 1;
    }

    /* Check that the window is centered.  Note that this is not a perfect
     * test; it could succeed even for broken code if the OS happens to put
     * the window in the center of the screen anyway. */
    const int device_w = graphics_device_width();
    const int device_h = graphics_device_height();
    int left, right, top, bottom;  // Distance from window to each screen edge.
#if defined(SIL_PLATFORM_LINUX)
    int x = 0, y = 0;
    unsigned int w = 0, h = 0;
    Window root, unused_child;
    int unused_parent_x, unused_parent_y;
    unsigned int unused_border, unused_depth;
    XGetGeometry(linux_x11_display(), linux_x11_window(), &root,
                 &unused_parent_x, &unused_parent_y, &w, &h, &unused_border,
                 &unused_depth);
    XTranslateCoordinates(linux_x11_display(), linux_x11_window(), root,
                          0, 0, &x, &y, &unused_child);
    left = x;
    top = y;
    right = device_w - (x + w);
    bottom = device_h - (y + h);
#elif defined(SIL_PLATFORM_MACOSX)
    /* For OSX, a window that appears "centered" will not in fact be
     * centered relative to the physical screen coordinates, due to the
     * menu bar and dock taking up screen space.  We can't get the usable
     * screen region from plain C code, so we skip this part of the test. */
    left = top = right = bottom = 0;
#elif defined(SIL_PLATFORM_WINDOWS)
    RECT rect;
    ASSERT(GetWindowRect(windows_window(), &rect));
    left = rect.left;
    right = device_w - rect.right;
    top = rect.top;
    bottom = device_h - rect.bottom;
#else
    ASSERT(!"unreachable");
#endif
    /* Theoretically it should be exact (+/-1 for rounding error), but
     * window borders and such may cause offsets, so allow a bit of leeway. */
    CHECK_INTRANGE(left - right, -device_w/10, device_w/10);
    CHECK_INTRANGE(top - bottom, -device_h/10, device_h/10);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_wrong_size)
{
    /* Assume that trying to use an INT_MAX by INT_MAX fullscreen mode
     * will fail.  (I'd like to see a system where it succeeds...) */
    CHECK_TRUE(graphics_set_display_attr("window", 0));
    GraphicsError error;
    CHECK_FALSE(graphics_set_display_mode(INT_MAX, INT_MAX, &error));
    CHECK_INTEQUAL(error, GRAPHICS_ERROR_MODE_NOT_SUPPORTED);

    /* Check that we can subsequently open a normal display. */
    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(0));
    CHECK_TRUE(mode_list->num_modes > 0);
    const int width = mode_list->modes[0].width;
    const int height = mode_list->modes[0].height;
    CHECK_TRUE(graphics_set_display_attr("window",
                                         graphics_has_windowed_mode()));
    CHECK_TRUE(graphics_set_display_mode(width, height, &error));
    CHECK_INTEQUAL(error, GRAPHICS_ERROR_SUCCESS);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_invalid)
{
    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(0));
    CHECK_TRUE(mode_list->num_modes > 0);
    const int width = mode_list->modes[0].width;
    const int height = mode_list->modes[0].height;

    CHECK_TRUE(graphics_set_display_attr("window",
                                         graphics_has_windowed_mode()));
    GraphicsError error;
    CHECK_FALSE(graphics_set_display_mode(0, 0, &error));
    CHECK_INTEQUAL(error, GRAPHICS_ERROR_MODE_NOT_SUPPORTED);
    CHECK_FALSE(graphics_set_display_mode(width, 0, &error));
    CHECK_INTEQUAL(error, GRAPHICS_ERROR_MODE_NOT_SUPPORTED);
    CHECK_FALSE(graphics_set_display_mode(0, height, &error));
    CHECK_INTEQUAL(error, GRAPHICS_ERROR_MODE_NOT_SUPPORTED);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_no_error_ret)
{
    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(0));
    CHECK_TRUE(mode_list->num_modes > 0);
    const int width = mode_list->modes[0].width;
    const int height = mode_list->modes[0].height;

    CHECK_TRUE(graphics_set_display_attr("window",
                                         graphics_has_windowed_mode()));
    CHECK_FALSE(graphics_set_display_mode(0, 0, NULL));
    CHECK_TRUE(graphics_set_display_mode(width, height, NULL));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_attr)
{
    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(0));
    CHECK_TRUE(mode_list->num_modes > 0);
    const int width = mode_list->modes[0].width;
    const int height = mode_list->modes[0].height;

    CHECK_TRUE(graphics_set_display_attr("depth_bits", 16));
    CHECK_TRUE(graphics_set_display_attr("multisample", 1));
    CHECK_TRUE(graphics_set_display_attr("refresh_rate", 0.0f));
    CHECK_TRUE(graphics_set_display_attr("stencil_bits", 0));
    /* Both vsync=true and vsync=false could potentially fail depending on
     * system capabilities, so just check that at least one succeeds. */
    CHECK_TRUE(graphics_set_display_attr("vsync", 0)
            || graphics_set_display_attr("vsync", 1));
    CHECK_TRUE(graphics_set_display_attr("window",
                                         graphics_has_windowed_mode()));
    CHECK_TRUE(graphics_set_display_mode(width, height, NULL));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_attr_invalid)
{
    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(0));
    CHECK_TRUE(mode_list->num_modes > 0);
    const int width = mode_list->modes[0].width;
    const int height = mode_list->modes[0].height;

    CHECK_FALSE(graphics_set_display_attr("NO_SUCH_ATTRIBUTE", 0));
    CHECK_FALSE(graphics_set_display_attr("depth_bits", -1));
    CHECK_FALSE(graphics_set_display_attr("device", -1));
    CHECK_FALSE(graphics_set_display_attr("multisample", -1));
    CHECK_FALSE(graphics_set_display_attr("refresh_rate", -1.0f));
    CHECK_FALSE(graphics_set_display_attr("refresh_rate", FLOAT_NAN()));
    CHECK_FALSE(graphics_set_display_attr("stencil_bits", -1));

    CHECK_TRUE(graphics_set_display_attr("window",
                                         graphics_has_windowed_mode()));
    /* The graphics_set_display_mode() call should still succeed (using
     * default attribute values). */
    GraphicsError error;
    CHECK_TRUE(graphics_set_display_mode(width, height, &error));
    CHECK_INTEQUAL(error, GRAPHICS_ERROR_SUCCESS);

    /* An out-of-range display device index should always be detected
     * before opening the window. */
    CHECK_FALSE(graphics_set_display_attr("device", INT_MAX));
    CHECK_TRUE(graphics_set_display_mode(width, height, &error));
    CHECK_INTEQUAL(error, GRAPHICS_ERROR_SUCCESS);

    /* Any of these could succeed if the system can't immediately detect
     * that the values are out of range, but if they do succeed, attempting
     * to set a display mode using those values should fail. */
    if (graphics_set_display_attr("depth_bits", INT_MAX)) {
        CHECK_FALSE(graphics_set_display_mode(width, height, &error));
        CHECK_INTEQUAL(error, GRAPHICS_ERROR_MODE_NOT_SUPPORTED);
        CHECK_TRUE(graphics_set_display_attr("depth_bits", 16));
        CHECK_TRUE(graphics_set_display_mode(width, height, &error));
        CHECK_INTEQUAL(error, GRAPHICS_ERROR_SUCCESS);
    }
    if (graphics_set_display_attr("multisample", INT_MAX)) {
        CHECK_FALSE(graphics_set_display_mode(width, height, &error));
        CHECK_INTEQUAL(error, GRAPHICS_ERROR_MODE_NOT_SUPPORTED);
        CHECK_TRUE(graphics_set_display_attr("multisample", 1));
        CHECK_TRUE(graphics_set_display_mode(width, height, &error));
        CHECK_INTEQUAL(error, GRAPHICS_ERROR_SUCCESS);
    }
    if (graphics_set_display_attr("stencil_bits", INT_MAX)) {
        CHECK_FALSE(graphics_set_display_mode(width, height, &error));
        CHECK_INTEQUAL(error, GRAPHICS_ERROR_MODE_NOT_SUPPORTED);
        CHECK_TRUE(graphics_set_display_attr("stencil_bits", 0));
        CHECK_TRUE(graphics_set_display_mode(width, height, &error));
        CHECK_INTEQUAL(error, GRAPHICS_ERROR_SUCCESS);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_renderer_info)
{
    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(0));
    CHECK_TRUE(mode_list->num_modes > 0);
    const int width = mode_list->modes[0].width;
    const int height = mode_list->modes[0].height;

    if (graphics_has_windowed_mode()) {
        CHECK_TRUE(graphics_set_display_attr("window", 1));
    } else {
        CHECK_FALSE(graphics_set_display_attr("window", 1));
    }
    CHECK_TRUE(graphics_set_display_mode(width, height, NULL));

    /* We don't know what text we'll get back, but it should never be NULL. */
    CHECK_TRUE(graphics_renderer_info() != NULL);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_window_title)
{
    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(0));
    CHECK_TRUE(mode_list->num_modes > 0);
    const int width = mode_list->modes[0].width;
    const int height = mode_list->modes[0].height;
    CHECK_TRUE(graphics_set_display_attr("window",
                                         graphics_has_windowed_mode()));
    CHECK_TRUE(graphics_set_display_mode(width, height, NULL));

    graphics_set_window_title("SIL Test");

#if defined(SIL_PLATFORM_LINUX)
    char *name = NULL;
    XFetchName(linux_x11_display(), linux_x11_window(), &name);
    CHECK_STREQUAL(name, "SIL Test");
    XFree(name);
    name = NULL;
    XGetIconName(linux_x11_display(), linux_x11_window(), &name);
    CHECK_STREQUAL(name, "SIL Test");
    XFree(name);
#elif defined(SIL_PLATFORM_MACOSX)
    char *title = macosx_get_window_title();
    CHECK_STREQUAL(title, "SIL Test");
    mem_free(title);
#else
    /* No other platforms support windows, so we're satisfied as long as
     * the call doesn't crash. */
#endif

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_window_title_early)
{
    /* graphics_set_window_title() should work even if it's called before
     * the window is opened. */
    graphics_set_window_title("SIL Test");

    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(0));
    CHECK_TRUE(mode_list->num_modes > 0);
    const int width = mode_list->modes[0].width;
    const int height = mode_list->modes[0].height;
    CHECK_TRUE(graphics_set_display_attr("window",
                                         graphics_has_windowed_mode()));
    CHECK_TRUE(graphics_set_display_mode(width, height, NULL));

#ifdef SIL_PLATFORM_LINUX
    char *name = NULL;
    XFetchName(linux_x11_display(), linux_x11_window(), &name);
    CHECK_STREQUAL(name, "SIL Test");
    XFree(name);
    name = NULL;
    XGetIconName(linux_x11_display(), linux_x11_window(), &name);
    CHECK_STREQUAL(name, "SIL Test");
    XFree(name);
#elif defined(SIL_PLATFORM_MACOSX)
    char *title = macosx_get_window_title();
    CHECK_STREQUAL(title, "SIL Test");
    mem_free(title);
#endif

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_show_mouse_pointer)
{
    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(0));
    CHECK_TRUE(mode_list->num_modes > 0);
    const int width = mode_list->modes[0].width;
    const int height = mode_list->modes[0].height;
    CHECK_TRUE(graphics_set_display_attr("window",
                                         graphics_has_windowed_mode()));
    CHECK_TRUE(graphics_set_display_mode(width, height, NULL));

    /* We have no way of checking directly whether this works, so just
     * call the function both ways and make sure it doesn't crash. */
    graphics_show_mouse_pointer(1);
    graphics_show_mouse_pointer(0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_mouse_pointer_state)
{
#if defined(SIL_PLATFORM_LINUX) || defined(SIL_PLATFORM_WINDOWS)
    /* Set/get should work even before a window has been opened. */
    graphics_show_mouse_pointer(1);
    CHECK_TRUE(graphics_get_mouse_pointer_state());
    graphics_show_mouse_pointer(0);
    CHECK_FALSE(graphics_get_mouse_pointer_state());
#endif

    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(0));
    CHECK_TRUE(mode_list->num_modes > 0);
    const int width = mode_list->modes[0].width;
    const int height = mode_list->modes[0].height;
    CHECK_TRUE(graphics_set_display_attr("window",
                                         graphics_has_windowed_mode()));
    CHECK_TRUE(graphics_set_display_mode(width, height, NULL));

#if defined(SIL_PLATFORM_LINUX) || defined(SIL_PLATFORM_WINDOWS)
    graphics_show_mouse_pointer(1);
    CHECK_TRUE(graphics_get_mouse_pointer_state());
    graphics_show_mouse_pointer(0);
    CHECK_FALSE(graphics_get_mouse_pointer_state());
#else
    /* We have no way of checking whether this works, so just call the
     * function and make sure it doesn't crash. */
    (void) graphics_get_mouse_pointer_state();
#endif

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_frame_period)
{
    const int width = graphics_device_width();
    const int height = graphics_device_height();
    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(width, height, NULL));

    const double period = graphics_frame_period();
    CHECK_TRUE(period >= 0);

#if defined(SIL_PLATFORM_LINUX)
    /* Linux should always give us a valid frame period, but it's a pain
     * to look it up exactly, so just check for nonzeroness. */
    CHECK_TRUE(period > 0);
#elif defined(SIL_PLATFORM_MACOSX)
    /* We can't predict what we'll get here, so just log it and pass. */
    DLOG("Got frame period: %g", period);
#elif defined(SIL_PLATFORM_WINDOWS)
    DEVMODE mode;
    ASSERT(EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &mode));
    if ((mode.dmFields & DM_DISPLAYFREQUENCY) && mode.dmDisplayFrequency > 0) {
        CHECK_DOUBLEEQUAL(period, 1.0 / (double)mode.dmDisplayFrequency);
    } else {
        CHECK_DOUBLEEQUAL(period, 0.0);
    }
#else
    /* All other platforms return NTSC timing (59.94Hz).  Note that we
     * do the same computation as graphics_frame_period() to ensure
     * equality -- 1.001/60.0 rounds differently from 1001.0/60000.0. */
    volatile int num = 1001, den = 60000;  // Prevent constant folding.
    CHECK_DOUBLEEQUAL(period, (double)num / (double)den);
#endif

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_refresh_rate)
{
    /* Make sure the display mode is always reset to default, even on
     * test failure. */
    #undef FAIL_ACTION
    #define FAIL_ACTION  goto fail

    /* This may set the real mouse pointer position on PC platforms, so
     * save and restore it to avoid interfering with whatever else the
     * user may be doing. */
    int x, y;
    get_mouse_position(&x, &y);

    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(1));
    int mode1 = -1, mode2 = -1;
    for (int i = mode_list->num_modes - 2; i >= 0; i--) {
        if (mode_list->modes[i].width >= graphics_device_width()
         || mode_list->modes[i].height >= graphics_device_height()) {
            continue;
        }
        if (mode_list->modes[i].device == mode_list->modes[i+1].device
         && mode_list->modes[i].width == mode_list->modes[i+1].width
         && mode_list->modes[i].height == mode_list->modes[i+1].height
         && mode_list->modes[i].refresh != 0) {
            int j;
            for (j = i+1; j+1 < mode_list->num_modes; j++) {
                if (mode_list->modes[j+1].width != mode_list->modes[i].width
                 || mode_list->modes[j+1].height != mode_list->modes[i].height)
                {
                    break;
                }
            }
            if (mode_list->modes[j].refresh - mode_list->modes[i].refresh
                >= 1.0f)
            {
#ifdef SIL_PLATFORM_LINUX
                /* Work around XRandR reporting invalid modes as available. */
                ASSERT(graphics_set_display_attr("window", 0));
                ASSERT(graphics_set_display_attr("device",
                                                 mode_list->modes[i].device));
                ASSERT(graphics_set_display_attr("refresh_rate",
                                                 mode_list->modes[i].refresh));
                if (!graphics_set_display_mode(mode_list->modes[i].width,
                                               mode_list->modes[i].height,
                                               NULL)) {
                    continue;
                }
#endif
                mode1 = i;
                mode2 = j;
                break;
            }
        }
    }
    if (mode1 < 0) {
        SKIP("No modes found which differ only in refresh rate.");
    }

    const int device = mode_list->modes[mode1].device;
    const int width = mode_list->modes[mode1].width;
    const int height = mode_list->modes[mode1].height;
    const float refresh1 = mode_list->modes[mode1].refresh;
    const float refresh2 = mode_list->modes[mode2].refresh;

    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_attr("device", device));
    CHECK_TRUE(graphics_set_display_attr("refresh_rate", refresh1));
    CHECK_TRUE(graphics_set_display_mode(width, height, NULL));
    CHECK_DOUBLERANGE(graphics_frame_period(),
                      1.0/(double)refresh1 - 0.1, 1.0/(double)refresh1 + 0.1);

    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_attr("device", device));
    CHECK_TRUE(graphics_set_display_attr("refresh_rate", refresh2));
    CHECK_TRUE(graphics_set_display_mode(width, height, NULL));
    CHECK_DOUBLERANGE(graphics_frame_period(),
                      1.0/(double)refresh1 - 0.1, 1.0/(double)refresh1 + 0.1);

    /* Non-exact matches should use the nearest value. */
    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_attr("device", device));
    CHECK_TRUE(graphics_set_display_attr("refresh_rate",
                                         refresh1 + (refresh2-refresh1)*0.3f));
    CHECK_TRUE(graphics_set_display_mode(width, height, NULL));
    CHECK_DOUBLERANGE(graphics_frame_period(),
                      1.0/(double)refresh1 - 0.1, 1.0/(double)refresh1 + 0.1);

    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_attr("device", device));
    CHECK_TRUE(graphics_set_display_attr("refresh_rate",
                                         refresh1 + (refresh2-refresh1)*0.7f));
    CHECK_TRUE(graphics_set_display_mode(width, height, NULL));
    CHECK_DOUBLERANGE(graphics_frame_period(),
                      1.0/(double)refresh2 - 0.1, 1.0/(double)refresh2 + 0.1);

    /* A value of zero should use the highest available refresh rate. */
    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_attr("device", device));
    CHECK_TRUE(graphics_set_display_attr("refresh_rate", 0.0f));
    CHECK_TRUE(graphics_set_display_mode(width, height, NULL));
    CHECK_DOUBLERANGE(graphics_frame_period(),
                      1.0/(double)refresh1 - 0.1, 1.0/(double)refresh1 + 0.1);

    /* Restore the original display mode before returning, since that may
     * be necessary to put the mouse pointer back in the proper place. */
    graphics_set_display_mode(graphics_device_width(),
                              graphics_device_height(), NULL);
    set_mouse_position(x, y);
    return 1;

  fail:
    graphics_set_display_mode(graphics_device_width(),
                              graphics_device_height(), NULL);
    set_mouse_position(x, y);
    return 0;

    #undef FAIL_ACTION
    #define FAIL_ACTION  return 0
}

/*-----------------------------------------------------------------------*/

TEST(test_has_focus_no_display)
{
    /* This test only applies to platforms with a windowed mode; other
     * platforms always return true for graphics_has_focus(). */
    if (graphics_has_windowed_mode()) {
        CHECK_FALSE(graphics_has_focus());
    } else {
        CHECK_TRUE(graphics_has_focus());
    }
    return 1;
}

/*************************************************************************/
/*************************************************************************/
