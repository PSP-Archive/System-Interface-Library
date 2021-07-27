/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/utility/strdup.c: strdup() implementation for systems which lack one.
 */

#include "src/base.h"

#ifdef NEED_STRDUP  // Defined by base.h if necessary.

/*************************************************************************/
/*************************************************************************/

char *RENAME_STRDUP(strdup)(const char *s)
{
    const size_t size = strlen(s) + 1;
    char *copy = malloc(size);
    if (copy) {
        memcpy(copy, s, size);
    }
    return copy;
}

/*************************************************************************/
/*************************************************************************/

#endif  // NEED_STRDUP
