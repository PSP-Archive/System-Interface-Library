/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/ios/input.c: iOS input device interface.
 */

/*
 * iOS supports two game controller profiles, the "standard" and
 * "extended" gamepad profiles.  These can be differentiated by calling
 * input_joystick_num_sticks(): if the controller supports analog stick
 * input, it is an extended-profile controller, otherwise it is a
 * standard-profile controller.
 *
 * For extended-profile controllers, stick 0 is the left stick and stick 1
 * is the right stick.
 */

#define IN_SYSDEP

#import "src/base.h"
#import "src/input.h"
#import "src/math.h"
#import "src/memory.h"
#import "src/sysdep.h"
#import "src/sysdep/ios/input.h"
#import "src/sysdep/ios/osk.h"
#import "src/sysdep/ios/util.h"
#import "src/time.h"

#import <Foundation/NSArray.h>
#import <Foundation/NSAutoreleasePool.h>
#import <Foundation/NSNotification.h>
#import <Foundation/NSString.h>
#import <GameController/GCController.h>

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Maximum number of game controllers supported. */
#define MAX_JOYSTICKS  16

/* Mapping from iOS button names to JoystickInfo.buttons[] indices. */
enum {
    IOS_JOYBUTTON_PAUSE = 0,
    IOS_JOYBUTTON_A,
    IOS_JOYBUTTON_B,
    IOS_JOYBUTTON_X,
    IOS_JOYBUTTON_Y,
    IOS_JOYBUTTON_L1,
    IOS_JOYBUTTON_R1,
    IOS_JOYBUTTON_L2,
    IOS_JOYBUTTON_R2,
    IOS_JOYBUTTON__NUM
};

/*-----------------------------------------------------------------------*/

/* Event callback passed to sys_input_init(). */
static InputEventCallback event_callback;

/* Data for connected joysticks. */
typedef struct JoystickInfo JoystickInfo;
struct JoystickInfo {
    GCController *controller;  // NULL if no controller in this slot.
    uint8_t pause_pressed;
    uint8_t buttons[IOS_JOYBUTTON__NUM];
    int8_t dpad_x, dpad_y;
    Vector2f sticks[2];
};
static JoystickInfo joysticks[MAX_JOYSTICKS];
/* Mutex for accessing joystick data. */
static SysMutexID joystick_mutex;
/* Joystick data passed back from sys_input_info(). */
static SysInputJoystick joystick_info[MAX_JOYSTICKS];

/* Dummy object used to hook into controller notifications. */
@interface ControllerNotificationHandler: NSObject
- (void)register_;
@end
static ControllerNotificationHandler *notification_handler;

/* Flag indicating whether the text input interface is active. */
static uint8_t text_input_on;

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * add_joystick:  Add a new joystick to the joysticks[] array.
 *
 * [Parameters]
 *     controller: GCController object to add.
 */
static void add_joystick(GCController *controller);

/**
 * remove_joystick:  Remove a joystick from the joysticks[] array.
 *
 * [Parameters]
 *     controller: GCController object to remove.
 */
static void remove_joystick(GCController *controller);

/*************************************************************************/
/******************** Interface: Basic functionality *********************/
/*************************************************************************/

int sys_input_init(void (*event_callback_)(const struct InputEvent *))
{
    PRECOND(event_callback_ != NULL, return 0);

    event_callback = event_callback_;
    mem_clear(joysticks, sizeof(joysticks));
    text_input_on = 0;

    joystick_mutex = sys_mutex_create(0, 0);
    if (!joystick_mutex) {
        DLOG("Failed to create joystick mutex");
        return 0;
    }

    if (ios_version_is_at_least("7.0")) {
        NSArray *controllers = [GCController controllers];
        /* Per iOS guidelines, we give preference to attached controllers
         * over unattached ones. */
        for (unsigned int i = 0; i < [controllers count]; i++) {
            GCController *controller = [controllers objectAtIndex:i];
            if (controller.attachedToDevice) {
                add_joystick(controller);
            }
        }
        for (unsigned int i = 0; i < [controllers count]; i++) {
            GCController *controller = [controllers objectAtIndex:i];
            if (!controller.attachedToDevice) {
                add_joystick(controller);
            }
        }

        notification_handler = [[ControllerNotificationHandler alloc] init];
        [notification_handler register_];
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

void sys_input_cleanup(void)
{
    if (ios_osk_is_running()) {
        ios_osk_close();
    }

    [[NSNotificationCenter defaultCenter] removeObserver:notification_handler];
    [notification_handler release];
    notification_handler = NULL;
    for (int i = 0; i < lenof(joysticks); i++) {
        if (joysticks[i].controller) {
            remove_joystick(joysticks[i].controller);
        }
    }
    sys_mutex_destroy(joystick_mutex);
    joystick_mutex = 0;

    event_callback = NULL;
}

/*-----------------------------------------------------------------------*/

void sys_input_update(void)
{
    if (ios_version_is_at_least("7.0")) {
        InputEvent event = {.type = INPUT_EVENT_JOYSTICK,
                            .timestamp = time_now()};
        for (int i = 0; i < lenof(joysticks); i++) {
            GCController *controller = joysticks[i].controller;
            if (!controller) {
                continue;
            }
            JoystickInfo state;
            state.buttons[IOS_JOYBUTTON_PAUSE] = joysticks[i].pause_pressed;
            joysticks[i].pause_pressed = 0;
            if (controller.extendedGamepad) {
                GCExtendedGamepad *pad = controller.extendedGamepad;
                state.buttons[IOS_JOYBUTTON_A] = pad.buttonA.pressed;
                state.buttons[IOS_JOYBUTTON_B] = pad.buttonB.pressed;
                state.buttons[IOS_JOYBUTTON_X] = pad.buttonX.pressed;
                state.buttons[IOS_JOYBUTTON_Y] = pad.buttonY.pressed;
                state.buttons[IOS_JOYBUTTON_L1] = pad.leftShoulder.pressed;
                state.buttons[IOS_JOYBUTTON_R1] = pad.rightShoulder.pressed;
                state.buttons[IOS_JOYBUTTON_L2] = pad.leftTrigger.pressed;
                state.buttons[IOS_JOYBUTTON_R2] = pad.rightTrigger.pressed;
                state.dpad_x = pad.dpad.left.pressed ? -1 :
                               pad.dpad.right.pressed ? 1 : 0;
                state.dpad_y = pad.dpad.up.pressed ? -1 :
                               pad.dpad.down.pressed ? 1 : 0;
                state.sticks[0].x = pad.leftThumbstick.xAxis.value;
                state.sticks[0].y = -pad.leftThumbstick.yAxis.value;
                state.sticks[1].x = pad.rightThumbstick.xAxis.value;
                state.sticks[1].y = -pad.rightThumbstick.yAxis.value;
            } else if (controller.gamepad) {
                GCGamepad *pad = controller.gamepad;
                state.buttons[IOS_JOYBUTTON_A] = pad.buttonA.pressed;
                state.buttons[IOS_JOYBUTTON_B] = pad.buttonB.pressed;
                state.buttons[IOS_JOYBUTTON_X] = pad.buttonX.pressed;
                state.buttons[IOS_JOYBUTTON_Y] = pad.buttonY.pressed;
                state.buttons[IOS_JOYBUTTON_L1] = pad.leftShoulder.pressed;
                state.buttons[IOS_JOYBUTTON_R1] = pad.rightShoulder.pressed;
                state.buttons[IOS_JOYBUTTON_L2] = 0;
                state.buttons[IOS_JOYBUTTON_R2] = 0;
                state.dpad_x = pad.dpad.left.pressed ? -1 :
                               pad.dpad.right.pressed ? 1 : 0;
                state.dpad_y = pad.dpad.up.pressed ? -1 :
                               pad.dpad.down.pressed ? 1 : 0;
                state.sticks[0].x = 0;
                state.sticks[0].y = 0;
                state.sticks[1].x = 0;
                state.sticks[1].y = 0;
            } else {
                continue;
            }
            for (int j = 0; j < lenof(state.buttons); j++) {
                if (joysticks[i].buttons[j] != state.buttons[j]) {
                    joysticks[i].buttons[j] = state.buttons[j];
                    event.detail = (state.buttons[j]
                                    ? INPUT_JOYSTICK_BUTTON_DOWN
                                    : INPUT_JOYSTICK_BUTTON_UP);
                    event.joystick.device = i;
                    event.joystick.index = j;
                    (*event_callback)(&event);
                }
            }
            if (joysticks[i].dpad_x != state.dpad_x
             || joysticks[i].dpad_y != state.dpad_y) {
                joysticks[i].dpad_x = state.dpad_x;
                joysticks[i].dpad_y = state.dpad_y;
                event.detail = INPUT_JOYSTICK_DPAD_CHANGE;
                event.joystick.device = i;
                event.joystick.x = state.dpad_x;
                event.joystick.y = state.dpad_y;
                (*event_callback)(&event);
            }
            for (int j = 0; j < lenof(state.sticks); j++) {
                if (joysticks[i].sticks[j].x != state.sticks[j].x
                 || joysticks[i].sticks[j].y != state.sticks[j].y) {
                    joysticks[i].sticks[j] = state.sticks[j];
                    event.detail = INPUT_JOYSTICK_STICK_CHANGE;
                    event.joystick.device = i;
                    event.joystick.index = j;
                    event.joystick.x = state.sticks[j].x;
                    event.joystick.y = state.sticks[j].y;
                    (*event_callback)(&event);
                }
            }
        }  // for (int i = 0; i < lenof(joysticks); i++)
    }  // if (ios_version_is_at_least("7.0"))

    if (text_input_on && !ios_osk_is_running()) {
        text_input_on = 0;
        InputEvent event = {.type = INPUT_EVENT_TEXT, .timestamp = time_now()};
        const int *text = ios_osk_get_text();
        if (!text) {
            DLOG("Failed to get OSK text");
            event.detail = INPUT_TEXT_CANCELLED;
            (*event_callback)(&event);
        } else {
            event.detail = INPUT_TEXT_CLEAR;
            (*event_callback)(&event);
            event.detail = INPUT_TEXT_INPUT;
            for (; *text; text++) {
                event.text.ch = *text;
                (*event_callback)(&event);
            }
            event.detail = INPUT_TEXT_DONE;
            (*event_callback)(&event);
        }
    }
}

/*-----------------------------------------------------------------------*/

void sys_input_info(SysInputInfo *info_ret)
{
    if (ios_version_is_at_least("7.0")) {
        sys_mutex_lock(joystick_mutex, -1);
        int i;
        for (i = lenof(joysticks) - 1; i >= 0; i--) {
            if (joysticks[i].controller) {
                break;
            }
        }
        sys_mutex_unlock(joystick_mutex);
        info_ret->has_joystick  = 1;
        info_ret->num_joysticks = i+1;
        info_ret->joysticks     = joystick_info;
    } else {
        info_ret->has_joystick  = 0;
    }
    info_ret->has_keyboard      = 0;
    info_ret->has_mouse         = 0;
    info_ret->has_text          = 1;
    info_ret->text_uses_custom_interface = 1;
    info_ret->text_has_prompt   = 1;
    info_ret->has_touch         = 1;
}

/*-----------------------------------------------------------------------*/

void sys_input_grab(UNUSED int grab)
{
    /* Meaningless on iOS. */
}

/*-----------------------------------------------------------------------*/

int sys_input_is_quit_requested(void)
{
    return ios_application_is_terminating;
}

/*-----------------------------------------------------------------------*/

int sys_input_is_suspend_requested(void)
{
    return ios_application_is_suspending;
}

/*-----------------------------------------------------------------------*/

void sys_input_acknowledge_suspend_request(void)
{
    sys_semaphore_signal(ios_suspend_semaphore);
    sys_semaphore_wait(ios_resume_semaphore, -1);
}

/*************************************************************************/
/********************* Interface: Joystick handling **********************/
/*************************************************************************/

void sys_input_enable_unfocused_joystick(UNUSED int enable)
{
    /* Nothing to do for iOS. */
}

/*-----------------------------------------------------------------------*/

char *sys_input_joystick_copy_name(int index)
{
    char *name = NULL;
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    /* iOS only exposes a vendor name and not a device name, but go ahead
     * and return that since it's better than nothing. */
    NSString *ns_name = joysticks[index].controller.vendorName;
    if (ns_name) {
        name = mem_strdup([ns_name UTF8String], 0);
    }

    [pool release];
    return name;
}

/*-----------------------------------------------------------------------*/

int sys_input_joystick_button_mapping(int index, int name)
{
    const int is_extended =
        (joysticks[index].controller.extendedGamepad != NULL);
    switch (name) {
        case INPUT_JOYBUTTON_START:      return IOS_JOYBUTTON_PAUSE;
        case INPUT_JOYBUTTON_FACE_UP:    return IOS_JOYBUTTON_Y;
        case INPUT_JOYBUTTON_FACE_LEFT:  return IOS_JOYBUTTON_X;
        case INPUT_JOYBUTTON_FACE_RIGHT: return IOS_JOYBUTTON_B;
        case INPUT_JOYBUTTON_FACE_DOWN:  return IOS_JOYBUTTON_A;
        case INPUT_JOYBUTTON_L1:         return IOS_JOYBUTTON_L1;
        case INPUT_JOYBUTTON_R1:         return IOS_JOYBUTTON_R1;
        case INPUT_JOYBUTTON_L2:         return is_extended ? IOS_JOYBUTTON_L2 : -1;
        case INPUT_JOYBUTTON_R2:         return is_extended ? IOS_JOYBUTTON_R2 : -1;
        default:                         return -1;
    }
}

/*-----------------------------------------------------------------------*/

void sys_input_joystick_rumble(UNUSED int index, UNUSED float left,
                               UNUSED float right, UNUSED float time)
{
    /* Rumble not supported on iOS. */
}

/*************************************************************************/
/*********************** Interface: Mouse handling ***********************/
/*************************************************************************/

void sys_input_mouse_set_position(UNUSED float x, UNUSED float y)
{
    /* No mouse support. */
}

/*************************************************************************/
/******************** Interface: Text entry handling *********************/
/*************************************************************************/

void sys_input_text_set_state(int on, const char *text, const char *prompt)
{
    if (on) {
        ios_osk_open(text ? text : "", prompt);
        text_input_on = 1;
    } else {
        ios_osk_close();
        text_input_on = 0;
    }
}

/*************************************************************************/
/*********************** Library-internal routines ***********************/
/*************************************************************************/

void ios_forward_input_event(const InputEvent *event)
{
    if (event_callback) {
        (*event_callback)(event);
    }
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

@implementation ControllerNotificationHandler

- (void)call_connect:(NSNotification *)notification {
    add_joystick((GCController *)notification.object);
}

- (void)call_disconnect:(NSNotification *)notification {
    remove_joystick((GCController *)notification.object);
}

- (void)register_ {
    [[NSNotificationCenter defaultCenter]
        addObserver:self selector:@selector(call_connect:)
        name:GCControllerDidConnectNotification object:nil];
    [[NSNotificationCenter defaultCenter]
        addObserver:self selector:@selector(call_disconnect:)
        name:GCControllerDidDisconnectNotification object:nil];
}

@end

/*-----------------------------------------------------------------------*/

static void add_joystick(GCController *controller)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    const char *vendor = [controller.vendorName UTF8String];

    /* Work around a bug in iOS 8.0.x where wireless controllers have a
     * spurious second copy of the controller show up with "(Forwarded)"
     * in the name.  (Worse, they never get removed becuase the disconnect
     * notification is sent with a different object.) */
    if (ios_version_is_at_least("8.0") && !ios_version_is_at_least("8.1")) {
        if (strstr(vendor, "(Forwarded)")) {
            DLOG("Ignoring bogus controller object %p (%s)", controller,
                 vendor);
            [pool release];
            return;
        }
    }

    if (!controller.gamepad && !controller.extendedGamepad) {
        DLOG("Controller %p (%s) does not support any gamepad profile,"
             " ignoring", controller, vendor);
        controller.playerIndex = GCControllerPlayerIndexUnset;
        [pool release];
        return;
    }
    const int is_extended = (controller.extendedGamepad != NULL);

    sys_mutex_lock(joystick_mutex, -1);
    int i;
    /* If the controller already has a player index assigned, try to use
     * that index. */
    if (controller.playerIndex != GCControllerPlayerIndexUnset
     && controller.playerIndex >= 0
        /* Cast to int avoids a bogus compiler warning with the iOS 9 SDK. */
     && (int)controller.playerIndex < lenof(joysticks)
     && joysticks[controller.playerIndex].controller == NULL) {
        i = controller.playerIndex;
    } else {
        for (i = 0; i < lenof(joysticks); i++) {
            if (!joysticks[i].controller) {
                break;
            }
        }
        if (i >= lenof(joysticks)) {
            sys_mutex_unlock(joystick_mutex);
            DLOG("Ignoring controller %p (%s) because joysticks[] is full",
                 controller, vendor);
            controller.playerIndex = GCControllerPlayerIndexUnset;
            [pool release];
            return;
        }
    }

    joysticks[i].controller = controller;
    joystick_info[i].connected = 1;
    joystick_info[i].can_rumble = 0;
    joystick_info[i].num_buttons = is_extended ? (IOS_JOYBUTTON_R2 + 1)
                                               : (IOS_JOYBUTTON_R1 + 1);
    joystick_info[i].num_sticks = is_extended ? 2 : 0;
    [controller retain];
    sys_mutex_unlock(joystick_mutex);
    DLOG("Added controller %p (%s) as index %d", controller, vendor, i);
    (*event_callback)(&(InputEvent){
        .type = INPUT_EVENT_JOYSTICK, .detail = INPUT_JOYSTICK_CONNECTED,
        .timestamp = time_now(), {.joystick = {.device = i}}});
    controller.playerIndex = i;

    /* Annoyingly, the pause button is not available as a regular input, so
     * we have to write a separate routine to handle it. */
    controller.controllerPausedHandler = ^(GCController *controller_) {
        sys_mutex_lock(joystick_mutex, -1);
        int i_;
        for (i_ = 0; i_ < lenof(joysticks); i_++) {
            if (joysticks[i_].controller == controller_) {
                /* This is detected and handled in sys_input_update(). */
                joysticks[i_].pause_pressed = 1;
                break;
            }
        }
        sys_mutex_unlock(joystick_mutex);
    };

    /* iOS (at least as of version 8.0) does not ping the idle timer for
     * controller input, so we have to do so ourselves.  We could
     * potentially also use this callback for input handling, but we
     * instead do that as part of sys_input_update() so we don't need to
     * lock and unlock the mutex on each input (which can be frequent with
     * analog sticks). */
    if (is_extended) {
        controller.extendedGamepad.valueChangedHandler =
            ^(UNUSED GCExtendedGamepad *gamepad,
              UNUSED GCControllerElement *element)
            {
                sys_reset_idle_timer();
            };
    } else {
        controller.gamepad.valueChangedHandler =
            ^(UNUSED GCGamepad *gamepad, UNUSED GCControllerElement *element) {
                sys_reset_idle_timer();
            };
    }

    [pool release];
}

/*-----------------------------------------------------------------------*/

static void remove_joystick(GCController *controller)
{
    sys_mutex_lock(joystick_mutex, -1);
    int i;
    for (i = 0; i < lenof(joysticks); i++) {
        if (joysticks[i].controller == controller) {
            joysticks[i].controller = NULL;
            joystick_info[i].connected = 0;
            controller.controllerPausedHandler = NULL;
            if (controller.extendedGamepad) {
                controller.extendedGamepad.valueChangedHandler = NULL;
            } else if (controller.gamepad) {
                controller.gamepad.valueChangedHandler = NULL;
            }
            [controller release];
            break;
        }
    }
    sys_mutex_unlock(joystick_mutex);
    if (i < lenof(joysticks)) {
        DLOG("Removed controller %p, index %d", controller, i);
        (*event_callback)(&(InputEvent){
            .type = INPUT_EVENT_JOYSTICK, .detail = INPUT_JOYSTICK_DISCONNECTED,
            .timestamp = time_now(), {.joystick = {.device = i}}});
    } else {
        DLOG("Failed to remove controller %p (not found)", controller);
    }
}

/*************************************************************************/
/*************************************************************************/
