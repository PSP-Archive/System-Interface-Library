/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/font/internal.h: Header for font test helper routines.
 */

#ifndef SIL_SRC_TEST_FONT_INTERNAL_H
#define SIL_SRC_TEST_FONT_INTERNAL_H

/*************************************************************************/
/*************************************************************************/

/**
 * CHECK_TEXTURE_MEMORY_FAILURES:  Wrapper around the CHECK_MEMORY_FAILRES()
 * macro which calls graphics_flush_resources() on failure to ensure all
 * texture resources are actually deleted.
 */
#define CHECK_TEXTURE_MEMORY_FAILURES(expr) \
    CHECK_MEMORY_FAILURES((expr) || (graphics_flush_resources(), 0))

/**
 * render_setup:  Clear the display, set coordinate transformation matrices
 * for a 1:1 mapping between view coordinates and display pixels, and set
 * other rendering parameters appropriately for text rendering.
 *
 * [Parameters]
 *     flip_v: True for (0,0) at the upper-left corner, false for (0,0) at
 *         the lower-left corner.
 */
extern void render_setup(int flip_v);

/**
 * check_render_result:  Check whether the result of rendering matches the
 * given data (optionally with a small margin of error for renderer
 * idiosyncrasies).
 *
 * [Parameters]
 *     x0, y0: Base coordinates (origin at lower-left) of rectangle to check.
 *     w, h: Size of rectangle to check.
 *     exact: True to check for an exact match; false to allow slight
 *         differences in pixel values.
 *     data: Alpha data (w*h chars) to compare against (origin at upper-left;
 *         characters are ' '==0, '.'==64, ':'==128, '#'==255).
 * [Return value]
 *     True if the display data matches the given data and all other
 *     portions of the display are empty, false otherwise.
 */
extern int check_render_result(int x0, int y0, int w, int h, int exact,
                               const char *data);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_TEST_FONT_INTERNAL_H
