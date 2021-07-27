/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/ios/main.h: Header for internal utility functions defined in
 * main.m.
 */

#ifndef SIL_SRC_SYSDEP_IOS_MAIN_H
#define SIL_SRC_SYSDEP_IOS_MAIN_H

/*************************************************************************/
/*************************************************************************/

/**
 * ios_resource_dir:  Return the pathname of the directory containing the
 * program's resource files, or "." if the directory is unknown.
 */
extern const char *ios_resource_dir(void);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_IOS_MAIN_H
