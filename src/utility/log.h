/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/utility/log.h: Internal header for logging-related functions.
 */

#ifndef SIL_SRC_UTILITY_LOG_H
#define SIL_SRC_UTILITY_LOG_H

#include "SIL/utility/log.h"  // Include the public header.

/*************************************************************************/
/************************ Test control interface *************************/
/*************************************************************************/

#ifdef SIL_INCLUDE_TESTS

/**
 * test_DLOG_last_message:  Buffer containing the last message written via
 * the DLOG() interface.
 */
extern char test_DLOG_last_message[SIL_DLOG_MAX_SIZE];

#endif

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_UTILITY_LOG_H
