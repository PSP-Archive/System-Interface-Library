/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/sysdep/psp/lalloc.h: Header for optimized memory allocator
 * for Lua.
 */

#ifndef SIL_SYSDEP_PSP_LALLOC_H
#define SIL_SYSDEP_PSP_LALLOC_H

#ifndef SIL_MEMORY_FORBID_MALLOC  // Not available if malloc() is disabled.

EXTERN_C_BEGIN

/*************************************************************************/
/*************************************************************************/

/**
 * lalloc:  Lua memory allocation function, called for all allocation,
 * resize, and free requests.
 *
 * [Parameters]
 *     ud: Opaque user data pointer (not used).
 *     ptr: Pointer to block to resize or free, or NULL for a new allocation.
 *     osize: Current size of block (not used).
 *     nsize: Requested new size of block, or zero to free the block.
 * [Return value]
 *     Pointer to allocated memory region, or NULL on error or block free.
 */
extern void *lalloc(void *ud, void *ptr, size_t osize, size_t nsize);

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_MEMORY_FORBID_MALLOC
#endif  // SIL_SYSDEP_PSP_LALLOC_H
