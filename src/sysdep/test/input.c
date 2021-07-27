/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/test/input.c: Testing implementation of the system-level
 * input handling functions.
 */

#define IN_SYSDEP_TEST

#include "src/base.h"
#include "src/input.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/test.h"
#include "src/time.h"
#include "src/utility/utf8.h"

#ifdef SIL_PLATFORM_LINUX
# include "src/sysdep/linux/internal.h"
# include <X11/Xlib.h>
#endif
#ifdef SIL_PLATFORM_WINDOWS
# include "src/sysdep/windows/internal.h"
#endif

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Should we fail on the next sys_input_init() call? */
static uint8_t fail_init;

/* Event callback passed to sys_input_init(). */
static InputEventCallback event_callback;

/* Has input been grabbed? (for checking via the test control interface) */
static uint8_t input_grabbed;

/* Has a quit or suspend been requested (via the test control interface)? */
static uint8_t quit_requested;
static uint8_t suspend_requested;

/* Enable flags for each type of input. */
static uint8_t joystick_enabled;
static uint8_t joystick_rumble_enabled;
static uint8_t keyboard_enabled;
static uint8_t keyboard_full_enabled;
static uint8_t mouse_enabled;
static uint8_t text_enabled;
static uint8_t text_custom_interface_enabled;
static uint8_t text_prompt_enabled;
static uint8_t touch_enabled;

/* Is unfocused joystick input enabled? */
static uint8_t joystick_unfocused_input;

/* Should the joystick be connected at sys_input_init() time? */
static uint8_t joystick_connected_on_init;

/* Joystick information for returning via sys_input_info(). */
static SysInputJoystick joystick_info[INPUT_MAX_JOYSTICKS + 1];

/* Joystick parameters reported via sys_input_info(). */
static int num_joysticks;
static uint8_t joystick_connected;
static int num_joystick_buttons;
static int num_joystick_sticks;

/* Name to return for sys_joystick_copy_name(). */
static const char *joystick_name = "Joystick Name";

/* Named joystick button mapping. */
static int joystick_button_mapping[INPUT_JOYBUTTON__NUM];

/* Current joystick rumble state (for checking via the test control
 * interface). */
static float joy_rumble_left, joy_rumble_right, joy_rumble_time;

/* Current keyboard input state. */
static uint8_t key_state[KEY__LAST];

/* Current mouse input state. */
static float mouse_x, mouse_y;  // [0.0,1.0)
static uint8_t mouse_left, mouse_middle, mouse_right;

/* Is the text input interface active? */
static uint8_t text_input_on;
/* Prompt and default text sent by the caller (for checking via the test
 * control interface; empty if not set by the caller). */
static char text_prompt[1000];
static char text_default[1000];

/* Array of touch IDs and current positions associated with
 * sys_test_input_touch_*() index values (id==0 indicates an unused entry). */
static struct {
    unsigned int id;
    float x, y;
} touches[INPUT_MAX_TOUCHES + 1];
/* Next touch ID to use for a new touch.  Incremented by 1 for each touch,
 * rolling over (and skipping zero) if necessary. */
static unsigned int next_touch_id;

/*************************************************************************/
/******************** Interface: Basic functionality *********************/
/*************************************************************************/

int sys_input_init(void (*event_callback_)(const struct InputEvent *))
{
    PRECOND(event_callback_ != NULL, return 0);

    if (fail_init) {
        fail_init = 0;
        return 0;
    }

    event_callback = event_callback_;

    joystick_enabled = 1;
    joystick_rumble_enabled = 1;
    keyboard_enabled = 1;
    keyboard_full_enabled = 1;
    mouse_enabled = 1;
    text_enabled = 1;
    text_custom_interface_enabled = 1;
    text_prompt_enabled = 1;
    touch_enabled = 1;

    joystick_unfocused_input = 1;
    num_joysticks = 1;
    joystick_connected = joystick_connected_on_init;
    num_joystick_buttons = 20;
    num_joystick_sticks = 2;
    for (int i = 0; i < lenof(joystick_button_mapping); i++) {
        joystick_button_mapping[i] = -1;
    }
    joy_rumble_left = joy_rumble_right = joy_rumble_time = 0;
    if (joystick_connected) {
        (*event_callback)(&(InputEvent){
            .type = INPUT_EVENT_JOYSTICK, .detail = INPUT_JOYSTICK_CONNECTED,
            .timestamp = time_now(),
            {.joystick = {.device = 0}}});
    }

    mem_clear(key_state, sizeof(key_state));

    mouse_x = mouse_y = 0;
    mouse_left = mouse_middle = mouse_right = 0;

    text_input_on = 0;
    *text_prompt = 0;
    *text_default = 0;

    mem_clear(touches, sizeof(touches));
    next_touch_id = 1;

    input_grabbed = 0;

    quit_requested = 0;
    suspend_requested = 0;

    return 1;
}

/*-----------------------------------------------------------------------*/

void sys_input_cleanup(void)
{
    /* Nothing to do. */
}

/*-----------------------------------------------------------------------*/

void sys_input_update(void)
{
    /* Mostly nothing to do, but on some systems we have to pump window
     * messages. */

#ifdef SIL_PLATFORM_WINDOWS
    windows_update_window();
#endif
}

/*-----------------------------------------------------------------------*/

void sys_input_info(SysInputInfo *info_ret)
{
    for (int i = 0; i < num_joysticks; i++) {
        joystick_info[i].connected   = joystick_connected;
        joystick_info[i].num_buttons = num_joystick_buttons;
        joystick_info[i].num_sticks  = num_joystick_sticks;
        joystick_info[i].can_rumble  = joystick_rumble_enabled;
    }
    info_ret->has_joystick     = joystick_enabled;
    info_ret->num_joysticks    = num_joysticks;
    info_ret->joysticks        = joystick_info;
    info_ret->has_keyboard     = keyboard_enabled;
    info_ret->keyboard_is_full = keyboard_full_enabled;
    info_ret->has_mouse        = mouse_enabled;
    info_ret->has_text         = text_enabled;
    info_ret->text_uses_custom_interface = text_custom_interface_enabled;
    info_ret->text_has_prompt  = text_prompt_enabled;
    info_ret->has_touch        = touch_enabled;
}

/*-----------------------------------------------------------------------*/

void sys_input_enable_unfocused_joystick(int enable)
{
    joystick_unfocused_input = (enable != 0);
}

/*-----------------------------------------------------------------------*/

void sys_input_grab(int grab)
{
    input_grabbed = (grab != 0);
}

/*-----------------------------------------------------------------------*/

int sys_input_is_quit_requested(void)
{
    return quit_requested;
}

/*-----------------------------------------------------------------------*/

int sys_input_is_suspend_requested(void)
{
    return suspend_requested;
}

/*-----------------------------------------------------------------------*/

void sys_input_acknowledge_suspend_request(void)
{
    suspend_requested = 0;
}

/*************************************************************************/
/********************* Interface: Joystick handling **********************/
/*************************************************************************/

char *sys_input_joystick_copy_name(int index)
{
    PRECOND(joystick_enabled, return NULL);
    PRECOND(index >= 0 && index < num_joysticks, return NULL);
    return mem_strdup(joystick_name, 0);
}

/*-----------------------------------------------------------------------*/

int sys_input_joystick_button_mapping(int index, int name)
{
    PRECOND(joystick_enabled, return -1);
    PRECOND(index >= 0 && index < num_joysticks, return -1);
    return joystick_button_mapping[name];
}

/*-----------------------------------------------------------------------*/

void sys_input_joystick_rumble(int index, float left, float right, float time)
{
    PRECOND(joystick_enabled, return);
    PRECOND(joystick_rumble_enabled, return);
    PRECOND(index >= 0 && index < num_joysticks, return);
    joy_rumble_left  = left;
    joy_rumble_right = right;
    joy_rumble_time  = time;
}

/*************************************************************************/
/*********************** Interface: Mouse handling ***********************/
/*************************************************************************/

void sys_input_mouse_set_position(float x, float y)
{
    PRECOND(mouse_enabled, return);
    PRECOND(x >= 0 && x <= 1, return);
    PRECOND(y >= 0 && y <= 1, return);
    mouse_x = ubound(x, 65535.0f/65536.0f);
    mouse_y = ubound(y, 65535.0f/65536.0f);
    (*event_callback)(&(InputEvent){
        .type = INPUT_EVENT_MOUSE, .detail = INPUT_MOUSE_MOVE,
        .timestamp = time_now(),
        {.mouse = {.x = mouse_x, .y = mouse_y}}});
}

/*************************************************************************/
/******************** Interface: Text entry handling *********************/
/*************************************************************************/

void sys_input_text_set_state(int on, const char *text, const char *prompt)
{
    PRECOND(text_enabled, return);
    if (on) {
        text_input_on = 1;
        ASSERT(strformat_check(text_prompt, sizeof(text_prompt), "%s",
                               prompt ? prompt : ""));
        ASSERT(strformat_check(text_default, sizeof(text_default), "%s",
                               text ? text : ""));
    } else {
        text_input_on = 0;
    }
}

/*************************************************************************/
/************************* Test control routines *************************/
/*************************************************************************/

void sys_test_input_fail_init(void)
{
    fail_init = 1;
}

/*-----------------------------------------------------------------------*/

int sys_test_input_get_unfocused_joystick_state(void)
{
    return joystick_unfocused_input;
}

/*-----------------------------------------------------------------------*/

int sys_test_input_get_grab_state(void)
{
    return input_grabbed;
}

/*-----------------------------------------------------------------------*/

void sys_test_input_send_quit_request(void)
{
    quit_requested = 1;
}

/*-----------------------------------------------------------------------*/

void sys_test_input_send_suspend_request(void)
{
    suspend_requested = 1;
}

/*-----------------------------------------------------------------------*/

void sys_test_input_enable_joystick(int on)
{
    joystick_enabled = (on != 0);
}

void sys_test_input_enable_joystick_rumble(int on)
{
    joystick_rumble_enabled = (on != 0);
}

void sys_test_input_enable_keyboard(int on)
{
    keyboard_enabled = (on != 0);
}

void sys_test_input_enable_keyboard_full(int on)
{
    keyboard_full_enabled = (on != 0);
}

void sys_test_input_enable_mouse(int on)
{
    mouse_enabled = (on != 0);
}

void sys_test_input_enable_text(int on)
{
    text_enabled = (on != 0);
}

void sys_test_input_enable_text_custom_interface(int on)
{
    text_custom_interface_enabled = (on != 0);
}

void sys_test_input_enable_text_prompt(int on)
{
    text_prompt_enabled = (on != 0);
}

void sys_test_input_enable_touch(int on)
{
    touch_enabled = (on != 0);
}

/*-----------------------------------------------------------------------*/

void sys_test_input_set_joy_num_devices(int num)
{
    PRECOND(num >= 0 && num <= INPUT_MAX_JOYSTICKS + 1, return);

    if (num_joysticks != num) {
        const double timestamp = time_now();
        if (joystick_connected) {
            if (num_joysticks < num) {
                for (int i = num_joysticks; i < num; i++) {
                    (*event_callback)(&(InputEvent){
                        .type = INPUT_EVENT_JOYSTICK,
                        .detail = INPUT_JOYSTICK_CONNECTED,
                        .timestamp = timestamp,
                        {.joystick = {.device = i}}});
                }
            } else {  // num_joysticks > num
                for (int i = num_joysticks-1; i >= num; i--) {
                    (*event_callback)(&(InputEvent){
                        .type = INPUT_EVENT_JOYSTICK,
                        .detail = INPUT_JOYSTICK_DISCONNECTED,
                        .timestamp = timestamp,
                        {.joystick = {.device = i}}});
                }
            }
        }
        num_joysticks = num;
    }  // if (num_joysticks != num)
}

/*-----------------------------------------------------------------------*/

void sys_test_input_set_joy_connected(int connected)
{
    if (joystick_connected != (connected != 0)) {
        joystick_connected = (connected != 0);
        const double timestamp = time_now();
        for (int i = 0; i < num_joysticks; i++) {
            (*event_callback)(&(InputEvent){
                .type = INPUT_EVENT_JOYSTICK,
                .detail = (joystick_connected ? INPUT_JOYSTICK_CONNECTED
                                              : INPUT_JOYSTICK_DISCONNECTED),
                .timestamp = timestamp,
                {.joystick = {.device = i}}});
        }
    }
}

/*-----------------------------------------------------------------------*/

void sys_test_input_set_joy_connected_on_init(int connected)
{
    joystick_connected_on_init = (connected != 0);
}

/*-----------------------------------------------------------------------*/

void sys_test_input_set_joy_num_buttons(int num)
{
    PRECOND(num >= 0 && num <= INPUT_MAX_JOYSTICK_BUTTONS + 1, return);
    num_joystick_buttons = num;
}

/*-----------------------------------------------------------------------*/

void sys_test_input_set_joy_button_mapping(int name, int num)
{
    PRECOND(name >= 0 && name < lenof(joystick_button_mapping), return);
    PRECOND(num >= -1 && num < num_joystick_buttons, return);
    joystick_button_mapping[name] = num;
}

/*-----------------------------------------------------------------------*/

void sys_test_input_set_joy_num_sticks(int num)
{
    PRECOND(num >= 0 && num <= INPUT_MAX_JOYSTICK_STICKS + 1, return);
    num_joystick_sticks = num;
}

/*-----------------------------------------------------------------------*/

void sys_test_input_set_joy_stick(int device, int index, float x, float y)
{
    PRECOND(device >= 0, return);
    PRECOND(index >= 0, return);
    PRECOND(x >= -1 && x <= 1, return);
    PRECOND(y >= -1 && y <= 1, return);
    if (joystick_enabled) {
        (*event_callback)(&(InputEvent){
            .type = INPUT_EVENT_JOYSTICK, .detail = INPUT_JOYSTICK_STICK_CHANGE,
            .timestamp = time_now(),
            {.joystick = {.device = device, .index = index, .x = x, .y = y}}});
    }
}

/*-----------------------------------------------------------------------*/

void sys_test_input_set_joy_dpad(int device, int x, int y)
{
    PRECOND(device >= 0, return);
    PRECOND(x >= -1 && x <= 1, return);
    PRECOND(y >= -1 && y <= 1, return);
    if (joystick_enabled) {
        (*event_callback)(&(InputEvent){
            .type = INPUT_EVENT_JOYSTICK, .detail = INPUT_JOYSTICK_DPAD_CHANGE,
            .timestamp = time_now(),
            {.joystick = {.device = device, .x = x, .y = y}}});
    }
}

/*-----------------------------------------------------------------------*/

void sys_test_input_set_joy_button(int device, int index, int pressed)
{
    PRECOND(device >= 0, return);
    PRECOND(index >= 0, return);
    if (joystick_enabled) {
        (*event_callback)(&(InputEvent){
            .type = INPUT_EVENT_JOYSTICK,
            .detail = pressed ? INPUT_JOYSTICK_BUTTON_DOWN
                              : INPUT_JOYSTICK_BUTTON_UP,
            .timestamp = time_now(),
            {.joystick = {.device = device, .index = index}}});
    }
}

/*-----------------------------------------------------------------------*/

float sys_test_input_get_rumble_left(void)
{
    return joy_rumble_left;
}

float sys_test_input_get_rumble_right(void)
{
    return joy_rumble_right;
}

float sys_test_input_get_rumble_time(void)
{
    return joy_rumble_time;
}

/*-----------------------------------------------------------------------*/

void sys_test_input_press_key(int key, int system_key)
{
    PRECOND(key >= KEY__NONE && key < KEY__LAST, return);
    if (key == KEY__NONE || !key_state[key]) {
        if (key != KEY__NONE) {
            key_state[key] = 1;
        }
        if (keyboard_enabled) {
            (*event_callback)(&(InputEvent){
                .type = INPUT_EVENT_KEYBOARD,
                .detail = (key==KEY__NONE ? INPUT_KEYBOARD_SYSTEM_KEY_DOWN
                                          : INPUT_KEYBOARD_KEY_DOWN),
                .timestamp = time_now(),
                {.keyboard = {.key = key, .system_key = system_key}}});
        }
    }
}

/*-----------------------------------------------------------------------*/

void sys_test_input_release_key(int key, int system_key)
{
    PRECOND(key >= KEY__NONE && key < KEY__LAST, return);
    if (key == KEY__NONE || key_state[key]) {
        if (key != KEY__NONE) {
            key_state[key] = 0;
        }
        if (keyboard_enabled) {
            (*event_callback)(&(InputEvent){
                .type = INPUT_EVENT_KEYBOARD,
                .detail = (key==KEY__NONE ? INPUT_KEYBOARD_SYSTEM_KEY_UP
                                          : INPUT_KEYBOARD_KEY_UP),
                .timestamp = time_now(),
                {.keyboard = {.key = key, .system_key = system_key}}});
        }
    }
}

/*-----------------------------------------------------------------------*/

void sys_test_input_send_memory_low(int64_t used_bytes, int64_t free_bytes)
{
    PRECOND(used_bytes >= 0, return);
    PRECOND(free_bytes >= 0, return);
    (*event_callback)(&(InputEvent){
        .type = INPUT_EVENT_MEMORY, .detail = INPUT_MEMORY_LOW,
        .timestamp = time_now(),
        {.memory = {.used_bytes = used_bytes, .free_bytes = free_bytes}}});
}

/*-----------------------------------------------------------------------*/

void sys_test_input_move_mouse(float dx, float dy)
{
    mouse_x = bound(mouse_x + dx, 0, 65535.0f/65536.0f);
    mouse_y = bound(mouse_y + dy, 0, 65535.0f/65536.0f);
    if (mouse_enabled) {
        (*event_callback)(&(InputEvent){
            .type = INPUT_EVENT_MOUSE, .detail = INPUT_MOUSE_MOVE,
            .timestamp = time_now(),
            {.mouse = {.x = mouse_x, .y = mouse_y}}});
    }
}

/*-----------------------------------------------------------------------*/

void sys_test_input_press_mouse_buttons(int left, int middle, int right)
{
    if (left && !mouse_left) {
        mouse_left = 1;
        if (mouse_enabled) {
            (*event_callback)(&(InputEvent){
                .type = INPUT_EVENT_MOUSE, .detail = INPUT_MOUSE_LMB_DOWN,
                .timestamp = time_now(),
                {.mouse = {.x = mouse_x, .y = mouse_y}}});
        }
    }
    if (middle && !mouse_middle) {
        mouse_middle = 1;
        if (mouse_enabled) {
            (*event_callback)(&(InputEvent){
                .type = INPUT_EVENT_MOUSE, .detail = INPUT_MOUSE_MMB_DOWN,
                .timestamp = time_now(),
                {.mouse = {.x = mouse_x, .y = mouse_y}}});
        }
    }
    if (right && !mouse_right) {
        mouse_right = 1;
        if (mouse_enabled) {
            (*event_callback)(&(InputEvent){
                .type = INPUT_EVENT_MOUSE, .detail = INPUT_MOUSE_RMB_DOWN,
                .timestamp = time_now(),
                {.mouse = {.x = mouse_x, .y = mouse_y}}});
        }
    }
}

/*-----------------------------------------------------------------------*/

void sys_test_input_release_mouse_buttons(int left, int middle, int right)
{
    if (left && mouse_left) {
        mouse_left = 0;
        if (mouse_enabled) {
            (*event_callback)(&(InputEvent){
                .type = INPUT_EVENT_MOUSE, .detail = INPUT_MOUSE_LMB_UP,
                .timestamp = time_now(),
                {.mouse = {.x = mouse_x, .y = mouse_y}}});
        }
    }
    if (middle && mouse_middle) {
        mouse_middle = 0;
        if (mouse_enabled) {
            (*event_callback)(&(InputEvent){
                .type = INPUT_EVENT_MOUSE, .detail = INPUT_MOUSE_MMB_UP,
                .timestamp = time_now(),
                {.mouse = {.x = mouse_x, .y = mouse_y}}});
        }
    }
    if (right && mouse_right) {
        mouse_right = 0;
        if (mouse_enabled) {
            (*event_callback)(&(InputEvent){
                .type = INPUT_EVENT_MOUSE, .detail = INPUT_MOUSE_RMB_UP,
                .timestamp = time_now(),
                {.mouse = {.x = mouse_x, .y = mouse_y}}});
        }
    }
}

/*-----------------------------------------------------------------------*/

void sys_test_input_add_mouse_scroll(int dh, int dv)
{
    if (dh) {
        if (mouse_enabled) {
            (*event_callback)(&(InputEvent){
                .type = INPUT_EVENT_MOUSE, .detail = INPUT_MOUSE_SCROLL_H,
                .timestamp = time_now(),
                {.mouse = {.x = mouse_x, .y = mouse_y, .scroll = dh}}});
        }
    }
    if (dv) {
        if (mouse_enabled) {
            (*event_callback)(&(InputEvent){
                .type = INPUT_EVENT_MOUSE, .detail = INPUT_MOUSE_SCROLL_V,
                .timestamp = time_now(),
                {.mouse = {.x = mouse_x, .y = mouse_y, .scroll = dv}}});
        }
    }
}

/*-----------------------------------------------------------------------*/

int sys_test_input_get_text_state(void)
{
    return text_input_on;
}

/*-----------------------------------------------------------------------*/

const char *sys_test_input_get_text_prompt(void)
{
    return text_prompt;
}

/*-----------------------------------------------------------------------*/

const char *sys_test_input_get_text_default(void)
{
    return text_default;
}

/*-----------------------------------------------------------------------*/

void sys_test_input_add_text_char(int32_t ch)
{
    ASSERT(ch >= 0);
    if (text_enabled) {
        (*event_callback)(&(InputEvent){
            .type = INPUT_EVENT_TEXT, .detail = INPUT_TEXT_INPUT,
            .timestamp = time_now(),
            {.text = {.ch = ch}}});
    }
}

/*-----------------------------------------------------------------------*/

void sys_test_input_add_text_event(InputEventDetail detail)
{
    ASSERT(detail > INPUT_TEXT_INPUT && detail < INPUT_TOUCH_DOWN);
    if (text_enabled) {
        (*event_callback)(&(InputEvent){
            .type = INPUT_EVENT_TEXT, .detail = detail,
            .timestamp = time_now()});
    }
}

/*-----------------------------------------------------------------------*/

void sys_test_input_enter_text(const char *text)
{
    PRECOND(text != NULL, return);
    int32_t ch;
    while ((ch = utf8_read(&text)) != 0) {
        ASSERT(ch != -1, return);
        sys_test_input_add_text_char(ch);
    }
    sys_test_input_add_text_event(INPUT_TEXT_DONE);
}

/*-----------------------------------------------------------------------*/

void sys_test_input_set_touch_id(unsigned int id)
{
    PRECOND(id != 0, return);
    next_touch_id = id;
}

/*-----------------------------------------------------------------------*/

void sys_test_input_touch_down(int index, float x, float y)
{
    PRECOND(index >= 0 && index < lenof(touches), return);
    PRECOND(x >= 0 && x <= 1, return);
    PRECOND(y >= 0 && y <= 1, return);
    PRECOND(touches[index].id == 0, return);
    touches[index].id = next_touch_id++;
    if (next_touch_id == 0) {
        next_touch_id++;
    }
    touches[index].x = ubound(x, 65535.0f/65536.0f);
    touches[index].y = ubound(y, 65535.0f/65536.0f);
    if (touch_enabled) {
        (*event_callback)(&(InputEvent){
            .type = INPUT_EVENT_TOUCH, .detail = INPUT_TOUCH_DOWN,
            .timestamp = time_now(),
            {.touch = {.id = touches[index].id,
                       .x = touches[index].x, .y = touches[index].y}}});
    }
}

/*-----------------------------------------------------------------------*/

void sys_test_input_touch_move(int index, float dx, float dy)
{
    PRECOND(index >= 0 && index < lenof(touches), return);
    PRECOND(touches[index].id != 0, return);
    touches[index].x = bound(touches[index].x + dx, 0, 65535.0f/65536.0f);
    touches[index].y = bound(touches[index].y + dy, 0, 65535.0f/65536.0f);
    if (touch_enabled) {
        (*event_callback)(&(InputEvent){
            .type = INPUT_EVENT_TOUCH, .detail = INPUT_TOUCH_MOVE,
            .timestamp = time_now(),
            {.touch = {.id = touches[index].id,
                       .x = touches[index].x, .y = touches[index].y}}});
    }
}

/*-----------------------------------------------------------------------*/

void sys_test_input_touch_move_to(int index, float x, float y)
{
    PRECOND(index >= 0 && index < lenof(touches), return);
    PRECOND(x >= 0 && x <= 1, return);
    PRECOND(y >= 0 && y <= 1, return);
    PRECOND(touches[index].id != 0, return);
    touches[index].x = ubound(x, 65535.0f/65536.0f);
    touches[index].y = ubound(y, 65535.0f/65536.0f);
    if (touch_enabled) {
        (*event_callback)(&(InputEvent){
            .type = INPUT_EVENT_TOUCH, .detail = INPUT_TOUCH_MOVE,
            .timestamp = time_now(),
            {.touch = {.id = touches[index].id,
                       .x = touches[index].x, .y = touches[index].y}}});
    }
}

/*-----------------------------------------------------------------------*/

void sys_test_input_touch_up(int index)
{
    PRECOND(index >= 0 && index < lenof(touches), return);
    PRECOND(touches[index].id != 0, return);
    if (touch_enabled) {
        (*event_callback)(&(InputEvent){
            .type = INPUT_EVENT_TOUCH, .detail = INPUT_TOUCH_UP,
            .timestamp = time_now(),
            {.touch = {.id = touches[index].id,
                       .x = touches[index].x, .y = touches[index].y}}});
    }
    touches[index].id = 0;
}

/*-----------------------------------------------------------------------*/

void sys_test_input_touch_cancel(int index)
{
    PRECOND(index >= 0 && index < lenof(touches), return);
    PRECOND(touches[index].id != 0, return);
    if (touch_enabled) {
        (*event_callback)(&(InputEvent){
            .type = INPUT_EVENT_TOUCH, .detail = INPUT_TOUCH_CANCEL,
            .timestamp = time_now(),
            {.touch = {.id = touches[index].id,
                       .x = touches[index].x, .y = touches[index].y}}});
    }
    touches[index].id = 0;
}

/*-----------------------------------------------------------------------*/

void sys_test_input_send_event(const InputEvent *event)
{
    (*event_callback)(event);
}

/*************************************************************************/
/*************************************************************************/
