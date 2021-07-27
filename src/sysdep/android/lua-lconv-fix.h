/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/android/lua-lconv-fix.h: Workaround for an Android system
 * header bug which breaks Lua compilation.
 */

#ifndef SIL_SRC_SYSDEP_ANDROID_LUA_LCONV_FIX_H
#define SIL_SRC_SYSDEP_ANDROID_LUA_LCONV_FIX_H

/*************************************************************************/
/*************************************************************************/

/* Android defines a nonstandard "struct lconv" which breaks compilation of
 * Lua.  Replace it with our own, and make sure the system's localeconv()
 * (which would return the original structure) is never called. */

#define lconv __android_lconv
#include <locale.h>  // Make sure we bring it in now rather than later.
#undef lconv

struct lconv {
    char decimal_point[1];
};
#define localeconv() NULL

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_ANDROID_LUA_LCONV_FIX_H
