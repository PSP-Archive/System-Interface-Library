/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/darwin/meminfo.h: Header for Darwin-specific memory
 * information functions.
 */

#ifndef SIL_SRC_SYSDEP_DARWIN_MEMINFO_H
#define SIL_SRC_SYSDEP_DARWIN_MEMINFO_H

/*************************************************************************/
/*************************************************************************/

/**
 * darwin_get_total_memory:  Return the total amount of memory installed
 * in the system.
 *
 * [Return value]
 *     Amount of memory installed in the system, in bytes.
 */
extern int64_t darwin_get_total_memory(void);

/**
 * darwin_get_process_size:  Return the amount of memory used by the
 * current process.
 *
 * [Return value]
 *     Amount of memory used by the current process, in bytes.
 */
extern int64_t darwin_get_process_size(void);

/**
 * darwin_get_free_memory:  Return the amount of memory in the system
 * available for allocation.
 *
 * [Return value]
 *     Amount of memory installed in the system, in bytes.
 */
extern int64_t darwin_get_free_memory(void);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_DARWIN_MEMINFO_H
