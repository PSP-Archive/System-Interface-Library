/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/utility/strtof.c: Custom implementation of the standard strtof()
 * function which uses only single-precision arithmetic.  This
 * implementation does not accept hexadecimal numbers or NaNs, and may
 * introduce a slight amount of error (on the order of 1 part in 10^7) in
 * the least-significant digits of the mantissa.
 */

#include "src/base.h"
#include "src/math.h"

#if defined(SIL_UTILITY_INCLUDE_STRTOF) || defined(SIL_INCLUDE_TESTS)
/* To the end of the file. */

/* If we're including the function solely for testing, it won't have been
 * renamed in base.h, so rename it here. */
#if !defined(SIL_UTILITY_INCLUDE_STRTOF)
# define strtof strtof_SIL
#endif

/*************************************************************************/
/*************************************************************************/

float strtof(const char *s, char **endptr)
{
    /* Work around attribute((nonnull)) bogosity which would prevent us
     * from checking for nullness of the input string. */
#if defined(__clang__)
    /* Clang 4.0 (at least) doesn't recognize that s has changed after an
     * asm statement that writes to it, which probably means there are all
     * sorts of wrong-code bugs hiding around asm statements... */
    const char * volatile foo = s;
    s = foo;
#elif defined(__GNUC__)
    __asm__ volatile("": "=r" (s): "0" (s));
#endif

    if (UNLIKELY(!s)) {
        if (endptr) {
            *endptr = NULL;
        }
        errno = EFAULT;
        return 0;
    }

    /* Array of powers of ten as floating point values. */
    static const float ten_to_the[] = {
        1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,  1e8,  1e9,
        1e10, 1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19,
        1e20, 1e21, 1e22, 1e23, 1e24, 1e25, 1e26, 1e27, 1e28, 1e29,
        1e30, 1e31, 1e32, 1e33, 1e34, 1e35, 1e36, 1e37, 1e38
    };

    /* Also define this array of negative powers so we can multiply by
     * them, which is faster than dividing by a positive power. */
    static const float ten_to_the_minus[] = {
        1e0,   1e-1,  1e-2,  1e-3,  1e-4,  1e-5,  1e-6,  1e-7,  1e-8,  1e-9,
        1e-10, 1e-11, 1e-12, 1e-13, 1e-14, 1e-15, 1e-16, 1e-17, 1e-18, 1e-19,
        1e-20, 1e-21, 1e-22, 1e-23, 1e-24, 1e-25, 1e-26, 1e-27, 1e-28, 1e-29,
        1e-30, 1e-31, 1e-32, 1e-33, 1e-34, 1e-35, 1e-36, 1e-37
    };

    int32_t value = 0; // Significant digits read, as an integer value.
    int sig_digs = 0;  // Number of significant digits read.
    int exponent = 0;  // Exponent to apply to "value".
    int negative = 0;  // Is the value negative?
    int saw_point = 0; // Have we seen a decimal point?


    /* Store the original string pointer in *endptr in case a conversion
     * error occurs before the first digit or decimal point.  (This way we
     * don't need an extra variable to hold the original pointer value.) */

    if (endptr) {
        *endptr = (char *)s;
    }

    /* Skip any leading spaces. */

    while (*s != 0 && *s == ' ') {
        s++;
    }
    if (*s == 0) {
        return 0;
    }

    /* Check for an optional sign character. */

    if (*s == '+') {
        s++;
    } else if (*s == '-') {
        negative = 1;
        s++;
    }

    /* Check for an infinite value. */

    if ((s[0]=='i' || s[0]=='I')
     && (s[1]=='n' || s[1]=='N')
     && (s[2]=='f' || s[2]=='F')) {
        if (endptr) {
            *endptr = (char *)(s+3);
        }
        return negative ? -INFINITY : INFINITY;
    }

    /* The first character must be either a digit or a decimal point. */

    if (!(*s >= '0' && *s <= '9') && *s != '.') {
        return 0;
    }

    /* Parse all the digits we can read.  However, we only store the first
     * eight significant digits, since that's sufficient to represent the
     * mantissa of any single-precision floating point value.  (The value
     * may be rounded incorrectly in certain edge cases, such as the
     * integer value 134217726 which will get rounded down to 134217720
     * instead of up to 134217728, but we don't worry about those cases.)
     *
     * Note that from this point on, we always return a valid value (or
     * ERANGE) as a result of the conversion. */

    do {
        if (*s == '.') {
            if (saw_point) {
                goto stop_conversion;
            } else {
                saw_point = 1;
            }
        } else {  // *s is a digit.
            if (sig_digs < 8) {
                value = (value * 10) + (*s - '0');
                /* Leading 0s don't count as significant digits. */
                if (value != 0) {
                    sig_digs++;
                }
                if (saw_point) {
                    /* We assume this decrement will never underflow.  This
                     * can only happen while parsing a string with more
                     * zeros after the decimal point than the magnitude of
                     * the most negative int value, e.g. 2^31+1 (or more)
                     * zeros in an environment with 32-bit ints.  If you
                     * need to do that, then don't use this function. (: */
                    exponent--;
                }
            } else if (!saw_point) {
                /* As above, we assume this increment will never overflow,
                 * which it won't without an additional int-overflow number
                 * of digits before the decimal point. */
                exponent++;
            }
        }
        s++;
    } while ((*s >= '0' && *s <= '9') || *s == '.');

    /* Check for the presence of a trailing exponent. */

    if (*s == 'e' || *s == 'E') {

        int exp_value = 0;     // Value read after the "e".
        int exp_negative = 0;  // Is the specified exponent negative?

        /* Check for a leading sign, and make sure there's at least one
         * exponent digit. */

        s++;
        if (*s == '+' || *s == '-') {
            exp_negative = (*s == '-');
            s++;
            if (!(*s >= '0' && *s <= '9')) {
                s -= 2;  // Reject the "e" and sign.
                goto stop_conversion;
            }
        } else if (!(*s >= '0' && *s <= '9')) {
            s--;  // Reject the "e".
            goto stop_conversion;
        }

        /* Read the absolute value of the exponent.  We store up to three
         * digits, at which point we will know the value overflows (but we
         * must still read to the end of the digit string). */

        do {
            if (exp_value < 100) {
                exp_value = (exp_value * 10) + (*s - '0');
            }
            s++;
        } while (*s >= '0' && *s <= '9');

        /* Add the parsed exponent value to the exponent we calculated
         * while reading the mantissa. */

        if (exp_negative) {
            exponent -= exp_value;
        } else {
            exponent += exp_value;
        }

    }

    /* We've parsed everything we can, so convert the value and return.
     * We also jump here at certain points above if we encounter an invalid
     * character in the string. */

  stop_conversion:;

    float result;

    if (value == 0) {

        /* A zero mantissa is always a zero value, regardless of exponent. */
        result = 0;

    } else if (exponent + (sig_digs - 1) > 38
            || exponent + (sig_digs - 1) < -38) {

        /* The value is known to be out of range (overflow or underflow). */
        result = (exponent + (sig_digs - 1) > 38) ? HUGE_VALF : 0;
        errno = ERANGE;

    } else if (exponent >= 0) {

#if defined(SIL_PLATFORM_PSP) && defined(DEBUG)
        /* In debug mode, the PSP's floating point overflow exception is
         * enabled by main.c.  Disable it here so we can safely generate
         * infinity on overflow. */
        uint32_t old_fcr31, temp;
        __asm__ volatile("cfc1 %[old_fcr31], $31\n"
                         "move %[temp], %[old_fcr31]\n"
                         "ins %[temp], $zero, 9, 1\n"
                         "ctc1 %[temp], $31"
                         : [old_fcr31] "=r" (old_fcr31), [temp] "=r" (temp));
#endif
        result = value * ten_to_the[exponent];
#if defined(SIL_PLATFORM_PSP) && defined(DEBUG)
        /* Restore the old exception flags. */
        __asm__ volatile("ctc1 %[old_fcr31], $31"
                         : /* no outputs */
                         : [old_fcr31] "r" (old_fcr31));
#endif
        if (result == INFINITY) {
            result = HUGE_VALF;
            errno = ERANGE;
        }

    } else {  // exponent < 0

        /* Since "value" is an integer rather than a normalized mantissa,
         * we could have an exponent outside the single-precision range but
         * still end up with a valid result: for example, 10^-41 is too
         * small to represent in a (normalized) single-precision variable,
         * but 12345e-41 == 1.2345e-37, which does fit.  So we need to
         * check for that case and pre-divide by enough powers of ten to
         * get the exponent within range of a single-precision value. */

        if (exponent < -37) {
            result = value * ten_to_the_minus[sig_digs];
            exponent += sig_digs;  // Sum is known to be no smaller than -37.
        } else {
            result = value;
        }

        result *= ten_to_the_minus[-exponent];
        if (result == 0) {
            errno = ERANGE;
        }

    }

    if (endptr) {
        *endptr = (char *)s;
    }
    return negative ? -result : result;
}

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_UTILITY_INCLUDE_STRTOF || SIL_INCLUDE_TESTS
