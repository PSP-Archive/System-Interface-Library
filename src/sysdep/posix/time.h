/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/posix/time.h: Header for POSIX-specific timekeeping routines.
 */

#ifndef SIL_SRC_SYSDEP_POSIX_TIME_H
#define SIL_SRC_SYSDEP_POSIX_TIME_H

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

/**
 * sys_posix_time_epoch:  Return the time_now() epoch in system time units.
 * If system time is presumed to be constant for the duration of the
 * computation, the following equality will always hold:
 *
 * time_now() == ((double)(sys_time_now() - sys_posix_time_epoch())
 *                / (double)sys_time_unit())
 *
 * If sys_time_init() has not yet been called, this function will return
 * zero.
 *
 * [Return value]
 *     time_now() epoch in system time units.
 */
extern PURE_FUNCTION uint64_t sys_posix_time_epoch(void);

/**
 * sys_posix_time_clock:  Return the CLOCK_* constant identifying the clock
 * (in the clock_gettime() sense) used to measure time for sys_time_now().
 *
 * If gettimeofday() is being used instead of clock_gettime(), this
 * function returns CLOCK_REALTIME.  (On Linux, at least, gettimeofday()
 * and clock_gettime(CLOCK_REALTIME) are equivalent aside from precision.)
 *
 * [Return value]
 *     System clock used for timekeeping (CLOCK_*).
 */
extern PURE_FUNCTION int sys_posix_time_clock(void);

/*************************************************************************/
/************************** Test control flags ***************************/
/*************************************************************************/

#ifdef SIL_INCLUDE_TESTS

/**
 * TEST_sys_posix_disable_clock_gettime:  If true, clock_gettime() will not
 * be used even if available.  Changes to this flag take effect only when
 * sys_time_init() is called.
 */
extern uint8_t TEST_sys_posix_disable_clock_gettime;

#endif  // SIL_INCLUDE_TESTS

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_POSIX_TIME_H
