/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/posix/files.h: Header for POSIX-specific file access routines.
 */

#ifndef SIL_SRC_SYSDEP_POSIX_FILES_H
#define SIL_SRC_SYSDEP_POSIX_FILES_H

struct SysFile;

/*************************************************************************/
/*************************************************************************/

/**
 * posix_fileno:  Return the POSIX file descriptor for the given file
 * handle.
 *
 * [Parameters]
 *     fh: File handle.
 * [Return value]
 *     File descriptor.
 */
extern int posix_fileno(const struct SysFile *fh);

/**
 * posix_file_path:  Return the pathname with which the given file handle
 * was opened, possibly modified to account for case-insensitive path
 * matching.  For a file handle created with sys_file_dup(), this function
 * returns the same value as would be returned by posix_file_path() on the
 * original file handle.  This function never fails for a valid file handle.
 *
 * [Parameters]
 *     fh: File handle.
 * [Return value]
 *     Pathname used in sys_file_open() call.
 */
extern const char *posix_file_path(const struct SysFile *fh);

#ifdef SIL_INCLUDE_TESTS
/**
 * TEST_posix_file_fail_init:  If set to true, the next (and only the next)
 * call to sys_file_init() will fail.
 */
extern uint8_t TEST_posix_file_fail_init;
#endif

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_POSIX_FILES_H
