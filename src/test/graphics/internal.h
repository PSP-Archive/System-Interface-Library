/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/graphics/internal.h: Declarations of helper functions for the
 * graphics subsystem tests.
 */

#ifndef SIL_SRC_TEST_GRAPHICS_INTERNAL_H
#define SIL_SRC_TEST_GRAPHICS_INTERNAL_H

/*************************************************************************/
/*************************************************************************/

/**
 * USES_GL:  Convenience #define indicating that the system uses OpenGL.
 * If defined, the sysdep/opengl/opengl.h header will also be included.
 */
#if defined(SIL_PLATFORM_ANDROID) || defined(SIL_PLATFORM_IOS) || defined(SIL_PLATFORM_LINUX) || defined(SIL_PLATFORM_MACOSX) || defined(SIL_PLATFORM_WINDOWS)
# define USES_GL
#endif

#ifdef USES_GL
# include "src/sysdep.h"
# include "src/sysdep/opengl/opengl.h"
#endif

/**
 * IMMEDIATE_RENDER_ALLOCS_MEMORY:  Convenience #define indicating that
 * the system allocates memory when performing immediate rendering of
 * primitives.  If this is _not_ defined, immediate primitive rendering
 * will succeed even when mem_alloc() is forced to fail.
 */
#ifdef USES_GL
# define IMMEDIATE_RENDER_ALLOCS_MEMORY
#endif

/*-----------------------------------------------------------------------*/

/**
 * TESTW, TESTH:  Window/viewport size used by run_tests_in_window() and
 * grab_display().
 */
#define TESTW  128
#define TESTH  96

/*-----------------------------------------------------------------------*/

/**
 * CHECK_COLORED_RECTANGLE:  Check that the display contains a rectangle
 * of the given size and color centered at the given position in a 64x64
 * viewport.
 *
 * [Parameters]
 *     w, h: Rectangle width and height, in pixels (assumed to be even).
 *     cx, cy: Rectangle center coordinates, in pixels.
 *     r, g, b: Rectangle color components.
 * [Return value]
 *     True if the display data is correct, false if not.
 */
/* This helper is defined in internal.c. */
extern int check_colored_rectangle(int w, int h, int cx, int cy,
                                   float r, float g, float b);
#define CHECK_COLORED_RECTANGLE(...) \
    CHECK_TRUE(check_colored_rectangle(__VA_ARGS__))

/**
 * CHECK_RECTANGLE:  Check that the display contains a white rectangle of
 * the given size centered at the given position in a 64x64 viewport.
 *
 * [Parameters]
 *     w, h: Rectangle width and height, in pixels (assumed to be even).
 *     cx, cy: Rectangle center coordinates, in pixels.
 * [Return value]
 *     True if the display data is correct, false if not.
 */
static inline int check_rectangle(int w, int h, int cx, int cy)
    {return check_colored_rectangle(w, h, cx, cy, 1, 1, 1);}
#define CHECK_RECTANGLE(...)  CHECK_TRUE(check_rectangle(__VA_ARGS__))

/**
 * CHECK_SQUARE:  Check that the display contains a 32x32 square of the
 * given color centered in a 64x64 viewport.
 *
 * [Parameters]
 *     r, g, b: Color components.
 * [Return value]
 *     True if the display data is correct, false if not.
 */
static inline int check_square(float r, float g, float b)
    {return check_colored_rectangle(32, 32, 32, 32, r, g, b);}
#define CHECK_SQUARE(...)  CHECK_TRUE(check_square(__VA_ARGS__))

/*-----------------------------------------------------------------------*/

/**
 * auto_mipmaps_supported:  Return whether the system's texture
 * implementation supports automatic generation of mipmaps.
 *
 * [Return value]
 *     True if the system supports automatic mipmap generation, false if not.
 */
extern int auto_mipmaps_supported(void);

/**
 * run_tests_in_window:  Call the given test function (typically a test
 * runner) with the graphics subsystem initialized and a display mode of
 * at least TESTWxTESTH pixels set up.  Windowed mode is used if the
 * system supports it.  Regardless of actual display size, the viewport
 * is set to TESTWxTESTH pixels.
 *
 * This function implicitly calls thread_init() and thread_cleanup(), so
 * test code itself should not do so (this will cause a memory leak).
 *
 * FAIL() is assumed to fail hard (the default behavior).
 *
 * [Parameters]
 *     function: Test function to call.
 * [Return value]
 *     The called function's return value, or false if the display mode
 *     could not be set.
 */
extern int run_tests_in_window(int (*function)(void));

/**
 * run_tests_in_sized_window:  Call the given test function with the
 * graphics subsystem initialized and a display mode of at least the given
 * size set up.  Equivalent to run_tests_in_window() with TESTW and TESTH
 * replaced by width and height.
 *
 * [Parameters]
 *     function: Test function to call.
 *     width, height: Minimum display size to use.
 * [Return value]
 *     The called function's return value, or false if the display mode
 *     could not be set.
 */
extern int run_tests_in_sized_window(int (*function)(void),
                                     int width, int height);

/**
 * open_window:  Set a windowed (if possible) display mode of at least the
 * given size.
 *
 * [Parameters]
 *     width, height: Desired display size.
 * [Return value]
 *     True on success, false on error.
 */
extern int open_window(int width, int height);

/**
 * force_close_window:  Close the currently open window using the
 * appropriate system-specific interface.  Useful for testing behavior
 * across a loss of graphics state.
 */
extern void force_close_window(void);

/**
 * grab_display:  Make a copy of region (0,0)-(TESTW,TESTH) of the display
 * data in RGBA format.
 *
 * [Return value]
 *     mem_alloc()ed buffer containing pixel data, or NULL on error.
 */
extern uint8_t *grab_display(void);

/**
 * draw_square:  Draw a square from (-0.5,-0.5) to (+0.5,+0.5) at the given
 * Z coordinate with the given color.
 *
 * [Parameters]
 *     z: Depth value (-1 through +1).
 *     r, g, b, a: Color components.
 */
extern void draw_square(float z, float r, float g, float b, float a);

/**
 * get_alternate_video_mode:  Find a non-default video mode on the default
 * display device and return its resolution.  By default, the function
 * chooses a mode whose resolution is as close to the default resolution
 * as possible.

 * On Linux, Mac OS X, and Windows, if the environment variable
 * SIL_TEST_ALTERNATE_DISPLAY_MODE is set to a string like "1280x720",
 * that resolution is used unconditionally.  This can be used to work
 * around systems that claim to provide video modes which they don't
 * actually support.
 *
 * [Parameters]
 *     width_ret: Pointer to variable to receive the mode width, in pixels.
 *     height_ret: Pointer to variable to receive the mode height, in pixels.
 * [Return value]
 *     True if a mode was found, false if not.
 */
extern int get_alternate_video_mode(int *width_ret, int *height_ret);

/**
 * get_mouse_position:  Return the current absolute position of the mouse
 * pointer on systems with a mouse pointer.
 *
 * [Parameters]
 *     x_ret: Pointer to variable in which to save the current X position.
 *     y_ret: Pointer to variable in which to save the current Y position.
 */
extern void get_mouse_position(int *x_ret, int *y_ret);

/**
 * set_mouse_position:  Set the absolute position of the mouse pointer on
 * systems with a mouse pointer.
 *
 * [Parameters]
 *     x, y: Absolute position to set.
 */
extern void set_mouse_position(int x, int y);


#ifdef SIL_PLATFORM_WINDOWS
/**
 * wine_new_window_workaround:  For Windows, attempt to detect whether the
 * program is running under the Wine environment on a non-Windows host, and
 * if so, delay the program for a short time.  This is used to work around
 * apparent bugs in Wine which break some of the tests, at least on a Linux
 * host.  (At a guess, the problem is that Wine doesn't wait for X11 to be
 * ready for drawing operations before claiming that the window has been
 * shown.)
 */
extern void wine_new_window_workaround(void);
#endif

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_TEST_GRAPHICS_INTERNAL_H
