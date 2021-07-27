/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/android/graphics.c: Graphics and rendering functionality for
 * Android.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/endian.h"
#include "src/graphics.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/android/internal.h"
#include "src/sysdep/opengl/opengl.h"
#include "src/thread.h"

#include <dlfcn.h>
#include <EGL/egl.h>

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Graphics capability structure returned to high-level code.  We get the
 * actual display size at init time and return that as the only supported
 * mode. */

static GraphicsDisplayModeEntry display_modes[1];

static SysGraphicsInfo graphics_info = {
    .has_windowed_mode = 0,
    .num_devices = 1,
    .num_modes = 1,
    .modes = display_modes,
};

/*-----------------------------------------------------------------------*/

/* Cached Java method IDs. */
static jmethodID getDisplayWidth, getDisplayHeight, getDisplayFullWidth;
static jmethodID getDisplayFullHeight, getDisplaySizeInches;

/* Have we been initialized? */
static uint8_t initted;

/* Are we currently suspended (via android_suspend_graphics())? */
static uint8_t suspended;

/* GL display objects. */
static EGLDisplay display;
static EGLConfig config;
static EGLSurface surface = EGL_NO_SURFACE;
static EGLContext context = EGL_NO_CONTEXT;

/* Display size and pixel format. */
static int display_width, display_height;
static EGLint display_format;

/* Requested depth and stencil buffer resolution. */
static int depth_bits;
static int stencil_bits;

/* Requested sample count for multisampling (1 = multisampling disabled). */
static int multisample_samples = 1;

/* Flag: sync to the vertical retrace on each frame? */
static uint8_t vsync;

/* Frame interval for display. */
static int frame_interval;

/* Requested OpenGL version (0 if not set). */
static int desired_opengl_major, desired_opengl_minor;

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * select_egl_config:  Select an appropriate EGL configuration for the
 * current display attributes.
 *
 * Note that the configuration is returned via a pointer rather than as
 * the function's return value because zero is a valid EGLConfig value.
 *
 * [Parameters]
 *     config_ret: Pointer to variable to receive the EGL configuration.
 * [Return value]
 *     True on success, false on error.
 */
static int select_egl_config(EGLConfig *config_ret);

/**
 * create_egl_context:  Create a new EGL context and make it the current
 * context.
 *
 * [Return value]
 *     Created context, or EGL_NO_CONTEXT on error.
 */
static EGLContext create_egl_context(void);

/**
 * create_gl_shader_compilation_context:  Create and make current a new
 * EGL context for the current thread which can be used to compile shaders.
 *
 * [Return value]
 *     True on success or if the current thread already has a GL context,
 *     false on error.
 */
static int create_gl_shader_compilation_context(void);

/**
 * android_eglGetProcAddress:  Wrapper for eglGetProcAddress() which
 * preferentially looks up the symbol via dlsym(), as a workaround for
 * EGL libraries which don't return pointers for core GL functions.
 *
 * [Parameters]
 *     name: Name of function to look up.
 * [Return value]
 *     Function pointer, or NULL if the function is not found.
 */
static void *android_eglGetProcAddress(const char *name);

/*************************************************************************/
/***************** Interface: Basic graphics operations ******************/
/*************************************************************************/

const SysGraphicsInfo *sys_graphics_init(void)
{
    PRECOND(!initted, return NULL);

    getDisplayWidth = get_method(0, "getDisplayWidth", "()I");
    getDisplayHeight = get_method(0, "getDisplayHeight", "()I");
    getDisplayFullWidth = get_method(0, "getDisplayFullWidth", "()I");
    getDisplayFullHeight = get_method(0, "getDisplayFullHeight", "()I");
    getDisplaySizeInches = get_method(0, "getDisplaySizeInches", "()F");
    ASSERT(getDisplayWidth != 0, return NULL);
    ASSERT(getDisplayHeight != 0, return NULL);
    ASSERT(getDisplayFullWidth != 0, return NULL);
    ASSERT(getDisplayFullHeight != 0, return NULL);
    ASSERT(getDisplaySizeInches != 0, return NULL);

    display_modes[0].device = 0;
    display_modes[0].device_name = NULL;
    display_modes[0].width = android_display_width();
    display_modes[0].height = android_display_height();
    display_modes[0].refresh = 0;
    graphics_info.num_modes = 1;

    /* Set up EGL (making sure the UI thread doesn't get in our way). */
    android_lock_ui_thread();
    {
        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (!eglInitialize(display, NULL, NULL)) {
            DLOG("eglInitialize() failed: %d", eglGetError());
            android_unlock_ui_thread();
            return NULL;
        }
    }
    android_unlock_ui_thread();

    android_toggle_navigation_bar(0);

    depth_bits = 16;
    desired_opengl_major = 0;
    desired_opengl_minor = 0;
    stencil_bits = 0;
    multisample_samples = 1;
    vsync = 1;
    frame_interval = 1;

    initted = 1;
    suspended = 0;
    return &graphics_info;
}

/*-----------------------------------------------------------------------*/

void sys_graphics_cleanup(void)
{
    PRECOND(initted, return);
    PRECOND(!suspended, return);

    android_lock_ui_thread();
    {
        if (context) {
            opengl_cleanup();
            eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                           EGL_NO_CONTEXT);
            eglDestroyContext(display, context);
            context = 0;
        }
        if (surface) {
            eglDestroySurface(display, surface);
            surface = 0;
        }
        eglTerminate(display);
        display = 0;
    }
    android_unlock_ui_thread();

    initted = 0;
}

/*-----------------------------------------------------------------------*/

int sys_graphics_device_width(void)
{
    return display_modes[0].width;
}

/*-----------------------------------------------------------------------*/

int sys_graphics_device_height(void)
{
    return display_modes[0].height;
}

/*-----------------------------------------------------------------------*/

int sys_graphics_set_display_attr(const char *name, va_list args)
{
    if (strcmp(name, "center_window") == 0) {
        return 1;  // Meaningless on Android.
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
        return (value == 0);
    }

    if (stricmp(name, "frame_interval") == 0) {
        const int new_frame_interval = va_arg(args, int);
        if (new_frame_interval <= 0) {
            DLOG("Invalid frame interval: %d", new_frame_interval);
            return 0;
        }
        frame_interval = new_frame_interval;
        eglSwapInterval(display, vsync ? frame_interval : 0);
        return 1;
    }

    if (strcmp(name, "multisample") == 0) {
        const int samples = va_arg(args, int);
        if (samples <= 0) {
            DLOG("Invalid value for attribute %s: %d", name, samples);
            return 0;
        }
        multisample_samples = samples;
        return 1;
    }

    if (strcmp(name, "opengl_debug") == 0) {
        opengl_enable_debug(va_arg(args, int) != 0);
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
        vsync = va_arg(args, int);
        eglSwapInterval(display, vsync ? frame_interval : 0);
        return 1;
    }

    if (strcmp(name, "window") == 0) {
        const int window = va_arg(args, int);
        return (window == 0);  // No windows on Android.
    }

    if (strcmp(name, "window_resizable") == 0) {
        return 1;  // No windows on Android.
    }

    return 0;
}

/*-----------------------------------------------------------------------*/

GraphicsError sys_graphics_set_display_mode(int width, int height)
{
    PRECOND(!suspended, return GRAPHICS_ERROR_UNKNOWN);

    GraphicsError error = GRAPHICS_ERROR_UNKNOWN;

    const int max_width = display_modes[graphics_info.num_modes-1].width;
    const int max_height = display_modes[graphics_info.num_modes-1].height;
    if (width <= 0 || width > max_width
     || height <= 0 || height > max_height) {
        DLOG("Invalid/unsupported size: %dx%d (maximum supported: %dx%d)",
             width, height, max_width, max_height);
        error = GRAPHICS_ERROR_MODE_NOT_SUPPORTED;
        goto error_return;
    }

    if (UNLIKELY(!select_egl_config(&config))) {
        error = GRAPHICS_ERROR_MODE_NOT_SUPPORTED;
        goto error_return;
    }
    if (UNLIKELY(!eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID,
                                     &display_format))) {
        DLOG("Failed to get visual format for configuration %p: %d",
             config, eglGetError());
        goto error_return;
    }

    int window_w = width, window_h = height;
    if (window_w == display_modes[0].width
     && window_h == display_modes[0].height) {
        /* If running at native resolution, set the setBuffersGeometry()
         * size parameters to zero to disable scaling. */
        window_w = window_h = 0;
    }

    android_lock_ui_thread();
    {
        if (context != EGL_NO_CONTEXT) {
            opengl_cleanup();
            eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                           EGL_NO_CONTEXT);
            eglDestroyContext(display, context);
            context = EGL_NO_CONTEXT;
        }
        if (surface != EGL_NO_SURFACE) {
            eglDestroySurface(display, surface);
            surface = EGL_NO_SURFACE;
        }
        const int android_error = ANativeWindow_setBuffersGeometry(
            android_window, window_w, window_h, display_format);
        if (UNLIKELY(android_error != 0)) {
            DLOG("ANativeWindow_setBuffersGeometry(%d,%d,%d) failed: %d",
                 window_w, window_h, display_format, error);
            android_unlock_ui_thread();
            goto error_return;
        }

        surface = eglCreateWindowSurface(display, config, android_window, NULL);
        if (UNLIKELY(surface == EGL_NO_SURFACE)) {
            DLOG("eglCreateWindowSurface() failed: %d", eglGetError());
            android_unlock_ui_thread();
            goto error_return;
        }
        eglQuerySurface(display, surface, EGL_WIDTH, &display_width);
        eglQuerySurface(display, surface, EGL_HEIGHT, &display_height);
        if (display_width != width || display_height != height) {
            /* Some devices (such as the 1st-generation Kindle Fire) report
             * a size of 1x1 here instead of the actual surface size, so in
             * that case, blindly assume we got what we asked for. */
            if (display_width == 1 && display_height == 1) {
                DLOG("WARNING: Device reported a display size of 1x1 pixel!"
                     "  Assuming we got %dx%d as requested.", width, height);
                display_width = width;
                display_height = height;
            } else {
                DLOG("Failed to get requested size %dx%d (got %dx%d instead)",
                     width, height, display_width, display_height);
                error = GRAPHICS_ERROR_MODE_NOT_SUPPORTED;
                android_unlock_ui_thread();
                goto error_destroy_surface;
            }
        }
    }
    android_unlock_ui_thread();

    context = create_egl_context();  // Calls opengl_lookup_functions().
    if (context == EGL_NO_CONTEXT) {
        DLOG("Failed to create EGL context!");
        goto error_destroy_surface;
    }

    if (!opengl_init(display_width, display_height,
                     OPENGL_FEATURE_DELAYED_DELETE
                   | OPENGL_FEATURE_FAST_STATIC_VBO
                   | OPENGL_FEATURE_FRAMEBUFFERS
                   | OPENGL_FEATURE_GENERATEMIPMAP)) {
        DLOG("Failed to set up OpenGL");
        error = GRAPHICS_ERROR_BACKEND_TOO_OLD;
        goto error_clear_current;
    }

    /* Many (all?) Android devices don't seem to support multiple EGL
     * contexts.  Try creating a second context now, and if it fails,
     * don't enable background shader compilation. */
    static uint8_t checked_second_context = 0;
    static uint8_t second_context_ok = 0;
    if (!checked_second_context) {
        checked_second_context = 1;
        DLOG("Checking background shader compilation...");
        const int thread = thread_create(
            (ThreadFunction *)create_gl_shader_compilation_context, NULL);
        if (thread) {
            second_context_ok = thread_wait(thread);
        }
        DLOG("...background shader compilation %savailable",
             second_context_ok ? "" : "NOT ");
    }
    if (second_context_ok) {
        opengl_set_compile_context_callback(
            create_gl_shader_compilation_context);
    }

    eglSwapInterval(display, vsync ? frame_interval : 0);

    return GRAPHICS_ERROR_SUCCESS;

  error_clear_current:
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  error_destroy_surface:
    eglDestroySurface(display, surface);
    surface = 0;
  error_return:
    return error;
}

/*-----------------------------------------------------------------------*/

int sys_graphics_display_is_window(void)
{
    return 0;  // No windows on Android.
}

/*-----------------------------------------------------------------------*/

void sys_graphics_set_window_title(UNUSED const char *title)
{
    /* Nothing to do. */
}

/*-----------------------------------------------------------------------*/

void sys_graphics_set_window_icon(UNUSED SysTexture *texture)
{
    /* Nothing to do. */
}

/*-----------------------------------------------------------------------*/

void sys_graphics_set_window_resize_limits(
    UNUSED int min_width, UNUSED int min_height,
    UNUSED int max_width, UNUSED int max_height,
    UNUSED int min_aspect_x, UNUSED int min_aspect_y,
    UNUSED int max_aspect_x, UNUSED int max_aspect_y)
{
    /* Nothing to do. */
}

/*-----------------------------------------------------------------------*/

void sys_graphics_show_mouse_pointer(UNUSED int on)
{
    /* Nothing to do -- Android has a mouse pointer but doesn't allow it
     * to be disabled programmatically. */
}

/*-----------------------------------------------------------------------*/

int sys_graphics_get_mouse_pointer_state(void)
{
    return 1;  // Always displayed when a mouse is in use.
}

/*-----------------------------------------------------------------------*/

void sys_graphics_get_frame_period(int *numerator_ret, int *denominator_ret)
{
    *numerator_ret = 1001 * (vsync ? frame_interval : 0);
    *denominator_ret = 60000;
}

/*-----------------------------------------------------------------------*/

int sys_graphics_has_focus(void)
{
    /* There's no concept of "focus" on Android, so always return true. */
    return 1;
}

/*-----------------------------------------------------------------------*/

void sys_graphics_start_frame(int *width_ret, int *height_ret)
{
    PRECOND(!suspended, return);

    *width_ret = display_width;
    *height_ret = display_height;

    if (context) {
        opengl_start_frame();
        opengl_free_dead_resources(0);
    }
}

/*-----------------------------------------------------------------------*/

void sys_graphics_finish_frame(void)
{
    PRECOND(!suspended, return);

    if (context) {
        eglSwapBuffers(display, surface);
    }
}

/*-----------------------------------------------------------------------*/

void sys_graphics_sync(int flush)
{
    if (context) {
        opengl_sync();
        if (flush) {
            opengl_free_dead_resources(1);
        }
    }
}

/*************************************************************************/
/*********************** Exported utility routines ***********************/
/*************************************************************************/

int android_display_width(void)
{
    const int has_immersive = (android_api_level >= 19);

    /* Note that we have to do this through Java because ANativeWindow
     * returns 1x1 for some devices and/or Android versions. */
    JNIEnv *env = get_jni_env();
    jobject activity_obj = android_activity->clazz;
    jmethodID getWidth;
    if (has_immersive) {
        if (UNLIKELY(!getDisplayFullWidth)) {  // Might be called before init.
            getDisplayFullWidth = get_method(0, "getDisplayFullWidth", "()I");
            ASSERT(getDisplayFullWidth != 0, goto error);
        }
        getWidth = getDisplayFullWidth;
    } else {
        if (UNLIKELY(!getDisplayWidth)) {
            getDisplayWidth = get_method(0, "getDisplayWidth", "()I");
            ASSERT(getDisplayWidth != 0, goto error);
        }
        getWidth = getDisplayWidth;
    }
    int width = (*env)->CallIntMethod(env, activity_obj, getWidth);
    ASSERT(!clear_exceptions(env), goto error);
    ASSERT(width > 0, goto error);
    return width;

  error:
    return 640;
}

/*-----------------------------------------------------------------------*/

int android_display_height(void)
{
    const int has_immersive = (android_api_level >= 19);

    JNIEnv *env = get_jni_env();
    jobject activity_obj = android_activity->clazz;
    jmethodID getHeight;
    if (has_immersive) {
        if (UNLIKELY(!getDisplayFullHeight)) {
            getDisplayFullHeight = get_method(0, "getDisplayFullHeight", "()I");
            ASSERT(getDisplayFullHeight != 0, goto error);
        }
        getHeight = getDisplayFullHeight;
    } else {
        if (UNLIKELY(!getDisplayHeight)) {
            getDisplayHeight = get_method(0, "getDisplayHeight", "()I");
            ASSERT(getDisplayHeight != 0, goto error);
        }
        getHeight = getDisplayHeight;
    }
    int height = (*env)->CallIntMethod(env, activity_obj, getHeight);
    ASSERT(!clear_exceptions(env), goto error);
    ASSERT(height > 0, goto error);
    return height;

  error:
    return android_display_width()*9/16;
}

/*-----------------------------------------------------------------------*/

int android_using_immersive_mode(void)
{
    return android_api_level >= 19;
}

/*-----------------------------------------------------------------------*/

float android_display_size_inches(void)
{
    JNIEnv *env = get_jni_env();
    jobject activity_obj = android_activity->clazz;
    if (UNLIKELY(!getDisplaySizeInches)) {  // Might be called before init.
        getDisplaySizeInches = get_method(0, "getDisplaySizeInches", "()F");
        ASSERT(getDisplaySizeInches != 0, goto error);
    }
    float size = (*env)->CallFloatMethod(
        env, activity_obj, getDisplaySizeInches);
    ASSERT(!clear_exceptions(env), goto error);
    ASSERT(size > 0, goto error);
    return size;

  error:
    return 10.0;
}

/*-----------------------------------------------------------------------*/

void android_suspend_graphics(void)
{
    if (!initted || !context) {
        return;
    }

    ASSERT(!suspended, return);

    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(display, surface);
    surface = 0;

    suspended = 1;
}

/*-----------------------------------------------------------------------*/

void android_resume_graphics(void)
{
    if (!initted || !suspended) {
        return;
    }

    ASSERT(android_window != NULL, return);

    ASSERT(ANativeWindow_setBuffersGeometry(
               android_window, display_width, display_height, display_format)
           == 0);
    surface = eglCreateWindowSurface(display, config, android_window, NULL);
    ASSERT(surface != EGL_NO_SURFACE);
    ASSERT(eglMakeCurrent(display, surface, surface, context));

    suspended = 0;
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static int select_egl_config(EGLConfig *config_ret)
{
    PRECOND(config_ret != NULL, return 0);

    static EGLint attributes[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_DEPTH_SIZE, 16,
        EGL_STENCIL_SIZE, 0,
        EGL_SAMPLE_BUFFERS, 0,
        EGL_SAMPLES, 0,
        EGL_NONE
    };

    for (int i = 0; attributes[i] != EGL_NONE; i += 2) {
        ASSERT(i+1 < lenof(attributes), break);
        if (attributes[i] == EGL_DEPTH_SIZE) {
            attributes[i+1] = depth_bits;
        } else if (attributes[i] == EGL_STENCIL_SIZE) {
            attributes[i+1] = stencil_bits;
        } else if (attributes[i] == EGL_SAMPLE_BUFFERS) {
            attributes[i+1] = (multisample_samples > 1);
        } else if (attributes[i] == EGL_SAMPLES) {
            attributes[i+1] =
                (multisample_samples > 1 ? multisample_samples : 0);
        }
    }

    EGLConfig configs[100];
    EGLint config_count = 0;
    eglChooseConfig(display, attributes, configs, lenof(configs),
                    &config_count);
    if (UNLIKELY(config_count < 1)) {
        DLOG("No valid EGL configurations found!");
        return 0;
    }

#ifdef DEBUG
    for (int i = 0; i < config_count; i++) {
        EGLint config_id = 0, surface_type = -1, renderable_type = -1;
        EGLint r_size = -1, g_size = -1, b_size = -1, depth_size = -1;
        EGLint stencil_size = -1, sample_buffers = -1, samples = -1;
        EGLint caveat = -1;
        eglGetConfigAttrib(display, configs[i], EGL_CONFIG_ID,
                           &config_id);
        eglGetConfigAttrib(display, configs[i], EGL_SURFACE_TYPE,
                           &surface_type);
        eglGetConfigAttrib(display, configs[i], EGL_RENDERABLE_TYPE,
                           &renderable_type);
        eglGetConfigAttrib(display, configs[i], EGL_RED_SIZE, &r_size);
        eglGetConfigAttrib(display, configs[i], EGL_GREEN_SIZE, &g_size);
        eglGetConfigAttrib(display, configs[i], EGL_BLUE_SIZE, &b_size);
        eglGetConfigAttrib(display, configs[i], EGL_DEPTH_SIZE, &depth_size);
        eglGetConfigAttrib(display, configs[i], EGL_STENCIL_SIZE,
                           &stencil_size);
        eglGetConfigAttrib(display, configs[i], EGL_SAMPLE_BUFFERS,
                           &sample_buffers);
        eglGetConfigAttrib(display, configs[i], EGL_SAMPLES, &samples);
        eglGetConfigAttrib(display, configs[i], EGL_CONFIG_CAVEAT, &caveat);
        DLOG("Configuration %d (%p):\n"
             "                 ID: %d\n"
             "       Surface type: 0x%X\n"
             "    Renderable type: 0x%X -%s%s%s%s%s\n"
             "     Red/green/blue: %d/%d/%d bits\n"
             "       Depth buffer: %d bits\n"
             "     Stencil buffer: %d bits\n"
             "     Sample buffers: %d (%d samples)\n"
             "             Caveat: 0x%X (%s)",
             i, configs[i], config_id, surface_type, renderable_type,
             renderable_type==0 ? "(None)" : "",
             renderable_type & EGL_OPENGL_ES_BIT ? " GLESv1" : "",
             renderable_type & EGL_OPENVG_BIT ? " VG" : "",
             renderable_type & EGL_OPENGL_ES2_BIT ? " GLESv2" : "",
             renderable_type & EGL_OPENGL_BIT ? " GL" : "",
             r_size, g_size, b_size, depth_size, stencil_size,
             sample_buffers, samples, caveat,
             (caveat==EGL_SLOW_CONFIG ? "Slow" :
              caveat==EGL_NON_CONFORMANT_CONFIG ? "Non-conformant" :
              caveat==EGL_NONE ? "No caveat" : "???"));
    }
#endif

    *config_ret = configs[0];
    return 1;
}

/*-----------------------------------------------------------------------*/

static EGLContext create_egl_context(void)
{
    EGLint attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };
    EGLContext this_context = eglCreateContext(display, config, NULL, attribs);
    if (this_context == EGL_NO_CONTEXT) {
        attribs[1] = 2;
        this_context = eglCreateContext(display, config, NULL, attribs);
    }
    if (UNLIKELY(this_context == EGL_NO_CONTEXT)) {
        DLOG("eglCreateContext() failed: %d", eglGetError());
        return EGL_NO_CONTEXT;
    }

    if (UNLIKELY(!eglMakeCurrent(display, surface, surface, this_context))) {
        DLOG("Failed to activate EGL context!");
        eglDestroyContext(display, this_context);
        return EGL_NO_CONTEXT;
    }
    opengl_lookup_functions(android_eglGetProcAddress);

    opengl_get_version();
    if (!opengl_version_is_at_least(desired_opengl_major,
                                    desired_opengl_minor)) {
        DLOG("EGL context version %d.%d < requested version %d.%d",
             opengl_major_version(), opengl_minor_version(),
             desired_opengl_major, desired_opengl_minor);
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(display, this_context);
        return EGL_NO_CONTEXT;
    }

    return this_context;
}

/*-----------------------------------------------------------------------*/

static int create_gl_shader_compilation_context(void)
{
    if (eglGetCurrentContext() != EGL_NO_CONTEXT) {
        return 1;
    }

    if (context == EGL_NO_CONTEXT) {
        DLOG("No main rendering context, can't create subthread context");
        return 0;
    }
    EGLContext sub_context = create_egl_context();
    if (sub_context == EGL_NO_CONTEXT) {
        DLOG("Failed to create subthread context");
        return 0;
    }

    /* Destroy the context now so we don't leak it when the thread exits. */
    eglDestroyContext(display, sub_context);

    return 1;
}

/*-----------------------------------------------------------------------*/

static void *android_eglGetProcAddress(const char *name)
{
    void *function = dlsym(RTLD_DEFAULT, name);
    if (!function) {
        DLOG("Failed to look up %s via dlsym(): %s", name, dlerror());
        function = eglGetProcAddress(name);
    }
    return function;
}

/*************************************************************************/
/*************************************************************************/
