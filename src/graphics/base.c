/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/graphics/base.c: Basic graphics functionality.
 */

#include "src/base.h"
#include "src/debug.h"
#include "src/graphics.h"
#include "src/graphics/internal.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/texture.h"

/*************************************************************************/
/********************** Test control data (global) ***********************/
/*************************************************************************/

#ifdef SIL_INCLUDE_TESTS
int TEST_graphics_sync_count;
#endif

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Have we already initialized? */
static uint8_t initted;

/* Does the system support a windowed mode? */
static uint8_t has_windowed_mode;

#ifdef DEBUG
/* Is sync-on-frame-start for debug mode enabled? */
static uint8_t debug_sync;
#endif

/* Number of display devices available. */
static int num_display_devices;

/* List of available display modes (allocated at initialization time). */
static GraphicsDisplayModeList *display_mode_list;
/* List of display modes, ignoring refresh. */
static GraphicsDisplayModeList *display_mode_list_no_refresh;

/* Current display size (0 = display is not initialized). */
static int display_width = 0, display_height = 0;

/*************************************************************************/
/********* Interface: Initialization, configuration, and cleanup *********/
/*************************************************************************/

int graphics_init(void)
{
    if (UNLIKELY(initted)) {
        DLOG("Already initialized!");
        return 0;
    }

    const SysGraphicsInfo *graphics_info = sys_graphics_init();
    if (UNLIKELY(!graphics_info)) {
        DLOG("sys_graphics_init() failed");
        return 0;
    }
    ASSERT(graphics_info->num_devices >= 1, sys_graphics_cleanup(); return 0);
    ASSERT(graphics_info->num_modes >= 1, sys_graphics_cleanup(); return 0);
    ASSERT(graphics_info->modes != NULL, sys_graphics_cleanup(); return 0);

    has_windowed_mode = graphics_info->has_windowed_mode;
    num_display_devices = graphics_info->num_devices;

    const int size = ((int)sizeof(*display_mode_list)
                      + ((graphics_info->num_modes - 1)
                         * (int)sizeof(*display_mode_list->modes)));
    display_mode_list = mem_alloc(size, 0, 0);
    if (UNLIKELY(!display_mode_list)) {
        DLOG("No memory for display mode list (%d bytes)", size);
        sys_graphics_cleanup();
        return 0;
    }
    display_mode_list->num_modes = graphics_info->num_modes;
    for (int i = 0; i < graphics_info->num_modes; i++) {
        display_mode_list->modes[i] = graphics_info->modes[i];
    }

    /* Sort the mode entries. */
    GraphicsDisplayModeEntry *modes = display_mode_list->modes; // For brevity.
    for (int i = 0; i < display_mode_list->num_modes - 1; i++) {
        int best = i;
        int best_pixels = modes[i].width * modes[i].height;
        for (int j = i+1; j < graphics_info->num_modes; j++) {
            int num_pixels = modes[j].width * modes[j].height;
            if (modes[j].device < modes[best].device
                || (modes[j].device == modes[best].device
                    && (num_pixels < best_pixels
                        || (num_pixels == best_pixels
                            && (modes[j].width < modes[best].width
                                || (modes[j].width == modes[best].width
                                    && modes[j].refresh < modes[best].refresh))))))
            {
                best = j;
                best_pixels = num_pixels;
            }
        }
        if (best != i) {
            GraphicsDisplayModeEntry temp = modes[i];
            modes[i] = modes[best];
            modes[best] = temp;
        }
    }

    /* Remove duplicate entries from the mode list. */
    for (int i = 1; i < display_mode_list->num_modes; i++) {
        if (modes[i].device == modes[i-1].device
         && modes[i].width == modes[i-1].width
         && modes[i].height == modes[i-1].height
         && modes[i].refresh == modes[i-1].refresh) {
            memmove(&modes[i], &modes[i+1],
                    sizeof(*modes) * (display_mode_list->num_modes - (i+1)));
            display_mode_list->num_modes--;
            i--;
        }
    }

    /* Create a separate list treating each refresh value as zero, for
     * returning from graphics_list_display_modes(0). */
    display_mode_list_no_refresh = mem_alloc(size, 0, 0);
    if (UNLIKELY(!display_mode_list_no_refresh)) {
        DLOG("No memory for no-refresh display mode list (%d bytes)", size);
        mem_free(display_mode_list);
        sys_graphics_cleanup();
        return 0;
    }
    display_mode_list_no_refresh->modes[0] = modes[0];
    display_mode_list_no_refresh->modes[0].refresh = 0;
    int num_no_refresh = 1;
    for (int i = 1; i < display_mode_list->num_modes; i++) {
        if (modes[i].device != modes[i-1].device
         || modes[i].width != modes[i-1].width
         || modes[i].height != modes[i-1].height) {
            display_mode_list_no_refresh->modes[num_no_refresh] = modes[i];
            display_mode_list_no_refresh->modes[num_no_refresh].refresh = 0;
            num_no_refresh++;
        }
    }
    display_mode_list_no_refresh->num_modes = num_no_refresh;

#ifdef DEBUG
    debug_sync = 0;
#endif
#ifdef SIL_INCLUDE_TESTS
    TEST_graphics_sync_count = 0;
#endif

    initted = 1;
    return 1;
}

/*-----------------------------------------------------------------------*/

void graphics_cleanup(void)
{
    if (!initted) {
        return;
    }

    primitive_cleanup();
    framebuffer_cleanup();

    display_width = display_height = 0;
    mem_free(display_mode_list);
    display_mode_list = NULL;
    mem_free(display_mode_list_no_refresh);
    display_mode_list_no_refresh = NULL;
    num_display_devices = 0;
    has_windowed_mode = 0;

    sys_graphics_cleanup();

    initted = 0;
}

/*-----------------------------------------------------------------------*/

int graphics_num_devices(void)
{
    return num_display_devices;
}

/*-----------------------------------------------------------------------*/

int graphics_device_width(void)
{
    return sys_graphics_device_width();
}

/*-----------------------------------------------------------------------*/

int graphics_device_height(void)
{
    return sys_graphics_device_height();
}

/*-----------------------------------------------------------------------*/

int graphics_has_windowed_mode(void)
{
    return has_windowed_mode;
}

/*-----------------------------------------------------------------------*/

const GraphicsDisplayModeList *graphics_list_display_modes(
    int include_refresh)
{
    if (include_refresh) {
        return display_mode_list;
    } else {
        return display_mode_list_no_refresh;
    }
}

/*-----------------------------------------------------------------------*/

int graphics_set_display_attr(const char *name, ...)
{
    va_list args;
    va_start(args, name);
    const int result = sys_graphics_set_display_attr(name, args);
    va_end(args);
    return result;
}

/*-----------------------------------------------------------------------*/

int graphics_set_display_mode(int width, int height, GraphicsError *error_ret)
{
    if (width <= 0 || height <= 0) {
        DLOG("Invalid parameters: %d %d", width, height);
        if (error_ret) {
            *error_ret = GRAPHICS_ERROR_MODE_NOT_SUPPORTED;
        }
        return 0;
    }

    GraphicsError error = sys_graphics_set_display_mode(width, height);
    if (error_ret) {
        *error_ret = error;
    }
    if (error != GRAPHICS_ERROR_SUCCESS
     && error != GRAPHICS_ERROR_STATE_LOST) {
        display_width = display_height = 0;
        return 0;
    }

    display_width = width;
    display_height = height;
    if (!graphics_viewport_width()) {
        graphics_set_viewport(0, 0, width, height);
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

const char *graphics_renderer_info(void)
{
    return sys_graphics_renderer_info();
}

/*-----------------------------------------------------------------------*/

int graphics_display_is_window(void)
{
    return sys_graphics_display_is_window();
}

/*-----------------------------------------------------------------------*/

void graphics_set_window_title(const char *title)
{
    if (UNLIKELY(!title)) {
        DLOG("title == NULL");
        return;
    }

    sys_graphics_set_window_title(title);
}

/*-----------------------------------------------------------------------*/

void graphics_set_window_resize_limits(
    int min_width, int min_height, int max_width, int max_height,
    int min_aspect_x, int min_aspect_y, int max_aspect_x, int max_aspect_y)
{
    if (!(min_width > 0 && min_height > 0)) {
        if (UNLIKELY(!(min_width == 0 && min_height == 0))) {
            DLOG("Invalid minimum size %dx%d", min_width, min_height);
            min_width = 0;
            min_height = 0;
        }
    }

    if (!(max_width > 0 && max_height > 0)) {
        if (UNLIKELY(!(max_width == 0 && max_height == 0))) {
            DLOG("Invalid maximum size %dx%d", max_width, max_height);
            max_width = 0;
            max_height = 0;
        }
    }

    if (!(min_aspect_x > 0 && min_aspect_y > 0)) {
        if (UNLIKELY(!(min_aspect_x == 0 && min_aspect_y == 0))) {
            DLOG("Invalid minimum aspect ratio %d:%d",
                 min_aspect_x, min_aspect_y);
            min_aspect_x = 0;
            min_aspect_y = 0;
        }
    }

    if (!(max_aspect_x > 0 && max_aspect_y > 0)) {
        if (UNLIKELY(!(max_aspect_x == 0 && max_aspect_y == 0))) {
            DLOG("Invalid maximum aspect ratio %d:%d",
                 max_aspect_x, max_aspect_y);
            max_aspect_x = 0;
            max_aspect_y = 0;
        }
    }

    sys_graphics_set_window_resize_limits(
        min_width, min_height, max_width, max_height,
        min_aspect_x, min_aspect_y, max_aspect_x, max_aspect_y);
}

/*-----------------------------------------------------------------------*/

void graphics_set_window_icon(int texture)
{
    if (UNLIKELY(!texture)) {
        DLOG("texture == 0");
        return;
    }

    SysTexture *systex = texture_lock_raw(texture);
    if (!systex) {
        DLOG("Failed to lock texture");
        return;
    }
    sys_graphics_set_window_icon(systex);
    texture_unlock(texture);
}

/*-----------------------------------------------------------------------*/

void graphics_show_mouse_pointer(int on)
{
    sys_graphics_show_mouse_pointer(on);
}

/*-----------------------------------------------------------------------*/

int graphics_get_mouse_pointer_state(void)
{
    return sys_graphics_get_mouse_pointer_state();
}

/*-----------------------------------------------------------------------*/

int graphics_display_width(void)
{
    return display_width;
}

/*-----------------------------------------------------------------------*/

int graphics_display_height(void)
{
    return display_height;
}

/*-----------------------------------------------------------------------*/

double graphics_frame_period(void)
{
    int numerator = 0, denominator = 1;
    sys_graphics_get_frame_period(&numerator, &denominator);
    return (double)numerator / (double)denominator;
}

/*-----------------------------------------------------------------------*/

int graphics_has_focus(void)
{
    return sys_graphics_has_focus();
}

/*************************************************************************/
/*********** Interface: Frame start/finish and synchronization ***********/
/*************************************************************************/

void graphics_start_frame(void)
{
#ifdef DEBUG
    debug_record_cpu_phase(DEBUG_CPU_PROCESS_END);
# if !(defined(SIL_PLATFORM_PSP) && defined(SIL_PLATFORM_PSP_GPU_WAIT_ON_FINISH))
    debug_record_cpu_phase(DEBUG_CPU_GPU_WAIT_START);
    if (debug_sync) {
        graphics_sync();
    }
    debug_record_cpu_phase(DEBUG_CPU_GPU_WAIT_END);
# endif
#endif

    sys_graphics_start_frame(&display_width, &display_height);
    sys_framebuffer_bind(NULL);

#ifdef DEBUG
    debug_record_cpu_phase(DEBUG_CPU_RENDER_START);
#endif
}

/*-----------------------------------------------------------------------*/

void graphics_finish_frame(void)
{
#ifdef DEBUG
    debug_record_cpu_phase(DEBUG_CPU_RENDER_END);
#endif

    sys_graphics_finish_frame();

#ifdef DEBUG
    debug_record_cpu_phase(DEBUG_CPU_PROCESS_START);
#endif
}

/*-----------------------------------------------------------------------*/

void graphics_sync(void)
{
    sys_graphics_sync(0);
#ifdef SIL_INCLUDE_TESTS
    TEST_graphics_sync_count++;
#endif
}

/*-----------------------------------------------------------------------*/

void graphics_flush_resources(void)
{
    sys_graphics_sync(1);
}

/*-----------------------------------------------------------------------*/

void graphics_enable_debug_sync(DEBUG_USED int enable)
{
#ifdef DEBUG
    debug_sync = (enable != 0);
#endif
}

/*************************************************************************/
/**************** Interface: Display clearing and reading ****************/
/*************************************************************************/

void graphics_clear(float r, float g, float b, float a,
                    float depth, unsigned int stencil)
{
    sys_graphics_clear(
        &(Vector4f){bound(r,0,1), bound(g,0,1), bound(b,0,1), bound(a,0,1)},
        (float[1]){bound(depth,0,1)}, stencil);
}

/*-----------------------------------------------------------------------*/

void graphics_clear_color(float r, float g, float b, float a)
{
    sys_graphics_clear(
        &(Vector4f){bound(r,0,1), bound(g,0,1), bound(b,0,1), bound(a,0,1)},
        NULL, 0);
}

/*-----------------------------------------------------------------------*/

void graphics_clear_depth(float depth, unsigned int stencil)
{
    sys_graphics_clear(NULL, (float[1]){bound(depth,0,1)}, stencil);
}

/*-----------------------------------------------------------------------*/

int graphics_read_pixels(int x, int y, int w, int h, void *buffer)
{
    if (UNLIKELY(!buffer)) {
        DLOG("buffer == NULL");
        return 0;
    }

    const int stride = w;
    if (x < 0) {
        buffer = (void *)((char *)buffer + (-x)*4);
        w -= (-x);
        x = 0;
    }
    if (y < 0) {
        buffer = (void *)((char *)buffer + (-y)*stride*4);
        h -= (-y);
        y = 0;
    }
    if (w <= 0 || h <= 0) {
        return 1;
    }
    return sys_graphics_read_pixels(x, y, w, h, stride, buffer);
}

/*************************************************************************/
/******** Interface: Shader object / generated shader mode switch ********/
/*************************************************************************/

int graphics_use_shader_objects(int enable)
{
    sys_graphics_set_shader_generator(NULL, NULL, NULL, 0, 0);
    sys_shader_clear_attributes();
    sys_shader_pipeline_apply(NULL);
    if (enable) {
        return sys_graphics_enable_shader_objects();
    } else {
        return sys_graphics_disable_shader_objects();
    }
}

/*************************************************************************/
/*************************************************************************/
