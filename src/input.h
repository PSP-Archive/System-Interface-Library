/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/input.h: Internal header for input device management.
 */

#ifndef SIL_SRC_INPUT_H
#define SIL_SRC_INPUT_H

#include "SIL/input.h"     // Include the public header.
#include "SIL/keycodes.h"  // Also include the key code definitions.

/*************************************************************************/
/*************************************************************************/

/**
 * input_init:  Initialize the input device management subsystem.  On
 * success, input_update() will automatically be called to retrieve
 * initial input state.
 *
 * [Return value]
 *     True on success, false on error.
 */
extern int input_init(void);

/**
 * input_cleanup:  Shut down the input device management subsystem.
 */
extern void input_cleanup(void);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_INPUT_H
