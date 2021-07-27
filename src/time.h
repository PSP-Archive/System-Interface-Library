/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/time.h: Internal header for timekeeping routines.
 */

#ifndef SIL_SRC_TIME_H
#define SIL_SRC_TIME_H

#include "SIL/time.h"  // Include the public header.

/*************************************************************************/
/*************************************************************************/

/**
 * time_init:  Initialize the timekeeping functionality.  This function
 * always succeeds.
 *
 * This function may be safely called multiple times; however, it is
 * undefined whether values returned from time_now() before a call to
 * time_init() are related to those returned after the call.
 */
extern void time_init(void);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_TIME_H
