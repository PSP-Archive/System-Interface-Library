/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/utility/stricmp.c: Case-insensitive string comparison functions for
 * systems which lack them.
 */

#include "src/base.h"

#ifdef NEED_STRICMP  // Defined by base.h if necessary.

/*************************************************************************/
/*************************************************************************/

/**
 * stricmp_tolower:  Helper function to return the lower-case version of a
 * character.  Defined here to avoid the necessity of multiple casts to
 * unsigned char in stricmp() and strnicmp().
 *
 * [Parameters]
 *     c: Character to convert to lower-case.
 * [Return value]
 *     Corresponding lower-case character.
 */
static inline CONST_FUNCTION unsigned char stricmp_tolower(char c)
{
    static const unsigned char stricmp_lower_table[256] = {
          0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
         16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
         32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
         48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
         64, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,
        112,113,114,115,116,117,118,119,120,121,122, 91, 92, 93, 94, 95,
         96, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,
        112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
        128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
        144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
        160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
        176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
        192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
        208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
        224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
        240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255,
    };
    return stricmp_lower_table[(unsigned char)c];
}

/*-----------------------------------------------------------------------*/

PURE_FUNCTION int RENAME_STRICMP(stricmp)(const char *s1, const char *s2)
{
    unsigned char c1, c2;
    while (c1 = stricmp_tolower(*s1++),
           c2 = stricmp_tolower(*s2++),
           c1 != 0 && c2 != 0) {
        if (c1 != c2) {
            break;
        }
    }
    return (int)c1 - (int)c2;
}

PURE_FUNCTION int RENAME_STRICMP(strnicmp)(const char *s1, const char *s2,
                                           STRNICMP_SIZE_T n)
{
    unsigned char c1 = 0, c2 = 0;  // Handle the n==0 case properly.
    while (n-- != 0 && (c1 = stricmp_tolower(*s1++),
                        c2 = stricmp_tolower(*s2++),
                        c1 != 0 && c2 != 0)) {
        if (c1 != c2) {
            break;
        }
    }
    return (int)c1 - (int)c2;
}

/*************************************************************************/
/*************************************************************************/

#endif  // NEED_STRICMP
