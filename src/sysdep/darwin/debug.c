/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/darwin/debug.c: Debugging utility functions for Darwin-based
 * systems (Mac/iOS).
 */

#ifdef DEBUG  // To the end of the file.

#define IN_SYSDEP

#include "src/base.h"
#include "src/sysdep.h"
#include "src/sysdep/darwin/meminfo.h"

/*************************************************************************/
/*************************************************************************/

int sys_debug_get_memory_stats(
    int64_t *total_ret, int64_t *self_ret, int64_t *avail_ret)
{
    *total_ret = darwin_get_total_memory();
    *self_ret = darwin_get_process_size();
    *avail_ret = darwin_get_free_memory();

    /* *avail_ret could legitimately be zero (though it would almost
     * certainly never be zero in practice). */
    return *total_ret != 0 && *self_ret != 0;
}

/*************************************************************************/
/*************************************************************************/

#endif  // DEBUG
