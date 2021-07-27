/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/utility/log.h: Header for logging-related functions.
 */

#ifndef SIL_UTILITY_LOG_H
#define SIL_UTILITY_LOG_H

EXTERN_C_BEGIN

/*************************************************************************/
/*************************************************************************/

/**
 * log_to_file:  Write all log messages to the given file in addition to
 * the default log target (the console on PC-type platforms, for example).
 * The file will be stored in a system-dependent location, typically the
 * same location as files created with userdata_save_data().  The filename
 * must not contain any path separators and must be less than 63 bytes long.
 * Pass NULL to close an open log file and log only to the default target.
 *
 * This function is not thread-safe; attempting to open or close a log file
 * while another thread is writing a log message may crash the program.
 *
 * If DEBUG is not defined, this function has no effect.
 *
 * [Parameters]
 *     name: Name of log file, or NULL to close an open log file.
 */
extern void log_to_file(const char *name);

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_UTILITY_LOG_H
