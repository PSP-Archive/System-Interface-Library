/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/input.c: Input device management.
 */

#include "src/base.h"
#include "src/input.h"
#include "src/mutex.h"
#include "src/sysdep.h"
#include "src/time.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Have we been initialized? */
static uint8_t initted;

/* Should movement-type input events be coalesced? */
static uint8_t coalesce_movement;

/* Input event callback set by the client code, or NULL if none. */
static InputEventCallback event_callback;

/* Information about available input devices. */
static SysInputInfo info;

/* Ring buffer (lock-free) for input events. */
static struct {
    /* Index at which to store the next incoming event. */
    int next_in;
    /* Index from which to retrieve the next event to be processed. */
    int next_out;
    /* Buffer for events.  Note that we need one more slot than the
     * defined size because we can't fill up the entire buffer (doing so
     * would cause the buffer to be treated as empty). */
    InputEvent buffer[INPUT_RING_BUFFER_SIZE + 1];
} event_buffer;

/* Single event buffer (lock-protected) for movement event coalescing. */
static InputEvent coalesce_event;
/* Mutex for synchronizing access to coalesce_event.  Created the first
 * time coalescing is enabled. */
static int coalesce_mutex;

/* Current joystick state. */
typedef struct JoystickInfo JoystickInfo;
struct JoystickInfo {
    struct {float x, y;} sticks[INPUT_MAX_JOYSTICK_STICKS];
    uint8_t buttons[INPUT_MAX_JOYSTICK_BUTTONS];
    int dpad_x, dpad_y;
};
static JoystickInfo joystick_state[INPUT_MAX_JOYSTICKS];

/* Current keyboard state. */
static uint8_t key_state[KEY__LAST];
static int key_modifier_state;
/* Keycode of last key pressed, or 0 if none. */
static int last_key_pressed;

/* Current mouse state. */
static float mouse_x, mouse_y;
static int mouse_hscroll, mouse_vscroll;
static uint8_t mouse_left, mouse_middle, mouse_right;

/* Current text input state. */
static uint8_t text_input_on;
/* Text input buffer for returning from input_text_get_char(). */
static int32_t text_buffer[1000];
static int text_buffer_len;
static int text_buffer_pos;  // Next index to return.

/* Current touch state.  Active touches are packed into the beginning of
 * the array with no holes; on removal of a touch, all subsequent entries
 * are shifted forward to fill the hole. */
typedef struct TouchState TouchState;
struct TouchState {
    /* ID for this touch, as used with input_touch_*() functions. */
    unsigned int id;
    /* Current coordinates of the touch. */
    float x, y;
    /* Initial coordinates of the touch. */
    float initial_x, initial_y;
};
static TouchState touches[INPUT_MAX_TOUCHES];
/* Number of active touches. */
static int num_touches;

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * update_info:  Retrieve input device information from the sysdep layer
 * and update local data accordingly.
 */
static void update_info(void);

/**
 * receive_event:  Event callback passed to system-dependent code.  This
 * function receives events and stores them directly to the event ring
 * buffer without performing any additional processing, except for
 * coalescing consecutive mouse/touch move events if enabled.
 *
 * [Parameters]
 *     event: Input event.
 */
static void receive_event(const InputEvent *event);

/**
 * flush_coalesced_event:  Flush the pending coalesced event, if any, into
 * the event queue.  Does nothing if coalescing is disabled.
 */
static void flush_coalesced_event(void);

/**
 * process_event:  Process an event retrieved from the ring buffer,
 * updating internal state as appropriate and passing the event off to the
 * client event callback (if any).
 *
 * [Parameters]
 *     event: Input event.
 */
static void process_event(InputEvent *event);

/**
 * recompute_key_modifiers:  Recompute the value of key_modifier_state
 * based on a change in state of the given key.
 *
 * [Parameters]
 *     key: Key for which a state change occurred (a KEY_* constant).
 */
static void recompute_key_modifiers(int key);

/**
 * lookup_touch:  Look up and return the touches[] array index for the
 * touch with the given ID.
 *
 * [Parameters]
 *     id: ID of touch to look up.
 * [Return value]
 *     Index of the touch in the array, or -1 if the ID is invalid.
 */
static int lookup_touch(unsigned int id);

/**
 * ring_buffer_get:  Retrieve the next event from the event ring buffer.
 *
 * [Parameters]
 *     event_ret: Pointer to InputEvent structure in which to store the
 *         retrieved event.
 * [Return value]
 *     True if an event was returned, false if the buffer was empty.
 */
static int ring_buffer_get(InputEvent *event_ret);

/**
 * ring_buffer_put:  Store the given event in the ring buffer.
 *
 * [Parameters]
 *     event: InputEvent to store in the buffer.
 * [Return value]
 *     True if the event was stored, false if the buffer was full.
 */
static int ring_buffer_put(const InputEvent *event);

/*************************************************************************/
/********************* Interface: Base functionality *********************/
/*************************************************************************/

int input_init(void)
{
    if (initted) {
        DLOG("Already initialized!");
        return 0;
    }

    mem_clear(&info, sizeof(info));
    event_buffer.next_in = 0;
    event_buffer.next_out = 0;

    coalesce_event.type = 0;
    coalesce_movement = 0;
    coalesce_mutex = 0;
    event_callback = NULL;
    mem_clear(joystick_state, sizeof(joystick_state));
    mem_clear(key_state, sizeof(key_state));
    key_modifier_state = 0;
    last_key_pressed = 0;
    mouse_hscroll = 0;
    mouse_vscroll = 0;
    mouse_x = 0.5;
    mouse_y = 0.5;
    mouse_left = 0;
    mouse_middle = 0;
    mouse_right = 0;
    text_input_on = 0;
    num_touches = 0;

    if (!sys_input_init(receive_event)) {
        return 0;
    }

    initted = 1;

    sys_input_update();
    update_info();
    return 1;
}

/*-----------------------------------------------------------------------*/

void input_cleanup(void)
{
    if (!initted) {
        return;
    }

    sys_input_cleanup();
    mem_clear(&info, sizeof(info));

    mutex_destroy(coalesce_mutex);

    initted = 0;
}

/*-----------------------------------------------------------------------*/

void input_update(void)
{
    if (!initted) {
        return;
    }

    sys_input_update();
    flush_coalesced_event();
    update_info();

    last_key_pressed = 0;
    mouse_hscroll = 0;
    mouse_vscroll = 0;
    InputEvent event;
    while (ring_buffer_get(&event)) {
        process_event(&event);
    }
}

/*-----------------------------------------------------------------------*/

void input_set_event_callback(InputEventCallback callback)
{
    event_callback = callback;
}

/*-----------------------------------------------------------------------*/

void input_enable_coalescing(int enable)
{
    if (enable && !coalesce_mutex) {
        coalesce_mutex = mutex_create(MUTEX_SIMPLE, MUTEX_UNLOCKED);
        if (UNLIKELY(!coalesce_mutex)) {
            DLOG("Failed to create mutex, can't enable coalescing");
            return;
        }
    }

    coalesce_movement = (enable != 0);
}

/*-----------------------------------------------------------------------*/

void input_grab(int grab)
{
    sys_input_grab(grab);
}

/*-----------------------------------------------------------------------*/

int input_is_quit_requested(void)
{
    return sys_input_is_quit_requested();
}

/*-----------------------------------------------------------------------*/

int input_is_suspend_requested(void)
{
    return sys_input_is_suspend_requested();
}

/*-----------------------------------------------------------------------*/

void input_acknowledge_suspend_request(void)
{
    sys_input_acknowledge_suspend_request();
}

/*************************************************************************/
/********************* Interface: Joystick handling **********************/
/*************************************************************************/

void input_enable_unfocused_joystick(int enable)
{
    sys_input_enable_unfocused_joystick(enable);
}

/*-----------------------------------------------------------------------*/

int input_joystick_present(void)
{
    return info.has_joystick && info.num_joysticks > 0;
}

/*-----------------------------------------------------------------------*/

int input_joystick_count(void)
{
    return info.has_joystick ? info.num_joysticks : 0;
}

/*-----------------------------------------------------------------------*/

int input_joystick_connected(int index)
{
    if (info.has_joystick && index >= 0 && index < info.num_joysticks) {
        return info.joysticks[index].connected;
    } else {
        return 0;
    }
}

/*-----------------------------------------------------------------------*/

char *input_joystick_copy_name(int index)
{
    if (info.has_joystick && index >= 0 && index < info.num_joysticks) {
        return sys_input_joystick_copy_name(index);
    } else {
        return 0;
    }
}

/*-----------------------------------------------------------------------*/

int input_joystick_num_buttons(int index)
{
    if (info.has_joystick && index >= 0 && index < info.num_joysticks) {
        return ubound(info.joysticks[index].num_buttons,
                      lenof(joystick_state[0].buttons));
    } else {
        return 0;
    }
}

/*-----------------------------------------------------------------------*/

int input_joystick_button_mapping(int index, int name)
{
    if (info.has_joystick && index >= 0 && index < info.num_joysticks
     && name >= 0 && name < INPUT_JOYBUTTON__NUM) {
        return sys_input_joystick_button_mapping(index, name);
    } else {
        return -1;
    }
}

/*-----------------------------------------------------------------------*/

int input_joystick_num_sticks(int index)
{
    if (info.has_joystick && index >= 0 && index < info.num_joysticks) {
        return ubound(info.joysticks[index].num_sticks,
                      lenof(joystick_state[0].sticks));
    } else {
        return 0;
    }
}

/*-----------------------------------------------------------------------*/

int input_joystick_can_rumble(int index)
{
    if (info.has_joystick && index >= 0 && index < info.num_joysticks) {
        return info.joysticks[index].can_rumble;
    } else {
        return 0;
    }
}

/*-----------------------------------------------------------------------*/

int input_joystick_button_state(int index, int button)
{
    if (!info.has_joystick || index < 0 || index >= info.num_joysticks
     || button < 0 || button >= info.joysticks[index].num_buttons
     || button >= lenof(joystick_state[0].buttons)) {
        return 0;
    }

    return joystick_state[index].buttons[button];
}

/*-----------------------------------------------------------------------*/

void input_joystick_read_stick(int index, int stick, float *x_ret,
                               float *y_ret)
{
    if (!info.has_joystick || index < 0 || index >= info.num_joysticks
     || stick < 0 || stick >= info.joysticks[index].num_sticks
     || stick >= lenof(joystick_state[0].sticks)) {
        if (x_ret) {
            *x_ret = 0;
        }
        if (y_ret) {
            *y_ret = 0;
        }
        return;
    }

    if (x_ret) {
        *x_ret = joystick_state[index].sticks[stick].x;
    }
    if (y_ret) {
        *y_ret = joystick_state[index].sticks[stick].y;
    }
}

/*-----------------------------------------------------------------------*/

void input_joystick_read_dpad(int index, int *x_ret, int *y_ret)
{
    if (!info.has_joystick || index < 0 || index >= info.num_joysticks) {
        if (x_ret) {
            *x_ret = 0;
        }
        if (y_ret) {
            *y_ret = 0;
        }
        return;
    }

    if (x_ret) {
        *x_ret = joystick_state[index].dpad_x;
    }
    if (y_ret) {
        *y_ret = joystick_state[index].dpad_y;
    }
}

/*-----------------------------------------------------------------------*/

void input_joystick_rumble(int index, float left, float right, float time)
{
    if (!info.has_joystick || index < 0 || index >= info.num_joysticks
     || !info.joysticks[index].can_rumble) {
        return;
    }

    sys_input_joystick_rumble(
        index,  bound(left,0,1), bound(right,0,1), lbound(time,0));
}

/*************************************************************************/
/********************* Interface: Keyboard handling **********************/
/*************************************************************************/

int input_keyboard_present(void)
{
    return info.has_keyboard;
}

/*-----------------------------------------------------------------------*/

int input_keyboard_is_full(void)
{
    return info.has_keyboard && info.keyboard_is_full;
}

/*-----------------------------------------------------------------------*/

int input_key_modifier_state(void)
{
    if (!info.has_keyboard) {
        return 0;
    }

    return key_modifier_state;
}

/*-----------------------------------------------------------------------*/

int input_key_state(int key)
{
    if (!info.has_keyboard || key <= KEY__NONE || key >= KEY__LAST) {
        return 0;
    }

    return key_state[key];
}

/*-----------------------------------------------------------------------*/

int input_key_last_pressed(void)
{
    if (!info.has_keyboard) {
        return 0;
    }

    return last_key_pressed;
}

/*************************************************************************/
/*********************** Interface: Mouse handling ***********************/
/*************************************************************************/

int input_mouse_present(void)
{
    return info.has_mouse;
}

/*-----------------------------------------------------------------------*/

void input_mouse_get_position(float *x_ret, float *y_ret)
{
    if (x_ret) {
        *x_ret = mouse_x;
    }
    if (y_ret) {
        *y_ret = mouse_y;
    }
}

/*-----------------------------------------------------------------------*/

int input_mouse_left_button_state(void)
{
    return mouse_left;
}

/*-----------------------------------------------------------------------*/

int input_mouse_middle_button_state(void)
{
    return mouse_middle;
}

/*-----------------------------------------------------------------------*/

int input_mouse_right_button_state(void)
{
    return mouse_right;
}

/*-----------------------------------------------------------------------*/

int input_mouse_horiz_scroll(void)
{
    return mouse_hscroll;
}

/*-----------------------------------------------------------------------*/

int input_mouse_vert_scroll(void)
{
    return mouse_vscroll;
}

/*-----------------------------------------------------------------------*/

void input_mouse_set_position(float x, float y)
{
    if (info.has_mouse) {
        sys_input_mouse_set_position(bound(x,0,1), bound(y,0,1));
    }
}

/*************************************************************************/
/******************** Interface: Text entry handling *********************/
/*************************************************************************/

int input_text_present(void)
{
    return info.has_text;
}

/*-----------------------------------------------------------------------*/

int input_text_uses_custom_interface(void)
{
    return info.has_text && info.text_uses_custom_interface;
}

/*-----------------------------------------------------------------------*/

int input_text_can_display_prompt(void)
{
    return info.has_text && info.text_uses_custom_interface
        && info.text_has_prompt;
}

/*-----------------------------------------------------------------------*/

void input_text_enable(void)
{
    if (info.has_text) {
        sys_input_text_set_state(1, NULL, NULL);
        text_input_on = 1;
        text_buffer_len = text_buffer_pos = 0;
    }
}

/*-----------------------------------------------------------------------*/

void input_text_enable_with_default_text(const char *text)
{
    if (input_text_uses_custom_interface()) {
        sys_input_text_set_state(1, text, NULL);
        text_input_on = 1;
        text_buffer_len = text_buffer_pos = 0;
    } else {
        input_text_enable();
    }
}

/*-----------------------------------------------------------------------*/

void input_text_enable_with_prompt(const char *text, const char *prompt)
{
    if (input_text_can_display_prompt()) {
        sys_input_text_set_state(1, text, prompt);
        text_input_on = 1;
        text_buffer_len = text_buffer_pos = 0;
    } else {
        input_text_enable_with_default_text(text);
    }
}

/*-----------------------------------------------------------------------*/

void input_text_disable(void)
{
    if (info.has_text) {
        sys_input_text_set_state(0, NULL, NULL);
        text_input_on = 0;
    }
}

/*-----------------------------------------------------------------------*/

int input_text_get_char(void)
{
    if (text_input_on) {
        if (text_buffer_pos < text_buffer_len) {
            const int32_t ch = text_buffer[text_buffer_pos];
            if (ch != -INPUT_TEXT_CANCELLED) {
                text_buffer_pos++;
            }
            return ch;
        } else {
            return 0;
        }
    } else {
        return -INPUT_TEXT_CANCELLED;
    }
}

/*************************************************************************/
/*********************** Interface: Touch handling ***********************/
/*************************************************************************/

int input_touch_present(void)
{
    return info.has_touch;
}

/*-----------------------------------------------------------------------*/

int input_touch_num_touches(void)
{
    return num_touches;
}

/*-----------------------------------------------------------------------*/

unsigned int input_touch_id_for_index(int index)
{
    if (index >= 0 && index < num_touches) {
        return touches[index].id;
    } else {
        return 0;
    }
}

/*-----------------------------------------------------------------------*/

int input_touch_active(unsigned int id)
{
    const int index = lookup_touch(id);
    return index >= 0;
}

/*-----------------------------------------------------------------------*/

void input_touch_get_position(unsigned int id, float *x_ret, float *y_ret)
{
    const int index = lookup_touch(id);
    if (index >= 0) {
        if (x_ret) {
            *x_ret = touches[index].x;
        }
        if (y_ret) {
            *y_ret = touches[index].y;
        }
    } else {
        if (x_ret) {
            *x_ret = 0.5;
        }
        if (y_ret) {
            *y_ret = 0.5;
        }
    }
}

/*-----------------------------------------------------------------------*/

void input_touch_get_initial_position(unsigned int id, float *x_ret,
                                      float *y_ret)
{
    const int index = lookup_touch(id);
    if (index >= 0) {
        if (x_ret) {
            *x_ret = touches[index].initial_x;
        }
        if (y_ret) {
            *y_ret = touches[index].initial_y;
        }
    } else {
        if (x_ret) {
            *x_ret = 0.5;
        }
        if (y_ret) {
            *y_ret = 0.5;
        }
    }
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static void update_info(void)
{
    sys_input_info(&info);

    ASSERT(!info.has_joystick || info.num_joysticks == 0 || info.joysticks,
           info.has_joystick = 0);
    if (info.has_joystick) {
        if (info.num_joysticks > lenof(joystick_state)) {
            static uint8_t warned = 0;
            if (!warned) {
                DLOG("System reports %d joysticks, but only %d supported",
                     info.num_joysticks, lenof(joystick_state));
                warned = 1;
            }
            info.num_joysticks = lenof(joystick_state);
        }
    }

    if (!info.has_mouse) {
        mouse_x = mouse_y = 0.5;
        mouse_left = mouse_middle = mouse_right = 0;
    }
}

/*-----------------------------------------------------------------------*/

static void receive_event(const InputEvent *event)
{
    ASSERT(!coalesce_movement || coalesce_mutex, coalesce_movement = 0);

    if (coalesce_movement
     && (event->detail == INPUT_JOYSTICK_STICK_CHANGE
      || event->detail == INPUT_MOUSE_MOVE
      || event->detail == INPUT_TOUCH_MOVE)) {
        mutex_lock(coalesce_mutex);
        if (event->type != coalesce_event.type
         || event->detail != coalesce_event.detail
         || (event->detail == INPUT_JOYSTICK_STICK_CHANGE
             && (event->joystick.device != coalesce_event.joystick.device
                 || event->joystick.index != coalesce_event.joystick.index))
         || (event->detail == INPUT_TOUCH_MOVE
             && event->touch.id != coalesce_event.touch.id))
        {
            if (coalesce_event.type != 0) {
                if (UNLIKELY(!ring_buffer_put(&coalesce_event))) {
                    DLOG("WARNING: dropped event %d/%d (%g) due to full buffer",
                         coalesce_event.type, coalesce_event.detail,
                         coalesce_event.timestamp);
                }
            }
        }
        coalesce_event = *event;
        mutex_unlock(coalesce_mutex);
        return;
    }

    if (UNLIKELY(!ring_buffer_put(event))) {
        DLOG("WARNING: dropped event %d/%d (%g) due to full buffer",
             event->type, event->detail, event->timestamp);
    }
}

/*-----------------------------------------------------------------------*/

static void flush_coalesced_event(void)
{
    ASSERT(!coalesce_movement || coalesce_mutex, coalesce_movement = 0);

    if (coalesce_movement) {
        ASSERT(coalesce_mutex);
        mutex_lock(coalesce_mutex);
        if (coalesce_event.type != 0) {
            if (UNLIKELY(!ring_buffer_put(&coalesce_event))) {
                DLOG("WARNING: dropped event %d/%d (%g) due to full buffer",
                     coalesce_event.type, coalesce_event.detail,
                     coalesce_event.timestamp);
            }
            coalesce_event.type = 0;
        }
        mutex_unlock(coalesce_mutex);
    }
}

/*-----------------------------------------------------------------------*/

static void process_event(InputEvent *event)
{
    switch (event->detail) {

      /**** Joystick events ****/

      case INPUT_JOYSTICK_CONNECTED:
      case INPUT_JOYSTICK_DISCONNECTED:
        /* Nothing to do here. */
        break;

      case INPUT_JOYSTICK_BUTTON_DOWN:
        ASSERT(event->type == INPUT_EVENT_JOYSTICK, return);
        ASSERT(event->joystick.device >= 0, return);
        ASSERT(event->joystick.index >= 0, return);
        /* We don't check against SysInputInfo because we may be looking
         * at a newly-connected joystick which hasn't been registered in
         * the info structure yet.  As long as the data fits in our static
         * array, we record it there. */
        if (event->joystick.device < lenof(joystick_state)
         && event->joystick.index < lenof(joystick_state[0].buttons)) {
            joystick_state[event->joystick.device]
                .buttons[event->joystick.index] = 1;
        }
        break;

      case INPUT_JOYSTICK_BUTTON_UP:
        ASSERT(event->type == INPUT_EVENT_JOYSTICK, return);
        ASSERT(event->joystick.device >= 0, return);
        ASSERT(event->joystick.index >= 0, return);
        if (event->joystick.device < lenof(joystick_state)
         && event->joystick.index < lenof(joystick_state[0].buttons)) {
            joystick_state[event->joystick.device]
                .buttons[event->joystick.index] = 0;
        }
        break;

      case INPUT_JOYSTICK_DPAD_CHANGE:
        ASSERT(event->type == INPUT_EVENT_JOYSTICK, return);
        ASSERT(event->joystick.device >= 0, return);
        ASSERT(event->joystick.x == (float)(int)event->joystick.x, return);
        ASSERT(event->joystick.y == (float)(int)event->joystick.y, return);
        if (event->joystick.device < lenof(joystick_state)) {
            joystick_state[event->joystick.device].dpad_x =
                (int)event->joystick.x;
            joystick_state[event->joystick.device].dpad_y =
                (int)event->joystick.y;
        }
        break;

      case INPUT_JOYSTICK_STICK_CHANGE:
        ASSERT(event->type == INPUT_EVENT_JOYSTICK, return);
        ASSERT(event->joystick.device >= 0, return);
        ASSERT(event->joystick.index >= 0, return);
        if (event->joystick.device < lenof(joystick_state)
         && event->joystick.index < lenof(joystick_state[0].sticks)) {
            joystick_state[event->joystick.device]
                .sticks[event->joystick.index].x = event->joystick.x;
            joystick_state[event->joystick.device]
                .sticks[event->joystick.index].y = event->joystick.y;
        }
        break;

      /**** Keyboard events ****/

      case INPUT_KEYBOARD_KEY_DOWN:
        ASSERT(event->type == INPUT_EVENT_KEYBOARD, return);
        key_state[event->keyboard.key] = 1;
        recompute_key_modifiers(event->keyboard.key);
        event->keyboard.modifiers = key_modifier_state;
        last_key_pressed = event->keyboard.key;
        break;

      case INPUT_KEYBOARD_KEY_UP:
        ASSERT(event->type == INPUT_EVENT_KEYBOARD, return);
        key_state[event->keyboard.key] = 0;
        recompute_key_modifiers(event->keyboard.key);
        event->keyboard.modifiers = key_modifier_state;
        break;

      case INPUT_KEYBOARD_SYSTEM_KEY_DOWN:
      case INPUT_KEYBOARD_SYSTEM_KEY_UP:
        ASSERT(event->type == INPUT_EVENT_KEYBOARD, return);
        event->keyboard.modifiers = key_modifier_state;
        break;

      /**** Memory pressure events ****/

      case INPUT_MEMORY_LOW:
        ASSERT(event->type == INPUT_EVENT_MEMORY, return);
        /* Nothing special to do, just pass it on to the client callback. */
        break;

      /**** Mouse events ****/

      case INPUT_MOUSE_MOVE:
        ASSERT(event->type == INPUT_EVENT_MOUSE, return);
        mouse_x = event->mouse.x;
        mouse_y = event->mouse.y;
        break;

      case INPUT_MOUSE_LMB_DOWN:
        ASSERT(event->type == INPUT_EVENT_MOUSE, return);
        mouse_left = 1;
        break;

      case INPUT_MOUSE_LMB_UP:
        ASSERT(event->type == INPUT_EVENT_MOUSE, return);
        mouse_left = 0;
        break;

      case INPUT_MOUSE_MMB_DOWN:
        ASSERT(event->type == INPUT_EVENT_MOUSE, return);
        mouse_middle = 1;
        break;

      case INPUT_MOUSE_MMB_UP:
        ASSERT(event->type == INPUT_EVENT_MOUSE, return);
        mouse_middle = 0;
        break;

      case INPUT_MOUSE_RMB_DOWN:
        ASSERT(event->type == INPUT_EVENT_MOUSE, return);
        mouse_right = 1;
        break;

      case INPUT_MOUSE_RMB_UP:
        ASSERT(event->type == INPUT_EVENT_MOUSE, return);
        mouse_right = 0;
        break;

      case INPUT_MOUSE_SCROLL_H:
        ASSERT(event->type == INPUT_EVENT_MOUSE, return);
        mouse_hscroll += event->mouse.scroll;
        break;

      case INPUT_MOUSE_SCROLL_V:
        ASSERT(event->type == INPUT_EVENT_MOUSE, return);
        mouse_vscroll += event->mouse.scroll;
        break;

      /**** Text events ****/

      case INPUT_TEXT_INPUT:
      case INPUT_TEXT_DONE:
      case INPUT_TEXT_CANCELLED:
      case INPUT_TEXT_CLEAR:
      case INPUT_TEXT_BACKSPACE:
      case INPUT_TEXT_DELETE:
      case INPUT_TEXT_CURSOR_LEFT:
      case INPUT_TEXT_CURSOR_RIGHT:
      case INPUT_TEXT_CURSOR_HOME:
      case INPUT_TEXT_CURSOR_END: {
        ASSERT(event->type == INPUT_EVENT_TEXT, return);
        int32_t ch;
        if (event->detail == INPUT_TEXT_INPUT) {
            ch = event->text.ch;
        } else {
            ch = -(event->detail);
        }
        /* Leave space for CANCELLED after DONE. */
        if (text_buffer_len >= lenof(text_buffer) - 1) {
            if (text_buffer_pos >= text_buffer_len) {
                text_buffer_pos = text_buffer_len = 0;
            } else if (text_buffer_pos > 0) {
                memmove(
                    &text_buffer[0], &text_buffer[text_buffer_pos],
                    sizeof(*text_buffer) * (text_buffer_len - text_buffer_pos));
                text_buffer_len -= text_buffer_pos;
                text_buffer_pos = 0;
            } else if (ch == -INPUT_TEXT_DONE) {
                ASSERT(text_buffer_len == lenof(text_buffer) - 1,
                       text_buffer_len = lenof(text_buffer) - 1);
                text_buffer_len--;
                DLOG("Lost text input: U+%04X", text_buffer[text_buffer_len]);
            } else {
                DLOG("Lost text input: U+%04X", ch);
                break;
            }
        }
        text_buffer[text_buffer_len++] = ch;
        if (ch == -INPUT_TEXT_DONE) {
            /* Add a CANCELLED terminator so input_text_get_char() knows to
             * stop here. */
            ASSERT(text_buffer_len < lenof(text_buffer),
                   text_buffer_len = lenof(text_buffer) - 1);
            text_buffer[text_buffer_len++] = -INPUT_TEXT_CANCELLED;
        }
        break;
      }  // case INPUT_TEXT_*

      /**** Touch events ****/

      case INPUT_TOUCH_DOWN:
        ASSERT(event->type == INPUT_EVENT_TOUCH, return);
        if (num_touches < lenof(touches)) {
            touches[num_touches].id = event->touch.id;
            touches[num_touches].x = event->touch.x;
            touches[num_touches].y = event->touch.y;
            touches[num_touches].initial_x = event->touch.x;
            touches[num_touches].initial_y = event->touch.y;
            event->touch.initial_x = event->touch.x;
            event->touch.initial_y = event->touch.y;
            num_touches++;
        } else {
            DLOG("Touch array full, dropping new touch %u", event->touch.id);
            return;  // Don't pass the event up in order to prevent desyncs.
        }
        break;

      case INPUT_TOUCH_MOVE: {
        ASSERT(event->type == INPUT_EVENT_TOUCH, return);
        const int index = lookup_touch(event->touch.id);
        if (index < 0) {
            return;
        }
        touches[index].x = event->touch.x;
        touches[index].y = event->touch.y;
        event->touch.initial_x = touches[index].initial_x;
        event->touch.initial_y = touches[index].initial_y;
        break;
      }  // case INPUT_TOUCH_MOVE

      case INPUT_TOUCH_UP:
      case INPUT_TOUCH_CANCEL: {
        ASSERT(event->type == INPUT_EVENT_TOUCH, return);
        const int index = lookup_touch(event->touch.id);
        if (index < 0) {
            return;
        }
        event->touch.initial_x = touches[index].initial_x;
        event->touch.initial_y = touches[index].initial_y;
        num_touches--;
        memmove(&touches[index], &touches[index+1],
                sizeof(*touches) * (num_touches - index));
        break;
      }  // case INPUT_TOUCH_UP, INPUT_TOUCH_CANCEL

    }  // switch (event->detail)

    if (event_callback) {
        event_callback(event);
    }
}

/*-----------------------------------------------------------------------*/

static void recompute_key_modifiers(int key)
{
    static const struct {
        int key1, key2;
        int modifier;
    } modifiers[] = {
        {KEY_LEFTSHIFT,   KEY_RIGHTSHIFT,   KEYMOD_SHIFT},
        {KEY_LEFTCONTROL, KEY_RIGHTCONTROL, KEYMOD_CONTROL},
        {KEY_LEFTALT,     KEY_RIGHTALT,     KEYMOD_ALT},
        {KEY_LEFTMETA,    KEY_RIGHTMETA,    KEYMOD_META},
        {KEY_LEFTSUPER,   KEY_RIGHTSUPER,   KEYMOD_SUPER},
        {KEY_NUMLOCK,     0,                KEYMOD_NUMLOCK},
        {KEY_CAPSLOCK,    0,                KEYMOD_CAPSLOCK},
        {KEY_SCROLLLOCK,  0,                KEYMOD_SCROLLLOCK},
    };

    int index = 0;
    for (index = 0; index < lenof(modifiers); index++) {
        if (key == modifiers[index].key1 || key == modifiers[index].key2) {
            break;
        }
    }
    if (index >= lenof(modifiers)) {
        return;
    }

    if (key_state[modifiers[index].key1]
     || (modifiers[index].key2 && key_state[modifiers[index].key2])) {
        key_modifier_state |= modifiers[index].modifier;
    } else {
        key_modifier_state &= ~modifiers[index].modifier;
    }
}

/*-----------------------------------------------------------------------*/

static int lookup_touch(unsigned int id)
{
    for (int i = 0; i < num_touches; i++) {
        if (touches[i].id == id) {
            return i;
        }
    }
    return -1;
}

/*-----------------------------------------------------------------------*/

static int ring_buffer_get(InputEvent *event_ret)
{
    if (event_buffer.next_out != event_buffer.next_in) {
        *event_ret = event_buffer.buffer[event_buffer.next_out];
        BARRIER();
        event_buffer.next_out =
            (event_buffer.next_out + 1) % lenof(event_buffer.buffer);
        BARRIER();
        return 1;
    } else {
        return 0;
    }
}

/*-----------------------------------------------------------------------*/

static int ring_buffer_put(const InputEvent *event)
{
    const int next_in_plus_1 =
        (event_buffer.next_in + 1) % lenof(event_buffer.buffer);
    if (next_in_plus_1 != event_buffer.next_out) {
        event_buffer.buffer[event_buffer.next_in] = *event;
        BARRIER();
        event_buffer.next_in = next_in_plus_1;
        BARRIER();
        return 1;
    } else {
        return 0;
    }
}

/*************************************************************************/
/*************************************************************************/
