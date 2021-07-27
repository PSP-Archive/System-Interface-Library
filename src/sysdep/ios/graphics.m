/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/ios/graphics.m: Graphics and rendering functionality for iOS.
 */

#define IN_SYSDEP

#import "src/base.h"
#import "src/endian.h"
#import "src/graphics.h"
#import "src/math.h"
#import "src/memory.h"
#import "src/sysdep.h"
#import "src/sysdep/ios/util.h"
#import "src/sysdep/ios/view.h"
#import "src/sysdep/opengl/opengl.h"
#import "src/thread.h"
#import "src/utility/tex-file.h"

#import <OpenGLES/EAGL.h>

/*
 * Miscellaneous notes about iPad graphics processing:
 *
 * - Mipmaps are _extremely_ important on early-generation iPad devices;
 *   when trying to display a texture at less than half size (even in
 *   GL_NEAREST mode), the iPad 1 GPU seems to fall into a slow mode in
 *   which the texture fill rate drops by almost two orders of magnitude.
 *   The drop is not quite as severe (though still significant) on the
 *   iPad 2.
 *
 * - Outside of the above case, the GPU seems to perform consistently for
 *   textures of up to 512x512x32bpp.  Observed texture fill rates on a
 *   600x600-pixel quad (1 trial), calculated as
 *   1 / (time per textured pixel - time per untextured pixel):
 *           Mtexels/sec     | 256x256 | 512x512 | 1024x1024 |
 *       --------------------+---------+---------+-----------+
 *        iPad 1, GL_NEAREST |  1863.3 |  1840.8 |     139.4 |
 *        iPad 1, GL_LINEAR  |  1848.0 |  1844.8 |     140.4 |
 *        iPad 2, GL_NEAREST |  7775.0 |  6980.0 |    2493.2 |
 *        iPad 2, GL_LINEAR  |  7972.5 |  6834.0 |    2001.9 |
 *
 * - As shown above, the iPad 1 exhibits no significant difference in
 *   performance between GL_LINEAR and GL_NEAREST at any texture size.
 *   On the iPad 2, GL_NEAREST is somewhat faster if the GPU has to drop
 *   out of its fastest mode.  Mipmaps (GL_LINEAR_MIPMAP_LINEAR) reduce
 *   the texture fill rate by a factor of 4-5 for the iPad 1, 10-15 for
 *   the iPad 2.
 */

/*************************************************************************/
/************************* Configuration options *************************/
/*************************************************************************/

/**
 * VSYNC_SEMAPHORES:  If defined, the vertical sync logic will create a
 * single, permanent vertical-sync thread and use semaphores for
 * interthread synchronization.  If not defined, a new thread will be
 * created for each vertical-sync operation, and synchronization will be
 * performed by waiting for the thread to terminate.
 *
 * As of iOS 4.2.1, the semaphore method seems to be about 4x as fast as
 * the thread create/join method (~250 vs. ~950 usec/frame).  250 usec for
 * a mere two wait/signal operations is still awfully long, but what can
 * you do...
 */
#define VSYNC_SEMAPHORES

/**
 * VSYNC_THREAD_PRIORITY:  Specifies the priority (relative to the main
 * thread) for the vertical sync thread.  Valid values are -16 through +16.
 * The vertical sync thread should typically be given a high priority so
 * the glFinish() call is executed as soon as possible.
 */
#define VSYNC_THREAD_PRIORITY  10

/*************************************************************************/
/****************************** Global data ******************************/
/*************************************************************************/

/* Allow autorotation? */
uint8_t ios_allow_autorotation = 1;

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Graphics capability structure returned to high-level code.  We get the
 * actual display size at init time and return that as the default mode,
 * adding an extra mode for high-refresh-rate displays. */

static GraphicsDisplayModeEntry display_modes[2];

static SysGraphicsInfo graphics_info = {
    .has_windowed_mode = 0,
    .num_devices = 1,
    .num_modes = 1,
    .modes = display_modes,
};

/*-----------------------------------------------------------------------*/

/* Have we been initialized? */
static uint8_t initted;

/* Frame counter at the beginning of the current frame. */
static int last_frame_counter;

/* Thread ID for the vertical sync thread. */
static int vsync_thread_id;

#ifdef VSYNC_SEMAPHORES
/* Semaphores for vertical sync control.  Semaphore 0 is set by the main
 * thread to release the vertical sync thread, and vice versa for
 * semaphore 1. */
static SysSemaphoreID vsync_sem[2];
/* Does the main thread currently hold its semaphore? */
static uint8_t main_thread_running;
/* Set by the main thread to tell the vertical sync thread to exit. */
static uint8_t vsync_thread_stop;
#endif

/* Requested depth and stencil buffer resolution. */
static int depth_bits;
static int stencil_bits;

/* Requested refresh rate. */
static float desired_refresh_rate;

/* Requested OpenGL version (0 if not set). */
static int desired_opengl_major, desired_opengl_minor;

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

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
 * wait_for_vsync:  Wait until the vertical sync thread releases GPU
 * control back to the main thread.
 */
static void wait_for_vsync(void);

/**
 * vsync_thread:  Thread routine used to wait for the next vertical sync
 * event after the GPU finishes processing.  In semaphore mode, this loops
 * indefinitely, synchronizing with the main thread.
 *
 * [Parameters]
 *     unused: Thread parameter (unused).
 * [Return value]
 *     0
 */
static int vsync_thread(void *unused);

/*************************************************************************/
/***************** Interface: Basic graphics operations ******************/
/*************************************************************************/

const SysGraphicsInfo *sys_graphics_init(void)
{
    if (UNLIKELY(initted)) {
        DLOG("Already initialized!");
        goto error_return;
    }

    /* Get the size of the screen.  This is fixed depending on the
     * particular device. */
    graphics_info.num_modes = 1;
    display_modes[0].device = 0;
    display_modes[0].device_name = NULL;
    display_modes[0].width = ios_display_width();
    display_modes[0].height = ios_display_height();

    /* If the device has a higher refresh rate than the default of 60,
     * add that refresh rate as a second mode. */
    const int native_fps = ios_get_native_refresh_rate();
    display_modes[0].refresh = ubound(native_fps, 60);
    if (native_fps > 60) {
        graphics_info.num_modes = 2;
        display_modes[1] = display_modes[0];
        display_modes[1].refresh = native_fps;
    }

    /*
     * Set up the common OpenGL library, with appropriate quirks:
     *
     * - We avoid glGenerateMipmap() because its performance is insanely
     *   slow on some devices, even with the FASTEST hint.
     *
     * - We avoid vertex array objects if possible (i.e., when using GLES
     *   2.0 rather than 3.0 or later) because for the comparatively simple
     *   shaders used by SIL, VAOs are actually slower than directly
     *   configuring the vertex arrays on each render call.
     *
     * - We disable integer vertex attributes because attempting to use
     *   them causes internal shader compiler errors on at least some iOS
     *   versions/devices (observed with iOS 10 on an iPad Pro).
     */
    uint32_t features = OPENGL_FEATURE_BROKEN_ATTRIB_INT
                      | OPENGL_FEATURE_DELAYED_DELETE
                      | OPENGL_FEATURE_FAST_STATIC_VBO
                      | OPENGL_FEATURE_FRAMEBUFFERS;
    /* iOS 8.[012] had a bug in the OpenGL library which failed to drop
     * the alpha channel from a framebuffer with alpha.  rdar://18478863 */
    if (ios_version_is_at_least("8.0") && !ios_version_is_at_least("8.3")) {
        features |= OPENGL_FEATURE_BROKEN_COPYTEXIMAGE;
    }
    if (!opengl_init(ios_display_width(), ios_display_height(), features)) {
        DLOG("Failed to set up OpenGL");
        goto error_return;
    }
    opengl_set_compile_context_callback(create_gl_shader_compilation_context);

#ifdef VSYNC_SEMAPHORES
    /* Create the vertical sync semaphores and start the thread running. */
    vsync_sem[0] = sys_semaphore_create(0, 1);
    if (UNLIKELY(!vsync_sem[0])) {
        DLOG("Failed to create vsync semaphore 0");
        goto error_cleanup_opengl;
    }
    vsync_sem[1] = sys_semaphore_create(1, 1);
    if (UNLIKELY(!vsync_sem[1])) {
        DLOG("Failed to create vsync semaphore 1");
        goto error_destroy_semaphore_0;
    }
    vsync_thread_stop = 0;
    vsync_thread_id = thread_create_with_priority(VSYNC_THREAD_PRIORITY,
                                                  vsync_thread, NULL);
    if (UNLIKELY(!vsync_thread_id)) {
        DLOG("Failed to create vsync thread");
        goto error_destroy_semaphores;
    }
    main_thread_running = 0;
#else  // !VSYNC_SEMAPHORES
    vsync_thread_id = 0;
#endif

    ios_set_frame_interval(1);

    depth_bits = 16;
    desired_opengl_major = 0;
    desired_opengl_minor = 0;
    desired_refresh_rate = 60;
    stencil_bits = 0;
    initted = 1;
    return &graphics_info;

#ifdef VSYNC_SEMAPHORES
  error_destroy_semaphores:
    sys_semaphore_destroy(vsync_sem[1]);
    vsync_sem[1] = 0;
  error_destroy_semaphore_0:
    sys_semaphore_destroy(vsync_sem[0]);
    vsync_sem[0] = 0;
  error_cleanup_opengl:
    opengl_cleanup();
#endif
  error_return:
    return NULL;
}

/*-----------------------------------------------------------------------*/

void sys_graphics_cleanup(void)
{
    PRECOND(initted, return);

    /* Make sure we've done at least one present() call to get the timer
     * running (the main thread blocks until we present() once). */
    ios_present_view();

#ifdef VSYNC_SEMAPHORES
    vsync_thread_stop = 1;
    sys_semaphore_signal(vsync_sem[0]);
    thread_wait(vsync_thread_id);
    vsync_thread_id = 0;
    sys_semaphore_destroy(vsync_sem[1]);
    sys_semaphore_destroy(vsync_sem[0]);
    vsync_sem[0] = vsync_sem[1] = 0;
#endif

    opengl_cleanup();

    initted = 0;
}

/*-----------------------------------------------------------------------*/

int sys_graphics_device_width(void)
{
    return ios_display_width();
}

/*-----------------------------------------------------------------------*/

int sys_graphics_device_height(void)
{
    return ios_display_height();
}

/*-----------------------------------------------------------------------*/

int sys_graphics_set_display_attr(const char *name, va_list args)
{
    if (strcmp(name, "center_window") == 0) {
        return 1;  // Meaningless on iOS.
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

    if (strcmp(name, "frame_interval") == 0) {
        const int new_frame_interval = va_arg(args, int);
        if (new_frame_interval <= 0) {
            DLOG("Invalid frame interval: %d", new_frame_interval);
        } else {
            ios_set_frame_interval(new_frame_interval);
        }
        return 1;
    }

    if (strcmp(name, "multisample") == 0) {
        const int samples = va_arg(args, int);
        if (samples <= 0) {
            DLOG("Invalid value for attribute %s: %d", name, samples);
            return 0;
        }
        return samples == 1;  // iOS doesn't support transparent multisampling.
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
        desired_refresh_rate = value;
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
        const int vsync = va_arg(args, int);
        return (vsync != 0);  // Vsync is required.
    }

    if (strcmp(name, "window") == 0) {
        const int window = va_arg(args, int);
        return (window == 0);  // No windows on iOS.
    }

    if (strcmp(name, "window_resizable") == 0) {
        return 1;  // No windows on iOS.
    }

    return 0;
}

/*-----------------------------------------------------------------------*/

GraphicsError sys_graphics_set_display_mode(int width, int height)
{
    if (width != display_modes[0].width
     || height != display_modes[0].height
     || depth_bits > 24
     || stencil_bits > 8) {
        return GRAPHICS_ERROR_MODE_NOT_SUPPORTED;
    }

    if (!opengl_version_is_at_least(desired_opengl_major,
                                    desired_opengl_minor)) {
        return GRAPHICS_ERROR_BACKEND_TOO_OLD;
    }

    int rate = iroundf(desired_refresh_rate);
    if (rate < 60) {
        rate = 60;
    }
    if (rate > ios_get_native_refresh_rate()) {
        rate = ios_get_native_refresh_rate();
    }
    ios_set_refresh_rate(rate);
    return GRAPHICS_ERROR_SUCCESS;
}

/*-----------------------------------------------------------------------*/

int sys_graphics_display_is_window(void)
{
    return 0;  // No windows on iOS.
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
    /* Nothing to do. */
}

/*-----------------------------------------------------------------------*/

int sys_graphics_get_mouse_pointer_state(void)
{
    return 0;  // No system mouse pointer.
}

/*-----------------------------------------------------------------------*/

void sys_graphics_get_frame_period(int *numerator_ret, int *denominator_ret)
{
    *numerator_ret = 1001 * ios_get_frame_interval();
    *denominator_ret = 60000;
}

/*-----------------------------------------------------------------------*/

int sys_graphics_has_focus(void)
{
    /* There's no concept of "focus" on iOS (and we handle moving to the
     * background as a suspend event), so always return true. */
    return 1;
}

/*-----------------------------------------------------------------------*/

void sys_graphics_start_frame(int *width_ret, int *height_ret)
{
    *width_ret = display_modes[0].width;
    *height_ret = display_modes[0].height;

    wait_for_vsync();

    last_frame_counter = ios_get_frame_counter();

    opengl_start_frame();
    opengl_free_dead_resources(0);
}

/*-----------------------------------------------------------------------*/

void sys_graphics_finish_frame(void)
{
    ios_present_view();

#ifdef VSYNC_SEMAPHORES
    sys_semaphore_signal(vsync_sem[0]);
    main_thread_running = 0;
#else
    vsync_thread_id = thread_create_with_priority(VSYNC_THREAD_PRIORITY,
                                                  vsync_thread, NULL);
    if (!vsync_thread_id) {
# ifdef DEBUG
        static int warned = 0;
        if (!warned) {
            warned = 1;
            DLOG("thread_create(vsync_thread) failed");
        }
# endif
        vsync_thread(NULL);  // Do the work in this thread instead.
    }
#endif
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
/**************************** Local routines *****************************/
/*************************************************************************/

static int create_gl_shader_compilation_context(void)
{
    if ([EAGLContext currentContext]) {
        return 1;
    }
    if ([global_view createGLContext:FALSE]) {
        DLOG("Failed to create GL shader compilation context");
        return 0;
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

static void wait_for_vsync(void)
{
    if (vsync_thread_id) {
#ifdef VSYNC_SEMAPHORES
        if (!main_thread_running) {
            sys_semaphore_wait(vsync_sem[1], -1);
            main_thread_running = 1;
        }
#else
        thread_wait(vsync_thread_id);
        vsync_thread_id = 0;
#endif
    }
}

/*-----------------------------------------------------------------------*/

static int vsync_thread(UNUSED void *unused)
{
#ifdef VSYNC_SEMAPHORES
  while (!vsync_thread_stop) {
    sys_semaphore_wait(vsync_sem[0], -1);
#endif

    const int next = last_frame_counter + ios_get_frame_interval();
    while (ios_get_frame_counter() < next) {
        ios_vsync();
    }

#ifdef VSYNC_SEMAPHORES
    sys_semaphore_signal(vsync_sem[1]);
    BARRIER();
  }
#endif

    return 0;
}

/*************************************************************************/
/*************************************************************************/
