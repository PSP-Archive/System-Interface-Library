/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/misc/joystick-db.c: Tests for the HID joystick database.
 */

#include "src/base.h"
#include "src/input.h"
#include "src/sysdep/misc/joystick-db.h"
#include "src/test/base.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_misc_joystick_db)

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_lookup_match_vidpid)
{
    const JoystickDesc *desc = joydb_lookup(0x054C, 0x05C4, 0, NULL);
    CHECK_TRUE(desc);
    CHECK_INTEQUAL(desc->lstick_x, JOYSTICK_VALUE_X);
    CHECK_INTEQUAL(desc->lstick_y, JOYSTICK_VALUE_Y);
    CHECK_INTEQUAL(desc->rstick_x, JOYSTICK_VALUE_Z);
    CHECK_INTEQUAL(desc->rstick_y, JOYSTICK_VALUE_RZ);
    CHECK_INTEQUAL(desc->dpad_type, JOYSTICK_DPAD_HAT);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_HOME], 12);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_START], 9);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_SELECT], 8);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_FACE_UP], 3);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_FACE_DOWN], 1);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_FACE_LEFT], 0);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_FACE_RIGHT], 2);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_L1], 4);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_R1], 5);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_L2], 6);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_R2], 7);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_L_STICK], 10);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_R_STICK], 11);
    CHECK_INTEQUAL(desc->l2_value, JOYSTICK_VALUE_NONE);
    CHECK_INTEQUAL(desc->r2_value, JOYSTICK_VALUE_NONE);
    CHECK_INTEQUAL(desc->linux_rumble, JOYSTICK_LINUX_RUMBLE_LEFT_STRONG);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_lookup_match_version)
{
#ifndef SIL_PLATFORM_LINUX
    SKIP("No test case available on this platform.");
#endif

    const JoystickDesc *desc = joydb_lookup(0x054C, 0x05C4, 0x8000, NULL);
    CHECK_TRUE(desc);
    CHECK_INTEQUAL(desc->lstick_x, JOYSTICK_VALUE_X);
    CHECK_INTEQUAL(desc->lstick_y, JOYSTICK_VALUE_Y);
    CHECK_INTEQUAL(desc->rstick_x, JOYSTICK_VALUE_RX);
    CHECK_INTEQUAL(desc->rstick_y, JOYSTICK_VALUE_RY);
    CHECK_INTEQUAL(desc->dpad_type, JOYSTICK_DPAD_HAT);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_HOME], 10);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_START], 9);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_SELECT], 8);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_FACE_UP], 2);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_FACE_DOWN], 0);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_FACE_LEFT], 3);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_FACE_RIGHT], 1);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_L1], 4);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_R1], 5);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_L2], 6);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_R2], 7);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_L_STICK], 11);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_R_STICK], 12);
    CHECK_INTEQUAL(desc->l2_value, JOYSTICK_VALUE_NONE);
    CHECK_INTEQUAL(desc->r2_value, JOYSTICK_VALUE_NONE);
    CHECK_INTEQUAL(desc->linux_rumble, JOYSTICK_LINUX_RUMBLE_LEFT_STRONG);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_lookup_match_name)
{
    const JoystickDesc *desc =
        joydb_lookup(0, 0, 0, "Microsoft X-Box 360 pad");
    CHECK_TRUE(desc);
    CHECK_INTEQUAL(desc->lstick_x, JOYSTICK_VALUE_X);
    CHECK_INTEQUAL(desc->lstick_y, JOYSTICK_VALUE_Y);
    CHECK_INTEQUAL(desc->rstick_x, JOYSTICK_VALUE_RX);
    CHECK_INTEQUAL(desc->rstick_y, JOYSTICK_VALUE_RY);
    CHECK_INTEQUAL(desc->dpad_type, JOYSTICK_DPAD_HAT);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_HOME], 8);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_START], 6);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_SELECT], 7);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_FACE_UP], 3);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_FACE_DOWN], 0);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_FACE_LEFT], 2);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_FACE_RIGHT], 1);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_L1], 4);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_R1], 5);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_L2], -1);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_R2], -1);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_L_STICK], 9);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_R_STICK], 10);
    CHECK_INTEQUAL(desc->l2_value, JOYSTICK_VALUE_Z);
    CHECK_INTEQUAL(desc->r2_value, JOYSTICK_VALUE_RZ);
    CHECK_INTEQUAL(desc->linux_rumble, JOYSTICK_LINUX_RUMBLE_LEFT_STRONG);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_lookup_match_second_name)
{
    const JoystickDesc *desc =
        joydb_lookup(0, 0, 0, "Sony PLAYSTATION(R)3 Controller");
    CHECK_TRUE(desc);
    CHECK_INTEQUAL(desc->lstick_x, JOYSTICK_VALUE_X);
    CHECK_INTEQUAL(desc->lstick_y, JOYSTICK_VALUE_Y);
    CHECK_INTEQUAL(desc->rstick_x, JOYSTICK_VALUE_Z);
    CHECK_INTEQUAL(desc->rstick_y, JOYSTICK_VALUE_RZ);
    CHECK_INTEQUAL(desc->dpad_type, JOYSTICK_DPAD_BUTTONS);
    CHECK_INTEQUAL(desc->dpad_up, 4);
    CHECK_INTEQUAL(desc->dpad_down, 6);
    CHECK_INTEQUAL(desc->dpad_left, 7);
    CHECK_INTEQUAL(desc->dpad_right, 5);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_HOME], 16);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_START], 3);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_SELECT], 0);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_FACE_UP], 12);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_FACE_DOWN], 14);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_FACE_LEFT], 15);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_FACE_RIGHT], 13);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_L1], 10);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_R1], 11);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_L2], 8);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_R2], 9);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_L_STICK], 1);
    CHECK_INTEQUAL(desc->button_map[INPUT_JOYBUTTON_R_STICK], 2);
    CHECK_INTEQUAL(desc->l2_value, JOYSTICK_VALUE_NONE);
    CHECK_INTEQUAL(desc->r2_value, JOYSTICK_VALUE_NONE);
    CHECK_INTEQUAL(desc->linux_rumble, JOYSTICK_LINUX_RUMBLE_RIGHT_STRONG);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_lookup_nomatch)
{
    const JoystickDesc *desc = joydb_lookup(0, 0, 0, "noname");
    CHECK_FALSE(desc);
    return 1;
}

/*************************************************************************/
/*************************************************************************/
