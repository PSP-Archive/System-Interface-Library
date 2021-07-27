/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/malloc.c: Substitutes for malloc() and related functions
 * that use our custom memory allocator.
 */

#ifdef SIL_PLATFORM_PSP_REPLACE_MALLOC  // To the end of the file.

/*
 * C++'s STL likes to allocate insane numbers of tiny buffers, which gives
 * our large-block-based memory allocator (and its author) horrible
 * headaches.  To get around that, we watch for allocations of small blocks
 * and redirect them to a more traditional malloc()-style heap.  The heap
 * consists of only a header and linked lists of free blocks, with no
 * additional per-block management information so that overhead is
 * minimized.
 *
 * Blocks are allocated in units of 8 bytes, the size of a block header.
 * The heap header keeps one list of free blocks for each possible
 * allocation size, up to the MALLOC_SIZE_LIMIT limit; to allocate a
 * block, malloc() needs simply to take the first block from the list of
 * the requested size, shifting up to larger-size lists if necessary.
 *
 * When a block is freed, the allocator attempts to coalesce it with any
 * preceding or following free block.  The following block can be easily
 * found by adding the block size to its base address; if that block
 * header's alloc.magic field is _not_ equal to HEAP_BLOCK_MAGIC, the
 * block is free and can thus be merged.  (This works because alloc.magic
 * overlays the low 16 bits of free.size, and HEAP_BLOCK_MAGIC is chosen
 * to be unaligned with respect to the block size, so no free block will
 * have the value HEAP_BLOCK_MAGIC in the lower 16 bits of its size.)
 * With respect to the previous block, the allocator keeps a "previous
 * block free" bit in each block's header; if that bit is set, the header
 * of the previous block can then be found by accessing the pointer value
 * immediately preceding the current block in memory.  (This pointer is
 * part of an 8-byte footer stored at the end of each free block.)
 *
 * Since attempting to adjust the heap size may cause it to be moved,
 * which would destroy all heap-based pointers, we instead allocate a new
 * heap when our first heap is full.  In addition to the per-heap arrays
 * of free blocks for each size, we also keep a global array which
 * indicates the first heap containing a free block of each size, so we
 * do not need to search all of the heaps in order to find an empty block
 * to allocate.  If all blocks in a heap are freed, the heap itself is
 * freed as well.
 *
 * For simplicity (and speed), we don't handle realloc() block resizing at
 * all, instead redirecting any resized block to the primary allocator.
 * Experience has shown this not to be a problem, as STL tends to allocate
 * and free but not reallocate.
 *
 * Note that this implementation of malloc() is specifically targeted at
 * the PSP; in particular, it will only work as written on 32-bit
 * platforms, and it depends on the semantics of union and bitfield layout
 * provided by GCC.  It is not intended to be a general-purpose allocator.
 */

/*************************************************************************/
/************************* Configuration options *************************/
/*************************************************************************/

/**
 * MALLOC_BLOCK_SIZE:  Block size used for allocation.  This MUST be the
 * same as sizeof(HeapBlock), or very bad things will happen!
 */
#define MALLOC_BLOCK_SIZE  8

/**
 * MALLOC_SIZE_LIMIT:  Maximum size region to allocate from our local
 * heaps, in bytes.
 */
#define MALLOC_SIZE_LIMIT  1024

/**
 * MALLOC_HEAP_SIZE:  Default size of a single heap, in bytes.  If we can't
 * allocate a heap of this size, or if doing so would cause free memory to
 * drop by more than half, we try again with half the size, repeating until
 * we go below MALLOC_HEAP_MIN_SIZE.
 */
#define MALLOC_HEAP_SIZE  1048576

/**
 * MALLOC_HEAP_MIN_SIZE:  Minimum size of a single heap, in bytes.
 */
#define MALLOC_HEAP_MIN_SIZE  4096

/*-------- Debugging options --------*/

/**
 * VERIFY_FREE_LISTS:  Define this to verify the integrity of the heap free
 * lists after every heap allocation or free.  This will naturally slow
 * down memory allocation.
 */
// #define VERIFY_FREE_LISTS

/**
 * VERIFY_FREE_LISTS_ALL_HEAPS:  Define this along with VERIFY_FREE_LISTS
 * to verify the integrity of _every_ heap's free list, not just the one in
 * which the allocation or free took place.  This can catch some invalid
 * writes after the fact, at a significant speed cost.
 */
// #define VERIFY_FREE_LISTS_ALL_HEAPS

/**
 * TRACE_ALLOCS:  Define this to log every heap allocation and free with
 * DLOG().  Only functions when DEBUG is defined.
 */
// #define TRACE_ALLOCS

/*************************************************************************/
/*************** Internal data structures and declarations ***************/
/*************************************************************************/

/* Include common headers. */

#include "src/base.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/psp/internal.h"

/* If SIL_MEMORY_FORBID_MALLOC is enabled, these names will be defined to
 * nonexistent dummy names; if it's disabled but debugging mode is enabled,
 * they'll be defined to macros that call the debugging versions of the
 * functions.  Either way, we need to cancel the redefinitions so we can
 * define the actual functions here. */
#undef malloc
#undef calloc
#undef realloc
#undef free

#ifdef DEBUG
/* These are needed for debug info display. */
# include "src/debug.h"
# include "src/graphics.h"
# include "src/math.h"
# include "src/sysdep/psp/ge-util.h"
#endif

/*************************************************************************/

typedef struct MallocHeap_ MallocHeap;
typedef union HeapBlock_ HeapBlock;

/* Heap header structure. */
struct MallocHeap_ {
    MallocHeap *next, *prev;  // Heap list pointers
    uintptr_t heap_base;      // Base address of free space in this heap
    uint32_t heap_size;       // Total data space in this heap
    uint32_t free_bytes;      // Bytes free in this heap
    uint32_t free_blocks;     // Number of free blocks in this heap

    /* First free block of each block-aligned allocation size up to the
     * allocation limit.  The last list also holds all free blocks larger
     * than the allocation limit. */
    HeapBlock *first_free[(MALLOC_SIZE_LIMIT + (MALLOC_BLOCK_SIZE-1))
                          / MALLOC_BLOCK_SIZE];
};

/* First heap in the heap list. */
static MallocHeap *first_heap;

/* Block header structure.  If the "pfree" bit (which can be safely read
 * or written using either the .free or the .alloc structure, regardless
 * of the block's status) is set, a pointer to the HeapBlock structure of
 * the preceding block can be found in the pointer value immediately
 * preceding the block, i.e. by accessing ((HeapBlock **)block)[-1].
 * (This method is adapted from Doug Lea's malloc() implementation.) */
union HeapBlock_ {
    struct {  // If free
        HeapBlock *next;  // Next free block
        uint32_t size:31; // Size of this free block (excluding the header)
        uint32_t pfree:1; // 1 if the previous block is free
    } free;
    struct {  // If allocated
        MallocHeap *heap; // Heap to which this block belongs
        uint16_t magic;   // Magic value identifying this as a heap block
        uint16_t size:15; // Size of this block (excluding the header)
        uint16_t pfree:1; // 1 if the previous block is free
    } alloc;
};

/* Magic value stored in HeapBlock.alloc.magic, used (1) to identify a
 * block as being allocated via malloc() as opposed to the primary
 * allocator, and (2) to identify a heap block as being in use rather than
 * free.  The value should therefore be chosen (1) so that it will never
 * be found in the second halfword before a block allocated from the
 * primary allocator (the allocator will still function correctly if this
 * condition is not satisfied, but free operations may take more time),
 * and (2) so that it is not a multiple of MALLOC_BLOCK_SIZE (this is
 * critical to correct functioning). */
#define HEAP_BLOCK_MAGIC  0xFADE

/* Block footer structure, used only for free blocks. */
typedef struct BlockFooter_ {
    HeapBlock *prev;      // Previous free block
    HeapBlock *this;      // Self pointer (MUST be the last structure field)
} BlockFooter;

/* Inline function to return the BlockFooter pointer for a properly
 * initialized free block. */
static BlockFooter *GET_BLOCK_FOOTER(const HeapBlock *freeblock) {
    return &((BlockFooter *)((uintptr_t)freeblock + sizeof(*freeblock)
                             + freeblock->free.size))[-1];
}

/*-----------------------------------------------------------------------*/

/* Global free block pointers, pointing to the first free block (if any) of
 * each size in any available heap.  This saves us the potential time waste
 * of scanning through multiple nearly-full heaps for a large block size.
 * The blocks listed here are always the same as those listed in the
 * relevant heap's free list table; that is, the following holds for all i:
 *     first_free[i].block == first_free[i].heap->first_free[i];
 * Additionally, the block listed is always from the first heap in the heap
 * list (which is maintained in address order, therefore the heap with the
 * lowest address) which has any blocks available in that size. */

static struct {
    HeapBlock *block;  // The block itself
    MallocHeap *heap;  // The heap it belongs to
} first_free[lenof(((MallocHeap *)0)->first_free)];

/*-----------------------------------------------------------------------*/

/* Hack to initialize the primary allocator's memory pools before program
 * startup.  See the documentation on SIL_PLATFORM_PSP_CXX_CONSTRUCTOR_HACK
 * in configs/psp.h. */

#ifdef SIL_PLATFORM_PSP_CXX_CONSTRUCTOR_HACK
static uint8_t initialized;
# define CHECK_INIT()  do {      \
    if (!initialized) {          \
        if (!psp_mem_init()) {   \
            sceKernelExitGame(); \
        }                        \
        initialized = 1;         \
    }                            \
} while (0)
#else
# define CHECK_INIT()  /*nothing*/
#endif

/*-----------------------------------------------------------------------*/

/* Since we have no way to retrieve source and line information from the
 * caller if it's outside our source tree (and thus our debugging macros),
 * we instead use the return address of the current function as the caller
 * name in such cases.  (In C++, of course, this will almost always be the
 * "new" operator implementation; but still, it's better than nothing.)
 * On GCC, we can use __builtin_return_address() to retrieve the return
 * address; other compilers may not have this, so we define
 * __builtin_return_address() as a macro that returns NULL in order to
 * avoid littering our code with #ifdefs. */

#ifndef __GNUC__
# define __builtin_return_address(level)  NULL
#endif

/*-----------------------------------------------------------------------*/

/* malloc() and free() helper functions. */

/*-------- Heap block management --------*/

/**
 * alloc_from_heap:  Allocate a new memory region from the malloc() heap.
 *
 * [Parameters]
 *     size: Size of memory to allocate, in bytes.
 * [Return value]
 *     Pointer to allocated memory, or NULL on error.
 */
static void *alloc_from_heap(size_t size);

/**
 * is_heap_block:  Return whether the given memory region was allocated
 * from the malloc() heap.
 *
 * [Parameters]
 *     ptr: Pointer to memory region to check.
 * [Return value]
 *     True if the memory region was allocated from the heap, false if not.
 */
static PURE_FUNCTION int is_heap_block(const void *ptr);

/**
 * free_from_heap:  Free the given memory region from the malloc() heap.
 *
 * [Parameters]
 *     ptr: Pointer to memory region to free.
 */
static void free_from_heap(void *ptr);

/*-------- Heap creation/deletion --------*/

/**
 * create_heap:  Create a new, empty heap, and return a pointer to the heap
 * and the solitary empty block created inside it.  The block is _not_
 * added to the heap's free list.
 *
 * [Parameters]
 *     block_ret: Pointer to variable to receive the free block pointer.
 * [Return value]
 *     Pointer to created heap, or NULL on error.
 */
static MallocHeap *create_heap(HeapBlock **block_ret);

/**
 * delete_heap:  Delete the given heap, which is assumed to be empty.
 *
 * [Parameters]
 *     heap: Heap to delete.
 */
static void delete_heap(MallocHeap *heap);

/*-------- Free list management --------*/

/**
 * add_to_free_list:  Add a block to a heap's free list.  The block header
 * does not need to be initialized.
 *
 * [Parameters]
 *     heap: Heap containing block.
 *     block: Block to add to free list.
 *     size: Size of block (excluding header).
 */
static void add_to_free_list(MallocHeap *heap, HeapBlock *block,
                             uint32_t size);

/**
 * remove_from_free_list:  Remove a block from a heap's free list.  The
 * block's header is not altered.
 *
 * [Parameters]
 *     heap: Heap containing block.
 *     block: Block to remove from free list.
 *     index: heap->first_free[] index for block size (passed in so
 *         precomputed values can be reused.
 *     is_first: True if this is known to be the first block in the global
 *         free list, otherwise false.  This is an optimization hint only
 *         and does not affect behavior.
 */
static inline void remove_from_free_list(MallocHeap *heap, HeapBlock *block,
                                         unsigned int index, int is_first);

/**
 * update_global_first_free:  Set the global free list pointer for the
 * given index.  The free list entry is assumed to already be pointing to
 * the proper heap, i.e. first_free[index].heap == heap.  If block == NULL,
 * the function searches subsequent heaps for a free block of the proper
 * size.
 *
 * [Parameters]
 *     index: Global free list index to update.
 *     heap: Heap containing block to update.
 *     block: Block to store in free list, or NULL to search other heaps.
 */
static inline void update_global_first_free(
    unsigned int index, MallocHeap *heap, HeapBlock *block);

#ifdef VERIFY_FREE_LISTS

/**
 * verify_free_lists:  Verify that all free lists are valid (containing
 * only properly-aligned blocks entirely contained within the respective
 * heap) and that no free list entries overlap the given allocated block
 * (whose HeapBlock.alloc structure must be properly initialized).  If a
 * problem is found, the function logs an error message via DLOG() and goes
 * into an infinite delay loop.
 *
 * [Parameters]
 *     allocated_block: Pointer to an allocated heap block to check.
 *     line: Source code line from which this function was called.
 */
static void verify_free_lists(HeapBlock *allocated_block, int line);

#endif

/*************************************************************************/
/************************** Exported functions ***************************/
/*************************************************************************/

void *malloc(size_t size)
{
    CHECK_INIT();
    if (size < MALLOC_SIZE_LIMIT) {
        if (UNLIKELY(size == 0)) {
            return NULL;
        }
        void *ptr = alloc_from_heap(size);
        if (ptr) {
#ifdef TRACE_ALLOCS
            DLOG("[%p:0] malloc(%u) -> %p (block size %u)",
                 __builtin_return_address(0), size, ptr,
                 ((HeapBlock *)ptr)[-1].alloc.size);
#endif
            return ptr;
        }
    }
#ifdef DEBUG
    char ra[11];
    strformat(ra, sizeof(ra), "%p", __builtin_return_address(0));
#endif
    return debug_mem_alloc(size, 0, 0, ra, 0, MEM_INFO_UNKNOWN);
}

/*----------------------------------*/

/* Also define the newlib reentrant versions. */
void *_malloc_r(UNUSED struct _reent *reent_ptr, size_t size)
{
    return malloc(size);
}

/*----------------------------------*/

#if defined(DEBUG) && !defined(SIL_MEMORY_FORBID_MALLOC)

/* If we're debugging, include a version that takes source file and line
 * number, so we can pass them to the primary allocator or log them when
 * tracing allocations. */
void *debug_malloc(size_t size, const char *file, int line)
{
    CHECK_INIT();
    if (size < MALLOC_SIZE_LIMIT) {
        if (UNLIKELY(size == 0)) {
            return NULL;
        }
        void *ptr = alloc_from_heap(size);
        if (ptr) {
# ifdef TRACE_ALLOCS
            DLOG("[%s:%d] malloc(%u) -> %p (block size %u)",
                 file, line, size, ptr,
                 ((HeapBlock *)ptr)[-1].alloc.size);
# endif
            return ptr;
        }
    }
    return debug_mem_alloc(size, 0, 0, file, line, MEM_INFO_UNKNOWN);
}

#endif  // DEBUG && !SIL_MEMORY_FORBID_MALLOC

/*-----------------------------------------------------------------------*/

void *calloc(size_t nmemb, size_t size)
{
    CHECK_INIT();
    if (nmemb * size < MALLOC_SIZE_LIMIT) {
        if (UNLIKELY(nmemb * size == 0)) {
            return NULL;
        }
        void *ptr = alloc_from_heap(nmemb * size);
        if (ptr) {
#ifdef TRACE_ALLOCS
            DLOG("[%p:0] calloc(%u,%u) -> %p (block size %u)",
                 __builtin_return_address(0), nmemb, size, ptr,
                 ((HeapBlock *)ptr)[-1].alloc.size);
#endif
            mem_clear(ptr, nmemb * size);
            return ptr;
        }
    }
#ifdef DEBUG
    char ra[11];
    strformat(ra, sizeof(ra), "%p", __builtin_return_address(0));
#endif
    return debug_mem_alloc(nmemb * size, 0, MEM_ALLOC_CLEAR,
                           ra, 0, MEM_INFO_UNKNOWN);
}

/*----------------------------------*/

void *_calloc_r(UNUSED struct _reent *reent_ptr, size_t nmemb, size_t size)
{
    return calloc(nmemb, size);
}

/*----------------------------------*/

#if defined(DEBUG) && !defined(SIL_MEMORY_FORBID_MALLOC)

void *debug_calloc(size_t nmemb, size_t size, const char *file, int line)
{
    CHECK_INIT();
    if (nmemb * size < MALLOC_SIZE_LIMIT) {
        if (UNLIKELY(nmemb * size == 0)) {
            return NULL;
        }
        void *ptr = alloc_from_heap(nmemb * size);
        if (ptr) {
# ifdef TRACE_ALLOCS
            DLOG("[%s:%d] calloc(%u,%u) -> %p (block size %u)",
                 file, line, nmemb, size, ptr,
                 ((HeapBlock *)ptr)[-1].alloc.size);
# endif
            mem_clear(ptr, nmemb * size);
            return ptr;
        }
    }
    return debug_mem_alloc(nmemb * size, 0, MEM_ALLOC_CLEAR,
                           file, line, MEM_INFO_UNKNOWN);
}

#endif  // DEBUG && !SIL_MEMORY_FORBID_MALLOC

/*-----------------------------------------------------------------------*/

void *realloc(void *ptr, size_t size)
{
    CHECK_INIT();
    if (ptr == NULL && size < MALLOC_SIZE_LIMIT) {
        if (UNLIKELY(size == 0)) {
            return NULL;
        }
        ptr = alloc_from_heap(size);
        if (ptr) {
#ifdef TRACE_ALLOCS
            DLOG("[%p:0] realloc(0x0,%u) -> %p (block size %u)",
                 __builtin_return_address(0), size, ptr,
                 ((HeapBlock *)ptr)[-1].alloc.size);
#endif
            return ptr;
        }
    } else if (ptr != NULL && is_heap_block(ptr)) {
#ifdef TRACE_ALLOCS
        DLOG("[%p:0] realloc(%p,%u) -> free %p",
             __builtin_return_address(0), ptr, size, ptr);
#endif
        if (size == 0) {
            free_from_heap(ptr);
            return NULL;
        } else {
            HeapBlock *block = (HeapBlock *)ptr - 1;
#ifdef DEBUG
            char ra[11];
            strformat(ra, sizeof(ra), "%p", __builtin_return_address(0));
#endif
            void *newptr = debug_mem_alloc(size, 0, 0,
                                           ra, 0, MEM_INFO_UNKNOWN);
            if (!newptr) {
                return NULL;
            }
            memcpy(newptr, ptr, min(size, block->alloc.size));
            free_from_heap(ptr);
            return newptr;
        }
    }
#ifdef DEBUG
    char ra[11];
    strformat(ra, sizeof(ra), "%p", __builtin_return_address(0));
#endif
    return debug_mem_realloc(ptr, size, 0, ra, 0, MEM_INFO_UNKNOWN);
}

/*----------------------------------*/

void *_realloc_r(UNUSED struct _reent *reent_ptr, void *ptr, size_t size)
{
    return realloc(ptr, size);
}

/*----------------------------------*/

#if defined(DEBUG) && !defined(SIL_MEMORY_FORBID_MALLOC)

void *debug_realloc(void *ptr, size_t size, const char *file, int line)
{
    CHECK_INIT();
    if (ptr == NULL && size < MALLOC_SIZE_LIMIT) {
        if (UNLIKELY(size == 0)) {
            return NULL;
        }
        ptr = alloc_from_heap(size);
        if (ptr) {
# ifdef TRACE_ALLOCS
            DLOG("[%s:%d] realloc(0x0,%u) -> %p (block size %u)",
                 file, line, size, ptr,
                 ((HeapBlock *)ptr)[-1].alloc.size);
# endif
            return ptr;
        }
    } else if (ptr != NULL && is_heap_block(ptr)) {
# ifdef TRACE_ALLOCS
        DLOG("[%s:%d] realloc(%p,%u) -> free %p",
             file, line, ptr, size, ptr);
# endif
        if (size == 0) {
            free_from_heap(ptr);
            return NULL;
        } else {
            HeapBlock *block = (HeapBlock *)ptr - 1;
            void *newptr = debug_mem_alloc(size, 0, 0,
                                           file, line, MEM_INFO_UNKNOWN);
            if (!newptr) {
                return NULL;
            }
            memcpy(newptr, ptr, min(size, block->alloc.size));
            free_from_heap(ptr);
            return newptr;
        }
    }
    return debug_mem_realloc(ptr, size, 0, file, line, MEM_INFO_UNKNOWN);
}

#endif  // DEBUG && !SIL_MEMORY_FORBID_MALLOC

/*-----------------------------------------------------------------------*/

void free(void *ptr)
{
    if (ptr && is_heap_block(ptr)) {
#ifdef TRACE_ALLOCS
        DLOG("[%p:0] free(%p)", __builtin_return_address(0), ptr);
#endif
        free_from_heap(ptr);
    } else {
#ifdef DEBUG
        char ra[11];
        strformat(ra, sizeof(ra), "%p", __builtin_return_address(0));
#endif
        debug_mem_free(ptr, ra, 0);
    }
}

/*----------------------------------*/

void _free_r(UNUSED struct _reent *reent_ptr, void *ptr)
{
    return free(ptr);
}

/*----------------------------------*/

#if defined(DEBUG) && !defined(SIL_MEMORY_FORBID_MALLOC)

void debug_free(void *ptr, const char *file, int line)
{
    if (ptr && is_heap_block(ptr)) {
# ifdef TRACE_ALLOCS
        DLOG("[%s:%d] free(%p)", file, line, ptr);
# endif
        free_from_heap(ptr);
    } else {
        debug_mem_free(ptr, file, line);
    }
}

#endif  // DEBUG && !SIL_MEMORY_FORBID_MALLOC

/*************************************************************************/
/******************** PSP-specific interface routines ********************/
/*************************************************************************/

#ifdef DEBUG

void malloc_display_debuginfo(void)
{
    const Vector4f bg_color = {0, 0, 0, 0.5};
    const Vector4f text_color = {1, 1, 1, 1};
    const int lineheight = debug_text_height();
    int y = lineheight;

    debug_fill_box(0, y, graphics_display_width(), lineheight, &bg_color);
    debug_draw_text(0, y, 1, &text_color, "malloc() heaps:");
    y += lineheight;

    for (MallocHeap *heap = first_heap; heap;
         heap = heap->next, y += lineheight)
    {
        debug_fill_box(0, y, graphics_display_width(), lineheight, &bg_color);
        debug_draw_text(20, y, 1, &text_color,
                        "%7X (size %7d): %7d free in %6d blocks (average %7d)",
                        (int)heap, heap->heap_size, heap->free_bytes,
                        heap->free_blocks,
                        heap->free_blocks > 0
                            ? heap->free_bytes / heap->free_blocks : 0);
    }
}

#endif  // DEBUG

/*************************************************************************/
/***************** Local routines: Heap block management *****************/
/*************************************************************************/

static void *alloc_from_heap(size_t size)
{
    PRECOND(sizeof(HeapBlock) == MALLOC_BLOCK_SIZE, return NULL);

    uint32_t aligned_size = align_up(size, sizeof(HeapBlock));

    /* Look for a free block large enough to fulfill the request. */

    MallocHeap *heap = NULL;
    HeapBlock *block = NULL;
    for (unsigned int index = (aligned_size / MALLOC_BLOCK_SIZE) - 1;
         index < lenof(heap->first_free);
         index++)
    {
        if (first_free[index].block) {
            block = first_free[index].block;
            heap = first_free[index].heap;
            remove_from_free_list(heap, block, index, 1);
            break;
        }
    }

    /* If we couldn't find a free block of sufficient size, allocate a
     * new heap and use its (solitary) free block. */

    if (!heap) {
        HeapBlock *new_block;  // Separate from "block" for efficiency reasons.
        heap = create_heap(&new_block);
        if (!heap) {
            return NULL;
        }
        block = new_block;
    }

    /* Split the free block if necessary and return it. */

    const uint32_t leftover_size = block->free.size - aligned_size;
    HeapBlock *next;
    if (leftover_size > sizeof(HeapBlock)) {
        next = (HeapBlock *)((uintptr_t)(block+1) + aligned_size);
        add_to_free_list(heap, next, leftover_size - sizeof(HeapBlock));
    } else {
        heap->free_blocks--;
        aligned_size = block->free.size;
        next = (HeapBlock *)((uintptr_t)(block+1) + aligned_size);
    }
    next->free.pfree = 0;  // Always safe.
    heap->free_bytes -= sizeof(HeapBlock) + aligned_size;
    block->alloc.heap = heap;
    block->alloc.magic = HEAP_BLOCK_MAGIC;
    block->alloc.size = aligned_size;
#ifdef VERIFY_FREE_LISTS
    verify_free_lists(block, __LINE__);
#endif
    return block + 1;
}

/*-----------------------------------------------------------------------*/

static PURE_FUNCTION int is_heap_block(const void *ptr)
{
    PRECOND(sizeof(HeapBlock) == MALLOC_BLOCK_SIZE, return 0);

    if (((uintptr_t)ptr % MALLOC_BLOCK_SIZE) != 0) {
        return 0;  // Alignment is wrong, so it can't be a heap block.
    }
    const HeapBlock *block = (const HeapBlock *)ptr - 1;
    if ((uintptr_t)ptr < (uintptr_t)block->alloc.heap
     || (uintptr_t)ptr >= (uintptr_t)block->alloc.heap + MALLOC_HEAP_SIZE) {
        /* It's out of range of the heap, so it can't be a heap block. */
    }
    if (block->alloc.magic != HEAP_BLOCK_MAGIC) {
        return 0;
    }

    /* The magic value only tells us that it's _probably_ a heap block.
     * Check the heap pointer against our list to make sure. */

    for (MallocHeap *heap = first_heap; heap; heap = heap->next) {
        if (heap == block->alloc.heap) {
            return 1;
        }
    }
    return 0;
}

/*-----------------------------------------------------------------------*/

static void free_from_heap(void *ptr)
{
    PRECOND(sizeof(HeapBlock) == MALLOC_BLOCK_SIZE, return);

    HeapBlock *block = (HeapBlock *)ptr - 1;
#ifdef VERIFY_FREE_LISTS
    verify_free_lists(block, __LINE__);
#endif
    MallocHeap * const heap = block->alloc.heap;
    uint32_t size = block->alloc.size;

    /* Update heap accounting information. */

    heap->free_bytes += sizeof(HeapBlock) + size;

    /* If the entire heap is now empty, free it. */

    if (heap->free_bytes >= heap->heap_size) {
        delete_heap(heap);
        return;
    }
    /* Can we coalesce this block with the immediately following one? */

    HeapBlock *next = (HeapBlock *)((uintptr_t)(block+1) + size);
    if (next->alloc.magic == HEAP_BLOCK_MAGIC) {
        /* The next block is in use, so we can't coalesce.  Instead, set
         * the next block's pfree flag. */

        next->alloc.pfree = 1;

    } else {
        /* The next block is free, so join the blocks together.  Note
         * that we don't add the current block to the free list yet; that
         * happens below. */

        size += sizeof(HeapBlock) + next->free.size;
        const unsigned int index =
            ubound((next->free.size / MALLOC_BLOCK_SIZE) - 1,
                   lenof(heap->first_free) - 1);
        remove_from_free_list(heap, next, index, 0);
        heap->free_blocks--;

    }

    /* Can we coalesce this block with the immediately preceding one? */

    if (block->free.pfree) {

        HeapBlock *prev = ((HeapBlock **)block)[-1];
        const unsigned int index =
            ubound((prev->free.size / MALLOC_BLOCK_SIZE) - 1,
                   lenof(heap->first_free) - 1);
        remove_from_free_list(heap, prev, index, 0);
        add_to_free_list(heap, prev,
                         prev->free.size + sizeof(HeapBlock) + size);

    } else {
        /* We can't coalesce, so insert the new free block at the beginning
         * of the appropriate list for its size. */

        add_to_free_list(heap, block, size);
        heap->free_blocks++;

    }
}

/*************************************************************************/
/**************** Local routines: Heap creation/deletion *****************/
/*************************************************************************/

static MallocHeap *create_heap(HeapBlock **block_ret)
{
    PRECOND(block_ret != NULL, return NULL);

    MallocHeap *heap = NULL;

    /* Allocate space for the heap from the primary allocator. */

    uint32_t heap_size = MALLOC_HEAP_SIZE;
    do {
        if (mem_avail(0) >= heap_size*2) {  // Leave some for other people.
            heap = mem_alloc(heap_size, MALLOC_BLOCK_SIZE, 0);
        }
    } while (!heap && (heap_size /= 2) >= MALLOC_HEAP_MIN_SIZE);
    if (!heap) {
        return NULL;
    }
    mem_clear(heap, sizeof(*heap));
#ifdef TRACE_ALLOCS
    DLOG("NEW HEAP at %p, total size %u", heap, heap_size);
#endif

    /* Link the new heap into the heap list, maintaining the list in
     * memory address order. */

    MallocHeap *prev_heap = NULL, *next_heap = first_heap;
    while (next_heap && next_heap < heap) {
        prev_heap = next_heap;
        next_heap = next_heap->next;
    }
    heap->prev = prev_heap;
    heap->next = next_heap;
    if (next_heap) {
        next_heap->prev = heap;
    }
    if (prev_heap) {
        prev_heap->next = heap;
    } else {
        first_heap = heap;
    }

    /* Set up the heap header, allocating all empty space in the heap to a
     * single free block.  We do _not_ add the block to the free list, to
     * save our caller (alloc_from_heap()) from having to remove it again. */

    const uint32_t heap_header_size =
        align_up(sizeof(MallocHeap), sizeof(HeapBlock));
    heap->heap_base = (uintptr_t)heap + heap_header_size;
    /* We subtract an extra HeapBlock here so that the allocation code does
     * not have to check for end-of-heap before modifying the following
     * block's "pfree" flag.  The last sizeof(HeapBlock) bytes are never
     * actually read (they can be considered additional heap overhead). */
    heap->heap_size = heap_size - heap_header_size - sizeof(HeapBlock);
    heap->free_bytes = heap->heap_size;
    heap->free_blocks = 1;
    for (int index = 0; index < lenof(heap->first_free); index++) {
        heap->first_free[index] = NULL;
    }

    HeapBlock *block = (HeapBlock *)heap->heap_base;
    block->free.next = NULL;
    block->free.size = heap->heap_size - sizeof(*block);
    block->free.pfree = 0;
    BlockFooter *footer = GET_BLOCK_FOOTER(block);
    footer->prev = NULL;
    footer->this = block;

    HeapBlock *dummy = (HeapBlock *)(heap->heap_base + heap->heap_size);
    dummy->alloc.heap = heap;
    /* Make sure the dummy block is considered in-use, so we don't try to
     * coalesce it in free(). */
    dummy->alloc.magic = HEAP_BLOCK_MAGIC;
    dummy->alloc.size = 0;
    dummy->alloc.pfree = 0;

    /* Return the newly-created heap and its block. */

    *block_ret = block;
    return heap;
}

/*-----------------------------------------------------------------------*/

static void delete_heap(MallocHeap *heap)
{
    PRECOND(heap != NULL, return);

    for (int index = 0; index < lenof(first_free); index++) {
        if (first_free[index].block && first_free[index].heap == heap) {
            update_global_first_free(index, heap, NULL);
        }
    }

    if (heap->prev) {
        heap->prev->next = heap->next;
    } else {
        first_heap = heap->next;
    }
    if (heap->next) {
        heap->next->prev = heap->prev;
    }

    mem_free(heap);
}

/*************************************************************************/
/***************** Local routines: Free list management ******************/
/*************************************************************************/

static void add_to_free_list(MallocHeap *heap, HeapBlock *block,
                             uint32_t size)
{
    /* Set up the block's header and footer, and link it into the proper
     * free list for the block's size. */

    block->free.size = size;
    const unsigned index = ubound((block->free.size / MALLOC_BLOCK_SIZE) - 1,
                                  lenof(heap->first_free) - 1);
    block->free.next = heap->first_free[index];
    if (heap->first_free[index]) {
        BlockFooter *footer = GET_BLOCK_FOOTER(heap->first_free[index]);
        footer->prev = block;
    }
    heap->first_free[index] = block;
    BlockFooter *footer = GET_BLOCK_FOOTER(block);
    footer->prev = NULL;
    footer->this = block;

    /* Update the global free list as well if (1) there is no free block
     * of this size or (2) the free block listed belongs to this or a
     * later heap. */

    if (!first_free[index].block || heap <= first_free[index].heap) {
        first_free[index].block = block;
        first_free[index].heap = heap;
    }
}

/*-----------------------------------------------------------------------*/

static inline void remove_from_free_list(MallocHeap *heap, HeapBlock *block,
                                         unsigned int index, int is_first)
{
    /* Optimize the case of removing the first block, as alloc_from_heap()
     * does. */

#ifdef __GNUC__
    if (__builtin_constant_p(is_first) && is_first) {
        HeapBlock *next = block->free.next;
        if (next) {
            BlockFooter *next_footer = GET_BLOCK_FOOTER(next);
            next_footer->prev = NULL;
        }
        heap->first_free[index] = next;
        update_global_first_free(index, heap, next);
        return;
    }
#endif

    HeapBlock *next = block->free.next;
    BlockFooter *footer = GET_BLOCK_FOOTER(block);
    HeapBlock *prev = footer->prev;
    if (next) {
        BlockFooter *next_footer = GET_BLOCK_FOOTER(next);
        next_footer->prev = prev;
    }
    if (prev) {
        prev->free.next = next;
    } else {
        heap->first_free[index] = next;
        if (first_free[index].heap == heap) {
            update_global_first_free(index, heap, next);
        }
    }
}

/*-----------------------------------------------------------------------*/

static inline void update_global_first_free(
    unsigned int index, MallocHeap *heap, HeapBlock *block)
{
    first_free[index].block = block;
    if (!block) {
        /* Check subsequent heaps for a free block of this size. */
        for (MallocHeap *heap_iter = heap->next; heap_iter;
             heap_iter = heap_iter->next)
        {
            if (heap_iter->first_free[index]) {
                first_free[index].block = heap_iter->first_free[index];
                first_free[index].heap = heap_iter;
                break;
            }
        }
    }
}

/*-----------------------------------------------------------------------*/

#ifdef VERIFY_FREE_LISTS

static void verify_free_lists(HeapBlock *allocated_block, int line)
{
    const uintptr_t alloc_block_base = (uintptr_t)allocated_block;
    const uintptr_t alloc_block_top =
        alloc_block_base + MALLOC_BLOCK_SIZE + allocated_block->alloc.size;

# ifdef VERIFY_FREE_LISTS_ALL_HEAPS
    for (MallocHeap *heap = first_heap; heap; heap = heap->next)
# else
    MallocHeap *heap = allocated_block->alloc.heap;
# endif
    {

        for (int index = 0; index < lenof(heap->first_free); index++) {

            if (first_free[index].heap == heap
             && first_free[index].block != heap->first_free[index]) {
                DLOG("*** VERIFY ERROR *** (called from %s:%d)\n"
                     "    Heap %p, index %u: First free block %p doesn't"
                     " match global first block %p", __FILE__, line, heap,
                     index, heap->first_free[index], first_free[index].block);
                goto fail;
            }

            if (heap->first_free[index]) {
                if (!first_free[index].block) {
                    DLOG("*** VERIFY ERROR *** (called from %s:%d)\n"
                         "    Heap %p, index %u: Free block %p available"
                         " but global first block is NULL", __FILE__, line,
                         heap, index, heap->first_free[index]);
                    goto fail;
                } else if (first_free[index].heap > heap) {
                    DLOG("*** VERIFY ERROR *** (called from %s:%d)\n"
                         "    Heap %p, index %u: Free block %p available"
                         " but global first block is in later heap %p",
                         __FILE__, line, heap, index, heap->first_free[index],
                         first_free[index].heap);
                    goto fail;
                }
            }

            for (HeapBlock *block = heap->first_free[index], *prev = NULL;
                 block != NULL;
                 prev = block, block = block->free.next)
            {

                if ((uintptr_t)block % MALLOC_BLOCK_SIZE != 0) {
                    DLOG("*** VERIFY ERROR *** (called from %s:%d)\n"
                         "    Heap %p, index %u: Block %p is misaligned",
                         __FILE__, line, heap, index, block);
                    goto fail;
                }

                if (block->alloc.magic == HEAP_BLOCK_MAGIC) {
                    DLOG("*** VERIFY ERROR *** (called from %s:%d)\n"
                         "    Heap %p, index %u: Block %p (size %u) is on the"
                         " free list but is in use", __FILE__, line, heap,
                         index, block, block->alloc.size);
                    goto fail;
                }

                if (block->free.size == 0) {
                    DLOG("*** VERIFY ERROR *** (called from %s:%d)\n"
                         "    Heap %p, index %u: Block %p has free size 0",
                         __FILE__, line, heap, index, block);
                    goto fail;
                }

                if (block->free.size % MALLOC_BLOCK_SIZE != 0) {
                    DLOG("*** VERIFY ERROR *** (called from %s:%d)\n"
                         "    Heap %p, index %u: Block %p's free size %u"
                         " is misaligned", __FILE__, line, heap, index, block,
                         block->free.size);
                    goto fail;
                }

                if ((uintptr_t)block < heap->heap_base
                 || (uintptr_t)block + MALLOC_BLOCK_SIZE + block->free.size
                        > heap->heap_base + heap->heap_size) {
                    DLOG("*** VERIFY ERROR *** (called from %s:%d)\n"
                         "    Heap %p, index %u: Block %p (size %u) is not"
                         " within heap", __FILE__, line, heap, index, block,
                         block->free.size);
                    goto fail;
                }

                if ((uintptr_t)block < alloc_block_top
                 && ((uintptr_t)block + MALLOC_BLOCK_SIZE + block->free.size
                     > alloc_block_base))
                {
                    DLOG("*** VERIFY ERROR *** (called from %s:%d)\n"
                         "    Heap %p, index %u: Block %p (size %u) overlaps"
                         " allocated block %p (size %u)", __FILE__, line,
                         heap, index, block, block->free.size,
                         allocated_block, allocated_block->alloc.size);
                    goto fail;
                }

                BlockFooter *footer = GET_BLOCK_FOOTER(block);

                if (footer->prev != prev) {
                    DLOG("*** VERIFY ERROR *** (called from %s:%d)\n"
                         "    Heap %p, index %u: Block %p (size %u) previous"
                         " pointer %p does not match actual previous block %p",
                         __FILE__, line, heap, index, block, block->free.size,
                         footer->prev, prev);
                    goto fail;
                }

                if (footer->this != block) {
                    DLOG("*** VERIFY ERROR *** (called from %s:%d)\n"
                         "    Heap %p, index %u: Block %p (size %u) self"
                         " pointer %p does not match actual block pointer",
                         __FILE__, line, heap, index, block, block->free.size,
                         footer->this);
                    goto fail;
                }

            }  // for (block)

        }  // for (index)

    }

    return;

  fail:
    for (;;) {
        sys_time_delay(1000000);
    }
}

#endif  // VERIFY_FREE_LISTS

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_PLATFORM_PSP_REPLACE_MALLOC
