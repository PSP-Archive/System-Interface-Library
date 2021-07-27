/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/posix/internal.h: Declarations of helper functions for
 * POSIX file-related tests.
 */

#ifndef SIL_SRC_TEST_SYSDEP_POSIX_INTERNAL_H
#define SIL_SRC_TEST_SYSDEP_POSIX_INTERNAL_H

/*************************************************************************/
/*************************************************************************/

/**
 * posix_get_tmpdir:  Return the absolute pathname for the system's
 * temporary directory.
 *
 * [Return value]
 *     System temporary directory pathanme.
 */
extern const char *posix_get_tmpdir(void);

/**
 * posix_create_temporary_dir:  Create a temporary directory and return
 * its pathname.
 *
 * [Parameters]
 *     basename: Base name to use for the temporary directory (the process
 *         ID and a random value will be appended).
 *     pathbuf: Buffer in which to store the generated pathname.
 *     pathbuf_size: Size of pathbuf, in bytes.
 * [Return value]
 *     True on success, false on error.
 */
extern int posix_create_temporary_dir(const char *basename, char *pathbuf,
                                      int pathbuf_size);

/**
 * posix_remove_temporary_dir:  Recursively remove the given directory, and
 * return whether any temporary files (filenames ending in "~") were found.
 *
 * [Parameters]
 *     path: Pathname of directory to remove.
 *     had_temp_files_ret: Pointer to variable to receive true if the
 *         directory contained any temporary files, false if not.
 * [Return value]
 *     True on success (the directory was removed), false on error.
 */
extern int posix_remove_temporary_dir(const char *path,
                                      int *had_temp_files_ret);

/**
 * posix_pipe_writer:  Wait 10 milliseconds, then write the string "foo"
 * to the filesystem object at the given path.
 *
 * As the name suggests, this function is intended to be run as a thread
 * to supply input to a named pipe for a function which attempts to open
 * the pipe for reading.
 *
 * [Parameters]
 *     path: Path of filesystem object to write to.
 * [Return value]
 *     Number of bytes written (should be 3 for a successful write), or
 *     zero on error.
 */
extern int posix_pipe_writer(void *path);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_TEST_SYSDEP_POSIX_INTERNAL_H
