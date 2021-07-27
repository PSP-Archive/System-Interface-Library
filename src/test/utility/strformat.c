/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/utility/strformat.c: Tests for strformat().
 */

#define FAIL_ACTION  failed = 1  // Continue through failures.

#include "src/base.h"
#include "src/memory.h"
#include "src/test/base.h"
#include "src/utility/strformat.h"

#if defined(SIL_PLATFORM_MACOSX)
/* [U]INTMAX_C() expanded to the wrong type through version 10.8 of the
 * OS X SDK, so disable format checks entirely when building with those
 * versions to avoid spurious warnings.  Reported to Apple as bug 11540697
 * and fixed in SDK 10.9. */
# ifndef MAC_OS_X_VERSION_10_9
#  define strformat strformat_invalid
# endif
#endif

/*************************************************************************/
/************************ Helper macros/routines *************************/
/*************************************************************************/

/**
 * strformat_invalid:  Call strformat() with a format string known to be
 * invalid.  Implemented by taking the function's varargs and passing them
 * to vstrformat() without attaching a FORMAT() tag to the function.
 */
static int strformat_invalid(char *buf, int size, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    const int res = vstrformat(buf, size, format, args);
    va_end(args);
    return res;
}

/*-----------------------------------------------------------------------*/

/**
 * _check_strformat:  Check that a call to strformat() has done what it
 * was supposed to, returning NULL if the test passed or an error string
 * if not.  Helper for the _TRY_STRFORMAT() macro.
 *
 * Note that the strformat() call itself must be part of the macro for
 * format string checking to work, and the FAIL() calls must be part of
 * the macro to return proper source line information.
 */
static const char *_check_strformat(
    int expect_retval, const char *expect, int retval,
    const char *buf, size_t bufsize)
{
    static char error_buf[1000];
    if (strcmp(expect, buf) != 0) {
        strformat(error_buf, sizeof(error_buf),
                  "bad result string ([%s], expected [%s])", buf, expect);
        return error_buf;
    }
    if (expect_retval != retval) {
        strformat(error_buf, sizeof(error_buf),
                  "bad return value (%d, expected %d)", retval, expect_retval);
        return error_buf;
    }
    for (const char *s = buf + strlen(buf) + 1; s < buf + bufsize; s++) {
        if (*(uint8_t *)s != 0xBE) {
            strformat(error_buf, sizeof(error_buf),
                      "memory corruption at relative offset %td", s - buf);
            return error_buf;
        }
    }
    return NULL;
}

/*-----------------------------------------------------------------------*/

/**
 * TRY_STRFORMAT:  Call strformat() and check the result against the
 * expected output string and return value.  The result is assumed to
 * fit in the test_buf[] array, and strlen(expect) is taken as the
 * expected return value.
 *
 * This is declared as a macro rather than a function so that DLOG()
 * prints the line of the test itself rather than the line of this test
 * code when an error is detected.
 *
 * [Parameters]
 *     expect: Expected result string (must be a literal string constant).
 */
#define TRY_STRFORMAT(expect,...) \
    TRY_STRFORMAT_EX(strlen((expect)), (expect), sizeof(test_buf), __VA_ARGS__)

/**
 * TRY_STRFORMAT_EX:  Call strformat() and check the result against the
 * expected output string and return value.  "bufsize" is passed to
 * strformat() as the output buffer size.
 *
 * [Parameters]
 *     expect_retval: Expected return value from strformat().
 *     expect: Expected result string (must be a literal string constant).
 *     bufsize: Result buffer size to pass to strformat().
 */
#define TRY_STRFORMAT_EX(expect_retval,expect,bufsize,...) \
    _TRY_STRFORMAT(strformat, (expect_retval), (expect), (bufsize), \
                   __VA_ARGS__)

/**
 * TRY_STRFORMAT_INVALID, TRY_STRFORMAT_INVALID_EX:  Call strformat() and
 * check the result against the expected output string and return value.
 * Identical to TRY_STRFORMAT{,_EX}, except that the format string is
 * assumed to be invalid and is not checked for validity.
 */
#define TRY_STRFORMAT_INVALID(expect,...) \
    TRY_STRFORMAT_INVALID_EX(strlen((expect)), (expect), sizeof(test_buf), \
                             __VA_ARGS__)
#define TRY_STRFORMAT_INVALID_EX(expect_retval,expect,bufsize,...) \
    _TRY_STRFORMAT(strformat_invalid, (expect_retval), (expect), (bufsize), \
                   __VA_ARGS__)

/**
 * _TRY_STRFORMAT:  Implementation of the various TRY_STRFORMAT macros.
 */
#define _TRY_STRFORMAT(function,expect_retval,expect,bufsize,...)  do { \
    const char *_expect = (expect);                                     \
    const int _expect_retval = (expect_retval);                         \
    const int _bufsize = (bufsize);                                     \
    /* Fill with a dummy value to detect memory corruption. */          \
    memset(test_buf, 0xBE, sizeof(test_buf));                           \
    const int _retval = function(test_buf, _bufsize, __VA_ARGS__);      \
    const char *_error = _check_strformat(                              \
        _expect_retval, _expect, _retval, test_buf, sizeof(test_buf));  \
    if (_error) {                                                       \
        FAIL("%s", _error);                                             \
    }                                                                   \
} while (0)

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

static int failed;
static char test_buf[1000];

DEFINE_GENERIC_TEST_RUNNER(test_utility_strformat)

/*-----------------------------------------------------------------------*/

TEST_INIT(init)
{
    failed = 0;
    strformat_enable_fullwidth(1);
    strformat_set_fullwidth(1);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
    strformat_enable_fullwidth(0);
    strformat_set_fullwidth(0);
    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_no_tokens)
{
    const char *emptystr = "";  // Avoid a compiler warning.
    TRY_STRFORMAT("", emptystr, ""); // 2nd empty string is to avoid a warning.
    TRY_STRFORMAT("abcde", "abcde");
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_token_percent)
{
    TRY_STRFORMAT("%", "%%");
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_token_c)
{
    TRY_STRFORMAT("0", "%c", 0x30);
    TRY_STRFORMAT("~", "%c", 0x7E);
    TRY_STRFORMAT("¡", "%c", 0xA1);
    TRY_STRFORMAT("\xDF\xBF", "%c", 0x7FF);
    TRY_STRFORMAT("グ", "%c", 0x30B0);
    TRY_STRFORMAT("０", "%c", 0xFF10);
    TRY_STRFORMAT("\xF0\x90\x80\x80", "%c", 0x10000);
    TRY_STRFORMAT("\xF7\xB0\x80\x80", "%c", 0x1F0000);
    TRY_STRFORMAT("\xF8\x88\x80\x80\x80", "%c", 0x200000);
    TRY_STRFORMAT("\xFB\xB0\x80\x80\x80", "%c", 0x3C00000);
    TRY_STRFORMAT("\xFC\x84\x80\x80\x80\x80", "%c", 0x4000000);
    TRY_STRFORMAT("\xFD\xBF\xBF\xBF\xBF\xBF", "%c", 0x7FFFFFFF);
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_token_d)
{
    TRY_STRFORMAT("123", "%d", 123);
    TRY_STRFORMAT("-123", "%d", -123);
    TRY_STRFORMAT("  123", "%5d", 123);
    TRY_STRFORMAT(" -123", "%5d", -123);
    TRY_STRFORMAT("00123", "%05d", 123);
    TRY_STRFORMAT("-0123", "%05d", -123);
    TRY_STRFORMAT("123  ", "%-5d", 123);
    TRY_STRFORMAT(" 0123", "% 05d", 123);
    TRY_STRFORMAT("-0123", "% 05d", -123);
    TRY_STRFORMAT(" 012345678901", "% 013lld", 12345678901LL);
    TRY_STRFORMAT("-012345678901", "% 013lld", -12345678901LL);
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_dynamic_field_width)
{
    TRY_STRFORMAT("  123", "%*d", 5, 123);
    TRY_STRFORMAT("123  ", "%*d", -5, 123);
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_token_plus_d)
{
    TRY_STRFORMAT("１２３", "%+d", 123);
    TRY_STRFORMAT("－１２３", "%+d", -123);
    TRY_STRFORMAT("　　１２３", "%+5d", 123);
    TRY_STRFORMAT("　－１２３", "%+5d", -123);
    TRY_STRFORMAT("００１２３", "%+05d", 123);
    TRY_STRFORMAT("－０１２３", "%+05d", -123);
    TRY_STRFORMAT("１２３４５６７８９０１", "%+lld", 12345678901LL);
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_token_e)
{
    TRY_STRFORMAT("1.234560e+00", "%e",    1.23456);
    TRY_STRFORMAT("1.234560e+00", "%Le",   1.23456L);
    TRY_STRFORMAT(" 1.23456e+00", "%12.5e", 1.23456);
    TRY_STRFORMAT("  1.2346e+00", "%12.4e", 1.23456);
    TRY_STRFORMAT("   1.235e+00", "%12.3e", 1.23456);
    TRY_STRFORMAT("    1.23e+00", "%12.2e", 1.23456);
    TRY_STRFORMAT("       1e+00", "%12.0e", 1.23456);
    TRY_STRFORMAT("-1.23456e+00", "%.5e", -1.23456);
    TRY_STRFORMAT("00001.23e+00", "%012.2e", 1.23456);
    TRY_STRFORMAT(" 0001.23e+00", "% 012.2e", 1.23456);
    TRY_STRFORMAT("   -1.23e+00", "%12.2e", -1.23456);
    TRY_STRFORMAT("-0001.23e+00", "%012.2e", -1.23456);
    TRY_STRFORMAT("1.2346e+01", "%.4e", 12.3456);
    TRY_STRFORMAT("1.2346e-03", "%.4e", 0.00123456);
    TRY_STRFORMAT("1.2346e+10", "%.4e", 1.23456e+10);
    TRY_STRFORMAT("1.2346e-30", "%.4e", 1.23456e-30);
#ifndef SIL_STRFORMAT_USE_FLOATS
    TRY_STRFORMAT("1.2346e+123", "%.4e", 1.23456e+123);
    TRY_STRFORMAT("1.2346e-123", "%.4e", 1.23456e-123);
#endif
    TRY_STRFORMAT("0.0000e+00", "%.4e", 0.0);
    TRY_STRFORMAT("  inf", "%5e", 1.0/0.0);
    TRY_STRFORMAT(" -inf", "%5e", -1.0/0.0);
    TRY_STRFORMAT("  nan", "%5e", DOUBLE_NAN());
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_exp_buffer_overflow)
{
    TRY_STRFORMAT("1.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e+00",
                  "%.200e", 1.0);
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_dynamic_field_precision)
{
    TRY_STRFORMAT("1.235e+00", "%.*e", 3, 1.23456);
    TRY_STRFORMAT("1e+00", "%.*e", -3, 1.23456);
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_token_plus_e)
{
    TRY_STRFORMAT("１．２３４５６０ｅ＋００", "%+e", 1.23456);
    TRY_STRFORMAT("－１．２３４５６０ｅ＋０１", "%+e", -12.3456);
    TRY_STRFORMAT("　　　１．２３５ｅ－１２", "%+12.3e", 1.23456e-12);
    TRY_STRFORMAT("０００１．２３５ｅ＋００", "%+012.3e", 1.23456);
    TRY_STRFORMAT("－００１．２３５ｅ＋００", "%+012.3e", -1.23456);
    TRY_STRFORMAT_INVALID("　００１．２３５ｅ＋００", "%+ 012.3e", 1.23456);
    TRY_STRFORMAT("　　ｉｎｆ", "%+5e", 1.0/0.0);
    TRY_STRFORMAT("　－ｉｎｆ", "%+5e", -1.0/0.0);
    TRY_STRFORMAT("　　ｎａｎ", "%+5e", DOUBLE_NAN());
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_token_E)
{
    TRY_STRFORMAT("1.2346E+01", "%.4E", 12.3456);
    TRY_STRFORMAT("  INF", "%5E", 1.0/0.0);
    TRY_STRFORMAT(" -INF", "%5E", -1.0/0.0);
    TRY_STRFORMAT("  NAN", "%5E", DOUBLE_NAN());
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_token_plus_E)
{
    TRY_STRFORMAT("　　　１．２３５Ｅ＋０１", "%+12.3E", 12.3456);
    TRY_STRFORMAT("　　ＩＮＦ", "%+5E", 1.0/0.0);
    TRY_STRFORMAT("　－ＩＮＦ", "%+5E", -1.0/0.0);
    TRY_STRFORMAT("　　ＮＡＮ", "%+5E", DOUBLE_NAN());
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_token_f)
{
    TRY_STRFORMAT("1.234560", "%f",    1.23456);
    TRY_STRFORMAT(" 1.23456", "%8.5f", 1.23456);
    TRY_STRFORMAT("  1.2346", "%8.4f", 1.23456);
    TRY_STRFORMAT("   1.235", "%8.3f", 1.23456);
    TRY_STRFORMAT("    1.23", "%8.2f", 1.23456);
    TRY_STRFORMAT("       1", "%8.0f", 1.23456);
    TRY_STRFORMAT("-1.23456", "%.5f", -1.23456);
    TRY_STRFORMAT("00001.23", "%08.2f", 1.23456);
    TRY_STRFORMAT("   -1.23", "%8.2f", -1.23456);
    TRY_STRFORMAT("-0001.23", "%08.2f", -1.23456);
    TRY_STRFORMAT("12.3456", "%.4f", 12.3456);
    TRY_STRFORMAT("0.0012", "%.4f", 0.00123456);
    TRY_STRFORMAT("  inf", "%5f", 1.0/0.0);
    TRY_STRFORMAT(" -inf", "%5f", -1.0/0.0);
    TRY_STRFORMAT("  nan", "%5f", DOUBLE_NAN());
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_token_plus_f)
{
    TRY_STRFORMAT("１．２３４５６０", "%+f", 1.23456);
    TRY_STRFORMAT("－１．２３４５６０", "%+f", -1.23456);
    TRY_STRFORMAT("　　　１．２３５", "%+8.3f", 1.23456);
    TRY_STRFORMAT("０００１．２３５", "%+08.3f", 1.23456);
    TRY_STRFORMAT("　　ｉｎｆ", "%+5f", 1.0/0.0);
    TRY_STRFORMAT("　－ｉｎｆ", "%+5f", -1.0/0.0);
    TRY_STRFORMAT("　　ｎａｎ", "%+5f", DOUBLE_NAN());
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_token_F)
{
    TRY_STRFORMAT("12.3456", "%.4F", 12.3456);
    TRY_STRFORMAT("  INF", "%5F", 1.0/0.0);
    TRY_STRFORMAT(" -INF", "%5F", -1.0/0.0);
    TRY_STRFORMAT("  NAN", "%5F", DOUBLE_NAN());
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_token_plus_F)
{
    TRY_STRFORMAT("　　　１．２３５", "%+8.3F", 1.23456);
    TRY_STRFORMAT("　　ＩＮＦ", "%+5F", 1.0/0.0);
    TRY_STRFORMAT("　－ＩＮＦ", "%+5F", -1.0/0.0);
    TRY_STRFORMAT("　　ＮＡＮ", "%+5F", DOUBLE_NAN());
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_token_g)
{
    TRY_STRFORMAT("1.23456", "%g", 1.23456);
    TRY_STRFORMAT("12.35", "%.4g", 12.3456);
    TRY_STRFORMAT("0.0001235", "%.4g", 0.000123456);
    TRY_STRFORMAT("1.23005", "%g", 1.2300456);
    TRY_STRFORMAT("1.23", "%.4g", 1.2300456);
    TRY_STRFORMAT("1.235e-05", "%.4g", 0.0000123456);
    TRY_STRFORMAT("1.235e+04", "%.4g", 12345.6);
    TRY_STRFORMAT("1.235e+10", "%.4g", 12345678910.0);
    TRY_STRFORMAT("1.24e+12", "%.4g", 1240356789012.0);
    TRY_STRFORMAT("50", "%g", 50.0);
    TRY_STRFORMAT(" 50", "%3g", 50.0);
    TRY_STRFORMAT("0", "%g", 0.0);
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_token_plus_g)
{
    TRY_STRFORMAT("１．２３４５６", "%+g", 1.23456);
    TRY_STRFORMAT("１．２３５", "%+.4g", 1.23456);
    TRY_STRFORMAT("１", "%+g", 1.0);
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_token_G)
{
    TRY_STRFORMAT("1.235E-05", "%.4G", 0.0000123456);
    TRY_STRFORMAT("  INF", "%5G", 1.0/0.0);
    TRY_STRFORMAT(" -INF", "%5G", -1.0/0.0);
    TRY_STRFORMAT("  NAN", "%5G", DOUBLE_NAN());
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_token_plus_G)
{
    TRY_STRFORMAT("　　　１．２３５Ｅ－０５", "%+12.4G", 0.0000123456);
    TRY_STRFORMAT("　　ＩＮＦ", "%+5G", 1.0/0.0);
    TRY_STRFORMAT("　－ＩＮＦ", "%+5G", -1.0/0.0);
    TRY_STRFORMAT("　　ＮＡＮ", "%+5G", DOUBLE_NAN());
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_token_i)
{
    TRY_STRFORMAT("123", "%i", 123);
    TRY_STRFORMAT("00123", "%05i", 123);
    TRY_STRFORMAT("-0123", "%05i", -123);
    TRY_STRFORMAT(" 0123", "% 05i", 123);
    TRY_STRFORMAT("１２３", "%+i", 123);
    TRY_STRFORMAT("００１２３", "%+05i", 123);
    /* This is a valid string, but it triggers a spurious compiler warning. */
    TRY_STRFORMAT_INVALID("　０１２３", "%+ 05i", 123);
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_token_o)
{
    TRY_STRFORMAT("173", "%o", 123);
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_token_p)
{
    TRY_STRFORMAT("0x12345678", "%p", (void *)0x12345678);
    TRY_STRFORMAT("(null)", "%p", NULL);
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_token_s)
{
    const char * volatile nullstr = NULL;  // Avoid a compiler warning.
    TRY_STRFORMAT("test", "%s", "test");
    TRY_STRFORMAT("(null)", "%s", nullstr);
    TRY_STRFORMAT("te", "%.2s", "test");
    TRY_STRFORMAT("test", "%.6s", "test\0X");  // Check for read overrun.
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_token_u)
{
    /* Use two tokens to make sure int-sized arguments are read correctly,
     * particularly on 64-bit systems. */
    TRY_STRFORMAT("123 1234", "%u %u", 123, 1234);
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_token_x)
{
    TRY_STRFORMAT("7b", "%x", 123);
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_token_X)
{
    TRY_STRFORMAT("7B", "%X", 123);
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_operand_size)
{
    /* strformat() assumes that anything smaller than an int is passed in
     * an int.  This seems to be the case on all current systems, but check
     * anyway just to be safe. */
    TRY_STRFORMAT("-7616 -46", "%hd %hhd", (short)-7616, (signed char)-46);
    TRY_STRFORMAT("57920 210", "%hu %hhu", (short)-7616, (signed char)-46);
    TRY_STRFORMAT("E240 D2", "%hX %hhX", (short)-7616, (signed char)-46);
    TRY_STRFORMAT_INVALID("-7616 -46", "%hd %hhd", 123456, 1234);
    TRY_STRFORMAT_INVALID("57920 210", "%hu %hhu", 123456, 1234);
    TRY_STRFORMAT_INVALID("E240 D2", "%hX %hhX", 123456, 1234);

    TRY_STRFORMAT("123456 1234", "%ld %d", 123456L, 1234);
    TRY_STRFORMAT("-123456 1234", "%ld %d", -123456L, 1234);
    TRY_STRFORMAT("12345678901 1234", "%lld %d", 12345678901LL, 1234);
    TRY_STRFORMAT("-12345678901 1234", "%lld %d", -12345678901LL, 1234);
    TRY_STRFORMAT("12345 1234", "%zd %d", (size_t)12345, 1234);
    TRY_STRFORMAT("-12345 1234", "%zd %d", (size_t)-12345, 1234);
    TRY_STRFORMAT("12345 1234", "%td %d", (ptrdiff_t)12345, 1234);
    TRY_STRFORMAT("-12345 1234", "%td %d", (ptrdiff_t)-12345, 1234);
    TRY_STRFORMAT("12345678901 1234", "%jd %d", INTMAX_C(12345678901), 1234);
    TRY_STRFORMAT("-12345678901 1234", "%jd %d", INTMAX_C(-12345678901), 1234);

    TRY_STRFORMAT("123456 1234", "%lu %u", 123456UL, 1234);
    TRY_STRFORMAT("12345678901 1234", "%llu %u", 12345678901ULL, 1234);
    TRY_STRFORMAT("12345 1234", "%zu %u", (size_t)12345, 1234);
    TRY_STRFORMAT("12345 1234", "%tu %u", (ptrdiff_t)12345, 1234);
    TRY_STRFORMAT("12345678901 1234", "%ju %d", UINTMAX_C(12345678901), 1234);

    TRY_STRFORMAT("1E240 1A2B", "%lX %X", 123456UL, 0x1A2B);
    TRY_STRFORMAT("FEDCBA9876543210 1A2B", "%llX %X",
                  0xFEDCBA9876543210ULL, 0x1A2B);
    TRY_STRFORMAT("3039 1A2B", "%zX %X", (size_t)12345, 0x1A2B);
    TRY_STRFORMAT("3039 1A2B", "%tX %X", (ptrdiff_t)12345, 0x1A2B);
    TRY_STRFORMAT("FEDCBA9876543210 1A2B", "%jX %X",
                  UINTMAX_C(0xFEDCBA9876543210), 0x1A2B);

    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_float_rounding)
{
    TRY_STRFORMAT("1.00e+03", "%.2e", 999.5);
    /* 0.[...]95 can't be represented exactly in floating point, so there's
     * no "exactly .5" edge case we need to test.  Use ...96 to avoid the
     * value being encoded as something slightly less than ...95 and
     * getting rounded the wrong way. */
    TRY_STRFORMAT("1.00e-03", "%.2e", 0.0009996);
    TRY_STRFORMAT("0.001000", "%.6f", 0.0009996);
    TRY_STRFORMAT("1e+03", "%.3g", 999.5);
    TRY_STRFORMAT("0.0001", "%.3g", 0.00009996);
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_fullwidth_off)
{
    strformat_set_fullwidth(0);
    TRY_STRFORMAT(" -123", "%+5d", -123);
    TRY_STRFORMAT("   1.235", "%+8.3f", 1.23456);
    TRY_STRFORMAT("0001.235", "%+08.3f", 1.23456);
    TRY_STRFORMAT(" -inf", "%+5f", -1.0/0.0);
    TRY_STRFORMAT("  nan", "%+5f", DOUBLE_NAN());
    TRY_STRFORMAT("1.235", "%+.4g", 1.23456);
    TRY_STRFORMAT("1.0000000000000000000000000000000000000000",
        "%+.40f", 1.0);  // Should _not_ be truncated (unlike for fullwidth).
    TRY_STRFORMAT("00123", "%+05i", 123);
    strformat_set_fullwidth(1);
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_fullwidth_disabled)
{
    strformat_enable_fullwidth(0);
    TRY_STRFORMAT("+123", "%+d", 123);
    TRY_STRFORMAT(" +123", "%+5d", 123);
    TRY_STRFORMAT("+0123", "%+05d", 123);
    TRY_STRFORMAT("+12345678901", "%+lld", 12345678901LL);
    TRY_STRFORMAT(" +12345678901", "%+13lld", 12345678901LL);
    TRY_STRFORMAT("+012345678901", "%+013lld", 12345678901LL);
    TRY_STRFORMAT("+1.235", "%+.3f", 1.23456);
    TRY_STRFORMAT(" +1.235", "%+7.3f", 1.23456);
    TRY_STRFORMAT("+01.235", "%+07.3f", 1.23456);
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_token_f_extra_precision)
{
    /* %f with more precision than the actual value has.  Note that the
     * value will be truncated to 126 decimal places, but this call checks
     * that we don't crash due to a floating-point overflow exception. */
    TRY_STRFORMAT("1.00000000000000000000000000000000000000000000000000"
                  "0000000000000000000000000000000000000000000000000000"
                  "000000000000000000000000", "%.312f", 1.0);
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_overflow)
{
    TRY_STRFORMAT_EX(5, "ab", 3, "abcde");
    TRY_STRFORMAT_INVALID_EX(3, "ab", 3, "ab%");
    TRY_STRFORMAT_INVALID_EX(2, "", 1, "%2d", 1);
    TRY_STRFORMAT_INVALID_EX(2, "", 1, "%-2d", 1);
    TRY_STRFORMAT_INVALID_EX(2, "", 1, "%02d", 1);
    TRY_STRFORMAT_INVALID_EX(3, "", 1, "%3d", -1);
    TRY_STRFORMAT_INVALID_EX(3, "", 1, "%-3d", -1);
    TRY_STRFORMAT_INVALID_EX(3, "", 1, "%03d", -1);
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_utf8_truncation)
{
    TRY_STRFORMAT_EX(2, "", 2, "\xC2\x80");
    TRY_STRFORMAT_EX(3, "", 3, "\xE0\xA0\x80");
    TRY_STRFORMAT_EX(4, "", 4, "\xF0\x90\x80\x80");
    TRY_STRFORMAT_EX(5, "", 5, "\xF8\x88\x80\x80\x80");
    TRY_STRFORMAT_EX(6, "", 6, "\xFC\x84\x80\x80\x80\x80");
    TRY_STRFORMAT_EX(2, "", 2, "%c", 0x80);
    TRY_STRFORMAT_EX(3, "", 3, "%c", 0x800);
    TRY_STRFORMAT_EX(4, "", 4, "%c", 0x10000);
    TRY_STRFORMAT_EX(5, "", 5, "%c", 0x200000);
    TRY_STRFORMAT_EX(6, "", 6, "%c", 0x4000000);
    TRY_STRFORMAT_EX(2, "", 2, "%s", "\xC2\x80");
    TRY_STRFORMAT_EX(3, "", 3, "%s", "\xE0\xA0\x80");
    TRY_STRFORMAT_EX(4, "", 4, "%s", "\xF0\x90\x80\x80");
    TRY_STRFORMAT_EX(5, "", 5, "%s", "\xF8\x88\x80\x80\x80");
    TRY_STRFORMAT_EX(6, "", 6, "%s", "\xFC\x84\x80\x80\x80\x80");
    TRY_STRFORMAT_EX(3, "", 3, "%+d", 0);
    TRY_STRFORMAT_EX(3, "", 3, "%+.0f", 0.0);
    TRY_STRFORMAT_EX(3, "", 3, "%+.0g", 0.0);
    TRY_STRFORMAT_EX(3, "", 3, "%+i", 0);
    TRY_STRFORMAT_EX(6, "０", 6, "%+02d", 1);
    TRY_STRFORMAT_EX(6, "０", 6, "%+02.0f", 1.0);
    TRY_STRFORMAT_EX(6, "０", 6, "%+02.0g", 1.0);
    TRY_STRFORMAT_EX(6, "０", 6, "%+02i", 1);
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_utf8_truncation_and_single_byte_char)
{
    /* Make sure a single-byte character after a multibyte character
     * doesn't get inserted into the buffer if the multibyte character
     * doesn't fit. */
    TRY_STRFORMAT_EX(4, "", 3, "０1");
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_invalid_parameters)
{
    CHECK_INTEQUAL(strformat_invalid(test_buf, sizeof(test_buf), NULL), 0);
    CHECK_INTEQUAL(strformat_invalid(test_buf, 0xFFFFFFFFU, "1"), 0);
    CHECK_INTEQUAL(strformat_invalid(NULL, 0, "1"), 1);
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_invalid_utf8)
{
    TRY_STRFORMAT("", "\x80");
    TRY_STRFORMAT("", "\xEF\xBC");
    TRY_STRFORMAT("", "%s", "\x80");
    TRY_STRFORMAT("", "%.2s", "０");  // 2 bytes of a 3-byte UTF-8 character.
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_ignored_modifiers)
{
    TRY_STRFORMAT_INVALID("1", "%#x", 1);  // "#" ignored.
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_misplaced_modifiers)
{
    TRY_STRFORMAT_INVALID("1 ", "%-02d", 1);
    TRY_STRFORMAT_INVALID("%2#d", "%2#d", 1);
    TRY_STRFORMAT_INVALID("%2 d", "%2 d", 1);
    TRY_STRFORMAT_INVALID("%2+d", "%2+d", 1);
    TRY_STRFORMAT_INVALID("%2-d", "%2-d", 1);
    TRY_STRFORMAT_INVALID("%.3#d", "%.3#d", 1);
    TRY_STRFORMAT_INVALID("%.3 d", "%.3 d", 1);
    TRY_STRFORMAT_INVALID("%.3+d", "%.3+d", 1);
    TRY_STRFORMAT_INVALID("%.3-d", "%.3-d", 1);
    TRY_STRFORMAT_INVALID("%l#d", "%l#d", 1);
    TRY_STRFORMAT_INVALID("%l d", "%l d", 1);
    TRY_STRFORMAT_INVALID("%l+d", "%l+d", 1);
    TRY_STRFORMAT_INVALID("%l-d", "%l-d", 1);
    TRY_STRFORMAT_INVALID("%l0d", "%l0d", 1);
    TRY_STRFORMAT_INVALID("%l1d", "%l1d", 1);
    TRY_STRFORMAT_INVALID("%l*d", "%l*d", 1, 1);
    TRY_STRFORMAT_INVALID("%l.1d", "%l.1d", 1);
    TRY_STRFORMAT_INVALID("%l.*d", "%l.*d", 1, 1);
    TRY_STRFORMAT_INVALID("%2#d", "%2#d", 1);
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_invalid_tokens)
{
    TRY_STRFORMAT_INVALID("%1*d", "%1*d", 1);
    TRY_STRFORMAT_INVALID("%1.1*d", "%1.1*d", 1);
    TRY_STRFORMAT_INVALID("%1.1.1d", "%1.1.1d", 1);
    TRY_STRFORMAT_INVALID("%lhd", "%lhd", 1);
    TRY_STRFORMAT_INVALID("%llld", "%llld", 1);
    TRY_STRFORMAT_INVALID("%lLd", "%lLd", 1);
    TRY_STRFORMAT_INVALID("%ljd", "%ljd", 1);
    TRY_STRFORMAT_INVALID("%ltd", "%ltd", 1);
    TRY_STRFORMAT_INVALID("%lzd", "%lzd", 1);
    TRY_STRFORMAT_INVALID("%?", "%?");
    TRY_STRFORMAT_INVALID("%", "%");
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_posix_arg_index)
{
    /* POSIX-style explicit argument indexing is not supported and
     * documented as such; check that the behavior is as documented. */
    TRY_STRFORMAT_INVALID("%2$d %1$d", "%2$d %1$d", 1, 2);
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_field_width_limit)
{
    /* Field width and precision should be silently truncated to 10000. */
    CHECK_INTEQUAL(strformat(NULL, 0, "%99999d", 1), 10000);
    CHECK_INTEQUAL(strformat(NULL, 0, "%*d", 32767, 1), 10000);
    char *hugestr;
    ASSERT(hugestr = mem_alloc(100001, 0, MEM_ALLOC_TEMP));
    memset(hugestr, 'a', 100000);
    hugestr[100000] = 0;
    CHECK_INTEQUAL(strformat(NULL, 0, "%.99999s", hugestr), 10000);
    CHECK_INTEQUAL(strformat(NULL, 0, "%.*s", 32767, hugestr), 10000);
    mem_free(hugestr);
    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_floating_point_truncation)
{
    TRY_STRFORMAT(
        "1.00000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000",
        "%.150f", 1.0);  // Truncated to 128 characters.
    TRY_STRFORMAT(
        "１．００００００００００００００００００００００００００００００"
        "００００００００００",
        "%+.50f", 1.0);  // Truncated to int(128/3) = 42 characters.
    return !failed;
}

/*-----------------------------------------------------------------------*/

#ifndef SIL_STRFORMAT_USE_FLOATS

TEST(test_floating_point_truncation_2)
{
    /* Should print the lower 127 digits of 1e130. */
    strformat(test_buf, sizeof(test_buf), "%.0f", 1e130);
    CHECK_TRUE(strncmp(test_buf, "00000000000", 11) == 0);
    CHECK_INTRANGE(test_buf[126], '0', '9');
    CHECK_INTEQUAL(test_buf[127], '\0');
    /* Should print the lower 127 digits of 1e130 followed by ".". */
    strformat(test_buf, sizeof(test_buf), "%.1f", 1e130);
    CHECK_TRUE(strncmp(test_buf, "00000000000", 11) == 0);
    CHECK_INTRANGE(test_buf[126], '0', '9');
    CHECK_STREQUAL(test_buf+127, ".");
    /* Should print the entire 127 digits of 1e126 followed by ".". */
    strformat(test_buf, sizeof(test_buf), "%.1f", 1e126);
    CHECK_TRUE(strncmp(test_buf, "100000000000000", 15) == 0);
    CHECK_INTRANGE(test_buf[126], '0', '9');
    CHECK_STREQUAL(test_buf+127, ".");
    /* Should print the entire 126 digits of 1e125 followed by ".0". */
    strformat(test_buf, sizeof(test_buf), "%.1f", 1e125);
    CHECK_TRUE(strncmp(test_buf, "100000000000000", 15) == 0);
    CHECK_INTRANGE(test_buf[125], '0', '9');
    CHECK_STREQUAL(test_buf+126, ".0");
    return !failed;
}

#endif  // !SIL_STRFORMAT_USE_FLOATS

/*-----------------------------------------------------------------------*/

TEST(test_many_format_arguments)
{
    /* Some ABIs put the first few function parameters in registers, which
     * can mask errors in format argument size handling, so we add checks
     * for values far enough down the parameter list that they will be on
     * the stack.  (For example, this covers the second branch of va_arg()
     * on GCC-compiled x86-64 code.) */

    TRY_STRFORMAT("0", "%s%s%s%s%s%c", "", "", "", "", "", 0x30);
    TRY_STRFORMAT("123", "%s%s%s%s%s%d", "", "", "", "", "", 123);
    /* x86-64 needs a whole bunch of args to get FP values onto the stack. */
    TRY_STRFORMAT("0000000000000000000001.234560",
                  "%g%g%g%g%g%g%g%g%g%g%g%g%g%g%g%g%g%g%g%g%g%f",
                  0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                  0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.23456);
    TRY_STRFORMAT("123", "%s%s%s%s%s%i", "", "", "", "", "", 123);
    TRY_STRFORMAT("173", "%s%s%s%s%s%o", "", "", "", "", "", 123);
    TRY_STRFORMAT("0x12345678", "%s%s%s%s%s%p", "", "", "", "", "",
                  (void *)0x12345678);
    TRY_STRFORMAT("test", "%s%s%s%s%s%s", "", "", "", "", "", "test");
    TRY_STRFORMAT("123 1234", "%s%s%s%s%s%u %u", "", "", "", "", "",
                  123, 1234);
    TRY_STRFORMAT("7b", "%s%s%s%s%s%x", "", "", "", "", "", 123);
    TRY_STRFORMAT("7B", "%s%s%s%s%s%X", "", "", "", "", "", 123);

    TRY_STRFORMAT("  123", "%s%s%s%s%s%*d", "", "", "", "", "", 5, 123);

    TRY_STRFORMAT("123", "%s%s%s%s%s%ld", "", "", "", "", "", (long)123);
    TRY_STRFORMAT("123", "%s%s%s%s%s%lu", "", "", "", "", "", (long)123);
    TRY_STRFORMAT("173", "%s%s%s%s%s%lo", "", "", "", "", "", (long)123);
    TRY_STRFORMAT("123", "%s%s%s%s%s%lld", "", "", "", "", "", (long long)123);
    TRY_STRFORMAT("123", "%s%s%s%s%s%llu", "", "", "", "", "", (long long)123);
    TRY_STRFORMAT("173", "%s%s%s%s%s%llo", "", "", "", "", "", (long long)123);
    TRY_STRFORMAT("123", "%s%s%s%s%s%zd", "", "", "", "", "", (size_t)123);
    TRY_STRFORMAT("123", "%s%s%s%s%s%zu", "", "", "", "", "", (size_t)123);
    TRY_STRFORMAT("173", "%s%s%s%s%s%zo", "", "", "", "", "", (size_t)123);
    TRY_STRFORMAT("123", "%s%s%s%s%s%td", "", "", "", "", "", (ptrdiff_t)123);
    TRY_STRFORMAT("123", "%s%s%s%s%s%tu", "", "", "", "", "", (ptrdiff_t)123);
    TRY_STRFORMAT("173", "%s%s%s%s%s%to", "", "", "", "", "", (ptrdiff_t)123);
    TRY_STRFORMAT("123", "%s%s%s%s%s%jd", "", "", "", "", "", (intmax_t)123);
    TRY_STRFORMAT("123", "%s%s%s%s%s%ju", "", "", "", "", "", (intmax_t)123);
    TRY_STRFORMAT("173", "%s%s%s%s%s%jo", "", "", "", "", "", (intmax_t)123);
    return !failed;
}

/*-----------------------------------------------------------------------*/

#undef FAIL_ACTION
#define FAIL_ACTION  return 0

/*-----------------------------------------------------------------------*/

TEST(test_strformat_check)
{
    CHECK_TRUE(strformat_check(test_buf, sizeof(test_buf), ""));
    CHECK_TRUE(strformat_check(test_buf, sizeof(test_buf), "%d", 1));
    CHECK_FALSE(strformat_check(test_buf, sizeof(test_buf), "%*d",
                                (int)sizeof(test_buf)+1, 1));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_strformat_append)
{
    char *buf;
    int len;

    buf = NULL;
    len = 0;
    CHECK_TRUE(strformat_append(&buf, &len, 0, "test %s", "foo"));
    CHECK_INTEQUAL(len, 8);
    CHECK_STREQUAL(buf, "test foo");

    CHECK_TRUE(strformat_append(&buf, &len, 0, "%d", 42));
    CHECK_INTEQUAL(len, 10);
    CHECK_STREQUAL(buf, "test foo42");

    mem_free(buf);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_strformat_append_memory_failures)
{
    char *buf;
    int len;

    buf = NULL;
    len = 0;
    CHECK_MEMORY_FAILURES(strformat_append(&buf, &len, 0, "test %s", "foo"));
    CHECK_INTEQUAL(len, 8);
    CHECK_STREQUAL(buf, "test foo");

    CHECK_MEMORY_FAILURES(strformat_append(&buf, &len, 0, "%d", 42));
    CHECK_INTEQUAL(len, 10);
    CHECK_STREQUAL(buf, "test foo42");

    mem_free(buf);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_strformat_append_invalid)
{
    char *buf = NULL;
    int len = 0;
    CHECK_FALSE(strformat_append(NULL, &len, 0, "test"));
    CHECK_FALSE(strformat_append(&buf, NULL, 0, "test"));
    CHECK_FALSE(strformat_append(&buf, &len, 0, NULL));
    CHECK_PTREQUAL(buf, NULL);
    CHECK_INTEQUAL(len, 0);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_strformat_alloc)
{
    char *buf;
    CHECK_TRUE(buf = strformat_alloc("test %s", "foo"));
    CHECK_STREQUAL(buf, "test foo");
    mem_free(buf);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_strformat_alloc_memory_failures)
{
    char *buf;
    CHECK_MEMORY_FAILURES(buf = strformat_alloc("test %s", "foo"));
    CHECK_STREQUAL(buf, "test foo");
    mem_free(buf);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_strformat_alloc_invalid)
{
    CHECK_FALSE(strformat_alloc(NULL));
    return 1;
}

/*************************************************************************/
/*************************************************************************/
