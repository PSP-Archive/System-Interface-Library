/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/misc/joystick-hid.c: Common code for handling HID-type
 * joystick devices.
 */

#include "src/base.h"
#include "src/input.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep/misc/joystick-db.h"
#include "src/sysdep/misc/joystick-hid.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

enum {  // Indices for HIDJoystickHandle.value_info[].
    VALUE_X,
    VALUE_Y,
    VALUE_Z,
    VALUE_RX,
    VALUE_RY,
    VALUE_RZ,
    VALUE_HAT,
    VALUE__NUM
};

/* HID joystick handle structure. */
struct HIDJoystickHandle {
    /* Callback and device index for generated events. */
    void (*event_callback)(const struct InputEvent *);
    int device_index;

    /* Device identification data. */
    uint16_t vendor_id, product_id;
    uint32_t dev_version;
    char *name;    // Might be NULL.
    char *serial;  // Might be NULL.

    /* Number of buttons and sticks available on the device. */
    int num_buttons, num_sticks;
    /* Number of real buttons, excluding virtual button indices used for
     * value-based trigger inputs. */
    int num_real_buttons;

    /* Current state of all inputs. */
    uint8_t button_state[INPUT_MAX_JOYSTICK_BUTTONS];
    uint8_t l2_value_state, r2_value_state;  // For value-input triggers.
    uint8_t dpad_state_up, dpad_state_down, dpad_state_left, dpad_state_right;
    Vector2f stick_state[3];
    /* Timestamp of the most recent change to stick_state if that change
     * has not yet been sent to the event callback, else zero. */
    double stick_timestamp[3];

    /* Mapping from button IDs used in the SIL interface to button IDs
     * (UsagePage<<16 | Usage) used by the device.  Entries 0 through
     * num_buttons-1 are valid. */
    uint32_t buttons[INPUT_MAX_JOYSTICK_BUTTONS];

    /* Mapping from SIL button names to button IDs. */
    int8_t button_map[INPUT_JOYBUTTON__NUM];

    /* Data for value-type inputs, indexed by VALUE_*. */
    struct {
        /* Is this entry valid? */
        uint8_t valid;
        /* Usage values.  (These are constant for any given index, but we
         * save them here for convenience.) */
        uint16_t usage_page;
        uint16_t usage;
        /* Minimum and maximum value of the logical value range,
         * corresponding to -1.0 and +1.0 after scaling. */
        int logical_min;
        int logical_max;
    } value_info[VALUE__NUM];

    /* D-pad configuration.  At most one of these three will be true/valid: */
    /* - Does this device have a native D-pad input? */
    uint8_t dpad_native;
    /* - Does this device have a hat-based D-pad input? */
    uint8_t dpad_hat;
    /* - SIL button IDs for D-pad inputs, or -1 if none. */
    int8_t dpad_up, dpad_down, dpad_left, dpad_right;

    /* Value indices for up to 3 sticks; see add_joystick() for how
     * inputs are assigned.  Entries 0 through num_sticks-1 are valid. */
    uint8_t stick_x[3], stick_y[3];

    /* Value indices for the left and right triggers, or -1 if they are
     * buttons. */
    int8_t l2_value_index, r2_value_index;
};

/*--------------------- Local routine declarations ----------------------*/

/**
 * sort_uint32:  Sort an array of uint32_t values in ascending order.
 * Helper function for add_rawinput_joystick().
 *
 * [Parameters]
 *     array: Array to sort.
 *     len: Length of array.
 */
static void sort_uint32(uint32_t *array, int len);

/**
 * usage_to_button:  Return the button index corresponding to the given
 * HID usage value.
 *
 * [Parameters]
 *     handle: Joystick handle.
 *     usage: Usage value (page<<16 | usage).
 * [Return value]
 *     Button index, or -1 if there is no matching button.
 */
static PURE_FUNCTION int usage_to_button(const HIDJoystickHandle *handle,
                                         uint32_t usage);

/**
 * scale_input:  Return the scaled value (in the range [-1.0,+1.0])
 * corresponding to the given raw input value for a joystick value input.
 *
 * [Parameters]
 *     handle: Joystick handle.
 *     index: Value index (VALUE_*).
 *     value: Raw input value.
 * [Return value]
 *     Scaled value, in the range [-1.0,+1.0].
 */
static PURE_FUNCTION float scale_value(const HIDJoystickHandle *handle,
                                       int index, int value);

/**
 * update_stick:  Update the X or Y coordinate of a stick on a joystick
 * device.  If a previous change is already pending, that event is sent out.
 *
 * [Parameters]
 *     handle: Joystick handle.
 *     timestamp: Event timestamp, compatible with time_now().
 *     stick: Stick to update.
 *     is_y: 0 if updating the X axis, 1 if updating the Y axis.
 *     value: New value of axis.
 */
static void update_stick(HIDJoystickHandle *handle, double timestamp,
                         int stick, int is_y, float value);

/**
 * send_dpad_event:  Generate a joystick D-pad change event.
 *
 * [Parameters]
 *     handle: Joystick handle.
 *     timestamp: Timestamp to use for the event.
 */
static void send_dpad_event(HIDJoystickHandle *handle, double timestamp);

/**
 * send_dpad_event:  Generate a joystick stick change event.
 *
 * [Parameters]
 *     handle: Joystick handle.
 *     timestamp: Timestamp to use for the event.
 *     stick: Stick index.
 */
static void send_stick_event(HIDJoystickHandle *handle, double timestamp,
                             int stick);

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

extern HIDJoystickHandle *hidjoy_create(const HIDJoystickInfo *info)
{
    PRECOND(info != NULL, goto error_return);

    /* Allocate and initialize a handle for the joystick. */
    HIDJoystickHandle *handle = mem_alloc(sizeof(*handle), 0, MEM_ALLOC_CLEAR);
    if (UNLIKELY(!handle)) {
        DLOG("No memory for HIDJoystickHandle");
        goto error_return;
    }
    handle->vendor_id = info->vendor_id;
    handle->product_id = info->product_id;
    handle->dev_version = info->dev_version;
    if (info->name) {
        handle->name = mem_strdup(info->name, 0);
        if (UNLIKELY(!handle->name)) {
            DLOG("No memory for copy of joystick name: %s", info->name);
            goto error_free_handle;
        }
    } else {
        handle->name = NULL;
    }
    if (info->serial) {
        handle->serial = mem_strdup(info->serial, 0);
        if (UNLIKELY(!handle->serial)) {
            DLOG("No memory for copy of joystick serial: %s", info->serial);
            goto error_free_name;
        }
    } else {
        handle->serial = NULL;
    }
    for (int i = 0; i < lenof(handle->button_map); i++) {
        handle->button_map[i] = -1;
    }
    handle->dpad_up = -1;
    handle->dpad_down = -1;
    handle->dpad_left = -1;
    handle->dpad_right = -1;
    handle->l2_value_index = -1;
    handle->r2_value_index = -1;

    /* Assign button inputs to SIL button IDs or native D-pad inputs.  We
     * sort buttons by usage value to ensure a consistent ordering and to
     * allow the use of binary search when looking up usage values for
     * input events. */
    uint32_t *usages = mem_alloc(sizeof(*usages) * lbound(info->num_buttons,1),
                                 0, MEM_ALLOC_TEMP);
    if (UNLIKELY(!usages)) {
        DLOG("No memory for %d button usages", info->num_buttons);
        goto error_free_serial;
    }
    STATIC_ASSERT(sizeof(*usages) == sizeof(*info->buttons), "Size mismatch");
    int num_buttons = 0;
    for (int i = 0; i < info->num_buttons; i++) {
        if (info->buttons[i]>>16 == HID_PAGE_GENERIC_DESKTOP
         && HID_USAGE_IS_DPAD(info->buttons[i] & 0xFFFF)) {
            handle->dpad_native = 1;
        } else {
            usages[num_buttons++] = info->buttons[i];
        }
    }
    sort_uint32(usages, num_buttons);
    handle->num_buttons = ubound(num_buttons, lenof(handle->buttons));
    STATIC_ASSERT(sizeof(*handle->buttons) == sizeof(*usages), "Size mismatch");
    memcpy(handle->buttons, usages, handle->num_buttons * sizeof(*usages));
    mem_free(usages);
    handle->num_real_buttons = handle->num_buttons;

    /* Look for value inputs that we're interested in. */
    struct {uint8_t x, y, z, rx, ry, rz;} stick_inputs = {0,0,0,0,0,0};
    for (int i = 0; i < info->num_values; i++) {
        const int usage_page = info->values[i].usage >> 16;
        const int usage = info->values[i].usage & 0xFFFF;
        const int logical_min = info->values[i].logical_min;
        const int logical_max = info->values[i].logical_max;
        if (UNLIKELY(logical_min >= logical_max)) {
            DLOG("%s (%04X/%04X): Ignoring value input (usage %d/0x%02X)"
                 " with invalid range %d-%d", handle->name, handle->vendor_id,
                 handle->product_id, usage_page, usage, logical_min,
                 logical_max);
            continue;
        }
        int value_index = -1;
        switch (usage) {
          case HID_USAGE_X:
            value_index = VALUE_X;
            stick_inputs.x = 1;
            break;
          case HID_USAGE_Y:
            value_index = VALUE_Y;
            stick_inputs.y = 1;
            break;
          case HID_USAGE_Z:
            value_index = VALUE_Z;
            stick_inputs.z = 1;
            break;
          case HID_USAGE_RX:
            value_index = VALUE_RX;
            stick_inputs.rx = 1;
            break;
          case HID_USAGE_RY:
            value_index = VALUE_RY;
            stick_inputs.ry = 1;
            break;
          case HID_USAGE_RZ:
            value_index = VALUE_RZ;
            stick_inputs.rz = 1;
            break;
          case HID_USAGE_HAT:
            if (logical_max - logical_min == 7) {
                value_index = VALUE_HAT;
                handle->dpad_hat = 1;
            } else {
                DLOG("%s (%04X/%04X): Unsupported hat range %d-%d",
                     handle->name, handle->vendor_id, handle->product_id,
                     logical_min, logical_max);
            }
            break;
        }
        if (value_index >= 0) {
            handle->value_info[value_index].valid = 1;
            handle->value_info[value_index].usage_page = usage_page;
            handle->value_info[value_index].usage = usage;
            handle->value_info[value_index].logical_min = logical_min;
            handle->value_info[value_index].logical_max = logical_max;
        }
    }

    /* Assign [XYZ] and R[XYZ] inputs to sticks as follows:
     *
     * - If the device supports Z and RX but not RY or RZ, assign X and Y
     *   to the first stick (whether or not they are supported) and Z and
     *   RX to the second stick.
     *
     * - If the device supports Z and RZ but not RX or RY, assign X and Y
     *   to the first stick (whether or not they are supported) and Z and
     *   RZ to the second stick.
     *
     * - Otherwise, assign X and Y to the first stick, RX and RY to the
     *   second stick, and Z and RZ to the third stick, then remove all
     *   trailing sticks for which both axes are unsupported.
     */
    handle->stick_x[0] = VALUE_X;
    handle->stick_y[0] = VALUE_Y;
    if (stick_inputs.z && stick_inputs.rx
     && !stick_inputs.ry && !stick_inputs.rz) {
        handle->num_sticks = 2;
        handle->stick_x[1] = VALUE_Z;
        handle->stick_y[1] = VALUE_RX;
    } else if (stick_inputs.z && stick_inputs.rz
            && !stick_inputs.rx && !stick_inputs.ry) {
        handle->num_sticks = 2;
        handle->stick_x[1] = VALUE_Z;
        handle->stick_y[1] = VALUE_RZ;
    } else {
        handle->stick_x[1] = VALUE_RX;
        handle->stick_y[1] = VALUE_RY;
        handle->stick_x[2] = VALUE_Z;
        handle->stick_y[2] = VALUE_RZ;
        if (stick_inputs.x || stick_inputs.y) {
            handle->num_sticks = 1;
        }
        if (stick_inputs.rx || stick_inputs.ry) {
            handle->num_sticks = 2;
        }
        if (stick_inputs.z || stick_inputs.rz) {
            handle->num_sticks = 3;
        }
    }

    /* Sanitize D-pad configuration. */
    if (handle->dpad_native) {
        handle->dpad_hat = 0;
    }
    if (handle->dpad_native || handle->dpad_hat) {
        handle->dpad_up = -1;
        handle->dpad_down = -1;
        handle->dpad_left = -1;
        handle->dpad_right = -1;
    }

    /* If we know about this device, update assignments accordingly. */
    const JoystickDesc *desc =
        joydb_lookup(handle->vendor_id, handle->product_id,
                     handle->dev_version, handle->name);
    if (desc) {
        for (int j = 0; j < INPUT_JOYBUTTON__NUM; j++) {
            handle->button_map[j] = desc->button_map[j];
        }
        handle->dpad_native = (desc->dpad_type == JOYSTICK_DPAD_NATIVE);
        handle->dpad_hat    = (desc->dpad_type == JOYSTICK_DPAD_HAT);
        if (desc->dpad_type == JOYSTICK_DPAD_BUTTONS) {
            handle->dpad_up     = desc->dpad_up;
            handle->dpad_down   = desc->dpad_down;
            handle->dpad_left   = desc->dpad_left;
            handle->dpad_right  = desc->dpad_right;
        }
        handle->num_sticks = 0;
        if (desc->lstick_x != JOYSTICK_VALUE_NONE) {
            handle->num_sticks = 1;
            /* A bit of a hack based on all JOYSTICK_VALUE_* constants
             * being one greater than our local VALUE_* constants. */
            handle->stick_x[0] = desc->lstick_x - 1;
            handle->stick_y[0] = desc->lstick_y - 1;
        }
        if (desc->rstick_x != JOYSTICK_VALUE_NONE) {
            handle->num_sticks = 2;
            handle->stick_x[1] = desc->rstick_x - 1;
            handle->stick_y[1] = desc->rstick_y - 1;
        }
        if (desc->l2_value != JOYSTICK_VALUE_NONE) {
            handle->l2_value_index = desc->l2_value - 1;
            handle->button_map[INPUT_JOYBUTTON_L2] = handle->num_buttons++;
        }
        if (desc->r2_value != JOYSTICK_VALUE_NONE) {
            handle->r2_value_index = desc->r2_value - 1;
            handle->button_map[INPUT_JOYBUTTON_R2] = handle->num_buttons++;
        }
    }

    /* All done, return the new handle. */
    return handle;

  error_free_serial:
    mem_free(handle->serial);
  error_free_name:
    mem_free(handle->name);
  error_free_handle:
    mem_free(handle);
  error_return:
    return NULL;
}

/*-----------------------------------------------------------------------*/

void hidjoy_destroy(HIDJoystickHandle *handle)
{
    if (!handle) {
        return;
    }

    mem_free(handle->serial);
    mem_free(handle->name);
    mem_free(handle);
}

/*-----------------------------------------------------------------------*/

void hidjoy_set_event_callback(
    HIDJoystickHandle *handle,
    void (*event_callback)(const struct InputEvent *), int device_index)
{
    PRECOND(handle != NULL, return);
    handle->event_callback = event_callback;
    handle->device_index = device_index;
}

/*-----------------------------------------------------------------------*/

int hidjoy_vendor_id(const HIDJoystickHandle *handle)
{
    PRECOND(handle != NULL, return 0);
    return handle->vendor_id;
}

/*-----------------------------------------------------------------------*/

int hidjoy_product_id(const HIDJoystickHandle *handle)
{
    PRECOND(handle != NULL, return 0);
    return handle->product_id;
}

/*-----------------------------------------------------------------------*/

const char *hidjoy_name(const HIDJoystickHandle *handle)
{
    PRECOND(handle != NULL, return NULL);
    return handle->name;
}

/*-----------------------------------------------------------------------*/

const char *hidjoy_serial(const HIDJoystickHandle *handle)
{
    PRECOND(handle != NULL, return NULL);
    return handle->serial;
}

/*-----------------------------------------------------------------------*/

int hidjoy_num_buttons(const HIDJoystickHandle *handle)
{
    PRECOND(handle != NULL, return 0);
    return handle->num_buttons;
}

/*-----------------------------------------------------------------------*/

int hidjoy_num_sticks(const HIDJoystickHandle *handle)
{
    PRECOND(handle != NULL, return 0);
    return handle->num_sticks;
}

/*-----------------------------------------------------------------------*/

int hidjoy_button_mapping(const HIDJoystickHandle *handle, int name)
{
    PRECOND(handle != NULL, return -1);
    return handle->button_map[name];
}

/*-----------------------------------------------------------------------*/

int hidjoy_is_input_used(const HIDJoystickHandle *handle, int usage_page,
                         int usage)
{
    PRECOND(handle != NULL, return 0);

    if (usage_page == HID_PAGE_GENERIC_DESKTOP && HID_USAGE_IS_DPAD(usage)) {
        return handle->dpad_native;
    } else if (usage_to_button(handle, usage_page<<16 | usage) >= 0) {
        return 1;
    } else if (usage_page == HID_PAGE_GENERIC_DESKTOP
            && usage == HID_USAGE_HAT) {
        return handle->dpad_hat;
    } else {
        int value_index = -1;
        for (int i = 0; i < lenof(handle->value_info); i++) {
            if (handle->value_info[i].usage_page == usage_page
             && handle->value_info[i].usage == usage) {
                value_index = i;
                break;
            }
        }
        return value_index >= 0;
    }
}

/*-----------------------------------------------------------------------*/

void hidjoy_handle_input(HIDJoystickHandle *handle, double timestamp,
                         int usage_page, int usage, int value)
{
    PRECOND(handle != NULL, return);
    PRECOND(handle->event_callback != NULL, return);

    if (usage_page == HID_PAGE_GENERIC_DESKTOP && HID_USAGE_IS_DPAD(usage)) {

        if (!handle->dpad_native) {
            return;
        }

        const int state = (value != 0);
        switch (usage) {
            case HID_USAGE_DPAD_UP:    handle->dpad_state_up = state;    break;
            case HID_USAGE_DPAD_DOWN:  handle->dpad_state_down = state;  break;
            case HID_USAGE_DPAD_LEFT:  handle->dpad_state_left = state;  break;
            case HID_USAGE_DPAD_RIGHT: handle->dpad_state_right = state; break;
        }
        send_dpad_event(handle, timestamp);

    } else if (usage_to_button(handle, usage_page<<16 | usage) >= 0) {

        const int state = (value != 0);
        const int button = usage_to_button(handle, usage_page<<16 | usage);
        const InputEventDetail detail = value ? INPUT_JOYSTICK_BUTTON_DOWN
                                              : INPUT_JOYSTICK_BUTTON_UP;
        (*handle->event_callback)(&(InputEvent){
            .type = INPUT_EVENT_JOYSTICK, .detail = detail,
            .timestamp = timestamp,
            {.joystick = {.device = handle->device_index, .index = button}}});
        if (button == handle->dpad_up) {
            handle->dpad_state_up = state;
            send_dpad_event(handle, timestamp);
        } else if (button == handle->dpad_down) {
            handle->dpad_state_down = state;
            send_dpad_event(handle, timestamp);
        } else if (button == handle->dpad_left) {
            handle->dpad_state_left = state;
            send_dpad_event(handle, timestamp);
        } else if (button == handle->dpad_right) {
            handle->dpad_state_right = state;
            send_dpad_event(handle, timestamp);
        }

    } else if (usage_page == HID_PAGE_GENERIC_DESKTOP
               && usage == HID_USAGE_HAT) {

        if (!handle->dpad_hat) {
            return;
        }

        int new_up, new_down, new_left, new_right;
        const int hat_value =
            value - handle->value_info[VALUE_HAT].logical_min;
        /* D-pad hat values range from 0 through 7, indicating clockwise
         * 45-degree increments from "up" (away from the user): so 0 is
         * "up", 3 is "down"+"right", and so on.  Out-of-range values
         * indicate no input. */
        if (hat_value < 0 || hat_value > 7) {
            new_up = new_down = new_left = new_right = 0;
        } else {
            new_up    = (hat_value == 7 || hat_value <= 1);
            new_down  = (hat_value >= 3 && hat_value <= 5);
            new_left  = (hat_value >= 5);
            new_right = (hat_value >= 1 && hat_value <= 3);
        }
        const int old_dpad_x =
            handle->dpad_state_right - handle->dpad_state_left;
        const int old_dpad_y = handle->dpad_state_down - handle->dpad_state_up;
        const int new_dpad_x = new_right - new_left;
        const int new_dpad_y = new_down - new_up;
        handle->dpad_state_up    = new_up;
        handle->dpad_state_down  = new_down;
        handle->dpad_state_left  = new_left;
        handle->dpad_state_right = new_right;
        if (new_dpad_x != old_dpad_x || new_dpad_y != old_dpad_y) {
            send_dpad_event(handle, timestamp);
        }

    } else {  // not D-pad, button, or hat

        int value_index = -1;
        for (int i = 0; i < lenof(handle->value_info); i++) {
            if (handle->value_info[i].usage_page == usage_page
             && handle->value_info[i].usage == usage) {
                value_index = i;
                break;
            }
        }
        if (value_index < 0) {
            return;
        }
        const float scaled_value = scale_value(handle, value_index, value);
        if (handle->l2_value_index == value_index) {
            /* Debounce the input by 1/16 on either side of the center
             * point. */
            int state;
            if (handle->l2_value_state) {
                state = (scaled_value >= -0.0625f);
            } else {
                state = (scaled_value >= 0.0625f);
            }
            if (state != handle->l2_value_state) {
                handle->l2_value_state = state;
                const InputEventDetail detail =
                    state ? INPUT_JOYSTICK_BUTTON_DOWN
                          : INPUT_JOYSTICK_BUTTON_UP;
                (*handle->event_callback)(&(InputEvent){
                    .type = INPUT_EVENT_JOYSTICK, .detail = detail,
                    .timestamp = timestamp,
                    {.joystick = {.device = handle->device_index,
                                  .index = handle->button_map[INPUT_JOYBUTTON_L2]}}});
            }
        } else if (handle->r2_value_index == value_index) {
            int state;
            if (handle->r2_value_state) {
                state = (scaled_value >= -0.0625f);
            } else {
                state = (scaled_value >= 0.0625f);
            }
            if (state != handle->r2_value_state) {
                handle->r2_value_state = state;
                const InputEventDetail detail =
                    state ? INPUT_JOYSTICK_BUTTON_DOWN
                          : INPUT_JOYSTICK_BUTTON_UP;
                (*handle->event_callback)(&(InputEvent){
                    .type = INPUT_EVENT_JOYSTICK, .detail = detail,
                    .timestamp = timestamp,
                    {.joystick = {.device = handle->device_index,
                                  .index = handle->button_map[INPUT_JOYBUTTON_R2]}}});
            }
        } else {
            for (int stick = 0; stick < handle->num_sticks; stick++) {
                if (handle->stick_x[stick] == value_index) {
                    if (scaled_value != handle->stick_state[stick].x) {
                        update_stick(handle, timestamp, stick, 0, scaled_value);
                    }
                    break;
                } else if (handle->stick_y[stick] == value_index) {
                    if (scaled_value != handle->stick_state[stick].y) {
                        update_stick(handle, timestamp, stick, 1, scaled_value);
                    }
                    break;
                }
            }
        }

    }  // if (various usages)
}

/*-----------------------------------------------------------------------*/

void hidjoy_flush_events(HIDJoystickHandle *handle)
{
    for (int stick = 0; stick < lenof(handle->stick_state); stick++) {
        if (handle->stick_timestamp[stick]) {
            send_stick_event(handle, handle->stick_timestamp[stick], stick);
            handle->stick_timestamp[stick] = 0;
        }
    }
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static void sort_uint32(uint32_t *array, int len)
{
    for (int i = 0; i < len-1; i++) {
        int best = i;
        for (int j = i+1; j < len; j++) {
            if (array[j] < array[best]) {
                best = j;
            }
        }
        if (best != i) {
            const uint32_t temp = array[i];
            array[i] = array[best];
            array[best] = temp;
        }
    }
}

/*-----------------------------------------------------------------------*/

static int usage_to_button(const HIDJoystickHandle *handle, uint32_t usage)
{
    int low = 0, high = handle->num_real_buttons - 1;
    while (low <= high) {
        const int mid = (low + high) / 2;
        if (usage == handle->buttons[mid]) {
            return mid;
        } else if (usage < handle->buttons[mid]) {
            high = mid-1;
        } else {
            low = mid+1;
        }
    }
    return -1;
}

/*-----------------------------------------------------------------------*/

static float scale_value(const HIDJoystickHandle *handle, int index, int value)
{
    PRECOND(index >= 0 && index < lenof(handle->value_info), return 0);
    PRECOND(handle->value_info[index].valid, return 0);

    const float rel_value = value - handle->value_info[index].logical_min;
    const float range = (handle->value_info[index].logical_max
                         - handle->value_info[index].logical_min);
    return (rel_value / range) * 2 - 1;
}

/*-----------------------------------------------------------------------*/

static void update_stick(HIDJoystickHandle *handle, double timestamp,
                         int stick, int is_y, float value)
{
    if (handle->stick_timestamp[stick]
     && handle->stick_timestamp[stick] != timestamp) {
        send_stick_event(handle, handle->stick_timestamp[stick], stick);
    }
    if (is_y) {
        handle->stick_state[stick].y = value;
    } else {
        handle->stick_state[stick].x = value;
    }
    handle->stick_timestamp[stick] = timestamp;
}

/*-----------------------------------------------------------------------*/

static void send_dpad_event(HIDJoystickHandle *handle, double timestamp)
{
    const int x = handle->dpad_state_right - handle->dpad_state_left;
    const int y = handle->dpad_state_down - handle->dpad_state_up;
    (*handle->event_callback)(&(InputEvent){
        .type = INPUT_EVENT_JOYSTICK, .detail = INPUT_JOYSTICK_DPAD_CHANGE,
        .timestamp = timestamp,
        {.joystick = {.device = handle->device_index, .x = x, .y = y}}});
}

/*-----------------------------------------------------------------------*/

static void send_stick_event(HIDJoystickHandle *handle, double timestamp,
                             int stick)
{
    (*handle->event_callback)(&(InputEvent){
        .type = INPUT_EVENT_JOYSTICK,
        .detail = INPUT_JOYSTICK_STICK_CHANGE,
        .timestamp = timestamp,
        {.joystick = {.device = handle->device_index, .index = stick,
                      .x = handle->stick_state[stick].x,
                      .y = handle->stick_state[stick].y}}});
}

/*************************************************************************/
/*************************************************************************/
