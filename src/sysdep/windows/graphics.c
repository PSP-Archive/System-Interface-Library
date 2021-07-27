/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/windows/graphics.c: Graphics and rendering functionality for
 * Windows.  This file contains the primary sys_graphics interface and
 * wrappers which select between the OpenGL and Direct3D implementations
 * of the individual graphics functions.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/framebuffer.h"
#include "src/graphics.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/random.h"
#include "src/shader.h"
#include "src/sysdep.h"
#include "src/sysdep/opengl/opengl.h"
#include "src/sysdep/windows/d3d.h"
#include "src/sysdep/windows/internal.h"
#include "src/texture.h"

#include "external/opengl-headers/GL/wglext.h"

/*************************************************************************/
/****************** Global data (only used for testing) ******************/
/*************************************************************************/

#ifdef SIL_INCLUDE_TESTS
uint8_t TEST_windows_force_direct3d = 0;
#endif

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Graphics capability structure returned to high-level code.  The display
 * device count and display mode list are filled in at initialization time. */
static SysGraphicsInfo graphics_info = {
    .has_windowed_mode = 1,
    .num_devices = 0,
    .num_modes = 0,
    .modes = NULL,
};

/*---------------------------- General data -----------------------------*/

/* Have we been initialized? */
static uint8_t initted;

/* Is Direct3D available? */
static uint8_t d3d_available;

/* Should OpenGL debugging be enabled? */
static uint8_t use_opengl_debug;

/* Should we show the mouse pointer? */
static uint8_t show_mouse_pointer;

/* Should the mouse pointer be grabbed (confined to the window's rectangle)
 * when a window is visible? */
static uint8_t grab_requested;

/* Is the mouse pointer currently grabbed?  (Again, this reflects
 * persistent system state so we initialize it statically.) */
static uint8_t mouse_grabbed = 0;

/* Flag indicating whether a quit request has been received from the system. */
static uint8_t quit_requested;

/* Should we force single-threaded windows? */
static uint8_t single_threaded;

/* Should we minimize a fullscreen window on focus loss? (1 = yes, 0 = no,
 * -1 = unspecified by client code) */
static int8_t minimize_fullscreen;

/* Window class for creating windows. */
static ATOM window_class;
static const char * const window_class_name = "SILWindowClass";

/* Window class used by choose_wgl_pixel_format() (see notes in that
 * function for why it's needed). */
static ATOM wgl_pixel_format_hack_class;
static const char * const wgl_pixel_format_hack_class_name = "SILCPFWindowClass";

/* Window style for non-fullscreen windows. */
static DWORD windowed_style = WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME;

/* Events used for synchronization with the window thread when creating a
 * window. */
static HANDLE create_window_event;
static HANDLE setpixelformat_event;  // For OpenGL only.

/* Data structure used to pass data to the window thread. */
typedef struct CreateWindowParams CreateWindowParams;
struct CreateWindowParams {
    RECT rect;          // Window rectangle for CreateWindow().
    uint8_t do_center;  // True if the window should be centered.
};

/* Magic token for WM_APP message used to close a window (see
 * window_thread_func()). */
#define SIL_WM_APP_CLOSE  0x434C4953  // 'SILC'

/* Magic token for WM_APP message used to flush the message queue (see
 * window_thread_func()). */
#define SIL_WM_APP_FLUSH  0x464C4953  // 'SILF'

/*-------------------- Display device and mode data ---------------------*/

/* Mapping from our device numbers to Windows device names and associated
 * data.  The length of the array is equal to the value of
 * graphics_info.num_devices. */
static struct {
    char name[32];
    int default_mode;  // Index into device_modes[].
} *devices;

/* Windows DEVMODE structures for each supported mode. */
static DEVMODE *device_modes;

/*------------------ Current display and window state -------------------*/

/* Current display device index. */
static int current_device;

/* Current display device mode, or -1 if the mode has not been changed
 * from the default.  This is statically initialized to avoid confusing
 * windows_reset_video_mode() if we error out during setup. */
static int current_mode = -1;

/* Video mode in use when window was iconified.  Used to restore the
 * desired mode when the window is brought back from icon state. */
static int saved_mode = -1;

/* Window handle for current window, or NULL if no window is open. */
static HWND current_window;

/* Thread for current window, or NULL if running single-threaded. */
static HANDLE window_thread;

/* Is the current window using Direct3D? */
static uint8_t window_is_d3d;

/* OpenGL context and associated DC for the current window, if
 * window_is_d3d is false.  (See sys_graphics_finish_frame() for why we
 * need to save the DC handle.) */
static HGLRC wgl_context;
static HDC wgl_context_dc;

/* Current window size and other rendering parameters. */
static int window_width, window_height;
static int window_depth_bits;
static int window_stencil_bits;
static int window_samples;

/* Window position in windowed mode, used to restore the position when
 * switching out of fullscreen mode. */
static int window_x, window_y;

/* Does the current window have the input focus? */
static uint8_t window_focused;

/* Is the current window in fullscreen mode? */
static uint8_t window_fullscreen;

/* Is the current window minimized? */
static uint8_t window_minimized;

/* Is a window resize operation pending?  (Backend resizes need to happen
 * between frames, so we can't execute them directly from the window
 * procedure.) */
static uint8_t window_resize_pending;

/* New size of the window for a pending resize. */
static int window_resize_new_width, window_resize_new_height;

/* Previous size of the window during a resize operation. */
static int window_resize_last_width, window_resize_last_height;

/* Override values for WM_GETMINMAXINFO, used when manually changing a
 * window's size. */
static int minmax_override_width, minmax_override_height;

/*--------------------------- Window settings ---------------------------*/

/* Should the window (in windowed mode) be centered when opened? */
static uint8_t center_window;

/* Is fullscreen mode selected (for the next mode change)? */
static uint8_t fullscreen;

/* Is multisampling enabled? */
static uint8_t multisample;

/* Is vertical sync enabled? */
static uint8_t vsync;

/* Display device to use for window creation. */
static int device_to_use;

/* Requested refresh rate. */
static float refresh_rate;

/* Requested depth and stencil sizes. */
static int depth_bits;
static int stencil_bits;

/* Requested number of samples for multisampling. */
static int multisample_samples;

/* Should we use Direct3D (true) or OpenGL (false) at the next mode change? */
static uint8_t use_direct3d;

/* Should the window be resizable? */
static uint8_t window_resizable;

/* Title to use with new windows. */
static char *window_title;

/* Size constraints for window resize operations. */
static int window_min_width, window_min_height;
static int window_max_width, window_max_height;
static int window_min_aspect_x, window_min_aspect_y;
static int window_max_aspect_x, window_max_aspect_y;

/* Requested OpenGL version (0 if not set). */
static int desired_opengl_major, desired_opengl_minor;

/*------------------------ WGL function pointers ------------------------*/

/* The non-extension functions could potentially be direct calls, but MSVC
 * doesn't like linking opengl32.lib with our (colliding) dyngl symbols,
 * so we look those up as well to avoid having to link with opengl32. */

static HMODULE opengl32_handle;

static HGLRC (WINAPI *p_wglCreateContext)(HDC dc);
static BOOL (WINAPI *p_wglDeleteContext)(HGLRC context);
static HGLRC (WINAPI *p_wglGetCurrentContext)(void);
static PROC (WINAPI *p_wglGetProcAddress)(const char *name);
static BOOL (WINAPI *p_wglMakeCurrent)(HDC dc, HGLRC context);
static PFNWGLSWAPINTERVALEXTPROC p_wglSwapIntervalEXT;

/* Also define wrappers for standard functions so thread.c can call them.
 * wglGetCurrentContext() is safe to call even without opengl32.dll loaded. */
HGLRC WINAPI wglCreateContext(HDC dc) {return (*p_wglCreateContext)(dc);}
BOOL WINAPI wglDeleteContext(HGLRC context) {return (*p_wglDeleteContext)(context);}
HGLRC WINAPI wglGetCurrentContext(void) {return wgl_context ? (*p_wglGetCurrentContext)() : NULL;}
PROC WINAPI wglGetProcAddress(const char *name) {return (*p_wglGetProcAddress)(name);}
BOOL WINAPI wglMakeCurrent(HDC dc, HGLRC context) {return (*p_wglMakeCurrent)(dc, context);}


/*--------------------- Local routine declarations ----------------------*/

/**
 * register_window_class:  Register a window class with the given base
 * name, window procedure, and icon.  A random number will be appended to
 * the base name to produce the final class name, to reduce the risk of
 * name collisions.
 *
 * [Parameters]
 *     name: Base name for window class.
 *     wndproc: Window procedure for the window class.
 *     icon: Icon for the window class, or NULL if none.
 * [Return value]
 *     Window class atom, or 0 on error.
 */
static ATOM register_window_class(const char *name, WNDPROC wndproc,
                                  HICON icon);

/**
 * window_thread_func:  Thread routine which manages a window.  Used to
 * avoid blocking the main thread on window operations such as resizes.
 *
 * [Parameters]
 *     params: Window creation parameters.
 * [Return value]
 *     0
 */
static DWORD CALLBACK window_thread_func(LPVOID params);

/**
 * create_window:  Create a new window with the given parameters.
 *
 * [Parameters]
 *     params: Window creation parameters.
 */
static void create_window(const CreateWindowParams *params);

/**
 * update_window:  Parse all pending messages for the current window.
 *
 * [Parameters]
 *     block: True to block until a message is received; false to return
 *         immediately if no messages are pending.
 * [Return value]
 *     0 if a message was received that should close the window; 1 if any
 *     other message was received; -1 if block was false and no messages
 *     were pending.
 */
static int update_window(int block);

/**
 * window_proc:  The "window procedure" associated with windows.
 *
 * [Parameters]
 *     hwnd: Window on which the message was received.
 *     uMsg: Message type.
 *     wParam, lParam: Message parameters.
 * [Return value]
 *     Message processing result (the meaning is message-dependent).
 */
static LRESULT CALLBACK window_proc(HWND hwnd, UINT uMsg,
                                    WPARAM wParam, LPARAM lParam);

/**
 * apply_window_resize_limits:  Modify the given width and height values
 * to obey any active window resizing constraints.
 *
 * The width and height should be the size of the client area, not the
 * window as a whole.
 *
 * [Parameters]
 *     width_ptr: Pointer to width value (modified on return).
 *     height_ptr: Pointer to height value (modified on return).
 *     wParam: WM_SIZING wParam argument indicating resize direction.
 */
static void apply_window_resize_limits(int *width_ptr, int *height_ptr,
                                       WPARAM wParam);

/**
 * resize_backend:  Resize the graphics backend resources following a
 * window resize operation, and set window_width and window_height to the
 * given size.
 *
 * [Parameters]
 *     width, height: New window size.
 */
static void resize_backend(int width, int height);

/**
 * add_video_mode:  Add a video mode to the mode list.
 *
 * [Parameters]
 *     device: Display device index.
 *     mode_info: DEVMODE structure describing the video mode.
 * [Return value]
 *     True on success, false on error (out of memory).
 */
static int add_video_mode(int device, const DEVMODE *mode_info);

/**
 * set_video_mode:  Switch to the given display device mode.
 *
 * [Parameters]
 *     mode: Mode to switch to (index into device_modes[]), or -1 to
 *         revert any active mode change.
 * [Return value]
 *     True on success, false on error.
 */
static int set_video_mode(int mode);

/**
 * should_minimize_fullscreen:  Return whether the current window should
 * be minimized on focus loss if in fullscreen mode.
 */
static int should_minimize_fullscreen(void);

/**
 * update_mouse_grab:  Enable or disable mouse grabbing via ClipCursor(),
 * depending on whether grabbing has been requested and the current state
 * of the window.
 */
static void update_mouse_grab(void);

/**
 * init_window_wgl:  Prepare a newly opened window for OpenGL rendering.
 *
 * [Return value]
 *     Error code (GRAPHICS_ERROR_SUCCESS on success).
 */
static GraphicsError init_window_wgl(void);

/**
 * set_wgl_pixel_format:  Set a pixel format appropriate to the current
 * OpenGL settings for the currently open window.
 *
 * [Return value]
 *     True on success, false on error.
 */
static int set_wgl_pixel_format(void);

/**
 * choose_wgl_pixel_format:  Call wglChoosePixelFormatARB() to choose a
 * pixel format for a new window.
 *
 * [Parameters]
 *     dc: Windows device context for the window.
 *     pfd: Pointer to a PIXELFORMATDESCRIPTOR for the basic pixel format.
 *     pixel_format_ret: Pointer to variable to receive the pixel format
 *         index on success.
 * [Return value]
 *     True on success; false on error or if wglChoosePixelFormatARB() is
 *     not available.
 */
static int choose_wgl_pixel_format(HDC dc, const PIXELFORMATDESCRIPTOR *pfd,
                                   int *pixel_format_ret);

/**
 * close_window:  Close the currently open window.
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
 * wgl_has_extension:  Return whether the given WGL extension is supported.
 * Requires a window to be open and a GL context to have been created.
 *
 * [Parameters]
 *     dc: GDI device context to use for checking.
 *     name: WGL extension name.
 * [Return value]
 *     True if the extension is supported, false if not.
 */
static int wgl_has_extension(HDC dc, const char *name);

/**
 * wglGetProcAddress_wrapper:  Wrapper for wglGetProcAddress() which falls
 * back to GetProcAddress() on failure (to handle OpenGL 1.0/1.1 functions).
 *
 * [Parameters]
 *     name: Name of function to look up.
 * [Return value]
 *     Function pointer, or NULL if the function is not found.
 */
static void *wglGetProcAddress_wrapper(const char *name);

/**
 * create_wgl_context:  Create a new GL context and set it as the current
 * context.
 *
 * [Parameters]
 *     dc: Device context with which to associate the GL context.
 * [Return value]
 *     Newly created GL context, or zero on error.
 */
static HGLRC create_wgl_context(HDC dc);

/**
 * create_gl_shader_compilation_context:  Create and make current a new
 * GL context for the current thread which can be used to compile shaders.
 *
 * [Return value]
 *     True on success or if the current thread already has a GL context,
 *     false on error.
 */
static int create_gl_shader_compilation_context(void);

#ifdef DEBUG
/**
 * describe_mode:  Return a human-readable description of the given
 * display mode.
 *
 * [Parameters]
 *     mode: Mode to describe.
 *     desc_buf: Buffer into which to store the description (should be
 *         at least 100 characters large).
 *     bufsize: Size of desc_buf, in bytes.
 */
static void describe_mode(int mode, char *desc_buf, int bufsize);
#endif

/**
 * wgl_pixel_format_hack_window_proc:  Dummy window procedure for the
 * wglChoosePixelFormatARB() hack.  We ought to be able to just use
 * DefWindowProc() itself as the window procedure, but that triggers a
 * compiler warning, so we play it safe.
 */
static LRESULT CALLBACK wgl_pixel_format_hack_window_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

/*************************************************************************/
/***************** Interface: Basic graphics operations ******************/
/*************************************************************************/

const SysGraphicsInfo *sys_graphics_init(void)
{
    PRECOND(!initted, return NULL);

    /* Load libraries for each supported backend. */
    d3d_available = d3d_open_library();

    opengl32_handle = LoadLibrary("opengl32.dll");
    if (opengl32_handle) {
        p_wglCreateContext = (void *)GetProcAddress(
            opengl32_handle, "wglCreateContext");
        p_wglDeleteContext = (void *)GetProcAddress(
            opengl32_handle, "wglDeleteContext");
        p_wglGetCurrentContext = (void *)GetProcAddress(
            opengl32_handle, "wglGetCurrentContext");
        p_wglGetProcAddress = (void *)GetProcAddress(
            opengl32_handle, "wglGetProcAddress");
        p_wglMakeCurrent = (void *)GetProcAddress(
            opengl32_handle, "wglMakeCurrent");
        if (p_wglCreateContext
         && p_wglDeleteContext
         && p_wglGetCurrentContext
         && p_wglGetProcAddress
         && p_wglMakeCurrent) {
            DLOG("opengl32.dll successfully loaded");
        } else {
            DLOG("Found invalid opengl32.dll (missing basic functions)");
            FreeLibrary(opengl32_handle);
            opengl32_handle = NULL;
        }
    } else {
        DLOG("Failed to load opengl32.dll");
    }

    /* Look up connected display devices.  We reorder devices so the
     * primary devices is always device 0. */
    DLOG("Enumerating display devices");
    graphics_info.num_devices = 1;
    devices = NULL;
    int got_primary = 0;
    for (int device = 0; ; device++) {
        DISPLAY_DEVICE device_info = {.cb = sizeof(device_info)};
        if (!EnumDisplayDevices(NULL, device, &device_info, 0)) {
            break;
        }
        /* MSDN documents bit 0 as DISPLAY_DEVICE_ACTIVE, but MinGW has
         * DISPLAY_DEVICE_ATTACHED_TO_DESKTOP instead.  Sample code in
         * other MSDN documents also uses ATTACHED_TO_DESKTOP, so maybe
         * ACTIVE is just an older name? */
        if (!(device_info.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)) {
            continue;
        }
        int index;
        if (device_info.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) {
            ASSERT(!got_primary);
            got_primary = 1;
            index = 0;
        } else {
            index = graphics_info.num_devices++;
        }
        void *new_devices =
            mem_realloc(devices, graphics_info.num_devices * sizeof(*devices),
                        0);
        if (UNLIKELY(!new_devices)) {
            DLOG("Out of memory adding display device %d", index);
            goto error_free_devices;
        }
        devices = new_devices;
        memcpy(devices[index].name, device_info.DeviceName,
               sizeof(devices[index].name));
    }
    if (UNLIKELY(!got_primary)) {
        DLOG("System reports no primary device (OS bug?)");
        graphics_info.num_devices--;
        if (UNLIKELY(graphics_info.num_devices == 0)) {
            DLOG("No display devices found!");
            goto error_free_devices;
        }
        memmove(&devices[0], &devices[1],
                sizeof(*devices) * graphics_info.num_devices);
    }
    DLOG("%d devices found", graphics_info.num_devices);

    /* Look up display modes for each device. */
    DLOG("Enumerating display modes");
    graphics_info.num_modes = 0;
    graphics_info.modes = NULL;
    device_modes = NULL;
    for (int device = 0; device < graphics_info.num_devices; device++) {
        const DWORD required_fields = DM_PELSWIDTH
                                    | DM_PELSHEIGHT
                                    | DM_BITSPERPEL;

        DEVMODE default_mode = {.dmSize = sizeof(default_mode)};
        if (UNLIKELY(!EnumDisplaySettings(devices[device].name,
                                          ENUM_CURRENT_SETTINGS,
                                          &default_mode))) {
            /* Try getting the mode from the registry instead. */
            default_mode.dmSize = sizeof(default_mode);  // Just in case.
            if (UNLIKELY(!EnumDisplaySettings(devices[device].name,
                                              ENUM_REGISTRY_SETTINGS,
                                              &default_mode))) {
                DLOG("Failed to get default mode for device \"%s\" (OS bug?)",
                     devices[device].name);
                mem_clear(&default_mode, sizeof(default_mode));
            }
        }
        if (UNLIKELY((default_mode.dmFields
                      & required_fields) != required_fields)) {
            DLOG("Default mode for device \"%s\" is missing required fields:"
                 "%s%s%s (OS bug?)", devices[device].name,
                 (default_mode.dmFields & DM_PELSWIDTH) ? "" : " width",
                 (default_mode.dmFields & DM_PELSHEIGHT) ? "" : " height",
                 (default_mode.dmFields & DM_BITSPERPEL) ? "" : " bpp");
            mem_clear(&default_mode, sizeof(default_mode));
        }
        /* The documentation says that fields whose dmFields bits are not
         * set should always be zero, but we clear them explicitly just to
         * be sure. */
        if (!(default_mode.dmFields & DM_DISPLAYFREQUENCY)) {
            default_mode.dmDisplayFrequency = 0;
        }
        if (!(default_mode.dmFields & DM_POSITION)) {
            default_mode.dmPosition.x = 0;
            default_mode.dmPosition.y = 0;
        }
        DLOG("Device %d (%s) default mode: %dx%dx%d @ %d Hz, position %d,%d",
             device, devices[device].name,
             default_mode.dmPelsWidth, default_mode.dmPelsHeight,
             default_mode.dmBitsPerPel, default_mode.dmDisplayFrequency,
             default_mode.dmPosition.x, default_mode.dmPosition.y);

        devices[device].default_mode = -1;
        const int first_mode = graphics_info.num_modes;
        int got_any_mode = 0;

        for (int mode = 0; ; mode++) {
            DEVMODE mode_info = {.dmSize = sizeof(mode_info)};
            if (!EnumDisplaySettings(devices[device].name, mode, &mode_info)) {
                break;
            }
            if (UNLIKELY((mode_info.dmFields
                          & required_fields) != required_fields)) {
                DLOG("Mode %d on display %s is missing required fields"
                     " (dmFlags = 0x%X)", mode, devices[device].name,
                     (int)mode_info.dmFields);
                continue;
            }
            if (!(mode_info.dmFields & DM_DISPLAYFREQUENCY)) {
                mode_info.dmDisplayFrequency = 0;
            }
            if (!(mode_info.dmFields & DM_POSITION)) {
                if (default_mode.dmFields & DM_POSITION) {
                    mode_info.dmFields |= DM_POSITION;
                    mode_info.dmPosition = default_mode.dmPosition;
                } else {
                    mode_info.dmPosition.x = 0;
                    mode_info.dmPosition.y = 0;
                }
            }

            /* Ignore modes with less than 32 bits per pixel. */
            if (mode_info.dmBitsPerPel < 32) {
                continue;
            }

            /* Ideally, we should check that we could actually change to
             * the mode before we record it in the list.  However,
             * repeatedly calling ChangeDisplaySettingsEx() with CDS_TEST
             * seems to freeze the system for short periods of time in
             * some configurations, so we do without the check and hope
             * that drivers only report modes which are in fact valid.
             * Various reports suggest that the presence of a monitor
             * connected via DisplayPort triggers the bug (see also
             * https://forum.unity.com/threads/298349/). */
            if (0) {
                const LONG change_test = ChangeDisplaySettingsEx(
                    devices[device].name, &mode_info, NULL,
                    CDS_FULLSCREEN | CDS_TEST, NULL);
                if (UNLIKELY(change_test != DISP_CHANGE_SUCCESSFUL)) {
                    DLOG("Ignoring mode %d (%dx%dx%d @ %dHz) on display %s"
                         " because ChangeDisplaySettingsEx(CDS_TEST) failed"
                         " with code %ld", mode, (int)mode_info.dmPelsWidth,
                         (int)mode_info.dmPelsHeight, (int)mode_info.dmBitsPerPel,
                         (int)mode_info.dmDisplayFrequency, devices[device].name,
                         change_test);
                    continue;
                }
            }  // if (0)

            /* This mode looks good, so add it to the arrays. */
            got_any_mode = 1;
            if (UNLIKELY(!add_video_mode(device, &mode_info))) {
                goto error_free_sizes;
            }
            if (devices[device].default_mode < 0
             && mode_info.dmDisplayFrequency == default_mode.dmDisplayFrequency
             && mode_info.dmPelsWidth == default_mode.dmPelsWidth
             && mode_info.dmPelsHeight == default_mode.dmPelsHeight
             && mode_info.dmBitsPerPel == default_mode.dmBitsPerPel) {
                devices[device].default_mode = graphics_info.num_modes - 1;
            }
        }  // for (int mode = 0; ; mode++)

        if (devices[device].default_mode < 0
         && default_mode.dmBitsPerPel >= 32) {
            DLOG("Failed to find default mode for device %s, adding to list:"
                 " %dx%dx%d @ %dHz", devices[device].name,
                 (int)default_mode.dmPelsWidth,
                 (int)default_mode.dmPelsHeight,
                 (int)default_mode.dmBitsPerPel,
                 (int)default_mode.dmDisplayFrequency);
            got_any_mode = 1;
            if (UNLIKELY(!add_video_mode(device, &default_mode))) {
                goto error_free_sizes;
            }
            devices[device].default_mode = graphics_info.num_modes - 1;
        }
        if (UNLIKELY(!got_any_mode)) {
            DLOG("Failed to get any modes for device %s, removing from list",
                 devices[device].name);
            graphics_info.num_devices--;
            if (UNLIKELY(graphics_info.num_devices == 0)) {
                DLOG("No devices left!");
                goto error_free_devices;
            }
            memmove(&devices[device], &devices[device+1],
                    sizeof(*devices) * (graphics_info.num_devices - device));
            device--;
        } else if (UNLIKELY(devices[device].default_mode < 0)) {
            DLOG("Failed to find default mode for device %s, using first"
                 " mode in list: %dx%dx%d @ %dHz", devices[device].name,
                 (int)device_modes[first_mode].dmPelsWidth,
                 (int)device_modes[first_mode].dmPelsHeight,
                 (int)device_modes[first_mode].dmBitsPerPel,
                 (int)device_modes[first_mode].dmDisplayFrequency);
            devices[device].default_mode = first_mode;
        }
    }
    DLOG("%d modes found", graphics_info.num_modes);

    /* Set up a window class to use when creating windows. */
    HICON icon = NULL;
#ifdef HAVE_DEFAULT_ICON
    /* Pass LR_SHARED so the system automatically cleans up after us. */
    icon = LoadImage(GetModuleHandle(NULL), "DefaultIcon",
                     IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
    if (!icon) {
        DLOG("Failed to load default icon: %s",
             windows_strerror(GetLastError()));
    }
#endif
    window_class = register_window_class(window_class_name, window_proc, icon);
    if (UNLIKELY(!window_class)) {
        DLOG("Failed to register window class");
        goto error_free_sizes;
    }

    /* Set up a second window class for the wglChoosePixelFormatARB() hack. */
    wgl_pixel_format_hack_class = register_window_class(
        wgl_pixel_format_hack_class_name, wgl_pixel_format_hack_window_proc,
        NULL);
    if (UNLIKELY(!wgl_pixel_format_hack_class)) {
        DLOG("Failed to register dummy window class");
        goto error_unregister_window_class;
    }

    /* Create event objects for synchronization when creating windows. */
    create_window_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!create_window_event) {
        DLOG("Failed to create create-window event object: %s",
             windows_strerror(GetLastError()));
        goto error_unregister_window_class_2;
    }
    setpixelformat_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!setpixelformat_event) {
        DLOG("Failed to create SetPixelFormat event object: %s",
             windows_strerror(GetLastError()));
        goto error_free_create_window_event;
    }

    /* Initialize other internal state and return. */
    center_window = 0;
    current_device = 0;
    current_mode = -1;
    current_window = NULL;
    depth_bits = 16;
    desired_opengl_major = 0;
    desired_opengl_minor = 0;
    device_to_use = 0;
    fullscreen = 0;
    grab_requested = 0;
    minimize_fullscreen = -1;
    minmax_override_width = 0;
    minmax_override_height = 0;
    multisample = 0;
    multisample_samples = 1;
    quit_requested = 0;
    refresh_rate = 0;
    show_mouse_pointer = 0;
    single_threaded = 0;
    stencil_bits = 0;
    use_direct3d = 0;
    use_opengl_debug = 0;
    vsync = 1;
    wgl_context = NULL;
    wgl_context_dc = NULL;
    window_focused = 0;
    window_max_aspect_x = 0;
    window_max_aspect_y = 0;
    window_max_height = 0;
    window_max_width = 0;
    window_min_aspect_x = 0;
    window_min_aspect_y = 0;
    window_min_height = 0;
    window_min_width = 0;
    window_resizable = 0;
    window_title = NULL;

    initted = 1;
    return &graphics_info;

  error_free_create_window_event:
    CloseHandle(create_window_event);
    create_window_event = NULL;
  error_unregister_window_class_2:
    UnregisterClass((void *)(uintptr_t)wgl_pixel_format_hack_class,
                    GetModuleHandle(NULL));
    wgl_pixel_format_hack_class = 0;
  error_unregister_window_class:
    UnregisterClass((void *)(uintptr_t)window_class, GetModuleHandle(NULL));
    window_class = 0;
  error_free_sizes:
    mem_free((void *)graphics_info.modes);
    graphics_info.modes = NULL;
    graphics_info.num_modes = 0;
    mem_free(device_modes);
    device_modes = NULL;
  error_free_devices:
    mem_free(devices);
    devices = NULL;
    graphics_info.num_devices = 0;
    if (opengl32_handle) {
        FreeLibrary(opengl32_handle);
        opengl32_handle = NULL;
    }
    d3d_close_library();
    return 0;
}

/*-----------------------------------------------------------------------*/

void sys_graphics_cleanup(void)
{
    PRECOND(initted, return);

    /* Reset the video mode before closing the window so that (if the
     * mode was changed) the monitor is already switching modes when the
     * window disappears. */
    set_video_mode(-1);

    if (current_window) {
        close_window();
    }
    mem_free(window_title);
    window_title = NULL;

    CloseHandle(setpixelformat_event);
    setpixelformat_event = NULL;
    CloseHandle(create_window_event);
    create_window_event = NULL;

    UnregisterClass((void *)(uintptr_t)wgl_pixel_format_hack_class,
                    GetModuleHandle(NULL));
    wgl_pixel_format_hack_class = 0;
    UnregisterClass((void *)(uintptr_t)window_class, GetModuleHandle(NULL));
    window_class = 0;

    mem_free((void *)graphics_info.modes);
    graphics_info.modes = NULL;
    graphics_info.num_modes = 0;
    mem_free(device_modes);
    device_modes = NULL;
    mem_free(devices);
    devices = NULL;
    graphics_info.num_devices = 0;

    if (opengl32_handle) {
        FreeLibrary(opengl32_handle);
        opengl32_handle = NULL;
    }
    d3d_close_library();

    initted = 0;
}

/*-----------------------------------------------------------------------*/

int sys_graphics_device_width(void)
{
    const int device = current_window ? current_device : device_to_use;
    const int mode = devices[device].default_mode;
    return graphics_info.modes[mode].width;
}

/*-----------------------------------------------------------------------*/

int sys_graphics_device_height(void)
{
    const int device = current_window ? current_device : device_to_use;
    const int mode = devices[device].default_mode;
    return graphics_info.modes[mode].height;
}

/*-----------------------------------------------------------------------*/

int sys_graphics_set_display_attr(const char *name, va_list args)
{
    if (strcmp(name, "backend_name") == 0) {
        const char *value = va_arg(args, const char *);
        if (strcmp(value, "direct3d") == 0) {
            use_direct3d = 1;
        } else if (strcmp(value, "opengl") == 0) {
            use_direct3d = 0;
        } else {
            DLOG("Invalid value for attribute %s: %s", name, value);
            return 0;
        }
        return 1;
    }

    if (strcmp(name, "center_window") == 0) {
        const int value = va_arg(args, int);
        center_window = (value != 0);
        return 1;
    }

    if (strcmp(name, "d3d_shader_debug_info") == 0) {
        const int value = va_arg(args, int);
        d3d_shader_set_debug_info(value != 0);
        return 1;
    }

    if (strcmp(name, "d3d_shader_opt_level") == 0) {
        const int value = va_arg(args, int);
        if (value < 0 || value > 3) {
            DLOG("Invalid value for attribute %s: %d", name, value);
            return 0;
        }
        d3d_shader_set_opt_level(value);
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
        if (value < 0 || value >= graphics_info.num_devices) {
            DLOG("Invalid value for attribute %s: %d", name, value);
            return 0;
        }
        device_to_use = value;
        return 1;
    }

    if (strcmp(name, "fullscreen_minimize_on_focus_loss") == 0) {
        const int value = va_arg(args, int);
        minimize_fullscreen = (value != 0);
        return 1;
    }

    if (strcmp(name, "multisample") == 0) {
        const int value = va_arg(args, int);
        return value == 1;  // FIXME: support multisampling on D3D
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
        /* We don't (can't) look up GL functions until after opening a
         * window, so it's not necessarily safe to call opengl_enable_debug()
         * here. */
        if (current_window && !window_is_d3d) {
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
        refresh_rate = value;
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
        if (current_window && !window_is_d3d && p_wglSwapIntervalEXT) {
            (*p_wglSwapIntervalEXT)(vsync);
        }
        return 1;
    }

    if (strcmp(name, "window") == 0) {
        fullscreen = (va_arg(args, int) == 0);
        return 1;
    }

    if (strcmp(name, "window_resizable") == 0) {
        window_resizable = (va_arg(args, int) != 0);
        DWORD new_style = WS_OVERLAPPEDWINDOW;
        if (!window_resizable) {
            new_style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
        }
        if (current_window && !window_fullscreen
         && new_style != windowed_style) {
            SetLastError(0);
            SetWindowLong(current_window, GWL_STYLE, new_style);
            if (UNLIKELY(GetLastError())) {
                DLOG("Failed to change window style: %s",
                     windows_strerror(GetLastError()));
            }
            /* Microsoft's documentation says a SetWindowPos() is required
             * after changing window style flags. */
            if (UNLIKELY(!SetWindowPos(
                             current_window, HWND_NOTOPMOST, 0, 0, 0, 0,
                             SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER
                                 | SWP_FRAMECHANGED | SWP_NOACTIVATE))) {
                DLOG("Failed to update window after style change: %s",
                     windows_strerror(GetLastError()));
            }
        }
        windowed_style = new_style;
        return 1;
    }

    if (strcmp(name, "window_thread") == 0) {
        const int value = va_arg(args, int);
        single_threaded = !value;
        return 1;
    }

    return 0;
}

/*-----------------------------------------------------------------------*/

GraphicsError sys_graphics_set_display_mode(int width, int height)
{
    GraphicsError error = GRAPHICS_ERROR_UNKNOWN;

    int create_direct3d = use_direct3d;
#ifdef SIL_INCLUDE_TESTS
    create_direct3d |= TEST_windows_force_direct3d;
#endif

    /* Check ahead of time that the requested backend is available. */
    if (create_direct3d) {
        if (!d3d_available) {
            DLOG("Direct3D requested but d3d11.dll not available");
            return GRAPHICS_ERROR_BACKEND_NOT_FOUND;
        }
    } else {
        if (!opengl32_handle) {
            DLOG("OpenGL requested but opengl32.dll not available");
            return GRAPHICS_ERROR_BACKEND_NOT_FOUND;
        }
    }

    /* Look up the device mode corresponding to the requested device and
     * display size. */
    int fullscreen_mode = -1;
    if (fullscreen) {
        const int device_defmode = devices[device_to_use].default_mode;
        const int device_width = device_modes[device_defmode].dmPelsWidth;
        const int device_height = device_modes[device_defmode].dmPelsHeight;
        if (refresh_rate==0 && width==device_width && height==device_height) {
            fullscreen_mode = -1;
        } else {
            for (int i = 0; i < graphics_info.num_modes; i++) {
                if (graphics_info.modes[i].width == width
                 && graphics_info.modes[i].height == height
                 && graphics_info.modes[i].device == device_to_use) {
                    if (fullscreen_mode < 0) {
                        fullscreen_mode = i;
                    } else if (refresh_rate > 0) {
                        if (fabsf(graphics_info.modes[i].refresh - refresh_rate)
                            < fabsf(graphics_info.modes[fullscreen_mode].refresh
                                        - refresh_rate))
                        {
                            fullscreen_mode = i;
                        }
                    } else {  // refresh_rate == 0
                        if (graphics_info.modes[i].refresh
                            > graphics_info.modes[fullscreen_mode].refresh)
                        {
                            fullscreen_mode = i;
                        }
                    }
                }
            }
            if (fullscreen_mode < 0) {
                DLOG("No video mode matching %dx%d on device %d",
                     width, height, device_to_use);
                error = GRAPHICS_ERROR_MODE_NOT_SUPPORTED;
                goto error_return;
            }
        }
    }

    /* Look up the position of the selected device. */
    const int device_x =
        device_modes[devices[device_to_use].default_mode].dmPosition.x;
    const int device_y =
        device_modes[devices[device_to_use].default_mode].dmPosition.y;

    /* See if we can toggle fullscreen or change size without closing the
     * window. */
    if (current_window
     && device_to_use == current_device
     && (create_direct3d != 0) == window_is_d3d
     && window_depth_bits >= depth_bits
     && (!multisample || window_samples >= multisample_samples)
     && window_stencil_bits >= stencil_bits) {
        if (fullscreen == window_fullscreen
         && width == window_width
         && height == window_height) {
            /* No change at all! */
            return GRAPHICS_ERROR_SUCCESS;
        } else if (fullscreen || window_fullscreen) {
            /* Handle changing between fullscreen and windowed mode or
             * between two fullscreen modes. */
            if (!window_fullscreen) {
                /* Save the current window position for restoring from
                 * fullscreen mode. */
                RECT rect;
                if (UNLIKELY(!GetWindowRect(current_window, &rect))) {
                    DLOG("Failed to save window position: %s",
                         windows_strerror(GetLastError()));
                    window_x = window_y = 0;
                } else {
                    window_x = rect.left - device_x;
                    window_y = rect.top - device_y;
                }
            }
            /* Switch modes if necessary. */
            if (fullscreen) {
                set_video_mode(fullscreen_mode);
            } else {
                set_video_mode(-1);
            }
            /* Toggle the window borders on or off as appropriate.
             * (Whose clever idea was it to have a public API which is
             * essentially just peek/poke into a private structure?
             * Good grief...) */
            DWORD style = GetWindowLong(current_window, GWL_STYLE);
            style &= ~(WS_OVERLAPPEDWINDOW | WS_POPUP);
            style |= fullscreen ? WS_POPUP : windowed_style;
            SetLastError(0);
            SetWindowLong(current_window, GWL_STYLE, style);
            if (UNLIKELY(GetLastError())) {
                DLOG("Failed to change window style: %s",
                     windows_strerror(GetLastError()));
            }
            /* Move and resize the window as appropriate. */
            int x, y, w, h;
            if (fullscreen) {
                x = device_x;
                y = device_y;
                w = width;
                h = height;
            } else {
                RECT rect = {.left = 0, .top = 0,
                             .right = width, .bottom = height};
                if (UNLIKELY(!AdjustWindowRectEx(&rect, style, FALSE, 0))) {
                    DLOG("AdjustWindowRectEx() failed for fullscreen"
                         " toggle: %s", windows_strerror(GetLastError()));
                }
                x = window_x + device_x;
                y = window_y + device_y;
                w = rect.right - rect.left;
                h = rect.bottom - rect.top;
            }
            minmax_override_width = width;
            minmax_override_height = height;
            const int result = SetWindowPos(
                current_window, fullscreen ? HWND_TOPMOST : HWND_NOTOPMOST,
                x, y, w, h, SWP_NOCOPYBITS | SWP_NOACTIVATE | SWP_FRAMECHANGED);
            minmax_override_width = 0;
            minmax_override_height = 0;
            if (LIKELY(result)) {
                window_width = width;
                window_height = height;
                window_fullscreen = fullscreen;
                /* Backend resizing will occur when WM_SIZE is received. */
                return GRAPHICS_ERROR_SUCCESS;
            } else {
                DLOG("SetWindowPos() failed for fullscreen toggle: %s",
                     windows_strerror(GetLastError()));
            }
        } else {
            /* We're resizing the window in windowed mode. */
            const DWORD style = GetWindowLong(current_window, GWL_STYLE);
            RECT rect = {.left = 0, .top = 0, .right = width, .bottom = height};
            if (UNLIKELY(!AdjustWindowRectEx(&rect, style, FALSE, 0))) {
                DLOG("AdjustWindowRectEx() failed for window resize: %s",
                     windows_strerror(GetLastError()));
            }
            minmax_override_width = width;
            minmax_override_height = height;
            const int result = SetWindowPos(
                current_window, HWND_NOTOPMOST, 0, 0,
                rect.right - rect.left, rect.bottom - rect.top,
                SWP_NOMOVE | SWP_NOACTIVATE);
            minmax_override_width = 0;
            minmax_override_height = 0;
            if (result) {
                window_width = width;
                window_height = height;
                return GRAPHICS_ERROR_SUCCESS;
            } else {
                DLOG("SetWindowPos() failed for window resize: %s",
                     windows_strerror(GetLastError()));
            }
        }
    }

    /* We can't reuse the existing window (if any), so close it. */
    const int window_was_open = (current_window != NULL);
    if (window_was_open) {
        close_window();
    }

    /* If we're changing display devices, restore the old device's mode. */
    if (current_mode != -1
     && graphics_info.modes[current_mode].device != device_to_use) {
        set_video_mode(-1);
    }

    /* Determine the initial window position and total size. */
    RECT new_rect = {.left = device_x,
                     .top = device_y,
                     .right = device_x + width,
                     .bottom = device_y + height};
    /* Windows doesn't allow us to say "default position on non-default
     * monitor", so force centering if the window is supposed to go to an
     * alternate monitor. */
    const int do_center = center_window || device_to_use != 0;
    if (!fullscreen) {
        if (do_center) {
            const int x_offset = (graphics_device_width() - width) / 2;
            const int y_offset = (graphics_device_height() - height) / 2;
            new_rect.left += x_offset;
            new_rect.top += y_offset;
            new_rect.right += x_offset;
            new_rect.bottom += y_offset;
        }
        AdjustWindowRectEx(&new_rect, windowed_style, FALSE, 0);
    }

    /* Change video modes if creating a fullscreen window.  Note that
     * Windows ignores the initial window mode (normal/minimized/maximized)
     * for WS_POPUP windows, so we change the video mode regardless of what
     * was specified in the STARTUPINFO structure. */
    if (fullscreen && !set_video_mode(fullscreen_mode)) {
        error = GRAPHICS_ERROR_MODE_NOT_SUPPORTED;
        goto error_return;
    }

    /* Actually create the window and associated GL/D3D context. */
    window_width = width;
    window_height = height;
    window_depth_bits = depth_bits;
    window_stencil_bits = stencil_bits;
    window_samples = multisample_samples;
    window_focused = 1;
    window_fullscreen = (fullscreen != 0);
    window_minimized = 0;
    window_resize_pending = 0;
    window_is_d3d = create_direct3d;
    CreateWindowParams cwp = {.rect = new_rect, .do_center = do_center};
    if (single_threaded) {
        window_thread = NULL;
        create_window(&cwp);
    } else {
        window_thread = CreateThread(NULL, 0, window_thread_func, &cwp, 0, NULL);
        if (UNLIKELY(!window_thread)) {
            DLOG("Failed to create window thread: %s",
                 windows_strerror(GetLastError()));
            error = GRAPHICS_ERROR_UNKNOWN;
            goto error_return;
        }
        WaitForSingleObject(create_window_event, INFINITE);
    }
    if (UNLIKELY(!current_window)) {
        if (window_thread) {
            CloseHandle(window_thread);
            window_thread = NULL;
        }
        error = GRAPHICS_ERROR_MODE_NOT_SUPPORTED;
        goto error_return;
    }

    if (create_direct3d) {
        if (!d3d_create_context(current_window, width, height, depth_bits,
                                stencil_bits, multisample_samples)) {
            error = GRAPHICS_ERROR_MODE_NOT_SUPPORTED;
            goto error_destroy_window;
        }
    } else {
        error = init_window_wgl();
        if (window_thread) {
            SetEvent(setpixelformat_event);
        }
        if (error != GRAPHICS_ERROR_SUCCESS) {
            goto error_destroy_window;
        }
    }

    /* Hide the pointer if requested, and return. */
    windows_show_mouse_pointer(-1);
    return window_was_open ? GRAPHICS_ERROR_STATE_LOST
                           : GRAPHICS_ERROR_SUCCESS;

  error_destroy_window:
    if (window_thread) {
        PostMessage(current_window, WM_APP, SIL_WM_APP_CLOSE, 0);
        WaitForSingleObject(window_thread, INFINITE);
        CloseHandle(window_thread);
        window_thread = NULL;
        ASSERT(!current_window);
    } else {
        DestroyWindow(current_window);
        current_window = NULL;
    }
  error_return:
    return error;
}

/*-----------------------------------------------------------------------*/

int sys_graphics_display_is_window(void)
{
    return current_window && !window_fullscreen;
}

/*-----------------------------------------------------------------------*/

void sys_graphics_set_window_title(const char *title)
{
    mem_free(window_title);
    window_title = mem_strdup(title, 0);
    if (UNLIKELY(!window_title)) {
        DLOG("No memory to copy window title");
    }
    if (current_window) {
        SetWindowText(current_window, title);
    }
}

/*-----------------------------------------------------------------------*/

void sys_graphics_set_window_icon(SysTexture *texture)
{
    const int width = sys_texture_width(texture);
    const int height = sys_texture_height(texture);
    const int large_icon = (width > 16 || height > 16);

    uint8_t *pixels = sys_texture_lock(texture, SYS_TEXTURE_LOCK_NORMAL,
                                       0, 0, width, height);
    if (!pixels) {
        DLOG("Failed to lock texture for icon");
        return;
    }

    /* Windows wants the image in BMP format. */
    const int stride = width * 4;
    const int bmp_size = sizeof(BITMAPINFOHEADER) + height * stride;
    uint8_t *bmp = mem_alloc(bmp_size, 0, MEM_ALLOC_TEMP);
    if (UNLIKELY(!bmp)) {
        DLOG("No memory for temporary copy of icon (%d bytes)", bmp_size);
        sys_texture_unlock(texture, 0);
        return;
    }
    BITMAPINFOHEADER *bmp_header = (BITMAPINFOHEADER *)bmp;
    mem_clear(bmp_header, sizeof(*bmp_header));
    bmp_header->biSize = sizeof(*bmp_header);
    bmp_header->biWidth = width;
    /* Windows icon bitmaps are double height, with the second half of
     * the buffer (the upper half of the bitmap) containing an AND mask
     * for use with monochrome displays.  Those things don't exist
     * anymore, but we still need to double the height as recorded in
     * the bitmap header or Windows will take the bottom half of our icon
     * and stretch it vertically. */
    bmp_header->biHeight = height * 2;
    bmp_header->biPlanes = 1;
    bmp_header->biBitCount = 32;
    bmp_header->biCompression = BI_RGB;
    bmp_header->biSizeImage = height * stride;
    /* Vertically flip the image and swap RGB order to match BMP format.
     * (Windows XP, at least, doesn't accept bitmaps in top-down order.) */
    for (int y = 0; y < height; y++) {
        const uint8_t *src = pixels + ((height-1)-y) * stride;
        uint8_t *dest = bmp + sizeof(BITMAPINFOHEADER) + y * stride;
        for (int x = 0; x < width; x++) {
            dest[x*4+0] = src[x*4+2];
            dest[x*4+1] = src[x*4+1];
            dest[x*4+2] = src[x*4+0];
            dest[x*4+3] = src[x*4+3];
        }
    }
    sys_texture_unlock(texture, 0);

    /* 0x00030000 is a magic value defined in the CreateIconFromResourceEx()
     * documentation. */
    HANDLE icon = CreateIconFromResourceEx(
        bmp, bmp_size, TRUE, 0x00030000, 0, 0, LR_SHARED);
    mem_free(bmp);
    if (UNLIKELY(!icon)) {
        DLOG("Failed to create icon resource: %s",
             windows_strerror(GetLastError()));
        return;
    }

    SendMessage(current_window, WM_SETICON, ICON_SMALL, (LPARAM)icon);
    if (large_icon) {
        SendMessage(current_window, WM_SETICON, ICON_BIG, (LPARAM)icon);
    }
}

/*-----------------------------------------------------------------------*/

void sys_graphics_set_window_resize_limits(
    int min_width, int min_height, int max_width, int max_height,
    int min_aspect_x, int min_aspect_y, int max_aspect_x, int max_aspect_y)
{
    window_min_width = min_width;
    window_min_height = min_height;
    window_max_width = max_width;
    window_max_height = max_height;
    window_min_aspect_x = min_aspect_x;
    window_min_aspect_y = min_aspect_y;
    window_max_aspect_x = max_aspect_x;
    window_max_aspect_y = max_aspect_y;
}

/*-----------------------------------------------------------------------*/

void sys_graphics_show_mouse_pointer(int on)
{
    show_mouse_pointer = (on != 0);
    windows_show_mouse_pointer(-1);
}

/*-----------------------------------------------------------------------*/

int sys_graphics_get_mouse_pointer_state(void)
{
    return show_mouse_pointer;
}

/*-----------------------------------------------------------------------*/

void sys_graphics_get_frame_period(int *numerator_ret, int *denominator_ret)
{
    int active_mode;
    if (current_mode >= 0) {
        active_mode = current_mode;
    } else {
        active_mode = devices[current_device].default_mode;
    }
    if (device_modes[active_mode].dmDisplayFrequency) {
        *numerator_ret = 1;
        *denominator_ret = device_modes[active_mode].dmDisplayFrequency;
    } else {
        *numerator_ret = 0;
        *denominator_ret = 1;
    }
}

/*-----------------------------------------------------------------------*/

int sys_graphics_has_focus(void)
{
    return window_focused;
}

/*-----------------------------------------------------------------------*/

void sys_graphics_start_frame(int *width_ret, int *height_ret)
{
    if (window_resize_pending) {
        resize_backend(window_resize_new_width, window_resize_new_height);
        window_resize_pending = 0;
    }

    *width_ret = window_width;
    *height_ret = window_height;
    if (window_is_d3d) {
        d3d_start_frame();
    } else {
        opengl_start_frame();
    }
}

/*-----------------------------------------------------------------------*/

void sys_graphics_finish_frame(void)
{
    if (window_is_d3d) {
        d3d_finish_frame();
    } else {
        /*
         * SwapBuffers() wants an HDC for the window to swap.  The trivial
         * way to get one would be to call GetDC(current_window) and then
         * release it after the swap, but that can confuse third-party
         * software which hooks SwapBuffers() and expects the handle value
         * to be identical on every call (such as the video capture
         * software OBS Studio).  Cases have even been observed in
         * which the per-frame GetDC()/ReleaseDC() pattern triggers
         * graphical corruption, caused by a third-party hook blindly
         * reusing a handle which we freed in a previous frame.  So we
         * need to save and reuse the handle passed to wglMakeCurrent()
         * rather than simply calling GetDC().
         */
        SwapBuffers(wgl_context_dc);
    }
}

/*-----------------------------------------------------------------------*/

void sys_graphics_sync(int flush)
{
    if (window_is_d3d) {
        d3d_sync();
    } else {
        opengl_sync();
        if (flush) {
            opengl_free_dead_resources(1);
        }
    }
}

/*-----------------------------------------------------------------------*/

/* Defined here instead of in input.c since we have to handle WM_QUIT
 * messages here. */
int sys_input_is_quit_requested(void)
{
    return quit_requested;
}

/*************************************************************************/
/********** sysdep wrappers for OpenGL/Direct3D implementations **********/
/*************************************************************************/

int sys_framebuffer_supported(void)
{
    if (window_is_d3d) {
        return d3d_sys_framebuffer_supported();
    } else {
        ASSERT(wgl_context);
        return opengl_sys_framebuffer_supported();
    }
}

/*-----------------------------------------------------------------------*/

SysFramebuffer *sys_framebuffer_create(
    int width, int height, FramebufferColorType color_type,
    int depth_bits_, int stencil_bits_)
{
    if (window_is_d3d) {
        return (SysFramebuffer *)d3d_sys_framebuffer_create(width, height, color_type, depth_bits_, stencil_bits_);
    } else {
        ASSERT(wgl_context);
        return (SysFramebuffer *)opengl_sys_framebuffer_create(width, height, color_type, depth_bits_, stencil_bits_);
    }
}

/*-----------------------------------------------------------------------*/

void sys_framebuffer_destroy(SysFramebuffer *framebuffer)
{
    if (window_is_d3d) {
        d3d_sys_framebuffer_destroy((struct D3DSysFramebuffer *)framebuffer);
    } else {
        ASSERT(wgl_context);
        opengl_sys_framebuffer_destroy((struct OpenGLSysFramebuffer *)framebuffer);
    }
}

/*-----------------------------------------------------------------------*/

void sys_framebuffer_bind(SysFramebuffer *framebuffer)
{
    if (window_is_d3d) {
        d3d_sys_framebuffer_bind((struct D3DSysFramebuffer *)framebuffer);
    } else {
        ASSERT(wgl_context);
        opengl_sys_framebuffer_bind((struct OpenGLSysFramebuffer *)framebuffer);
    }
}

/*-----------------------------------------------------------------------*/

SysTexture *sys_framebuffer_get_texture(SysFramebuffer *framebuffer)
{
    if (window_is_d3d) {
        return (SysTexture *)d3d_sys_framebuffer_get_texture((struct D3DSysFramebuffer *)framebuffer);
    } else {
        ASSERT(wgl_context);
        return (SysTexture *)opengl_sys_framebuffer_get_texture((struct OpenGLSysFramebuffer *)framebuffer);
    }
}

/*-----------------------------------------------------------------------*/

void sys_framebuffer_set_antialias(SysFramebuffer *framebuffer, int on)
{
    if (window_is_d3d) {
        d3d_sys_framebuffer_set_antialias((struct D3DSysFramebuffer *)framebuffer, on);
    } else {
        ASSERT(wgl_context);
        opengl_sys_framebuffer_set_antialias((struct OpenGLSysFramebuffer *)framebuffer, on);
    }
}

/*-----------------------------------------------------------------------*/

void sys_framebuffer_discard_data(SysFramebuffer *framebuffer)
{
    if (window_is_d3d) {
        d3d_sys_framebuffer_discard_data((struct D3DSysFramebuffer *)framebuffer);
    } else {
        ASSERT(wgl_context);
        opengl_sys_framebuffer_discard_data((struct OpenGLSysFramebuffer *)framebuffer);
    }
}

/*-----------------------------------------------------------------------*/

const char *sys_graphics_renderer_info(void)
{
    if (window_is_d3d) {
        return d3d_sys_graphics_renderer_info();
    } else {
        ASSERT(wgl_context);
        return opengl_sys_graphics_renderer_info();
    }
}

/*-----------------------------------------------------------------------*/

void sys_graphics_clear(const Vector4f *color, const float *depth,
                        unsigned int stencil)
{
    if (window_is_d3d) {
        d3d_sys_graphics_clear(color, depth, stencil);
    } else {
        ASSERT(wgl_context);
        opengl_sys_graphics_clear(color, depth, stencil);
    }
}

/*-----------------------------------------------------------------------*/

int sys_graphics_read_pixels(int x, int y, int w, int h, int stride, void *buffer)
{
    if (window_is_d3d) {
        return d3d_sys_graphics_read_pixels(x, y, w, h, stride, buffer);
    } else {
        ASSERT(wgl_context);
        return opengl_sys_graphics_read_pixels(x, y, w, h, stride, buffer);
    }
}

/*-----------------------------------------------------------------------*/

void sys_graphics_set_viewport(int left, int bottom, int width, int height)
{
    if (window_is_d3d) {
        d3d_sys_graphics_set_viewport(left, bottom, width, height);
    } else {
        ASSERT(wgl_context);
        opengl_sys_graphics_set_viewport(left, bottom, width, height);
    }
}

/*-----------------------------------------------------------------------*/

void sys_graphics_set_clip_region(int left, int bottom, int width, int height)
{
    if (window_is_d3d) {
        d3d_sys_graphics_set_clip_region(left, bottom, width, height);
    } else {
        ASSERT(wgl_context);
        opengl_sys_graphics_set_clip_region(left, bottom, width, height);
    }
}

/*-----------------------------------------------------------------------*/

void sys_graphics_set_depth_range(float near, float far)
{
    if (window_is_d3d) {
        d3d_sys_graphics_set_depth_range(near, far);
    } else {
        ASSERT(wgl_context);
        opengl_sys_graphics_set_depth_range(near, far);
    }
}

/*-----------------------------------------------------------------------*/

int sys_graphics_set_blend(int operation, int src_factor, int dest_factor)
{
    if (window_is_d3d) {
        return d3d_sys_graphics_set_blend(operation, src_factor, dest_factor);
    } else {
        ASSERT(wgl_context);
        return opengl_sys_graphics_set_blend(operation, src_factor, dest_factor);
    }
}

/*-----------------------------------------------------------------------*/

int sys_graphics_set_blend_alpha(int enable, int src_factor, int dest_factor)
{
    if (window_is_d3d) {
        return d3d_sys_graphics_set_blend_alpha(enable, src_factor, dest_factor);
    } else {
        ASSERT(wgl_context);
        return opengl_sys_graphics_set_blend_alpha(enable, src_factor, dest_factor);
    }
}

/*-----------------------------------------------------------------------*/

void sys_graphics_set_int_param(SysGraphicsParam id, int value)
{
    if (window_is_d3d) {
        d3d_sys_graphics_set_int_param(id, value);
    } else {
        ASSERT(wgl_context);
        opengl_sys_graphics_set_int_param(id, value);
    }
}

/*-----------------------------------------------------------------------*/

void sys_graphics_set_float_param(SysGraphicsParam id, float value)
{
    if (window_is_d3d) {
        d3d_sys_graphics_set_float_param(id, value);
    } else {
        ASSERT(wgl_context);
        opengl_sys_graphics_set_float_param(id, value);
    }
}

/*-----------------------------------------------------------------------*/

void sys_graphics_set_vec2_param(SysGraphicsParam id, const Vector2f *value)
{
    if (window_is_d3d) {
        d3d_sys_graphics_set_vec2_param(id, value);
    } else {
        ASSERT(wgl_context);
        opengl_sys_graphics_set_vec2_param(id, value);
    }
}

/*-----------------------------------------------------------------------*/

void sys_graphics_set_vec4_param(SysGraphicsParam id, const Vector4f *value)
{
    if (window_is_d3d) {
        d3d_sys_graphics_set_vec4_param(id, value);
    } else {
        ASSERT(wgl_context);
        opengl_sys_graphics_set_vec4_param(id, value);
    }
}

/*-----------------------------------------------------------------------*/

void sys_graphics_set_matrix_param(SysGraphicsParam id, const Matrix4f *value)
{
    if (window_is_d3d) {
        d3d_sys_graphics_set_matrix_param(id, value);
    } else {
        ASSERT(wgl_context);
        opengl_sys_graphics_set_matrix_param(id, value);
    }
}

/*-----------------------------------------------------------------------*/

void sys_graphics_get_matrix_param(SysGraphicsParam id, Matrix4f *value_ret)
{
    if (window_is_d3d) {
        d3d_sys_graphics_get_matrix_param(id, value_ret);
    } else {
        ASSERT(wgl_context);
        opengl_sys_graphics_get_matrix_param(id, value_ret);
    }
}

/*-----------------------------------------------------------------------*/

float sys_graphics_max_point_size(void)
{
    if (window_is_d3d) {
        return d3d_sys_graphics_max_point_size();
    } else {
        ASSERT(wgl_context);
        return opengl_sys_graphics_max_point_size();
    }
}

/*-----------------------------------------------------------------------*/

SysPrimitive *sys_graphics_create_primitive(enum GraphicsPrimitiveType type, const void *data, const uint32_t *format, int size, int count, const void *index_data, int index_size, int index_count, int immediate)
{
    if (window_is_d3d) {
        return (SysPrimitive *)d3d_sys_graphics_create_primitive(type, data, format, size, count, index_data, index_size, index_count, immediate);
    } else {
        ASSERT(wgl_context);
        return (SysPrimitive *)opengl_sys_graphics_create_primitive(type, data, format, size, count, index_data, index_size, index_count, immediate);
    }
}

/*-----------------------------------------------------------------------*/

void sys_graphics_draw_primitive(SysPrimitive *primitive, int start, int count)
{
    if (window_is_d3d) {
        d3d_sys_graphics_draw_primitive((struct D3DSysPrimitive *)primitive, start, count);
    } else {
        ASSERT(wgl_context);
        opengl_sys_graphics_draw_primitive((struct OpenGLSysPrimitive *)primitive, start, count);
    }
}

/*-----------------------------------------------------------------------*/

void sys_graphics_destroy_primitive(SysPrimitive *primitive)
{
    if (window_is_d3d) {
        d3d_sys_graphics_destroy_primitive((struct D3DSysPrimitive *)primitive);
    } else {
        ASSERT(wgl_context);
        opengl_sys_graphics_destroy_primitive((struct OpenGLSysPrimitive *)primitive);
    }
}

/*-----------------------------------------------------------------------*/

int sys_graphics_set_shader_generator(void *vertex_source_callback, void *fragment_source_callback, void *key_callback, int hash_table_size, int dynamic_resize)
{
    if (window_is_d3d) {
        return vertex_source_callback == NULL;  // Not supported.
    } else {
        ASSERT(wgl_context);
        return opengl_sys_graphics_set_shader_generator(vertex_source_callback, fragment_source_callback, key_callback, hash_table_size, dynamic_resize);
    }
}

/*-----------------------------------------------------------------------*/

int sys_graphics_add_shader_uniform(const char *name)
{
    if (window_is_d3d) {
        return 0;  // Not supported.
    } else {
        ASSERT(wgl_context);
        return opengl_sys_graphics_add_shader_uniform(name);
    }
}

/*-----------------------------------------------------------------------*/

int sys_graphics_add_shader_attribute(const char *name, int size)
{
    if (window_is_d3d) {
        return 0;  // Not supported.
    } else {
        ASSERT(wgl_context);
        return opengl_sys_graphics_add_shader_attribute(name, size);
    }
}

/*-----------------------------------------------------------------------*/

void sys_graphics_set_shader_uniform_int(int uniform, int value)
{
    if (window_is_d3d) {
        return;  // Not supported.
    } else {
        ASSERT(wgl_context);
        opengl_sys_graphics_set_shader_uniform_int(uniform, value);
    }
}

/*-----------------------------------------------------------------------*/

void sys_graphics_set_shader_uniform_float(int uniform, float value)
{
    if (window_is_d3d) {
        return;  // Not supported.
    } else {
        ASSERT(wgl_context);
        opengl_sys_graphics_set_shader_uniform_float(uniform, value);
    }
}

/*-----------------------------------------------------------------------*/

void sys_graphics_set_shader_uniform_vec2(int uniform, const Vector2f *value)
{
    if (window_is_d3d) {
        return;  // Not supported.
    } else {
        ASSERT(wgl_context);
        opengl_sys_graphics_set_shader_uniform_vec2(uniform, value);
    }
}

/*-----------------------------------------------------------------------*/

void sys_graphics_set_shader_uniform_vec3(int uniform, const Vector3f *value)
{
    if (window_is_d3d) {
        return;  // Not supported.
    } else {
        ASSERT(wgl_context);
        opengl_sys_graphics_set_shader_uniform_vec3(uniform, value);
    }
}

/*-----------------------------------------------------------------------*/

void sys_graphics_set_shader_uniform_vec4(int uniform, const Vector4f *value)
{
    if (window_is_d3d) {
        return;  // Not supported.
    } else {
        ASSERT(wgl_context);
        opengl_sys_graphics_set_shader_uniform_vec4(uniform, value);
    }
}

/*-----------------------------------------------------------------------*/

void sys_graphics_set_shader_uniform_mat4(int uniform, const Matrix4f *value)
{
    if (window_is_d3d) {
        return;  // Not supported.
    } else {
        ASSERT(wgl_context);
        opengl_sys_graphics_set_shader_uniform_mat4(uniform, value);
    }
}

/*-----------------------------------------------------------------------*/

int sys_graphics_enable_shader_objects(void)
{
    if (window_is_d3d) {
        return d3d_sys_graphics_enable_shader_objects();
    } else {
        ASSERT(wgl_context);
        return opengl_sys_graphics_enable_shader_objects();
    }
}

/*-----------------------------------------------------------------------*/

int sys_graphics_disable_shader_objects(void)
{
    if (window_is_d3d) {
        return d3d_sys_graphics_disable_shader_objects();
    } else {
        ASSERT(wgl_context);
        return opengl_sys_graphics_disable_shader_objects();
    }
}

/*-----------------------------------------------------------------------*/

int sys_shader_background_compilation_supported(void)
{
    if (window_is_d3d) {
        return d3d_sys_shader_background_compilation_supported();
    } else {
        ASSERT(wgl_context);
        return opengl_sys_shader_background_compilation_supported();
    }
}

/*-----------------------------------------------------------------------*/

void sys_shader_enable_get_binary(int enable)
{
    if (window_is_d3d) {
        d3d_sys_shader_enable_get_binary(enable);
    } else {
        ASSERT(wgl_context);
        opengl_sys_shader_enable_get_binary(enable);
    }
}

/*-----------------------------------------------------------------------*/

int sys_shader_max_attributes(void)
{
    if (window_is_d3d) {
        return d3d_sys_shader_max_attributes();
    } else {
        ASSERT(wgl_context);
        return opengl_sys_shader_max_attributes();
    }
}

/*-----------------------------------------------------------------------*/

int sys_shader_set_attribute(int index, const char *name)
{
    if (window_is_d3d) {
        return d3d_sys_shader_set_attribute(index, name);
    } else {
        ASSERT(wgl_context);
        return opengl_sys_shader_set_attribute(index, name);
    }
}

/*-----------------------------------------------------------------------*/

void sys_shader_bind_standard_attribute(enum ShaderAttribute attribute, int index)
{
    if (window_is_d3d) {
        d3d_sys_shader_bind_standard_attribute(attribute, index);
    } else {
        ASSERT(wgl_context);
        opengl_sys_shader_bind_standard_attribute(attribute, index);
    }
}

/*-----------------------------------------------------------------------*/

void sys_shader_clear_attributes(void)
{
    if (window_is_d3d) {
        d3d_sys_shader_clear_attributes();
    } else {
        ASSERT(wgl_context);
        opengl_sys_shader_clear_attributes();
    }
}

/*-----------------------------------------------------------------------*/

SysShader *sys_shader_create(enum ShaderType type, const void *data, int size, int is_binary)
{
    if (window_is_d3d) {
        return (SysShader *)d3d_sys_shader_create(type, data, size, is_binary);
    } else {
        ASSERT(wgl_context);
        return (SysShader *)opengl_sys_shader_create(type, data, size, is_binary);
    }
}

/*-----------------------------------------------------------------------*/

void sys_shader_destroy(SysShader *shader)
{
    if (window_is_d3d) {
        d3d_sys_shader_destroy((struct D3DSysShader *)shader);
    } else {
        ASSERT(wgl_context);
        opengl_sys_shader_destroy((struct OpenGLSysShader *)shader);
    }
}

/*-----------------------------------------------------------------------*/

void *sys_shader_get_binary(SysShader *shader, int *size_ret)
{
    if (window_is_d3d) {
        return d3d_sys_shader_get_binary((struct D3DSysShader *)shader, size_ret);
    } else {
        ASSERT(wgl_context);
        return opengl_sys_shader_get_binary((struct OpenGLSysShader *)shader, size_ret);
    }
}

/*-----------------------------------------------------------------------*/

void *sys_shader_compile(enum ShaderType type, const char *source, int length, int *size_ret)
{
    if (window_is_d3d) {
        return d3d_sys_shader_compile(type, source, length, size_ret);
    } else {
        ASSERT(wgl_context);
        return opengl_sys_shader_compile(type, source, length, size_ret);
    }
}

/*-----------------------------------------------------------------------*/

int sys_shader_get_uniform_id(SysShader *shader, const char *name)
{
    if (window_is_d3d) {
        return d3d_sys_shader_get_uniform_id((struct D3DSysShader *)shader, name);
    } else {
        ASSERT(wgl_context);
        return opengl_sys_shader_get_uniform_id((struct OpenGLSysShader *)shader, name);
    }
}

/*-----------------------------------------------------------------------*/

void sys_shader_set_uniform_int(SysShader *shader, int uniform, int value)
{
    if (window_is_d3d) {
        d3d_sys_shader_set_uniform_int((struct D3DSysShader *)shader, uniform, value);
    } else {
        ASSERT(wgl_context);
        opengl_sys_shader_set_uniform_int((struct OpenGLSysShader *)shader, uniform, value);
    }
}

/*-----------------------------------------------------------------------*/

void sys_shader_set_uniform_float(SysShader *shader, int uniform, float value)
{
    if (window_is_d3d) {
        d3d_sys_shader_set_uniform_float((struct D3DSysShader *)shader, uniform, value);
    } else {
        ASSERT(wgl_context);
        opengl_sys_shader_set_uniform_float((struct OpenGLSysShader *)shader, uniform, value);
    }
}

/*-----------------------------------------------------------------------*/

void sys_shader_set_uniform_vec2(SysShader *shader, int uniform, const Vector2f *value)
{
    if (window_is_d3d) {
        d3d_sys_shader_set_uniform_vec2((struct D3DSysShader *)shader, uniform, value);
    } else {
        ASSERT(wgl_context);
        opengl_sys_shader_set_uniform_vec2((struct OpenGLSysShader *)shader, uniform, value);
    }
}

/*-----------------------------------------------------------------------*/

void sys_shader_set_uniform_vec3(SysShader *shader, int uniform, const Vector3f *value)
{
    if (window_is_d3d) {
        d3d_sys_shader_set_uniform_vec3((struct D3DSysShader *)shader, uniform, value);
    } else {
        ASSERT(wgl_context);
        opengl_sys_shader_set_uniform_vec3((struct OpenGLSysShader *)shader, uniform, value);
    }
}

/*-----------------------------------------------------------------------*/

void sys_shader_set_uniform_vec4(SysShader *shader, int uniform, const Vector4f *value)
{
    if (window_is_d3d) {
        d3d_sys_shader_set_uniform_vec4((struct D3DSysShader *)shader, uniform, value);
    } else {
        ASSERT(wgl_context);
        opengl_sys_shader_set_uniform_vec4((struct OpenGLSysShader *)shader, uniform, value);
    }
}

/*-----------------------------------------------------------------------*/

void sys_shader_set_uniform_mat4(SysShader *shader, int uniform, const Matrix4f *value)
{
    if (window_is_d3d) {
        d3d_sys_shader_set_uniform_mat4((struct D3DSysShader *)shader, uniform, value);
    } else {
        ASSERT(wgl_context);
        opengl_sys_shader_set_uniform_mat4((struct OpenGLSysShader *)shader, uniform, value);
    }
}

/*-----------------------------------------------------------------------*/

SysShaderPipeline *sys_shader_pipeline_create(SysShader *vertex_shader, SysShader *fragment_shader)
{
    if (window_is_d3d) {
        return (SysShaderPipeline *)d3d_sys_shader_pipeline_create((struct D3DSysShader *)vertex_shader, (struct D3DSysShader *)fragment_shader);
    } else {
        ASSERT(wgl_context);
        return (SysShaderPipeline *)opengl_sys_shader_pipeline_create((struct OpenGLSysShader *)vertex_shader, (struct OpenGLSysShader *)fragment_shader);
    }
}

/*-----------------------------------------------------------------------*/

void sys_shader_pipeline_destroy(SysShaderPipeline *pipeline)
{
    if (window_is_d3d) {
        d3d_sys_shader_pipeline_destroy((struct D3DSysShaderPipeline *)pipeline);
    } else {
        ASSERT(wgl_context);
        opengl_sys_shader_pipeline_destroy((struct OpenGLSysShaderPipeline *)pipeline);
    }
}

/*-----------------------------------------------------------------------*/

void sys_shader_pipeline_apply(SysShaderPipeline *pipeline)
{
    if (window_is_d3d) {
        d3d_sys_shader_pipeline_apply((struct D3DSysShaderPipeline *)pipeline);
    } else {
        ASSERT(wgl_context);
        opengl_sys_shader_pipeline_apply((struct OpenGLSysShaderPipeline *)pipeline);
    }
}

/*-----------------------------------------------------------------------*/

SysTexture *sys_texture_create(int width, int height, enum TextureFormat data_format, int num_levels, void *data, int stride, const int32_t *level_offsets, const int32_t *level_sizes, int mipmaps, int mem_flags, int reuse)
{
    if (window_is_d3d) {
        return (SysTexture *)d3d_sys_texture_create(width, height, data_format, num_levels, data, stride, level_offsets, level_sizes, mipmaps, mem_flags, reuse);
    } else {
        ASSERT(wgl_context);
        return (SysTexture *)opengl_sys_texture_create(width, height, data_format, num_levels, data, stride, level_offsets, level_sizes, mipmaps, mem_flags, reuse);
    }
}

/*-----------------------------------------------------------------------*/

void sys_texture_destroy(SysTexture *texture)
{
    if (window_is_d3d) {
        d3d_sys_texture_destroy((struct D3DSysTexture *)texture);
    } else {
        ASSERT(wgl_context);
        opengl_sys_texture_destroy((struct OpenGLSysTexture *)texture);
    }
}

/*-----------------------------------------------------------------------*/

int sys_texture_width(SysTexture *texture)
{
    if (window_is_d3d) {
        return d3d_sys_texture_width((struct D3DSysTexture *)texture);
    } else {
        ASSERT(wgl_context);
        return opengl_sys_texture_width((struct OpenGLSysTexture *)texture);
    }
}

/*-----------------------------------------------------------------------*/

int sys_texture_height(SysTexture *texture)
{
    if (window_is_d3d) {
        return d3d_sys_texture_height((struct D3DSysTexture *)texture);
    } else {
        ASSERT(wgl_context);
        return opengl_sys_texture_height((struct OpenGLSysTexture *)texture);
    }
}

/*-----------------------------------------------------------------------*/

int sys_texture_has_mipmaps(SysTexture *texture)
{
    if (window_is_d3d) {
        return d3d_sys_texture_has_mipmaps((struct D3DSysTexture *)texture);
    } else {
        ASSERT(wgl_context);
        return opengl_sys_texture_has_mipmaps((struct OpenGLSysTexture *)texture);
    }
}

/*-----------------------------------------------------------------------*/

SysTexture *sys_texture_grab(int x, int y, int w, int h, int readable, int mipmaps, int mem_flags)
{
    if (window_is_d3d) {
        return (SysTexture *)d3d_sys_texture_grab(x, y, w, h, readable, mipmaps, mem_flags);
    } else {
        ASSERT(wgl_context);
        return (SysTexture *)opengl_sys_texture_grab(x, y, w, h, readable, mipmaps, mem_flags);
    }
}

/*-----------------------------------------------------------------------*/

void *sys_texture_lock(SysTexture *texture, SysTextureLockMode lock_mode, int x, int y, int w, int h)
{
    if (window_is_d3d) {
        return d3d_sys_texture_lock((struct D3DSysTexture *)texture, lock_mode, x, y, w, h);
    } else {
        ASSERT(wgl_context);
        return opengl_sys_texture_lock((struct OpenGLSysTexture *)texture, lock_mode, x, y, w, h);
    }
}

/*-----------------------------------------------------------------------*/

void sys_texture_unlock(SysTexture *texture, int update)
{
    if (window_is_d3d) {
        d3d_sys_texture_unlock((struct D3DSysTexture *)texture, update);
    } else {
        ASSERT(wgl_context);
        opengl_sys_texture_unlock((struct OpenGLSysTexture *)texture, update);
    }
}

/*-----------------------------------------------------------------------*/

void sys_texture_flush(SysTexture *texture)
{
    if (window_is_d3d) {
        d3d_sys_texture_flush((struct D3DSysTexture *)texture);
    } else {
        ASSERT(wgl_context);
        opengl_sys_texture_flush((struct OpenGLSysTexture *)texture);
    }
}

/*-----------------------------------------------------------------------*/

void sys_texture_set_repeat(SysTexture *texture, int repeat_u, int repeat_v)
{
    if (window_is_d3d) {
        d3d_sys_texture_set_repeat((struct D3DSysTexture *)texture, repeat_u, repeat_v);
    } else {
        ASSERT(wgl_context);
        opengl_sys_texture_set_repeat((struct OpenGLSysTexture *)texture, repeat_u, repeat_v);
    }
}

/*-----------------------------------------------------------------------*/

void sys_texture_set_antialias(SysTexture *texture, int on)
{
    if (window_is_d3d) {
        d3d_sys_texture_set_antialias((struct D3DSysTexture *)texture, on);
    } else {
        ASSERT(wgl_context);
        opengl_sys_texture_set_antialias((struct OpenGLSysTexture *)texture, on);
    }
}

/*-----------------------------------------------------------------------*/

void sys_texture_apply(int unit, SysTexture *texture)
{
    if (window_is_d3d) {
        d3d_sys_texture_apply(unit, (struct D3DSysTexture *)texture);
    } else {
        ASSERT(wgl_context);
        opengl_sys_texture_apply(unit, (struct OpenGLSysTexture *)texture);
    }
}

/*-----------------------------------------------------------------------*/

int sys_texture_num_units(void)
{
    if (window_is_d3d) {
        return d3d_sys_texture_num_units();
    } else {
        ASSERT(wgl_context);
        return opengl_sys_texture_num_units();
    }
}

/*************************************************************************/
/****************** Windows-internal exported routines *******************/
/*************************************************************************/

void windows_update_window(void)
{
    if (current_window && !window_thread) {
        /* SIL_WM_APP_CLOSE is never sent in single-threaded mode, so we
         * don't have to check for a return value of zero. */
        while (update_window(0) != -1) { /*spin*/ }
    }
}

/*-----------------------------------------------------------------------*/

void windows_close_window(void)
{
    if (current_window) {
        close_window();
    }
}

/*-----------------------------------------------------------------------*/

void windows_flush_message_queue(void)
{
    if (current_window) {
        if (window_thread) {
            HANDLE event = CreateEvent(NULL, FALSE, FALSE, NULL);
            if (UNLIKELY(!event)) {
                DLOG("Failed to create flush event");
                return;
            }
            PostMessage(current_window, WM_APP, SIL_WM_APP_FLUSH,
                        (LPARAM)event);
            WaitForSingleObject(event, INFINITE);
            CloseHandle(event);
        } else {
            windows_update_window();
        }
    }
}

/*-----------------------------------------------------------------------*/

void windows_reset_video_mode(void)
{
    set_video_mode(-1);
}

/*-----------------------------------------------------------------------*/

void windows_set_mouse_grab(int grab)
{
    grab_requested = (grab != 0);
    update_mouse_grab();
}

/*-----------------------------------------------------------------------*/

void windows_show_mouse_pointer(int override)
{
    const int on = (override >= 0) ? override : show_mouse_pointer;

    /* ShowCursor() acts like a counter rather than a boolean flag, so
     * we have to repeatedly call it until the cursor actually turns on
     * or off (as indicated by the return value).  To keep the counter
     * from incrementing or decrementing without bound, we advance to
     * one past the target value and then go in the opposite direction
     * until we reach the target. */
    if (on) {
        while (ShowCursor(TRUE) < 1) {}
        while (ShowCursor(FALSE) > 0) {}
    } else {
        while (ShowCursor(FALSE) > -2) {}
        while (ShowCursor(TRUE) < -1) {}
    }
}

/*-----------------------------------------------------------------------*/

int windows_vsync_interval(void)
{
    return vsync;
}

/*-----------------------------------------------------------------------*/

HGLRC windows_wgl_context(void)
{
    return wgl_context;
}

/*-----------------------------------------------------------------------*/

HWND windows_window(void)
{
    return current_window;
}

/*-----------------------------------------------------------------------*/

const char *windows_window_title(void)
{
    return window_title;
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static ATOM register_window_class(const char *name, WNDPROC wndproc,
                                  HICON icon)
{
    char namebuf[256];
    WNDCLASSEX class_def;
    mem_clear(&class_def, sizeof(class_def));
    class_def.cbSize = sizeof(class_def);
    class_def.lpfnWndProc = wndproc;
    class_def.hInstance = GetModuleHandle(NULL);
    class_def.hCursor = LoadCursor(NULL, IDC_ARROW);
    class_def.hIcon = icon;
    class_def.lpszClassName = namebuf;

    LARGE_INTEGER now_buf;
    QueryPerformanceCounter(&now_buf);
    uint64_t random_state = now_buf.QuadPart;
    for (int try = 0; try < 20; try++) {
        ASSERT(strformat_check(namebuf, sizeof(namebuf), "%s.%d", name,
                               random32_r(&random_state)));
        ATOM class = RegisterClassEx(&class_def);
        if (class) {
            return class;
        }
        DLOG("RegisterClassEx(%s) failed: %s", namebuf,
             windows_strerror(GetLastError()));
    }

    DLOG("Unable to generate valid random name for class %s", name);
    return 0;
}

/*-----------------------------------------------------------------------*/

static DWORD CALLBACK window_thread_func(LPVOID params_)
{
    const CreateWindowParams *params = params_;
    create_window(params);
    SetEvent(create_window_event);
    if (UNLIKELY(!current_window)) {
        return 0;
    }
    if (!window_is_d3d) {
        /* Make sure default message behaviors don't try to operate on
         * the window until the pixel format has been set. */
        WaitForSingleObject(setpixelformat_event, INFINITE);
    }

    while (update_window(1)) { /*spin*/ }

    DestroyWindow(current_window);
    current_window = NULL;
    return 0;
}

/*-----------------------------------------------------------------------*/

static void create_window(const CreateWindowParams *params)
{
    const DWORD style =
        (window_fullscreen ? WS_POPUP : windowed_style) | WS_VISIBLE;
    const int x = (window_fullscreen || params->do_center
                   ? params->rect.left : CW_USEDEFAULT);
    const int y = (window_fullscreen || params->do_center
                   ? params->rect.top : CW_USEDEFAULT);
    const int width = params->rect.right - params->rect.left;
    const int height = params->rect.bottom - params->rect.top;
    DLOG("Creating window: title=[%s] style=0x%X origin=%d,%d size=%dx%d",
         window_title, style, x, y, width, height);
    current_window = CreateWindow(
        (void *)(uintptr_t)window_class, window_title, style,
        x, y, width, height, NULL, NULL, GetModuleHandle(NULL), NULL);
    if (UNLIKELY(!current_window)) {
        DLOG("CreateWindow() failed: %s", windows_strerror(GetLastError()));
    }
}

/*-----------------------------------------------------------------------*/

static int update_window(int block)
{
    MSG message;
    if (block) {
        GetMessage(&message, NULL, 0, 0);
    } else {
        if (!PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) {
            return -1;
        }
    }

    if (message.hwnd == current_window
     && message.message == WM_APP
     && message.wParam == SIL_WM_APP_CLOSE) {
        /* We use WM_APP with a magic cookie to signal a window close
         * request from SIL.  We previously used WM_USER based on the
         * documentation that WM_USER is available for use by private
         * window classes, but (partly because we also forgot to check
         * the message HWND) that triggered spurious closes when a
         * Windows-internal hidden window received a WM_USER message.
         * (The wParam of that message was 0xBABE, which I suppose says
         * something about the mindset of Microsoft programmers...) */
        return 0;
    } else if (message.message == WM_QUIT) {
        /* Windows refuses to send this to the window procedure, so we
         * have to handle it separately. */
        quit_requested = 1;
    } else {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

static LRESULT CALLBACK window_proc(HWND hwnd, UINT uMsg,
                                    WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_ACTIVATE) {
        /* We get all three of WM_ACTIVATEAPP, WM_ACTIVATE, and WM_SETFOCUS
         * when a window is activated, and similarly (but with WM_KILLFOCUS)
         * when a window is deactivated.  We use WM_ACTIVATE as it directly
         * reflects the activation state of the window and is probably best
         * to ensure that fullscreen auto-minimize is applied when
         * appropriate, though in practice it may not make any difference. */
        window_focused = (LOWORD(wParam) != WA_INACTIVE);
        update_mouse_grab();
        if (!window_focused && window_fullscreen
         && should_minimize_fullscreen()) {
            ShowWindow(hwnd, SW_MINIMIZE);
        }
        return DefWindowProc(hwnd, uMsg, wParam, lParam);

    } else if (uMsg == WM_APP) {
        if (wParam == SIL_WM_APP_CLOSE) {
            ASSERT(!"unreachable");  // Handled in the thread function.
        } else if (wParam == SIL_WM_APP_FLUSH) {
            HANDLE event = (HANDLE)lParam;
            ASSERT(SetEvent(event));
        } else {
            DLOG("Unexpected WM_APP message with wParam 0x%X", wParam);
        }
        return 1;

    } else if (uMsg == WM_CLOSE) {
        quit_requested = 1;
        return 0;

    } else if (uMsg == WM_ENDSESSION) {
        if (lParam & ENDSESSION_CRITICAL) {
            /* For a critical shutdown, let the OS close us immediately. */
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }
        quit_requested = 1;
        return 0;

    } else if (uMsg == WM_GETMINMAXINFO) {
        MINMAXINFO *info = (MINMAXINFO *)lParam;
        DWORD style = GetWindowLong(current_window, GWL_STYLE);
        if (style == 0) {
            /* It seems the window style is not yet set when we get the
             * first message of this type, so we have to figure out what
             * it will eventually be based on the window parameters. */
            if (window_fullscreen) {
                style = WS_POPUP;
            } else {
                style = windowed_style;
            }
        }
        if (minmax_override_width > 0 && minmax_override_height > 0) {
            RECT rect = {.left = 0, .top = 0, .right = minmax_override_width,
                         .bottom = minmax_override_height};
            AdjustWindowRectEx(&rect, style, FALSE, 0);
            info->ptMinTrackSize.x = rect.right - rect.left;
            info->ptMinTrackSize.y = rect.bottom - rect.top;
            info->ptMaxTrackSize.x = rect.right - rect.left;
            info->ptMaxTrackSize.y = rect.bottom - rect.top;
        } else if (window_resizable && !fullscreen) {
            if (window_min_width && window_min_height) {
                RECT rect = {.left = 0, .top = 0, .right = window_min_width,
                             .bottom = window_min_height};
                AdjustWindowRectEx(&rect, style, FALSE, 0);
                info->ptMinTrackSize.x = rect.right - rect.left;
                info->ptMinTrackSize.y = rect.bottom - rect.top;
            }
            if (window_max_width && window_max_height) {
                RECT rect = {.left = 0, .top = 0, .right = window_max_width,
                             .bottom = window_max_height};
                AdjustWindowRectEx(&rect, style, FALSE, 0);
                info->ptMaxTrackSize.x = rect.right - rect.left;
                info->ptMaxTrackSize.y = rect.bottom - rect.top;
            }
        } else {
            RECT rect = {.left = 0, .top = 0, .right = window_width,
                         .bottom = window_height};
            AdjustWindowRectEx(&rect, style, FALSE, 0);
            info->ptMinTrackSize.x = rect.right - rect.left;
            info->ptMinTrackSize.y = rect.bottom - rect.top;
            info->ptMaxTrackSize.x = rect.right - rect.left;
            info->ptMaxTrackSize.y = rect.bottom - rect.top;
        }
        return 0;

    } else if (uMsg == WM_SETCURSOR) {
        /* We don't want to hide the cursor in the window's title bar,
         * so override the current ShowCursor() setting in that case.
         * (courtesy stackoverflow://5629613) */
        if (LOWORD(lParam) == HTCLIENT) {
            windows_show_mouse_pointer(-1);
        } else {
            windows_show_mouse_pointer(1);
        }
        return DefWindowProc(hwnd, uMsg, wParam, lParam);

    } else if (uMsg == WM_SIZE) {
        if (wParam == SIZE_MINIMIZED) {
            if (!window_minimized) {
                if (window_fullscreen) {
                    saved_mode = current_mode;
                    set_video_mode(-1);
                }
                window_minimized = 1;
                update_mouse_grab();
            }
        } else if (wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED) {
            if (window_minimized) {
                if (window_fullscreen) {
                    if (saved_mode >= 0 && UNLIKELY(!set_video_mode(saved_mode))) {
                        /* Try to recover by switching to windowed mode. */
                        const int saved_fullscreen = fullscreen;
                        fullscreen = 0;
                        sys_graphics_set_display_mode(window_width, window_height);
                        fullscreen = saved_fullscreen;
                    }
                }
                window_minimized = 0;
                update_mouse_grab();
            }
        }

        /* Call DefWindowProc() before setting window_resize_pending in
         * case it triggers any device-level operations (buffer changes
         * etc.), to avoid any risk of racing with the main thread
         * detecting window_resize_pending set. */
        const int result = DefWindowProc(hwnd, uMsg, wParam, lParam);
        BARRIER();

        if (wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED) {
            const int width = LOWORD(lParam);
            const int height = HIWORD(lParam);
            if (width > 0 && height > 0) {  // Should always be true, but just in case.
                window_resize_pending = 1;
                window_resize_new_width = width;
                window_resize_new_height = height;
            }
            window_resize_last_width = width;
            window_resize_last_height = height;
        }

        return result;

    } else if (uMsg == WM_SIZING) {
        /* WM_SIZING gives us the window size (not client size), so we
         * need to calculate the client size to enforce constraints. */
        RECT nc_size = {0, 0, 0, 0};
        AdjustWindowRectEx(
            &nc_size, GetWindowLong(current_window, GWL_STYLE), FALSE, 0);
        const int nc_width = nc_size.right - nc_size.left;
        const int nc_height = nc_size.bottom - nc_size.top;
        RECT *rect = (RECT *)lParam;
        int width = rect->right - rect->left - nc_width;
        int height = rect->bottom - rect->top - nc_height;

        apply_window_resize_limits(&width, &height, wParam);

        window_resize_last_width = width;
        window_resize_last_height = height;

        width += nc_width;
        height += nc_height;
        /* If the callback changed the height on a WMSZ_LEFT/RIGHT or
         * the width on a WMSZ_TOP/BOTTOM, we arbitrarily choose to
         * move the upper-left or lower-right corner as appropriate. */
        if (wParam == WMSZ_BOTTOMLEFT) {
            rect->left = rect->right - width;
            rect->bottom = rect->top + height;
        } else if (wParam == WMSZ_LEFT
                || wParam == WMSZ_TOPLEFT
                || wParam == WMSZ_TOP) {
            rect->left = rect->right - width;
            rect->top = rect->bottom - height;
        } else if (wParam == WMSZ_TOPRIGHT) {
            rect->right = rect->left + width;
            rect->top = rect->bottom - height;
        } else if (wParam == WMSZ_RIGHT
                || wParam == WMSZ_BOTTOMRIGHT
                || wParam == WMSZ_BOTTOM) {
            rect->right = rect->left + width;
            rect->bottom = rect->top + height;
        }
        return TRUE;

    } else if (uMsg == WM_SYSCOMMAND && (wParam & 0xFFF0) == SC_MAXIMIZE) {
        if (!window_fullscreen && window_resizable) {
            window_minimized = 0;
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        } else {
            if (window_minimized) {
                DefWindowProc(hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
                ASSERT(window_minimized == 0);
            }
            return 0;
        }

    } else if (uMsg == WM_SYSCOMMAND && (wParam & 0xFFF0) == SC_KEYMENU) {
        /* Only allow the window menu through, and only if not fullscreen.
         * Passing this through unconditionally causes the window to lose
         * focus after an Alt press. */
        if (!window_fullscreen && lParam == ' ') {
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        } else {
            return 0;
        }

    } else if (uMsg == WM_UNICHAR && wParam == UNICODE_NOCHAR) {
        return TRUE;  // Declare that we support WM_UNICHAR.

    } else if (uMsg == WM_WINDOWPOSCHANGING) {
        /* The "snap" feature in Windows 10 bypasses WM_SIZING, so we have
         * to enforce window resize constraints manually here. */
        WINDOWPOS *wp = (WINDOWPOS *)lParam;
        if (!(wp->flags & SWP_NOSIZE)) {
            const DWORD style = GetWindowLong(current_window, GWL_STYLE);
            const int is_fullscreen = (style & WS_POPUP) != 0;
            if (!is_fullscreen && window_resizable) {
                RECT nc_size = {0, 0, 0, 0};
                AdjustWindowRectEx(&nc_size, style, FALSE, 0);
                const int nc_width = nc_size.right - nc_size.left;
                const int nc_height = nc_size.bottom - nc_size.top;
                int width = wp->cx - nc_width;
                int height = wp->cy - nc_height;
                apply_window_resize_limits(&width, &height, WMSZ_BOTTOMRIGHT);
                wp->cx = width + nc_width;
                wp->cy = height + nc_height;
            }
        }
        return DefWindowProc(hwnd, uMsg, wParam, lParam);

    } else if (windows_handle_input_message(hwnd, uMsg, wParam, lParam)) {
        return 0;

    } else {
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

/*-----------------------------------------------------------------------*/

static void apply_window_resize_limits(int *width_ptr, int *height_ptr,
                                       WPARAM wParam)
{
    int width = *width_ptr;
    int height = *height_ptr;

    if (window_min_width > 0 && window_min_height > 0) {
        width = lbound(width, window_min_width);
        height = lbound(height, window_min_height);
    }
    if (window_max_width > 0 && window_max_height > 0) {
        width = ubound(width, window_max_width);
        height = ubound(height, window_max_height);
    }

    /*
     * Apply aspect ratio limits, since Windows does not support them
     * natively.  These tests are formulated so that we don't have to
     * explicitly check for an unset limit; the tests will naturally
     * fail if the corresponding limit is unset.
     *
     * When adjusting the aspect ratio, we choose a coordinate to
     * modify as follows:
     *    - If dragging in only one dimension, we modify the other
     *      dimension.
     *    - Otherwise, we modify whichever of width and height requires
     *      the smaller adjustment.
     */
    if (width * window_min_aspect_y < height * window_min_aspect_x) {
        const int width_for_height =
            (height * window_min_aspect_x + window_min_aspect_y/2)
            / window_min_aspect_y;
        const int height_for_width =
            (width * window_min_aspect_y + window_min_aspect_x/2)
            / window_min_aspect_x;
        const int dw = width_for_height - window_resize_last_width;
        const int dh = height_for_width - window_resize_last_height;
        if (wParam == WMSZ_TOP || wParam == WMSZ_BOTTOM) {
            width = width_for_height;
        } else if (wParam == WMSZ_LEFT || wParam == WMSZ_RIGHT) {
            height = height_for_width;
        } else if (abs(dw) < abs(dh)) {
            width = width_for_height;
        } else {
            height = height_for_width;
        }
    }
    if (width * window_max_aspect_y > height * window_max_aspect_x) {
        const int width_for_height =
            (height * window_max_aspect_x + window_max_aspect_y/2)
            / window_max_aspect_y;
        const int height_for_width =
            (width * window_max_aspect_y + window_max_aspect_x/2)
            / window_max_aspect_x;
        const int dw = width_for_height - window_resize_last_width;
        const int dh = height_for_width - window_resize_last_height;
        if (wParam == WMSZ_TOP || wParam == WMSZ_BOTTOM) {
            width = width_for_height;
        } else if (wParam == WMSZ_LEFT || wParam == WMSZ_RIGHT) {
            height = height_for_width;
        } else if (abs(dw) < abs(dh)) {
            width = width_for_height;
        } else {
            height = height_for_width;
        }
    }

    *width_ptr = width;
    *height_ptr = height;
}

/*-----------------------------------------------------------------------*/

static void resize_backend(int width, int height)
{
    PRECOND(current_window != NULL, return);
    window_width = width;
    window_height = height;
    if (window_is_d3d) {
        d3d_resize_window();
    } else {
        opengl_set_display_size(width, height);
    }
}

/*-----------------------------------------------------------------------*/

static int add_video_mode(int device, const DEVMODE *mode_info)
{
    const int new_num_modes = graphics_info.num_modes + 1;
    void *new_device_modes = mem_realloc(
        device_modes, sizeof(*device_modes) * new_num_modes, 0);
    if (UNLIKELY(!new_device_modes)) {
        DLOG("No memory to expand device mode list to %d entries",
             new_num_modes);
        return 0;
    }
    device_modes = new_device_modes;
    GraphicsDisplayModeEntry *modes = mem_realloc(
        (GraphicsDisplayModeEntry *)graphics_info.modes,
        sizeof(*graphics_info.modes) * new_num_modes, 0);
    if (UNLIKELY(!modes)) {
        DLOG("No memory to expand mode list to %d entries", new_num_modes);
        return 0;
    }
    graphics_info.modes = modes;
    const int index = graphics_info.num_modes++;
    device_modes[index] = *mode_info;
    modes[index].device = device;
    modes[index].device_name = devices[device].name;
    modes[index].width = mode_info->dmPelsWidth;
    modes[index].height = mode_info->dmPelsHeight;
    modes[index].refresh = mode_info->dmDisplayFrequency;
    return 1;
}

/*-----------------------------------------------------------------------*/

static int set_video_mode(int mode)
{
    PRECOND(mode >= -1 && mode < graphics_info.num_modes, return 0);

    if (mode == current_mode) {
        return 1;
    }

    int device;
    if (mode >= 0) {
        device = graphics_info.modes[mode].device;
    } else if (current_window) {
        device = current_device;
    } else {
        device = device_to_use;
    }
    if (current_mode >= 0
     && device != graphics_info.modes[current_mode].device) {
        set_video_mode(-1);
    }

#ifdef DEBUG
    char old_mode_desc[100], new_mode_desc[100];
    describe_mode(current_mode, old_mode_desc, sizeof(old_mode_desc));
    describe_mode(mode, new_mode_desc, sizeof(new_mode_desc));
    DLOG("Changing mode on device %s: %s to %s",
         devices[device].name, old_mode_desc, new_mode_desc);
#endif

    LONG result;
    if (mode == -1) {
        /* The Windows documentation suggests that we should use a flags
         * value of 0 when switching back to the default mode, but SDL's
         * experience is that doing so causes windows to be resized and
         * rearranged based on the current mode, so we set CDS_FULLSCREEN
         * in this case as well.  See:
         * https://bugzilla.libsdl.org/show_bug.cgi?id=3315 */
        result = ChangeDisplaySettingsEx(
            devices[device].name, NULL, NULL, CDS_FULLSCREEN, NULL);
    } else {
        result = ChangeDisplaySettingsEx(
            devices[device].name, &device_modes[mode], NULL,
            CDS_FULLSCREEN, NULL);
    }
    if (result != DISP_CHANGE_SUCCESSFUL) {
        DLOG("Failed to change mode: %ld (%s)", (long)result,
             result == DISP_CHANGE_BADDUALVIEW ? "DualView error" :
             result == DISP_CHANGE_BADFLAGS    ? "Invalid flags" :
             result == DISP_CHANGE_BADMODE     ? "Mode not supported" :
             result == DISP_CHANGE_BADPARAM    ? "Invalid parameter" :
             result == DISP_CHANGE_FAILED      ? "Driver error" :
             result == DISP_CHANGE_NOTUPDATED  ? "Failed to update registry" :
             result == DISP_CHANGE_RESTART     ? "Restart required" :
             "Unknown error");
        return 0;
    }

    current_mode = mode;
    return 1;
}

/*-----------------------------------------------------------------------*/

static int should_minimize_fullscreen(void)
{
    if (minimize_fullscreen >= 0) {
        return minimize_fullscreen;
    }

    /* If the SDL hint variable is present, use it to override default
     * behavior. */
    const char *sdl_hint = windows_getenv("SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS");
    if (sdl_hint && *sdl_hint) {
        return strcmp(sdl_hint, "0") != 0 && stricmp(sdl_hint, "false") != 0;
    }

    /* Otherwise, always minimize. */
    return 1;
}

/*-----------------------------------------------------------------------*/

static void update_mouse_grab(void)
{
    int grab = grab_requested;

    /* Don't grab if there's no window visible or the window doesn't have
     * input focus. */
    if (!current_window || window_minimized || !window_focused) {
        grab = 0;
    }

    if (grab && !mouse_grabbed) {
        RECT rect;
        if (UNLIKELY(!GetClientRect(current_window, &rect))) {
            DLOG("GetClientRect() failed: %s",
                 windows_strerror(GetLastError()));
        } else if (UNLIKELY(!ClipCursor(&rect))) {
            DLOG("ClipCursor() failed: %s", windows_strerror(GetLastError()));
        } else {
            mouse_grabbed = 1;
        }
    } else if (!grab && mouse_grabbed) {
        ClipCursor(NULL);
        mouse_grabbed = 0;
    }
}

/*-----------------------------------------------------------------------*/

static GraphicsError init_window_wgl(void)
{
    GraphicsError error = GRAPHICS_ERROR_UNKNOWN;

    if (!set_wgl_pixel_format()) {
        error = GRAPHICS_ERROR_MODE_NOT_SUPPORTED;
        goto error_return;
    }

    HDC dc = GetDC(current_window);
    ASSERT(dc);  // Should never fail.

    wgl_context = create_wgl_context(dc);
    if (UNLIKELY(!wgl_context)) {
        DLOG("Failed to create GL context");
        goto error_release_dc;
    }

    /* OpenGL function pointers in Windows are context-specific, so we
     * have to reinitialize them each time around. */
    if (wgl_has_extension(dc, "WGL_EXT_swap_control")) {
        p_wglSwapIntervalEXT =
            (void *)wglGetProcAddress("wglGetSwapIntervalEXT");
        if (p_wglSwapIntervalEXT) {
            (*p_wglSwapIntervalEXT)(vsync);
        }
    } else {
        p_wglSwapIntervalEXT = NULL;
    }
    opengl_lookup_functions(wglGetProcAddress_wrapper);

    /* Set up OpenGL now that we have a context to work with. */
    if (UNLIKELY(!setup_opengl(window_width, window_height))) {
        error = GRAPHICS_ERROR_BACKEND_TOO_OLD;
        goto error_close_context;
    }

    /* Enable multisampling if requested. */
    if (multisample) {
        glEnable(GL_MULTISAMPLE);
    } else {
        glDisable(GL_MULTISAMPLE);
    }

    /* Don't release the DC handle because we pass it to SwapBuffers().
     * See note in sys_graphics_finish_frame() for details. */
    wgl_context_dc = dc;
    return GRAPHICS_ERROR_SUCCESS;

  error_close_context:
    wglMakeCurrent(dc, NULL);
    wglDeleteContext(wgl_context);
    wgl_context = NULL;
  error_release_dc:
    ReleaseDC(current_window, dc);
  error_return:
    return error;
}

/*-----------------------------------------------------------------------*/

static int set_wgl_pixel_format(void)
{
    PRECOND(current_window != NULL, return 0);

    HDC dc = GetDC(current_window);
    ASSERT(dc);  // Should never fail.

    PIXELFORMATDESCRIPTOR pfd;
    mem_clear(&pfd, sizeof(pfd));
    pfd.nSize           = sizeof(pfd);
    pfd.nVersion        = 1;
    pfd.dwFlags         = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL
                        | PFD_DOUBLEBUFFER
                        | (depth_bits == 0 ? PFD_DEPTH_DONTCARE : 0);
    pfd.iPixelType      = PFD_TYPE_RGBA;
    pfd.cColorBits      = 24;
    pfd.cRedBits        = 8;
    pfd.cGreenBits      = 8;
    pfd.cBlueBits       = 8;
    pfd.cDepthBits      = depth_bits;
    pfd.cStencilBits    = stencil_bits;

    int pixel_format;
    if (!choose_wgl_pixel_format(dc, &pfd, &pixel_format)) {
        /* Fall back to standard Windows lookup. */
        pixel_format = ChoosePixelFormat(dc, &pfd);
        if (!pixel_format) {
            DLOG("Failed to choose a pixel format: %s",
                 windows_strerror(GetLastError()));
            goto error_release_dc;
        }
    }
    if (UNLIKELY(!DescribePixelFormat(dc, pixel_format, sizeof(pfd), &pfd))) {
        DLOG("DescribePixelFormat() failed: %s",
             windows_strerror(GetLastError()));
        goto error_release_dc;
    }
    if (pfd.cRedBits < 8
     || pfd.cGreenBits < 8
     || pfd.cBlueBits < 8
     || pfd.cDepthBits < depth_bits
     || pfd.cStencilBits < stencil_bits) {
        DLOG("No matching pixel formats found");
        goto error_release_dc;
    }

    if (UNLIKELY(!SetPixelFormat(dc, pixel_format, &pfd))) {
        DLOG("SetPixelFormat() failed: %s", windows_strerror(GetLastError()));
        goto error_release_dc;
    }

    ReleaseDC(current_window, dc);
    return 1;

  error_release_dc:
    ReleaseDC(current_window, dc);
    return 0;
}

/*-----------------------------------------------------------------------*/

static int choose_wgl_pixel_format(HDC dc, const PIXELFORMATDESCRIPTOR *pfd,
                                   int *pixel_format_ret)
{
    /*
     * In order to properly choose an OpenGL-compatible pixel format, we
     * have to call wglChoosePixelFormatARB().  But we have to look up
     * that function pointer dynamically, which we can't until we have a
     * GL context -- and we can't do _that_ until we have a window with a
     * pixel format already set.  We also can't change the pixel format
     * once set, so we can't set a dummy format, do the call, and change
     * to the real format.  Is Microsoft deliberately trying to make OpenGL
     * hard to use or something?  At any rate, SDL's solution to this is to
     * create a tiny dummy window and use that to look up the pixel format,
     * and the window never seems to actually get displayed anyway, so
     * we'll borrow that idea.
     *
     * We use a separate window class for the dummy window so as not to
     * pollute the real window procedure with unnecessary events.
     */

    HWND dummy_window = CreateWindow(
        (void *)(uintptr_t)wgl_pixel_format_hack_class,
        window_title, WS_POPUP | WS_DISABLED,
        0, 0, 10, 10, NULL, NULL, GetModuleHandle(NULL), NULL);
    if (UNLIKELY(!dummy_window)) {
        DLOG("Failed to create dummy window");
        goto error_return;
    }
    /* Commit comments in the SDL source suggest that we need to manually
     * pump events here and after closing the window to avoid potential
     * assertion failures with MessageBox(). */
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    HDC dummy_dc = GetDC(dummy_window);
    ASSERT(dummy_dc);
    PIXELFORMATDESCRIPTOR dummy_pfd = *pfd;
    int dummy_format = ChoosePixelFormat(dummy_dc, &dummy_pfd);
    if (!dummy_format) {
        DLOG("Failed to choose a pixel format: %s",
             windows_strerror(GetLastError()));
        goto error_release_dc;
    }
    if (UNLIKELY(!DescribePixelFormat(dummy_dc, dummy_format,
                                      sizeof(dummy_pfd), &dummy_pfd))) {
        DLOG("DescribePixelFormat() failed: %s",
             windows_strerror(GetLastError()));
        goto error_release_dc;
    }
    if (UNLIKELY(!SetPixelFormat(dummy_dc, dummy_format, &dummy_pfd))) {
        DLOG("SetPixelFormat() failed: %s", windows_strerror(GetLastError()));
        goto error_release_dc;
    }

    HGLRC dummy_context = wglCreateContext(dummy_dc);
    if (UNLIKELY(!dummy_context)) {
        DLOG("wglCreateContext() failed: %s",
             windows_strerror(GetLastError()));
        goto error_release_dc;
    }
    if (UNLIKELY(!wglMakeCurrent(dummy_dc, dummy_context))) {
        DLOG("wglMakeCurrent() failed: %s", windows_strerror(GetLastError()));
        goto error_delete_context;
    }

    if (!wgl_has_extension(dummy_dc, "WGL_ARB_pixel_format")) {
        goto error_unbind_context;
    }
    PFNWGLCHOOSEPIXELFORMATARBPROC p_wglChoosePixelFormatARB;
    p_wglChoosePixelFormatARB =
        (void *)wglGetProcAddress("wglChoosePixelFormatARB");
    if (UNLIKELY(!p_wglChoosePixelFormatARB)) {
        DLOG("Driver declares support for wglChoosePixelFormatARB() but"
             " doesn't define it");
        goto error_unbind_context;
    }

    int attributes[13*2+1];
    int num_attrs = 0;
    #define ADD_ATTRIBUTE(name, value)  do { \
        ASSERT(num_attrs+2 < lenof(attributes), goto error_unbind_context); \
        attributes[num_attrs++] = (name);    \
        attributes[num_attrs++] = (value);   \
    } while (0)
    ADD_ATTRIBUTE(WGL_DRAW_TO_WINDOW_ARB, TRUE);
    ADD_ATTRIBUTE(WGL_SUPPORT_OPENGL_ARB, TRUE);
    ADD_ATTRIBUTE(WGL_DOUBLE_BUFFER_ARB,  TRUE);
    ADD_ATTRIBUTE(WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB);
    ADD_ATTRIBUTE(WGL_COLOR_BITS_ARB,     24);
    ADD_ATTRIBUTE(WGL_RED_BITS_ARB,       8);
    ADD_ATTRIBUTE(WGL_GREEN_BITS_ARB,     8);
    ADD_ATTRIBUTE(WGL_BLUE_BITS_ARB,      8);
    ADD_ATTRIBUTE(WGL_DEPTH_BITS_ARB,     depth_bits);
    ADD_ATTRIBUTE(WGL_STENCIL_BITS_ARB,   stencil_bits);
    if (multisample_samples > 1) {
        ADD_ATTRIBUTE(WGL_SAMPLE_BUFFERS_ARB, 1);
        ADD_ATTRIBUTE(WGL_SAMPLES_ARB,        multisample_samples);
    }
    /* According to SDL, some ATI drivers break if we don't explicitly set
     * the ACCELERATION attribute. */
    ADD_ATTRIBUTE(WGL_ACCELERATION_ARB,   WGL_FULL_ACCELERATION_ARB);
    const int accel_attr = num_attrs - 1;
    ASSERT(num_attrs < lenof(attributes), goto error_unbind_context);
    attributes[num_attrs] = 0;

    UINT formats_found = 0;
    if (!(*p_wglChoosePixelFormatARB)(
            dc, attributes, NULL, 1, pixel_format_ret, &formats_found)) {
        DLOG("wglChoosePixelFormatARB() failed");
        formats_found = 0;
    }
    if (!formats_found) {
        /* The call might have failed because of the ACCELERATION attribute,
         * so try again requesting no acceleration. */
        attributes[accel_attr] = WGL_NO_ACCELERATION_ARB;
        if (!(*p_wglChoosePixelFormatARB)(
                dc, attributes, NULL, 1, pixel_format_ret, &formats_found)) {
            DLOG("wglChoosePixelFormatARB() failed (NO_ACCELERATION)");
            formats_found = 0;
        }
    }

    wglMakeCurrent(dummy_dc, NULL);
    wglDeleteContext(dummy_context);
    ReleaseDC(dummy_window, dummy_dc);
    DestroyWindow(dummy_window);
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return formats_found > 0;

  error_unbind_context:
    wglMakeCurrent(dummy_dc, NULL);
  error_delete_context:
    wglDeleteContext(dummy_context);
  error_release_dc:
    ReleaseDC(dummy_window, dummy_dc);
    DestroyWindow(dummy_window);
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

static void close_window(void)
{
    PRECOND(current_window, return);

    if (window_is_d3d) {
        d3d_destroy_context();
    } else {
        ASSERT(wgl_context);
        ASSERT(wgl_context_dc);
        opengl_cleanup();
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(wgl_context);
        wgl_context = NULL;
        ReleaseDC(current_window, wgl_context_dc);
        wgl_context_dc = NULL;
    }

    if (window_thread) {
        PostMessage(current_window, WM_APP, SIL_WM_APP_CLOSE, 0);
        WaitForSingleObject(window_thread, INFINITE);
        CloseHandle(window_thread);
        window_thread = NULL;
        ASSERT(!current_window);
    } else {
        DestroyWindow(current_window);
        current_window = NULL;
    }
    window_focused = 0;

    update_mouse_grab();
    windows_show_mouse_pointer(1);
}

/*-----------------------------------------------------------------------*/

static int setup_opengl(int width, int height)
{
    opengl_enable_debug(use_opengl_debug);

    uint32_t gl_flags = OPENGL_FEATURE_FAST_STATIC_VBO
                      | OPENGL_FEATURE_FAST_DYNAMIC_VBO;
    /* Only use glGenerateMipmap() for drivers claiming to support GL3.0
     * (in which the function became standard), since earlier drivers may
     * have broken implementations. */
    const char *version_string = (const char *)glGetString(GL_VERSION);
    if (version_string && strtoul(version_string, NULL, 10) >= 3) {
        gl_flags |= OPENGL_FEATURE_GENERATEMIPMAP;
    }
    /* We could theoretically use native quads, but all modern GPUs just
     * convert them into triangles anyway, and some OpenGL drivers are
     * broken with respect to quads (e.g. the VMware driver renders three
     * vertices of a QUAD_STRIP as a triangle), so it's not worth it. */

    /* Some Intel drivers (at least build 10.18.15.4256) implement the
     * glProgramUniform functions incorrectly, so that modifying a
     * uniform on a non-current shader makes that shader current.
     * To avoid this, we just disable separate shaders on Intel drivers
     * entirely.  It's low-end hardware anyway, so it's not like users
     * should be expecting good performance in the first place... */
    const char *vendor = (const char *)glGetString(GL_VENDOR);
    if (vendor && strcmp(vendor, "Intel") == 0) {
        gl_flags |= OPENGL_FEATURE_NO_SEPARATE_SHADERS;
    }

    if (UNLIKELY(!opengl_init(width, height, gl_flags))) {
        DLOG("Failed to set up OpenGL!");
        return 0;
    }

    opengl_set_compile_context_callback(create_gl_shader_compilation_context);

    return 1;
}

/*-----------------------------------------------------------------------*/

static int wgl_has_extension(HDC dc, const char *name)
{
    /* wglGetExtensionsString() may appear under a number of different
     * names. */
    PFNWGLGETEXTENSIONSSTRINGARBPROC p_wglGetExtensionsString =
        (void *)wglGetProcAddress("wglGetExtensionsString");
    if (!p_wglGetExtensionsString) {
        p_wglGetExtensionsString =
            (void *)wglGetProcAddress("wglGetExtensionsStringARB");
    }
    if (!p_wglGetExtensionsString) {
        p_wglGetExtensionsString =
            (void *)wglGetProcAddress("wglGetExtensionsStringEXT");
    }
    if (p_wglGetExtensionsString) {
        const char *extensions = (*p_wglGetExtensionsString)(dc);
        if (extensions) {
            const int namelen = strlen(name);
            const char *s = extensions;
            while ((s = strstr(s, name)) != NULL) {
                if ((s == extensions || s[-1] == ' ')
                 && (s[namelen] == 0 || s[namelen] == ' ')) {
                    return 1;
                }
                s += namelen;
                s += strcspn(s, " ");
            }
        }
    }

    /* Alternatively, wglGetExtensionsString() may not exist at all and
     * the extensions may be exported via glGetString[i](). */
    void (GLAPIENTRY *p_glGetIntegerv)(GLenum pname, GLint *data) =
        (void *)GetProcAddress(GetModuleHandle("opengl32.dll"),
                               "glGetIntegerv");
    PFNGLGETSTRINGIPROC p_glGetStringi =
        (void *)wglGetProcAddress("glGetStringi");
    if (p_glGetIntegerv && p_glGetStringi) {
        GLint num_extensions = 0;
        (*p_glGetIntegerv)(GL_NUM_EXTENSIONS, &num_extensions);
        for (int i = 0; i < num_extensions; i++) {
            const char *extension =
                (const char *)(*p_glGetStringi)(GL_EXTENSIONS, i);
            if (extension && strcmp(extension, name) == 0) {
                return 1;
            }
        }
    } else {
        const GLubyte *(GLAPIENTRY *p_glGetString)(GLenum name) =
            (void *)GetProcAddress(GetModuleHandle("opengl32.dll"),
                                   "glGetString");
        if (p_glGetString) {
            const char *extensions =
                (const char *)(*p_glGetString)(GL_EXTENSIONS);
            if (extensions) {
                const int namelen = strlen(name);
                const char *s = extensions;
                while ((s = strstr(s, name)) != NULL) {
                    if ((s == extensions || s[-1] == ' ')
                        && (s[namelen] == 0 || s[namelen] == ' ')) {
                        return 1;
                    }
                    s += namelen;
                    s += strcspn(s, " ");
                }
            }
        } else {
            DLOG("Warning: glGetString() not found");
        }
    }

    return 0;
}

/*-----------------------------------------------------------------------*/

static void *wglGetProcAddress_wrapper(const char *name)
{
    void *function = wglGetProcAddress(name);
    if (!function) {
        /* wglGetProcAddress() does _not_ return function pointers for
         * OpenGL 1.0/1.1 functions (why?!) so try looking them up via
         * the Windows API. */
        function = GetProcAddress(GetModuleHandle("opengl32.dll"), name);
    }
    return function;
}

/*-----------------------------------------------------------------------*/

static HGLRC create_wgl_context(HDC dc)
{
    HGLRC context = 0;
    HGLRC (WINAPI *p_wglCreateContextAttribsARB)(HDC, HGLRC, const int *) =
        (void *)wglGetProcAddress("wglCreateContextAttribsARB");
    if (p_wglCreateContextAttribsARB) {
        int attribs[9];
        int index = 0;
        if (desired_opengl_major >= 3) {
            attribs[index++] = WGL_CONTEXT_MAJOR_VERSION_ARB;
            attribs[index++] = desired_opengl_major;
            attribs[index++] = WGL_CONTEXT_MINOR_VERSION_ARB;
            attribs[index++] = desired_opengl_minor;
            attribs[index++] = WGL_CONTEXT_PROFILE_MASK_ARB;
            attribs[index++] = WGL_CONTEXT_CORE_PROFILE_BIT_ARB;
        }
        if (use_opengl_debug) {
            attribs[index++] = WGL_CONTEXT_FLAGS_ARB;
            attribs[index++] = WGL_CONTEXT_DEBUG_BIT_ARB;
        }
        ASSERT(index < lenof(attribs), return 0);
        attribs[index] = 0;
        context = (*p_wglCreateContextAttribsARB)(dc, 0, attribs);
    } else {
        /* If OpenGL 3.0+ is requested, assume it's not available due to
         * lack of wglCreateContextAttribsARB(). */
        if (desired_opengl_major >= 3) {
            DLOG("wglCreateContextAttribsARB() not available, can't create"
                 " OpenGL %d.%d context", desired_opengl_major,
                 desired_opengl_minor);
            return 0;
        }
        context = wglCreateContext(dc);
    }
    if (UNLIKELY(!context)) {
        DLOG("wglCreateContext() failed: %s",
             windows_strerror(GetLastError()));
        return 0;
    }
    if (UNLIKELY(!wglMakeCurrent(dc, context))) {
        DLOG("wglMakeCurrent() failed: %s", windows_strerror(GetLastError()));
        wglDeleteContext(context);
        return 0;
    }
    return context;
}

/*-----------------------------------------------------------------------*/

static int create_gl_shader_compilation_context(void)
{
    if (wglGetCurrentContext()) {
        return 1;
    }

    if (!current_window) {
        DLOG("No window open, can't create subthread context");
        return 0;
    }
    if (!create_wgl_context(GetDC(current_window))) {
        DLOG("Failed to create subthread context");
        return 0;
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

#ifdef DEBUG

static void describe_mode(int mode, char *desc_buf, int bufsize)
{
    PRECOND(mode >= -1 && mode < graphics_info.num_modes, return);
    PRECOND(desc_buf != NULL, return);
    PRECOND(bufsize > 0, return);

    if (mode == -1) {
        const int device = current_window ? current_device : device_to_use;
        mode = devices[device].default_mode;
        strformat(desc_buf, bufsize, "default ");
    } else {
        *desc_buf = 0;
    }

    strformat(desc_buf+strlen(desc_buf), bufsize-strlen(desc_buf),
              "mode %d (%dx%dx%d+%d,%d @ %dHz)",
              mode,
              (int)device_modes[mode].dmPelsWidth,
              (int)device_modes[mode].dmPelsHeight,
              (int)device_modes[mode].dmBitsPerPel,
              (int)device_modes[mode].dmPosition.x,
              (int)device_modes[mode].dmPosition.y,
              (int)device_modes[mode].dmDisplayFrequency);
}

#endif  // DEBUG

/*************************************************************************/
/*************************************************************************/
