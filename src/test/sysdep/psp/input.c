/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/psp/input.c: Tests for PSP input functionality (what of
 * it we can test in an automated manner, which isn't much).
 */

#include "src/base.h"
#include "src/input.h"
#define IN_SYSDEP  // So we get the real functions instead of the diversions.
#include "src/sysdep.h"
#include "src/test/base.h"

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

/**
 * event_callback:  Callback which receives input events from the sys_input
 * module.  We don't actually do anything that generates events, so this is
 * a no-op.
 */
static void event_callback(UNUSED const InputEvent *event)
{
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_psp_input)

/*-----------------------------------------------------------------------*/

TEST_INIT(init)
{
    CHECK_TRUE(sys_input_init(event_callback));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
    sys_input_cleanup();
    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_info)
{
    SysInputInfo info;
    sys_input_info(&info);

    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_TRUE(info.joysticks[0].connected);
    CHECK_FALSE(info.joysticks[0].can_rumble);
    CHECK_INTEQUAL(info.joysticks[0].num_buttons, 16);
    CHECK_INTEQUAL(info.joysticks[0].num_sticks, 1);

    CHECK_FALSE(info.has_keyboard);

    CHECK_FALSE(info.has_mouse);

    CHECK_TRUE(info.has_text);
    CHECK_TRUE(info.text_uses_custom_interface);
    CHECK_TRUE(info.text_has_prompt);

    CHECK_FALSE(info.has_touch);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_grab)
{
    sys_input_grab(0);  // Just make sure it doesn't crash.
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_is_quit_requested)
{
    CHECK_FALSE(sys_input_is_quit_requested());
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_is_suspend_requested)
{
    CHECK_FALSE(sys_input_is_suspend_requested());
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_copy_name)
{
    char *name;
    CHECK_TRUE(name = sys_input_joystick_copy_name(0));
    CHECK_STREQUAL(name, "Sony PlayStation Portable");
    mem_free(name);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_button_mapping)
{
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_HOME), -1);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_START), 3);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_SELECT), 0);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_FACE_UP), 12);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_FACE_LEFT), 15);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_FACE_RIGHT), 13);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_FACE_DOWN), 14);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_L1), 8);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_R1), 9);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_L2), -1);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_R2), -1);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_L_STICK), -1);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_R_STICK), -1);
    return 1;
}

/*************************************************************************/
/*************************************************************************/
