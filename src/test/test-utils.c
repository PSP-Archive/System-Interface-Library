/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/test-utils.c: Miscellaneous utility routines for test code.
 */

#include "src/base.h"
#include "src/test/base.h"

#ifdef SIL_PLATFORM_WINDOWS
# include "src/sysdep/windows/internal.h"
# include "src/sysdep/windows/utf8-wrappers.h"
#endif

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

const char *testutil_getenv(const char *name)
{
    PRECOND(name != NULL, return "");

#if defined(SIL_PLATFORM_LINUX) || defined(SIL_PLATFORM_MACOSX)
    const char *value = getenv(name);
    return value ? value : "";
#elif defined(SIL_PLATFORM_WINDOWS)
    static char buffer[4096];
    const DWORD result = GetEnvironmentVariableU(name, buffer, sizeof(buffer));
    if (result >= sizeof(buffer)) {
        DLOG("%s: value too long (%u >= %zu), treating as nonexistent",
             name, result, sizeof(buffer));
        return "";
    } else if (result > 0) {
        buffer[result] = '\0';
        return buffer;
    } else {
        return "";
    }
#else
    return "";
#endif
}

/*************************************************************************/
/*************************************************************************/
