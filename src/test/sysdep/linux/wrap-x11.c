/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/linux/wrap-x11.c: Wrappers for X11 functions allowing
 * failure injection and call counting.
 */

#include "src/base.h"
#include "src/sysdep.h"
#include "src/sysdep/linux/internal.h"
#include "src/test/base.h"
#include "src/test/sysdep/linux/wrap-x11.h"

#include <dlfcn.h>
#define RTLD_DEFAULT  NULL
#define RTLD_NEXT     ((void *)(intptr_t)-1)

#include <time.h>
#include <GL/glx.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/Xinerama.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/xf86vmode.h>

/*************************************************************************/
/*************************** Data for wrappers ***************************/
/*************************************************************************/

/* Flags for causing wrapped calls to fail. */
uint8_t disable_XCreateBitmapFromData;
uint8_t disable_XCreateColormap;
uint8_t disable_XCreateGC;
uint8_t disable_XCreateIC;
uint8_t disable_XCreatePixmap;
uint8_t disable_XCreatePixmapCursor;
uint8_t disable_XCreateWindow;
uint8_t disable_XGetImage;
uint8_t disable_XLoadQueryFont;
uint8_t disable_XQueryPointer;
uint8_t disable_XF86VidModeQueryExtension;
uint8_t disable_XF86VidModeGetAllModeLines;
uint8_t disable_XF86VidModeGetModeLine;
uint8_t disable_XIQueryVersion;
uint8_t disable_XRRQueryExtension;
uint8_t disable_XRRQueryVersion;
uint8_t disable_XRRGetCrtcInfo;
uint8_t disable_XRRGetOutputInfo;
uint8_t disable_XRRGetPanning;
uint8_t disable_XRRGetScreenResources;
uint8_t disable_XineramaQueryExtension;
uint8_t disable_XineramaIsActive;
uint8_t disable_glXQueryExtension;
uint8_t disable_glXCreateWindow;
uint8_t disable_glXCreateNewContext;
uint8_t disable_glXMakeContextCurrent;

/* Number of calls after which to disable specific wrapped calls. */
uint8_t disable_XCreateGC_after;

/* Flags for generating X11 errors from wrapped calls. */
uint8_t error_XCreateIC;
uint8_t error_XCreateWindow;
uint8_t error_XMoveResizeWindow;

/* Counters indicating the number of times certain functions were called. */
int called_XCreateWindow;
int called_XIconifyWindow;
int called_XMoveResizeWindow;
int called_XMoveWindow;
int called_XResetScreenSaver;
int called_XResizeWindow;
int called_XF86VidModeGetAllModeLines;
int called_XF86VidModeGetModeLine;
int called_XRRGetCrtcInfo;
int called_XRRGetOutputInfo;
int called_XRRGetPanning;
int called_XRRGetScreenResources;
int called_XRRSetCrtcConfig;
int called_XineramaIsActive;
int called_XineramaQueryScreens;

/* A copy of the last event sent via XSendEvent() and associated parameters. */
XEvent last_event;
Display *last_event_display;
Window last_event_window;
Bool last_event_propagate;
long last_event_mask;

/* Override return string for Xutf8LookupString() (NULL = no override). */
const char *Xutf8LookupString_override;

/* Versions to report from XIQueryVersion() and XRRQueryVersion(),
 * overriding what the system returns.  Only used if at least one of the
 * relevant {major,minor} pair is nonzero. */
uint8_t xinput_version_major;
uint8_t xinput_version_minor;
uint8_t xrandr_version_major;
uint8_t xrandr_version_minor;

/* Client version reported in the most recent call to XIQueryVersion(). */
int xinput_client_major;
int xinput_client_minor;

/* Flag indicating whether to indicate the presence of a touchscreen in
 * XIQueryDevice(): 1 to indicate that a touchscreen is present, 0 to
 * indicate that no touchscreen is present, or -1 to leave the device list
 * returned by the system unmodified. */
int8_t xinput_simulate_touchscreen = -1;

/*************************************************************************/
/********************** Exported utility functions ***********************/
/*************************************************************************/

void clear_x11_wrapper_variables(void)
{
    disable_XCreateBitmapFromData = 0;
    disable_XCreateColormap = 0;
    disable_XCreateGC = 0;
    disable_XCreateIC = 0;
    disable_XCreatePixmap = 0;
    disable_XCreatePixmapCursor = 0;
    disable_XCreateWindow = 0;
    disable_XGetImage = 0;
    disable_XLoadQueryFont = 0;
    disable_XQueryPointer = 0;
    disable_XF86VidModeQueryExtension = 0;
    disable_XF86VidModeGetAllModeLines = 0;
    disable_XF86VidModeGetModeLine = 0;
    disable_XIQueryVersion = 0;
    disable_XRRQueryExtension = 0;
    disable_XRRQueryVersion = 0;
    disable_XRRGetCrtcInfo = 0;
    disable_XRRGetOutputInfo = 0;
    disable_XRRGetPanning = 0;
    disable_XRRGetScreenResources = 0;
    disable_XineramaQueryExtension = 0;
    disable_XineramaIsActive = 0;
    disable_glXQueryExtension = 0;
    disable_glXCreateWindow = 0;
    disable_glXCreateNewContext = 0;
    disable_glXMakeContextCurrent = 0;

    disable_XCreateGC_after = 0;

    error_XCreateIC = 0;
    error_XCreateWindow = 0;
    error_XMoveResizeWindow = 0;

    called_XCreateWindow = 0;
    called_XIconifyWindow = 0;
    called_XMoveResizeWindow = 0;
    called_XMoveWindow = 0;
    called_XResetScreenSaver = 0;
    called_XResizeWindow = 0;
    called_XF86VidModeGetAllModeLines = 0;
    called_XF86VidModeGetModeLine = 0;
    called_XRRGetCrtcInfo = 0;
    called_XRRGetOutputInfo = 0;
    called_XRRGetPanning = 0;
    called_XRRGetScreenResources = 0;
    called_XRRSetCrtcConfig = 0;
    called_XineramaIsActive = 0;
    called_XineramaQueryScreens = 0;

    mem_clear(&last_event, sizeof(last_event));
    last_event_display = NULL;
    last_event_window = None;
    last_event_propagate = False;
    last_event_mask = 0;

    Xutf8LookupString_override = NULL;

    xinput_version_major = 0;
    xinput_version_minor = 0;
    xrandr_version_major = 0;
    xrandr_version_minor = 0;

    xinput_client_major = 0;
    xinput_client_minor = 0;

    xinput_simulate_touchscreen = -1;
}

/*************************************************************************/
/*************************** Wrapper functions ***************************/
/*************************************************************************/

/*
 * Macros which call the wrapped function and return its value from the
 * wrapper.  TRAMPOLINE requires that the function be present;
 * TRAMPOLINE_OPT allows the function to be missing, returning zero in
 * that case.
 *
 * Ideally, we'd use GCC's __builtin_return(__builtin_apply(...)) for
 * these, but they're broken on 32-bit x86, they're not supported by Clang,
 * and it appears even the GCC developers are considering phasing them out,
 * so we use handwritten assembly instead, with __builtin_* as a fallback
 * for unknown architectures.
 *
 * See the note at DEFINE_FUNCTIONS in wrap-io.c for details.
 */

#if defined(SIL_ARCH_X86_32)

#define DEFINE_TRAMPOLINE(rettype, name, args) \
extern rettype trampoline_##name args;          \
    __asm__(                                    \
        "    .section .rodata.str1.1,\"aMS\",@progbits,1\n" \
        "0:  .string \"" #name "\"\n"           \
        "    .text\n"                           \
        "trampoline_" #name ":\n"               \
        "    lea 0b, %eax\n"                    \
        "    push %eax\n"                       \
        "    push $-1\n"                        \
        "    call dlsym\n"                      \
        "    add $8, %esp\n"                    \
        "    jmp *%eax\n"                       \
    );
#define DEFINE_TRAMPOLINE_OPT(rettype, name, args) \
extern rettype trampoline_##name args;          \
    __asm__(                                    \
        "    .section .rodata.str1.1,\"aMS\",@progbits,1\n" \
        "0:  .string \"" #name "\"\n"           \
        "    .text\n"                           \
        "trampoline_" #name ":\n"               \
        "    lea 0b, %eax\n"                    \
        "    push %eax\n"                       \
        "    push $-1\n"                        \
        "    call dlsym\n"                      \
        "    add $8, %esp\n"                    \
        "    test %eax, %eax\n"                 \
        "    jz 1f\n"                           \
        "    jmp *%eax\n"                       \
        "1:  ret\n"                             \
    );

#elif defined(SIL_ARCH_X86_64)

#define DEFINE_TRAMPOLINE(rettype, name, args) \
extern rettype trampoline_##name args;          \
    __asm__(                                    \
        "    .section .rodata.str1.1,\"aMS\",@progbits,1\n" \
        "0:  .string \"" #name "\"\n"           \
        "    .text\n"                           \
        "trampoline_" #name ":\n"               \
        "    push %rdi\n"                       \
        "    push %rsi\n"                       \
        "    push %rdx\n"                       \
        "    push %rcx\n"                       \
        "    push %r8\n"                        \
        "    push %r9\n"                        \
        "    mov $-1, %rdi\n"                   \
        "    lea 0b(%rip), %rsi\n"              \
        "    call dlsym@PLT\n"                  \
        "    pop %r9\n"                         \
        "    pop %r8\n"                         \
        "    pop %rcx\n"                        \
        "    pop %rdx\n"                        \
        "    pop %rsi\n"                        \
        "    pop %rdi\n"                        \
        "    jmp *%rax\n"                       \
    );
#define DEFINE_TRAMPOLINE_OPT(rettype, name, args) \
extern rettype trampoline_##name args;          \
    __asm__(                                    \
        "    .section .rodata.str1.1,\"aMS\",@progbits,1\n" \
        "0:  .string \"" #name "\"\n"           \
        "    .text\n"                           \
        "trampoline_" #name ":\n"               \
        "    push %rdi\n"                       \
        "    push %rsi\n"                       \
        "    push %rdx\n"                       \
        "    push %rcx\n"                       \
        "    push %r8\n"                        \
        "    push %r9\n"                        \
        "    mov $-1, %rdi\n"                   \
        "    lea 0b(%rip), %rsi\n"              \
        "    call dlsym@PLT\n"                  \
        "    pop %r9\n"                         \
        "    pop %r8\n"                         \
        "    pop %rcx\n"                        \
        "    pop %rdx\n"                        \
        "    pop %rsi\n"                        \
        "    pop %rdi\n"                        \
        "    test %rax, %rax\n"                 \
        "    jz 1f\n"                           \
        "    jmp *%rax\n"                       \
        "1:  ret\n"                             \
    );

#else  // unknown architecture

/* Needed since most parameters are only referenced by __builtin_apply_args. */
#pragma GCC diagnostic ignored "-Wunused-parameter"

#define MAX_ARG_SIZE  (16 * sizeof(void *))
#define DEFINE_TRAMPOLINE(rettype, name, args) \
static rettype trampoline_##name args {              \
    void *wrapped_func = dlsym(RTLD_NEXT, #name);    \
    ASSERT(wrapped_func != name);                    \
    ASSERT(wrapped_func != NULL);                    \
    __builtin_return(__builtin_apply(wrapped_func, __builtin_apply_args(), \
                                     MAX_ARG_SIZE)); \
}
#define DEFINE_TRAMPOLINE_OPT(rettype, name, args) \
static rettype trampoline_##name args {              \
    void *wrapped_func = dlsym(RTLD_NEXT, #name);    \
    ASSERT(wrapped_func != name);                    \
    if (!wrapped_func) {                             \
        return 0;                                    \
    }                                                \
    __builtin_return(__builtin_apply(wrapped_func, __builtin_apply_args(), \
                                     MAX_ARG_SIZE)); \
}

#endif  // SIL_ARCH_*

/*-------------------------- Core X11 wrappers --------------------------*/

DEFINE_TRAMPOLINE(Pixmap, XCreateBitmapFromData,
                  (Display *display, Drawable d, const char *data,
                   unsigned int width, unsigned int height))
Pixmap XCreateBitmapFromData(Display *display, Drawable d, const char *data,
                             unsigned int width, unsigned int height)
{
    if (disable_XCreateBitmapFromData) {
        return None;
    }
    return trampoline_XCreateBitmapFromData(display, d, data, width, height);
}

DEFINE_TRAMPOLINE(Colormap, XCreateColormap,
                  (Display *display, Window w, Visual *visual, int alloc))
Colormap XCreateColormap(Display *display, Window w, Visual *visual, int alloc)
{
    if (disable_XCreateColormap) {
        return None;
    }
    return trampoline_XCreateColormap(display, w, visual, alloc);
}

DEFINE_TRAMPOLINE(GC, XCreateGC,
                  (Display *display, Drawable d, unsigned long valuemask,
                   XGCValues *values))
GC XCreateGC(Display *display, Drawable d, unsigned long valuemask,
             XGCValues *values)
{
    if (disable_XCreateGC) {
        return None;
    }
    if (disable_XCreateGC_after > 0) {
        disable_XCreateGC_after--;
        if (disable_XCreateGC_after == 0) {
            disable_XCreateGC = 1;
        }
    }
    return trampoline_XCreateGC(display, d, valuemask, values);
}

DEFINE_TRAMPOLINE(XIC, XCreateIC, (XIM im, ...))
XIC XCreateIC(XIM im, ...)
{
    if (disable_XCreateIC) {
        return None;
    }
    if (error_XCreateIC) {
        XWindowAttributes dummy;
        XGetWindowAttributes(linux_x11_display(), None, &dummy);
    }
    /* We can't pass varargs without __builtin_apply() (which we can't
     * unconditionally use because of the x86-32 bug), so we explicitly
     * pull exactly the number of arguments we pass in graphics.c. */
    va_list args;
    va_start(args, im);
    const char *arg1 = va_arg(args, const char *);
    ASSERT(arg1);
    const long arg2 = va_arg(args, long);
    const char *arg3 = va_arg(args, const char *);
    ASSERT(arg3);
    const long arg4 = va_arg(args, long);
    const char *arg5 = va_arg(args, const char *);
    ASSERT(!arg5);
    va_end(args);
    return trampoline_XCreateIC(im, arg1, arg2, arg3, arg4, arg5);
}

DEFINE_TRAMPOLINE(Pixmap, XCreatePixmap,
                  (Display *display, Drawable d, unsigned int width,
                   unsigned int height, unsigned int depth))
Pixmap XCreatePixmap(Display *display, Drawable d, unsigned int width,
                     unsigned int height, unsigned int depth)
{
    if (disable_XCreatePixmap) {
        return None;
    }
    return trampoline_XCreatePixmap(display, d, width, height, depth);
}

DEFINE_TRAMPOLINE(Cursor, XCreatePixmapCursor,
                  (Display *display, Pixmap source, Pixmap mask,
                   XColor *foreground_color, XColor *background_color,
                   unsigned int x, unsigned int y))
Cursor XCreatePixmapCursor(Display *display, Pixmap source, Pixmap mask,
                           XColor *foreground_color, XColor *background_color,
                           unsigned int x, unsigned int y)
{
    if (disable_XCreatePixmapCursor) {
        return None;
    }
    return trampoline_XCreatePixmapCursor(display, source, mask,
                                          foreground_color, background_color,
                                          x, y);
}

DEFINE_TRAMPOLINE(Window, XCreateWindow,
                  (Display *display, Window parent, int x, int y,
                   unsigned int width, unsigned int height,
                   unsigned int border_width, int depth, unsigned int class,
                   Visual *visual, unsigned long valuemask,
                   XSetWindowAttributes *attributes))
Window XCreateWindow(Display *display, Window parent, int x, int y,
                     unsigned int width, unsigned int height,
                     unsigned int border_width, int depth, unsigned int class,
                     Visual *visual, unsigned long valuemask,
                     XSetWindowAttributes *attributes)
{
    called_XCreateWindow++;
    if (disable_XCreateWindow) {
        return None;
    }
    if (error_XCreateWindow) {
        XWindowAttributes dummy;
        XGetWindowAttributes(linux_x11_display(), None, &dummy);
    }
    return trampoline_XCreateWindow(display, parent, x, y, width, height,
                                    border_width, depth, class, visual,
                                    valuemask, attributes);
}

DEFINE_TRAMPOLINE(XImage *, XGetImage,
                  (Display *display, Drawable d, int x, int y,
                   unsigned int width, unsigned int height,
                   unsigned long plane_mask, int format))
XImage *XGetImage(Display *display, Drawable d, int x, int y,
                  unsigned int width, unsigned int height,
                  unsigned long plane_mask, int format)
{
    if (disable_XGetImage) {
        return None;
    }
    return trampoline_XGetImage(display, d, x, y, width, height, plane_mask,
                                format);
}

DEFINE_TRAMPOLINE(Status, XIconifyWindow,
                  (Display *display, Window w, int screen_number))
Status XIconifyWindow(Display *display, Window w, int screen_number)
{
    called_XIconifyWindow++;
    return trampoline_XIconifyWindow(display, w, screen_number);
}

DEFINE_TRAMPOLINE(XFontStruct *, XLoadQueryFont,
                  (Display *display, const char *name))
XFontStruct *XLoadQueryFont(Display *display, const char *name)
{
    if (disable_XLoadQueryFont) {
        return NULL;
    }
    return trampoline_XLoadQueryFont(display, name);
}

DEFINE_TRAMPOLINE(int, XMoveResizeWindow,
                  (Display *display, Window w, int x, int y,
                   unsigned int width, unsigned int height))
int XMoveResizeWindow(Display *display, Window w, int x, int y,
                      unsigned int width, unsigned int height)
{
    called_XMoveResizeWindow++;
    if (error_XMoveResizeWindow) {
        XWindowAttributes dummy;
        XGetWindowAttributes(linux_x11_display(), None, &dummy);
        /* The return value is not documented, but libX11 always returns 1. */
        return 1;
    }
    return trampoline_XMoveResizeWindow(display, w, x, y, width, height);
}

DEFINE_TRAMPOLINE(int, XMoveWindow, (Display *display, Window w, int x, int y))
int XMoveWindow(Display *display, Window w, int x, int y)
{
    called_XMoveWindow++;
    return trampoline_XMoveWindow(display, w, x, y);
}

DEFINE_TRAMPOLINE(Bool, XQueryPointer,
                  (Display *display, Window w, Window *root_return,
                   Window *child_return, int *root_x_return,
                   int *root_y_return, int *win_x_return, int *win_y_return,
                   unsigned int *mask_return))
Bool XQueryPointer(Display *display, Window w, Window *root_return,
                   Window *child_return, int *root_x_return,
                   int *root_y_return, int *win_x_return, int *win_y_return,
                   unsigned int *mask_return)
{
    if (disable_XQueryPointer) {
        return False;
    }
    return trampoline_XQueryPointer(display, w, root_return, child_return,
                                    root_x_return, root_y_return,
                                    win_x_return, win_y_return, mask_return);
}

DEFINE_TRAMPOLINE(int, XResetScreenSaver, (Display *display))
int XResetScreenSaver(Display *display)
{
    called_XResetScreenSaver++;
    return trampoline_XResetScreenSaver(display);
}

DEFINE_TRAMPOLINE(int, XResizeWindow,
                  (Display *display, Window w, unsigned int width,
                   unsigned int height))
int XResizeWindow(Display *display, Window w, unsigned int width,
                  unsigned int height)
{
    called_XResizeWindow++;
    return trampoline_XResizeWindow(display, w, width, height);
}

DEFINE_TRAMPOLINE(Bool, XSendEvent,
                  (Display *display, Window w, Bool propagate, long event_mask,
                   XEvent *event_send))
Bool XSendEvent(Display *display, Window w, Bool propagate, long event_mask,
                XEvent *event_send)
{
    /* Only log events which are related to our current window.  (XOpenIM()
     * and XCreateIC() may trigger XSendEvent() calls by the input method,
     * but we don't want our tests to be confused by those.) */
    Window current_window = linux_x11_window();
    if (current_window) {
        ASSERT(display == linux_x11_display());
        Window root = RootWindow(display, linux_x11_screen());
        if (w == current_window
         || event_send->xany.window == current_window
         || (w == root && event_send->xany.window == root)) {
            last_event_display = display;
            last_event_window = w;
            last_event_propagate = propagate;
            last_event_mask = event_mask;
            last_event = *event_send;
        }
    }
    return trampoline_XSendEvent(display, w, propagate, event_mask,
                                 event_send);
}

DEFINE_TRAMPOLINE(int, Xutf8LookupString,
                  (XIC ic, XKeyPressedEvent *event, char *buffer_return,
                   int bytes_buffer, KeySym *keysym_return,
                   Status *status_return))
int Xutf8LookupString(XIC ic, XKeyPressedEvent *event, char *buffer_return,
                      int bytes_buffer, KeySym *keysym_return,
                      Status *status_return)
{
    if (Xutf8LookupString_override) {
        if ((int)strlen(Xutf8LookupString_override) > bytes_buffer) {
            if (status_return) {
                *status_return = XBufferOverflow;
            }
        } else {
            memcpy(buffer_return, Xutf8LookupString_override,
                   strlen(Xutf8LookupString_override));
            if (status_return) {
                *status_return = XLookupChars;
            }
        }
        return strlen(Xutf8LookupString_override);
    }
    return trampoline_Xutf8LookupString(ic, event, buffer_return, bytes_buffer,
                                        keysym_return, status_return);
}

/*------------------------ XF86VidMode wrappers -------------------------*/

DEFINE_TRAMPOLINE_OPT(Bool, XF86VidModeQueryExtension,
                      (Display *dpy, int *event_base_return,
                       int *error_base_return))
Bool XF86VidModeQueryExtension(Display *dpy, int *event_base_return,
                               int *error_base_return)
{
    if (disable_XF86VidModeQueryExtension) {
        return False;
    }
    return trampoline_XF86VidModeQueryExtension(dpy, event_base_return,
                                                error_base_return);
}

DEFINE_TRAMPOLINE(Bool, XF86VidModeGetAllModeLines,
                  (Display *dpy, int screen, int *modecount_return,
                   XF86VidModeModeInfo ***modelinesPtr_return))
Bool XF86VidModeGetAllModeLines(Display *dpy, int screen,
                                int *modecount_return,
                                XF86VidModeModeInfo ***modelinesPtr_return)
{
    called_XF86VidModeGetAllModeLines++;
    if (disable_XF86VidModeGetAllModeLines) {
        return False;
    }
    return trampoline_XF86VidModeGetAllModeLines(dpy, screen, modecount_return,
                                                 modelinesPtr_return);
}

DEFINE_TRAMPOLINE(Bool, XF86VidModeGetModeLine,
                  (Display *dpy, int screen, int *dotclock_return,
                   XF86VidModeModeLine *modeline_return))
Bool XF86VidModeGetModeLine(Display *dpy, int screen, int *dotclock_return,
                            XF86VidModeModeLine *modeline_return)
{
    called_XF86VidModeGetModeLine++;
    if (disable_XF86VidModeGetModeLine) {
        return False;
    }
    return trampoline_XF86VidModeGetModeLine(dpy, screen, dotclock_return,
                                             modeline_return);
}

/*-------------------------- XInput2 wrappers ---------------------------*/

Status XIQueryVersion(Display *display, int *major_version_inout,
                      int *minor_version_inout)
{
    if (disable_XIQueryVersion) {
        return BadRequest;
    }
    __typeof__(XIQueryVersion) *p_XIQueryVersion =
        dlsym(RTLD_NEXT, "XIQueryVersion");
    ASSERT(p_XIQueryVersion != XIQueryVersion);
    if (!p_XIQueryVersion) {
        return BadRequest;
    }
    xinput_client_major = *major_version_inout;
    xinput_client_minor = *minor_version_inout;
    Status result = (*p_XIQueryVersion)(display, major_version_inout,
                                        minor_version_inout);
    if (result != Success) {
        return result;
    }
    if (xinput_version_major || xinput_version_minor) {
        *major_version_inout = xinput_version_major;
        *minor_version_inout = xinput_version_minor;
    }
    return Success;
}

XIDeviceInfo *XIQueryDevice(Display *display, int deviceid,
                            int *ndevices_return)
{
    __typeof__(XIQueryDevice) *p_XIQueryDevice =
        dlsym(RTLD_NEXT, "XIQueryDevice");
    ASSERT(p_XIQueryDevice);
    ASSERT(p_XIQueryDevice != XIQueryDevice);
    XIDeviceInfo *devices =
        (*p_XIQueryDevice)(display, deviceid, ndevices_return);
    if (devices && xinput_simulate_touchscreen >= 0) {
        int found = 0;
        for (int i = 0; i < *ndevices_return; i++) {
            if (devices[i].use == XIMasterPointer) {
                found = 1;
                if (xinput_simulate_touchscreen) {
                    int j;
                    for (j = 0; j < devices[i].num_classes; j++) {
                        XITouchClassInfo *class =
                            (XITouchClassInfo *)devices[i].classes[j];
                        if (class->type == XITouchClass
                         && class->mode == XIDirectTouch) {
                            break;
                        }
                    }
                    if (j == devices[i].num_classes) {
                        /* No touchscreen, so fake one.  We don't check
                         * any classes other than XITouchClass, and
                         * XFreeDeviceList() doesn't care if we modify this
                         * data, so just arbitrarily overwrite the last
                         * class. */
                        ASSERT(j > 0);
                        XITouchClassInfo *class =
                            (XITouchClassInfo *)devices[i].classes[j-1];
                        class->type = XITouchClass;
                        class->mode = XIDirectTouch;
                        class->num_touches = 5;  // Arbitrary.
                    }
                } else {
                    for (int j = 0; j < devices[i].num_classes; j++) {
                        XITouchClassInfo *class =
                            (XITouchClassInfo *)devices[i].classes[j];
                        if (class->type == XITouchClass
                         && class->mode == XIDirectTouch) {
                            /* Null out the entry so this isn't detected as
                             * a touchscreen. */
                            devices[i].classes[j]->type = -1;
                        }
                    }
                }
            }
        }
        ASSERT(found);
    }
    return devices;
}

/*--------------------------- XRandR wrappers ---------------------------*/

DEFINE_TRAMPOLINE_OPT(Bool, XRRQueryExtension,
                      (Display *dpy, int *event_base_return,
                       int *error_base_return))
Bool XRRQueryExtension(Display *dpy, int *event_base_return,
                       int *error_base_return)
{
    if (disable_XRRQueryExtension) {
        return False;
    }
    return trampoline_XRRQueryExtension(dpy, event_base_return,
                                        error_base_return);
}

Status XRRQueryVersion(Display *dpy, int *major_version_return,
                       int *minor_version_return)
{
    if (disable_XRRQueryVersion) {
        /* The return value is prototyped as "Status", but it's really Bool. */
        return False;
    }
    __typeof__(XRRQueryVersion) *p_XRRQueryVersion =
        dlsym(RTLD_NEXT, "XRRQueryVersion");
    ASSERT(p_XRRQueryVersion != XRRQueryVersion);
    if (!p_XRRQueryVersion) {
        return False;
    }
    if (!(*p_XRRQueryVersion)(dpy, major_version_return,
                              minor_version_return)) {
        return False;
    }
    if (xrandr_version_major || xrandr_version_minor) {
        *major_version_return = xrandr_version_major;
        *minor_version_return = xrandr_version_minor;
    }
    return True;
}

DEFINE_TRAMPOLINE(XRRCrtcInfo *, XRRGetCrtcInfo,
                  (Display *dpy, XRRScreenResources *resources, RRCrtc crtc))
XRRCrtcInfo *XRRGetCrtcInfo(Display *dpy, XRRScreenResources *resources,
                            RRCrtc crtc)
{
    called_XRRGetCrtcInfo++;
    if (disable_XRRGetCrtcInfo) {
        return NULL;
    }
    return trampoline_XRRGetCrtcInfo(dpy, resources, crtc);
}

DEFINE_TRAMPOLINE(XRROutputInfo *, XRRGetOutputInfo,
                  (Display *dpy, XRRScreenResources *resources,
                   RROutput output))
XRROutputInfo *XRRGetOutputInfo(Display *dpy, XRRScreenResources *resources,
                                RROutput output)
{
    called_XRRGetOutputInfo++;
    if (disable_XRRGetOutputInfo) {
        return NULL;
    }
    return trampoline_XRRGetOutputInfo(dpy, resources, output);
}

DEFINE_TRAMPOLINE(XRRPanning *, XRRGetPanning,
                  (Display *dpy, XRRScreenResources *resources, RRCrtc crtc))
XRRPanning *XRRGetPanning(Display *dpy, XRRScreenResources *resources,
                          RRCrtc crtc)
{
    called_XRRGetPanning++;
    if (disable_XRRGetPanning) {
        return NULL;
    }
    return trampoline_XRRGetPanning(dpy, resources, crtc);
}

DEFINE_TRAMPOLINE(XRRScreenResources *, XRRGetScreenResources,
                  (Display *dpy, Window window))
XRRScreenResources *XRRGetScreenResources(Display *dpy, Window window)
{
    called_XRRGetScreenResources++;
    if (disable_XRRGetScreenResources) {
        return NULL;
    }
    return trampoline_XRRGetScreenResources(dpy, window);
}

DEFINE_TRAMPOLINE(Status, XRRSetCrtcConfig,
                  (Display *dpy, XRRScreenResources *resources,
                   RRCrtc crtc, Time timestamp, int x, int y, RRMode mode,
                   Rotation rotation, RROutput *outputs, int noutputs))
Status XRRSetCrtcConfig(Display *dpy, XRRScreenResources *resources,
                        RRCrtc crtc, Time timestamp, int x, int y, RRMode mode,
                        Rotation rotation, RROutput *outputs, int noutputs)
{
    called_XRRSetCrtcConfig++;
    return trampoline_XRRSetCrtcConfig(dpy, resources, crtc, timestamp, x, y,
                                       mode, rotation, outputs, noutputs);
}

/*-------------------------- Xinerama wrappers --------------------------*/

DEFINE_TRAMPOLINE_OPT(Bool, XineramaQueryExtension,
                      (Display *dpy, int *event_base_return,
                       int *error_base_return))
Bool XineramaQueryExtension(Display *dpy, int *event_base_return,
                               int *error_base_return)
{
    if (disable_XineramaQueryExtension) {
        return False;
    }
    return trampoline_XineramaQueryExtension(dpy, event_base_return,
                                             error_base_return);
}

DEFINE_TRAMPOLINE(Bool, XineramaIsActive, (Display *dpy))
Bool XineramaIsActive(Display *dpy)
{
    called_XineramaIsActive++;
    if (disable_XineramaIsActive) {
        return False;
    }
    return trampoline_XineramaIsActive(dpy);
}

DEFINE_TRAMPOLINE(XineramaScreenInfo *, XineramaQueryScreens,
                  (Display *dpy, int *number_return))
XineramaScreenInfo *XineramaQueryScreens(Display *dpy, int *number_return)
{
    called_XineramaQueryScreens++;
    return trampoline_XineramaQueryScreens(dpy, number_return);
}

/*---------------------------- glX wrappers -----------------------------*/

DEFINE_TRAMPOLINE_OPT(Bool, glXQueryExtension,
                      (Display *dpy, int *event_base_return,
                       int *error_base_return))
Bool glXQueryExtension(Display *dpy, int *event_base_return,
                       int *error_base_return)
{
    if (disable_glXQueryExtension) {
        return False;
    }
    return trampoline_glXQueryExtension(dpy, event_base_return,
                                        error_base_return);
}

DEFINE_TRAMPOLINE(GLXWindow, glXCreateWindow,
                  (Display *dpy, GLXFBConfig config, Window win,
                   const int *attribList))
GLXWindow glXCreateWindow(Display *dpy, GLXFBConfig config, Window win,
                          const int *attribList)
{
    if (disable_glXCreateWindow) {
        return None;
    }
    return trampoline_glXCreateWindow(dpy, config, win, attribList);
}

DEFINE_TRAMPOLINE(GLXContext, glXCreateNewContext,
                  (Display *dpy, GLXFBConfig config, int renderType,
                   GLXContext shareList, Bool direct))
GLXContext glXCreateNewContext(Display *dpy, GLXFBConfig config,
                               int renderType, GLXContext shareList,
                               Bool direct)
{
    if (disable_glXCreateNewContext) {
        return None;
    }
    return trampoline_glXCreateNewContext(dpy, config, renderType,
                                          shareList, direct);
}

DEFINE_TRAMPOLINE(Bool, glXMakeContextCurrent,
                  (Display *dpy, GLXDrawable draw, GLXDrawable read,
                   GLXContext ctx))
Bool glXMakeContextCurrent(Display *dpy, GLXDrawable draw,
                           GLXDrawable read, GLXContext ctx)
{
    if (disable_glXMakeContextCurrent) {
        return False;
    }
    return trampoline_glXMakeContextCurrent(dpy, draw, read, ctx);
}

/*************************************************************************/
/*************************************************************************/
