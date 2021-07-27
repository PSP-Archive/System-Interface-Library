/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/misc/joystick-db.h: Header for the HID joystick database.
 */

#ifndef SIL_SRC_SYSDEP_MISC_JOYSTICK_DB_H
#define SIL_SRC_SYSDEP_MISC_JOYSTICK_DB_H

/*************************************************************************/
/*************************************************************************/

/**
 * JoystickDpadType:  D-pad input types.
 */
typedef enum JoystickDpadType {
    /* No D-pad input. */
    JOYSTICK_DPAD_NONE = 0,
    /* D-pad input uses the Generic Desktop D-pad elements. */
    JOYSTICK_DPAD_NATIVE,
    /* D-pad input uses the X and Y elements. */
    JOYSTICK_DPAD_XY,
    /* D-pad input uses the hat element. */
    JOYSTICK_DPAD_HAT,
    /* D-pad input uses buttons for each cardinal direction. */
    JOYSTICK_DPAD_BUTTONS,
} JoystickDpadType;


/**
 * JoystickLinuxRumbleType:  Linux rumble motor assignment types.
 */
typedef enum JoystickLinuxRumbleType {
    /* None of the below, or unknown. */
    JOYSTICK_LINUX_RUMBLE_UNKNOWN = 0,
    /* The left motor corresponds to the Linux driver's "strong" motor,
     * and the right motor is the "weak" motor. */
    JOYSTICK_LINUX_RUMBLE_LEFT_STRONG,
    /* The right motor corresponds to the Linux driver's "strong" motor,
     * and the left motor is the "weak" motor. */
    JOYSTICK_LINUX_RUMBLE_RIGHT_STRONG,
} JoystickLinuxRumbleType;


/**
 * JoystickValueInput:  Constants representing value (axis) inputs.
 */
typedef enum JoystickValueInput {
    JOYSTICK_VALUE_NONE = 0,  // Indicates "input not available".
    JOYSTICK_VALUE_X,
    JOYSTICK_VALUE_Y,
    JOYSTICK_VALUE_Z,
    JOYSTICK_VALUE_RX,
    JOYSTICK_VALUE_RY,
    JOYSTICK_VALUE_RZ,
    JOYSTICK_VALUE_HAT,
} JoystickValueInput;

/**
 * JoystickDesc:  Structure describing a joystick device configuration.
 */
typedef struct JoystickDesc JoystickDesc;
struct JoystickDesc {

    /* Name(s) reported by the device or its driver. */
    const char *names[2];
    /* Should we match this configuration regardless of device name (if
     * vendor and product ID match)? */
    uint8_t ignore_name;

    /* Vendor and product IDs. */
    uint16_t vendor_id, product_id;
    /* Should we match this configuration regardless of vendor and product
     * IDs and version code (if the name matches)? */
    uint8_t ignore_vid_pid;

    /* Hardware/driver version code. */
    uint32_t dev_version;
    /* Mask for testing the version code (0 = ignore version code). */
    uint32_t version_mask;

    /* X and Y inputs (JOYSTICK_VALUE_*) for the left and right analog
     * sticks, or JOYSTICK_VALUE_NONE if the device does not have such an
     * input. */
    int8_t lstick_x, lstick_y;
    int8_t rstick_x, rstick_y;

    /* D-pad input type (JoystickDpadType). */
    int8_t dpad_type;
    /* Button numbers for D-pad inputs if type == JOYSTICK_DPAD_BUTTONS,
     * else -1. */
    int8_t dpad_up, dpad_down, dpad_left, dpad_right;

    /* Mapping from logical button names to button indices. */
    int8_t button_map[INPUT_JOYBUTTON__NUM];

    /* Value inputs for L2 and R2, if they are mapped to values instead of
     * buttons.  JOYSTICK_VALUE_NONE indicates that the inputs either are
     * buttons or do not exist on the device. */
    int8_t l2_value, r2_value;

    /* Rumble motor assignment type (Linux-specific). */
    JoystickLinuxRumbleType linux_rumble;
};

/*-----------------------------------------------------------------------*/

/**
 * joydb_lookup:  Return the configuration, if any, corresponding to the
 * given device parameters.
 *
 * [Parameters]
 *     vendor_id: Vendor ID of device.
 *     product_id: Product ID of device.
 *     dev_version: Hardware/driver version code.
 *     name: Name string reported by device, or NULL if the device did not
 *         report any name.
 * [Return value]
 *     Device configuration, or NULL if there are no matching records in
 *     the database.
 */
extern const JoystickDesc *joydb_lookup(int vendor_id, int product_id,
                                        uint32_t dev_version, const char *name);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_MISC_JOYSTICK_DB_H
