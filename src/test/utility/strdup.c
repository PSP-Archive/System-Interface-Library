/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/utility/strdup.c: Tests for strdup().
 */

#include "src/base.h"
#include "src/test/base.h"

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

int test_utility_strdup(void)
{
#ifdef SIL_MEMORY_FORBID_MALLOC
    SKIP("strdup() forbidden by SIL_MEMORY_FORBID_MALLOC, can't test.");
#else
# ifdef STRICMP_IS_RENAMED
    char *s = strdup_SIL("test");
# else
    char *s = strdup("test");
# endif
    CHECK_STREQUAL(s, "test");
    free(s);
    return 1;
#endif
}

/*************************************************************************/
/*************************************************************************/
