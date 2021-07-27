/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/linux/input.c: Input device interface for Linux.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/graphics.h"
#include "src/input.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/linux/internal.h"
#include "src/sysdep/misc/joystick-db.h"
#include "src/sysdep/posix/time.h"
#include "src/thread.h"
#include "src/time.h"
#include "src/utility/utf8.h"

#include <dirent.h>
#include <fcntl.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <linux/input.h>
/* Undefine KEY_* symbols that (could potentially) conflict with ours. */
#undef KEY_A
#undef KEY_B
#undef KEY_C
#undef KEY_D
#undef KEY_E
#undef KEY_F
#undef KEY_G
#undef KEY_H
#undef KEY_I
#undef KEY_J
#undef KEY_K
#undef KEY_L
#undef KEY_M
#undef KEY_N
#undef KEY_O
#undef KEY_P
#undef KEY_Q
#undef KEY_R
#undef KEY_S
#undef KEY_T
#undef KEY_U
#undef KEY_V
#undef KEY_W
#undef KEY_X
#undef KEY_Y
#undef KEY_Z
#undef KEY_0
#undef KEY_1
#undef KEY_2
#undef KEY_3
#undef KEY_4
#undef KEY_5
#undef KEY_6
#undef KEY_7
#undef KEY_8
#undef KEY_9
#undef KEY_TAB
#undef KEY_ENTER
#undef KEY_SPACE
#undef KEY_EXCLAMATION
#undef KEY_QUOTE
#undef KEY_SHARP
#undef KEY_DOLLAR
#undef KEY_PERCENT
#undef KEY_AMPERSAND
#undef KEY_APOSTROPHE
#undef KEY_LEFTPAREN
#undef KEY_RIGHTPAREN
#undef KEY_ASTERISK
#undef KEY_PLUS
#undef KEY_COMMA
#undef KEY_HYPHEN
#undef KEY_PERIOD
#undef KEY_SLASH
#undef KEY_COLON
#undef KEY_SEMICOLON
#undef KEY_LESS
#undef KEY_EQUALS
#undef KEY_GREATER
#undef KEY_QUESTION
#undef KEY_ATSIGN
#undef KEY_LEFTBRACKET
#undef KEY_BACKSLASH
#undef KEY_RIGHTBRACKET
#undef KEY_CIRCUMFLEX
#undef KEY_UNDERSCORE
#undef KEY_BACKQUOTE
#undef KEY_LEFTBRACE
#undef KEY_PIPE
#undef KEY_RIGHTBRACE
#undef KEY_TILDE
#undef KEY_POUND
#undef KEY_YEN
#undef KEY_EURO
#undef KEY_LEFTSHIFT
#undef KEY_RIGHTSHIFT
#undef KEY_LEFTCONTROL
#undef KEY_RIGHTCONTROL
#undef KEY_LEFTALT
#undef KEY_RIGHTALT
#undef KEY_LEFTMETA
#undef KEY_RIGHTMETA
#undef KEY_LEFTSUPER
#undef KEY_RIGHTSUPER
#undef KEY_NUMLOCK
#undef KEY_CAPSLOCK
#undef KEY_SCROLLLOCK
#undef KEY_BACKSPACE
#undef KEY_UNDO
#undef KEY_INSERT
#undef KEY_DELETE
#undef KEY_HOME
#undef KEY_END
#undef KEY_PAGEUP
#undef KEY_PAGEDOWN
#undef KEY_UP
#undef KEY_DOWN
#undef KEY_LEFT
#undef KEY_RIGHT
#undef KEY_ESCAPE
#undef KEY_F1
#undef KEY_F2
#undef KEY_F3
#undef KEY_F4
#undef KEY_F5
#undef KEY_F6
#undef KEY_F7
#undef KEY_F8
#undef KEY_F9
#undef KEY_F10
#undef KEY_F11
#undef KEY_F12
#undef KEY_F13
#undef KEY_F14
#undef KEY_F15
#undef KEY_F16
#undef KEY_F17
#undef KEY_F18
#undef KEY_F19
#undef KEY_F20
#undef KEY_PRINTSCREEN
#undef KEY_PAUSE
#undef KEY_MENU
#undef KEY_NUMPAD_0
#undef KEY_NUMPAD_1
#undef KEY_NUMPAD_2
#undef KEY_NUMPAD_3
#undef KEY_NUMPAD_4
#undef KEY_NUMPAD_5
#undef KEY_NUMPAD_6
#undef KEY_NUMPAD_7
#undef KEY_NUMPAD_8
#undef KEY_NUMPAD_9
#undef KEY_NUMPAD_DECIMAL
#undef KEY_NUMPAD_DIVIDE
#undef KEY_NUMPAD_MULTIPLY
#undef KEY_NUMPAD_SUBTRACT
#undef KEY_NUMPAD_ADD
#undef KEY_NUMPAD_EQUALS
#undef KEY_NUMPAD_ENTER
#undef KEY_ANDROID_BACK

#include <X11/X.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XInput2.h>
/* X11 headers don't define these, but we can guess what their values
 * should be... */
#define Button6  (Button5 + 1)
#define Button7  (Button5 + 2)

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/*---------------------------- Key code map -----------------------------*/

/* Mapping from X11 KeySyms to SIL keycodes.  This is sorted at init time
 * so we can binary-search it for keysyms. */
static uint8_t keysym_map_sorted;  // Flag: have we sorted it yet?
static struct KeysymMapEntry {
    KeySym keysym;
    int sil_keycode;
} keysym_map[] = {
    {XK_BackSpace,          KEY_BACKSPACE},
    {XK_Tab,                KEY_TAB},
    {XK_Return,             KEY_ENTER},
    {XK_Pause,              KEY_PAUSE},
    {XK_Scroll_Lock,        KEY_SCROLLLOCK},
    {XK_Sys_Req,            KEY_PRINTSCREEN},
    {XK_Escape,             KEY_ESCAPE},
    {XK_Delete,             KEY_DELETE},
    {XK_Kanji,              KEY_KANJI},
    {XK_Muhenkan,           KEY_MUHENKAN},
    {XK_Henkan_Mode,        KEY_HENKAN},
    {XK_Romaji,             KEY__NONE},
    {XK_Hiragana,           KEY__NONE},
    {XK_Katakana,           KEY__NONE},
    {XK_Hiragana_Katakana,  KEY_KANA},
    {XK_Zenkaku,            KEY__NONE},
    {XK_Hankaku,            KEY__NONE},
    {XK_Zenkaku_Hankaku,    KEY_KANJI},
    {XK_Touroku,            KEY__NONE},
    {XK_Massyo,             KEY__NONE},
    {XK_Kana_Lock,          KEY__NONE},
    {XK_Kana_Shift,         KEY__NONE},
    {XK_Eisu_Shift,         KEY__NONE},
    {XK_Eisu_toggle,        KEY__NONE},  // Lowercase "t" as in X11 header.
    {XK_Kanji_Bangou,       KEY__NONE},
    {XK_Zen_Koho,           KEY__NONE},
    {XK_Mae_Koho,           KEY__NONE},
    {XK_Home,               KEY_HOME},
    {XK_Left,               KEY_LEFT},
    {XK_Up,                 KEY_UP},
    {XK_Right,              KEY_RIGHT},
    {XK_Down,               KEY_DOWN},
    {XK_Page_Up,            KEY_PAGEUP},
    {XK_Page_Down,          KEY_PAGEDOWN},
    {XK_End,                KEY_END},
    {XK_Begin,              KEY__NONE},
    {XK_Select,             KEY__NONE},
    {XK_Print,              KEY__NONE},
    {XK_Execute,            KEY__NONE},
    {XK_Insert,             KEY_INSERT},
    {XK_Undo,               KEY_UNDO},
    {XK_Redo,               KEY__NONE},
    {XK_Menu,               KEY_MENU},
    {XK_Find,               KEY__NONE},
    {XK_Cancel,             KEY__NONE},
    {XK_Help,               KEY__NONE},
    {XK_Break,              KEY__NONE},
    {XK_Mode_switch,        KEY__NONE},
    {XK_Num_Lock,           KEY_NUMLOCK},
    {XK_KP_Space,           KEY__NONE},
    {XK_KP_Tab,             KEY__NONE},
    {XK_KP_Enter,           KEY_NUMPAD_ENTER},
    {XK_KP_F1,              KEY__NONE},
    {XK_KP_F2,              KEY__NONE},
    {XK_KP_F3,              KEY__NONE},
    {XK_KP_F4,              KEY__NONE},
    {XK_KP_Home,            KEY_NUMPAD_7},
    {XK_KP_Left,            KEY_NUMPAD_4},
    {XK_KP_Up,              KEY_NUMPAD_8},
    {XK_KP_Right,           KEY_NUMPAD_6},
    {XK_KP_Down,            KEY_NUMPAD_2},
    {XK_KP_Page_Up,         KEY_NUMPAD_9},
    {XK_KP_Page_Down,       KEY_NUMPAD_3},
    {XK_KP_End,             KEY_NUMPAD_1},
    {XK_KP_Begin,           KEY_NUMPAD_5},
    {XK_KP_Insert,          KEY_NUMPAD_0},
    {XK_KP_Delete,          KEY_NUMPAD_DECIMAL},
    {XK_KP_Equal,           KEY_NUMPAD_EQUALS},
    {XK_KP_Multiply,        KEY_NUMPAD_MULTIPLY},
    {XK_KP_Add,             KEY_NUMPAD_ADD},
    {XK_KP_Separator,       KEY__NONE},
    {XK_KP_Subtract,        KEY_NUMPAD_SUBTRACT},
    {XK_KP_Decimal,         KEY_NUMPAD_DECIMAL},
    {XK_KP_Divide,          KEY_NUMPAD_DIVIDE},
    {XK_KP_0,               KEY_NUMPAD_0},
    {XK_KP_1,               KEY_NUMPAD_1},
    {XK_KP_2,               KEY_NUMPAD_2},
    {XK_KP_3,               KEY_NUMPAD_3},
    {XK_KP_4,               KEY_NUMPAD_4},
    {XK_KP_5,               KEY_NUMPAD_5},
    {XK_KP_6,               KEY_NUMPAD_6},
    {XK_KP_7,               KEY_NUMPAD_7},
    {XK_KP_8,               KEY_NUMPAD_8},
    {XK_KP_9,               KEY_NUMPAD_9},
    {XK_F1,                 KEY_F1},
    {XK_F2,                 KEY_F2},
    {XK_F3,                 KEY_F3},
    {XK_F4,                 KEY_F4},
    {XK_F5,                 KEY_F5},
    {XK_F6,                 KEY_F6},
    {XK_F7,                 KEY_F7},
    {XK_F8,                 KEY_F8},
    {XK_F9,                 KEY_F9},
    {XK_F10,                KEY_F10},
    {XK_F11,                KEY_F11},
    {XK_F12,                KEY_F12},
    {XK_F13,                KEY_F13},
    {XK_F14,                KEY_F14},
    {XK_F15,                KEY_F15},
    {XK_F16,                KEY_F16},
    {XK_F17,                KEY_F17},
    {XK_F18,                KEY_F18},
    {XK_F19,                KEY_F19},
    {XK_F20,                KEY_F20},
    {XK_Shift_L,            KEY_LEFTSHIFT},
    {XK_Shift_R,            KEY_RIGHTSHIFT},
    {XK_Control_L,          KEY_LEFTCONTROL},
    {XK_Control_R,          KEY_RIGHTCONTROL},
    {XK_Caps_Lock,          KEY_CAPSLOCK},
    {XK_Shift_Lock,         KEY__NONE},
    {XK_Meta_L,             KEY_LEFTMETA},
    {XK_Meta_R,             KEY_RIGHTMETA},
    {XK_Alt_L,              KEY_LEFTALT},
    {XK_Alt_R,              KEY_RIGHTALT},
    {XK_Super_L,            KEY_LEFTSUPER},
    {XK_Super_R,            KEY_RIGHTSUPER},
    {XK_Hyper_L,            KEY__NONE},
    {XK_Hyper_R,            KEY__NONE},
    {XK_space,              KEY_SPACE},
    {XK_exclam,             KEY_EXCLAMATION},
    {XK_quotedbl,           KEY_QUOTE},
    {XK_numbersign,         KEY_SHARP},
    {XK_dollar,             KEY_DOLLAR},
    {XK_percent,            KEY_PERCENT},
    {XK_ampersand,          KEY_AMPERSAND},
    {XK_apostrophe,         KEY_APOSTROPHE},
    {XK_parenleft,          KEY_LEFTPAREN},
    {XK_parenright,         KEY_RIGHTPAREN},
    {XK_asterisk,           KEY_ASTERISK},
    {XK_plus,               KEY_PLUS},
    {XK_comma,              KEY_COMMA},
    {XK_minus,              KEY_HYPHEN},
    {XK_period,             KEY_PERIOD},
    {XK_slash,              KEY_SLASH},
    {XK_0,                  KEY_0},
    {XK_1,                  KEY_1},
    {XK_2,                  KEY_2},
    {XK_3,                  KEY_3},
    {XK_4,                  KEY_4},
    {XK_5,                  KEY_5},
    {XK_6,                  KEY_6},
    {XK_7,                  KEY_7},
    {XK_8,                  KEY_8},
    {XK_9,                  KEY_9},
    {XK_colon,              KEY_COLON},
    {XK_semicolon,          KEY_SEMICOLON},
    {XK_less,               KEY_LESS},
    {XK_equal,              KEY_EQUALS},
    {XK_greater,            KEY_GREATER},
    {XK_question,           KEY_QUESTION},
    {XK_at,                 KEY_ATSIGN},
    {XK_A,                  KEY_A},
    {XK_B,                  KEY_B},
    {XK_C,                  KEY_C},
    {XK_D,                  KEY_D},
    {XK_E,                  KEY_E},
    {XK_F,                  KEY_F},
    {XK_G,                  KEY_G},
    {XK_H,                  KEY_H},
    {XK_I,                  KEY_I},
    {XK_J,                  KEY_J},
    {XK_K,                  KEY_K},
    {XK_L,                  KEY_L},
    {XK_M,                  KEY_M},
    {XK_N,                  KEY_N},
    {XK_O,                  KEY_O},
    {XK_P,                  KEY_P},
    {XK_Q,                  KEY_Q},
    {XK_R,                  KEY_R},
    {XK_S,                  KEY_S},
    {XK_T,                  KEY_T},
    {XK_U,                  KEY_U},
    {XK_V,                  KEY_V},
    {XK_W,                  KEY_W},
    {XK_X,                  KEY_X},
    {XK_Y,                  KEY_Y},
    {XK_Z,                  KEY_Z},
    {XK_bracketleft,        KEY_LEFTBRACKET},
    {XK_backslash,          KEY_BACKSLASH},
    {XK_bracketright,       KEY_RIGHTBRACKET},
    {XK_asciicircum,        KEY_CIRCUMFLEX},
    {XK_underscore,         KEY_UNDERSCORE},
    {XK_grave,              KEY_BACKQUOTE},
    {XK_a,                  KEY_A},
    {XK_b,                  KEY_B},
    {XK_c,                  KEY_C},
    {XK_d,                  KEY_D},
    {XK_e,                  KEY_E},
    {XK_f,                  KEY_F},
    {XK_g,                  KEY_G},
    {XK_h,                  KEY_H},
    {XK_i,                  KEY_I},
    {XK_j,                  KEY_J},
    {XK_k,                  KEY_K},
    {XK_l,                  KEY_L},
    {XK_m,                  KEY_M},
    {XK_n,                  KEY_N},
    {XK_o,                  KEY_O},
    {XK_p,                  KEY_P},
    {XK_q,                  KEY_Q},
    {XK_r,                  KEY_R},
    {XK_s,                  KEY_S},
    {XK_t,                  KEY_T},
    {XK_u,                  KEY_U},
    {XK_v,                  KEY_V},
    {XK_w,                  KEY_W},
    {XK_x,                  KEY_X},
    {XK_y,                  KEY_Y},
    {XK_z,                  KEY_Z},
    {XK_braceleft,          KEY_LEFTBRACE},
    {XK_bar,                KEY_PIPE},
    {XK_braceright,         KEY_RIGHTBRACE},
    {XK_asciitilde,         KEY_TILDE},
    {XK_sterling,           KEY_POUND},
    {XK_yen,                KEY_YEN},
    {XK_EuroSign,           KEY_EURO},
};

/*------------------------- Convenience macros --------------------------*/

/* Convenience macros for handling bitmaps returned from event device
 * ioctls. */
#define DECLARE_BITS(name, size)  uint8_t name[((size)+7)/8]
#define BIT(array, index)         ((array)[((index)/8)] & (1 << ((index)%8)))

/*---------------------------- General data -----------------------------*/

/* Event callback passed to sys_input_init(). */
static InputEventCallback event_callback;

/* Flag indicating whether a quit event (window close, ^C, etc.) has been
 * received. */
static uint8_t quit_requested;

/* Flag indicating whether we should send joystick input events even while
 * the window is not focused. */
static uint8_t ignore_focus_for_joysticks;

/* Epoch for X11 event timestamps (in terms of time_now()). */
static double x11_timestamp_epoch;
/* Last timestamp seen in an X11 event (used for detecting wraparound).
 * A value of zero means the epoch has not yet been set. */
static uint32_t last_x11_timestamp;
/* XInput2 extension code (for handling XInput2 events). */
static int xinput2_opcode;

/*---------------------------- Joystick data ----------------------------*/

/* Maximum number of /dev/input/eventX devices to scan.  Note that at least
 * through Linux 3.18.2, the kernel has a hardcoded limit of 32 event
 * devices (EVDEV_MINORS in drivers/input/evdev.c), so this should be
 * reasonably future-proof. */
#define MAX_EVENT_DEVICES  64

/* Data for each /dev/input/eventX device node scanned (0 = node does not
 * exist). */
typedef struct EventDevInfo EventDevInfo;
struct EventDevInfo {
    /* File descriptor open for this device (-1 = not open). */
    int fd;
    /* True if the device was opened in read-only mode. */
    uint8_t readonly;
    /* Path of the device file. */
    char path[32];
    /* Name reported by the device. */
    char name[256];
    /* Device information flags from ioctl(EVIOCGBIT). */
    DECLARE_BITS(ev, EV_MAX+1);
    DECLARE_BITS(key, KEY_MAX+1);
    DECLARE_BITS(abs, ABS_MAX+1);
    /* System clock (as in clock_gettime()) used by events. */
    clockid_t clock_id;
};
static EventDevInfo event_info[MAX_EVENT_DEVICES];

/* Number of available joystick devices. */
static int num_joysticks;

/* Data for each joystick device. */
typedef struct JoystickInfo JoystickInfo;
struct JoystickInfo {
    /* Event device index (0 through MAX_EVENT_DEVICES-1), or -1 if this
     * joystick is currently disconnected. */
    int event_dev;

    /* Joystick ID (for detecting reconnections).  The input_id structure
     * from evdev is conveniently 64 bits wide, so we treat it as a single
     * integer. */
    union {
        uint64_t id;
        struct input_id id_struct;
    };

    /* Range data for analog axes. */
    struct input_absinfo absinfo[ABS_HAT3Y+1];

    /* Flag: Does this device support force feedback (rumble)? */
    uint8_t can_rumble;
    /* Rumble motor assignment type (JOYSTICK_LINUX_RUMBLE_* from
     * joystick-db.h). */
    JoystickLinuxRumbleType rumble_type;
    /* Flag: Is a force feedback effect defined? */
    uint8_t ff_effect_defined;
    /* Force feedback effect data. */
    struct ff_effect ff_effect;

    /* Number of buttons and sticks on this device. */
    int num_buttons;
    int num_sticks;

    /* Mapping from EV_KEY inputs to joystick button numbers (-1 = button
     * does not exist on this device). */
    int8_t ev_keymap_low[32];   // BTN_JOYSTICK (and BTN_GAMEPAD)
    int8_t ev_keymap_high[40];  // BTN_TRIGGER_HAPPY

    /* EV_KEY inputs for D-pad, or -1 if none. */
    int16_t dpad_up, dpad_down, dpad_left, dpad_right;
    /* EV_ABS inputs for D-pad, or -1 if none.  (If these are set, the EV_KEY
     * inputs will not be set, and vice versa.) */
    int16_t dpad_x, dpad_y;
    /* Current state of D-pad input. */
    uint8_t dpad_state_up, dpad_state_down, dpad_state_left, dpad_state_right;

    /* EV_ABS inputs for up to 3 sticks. */
    int8_t stick_x[3], stick_y[3];
    /* Current states of stick input. */
    Vector2f stick_state[3];
    /* Timestamp of the most recent change to stick_state if that change
     * has not yet been sent to the event callback, else zero. */
    double stick_timestamp[3];

    /* EV_ABS inputs for L2 and R2 (the left and right triggers), or -1 if
     * the inputs are buttons. */
    int8_t l2_abs, r2_abs;
    /* Current states of simulated buttons for EV_ABS triggers. */
    uint8_t l2_abs_state, r2_abs_state;

    /* Button mapping used by sys_input_joystick_button_mapping(). */
    int8_t button_map[INPUT_JOYBUTTON__NUM];
};
static JoystickInfo joysticks[MAX_EVENT_DEVICES];

/* Mutex for accessing joysticks[]. */
static SysMutexID joysticks_mutex;

/* Joystick information array returned for sys_input_info(). */
static SysInputJoystick joystick_info[MAX_EVENT_DEVICES];

/* Thread ID of joystick-scan thread. */
static SysThreadID scan_joystick_thread_id;
/* Pipe used to tell the joystick-scan thread to stop. */
static int scan_joystick_thread_stop_pipe[2];

/*---------------------------- Keyboard data ----------------------------*/

/* Current state of all keys, used to simulate key releases in
 * linux_clear_window_input_state().  The value of each element is the X11
 * keycode for the key if pressed, 0 otherwise. */
static int keystate[KEY__LAST];

/* Delayed key release state.  The value of each element is the X11
 * keycode for the key if a delayed release was detected, 0 otherwise.
 * See KeyRelease handling in handle_key_event() for why we need this. */
static int key_release[KEY__LAST];

/* Flags indicating keys which were pressed during this iteration of
 * sys_input_update(), used for the same purpose. */
static uint8_t newkeys[KEY__LAST];

/*----------------------------- Mouse data ------------------------------*/

/* Mouse button state and last recorded position, for
 * linux_clear_window_input_state(). */
static uint8_t mouse_lbutton, mouse_mbutton, mouse_rbutton;
static float mouse_x, mouse_y;

/* Mouse position override, for sys_input_mouse_set_position() (see comments
 * in that function). */
static uint8_t mouse_position_overridden;
static int mouse_position_override_x, mouse_position_override_y;

/* Timeout for mouse position override, used to avoid loss of mouse input
 * in case the event corresponding to an override gets dropped (as has
 * been observed to happen if the user is moving the window at the time of
 * the XWarpPointer() call). */
static double mouse_position_override_timeout;

/*--------------------------- Text input data ---------------------------*/

/* Text input flag. */
static uint8_t text_active;

/*--------------------- Local routine declarations ----------------------*/

/**
 * scan_joystick:  Scan the given event device and add it to the joystick
 * array if appropriate.
 *
 * [Parameters]
 *     index: Device index (0...MAX_EVENT_DEVICES-1).
 */
static void scan_joystick(int index);

/**
 * scan_joystick_thread:  Thread routine to watch for joystick devices
 * being added or removed and update internal state accordingly.
 *
 * [Parameters]
 *     inotify_fd: inotify file descriptor for watching /dev/input.
 *         Closed on return.
 * [Return value]
 *     0
 */
static int scan_joystick_thread(void *unused);

/**
 * init_joystick:  Initialize the joystick data for the given joystick.
 *
 * [Parameters]
 *     joystick: JoystickInfo structure of joystick to initialize.
 *     device: Event device index (0...MAX_EVENT_DEVICES-1).
 *     id: Joystick ID.
 */
static void init_joystick(JoystickInfo *joystick, int index, uint64_t id);

/**
 * joystick_db_value_index_to_abs:  Return the evdev ABS_* code
 * corresponding to a JOYSTICK_VALUE_* index from the joystick database.
 *
 * [Parameters]
 *     index: Value index (JOYSTICK_VALUE_*, but not JOYSTICK_VALUE_NONE).
 * [Return value]
 *     Corresponding ABS_* code.
 */
static CONST_FUNCTION int joystick_db_value_index_to_abs(
    JoystickValueInput index);

/**
 * joystick_button_to_key:  Return the evdev KEY_* code corresponding to
 * the given joystick button.  Helper function for init_joystick().
 *
 * [Parameters]
 *     joystick: JoystickInfo structure of joystick to examine.
 *     button: Button index to look up.
 * [Return value]
 *     Keycode for EV_KEY events corresponding to the given button, or -1
 *     if the button is not defined.
 */
static int joystick_button_to_key(const JoystickInfo *joystick, int button);

/**
 * handle_joystick_abs_event:  Process an EV_ABS event received from a
 * joystick device.
 *
 * [Parameters]
 *     device: Device index.
 *     timestamp: Event timestamp, compatible with time_now().
 *     input: Index of the input to which the event relates (evdev ABS_*).
 *     raw_value: Raw value of the input.
 */
static void handle_joystick_abs_event(int device, double timestamp,
                                      int input, int raw_value);

/**
 * handle_joystick_key_event:  Process an EV_KEY event received from a
 * joystick device.
 *
 * [Parameters]
 *     device: Device index.
 *     timestamp: Event timestamp, compatible with time_now().
 *     input: Index of the input to which the event relates (evdev KEY_* or
 *         BTN_*).
 *     state: True if the key is now pressed, false if it is now released.
 */
static void handle_joystick_key_event(int device, double timestamp,
                                      int input, int state);

/**
 * update_stick:  Update the X or Y coordinate of a stick on a joystick
 * device.  If a previous change is already pending, that event is sent out.
 *
 * [Parameters]
 *     device: Device index.
 *     timestamp: Event timestamp, compatible with time_now().
 *     stick: Stick to update.
 *     is_y: 0 if updating the X axis, 1 if updating the Y axis.
 *     value: New value of axis.
 */
static void update_stick(int device, double timestamp, int stick, int is_y,
                         float value);

/**
 * flush_joystick_events:  Flush any pending stick events on the given
 * joystick device.
 *
 * [Parameters]
 *     device: Device index.
 */
static void flush_joystick_events(int device);

/**
 * handle_key_event:  Process a KeyPress or KeyRelease event.
 *
 * [Parameters]
 *     event: X11 event.
 */
static void handle_key_event(const XKeyEvent *event);

/**
 * handle_button_event:  Process a ButtonPress or ButtonRelease event.
 *
 * [Parameters]
 *     event: X11 event.
 */
static void handle_button_event(const XButtonEvent *event);

/**
 * handle_motion_event:  Process a MotionNotify event.
 *
 * [Parameters]
 *     event: X11 event.
 */
static void handle_motion_event(const XMotionEvent *event);

/**
 * handle_motion_event:  Process a MotionNotify event.
 *
 * [Parameters]
 *     event: X11 event.
 */
static void handle_motion_event(const XMotionEvent *event);

/**
 * handle_enter_window_event:  Process an EnterNotify event.
 *
 * [Parameters]
 *     event: X11 event.
 */
static void handle_enter_window_event(const XEnterWindowEvent *event);

/**
 * handle_touch_event:  Process a TouchBegin/TouchUpdate/TouchEnd event.
 *
 * [Parameters]
 *     event: XInput2 event.
 */
static void handle_touch_event(const XIDeviceEvent *event);

/**
 * handle_xinput2_event:  Process an event from the XInput2 extension.
 *
 * [Parameters]
 *     event: X11 event.
 */
static void handle_xinput2_event(const XGenericEventCookie *event);

/**
 * convert_x11_timestamp:  Return a SIL timestamp corresponding to the
 * given X11 event timestamp.
 *
 * [Parameters]
 *     timestamp: X11 timestamp to convert.
 * [Return value]
 *     Equivalent SIL timestamp.
 */
static double convert_x11_timestamp(uint32_t timestamp);

/**
 * convert_x11_keysym:  Return a SIL keycode corresponding to the given
 * X11 KeySym.
 *
 * [Parameters]
 *     keysym: X11 KeySym to convert.
 * [Return value]
 *     Corresponding SIL keycode, or KEY__NONE if none.
 */
static int convert_x11_keysym(KeySym keysym);

/**
 * convert_mouse_coords:  Convert mouse coordinates reported by X11 to
 * those expected by the SIL input subsystem.  This function takes into
 * account the position override set by sys_input_mouse_set_position().
 *
 * [Parameters]
 *     x_in, y_in: Integer mouse coordinates from X11.
 *     x_out, y_out: Pointers to variables to receive the converted
 *         floating-point mouse coordinates.
 */
static void convert_mouse_coords(int x_in, int y_in,
                                 float *x_out, float *y_out);

/**
 * normalize_joystick_axis:  Convert the given EV_ABS event input to a
 * normalized floating-point value in the range [-1,+1].
 *
 * [Parameters]
 *     raw: Raw input value.
 *     absinfo: Range data for the input.
 * [Return value]
 *     Normalized input value, a floating-point value in [-1,+1].
 */
static float normalize_joystick_axis(int raw,
                                     const struct input_absinfo *absinfo);

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
 * send_joystick_button_event:  Send a button event for a joystick device.
 *
 * [Parameters]
 *     timestamp: Event timestamp.
 *     device: Device index.
 *     button: Button index.
 *     value: Button state (true = pressed, false = released).
 */
static void send_joystick_button_event(double timestamp, int device,
                                       int button, int value);

/**
 * send_joystick_dpad_event:  Send a D-pad event for a joystick device.
 *
 * [Parameters]
 *     timestamp: Event timestamp.
 *     device: Device index.
 */
static void send_joystick_dpad_event(double timestamp, int device);

/**
 * send_joystick_stick_event:  Send a stick event for a joystick device.
 *
 * [Parameters]
 *     timestamp: Event timestamp.
 *     device: Device index.
 *     stick: Stick index.
 *     value: Stick X/Y values.
 */
static void send_joystick_stick_event(double timestamp, int device, int stick,
                                      const Vector2f *value);

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int sys_input_init(void (*event_callback_)(const struct InputEvent *))
{
    PRECOND(event_callback_ != NULL, return 0);

    if (!keysym_map_sorted) {
        for (int i = 0; i < lenof(keysym_map) - 1; i++) {
            int best = i;
            for (int j = i+1; j < lenof(keysym_map); j++) {
                if (keysym_map[j].keysym < keysym_map[best].keysym) {
                    best = j;
                }
            }
            if (best != i) {
                struct KeysymMapEntry temp = keysym_map[i];
                keysym_map[i] = keysym_map[best];
                keysym_map[best] = temp;
            }
        }
        keysym_map_sorted = 1;
        for (int i = 0; i < lenof(keysym_map) - 1; i++) {
            ASSERT(keysym_map[i].keysym < keysym_map[i+1].keysym);
        }
    }

    event_callback = event_callback_;

    joysticks_mutex = sys_mutex_create(0, 0);
    if (UNLIKELY(!joysticks_mutex)) {
        DLOG("Failed to create joystick mutex");
        return 0;
    }

    for (int i = 0; i < lenof(event_info); i++) {
        event_info[i].fd = -1;
    }
    num_joysticks = 0;
    scan_joystick_thread_id = 0;
    ignore_focus_for_joysticks = 1;

    DIR *d = opendir("/dev/input");
    if (!d) {
        DLOG("Can't open /dev/input, no joysticks will be available");
        goto done;
    }

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (strncmp(de->d_name, "event", 5) == 0) {
            char *s = &de->d_name[5];
            unsigned long index = strtoul(s, &s, 10);
            if (!*s && index < lenof(event_info)) {
                scan_joystick((int)index);
            }
        }
    }
    closedir(d);

    const int inotify_fd = inotify_init();
    if (UNLIKELY(inotify_fd == -1)) {
        DLOG("inotify_init() failed: %s", strerror(errno));
        /* This could fail due to kernel settings (CONFIG_INOTIFY_USER=n),
         * so don't abort. */
        DLOG("Joystick connect/disconnect support will be disabled.");
        goto done;
    }
    /* We deliberately skip graceful error handling here, since this call
     * should "never" fail: (1) /dev/input is known to exist and be
     * readable (due to opendir() success above), and (2) the inotify
     * facility is known to be available (due to inotify_init() success).
     * The only conceivable cause of failure would be an external process
     * making /dev/input unreadable at this exact instant, which should
     * not happen outside of a targeted attack. */
    ASSERT(inotify_add_watch(inotify_fd, "/dev/input",
                             IN_ATTRIB | IN_CREATE | IN_DELETE) != -1,
           goto done);
    /* This similarly is highly unlikely to fail, but it's conceivable (it
     * could fail if the inotify descriptor filled the last slot in the
     * process's file descriptor table), so we handle errors gracefully. */
    if (UNLIKELY(pipe(scan_joystick_thread_stop_pipe) != 0)) {
        DLOG("pipe() failed, joystick connect/disconnect support will be"
             " disabled: %s", strerror(errno));
        close(inotify_fd);
        goto done;
    }
    static const ThreadAttributes attr;  // All zero.
    scan_joystick_thread_id = sys_thread_create(&attr, scan_joystick_thread,
                                                (void *)(intptr_t)inotify_fd);
    if (UNLIKELY(!scan_joystick_thread_id)) {
        DLOG("Failed to create joystick scanning thread");
        close(scan_joystick_thread_stop_pipe[0]);
        close(scan_joystick_thread_stop_pipe[1]);
        scan_joystick_thread_stop_pipe[0] = -1;
        scan_joystick_thread_stop_pipe[1] = -1;
        close(inotify_fd);
        goto done;
    }

    if (!XQueryExtension(linux_x11_display(), "XInputExtension",
                         &xinput2_opcode, (int[1]){0}, (int[1]){0})) {
        xinput2_opcode = 0;
    }

  done:
    mem_clear(keystate, sizeof(keystate));
    mouse_lbutton = 0;
    mouse_mbutton = 0;
    mouse_rbutton = 0;
    mouse_position_overridden = 0;
    text_active = 0;
    quit_requested = 0;
    last_x11_timestamp = 0;  // Force setting epoch on the first event.

    return 1;
}

/*-----------------------------------------------------------------------*/

void sys_input_cleanup(void)
{
    if (scan_joystick_thread_id) {
        close(scan_joystick_thread_stop_pipe[1]); // Causes select() to return.
        int unused_retval;
        sys_thread_wait(scan_joystick_thread_id, &unused_retval);
        close(scan_joystick_thread_stop_pipe[0]);
        scan_joystick_thread_id = 0;
        scan_joystick_thread_stop_pipe[0] = -1;
        scan_joystick_thread_stop_pipe[1] = -1;
    }

    for (int i = 0; i < lenof(event_info); i++) {
        if (event_info[i].fd >= 0) {
            close(event_info[i].fd);
        }
    }

    sys_mutex_destroy(joysticks_mutex);
    joysticks_mutex = 0;
}

/*-----------------------------------------------------------------------*/

void sys_input_update(void)
{
    const double now = time_now();

    mem_clear(newkeys, sizeof(newkeys));
    for (int key = 1; key < lenof(key_release); key++) {
        if (key_release[key]) {
            (*event_callback)(&(InputEvent){
                .type = INPUT_EVENT_KEYBOARD, .detail = INPUT_KEYBOARD_KEY_UP,
                .timestamp = now,
                {.keyboard = {.key = key, .system_key = key_release[key],
                              .is_repeat = 0}}});
            key_release[key] = 0;
        }
    }

    if (linux_x11_window()) {
        XFlush(linux_x11_display());
        XEvent event;
        while (linux_get_window_event(&event)) {
            switch (event.type) {
              case KeyPress:
              case KeyRelease:
                handle_key_event(&event.xkey);
                break;
              case ButtonPress:
              case ButtonRelease:
                handle_button_event(&event.xbutton);
                break;
              case EnterNotify:
                handle_enter_window_event(&event.xcrossing);
                break;
              case MotionNotify:
                handle_motion_event(&event.xmotion);
                break;
              case GenericEvent:
                XGetEventData(linux_x11_display(), &event.xcookie);
                if (event.xcookie.extension == xinput2_opcode) {
                    handle_xinput2_event(&event.xcookie);
                }
                XFreeEventData(linux_x11_display(), &event.xcookie);
                break;
            }
        }
    }

    /* Since we read directly from the input device, we'll get input
     * events regardless of whether we currently have the X11 input focus.
     * We need to keep our internal state current so we process the events
     * regardless of focus, but we skip the actual call to the event
     * callback (in the send_joystick_*_event() functions) when not
     * focused if the user has requested us to do so.  However, joystick
     * connect/disconnect messages are always sent regardless of focus
     * since they're not "input" in the same sense as other joystick events. */
    sys_mutex_lock(joysticks_mutex, -1);
    for (int i = 0; i < num_joysticks; i++) {
        const int device = joysticks[i].event_dev;
        if (device < 0) {
            continue;
        }
        const int fd = event_info[device].fd;
        struct input_event event;
        int nread;
        while ((nread = read(fd, &event, sizeof(event))) == sizeof(event)) {
            double timestamp;
            if (event_info[device].clock_id == sys_posix_time_clock()) {
                uint64_t epoch = sys_posix_time_epoch();
                if (sys_time_unit() == 1000000000) {
                    epoch /= 1000;
                }
                const uint64_t time = ((uint64_t)event.time.tv_sec * 1000000
                                       + event.time.tv_usec);
                timestamp = (time - epoch) / 1.0e6;
            } else {
                /* Failure means sys_time_now() is using CLOCK_MONOTONIC
                 * but the kernel doesn't support the EVIOCSCLOCKID ioctl.
                 * This means evdev timestamps aren't compatible with
                 * sys_time_now() time values, so we ignore them and just
                 * use the current time for our event timestamps. */
                timestamp = now;
            }
            if (event.type == EV_ABS) {
                handle_joystick_abs_event(i, timestamp, event.code,
                                          event.value);
            } else if (event.type == EV_KEY) {
                handle_joystick_key_event(i, timestamp, event.code,
                                          event.value);
            }
        }
        flush_joystick_events(i);
        if (nread < 0 && errno != EAGAIN) {
            DLOG("Joystick %d disconnected (read error: %s)", i,
                 strerror(errno));
            send_joystick_connect_event(i, INPUT_JOYSTICK_DISCONNECTED);
            joysticks[i].event_dev = -1;
        }
    }
    sys_mutex_unlock(joysticks_mutex);
}

/*-----------------------------------------------------------------------*/

void sys_input_info(SysInputInfo *info_ret)
{
    info_ret->has_joystick = (num_joysticks > 0);
    info_ret->num_joysticks = num_joysticks;
    info_ret->joysticks = joystick_info;
    for (int i = 0; i < num_joysticks; i++) {
        joystick_info[i].connected   = (joysticks[i].event_dev >= 0);
        joystick_info[i].can_rumble  = joysticks[i].can_rumble;
        joystick_info[i].num_buttons = joysticks[i].num_buttons;
        joystick_info[i].num_sticks  = joysticks[i].num_sticks;
    }

    /* We assume that a keyboard and mouse are present. */
    info_ret->has_keyboard = 1;
    info_ret->keyboard_is_full = 1;
    info_ret->has_mouse = 1;

    /* We convert keypresses into text events when text input is enabled. */
    info_ret->has_text = 1;
    info_ret->text_uses_custom_interface = 0;
    info_ret->text_has_prompt = 0;

    info_ret->has_touch = linux_x11_touchscreen_present();
}

/*-----------------------------------------------------------------------*/

void sys_input_grab(int grab)
{
    linux_set_window_grab(grab);
}

/*-----------------------------------------------------------------------*/

int sys_input_is_quit_requested(void)
{
    return quit_requested;
}

/*-----------------------------------------------------------------------*/

int sys_input_is_suspend_requested(void)
{
    /* Not supported on Linux. */
    return 0;
}

/*-----------------------------------------------------------------------*/

void sys_input_acknowledge_suspend_request(void)
{
    /* Not supported on Linux. */
}

/*************************************************************************/
/********************* Interface: Joystick handling **********************/
/*************************************************************************/

void sys_input_enable_unfocused_joystick(int enable)
{
    ignore_focus_for_joysticks = (enable != 0);
}

/*-----------------------------------------------------------------------*/

char *sys_input_joystick_copy_name(int index)
{
    char *retval;
    sys_mutex_lock(joysticks_mutex, -1);
    if (joysticks[index].event_dev >= 0) {
        retval = mem_strdup(event_info[joysticks[index].event_dev].name, 0);
    } else {
        /* The device must have been removed since the last call to
         * sys_input_info(). */
        retval = NULL;
    }
    sys_mutex_unlock(joysticks_mutex);
    return retval;
}

/*-----------------------------------------------------------------------*/

int sys_input_joystick_button_mapping(int index, int name)
{
    int retval;
    sys_mutex_lock(joysticks_mutex, -1);
    if (joysticks[index].event_dev >= 0) {
        retval = joysticks[index].button_map[name];
    } else {
        retval = -1;
    }
    sys_mutex_unlock(joysticks_mutex);
    return retval;
}

/*-----------------------------------------------------------------------*/

void sys_input_joystick_rumble(int index, float left, float right, float time)
{
    sys_mutex_lock(joysticks_mutex, -1);
    if (joysticks[index].event_dev < 0) {
        sys_mutex_unlock(joysticks_mutex);
        return;
    }

    /* Note that event_info[] access is safe without a separate lock
     * because the event_info[] entry for a device is not modified while
     * it is assigned to a joystick, and that assignment can only be
     * removed while joysticks_mutex is locked. */
    const int fd = event_info[joysticks[index].event_dev].fd;

    if (joysticks[index].ff_effect.id != -1) {
        if (UNLIKELY(ioctl(fd, EVIOCRMFF, joysticks[index].ff_effect.id) < 0)) {
            DLOG("%s: ioctl(EVIOCRMFF, %d): %s",
                 event_info[joysticks[index].event_dev].path,
                 joysticks[index].ff_effect.id, strerror(errno));
            sys_mutex_unlock(joysticks_mutex);
            return;
        }
        joysticks[index].ff_effect.id = -1;
    }

    if (time == 0) {
        sys_mutex_unlock(joysticks_mutex);
        return;
    }

    /* The Linux driver uses "strong" and "weak" rather than "left" and
     * "right" to differentiate the motors.  If we know which is which, we
     * set the individual motor strengths appropriately; otherwise we just
     * take the average and assign it to both. */
    const int left_i = iroundf(left * 0xFFFF);
    const int right_i = iroundf(right * 0xFFFF);
    const int both_i = iroundf(((left + right) / 2) * 0xFFFF);
    joysticks[index].ff_effect.type = FF_RUMBLE;
    switch (joysticks[index].rumble_type) {
      case JOYSTICK_LINUX_RUMBLE_LEFT_STRONG:
        joysticks[index].ff_effect.u.rumble.strong_magnitude = left_i;
        joysticks[index].ff_effect.u.rumble.weak_magnitude = right_i;
        break;
      case JOYSTICK_LINUX_RUMBLE_RIGHT_STRONG:
        joysticks[index].ff_effect.u.rumble.strong_magnitude = right_i;
        joysticks[index].ff_effect.u.rumble.weak_magnitude = left_i;
        break;
      default:
        joysticks[index].ff_effect.u.rumble.strong_magnitude = both_i;
        joysticks[index].ff_effect.u.rumble.weak_magnitude = both_i;
        break;
    }
    joysticks[index].ff_effect.replay.length = iroundf(time * 1000);
    joysticks[index].ff_effect.replay.delay = 0;
    if (UNLIKELY(ioctl(fd, EVIOCSFF, &joysticks[index].ff_effect) < 0)) {
        DLOG("%s: ioctl(EVIOCSFF): %s",
             event_info[joysticks[index].event_dev].path, strerror(errno));
        joysticks[index].ff_effect.id = -1;  // Just in case.
        sys_mutex_unlock(joysticks_mutex);
        return;
    }

    struct input_event event;
    gettimeofday(&event.time, NULL);
    event.type = EV_FF;
    event.code = joysticks[index].ff_effect.id;
    event.value = 1;
    if (UNLIKELY(write(fd, &event, sizeof(event)) != sizeof(event))) {
        DLOG("%s: write(): %s",
             event_info[joysticks[index].event_dev].path, strerror(errno));
    }

    sys_mutex_unlock(joysticks_mutex);
}

/*************************************************************************/
/*********************** Interface: Mouse handling ***********************/
/*************************************************************************/

void sys_input_mouse_set_position(float x, float y)
{
    Window window = linux_x11_window();
    if (window) {
        const int width = linux_x11_window_width();
        const int height = linux_x11_window_height();
        const int ix = bound(iroundf(x*width),  0, width-1);
        const int iy = bound(iroundf(y*height), 0, height-1);

        Display *display = linux_x11_display();
        Window unused_r, unused_c;
        int unused_rx, unused_ry, cur_x, cur_y;
        unsigned int unused_mask;
        if (UNLIKELY(!XQueryPointer(display, window, &unused_r, &unused_c,
                                    &unused_rx, &unused_ry, &cur_x, &cur_y,
                                    &unused_mask))) {
            DLOG("Failed to get pointer position");
            cur_x = cur_y = -1;
        }
        if (ix != cur_x || iy != cur_y) {
            XWarpPointer(display, None, window, 0, 0, 0, 0, ix, iy);
            (*event_callback)(&(InputEvent){
                .type = INPUT_EVENT_MOUSE, .detail = INPUT_MOUSE_MOVE,
                .timestamp = time_now(),
                {.mouse = {.x = (float)ix / width, .y = (float)iy / height}}});
            /* There may already be mouse events in the queue, so until the
             * MotionNotify event generated by this call is processed, we
             * override the mouse position to the requested one. */
            linux_override_mouse_position(ix, iy);
        }
    }
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
/******************* Linux-internal exported routines ********************/
/*************************************************************************/

void linux_clear_window_input_state(void)
{
    const double timestamp = time_now();

    for (int key = 0; key < lenof(keystate); key++) {
        if (keystate[key]) {
            (*event_callback)(&(InputEvent){
                .type = INPUT_EVENT_KEYBOARD, .detail = INPUT_KEYBOARD_KEY_UP,
                .timestamp = timestamp,
                {.keyboard = {.key = key, .system_key = keystate[key],
                              .is_repeat = 0}}});
            keystate[key] = 0;
        }
    }

    InputEvent mouse_event = {
        .type = INPUT_EVENT_MOUSE, .timestamp = timestamp};
    if (mouse_position_overridden) {
        convert_mouse_coords(
            mouse_position_override_x, mouse_position_override_y,
            &mouse_event.mouse.x, &mouse_event.mouse.y);
    } else {
        mouse_event.mouse.x = mouse_x;
        mouse_event.mouse.y = mouse_y;
    }
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

/*-----------------------------------------------------------------------*/

void linux_override_mouse_position(int x, int y)
{
    mouse_position_overridden = 1;
    mouse_position_override_x = x;
    mouse_position_override_y = y;
    mouse_position_override_timeout = time_now() + 1.0;
}

/*-----------------------------------------------------------------------*/

void linux_set_quit_requested(void)
{
    quit_requested = 1;
}

/*************************************************************************/
/******************* Local routines: Joystick handling *******************/
/*************************************************************************/

static void scan_joystick(int index)
{
    PRECOND(index >= 0 && index < lenof(event_info), return);

    ASSERT(strformat_check(event_info[index].path,
                           sizeof(event_info[index].path),
                           "/dev/input/event%d", index));
    const char *path = event_info[index].path;
    int readonly = 0;
    int fd = open(path, O_RDWR);
    if (fd < 0 && errno == EACCES) {
        readonly = 1;
        fd = open(path, O_RDONLY);
    }
    if (fd < 0) {
        return;
    }
    /* This call can't fail under current (<= 3.17) versions of Linux, and
     * there's no reason for it to fail in theory either -- but if by any
     * chance it does fail, sys_input_update() will block on every call
     * until a joystick event is received, so we check for failure just to
     * be safe. */
    if (UNLIKELY(fcntl(fd, F_SETFL, O_NONBLOCK) < 0)) {
        DLOG("%s: fcntl(F_SETFL, O_NONBLOCK): %s", path, strerror(errno));
        close(fd);
        return;
    }

    /*
     * Event devices don't have an explicit "device type", so the best we
     * can do is guess based on the device's capabilities.  We treat a
     * device as a joystick if it meets either of these criteria:
     *
     * (1) The device supports absolute X/Y inputs and at least one key in
     * the BTN_JOYSTICK or BTN_GAMEPAD range (note that the input subsystem
     * treats buttons as "keys").
     *
     * (2) The device supports D-pad key inputs (for example, a retro-style
     * gamepad without an analog stick).
     */
    if (UNLIKELY(ioctl(fd, EVIOCGBIT(0, sizeof(event_info[index].ev)),
                       event_info[index].ev) < 0)) {
        DLOG("%s: EVIOCGBIT(0): %s", path, strerror(errno));
        mem_clear(event_info[index].ev, sizeof(event_info[index].ev));
    }
    if (UNLIKELY(ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(event_info[index].key)),
                       event_info[index].key) < 0)) {
        DLOG("%s: EVIOCGBIT(EV_KEY): %s", path, strerror(errno));
        mem_clear(event_info[index].key, sizeof(event_info[index].key));
    }
    if (UNLIKELY(ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(event_info[index].abs)),
                       event_info[index].abs) < 0)) {
        DLOG("%s: EVIOCGBIT(EV_ABS): %s", path, strerror(errno));
        mem_clear(event_info[index].abs, sizeof(event_info[index].abs));
    }
    int is_joystick = 0;
    if (BIT(event_info[index].ev, EV_KEY)) {
        if (BIT(event_info[index].ev, EV_ABS)
         && BIT(event_info[index].abs, ABS_X)
         && BIT(event_info[index].abs, ABS_Y)) {
            for (int i = 0; i < 16; i++) {
                if (BIT(event_info[index].key, BTN_JOYSTICK+i)
                 || BIT(event_info[index].key, BTN_GAMEPAD+i)) {
                    is_joystick = 1;
                    break;
                }
            }
        } else if (BIT(event_info[index].key, BTN_DPAD_UP)) {
            is_joystick = 1;
        }
    }
    if (!is_joystick) {
        close(fd);
        return;
    }

    /* Tell the kernel to use the same system clock for event timestamps as
     * we do in sys_time_now(), so we can pass the timestamps to the upper
     * layer.  This may fail on old kernels without the relevant ioctl. */
    clockid_t clock_id = sys_posix_time_clock();
    if (ioctl(fd, EVIOCSCLOCKID, (int[]){clock_id}) != 0) {
        DLOG("%s: ioctl(EVIOCSCLOCKID) failed, event timestamps may be"
             " inaccurate: %s", path, strerror(errno));
        clock_id = CLOCK_REALTIME;
    }

    event_info[index].fd = fd;
    event_info[index].readonly = readonly;
    event_info[index].clock_id = clock_id;
    if (UNLIKELY(ioctl(fd, EVIOCGNAME(sizeof(event_info[index].name)),
                       event_info[index].name) < 0)) {
        DLOG("%s: ioctl(EVIOCGNAME): %s", path, strerror(errno));
        event_info[index].name[0] = '\0';
    }
    union {
        struct input_id id;
        uint64_t int64;
    } id;
    if (UNLIKELY(ioctl(fd, EVIOCGID, &id) < 0)) {
        DLOG("%s: ioctl(EVIOCGID): %s", path, strerror(errno));
        id.int64 = 0;
    }

    /* If we've seen this joystick before, assign it to the same slot it
     * was previously in.  Otherwise, pick the first slot with no
     * connected joystick (even if that slot was previously used for a
     * different joystick). */
    sys_mutex_lock(joysticks_mutex, -1);
    int joy_index;
    for (joy_index = 0; joy_index < num_joysticks; joy_index++) {
        if (joysticks[joy_index].event_dev < 0
         && joysticks[joy_index].id == id.int64) {
            DLOG("Joystick %d (%s: %s @ %04X:%04X:%04X:%04X) reconnected",
                 joy_index, path, event_info[index].name, id.id.bustype,
                 id.id.vendor, id.id.product, id.id.version);
            break;
        }
    }
    if (joy_index >= num_joysticks) {
        for (joy_index = 0; joy_index < num_joysticks; joy_index++) {
            if (joysticks[joy_index].event_dev < 0) {
                break;
            }
        }
        if (joy_index == num_joysticks) {
            ASSERT(num_joysticks < lenof(joysticks), joy_index--);
            num_joysticks = joy_index + 1;
        }
        DLOG("New joystick %d: %s: %s @ %04X:%04X:%04X:%04X",
             joy_index, path, event_info[index].name, id.id.bustype,
             id.id.vendor, id.id.product, id.id.version);
    }
    init_joystick(&joysticks[joy_index], index, id.int64);
    send_joystick_connect_event(joy_index, INPUT_JOYSTICK_CONNECTED);
    sys_mutex_unlock(joysticks_mutex);
}

/*-----------------------------------------------------------------------*/

int scan_joystick_thread(void *inotify_fd_)
{
    const int inotify_fd = (int)(intptr_t)inotify_fd_;

    for (;;) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(inotify_fd, &fds);
        FD_SET(scan_joystick_thread_stop_pipe[0], &fds);
        const int nfds =
            max(inotify_fd, scan_joystick_thread_stop_pipe[0]) + 1;
        select(nfds, &fds, NULL, NULL, NULL);
        if (FD_ISSET(scan_joystick_thread_stop_pipe[0], &fds)) {
            break;
        } else if (UNLIKELY(!FD_ISSET(inotify_fd, &fds))) {
            /* This should be impossible since we use no timeout and set
             * SA_RESTART on all our signal handlers, but Linux (at least
             * through version 3.17) still returns EINTR when the process
             * is stopped (SIGSTOP/SIGTSTP) and subsequently resumed
             * (SIGCONT) during a select() call.  See "man 7 signal" for
             * details. */
            continue;
        }

        /* inotify_event defines a name buffer with zero length, but the
         * actual event will be larger.  We can't determine exactly how
         * large the event is without reading it, so we just iterate over
         * all possible sizes until read() succeeds.  (There's
         * ioctl(FIONREAD), but that will count all pending events, which
         * could cause us to lose events if we used the result as the read
         * size.) */
        struct inotify_event event_buffer[100];
        STATIC_ASSERT(sizeof(event_buffer)
                          > sizeof(struct inotify_event) + NAME_MAX + 1,
                      "Event buffser size too small");
        struct inotify_event *event = event_buffer;
        int read_size = sizeof(struct inotify_event);
        while (errno = 0,
               read(inotify_fd, event_buffer, read_size) != read_size) {
            if (UNLIKELY(errno != EINVAL)) {
                if (errno == EINTR) {
                    /* As above, this is necessary because of a Linux
                     * idiosyncrasy that causes EINTR to be returned after
                     * process stop/continue, in this case while reading
                     * from an inotify descriptor. */
                    continue;
                } else {
                    DLOG("read(inotify_fd): %s", strerror(errno));
                    /* Terminate the thread so we don't spin endlessly
                     * on an unrecoverable error. */
                    goto out;
                }
            }
            read_size += sizeof(struct inotify_event);
            ASSERT(read_size <= (int)sizeof(event_buffer), goto out);
        }

        if (strncmp(event->name, "event", 5) != 0) {
            continue;
        }
        char *s = &event->name[5];
        unsigned long device = strtoul(s, &s, 10);
        if (*s || device >= lenof(event_info)) {
            continue;
        }

        if (event->mask & IN_DELETE) {
            if (event_info[device].fd >= 0) {
                sys_mutex_lock(joysticks_mutex, -1);
                for (int i = 0; i < lenof(joysticks); i++) {
                    if (joysticks[i].event_dev == (int)device) {
                        DLOG("Joystick %d disconnected", i);
                        joysticks[i].event_dev = -1;
                        send_joystick_connect_event(
                            i, INPUT_JOYSTICK_DISCONNECTED);
                        break;
                    }
                }
                sys_mutex_unlock(joysticks_mutex);
                close(event_info[device].fd);
                event_info[device].fd = -1;
            }
        } else {  // CREATE or ATTRIB
            if (event_info[device].fd < 0) {
                scan_joystick((int)device);
            }
        }
    }  // for (;;)

  out:
    close(inotify_fd);
    return 0;
}

/*-----------------------------------------------------------------------*/

static void init_joystick(JoystickInfo *joystick, int device, uint64_t id)
{
    PRECOND(device >= 0 && device < lenof(event_info), return);
    PRECOND(event_info[device].fd >= 0, return);

    /* Initialize everything to unset. */
    mem_clear(joystick, sizeof(*joystick));
    joystick->event_dev = device;
    joystick->id = id;
    for (int i = 0; i < lenof(joystick->button_map); i++) {
        joystick->button_map[i] = -1;
    }
    joystick->ff_effect.id = -1;
    joystick->num_buttons = 0;
    joystick->num_sticks = 0;
    for (int i = 0; i < lenof(joystick->ev_keymap_low); i++) {
        joystick->ev_keymap_low[i] = -1;
    }
    for (int i = 0; i < lenof(joystick->ev_keymap_high); i++) {
        joystick->ev_keymap_high[i] = -1;
    }
    joystick->dpad_up = -1;
    joystick->dpad_down = -1;
    joystick->dpad_left = -1;
    joystick->dpad_right = -1;
    joystick->dpad_x = -1;
    joystick->dpad_y = -1;
    joystick->dpad_state_up = 0;
    joystick->dpad_state_down = 0;
    joystick->dpad_state_left = 0;
    joystick->dpad_state_right = 0;
    for (int i = 0; i < lenof(joystick->stick_x); i++) {
        joystick->stick_x[i] = -1;
        joystick->stick_y[i] = -1;
        joystick->stick_state[i].x = 0;
        joystick->stick_state[i].y = 0;
        joystick->stick_timestamp[i] = 0;
    }
    joystick->l2_abs = -1;
    joystick->r2_abs = -1;
    joystick->l2_abs_state = 0;
    joystick->r2_abs_state = 0;

    /* Check whether the device supports force feedback (rumble).  If the
     * device was opened in read-only mode, we won't be able to send rumble
     * events to it, so we disable rumble even if the device supports it. */
    if (!event_info[device].readonly && BIT(event_info[device].ev, EV_FF)) {
        DECLARE_BITS(ff, FF_WAVEFORM_MAX+1);
        if (UNLIKELY(ioctl(event_info[device].fd,
                           EVIOCGBIT(EV_FF, sizeof(ff)), ff) < 0)) {
            DLOG("%s: EVIOCGBIT(EV_FF): %s", event_info[device].path,
                 strerror(errno));
            mem_clear(ff, sizeof(ff));
        }
        if (BIT(ff, FF_RUMBLE)) {
            joystick->can_rumble = 1;
            joystick->rumble_type = JOYSTICK_LINUX_RUMBLE_UNKNOWN;
        }
    }

    /* Read analog input parameters from the device.  To save time, we only
     * read data for inputs we might actually use. */
    for (int i = ABS_X; i <= ABS_RZ; i++) {
        if (BIT(event_info[device].abs, i)) {
            if (UNLIKELY(ioctl(event_info[device].fd, EVIOCGABS(i),
                               &joystick->absinfo[i]) < 0)) {
                DLOG("%s: EVIOCGABS(%d): %s", event_info[device].path, i,
                     strerror(errno));
            }
        }
    }
    for (int i = ABS_HAT0X; i <= ABS_HAT3X; i += 2) {
        if (BIT(event_info[device].abs,i) && BIT(event_info[device].abs,i+1)) {
            if (UNLIKELY(ioctl(event_info[device].fd, EVIOCGABS(i),
                               &joystick->absinfo[i]) < 0)) {
                DLOG("%s: EVIOCGABS(%d): %s", event_info[device].path, i,
                     strerror(errno));
            }
            if (UNLIKELY(ioctl(event_info[device].fd, EVIOCGABS(i+1),
                               &joystick->absinfo[i+1]) < 0)) {
                DLOG("%s: EVIOCGABS(%d): %s", event_info[device].path, i+1,
                     strerror(errno));
            }
            break;
        }
    }

    /* Set up initial assignments based on the data reported by evdev. */

    /* EV_KEY events in the BTN_JOYSTICK, BTN_GAMEPAD, and BTN_TRIGGER_HAPPY
     * ranges map to buttons.  BTN_GAMEPAD buttons are assigned before
     * BTN_JOYSTICK so that in case a device reports both kinds, we map the
     * named buttons to lower numbers (which are more user-friendly). */
    for (int i = BTN_GAMEPAD; i < BTN_GAMEPAD+16; i++) {
        if (BIT(event_info[device].key, i)) {
            const int button = joystick->num_buttons++;
            joystick->ev_keymap_low[i - BTN_JOYSTICK] = button;
            switch (i) {
              case BTN_SOUTH:
                joystick->button_map[INPUT_JOYBUTTON_FACE_DOWN] = button;
                break;
              case BTN_EAST:
                joystick->button_map[INPUT_JOYBUTTON_FACE_RIGHT] = button;
                break;
              case BTN_NORTH:
                joystick->button_map[INPUT_JOYBUTTON_FACE_UP] = button;
                break;
              case BTN_WEST:
                joystick->button_map[INPUT_JOYBUTTON_FACE_LEFT] = button;
                break;
              case BTN_TL:
                joystick->button_map[INPUT_JOYBUTTON_L1] = button;
                break;
              case BTN_TR:
                joystick->button_map[INPUT_JOYBUTTON_R1] = button;
                break;
              case BTN_TL2:
                joystick->button_map[INPUT_JOYBUTTON_L2] = button;
                break;
              case BTN_TR2:
                joystick->button_map[INPUT_JOYBUTTON_R2] = button;
                break;
              case BTN_SELECT:
                joystick->button_map[INPUT_JOYBUTTON_SELECT] = button;
                break;
              case BTN_START:
                joystick->button_map[INPUT_JOYBUTTON_START] = button;
                break;
              case BTN_MODE:
                joystick->button_map[INPUT_JOYBUTTON_HOME] = button;
                break;
              case BTN_THUMBL:
                joystick->button_map[INPUT_JOYBUTTON_L_STICK] = button;
                break;
              case BTN_THUMBR:
                joystick->button_map[INPUT_JOYBUTTON_R_STICK] = button;
                break;
            }
        }
    }
    for (int i = BTN_JOYSTICK; i < BTN_GAMEPAD; i++) {
        if (BIT(event_info[device].key, i)) {
            joystick->ev_keymap_low[i - BTN_JOYSTICK] = joystick->num_buttons++;
        }
    }
    for (int i = BTN_TRIGGER_HAPPY; i < BTN_TRIGGER_HAPPY + 40; i++) {
        if (BIT(event_info[device].key, i)) {
            joystick->ev_keymap_high[i - BTN_TRIGGER_HAPPY] =
                joystick->num_buttons++;
        }
    }

    /* BTN_DPAD_* key events are assigned to the D-pad (naturally).  We
     * assume that if any of the BTN_DPAD_* buttons are present, all of
     * them are. */
    if (BIT(event_info[device].key, BTN_DPAD_UP)) {
        joystick->dpad_up = BTN_DPAD_UP;
        joystick->dpad_down = BTN_DPAD_DOWN;
        joystick->dpad_left = BTN_DPAD_LEFT;
        joystick->dpad_right = BTN_DPAD_RIGHT;
    }

    /* If the device has any hats but no BTN_DPAD_* buttons, assign the
     * first hat's axes to the D-pad. */
    if (joystick->dpad_up < 0) {
        for (int i = 0; i < 4; i++) {
            if (BIT(event_info[device].abs, ABS_HAT0X + i*2)
             && BIT(event_info[device].abs, ABS_HAT0Y + i*2)) {
                joystick->dpad_x = ABS_HAT0X + i*2;
                joystick->dpad_y = ABS_HAT0Y + i*2;
                break;
            }
        }
    }

    /* Assign ABS_[XYZ] and ABS_R[XYZ] inputs to sticks, following the
     * same rules as for generic HID joysticks (see hidjoy_create() in
     * src/sysdep/misc/joystick-hid.c). */
    if (BIT(event_info[device].abs, ABS_Z)
     && BIT(event_info[device].abs, ABS_RX)
     && !BIT(event_info[device].abs, ABS_RY)
     && !BIT(event_info[device].abs, ABS_RZ)) {
        joystick->num_sticks = 2;
        joystick->stick_x[0] = ABS_X;
        joystick->stick_y[0] = ABS_Y;
        joystick->stick_x[1] = ABS_Z;
        joystick->stick_y[1] = ABS_RX;
    } else if (BIT(event_info[device].abs, ABS_Z)
            && BIT(event_info[device].abs, ABS_RZ)
            && !BIT(event_info[device].abs, ABS_RX)
            && !BIT(event_info[device].abs, ABS_RY)) {
        joystick->num_sticks = 2;
        joystick->stick_x[0] = ABS_X;
        joystick->stick_y[0] = ABS_Y;
        joystick->stick_x[1] = ABS_Z;
        joystick->stick_y[1] = ABS_RZ;
    } else {
        joystick->stick_x[0] = ABS_X;
        joystick->stick_y[0] = ABS_Y;
        joystick->stick_x[1] = ABS_RX;
        joystick->stick_y[1] = ABS_RY;
        joystick->stick_x[2] = ABS_Z;
        joystick->stick_y[2] = ABS_RZ;
        if (BIT(event_info[device].abs, ABS_X)
         || BIT(event_info[device].abs, ABS_Y)) {
            joystick->num_sticks = 1;
        }
        if (BIT(event_info[device].abs, ABS_RX)
         || BIT(event_info[device].abs, ABS_RY)) {
            joystick->num_sticks = 2;
        }
        if (BIT(event_info[device].abs, ABS_Z)
         || BIT(event_info[device].abs, ABS_RZ)) {
            joystick->num_sticks = 3;
        }
    }

    /* If we know about this device, update assignments accordingly. */
    const JoystickDesc *desc =
        joydb_lookup(joystick->id_struct.vendor, joystick->id_struct.product,
                     joystick->id_struct.version, event_info[device].name);
    if (desc) {
        joystick->rumble_type = desc->linux_rumble;
        for (int j = 0; j < INPUT_JOYBUTTON__NUM; j++) {
            joystick->button_map[j] = desc->button_map[j];
        }
        switch (desc->dpad_type) {
          case JOYSTICK_DPAD_NATIVE:
            /* If the device had native D-pad buttons, we already mapped
             * them to the D-pad. */
            break;
          case JOYSTICK_DPAD_HAT:
            joystick->dpad_x = ABS_HAT0X;
            joystick->dpad_y = ABS_HAT0Y;
            joystick->dpad_up = -1;
            joystick->dpad_down = -1;
            joystick->dpad_left = -1;
            joystick->dpad_right = -1;
            break;
          case JOYSTICK_DPAD_BUTTONS:
            joystick->dpad_x = -1;
            joystick->dpad_y = -1;
            joystick->dpad_up =
                joystick_button_to_key(joystick, desc->dpad_up);
            joystick->dpad_down =
                joystick_button_to_key(joystick, desc->dpad_down);
            joystick->dpad_left =
                joystick_button_to_key(joystick, desc->dpad_left);
            joystick->dpad_right =
                joystick_button_to_key(joystick, desc->dpad_right);
            break;
          default:
            break;
        }
        joystick->num_sticks = 0;
        if (desc->lstick_x != JOYSTICK_VALUE_NONE) {
            joystick->num_sticks = 1;
            joystick->stick_x[0] =
                joystick_db_value_index_to_abs(desc->lstick_x);
            joystick->stick_y[0] =
                joystick_db_value_index_to_abs(desc->lstick_y);
        }
        if (desc->rstick_x != JOYSTICK_VALUE_NONE) {
            joystick->num_sticks = 2;
            joystick->stick_x[1] =
                joystick_db_value_index_to_abs(desc->rstick_x);
            joystick->stick_y[1] =
                joystick_db_value_index_to_abs(desc->rstick_y);
        }
        if (desc->l2_value != JOYSTICK_VALUE_NONE) {
            joystick->l2_abs = joystick_db_value_index_to_abs(desc->l2_value);
            joystick->button_map[INPUT_JOYBUTTON_L2] = joystick->num_buttons++;
        }
        if (desc->r2_value != JOYSTICK_VALUE_NONE) {
            joystick->r2_abs = joystick_db_value_index_to_abs(desc->r2_value);
            joystick->button_map[INPUT_JOYBUTTON_R2] = joystick->num_buttons++;
        }
    }
}

/*-----------------------------------------------------------------------*/

static int joystick_db_value_index_to_abs(JoystickValueInput index)
{
    switch (index) {
        case JOYSTICK_VALUE_NONE: ASSERT(!"Invalid parameter"); return 0;  // NOTREACHED
        case JOYSTICK_VALUE_X:    return ABS_X;
        case JOYSTICK_VALUE_Y:    return ABS_Y;
        case JOYSTICK_VALUE_Z:    return ABS_Z;
        case JOYSTICK_VALUE_RX:   return ABS_RX;
        case JOYSTICK_VALUE_RY:   return ABS_RY;
        case JOYSTICK_VALUE_RZ:   return ABS_RZ;
        case JOYSTICK_VALUE_HAT:  ASSERT(!"Invalid parameter"); return 0;  // NOTREACHED
    }
    ASSERT(!"impossible", return 0);  // NOTREACHED
}

/*-----------------------------------------------------------------------*/

static int joystick_button_to_key(const JoystickInfo *joystick, int button)
{
    for (int i = 0; i < lenof(joystick->ev_keymap_low); i++) {
        if (joystick->ev_keymap_low[i] == button) {
            return i + BTN_JOYSTICK;
        }
    }
    for (int i = 0; i < lenof(joystick->ev_keymap_high); i++) {
        if (joystick->ev_keymap_high[i] == button) {
            return i + BTN_TRIGGER_HAPPY;
        }
    }
    return -1;
}

/*-----------------------------------------------------------------------*/

static void handle_joystick_abs_event(int device, double timestamp,
                                      int input, int raw_value)
{
    JoystickInfo *joystick = &joysticks[device];

    if (input >= lenof(joystick->absinfo)) {
        return;
    }
    const float value =
        normalize_joystick_axis(raw_value, &joystick->absinfo[input]);

    if (input == joystick->dpad_x) {
        joystick->dpad_state_left = (value < 0);
        joystick->dpad_state_right = (value > 0);
        send_joystick_dpad_event(timestamp, device);
    } else if (input == joystick->dpad_y) {
        joystick->dpad_state_up = (value < 0);
        joystick->dpad_state_down = (value > 0);
        send_joystick_dpad_event(timestamp, device);
    } else if (input == joystick->l2_abs) {
        /* Debounce the input by 1/16 on either side of the center point,
         * since at least some devices (like the Xbox 360 controller)
         * report no "flat" value for the input. */
        int state;
        if (joystick->l2_abs_state) {
            state = (value >= -0.0625f);
        } else {
            state = (value >= 0.0625f);
        }
        if (state != joystick->l2_abs_state) {
            joystick->l2_abs_state = state;
            send_joystick_button_event(
                timestamp, device,
                joystick->button_map[INPUT_JOYBUTTON_L2], state);
        }
    } else if (input == joystick->r2_abs) {
        int state;
        if (joystick->r2_abs_state) {
            state = (value >= -0.0625f);
        } else {
            state = (value >= 0.0625f);
        }
        if (state != joystick->r2_abs_state) {
            joystick->r2_abs_state = state;
            send_joystick_button_event(
                timestamp, device,
                joystick->button_map[INPUT_JOYBUTTON_R2], state);
        }
    } else {
        for (int stick = 0; stick < lenof(joystick->stick_x); stick++) {
            if (input == joystick->stick_x[stick]) {
                if (value != joystick->stick_state[stick].x) {
                    update_stick(device, timestamp, stick, 0, value);
                }
                break;
            } else if (input == joystick->stick_y[stick]) {
                if (value != joystick->stick_state[stick].y) {
                    update_stick(device, timestamp, stick, 1, value);
                }
                break;
            }
        }
    }
}

/*-----------------------------------------------------------------------*/

static void handle_joystick_key_event(int device, double timestamp,
                                      int input, int state)
{
    JoystickInfo *joystick = &joysticks[device];

    int button;
    if (input >= BTN_JOYSTICK
     && input < BTN_JOYSTICK + lenof(joystick->ev_keymap_low)) {
        button = joystick->ev_keymap_low[input - BTN_JOYSTICK];
    } else if (input >= BTN_TRIGGER_HAPPY
            && input < BTN_TRIGGER_HAPPY + lenof(joystick->ev_keymap_high)) {
        button = joystick->ev_keymap_high[input - BTN_TRIGGER_HAPPY];
    } else {
        button = -1;
    }
    if (button != -1) {
        send_joystick_button_event(timestamp, device, button, state);
    }

    if (input == joystick->dpad_up) {
        joystick->dpad_state_up = state;
        send_joystick_dpad_event(timestamp, device);
    } else if (input == joystick->dpad_down) {
        joystick->dpad_state_down = state;
        send_joystick_dpad_event(timestamp, device);
    } else if (input == joystick->dpad_left) {
        joystick->dpad_state_left = state;
        send_joystick_dpad_event(timestamp, device);
    } else if (input == joystick->dpad_right) {
        joystick->dpad_state_right = state;
        send_joystick_dpad_event(timestamp, device);
    }
}

/*-----------------------------------------------------------------------*/

static void update_stick(int device, double timestamp, int stick, int is_y,
                         float value)
{
    JoystickInfo *joystick = &joysticks[device];

    if (joystick->stick_timestamp[stick]
     && joystick->stick_timestamp[stick] != timestamp) {
        send_joystick_stick_event(joystick->stick_timestamp[stick], device,
                                  stick, &joystick->stick_state[stick]);
    }
    if (is_y) {
        joystick->stick_state[stick].y = value;
    } else {
        joystick->stick_state[stick].x = value;
    }
    joystick->stick_timestamp[stick] = timestamp;
}

/*-----------------------------------------------------------------------*/

static void flush_joystick_events(int device)
{
    JoystickInfo *joystick = &joysticks[device];

    for (int stick = 0; stick < lenof(joystick->stick_state); stick++) {
        if (joystick->stick_timestamp[stick]) {
            send_joystick_stick_event(joystick->stick_timestamp[stick], device,
                                      stick, &joystick->stick_state[stick]);
            joystick->stick_timestamp[stick] = 0;
        }
    }
}

/*************************************************************************/
/****************** Local routines: X11 event handlers *******************/
/*************************************************************************/

static void handle_key_event(const XKeyEvent *event)
{
    /* X11 implements key repeat by sending a synthetic KeyRelease
     * followed by KeyPress for each repeat event.  The Xkb library in
     * the X.org server provides XkbSetDetectableAutoRepeat() to suppress
     * the synthetic KeyRelease events, but whether the function actually
     * does anything ultimately depends on server-side support, so to be
     * safe, we don't rely on it and just check directly for a queued
     * KeyPress event. */
    int is_repeat = 0;
    XEvent next_event;
    if (event->type == KeyRelease) {
        /* Ideally this should be QueuedAlready rather than QueuedAfterReading
         * since we expect the repeat KeyPress to be sent along with the
         * synthetic KeyRelease, but it's possible that the previous receive
         * operation stopped between the KeyRelease and KeyPress events. */
        if (XEventsQueued(linux_x11_display(), QueuedAfterReading) > 0) {
            XPeekEvent(linux_x11_display(), &next_event);
            if (next_event.type == KeyPress
             && next_event.xkey.time == event->time
             && next_event.xkey.keycode == event->keycode) {
                is_repeat = 1;
                /* Replace the original KeyRelease event with the new
                 * KeyPress event, and consume it from the queue. */
                event = &next_event.xkey;
                XEvent discard;
                XNextEvent(linux_x11_display(), &discard);
                /* Verify that the event we discarded is in fact the same
                 * one that we peeked at above.  If this fails, it probably
                 * indicates a bug in Xlib.  Note that we can't just call
                 * memcmp() because some bytes in the struct may be
                 * undefined due to padding.  These will all be optimized
                 * out if !DEBUG, so there's no performance impact. */
                ASSERT(discard.xkey.type == event->type);
                ASSERT(discard.xkey.serial == event->serial);
                ASSERT(discard.xkey.send_event == event->send_event);
                ASSERT(discard.xkey.display == event->display);
                ASSERT(discard.xkey.window == event->window);
                ASSERT(discard.xkey.root == event->root);
                ASSERT(discard.xkey.subwindow == event->subwindow);
                ASSERT(discard.xkey.time == event->time);
                ASSERT(discard.xkey.x == event->x);
                ASSERT(discard.xkey.y == event->y);
                ASSERT(discard.xkey.x_root == event->x_root);
                ASSERT(discard.xkey.y_root == event->y_root);
                ASSERT(discard.xkey.state == event->state);
                ASSERT(discard.xkey.keycode == event->keycode);
                ASSERT(discard.xkey.same_screen == event->same_screen);
                /* If the previous KeyPress event wasn't filtered, this one
                 * shouldn't be filtered either, but we need to pass it
                 * along to the input context anyway so the IC's state
                 * remains consistent. */
                ASSERT(!XFilterEvent(&discard, None));
            }
        }
    }

    const double timestamp = convert_x11_timestamp(event->time);
    const int is_press = (event->type == KeyPress);
    const int keycode = event->keycode;

    /*
     * Normally we ask X11 to convert the keycode to a keysym, but we map
     * the typewriter number keys directly to numbers regardless of
     * keyboard mapping.  This is to account for the strange case of the
     * AZERTY layout, for which the numbers are normally accessed with the
     * Shift key (having various letters or punctuation as the base keysym)
     * but users seem to view them as number keys, to the extent that most
     * games display them as numbers in keyboard configuration UI.
     * See also: https://bugzilla.libsdl.org/show_bug.cgi?id=3188#c10
     */
    KeySym keysym;
    if (keycode >= 10 && keycode <= 18) {
        keysym = XK_1 + (keycode - 10);
    } else if (keycode == 19) {
        keysym = XK_0;
    } else {
        keysym = XkbKeycodeToKeysym(linux_x11_display(), keycode, 0, 0);
    }
    const int key = keysym ? convert_x11_keysym(keysym) : 0;

    if (key) {
        /*
         * Some broken(?) X servers seem to send KeyRelease events
         * immediately after the corresponding KeyPress for keys
         * with modifiers pressed, like Ctrl+A, rather than sending
         * KeyRelease followed by KeyPress at each repeat interval.
         * We can't reliably recover the physical key state from
         * these bogus events, but we can at least make sure that
         * the keypress isn't lost completely by delaying the
         * release event until the next sys_input_update() call.
         */
        if (!is_press && newkeys[key]) {
            key_release[key] = keycode;
            return;
        }
        key_release[key] = 0;
        /* Avoid sending KEY_UP for an unpressed key.  This should
         * normally never happen, but we could encounter it after
         * linux_clear_window_input_state() if the key remains pressed
         * through the window reconfiguration.  We let KEY_DOWN through
         * for pressed keys since that could just be a result of key repeat. */
        if (!is_press && !keystate[key]) {
            return;
        }
        keystate[key] = is_press ? keycode : 0;
        newkeys[key] = is_press;
        (*event_callback)(&(InputEvent){
            .type = INPUT_EVENT_KEYBOARD,
            .detail = is_press ? INPUT_KEYBOARD_KEY_DOWN
                               : INPUT_KEYBOARD_KEY_UP,
            .timestamp = timestamp,
            {.keyboard = {.key = key, .system_key = keycode,
                          .is_repeat = is_repeat}}});
    } else {
        (*event_callback)(&(InputEvent){
            .type = INPUT_EVENT_KEYBOARD,
            .detail = is_press ? INPUT_KEYBOARD_SYSTEM_KEY_DOWN
                               : INPUT_KEYBOARD_SYSTEM_KEY_UP,
            .timestamp = timestamp,
            {.keyboard = {.key = KEY__NONE, .system_key = keycode,
                          .is_repeat = is_repeat}}});
    }

    if (is_press && text_active) {
        InputEvent text_event = {
            .type = INPUT_EVENT_TEXT, .detail = 0, .timestamp = timestamp};
        if (key == KEY_BACKSPACE) {
            text_event.detail = INPUT_TEXT_BACKSPACE;
            (*event_callback)(&text_event);
        } else if (key == KEY_DELETE) {
            text_event.detail = INPUT_TEXT_DELETE;
            (*event_callback)(&text_event);
        } else if (key == KEY_LEFT) {
            text_event.detail = INPUT_TEXT_CURSOR_LEFT;
            (*event_callback)(&text_event);
        } else if (key == KEY_RIGHT) {
            text_event.detail = INPUT_TEXT_CURSOR_RIGHT;
            (*event_callback)(&text_event);
        } else if (key == KEY_HOME) {
            text_event.detail = INPUT_TEXT_CURSOR_HOME;
            (*event_callback)(&text_event);
        } else if (key == KEY_END) {
            text_event.detail = INPUT_TEXT_CURSOR_END;
            (*event_callback)(&text_event);
        } else if (key == KEY_ESCAPE) {
            text_event.detail = INPUT_TEXT_CANCELLED;
            (*event_callback)(&text_event);
        } else if (key == KEY_ENTER || key == KEY_NUMPAD_ENTER) {
            text_event.detail = INPUT_TEXT_DONE;
            (*event_callback)(&text_event);
        } else {
            char *text;
            int textlen;
            char buf[1000];

            XIC ic = linux_x11_ic();
            if (ic) {
                KeySym unused_keysym;
                Status status;
                textlen = Xutf8LookupString(
                    ic, (XKeyPressedEvent *)event, buf, sizeof(buf)-1,
                    &unused_keysym, &status);
                if (status == XBufferOverflow) {
                    text = mem_alloc(textlen+1, 1, MEM_ALLOC_TEMP);
                    if (UNLIKELY(!text)) {
                        DLOG("No memory for composed text (%d bytes),"
                             " discarding event", textlen);
                    } else {
                        textlen = Xutf8LookupString(
                            ic, (XKeyPressedEvent *)event, text, textlen,
                            &unused_keysym, &status);
                        ASSERT(status != XBufferOverflow, textlen = 0);
                    }
                } else if (status == XLookupChars || status == XLookupBoth) {
                    text = buf;
                } else {
                    text = NULL;
                }
            } else {
                KeySym unused_keysym;
                textlen = XLookupString((XKeyEvent *)event, buf, sizeof(buf)-1,
                                        &unused_keysym, NULL);
                ASSERT(textlen <= (int)sizeof(buf)-1, textlen = sizeof(buf)-1);
                text = buf;
            }

            if (text) {
                text[textlen] = '\0';
                text_event.detail = INPUT_TEXT_INPUT;
                const char *s = text;
                while ((text_event.text.ch = utf8_read(&s)) != 0) {
                    if (UNLIKELY(text_event.text.ch < 0)) {
                        DLOG("Invalid UTF-8 in X11 input string");
                        continue;
                    }
                    (*event_callback)(&text_event);
                }
                if (text != buf) {
                    mem_free(text);
                }
            }
        }
    }
}

/*-----------------------------------------------------------------------*/

static void handle_button_event(const XButtonEvent *event)
{
    const int is_press = (event->type == ButtonPress);
    InputEventDetail detail = 0;
    int scroll = 0;

    switch (event->button) {
      case Button1:
        if (is_press == mouse_lbutton) {
            return;  // Avoid redundant events (as for KeyRelease).
        }
        mouse_lbutton = is_press;
        detail = is_press ? INPUT_MOUSE_LMB_DOWN : INPUT_MOUSE_LMB_UP;
        break;
      case Button2:
        if (is_press == mouse_mbutton) {
            return;
        }
        mouse_mbutton = is_press;
        detail = is_press ? INPUT_MOUSE_MMB_DOWN : INPUT_MOUSE_MMB_UP;
        break;
      case Button3:
        if (is_press == mouse_rbutton) {
            return;
        }
        mouse_rbutton = is_press;
        detail = is_press ? INPUT_MOUSE_RMB_DOWN : INPUT_MOUSE_RMB_UP;
        break;
      case Button4:
        if (is_press) {
            detail = INPUT_MOUSE_SCROLL_V;
            scroll = -1;
        }
        break;
      case Button5:
        if (is_press) {
            detail = INPUT_MOUSE_SCROLL_V;
            scroll = 1;
        }
        break;
      case Button6:
        if (is_press) {
            detail = INPUT_MOUSE_SCROLL_H;
            scroll = -1;
        }
        break;
      case Button7:
        if (is_press) {
            detail = INPUT_MOUSE_SCROLL_H;
            scroll = 1;
        }
        break;
    }

    if (detail) {
        InputEvent sil_event = {
            .type = INPUT_EVENT_MOUSE, .detail = detail,
            .timestamp = convert_x11_timestamp(event->time)};
        convert_mouse_coords(event->x, event->y,
                             &sil_event.mouse.x, &sil_event.mouse.y);
        sil_event.mouse.scroll = scroll;
        (*event_callback)(&sil_event);
    }
}

/*-----------------------------------------------------------------------*/

static void handle_motion_event(const XMotionEvent *event)
{
    const double timestamp = convert_x11_timestamp(event->time);
    if (mouse_position_overridden
     && timestamp >= mouse_position_override_timeout) {
        DLOG("Cancelling mouse position override due to timeout");
        mouse_position_overridden = 0;
    }
    if (mouse_position_overridden) {
        mouse_position_overridden =
            (event->x != mouse_position_override_x ||
             event->y != mouse_position_override_y);
    } else {
        InputEvent sil_event = {
            .type = INPUT_EVENT_MOUSE, .detail = INPUT_MOUSE_MOVE,
            .timestamp = timestamp};
        convert_mouse_coords(event->x, event->y, &mouse_x, &mouse_y);
        sil_event.mouse.x = mouse_x;
        sil_event.mouse.y = mouse_y;
        (*event_callback)(&sil_event);
    }
}

/*-----------------------------------------------------------------------*/

static void handle_enter_window_event(const XEnterWindowEvent *event)
{
    /* Treat it like an ordinary pointer motion event. */
    XMotionEvent motion_event = {
        .type = MotionNotify,
        .serial = event->serial,
        .send_event = event->send_event,
        .display = event->display,
        .window = event->window,
        .root = event->root,
        .subwindow = event->subwindow,
        .time = event->time,
        .x = event->x,
        .y = event->y,
        .x_root = event->root,
        .y_root = event->y_root,
        .state = 0,  // May not be correct, but we don't use this field.
        .is_hint = NotifyNormal,
        .same_screen = True,
    };
    handle_motion_event(&motion_event);
}

/*-----------------------------------------------------------------------*/

static void handle_touch_event(const XIDeviceEvent *event)
{
    InputEvent sil_event = {
        .type = INPUT_EVENT_TOUCH,
        .timestamp = convert_x11_timestamp(event->time),
        {.touch = {.id = event->detail}}};
    if (event->evtype == XI_TouchBegin) {
        sil_event.detail = INPUT_TOUCH_DOWN;
    } else if (event->evtype == XI_TouchUpdate) {
        sil_event.detail = INPUT_TOUCH_MOVE;
    } else {
        ASSERT(event->evtype == XI_TouchEnd, return);
        sil_event.detail = INPUT_TOUCH_UP;
    }
    convert_mouse_coords(event->event_x, event->event_y,
                         &sil_event.touch.x, &sil_event.touch.y);
    (*event_callback)(&sil_event);
}

/*-----------------------------------------------------------------------*/

static void handle_xinput2_event(const XGenericEventCookie *event)
{
    const XIDeviceEvent *device_event = (const XIDeviceEvent *)event->data;

    switch (event->evtype) {
      case XI_Motion:
        if (device_event->flags & XIPointerEmulated) {
            /* Ignore emulated pointer events from the touchscreen.
             * Theoretically XInput2 is supposed to suppress these on its
             * own when we request touch events, but that doesn't seem to
             * happen, so... */
            break;
        }
        handle_motion_event(&(const XMotionEvent){
            .type = MotionNotify,
            .serial = event->serial,
            .send_event = event->send_event,
            .display = event->display,
            .window = device_event->event,
            .root = device_event->root,
            .subwindow = device_event->child,
            .time = device_event->time,
            .x = device_event->event_x,
            .y = device_event->event_y,
            .x_root = device_event->root_x,
            .y_root = device_event->root_y,
            .state = 0,  // We don't use this field.
            .is_hint = NotifyNormal,
            .same_screen = True,
            });
        break;

      case XI_ButtonPress:
      case XI_ButtonRelease:
        handle_button_event(&(const XButtonEvent){
            .type = event->evtype==XI_ButtonPress ? ButtonPress : ButtonRelease,
            .serial = event->serial,
            .send_event = event->send_event,
            .display = event->display,
            .window = device_event->event,
            .root = device_event->root,
            .subwindow = device_event->child,
            .time = device_event->time,
            .x = device_event->event_x,
            .y = device_event->event_y,
            .x_root = device_event->root_x,
            .y_root = device_event->root_y,
            .state = 0,  // We don't use this field.
            .button = device_event->detail,
            .same_screen = True,
            });
        break;

      case XI_TouchBegin:
      case XI_TouchUpdate:
      case XI_TouchEnd:
        handle_touch_event(device_event);
        break;
    }
}

/*************************************************************************/
/**************** Local routines: Miscellaneous utilities ****************/
/*************************************************************************/

static double convert_x11_timestamp(uint32_t timestamp)
{
    /* X11 events should normally have a proper timestamp, but some
     * generated events may have a timestamp of zero instead (see e.g.
     * https://www.libreoffice.org/bugzilla/show_bug.cgi?id=39367). */
    if (timestamp == 0) {
        return time_now();
    }

    if (last_x11_timestamp == 0) {
        x11_timestamp_epoch = time_now() - (timestamp / 1000.0);
    } else if (timestamp < last_x11_timestamp) {
        x11_timestamp_epoch += 4294967.296;
    }
    last_x11_timestamp = timestamp;
    return x11_timestamp_epoch + (timestamp / 1000.0);
}

/*-----------------------------------------------------------------------*/

static int convert_x11_keysym(KeySym keysym)
{
    int low = 0, high = lenof(keysym_map) - 1;
    while (low <= high) {
        const int mid = (low + high) / 2;
        if (keysym == keysym_map[mid].keysym) {
            return keysym_map[mid].sil_keycode;
        } else if (keysym < keysym_map[mid].keysym) {
            high = mid-1;
        } else {
            low = mid+1;
        }
    }
    DLOG("Unrecognized keysym 0x%X", (int)keysym);
    return KEY__NONE;
}

/*-----------------------------------------------------------------------*/

static void convert_mouse_coords(int x_in, int y_in,
                                 float *x_out, float *y_out)
{
    if (mouse_position_overridden) {
        x_in = mouse_position_override_x;
        y_in = mouse_position_override_y;
    }

    int width = linux_x11_window_width();
    int height = linux_x11_window_height();
    ASSERT(width > 0, width = 1); // We only receive events with a window open.
    ASSERT(height > 0, height = 1);
    *x_out = (float)bound(x_in, 0, width-1)  / (float)width;
    *y_out = (float)bound(y_in, 0, height-1) / (float)height;
}

/*-----------------------------------------------------------------------*/

static float normalize_joystick_axis(int raw,
                                     const struct input_absinfo *absinfo)
{
    if (absinfo->minimum >= absinfo->maximum) {
        return 0;  // Not initialized or invalid.
    }
    const float minimum = absinfo->minimum;
    const float maximum = absinfo->maximum;
    const float midpoint = (minimum + maximum) * 0.5f;
    const float flat = absinfo->flat;
    if (raw < midpoint - flat) {
        return (raw - (midpoint - flat)) / ((midpoint - flat) - minimum);
    } else if (raw > midpoint + flat) {
        return (raw - (midpoint + flat)) / (maximum - (midpoint + flat));
    } else {
        return 0;
    }
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

/*-----------------------------------------------------------------------*/

static void send_joystick_button_event(double timestamp, int device,
                                       int button, int value)
{
    if (!ignore_focus_for_joysticks && !sys_graphics_has_focus()) {
        return;
    }
    (*event_callback)(&(InputEvent){
        .type = INPUT_EVENT_JOYSTICK,
        .detail = value ? INPUT_JOYSTICK_BUTTON_DOWN : INPUT_JOYSTICK_BUTTON_UP,
        .timestamp = timestamp,
        {.joystick = {.device = device, .index = button}}});
}

/*-----------------------------------------------------------------------*/

static void send_joystick_dpad_event(double timestamp, int device)
{
    if (!ignore_focus_for_joysticks && !sys_graphics_has_focus()) {
        return;
    }
    JoystickInfo *joystick = &joysticks[device];
    const int x = joystick->dpad_state_right - joystick->dpad_state_left;
    const int y = joystick->dpad_state_down - joystick->dpad_state_up;
    (*event_callback)(&(InputEvent){
        .type = INPUT_EVENT_JOYSTICK,
        .detail = INPUT_JOYSTICK_DPAD_CHANGE,
        .timestamp = timestamp,
        {.joystick = {.device = device, .x = x, .y = y}}});
}

/*-----------------------------------------------------------------------*/

static void send_joystick_stick_event(double timestamp, int device, int stick,
                                      const Vector2f *value)
{
    if (!ignore_focus_for_joysticks && !sys_graphics_has_focus()) {
        return;
    }
    (*event_callback)(&(InputEvent){
        .type = INPUT_EVENT_JOYSTICK,
        .detail = INPUT_JOYSTICK_STICK_CHANGE,
        .timestamp = timestamp,
        {.joystick = {.device = device, .index = stick,
                      .x = value->x, .y = value->y}}});
}

/*************************************************************************/
/*************************************************************************/
