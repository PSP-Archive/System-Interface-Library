/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/memory.c: Extended memory management functions.
 */

/* We define this to ensure that sys_mem_*() are declared by sysdep.h even
 * if we're using the default implementation (defined below). */
#define SIL__IN_MEMORY_C

#include "src/base.h"
#include "src/memory.h"
#include "src/sysdep.h"

#ifdef DEBUG  // These macros shouldn't even be referenced when !DEBUG.
# ifdef SIL_DEBUG_USE_VALGRIND
#  include <valgrind/memcheck.h>
# else
#  define VALGRIND_MAKE_MEM_UNDEFINED(ptr,size)  /*nothing*/
# endif
#endif

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Default allocation alignment.  This definition is correct on all
 * supported systems. */
#if defined(SIL_ARCH_X86_64)
# define DEFAULT_ALIGNMENT  16
#else
# define DEFAULT_ALIGNMENT  ((int)sizeof(void *))
#endif

/* Data attached to the beginning of each block. */
typedef struct BlockInfo BlockInfo;
struct BlockInfo {
    void *base;              // Pointer to the block allocated by sys_mem_*().
    int align;               // Alignment requested by the caller.
    int64_t size;            // Size originally requested by the caller.
#ifdef DEBUG
    void *ptr;               // Pointer to the block returned to the caller.
    BlockInfo *next, *prev;  // Links for alloc_list.
    const char *file;        // Allocation call site (source filename).
    uint16_t line;           // Allocation call site (line number).
    int16_t info;            // Usage type (MEM_INFO_*).
#endif
};

/* Helper macro to return the BlockInfo for a block. */
#define BLOCK_INFO(ptr) \
    ((BlockInfo *)align_down((uintptr_t)(ptr), sizeof(void *)) - 1)

#ifdef DEBUG
/* Mutex for touching global debug state.  We call the sys_mutex_*()
 * functions directly instead of using the mutex wrapper to help test
 * mutex_create() under out-of-memory conditions.  Note that this mutex
 * is leaked on program exit. */
static SysMutexID mem_debug_mutex;
/* List of all allocated blocks.  The actual list is contained between
 * these two fenceposts. */
static BlockInfo alloc_list_tail;  // Tentative definition (C99 6.9.2).
static BlockInfo alloc_list_head = {.next = &alloc_list_tail};
static BlockInfo alloc_list_tail = {.prev = &alloc_list_head};
#endif

/*-----------------------------------------------------------------------*/

#ifdef DEBUG

/* Variables used only to implement test-specific behavior.  These are left
 * defined (to no-op values, constant where possible) even when tests are
 * not included to reduce the amount of code which differs between test and
 * non-test builds. */

/* Flag indicating whether the return pointer override (TEST_mem_use()) is
 * active.  To help the compiler optimize out the relevant code when tests
 * are disabled, this flag is only set by TEST_mem_use(), but a NULL value
 * in TEST_use_ptr also indicates that the override is disabled (in that
 * case, because the pointer was already returned from an allocation call). */
static
#ifndef SIL_INCLUDE_TESTS
    const
#endif
    uint8_t TEST_enable_use_ptr = 0;

/* Flag indicating whether forced allocation failure is active. */
static
#ifndef SIL_INCLUDE_TESTS
    const
#endif
    uint8_t TEST_enable_forced_failure = 0;

/* Flag for forcing realloc failures even if the requested size is less
 * than or equal to the original block size. */
static
#ifndef SIL_INCLUDE_TESTS
    const
#endif
    uint8_t TEST_fail_on_shrink = 0;

/* Pointer to return for next allocation operation (if the size and
 * alignment parameters fit), or NULL for regular allocation behavior. */
static void *TEST_use_ptr = NULL;

/* Number of calls until we should deliberately fail an alloc or realloc
 * call (0 = fail on the next call). */
static int TEST_fail_countdown;

/* Call count interval at which we should repeatedly fail (0 = fail once
 * and succeed thereafter). */
static
#ifndef SIL_INCLUDE_TESTS
    const
#endif
    int TEST_fail_every = 1;

#endif

/*-----------------------------------------------------------------------*/

#ifndef SIL_MEMORY_CUSTOM

/* Values to return from sys_mem_avail() and sys_mem_contig().  Only
 * modified by the test interface function TEST_mem_set_sys_avail(). */
static
#ifndef SIL_INCLUDE_TESTS
    const
#endif
    int64_t
        sys_main_avail  = MEM_AVAIL_UNKNOWN,
        sys_main_contig = MEM_AVAIL_UNKNOWN,
        sys_temp_avail  = 0,
        sys_temp_contig = 0;

/* Flag for forcing failures from sys_mem_[re]alloc() (only when not using
 * a custom sys_mem implementation). */
static
#ifndef SIL_INCLUDE_TESTS
    const
#endif
    uint8_t TEST_fail_sys_alloc = 0;

#endif  // !SIL_MEMORY_CUSTOM

/*-----------------------------------------------------------------------*/

/* Macros for modifying the mem_*() function definitions depending on
 * whether we're building in debugging mode.  This saves us the trouble
 * of writing out two separate definition lines for each function. */

#ifdef DEBUG
# define DEBUG_(name)     debug_##name
# define _FILE_LINE       , UNUSED const char *file, UNUSED int line
# define _FILE_LINE_INFO  , UNUSED const char *file, UNUSED int line, int info
#else
# define DEBUG_(name)     name
# define _FILE_LINE       /*nothing*/
# define _FILE_LINE_INFO  /*nothing*/
#endif

/*-----------------------------------------------------------------------*/

/* Macros for logging allocations.  These are defined (to nothing) even in
 * non-debug mode so we don't have to bracket every one with "#ifdef DEBUG". */

#if defined(DEBUG) && defined(SIL_MEMORY_LOG_ALLOCS)
# define LOG_ALLOC(size,flags,ptr) \
    DLOG("[%s:%d] alloc(%lld,%d) -> %p", file, line, (long long)(size), (flags), (ptr))
# define LOG_REALLOC(old,size,flags,ptr) \
    DLOG("[%s:%d] realloc(%p,%lld,%d) -> %p", file, line, (old), (long long)(size), (flags), (ptr))
# define LOG_FREE(ptr) \
    DLOG("[%s:%d] free(%p)", file, line, (ptr))
#else
# define LOG_ALLOC(size,flags,ptr)        /*nothing*/
# define LOG_REALLOC(old,size,flags,ptr)  /*nothing*/
# define LOG_FREE(ptr)                    /*nothing*/
#endif

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

#ifdef DEBUG

/**
 * lock_debug_mutex, unlock_debug_mutex:  Lock or unlock the mutex used
 * for managing debugging information.
 */
static void lock_debug_mutex(void);
static void unlock_debug_mutex(void);

#endif

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

void *DEBUG_(mem_alloc)(int64_t size, int align, int flags _FILE_LINE_INFO)
{
    PRECOND(size >= 0, return NULL);
    PRECOND((align & (align-1)) == 0, return NULL); //Ensure it's a power of 2.

    const int default_align = DEFAULT_ALIGNMENT;
    const int max_align = sys_mem_max_align();
    ASSERT(max_align >= default_align);
    if (align == 0) {
        align = default_align;
    }

#ifdef DEBUG
    lock_debug_mutex();
    if (TEST_enable_forced_failure && TEST_fail_countdown >= 0) {
        TEST_fail_countdown--;
        if (TEST_fail_countdown < 0) {
            DLOG("Failing due to TEST_mem_fail_after() (from %s:%d)",
                 file, line);
            TEST_fail_countdown = TEST_fail_every - 1;
            unlock_debug_mutex();
            LOG_ALLOC(size, flags, NULL);
            return NULL;
        }
    }
#endif

    if (UNLIKELY(size <= 0)) {
#ifdef DEBUG
        unlock_debug_mutex();
#endif
        LOG_ALLOC(size, flags, NULL);
        return NULL;
    }

#ifdef DEBUG
    if (TEST_enable_use_ptr && TEST_use_ptr) {
        BlockInfo *use_block = BLOCK_INFO(TEST_use_ptr);
        if (size <= use_block->size) {
            if ((uintptr_t)TEST_use_ptr % align == 0) {
                use_block->size = size;
                void *ptr = TEST_use_ptr;
                TEST_use_ptr = NULL;
                unlock_debug_mutex();
                return ptr;
            }
        }
        void *old_ptr = TEST_use_ptr;
        TEST_use_ptr = NULL;
        unlock_debug_mutex();
        mem_free(old_ptr);
        lock_debug_mutex();
    }
#endif

    int64_t alloc_size = size;
    int alloc_align = align;
    if (align > max_align) {
        alloc_size += align - max_align;
        alloc_align = max_align;
    }
    alloc_size += align_up(sizeof(BlockInfo), alloc_align);
#ifdef DEBUG
    /* Add initial padding to ensure the returned block is _not_ aligned
     * to the next higher power of two. */
    alloc_size += align;
#endif
    if (alloc_align < (int)sizeof(void *)) {
        alloc_align = sizeof(void *);
    }

    void * const base = sys_mem_alloc(alloc_size, alloc_align,
                                      flags & ~MEM_ALLOC_CLEAR);
    if (UNLIKELY(!base)) {
#ifdef DEBUG
        unlock_debug_mutex();
#endif
        LOG_ALLOC(size, flags, NULL);
        return NULL;
    }
    ASSERT((uintptr_t)base % alloc_align == 0);

    void *ptr;
    uintptr_t address = align_up((uintptr_t)base + sizeof(BlockInfo), align);
#ifdef DEBUG
    if (address % (align*2) == 0) {
        address += align;
    }
#endif
    ptr = (void *)address;
    BlockInfo * const block = BLOCK_INFO(ptr);
    block->base = base;
    block->size = size;
    block->align = align;
#ifdef DEBUG
    block->ptr = ptr;
    block->file = file;
    block->line = line;
    block->info = info;
    alloc_list_tail.prev->next = block;
    block->prev = alloc_list_tail.prev;
    block->next = &alloc_list_tail;
    alloc_list_tail.prev = block;
    unlock_debug_mutex();
#endif

    if (flags & MEM_ALLOC_CLEAR) {
        mem_clear(ptr, size);
    }
#if defined(DEBUG) && defined(SIL_MEMORY_DEBUG_FILL_UNUSED)
    else {
        memset(ptr, 0xBB, size);
        VALGRIND_MAKE_MEM_UNDEFINED(ptr, size);
    }
#endif

    LOG_ALLOC(size, flags, ptr);
    return ptr;
}

/*-----------------------------------------------------------------------*/

void DEBUG_(mem_free)(void *ptr _FILE_LINE)
{
    LOG_FREE(ptr);

    if (!ptr) {
        return;
    }

    BlockInfo * const block = BLOCK_INFO(ptr);
    void * const base = block->base;

#ifdef SIL_MEMORY_CHECK_POINTERS
    for (BlockInfo *i = alloc_list_head.next; i != block; i = i->next) {
        ASSERT(i != &alloc_list_tail, return);
        ASSERT(i != NULL, return);
    }
#endif

#ifdef DEBUG
    lock_debug_mutex();
    block->next->prev = block->prev;
    block->prev->next = block->next;
    if (TEST_enable_use_ptr && TEST_use_ptr == ptr) {
        TEST_use_ptr = NULL;
    }
    unlock_debug_mutex();
# ifdef SIL_MEMORY_DEBUG_FILL_UNUSED
    memset(base, 0xDD, ((char *)ptr + block->size) - (char *)base);
# endif
#endif

    sys_mem_free(base);
}

/*-----------------------------------------------------------------------*/

void *DEBUG_(mem_realloc)(void *ptr, int64_t size, int flags _FILE_LINE_INFO)
{
    PRECOND(size >= 0, return NULL);

    if (UNLIKELY(size == 0)) {
        debug_mem_free(ptr, file, line);
        return NULL;
    }

    if (UNLIKELY(!ptr)) {
        return debug_mem_alloc(size, 0, flags, file, line, info);
    }

    const BlockInfo * const block = BLOCK_INFO(ptr);

#ifdef SIL_MEMORY_CHECK_POINTERS
    for (BlockInfo *i = alloc_list_head.next; i != block; i = i->next) {
        ASSERT(i != &alloc_list_tail, return);
        ASSERT(i != NULL, return);
    }
#endif

#ifdef DEBUG
    lock_debug_mutex();
    if (TEST_enable_forced_failure
     && TEST_fail_countdown >= 0
     && (size > block->size || TEST_fail_on_shrink)) {
        TEST_fail_countdown--;
        if (TEST_fail_countdown < 0) {
            DLOG("Failing due to TEST_mem_fail_after() (from %s:%d)",
                 file, line);
            TEST_fail_countdown = TEST_fail_every - 1;
            unlock_debug_mutex();
            LOG_REALLOC(ptr, size, flags, NULL);
            return NULL;
        }
    }
    if (TEST_enable_use_ptr && TEST_use_ptr) {
        BlockInfo *use_block = BLOCK_INFO(TEST_use_ptr);
        if (size <= use_block->size) {
            if ((uintptr_t)TEST_use_ptr % block->align == 0) {
                use_block->size = size;
                memcpy(TEST_use_ptr, ptr, min(block->size, size));
                void *old_ptr = ptr;
                ptr = TEST_use_ptr;
                TEST_use_ptr = NULL;
                unlock_debug_mutex();
                mem_free(old_ptr);
                return ptr;
            }
        }
        void *old_ptr = TEST_use_ptr;
        TEST_use_ptr = NULL;
        unlock_debug_mutex();
        mem_free(old_ptr);
        lock_debug_mutex();
    }
#endif

#ifdef DEBUG
    BlockInfo * const next = block->next;
    BlockInfo * const prev = block->prev;
#endif
    const int old_offset = (char *)ptr - (char *)block->base;
    const int64_t old_size = block->size;
    const int align = block->align;
    int64_t alloc_size = size;
    int alloc_align = align;
    const int max_align = sys_mem_max_align();
    ASSERT(max_align >= (int)sizeof(void *));
    if (align > max_align) {
        alloc_size += align - max_align;
        alloc_align = max_align;
    }
    alloc_size += align_up(sizeof(BlockInfo), alloc_align);
#ifdef DEBUG
    alloc_size += align;  // As for mem_alloc().
#endif
    if (alloc_align < (int)sizeof(void *)) {
        alloc_align = sizeof(void *);
    }

    void * const new_base =
        sys_mem_realloc(block->base, alloc_size, alloc_align,
                        flags & ~MEM_ALLOC_CLEAR);
    if (UNLIKELY(!new_base)) {
#ifdef DEBUG
        unlock_debug_mutex();
#endif
        LOG_REALLOC(ptr, size, flags, NULL);
        return NULL;
    }
    ASSERT((uintptr_t)new_base % alloc_align == 0);

    void *new_ptr;
    uintptr_t address =
        align_up((uintptr_t)new_base + sizeof(BlockInfo), align);
#ifdef DEBUG
    if (address % (align*2) == 0) {
        address += align;
    }
#endif
    new_ptr = (void *)address;

    /* If the offset from the base address changed, we incur the cost of
     * moving the user data within the reallocated region on top of the
     * already-paid cost of moving the data between system-allocated
     * blocks.  Normally this can only happen when the requested alignment
     * is greater than the system's maximum supported alignment, but since
     * we deliberately misalign with respect to the next higher alignment
     * when debugging, this can also apply to smaller alignments depending
     * on the alignment of the block returned by the system.  Note that
     * there's no concern of data getting cut off by a resize operation
     * because we always reserve the maximum amount of memory necessary
     * for alignment (i.e., we overallocate), so the only thing that can
     * get cut off is padding data at the end. */
    const int offset = (char *)new_ptr - (char *)new_base;
    if (offset != old_offset) {
        memmove(new_ptr, (char *)new_base + old_offset, min(old_size, size));
    }

    BlockInfo * const new_block = BLOCK_INFO(new_ptr);
    new_block->base = new_base;
    new_block->size = size;
    new_block->align = align;
#ifdef DEBUG
    new_block->ptr = new_ptr;
    new_block->file = file;
    new_block->line = line;
    new_block->info = info;
    new_block->next = next;
    new_block->prev = prev;
    next->prev = new_block;
    prev->next = new_block;
    unlock_debug_mutex();
#endif

    void *end_ptr = (void *)((char *)new_ptr + old_size);
    if (size > old_size && (flags & MEM_ALLOC_CLEAR)) {
        mem_clear(end_ptr, size - old_size);
    }
#ifdef DEBUG
    else if (size > old_size) {
        memset(end_ptr, 0xBB, size - old_size);
        VALGRIND_MAKE_MEM_UNDEFINED(end_ptr, size - old_size);
    }
#endif

    LOG_REALLOC(ptr, size, flags, new_ptr);
    return new_ptr;
}

/*-----------------------------------------------------------------------*/

char *DEBUG_(mem_strdup)(const char *str, int flags _FILE_LINE_INFO)
{
    if (!str) {
        return NULL;
    }

    const uint64_t size = strlen(str) + 1;
    char *newstr = (char *)debug_mem_alloc(size, 1, flags & ~MEM_ALLOC_CLEAR,
                                           file, line, info);
    if (newstr) {
        memcpy(newstr, str, size);
    }
    return newstr;
}

/*-----------------------------------------------------------------------*/

int64_t mem_avail(int flags)
{
    return sys_mem_avail(flags);
}

/*-----------------------------------------------------------------------*/

int64_t mem_contig(int flags)
{
    /* Make sure to account for the size of our block header in the
     * returned value. */
    const int64_t overhead = align_up(sizeof(BlockInfo), DEFAULT_ALIGNMENT);
    const int64_t contig = sys_mem_contig(flags);
    if (contig == MEM_AVAIL_UNKNOWN) {
        return MEM_AVAIL_UNKNOWN;
    } else if (contig < overhead) {
        return 0;
    } else {
        return contig - overhead;
    }
}

/*************************************************************************/
/********************* Debug-only interface routines *********************/
/*************************************************************************/

#ifdef DEBUG

/*-----------------------------------------------------------------------*/

void mem_debug_set_info(void *ptr, int info)
{
    PRECOND(ptr != NULL, return);

    BlockInfo *block = BLOCK_INFO(ptr);
    block->info = info;
}

/*-----------------------------------------------------------------------*/

int64_t mem_debug_bytes_allocated(void)
{
    int64_t total = 0;
    lock_debug_mutex();
    for (BlockInfo *block = alloc_list_head.next; block != &alloc_list_tail;
         block = block->next)
    {
        total += block->size;
    }
    unlock_debug_mutex();
    return total;
}

/*-----------------------------------------------------------------------*/

void mem_debug_report_allocs(void)
{
    lock_debug_mutex();
    for (BlockInfo *block = alloc_list_head.next; block != &alloc_list_tail;
         block = block->next)
    {
        DLOG("%p: %lld byte%s (by %s:%u, type %d)", block->ptr,
             (long long)block->size, block->size==1 ? "" : "s",
             block->file, block->line, block->info);
    }
    unlock_debug_mutex();
}

/*-----------------------------------------------------------------------*/

void mem_debug_get_map(const void *base, int64_t size,
                       int8_t *buf, int bufsize)
{
    PRECOND(buf != NULL, return);
    PRECOND(bufsize > 0, return);

    lock_debug_mutex();

    /* First sort the entire allocation list to simplify finding the
     * allocations belonging to each buf[] element.  Since newly allocated
     * blocks are appended to alloc_list, the typical case (assuming this
     * function is called with reasonable frequency) is that we have a
     * long list of already-sorted blocks followed by a few out-of-order
     * elements, so we use an insertion sort starting from the head of the
     * list. */
    for (BlockInfo *block = alloc_list_head.next, *next;
         block != &alloc_list_tail; block = next)
    {
        next = block->next;
        BlockInfo *check = block->prev;
        if (check != &alloc_list_head && UNLIKELY(block < check)) {
            while (check->prev != &alloc_list_head && block < check->prev) {
                check = check->prev;
            }
            block->next->prev = block->prev;
            block->prev->next = block->next;
            block->prev = check->prev;
            block->next = check;
            check->prev->next = block;
            check->prev = block;
        }
    }

    /* Scan up the now-sorted list and fill each element of buf[]
     * appropriately. */
    BlockInfo *block = alloc_list_head.next;
    for (int i = 0; i < bufsize; i++) {
        const char *start =
            (const char *)base + (int)((int64_t)size * i / bufsize);
        const char *end =
            (const char *)base + (int)((int64_t)size * (i+1) / bufsize);
        const int num_bytes = end - start;
        int bytes_font = 0, bytes_manage = 0, bytes_sound = 0,
            bytes_texture = 0, bytes_unknown = 0;

        while (block != &alloc_list_tail
               && (const char *)block->ptr + block->size <= start) {
            block = block->next;
        }
        for (; block != &alloc_list_tail && (char *)block < end;
             block = block->next)
        {
            const char *block_start = (const char *)block;
            const char *block_end = (const char *)block->ptr + block->size;
            const int block_bytes =
                ubound(block_end, end) - lbound(block_start, start);
            switch (block->info) {
                case MEM_INFO_FONT:    bytes_font    += block_bytes; break;
                case MEM_INFO_MANAGE:  bytes_manage  += block_bytes; break;
                case MEM_INFO_SOUND:   bytes_sound   += block_bytes; break;
                case MEM_INFO_TEXTURE: bytes_texture += block_bytes; break;
                default:               bytes_unknown += block_bytes; break;
            }
            if (block_end > end) {
                break;  // Stay on this block for the next buf[] element.
            }
        }
        const int bytes_free = (num_bytes - bytes_unknown - bytes_font
                                - bytes_manage - bytes_sound - bytes_texture);
        if (bytes_free == num_bytes) {
            buf[i] = -1;
        } else if (bytes_font > bytes_manage
                   && bytes_font > bytes_sound
                   && bytes_font > bytes_texture
                   && bytes_font > bytes_unknown) {
            buf[i] = MEM_INFO_FONT;
        } else if (bytes_manage > bytes_sound
                   && bytes_manage > bytes_texture
                   && bytes_manage > bytes_unknown) {
            buf[i] = MEM_INFO_MANAGE;
        } else if (bytes_sound > bytes_texture
                   && bytes_sound > bytes_unknown) {
            buf[i] = MEM_INFO_SOUND;
        } else if (bytes_texture > bytes_unknown) {
            buf[i] = MEM_INFO_TEXTURE;
        } else {
            buf[i] = MEM_INFO_UNKNOWN;
        }
    }

    unlock_debug_mutex();
}

/*-----------------------------------------------------------------------*/

#endif  // DEBUG

/*************************************************************************/
/************************* Test control routines *************************/
/*************************************************************************/

#ifdef SIL_INCLUDE_TESTS

#ifdef SIL_MEMORY_CUSTOM
# define UNUSED_CUSTOM  UNUSED
#else
# define UNUSED_CUSTOM  /*nothing*/
#endif

/*-----------------------------------------------------------------------*/

void TEST_mem_use(void *ptr)
{
    lock_debug_mutex();
    if (TEST_use_ptr) {
        void *old_ptr = TEST_use_ptr;
        TEST_use_ptr = NULL;
        unlock_debug_mutex();
        mem_free(old_ptr);
        lock_debug_mutex();
    }
    if (ptr) {
        for (BlockInfo *block = alloc_list_head.next; block != BLOCK_INFO(ptr);
             block = block->next)
        {
            ASSERT(block != &alloc_list_tail);
            ASSERT(block != NULL);
        }
        TEST_use_ptr = ptr;
    }
    TEST_enable_use_ptr = (TEST_use_ptr != NULL);
    unlock_debug_mutex();
}

/*-----------------------------------------------------------------------*/

void *TEST_mem_push_alloc_list(void)
{
    void *ptr = alloc_list_head.next;
    if (ptr == &alloc_list_tail) {
        return NULL;
    }
    alloc_list_head.next->prev = NULL;
    alloc_list_head.next = &alloc_list_tail;
    alloc_list_tail.prev->next = NULL;
    alloc_list_tail.prev = &alloc_list_head;
    return ptr;
}

/*-----------------------------------------------------------------------*/

void TEST_mem_pop_alloc_list(void *list)
{
    if (!list) {
        return;
    }
    BlockInfo *block = (BlockInfo *)list;
    alloc_list_tail.prev->next = block;
    block->prev = alloc_list_tail.prev;
    while (block->next) {
        block = block->next;
    }
    alloc_list_tail.prev = block;
    block->next = &alloc_list_tail;
}

/*-----------------------------------------------------------------------*/

void TEST_mem_fail_after(int calls, int every, int fail_on_shrink)
{
    lock_debug_mutex();
    TEST_enable_forced_failure = (calls >= 0);
    TEST_fail_countdown = calls;
    TEST_fail_every = every;
    TEST_fail_on_shrink = (fail_on_shrink != 0);
    unlock_debug_mutex();
}

/*-----------------------------------------------------------------------*/

void TEST_mem_fail_sys_alloc(UNUSED_CUSTOM int fail)
{
#ifndef SIL_MEMORY_CUSTOM
    TEST_fail_sys_alloc = (fail != 0);
#endif
}

/*-----------------------------------------------------------------------*/

void TEST_mem_set_sys_avail(
    UNUSED_CUSTOM int64_t main_avail, UNUSED_CUSTOM int64_t main_contig,
    UNUSED_CUSTOM int64_t temp_avail, UNUSED_CUSTOM int64_t temp_contig)
{
#ifndef SIL_MEMORY_CUSTOM
    sys_main_avail  = main_avail;
    sys_main_contig = main_contig;
    sys_temp_avail  = temp_avail;
    sys_temp_contig = temp_contig;
#endif
}

/*-----------------------------------------------------------------------*/

#undef UNUSED_CUSTOM

#endif  // DEBUG

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

#ifdef DEBUG

static void lock_debug_mutex(void)
{
    if (UNLIKELY(mem_debug_mutex == (SysMutexID)-1)) {  // See below.
        return;
    }
    if (UNLIKELY(!mem_debug_mutex)) {
        /* We assume that there won't be any risk of racing on the very
         * first allocation.  Since this is only a debugging feature, we
         * don't try harder to do it properly.  We set mem_debug_mutex to
         * -1 first to avoid infinite recursion if sys_mutex_create() tries
         * to allocate memory. */
        mem_debug_mutex = (SysMutexID)-1;
        mem_debug_mutex = sys_mutex_create(0, 0);
        ASSERT(mem_debug_mutex != 0);
    }
    sys_mutex_lock(mem_debug_mutex, -1);
}

static void unlock_debug_mutex(void)
{
    if (UNLIKELY(mem_debug_mutex == (SysMutexID)-1)) {
        return;
    }
    sys_mutex_unlock(mem_debug_mutex);
}

#endif  // DEBUG

/*************************************************************************/
/****************** Default sys_mem_*() implementation *******************/
/*************************************************************************/

#ifndef SIL_MEMORY_CUSTOM

/*
 * This default implementation of the system-level memory management
 * functions simply wraps the standard malloc() family of routines.
 */

/* If the malloc() family of functions is disabled, re-enable them. */
#ifdef SIL_MEMORY_FORBID_MALLOC
# undef malloc
# undef free
# undef realloc
#endif

/*-----------------------------------------------------------------------*/

static inline void *sys_mem_alloc(int64_t size, UNUSED int align,
                                  UNUSED int flags)
{
    if (UNLIKELY(TEST_fail_sys_alloc)) {
        return NULL;
    }
    if (sizeof(size_t) < 8) {
        const size_t max_size = ~(size_t)0;
        if (UNLIKELY((uint64_t)size > max_size)) {
            DLOG("Size %lld too large (max %zu)", (long long)size, max_size);
            return NULL;
        }
    }
    return malloc((size_t)size);
}

/*-----------------------------------------------------------------------*/

static inline void sys_mem_free(void *ptr)
{
    free(ptr);
}

/*-----------------------------------------------------------------------*/

static inline void *sys_mem_realloc(void *ptr, int64_t size, UNUSED int align,
                                    UNUSED int flags)
{
    if (UNLIKELY(TEST_fail_sys_alloc)) {
        return NULL;
    }
    if (sizeof(size_t) < 8) {
        const size_t max_size = ~(size_t)0;
        if (UNLIKELY((uint64_t)size > max_size)) {
            DLOG("Size %lld too large (max %zu)", (long long)size, max_size);
            return NULL;
        }
    }
    return realloc(ptr, (size_t)size);
}

/*-----------------------------------------------------------------------*/

static inline int64_t sys_mem_avail(int flags)
{
    if (flags & MEM_ALLOC_TEMP) {
        return sys_temp_avail;
    } else {
        return sys_main_avail;
    }
}

/*-----------------------------------------------------------------------*/

static inline int64_t sys_mem_contig(int flags)
{
    if (flags & MEM_ALLOC_TEMP) {
        return sys_temp_contig;
    } else {
        return sys_main_contig;
    }
}

/*-----------------------------------------------------------------------*/

static inline int sys_mem_max_align(void)
{
    /* malloc() is defined to return a pointer "suitably aligned so that
     * it may be assigned to a pointer of any type of object and then used
     * to access such an object ... in the space allocated" (C99 7.20.3).
     * On modern systems, this generally means that blocks will be at least
     * pointer-aligned, with the exception of x86-64 which forces 16-byte
     * alignment. */
#if defined(SIL_ARCH_X86_64)
    return 16;
#else
    return sizeof(void *);
#endif
}

/*-----------------------------------------------------------------------*/

#endif  // !SIL_MEMORY_CUSTOM

/*************************************************************************/
/*************************************************************************/
