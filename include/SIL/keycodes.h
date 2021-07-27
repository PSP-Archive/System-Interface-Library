/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/keycodes.h: Platform-agnostic key IDs for keyboard handling.
 */

#ifndef SIL_KEYCODES_H
#define SIL_KEYCODES_H

EXTERN_C_BEGIN

/*************************************************************************/
/*************************************************************************/

/**
 * KEY_*:  Key codes for passing to input_key_state().  Note that not all
 * keys are supported on all keyboards -- for example, KEY_ATSIGN "@" is a
 * physical key on Japanese keyboards, but not on US keyboards (where it is
 * typed using Shift+2).
 *
 * Zero is guaranteed not to be a valid key code.
 */
enum {
    /* Fencepost value smaller than any valid key. */
    KEY__NONE = 0,

    /* Basic alphanumeric keys. */
    KEY_A,
    KEY_B,
    KEY_C,
    KEY_D,
    KEY_E,
    KEY_F,
    KEY_G,
    KEY_H,
    KEY_I,
    KEY_J,
    KEY_K,
    KEY_L,
    KEY_M,
    KEY_N,
    KEY_O,
    KEY_P,
    KEY_Q,
    KEY_R,
    KEY_S,
    KEY_T,
    KEY_U,
    KEY_V,
    KEY_W,
    KEY_X,
    KEY_Y,
    KEY_Z,
    KEY_0,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,

    /* Punctuation and other symbols. */
    KEY_TAB,
    KEY_ENTER,
    KEY_SPACE,
    KEY_EXCLAMATION,
    KEY_QUOTE,
    KEY_SHARP,
    KEY_DOLLAR,
    KEY_PERCENT,
    KEY_AMPERSAND,
    KEY_APOSTROPHE,
    KEY_LEFTPAREN,
    KEY_RIGHTPAREN,
    KEY_ASTERISK,
    KEY_PLUS,
    KEY_COMMA,
    KEY_HYPHEN,
    KEY_PERIOD,
    KEY_SLASH,
    KEY_COLON,
    KEY_SEMICOLON,
    KEY_LESS,
    KEY_EQUALS,
    KEY_GREATER,
    KEY_QUESTION,
    KEY_ATSIGN,
    KEY_LEFTBRACKET,
    KEY_BACKSLASH,
    KEY_RIGHTBRACKET,
    KEY_CIRCUMFLEX,
    KEY_UNDERSCORE,
    KEY_BACKQUOTE,
    KEY_LEFTBRACE,
    KEY_PIPE,
    KEY_RIGHTBRACE,
    KEY_TILDE,
    KEY_POUND,  // The currency symbol (£), not the sharp sign
    KEY_YEN,
    KEY_EURO,

    /* Shift-type keys. */
    KEY_LEFTSHIFT,
    KEY_RIGHTSHIFT,
    KEY_LEFTCONTROL,
    KEY_RIGHTCONTROL,
    KEY_LEFTALT,
    KEY_RIGHTALT,
    KEY_LEFTMETA,
    KEY_RIGHTMETA,
    KEY_LEFTSUPER,
    KEY_RIGHTSUPER,
    KEY_NUMLOCK,
    KEY_CAPSLOCK,
    KEY_SCROLLLOCK,
    /* Specific to Japanese 106-key layout: */
    KEY_KANJI,
    KEY_KANA,
    KEY_HENKAN,
    KEY_MUHENKAN,

    /* Editing and cursor movement keys. */
    KEY_BACKSPACE,
    KEY_UNDO,
    KEY_INSERT,
    KEY_DELETE,
    KEY_HOME,
    KEY_END,
    KEY_PAGEUP,
    KEY_PAGEDOWN,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,

    /* Function keys. */
    KEY_ESCAPE,
    KEY_F1,
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_F8,
    KEY_F9,
    KEY_F10,
    KEY_F11,
    KEY_F12,
    KEY_F13,
    KEY_F14,
    KEY_F15,
    KEY_F16,
    KEY_F17,
    KEY_F18,
    KEY_F19,
    KEY_F20,
    KEY_PRINTSCREEN,
    KEY_PAUSE,
    KEY_MENU,

    /* Number pad keys. */
    KEY_NUMPAD_0,
    KEY_NUMPAD_1,
    KEY_NUMPAD_2,
    KEY_NUMPAD_3,
    KEY_NUMPAD_4,
    KEY_NUMPAD_5,
    KEY_NUMPAD_6,
    KEY_NUMPAD_7,
    KEY_NUMPAD_8,
    KEY_NUMPAD_9,
    KEY_NUMPAD_DECIMAL,
    KEY_NUMPAD_DIVIDE,
    KEY_NUMPAD_MULTIPLY,
    KEY_NUMPAD_SUBTRACT,
    KEY_NUMPAD_ADD,
    KEY_NUMPAD_EQUALS,
    KEY_NUMPAD_ENTER,

    /* System-specific keys. */
    KEY_ANDROID_BACK,

    /* Fencepost value larger than any valid key. */
    KEY__LAST
};

/*-----------------------------------------------------------------------*/

/**
 * KEYMOD_*:  Flags returned from the input_key_modifier_state() function
 * or in the "modifiers" field of a keyboard event.
 */
#define KEYMOD_SHIFT       (1 << 0)
#define KEYMOD_CONTROL     (1 << 1)
#define KEYMOD_ALT         (1 << 2)  // Also the Option key on Mac keyboards.
#define KEYMOD_META        (1 << 3)  // The Command key on Mac keyboards.
#define KEYMOD_SUPER       (1 << 4)
#define KEYMOD_CAPSLOCK    (1 << 5)
#define KEYMOD_NUMLOCK     (1 << 6)
#define KEYMOD_SCROLLLOCK  (1 << 7)

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_INPUT_KEYS_H
