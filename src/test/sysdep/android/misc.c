/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/android/misc.c: Tests for miscellaneous Android-specific
 * functions.
 */

#include "src/base.h"
#include "src/graphics.h"
#include "src/memory.h"
#define IN_SYSDEP  // So we get the real functions instead of the diversions.
#include "src/sysdep.h"
#include "src/sysdep/android/internal.h"
#include "src/test/base.h"

#include <time.h>

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_android_misc)

/*-----------------------------------------------------------------------*/

TEST(test_language)
{
    char language[3], dialect[3];

    /* We have no way of knowing what language the user selected, so just
     * make sure the function works. */
    CHECK_TRUE(sys_get_language(0, language, dialect));
    CHECK_TRUE(strlen(language) == 2);
    CHECK_TRUE(strlen(dialect) == 0 || strlen(dialect) == 2);

    CHECK_FALSE(sys_get_language(1, language, dialect));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_open_file_url)
{
    CHECK_FALSE(sys_open_file(NULL));
    CHECK_TRUE(sys_open_url(NULL));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_reset_idle_timer)
{
    /* We can't detect the effect of this on the system; just ensure that
     * calling it multiple times in short succession doesn't lead to a
     * deadlock or otherwise fail. */
    sys_reset_idle_timer();
    nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = 1000000000/60}, NULL);
    sys_reset_idle_timer();

    android_stop_idle_timer_thread();
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_toggle_navigation_bar)
{
    ASSERT(graphics_init());
    const GraphicsDisplayModeList *mode_list;
    ASSERT(mode_list = graphics_list_display_modes(0));
    ASSERT(mode_list->num_modes > 0);
    ASSERT(graphics_set_display_mode(mode_list->modes[0].width,
                                     mode_list->modes[0].height, NULL));
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);

    /* The navigation bar should be disabled by default. */
    CHECK_FALSE(android_get_navigation_bar_state());

    /* Check that we can enable the navigation bar, but only on
     * Android [3.0,4.3]. */
    android_toggle_navigation_bar(1);
    if (android_api_level >= 11 && android_api_level <= 18) {
        CHECK_TRUE(android_get_navigation_bar_state());
    } else {
        CHECK_FALSE(android_get_navigation_bar_state());
    }

    /* Check that we can disable the navigation bar again. */
    android_toggle_navigation_bar(0);
    CHECK_FALSE(android_get_navigation_bar_state());

    graphics_finish_frame();
    graphics_cleanup();
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_info_strings)
{
    /* Just check that they all return non-NULL. */
    CHECK_TRUE(android_get_hardware());
    CHECK_TRUE(android_get_manufacturer());
    CHECK_TRUE(android_get_model());
    CHECK_TRUE(android_get_product());

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_resource_string)
{
    char *appName;
    CHECK_TRUE(appName = android_get_resource_string("appName"));

    mem_free(appName);
    return 1;
}

/*************************************************************************/
/*************************************************************************/
