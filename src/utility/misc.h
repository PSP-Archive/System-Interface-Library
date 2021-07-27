/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/utility/misc.h: Internal header for miscellaneous utility functions.
 */

#ifndef SIL_SRC_UTILITY_MISC_H
#define SIL_SRC_UTILITY_MISC_H

#include "SIL/utility/misc.h"  // Include the public header.

/*************************************************************************/
/*************************************************************************/

/**
 * split_args:  Split the string s into individual arguments at whitespace
 * characters, and return a newly-allocated (using mem_alloc()) array
 * containing pointers to the arguments.  The pointers point into s, which
 * is destroyed by this function; the array is terminated with a pointer
 * value of NULL (thus the array has (*argc_ret)+1 elements allocated).
 *
 * If insert_dummy is true, an element pointing to a constant empty string
 * will be inserted as the first element in the argument array.
 *
 * On error, *argc_ret and *argv_ret are left unmodified, but the input
 * string is still destroyed.
 *
 * [Parameters]
 *     s: String containing arguments to split (destroyed).
 *     insert_dummy: True to insert a NULL element as (*argv_ret)[0].
 *     argc_ret: Pointer to variable to receive the length of *argv_ret.
 *     argv_ret: Pointer to variable to receive the argument array.
 * [Return value]
 *     True on success, false on invalid parameter or out of memory.
 */
extern int split_args(char *s, int insert_dummy,
                      int *argc_ret, char ***argv_ret);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_UTILITY_MISC_H
