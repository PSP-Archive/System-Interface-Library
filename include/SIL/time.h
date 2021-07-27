/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/time.h: Header for timekeeping routines.
 */

/*
 * This header declares functions used for timekeeping.  All functions
 * operate in units of seconds; timestamps and time intervals are
 * floating-point values which may have sub-second (fractional) parts.
 * The actual precision of the functions is system-dependent.
 *
 * In addition to basic timekeeping functions (obtain the current time,
 * wait for a period of time), this header also provides a simple "marker"
 * interface that can be used for low-granularity profiling, by recording
 * the times at which particular events occurred and later reading out
 * those times as offsets from a fixed initial time.  For example, the
 * caller might call time_mark_reset() at the beginning of a frame, then
 * call time_mark() at certain points during the frame -- after processing
 * events, when rendering completes, and so on.  The caller could then use
 * the values returned by time_get_mark() in drawing a profiling display.
 *
 * Marker handling is automatically disabled if DEBUG is not defined;
 * time_mark_reset() and time_mark() become no-ops, and time_get_mark()
 * always returns a negative value (indicating a nonexistent mark).
 */

#ifndef SIL_TIME_H
#define SIL_TIME_H

EXTERN_C_BEGIN

/*************************************************************************/
/************************* Configuration options *************************/
/*************************************************************************/

/**
 * TIME_MAX_MARKS:  Maximum number of marks that can be registered with
 * time_mark() for a single call to time_mark_reset().
 */
#define TIME_MAX_MARKS  64

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

/*---------------------------- Core routines ----------------------------*/

/**
 * time_now:  Return the current time as a scalar value.  The return value
 * is in units of seconds and increases monotonically over the course of
 * the program's execution.  The epoch is no later than the starting time
 * of the program, so timestamp values returned by this function are always
 * nonnegative; however, the epoch is otherwise undefined, so the actual
 * value of a timestamp is meaningless except in comparison to other
 * timestamps.
 *
 * [Return value]
 *     Current timestamp, in seconds.
 */
extern double time_now(void);

/**
 * time_delay:  Wait for the specified amount of time.  Returns immediately
 * if the specified time is zero or negative.
 *
 * This function will attempt to wait for as close to the specified time
 * as possible, but the actual delay may be slightly greater or less than
 * the specified time due to system constraints.  For accurate timing over
 * repeated calls, use time_delay_until().
 *
 * [Parameters]
 *     time: Amount of time to delay, in seconds.
 */
extern void time_delay(double time);

/**
 * time_delay_until:  Wait until the specified time.  Returns immediately
 * if the specified time is less than or equal to the current time (as
 * returned by time_now()).
 *
 * This function will attempt to wait until as close to the specified time
 * as possible, but the actual delay may be slightly greater or less than
 * the specified time due to system constraints.  For accurate timing over
 * repeated calls, obtain a base timestamp from time_now() and call this
 * function with a target timestamp computed from that base timestamp and
 * an iteration period.  For example:
 *
 *     const double start = time_now();
 *     const double period = 1/60.0;
 *     double iteration = 0;
 *     for (;;) {
 *         iteration += 1;
 *         time_delay_until(start + iteration * period);
 *         // The remainder of the loop body will be executed 60 times
 *         // per second, on average.
 *     }
 *
 * [Parameters]
 *     target: Timestamp until which to delay, in seconds.
 */
extern void time_delay_until(double target);

/*------------------- Real (wall-clock) time routines -------------------*/

/**
 * DateTime:  Structure representing a particular instant in real-world
 * (wall-clock) time.  Analogous to "struct tm" as used by the standard C
 * library gmtime() and localtime() functions, but in particular, the year
 * and month fields directly match real-world values rather than requiring
 * +1900 and +1 adjustments.
 *
 * Note that there is no function to convert between timestamps returned
 * by time_now() and real-world time, because there is not necessarily a
 * one-to-one mapping between the two measurement systems.  Generally
 * speaking, timestamps returned by time_now() are based on a monotonic
 * (continuously increasing) system clock, while real-world time may
 * occasionally receive discontinuous changes, such as from NTP or manual
 * time adjustments or when entering or leaving daylight saving time
 * ("summer time").
 *
 * Also note that in 32-bit Android, iOS, and Linux builds, time_get_utc()
 * and time_get_local() will return incorrect results after 03:14:07 UTC
 * on January 19, 2038.  This is an unavoidable limitation of the 32-bit
 * environments on these platforms; if you intend to make use of real-world
 * time, you should consider explicitly not supporting these 32-bit
 * configurations.
 */
typedef struct DateTime DateTime;
struct DateTime {
    int16_t year;   // E.g., 2018
    int8_t month;   // 1=January, 2=Febraury, ..., 12=December
    int8_t day;     // 1-31
    int8_t weekday; // 0=Sunday, 1=Monday, ..., 6=Saturday
    int8_t hour;    // 0-23
    int8_t minute;  // 0-59
    int8_t second;  // 0-59, or 60 during a leap second
    int32_t nsec;   // 0-999_999_999
};

/**
 * time_get_utc:  Return the current real time in Coordinated Universal
 * Time (UTC).
 *
 * [Parameters]
 *     time_ret: Pointer to variable to receive the current time.
 */
extern void time_get_utc(DateTime *time_ret);

/**
 * time_get_local:  Return the current real time in the local time zone
 * and the offset from Coordinated Universal Time (UTC) to local time.
 *
 * The UTC offset returned by this function will always be less than
 * one day (1440 minutes) in magnitude.
 *
 * [Parameters]
 *     time_ret: Pointer to variable to receive the current time.
 * [Return value]
 *     Offset from UTC in minutes, computed as (local time - UTC).
 */
extern int time_get_local(DateTime *time_ret);

/**
 * time_is_utc_before:  Return whether the current real time in Coordinated
 * Universal Time (UTC) is strictly earlier than the given time.
 *
 * Note that there is no local-time equivalent of this function because
 * such a function would not always be well-defined.  In areas that
 * observe DST (daylight saving time or "summer time"), when reverting
 * from DST to standard time, a period of time (typically one hour) will
 * be repeated, so it is possible for a point in time during that hour to
 * be both before and after another time during that hour.  For example,
 * when the hour from 1:00 to 2:00 is repeated, the first 1:45 is both
 * before and after 1:30 -- the first 1:30 has already passed, but the
 * second 1:30 has not yet been occurred.  For this reason, time
 * comparisons should always be done using UTC, and SIL encourages this by
 * not providing local-time comparison functions.
 *
 * [Parameters]
 *     time: Time for comparison.
 * [Return value]
 *     True if the current time is strictly before the given time; false if
 *     the current time is equal to or after the given time.
 */
extern int time_is_utc_before(const DateTime *time);

/**
 * time_is_utc_after:  Return whether the current real time in Coordinated
 * Universal Time (UTC) is equal to or later than the given time.
 *
 * While the resolution of most systems' timestamps makes it extremely
 * unlikely that the current time would exactly match any particular time
 * instant, this function is defined as "equal to or later than" so that
 * for any particular instant of real time and comparison value,
 * time_is_utc_before() and time_is_utc_after() always give the opposite
 * result.  (This function is provided primarily as a convenience for
 * readability's sake.)
 *
 * [Parameters]
 *     time: Time for comparison.
 * [Return value]
 *     True if the current time is equal to or after the given time; false
 *     if the current time is before the given time.
 */
static inline int time_is_utc_after(const DateTime *time) {
    return !time_is_utc_before(time);
}

/*-------------------- Time markers (for debugging) ---------------------*/

/**
 * time_mark_reset:  Set the current time as the reference time for calls
 * to time_get_mark(), and clear all previously set marks.
 *
 * This function is a no-op if DEBUG is not defined.
 */
#ifdef DEBUG
extern void time_mark_reset(void);
#else
#define time_mark_reset()  /*nothing*/
#endif

/**
 * time_mark:  Associate the current time with the given mark value, for
 * later retrieval with time_get_mark().  Behavior is undefined if the
 * same mark value is used multiple times with no intervening call to
 * time_mark_reset().
 *
 * The number of marks that can be recorded is limited by the
 * TIME_MAX_MARKS constant defined in src/time.c.
 *
 * This function is a no-op if DEBUG is not defined.
 *
 * [Parameters]
 *     mark: Mark value to associate current time with.
 */
#ifdef DEBUG
extern void time_mark(int mark);
#else
#define time_mark(mark)  /*nothing*/
#endif

/**
 * time_get_mark:  Retrieve the time associated with the given mark value
 * as the number of seconds elapsed between the time_mark_reset() call and
 * the time_mark() call for that mark value.  If the given mark value has
 * not been registered, an unspecified negative value is returned.
 *
 * This function always returns a negative value if DEBUG is not defined.
 *
 * [Parameters]
 *     mark: Mark value for which to retrieve elapsed time.
 * [Return value]
 *     Elapsed time in seconds, or a negative value if the mark does not exist.
 */
#ifdef DEBUG
extern double time_get_mark(int mark);
#else
#define time_get_mark(mark)  (-1)
#endif

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_TIME_H
