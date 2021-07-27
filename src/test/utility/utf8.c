/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/utility/utf8.c: Tests for UTF-8 processing functions.
 */

#define FAIL_ACTION  failed = 1  // Continue through failures.

#include "src/base.h"
#include "src/test/base.h"
#include "src/utility/utf8.h"

/*************************************************************************/
/*************************************************************************/

int test_utility_utf8(void)
{
    int failed = 0;

    /* Macro to run one test on both utf8_read() and utf8_charlen(). */
    #define TEST_UTF8(string,firstchar,charlen)  do {           \
        const char * const _string = (string);                  \
        const int32_t _firstchar = (firstchar);                 \
        const int _charlen = (charlen);                         \
        const int _consume = (_firstchar==-1 ? 1 : _charlen);   \
        const char *_s = _string;                               \
        const int32_t _got_firstchar = utf8_read(&_s);          \
        const int _got_charlen = utf8_charlen(_string);         \
        if (_got_firstchar != _firstchar) {                     \
            FAIL("utf8_read() returned %d, expecting %d",       \
                 _got_firstchar, _firstchar);                   \
        }                                                       \
        if (_s != _string + _consume) {                         \
            FAIL("utf8_read() consumed %d bytes, expecting %d", \
                 (int)(_s - _string), _consume);                \
        }                                                       \
        if (_got_charlen != _charlen) {                         \
            FAIL("utf8_charlen() returned %d, expecting %d",    \
                 _got_charlen, _charlen);                       \
        }                                                       \
    } while (0)

    /* Check processing of the empty string. */
    TEST_UTF8("", 0, 0);

    /* Check processing of characters of each possible length. */
    TEST_UTF8("\x12",                           0x12, 1);
    TEST_UTF8("\xC2\x80",                       0x80, 2);
    TEST_UTF8("\xE1\x80\x80",                 0x1000, 3);
    TEST_UTF8("\xF1\x80\x80\x80",            0x40000, 4);
    TEST_UTF8("\xF9\x80\x80\x80\x80",      0x1000000, 5);
    TEST_UTF8("\xFD\x80\x80\x80\x80\x80", 0x40000000, 6);

    /* Check processing of invalid UTF-8 byte sequences. */
    TEST_UTF8("\x80",                     -1, 0);
    TEST_UTF8("\xC2\x01",                 -1, 0);
    TEST_UTF8("\xC2\xFF",                 -1, 0);
    TEST_UTF8("\xE1\x01\x80",             -1, 0);
    TEST_UTF8("\xE1\xFF\x80",             -1, 0);
    TEST_UTF8("\xE1\x80\x01",             -1, 0);
    TEST_UTF8("\xE1\x80\xFF",             -1, 0);
    TEST_UTF8("\xF1\x01\x80\x80",         -1, 0);
    TEST_UTF8("\xF1\xFF\x80\x80",         -1, 0);
    TEST_UTF8("\xF1\x80\x01\x80",         -1, 0);
    TEST_UTF8("\xF1\x80\xFF\x80",         -1, 0);
    TEST_UTF8("\xF1\x80\x80\x01",         -1, 0);
    TEST_UTF8("\xF1\x80\x80\xFF",         -1, 0);
    TEST_UTF8("\xF9\x01\x80\x80\x80",     -1, 0);
    TEST_UTF8("\xF9\xFF\x80\x80\x80",     -1, 0);
    TEST_UTF8("\xF9\x80\x01\x80\x80",     -1, 0);
    TEST_UTF8("\xF9\x80\xFF\x80\x80",     -1, 0);
    TEST_UTF8("\xF9\x80\x80\x01\x80",     -1, 0);
    TEST_UTF8("\xF9\x80\x80\xFF\x80",     -1, 0);
    TEST_UTF8("\xF9\x80\x80\x80\x01",     -1, 0);
    TEST_UTF8("\xF9\x80\x80\x80\xFF",     -1, 0);
    TEST_UTF8("\xFD\x01\x80\x80\x80\x80", -1, 0);
    TEST_UTF8("\xFD\xFF\x80\x80\x80\x80", -1, 0);
    TEST_UTF8("\xFD\x80\x01\x80\x80\x80", -1, 0);
    TEST_UTF8("\xFD\x80\xFF\x80\x80\x80", -1, 0);
    TEST_UTF8("\xFD\x80\x80\x01\x80\x80", -1, 0);
    TEST_UTF8("\xFD\x80\x80\xFF\x80\x80", -1, 0);
    TEST_UTF8("\xFD\x80\x80\x80\x01\x80", -1, 0);
    TEST_UTF8("\xFD\x80\x80\x80\xFF\x80", -1, 0);
    TEST_UTF8("\xFD\x80\x80\x80\x80\x01", -1, 0);
    TEST_UTF8("\xFD\x80\x80\x80\x80\xFF", -1, 0);

    return !failed;
}

/*************************************************************************/
/*************************************************************************/
