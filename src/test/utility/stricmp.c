/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/utility/stricmp.c: Tests for stricmp() and strnicmp().
 */

#include "src/base.h"
#include "src/test/base.h"

/*************************************************************************/
/********************** Local routine declarations ***********************/
/*************************************************************************/

/**
 * try_stricmp, try_strnicmp:  Call stricmp() or strnicmp() and check the
 * result against the expected return value.  If the system uses a built-in
 * stricmp() (or strcasecmp(), etc.), test with both that function and the
 * one in utility/stricmp.c.
 *
 * [Parameters]
 *     line: Source line number of the test.
 *     expect: Expected return value.
 *     s1, s2, n: Arguments to str[n]icmp().
 * [Return value]
 *     True if the result matched the expected value in all cases, false
 *     otherwise.
 */
static int try_stricmp(int line, int expect, const char *s1, const char *s2);
static int try_strnicmp(int line, int expect, const char *s1, const char *s2,
                        unsigned int n);

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

int test_utility_stricmp(void)
{
    int failed = 0;

#define TRY_STRICMP(expect,s1,s2) \
    (failed |= !try_stricmp(__LINE__, (expect), (s1), (s2)))
#define TRY_STRNICMP(expect,s1,s2,n) \
    (failed |= !try_strnicmp(__LINE__, (expect), (s1), (s2), (n)))

    /* First check simple cases with no letters. */

    TRY_STRICMP (-1, "012", "123");
    TRY_STRICMP (-1, "112", "123");
    TRY_STRICMP (-1, "122", "123");
    TRY_STRICMP ( 0, "123", "123");
    TRY_STRICMP (+1, "124", "123");
    TRY_STRICMP (+1, "134", "123");
    TRY_STRICMP (+1, "234", "123");
    TRY_STRNICMP(-1, "012", "123", 999);
    TRY_STRNICMP(-1, "112", "123", 999);
    TRY_STRNICMP(-1, "122", "123", 999);
    TRY_STRNICMP( 0, "123", "123", 999);
    TRY_STRNICMP(+1, "124", "123", 999);
    TRY_STRNICMP(+1, "134", "123", 999);
    TRY_STRNICMP(+1, "234", "123", 999);

    /* Check that strnicmp() respects the length parameters. */

    TRY_STRNICMP(-1, "012", "123", 3);
    TRY_STRNICMP(-1, "112", "123", 3);
    TRY_STRNICMP(-1, "122", "123", 3);
    TRY_STRNICMP( 0, "123", "123", 3);
    TRY_STRNICMP(+1, "124", "123", 3);
    TRY_STRNICMP(+1, "134", "123", 3);
    TRY_STRNICMP(+1, "234", "123", 3);
    TRY_STRNICMP(-1, "012", "123", 2);
    TRY_STRNICMP(-1, "112", "123", 2);
    TRY_STRNICMP( 0, "122", "123", 2);
    TRY_STRNICMP( 0, "123", "123", 2);
    TRY_STRNICMP( 0, "124", "123", 2);
    TRY_STRNICMP(+1, "134", "123", 2);
    TRY_STRNICMP(+1, "234", "123", 2);
    TRY_STRNICMP(-1, "012", "123", 1);
    TRY_STRNICMP( 0, "112", "123", 1);
    TRY_STRNICMP( 0, "122", "123", 1);
    TRY_STRNICMP( 0, "123", "123", 1);
    TRY_STRNICMP( 0, "124", "123", 1);
    TRY_STRNICMP( 0, "134", "123", 1);
    TRY_STRNICMP(+1, "234", "123", 1);
    TRY_STRNICMP( 0, "012", "123", 0);
    TRY_STRNICMP( 0, "112", "123", 0);
    TRY_STRNICMP( 0, "122", "123", 0);
    TRY_STRNICMP( 0, "123", "123", 0);
    TRY_STRNICMP( 0, "124", "123", 0);
    TRY_STRNICMP( 0, "134", "123", 0);
    TRY_STRNICMP( 0, "234", "123", 0);

    /* Check behavior with empty strings. */

    TRY_STRICMP (-1, "",    "123");
    TRY_STRICMP ( 0, "",    "");
    TRY_STRICMP (+1, "123", "");
    TRY_STRNICMP(-1, "",    "123", 999);
    TRY_STRNICMP( 0, "",    "",    999);
    TRY_STRNICMP(+1, "123", "",    999);

    /* Check alphabetic strings with matching case. */

    TRY_STRICMP (-1, "ABC", "BCD");
    TRY_STRICMP (-1, "BBC", "BCD");
    TRY_STRICMP (-1, "BCC", "BCD");
    TRY_STRICMP ( 0, "BCD", "BCD");
    TRY_STRICMP (+1, "BCE", "BCD");
    TRY_STRICMP (+1, "BDE", "BCD");
    TRY_STRICMP (+1, "CDE", "BCD");
    TRY_STRICMP (-1, "abc", "bcd");
    TRY_STRICMP (-1, "bbc", "bcd");
    TRY_STRICMP (-1, "bcc", "bcd");
    TRY_STRICMP ( 0, "bcd", "bcd");
    TRY_STRICMP (+1, "bce", "bcd");
    TRY_STRICMP (+1, "bde", "bcd");
    TRY_STRICMP (+1, "cde", "bcd");
    TRY_STRNICMP(-1, "ABC", "BCD", 999);
    TRY_STRNICMP(-1, "BBC", "BCD", 999);
    TRY_STRNICMP(-1, "BCC", "BCD", 999);
    TRY_STRNICMP( 0, "BCD", "BCD", 999);
    TRY_STRNICMP(+1, "BCE", "BCD", 999);
    TRY_STRNICMP(+1, "BDE", "BCD", 999);
    TRY_STRNICMP(+1, "CDE", "BCD", 999);
    TRY_STRNICMP(-1, "abc", "bcd", 999);
    TRY_STRNICMP(-1, "bbc", "bcd", 999);
    TRY_STRNICMP(-1, "bcc", "bcd", 999);
    TRY_STRNICMP( 0, "bcd", "bcd", 999);
    TRY_STRNICMP(+1, "bce", "bcd", 999);
    TRY_STRNICMP(+1, "bde", "bcd", 999);
    TRY_STRNICMP(+1, "cde", "bcd", 999);

    /* Check alphabetic strings with differing case. */

    TRY_STRICMP (-1, "ABC", "bcd");
    TRY_STRICMP (-1, "BBC", "bcd");
    TRY_STRICMP (-1, "BCC", "bcd");
    TRY_STRICMP ( 0, "BCD", "bcd");
    TRY_STRICMP (+1, "BCE", "bcd");
    TRY_STRICMP (+1, "BDE", "bcd");
    TRY_STRICMP (+1, "CDE", "bcd");
    TRY_STRICMP (-1, "abc", "BCD");
    TRY_STRICMP (-1, "bbc", "BCD");
    TRY_STRICMP (-1, "bcc", "BCD");
    TRY_STRICMP ( 0, "bcd", "BCD");
    TRY_STRICMP (+1, "bce", "BCD");
    TRY_STRICMP (+1, "bde", "BCD");
    TRY_STRICMP (+1, "cde", "BCD");
    TRY_STRNICMP(-1, "ABC", "bcd", 999);
    TRY_STRNICMP(-1, "BBC", "bcd", 999);
    TRY_STRNICMP(-1, "BCC", "bcd", 999);
    TRY_STRNICMP( 0, "BCD", "bcd", 999);
    TRY_STRNICMP(+1, "BCE", "bcd", 999);
    TRY_STRNICMP(+1, "BDE", "bcd", 999);
    TRY_STRNICMP(+1, "CDE", "bcd", 999);
    TRY_STRNICMP(-1, "abc", "BCD", 999);
    TRY_STRNICMP(-1, "bbc", "BCD", 999);
    TRY_STRNICMP(-1, "bcc", "BCD", 999);
    TRY_STRNICMP( 0, "bcd", "BCD", 999);
    TRY_STRNICMP(+1, "bce", "BCD", 999);
    TRY_STRNICMP(+1, "bde", "BCD", 999);
    TRY_STRNICMP(+1, "cde", "BCD", 999);

    /* Check that alphabetic strings are treated as lowercase for
     * comparison against other characters. */

    TRY_STRICMP (-1, "___", "ABC");
    TRY_STRICMP (+1, "ABC", "___");
    TRY_STRNICMP(-1, "___", "ABC", 999);
    TRY_STRNICMP(+1, "ABC", "___", 999);

    /* Check that non-ASCII letters with differing case do _not_ compare
     * equal, as currently specified by the interface definition.  We use
     * Unicode fullwidth ASCII (U+FFxx) for this test. */

    TRY_STRICMP (-1, "ＢＣＤ", "ｂｃｄ");
    TRY_STRICMP (+1, "ｂｃｄ", "ＢＣＤ");
    TRY_STRNICMP(-1, "ＢＣＤ", "ｂｃｄ", 999);
    TRY_STRNICMP(+1, "ｂｃｄ", "ＢＣＤ", 999);

    /* All done. */

    return !failed;

#undef TRY_STRICMP
#undef TRY_STRNICMP
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static int try_stricmp(int line, int expect, const char *s1, const char *s2)
{
    const int retval = stricmp(s1, s2);
    if ((expect <  0 && retval >= 0)
     || (expect == 0 && retval != 0)
     || (expect >  0 && retval <= 0)) {
        FAIL("Line %d: stricmp(\"%s\",\"%s\"): bad return value (%d,"
             " expected %s0)", line, s1, s2, retval,
             expect<0 ? "<" : expect>0 ? ">" : "");
        return 0;
    }

#ifdef STRICMP_IS_RENAMED
    const int retval2 = stricmp_SIL(s1, s2);
    if ((expect <  0 && retval2 >= 0)
     || (expect == 0 && retval2 != 0)
     || (expect >  0 && retval2 <= 0)) {
        FAIL("Line %d: stricmp_SIL(\"%s\",\"%s\"): bad return value (%d,"
             " expected %s0)", line, s1, s2, retval2,
             expect<0 ? "<" : expect>0 ? ">" : "");
        return 0;
    }
#endif

    return 1;
}

static int try_strnicmp(int line, int expect, const char *s1, const char *s2,
                        unsigned int n)
{
    const int retval = strnicmp(s1, s2, n);
    if ((expect <  0 && retval >= 0)
     || (expect == 0 && retval != 0)
     || (expect >  0 && retval <= 0)) {
        FAIL("Line %d: strnicmp(\"%s\",\"%s\",%d): bad return value (%d,"
             " expected %s0)", line, s1, s2, n, retval,
             expect<0 ? "<" : expect>0 ? ">" : "");
        return 0;
    }

#ifdef STRICMP_IS_RENAMED
    const int retval2 = strnicmp_SIL(s1, s2, n);
    if ((expect <  0 && retval2 >= 0)
     || (expect == 0 && retval2 != 0)
     || (expect >  0 && retval2 <= 0)) {
        FAIL("Line %d: strnicmp_SIL(\"%s\",\"%s\",%d): bad return value"
             " (%d, expected %s0)", line, s1, s2, n, retval2,
             expect<0 ? "<" : expect>0 ? ">" : "");
        return 0;
    }
#endif

    return 1;
}

/*************************************************************************/
/*************************************************************************/
