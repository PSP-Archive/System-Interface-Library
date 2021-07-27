/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/stdlib.c: Implementations for some useful functions from
 * <stdlib.h> not defined by psplibc.
 */

#include "src/base.h"

/*************************************************************************/
/*************************************************************************/

int atoi(const char *s)
{
    return (int)atol(s);  // int and long are the same type on the PSP.
}

long atol(const char *s)
{
    return strtol(s, NULL, 10);
}

/*************************************************************************/
/*************************************************************************/
