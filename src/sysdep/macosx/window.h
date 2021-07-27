/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/macosx/window.h: Header for the SILWindow utility functions.
 */

#ifndef SIL_SRC_SYSDEP_MACOSX_WINDOW_H
#define SIL_SRC_SYSDEP_MACOSX_WINDOW_H

#include <ApplicationServices/ApplicationServices.h>
#include <OpenGL/OpenGL.h>

/*************************************************************************/
/*************************************************************************/

/**
 * SILWindow_create:  Create (open) a new SILWindow.
 *
 * [Parameters]
 *     x, y: Desired window origin (lower-left corner) coordinates; ignored
 *         for fullscreen windows.
 *     width, height: Desired window size, in pixels.
 *     screen: Screen index on which to open the window (0 = default screen).
 *     fullscreen: True to create a fullscreen window, false to create a
 *         regular window.
 *     resizable: True if the window should be resizable in windowed mode.
 *     pixel_format: OpenGL pixel format to use.
 * [Return value]
 *     Newly created SILWindow instance, or NULL on error.
 */
extern void *SILWindow_create(int x, int y, int width, int height, int screen,
                              int fullscreen, int resizable,
                              CGLPixelFormatObj pixel_format);

/**
 * SILWindow_destroy:  Destroy (close) a SILWindow.
 *
 * [Parameters]
 *     window: Window to destroy (may be NULL).
 */
extern void SILWindow_destroy(void *window);

/**
 * SILWindow_frame:  Return the bounding rectangle of the given window in
 * CoreGraphics screen coordinates.
 *
 * [Parameters]
 *     window: Window to check.
 * [Return value]
 *     Bounds of window.
 */
extern CGRect SILWindow_frame(void *window);

/**
 * SILWindow_content_frame:  Return the bounding rectangle of the given
 * window's content frame in CoreGraphics screen coordinates.
 *
 * [Parameters]
 *     window: Window to check.
 * [Return value]
 *     Bounds of window's content frame.
 */
extern CGRect SILWindow_content_frame(void *window);

/**
 * SILWindow_is_moving:  Return whether the given window is currently
 * being moved by the user.
 *
 * [Parameters]
 *     window: Window to check.
 * [Return value]
 *     True if the window is being moved, false if not.
 */
extern int SILWindow_is_moving(void *window);

/**
 * SILWindow_has_focus:  Return whether the given window has input focus.
 *
 * [Parameters]
 *     window: Window to check.
 * [Return value]
 *     True if the window has input focus, false if not.
 */
extern int SILWindow_has_focus(void *window);

/**
 * SILWindow_set_title:  Set the title of a window.
 *
 * [Parameters]
 *     window: Window to set title of.
 *     title: New title string.
 */
extern void SILWindow_set_title(void *window, const char *title);

/**
 * SILWindow_get_title:  Return the title of the given window.  The
 * returned string should be freed with mem_free() when no longer needed.
 *
 * [Parameters]
 *     window: Window to retrieve title of.
 * [Return value]
 *     Title string, or NULL on error.
 */
extern char *SILWindow_get_title(void *window);

/**
 * SILWindow_set_fullscreen:  Set whether the given window should be
 * displayed as a fullscreen window, and optionally resize it.
 *
 * [Parameters]
 *     window: Window to operate on.
 *     fullscreen: True to display the window as a fullscreen window, false
 *         to display the window as a normal window.
 *     screen: Screen index on which to display the window (0 = default
 *         screen).
 *     width, height: New size for the window if switching out of fullscreen.
 * [Return value]
 *     True on success, false on error.
 */
extern int SILWindow_set_fullscreen(void *window, int fullscreen, int screen,
                                    int width, int height);

/**
 * SILWindow_is_fullscreen:  Return whether the given window is currently
 * displayed as a fullscreen window.
 *
 * [Parameters]
 *     window: Window to operate on.
 * [Return value]
 *     True if the window is currently displayed in fullscreen mode, false
 *     if not.
 */
extern int SILWindow_is_fullscreen(void *window);

/**
 * SILWindow_set_show_pointer:  Set whether to show the mouse pointer when
 * it is inside the given window.
 *
 * [Parameters]
 *     window: Window to operate on.
 *     show: True to show the pointer, false to hide it.
 */
extern void SILWindow_set_show_pointer(void *window, int show);

/**
 * SILWindow_resize:  Set the size of the given window's content area.
 *
 * [Parameters]
 *     window: Window to resize.
 *     width, height: New content area size, in pixels.
 * [Return value]
 *     True on success, false on error.
 */
extern int SILWindow_resize(void *window, int width, int height);

/**
 * SILWindow_set_resizable:  Set whether the given window should be
 * resizable in windowed mode.
 *
 * [Parameters]
 *     window: Window to operate on.
 *     resizable: True if the window should be resizable, false if not.
 */
extern void SILWindow_set_resizable(void *window_, int resizable);

/**
 * SILWindow_set_resize_limits:  Set the constraints on user-initiated
 * window resize operations.
 *
 * [Parameters]
 *     window: Window to operate on.
 *     min_width, min_height: Minimum allowable size for the window, or 0x0
 *         for no limit.
 *     max_width, max_height: Maximum allowable size for the window, or 0x0
 *         for no limit.
 *     min_aspect_x, min_aspect_y: Minimum allowable aspect ratio for the
 *         window, or 0/0 for no limit.
 *     max_aspect_x, max_aspect_y: Maximum allowable aspect ratio for the
 *         window, or 0/0 for no limit.
 */
extern void SILWindow_set_resize_limits(void *window_,
                                        int min_width, int min_height,
                                        int max_width, int max_height,
                                        int min_aspect_x, int min_aspect_y,
                                        int max_aspect_x, int max_aspect_y);

/**
 * SILWindow_update_gl_context:  Perform any updates to the GL context
 * which may be needed due to window movement or resizing.
 *
 * [Parameters]
 *     window: Window to update.
 */
extern void SILWindow_update_gl_context(void *window);

/**
 * SILWindow_create_gl_shader_compilation_context:  Create an OpenGL
 * context for the current thread which can be used to compile shaders.
 * The current thread must not already have an OpenGL context.
 *
 * [Parameters]
 *     window: Window to use for creating the GL context.
 * [Return value]
 *     True on success, false on error.
 */
extern int SILWindow_create_gl_shader_compilation_context(void *window);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_MACOSX_WINDOW_H
