/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/input.c: PSP input device interface.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/input.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/time.h"
#include "src/sysdep/psp/internal.h"
#include "src/sysdep/psp/sound-low.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Analog pad input threshold (distance from the central value of 128). */
#define ANALOG_THRESHOLD  32

/* Maximum number of characters to accept for on-screen keyboard input. */
#define OSK_MAXLEN  1000

/*-----------------------------------------------------------------------*/

/* Event callback passed to sys_input_init(). */
static InputEventCallback event_callback;

/* Joystick info for sys_input_info(). */
static const SysInputJoystick joystick_info = {
    .connected   = 1,
    .can_rumble  = 0,
    .num_buttons = 16,
    .num_sticks  = 1,
};

/* Current button state (1 = pressed). */
static uint8_t buttons[16];

/* Current analog pad state. */
static float pad_x, pad_y;

/* Parameter blocks for the on-screen keyboard (OSK) system utility. */
static SceUtilityOskParams osk_params;
static SceUtilityOskData osk_data;

/* Is the OSK active? */
static uint8_t osk_active;
/* Is the OSK currently shutting down? (only valid if osk_active is true) */
static uint8_t osk_shutting_down;

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * send_text_event:  Generate a text input event and send it to the
 * event callback.
 *
 * [Parameters]
 *     detail: Event detail code (INPUT_TEXT_*).
 *     ch: Unicode codepoint for INPUT_TEXT_INPUT events.
 */
static void send_text_event(InputEventDetail detail, int32_t ch);

/**
 * finish_osk:  Perform cleanup and data processing for a completed OSK
 * dialog.
 */
static void finish_osk(void);

/**
 * utf8to16:  Convert a string from 8-bit to 16-bit Unicode format.  The
 * returned string is stored in a newly mem_alloc()ed buffer.
 *
 * These functions only support Unicode characters with codepoints in the
 * range 0-65535 (16-bit characters).  Support for UTF-16 surrogates is
 * not implemented.
 *
 * [Parameters]
 *     str: String to convert.
 * [Return value]
 *     Converted string, or NULL on error.
 */
static uint16_t *utf8to16(const char *str);

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int sys_input_init(void (*event_callback_)(const struct InputEvent *))
{
    PRECOND(event_callback_ != NULL, return 0);

    event_callback = event_callback_;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    (*event_callback)(&(InputEvent){
        .type = INPUT_EVENT_JOYSTICK, .detail = INPUT_JOYSTICK_CONNECTED,
        .timestamp = time_now(), {.joystick = {.device = 0}}});

    mem_clear(&osk_params, sizeof(osk_params));
    mem_clear(&osk_data, sizeof(osk_data));
    osk_params.base.size = sizeof(osk_params);
    osk_params.base.graphicsThread = THREADPRI_UTILITY_BASE + 1;
    osk_params.base.accessThread   = THREADPRI_UTILITY_BASE + 3;
    osk_params.base.fontThread     = THREADPRI_UTILITY_BASE + 2;
    osk_params.base.soundThread    = THREADPRI_UTILITY_BASE;
    osk_params.datacount = 1;
    osk_params.data      = &osk_data;
    osk_data.language    = PSP_UTILITY_OSK_LANGUAGE_DEFAULT;
    osk_data.inputtype   = PSP_UTILITY_OSK_INPUTTYPE_ALL;
    osk_data.lines       = 1;

    osk_active = 0;

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
    const double now = time_now();
    const float last_pad_x = pad_x;
    const float last_pad_y = pad_y;
    const int last_dpad_x = buttons[7] ? -1 : buttons[5] ? +1 : 0;
    const int last_dpad_y = buttons[4] ? -1 : buttons[6] ? +1 : 0;

    SceCtrlData pad_data;

    /* Retrieve the current control state. */
    sceCtrlPeekBufferPositive(&pad_data, 1);

    /* Record the analog pad position. */
    if (pad_data.Lx >= 128 - ANALOG_THRESHOLD
     && pad_data.Lx < 128 + ANALOG_THRESHOLD) {
        pad_x = 0;
    } else {
        pad_x = (pad_data.Lx - 127.5f) * (1/127.5f);
    }
    if (pad_data.Ly >= 128 - ANALOG_THRESHOLD
     && pad_data.Ly < 128 + ANALOG_THRESHOLD) {
        pad_y = 0;
    } else {
        pad_y = (pad_data.Ly - 127.5f) * (1/127.5f);
    }

    /*
     * Deal with two PSP firmware bugs:
     * (1) Even if the HOLD switch is active, analog pad input is still
     *     passed to the application, so nullify it ourselves in that case.
     * (2) Moving the analog pad doesn't reset the power-save timer, so
     *     reset it manually if analog pad input is detected.
     */
    if (pad_data.Buttons & PSP_CTRL_HOLD) {
        pad_x = pad_y = 0;
    }
    if (pad_x || pad_y) {
        scePowerTick(0);
    }

    /* Generate a STICK_CHANGE event if the analog pad was moved. */
    if (pad_x != last_pad_x || pad_y != last_pad_y) {
        (*event_callback)(&(InputEvent){
            .type = INPUT_EVENT_JOYSTICK,
            .detail = INPUT_JOYSTICK_STICK_CHANGE,
            .timestamp = now,
            {.joystick = {.device = 0, .index = 0, .x = pad_x, .y = pad_y}}});
    }

    /* Record button state, generating BUTTON events as appropriate. */
    for (int i = 0; i < lenof(buttons); i++) {
        const int state = (pad_data.Buttons >> i) & 1;
        /* Buttons 4-7 are the D-pad, so we ignore them here. */
        if ((i < 4 || i > 7) && state != buttons[i]) {
            (*event_callback)(&(InputEvent){
                .type = INPUT_EVENT_JOYSTICK,
                .detail = (state ? INPUT_JOYSTICK_BUTTON_DOWN
                                 : INPUT_JOYSTICK_BUTTON_UP),
                .timestamp = now,
                {.joystick = {.device = 0, .index = i}}});
        }
        buttons[i] = state;
    }

    /* Generate a DPAD_CHANGE event if appropriate. */
    const int dpad_x = buttons[7] ? -1 : buttons[5] ? +1 : 0;
    const int dpad_y = buttons[4] ? -1 : buttons[6] ? +1 : 0;
    if (dpad_x != last_dpad_x || dpad_y != last_dpad_y) {
        (*event_callback)(&(InputEvent){
            .type = INPUT_EVENT_JOYSTICK,
            .detail = INPUT_JOYSTICK_DPAD_CHANGE,
            .timestamp = now,
            {.joystick = {.device = 0, .x = dpad_x, .y = dpad_y}}});
    }

    /* Update the OSK if it's running. */
    if (osk_active) {
        const int status = sceUtilityOskGetStatus();
        if (status == PSP_UTILITY_DIALOG_VISIBLE) {
            int res = sceUtilityOskUpdate(1);
            if (res < 0) {
                DLOG("sceUtilityOskUpdate() failed: %s", psp_strerror(res));
            }
        } else if (sceUtilityOskGetStatus() == PSP_UTILITY_DIALOG_QUIT) {
            int res = sceUtilityOskShutdownStart();
            if (res < 0) {
                DLOG("sceUtilityOskShutdownStart(): %s", psp_strerror(res));
            } else {
                osk_shutting_down = 1;
            }
        } else if (sceUtilityOskGetStatus() == PSP_UTILITY_DIALOG_FINISHED) {
            finish_osk();
        }
    }
}

/*-----------------------------------------------------------------------*/

void sys_input_info(SysInputInfo *info_ret)
{
    info_ret->has_joystick    =  1;
    info_ret->num_joysticks   =  1;
    info_ret->joysticks       =  &joystick_info;
    info_ret->has_keyboard    =  0;
    info_ret->has_mouse       =  0;
    info_ret->has_text        =  1;
    info_ret->text_uses_custom_interface = 1;
    info_ret->text_has_prompt =  1;
    info_ret->has_touch       =  0;
}

/*-----------------------------------------------------------------------*/

void sys_input_grab(UNUSED int grab)
{
    /* Meaningless on the PSP. */
}

/*-----------------------------------------------------------------------*/

int sys_input_is_quit_requested(void)
{
    /* We terminate immediately on the HOME screen callback, so this
     * function never returns true. */
    return 0;
}

/*-----------------------------------------------------------------------*/

int sys_input_is_suspend_requested(void)
{
    return psp_suspend;
}

/*-----------------------------------------------------------------------*/

void sys_input_acknowledge_suspend_request(void)
{
    if (psp_suspend) {
        psp_sound_low_pause();
        psp_file_pause();
        sys_graphics_sync(0);

        sceKernelSignalSema(psp_suspend_ok_sema, 1);
        sceKernelWaitSema(psp_resume_sema, 1, NULL);

        psp_file_unpause();
        psp_sound_low_unpause();
    }
}

/*************************************************************************/
/********************* Interface: Joystick handling **********************/
/*************************************************************************/

void sys_input_enable_unfocused_joystick(int enable)
{
    /* Nothing to do on the PSP. */
}

/*-----------------------------------------------------------------------*/

char *sys_input_joystick_copy_name(int index)
{
    ASSERT(index == 0, return NULL);
    return mem_strdup("Sony PlayStation Portable", 0);
}

/*-----------------------------------------------------------------------*/

int sys_input_joystick_button_mapping(int index, int name)
{
    ASSERT(index == 0, return -1);
    switch (name) {
      case INPUT_JOYBUTTON_START:
        return 3;
      case INPUT_JOYBUTTON_SELECT:
        return 0;
      case INPUT_JOYBUTTON_FACE_UP:
        return 12;
      case INPUT_JOYBUTTON_FACE_LEFT:
        return 15;
      case INPUT_JOYBUTTON_FACE_RIGHT:
        return 13;
      case INPUT_JOYBUTTON_FACE_DOWN:
        return 14;
      case INPUT_JOYBUTTON_L1:
        return 8;
      case INPUT_JOYBUTTON_R1:
        return 9;
      default:
        return -1;
    }
}

/*-----------------------------------------------------------------------*/

void sys_input_joystick_rumble(UNUSED int index, UNUSED float left,       // NOTREACHED
                               UNUSED float right, UNUSED float time) {}  // NOTREACHED

/*************************************************************************/
/*********************** Interface: Mouse handling ***********************/
/*************************************************************************/

void sys_input_mouse_set_position(UNUSED float x, UNUSED float y) {}  // NOTREACHED

/*************************************************************************/
/******************** Interface: Text entry handling *********************/
/*************************************************************************/

void sys_input_text_set_state(int on, const char *text, const char *prompt)
{
    if (!on) {
        if (osk_active && !osk_shutting_down) {
            int res = sceUtilityOskShutdownStart();
            if (res < 0) {
                DLOG("sceUtilityOskShutdownStart(): %s", psp_strerror(res));
            } else {
                osk_data.result = PSP_UTILITY_OSK_RESULT_CANCELLED;
                osk_shutting_down = 1;
            }
        }
        return;
    }

    /* The order of these field names is apparently reversed in the current
     * (r2493) PSPSDK.  Set both to maxlen+1 just to be safe; the null
     * terminator in "text" will keep the OSK from overrunning the end of
     * the default text. */
    osk_data.outtextlength = OSK_MAXLEN + 1;
    osk_data.outtextlimit  = OSK_MAXLEN + 1;

    osk_data.desc = utf8to16(prompt);
    if (!osk_data.desc) {
        DLOG("No memory for prompt buffer");
        goto error_return;
    }

    osk_data.intext = utf8to16(text);
    if (!osk_data.intext) {
        DLOG("No memory for default text buffer");
        goto error_free_desc;
    }

    osk_data.outtext = mem_alloc(2 * (OSK_MAXLEN + 1), 2, MEM_ALLOC_TEMP);
    if (!osk_data.outtext) {
        DLOG("No memory for output text buffer");
        goto error_free_intext;
    }

    int res = sceUtilityOskInitStart(&osk_params);
    if (res < 0) {
        DLOG("sceUtilityOskInitStart() failed: %s", psp_strerror(res));
        goto error_free_outtext;
    }

    osk_active = 1;
    return;

  error_free_outtext:
    mem_free(osk_data.outtext);
  error_free_intext:
    mem_free(osk_data.intext);
  error_free_desc:
    mem_free(osk_data.desc);
  error_return:
    return;
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static void send_text_event(InputEventDetail detail, int32_t ch)
{
    (*event_callback)(&(InputEvent){
        .type = INPUT_EVENT_TEXT, .detail = detail, .timestamp = time_now(),
        {.text = {.ch = ch}}});
}

/*-----------------------------------------------------------------------*/

static void finish_osk(void)
{
    if (osk_data.result == PSP_UTILITY_OSK_RESULT_CANCELLED) {
        send_text_event(INPUT_TEXT_CANCELLED, 0);
    } else {
        const int len = min(osk_data.outtextlength, osk_data.outtextlimit);
        for (int i = 0; i < len; i++) {
            ASSERT(osk_data.outtext[i] != 0, break);
            send_text_event(INPUT_TEXT_INPUT, osk_data.outtext[i]);
        }
        send_text_event(INPUT_TEXT_DONE, 0);
    }
}

/*-----------------------------------------------------------------------*/

static uint16_t *utf8to16(const char *str)
{
    /* Allocate a buffer big enough for the longest possible result; we'll
     * shrink it if necessary when we're done. */
    const int bufsize = (strlen(str) + 1) * 2;
    uint16_t *out = mem_alloc(bufsize, 2, MEM_ALLOC_TEMP);
    if (!out) {
        DLOG("Can't allocate %u bytes", bufsize);
        return NULL;
    }

    /* Convert the string, character by character. */
    int inpos = 0, outlen = 0;
    while (str[inpos] != 0) {
        const uint8_t ch = (uint8_t)str[inpos++];
        if (ch < 0x80) {
            out[outlen++] = ch;
        } else if (ch < 0xC0) {
            /* Continuation bytes are invalid as the first byte of a UTF-8
             * sequence. */
            DLOG("Invalid continuation byte 0x%02X at offset %u", ch, inpos-1);
            goto fail;
        } else if (ch < 0xE0) {
            const uint8_t ch_1 = (uint8_t)str[inpos++];
            if (ch_1 < 0x80 || ch_1 >= 0xC0) {
                /* The required continuation byte is missing. */
                DLOG("Missing continuation byte at offset %u (got 0x%02X)",
                     inpos-1, ch_1);
                goto fail;
            } else if (ch < 0xC2) {
                /* Characters with codepoints less than 128 must be coded
                 * using the single-byte format; for example, C1 9C for the
                 * backslash character (U+005C) is invalid.  This is a
                 * common attack vector against security vulnerabilities,
                 * so we explicitly disallow such invalid forms. */
                DLOG("Invalid extended form 0x%02X 0x%02X at offset %u",
                     inpos-2, ch, ch_1);
                goto fail;
            } else {
                out[outlen++] = (ch   & 0x1F) << 6
                              | (ch_1 & 0x3F) << 0;
            }
        } else if (ch < 0xF0) {
            const uint8_t ch_1 = (uint8_t)str[inpos++];
            const uint8_t ch_2 = (uint8_t)str[inpos++];
            if (ch_1 < 0x80 || ch_1 >= 0xC0) {
                DLOG("Missing continuation byte at offset %u (got 0x%02X)",
                     inpos-2, ch_1);
                goto fail;
            } else if (ch_2 < 0x80 || ch_2 >= 0xC0) {
                DLOG("Missing continuation byte at offset %u (got 0x%02X)",
                     inpos-1, ch_2);
                goto fail;
            } else if (ch == 0xE0 && ch_1 < 0xA0) {
                DLOG("Invalid extended form 0x%02X 0x%02X 0x%02X at offset %u",
                     inpos-3, ch, ch_1, ch_2);
                goto fail;
            } else {
                out[outlen++] = (ch   & 0x0F) << 12
                              | (ch_1 & 0x3F) <<  6
                              | (ch_2 & 0x3F) <<  0;
                if (out[outlen-1] >= 0xD800 && out[outlen-1] < 0xE000) {
                    DLOG("Invalid surrogate 0x%04X at offset %u",
                         out[outlen-1], inpos-3);
                    goto fail;
                }
            }
        } else {
            DLOG("Out-of-range codepoint with first byte 0x%02X at offset %u",
                 ch, inpos-1);
            goto fail;
        }
    }

    /* Append a terminating null. */
    out[outlen++] = 0;

    /* If we ended up with fewer characters than we allocated space for,
     * shrink the output buffer before returning it. */
    if (outlen*2 < bufsize) {
        /* This should never fail, but just in case, save the result in a
         * temporary variable and check for NULL first. */
        uint16_t *new_out = mem_realloc(out, outlen*2, MEM_ALLOC_TEMP);
        if (new_out) {
            out = new_out;
        }
    }

    return out;

  fail:
    mem_free(out);
    return NULL;
}

/*************************************************************************/
/*************************************************************************/
