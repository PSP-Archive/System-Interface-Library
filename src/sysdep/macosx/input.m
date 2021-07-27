/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/macosx/input.c: Input device interface for Mac OS X.
 */

#define IN_SYSDEP

#import "src/base.h"
#import "src/input.h"
#import "src/math.h"
#import "src/memory.h"
#import "src/sysdep.h"
#import "src/sysdep/darwin/time.h"
#import "src/sysdep/macosx/graphics.h"
#import "src/sysdep/macosx/input.h"
#import "src/sysdep/macosx/window.h"
#import "src/sysdep/misc/joystick-hid.h"
#import "src/time.h"

#import "src/sysdep/macosx/osx-headers.h"
#import <AppKit/NSEvent.h>
#import <AppKit/NSTextInputClient.h>
#import <AppKit/NSTextInputContext.h>
#import <AppKit/NSView.h>
#import <AppKit/NSWindow.h>
#import <Carbon/Carbon.h>
#import <ForceFeedback/ForceFeedback.h>
#import <ForceFeedback/ForceFeedbackConstants.h>
#import <IOKit/hid/IOHIDKeys.h>
#import <IOKit/hid/IOHIDLib.h>

/*************************************************************************/
/********************* Library-internal shared data **********************/
/*************************************************************************/

/* Flag set when a quit request is detected. */
uint8_t macosx_quit_requested = 0;

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/*--------------------------- Keycode mapping ---------------------------*/

/* Mapping from OS X virtual key codes to KEY_* symbols (defined as uint8_t
 * to save space, since currently all KEY_* values fit in 8 bits).  The
 * weird key order matches the order in <Carbon/HIToolbox/Events.h> --
 * yes, even the seemingly reversed-order entries... */
static const uint8_t vk_map[] = {
    [kVK_ANSI_A             ] = KEY_A,
    [kVK_ANSI_S             ] = KEY_S,
    [kVK_ANSI_D             ] = KEY_D,
    [kVK_ANSI_F             ] = KEY_F,
    [kVK_ANSI_H             ] = KEY_H,
    [kVK_ANSI_G             ] = KEY_G,
    [kVK_ANSI_Z             ] = KEY_Z,
    [kVK_ANSI_X             ] = KEY_X,
    [kVK_ANSI_C             ] = KEY_C,
    [kVK_ANSI_V             ] = KEY_V,
    [kVK_ANSI_B             ] = KEY_B,
    [kVK_ANSI_Q             ] = KEY_Q,
    [kVK_ANSI_W             ] = KEY_W,
    [kVK_ANSI_E             ] = KEY_E,
    [kVK_ANSI_R             ] = KEY_R,
    [kVK_ANSI_Y             ] = KEY_Y,
    [kVK_ANSI_T             ] = KEY_T,
    [kVK_ANSI_1             ] = KEY_1,
    [kVK_ANSI_2             ] = KEY_2,
    [kVK_ANSI_3             ] = KEY_3,
    [kVK_ANSI_4             ] = KEY_4,
    [kVK_ANSI_6             ] = KEY_6,
    [kVK_ANSI_5             ] = KEY_5,
    [kVK_ANSI_Equal         ] = KEY_EQUALS,
    [kVK_ANSI_9             ] = KEY_9,
    [kVK_ANSI_7             ] = KEY_7,
    [kVK_ANSI_Minus         ] = KEY_HYPHEN,
    [kVK_ANSI_8             ] = KEY_8,
    [kVK_ANSI_0             ] = KEY_0,
    [kVK_ANSI_RightBracket  ] = KEY_RIGHTBRACKET,
    [kVK_ANSI_O             ] = KEY_O,
    [kVK_ANSI_U             ] = KEY_U,
    [kVK_ANSI_LeftBracket   ] = KEY_LEFTBRACKET,
    [kVK_ANSI_I             ] = KEY_I,
    [kVK_ANSI_P             ] = KEY_P,
    [kVK_ANSI_L             ] = KEY_L,
    [kVK_ANSI_J             ] = KEY_J,
    [kVK_ANSI_Quote         ] = KEY_APOSTROPHE,
    [kVK_ANSI_K             ] = KEY_K,
    [kVK_ANSI_Semicolon     ] = KEY_SEMICOLON,
    [kVK_ANSI_Backslash     ] = KEY_BACKSLASH,
    [kVK_ANSI_Comma         ] = KEY_COMMA,
    [kVK_ANSI_Slash         ] = KEY_SLASH,
    [kVK_ANSI_N             ] = KEY_N,
    [kVK_ANSI_M             ] = KEY_M,
    [kVK_ANSI_Period        ] = KEY_PERIOD,
    [kVK_ANSI_Grave         ] = KEY_BACKQUOTE,
    [kVK_ANSI_KeypadDecimal ] = KEY_NUMPAD_DECIMAL,
    [kVK_ANSI_KeypadMultiply] = KEY_NUMPAD_MULTIPLY,
    [kVK_ANSI_KeypadPlus    ] = KEY_NUMPAD_ADD,
    [kVK_ANSI_KeypadClear   ] = 0,
    [kVK_ANSI_KeypadDivide  ] = KEY_NUMPAD_DIVIDE,
    [kVK_ANSI_KeypadEnter   ] = KEY_NUMPAD_ENTER,
    [kVK_ANSI_KeypadMinus   ] = KEY_NUMPAD_SUBTRACT,
    [kVK_ANSI_KeypadEquals  ] = KEY_NUMPAD_EQUALS,
    [kVK_ANSI_Keypad0       ] = KEY_NUMPAD_0,
    [kVK_ANSI_Keypad1       ] = KEY_NUMPAD_1,
    [kVK_ANSI_Keypad2       ] = KEY_NUMPAD_2,
    [kVK_ANSI_Keypad3       ] = KEY_NUMPAD_3,
    [kVK_ANSI_Keypad4       ] = KEY_NUMPAD_4,
    [kVK_ANSI_Keypad5       ] = KEY_NUMPAD_5,
    [kVK_ANSI_Keypad6       ] = KEY_NUMPAD_6,
    [kVK_ANSI_Keypad7       ] = KEY_NUMPAD_7,
    [kVK_ANSI_Keypad8       ] = KEY_NUMPAD_8,
    [kVK_ANSI_Keypad9       ] = KEY_NUMPAD_9,
    [kVK_Return             ] = KEY_ENTER,
    [kVK_Tab                ] = KEY_TAB,
    [kVK_Space              ] = KEY_SPACE,
    [kVK_Delete             ] = KEY_BACKSPACE,
    [kVK_Escape             ] = KEY_ESCAPE,
    [54                     ] = KEY_RIGHTMETA,  // should be kVK_RightCommand
    [kVK_Command            ] = KEY_LEFTMETA,
    [kVK_Shift              ] = KEY_LEFTSHIFT,
    [kVK_CapsLock           ] = KEY_CAPSLOCK,
    [kVK_Option             ] = KEY_LEFTALT,
    [kVK_Control            ] = KEY_LEFTCONTROL,
    [kVK_RightShift         ] = KEY_RIGHTSHIFT,
    [kVK_RightOption        ] = KEY_RIGHTALT,
    [kVK_RightControl       ] = KEY_RIGHTCONTROL,
    [kVK_Function           ] = 0,
    [kVK_F17                ] = KEY_F17,
    [kVK_VolumeUp           ] = 0,
    [kVK_VolumeDown         ] = 0,
    [kVK_Mute               ] = 0,
    [kVK_F18                ] = KEY_F18,
    [kVK_F19                ] = KEY_F19,
    [kVK_F20                ] = KEY_F20,
    [kVK_F5                 ] = KEY_F5,
    [kVK_F6                 ] = KEY_F6,
    [kVK_F7                 ] = KEY_F7,
    [kVK_F3                 ] = KEY_F3,
    [kVK_F8                 ] = KEY_F8,
    [kVK_F9                 ] = KEY_F9,
    [kVK_F11                ] = KEY_F11,
    [kVK_F13                ] = KEY_F13,
    [kVK_F16                ] = KEY_F16,
    [kVK_F14                ] = KEY_F14,
    [kVK_F10                ] = KEY_F10,
    [kVK_F12                ] = KEY_F12,
    [kVK_F15                ] = KEY_F15,
    [kVK_Help               ] = 0,
    [kVK_Home               ] = KEY_HOME,
    [kVK_PageUp             ] = KEY_PAGEUP,
    [kVK_ForwardDelete      ] = KEY_DELETE,
    [kVK_F4                 ] = KEY_F4,
    [kVK_End                ] = KEY_END,
    [kVK_F2                 ] = KEY_F2,
    [kVK_PageDown           ] = KEY_PAGEDOWN,
    [kVK_F1                 ] = KEY_F1,
    [kVK_LeftArrow          ] = KEY_LEFT,
    [kVK_RightArrow         ] = KEY_RIGHT,
    [kVK_DownArrow          ] = KEY_DOWN,
    [kVK_UpArrow            ] = KEY_UP,
    [kVK_ISO_Section        ] = 0,
    [kVK_JIS_Yen            ] = KEY_YEN,
    [kVK_JIS_Underscore     ] = KEY_UNDERSCORE,
    [kVK_JIS_KeypadComma    ] = KEY_COMMA,
    [kVK_JIS_Eisu           ] = 0,
    [kVK_JIS_Kana           ] = KEY_KANA,
};

/*------------------------- Other runtime data --------------------------*/

/* Is input grabbing currently requested? */
static uint8_t grab_requested;

/* Event callback passed to sys_input_init(). */
static InputEventCallback event_callback;


/* Mutex for accessing the joysticks[] and joystick_info[] arrays.  Note
 * that joystick discovery may cause the arrays to be reallocated, so
 * pointers to individual elements should not be maintained without the
 * mutex held. */
static SysMutexID joystick_mutex;

/* HID device manager handle. */
IOHIDManagerRef hid_manager;
/* Array of HID match expressions (created at runtime since it's a CFArray). */
CFArrayRef hid_match_array;

/* Allow joystick input when unfocused? */
static uint8_t joystick_ignore_focus;

/* Number of available joystick devices. */
static int num_joysticks;

/* Data for each joystick device. */
typedef struct JoystickInfo JoystickInfo;
struct JoystickInfo {
    /* Device handle. */
    IOHIDDeviceRef device;
    /* Handle for force-feedback effects, or NULL if the device does not
     * support force feedback. */
    FFDeviceObjectReference ff_device;
    /* Current force-feedback (rumble) effect handle, or NULL if no effect
     * is registered. */
    FFEffectObjectReference ff_effect;
    /* Handle for common HID processing. */
    HIDJoystickHandle *hid_handle;
};
static JoystickInfo *joysticks;

/* Joystick information array returned for sys_input_info(). */
static SysInputJoystick *joystick_info;


/* Set of keys currently pressed (for macosx_clear_window_input_state()). */
static uint8_t key_state[KEY__LAST];


/* Is the mouse currently grabbed? */
static uint8_t mouse_grabbed;
/* Did we just grab the mouse?  (This is used to suppress the first motion
 * event after a grab, which may include the delta from warping the mouse
 * pointer to the center of the window.) */
static uint8_t just_grabbed;
/* Current mouse coordinates (used while the mouse is grabbed). */
static Vector2f mouse_pos;
/* Current mouse button state (used to suppress spurious mouse-up events). */
static uint8_t mouse_lbutton, mouse_mbutton, mouse_rbutton;


/**
 * SILTextInputView:  This class implements the NSTextInputClient protocol
 * in order to translate keyboard events into text.  For the purposes of
 * the protocol, the class acts like an NSTextView whose contents are
 * always empty except for any marked text (i.e., not-yet-committed text
 * from the input method); conceptually, all committed text is instantly
 * "sucked up" by the input event handler and passed up to the high-level
 * input layer.
 */
@interface SILTextInputView: NSView <NSTextInputClient>
@end

/* SILTextInputView for text input.  If nil, text input is not active. */
static SILTextInputView *text_view;
/* Timestamp passed from the key event handler to the text input client;
 * set to -1 if not currently processing a key event. */
static double text_timestamp;

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * hid_device_added:  IOHIDManager callback for added devices.
 *
 * [Parameters]
 *     opaque: Not used.
 *     result: Not used.
 *     sender: IOHIDManager from which this event was generated (always
 *         hid_manager).
 *     device: HID device handle.
 */
static void hid_device_added(void *opaque, IOReturn result, void *sender,
                             IOHIDDeviceRef device);

/**
 * hid_device_removed:  IOHIDManager callback for added devices.
 *
 * [Parameters]
 *     opaque: Not used.
 *     result: Not used.
 *     sender: IOHIDManager from which this event was generated (always
 *         hid_manager).
 *     device: HID device handle.
 */
static void hid_device_removed(void *opaque, IOReturn result, void *sender,
                               IOHIDDeviceRef device);

/**
 * add_joystick:  Determine whether the given device is a joystick-type
 * device and, if so, add it to the joystick list.
 *
 * [Parameters]
 *     device: Raw input device handle.
 */
static void add_joystick(IOHIDDeviceRef device);

/**
 * get_hid_property_int:  Retrieve an integer property from a HID device
 * or element.
 *
 * [Parameters]
 *     handle: Device or element handle.
 *     property: Property ID (kIOHID*Key).
 * [Return value]
 *     Property value, or 0 if the property was not found.
 */
static int get_hid_property_int(void *handle, CFStringRef property);
#define get_hid_property_int(handle, property) \
    get_hid_property_int((handle), CFSTR(property))

/**
 * get_hid_property_string:  Retrieve a string property from a HID device
 * or element.  On success, the property value is returned in a newly
 * allocated buffer; the caller should free it with mem_free() when it is
 * no longer needed.
 *
 * [Parameters]
 *     handle: Device or element handle.
 *     property: Property ID (kIOHID*Key).
 * [Return value]
 *     Property value, or NULL if the property was not found.
 */
static char *get_hid_property_string(void *handle, CFStringRef property);
#define get_hid_property_string(handle, property) \
    get_hid_property_string((handle), CFSTR(property))

/**
 * joystick_input_callback:  IOHIDDevice callback for input events.
 *
 * [Parameters]
 *     opaque: joysticks[] index of the device, cast to a pointer.
 *     result: Not used.
 *     sender: IOHIDDevice from which this event was generated.
 *     value: IOHIDValue instance for the changed value.
 */
static void joystick_input_callback(void *opaque, IOReturn result,
                                    void *sender, IOHIDValueRef value);

/**
 * send_joystick_connect_event:  Generate a joystick connection or
 * disconnection event.
 *
 * [Parameters]
 *     device: Device index.
 *     detail: Event detail code (INPUT_JOYSTICK_CONNECTED or
 *         INPUT_JOYSTICK_DISCONNECTED).
 */
static void send_joystick_connect_event(int device, InputEventDetail detail);

/**
 * convert_mouse_coords:  Convert mouse coordinates from a mouse event to
 * coordinates expected by the SIL input subsystem.
 *
 * For thread safety, this should only be called from input event handlers.
 *
 * [Parameters]
 *     event: Cocoa mouse event from which to take mouse coordinates.
 *     x_out, y_out: Pointers to variables to receive the converted
 *         floating-point mouse coordinates.
 * [Return value]
 *     True if the original coordinates were within the window bounds,
 *     false if not.
 */
static int convert_mouse_coords(NSEvent *event, float *x_out, float *y_out);

/**
 * Convert NSAttributedString objects to NSString while leaving NSString
 * instances alone.  Helper function for NSTextInputClient protocol methods
 * which take an object of either type.
 *
 * [Parameters]
 *     aString: NSString or NSAttributedString instance.
 * [Return value]
 *     aString if it is an NSString instance; [aString string] if aString
 *     is an NSAttributedString instance.
 */
static NSString *deattribute_string(id aString);

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int sys_input_init(void (*event_callback_)(const struct InputEvent *))
{
    PRECOND(event_callback_ != NULL, return 0);

    event_callback = event_callback_;

    joystick_mutex = sys_mutex_create(0, 0);
    if (UNLIKELY(!joystick_mutex)) {
        DLOG("Failed to create joystick mutex");
        goto error_return;
    }

    hid_match_array = (CFArrayRef)[@[
        @{@kIOHIDDeviceUsagePageKey: @HID_PAGE_GENERIC_DESKTOP,
          @kIOHIDDeviceUsageKey: @HID_USAGE_JOYSTICK},
        @{@kIOHIDDeviceUsagePageKey: @HID_PAGE_GENERIC_DESKTOP,
          @kIOHIDDeviceUsageKey: @HID_USAGE_GAMEPAD},
    ] retain];
    if (UNLIKELY(!hid_match_array)) {
        DLOG("Failed to create HID match array");
        goto error_destroy_joystick_mutex;
    }

    hid_manager = IOHIDManagerCreate(kCFAllocatorDefault, 0);
    if (UNLIKELY(!hid_manager)) {
        DLOG("Failed to create HID manager");
        goto error_destroy_hid_match_array;
    }
    IOHIDManagerSetDeviceMatchingMultiple(hid_manager, hid_match_array);
    IOHIDManagerRegisterDeviceMatchingCallback(hid_manager,
                                               hid_device_added, NULL);
    IOHIDManagerRegisterDeviceRemovalCallback(hid_manager,
                                              hid_device_removed, NULL);
    IOHIDManagerScheduleWithRunLoop(hid_manager, CFRunLoopGetMain(),
                                    kCFRunLoopDefaultMode);
    int result = IOHIDManagerOpen(hid_manager, 0);
    if (result != kIOReturnSuccess) {
        DLOG("Failed to open HID manager");
        goto error_destroy_hid_manager;
    }

    grab_requested = 0;
    joystick_ignore_focus = 1;
    num_joysticks = 0;
    joystick_info = NULL;
    mem_clear(key_state, sizeof(key_state));
    mouse_grabbed = 0;
    just_grabbed = 0;
    mouse_pos.x = 0.5;
    mouse_pos.y = 0.5;
    mouse_lbutton = 0;
    mouse_mbutton = 0;
    mouse_rbutton = 0;
    text_view = nil;
    text_timestamp = -1;

    return 1;

  error_destroy_hid_manager:
    CFRelease(hid_manager);
  error_destroy_hid_match_array:
    CFRelease(hid_match_array);
  error_destroy_joystick_mutex:
    sys_mutex_destroy(joystick_mutex);
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

void sys_input_cleanup(void)
{
    CFRelease(hid_manager);
    hid_manager = NULL;
    CFRelease(hid_match_array);
    hid_match_array = NULL;

    for (int i = 0; i < num_joysticks; i++) {
        if (joysticks[i].ff_device) {
            if (joysticks[i].ff_effect) {
                FFEffectStop(joysticks[i].ff_effect);
                FFDeviceReleaseEffect(joysticks[i].ff_device,
                                      joysticks[i].ff_effect);
            }
            FFReleaseDevice(joysticks[i].ff_device);
        }
        hidjoy_destroy(joysticks[i].hid_handle);
    }
    mem_free(joysticks);
    joysticks = NULL;
    num_joysticks = 0;
    mem_free(joystick_info);
    joystick_info = NULL;
    sys_mutex_destroy(joystick_mutex);
    joystick_mutex = 0;
}

/*-----------------------------------------------------------------------*/

void sys_input_update(void)
{
    /* Cocoa events are handled by the window callbacks. */

    /* Update grab state in case a grab was previously requested while the
     * window was being moved. */
    if (macosx_window() && sys_graphics_has_focus()) {
        macosx_update_mouse_grab(-1);
    }

    /* Send out pending joystick events.  We do this here instead of in
     * the value callback so that simultaneous stick X and Y axis changes
     * are properly merged into seingle events. */
    sys_mutex_lock(joystick_mutex, -1);
    for (int i = 0; i < num_joysticks; i++) {
        if (joystick_info[i].connected) {
            hidjoy_flush_events(joysticks[i].hid_handle);
        }
    }
    sys_mutex_unlock(joystick_mutex);
}

/*-----------------------------------------------------------------------*/

void sys_input_info(SysInputInfo *info_ret)
{
    info_ret->has_joystick = 1;
    info_ret->num_joysticks = num_joysticks;
    info_ret->joysticks = joystick_info;

    /* Assume a keyboard and mouse are present. */
    info_ret->has_keyboard = 1;
    info_ret->keyboard_is_full = 1;
    info_ret->has_mouse = 1;

    /* We provide text entry by sending keystrokes through an input context. */
    info_ret->has_text = 1;
    info_ret->text_uses_custom_interface = 0;
    info_ret->text_has_prompt = 0;

    /* We don't support touch events yet. */
    info_ret->has_touch = 0;
}

/*-----------------------------------------------------------------------*/

void sys_input_grab(int grab)
{
    grab_requested = (grab != 0);
    macosx_update_mouse_grab(-1);
}

/*-----------------------------------------------------------------------*/

int sys_input_is_quit_requested(void)
{
    return macosx_quit_requested;
}

/*-----------------------------------------------------------------------*/

int sys_input_is_suspend_requested(void)
{
    return 0;
}

/*-----------------------------------------------------------------------*/

void sys_input_acknowledge_suspend_request(void)
{
}

/*************************************************************************/
/********************* Interface: Joystick handling **********************/
/*************************************************************************/

void sys_input_enable_unfocused_joystick(int enable)
{
    joystick_ignore_focus = (enable != 0);
}

/*-----------------------------------------------------------------------*/

char *sys_input_joystick_copy_name(int index)
{
    char *retval;
    sys_mutex_lock(joystick_mutex, -1);
    if (joystick_info[index].connected) {
        retval = mem_strdup(hidjoy_name(joysticks[index].hid_handle), 0);
    } else {
        /* The device must have been removed since the last call to
         * sys_input_info(). */
        retval = NULL;
    }
    sys_mutex_unlock(joystick_mutex);
    return retval;
}

/*-----------------------------------------------------------------------*/

int sys_input_joystick_button_mapping(int index, int name)
{
    int retval;
    sys_mutex_lock(joystick_mutex, -1);
    if (joystick_info[index].connected) {
        retval = hidjoy_button_mapping(joysticks[index].hid_handle, name);
    } else {
        retval = -1;
    }
    sys_mutex_unlock(joystick_mutex);
    return retval;
}

/*-----------------------------------------------------------------------*/

void sys_input_joystick_rumble(int index, float left, float right, float time)
{
    sys_mutex_lock(joystick_mutex, -1);
    ASSERT(index < num_joysticks);  // num_joysticks never decreases.
    JoystickInfo *joystick = &joysticks[index];

    if (!joystick->ff_device) {
        /* The device doesn't support rumble, or it was asynchronously
         * removed since the last call to sys_input_info(). */
        goto out;
    }

    if (joysticks[index].ff_effect != NULL) {
        FFEffectStop(joystick->ff_effect);
        FFDeviceReleaseEffect(joystick->ff_device, joystick->ff_effect);
        joystick->ff_effect = NULL;
    }

    FFEFFECT effect;
    mem_clear(&effect, sizeof(effect));
    effect.dwSize = sizeof(effect);
    effect.dwFlags = 0;
    effect.dwDuration = iroundf(time * 1000000);
    effect.dwTriggerButton = FFEB_NOTRIGGER;
    /* OS X doesn't have a "rumble" effect, so we do like Linux (ff-core.c)
     * and simulate it with a periodic sine wave.  Unfortunately, this
     * means we can't use different strengths for the left and right
     * motors. */
    FFPERIODIC periodic;
    mem_clear(&periodic, sizeof(periodic));
    periodic.dwMagnitude = iroundf(10000 * ((left + right) / 2));
    periodic.dwPeriod = 50000;
    effect.cbTypeSpecificParams = sizeof(periodic);
    effect.lpvTypeSpecificParams = &periodic;
    HRESULT result = FFDeviceCreateEffect(
        joystick->ff_device, kFFEffectType_Sine_ID, &effect,
        &joystick->ff_effect);
    if (UNLIKELY(result != FF_OK)) {
        DLOG("Failed to create force feedback effect: %d", result);
        joystick->ff_effect = NULL;
        goto out;
    }

    result = FFEffectStart(joystick->ff_effect, 1, 0);
    if (UNLIKELY(result != FF_OK)) {
        DLOG("Failed to start force feedback effect: %d", result);
        FFDeviceReleaseEffect(joystick->ff_device, joystick->ff_effect);
        joystick->ff_effect = NULL;
        goto out;
    }

  out:
    sys_mutex_unlock(joystick_mutex);
}

/*************************************************************************/
/*********************** Interface: Mouse handling ***********************/
/*************************************************************************/

void sys_input_mouse_set_position(float x, float y)
{
    if (!macosx_window()) {
        return;
    }

    const CGRect frame = SILWindow_content_frame(macosx_window());
    const int width = iroundf(frame.size.width);
    const int height = iroundf(frame.size.height);
    const int ix = bound(iroundf(x*width),  0, width-1);
    const int iy = bound(iroundf(y*height), 0, height-1);

    /* Don't move the system mouse pointer if input is grabbed (we keep it
     * centered in the window in that case). */
    if (!mouse_grabbed) {
        CGWarpMouseCursorPosition((CGPoint){ix + iroundf(frame.origin.x),
                                            iy + iroundf(frame.origin.y)});
        /* Normally, OS X suppresses mouse movement events for 250
         * milliseconds after a WarpMouse call, but we can get around that
         * by manually reassociating mouse input with the pointer. */
        CGAssociateMouseAndMouseCursorPosition(1);
    }

    mouse_pos.x = (float)ix / (float)width;
    mouse_pos.y = (float)iy / (float)height;
    (*event_callback)(&(InputEvent){
        .type = INPUT_EVENT_MOUSE, .detail = INPUT_MOUSE_MOVE,
        .timestamp = time_now(),
        {.mouse = {.x = mouse_pos.x, .y = mouse_pos.y}}});
}

/*************************************************************************/
/******************** Interface: Text entry handling *********************/
/*************************************************************************/

/* Helper to call window/view methods on main thread. */
static void do_text_set_state(void *param)
{
    NSWindow *window = macosx_window();
    ASSERT(window, return);

    const int on = (param != NULL);

    if (on) {
        text_view = [[SILTextInputView alloc]
                        initWithFrame:(NSRect){{0,0},{0,0}}];
        [[window contentView] addSubview:text_view];
        [window makeFirstResponder:text_view];
    } else if (!on) {
        [text_view removeFromSuperview];
        [text_view release];
        text_view = nil;
    }
}

void sys_input_text_set_state(int on, UNUSED const char *text,
                              UNUSED const char *prompt)
{
    NSWindow *window = macosx_window();
    if (!window) {
        return;
    }

    if (on && !text_view) {
        dispatch_sync_f(dispatch_get_main_queue(),
                        (void *)1, do_text_set_state);
    } else if (!on && text_view) {
        dispatch_sync_f(dispatch_get_main_queue(),
                        NULL, do_text_set_state);
    }
}

/*************************************************************************/
/******************** SILTextInputView implementation ********************/
/*************************************************************************/

@implementation SILTextInputView {
    NSString *marked_text;
    NSRange selection_range;
}

/*--------------------- Initialization and cleanup ----------------------*/

- (id)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    marked_text = nil;
    selection_range = (NSRange){NSNotFound, 0};
    return self;
}


- (oneway void)dealloc
{
    [marked_text release];
    [super dealloc];
}

/*---------------------- NSTextInputClient methods ----------------------*/

- (BOOL)hasMarkedText
{
    return marked_text != nil;
}


- (NSRange)markedRange
{
    if (marked_text) {
        return (NSRange){0, marked_text.length};
    } else {
        return (NSRange){NSNotFound, 0};
    }
}


- (NSRange)selectedRange
{
    return selection_range;
}


- (void)setMarkedText:(id)aString selectedRange:(NSRange)selectedRange
                                  replacementRange:(NSRange)replacementRange
{
    NSString *string = deattribute_string(aString);
    if (marked_text) {
        /* This is undocumented, but a replacementRange of {NSNotFound,0}
         * seems to be used to mean "the whole string" (observed with the
         * standard Japanese IME). */
        NSString *new_text;
        if (replacementRange.location == NSNotFound) {
            ASSERT(replacementRange.length == 0);
            new_text = [NSString stringWithString:string];
        } else {
            new_text = [marked_text
                           stringByReplacingCharactersInRange:replacementRange
                           withString:string];
        }
        [marked_text release];
        marked_text = new_text;
    } else {
        marked_text = [NSString stringWithString:string];
    }
    [marked_text retain];
    selection_range = selectedRange;
}


- (void)unmarkText
{
    if (marked_text) {
        InputEvent event = {
            .type = INPUT_EVENT_TEXT, .detail = INPUT_TEXT_INPUT,
            .timestamp = text_timestamp};
        ASSERT(event.timestamp >= 0, event.timestamp = time_now());
        for (int i = 0; i < (int)marked_text.length; i++) {
            event.text.ch = [marked_text characterAtIndex:i];
            (*event_callback)(&event);
        }
        [marked_text release];
        marked_text = nil;
        selection_range = (NSRange){NSNotFound, 0};
    }
}


- (NSArray *)validAttributesForMarkedText
{
    return [NSArray array];
}


- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange) UNUSED range
                        actualRange:(NSRangePointer) UNUSED actualRange
{
    return nil;  // The "document" is always empty for our purposes.
}


- (void)insertText:(id)aString
    replacementRange:(NSRange) UNUSED replacementRange
{
    NSString *string = deattribute_string(aString);
    InputEvent event = {
        .type = INPUT_EVENT_TEXT, .detail = INPUT_TEXT_INPUT,
        .timestamp = text_timestamp};
    ASSERT(event.timestamp >= 0, event.timestamp = time_now());
    for (int i = 0; i < (int)string.length; i++) {
        event.text.ch = [string characterAtIndex:i];
        (*event_callback)(&event);
    }
    /* The standard Japanese IME calls this method instead of -[unmarkText]
     * to commit an input string. */
    if (marked_text) {
        [marked_text release];
        marked_text = nil;
        selection_range = (NSRange){NSNotFound, 0};
    }
}


- (NSUInteger)characterIndexForPoint:(NSPoint) UNUSED point
{
    return NSNotFound;
}


- (NSRect) firstRectForCharacterRange:(NSRange) UNUSED range
           actualRange:(NSRangePointer) UNUSED actualRange
{
    return (NSRect){{0,0}, {0,0}};
}


- (void)doCommandBySelector:(SEL)selector
{
    if ([self respondsToSelector:selector]) {
        [self performSelector:selector withObject:self];
    }
}

/*------ NSResponder methods (called via -[doCommandBySelector:]) -------*/

- (void)insertNewline:(id) UNUSED sender  // i.e. Enter key
{
    InputEvent event = {
        .type = INPUT_EVENT_TEXT, .detail = INPUT_TEXT_DONE,
        .timestamp = text_timestamp};
    ASSERT(event.timestamp >= 0, event.timestamp = time_now());
    (*event_callback)(&event);
}


- (void)cancelOperation:(id) UNUSED sender
{
    InputEvent event = {
        .type = INPUT_EVENT_TEXT, .detail = INPUT_TEXT_CANCELLED,
        .timestamp = text_timestamp};
    ASSERT(event.timestamp >= 0, event.timestamp = time_now());
    (*event_callback)(&event);
}


- (void)deleteBackward:(id) UNUSED sender
{
    InputEvent event = {
        .type = INPUT_EVENT_TEXT, .detail = INPUT_TEXT_BACKSPACE,
        .timestamp = text_timestamp};
    ASSERT(event.timestamp >= 0, event.timestamp = time_now());
    (*event_callback)(&event);
}


- (void)deleteForward:(id) UNUSED sender
{
    InputEvent event = {
        .type = INPUT_EVENT_TEXT, .detail = INPUT_TEXT_DELETE,
        .timestamp = text_timestamp};
    ASSERT(event.timestamp >= 0, event.timestamp = time_now());
    (*event_callback)(&event);
}


- (void)moveLeft:(id) UNUSED sender
{
    InputEvent event = {
        .type = INPUT_EVENT_TEXT, .detail = INPUT_TEXT_CURSOR_LEFT,
        .timestamp = text_timestamp};
    ASSERT(event.timestamp >= 0, event.timestamp = time_now());
    (*event_callback)(&event);
}


- (void)moveRight:(id) UNUSED sender
{
    InputEvent event = {
        .type = INPUT_EVENT_TEXT, .detail = INPUT_TEXT_CURSOR_RIGHT,
        .timestamp = text_timestamp};
    ASSERT(event.timestamp >= 0, event.timestamp = time_now());
    (*event_callback)(&event);
}


- (void)moveToBeginningOfLine:(id) UNUSED sender
{
    InputEvent event = {
        .type = INPUT_EVENT_TEXT, .detail = INPUT_TEXT_CURSOR_HOME,
        .timestamp = text_timestamp};
    ASSERT(event.timestamp >= 0, event.timestamp = time_now());
    (*event_callback)(&event);
}
/* Aliases: */
- (void)moveToBeginningOfDocument:(id)sender {
    [self moveToBeginningOfLine:sender];
}
- (void)scrollToBeginningOfDocument:(id)sender { // Default binding of Home key
    [self moveToBeginningOfLine:sender];
}


- (void)moveToEndOfLine:(id) UNUSED sender
{
    InputEvent event = {
        .type = INPUT_EVENT_TEXT, .detail = INPUT_TEXT_CURSOR_END,
        .timestamp = text_timestamp};
    ASSERT(event.timestamp >= 0, event.timestamp = time_now());
    (*event_callback)(&event);
}
/* Aliases: */
- (void)moveToEndOfDocument:(id)sender {
    [self moveToEndOfLine:sender];
}
- (void)scrollToEndOfDocument:(id)sender {  // Default binding of End key
    [self moveToEndOfLine:sender];
}

/*-----------------------------------------------------------------------*/

@end  // SILTextInputView

/*************************************************************************/
/*********************** Internal utility routines ***********************/
/*************************************************************************/

void macosx_update_mouse_grab(int grab)
{
    void *window = macosx_window();
    if (!window) {
        return;
    }

    if (grab < 0) {
        grab = grab_requested;
    }

    const CGRect frame = SILWindow_content_frame(window);
    if (grab && !mouse_grabbed) {
        /* Don't try to grab while the window is being moved, since the
         * WarpMouse call would move the window as well. */
        if (SILWindow_is_moving(window)) {
            return;
        }
        CGAssociateMouseAndMouseCursorPosition(0);
        CGWarpMouseCursorPosition(
            (CGPoint){frame.origin.x + frame.size.width / 2,
                      frame.origin.y + frame.size.height / 2});
        mouse_grabbed = 1;
        just_grabbed = 1;
    } else if (!grab && mouse_grabbed) {
        CGWarpMouseCursorPosition(
            (CGPoint){frame.origin.x + mouse_pos.x * frame.size.width,
                      frame.origin.y + mouse_pos.y * frame.size.height});
        CGAssociateMouseAndMouseCursorPosition(1);
        mouse_grabbed = 0;
    }
}

/*-----------------------------------------------------------------------*/

void macosx_handle_mouse_event(void *cocoa_event_)
{
    NSEvent *cocoa_event = cocoa_event_;

    InputEventDetail detail;
    switch ([cocoa_event type]) {
        case NSLeftMouseDown:  detail = INPUT_MOUSE_LMB_DOWN; break;
        case NSOtherMouseDown: detail = INPUT_MOUSE_MMB_DOWN; break;
        case NSRightMouseDown: detail = INPUT_MOUSE_RMB_DOWN; break;
        case NSLeftMouseUp:    detail = INPUT_MOUSE_LMB_UP; break;
        case NSOtherMouseUp:   detail = INPUT_MOUSE_MMB_UP; break;
        case NSRightMouseUp:   detail = INPUT_MOUSE_RMB_UP; break;
        default:               detail = INPUT_MOUSE_MOVE; break;
    }

    if (mouse_grabbed) {
        if (detail == INPUT_MOUSE_MOVE) {
            if (just_grabbed) {
                just_grabbed = 0;
            } else {
                /* There must be a window open for this function to have
                 * been called. */
                ASSERT(macosx_window());
                const CGRect frame = SILWindow_content_frame(macosx_window());
                const int width = iroundf(frame.size.width);
                const int height = iroundf(frame.size.height);
                const int x = iroundf((mouse_pos.x * width)
                                      + [cocoa_event deltaX]);
                const int y = iroundf((mouse_pos.y * height)
                                      + [cocoa_event deltaY]);
                mouse_pos.x = bound(x, 0, width-1) / (float)width;
                mouse_pos.y = bound(y, 0, height-1) / (float)height;
            }
        }
    } else {
        const int in_bounds =
            convert_mouse_coords(cocoa_event, &mouse_pos.x, &mouse_pos.y);
        /* Suppress mouse-down events outside the window bounds (such as
         * in the title bar) because OSX 10.10 through 10.12 pass
         * mouse-down but not mouse-up events during a title-bar-drag
         * window move action. */
        if (!in_bounds) {
            if (detail == INPUT_MOUSE_LMB_DOWN
             || detail == INPUT_MOUSE_MMB_DOWN
             || detail == INPUT_MOUSE_RMB_DOWN) {
                return;
            }
        }
    }

    /* Suppress mouse-up events for buttons we haven't reported as down. */
    switch (detail) {
      case INPUT_MOUSE_LMB_DOWN:
        mouse_lbutton = 1;
        break;
      case INPUT_MOUSE_MMB_DOWN:
        mouse_mbutton = 1;
        break;
      case INPUT_MOUSE_RMB_DOWN:
        mouse_rbutton = 1;
        break;
      case INPUT_MOUSE_LMB_UP:
        if (!mouse_lbutton) {
            return;
        }
        mouse_lbutton = 0;
        break;
      case INPUT_MOUSE_MMB_UP:
        if (!mouse_mbutton) {
            return;
        }
        mouse_mbutton = 0;
        break;
      case INPUT_MOUSE_RMB_UP:
        if (!mouse_rbutton) {
            return;
        }
        mouse_rbutton = 0;
        break;
      default:
        break;
    }

    (*event_callback)(&(InputEvent){
        .type = INPUT_EVENT_MOUSE, .detail = detail,
        .timestamp = [cocoa_event timestamp] - darwin_time_epoch(),
        {.mouse = {.x = mouse_pos.x, .y = mouse_pos.y, .scroll = 0}}});
}

/*-----------------------------------------------------------------------*/

void macosx_handle_scroll_event(void *cocoa_event_)
{
    NSEvent *cocoa_event = cocoa_event_;

    float dx = [cocoa_event deltaX];
    float dy = [cocoa_event deltaY];
    if ([cocoa_event hasPreciseScrollingDeltas]) {
        CGEventRef carbon_event = [cocoa_event CGEvent];
        if (carbon_event) {
            CGEventSourceRef source =
                CGEventCreateSourceFromEvent(carbon_event);
            if (source) {
                const double scale = CGEventSourceGetPixelsPerLine(source);
                if (scale > 0) {
                    dx = [cocoa_event scrollingDeltaX] / (float)scale;
                    dy = [cocoa_event scrollingDeltaY] / (float)scale;
                }
                CFRelease(source);
            }
        }
    }

    if (dx) {
        (*event_callback)(&(InputEvent){
            .type = INPUT_EVENT_MOUSE, .detail = INPUT_MOUSE_SCROLL_H,
            .timestamp = [cocoa_event timestamp] - darwin_time_epoch(),
            {.mouse = {.x = mouse_pos.x, .y = mouse_pos.y, .scroll = dx}}});
    }
    if (dy) {
        (*event_callback)(&(InputEvent){
            .type = INPUT_EVENT_MOUSE, .detail = INPUT_MOUSE_SCROLL_V,
            .timestamp = [cocoa_event timestamp] - darwin_time_epoch(),
            {.mouse = {.x = mouse_pos.x, .y = mouse_pos.y, .scroll = dy}}});
    }
}

/*-----------------------------------------------------------------------*/

void macosx_handle_key_event(void *cocoa_event_)
{
    NSEvent *cocoa_event = cocoa_event_;

    const int keycode = [cocoa_event keyCode];
    int sil_key = KEY__NONE;
    if (keycode >= 0 && keycode < lenof(vk_map)) {
        sil_key = vk_map[keycode];
    }
    InputEvent event = {
        .type = INPUT_EVENT_KEYBOARD,
        .timestamp = [cocoa_event timestamp] - darwin_time_epoch(),
        {.keyboard = {.key = sil_key, .system_key = keycode}}};

    const int type = [cocoa_event type];
    if (type == NSFlagsChanged) {
        /* This is stupidly complex and dependent on undocumented flags because
         * OS X doesn't give us the pressed/released state of the key. */
        unsigned int mask = 0;
        switch (keycode) {
            case 54:               mask = 0x0010; break;  // kVK_RightCommand
            case kVK_Command:      mask = 0x0008; break;
            case kVK_Shift:        mask = 0x0002; break;
            case kVK_CapsLock:     mask = NSAlphaShiftKeyMask; break;
            case kVK_Option:       mask = 0x0020; break;
            case kVK_Control:      mask = 0x0001; break;
            case kVK_RightShift:   mask = 0x0004; break;
            case kVK_RightOption:  mask = 0x0040; break;
            case kVK_RightControl: mask = 0x2000; break;
        }
        if (!mask) {
            event.detail = 0;  // Unknown key, so we can't determine its state.
        } else if ([cocoa_event modifierFlags] & mask) {
            event.detail = INPUT_KEYBOARD_KEY_DOWN;
        } else {
            event.detail = INPUT_KEYBOARD_KEY_UP;
        }
        event.keyboard.is_repeat = 0;
    } else if (type == NSKeyDown) {
        event.detail = INPUT_KEYBOARD_KEY_DOWN;
        event.keyboard.is_repeat = [cocoa_event isARepeat];
    } else {
        event.detail = INPUT_KEYBOARD_KEY_UP;
        event.keyboard.is_repeat = 0;
    }

    if (event.detail) {
        if (sil_key) {
            ASSERT(sil_key >= 0 && sil_key < lenof(key_state), return);
            if (event.detail == INPUT_KEYBOARD_KEY_DOWN) {
                key_state[sil_key] = 1;
            } else {
                if (!key_state[sil_key]) {
                    return;  // Spurious key-up event.
                }
                key_state[sil_key] = 0;
            }
        } else {
            event.detail += (INPUT_KEYBOARD_SYSTEM_KEY_DOWN
                             - INPUT_KEYBOARD_KEY_DOWN);
        }
        (*event_callback)(&event);
    }

    if (type == NSKeyDown && text_view && macosx_window()) {
        text_timestamp = event.timestamp;
        [text_view interpretKeyEvents:[NSArray arrayWithObject:cocoa_event]];
        text_timestamp = -1;
    }
}

/*-----------------------------------------------------------------------*/

void macosx_clear_window_input_state(void)
{
    const double timestamp = time_now();

    for (int vk = 0; vk < lenof(vk_map); vk++) {
        const int key = vk_map[vk];
        if (key && key_state[key]) {
            (*event_callback)(&(InputEvent){
                .type = INPUT_EVENT_KEYBOARD, .detail = INPUT_KEYBOARD_KEY_UP,
                .timestamp = timestamp,
                {.keyboard = {.key = key, .system_key = vk, .is_repeat = 0}}});
            key_state[key] = 0;
        }
    }

    InputEvent mouse_event = {
        .type = INPUT_EVENT_MOUSE, .timestamp = timestamp,
        {.mouse = {.x = mouse_pos.x, .y = mouse_pos.y}}};
    if (mouse_lbutton) {
        mouse_event.detail = INPUT_MOUSE_LMB_UP;
        (*event_callback)(&mouse_event);
        mouse_lbutton = 0;
    }
    if (mouse_mbutton) {
        mouse_event.detail = INPUT_MOUSE_MMB_UP;
        (*event_callback)(&mouse_event);
        mouse_mbutton = 0;
    }
    if (mouse_rbutton) {
        mouse_event.detail = INPUT_MOUSE_RMB_UP;
        (*event_callback)(&mouse_event);
        mouse_rbutton = 0;
    }
}

/*************************************************************************/
/*************************** Joystick handling ***************************/
/*************************************************************************/

static void hid_device_added(UNUSED void *opaque, UNUSED IOReturn result,
                             void *sender, IOHIDDeviceRef device)
{
    ASSERT(sender == hid_manager);
    add_joystick(device);
}

/*-----------------------------------------------------------------------*/

static void hid_device_removed(UNUSED void *opaque, UNUSED IOReturn result,
                               void *sender, IOHIDDeviceRef device)
{
    ASSERT(sender == hid_manager);

    sys_mutex_lock(joystick_mutex, -1);
    for (int i = 0; i < num_joysticks; i++) {
        if (joysticks[i].device == device) {
            DLOG("Joystick %d (%s, %04X/%04X, serial %s) disconnected",
                 i, hidjoy_name(joysticks[i].hid_handle),
                 hidjoy_vendor_id(joysticks[i].hid_handle),
                 hidjoy_product_id(joysticks[i].hid_handle),
                 hidjoy_serial(joysticks[i].hid_handle));
            hidjoy_flush_events(joysticks[i].hid_handle);
            if (joysticks[i].ff_device) {
                if (joysticks[i].ff_effect) {
                    FFDeviceReleaseEffect(joysticks[i].ff_device,
                                          joysticks[i].ff_effect);
                    joysticks[i].ff_effect = NULL;
                }
                FFReleaseDevice(joysticks[i].ff_device);
                joysticks[i].ff_device = NULL;
            }
            joysticks[i].device = NULL;
            send_joystick_connect_event(i, INPUT_JOYSTICK_DISCONNECTED);
            break;
        }
    }
    sys_mutex_unlock(joystick_mutex);
}

/*-----------------------------------------------------------------------*/

static void add_joystick(IOHIDDeviceRef device)
{
    /* See if this is a device that was previously disconnected. */
    const int vendor_id = get_hid_property_int(device, kIOHIDVendorIDKey);
    const int product_id = get_hid_property_int(device, kIOHIDProductIDKey);
    char *serial = get_hid_property_string(device, kIOHIDSerialNumberKey);
    sys_mutex_lock(joystick_mutex, -1);
    for (int i = 0; i < num_joysticks; i++) {
        if (hidjoy_vendor_id(joysticks[i].hid_handle) == vendor_id
         && hidjoy_product_id(joysticks[i].hid_handle) == product_id
         && (serial ? (hidjoy_serial(joysticks[i].hid_handle)
                       && strcmp(hidjoy_serial(joysticks[i].hid_handle),
                                 serial) == 0)
                    : !hidjoy_serial(joysticks[i].hid_handle))) {
            if (joystick_info[i].connected && joysticks[i].device == device) {
                /* We're already watching this device. */
                sys_mutex_unlock(joystick_mutex);
                mem_free(serial);
                return;
            } else if (!joystick_info[i].connected) {
                joysticks[i].device = device;
                joystick_info[i].connected = 1;
                send_joystick_connect_event(i, INPUT_JOYSTICK_CONNECTED);
                DLOG("Joystick %d (%s: %04X/%04X, serial %s) reconnected",
                     i, hidjoy_name(joysticks[i].hid_handle),
                     hidjoy_vendor_id(joysticks[i].hid_handle),
                     hidjoy_product_id(joysticks[i].hid_handle),
                     hidjoy_serial(joysticks[i].hid_handle));
                if (joystick_info[i].can_rumble) {
                    io_service_t port = IOHIDDeviceGetService(device);
                    FFDeviceObjectReference ff_device;
                    if (LIKELY(FFCreateDevice(port, &ff_device) == FF_OK)) {
                        joysticks[i].ff_device = ff_device;
                    } else {
                        DLOG("Failed to reopen force feedback interface,"
                             " disabling rumble");
                        joystick_info[i].can_rumble = 0;
                    }
                }
                sys_mutex_unlock(joystick_mutex);
                mem_free(serial);
                return;
            }
        }
    }
    sys_mutex_unlock(joystick_mutex);

    /* Create a generic HID joystick handle for the device. */
    HIDJoystickInfo hid_info;
    mem_clear(&hid_info, sizeof(hid_info));
    hid_info.vendor_id = vendor_id;
    hid_info.product_id = product_id;
    hid_info.dev_version = get_hid_property_int(device, kIOHIDVersionNumberKey);
    hid_info.name = get_hid_property_string(device, kIOHIDProductKey);
    hid_info.serial = serial;

    CFArrayRef elements = IOHIDDeviceCopyMatchingElements(device, NULL, 0);
    if (UNLIKELY(!elements)) {
        DLOG("%s (%04X/%04X): Failed to read element list",
             hid_info.name, hid_info.vendor_id, hid_info.product_id);
        mem_free(hid_info.serial);
        mem_free(hid_info.name);
        return;
    }
    const int num_elements = CFArrayGetCount(elements);
    if (UNLIKELY(!num_elements)) {
        DLOG("%s (%04X/%04X): No elements found",
             hid_info.name, hid_info.vendor_id, hid_info.product_id);
        CFRelease(elements);
        mem_free(hid_info.serial);
        mem_free(hid_info.name);
        return;
    }

    hid_info.buttons = mem_alloc(sizeof(*hid_info.buttons) * num_elements, 0,
                                 MEM_ALLOC_TEMP);
    hid_info.values = mem_alloc(sizeof(*hid_info.values) * num_elements, 0,
                                MEM_ALLOC_TEMP);
    if (UNLIKELY(!hid_info.buttons) || UNLIKELY(!hid_info.values)) {
        DLOG("No memory for HID element arrays (%d elements)", num_elements);
        mem_free(hid_info.buttons);
        mem_free(hid_info.values);
        CFRelease(elements);
        mem_free(hid_info.serial);
        mem_free(hid_info.name);
        return;
    }
    for (int i = 0; i < num_elements; i++) {
        const IOHIDElementRef element =
            (IOHIDElementRef)CFArrayGetValueAtIndex(elements, i);
        const IOHIDElementType type = IOHIDElementGetType(element);
        const int usage_page = IOHIDElementGetUsagePage(element);
        const uint32_t usage = usage_page<<16 | IOHIDElementGetUsage(element);
        if (type == kIOHIDElementTypeInput_Button
            && usage_page == HID_PAGE_BUTTON)
        {
            hid_info.buttons[hid_info.num_buttons++] = usage;
        } else if ((type == kIOHIDElementTypeInput_Axis
                    || type == kIOHIDElementTypeInput_Misc)
                   && usage_page == HID_PAGE_GENERIC_DESKTOP)
        {
            /* One might expect stick inputs to have type "axis", but
             * they seem to actually be type "misc" (at least with PS3
             * controllers), so we accept either type. */
            hid_info.values[hid_info.num_values].usage = usage;
            hid_info.values[hid_info.num_values].logical_min =
                IOHIDElementGetLogicalMin(element);
            hid_info.values[hid_info.num_values].logical_max =
                IOHIDElementGetLogicalMax(element);
            hid_info.num_values++;
        }
    }
    CFRelease(elements);

    HIDJoystickHandle *hid_handle = hidjoy_create(&hid_info);
    mem_free(hid_info.values);
    mem_free(hid_info.buttons);
    mem_free(hid_info.serial);
    mem_free(hid_info.name);
    if (UNLIKELY(!hid_handle)) {
        DLOG("Failed to create generic HID handle");
        return;
    }

    /* Open a force-feedback handle if the device supports rumble. */
    FFDeviceObjectReference ff_device;
    if (FFCreateDevice(IOHIDDeviceGetService(device), &ff_device) != FF_OK) {
        ff_device = NULL;
    }

    /* Add the device to the joystick list. */
    JoystickInfo info = {
        .device = device,
        .ff_device = ff_device,
        .ff_effect = NULL,
        .hid_handle = hid_handle,
    };
    sys_mutex_lock(joystick_mutex, -1);
    int index;
    for (index = 0; index < num_joysticks; index++) {
        if (!joystick_info[index].connected) {
            hidjoy_destroy(joysticks[index].hid_handle);
            break;
        }
    }
    if (index == num_joysticks) {
        JoystickInfo *new_joysticks =
            mem_realloc(joysticks, sizeof(*joysticks) * (num_joysticks + 1), 0);
        if (UNLIKELY(!new_joysticks)) {
            DLOG("No memory to expand joysticks array to %d entries",
                 num_joysticks + 1);
            sys_mutex_unlock(joystick_mutex);
            return;
        }
        joysticks = new_joysticks;
        SysInputJoystick *new_joystick_info =
            mem_realloc(joystick_info,
                        sizeof(*joystick_info) * (num_joysticks + 1), 0);
        if (UNLIKELY(!new_joystick_info)) {
            DLOG("No memory to expand joystick_info array to %d entries",
                 num_joysticks + 1);
            sys_mutex_unlock(joystick_mutex);
            return;
        }
        joystick_info = new_joystick_info;
        num_joysticks++;
    }
    joysticks[index].device = device;
    joysticks[index].ff_device = ff_device;
    joysticks[index].ff_effect = NULL;
    joysticks[index].hid_handle = hid_handle;
    joystick_info[index].connected = 1;
    joystick_info[index].can_rumble = (info.ff_device != NULL);
    joystick_info[index].num_buttons = hidjoy_num_buttons(hid_handle);
    joystick_info[index].num_sticks = hidjoy_num_sticks(hid_handle);
    send_joystick_connect_event(index, INPUT_JOYSTICK_CONNECTED);
    IOHIDDeviceRegisterInputValueCallback(device, joystick_input_callback,
                                          (void *)(intptr_t)index);
    sys_mutex_unlock(joystick_mutex);
    hidjoy_set_event_callback(hid_handle, event_callback, index);

    /* Report the new joystick and return. */
    DLOG("New joystick %d: %s (%04X/%04X, serial %s), %d buttons, %d sticks",
         index, hidjoy_name(hid_handle), hidjoy_vendor_id(hid_handle),
         hidjoy_product_id(hid_handle), hidjoy_serial(hid_handle),
         hidjoy_num_buttons(hid_handle), hidjoy_num_sticks(hid_handle));
    return;
}

/*-----------------------------------------------------------------------*/

#undef get_hid_property_int
static int get_hid_property_int(void *handle, CFStringRef property)
{
    CFTypeRef value;
    if (CFGetTypeID(handle) == IOHIDDeviceGetTypeID()) {
        value = IOHIDDeviceGetProperty(handle, property);
    } else {
        ASSERT(CFGetTypeID(handle) == IOHIDElementGetTypeID(), return 0);
        value = IOHIDElementGetProperty(handle, property);
    }
    if (!value || CFGetTypeID(value) != CFNumberGetTypeID()) {
        return 0;
    }
    int value_int;
    CFNumberGetValue((CFNumberRef)value, kCFNumberSInt32Type, &value_int);
    return value_int;
}

/*-----------------------------------------------------------------------*/

#undef get_hid_property_string
static char *get_hid_property_string(void *handle, CFStringRef property)
{
    CFTypeRef value;
    if (CFGetTypeID(handle) == IOHIDDeviceGetTypeID()) {
        value = IOHIDDeviceGetProperty(handle, property);
    } else {
        ASSERT(CFGetTypeID(handle) == IOHIDElementGetTypeID(), return 0);
        value = IOHIDElementGetProperty(handle, property);
    }
    if (!value || CFGetTypeID(value) != CFStringGetTypeID()) {
        return 0;
    }
    int max_length = CFStringGetLength(value) * 3;  // UTF16->UTF8 worst case.
    char *string = mem_alloc(max_length+1, 0, 0);
    if (UNLIKELY(!string)) {
        DLOG("No memory for property string (%d bytes)", max_length+1);
    }
    ASSERT(CFStringGetCString((CFStringRef)value, string, max_length+1,
                              kCFStringEncodingUTF8), return NULL);
    return string;
}

/*-----------------------------------------------------------------------*/

static void joystick_input_callback(void *opaque, UNUSED IOReturn result,
                                    UNUSED void *sender, IOHIDValueRef value)
{
    /* Note that we don't need to take the mutex in this routine because
     * it's always called from the main thread's run loop, so the joystick
     * add/remove callbacks (which are also called on the main thread)
     * can't be running at the same time. */

    if (!joystick_ignore_focus && !sys_graphics_has_focus()) {
        return;
    }

    const int device_index = (int)(intptr_t)opaque;
    ASSERT(device_index >= 0 && device_index < num_joysticks, return);
    /* The documentation says "sender" should be the IOHIDDeviceRef for
     * the device, but in at least some cases we can get an IOHIDQueueRef
     * instead.  Rather than going through the expense of verifying that
     * we do in fact have the right device, we just assume the OS is
     * properly matching values with callbacks. */

    JoystickInfo *joystick = &joysticks[device_index];

    const IOHIDElementRef element = IOHIDValueGetElement(value);
    const int usage_page = IOHIDElementGetUsagePage(element);
    const int usage = IOHIDElementGetUsage(element);
    const double timestamp =
        darwin_time_from_timestamp(IOHIDValueGetTimeStamp(value));
    /* IMPORTANT: In OS X 10.9.5 (and presumably earlier versions),
     * IOHIDValueGetIntegerValue() contains a buffer overflow resulting
     * from a failure to check the size of the data being retrieved before
     * copying it into a local stack-based buffer.  This presumably occurs
     * when the value in question is not numeric, so to work around the
     * bug, we don't attempt to obtain the numeric value until we know
     * that the value is in fact numeric.  This bug was reported to Apple
     * as bug 18996167, and was fixed for OS X 10.10 and later in security
     * update 2015-004 (see https://support.apple.com/en-us/HT204659). */
    if (!hidjoy_is_input_used(joystick->hid_handle, usage_page, usage)) {
        return;
    }
    const int int_value = IOHIDValueGetIntegerValue(value);

    hidjoy_handle_input(joystick->hid_handle, timestamp, usage_page, usage,
                        int_value);
}

/*-----------------------------------------------------------------------*/

static void send_joystick_connect_event(int device, InputEventDetail detail)
{
    PRECOND(detail == INPUT_JOYSTICK_CONNECTED
         || detail == INPUT_JOYSTICK_DISCONNECTED);

    (*event_callback)(&(InputEvent){
        .type = INPUT_EVENT_JOYSTICK,
        .detail = detail,
        .timestamp = time_now(),
        {.joystick = {.device = device}}});
}

/*************************************************************************/
/************************* Other local routines **************************/
/*************************************************************************/

static int convert_mouse_coords(NSEvent *event, float *x_out, float *y_out)
{
    NSWindow *window = macosx_window();
    /* We could potentially get mouse events in the interval between
     * creating the window itself and storing the window pointer where
     * macosx_window() can see it, or we could see events which were sent
     * right before a previous window was destroyed.  In those cases, we
     * use the window from the event if available, and otherwise just
     * return "center of the window". */
    if (UNLIKELY(!window)) {
        window = [event window];
        if (!window) {
            *x_out = *y_out = 0.5;
            return 1;
        }
    }

    const CGRect frame = [window contentRectForFrameRect:[window frame]];
    int width = iroundf(frame.size.width);
    int height = iroundf(frame.size.height);
    ASSERT(width > 0, width = 1);
    ASSERT(height > 0, height = 1);

    NSPoint position = [event locationInWindow];
    if ([event window]) {
        const NSRect window_frame = [[event window] frame];
        position.x += window_frame.origin.x;
        position.y += window_frame.origin.y;
    }

    const int x_in = iroundf(position.x - frame.origin.x);
    const int y_in = iroundf((height-1) - (position.y - frame.origin.y));

    int in_bounds = 1;
    if (x_in < 0) {
        in_bounds = 0;
        *x_out = 0;
    } else if (x_in >= width) {
        in_bounds = 0;
        *x_out = (width-1) / (float)width;
    } else {
        *x_out = x_in / (float)width;
    }
    if (y_in < 0) {
        in_bounds = 0;
        *y_out = 0;
    } else if (y_in >= height) {
        in_bounds = 0;
        *y_out = (height-1) / (float)height;
    } else {
        *y_out = y_in / (float)height;
    }
    return in_bounds;
}

/*-----------------------------------------------------------------------*/

static NSString *deattribute_string(id aString)
{
    NSString *string;
    if ([(NSObject *)aString isKindOfClass:[NSAttributedString class]]) {
        string = [(NSAttributedString *)aString string];
    } else {
        string = (NSString *)aString;
    }
    ASSERT([string isKindOfClass:[NSString class]]);
    return string;
}

/*************************************************************************/
/*************************************************************************/
