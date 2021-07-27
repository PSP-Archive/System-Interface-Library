/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/windows/userdata.h: Test control header for Windows user data
 * access routines.
 */

#ifndef SIL_SRC_SYSDEP_WINDOWS_USERDATA_H
#define SIL_SRC_SYSDEP_WINDOWS_USERDATA_H

/*************************************************************************/
/************************ Test control interface *************************/
/*************************************************************************/

#ifdef SIL_INCLUDE_TESTS

/**
 * TEST_windows_userdata_path:  If not NULL, this path will be used in
 * place of the path returned by sys_userdata_get_data_path() when
 * generating pathnames for user data files.
 */
extern const char *TEST_windows_userdata_path;

#endif  // SIL_INCLUDE_TESTS

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_WINDOWS_USERDATA_H
