/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/memory-low.c: PSP low-level memory management functions.
 */

#include "src/base.h"
#include "src/sysdep.h"
#include "src/sysdep/psp/internal.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Address and size of each memory pool. */
static void *main_pool, *temp_pool;
static uint32_t main_poolsize, temp_poolsize;

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int psp_mem_alloc_pools(void **main_pool_ret, uint32_t *main_poolsize_ret,
                        void **temp_pool_ret, uint32_t *temp_poolsize_ret)
{
    temp_poolsize = align_up((SIL_PLATFORM_PSP_MEMORY_POOL_TEMP_SIZE), 4096);
    if ((SIL_PLATFORM_PSP_MEMORY_POOL_SIZE) <= 0) {
        main_poolsize = align_down(sceKernelTotalFreeMemSize(), 4096);
        uint32_t spare = temp_poolsize
            + align_up(-(SIL_PLATFORM_PSP_MEMORY_POOL_SIZE), 4096);
        if (main_poolsize <= spare) {
            DLOG("Not enough memory! (main=%d temp=%u total_free=%u)",
                 (SIL_PLATFORM_PSP_MEMORY_POOL_SIZE), temp_poolsize,
                 sceKernelTotalFreeMemSize());
            return 0;
        }
        main_poolsize -= spare;
        DLOG("Using main pool size of %u (total_free=%u spare=%u)",
             main_poolsize, sceKernelTotalFreeMemSize(), spare);
        uint32_t max_free = sceKernelMaxFreeMemSize();
        if (main_poolsize > max_free) {
            main_poolsize = max_free;
            DLOG("Shrinking main pool size to max_free (%u)", main_poolsize);
        }
    } else {
        main_poolsize = SIL_PLATFORM_PSP_MEMORY_POOL_SIZE;
    }

    SceUID block = sceKernelAllocPartitionMemory(
        PSP_MEMORY_PARTITION_USER, "SILMainPool", PSP_SMEM_Low,
        main_poolsize, NULL);
    if (block <= 0) {
        DLOG("Not enough memory! (want=%u total_free=%u max_free=%u)",
             main_poolsize, sceKernelTotalFreeMemSize(),
             sceKernelMaxFreeMemSize());
        return 0;
    }
    main_pool = sceKernelGetBlockHeadAddr(block);
    mem_clear(main_pool, main_poolsize);

    if (temp_poolsize == 0) {
        DLOG("Not using a temporary pool");
        temp_pool = NULL;
    } else {
        DLOG("Using temporary pool size of %u", temp_poolsize);
        uint32_t max_free = sceKernelMaxFreeMemSize();
        if (temp_poolsize > max_free) {
            temp_poolsize = max_free;
            DLOG("Shrinking temporary pool size to max_free (%u)",
                 temp_poolsize);
        }
        block = sceKernelAllocPartitionMemory(
            PSP_MEMORY_PARTITION_USER, "SILTempPool", PSP_SMEM_Low,
            temp_poolsize, NULL);
        if (block > 0) {
            temp_pool = sceKernelGetBlockHeadAddr(block);
        } else {
            DLOG("sceKernelMaxFreeMemSize() lied!!");
            temp_pool = NULL;
            temp_poolsize = 0;
        }
    }
    if (temp_pool) {
        mem_clear(temp_pool, temp_poolsize);
    }

    *main_pool_ret     = main_pool;
    *main_poolsize_ret = main_poolsize;
    *temp_pool_ret     = temp_pool;
    *temp_poolsize_ret = temp_poolsize;
    return 1;
}

/*************************************************************************/
/*************************************************************************/
