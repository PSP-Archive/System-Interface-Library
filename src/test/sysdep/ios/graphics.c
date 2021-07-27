/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/ios/graphics.c: iOS-specific graphics tests.
 */

#include "src/base.h"
#include "src/graphics.h"
#define IN_SYSDEP  // So we get the real sys_time_*() functions.
#include "src/sysdep.h"
#include "src/sysdep/ios/util.h"
#include "src/test/base.h"
#include "src/thread.h"
#include "src/time.h"

/*************************************************************************/
/***************************** Test runners ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_ios_graphics)

/*-----------------------------------------------------------------------*/

int test_ios_graphics_device_size_early(void)
{
    /* Under iOS 8, the view size is sometimes returned as if in portrait
     * orientation before the first frame has been drawn.  Check that
     * graphics_device_width() and graphics_device_height() return the
     * values expected for the app's orientation (currently always
     * landscape). */

    CHECK_TRUE(graphics_device_width() > graphics_device_height());
    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_ios_graphics_device_size)
{
    ASSERT(thread_init());
    ASSERT(graphics_init());
    const int width = graphics_device_width();
    const int height = graphics_device_height();
    graphics_cleanup();
    thread_cleanup();

    CHECK_TRUE(width > height);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_frame_interval)
{
    ASSERT(thread_init());
    ASSERT(graphics_init());
    sys_time_init();
    const double time_unit = sys_time_unit();

    CHECK_TRUE(graphics_set_display_attr("vsync", 1));
    CHECK_TRUE(graphics_set_display_attr("frame_interval", 1));
    CHECK_TRUE(graphics_set_display_mode(graphics_device_width(),
                                         graphics_device_height(), NULL));
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();

    double start = sys_time_now() / time_unit;
    for (int i = 0; i < 10; i++) {
        graphics_start_frame();
        graphics_clear((i+1)/10.0f, (i+1)/10.0f, (i+1)/10.0f, 0, 1, 0);
        graphics_finish_frame();
    }
    const double interval1_time = (sys_time_now() / time_unit) - start;

    /* This should take effect immediately. */
    CHECK_TRUE(graphics_set_display_attr("frame_interval", 2));

    start = sys_time_now() / time_unit;
    for (int i = 0; i < 10; i++) {
        graphics_start_frame();
        graphics_clear((9-i)/10.0f, (9-i)/10.0f, (9-i)/10.0f, 0, 1, 0);
        graphics_finish_frame();
    }
    const double interval2_time = (sys_time_now() / time_unit) - start;

    /* Cleanup and reinit should reset the frame interval to 1. */
    graphics_cleanup();
    graphics_init();
    CHECK_TRUE(graphics_set_display_mode(graphics_device_width(),
                                         graphics_device_height(), NULL));
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();

    start = sys_time_now() / time_unit;
    for (int i = 0; i < 10; i++) {
        graphics_start_frame();
        graphics_clear((i+1)/10.0f, (i+1)/10.0f, (i+1)/10.0f, 0, 1, 0);
        graphics_finish_frame();
    }
    const double interval1_time2 = (sys_time_now() / time_unit) - start;

    graphics_cleanup();
    thread_cleanup();

    CHECK_DOUBLERANGE(interval2_time, interval1_time*1.6, interval1_time*2.4);
    CHECK_DOUBLERANGE(interval1_time2, interval1_time*0.8, interval1_time*1.2);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_high_refresh_rate)
{
    const int refresh = ios_get_native_refresh_rate();
    if (refresh < 120) {
        SKIP("Display is not high-refresh-rate.");
    }

    ASSERT(thread_init());
    ASSERT(graphics_init());
    sys_time_init();
    const double time_unit = sys_time_unit();

    const GraphicsDisplayModeList *modes;
    CHECK_TRUE(modes = graphics_list_display_modes(1));
    CHECK_INTEQUAL(modes->num_modes, 2);
    CHECK_INTEQUAL(modes->modes[1].device, modes->modes[0].device);
    CHECK_STREQUAL(modes->modes[1].device_name, modes->modes[0].device_name);
    CHECK_INTEQUAL(modes->modes[1].width, modes->modes[0].width);
    CHECK_INTEQUAL(modes->modes[1].height, modes->modes[0].height);
    CHECK_FLOATEQUAL(modes->modes[0].refresh, 60);
    CHECK_FLOATEQUAL(modes->modes[1].refresh, refresh);

    CHECK_TRUE(graphics_set_display_attr("refresh_rate", (float)refresh));
    CHECK_TRUE(graphics_set_display_mode(graphics_device_width(),
                                         graphics_device_height(), NULL));
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();

    ios_vsync();
    double start = sys_time_now() / time_unit;
    const int vsync_count = ios_get_frame_counter();
    while (ios_get_frame_counter() < vsync_count+10) {
        if ((sys_time_now() / time_unit) - start > 1) {
            graphics_cleanup();
            FAIL("Timeout while waiting for vsync");
        }
        ios_vsync();
    }
    const double time = (sys_time_now() / time_unit) - start;

    graphics_cleanup();
    thread_cleanup();

    CHECK_DOUBLERANGE(time, 0.8f*(10.0f/refresh), 1.2f*(10.0f/refresh));
    return 1;
}

/*************************************************************************/
/*************************************************************************/
