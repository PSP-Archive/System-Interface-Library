/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/memory.c: Memory management routines.
 */

/*
 * This file implements the sys_mem_*() system-level memory management
 * routines used by the high-level mem_*() functions.  Each memory pool is
 * divided into blocks, each of which starts with an AreaInfo header.
 * When an allocation is requested, the code looks through the free list
 * for a block of at least the requested size and converts it to an in-use
 * block, splitting off the unused portion (if any) into a new free block.
 * The pointer returned to the caller is located just past the end of the
 * block header, rounded up to the requested alignment if necessary.
 * Freeing an allocated block is then simply a matter of returning the
 * block to the free list, merging it with any adjoining free blocks.
 * Resize requests are handled by first attempting to resize the block in
 * place (which will always succeed if the requested size is no larger
 * than the current size); if that fails, a new block is allocated and
 * the data copied over from the old block, which is then freed.
 *
 * To reduce the risk of fragmentation, all block sizes are rounded up to
 * a multiple of MEM_BLOCKSIZE (defined below).  Because of this, and also
 * because it is not optimized for fast allocation, this implementation is
 * not suited to numerous allocations of small blocks, such as would be
 * used by a naive implementation of the C++ STL.
 *
 * This implementation is _not_ thread-safe.  Allocations must all be
 * performed from a single thread, or calls to these functions must be
 * protected by a lock to ensure that no more than one thread calls them
 * at any time.
 */

#include "src/base.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/psp/internal.h"

/*************************************************************************/
/************************* Configuration options *************************/
/*************************************************************************/

/**
 * POINTER_CHECK:  If defined, sys_mem_realloc() and sys_mem_free() will
 * check that the passed-in pointer points to an allocated memory block by
 * verifying the validity of the AreaInfo structure immediately before the
 * pointer address.  This will slow down sys_mem_realloc() and
 * sys_mem_free() slightly.
 *
 * If both this and DEEP_POINTER_CHECK are defined, DEEP_POINTER_CHECK
 * takes precedence.
 */
#define POINTER_CHECK

/**
 * DEEP_POINTER_CHECK:  If defined, sys_mem_realloc() and sys_mem_free()
 * will check that the passed-in pointer points to an allocated memory
 * block by walking the block list and confirming that the pointer is the
 * correct user data pointer for an allocated block.  This will slow down
 * sys_mem_realloc() and sys_mem_free() significantly.
 */
// #define DEEP_POINTER_CHECK

/**
 * FREE_LIST_CHECK:  If defined, operations on the free list will check
 * after the operation that the list is properly constructed.  This will
 * slow down all memory operations significantly.
 */
// #define FREE_LIST_CHECK

/**
 * TRACE_ALLOCS:  If defined, all memory allocation/free operations will be
 * logged using DLOG().  Ignored if not building in debug mode.
 */
// #define TRACE_ALLOCS

/**
 * PROFILE_ALLOCS:  If defined, memory allocation/free operations will be
 * profiled, with profiling data logged using DLOG() every 10000 operations.
 * This can be used even if not building in debug mode, though naturally
 * the log messages will not be visible; use a debugger to grab the
 * profiling data.
 */
// #define PROFILE_ALLOCS

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

typedef struct AreaInfo_ AreaInfo;

/* Block size granularity for allocations. */
#define MEM_BLOCKSIZE   64

/* Data for memory pools. */
typedef struct MemoryPool_ {
    void *base;
    uint32_t size;
    AreaInfo *first_free;  // Free block with the lowest address.
    AreaInfo *last_free;   // Free block with the highest address.
} MemoryPool;
static MemoryPool
    main_pool,  // Pool for ordinary allocations.
    temp_pool;  // Pool for temporary allocations (to avoid fragmentation).

/* Memory block header structure. */
struct AreaInfo_ {
    uint32_t magic;     // Set to AREAINFO_MAGIC.
    uint32_t free:1,    // True if this is a free block, false if allocated.
             temp:1,    // True if this block belongs to the temporary pool.
             nblocks:30;// Size of this block in MEM_BLOCKSIZE units
                        //    (0 indicates the end of the pool).
    AreaInfo *prev;     // Pointer to the previous block (NULL if none).
    AreaInfo *prev_free;// Pointer to the previous free block (if a free block).
    AreaInfo *next_free;// Pointer to the next free block (if a free block).
    /* The following fields are only valid for allocated blocks. */
    uint32_t alloc_size:30, // Requested block size (in bytes).
             alloc_temp:1,  // State of the MEM_ALLOC_TEMP flag.
             alloc_top:1;   // State of the MEM_ALLOC_TOP flag.
    uint16_t align;     // Requested alignment (in bytes).
    uint16_t alignofs;  // Offset from the end of this structure to the user
                        //    data pointer.
    void *base;         // User data pointer returned by sys_mem_alloc().
};
#define AREAINFO_MAGIC  0xA4EA19F0

/* Macro to return a pointer to the block following the given one. */
#define NEXT_AREA(area) \
    ((AreaInfo *)((uintptr_t)(area) + ((area)->nblocks * MEM_BLOCKSIZE)))

/* Macro to check whether the given block is the fencepost at the end of
 * the memory pool. */
#define AREA_IS_FENCEPOST(area)  ((area)->nblocks == 0)

/*-----------------------------------------------------------------------*/

/* Logging macros. */

#if defined(DEBUG) && defined(TRACE_ALLOCS)

# define LOG_ALLOC(size,flags,ptr) \
    DLOG("alloc(%d,%d) -> %p", (size), (flags), (ptr))
# define LOG_REALLOC(old,size,flags,ptr) \
    DLOG("realloc(%p,%d,%d) -> %p", (old), (size), (flags), (ptr))
# define LOG_FREE(ptr) \
    DLOG("free(%p)", (ptr))

#else

# define LOG_ALLOC(size,flags,ptr)        /*nothing*/
# define LOG_REALLOC(old,size,flags,ptr)  /*nothing*/
# define LOG_FREE(ptr)                    /*nothing*/

#endif

/*-----------------------------------------------------------------------*/

/* Profiling data and macros. */

#ifdef PROFILE_ALLOCS

# include <pspuser.h>

static uint32_t malloc_usec, realloc_usec, free_usec;
static uint32_t malloc_calls, realloc_calls, free_calls;

# define CHECK_PRINT_PROFILE()  do {                                    \
    if (malloc_calls + realloc_calls + free_calls >= 10000) {           \
        DLOG("[profile]  malloc: %u usec / %u calls = %u usec/call",    \
             malloc_usec, malloc_calls,                                 \
             malloc_calls ? malloc_usec / malloc_calls : 0);            \
        DLOG("[profile] realloc: %u usec / %u calls = %u usec/call",    \
             realloc_usec, realloc_calls,                               \
             realloc_calls ? realloc_usec / realloc_calls : 0);         \
        DLOG("[profile]    free: %u usec / %u calls = %u usec/call",    \
             free_usec, free_calls,                                     \
             free_calls ? free_usec / free_calls : 0);                  \
        malloc_usec  = realloc_usec  = free_usec  = 0;                  \
        malloc_calls = realloc_calls = free_calls = 0;                  \
    }                                                                   \
} while (0)

#endif

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/*-------- Block management --------*/

/**
 * do_alloc:  Allocate a block of memory, and return a pointer to the
 * block's AreaInfo structure.
 *
 * [Parameters]
 *     pool: Pool in which to allocate memory.
 *     size: Number of bytes to allocate (must be nonzero).
 *     align: Alignment for allocation, in bytes (must be nonzero).
 *     top: True to allocate from the top of the pool, false to allocate
 *         from the bottom.
 * [Return value]
 *     AreaInfo pointer for the newly allocated block, or NULL on error.
 */
static AreaInfo *do_alloc(MemoryPool *pool, int size, int align, int top);

/**
 * do_free:  Free a block of memory, coalescing it with any preceding or
 * following free block.
 *
 * [Parameters]
 *     area: Block to free.
 */
static inline void do_free(AreaInfo *area);

/**
 * ptr_to_area:  Convert the pointer parameter passed to sys_mem_realloc()
 * or sys_mem_free() to an AreaInfo pointer.
 *
 * [Parameters]
 *     ptr: User data pointer.
 * [Return value]
 *     Pointer to the associated AreaInfo structure, or NULL if the pointer
 *     was invalid.
 */
static inline AreaInfo *ptr_to_area(const void *ptr);

/**
 * split_area:  Split the given block into two, marking one of the split
 * blocks as allocated.
 *
 * which == SPLIT_USE_BACK may only be used on free blocks.
 *
 * [Parameters]
 *     area: Block to split.
 *     newblocks: Desired size of the first block, in MEM_BLOCKSIZE units.
 *     which: Which of the two blocks to allocate (SPLIT_USE_FRONT for the
 *         first, SPLIT_USE_BACK for the second).
 * [Return value]
 *     Pointer to the allocated block.
 */
typedef enum {SPLIT_USE_FRONT, SPLIT_USE_BACK} SplitUseSelect;
static inline AreaInfo *split_area(AreaInfo *area, int newblocks,
                                   SplitUseSelect which);

/**
 * merge_free:  Attempt to merge the given free block with the following
 * block.  This function does nothing if the given block is at the end of
 * the pool or the following block is allocated.
 *
 * [Parameters]
 *     area: Block to merge.
 */
static inline void merge_free(AreaInfo *area);

/**
 * mark_used:  Mark the given block as allocated, and remove it from the
 * free list.
 *
 * [Parameters]
 *     area: Block to mark as allocated.
 * [Preconditions]
 *     area is on the free list.
 */
static inline void mark_used(AreaInfo *area);

/**
 * mark_free:  Mark the given block as free, and add it to the free list.
 *
 * [Parameters]
 *     area: Block to mark as free.
 * [Preconditions]
 *     area is not on the free list.
 *     area->prev is set correctly.
 */
static inline void mark_free(AreaInfo *area);

#ifdef FREE_LIST_CHECK
/**
 * free_list_check:  Check that the free list is properly constructed.
 *
 * This function is only defined when FREE_LIST_CHECK is enabled.
 */
static NOINLINE void free_list_check(void);
#endif

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

void *sys_mem_alloc(int64_t size_, int align, int flags)
{
#ifdef PROFILE_ALLOCS
    CHECK_PRINT_PROFILE();
    malloc_calls++;
    const uint32_t start = sceKernelGetSystemTimeLow();
#endif

    if (UNLIKELY(size_ > 0x7FFFFFFF)) {
        DLOG("Unable to allocate %lld bytes (size too large)", size_);
        return 0;
    }
    const int size = (int)size_;

    if (!align) {
        align = 16;
    }

    AreaInfo *newarea;
    if (!(flags & MEM_ALLOC_TEMP)
     || !(newarea = do_alloc(&temp_pool, size, align, flags & MEM_ALLOC_TOP)))
    {
        /* If the temporary pool was requested but we weren't able to
         * allocate from it, try to allocate from the main pool instead.
         * In this case, we allocate from the top of the pool to try and
         * avoid fragmentation due to subsequent main pool allocations. */
        newarea = do_alloc(&main_pool, size, align,
                           flags & (MEM_ALLOC_TOP | MEM_ALLOC_TEMP));
    }
    if (UNLIKELY(!newarea)) {
#ifdef PROFILE_ALLOCS
        malloc_usec += sceKernelGetSystemTimeLow() - start;
#endif
        DLOG("Unable to allocate %u bytes", size);
        return NULL;
    }
    void *base = newarea->base;
    newarea->alloc_temp = (flags & MEM_ALLOC_TEMP ? 1 : 0);
    newarea->alloc_top  = (flags & MEM_ALLOC_TOP  ? 1 : 0);

    LOG_ALLOC(size, flags, base);
#ifdef PROFILE_ALLOCS
    malloc_usec += sceKernelGetSystemTimeLow() - start;
#endif
    return base;
}

/*-----------------------------------------------------------------------*/

void *sys_mem_realloc(void *ptr, int64_t size_, int align, int flags)
{
#ifdef PROFILE_ALLOCS
    CHECK_PRINT_PROFILE();
    realloc_calls++;
    const uint32_t start = sceKernelGetSystemTimeLow();
#endif

    if (UNLIKELY(size_ > 0x7FFFFFFF)) {
        DLOG("Unable to allocate %llu bytes (size too large)", size_);
        return 0;
    }
    const int size = (int)size_;

    /* Verify the pointer's validity, and pull the AreaInfo pointers for
     * both this and the previous block. */
    AreaInfo *area = ptr_to_area(ptr);
    if (UNLIKELY(!area)) {
        DLOG("realloc(%p,%zu,%u): Invalid pointer!", ptr, size, flags);
        return NULL;
    }
    AreaInfo *prev = area->prev;
    const int oldsize = area->alloc_size;

    /* If the MEM_ALLOC_TEMP or MEM_ALLOC_TOP flags were changed, we'll
     * have to reallocate in any case. */
    if (area->alloc_temp != (flags & MEM_ALLOC_TEMP ? 1 : 0)
     || area->alloc_top  != (flags & MEM_ALLOC_TOP  ? 1 : 0)) {
        void *newbuf = sys_mem_alloc(size, align, flags & ~MEM_ALLOC_CLEAR);
        if (!newbuf) {
#ifdef PROFILE_ALLOCS
            realloc_usec += sceKernelGetSystemTimeLow() - start;
#endif
            return NULL;
        }
        memcpy(newbuf, ptr, min(size,oldsize));
        sys_mem_free(ptr);
        LOG_REALLOC(ptr, size, flags, newbuf);
#ifdef PROFILE_ALLOCS
        realloc_usec += sceKernelGetSystemTimeLow() - start;
#endif
        return newbuf;
    }

    /* Determine the required block size in MEM_BLOCKSIZE units. */
    const int nblocks =
        (sizeof(AreaInfo) + area->alignofs + size + MEM_BLOCKSIZE-1)
        / MEM_BLOCKSIZE;

    /* If the block isn't expanding, we can trivially do the operation
     * in place. */
    if (nblocks <= area->nblocks) {

        if (nblocks < area->nblocks) {
            (void) split_area(area, nblocks, SPLIT_USE_FRONT);
        }  // Otherwise, the size is unchanged and there's nothing to do.

    } else {  // nblocks > area->nblocks

        const int addblocks = nblocks - area->nblocks;
        AreaInfo *next = NEXT_AREA(area);
        if (AREA_IS_FENCEPOST(next)) {
            next = NULL;
        }

        /* First see if there's enough free space immediately following
         * this block to fulfill the request. */
        if (next && next->free && next->nblocks >= addblocks) {
            if (next->nblocks > addblocks) {
                (void) split_area(next, addblocks, SPLIT_USE_FRONT);
            } else {
                mark_used(next);
            }
            area->nblocks += addblocks;
            mem_clear(next, sizeof(*next));
            next = NEXT_AREA(area);
            next->prev = area;

        /* There wasn't enough free space after the block, so check again
         * including any free space immediately before the block. */
        } else if (prev && prev->free) {
            const int totalavail = prev->nblocks + area->nblocks
                                 + (next && next->free ? next->nblocks : 0);
            if (totalavail >= nblocks) {
                /* Move the data to the nearest end of the combined
                 * (prev + this + next) block.  This both avoids pushing
                 * data into the large free space that precedes ALLOC_TOP
                 * allocations, and also helps avoid overwriting user data
                 * with AreaInfo metadata (see below). */
                const int top =
                    !(next && next->free && next->nblocks > prev->nblocks);
                /* First merge the three blocks into one, which will be
                 * marked as free (for the moment). */
                const int alignofs = area->alignofs;
                mark_free(area);
                merge_free(prev);  // prev + this
                merge_free(prev);  // (prev + this) + next
                /*
                 * Split off enough of the block to fulfill the request.
                 * The new block will always contain the entire old block,
                 * due to the choice of movement direction above:
                 *    - If next->nblocks <= prev->nblocks, we will use all
                 *      of next and some nonzero portion of prev, since we
                 *      already determined that next alone cannot fulfill
                 *      the request.
                 *    - Otherwise, prev is smaller than next, so by the
                 *      same logic, we will use all of prev and some
                 *      nonzero portion of next.
                 * This ensures that AreaInfo metadata will not overwrite
                 * user data during the split operation.
                 */
                if (prev->nblocks == nblocks) {
                    mark_used(prev);
                    area = prev;
                } else if (top) {
                    area = split_area(prev, prev->nblocks - nblocks,
                                      SPLIT_USE_BACK);
                } else {
                    area = split_area(prev, nblocks, SPLIT_USE_FRONT);
                }
                /* Set up the metadata and move the user data itself. */
                area->align    = align;
                area->alignofs = alignofs;
                area->base     = (uint8_t *)area + sizeof(AreaInfo) + alignofs;
                area->alloc_size = size;
                memmove(area->base, ptr, oldsize);
            } else {  // totalavail < nblocks
                goto realloc_last_try;
            }

        /* There was insufficient room to expand the block in either
         * direction, so we'll have to allocate a separate block and copy
         * the data over. */
        } else {
          realloc_last_try:;
            const int oldalign = area->align;
            AreaInfo *newarea;
            if (!(flags & MEM_ALLOC_TEMP)
             || !(newarea = do_alloc(&temp_pool, size, oldalign,
                                     flags & MEM_ALLOC_TOP)))
            {
                newarea = do_alloc(&main_pool, size, oldalign,
                                   flags & (MEM_ALLOC_TOP | MEM_ALLOC_TEMP));
            }
            if (UNLIKELY(!newarea)) {
#ifdef PROFILE_ALLOCS
                realloc_usec += sceKernelGetSystemTimeLow() - start;
#endif
                DLOG("Unable to realloc %p (%u bytes) to %u bytes",
                     ptr, oldsize, size);
                return NULL;
            }
            memcpy(newarea->base, ptr, oldsize);
            do_free(area);
            area = newarea;
        }

    }  // if (nblocks <= area->blocks)

    void *base = area->base;
    area->alloc_size = size;
    area->alloc_temp = (flags & MEM_ALLOC_TEMP ? 1 : 0);
    area->alloc_top  = (flags & MEM_ALLOC_TOP  ? 1 : 0);

    LOG_REALLOC(ptr, size, flags, base);
#ifdef PROFILE_ALLOCS
    realloc_usec += sceKernelGetSystemTimeLow() - start;
#endif
    return base;
}

/*-----------------------------------------------------------------------*/

void sys_mem_free(void *ptr)
{
#ifdef PROFILE_ALLOCS
    CHECK_PRINT_PROFILE();
    free_calls++;
    const uint32_t start = sceKernelGetSystemTimeLow();
#endif

    if (ptr) {
        AreaInfo *area;
        area = ptr_to_area(ptr);
        if (UNLIKELY(!area)) {
#ifdef PROFILE_ALLOCS
            free_usec += sceKernelGetSystemTimeLow() - start;
#endif
            DLOG("free(%p): Invalid pointer!", ptr);
            return;
        }
        do_free(area);
        LOG_FREE(ptr);
    }

#ifdef PROFILE_ALLOCS
    free_usec += sceKernelGetSystemTimeLow() - start;
#endif
}

/*-----------------------------------------------------------------------*/

int64_t sys_mem_avail(int flags)
{
    int free_bytes = 0;
    AreaInfo *area =
        (flags & MEM_ALLOC_TEMP) ? (AreaInfo *)temp_pool.first_free
                                 : (AreaInfo *)main_pool.first_free;
    AreaInfo *next_free;
    for (; area; area = next_free) {
        next_free = area->next_free;
        free_bytes +=
            area->nblocks * MEM_BLOCKSIZE - align_up(sizeof(AreaInfo), 16);
    }
    return free_bytes;
}

/*-----------------------------------------------------------------------*/

int64_t sys_mem_contig(int flags)
{
    int max_contig = 0;
    AreaInfo *area =
        (flags & MEM_ALLOC_TEMP) ? (AreaInfo *)temp_pool.first_free
                                 : (AreaInfo *)main_pool.first_free;
    AreaInfo *next_free;
    for (; area; area = next_free) {
        next_free = area->next_free;
        if (area->nblocks > max_contig) {
            max_contig = area->nblocks;
        }
    }
    return max_contig * MEM_BLOCKSIZE - align_up(sizeof(AreaInfo), 16);
}

/*-----------------------------------------------------------------------*/

int sys_mem_max_align(void)
{
    return MEM_BLOCKSIZE;
}

/*************************************************************************/
/********************* PSP-internal library routines *********************/
/*************************************************************************/

int psp_mem_init(void)
{
    if (sizeof(AreaInfo) > MEM_BLOCKSIZE) {
        DLOG("Block size %d too small for AreaInfo size %d", MEM_BLOCKSIZE,
             (int)sizeof(AreaInfo));
        return 0;
    }

    if (!main_pool.base || !main_pool.size) {
        if (UNLIKELY(!psp_mem_alloc_pools(&main_pool.base, &main_pool.size,
                                          &temp_pool.base, &temp_pool.size))) {
            return 0;
        } else if (UNLIKELY(!main_pool.base) || UNLIKELY(!main_pool.size)) {
            DLOG("psp_mem_alloc_pools() failed to set a main pool!");
            return 0;
        }
    }

    AreaInfo *area = (AreaInfo *)main_pool.base;
    main_pool.first_free = main_pool.last_free = area;
    AreaInfo *fencepost;
    area->magic   = AREAINFO_MAGIC;
    area->free    = 1;
    area->temp    = 0;
    area->nblocks = (main_pool.size / MEM_BLOCKSIZE) - 1;
    area->prev    = NULL;
    fencepost =
        (AreaInfo *)((uintptr_t)area + (area->nblocks * MEM_BLOCKSIZE));
    fencepost->magic      = AREAINFO_MAGIC;
    fencepost->free       = 0;  // Make sure merge_area() doesn't try to
                                // merge the fencepost.
    fencepost->temp       = 0;
    fencepost->nblocks    = 0;
    fencepost->prev       = area;
    /* None of these are used, but initialize them just for completeness. */
    fencepost->alloc_size = 0;
    fencepost->alloc_temp = 0;
    fencepost->alloc_top  = 0;
    fencepost->align      = 1;
    fencepost->alignofs   = 0;
    fencepost->base       = NULL;

    if (temp_pool.base) {
        area = (AreaInfo *)temp_pool.base;
        temp_pool.first_free = temp_pool.last_free = area;
        area->magic   = AREAINFO_MAGIC;
        area->free    = 1;
        area->temp    = 1;
        area->nblocks = (temp_pool.size / MEM_BLOCKSIZE) - 1;
        area->prev    = NULL;
        fencepost =
            (AreaInfo *)((uintptr_t)area + (area->nblocks * MEM_BLOCKSIZE));
        fencepost->magic      = AREAINFO_MAGIC;
        fencepost->free       = 0;
        fencepost->temp       = 1;
        fencepost->nblocks    = 0;
        fencepost->prev       = area;
        fencepost->alloc_size = 0;
        fencepost->alloc_temp = 1;
        fencepost->alloc_top  = 0;
        fencepost->align      = 1;
        fencepost->alignofs   = 0;
        fencepost->base       = NULL;
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

int64_t psp_mem_total(void)
{
    return main_pool.size + temp_pool.size;
}

/*-----------------------------------------------------------------------*/

void psp_mem_get_pool_info(void **main_base_ret, uint32_t *main_size_ret,
                           void **temp_base_ret, uint32_t *temp_size_ret)
{
    *main_base_ret = main_pool.base;
    *main_size_ret = main_pool.size;
    *temp_base_ret = temp_pool.base;
    *temp_size_ret = temp_pool.size;
}

/*-----------------------------------------------------------------------*/

void psp_mem_report_allocs(void)
{
    for (int temp = 0; temp < 2; temp++) {
        AreaInfo *area = temp ? (AreaInfo *)temp_pool.base
                              : (AreaInfo *)main_pool.base;
        for (; !AREA_IS_FENCEPOST(area); area = NEXT_AREA(area)) {
            if (area->free) {
                DLOG("[%s] %p-%p, free", temp ? "temp" : "main",
                     area, (char *)area + (area->nblocks * MEM_BLOCKSIZE));
            } else {
                DLOG("[%s] %p-%p, allocated (%d)", temp ? "temp" : "main",
                     area, (char *)area + (area->nblocks * MEM_BLOCKSIZE),
                     area->alloc_size);
            }
        }
    }
}

/*************************************************************************/
/******************* Local routines (block management) *******************/
/*************************************************************************/

static AreaInfo *do_alloc(MemoryPool *pool, int size, int align, int top)
{
    PRECOND(pool != NULL, return NULL);
    PRECOND(pool->base != NULL, return NULL);
    PRECOND(size > 0, return NULL);
    PRECOND(align > 0, return NULL);
    if (UNLIKELY(align > MEM_BLOCKSIZE)) {
        DLOG("align(%d) > blocksize(%d) not supported", align, MEM_BLOCKSIZE);
        return NULL;
    }

    /* Calculate the user data offset required for the requested alignment
     * and the number of MEM_BLOCKSIZE units required.  An alignment of 0
     * means the default, which is 4 bytes on the PSP; since the AreaInfo
     * structure size is itself 4-byte aligned, we don't need a separate
     * offset in that case. */
    int alignofs;
    if (!align) {
        alignofs = 0;
    } else {
        alignofs = sizeof(AreaInfo) % align;
        if (alignofs > 0) {
            alignofs = align - alignofs;
        }
    }
    int nblocks = (sizeof(AreaInfo) + alignofs + size + MEM_BLOCKSIZE-1)
                  / MEM_BLOCKSIZE;

    /* Find a free block with sufficient space.  We just use a simple
     * linear search. */
    AreaInfo *found = NULL;
    for (AreaInfo *area = top ? pool->last_free : pool->first_free, *next;
         area && !found; area = next)
    {
        next = top ? area->prev_free : area->next_free;
        if (area->free && area->nblocks >= nblocks) {
            found = area;
        }
    }
    if (!found) {
        return NULL;
    }

    /* Claim the free block we found for our use.  If the block is larger
     * than the requested size, split off the remainder into a new free
     * block. */
    AreaInfo *newarea;
    if (found->nblocks == nblocks) {
        mark_used(found);
        newarea = found;
    } else if (top) {
        newarea = split_area(found, found->nblocks - nblocks, SPLIT_USE_BACK);
    } else {
        newarea = split_area(found, nblocks, SPLIT_USE_FRONT);
    }

    /* Initialize and return the newly-allocated block. */
    newarea->temp       = found->temp;
    newarea->nblocks    = nblocks;
    newarea->alloc_size = size;
    newarea->align      = align;
    newarea->alignofs   = (uint16_t)alignofs;
    newarea->base       = (uint8_t *)newarea + sizeof(AreaInfo) + alignofs;
    return newarea;
}

/*-----------------------------------------------------------------------*/

static inline void do_free(AreaInfo *area)
{
    PRECOND(area != NULL, return);
    AreaInfo *prev = area->prev;

    mark_free(area);
    if (prev && prev->free) {
        merge_free(prev);
        area = prev;
    }
    merge_free(area);
}

/*-----------------------------------------------------------------------*/

static inline AreaInfo *ptr_to_area(const void *ptr)
{
    PRECOND(ptr != NULL, return NULL);

    AreaInfo *area;

#ifdef DEEP_POINTER_CHECK

    if ((uintptr_t)ptr >= (uintptr_t)temp_pool.base
     && (uintptr_t)ptr < (uintptr_t)temp_pool.base + temp_pool.size) {
        area = (AreaInfo *)temp_pool.base;
    } else {
        area = (AreaInfo *)main_pool.base;
    }
    AreaInfo *prev = NULL;
    for (;;) {
        if (UNLIKELY(AREA_IS_FENCEPOST(area))) {
            return NULL;
        }
        if (!(area->free || area->base != ptr)) {
            break;
        }
        prev = area;
        area = NEXT_AREA(area);
    }
    if (UNLIKELY(prev != area->prev)) {
        DLOG("prev mismatch for %p (ptr %p): area=%p found=%p", area, ptr,
             area->prev, prev);
        return NULL;
    }

#else  // !DEEP_POINTER_CHECK

    /* We ensure that sizeof(AreaInfo) <= MEM_BLOCKSIZE and we don't allow
     * alignments greater than MEM_BLOCKSIZE, so the AreaInfo structure can
     * always be found at the first MEM_BLOCKSIZE-aligned address preceding
     * the given pointer. */
    area = (AreaInfo *)(((uintptr_t)ptr - 1) & -MEM_BLOCKSIZE);

# ifdef POINTER_CHECK
    if (UNLIKELY(area->magic != AREAINFO_MAGIC)) {
        DLOG("Bad magic at %p for ptr %p: %08X", area, ptr, area->magic);
        return NULL;
    }
    if (UNLIKELY(area->free)) {
        return NULL;
    }
    if (UNLIKELY(area->base != ptr)) {
        DLOG("ptr mismatch for %p: area=%p, ptr=%p", area, area->base, ptr);
    }
# endif  // POINTER_CHECK

#endif  // DEEP_POINTER_CHECK

    return area;
}

/*-----------------------------------------------------------------------*/

static inline AreaInfo *split_area(AreaInfo *area, int newblocks,
                                   SplitUseSelect which)
{
    PRECOND(area != NULL, return NULL);
    PRECOND(newblocks > 0, return NULL);
    PRECOND(newblocks < area->nblocks, return NULL);
    PRECOND(which == SPLIT_USE_FRONT || area->free, return NULL);

    const int oldblocks = area->nblocks;
    area->nblocks = newblocks;
    AreaInfo *newarea = NEXT_AREA(area);
    newarea->magic   = AREAINFO_MAGIC;
    newarea->temp    = area->temp;
    newarea->nblocks = oldblocks - newblocks;
    newarea->prev    = area;
    AreaInfo *next = NEXT_AREA(newarea);
    next->prev = newarea;

    if (which == SPLIT_USE_FRONT) {
        if (area->free) {
#ifdef FREE_LIST_CHECK
            /* newarea hasn't been added to the free list yet, so clear its
             * "free" flag so that the free list check doesn't report it as
             * an error. */
            newarea->free = 0;
#endif
            mark_used(area);
        }
        mark_free(newarea);
        merge_free(newarea);
        return area;
    } else {
        /* newarea hasn't been added to the free list, so we don't call
         * mark_used(). */
        newarea->free = 0;
        return newarea;
    }
}

/*-----------------------------------------------------------------------*/

static inline void merge_free(AreaInfo *area)
{
    PRECOND(area != NULL, return);
    PRECOND(area->free, return);

    AreaInfo *next = NEXT_AREA(area);
    /* The fencepost block is initialized with AreaInfo.free set to false,
     * so we don't need an explicit FENCEPOST check here. */
    if (next->free) {
        area->nblocks += next->nblocks;
        area->next_free = next->next_free;
        mem_clear(next, sizeof(*next));
        AreaInfo *next2 = NEXT_AREA(area);
        next2->prev = area;
        if (area->next_free) {
            area->next_free->prev_free = area;
        } else {
            MemoryPool *pool;
            if ((uintptr_t)area >= (uintptr_t)temp_pool.base
             && (uintptr_t)area < (uintptr_t)temp_pool.base + temp_pool.size)
            {
                pool = &temp_pool;
            } else {
                pool = &main_pool;
            }
            pool->last_free = area;
        }
    }
}

/*-----------------------------------------------------------------------*/

static inline void mark_used(AreaInfo *area)
{
    PRECOND(area != NULL, return);
    PRECOND(area->free, return);

    MemoryPool *pool = area->temp ? &temp_pool : &main_pool;

    area->free = 0;

    if (area->prev_free) {
        area->prev_free->next_free = area->next_free;
    } else {
        pool->first_free = area->next_free;
    }
    if (area->next_free) {
        area->next_free->prev_free = area->prev_free;
    } else {
        pool->last_free = area->prev_free;
    }

#ifdef DEBUG
    /* Set the free list pointers so that the program will crash if it
     * (erroneously) tries to use them. */
    area->prev_free = area->next_free = (AreaInfo *) -1;
#endif

#ifdef FREE_LIST_CHECK
    free_list_check();
#endif
}

/*-----------------------------------------------------------------------*/

static inline void mark_free(AreaInfo *area)
{
    PRECOND(area != NULL, return);

    MemoryPool *pool = area->temp ? &temp_pool : &main_pool;

    area->free = 1;

    if (!pool->first_free) {
        ASSERT(!pool->last_free, return);
        area->prev_free = NULL;
        area->next_free = NULL;
        pool->first_free = pool->last_free = area;

    } else if (area < pool->first_free) {
        area->prev_free = NULL;
        area->next_free = pool->first_free;
        pool->first_free->prev_free = area;
        pool->first_free = area;

    } else if (area > pool->last_free) {
        area->prev_free = pool->last_free;
        area->next_free = NULL;
        pool->last_free->next_free = area;
        pool->last_free = area;

    } else {
        AreaInfo *prev_free;
        for (prev_free = area->prev; prev_free; prev_free = prev_free->prev) {
            const int is_free = prev_free->free;
            if (is_free) {
                break;
            }
        }
        /* We already know that this isn't at the head or tail of the list. */
        ASSERT(prev_free != NULL, return);
        ASSERT(prev_free->next_free != NULL, return);
        area->prev_free = prev_free;
        area->next_free = prev_free->next_free;
        area->prev_free->next_free = area;
        area->next_free->prev_free = area;
    }

#ifdef FREE_LIST_CHECK
    free_list_check();
#endif
}

/*-----------------------------------------------------------------------*/

#ifdef FREE_LIST_CHECK

static NOINLINE void free_list_check(void)
{
    for (MemoryPool *pool = &main_pool; pool;
         pool = (pool==&main_pool ? &temp_pool : NULL))
    {

        if ((pool->first_free && !pool->last_free)
         || (!pool->first_free && pool->last_free)) {
            DLOG("%s pool (%p): NULLness of first_free(%p) and last_free(%p)"
                 " doesn't match", pool==&main_pool ? "Main" : "Temporary",
                 pool->base, pool->first_free, pool->last_free);
            for (;;) sys_time_delay(1000000);
        }
        if (pool->first_free) {
            if (pool->first_free->prev_free != NULL) {
                DLOG("%s pool (%p): first_free(%p)->prev_free != NULL",
                     pool==&main_pool ? "Main" : "Temporary", pool->base,
                     pool->first_free);
                for (;;) sys_time_delay(1000000);
            }
        }
        if (pool->last_free) {
            if (pool->last_free->next_free != NULL) {
                DLOG("%s pool (%p): last_free(%p)->next_free != NULL",
                     pool==&main_pool ? "Main" : "Temporary", pool->base,
                     pool->first_free);
                for (;;) sys_time_delay(1000000);
            }
        }

        AreaInfo *free_area = pool->first_free;
        for (AreaInfo *area = (AreaInfo *)pool->base, *next;
             area && free_area; area = next)
        {
            if (area < free_area) {
                if (area->free) {
                    DLOG("%s pool (%p): Free area %p is not on free list",
                         pool==&main_pool ? "Main" : "Temporary", pool->base,
                         area);
                    for (;;) sys_time_delay(1000000);
                }
            } else if (area == free_area) {
                if (!area->free) {
                    DLOG("%s pool (%p): In-use area %p is on free list",
                         pool==&main_pool ? "Main" : "Temporary", pool->base,
                         area);
                    for (;;) sys_time_delay(1000000);
                }
                if (area->next_free) {
                    if (area->next_free < area) {
                        DLOG("%s pool (%p): %p->next_free(%p) is out of order",
                             pool==&main_pool ? "Main" : "Temporary",
                             pool->base, area, area->next_free);
                        for (;;) sys_time_delay(1000000);
                    }
                    if (area->next_free->prev_free != area) {
                        DLOG("%s pool (%p): %p->next_free(%p)->prev_free(%p)"
                             " doesn't match",
                             pool==&main_pool ? "Main" : "Temporary",
                             pool->base, area, area->next_free,
                             area->next_free->prev_free);
                        for (;;) sys_time_delay(1000000);
                    }
                }
                free_area = area->next_free;
            } else {  // area > free_area
                DLOG("%s pool (%p): Free list entry %p is not a valid area",
                     pool==&main_pool ? "Main" : "Temporary", pool->base,
                     free_area);
                for (;;) sys_time_delay(1000000);
            }
            next = NEXT_AREA(area);
        }

        if (free_area) {
            DLOG("%s pool (%p): Free list contains area %p not in pool",
                 pool==&main_pool ? "Main" : "Temporary", pool->base,
                 free_area);
            for (;;) sys_time_delay(1000000);
        }

    }  // for (pool)
}

#endif  // FREE_LIST_CHECK

/*************************************************************************/
/*************************************************************************/
