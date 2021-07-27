/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/graphics/misc.c: Miscellaneous tests for graphics functions.
 */

#include "src/base.h"
#include "src/graphics.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/test/base.h"
#include "src/test/graphics/internal.h"
#include "src/texture.h"
#include "src/thread.h"

#if defined(SIL_PLATFORM_LINUX)
# include "src/sysdep/linux/internal.h"
# include <dlfcn.h>
# define RTLD_DEFAULT  NULL  // See note in src/sysdep/linux/graphics.c
# include <X11/Xatom.h>
# include <X11/Xlib.h>
# include <X11/extensions/Xrandr.h>
# include <X11/extensions/xf86vmode.h>
# include <time.h>  // For nanosleep().
#elif defined(SIL_PLATFORM_MACOSX)
# include "src/sysdep/macosx/graphics.h"
# include <CoreGraphics/CoreGraphics.h>
# include <time.h>  // For nanosleep().
#elif defined(SIL_PLATFORM_WINDOWS)
# include "src/sysdep/windows/internal.h"
#endif

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

/**
 * get_display_resolution:  Get the current display resolution from the
 * operating system.
 *
 * [Parameters]
 *     width_ret, height_ret: Pointers to variables to receive the current
 *         display resolution.
 * [Return value]
 *     True on success, false if the function is not implemented on the
 *     current platform.
 */
static int get_display_resolution(int *width_ret, int *height_ret)
{
#if defined(SIL_PLATFORM_LINUX)

    *width_ret = *height_ret = -1;

    #define LOOKUP(sym) \
        __typeof__(sym) *p_##sym; \
        p_##sym = dlsym(RTLD_DEFAULT, #sym)
    LOOKUP(XRRFreeCrtcInfo);
    LOOKUP(XRRFreeOutputInfo);
    LOOKUP(XRRFreeScreenResources);
    LOOKUP(XRRGetCrtcInfo);
    LOOKUP(XRRGetOutputInfo);
    LOOKUP(XRRGetScreenResources);
    LOOKUP(XRRQueryExtension);
    LOOKUP(XRRQueryVersion);
    LOOKUP(XF86VidModeGetModeLine);
    LOOKUP(XF86VidModeQueryExtension);
    #undef LOOKUP

    Display *display = linux_x11_display();
    int major, minor;
    if (p_XRRQueryExtension
     && (*p_XRRQueryExtension)(display, (int[1]){0}, (int[1]){0})
     && (*p_XRRQueryVersion)(display, &major, &minor)
     && (major >= 2 || (major == 1 && minor >= 2))) {
        XRRScreenResources *sr =
            (*p_XRRGetScreenResources)(display, RootWindow(display, 0));
        ASSERT(sr);
        for (int i = 0; i < sr->noutput; i++) {
            XRROutputInfo *oi =
                (*p_XRRGetOutputInfo)(display, sr, sr->outputs[i]);
            ASSERT(oi);
            if (!oi) {
                continue;
            }
            if (!oi->crtc || oi->connection == RR_Disconnected) {
                (*p_XRRFreeOutputInfo)(oi);
                continue;
            }
            XRRCrtcInfo *crtc = (*p_XRRGetCrtcInfo)(display, sr, oi->crtc);
            ASSERT(crtc);
            const int rotated =
                (crtc->rotation & (RR_Rotate_90 | RR_Rotate_270)) != 0;
            for (int j = 0; j < oi->nmode; j++) {
                const XRRModeInfo *mode = NULL;
                for (int k = 0; k < sr->nmode; k++) {
                    if (sr->modes[k].id == oi->modes[j]) {
                        mode = &sr->modes[k];
                        break;
                    }
                }
                ASSERT(mode);
                if (mode->id == crtc->mode) {
                    if (rotated) {
                        *width_ret = mode->height;
                        *height_ret = mode->width;
                    } else {
                        *width_ret = mode->width;
                        *height_ret = mode->height;
                    }
                    break;
                }
            }
            (*p_XRRFreeCrtcInfo)(crtc);
            (*p_XRRFreeOutputInfo)(oi);
            break;
        }
        (*p_XRRFreeScreenResources)(sr);
    } else if (p_XF86VidModeQueryExtension
            && (*p_XF86VidModeQueryExtension)(display,
                                              (int[1]){0}, (int[1]){0})) {
        int dotclock;
        XF86VidModeModeLine modeline;
        ASSERT((*p_XF86VidModeGetModeLine)(display, DefaultScreen(display),
                                           &dotclock, &modeline));
        *width_ret = modeline.hdisplay;
        *height_ret = modeline.vdisplay;
    } else {
        FAIL("No video mode interface found!");
    }

    ASSERT(*width_ret != -1);

#elif defined(SIL_PLATFORM_MACOSX)

    CGDirectDisplayID display_id = macosx_display_id(0);
    CGDisplayModeRef current_mode = CGDisplayCopyDisplayMode(display_id);
    *width_ret = CGDisplayModeGetWidth(current_mode);
    *height_ret = CGDisplayModeGetHeight(current_mode);

#elif defined(SIL_PLATFORM_WINDOWS)

    *width_ret = *height_ret = -1;

    for (int device = 0; ; device++) {
        DISPLAY_DEVICE device_info = {.cb = sizeof(device_info)};
        ASSERT(EnumDisplayDevices(NULL, device, &device_info, 0));
        if (!(device_info.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)
         || !(device_info.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)) {
            continue;
        }
        DEVMODE default_mode = {.dmSize = sizeof(default_mode)};
        ASSERT(EnumDisplaySettings(device_info.DeviceName,
                                   ENUM_REGISTRY_SETTINGS, &default_mode));
        ASSERT(default_mode.dmFields & DM_PELSWIDTH);
        ASSERT(default_mode.dmFields & DM_PELSHEIGHT);
        *width_ret = default_mode.dmPelsWidth;
        *height_ret = default_mode.dmPelsHeight;
        break;
    }

    ASSERT(*width_ret != -1);

#else

    (void) width_ret;  // Avoid unused-parameter warnings.
    (void) height_ret;
    return 0;

#endif

    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_graphics_misc)

TEST_INIT(init)
{
    CHECK_TRUE(thread_init());
    return graphics_init();
}

TEST_CLEANUP(cleanup)
{
    graphics_cleanup();
    thread_cleanup();
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_window_icon)
{
    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(0));
    CHECK_TRUE(mode_list->num_modes > 0);
    const int width = mode_list->modes[0].width;
    const int height = mode_list->modes[0].height;
    CHECK_TRUE(graphics_set_display_attr("window",
                                         graphics_has_windowed_mode()));
    CHECK_TRUE(graphics_set_display_mode(width, height, NULL));

    int texture;
    CHECK_TRUE(texture = texture_create(32, 32, 0, 0));
    uint8_t *pixels;
    CHECK_TRUE(pixels = texture_lock(texture));
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            pixels[(y*32+x)*4+0] = x<<3;
            pixels[(y*32+x)*4+1] = y;
            pixels[(y*32+x)*4+2] = x<<3 ^ y;
            pixels[(y*32+x)*4+3] = (y*32+x)/4;
        }
    }
    texture_unlock(texture);
    graphics_set_window_icon(texture);
#ifdef SIL_PLATFORM_LINUX
    Display *x11_display = linux_x11_display();
    Atom net_wm_icon = XInternAtom(x11_display, "_NET_WM_ICON", True);
    if (net_wm_icon) {
        Atom actual_type;
        int actual_format;
        unsigned long nitems, bytes_after;
        unsigned char *prop;
        CHECK_INTEQUAL(
            XGetWindowProperty(x11_display, linux_x11_window(), net_wm_icon,
                               0, 2+32*32, False, AnyPropertyType, &actual_type,
                               &actual_format, &nitems, &bytes_after, &prop),
            Success);
        CHECK_INTEQUAL(actual_type, XA_CARDINAL);
        CHECK_INTEQUAL(actual_format, 32);
        CHECK_INTEQUAL(nitems, 2+32*32);
        CHECK_INTEQUAL(bytes_after, 0);
        /* "long" instead of "int32_t" is correct -- see linux/graphics.c. */
        const long *icon_data = (long *)(void *)prop;
        CHECK_INTEQUAL(icon_data[0], 32);
        CHECK_INTEQUAL(icon_data[1], 32);
        for (int y = 0; y < 32; y++) {
            for (int x = 0; x < 32; x++) {
                const int r = x<<3, g = y, b = x<<3 ^ y, a = (y*32+x)/4;
                const long expected = a<<24 | r<<16 | g<<8 | b;
                if (icon_data[2+(y*32+x)] != expected) {
                    FAIL("icon_data[%d] was 0x%08lX but should have been"
                         " 0x%08lX", 2+(y*32+x), icon_data[2+(y*32+x)],
                         expected);
                }
            }
        }
        XFree(prop);
    } else {
        /* Maybe there's no WM running? */
        WARN("_NET_WM_ICON not defined; can't retrieve icon data");
    }
#else
    /* We can't retrieve the icon once set, so just make sure it doesn't
     * break the texture. */
#endif
    const uint8_t *pixels_ro;
    CHECK_TRUE(pixels_ro = texture_lock_readonly(texture));
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            CHECK_PIXEL(&pixels_ro[(y*32+x)*4],
                        x<<3, y, x<<3 ^ y, (y*32+x)/4, x, y);
        }
    }
    texture_destroy(texture);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_window_title_icon_invalid)
{
    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(0));
    CHECK_TRUE(mode_list->num_modes > 0);
    const int width = mode_list->modes[0].width;
    const int height = mode_list->modes[0].height;
    CHECK_TRUE(graphics_set_display_attr("window",
                                         graphics_has_windowed_mode()));
    CHECK_TRUE(graphics_set_display_mode(width, height, NULL));

    /* These don't return values, so just make sure they don't crash. */
    graphics_set_window_title(NULL);
    graphics_set_window_icon(0);
    int texture;
    CHECK_TRUE(texture = texture_create(32, 32, 0, 0));
    texture_destroy(texture);
    graphics_set_window_icon(texture);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_texture_create_from_display_orientation)
{
    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(0));
    CHECK_TRUE(mode_list->num_modes > 0);
    const int width = mode_list->modes[0].width;
    const int height = mode_list->modes[0].height;
    CHECK_TRUE(graphics_set_display_attr("window",
                                         graphics_has_windowed_mode()));
    CHECK_TRUE(graphics_set_display_mode(width, height, NULL));
    graphics_start_frame();
    graphics_clear(0, 0, 1, 0, 1, 0);
    graphics_set_viewport(64, 40, 48, 32);
    draw_square(0, 1,1,1,1);

    int texture;
    const uint8_t *pixels;

    /* Check that pixels are read in the correct direction (bottom to top,
     * left to right). */
    CHECK_TRUE(
        texture = texture_create_from_display(0, 0, width, height, 1, 0, 0));
    CHECK_TRUE(pixels = texture_lock_readonly(texture));
    for (int i = 0; i < width*height*4; i += 4) {
        const int x = (i/4) % width;
        const int y = (i/4) / width;
        const int p = ((x >= 76 && x < 100) && (y >= 48 && y < 64)) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,255,255, x, y);
    }
    texture_destroy(texture);

    /* Check that grab coordinates are properly based at the bottom-left. */
    CHECK_TRUE(
        texture = texture_create_from_display(75, 47, 26, 18, 1, 0, 0));
    CHECK_TRUE(pixels = texture_lock_readonly(texture));
    for (int i = 0; i < 26*18*4; i += 4) {
        const int x = (i/4) % 26;
        const int y = (i/4) / 26;
        const int p = ((x >= 1 && x < 25) && (y >= 1 && y < 17)) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,255,255, x, y);
    }
    texture_destroy(texture);

    graphics_finish_frame();
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_texture_create_from_display_unreadable)
{
    const GraphicsDisplayModeList *mode_list;
    CHECK_TRUE(mode_list = graphics_list_display_modes(0));
    CHECK_TRUE(mode_list->num_modes > 0);
    const int width = mode_list->modes[0].width;
    const int height = mode_list->modes[0].height;
    CHECK_TRUE(graphics_set_display_attr("window",
                                         graphics_has_windowed_mode()));
    CHECK_TRUE(graphics_set_display_mode(width, height, NULL));
    graphics_start_frame();

    int texture;
    uint8_t *pixels;

    graphics_clear(0.2, 0.4, 0.6, 0, 1, 0);
    graphics_set_clip_region(1, 1, 2, 3);
    graphics_clear(0.8, 0.6, 0.4, 0, 1, 0);
    graphics_set_clip_region(0, 0, 0, 0);
    CHECK_TRUE(
        texture = texture_create_from_display(0, 0, width, height, 1, 0, 0));
    texture_set_repeat(texture, 0, 0);
    texture_set_antialias(texture, 0);

    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_set_viewport(0, 0, width, height);
    texture_apply(0, texture);
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    graphics_add_vertex(&(Vector3f){-1,-1,0}, &(Vector2f){0,0}, NULL);
    graphics_add_vertex(&(Vector3f){-1,+1,0}, &(Vector2f){0,1}, NULL);
    graphics_add_vertex(&(Vector3f){+1,+1,0}, &(Vector2f){1,1}, NULL);
    graphics_add_vertex(&(Vector3f){+1,-1,0}, &(Vector2f){1,0}, NULL);
    graphics_end_and_draw_primitive();
    texture_apply(0, 0);
    graphics_set_viewport(0, 0, TESTW, TESTH);
    ASSERT(pixels = mem_alloc(width*height*4, 64, 0));
    CHECK_TRUE(graphics_read_pixels(0, 0, width, height, pixels));
    for (int i = 0; i < width*height*4; i += 4) {
        const int x = (i/4) % width, y = (i/4) / width;
        if (x >= 1 && x <= 2 && y >= 1 && y <= 3) {
            CHECK_PIXEL(&pixels[i], 204,153,102,255, x, y);
        } else {
            CHECK_PIXEL(&pixels[i], 51,102,153,255, x, y);
        }
    }
    mem_free(pixels);

    graphics_finish_frame();
    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_multisample)
{
    if (!(graphics_set_display_attr("multisample", 4))) {
        SKIP("Multisample not supported.");
    }
    if (!open_window(TESTW, TESTH)) {
        SKIP("Unable to set a multisample display mode.");
    }
    graphics_set_viewport(0, 0, TESTW, TESTH);

    graphics_start_frame();

    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){1.0f/TESTW,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){1.0f/TESTW,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    uint8_t *pixels;
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        if (x == TESTW/2 && y >= TESTH/2) {
            /* Sample points are generally pseudorandom, so we don't know
             * exactly what value we'll get back from the renderer, but it
             * should be a non-black, non-white grey. */
            const int p = pixels[i];
            if (p == 0 || p == 255 || pixels[i+1] != p || pixels[i+2] != p
             || pixels[i+3] != 255) {
                FAIL("Pixel (%d,%d) was RGBA (%u,%u,%u,%u) but should have"
                     " been multisampled grey",
                     x, y, pixels[i+0], pixels[i+1], pixels[i+2], pixels[i+3]);
            }
        } else {
            const int p = (x < TESTW/2 && y >= TESTH/2) ? 255 : 0;
            CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
        }
    }
    mem_free(pixels);

    graphics_finish_frame();
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_read_pixels_after_window_size_change)
{
    if (!graphics_has_windowed_mode()) {
        SKIP("This platform does not support windowed mode.");
    }

    uint8_t *pixels;

    ASSERT(open_window(TESTW, TESTH));
    CHECK_TRUE(graphics_set_display_mode(TESTW*2, TESTH*2, NULL));
    /* Some drivers may need an extra frame or a bit of time to settle
     * before we can read data. */
#ifdef SIL_PLATFORM_WINDOWS
    Sleep(50);
#elif defined(SIL_PLATFORM_LINUX) || defined(SIL_PLATFORM_MACOSX)
    nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = 50*1000*1000}, NULL);
#endif
    graphics_start_frame();
    graphics_finish_frame();
    graphics_start_frame();
    graphics_clear(0.2, 0.4, 0.6, 0, 1, 0);
    graphics_set_viewport(0, 0, TESTW*2, TESTH*2);
    draw_square(0, 1,1,1,1);

    ASSERT(pixels = mem_alloc((TESTW*2) * (TESTH*2) * 4, 0, MEM_ALLOC_TEMP));
    CHECK_TRUE(graphics_read_pixels(0, 0, TESTW*2, TESTH*2, pixels));
    for (int i = 0; i < (TESTW*2)*(TESTH*2)*4; i += 4) {
        const int x = (i/4) % (TESTW*2), y = (i/4) / (TESTW*2);
        if (x >= TESTW/2 && x < 3*TESTW/2 && y >= TESTH/2 && y < 3*TESTH/2) {
            CHECK_PIXEL(&pixels[i], 255,255,255,255, x, y);
        } else {
            CHECK_PIXEL(&pixels[i], 51,102,153,255, x, y);
        }
    }
    mem_free(pixels);

    int texture;
    CHECK_TRUE(texture = texture_create_from_display(
                   0, 0, TESTW*2, TESTH*2, 1, 0, 0));
    CHECK_TRUE(pixels = texture_lock(texture));
    for (int i = 0; i < (TESTW*2)*(TESTH*2)*4; i += 4) {
        const int x = (i/4) % (TESTW*2), y = (i/4) / (TESTW*2);
        if (x >= TESTW/2 && x < 3*TESTW/2 && y >= TESTH/2 && y < 3*TESTH/2) {
            CHECK_PIXEL(&pixels[i], 255,255,255,255, x, y);
        } else {
            CHECK_PIXEL(&pixels[i], 51,102,153,255, x, y);
        }
    }
    texture_destroy(texture);

    graphics_finish_frame();
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_window_restores_display_mode)
{
    if (!graphics_has_windowed_mode()) {
        SKIP("This platform does not support windowed mode.");
    }

    const int orig_width = graphics_device_width();
    const int orig_height = graphics_device_height();
    int width, height;
    if (!get_alternate_video_mode(&width, &height)) {
        SKIP("No alternate video mode available.");
    }

    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(width, height, NULL));
    CHECK_FALSE(graphics_display_is_window());
    CHECK_TRUE(graphics_set_display_attr("window", 1));
    CHECK_TRUE(graphics_set_display_mode(orig_width, orig_height, NULL));
    CHECK_TRUE(graphics_display_is_window());
    int new_width, new_height;
    if (!get_display_resolution(&new_width, &new_height)) {
        FAIL("Don't know how to get display resolution on this platform");
    }
    CHECK_INTEQUAL(new_width, orig_width);
    CHECK_INTEQUAL(new_height, orig_height);

    /* Also check with a same-sized window since that may take a different
     * code path. */
    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(width, height, NULL));
    CHECK_FALSE(graphics_display_is_window());
    CHECK_TRUE(graphics_set_display_attr("window", 1));
    CHECK_TRUE(graphics_set_display_mode(width, height, NULL));
    CHECK_TRUE(graphics_display_is_window());
    if (!get_display_resolution(&new_width, &new_height)) {
        FAIL("Don't know how to get display resolution on this platform");
    }
    CHECK_INTEQUAL(new_width, orig_width);
    CHECK_INTEQUAL(new_height, orig_height);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_enable_debug_sync)
{
    ASSERT(open_window(TESTW, TESTH));

    /* Make sure the counter works correctly. */
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_INTEQUAL(TEST_graphics_sync_count, 0);
    graphics_sync();
    CHECK_INTEQUAL(TEST_graphics_sync_count, 1);

    /* Debug sync should be disabled by default. */
    graphics_finish_frame();
    CHECK_INTEQUAL(TEST_graphics_sync_count, 1);

    /* Check that debug sync can be enabled. */
    graphics_enable_debug_sync(1);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
#if defined(SIL_PLATFORM_PSP) && defined(SIL_PLATFORM_PSP_GPU_WAIT_ON_FINISH)
    /* If PSP_GPU_WAIT_ON_FINISH, the low-level code will always sync, so
     * we don't sync separately and we don't see an increase in the sync
     * count here. */
    CHECK_INTEQUAL(TEST_graphics_sync_count, 1);
#else
    CHECK_INTEQUAL(TEST_graphics_sync_count, 2);
#endif

    /* Check that debug sync can be disabled again. */
    graphics_enable_debug_sync(0);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
#if defined(SIL_PLATFORM_PSP) && defined(SIL_PLATFORM_PSP_GPU_WAIT_ON_FINISH)
    CHECK_INTEQUAL(TEST_graphics_sync_count, 1);
#else
    CHECK_INTEQUAL(TEST_graphics_sync_count, 2);
#endif

    return 1;
}

/*************************************************************************/
/*************************************************************************/
