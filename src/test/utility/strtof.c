/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/utility/strtof.c: Tests for strtof().
 */

#define FAIL_ACTION  failed = 1  // Continue through failures.

#include "src/base.h"
#include "src/math.h"
#include "src/test/base.h"

/* If we're not overriding the system library's strtof(), make sure we
 * test the SIL version and not the system one. */
#if !defined(SIL_UTILITY_INCLUDE_STRTOF)
# define strtof strtof_SIL
#endif

#ifdef __GNUC__
/* Disable -Wnonnull warnings for these tests, since we make some
 * deliberately invalid calls. */
# pragma GCC diagnostic ignored "-Wnonnull"
#endif

/*************************************************************************/
/************************ Helper macros/routines *************************/
/*************************************************************************/

/**
 * TRY_STRTOF:  Call strtof() and check the result against the expected
 * return value and number of bytes consumed (consume < 0 means "the
 * entire input string should be consumed"); also verify whether ERANGE
 * is generated when expected.  Note that we use a strict equality test
 * on the return value, so "expect" should be given to sufficient
 * precision to exactly specify the desired value.
 */
#define TRY_STRTOF(str,expect,consume,erange)  do { \
    const char *_errmsg = _try_strtof((str), (expect), (consume), (erange)); \
    if (_errmsg) { \
        FAIL("%s", _errmsg); \
    } \
} while (0)

/*-----------------------------------------------------------------------*/

/**
 * _try_strtof:  Helper function for the TRY_STRTOF() macro, used to reduce
 * code size.  Returns NULL if the test passes, otherwise an error message.
 */
static const char *_try_strtof(const char *str, float expect, int consume,
                               int erange)
{
    static char error_buf[1000];

    if (consume < 0) {
        consume = strlen(str);
    }

    char *end;
    errno = 0;
    float retval = strtof(str, &end);
    const int consumed = end - str;
    const int got_erange = (errno == ERANGE);

    if (retval != expect) {
        volatile float x;
        x = retval;
        const unsigned long retval_bits = FLOAT_BITS(x);
        x = expect;
        const unsigned long expect_bits = FLOAT_BITS(x);
        strformat(error_buf, sizeof(error_buf),
                  "strtof(\"%s\") returned %.8g (0x%08lX), expected %.8g"
                  " (0x%08lX)", str, retval, retval_bits, expect, expect_bits);
        return error_buf;
    }
    if (consumed != consume) {
        strformat(error_buf, sizeof(error_buf),
                  "strtof(\"%s\") consumed %d bytes, expected %d",
                  str, consumed, consume);
        return error_buf;
    }
    if ((erange != 0) != got_erange) {
        strformat(error_buf, sizeof(error_buf),
                  "strtof(\"%s\") %s ERANGE, but %s",
                  str, got_erange ? "generated" : "did not generate",
                  erange ? "should have" : "should not have");
        return error_buf;
    }
    return NULL;
}

/*************************************************************************/
/***************************** Test routine ******************************/
/*************************************************************************/

int test_utility_strtof(void)
{
#if defined(SIL_ARCH_X86) && defined(__GNUC__)
    {
        uint32_t mxcsr;
        __asm__("stmxcsr %0" : "=m" (mxcsr));
        if (!(mxcsr & (1<<6))) {
            SKIP("*** mxcsr.DAZ is not set.  (Are you running under"
                 " Valgrind?)");
        }
    }
#endif

    int failed = 0;

    /* First check simple cases that resolve to exact values. */

    TRY_STRTOF("",              0.0f,           -1, 0);
    TRY_STRTOF("0",             0.0f,           -1, 0);
    TRY_STRTOF("0.0",           0.0f,           -1, 0);
    TRY_STRTOF("0.",            0.0f,           -1, 0);
    TRY_STRTOF(".0",            0.0f,           -1, 0);
    TRY_STRTOF("1",             1.0000000f,     -1, 0);
    TRY_STRTOF(".5",            0.50000000f,    -1, 0);
    TRY_STRTOF("1.5",           1.5000000f,     -1, 0);
    TRY_STRTOF("+1",            1.0000000f,     -1, 0);
    TRY_STRTOF("+.5",           0.50000000f,    -1, 0);
    TRY_STRTOF("+1.5",          1.5000000f,     -1, 0);
    TRY_STRTOF("-1",           -1.0000000f,     -1, 0);
    TRY_STRTOF("-.5",          -0.50000000f,    -1, 0);
    TRY_STRTOF("-1.5",         -1.5000000f,     -1, 0);

    /* Try a few nonexact values and make sure we get the expected
     * approximations (these all assume IEEE 754 floating point format
     * and round-to-nearest mode). */

    TRY_STRTOF("1.2345678",     1.2345678f,     -1, 0);  // 0x3F9E0651
    /* 1 - 2^-25 = 0.9999999701976776123046875; since our algorithm
     * doesn't guarantee accuracy in the low-end digits, we test with
     * the largest value that rounds correctly.  (Simple explanation:
     * our strtof() parses the mantissa (up to 8 digits) as an integer
     * and converts to a float to multiply with the proper power of 10,
     * so e.g. 99999996 rounds to 100000000 even though 0.9999999[67]
     * would round to 0.99999994 in a perfect algorithm.) */
    TRY_STRTOF("0.99999995",    0.99999994f,    -1, 0);  // 0x3F7FFFFF
    TRY_STRTOF("0.99999998",    1.0000000f,     -1, 0);  // 0x3F800000
    TRY_STRTOF("1.0000001",     1.0000001f,     -1, 0);  // 0x3F800001

    /* Check that large numbers of decimal places are handled properly
     * (even though they won't show up in the returned value). */
    TRY_STRTOF("1.00000000001", 1.0000000f,     -1, 0);

    /* Check that exponential notation is handled properly. */

    TRY_STRTOF("1e1",           10.000000f,     -1, 0);
    TRY_STRTOF("1e+1",          10.000000f,     -1, 0);
    TRY_STRTOF("5e-1",          0.50000000f,    -1, 0);
    TRY_STRTOF("1.5e1",         15.000000f,     -1, 0);
    TRY_STRTOF("1.5e+1",        15.000000f,     -1, 0);
    TRY_STRTOF("2.5e-1",        0.25000000f,    -1, 0);
    TRY_STRTOF("1E1",           10.000000f,     -1, 0);
    TRY_STRTOF("1E+1",          10.000000f,     -1, 0);
    TRY_STRTOF("5E-1",          0.50000000f,    -1, 0);
    TRY_STRTOF("1.5E1",         15.000000f,     -1, 0);
    TRY_STRTOF("1.5E+1",        15.000000f,     -1, 0);
    TRY_STRTOF("2.5E-1",        0.25000000f,    -1, 0);

    /* Check that infinite values are properly parsed. */

    TRY_STRTOF("inf",           INFINITY,       -1, 0);
    TRY_STRTOF("Inf",           INFINITY,       -1, 0);
    TRY_STRTOF("INF",           INFINITY,       -1, 0);
    TRY_STRTOF("+inf",          INFINITY,       -1, 0);
    TRY_STRTOF("+Inf",          INFINITY,       -1, 0);
    TRY_STRTOF("+INF",          INFINITY,       -1, 0);
    TRY_STRTOF("-inf",         -INFINITY,       -1, 0);
    TRY_STRTOF("-Inf",         -INFINITY,       -1, 0);
    TRY_STRTOF("-INF",         -INFINITY,       -1, 0);
    TRY_STRTOF("inf5",          INFINITY,        3, 0);
    TRY_STRTOF("inf.5",         INFINITY,        3, 0);

    /* Check that out-of-range values properly generate ERANGE. */

    TRY_STRTOF("10000000000000000000000000000000000000000", HUGE_VALF, -1, 1);
    TRY_STRTOF("0.0000000000000000000000000000000000000001", 0, -1, 1);
    TRY_STRTOF("1e40",          HUGE_VALF,      -1, 1);
    TRY_STRTOF("1e-40",         0,              -1, 1);
    TRY_STRTOF("1e4000",        HUGE_VALF,      -1, 1);
    TRY_STRTOF("1e-4000",       0,              -1, 1);
    TRY_STRTOF("10000000e33",   HUGE_VALF,      -1, 1);
    TRY_STRTOF("0.0000001e-33", 0,              -1, 1);

    /* Check that out-of-range exponents with mantissas that pull them
     * back in range do _not_ generate ERANGE. */

    TRY_STRTOF("0.0000001e40",  1.0000000e33,   -1, 0);
    TRY_STRTOF("10000000e-40",  1.0000000e-33,  -1, 0);

    /* Check that the edges of the valid range are handled correctly.
     * For underflow, we assume that denormals are flushed to zero. */

    TRY_STRTOF("3.4028235e+38", 3.4028235e+38,  -1, 0);
    TRY_STRTOF("3.4028238e+38", HUGE_VALF,      -1, 1);
    TRY_STRTOF("1.1754944e-38", 1.1754944e-38,  -1, 0);
    TRY_STRTOF("1.1754941e-38", 0,              -1, 1);

    /* Check that leading spaces are skipped. */

    TRY_STRTOF(" 1.5",          1.5000000f,     -1, 0);
    TRY_STRTOF("   1.5",        1.5000000f,     -1, 0);

    /* Check that other leading characters cause an abort. */

    TRY_STRTOF("_1.5",          0.0f,            0, 0);
    TRY_STRTOF("\t1.5",         0.0f,            0, 0);
    TRY_STRTOF("\n1.5",         0.0f,            0, 0);

    /* Check that trailing spaces are _not_ skipped. */

    TRY_STRTOF("1.5 ",          1.5000000f,      3, 0);

    /* Check that a second period or other trailing junk doesn't confuse
     * the function. */

    TRY_STRTOF("1.5.2",         1.5000000f,      3, 0);
    TRY_STRTOF("1.5e1.2",       15.000000f,      5, 0);
    TRY_STRTOF("1.5z",          1.5000000f,      3, 0);
    TRY_STRTOF("1.5e1z",        15.000000f,      5, 0);

    /* Check that not-quite-"inf" values are properly rejected. */

    TRY_STRTOF("ing",           0,               0, 0);
    TRY_STRTOF("Ing",           0,               0, 0);
    TRY_STRTOF("INg",           0,               0, 0);
    TRY_STRTOF("io",            0,               0, 0);
    TRY_STRTOF("Io",            0,               0, 0);

    /* Check that an invalid character after "e" doesn't consume the "e". */

    TRY_STRTOF("1.5ez",         1.5000000f,      3, 0);
    TRY_STRTOF("1.5E!",         1.5000000f,      3, 0);
    TRY_STRTOF("1.5e+z",        1.5000000f,      3, 0);
    TRY_STRTOF("1.5E+!",        1.5000000f,      3, 0);
    TRY_STRTOF("1.5e-z",        1.5000000f,      3, 0);
    TRY_STRTOF("1.5E-!",        1.5000000f,      3, 0);

    /* Make sure we return EFAULT instead of crashing on NULL input.
     * (Note that C99 7.1.4.1 specifies undefined behavior for this case,
     * but we consistently return EFAULT to avoid crashes.) */

    errno = 0;
    if (strtof(NULL, NULL) != 0) {
        FAIL("strtof(NULL, NULL) did not return 0");
    } else if (errno != EFAULT) {
        FAIL("strtof(NULL, NULL) did not set errno = EFAULT");
    }
    const char *dummy = "";
    errno = 0;
    if (strtof(NULL, (char **)&dummy) != 0) {
        FAIL("strtof(NULL, &dummy) did not return 0");
    } else if (errno != EFAULT) {
        FAIL("strtof(NULL, &dummy) did not set errno = EFAULT");
    } else if (dummy != NULL) {
        FAIL("strtof(NULL, &dummy) did not set dummy = NULL");
    }

    /* Make sure we don't break if endptr == NULL. */

    if (strtof("1.5", NULL) != 1.5f) {
        FAIL("strtof(\"1.5\", NULL) did not return 1.5 as expected");
    }
    if (strtof("inf", NULL) != INFINITY) {
        FAIL("strtof(\"inf\", NULL) did not return INFINITY as expected");
    }

    /* All done. */

    return !failed;
}

/*************************************************************************/
/*************************************************************************/
