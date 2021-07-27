/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/misc/joystick-hid.c: Tests for the generic HID joystick
 * interface.
 */

#include "src/base.h"
#include "src/input.h"
#include "src/sysdep/misc/joystick-hid.h"
#include "src/test/base.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Description of a simple joystick with a single stick and button. */
static HIDJoystickInfo basic_joystick_info = {
    .vendor_id = 0x0123,
    .product_id = 0x4567,
    .name = (char *)"Test Joystick",
    .serial = (char *)"123XYZ",
    .num_buttons = 1,
    .buttons = (uint32_t[]){HID_PAGE_BUTTON<<16 | 1},
    .num_values = 2,
    .values = (HIDJoystickValueInfo[]){
        {HID_PAGE_GENERIC_DESKTOP<<16 | HID_USAGE_X, -128, 127},
        {HID_PAGE_GENERIC_DESKTOP<<16 | HID_USAGE_Y, -128, 127},
    },
};

/* Description of a simple joystick with two sticks (X/Y and Z/RX) and no
 * buttons. */
static HIDJoystickInfo zrx_joystick_info = {
    .vendor_id = 0x0123,
    .product_id = 0x4567,
    .name = (char *)"Test Joystick",
    .serial = (char *)"123XYZ",
    .num_buttons = 0,
    .buttons = NULL,
    .num_values = 4,
    .values = (HIDJoystickValueInfo[]){
        {HID_PAGE_GENERIC_DESKTOP<<16 | HID_USAGE_X, -128, 127},
        {HID_PAGE_GENERIC_DESKTOP<<16 | HID_USAGE_Y, -128, 127},
        {HID_PAGE_GENERIC_DESKTOP<<16 | HID_USAGE_Z, -128, 127},
        {HID_PAGE_GENERIC_DESKTOP<<16 | HID_USAGE_RX, -128, 127},
    },
};

/* Description of a simple joystick with two sticks (X/Y and Z/RZ) and no
 * buttons. */
static HIDJoystickInfo zrz_joystick_info = {
    .vendor_id = 0x0123,
    .product_id = 0x4567,
    .name = (char *)"Test Joystick",
    .serial = (char *)"123XYZ",
    .num_buttons = 0,
    .buttons = NULL,
    .num_values = 4,
    .values = (HIDJoystickValueInfo[]){
        {HID_PAGE_GENERIC_DESKTOP<<16 | HID_USAGE_X, -128, 127},
        {HID_PAGE_GENERIC_DESKTOP<<16 | HID_USAGE_Y, -128, 127},
        {HID_PAGE_GENERIC_DESKTOP<<16 | HID_USAGE_Z, -128, 127},
        {HID_PAGE_GENERIC_DESKTOP<<16 | HID_USAGE_RZ, -128, 127},
    },
};

/* Description of a simple joystick with two sticks, a trigger (on RZ),
 * and no buttons. */
static HIDJoystickInfo tristick_joystick_info = {
    .vendor_id = 0x0123,
    .product_id = 0x4567,
    .name = (char *)"Test Joystick",
    .serial = (char *)"123XYZ",
    .num_buttons = 0,
    .buttons = NULL,
    .num_values = 5,
    .values = (HIDJoystickValueInfo[]){
        {HID_PAGE_GENERIC_DESKTOP<<16 | HID_USAGE_X, -128, 127},
        {HID_PAGE_GENERIC_DESKTOP<<16 | HID_USAGE_Y, -128, 127},
        {HID_PAGE_GENERIC_DESKTOP<<16 | HID_USAGE_RX, -128, 127},
        {HID_PAGE_GENERIC_DESKTOP<<16 | HID_USAGE_RY, -128, 127},
        {HID_PAGE_GENERIC_DESKTOP<<16 | HID_USAGE_RZ, 0, 255},
    },
};

/* Description of a simple joystick with no sticks, 4 buttons, and a
 * native D-pad. */
static HIDJoystickInfo dpad_joystick_info = {
    .vendor_id = 0x0123,
    .product_id = 0x4567,
    .name = (char *)"Test Joystick",
    .serial = (char *)"123XYZ",
    .num_buttons = 8,
    .buttons = (uint32_t[]){HID_PAGE_BUTTON<<16 | 1,
                            HID_PAGE_BUTTON<<16 | 2,
                            HID_PAGE_BUTTON<<16 | 3,
                            HID_PAGE_BUTTON<<16 | 4,
                            HID_PAGE_GENERIC_DESKTOP<<16 | HID_USAGE_DPAD_UP,
                            HID_PAGE_GENERIC_DESKTOP<<16 | HID_USAGE_DPAD_DOWN,
                            HID_PAGE_GENERIC_DESKTOP<<16 | HID_USAGE_DPAD_LEFT,
                            HID_PAGE_GENERIC_DESKTOP<<16 | HID_USAGE_DPAD_RIGHT},
    .num_values = 0,
    .values = NULL,
};

/* Description of a simple joystick with no sticks, 4 buttons, a native
 * D-pad, and a hat input. */
static HIDJoystickInfo dpad_plus_hat_joystick_info = {
    .vendor_id = 0x0123,
    .product_id = 0x4567,
    .name = (char *)"Test Joystick",
    .serial = (char *)"123XYZ",
    .num_buttons = 8,
    .buttons = (uint32_t[]){HID_PAGE_BUTTON<<16 | 1,
                            HID_PAGE_BUTTON<<16 | 2,
                            HID_PAGE_BUTTON<<16 | 3,
                            HID_PAGE_BUTTON<<16 | 4,
                            HID_PAGE_GENERIC_DESKTOP<<16 | HID_USAGE_DPAD_UP,
                            HID_PAGE_GENERIC_DESKTOP<<16 | HID_USAGE_DPAD_DOWN,
                            HID_PAGE_GENERIC_DESKTOP<<16 | HID_USAGE_DPAD_LEFT,
                            HID_PAGE_GENERIC_DESKTOP<<16 | HID_USAGE_DPAD_RIGHT},
    .num_values = 1,
    .values = (HIDJoystickValueInfo[]){
        {HID_PAGE_GENERIC_DESKTOP<<16 | HID_USAGE_HAT, 1, 8},
    },
};

/* Description of a simple joystick with no sticks, 4 buttons, and a
 * hat-based D-pad. */
static HIDJoystickInfo hat_joystick_info = {
    .vendor_id = 0x0123,
    .product_id = 0x4567,
    .name = (char *)"Test Joystick",
    .serial = (char *)"123XYZ",
    .num_buttons = 4,
    .buttons = (uint32_t[]){HID_PAGE_BUTTON<<16 | 1,
                            HID_PAGE_BUTTON<<16 | 2,
                            HID_PAGE_BUTTON<<16 | 3,
                            HID_PAGE_BUTTON<<16 | 4},
    .num_values = 1,
    .values = (HIDJoystickValueInfo[]){
        {HID_PAGE_GENERIC_DESKTOP<<16 | HID_USAGE_HAT, 1, 8},
    },
};

/* Description of a PlayStation 3 controller. */
static HIDJoystickInfo ps3_joystick_info = {
    .vendor_id = 0x054C,
    .product_id = 0x0268,
    .name = (char *)"PLAYSTATION(R)3 Controller",
    .serial = NULL,
    .num_buttons = 19,
    .buttons = (uint32_t[]){HID_PAGE_BUTTON<<16 | 1,
                            HID_PAGE_BUTTON<<16 | 2,
                            HID_PAGE_BUTTON<<16 | 3,
                            HID_PAGE_BUTTON<<16 | 4,
                            HID_PAGE_BUTTON<<16 | 5,
                            HID_PAGE_BUTTON<<16 | 6,
                            HID_PAGE_BUTTON<<16 | 7,
                            HID_PAGE_BUTTON<<16 | 8,
                            HID_PAGE_BUTTON<<16 | 9,
                            HID_PAGE_BUTTON<<16 | 10,
                            HID_PAGE_BUTTON<<16 | 11,
                            HID_PAGE_BUTTON<<16 | 12,
                            HID_PAGE_BUTTON<<16 | 13,
                            HID_PAGE_BUTTON<<16 | 14,
                            HID_PAGE_BUTTON<<16 | 15,
                            HID_PAGE_BUTTON<<16 | 16,
                            HID_PAGE_BUTTON<<16 | 17,
                            HID_PAGE_BUTTON<<16 | 18,
                            HID_PAGE_BUTTON<<16 | 19},
    .num_values = 4,
    .values = (HIDJoystickValueInfo[]){
        {HID_PAGE_GENERIC_DESKTOP<<16 | HID_USAGE_X,  0, 255},
        {HID_PAGE_GENERIC_DESKTOP<<16 | HID_USAGE_Y,  0, 255},
        {HID_PAGE_GENERIC_DESKTOP<<16 | HID_USAGE_Z,  0, 255},
        {HID_PAGE_GENERIC_DESKTOP<<16 | HID_USAGE_RZ, 0, 255},
    },
};

/* Description of an Xbox 360 controller. */
static HIDJoystickInfo x360_joystick_info = {
    .vendor_id = 0x045E,
    .product_id = 0x028E,
    .name = (char *)"Microsoft X-Box 360 pad",
    .serial = NULL,
    .num_buttons = 11,
    .buttons = (uint32_t[]){HID_PAGE_BUTTON<<16 | 1,
                            HID_PAGE_BUTTON<<16 | 2,
                            HID_PAGE_BUTTON<<16 | 3,
                            HID_PAGE_BUTTON<<16 | 4,
                            HID_PAGE_BUTTON<<16 | 5,
                            HID_PAGE_BUTTON<<16 | 6,
                            HID_PAGE_BUTTON<<16 | 7,
                            HID_PAGE_BUTTON<<16 | 8,
                            HID_PAGE_BUTTON<<16 | 9,
                            HID_PAGE_BUTTON<<16 | 10,
                            HID_PAGE_BUTTON<<16 | 11},
    .num_values = 7,
    .values = (HIDJoystickValueInfo[]){
        {HID_PAGE_GENERIC_DESKTOP<<16 | HID_USAGE_X,  -128, 127},
        {HID_PAGE_GENERIC_DESKTOP<<16 | HID_USAGE_Y,  -128, 127},
        {HID_PAGE_GENERIC_DESKTOP<<16 | HID_USAGE_Z,     0, 255},
        {HID_PAGE_GENERIC_DESKTOP<<16 | HID_USAGE_RX, -128, 127},
        {HID_PAGE_GENERIC_DESKTOP<<16 | HID_USAGE_RY, -128, 127},
        {HID_PAGE_GENERIC_DESKTOP<<16 | HID_USAGE_RZ,    0, 255},
        {HID_PAGE_GENERIC_DESKTOP<<16 | HID_USAGE_HAT,   0,   7},
    },
};

/*-----------------------------------------------------------------------*/

/* Events received through the event callback. */
static InputEvent events[10];
static int num_events;

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

/**
 * event_callback:  Event callback for generic HID joysticks.  Records
 * received events in the events[] buffer.
 */
static void event_callback(const InputEvent *event)
{
    ASSERT(num_events < lenof(events));
    events[num_events++] = *event;
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_misc_joystick_hid)

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_create)
{
    HIDJoystickHandle *handle;
    CHECK_TRUE(handle = hidjoy_create(&basic_joystick_info));

    hidjoy_destroy(handle);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_memory_failure)
{
    HIDJoystickHandle *handle;
    CHECK_MEMORY_FAILURES(handle = hidjoy_create(&basic_joystick_info));

    hidjoy_destroy(handle);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_null_name_serial)
{
    HIDJoystickInfo info = basic_joystick_info;
    info.name = NULL;
    info.serial = NULL;

    HIDJoystickHandle *handle;
    CHECK_TRUE(handle = hidjoy_create(&info));

    hidjoy_destroy(handle);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_destroy_null)
{
    hidjoy_destroy(NULL);  // Should not crash.
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_vendor_id)
{
    HIDJoystickInfo info = basic_joystick_info;
    HIDJoystickHandle *handle;
    CHECK_TRUE(handle = hidjoy_create(&info));

    /* Changing the value in the info struct should not affect the value
     * returned by the function. */
    info.vendor_id = ~info.vendor_id;
    CHECK_INTEQUAL(hidjoy_vendor_id(handle), basic_joystick_info.vendor_id);

    hidjoy_destroy(handle);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_product_id)
{
    HIDJoystickInfo info = basic_joystick_info;
    HIDJoystickHandle *handle;
    CHECK_TRUE(handle = hidjoy_create(&info));

    info.product_id = ~info.product_id;
    CHECK_INTEQUAL(hidjoy_product_id(handle), basic_joystick_info.product_id);

    hidjoy_destroy(handle);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_name)
{
    HIDJoystickInfo info = basic_joystick_info;
    HIDJoystickHandle *handle;
    CHECK_TRUE(handle = hidjoy_create(&info));

    info.name = NULL;
    CHECK_STREQUAL(hidjoy_name(handle), basic_joystick_info.name);

    hidjoy_destroy(handle);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_serial)
{
    HIDJoystickInfo info = basic_joystick_info;
    HIDJoystickHandle *handle;
    CHECK_TRUE(handle = hidjoy_create(&info));

    info.serial = NULL;
    CHECK_STREQUAL(hidjoy_serial(handle), basic_joystick_info.serial);

    hidjoy_destroy(handle);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_num_buttons)
{
    HIDJoystickInfo info = basic_joystick_info;
    HIDJoystickHandle *handle;
    CHECK_TRUE(handle = hidjoy_create(&info));

    info.num_buttons = ~info.num_buttons;
    CHECK_INTEQUAL(hidjoy_num_buttons(handle), basic_joystick_info.num_buttons);

    hidjoy_destroy(handle);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_num_sticks)
{
    HIDJoystickHandle *handle;
    CHECK_TRUE(handle = hidjoy_create(&basic_joystick_info));

    CHECK_INTEQUAL(hidjoy_num_sticks(handle), 1);  // Derived, not copied.

    hidjoy_destroy(handle);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_button_mapping)
{
    HIDJoystickHandle *handle;
    CHECK_TRUE(handle = hidjoy_create(&ps3_joystick_info));

    CHECK_INTEQUAL(hidjoy_button_mapping(handle,
                                         INPUT_JOYBUTTON_HOME), 16);
    CHECK_INTEQUAL(hidjoy_button_mapping(handle,
                                         INPUT_JOYBUTTON_START), 3);
    CHECK_INTEQUAL(hidjoy_button_mapping(handle,
                                         INPUT_JOYBUTTON_SELECT), 0);
    CHECK_INTEQUAL(hidjoy_button_mapping(handle,
                                         INPUT_JOYBUTTON_FACE_UP), 12);
    CHECK_INTEQUAL(hidjoy_button_mapping(handle,
                                         INPUT_JOYBUTTON_FACE_LEFT), 15);
    CHECK_INTEQUAL(hidjoy_button_mapping(handle,
                                         INPUT_JOYBUTTON_FACE_RIGHT), 13);
    CHECK_INTEQUAL(hidjoy_button_mapping(handle,
                                         INPUT_JOYBUTTON_FACE_DOWN), 14);
    CHECK_INTEQUAL(hidjoy_button_mapping(handle,
                                         INPUT_JOYBUTTON_L1), 10);
    CHECK_INTEQUAL(hidjoy_button_mapping(handle,
                                         INPUT_JOYBUTTON_R1), 11);
    CHECK_INTEQUAL(hidjoy_button_mapping(handle,
                                         INPUT_JOYBUTTON_L2), 8);
    CHECK_INTEQUAL(hidjoy_button_mapping(handle,
                                         INPUT_JOYBUTTON_R2), 9);
    CHECK_INTEQUAL(hidjoy_button_mapping(handle,
                                         INPUT_JOYBUTTON_L_STICK), 1);
    CHECK_INTEQUAL(hidjoy_button_mapping(handle,
                                         INPUT_JOYBUTTON_R_STICK), 2);

    hidjoy_destroy(handle);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_is_input_used)
{
    HIDJoystickHandle *handle;
    CHECK_TRUE(handle = hidjoy_create(&basic_joystick_info));

    CHECK_TRUE(hidjoy_is_input_used(handle, HID_PAGE_BUTTON, 1));
    CHECK_FALSE(hidjoy_is_input_used(handle, HID_PAGE_BUTTON, 2));

    CHECK_TRUE(hidjoy_is_input_used(handle, HID_PAGE_GENERIC_DESKTOP,
                                    HID_USAGE_X));
    CHECK_FALSE(hidjoy_is_input_used(handle, HID_PAGE_GENERIC_DESKTOP,
                                     HID_USAGE_RX));
    CHECK_FALSE(hidjoy_is_input_used(handle, HID_PAGE_GENERIC_DESKTOP,
                                     HID_USAGE_HAT));
    CHECK_FALSE(hidjoy_is_input_used(handle, HID_PAGE_GENERIC_DESKTOP,
                                     HID_USAGE_DPAD_UP));

    hidjoy_destroy(handle);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_input_button)
{
    HIDJoystickHandle *handle;
    CHECK_TRUE(handle = hidjoy_create(&basic_joystick_info));
    hidjoy_set_event_callback(handle, event_callback, 42);

    num_events = 0;
    hidjoy_handle_input(handle, 1.0, HID_PAGE_BUTTON, 1, 1);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_INTEQUAL(events[0].joystick.index, 0);

    /* Repeating the same value should generate a new button event (the
     * caller is responsible for filtering out duplicate events). */
    num_events = 0;
    hidjoy_handle_input(handle, 2.0, HID_PAGE_BUTTON, 1, 1);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_INTEQUAL(events[0].joystick.index, 0);

    num_events = 0;
    hidjoy_handle_input(handle, 3.0, HID_PAGE_BUTTON, 1, 0);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_UP);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 3.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_INTEQUAL(events[0].joystick.index, 0);

    hidjoy_destroy(handle);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_input_stick)
{
    HIDJoystickHandle *handle;
    CHECK_TRUE(handle = hidjoy_create(&basic_joystick_info));
    hidjoy_set_event_callback(handle, event_callback, 42);

    num_events = 0;
    hidjoy_handle_input(handle, 1.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_X, 127);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_INTEQUAL(events[0].joystick.index, 0);
    CHECK_FLOATEQUAL(events[0].joystick.x, 1);
    CHECK_FLOATEQUAL(events[0].joystick.y, 0);

    num_events = 0;
    hidjoy_handle_input(handle, 2.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_X, 127);
    hidjoy_flush_events(handle);
    /* Duplicate stick events _should_ be suppressed. */
    CHECK_INTEQUAL(num_events, 0);

    num_events = 0;
    hidjoy_handle_input(handle, 3.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_Y, -128);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 3.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_INTEQUAL(events[0].joystick.index, 0);
    CHECK_FLOATEQUAL(events[0].joystick.x, 1);
    CHECK_FLOATEQUAL(events[0].joystick.y, -1);

    hidjoy_destroy(handle);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_input_unused_button)
{
    HIDJoystickHandle *handle;
    CHECK_TRUE(handle = hidjoy_create(&basic_joystick_info));
    hidjoy_set_event_callback(handle, event_callback, 42);

    num_events = 0;
    hidjoy_handle_input(handle, 1.0, HID_PAGE_BUTTON, 2, 1);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 0);

    hidjoy_destroy(handle);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_input_unused_stick)
{
    HIDJoystickHandle *handle;
    CHECK_TRUE(handle = hidjoy_create(&basic_joystick_info));
    hidjoy_set_event_callback(handle, event_callback, 42);

    num_events = 0;
    hidjoy_handle_input(handle, 1.0, HID_PAGE_GENERIC_DESKTOP, HID_USAGE_Z, 1);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 0);

    hidjoy_destroy(handle);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_input_unused_hat)
{
    HIDJoystickHandle *handle;
    CHECK_TRUE(handle = hidjoy_create(&basic_joystick_info));
    hidjoy_set_event_callback(handle, event_callback, 42);

    num_events = 0;
    hidjoy_handle_input(handle, 1.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_HAT, 1);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 0);

    hidjoy_destroy(handle);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_input_unused_dpad)
{
    HIDJoystickHandle *handle;
    CHECK_TRUE(handle = hidjoy_create(&basic_joystick_info));
    hidjoy_set_event_callback(handle, event_callback, 42);

    num_events = 0;
    hidjoy_handle_input(handle, 1.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_DPAD_UP, 1);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 0);

    hidjoy_destroy(handle);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_input_stick_merge_inputs)
{
    HIDJoystickHandle *handle;
    CHECK_TRUE(handle = hidjoy_create(&basic_joystick_info));
    hidjoy_set_event_callback(handle, event_callback, 42);

    num_events = 0;
    hidjoy_handle_input(handle, 1.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_X, 127);
    CHECK_INTEQUAL(num_events, 0);  // Should be stored as a pending change.

    num_events = 0;
    hidjoy_handle_input(handle, 1.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_Y, -128);
    CHECK_INTEQUAL(num_events, 0);

    num_events = 0;
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_INTEQUAL(events[0].joystick.index, 0);
    CHECK_FLOATEQUAL(events[0].joystick.x, 1);
    CHECK_FLOATEQUAL(events[0].joystick.y, -1);

    hidjoy_destroy(handle);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_input_stick_merge_inputs_different_timestamp)
{
    HIDJoystickHandle *handle;
    CHECK_TRUE(handle = hidjoy_create(&basic_joystick_info));
    hidjoy_set_event_callback(handle, event_callback, 42);

    num_events = 0;
    hidjoy_handle_input(handle, 1.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_X, 127);
    CHECK_INTEQUAL(num_events, 0);  // Should be stored as a pending change.

    num_events = 0;
    hidjoy_handle_input(handle, 2.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_Y, -128);
    CHECK_INTEQUAL(num_events, 1);  // Different timestamp flushes the event.
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_INTEQUAL(events[0].joystick.index, 0);
    CHECK_FLOATEQUAL(events[0].joystick.x, 1);
    CHECK_FLOATEQUAL(events[0].joystick.y, 0);

    num_events = 0;
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_INTEQUAL(events[0].joystick.index, 0);
    CHECK_FLOATEQUAL(events[0].joystick.x, 1);
    CHECK_FLOATEQUAL(events[0].joystick.y, -1);

    hidjoy_destroy(handle);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_sorted_buttons)
{
    HIDJoystickInfo info = ps3_joystick_info;
    const uint32_t temp = info.buttons[0];
    info.buttons[0] = info.buttons[info.num_buttons-1];
    info.buttons[info.num_buttons-1] = temp;
    HIDJoystickHandle *handle;
    CHECK_TRUE(handle = hidjoy_create(&info));
    hidjoy_set_event_callback(handle, event_callback, 42);

    num_events = 0;
    hidjoy_handle_input(handle, 1.0, HID_PAGE_BUTTON, 1, 1);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_INTEQUAL(events[0].joystick.index, 0);

    hidjoy_destroy(handle);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_stick_invalid_range)
{
    HIDJoystickInfo info = basic_joystick_info;
    info.values[0].logical_min = info.values[0].logical_max;
    HIDJoystickHandle *handle;
    CHECK_TRUE(handle = hidjoy_create(&info));
    hidjoy_set_event_callback(handle, event_callback, 42);

    num_events = 0;
    hidjoy_handle_input(handle, 1.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_X, 127);  // Should be ignored.
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 0);

    num_events = 0;
    hidjoy_handle_input(handle, 2.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_Y, -128);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_INTEQUAL(events[0].joystick.index, 0);
    CHECK_FLOATEQUAL(events[0].joystick.x, 0);
    CHECK_FLOATEQUAL(events[0].joystick.y, -1);

    hidjoy_destroy(handle);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_two_sticks_zrx)
{
    HIDJoystickHandle *handle;
    CHECK_TRUE(handle = hidjoy_create(&zrx_joystick_info));
    hidjoy_set_event_callback(handle, event_callback, 42);

    CHECK_INTEQUAL(hidjoy_num_sticks(handle), 2);

    num_events = 0;
    hidjoy_handle_input(handle, 1.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_X, 127);
    hidjoy_handle_input(handle, 1.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_Y, -128);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_INTEQUAL(events[0].joystick.index, 0);
    CHECK_FLOATEQUAL(events[0].joystick.x, 1);
    CHECK_FLOATEQUAL(events[0].joystick.y, -1);

    num_events = 0;
    hidjoy_handle_input(handle, 1.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_Z, -128);
    hidjoy_handle_input(handle, 1.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_RX, 127);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_INTEQUAL(events[0].joystick.index, 1);
    CHECK_FLOATEQUAL(events[0].joystick.x, -1);
    CHECK_FLOATEQUAL(events[0].joystick.y, 1);

    hidjoy_destroy(handle);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_two_sticks_zrz)
{
    HIDJoystickHandle *handle;
    CHECK_TRUE(handle = hidjoy_create(&zrz_joystick_info));
    hidjoy_set_event_callback(handle, event_callback, 42);

    CHECK_INTEQUAL(hidjoy_num_sticks(handle), 2);

    num_events = 0;
    hidjoy_handle_input(handle, 1.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_X, 127);
    hidjoy_handle_input(handle, 1.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_Y, -128);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_INTEQUAL(events[0].joystick.index, 0);
    CHECK_FLOATEQUAL(events[0].joystick.x, 1);
    CHECK_FLOATEQUAL(events[0].joystick.y, -1);

    num_events = 0;
    hidjoy_handle_input(handle, 1.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_Z, -128);
    hidjoy_handle_input(handle, 1.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_RZ, 127);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_INTEQUAL(events[0].joystick.index, 1);
    CHECK_FLOATEQUAL(events[0].joystick.x, -1);
    CHECK_FLOATEQUAL(events[0].joystick.y, 1);

    hidjoy_destroy(handle);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_three_sticks)
{
    HIDJoystickHandle *handle;
    CHECK_TRUE(handle = hidjoy_create(&tristick_joystick_info));
    hidjoy_set_event_callback(handle, event_callback, 42);

    CHECK_INTEQUAL(hidjoy_num_sticks(handle), 3);

    num_events = 0;
    hidjoy_handle_input(handle, 1.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_X, 127);
    hidjoy_handle_input(handle, 1.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_Y, -128);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_INTEQUAL(events[0].joystick.index, 0);
    CHECK_FLOATEQUAL(events[0].joystick.x, 1);
    CHECK_FLOATEQUAL(events[0].joystick.y, -1);

    num_events = 0;
    hidjoy_handle_input(handle, 1.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_RX, -128);
    hidjoy_handle_input(handle, 1.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_RY, 127);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_INTEQUAL(events[0].joystick.index, 1);
    CHECK_FLOATEQUAL(events[0].joystick.x, -1);
    CHECK_FLOATEQUAL(events[0].joystick.y, 1);

    num_events = 0;
    hidjoy_handle_input(handle, 1.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_RZ, 255);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_INTEQUAL(events[0].joystick.index, 2);
    CHECK_FLOATEQUAL(events[0].joystick.x, 0);
    CHECK_FLOATEQUAL(events[0].joystick.y, 1);

    hidjoy_destroy(handle);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_native_dpad)
{
    HIDJoystickHandle *handle;
    CHECK_TRUE(handle = hidjoy_create(&dpad_joystick_info));
    hidjoy_set_event_callback(handle, event_callback, 42);

    /* The native D-pad inputs should not be treated as buttons. */
    CHECK_INTEQUAL(hidjoy_num_buttons(handle), 4);

    num_events = 0;
    hidjoy_handle_input(handle, 1.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_DPAD_UP, 1);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_FLOATEQUAL(events[0].joystick.x, 0);
    CHECK_FLOATEQUAL(events[0].joystick.y, -1);

    num_events = 0;
    hidjoy_handle_input(handle, 2.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_DPAD_UP, 1);  // Duplicate event.
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_FLOATEQUAL(events[0].joystick.x, 0);
    CHECK_FLOATEQUAL(events[0].joystick.y, -1);

    num_events = 0;
    hidjoy_handle_input(handle, 3.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_DPAD_LEFT, 1);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 3.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_FLOATEQUAL(events[0].joystick.x, -1);
    CHECK_FLOATEQUAL(events[0].joystick.y, -1);

    num_events = 0;
    hidjoy_handle_input(handle, 4.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_DPAD_UP, 0);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 4.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_FLOATEQUAL(events[0].joystick.x, -1);
    CHECK_FLOATEQUAL(events[0].joystick.y, 0);

    num_events = 0;
    hidjoy_handle_input(handle, 5.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_DPAD_DOWN, 1);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 5.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_FLOATEQUAL(events[0].joystick.x, -1);
    CHECK_FLOATEQUAL(events[0].joystick.y, 1);

    num_events = 0;
    hidjoy_handle_input(handle, 6.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_DPAD_LEFT, 0);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 6.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_FLOATEQUAL(events[0].joystick.x, 0);
    CHECK_FLOATEQUAL(events[0].joystick.y, 1);

    num_events = 0;
    hidjoy_handle_input(handle, 7.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_DPAD_RIGHT, 1);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 7.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_FLOATEQUAL(events[0].joystick.x, 1);
    CHECK_FLOATEQUAL(events[0].joystick.y, 1);

    /* The code should handle opposite directions being pressed at the
     * same time and should not merge consecutive events. */
    num_events = 0;
    hidjoy_handle_input(handle, 8.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_DPAD_UP, 1);
    hidjoy_handle_input(handle, 8.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_DPAD_LEFT, 1);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 8.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_FLOATEQUAL(events[0].joystick.x, 1);
    CHECK_FLOATEQUAL(events[0].joystick.y, 0);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 8.0);
    CHECK_INTEQUAL(events[1].joystick.device, 42);
    CHECK_FLOATEQUAL(events[1].joystick.x, 0);
    CHECK_FLOATEQUAL(events[1].joystick.y, 0);

    hidjoy_destroy(handle);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_hat_dpad)
{
    HIDJoystickHandle *handle;
    CHECK_TRUE(handle = hidjoy_create(&hat_joystick_info));
    hidjoy_set_event_callback(handle, event_callback, 42);

    num_events = 0;
    hidjoy_handle_input(handle, 1.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_HAT, 1);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_FLOATEQUAL(events[0].joystick.x, 0);
    CHECK_FLOATEQUAL(events[0].joystick.y, -1);

    num_events = 0;
    hidjoy_handle_input(handle, 1.5, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_HAT, 1);  // Duplicate event.
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 0);

    num_events = 0;
    hidjoy_handle_input(handle, 2.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_HAT, 0);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_FLOATEQUAL(events[0].joystick.x, 0);
    CHECK_FLOATEQUAL(events[0].joystick.y, 0);

    num_events = 0;
    hidjoy_handle_input(handle, 3.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_HAT, 2);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 3.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_FLOATEQUAL(events[0].joystick.x, 1);
    CHECK_FLOATEQUAL(events[0].joystick.y, -1);

    num_events = 0;
    hidjoy_handle_input(handle, 4.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_HAT, 3);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 4.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_FLOATEQUAL(events[0].joystick.x, 1);
    CHECK_FLOATEQUAL(events[0].joystick.y, 0);

    num_events = 0;
    hidjoy_handle_input(handle, 5.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_HAT, 4);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 5.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_FLOATEQUAL(events[0].joystick.x, 1);
    CHECK_FLOATEQUAL(events[0].joystick.y, 1);

    num_events = 0;
    hidjoy_handle_input(handle, 6.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_HAT, 5);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 6.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_FLOATEQUAL(events[0].joystick.x, 0);
    CHECK_FLOATEQUAL(events[0].joystick.y, 1);

    num_events = 0;
    hidjoy_handle_input(handle, 7.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_HAT, 6);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 7.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_FLOATEQUAL(events[0].joystick.x, -1);
    CHECK_FLOATEQUAL(events[0].joystick.y, 1);

    num_events = 0;
    hidjoy_handle_input(handle, 8.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_HAT, 7);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 8.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_FLOATEQUAL(events[0].joystick.x, -1);
    CHECK_FLOATEQUAL(events[0].joystick.y, 0);

    num_events = 0;
    hidjoy_handle_input(handle, 9.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_HAT, 8);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 9.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_FLOATEQUAL(events[0].joystick.x, -1);
    CHECK_FLOATEQUAL(events[0].joystick.y, -1);

    num_events = 0;
    hidjoy_handle_input(handle, 10.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_HAT, 9);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 10.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_FLOATEQUAL(events[0].joystick.x, 0);
    CHECK_FLOATEQUAL(events[0].joystick.y, 0);

    hidjoy_destroy(handle);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_native_dpad_with_hat)
{
    HIDJoystickHandle *handle;
    CHECK_TRUE(handle = hidjoy_create(&dpad_plus_hat_joystick_info));
    hidjoy_set_event_callback(handle, event_callback, 42);

    num_events = 0;
    hidjoy_handle_input(handle, 1.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_DPAD_UP, 1);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_FLOATEQUAL(events[0].joystick.x, 0);
    CHECK_FLOATEQUAL(events[0].joystick.y, -1);

    /* The hat input should not be translated to D-pad events. */
    num_events = 0;
    hidjoy_handle_input(handle, 1.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_HAT, 1);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 0);

    hidjoy_destroy(handle);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_hat_wrong_range)
{
    HIDJoystickInfo info = hat_joystick_info;
    info.values[0].logical_max = 9;  // Should cause the hat to be ignored.
    HIDJoystickHandle *handle;
    CHECK_TRUE(handle = hidjoy_create(&info));
    hidjoy_set_event_callback(handle, event_callback, 42);

    num_events = 0;
    hidjoy_handle_input(handle, 1.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_HAT, 1);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 0);

    hidjoy_destroy(handle);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_button_dpad)
{
    HIDJoystickHandle *handle;
    CHECK_TRUE(handle = hidjoy_create(&ps3_joystick_info));
    hidjoy_set_event_callback(handle, event_callback, 42);

    num_events = 0;
    hidjoy_handle_input(handle, 1.0, HID_PAGE_BUTTON, 5, 1);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_FLOATEQUAL(events[0].joystick.index, 4);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 1.0);
    CHECK_INTEQUAL(events[1].joystick.device, 42);
    CHECK_FLOATEQUAL(events[1].joystick.x, 0);
    CHECK_FLOATEQUAL(events[1].joystick.y, -1);

    num_events = 0;
    hidjoy_handle_input(handle, 2.0, HID_PAGE_BUTTON, 5, 1);  // Duplicate.
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_FLOATEQUAL(events[0].joystick.index, 4);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 2.0);
    CHECK_INTEQUAL(events[1].joystick.device, 42);
    CHECK_FLOATEQUAL(events[1].joystick.x, 0);
    CHECK_FLOATEQUAL(events[1].joystick.y, -1);

    num_events = 0;
    hidjoy_handle_input(handle, 3.0, HID_PAGE_BUTTON, 8, 1);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 3.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_FLOATEQUAL(events[0].joystick.index, 7);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 3.0);
    CHECK_INTEQUAL(events[1].joystick.device, 42);
    CHECK_FLOATEQUAL(events[1].joystick.x, -1);
    CHECK_FLOATEQUAL(events[1].joystick.y, -1);

    num_events = 0;
    hidjoy_handle_input(handle, 4.0, HID_PAGE_BUTTON, 5, 0);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_UP);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 4.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_FLOATEQUAL(events[0].joystick.index, 4);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 4.0);
    CHECK_INTEQUAL(events[1].joystick.device, 42);
    CHECK_FLOATEQUAL(events[1].joystick.x, -1);
    CHECK_FLOATEQUAL(events[1].joystick.y, 0);

    num_events = 0;
    hidjoy_handle_input(handle, 5.0, HID_PAGE_BUTTON, 7, 1);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 5.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_FLOATEQUAL(events[0].joystick.index, 6);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 5.0);
    CHECK_INTEQUAL(events[1].joystick.device, 42);
    CHECK_FLOATEQUAL(events[1].joystick.x, -1);
    CHECK_FLOATEQUAL(events[1].joystick.y, 1);

    num_events = 0;
    hidjoy_handle_input(handle, 6.0, HID_PAGE_BUTTON, 8, 0);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_UP);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 6.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_FLOATEQUAL(events[0].joystick.index, 7);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 6.0);
    CHECK_INTEQUAL(events[1].joystick.device, 42);
    CHECK_FLOATEQUAL(events[1].joystick.x, 0);
    CHECK_FLOATEQUAL(events[1].joystick.y, 1);

    num_events = 0;
    hidjoy_handle_input(handle, 7.0, HID_PAGE_BUTTON, 6, 1);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 7.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_FLOATEQUAL(events[0].joystick.index, 5);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 7.0);
    CHECK_INTEQUAL(events[1].joystick.device, 42);
    CHECK_FLOATEQUAL(events[1].joystick.x, 1);
    CHECK_FLOATEQUAL(events[1].joystick.y, 1);

    /* The code should handle opposite directions being pressed at the
     * same time and should not merge consecutive events. */
    num_events = 0;
    hidjoy_handle_input(handle, 8.0, HID_PAGE_BUTTON, 5, 1);
    hidjoy_handle_input(handle, 8.0, HID_PAGE_BUTTON, 8, 1);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 4);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 8.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_FLOATEQUAL(events[0].joystick.index, 4);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 8.0);
    CHECK_INTEQUAL(events[1].joystick.device, 42);
    CHECK_FLOATEQUAL(events[1].joystick.x, 1);
    CHECK_FLOATEQUAL(events[1].joystick.y, 0);
    CHECK_INTEQUAL(events[2].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[2].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[2].timestamp, 8.0);
    CHECK_INTEQUAL(events[2].joystick.device, 42);
    CHECK_FLOATEQUAL(events[2].joystick.index, 7);
    CHECK_INTEQUAL(events[3].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[3].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[3].timestamp, 8.0);
    CHECK_INTEQUAL(events[3].joystick.device, 42);
    CHECK_FLOATEQUAL(events[3].joystick.x, 0);
    CHECK_FLOATEQUAL(events[3].joystick.y, 0);

    hidjoy_destroy(handle);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_l2r2_trigger)
{
    HIDJoystickHandle *handle;
    CHECK_TRUE(handle = hidjoy_create(&x360_joystick_info));
    hidjoy_set_event_callback(handle, event_callback, 42);

    num_events = 0;
    hidjoy_handle_input(handle, 1.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_Z, 255);
    hidjoy_handle_input(handle, 1.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_RZ, 135); // Just short of midpoint+debounce.
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_INTEQUAL(events[0].joystick.index,
                   hidjoy_button_mapping(handle, INPUT_JOYBUTTON_L2));

    num_events = 0;
    hidjoy_handle_input(handle, 2.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_Z, 120);  // Just beyond midpoint - debounce.
    hidjoy_handle_input(handle, 2.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_RZ, 136);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_INTEQUAL(events[0].joystick.index,
                   hidjoy_button_mapping(handle, INPUT_JOYBUTTON_R2));

    num_events = 0;
    hidjoy_handle_input(handle, 3.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_Z, 119);
    hidjoy_handle_input(handle, 3.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_RZ, 255);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_UP);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 3.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_INTEQUAL(events[0].joystick.index,
                   hidjoy_button_mapping(handle, INPUT_JOYBUTTON_L2));

    num_events = 0;
    hidjoy_handle_input(handle, 3.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_Z, 0);
    hidjoy_handle_input(handle, 3.0, HID_PAGE_GENERIC_DESKTOP,
                        HID_USAGE_RZ, 0);
    hidjoy_flush_events(handle);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_UP);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 3.0);
    CHECK_INTEQUAL(events[0].joystick.device, 42);
    CHECK_INTEQUAL(events[0].joystick.index,
                   hidjoy_button_mapping(handle, INPUT_JOYBUTTON_R2));

    hidjoy_destroy(handle);
    return 1;
}

/*************************************************************************/
/*************************************************************************/
