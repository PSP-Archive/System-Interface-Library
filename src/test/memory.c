/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/memory.c: Test routines for memory management functions.
 */

#include "src/base.h"
#include "src/memory.h"
#include "src/random.h"
#include "src/sysdep.h"
#include "src/test/base.h"

#ifdef SIL_DEBUG_USE_VALGRIND
# include <valgrind/memcheck.h>
#else
# define VALGRIND_MAKE_MEM_DEFINED(ptr,size)  /*nothing*/
#endif

/*************************************************************************/
/*************************************************************************/

/* Convenience macro to return the same maximum alignment as used by
 * mem_[re]alloc(). */
#ifdef SIL_MEMORY_CUSTOM
# define MAX_ALIGN() sys_mem_max_align()
#else
# define MAX_ALIGN() sizeof(void *)  // See sys_mem_max_align() in ../memory.c.
#endif

/*************************************************************************/
/***************************** Test runner *******************************/
/*************************************************************************/

static int do_test_memory(void);
int test_memory(void)
{
    /* Ignore any previous allocations during these tests so we have a
     * consistent environment. */
    void *saved_alloc_list = TEST_mem_push_alloc_list();
    int result = do_test_memory();
    TEST_mem_pop_alloc_list(saved_alloc_list);
    return result;
}

DEFINE_GENERIC_TEST_RUNNER(do_test_memory)

/*************************************************************************/
/*********************** Generic allocation tests ************************/
/*************************************************************************/

TEST(test_alloc_and_free)
{
    const int64_t used = mem_debug_bytes_allocated();
    char *ptr;

    CHECK_TRUE(ptr = mem_alloc(1, 0, 0));
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);
    mem_free(ptr);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_alloc_clear)
{
    const int64_t used = mem_debug_bytes_allocated();
    char *ptr;

    CHECK_TRUE(ptr = mem_alloc(1, 0, MEM_ALLOC_CLEAR));
    CHECK_TRUE(*ptr == 0);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);
    mem_free(ptr);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used);

#ifdef SIL_MEMORY_DEBUG_FILL_UNUSED
    /* For completeness, also check that without MEM_ALLOC_CLEAR the
     * memory is not cleared (debugging code in ../memory.c explicitly
     * sets it to 0xBB). */
    CHECK_TRUE(ptr = mem_alloc(1, 0, 0));
    VALGRIND_MAKE_MEM_DEFINED(ptr, 1);
    CHECK_TRUE((uint8_t)*ptr == 0xBB);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);
    mem_free(ptr);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used);
#endif

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_alloc_top)
{
    const int64_t used = mem_debug_bytes_allocated();
    char *ptr;

    CHECK_TRUE(ptr = mem_alloc(1, 0, MEM_ALLOC_TOP));
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);
    mem_free(ptr);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_alloc_temp)
{
    const int64_t used = mem_debug_bytes_allocated();
    char *ptr;

    CHECK_TRUE(ptr = mem_alloc(1, 0, MEM_ALLOC_TEMP));
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);
    mem_free(ptr);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_alloc_aligned_by_system)
{
    const int64_t used = mem_debug_bytes_allocated();
    const int max_align = MAX_ALIGN();

    for (int align = 1; align <= max_align; align *= 2) {
        char *ptr;
        CHECK_TRUE(ptr = mem_alloc(1, align, 0));
        CHECK_INTEQUAL((uintptr_t)ptr % align, 0);
        /* Also check that the pointer is unaligned with respect to the
         * next higher alignment, as documented for debug mode. */
        CHECK_INTEQUAL((uintptr_t)ptr % (align*2), align);
        CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);
        mem_free(ptr);
        CHECK_INTEQUAL(mem_debug_bytes_allocated(), used);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_alloc_aligned_manually)
{
    const int64_t used = mem_debug_bytes_allocated();
    const int max_align = MAX_ALIGN();

    for (int align = max_align*2; align <= max_align*16; align *= 2) {
        char *ptr;
        CHECK_TRUE(ptr = mem_alloc(1, align, 0));
        CHECK_INTEQUAL((uintptr_t)ptr % align, 0);
        CHECK_INTEQUAL((uintptr_t)ptr % (align*2), align);
        CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);
        mem_free(ptr);
        CHECK_INTEQUAL(mem_debug_bytes_allocated(), used);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_alloc_default_alignment)
{
    const int64_t used = mem_debug_bytes_allocated();
    /* As for DEFAULT_ALIGNMENT in src/memory.c. */
    const int default_align = sizeof(void *);

    char *ptr;
    CHECK_TRUE(ptr = mem_alloc(1, default_align, 0));
    CHECK_INTEQUAL((uintptr_t)ptr % default_align, 0);
    CHECK_INTEQUAL((uintptr_t)ptr % (default_align*2), default_align);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);
    mem_free(ptr);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used);

    return 1;
}

/*-----------------------------------------------------------------------*/

#ifndef SIL_MEMORY_CUSTOM

TEST(test_alloc_sys_mem_fail)
{
    const int64_t used = mem_debug_bytes_allocated();

    TEST_mem_fail_sys_alloc(1);
    CHECK_FALSE(mem_alloc(1, 0, 0));
    TEST_mem_fail_sys_alloc(0);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used);

    return 1;
}

#endif

/*-----------------------------------------------------------------------*/

TEST(test_alloc_zero_size)
{
    const int64_t used = mem_debug_bytes_allocated();

    CHECK_FALSE(mem_alloc(0, 0, 0));
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_alloc_64_bit_size)
{
    if (sizeof(size_t) < 8) {
        const int64_t used = mem_debug_bytes_allocated();

        /* Make sure it's not truncated to 1 byte. */
        CHECK_FALSE(mem_alloc(0x1000000000000001ULL, 0, 0));
        CHECK_INTEQUAL(mem_debug_bytes_allocated(), used);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_free_null_pointer)
{
    /* Just make sure it doesn't crash. */
    mem_free(NULL);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_realloc_same_size)
{
    const int64_t used = mem_debug_bytes_allocated();
    char *ptr;

    CHECK_TRUE(ptr = mem_alloc(1, 0, 0));
    *ptr = 1;
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);
    CHECK_TRUE(ptr = mem_realloc(ptr, 1, 0));
    CHECK_INTEQUAL(*ptr, 1);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);
    mem_free(ptr);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_realloc_grow)
{
    const int64_t used = mem_debug_bytes_allocated();
    char *ptr;

    CHECK_TRUE(ptr = mem_alloc(1, 0, 0));
    *ptr = 1;
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);
    CHECK_TRUE(ptr = mem_realloc(ptr, 2, 0));
    CHECK_INTEQUAL(ptr[0], 1);
#ifdef SIL_MEMORY_DEBUG_FILL_UNUSED
    VALGRIND_MAKE_MEM_DEFINED(ptr+1, 1);
    CHECK_INTEQUAL((uint8_t)ptr[1], 0xBB);
#endif
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+2);
    mem_free(ptr);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_realloc_grow_clear)
{
    const int64_t used = mem_debug_bytes_allocated();
    char *ptr;

    CHECK_TRUE(ptr = mem_alloc(1, 0, 0));
    *ptr = 1;
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);
    CHECK_TRUE(ptr = mem_realloc(ptr, 2, MEM_ALLOC_CLEAR));
    CHECK_INTEQUAL(ptr[0], 1);
    CHECK_INTEQUAL(ptr[1], 0);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+2);
    mem_free(ptr);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_realloc_shrink)
{
    const int64_t used = mem_debug_bytes_allocated();
    char *ptr;

    CHECK_TRUE(ptr = mem_alloc(1, 0, 0));
    *ptr = 1;
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);
    CHECK_TRUE(ptr = mem_realloc(ptr, 2, 0));
    CHECK_INTEQUAL(ptr[0], 1);
#ifdef SIL_MEMORY_DEBUG_FILL_UNUSED
    VALGRIND_MAKE_MEM_DEFINED(ptr+1, 1);
    CHECK_INTEQUAL((uint8_t)ptr[1], 0xBB);
#endif
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+2);
    mem_free(ptr);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_realloc_different_flags)
{
    const int64_t used = mem_debug_bytes_allocated();
    char *ptr;

    CHECK_TRUE(ptr = mem_alloc(1, 0, MEM_ALLOC_TOP));
    *ptr = 1;
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);
    CHECK_TRUE(ptr = mem_realloc(ptr, 1, 0));
    CHECK_INTEQUAL(*ptr, 1);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);
    CHECK_TRUE(ptr = mem_realloc(ptr, 1, MEM_ALLOC_TEMP));
    CHECK_INTEQUAL(*ptr, 1);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);
    mem_free(ptr);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_realloc_aligned_by_system)
{
    const int64_t used = mem_debug_bytes_allocated();
    const int max_align = MAX_ALIGN();

    for (int align = 1; align <= max_align; align *= 2) {
        char *ptr;

        CHECK_TRUE(ptr = mem_alloc(align, align, 0));
        CHECK_INTEQUAL((uintptr_t)ptr % align, 0);
        CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+align);
        for (int i = 0; i < align; i++) {
            ptr[i] = (char)i;
        }

        CHECK_TRUE(ptr = mem_realloc(ptr, align*2, 0));
        CHECK_INTEQUAL((uintptr_t)ptr % align, 0);
        CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+align*2);
        for (int i = 0; i < align; i++) {
            CHECK_INTEQUAL(ptr[i], (char)i);
        }

        mem_free(ptr);
        CHECK_INTEQUAL(mem_debug_bytes_allocated(), used);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_realloc_aligned_manually)
{
    const int64_t used = mem_debug_bytes_allocated();
    const int max_align = MAX_ALIGN();

    for (int align = max_align*2; align <= max_align*16; align *= 2) {
        char *ptr;

        CHECK_TRUE(ptr = mem_alloc(align, align, 0));
        CHECK_INTEQUAL((uintptr_t)ptr % align, 0);
        CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+align);
        for (int i = 0; i < align; i++) {
            ptr[i] = (char)i;
        }

        CHECK_TRUE(ptr = mem_realloc(ptr, align*2, 0));
        CHECK_INTEQUAL((uintptr_t)ptr % align, 0);
        CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+align*2);
        for (int i = 0; i < align; i++) {
            CHECK_INTEQUAL(ptr[i], (char)i);
        }

        mem_free(ptr);
        CHECK_INTEQUAL(mem_debug_bytes_allocated(), used);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_realloc_changed_internal_offset)
{
    /*
     * This test is designed to exercise the logic in mem_realloc() which
     * moves user data if the offset from the system-allocated pointer to
     * the aligned user pointer changes, which can occur when requesting
     * alignment greater than the system provides.  Since we don't have a
     * test control interface to request a specific value in the low bits
     * of the address returned by sys_mem_*(), we rely on the fact that
     * incrementing the pointer returned from mem_alloc() by less than
     * sizeof(void *) will fool mem_realloc() into thinking it's the same
     * block with a different internal offset.  We also assume that the
     * platform's memory allocation size alignment is greater than 1 byte,
     * since we deliberately write one byte past the end of the first
     * allocation.
     */

    const int max_align = MAX_ALIGN();
    const int align = max_align * 4;

    const int64_t used = mem_debug_bytes_allocated();
    char *ptr;
    const int alloc_size = align;

    CHECK_TRUE(ptr = mem_alloc(alloc_size-1, align, 0));
    CHECK_INTEQUAL((uintptr_t)ptr % (max_align*4), 0);
    /* Since we touch a byte of memory which is technically invalid, we
     * need to tell Valgrind it's okay to write to that byte. */
    VALGRIND_MAKE_MEM_DEFINED(&ptr[alloc_size], 1);
    ptr++;
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+(alloc_size-1));
    for (int i = -1; i < alloc_size-1; i++) {
        ptr[i] = (char)i;
    }

    CHECK_TRUE(ptr = mem_realloc(ptr, alloc_size, MEM_ALLOC_CLEAR));
    CHECK_INTEQUAL((uintptr_t)ptr % (max_align*4), 0);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+alloc_size);
    for (int i = 0; i < alloc_size-1; i++) {
        CHECK_INTEQUAL(ptr[i], (char)i);
    }
    CHECK_INTEQUAL(ptr[alloc_size-1], 0);

    mem_free(ptr);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_realloc_alloc)
{
    const int64_t used = mem_debug_bytes_allocated();
    char *ptr;

    CHECK_TRUE(ptr = mem_realloc(NULL, 1, 0));
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);
    mem_free(ptr);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_realloc_free)
{
    const int64_t used = mem_debug_bytes_allocated();
    char *ptr;

    CHECK_TRUE(ptr = mem_alloc(1, 0, 0));
    *ptr = 1;
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);
    CHECK_FALSE(ptr = mem_realloc(ptr, 0, 0));
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_realloc_64_bit_size)
{
    if (sizeof(size_t) < 8) {
        const int64_t used = mem_debug_bytes_allocated();
        char *ptr;

        CHECK_TRUE(ptr = mem_alloc(1, 0, 0));
        *ptr = 1;
        /* Make sure the size isn't truncated. */
        CHECK_FALSE(mem_realloc(ptr, 0x1000000000000001ULL, 0));
        CHECK_INTEQUAL(*ptr, 1);
        mem_free(ptr);
        CHECK_INTEQUAL(mem_debug_bytes_allocated(), used);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

#ifndef SIL_MEMORY_CUSTOM

TEST(test_realloc_sys_mem_fail)
{
    const int64_t used = mem_debug_bytes_allocated();
    char *ptr;

    CHECK_TRUE(ptr = mem_alloc(1, 0, 0));
    *ptr = 1;
    TEST_mem_fail_sys_alloc(1);
    CHECK_FALSE(mem_realloc(ptr, 100, 0));
    TEST_mem_fail_sys_alloc(0);
    CHECK_INTEQUAL(*ptr, 1);
    mem_free(ptr);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used);

    return 1;
}

#endif

/*-----------------------------------------------------------------------*/

TEST(test_debug_block_list)
{
    /* Check a couple of extra code paths for managing the list of blocks
     * not taken otherwise. */

    void *ptr, *ptr2;

    CHECK_TRUE(ptr = mem_alloc(1, 0, 0));
    CHECK_TRUE(ptr2 = mem_alloc(1, 0, 0));

    /* Realloc of the most recently allocated block. */
    CHECK_TRUE(ptr2 = mem_realloc(ptr2, 2, 0));

    /* Free of the most recently allocated block. */
    mem_free(ptr2);

    mem_free(ptr);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_strdup)
{
    const int64_t used = mem_debug_bytes_allocated();
    char *ptr;

    CHECK_TRUE(ptr = mem_strdup("a", 0));
    CHECK_INTEQUAL(ptr[0], 'a');
    CHECK_INTEQUAL(ptr[1], 0);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+2);
    mem_free(ptr);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_strdup_null_pointer)
{
    const int64_t used = mem_debug_bytes_allocated();

    CHECK_FALSE(mem_strdup(NULL, 0));
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used);

    return 1;
}

/*************************************************************************/
/*********************** Memory information tests ************************/
/*************************************************************************/

TEST(test_avail)
{
    const int64_t avail = mem_avail(0);
    if (avail == MEM_AVAIL_UNKNOWN) {
        DLOG("mem_avail(0) == MEM_AVAIL_UNKNOWN, can't test");
    } else {
        char *ptr;
        CHECK_TRUE(ptr = mem_alloc(1, 0, 0));
        CHECK_TRUE(mem_avail(0) < avail);
        mem_free(ptr);
        CHECK_INTEQUAL(mem_avail(0), avail);
    }

    const int64_t temp_avail = mem_avail(MEM_ALLOC_TEMP);
    if (temp_avail == 0 || temp_avail == MEM_AVAIL_UNKNOWN) {
        DLOG("mem_avail(MEM_ALLOC_TEMP) == {0 | MEM_AVAIL_UNKNOWN}, can't"
             " test");
    } else {
        char *ptr;
        CHECK_TRUE(ptr = mem_alloc(1, 0, MEM_ALLOC_TEMP));
        CHECK_TRUE(mem_avail(MEM_ALLOC_TEMP) < temp_avail);
        mem_free(ptr);
        CHECK_INTEQUAL(mem_avail(MEM_ALLOC_TEMP), temp_avail);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_contig)
{
    const int64_t contig = mem_contig(0);
    if (contig == MEM_AVAIL_UNKNOWN) {
        DLOG("mem_contig(0) == MEM_AVAIL_UNKNOWN, can't test");
    } else {
        const int64_t avail = mem_avail(0);  // Assume it succeeds.
        char *ptr;
        /* Under DEBUG, mem_alloc() adds initial padding to ensure
         * misalignment with respect to the next-higher power of two.
         * Account for that here to avoid spurious allocation failure. */
        CHECK_TRUE(ptr = mem_alloc(contig - 1, 1, 0));
        CHECK_TRUE(mem_avail(0) < avail);
        mem_free(ptr);
        CHECK_INTEQUAL(mem_avail(0), avail);
    }

    const int64_t temp_contig = mem_contig(MEM_ALLOC_TEMP);
    if (temp_contig == 0 || temp_contig == MEM_AVAIL_UNKNOWN) {
        DLOG("mem_contig(MEM_ALLOC_TEMP) == {0 | MEM_AVAIL_UNKNOWN}, can't"
             " test");
    } else {
        const int64_t temp_avail = mem_avail(MEM_ALLOC_TEMP);
        char *ptr;
        CHECK_TRUE(ptr = mem_alloc(temp_contig - 1, 1, MEM_ALLOC_TEMP));
        CHECK_TRUE(mem_avail(MEM_ALLOC_TEMP) < temp_avail);
        mem_free(ptr);
        CHECK_INTEQUAL(mem_avail(MEM_ALLOC_TEMP), temp_avail);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

#ifndef SIL_MEMORY_CUSTOM

TEST(test_set_sys_avail)
{
    TEST_mem_set_sys_avail(1024, 1024, 0, 0);
    const int64_t overhead = 1024 - mem_contig(0);
    CHECK_INTRANGE(overhead, 5, 1023);

    TEST_mem_set_sys_avail(65536, 61440+overhead, 16384, 12288+overhead);
    CHECK_INTEQUAL(mem_avail(0), 65536);
    CHECK_INTEQUAL(mem_contig(0), 61440);
    CHECK_INTEQUAL(mem_avail(MEM_ALLOC_TEMP), 16384);
    CHECK_INTEQUAL(mem_contig(MEM_ALLOC_TEMP), 12288);

    /* Values smaller than overhead should be rounded down to zero for
     * mem_contig(). */
    TEST_mem_set_sys_avail(4, 3, 2, 1);
    CHECK_INTEQUAL(mem_avail(0), 4);
    CHECK_INTEQUAL(mem_contig(0), 0);
    CHECK_INTEQUAL(mem_avail(MEM_ALLOC_TEMP), 2);
    CHECK_INTEQUAL(mem_contig(MEM_ALLOC_TEMP), 0);

    /* MEM_AVAIL_UNKNOWN should be left unchanged for mem_contig(). */
    TEST_mem_set_sys_avail(MEM_AVAIL_UNKNOWN, MEM_AVAIL_UNKNOWN,
                           MEM_AVAIL_UNKNOWN, MEM_AVAIL_UNKNOWN);
    CHECK_INTEQUAL(mem_avail(0), MEM_AVAIL_UNKNOWN);
    CHECK_INTEQUAL(mem_contig(0), MEM_AVAIL_UNKNOWN);
    CHECK_INTEQUAL(mem_avail(MEM_ALLOC_TEMP), MEM_AVAIL_UNKNOWN);
    CHECK_INTEQUAL(mem_contig(MEM_ALLOC_TEMP), MEM_AVAIL_UNKNOWN);

    TEST_mem_set_sys_avail(MEM_AVAIL_UNKNOWN, MEM_AVAIL_UNKNOWN, 0, 0);
    return 1;
}

#endif

/*-----------------------------------------------------------------------*/

TEST(test_report_allocs)
{
    char *ptr, *ptr2;

    DLOG("dummy message");
    mem_debug_report_allocs();  // Will log nothing.
    CHECK_DLOG_TEXT("dummy message");

    /* Be careful to keep these two lines together or the test will break! */
    const int alloc_line = __LINE__ + 1;
    CHECK_TRUE(ptr = mem_alloc(1, 0, 0));

    mem_debug_report_allocs();
    CHECK_DLOG_TEXT("%p: 1 byte (by %s:%u, type 0)",
                    ptr, __FILE__, alloc_line);

    const int realloc_line = __LINE__ + 1;
    CHECK_TRUE(ptr = debug_mem_realloc(ptr, 2, 0, __FILE__, realloc_line, 4));

    mem_debug_report_allocs();
    CHECK_DLOG_TEXT("%p: 2 bytes (by %s:%u, type 4)",
                    ptr, __FILE__, realloc_line);

    const int alloc2_line = __LINE__ + 1;
    CHECK_TRUE(ptr2 = debug_mem_alloc(3, 0, 0, __FILE__, alloc2_line, 6));

    mem_debug_report_allocs();  // Should log the most recent allocation last.
    CHECK_DLOG_TEXT("%p: 3 bytes (by %s:%u, type 6)",
                    ptr2, __FILE__, alloc2_line);

    mem_free(ptr);
    mem_free(ptr2);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_info)
{
    char *ptr;

    /* Be careful to keep these two lines together or the test will break! */
    const int alloc_line = __LINE__ + 1;
    CHECK_TRUE(ptr = mem_alloc(1, 0, 0));

    mem_debug_report_allocs();
    CHECK_DLOG_TEXT("%p: 1 byte (by %s:%u, type 0)",
                    ptr, __FILE__, alloc_line);

    mem_debug_set_info(ptr, 1);
    mem_debug_report_allocs();
    CHECK_DLOG_TEXT("%p: 1 byte (by %s:%u, type 1)",
                    ptr, __FILE__, alloc_line);

    mem_free(ptr);
    return 1;
}

/*************************************************************************/
/*********************** mem_debug_get_map() tests ***********************/
/*************************************************************************/

/* For these tests, we need to know the size of the BlockInfo structure
 * that precedes each allocated block, since the allocation information
 * returned by mem_debug_get_map() treats BlockInfo memory as allocated.
 * This structure definition should be kept in sync with the definition
 * in src/memory.c. */

typedef struct BlockInfo BlockInfo;
struct BlockInfo {
    void *base;
    int align;
    int64_t size;
    void *ptr;
    BlockInfo *next, *prev;
    const char *file;
    uint16_t line;
    int16_t info;
};

/*-----------------------------------------------------------------------*/

TEST(test_get_map)
{
    char *ptr;
    CHECK_TRUE(ptr = mem_alloc(1, 0, 0));
    char *base = ptr - sizeof(BlockInfo);

    int8_t map[3];

    /* Check with the allocated block fitting exactly in a map slot. */
    mem_debug_get_map(base - (sizeof(BlockInfo)+1),
                      3*(sizeof(BlockInfo)+1), map, 3);
    CHECK_INTEQUAL(map[0], -1);
    CHECK_INTEQUAL(map[1], MEM_INFO_UNKNOWN);
    CHECK_INTEQUAL(map[2], -1);

    /* Check with the allocated block smaller than a map slot. */
    mem_debug_get_map(base - 4*(sizeof(BlockInfo)+1),
                      9*(sizeof(BlockInfo)+1), map, 3);
    CHECK_INTEQUAL(map[0], -1);
    CHECK_INTEQUAL(map[1], MEM_INFO_UNKNOWN);
    CHECK_INTEQUAL(map[2], -1);

    /* Check with 1 byte allocated at the end of a slot. */
    mem_debug_get_map(base - (sizeof(BlockInfo)+1) + 1,
                      3*(sizeof(BlockInfo)+1), map, 3);
    CHECK_INTEQUAL(map[0], MEM_INFO_UNKNOWN);
    CHECK_INTEQUAL(map[1], MEM_INFO_UNKNOWN);
    CHECK_INTEQUAL(map[2], -1);

    /* Check with 1 byte allocated at the beginning of a slot. */
    mem_debug_get_map(base - (sizeof(BlockInfo)+1) - 1,
                      3*(sizeof(BlockInfo)+1), map, 3);
    CHECK_INTEQUAL(map[0], -1);
    CHECK_INTEQUAL(map[1], MEM_INFO_UNKNOWN);
    CHECK_INTEQUAL(map[2], MEM_INFO_UNKNOWN);

    /* Check with the allocation overlapping the start of the first slot. */
    mem_debug_get_map(ptr, 3*(sizeof(BlockInfo)+1), map, 3);
    CHECK_INTEQUAL(map[0], MEM_INFO_UNKNOWN);
    CHECK_INTEQUAL(map[1], -1);
    CHECK_INTEQUAL(map[2], -1);

    /* Check with the allocation completely before the first slot. */
    mem_debug_get_map(ptr+1, 3*(sizeof(BlockInfo)+1), map, 3);
    CHECK_INTEQUAL(map[0], -1);
    CHECK_INTEQUAL(map[1], -1);
    CHECK_INTEQUAL(map[2], -1);

    /* Check with the allocation overlapping the end of the last slot. */
    mem_debug_get_map(base - 3*(sizeof(BlockInfo)+1) + 1,
                      3*(sizeof(BlockInfo)+1), map, 3);
    CHECK_INTEQUAL(map[0], -1);
    CHECK_INTEQUAL(map[1], -1);
    CHECK_INTEQUAL(map[2], MEM_INFO_UNKNOWN);

    /* Check with the allocation completely after the last slot. */
    mem_debug_get_map(base - 3*(sizeof(BlockInfo)+1),
                      3*(sizeof(BlockInfo)+1), map, 3);
    CHECK_INTEQUAL(map[0], -1);
    CHECK_INTEQUAL(map[1], -1);
    CHECK_INTEQUAL(map[2], -1);

    mem_free(ptr);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_map_info)
{
    char *ptr;
    CHECK_TRUE(ptr = mem_alloc(1, 0, 0));

    int8_t map[1];

    static const int infos[] = {
        MEM_INFO_FONT,
        MEM_INFO_MANAGE,
        MEM_INFO_SOUND,
        MEM_INFO_TEXTURE,
    };
    for (int i = 0; i < lenof(infos); i++) {
        mem_debug_set_info(ptr, infos[i]);
        mem_debug_get_map(ptr, 1, map, 1);
        CHECK_INTEQUAL(map[0], infos[i]);
    }

    mem_free(ptr);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_map_multiple_blocks)
{
    char *ptr1, *ptr2, *ptr3;
    BlockInfo *block1, *block2, *block3;

    /* Note that an odd block size like this will ensure that there are
     * unused bytes after (and, consequently, before) each block. */
    const int alloc_size = 1;
    const int block_size = alloc_size + sizeof(BlockInfo);

    int8_t map[1000];
    ASSERT((int)sizeof(map) >= block_size+2);


    CHECK_TRUE(ptr1 = mem_alloc(alloc_size, sizeof(void *), 0));
    CHECK_TRUE(ptr2 = mem_alloc(alloc_size, sizeof(void *), 0));
    CHECK_TRUE(ptr3 = mem_alloc(alloc_size, sizeof(void *), 0));
    block1 = (BlockInfo *)((uintptr_t)ptr1 - sizeof(BlockInfo));
    block2 = (BlockInfo *)((uintptr_t)ptr2 - sizeof(BlockInfo));
    block3 = (BlockInfo *)((uintptr_t)ptr3 - sizeof(BlockInfo));
    /* Manually reorder the block list as though ptr1, ptr2, and ptr3 were
     * allocated in decreasing address order, to ensure that we hit all the
     * conditions in the sort algorithm. */
    ASSERT(block1->prev != block2 && block1->prev != block3);
    ASSERT(block1->next == block2);
    ASSERT(block2->prev == block1);
    ASSERT(block2->next == block3);
    ASSERT(block3->prev == block2);
    ASSERT(block3->next != block1 && block3->next != block2);
    BlockInfo *head = block1->prev;
    BlockInfo *tail = block3->next;
    if (ptr2 < ptr1) {
        char *temp = ptr1; ptr1 = ptr2; ptr2 = temp;
        BlockInfo *btemp = block1; block1 = block2; block2 = btemp;
    }
    if (ptr3 < ptr2) {
        char *temp = ptr2; ptr2 = ptr3; ptr3 = temp;
        BlockInfo *btemp = block2; block2 = block3; block3 = btemp;
        if (ptr2 < ptr1) {
            temp = ptr1; ptr1 = ptr2; ptr2 = temp;
            btemp = block1; block1 = block2; block2 = btemp;
        }
    }
    head->next = block3;
    block3->prev = head;
    block3->next = block2;
    block2->prev = block3;
    block2->next = block1;
    block1->prev = block2;
    block1->next = tail;
    tail->prev = block1;

    mem_debug_get_map((char *)block1 - 1, block_size+2, map, block_size+2);
    CHECK_INTEQUAL(map[0], -1);
    CHECK_INTEQUAL(map[1], MEM_INFO_UNKNOWN);
    CHECK_INTEQUAL(map[block_size], MEM_INFO_UNKNOWN);
    CHECK_INTEQUAL(map[block_size+1], -1);

    mem_debug_get_map((char *)block2 - 1, block_size+2, map, block_size+2);
    CHECK_INTEQUAL(map[0], -1);
    CHECK_INTEQUAL(map[1], MEM_INFO_UNKNOWN);
    CHECK_INTEQUAL(map[block_size], MEM_INFO_UNKNOWN);
    CHECK_INTEQUAL(map[block_size+1], -1);

    mem_debug_get_map((char *)block3 - 1, block_size+2, map, block_size+2);
    CHECK_INTEQUAL(map[0], -1);
    CHECK_INTEQUAL(map[1], MEM_INFO_UNKNOWN);
    CHECK_INTEQUAL(map[block_size], MEM_INFO_UNKNOWN);
    CHECK_INTEQUAL(map[block_size+1], -1);

    CHECK_PTREQUAL(head->next, block1);
    CHECK_PTREQUAL(block1->prev, head);
    CHECK_PTREQUAL(block1->next, block2);
    CHECK_PTREQUAL(block2->prev, block1);
    CHECK_PTREQUAL(block2->next, block3);
    CHECK_PTREQUAL(block3->prev, block2);
    CHECK_PTREQUAL(block3->next, tail);
    CHECK_PTREQUAL(tail->prev, block3);


    /* Reorder the list as though ptr2 was deallocated and reallocated at
     * the same address. */
    head->next = block1;
    block1->prev = head;
    block1->next = block3;
    block3->prev = block1;
    block3->next = block2;
    block2->prev = block3;
    block2->next = tail;
    tail->prev = block2;

    mem_debug_get_map((char *)block1 - 1, block_size+2, map, block_size+2);
    CHECK_INTEQUAL(map[0], -1);
    CHECK_INTEQUAL(map[1], MEM_INFO_UNKNOWN);
    CHECK_INTEQUAL(map[block_size], MEM_INFO_UNKNOWN);
    CHECK_INTEQUAL(map[block_size+1], -1);

    mem_debug_get_map((char *)block2 - 1, block_size+2, map, block_size+2);
    CHECK_INTEQUAL(map[0], -1);
    CHECK_INTEQUAL(map[1], MEM_INFO_UNKNOWN);
    CHECK_INTEQUAL(map[block_size], MEM_INFO_UNKNOWN);
    CHECK_INTEQUAL(map[block_size+1], -1);

    mem_debug_get_map((char *)block3 - 1, block_size+2, map, block_size+2);
    CHECK_INTEQUAL(map[0], -1);
    CHECK_INTEQUAL(map[1], MEM_INFO_UNKNOWN);
    CHECK_INTEQUAL(map[block_size], MEM_INFO_UNKNOWN);
    CHECK_INTEQUAL(map[block_size+1], -1);

    CHECK_PTREQUAL(head->next, block1);
    CHECK_PTREQUAL(block1->prev, head);
    CHECK_PTREQUAL(block1->next, block2);
    CHECK_PTREQUAL(block2->prev, block1);
    CHECK_PTREQUAL(block2->next, block3);
    CHECK_PTREQUAL(block3->prev, block2);
    CHECK_PTREQUAL(block3->next, tail);
    CHECK_PTREQUAL(tail->prev, block3);


    mem_free(ptr1);
    mem_free(ptr2);
    mem_free(ptr3);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_map_multiple_infos)
{
    char *ptr1, *ptr2, *ptr3, *ptr4, *ptr5;
    CHECK_TRUE(ptr1 = mem_alloc(1, 0, 0));
    CHECK_TRUE(ptr2 = mem_alloc(2, 0, 0));
    CHECK_TRUE(ptr3 = mem_alloc(3, 0, 0));
    CHECK_TRUE(ptr4 = mem_alloc(4, 0, 0));
    CHECK_TRUE(ptr5 = mem_alloc(5, 0, 0));

    char *ptr_min = min(min(min(ptr1, ptr2), min(ptr3, ptr4)), ptr5);
    char *ptr_max = max(max(max(ptr1+1, ptr2+2), max(ptr3+3, ptr4+4)), ptr5+5);
    char *range_base = ptr_min - sizeof(BlockInfo);
    int64_t range_size = ptr_max - range_base;

    int8_t map[1];

    mem_debug_set_info(ptr5, MEM_INFO_FONT);
    mem_debug_set_info(ptr4, MEM_INFO_MANAGE);
    mem_debug_set_info(ptr3, MEM_INFO_SOUND);
    mem_debug_set_info(ptr2, MEM_INFO_TEXTURE);
    mem_debug_set_info(ptr1, MEM_INFO_UNKNOWN);
    mem_debug_get_map(range_base, range_size, map, 1);
    CHECK_INTEQUAL(map[0], MEM_INFO_FONT);

    mem_debug_set_info(ptr1, MEM_INFO_FONT);
    mem_debug_set_info(ptr5, MEM_INFO_MANAGE);
    mem_debug_set_info(ptr4, MEM_INFO_SOUND);
    mem_debug_set_info(ptr3, MEM_INFO_TEXTURE);
    mem_debug_set_info(ptr2, MEM_INFO_UNKNOWN);
    mem_debug_get_map(range_base, range_size, map, 1);
    CHECK_INTEQUAL(map[0], MEM_INFO_MANAGE);

    mem_debug_set_info(ptr2, MEM_INFO_FONT);
    mem_debug_set_info(ptr1, MEM_INFO_MANAGE);
    mem_debug_set_info(ptr5, MEM_INFO_SOUND);
    mem_debug_set_info(ptr4, MEM_INFO_TEXTURE);
    mem_debug_set_info(ptr3, MEM_INFO_UNKNOWN);
    mem_debug_get_map(range_base, range_size, map, 1);
    CHECK_INTEQUAL(map[0], MEM_INFO_SOUND);

    mem_debug_set_info(ptr3, MEM_INFO_FONT);
    mem_debug_set_info(ptr2, MEM_INFO_MANAGE);
    mem_debug_set_info(ptr1, MEM_INFO_SOUND);
    mem_debug_set_info(ptr5, MEM_INFO_TEXTURE);
    mem_debug_set_info(ptr4, MEM_INFO_UNKNOWN);
    mem_debug_get_map(range_base, range_size, map, 1);
    CHECK_INTEQUAL(map[0], MEM_INFO_TEXTURE);

    mem_debug_set_info(ptr4, MEM_INFO_FONT);
    mem_debug_set_info(ptr3, MEM_INFO_MANAGE);
    mem_debug_set_info(ptr2, MEM_INFO_SOUND);
    mem_debug_set_info(ptr1, MEM_INFO_TEXTURE);
    mem_debug_set_info(ptr5, MEM_INFO_UNKNOWN);
    mem_debug_get_map(range_base, range_size, map, 1);
    CHECK_INTEQUAL(map[0], MEM_INFO_UNKNOWN);

    mem_free(ptr1);
    mem_free(ptr2);
    mem_free(ptr3);
    mem_free(ptr4);
    mem_free(ptr5);
    return 1;
}

/*************************************************************************/
/************************* TEST_mem_use() tests **************************/
/*************************************************************************/

TEST(test_use_alloc)
{
    const int64_t used = mem_debug_bytes_allocated();
    char *ptr;

    CHECK_TRUE(ptr = mem_alloc(1, 0, 0));
    TEST_mem_use(ptr);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);

    CHECK_PTREQUAL(mem_alloc(1, 0, 0), ptr);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);

    mem_free(ptr);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_use_alloc_small)
{
    const int64_t used = mem_debug_bytes_allocated();
    char *ptr;

    CHECK_TRUE(ptr = mem_alloc(2, 0, 0));
    TEST_mem_use(ptr);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+2);

    CHECK_PTREQUAL(mem_alloc(1, 0, 0), ptr);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);

    mem_free(ptr);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_use_alloc_too_big)
{
    const int64_t used = mem_debug_bytes_allocated();
    char *ptr, *ptr2;

    CHECK_TRUE(ptr = mem_alloc(1, 0, 0));
    TEST_mem_use(ptr);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);

    CHECK_TRUE(ptr2 = mem_alloc(2, 0, 0));
    /* We can't check ptr2 != ptr here because we might end up with the
     * same pointer by coincidence.  Just make sure the number of bytes
     * allocated is correct. */
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+2);

    mem_free(ptr2);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_use_alloc_wrong_alignment)
{
    const int64_t used = mem_debug_bytes_allocated();
    char *ptr, *ptr2;

    CHECK_TRUE(ptr = mem_alloc(1, 1, 0));
    TEST_mem_use(ptr);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);

    int align = 2;
    while ((uintptr_t)ptr % align == 0) {
        align *= 2;
    }
    CHECK_TRUE(ptr2 = mem_alloc(1, align, 0));
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);

    mem_free(ptr2);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_use_alloc_zero_size)
{
    const int64_t used = mem_debug_bytes_allocated();
    char *ptr;

    CHECK_TRUE(ptr = mem_alloc(1, 0, 0));
    TEST_mem_use(ptr);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);

    /* This shouldn't cause the pending use block to be cleared. */
    CHECK_FALSE(mem_alloc(0, 0, 0));
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);

    CHECK_PTREQUAL(mem_alloc(1, 0, 0), ptr);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);

    mem_free(ptr);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_use_realloc_grow)
{
    const int64_t used = mem_debug_bytes_allocated();
    char *ptr, *ptr2;

    CHECK_TRUE(ptr = mem_alloc(1, 0, 0));
    ptr[0] = 1;
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);

    CHECK_TRUE(ptr2 = mem_alloc(2, 0, 0));
    ptr2[0] = ptr2[1] = 2;
    TEST_mem_use(ptr2);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+3);

    CHECK_PTREQUAL(mem_realloc(ptr, 2, 0), ptr2);
    CHECK_INTEQUAL(ptr2[0], 1);
    CHECK_INTEQUAL(ptr2[1], 2);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+2);

    mem_free(ptr2);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_use_realloc_shrink)
{
    const int64_t used = mem_debug_bytes_allocated();
    char *ptr, *ptr2;

    CHECK_TRUE(ptr = mem_alloc(3, 0, 0));
    ptr[0] = ptr[1] = ptr[2] = 1;
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+3);

    CHECK_TRUE(ptr2 = mem_alloc(2, 0, 0));
    ptr2[0] = ptr2[1] = 2;
    TEST_mem_use(ptr2);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+5);

    CHECK_PTREQUAL(mem_realloc(ptr, 1, 0), ptr2);
    CHECK_INTEQUAL(ptr2[0], 1);
    CHECK_INTEQUAL(ptr2[1], 2);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);

    mem_free(ptr2);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_use_realloc_too_big)
{
    const int64_t used = mem_debug_bytes_allocated();
    char *ptr, *ptr2;

    CHECK_TRUE(ptr = mem_alloc(1, 0, 0));
    ptr[0] = 1;
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);

    CHECK_TRUE(ptr2 = mem_alloc(2, 0, 0));
    ptr2[0] = ptr2[1] = 2;
    TEST_mem_use(ptr2);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+3);

    CHECK_TRUE(ptr = mem_realloc(ptr, 3, 0));
    CHECK_INTEQUAL(ptr[0], 1);
#ifdef SIL_MEMORY_DEBUG_FILL_UNUSED
    VALGRIND_MAKE_MEM_DEFINED(ptr+1, 2);
    CHECK_INTEQUAL((uint8_t)ptr[1], 0xBB);
    CHECK_INTEQUAL((uint8_t)ptr[2], 0xBB);
#endif
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+3);

    mem_free(ptr);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_use_realloc_wrong_alignment)
{
    const int64_t used = mem_debug_bytes_allocated();
    char *ptr, *ptr2;

    CHECK_TRUE(ptr = mem_alloc(2, 1, 0));
    ptr[0] = ptr[1] = 2;
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+2);

    int align = 2;
    while ((uintptr_t)ptr % align == 0) {
        align *= 2;
    }
    CHECK_TRUE(ptr2 = mem_alloc(1, align, 0));
    ptr2[0] = 1;
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+3);

    TEST_mem_use(ptr);
    CHECK_TRUE(ptr2 = mem_realloc(ptr2, 2, 0));
    CHECK_INTEQUAL(ptr2[0], 1);
#ifdef SIL_MEMORY_DEBUG_FILL_UNUSED
    VALGRIND_MAKE_MEM_DEFINED(ptr2+1, 1);
    CHECK_INTEQUAL((uint8_t)ptr2[1], 0xBB);
#endif
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+2);

    mem_free(ptr2);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_use_realloc_null)
{
    const int64_t used = mem_debug_bytes_allocated();
    char *ptr;

    CHECK_TRUE(ptr = mem_alloc(1, 0, 0));
    TEST_mem_use(ptr);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);

    /* This should behave just like mem_alloc() and take the use block. */
    CHECK_PTREQUAL(mem_realloc(NULL, 1, 0), ptr);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);

    mem_free(ptr);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_use_realloc_zero_size)
{
    const int64_t used = mem_debug_bytes_allocated();
    char *ptr, *ptr2;

    CHECK_TRUE(ptr = mem_alloc(1, 0, 0));
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);

    CHECK_TRUE(ptr2 = mem_alloc(1, 0, 0));
    TEST_mem_use(ptr2);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+2);

    /* This shouldn't cause the pending use block to be cleared. */
    CHECK_FALSE(mem_realloc(ptr, 0, 0));
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);

    CHECK_PTREQUAL(mem_alloc(1, 0, 0), ptr2);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);

    mem_free(ptr2);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_use_overwrite)
{
    const int64_t used = mem_debug_bytes_allocated();
    char *ptr, *ptr2;

    CHECK_TRUE(ptr = mem_alloc(1, 0, 0));
    TEST_mem_use(ptr);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);

    CHECK_TRUE(ptr2 = mem_alloc(1, 0, 0));
    TEST_mem_use(ptr2);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);

    CHECK_PTREQUAL(mem_alloc(1, 0, 0), ptr2);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);

    mem_free(ptr2);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_use_null)
{
    const int64_t used = mem_debug_bytes_allocated();
    char *ptr;

    CHECK_TRUE(ptr = mem_alloc(1, 0, 0));
    TEST_mem_use(ptr);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);

    TEST_mem_use(NULL);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used);

    CHECK_TRUE(ptr = mem_alloc(1, 0, 0));
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);

    mem_free(ptr);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_use_free)
{
    const int64_t used = mem_debug_bytes_allocated();
    char *ptr;

    CHECK_TRUE(ptr = mem_alloc(1, 0, 0));
    TEST_mem_use(ptr);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);

    mem_free(ptr);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used);

    CHECK_TRUE(ptr = mem_alloc(1, 0, 0));
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);

    mem_free(ptr);
    return 1;
}

/*************************************************************************/
/***************** Tests for other test control routines *****************/
/*************************************************************************/

TEST(test_push_pop_alloc_list)
{
    const int64_t used = mem_debug_bytes_allocated();
    char *ptr, *ptr2, *ptr3, *ptr4;
    void *list, *list2;

    CHECK_TRUE(ptr = mem_alloc(1, 0, 0));
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);

    /* Check that pushing the list gives an empty list. */
    list = TEST_mem_push_alloc_list();
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), 0);

    /* Check that popping restores the original list. */
    TEST_mem_pop_alloc_list(list);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+1);

    /* Check that popping merges with any intervening allocations. */
    list = TEST_mem_push_alloc_list();
    CHECK_TRUE(ptr2 = mem_alloc(1, 0, 0));
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), 1);
    TEST_mem_pop_alloc_list(list);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+2);
    mem_free(ptr2);

    /* Check that nested push/pop work as expected. */
    list = TEST_mem_push_alloc_list();
    CHECK_TRUE(ptr2 = mem_alloc(1, 0, 0));
    CHECK_TRUE(ptr3 = mem_alloc(1, 0, 0));
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), 2);
    list2 = TEST_mem_push_alloc_list();
    CHECK_TRUE(ptr4 = mem_alloc(1, 0, 0));
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), 1);
    TEST_mem_pop_alloc_list(list2);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), 3);
    TEST_mem_pop_alloc_list(list);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), used+4);
    mem_free(ptr4);
    mem_free(ptr3);
    mem_free(ptr2);

    /* Test that pushing and popping an empty list work. */
    list = TEST_mem_push_alloc_list();
    list2 = TEST_mem_push_alloc_list();
    CHECK_PTREQUAL(list2, NULL);
    TEST_mem_pop_alloc_list(list2);
    TEST_mem_pop_alloc_list(list);

    mem_free(ptr);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fail_after)
{
    char *ptr, *ptr2, *ptr3;

    /* Check normal behavior for every == 0. */
    TEST_mem_fail_after(1, 0, 0);
    CHECK_TRUE(ptr = mem_alloc(1, 0, 0));
    CHECK_FALSE(mem_alloc(1, 0, 0));
    CHECK_TRUE(ptr2 = mem_alloc(1, 0, 0));
    CHECK_TRUE(ptr3 = mem_alloc(1, 0, 0));
    mem_free(ptr);
    mem_free(ptr2);
    mem_free(ptr3);

    /* Check normal behavior for every == 1. */
    TEST_mem_fail_after(1, 1, 0);
    CHECK_TRUE(ptr = mem_alloc(1, 0, 0));
    CHECK_FALSE(mem_alloc(1, 0, 0));
    CHECK_FALSE(mem_alloc(1, 0, 0));
    mem_free(ptr);

    /* Check normal behavior for every > 1. */
    TEST_mem_fail_after(1, 3, 0);
    CHECK_TRUE(ptr = mem_alloc(1, 0, 0));
    CHECK_FALSE(mem_alloc(1, 0, 0));
    CHECK_TRUE(ptr2 = mem_alloc(1, 0, 0));
    CHECK_TRUE(ptr3 = mem_alloc(1, 0, 0));
    CHECK_FALSE(mem_alloc(1, 0, 0));
    mem_free(ptr);
    mem_free(ptr2);
    mem_free(ptr3);

    /* Check that realloc() also fails. */
    TEST_mem_fail_after(2, 0, 0);
    CHECK_TRUE(ptr = mem_alloc(1, 0, 0));
    CHECK_TRUE(ptr = mem_realloc(ptr, 2, 0));
    CHECK_FALSE(mem_realloc(ptr, 3, 0));
    CHECK_TRUE(ptr2 = mem_alloc(1, 0, 0));
    CHECK_TRUE(ptr2 = mem_realloc(ptr2, 2, 0));
    mem_free(ptr);
    mem_free(ptr2);

    /* Check that strdup() also fails. */
    TEST_mem_fail_after(2, 0, 0);
    CHECK_TRUE(ptr = mem_alloc(1, 0, 0));
    CHECK_TRUE(ptr = mem_realloc(ptr, 2, 0));
    CHECK_FALSE(mem_strdup("test", 0));
    CHECK_TRUE(ptr2 = mem_alloc(1, 0, 0));
    CHECK_TRUE(ptr3 = mem_alloc(1, 0, 0));
    mem_free(ptr);
    mem_free(ptr2);
    mem_free(ptr3);

    /* Check that shrinking realloc() and free() calls aren't counted. */
    TEST_mem_fail_after(4, 0, 0);
    CHECK_TRUE(ptr = mem_alloc(2, 0, 0));
    CHECK_TRUE(ptr2 = mem_alloc(1, 0, 0));
    CHECK_TRUE(ptr = mem_realloc(ptr, 1, 0));
    CHECK_FALSE(mem_realloc(ptr, 0, 0));
    mem_free(ptr2);
    CHECK_TRUE(ptr = mem_alloc(1, 0, 0));
    CHECK_TRUE(ptr2 = mem_alloc(1, 0, 0));
    CHECK_FALSE(mem_alloc(1, 0, 0));
    CHECK_TRUE(ptr3 = mem_alloc(1, 0, 0));
    mem_free(ptr);
    mem_free(ptr2);
    mem_free(ptr3);

    /* Check that shrinking realloc() doesn't fail. */
    TEST_mem_fail_after(2, 1, 0);
    CHECK_TRUE(ptr = mem_alloc(1, 0, 0));
    CHECK_TRUE(ptr = mem_realloc(ptr, 2, 0));
    CHECK_FALSE(mem_realloc(ptr, 3, 0));
    CHECK_TRUE(ptr = mem_realloc(ptr, 2, 0));
    CHECK_TRUE(ptr = mem_realloc(ptr, 1, 0));
    CHECK_FALSE(mem_alloc(1, 0, 0));
    mem_free(ptr);

    /* Check that failing allocation calls are counted. */
    TEST_mem_fail_after(1, 0, 0);
    CHECK_FALSE(mem_alloc(0, 0, 0));
    CHECK_FALSE(mem_alloc(1, 0, 0));
    CHECK_TRUE(ptr = mem_alloc(1, 0, 0));
    mem_free(ptr);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fail_on_shrink)
{
    char *ptr;

    /* Check that shrinking realloc() fails with the flag set. */
    TEST_mem_fail_after(1, 0, 1);
    CHECK_TRUE(ptr = mem_alloc(2, 0, 0));
    CHECK_FALSE(mem_realloc(ptr, 1, 0));
    mem_free(ptr);

    /* Check that shrinking realloc() calls are counted with the flag set. */
    TEST_mem_fail_after(2, 0, 1);
    CHECK_TRUE(ptr = mem_alloc(3, 0, 0));
    CHECK_TRUE(ptr = mem_realloc(ptr, 2, 0));
    CHECK_FALSE(mem_realloc(ptr, 1, 0));
    mem_free(ptr);

    /* Check that the flag can be disabled. */
    TEST_mem_fail_after(2, 0, 0);
    CHECK_TRUE(ptr = mem_alloc(2, 0, 0));
    CHECK_TRUE(ptr = mem_realloc(ptr, 3, 0));
    CHECK_TRUE(ptr = mem_realloc(ptr, 1, 0));
    CHECK_FALSE(mem_alloc(1, 0, 0));
    mem_free(ptr);

    return 1;
}

/*************************************************************************/
/****************************** Fuzz tests *******************************/
/*************************************************************************/

TEST(fuzz_mem_alloc)
{
    void *ptr[100];
    int64_t sizes[100];
    const int NUM_ITERATIONS = 10000;
    const int GET_MAP_INTERVAL = 100;
    const float ALLOC_CHANCE = 1/3.0f;
    const float REALLOC_CHANCE = 1/3.0f;
    const int MAX_ALLOC_SIZE = 100000;
    const float NO_ALIGN_CHANCE = 0.5f;
    const int MAX_ALIGN_SHIFT = 8;

    srandom_env();
    const uint32_t seed = urandom32();
    DLOG("Random seed: %u", seed);
    srandom32(seed);

    for (int i = 0; i < lenof(ptr); i++) {
        ptr[i] = NULL;
        sizes[i] = 0;
    }

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        const int index = random32() % lenof(ptr);
        const float action_selector = frandomf();
        const long size = random32() % (MAX_ALLOC_SIZE + 1);
        const float align_selector = frandomf();
        const long align_random = random32();
        const int flags = random32() & 0x7;
        int align;
        if (align_selector < NO_ALIGN_CHANCE) {
            align = 0;
        } else {
            align = 1 << (align_random % (MAX_ALIGN_SHIFT + 1));
        }

        if (action_selector < ALLOC_CHANCE) {
            if (ptr[index]) {
                mem_free(ptr[index]);
            }
            ptr[index] = mem_alloc(size, align, flags);
            sizes[index] = size;
            if (size == 0) {
                if (ptr[index]) {
                    FAIL("Seed %u iteration %d: mem_alloc(%lld,%d,%d)"
                         " returned %p but should have failed", seed, i,
                         (long long)size, align, flags, ptr[index]);
                }
            } else {
                if (!ptr[index]) {
                    FAIL("Seed %u iteration %d: mem_alloc(%lld,%d,%d)"
                         " failed but should have succeeded", seed, i,
                         (long long)size, align, flags);
                }
            }
        } else if (action_selector < ALLOC_CHANCE + REALLOC_CHANCE) {
            void *old_ptr = ptr[index];
            ptr[index] = mem_realloc(old_ptr, size, flags);
            sizes[index] = size;
            if (size == 0) {
                if (ptr[index]) {
                    FAIL("Seed %u iteration %d: mem_realloc(%p,%lld,%d)"
                         " returned %p but should have failed", seed, i,
                         old_ptr, (long long)size, flags, ptr[index]);
                }
            } else {
                if (!ptr[index]) {
                    FAIL("Seed %u iteration %d: mem_alloc(%p,%lld,%d)"
                         " failed but should have succeeded", seed, i,
                         old_ptr, (long long)size, flags);
                }
            }
        } else {
            mem_free(ptr[index]);
            ptr[index] = NULL;
            sizes[index] = 0;
        }

        int64_t total_size = 0;
        for (int j = 0; j < lenof(sizes); j++) {
            total_size += sizes[j];
        }
        const int64_t bytes_allocated = mem_debug_bytes_allocated();
        if (bytes_allocated != total_size) {
            FAIL("Seed %u iteration %d: bytes_allocated was %lld but should"
                 " have been %lld", seed, i, (long long)bytes_allocated,
                 (long long)total_size);
        }

        if ((i+1) % GET_MAP_INTERVAL == 0) {
            int8_t map[1];
            mem_debug_get_map((void *)0, (int64_t)0x7FFFFFFFFFFFFFFFLL, map, 1);
            const int expected = (total_size==0 ? -1 : 0);
            if (map[0] != expected) {
                FAIL("Seed %u iteration %d: map[0] was %d but should have"
                 " been %d", seed, i, map[0], expected);
            }
        }
    }  // for (int i = 0; i < NUM_ITERATIONS; i++) {

    for (int i = 0; i < lenof(ptr); i++) {
        mem_free(ptr[i]);
    }
    return 1;
}

/*************************************************************************/
/*************************************************************************/
