/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/debug.h: Header for debugging routines.
 */

/*
 * SIL provides a simple debugging interface which can display information
 * about the program's CPU and memory usage.  The debug interface can be
 * activated in any of the following ways:
 *
 * - Programmatically, by calling debug_interface_activate().
 *
 * - If debug_interface_enable_auto(1) has been called:
 *
 *    + Using a mouse, by clicking the left, middle, and right mouse
 *      buttons at the same time.
 *
 *    + Using a touchscreen interface, by touching the upper-left and
 *      lower-left corners of the screen at the same time.  In this case,
 *      the debug interface stays active only as long as at least one
 *      touch remains on the screen.
 *
 * On the PSP, it is also possible to display the most recent debug log
 * messages by holding the L and R buttons and pressing the Select button.
 *
 * The debugging interface allows CPU and memory usage meters to be
 * toggled on or off; these can also be toggled programmatically.
 *
 * The CPU meter shows a rolling average of CPU usage as a percentage of
 * frame time.  The meter breaks down CPU time into rendering (red), debug
 * interface overhead (yellow), non-render processing (green), and GPU wait
 * (blue); more specifically, red indicates the time between calls to
 * graphics_start_frame() and graphics_finish_frame(), and green indicates
 * the time between a call to graphics_finish_frame() and the next call to
 * graphics_start_frame().  (Naturally, this division is only meaningful if
 * the client program similarly separates its rendering and non-render
 * processing logic.)  Note that on OpenGL platforms, glFinish() is called
 * to determine GPU wait time, which may reduce performance for some
 * platforms and rendering patterns.
 *
 * The memory meter shows the amount of memory used by the program, the
 * amount reserved by the system, and the amount available for use by the
 * program, including memory which is currently in use but which the system
 * will reclaim if needed without disrupting the behavior of the program.
 * Note that the latter value may not be accurate; for example, on iOS, the
 * operating system will kill the program if available memory falls below a
 * predetermined threshold, even if a requested allocation could succeed.
 * On platforms which reserve a fixed amount of memory for the program
 * (currently this only applies to the PSP), the memory meter instead
 * displays a map of all memory available to the program, colored by usage:
 * red for textures, green for audio data, blue for fonts, cyan for
 * resource management overhead, and white for all other allocations.
 *
 * This header also provides utility functions for simple drawing (filled
 * boxes and text) for use by external code.  These functions are only
 * available when building in debug mode, so they should not be used for
 * normal rendering.
 *
 * Note that the debug interface modifies the current rendering state, so
 * client code should treat the rendering state as undefined at the
 * beginning of each frame and set all required parameters (including the
 * projection matrix, for example).
 */

#ifndef SIL_DEBUG_H
#define SIL_DEBUG_H

#ifdef DEBUG  // To the end of the file.

EXTERN_C_BEGIN

struct Vector4f;

/*************************************************************************/
/*************************************************************************/

/**
 * debug_interface_activate:  Activate or deactivate the debug interface.
 *
 * [Parameters]
 *     on: True to activate the debug interface, false to deactivate it.
 */
extern void debug_interface_activate(int on);

/**
 * debug_interface_enable_auto:  Enable or disable the mouse and touch
 * methods to activate the debug interface.  By default, these are disabled.
 *
 * [Parameters]
 *     enable: True to enable mouse and touch activation, false to disable.
 */
extern void debug_interface_enable_auto(int enable);

/**
 * debug_interface_is_active:  Return whether the debug interface is
 * currently active.
 *
 * [Return value]
 *     True if the debug interface is active, false if not.
 */
extern int debug_interface_is_active(void);

/**
 * debug_show_cpu_usage:  Show or hide the CPU usage display.
 *
 * [Parameters]
 *     on: True to show CPU usage, false to hide it.
 */
extern void debug_show_cpu_usage(int on);

/**
 * debug_cpu_usage_is_on:  Return whether the CPU usage display is
 * currently shown.
 *
 * [Return value]
 *     True if the CPU usage display is shown, false if not.
 */
extern int debug_cpu_usage_is_on(void);

/**
 * debug_show_memory_usage:  Show or hide the memory usage display.
 *
 * [Parameters]
 *     on: True to show memory usage, false to hide it.
 */
extern void debug_show_memory_usage(int on);

/**
 * debug_memory_usage_is_on:  Return whether the memory usage display is
 * currently shown.
 *
 * [Return value]
 *     True if the CPU usage display is shown, false if not.
 */
extern int debug_memory_usage_is_on(void);

/*-----------------------------------------------------------------------*/

/**
 * debug_draw_text:  Draw a string of text on the screen using a simple
 * debug font (12 pixels high for an Nx720 display, scaled based on display
 * height).  Non-ASCII characters are not supported.
 *
 * [Parameters]
 *     x, y: Upper-left coordinates for text, in display coordinates.
 *     alignment: >0 for left alignment, 0 for center alignment, <0 for
 *         right alignment.
 *     color: Color for text (red/green/blue/alpha, each 0.0-1.0).
 *     format: Format string for text (may contain printf()-style tokens).
 *     ...: Format arguments.
 * [Return value]
 *     Width of text drawn, in display coordinate units.
 */
extern int debug_draw_text(int x, int y, int alignment,
                           const struct Vector4f *color,
                           const char *format, ...);

/**
 * debug_text_width:  Return the width of the given text string as it would
 * be rendered in the font used by debug_draw_text().
 *
 * This routine is guaranteed not to call DLOG().
 *
 * [Parameters]
 *     text: Text string for which to calculate width.
 *     len: Length of string in bytes, or 0 if the string is null-terminated.
 * [Return value]
 *     Width of text, in display coordinate units.
 */
extern PURE_FUNCTION int debug_text_width(const char *text, int len);

/**
 * debug_text_height:  Return the height of text rendered by debug_draw_text().
 *
 * This routine is guaranteed not to call DLOG().
 *
 * [Return value]
 *     Height of text, in display coordinate units.
 */
extern PURE_FUNCTION int debug_text_height(void);

/**
 * debug_fill_box:  Fill a rectangle on the display.
 *
 * This routine may change the current texture and blend states.
 *
 * [Parameters]
 *     x, y: Base coordinates of rectangle.
 *     w, h: Width and height of rectangle.
 *     color: Fill color.
 */
extern void debug_fill_box(int x, int y, int w, int h,
                           const struct Vector4f *color);

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // DEBUG
#endif  // SIL_DEBUG_H
