/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/macosx/graphics.c: Graphics and rendering functionality for
 * Mac OS X.
 */

#define IN_SYSDEP

#import "src/base.h"
#import "src/graphics.h"
#import "src/math.h"
#import "src/memory.h"
#import "src/mutex.h"
#import "src/sysdep.h"
#import "src/sysdep/macosx/graphics.h"
#import "src/sysdep/macosx/input.h"
#import "src/sysdep/macosx/util.h"
#import "src/sysdep/macosx/window.h"
#import "src/sysdep/opengl/opengl.h"
#import "src/sysdep/opengl/internal.h"  // Needed for Steam bug workaround.

#import "src/sysdep/macosx/osx-headers.h"
#import <AppKit/NSApplication.h>
#import <AppKit/NSImage.h>
#import <AppKit/NSOpenGL.h>
#import <AppKit/NSScreen.h>
#import <CoreFoundation/CFArray.h>
#import <CoreFoundation/CFDictionary.h>
#import <CoreFoundation/CFString.h>
#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSAutoreleasePool.h>
#import <Foundation/NSDictionary.h>
#import <IOKit/graphics/IOGraphicsLib.h>
#import <OpenGL/OpenGL.h>
#import <dlfcn.h>

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Duration of fade-out/in when changing video modes. */
#define FADE_TIME  0.25

/*-----------------------------------------------------------------------*/

/* Graphics capability structure returned to high-level code.  The display
 * device count and display mode list are filled in at initialization time. */
static SysGraphicsInfo graphics_info = {
    .has_windowed_mode = 1,
    .num_devices = 0,
    .num_modes = 0,
    .modes = NULL,
};

/*-----------------------------------------------------------------------*/

/* List of available display devices. */
typedef struct DisplayDevice DisplayDevice;
struct DisplayDevice {
    int screen;                     // Index into [NSScreen screens].
    CGDirectDisplayID display_id;
    char *name;                     // NULL if not known.
    CFArrayRef modes;               // Indexed by VideoMode.mode.
    CGDisplayModeRef default_mode;  // As returned by CGDisplayCopyDisplayMode.
    int default_mode_index;         // -1 if not found in mode array.
};
static DisplayDevice *display_devices;
static int num_display_devices;

/* List of available video modes. */
typedef struct VideoMode VideoMode;
struct VideoMode {
    int device;  // Index into display_devices[].
    int mode;    // Index into DisplayDevice.modes[].
    int width, height;
    double refresh;
};
static VideoMode *video_modes;
static int num_video_modes;

/* Currently active video mode, or -1 if the mode hasn't been changed.
 * This is statically initialized to avoid confusing
 * macosx_reset_video_mode() if we error out during setup. */
static int current_video_mode = -1;

/* Video mode for the current fullscreen window.  Used to record the
 * current video mode while the window is minimized. */
static int saved_video_mode;

/*-----------------------------------------------------------------------*/

/* Have we been initialized? */
static uint8_t initted;

/* Requested OpenGL version (0 if not set). */
static int desired_opengl_major, desired_opengl_minor;

/* Should we enable OpenGL debugging? */
static uint8_t use_opengl_debug;

/* Should we show the mouse pointer? */
static uint8_t show_mouse_pointer;

/* Should the window (in windowed mode) be centered when opened? */
static uint8_t center_window;

/* Is the current window in fullscreen mode? */
static uint8_t window_fullscreen;

/* Is fullscreen mode selected (for the next mode change)? */
static uint8_t fullscreen_wanted;

/* Is multisampling enabled, and with how many samples? */
static uint8_t multisample;
static int multisample_samples;

/* Is vertical sync enabled? */
static uint8_t vsync;

/* Should the window be resizable? */
static uint8_t window_resizable;

/* Desired display device for new windows (display_devices[] index). */
static int desired_device;

/* Desired refresh rate for fullscreen windows. */
static float fullscreen_refresh;

/* Desired depth and stencil buffer bit depths. */
static int depth_bits, stencil_bits;

/* Requested depth and stencil sizes. */
static int depth_bits;
static int stencil_bits;

/* Requested number of samples for multisampling. */
static int samples;

/* Current display (NSWindow). */
static void *window;  // NULL if no window is open.
/* Display device on which the window was opened. */
static int window_device;

/* Title string for window (owned by us). */
static char *window_title;

/* Window resize limits. */
static int window_min_width, window_min_height;
static int window_max_width, window_max_height;
static int window_min_aspect_x, window_min_aspect_y;
static int window_max_aspect_x, window_max_aspect_y;

/*-----------------------------------------------------------------------*/

/* Convenience macros which wrap a CoreGraphics or CoreGL call and return
 * whether the call succeeded, logging an error message if not. */
#define CG_CHECK(expr)  \
    (check_call((expr), CGErrorString, #expr, __LINE__, __FUNCTION__))
#define CGL_CHECK(expr)  \
    (check_call((expr), CGLErrorString_, #expr, __LINE__, __FUNCTION__))
/* CGLError is defined as an enum, so we need to insert a conversion step. */
static inline const char *CGLErrorString_(int32_t error) {
    return CGLErrorString(error);
}

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * enumerate_modes:  Look up and cache available display modes on all displays.
 *
 * [Return value]
 *     True on success, false on error.
 */
static int enumerate_modes(void);

/**
 * set_video_mode:  Switch the display to the given video mode.
 *
 * [Parameters]
 *     mode_index: Mode to switch to (index into video_modes[]).
 * [Return value]
 *     True on success, false on error.
 */
static int set_video_mode(int mode_index);

/**
 * open_window:  Open a new window of the given size and mode, and
 * initialize an OpenGL context for the window.  The display mode is
 * changed as required.
 *
 * [Parameters]
 *     width: Desired window width.
 *     height: Desired window height.
 *     device: Index of display device on which to open window.
 *     fullscreen: True for fullscreen mode, false for a regular window.
 *     resizable: True for a resizable window, false for a non-resizable
 *         window.  If fullscreen is true, this affects whether the window
 *         is resizable after changing out of fullscreen mode.
 *     center: True to center a non-fullscreen window, false for default
 *         placement.
 * [Return value]
 *     Newly opened window, or NULL on error.
 */
static void *open_window(int width, int height, int device, int fullscreen,
                         int resizable, int center);

/**
 * close_window:  Close the current window.  The display mode is not changed.
 */
static void close_window(void);

/**
 * setup_opengl:  Initialize the OpenGL subsystem.  This must be called
 * after a display surface has been created.
 *
 * [Parameters]
 *     width, height: Display size, in pixels.
 */
static int setup_opengl(int width, int height);

/**
 * create_gl_shader_compilation_context:  Create and make current a new
 * GL context for the current thread which can be used to compile shaders.
 *
 * [Return value]
 *     True on success or if the current thread already has a GL context,
 *     false on error.
 */
static int create_gl_shader_compilation_context(void);

/**
 * dlGetProcAddress:  Look up the address of a function using libdl.
 * Required because OS X seems to have no equivalent to glXGetProcAddress()
 * or eglGetProcAddress().
 *
 * [Parameters]
 *     name: Name of function to look up.
 * [Return value]
 *     Function pointer, or NULL if the function is not found.
 */
static void *dlGetProcAddress(const char *name);

/**
 * get_opengl_buffer_params:  Return the number of depth bits, multisample
 * buffers, multisample samples, and stencil bits for the current window.
 *
 * [Parameters]
 *     depth_ret: Pointer to variable to receive the number of depth bits.
 *     buffers_ret: Pointer to variable to receive the number of multisample
 *         buffers.
 *     samples_ret: Pointer to variable to receive the number of multisample
 *         samples.
 *     stencil_ret: Pointer to variable to receive the number of stencil bits.
 * [Return value]
 *     True on success, false on error.
 */
static int get_opengl_buffer_params(int *depth_ret, int *buffers_ret,
                                    int *samples_ret, int *stencil_ret);

/**
 * check_call:  Return the success status indicated by the given status
 * code, logging an error message if the status code indicates failure.
 * Implements the CG_CHECK() and CGL_CHECK() macros.
 *
 * [Parameters]
 *     result: Result of the system call.
 *     strerror_func: Function which returns an error string for the given
 *         error code.
 *     expr: String containing the expression evaluated.
 *     line: Line number of the expression.
 *     function: Name of the function containing the expression.
 * [Return value]
 *     True if result == 0 (success), false otherwise.
 */
static int check_call(int32_t result, const char *(*strerror_func)(int32_t),
                      const char *expr, int line, const char *function);

/**
 * CGErrorString:  Return a string describing the given CoreGraphics error
 * code, analagous to the OpenGL framework's CGLErrorString().  (It seems
 * the CoreGraphics library itself is missing such a function.)
 *
 * [Parameters]
 *     error: CoreGraphics error code.
 * [Return value]
 *     String describing the error.
 */
static const char *CGErrorString(CGError error);

/*************************************************************************/
/***************** Interface: Basic graphics operations ******************/
/*************************************************************************/

const SysGraphicsInfo *sys_graphics_init(void)
{
    PRECOND(!initted, goto error_return);

    if (UNLIKELY(!enumerate_modes())) {
        DLOG("Failed to look up video modes");
        goto error_return;
    }

    center_window = 0;
    current_video_mode = -1;
    depth_bits = 16;
    desired_device = 0;
    desired_opengl_major = 0;
    desired_opengl_minor = 0;
    fullscreen_refresh = 0;
    fullscreen_wanted = 0;
    multisample = 0;
    multisample_samples = 1;
    saved_video_mode = -1;
    show_mouse_pointer = 0;
    stencil_bits = 0;
    use_opengl_debug = 0;
    vsync = 1;
    window = NULL;
    window_device = kCGNullDirectDisplay;
    window_fullscreen = 0;
    window_max_aspect_x = 0;
    window_max_aspect_y = 0;
    window_max_width = 0;
    window_max_height = 0;
    window_min_aspect_x = 0;
    window_min_aspect_y = 0;
    window_min_width = 0;
    window_min_height = 0;
    window_resizable = 0;
    window_title = NULL;

    initted = 1;
    return &graphics_info;

  error_return:
    return NULL;
}

/*-----------------------------------------------------------------------*/

void sys_graphics_cleanup(void)
{
    PRECOND(initted, return);

    macosx_reset_video_mode();

    if (window) {
        close_window();
    }
    mem_free(window_title);
    window_title = NULL;

    mem_free((void *)graphics_info.modes);
    graphics_info.modes = NULL;
    graphics_info.num_modes = 0;
    mem_free(video_modes);
    video_modes = NULL;
    current_video_mode = -1;
    for (int i = 0; i < num_display_devices; i++) {
        mem_free(display_devices[i].name);
        if (display_devices[i].modes) {
            CFRelease(display_devices[i].modes);
        }
    }
    mem_free(display_devices);
    display_devices = NULL;

    initted = 0;
}

/*-----------------------------------------------------------------------*/

int sys_graphics_device_width(void)
{
    const int device = window ? window_device : desired_device;
    return CGDisplayModeGetWidth(display_devices[device].default_mode);
}

/*-----------------------------------------------------------------------*/

int sys_graphics_device_height(void)
{
    const int device = window ? window_device : desired_device;
    return CGDisplayModeGetHeight(display_devices[device].default_mode);
}

/*-----------------------------------------------------------------------*/

int sys_graphics_set_display_attr(const char *name, va_list args)
{
    if (strcmp(name, "center_window") == 0) {
        const int value = va_arg(args, int);
        center_window = (value != 0);
        return 1;
    }

    if (strcmp(name, "depth_bits") == 0) {
        const int value = va_arg(args, int);
        if (value < 0) {
            DLOG("Invalid value for attribute %s: %d", name, value);
            return 0;
        }
        depth_bits = value;
        return 1;
    }

    if (strcmp(name, "device") == 0) {
        const int value = va_arg(args, int);
        if (value < 0 || value >= num_display_devices) {
            DLOG("Invalid value for attribute %s: %d", name, value);
            return 0;
        }
        desired_device = value;
        return 1;
    }

    if (strcmp(name, "multisample") == 0) {
        const int value = va_arg(args, int);
        if (value <= 0) {
            DLOG("Invalid value for attribute %s: %d", name, value);
            return 0;
        }
        multisample = (value > 1);
        multisample_samples = value;
        return 1;
    }

    if (strcmp(name, "opengl_debug") == 0) {
        use_opengl_debug = (va_arg(args, int) != 0);
        if (window) {
            opengl_enable_debug(use_opengl_debug);
        }
        return 1;
    }

    if (strcmp(name, "opengl_version") == 0) {
        desired_opengl_major = va_arg(args, int);
        desired_opengl_minor = va_arg(args, int);
        return 1;
    }

    if (strcmp(name, "refresh_rate") == 0) {
        const float value = (float)va_arg(args, double);
        if (!(value >= 0)) {
            DLOG("Invalid value for attribute %s: %g", name, value);
            return 0;
        }
        fullscreen_refresh = value;
        return 1;
    }

    if (strcmp(name, "stencil_bits") == 0) {
        const int value = va_arg(args, int);
        if (value < 0) {
            DLOG("Invalid value for attribute %s: %d", name, value);
            return 0;
        }
        stencil_bits = value;
        return 1;
    }

    if (strcmp(name, "vsync") == 0) {
        vsync = (va_arg(args, int) != 0);
        return 1;
    }

    if (strcmp(name, "window") == 0) {
        fullscreen_wanted = (va_arg(args, int) == 0);
        return 1;
    }

    if (strcmp(name, "window_resizable") == 0) {
        window_resizable = (va_arg(args, int) != 0);
        if (window && current_video_mode == -1) {
            SILWindow_set_resizable(window, window_resizable);
        }
        return 1;
    }

    return 0;
}

/*-----------------------------------------------------------------------*/

GraphicsError sys_graphics_set_display_mode(int width, int height)
{
    /* Look up the video mode corresponding to the requested display size
     * and screen. */
    int fullscreen_video_mode = -1;
    if (fullscreen_wanted) {
        int current_width, current_height;
        if (current_video_mode >= 0) {
            current_width = video_modes[current_video_mode].width;
            current_height = video_modes[current_video_mode].height;
        } else {
            current_width = sys_graphics_device_width();
            current_height = sys_graphics_device_height();
        }
        if (fullscreen_refresh == 0
         && width == current_width
         && height == current_height) {
            /* Avoid unnecessary mode changes. */
            fullscreen_video_mode = current_video_mode;
        } else {
            float best_refresh;
            for (int i = 0; i < num_video_modes; i++) {
                if (video_modes[i].width == width
                 && video_modes[i].height == height
                 && video_modes[i].device == desired_device) {
                    const double refresh = video_modes[i].refresh;
                    int better;
                    if (fullscreen_video_mode < 0) {
                        better = 1;
                    } else if (fullscreen_refresh > 0) {
                        better = (fabs(refresh - fullscreen_refresh)
                                  < fabs(best_refresh - fullscreen_refresh));
                    } else {  // fullscreen_refresh == 0
                        better = (refresh > best_refresh);
                    }
                    if (better) {
                        fullscreen_video_mode = i;
                        best_refresh = refresh;
                    }
                }
            }
            if (fullscreen_video_mode < 0) {
                DLOG("No video mode matching %dx%d on display device %d",
                     width, height, desired_device);
                return GRAPHICS_ERROR_MODE_NOT_SUPPORTED;
            }
        }
    }
    const int new_video_mode = fullscreen_wanted ? fullscreen_video_mode : -1;

    /* If changing display modes, fade out and perform the mode change. */
    CGDisplayFadeReservationToken fade_token =
        kCGDisplayFadeReservationInvalidToken;
    if (new_video_mode != current_video_mode) {
        if (CG_CHECK(CGAcquireDisplayFadeReservation(5, &fade_token))) {
            CG_CHECK(CGDisplayFade(fade_token, FADE_TIME, 0, 1,
                                   0, 0, 0, true));
        }
        int ok;
        if (new_video_mode >= 0) {
            ok = set_video_mode(fullscreen_video_mode);
        } else {
            macosx_reset_video_mode();
            ok = 1;
        }
        if (!ok) {
            if (fade_token != kCGDisplayFadeReservationInvalidToken) {
                CG_CHECK(CGDisplayFade(fade_token, FADE_TIME, 1, 0,
                                       0, 0, 0, false));
                CG_CHECK(CGReleaseDisplayFadeReservation(fade_token));
            }
            return GRAPHICS_ERROR_MODE_NOT_SUPPORTED;
        }
    }

    /* See if we can switch fullscreen or change size without closing the
     * window. */
    int window_depth, window_buffers, window_samples, window_stencil;
    if (window
     && LIKELY(get_opengl_buffer_params(&window_depth, &window_buffers,
                                        &window_samples, &window_stencil))
     && window_depth >= depth_bits
     && (!multisample || (window_buffers && window_samples >= samples))
     && window_stencil >= stencil_bits) {
        /* OSX seems to drop input events at least during the fullscreen
         * enter/exit animation and possibly during ordinary resize, so
         * force-clear all input state to try and avoid desyncs. */
        macosx_clear_window_input_state();

        const CGSize size = SILWindow_content_frame(window).size;
        int ok;
        if (fullscreen_wanted != window_fullscreen) {
            ok = SILWindow_set_fullscreen(window, fullscreen_wanted,
                                          window_device, width, height);

        } else if (width == size.width && height == size.height) {
            ok = 1;
        } else {
            ok = SILWindow_resize(window, width, height);
        }

        if (ok) {
            if (fade_token != kCGDisplayFadeReservationInvalidToken) {
                CG_CHECK(CGDisplayFade(fade_token, FADE_TIME, 1, 0,
                                       0, 0, 0, false));
                CG_CHECK(CGReleaseDisplayFadeReservation(fade_token));
            }
            window_fullscreen = fullscreen_wanted;
            opengl_set_display_size(width, height);
            return GRAPHICS_ERROR_SUCCESS;
        }
    }

    const int window_was_open = (window != NULL);
    if (window_was_open) {
        macosx_clear_window_input_state();
        close_window();
    }

    saved_video_mode = -1;
    window = open_window(width, height, desired_device, fullscreen_wanted,
                         window_resizable && new_video_mode == -1,
                         center_window);
    if (fade_token != kCGDisplayFadeReservationInvalidToken) {
        CG_CHECK(CGDisplayFade(fade_token, FADE_TIME, 1, 0, 0, 0, 0, false));
        CG_CHECK(CGReleaseDisplayFadeReservation(fade_token));
    }
    if (!window) {
        DLOG("Failed to create window (device %d, %dx%d%s)", desired_device,
             width, height, fullscreen_wanted ? ", full-screen" : "");
        return GRAPHICS_ERROR_MODE_NOT_SUPPORTED;
    }

    /* We can't call OpenGL functions until we have a context (i.e., a
     * window). */
    opengl_lookup_functions(dlGetProcAddress);

    /* Check that we got the OpenGL buffer parameters we requested. */
    if (UNLIKELY(!get_opengl_buffer_params(&window_depth, &window_buffers,
                                           &window_samples, &window_stencil))) {
        return GRAPHICS_ERROR_UNKNOWN;
    }
    if (window_depth < depth_bits) {
        DLOG("Requested %d depth bits but only got %d",
             depth_bits, window_depth);
        return GRAPHICS_ERROR_MODE_NOT_SUPPORTED;
    }
    if (multisample) {
        if (!window_buffers) {
            DLOG("Requested multisampling but didn't get it");
            return GRAPHICS_ERROR_MODE_NOT_SUPPORTED;
        }
        if (window_samples < samples) {
            DLOG("Requested multisampling with %d samples but only got %d",
                 samples, window_samples);
            return GRAPHICS_ERROR_MODE_NOT_SUPPORTED;
        }
    }
    if (window_stencil < stencil_bits) {
        DLOG("Requested %d stencil bits but only got %d",
             stencil_bits, window_stencil);
        return GRAPHICS_ERROR_MODE_NOT_SUPPORTED;
    }

    /* Set up OpenGL now that we have a context to work with. */
    opengl_enable_debug(use_opengl_debug);
    if (UNLIKELY(!setup_opengl(width, height))) {
        return GRAPHICS_ERROR_BACKEND_TOO_OLD;
    }

    /* Enable multisampling if requested. */
    if (multisample) {
        glEnable(GL_MULTISAMPLE);
    } else {
        glDisable(GL_MULTISAMPLE);
    }

    /* Confine the mouse pointer to the window if requested. */
    macosx_update_mouse_grab(-1);

    window_fullscreen = SILWindow_is_fullscreen(window);
    window_device = desired_device;
    return window_was_open ? GRAPHICS_ERROR_STATE_LOST
                           : GRAPHICS_ERROR_SUCCESS;
}

/*-----------------------------------------------------------------------*/

int sys_graphics_display_is_window(void)
{
    return window && !window_fullscreen;
}

/*-----------------------------------------------------------------------*/

void sys_graphics_set_window_title(const char *title)
{
    mem_free(window_title);
    window_title = mem_strdup(title, 0);
    if (!window_title) {
        DLOG("Failed to strdup title!");
        return;
    }
    if (window) {
        SILWindow_set_title(window, window_title);
    }
}

/*-----------------------------------------------------------------------*/

void sys_graphics_set_window_icon(SysTexture *texture)
{
    const int width = sys_texture_width(texture);
    const int height = sys_texture_height(texture);

    uint8_t *pixels = sys_texture_lock(texture, SYS_TEXTURE_LOCK_NORMAL,
                                       0, 0, width, height);
    if (UNLIKELY(!pixels)) {
        DLOG("Failed to lock texture for icon");
        return;
    }

    NSImage *image = macosx_create_image(pixels, width, height);
    sys_texture_unlock(texture, 0);
    if (UNLIKELY(!image)) {
        DLOG("Failed to create image for new icon");
        return;
    }

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    /* Normally we could do "NSApp.applicationIconImage = image", but for
     * some reason NSApp is declared as "id" instead of "NSApplication *". */
    [NSApp setApplicationIconImage:image];
    [image release];

    [pool release];
}

/*-----------------------------------------------------------------------*/

void sys_graphics_set_window_resize_limits(
    UNUSED int min_width, UNUSED int min_height,
    UNUSED int max_width, UNUSED int max_height,
    UNUSED int min_aspect_x, UNUSED int min_aspect_y,
    UNUSED int max_aspect_x, UNUSED int max_aspect_y)
{
    window_min_width = min_width;
    window_min_height = min_height;
    window_max_width = max_width;
    window_max_height = max_height;
    window_min_aspect_x = min_aspect_x;
    window_min_aspect_y = min_aspect_y;
    window_max_aspect_x = max_aspect_x;
    window_max_aspect_y = max_aspect_y;

    if (window) {
        SILWindow_set_resize_limits(window,
                                    window_min_width, window_min_height,
                                    window_max_width, window_max_height,
                                    window_min_aspect_x, window_min_aspect_y,
                                    window_max_aspect_x, window_max_aspect_y);
    }
}

/*-----------------------------------------------------------------------*/

void sys_graphics_show_mouse_pointer(int on)
{
    show_mouse_pointer = (on != 0);
    if (window) {
        SILWindow_set_show_pointer(window, show_mouse_pointer);
    }
}

/*-----------------------------------------------------------------------*/

int sys_graphics_get_mouse_pointer_state(void)
{
    return show_mouse_pointer;
}

/*-----------------------------------------------------------------------*/

void sys_graphics_get_frame_period(int *numerator_ret, int *denominator_ret)
{
    double refresh_rate;
    if (current_video_mode >= 0) {
        refresh_rate = video_modes[current_video_mode].refresh;
    } else {
        const int device = window ? window_device : desired_device;
        refresh_rate = CGDisplayModeGetRefreshRate(
            display_devices[device].default_mode);
    }
    if (refresh_rate == 0) {
        *numerator_ret = 0;
        *denominator_ret = 1;
    } else if (fabs(refresh_rate - round(refresh_rate)) < 1e-6) {
        *numerator_ret = 1;
        *denominator_ret = iround(refresh_rate);
    } else if (fabs(refresh_rate*1.001-round(refresh_rate*1.001)) < 1e-6) {
        *numerator_ret = 1001;
        *denominator_ret = iround(refresh_rate*1001);
    } else {
        *numerator_ret = 1000;
        *denominator_ret = iround(refresh_rate*1000);
    }
}

/*-----------------------------------------------------------------------*/

int sys_graphics_has_focus(void)
{
    return window && SILWindow_has_focus(window);
}

/*-----------------------------------------------------------------------*/

void sys_graphics_start_frame(int *width_ret, int *height_ret)
{
    if (!window) {
        *width_ret = 0;
        *height_ret = 0;
        return;
    }

    const CGSize size = SILWindow_content_frame(window).size;
    *width_ret = iroundf(size.width);
    *height_ret = iroundf(size.height);

    SILWindow_update_gl_context(window);

    opengl_start_frame();
}

/*-----------------------------------------------------------------------*/

void sys_graphics_finish_frame(void)
{
    if (!window) {
        return;
    }

    /* At least as of January 2018, the library injected by the Mac Steam
     * client to implement the Steam overlay (gameoverlayrenderer.dylib)
     * fails to properly clear vertex array state or set up a VAO when
     * taking a screenshot, causing screenshots to show up as corrupted
     * data because the glDrawArrays() call used to copy the window
     * framebuffer fails. */
    glBindVertexArray(0);
    opengl_primitive_reset_bindings();
    static GLuint steam_hack_vao = 0;
    if (!steam_hack_vao) {
        glGenVertexArrays(1, &steam_hack_vao);
    }
    glBindVertexArray(steam_hack_vao);

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    [[NSOpenGLContext currentContext] flushBuffer];
    [pool release];
}

/*-----------------------------------------------------------------------*/

void sys_graphics_sync(int flush)
{
    opengl_sync();
    if (flush) {
        opengl_free_dead_resources(1);
    }
}

/*************************************************************************/
/*********************** Internal utility routines ***********************/
/*************************************************************************/

CGDirectDisplayID macosx_display_id(int display)
{
    PRECOND(display >= 0 && display < num_display_devices, return 0);
    return display_devices[display].display_id;
}

/*-----------------------------------------------------------------------*/

void *macosx_window(void)
{
    return window;
}

/*-----------------------------------------------------------------------*/

int macosx_window_x(void)
{
    if (window) {
        return iroundf(SILWindow_content_frame(window).origin.x);
    } else {
        return 0;
    }
}

/*-----------------------------------------------------------------------*/

int macosx_window_y(void)
{
    if (window) {
        return iroundf(SILWindow_content_frame(window).origin.y);
    } else {
        return 0;
    }
}

/*-----------------------------------------------------------------------*/

int macosx_window_width(void)
{
    if (window) {
        return iroundf(SILWindow_content_frame(window).size.width);
    } else {
        return 0;
    }
}

/*-----------------------------------------------------------------------*/

int macosx_window_height(void)
{
    if (window) {
        return iroundf(SILWindow_content_frame(window).size.height);
    } else {
        return 0;
    }
}

/*-----------------------------------------------------------------------*/

char *macosx_get_window_title(void)
{
    if (window) {
        return SILWindow_get_title(window);
    } else {
        return NULL;
    }
}

/*-----------------------------------------------------------------------*/

void macosx_close_window(void)
{
    if (window) {
        close_window();
    }
}

/*-----------------------------------------------------------------------*/

void macosx_handle_focus_change(int focus)
{
    if (!window) {
        return;  // Ignore focus change during window creation.
    }

    if (focus) {

        if (window_fullscreen) {
            if (saved_video_mode >= 0 && UNLIKELY(!set_video_mode(saved_video_mode))) {
                DLOG("Failed to restore video mode");
                /* Try to recover by switching to windowed mode. */
                const int saved_fullscreen = fullscreen_wanted;
                fullscreen_wanted = 0;
                sys_graphics_set_display_mode(macosx_window_width(),
                                              macosx_window_height());
                fullscreen_wanted = saved_fullscreen;
            }
            saved_video_mode = -1;
        }
        macosx_update_mouse_grab(-1);

    } else {  // !focus

        if (window_fullscreen) {
            saved_video_mode = current_video_mode;
            macosx_reset_video_mode();
        } else {
            saved_video_mode = -1;
        }
        macosx_update_mouse_grab(0);

    }  // if (focus)
}

/*-----------------------------------------------------------------------*/

void macosx_reset_video_mode(void)
{
    if (current_video_mode >= 0) {
        const int device_index = video_modes[current_video_mode].device;
        const DisplayDevice *device = &display_devices[device_index];
        const int default_mode = device->default_mode_index;
        if (video_modes[current_video_mode].mode != default_mode) {
            if (!CG_CHECK(CGDisplaySetDisplayMode(
                              device->display_id, device->default_mode,
                              NULL))) {
                DLOG("Failed to restore original display mode on screen %d"
                     " (%s)", device->screen, device->name);
            }
        }
        current_video_mode = -1;
    }
}

/*-----------------------------------------------------------------------*/

void macosx_window_changed_fullscreen(void *window_, int fullscreen)
{
    if (!window) {  // We could get called while the window is being created.
        return;
    }

    ASSERT(window_ == window, return);
    window_fullscreen = (fullscreen != 0);
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static int enumerate_modes(void)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    int num_devices = [NSScreen screens].count;
    if (UNLIKELY(!num_devices)) {
        DLOG("No display devices found");
        goto error_return;
    }
    display_devices = mem_alloc(sizeof(*display_devices) * num_devices, 0, 0);
    if (UNLIKELY(!display_devices)) {
        DLOG("No memory for %d display devices", num_devices);
        goto error_return;
    }
    num_display_devices = 0;

    ASSERT(!video_modes, video_modes = NULL);
    num_video_modes = 0;

    for (int i = 0; i < num_devices; i++) {
        NSDictionary *desc =
            ((NSScreen *)[[NSScreen screens]
                             objectAtIndex:i]).deviceDescription;
        NSNumber *ns_display_id = [desc objectForKey:@"NSScreenNumber"];
        ASSERT(ns_display_id);
        const CGDirectDisplayID display_id = [ns_display_id unsignedIntValue];

        char *device_name = NULL;
#pragma clang diagnostic push
#pragma clang diagnostic ignored  "-Wdeprecated-declarations"
        io_service_t display_ioport = CGDisplayIOServicePort(display_id);
#pragma clang diagnostic pop
        CFDictionaryRef display_info = IODisplayCreateInfoDictionary(
            display_ioport, kIODisplayOnlyPreferredName);
        if (LIKELY(display_info)) {
            CFDictionaryRef names = CFDictionaryGetValue(
                display_info, CFSTR(kDisplayProductName));
            if (LIKELY(names)) {
                const int count = CFDictionaryGetCount(names);
                if (LIKELY(count > 0)) {
                    const void **values = mem_alloc(
                        sizeof(*values) * count, 0, MEM_ALLOC_TEMP);
                    if (LIKELY(values)) {
                        CFDictionaryGetKeysAndValues(names, NULL, values);
                        CFStringRef name = (CFStringRef)values[0];
                        const int name_copy_size = CFStringGetLength(name)*3+1;
                        char *name_copy = mem_alloc(name_copy_size, 0, 0);
                        if (UNLIKELY(!name_copy)) {
                            DLOG("No memory for copy of display %d name", i);
                        } else if (LIKELY(CFStringGetCString(
                                              name, name_copy, name_copy_size,
                                              kCFStringEncodingUTF8))) {
                            device_name = name_copy;
                        } else {
                            mem_free(name_copy);
                        }
                        mem_free(values);
                    }
                }
            }
            CFRelease(display_info);
        }

        CGDisplayModeRef current_mode = CGDisplayCopyDisplayMode(display_id);
        if (UNLIKELY(!current_mode)) {
            DLOG("Failed to get current mode for screen %d (%s)", i,
                 device_name);
            mem_free(device_name);
            continue;
        }
        const int current_width = CGDisplayModeGetWidth(current_mode);
        const int current_height = CGDisplayModeGetHeight(current_mode);
        const double current_refresh =
            CGDisplayModeGetRefreshRate(current_mode);

        CFArrayRef modes = CGDisplayCopyAllDisplayModes(display_id, NULL);
        if (UNLIKELY(!modes) || UNLIKELY(CFArrayGetCount(modes) == 0)) {
            DLOG("Got no modes for screen %d (%s)", i, device_name);
            CFRelease(current_mode);
            mem_free(device_name);
            continue;
        }
        const int num_modes = CFArrayGetCount(modes);
        VideoMode *new_video_modes = mem_realloc(
            video_modes,
            sizeof(*new_video_modes) * (num_video_modes + num_modes), 0);
        if (UNLIKELY(!new_video_modes)) {
            DLOG("No memory to append %d modes", num_modes);
            CFRelease(modes);
            CFRelease(current_mode);
            mem_free(device_name);
            continue;
        }
        video_modes = new_video_modes;

        int current_mode_index = -1;
        for (int j = 0; j < num_modes; j++) {
            CGDisplayModeRef mode =
                (CGDisplayModeRef)CFArrayGetValueAtIndex(modes, j);
            ASSERT(mode, continue);
#pragma clang diagnostic push
#pragma clang diagnostic ignored  "-Wdeprecated-declarations"
            CFStringRef pixel_encoding = CGDisplayModeCopyPixelEncoding(mode);
#pragma clang diagnostic pop

            /* Only accept RGB modes with at least 8 bits per component.
             * OS X doesn't seem to provide a programmer-friendly way to
             * determine the number of bits per component, so this is
             * rather more complicated than it ought to be. */
            ASSERT(pixel_encoding, continue);
            char buf[1000];
            ASSERT(CFStringGetCString(pixel_encoding, buf, sizeof(buf),
                                      kCFStringEncodingUTF8));
            if (!strchr(buf, 'R')
             || !strchr(buf, 'G')
             || !strchr(buf, 'B')
             || strcmp(buf, IO16BitDirectPixels) == 0) {
                DLOG("Ignoring mode %dx%d with bad pixel format %s",
                     (int)CGDisplayModeGetWidth(mode),
                     (int)CGDisplayModeGetHeight(mode), buf);
                CFRelease(pixel_encoding);
                continue;
            }

            video_modes[num_video_modes].device = num_display_devices;
            video_modes[num_video_modes].mode = j;
            video_modes[num_video_modes].width = CGDisplayModeGetWidth(mode);
            video_modes[num_video_modes].height = CGDisplayModeGetHeight(mode);
            video_modes[num_video_modes].refresh =
                CGDisplayModeGetRefreshRate(mode);
            num_video_modes++;
            if ((int)CGDisplayModeGetWidth(mode) == current_width
             && (int)CGDisplayModeGetHeight(mode) == current_height
             && fabs(CGDisplayModeGetRefreshRate(mode)
                     - current_refresh) < 0.001) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored  "-Wdeprecated-declarations"
                CFStringRef current_pixenc =
                    CGDisplayModeCopyPixelEncoding(current_mode);
#pragma clang diagnostic pop
                if (CFStringCompare(pixel_encoding, current_pixenc,
                                    0) == kCFCompareEqualTo) {
                    current_mode_index = j;
                }
                CFRelease(current_pixenc);
            }

            CFRelease(pixel_encoding);
        }

        display_devices[num_display_devices].screen = i;
        display_devices[num_display_devices].display_id = display_id;
        display_devices[num_display_devices].name = device_name;
        display_devices[num_display_devices].modes = modes;
        display_devices[num_display_devices].default_mode = current_mode;
        display_devices[num_display_devices].default_mode_index =
            current_mode_index;
        num_display_devices++;
    }

    if (UNLIKELY(num_display_devices == 0)) {
        DLOG("No valid display devices found");
        goto error_free_video_modes;
    }
    if (UNLIKELY(num_video_modes == 0)) {
        DLOG("No valid display modes found");
        goto error_free_video_modes;
    }

    GraphicsDisplayModeEntry *modes =
        mem_alloc(sizeof(*modes) * num_video_modes, 0, 0);
    if (UNLIKELY(!modes)) {
        DLOG("No memory for %d modes", num_video_modes);
        goto error_free_video_modes;
    }
    for (int i = 0; i < num_video_modes; i++) {
        modes[i].device = video_modes[i].device;
        modes[i].device_name = display_devices[video_modes[i].device].name;
        modes[i].width = video_modes[i].width;
        modes[i].height = video_modes[i].height;
        modes[i].refresh = video_modes[i].refresh;
    }

    graphics_info.num_devices = num_display_devices;
    graphics_info.modes = modes;
    graphics_info.num_modes = num_video_modes;

    [pool release];
    return 1;

  error_free_video_modes:
    mem_free(video_modes);
    video_modes = NULL;
    for (int i = 0; i < num_display_devices; i++) {
        mem_free(display_devices[i].name);
        if (display_devices[i].modes) {
            CFRelease(display_devices[i].modes);
        }
    }
    mem_free(display_devices);
    display_devices = NULL;
  error_return:
    [pool release];
    return 0;
}

/*-----------------------------------------------------------------------*/

static int set_video_mode(int mode_index)
{
    PRECOND(mode_index >= 0 && mode_index < num_video_modes, return 0);

    if (mode_index == current_video_mode) {
        return 1;
    }

    const VideoMode *mode = &video_modes[mode_index];
    if (current_video_mode != -1
     && mode->device != video_modes[current_video_mode].device) {
        macosx_reset_video_mode();
    }

    const DisplayDevice *device = &display_devices[mode->device];
    if (current_video_mode < 0 && mode->mode == device->default_mode_index) {
        current_video_mode = mode_index;
        return 1;
    }

    CGDisplayModeRef cg_mode =
        (CGDisplayModeRef)CFArrayGetValueAtIndex(device->modes, mode->mode);
    ASSERT(cg_mode != NULL, return 0);
    if (!CG_CHECK(CGDisplaySetDisplayMode(device->display_id, cg_mode, NULL))) {
        DLOG("Failed to set mode %d (%dx%d) on screen %d (%s)", mode->mode,
             mode->width, mode->height, device->screen, device->name);
        return 0;
    }

    current_video_mode = mode_index;
    return 1;
}

/*-----------------------------------------------------------------------*/

static void *open_window(int width, int height, int device, int fullscreen,
                         int resizable, UNUSED int center)
{
    /* OS X only allows us to ask for OpenGL 3.2 (core) or 2.1 (legacy).
     * The legacy profile seems to be broken on some systems, so we always
     * create a 3.2 core profile; if the user asked for something beyond
     * 3.2, we just fail here. */
    if (desired_opengl_major > 3
     || (desired_opengl_major == 3 && desired_opengl_minor > 2)) {
        DLOG("OpenGL version %d.%d not supported",
             desired_opengl_major, desired_opengl_minor);
        return NULL;
    }

    CGLPixelFormatAttribute attributes[] = {
        kCGLPFAOpenGLProfile, (int)kCGLOGLPVersion_3_2_Core,
        kCGLPFAColorSize, 24,
        kCGLPFADepthSize, depth_bits,
        kCGLPFAStencilSize, stencil_bits,
        kCGLPFASamples, multisample_samples,
        kCGLPFADoubleBuffer,
        kCGLPFAMinimumPolicy,
        0,  // kCGLPFAMultisample, if multisample is true
        0
    };
    if (multisample) {
        attributes[lenof(attributes)-2] = kCGLPFAMultisample;
    }
    CGLPixelFormatObj pixel_format = NULL;
    GLint num_screens;
    if (!CGL_CHECK(CGLChoosePixelFormat(attributes, &pixel_format,
                                        &num_screens))
     || !pixel_format) {
        DLOG("Failed to choose a pixel format");
        return NULL;
    }

    /* OS X doesn't seem to have a way to request default placement of a
     * new window, so we just center the window regardless of whether
     * centering was requested. */
    NSScreen *screen = [[NSScreen screens]
                           objectAtIndex:display_devices[device].screen];
    const CGRect screen_bounds = screen.frame;
    const CGRect cocoa_bounds = screen.visibleFrame;
    const int x = iroundf(cocoa_bounds.origin.x - screen_bounds.origin.x
                          + (cocoa_bounds.size.width - width) / 2);
    const int y = iroundf(cocoa_bounds.origin.y - screen_bounds.origin.y
                          + (cocoa_bounds.size.height - height) / 2);

    void *win = SILWindow_create(x, y, width, height,
                                 display_devices[device].screen, fullscreen,
                                 resizable, pixel_format);
    CGLReleasePixelFormat(pixel_format);
    if (!win) {
        DLOG("Failed to create window");
        return NULL;
    }

    if (window_title) {
        SILWindow_set_title(win, window_title);
    }
    SILWindow_set_show_pointer(win, show_mouse_pointer);
    SILWindow_set_resize_limits(win,
                                window_min_width, window_min_height,
                                window_max_width, window_max_height,
                                window_min_aspect_x, window_min_aspect_y,
                                window_max_aspect_x, window_max_aspect_y);

    return win;
}

/*-----------------------------------------------------------------------*/

static void close_window(void)
{
    ASSERT(window, return);

    opengl_cleanup();
    macosx_update_mouse_grab(0);
    sys_input_text_set_state(0, NULL, NULL);

    SILWindow_destroy(window);
    window = NULL;
}

/*-----------------------------------------------------------------------*/

static int setup_opengl(int width, int height)
{
    const uint32_t gl_flags = OPENGL_FEATURE_FAST_DYNAMIC_VBO
                            | OPENGL_FEATURE_FAST_STATIC_VBO
                            | OPENGL_FEATURE_FRAMEBUFFERS
                            | OPENGL_FEATURE_MANDATORY_VAO;
    if (!opengl_init(width, height, gl_flags)) {
        DLOG("Failed to set up OpenGL!");
        return 0;
    }

    opengl_set_compile_context_callback(create_gl_shader_compilation_context);

    return 1;
}

/*-----------------------------------------------------------------------*/

static int create_gl_shader_compilation_context(void)
{
    if ([NSOpenGLContext currentContext]) {
        return 1;
    }

    if (!window) {
        DLOG("No window open, can't create a shader compilation context");
        return 0;
    }
    if (UNLIKELY(!SILWindow_create_gl_shader_compilation_context(window))) {
        DLOG("Failed to create GL shader compilation context");
        return 0;
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

static void *dlGetProcAddress(const char *name)
{
    return dlsym(RTLD_DEFAULT, name);
}

/*-----------------------------------------------------------------------*/

static int get_opengl_buffer_params(int *depth_ret, int *buffers_ret,
                                    int *samples_ret, int *stencil_ret)
{
    PRECOND(depth_ret != NULL, return 0);
    PRECOND(buffers_ret != NULL, return 0);
    PRECOND(samples_ret != NULL, return 0);
    PRECOND(stencil_ret != NULL, return 0);

    GLint value;
    opengl_clear_error();
    /* For totally incomprehensible reasons, the second parameter has to
     * be GL_DEPTH and not GL_DEPTH_ATTACHMENT when using the default
     * framebuffer. */
    glGetFramebufferAttachmentParameteriv(
        GL_DRAW_FRAMEBUFFER, GL_DEPTH,
        GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &value);
    if (value != GL_NONE) {
        glGetFramebufferAttachmentParameteriv(
            GL_FRAMEBUFFER, GL_DEPTH,
            GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &value);
        *depth_ret = value;
    } else {
        *depth_ret = 0;
    }
    glGetIntegerv(GL_SAMPLE_BUFFERS, &value);
    *buffers_ret = value;
    glGetIntegerv(GL_SAMPLES, &value);
    *samples_ret = value;
    glGetFramebufferAttachmentParameteriv(
        GL_DRAW_FRAMEBUFFER, GL_STENCIL,
        GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &value);
    if (value != GL_NONE) {
        glGetFramebufferAttachmentParameteriv(
            GL_FRAMEBUFFER, GL_STENCIL,
            GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &value);
        *stencil_ret = value;
    } else {
        *stencil_ret = 0;
    }
    const GLenum gl_error = glGetError();
    if (UNLIKELY(gl_error != GL_NO_ERROR)) {
        DLOG("Failed to get OpenGL attributes: 0x%04X", gl_error);
        return 0;
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

static int check_call(int32_t result, const char *(*strerror_func)(int32_t),
                      const char *expr, int line, const char *function)
{
    if (LIKELY(result == 0)) {
        return 1;
    } else {
#ifdef DEBUG
        do_DLOG(__FILE__, line, function, "%s failed: %d (%s)",
                expr, result, (*strerror_func)(result));
#else
        /* Avoid unused-parameter warnings. */
        (void) strerror_func;
        (void) expr;
        (void) line;
        (void) function;
#endif
        return 0;
    }
}

/*-----------------------------------------------------------------------*/

static const char *CGErrorString(CGError error)
{
    switch (error) {
      case kCGErrorSuccess:
        return "Success";
      case kCGErrorFailure:
        return "General failure";
      case kCGErrorIllegalArgument:
        return "Illegal argument";
      case kCGErrorInvalidConnection:
        return "Invalid window server connection";
      case kCGErrorInvalidContext:
        return "Invalid context identifier";
      case kCGErrorCannotComplete:
        return "Cannot complete operation";
      case kCGErrorNotImplemented:
        return "Not implemented";
      case kCGErrorRangeCheck:
        return "Value out of range";
      case kCGErrorTypeCheck:
        return "Invalid data type";
      case kCGErrorInvalidOperation:
        return "Invalid operation";
      case kCGErrorNoneAvailable:
        return "No resource available";
    };

    static char buf[40];
    strformat(buf, sizeof(buf), "Unknown CoreGraphics error %d", error);
    return buf;
}

/*************************************************************************/
/*************************************************************************/
