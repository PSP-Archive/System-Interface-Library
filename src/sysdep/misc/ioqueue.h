/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/misc/ioqueue.h: Header for asynchronous I/O functions.
 */

/*
 * This header declares functionality for asynchronous I/O operations
 * (though no "output" functions are currently implemented due to lack of
 * need).  These are similar in concept to the POSIX "aio" library, but
 * also allow asynchronous open operations.
 *
 * To start an operation, call ioq_open() or ioq_read().  These functions
 * take the same parameters as the equivalent POSIX system calls, but
 * return an "I/O request ID" rather than a file descriptor or byte count.
 * This request ID can then be passed to ioq_poll() to check the status of
 * the request, or to ioq_wait() to wait for completion and obtain the
 * result.  Note that a pending request will continue to use system
 * resources even after the operation completes until its result has been
 * retrieved with ioq_wait().
 *
 * A pending request can be cancelled by calling ioq_cancel(), which will
 * stop the operation (if possible) and set the request to an error state.
 * the request must still be waited for with ioq_wait().
 *
 * File descriptors opened with ioq_open() can be used with regular read(),
 * and likewise for open() and ioq_read(); however, behavior is undefined
 * if read() is called while an asynchronous read is pending on the same
 * file descriptor.  There is no asynchronous close operation, so file
 * descriptors from ioq_open() should be closed with regular close().
 *
 * ioq_*() functions set errno on error, like regular library functions.
 * However, the ESRCH error code is used to indicate that an invalid
 * request ID was passed to a function (like EBADFD for file descriptors).
 *
 * All ioq_*() functions except ioq_reset() are thread-safe.
 *
 * Note that ioq_open() is not currently used by SIL.
 */

#ifndef SIL_SRC_SYSDEP_MISC_IOQUEUE_H
#define SIL_SRC_SYSDEP_MISC_IOQUEUE_H

#ifdef SIL_PLATFORM_WINDOWS
# include "src/sysdep/windows/internal.h"  // For HANDLE type definition.
#endif

/*************************************************************************/
/*************************************************************************/

/**
 * IOQHandle:  Type of a file descriptor or handle, equivalent to the type
 * used in the host system's file-related system calls.  Used as the type
 * of the first parameter to ioq_read().
 */
#if defined(SIL_PLATFORM_WINDOWS)
typedef HANDLE IOQHandle;
#else
typedef int IOQHandle;
#endif

/**
 * IOQHANDLE_INVALID:  IOQHandle value indicating an invalid handle (failed
 * open).
  */
#ifdef SIL_PLATFORM_WINDOWS
# define IOQHANDLE_INVALID  INVALID_HANDLE_VALUE
#else
# define IOQHANDLE_INVALID  (-1)
#endif

/**
 * RESULT_TO_IOQHANDLE:  Macro to convert an ioq_open() operation result
 * (ioq_wait() return value) to an IOQHandle.
 */
#define RESULT_TO_IOQHANDLE(result)  ((IOQHandle)(intptr_t)(result))

/*-----------------------------------------------------------------------*/

/**
 * ioq_init:  Initialize the ioqueue library.
 *
 * [Return value]
 *     True on success, false on error.
 */
extern int ioq_init(void);

/**
 * ioq_set_read_limit:  Set the maximum number of bytes to read in a single
 * read operation.  Read requests larger than this value will be read using
 * multiple system calls, each one reading no more than the number of bytes
 * specified here.  Larger values reduce overhead but increase the potential
 * delay in responding to requests with deadlines.
 *
 * The default value is 1048576.
 */
extern void ioq_set_read_limit(int64_t limit);

/**
 * ioq_open:  Start an asynchronous open operation.
 *
 * The deadline parameter indicates the maximum acceptable delay from the
 * time the request is submitted to the time the operation is started.
 * This is a "best effort" deadline; ioqueue will attempt to schedule the
 * operation before the requested deadline, but this is not guaranteed.
 * A value of zero for deadline indicates that the request should be
 * started as soon as possible.  A negative value indicates no deadline;
 * all such requests will be processed in FIFO order and may be delayed in
 * order to service requests with deadlines.
 *
 * On Windows, the operation result is a HANDLE (cast to int64_t) which
 * can be used in subsequent Windows system calls.  The result should be
 * cast back using the RESULT_TO_IOQHANDLE() macro before being tested or
 * otherwise used.
 *
 * [Parameters]
 *     path: Path of file to open.
 *     flags: Open flags (must not include O_WRONLY or O_RDWR).
 *     deadline: Time by which the open operation should be started (in
 *         seconds), or -1 for no deadline.
 * [Return value]
 *     I/O request ID (nonzero), or zero on error.
 * [Operation result]
 *     New file descriptor, or IOQHANDLE_INVALID on error.
 */
extern int ioq_open(const char *path, int flags, double deadline);

/**
 * ioq_read:  Start an asynchronous read operation.  deadline is
 * interpreted as for ioq_open().
 *
 * For Windows, the file must have been opened for synchronous I/O (i.e.,
 * without FILE_FLAG_OVERLAPPED set).
 *
 * [Parameters]
 *     fd: File descriptor to read from.
 *     buf: Buffer into which to read.
 *     count: Number of bytes to read.
 *     pos: File position for read.
 *     deadline: Time by which the read operation should be started (in
 *         seconds), or -1 for no deadline.
 * [Return value]
 *     I/O request ID (nonzero), or zero on error.
 * [Operation result]
 *     Number of bytes read, or -1 on error.  End-of-file is not considered
 *     an error.
 */
extern int ioq_read(IOQHandle fd, void *buf, int64_t count, int64_t pos,
                    double deadline);

/**
 * ioq_poll:  Return the completion status of an asynchronous operation.
 *
 * [Parameters]
 *     request: I/O request ID.
 * [Return value]
 *     True if the request has completed or the request ID is invalid;
 *     false if the request is still in progress.
 */
extern int ioq_poll(int request);

/**
 * ioq_wait:  Wait for an asynchronous operation to complete, and return
 * its result.  After calling this function, the request ID is no longer
 * valid.
 *
 * The error code returned in *error_ret is system-specific:
 *    - For POSIX systems: the value of errno
 *    - For Windows: the error code from GetLastError()
 * However, *error_ret will always be set to 0 for a successful operation.
 *
 * Callers can distinguish between -1 returned due to an invalid request ID
 * and -1 returned as the result of the operation as follows:
 *    - If the request ID was invalid, errno is set to ESRCH and *error_ret
 *      is set to 0.
 *    - If the request ID was valid but the operation result was -1, errno
 *      is left unchanged and *error_ret is set to a nonzero value.
 *
 * [Parameters]
 *     request: I/O request ID.
 *     error_ret: Pointer to variable to receive the error code for a
 *         failed operation, 0 for a successful operation.  May be NULL if
 *         the error code is not needed.
 * [Return value]
 *     Operation result, or -1 if the request ID is invalid.
 */
extern int64_t ioq_wait(int request, int *error_ret);

/**
 * ioq_cancel:  Cancel an asynchronous operation.  The request must still
 * be waited for with ioq_wait(), which will return failure with error
 * ECANCELED (on Windows: ERROR_OPERATION_ABORTED).
 *
 * If the request was a read request which was split into several read
 * operations (see ioq_set_read_limit()), ioq_wait() will return failure
 * even if some data was successfully read in.  There is no way for the
 * caller to obtain the number of bytes read before cancellation.
 *
 * Note that while this function always succeeds for a valid request ID,
 * it will not necessarily abort the I/O itself, depending on the state of
 * the operation.  However, aborting an open operation will close the file
 * descriptor if the open succeeded.
 *
 * [Parameters]
 *     request: I/O request ID.
 */
extern void ioq_cancel(int request);

/**
 * ioq_cancel_fd:  Cancel all pending read operations on the given file.
 * This should be called before closing the file if there might be any
 * unwaited read requests on the file.
 *
 * Cancelled requests are processed as with ioq_cancel().
 *
 * [Parameters]
 *     fd: File descriptor for which to cancel pending operations.
 */
extern void ioq_cancel_fd(IOQHandle fd);

/**
 * ioq_reset:  Reset all internal state and free any statically allocated
 * resources.  Behavior is undefined if this function is called while any
 * operations are pending.
 */
extern void ioq_reset(void);

/*************************************************************************/
/************************ Test control interface *************************/
/*************************************************************************/

#ifdef SIL_INCLUDE_TESTS

/**
 * TEST_misc_ioqueue_move_on_realloc:  Enable or disable moving the I/O
 * request array on expansion.  When enabled, the I/O request array will
 * always be moved to a different address when it is expanded, to expose
 * errors caused by dangling pointers.
 *
 * [Parameters]
 *     enable: True to enable, false to disable.
 */
extern void TEST_misc_ioqueue_move_on_realloc(int enable);

/**
 * TEST_misc_ioqueue_block_io_thread:  Enable or disable blocking of the
 * I/O thread.  While enabled, the I/O thread will stop executing; this can
 * be used to intentionally queue up several requests before allowing any
 * to be processed.
 *
 * This flag is reset to false (disabled) by ioq_reset().
 *
 * [Parameters]
 *     enable: True to enable, false to disable.
 */
extern void TEST_misc_ioqueue_block_io_thread(int enable);

/**
 * TEST_misc_ioqueue_unblock_on_wait:  Enable or disable unblocking of the
 * I/O thread on calls to ioq_wait().  When enabled, calling ioq_wait() on
 * a pending request causes the I/O thread to run until that request is
 * processed.  Has no effect if the I/O thread is not blocked via
 * TEST_misc_ioqueue_block_io_thread().
 *
 * Behavior is undefined if multiple threads make simultaneous calls to
 * ioq_wait() while this flag is enabled.
 *
 * This flag is reset to false (disabled) by ioq_reset().
 *
 * [Parameters]
 *     enable: True to enable, false to disable.
 */
extern void TEST_misc_ioqueue_unblock_on_wait(int enable);

/**
 * TEST_misc_ioqueue_step_io_thread:  Execute one iteration of the I/O
 * thread and return when that iteration has completed.  If there are no
 * pending requests, this function will block until one has been submitted.
 *
 * This function is intended to be called while the I/O thread is blocked
 * via TEST_misc_ioqueue_block_io_thread(1), though it can be called
 * without problems even when the thread is running normally.
 */
extern void TEST_misc_ioqueue_step_io_thread(void);

/**
 * TEST_misc_ioqueue_permfail_next_read:  Enable or disable force-failing
 * of the next call to ioq_read() with ENOMEM.  If enabled, this only takes
 * effect for one call and is subsequently disabled.
 *
 * [Parameters]
 *     enable: True to enable, false to disable.
 */
extern void TEST_misc_ioqueue_permfail_next_read(int enable);

/**
 * TEST_misc_ioqueue_tempfail_next_read:  Enable or disable force-failing
 * of the next call to ioq_read() with EAGAIN.  If enabled, this only takes
 * effect for one call and is subsequently disabled.
 *
 * [Parameters]
 *     enable: True to enable, false to disable.
 */
extern void TEST_misc_ioqueue_tempfail_next_read(int enable);

/**
 * TEST_misc_ioqueue_iofail_next_read:  Enable or disable force-failing of
 * the next read operation enqueued by ioq_read() with EIO.  If enabled,
 * this only takes effect for one operation and is subsequently disabled.
 *
 * [Parameters]
 *     enable: True to enable, false to disable.
 */
extern void TEST_misc_ioqueue_iofail_next_read(int enable);

#endif  // SIL_INCLUDE_TESTS

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_MISC_IOQUEUE_H
