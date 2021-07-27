/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/userdata.h: User data management header.
 */

#ifndef SIL_SRC_USERDATA_H
#define SIL_SRC_USERDATA_H

#include "SIL/userdata.h"  // Include the public header.

/*************************************************************************/
/************************** Internal interface ***************************/
/*************************************************************************/

/**
 * userdata_init:  Initialize the user data management functionality.
 *
 * [Return value]
 *     True on success, false on error.
 */
extern int userdata_init(void);

/**
 * userdata_cleanup:  Shut down the user data management functionality.
 * There must be no user data operations in progress when this function is
 * called.
 */
extern void userdata_cleanup(void);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_USERDATA_H
