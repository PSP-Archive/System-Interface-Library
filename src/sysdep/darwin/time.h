/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/darwin/time.h: Header for Darwin-specific timekeeping routines.
 */

#ifndef SIL_SRC_SYSDEP_DARWIN_TIME_H
#define SIL_SRC_SYSDEP_DARWIN_TIME_H

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

/**
 * darwin_time_epoch():  Return the time_now() epoch in the time base used
 * by the mach_absolute_time() system call.  This can be subtracted from
 * timestamps in UI events (the NSEvent.timestamp property) to give a
 * timestamp compatible with time_now().
 *
 * If sys_time_init() has not yet been called, this function will return
 * zero.
 *
 * [Return value]
 *     time_now() epoch in the mach_absolute_time() time base, expressed
 *     in seconds.
 */
extern PURE_FUNCTION double darwin_time_epoch(void);

/**
 * darwin_time_from_timestamp:  Convert a Mach absolute-time timestamp to
 * a timestamp compatible with time_now().
 *
 * [Parameters]
 *     timestamp: Mach absolute-time timestamp.
 * [Return value]
 *     Corresponding timestamp compatible with time_now().
 */
extern PURE_FUNCTION double darwin_time_from_timestamp(uint64_t timestamp);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_DARWIN_TIME_H
