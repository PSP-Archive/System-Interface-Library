/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/utility/png.h: Internal header for PNG file manipulation functions.
 */

#ifndef SIL_SRC_UTILITY_PNG_H
#define SIL_SRC_UTILITY_PNG_H

#include "SIL/utility/png.h"  // Include the public header.

/*************************************************************************/
/************************ Test control interface *************************/
/*************************************************************************/

#ifdef SIL_INCLUDE_TESTS

/**
 * TEST_png_create_num_allocs:  Number of memory allocation operations
 * performed by the most recent call to png_create().
 */
extern int TEST_png_create_num_allocs;

#endif

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_UTILITY_PNG_H
