/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/macosx/graphics.m: Graphics-related tests specific to
 * Mac OS X.
 */

#include "src/base.h"
#include "src/graphics.h"
#include "src/test/base.h"
#include "src/test/graphics/internal.h"
#include "src/thread.h"

#include "src/sysdep/macosx/osx-headers.h"
#include <AppKit/NSScreen.h>

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_macosx_graphics)

TEST_INIT(init)
{
    CHECK_TRUE(thread_init());
    CHECK_TRUE(graphics_init());
    return 1;
}

TEST_CLEANUP(cleanup)
{
    graphics_cleanup();
    thread_cleanup();
    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_video_modes)
{
    const int default_width = graphics_device_width();
    const int default_height = graphics_device_height();

    int mode_width, mode_height;
    if (!get_alternate_video_mode(&mode_width, &mode_height)) {
        SKIP("No alternate video mode available.");
    }

    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(mode_width, mode_height, 0));
    CHECK_FLOATEQUAL([NSScreen mainScreen].frame.size.width, mode_width);
    CHECK_FLOATEQUAL([NSScreen mainScreen].frame.size.height, mode_height);

    /* Check that graphics_device_{width,height}() return the resolution
     * of the default mode and not the current mode. */
    CHECK_INTEQUAL(graphics_device_width(), default_width);
    CHECK_INTEQUAL(graphics_device_height(), default_height);

    /* Check that switching back to the default mode works (test for bug
     * fixed in r3233). */
    CHECK_TRUE(graphics_set_display_mode(default_width, default_height, 0));
    CHECK_FLOATEQUAL([NSScreen mainScreen].frame.size.width, default_width);
    CHECK_FLOATEQUAL([NSScreen mainScreen].frame.size.height, default_height);

    return 1;
}

/*************************************************************************/
/*************************************************************************/
