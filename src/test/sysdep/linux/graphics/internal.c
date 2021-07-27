/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/linux/graphics/internal.c: Helper functions for
 * Linux-specific graphics tests.
 */

#include "src/base.h"
#include "src/graphics.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep/linux/internal.h"
#include "src/test/base.h"
#include "src/test/sysdep/linux/graphics/internal.h"
#include "src/test/sysdep/linux/wrap-x11.h"
#include "src/thread.h"

#include <dlfcn.h>
#define RTLD_DEFAULT  NULL
#define RTLD_NEXT     ((void *)(intptr_t)-1)

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xinerama.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/xf86vmode.h>

/*************************************************************************/
/*************************************************************************/

int check_vidmode(void)
{
    __typeof__(XF86VidModeQueryExtension) *p_XF86VidModeQueryExtension =
        dlsym(RTLD_NEXT, "XF86VidModeQueryExtension");
    int event_base, error_base;
    return p_XF86VidModeQueryExtension
        && (*p_XF86VidModeQueryExtension)(linux_x11_display(),
                                          &event_base, &error_base);
}

/*-----------------------------------------------------------------------*/

int check_xinerama(void)
{
    __typeof__(XineramaQueryExtension) *p_XineramaQueryExtension =
        dlsym(RTLD_NEXT, "XineramaQueryExtension");
    __typeof__(XineramaIsActive) *p_XineramaIsActive =
        dlsym(RTLD_NEXT, "XineramaIsActive");
    int event_base, error_base;
    return p_XineramaQueryExtension
        && p_XineramaIsActive
        && (*p_XineramaQueryExtension)(linux_x11_display(),
                                       &event_base, &error_base)
        && (*p_XineramaIsActive)(linux_x11_display());
}

/*-----------------------------------------------------------------------*/

int check_xrandr(void)
{
    __typeof__(XRRQueryExtension) *p_XRRQueryExtension =
        dlsym(RTLD_NEXT, "XRRQueryExtension");
    __typeof__(XRRQueryVersion) *p_XRRQueryVersion =
        dlsym(RTLD_NEXT, "XRRQueryVersion");
    int event_base, error_base, major, minor;
    return p_XRRQueryExtension
        && p_XRRQueryVersion
        && (*p_XRRQueryExtension)(linux_x11_display(), &event_base, &error_base)
        && (*p_XRRQueryVersion)(linux_x11_display(), &major, &minor)
        && (major >= 2 || (major == 1 && minor >= 2));
}

/*-----------------------------------------------------------------------*/

void clear_variables(void)
{
    ASSERT(unsetenv("SIL_X11_CREATE_FULLSCREEN") == 0);
    ASSERT(unsetenv("SIL_X11_EWMH_FULLSCREEN_MOVE_BEFORE") == 0);
    ASSERT(unsetenv("SIL_X11_EWMH_FULLSCREEN_RESIZE_AFTER") == 0);
    ASSERT(unsetenv("SIL_X11_FULLSCREEN_METHOD") == 0);
    ASSERT(unsetenv("SIL_X11_USE_TRANSIENT_FOR_HINT") == 0);
    ASSERT(unsetenv("SIL_X11_VIDEO_MODE_INTERFACE") == 0);
    ASSERT(unsetenv("SIL_X11_RESOURCE_CLASS") == 0);
    ASSERT(unsetenv("SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS") == 0);

    clear_x11_wrapper_variables();
}

/*-----------------------------------------------------------------------*/

void xrandr_get_current_resolution(int *width_ret, int *height_ret)
{
    PRECOND(width_ret);
    PRECOND(height_ret);
    PRECOND(linux_x11_window());

    /* We don't have wrappers for these, so we need to look them up. */
    __typeof__(XRRFreeScreenResources) *p_XRRFreeScreenResources =
        dlsym(RTLD_DEFAULT, "XRRFreeScreenResources");
    __typeof__(XRRFreeOutputInfo) *p_XRRFreeOutputInfo =
        dlsym(RTLD_DEFAULT, "XRRFreeOutputInfo");
    __typeof__(XRRFreeCrtcInfo) *p_XRRFreeCrtcInfo =
        dlsym(RTLD_DEFAULT, "XRRFreeCrtcInfo");
    PRECOND(p_XRRFreeScreenResources);
    PRECOND(p_XRRFreeOutputInfo);
    PRECOND(p_XRRFreeCrtcInfo);

    Display *display = linux_x11_display();
    Window root = RootWindow(display, linux_x11_screen());
    XRRScreenResources *screen_resources =
        XRRGetScreenResources(display, root);
    ASSERT(screen_resources);

    XRROutputInfo *output_info = NULL;
    for (int i = 0; i < screen_resources->noutput; i++) {
        XRROutputInfo *oi = XRRGetOutputInfo(display, screen_resources,
                                             screen_resources->outputs[i]);
        if (oi) {
            if (oi->connection != RR_Disconnected) {
                output_info = oi;
                break;
            }
            (*p_XRRFreeOutputInfo)(oi);
        }
    }
    ASSERT(output_info);

    XRRCrtcInfo *crtc_info = XRRGetCrtcInfo(display, screen_resources,
                                            output_info->crtc);
    ASSERT(crtc_info);

    XRRModeInfo *mode = NULL;
    for (int i = 0; i < output_info->nmode; i++) {
        if (screen_resources->modes[i].id == crtc_info->mode) {
            mode = &screen_resources->modes[i];
            break;
        }
    }
    ASSERT(mode);

    *width_ret = mode->width;
    *height_ret = mode->height;
    (*p_XRRFreeCrtcInfo)(crtc_info);
    (*p_XRRFreeOutputInfo)(output_info);
    (*p_XRRFreeScreenResources)(screen_resources);
}

/*************************************************************************/
/*************************************************************************/
