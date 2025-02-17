/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/misc/joystick-hid.h: Header for HID joystick common code.
 */

#ifndef SIL_SRC_SYSDEP_MISC_JOYSTICK_HID_H
#define SIL_SRC_SYSDEP_MISC_JOYSTICK_HID_H

struct InputEvent;

/*************************************************************************/
/*************************************************************************/

/* HID Usage Page values. */
#define HID_PAGE_GENERIC_DESKTOP  1
#define HID_PAGE_BUTTON           9

/* HID Usage values. */
#define HID_USAGE_JOYSTICK        0x04
#define HID_USAGE_GAMEPAD         0x05
#define HID_USAGE_X               0x30
#define HID_USAGE_Y               0x31
#define HID_USAGE_Z               0x32
#define HID_USAGE_RX              0x33
#define HID_USAGE_RY              0x34
#define HID_USAGE_RZ              0x35
#define HID_USAGE_HAT             0x39
#define HID_USAGE_START           0x3D
#define HID_USAGE_SELECT          0x3E
#define HID_USAGE_DPAD_UP         0x90
#define HID_USAGE_DPAD_DOWN       0x91
#define HID_USAGE_DPAD_RIGHT      0x92
#define HID_USAGE_DPAD_LEFT       0x93
#define HID_USAGE_IS_DPAD(usage)  (((usage) & ~3) == HID_USAGE_DPAD_UP)

/**
 * HIDJoystickHandle:  Data type for a HID joystick device handle.
 */
typedef struct HIDJoystickHandle HIDJoystickHandle;

/**
 * HIDJoystickValueInfo:  Structure describing a value input for a HID
 * joystick device.  Used in the HIDJoystickInfo structure.
 */
typedef struct HIDJoystickValueInfo HIDJoystickValueInfo;
struct HIDJoystickValueInfo {
    uint32_t usage;  // usage_page<<16 | usage, as for buttons.
    int logical_min;
    int logical_max;
};

/**
 * HIDJoystickInfo:  Structure describing a HID joystick device.
 */
typedef struct HIDJoystickInfo HIDJoystickInfo;
struct HIDJoystickInfo {
    /* Vendor and product IDs. */
    uint16_t vendor_id, product_id;
    /* Device version reported by the device driver, or 0 if not available. */
    uint32_t dev_version;
    /* Name reported by the device driver, or NULL if not available. */
    char *name;
    /* Serial number (or similar identifying string) of the device, or
     * NULL if not available. */
    char *serial;

    /* Button inputs available on the device.  Each entry is a 32-bit
     * value generated as (usage_page<<16 | usage). */
    int num_buttons;
    uint32_t *buttons;

    /* Value inputs available on the device. */
    int num_values;
    HIDJoystickValueInfo *values;
};

/*-----------------------------------------------------------------------*/

/**
 * hidjoy_create:  Create a handle for a HID joystick device.
 *
 * [Parameters]
 *     info: HIDJoystickInfo structure describing the device.
 * [Return value]
 *     New HID joystick handle, or NULL on error.
 */
extern HIDJoystickHandle *hidjoy_create(const HIDJoystickInfo *info);

/**
 * hidjoy_destroy:  Destroy a handle for a HID joystick device.
 *
 * [Parameters]
 *     handle: Joystick handle to destroy (may be NULL).
 */
extern void hidjoy_destroy(HIDJoystickHandle *handle);

/**
 * hidjoy_set_event_callback:  Set the callback function and device index
 * for input events generated by this joystick.
 *
 * An event callback must be set with this function before calling
 * hidjoy_handle_input().
 *
 * [Parameters]
 *     handle: Joystick handle.
 *     event_callback: Callback function to which events will be passed.
 *     device_index: Joystick device index to use for generated events.
 */
extern void hidjoy_set_event_callback(
    HIDJoystickHandle *handle,
    void (*event_callback)(const struct InputEvent *), int device_index);

/**
 * hidjoy_vendor_id, hidjoy_product_id:  Return the vendor or product ID
 * for the given joystick, as passed to hidjoy_create() in the
 * HIDJoystickInfo structure.
 *
 * [Parameters]
 *     handle: Joystick handle.
 * [Return value]
 *     Vendor or product ID.
 */
extern PURE_FUNCTION int hidjoy_vendor_id(const HIDJoystickHandle *handle);
extern PURE_FUNCTION int hidjoy_product_id(const HIDJoystickHandle *handle);

/**
 * hidjoy_vendor_id, hidjoy_product_id:  Return the vendor or product ID
 * for the given joystick, as passed to hidjoy_create() in the
 * HIDJoystickInfo structure.
 *
 * [Parameters]
 *     handle: Joystick handle.
 * [Return value]
 *     Vendor or product ID.
 */
extern PURE_FUNCTION const char *hidjoy_name(const HIDJoystickHandle *handle);
extern PURE_FUNCTION const char *hidjoy_serial(const HIDJoystickHandle *handle);

/**
 * hidjoy_num_buttons, hidjoy_num_sticks:  Return the number of button or
 * stick inputs for the given joystick.  The returned pointer is valid
 * until the handle is destroyed.
 *
 * [Parameters]
 *     handle: Joystick handle.
 * [Return value]
 *     Number of button or stick inputs.
 */
extern PURE_FUNCTION int hidjoy_num_buttons(const HIDJoystickHandle *handle);
extern PURE_FUNCTION int hidjoy_num_sticks(const HIDJoystickHandle *handle);

/**
 * hidjoy_button_mapping:  Return the button corresponding to the given
 * logical button name, or -1 if there is no corresponding button.  This
 * function can be used to implement sys_input_joystick_button_mapping().
 *
 * [Parameters]
 *     handle: Joystick handle.
 *     name: Logical button name to look up (INPUT_JOYBUTTON_*).
 * [Return value]
 *     Corresponding joystick button index, or -1 if none.
 */
extern PURE_FUNCTION int hidjoy_button_mapping(const HIDJoystickHandle *handle,
                                               int name);

/**
 * hidjoy_is_input_used:  Return whether the given usage page and usage
 * value represent an input used by SIL (and thus one which should be
 * passed to hidjoy_handle_input()).
 *
 * [Parameters]
 *     handle: Joystick handle.
 *     usage_page: Usage page of the input.
 *     usage: Usage value of the input.
 * [Return value]
 *     True if the input is used, false if not.
 */
extern PURE_FUNCTION int hidjoy_is_input_used(const HIDJoystickHandle *handle,
                                              int usage_page, int usage);

/**
 * hidjoy_handle_input:  Process an input value from a joystick.
 *
 * [Parameters]
 *     handle: Joystick handle.
 *     usage_page: Usage page of the input.
 *     usage: Usage value of the input.
 *     value: Input value.
 *     timestamp: Timestamp to use for generated events.
 */
extern void hidjoy_handle_input(HIDJoystickHandle *handle, double timestamp,
                                int usage_page, int usage, int value);

/**
 * hidjoy_flush_events:  Flush any pending events on a joystick.  This
 * function must be called after each sequence of one ore more calls to
 * hidjoy_handle_input() to ensure that all events have been generated.
 *
 * [Parameters]
 *     handle: Joystick handle.
 */
extern void hidjoy_flush_events(HIDJoystickHandle *handle);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_MISC_JOYSTICK_HID_H
