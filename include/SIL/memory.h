/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/memory.h: Header for extended memory management.
 */

#ifndef SIL_MEMORY_H
#define SIL_MEMORY_H

/*
 * This header exports an extended memory allocation interface, consisting
 * of the following functions:
 *
 *     mem_alloc()   -- allocate a block of memory
 *     mem_realloc() -- resize a block of memory
 *     mem_free()    -- free a block of memory
 *     mem_strdup()  -- duplicate a string using mem_alloc()
 *     mem_avail()   -- return the total amount of free memory available
 *     mem_contig()  -- return the size of the largest allocatable block
 *
 * All functions are thread-safe (will work properly from multiple threads)
 * as long as, and to the same degree that, the sys_mem_*() implementations
 * are also thread-safe.
 *
 * These functions differ from the malloc() family of functions provided
 * by the standard C library in that they allow the caller to control
 * where the memory is allocated.  While the location of allocated memory
 * is typically irrelevant on machines such as modern PCs with virtual
 * memory capabilities, proper operation in less-endowed environments
 * often requires careful management of allocated memory, which the
 * standard malloc()-family functions do not permit.
 *
 * It is also sometimes necessary to allocate a block of memory with a
 * specific address alignment, such as when the memory will be passed to
 * hardware which can only access memory on specific alignment boundaries.
 * malloc() does not allow the caller to specify the alignment, while these
 * functions do.  (POSIX provides the posix_memalign() function which has
 * this capability, but we do not assume the availability of POSIX.)
 *
 * In addition to the above, debugging versions of the memory allocation
 * functions are defined in debug mode: debug_mem_alloc(),
 * debug_mem_realloc(), debug_mem_free(), and debug_mem_strdup().  These
 * functions are primarily for internal use, and allow various system
 * utilities to pass call site and allocation purpose information from
 * the caller to the memory allocator.
 *
 * The memory management code also includes some debugging-specific
 * functionality, activated only when DEBUG is defined:
 *
 * - All allocations are tracked, to help track memory use and find leaks.
 *   (This incurs a small amount of additional overhead per allocated
 *   block: 24 bytes on 32-bit systems, and 40 bytes on 64-bit systems.)
 *   Information on current allocations can be accessed via the
 *   mem_debug_bytes_allocated() and mem_debug_report_allocs() functions.
 *
 * - Allocations requesting a specific alignment will be adjusted so the
 *   starting address of the block is unaligned with respect to the next
 *   higher power of two, to help catch alignment errors.  Allocations
 *   which do not request a specific alignment will be unaligned with
 *   respect to twice the default allocation alignment.
 *
 * - Memory allocated without MEM_ALLOC_CLEAR will be filled with 0xBB,
 *   and memory freed will be filled with 0xDD, to help in locating use of
 *   uninitialized memory or use-after-free errors.
 */

EXTERN_C_BEGIN

/*************************************************************************/
/****************** Common memory management constants *******************/
/*************************************************************************/

/**
 * MEM_ALLOC_*:  Flags for mem_alloc(), mem_realloc(), and mem_strdup()
 * for controlling memory allocation behavior.
 *
 * Note that many object creation functions take these flags as well.
 * In such cases, MEM_ALLOC_CLEAR will typically have no effect, since the
 * flags are only used to control placement of internal data structures.
 * Functions which do honor MEM_ALLOC_CLEAR, such as texture_create(),
 * will specify its effect in their documentation.
 */

/* Clear newly-allocated memory.  This applies to both memory returned by
 * mem_alloc() and the newly-allocated part of a memory block which was
 * expanded by mem_realloc(). */
#define MEM_ALLOC_CLEAR  (1<<0)

/* Allocate from the top of the free memory pool, rather than from the
 * bottom.  Ignored on systems which do not allocate from a fixed-size
 * pool. */
#define MEM_ALLOC_TOP    (1<<1)

/* Assume that the allocated memory block will soon be freed, and allocate
 * it so as to minimize fragmentation with respect to allocations which do
 * not include this flag. */
#define MEM_ALLOC_TEMP   (1<<2)

/*-----------------------------------------------------------------------*/

/**
 * MEM_AVAIL_UNKNOWN:  Return value from mem_avail() and mem_contig()
 * indicating that the amount of available memory is unknown.
 */
#define MEM_AVAIL_UNKNOWN  ((int64_t)-1)

/*************************************************************************/
/********************** Memory management interface **********************/
/*************************************************************************/

/**
 * mem_alloc:  Allocate a block of memory.  If size == 0, does nothing and
 * returns NULL.
 *
 * [Parameters]
 *     size: Size of block to allocate, in bytes.
 *     align: Desired address alignment, in bytes (must be a power of 2).
 *         If zero, the allocated memory will be suitably aligned for any
 *         basic data type.
 *     flags: Operation flags (MEM_ALLOC_*).
 * [Return value]
 *     Pointer to allocated block, or NULL on error.
 */
#if defined(__GNUC__) && __GNUC__ >= 3
# ifdef SIL_MEMORY_FORBID_MALLOC
#  undef malloc
# endif
__attribute__((malloc))
# ifdef SIL_MEMORY_FORBID_MALLOC
#  define malloc  _sil__DO_NOT_USE_malloc
# endif
#
#endif
extern void *mem_alloc(int64_t size, int align, int flags);

/**
 * mem_free:  Free a block of memory.  Does nothing if ptr == NULL.
 *
 * [Parameters]
 *     ptr: Pointer to block to free.
 */
extern void mem_free(void *ptr);

/**
 * mem_realloc:  Expand or shrink a block of memory.  If the operation
 * fails, the original memory block is left unchanged (it is not freed).
 * The alignment value specified at mem_alloc() time is used for the new
 * block as well.
 *
 * If size == 0, this function behaves identically to mem_free(ptr),
 * returning a value of NULL.  If size != 0 && ptr == NULL, this function
 * behaves identically to mem_alloc(size,0,flags).
 *
 * If either or both of the MEM_ALLOC_TOP and MEM_ALLOC_TEMP flags are
 * different in value from those given in the mem_alloc() or mem_realloc()
 * call that allocated the original block, and the new flags would result
 * in a different allocation pattern, mem_realloc() will allocate a new
 * block with the new flags and copy the data into it.  (Thus, changing
 * flags should be avoided for optimum performance.)
 *
 * This function is not guaranteed under any circumstances to return the
 * same pointer as was passed in, and is likewise not guaranteed to succeed
 * even on a shrink operation.
 *
 * [Parameters]
 *     ptr: Pointer to block to resize, or NULL to allocate a new block.
 *     size: New size for the block, in bytes, or zero to free the block.
 *     flags: Operation flags (MEM_ALLOC_*, but MEM_ALLOC_CLEAR is ignored).
 * [Return value]
 *     Pointer to resized block; NULL on error or if size == 0.
 */
extern void *mem_realloc(void *ptr, int64_t size, int flags);

/**
 * mem_strdup:  Duplicate a string into a block of memory newly allocated
 * with mem_alloc().
 *
 * [Parameters]
 *     str: String to duplicate.
 *     flags: Operation flags (MEM_ALLOC_*).
 * [Return value]
 *     Pointer to duplicated string, or NULL on error.
 */
extern char *mem_strdup(const char *str, int flags);

/**
 * mem_avail:  Return the total amount of memory available to be allocated
 * with mem_alloc().  If the system does not provide this information,
 * MEM_AVAIL_UNKNOWN (a negative value) is returned instead.
 *
 * The return value from this function should be considered an estimate
 * only; memory fragmentation, allocation overhead, and other factors will
 * reduce the actual amount of memory that can be allocated.
 *
 * If MEM_ALLOC_TEMP is specified in flags, then:
 *    - If the system uses a separate memory pool for allocations with
 *      the MEM_ALLOC_TEMP flag, the amount of memory available in that
 *      pool is returned.  In this case, mem_avail(0) returns the amount
 *      of memory available in the non-temporary pool only.
 *    - If the system does _not_ use such a separate pool, zero is returned.
 * Other MEM_ALLOC_* flags are ignored.
 *
 * [Return value]
 *     Total amount of memory available, in bytes.
 */
extern int64_t mem_avail(int flags);

/**
 * mem_contig:  Return the size of the largest single block for which a
 * call to mem_alloc(size,0,flags) will currently succeed.  If the system
 * does not provide this information, MEM_AVAIL_UNKNOWN (a negative value)
 * is returned instead.
 *
 * The flags parameter is treated as for mem_avail().
 *
 * [Return value]
 *     Maximum allocatable block size, in bytes.
 */
extern int64_t mem_contig(int flags);

/*************************************************************************/
/************************** Debugging interface **************************/
/*************************************************************************/

#ifdef DEBUG

/*-----------------------------------------------------------------------*/

/**
 * MEM_INFO_*:  Constants indicating memory usage types, for use as the
 * "info" parameter to debug_mem_alloc() calls.
 */
enum {
    MEM_INFO_UNKNOWN = 0,
    MEM_INFO_FONT,      // Memory holding font data.
    MEM_INFO_MANAGE,    // Memory used for internal data management.
    MEM_INFO_SOUND,     // Memory holding sound data.
    MEM_INFO_TEXTURE,   // Memory holding texture data.
};

/*-----------------------------------------------------------------------*/

/*
 * Debugging versions of the basic memory management functions.  These
 * take the additional parameters:
 *     file: Source filename from which the call occurred
 *     line: Source line number at which the call occurred
 *     info: Intended usage of this block (MEM_INFO_*)
 *
 * When DEBUG is defined, the standard mem_*() functions are redefined to
 * call these functions using __FILE__ and __LINE__.  However, routines
 * which perform memory allocation on behalf of other code may call these
 * debug_mem_*() functions directly to pass down their caller's information.
 * (The resource management code makes use of this, for example.)  In this
 * case, the caller must ensure in particular that "file" points to a
 * literal constant string, as the string content is not copied.
 *
 * When DEBUG is not defined, the debugging versions are defined as macros
 * that simply strip out the file/line/info parameters and call the regular
 * allocation functions.
 */

/* Debugging versions of the functions (no special implementation here). */
extern void *debug_mem_alloc(int64_t size, int align, int flags,
                             const char *file, int line, int info);
extern void debug_mem_free(void *ptr, const char *file, int line);
extern void *debug_mem_realloc(void *ptr, int64_t size, int flags,
                               const char *file, int line, int info);
extern char *debug_mem_strdup(const char *str, int flags,
                              const char *file, int line, int info);

# define mem_alloc(size,align,flags) \
    debug_mem_alloc((size), (align), (flags), __FILE__, __LINE__, \
                    MEM_INFO_UNKNOWN)
# define mem_free(ptr) \
    debug_mem_free((ptr), __FILE__, __LINE__)
# define mem_realloc(ptr,size,flags) \
    debug_mem_realloc((ptr), (size), (flags), __FILE__, __LINE__, \
                      MEM_INFO_UNKNOWN)
# define mem_strdup(str,flags) \
    debug_mem_strdup((str), (flags), __FILE__, __LINE__, MEM_INFO_UNKNOWN)

/**
 * mem_debug_set_info:  Set the usage information for the given memory
 * block, which must have been a block previously returned by mem_alloc()
 * or mem_realloc().
 *
 * If not building in debug mode, this function is a no-op.
 *
 * [Parameters]
 *     ptr: Memory block pointer.
 *     info: Intended usage of the block (MEM_INFO_*).
 */
extern void mem_debug_set_info(void *ptr, int info);

/*-----------------------------------------------------------------------*/

/**
 * mem_debug_bytes_allocated:  Return the total number of bytes currently
 * allocated via the mem_*() functions, excluding system and memory
 * management overhead.
 *
 * Only available in debug mode (i.e., when DEBUG is defined).
 *
 * [Return value]
 *     Total number of bytes allocated.
 */
extern int64_t mem_debug_bytes_allocated(void);

/**
 * mem_debug_report_allocs:  Output (via DLOG()) a list of all allocated
 * memory blocks.
 *
 * Only available in debug mode (i.e., when DEBUG is defined).
 */
extern void mem_debug_report_allocs(void);

/**
 * mem_debug_get_map:  Return information about memory allocated in the
 * given range.
 *
 * The memory range is divided into bufsize equal chunks, with buf[0]
 * corresponding to the first (size/bufsize) bytes of the memory range,
 * buf[1] corresponding to the next (size/bufsize) bytes, and so on.
 * Each element of buf[] is set to reflect the state of the corresponding
 * subrange of memory:
 *    - If the entire subrange is free, the byte is set to -1.
 *    - Otherwise, the element is set to the MEM_INFO_* value which has the
 *         greatest amount of memory allocated in that subrange.  (If two
 *         usage types have the same amount of memory allocated, the one
 *         returned is unspecified.)
 *
 * Only available in debug mode.
 *
 * [Parameters]
 *     base: Base of memory range for which to get information.
 *     size: Size of memory range for which to get information, in bytes.
 *     buf: Buffer in which to store memory allocation information.
 *     bufsize: Size of buf, in bytes.
 */
extern void mem_debug_get_map(const void *base, int64_t size,
                              int8_t *buf, int bufsize);

/*-----------------------------------------------------------------------*/

#else  // !DEBUG

/* Rewrite debug_mem_*() calls to just call the regular functions. */
# define debug_mem_alloc(size,align,flags,file,line,info) \
    mem_alloc((size), (align), (flags))
# define debug_mem_free(ptr,file,line) \
    mem_free((ptr))
# define debug_mem_realloc(ptr,size,flags,file,line,info) \
    mem_realloc((ptr), (size), (flags))
# define debug_mem_strdup(str,flags,file,line,info) \
    mem_strdup((str), (flags))

/* Turn mem_debug_set_info() into a no-op. */
# define mem_debug_set_info(ptr,info)  /*nothing*/

#endif

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_MEMORY_H
