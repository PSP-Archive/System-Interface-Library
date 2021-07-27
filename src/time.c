/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/time.c: Timekeeping routines.
 */

#include "src/base.h"
#include "src/math.h"
#include "src/sysdep.h"
#include "src/time.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* sys_time_now() time units per second. */
static uint64_t sys_unit;

/* sys_time_now() time unit in seconds. */
static double sys_unit_in_sec;

/* sys_time_now() value when time_init() was last called. */
static uint64_t sys_epoch;

/*-----------------------------------------------------------------------*/

#ifdef DEBUG

/* Time at which time_mark_reset() was called. */
static uint64_t mark_base;

/* Mark table for time_mark() and time_get_mark(). */
static struct {
    int mark;
    uint64_t time;
} marks[TIME_MAX_MARKS];
static unsigned int next_mark;

#endif

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * days_for_month:  Return the number of days in the given month of the
 * given year.
 */
static inline CONST_FUNCTION int days_for_month(int year, int month);

/*************************************************************************/
/*********************** Interface: Core routines ************************/
/*************************************************************************/

void time_init(void)
{
    sys_time_init();
    sys_unit = sys_time_unit();
    sys_unit_in_sec = 1.0 / sys_unit;
    sys_epoch = sys_time_now();
}

/*-----------------------------------------------------------------------*/

double time_now(void)
{
    /*
     * Implementation note:  This function returns a double-precision
     * rather than a single-precision value to minimize loss of precision
     * over time.  Since a single-precision value only has 24 mantissa bits
     * (including the assumed leading "1"), the precision of the value
     * drops to 1ms after only 8192 seconds, or a bit more than 2 hours;
     * this loss of precision is sufficient to cause jitter for some usage
     * patterns.  Likewise, we set the epoch to the time of the last call
     * to time_init() to avoid wasting bits of precision on a large initial
     * value.
     *
     * We use floating-point multiplication by the inverse of the time unit
     * because it's generally faster than floating-point division, though
     * it may introduce slight (typically insignificant) inaccuracies.
     */

    const uint64_t time = sys_time_now() - sys_epoch;
#if defined(SIL_ARCH_MIPS_32) && (defined(__GNUC__) && !defined(__clang__))
    /* Current versions of GCC are horribly inefficient at converting
     * uint64_t to double, so do it manually -- shift the 64-bit integer
     * into the mantissa of a double and set the exponent appropriately. */
    double now;
    __asm__(
        ".set push; .set noreorder\n"
        "or $t2, %[time_lo], %[time_hi]\n"  // Early check for zero
        "beqzl $t2, 9f\n"
        "move $t3, $zero\n"
        "clz $a0, %[time_lo]\n"
        "clz $a1, %[time_hi]\n"
        "addiu $a2, $a0, 32\n"
        "movn $a2, $a1, %[time_hi]\n"  // Number of leading zero bits
        "addiu $a3, $a2, -11\n"
        "beqzl $a3, 1f\n"
        "nop\n"                 // If there's no need to shift
        "bltz $a3, 0f\n"

        "li $t0, 32\n"          // If we need to shift left
        "subu $t0, $t0, $a3\n"
        "srlv $a1, %[time_lo], $t0\n"
        "sllv $t2, %[time_lo], $a3\n"
        "sllv $t3, %[time_hi], $a3\n"
        "or $t3, $t3, $a1\n"
        "slti $t1, $a3, 32\n"
        "movz $t3, $t2, $t1\n"
        "b 1f\n"
        "movz $t2, $zero, $t1\n"

        "0:\n"                  // If we need to shift right
        "negu $a3, $a3\n"
        "li $t0, 32\n"
        "subu $t0, $t0, $a3\n"
        "sllv $a0, %[time_hi], $t0\n"
        "srlv $t2, %[time_lo], $a3\n"
        "srlv $t3, %[time_hi], $a3\n"
        "or $t2, $t2, $a0\n"

        "1:\n"                  // Common finalization
#if defined(SIL_ARCH_MIPS_MIPS32R2) || defined(SIL_PLATFORM_PSP)
        "ext $t3, $t3, 0, 20\n"
#else
        "and $t3, $t3, %[xFFFFF]\n"
#endif
        "li $a0, 0x43E\n"
        "subu $a0, $a0, $a2\n"
#if defined(SIL_ARCH_MIPS_MIPS32R2) || defined(SIL_PLATFORM_PSP)
        "ins $t3, $a0, 20, 11\n"
#else
        "andi $a0, $a0, 0x3FF\n"
        "sll $a0, $a0, 20\n"
        "or $t3, $t3, $a0\n"
#endif
        "9:\n"
        "sw $t2, %[now]\n"
        "sw $t3, 4+%[now]\n"

        ".set pop"
        : [now] "=m" (now)
        : [time_lo] "r" ((uint32_t)time), [time_hi] "r" ((uint32_t)(time>>32))
#if !(defined(SIL_ARCH_MIPS_MIPS32R2) || defined(SIL_PLATFORM_PSP))
          , [xFFFFF] "r" (0xFFFFF)
#endif
        : "a0", "a1", "a2", "a3", "t0", "t1", "t2", "t3"
    );
    return now * sys_unit_in_sec;
#else  // !(SIL_ARCH_MIPS_32 && (__GNUC__ && !__clang__))
    return time * sys_unit_in_sec;
#endif
}

/*-----------------------------------------------------------------------*/

void time_delay(double time)
{
    while (time >= 60) {
        sys_time_delay(60 * sys_unit);
        time -= 60;
    }
    sys_time_delay((uint64_t)ceil(lbound(time, 0) * (double)sys_unit));
}

/*-----------------------------------------------------------------------*/

void time_delay_until(double target)
{
    time_delay(target - time_now());
}

/*************************************************************************/
/************** Interface: Real (wall-clock) time routines ***************/
/*************************************************************************/

void time_get_utc(DateTime *time_ret)
{
    if (UNLIKELY(!time_ret)) {
        DLOG("time_ret == NULL");
        return;
    }

    sys_time_get_utc(time_ret);
}

/*-----------------------------------------------------------------------*/

int time_get_local(DateTime *time_ret)
{
    if (UNLIKELY(!time_ret)) {
        DLOG("time_ret == NULL");
        return 0;
    }

    int utc_offset = sys_time_get_utc(time_ret);
    ASSERT(utc_offset > -1440, utc_offset %= 1440);
    ASSERT(utc_offset < 1440, utc_offset %= 1440);

    /* This addition will probably overflow the 8-bit minutes field, so
     * pull the value out into a local variable. */
    int minute = time_ret->minute + utc_offset;
    if (minute < 0) {
        minute += 1440;
        time_ret->hour -= 24;
    }
    time_ret->hour += minute / 60;
    time_ret->minute = minute % 60;

    if (time_ret->hour < 0) {
        time_ret->hour += 24;
        time_ret->weekday = (time_ret->weekday + 6) % 7;
        time_ret->day--;
        if (time_ret->day < 1) {
            time_ret->month--;
            if (time_ret->month < 1) {
                time_ret->month = 12;
                time_ret->year--;
            }
            time_ret->day = days_for_month(time_ret->year, time_ret->month);
        }
    } else if (time_ret->hour > 23) {
        time_ret->hour -= 24;
        time_ret->weekday = (time_ret->weekday + 1) % 7;
        time_ret->day++;
        if (time_ret->day > days_for_month(time_ret->year, time_ret->month)) {
            time_ret->day = 1;
            time_ret->month++;
            if (time_ret->month > 12) {
                time_ret->month = 1;
                time_ret->year++;
            }
        }
    }

    return utc_offset;
}

/*-----------------------------------------------------------------------*/

int time_is_utc_before(const DateTime *time)
{
    if (UNLIKELY(!time)) {
        DLOG("time == NULL");
        return 0;
    }

    DateTime now;
    sys_time_get_utc(&now);
    if (now.year < time->year) return 1;
    if (now.year > time->year) return 0;
    if (now.month < time->month) return 1;
    if (now.month > time->month) return 0;
    if (now.day < time->day) return 1;
    if (now.day > time->day) return 0;
    if (now.hour < time->hour) return 1;
    if (now.hour > time->hour) return 0;
    if (now.minute < time->minute) return 1;
    if (now.minute > time->minute) return 0;
    if (now.second < time->second) return 1;
    if (now.second > time->second) return 0;
    return now.nsec < time->nsec;
}

/*************************************************************************/
/**************** Interface: Time markers (for debugging) ****************/
/*************************************************************************/

#ifdef DEBUG

/*-----------------------------------------------------------------------*/

void time_mark_reset(void)
{
    mark_base = sys_time_now();
    next_mark = 0;
}

/*-----------------------------------------------------------------------*/

void time_mark(int mark)
{
    if (LIKELY(next_mark < lenof(marks))) {
        marks[next_mark].mark = mark;
        marks[next_mark].time = sys_time_now();
        next_mark++;
    }
}

/*-----------------------------------------------------------------------*/

double time_get_mark(int mark)
{
    for (unsigned int i = 0; i < next_mark; i++) {
        if (marks[i].mark == mark) {
            return (marks[i].time - mark_base) * sys_unit_in_sec;
        }
    }
    return -1;
}

/*-----------------------------------------------------------------------*/

#endif  // DEBUG

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static inline int days_for_month(int year, int month)
{
    static int8_t days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (month != 2) {
        return days[month-1];
    } else if (year % 4 != 0) {
        return 28;
    } else if (year % 100 != 0) {
        return 29;
    } else if (year % 400 != 0) {
        return 28;
    } else {
        return 29;
    }
}

/*************************************************************************/
/*************************************************************************/
