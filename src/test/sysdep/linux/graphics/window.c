/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/linux/graphics/window.c: Linux-specific graphics tests
 * covering window handling.
 */

#include "src/base.h"
#include "src/graphics.h"
#include "src/sysdep.h"
#include "src/sysdep/linux/internal.h"
#include "src/sysdep/opengl/dyngl.h"
#include "src/sysdep/opengl/opengl.h"
#include "src/test/base.h"
#include "src/test/graphics/internal.h"
#include "src/test/sysdep/linux/graphics/internal.h"
#include "src/test/sysdep/linux/wrap-x11.h"
#include "src/texture.h"

#include <time.h>
#include <GL/glx.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xinerama.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/xf86vmode.h>

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_linux_graphics_window)

/*-----------------------------------------------------------------------*/

TEST_INIT(init)
{
    clear_variables();
    if (!strstr(CURRENT_TEST_NAME(), "_NOINIT")) {
        CHECK_TRUE(graphics_init());
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
    if (!strstr(CURRENT_TEST_NAME(), "_NOCLEANUP")) {
        graphics_cleanup();
    }
    CHECK_INTEQUAL(linux_x11_get_error(), 0);
    clear_variables();
    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_create_blank_cursor_NOINIT)
{
    disable_XCreateBitmapFromData = 1;
    CHECK_TRUE(graphics_init());
    disable_XCreateBitmapFromData = 0;
    CHECK_TRUE(graphics_set_display_mode(TESTW, TESTH, NULL));
    graphics_show_mouse_pointer(0);
    CHECK_TRUE(graphics_get_mouse_pointer_state());

    graphics_cleanup();
    disable_XCreatePixmapCursor = 1;
    CHECK_TRUE(graphics_init());
    disable_XCreatePixmapCursor = 0;
    CHECK_TRUE(graphics_set_display_mode(TESTW, TESTH, NULL));
    graphics_show_mouse_pointer(0);
    CHECK_TRUE(graphics_get_mouse_pointer_state());

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_window_title_memory_failure)
{
    CHECK_TRUE(graphics_set_display_mode(TESTW, TESTH, NULL));

    char *name;
    CHECK_MEMORY_FAILURES(
        (graphics_set_window_title("SIL Test"),
         name = NULL,
         XFetchName(linux_x11_display(), linux_x11_window(), &name),
         (name && strcmp(name, "SIL Test") == 0) || (XFree(name), 0)));
    XFree(name);
    name = NULL;
    XGetIconName(linux_x11_display(), linux_x11_window(), &name);
    CHECK_STREQUAL(name, "SIL Test");
    XFree(name);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_window_icon_memory_failure)
{
    Display *display = linux_x11_display();
    Atom net_wm_icon = XInternAtom(display, "_NET_WM_ICON", True);
    if (!net_wm_icon) {
        SKIP("_NET_WM_ICON atom not found.");
    }

    CHECK_TRUE(graphics_set_display_mode(TESTW, TESTH, NULL));

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

    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop;
    CHECK_MEMORY_FAILURES(
        (graphics_set_window_icon(texture),
         (XGetWindowProperty(display, linux_x11_window(), net_wm_icon,
                             0, 2+32*32, False, AnyPropertyType, &actual_type,
                             &actual_format, &nitems, &bytes_after, &prop)
          == Success && nitems == 2+32*32)));
    CHECK_INTEQUAL(actual_type, XA_CARDINAL);
    CHECK_INTEQUAL(actual_format, 32);
    CHECK_INTEQUAL(bytes_after, 0);
    const long *icon_data = (long *)(void *)prop;
    CHECK_INTEQUAL(icon_data[0], 32);
    CHECK_INTEQUAL(icon_data[1], 32);
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            const int r = x<<3, g = y, b = x<<3 ^ y, a = (y*32+x)/4;
            const long expected = a<<24 | r<<16 | g<<8 | b;
            if (icon_data[2+(y*32+x)] != expected) {
                FAIL("icon_data[%d] was 0x%08lX but should have been 0x%08lX",
                     2+(y*32+x), icon_data[2+(y*32+x)], expected);
            }
        }
    }
    XFree(prop);

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

TEST(test_set_window_icon_early)
{
    Display *display = linux_x11_display();
    Atom net_wm_icon = XInternAtom(display, "_NET_WM_ICON", True);
    if (!net_wm_icon) {
        SKIP("_NET_WM_ICON atom not found.");
    }

    CHECK_TRUE(graphics_set_display_mode(TESTW, TESTH, NULL));

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
    texture_destroy(texture);

    linux_close_window();
    CHECK_TRUE(graphics_set_display_mode(TESTW, TESTH, NULL));
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop;
    CHECK_INTEQUAL(XGetWindowProperty(display, linux_x11_window(), net_wm_icon,
                                      0, 2+32*32, False, AnyPropertyType,
                                      &actual_type, &actual_format, &nitems,
                                      &bytes_after, &prop), Success);
    CHECK_INTEQUAL(actual_type, XA_CARDINAL);
    CHECK_INTEQUAL(actual_format, 32);
    CHECK_INTEQUAL(nitems, 2+32*32);
    CHECK_INTEQUAL(bytes_after, 0);
    const long *icon_data = (long *)(void *)prop;
    CHECK_INTEQUAL(icon_data[0], 32);
    CHECK_INTEQUAL(icon_data[1], 32);
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            const int r = x<<3, g = y, b = x<<3 ^ y, a = (y*32+x)/4;
            const long expected = a<<24 | r<<16 | g<<8 | b;
            if (icon_data[2+(y*32+x)] != expected) {
                FAIL("icon_data[%d] was 0x%08lX but should have been 0x%08lX",
                     2+(y*32+x), icon_data[2+(y*32+x)], expected);
            }
        }
    }
    XFree(prop);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_XCreateColormap_failure)
{
    disable_XCreateColormap = 1;
    GraphicsError error;
    CHECK_FALSE(graphics_set_display_mode(TESTW, TESTH, &error));
    disable_XCreateColormap = 0;
    CHECK_INTEQUAL(error, GRAPHICS_ERROR_MODE_NOT_SUPPORTED);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_XCreateWindow_failure)
{
    disable_XCreateWindow = 1;
    GraphicsError error;
    CHECK_FALSE(graphics_set_display_mode(TESTW, TESTH, &error));
    disable_XCreateWindow = 0;
    CHECK_INTEQUAL(error, GRAPHICS_ERROR_MODE_NOT_SUPPORTED);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_XCreateWindow_error)
{
    error_XCreateWindow = 1;
    GraphicsError error;
    CHECK_FALSE(graphics_set_display_mode(TESTW, TESTH, &error));
    error_XCreateWindow = 0;
    CHECK_INTEQUAL(error, GRAPHICS_ERROR_MODE_NOT_SUPPORTED);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_glXQueryExtension_failure_NOINIT)
{
    disable_glXQueryExtension = 1;
    CHECK_TRUE(graphics_init());
    GraphicsError error;
    CHECK_FALSE(graphics_set_display_mode(TESTW, TESTH, &error));
    disable_glXCreateWindow = 0;
    CHECK_INTEQUAL(error, GRAPHICS_ERROR_BACKEND_NOT_FOUND);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_glXCreateWindow_failure)
{
    disable_glXCreateWindow = 1;
    GraphicsError error;
    CHECK_FALSE(graphics_set_display_mode(TESTW, TESTH, &error));
    disable_glXCreateWindow = 0;
    CHECK_INTEQUAL(error, GRAPHICS_ERROR_MODE_NOT_SUPPORTED);
    return 1;
}

/*-----------------------------------------------------------------------*/

#if 0  // FIXME: currently broken if glXCreateContextAttribsARB() is present
TEST(test_glXCreateNewContext_failure)
{
    disable_glXCreateNewContext = 1;
    GraphicsError error;
    CHECK_FALSE(graphics_set_display_mode(TESTW, TESTH, &error));
    disable_glXCreateNewContext = 0;
    CHECK_INTEQUAL(error, GRAPHICS_ERROR_MODE_NOT_SUPPORTED);
    return 1;
}
#endif

/*-----------------------------------------------------------------------*/

TEST(test_glXMakeContextCurrent_failure)
{
    disable_glXMakeContextCurrent = 1;
    GraphicsError error;
    CHECK_FALSE(graphics_set_display_mode(TESTW, TESTH, &error));
    disable_glXMakeContextCurrent = 0;
    CHECK_INTEQUAL(error, GRAPHICS_ERROR_MODE_NOT_SUPPORTED);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_resize_window_noncentered)
{
    CHECK_TRUE(graphics_set_display_attr("center_window", 0));
    CHECK_TRUE(graphics_set_display_mode(TESTW, TESTH, NULL));

    called_XCreateWindow = 0;
    called_XMoveResizeWindow = 0;
    called_XResizeWindow = 0;
    CHECK_TRUE(graphics_set_display_mode(TESTW+64, TESTH+64, NULL));
    CHECK_INTEQUAL(called_XCreateWindow, 0);
    CHECK_INTEQUAL(called_XMoveResizeWindow, 0);
    CHECK_INTEQUAL(called_XResizeWindow, 1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_resize_window_centered)
{
    CHECK_TRUE(graphics_set_display_attr("center_window", 1));
    CHECK_TRUE(graphics_set_display_mode(TESTW, TESTH, NULL));

    called_XCreateWindow = 0;
    called_XMoveResizeWindow = 0;
    called_XResizeWindow = 0;
    CHECK_TRUE(graphics_set_display_mode(TESTW+64, TESTH-32, NULL));
    CHECK_INTEQUAL(called_XCreateWindow, 0);
    CHECK_INTEQUAL(called_XMoveResizeWindow, 1);
    CHECK_INTEQUAL(called_XResizeWindow, 0);
    /* Give the window manager a chance to handle the move request. */
    XSync(linux_x11_display(), False);
    nanosleep(&(struct timespec){0, 100*1000*1000}, NULL);
    int x1, y1;
    Window unused_child;
    XTranslateCoordinates(linux_x11_display(), linux_x11_window(),
                          RootWindow(linux_x11_display(), linux_x11_screen()),
                          0, 0, &x1, &y1, &unused_child);

    /* Check coordinates separately since window managers may place windows
     * slightly differently at creation time vs. with XMoveResize(). */
    called_XCreateWindow = 0;
    called_XMoveResizeWindow = 0;
    called_XResizeWindow = 0;
    CHECK_TRUE(graphics_set_display_mode(TESTW, TESTH, NULL));
    CHECK_INTEQUAL(called_XCreateWindow, 0);
    CHECK_INTEQUAL(called_XMoveResizeWindow, 1);
    CHECK_INTEQUAL(called_XResizeWindow, 0);
    XSync(linux_x11_display(), False);
    nanosleep(&(struct timespec){0, 100*1000*1000}, NULL);
    int x2, y2;
    XTranslateCoordinates(linux_x11_display(), linux_x11_window(),
                          RootWindow(linux_x11_display(), linux_x11_screen()),
                          0, 0, &x2, &y2, &unused_child);
    CHECK_INTEQUAL(x2, x1+32);
    CHECK_INTEQUAL(y2, y1-16);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_resize_window_error)
{
    CHECK_TRUE(graphics_set_display_attr("center_window", 1));
    CHECK_TRUE(graphics_set_display_mode(TESTW, TESTH, NULL));

    called_XCreateWindow = 0;
    called_XMoveResizeWindow = 0;
    called_XResizeWindow = 0;
    error_XMoveResizeWindow = 1;
    CHECK_TRUE(graphics_set_display_mode(TESTW+64, TESTH+64, NULL));
    error_XMoveResizeWindow = 0;
    CHECK_INTEQUAL(called_XCreateWindow, 1);
    CHECK_INTEQUAL(called_XMoveResizeWindow, 1);
    CHECK_INTEQUAL(called_XResizeWindow, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_resize_window_to_fullscreen)
{
    CHECK_TRUE(graphics_set_display_mode(TESTW, TESTH, NULL));

    called_XCreateWindow = 0;
    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(graphics_device_width(),
                                         graphics_device_height(), NULL));
    CHECK_INTEQUAL(called_XCreateWindow, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_resize_window_from_fullscreen)
{
    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(graphics_device_width(),
                                         graphics_device_height(), NULL));

    called_XCreateWindow = 0;
    CHECK_TRUE(graphics_set_display_attr("window", 1));
    CHECK_TRUE(graphics_set_display_mode(TESTW, TESTH, NULL));
    CHECK_INTEQUAL(called_XCreateWindow, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_resize_window_different_gl_config)
{
    CHECK_TRUE(graphics_set_display_attr("depth_bits", 0));
    CHECK_TRUE(graphics_set_display_attr("stencil_bits", 0));
    CHECK_TRUE(graphics_set_display_mode(TESTW, TESTH, NULL));

    called_XCreateWindow = 0;
    CHECK_TRUE(graphics_set_display_attr("depth_bits", 16));
    CHECK_TRUE(graphics_set_display_attr("stencil_bits", 8));
    CHECK_TRUE(graphics_set_display_mode(TESTW, TESTH, NULL));
    CHECK_INTEQUAL(called_XCreateWindow, 1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_window_resizable)
{
    graphics_set_display_attr("window_resizable", 1);
    CHECK_TRUE(graphics_set_display_mode(300, 150, NULL));

    XSizeHints hints;
    long dummy;
    XGetWMNormalHints(linux_x11_display(), linux_x11_window(), &hints, &dummy);
    CHECK_INTEQUAL(hints.flags, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_window_resize_limits)
{
    graphics_set_display_attr("window_resizable", 1);
    graphics_set_window_resize_limits(200,100, 800,400, 3,2, 9,4);
    CHECK_TRUE(graphics_set_display_mode(300, 150, NULL));

    XSizeHints hints;
    long dummy;
    XGetWMNormalHints(linux_x11_display(), linux_x11_window(), &hints, &dummy);
    CHECK_INTEQUAL(hints.flags, PMinSize | PMaxSize | PAspect);
    CHECK_INTEQUAL(hints.min_width, 200);
    CHECK_INTEQUAL(hints.min_height, 100);
    CHECK_INTEQUAL(hints.max_width, 800);
    CHECK_INTEQUAL(hints.max_height, 400);
    CHECK_INTEQUAL(hints.min_aspect.x, 3);
    CHECK_INTEQUAL(hints.min_aspect.y, 2);
    CHECK_INTEQUAL(hints.max_aspect.x, 9);
    CHECK_INTEQUAL(hints.max_aspect.y, 4);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_window_resize_limits_not_resizable)
{
    graphics_set_window_resize_limits(200,100, 800,400, 3,2, 9,4);
    CHECK_TRUE(graphics_set_display_mode(300, 150, NULL));

    XSizeHints hints;
    long dummy;
    XGetWMNormalHints(linux_x11_display(), linux_x11_window(), &hints, &dummy);
    CHECK_INTEQUAL(hints.flags, PMinSize | PMaxSize);
    CHECK_INTEQUAL(hints.min_width, 300);
    CHECK_INTEQUAL(hints.min_height, 150);
    CHECK_INTEQUAL(hints.max_width, 300);
    CHECK_INTEQUAL(hints.max_height, 150);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_window_resize_limits_fullscreen)
{
    const int width = graphics_device_width();
    const int height = graphics_device_height();

    graphics_set_display_attr("window_resizable", 1);
    graphics_set_window_resize_limits(200,100, 800,400, 3,2, 9,4);
    graphics_set_display_attr("window", 0);
    CHECK_TRUE(graphics_set_display_mode(width, height, NULL));

    XSizeHints hints;
    long dummy;
    XGetWMNormalHints(linux_x11_display(), linux_x11_window(), &hints, &dummy);
    CHECK_INTEQUAL(hints.flags, PMinSize | PMaxSize);
    CHECK_INTEQUAL(hints.min_width, width);
    CHECK_INTEQUAL(hints.min_height, height);
    CHECK_INTEQUAL(hints.max_width, width);
    CHECK_INTEQUAL(hints.max_height, height);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_window_resize_limits_after_create)
{
    CHECK_TRUE(graphics_set_display_mode(300, 150, NULL));

    graphics_set_display_attr("window_resizable", 1);
    graphics_set_window_resize_limits(200,100, 800,400, 3,2, 9,4);

    XSizeHints hints;
    long dummy;
    XGetWMNormalHints(linux_x11_display(), linux_x11_window(), &hints, &dummy);
    CHECK_INTEQUAL(hints.flags, PMinSize | PMaxSize | PAspect);
    CHECK_INTEQUAL(hints.min_width, 200);
    CHECK_INTEQUAL(hints.min_height, 100);
    CHECK_INTEQUAL(hints.max_width, 800);
    CHECK_INTEQUAL(hints.max_height, 400);
    CHECK_INTEQUAL(hints.min_aspect.x, 3);
    CHECK_INTEQUAL(hints.min_aspect.y, 2);
    CHECK_INTEQUAL(hints.max_aspect.x, 9);
    CHECK_INTEQUAL(hints.max_aspect.y, 4);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_window_resize_limits_partial)
{
    graphics_set_display_attr("window_resizable", 1);
    graphics_set_window_resize_limits(200,100, 800,400, 3,2, 9,4);
    CHECK_TRUE(graphics_set_display_mode(300, 150, NULL));

    XSizeHints hints;
    long dummy;

    graphics_set_window_resize_limits(200,100, 0,0, 0,0, 0,0);
    XGetWMNormalHints(linux_x11_display(), linux_x11_window(), &hints, &dummy);
    CHECK_INTEQUAL(hints.flags, PMinSize);
    CHECK_INTEQUAL(hints.min_width, 200);
    CHECK_INTEQUAL(hints.min_height, 100);

    graphics_set_window_resize_limits(0,0, 800,400, 0,0, 0,0);
    XGetWMNormalHints(linux_x11_display(), linux_x11_window(), &hints, &dummy);
    CHECK_INTEQUAL(hints.flags, PMaxSize);
    CHECK_INTEQUAL(hints.max_width, 800);
    CHECK_INTEQUAL(hints.max_height, 400);

    graphics_set_window_resize_limits(0,0, 0,0, 3,2, 0,0);
    XGetWMNormalHints(linux_x11_display(), linux_x11_window(), &hints, &dummy);
    CHECK_INTEQUAL(hints.flags, PAspect);
    CHECK_INTEQUAL(hints.min_aspect.x, 3);
    CHECK_INTEQUAL(hints.min_aspect.y, 2);
    CHECK_INTEQUAL(hints.max_aspect.x, INT_MAX);
    CHECK_INTEQUAL(hints.max_aspect.y, 1);

    graphics_set_window_resize_limits(0,0, 0,0, 0,0, 9,4);
    XGetWMNormalHints(linux_x11_display(), linux_x11_window(), &hints, &dummy);
    CHECK_INTEQUAL(hints.flags, PAspect);
    CHECK_INTEQUAL(hints.min_aspect.x, 1);
    CHECK_INTEQUAL(hints.min_aspect.y, INT_MAX);
    CHECK_INTEQUAL(hints.max_aspect.x, 9);
    CHECK_INTEQUAL(hints.max_aspect.y, 4);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_window_resize_limits_invalid)
{
    graphics_set_display_attr("window_resizable", 1);
    graphics_set_window_resize_limits(200,100, 800,400, 3,2, 9,4);
    CHECK_TRUE(graphics_set_display_mode(300, 150, NULL));

    XSizeHints hints;
    long dummy;

    /* Invalid minimum size. */

    graphics_set_window_resize_limits(0,1, 800,400, 3,2, 9,4);
    XGetWMNormalHints(linux_x11_display(), linux_x11_window(), &hints, &dummy);
    CHECK_INTEQUAL(hints.flags, PMaxSize | PAspect);
    CHECK_INTEQUAL(hints.max_width, 800);
    CHECK_INTEQUAL(hints.max_height, 400);
    CHECK_INTEQUAL(hints.min_aspect.x, 3);
    CHECK_INTEQUAL(hints.min_aspect.y, 2);
    CHECK_INTEQUAL(hints.max_aspect.x, 9);
    CHECK_INTEQUAL(hints.max_aspect.y, 4);

    graphics_set_window_resize_limits(1,0, 800,400, 3,2, 9,4);
    XGetWMNormalHints(linux_x11_display(), linux_x11_window(), &hints, &dummy);
    CHECK_INTEQUAL(hints.flags, PMaxSize | PAspect);
    CHECK_INTEQUAL(hints.max_width, 800);
    CHECK_INTEQUAL(hints.max_height, 400);
    CHECK_INTEQUAL(hints.min_aspect.x, 3);
    CHECK_INTEQUAL(hints.min_aspect.y, 2);
    CHECK_INTEQUAL(hints.max_aspect.x, 9);
    CHECK_INTEQUAL(hints.max_aspect.y, 4);

    graphics_set_window_resize_limits(-1,-1, 800,400, 3,2, 9,4);
    XGetWMNormalHints(linux_x11_display(), linux_x11_window(), &hints, &dummy);
    CHECK_INTEQUAL(hints.flags, PMaxSize | PAspect);
    CHECK_INTEQUAL(hints.max_width, 800);
    CHECK_INTEQUAL(hints.max_height, 400);
    CHECK_INTEQUAL(hints.min_aspect.x, 3);
    CHECK_INTEQUAL(hints.min_aspect.y, 2);
    CHECK_INTEQUAL(hints.max_aspect.x, 9);
    CHECK_INTEQUAL(hints.max_aspect.y, 4);

    /* Invalid maximum size. */

    graphics_set_window_resize_limits(200,100, 0,1, 3,2, 9,4);
    XGetWMNormalHints(linux_x11_display(), linux_x11_window(), &hints, &dummy);
    CHECK_INTEQUAL(hints.flags, PMinSize | PAspect);
    CHECK_INTEQUAL(hints.min_width, 200);
    CHECK_INTEQUAL(hints.min_height, 100);
    CHECK_INTEQUAL(hints.min_aspect.x, 3);
    CHECK_INTEQUAL(hints.min_aspect.y, 2);
    CHECK_INTEQUAL(hints.max_aspect.x, 9);
    CHECK_INTEQUAL(hints.max_aspect.y, 4);

    graphics_set_window_resize_limits(200,100, 1,0, 3,2, 9,4);
    XGetWMNormalHints(linux_x11_display(), linux_x11_window(), &hints, &dummy);
    CHECK_INTEQUAL(hints.flags, PMinSize | PAspect);
    CHECK_INTEQUAL(hints.min_width, 200);
    CHECK_INTEQUAL(hints.min_height, 100);
    CHECK_INTEQUAL(hints.min_aspect.x, 3);
    CHECK_INTEQUAL(hints.min_aspect.y, 2);
    CHECK_INTEQUAL(hints.max_aspect.x, 9);
    CHECK_INTEQUAL(hints.max_aspect.y, 4);

    graphics_set_window_resize_limits(200,100, -1,-1, 3,2, 9,4);
    XGetWMNormalHints(linux_x11_display(), linux_x11_window(), &hints, &dummy);
    CHECK_INTEQUAL(hints.flags, PMinSize | PAspect);
    CHECK_INTEQUAL(hints.min_width, 200);
    CHECK_INTEQUAL(hints.min_height, 100);
    CHECK_INTEQUAL(hints.min_aspect.x, 3);
    CHECK_INTEQUAL(hints.min_aspect.y, 2);
    CHECK_INTEQUAL(hints.max_aspect.x, 9);
    CHECK_INTEQUAL(hints.max_aspect.y, 4);

    /* Invalid minimum aspect. */

    graphics_set_window_resize_limits(200,100, 800,400, 0,1, 9,4);
    XGetWMNormalHints(linux_x11_display(), linux_x11_window(), &hints, &dummy);
    CHECK_INTEQUAL(hints.flags, PMinSize | PMaxSize | PAspect);
    CHECK_INTEQUAL(hints.min_width, 200);
    CHECK_INTEQUAL(hints.min_height, 100);
    CHECK_INTEQUAL(hints.max_width, 800);
    CHECK_INTEQUAL(hints.max_height, 400);
    CHECK_INTEQUAL(hints.min_aspect.x, 1);
    CHECK_INTEQUAL(hints.min_aspect.y, INT_MAX);
    CHECK_INTEQUAL(hints.max_aspect.x, 9);
    CHECK_INTEQUAL(hints.max_aspect.y, 4);

    graphics_set_window_resize_limits(200,100, 800,400, 1,0, 9,4);
    XGetWMNormalHints(linux_x11_display(), linux_x11_window(), &hints, &dummy);
    CHECK_INTEQUAL(hints.flags, PMinSize | PMaxSize | PAspect);
    CHECK_INTEQUAL(hints.min_width, 200);
    CHECK_INTEQUAL(hints.min_height, 100);
    CHECK_INTEQUAL(hints.max_width, 800);
    CHECK_INTEQUAL(hints.max_height, 400);
    CHECK_INTEQUAL(hints.min_aspect.x, 1);
    CHECK_INTEQUAL(hints.min_aspect.y, INT_MAX);
    CHECK_INTEQUAL(hints.max_aspect.x, 9);
    CHECK_INTEQUAL(hints.max_aspect.y, 4);

    graphics_set_window_resize_limits(200,100, 800,400, -1,-1, 9,4);
    XGetWMNormalHints(linux_x11_display(), linux_x11_window(), &hints, &dummy);
    CHECK_INTEQUAL(hints.flags, PMinSize | PMaxSize | PAspect);
    CHECK_INTEQUAL(hints.min_width, 200);
    CHECK_INTEQUAL(hints.min_height, 100);
    CHECK_INTEQUAL(hints.max_width, 800);
    CHECK_INTEQUAL(hints.max_height, 400);
    CHECK_INTEQUAL(hints.min_aspect.x, 1);
    CHECK_INTEQUAL(hints.min_aspect.y, INT_MAX);
    CHECK_INTEQUAL(hints.max_aspect.x, 9);
    CHECK_INTEQUAL(hints.max_aspect.y, 4);

    /* Invalid maximum aspect. */

    graphics_set_window_resize_limits(200,100, 800,400, 3,2, 0,1);
    XGetWMNormalHints(linux_x11_display(), linux_x11_window(), &hints, &dummy);
    CHECK_INTEQUAL(hints.flags, PMinSize | PMaxSize | PAspect);
    CHECK_INTEQUAL(hints.min_width, 200);
    CHECK_INTEQUAL(hints.min_height, 100);
    CHECK_INTEQUAL(hints.max_width, 800);
    CHECK_INTEQUAL(hints.max_height, 400);
    CHECK_INTEQUAL(hints.min_aspect.x, 3);
    CHECK_INTEQUAL(hints.min_aspect.y, 2);
    CHECK_INTEQUAL(hints.max_aspect.x, INT_MAX);
    CHECK_INTEQUAL(hints.max_aspect.y, 1);

    graphics_set_window_resize_limits(200,100, 800,400, 3,2, 1,0);
    XGetWMNormalHints(linux_x11_display(), linux_x11_window(), &hints, &dummy);
    CHECK_INTEQUAL(hints.flags, PMinSize | PMaxSize | PAspect);
    CHECK_INTEQUAL(hints.min_width, 200);
    CHECK_INTEQUAL(hints.min_height, 100);
    CHECK_INTEQUAL(hints.max_width, 800);
    CHECK_INTEQUAL(hints.max_height, 400);
    CHECK_INTEQUAL(hints.min_aspect.x, 3);
    CHECK_INTEQUAL(hints.min_aspect.y, 2);
    CHECK_INTEQUAL(hints.max_aspect.x, INT_MAX);
    CHECK_INTEQUAL(hints.max_aspect.y, 1);

    graphics_set_window_resize_limits(200,100, 800,400, 3,2, -1,-1);
    XGetWMNormalHints(linux_x11_display(), linux_x11_window(), &hints, &dummy);
    CHECK_INTEQUAL(hints.flags, PMinSize | PMaxSize | PAspect);
    CHECK_INTEQUAL(hints.min_width, 200);
    CHECK_INTEQUAL(hints.min_height, 100);
    CHECK_INTEQUAL(hints.max_width, 800);
    CHECK_INTEQUAL(hints.max_height, 400);
    CHECK_INTEQUAL(hints.min_aspect.x, 3);
    CHECK_INTEQUAL(hints.min_aspect.y, 2);
    CHECK_INTEQUAL(hints.max_aspect.x, INT_MAX);
    CHECK_INTEQUAL(hints.max_aspect.y, 1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fullscreen_to_resizable_window)
{
    const int width = graphics_device_width();
    const int height = graphics_device_height();
    ASSERT(!(width == 300 && height == 150));  // Avoid false positives.

    graphics_set_display_attr("window_resizable", 1);
    graphics_set_window_resize_limits(200,100, 800,400, 3,2, 9,4);
    graphics_set_display_attr("window", 0);
    CHECK_TRUE(graphics_set_display_mode(width, height, NULL));

    graphics_set_display_attr("window", 1);
    CHECK_TRUE(graphics_set_display_mode(300, 150, NULL));
    /* Give the window manager a chance to respond to the window resize. */
    for (int i = 0; i < 3; i++) {
        nanosleep(&(struct timespec){0, 10000000}, NULL);  // 10ms
        graphics_start_frame();
        graphics_finish_frame();
    }

    XWindowAttributes attr;
    ASSERT(XGetWindowAttributes(linux_x11_display(), linux_x11_window(),
                                &attr));
    CHECK_INTEQUAL(attr.width, 300);
    CHECK_INTEQUAL(attr.height, 150);

    XSizeHints hints;
    long dummy;
    XGetWMNormalHints(linux_x11_display(), linux_x11_window(), &hints, &dummy);
    CHECK_INTEQUAL(hints.flags, PMinSize | PMaxSize | PAspect);
    CHECK_INTEQUAL(hints.min_width, 200);
    CHECK_INTEQUAL(hints.min_height, 100);
    CHECK_INTEQUAL(hints.max_width, 800);
    CHECK_INTEQUAL(hints.max_height, 400);
    CHECK_INTEQUAL(hints.min_aspect.x, 3);
    CHECK_INTEQUAL(hints.min_aspect.y, 2);
    CHECK_INTEQUAL(hints.max_aspect.x, 9);
    CHECK_INTEQUAL(hints.max_aspect.y, 4);

    return 1;
}

/*-----------------------------------------------------------------------*/

/* This test can fail depending on window manager behavior. */
TEST(test_window_position_preserved_after_fullscreen)
{
    int x, y;
    CHECK_TRUE(graphics_set_display_mode(TESTH, TESTW, NULL));
    graphics_start_frame();
    graphics_finish_frame();
    ASSERT(XTranslateCoordinates(linux_x11_display(), linux_x11_window(),
                                 RootWindow(linux_x11_display(), linux_x11_screen()),
                                 0, 0, &x, &y, (Window[1]){0}));

    int new_x, new_y;
    XMoveWindow(linux_x11_display(), linux_x11_window(), x+1, y-2);
    XSync(linux_x11_display(), False);
    graphics_start_frame();
    graphics_finish_frame();
    nanosleep(&(struct timespec){0, 100*1000*1000}, NULL);
    ASSERT(XTranslateCoordinates(linux_x11_display(), linux_x11_window(),
                                 RootWindow(linux_x11_display(), linux_x11_screen()),
                                 0, 0, &new_x, &new_y, (Window[1]){0}));
    CHECK_INTEQUAL(new_x, x+1);
    CHECK_INTEQUAL(new_y, y-2);

    CHECK_TRUE(graphics_set_display_attr("window", 0));
    CHECK_TRUE(graphics_set_display_mode(
                   graphics_device_width(), graphics_device_height(), NULL));

    CHECK_TRUE(graphics_set_display_attr("window", 1));
    CHECK_TRUE(graphics_set_display_mode(TESTH, TESTW, NULL));
    graphics_start_frame();
    graphics_finish_frame();
    nanosleep(&(struct timespec){0, 100*1000*1000}, NULL);
    ASSERT(XTranslateCoordinates(linux_x11_display(), linux_x11_window(),
                                 RootWindow(linux_x11_display(), linux_x11_screen()),
                                 0, 0, &new_x, &new_y, (Window[1]){0}));
    CHECK_INTEQUAL(new_x, x+1);
    CHECK_INTEQUAL(new_y, y-2);

    return 1;
}

/*-----------------------------------------------------------------------*/

/* This test can fail depending on window manager behavior. */
TEST(test_window_position_preserved_after_close)
{
    int x, y;
    CHECK_TRUE(graphics_set_display_mode(TESTH, TESTW, NULL));
    graphics_start_frame();
    graphics_finish_frame();
    ASSERT(XTranslateCoordinates(linux_x11_display(), linux_x11_window(),
                                 RootWindow(linux_x11_display(), linux_x11_screen()),
                                 0, 0, &x, &y, (Window[1]){0}));

    int new_x, new_y;
    XMoveWindow(linux_x11_display(), linux_x11_window(), x+1, y-2);
    XSync(linux_x11_display(), False);
    graphics_start_frame();
    graphics_finish_frame();
    nanosleep(&(struct timespec){0, 100*1000*1000}, NULL);
    ASSERT(XTranslateCoordinates(linux_x11_display(), linux_x11_window(),
                                 RootWindow(linux_x11_display(), linux_x11_screen()),
                                 0, 0, &new_x, &new_y, (Window[1]){0}));
    CHECK_INTEQUAL(new_x, x+1);
    CHECK_INTEQUAL(new_y, y-2);

    linux_close_window();
    CHECK_TRUE(graphics_set_display_mode(TESTH, TESTW, NULL));
    graphics_start_frame();
    graphics_finish_frame();
    nanosleep(&(struct timespec){0, 100*1000*1000}, NULL);
    ASSERT(XTranslateCoordinates(linux_x11_display(), linux_x11_window(),
                                 RootWindow(linux_x11_display(), linux_x11_screen()),
                                 0, 0, &new_x, &new_y, (Window[1]){0}));
    CHECK_INTEQUAL(new_x, x+1);
    CHECK_INTEQUAL(new_y, y-2);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_window_focus)
{
    CHECK_TRUE(graphics_set_display_mode(TESTW, TESTH, NULL));
    Display *display = linux_x11_display();
    Window window = linux_x11_window();

    XSetInputFocus(display, window, RevertToNone, CurrentTime);
    XSync(display, False);
    while (XPending(display)) {
        XEvent unused_event;
        (void) linux_get_window_event(&unused_event);
    }
    CHECK_TRUE(graphics_has_focus());

    XSetInputFocus(display, None, RevertToNone, CurrentTime);
    XSync(display, False);
    while (XPending(display)) {
        XEvent unused_event;
        (void) linux_get_window_event(&unused_event);
    }
    CHECK_FALSE(graphics_has_focus());

    return 1;
}

/*************************************************************************/
/*************************************************************************/
