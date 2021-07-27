/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/posix/util.h: Internal utility functions for POSIX systems.
 */

#ifndef SIL_SRC_SYSDEP_POSIX_UTIL_H
#define SIL_SRC_SYSDEP_POSIX_UTIL_H

#include <time.h>

/*************************************************************************/
/*************************************************************************/

/**
 * timeout_to_ts:  Return a timespec structure corresponding to the given
 * timeout.  Helper function for sys_condvar_wait(), sys_mutex_lock(), and
 * sys_semaphore_wait().
 *
 * [Parameters]
 *     timeout: Timeout, in seconds (must be nonnegative).
 * [Return value]
 *     Corresponding timespec structure.
 */
extern struct timespec timeout_to_ts(float timeout);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_POSIX_UTIL_H
