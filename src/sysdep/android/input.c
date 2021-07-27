/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/android/input.c: Android input device interface.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/input.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/android/internal.h"
#include "src/sysdep/posix/time.h"
#include "src/thread.h"
#include "src/time.h"
#include "src/utility/utf8.h"

#define time __UNUSED_time  // Avoid a shadowing warning.
#include <pthread.h>
#include <time.h>
#undef time

/*************************************************************************/
/************************* Configuration options *************************/
/*************************************************************************/

/**
 * SIL_PLATFORM_ANDROID_INPUT_DEVICE_SCAN_INTERVAL:  Interval between
 * scans of the the input device list to check for new or removed input
 * devices, in seconds.  Note that even if there are no changes to the
 * input configuration, there is still a moderate penalty (~0.5ms) for the
 * scan.  The default is 1.0 seconds.
 */
#ifndef SIL_PLATFORM_ANDROID_INPUT_DEVICE_SCAN_INTERVAL
# define SIL_PLATFORM_ANDROID_INPUT_DEVICE_SCAN_INTERVAL  1.0
#endif

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Joystick button definitions for Android devices, used to remap device
 * keycodes to button index values. */
enum {
    ANDROID_JOY_BUTTON_SELECT = 0,
    ANDROID_JOY_BUTTON_START = 1,
    ANDROID_JOY_BUTTON_MENU = 2,  // "PS" button on the PS3 controller, etc.
    ANDROID_JOY_BUTTON_A = 3,
    ANDROID_JOY_BUTTON_B = 4,
    ANDROID_JOY_BUTTON_C = 5,
    ANDROID_JOY_BUTTON_X = 6,
    ANDROID_JOY_BUTTON_Y = 7,
    ANDROID_JOY_BUTTON_Z = 8,
    ANDROID_JOY_BUTTON_L1 = 9,  // Primary left shoulder button
    ANDROID_JOY_BUTTON_R1 = 10, // Primary right shoulder button
    ANDROID_JOY_BUTTON_L2 = 11, // Secondary left shoulder button
    ANDROID_JOY_BUTTON_R2 = 12, // Secondary right shoulder button
    ANDROID_JOY_BUTTON_L3 = 13, // Left stick button
    ANDROID_JOY_BUTTON_R3 = 14, // Right stick button
    ANDROID_JOY_BUTTON__NUM
};

/*-----------------------------------------------------------------------*/

/*
 * Wrappers for functions not statically known to be present:
 *    - AMotionEvent_getAxisValue() (SDK level 12)
 */

#if SIL_PLATFORM_ANDROID_MIN_SDK_VERSION < 12
extern __attribute__((weak)) float AMotionEvent_getAxisValue(
    const AInputEvent* motion_event, int32_t axis, size_t pointer_index);
static inline int have_getAxisValue(void)
    {return AMotionEvent_getAxisValue != NULL;}
#else
static inline int have_getAxisValue(void) {return 1;}
#endif

/*-----------------------------------------------------------------------*/

/* Cached Java method IDs. */
static jmethodID scanInputDevices, hasJoystick, hasKeyboard, hasMouse,
    getDeviceName, isInputDeviceDpad, isInputDeviceJoystick,
    isInputDeviceKeyboard, isInputDeviceMouse, getJoystickId,
    getAxisThreshold, doesJoystickRumble, showInputDialog, dismissInputDialog,
    isInputDialogFinished, getInputDialogText;

/* Flag: Has the input subsystem been initialized? */
static uint8_t initted;

/* Flag: Does Android use clock_gettime(CLOCK_MONOTONIC) for the
 * java.lang.System.nanoTime() return value?  (See sys_input_init() for
 * details.) */
static uint8_t nanotime_uses_clock_monotonic;

/* Offset (in nanoseconds, usually negative) to add to a Java nanoTime()
 * timestamp to get a value compatible with time_now().  Only used when
 * nanotime_uses_clock_monotonic is false. */
static uint8_t java_time_offset_known;  // Have we set it yet?
static int64_t java_time_offset;

/* Event callback passed to sys_input_init(). */
static InputEventCallback event_callback;

/* Input device info block for sys_input_info(). */
static SysInputInfo input_info;
/* Joystick info for sys_input_info(). */
static SysInputJoystick joystick_info[INPUT_MAX_JOYSTICKS];

/* Timestamp at which we last scanned for new input devices. */
static double last_input_scan;

/* Joystick index to Android input device ID mapping. */
static int joystick_device[INPUT_MAX_JOYSTICKS];
/* Current joystick input state. */
typedef struct JoystickState JoystickState;
struct JoystickState {
    Vector2f stick[2];
    float stick_threshold[2];
    uint8_t dpad_up, dpad_down, dpad_left, dpad_right;
    uint8_t button[ANDROID_JOY_BUTTON__NUM];

    /* Axis index for the X axis of the right stick (if any), -1 if unknown.
     * If only manufacturers would cooperate about things like this... */
    int8_t rx_axis;
    /* Axis index for the Y axis of the right stick (if any).  Undefined if
     * rx_axis == -1. */
    int8_t ry_axis;
    /* 1 if the joystick uses the HAT_X and HAT_Y axes instead of key
     * events to report D-pad input, 0 if not, -1 if unknown. */
    int8_t dpad_is_hat;
    /* 1 if the joystick reports L2/R2 presses with the LTRIGGER and
     * RTRIGGER axes but _not_ with key events, 0 if L2/R2 key events are
     * available, -1 if unknown. */
    int8_t l2r2_axes_only;

    /* Keycodes for each button in button[]. */
    int16_t button_key[ANDROID_JOY_BUTTON__NUM];

    /* Device name, for returning from copy_name(); NULL if unknown. */
    char *name;

    /* Was this joystick previously connected?  (Temporary field used by
     * update_input_devices().) */
    uint8_t was_connected;
};
static JoystickState joystick_state[INPUT_MAX_JOYSTICKS];

/* Flag: Is this an Xperia Play device? */
static uint8_t is_xperia_play;
/* Xperia Play touchpad state for analog stick emulation. */
static uint8_t xperia_stick_active[2];
static int32_t xperia_stick_pointer[2];
const float XPERIA_STICK_DEADZONE = 0.3;

/* Current mouse input state. */
static float mouse_x, mouse_y;
static uint8_t mouse_left, mouse_middle, mouse_right;

/* Currently open text input dialog. */
static jobject text_dialog;

/* Mapping of Android pointer IDs to SIL touch IDs.  pointer < 0 indicates a
 * free entry. */
static struct {
    int32_t pointer;
    unsigned int id;
} touch_map[INPUT_MAX_TOUCHES];
/* Next touch ID to use for a new touch.  Incremented by 1 for each touch,
 * rolling over (and skipping zero) if necessary. */
static unsigned int next_touch_id = 1;

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * update_input_devices:  Update internal state based on the current set
 * of connected input devices.
 *
 * [Parameters]
 *     index: Joystick index.
 *     device: Android input device ID.
 */
static void update_input_devices(void);

/**
 * init_joystick:  Set up joystick_info[] and joystick_state[] data for a
 * newly detected joystick device.
 *
 * [Parameters]
 *     index: Joystick index.
 *     device: Android input device ID.
 */
static void init_joystick(int index, int device);

/**
 * lookup_joystick_device:  Return the joystick_device[] index for the
 * given input device, or -1 if the device is unknown.
 *
 * [Parameters]
 *     device: Android input device ID.
 * [Return value]
 *     Joystick index, or -1 if not a known joystick.
 */
static int lookup_joystick_device(int device);

/**
 * filter_axis_input:  Adjust the given joystick axis input for the input
 * threshold, and return the adjusted value.
 *
 * [Parameters]
 *     input: Raw axis value, in the range [-1.0,+1.0].
 *     threshold: Axis input threshold.
 * [Return value]
 *     Filtered axis value.
 */
static CONST_FUNCTION float filter_axis_input(float input, float threshold);

/**
 * handle_touch:  Process a MotionEvent for a touchscreen device.
 *
 * [Parameters]
 *     event: Input event.
 */
static void handle_touch(AInputEvent *event);

/**
 * lookup_touch:  Look up the touch with the given pointer ID in touch_map.
 * If the ID is not found and "new" is true, allocate a new entry for the
 * touch (if one is free).
 *
 * [Parameters]
 *     pointer: Pointer ID, as returned by AMotionEvent_getPointerId().
 *     new: True if the touch is a new touch, false if not.
 * [Return value]
 *     Index into touch_map[] of the entry for the given touch, or -1 if none.
 */
static int lookup_touch(int32_t pointer, int new);

/**
 * handle_joystick_stick:  Process a MotionEvent for a joystick device.
 *
 * [Parameters]
 *     event: Input event.
 *     index: Joystick index.
 */
static void handle_joystick_stick(AInputEvent *event, int index);

/**
 * handle_xperia_touchpad:  Process a MotionEvent for the Xperia Play touchpad.
 *
 * [Parameters]
 *     event: Input event.
 */
static void handle_xperia_touchpad(AInputEvent *event);

/**
 * handle_mouse_motion:  Process a MotionEvent for a mouse or generic
 * touchpad device.
 *
 * [Parameters]
 *     event: Input event.
 */
static void handle_mouse_motion(AInputEvent *event);

/**
 * handle_joystick_key:  Process a KeyEvent for a joystick device.
 *
 * [Parameters]
 *     event: Input event.
 *     index: Joystick index.
 */
static void handle_joystick_key(AInputEvent *event, int index);

/**
 * handle_mouse_key:  Process a KeyEvent for a mosue device.
 *
 * [Parameters]
 *     event: Input event.
 */
static void handle_mouse_key(AInputEvent *event);

/**
 * handle_generic_key:  Process a KeyEvent for a non-joystick device.
 *
 * [Parameters]
 *     event: Input event.
 */
static void handle_generic_key(AInputEvent *event);

/**
 * update_text_dialog:  Check the status of a running text dialog and
 * generate text input events as appropriate.
 */
static void update_text_dialog(void);

/**
 * send_motion_events:  Send InputEvents for the given Android motion
 * event to the event callback, processing all historical samples in the
 * event.  This function can be used for motion events with actions other
 * than AMOTION_EVENT_ACTION_MOVE, though only MOVE actions potentially
 * include historical samples.
 *
 * [Parameters]
 *     event: Android input event (assumed to be type MOTION).
 *     pointer_index: Pointer index for getX() and similar calls.
 *     template: InputEvent template for events to be sent to the callback.
 *         On return, template->timestamp will contain the timestamp of the
 *         event.
 *     x_ptr: Pointer to field into which X coordinate will be written.
 *     y_ptr: Pointer to field into which Y coordinate will be written.
 */
static void send_motion_events(
    AInputEvent *event, int pointer_index,
    InputEvent *template, float *x_ptr, float *y_ptr);

/**
 * set_java_time_offset:  Calculate the value of java_time_offset, if
 * possible.  (If time_init() has not yet been called, this function will
 * do nothing.)
 */
static void set_java_time_offset(void);

/**
 * convert_java_timestamp:  Convert the given Java nanoTime()-style
 * timestamp to a timestamp compatible with time_now().
 *
 * [Parameters]
 *     time: Java timestamp (compatible with java.lang.System.nanoTime()).
 * [Return value]
 *     Equivalent timestamp compatible with time_now().
 */
static PURE_FUNCTION double convert_java_timestamp(uint64_t time);

/*************************************************************************/
/******************** Interface: Basic functionality *********************/
/*************************************************************************/

int sys_input_init(void (*event_callback_)(const struct InputEvent *))
{
    PRECOND(event_callback_ != NULL, return 0);
    PRECOND(!initted, return 0);

    event_callback = event_callback_;

#if SIL_PLATFORM_ANDROID_MIN_SDK_VERSION < 12
    if (!have_getAxisValue()) {
        DLOG("WARNING: AMotionEvent_getAxisValue could not be resolved;"
             " gamepad support will be limited.  (This is normal if you are"
             " running a version of Android earlier than Honeycomb MR1.)");
    }
#endif  // SIL_PLATFORM_ANDROID_MIN_SDK_VERSION < 12

    JNIEnv *env = get_jni_env();
    jobject activity_obj = android_activity->clazz;

    /*
     * On Android, java.lang.System.nanoTime() normally takes its value
     * from clock_gettime(CLOCK_MONOTONIC); see:
     *    - Dalvik_java_lang_System_nanoTime() (in
     *         platform/dalvik/vm/native/java_lang_System.cpp)
     *    - dvmGetRelativeTimeNsec (in platform/dalvik/vm/Misc.cpp)
     * Since system/posix/time.c also uses CLOCK_MONOTONIC when
     * available, we can convert directly and accurately between
     * Android event timestamps and the time_now() time base.  Here,
     * we run a quick check to see whether Android really is using
     * CLOCK_MONOTONIC.
     */
    nanotime_uses_clock_monotonic = 0;
    jclass System_class = get_class("java.lang.System");
    ASSERT(System_class != 0, return 0);
    jmethodID nanoTime = get_static_method(System_class, "nanoTime", "()J");
    ASSERT(nanoTime != 0, return 0);
    const uint64_t java_time = (*env)->CallStaticLongMethod(
        env, System_class, nanoTime);
    struct timespec ts;
#ifdef CLOCK_MONOTONIC
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        const uint64_t clock_time =
            ((uint64_t)ts.tv_sec)*1000000000 + ts.tv_nsec;
        nanotime_uses_clock_monotonic =
            (llabs((int64_t)(java_time - clock_time)) < 1000000);
    } else {
        DLOG("clock_gettime(CLOCK_MONOTONIC): %s", strerror(errno));
    }
#endif
    if (nanotime_uses_clock_monotonic) {
        DLOG("Assuming clock_gettime(CLOCK_MONOTONIC) as nanoTime() source.");
    } else {
        DLOG("nanoTime() source unknown, event timestamps may be inaccurate!");
        set_java_time_offset();
    }

    scanInputDevices = get_method(0, "scanInputDevices", "()Z");
    hasJoystick = get_method(0, "hasJoystick", "()Z");
    hasKeyboard = get_method(0, "hasKeyboard", "()Z");
    hasMouse = get_method(0, "hasMouse", "()Z");
    getDeviceName = get_method(0, "getDeviceName", "(I)Ljava/lang/String;");
    isInputDeviceDpad = get_method(0, "isInputDeviceDpad", "(I)Z");
    isInputDeviceJoystick = get_method(0, "isInputDeviceJoystick", "(I)Z");
    isInputDeviceKeyboard = get_method(0, "isInputDeviceKeyboard", "(I)Z");
    isInputDeviceMouse = get_method(0, "isInputDeviceMouse", "(I)Z");
    getJoystickId = get_method(0, "getJoystickId", "(I)I");
    getAxisThreshold = get_method(0, "getAxisThreshold", "(II)F");
    doesJoystickRumble = get_method(0, "doesJoystickRumble", "(I)Z");
    showInputDialog = get_method(
        0, "showInputDialog",
        ("(Ljava/lang/String;Ljava/lang/String;)"
         "L" SIL_PLATFORM_ANDROID_PACKAGE_JNI "/InputDialog;"));
    dismissInputDialog = get_method(
        0, "dismissInputDialog",
        "(L" SIL_PLATFORM_ANDROID_PACKAGE_JNI "/InputDialog;)V");
    isInputDialogFinished = get_method(
        0, "isInputDialogFinished",
        "(L" SIL_PLATFORM_ANDROID_PACKAGE_JNI "/InputDialog;)Z");
    getInputDialogText = get_method(
        0, "getInputDialogText",
        ("(L" SIL_PLATFORM_ANDROID_PACKAGE_JNI "/InputDialog;)"
         "Ljava/lang/String;"));
    ASSERT(scanInputDevices != 0, return 0);
    ASSERT(hasJoystick != 0, return 0);
    ASSERT(hasKeyboard != 0, return 0);
    ASSERT(getDeviceName != 0, return 0);
    ASSERT(isInputDeviceDpad != 0, return 0);
    ASSERT(isInputDeviceJoystick != 0, return 0);
    ASSERT(isInputDeviceKeyboard != 0, return 0);
    ASSERT(getJoystickId != 0, return 0);
    ASSERT(getAxisThreshold != 0, return 0);
    ASSERT(doesJoystickRumble != 0, return 0);
    ASSERT(showInputDialog != 0, return 0);
    ASSERT(dismissInputDialog != 0, return 0);
    ASSERT(isInputDialogFinished != 0, return 0);
    ASSERT(getInputDialogText != 0, return 0);

    jmethodID getBuildInfo = get_method(
        0, "getBuildInfo", "(I)Ljava/lang/String;");
    ASSERT(getBuildInfo != 0, return 0);
    jstring j_manufacturer = (*env)->CallObjectMethod(
        env, activity_obj, getBuildInfo, BUILD_INFO_MANUFACTURER);
    jstring j_model = (*env)->CallObjectMethod(
        env, activity_obj, getBuildInfo, BUILD_INFO_MODEL);
    ASSERT(!clear_exceptions(env), return 0);
    ASSERT(j_manufacturer != 0 && j_model != 0, return 0);
    const char *manufacturer =
        (*env)->GetStringUTFChars(env, j_manufacturer, NULL);
    const char *model = (*env)->GetStringUTFChars(env, j_model, NULL);
    is_xperia_play =
        (manufacturer && strcmp(manufacturer, "Sony Ericsson") == 0)
        && (model && strcmp(model, "R800i") == 0);
    if (manufacturer) {
        (*env)->ReleaseStringUTFChars(env, j_manufacturer, manufacturer);
    }
    if (model) {
        (*env)->ReleaseStringUTFChars(env, j_model, model);
    }
    (*env)->DeleteLocalRef(env, j_manufacturer);
    (*env)->DeleteLocalRef(env, j_model);

    mem_clear(joystick_info, sizeof(joystick_info));
    mem_clear(joystick_device, sizeof(joystick_device));
    mem_clear(joystick_state, sizeof(joystick_state));

    mouse_x = mouse_y = 0;
    mouse_left = mouse_middle = mouse_right = 0;

    text_dialog = 0;

    for (int i = 0; i < lenof(touch_map); i++) {
        touch_map[i].pointer = -1;
    }
    next_touch_id = 1;

    last_input_scan =
        time_now() - SIL_PLATFORM_ANDROID_INPUT_DEVICE_SCAN_INTERVAL;

    initted = 1;
    return 1;
}

/*-----------------------------------------------------------------------*/

void sys_input_cleanup(void)
{
    initted = 0;
}

/*-----------------------------------------------------------------------*/

void sys_input_update(void)
{
    if (text_dialog) {
        update_text_dialog();
    }
}

/*-----------------------------------------------------------------------*/

void sys_input_info(SysInputInfo *info_ret)
{
    const double now = time_now();
    if (now - last_input_scan < SIL_PLATFORM_ANDROID_INPUT_DEVICE_SCAN_INTERVAL)
    {
        goto out;
    }
    last_input_scan = now;

    JNIEnv *env = get_jni_env();
    jobject activity_obj = android_activity->clazz;
    const int devices_changed =
        (*env)->CallBooleanMethod(env, activity_obj, scanInputDevices);
    ASSERT(!clear_exceptions(env), goto out);
    if (!devices_changed) {
        goto out;
    }

    DLOG("Input device configuration change detected.");
    update_input_devices();

  out:
    *info_ret = input_info;
}

/*-----------------------------------------------------------------------*/

void sys_input_grab(UNUSED int grab)
{
    /* Meaningless on Android. */
}

/*-----------------------------------------------------------------------*/

int sys_input_is_quit_requested(void)
{
    return android_quit_requested;
}

/*-----------------------------------------------------------------------*/

int sys_input_is_suspend_requested(void)
{
    return android_suspend_requested;
}

/*-----------------------------------------------------------------------*/

void sys_input_acknowledge_suspend_request(void)
{
    android_suspend_graphics();

    sys_semaphore_signal(android_suspend_semaphore);
    sys_semaphore_wait(android_resume_semaphore, -1);

    if (android_quit_requested) {
        thread_exit(0);
    }

    /* Clear all input state, since we don't know what's happened in the
     * interim. */
    mem_clear(joystick_device, sizeof(joystick_device));
    mem_clear(joystick_state, sizeof(joystick_state));
    mem_clear(xperia_stick_active, sizeof(xperia_stick_active));
    JNIEnv *env = get_jni_env();
    jobject activity_obj = android_activity->clazz;
    (*env)->CallBooleanMethod(env, activity_obj, scanInputDevices);
    ASSERT(!clear_exceptions(env));
    update_input_devices();

    android_resume_graphics();
}

/*************************************************************************/
/********************* Interface: Joystick handling **********************/
/*************************************************************************/

void sys_input_enable_unfocused_joystick(int enable)
{
    /* Nothing to do for Android. */
}

/*-----------------------------------------------------------------------*/

char *sys_input_joystick_copy_name(int index)
{
    char *retval;
    if (joystick_state[index].name) {
        retval = mem_strdup(joystick_state[index].name, 0);
    } else {
        retval = NULL;
    }
    return retval;
}

/*-----------------------------------------------------------------------*/

int sys_input_joystick_button_mapping(UNUSED int index, int name)
{
    switch (name) {
      case INPUT_JOYBUTTON_HOME:
        return ANDROID_JOY_BUTTON_MENU;
      case INPUT_JOYBUTTON_START:
        return ANDROID_JOY_BUTTON_START;
      case INPUT_JOYBUTTON_SELECT:
        return ANDROID_JOY_BUTTON_SELECT;
      case INPUT_JOYBUTTON_FACE_UP:
        return ANDROID_JOY_BUTTON_Y;
      case INPUT_JOYBUTTON_FACE_LEFT:
        return ANDROID_JOY_BUTTON_X;
      case INPUT_JOYBUTTON_FACE_RIGHT:
        return ANDROID_JOY_BUTTON_B;
      case INPUT_JOYBUTTON_FACE_DOWN:
        return ANDROID_JOY_BUTTON_A;
      case INPUT_JOYBUTTON_L1:
        return ANDROID_JOY_BUTTON_L1;
      case INPUT_JOYBUTTON_R1:
        return ANDROID_JOY_BUTTON_R1;
      case INPUT_JOYBUTTON_L2:
        return ANDROID_JOY_BUTTON_L2;
      case INPUT_JOYBUTTON_R2:
        return ANDROID_JOY_BUTTON_R2;
      case INPUT_JOYBUTTON_L_STICK:
        return ANDROID_JOY_BUTTON_L3;
      case INPUT_JOYBUTTON_R_STICK:
        return ANDROID_JOY_BUTTON_R3;
      default:
        return -1;
    }
}

/*-----------------------------------------------------------------------*/

void sys_input_joystick_rumble(UNUSED int index, UNUSED float left,
                               UNUSED float right, UNUSED float time)
{
    /* Not supported on Android. */
}

/*************************************************************************/
/*********************** Interface: Mouse handling ***********************/
/*************************************************************************/

void sys_input_mouse_set_position(UNUSED float x, UNUSED float y)
{
    /* Android doesn't allow setting the mouse pointer position. */
}

/*************************************************************************/
/******************** Interface: Text entry handling *********************/
/*************************************************************************/

void sys_input_text_set_state(int on, const char *text, const char *prompt)
{
    JNIEnv *env = get_jni_env();
    jobject activity_obj = android_activity->clazz;

    if (text_dialog) {
        (*env)->CallVoidMethod(env, activity_obj, dismissInputDialog,
                               text_dialog);
        (*env)->DeleteGlobalRef(env, text_dialog);
        ASSERT(!clear_exceptions(env));
        text_dialog = 0;
    }

    if (on) {
        jstring j_text = (*env)->NewStringUTF(env, text ? text : "");
        ASSERT(j_text != 0, clear_exceptions(env); return);
        jstring j_prompt = (*env)->NewStringUTF(env, prompt ? prompt : "");
        ASSERT(j_prompt != 0, clear_exceptions(env); (*env)->DeleteLocalRef(env, j_text); return);
        jobject dialog = (*env)->CallObjectMethod(
            env, activity_obj, showInputDialog, j_prompt, j_text);
        (*env)->DeleteLocalRef(env, j_text);
        (*env)->DeleteLocalRef(env, j_prompt);
        if (!clear_exceptions(env) && dialog) {
            text_dialog = (*env)->NewGlobalRef(env, dialog);
            if (!text_dialog) {
                DLOG("Failed to create global reference to text input dialog!");
                (*env)->CallVoidMethod(env, activity_obj, dismissInputDialog,
                                       dialog);
                ASSERT(!clear_exceptions(env));
            }
            (*env)->DeleteLocalRef(env, dialog);
        } else {
            DLOG("Failed to open text input dialog!");
        }
    }
}

/*************************************************************************/
/*********************** Library-internal routines ***********************/
/*************************************************************************/

int32_t android_handle_input_event(AInputEvent *event)
{
    if (!initted) {
        return 0;
    }

    const int device = AInputEvent_getDeviceId(event);
    const int type = AInputEvent_getType(event);
    const int source = AInputEvent_getSource(event);

    if (type == AINPUT_EVENT_TYPE_MOTION
     && ((source & AINPUT_SOURCE_TOUCHSCREEN) == AINPUT_SOURCE_TOUCHSCREEN)) {
        handle_touch(event);
        return 1;
    }

    if (type == AINPUT_EVENT_TYPE_MOTION
        && ((source & AINPUT_SOURCE_GAMEPAD) == AINPUT_SOURCE_GAMEPAD
            || (source & AINPUT_SOURCE_JOYSTICK) == AINPUT_SOURCE_JOYSTICK))
    {
        const int index = lookup_joystick_device(device);
        if (index >= 0) {
            handle_joystick_stick(event, index);
        } else {
            DLOG("Got joystick motion event for unknown device %d", device);
        }
        return 1;
    }

    if (type == AINPUT_EVENT_TYPE_MOTION
     && ((source & AINPUT_SOURCE_TOUCHPAD) == AINPUT_SOURCE_TOUCHPAD)) {
        if (is_xperia_play) {
            /* Special handling for the Xperia Play "analog stick" touchpad. */
            handle_xperia_touchpad(event);
        } else {
            handle_mouse_motion(event);
        }
        return 1;
    }

    if (type == AINPUT_EVENT_TYPE_MOTION
     && ((source & AINPUT_SOURCE_MOUSE) == AINPUT_SOURCE_MOUSE)) {
        handle_mouse_motion(event);
        return 1;
    }

    if (type == AINPUT_EVENT_TYPE_KEY) {
        /* System buttons are passed through here first, so we need to let
         * the system see them. */
        const int32_t keycode = AKeyEvent_getKeyCode(event);
        if (keycode == AKEYCODE_VOLUME_UP
         || keycode == AKEYCODE_VOLUME_DOWN
         || keycode == AKEYCODE_POWER
         || keycode == AKEYCODE_CAMERA) {
            return 0;
        }
        /* If the "Back" button on the Android 3.0+ navigation softkey bar
         * is touched in "lights out" mode, "lights out" is cancelled (not
         * just suspended), so we need to activate it again. */
        if (keycode == AKEYCODE_BACK && !android_get_navigation_bar_state()) {
            android_toggle_navigation_bar(0);
        }
        /* The "source" value doesn't seem to always reflect the actual
         * input source (e.g. Xperia Play D-pad buttons report KEYBOARD
         * instead of DPAD), so we have to check the device itself. */
        JNIEnv *env = get_jni_env();
        jobject activity_obj = android_activity->clazz;
        const int is_dpad = (*env)->CallBooleanMethod(
            env, activity_obj, isInputDeviceDpad, device);
        const int is_joystick = (*env)->CallBooleanMethod(
            env, activity_obj, isInputDeviceJoystick, device);
        const int is_keyboard = (*env)->CallBooleanMethod(
            env, activity_obj, isInputDeviceKeyboard, device);
        const int is_mouse = (*env)->CallBooleanMethod(
            env, activity_obj, isInputDeviceMouse, device);
        ASSERT(!clear_exceptions(env));
        /* A single device might be (for example) both a keyboard and a
         * mouse at the same time, so we need to check all applicable
         * key sets.  For a keyboard/mouse combo, we treat BACK/MENU
         * events as mouse button presses since there's no way to
         * distinguish them from keyboard keys (MotionEvent.getButtonState()
         * is missing from the NDK, and it doesn't seem to work anyway if
         * called via dynamic symbol lookup). */
        if (is_dpad || is_joystick) {
            const int index = lookup_joystick_device(device);
            if (index >= 0) {
                handle_joystick_key(event, index);
            } else {
                DLOG("Got joystick/dpad key event for unknown device %d",
                     device);
            }
        }
        if (is_mouse) {
            handle_mouse_key(event);
        }
        if (is_keyboard || !(is_dpad || is_joystick || is_mouse)) {
            handle_generic_key(event);
        }
        return 1;
    }

    return 0;
}

/*-----------------------------------------------------------------------*/

void android_forward_input_event(const InputEvent *event)
{
    (*event_callback)(event);
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static void update_input_devices(void)
{
    JNIEnv *env = get_jni_env();
    jobject activity_obj = android_activity->clazz;

    /* Special case for the Xperia Play: the gamepad is reported as a
     * keyboard + D-pad, but treat it as a "joystick" anyway. */
    int has_joystick = is_xperia_play
        || (*env)->CallBooleanMethod(env, activity_obj, hasJoystick);
    int num_joysticks;
    if (has_joystick) {
        for (int i = 0; i < lenof(joystick_device); i++) {
            joystick_state[i].was_connected = joystick_info[i].connected;
            joystick_info[i].connected = 0;
        }
        /* First pass: re-register joysticks that were already known,
         * keeping the same joystick index. */
        for (int i = 0; i < lenof(joystick_device); i++) {
            const int device_id = (*env)->CallIntMethod(
                env, activity_obj, getJoystickId, i);
            if (!device_id) {
                break;
            }
            for (int j = 0; j < lenof(joystick_device); j++) {
                if (device_id == joystick_device[j]) {
                    joystick_info[j].connected = 1;
                    break;
                }
            }
        }
        /* Send disconnect events for any joysticks which are no longer
         * connected. */
        for (int i = 0; i < lenof(joystick_device); i++) {
            if (joystick_state[i].was_connected
             && !joystick_info[i].connected) {
                DLOG("Joystick %d disconnected", i);
                (*event_callback)(&(InputEvent){
                    .type = INPUT_EVENT_JOYSTICK,
                    .detail = INPUT_JOYSTICK_DISCONNECTED,
                    .timestamp = time_now(), {.joystick = {.device = i}}});
                mem_free(joystick_state[i].name);
                joystick_state[i].name = NULL;
            }
        }
        /* Second pass: assign new joysticks, starting from the lowest
         * currently-unused index. */
        for (int i = 0; i < lenof(joystick_device); i++) {
            const int device_id = (*env)->CallIntMethod(
                env, activity_obj, getJoystickId, i);
            if (!device_id) {
                break;
            }
            int found = 0;
            for (int j = 0; j < lenof(joystick_device); j++) {
                if (device_id == joystick_device[j]) {
                    found = 1;
                    break;
                }
            }
            if (found) {
                continue;  // Was already handled above.
            }
            int index = -1;
            for (int j = 0; j < lenof(joystick_device); j++) {
                if (!joystick_info[j].connected) {
                    index = j;
                    break;
                }
            }
            ASSERT(index >= 0, continue);
            joystick_device[index] = device_id;
            init_joystick(index, device_id);
        }
        /* Count the number of valid joystick entries, including joysticks
         * that are currently disconnected but have a lower index than a
         * connected joystick. */
        for (num_joysticks = lenof(joystick_device); num_joysticks > 0;
             num_joysticks--)
        {
            if (joystick_info[num_joysticks-1].connected) {
                break;
            }
        }
        for (int i = 0; i < num_joysticks; i++) {
            DLOG("Joystick %d (%s): input device %d%s",
                 i, joystick_state[i].name, joystick_device[i],
                 joystick_info[i].connected ? "" : " (disconnected)");
        }
    } else {
        num_joysticks = 0;
        for (int i = 0; i < lenof(joystick_device); i++) {
            joystick_device[i] = 0;
            joystick_info[i].connected = 0;
            mem_free(joystick_state[i].name);
            joystick_state[i].name = NULL;
        }
    }

    input_info.has_joystick     = has_joystick;
    input_info.num_joysticks    = num_joysticks;
    input_info.joysticks        = joystick_info;
    input_info.has_keyboard     = 1;  // We always have at least BACK/MENU.
    input_info.keyboard_is_full =
        (*env)->CallBooleanMethod(env, activity_obj, hasKeyboard);
    input_info.has_mouse        =
        (*env)->CallBooleanMethod(env, activity_obj, hasMouse);
    input_info.has_text         = 1;
    input_info.text_uses_custom_interface = 1;
    input_info.text_has_prompt  = 1;
    input_info.has_touch        = 1;

    ASSERT(!clear_exceptions(env));
}

/*-----------------------------------------------------------------------*/

static void init_joystick(int index, int device)
{
    PRECOND(index >= 0 && index < lenof(joystick_info), return);

    JNIEnv *env = get_jni_env();
    jobject activity_obj = android_activity->clazz;

    static const int8_t default_button_key[ANDROID_JOY_BUTTON__NUM] = {
        [ANDROID_JOY_BUTTON_SELECT] = AKEYCODE_BUTTON_SELECT,
        [ANDROID_JOY_BUTTON_START ] = AKEYCODE_BUTTON_START,
        [ANDROID_JOY_BUTTON_MENU  ] = AKEYCODE_BUTTON_MODE,
        [ANDROID_JOY_BUTTON_A     ] = AKEYCODE_BUTTON_A,
        [ANDROID_JOY_BUTTON_B     ] = AKEYCODE_BUTTON_B,
        [ANDROID_JOY_BUTTON_C     ] = AKEYCODE_BUTTON_C,
        [ANDROID_JOY_BUTTON_X     ] = AKEYCODE_BUTTON_X,
        [ANDROID_JOY_BUTTON_Y     ] = AKEYCODE_BUTTON_Y,
        [ANDROID_JOY_BUTTON_Z     ] = AKEYCODE_BUTTON_Z,
        [ANDROID_JOY_BUTTON_L1    ] = AKEYCODE_BUTTON_L1,
        [ANDROID_JOY_BUTTON_R1    ] = AKEYCODE_BUTTON_R1,
        [ANDROID_JOY_BUTTON_L2    ] = AKEYCODE_BUTTON_L2,
        [ANDROID_JOY_BUTTON_R2    ] = AKEYCODE_BUTTON_R2,
        [ANDROID_JOY_BUTTON_L3    ] = AKEYCODE_BUTTON_THUMBL,
        [ANDROID_JOY_BUTTON_R3    ] = AKEYCODE_BUTTON_THUMBR,
    };
    ASSERT(lenof(default_button_key)
           == lenof(joystick_state[index].button_key));

    joystick_info[index].connected   = 1;
    joystick_info[index].can_rumble  = (*env)->CallBooleanMethod(
        env, activity_obj, doesJoystickRumble, device);
    joystick_info[index].num_buttons = lenof(joystick_state[index].button);
    joystick_info[index].num_sticks  = lenof(joystick_state[index].stick);

    joystick_state[index].rx_axis = -1;
    joystick_state[index].ry_axis = -1;
    joystick_state[index].l2r2_axes_only = -1;
    joystick_state[index].dpad_is_hat = -1;
    for (int i = 0; i < lenof(joystick_state[index].button_key); i++) {
        ASSERT(default_button_key[i] != 0);
        joystick_state[index].button_key[i] = default_button_key[i];
    }

    ASSERT(!clear_exceptions(env), return);

    jstring j_name = (*env)->CallObjectMethod(
        env, activity_obj, getDeviceName, device);
    ASSERT(!clear_exceptions(env), return);
    ASSERT(j_name != 0, return);
    const char *name = (*env)->GetStringUTFChars(env, j_name, NULL);
    if (name) {
        joystick_state[index].name = mem_strdup(name, 0);
        if (UNLIKELY(!joystick_state[index].name)) {
            DLOG("Failed to copy joystick name: %s", name);
        }
        (*env)->ReleaseStringUTFChars(env, j_name, name);
    } else {
        joystick_state[index].name = NULL;
    }
    (*env)->DeleteLocalRef(env, j_name);

    name = joystick_state[index].name ? joystick_state[index].name : "";
    if (is_xperia_play && strcmp(name, "keypad-zeus") == 0) {
        joystick_state[index].button_key[ANDROID_JOY_BUTTON_A] =
            AKEYCODE_DPAD_CENTER;
        joystick_state[index].button_key[ANDROID_JOY_BUTTON_B] = AKEYCODE_BACK;

    } else if (strcmp(name, "Microsoft X-Box 360 pad") == 0) {
        joystick_state[index].rx_axis = AMOTION_EVENT_AXIS_Z;
        joystick_state[index].ry_axis = AMOTION_EVENT_AXIS_RZ;
        joystick_state[index].dpad_is_hat = 1;
        joystick_state[index].l2r2_axes_only = 1;

    } else if (strcmp(name, "Sony PLAYSTATION(R)3 Controller") == 0) {
        joystick_state[index].rx_axis = AMOTION_EVENT_AXIS_Z;
        joystick_state[index].ry_axis = AMOTION_EVENT_AXIS_RZ;
        joystick_state[index].dpad_is_hat = 0;
        joystick_state[index].l2r2_axes_only = 0;
        joystick_state[index].button_key[ANDROID_JOY_BUTTON_MENU] =
            AKEYCODE_BUTTON_1;
        /* Map these like the Xbox gamepad: Circle -> B, Cross -> A,
         * Square -> X, Triangle -> Y */
        joystick_state[index].button_key[ANDROID_JOY_BUTTON_Y] =
            AKEYCODE_BUTTON_B;
        joystick_state[index].button_key[ANDROID_JOY_BUTTON_A] =
            AKEYCODE_BUTTON_X;
        joystick_state[index].button_key[ANDROID_JOY_BUTTON_X] =
            AKEYCODE_BUTTON_A;
        joystick_state[index].button_key[ANDROID_JOY_BUTTON_B] =
            AKEYCODE_BUTTON_Y;

    } else {
        const float z_threshold = (*env)->CallFloatMethod(
            env, activity_obj, getAxisThreshold, device, AMOTION_EVENT_AXIS_Z);
        const float rx_threshold = (*env)->CallFloatMethod(
            env, activity_obj, getAxisThreshold, device, AMOTION_EVENT_AXIS_RX);
        const float ry_threshold = (*env)->CallFloatMethod(
            env, activity_obj, getAxisThreshold, device, AMOTION_EVENT_AXIS_RY);
        const float rz_threshold = (*env)->CallFloatMethod(
            env, activity_obj, getAxisThreshold, device, AMOTION_EVENT_AXIS_RZ);
        DLOG("Guess right stick axes: thresholds Z=%g RX=%g RY=%g RZ=%g",
             z_threshold, rx_threshold, ry_threshold, rz_threshold);
        if (z_threshold != 0) {
            if (fabsf(rx_threshold - z_threshold) / z_threshold < 0.001f) {
                DLOG("  --> Guessing X = AXIS_Z, Y = AXIS_RX");
                joystick_state[index].rx_axis = AMOTION_EVENT_AXIS_Z;
                joystick_state[index].ry_axis = AMOTION_EVENT_AXIS_RX;
            } else if (fabsf(rz_threshold - z_threshold) / z_threshold < 0.001f) {
                DLOG("  --> Guessing X = AXIS_Z, Y = AXIS_RZ");
                joystick_state[index].rx_axis = AMOTION_EVENT_AXIS_Z;
                joystick_state[index].ry_axis = AMOTION_EVENT_AXIS_RZ;
            }
        } else if (rx_threshold != 0) {
            if (fabsf(ry_threshold - rx_threshold) / rx_threshold < 0.001f) {
                DLOG("  --> Guessing X = AXIS_RX, Y = AXIS_RY");
                joystick_state[index].rx_axis = AMOTION_EVENT_AXIS_RX;
                joystick_state[index].ry_axis = AMOTION_EVENT_AXIS_RY;
            }
        }
        if (joystick_state[index].rx_axis < 0) {
            DLOG("  --> No idea, giving up...");
        }
    }

    joystick_state[index].stick_threshold[0] = (*env)->CallFloatMethod(
        env, activity_obj, getAxisThreshold, device, AMOTION_EVENT_AXIS_X);
    if (joystick_state[index].rx_axis >= 0) {
        joystick_state[index].stick_threshold[1] = (*env)->CallFloatMethod(
            env, activity_obj, getAxisThreshold, device,
            joystick_state[index].rx_axis);
    }

    DLOG("Joystick %d (%s) connected", index, name);
    (*event_callback)(&(InputEvent){
        .type = INPUT_EVENT_JOYSTICK, .detail = INPUT_JOYSTICK_CONNECTED,
        .timestamp = time_now(), {.joystick = {.device = index}}});
}

/*-----------------------------------------------------------------------*/

static int lookup_joystick_device(int device)
{
    for (int i = 0; i < lenof(joystick_device); i++) {
        if (joystick_info[i].connected && device == joystick_device[i]) {
            return (int)i;
        }
    }
    return -1;
}

/*-----------------------------------------------------------------------*/

static float filter_axis_input(float input, float threshold)
{
    if (input < 0) {
        return ubound(input + threshold, 0.0f) / (1.0f - threshold);
    } else {
        return lbound(input - threshold, 0.0f) / (1.0f - threshold);
    }
}

/*-----------------------------------------------------------------------*/

static void handle_touch(AInputEvent *event)
{
    const double timestamp =
        convert_java_timestamp(AMotionEvent_getEventTime(event));
    const int32_t status = AMotionEvent_getAction(event);
    const int action = status & AMOTION_EVENT_ACTION_MASK;
    const float width = android_display_width();
    const float height = android_display_height();

    switch (action) {
      case AMOTION_EVENT_ACTION_DOWN:
      case AMOTION_EVENT_ACTION_POINTER_DOWN: {
        const int pointer_index =
            (status & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
            >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        const int32_t pointer =
            AMotionEvent_getPointerId(event, pointer_index);
        const float x = AMotionEvent_getX(event, pointer_index) / width;
        const float y = AMotionEvent_getY(event, pointer_index) / height;
        const int index = lookup_touch(pointer, 1);
        if (index >= 0) {
            touch_map[index].id = next_touch_id;
            next_touch_id++;
            if (next_touch_id == 0) {
                next_touch_id++;
            }
            (*event_callback)(&(InputEvent){
                .type = INPUT_EVENT_TOUCH, .detail = INPUT_TOUCH_DOWN,
                .timestamp = timestamp,
                {.touch = {.id = touch_map[index].id, .x = x, .y = y}}});
        }
        break;
      }

      case AMOTION_EVENT_ACTION_MOVE: {
        const int count = AMotionEvent_getPointerCount(event);
        for (int i = 0; i < count; i++) {
            const int32_t pointer = AMotionEvent_getPointerId(event, i);
            const int index = lookup_touch(pointer, 0);
            if (index >= 0) {
                InputEvent template = {
                    .type = INPUT_EVENT_TOUCH, .detail = INPUT_TOUCH_MOVE,
                    .timestamp = 0,  // Placeholder so union initializer works.
                    {.touch = {.id = touch_map[index].id}}};
                send_motion_events(event, index, &template, &template.touch.x,
                                   &template.touch.y);
            }
        }
        break;
      }

      case AMOTION_EVENT_ACTION_UP:
      case AMOTION_EVENT_ACTION_POINTER_UP:
      case AMOTION_EVENT_ACTION_CANCEL: {
        const int pointer_index =
            (status & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
            >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        const int32_t pointer =
            AMotionEvent_getPointerId(event, pointer_index);
        const float x = AMotionEvent_getX(event, pointer_index) / width;
        const float y = AMotionEvent_getY(event, pointer_index) / height;
        const int index = lookup_touch(pointer, 0);
        if (index >= 0) {
            (*event_callback)(&(InputEvent){
                .type = INPUT_EVENT_TOUCH,
                .detail = (action == AMOTION_EVENT_ACTION_CANCEL
                           ? INPUT_TOUCH_CANCEL : INPUT_TOUCH_UP),
                .timestamp = timestamp,
                {.touch = {.id = touch_map[index].id, .x = x, .y = y}}});
            touch_map[index].pointer = -1;
        }
        break;
      }
    }
}

/*-----------------------------------------------------------------------*/

static int lookup_touch(int pointer, int new)
{
    int unused = -1;
    for (int i = 0; i < lenof(touch_map); i++) {
        if (touch_map[i].pointer == pointer) {
            if (new) {
                DLOG("Strange: already had active record for new touch %d",
                     pointer);
            }
            return i;
        } else if (new && unused < 0 && touch_map[i].pointer < 0) {
            unused = i;
        }
    }
    if (new && unused >= 0) {
        touch_map[unused].pointer = pointer;
    }
    return unused;
}

/*-----------------------------------------------------------------------*/

static void handle_joystick_stick(AInputEvent *event, int index)
{
    ASSERT(AMotionEvent_getPointerCount(event) > 0, return);

    const double timestamp =
        convert_java_timestamp(AMotionEvent_getEventTime(event));

#if SIL_PLATFORM_ANDROID_MIN_SDK_VERSION < 12
    if (!have_getAxisValue()) {
        joystick_state[index].stick[0].x = AMotionEvent_getX(event, 0);
        joystick_state[index].stick[0].y = AMotionEvent_getY(event, 0);
        (*event_callback)(&(InputEvent){
            .type = INPUT_EVENT_JOYSTICK,
            .detail = INPUT_JOYSTICK_STICK_CHANGE,
            .timestamp = timestamp,
            {.joystick = {.device = index, .index = 0,
                          .x = AMotionEvent_getX(event, 0),
                          .y = AMotionEvent_getY(event, 0)}}});
        return;
    }
#endif

    const float lx_raw =
        AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_X, 0);
    const float ly_raw =
        AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_Y, 0);
    float rx_raw = 0, ry_raw = 0;
    if (joystick_state[index].rx_axis < 0) {
        const float axis_rx =
            AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_RX, 0);
        const float axis_ry =
            AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_RY, 0);
        const float axis_rz =
            AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_RZ, 0);
        if (fabsf(axis_ry) >= 0.25f) {
            DLOG("Using (AXIS_RX, AXIS_RY) as right stick axes (values = %.3f,"
                 " %.3f)", axis_rx, axis_ry);
            joystick_state[index].rx_axis = AMOTION_EVENT_AXIS_RX;
            joystick_state[index].ry_axis = AMOTION_EVENT_AXIS_RY;
        } else if (fabsf(axis_rx) >= 0.25f) {
            DLOG("Using (AXIS_Z, AXIS_RX) as right stick axes (values = %.3f,"
                 " %.3f)",
                 AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_Z, 0),
                 axis_rx);
            joystick_state[index].rx_axis = AMOTION_EVENT_AXIS_Z;
            joystick_state[index].ry_axis = AMOTION_EVENT_AXIS_RX;
        } else if (fabsf(axis_rz) >= 0.25f && axis_rz >= -0.9f) {
            /* If we see AXIS_RZ == -1, it might be a broken driver sending
             * trigger inputs on the analog stick axes, so ignore. */
            DLOG("Using (AXIS_Z, AXIS_RZ) as right stick axes (values = %.3f,"
                 " %.3f)",
                 AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_Z, 0),
                 axis_rz);
            joystick_state[index].rx_axis = AMOTION_EVENT_AXIS_Z;
            joystick_state[index].ry_axis = AMOTION_EVENT_AXIS_RZ;
        }
        if (joystick_state[index].rx_axis >= 0) {
            JNIEnv *env = get_jni_env();
            jobject activity_obj = android_activity->clazz;
            joystick_state[index].stick_threshold[1] = (*env)->CallFloatMethod(
                env, activity_obj, getAxisThreshold,
                AInputEvent_getDeviceId(event), joystick_state[index].rx_axis);
        }
    }
    if (joystick_state[index].rx_axis >= 0) {
        rx_raw = AMotionEvent_getAxisValue(event, joystick_state[index].rx_axis, 0);
        ry_raw = AMotionEvent_getAxisValue(event, joystick_state[index].ry_axis, 0);
    }
    const float lx =
        filter_axis_input(lx_raw, joystick_state[index].stick_threshold[0]);
    const float ly =
        filter_axis_input(ly_raw, joystick_state[index].stick_threshold[0]);
    const float rx =
        filter_axis_input(rx_raw, joystick_state[index].stick_threshold[1]);
    const float ry =
        filter_axis_input(ry_raw, joystick_state[index].stick_threshold[1]);
    if (lx != joystick_state[index].stick[0].x
     || ly != joystick_state[index].stick[0].y) {
        (*event_callback)(&(InputEvent){
            .type = INPUT_EVENT_JOYSTICK,
            .detail = INPUT_JOYSTICK_STICK_CHANGE,
            .timestamp = timestamp,
            {.joystick = {.device = index, .index = 0, .x = lx, .y = ly}}});
        joystick_state[index].stick[0].x = lx;
        joystick_state[index].stick[0].y = ly;
    }
    if (rx != joystick_state[index].stick[1].x
     || ry != joystick_state[index].stick[1].y) {
        (*event_callback)(&(InputEvent){
            .type = INPUT_EVENT_JOYSTICK,
            .detail = INPUT_JOYSTICK_STICK_CHANGE,
            .timestamp = timestamp,
            {.joystick = {.device = index, .index = 1, .x = rx, .y = ry}}});
        joystick_state[index].stick[1].x = rx;
        joystick_state[index].stick[1].y = ry;
    }

    if (joystick_state[index].dpad_is_hat < 0) {
        const float x =
            AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_HAT_X, 0);
        const float y =
            AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_HAT_Y, 0);
        if (fabsf(x) >= 0.5f || fabsf(y) >= 0.5f) {
            DLOG("Using hats as D-pad (X=%g, Y=%g)",
                 AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_HAT_X, 0),
                 AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_HAT_Y, 0));
            joystick_state[index].dpad_is_hat = 1;
        }
    }
    if (joystick_state[index].dpad_is_hat > 0) {
        const float x =
            AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_HAT_X, 0);
        const float y =
            AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_HAT_Y, 0);
        const int dpad_up = (y < -0.5f);
        const int dpad_down = (y > 0.5f);
        const int dpad_left = (x < -0.5f);
        const int dpad_right = (x > 0.5f);
        if (dpad_up != joystick_state[index].dpad_up
         || dpad_down != joystick_state[index].dpad_down
         || dpad_left != joystick_state[index].dpad_left
         || dpad_right != joystick_state[index].dpad_right) {
            (*event_callback)(&(InputEvent){
                .type = INPUT_EVENT_JOYSTICK,
                .detail = INPUT_JOYSTICK_DPAD_CHANGE,
                .timestamp = timestamp,
                {.joystick = {.device = index,
                              .x = dpad_right - dpad_left,
                              .y = dpad_down - dpad_up}}});
        }
        joystick_state[index].dpad_up = dpad_up;
        joystick_state[index].dpad_down = dpad_down;
        joystick_state[index].dpad_left = dpad_left;
        joystick_state[index].dpad_right = dpad_right;
    }

    if (joystick_state[index].l2r2_axes_only < 0) {
        const float l =
            AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_LTRIGGER, 0);
        const float r =
            AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_RTRIGGER, 0);
        if (l >= 0.5f || r >= 0.5f) {
            DLOG("Assuming no L2/R2 buttons (AXIS_LTRIGGER=%.3f,"
                 " AXIS_RTRIGGER=%.3f)", l, r);
            joystick_state[index].l2r2_axes_only = 1;
        }
    }
    if (joystick_state[index].l2r2_axes_only > 0) {
        const float l =
            AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_LTRIGGER, 0);
        const float r =
            AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_RTRIGGER, 0);
        const int l2 = (l >= 0.5f);
        const int r2 = (r >= 0.5f);
        if (l2 != joystick_state[index].button[ANDROID_JOY_BUTTON_L2]) {
            (*event_callback)(&(InputEvent){
                .type = INPUT_EVENT_JOYSTICK,
                .detail = (l2 ? INPUT_JOYSTICK_BUTTON_DOWN
                              : INPUT_JOYSTICK_BUTTON_UP),
                .timestamp = timestamp,
                {.joystick = {.device = index,
                              .index = ANDROID_JOY_BUTTON_L2}}});
            joystick_state[index].button[ANDROID_JOY_BUTTON_L2] = l2;
        }
        if (r2 != joystick_state[index].button[ANDROID_JOY_BUTTON_R2]) {
            (*event_callback)(&(InputEvent){
                .type = INPUT_EVENT_JOYSTICK,
                .detail = (r2 ? INPUT_JOYSTICK_BUTTON_DOWN
                              : INPUT_JOYSTICK_BUTTON_UP),
                .timestamp = timestamp,
                {.joystick = {.device = index,
                              .index = ANDROID_JOY_BUTTON_R2}}});
            joystick_state[index].button[ANDROID_JOY_BUTTON_R2] = r2;
        }
    }
}

/*-----------------------------------------------------------------------*/

static void handle_xperia_touchpad(AInputEvent *event)
{
    const double timestamp =
        convert_java_timestamp(AMotionEvent_getEventTime(event));
    const int32_t status = AMotionEvent_getAction(event);
    const int action = status & AMOTION_EVENT_ACTION_MASK;
    const int is_down = (action == AMOTION_EVENT_ACTION_DOWN
                         || action == AMOTION_EVENT_ACTION_POINTER_DOWN);
    const int is_up = (!is_down && action != AMOTION_EVENT_ACTION_MOVE);
    const int index = ((status & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                       >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);
    const int32_t id = AMotionEvent_getPointerId(event, index);
    float x = AMotionEvent_getX(event, index);
    float y = AMotionEvent_getY(event, index);
    int update_stick = -1;

    if (is_down) {
        if (!xperia_stick_active[0] && x <= 360) {
            xperia_stick_active[0] = 1;
            xperia_stick_pointer[0] = id;
            update_stick = 0;
        } else if (!xperia_stick_active[1] && x >= (966-360)) {
            xperia_stick_active[1] = 1;
            xperia_stick_pointer[1] = id;
            update_stick = 1;
        }
    } else {
        for (int i = 0; i < lenof(xperia_stick_active); i++) {
            if (xperia_stick_active[i] && xperia_stick_pointer[i] == id) {
                if (is_up) {
                    xperia_stick_active[i] = 0;
                    joystick_state[0].stick[i].x = 0;
                    joystick_state[0].stick[i].y = 0;
                    (*event_callback)(&(InputEvent){
                        .type = INPUT_EVENT_JOYSTICK,
                        .detail = INPUT_JOYSTICK_STICK_CHANGE,
                        .timestamp = timestamp,
                        {.joystick = {.device = index, .index = i,
                                      .x = 0, .y = 0}}});
                } else {
                    update_stick = (int)i;
                }
                break;
            }
        }
    }

    if (update_stick >= 0) {
        /* The central points are offset (180,180) from each edge, but
         * we tweak the parameters a bit for more natural behavior when
         * used by real people. */
        if (update_stick == 1) {
            x -= (966-160);
        } else {
            x -= 160;
        }
        y = 170 - y;  // Y=0 is the bottom edge rather than the top.
        x /= 140;
        y /= 140;
        float r = sqrtf(x*x + y*y);
        if (r > 0) {
            x /= r;
            y /= r;
            if (r > 1) {
                r = 1;
            }
            if (r < XPERIA_STICK_DEADZONE) {
                x = y = 0;
            } else {
                x *= (r - XPERIA_STICK_DEADZONE) / (1 - XPERIA_STICK_DEADZONE);
                y *= (r - XPERIA_STICK_DEADZONE) / (1 - XPERIA_STICK_DEADZONE);
            }
        }
        joystick_state[0].stick[update_stick].x = x;
        joystick_state[0].stick[update_stick].y = y;
        (*event_callback)(&(InputEvent){
            .type = INPUT_EVENT_JOYSTICK,
            .detail = INPUT_JOYSTICK_STICK_CHANGE,
            .timestamp = timestamp,
            {.joystick = {.device = index, .index = update_stick,
                          .x = x, .y = y}}});
    }
}

/*-----------------------------------------------------------------------*/

static void handle_mouse_motion(AInputEvent *event)
{
    const double timestamp =
        convert_java_timestamp(AMotionEvent_getEventTime(event));
    const int32_t status = AMotionEvent_getAction(event);
    const int action = status & AMOTION_EVENT_ACTION_MASK;
    const int index = ((status & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                       >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);

    if (action == AMOTION_EVENT_ACTION_MOVE
     || action == AMOTION_EVENT_ACTION_HOVER_MOVE) {
        InputEvent template = {
            .type = INPUT_EVENT_MOUSE, .detail = INPUT_MOUSE_MOVE};
        send_motion_events(event, index, &template, &template.mouse.x,
                           &template.mouse.y);
        mouse_x = template.mouse.x;
        mouse_y = template.mouse.y;
    }

    const int left = (action == AMOTION_EVENT_ACTION_DOWN
                   || action == AMOTION_EVENT_ACTION_POINTER_DOWN
                   || action == AMOTION_EVENT_ACTION_MOVE);
    /* The middle and right buttons are handled through key events.  ICS
     * provides the AMotionEvent_getButtonState() function, but it doesn't
     * seem to return the advertised values. */
    if (left != mouse_left) {
        (*event_callback)(&(InputEvent){
            .type = INPUT_EVENT_MOUSE,
            .detail = (left ? INPUT_MOUSE_LMB_DOWN : INPUT_MOUSE_LMB_UP),
            .timestamp = timestamp,
            {.mouse = {.x = mouse_x, .y = mouse_y}}});
        mouse_left = left;
    }

    if (action == AMOTION_EVENT_ACTION_SCROLL && have_getAxisValue()) {
        const float hscroll = AMotionEvent_getAxisValue(
            event, AMOTION_EVENT_AXIS_HSCROLL, index);
        const float vscroll = AMotionEvent_getAxisValue(
            event, AMOTION_EVENT_AXIS_VSCROLL, index);
        if (hscroll) {
            (*event_callback)(&(InputEvent){
                .type = INPUT_EVENT_MOUSE, .detail = INPUT_MOUSE_SCROLL_H,
                .timestamp = timestamp,
                {.mouse = {.x = mouse_x, .y = mouse_y, .scroll = hscroll}}});
        }
        if (vscroll) {
            (*event_callback)(&(InputEvent){
                .type = INPUT_EVENT_MOUSE, .detail = INPUT_MOUSE_SCROLL_V,
                .timestamp = timestamp,
                {.mouse = {.x = mouse_x, .y = mouse_y, .scroll = vscroll}}});
        }
    }
}

/*-----------------------------------------------------------------------*/

static void handle_joystick_key(AInputEvent *event, int index)
{
    const double timestamp =
        convert_java_timestamp(AKeyEvent_getEventTime(event));
    const int action = AKeyEvent_getAction(event);
    const int pressed = (action == AKEY_EVENT_ACTION_DOWN);
    const int32_t keycode = AKeyEvent_getKeyCode(event);

    if (pressed) {
        DLOG("Joystick %d: key %d pressed", index, keycode);
    }

    switch (keycode) {
      case AKEYCODE_DPAD_UP:
        joystick_state[index].dpad_up = pressed;
        goto do_dpad;
      case AKEYCODE_DPAD_DOWN:
        joystick_state[index].dpad_down = pressed;
        goto do_dpad;
      case AKEYCODE_DPAD_LEFT:
        joystick_state[index].dpad_left = pressed;
        goto do_dpad;
      case AKEYCODE_DPAD_RIGHT:
        joystick_state[index].dpad_right = pressed;
      do_dpad:
        joystick_state[index].dpad_is_hat = 0;
        (*event_callback)(&(InputEvent){
            .type = INPUT_EVENT_JOYSTICK, .detail = INPUT_JOYSTICK_DPAD_CHANGE,
            .timestamp = timestamp,
            {.joystick = {.device = index,
                          .x = (joystick_state[index].dpad_right
                                - joystick_state[index].dpad_left),
                          .y = (joystick_state[index].dpad_down
                                - joystick_state[index].dpad_up)}}});
        break;

      default:
        for (int i = 0; i < lenof(joystick_state[index].button); i++) {
            if (keycode == joystick_state[index].button_key[i]) {
                joystick_state[index].button[i] = pressed;
                if (i == ANDROID_JOY_BUTTON_L2 || i == ANDROID_JOY_BUTTON_R2) {
                    joystick_state[index].l2r2_axes_only = 0;
                }
                (*event_callback)(&(InputEvent){
                    .type = INPUT_EVENT_JOYSTICK,
                    .detail = (pressed ? INPUT_JOYSTICK_BUTTON_DOWN
                                       : INPUT_JOYSTICK_BUTTON_UP),
                    .timestamp = timestamp,
                    {.joystick = {.device = index, .index = i}}});
            }
        }
    }
}

/*-----------------------------------------------------------------------*/

static void handle_mouse_key(AInputEvent *event)
{
    const double timestamp =
        convert_java_timestamp(AKeyEvent_getEventTime(event));
    const int action = AKeyEvent_getAction(event);
    const int pressed = (action == AKEY_EVENT_ACTION_DOWN);
    const int32_t keycode = AKeyEvent_getKeyCode(event);

    DLOG("Mouse: key %d %s", keycode, pressed ? "pressed" : "released");

    switch (keycode) {
      case AKEYCODE_BACK:
        if (pressed != mouse_right) {
            (*event_callback)(&(InputEvent){
                .type = INPUT_EVENT_MOUSE,
                .detail = (pressed ? INPUT_MOUSE_RMB_DOWN : INPUT_MOUSE_RMB_UP),
                .timestamp = timestamp,
                {.mouse = {.x = mouse_x, .y = mouse_y}}});
            mouse_right = pressed;
        }
        break;
      case AKEYCODE_MENU:
        if (pressed != mouse_middle) {
            (*event_callback)(&(InputEvent){
                .type = INPUT_EVENT_MOUSE,
                .detail = (pressed ? INPUT_MOUSE_MMB_DOWN : INPUT_MOUSE_MMB_UP),
                .timestamp = timestamp,
                {.mouse = {.x = mouse_x, .y = mouse_y}}});
            mouse_middle = pressed;
        }
        break;
    }
}

/*-----------------------------------------------------------------------*/

static void handle_generic_key(AInputEvent *event)
{
    const double timestamp =
        convert_java_timestamp(AKeyEvent_getEventTime(event));
    const int action = AKeyEvent_getAction(event);
    if (action == AKEY_EVENT_ACTION_MULTIPLE) {
        return;
    }
    const int pressed = (action == AKEY_EVENT_ACTION_DOWN);
    const int32_t keycode = AKeyEvent_getKeyCode(event);

    if (is_xperia_play) {
        /* For some reason, SELECT and START don't show up on the gamepad
           device. */
        int button;
        if (keycode == AKEYCODE_BUTTON_SELECT) {
            button = ANDROID_JOY_BUTTON_SELECT;
        } else if (keycode == AKEYCODE_BUTTON_START) {
            button = ANDROID_JOY_BUTTON_START;
        } else {
            button = -1;
        }
        if (button >= 0) {
            joystick_state[0].button[button] = pressed;
            (*event_callback)(&(InputEvent){
                .type = INPUT_EVENT_JOYSTICK,
                .detail = pressed ? INPUT_JOYSTICK_BUTTON_DOWN
                                  : INPUT_JOYSTICK_BUTTON_UP,
                .timestamp = timestamp,
                {.joystick = {.device = 0, .index = button}}});
            return;
        }
    }

    static const int key_xlate[] = {
        [AKEYCODE_0                 ] = KEY_0,
        [AKEYCODE_1                 ] = KEY_1,
        [AKEYCODE_2                 ] = KEY_2,
        [AKEYCODE_3                 ] = KEY_3,
        [AKEYCODE_4                 ] = KEY_4,
        [AKEYCODE_5                 ] = KEY_5,
        [AKEYCODE_6                 ] = KEY_6,
        [AKEYCODE_7                 ] = KEY_7,
        [AKEYCODE_8                 ] = KEY_8,
        [AKEYCODE_9                 ] = KEY_9,
        [AKEYCODE_DPAD_UP           ] = KEY_UP,
        [AKEYCODE_DPAD_DOWN         ] = KEY_DOWN,
        [AKEYCODE_DPAD_LEFT         ] = KEY_LEFT,
        [AKEYCODE_DPAD_RIGHT        ] = KEY_RIGHT,
        [AKEYCODE_A                 ] = KEY_A,
        [AKEYCODE_B                 ] = KEY_B,
        [AKEYCODE_C                 ] = KEY_C,
        [AKEYCODE_D                 ] = KEY_D,
        [AKEYCODE_E                 ] = KEY_E,
        [AKEYCODE_F                 ] = KEY_F,
        [AKEYCODE_G                 ] = KEY_G,
        [AKEYCODE_H                 ] = KEY_H,
        [AKEYCODE_I                 ] = KEY_I,
        [AKEYCODE_J                 ] = KEY_J,
        [AKEYCODE_K                 ] = KEY_K,
        [AKEYCODE_L                 ] = KEY_L,
        [AKEYCODE_M                 ] = KEY_M,
        [AKEYCODE_N                 ] = KEY_N,
        [AKEYCODE_O                 ] = KEY_O,
        [AKEYCODE_P                 ] = KEY_P,
        [AKEYCODE_Q                 ] = KEY_Q,
        [AKEYCODE_R                 ] = KEY_R,
        [AKEYCODE_S                 ] = KEY_S,
        [AKEYCODE_T                 ] = KEY_T,
        [AKEYCODE_U                 ] = KEY_U,
        [AKEYCODE_V                 ] = KEY_V,
        [AKEYCODE_W                 ] = KEY_W,
        [AKEYCODE_X                 ] = KEY_X,
        [AKEYCODE_Y                 ] = KEY_Y,
        [AKEYCODE_Z                 ] = KEY_Z,
        [AKEYCODE_COMMA             ] = KEY_COMMA,
        [AKEYCODE_PERIOD            ] = KEY_PERIOD,
        [AKEYCODE_ALT_LEFT          ] = KEY_LEFTALT,
        [AKEYCODE_ALT_RIGHT         ] = KEY_RIGHTALT,
        [AKEYCODE_SHIFT_LEFT        ] = KEY_LEFTSHIFT,
        [AKEYCODE_SHIFT_RIGHT       ] = KEY_RIGHTSHIFT,
        [AKEYCODE_TAB               ] = KEY_TAB,
        [AKEYCODE_SPACE             ] = KEY_SPACE,
        [AKEYCODE_ENTER             ] = KEY_ENTER,
        [AKEYCODE_DEL               ] = KEY_BACKSPACE,
        [AKEYCODE_GRAVE             ] = KEY_BACKQUOTE,
        [AKEYCODE_MINUS             ] = KEY_HYPHEN,
        [AKEYCODE_EQUALS            ] = KEY_EQUALS,
        [AKEYCODE_LEFT_BRACKET      ] = KEY_LEFTBRACKET,
        [AKEYCODE_RIGHT_BRACKET     ] = KEY_RIGHTBRACKET,
        [AKEYCODE_BACKSLASH         ] = KEY_BACKSLASH,
        [AKEYCODE_SEMICOLON         ] = KEY_SEMICOLON,
        [AKEYCODE_SLASH             ] = KEY_SLASH,
        [AKEYCODE_AT                ] = KEY_ATSIGN,
        [AKEYCODE_NUM               ] = KEY_NUMLOCK,
        [AKEYCODE_PLUS              ] = KEY_PLUS,
        [AKEYCODE_PAGE_UP           ] = KEY_PAGEUP,
        [AKEYCODE_PAGE_DOWN         ] = KEY_PAGEDOWN,
        [AKEYCODE_BACK              ] = KEY_ANDROID_BACK,
        [AKEYCODE_MENU              ] = KEY_MENU,
        [AKEYCODE_ESCAPE            ] = KEY_ESCAPE,
        [AKEYCODE_FORWARD_DEL       ] = KEY_DELETE,
        [AKEYCODE_CTRL_LEFT         ] = KEY_LEFTCONTROL,
        [AKEYCODE_CTRL_RIGHT        ] = KEY_RIGHTCONTROL,
        [AKEYCODE_CAPS_LOCK         ] = KEY_CAPSLOCK,
        [AKEYCODE_SCROLL_LOCK       ] = KEY_SCROLLLOCK,
        [AKEYCODE_META_LEFT         ] = KEY_LEFTMETA,
        [AKEYCODE_META_RIGHT        ] = KEY_RIGHTMETA,
        [AKEYCODE_BREAK             ] = KEY_PAUSE,
        [AKEYCODE_MOVE_HOME         ] = KEY_HOME,
        [AKEYCODE_MOVE_END          ] = KEY_END,
        [AKEYCODE_INSERT            ] = KEY_INSERT,
        [AKEYCODE_F1                ] = KEY_F1,
        [AKEYCODE_F2                ] = KEY_F2,
        [AKEYCODE_F3                ] = KEY_F3,
        [AKEYCODE_F4                ] = KEY_F4,
        [AKEYCODE_F5                ] = KEY_F5,
        [AKEYCODE_F6                ] = KEY_F6,
        [AKEYCODE_F7                ] = KEY_F7,
        [AKEYCODE_F8                ] = KEY_F8,
        [AKEYCODE_F9                ] = KEY_F9,
        [AKEYCODE_F10               ] = KEY_F10,
        [AKEYCODE_F11               ] = KEY_F11,
        [AKEYCODE_F12               ] = KEY_F12,
        [AKEYCODE_NUM_LOCK          ] = KEY_NUMLOCK,
        [AKEYCODE_NUMPAD_0          ] = KEY_NUMPAD_0,
        [AKEYCODE_NUMPAD_1          ] = KEY_NUMPAD_1,
        [AKEYCODE_NUMPAD_2          ] = KEY_NUMPAD_2,
        [AKEYCODE_NUMPAD_3          ] = KEY_NUMPAD_3,
        [AKEYCODE_NUMPAD_4          ] = KEY_NUMPAD_4,
        [AKEYCODE_NUMPAD_5          ] = KEY_NUMPAD_5,
        [AKEYCODE_NUMPAD_6          ] = KEY_NUMPAD_6,
        [AKEYCODE_NUMPAD_7          ] = KEY_NUMPAD_7,
        [AKEYCODE_NUMPAD_8          ] = KEY_NUMPAD_8,
        [AKEYCODE_NUMPAD_9          ] = KEY_NUMPAD_9,
        [AKEYCODE_NUMPAD_DIVIDE     ] = KEY_NUMPAD_DIVIDE,
        [AKEYCODE_NUMPAD_MULTIPLY   ] = KEY_NUMPAD_MULTIPLY,
        [AKEYCODE_NUMPAD_SUBTRACT   ] = KEY_NUMPAD_SUBTRACT,
        [AKEYCODE_NUMPAD_ADD        ] = KEY_NUMPAD_ADD,
        [AKEYCODE_NUMPAD_DOT        ] = KEY_NUMPAD_DECIMAL,
        [AKEYCODE_NUMPAD_ENTER      ] = KEY_NUMPAD_ENTER,
        [AKEYCODE_NUMPAD_EQUALS     ] = KEY_NUMPAD_EQUALS,
        [AKEYCODE_YEN               ] = KEY_YEN,
    };
    if (keycode >= 0 && keycode < lenof(key_xlate)
     && key_xlate[keycode] != 0) {
        (*event_callback)(&(InputEvent){
            .type = INPUT_EVENT_KEYBOARD,
            .detail = pressed ? INPUT_KEYBOARD_KEY_DOWN
                              : INPUT_KEYBOARD_KEY_UP,
            .timestamp = timestamp,
            {.keyboard = {.key = key_xlate[keycode], .system_key = keycode}}});
    } else {
        DLOG("Unrecognized key %d, meta 0x%X",
             keycode, AKeyEvent_getMetaState(event));
        (*event_callback)(&(InputEvent){
            .type = INPUT_EVENT_KEYBOARD,
            .detail = pressed ? INPUT_KEYBOARD_SYSTEM_KEY_DOWN
                              : INPUT_KEYBOARD_SYSTEM_KEY_UP,
            .timestamp = timestamp,
            {.keyboard = {.key = KEY__NONE, .system_key = keycode}}});
    }
}

/*-----------------------------------------------------------------------*/

static void update_text_dialog(void)
{
    ASSERT(text_dialog != NULL, return);

    InputEvent event = {.type = INPUT_EVENT_TEXT, .timestamp = time_now()};

    JNIEnv *env = get_jni_env();
    jobject activity_obj = android_activity->clazz;

    /* If the dialog is still running, there's nothing to do. */
    const int finished = (*env)->CallBooleanMethod(
        env, activity_obj, isInputDialogFinished, text_dialog);
    ASSERT(!clear_exceptions(env), text_dialog = 0; goto cancel);
    if (!finished) {
        return;
    }

    /* Grab the text string and close the dialog immediately, so we
     * can get the calls to activity_obj out of the way. */
    jstring j_text = (*env)->CallObjectMethod(
        env, activity_obj, getInputDialogText, text_dialog);
    (*env)->CallVoidMethod(env, activity_obj, dismissInputDialog, text_dialog);
    (*env)->DeleteGlobalRef(env, text_dialog);
    text_dialog = 0;
    ASSERT(!clear_exceptions(env), j_text = 0);

    /* If we got a null string, the dialog was cancelled. */
    if (!j_text) {
      cancel:
        event.detail = INPUT_TEXT_CANCELLED;
        (*event_callback)(&event);
        return;
    }

    /* Retrieve and return the string contents. */
    const char *text = (*env)->GetStringUTFChars(env, j_text, NULL);
    if (!text) {
        DLOG("Failed to get input string");
        (*env)->DeleteLocalRef(env, j_text);
        goto cancel;
    }
    event.detail = INPUT_TEXT_CLEAR;
    (*event_callback)(&event);
    event.detail = INPUT_TEXT_INPUT;
    const char *s = text;
    int32_t ch;
    while ((ch = utf8_read(&s)) != 0) {
        if (ch > 0) {
            event.text.ch = ch;
            (*event_callback)(&event);
        }
    }
    (*env)->ReleaseStringUTFChars(env, j_text, text);
    (*env)->DeleteLocalRef(env, j_text);
    event.detail = INPUT_TEXT_DONE;
    (*event_callback)(&event);
}

/*-----------------------------------------------------------------------*/

static void send_motion_events(
    AInputEvent *event, int pointer_index,
    InputEvent *template, float *x_ptr, float *y_ptr)
{
    const float width = android_display_width();
    const float height = android_display_height();

    const int history_size = AMotionEvent_getHistorySize(event);
    for (int i = 0; i < history_size; i++) {
        template->timestamp = convert_java_timestamp(
            AMotionEvent_getHistoricalEventTime(event, i));
        *x_ptr = AMotionEvent_getHistoricalX(event, pointer_index, i) / width;
        *y_ptr = AMotionEvent_getHistoricalY(event, pointer_index, i) / height;
        (*event_callback)(template);
    }

    template->timestamp = convert_java_timestamp(
        AMotionEvent_getEventTime(event));
    *x_ptr = AMotionEvent_getX(event, pointer_index) / width;
    *y_ptr = AMotionEvent_getY(event, pointer_index) / height;
    (*event_callback)(template);
}

/*-----------------------------------------------------------------------*/

static void set_java_time_offset(void)
{
    java_time_offset_known = 0;
    java_time_offset = 0;

    if (UNLIKELY(sys_posix_time_epoch() == 0)) {
        return;  // We'll have to work it out later.
    }

    JNIEnv *env = get_jni_env();
    jclass System_class = get_class("java.lang.System");
    ASSERT(System_class != 0, return);
    jmethodID nanoTime = get_static_method(System_class, "nanoTime", "()J");
    ASSERT(nanoTime != 0, return);
    const uint64_t java_time = (*env)->CallStaticLongMethod(
        env, System_class, nanoTime);
    uint64_t sys_time = sys_time_now();
    if (sys_time_unit() != 1000000000) {
        ASSERT(sys_time_unit() == 1000000, return);
        sys_time *= 1000;
    }
    java_time_offset = sys_time - java_time;
    java_time_offset_known = 1;
}

/*-----------------------------------------------------------------------*/

static double convert_java_timestamp(uint64_t time)
{
    if (LIKELY(nanotime_uses_clock_monotonic)) {
        return (time - sys_posix_time_epoch()) * 1.0e-9;
    }

    if (UNLIKELY(!java_time_offset_known)) {
        set_java_time_offset();
    }
    return (time + java_time_offset) * 1.0e-9;
}

/*************************************************************************/
/*************************************************************************/
