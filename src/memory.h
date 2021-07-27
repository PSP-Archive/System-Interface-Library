/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/memory.h: Internal header for extended memory management.
 */

#ifndef SIL_SRC_MEMORY_H
#define SIL_SRC_MEMORY_H

#include "SIL/memory.h"  // Include the public header.

/*************************************************************************/
/************************ Test control interface *************************/
/*************************************************************************/

#ifdef SIL_INCLUDE_TESTS

/**
 * TEST_mem_use:  Use the given pointer as the result of the next
 * allocation operation (mem_alloc() or mem_realloc() with size > 0),
 * provided that the allocation request is compatible with the block at
 * the given pointer; otherwise, the block is freed and allocation
 * proceeds normally.  For the request to be compatible:
 *    - The requested size must be no larger than the size of the block.
 *    - The effective requested alignment (the requested alignment if it
 *         is nonzero, otherwise sizeof(void *)) must be no larger than
 *         the alignment of the pointer.
 *
 * If the pointer passed to this function is received by mem_free() before
 * being used for an allocation, the block will be freed normally and not
 * used for any subsequent allocation.
 *
 * If the block passed to a previous call to TEST_mem_use() has not yet
 * been returned, that block is freed.
 *
 * Note that if the requested size for which the block is returned is less
 * than the block's size, excess memory will not be returned to the system
 * until the block is freed.
 *
 * ptr must point to a block previously allocated with mem_alloc() or
 * mem_realloc(), or must be NULL (in which case any pending block is
 * freed but no new block is set).
 *
 * [Parameters]
 *     ptr: Pointer to block to use as next allocation result, or NULL to
 *         clear a pending use block.
 */
extern void TEST_mem_use(void *ptr);

/**
 * TEST_mem_push_alloc_list:  Clear the list of allocated blocks, returning
 * a pointer to the list for later use with TEST_mem_pop_alloc_list().
 * This allows testing in a consistent environment, even when initalization
 * routines have already allocated memory.
 *
 * Blocks which were allocated before this function was called MUST NOT be
 * freed until TEST_mem_pop_alloc_list() has been called.
 *
 * Push and pop calls can be nested, as long as the return values from
 * nested calls to this function are properly passed in the reverse order
 * to TEST_mem_pop_alloc_list().
 *
 * [Return value]
 *     List pointer to pass to TEST_mem_pop_alloc_list().
 */
extern void *TEST_mem_push_alloc_list(void);

/**
 * TEST_mem_pop_alloc_list:  Restore the list of allocated blocks saved
 * with TEST_mem_push_alloc_list().  The saved list is merged with the
 * current list, so any existing blocks are preserved and can be used as
 * normal.
 *
 * [Parameters]
 *     list: List pointer returned from TEST_mem_push_alloc_list().
 */
extern void TEST_mem_pop_alloc_list(void *list);

/**
 * TEST_mem_fail_after:  Force a memory allocation call to fail after
 * "calls" allocation calls (whether successful or not) have been made.
 * Pass a negative value for "calls" to cancel a pending failure.
 *
 * [Parameters]
 *     calls: Number of calls after which to fail (0 = fail the next call,
 *         -1 = cancel any pending failure).
 *     every: Call count interval at which to fail subsequent calls
 *         (1 = fail every call after the first, 2 = fail every second
 *         call after the first, 0 = fail once and succeed thereafter).
 *     fail_on_shrink: True to cause mem_realloc() calls to fail when they
 *         would shrink or leave unchanged the size of an existing block;
 *         false to allow such calls to succeed even when mem_alloc() would
 *         fail.
 */
extern void TEST_mem_fail_after(int calls, int every, int fail_on_shrink);

/**
 * TEST_mem_fail_sys_alloc:  Enable or disable force-failing of system
 * memory allocations.  Only has an effect when the default system memory
 * allocator is used (i.e., SIL_MEMORY_CUSTOM is undefined).
 *
 * [Parameters]
 *     fail: True to force failures for sys_mem_alloc() and sys_mem_realloc();
 *         false for normal behavior.
 */
extern void TEST_mem_fail_sys_alloc(int fail);

/**
 * TEST_mem_set_sys_avail:  Set the amount of available memory returned by
 * sys_mem_avail() and sys_mem_contig().  Only has an effect when the
 * default system memory allocator is used (i.e., SIL_MEMORY_CUSTOM is
 * undefined).
 *
 * [Parameters]
 *     main_avail: Value to return for sys_mem_avail(0).
 *     main_contig: Value to return for sys_mem_contig(0).
 *     temp_avail: Value to return for sys_mem_avail(MEM_ALLOC_TEMP).
 *     temp_contig: Value to return for sys_mem_contig(MEM_ALLOC_TEMP).
 */
extern void TEST_mem_set_sys_avail(int64_t main_avail, int64_t main_contig,
                                   int64_t temp_avail, int64_t temp_contig);

#endif

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_MEMORY_H
