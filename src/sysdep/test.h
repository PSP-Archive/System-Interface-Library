/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/test.h: Test control interface for system-specific functionality.
 */

/*
 * This header declares routines specific to the testing implementation of
 * the sys_*() functions (located under sysdep/test), which can be used to
 * simulate input or check simulated output when running tests.  These
 * routines have no effect when not running tests.
 */

#ifndef SIL_SRC_SYSDEP_TEST_H
#define SIL_SRC_SYSDEP_TEST_H

#ifdef SIL_INCLUDE_TESTS  // To the end of the file.

struct DateTime;
struct InputEvent;
enum InputEventDetail;

/*************************************************************************/
/*************************************************************************/

/*----------------------- Debugging functionality -----------------------*/

/**
 * sys_test_debug_set_memory_stats:  Set the values to be returned by the
 * sys_debug_get_memory_stats() function.
 *
 * [Parameters]
 *     total: Value to return in *total_ret.
 *     self: Value to return in *self_ret.
 *     avail: Value to return in *avail_ret.
 */
extern void sys_test_debug_set_memory_stats(
    int64_t total, int64_t self, int64_t avail);

/**
 * sys_test_debug_fail_memory_stats:  Force the next call to
 * sys_debug_get_memory_stats() to fail.
 */
extern void sys_test_debug_fail_memory_stats(void);

/*------------------------ Input device handling ------------------------*/

/**
 * sys_test_input_fail_init:  Force the next call to sys_input_init() to
 * fail.
 */
extern void sys_test_input_fail_init(void);

/**
 * sys_test_input_get_unfocused_joystick_state:  Return whether unfocused
 * joystick input is enabled.
 *
 * [Return value]
 *     True if unfocused joystick input is enabled, false if not.
 */
extern int sys_test_input_get_unfocused_joystick_state(void);

/**
 * sys_test_input_get_grab_state:  Return whether input has been grabbed.
 *
 * [Return value]
 *     True if input has been grabbed, false if not.
 */
extern int sys_test_input_get_grab_state(void);

/**
 * sys_test_input_send_quit_request, sys_test_input_send_suspend_request:
 * Simulate a system quit or suspend request to the program.
 */
extern void sys_test_input_send_quit_request(void);
extern void sys_test_input_send_suspend_request(void);

/**
 * sys_test_input_enable_*:  Enable or disable specific input types or
 * features.  All types and features default to enabled.
 *
 * [Parameters]
 *     on: True to enable the input type or feature, false to disable it.
 */
extern void sys_test_input_enable_joystick(int on);
extern void sys_test_input_enable_joystick_rumble(int on);
extern void sys_test_input_enable_keyboard(int on);
extern void sys_test_input_enable_keyboard_full(int on);
extern void sys_test_input_enable_mouse(int on);
extern void sys_test_input_enable_text(int on);
extern void sys_test_input_enable_text_custom_interface(int on);
extern void sys_test_input_enable_text_prompt(int on);
extern void sys_test_input_enable_touch(int on);

/**
 * sys_test_input_set_joy_num_devices:  Set the number of joystick devices
 * reported via sys_input_info().  The default is 1.
 *
 * Note that if the number of devices changes, CONNECTED or DISCONNECTED
 * events are sent regardless of whether joystick input is enabled.
 *
 * [Parameters]
 *     num: Number of devices to report (0 through INPUT_MAX_JOYSTICKS+1).
 */
extern void sys_test_input_set_joy_num_devices(int num);

/**
 * sys_test_input_set_joy_connected:  Set whether joystick devices should
 * be reported as connected via sys_input_info().  The default is to report
 * joystick devices as connected.
 *
 * Note that if the connection state changes, CONNECTED or DISCONNECTED
 * events are sent regardless of whether joystick input is enabled.
 *
 * [Parameters]
 *     connected: True to report joystick devices as connected, false to
 *         report them as disconnected.
 */
extern void sys_test_input_set_joy_connected(int connected);

/**
 * sys_test_input_set_joy_connected_on_init:  Set whether joystick devices
 * should be initialized to connected or disconnected state when
 * sys_input_init() is called.  The default is for joystick devices to
 * start out disconnected.
 *
 * [Parameters]
 *     connected: True to initialize joystick devices as connected, false
 *         to initialize them as disconnected.
 */
extern void sys_test_input_set_joy_connected_on_init(int connected);

/**
 * sys_test_input_set_joy_num_buttons:  Set the number of buttons per joystick
 * device reported via sys_input_info().  The default is 20.
 *
 * [Parameters]
 *     num: Number of buttons to report (0 through
 *         INPUT_MAX_JOYSTICK_BUTTONS+1).
 */
extern void sys_test_input_set_joy_num_buttons(int num);

/**
 * sys_test_input_set_joy_button_mapping:  Set the mapping between joystick
 * button names and button numbers.  By default, all names are unmapped.
 *
 * [Parameters]
 *     name: Button name (INPUT_JOYBUTTON_*).
 *     num: Joystick button number to map to name, or -1 to unmap.
 */
extern void sys_test_input_set_joy_button_mapping(int name, int num);

/**
 * sys_test_input_set_joy_num_sticks:  Set the number of sticks per joystick
 * device reported via sys_input_info().  The default is 2.
 *
 * [Parameters]
 *     num: Number of sticks to report (0 through INPUT_MAX_JOYSTICK_STICKS+1).
 */
extern void sys_test_input_set_joy_num_sticks(int num);

/**
 * sys_test_input_set_joy_stick:  Set the simulated position of the given
 * joystick stick.  Note that invalid (but only positive) device and
 * stick index values may be passed to exercise relevant code paths.
 *
 * [Parameters]
 *     device: Device index (the only valid device is 0).
 *     index: Stick index (valid sticks are 0 or 1).
 *     x, y: Stick position, in [-1,+1].
 */
extern void sys_test_input_set_joy_stick(int device, int index, float x, float y);

/**
 * sys_test_input_set_joy_dpad:  Set the simulated joystick directional pad
 * inputs.  Note that invalid (but only positive) device index values may
 * be passed to exercise relevant code paths.
 *
 * [Parameters]
 *     device: Device index (the only valid device is 0).
 *     x: Horizontal input (-1 = left, +1 = right).
 *     y: Vertical input (-1 = up, +1 = down).
 */
extern void sys_test_input_set_joy_dpad(int device, int x, int y);

/**
 * sys_test_input_set_joy_button:  Set the simulated state of the given
 * joystick button.  Note that invalid (but only positive) device and
 * button index values may be passed to exercise relevant code paths.
 *
 * [Parameters]
 *     device: Device index (the only valid device is 0).
 *     index: Button index (valid buttons are 0-19).
 *     pressed: True to put the button in the pressed state, false to put it
 *         in the released state.
 */
extern void sys_test_input_set_joy_button(int device, int index, int pressed);

/**
 * sys_test_input_get_rumble_left, sys_test_input_get_rumble_right,
 * sys_test_input_get_rumble_time:  Return the parameters for the most
 * recent joystick rumble operation.  Note that time is not tracked for
 * rumble effects; the values set by a rumble call will remain active
 * until the next rumble call.
 *
 * [Return value]
 *     Requested rumble parameter.
 */
extern float sys_test_input_get_rumble_left(void);
extern float sys_test_input_get_rumble_right(void);
extern float sys_test_input_get_rumble_time(void);

/**
 * sys_test_input_press_key, sys_test_input_release_key:  Simulate the
 * press or release of a keyboard key.  These functions do nothing if the
 * key is already in the requested state.
 *
 * If key is KEY_NONE, these functions send a SYSTEM_KEY event instead of
 * a regular KEY event.
 *
 * [Parameters]
 *     key: Key code of key to press or release (KEY_*).
 *     system_key: Value to store in event's system_key field.
 */
extern void sys_test_input_press_key(int key, int system_key);
extern void sys_test_input_release_key(int key, int system_key);

/**
 * sys_test_input_send_memory_low:  Simulate a memory pressure event.
 *
 * [Parameters]
 *     used_bytes: Value to report in the event's used_bytes field.
 *     free_bytes: Value to report in the event's free_bytes field.
 */
extern void sys_test_input_send_memory_low(int64_t used_bytes,
                                           int64_t free_bytes);

/**
 * sys_test_input_move_mouse:  Move the simulated position of the mouse
 * pointer by the given amount.  The resulting position is clamped to
 * [0,65535/65536].
 *
 * Absolute mouse position should be done through the standard
 * sys_input_mouse_set_position() interface, which will likewise clamp
 * coordinates to [0,65535/65536].
 *
 * [Parameters]
 *     dx, dy: Amounts to add to current mouse coordinates.
 */
extern void sys_test_input_move_mouse(float dx, float dy);

/**
 * sys_test_input_press_mouse_buttons, sys_test_input_release_mouse_buttons:
 * Simulate the press or release of one or more mouse buttons.  Any buttons
 * which are already in the requested state remain unchanged.
 *
 * [Parameters]
 *     left, middle, right: True to simulate a press or release of the
 *         relevant button, false to leave the button's state alone.
 */
extern void sys_test_input_press_mouse_buttons(int left, int middle,
                                               int right);
extern void sys_test_input_release_mouse_buttons(int left, int middle,
                                                 int right);

/**
 * sys_test_input_add_mouse_scroll:  Simulate horizontal and/or vertical
 * mouse scroll events.
 *
 * [Parameters]
 *     dh: Horizontal scroll amount (negative = left, positive = right).
 *     dv: Vertical scroll amount (negative = up, positive = down).
 */
extern void sys_test_input_add_mouse_scroll(int dh, int dv);

/**
 * sys_test_input_get_text_state:  Return whether the text input interface
 * is active (i.e., input_text_show*() has been called with no subsequent
 * input_text_hide()).
 *
 * [Return value]
 *     True if the text input interface is active, false if not.
 */
extern int sys_test_input_get_text_state(void);

/**
 * sys_test_input_get_text_prompt, sys_test_input_get_text_default:  Return
 * the prompt string or default input string specified with the most recent
 * call to begin text input.
 *
 * [Return value]
 *     Requested string, or the empty string if none was specified with the
 *     call to begin text input.
 */
extern const char *sys_test_input_get_text_prompt(void);
extern const char *sys_test_input_get_text_default(void);

/**
 * sys_test_input_add_text_char:  Simulate a single character input for the
 * active text input session.
 *
 * [Parameters]
 *     ch: Input Unicode character (must be nonnegative).
 */
extern void sys_test_input_add_text_char(int32_t ch);

/**
 * sys_test_input_add_text_char:  Simulate a single action for the active
 * text input session.
 *
 * [Parameters]
 *     detail: INPUT_TEXT_* event code.
 */
extern void sys_test_input_add_text_event(enum InputEventDetail detail);

/**
 * sys_test_input_enter_text:  Simulate entering the given string for the
 * active text input session.  This is exactly equivalent to calling
 * sys_test_input_add_text_char() for each character in the string
 * followed by INPUT_TEXT_DONE.  Note that this does not clear any
 * previous input added via sys_test_input_add_text_char().
 *
 * [Parameters]
 *     text: Text to enter.
 */
extern void sys_test_input_enter_text(const char *text);

/**
 * sys_test_input_set_touch_id:  Set the ID to use for the next simulated
 * touch.
 *
 * [Parameters]
 *     id: Touch ID (must be nonzero).
 */
extern void sys_test_input_set_touch_id(unsigned int id);

/**
 * sys_test_input_touch_down:  Simulate starting a new touch at the given
 * position.  The given touch index must not already be down.
 *
 * The touch index is used in other sys_test_input_touch_*() functions to
 * distinguish between multiple simulated touches.  The functions allow
 * one more simultaneous touch than the core code supports, to permit
 * testing of array-full conditions.
 *
 * As with mouse coordinates, a touch coordinate of 1.0 is permitted, but
 * coordinates will be clamped to [0,65535/65536].
 *
 * [Parameters]
 *     index: Touch index (0 through INPUT_MAX_TOUCHES inclusive).
 *     x, y: Initial touch position, in [0,1].
 */
extern void sys_test_input_touch_down(int index, float x, float y);

/**
 * sys_test_input_touch_move:  Simulate moving a touch by the given amount.
 * The given touch index must be down.
 *
 * [Parameters]
 *     index: Touch index (0 through INPUT_MAX_TOUCHES inclusive).
 *     dx, dy: Amount to add to current touch coordinates.
 */
extern void sys_test_input_touch_move(int index, float dx, float dy);

/**
 * sys_test_input_touch_move_to:  Simulate moving a touch to the given
 * position.  The given touch index must be down.
 *
 * [Parameters]
 *     index: Touch index (0 through INPUT_MAX_TOUCHES inclusive).
 *     x, y: Touch position, in [0,1].
 */
extern void sys_test_input_touch_move_to(int index, float x, float y);

/**
 * sys_test_input_touch_up:  Simulate ending a touch.  The given touch
 * index must be down.
 *
 * [Parameters]
 *     index: Touch index (0 through INPUT_MAX_TOUCHES inclusive).
 */
extern void sys_test_input_touch_up(int index);

/**
 * sys_test_input_touch_cancel:  Simulate cancelling a touch.  The given
 * touch index must be down.
 *
 * [Parameters]
 *     index: Touch index (0 through INPUT_MAX_TOUCHES inclusive).
 */
extern void sys_test_input_touch_cancel(int index);

/**
 * sys_test_input_send_event:  Simulate an arbitrary input event.  The
 * passed-in event structure is sent directly to the core event callback.
 *
 * [Parameters]
 *     event: Event to send.
 */
extern void sys_test_input_send_event(const struct InputEvent *event);

/*--------------------------- Sound playback ----------------------------*/

/**
 * sys_test_sound_use_live_routines:  If true, the testing versions of
 * sys_sound_*() will trampoline to the real system implementation rather
 * than executing the test implementation.  Used for testing the live
 * functions within the testing framework.
 */
extern uint8_t sys_test_sound_use_live_routines;

/**
 * sys_test_sound_set_output_rate:  Set the audio output rate for the
 * software mixer.  This function must be called while the audio output
 * subsystem is uninitialized (i.e., before sys_sound_init() or after
 * sys_sound_cleanup()).
 *
 * [Parameters]
 *     rate: Audio output rate, in samples per second.
 */
extern void sys_test_sound_set_output_rate(int rate);

/**
 * sys_test_sound_set_headphone_disconnect:  Simulate a headphone
 * disconnect event.  Headphone disconnect detection must be enabled when
 * this function is called.
 */
extern void sys_test_sound_set_headphone_disconnect(void);

/*---------------------- Timekeeping functionality ----------------------*/

/**
 * sys_test_time_set:  Set the current time reported by sys_time_now().
 *
 * [Parameters]
 *     time: Time to set.
 */
extern void sys_test_time_set(uint64_t time);

/**
 * sys_test_time_set_seconds:  Set the current time reported by
 * sys_time_now() in units of seconds.
 *
 * [Parameters]
 *     time: Time to set, in seconds.
 */
extern void sys_test_time_set_seconds(double time);

/**
 * sys_test_time_set_utc:  Set the current time and timezone offset
 * reported by sys_time_get_utc().
 *
 * [Parameters]
 *     time: Time to set.
 *     utc_offset: UTC offset to set, in minutes.
 */
extern void sys_test_time_set_utc(const struct DateTime *time, int utc_offset);

/*-------------------------- User data access ---------------------------*/

/**
 * sys_test_userdata_use_live_routines:  If true, the testing versions of
 * sys_userdata_*() will trampoline to the real system implementation
 * rather than executing the test implementation.  Used for testing the
 * live functions within the testing framework.
 */
extern uint8_t sys_test_userdata_use_live_routines;

/**
 * sys_test_userdata_writable:  If false, all attempts to write data will
 * fail.  Defaults to true (data can be written).
 */
extern uint8_t sys_test_userdata_writable;

/**
 * sys_test_userdata_get_screenshot:  Return the pixel data for the most
 * recently saved screenshot, or NULL if no screenshot has been saved.
 *
 * [Parameters]
 *     width_ret: Pointer to variable to receive the image width, in pixels.
 *     height_ret: Pointer to variable to receive the image height, in pixels.
 * [Return value]
 *     Pointer to pixel data, or NULL if no screenshot has been saved.
 */
extern const void *sys_test_userdata_get_screenshot(int *width_ret,
                                                    int *height_ret);

/*--------------------- Miscellaneous functionality ---------------------*/

/**
 * sys_test_set_language:  Set a language and dialect to be returned by
 * sys_get_language().  The language array must remain contiguous (thus it
 * is not allowed to add index 1 before index 0), and the language and
 * dialect string must be a valid string for sys_get_language():
 *    language =~ /^[a-z]{2}$/
 *    dialect =~ /^([A-Z]{2,3})?$/
 * Invalid calls cause the program to abort.
 *
 * [Parameters]
 *     index: Index to modify.
 *     language: Language string to set.
 *     dialect: Dialect string to set.
 */
extern void sys_test_set_language(int index, const char *language,
                                  const char *dialect);

/**
 * sys_test_clear_languages:  Clear the list of languages to be returned
 * by sys_get_language().  After this call, sys_get_language() will fail
 * for all index values.
 */
extern void sys_test_clear_languages(void);

/**
 * sys_test_get_last_console_output:  Return the last string printed via
 * sys_console_vprintf().  The string will be truncated after 999 bytes.
 *
 * [Return value]
 *     Last string printed via sys_console_vprintf().
 */
extern const char *sys_test_get_last_console_output(void);

/**
 * sys_test_get_last_displayed_error:  Return the last error message
 * passed to sys_display_error().  The message will be truncated after
 * 999 bytes.
 *
 * [Return value]
 *     Last message passed to sys_display_error().
 */
extern const char *sys_test_get_last_displayed_error(void);

/**
 * sys_test_get_last_external_open_path:  Return the last pathname passed
 * to sys_open_file() or sys_open_url().
 *
 * [Return value]
 *     Last path passed to sys_open_file() or sys_open_url().
 */
extern const char *sys_test_get_last_external_open_path(void);

/**
 * sys_test_get_idle_reset_flag:  Return the state of the flag indicating
 * whether sys_reset_idle_timer() has been called.
 *
 * [Return value]
 *     True if sys_reset_idle_timer() has been called.
 */
extern int sys_test_get_idle_reset_flag(void);

/**
 * sys_test_clear_idle_reset_flag:  Clear the flag indicating whether
 * sys_reset_idle_timer() has been called, so that subsequent calls to
 * ...get_idle_reset_flag() return false until sys_reset_idle_timer()
 * is called again.
 */
extern void sys_test_clear_idle_reset_flag(void);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_INCLUDE_TESTS
#endif  // SIL_SRC_SYSDEP_TEST_H
