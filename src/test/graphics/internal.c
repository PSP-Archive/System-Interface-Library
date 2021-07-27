/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/graphics/internal.c: Helper functions for the graphics
 * subsystem tests.
 */

#include "src/base.h"
#include "src/graphics.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/test/base.h"
#include "src/test/graphics/internal.h"
#include "src/thread.h"

#if defined(SIL_PLATFORM_LINUX)
# include "src/sysdep/linux/internal.h"
#elif defined(SIL_PLATFORM_MACOSX)
# include "src/sysdep/macosx/graphics.h"
#elif defined(SIL_PLATFORM_WINDOWS)
# include "src/sysdep/windows/internal.h"
#endif

/*************************************************************************/
/*************************************************************************/

int auto_mipmaps_supported(void)
{
#ifdef SIL_PLATFORM_PSP
    return 0;
#else
    return 1;
#endif
}

/*-----------------------------------------------------------------------*/

int run_tests_in_window(int (*function)(void))
{
    return run_tests_in_sized_window(function, TESTW, TESTH);
}

/*-----------------------------------------------------------------------*/

int run_tests_in_sized_window(int (*function)(void), int width, int height)
{
    ASSERT(thread_init());
    ASSERT(graphics_init());
    ASSERT(graphics_set_display_attr("stencil_bits", 8));  // Ensure a stencil buffer.
    graphics_set_display_attr("vsync", 0);  // Run as fast as possible.
    if (!open_window(width, height)) {
        graphics_cleanup();
        FAIL("Unable to open window for tests");
    }
    graphics_set_viewport(0, 0, width, height);

    const int result = (*function)();

#ifdef SIL_PLATFORM_ANDROID
    /* Some devices/versions crash if we don't have a single rendering
     * operation between eglCreateContext() and eglDestroyContext(). */
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
#endif
    graphics_cleanup();
    thread_cleanup();
    return result;
}

/*-----------------------------------------------------------------------*/

int open_window(int width, int height)
{
    if (!graphics_set_display_attr("window", 1)) {
        /* Look for the smallest usable display size and use that instead. */
        const GraphicsDisplayModeList *mode_list;
        ASSERT(mode_list = graphics_list_display_modes(0));
        ASSERT(mode_list->num_modes > 0);
        int i;
        for (i = 0; i < mode_list->num_modes; i++) {
            if (mode_list->modes[i].width >= width
             && mode_list->modes[i].height >= height) {
                break;
            }
        }
        if (i >= mode_list->num_modes) {
            FAIL("No available display mode of size at least %dx%d",
                 width, height);
        }
        width = mode_list->modes[i].width;
        height = mode_list->modes[i].height;
    }

    const int success = graphics_set_display_mode(width, height, NULL);
#ifdef SIL_PLATFORM_WINDOWS
    if (success) {
        wine_new_window_workaround();
    }
#endif
    return success;
}

/*-----------------------------------------------------------------------*/

void force_close_window(void)
{
#if defined(SIL_PLATFORM_LINUX)
    linux_close_window();
#elif defined(SIL_PLATFORM_MACOSX)
    macosx_close_window();
#elif defined(SIL_PLATFORM_WINDOWS)
    windows_close_window();
#else
    WARN("Not implemented on this platform");
#endif
}

/*-----------------------------------------------------------------------*/

uint8_t *grab_display(void)
{
    const int width = TESTW, height = TESTH;

    uint8_t *pixels = mem_alloc(width*height*4, 0, MEM_ALLOC_TEMP);
    if (!pixels) {
        DLOG("Failed to allocate memory");
        return NULL;
    }
    if (!graphics_read_pixels(0, 0, width, height, pixels)) {
        DLOG("Failed to read pixels");
        mem_free(pixels);
        return NULL;
    }
    return pixels;
}

/*-----------------------------------------------------------------------*/

void draw_square(float z, float r, float g, float b, float a)
{
    ASSERT(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    ASSERT(graphics_add_vertex(&(Vector3f){-0.5, -0.5, z}, NULL,
                               &(Vector4f){r, g, b, a}));
    ASSERT(graphics_add_vertex(&(Vector3f){-0.5, +0.5, z}, NULL,
                               &(Vector4f){r, g, b, a}));
    ASSERT(graphics_add_vertex(&(Vector3f){+0.5, +0.5, z}, NULL,
                               &(Vector4f){r, g, b, a}));
    ASSERT(graphics_add_vertex(&(Vector3f){+0.5, -0.5, z}, NULL,
                               &(Vector4f){r, g, b, a}));
    ASSERT(graphics_end_and_draw_primitive());
}

/*-----------------------------------------------------------------------*/

int check_colored_rectangle(int w, int h, int cx, int cy,
                            float r, float g, float b)
{
    const int R = iroundf(r*255), G = iroundf(g*255), B = iroundf(b*255);

    uint8_t pixels[64*64*4];
    ASSERT(graphics_read_pixels(0, 0, 64, 64, pixels));
    for (int i = 0; i < 64*64*4; i += 4) {
        const int x = (i/4) % 64;
        const int y = (i/4) / 64;
        if (x >= cx-w/2 && x < cx+w/2 && y >= cy-h/2 && y < cy+h/2) {
            CHECK_PIXEL_NEAR(&pixels[i], R,G,B,255, 1, x, y);
        } else {
            CHECK_PIXEL(&pixels[i], 0,0,0,255, x, y);
        }
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

int get_alternate_video_mode(int *width_ret, int *height_ret)
{
    const char *env = testutil_getenv("SIL_TEST_ALTERNATE_DISPLAY_MODE");
    if (*env) {
        const char *s;
        *width_ret = (int)strtol(env, (char **)&s, 10);
        if (*s == 'x') {
            *height_ret = (int)strtol(s+1, (char **)&s, 10);
            if (!*s) {
                return 1;
            }
        }
        static uint8_t warned = 0;
        if (!warned) {
            WARN("Invalid syntax for SIL_TEST_ALTERNATE_DISPLAY_MODE: %s\n",
                 env);
            warned = 1;
        }
    }

    int high_res;
#ifdef SIL_PLATFORM_LINUX
    high_res = 0; // X11 can fail to switch to higher-than-default resolutions.
#else
    high_res = 1; // Avoid windows getting moved around by the window manager.
#endif

    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(1));
    CHECK_TRUE(mode_list->num_modes > 0);

    const int default_width = graphics_device_width();
    const int default_height = graphics_device_height();
    const int default_pixels = default_width * default_height;
    int best_mode = -1;
    int best_pixels = 0;
    for (int i = 0; i < mode_list->num_modes; i++) {
        if (mode_list->modes[i].device != 0) {
            break;
        }
        if (mode_list->modes[i].width != default_width
         || mode_list->modes[i].height != default_height) {
            const int pixels =
                mode_list->modes[i].width * mode_list->modes[i].height;
            int use;
            if (best_mode < 0) {
                use = 1;
            } else if (high_res) {
                use = (best_pixels < default_pixels && pixels > best_pixels)
                   || (best_pixels > default_pixels && pixels < default_pixels
                                                    && pixels < best_pixels);
            } else {  // !high_res
                use = (best_pixels > default_pixels && pixels < best_pixels)
                   || (best_pixels < default_pixels && pixels < default_pixels
                                                    && pixels > best_pixels);
            }
            if (use) {
                best_mode = i;
                best_pixels = pixels;
            }
        }
    }
    if (best_mode < 0) {
        return 0;
    }
    *width_ret = mode_list->modes[best_mode].width;
    *height_ret = mode_list->modes[best_mode].height;
    return 1;
}

/*-----------------------------------------------------------------------*/

void get_mouse_position(int *x_ret, int *y_ret)
{
    PRECOND(x_ret != NULL, return);
    PRECOND(y_ret != NULL, return);

    *x_ret = *y_ret = -1;

#if defined(SIL_PLATFORM_LINUX)
    Display *display = linux_x11_display();
    Window root = RootWindow(display, linux_x11_screen());
    Window unused_r, unused_c;
    int unused_wx, unused_wy;
    unsigned int unused_mask;
    if (UNLIKELY(!XQueryPointer(display, root, &unused_r, &unused_c,
                                x_ret, y_ret, &unused_wx, &unused_wy,
                                &unused_mask))) {
        DLOG("Failed to get pointer position");
    }
#elif defined(SIL_PLATFORM_MACOSX)
    CGEventRef event = CGEventCreate(NULL);
    ASSERT(event);
    CGPoint point = CGEventGetLocation(event);
    CFRelease(event);
    *x_ret = itruncf(point.x);
    *y_ret = itruncf(point.y);
#elif defined(SIL_PLATFORM_WINDOWS)
    POINT position;
    GetCursorPos(&position);
    *x_ret = position.x;
    *y_ret = position.y;
#endif
}

/*-----------------------------------------------------------------------*/

void set_mouse_position(int x, int y)
{
    if (x < 0 || y < 0) {
        return;  // The pointer position was not successfully saved.
    }

#if defined(SIL_PLATFORM_LINUX)
    Display *display = linux_x11_display();
    Window root = RootWindow(display, linux_x11_screen());
    XWarpPointer(display, None, root, 0, 0, 0, 0, x, y);
#elif defined(SIL_PLATFORM_MACOSX)
    CGWarpMouseCursorPosition((CGPoint){x, y});
    CGAssociateMouseAndMouseCursorPosition(1);
#elif defined(SIL_PLATFORM_WINDOWS)
    SetCursorPos(x, y);
#endif
}

/*-----------------------------------------------------------------------*/

#ifdef SIL_PLATFORM_WINDOWS

void wine_new_window_workaround(void)
{
    int running_under_wine = 0;
    char *envp = GetEnvironmentStrings();
    const char *s = envp;
    while (*s) {
        if (strncmp(s, "WINE", 4) == 0) {
            running_under_wine = 1;
            break;
        }
        s += strlen(s) + 1;
    }
    FreeEnvironmentStrings(envp);

    if (running_under_wine) {
        Sleep(50);
        graphics_start_frame();
        graphics_finish_frame();
    }
}

#endif

/*************************************************************************/
/*************************************************************************/
