/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/windows/input.c: Input device interface for Windows.
 */

/*
 * SIL on Windows supports two environment variables for controlling how
 * joysticks are detected and read:
 *
 * - SIL_WINDOWS_USE_RAWINPUT: Set to 0 to disable the raw input
 *   (HID-based) interface.
 *
 * - SIL_WINDOWS_USE_XINPUT: Set to 0 to disable the XInput interface.
 *
 * By default, both interfaces are enabled; joysticks supported by XInput
 * will be managed through XInput, and other joysticks will be managed
 * through the raw input interface.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/graphics.h"
#include "src/input.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/misc/joystick-hid.h"
#include "src/sysdep/windows/internal.h"
#include "src/sysdep/windows/xinput.h"
#include "src/thread.h"
#include "src/time.h"
#include "src/utility/misc.h"
#include "src/utility/utf8.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/*--------------- HID library handles and associated data ---------------*/

static HMODULE hid_module;

static BOOLEAN (__stdcall *p_HidD_GetProductString)(
    HANDLE HidDeviceObject, PVOID Buffer, ULONG BufferLength);
static BOOLEAN (__stdcall *p_HidD_GetSerialNumberString)(
    HANDLE HidDeviceObject, PVOID Buffer, ULONG BufferLength);
static NTSTATUS (__stdcall *p_HidP_GetCaps)(
    PHIDP_PREPARSED_DATA PreparsedData, PHIDP_CAPS Capabilities);
static NTSTATUS (__stdcall *p_HidP_GetSpecificButtonCaps)(
    HIDP_REPORT_TYPE ReportType, USAGE UsagePage, USHORT LinkCollection,
    USAGE Usage, PHIDP_BUTTON_CAPS ButtonCaps, PULONG ButtonCapsLength,
    PHIDP_PREPARSED_DATA PreparsedData);
static NTSTATUS (__stdcall *p_HidP_GetSpecificValueCaps)(
    HIDP_REPORT_TYPE ReportType, USAGE UsagePage, USHORT LinkCollection,
    USAGE Usage, PHIDP_VALUE_CAPS ValueCaps, PULONG ValueCapsLength,
    PHIDP_PREPARSED_DATA PreparsedData);
static NTSTATUS (__stdcall *p_HidP_GetUsageValue)(
    HIDP_REPORT_TYPE ReportType, USAGE UsagePage, USHORT LinkCollection,
    USAGE Usage, PULONG UsageValue, PHIDP_PREPARSED_DATA PreparsedData,
    PCHAR Report, ULONG ReportLength);
static NTSTATUS (__stdcall *p_HidP_GetUsagesEx)(
    HIDP_REPORT_TYPE ReportType, USHORT LinkCollection,
    PUSAGE_AND_PAGE ButtonList, ULONG *UsageLength,
    PHIDP_PREPARSED_DATA PreparsedData, PCHAR Report, ULONG ReportLength);

/*-------------- XInput library handle and associated data --------------*/

static HMODULE xinput_module;

static DWORD (WINAPI *p_XInputGetCapabilities)(
    DWORD dwUserIndex, DWORD dwFlags, XINPUT_CAPABILITIES *pCapabilities);
static DWORD (WINAPI *p_XInputGetState)(
    DWORD dwUserIndex, XINPUT_STATE *pState);
static DWORD (WINAPI *p_XInputSetState)(
    DWORD dwUserIndex, XINPUT_VIBRATION *pVibration);

/* Mapping from SIL button names to XInput button flags. */
static const struct {uint16_t flag; uint8_t name;} xinput_button_map[] = {
    {XINPUT_GAMEPAD_A,              INPUT_JOYBUTTON_FACE_DOWN},
    {XINPUT_GAMEPAD_B,              INPUT_JOYBUTTON_FACE_RIGHT},
    {XINPUT_GAMEPAD_X,              INPUT_JOYBUTTON_FACE_LEFT},
    {XINPUT_GAMEPAD_Y,              INPUT_JOYBUTTON_FACE_UP},
    {XINPUT_GAMEPAD_LEFT_SHOULDER,  INPUT_JOYBUTTON_L1},
    {XINPUT_GAMEPAD_RIGHT_SHOULDER, INPUT_JOYBUTTON_R1},
    {XINPUT_GAMEPAD_BACK,           INPUT_JOYBUTTON_SELECT},
    {XINPUT_GAMEPAD_START,          INPUT_JOYBUTTON_START},
    {XINPUT_GAMEPAD_LEFT_THUMB,     INPUT_JOYBUTTON_L_STICK},
    {XINPUT_GAMEPAD_RIGHT_THUMB,    INPUT_JOYBUTTON_R_STICK},
};

/*--------------------------- Keycode mapping ---------------------------*/

/* Mapping from Windows virtual keycodes to KEY_* symbols (defined as
 * uint8_t to save space, since currently all KEY_* values fit in 8 bits). */
static uint8_t vk_map[0x100] = {
    ['A'          ] = KEY_A,
    ['B'          ] = KEY_B,
    ['C'          ] = KEY_C,
    ['D'          ] = KEY_D,
    ['E'          ] = KEY_E,
    ['F'          ] = KEY_F,
    ['G'          ] = KEY_G,
    ['H'          ] = KEY_H,
    ['I'          ] = KEY_I,
    ['J'          ] = KEY_J,
    ['K'          ] = KEY_K,
    ['L'          ] = KEY_L,
    ['M'          ] = KEY_M,
    ['N'          ] = KEY_N,
    ['O'          ] = KEY_O,
    ['P'          ] = KEY_P,
    ['Q'          ] = KEY_Q,
    ['R'          ] = KEY_R,
    ['S'          ] = KEY_S,
    ['T'          ] = KEY_T,
    ['U'          ] = KEY_U,
    ['V'          ] = KEY_V,
    ['W'          ] = KEY_W,
    ['X'          ] = KEY_X,
    ['Y'          ] = KEY_Y,
    ['Z'          ] = KEY_Z,
    ['0'          ] = KEY_0,
    ['1'          ] = KEY_1,
    ['2'          ] = KEY_2,
    ['3'          ] = KEY_3,
    ['4'          ] = KEY_4,
    ['5'          ] = KEY_5,
    ['6'          ] = KEY_6,
    ['7'          ] = KEY_7,
    ['8'          ] = KEY_8,
    ['9'          ] = KEY_9,
    [VK_BACK      ] = KEY_BACKSPACE,
    [VK_TAB       ] = KEY_TAB,
    [VK_RETURN    ] = KEY_ENTER,
    [VK_SHIFT     ] = KEY_LEFTSHIFT,
    [VK_CONTROL   ] = KEY_LEFTCONTROL,
    [VK_MENU      ] = KEY_LEFTALT,
    [VK_PAUSE     ] = KEY_PAUSE,
    [VK_CAPITAL   ] = KEY_CAPSLOCK,
    [VK_ESCAPE    ] = KEY_ESCAPE,
    [VK_SPACE     ] = KEY_SPACE,
    [VK_PRIOR     ] = KEY_PAGEUP,
    [VK_NEXT      ] = KEY_PAGEDOWN,
    [VK_END       ] = KEY_END,
    [VK_HOME      ] = KEY_HOME,
    [VK_LEFT      ] = KEY_LEFT,
    [VK_UP        ] = KEY_UP,
    [VK_RIGHT     ] = KEY_RIGHT,
    [VK_DOWN      ] = KEY_DOWN,
    [VK_PRINT     ] = KEY_PRINTSCREEN,
    [VK_INSERT    ] = KEY_INSERT,
    [VK_DELETE    ] = KEY_DELETE,
    [VK_LWIN      ] = KEY_LEFTMETA,
    [VK_RWIN      ] = KEY_RIGHTMETA,
    [VK_NUMPAD0   ] = KEY_NUMPAD_0,
    [VK_NUMPAD1   ] = KEY_NUMPAD_1,
    [VK_NUMPAD2   ] = KEY_NUMPAD_2,
    [VK_NUMPAD3   ] = KEY_NUMPAD_3,
    [VK_NUMPAD4   ] = KEY_NUMPAD_4,
    [VK_NUMPAD5   ] = KEY_NUMPAD_5,
    [VK_NUMPAD6   ] = KEY_NUMPAD_6,
    [VK_NUMPAD7   ] = KEY_NUMPAD_7,
    [VK_NUMPAD8   ] = KEY_NUMPAD_8,
    [VK_NUMPAD9   ] = KEY_NUMPAD_9,
    [VK_MULTIPLY  ] = KEY_NUMPAD_MULTIPLY,
    [VK_ADD       ] = KEY_NUMPAD_ADD,
    [VK_SUBTRACT  ] = KEY_NUMPAD_SUBTRACT,
    [VK_DECIMAL   ] = KEY_NUMPAD_DECIMAL,
    [VK_DIVIDE    ] = KEY_NUMPAD_DIVIDE,
    [VK_F1        ] = KEY_F1,
    [VK_F2        ] = KEY_F2,
    [VK_F3        ] = KEY_F3,
    [VK_F4        ] = KEY_F4,
    [VK_F5        ] = KEY_F5,
    [VK_F6        ] = KEY_F6,
    [VK_F7        ] = KEY_F7,
    [VK_F8        ] = KEY_F8,
    [VK_F9        ] = KEY_F9,
    [VK_F10       ] = KEY_F10,
    [VK_F11       ] = KEY_F11,
    [VK_F12       ] = KEY_F12,
    [VK_F13       ] = KEY_F13,
    [VK_F14       ] = KEY_F14,
    [VK_F15       ] = KEY_F15,
    [VK_F16       ] = KEY_F16,
    [VK_F17       ] = KEY_F17,
    [VK_F18       ] = KEY_F18,
    [VK_F19       ] = KEY_F19,
    [VK_F20       ] = KEY_F20,
    [VK_NUMLOCK   ] = KEY_NUMLOCK,
    [VK_SCROLL    ] = KEY_SCROLLLOCK,
    [VK_LSHIFT    ] = KEY_LEFTSHIFT,
    [VK_RSHIFT    ] = KEY_RIGHTSHIFT,
    [VK_LCONTROL  ] = KEY_LEFTCONTROL,
    [VK_RCONTROL  ] = KEY_RIGHTCONTROL,
    [VK_LMENU     ] = KEY_LEFTALT,
    [VK_RMENU     ] = KEY_RIGHTALT,
    /* The VK_OEM_* keys are set at runtime. */
};  // vk_map[]

/*------------------------- Other runtime data --------------------------*/

/* Has the input subsystem been initialized?  (Input messages are discarded
 * if received when this is false.) */
static uint8_t initted;

/* Event callback passed to sys_input_init(). */
static InputEventCallback event_callback;

/* Mutex held by input message handler, used to ensure no messages are
 * being processed while we free stuff in cleanup.  This is a raw
 * CRITICAL_SECTION rather than a dynamically-allocated SysMutexID so we
 * don't trigger memory leak errors in tests. */
static CRITICAL_SECTION input_message_lock;


/* Thread for discovering new and disconnected joysticks. */
static SysThreadID joystick_discovery_thread_id;

/* Semaphore used to stop the joystick discovery thread. */
static SysSemaphoreID joystick_discovery_stop_sem;

/* Mutex for accessing the joysticks[] and joystick_info[] arrays.  Note
 * that joystick discovery may cause the arrays to be reallocated, so
 * pointers to individual elements should not be maintained without the
 * mutex held. */
static SysMutexID joystick_mutex;

/* Descriptors passed to RegisterRawInputDevices() to enable joystick input.
 * The dwFlags and hwndTarget fields are rewritten as needed. */
static RAWINPUTDEVICE joystick_descs[2] = {
    {.usUsagePage = HID_PAGE_GENERIC_DESKTOP,
     .usUsage = HID_USAGE_JOYSTICK,
     .dwFlags = 0,
     .hwndTarget = NULL},
    {.usUsagePage = HID_PAGE_GENERIC_DESKTOP,
     .usUsage = HID_USAGE_GAMEPAD,
     .dwFlags = 0,
     .hwndTarget = NULL},
};

/* Number of available joystick devices. */
static int num_joysticks;

/* Data for each joystick device. */
typedef struct JoystickInfo JoystickInfo;
struct JoystickInfo {

    /******** Common data ********/

    /* Number of buttons and sticks available on the device. */
    int num_buttons, num_sticks;

    /* Timeout for the current rumble action, or 0 if no rumble is active. */
    double rumble_timeout;

    /* Is this an XInput device (true) or raw input device (false)? */
    uint8_t is_xinput;

    /******** XInput device data ********/

    /* Device index passed as dwUserIndex to XInput functions. */
    int xinput_device;

    /* Capabilities structure returned by XInputGetCapabilities(). */
    XINPUT_CAPABILITIES xinput_caps;

    /* Does this joystick have a D-pad?  (Derived from xinput_caps.) */
    uint8_t has_dpad;

    /* Mapping from logical to physical buttons. */
    int8_t button_map[INPUT_JOYBUTTON__NUM];

    /* Current state of all inputs. */
    uint8_t button_state[INPUT_MAX_JOYSTICK_BUTTONS];
    uint8_t dpad_state_up, dpad_state_down, dpad_state_left, dpad_state_right;
    Vector2f stick_state[3];

    /******** Raw-input device data ********/

    /* Device handle. */
    HANDLE device;

    /* Raw device handle (for rumble support). */
    HANDLE raw_device;

    /* Preparsed data from GetRawInputDeviceInfo() (pointer). */
    PHIDP_PREPARSED_DATA preparsed_data;

    /* Current state of button and value inputs, used to detect changes in
     * input handling (since Windows doesn't tell us what changed). */
    struct JoystickInputState {
        uint8_t is_button;  // Is this a button (true) or value (false) input?
        uint8_t bit_width;  // Bit width of value inputs.
        uint8_t is_signed;  // Is this value input signed?
        uint16_t usage_page;
        uint16_t usage;
        int value;
    } *input_state;
    int num_input_state;

    /* Handle for common HID processing. */
    HIDJoystickHandle *hid_handle;
};
static JoystickInfo *joysticks;

/* Joystick information array returned for sys_input_info(). */
static SysInputJoystick *joystick_info;

/* Should we send joystick events while the window is inactive? */
static uint8_t joystick_ignore_focus;


/* Text input flag. */
static uint8_t text_active;

/* Most recently received WM_CHAR high surrogate.  Used to reconstruct the
 * actual character when the low surrogate is received.  0 indicates that
 * the last received character was not a high surrogate. */
static uint16_t pending_utf16_surrogate;


/* Flag: Is touch input available? */
static uint8_t touch_available;
/* Flag: Convert touch events to mouse events? */
static uint8_t touch_to_mouse;
/* Mapping of Windows pointer IDs to SIL touch IDs.  id == 0 indicates a
 * free entry. */
static struct {
    int pointer;
    unsigned int id;
} touch_map[INPUT_MAX_TOUCHES];
/* Next touch ID to use for a new touch.  Incremented by 1 for each touch,
 * rolling over (and skipping zero) if necessary. */
static unsigned int next_touch_id = 1;

/*--------------------- Local routine declarations ----------------------*/

/**
 * scan_rawinput_joysticks:  Scan the raw input device list for new or
 * disconnected joystick devices and update the joystick list accordingly.
 */
static void scan_rawinput_joysticks(void);

/**
 * add_rawinput_joystick:  Determine whether the given device is a
 * joystick-type device and, if so, add it to the joystick list.
 *
 * [Parameters]
 *     device: Raw input device handle.
 */
static void add_rawinput_joystick(HANDLE device);

/**
 * handle_rawinput_joystick:  Process input from a joystick WM_INPUT event.
 *
 * joystick_mutex is assumed to be locked on entry.
 *
 * [Parameters]
 *     num: Device number (0 through num_joysticks-1).
 *     data: Raw input data from the WM_INPUT message.
 */
static void handle_rawinput_joystick(int num, RAWINPUT *data);

/**
 * gridi:  Wrapper for GetRawInputDeviceInfo() which requests the size of
 * the data to be retrieved, then allocates a buffer of that size and
 * returns the data stored in that buffer.  The buffer should be freed
 * with mem_free() when no longer needed.
 *
 * [Parameters]
 *     device: Device handle.
 *     command: GetRawInputDeviceInfo() command (RIDI_*).
 * [Return value]
 *     Newly allocated buffer containing the requested data, or NULL on error.
 */
static void *gridi(HANDLE device, UINT command);

/**
 * scan_xinput_joysticks:  Add any XInput joysticks in the system to the
 * joystick list.
 */
static void scan_xinput_joysticks(void);

/**
 * poll_xinput_joystick:  Poll the given XInput joystick device for state
 * changes, and generate appropriate input events.
 *
 * joystick_mutex is assumed to be locked on entry.
 *
 * [Parameters]
 *     num: Device number (0 through num_joysticks-1).
 */
static void poll_xinput_joystick(int num);

/**
 * handle_xinput_button:  Check for a button state change on an XInput
 * joystick device and generate an input event if appropriate.
 *
 * [Parameters]
 *     joystick: JoystickInfo structure for joystick.
 *     index: Button index.
 *     value: Button value (true = pressed, false = not pressed).
 *     event: Input event template (must have all fields except detail and
 *         joystick.index filled in).
 */
static void handle_xinput_button(JoystickInfo *joystick, int index,
                                 int value, InputEvent *event);

/**
 * handle_xinput_stick:  Check for an analog stick state change on an
 * XInput joystick device and generate an input event if appropriate.
 *
 * [Parameters]
 *     joystick: JoystickInfo structure for joystick.
 *     index: Stick index.
 *     raw_x, raw_y: Raw input values.
 *     event: Input event template (must have all fields except detail and
 *         joystick.index filled in).
 */
static void handle_xinput_stick(JoystickInfo *joystick, int index,
                                int raw_x, int raw_y, InputEvent *event);

/**
 * scale_xinput_axis:  Scale a raw XInput analog stick axis input to the
 * range [-1.0,+1.0].
 *
 * [Parameters]
 *     raw_value: Raw input value, in the range [-32768,+32767].
 * [Return value]
 *     Scaled input value, in the range [-1.0,+1.0].
 */
static CONST_FUNCTION float scale_xinput_axis(int raw_value);

/**
 * joystick_discovery_thread:  Thread routine which periodically polls the
 * system to see if any joysticks have been connected or disconnected.
 *
 * [Parameters]
 *     unused: Thread parameter (unused).
 * [Return value]
 *     0
 */
static int joystick_discovery_thread(void *unused);

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
 * do_rumble:  Start a rumble effect on a joystick device.
 *
 * [Parameters]
 *     joystick: JoystickInfo structure for joystick.
 *     left, right: Left and right motor strength.
 *     time: Length of effect, in seconds.
 */
static void do_rumble(JoystickInfo *joystick, float left, float right,
                      float time);

/**
 * update_vk_map:  Update the vk_map[] array with appropriate mappings from
 * Windows virtual keycodes to SIL keycodes based on the current keyboard
 * layout.
 */
static void update_vk_map(void);

/**
 * handle_mouse_event:  Process a mouse input event.
 *
 * [Parameters]
 *     detail: Event detail code (INPUT_MOUSE_*).
 *     lParam: lParam value from input event.
 *     scroll: Scroll count.
 */
static void handle_mouse_event(InputEventDetail detail, LPARAM lParam,
                               float scroll);

/**
 * handle_touch_event:  Process a touch input event.
 *
 * [Parameters]
 *     detail: Event detail code (INPUT_MOUSE_*).
 *     wParam: wParam value from input event.
 *     lParam: lParam value from input event.
 * [Return value]
 *     True if the event was consumed, false if not.
 */
static int handle_touch_event(InputEventDetail detail, WPARAM wParam,
                              LPARAM lParam);

/**
 * lookup_touch:  Look up the touch with the given pointer ID in touch_map.
 * If the ID is not found and "new" is true, allocate a new entry for the
 * touch (if one is free).
 *
 * [Parameters]
 *     pointer: Pointer ID, as returned by GET_POINTERID_WPARAM(wParam).
 *     new: True if the touch is a new touch, false if not.
 * [Return value]
 *     Index into touch_map[] of the entry for the given touch, or -1 if none.
 */
static int lookup_touch(int pointer, int new);

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int sys_input_init(void (*event_callback_)(const struct InputEvent *))
{
    PRECOND(event_callback_ != NULL, return 0);

    event_callback = event_callback_;

    char *rawinput_hint = windows_getenv("SIL_WINDOWS_USE_RAWINPUT");
    if (rawinput_hint && !*rawinput_hint) {
        mem_free(rawinput_hint);
        rawinput_hint = NULL;
    }
    if (!rawinput_hint || strcmp(rawinput_hint, "0") != 0) {
        hid_module = LoadLibrary("hid.dll");
        if (hid_module) {
            #define LOOKUP(name) \
                (p_##name = (void *)GetProcAddress(hid_module, #name))
            if (!LOOKUP(HidD_GetProductString)
             || !LOOKUP(HidD_GetSerialNumberString)
             || !LOOKUP(HidP_GetCaps)
             || !LOOKUP(HidP_GetSpecificButtonCaps)
             || !LOOKUP(HidP_GetSpecificValueCaps)
             || !LOOKUP(HidP_GetUsageValue)
             || !LOOKUP(HidP_GetUsagesEx)) {
                FreeLibrary(hid_module);
                hid_module = NULL;
                if (rawinput_hint) {
                    DLOG("Raw input joystick support was requested, but"
                         " HID.DLL was not found or incompatible");
                }
            }
            #undef LOOKUP
        }
    }
    mem_free(rawinput_hint);

    char *xinput_hint = windows_getenv("SIL_WINDOWS_USE_XINPUT");
    if (xinput_hint && !*xinput_hint) {
        mem_free(xinput_hint);
        xinput_hint = NULL;
    }
    if (!xinput_hint || strcmp(xinput_hint, "0") != 0) {
        /* Prefer xinput1_4.dll if available since that's standard with
         * Windows 8 and later.  Don't attempt to load xinput9_1_0.dll
         * (standard with Windows Vista and 7) since it doesn't properly
         * report device capabilities. */
        xinput_module = LoadLibrary("xinput1_4.dll");
        if (!xinput_module) {
            xinput_module = LoadLibrary("xinput1_3.dll");
        }
        if (xinput_module) {
            #define LOOKUP(name) \
                (p_##name = (void *)GetProcAddress(xinput_module, #name))
            if (!LOOKUP(XInputGetCapabilities)
             || !LOOKUP(XInputGetState)
             || !LOOKUP(XInputSetState)) {
                FreeLibrary(xinput_module);
                xinput_module = NULL;
                if (xinput_hint) {
                    DLOG("XInput joystick support was requested, but"
                         " XINPUT1_3.DLL and XINPUT1_4.DLL were not found"
                         " or incompatible");
                }
            }
            #undef LOOKUP
        }
    }
    mem_free(xinput_hint);

    joystick_ignore_focus = 1;
    joystick_mutex = sys_mutex_create(0, 0);
    if (UNLIKELY(!joystick_mutex)) {
        DLOG("Failed to create mutex for joystick array");
        goto error_free_xinput_module;
    }
    if (xinput_module) {
        DLOG("Enumerating XInput joysticks");
        scan_xinput_joysticks();
    }
    if (hid_module) {
        DLOG("Enumerating HID joysticks");
        scan_rawinput_joysticks();
    }
    DLOG("%d joysticks found", num_joysticks);
    joystick_discovery_stop_sem = sys_semaphore_create(0, 1);
    if (UNLIKELY(!joystick_discovery_stop_sem)) {
        DLOG("Failed to create joystick discovery stop semaphore");
        goto error_close_joysticks;
    }
    static const ThreadAttributes jdt_attr;  // All zero.
    joystick_discovery_thread_id =
        sys_thread_create(&jdt_attr, joystick_discovery_thread, NULL);
    if (UNLIKELY(!joystick_discovery_thread_id)) {
        DLOG("Failed to create joystick discovery thread");
        goto error_destroy_joystick_discovery_stop_sem;
    }

    touch_available =
        (windows_version_is_at_least(WINDOWS_VERSION_8)
         && (GetSystemMetrics(SM_DIGITIZER) & NID_INTEGRATED_TOUCH));
    touch_to_mouse = 0;

    update_vk_map();
    text_active = 0;
    next_touch_id = 1;

    /* Must be last, so the input message handler doesn't try to process
     * events until everything else is set up. */
    EnterCriticalSection(&input_message_lock);
    initted = 1;
    LeaveCriticalSection(&input_message_lock);

    return 1;

  error_destroy_joystick_discovery_stop_sem:
    sys_semaphore_destroy(joystick_discovery_stop_sem);
    joystick_discovery_stop_sem = 0;
  error_close_joysticks:
    sys_mutex_destroy(joystick_mutex);
    joystick_mutex = 0;
    for (int i = 0; i < num_joysticks; i++) {
        if (!joysticks[i].is_xinput) {
            if (joysticks[i].raw_device) {
                CloseHandle(joysticks[i].raw_device);
            }
            mem_free(joysticks[i].preparsed_data);
            mem_free(joysticks[i].input_state);
            hidjoy_destroy(joysticks[i].hid_handle);
        }
    }
    mem_free(joysticks);
    joysticks = NULL;
    num_joysticks = 0;
    mem_free(joystick_info);
    joystick_info = NULL;
  error_free_xinput_module:
    if (xinput_module) {
        FreeLibrary(xinput_module);
        xinput_module = NULL;
    }
    if (hid_module) {
        FreeLibrary(hid_module);
        hid_module = NULL;
    }
    return 0;
}

/*-----------------------------------------------------------------------*/

void sys_input_cleanup(void)
{
    /* Clear this first as a signal to the window message handler, so
     * that input events received while cleaning up don't cause null
     * pointer dereferences. */
    EnterCriticalSection(&input_message_lock);
    initted = 0;
    LeaveCriticalSection(&input_message_lock);

    if (joystick_discovery_thread_id) {
        sys_semaphore_signal(joystick_discovery_stop_sem);
        sys_thread_wait(joystick_discovery_thread_id, (int[]){0});
        joystick_discovery_thread_id = 0;
    }
    if (joystick_discovery_stop_sem) {
        sys_semaphore_destroy(joystick_discovery_stop_sem);
        joystick_discovery_stop_sem = 0;
    }
    if (joystick_mutex) {
        sys_mutex_destroy(joystick_mutex);
        joystick_mutex = 0;
    }

    for (int i = 0; i < num_joysticks; i++) {
        if (!joysticks[i].is_xinput) {
            if (joysticks[i].raw_device) {
                CloseHandle(joysticks[i].raw_device);
            }
            mem_free(joysticks[i].preparsed_data);
            mem_free(joysticks[i].input_state);
            hidjoy_destroy(joysticks[i].hid_handle);
        }
    }
    mem_free(joysticks);
    joysticks = NULL;
    num_joysticks = 0;
    mem_free(joystick_info);
    joystick_info = NULL;

    if (xinput_module) {
        FreeLibrary(xinput_module);
        xinput_module = NULL;
    }
    if (hid_module) {
        FreeLibrary(hid_module);
        hid_module = NULL;
    }

    event_callback = NULL;
}

/*-----------------------------------------------------------------------*/

void sys_input_update(void)
{
    windows_update_window();

    sys_mutex_lock(joystick_mutex, -1);
    const double now = time_now();
    for (int i = 0; i < num_joysticks; i++) {
        if (joystick_info[i].connected && joysticks[i].rumble_timeout
         && now >= joysticks[i].rumble_timeout) {
            do_rumble(&joysticks[i], 0, 0, 0);
        }
    }
    if (xinput_module && (joystick_ignore_focus || sys_graphics_has_focus())) {
        for (int i = 0; i < num_joysticks; i++) {
            if (joystick_info[i].connected && joysticks[i].is_xinput) {
                poll_xinput_joystick(i);
            }
        }
    }
    sys_mutex_unlock(joystick_mutex);
}

/*-----------------------------------------------------------------------*/

void sys_input_info(SysInputInfo *info_ret)
{
    info_ret->has_joystick = (num_joysticks > 0);
    info_ret->num_joysticks = num_joysticks;
    info_ret->joysticks = joystick_info;

    /* Assume a keyboard and mouse are present. */
    info_ret->has_keyboard = 1;
    info_ret->keyboard_is_full = 1;
    info_ret->has_mouse = 1;

    /* We use Windows text events to provide text entry. */
    info_ret->has_text = 1;
    info_ret->text_uses_custom_interface = 0;
    info_ret->text_has_prompt = 0;

    info_ret->has_touch = touch_available;
}

/*-----------------------------------------------------------------------*/

void sys_input_grab(int grab)
{
    windows_set_mouse_grab(grab);
}

/*-----------------------------------------------------------------------*/

/* sys_input_is_quit_requested() is defined in graphics.c. */

/*-----------------------------------------------------------------------*/

int sys_input_is_suspend_requested(void)
{
    /* Not supported. */
    return 0;
}

/*-----------------------------------------------------------------------*/

void sys_input_acknowledge_suspend_request(void)
{
    /* Not supported. */
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
        if (joysticks[index].is_xinput) {
            retval = mem_strdup("XInput", 0);
        } else {
            retval = mem_strdup(hidjoy_name(joysticks[index].hid_handle), 0);
        }
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
        if (joysticks[index].is_xinput) {
            retval = joysticks[index].button_map[name];
        } else {
            retval = hidjoy_button_mapping(joysticks[index].hid_handle, name);
        }
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
    if (joystick_info[index].connected) {
        do_rumble(joystick, left, right, time);
    }
    sys_mutex_unlock(joystick_mutex);
}

/*************************************************************************/
/*********************** Interface: Mouse handling ***********************/
/*************************************************************************/

void sys_input_mouse_set_position(float x, float y)
{
    const HWND window = windows_window();
    if (!window) {
        return;
    }

    const int width = graphics_display_width();
    const int height = graphics_display_height();
    POINT point = {.x = bound(iroundf(x*width),  0, width-1),
                   .y = bound(iroundf(y*height), 0, height-1)};
    ClientToScreen(window, &point);
    SetCursorPos(point.x, point.y);
}

/*************************************************************************/
/******************** Interface: Text entry handling *********************/
/*************************************************************************/

void sys_input_text_set_state(int on, UNUSED const char *text,
                              UNUSED const char *prompt)
{
    text_active = (on != 0);
}

/*************************************************************************/
/*********************** Exported utility routines ***********************/
/*************************************************************************/

void windows_set_touch_to_mouse(int enable)
{
    touch_to_mouse = (enable != 0);
}

/*************************************************************************/
/****************** Windows-internal exported routines *******************/
/*************************************************************************/

void windows_init_input_message_lock(void)
{
    InitializeCriticalSection(&input_message_lock);
}

/*-----------------------------------------------------------------------*/

int windows_handle_input_message(HWND hwnd, UINT uMsg,
                                 WPARAM wParam, LPARAM lParam)
{
    EnterCriticalSection(&input_message_lock);
    if (!initted) {
        LeaveCriticalSection(&input_message_lock);
        return 0;  // Input subsystem is not yet initialized or in cleanup.
    }

    int retval;

    switch (uMsg) {

      case WM_CREATE:
      case WM_DESTROY:
        if (hid_module) {
            DWORD flags;
            HWND window;
            if (uMsg == WM_CREATE) {
                if (windows_version_is_at_least(WINDOWS_VERSION_VISTA)) {
                    flags = RIDEV_DEVNOTIFY;
                } else {
                    flags = 0;   // DEVNOTIFY is not available in XP.
                }
                flags |= RIDEV_INPUTSINK;
                window = hwnd;
            } else {
                flags = RIDEV_REMOVE;
                window = NULL;
            }
            for (int i = 0; i < lenof(joystick_descs); i++) {
                joystick_descs[i].dwFlags = flags;
                joystick_descs[i].hwndTarget = window;
            }
            if (UNLIKELY(!RegisterRawInputDevices(joystick_descs,
                                                  lenof(joystick_descs),
                                                  sizeof(*joystick_descs)))) {
                DLOG("RegisterRawInputDevices() failed for WM_%s: %s",
                     uMsg==WM_CREATE ? "CREATE" : "DESTROY",
                     windows_strerror(GetLastError()));
            }
        }
        retval = 0;
        break;

      case WM_INPUT: {
        if (!joystick_ignore_focus && wParam == RIM_INPUTSINK) {
            retval = 0;
            break;
        }
        if (!hid_module) {
            retval = 0;
            break;
        }
        RAWINPUT *data;
        UINT size;
        if (UNLIKELY(GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &size,
                                     sizeof(RAWINPUTHEADER)) == (UINT)(-1))) {
            DLOG("GetRawInputData(NULL) failed: %s",
                 windows_strerror(GetLastError()));
            retval = 0;
            break;
        }
        data = mem_alloc(size, 0, MEM_ALLOC_TEMP);
        if (UNLIKELY(!data)) {
            DLOG("Failed to allocate buffer for joystick input (%u bytes)",
                 size);
            retval = 0;
            break;
        }
        if (UNLIKELY(GetRawInputData((HRAWINPUT)lParam, RID_INPUT, data, &size,
                                     sizeof(RAWINPUTHEADER)) == (UINT)(-1))) {
            DLOG("GetRawInputData() failed: %s",
                 windows_strerror(GetLastError()));
            mem_free(data);
            retval = 0;
            break;
        }
        sys_mutex_lock(joystick_mutex, -1);
        for (int i = 0; i < num_joysticks; i++) {
            if (!joysticks[i].is_xinput
             && joysticks[i].device == data->header.hDevice) {
                handle_rawinput_joystick(i, data);
                break;
            }
        }
        sys_mutex_unlock(joystick_mutex);
        mem_free(data);
        retval = 0;
        break;
      }  // case WM_INPUT

      case WM_INPUT_DEVICE_CHANGE:
        if (!hid_module) {
            retval = 0;
            break;
        }
        if (wParam == GIDC_ARRIVAL) {
            add_rawinput_joystick((HANDLE)lParam);
        } else if (wParam == GIDC_REMOVAL) {
            sys_mutex_lock(joystick_mutex, -1);
            for (int i = 0; i < num_joysticks; i++) {
                if (!joysticks[i].is_xinput
                 && joysticks[i].device == (HANDLE)lParam) {
                    joysticks[i].device = NULL;
                    if (joysticks[i].raw_device) {
                        CloseHandle(joysticks[i].raw_device);
                        joysticks[i].raw_device = NULL;
                    }
                    joystick_info[i].connected = 0;
                    send_joystick_connect_event(i, INPUT_JOYSTICK_DISCONNECTED);
                    break;
                }
            }
            sys_mutex_unlock(joystick_mutex);
        }
        retval = 0;
        break;

      case WM_INPUTLANGCHANGE:
        update_vk_map();
        retval = 0;
        break;

      case WM_KEYDOWN:
      case WM_SYSKEYDOWN:
      case WM_KEYUP:
      case WM_SYSKEYUP: {
        const double now = time_now();
        const int down = !(HIWORD(lParam) & KF_UP);
        const int vk = wParam;
        int key = (vk < lenof(vk_map) ? vk_map[vk] : KEY__NONE);
        /* Distinguish numeric pad Enter from primary Enter with the
         * "extended key" bit, which is set for all numeric-pad keys.
         * (See, e.g.: http://www.tcl.tk/cgi-bin/tct/tip/158.html) */
        if (key == KEY_ENTER && (HIWORD(lParam) & KF_EXTENDED)) {
            key = KEY_NUMPAD_ENTER;
        }
        /* Also use the "extended key" bit to distinguish right and left
         * Shift/Ctrl/etc. */
        if (vk == VK_SHIFT && (HIWORD(lParam) & KF_EXTENDED)) {
            key = KEY_RIGHTSHIFT;
        }
        if (vk == VK_CONTROL && (HIWORD(lParam) & KF_EXTENDED)) {
            key = KEY_RIGHTCONTROL;
        }
        if (vk == VK_MENU && (HIWORD(lParam) & KF_EXTENDED)) {
            key = KEY_RIGHTALT;
        }
        InputEventDetail detail;
        if (key) {
            detail = down ? INPUT_KEYBOARD_KEY_DOWN : INPUT_KEYBOARD_KEY_UP;
        } else {
            detail = down ? INPUT_KEYBOARD_SYSTEM_KEY_DOWN
                          : INPUT_KEYBOARD_SYSTEM_KEY_UP;
        }
        (*event_callback)(&(InputEvent){
            .type = INPUT_EVENT_KEYBOARD, .detail = detail,
            .timestamp = now,
            {.keyboard = {.key = key, .system_key = vk,
                          .is_repeat = down && (HIWORD(lParam) & KF_REPEAT)}}});
        if (uMsg == WM_KEYDOWN && text_active) {
            InputEvent text_event = {
                .type = INPUT_EVENT_TEXT, .detail = 0, .timestamp = now};
            if (key == KEY_BACKSPACE) {
                text_event.detail = INPUT_TEXT_BACKSPACE;
            } else if (key == KEY_DELETE) {
                text_event.detail = INPUT_TEXT_DELETE;
            } else if (key == KEY_LEFT) {
                text_event.detail = INPUT_TEXT_CURSOR_LEFT;
            } else if (key == KEY_RIGHT) {
                text_event.detail = INPUT_TEXT_CURSOR_RIGHT;
            } else if (key == KEY_HOME) {
                text_event.detail = INPUT_TEXT_CURSOR_HOME;
            } else if (key == KEY_END) {
                text_event.detail = INPUT_TEXT_CURSOR_END;
            } else if (key == KEY_ESCAPE) {
                text_event.detail = INPUT_TEXT_CANCELLED;
            } else if (key == KEY_ENTER || key == KEY_NUMPAD_ENTER) {
                text_event.detail = INPUT_TEXT_DONE;
            }
            if (text_event.detail) {
                for (int i = 0; i < LOWORD(lParam); i++) {
                    (*event_callback)(&text_event);
                }
            }
        }
        retval = 0;
        break;
      }  // case WM_KEYDOWN, WM_SYSKEYDOWN, WM_KEYUP, WM_SYSKEYUP

      case WM_CHAR:
      case WM_UNICHAR: {
        if (text_active) {
            /* Ignore events handled by WM_KEYDOWN. */
            if (wParam >= 32) {
                int send = 1;
                /* Deal with UTF-16 surrogate pairs. */
                if (pending_utf16_surrogate) {
                    const unsigned int high = pending_utf16_surrogate;
                    pending_utf16_surrogate = 0;
                    if (wParam >= 0xDC00 && wParam <= 0xDFFF) {
                        wParam = 0x10000 + ((high&0x3FF)<<10) + (wParam&0x3FF);
                    } else {
                        DLOG("Discarding lone surrogate U+%04X", high);
                    }
                } else if (wParam >= 0xD800 && wParam <= 0xDBFF) {
                    pending_utf16_surrogate = wParam;
                    send = 0;
                } else if (wParam >= 0xDC00 && wParam <= 0xDFFF) {
                    DLOG("Discarding lone surrogate U+%04X", wParam);
                    send = 0;
                }
                if (send) {
                    (*event_callback)(&(InputEvent){
                        .type = INPUT_EVENT_TEXT, .detail = INPUT_TEXT_INPUT,
                        .timestamp = time_now(), {.text = {.ch = wParam}}});
                }
            }
        }
        /* Consume the message to prevent DefProcWindow() from translating
         * WM_UNICHAR messages into WM_CHAR. */
        retval = 1;
        break;
      }  // case WM_CHAR, WM_UNICHAR

      case WM_MOUSEMOVE:
        handle_mouse_event(INPUT_MOUSE_MOVE, lParam, 0);
        retval = 0;
        break;

      case WM_LBUTTONDOWN:
        handle_mouse_event(INPUT_MOUSE_LMB_DOWN, lParam, 0);
        retval = 0;
        break;

      case WM_LBUTTONUP:
        handle_mouse_event(INPUT_MOUSE_LMB_UP, lParam, 0);
        retval = 0;
        break;

      case WM_MBUTTONDOWN:
        handle_mouse_event(INPUT_MOUSE_MMB_DOWN, lParam, 0);
        retval = 0;
        break;

      case WM_MBUTTONUP:
        handle_mouse_event(INPUT_MOUSE_MMB_UP, lParam, 0);
        retval = 0;
        break;

      case WM_RBUTTONDOWN:
        handle_mouse_event(INPUT_MOUSE_RMB_DOWN, lParam, 0);
        retval = 0;
        break;

      case WM_RBUTTONUP:
        handle_mouse_event(INPUT_MOUSE_RMB_UP, lParam, 0);
        retval = 0;
        break;

      case WM_MOUSEWHEEL:
      case WM_MOUSEHWHEEL: {
        int scroll = (int16_t)HIWORD(wParam);
        if (scroll != 0) {
            if (uMsg == WM_MOUSEWHEEL) {
                scroll = -scroll;
            }
            const InputEventDetail detail =
                (uMsg == WM_MOUSEHWHEEL ? INPUT_MOUSE_SCROLL_H
                                        : INPUT_MOUSE_SCROLL_V);
            handle_mouse_event(detail, lParam, scroll / (float)WHEEL_DELTA);
        }
        retval = 0;
        break;
      }  // case WM_MOUSEWHEEL, WM_MOUSEHWHEEL

      case WM_POINTERDOWN:
        retval = handle_touch_event(INPUT_TOUCH_DOWN, wParam, lParam);
        break;

      case WM_POINTERUPDATE:
        retval = handle_touch_event(INPUT_TOUCH_MOVE, wParam, lParam);
        break;

      case WM_POINTERUP:
        retval = handle_touch_event(INPUT_TOUCH_UP, wParam, lParam);
        break;

      case WM_POINTERCAPTURECHANGED:
        retval = handle_touch_event(INPUT_TOUCH_CANCEL, wParam, lParam);
        break;

      default:
        retval = 0;
        break;

    }  // switch (uMsg)

    LeaveCriticalSection(&input_message_lock);
    return retval;
}

/*************************************************************************/
/************** Local routines: Raw input joystick handling **************/
/*************************************************************************/

static void scan_rawinput_joysticks(void)
{
    sys_mutex_lock(joystick_mutex, -1);
    for (int i = 0; i < num_joysticks; i++) {
        if (joystick_info[i].connected && !joysticks[i].is_xinput) {
            RID_DEVICE_INFO device_info = {.cbSize = sizeof(device_info)};
            if (GetRawInputDeviceInfo(
                    joysticks[i].device, RIDI_DEVICEINFO, &device_info,
                    (UINT[]){sizeof(device_info)}) == (UINT)(-1)) {
                DLOG("%s (%04X/%04X): GetRawInputDeviceInfo() failed (%s),"
                     " assuming disconnected",
                     hidjoy_name(joysticks[i].hid_handle),
                     hidjoy_vendor_id(joysticks[i].hid_handle),
                     hidjoy_product_id(joysticks[i].hid_handle),
                     windows_strerror(GetLastError()));
                hidjoy_flush_events(joysticks[i].hid_handle);
                joystick_info[i].connected = 0;
                send_joystick_connect_event(i, INPUT_JOYSTICK_DISCONNECTED);
            }
        }
    }
    sys_mutex_unlock(joystick_mutex);

    /* In theory we could get the device list with a simple count-alloc-get
     * pattern, but there's always the chance that the system will add a
     * new device just as we're doing the second GRIDL call, so we loop
     * until GRIDL succeeds. */
    UINT num_devices = 1;
    RAWINPUTDEVICELIST *devices =
        mem_alloc(sizeof(*devices), 0, MEM_ALLOC_TEMP);
    if (UNLIKELY(!devices)) {
        DLOG("No memory for 1 device structure");
        return;
    }
    UINT last_num_devices = num_devices;
    while (GetRawInputDeviceList(devices, &num_devices,
                                 sizeof(*devices)) == (UINT)(-1)) {
        if (UNLIKELY(GetLastError() != ERROR_INSUFFICIENT_BUFFER)) {
            DLOG("GetRawInputDeviceList() failed: %s",
                 windows_strerror(GetLastError()));
            mem_free(devices);
            return;
        }
        if (UNLIKELY(num_devices <= last_num_devices)) {
            /* At least as of January 2018, the gameoverlayrenderer.dll
             * library injected by Valve's Steam client when starting a
             * game wraps GRIDL with a buggy function that does not update
             * num_devices, so we end up infinite-looping here. */
            DLOG("GRIDL returned INSUFFICIENT_BUFFER but did not update"
                 " num_devices (Steam bug?), assuming no rawinput devices");
            mem_free(devices);
            return;
        }
        RAWINPUTDEVICELIST *new_devices = mem_realloc(
            devices, sizeof(*devices) * num_devices, MEM_ALLOC_TEMP);
        if (UNLIKELY(!new_devices)) {
            DLOG("No memory for %u device structures", num_devices);
            mem_free(devices);
            return;
        }
        devices = new_devices;
        last_num_devices = num_devices;
    }

    for (UINT i = 0; i < num_devices; i++) {
        add_rawinput_joystick(devices[i].hDevice);
    }

    mem_free(devices);
}

/*-----------------------------------------------------------------------*/

static void add_rawinput_joystick(HANDLE device)
{
    PRECOND(hid_module, return);

    RID_DEVICE_INFO device_info = {.cbSize = sizeof(device_info)};
    if (UNLIKELY(GetRawInputDeviceInfo(
                     device, RIDI_DEVICEINFO, &device_info,
                     (UINT[]){sizeof(device_info)}) == (UINT)(-1))) {
        DLOG("GetRawInputDeviceInfo(%p, DEVICEINFO) failed: %s",
             device, windows_strerror(GetLastError()));
        goto error_return;
    }
    if (device_info.dwType != RIM_TYPEHID
     || device_info.hid.usUsagePage != HID_PAGE_GENERIC_DESKTOP
     || (device_info.hid.usUsage != HID_USAGE_JOYSTICK
      && device_info.hid.usUsage != HID_USAGE_GAMEPAD))
    {
        goto error_return;
    }

    /* See if this is a device that is currently connected or was
     * previously disconnected. */
    sys_mutex_lock(joystick_mutex, -1);
    for (int i = 0; i < num_joysticks; i++) {
        if (!joysticks[i].is_xinput
         && (hidjoy_vendor_id(joysticks[i].hid_handle)
             == (int)device_info.hid.dwVendorId)
         && (hidjoy_product_id(joysticks[i].hid_handle)
             == (int)device_info.hid.dwProductId))
        {
            if (joystick_info[i].connected && joysticks[i].device == device) {
                /* We're already watching this device. */
                sys_mutex_unlock(joystick_mutex);
                return;
            } else if (!joystick_info[i].connected) {
                joysticks[i].device = device;
                joysticks[i].rumble_timeout = 0;
                joystick_info[i].connected = 1;
                send_joystick_connect_event(i, INPUT_JOYSTICK_CONNECTED);
                DLOG("Joystick %d (%s: %04X/%04X) reconnected", i,
                     hidjoy_name(joysticks[i].hid_handle),
                     hidjoy_vendor_id(joysticks[i].hid_handle),
                     hidjoy_product_id(joysticks[i].hid_handle));
                sys_mutex_unlock(joystick_mutex);
                return;
            }
        }
    }
    sys_mutex_unlock(joystick_mutex);

    /* Look up the device pathname.  If this is an XInput device
     * (identified by "IG_" in the pathname) and XInput is enabled, ignore
     * the device since we'll handle it through XInput instead. */
    char *path = gridi(device, RIDI_DEVICENAME);
    if (UNLIKELY(!path)) {
        DLOG("GetRawInputDeviceInfo(%04X/%04X, DEVICENAME) failed: %s",
             device_info.hid.dwVendorId, device_info.hid.dwProductId,
             windows_strerror(GetLastError()));
        mem_free(path);
        goto error_return;
    }
    /* Work around a bug(?) in at least Windows XP. */
    if (strncmp(path, "\\??\\", 4) == 0) {
        path[1] = '\\';
    }
    if (xinput_module && strstr(path, "IG_")) {
        mem_free(path);
        return;  // XInput will handle it.
    }

    /* Open the corresponding HID device and get the human-readable device
     * name and serial number. */
    char *name = NULL;
    char *serial = NULL;
    HANDLE raw_device = CreateFile(path, GENERIC_READ | GENERIC_WRITE,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                   OPEN_EXISTING, 0, NULL);
    if (UNLIKELY(raw_device == INVALID_HANDLE_VALUE)) {
        DLOG("CreateFile(%s): %s", path, windows_strerror(GetLastError()));
        raw_device = NULL;
    } else {
        /* The documentation states that the maximum string length for USB
         * devices is 126 wide characters plus the trailing null.  It
         * doesn't say anything about other protocols, but 126 characters
         * Ought To Be Enough For Anybody, so we run with it.  The
         * interface doesn't provide a way to get the actual string length,
         * so we just assume the maximum is safe. */
        WCHAR name16[127];
        if (UNLIKELY(!(*p_HidD_GetProductString)(
                         /* Yes, sizeof() (not lenof()) is correct here. */
                         raw_device, name16, sizeof(name16)))) {
            DLOG("Failed to get product name for USB device %s", path);
        } else {
            name16[lenof(name16)-1] = 0;  // Just to be safe.
            name = strdup_16to8(name16);
            if (UNLIKELY(!name)) {
                DLOG("Out of memory converting product name for USB device %s",
                     path);
            }
        }
        if (UNLIKELY(!(*p_HidD_GetSerialNumberString)(
                         raw_device, name16, sizeof(name16)))) {
            DLOG("Failed to get serial number for USB device %s", path);
        } else {
            name16[lenof(name16)-1] = 0;
            serial = strdup_16to8(name16);
            if (UNLIKELY(!serial)) {
                DLOG("Out of memory converting serial number for USB device"
                     " %s", path);
            }
        }
    }
    mem_free(path);

    /* Retrieve device information. */
    PHIDP_PREPARSED_DATA preparsed_data = gridi(device, RIDI_PREPARSEDDATA);
    if (UNLIKELY(!preparsed_data)) {
        DLOG("GetRawInputDeviceInfo(%04X/%04X, PREPARSEDDATA) failed: %s",
             device_info.hid.dwVendorId, device_info.hid.dwProductId,
             windows_strerror(GetLastError()));
        goto error_free_serial;
    }
    HIDP_CAPS caps;
    if (UNLIKELY((*p_HidP_GetCaps)(preparsed_data, &caps)
                 != HIDP_STATUS_SUCCESS)) {
        DLOG("%s (%04X/%04X): HidP_GetCaps() failed",
             name, device_info.hid.dwVendorId, device_info.hid.dwProductId);
        goto error_free_preparsed_data;
    }

    HIDP_BUTTON_CAPS *button_caps =
        mem_alloc(sizeof(*button_caps) * caps.NumberInputButtonCaps, 0, 0);
    if (caps.NumberInputButtonCaps > 0) {
        if (UNLIKELY(!button_caps)) {
            DLOG("%s (%04X/%04X): No memory for %d button input descriptors",
                 name, device_info.hid.dwVendorId, device_info.hid.dwProductId,
                 caps.NumberInputButtonCaps);
            goto error_free_preparsed_data;
        }
        if (UNLIKELY((*p_HidP_GetSpecificButtonCaps)(
                         HidP_Input, 0, 0, 0, button_caps,
                         (ULONG[]){caps.NumberInputButtonCaps}, preparsed_data)
                     != HIDP_STATUS_SUCCESS)) {
            DLOG("%s (%04X/%04X): Failed to read button descriptors",
                 name, device_info.hid.dwVendorId, device_info.hid.dwProductId);
            goto error_free_button_caps;
        }
    }

    HIDP_VALUE_CAPS *value_caps =
        mem_alloc(sizeof(*value_caps) * caps.NumberInputValueCaps, 0, 0);
    if (caps.NumberInputValueCaps > 0) {
        if (UNLIKELY(!value_caps)) {
            DLOG("%s (%04X/%04X): No memory for %d value input descriptors",
                 name, device_info.hid.dwVendorId, device_info.hid.dwProductId,
                 caps.NumberInputValueCaps);
            goto error_free_button_caps;
        }
        if (UNLIKELY((*p_HidP_GetSpecificValueCaps)(
                         HidP_Input, 0, 0, 0, value_caps,
                         (ULONG[]){caps.NumberInputValueCaps}, preparsed_data)
                     != HIDP_STATUS_SUCCESS)) {
            DLOG("%s (%04X/%04X): Failed to read value descriptors",
                 name, device_info.hid.dwVendorId, device_info.hid.dwProductId);
            goto error_free_value_caps;
        }
    }

    /* Create a generic HID joystick handle for the device. */
    HIDJoystickInfo hid_info;
    mem_clear(&hid_info, sizeof(hid_info));
    hid_info.vendor_id = device_info.hid.dwVendorId;
    hid_info.product_id = device_info.hid.dwProductId;
    hid_info.dev_version = device_info.hid.dwVersionNumber;
    hid_info.name = name;
    hid_info.serial = serial;

    hid_info.num_buttons = 0;
    for (int i = 0; i < caps.NumberInputButtonCaps; i++) {
        if (button_caps[i].IsRange) {
            const int low = button_caps[i].Range.UsageMin;
            const int high = button_caps[i].Range.UsageMax;
            hid_info.num_buttons += (high+1) - low;
        } else {
            hid_info.num_buttons++;
        }
    }
    if (hid_info.num_buttons > 0) {
        hid_info.buttons =
            mem_alloc(sizeof(*hid_info.buttons) * hid_info.num_buttons, 0,
                      MEM_ALLOC_TEMP);
        if (UNLIKELY(!hid_info.buttons)) {
            DLOG("No memory for %d button inputs", hid_info.num_buttons);
            goto error_free_value_caps;
        }
        hid_info.num_buttons = 0;
        for (int i = 0; i < caps.NumberInputButtonCaps; i++) {
            uint32_t low, high;
            if (button_caps[i].IsRange) {
                low = button_caps[i].Range.UsageMin;
                high = button_caps[i].Range.UsageMax;
            } else {
                low = high = button_caps[i].NotRange.Usage;
            }
            low |= button_caps[i].UsagePage << 16;
            high |= button_caps[i].UsagePage << 16;
            for (uint32_t usage = low; usage <= high; usage++) {
                hid_info.buttons[hid_info.num_buttons++] = usage;
            }
        }
    }

    hid_info.num_values = 0;
    for (int i = 0; i < caps.NumberInputValueCaps; i++) {
        if (value_caps[i].IsRange) {
            const int low = value_caps[i].Range.UsageMin;
            const int high = value_caps[i].Range.UsageMax;
            hid_info.num_values += (high+1) - low;
        } else {
            hid_info.num_values++;
        }
    }
    if (hid_info.num_values > 0) {
        hid_info.values =
            mem_alloc(sizeof(*hid_info.values) * hid_info.num_values, 0,
                      MEM_ALLOC_TEMP);
        if (UNLIKELY(!hid_info.values)) {
            DLOG("No memory for %d value inputs", hid_info.num_values);
            mem_free(hid_info.buttons);
            goto error_free_value_caps;
        }
        hid_info.num_values = 0;
        for (int i = 0; i < caps.NumberInputValueCaps; i++) {
            uint32_t low, high;
            if (value_caps[i].IsRange) {
                low = value_caps[i].Range.UsageMin;
                high = value_caps[i].Range.UsageMax;
            } else {
                low = high = value_caps[i].NotRange.Usage;
            }
            low |= value_caps[i].UsagePage << 16;
            high |= value_caps[i].UsagePage << 16;
            LONG logical_min = value_caps[i].LogicalMin;
            LONG logical_max = value_caps[i].LogicalMax;
            const int bit_width = value_caps[i].BitSize;
            if (logical_min >= 0 && logical_max < 0) {
                logical_max += (LONG)1 << bit_width;
            }
            for (uint32_t usage = low; usage <= high; usage++) {
                hid_info.values[hid_info.num_values].usage = usage;
                hid_info.values[hid_info.num_values].logical_min = logical_min;
                hid_info.values[hid_info.num_values].logical_max = logical_max;
                hid_info.num_values++;
            }
        }
    }

    HIDJoystickHandle *hid_handle = hidjoy_create(&hid_info);
    mem_free(hid_info.values);
    mem_free(hid_info.buttons);
    if (UNLIKELY(!hid_handle)) {
        DLOG("Failed to create generic HID handle");
        goto error_free_value_caps;
    }

    /* Prepare the input state array for the joystick. */
    struct JoystickInputState *input_state = mem_alloc(
        sizeof(*input_state) * (hid_info.num_buttons + hid_info.num_values),
        0, MEM_ALLOC_TEMP);
    if (UNLIKELY(!input_state)) {
        DLOG("No memory for joystick input state (%d inputs)",
             hid_info.num_buttons + hid_info.num_values);
        goto error_destroy_hid_handle;
    }
    int num_input_state = 0;
    for (int i = 0; i < caps.NumberInputButtonCaps; i++) {
        int low, high;
        if (button_caps[i].IsRange) {
            low = button_caps[i].Range.UsageMin;
            high = button_caps[i].Range.UsageMax;
        } else {
            low = high = button_caps[i].NotRange.Usage;
        }
        const int usage_page = button_caps[i].UsagePage;
        for (int usage = low; usage <= high; usage++) {
            if (hidjoy_is_input_used(hid_handle, usage_page, usage)) {
                input_state[num_input_state].is_button = 1;
                input_state[num_input_state].usage_page = usage_page;
                input_state[num_input_state].usage = usage;
                input_state[num_input_state].value = 0;
                num_input_state++;
            }
        }
    }
    for (int i = 0; i < caps.NumberInputValueCaps; i++) {
        int low, high;
        if (value_caps[i].IsRange) {
            low = value_caps[i].Range.UsageMin;
            high = value_caps[i].Range.UsageMax;
        } else {
            low = high = value_caps[i].NotRange.Usage;
        }
        const int usage_page = value_caps[i].UsagePage;
        const LONG logical_min = value_caps[i].LogicalMin;
        const int bit_width = value_caps[i].BitSize;
        const int is_signed = (logical_min < 0);
        for (int usage = low; usage <= high; usage++) {
            if (hidjoy_is_input_used(hid_handle, usage_page, usage)) {
                input_state[num_input_state].is_button = 0;
                input_state[num_input_state].bit_width = bit_width;
                input_state[num_input_state].is_signed = is_signed;
                input_state[num_input_state].usage_page = usage_page;
                input_state[num_input_state].usage = usage;
                input_state[num_input_state].value = 0;
                num_input_state++;
            }
        }
    }

    /* Add the device to the joystick list. */
    sys_mutex_lock(joystick_mutex, -1);
    int index;
    for (index = 0; index < num_joysticks; index++) {
        if (!joystick_info[index].connected) {
            if (!joysticks[index].is_xinput) {
                mem_free(joysticks[index].preparsed_data);
                mem_free(joysticks[index].input_state);
                hidjoy_destroy(joysticks[index].hid_handle);
            }
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
            goto error_free_input_state;
        }
        joysticks = new_joysticks;
        SysInputJoystick *new_joystick_info =
            mem_realloc(joystick_info,
                        sizeof(*joystick_info) * (num_joysticks + 1), 0);
        if (UNLIKELY(!new_joystick_info)) {
            DLOG("No memory to expand joystick_info array to %d entries",
                 num_joysticks + 1);
            sys_mutex_unlock(joystick_mutex);
            goto error_free_input_state;
        }
        joystick_info = new_joystick_info;
        num_joysticks++;
    }
    joysticks[index].num_buttons = hidjoy_num_buttons(hid_handle);
    joysticks[index].num_sticks = hidjoy_num_sticks(hid_handle);
    joysticks[index].rumble_timeout = 0;
    joysticks[index].is_xinput = 0;
    joysticks[index].device = device;
    joysticks[index].raw_device = raw_device;
    joysticks[index].preparsed_data = preparsed_data;
    joysticks[index].input_state = input_state;
    joysticks[index].num_input_state = num_input_state;
    joysticks[index].hid_handle = hid_handle;
    joystick_info[index].connected = 1;
    joystick_info[index].can_rumble = 0;
    joystick_info[index].num_buttons = joysticks[index].num_buttons;
    joystick_info[index].num_sticks = joysticks[index].num_sticks;
    send_joystick_connect_event(index, INPUT_JOYSTICK_CONNECTED);
    sys_mutex_unlock(joystick_mutex);
    hidjoy_set_event_callback(hid_handle, event_callback, index);

    /* Other device-specific setup. */
    if (hidjoy_vendor_id(hid_handle) == 0x054C
        && (hidjoy_product_id(hid_handle) == 0x05C4
            || hidjoy_product_id(hid_handle) == 0x09CC)) {
        /* Sony DualShock 4: Set the rumble capability flag, and turn off
         * the LED now since it will be implicitly turned off by rumble
         * calls. */
        joystick_info[index].can_rumble = 1;
        uint8_t buf[32];
        mem_clear(buf, sizeof(buf));
        buf[0] = 0x05;
        buf[1] = 0xFF;
        if (UNLIKELY(!WriteFile(raw_device, buf, sizeof(buf),
                                (DWORD[1]){0}, NULL))) {
            DLOG("Failed to turn off DualShock 4 LED: %s",
                 windows_strerror(GetLastError()));
        }
    }

    /* Report the new joystick and return. */
    DLOG("New joystick %d: %s (%04X/%04X, serial %s), %d buttons, %d sticks",
         index, hidjoy_name(hid_handle), hidjoy_vendor_id(hid_handle),
         hidjoy_product_id(hid_handle), hidjoy_serial(hid_handle),
         hidjoy_num_buttons(hid_handle), hidjoy_num_sticks(hid_handle));
    mem_free(name);
    mem_free(serial);
    mem_free(button_caps);
    mem_free(value_caps);
    return;

  error_free_input_state:
    mem_free(input_state);
  error_destroy_hid_handle:
    hidjoy_destroy(hid_handle);
  error_free_value_caps:
    mem_free(value_caps);
  error_free_button_caps:
    mem_free(button_caps);
  error_free_preparsed_data:
    mem_free(preparsed_data);
  error_free_serial:
    mem_free(serial);
    mem_free(name);
    CloseHandle(raw_device);
  error_return:
    return;
}

/*-----------------------------------------------------------------------*/

static void handle_rawinput_joystick(int num, RAWINPUT *data)
{
    PRECOND(hid_module, return);

    JoystickInfo *joystick = &joysticks[num];
    PRECOND(!joystick->is_xinput, return);

    const double timestamp = time_now();
    NTSTATUS result;

    /* Handle button input.  The HID interface only gives us "all buttons
     * which are currently pressed", so we have to iterate over the full
     * list of buttons to see what has actually changed. */
    USAGE_AND_PAGE pressed_buttons[INPUT_MAX_JOYSTICK_BUTTONS];
    ULONG num_buttons = lenof(pressed_buttons);
    result = (*p_HidP_GetUsagesEx)(
        HidP_Input, 0, pressed_buttons, &num_buttons,
        joystick->preparsed_data, (void *)&data->data.hid.bRawData,
        data->data.hid.dwSizeHid);
    if (result == HIDP_STATUS_SUCCESS) {
        for (int i = 0; i < joystick->num_input_state; i++) {
            if (!joystick->input_state[i].is_button) {
                continue;
            }
            int value = 0;
            for (ULONG j = 0; j < num_buttons; j++) {
                if (pressed_buttons[j].Usage == joystick->input_state[i].usage
                 && pressed_buttons[j].UsagePage == joystick->input_state[i].usage_page) {
                    value = 1;
                    break;
                }
            }
            if (value != joystick->input_state[i].value) {
                joystick->input_state[i].value = value;
                hidjoy_handle_input(joystick->hid_handle, timestamp,
                                    joystick->input_state[i].usage_page,
                                    joystick->input_state[i].usage,
                                    joystick->input_state[i].value);
            }
        }
    } else if (result == HIDP_STATUS_BUFFER_TOO_SMALL) {
        /* Assume all buttons we care about are pressed. */
        for (int i = 0; i < joystick->num_input_state; i++) {
            if (joystick->input_state[i].is_button
             && !joystick->input_state[i].value) {
                joystick->input_state[i].value = 1;
                hidjoy_handle_input(joystick->hid_handle, timestamp,
                                    joystick->input_state[i].usage_page,
                                    joystick->input_state[i].usage,
                                    joystick->input_state[i].value);
            }
        }
    } else {
        DLOG("%s (%04X/%04X): Failed to read button data: %s",
             hidjoy_name(joystick->hid_handle),
             hidjoy_vendor_id(joystick->hid_handle),
             hidjoy_product_id(joystick->hid_handle),
             windows_strerror(result));
    }

    for (int i = 0; i < joystick->num_input_state; i++) {
        if (joystick->input_state[i].is_button) {
            continue;
        }
        ULONG u_value;
        result = (*p_HidP_GetUsageValue)(
            HidP_Input, joystick->input_state[i].usage_page, 0,
            joystick->input_state[i].usage, &u_value,
            joystick->preparsed_data, (void *)&data->data.hid.bRawData,
            data->data.hid.dwSizeHid);
        if (UNLIKELY(result != HIDP_STATUS_SUCCESS)) {
            DLOG("%s (%04X/%04X): Failed to read value %d/0x%X: %s",
                 hidjoy_name(joystick->hid_handle),
                 hidjoy_vendor_id(joystick->hid_handle),
                 hidjoy_product_id(joystick->hid_handle),
                 joystick->input_state[i].usage_page,
                 joystick->input_state[i].usage, windows_strerror(result));
            continue;
        }
        const ULONG mask = ((ULONG)1 << joystick->input_state[i].bit_width) - 1;
        int value = (int)(u_value & mask);
        if (joystick->input_state[i].is_signed) {
            if (value & (1 << (joystick->input_state[i].bit_width - 1))) {
                value -= 1 << joystick->input_state[i].bit_width;
            }
        }
        if (value != joystick->input_state[i].value) {
            joystick->input_state[i].value = value;
            hidjoy_handle_input(joystick->hid_handle, timestamp,
                                joystick->input_state[i].usage_page,
                                joystick->input_state[i].usage,
                                joystick->input_state[i].value);
        }
    }

    hidjoy_flush_events(joystick->hid_handle);
}

/*-----------------------------------------------------------------------*/

static void *gridi(HANDLE device, UINT command)
{
    /* This must be initialized to zero to prevent buffer overruns inside
     * USER32.dll on at least Windows XP. */
    UINT size = 0;
    if (UNLIKELY(GetRawInputDeviceInfo(device, command,
                                       NULL, &size) == (UINT)(-1))) {
        return NULL;
    }
    void *data = mem_alloc(size, 0, 0);
    if (UNLIKELY(!data)) {
        DLOG("No memory for input device data (%u bytes)", size);
        SetLastError(ERROR_OUTOFMEMORY);
        return NULL;
    }
    if (UNLIKELY(GetRawInputDeviceInfo(device, command,
                                       data, &size) == (UINT)(-1))) {
        mem_free(data);
        return NULL;
    }
    return data;
}

/*************************************************************************/
/*************** Local routines: XInput joystick handling ****************/
/*************************************************************************/

static void scan_xinput_joysticks(void)
{
    PRECOND(xinput_module, return);

    XINPUT_CAPABILITIES caps;
    for (int device = 0; device < 4; device++) {
        if ((*p_XInputGetCapabilities)(device, XINPUT_FLAG_GAMEPAD,
                                       &caps) != ERROR_SUCCESS) {
            continue;
        }
        JoystickInfo info;
        mem_clear(&info, sizeof(info));
        for (int i = 0; i < lenof(info.button_map); i++) {
            info.button_map[i] = -1;
        }
        info.is_xinput = 1;
        info.xinput_device = device;
        info.xinput_caps = caps;
        const WORD dpad_buttons = (XINPUT_GAMEPAD_DPAD_UP
                                 | XINPUT_GAMEPAD_DPAD_DOWN
                                 | XINPUT_GAMEPAD_DPAD_LEFT
                                 | XINPUT_GAMEPAD_DPAD_RIGHT);
        info.has_dpad = (caps.Gamepad.wButtons & dpad_buttons) == dpad_buttons;

        for (int i = 0; i < lenof(xinput_button_map); i++) {
            if (caps.Gamepad.wButtons & xinput_button_map[i].flag) {
                const int index = info.num_buttons++;
                info.button_map[xinput_button_map[i].name] = index;
            }
        }
        if (caps.Gamepad.bLeftTrigger) {
            const int index = info.num_buttons++;
            info.button_map[INPUT_JOYBUTTON_L2] = index;
        }
        if (caps.Gamepad.bRightTrigger) {
            const int index = info.num_buttons++;
            info.button_map[INPUT_JOYBUTTON_R2] = index;
        }
        if (caps.Gamepad.sThumbLX && caps.Gamepad.sThumbLY) {
            info.num_sticks = 1;
        }
        if (caps.Gamepad.sThumbRX && caps.Gamepad.sThumbRY) {
            info.num_sticks = 2;
        }

        sys_mutex_lock(joystick_mutex, -1);
        int i;
        for (i = 0; i < num_joysticks; i++) {
            if (joysticks[i].is_xinput
             && joysticks[i].xinput_device == device) {
                break;
            }
        }
        if (i == num_joysticks) {
            for (i = 0; i < num_joysticks; i++) {
                if (!joystick_info[i].connected) {
                    if (!joysticks[i].is_xinput) {
                        mem_free(joysticks[i].preparsed_data);
                        mem_free(joysticks[i].input_state);
                        hidjoy_destroy(joysticks[i].hid_handle);
                    }
                    break;
                }
            }
        }
        const int is_new = (i == num_joysticks);
        if (is_new) {
            JoystickInfo *new_joysticks = mem_realloc(
                joysticks, sizeof(*joysticks) * (num_joysticks + 1), 0);
            if (UNLIKELY(!new_joysticks)) {
                DLOG("No memory to expand joysticks array to %d entries",
                     num_joysticks + 1);
                sys_mutex_unlock(joystick_mutex);
                return;
            }
            joysticks = new_joysticks;
            SysInputJoystick *new_joystick_info = mem_realloc(
                joystick_info, sizeof(*joystick_info) * (num_joysticks + 1), 0);
            if (UNLIKELY(!new_joystick_info)) {
                DLOG("No memory to expand joystick_info array to %d entries",
                     num_joysticks + 1);
                sys_mutex_unlock(joystick_mutex);
                return;
            }
            joystick_info = new_joystick_info;
            num_joysticks++;
            joystick_info[i].connected = 0;
        }
        if (!joystick_info[i].connected) {
            joysticks[i] = info;
            joystick_info[i].connected = 1;
            joystick_info[i].can_rumble = (caps.Vibration.wLeftMotorSpeed
                                        || caps.Vibration.wRightMotorSpeed);
            joystick_info[i].num_buttons = info.num_buttons;
            joystick_info[i].num_sticks = info.num_sticks;
            if (is_new) {
                DLOG("New joystick %d: XInput %d, %d buttons, %d sticks",
                     i, device, info.num_buttons, info.num_sticks);
            } else {
                DLOG("Joystick %d (XInput %d) reconnected", i, device);
            }
            send_joystick_connect_event(i, INPUT_JOYSTICK_CONNECTED);
        }
        sys_mutex_unlock(joystick_mutex);
    }
}

/*-----------------------------------------------------------------------*/

static void poll_xinput_joystick(int num)
{
    PRECOND(xinput_module, return);

    JoystickInfo *joystick = &joysticks[num];
    PRECOND(joystick->is_xinput, return);
    InputEvent event = {.type = INPUT_EVENT_JOYSTICK, .timestamp = time_now(),
                        {.joystick = {.device = num}}};

    XINPUT_STATE state;
    const DWORD result = (*p_XInputGetState)(joystick->xinput_device, &state);
    if (UNLIKELY(result != ERROR_SUCCESS)) {
        DLOG("Failed to get state of XInput device %d (%s), assuming"
             " disconnected", joystick->xinput_device,
             windows_strerror(result));
        joystick_info[num].connected = 0;
        send_joystick_connect_event(num, INPUT_JOYSTICK_DISCONNECTED);
        return;
    }

    for (int i = 0; i < lenof(xinput_button_map); i++) {
        const int index = joystick->button_map[xinput_button_map[i].name];
        if (index >= 0) {
            const int value =
                ((state.Gamepad.wButtons & xinput_button_map[i].flag) != 0);
            handle_xinput_button(joystick, index, value, &event);
        }
    }
    const int l2_index = joystick->button_map[INPUT_JOYBUTTON_L2];
    if (l2_index >= 0) {
        /* Debounce the input by 1/16 on either side of the center point. */
        int value;
        if (joystick->button_state[l2_index]) {
            value = (state.Gamepad.bLeftTrigger >= 120);
        } else {
            value = (state.Gamepad.bLeftTrigger >= 136);
        }
        handle_xinput_button(joystick, l2_index, value, &event);
    }
    const int r2_index = joystick->button_map[INPUT_JOYBUTTON_R2];
    if (r2_index >= 0) {
        int value;
        if (joystick->button_state[r2_index]) {
            value = (state.Gamepad.bRightTrigger >= 120);
        } else {
            value = (state.Gamepad.bRightTrigger >= 136);
        }
        handle_xinput_button(joystick, r2_index, value, &event);
    }

    if (joystick->num_sticks >= 1) {
        handle_xinput_stick(joystick, 0, state.Gamepad.sThumbLX,
                            state.Gamepad.sThumbLY, &event);
    }
    if (joystick->num_sticks >= 2) {
        handle_xinput_stick(joystick, 1, state.Gamepad.sThumbRX,
                            state.Gamepad.sThumbRY, &event);
    }

    if (joystick->has_dpad) {
        const int new_up =
            ((state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0);
        const int new_down =
            ((state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0);
        const int new_left =
            ((state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0);
        const int new_right =
            ((state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0);
        const int old_dpad_x =
            joystick->dpad_state_right - joystick->dpad_state_left;
        const int old_dpad_y =
            joystick->dpad_state_down - joystick->dpad_state_up;
        const int new_dpad_x = new_right - new_left;
        const int new_dpad_y = new_down - new_up;
        if (new_dpad_x != old_dpad_x || new_dpad_y != old_dpad_y) {
            event.detail = INPUT_JOYSTICK_DPAD_CHANGE;
            event.joystick.x = new_dpad_x;
            event.joystick.y = new_dpad_y;
            (*event_callback)(&event);
        }
        joystick->dpad_state_up    = new_up;
        joystick->dpad_state_down  = new_down;
        joystick->dpad_state_left  = new_left;
        joystick->dpad_state_right = new_right;
    }
}

/*-----------------------------------------------------------------------*/

static void handle_xinput_button(JoystickInfo *joystick, int index,
                                 int value, InputEvent *event)
{
    if (value && !joystick->button_state[index]) {
        event->detail = INPUT_JOYSTICK_BUTTON_DOWN;
        event->joystick.index = index;
        (*event_callback)(event);
        joystick->button_state[index] = 1;
    } else if (!value && joystick->button_state[index]) {
        event->detail = INPUT_JOYSTICK_BUTTON_UP;
        event->joystick.index = index;
        (*event_callback)(event);
        joystick->button_state[index] = 0;
    }
}

/*-----------------------------------------------------------------------*/

static void handle_xinput_stick(JoystickInfo *joystick, int index,
                                int raw_x, int raw_y, InputEvent *event)
{
    Vector2f value = {.x = scale_xinput_axis(raw_x),
                      .y = -scale_xinput_axis(raw_y)};
    if (value.x != joystick->stick_state[index].x
     || value.y != joystick->stick_state[index].y) {
        event->detail = INPUT_JOYSTICK_STICK_CHANGE;
        event->joystick.index = index;
        event->joystick.x = value.x;
        event->joystick.y = value.y;
        (*event_callback)(event);
        joystick->stick_state[index] = value;
    }
}

/*-----------------------------------------------------------------------*/

static float scale_xinput_axis(int raw_value)
{
    if (raw_value < 0) {
        return raw_value / 32768.0f;
    } else {
        return raw_value / 32767.0f;
    }
}

/*************************************************************************/
/********************* Local routines: Miscellaneous *********************/
/*************************************************************************/

static int joystick_discovery_thread(UNUSED void *unused)
{
    /* Only scan for raw input devices on Windows XP, since newer versions
     * of Windows provide us with relevant events. */
    const int scan_rawinput =
        !windows_version_is_at_least(WINDOWS_VERSION_VISTA);

    while (!sys_semaphore_wait(joystick_discovery_stop_sem, 1.0)) {
        if (xinput_module) {
            scan_xinput_joysticks();
        }
        if (hid_module && scan_rawinput) {
            scan_rawinput_joysticks();
        }
    }

    return 0;
}

/*-----------------------------------------------------------------------*/

static void send_joystick_connect_event(int device, InputEventDetail detail)
{
    PRECOND(detail == INPUT_JOYSTICK_CONNECTED
         || detail == INPUT_JOYSTICK_DISCONNECTED);

    (*event_callback)(&(InputEvent){
        .type = INPUT_EVENT_JOYSTICK, .detail = detail,
        .timestamp = time_now(), {.joystick = {.device = device}}});
}

/*-----------------------------------------------------------------------*/

static void do_rumble(JoystickInfo *joystick, float left, float right,
                      float time)
{
    if (joystick->is_xinput) {
        ASSERT(xinput_module);
        XINPUT_VIBRATION vibration = {
            .wLeftMotorSpeed = iroundf(left * 65535),
            .wRightMotorSpeed = iroundf(right * 65535)};
        (*p_XInputSetState)(joystick->xinput_device, &vibration);
    } else {
        ASSERT(hid_module);
        /* This has to be handled specially for each supported device. */
        if (hidjoy_vendor_id(joystick->hid_handle) == 0x054C
         && hidjoy_product_id(joystick->hid_handle) == 0x05C4) {
            /* Sony DualShock 4 */
            uint8_t buf[32];
            mem_clear(buf, sizeof(buf));
            buf[0] = 0x05;
            buf[1] = 0xFF;
            buf[4] = iroundf(right * 255);
            buf[5] = iroundf(left * 255);
            if (UNLIKELY(!WriteFile(joystick->raw_device, buf, sizeof(buf),
                                    (DWORD[1]){0}, NULL))) {
                DLOG("Failed to send DualShock 4 rumble report: %s",
                     windows_strerror(GetLastError()));
            }
        }
    }
    if (left > 0 || right > 0) {
        joystick->rumble_timeout = time_now() + time;
    } else {
        joystick->rumble_timeout = 0;
    }
}

/*-----------------------------------------------------------------------*/

static void update_vk_map(void)
{
    for (int vk = VK_OEM_1; vk < lenof(vk_map); vk++) {
        const int ch = MapVirtualKey(vk, 2 /*MAPVK_VK_TO_CHAR*/);
        if (ch && !(ch & 0x8000)) {
            int code = KEY__NONE;
            switch (ch) {
                case ' ':  code = KEY_SPACE;        break;
                case '!':  code = KEY_EXCLAMATION;  break;
                case '"':  code = KEY_QUOTE;        break;
                case '#':  code = KEY_SHARP;        break;
                case '$':  code = KEY_DOLLAR;       break;
                case '%':  code = KEY_PERCENT;      break;
                case '&':  code = KEY_AMPERSAND;    break;
                case '\'': code = KEY_APOSTROPHE;   break;
                case '(':  code = KEY_LEFTPAREN;    break;
                case ')':  code = KEY_RIGHTPAREN;   break;
                case '*':  code = KEY_ASTERISK;     break;
                case '+':  code = KEY_PLUS;         break;
                case ',':  code = KEY_COMMA;        break;
                case '-':  code = KEY_HYPHEN;       break;
                case '.':  code = KEY_PERIOD;       break;
                case '/':  code = KEY_SLASH;        break;
                case ':':  code = KEY_COLON;        break;
                case ';':  code = KEY_SEMICOLON;    break;
                case '<':  code = KEY_LESS;         break;
                case '=':  code = KEY_EQUALS;       break;
                case '>':  code = KEY_GREATER;      break;
                case '?':  code = KEY_QUESTION;     break;
                case '@':  code = KEY_ATSIGN;       break;
                case '[':  code = KEY_LEFTBRACKET;  break;
                case '\\': code = KEY_BACKSLASH;    break;
                case ']':  code = KEY_RIGHTBRACKET; break;
                case '^':  code = KEY_CIRCUMFLEX;   break;
                case '_':  code = KEY_UNDERSCORE;   break;
                case '`':  code = KEY_BACKQUOTE;    break;
                case '{':  code = KEY_LEFTBRACE;    break;
                case '|':  code = KEY_PIPE;         break;
                case '}':  code = KEY_RIGHTBRACE;   break;
                case '~':  code = KEY_TILDE;        break;
            }
            vk_map[vk] = code;
        }
    }
}

/*-----------------------------------------------------------------------*/

static void handle_mouse_event(InputEventDetail detail, LPARAM lParam,
                               float scroll)
{
    const int width = graphics_display_width();
    const int height = graphics_display_height();
    POINT p = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    if (scroll != 0) {
        /* Bizarrely, scroll wheel messages have absolute rather than
         * relative coordinates. */
        ScreenToClient(windows_window(), &p);
    }
    const float x = (float)bound(p.x, 0, width-1) / (float)width;
    const float y = (float)bound(p.y, 0, height-1) / (float)height;
    (*event_callback)(&(InputEvent){
        .type = INPUT_EVENT_MOUSE, .detail = detail, .timestamp = time_now(),
        {.mouse = {.x = x, .y = y, .scroll = scroll}}});
}

/*-----------------------------------------------------------------------*/

static int handle_touch_event(InputEventDetail detail, WPARAM wParam,
                              LPARAM lParam)
{
    if (touch_to_mouse) {
        return 0;
    }

    const int pointer = GET_POINTERID_WPARAM(wParam);
    BOOL (WINAPI *p_GetPointerType)(UINT32, POINTER_INPUT_TYPE *) =
        (void *)GetProcAddress(GetModuleHandle("user32.dll"),
                               "GetPointerType");
    ASSERT(p_GetPointerType, return 0);
    POINTER_INPUT_TYPE type;
    if (UNLIKELY(!(*p_GetPointerType)(pointer, &type))) {
        DLOG("GetPointerType(%d) failed: %s", pointer,
             windows_strerror(GetLastError()));
        return 0;
    }
    if (type == PT_MOUSE || type == PT_TOUCHPAD) {
        return 0;  // These aren't the touches we're looking for.
    }

    const int index = lookup_touch(pointer, detail == INPUT_TOUCH_DOWN);
    if (index < 0) {
        /* Table is full (or was full when the touch-down event was
         * received), but consume the touch anyway. */
        return 1;
    }

    POINT p = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    ScreenToClient(windows_window(), &p);
    const int width = graphics_display_width();
    const int height = graphics_display_height();
    const float x = (float)bound(p.x, 0, width-1) / (float)width;
    const float y = (float)bound(p.y, 0, height-1) / (float)height;
    (*event_callback)(&(InputEvent){
        .type = INPUT_EVENT_TOUCH, .detail = detail, .timestamp = time_now(),
        {.touch = {.id = touch_map[index].id, .x = x, .y = y}}});

    if (detail == INPUT_TOUCH_UP || detail == INPUT_TOUCH_CANCEL) {
        touch_map[index].id = 0;
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

static int lookup_touch(int pointer, int new)
{
    int unused = -1;
    for (int i = 0; i < lenof(touch_map); i++) {
        if (touch_map[i].id != 0 && touch_map[i].pointer == pointer) {
            if (new) {
                DLOG("Strange: already had active record for new touch %d",
                     pointer);
            }
            return i;
        } else if (new && unused < 0 && touch_map[i].id == 0) {
            unused = i;
        }
    }
    if (new && unused >= 0) {
        touch_map[unused].pointer = pointer;
        touch_map[unused].id = next_touch_id;
        next_touch_id++;
        if (next_touch_id == 0) {
            next_touch_id++;
        }
    }
    return unused;
}

/*************************************************************************/
/*************************************************************************/
