/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/input.h: Input device management header.
 */

/*
 * This subsystem currently supports the following input devices:
 *
 * - Joystick (multiple devices supported)
 *
 * - Keyboard (keycodes are defined in <SIL/keycodes.h>)
 *
 * - Mouse (with left/middle/right buttons and scrolling functionality)
 *
 * - Text entry (covering both regular keyboards and software keyboards,
 *   handwriting recognizers, or similar non-physical-keyboard text input
 *   devices)
 *
 * - Touchscreen input (with multitouch support)
 */

#ifndef SIL_INPUT_H
#define SIL_INPUT_H

EXTERN_C_BEGIN

/*************************************************************************/
/*********************** Constants and data types ************************/
/*************************************************************************/

/*-------------------------- Internal limits ----------------------------*/

/**
 * INPUT_RING_BUFFER_SIZE:  Number of events which can be buffered between
 * consecutive calls to input_update().
 */
#ifndef INPUT_RING_BUFFER_SIZE
# define INPUT_RING_BUFFER_SIZE  1023
#endif

/**
 * INPUT_MAX_JOYSTICKS:  Maximum number of joystick devices which the
 * library will support.
 */
#ifndef INPUT_MAX_JOYSTICKS
# define INPUT_MAX_JOYSTICKS  10
#endif

/**
 * INPUT_MAX_JOYSTICK_STICKS:  Maximum number of stick inputs per joystick
 * device which the library will support.
 */
#ifndef INPUT_MAX_JOYSTICK_STICKS
# define INPUT_MAX_JOYSTICK_STICKS  4
#endif

/**
 * INPUT_MAX_JOYSTICK_BUTTONS:  Maximum number of button inputs per
 * joystick device which the library will support.
 */
#ifndef INPUT_MAX_JOYSTICK_BUTTONS
# define INPUT_MAX_JOYSTICK_BUTTONS  64
#endif

/**
 * INPUT_MAX_TOUCHES:  Maximum number of simultaneous touches which the
 * library will support.
 */
#ifndef INPUT_MAX_TOUCHES
# define INPUT_MAX_TOUCHES  32
#endif

/*--------------------------- Joystick input ----------------------------*/

/**
 * INPUT_JOYBUTTON_*:  Names for buttons seen commonly on joysticks,
 * gamepads, or similar devices.  These can be passed to the
 * input_joystick_button_mapping() function to retrieve the corresponding
 * button number (if any) on a specific joystick device.
 */
enum {
    /*
     * "Home" button, typically used to access a system menu.  (Even on
     * controllers with such a button, some systems intercept the button
     * and do not pass it on to the application.)  Examples:
     *    - The "PS" button on Sony PlayStation 3 gamepads.
     *    - The "Home" button (with the Xbox logo) on Microsoft Xbox gamepads.
     */
    INPUT_JOYBUTTON_HOME = 0,

    /*
     * "Start" button, typically used to start or pause a game.
     */
    INPUT_JOYBUTTON_START,

    /*
     * "Select" button, typically used to choose between menu options or
     * for auxiliary actions.  Examples:
     *    - The "Select" button on Sony and Nintendo gamepads.
     *    - The "Back" button on Microsoft Xbox gamepads.
     */
    INPUT_JOYBUTTON_SELECT,

    /*
     * The uppermost of the face buttons on gamepads with four face buttons.
     * Examples:
     *    - The Triangle button on Sony PlayStation gamepads.
     *    - The X button on Nintendo gamepads.
     *    - The Y button on Microsoft Xbox gamepads.
     * (These confusing differences between Nintendo and Microsoft button
     * naming are the reason for using neutral names like "FACE_UP" here.)
     */
    INPUT_JOYBUTTON_FACE_UP,

    /*
     * The leftmost of the face buttons on gamepads with four face buttons.
     * Examples:
     *    - The Square button on Sony PlayStation gamepads.
     *    - The Y button on Nintendo gamepads.
     *    - The X button on Microsoft Xbox gamepads.
     */
    INPUT_JOYBUTTON_FACE_LEFT,

    /*
     * The rightmost of the face buttons on gamepads with four face buttons.
     * Examples:
     *    - The Circle button on Sony PlayStation gamepads.
     *    - The A button on Nintendo gamepads.
     *    - The B button on Microsoft Xbox gamepads.
     */
    INPUT_JOYBUTTON_FACE_RIGHT,

    /*
     * The lowermost of the face buttons on gamepads with four face buttons.
     * Examples:
     *    - The Cross button on Sony PlayStation gamepads.
     *    - The B button on Nintendo gamepads.
     *    - The A button on Microsoft Xbox gamepads.
     */
    INPUT_JOYBUTTON_FACE_DOWN,

    /*
     * The first (or only) left and right shoulder buttons.  Examples:
     *    - The L1/R1 buttons on Sony PlayStation gamepads.
     *    - The L/R buttons on Nintendo gamepads.
     *    - The LB/RB buttons on Microsoft Xbox gamepads.
     */
    INPUT_JOYBUTTON_L1,
    INPUT_JOYBUTTON_R1,

    /*
     * The second left and right shoulder buttons.  Examples:
     *    - The L2/R2 buttons on Sony PlayStation gamepads.
     *    - The ZL/ZR buttons (triggers) on Nintendo gamepads.
     *    - The LT/RT buttons (triggers) on Microsoft Xbox gamepads.
     */
    INPUT_JOYBUTTON_L2,
    INPUT_JOYBUTTON_R2,

    /*
     * The left and right analog stick buttons, activated by pressing down
     * (into the controller) on the respective analog sticks.
     */
    INPUT_JOYBUTTON_L_STICK,
    INPUT_JOYBUTTON_R_STICK,

    INPUT_JOYBUTTON__NUM  // List terminator -- not a valid value.
};

/*--------------------------- Event handling ----------------------------*/

/**
 * InputEventType:  Types of input events.  Each type corresponds to
 * exactly one ...InputEvent type and vice-versa.
 */
typedef enum InputEventType {
    INPUT_EVENT_JOYSTICK = 1,   // JoystickInputEvent
    INPUT_EVENT_KEYBOARD,       // KeyboardInputEvent
    INPUT_EVENT_MEMORY,         // MemoryInputEvent
    INPUT_EVENT_MOUSE,          // MouseInputEvent
    INPUT_EVENT_TEXT,           // TextInputEvent
    INPUT_EVENT_TOUCH,          // TouchInputEvent
} InputEventType;

/**
 * InputEventDetail:  Detailed event type information for input events.
 */
typedef enum InputEventDetail {
    /* Joystick events.  A CONNECTED event will be generated for each
     * joystick which is already connected when the program starts up. */
    INPUT_JOYSTICK_CONNECTED = 1,   // The joystick was just connected.
    INPUT_JOYSTICK_DISCONNECTED,    // The joystick was just disconnected.
    INPUT_JOYSTICK_BUTTON_DOWN,     // A button was pressed.
    INPUT_JOYSTICK_BUTTON_UP,       // A button was released.
    INPUT_JOYSTICK_DPAD_CHANGE,     // The D-pad's X or Y input value changed.
    INPUT_JOYSTICK_STICK_CHANGE,    // A stick's X or Y input value changed.

    /* Keyboard events.  The SYSTEM_KEY events are used for keys which SIL
     * does not recognize. */
    INPUT_KEYBOARD_KEY_DOWN,        // A key was pressed.
    INPUT_KEYBOARD_KEY_UP,          // A key was released.
    INPUT_KEYBOARD_SYSTEM_KEY_DOWN, // A system-specific key was pressed.
    INPUT_KEYBOARD_SYSTEM_KEY_UP,   // A system-specific key was released.

    /* Memory pressure events.  Only generated on Android and iOS, and
     * only available through the event interface. */
    INPUT_MEMORY_LOW,           // The system is running low on memory.

    /* Mouse events. */
    INPUT_MOUSE_MOVE,           // The mouse moved.
    INPUT_MOUSE_LMB_DOWN,       // The left mouse button was pressed.
    INPUT_MOUSE_LMB_UP,         // The left mouse button was released.
    INPUT_MOUSE_MMB_DOWN,       // The middle mouse button was pressed.
    INPUT_MOUSE_MMB_UP,         // The middle mouse button was released.
    INPUT_MOUSE_RMB_DOWN,       // The right mouse button was pressed.
    INPUT_MOUSE_RMB_UP,         // The right mouse button was released.
    INPUT_MOUSE_SCROLL_H,       // The user scrolled horizontally.
    INPUT_MOUSE_SCROLL_V,       // The user scrolled vertically.

    /* Text input events. */
    INPUT_TEXT_INPUT,           // A character of input was received.
    INPUT_TEXT_DONE,            // The user signalled completion.
    INPUT_TEXT_CANCELLED,       // The user cancelled text entry, or an error
                                //     occurred.
    INPUT_TEXT_CLEAR,           // User input: clear the current input string.
    INPUT_TEXT_BACKSPACE,       // User input: delete the preceding character.
    INPUT_TEXT_DELETE,          // User input: delete the following character.
    INPUT_TEXT_CURSOR_LEFT,     // User input: move one character to the left.
    INPUT_TEXT_CURSOR_RIGHT,    // User input: move one character to the right.
    INPUT_TEXT_CURSOR_HOME,     // User input: move to the start of the text.
    INPUT_TEXT_CURSOR_END,      // User input: move to the end of the text.

    /* Touch events. */
    INPUT_TOUCH_DOWN,           // A new touch has started.
    INPUT_TOUCH_MOVE,           // A touch moved to a new position.
    INPUT_TOUCH_UP,             // A touch ended (finger lifted, for example).
    INPUT_TOUCH_CANCEL,         // An in-progress touch was cancelled by the
                                //    system (because the program was
                                //    suspended, for example), and no action
                                //    should be taken for the touch.
} InputEventDetail;

/**
 * JoystickInputEvent:  Input event structure for joystick events.
 */
typedef struct JoystickInputEvent JoystickInputEvent;
struct JoystickInputEvent {
    /* Joystick device index. */
    int device;
    /* Stick or button index.  Invalid for DPAD_CHANGE events. */
    int index;
    /* X and Y input values, between -1 (left/up) and +1 (right/down)
     * inclusive.  For DPAD_CHANGE events, these only take integral values
     * (-1, 0, or +1).  These fields are invalid for BUTTON_DOWN and
     * BUTTON_UP events. */
    float x, y;
};

/**
 * KeyboardInputEvent:  Input event structure for keyboard events.
 */
typedef struct KeyboardInputEvent KeyboardInputEvent;
struct KeyboardInputEvent {
    /* Key code (KEY_* constant) for this event.  For SYSTEM_KEY events,
     * this will be zero (a.k.a. KEY__NONE).  See <SIL/keycodes.h> for
     * key code definitions. */
    int key;
    /* System-specific key code for this event.  This is set for all key
     * events; the meaning of the value is (of course) system-dependent. */
    int system_key;
    /* Modifiers pressed when this event occurred (bitwise OR of zero or
     * more KEYMOD_* constants). */
    unsigned int modifiers;
    /* True (nonzero) if this is a key-down event generated by system-level
     * key repeat functionality; false (zero) otherwise.  Always false for
     * key-up events. */
    uint8_t is_repeat;
};

/**
 * MemoryInputEvent:  Input event struture for memory pressure events.
 */
typedef struct MemoryInputEvent MemoryInputEvent;
struct MemoryInputEvent {
    /* Estimated amount of memory in use by the program (nonnegative). */
    int64_t used_bytes;
    /* Estimated amount of memory available to be allocated (nonnegative). */
    int64_t free_bytes;
};

/**
 * MouseInputEvent:  Input event structure for mouse events.
 */
typedef struct MouseInputEvent MouseInputEvent;
struct MouseInputEvent {
    /* Position of the mouse at the time of this event.  Both
     * coordinates are in the range 0 to 1, as for values returned by
     * input_mouse_get_position(). */
    float x, y;
    /* Scroll distance (negative for up/left scroll, positive for
     * down/right scroll).  A scroll of 1.0 is equivalent to scrolling by
     * one notch on a notched scroll wheel.  Valid only for SCROLL_H and
     * SCROLL_V events. */
    float scroll;
};

/**
 * TextInputEvent:  Input event structure for text input events.
 */
typedef struct TextInputEvent TextInputEvent;
struct TextInputEvent {
    /* The Unicode codepoint of a character which was input.  Only defined
     * for INPUT_TEXT_INPUT events. */
    int32_t ch;
};

/**
 * TouchInputEvent:  Input event structure for touch events.
 */
typedef struct TouchInputEvent TouchInputEvent;
struct TouchInputEvent {
    /* Touch ID, as used with functions such as input_touch_get_position(). */
    unsigned int id;
    /* Position of the touch at the time of this event.  Both
     * coordinates are in the range 0 to 1, as for values returned by
     * input_touch_get_position(). */
    float x, y;
    /* Position of the touch when it was first detected.  For INPUT_TOUCH_DOWN
     * events, these will be equal to the current touch coordinates above. */
    float initial_x, initial_y;
};

/**
 * InputEvent:  Generic input event structure.  Of the type-specific
 * structures, only the structure corresponding to the type code in
 * InputEvent.type is valid for any given event.
 */
typedef struct InputEvent InputEvent;
struct InputEvent {
    /* Type of this event, indicating which type-specific structure is
     * valid. */
    InputEventType type;
    /* Detail code for this event. */
    InputEventDetail detail;
    /* Timestamp of this event, comparable to values returned from time_now().
     * Currently, this is always the time at which input_update() is called,
     * but this may change in the future. */
    double timestamp;
    /* Type-specific data. */
    union {
        JoystickInputEvent joystick;
        KeyboardInputEvent keyboard;
        MemoryInputEvent memory;
        MouseInputEvent mouse;
        TextInputEvent text;
        TouchInputEvent touch;
    };
};

/**
 * InputEventCallback:  Function type for the event handling callback
 * passed to input_set_event_callback().
 *
 * [Parameters]
 *     event: Event structure (guaranteed to be non-NULL).
 */
typedef void (*InputEventCallback)(const InputEvent *event);

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

/*------------------------- Base functionality --------------------------*/

/**
 * input_update:  Obtain the current state of all input devices.  This
 * function must be called periodically to refresh input device state;
 * on some systems, input device state is only read during this call.
 */
extern void input_update(void);

/**
 * input_set_event_callback:  Set a function to be called whenever a change
 * in the state of an input device is detected.  If set, the callback will
 * be called from input_update() for each state change detected during that
 * call, and calls from the callback to input_*() functions which return
 * current input state will reflect the state change described by the event
 * passed to the callback.
 *
 * Be aware that some types of input can generate multiple events.  For
 * example, typing on a PC keyboard while text input is active will
 * generate both keyboard and text events, and D-pad input from a joystick
 * may generate any or all of D-pad, button, and stick events.
 *
 * [Parameters]
 *     callback: Function to call, or NULL to disable the callback.
 */
extern void input_set_event_callback(InputEventCallback callback);

/**
 * input_enable_coalescing():  Control whether consecutive movement-type
 * input events for the same input device are coalesced (merged) into a
 * single event in the queue.  Coalescing reduces the risk of input queue
 * overflow with noisy input devices such as sensitive mice, at the cost
 * of losing information about the path taken by the input device to the
 * most recent point.  Coalescing also increases event processing overhead
 * due to the additional cross-thread synchronization required.
 *
 * Note that the risk of input queue overflow can also be reduced by
 * defining INPUT_RING_BUFFER_SIZE to a larger value than the default
 * (see #define above).
 *
 * The following events are coalesced:
 *    - type = INPUT_EVENT_JOYSTICK, detail = INPUT_JOYSTICK_STICK_CHANGE
 *    - type = INPUT_EVENT_MOUSE, detail = INPUT_MOUSE_MOVE
 *    - type = INPUT_EVENT_TOUCH, detail = INPUT_TOUCH_MOVE
 *
 * Coalescing is disabled by default.
 *
 * [Parameters]
 *     enable: True to enable coalescing, false to disable.
 */
extern void input_enable_coalescing(int enable);

/**
 * input_grab:  Activate or deactivate input grabbing.  When input is
 * grabbed, the mouse pointer will be confined to the display window while
 * it has focus.  The user can still switch to another program using
 * keyboard shortcuts (such as Alt-Tab on Windows or Command-Tab on Mac).
 *
 * This function has no effect in non-PC environments.
 *
 * [Parameters]
 *     grab: True to grab input, false to ungrab input.
 */
extern void input_grab(int grab);

/**
 * input_is_quit_requested:  Return whether a system-specific quit request
 * (such as a window close event) has been received.
 *
 * [Return value]
 *     True if a quit request has been received, false if not.
 */
extern int input_is_quit_requested(void);

/**
 * input_is_suspend_requested:  Return whether a system-specific suspend
 * request has been received.  Upon detecting a suspend request, the
 * caller should immediately take any actions necessary to preserve state
 * and then call input_acknowledge_suspend_request(), which will block
 * until the system resumes from suspend (if it returns at all; see the
 * documentation for that function).
 *
 * It is safe on all supported systems to call any SIL functions,
 * including graphics rendering, after receiving a true return value from
 * this function.  However, be aware that most systems only allow a few
 * seconds for the program to acknowledge a suspend request before they
 * forcibly terminate the program, so the caller should perform any
 * necessary pre-suspend actions (such as saving state data) as quickly as
 * possible, and in any case should not wait for user interaction.
 *
 * [Return value]
 *     True if a suspend request has been received, false if not.
 */
extern int input_is_suspend_requested(void);

/**
 * input_acknowledge_suspend_request:  Acknowledge a suspend request from
 * the system.  If a suspend request is pending, this function allows the
 * suspend to proceed, and it does not return until the system wakes the
 * program from its suspended state.  Otherwise, this function does nothing.
 *
 * Note that there is no guarantee this function will return at all: the
 * OS may terminate the process while it is suspended, or the user may
 * shut down the system.  Any data which is auto-saved on quit, for
 * example, should also be auto-saved before calling this function.
 */
extern void input_acknowledge_suspend_request(void);

/*-------------------------- Joystick handling --------------------------*/

/**
 * input_enable_unfocused_joystick:  Set whether or not to send joystick
 * input events while the application does not have input focus.  The
 * default is true (input events will be sent).
 *
 * Note that joystick connect/disconnect events will always be sent
 * as soon as they are detected, regardless of focus state.
 *
 * On platforms which do not have the concept of input focus, calling this
 * function has no effect.
 *
 * [Parameters]
 *     enable: True to send joystick input events while the window does
 *         not have focus, false to suppress them.
 */
extern void input_enable_unfocused_joystick(int enable);

/**
 * input_joystick_present:  Return whether at least one joystick device is
 * available.
 *
 * [Return value]
 *     True if a joystick device is available, false if not.
 */
extern int input_joystick_present(void);

/**
 * input_joystick_count:  Return the number of joystick devices available.
 * This number may include "ghost" devices which have been removed from the
 * system since the program started.
 *
 * [Return value]
 *     Number of joystick devices available.
 */
extern int input_joystick_count(void);

/**
 * input_joystick_connected:  Return whether the given joystick is
 * currently connected to the system.
 *
 * [Parameters]
 *     index: Joystick index.
 * [Return value]
 *     True if the given joystick is connected, false if not.
 */
extern int input_joystick_connected(int index);

/**
 * input_joystick_copy_name:  Return the name of the given joystick, if
 * available, in a newly allocated buffer.  On PC-class systems, this is
 * typically the name string reported by the particular device in use (or
 * its driver).
 *
 * The caller should free the returned string with mem_free() when it is
 * no longer needed.
 *
 * [Parameters]
 *     index: Joystick index.
 * [Return value]
 *     Name of the joystick (in a newly allocated buffer), or NULL if the
 *     joystick's name is unavailable or the index is invalid.
 */
extern char *input_joystick_copy_name(int index);

/**
 * input_joystick_num_buttons:  Return the number of buttons available on the
 * given joystick, or zero if "index" does not represent a connected joystick.
 *
 * For all input_joystick_*() functions which take an index parameter, the
 * value passed should be in the range 0 through input_joystick_count()-1,
 * where 0 represents the "first" joystick (the meaning of "first" is system
 * dependent).  As long as a particular joystick device remains connected,
 * its index will not change, even if a joystick with a lower index is
 * removed.
 *
 * [Parameters]
 *     index: Joystick index.
 * [Return value]
 *     Number of joystick buttons available.
 */
extern int input_joystick_num_buttons(int index);

/**
 * input_joystick_button_mapping:  Return the joystick button, if any,
 * which corresponds to the given button name.  If "index" does not
 * represent a connected joystick, or if the configuration of the joystick
 * is unknown, this function returns -1 ("no such button") for all button
 * names.
 *
 * [Parameters]
 *     index: Joystick index.
 *     name: Button name (INPUT_JOYBUTTON_*).
 * [Return value]
 *     Corresponding button number, or -1 if there is no corresponding button.
 */
extern int input_joystick_button_mapping(int index, int name);

/**
 * input_joystick_num_sticks:  Return the number of sticks available on the
 * given joystick, or zero if "index" does not represent a connected joystick.
 *
 * [Parameters]
 *     index: Joystick index.
 * [Return value]
 *     Number of joystick sticks available.
 */
extern int input_joystick_num_sticks(int index);

/**
 * input_joystick_can_rumble:  Return whether the given joystick supports
 * rumble (force feedback) effects, or zero if "index" does not represent a
 * connected joystick.
 *
 * [Parameters]
 *     index: Joystick index.
 * [Return value]
 *     True if the given joystick supports force feedback, false if not.
 */
extern int input_joystick_can_rumble(int index);

/**
 * input_joystick_button_state:  Return the state of the given joystick
 * button. Buttons outside the valid range for the joystick are treated as
 * not pressed.  If "index" does not represent a connected joystick, all
 * buttons are treated as not pressed.
 *
 * [Parameters]
 *     index: Joystick index.
 *     button: Button to check (0, 1, ...).
 * [Return value]
 *     True if the button is pressed, false if not.
 */
extern int input_joystick_button_state(int index, int button);

/**
 * input_joystick_read_stick:  Return the X and Y positions of the given
 * joystick stick.  The returned values are -1 for full left/up, +1 for
 * full right/down.  If "index" does not represent a connected joystick or
 * given stick index is out of range, zero is returned for both positions.
 *
 * [Parameters]
 *     index: Joystick index.
 *     stick: Stick index (0, 1, ...).
 *     x_ret: Pointer to variable to receive X position (NULL if not needed).
 *     y_ret: Pointer to variable to receive Y position (NULL if not needed).
 */
extern void input_joystick_read_stick(int index, int stick, float *x_ret,
                                      float *y_ret);

/**
 * input_joystick_read_dpad:  Return the X and Y positions of the directional
 * pad, if any, on the given joystick.  The returned values are -1 for
 * left/up, +1 for right/down.  If "index" does not repesent a connected
 * joystick or the joystick does not have a directional pad or similar
 * input, zero is returned for both positions.
 *
 * [Parameters]
 *     index: Joystick index.
 *     x_ret: Pointer to variable to receive X position (NULL if not needed).
 *     y_ret: Pointer to variable to receive Y position (NULL if not needed).
 */
extern void input_joystick_read_dpad(int index, int *x_ret, int *y_ret);

/**
 * input_joystick_rumble:  Send a "rumble" (force feedback) effect to the
 * given joystick.  If a previous rumble effect is already in progress, it
 * is cancelled. Does nothing if "index" does not represent a connected
 * joystick or the joystick does not support force feedback.
 *
 * Extremely long durations (>35 minutes) may cause incorrect behavior on
 * some systems.
 *
 * [Parameters]
 *     index: Joystick index.
 *     left: Left motor intensity (0 = off, 1 = full intensity).
 *     right: Right motor intensity (0 = off, 1 = full intensity).
 *     time: Duration of effect, in seconds.
 */
extern void input_joystick_rumble(int index, float left, float right,
                                  float time);

/*-------------------------- Keyboard handling --------------------------*/

/**
 * input_keyboard_present:  Return whether a keyboard device is available.
 *
 * [Return value]
 *     True if a keyboard device is available, false if not.
 */
extern int input_keyboard_present(void);

/**
 * input_keyboard_is_full:  Return whether a "full" keyboard device (one
 * capable of general text input, as opposed to a numeric keypad or similar
 * device) is available.
 *
 * [Return value]
 *     True if a full keyboard device is available, false if not.
 */
extern int input_keyboard_is_full(void);

/**
 * input_key_state:  Return the state of the given key, or false if no
 * keyboard is available.
 *
 * [Parameters]
 *     key: Key to check (KEY_* constant).
 * [Return value]
 *     True if the key is pressed, false if not.
 */
extern int input_key_state(int key);

/**
 * input_key_modifier_state:  Return the state of the the modifier keys
 * (Shift, Control, etc.), or 0 if no keyboard is available.
 *
 * [Return value]
 *     Bitwise OR of zero or more KEYMOD_* constants indicating which
 *     modifier keys are currently pressed.
 */
extern int input_key_modifier_state(void);

/**
 * input_key_last_pressed:  Return the ID of the last key pressed between
 * the last two calls to input_update(), or zero if no key was pressed (or
 * no keyboard is available).
 *
 * [Return value]
 *     KEY_* constant representing last key pressed (nonzero), or zero if none.
 */
extern int input_key_last_pressed(void);

/*--------------------------- Mouse handling ----------------------------*/

/**
 * input_mouse_present:  Return whether a mouse or similar pointing device
 * is available.
 *
 * [Return value]
 *     True if a mouse device is available, false if not.
 */
extern int input_mouse_present(void);

/**
 * input_mouse_get_position:  Return the current mouse position.  The
 * position is returned as floating point values in the range 0 to 1,
 * where (0,0) is the top left corner of the display and (1,1) is just
 * outside the bottom right corner; thus coordinates are always strictly
 * less than 1.0.  If no mouse is available, both values are set to 0.5
 * (the center of the display).
 *
 * [Parameters]
 *     x_ret: Pointer to variable to receive X position (NULL if not needed).
 *     y_ret: Pointer to variable to receive Y position (NULL if not needed).
 */
extern void input_mouse_get_position(float *x_ret, float *y_ret);

/**
 * input_mouse_left_button_state, input_mouse_middle_button_state,
 * input_mouse_right_button_state:  Return the state of the left, middle,
 * or right mouse button, respectively.  If no mouse is available, all
 * buttons are treated as not pressed.
 *
 * [Return value]
 *     True if the button is pressed, false if not.
 */
extern int input_mouse_left_button_state(void);
extern int input_mouse_middle_button_state(void);
extern int input_mouse_right_button_state(void);

/**
 * input_mouse_horiz_scroll, input_mouse_vert_scroll:  Return the number of
 * units of scroll between the last two calls to input_update().  Negative
 * values indicate scrolling left or up; positive values indicate scrolling
 * right or down.  A unit of scroll is typically one step of the scroll
 * wheel or one click of a scroll button, but the exact definition is
 * system-dependent.  If no mouse is available, these functions always
 * return zero.
 *
 * [Return value]
 *     Horizontal or vertical scroll distance.
 */
extern int input_mouse_horiz_scroll(void);
extern int input_mouse_vert_scroll(void);

/**
 * input_mouse_set_position:  Set the position of the mouse pointer, if a
 * mouse is available and the system supports pointer repositioning.  The
 * x and y parameters are interpreted as for input_mouse_get_position(),
 * except that a value of exactly 1.0 is allowed and is interpreted to mean
 * the right or bottom edge of the screen.
 *
 * If the pointer position is changed, input_mouse_get_position() will
 * return the new position immediately, even without an input_update()
 * call.  If the pointer position cannot be changed (due to operating
 * system restrictions, for example), input_mouse_get_position() will
 * behave as it would if this function had not been called.
 *
 * If an event callback has been established, this function will generate
 * a MOUSE_MOVE event if it successfully sets the mouse pointer position.
 *
 * [Parameters]
 *     x, y: New mouse pointer position.
 */
extern void input_mouse_set_position(float x, float y);

/*------------------------- Text entry handling -------------------------*/

/**
 * input_text_present:  Return whether a text entry interface (such as a
 * real or virtual keyboard) is available.
 *
 * [Return value]
 *     True if a text entry interface is available, false if not.
 */
extern int input_text_present(void);

/**
 * input_text_uses_custom_interface:  Return whether the text entry
 * functionality displays its own input interface on the screen.  If no
 * text entry interface is available, this function returns false.
 *
 * [Return value]
 *     True if a text entry interface is available and it displays a custom
 *     input interface on the screen, false otherwise.
 */
extern int input_text_uses_custom_interface(void);

/**
 * input_text_can_display_prompt:  Return whether the custom input
 * interface displayed for text entry can show a user-defined prompt
 * string.  If no text entry interface is available or it does not use a
 * custom input interface, this function returns false.
 *
 * [Return value]
 *     True if a text entry interface is available, it displays a custom
 *     input interface on the screen, and the custom interface can display
 *     a user-defined prompt string; false otherwise.
 */
extern int input_text_can_display_prompt(void);

/**
 * input_text_enable:  Enable text input events.  If the system uses a
 * custom interface for text input, this displays the text input interface.
 */
extern void input_text_enable(void);

/**
 * input_text_enable_with_default_text:  Enable text input events, and if
 * the system uses a custom interface for text input, provide the given
 * string as the default string in the interface.
 *
 * The passed-in string will be copied by SIL, so the caller's copy does
 * not need to persist after the function returns.
 *
 * [Parameters]
 *     text: Default text (UTF-8 encoded).
 */
extern void input_text_enable_with_default_text(const char *text);

/**
 * input_text_enable_with_prompt:  Enable text input events, and if the
 * system uses a custom interface for text input, provide default text and
 * show the given prompt string in the interface.
 *
 * The passed-in strings will be copied by SIL, so the caller's copies do
 * not need to persist after the function returns.
 *
 * [Parameters]
 *     text: Default text (UTF-8 encoded).
 *     prompt: Prompt string (UTF-8 encoded).
 */
extern void input_text_enable_with_prompt(const char *text, const char *prompt);

/**
 * input_text_disable:  Disable text input events, and if the system uses
 * a custom interface for text input, hide that interface.
 */
extern void input_text_disable(void);

/**
 * input_text_get_char:  Return the next pending text entry character or
 * event in the sequence received, or zero if no input is pending.
 * Characters are returned as their Unicode codepoint; events are returned
 * as the negative (arithmetic inverse) of the appropriate INPUT_TEXT_*
 * constant.
 *
 * If called while text input is not enabled, this function will act as
 * though an INPUT_TEXT_CANCELLED event is always pending.
 *
 * Note that this function has an internal buffer of approximately 1000
 * characters or event codes.  Particularly on systems which use a
 * custom interface, this implies a limit of the same size on the
 * returned string.  To read strings of unlimited length, use the event
 * interface instead.
 *
 * [Return value]
 *     Next character or event code, or zero if no text input is pending.
 */
extern int input_text_get_char(void);

/*--------------------------- Touch handling ----------------------------*/

/**
 * input_touch_present:  Return whether a touch interface is available.
 *
 * [Return value]
 *     True if a touch interface is available, false if not.
 */
extern int input_touch_present(void);

/**
 * input_touch_num_touches:  Return the number of active touches.  This is
 * always zero if no touch interface is available.
 *
 * [Return value]
 *     Number of active touches.
 */
extern int input_touch_num_touches(void);

/**
 * input_touch_id_for_index:  Return the unique touch ID for the given
 * touch index, or zero if the index is out of range (or no touch interface
 * is available).  Touch IDs are always nonzero and are guaranteed not to
 * be reused for a period of 2^32-1 touches.
 *
 * [Parameters]
 *     index: Touch index (0 <= index < input_touch_num_touches()).
 * [Return value]
 *     Touch ID (nonzero).
 */
extern unsigned int input_touch_id_for_index(int index);

/**
 * input_touch_active:  Return whether the given touch ID represents an
 * active touch.  If no touch interface is available, this function always
 * returns false.
 *
 * [Parameters]
 *     id: Touch ID.
 * [Return value]
 *     True if the ID represents an active touch, false if not.
 */
extern int input_touch_active(unsigned int id);

/**
 * input_touch_get_position:  Return the position of the given touch.  As
 * with input_mouse_get_position(), the position is returned as floating
 * point values in the range 0 through 1, where (0,0) is the top left
 * corner of the display and (1,1) is the bottom right corner.  If the
 * given ID does not represent an active touch, both values are set to 0.5
 * (the center of the display).
 *
 * [Parameters]
 *     id: Touch ID.
 *     x_ret: Pointer to variable to receive X position (NULL if not needed).
 *     y_ret: Pointer to variable to receive Y position (NULL if not needed).
 */
extern void input_touch_get_position(unsigned int id, float *x_ret,
                                     float *y_ret);

/**
 * input_touch_get_initial_position:  Return the initial position of the
 * given touch (the position where the touch began).  The coordinates are
 * returned as for input_touch_get_position().
 *
 * [Parameters]
 *     id: Touch ID.
 *     x_ret: Pointer to variable to receive X position (NULL if not needed).
 *     y_ret: Pointer to variable to receive Y position (NULL if not needed).
 */
extern void input_touch_get_initial_position(unsigned int id, float *x_ret,
                                             float *y_ret);

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_INPUT_H
