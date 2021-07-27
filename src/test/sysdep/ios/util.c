/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/ios/util.c: Tests for iOS utility functions.
 */

#include "src/base.h"
#include "src/sysdep.h"
#include "src/sysdep/ios/util.h"
#include "src/sysdep/posix/fileutil.h"
#include "src/test/base.h"

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

/**
 * vsync_test:  V-sync function which simply increments the variable
 * pointed to by its argument.
 *
 * [Parameters]
 *     userdata: Pointer to variable to be incremented (int *).
 */
static void vsync_test(void *userdata)
{
    *((int *)userdata) += 1;
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_ios_util)

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_ios_get_model)
{
    /* Assume we're running on a known device. */
    CHECK_TRUE(ios_get_device() != 0);
    CHECK_TRUE(ios_get_model() != 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_ios_get_model_for)
{
    static const struct {
        const char *machine;
        iOSModel model;
    } testlist[] = {

        {"iPhone0,0",   IOS_MODEL_IPHONE_1},     // Not a real device.
        {"iPhone1,1",   IOS_MODEL_IPHONE_1},
        {"iPhone1,2",   IOS_MODEL_IPHONE_3G},
        {"iPhone1,2*",  IOS_MODEL_IPHONE_3G},
        {"iPhone2,1",   IOS_MODEL_IPHONE_3GS},
        {"iPhone3,1",   IOS_MODEL_IPHONE_4},
        {"iPhone3,3",   IOS_MODEL_IPHONE_4},
        {"iPhone4,1",   IOS_MODEL_IPHONE_4S},
        {"iPhone4,1*",  IOS_MODEL_IPHONE_4S},
        {"iPhone5,1",   IOS_MODEL_IPHONE_5},
        {"iPhone5,2",   IOS_MODEL_IPHONE_5},
        {"iPhone5,3",   IOS_MODEL_IPHONE_5C},
        {"iPhone5,4",   IOS_MODEL_IPHONE_5C},
        {"iPhone6,1",   IOS_MODEL_IPHONE_5S},
        {"iPhone6,2",   IOS_MODEL_IPHONE_5S},
        {"iPhone7,1",   IOS_MODEL_IPHONE_6_PLUS},
        {"iPhone7,2",   IOS_MODEL_IPHONE_6},
        {"iPhone8,1",   IOS_MODEL_IPHONE_6S},
        {"iPhone8,2",   IOS_MODEL_IPHONE_6S_PLUS},
        {"iPhone8,4",   IOS_MODEL_IPHONE_SE},
        {"iPhone9,1",   IOS_MODEL_IPHONE_7},
        {"iPhone9,2",   IOS_MODEL_IPHONE_7_PLUS},
        {"iPhone9,3",   IOS_MODEL_IPHONE_7},
        {"iPhone9,4",   IOS_MODEL_IPHONE_7_PLUS},
        {"iPhone10,1",  IOS_MODEL_IPHONE_8},
        {"iPhone10,2",  IOS_MODEL_IPHONE_8_PLUS},
        {"iPhone10,3",  IOS_MODEL_IPHONE_X},
        {"iPhone10,4",  IOS_MODEL_IPHONE_8},
        {"iPhone10,5",  IOS_MODEL_IPHONE_8_PLUS},
        {"iPhone10,6",  IOS_MODEL_IPHONE_X},
        {"iPhone11,2",  IOS_MODEL_IPHONE_XS},
        {"iPhone11,6",  IOS_MODEL_IPHONE_XS_MAX},
        {"iPhone11,8",  IOS_MODEL_IPHONE_XR},
        {"iPhone12,1",  IOS_MODEL_IPHONE_11},
        {"iPhone12,3",  IOS_MODEL_IPHONE_11_PRO},
        {"iPhone12,5",  IOS_MODEL_IPHONE_11_MAX},
        {"iPhone199,0", IOS_MODEL_IPHONE_11},   // Not a real device.

        {"iPod0,0",     IOS_MODEL_IPOD_1},      // Not a real device.
        {"iPod1,1",     IOS_MODEL_IPOD_1},
        {"iPod2,1",     IOS_MODEL_IPOD_2},
        {"iPod3,1",     IOS_MODEL_IPOD_3},
        {"iPod4,1",     IOS_MODEL_IPOD_4},
        {"iPod5,1",     IOS_MODEL_IPOD_5},
        {"iPod7,1",     IOS_MODEL_IPOD_6},
        {"iPod9,1",     IOS_MODEL_IPOD_7},
        {"iPod199,0",   IOS_MODEL_IPOD_7},      // Not a real device.

        {"iPad0,0",     IOS_MODEL_IPAD_1},      // Not a real device.
        {"iPad1,1",     IOS_MODEL_IPAD_1},
        {"iPad2,1",     IOS_MODEL_IPAD_2},
        {"iPad2,2",     IOS_MODEL_IPAD_2},
        {"iPad2,3",     IOS_MODEL_IPAD_2},
        {"iPad2,4",     IOS_MODEL_IPAD_2},
        {"iPad2,5",     IOS_MODEL_IPAD_MINI_1},
        {"iPad2,6",     IOS_MODEL_IPAD_MINI_1},
        {"iPad2,7",     IOS_MODEL_IPAD_MINI_1},
        {"iPad3,1",     IOS_MODEL_IPAD_3},
        {"iPad3,2",     IOS_MODEL_IPAD_3},
        {"iPad3,3",     IOS_MODEL_IPAD_3},
        {"iPad3,4",     IOS_MODEL_IPAD_4},
        {"iPad3,5",     IOS_MODEL_IPAD_4},
        {"iPad3,6",     IOS_MODEL_IPAD_4},
        {"iPad4,1",     IOS_MODEL_IPAD_AIR},
        {"iPad4,2",     IOS_MODEL_IPAD_AIR},
        {"iPad4,3",     IOS_MODEL_IPAD_AIR},
        {"iPad4,4",     IOS_MODEL_IPAD_MINI_2},
        {"iPad4,5",     IOS_MODEL_IPAD_MINI_2},
        {"iPad4,6",     IOS_MODEL_IPAD_MINI_2},
        {"iPad4,7",     IOS_MODEL_IPAD_MINI_3},
        {"iPad4,8",     IOS_MODEL_IPAD_MINI_3},
        {"iPad4,9",     IOS_MODEL_IPAD_MINI_3},
        {"iPad5,1",     IOS_MODEL_IPAD_MINI_4},
        {"iPad5,2",     IOS_MODEL_IPAD_MINI_4},
        {"iPad5,3",     IOS_MODEL_IPAD_AIR_2},
        {"iPad5,4",     IOS_MODEL_IPAD_AIR_2},
        {"iPad6,3",     IOS_MODEL_IPAD_PRO_9IN},
        {"iPad6,4",     IOS_MODEL_IPAD_PRO_9IN},
        {"iPad6,7",     IOS_MODEL_IPAD_PRO},
        {"iPad6,8",     IOS_MODEL_IPAD_PRO},
        {"iPad6,11",    IOS_MODEL_IPAD_5},
        {"iPad6,12",    IOS_MODEL_IPAD_5},
        {"iPad7,1",     IOS_MODEL_IPAD_PRO_2G},
        {"iPad7,2",     IOS_MODEL_IPAD_PRO_2G},
        {"iPad7,3",     IOS_MODEL_IPAD_PRO_2G_10IN},
        {"iPad7,4",     IOS_MODEL_IPAD_PRO_2G_10IN},
        {"iPad7,5",     IOS_MODEL_IPAD_6},
        {"iPad7,6",     IOS_MODEL_IPAD_6},
        {"iPad7,11",    IOS_MODEL_IPAD_7},
        {"iPad7,12",    IOS_MODEL_IPAD_7},
        {"iPad8,1",     IOS_MODEL_IPAD_PRO_3G_11IN},
        {"iPad8,2",     IOS_MODEL_IPAD_PRO_3G_11IN},
        {"iPad8,3",     IOS_MODEL_IPAD_PRO_3G_11IN},
        {"iPad8,4",     IOS_MODEL_IPAD_PRO_3G_11IN},
        {"iPad8,5",     IOS_MODEL_IPAD_PRO_3G},
        {"iPad8,6",     IOS_MODEL_IPAD_PRO_3G},
        {"iPad8,7",     IOS_MODEL_IPAD_PRO_3G},
        {"iPad8,8",     IOS_MODEL_IPAD_PRO_3G},
        {"iPad11,1",    IOS_MODEL_IPAD_MINI_5},
        {"iPad11,2",    IOS_MODEL_IPAD_MINI_5},
        {"iPad11,3",    IOS_MODEL_IPAD_AIR_3},
        {"iPad11,4",    IOS_MODEL_IPAD_AIR_3},
        {"iPad199,0",   IOS_MODEL_IPAD_AIR_3},  // Not a real device.

        {"noSuchModel", 0},                     // Not a real device.

    };  // testlist[]

    int failed = 0;
    #undef FAIL_ACTION
    #define FAIL_ACTION  failed = 1

    for (int i = 0; i < lenof(testlist); i++) {
        const iOSModel model = ios_get_model_for(testlist[i].machine);
        if (model != testlist[i].model) {
            FAIL("ios_get_model_for(\"%s\") was 0x%04X but should have been"
                 " 0x%04X", testlist[i].machine, model, testlist[i].model);
        }
    }

    return !failed;

    #undef FAIL_ACTION
    #define FAIL_ACTION  return 0
}

/*-----------------------------------------------------------------------*/

TEST(test_ios_compare_versions)
{
    CHECK_INTEQUAL(ios_compare_versions("2",       "1"),       +1);
    CHECK_INTEQUAL(ios_compare_versions("1",       "1"),        0);
    CHECK_INTEQUAL(ios_compare_versions("1",       "2"),       -1);

    CHECK_INTEQUAL(ios_compare_versions("1.2",     "1.1"),     +1);
    CHECK_INTEQUAL(ios_compare_versions("1.1",     "1.1"),      0);
    CHECK_INTEQUAL(ios_compare_versions("1.1",     "1.2"),     -1);

    CHECK_INTEQUAL(ios_compare_versions("1.1",     "1"),       +1);
    CHECK_INTEQUAL(ios_compare_versions("1",       "1.1"),     -1);

    CHECK_INTEQUAL(ios_compare_versions("1.1",     "1.1b1"),   +1);
    CHECK_INTEQUAL(ios_compare_versions("1.1b1",   "1.1b1"),    0);
    CHECK_INTEQUAL(ios_compare_versions("1.1b1",   "1.1"),     -1);

    CHECK_INTEQUAL(ios_compare_versions("1.1b1",   "1.1a2"),   +1);
    CHECK_INTEQUAL(ios_compare_versions("1.1a2",   "1.1b1"),   -1);

    CHECK_INTEQUAL(ios_compare_versions("1.1b2",   "1.1b1"),   +1);
    CHECK_INTEQUAL(ios_compare_versions("1.1b1",   "1.1b1"),    0);
    CHECK_INTEQUAL(ios_compare_versions("1.1b1",   "1.1b2"),   -1);

    CHECK_INTEQUAL(ios_compare_versions("1.1b2.2", "1.1b2.1"), +1);
    CHECK_INTEQUAL(ios_compare_versions("1.1b2.1", "1.1b2.1"),  0);
    CHECK_INTEQUAL(ios_compare_versions("1.1b2.1", "1.1b2.2"), -1);

    CHECK_INTEQUAL(ios_compare_versions("1.1b23",  "1.1b3"),   +1);
    CHECK_INTEQUAL(ios_compare_versions("1.1b3",   "1.1b23"),  -1);

    CHECK_INTEQUAL(ios_compare_versions("1.1b2a",  "1.1b2"),   +1);
    CHECK_INTEQUAL(ios_compare_versions("1.1b2a",  "1.1b2a"),   0);
    CHECK_INTEQUAL(ios_compare_versions("1.1b2",   "1.1b2a"),  -1);

    CHECK_INTEQUAL(ios_compare_versions("1.1b2a5", "1.1b2a34"),+1);
    CHECK_INTEQUAL(ios_compare_versions("1.1b2a34","1.1b2a5"), -1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_version_is_at_least)
{
    CHECK_TRUE(ios_version_is_at_least("1.0"));

    CHECK_TRUE(ios_version_is_at_least(NULL));
    CHECK_TRUE(ios_version_is_at_least(""));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_application_name)
{
    const char *name;
    CHECK_TRUE(name = ios_get_application_name());
    CHECK_TRUE(strlen(name) > 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_application_support_path)
{
    const char *path;
    CHECK_TRUE(path = ios_get_application_support_path());
    CHECK_TRUE(strlen(path) > 0);

    /* The directory won't exist on a fresh install, so attempt to create
     * it before checking for existence.  We don't have a "mkdir -p"
     * equivalent, but attempting to write to "." in the directory will
     * accomplish the same thing. */
    char buf[1000];
    ASSERT(strformat_check(buf, sizeof(buf), "%s/.", path));
    posix_write_file(buf, "", 0, 0);
    SysDir *dir;
    CHECK_TRUE(dir = sys_dir_open(path));
    sys_dir_close(dir);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_documents_path)
{
    const char *path;
    CHECK_TRUE(path = ios_get_documents_path());
    CHECK_TRUE(strlen(path) > 0);

    char buf[1000];
    ASSERT(strformat_check(buf, sizeof(buf), "%s/.", path));
    posix_write_file(buf, "", 0, 0);
    SysDir *dir;
    CHECK_TRUE(dir = sys_dir_open(path));
    sys_dir_close(dir);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_vsync_function)
{
    int intvar = 0;
    ios_register_vsync_function(vsync_test, &intvar);
    /* Vertical sync functions are run in parallel, so we need to wait for
     * two frames to ensure the function has been called. */
    ios_vsync();
    ios_vsync();
    /* Wait an extra frame to check that the function was only called once. */
    ios_vsync();
    CHECK_INTEQUAL(intvar, 1);

    const int lenof_vsync_functions = 10;  // Must match definition in util.m.
    intvar = 0;
    for (int i = 0; i < lenof_vsync_functions + 1; i++) {
        /* The last iteration's call will get ignored due to a full array. */
        ios_register_vsync_function(vsync_test, &intvar);
    }
    ios_vsync();
    CHECK_INTEQUAL(intvar, lenof_vsync_functions);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_status_bar_height)
{
    ios_toggle_status_bar(1);
    for (int i = 0; i < 30; i++) {
        ios_vsync();
    }
    CHECK_INTRANGE(ios_status_bar_visible_height(), 1, ios_display_height()/15);

    ios_toggle_status_bar(0);
    for (int i = 0; i < 30; i++) {
        ios_vsync();
    }
    CHECK_INTEQUAL(ios_status_bar_visible_height(), 0);

    return 1;
}

/*************************************************************************/
/*************************************************************************/
