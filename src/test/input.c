/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/input.c: Tests for the high-level timekeeping functions.
 */

#include "src/base.h"
#include "src/input.h"
#include "src/sysdep/test.h"
#include "src/test/base.h"
#include "src/thread.h"
#include "src/time.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Copy of input events received by receive_event() or receive_text_event(). */
static InputEvent events[10];

/* Number of events received by receive_event() or receive_text_event(). */
static int num_events;

/* Array for holding characters or events received for text input, and the
 * number of values stored. */
static struct {int detail; int32_t ch;} text_input[100];
static int text_input_len;

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

/**
 * receive_event:  Input event callback function which stores the received
 * event in last_event for test code to examine.
 */
static void receive_event(const InputEvent *event)
{
    ASSERT(event != NULL);
    if (num_events < lenof(events)) {
        events[num_events] = *event;
    }
    num_events++;
}

/*-----------------------------------------------------------------------*/

/**
 * receive_text_event:  Input event callback function which expects only
 * text input events and stores the characters or event codes in the
 * text_input array.  The last_event structure is also updated as for
 * receive_event().
 */
static void receive_text_event(const InputEvent *event)
{
    ASSERT(event != NULL);
    ASSERT(event->type == INPUT_EVENT_TEXT);
    ASSERT(text_input_len < lenof(text_input));

    text_input[text_input_len].detail = event->detail;
    text_input[text_input_len].ch = event->text.ch;
    text_input_len++;

    if (num_events < lenof(events)) {
        events[num_events] = *event;
    }
    num_events++;
}

/*************************************************************************/
/***************** Test runner and init/cleanup routines *****************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_input)

TEST_INIT(init)
{
    time_init();
    CHECK_TRUE(thread_init());

    sys_test_input_set_joy_connected_on_init(0);
    CHECK_TRUE(input_init());
    mem_clear(events, sizeof(events));
    num_events = 0;

    return 1;
}

TEST_CLEANUP(cleanup)
{
    input_cleanup();
    sys_test_input_set_joy_connected_on_init(0);

    thread_cleanup();
    return 1;
}

/*************************************************************************/
/********************** General functionality tests **********************/
/*************************************************************************/

TEST(test_init_cleanup)
{
    /* Double init. */
    CHECK_FALSE(input_init());

    /* Double cleanup. */
    input_cleanup();
    input_cleanup();

    sys_test_input_fail_init();
    CHECK_FALSE(input_init());

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_grab_input)
{
    CHECK_FALSE(sys_test_input_get_grab_state());

    input_grab(1);
    input_update();
    CHECK_TRUE(sys_test_input_get_grab_state());

    input_grab(0);
    input_update();
    CHECK_FALSE(sys_test_input_get_grab_state());

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_quit)
{
    CHECK_FALSE(input_is_quit_requested());

    sys_test_input_send_quit_request();
    input_update();
    CHECK_TRUE(input_is_quit_requested());

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_suspend)
{
    CHECK_FALSE(input_is_suspend_requested());

    sys_test_input_send_suspend_request();
    input_update();
    CHECK_TRUE(input_is_suspend_requested());

    input_acknowledge_suspend_request();
    input_update();
    CHECK_FALSE(input_is_suspend_requested());

    return 1;
}

/*************************************************************************/
/**************************** Joystick tests *****************************/
/*************************************************************************/

TEST(test_enable_unfocused_joystick)
{
    CHECK_TRUE(sys_test_input_get_unfocused_joystick_state());

    input_enable_unfocused_joystick(0);
    input_update();
    CHECK_FALSE(sys_test_input_get_unfocused_joystick_state());

    input_enable_unfocused_joystick(1);
    input_update();
    CHECK_TRUE(sys_test_input_get_unfocused_joystick_state());

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_present)
{
    CHECK_TRUE(input_joystick_present());

    sys_test_input_set_joy_num_devices(0);
    input_update();
    CHECK_FALSE(input_joystick_present());

    sys_test_input_set_joy_num_devices(1);
    sys_test_input_enable_joystick(0);
    input_update();
    CHECK_FALSE(input_joystick_present());

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_count)
{
    CHECK_INTEQUAL(input_joystick_count(), 1);

    sys_test_input_enable_joystick(0);
    input_update();
    CHECK_INTEQUAL(input_joystick_count(), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joy_too_many_devices)
{
    sys_test_input_set_joy_num_devices(INPUT_MAX_JOYSTICKS + 1);
    input_update();
    CHECK_INTEQUAL(input_joystick_count(), INPUT_MAX_JOYSTICKS);
    CHECK_DLOG_TEXT("System reports %d joysticks, but only %d supported",
                    INPUT_MAX_JOYSTICKS + 1, INPUT_MAX_JOYSTICKS);

    /* Shouldn't warn the second time. */
    DLOG("dummy message");
    sys_test_input_set_joy_num_devices(INPUT_MAX_JOYSTICKS + 1);
    input_update();
    CHECK_INTEQUAL(input_joystick_count(), INPUT_MAX_JOYSTICKS);
    CHECK_DLOG_TEXT("dummy message");

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joy_connected)
{
    CHECK_FALSE(input_joystick_connected(0));

    sys_test_input_set_joy_connected(1);
    input_update();
    CHECK_TRUE(input_joystick_connected(0));

    sys_test_input_enable_joystick(0);
    input_update();
    CHECK_FALSE(input_joystick_connected(0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joy_connected_invalid)
{
    CHECK_FALSE(input_joystick_connected(-1));
    CHECK_FALSE(input_joystick_connected(input_joystick_count()));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joy_copy_name)
{
    char *name = input_joystick_copy_name(0);
    CHECK_STREQUAL(name, "Joystick Name");
    mem_free(name);

    sys_test_input_enable_joystick(0);
    input_update();
    CHECK_FALSE(input_joystick_copy_name(0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joy_copy_name_invalid)
{
    CHECK_FALSE(input_joystick_copy_name(-1));
    CHECK_FALSE(input_joystick_copy_name(input_joystick_count()));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joy_can_rumble)
{
    sys_test_input_enable_joystick(1);
    sys_test_input_enable_joystick_rumble(1);
    input_update();
    CHECK_TRUE(input_joystick_can_rumble(0));

    sys_test_input_enable_joystick(1);
    sys_test_input_enable_joystick_rumble(0);
    input_update();
    CHECK_FALSE(input_joystick_can_rumble(0));

    sys_test_input_enable_joystick(0);
    sys_test_input_enable_joystick_rumble(1);
    input_update();
    CHECK_FALSE(input_joystick_can_rumble(0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joy_can_rumble_invalid)
{
    CHECK_FALSE(input_joystick_can_rumble(-1));
    CHECK_FALSE(input_joystick_can_rumble(input_joystick_count()));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joy_num_buttons)
{
    CHECK_INTEQUAL(input_joystick_num_buttons(0), 20);

    sys_test_input_enable_joystick(0);
    input_update();
    CHECK_INTEQUAL(input_joystick_num_buttons(0), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joy_num_buttons_invalid)
{
    CHECK_INTEQUAL(input_joystick_num_buttons(-1), 0);
    CHECK_INTEQUAL(input_joystick_num_buttons(input_joystick_count()), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joy_button_mapping)
{
    for (int i = 0; i < INPUT_JOYBUTTON__NUM; i++) {
        CHECK_INTEQUAL(input_joystick_button_mapping(0, i), -1);
    }

    sys_test_input_set_joy_button_mapping(INPUT_JOYBUTTON_START, 5);
    CHECK_INTEQUAL(input_joystick_button_mapping(0, INPUT_JOYBUTTON_START), 5);
    for (int i = 0; i < INPUT_JOYBUTTON__NUM; i++) {
        if (i != INPUT_JOYBUTTON_START) {
            CHECK_INTEQUAL(input_joystick_button_mapping(0, i), -1);
        }
    }

    sys_test_input_enable_joystick(0);
    input_update();
    for (int i = 0; i < INPUT_JOYBUTTON__NUM; i++) {
        CHECK_INTEQUAL(input_joystick_button_mapping(0, i), -1);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joy_button_mapping_invalid)
{
    CHECK_INTEQUAL(input_joystick_button_mapping(
                       -1, INPUT_JOYBUTTON_START), -1);
    CHECK_INTEQUAL(input_joystick_button_mapping(
                       input_joystick_count(), INPUT_JOYBUTTON_START), -1);
    CHECK_INTEQUAL(input_joystick_button_mapping(0, -1), -1);
    CHECK_INTEQUAL(input_joystick_button_mapping(0, INPUT_JOYBUTTON__NUM), -1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joy_num_sticks)
{
    CHECK_INTEQUAL(input_joystick_num_sticks(0), 2);

    sys_test_input_enable_joystick(0);
    input_update();
    CHECK_INTEQUAL(input_joystick_num_sticks(0), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joy_num_sticks_invalid)
{
    CHECK_INTEQUAL(input_joystick_num_sticks(-1), 0);
    CHECK_INTEQUAL(input_joystick_num_sticks(input_joystick_count()), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joy_button_state)
{
    sys_test_input_set_joy_button(0, 0, 1);
    input_update();
    CHECK_TRUE(input_joystick_button_state(0, 0));
    CHECK_FALSE(input_joystick_button_state(0, 1));

    sys_test_input_set_joy_button(0, 0, 0);
    sys_test_input_set_joy_button(0, 1, 1);
    input_update();
    CHECK_FALSE(input_joystick_button_state(0, 0));
    CHECK_TRUE(input_joystick_button_state(0, 1));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joy_button_state_invalid)
{
    sys_test_input_set_joy_button(0, 0, 1);
    input_update();

    CHECK_FALSE(input_joystick_button_state(-1, 0));
    CHECK_FALSE(input_joystick_button_state(input_joystick_count(), 0));
    CHECK_FALSE(input_joystick_button_state(0, -1));
    CHECK_FALSE(input_joystick_button_state(0, input_joystick_num_buttons(0)));

    sys_test_input_enable_joystick(0);
    input_update();
    CHECK_FALSE(input_joystick_button_state(0, 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joy_set_button_out_of_range)
{
    sys_test_input_set_joy_button(0, 20, 1);
    input_update();
    CHECK_FALSE(input_joystick_button_state(0, 0));
    CHECK_FALSE(input_joystick_button_state(0, 1));
    CHECK_FALSE(input_joystick_button_state(0, 19));

    sys_test_input_set_joy_button(0, INPUT_MAX_JOYSTICK_BUTTONS, 1);
    input_update();
    CHECK_FALSE(input_joystick_button_state(0, 0));
    CHECK_FALSE(input_joystick_button_state(0, 1));
    CHECK_FALSE(input_joystick_button_state(0, 19));

    sys_test_input_set_joy_button(1, 0, 1);
    input_update();
    CHECK_FALSE(input_joystick_button_state(0, 0));
    CHECK_FALSE(input_joystick_button_state(0, 1));
    CHECK_FALSE(input_joystick_button_state(0, 19));

    sys_test_input_set_joy_button(INPUT_MAX_JOYSTICKS, 0, 1);
    input_update();
    CHECK_FALSE(input_joystick_button_state(0, 0));
    CHECK_FALSE(input_joystick_button_state(0, 1));
    CHECK_FALSE(input_joystick_button_state(0, 19));

    sys_test_input_set_joy_button(0, 0, 1);
    input_update();

    sys_test_input_set_joy_button(0, 20, 0);
    input_update();
    CHECK_TRUE(input_joystick_button_state(0, 0));
    CHECK_FALSE(input_joystick_button_state(0, 1));
    CHECK_FALSE(input_joystick_button_state(0, 19));

    sys_test_input_set_joy_button(0, INPUT_MAX_JOYSTICK_BUTTONS, 0);
    input_update();
    CHECK_TRUE(input_joystick_button_state(0, 0));
    CHECK_FALSE(input_joystick_button_state(0, 1));
    CHECK_FALSE(input_joystick_button_state(0, 19));

    sys_test_input_set_joy_button(1, 0, 0);
    input_update();
    CHECK_TRUE(input_joystick_button_state(0, 0));
    CHECK_FALSE(input_joystick_button_state(0, 1));
    CHECK_FALSE(input_joystick_button_state(0, 19));

    sys_test_input_set_joy_button(INPUT_MAX_JOYSTICKS, 0, 0);
    input_update();
    CHECK_TRUE(input_joystick_button_state(0, 0));
    CHECK_FALSE(input_joystick_button_state(0, 1));
    CHECK_FALSE(input_joystick_button_state(0, 19));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joy_too_many_buttons)
{
    sys_test_input_set_joy_num_buttons(INPUT_MAX_JOYSTICK_BUTTONS + 1);
    input_update();
    CHECK_INTEQUAL(input_joystick_num_buttons(0), INPUT_MAX_JOYSTICK_BUTTONS);

    sys_test_input_set_joy_button(0, INPUT_MAX_JOYSTICK_BUTTONS, 1);
    input_update();
    CHECK_FALSE(input_joystick_button_state(0, INPUT_MAX_JOYSTICK_BUTTONS));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joy_read_stick)
{
    float x, y;

    sys_test_input_set_joy_stick(0, 0, -1, +1);
    sys_test_input_set_joy_stick(0, 1, +0.5, -0.5);
    input_update();
    x = y = 0;
    input_joystick_read_stick(0, 0, &x, &y);
    CHECK_FLOATEQUAL(x, -1);
    CHECK_FLOATEQUAL(y, +1);
    input_joystick_read_stick(0, 1, &x, &y);
    CHECK_FLOATEQUAL(x, +0.5);
    CHECK_FLOATEQUAL(y, -0.5);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joy_read_stick_null_pointer)
{
    float x, y;

    sys_test_input_set_joy_stick(0, 0, -1, +1);
    input_update();
    x = y = 0;
    input_joystick_read_stick(0, 0, &x, NULL);
    CHECK_FLOATEQUAL(x, -1);
    input_joystick_read_stick(0, 0, NULL, &y);
    CHECK_FLOATEQUAL(y, +1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joy_read_stick_invalid)
{
    float x, y;

    sys_test_input_set_joy_stick(0, 0, -1, +1);
    input_update();

    x = y = 1;
    input_joystick_read_stick(-1, 0, &x, &y);
    CHECK_FLOATEQUAL(x, 0);
    CHECK_FLOATEQUAL(y, 0);

    x = y = 1;
    input_joystick_read_stick(-1, 0, &x, NULL);
    input_joystick_read_stick(-1, 0, NULL, &y);
    CHECK_FLOATEQUAL(x, 0);
    CHECK_FLOATEQUAL(y, 0);

    x = y = 1;
    input_joystick_read_stick(input_joystick_count(), 0, &x, &y);
    CHECK_FLOATEQUAL(x, 0);
    CHECK_FLOATEQUAL(y, 0);

    x = y = 1;
    input_joystick_read_stick(0, -1, &x, &y);
    CHECK_FLOATEQUAL(x, 0);
    CHECK_FLOATEQUAL(y, 0);

    x = y = 1;
    input_joystick_read_stick(0, input_joystick_num_sticks(0), &x, &y);
    CHECK_FLOATEQUAL(x, 0);
    CHECK_FLOATEQUAL(y, 0);

    sys_test_input_enable_joystick(0);
    input_update();
    x = y = 1;
    input_joystick_read_stick(0, 0, &x, &y);
    CHECK_FLOATEQUAL(x, 0);
    CHECK_FLOATEQUAL(y, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joy_set_stick_out_of_range)
{
    float x, y;

    sys_test_input_set_joy_stick(0, 2, +0.5, -0.5);
    input_update();
    x = y = -5;
    input_joystick_read_stick(0, 0, &x, &y);
    CHECK_FLOATEQUAL(x, 0);
    CHECK_FLOATEQUAL(y, 0);
    x = y = -5;
    input_joystick_read_stick(0, 1, &x, &y);
    CHECK_FLOATEQUAL(x, 0);
    CHECK_FLOATEQUAL(y, 0);

    sys_test_input_set_joy_stick(0, INPUT_MAX_JOYSTICK_STICKS, +0.5, -0.5);
    input_update();
    x = y = -5;
    input_joystick_read_stick(0, 0, &x, &y);
    CHECK_FLOATEQUAL(x, 0);
    CHECK_FLOATEQUAL(y, 0);
    x = y = -5;
    input_joystick_read_stick(0, 1, &x, &y);
    CHECK_FLOATEQUAL(x, 0);
    CHECK_FLOATEQUAL(y, 0);

    sys_test_input_set_joy_stick(1, 0, +0.5, -0.5);
    input_update();
    x = y = -5;
    input_joystick_read_stick(0, 0, &x, &y);
    CHECK_FLOATEQUAL(x, 0);
    CHECK_FLOATEQUAL(y, 0);
    x = y = -5;
    input_joystick_read_stick(0, 1, &x, &y);
    CHECK_FLOATEQUAL(x, 0);
    CHECK_FLOATEQUAL(y, 0);

    sys_test_input_set_joy_stick(INPUT_MAX_JOYSTICKS, 0, +0.5, -0.5);
    input_update();
    x = y = -5;
    input_joystick_read_stick(0, 0, &x, &y);
    CHECK_FLOATEQUAL(x, 0);
    CHECK_FLOATEQUAL(y, 0);
    x = y = -5;
    input_joystick_read_stick(0, 1, &x, &y);
    CHECK_FLOATEQUAL(x, 0);
    CHECK_FLOATEQUAL(y, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joy_too_many_sticks)
{
    sys_test_input_set_joy_num_sticks(INPUT_MAX_JOYSTICK_STICKS + 1);
    input_update();
    CHECK_INTEQUAL(input_joystick_num_sticks(0), INPUT_MAX_JOYSTICK_STICKS);

    sys_test_input_set_joy_stick(0, INPUT_MAX_JOYSTICK_STICKS, 0.25, 0.25);
    input_update();
    float x, y;
    x = y = -5;
    input_joystick_read_stick(0, INPUT_MAX_JOYSTICK_STICKS, &x, &y);
    CHECK_FLOATEQUAL(x, 0);
    CHECK_FLOATEQUAL(y, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joy_read_dpad)
{
    sys_test_input_set_joy_dpad(0, -1, +1);
    input_update();

    int x = 0, y = 0;
    input_joystick_read_dpad(0, &x, &y);
    CHECK_INTEQUAL(x, -1);
    CHECK_INTEQUAL(y, +1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joy_read_dpad_null_pointer)
{
    sys_test_input_set_joy_dpad(0, -1, +1);
    input_update();

    int x = 0, y = 0;
    input_joystick_read_dpad(0, &x, NULL);
    CHECK_INTEQUAL(x, -1);
    input_joystick_read_dpad(0, NULL, &y);
    CHECK_INTEQUAL(y, +1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joy_read_dpad_invalid)
{
    sys_test_input_set_joy_dpad(0, -1, +1);
    input_update();

    int x = 1, y = 1;
    input_joystick_read_dpad(-1, &x, &y);
    CHECK_INTEQUAL(x, 0);
    CHECK_INTEQUAL(y, 0);

    x = y = 1;
    input_joystick_read_dpad(-1, &x, NULL);
    input_joystick_read_dpad(-1, NULL, &y);
    CHECK_INTEQUAL(x, 0);
    CHECK_INTEQUAL(y, 0);

    x = y = 1;
    input_joystick_read_dpad(input_joystick_count(), &x, &y);
    CHECK_INTEQUAL(x, 0);
    CHECK_INTEQUAL(y, 0);

    sys_test_input_enable_joystick(0);
    input_update();
    x = y = 1;
    input_joystick_read_dpad(0, &x, &y);
    CHECK_INTEQUAL(x, 0);
    CHECK_INTEQUAL(y, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joy_set_dpad_out_of_range)
{
    int x, y;

    sys_test_input_set_joy_dpad(1, -1, +1);
    input_update();
    x = y = -5;
    input_joystick_read_dpad(0, &x, &y);
    CHECK_INTEQUAL(x, 0);
    CHECK_INTEQUAL(y, 0);

    sys_test_input_set_joy_dpad(INPUT_MAX_JOYSTICKS, -1, +1);
    input_update();
    x = y = -5;
    input_joystick_read_dpad(0, &x, &y);
    CHECK_INTEQUAL(x, 0);
    CHECK_INTEQUAL(y, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joy_rumble)
{
    input_joystick_rumble(0, 0.75, 0.5, 0.25);
    CHECK_FLOATEQUAL(sys_test_input_get_rumble_left(), 0.75);
    CHECK_FLOATEQUAL(sys_test_input_get_rumble_right(), 0.5);
    CHECK_FLOATEQUAL(sys_test_input_get_rumble_time(), 0.25);

    input_joystick_rumble(0, 0, 0, 0);
    CHECK_FLOATEQUAL(sys_test_input_get_rumble_left(), 0);
    CHECK_FLOATEQUAL(sys_test_input_get_rumble_right(), 0);
    CHECK_FLOATEQUAL(sys_test_input_get_rumble_time(), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joy_rumble_bounds)
{
    input_joystick_rumble(0, -1, -1, 1);
    CHECK_FLOATEQUAL(sys_test_input_get_rumble_left(), 0);
    CHECK_FLOATEQUAL(sys_test_input_get_rumble_right(), 0);
    CHECK_FLOATEQUAL(sys_test_input_get_rumble_time(), 1);

    input_joystick_rumble(0, 2, 2, 3);
    CHECK_FLOATEQUAL(sys_test_input_get_rumble_left(), 1);
    CHECK_FLOATEQUAL(sys_test_input_get_rumble_right(), 1);
    CHECK_FLOATEQUAL(sys_test_input_get_rumble_time(), 3);

    input_joystick_rumble(0, -1, +1, -1);
    CHECK_FLOATEQUAL(sys_test_input_get_rumble_left(), 0);
    CHECK_FLOATEQUAL(sys_test_input_get_rumble_right(), 1);
    CHECK_FLOATEQUAL(sys_test_input_get_rumble_time(), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joy_rumble_invalid)
{
    input_joystick_rumble(0, 0, 0, 0);
    CHECK_FLOATEQUAL(sys_test_input_get_rumble_left(), 0);
    CHECK_FLOATEQUAL(sys_test_input_get_rumble_right(), 0);
    CHECK_FLOATEQUAL(sys_test_input_get_rumble_time(), 0);

    input_joystick_rumble(-1, 0.75, 0.5, 0.25);
    CHECK_FLOATEQUAL(sys_test_input_get_rumble_left(), 0);
    CHECK_FLOATEQUAL(sys_test_input_get_rumble_right(), 0);
    CHECK_FLOATEQUAL(sys_test_input_get_rumble_time(), 0);

    input_joystick_rumble(input_joystick_count(), 0.75, 0.5, 0.25);
    CHECK_FLOATEQUAL(sys_test_input_get_rumble_left(), 0);
    CHECK_FLOATEQUAL(sys_test_input_get_rumble_right(), 0);
    CHECK_FLOATEQUAL(sys_test_input_get_rumble_time(), 0);

    sys_test_input_enable_joystick(1);
    sys_test_input_enable_joystick_rumble(0);
    input_update();
    input_joystick_rumble(0, 0.75, 0.5, 0.25);
    CHECK_FLOATEQUAL(sys_test_input_get_rumble_left(), 0);
    CHECK_FLOATEQUAL(sys_test_input_get_rumble_right(), 0);
    CHECK_FLOATEQUAL(sys_test_input_get_rumble_time(), 0);

    sys_test_input_enable_joystick(0);
    sys_test_input_enable_joystick_rumble(1);
    input_update();
    input_joystick_rumble(0, 0.75, 0.5, 0.25);
    CHECK_FLOATEQUAL(sys_test_input_get_rumble_left(), 0);
    CHECK_FLOATEQUAL(sys_test_input_get_rumble_right(), 0);
    CHECK_FLOATEQUAL(sys_test_input_get_rumble_time(), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joy_event_connect)
{
    input_set_event_callback(receive_event);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    sys_test_input_set_joy_connected(1);
    input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_CONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(2.0);
    sys_test_input_set_joy_connected(0);
    input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DISCONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(3.0);
    sys_test_input_set_joy_connected(0);  // Should not generate an event.
    input_update();
    CHECK_INTEQUAL(num_events, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joy_event_connect_initial)
{
    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    input_cleanup();
    sys_test_input_set_joy_connected_on_init(1);
    CHECK_TRUE(input_init());
    input_set_event_callback(receive_event);

    sys_test_time_set_seconds(1.5);
    input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_CONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joy_event_connect_on_num_devices_change)
{
    input_set_event_callback(receive_event);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    sys_test_input_set_joy_connected(1);
    sys_test_input_set_joy_num_devices(2);
    input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_CONNECTED);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 1.0);
    CHECK_INTEQUAL(events[1].joystick.device, 1);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(2.0);
    sys_test_input_set_joy_num_devices(1);
    input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DISCONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].joystick.device, 1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joy_event_button_down)
{
    input_set_event_callback(receive_event);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    sys_test_input_set_joy_button(0, 2, 1);
    input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index, 2);

    /* Events with out-of-range device/button index values should be
     * passed on anyway. */
    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(2.0);
    sys_test_input_set_joy_button(1, 22, 1);
    input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].joystick.device, 1);
    CHECK_INTEQUAL(events[0].joystick.index, 22);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joy_event_button_up)
{
    input_set_event_callback(receive_event);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    sys_test_input_set_joy_button(0, 2, 1);
    sys_test_input_set_joy_button(0, 2, 0);
    input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_BUTTON_UP);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 1.0);
    CHECK_INTEQUAL(events[1].joystick.device, 0);
    CHECK_INTEQUAL(events[1].joystick.index, 2);

    /* Events with out-of-range device/button index values should be
     * passed on anyway. */
    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(2.0);
    sys_test_input_set_joy_button(1, 22, 1);
    sys_test_input_set_joy_button(1, 22, 0);
    input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_BUTTON_UP);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 2.0);
    CHECK_INTEQUAL(events[1].joystick.device, 1);
    CHECK_INTEQUAL(events[1].joystick.index, 22);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joy_event_dpad_change)
{
    input_set_event_callback(receive_event);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    sys_test_input_set_joy_dpad(0, +1, -1);
    input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_FLOATEQUAL(events[0].joystick.x, +1);
    CHECK_FLOATEQUAL(events[0].joystick.y, -1);

    /* Events with out-of-range device index values should be passed on
     * anyway. */
    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(2.0);
    sys_test_input_set_joy_dpad(1, -1, +1);
    input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].joystick.device, 1);
    CHECK_FLOATEQUAL(events[0].joystick.x, -1);
    CHECK_FLOATEQUAL(events[0].joystick.y, +1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joy_event_stick_change)
{
    input_set_event_callback(receive_event);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    sys_test_input_set_joy_stick(0, 1, +0.25, -0.75);
    input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index, 1);
    CHECK_FLOATEQUAL(events[0].joystick.x, +0.25);
    CHECK_FLOATEQUAL(events[0].joystick.y, -0.75);

    /* Events with out-of-range device/stick index values should be
     * passed on anyway. */
    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(2.0);
    sys_test_input_set_joy_stick(1, 2, +0.75, -0.25);
    input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].joystick.device, 1);
    CHECK_INTEQUAL(events[0].joystick.index, 2);
    CHECK_FLOATEQUAL(events[0].joystick.x, +0.75);
    CHECK_FLOATEQUAL(events[0].joystick.y, -0.25);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_control_while_disabled)
{
    input_set_event_callback(receive_event);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_input_enable_joystick(0);
    sys_test_input_set_joy_button(0, 0, 1);
    sys_test_input_set_joy_button(0, 1, 1);
    sys_test_input_set_joy_button(0, 0, 0);
    sys_test_input_set_joy_stick(0, 0, +0.5, -0.5);
    sys_test_input_set_joy_dpad(0, -1, +1);
    input_update();

    CHECK_FALSE(input_joystick_button_state(0, 0));
    CHECK_FALSE(input_joystick_button_state(0, 1));

    float xf = -5, yf = -5;
    input_joystick_read_stick(0, 0, &xf, &yf);
    CHECK_FLOATEQUAL(xf, 0);
    CHECK_FLOATEQUAL(yf, 0);

    int xi = -5, yi = -5;
    input_joystick_read_dpad(0, &xi, &yi);
    CHECK_INTEQUAL(xi, 0);
    CHECK_INTEQUAL(yi, 0);

    CHECK_INTEQUAL(num_events, 0);

    return 1;
}

/*************************************************************************/
/**************************** Keyboard tests *****************************/
/*************************************************************************/

TEST(test_keyboard_present)
{
    CHECK_TRUE(input_keyboard_present());

    sys_test_input_enable_keyboard(0);
    input_update();
    CHECK_FALSE(input_keyboard_present());

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_keyboard_is_full)
{
    CHECK_TRUE(input_keyboard_is_full());

    sys_test_input_enable_keyboard_full(0);
    input_update();
    CHECK_FALSE(input_keyboard_is_full());

    sys_test_input_enable_keyboard_full(1);
    sys_test_input_enable_keyboard(0);
    input_update();
    CHECK_FALSE(input_keyboard_is_full());

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_key_state)
{
    sys_test_input_press_key(KEY_A, 1);
    input_update();
    CHECK_TRUE(input_key_state(KEY_A));
    CHECK_FALSE(input_key_state(KEY_B));

    sys_test_input_press_key(KEY_B, 2);
    sys_test_input_release_key(KEY_A, 1);
    input_update();
    CHECK_FALSE(input_key_state(KEY_A));
    CHECK_TRUE(input_key_state(KEY_B));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_key_state_invalid)
{
    CHECK_FALSE(input_key_state(KEY__NONE));
    CHECK_FALSE(input_key_state(KEY__LAST));

    sys_test_input_press_key(KEY_A, 1);
    sys_test_input_enable_keyboard(0);
    input_update();
    CHECK_FALSE(input_key_state(KEY_A));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_key_modifier_state)
{
    sys_test_input_press_key(KEY_LEFTSHIFT, 1);
    input_update();
    CHECK_INTEQUAL(input_key_modifier_state(), KEYMOD_SHIFT);

    sys_test_input_press_key(KEY_LEFTCONTROL, 2);
    input_update();
    CHECK_INTEQUAL(input_key_modifier_state(), KEYMOD_SHIFT | KEYMOD_CONTROL);

    sys_test_input_release_key(KEY_LEFTSHIFT, 1);
    input_update();
    CHECK_INTEQUAL(input_key_modifier_state(), KEYMOD_CONTROL);

    sys_test_input_press_key(KEY_RIGHTCONTROL, 3);
    input_update();
    CHECK_INTEQUAL(input_key_modifier_state(), KEYMOD_CONTROL);

    sys_test_input_release_key(KEY_LEFTCONTROL, 2);
    input_update();
    CHECK_INTEQUAL(input_key_modifier_state(), KEYMOD_CONTROL);

    sys_test_input_release_key(KEY_RIGHTCONTROL, 3);
    input_update();
    CHECK_INTEQUAL(input_key_modifier_state(), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_key_modifier_state_exhaustive)
{
    static const struct {int key, modifier;} modifiers[] = {
        {KEY_LEFTSHIFT,    KEYMOD_SHIFT},
        {KEY_RIGHTSHIFT,   KEYMOD_SHIFT},
        {KEY_LEFTCONTROL,  KEYMOD_CONTROL},
        {KEY_RIGHTCONTROL, KEYMOD_CONTROL},
        {KEY_LEFTALT,      KEYMOD_ALT},
        {KEY_RIGHTALT,     KEYMOD_ALT},
        {KEY_LEFTMETA,     KEYMOD_META},
        {KEY_RIGHTMETA,    KEYMOD_META},
        {KEY_LEFTSUPER,    KEYMOD_SUPER},
        {KEY_RIGHTSUPER,   KEYMOD_SUPER},
        {KEY_NUMLOCK,      KEYMOD_NUMLOCK},
        {KEY_CAPSLOCK,     KEYMOD_CAPSLOCK},
        {KEY_SCROLLLOCK,   KEYMOD_SCROLLLOCK},
    };

    for (int i = 0; i < lenof(modifiers); i++) {
        sys_test_input_press_key(modifiers[i].key, i+1);
        input_update();
        if (input_key_modifier_state() != modifiers[i].modifier) {
            FAIL("input_key_modifier_state() was %d but should have been %d"
                 " for iteration %d", input_key_modifier_state(),
                 modifiers[i].modifier, i);
        }

        sys_test_input_release_key(modifiers[i].key, i+1);
        input_update();
        if (input_key_modifier_state() != 0) {
            FAIL("input_key_modifier_state() was %d but should have been 0"
                 " for iteration %d", input_key_modifier_state(), i);
        }
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_key_last_pressed)
{
    CHECK_INTEQUAL(input_key_last_pressed(), 0);

    sys_test_input_press_key(KEY_A, 1);
    input_update();
    CHECK_INTEQUAL(input_key_last_pressed(), KEY_A);

    input_update();
    CHECK_INTEQUAL(input_key_last_pressed(), 0); // Nothing pressed this cycle.

    sys_test_input_press_key(KEY_B, 2);
    sys_test_input_press_key(KEY_C, 3);
    sys_test_input_press_key(KEY_B, 2);
    sys_test_input_release_key(KEY_A, 1);
    input_update();
    CHECK_INTEQUAL(input_key_last_pressed(), KEY_C);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_key_last_pressed_invalid)
{
    sys_test_input_press_key(KEY_A, 1);
    sys_test_input_enable_keyboard(0);
    input_update();
    CHECK_INTEQUAL(input_key_last_pressed(), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_key_event_down)
{
    input_set_event_callback(receive_event);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    sys_test_input_press_key(KEY_A, 1);
    input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_A);
    CHECK_INTEQUAL(events[0].keyboard.modifiers, 0);
    CHECK_INTEQUAL(events[0].keyboard.system_key, 1);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(2.0);
    sys_test_input_press_key(KEY_A, 1);
    input_update();
    CHECK_INTEQUAL(num_events, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_key_event_up)
{
    input_set_event_callback(receive_event);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    sys_test_input_press_key(KEY_A, 1);
    input_update();
    sys_test_input_release_key(KEY_A, 1);
    input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[1].detail, INPUT_KEYBOARD_KEY_UP);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 1.0);
    CHECK_INTEQUAL(events[1].keyboard.key, KEY_A);
    CHECK_INTEQUAL(events[1].keyboard.modifiers, 0);
    CHECK_INTEQUAL(events[1].keyboard.system_key, 1);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(2.0);
    sys_test_input_release_key(KEY_A, 1);
    input_update();
    CHECK_INTEQUAL(num_events, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_key_event_with_modifiers)
{
    input_set_event_callback(receive_event);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    sys_test_input_press_key(KEY_LEFTSHIFT, 10);
    input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_LEFTSHIFT);
    CHECK_INTEQUAL(events[0].keyboard.modifiers, KEYMOD_SHIFT);
    CHECK_INTEQUAL(events[0].keyboard.system_key, 10);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(2.0);
    sys_test_input_press_key(KEY_A, 1);
    input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_A);
    CHECK_INTEQUAL(events[0].keyboard.modifiers, KEYMOD_SHIFT);
    CHECK_INTEQUAL(events[0].keyboard.system_key, 1);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(3.0);
    sys_test_input_release_key(KEY_LEFTSHIFT, 10);
    input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_UP);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 3.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_LEFTSHIFT);
    CHECK_INTEQUAL(events[0].keyboard.modifiers, 0);
    CHECK_INTEQUAL(events[0].keyboard.system_key, 10);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(4.0);
    sys_test_input_release_key(KEY_A, 1);
    input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_UP);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 4.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_A);
    CHECK_INTEQUAL(events[0].keyboard.modifiers, 0);
    CHECK_INTEQUAL(events[0].keyboard.system_key, 1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_key_event_system_key_down)
{
    input_set_event_callback(receive_event);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    sys_test_input_press_key(KEY__NONE, 123);
    input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_SYSTEM_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY__NONE);
    CHECK_INTEQUAL(events[0].keyboard.modifiers, 0);
    CHECK_INTEQUAL(events[0].keyboard.system_key, 123);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_key_event_system_key_up)
{
    input_set_event_callback(receive_event);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    sys_test_input_press_key(KEY__NONE, 123);
    input_update();
    sys_test_input_release_key(KEY__NONE, 456);
    input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[1].detail, INPUT_KEYBOARD_SYSTEM_KEY_UP);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 1.0);
    CHECK_INTEQUAL(events[1].keyboard.key, KEY__NONE);
    CHECK_INTEQUAL(events[1].keyboard.modifiers, 0);
    CHECK_INTEQUAL(events[1].keyboard.system_key, 456);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_keyboard_control_while_disabled)
{
    input_set_event_callback(receive_event);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_input_enable_keyboard(0);
    sys_test_input_press_key(KEY_A, 1);
    sys_test_input_press_key(KEY_B, 2);
    sys_test_input_release_key(KEY_A, 1);
    input_update();
    CHECK_FALSE(input_key_state(KEY_A));
    CHECK_FALSE(input_key_state(KEY_B));
    CHECK_INTEQUAL(input_key_modifier_state(), 0);
    CHECK_INTEQUAL(input_key_last_pressed(), 0);
    CHECK_INTEQUAL(num_events, 0);

    return 1;
}

/*************************************************************************/
/********************** Memory pressure event tests **********************/
/*************************************************************************/

TEST(test_memory_low)
{
    input_set_event_callback(receive_event);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    sys_test_input_send_memory_low(123, 456);
    input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MEMORY);
    CHECK_INTEQUAL(events[0].detail, INPUT_MEMORY_LOW);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].memory.used_bytes, 123);
    CHECK_INTEQUAL(events[0].memory.free_bytes, 456);

    return 1;
}

/*************************************************************************/
/****************************** Mouse tests ******************************/
/*************************************************************************/

TEST(test_mouse_present)
{
    CHECK_TRUE(input_mouse_present());

    sys_test_input_enable_mouse(0);
    input_update();
    CHECK_FALSE(input_mouse_present());

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mouse_set_get_position)
{
    float x, y;

    input_mouse_set_position(0.25, 0.75);
    input_update();
    x = y = -1;
    input_mouse_get_position(&x, &y);
    CHECK_FLOATEQUAL(x, 0.25);
    CHECK_FLOATEQUAL(y, 0.75);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mouse_move)
{
    float x, y;

    input_mouse_set_position(0.25, 0.75);
    input_update();
    x = y = -1;
    input_mouse_get_position(&x, &y);
    CHECK_FLOATEQUAL(x, 0.25);
    CHECK_FLOATEQUAL(y, 0.75);

    sys_test_input_move_mouse(+0.125, -0.125);
    input_update();
    x = y = -1;
    input_mouse_get_position(&x, &y);
    CHECK_FLOATEQUAL(x, 0.375);
    CHECK_FLOATEQUAL(y, 0.625);

    sys_test_input_move_mouse(+2, -2);
    input_update();
    x = y = -1;
    input_mouse_get_position(&x, &y);
    CHECK_FLOATEQUAL(x, 65535.0f/65536.0f);
    CHECK_FLOATEQUAL(y, 0);

    sys_test_input_move_mouse(-2, +2);
    input_update();
    x = y = -1;
    input_mouse_get_position(&x, &y);
    CHECK_FLOATEQUAL(x, 0);
    CHECK_FLOATEQUAL(y, 65535.0f/65536.0f);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mouse_get_position_null)
{
    float x, y;

    input_mouse_set_position(0.25, 0.75);
    input_update();

    x = y = -1;
    input_mouse_get_position(&x, NULL);
    CHECK_FLOATEQUAL(x, 0.25);
    input_mouse_get_position(NULL, &y);
    CHECK_FLOATEQUAL(y, 0.75);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mouse_set_position_bounds)
{
    float x, y;

    input_mouse_set_position(-1, -1);
    input_update();
    x = y = -1;
    input_mouse_get_position(&x, &y);
    CHECK_FLOATEQUAL(x, 0);
    CHECK_FLOATEQUAL(y, 0);

    input_mouse_set_position(1, 1);
    input_update();
    x = y = -1;
    input_mouse_get_position(&x, &y);
    CHECK_FLOATEQUAL(x, 65535.0f/65536.0f);
    CHECK_FLOATEQUAL(y, 65535.0f/65536.0f);

    input_mouse_set_position(2, 2);
    input_update();
    x = y = -1;
    input_mouse_get_position(&x, &y);
    CHECK_FLOATEQUAL(x, 65535.0f/65536.0f);
    CHECK_FLOATEQUAL(y, 65535.0f/65536.0f);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mouse_get_position_invalid)
{
    float x, y;

    input_mouse_set_position(0.25, 0.75);
    input_update();
    sys_test_input_enable_mouse(0);
    input_update();
    x = y = -1;
    input_mouse_get_position(&x, &y);
    CHECK_FLOATEQUAL(x, 0.5);
    CHECK_FLOATEQUAL(y, 0.5);

    input_mouse_set_position(0.25, 0.75);
    input_update();
    x = y = -1;
    input_mouse_get_position(&x, &y);
    CHECK_FLOATEQUAL(x, 0.5);
    CHECK_FLOATEQUAL(y, 0.5);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mouse_buttons)
{
    sys_test_input_press_mouse_buttons(1, 0, 1);
    input_update();
    CHECK_TRUE(input_mouse_left_button_state());
    CHECK_FALSE(input_mouse_middle_button_state());
    CHECK_TRUE(input_mouse_right_button_state());

    sys_test_input_press_mouse_buttons(0, 1, 0);
    sys_test_input_release_mouse_buttons(1, 0, 1);
    input_update();
    CHECK_FALSE(input_mouse_left_button_state());
    CHECK_TRUE(input_mouse_middle_button_state());
    CHECK_FALSE(input_mouse_right_button_state());

    sys_test_input_press_mouse_buttons(0, 0, 1);
    sys_test_input_release_mouse_buttons(0, 1, 0);
    input_update();
    CHECK_FALSE(input_mouse_left_button_state());
    CHECK_FALSE(input_mouse_middle_button_state());
    CHECK_TRUE(input_mouse_right_button_state());

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mouse_buttons_invalid)
{
    sys_test_input_press_mouse_buttons(1, 1, 1);
    input_update();
    sys_test_input_enable_mouse(0);
    input_update();
    CHECK_FALSE(input_mouse_left_button_state());
    CHECK_FALSE(input_mouse_middle_button_state());
    CHECK_FALSE(input_mouse_right_button_state());

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mouse_scroll)
{
    sys_test_input_add_mouse_scroll(2, -3);
    input_update();
    CHECK_INTEQUAL(input_mouse_horiz_scroll(), 2);
    CHECK_INTEQUAL(input_mouse_vert_scroll(), -3);

    sys_test_input_add_mouse_scroll(1, 4);
    input_update();
    sys_test_input_add_mouse_scroll(3, -1);
    sys_test_input_add_mouse_scroll(0, -5);
    input_update();
    CHECK_INTEQUAL(input_mouse_horiz_scroll(), 3);
    CHECK_INTEQUAL(input_mouse_vert_scroll(), -6);

    input_update();
    CHECK_INTEQUAL(input_mouse_horiz_scroll(), 0);
    CHECK_INTEQUAL(input_mouse_vert_scroll(), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mouse_event_move)
{
    input_mouse_set_position(0.25, 0.25);
    input_update();
    input_set_event_callback(receive_event);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    sys_test_input_move_mouse(0.25, 0);
    input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.5);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.25);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(2.0);
    sys_test_input_move_mouse(0, 0.125);
    input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.5);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.375);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(3.0);
    input_mouse_set_position(0.75, 0.25);
    input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 3.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.75);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.25);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mouse_event_lmb)
{
    input_mouse_set_position(0.25, 0.5);
    input_update();
    input_set_event_callback(receive_event);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    sys_test_input_press_mouse_buttons(1, 0, 0);
    input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_LMB_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.25);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.5);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(1.5);
    sys_test_input_press_mouse_buttons(1, 0, 0);
    input_update();
    CHECK_INTEQUAL(num_events, 0);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(2.0);
    sys_test_input_release_mouse_buttons(1, 0, 0);
    input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_LMB_UP);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.25);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.5);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(2.5);
    sys_test_input_release_mouse_buttons(1, 0, 0);
    input_update();
    CHECK_INTEQUAL(num_events, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mouse_event_mmb)
{
    input_mouse_set_position(0.25, 0.5);
    input_update();
    input_set_event_callback(receive_event);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    sys_test_input_press_mouse_buttons(0, 1, 0);
    input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MMB_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.25);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.5);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(1.5);
    sys_test_input_press_mouse_buttons(0, 1, 0);
    input_update();
    CHECK_INTEQUAL(num_events, 0);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(2.0);
    sys_test_input_release_mouse_buttons(0, 1, 0);
    input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MMB_UP);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.25);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.5);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(2.5);
    sys_test_input_release_mouse_buttons(0, 1, 0);
    input_update();
    CHECK_INTEQUAL(num_events, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mouse_event_rmb)
{
    input_mouse_set_position(0.25, 0.5);
    input_update();
    input_set_event_callback(receive_event);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    sys_test_input_press_mouse_buttons(0, 0, 1);
    input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_RMB_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.25);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.5);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(1.5);
    sys_test_input_press_mouse_buttons(0, 0, 1);
    input_update();
    CHECK_INTEQUAL(num_events, 0);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(2.0);
    sys_test_input_release_mouse_buttons(0, 0, 1);
    input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_RMB_UP);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.25);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.5);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(2.5);
    sys_test_input_release_mouse_buttons(0, 0, 1);
    input_update();
    CHECK_INTEQUAL(num_events, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mouse_event_scroll)
{
    input_mouse_set_position(0.25, 0.5);
    input_update();
    input_set_event_callback(receive_event);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    sys_test_input_add_mouse_scroll(3, 0);
    input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_SCROLL_H);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.25);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.5);
    CHECK_INTEQUAL(events[0].mouse.scroll, 3);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(2.0);
    sys_test_input_add_mouse_scroll(0, -2);
    input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_SCROLL_V);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.25);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.5);
    CHECK_INTEQUAL(events[0].mouse.scroll, -2);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mouse_control_while_disabled)
{
    input_set_event_callback(receive_event);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_input_enable_mouse(0);
    sys_test_input_move_mouse(1, 1);
    sys_test_input_press_mouse_buttons(1, 1, 1);
    sys_test_input_release_mouse_buttons(1, 1, 1);
    sys_test_input_press_mouse_buttons(1, 1, 1);
    sys_test_input_add_mouse_scroll(3, -2);
    input_update();
    float x, y;
    input_mouse_get_position(&x, &y);
    CHECK_FLOATEQUAL(x, 0.5);
    CHECK_FLOATEQUAL(y, 0.5);
    CHECK_FALSE(input_mouse_left_button_state());
    CHECK_FALSE(input_mouse_middle_button_state());
    CHECK_FALSE(input_mouse_right_button_state());
    CHECK_INTEQUAL(input_mouse_horiz_scroll(), 0);
    CHECK_INTEQUAL(input_mouse_vert_scroll(), 0);
    CHECK_INTEQUAL(num_events, 0);

    return 1;
}

/*************************************************************************/
/*************************** Text input tests ****************************/
/*************************************************************************/

TEST(test_text_present)
{
    CHECK_TRUE(input_text_present());

    sys_test_input_enable_text_custom_interface(0);
    sys_test_input_enable_text_prompt(0);
    input_update();
    CHECK_FALSE(input_text_uses_custom_interface());
    CHECK_FALSE(input_text_can_display_prompt());

    sys_test_input_enable_text_custom_interface(0);
    sys_test_input_enable_text_prompt(1);
    input_update();
    CHECK_FALSE(input_text_uses_custom_interface());
    CHECK_FALSE(input_text_can_display_prompt());

    sys_test_input_enable_text_custom_interface(1);
    sys_test_input_enable_text_prompt(0);
    input_update();
    CHECK_TRUE(input_text_uses_custom_interface());
    CHECK_FALSE(input_text_can_display_prompt());

    sys_test_input_enable_text_custom_interface(1);
    sys_test_input_enable_text_prompt(1);
    input_update();
    CHECK_TRUE(input_text_uses_custom_interface());
    CHECK_TRUE(input_text_can_display_prompt());

    sys_test_input_enable_text(0);
    input_update();
    CHECK_FALSE(input_text_present());
    CHECK_FALSE(input_text_uses_custom_interface());
    CHECK_FALSE(input_text_can_display_prompt());

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_text_enable_disable)
{
    input_text_enable();
    CHECK_TRUE(sys_test_input_get_text_state());
    CHECK_STREQUAL(sys_test_input_get_text_prompt(), "");
    CHECK_STREQUAL(sys_test_input_get_text_default(), "");

    input_text_disable();
    CHECK_FALSE(sys_test_input_get_text_state());

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_text_enable_disable_default)
{
    input_text_enable_with_default_text("default");
    CHECK_TRUE(sys_test_input_get_text_state());
    CHECK_STREQUAL(sys_test_input_get_text_prompt(), "");
    CHECK_STREQUAL(sys_test_input_get_text_default(), "default");

    input_text_disable();
    CHECK_FALSE(sys_test_input_get_text_state());

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_text_enable_disable_prompt)
{
    input_text_enable_with_prompt("default", "prompt");
    CHECK_TRUE(sys_test_input_get_text_state());
    CHECK_STREQUAL(sys_test_input_get_text_prompt(), "prompt");
    CHECK_STREQUAL(sys_test_input_get_text_default(), "default");

    input_text_disable();
    CHECK_FALSE(sys_test_input_get_text_state());

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_text_enable_disable_invalid)
{
    sys_test_input_enable_text_prompt(0);
    input_update();
    input_text_enable_with_prompt("default", "prompt");
    CHECK_TRUE(sys_test_input_get_text_state());
    CHECK_STREQUAL(sys_test_input_get_text_prompt(), "");
    CHECK_STREQUAL(sys_test_input_get_text_default(), "default");
    input_text_disable();
    CHECK_FALSE(sys_test_input_get_text_state());

    sys_test_input_enable_text_custom_interface(0);
    input_update();
    input_text_enable_with_default_text("default");
    CHECK_TRUE(sys_test_input_get_text_state());
    CHECK_STREQUAL(sys_test_input_get_text_prompt(), "");
    CHECK_STREQUAL(sys_test_input_get_text_default(), "");
    input_text_disable();
    CHECK_FALSE(sys_test_input_get_text_state());

    sys_test_input_enable_text(0);
    input_update();
    input_text_enable();
    CHECK_FALSE(sys_test_input_get_text_state());
    input_text_disable();
    CHECK_FALSE(sys_test_input_get_text_state());

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_text_get_char)
{
    /* INPUT_TEXT_CANCELLED should be returned if no text has ever been
     * requested. */
    CHECK_INTEQUAL(input_text_get_char(), -INPUT_TEXT_CANCELLED);

    input_text_enable();
    ASSERT(sys_test_input_get_text_state());

    input_update();
    CHECK_INTEQUAL(input_text_get_char(), 0);

    sys_test_input_add_text_char('x');
    sys_test_input_add_text_char('y');
    input_update();
    CHECK_INTEQUAL(input_text_get_char(), 'x');
    CHECK_INTEQUAL(input_text_get_char(), 'y');
    CHECK_INTEQUAL(input_text_get_char(), 0);

    sys_test_input_enter_text("a—ÿ");
    input_update();
    CHECK_INTEQUAL(input_text_get_char(), 'a');
    CHECK_INTEQUAL(input_text_get_char(), 0x2014);
    CHECK_INTEQUAL(input_text_get_char(), 0x00FF);
    CHECK_INTEQUAL(input_text_get_char(), -INPUT_TEXT_DONE);
    /* INPUT_TEXT_DONE should be followed by INPUT_TEXT_CANCELLED. */
    CHECK_INTEQUAL(input_text_get_char(), -INPUT_TEXT_CANCELLED);
    /* INPUT_TEXT_CANCELLED should be sticky. */
    CHECK_INTEQUAL(input_text_get_char(), -INPUT_TEXT_CANCELLED);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_text_get_char_overflow)
{
    input_text_enable();
    ASSERT(sys_test_input_get_text_state());

    /* Check that we can get 999 characters in. */
    for (int i = 0; i < 998; i++) {
        sys_test_input_add_text_char('a');
    }
    sys_test_input_add_text_char('b');
    input_update();
    for (int i = 0; i < 998; i++) {
        const int32_t ch = input_text_get_char();
        if (ch != 'a') {
            FAIL("input_text_get_char() was %d but should have been %d at"
                 " index %d", ch, 'a', i);
        }
    }
    CHECK_INTEQUAL(input_text_get_char(), 'b');
    CHECK_INTEQUAL(input_text_get_char(), 0);

    /* Check that we can't get 1000 characters in. */
    for (int i = 0; i < 999; i++) {
        sys_test_input_add_text_char('a');
    }
    sys_test_input_add_text_char('b');
    input_update();
    for (int i = 0; i < 999; i++) {
        const int32_t ch = input_text_get_char();
        if (ch != 'a') {
            FAIL("input_text_get_char() was %d but should have been %d at"
                 " index %d", ch, 'a', i);
        }
    }
    CHECK_INTEQUAL(input_text_get_char(), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_text_get_char_overflow_move_buffer)
{
    input_text_enable();
    ASSERT(sys_test_input_get_text_state());

    for (int i = 0; i < 999; i++) {
        sys_test_input_add_text_char('a');
    }
    input_update();
    for (int i = 0; i < 500; i++) {
        const int32_t ch = input_text_get_char();
        if (ch != 'a') {
            FAIL("input_text_get_char() was %d but should have been %d at"
                 " index %d", ch, 'a', i);
        }
    }
    for (int i = 0; i < 499; i++) {
        sys_test_input_add_text_char('a');
    }
    sys_test_input_add_text_char('b');
    input_update();
    for (int i = 0; i < 998; i++) {
        const int32_t ch = input_text_get_char();
        if (ch != 'a') {
            FAIL("input_text_get_char() was %d but should have been %d at"
                 " index %d", ch, 'a', i);
        }
    }
    CHECK_INTEQUAL(input_text_get_char(), 'b');
    CHECK_INTEQUAL(input_text_get_char(), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_text_get_char_overflow_on_done)
{
    input_text_enable();
    ASSERT(sys_test_input_get_text_state());

    for (int i = 0; i < 997; i++) {
        sys_test_input_add_text_char('a');
    }
    sys_test_input_add_text_char('b');
    sys_test_input_add_text_char('c');
    sys_test_input_add_text_event(INPUT_TEXT_DONE);  // Should overwrite 'c'.
    input_update();
    for (int i = 0; i < 997; i++) {
        const int32_t ch = input_text_get_char();
        if (ch != 'a') {
            FAIL("input_text_get_char() was %d but should have been %d at"
                 " index %d", ch, 'a', i);
        }
    }
    CHECK_INTEQUAL(input_text_get_char(), 'b');
    CHECK_INTEQUAL(input_text_get_char(), -INPUT_TEXT_DONE);
    CHECK_INTEQUAL(input_text_get_char(), -INPUT_TEXT_CANCELLED);
    CHECK_INTEQUAL(input_text_get_char(), -INPUT_TEXT_CANCELLED);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_text_get_char_invalid)
{
    CHECK_INTEQUAL(input_text_get_char(), -INPUT_TEXT_CANCELLED);

    sys_test_input_enable_text(0);
    input_update();
    input_text_enable();
    sys_test_input_add_text_char('x');
    sys_test_input_add_text_event(INPUT_TEXT_DONE);
    input_update();
    CHECK_INTEQUAL(input_text_get_char(), -INPUT_TEXT_CANCELLED);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_text_event)
{
    input_text_enable();
    ASSERT(sys_test_input_get_text_state());

    text_input_len = 0;
    input_set_event_callback(receive_text_event);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    input_update();
    CHECK_INTEQUAL(num_events, 0);
    CHECK_INTEQUAL(text_input_len, 0);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(2.0);
    sys_test_input_add_text_char('x');
    sys_test_input_add_text_char('y');
    input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_TEXT);
    CHECK_INTEQUAL(events[1].detail, INPUT_TEXT_INPUT);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 2.0);
    CHECK_INTEQUAL(events[1].text.ch, 'y');
    CHECK_INTEQUAL(text_input_len, 2);
    CHECK_INTEQUAL(text_input[0].detail, INPUT_TEXT_INPUT);
    CHECK_INTEQUAL(text_input[0].ch, 'x');
    CHECK_INTEQUAL(text_input[1].detail, INPUT_TEXT_INPUT);
    CHECK_INTEQUAL(text_input[1].ch, 'y');

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(3.0);
    sys_test_input_enter_text("a—ÿ");
    input_update();
    CHECK_INTEQUAL(num_events, 4);
    CHECK_INTEQUAL(events[3].type, INPUT_EVENT_TEXT);
    CHECK_INTEQUAL(events[3].detail, INPUT_TEXT_DONE);
    CHECK_DOUBLEEQUAL(events[3].timestamp, 3.0);
    CHECK_INTEQUAL(text_input_len, 6);
    CHECK_INTEQUAL(text_input[0].detail, INPUT_TEXT_INPUT);
    CHECK_INTEQUAL(text_input[0].ch, 'x');
    CHECK_INTEQUAL(text_input[1].detail, INPUT_TEXT_INPUT);
    CHECK_INTEQUAL(text_input[1].ch, 'y');
    CHECK_INTEQUAL(text_input[2].detail, INPUT_TEXT_INPUT);
    CHECK_INTEQUAL(text_input[2].ch, 'a');
    CHECK_INTEQUAL(text_input[3].detail, INPUT_TEXT_INPUT);
    CHECK_INTEQUAL(text_input[3].ch, 0x2014);
    CHECK_INTEQUAL(text_input[4].detail, INPUT_TEXT_INPUT);
    CHECK_INTEQUAL(text_input[4].ch, 0x00FF);
    CHECK_INTEQUAL(text_input[5].detail, INPUT_TEXT_DONE);

    return 1;
}

/*************************************************************************/
/****************************** Touch tests ******************************/
/*************************************************************************/

TEST(test_touch_present)
{
    CHECK_TRUE(input_touch_present());

    sys_test_input_enable_touch(0);
    input_update();
    CHECK_FALSE(input_touch_present());

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_num_touches)
{
    CHECK_INTEQUAL(input_touch_num_touches(), 0);

    sys_test_input_touch_down(0, 0, 0);
    input_update();
    CHECK_INTEQUAL(input_touch_num_touches(), 1);

    sys_test_input_touch_down(1, 0.625, 0.625);
    input_update();
    CHECK_INTEQUAL(input_touch_num_touches(), 2);

    sys_test_input_touch_up(0);
    input_update();
    CHECK_INTEQUAL(input_touch_num_touches(), 1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_touch_id)
{
    CHECK_INTEQUAL(input_touch_id_for_index(-1), 0);
    CHECK_INTEQUAL(input_touch_id_for_index(0), 0);

    sys_test_input_touch_down(0, 0, 0);
    input_update();
    CHECK_INTEQUAL(input_touch_id_for_index(0), 1);

    sys_test_input_touch_down(1, 0.625, 0.625);
    input_update();
    CHECK_INTEQUAL(input_touch_id_for_index(0), 1);
    CHECK_INTEQUAL(input_touch_id_for_index(1), 2);

    sys_test_input_touch_up(0);
    input_update();
    CHECK_INTEQUAL(input_touch_id_for_index(0), 2);
    CHECK_INTEQUAL(input_touch_id_for_index(1), 0);

    const unsigned int last_id = 0U - 1U;
    sys_test_input_set_touch_id(last_id);
    sys_test_input_touch_down(2, 0, 0);
    sys_test_input_touch_down(3, 1, 1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_touch_active)
{
    CHECK_FALSE(input_touch_active(0));

    sys_test_input_touch_down(0, 0, 0);
    input_update();
    CHECK_TRUE(input_touch_active(1));
    CHECK_FALSE(input_touch_active(2));

    sys_test_input_touch_down(1, 0.625, 0.625);
    input_update();
    CHECK_INTEQUAL(input_touch_num_touches(), 2);
    CHECK_TRUE(input_touch_active(1));
    CHECK_TRUE(input_touch_active(2));

    sys_test_input_touch_up(0);
    input_update();
    CHECK_FALSE(input_touch_active(1));
    CHECK_TRUE(input_touch_active(2));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_touch_get_position)
{
    float x, y;

    x = y = -1;
    input_touch_get_position(0, &x, &y);
    CHECK_FLOATEQUAL(x, 0.5);
    CHECK_FLOATEQUAL(y, 0.5);
    x = y = -1;
    input_touch_get_initial_position(0, &x, &y);
    CHECK_FLOATEQUAL(x, 0.5);
    CHECK_FLOATEQUAL(y, 0.5);

    sys_test_input_touch_down(0, 0, 0);
    input_update();
    x = y = -1;
    input_touch_get_position(1, &x, &y);
    CHECK_FLOATEQUAL(x, 0);
    CHECK_FLOATEQUAL(y, 0);
    x = y = -1;
    input_touch_get_initial_position(1, &x, &y);
    CHECK_FLOATEQUAL(x, 0);
    CHECK_FLOATEQUAL(y, 0);

    sys_test_input_touch_down(1, 0.625, 0.625);
    input_update();
    x = y = -1;
    input_touch_get_position(1, &x, &y);
    CHECK_FLOATEQUAL(x, 0);
    CHECK_FLOATEQUAL(y, 0);
    x = y = -1;
    input_touch_get_initial_position(1, &x, &y);
    CHECK_FLOATEQUAL(x, 0);
    CHECK_FLOATEQUAL(y, 0);
    x = y = -1;
    input_touch_get_position(2, &x, &y);
    CHECK_FLOATEQUAL(x, 0.625);
    CHECK_FLOATEQUAL(y, 0.625);
    x = y = -1;
    input_touch_get_initial_position(2, &x, &y);
    CHECK_FLOATEQUAL(x, 0.625);
    CHECK_FLOATEQUAL(y, 0.625);

    sys_test_input_touch_move(0, 0.25, 0.25);
    input_update();
    x = y = -1;
    input_touch_get_position(1, &x, &y);
    CHECK_FLOATEQUAL(x, 0.25);
    CHECK_FLOATEQUAL(y, 0.25);
    x = y = -1;
    input_touch_get_initial_position(1, &x, &y);
    CHECK_FLOATEQUAL(x, 0);
    CHECK_FLOATEQUAL(y, 0);
    x = y = -1;
    input_touch_get_position(2, &x, &y);
    CHECK_FLOATEQUAL(x, 0.625);
    CHECK_FLOATEQUAL(y, 0.625);
    x = y = -1;
    input_touch_get_initial_position(2, &x, &y);
    CHECK_FLOATEQUAL(x, 0.625);
    CHECK_FLOATEQUAL(y, 0.625);

    sys_test_input_touch_move_to(1, 0.375, 0.375);
    input_update();
    x = y = -1;
    input_touch_get_position(1, &x, &y);
    CHECK_FLOATEQUAL(x, 0.25);
    CHECK_FLOATEQUAL(y, 0.25);
    x = y = -1;
    input_touch_get_initial_position(1, &x, &y);
    CHECK_FLOATEQUAL(x, 0);
    CHECK_FLOATEQUAL(y, 0);
    x = y = -1;
    input_touch_get_position(2, &x, &y);
    CHECK_FLOATEQUAL(x, 0.375);
    CHECK_FLOATEQUAL(y, 0.375);
    x = y = -1;
    input_touch_get_initial_position(2, &x, &y);
    CHECK_FLOATEQUAL(x, 0.625);
    CHECK_FLOATEQUAL(y, 0.625);

    sys_test_input_touch_up(0);
    input_update();
    x = y = -1;
    input_touch_get_position(1, &x, &y);
    CHECK_FLOATEQUAL(x, 0.5);
    CHECK_FLOATEQUAL(y, 0.5);
    x = y = -1;
    input_touch_get_initial_position(1, &x, &y);
    CHECK_FLOATEQUAL(x, 0.5);
    CHECK_FLOATEQUAL(y, 0.5);
    x = y = -1;
    input_touch_get_position(2, &x, &y);
    CHECK_FLOATEQUAL(x, 0.375);
    CHECK_FLOATEQUAL(y, 0.375);
    x = y = -1;
    input_touch_get_initial_position(2, &x, &y);
    CHECK_FLOATEQUAL(x, 0.625);
    CHECK_FLOATEQUAL(y, 0.625);

    sys_test_input_touch_move(1, 0.375, 0.375);
    input_update();
    x = y = -1;
    input_touch_get_position(1, &x, &y);
    CHECK_FLOATEQUAL(x, 0.5);
    CHECK_FLOATEQUAL(y, 0.5);
    x = y = -1;
    input_touch_get_initial_position(1, &x, &y);
    CHECK_FLOATEQUAL(x, 0.5);
    CHECK_FLOATEQUAL(y, 0.5);
    x = y = -1;
    input_touch_get_position(2, &x, &y);
    CHECK_FLOATEQUAL(x, 0.75);
    CHECK_FLOATEQUAL(y, 0.75);
    x = y = -1;
    input_touch_get_initial_position(2, &x, &y);
    CHECK_FLOATEQUAL(x, 0.625);
    CHECK_FLOATEQUAL(y, 0.625);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_touch_get_position_null)
{
    float x, y;

    sys_test_input_touch_down(0, 0.25, 0.25);
    input_update();

    x = y = -1;
    input_touch_get_position(1, &x, NULL);
    CHECK_FLOATEQUAL(x, 0.25);
    input_touch_get_position(1, NULL, &y);
    CHECK_FLOATEQUAL(y, 0.25);
    x = y = -1;
    input_touch_get_initial_position(1, &x, NULL);
    CHECK_FLOATEQUAL(x, 0.25);
    input_touch_get_initial_position(1, NULL, &y);
    CHECK_FLOATEQUAL(y, 0.25);

    x = y = -1;
    input_touch_get_position(0, &x, NULL);
    CHECK_FLOATEQUAL(x, 0.5);
    input_touch_get_position(0, NULL, &y);
    CHECK_FLOATEQUAL(y, 0.5);
    x = y = -1;
    input_touch_get_initial_position(0, &x, NULL);
    CHECK_FLOATEQUAL(x, 0.5);
    input_touch_get_initial_position(0, NULL, &y);
    CHECK_FLOATEQUAL(y, 0.5);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_touch_position_bounds)
{
    float x, y;

    sys_test_input_touch_down(0, 0.25, 0.25);
    input_update();
    x = y = -1;
    input_touch_get_position(1, &x, &y);
    CHECK_FLOATEQUAL(x, 0.25);
    CHECK_FLOATEQUAL(y, 0.25);
    x = y = -1;
    input_touch_get_initial_position(1, &x, &y);
    CHECK_FLOATEQUAL(x, 0.25);
    CHECK_FLOATEQUAL(y, 0.25);

    sys_test_input_touch_move_to(0, 1, 1);
    input_update();
    x = y = -1;
    input_touch_get_position(1, &x, &y);
    CHECK_FLOATEQUAL(x, 65535.0f/65536.0f);
    CHECK_FLOATEQUAL(y, 65535.0f/65536.0f);
    x = y = -1;
    input_touch_get_initial_position(1, &x, &y);
    CHECK_FLOATEQUAL(x, 0.25);
    CHECK_FLOATEQUAL(y, 0.25);

    sys_test_input_touch_move(0, -2, -2);
    input_update();
    x = y = -1;
    input_touch_get_position(1, &x, &y);
    CHECK_FLOATEQUAL(x, 0);
    CHECK_FLOATEQUAL(y, 0);
    x = y = -1;
    input_touch_get_initial_position(1, &x, &y);
    CHECK_FLOATEQUAL(x, 0.25);
    CHECK_FLOATEQUAL(y, 0.25);

    sys_test_input_touch_move(0, 1.5, 1.5);
    input_update();
    x = y = -1;
    input_touch_get_position(1, &x, &y);
    CHECK_FLOATEQUAL(x, 65535.0f/65536.0f);
    CHECK_FLOATEQUAL(y, 65535.0f/65536.0f);
    x = y = -1;
    input_touch_get_initial_position(1, &x, &y);
    CHECK_FLOATEQUAL(x, 0.25);
    CHECK_FLOATEQUAL(y, 0.25);

    sys_test_input_touch_down(1, 1, 1);
    input_update();
    x = y = -1;
    input_touch_get_position(2, &x, &y);
    CHECK_FLOATEQUAL(x, 65535.0f/65536.0f);
    CHECK_FLOATEQUAL(y, 65535.0f/65536.0f);
    x = y = -1;
    input_touch_get_initial_position(2, &x, &y);
    CHECK_FLOATEQUAL(x, 65535.0f/65536.0f);
    CHECK_FLOATEQUAL(y, 65535.0f/65536.0f);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_touch_array_overflow)
{
    for (int i = 0; i < INPUT_MAX_TOUCHES + 1; i++) {
        sys_test_input_touch_down(i, 0, 0);
    }
    input_update();
    const int good_id = INPUT_MAX_TOUCHES;
    const int missing_id = INPUT_MAX_TOUCHES + 1;
    CHECK_INTEQUAL(input_touch_num_touches(), INPUT_MAX_TOUCHES);
    CHECK_TRUE(input_touch_active(good_id));
    CHECK_FALSE(input_touch_active(missing_id));

    for (int i = 0; i < INPUT_MAX_TOUCHES + 1; i++) {
        sys_test_input_touch_move_to(i, 0.25, 0.25);
    }
    input_update();
    float x, y;
    x = y = -1;
    input_touch_get_position(good_id, &x, &y);
    CHECK_FLOATEQUAL(x, 0.25);
    CHECK_FLOATEQUAL(y, 0.25);
    x = y = -1;
    input_touch_get_position(missing_id, &x, &y);
    CHECK_FLOATEQUAL(x, 0.5);
    CHECK_FLOATEQUAL(y, 0.5);

    sys_test_input_touch_up(INPUT_MAX_TOUCHES - 1);
    input_update();
    CHECK_INTEQUAL(input_touch_num_touches(), INPUT_MAX_TOUCHES - 1);
    CHECK_FALSE(input_touch_active(good_id));
    CHECK_FALSE(input_touch_active(missing_id));

    sys_test_input_touch_up(INPUT_MAX_TOUCHES);
    input_update();
    CHECK_INTEQUAL(input_touch_num_touches(), INPUT_MAX_TOUCHES - 1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_touch_event_down)
{
    input_set_event_callback(receive_event);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    sys_test_input_touch_down(0, 0.25, 0.25);
    input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_TOUCH);
    CHECK_INTEQUAL(events[0].detail, INPUT_TOUCH_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[0].touch.id, 1);
    CHECK_FLOATEQUAL(events[0].touch.x, 0.25);
    CHECK_FLOATEQUAL(events[0].touch.y, 0.25);
    CHECK_FLOATEQUAL(events[0].touch.initial_x, 0.25);
    CHECK_FLOATEQUAL(events[0].touch.initial_y, 0.25);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_touch_event_move)
{
    input_set_event_callback(receive_event);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    sys_test_input_touch_down(0, 0.25, 0.25);
    sys_test_input_touch_move(0, 0.125, 0.5);
    input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_TOUCH);
    CHECK_INTEQUAL(events[1].detail, INPUT_TOUCH_MOVE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[1].touch.id, 1);
    CHECK_FLOATEQUAL(events[1].touch.x, 0.375);
    CHECK_FLOATEQUAL(events[1].touch.y, 0.75);
    CHECK_FLOATEQUAL(events[1].touch.initial_x, 0.25);
    CHECK_FLOATEQUAL(events[1].touch.initial_y, 0.25);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_touch_event_up)
{
    input_set_event_callback(receive_event);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    sys_test_input_touch_down(0, 0.25, 0.25);
    sys_test_input_touch_up(0);
    input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_TOUCH);
    CHECK_INTEQUAL(events[1].detail, INPUT_TOUCH_UP);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[1].touch.id, 1);
    CHECK_FLOATEQUAL(events[1].touch.x, 0.25);
    CHECK_FLOATEQUAL(events[1].touch.y, 0.25);
    CHECK_FLOATEQUAL(events[1].touch.initial_x, 0.25);
    CHECK_FLOATEQUAL(events[1].touch.initial_y, 0.25);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_touch_event_cancel)
{
    input_set_event_callback(receive_event);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    sys_test_input_touch_down(0, 0.25, 0.25);
    sys_test_input_touch_cancel(0);
    input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_TOUCH);
    CHECK_INTEQUAL(events[1].detail, INPUT_TOUCH_CANCEL);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[1].touch.id, 1);
    CHECK_FLOATEQUAL(events[1].touch.x, 0.25);
    CHECK_FLOATEQUAL(events[1].touch.y, 0.25);
    CHECK_FLOATEQUAL(events[1].touch.initial_x, 0.25);
    CHECK_FLOATEQUAL(events[1].touch.initial_y, 0.25);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_touch_event_array_overflow)
{
    input_set_event_callback(receive_event);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    for (int i = 0; i < INPUT_MAX_TOUCHES + 1; i++) {
        sys_test_input_touch_down(i, 0.25, 0.25);
    }
    input_update();

    CHECK_INTEQUAL(num_events, INPUT_MAX_TOUCHES);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_TOUCH);
    CHECK_INTEQUAL(events[0].detail, INPUT_TOUCH_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[0].touch.id, 1);
    CHECK_FLOATEQUAL(events[0].touch.x, 0.25);
    CHECK_FLOATEQUAL(events[0].touch.y, 0.25);
    CHECK_FLOATEQUAL(events[0].touch.initial_x, 0.25);
    CHECK_FLOATEQUAL(events[0].touch.initial_y, 0.25);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(2.0);
    sys_test_input_touch_move_to(INPUT_MAX_TOUCHES, 0.25, 0.25);
    input_update();
    /* The event should have been discarded by the input layer. */
    CHECK_INTEQUAL(num_events, 0);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(3.0);
    sys_test_input_touch_up(INPUT_MAX_TOUCHES);
    input_update();
    CHECK_INTEQUAL(num_events, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_touch_control_while_disabled)
{
    input_set_event_callback(receive_event);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_input_enable_touch(0);
    sys_test_input_touch_down(0, 0.25, 0.25);
    sys_test_input_touch_move_to(0, 0.75, 0.5);
    sys_test_input_touch_move(0, -0.375, 0.25);
    sys_test_input_touch_up(0);
    sys_test_input_touch_down(0, 1.0, 1.0);
    sys_test_input_touch_cancel(0);
    sys_test_input_touch_down(0, 0.125, 0.125);
    sys_test_input_enable_touch(1);
    input_update();
    CHECK_INTEQUAL(input_touch_num_touches(), 0);
    CHECK_INTEQUAL(input_touch_id_for_index(0), 0);
    float x, y;
    x = y = -1;
    input_touch_get_position(1, &x, &y);
    CHECK_FLOATEQUAL(x, 0.5);
    CHECK_FLOATEQUAL(y, 0.5);
    x = y = -1;
    input_touch_get_position(2, &x, &y);
    CHECK_FLOATEQUAL(x, 0.5);
    CHECK_FLOATEQUAL(y, 0.5);
    x = y = -1;
    input_touch_get_position(3, &x, &y);
    CHECK_FLOATEQUAL(x, 0.5);
    CHECK_FLOATEQUAL(y, 0.5);
    CHECK_INTEQUAL(num_events, 0);

    return 1;
}

/*************************************************************************/
/************************ Event coalescing tests *************************/
/*************************************************************************/

TEST(test_coalesce_joystick_stick_change)
{
    float x, y;

    sys_test_input_set_joy_connected(1);
    sys_test_input_set_joy_num_devices(2);
    input_update();
    input_set_event_callback(receive_event);
    input_enable_coalescing(1);

    mem_clear(events, sizeof(events));
    num_events = 0;
    x = y = 999;
    sys_test_time_set_seconds(1.0);
    sys_test_input_set_joy_stick(0, 1, +0.25, -0.75);
    sys_test_time_set_seconds(2.0);
    sys_test_input_set_joy_stick(0, 1, +0.5, -0.5);
    input_update();
    /* The two events should be coalesced. */
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index, 1);
    CHECK_FLOATEQUAL(events[0].joystick.x, +0.5);
    CHECK_FLOATEQUAL(events[0].joystick.y, -0.5);
    input_joystick_read_stick(0, 0, &x, &y);
    CHECK_FLOATEQUAL(x, 0);
    CHECK_FLOATEQUAL(y, 0);
    input_joystick_read_stick(0, 1, &x, &y);
    CHECK_FLOATEQUAL(x, +0.5);
    CHECK_FLOATEQUAL(y, -0.5);

    mem_clear(events, sizeof(events));
    num_events = 0;
    x = y = 999;
    sys_test_time_set_seconds(3.0);
    sys_test_input_set_joy_stick(0, 1, +0.25, -0.75);
    sys_test_time_set_seconds(4.0);
    sys_test_input_set_joy_stick(0, 0, +0.5, -0.5);
    input_update();
    /* The stick index is different, so the events should not be coalesced. */
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 3.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index, 1);
    CHECK_FLOATEQUAL(events[0].joystick.x, +0.25);
    CHECK_FLOATEQUAL(events[0].joystick.y, -0.75);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 4.0);
    CHECK_INTEQUAL(events[1].joystick.device, 0);
    CHECK_INTEQUAL(events[1].joystick.index, 0);
    CHECK_FLOATEQUAL(events[1].joystick.x, +0.5);
    CHECK_FLOATEQUAL(events[1].joystick.y, -0.5);
    input_joystick_read_stick(0, 0, &x, &y);
    CHECK_FLOATEQUAL(x, +0.5);
    CHECK_FLOATEQUAL(y, -0.5);
    input_joystick_read_stick(0, 1, &x, &y);
    CHECK_FLOATEQUAL(x, +0.25);
    CHECK_FLOATEQUAL(y, -0.75);

    mem_clear(events, sizeof(events));
    num_events = 0;
    x = y = 999;
    sys_test_time_set_seconds(5.0);
    sys_test_input_set_joy_stick(0, 1, +0.25, -0.75);
    sys_test_time_set_seconds(6.0);
    sys_test_input_set_joy_stick(1, 0, +0.75, -0.25);
    input_update();
    /* The device index is different, so the events should not be coalesced. */
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 5.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index, 1);
    CHECK_FLOATEQUAL(events[0].joystick.x, +0.25);
    CHECK_FLOATEQUAL(events[0].joystick.y, -0.75);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 6.0);
    CHECK_INTEQUAL(events[1].joystick.device, 1);
    CHECK_INTEQUAL(events[1].joystick.index, 0);
    CHECK_FLOATEQUAL(events[1].joystick.x, +0.75);
    CHECK_FLOATEQUAL(events[1].joystick.y, -0.25);
    input_joystick_read_stick(0, 0, &x, &y);
    CHECK_FLOATEQUAL(x, +0.5);
    CHECK_FLOATEQUAL(y, -0.5);
    input_joystick_read_stick(0, 1, &x, &y);
    CHECK_FLOATEQUAL(x, +0.25);
    CHECK_FLOATEQUAL(y, -0.75);
    input_joystick_read_stick(1, 0, &x, &y);
    CHECK_FLOATEQUAL(x, +0.75);
    CHECK_FLOATEQUAL(y, -0.25);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_coalesce_disabled_joystick_stick_change)
{
    float x, y;

    input_set_event_callback(receive_event);
    input_enable_coalescing(1);
    input_enable_coalescing(0);

    mem_clear(events, sizeof(events));
    num_events = 0;
    x = y = 999;
    sys_test_time_set_seconds(1.0);
    sys_test_input_set_joy_stick(0, 1, +0.25, -0.75);
    sys_test_time_set_seconds(2.0);
    sys_test_input_set_joy_stick(0, 1, +0.5, -0.5);
    input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index, 1);
    CHECK_FLOATEQUAL(events[0].joystick.x, +0.25);
    CHECK_FLOATEQUAL(events[0].joystick.y, -0.75);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 2.0);
    CHECK_INTEQUAL(events[1].joystick.device, 0);
    CHECK_INTEQUAL(events[1].joystick.index, 1);
    CHECK_FLOATEQUAL(events[1].joystick.x, +0.5);
    CHECK_FLOATEQUAL(events[1].joystick.y, -0.5);
    input_joystick_read_stick(0, 0, &x, &y);
    CHECK_FLOATEQUAL(x, 0);
    CHECK_FLOATEQUAL(y, 0);
    input_joystick_read_stick(0, 1, &x, &y);
    CHECK_FLOATEQUAL(x, +0.5);
    CHECK_FLOATEQUAL(y, -0.5);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_coalesce_joystick_dpad_change)
{
    int x, y;

    input_set_event_callback(receive_event);
    input_enable_coalescing(1);

    /* D-pad events should not be coalesced. */
    mem_clear(events, sizeof(events));
    num_events = 0;
    x = y = 999;
    sys_test_time_set_seconds(1.0);
    sys_test_input_set_joy_dpad(0, +1, -1);
    sys_test_time_set_seconds(2.0);
    sys_test_input_set_joy_dpad(0, -1, +1);
    input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_FLOATEQUAL(events[0].joystick.x, +1.0);
    CHECK_FLOATEQUAL(events[0].joystick.y, -1.0);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 2.0);
    CHECK_INTEQUAL(events[1].joystick.device, 0);
    CHECK_FLOATEQUAL(events[1].joystick.x, -1.0);
    CHECK_FLOATEQUAL(events[1].joystick.y, +1.0);
    input_joystick_read_dpad(0, &x, &y);
    CHECK_INTEQUAL(x, -1);
    CHECK_INTEQUAL(y, +1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_coalesce_mouse_move)
{
    float x, y;

    input_mouse_set_position(0.25, 0.5);
    input_update();
    input_set_event_callback(receive_event);
    input_enable_coalescing(1);

    mem_clear(events, sizeof(events));
    num_events = 0;
    x = y = 999;
    sys_test_time_set_seconds(1.0);
    sys_test_input_move_mouse(0.25, 0);
    sys_test_time_set_seconds(2.0);
    sys_test_input_move_mouse(0, 0.25);
    input_update();
    /* The two events should be coalesced. */
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.5);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.75);
    input_mouse_get_position(&x, &y);
    CHECK_FLOATEQUAL(x, 0.5);
    CHECK_FLOATEQUAL(y, 0.75);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_coalesce_disabled_mouse_move)
{
    float x, y;

    input_enable_coalescing(1);
    input_enable_coalescing(0);
    input_set_event_callback(receive_event);
    input_mouse_set_position(0.25, 0.5);
    input_update();

    mem_clear(events, sizeof(events));
    num_events = 0;
    x = y = 999;
    sys_test_time_set_seconds(1.0);
    sys_test_input_move_mouse(0.25, 0);
    sys_test_time_set_seconds(2.0);
    sys_test_input_move_mouse(0, 0.25);
    input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.5);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.5);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[1].detail, INPUT_MOUSE_MOVE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 2.0);
    CHECK_FLOATEQUAL(events[1].mouse.x, 0.5);
    CHECK_FLOATEQUAL(events[1].mouse.y, 0.75);
    input_mouse_get_position(&x, &y);
    CHECK_FLOATEQUAL(x, 0.5);
    CHECK_FLOATEQUAL(y, 0.75);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_coalesce_touch_move)
{
    float x, y;

    sys_test_input_touch_down(0, 0.25, 0.5);
    input_update();
    input_set_event_callback(receive_event);
    input_enable_coalescing(1);

    mem_clear(events, sizeof(events));
    num_events = 0;
    x = y = 999;
    sys_test_time_set_seconds(1.0);
    sys_test_input_touch_move(0, 0.25, 0);
    sys_test_time_set_seconds(2.0);
    sys_test_input_touch_move(0, 0, 0.25);
    input_update();
    /* The two events should be coalesced. */
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_TOUCH);
    CHECK_INTEQUAL(events[0].detail, INPUT_TOUCH_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].touch.id, 1);
    CHECK_FLOATEQUAL(events[0].touch.x, 0.5);
    CHECK_FLOATEQUAL(events[0].touch.y, 0.75);
    CHECK_FLOATEQUAL(events[0].touch.initial_x, 0.25);
    CHECK_FLOATEQUAL(events[0].touch.initial_y, 0.5);
    input_touch_get_position(1, &x, &y);
    CHECK_FLOATEQUAL(x, 0.5);
    CHECK_FLOATEQUAL(y, 0.75);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_coalesce_disabled_touch_move)
{
    float x, y;

    input_set_event_callback(receive_event);
    sys_test_input_touch_down(0, 0.25, 0.5);
    input_update();
    input_enable_coalescing(1);
    input_enable_coalescing(0);

    mem_clear(events, sizeof(events));
    num_events = 0;
    x = y = 999;
    sys_test_time_set_seconds(1.0);
    sys_test_input_touch_move(0, 0.25, 0);
    sys_test_time_set_seconds(2.0);
    sys_test_input_touch_move(0, 0, 0.25);
    input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_TOUCH);
    CHECK_INTEQUAL(events[0].detail, INPUT_TOUCH_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].touch.id, 1);
    CHECK_FLOATEQUAL(events[0].touch.x, 0.5);
    CHECK_FLOATEQUAL(events[0].touch.y, 0.5);
    CHECK_FLOATEQUAL(events[0].touch.initial_x, 0.25);
    CHECK_FLOATEQUAL(events[0].touch.initial_y, 0.5);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_TOUCH);
    CHECK_INTEQUAL(events[1].detail, INPUT_TOUCH_MOVE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 2.0);
    CHECK_INTEQUAL(events[1].touch.id, 1);
    CHECK_FLOATEQUAL(events[1].touch.x, 0.5);
    CHECK_FLOATEQUAL(events[1].touch.y, 0.75);
    CHECK_FLOATEQUAL(events[1].touch.initial_x, 0.25);
    CHECK_FLOATEQUAL(events[1].touch.initial_y, 0.5);
    input_touch_get_position(1, &x, &y);
    CHECK_FLOATEQUAL(x, 0.5);
    CHECK_FLOATEQUAL(y, 0.75);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_coalesce_different_types)
{
    float x, y;

    input_set_event_callback(receive_event);
    input_mouse_set_position(0.25, 0.5);
    sys_test_input_touch_down(0, 0.25, 0.5);
    input_update();
    input_enable_coalescing(1);

    mem_clear(events, sizeof(events));
    num_events = 0;
    x = y = 999;
    sys_test_time_set_seconds(1.0);
    sys_test_input_move_mouse(0.25, 0);
    sys_test_time_set_seconds(2.0);
    sys_test_input_touch_move(0, 0, 0.25);
    input_update();
    /* The events should not be coalesced because they are of different
     * types. */
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.5);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.5);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_TOUCH);
    CHECK_INTEQUAL(events[1].detail, INPUT_TOUCH_MOVE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 2.0);
    CHECK_INTEQUAL(events[1].touch.id, 1);
    CHECK_FLOATEQUAL(events[1].touch.x, 0.25);
    CHECK_FLOATEQUAL(events[1].touch.y, 0.75);
    CHECK_FLOATEQUAL(events[1].touch.initial_x, 0.25);
    CHECK_FLOATEQUAL(events[1].touch.initial_y, 0.5);
    input_mouse_get_position(&x, &y);
    CHECK_FLOATEQUAL(x, 0.5);
    CHECK_FLOATEQUAL(y, 0.5);
    input_touch_get_position(1, &x, &y);
    CHECK_FLOATEQUAL(x, 0.25);
    CHECK_FLOATEQUAL(y, 0.75);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_coalesce_different_touch_ids)
{
    float x, y;

    input_set_event_callback(receive_event);
    sys_test_input_touch_down(0, 0.25, 0.5);
    sys_test_input_touch_down(1, 0.5, 0.75);
    input_update();
    input_enable_coalescing(1);

    mem_clear(events, sizeof(events));
    num_events = 0;
    x = y = 999;
    sys_test_time_set_seconds(1.0);
    sys_test_input_touch_move(0, 0, 0.25);
    sys_test_time_set_seconds(2.0);
    sys_test_input_touch_move(1, 0, -0.25);
    input_update();
    /* The events should not be coalesced because they have different
     * touch IDs. */
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_TOUCH);
    CHECK_INTEQUAL(events[0].detail, INPUT_TOUCH_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].touch.id, 1);
    CHECK_FLOATEQUAL(events[0].touch.x, 0.25);
    CHECK_FLOATEQUAL(events[0].touch.y, 0.75);
    CHECK_FLOATEQUAL(events[0].touch.initial_x, 0.25);
    CHECK_FLOATEQUAL(events[0].touch.initial_y, 0.5);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_TOUCH);
    CHECK_INTEQUAL(events[1].detail, INPUT_TOUCH_MOVE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 2.0);
    CHECK_INTEQUAL(events[1].touch.id, 2);
    CHECK_FLOATEQUAL(events[1].touch.x, 0.5);
    CHECK_FLOATEQUAL(events[1].touch.y, 0.5);
    CHECK_FLOATEQUAL(events[1].touch.initial_x, 0.5);
    CHECK_FLOATEQUAL(events[1].touch.initial_y, 0.75);
    input_touch_get_position(1, &x, &y);
    CHECK_FLOATEQUAL(x, 0.25);
    CHECK_FLOATEQUAL(y, 0.75);
    input_touch_get_position(2, &x, &y);
    CHECK_FLOATEQUAL(x, 0.5);
    CHECK_FLOATEQUAL(y, 0.5);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_coalesce_init_failure)
{
#ifdef SIL_PLATFORM_PSP
    SKIP("Mutex allocation cannot fail on this system.");
#endif

    float x, y;

    input_mouse_set_position(0.25, 0.5);
    input_update();
    input_set_event_callback(receive_event);
    TEST_mem_fail_after(0, 1, 0);
    /* This will fail due to mutex creation failure. */
    input_enable_coalescing(1);
    TEST_mem_fail_after(-1, 0, 0);

    mem_clear(events, sizeof(events));
    num_events = 0;
    x = y = 999;
    sys_test_time_set_seconds(1.0);
    sys_test_input_move_mouse(0.25, 0);
    sys_test_time_set_seconds(2.0);
    sys_test_input_move_mouse(0, 0.25);
    input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.5);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.5);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[1].detail, INPUT_MOUSE_MOVE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 2.0);
    CHECK_FLOATEQUAL(events[1].mouse.x, 0.5);
    CHECK_FLOATEQUAL(events[1].mouse.y, 0.75);
    input_mouse_get_position(&x, &y);
    CHECK_FLOATEQUAL(x, 0.5);
    CHECK_FLOATEQUAL(y, 0.75);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_coalesce_repeat_enable)
{
    float x, y;

    input_mouse_set_position(0.25, 0.5);
    input_update();
    input_set_event_callback(receive_event);
    input_enable_coalescing(1);
    /* This second call should not cause the mutex created by the first
     * call to be leaked. */
    input_enable_coalescing(1);

    mem_clear(events, sizeof(events));
    num_events = 0;
    x = y = 999;
    sys_test_time_set_seconds(1.0);
    sys_test_input_move_mouse(0.25, 0);
    sys_test_time_set_seconds(2.0);
    sys_test_input_move_mouse(0, 0.25);
    input_update();
    /* The two events should be coalesced. */
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.5);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.75);
    input_mouse_get_position(&x, &y);
    CHECK_FLOATEQUAL(x, 0.5);
    CHECK_FLOATEQUAL(y, 0.75);

    return 1;
}

/*************************************************************************/
/************************** Miscellaneous tests **************************/
/*************************************************************************/

TEST(test_update_before_init)
{
    float x, y;

    input_mouse_set_position(0.25, 0.75);
    input_update();
    x = y = -1;
    input_mouse_get_position(&x, &y);
    CHECK_FLOATEQUAL(x, 0.25);
    CHECK_FLOATEQUAL(y, 0.75);

    input_cleanup();

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_input_move_mouse(+0.125, -0.125);
    input_update();  // This should do nothing.
    x = y = -1;
    input_mouse_get_position(&x, &y);
    CHECK_FLOATEQUAL(x, 0.25);
    CHECK_FLOATEQUAL(y, 0.75);
    CHECK_INTEQUAL(num_events, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_clear_on_init)
{
    float x, y;
    int xi, yi;

    sys_test_input_set_joy_button(0, 0, 1);
    sys_test_input_set_joy_stick(0, 0, -1, +1);
    sys_test_input_set_joy_dpad(0, +1, -1);
    sys_test_input_press_key(KEY_A, 1);
    input_mouse_set_position(0.25, 0.75);
    sys_test_input_press_mouse_buttons(1, 1, 1);
    sys_test_input_add_mouse_scroll(3, -2);
    input_text_enable();
    sys_test_input_touch_down(0, 0.75, 0.25);
    input_update();
    CHECK_TRUE(input_joystick_button_state(0, 0));
    x = y = 3;
    input_joystick_read_stick(0, 0, &x, &y);
    CHECK_FLOATEQUAL(x, -1);
    CHECK_FLOATEQUAL(y, +1);
    xi = yi = 3;
    input_joystick_read_dpad(0, &xi, &yi);
    CHECK_INTEQUAL(xi, +1);
    CHECK_INTEQUAL(yi, -1);
    CHECK_TRUE(input_key_state(KEY_A));
    CHECK_INTEQUAL(input_key_last_pressed(), KEY_A);
    x = y = -1;
    input_mouse_get_position(&x, &y);
    CHECK_FLOATEQUAL(x, 0.25);
    CHECK_FLOATEQUAL(y, 0.75);
    CHECK_TRUE(input_mouse_left_button_state());
    CHECK_TRUE(input_mouse_middle_button_state());
    CHECK_TRUE(input_mouse_right_button_state());
    CHECK_INTEQUAL(input_mouse_horiz_scroll(), 3);
    CHECK_INTEQUAL(input_mouse_vert_scroll(), -2);
    CHECK_TRUE(sys_test_input_get_text_state());
    CHECK_INTEQUAL(input_touch_num_touches(), 1);
    CHECK_INTEQUAL(input_touch_id_for_index(0), 1);
    CHECK_TRUE(input_touch_active(1));
    x = y = -1;
    input_touch_get_position(1, &x, &y);
    CHECK_FLOATEQUAL(x, 0.75);
    CHECK_FLOATEQUAL(y, 0.25);
    x = y = -1;
    input_touch_get_initial_position(1, &x, &y);
    CHECK_FLOATEQUAL(x, 0.75);
    CHECK_FLOATEQUAL(y, 0.25);

    input_cleanup();
    input_init();

    CHECK_FALSE(input_joystick_button_state(0, 0));
    x = y = 3;
    input_joystick_read_stick(0, 0, &x, &y);
    CHECK_FLOATEQUAL(x, 0);
    CHECK_FLOATEQUAL(y, 0);
    xi = yi = 3;
    input_joystick_read_dpad(0, &xi, &yi);
    CHECK_INTEQUAL(xi, 0);
    CHECK_INTEQUAL(yi, 0);
    CHECK_FALSE(input_key_state(KEY_A));
    CHECK_INTEQUAL(input_key_last_pressed(), 0);
    x = y = -1;
    input_mouse_get_position(&x, &y);
    CHECK_FLOATEQUAL(x, 0.5);
    CHECK_FLOATEQUAL(y, 0.5);
    CHECK_FALSE(input_mouse_left_button_state());
    CHECK_FALSE(input_mouse_middle_button_state());
    CHECK_FALSE(input_mouse_right_button_state());
    CHECK_INTEQUAL(input_mouse_horiz_scroll(), 0);
    CHECK_INTEQUAL(input_mouse_vert_scroll(), 0);
    CHECK_INTEQUAL(input_touch_num_touches(), 0);
    CHECK_INTEQUAL(input_touch_id_for_index(0), 0);
    CHECK_FALSE(input_touch_active(1));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_ring_buffer_overflow)
{
    sys_test_input_press_key(KEY_A, 1);
    for (int i = 1; i < INPUT_RING_BUFFER_SIZE; i++) {
        if (i%2 == 1) {
            sys_test_input_press_key(KEY_B, 2);
        } else {
            sys_test_input_release_key(KEY_B, 2);
        }
    }
    sys_test_input_press_key(KEY_C, 3);  // Will be dropped due to full buffer.
    input_update();
    CHECK_TRUE(input_key_state(KEY_A));
    CHECK_FALSE(input_key_state(KEY_C));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_ring_buffer_overflow_coalesce)
{
    float x, y;

    input_mouse_set_position(0.25, 0.5);
    sys_test_input_touch_down(0, 0.25, 0.5);
    input_update();
    input_enable_coalescing(1);

    sys_test_input_press_key(KEY_A, 1);
    for (int i = 1; i < INPUT_RING_BUFFER_SIZE; i++) {
        if (i%2 == 1) {
            sys_test_input_press_key(KEY_B, 2);
        } else {
            sys_test_input_release_key(KEY_B, 2);
        }
    }
    /* This should be dropped by the event receive callback. */
    sys_test_time_set_seconds(1.0);
    sys_test_input_move_mouse(0.25, 0);
    /* This should be dropped by input_update() when the coalesced event
     * is flushed. */
    sys_test_time_set_seconds(2.0);
    sys_test_input_touch_move(0, 0, 0.25);
    input_update();
    input_mouse_get_position(&x, &y);
    CHECK_FLOATEQUAL(x, 0.25);
    CHECK_FLOATEQUAL(y, 0.5);
    input_touch_get_position(1, &x, &y);
    CHECK_FLOATEQUAL(x, 0.25);
    CHECK_FLOATEQUAL(y, 0.5);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_invalid_event)
{
    input_set_event_callback(receive_event);

    sys_test_input_send_event(&(InputEvent){
        .type = 0x7FFFFFFF, .detail = 0x7FFFFFFE, .timestamp = -3.0});
    input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, 0x7FFFFFFF);
    CHECK_INTEQUAL(events[0].detail, 0x7FFFFFFE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, -3.0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_clear_event_callback)
{
    input_mouse_set_position(0.25, 0.5);
    input_update();
    input_set_event_callback(receive_event);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    sys_test_input_move_mouse(0.25, 0.25);
    input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.5);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.75);

    input_set_event_callback(NULL);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(2.0);
    input_mouse_set_position(0.75, 0.25);
    input_update();
    CHECK_INTEQUAL(num_events, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_reinit_clears_event_callback)
{
    input_mouse_set_position(0.25, 0.5);
    input_update();
    input_set_event_callback(receive_event);

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    sys_test_input_move_mouse(0.25, 0.25);
    input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.5);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.75);

    input_cleanup();
    ASSERT(input_init());

    mem_clear(events, sizeof(events));
    num_events = 0;
    sys_test_time_set_seconds(2.0);
    input_mouse_set_position(0.75, 0.25);
    input_update();
    CHECK_INTEQUAL(num_events, 0);

    return 1;
}

/*************************************************************************/
/*************************************************************************/
