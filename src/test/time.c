/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/time.c: Tests for the high-level timekeeping functions.
 */

#include "src/base.h"
#include "src/sysdep.h"
#include "src/sysdep/test.h"
#include "src/test/base.h"
#include "src/time.h"

/*-----------------------------------------------------------------------*/

/* Utility macro to check time values and generate useful failure messages. */
#define CHECK_THAT_TIME_IS(expected_time)  do {                 \
    const double _expected_time = (expected_time);              \
    const double _time_now = time_now();                        \
    if (_time_now != _expected_time) {                          \
        FAIL("Expected time %g (0x%016llX), got %g"             \
             " (0x%016llX) for time_now()",                     \
             _expected_time, DOUBLE_BITS(_expected_time),       \
             _time_now, DOUBLE_BITS(_time_now));                \
    }                                                           \
} while (0)

/*************************************************************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_time)

TEST_INIT(init)
{
    time_init();
    return 1;
}

TEST_CLEANUP(cleanup)
{
    /* Re-init (since there's no time_cleanup()) to make sure we don't
     * leave any junk behind. */
    time_init();
    time_mark_reset();
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_time_now)
{
    sys_test_time_set_seconds(1.0);
    CHECK_THAT_TIME_IS(1.0);
    sys_test_time_set_seconds(1.5);
    CHECK_THAT_TIME_IS(1.5);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_time_delay)
{
    sys_test_time_set_seconds(0.0);
    time_delay(1.0);
    CHECK_THAT_TIME_IS(1.0);
    sys_test_time_set_seconds(0.0);
    time_delay(1.0 + 1.0/(1ULL<<52));
    CHECK_THAT_TIME_IS(1.0 + 1.0/sys_time_unit());
    sys_test_time_set_seconds(0.0);
    time_delay(120.0f);  // Longer than the 60sec limit on sys_time_delay().
    CHECK_THAT_TIME_IS(120.0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_time_delay_until)
{
    sys_test_time_set_seconds(1.0);
    time_delay_until(1.5);
    CHECK_THAT_TIME_IS(1.5);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_time_get_utc)
{
    sys_test_time_set_utc(&(DateTime){2001, 3, 2, 5, 7, 8, 9, 123456789}, 30);

    DateTime time;
    time_get_utc(&time);
    CHECK_INTEQUAL(time.year,    2001);
    CHECK_INTEQUAL(time.month,   3);
    CHECK_INTEQUAL(time.day,     2);
    CHECK_INTEQUAL(time.weekday, 5);
    CHECK_INTEQUAL(time.hour,    7);
    CHECK_INTEQUAL(time.minute,  8);
    CHECK_INTEQUAL(time.second,  9);
    CHECK_INTEQUAL(time.nsec,    123456789);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_time_get_utc_invalid)
{
    time_get_utc(NULL);  // Just make sure it doesn't crash.
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_time_get_local)
{
    sys_test_time_set_utc(&(DateTime){2001, 3, 2, 5, 7, 8, 9, 123456789}, 30);

    DateTime time;
    time_get_local(&time);
    CHECK_INTEQUAL(time.year,    2001);
    CHECK_INTEQUAL(time.month,   3);
    CHECK_INTEQUAL(time.day,     2);
    CHECK_INTEQUAL(time.weekday, 5);
    CHECK_INTEQUAL(time.hour,    7);
    CHECK_INTEQUAL(time.minute,  38);
    CHECK_INTEQUAL(time.second,  9);
    CHECK_INTEQUAL(time.nsec,    123456789);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_time_get_local_minute_overflow)
{
    DateTime time;

    sys_test_time_set_utc(&(DateTime){2001, 3, 2, 5, 7, 8, 9, 123456789}, 52);
    time_get_local(&time);
    CHECK_INTEQUAL(time.year,    2001);
    CHECK_INTEQUAL(time.month,   3);
    CHECK_INTEQUAL(time.day,     2);
    CHECK_INTEQUAL(time.weekday, 5);
    CHECK_INTEQUAL(time.hour,    8);
    CHECK_INTEQUAL(time.minute,  0);
    CHECK_INTEQUAL(time.second,  9);
    CHECK_INTEQUAL(time.nsec,    123456789);

    /* Also check overflowing by multiple hours at once. */
    sys_test_time_set_utc(&(DateTime){2001, 3, 2, 5, 7, 8, 9, 123456789}, 352);
    time_get_local(&time);
    CHECK_INTEQUAL(time.year,    2001);
    CHECK_INTEQUAL(time.month,   3);
    CHECK_INTEQUAL(time.day,     2);
    CHECK_INTEQUAL(time.weekday, 5);
    CHECK_INTEQUAL(time.hour,    13);
    CHECK_INTEQUAL(time.minute,  0);
    CHECK_INTEQUAL(time.second,  9);
    CHECK_INTEQUAL(time.nsec,    123456789);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_time_get_local_minute_underflow)
{
    DateTime time;

    sys_test_time_set_utc(&(DateTime){2001, 3, 2, 5, 7, 8, 9, 123456789}, -9);
    time_get_local(&time);
    CHECK_INTEQUAL(time.year,    2001);
    CHECK_INTEQUAL(time.month,   3);
    CHECK_INTEQUAL(time.day,     2);
    CHECK_INTEQUAL(time.weekday, 5);
    CHECK_INTEQUAL(time.hour,    6);
    CHECK_INTEQUAL(time.minute,  59);
    CHECK_INTEQUAL(time.second,  9);
    CHECK_INTEQUAL(time.nsec,    123456789);

    /* Also check underflowing by multiple hours at once. */
    sys_test_time_set_utc(&(DateTime){2001, 3, 2, 5, 7, 8, 9, 123456789}, -309);
    time_get_local(&time);
    CHECK_INTEQUAL(time.year,    2001);
    CHECK_INTEQUAL(time.month,   3);
    CHECK_INTEQUAL(time.day,     2);
    CHECK_INTEQUAL(time.weekday, 5);
    CHECK_INTEQUAL(time.hour,    1);
    CHECK_INTEQUAL(time.minute,  59);
    CHECK_INTEQUAL(time.second,  9);
    CHECK_INTEQUAL(time.nsec,    123456789);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_time_get_local_hour_overflow)
{
    sys_test_time_set_utc(&(DateTime){2001, 3, 2, 5, 23, 8, 9, 123456789}, 52);

    DateTime time;
    time_get_local(&time);
    CHECK_INTEQUAL(time.year,    2001);
    CHECK_INTEQUAL(time.month,   3);
    CHECK_INTEQUAL(time.day,     3);
    CHECK_INTEQUAL(time.weekday, 6);
    CHECK_INTEQUAL(time.hour,    0);
    CHECK_INTEQUAL(time.minute,  0);
    CHECK_INTEQUAL(time.second,  9);
    CHECK_INTEQUAL(time.nsec,    123456789);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_time_get_local_hour_underflow)
{
    sys_test_time_set_utc(&(DateTime){2001, 3, 2, 5, 0, 8, 9, 123456789}, -9);

    DateTime time;
    time_get_local(&time);
    CHECK_INTEQUAL(time.year,    2001);
    CHECK_INTEQUAL(time.month,   3);
    CHECK_INTEQUAL(time.day,     1);
    CHECK_INTEQUAL(time.weekday, 4);
    CHECK_INTEQUAL(time.hour,    23);
    CHECK_INTEQUAL(time.minute,  59);
    CHECK_INTEQUAL(time.second,  9);
    CHECK_INTEQUAL(time.nsec,    123456789);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_time_get_local_day_overflow)
{
    sys_test_time_set_utc(&(DateTime){2001, 3, 31, 6, 23, 8, 9, 123456789}, 52);

    DateTime time;
    time_get_local(&time);
    CHECK_INTEQUAL(time.year,    2001);
    CHECK_INTEQUAL(time.month,   4);
    CHECK_INTEQUAL(time.day,     1);
    CHECK_INTEQUAL(time.weekday, 0);
    CHECK_INTEQUAL(time.hour,    0);
    CHECK_INTEQUAL(time.minute,  0);
    CHECK_INTEQUAL(time.second,  9);
    CHECK_INTEQUAL(time.nsec,    123456789);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_time_get_local_day_underflow)
{
    sys_test_time_set_utc(&(DateTime){2001, 4, 1, 0, 0, 8, 9, 123456789}, -9);

    DateTime time;
    time_get_local(&time);
    CHECK_INTEQUAL(time.year,    2001);
    CHECK_INTEQUAL(time.month,   3);
    CHECK_INTEQUAL(time.day,     31);
    CHECK_INTEQUAL(time.weekday, 6);
    CHECK_INTEQUAL(time.hour,    23);
    CHECK_INTEQUAL(time.minute,  59);
    CHECK_INTEQUAL(time.second,  9);
    CHECK_INTEQUAL(time.nsec,    123456789);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_time_get_local_month_overflow)
{
    sys_test_time_set_utc(&(DateTime){2001, 12, 31, 1, 23, 8, 9, 123456789}, 52);

    DateTime time;
    time_get_local(&time);
    CHECK_INTEQUAL(time.year,    2002);
    CHECK_INTEQUAL(time.month,   1);
    CHECK_INTEQUAL(time.day,     1);
    CHECK_INTEQUAL(time.weekday, 2);
    CHECK_INTEQUAL(time.hour,    0);
    CHECK_INTEQUAL(time.minute,  0);
    CHECK_INTEQUAL(time.second,  9);
    CHECK_INTEQUAL(time.nsec,    123456789);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_time_get_local_month_underflow)
{
    sys_test_time_set_utc(&(DateTime){2002, 1, 1, 2, 0, 8, 9, 123456789}, -9);

    DateTime time;
    time_get_local(&time);
    CHECK_INTEQUAL(time.year,    2001);
    CHECK_INTEQUAL(time.month,   12);
    CHECK_INTEQUAL(time.day,     31);
    CHECK_INTEQUAL(time.weekday, 1);
    CHECK_INTEQUAL(time.hour,    23);
    CHECK_INTEQUAL(time.minute,  59);
    CHECK_INTEQUAL(time.second,  9);
    CHECK_INTEQUAL(time.nsec,    123456789);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_time_get_local_month_lengths)
{
    DateTime time;

    sys_test_time_set_utc(&(DateTime){2001, 1, 1, 1, 0, 0, 0, 0}, -1);
    time_get_local(&time);
    CHECK_INTEQUAL(time.month, 12);
    CHECK_INTEQUAL(time.day,   31);
    sys_test_time_set_utc(&time, 1);
    time_get_local(&time);
    CHECK_INTEQUAL(time.month, 1);
    CHECK_INTEQUAL(time.day,   1);

    sys_test_time_set_utc(&(DateTime){2001, 2, 1, 4, 0, 0, 0, 0}, -1);
    time_get_local(&time);
    CHECK_INTEQUAL(time.month, 1);
    CHECK_INTEQUAL(time.day,   31);
    sys_test_time_set_utc(&time, 1);
    time_get_local(&time);
    CHECK_INTEQUAL(time.month, 2);
    CHECK_INTEQUAL(time.day,   1);

    sys_test_time_set_utc(&(DateTime){2001, 3, 1, 4, 0, 0, 0, 0}, -1);
    time_get_local(&time);
    CHECK_INTEQUAL(time.month, 2);
    CHECK_INTEQUAL(time.day,   28);
    sys_test_time_set_utc(&time, 1);
    time_get_local(&time);
    CHECK_INTEQUAL(time.month, 3);
    CHECK_INTEQUAL(time.day,   1);

    sys_test_time_set_utc(&(DateTime){2001, 4, 1, 0, 0, 0, 0, 0}, -1);
    time_get_local(&time);
    CHECK_INTEQUAL(time.month, 3);
    CHECK_INTEQUAL(time.day,   31);
    sys_test_time_set_utc(&time, 1);
    time_get_local(&time);
    CHECK_INTEQUAL(time.month, 4);
    CHECK_INTEQUAL(time.day,   1);

    sys_test_time_set_utc(&(DateTime){2001, 5, 1, 2, 0, 0, 0, 0}, -1);
    time_get_local(&time);
    CHECK_INTEQUAL(time.month, 4);
    CHECK_INTEQUAL(time.day,   30);
    sys_test_time_set_utc(&time, 1);
    time_get_local(&time);
    CHECK_INTEQUAL(time.month, 5);
    CHECK_INTEQUAL(time.day,   1);

    sys_test_time_set_utc(&(DateTime){2001, 6, 1, 5, 0, 0, 0, 0}, -1);
    time_get_local(&time);
    CHECK_INTEQUAL(time.month, 5);
    CHECK_INTEQUAL(time.day,   31);
    sys_test_time_set_utc(&time, 1);
    time_get_local(&time);
    CHECK_INTEQUAL(time.month, 6);
    CHECK_INTEQUAL(time.day,   1);

    sys_test_time_set_utc(&(DateTime){2001, 7, 1, 0, 0, 0, 0, 0}, -1);
    time_get_local(&time);
    CHECK_INTEQUAL(time.month, 6);
    CHECK_INTEQUAL(time.day,   30);
    sys_test_time_set_utc(&time, 1);
    time_get_local(&time);
    CHECK_INTEQUAL(time.month, 7);
    CHECK_INTEQUAL(time.day,   1);

    sys_test_time_set_utc(&(DateTime){2001, 8, 1, 3, 0, 0, 0, 0}, -1);
    time_get_local(&time);
    CHECK_INTEQUAL(time.month, 7);
    CHECK_INTEQUAL(time.day,   31);
    sys_test_time_set_utc(&time, 1);
    time_get_local(&time);
    CHECK_INTEQUAL(time.month, 8);
    CHECK_INTEQUAL(time.day,   1);

    sys_test_time_set_utc(&(DateTime){2001, 9, 1, 6, 0, 0, 0, 0}, -1);
    time_get_local(&time);
    CHECK_INTEQUAL(time.month, 8);
    CHECK_INTEQUAL(time.day,   31);
    sys_test_time_set_utc(&time, 1);
    time_get_local(&time);
    CHECK_INTEQUAL(time.month, 9);
    CHECK_INTEQUAL(time.day,   1);

    sys_test_time_set_utc(&(DateTime){2001, 10, 1, 1, 0, 0, 0, 0}, -1);
    time_get_local(&time);
    CHECK_INTEQUAL(time.month, 9);
    CHECK_INTEQUAL(time.day,   30);
    sys_test_time_set_utc(&time, 1);
    time_get_local(&time);
    CHECK_INTEQUAL(time.month, 10);
    CHECK_INTEQUAL(time.day,   1);

    sys_test_time_set_utc(&(DateTime){2001, 11, 1, 4, 0, 0, 0, 0}, -1);
    time_get_local(&time);
    CHECK_INTEQUAL(time.month, 10);
    CHECK_INTEQUAL(time.day,   31);
    sys_test_time_set_utc(&time, 1);
    time_get_local(&time);
    CHECK_INTEQUAL(time.month, 11);
    CHECK_INTEQUAL(time.day,   1);

    sys_test_time_set_utc(&(DateTime){2001, 12, 1, 6, 0, 0, 0, 0}, -1);
    time_get_local(&time);
    CHECK_INTEQUAL(time.month, 11);
    CHECK_INTEQUAL(time.day,   30);
    sys_test_time_set_utc(&time, 1);
    time_get_local(&time);
    CHECK_INTEQUAL(time.month, 12);
    CHECK_INTEQUAL(time.day,   1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_time_get_local_leap_years)
{
    DateTime time;

    /* Divisible by 4, not divisible by 100 (leap year) */
    sys_test_time_set_utc(&(DateTime){2028, 3, 1, 3, 0, 0, 0, 0}, -1);
    time_get_local(&time);
    CHECK_INTEQUAL(time.month, 2);
    CHECK_INTEQUAL(time.day,   29);
    sys_test_time_set_utc(&(DateTime){2028, 2, 28, 1, 23, 59, 0, 0}, 1);
    time_get_local(&time);
    CHECK_INTEQUAL(time.month, 2);
    CHECK_INTEQUAL(time.day,   29);
    sys_test_time_set_utc(&(DateTime){2028, 2, 29, 2, 23, 59, 0, 0}, 1);
    time_get_local(&time);
    CHECK_INTEQUAL(time.month, 3);
    CHECK_INTEQUAL(time.day,   1);

    /* Divisible by 100, not divisible by 400 (not a leap year) */
    sys_test_time_set_utc(&(DateTime){2100, 3, 1, 1, 0, 0, 0, 0}, -1);
    time_get_local(&time);
    CHECK_INTEQUAL(time.month, 2);
    CHECK_INTEQUAL(time.day,   28);
    sys_test_time_set_utc(&(DateTime){2100, 2, 28, 0, 23, 59, 0, 0}, 1);
    time_get_local(&time);
    CHECK_INTEQUAL(time.month, 3);
    CHECK_INTEQUAL(time.day,   1);

    /* Divisible by 400 (leap year) */
    sys_test_time_set_utc(&(DateTime){2000, 3, 1, 3, 0, 0, 0, 0}, -1);
    time_get_local(&time);
    CHECK_INTEQUAL(time.month, 2);
    CHECK_INTEQUAL(time.day,   29);
    sys_test_time_set_utc(&(DateTime){2000, 2, 28, 1, 23, 59, 0, 0}, 1);
    time_get_local(&time);
    CHECK_INTEQUAL(time.month, 2);
    CHECK_INTEQUAL(time.day,   29);
    sys_test_time_set_utc(&(DateTime){2000, 2, 29, 2, 23, 59, 0, 0}, 1);
    time_get_local(&time);
    CHECK_INTEQUAL(time.month, 3);
    CHECK_INTEQUAL(time.day,   1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_time_get_local_invalid)
{
    time_get_local(NULL);  // Just make sure it doesn't crash.
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_time_is_utc_before)
{
    sys_test_time_set_utc(&(DateTime){2001, 3, 2, 5, 7, 8, 9, 123456789}, 30);

    CHECK_TRUE(time_is_utc_before(
                   &(DateTime){2002, 3, 2, 5, 7, 8, 9, 123456789}));
    CHECK_FALSE(time_is_utc_before(
                   &(DateTime){2000, 3, 2, 5, 7, 8, 9, 123456789}));

    CHECK_TRUE(time_is_utc_before(
                   &(DateTime){2001, 4, 2, 1, 7, 8, 9, 123456789}));
    CHECK_FALSE(time_is_utc_before(
                   &(DateTime){2001, 2, 2, 5, 7, 8, 9, 123456789}));

    CHECK_TRUE(time_is_utc_before(
                   &(DateTime){2001, 3, 3, 6, 7, 8, 9, 123456789}));
    CHECK_FALSE(time_is_utc_before(
                   &(DateTime){2001, 3, 1, 4, 7, 8, 9, 123456789}));

    CHECK_TRUE(time_is_utc_before(
                   &(DateTime){2001, 3, 2, 5, 8, 8, 9, 123456789}));
    CHECK_FALSE(time_is_utc_before(
                   &(DateTime){2001, 3, 2, 5, 6, 8, 9, 123456789}));

    CHECK_TRUE(time_is_utc_before(
                   &(DateTime){2001, 3, 2, 5, 7, 9, 9, 123456789}));
    CHECK_FALSE(time_is_utc_before(
                   &(DateTime){2001, 3, 2, 5, 7, 7, 9, 123456789}));

    CHECK_TRUE(time_is_utc_before(
                   &(DateTime){2001, 3, 2, 5, 7, 8, 10, 123456789}));
    CHECK_FALSE(time_is_utc_before(
                   &(DateTime){2001, 3, 2, 5, 7, 8, 8, 123456789}));

    CHECK_TRUE(time_is_utc_before(
                   &(DateTime){2001, 3, 2, 5, 7, 8, 9, 123456790}));
    CHECK_FALSE(time_is_utc_before(
                   &(DateTime){2001, 3, 2, 5, 7, 8, 9, 123456788}));

    /* Should return false for an exact match. */
    CHECK_FALSE(time_is_utc_before(
                   &(DateTime){2001, 3, 2, 5, 7, 8, 9, 123456789}));

    /* An incorrect weekday should not affect the result. */
    CHECK_FALSE(time_is_utc_before(
                   &(DateTime){2001, 3, 2, 4, 7, 8, 9, 123456789}));
    CHECK_FALSE(time_is_utc_before(
                   &(DateTime){2001, 3, 2, 6, 7, 8, 9, 123456789}));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_time_is_utc_before_invalid)
{
    time_is_utc_before(NULL);  // Just make sure it doesn't crash.
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_time_is_utc_after)
{
    sys_test_time_set_utc(&(DateTime){2001, 3, 2, 5, 7, 8, 9, 123456789}, 30);

    CHECK_FALSE(time_is_utc_after(
                   &(DateTime){2002, 3, 2, 5, 7, 8, 9, 123456789}));
    CHECK_TRUE(time_is_utc_after(
                   &(DateTime){2000, 3, 2, 5, 7, 8, 9, 123456789}));

    CHECK_FALSE(time_is_utc_after(
                   &(DateTime){2001, 4, 2, 1, 7, 8, 9, 123456789}));
    CHECK_TRUE(time_is_utc_after(
                   &(DateTime){2001, 2, 2, 5, 7, 8, 9, 123456789}));

    CHECK_FALSE(time_is_utc_after(
                   &(DateTime){2001, 3, 3, 6, 7, 8, 9, 123456789}));
    CHECK_TRUE(time_is_utc_after(
                   &(DateTime){2001, 3, 1, 4, 7, 8, 9, 123456789}));

    CHECK_FALSE(time_is_utc_after(
                   &(DateTime){2001, 3, 2, 5, 8, 8, 9, 123456789}));
    CHECK_TRUE(time_is_utc_after(
                   &(DateTime){2001, 3, 2, 5, 6, 8, 9, 123456789}));

    CHECK_FALSE(time_is_utc_after(
                   &(DateTime){2001, 3, 2, 5, 7, 9, 9, 123456789}));
    CHECK_TRUE(time_is_utc_after(
                   &(DateTime){2001, 3, 2, 5, 7, 7, 9, 123456789}));

    CHECK_FALSE(time_is_utc_after(
                   &(DateTime){2001, 3, 2, 5, 7, 8, 10, 123456789}));
    CHECK_TRUE(time_is_utc_after(
                   &(DateTime){2001, 3, 2, 5, 7, 8, 8, 123456789}));

    CHECK_FALSE(time_is_utc_after(
                   &(DateTime){2001, 3, 2, 5, 7, 8, 9, 123456790}));
    CHECK_TRUE(time_is_utc_after(
                   &(DateTime){2001, 3, 2, 5, 7, 8, 9, 123456788}));

    /* Should return true for an exact match. */
    CHECK_TRUE(time_is_utc_after(
                   &(DateTime){2001, 3, 2, 5, 7, 8, 9, 123456789}));

    /* An incorrect weekday should not affect the result. */
    CHECK_TRUE(time_is_utc_after(
                   &(DateTime){2001, 3, 2, 4, 7, 8, 9, 123456789}));
    CHECK_TRUE(time_is_utc_after(
                   &(DateTime){2001, 3, 2, 6, 7, 8, 9, 123456789}));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_time_is_utc_after_invalid)
{
    time_is_utc_after(NULL);  // Just make sure it doesn't crash.
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_time_mark)
{
    sys_test_time_set_seconds(0.0);
    time_mark_reset();
    time_delay(1.0f);
    time_mark(123);
    time_delay(1.0f);
    time_mark(456);
    time_delay(1.0f);
    CHECK_DOUBLEEQUAL(time_get_mark(123), 1.0);
    CHECK_DOUBLEEQUAL(time_get_mark(456), 2.0);
    CHECK_DOUBLEEQUAL(time_get_mark(789), -1.0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_time_mark_overflow)
{
    sys_test_time_set_seconds(0.0);
    time_mark_reset();
    for (int i = 1; i <= TIME_MAX_MARKS+1; i++) {
        time_delay(1.0f);
        time_mark(i);
    }
    for (int i = 1; i <= TIME_MAX_MARKS; i++) {
        CHECK_DOUBLEEQUAL(time_get_mark(i), i);
    }
    CHECK_DOUBLEEQUAL(time_get_mark(TIME_MAX_MARKS+1), -1.0);

    return 1;
}

/*************************************************************************/
/*************************************************************************/
