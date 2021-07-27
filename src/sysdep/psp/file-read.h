/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/file-read.h: Header for PSP low-level file reading logic.
 */

#ifndef SIL_SRC_SYSDEP_PSP_FILE_READ_H
#define SIL_SRC_SYSDEP_PSP_FILE_READ_H

/*************************************************************************/
/*************************************************************************/

/**
 * psp_file_read_init:  Initialize the low-level file reading functionality.
 *
 * [Return value]
 *     True on success, false on error.
 */
extern int psp_file_read_init(void);

/**
 * psp_file_read_cleanup:  Shut down the low-level file reading functionality.
 */
extern void psp_file_read_cleanup(void);

/**
 * psp_file_read_submit:  Submit a file read request.
 *
 * [Parameters]
 *     fd: File descriptor.
 *     start: Read offset, in bytes from the beginning of the file.
 *     len: Number of bytes to read.
 *     buf: Buffer into which to read data.
 *     timed: True to use a deadline, false for an immediate read.
 *     time_limit: Time limit for starting the read operation, in μsec
 *         from this call.  Ignored if "timed" is false.
 * [Return value]
 *     Request ID (nonzero), or zero on error.
 */
extern int psp_file_read_submit(int fd, int64_t start, int32_t len, void *buf,
                                int timed, int32_t time_limit);

/**
 * psp_file_read_check:  Check whether the given request has completed.
 *
 * [Parameters]
 *     id: Request ID.
 * [Return value]
 *     >0 if the request has completed, 0 if the request is in progress,
 *     <0 if the request ID is invalid.
 */
extern int psp_file_read_check(int id);

/**
 * psp_file_read_wait:  Wait for the given request to complete, and return
 * its result.
 *
 * [Parameters]
 *     id: Request ID.
 * [Return value]
 *     Number of bytes read (>=0) on success, negative on error.
 */
extern int psp_file_read_wait(int id);

/**
 * psp_file_read_abort:  Abort the given request.  Does nothing if the
 * request has already completed.
 *
 * Even after aborting a request, the caller must call psp_file_read_wait()
 * to release resources used by the request.
 *
 * [Parameters]
 *     id: Request ID.
 * [Return value]
 *     True on success, false on error (invalid ID).
 */
extern int psp_file_read_abort(int id);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_PSP_FILE_READ_H
