/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/linux/input.c: Tests for Linux input handling.
 */

#include "src/base.h"
#include "src/graphics.h"
#include "src/input.h"
#define IN_SYSDEP  // So we get the real functions instead of the diversions.
#include "src/sysdep.h"
#include "src/sysdep/linux/internal.h"
#include "src/sysdep/posix/time.h"
#include "src/sysdep/test.h"
#include "src/test/base.h"
#include "src/test/graphics/internal.h"
#include "src/test/sysdep/linux/wrap-io.h"
#include "src/test/sysdep/linux/wrap-x11.h"
#include "src/time.h"

#include <dlfcn.h>
#define RTLD_DEFAULT  NULL
#define RTLD_NEXT     ((void *)(intptr_t)-1)

#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>
#include <X11/XF86keysym.h>
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XInput2.h>

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

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Keycodes for various keys used in testing, initialized by the test
 * runner. */
static int keycode_1;
static int keycode_backspace;
static int keycode_delete;
static int keycode_left;
static int keycode_right;
static int keycode_home;
static int keycode_end;
static int keycode_escape;
static int keycode_return;
static int keycode_kp_enter;
/* Invalid keycode (doesn't map to any keysym). */
static int keycode_invalid;
/* Key that doesn't map to any SIL keycode (if we can find such a key). */
static int keycode_unmapped;

/* Buffer of events received from the Linux sys_input module. */
static InputEvent events[10];
static int num_events;

/*-----------------------------------------------------------------------*/

/* Is the simulated joystick currently connected? */
static uint8_t joystick_connected;
/* Joystick identification information. */
static struct input_id joystick_id;
static char joystick_name[100];
/* Inputs available on the joystick. */
static uint8_t joystick_ev_bits[(EV_MAX+1) / 8];
static uint8_t joystick_ev_key[(KEY_MAX+1) / 8];
static uint8_t joystick_ev_abs[(ABS_MAX+1) / 8];
static uint8_t joystick_ev_ff[(FF_MAX+1) / 8];
/* Current state of ABS inputs. */
static struct input_absinfo joystick_absinfo[ABS_MAX+1];
/* Input events to send via read(). */
static struct input_event joystick_events[10];
static int num_joystick_events;

/* Is a force-feedback effect defined? */
static uint8_t ff_effect_defined;
/* Currently defined force-feedback effect. */
struct ff_effect ff_effect;
/* Last force-feedback event written to joystick. */
struct input_event ff_event;

/* File descriptor for the simulated joystick (-1 if not open). */
static int joystick_fd = -1;
/* Error code to return from open/read/write/ioctl operations on the
 * simulated joystick device, or zero for normal behavior.  This is reset
 * to zero after triggering a single error. */
static int joystick_fd_errno;
/* Error code to return from fcntl() on the simulated joystick device, or
 * zero for normal behavior.  This is reset to zero after triggering a
 * single error. */
static int joystick_fcntl_errno;
/* A specific ioctl() request on the joystick device to fail with EIO.
 * The size field of the request is ignored when checking for a match.
 * This is reset to zero after triggering a single error. */
static unsigned int joystick_fail_ioctl;

/* Is the simulated /dev/input directory open? */
static uint8_t devinput_dir_open;
/* Next eventN entry to return from readdir() (-1 = end of directory). */
static int devinput_next_entry;
/* Error code to return from opendir("/dev/input"), or zero for normal
 * behavior.  This is reset to zero after triggering a single error. */
static int devinput_opendir_errno;
/* If non-NULL, readdir("/dev/input") will return only this name instead
 * of the normal list of names. */
const char *devinput_readdir_override = NULL;

/* File descriptor for simulated inotify (-1 if not active). */
static int inotify_fd = -1;
/* File descriptor used in tests to send simulated inotify events. */
static int inotify_send_fd = -1;
/* Number of bytes written in the last write() call on inotify_send_fd.
 * This must be set correctly before the write to ensure that subsequent
 * reads on inotify_fd behave correctly. */
static int inotify_bytes_written;
/* Have we seen a read() call on inotify_fd? */
static uint8_t inotify_got_read;
/* Error code to return from read/write operations on the simulated
 * inotify descriptor, or zero for normal behavior.  This is reset to
 * zero after triggering a single error. */
static int inotify_fd_errno;

/* /dev/input device name used to test handling of device names outside the
 * range of event_info[].  Must start with "event1" to be recognized by the
 * open() wrapper. */
#define OUT_OF_RANGE_EVENT_DEVICE  "event123456789"
/* Magic pointer value indicating an opendir() handle to /dev/input. */
#define SIMULATED_DEVINPUT_DIR  ((DIR *)(intptr_t)-1)
/* Macro to set a bit in an evdev array. */
#define SET_BIT(array, index)  ((array)[((index)/8)] |= 1 << ((index)%8))

/*************************************************************************/
/************************ Joystick I/O overrides *************************/
/*************************************************************************/

static int joystick_open(const char *pathname, int flags, ...)
{
    if (strncmp(pathname, "/dev/input/event", 16) != 0) {
        va_list args;
        va_start(args, flags);
        int mode = va_arg(args, int);
        return trampoline_open(pathname, flags, mode);
    }

    if (strncmp(pathname, "/dev/input/event1", 17) != 0) {
        errno = EACCES;
        return -1;
    }
    ASSERT(joystick_fd < 0);
    if (joystick_fd_errno) {
        errno = joystick_fd_errno;
        joystick_fd_errno = 0;
        return -1;
    }
    ASSERT((joystick_fd = open("/dev/null", O_RDWR)) >= 0);
    return joystick_fd;
}

/*-----------------------------------------------------------------------*/

static int joystick_close(int fd)
{
    if (fd == joystick_fd) {
        /* Save the FD aside and reset joystick_fd before calling close()
         * to avoid infinite recursion without having to manually call the
         * wrapped function pointer. */
        const int fd_to_close = joystick_fd;
        joystick_fd = -1;
        close(fd_to_close);
        return 0;
    } else if (fd == inotify_fd) {
        const int fd_to_close = inotify_fd;
        inotify_fd = -1;
        close(fd_to_close);
        close(inotify_send_fd);
        inotify_send_fd = -1;
        return 0;
    } else {
        return trampoline_close(fd);
    }
}

/*-----------------------------------------------------------------------*/

static ssize_t joystick_read(int fd, void *buf, size_t count)
{
    if (fd == joystick_fd) {
        if (joystick_fd_errno) {
            errno = joystick_fd_errno;
            joystick_fd_errno = 0;
            return -1;
        }
        const size_t event_size = sizeof(*joystick_events);
        if (count % event_size != 0) {
            errno = EINVAL;
            return -1;
        }
        const int events_to_copy =
            min(num_joystick_events, (int)(count / event_size));
        memcpy(buf, joystick_events, events_to_copy * event_size);
        memmove(joystick_events, &joystick_events[events_to_copy],
                (num_joystick_events - events_to_copy) * event_size);
        num_joystick_events -= events_to_copy;
        return events_to_copy * event_size;
    } else if (fd == inotify_fd) {
        if (inotify_fd_errno) {
            errno = inotify_fd_errno;
            inotify_fd_errno = 0;
            return -1;
        }
        /* We need to simulate the behavior of inotify descriptors which
         * returns EINVAL if the requested buffer is too small to hold the
         * next event. */
        if ((int)count < inotify_bytes_written) {
            errno = EINVAL;
            return -1;
        }
        inotify_bytes_written = 0;
        inotify_got_read = 1;
        return trampoline_read(fd, buf, count);
    } else {
        return trampoline_read(fd, buf, count);
    }
}

/*-----------------------------------------------------------------------*/

static ssize_t joystick_write(int fd, const void *buf, size_t count)
{
    if (fd == joystick_fd) {
        if (joystick_fd_errno) {
            errno = joystick_fd_errno;
            joystick_fd_errno = 0;
            return -1;
        }
        const size_t event_size = sizeof(ff_event);
        if (count % event_size != 0) {
            errno = EINVAL;
            return -1;
        }
        if (count > 0) {
            memcpy(&ff_event, (char *)buf + (count - event_size), event_size);
        }
        return count;
    } else {
        return trampoline_write(fd, buf, count);
    }
}

/*-----------------------------------------------------------------------*/

static int joystick_fcntl(int fd, int cmd, ...)
{
    if (fd == joystick_fd && joystick_fcntl_errno) {
        errno = joystick_fcntl_errno;
        joystick_fcntl_errno = 0;
        return -1;
    }
    va_list args;
    va_start(args, cmd);
    void *arg = va_arg(args, void *);
    return trampoline_fcntl(fd, cmd, arg);
}

/*-----------------------------------------------------------------------*/

static int joystick_ioctl(int fd, unsigned long request, ...)
{
    if (fd == joystick_fd) {
        if (joystick_fd_errno) {
            errno = joystick_fd_errno;
            joystick_fd_errno = 0;
            return -1;
        }
        if ((request & 0xC000FFFFUL) == (joystick_fail_ioctl & 0xC000FFFFUL)) {
            joystick_fail_ioctl = 0;
            errno = EIO;
            return -1;
        }
        if (request == EVIOCGID) {
            va_list args;
            va_start(args, request);
            struct input_id *id = va_arg(args, struct input_id *);
            va_end(args);
            *id = joystick_id;
            return 0;
        } else if (request == EVIOCRMFF) {
            va_list args;
            va_start(args, request);
            int id = va_arg(args, int);
            va_end(args);
            if (!ff_effect_defined) {
                errno = EINVAL;
                return -1;
            } else if (id != ff_effect.id) {
                errno = EINVAL;
                return -1;
            } else {
                ff_effect_defined = 0;
                return 0;
            }
        } else if (request == EVIOCSCLOCKID) {
            va_list args;
            va_start(args, request);
            int *clock_id_ptr = va_arg(args, int *);
            va_end(args);
            ASSERT(clock_id_ptr);
            ASSERT(*clock_id_ptr == sys_posix_time_clock());
            return 0;
        } else if (request == EVIOCSFF) {
            va_list args;
            va_start(args, request);
            struct ff_effect *effect = va_arg(args, struct ff_effect *);
            va_end(args);
            if (ff_effect_defined) {
                errno = EINVAL;
                return -1;
            } else {
                effect->id = ff_effect.id + 1;
                ff_effect = *effect;
                ff_effect_defined = 1;
                return 0;
            }
        } else if ((request & 0xFFFFFFC0UL) == EVIOCGABS(0)) {
            const int index = request & 0x3F;
            va_list args;
            va_start(args, request);
            struct input_absinfo *absinfo =
                va_arg(args, struct input_absinfo *);
            va_end(args);
            if (index < lenof(joystick_absinfo)) {
                *absinfo = joystick_absinfo[index];
            } else {
                mem_clear(absinfo, sizeof(*absinfo));
            }
            return 0;
        } else if ((request & 0xC000FFFFUL) == EVIOCGBIT(0, 0)) {
            const int len = request>>16 & 0x3FFF;
            va_list args;
            va_start(args, request);
            void *buf = va_arg(args, void *);
            va_end(args);
            mem_clear(buf, len);
            memcpy(buf, joystick_ev_bits,
                   ubound((int)sizeof(joystick_ev_bits), len));
            return 0;
        } else if ((request & 0xC000FFFFUL) == EVIOCGBIT(EV_ABS, 0)) {
            const int len = request>>16 & 0x3FFF;
            va_list args;
            va_start(args, request);
            void *buf = va_arg(args, void *);
            va_end(args);
            mem_clear(buf, len);
            memcpy(buf, joystick_ev_abs,
                   ubound((int)sizeof(joystick_ev_abs), len));
            return 0;
        } else if ((request & 0xC000FFFFUL) == EVIOCGBIT(EV_FF, 0)) {
            const int len = request>>16 & 0x3FFF;
            va_list args;
            va_start(args, request);
            void *buf = va_arg(args, void *);
            va_end(args);
            mem_clear(buf, len);
            memcpy(buf, joystick_ev_ff,
                   ubound((int)sizeof(joystick_ev_ff), len));
            return 0;
        } else if ((request & 0xC000FFFFUL) == EVIOCGBIT(EV_KEY, 0)) {
            const int len = request>>16 & 0x3FFF;
            va_list args;
            va_start(args, request);
            void *buf = va_arg(args, void *);
            va_end(args);
            mem_clear(buf, len);
            memcpy(buf, joystick_ev_key,
                   ubound((int)sizeof(joystick_ev_key), len));
            return 0;
        } else if ((request & 0xC000FFFFUL) == EVIOCGNAME(0)) {
            const int len = request>>16 & 0x3FFF;
            va_list args;
            va_start(args, request);
            void *buf = va_arg(args, void *);
            va_end(args);
            mem_clear(buf, len);
            memcpy(buf, joystick_name,
                   ubound((int)sizeof(joystick_name), len));
            return 0;
        } else {
            errno = ENOTTY;
            return -1;
        }
    } else if (fd == inotify_fd) {
        errno = ENOTTY;
        return -1;
    } else {
        va_list args;
        va_start(args, request);
        void *arg = va_arg(args, void *);
        return trampoline_ioctl(fd, request, arg);
    }
}

/*-----------------------------------------------------------------------*/

static DIR *joystick_opendir(const char *pathname)
{
    if (strcmp(pathname, "/dev/input") != 0) {
        return trampoline_opendir(pathname);
    }

    ASSERT(!devinput_dir_open);
    if (devinput_opendir_errno) {
        errno = devinput_opendir_errno;
        devinput_opendir_errno = 0;
        return NULL;
    }
    devinput_dir_open = 1;
    devinput_next_entry = 0;
    return SIMULATED_DEVINPUT_DIR;
}

/*-----------------------------------------------------------------------*/

static struct dirent *joystick_readdir(DIR *d)
{
    if (d != SIMULATED_DEVINPUT_DIR) {
        return trampoline_readdir(d);
    }

    if (!devinput_dir_open) {
        errno = EBADF;
        return NULL;
    }
    if (devinput_next_entry < 0) {
        return NULL;
    }
    static struct dirent de;
    if (devinput_next_entry == 0) {
        if (devinput_readdir_override) {
            ASSERT(strformat_check(de.d_name, sizeof(de.d_name), "%s",
                                   devinput_readdir_override));
            devinput_next_entry = -1;
        } else {
            ASSERT(strformat_check(de.d_name, sizeof(de.d_name), "mice"));
            devinput_next_entry++;
        }
    } else {
        ASSERT(strformat_check(de.d_name, sizeof(de.d_name), "event%d",
                               devinput_next_entry - 1));
        if (devinput_next_entry == 1 && joystick_connected) {
            devinput_next_entry++;
        } else {
            devinput_next_entry = -1;
        }
    }
    return &de;
}

/*-----------------------------------------------------------------------*/

static int joystick_closedir(DIR *d)
{
    if (d != SIMULATED_DEVINPUT_DIR) {
        return trampoline_closedir(d);
    }

    if (!devinput_dir_open) {
        errno = EBADF;
        return -1;
    }
    devinput_dir_open = 0;
    return 0;
}

/*-----------------------------------------------------------------------*/

static int joystick_inotify_init(void)
{
    ASSERT(inotify_fd < 0);
    if (inotify_fd_errno) {
        errno = inotify_fd_errno;
        inotify_fd_errno = 0;
        return -1;
    }
    inotify_fd = inotify_init1(0);
    return inotify_fd;
}

/*-----------------------------------------------------------------------*/

static int joystick_inotify_add_watch(int fd, const char *pathname,
                                      uint32_t mask)
{
    if (fd == inotify_fd
        && (strcmp(pathname, "/dev/input") != 0
            || mask != (IN_ATTRIB | IN_CREATE | IN_DELETE)))
    {
        inotify_fd = -1;
    }
    if (fd != inotify_fd) {
        return trampoline_inotify_add_watch(fd, pathname, mask);
    }

    if (inotify_fd_errno) {
        errno = inotify_fd_errno;
        inotify_fd_errno = 0;
        return -1;
    }
    int pipe_fds[2];
    ASSERT(pipe(pipe_fds) == 0);
    ASSERT(dup2(pipe_fds[0], inotify_fd) == inotify_fd);
    close(pipe_fds[0]);
    inotify_send_fd = pipe_fds[1];
    return 0;
}

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

/**
 * event_callback:  Callback which receives input events from the sys_input
 * module.
 */
static void event_callback(const InputEvent *event)
{
    ASSERT(num_events < lenof(events));
    events[num_events++] = *event;
}

/*-----------------------------------------------------------------------*/

/**
 * check_xinput2:  Check whether XInput 2.2+ is available.  If so, return
 * the X11 opcode for the extension; otherwise, return zero.
 */
static int check_xinput2(void)
{
    int xinput2_opcode;
    if (!XQueryExtension(linux_x11_display(), "XInputExtension",
                         &xinput2_opcode, (int[1]){0}, (int[1]){0})) {
        return 0;
    }
    __typeof__(XIQueryVersion) *p_XIQueryVersion =
        dlsym(RTLD_NEXT, "XIQueryVersion");
    if (!p_XIQueryVersion) {
        return 0;
    }
    int major = 2;
    int minor = 2;
    if ((*p_XIQueryVersion)(linux_x11_display(), &major, &minor) != Success) {
        return 0;
    }
    if (!(major > 2 || (major == 2 && minor >= 2))) {
        return 0;
    }
    return xinput2_opcode;
}

/*-----------------------------------------------------------------------*/

/**
 * xinput2_master_pointer:  Return the XInput2 device ID of the master
 * pointer device, for use in synthesized XInput2 events.  Assumes that
 * XInput2 is available.
 */
static int xinput2_master_pointer(void)
{
    __typeof__(XIQueryDevice) *p_XIQueryDevice =
        dlsym(RTLD_NEXT, "XIQueryDevice");
    __typeof__(XIFreeDeviceInfo) *p_XIFreeDeviceInfo =
        dlsym(RTLD_NEXT, "XIFreeDeviceInfo");
    ASSERT(p_XIQueryDevice);
    ASSERT(p_XIFreeDeviceInfo);

    int num_devices = 0;
    XIDeviceInfo *devices = (*p_XIQueryDevice)(
        linux_x11_display(), XIAllMasterDevices, &num_devices);
    if (!devices) {
        return 0;
    }

    int deviceid = 0;
    for (int i = 0; i < num_devices; i++) {
        if (devices[i].use == XIMasterPointer) {
            deviceid = devices[i].deviceid;
            break;
        }
    }

    (*p_XIFreeDeviceInfo)(devices);
    return deviceid;
}

/*-----------------------------------------------------------------------*/

/**
 * XIGetSelectedEvents:  Wrapper for XInput2 function of the same name,
 * since we don't link against libXi.  Returns NULL if libXi is not loaded.
 */
XIEventMask *XIGetSelectedEvents(Display *display, Window win,
                                 int *num_masks_return)
{
    __typeof__(XIGetSelectedEvents) *p_XIGetSelectedEvents =
        dlsym(RTLD_NEXT, "XIGetSelectedEvents");
    ASSERT(p_XIGetSelectedEvents != XIGetSelectedEvents);
    if (!p_XIGetSelectedEvents) {
        return NULL;
    }
    return (*p_XIGetSelectedEvents)(display, win, num_masks_return);
}

/*-----------------------------------------------------------------------*/

/**
 * write_inotify:  Send an inotify event to the simulated inotify descriptor.
 * The descriptor is assumed to be open.
 *
 * [Parameters]
 *     mask: Event bitmask (IN_*).
 *     name: Event filename (length must be < sizeof(struct inotify_event)).
 *     wait_for_read: True to wait for the event to be read before returning.
 */
static void write_inotify(uint32_t mask, const char *name, int wait_for_read)
{
    PRECOND(strlen(name) + 1 <= sizeof(struct inotify_event));

    struct inotify_event event_buffer[2];
    mem_clear(event_buffer, sizeof(event_buffer));
    struct inotify_event *event = &event_buffer[0];
    event->wd = 0;  // Not used by the input code.
    event->mask = mask;
    event->cookie = 0;  // Not used by the input code.
    event->len = strlen(name) + 1;  // Includes the trailing null.
    memcpy(event->name, name, event->len);
    inotify_bytes_written = sizeof(event_buffer);
    inotify_got_read = 0;
    BARRIER();
    ASSERT(write(inotify_send_fd, event_buffer, sizeof(event_buffer))
           == sizeof(event_buffer));

    if (wait_for_read) {
        do {
            nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = 1000*1000},
                      NULL);
            BARRIER();
        } while (!inotify_got_read);
    }
}

/*-----------------------------------------------------------------------*/

/**
 * reset_joystick_events:  Reset the timestamps for all events in
 * joystick_events[] such that the relative timestamp for each events is
 * the index of the event plus one.
 */
static void reset_joystick_events(void)
{
    for (int i = 0; i < lenof(joystick_events); i++) {
        const uint64_t unit = sys_time_unit();
        const uint64_t epoch = sys_posix_time_epoch() / (unit / 1000000);
        joystick_events[i].time.tv_sec = (epoch / 1000000) + (i+1);
        joystick_events[i].time.tv_usec = epoch % 1000000;
    }
}

/*-----------------------------------------------------------------------*/

/**
 * setup_dpad_controller:  Set up the simulated joystick data to emulate a
 * generic gamepad with a D-pad and no analog stick.
 */
static void setup_dpad_controller(void)
{
    joystick_id.bustype = BUS_USB;
    joystick_id.vendor = 0;
    joystick_id.product = 0;
    joystick_id.version = 0;
    ASSERT(strformat_check(joystick_name, sizeof(joystick_name),
                           "SIL test gamepad"));
    mem_clear(joystick_ev_bits, sizeof(joystick_ev_bits));
    SET_BIT(joystick_ev_bits, EV_KEY);
    mem_clear(joystick_ev_abs, sizeof(joystick_ev_abs));
    mem_clear(joystick_ev_key, sizeof(joystick_ev_key));
    SET_BIT(joystick_ev_key, BTN_DPAD_UP);
    SET_BIT(joystick_ev_key, BTN_DPAD_DOWN);
    SET_BIT(joystick_ev_key, BTN_DPAD_LEFT);
    SET_BIT(joystick_ev_key, BTN_DPAD_RIGHT);
    SET_BIT(joystick_ev_key, BTN_A);
    SET_BIT(joystick_ev_key, BTN_B);
    SET_BIT(joystick_ev_key, BTN_SELECT);
    SET_BIT(joystick_ev_key, BTN_START);
    mem_clear(joystick_ev_ff, sizeof(joystick_ev_ff));
}

/*-----------------------------------------------------------------------*/

/**
 * setup_ps3_controller:  Set up the simulated joystick data to emulate a
 * USB-connected PlayStation 3 gamepad.
 */
static void setup_ps3_controller(void)
{
    joystick_id.bustype = BUS_USB;
    joystick_id.vendor = 0x054C;
    joystick_id.product = 0x0268;
    joystick_id.version = 0x000C;
    ASSERT(strformat_check(joystick_name, sizeof(joystick_name),
                           "Sony PLAYSTATION(R)3 Controller"));
    /* We don't actually use all of these flags in the input code, but we
     * set them here just for completeness. */
    SET_BIT(joystick_ev_bits, EV_SYN);
    SET_BIT(joystick_ev_bits, EV_MSC);
    SET_BIT(joystick_ev_abs, ABS_Z);
    SET_BIT(joystick_ev_abs, ABS_RZ);
    for (int i = 0x28; i < 0x3F; i++) {
        SET_BIT(joystick_ev_abs, i);
    }
    for (int i = BTN_JOYSTICK; i < BTN_JOYSTICK+16; i++) {
        SET_BIT(joystick_ev_key, i);
    }
    for (int i = BTN_TRIGGER_HAPPY; i < BTN_TRIGGER_HAPPY+16; i++) {
        SET_BIT(joystick_ev_key, i);
    }
    SET_BIT(joystick_ev_ff, FF_PERIODIC);
    SET_BIT(joystick_ev_ff, FF_SQUARE);
    SET_BIT(joystick_ev_ff, FF_TRIANGLE);
    SET_BIT(joystick_ev_ff, FF_SINE);
    SET_BIT(joystick_ev_ff, FF_GAIN);
    joystick_absinfo[ABS_X].maximum = 255;
    joystick_absinfo[ABS_X].flat = 15;
    joystick_absinfo[ABS_Y].maximum = 255;
    joystick_absinfo[ABS_Y].flat = 15;
    joystick_absinfo[ABS_Z].maximum = 255;
    joystick_absinfo[ABS_Z].flat = 15;
    joystick_absinfo[ABS_RZ].maximum = 255;
    joystick_absinfo[ABS_RZ].flat = 15;
}

/*-----------------------------------------------------------------------*/

/**
 * setup_x360_controller:  Set up the simulated joystick data to emulate a
 * USB-connected Xbox 360 gamepad.
 */
static void setup_x360_controller(void)
{
    joystick_id.bustype = BUS_USB;
    joystick_id.vendor = 0x045E;
    joystick_id.product = 0x028E;
    joystick_id.version = 0x0114;
    ASSERT(strformat_check(joystick_name, sizeof(joystick_name),
                           "Microsoft X-Box 360 pad"));
    SET_BIT(joystick_ev_bits, EV_SYN);
    SET_BIT(joystick_ev_abs, ABS_Z);
    SET_BIT(joystick_ev_abs, ABS_RX);
    SET_BIT(joystick_ev_abs, ABS_RY);
    SET_BIT(joystick_ev_abs, ABS_RZ);
    SET_BIT(joystick_ev_abs, ABS_HAT0X);
    SET_BIT(joystick_ev_abs, ABS_HAT0Y);
    mem_clear(joystick_ev_key, sizeof(joystick_ev_key));
    SET_BIT(joystick_ev_key, BTN_SOUTH);
    SET_BIT(joystick_ev_key, BTN_EAST);
    SET_BIT(joystick_ev_key, BTN_NORTH);
    SET_BIT(joystick_ev_key, BTN_WEST);
    SET_BIT(joystick_ev_key, BTN_TL);
    SET_BIT(joystick_ev_key, BTN_TR);
    SET_BIT(joystick_ev_key, BTN_SELECT);
    SET_BIT(joystick_ev_key, BTN_START);
    SET_BIT(joystick_ev_key, BTN_MODE);
    SET_BIT(joystick_ev_key, BTN_THUMBL);
    SET_BIT(joystick_ev_key, BTN_THUMBR);
    SET_BIT(joystick_ev_ff, FF_PERIODIC);
    SET_BIT(joystick_ev_ff, FF_SQUARE);
    SET_BIT(joystick_ev_ff, FF_TRIANGLE);
    SET_BIT(joystick_ev_ff, FF_SINE);
    SET_BIT(joystick_ev_ff, FF_GAIN);
    joystick_absinfo[ABS_X].minimum = -32768;
    joystick_absinfo[ABS_X].maximum = 32767;
    joystick_absinfo[ABS_X].fuzz = 16;
    joystick_absinfo[ABS_X].flat = 128;
    joystick_absinfo[ABS_Y].minimum = -32768;
    joystick_absinfo[ABS_Y].maximum = 32767;
    joystick_absinfo[ABS_Y].fuzz = 16;
    joystick_absinfo[ABS_Y].flat = 128;
    joystick_absinfo[ABS_Z].maximum = 255;
    joystick_absinfo[ABS_RX].minimum = -32768;
    joystick_absinfo[ABS_RX].maximum = 32767;
    joystick_absinfo[ABS_RX].fuzz = 16;
    joystick_absinfo[ABS_RX].flat = 128;
    joystick_absinfo[ABS_RY].minimum = -32768;
    joystick_absinfo[ABS_RY].maximum = 32767;
    joystick_absinfo[ABS_RY].fuzz = 16;
    joystick_absinfo[ABS_RY].flat = 128;
    joystick_absinfo[ABS_RZ].maximum = 255;
    joystick_absinfo[ABS_HAT0X].minimum = -1;
    joystick_absinfo[ABS_HAT0X].maximum = 1;
    joystick_absinfo[ABS_HAT0Y].minimum = -1;
    joystick_absinfo[ABS_HAT0Y].maximum = 1;
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

static int do_test_linux_input(void);
int test_linux_input(void)
{
    Display *display = linux_x11_display();
    for (int keycode = 1; keycode < 256; keycode++) {
        const KeySym keysym = XkbKeycodeToKeysym(display, keycode, 0, 0);
        if (keysym == XK_1) {
            keycode_1 = keycode;
        } else if (keysym == XK_BackSpace) {
            keycode_backspace = keycode;
        } else if (keysym == XK_Delete) {
            keycode_delete = keycode;
        } else if (keysym == XK_Left) {
            keycode_left = keycode;
        } else if (keysym == XK_Right) {
            keycode_right = keycode;
        } else if (keysym == XK_Home) {
            keycode_home = keycode;
        } else if (keysym == XK_End) {
            keycode_end = keycode;
        } else if (keysym == XK_Escape) {
            keycode_escape = keycode;
        } else if (keysym == XK_Return) {
            keycode_return = keycode;
        } else if (keysym == XK_KP_Enter) {
            keycode_kp_enter = keycode;
        } else if (keysym == NoSymbol) {
            keycode_invalid = keycode;
        } else if ((keysym >= XK_F21 && keysym <= XK_F35)
                || keysym == XK_Hyper_L
                || keysym == XK_Hyper_R
                || keysym == XF86XK_MonBrightnessUp
                || keysym == XF86XK_KbdLightOnOff
                || keysym == XF86XK_Display
                || keysym == XF86XK_Standby
                || keysym == XF86XK_PowerDown
                || keysym == XF86XK_AudioPlay
                || keysym == XF86XK_Start) {
            /* Hopefully most keymaps will have at least one of the above. */
            keycode_unmapped = keycode;
        }
    }

    if (!keycode_unmapped) {
        WARN("Can't find an unmapped X11 keycode; SYSTEM_KEY event tests"
             " will be skipped.");
    }

    return run_tests_in_window(do_test_linux_input);
}

DEFINE_GENERIC_TEST_RUNNER(do_test_linux_input)

/*-----------------------------------------------------------------------*/

TEST_INIT(init)
{
    joystick_connected = 0;
    mem_clear(&joystick_id, sizeof(joystick_id));
    mem_clear(joystick_name, sizeof(joystick_name));
    mem_clear(joystick_ev_bits, sizeof(joystick_ev_bits));
    mem_clear(joystick_ev_key, sizeof(joystick_ev_key));
    mem_clear(joystick_ev_abs, sizeof(joystick_ev_abs));
    mem_clear(joystick_ev_ff, sizeof(joystick_ev_ff));
    mem_clear(joystick_absinfo, sizeof(joystick_absinfo));
    mem_clear(joystick_events, sizeof(joystick_events));
    num_joystick_events = 0;
    ff_effect_defined = 0;
    mem_clear(&ff_effect, sizeof(ff_effect));
    mem_clear(&ff_event, sizeof(ff_event));
    joystick_fd = -1;
    joystick_fd_errno = 0;
    joystick_fcntl_errno = 0;
    joystick_fail_ioctl = 0;
    devinput_dir_open = 0;
    devinput_next_entry = 0;
    devinput_opendir_errno = 0;
    devinput_readdir_override = NULL;
    inotify_fd = -1;
    inotify_send_fd = -1;
    inotify_bytes_written = 0;
    inotify_got_read = 0;
    inotify_fd_errno = 0;

    clear_io_wrapper_variables();
    clear_x11_wrapper_variables();
    override_open = joystick_open;
    override_close = joystick_close;
    override_read = joystick_read;
    override_write = joystick_write;
    override_fcntl = joystick_fcntl;
    override_ioctl = joystick_ioctl;
    override_opendir = joystick_opendir;
    override_readdir = joystick_readdir;
    override_closedir = joystick_closedir;
    override_inotify_init = joystick_inotify_init;
    override_inotify_add_watch = joystick_inotify_add_watch;

    /* Set up a simple joystick configuration to avoid having to repeat
     * this code in every tests.  Some tests modify the flags as needed. */
    SET_BIT(joystick_ev_bits, EV_ABS);
    SET_BIT(joystick_ev_bits, EV_KEY);
    SET_BIT(joystick_ev_bits, EV_FF);
    SET_BIT(joystick_ev_abs, ABS_X);
    SET_BIT(joystick_ev_abs, ABS_Y);
    /* Include a non-stick absolute input to verify that it gets ignored. */
    SET_BIT(joystick_ev_abs, ABS_VOLUME);
    SET_BIT(joystick_ev_key, BTN_JOYSTICK+0);
    SET_BIT(joystick_ev_key, BTN_JOYSTICK+2);
    SET_BIT(joystick_ev_key, BTN_TRIGGER_HAPPY+1);
    /* Include a non-button key input to verify that it gets ignored. */
    SET_BIT(joystick_ev_key, KEY_OK);
    SET_BIT(joystick_ev_ff, FF_RUMBLE);
    joystick_absinfo[ABS_X].maximum = 255;
    joystick_absinfo[ABS_Y].minimum = -128;
    joystick_absinfo[ABS_Y].maximum = 127;
    joystick_absinfo[ABS_Y].flat = 1;
    joystick_absinfo[ABS_VOLUME].maximum = 255;

    time_init();
    sys_time_init();  // Since time_init() will hit the test implementation.
    (void) sys_time_now();  // Initialize the sys_time_now() epoch.
    reset_joystick_events();

    num_events = 0;
    CHECK_TRUE(sys_input_init(event_callback));

    /* Ignore any real input events that may have come through since the
     * end of the last test. */
    while (XPending(linux_x11_display())) {
        XEvent event;
        XNextEvent(linux_x11_display(), &event);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
    sys_input_cleanup();

    clear_io_wrapper_variables();
    clear_x11_wrapper_variables();

    CHECK_INTEQUAL(joystick_fd, -1);
    CHECK_FALSE(devinput_dir_open);
    CHECK_INTEQUAL(inotify_fd, -1);
    CHECK_INTEQUAL(inotify_send_fd, -1);

    return 1;
}

/*************************************************************************/
/*************************** Tests: Basic tests **************************/
/*************************************************************************/

TEST(test_init_memory_failure)
{
    sys_input_cleanup();
    CHECK_MEMORY_FAILURES(sys_input_init(event_callback));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_info)
{
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_FALSE(info.has_joystick);
    CHECK_TRUE(info.has_keyboard);
    CHECK_TRUE(info.keyboard_is_full);
    CHECK_TRUE(info.has_mouse);
    CHECK_TRUE(info.has_text);
    CHECK_FALSE(info.text_uses_custom_interface);
    CHECK_FALSE(info.text_has_prompt);
    if (linux_x11_touchscreen_present()) {
        CHECK_TRUE(info.has_touch);
    } else {
        CHECK_FALSE(info.has_touch);
    }

    return 1;
}

/*************************************************************************/
/************************* Tests: Joystick input *************************/
/*************************************************************************/

TEST(test_joystick_initially_connected)
{
    sys_input_cleanup();

    joystick_connected = 1;

    sys_test_time_set_seconds(1.0);
    CHECK_TRUE(sys_input_init(event_callback));

    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_TRUE(info.joysticks);
    CHECK_TRUE(info.joysticks[0].connected);
    CHECK_TRUE(info.joysticks[0].can_rumble);
    CHECK_INTEQUAL(info.joysticks[0].num_buttons, 3);
    CHECK_INTEQUAL(info.joysticks[0].num_sticks, 1);

    char *name = sys_input_joystick_copy_name(0);
    CHECK_STREQUAL(name, "");
    mem_free(name);

    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_CONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_readdir_no_dev_input)
{
    sys_input_cleanup();

    joystick_connected = 1;
    devinput_opendir_errno = ENOENT;

    CHECK_TRUE(sys_input_init(event_callback));

    SysInputInfo info;
    sys_input_info(&info);
    CHECK_FALSE(info.has_joystick);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_readdir_bad_event_device_name)
{
    sys_input_cleanup();

    joystick_connected = 1;
    devinput_readdir_override = "event1a";

    CHECK_TRUE(sys_input_init(event_callback));

    SysInputInfo info;
    sys_input_info(&info);
    CHECK_FALSE(info.has_joystick);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_readdir_event_index_out_of_range)
{
    sys_input_cleanup();

    joystick_connected = 1;
    devinput_readdir_override = OUT_OF_RANGE_EVENT_DEVICE;

    CHECK_TRUE(sys_input_init(event_callback));

    SysInputInfo info;
    sys_input_info(&info);
    CHECK_FALSE(info.has_joystick);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_initially_connected_memory_failure)
{
    sys_input_cleanup();

    joystick_connected = 1;

    sys_test_time_set_seconds(1.0);
    SysInputInfo info;
    /* We need to make sure to reinitialize the input subsystem before
     * returning failure because the test cleanup routine will call
     * sys_input_cleanup(), which we're not allowed to call twice in
     * succession without an intervening successful sys_input_init() call. */
    #undef FAIL_ACTION
    #define FAIL_ACTION  ASSERT(sys_input_init(event_callback)); return 0
    CHECK_MEMORY_FAILURES((num_events = 0, sys_input_init(event_callback)
                           && ((sys_input_info(&info), info.has_joystick)
                               || (sys_input_cleanup(), 0))));
    #undef FAIL_ACTION
    #define FAIL_ACTION  return 0

    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_TRUE(info.joysticks);
    CHECK_TRUE(info.joysticks[0].connected);
    CHECK_TRUE(info.joysticks[0].can_rumble);
    CHECK_INTEQUAL(info.joysticks[0].num_buttons, 3);
    CHECK_INTEQUAL(info.joysticks[0].num_sticks, 1);

    char *name = sys_input_joystick_copy_name(0);
    CHECK_STREQUAL(name, "");
    mem_free(name);

    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_CONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_open_error)
{
    sys_input_cleanup();

    joystick_fd_errno = ENODEV;
    joystick_connected = 1;
    CHECK_TRUE(sys_input_init(event_callback));
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_FALSE(info.has_joystick);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_open_readonly)
{
    sys_input_cleanup();

    joystick_fd_errno = EACCES; // Will cause the device to be opened readonly.
    joystick_connected = 1;
    sys_test_time_set_seconds(1.0);
    CHECK_TRUE(sys_input_init(event_callback));

    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_TRUE(info.joysticks);
    CHECK_TRUE(info.joysticks[0].connected);
    CHECK_FALSE(info.joysticks[0].can_rumble);  // Read-only, so no rumble.
    CHECK_INTEQUAL(info.joysticks[0].num_buttons, 3);
    CHECK_INTEQUAL(info.joysticks[0].num_sticks, 1);

    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_CONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);

    char *name = sys_input_joystick_copy_name(0);
    CHECK_STREQUAL(name, "");
    mem_free(name);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_open_fcntl_error)
{
    sys_input_cleanup();

    joystick_fcntl_errno = EIO;
    joystick_connected = 1;
    CHECK_TRUE(sys_input_init(event_callback));
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_FALSE(info.has_joystick);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_ioctl_fail_GBIT_0)
{
    sys_input_cleanup();

    joystick_fail_ioctl = EVIOCGBIT(0, 0);
    joystick_connected = 1;
    CHECK_TRUE(sys_input_init(event_callback));
    SysInputInfo info;
    sys_input_info(&info);
    /* Failure to retrieve base EV_* bits will cause the device to not be
     * detected as a joystick. */
    CHECK_FALSE(info.has_joystick);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_ioctl_fail_GBIT_KEY)
{
    sys_input_cleanup();

    joystick_fail_ioctl = EVIOCGBIT(EV_KEY, 0);
    joystick_connected = 1;
    CHECK_TRUE(sys_input_init(event_callback));
    SysInputInfo info;
    sys_input_info(&info);
    /* With X/Y absolute inputs but no key inputs, the device will not be
     * detected as a joystick. */
    CHECK_FALSE(info.has_joystick);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_ioctl_fail_GBIT_ABS)
{
    sys_input_cleanup();

    joystick_fail_ioctl = EVIOCGBIT(EV_ABS, 0);
    joystick_connected = 1;
    CHECK_TRUE(sys_input_init(event_callback));
    SysInputInfo info;
    sys_input_info(&info);
    /* With no absolute inputs and no D-pad buttons detected, the device
     * will not be detected as a joystick. */
    CHECK_FALSE(info.has_joystick);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_ioctl_fail_GBIT_FF)
{
    sys_input_cleanup();

    joystick_fail_ioctl = EVIOCGBIT(EV_FF, 0);
    joystick_connected = 1;
    CHECK_TRUE(sys_input_init(event_callback));
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_FALSE(info.joysticks[0].can_rumble);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_ioctl_fail_GNAME)
{
    sys_input_cleanup();

    joystick_fail_ioctl = EVIOCGNAME(0);
    ASSERT(strformat_check(joystick_name, sizeof(joystick_name), "test"));
    joystick_connected = 1;
    CHECK_TRUE(sys_input_init(event_callback));
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);

    char *name = sys_input_joystick_copy_name(0);
    CHECK_STREQUAL(name, "");
    mem_free(name);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_ioctl_fail_GID)
{
    sys_input_cleanup();

    joystick_fail_ioctl = EVIOCGID;
    joystick_connected = 1;
    CHECK_TRUE(sys_input_init(event_callback));
    SysInputInfo info;
    sys_input_info(&info);
    /* We can't detect the effect of this failure (yet -- we test it again
     * in the reconnect tests below), but the joystick should still be
     * accepted. */
    CHECK_TRUE(info.has_joystick);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_pipe_failure)
{
    sys_input_cleanup();

    int fds[8192];  // Enough, I hope?
    int num_fds = 0;
    for (int fd; (fd = open("/dev/null", O_RDONLY)) != -1; ) {
        ASSERT(num_fds < lenof(fds));
        fds[num_fds++] = fd;
    }
    /* We need to leave 4 file descriptors open: 1 for the joystick device
     * opened during the readdir() loop, and 3 so the inotify simulation
     * pipe gets properly created in our inotify_add_watch() wrapper.  This
     * will leave 1 file descriptor open at the point of the pipe() call
     * for the joystick scanning thread, so pipe() will fail. */
    ASSERT(num_fds >= 3);
    ASSERT(close(fds[--num_fds]) == 0);
    ASSERT(close(fds[--num_fds]) == 0);
    ASSERT(close(fds[--num_fds]) == 0);
    ASSERT(close(fds[--num_fds]) == 0);

    joystick_connected = 1;

    CHECK_TRUE(sys_input_init(event_callback));

    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_INTEQUAL(inotify_fd, -1);

    for (int i = 0; i < num_fds; i++) {
        ASSERT(close(fds[i]) == 0);
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_connect)
{
    sys_input_cleanup();

    sys_test_time_set_seconds(1.0);
    CHECK_TRUE(sys_input_init(event_callback));

    SysInputInfo info;
    sys_input_info(&info);
    CHECK_FALSE(info.has_joystick);

    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);

    sys_test_time_set_seconds(2.0);
    joystick_connected = 1;
    CHECK_TRUE(inotify_send_fd >= 0);
    write_inotify(IN_CREATE, "event1", 1);

    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_CONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);

    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_TRUE(info.joysticks);
    CHECK_TRUE(info.joysticks[0].connected);
    CHECK_TRUE(info.joysticks[0].can_rumble);
    CHECK_INTEQUAL(info.joysticks[0].num_buttons, 3);
    CHECK_INTEQUAL(info.joysticks[0].num_sticks, 1);

    char *name = sys_input_joystick_copy_name(0);
    CHECK_STREQUAL(name, "");
    mem_free(name);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_inotify_after_connect)
{
    sys_input_cleanup();

    sys_test_time_set_seconds(1.0);
    CHECK_TRUE(sys_input_init(event_callback));

    SysInputInfo info;
    sys_input_info(&info);
    CHECK_FALSE(info.has_joystick);

    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);

    sys_test_time_set_seconds(2.0);
    joystick_connected = 1;
    CHECK_TRUE(inotify_send_fd >= 0);
    write_inotify(IN_CREATE, "event1", 1);

    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_CONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);

    /* This should not generate an additional event. */
    num_events = 0;
    write_inotify(IN_ATTRIB, "event1", 1);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_disconnect_inotify)
{
    sys_input_cleanup();

    joystick_connected = 1;

    sys_test_time_set_seconds(1.0);
    CHECK_TRUE(sys_input_init(event_callback));
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_TRUE(info.joysticks[0].connected);

    sys_test_time_set_seconds(2.0);
    num_events = 0;
    joystick_connected = 0;
    CHECK_TRUE(inotify_send_fd >= 0);
    write_inotify(IN_DELETE, "event1", 1);

    sys_input_update();
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_FALSE(info.joysticks[0].connected);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DISCONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_disconnect_read)
{
    sys_input_cleanup();

    joystick_connected = 1;

    sys_test_time_set_seconds(1.0);
    CHECK_TRUE(sys_input_init(event_callback));
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_TRUE(info.joysticks[0].connected);

    sys_test_time_set_seconds(2.0);
    num_events = 0;
    joystick_fd_errno = ENODEV;
    sys_input_update();
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_FALSE(info.joysticks[0].connected);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DISCONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);

    /* The inotify event should not trigger a second disconnect event. */
    sys_test_time_set_seconds(3.0);
    num_events = 0;
    joystick_connected = 0;
    CHECK_TRUE(inotify_send_fd >= 0);
    write_inotify(IN_DELETE, "event1", 1);
    sys_input_update();
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_FALSE(info.joysticks[0].connected);
    CHECK_INTEQUAL(num_events, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_inotify_read_error)
{
    sys_input_cleanup();

    joystick_connected = 1;

    CHECK_TRUE(sys_input_init(event_callback));
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_TRUE(info.joysticks[0].connected);

    num_events = 0;
    inotify_fd_errno = ENODEV;
    joystick_connected = 0;
    CHECK_TRUE(inotify_send_fd >= 0);
    /* The data will never be read, so don't try to wait for the read()
     * call.  Instead, sleep for long enough that the inotify thread
     * should have a chance to detect the error. */
    write_inotify(IN_DELETE, "event1", 0);
    nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = 10*1000*1000}, NULL);

    sys_input_update();
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_TRUE(info.joysticks[0].connected);
    CHECK_INTEQUAL(num_events, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_inotify_interrupt)
{
    sys_input_cleanup();

    joystick_connected = 1;

    sys_test_time_set_seconds(1.0);
    CHECK_TRUE(sys_input_init(event_callback));
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_TRUE(info.joysticks[0].connected);

    sys_test_time_set_seconds(2.0);
    num_events = 0;
    inotify_fd_errno = EINTR;
    joystick_connected = 0;
    CHECK_TRUE(inotify_send_fd >= 0);
    write_inotify(IN_DELETE, "event1", 1);

    sys_input_update();
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_FALSE(info.joysticks[0].connected);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DISCONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_inotify_bad_event_device_name)
{
    sys_input_cleanup();

    CHECK_TRUE(sys_input_init(event_callback));

    SysInputInfo info;
    sys_input_info(&info);
    CHECK_FALSE(info.has_joystick);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);

    CHECK_TRUE(inotify_send_fd >= 0);
    write_inotify(IN_CREATE, "event1a", 1);  // Should be ignored.

    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);
    sys_input_info(&info);
    CHECK_FALSE(info.has_joystick);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_inotify_non_event_device)
{
    sys_input_cleanup();

    CHECK_TRUE(sys_input_init(event_callback));

    SysInputInfo info;
    sys_input_info(&info);
    CHECK_FALSE(info.has_joystick);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);

    CHECK_TRUE(inotify_send_fd >= 0);
    write_inotify(IN_CREATE, "mouse0", 1);  // Should be ignored.

    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);
    sys_input_info(&info);
    CHECK_FALSE(info.has_joystick);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_inotify_event_index_out_of_range)
{
    sys_input_cleanup();

    CHECK_TRUE(sys_input_init(event_callback));

    SysInputInfo info;
    sys_input_info(&info);
    CHECK_FALSE(info.has_joystick);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);

    joystick_connected = 1;
    CHECK_TRUE(inotify_send_fd >= 0);
    write_inotify(IN_CREATE,
                  OUT_OF_RANGE_EVENT_DEVICE, 1);  // Should be ignored.

    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);
    sys_input_info(&info);
    CHECK_FALSE(info.has_joystick);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_inotify_delete_non_joystick_device)
{
    sys_input_cleanup();

    joystick_connected = 1;

    CHECK_TRUE(sys_input_init(event_callback));
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_TRUE(info.joysticks[0].connected);

    num_events = 0;
    CHECK_TRUE(inotify_send_fd >= 0);
    write_inotify(IN_DELETE, "event0", 1);  // Should be ignored.

    sys_input_update();
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_TRUE(info.joysticks[0].connected);
    CHECK_INTEQUAL(num_events, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_inotify_init_failure)
{
    sys_input_cleanup();

    joystick_connected = 1;
    inotify_fd_errno = ENOSYS;

    CHECK_TRUE(sys_input_init(event_callback));

    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_INTEQUAL(inotify_fd, -1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_reconnect)
{
    sys_input_cleanup();

    sys_test_time_set_seconds(1.0);
    joystick_id.product = 1;
    joystick_connected = 1;
    CHECK_TRUE(sys_input_init(event_callback));
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_TRUE(info.joysticks[0].connected);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_CONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);

    sys_test_time_set_seconds(2.0);
    num_events = 0;
    joystick_id.product = 10;
    joystick_connected = 1;
    /* Our open() wrapper will accept anything beginning with "event1" as
     * the joystick device, but it only has one FD slot, so we need to
     * save the current simulated joystick FD while we add and remove this
     * second device. */
    const int saved_joystick_fd = joystick_fd;
    joystick_fd = -1;
    CHECK_TRUE(inotify_send_fd >= 0);
    write_inotify(IN_CREATE, "event10", 1);
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 2);
    CHECK_TRUE(info.joysticks[0].connected);
    CHECK_TRUE(info.joysticks[1].connected);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_CONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].joystick.device, 1);

    sys_test_time_set_seconds(3.0);
    num_events = 0;
    CHECK_TRUE(inotify_send_fd >= 0);
    write_inotify(IN_DELETE, "event10", 1);
    ASSERT(joystick_fd == -1);
    joystick_fd = saved_joystick_fd;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 2);
    CHECK_TRUE(info.joysticks[0].connected);
    CHECK_FALSE(info.joysticks[1].connected);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DISCONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 3.0);
    CHECK_INTEQUAL(events[0].joystick.device, 1);

    sys_test_time_set_seconds(4.0);
    num_events = 0;
    joystick_connected = 0;
    CHECK_TRUE(inotify_send_fd >= 0);
    write_inotify(IN_DELETE, "event1", 1);
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 2);
    CHECK_FALSE(info.joysticks[0].connected);
    CHECK_FALSE(info.joysticks[1].connected);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DISCONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 4.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);

    sys_test_time_set_seconds(5.0);
    num_events = 0;
    joystick_connected = 1;
    CHECK_TRUE(inotify_send_fd >= 0);
    /* joystick_id.product is still 10, so this should be detected as the
     * second joystick (device index 1) even though the device name is the
     * one that used to be assigned to the first joystick. */
    write_inotify(IN_CREATE, "event1", 1);
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 2);
    CHECK_FALSE(info.joysticks[0].connected);
    CHECK_TRUE(info.joysticks[1].connected);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_CONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 5.0);
    CHECK_INTEQUAL(events[0].joystick.device, 1);

    sys_test_time_set_seconds(6.0);
    num_events = 0;
    joystick_connected = 0;
    CHECK_TRUE(inotify_send_fd >= 0);
    write_inotify(IN_DELETE, "event1", 1);
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 2);
    CHECK_FALSE(info.joysticks[0].connected);
    CHECK_FALSE(info.joysticks[1].connected);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DISCONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 6.0);
    CHECK_INTEQUAL(events[0].joystick.device, 1);

    sys_test_time_set_seconds(7.0);
    num_events = 0;
    joystick_id.product = 2;
    joystick_connected = 1;
    CHECK_TRUE(inotify_send_fd >= 0);
    /* This product ID isn't known, so it should overwrite the first entry
     * in the joystick table. */
    write_inotify(IN_CREATE, "event1", 1);
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 2);
    CHECK_TRUE(info.joysticks[0].connected);
    CHECK_FALSE(info.joysticks[1].connected);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_CONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 7.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_reconnect_ioctl_fail_GID)
{
    sys_input_cleanup();

    /* Everything through time 4.0 is identical to the joystick_reconnect
     * test. */
    sys_test_time_set_seconds(1.0);
    joystick_id.product = 1;
    joystick_connected = 1;
    CHECK_TRUE(sys_input_init(event_callback));
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_TRUE(info.joysticks[0].connected);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_CONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);

    sys_test_time_set_seconds(2.0);
    num_events = 0;
    joystick_id.product = 10;
    joystick_connected = 1;
    const int saved_joystick_fd = joystick_fd;
    joystick_fd = -1;
    CHECK_TRUE(inotify_send_fd >= 0);
    write_inotify(IN_CREATE, "event10", 1);
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 2);
    CHECK_TRUE(info.joysticks[0].connected);
    CHECK_TRUE(info.joysticks[1].connected);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_CONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].joystick.device, 1);

    sys_test_time_set_seconds(3.0);
    num_events = 0;
    CHECK_TRUE(inotify_send_fd >= 0);
    write_inotify(IN_DELETE, "event10", 1);
    ASSERT(joystick_fd == -1);
    joystick_fd = saved_joystick_fd;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 2);
    CHECK_TRUE(info.joysticks[0].connected);
    CHECK_FALSE(info.joysticks[1].connected);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DISCONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 3.0);
    CHECK_INTEQUAL(events[0].joystick.device, 1);

    sys_test_time_set_seconds(4.0);
    num_events = 0;
    joystick_connected = 0;
    CHECK_TRUE(inotify_send_fd >= 0);
    write_inotify(IN_DELETE, "event1", 1);
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 2);
    CHECK_FALSE(info.joysticks[0].connected);
    CHECK_FALSE(info.joysticks[1].connected);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DISCONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 4.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);

    sys_test_time_set_seconds(5.0);
    num_events = 0;
    joystick_fail_ioctl = EVIOCGID;
    joystick_connected = 1;
    CHECK_TRUE(inotify_send_fd >= 0);
    write_inotify(IN_CREATE, "event1", 1);
    /* The ioctl() failure should have prevented the joystick code from
     * reading the product ID, so the device should be treated as unknown
     * and assigned to the first slot. */
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 2);
    CHECK_TRUE(info.joysticks[0].connected);
    CHECK_FALSE(info.joysticks[1].connected);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_CONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 5.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_detect_dpad_only)
{
    sys_input_cleanup();

    setup_dpad_controller();
    /* Give it a solitary X axis input to exercise the "requires both X
     * and Y axes" test. */
    SET_BIT(joystick_ev_bits, EV_ABS);
    SET_BIT(joystick_ev_abs, ABS_X);
    joystick_connected = 1;

    sys_test_time_set_seconds(1.0);
    CHECK_TRUE(sys_input_init(event_callback));

    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_TRUE(info.joysticks);
    CHECK_TRUE(info.joysticks[0].connected);
    CHECK_FALSE(info.joysticks[0].can_rumble);
    CHECK_INTEQUAL(info.joysticks[0].num_buttons, 4);  // Not 8!
    CHECK_INTEQUAL(info.joysticks[0].num_sticks, 1);  // The X axis we put in.

    char *name = sys_input_joystick_copy_name(0);
    CHECK_STREQUAL(name, "SIL test gamepad");
    mem_free(name);

    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_CONNECTED);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_copy_name_memory_failure)
{
    sys_input_cleanup();

    ASSERT(strformat_check(joystick_name, sizeof(joystick_name), "Test Name"));
    joystick_connected = 1;

    CHECK_TRUE(sys_input_init(event_callback));

    char *name;
    CHECK_MEMORY_FAILURES(name = sys_input_joystick_copy_name(0));
    CHECK_STREQUAL(name, "Test Name");
    mem_free(name);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_copy_name_disconnected)
{
    sys_input_cleanup();

    ASSERT(strformat_check(joystick_name, sizeof(joystick_name), "Test Name"));
    joystick_connected = 1;

    CHECK_TRUE(sys_input_init(event_callback));

    joystick_connected = 0;
    CHECK_TRUE(inotify_send_fd >= 0);
    write_inotify(IN_DELETE, "event1", 1);
    sys_input_update();
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.num_joysticks, 1);
    CHECK_FALSE(info.joysticks[0].connected);

    CHECK_FALSE(sys_input_joystick_copy_name(0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_button_map_none)
{
    sys_input_cleanup();

    joystick_connected = 1;

    CHECK_TRUE(sys_input_init(event_callback));
    sys_input_update();
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);

    for (int i = 0; i < INPUT_JOYBUTTON__NUM; i++) {
        CHECK_INTEQUAL(sys_input_joystick_button_mapping(0, i), -1);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_button_map_gamepad)
{
    sys_input_cleanup();

    setup_x360_controller();
    /* Prevent it from being recognized by the joystick database. */
    mem_clear(&joystick_id, sizeof(joystick_id));
    mem_clear(joystick_name, sizeof(joystick_name));
    /* Add L2/R2 buttons so we can test those assignments. */
    SET_BIT(joystick_ev_key, BTN_TL2);
    SET_BIT(joystick_ev_key, BTN_TR2);
    joystick_connected = 1;

    CHECK_TRUE(sys_input_init(event_callback));

    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_HOME), 10);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_START), 9);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_SELECT), 8);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_FACE_UP), 2);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_FACE_LEFT), 3);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_FACE_RIGHT), 1);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_FACE_DOWN), 0);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_L1), 4);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_R1), 5);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_L2), 6);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_R2), 7);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_L_STICK), 11);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_R_STICK), 12);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_button_map_from_db)
{
    sys_input_cleanup();

    setup_ps3_controller();
    joystick_connected = 1;

    CHECK_TRUE(sys_input_init(event_callback));

    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_HOME), 16);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_START), 3);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_SELECT), 0);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_FACE_UP), 12);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_FACE_LEFT), 15);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_FACE_RIGHT), 13);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_FACE_DOWN), 14);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_L1), 10);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_R1), 11);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_L2), 8);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_R2), 9);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_L_STICK), 1);
    CHECK_INTEQUAL(sys_input_joystick_button_mapping(
                       0, INPUT_JOYBUTTON_R_STICK), 2);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_button_map_disconnected)
{
    sys_input_cleanup();
    setup_ps3_controller();
    joystick_connected = 1;
    CHECK_TRUE(sys_input_init(event_callback));

    joystick_connected = 0;
    CHECK_TRUE(inotify_send_fd >= 0);
    write_inotify(IN_DELETE, "event1", 1);
    sys_input_update();

    for (int i = 0; i < INPUT_JOYBUTTON__NUM; i++) {
        CHECK_INTEQUAL(sys_input_joystick_button_mapping(0, i), -1);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_rumble)
{
    sys_input_cleanup();
    joystick_connected = 1;
    CHECK_TRUE(sys_input_init(event_callback));

    sys_input_joystick_rumble(0, 1, 0.6, 1);
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double now = tv.tv_sec + tv.tv_usec * 1.0e-6;
    CHECK_TRUE(ff_effect_defined);
    CHECK_INTEQUAL(ff_effect.type, FF_RUMBLE);
    CHECK_INTEQUAL(ff_effect.replay.length, 1000);
    CHECK_INTEQUAL(ff_effect.replay.delay, 0);
    CHECK_INTEQUAL(ff_effect.u.rumble.strong_magnitude, 0xCCCC);
    CHECK_INTEQUAL(ff_effect.u.rumble.weak_magnitude, 0xCCCC);
    CHECK_TRUE(ff_event.time.tv_sec + ff_event.time.tv_usec * 1.0e-6 <= now);
    CHECK_INTEQUAL(ff_event.type, EV_FF);
    CHECK_INTEQUAL(ff_event.code, ff_effect.id);
    CHECK_INTEQUAL(ff_event.value, 1);

    mem_clear(&ff_event, sizeof(ff_event));
    sys_input_joystick_rumble(0, 0.6, 0.2, 1);
    gettimeofday(&tv, NULL);
    now = tv.tv_sec + tv.tv_usec * 1.0e-6;
    CHECK_TRUE(ff_effect_defined);
    CHECK_INTEQUAL(ff_effect.type, FF_RUMBLE);
    CHECK_INTEQUAL(ff_effect.replay.length, 1000);
    CHECK_INTEQUAL(ff_effect.replay.delay, 0);
    CHECK_INTEQUAL(ff_effect.u.rumble.strong_magnitude, 0x6666);
    CHECK_INTEQUAL(ff_effect.u.rumble.weak_magnitude, 0x6666);
    CHECK_TRUE(ff_event.time.tv_sec + ff_event.time.tv_usec * 1.0e-6 <= now);
    CHECK_INTEQUAL(ff_event.type, EV_FF);
    CHECK_INTEQUAL(ff_event.code, ff_effect.id);
    CHECK_INTEQUAL(ff_event.value, 1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_rumble_left_strong)
{
    sys_input_cleanup();
    setup_x360_controller();
    joystick_connected = 1;
    CHECK_TRUE(sys_input_init(event_callback));

    sys_input_joystick_rumble(0, 1, 0.6, 1);
    CHECK_TRUE(ff_effect_defined);
    CHECK_INTEQUAL(ff_effect.type, FF_RUMBLE);
    CHECK_INTEQUAL(ff_effect.u.rumble.strong_magnitude, 0xFFFF);
    CHECK_INTEQUAL(ff_effect.u.rumble.weak_magnitude, 0x9999);
    CHECK_INTEQUAL(ff_event.type, EV_FF);
    CHECK_INTEQUAL(ff_event.code, ff_effect.id);
    CHECK_INTEQUAL(ff_event.value, 1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_rumble_right_strong)
{
    sys_input_cleanup();
    setup_ps3_controller();
    joystick_connected = 1;
    CHECK_TRUE(sys_input_init(event_callback));

    sys_input_joystick_rumble(0, 1, 0.6, 1);
    CHECK_TRUE(ff_effect_defined);
    CHECK_INTEQUAL(ff_effect.type, FF_RUMBLE);
    CHECK_INTEQUAL(ff_effect.u.rumble.strong_magnitude, 0x9999);
    CHECK_INTEQUAL(ff_effect.u.rumble.weak_magnitude, 0xFFFF);
    CHECK_INTEQUAL(ff_event.type, EV_FF);
    CHECK_INTEQUAL(ff_event.code, ff_effect.id);
    CHECK_INTEQUAL(ff_event.value, 1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_rumble_sff_error)
{
    sys_input_cleanup();
    joystick_connected = 1;
    CHECK_TRUE(sys_input_init(event_callback));

    joystick_fd_errno = ENODEV;
    sys_input_joystick_rumble(0, 1, 0.6, 1);
    CHECK_FALSE(ff_effect_defined);
    CHECK_INTEQUAL(ff_event.type, 0);
    CHECK_INTEQUAL(ff_event.code, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_rumble_rmff_error)
{
    sys_input_cleanup();
    joystick_connected = 1;
    CHECK_TRUE(sys_input_init(event_callback));

    sys_input_joystick_rumble(0, 1, 0.6, 1);
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double now = tv.tv_sec + tv.tv_usec * 1.0e-6;
    CHECK_TRUE(ff_effect_defined);
    CHECK_INTEQUAL(ff_effect.type, FF_RUMBLE);
    CHECK_INTEQUAL(ff_effect.replay.length, 1000);
    CHECK_INTEQUAL(ff_effect.replay.delay, 0);
    CHECK_INTEQUAL(ff_effect.u.rumble.strong_magnitude, 0xCCCC);
    CHECK_INTEQUAL(ff_effect.u.rumble.weak_magnitude, 0xCCCC);
    CHECK_TRUE(ff_event.time.tv_sec + ff_event.time.tv_usec * 1.0e-6 <= now);
    CHECK_INTEQUAL(ff_event.type, EV_FF);
    CHECK_INTEQUAL(ff_event.code, ff_effect.id);
    CHECK_INTEQUAL(ff_event.value, 1);

    const int last_id = ff_effect.id;
    mem_clear(&ff_event, sizeof(ff_event));
    joystick_fd_errno = ENODEV;
    sys_input_joystick_rumble(0, 0.6, 0.2, 1);
    CHECK_TRUE(ff_effect_defined);
    CHECK_INTEQUAL(ff_effect.id, last_id);
    CHECK_INTEQUAL(ff_event.type, 0);
    CHECK_INTEQUAL(ff_event.code, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_rumble_disconnected)
{
    sys_input_cleanup();
    joystick_connected = 1;
    CHECK_TRUE(sys_input_init(event_callback));

    joystick_connected = 0;
    CHECK_TRUE(inotify_send_fd >= 0);
    write_inotify(IN_DELETE, "event1", 1);
    sys_input_update();

    sys_input_joystick_rumble(0, 1, 1, 1);
    CHECK_FALSE(ff_effect_defined);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_rumble_effect_not_supported)
{
    sys_input_cleanup();

    mem_clear(joystick_ev_ff, sizeof(joystick_ev_ff));
    SET_BIT(joystick_ev_ff, FF_PERIODIC);  // Arbitrary non-FF_RUMBLE bit.
    joystick_connected = 1;

    CHECK_TRUE(sys_input_init(event_callback));
    sys_input_update();
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_FALSE(info.joysticks[0].can_rumble);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_key_input)
{
    sys_input_cleanup();
    joystick_connected = 1;
    CHECK_TRUE(sys_input_init(event_callback));

    /* The timestamp from the test sys_time implementation won't be used;
     * we set it anyway to help detect failures. */
    sys_test_time_set_seconds(123.0);

    num_events = 0;
    reset_joystick_events();
    joystick_events[0].type = EV_KEY;
    joystick_events[0].code = BTN_JOYSTICK+0;
    joystick_events[0].value = 1;
    joystick_events[1].type = EV_KEY;
    joystick_events[1].code = KEY_OK;  // Should not become a button event.
    joystick_events[1].value = 1;
    joystick_events[2].type = EV_KEY;
    joystick_events[2].code = BTN_TRIGGER_HAPPY+1;
    joystick_events[2].value = 1;
    joystick_events[3].type = EV_KEY;
    joystick_events[3].code = BTN_JOYSTICK+0;
    joystick_events[3].value = 0;
    joystick_events[4].type = EV_KEY;
    joystick_events[4].code = BTN_JOYSTICK+2;
    joystick_events[4].value = 1;
    num_joystick_events = 5;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 4);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index, 0);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 3.0);
    CHECK_INTEQUAL(events[1].joystick.device, 0);
    CHECK_INTEQUAL(events[1].joystick.index, 2);
    CHECK_INTEQUAL(events[2].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[2].detail, INPUT_JOYSTICK_BUTTON_UP);
    CHECK_DOUBLEEQUAL(events[2].timestamp, 4.0);
    CHECK_INTEQUAL(events[2].joystick.device, 0);
    CHECK_INTEQUAL(events[2].joystick.index, 0);
    CHECK_INTEQUAL(events[3].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[3].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[3].timestamp, 5.0);
    CHECK_INTEQUAL(events[3].joystick.device, 0);
    CHECK_INTEQUAL(events[3].joystick.index, 1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_ioctl_sclockid_failure)
{
    sys_input_cleanup();
    joystick_fail_ioctl = EVIOCSCLOCKID;
    joystick_connected = 1;
    CHECK_TRUE(sys_input_init(event_callback));

    sys_test_time_set_seconds(123.0);  // This value _is_ used for this test.
    num_events = 0;
    reset_joystick_events();
    joystick_events[0].type = EV_KEY;
    joystick_events[0].code = BTN_JOYSTICK+0;
    joystick_events[0].value = 1;
    num_joystick_events = 1;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 123.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_dpad_key_input)
{
    sys_input_cleanup();
    setup_dpad_controller();
    joystick_connected = 1;
    CHECK_TRUE(sys_input_init(event_callback));
    sys_test_time_set_seconds(123.0);

    num_events = 0;
    reset_joystick_events();
    joystick_events[0].type = EV_KEY;
    joystick_events[0].code = BTN_DPAD_UP;
    joystick_events[0].value = 1;
    num_joystick_events = 1;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_FLOATEQUAL(events[0].joystick.x, 0);
    CHECK_FLOATEQUAL(events[0].joystick.y, -1);

    num_events = 0;
    reset_joystick_events();
    joystick_events[0].type = EV_KEY;
    joystick_events[0].code = BTN_DPAD_LEFT;
    joystick_events[0].value = 1;
    num_joystick_events = 1;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_FLOATEQUAL(events[0].joystick.x, -1);
    CHECK_FLOATEQUAL(events[0].joystick.y, -1);

    num_events = 0;
    reset_joystick_events();
    joystick_events[0].type = EV_KEY;
    joystick_events[0].code = BTN_DPAD_LEFT;
    joystick_events[0].value = 0;
    num_joystick_events = 1;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_FLOATEQUAL(events[0].joystick.x, 0);
    CHECK_FLOATEQUAL(events[0].joystick.y, -1);

    num_events = 0;
    reset_joystick_events();
    joystick_events[0].type = EV_KEY;
    /* The code should be able to handle opposite-direction buttons being
     * down at the same time. */
    joystick_events[0].code = BTN_DPAD_DOWN;
    joystick_events[0].value = 1;
    num_joystick_events = 1;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_FLOATEQUAL(events[0].joystick.x, 0);
    CHECK_FLOATEQUAL(events[0].joystick.y, 0);

    num_events = 0;
    reset_joystick_events();
    joystick_events[0].type = EV_KEY;
    joystick_events[0].code = BTN_DPAD_UP;
    joystick_events[0].value = 0;
    num_joystick_events = 1;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_FLOATEQUAL(events[0].joystick.x, 0);
    CHECK_FLOATEQUAL(events[0].joystick.y, 1);

    num_events = 0;
    reset_joystick_events();
    joystick_events[0].type = EV_KEY;
    joystick_events[0].code = BTN_DPAD_RIGHT;
    joystick_events[0].value = 1;
    num_joystick_events = 1;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_FLOATEQUAL(events[0].joystick.x, 1);
    CHECK_FLOATEQUAL(events[0].joystick.y, 1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_abs_input)
{
    sys_input_cleanup();
    joystick_connected = 1;
    CHECK_TRUE(sys_input_init(event_callback));
    sys_test_time_set_seconds(123.0);

    num_events = 0;
    reset_joystick_events();
    joystick_events[0].type = EV_ABS;
    joystick_events[0].code = ABS_X;
    joystick_events[0].value = 0;
    joystick_events[1].type = EV_ABS;
    joystick_events[1].code = ABS_Y;
    joystick_events[1].value = 127;
    joystick_events[2].type = EV_ABS;
    joystick_events[2].code = ABS_VOLUME;  // Should not become a stick event.
    joystick_events[2].value = 128;
    num_joystick_events = 3;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index, 0);
    CHECK_FLOATEQUAL(events[0].joystick.x, -1);
    CHECK_FLOATEQUAL(events[0].joystick.y, 0);  // Initial value.
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 2.0);
    CHECK_INTEQUAL(events[1].joystick.device, 0);
    CHECK_INTEQUAL(events[1].joystick.index, 0);
    CHECK_FLOATEQUAL(events[1].joystick.x, -1);
    CHECK_FLOATEQUAL(events[1].joystick.y, 1);

    /* Repeated identical inputs should not generate new events. */
    num_events = 0;
    reset_joystick_events();
    joystick_events[0].type = EV_ABS;
    joystick_events[0].code = ABS_X;
    joystick_events[0].value = 0;
    joystick_events[1].type = EV_ABS;
    joystick_events[1].code = ABS_Y;
    joystick_events[1].value = 127;
    num_joystick_events = 2;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);

    num_events = 0;
    reset_joystick_events();
    joystick_events[0].type = EV_ABS;
    joystick_events[0].code = ABS_X;
    joystick_events[0].value = 255;
    joystick_events[1].type = EV_ABS;
    joystick_events[1].code = ABS_Y;
    joystick_events[1].value = -128;
    num_joystick_events = 2;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index, 0);
    CHECK_FLOATEQUAL(events[0].joystick.x, 1);
    CHECK_FLOATEQUAL(events[0].joystick.y, 1);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 2.0);
    CHECK_INTEQUAL(events[1].joystick.device, 0);
    CHECK_INTEQUAL(events[1].joystick.index, 0);
    CHECK_FLOATEQUAL(events[1].joystick.x, 1);
    CHECK_FLOATEQUAL(events[1].joystick.y, -1);

    num_events = 0;
    reset_joystick_events();
    joystick_events[0].type = EV_ABS;
    joystick_events[0].code = ABS_X;
    joystick_events[0].value = 128;  // No flat range.
    joystick_events[1].type = EV_ABS;
    joystick_events[1].code = ABS_Y;
    joystick_events[1].value = -1;  // Within the flat range.
    num_joystick_events = 2;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index, 0);
    CHECK_FLOATNEAR(events[0].joystick.x, 0.5f/127.5f, 0.1f/127.5f);
    CHECK_FLOATEQUAL(events[0].joystick.y, -1);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 2.0);
    CHECK_INTEQUAL(events[1].joystick.device, 0);
    CHECK_INTEQUAL(events[1].joystick.index, 0);
    CHECK_FLOATNEAR(events[1].joystick.x, 0.5f/127.5f, 0.1f/127.5f);
    CHECK_FLOATEQUAL(events[1].joystick.y, 0);

    num_events = 0;
    reset_joystick_events();
    joystick_events[0].type = EV_ABS;
    joystick_events[0].code = ABS_X;
    joystick_events[0].value = 126;
    joystick_events[1].type = EV_ABS;
    joystick_events[1].code = ABS_Y;
    joystick_events[1].value = 1;  // Just outside the flat range.
    num_joystick_events = 2;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index, 0);
    CHECK_FLOATNEAR(events[0].joystick.x, -1.5f/127.5f, 0.1f/127.5f);
    CHECK_FLOATEQUAL(events[0].joystick.y, 0);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 2.0);
    CHECK_INTEQUAL(events[1].joystick.device, 0);
    CHECK_INTEQUAL(events[1].joystick.index, 0);
    CHECK_FLOATNEAR(events[1].joystick.x, -1.5f/127.5f, 0.1f/127.5f);
    CHECK_FLOATNEAR(events[1].joystick.y, 0.5f/126.5f, 0.1f/126.5f);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_abs_input_merge_axes)
{
    sys_input_cleanup();
    joystick_connected = 1;
    CHECK_TRUE(sys_input_init(event_callback));
    sys_test_time_set_seconds(123.0);

    num_events = 0;
    reset_joystick_events();
    joystick_events[0].type = EV_ABS;
    joystick_events[0].code = ABS_X;
    joystick_events[0].value = 0;
    joystick_events[1].type = EV_ABS;
    joystick_events[1].code = ABS_Y;
    joystick_events[1].value = 127;
    /* Set the two events to the same timestamp so the code will
     * recognize them as eligible for merging. */
    joystick_events[1].time = joystick_events[0].time;
    num_joystick_events = 2;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index, 0);
    CHECK_FLOATEQUAL(events[0].joystick.x, -1);
    CHECK_FLOATEQUAL(events[0].joystick.y, 1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_abs_input_min_max_equal)
{
    sys_input_cleanup();
    joystick_absinfo[ABS_X].minimum = 0;
    joystick_absinfo[ABS_X].maximum = 0;
    joystick_connected = 1;
    CHECK_TRUE(sys_input_init(event_callback));
    sys_test_time_set_seconds(123.0);

    num_events = 0;
    reset_joystick_events();
    joystick_events[0].type = EV_ABS;
    joystick_events[0].code = ABS_X;
    joystick_events[0].value = 0;  // Should not trigger divide-by-zero.
    joystick_events[1].type = EV_ABS;
    joystick_events[1].code = ABS_Y;
    joystick_events[1].value = 127;
    num_joystick_events = 2;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index, 0);
    CHECK_FLOATEQUAL(events[0].joystick.x, 0);
    CHECK_FLOATEQUAL(events[0].joystick.y, 1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_dpad_abs_input)
{
    sys_input_cleanup();
    setup_x360_controller();
    joystick_connected = 1;
    CHECK_TRUE(sys_input_init(event_callback));
    sys_test_time_set_seconds(123.0);

    num_events = 0;
    reset_joystick_events();
    joystick_events[0].type = EV_ABS;
    joystick_events[0].code = ABS_HAT0X;
    joystick_events[0].value = -1;
    joystick_events[1].type = EV_ABS;
    joystick_events[1].code = ABS_HAT0Y;
    joystick_events[1].value = 1;
    num_joystick_events = 2;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_FLOATEQUAL(events[0].joystick.x, -1);
    CHECK_FLOATEQUAL(events[0].joystick.y, 0);  // Initial value.
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 2.0);
    CHECK_INTEQUAL(events[1].joystick.device, 0);
    CHECK_FLOATEQUAL(events[1].joystick.x, -1);
    CHECK_FLOATEQUAL(events[1].joystick.y, 1);

    num_events = 0;
    reset_joystick_events();
    joystick_events[0].type = EV_ABS;
    joystick_events[0].code = ABS_HAT0X;
    joystick_events[0].value = 0;
    joystick_events[1].type = EV_ABS;
    joystick_events[1].code = ABS_HAT0Y;
    joystick_events[1].value = 0;
    num_joystick_events = 2;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_FLOATEQUAL(events[0].joystick.x, 0);
    CHECK_FLOATEQUAL(events[0].joystick.y, 1);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 2.0);
    CHECK_INTEQUAL(events[1].joystick.device, 0);
    CHECK_FLOATEQUAL(events[1].joystick.x, 0);
    CHECK_FLOATEQUAL(events[1].joystick.y, 0);

    num_events = 0;
    reset_joystick_events();
    joystick_events[0].type = EV_ABS;
    joystick_events[0].code = ABS_HAT0X;
    joystick_events[0].value = 1;
    joystick_events[1].type = EV_ABS;
    joystick_events[1].code = ABS_HAT0Y;
    joystick_events[1].value = -1;
    num_joystick_events = 2;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_FLOATEQUAL(events[0].joystick.x, 1);
    CHECK_FLOATEQUAL(events[0].joystick.y, 0);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 2.0);
    CHECK_INTEQUAL(events[1].joystick.device, 0);
    CHECK_FLOATEQUAL(events[1].joystick.x, 1);
    CHECK_FLOATEQUAL(events[1].joystick.y, -1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_trigger_abs_input)
{
    sys_input_cleanup();
    setup_x360_controller();
    joystick_connected = 1;
    CHECK_TRUE(sys_input_init(event_callback));
    sys_test_time_set_seconds(123.0);

    num_events = 0;
    reset_joystick_events();
    joystick_events[0].type = EV_ABS;
    joystick_events[0].code = ABS_Z;
    joystick_events[0].value = 255;
    joystick_events[1].type = EV_ABS;
    joystick_events[1].code = ABS_RZ;
    joystick_events[1].value = 135;  // Just short of the midpoint + debounce.
    num_joystick_events = 2;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index,
                   sys_input_joystick_button_mapping(0, INPUT_JOYBUTTON_L2));

    num_events = 0;
    reset_joystick_events();
    joystick_events[0].type = EV_ABS;
    joystick_events[0].code = ABS_Z;
    joystick_events[0].value = 120;  // Just beyond the midpoint - debounce.
    joystick_events[1].type = EV_ABS;
    joystick_events[1].code = ABS_RZ;
    joystick_events[1].value = 136;
    num_joystick_events = 2;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index,
                   sys_input_joystick_button_mapping(0, INPUT_JOYBUTTON_R2));

    num_events = 0;
    reset_joystick_events();
    joystick_events[0].type = EV_ABS;
    joystick_events[0].code = ABS_Z;
    joystick_events[0].value = 119;
    joystick_events[1].type = EV_ABS;
    joystick_events[1].code = ABS_RZ;
    joystick_events[1].value = 255;
    num_joystick_events = 2;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_UP);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index,
                   sys_input_joystick_button_mapping(0, INPUT_JOYBUTTON_L2));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_unhandled_input)
{
    sys_input_cleanup();
    joystick_connected = 1;
    CHECK_TRUE(sys_input_init(event_callback));
    sys_test_time_set_seconds(123.0);

    num_events = 0;
    reset_joystick_events();
    /* Arbitrary (unhandled) event, which should be ignored. */
    joystick_events[0].type = EV_SYN;
    joystick_events[0].code = SYN_REPORT;
    joystick_events[0].value = 0;
    num_joystick_events = 1;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_default_stick_mapping_zrx)
{
    sys_input_cleanup();
    SET_BIT(joystick_ev_abs, ABS_Z);
    SET_BIT(joystick_ev_abs, ABS_RX);
    joystick_absinfo[ABS_Z] = joystick_absinfo[ABS_X];
    joystick_absinfo[ABS_RX] = joystick_absinfo[ABS_Y];
    joystick_connected = 1;
    CHECK_TRUE(sys_input_init(event_callback));
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.joysticks[0].num_sticks, 2);
    sys_test_time_set_seconds(123.0);

    num_events = 0;
    reset_joystick_events();
    joystick_events[0].type = EV_ABS;
    joystick_events[0].code = ABS_X;
    joystick_events[0].value = 0;
    joystick_events[1].type = EV_ABS;
    joystick_events[1].code = ABS_Y;
    joystick_events[1].value = 127;
    joystick_events[1].time = joystick_events[0].time;
    joystick_events[2].type = EV_ABS;
    joystick_events[2].code = ABS_Z;
    joystick_events[2].value = 255;
    joystick_events[3].type = EV_ABS;
    joystick_events[3].code = ABS_RX;
    joystick_events[3].value = -128;
    joystick_events[3].time = joystick_events[2].time;
    num_joystick_events = 4;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index, 0);
    CHECK_FLOATEQUAL(events[0].joystick.x, -1);
    CHECK_FLOATEQUAL(events[0].joystick.y, 1);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 3.0);
    CHECK_INTEQUAL(events[1].joystick.device, 0);
    CHECK_INTEQUAL(events[1].joystick.index, 1);
    CHECK_FLOATEQUAL(events[1].joystick.x, 1);
    CHECK_FLOATEQUAL(events[1].joystick.y, -1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_default_stick_mapping_zrz)
{
    sys_input_cleanup();
    SET_BIT(joystick_ev_abs, ABS_Z);
    SET_BIT(joystick_ev_abs, ABS_RZ);
    joystick_absinfo[ABS_Z] = joystick_absinfo[ABS_X];
    joystick_absinfo[ABS_RZ] = joystick_absinfo[ABS_Y];
    joystick_connected = 1;
    CHECK_TRUE(sys_input_init(event_callback));
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.joysticks[0].num_sticks, 2);
    sys_test_time_set_seconds(123.0);

    num_events = 0;
    reset_joystick_events();
    joystick_events[0].type = EV_ABS;
    joystick_events[0].code = ABS_X;
    joystick_events[0].value = 0;
    joystick_events[1].type = EV_ABS;
    joystick_events[1].code = ABS_Y;
    joystick_events[1].value = 127;
    joystick_events[1].time = joystick_events[0].time;
    joystick_events[2].type = EV_ABS;
    joystick_events[2].code = ABS_Z;
    joystick_events[2].value = 255;
    joystick_events[3].type = EV_ABS;
    joystick_events[3].code = ABS_RZ;
    joystick_events[3].value = -128;
    joystick_events[3].time = joystick_events[2].time;
    num_joystick_events = 4;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index, 0);
    CHECK_FLOATEQUAL(events[0].joystick.x, -1);
    CHECK_FLOATEQUAL(events[0].joystick.y, 1);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 3.0);
    CHECK_INTEQUAL(events[1].joystick.device, 0);
    CHECK_INTEQUAL(events[1].joystick.index, 1);
    CHECK_FLOATEQUAL(events[1].joystick.x, 1);
    CHECK_FLOATEQUAL(events[1].joystick.y, -1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_default_stick_mapping_rxry)
{
    sys_input_cleanup();
    SET_BIT(joystick_ev_abs, ABS_RX);
    SET_BIT(joystick_ev_abs, ABS_RY);
    joystick_absinfo[ABS_RX] = joystick_absinfo[ABS_X];
    joystick_absinfo[ABS_RY] = joystick_absinfo[ABS_Y];
    joystick_connected = 1;
    CHECK_TRUE(sys_input_init(event_callback));
    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_joystick);
    CHECK_INTEQUAL(info.joysticks[0].num_sticks, 2);
    sys_test_time_set_seconds(123.0);

    num_events = 0;
    reset_joystick_events();
    joystick_events[0].type = EV_ABS;
    joystick_events[0].code = ABS_X;
    joystick_events[0].value = 0;
    joystick_events[1].type = EV_ABS;
    joystick_events[1].code = ABS_Y;
    joystick_events[1].value = 127;
    joystick_events[1].time = joystick_events[0].time;
    joystick_events[2].type = EV_ABS;
    joystick_events[2].code = ABS_RX;
    joystick_events[2].value = 255;
    joystick_events[3].type = EV_ABS;
    joystick_events[3].code = ABS_RY;
    joystick_events[3].value = -128;
    joystick_events[3].time = joystick_events[2].time;
    num_joystick_events = 4;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index, 0);
    CHECK_FLOATEQUAL(events[0].joystick.x, -1);
    CHECK_FLOATEQUAL(events[0].joystick.y, 1);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 3.0);
    CHECK_INTEQUAL(events[1].joystick.device, 0);
    CHECK_INTEQUAL(events[1].joystick.index, 1);
    CHECK_FLOATEQUAL(events[1].joystick.x, 1);
    CHECK_FLOATEQUAL(events[1].joystick.y, -1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_db_dpad_mapping_buttons)
{
    sys_input_cleanup();
    setup_ps3_controller();
    joystick_connected = 1;
    CHECK_TRUE(sys_input_init(event_callback));
    sys_test_time_set_seconds(123.0);

    num_events = 0;
    reset_joystick_events();
    joystick_events[0].type = EV_KEY;
    joystick_events[0].code = BTN_JOYSTICK+4;
    joystick_events[0].value = 1;
    num_joystick_events = 1;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index, 4);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 1.0);
    CHECK_INTEQUAL(events[1].joystick.device, 0);
    CHECK_FLOATEQUAL(events[1].joystick.x, 0);
    CHECK_FLOATEQUAL(events[1].joystick.y, -1);

    num_events = 0;
    reset_joystick_events();
    joystick_events[0].type = EV_KEY;
    joystick_events[0].code = BTN_JOYSTICK+5;
    joystick_events[0].value = 1;
    num_joystick_events = 1;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index, 5);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 1.0);
    CHECK_INTEQUAL(events[1].joystick.device, 0);
    CHECK_FLOATEQUAL(events[1].joystick.x, 1);
    CHECK_FLOATEQUAL(events[1].joystick.y, -1);

    num_events = 0;
    reset_joystick_events();
    joystick_events[0].type = EV_KEY;
    joystick_events[0].code = BTN_JOYSTICK+6;
    joystick_events[0].value = 1;
    num_joystick_events = 1;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index, 6);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 1.0);
    CHECK_INTEQUAL(events[1].joystick.device, 0);
    CHECK_FLOATEQUAL(events[1].joystick.x, 1);
    CHECK_FLOATEQUAL(events[1].joystick.y, 0);

    num_events = 0;
    reset_joystick_events();
    joystick_events[0].type = EV_KEY;
    joystick_events[0].code = BTN_JOYSTICK+7;
    joystick_events[0].value = 1;
    num_joystick_events = 1;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index, 7);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 1.0);
    CHECK_INTEQUAL(events[1].joystick.device, 0);
    CHECK_FLOATEQUAL(events[1].joystick.x, 0);
    CHECK_FLOATEQUAL(events[1].joystick.y, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_db_button_mapping_trigger_happy)
{
    sys_input_cleanup();
    ASSERT(strformat_check(joystick_name, sizeof(joystick_name),
                           "Linux test"));
    joystick_connected = 1;
    CHECK_TRUE(sys_input_init(event_callback));
    sys_test_time_set_seconds(123.0);

    num_events = 0;
    reset_joystick_events();
    joystick_events[0].type = EV_KEY;
    joystick_events[0].code = BTN_TRIGGER_HAPPY+1;
    joystick_events[0].value = 1;
    num_joystick_events = 1;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index, 2);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_DPAD_CHANGE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 1.0);
    CHECK_INTEQUAL(events[1].joystick.device, 0);
    CHECK_FLOATEQUAL(events[1].joystick.x, -1);
    CHECK_FLOATEQUAL(events[1].joystick.y, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_db_stick_mapping)
{
    sys_input_cleanup();
    setup_x360_controller();
    joystick_connected = 1;
    CHECK_TRUE(sys_input_init(event_callback));
    sys_test_time_set_seconds(123.0);

    num_events = 0;
    reset_joystick_events();
    joystick_events[0].type = EV_ABS;
    joystick_events[0].code = ABS_X;
    joystick_events[0].value = -32768;
    joystick_events[1].type = EV_ABS;
    joystick_events[1].code = ABS_Y;
    joystick_events[1].value = 32767;
    joystick_events[1].time = joystick_events[0].time;
    joystick_events[2].type = EV_ABS;
    joystick_events[2].code = ABS_RX;
    joystick_events[2].value = 32767;
    joystick_events[3].type = EV_ABS;
    joystick_events[3].code = ABS_RY;
    joystick_events[3].value = -32768;
    joystick_events[3].time = joystick_events[2].time;
    num_joystick_events = 4;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index, 0);
    CHECK_FLOATEQUAL(events[0].joystick.x, -1);
    CHECK_FLOATEQUAL(events[0].joystick.y, 1);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[1].detail, INPUT_JOYSTICK_STICK_CHANGE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 3.0);
    CHECK_INTEQUAL(events[1].joystick.device, 0);
    CHECK_INTEQUAL(events[1].joystick.index, 1);
    CHECK_FLOATEQUAL(events[1].joystick.x, 1);
    CHECK_FLOATEQUAL(events[1].joystick.y, -1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_joystick_enable_unfocused_input)
{
    sys_input_cleanup();
    setup_ps3_controller();
    joystick_connected = 1;
    CHECK_TRUE(sys_input_init(event_callback));
    sys_test_time_set_seconds(123.0);

    XSetInputFocus(linux_x11_display(), linux_x11_window(), RevertToNone,
                   CurrentTime);
    XSync(linux_x11_display(), False);
    while (XPending(linux_x11_display())) {
        XEvent unused_event;
        (void) linux_get_window_event(&unused_event);
    }
    CHECK_TRUE(graphics_has_focus());
    XSetInputFocus(linux_x11_display(), None, RevertToNone, CurrentTime);
    XSync(linux_x11_display(), False);
    while (XPending(linux_x11_display())) {
        XEvent unused_event;
        (void) linux_get_window_event(&unused_event);
    }
    CHECK_FALSE(graphics_has_focus());

    /* By default, we should receive input events while the window is not
     * focused. */
    num_events = 0;
    reset_joystick_events();
    joystick_events[0].type = EV_KEY;
    joystick_events[0].code = BTN_JOYSTICK+1;
    joystick_events[0].value = 1;
    num_joystick_events = 1;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index, 1);

    /* Check that we can disable unfocused input events. */
    sys_input_enable_unfocused_joystick(0);
    num_events = 0;
    reset_joystick_events();
    joystick_events[0].type = EV_KEY;
    joystick_events[0].code = BTN_JOYSTICK+2;
    joystick_events[0].value = 1;
    num_joystick_events = 1;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);

    /* Check that we can re-enable unfocused input events. */
    sys_input_enable_unfocused_joystick(1);
    num_events = 0;
    reset_joystick_events();
    joystick_events[0].type = EV_KEY;
    joystick_events[0].code = BTN_JOYSTICK+3;
    joystick_events[0].value = 1;
    num_joystick_events = 1;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_JOYSTICK);
    CHECK_INTEQUAL(events[0].detail, INPUT_JOYSTICK_BUTTON_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].joystick.device, 0);
    CHECK_INTEQUAL(events[0].joystick.index, 3);

    return 1;
}

/*************************************************************************/
/************************* Tests: Keyboard input *************************/
/*************************************************************************/

TEST(test_key_down)
{
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    XKeyEvent event = {
        .type = KeyPress,
        .display = linux_x11_display(),
        .window = linux_x11_window(),
        .root = RootWindow(linux_x11_display(), linux_x11_screen()),
        .subwindow = None,
        .time = 0,
        .x = 0,
        .y = 0,
        .x_root = 0,
        .y_root = 0,
        .state = 0,
        .keycode = keycode_1,
        .same_screen = True,
    };
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_1);
    CHECK_INTEQUAL(events[0].keyboard.system_key, keycode_1);
    CHECK_FALSE(events[0].keyboard.is_repeat);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_key_down_unsupported)
{
    if (!keycode_unmapped) {
        SKIP("Can't find an unmapped keycode.");
    }

    num_events = 0;
    sys_test_time_set_seconds(1.0);
    XKeyEvent event = {
        .type = KeyPress,
        .display = linux_x11_display(),
        .window = linux_x11_window(),
        .root = RootWindow(linux_x11_display(), linux_x11_screen()),
        .subwindow = None,
        .time = 0,
        .x = 0,
        .y = 0,
        .x_root = 0,
        .y_root = 0,
        .state = 0,
        .keycode = keycode_unmapped,
        .same_screen = True,
    };
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_SYSTEM_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].keyboard.key, 0);
    CHECK_INTEQUAL(events[0].keyboard.system_key, keycode_unmapped);
    CHECK_FALSE(events[0].keyboard.is_repeat);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_key_down_no_keysym)
{
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    XKeyEvent event = {
        .type = KeyPress,
        .display = linux_x11_display(),
        .window = linux_x11_window(),
        .root = RootWindow(linux_x11_display(), linux_x11_screen()),
        .subwindow = None,
        .time = 0,
        .x = 0,
        .y = 0,
        .x_root = 0,
        .y_root = 0,
        .state = 0,
        .keycode = keycode_invalid,
        .same_screen = True,
    };
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_SYSTEM_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].keyboard.key, 0);
    CHECK_INTEQUAL(events[0].keyboard.system_key, keycode_invalid);
    CHECK_FALSE(events[0].keyboard.is_repeat);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_key_up)
{
    /* A KeyRelease for an unpressed key should be ignored. */
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    XKeyEvent event = {
        .type = KeyRelease,
        .display = linux_x11_display(),
        .window = linux_x11_window(),
        .root = RootWindow(linux_x11_display(), linux_x11_screen()),
        .subwindow = None,
        .time = 0,
        .x = 0,
        .y = 0,
        .x_root = 0,
        .y_root = 0,
        .state = 0,
        .keycode = keycode_1,
        .same_screen = True,
    };
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);

    num_events = 0;
    sys_test_time_set_seconds(2.0);
    event.type = KeyPress;
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    /* Make sure to call sys_input_update() once so as not to trigger the
     * KeyRelease hack. */
    XSync(event.display, False);
    sys_input_update();
    event.type = KeyRelease;
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_1);
    CHECK_INTEQUAL(events[0].keyboard.system_key, keycode_1);
    CHECK_FALSE(events[0].keyboard.is_repeat);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[1].detail, INPUT_KEYBOARD_KEY_UP);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 2.0);
    CHECK_INTEQUAL(events[1].keyboard.key, KEY_1);
    CHECK_INTEQUAL(events[1].keyboard.system_key, keycode_1);
    CHECK_FALSE(events[1].keyboard.is_repeat);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_key_up_unsupported)
{
    if (!keycode_unmapped) {
        SKIP("Can't find an unmapped keycode.");
    }

    num_events = 0;
    sys_test_time_set_seconds(1.0);
    XKeyEvent event = {
        .type = KeyRelease,
        .display = linux_x11_display(),
        .window = linux_x11_window(),
        .root = RootWindow(linux_x11_display(), linux_x11_screen()),
        .subwindow = None,
        .time = 0,
        .x = 0,
        .y = 0,
        .x_root = 0,
        .y_root = 0,
        .state = 0,
        .keycode = keycode_unmapped,
        .same_screen = True,
    };
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_SYSTEM_KEY_UP);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].keyboard.key, 0);
    CHECK_INTEQUAL(events[0].keyboard.system_key, keycode_unmapped);
    CHECK_FALSE(events[0].keyboard.is_repeat);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_key_up_no_keysym)
{
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    XKeyEvent event = {
        .type = KeyRelease,
        .display = linux_x11_display(),
        .window = linux_x11_window(),
        .root = RootWindow(linux_x11_display(), linux_x11_screen()),
        .subwindow = None,
        .time = 0,
        .x = 0,
        .y = 0,
        .x_root = 0,
        .y_root = 0,
        .state = 0,
        .keycode = keycode_invalid,
        .same_screen = True,
    };
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_SYSTEM_KEY_UP);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].keyboard.key, 0);
    CHECK_INTEQUAL(events[0].keyboard.system_key, keycode_invalid);
    CHECK_FALSE(events[0].keyboard.is_repeat);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_key_repeat)
{
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    XKeyEvent event = {
        .type = KeyPress,
        .display = linux_x11_display(),
        .window = linux_x11_window(),
        .root = RootWindow(linux_x11_display(), linux_x11_screen()),
        .subwindow = None,
        .time = 0,
        .x = 0,
        .y = 0,
        .x_root = 0,
        .y_root = 0,
        .state = 0,
        .keycode = keycode_1,
        .same_screen = True,
    };
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_1);
    CHECK_INTEQUAL(events[0].keyboard.system_key, keycode_1);
    CHECK_FALSE(events[0].keyboard.is_repeat);

    num_events = 0;
    sys_test_time_set_seconds(2.0);
    event.type = KeyRelease;
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    event.type = KeyPress;
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_1);
    CHECK_INTEQUAL(events[0].keyboard.system_key, keycode_1);
    CHECK_TRUE(events[0].keyboard.is_repeat);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_key_repeat_no_keysym)
{
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    XKeyEvent event = {
        .type = KeyPress,
        .display = linux_x11_display(),
        .window = linux_x11_window(),
        .root = RootWindow(linux_x11_display(), linux_x11_screen()),
        .subwindow = None,
        .time = 0,
        .x = 0,
        .y = 0,
        .x_root = 0,
        .y_root = 0,
        .state = 0,
        .keycode = keycode_invalid,
        .same_screen = True,
    };
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_SYSTEM_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].keyboard.key, 0);
    CHECK_INTEQUAL(events[0].keyboard.system_key, keycode_invalid);
    CHECK_FALSE(events[0].keyboard.is_repeat);

    num_events = 0;
    sys_test_time_set_seconds(2.0);
    event.type = KeyRelease;
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    event.type = KeyPress;
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_SYSTEM_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].keyboard.key, 0);
    CHECK_INTEQUAL(events[0].keyboard.system_key, keycode_invalid);
    CHECK_TRUE(events[0].keyboard.is_repeat);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_key_repeat_different_time)
{
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    XKeyEvent event = {
        .type = KeyPress,
        .display = linux_x11_display(),
        .window = linux_x11_window(),
        .root = RootWindow(linux_x11_display(), linux_x11_screen()),
        .subwindow = None,
        .time = 0,
        .x = 0,
        .y = 0,
        .x_root = 0,
        .y_root = 0,
        .state = 0,
        .keycode = keycode_1,
        .same_screen = True,
    };
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_1);
    CHECK_INTEQUAL(events[0].keyboard.system_key, keycode_1);
    CHECK_FALSE(events[0].keyboard.is_repeat);

    num_events = 0;
    sys_test_time_set_seconds(2.0);
    event.time = 1000;
    event.type = KeyRelease;
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    event.time = 2000;
    event.type = KeyPress;
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_UP);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_1);
    CHECK_INTEQUAL(events[0].keyboard.system_key, keycode_1);
    CHECK_FALSE(events[0].keyboard.is_repeat);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[1].detail, INPUT_KEYBOARD_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 3.0);
    CHECK_INTEQUAL(events[1].keyboard.key, KEY_1);
    CHECK_INTEQUAL(events[1].keyboard.system_key, keycode_1);
    CHECK_FALSE(events[1].keyboard.is_repeat);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_key_repeat_different_key)
{
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    XKeyEvent event = {
        .type = KeyPress,
        .display = linux_x11_display(),
        .window = linux_x11_window(),
        .root = RootWindow(linux_x11_display(), linux_x11_screen()),
        .subwindow = None,
        .time = 0,
        .x = 0,
        .y = 0,
        .x_root = 0,
        .y_root = 0,
        .state = 0,
        .keycode = keycode_1,
        .same_screen = True,
    };
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_1);
    CHECK_INTEQUAL(events[0].keyboard.system_key, keycode_1);
    CHECK_FALSE(events[0].keyboard.is_repeat);

    num_events = 0;
    sys_test_time_set_seconds(2.0);
    event.type = KeyRelease;
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    event.type = KeyPress;
    event.keycode = keycode_backspace;
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_UP);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_1);
    CHECK_INTEQUAL(events[0].keyboard.system_key, keycode_1);
    CHECK_FALSE(events[0].keyboard.is_repeat);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[1].detail, INPUT_KEYBOARD_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 2.0);
    CHECK_INTEQUAL(events[1].keyboard.key, KEY_BACKSPACE);
    CHECK_INTEQUAL(events[1].keyboard.system_key, keycode_backspace);
    CHECK_FALSE(events[1].keyboard.is_repeat);

    return 1;
}

/*-----------------------------------------------------------------------*/

/* Test for the X11 key release glitch workaround. */
TEST(test_key_up_linux_hack)
{
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    XKeyEvent event = {
        .type = KeyPress,
        .display = linux_x11_display(),
        .window = linux_x11_window(),
        .root = RootWindow(linux_x11_display(), linux_x11_screen()),
        .subwindow = None,
        .time = 0,
        .x = 0,
        .y = 0,
        .x_root = 0,
        .y_root = 0,
        .state = 0,
        .keycode = keycode_1,
        .same_screen = True,
    };
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    /* This release event should be delayed until the second update call. */
    event.type = KeyRelease;
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_1);
    CHECK_INTEQUAL(events[0].keyboard.system_key, keycode_1);
    CHECK_FALSE(events[0].keyboard.is_repeat);

    num_events = 0;
    sys_test_time_set_seconds(2.0);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_UP);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_1);
    CHECK_INTEQUAL(events[0].keyboard.system_key, keycode_1);
    CHECK_FALSE(events[0].keyboard.is_repeat);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_key_up_on_window_change)
{
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    XKeyEvent event = {
        .type = KeyPress,
        .display = linux_x11_display(),
        .window = linux_x11_window(),
        .root = RootWindow(linux_x11_display(), linux_x11_screen()),
        .subwindow = None,
        .time = 0,
        .x = 0,
        .y = 0,
        .x_root = 0,
        .y_root = 0,
        .state = 0,
        .keycode = keycode_1,
        .same_screen = True,
    };
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_1);
    CHECK_INTEQUAL(events[0].keyboard.system_key, keycode_1);
    CHECK_FALSE(events[0].keyboard.is_repeat);

    /* Forced-release is applied even if the window size doesn't actually
     * change. */
    num_events = 0;
    sys_test_time_set_seconds(2.0);
    ASSERT(graphics_set_display_mode(TESTW, TESTH, NULL));
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_UP);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_1);
    CHECK_INTEQUAL(events[0].keyboard.system_key, keycode_1);
    CHECK_FALSE(events[0].keyboard.is_repeat);

    /* Receiving the actual KeyRelease shouldn't trigger another event. */
    num_events = 0;
    sys_test_time_set_seconds(3.0);
    event.type = KeyRelease;
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);

    return 1;
}

/*************************************************************************/
/*************************** Tests: Mouse input **************************/
/*************************************************************************/

TEST(test_mouse_position)
{
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    XMotionEvent event = {
        .type = MotionNotify,
        .display = linux_x11_display(),
        .window = linux_x11_window(),
        .root = RootWindow(linux_x11_display(), linux_x11_screen()),
        .subwindow = None,
        .time = 0,
        .x = TESTW/4,
        .y = TESTH/2,
        .x_root = 0,  // These are ignored, so we don't bother setting them.
        .y_root = 0,
        .state = 0,
        .is_hint = NotifyNormal,
        .same_screen = True,
    };
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.25);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.5);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mouse_position_out_of_range)
{
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    XMotionEvent event = {
        .type = MotionNotify,
        .display = linux_x11_display(),
        .window = linux_x11_window(),
        .root = RootWindow(linux_x11_display(), linux_x11_screen()),
        .subwindow = None,
        .time = 0,
        .x = TESTW*5/4,
        .y = TESTH*3/2,
        .x_root = 0,
        .y_root = 0,
        .state = 0,
        .is_hint = NotifyNormal,
        .same_screen = True,
    };
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, (float)(TESTW-1) / (float)TESTW);
    CHECK_FLOATEQUAL(events[0].mouse.y, (float)(TESTH-1) / (float)TESTH);

    num_events = 0;
    sys_test_time_set_seconds(2.0);
    event.x = -TESTW/4;
    event.y = -TESTH/2;
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mouse_buttons)
{
    static const int event_map[][3] = {
        {ButtonPress,   Button1, INPUT_MOUSE_LMB_DOWN},
        {ButtonRelease, Button1, INPUT_MOUSE_LMB_UP},
        {ButtonPress,   Button2, INPUT_MOUSE_MMB_DOWN},
        {ButtonRelease, Button2, INPUT_MOUSE_MMB_UP},
        {ButtonPress,   Button3, INPUT_MOUSE_RMB_DOWN},
        {ButtonRelease, Button3, INPUT_MOUSE_RMB_UP},
    };
    double time = 1.0;

    for (int i = 0; i < lenof(event_map); i++, time += 1.0) {
        num_events = 0;
        sys_test_time_set_seconds(time);
        XButtonEvent event = {
            .type = event_map[i][0],
            .display = linux_x11_display(),
            .window = linux_x11_window(),
            .root = RootWindow(linux_x11_display(), linux_x11_screen()),
            .subwindow = None,
            .time = 0,
            .x = i,
            .y = i+1,
            .x_root = 0,
            .y_root = 0,
            .state = 0,
            .button = event_map[i][1],
            .same_screen = True,
        };
        CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                              (XEvent *)&event));
        XSync(event.display, False);
        sys_input_update();
        CHECK_INTEQUAL(num_events, 1);
        CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
        CHECK_INTEQUAL(events[0].detail, event_map[i][2]);
        CHECK_DOUBLEEQUAL(events[0].timestamp, time);
        CHECK_FLOATEQUAL(events[0].mouse.x, (float)i/TESTW);
        CHECK_FLOATEQUAL(events[0].mouse.y, (float)(i+1)/TESTH);

        /* A repeat X11 event with the same state should not trigger a
         * second SIL event. */
        num_events = 0;
        sys_test_time_set_seconds(time + 0.5);
        CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                              (XEvent *)&event));
        XSync(event.display, False);
        sys_input_update();
        CHECK_INTEQUAL(num_events, 0);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mouse_scroll)
{
    static const int event_map[][3] = {
        {Button4,   INPUT_MOUSE_SCROLL_V, -1},
        {Button5,   INPUT_MOUSE_SCROLL_V, +1},
        {Button5+1, INPUT_MOUSE_SCROLL_H, -1},
        {Button5+2, INPUT_MOUSE_SCROLL_H, +1},
    };
    double time = 1.0;

    for (int i = 0; i < lenof(event_map); i++, time += 1.0) {
        num_events = 0;
        sys_test_time_set_seconds(time);
        XButtonEvent event = {
            .type = ButtonPress,
            .display = linux_x11_display(),
            .window = linux_x11_window(),
            .root = RootWindow(linux_x11_display(), linux_x11_screen()),
            .subwindow = None,
            .time = 0,
            .x = i,
            .y = i+1,
            .x_root = 0,
            .y_root = 0,
            .state = 0,
            .button = event_map[i][0],
            .same_screen = True,
        };
        CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                              (XEvent *)&event));
        event.type = ButtonRelease;
        CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                              (XEvent *)&event));
        XSync(event.display, False);
        sys_input_update();
        CHECK_INTEQUAL(num_events, 1);
        CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
        CHECK_INTEQUAL(events[0].detail, event_map[i][1]);
        CHECK_DOUBLEEQUAL(events[0].timestamp, time);
        CHECK_FLOATEQUAL(events[0].mouse.x, (float)i/TESTW);
        CHECK_FLOATEQUAL(events[0].mouse.y, (float)(i+1)/TESTH);
        CHECK_INTEQUAL(events[0].mouse.scroll, event_map[i][2]);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mouse_set_position)
{
    /* This will set the real mouse pointer position, so save and restore
     * it to avoid interfering with whatever else the user may be doing. */
    int saved_x, saved_y;
    get_mouse_position(&saved_x, &saved_y);

    /* Make sure the mouse pointer is not already at the target position. */
    {
        int x, y;
        Display *display = linux_x11_display();
        Window window = linux_x11_window();
        Window unused_r, unused_c;
        int unused_rx, unused_ry;
        unsigned int unused_mask;
        ASSERT(XQueryPointer(display, window, &unused_r, &unused_c,
                             &unused_rx, &unused_ry, &x, &y, &unused_mask));
        if (x != 0 || y != 0) {
            XWarpPointer(display, None, window, 0, 0, 0, 0, 0, 0);
            XSync(display, False);
            sys_input_update();
        }
    }

    num_events = 0;
    sys_test_time_set_seconds(1.0);
    sys_input_mouse_set_position(0.5, 0.75);
    /* Discard the XWarpPointer event so we can send our own events first. */
    XSync(linux_x11_display(), True);

    /* These events must occur within the position override timeout
     * (1 second). */
    sys_test_time_set_seconds(1.25);
    /* This event will be ignored. */
    XMotionEvent event = {
        .type = MotionNotify,
        .display = linux_x11_display(),
        .window = linux_x11_window(),
        .root = RootWindow(linux_x11_display(), linux_x11_screen()),
        .subwindow = None,
        .time = 0,
        .x = TESTW/4,
        .y = TESTH/2,
        .x_root = 0,
        .y_root = 0,
        .state = 0,
        .is_hint = NotifyNormal,
        .same_screen = True,
    };
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();

    sys_test_time_set_seconds(1.5);
    XButtonEvent event2 = {
        .type = ButtonPress,
        .display = linux_x11_display(),
        .window = linux_x11_window(),
        .root = RootWindow(linux_x11_display(), linux_x11_screen()),
        .subwindow = None,
        .time = 0,
        .x = TESTW/4,  // These coordinates will be overridden.
        .y = TESTH/2,
        .x_root = 0,
        .y_root = 0,
        .state = 0,
        .button = Button1,
        .same_screen = True,
    };
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event2));
    XSync(event.display, False);
    sys_input_update();

    /* The MotionNotify event above should be ignored because of the
     * set_position() call, so we'll only get 2 events. */
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.5);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.75);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[1].detail, INPUT_MOUSE_LMB_DOWN);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 1.5);
    CHECK_FLOATEQUAL(events[1].mouse.x, 0.5);
    CHECK_FLOATEQUAL(events[1].mouse.y, 0.75);

    /* Setting the pointer again to the same position should not generate
     * an event. */
    num_events = 0;
    sys_test_time_set_seconds(2.0);
    sys_input_mouse_set_position(0.5, 0.75);
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);

    /* Setting the pointer again to the same position should generate an
     * event if XQueryPointer() fails. */
    num_events = 0;
    sys_test_time_set_seconds(3.0);
    disable_XQueryPointer = 1;
    sys_input_mouse_set_position(0.5, 0.75);
    disable_XQueryPointer = 0;
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 3.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.5);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.75);

    /* Setting out-of-bounds coordinates should clamp the coordinates to
     * the window bounds. */
    num_events = 0;
    sys_test_time_set_seconds(4.0);
    sys_input_mouse_set_position(-1, -1);
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 4.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0);

    num_events = 0;
    sys_test_time_set_seconds(5.0);
    sys_input_mouse_set_position(2, 2);
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 5.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, (float)(TESTW-1)/TESTW);
    CHECK_FLOATEQUAL(events[0].mouse.y, (float)(TESTH-1)/TESTH);

    /* Set operations should do nothing (but not crash) if no window is
     * open. */
    sys_input_cleanup();
    graphics_cleanup();
    ASSERT(graphics_init());
    ASSERT(sys_input_init(event_callback));
    CHECK_FALSE(linux_x11_window());
    num_events = 0;
    sys_test_time_set_seconds(8.0);
    sys_input_mouse_set_position(0.25, 0.5);
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);

    ASSERT(graphics_set_display_mode(TESTW, TESTH, NULL));
    set_mouse_position(saved_x, saved_y);
    /* Without this, the MotionNotify event can sometimes leak into the
     * next test and show up as an unexpected input event. */
    XSync(linux_x11_display(), False);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mouse_set_position_override_timeout)
{
    /* This will set the real mouse pointer position, so save and restore
     * it to avoid interfering with whatever else the user may be doing. */
    int saved_x, saved_y;
    get_mouse_position(&saved_x, &saved_y);

    /* Make sure the mouse pointer is not already at the target position. */
    {
        int x, y;
        Display *display = linux_x11_display();
        Window window = linux_x11_window();
        Window unused_r, unused_c;
        int unused_rx, unused_ry;
        unsigned int unused_mask;
        ASSERT(XQueryPointer(display, window, &unused_r, &unused_c,
                             &unused_rx, &unused_ry, &x, &y, &unused_mask));
        if (x != 0 || y != 0) {
            XWarpPointer(display, None, window, 0, 0, 0, 0, 0, 0);
            XSync(display, False);
            sys_input_update();
        }
    }

    num_events = 0;
    sys_test_time_set_seconds(1.0);
    sys_input_mouse_set_position(0.5, 0.75);
    /* Discard the XWarpPointer event so we can send our own events first. */
    XSync(linux_x11_display(), True);

    /* Force expiration of the position override timeout. */
    sys_test_time_set_seconds(2.0);
    XMotionEvent event = {
        .type = MotionNotify,
        .display = linux_x11_display(),
        .window = linux_x11_window(),
        .root = RootWindow(linux_x11_display(), linux_x11_screen()),
        .subwindow = None,
        .time = 0,
        .x = TESTW/4,
        .y = TESTH/2,
        .x_root = 0,
        .y_root = 0,
        .state = 0,
        .is_hint = NotifyNormal,
        .same_screen = True,
    };
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();

    sys_test_time_set_seconds(3.0);
    XButtonEvent event2 = {
        .type = ButtonPress,
        .display = linux_x11_display(),
        .window = linux_x11_window(),
        .root = RootWindow(linux_x11_display(), linux_x11_screen()),
        .subwindow = None,
        .time = 0,
        .x = TESTW/4,  // These coordinates will _not_ be overridden.
        .y = TESTH/2,
        .x_root = 0,
        .y_root = 0,
        .state = 0,
        .button = Button1,
        .same_screen = True,
    };
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event2));
    XSync(event.display, False);
    sys_input_update();

    CHECK_INTEQUAL(num_events, 3);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.5);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.75);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[1].detail, INPUT_MOUSE_MOVE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 2.0);
    CHECK_FLOATEQUAL(events[1].mouse.x, 0.25);
    CHECK_FLOATEQUAL(events[1].mouse.y, 0.5);
    CHECK_INTEQUAL(events[2].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[2].detail, INPUT_MOUSE_LMB_DOWN);
    CHECK_DOUBLEEQUAL(events[2].timestamp, 3.0);
    CHECK_FLOATEQUAL(events[2].mouse.x, 0.25);
    CHECK_FLOATEQUAL(events[2].mouse.y, 0.5);

    ASSERT(graphics_set_display_mode(TESTW, TESTH, NULL));
    set_mouse_position(saved_x, saved_y);
    XSync(linux_x11_display(), False);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mouse_up_on_window_change)
{
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    XButtonEvent event = {
        .type = ButtonPress,
        .display = linux_x11_display(),
        .window = linux_x11_window(),
        .root = RootWindow(linux_x11_display(), linux_x11_screen()),
        .subwindow = None,
        .time = 0,
        .x = TESTW/4,
        .y = TESTH/2,
        .x_root = 0,
        .y_root = 0,
        .state = 0,
        .button = Button1,
        .same_screen = True,
    };
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    event.button = Button2;
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    event.button = Button3;
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 3);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_LMB_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.25);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.5);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[1].detail, INPUT_MOUSE_MMB_DOWN);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[1].mouse.x, 0.25);
    CHECK_FLOATEQUAL(events[1].mouse.y, 0.5);
    CHECK_INTEQUAL(events[2].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[2].detail, INPUT_MOUSE_RMB_DOWN);
    CHECK_DOUBLEEQUAL(events[2].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[2].mouse.x, 0.25);
    CHECK_FLOATEQUAL(events[2].mouse.y, 0.5);

    /* Forced-release is applied even if the window size doesn't actually
     * change. */
    num_events = 0;
    sys_test_time_set_seconds(2.0);
    ASSERT(graphics_set_display_mode(TESTW, TESTH, NULL));
    CHECK_INTEQUAL(num_events, 3);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_LMB_UP);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.25);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.5);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[1].detail, INPUT_MOUSE_MMB_UP);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 2.0);
    CHECK_FLOATEQUAL(events[1].mouse.x, 0.25);
    CHECK_FLOATEQUAL(events[1].mouse.y, 0.5);
    CHECK_INTEQUAL(events[2].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[2].detail, INPUT_MOUSE_RMB_UP);
    CHECK_DOUBLEEQUAL(events[2].timestamp, 2.0);
    CHECK_FLOATEQUAL(events[2].mouse.x, 0.25);
    CHECK_FLOATEQUAL(events[2].mouse.y, 0.5);

    /* Receiving the actual ButtonReleases shouldn't trigger more events. */
    num_events = 0;
    sys_test_time_set_seconds(3.0);
    event.type = ButtonRelease;
    event.button = Button1;
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    event.button = Button2;
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    event.button = Button3;
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mouse_up_on_window_change_pos_override)
{
    num_events = 0;
    sys_test_time_set_seconds(1.0);
    XButtonEvent event = {
        .type = ButtonPress,
        .display = linux_x11_display(),
        .window = linux_x11_window(),
        .root = RootWindow(linux_x11_display(), linux_x11_screen()),
        .subwindow = None,
        .time = 0,
        .x = TESTW/4,
        .y = TESTH/2,
        .x_root = 0,
        .y_root = 0,
        .state = 0,
        .button = Button1,
        .same_screen = True,
    };
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    event.button = Button2;
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    event.button = Button3;
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 3);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_LMB_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.25);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.5);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[1].detail, INPUT_MOUSE_MMB_DOWN);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[1].mouse.x, 0.25);
    CHECK_FLOATEQUAL(events[1].mouse.y, 0.5);
    CHECK_INTEQUAL(events[2].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[2].detail, INPUT_MOUSE_RMB_DOWN);
    CHECK_DOUBLEEQUAL(events[2].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[2].mouse.x, 0.25);
    CHECK_FLOATEQUAL(events[2].mouse.y, 0.5);

    linux_override_mouse_position(TESTW/2, TESTH*3/4);
    num_events = 0;
    sys_test_time_set_seconds(2.0);
    ASSERT(graphics_set_display_mode(TESTW, TESTH, NULL));
    CHECK_INTEQUAL(num_events, 3);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_LMB_UP);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.5);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.75);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[1].detail, INPUT_MOUSE_MMB_UP);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 2.0);
    CHECK_FLOATEQUAL(events[1].mouse.x, 0.5);
    CHECK_FLOATEQUAL(events[1].mouse.y, 0.75);
    CHECK_INTEQUAL(events[2].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[2].detail, INPUT_MOUSE_RMB_UP);
    CHECK_DOUBLEEQUAL(events[2].timestamp, 2.0);
    CHECK_FLOATEQUAL(events[2].mouse.x, 0.5);
    CHECK_FLOATEQUAL(events[2].mouse.y, 0.75);

    return 1;
}

/*************************************************************************/
/*************************** Tests: Text input ***************************/
/*************************************************************************/

TEST(test_text_input_char)
{
    sys_input_text_set_state(1, NULL, NULL);

    num_events = 0;
    sys_test_time_set_seconds(1.0);
    XKeyEvent event = {
        .type = KeyPress,
        .display = linux_x11_display(),
        .window = linux_x11_window(),
        .root = RootWindow(linux_x11_display(), linux_x11_screen()),
        .subwindow = None,
        .time = 0,
        .x = 0,
        .y = 0,
        .x_root = 0,
        .y_root = 0,
        .state = 0,
        .keycode = keycode_1,
        .same_screen = True,
    };
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_1);
    CHECK_INTEQUAL(events[0].keyboard.system_key, keycode_1);
    CHECK_FALSE(events[0].keyboard.is_repeat);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_TEXT);
    CHECK_INTEQUAL(events[1].detail, INPUT_TEXT_INPUT);
    CHECK_INTEQUAL(events[1].timestamp, 1.0);
    CHECK_INTEQUAL(events[1].text.ch, '1');

    sys_input_text_set_state(0, NULL, NULL);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* Test handling of an input string longer than the internal buffer (1000
 * bytes).  To avoid events[] overflow, we use invalid UTF-8 continuation
 * bytes for most of the string. */
TEST(test_text_input_char_buffer_overflow)
{
    if (!linux_x11_ic()) {
        SKIP("No input context present.");
    }

    char buf[1002];
    memset(buf, 0x80, sizeof(buf)-2);
    buf[sizeof(buf)-2] = 'a';
    buf[sizeof(buf)-1] = 0;
    Xutf8LookupString_override = buf;

    sys_input_text_set_state(1, NULL, NULL);

    num_events = 0;
    sys_test_time_set_seconds(1.0);
    XKeyEvent event = {
        .type = KeyPress,
        .display = linux_x11_display(),
        .window = linux_x11_window(),
        .root = RootWindow(linux_x11_display(), linux_x11_screen()),
        .subwindow = None,
        .time = 0,
        .x = 0,
        .y = 0,
        .x_root = 0,
        .y_root = 0,
        .state = 0,
        .keycode = keycode_1,
        .same_screen = True,
    };
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_1);
    CHECK_INTEQUAL(events[0].keyboard.system_key, keycode_1);
    CHECK_FALSE(events[0].keyboard.is_repeat);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_TEXT);
    CHECK_INTEQUAL(events[1].detail, INPUT_TEXT_INPUT);
    CHECK_INTEQUAL(events[1].timestamp, 1.0);
    CHECK_INTEQUAL(events[1].text.ch, 'a');

    sys_input_text_set_state(0, NULL, NULL);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_text_input_char_buffer_overflow_memory_failure)
{
    if (!linux_x11_ic()) {
        SKIP("No input context present.");
    }

    char buf[1002];
    memset(buf, 0x80, sizeof(buf)-2);
    buf[sizeof(buf)-2] = 'a';
    buf[sizeof(buf)-1] = 0;
    Xutf8LookupString_override = buf;

    sys_input_text_set_state(1, NULL, NULL);

    num_events = 0;
    sys_test_time_set_seconds(1.0);
    XKeyEvent event = {
        .type = KeyPress,
        .display = linux_x11_display(),
        .window = linux_x11_window(),
        .root = RootWindow(linux_x11_display(), linux_x11_screen()),
        .subwindow = None,
        .time = 0,
        .x = 0,
        .y = 0,
        .x_root = 0,
        .y_root = 0,
        .state = 0,
        .keycode = keycode_1,
        .same_screen = True,
    };
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    TEST_mem_fail_after(0, 1, 0);
    sys_input_update();
    TEST_mem_fail_after(-1, 0, 0);
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_1);
    CHECK_INTEQUAL(events[0].keyboard.system_key, keycode_1);
    CHECK_FALSE(events[0].keyboard.is_repeat);

    sys_input_text_set_state(0, NULL, NULL);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_text_input_char_no_ic)
{
    disable_XCreateIC = 1;
    sys_input_cleanup();
    graphics_cleanup();
    ASSERT(graphics_init());
    ASSERT(sys_input_init(event_callback));
    ASSERT(graphics_set_display_mode(TESTW, TESTH, NULL));
    disable_XCreateIC = 0;

    sys_input_text_set_state(1, NULL, NULL);

    num_events = 0;
    sys_test_time_set_seconds(1.0);
    XKeyEvent event = {
        .type = KeyPress,
        .display = linux_x11_display(),
        .window = linux_x11_window(),
        .root = RootWindow(linux_x11_display(), linux_x11_screen()),
        .subwindow = None,
        .time = 0,
        .x = 0,
        .y = 0,
        .x_root = 0,
        .y_root = 0,
        .state = 0,
        .keycode = keycode_1,
        .same_screen = True,
    };
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    /* We seem to sometimes get a MOUSE_MOVE event here.  Perhaps a delayed
     * side effect of an earlier test? */
    if (events[0].detail == INPUT_MOUSE_MOVE) {
        num_events--;
        memmove(&events[0], &events[1], sizeof(*events) * num_events);
    }
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_1);
    CHECK_INTEQUAL(events[0].keyboard.system_key, keycode_1);
    CHECK_FALSE(events[0].keyboard.is_repeat);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_TEXT);
    CHECK_INTEQUAL(events[1].detail, INPUT_TEXT_INPUT);
    CHECK_INTEQUAL(events[1].timestamp, 1.0);
    CHECK_INTEQUAL(events[1].text.ch, '1');

    sys_input_text_set_state(0, NULL, NULL);
    CHECK_TRUE(graphics_set_display_mode(TESTW, TESTH, NULL));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_text_input_action)
{
    sys_input_text_set_state(1, NULL, NULL);

    const int event_map[][3] = {
        {keycode_backspace, KEY_BACKSPACE,    INPUT_TEXT_BACKSPACE},
        {keycode_delete,    KEY_DELETE,       INPUT_TEXT_DELETE},
        {keycode_left,      KEY_LEFT,         INPUT_TEXT_CURSOR_LEFT},
        {keycode_right,     KEY_RIGHT,        INPUT_TEXT_CURSOR_RIGHT},
        {keycode_home,      KEY_HOME,         INPUT_TEXT_CURSOR_HOME},
        {keycode_end,       KEY_END,          INPUT_TEXT_CURSOR_END},
        {keycode_escape,    KEY_ESCAPE,       INPUT_TEXT_CANCELLED},
        {keycode_return,    KEY_ENTER,        INPUT_TEXT_DONE},
        {keycode_kp_enter,  KEY_NUMPAD_ENTER, INPUT_TEXT_DONE},
    };
    double time = 1.0;

    for (int i = 0; i < lenof(event_map); i++, time += 1.0) {
        num_events = 0;
        sys_test_time_set_seconds(time);
        XKeyEvent event = {
            .type = KeyPress,
            .display = linux_x11_display(),
            .window = linux_x11_window(),
            .root = RootWindow(linux_x11_display(), linux_x11_screen()),
            .subwindow = None,
            .time = 0,
            .x = 0,
            .y = 0,
            .x_root = 0,
            .y_root = 0,
            .state = 0,
            .keycode = event_map[i][0],
            .same_screen = True,
        };
        CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                              (XEvent *)&event));
        XSync(event.display, False);
        sys_input_update();
        CHECK_INTEQUAL(num_events, 2);
        CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
        CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_DOWN);
        CHECK_DOUBLEEQUAL(events[0].timestamp, time);
        CHECK_INTEQUAL(events[0].keyboard.key, event_map[i][1]);
        CHECK_INTEQUAL(events[0].keyboard.system_key, event_map[i][0]);
        CHECK_FALSE(events[0].keyboard.is_repeat);
        CHECK_INTEQUAL(events[1].type, INPUT_EVENT_TEXT);
        CHECK_INTEQUAL(events[1].detail, event_map[i][2]);
        CHECK_INTEQUAL(events[1].timestamp, time);
    }

    sys_input_text_set_state(0, NULL, NULL);
    return 1;
}

/*************************************************************************/
/************************** Tests: Touch input ***************************/
/*************************************************************************/

TEST(test_xinput2_client_version)
{
    if (!check_xinput2()) {
        SKIP("XInput2 not available.");
    }

    sys_input_cleanup();
    graphics_cleanup();
    ASSERT(graphics_init());
    ASSERT(sys_input_init(event_callback));
    ASSERT(graphics_set_display_mode(TESTW, TESTH, NULL));
    CHECK_TRUE(xinput_client_major > 2
               || (xinput_client_major == 2 && xinput_client_minor >= 2));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_touchscreen_present)
{
    if (!check_xinput2()) {
        SKIP("XInput2 not available.");
    }

    sys_input_cleanup();
    graphics_cleanup();
    xinput_simulate_touchscreen = 1;
    ASSERT(graphics_init());
    ASSERT(sys_input_init(event_callback));
    ASSERT(graphics_set_display_mode(TESTW, TESTH, NULL));

    SysInputInfo info;
    sys_input_info(&info);
    CHECK_TRUE(info.has_touch);

    int num_masks;
    XIEventMask *xinput_mask = XIGetSelectedEvents(
        linux_x11_display(), linux_x11_window(), &num_masks);
    CHECK_INTEQUAL(num_masks, 1);
    CHECK_INTEQUAL(xinput_mask->deviceid, XIAllMasterDevices);
    uint32_t xinput_bits = 0;
    for (int i = 0; i < ubound(xinput_mask->mask_len, 4); i++) {
        xinput_bits |= xinput_mask->mask[i] << (i*8);
    }
    CHECK_TRUE(xinput_bits & XI_ButtonPressMask);
    CHECK_TRUE(xinput_bits & XI_ButtonReleaseMask);
    CHECK_TRUE(xinput_bits & XI_MotionMask);
    CHECK_TRUE(xinput_bits & XI_TouchBeginMask);
    CHECK_TRUE(xinput_bits & XI_TouchUpdateMask);
    CHECK_TRUE(xinput_bits & XI_TouchEndMask);
    XFree(xinput_mask);

    /* The standard X pointer events should _not_ be in the event mask
     * since we get them from XInput2 if a touchscreen is present. */
    XWindowAttributes attributes;
    CHECK_TRUE(XGetWindowAttributes(linux_x11_display(),
                                    linux_x11_window(), &attributes));
    CHECK_FALSE(attributes.your_event_mask & ButtonPressMask);
    CHECK_FALSE(attributes.your_event_mask & ButtonReleaseMask);
    CHECK_FALSE(attributes.your_event_mask & PointerMotionMask);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_touchscreen_absent)
{
    if (!check_xinput2()) {
        SKIP("XInput2 not available.");
    }

    sys_input_cleanup();
    graphics_cleanup();
    xinput_simulate_touchscreen = 0;
    ASSERT(graphics_init());
    ASSERT(sys_input_init(event_callback));
    ASSERT(graphics_set_display_mode(TESTW, TESTH, NULL));

    SysInputInfo info;
    sys_input_info(&info);
    CHECK_FALSE(info.has_touch);

    int num_masks;
    XIEventMask *xinput_mask = XIGetSelectedEvents(
        linux_x11_display(), linux_x11_window(), &num_masks);
    if (xinput_mask) {
        XFree(xinput_mask);
    }

    XWindowAttributes attributes;
    CHECK_TRUE(XGetWindowAttributes(linux_x11_display(),
                                    linux_x11_window(), &attributes));
    CHECK_TRUE(attributes.your_event_mask & ButtonPressMask);
    CHECK_TRUE(attributes.your_event_mask & ButtonReleaseMask);
    CHECK_TRUE(attributes.your_event_mask & EnterWindowMask);
    CHECK_TRUE(attributes.your_event_mask & PointerMotionMask);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_touch_input)
{
    const int xinput2_opcode = check_xinput2();
    if (!xinput2_opcode) {
        SKIP("XInput2 not available.");
    }

    sys_input_cleanup();
    graphics_cleanup();
    xinput_simulate_touchscreen = 1;
    ASSERT(graphics_init());
    ASSERT(sys_input_init(event_callback));
    ASSERT(graphics_set_display_mode(TESTW, TESTH, NULL));

    num_events = 0;
    sys_test_time_set_seconds(1.0);
    XIDeviceEvent xi2_event = {
        .type = GenericEvent,
        .display = linux_x11_display(),
        .extension = xinput2_opcode,
        .evtype = XI_TouchBegin,
        .time = 0,
        .deviceid = xinput2_master_pointer(),
        .sourceid = 0,
        .detail = 123,
        .root = RootWindow(linux_x11_display(), linux_x11_screen()),
        .event = linux_x11_window(),
        .child = 0,
        .root_x = TESTW/4,
        .root_y = TESTH/2,
        .event_x = TESTW/4,
        .event_y = TESTH/2,
        .flags = 0,
    };
    mem_clear(&xi2_event.buttons, sizeof(xi2_event.buttons));
    mem_clear(&xi2_event.valuators, sizeof(xi2_event.valuators));
    mem_clear(&xi2_event.mods, sizeof(xi2_event.mods));
    mem_clear(&xi2_event.group, sizeof(xi2_event.group));
    XGenericEventCookie event;
    memcpy(&event, &xi2_event, sizeof(event));
    event.cookie = 0;
    event.data = &xi2_event;
    CHECK_TRUE(XSendEvent(event.display, xi2_event.event, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_TOUCH);
    CHECK_INTEQUAL(events[0].detail, INPUT_TOUCH_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].touch.id, 123);
    CHECK_FLOATEQUAL(events[0].touch.x, 0.25);
    CHECK_FLOATEQUAL(events[0].touch.y, 0.5);

    num_events = 0;
    sys_test_time_set_seconds(2.0);
    xi2_event.evtype = XI_TouchUpdate;
    xi2_event.root_x = TESTW/2;
    xi2_event.root_y = TESTH/4;
    xi2_event.event_x = TESTW/2;
    xi2_event.event_y = TESTH/4;
    memcpy(&event, &xi2_event, sizeof(event));
    event.cookie = 0;
    event.data = &xi2_event;
    CHECK_TRUE(XSendEvent(event.display, xi2_event.event, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_TOUCH);
    CHECK_INTEQUAL(events[0].detail, INPUT_TOUCH_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].touch.id, 123);
    CHECK_FLOATEQUAL(events[0].touch.x, 0.5);
    CHECK_FLOATEQUAL(events[0].touch.y, 0.25);

    num_events = 0;
    sys_test_time_set_seconds(3.0);
    xi2_event.evtype = XI_TouchEnd;
    xi2_event.root_x = TESTW*3/8;
    xi2_event.root_y = TESTH*3/4;
    xi2_event.event_x = TESTW*3/8;
    xi2_event.event_y = TESTH*3/4;
    memcpy(&event, &xi2_event, sizeof(event));
    event.cookie = 0;
    event.data = &xi2_event;
    CHECK_TRUE(XSendEvent(event.display, xi2_event.event, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_TOUCH);
    CHECK_INTEQUAL(events[0].detail, INPUT_TOUCH_UP);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 3.0);
    CHECK_INTEQUAL(events[0].touch.id, 123);
    CHECK_FLOATEQUAL(events[0].touch.x, 0.375);
    CHECK_FLOATEQUAL(events[0].touch.y, 0.75);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_touch_out_of_bounds)
{
    const int xinput2_opcode = check_xinput2();
    if (!xinput2_opcode) {
        SKIP("XInput2 not available.");
    }

    sys_input_cleanup();
    graphics_cleanup();
    xinput_simulate_touchscreen = 1;
    ASSERT(graphics_init());
    ASSERT(sys_input_init(event_callback));
    ASSERT(graphics_set_display_mode(TESTW, TESTH, NULL));

    num_events = 0;
    sys_test_time_set_seconds(1.0);
    XIDeviceEvent xi2_event = {
        .type = GenericEvent,
        .display = linux_x11_display(),
        .extension = xinput2_opcode,
        .evtype = XI_TouchBegin,
        .time = 0,
        .deviceid = xinput2_master_pointer(),
        .sourceid = 0,
        .detail = 123,
        .root = RootWindow(linux_x11_display(), linux_x11_screen()),
        .event = linux_x11_window(),
        .child = 0,
        .root_x = TESTW/4,
        .root_y = TESTH/2,
        .event_x = TESTW/4,
        .event_y = TESTH/2,
        .flags = 0,
    };
    mem_clear(&xi2_event.buttons, sizeof(xi2_event.buttons));
    mem_clear(&xi2_event.valuators, sizeof(xi2_event.valuators));
    mem_clear(&xi2_event.mods, sizeof(xi2_event.mods));
    mem_clear(&xi2_event.group, sizeof(xi2_event.group));
    XGenericEventCookie event;
    memcpy(&event, &xi2_event, sizeof(event));
    event.cookie = 0;
    event.data = &xi2_event;
    CHECK_TRUE(XSendEvent(event.display, xi2_event.event, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_TOUCH);
    CHECK_INTEQUAL(events[0].detail, INPUT_TOUCH_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].touch.id, 123);
    CHECK_FLOATEQUAL(events[0].touch.x, 0.25);
    CHECK_FLOATEQUAL(events[0].touch.y, 0.5);

    num_events = 0;
    sys_test_time_set_seconds(2.0);
    xi2_event.evtype = XI_TouchUpdate;
    xi2_event.root_x = 0;
    xi2_event.root_y = 0;
    xi2_event.event_x = -TESTW/4;
    xi2_event.event_y = -TESTH/2;
    memcpy(&event, &xi2_event, sizeof(event));
    event.cookie = 0;
    event.data = &xi2_event;
    CHECK_TRUE(XSendEvent(event.display, xi2_event.event, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_TOUCH);
    CHECK_INTEQUAL(events[0].detail, INPUT_TOUCH_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].touch.id, 123);
    CHECK_FLOATEQUAL(events[0].touch.x, 0);
    CHECK_FLOATEQUAL(events[0].touch.y, 0);

    num_events = 0;
    sys_test_time_set_seconds(3.0);
    xi2_event.evtype = XI_TouchEnd;
    xi2_event.root_x = TESTW*5/4;
    xi2_event.root_y = TESTH*3/2;
    xi2_event.event_x = TESTW*5/4;
    xi2_event.event_y = TESTH*3/2;
    memcpy(&event, &xi2_event, sizeof(event));
    event.cookie = 0;
    event.data = &xi2_event;
    CHECK_TRUE(XSendEvent(event.display, xi2_event.event, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_TOUCH);
    CHECK_INTEQUAL(events[0].detail, INPUT_TOUCH_UP);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 3.0);
    CHECK_INTEQUAL(events[0].touch.id, 123);
    CHECK_FLOATEQUAL(events[0].touch.x, (float)(TESTW-1)/TESTW);
    CHECK_FLOATEQUAL(events[0].touch.y, (float)(TESTH-1)/TESTH);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_touch_pointer_input)
{
    const int xinput2_opcode = check_xinput2();
    if (!xinput2_opcode) {
        SKIP("XInput2 not available.");
    }

    sys_input_cleanup();
    graphics_cleanup();
    xinput_simulate_touchscreen = 1;
    ASSERT(graphics_init());
    ASSERT(sys_input_init(event_callback));
    ASSERT(graphics_set_display_mode(TESTW, TESTH, NULL));

    num_events = 0;
    sys_test_time_set_seconds(1.0);
    XIDeviceEvent xi2_event = {
        .type = GenericEvent,
        .display = linux_x11_display(),
        .extension = xinput2_opcode,
        .evtype = XI_Motion,
        .time = 0,
        .deviceid = xinput2_master_pointer(),
        .sourceid = 0,
        .detail = 123,
        .root = RootWindow(linux_x11_display(), linux_x11_screen()),
        .event = linux_x11_window(),
        .child = 0,
        .root_x = TESTW/4,
        .root_y = TESTH/2,
        .event_x = TESTW/4,
        .event_y = TESTH/2,
        .flags = 0,
    };
    mem_clear(&xi2_event.buttons, sizeof(xi2_event.buttons));
    mem_clear(&xi2_event.valuators, sizeof(xi2_event.valuators));
    mem_clear(&xi2_event.mods, sizeof(xi2_event.mods));
    mem_clear(&xi2_event.group, sizeof(xi2_event.group));
    XGenericEventCookie event;
    memcpy(&event, &xi2_event, sizeof(event));
    event.cookie = 0;
    event.data = &xi2_event;
    CHECK_TRUE(XSendEvent(event.display, xi2_event.event, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.25);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.5);

    num_events = 0;
    sys_test_time_set_seconds(2.0);
    xi2_event.evtype = XI_ButtonPress;
    xi2_event.detail = 1;
    xi2_event.root_x = TESTW/2;
    xi2_event.root_y = TESTH/4;
    xi2_event.event_x = TESTW/2;
    xi2_event.event_y = TESTH/4;
    memcpy(&event, &xi2_event, sizeof(event));
    event.cookie = 0;
    event.data = &xi2_event;
    CHECK_TRUE(XSendEvent(event.display, xi2_event.event, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_LMB_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.5);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.25);

    num_events = 0;
    sys_test_time_set_seconds(3.0);
    xi2_event.evtype = XI_ButtonRelease;
    xi2_event.root_x = TESTW*3/8;
    xi2_event.root_y = TESTH*3/4;
    xi2_event.event_x = TESTW*3/8;
    xi2_event.event_y = TESTH*3/4;
    memcpy(&event, &xi2_event, sizeof(event));
    event.cookie = 0;
    event.data = &xi2_event;
    CHECK_TRUE(XSendEvent(event.display, xi2_event.event, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_LMB_UP);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 3.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.375);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.75);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_touch_emulated_pointer_input)
{
    const int xinput2_opcode = check_xinput2();
    if (!xinput2_opcode) {
        SKIP("XInput2 not available.");
    }

    sys_input_cleanup();
    graphics_cleanup();
    xinput_simulate_touchscreen = 1;
    ASSERT(graphics_init());
    ASSERT(sys_input_init(event_callback));
    ASSERT(graphics_set_display_mode(TESTW, TESTH, NULL));

    num_events = 0;
    sys_test_time_set_seconds(1.0);
    XIDeviceEvent xi2_event = {
        .type = GenericEvent,
        .display = linux_x11_display(),
        .extension = xinput2_opcode,
        .evtype = XI_Motion,
        .time = 0,
        .deviceid = xinput2_master_pointer(),
        .sourceid = 0,
        .detail = 123,
        .root = RootWindow(linux_x11_display(), linux_x11_screen()),
        .event = linux_x11_window(),
        .child = 0,
        .root_x = TESTW/4,
        .root_y = TESTH/2,
        .event_x = TESTW/4,
        .event_y = TESTH/2,
        .flags = XIPointerEmulated,
    };
    mem_clear(&xi2_event.buttons, sizeof(xi2_event.buttons));
    mem_clear(&xi2_event.valuators, sizeof(xi2_event.valuators));
    mem_clear(&xi2_event.mods, sizeof(xi2_event.mods));
    mem_clear(&xi2_event.group, sizeof(xi2_event.group));
    XGenericEventCookie event;
    memcpy(&event, &xi2_event, sizeof(event));
    event.cookie = 0;
    event.data = &xi2_event;
    CHECK_TRUE(XSendEvent(event.display, xi2_event.event, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);

    return 1;
}

/*************************************************************************/
/************************** Tests: Miscellaneous *************************/
/*************************************************************************/

TEST(test_quit_by_signal)
{
    raise(SIGINT);
    num_events = 0;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);
    CHECK_TRUE(sys_input_is_quit_requested());

    sys_input_cleanup();
    ASSERT(sys_input_init(event_callback));
    raise(SIGTERM);
    num_events = 0;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);
    CHECK_TRUE(sys_input_is_quit_requested());

    sys_input_cleanup();
    ASSERT(sys_input_init(event_callback));
    raise(SIGHUP);
    num_events = 0;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);
    CHECK_TRUE(sys_input_is_quit_requested());

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_quit_by_window)
{
    Display *display = linux_x11_display();
    Atom wm_protocols = XInternAtom(display, "WM_PROTOCOLS", True);
    Atom wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", True);
    if (!wm_protocols || !wm_delete_window) {
        SKIP("WM_PROTOCOLS and/or WM_DELETE_WINDOW atoms not found.");
    }

    Window window = linux_x11_window();
    XClientMessageEvent event = {
        .type = ClientMessage,
        .display = display,
        .window = window,
        .message_type = wm_protocols,
        .format = 32,
        .data = {.l = {wm_delete_window}},
    };
    XSendEvent(display, window, False, 0, (XEvent *)&event);
    XSync(display, False);
    num_events = 0;
    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);
    CHECK_TRUE(sys_input_is_quit_requested());

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_suspend)
{
    /* We don't support suspend/resume on Linux, so just check that the
     * associated functions behave properly. */
    CHECK_FALSE(sys_input_is_suspend_requested());
    sys_input_acknowledge_suspend_request();  // Should do nothing.

    return 1;
}

/*-----------------------------------------------------------------------*/

/* For this test, we want to be sure to clean up on return even if the
 * test fails, so that we don't leave input in a grabbed state. */
#undef FAIL_ACTION
#define FAIL_ACTION  result = 0; goto out

TEST(test_grab)
{
    int result = 1;

    int saved_x, saved_y;
    get_mouse_position(&saved_x, &saved_y);

    /* If not grabbed (the default), setting the mouse position outside the
     * window should work.  We verify this by setting two positions which
     * are separated by more than the window size and verifying that we got
     * the positions we just set.  (For this check, we rely on X11 calls to
     * set the position, since sys_input_mouse_set_position() only sets
     * positions within the current window.) */
    CHECK_FALSE(linux_get_window_grab());
    if (saved_x >= 0 && saved_y >= 0) {
        int x, y;
        set_mouse_position(0, 0);
        get_mouse_position(&x, &y);
        CHECK_INTEQUAL(x, 0);
        CHECK_INTEQUAL(y, 0);
        set_mouse_position(TESTW+1, TESTH+1);
        get_mouse_position(&x, &y);
        CHECK_INTEQUAL(x, TESTW+1);
        CHECK_INTEQUAL(y, TESTH+1);
    }

    sys_input_grab(1);
    CHECK_TRUE(linux_get_window_grab());
    if (saved_x >= 0 && saved_y >= 0) {
        /* Depending on the position of the window, one or the other of
         * these set-position operations might succeed, so we have to
         * check both. */
        int x, y;
        set_mouse_position(0, 0);
        get_mouse_position(&x, &y);
        if (x == 0 && y == 0) {
            set_mouse_position(TESTW+1, TESTH+1);
            get_mouse_position(&x, &y);
            CHECK_FALSE(x == TESTW+1 && y == TESTH+1);
        }
    }

    /* Make sure sys_input_grab() doesn't just blindly flip the grab state. */
    sys_input_grab(1);
    CHECK_TRUE(linux_get_window_grab());
    if (saved_x >= 0 && saved_y >= 0) {
        int x, y;
        set_mouse_position(0, 0);
        get_mouse_position(&x, &y);
        if (x == 0 && y == 0) {
            set_mouse_position(TESTW+1, TESTH+1);
            get_mouse_position(&x, &y);
            CHECK_FALSE(x == TESTW+1 && y == TESTH+1);
        }
    }

    sys_input_grab(0);
    CHECK_FALSE(linux_get_window_grab());
    if (saved_x >= 0 && saved_y >= 0) {
        int x, y;
        set_mouse_position(0, 0);
        get_mouse_position(&x, &y);
        CHECK_INTEQUAL(x, 0);
        CHECK_INTEQUAL(y, 0);
        set_mouse_position(TESTW+1, TESTH+1);
        get_mouse_position(&x, &y);
        CHECK_INTEQUAL(x, TESTW+1);
        CHECK_INTEQUAL(y, TESTH+1);
    }

    /* Grabbing should work even when a window isn't open. */
    graphics_cleanup();
    ASSERT(graphics_init());
    sys_input_grab(1);
    CHECK_FALSE(linux_get_window_grab());  // No window is open yet.
    ASSERT(graphics_set_display_attr("vsync", 0));
    ASSERT(open_window(TESTW, TESTH));
    graphics_set_viewport(0, 0, TESTW, TESTH);
    CHECK_TRUE(linux_get_window_grab());
    if (saved_x >= 0 && saved_y >= 0) {
        int x, y;
        set_mouse_position(0, 0);
        get_mouse_position(&x, &y);
        if (x == 0 && y == 0) {
            set_mouse_position(TESTW+1, TESTH+1);
            get_mouse_position(&x, &y);
            CHECK_FALSE(x == TESTW+1 && y == TESTH+1);
        }
    }

  out:
    sys_input_grab(0);
    XUngrabPointer(linux_x11_display(), CurrentTime);
    set_mouse_position(saved_x, saved_y);
    XSync(linux_x11_display(), False);
    return result;
}

#undef FAIL_ACTION
#define FAIL_ACTION  return 0

/*-----------------------------------------------------------------------*/

TEST(test_XCreateIC_error)
{
    error_XCreateIC = 1;
    CHECK_TRUE(graphics_set_display_mode(TESTW+1, TESTH+1, NULL));
    error_XCreateIC = 0;

    sys_input_text_set_state(1, NULL, NULL);

    num_events = 0;
    sys_test_time_set_seconds(1.0);
    XKeyEvent event = {
        .type = KeyPress,
        .display = linux_x11_display(),
        .window = linux_x11_window(),
        .root = RootWindow(linux_x11_display(), linux_x11_screen()),
        .subwindow = None,
        .time = 0,
        .x = 0,
        .y = 0,
        .x_root = 0,
        .y_root = 0,
        .state = 0,
        .keycode = keycode_1,
        .same_screen = True,
    };
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_1);
    CHECK_INTEQUAL(events[0].keyboard.system_key, keycode_1);
    CHECK_FALSE(events[0].keyboard.is_repeat);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_TEXT);
    CHECK_INTEQUAL(events[1].detail, INPUT_TEXT_INPUT);
    CHECK_INTEQUAL(events[1].timestamp, 1.0);
    CHECK_INTEQUAL(events[1].text.ch, '1');

    sys_input_text_set_state(0, NULL, NULL);
    CHECK_TRUE(graphics_set_display_mode(TESTW, TESTH, NULL));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_x11_timestamp_wraparound)
{
    sys_test_time_set_seconds(1.0);

    num_events = 0;
    XMotionEvent event = {
        .type = MotionNotify,
        .display = linux_x11_display(),
        .window = linux_x11_window(),
        .root = RootWindow(linux_x11_display(), linux_x11_screen()),
        .subwindow = None,
        .time = 1000,
        .x = TESTW/4,
        .y = TESTH/2,
        .x_root = 0,
        .y_root = 0,
        .state = 0,
        .is_hint = NotifyNormal,
        .same_screen = True,
    };
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.25);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.5);

    num_events = 0;
    event.time = 4294967000;
    event.x = TESTW/2;
    event.y = TESTH/4;
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 4294967.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.5);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.25);

    num_events = 0;
    event.time = 704;
    event.x = TESTW/4;
    event.y = TESTH/2;
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 4294968.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.25);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.5);

    num_events = 0;
    event.time = 1704;
    event.x = TESTW/2;
    event.y = TESTH/4;
    CHECK_TRUE(XSendEvent(event.display, event.window, False, 0,
                          (XEvent *)&event));
    XSync(event.display, False);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 4294969.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.5);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.25);

    return 1;
}

/*************************************************************************/
/*************************************************************************/
