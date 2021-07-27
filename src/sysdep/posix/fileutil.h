/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/posix/fileutil.h: Header for miscellaneous file utility
 * functions.
 */

/*
 * This header declares functions for miscellaneous operations on entire
 * files (read, write, copy) which can be used in any POSIX-compatible
 * environment.  Note in particular that the write and copy functions rely
 * on rename() atomically replacing the target file, as required by POSIX.
 */

#ifndef SIL_SRC_SYSDEP_POSIX_FILEUTIL_H
#define SIL_SRC_SYSDEP_POSIX_FILEUTIL_H

#include <sys/types.h>  // For ssize_t.

/*************************************************************************/
/*************************************************************************/

/**
 * posix_read_file:  Read the file at the given path into memory allocated
 * using mem_alloc().
 *
 * On error, errno is set to indicate the cause of the error, and *size_ret
 * is not modified.
 *
 * This function blocks until the read is complete (or fails).
 *
 * [Parameters]
 *     path: Path of file to read.
 *     size_ret: Pointer to variable to receive the data size, in bytes.
 *     mem_flags: Memory allocation flags (MEM_*).
 * [Return value]
 *     Newly-allocated buffer containing the file data, or NULL on error.
 */
extern void *posix_read_file(const char *path, ssize_t *size_ret,
                             unsigned int mem_flags);

/**
 * posix_write_file:  Write the given data to the given path.  If the
 * containing directory does not exist, it (and any missing parent
 * directories) will be created.  If a file (including a non-directory
 * special file) already exists at the given path, it is atomically
 * replaced with the new file; however, this function will not attempt to
 * replace a file for which the process does not have write permission
 * (this is indicated by a failure return with errno == EACCESS).
 *
 * On error, errno is set to indicate the cause of the error.  For any
 * error other than EIO, any existing file is guaranteed to be unmodified.
 *
 * This function blocks until the write is complete (or fails).
 *
 * [Parameters]
 *     path: Path of file to write.
 *     data: Data to write to file.
 *     size: Size of data, in bytes.
 *     sync: True to flush data to permanent storage before closing the
 *         file; false to leave flush timing to the operating system.
 * [Return value]
 *     True if the file was successfully written, false if not.
 */
extern int posix_write_file(const char *path, const void *data, ssize_t size,
                            int sync);

/**
 * posix_copy_file:  Copy a file to a new pathname, optionally preserving
 * the source file's access and modification times.  If the containing
 * directory does not exist, it (and any missing parent directories) will
 * be created.  This function works correctly even if "from" and "to" point
 * to the same file (though naturally the end result is a no-op aside from
 * any metadata changes).
 *
 * If a file already exists at the path "to", it is atomically replaced
 * with the new file; however, this function will not attempt to replace a
 * file for which the process does not have write permission (this is
 * indicated by a failure return with errno == EACCESS).
 *
 * On error, errno is set to indicate the cause of the error.  For any
 * error other than EIO, any existing file at "to" is guaranteed to be
 * unmodified.
 *
 * This function blocks until the copy is complete (or fails).
 *
 * [Parameters]
 *     from: Path of file to copy.
 *     to: Path to copy file to.
 *     preserve_times: True to copy the source file's access and
 *         modification times to the destination file.
 *     buffer_size: Size of memory buffer for copying data, in bytes, or
 *         zero for the default size.  This is an optimization hint only
 *         and does not affect the final result.
 * [Return value]
 *     True if the file was successfully copied, false if not.
 */
extern int posix_copy_file(const char *from, const char *to,
                           int preserve_times, ssize_t buffer_size);

/**
 * posix_mkdir_p:  Create a directory and any missing parent directories,
 * like "mkdir -p".  On error, errno is set to indicate the cause of the
 * error.
 *
 * [Parameters]
 *     path: Path of directory to create.
 * [Return value]
 *     True on success, false if the directory or a parent directory could
 *     not be created.
 */
extern int posix_mkdir_p(const char *path);

/**
 * posix_rmdir_r:  Remove a directory tree, like "rmdir -r".
 *
 * On error, errno is set to indicate the cause of the error.  If more than
 * one error occurred (for example, two directory entries could not be
 * removed), it is undefined which error is described by errno.
 *
 * [Parameters]
 *     path: Path of directory tree to remove.
 * [Return value]
 *     True on success, false if one or more files or directories could not
 *     be removed.
 */
extern int posix_rmdir_r(const char *path);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_POSIX_FILEUTIL_H
