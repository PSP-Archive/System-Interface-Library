/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/configs/psp.h: Configuration header for the PSP.
 */

#ifndef SIL_CONFIGS_PSP_H
#define SIL_CONFIGS_PSP_H

/*************************************************************************/
/*************************************************************************/

#define SIL_PLATFORM_PSP
#define SIL_SYSTEM_COMMON_HEADER  "sysdep/psp/common.h"
#define SIL_SYSTEM_MATH_HEADER    "sysdep/psp/math.h"

#define SIL_ARCH_MIPS
#define SIL_ARCH_MIPS_32
#define IS_LITTLE_ENDIAN

#define HAVE_STRDUP
/* Newlib has strcasecmp() but not in <strings.h>, so use our own instead. */
//#define HAVE_STRCASECMP
//#define STRNICMP_SIZE_T  size_t

/* We use a custom memory allocator on the PSP. */
#define SIL_MEMORY_CUSTOM

/* Unpack resources in reverse order to avoid memory fragmentation. */
#define SIL_RESOURCE_SYNC_IN_REVERSE

/* Texture data must be 64-byte aligned. */
#define SIL_TEXTURE_ALIGNMENT  64

/*-----------------------------------------------------------------------*/

/*
 * The following PSP-specific configuration options are available and may
 * be enabled by defining the corresponding preprocessor symbol:
 *
 * - SIL_PLATFORM_PSP_CXX_CONSTRUCTOR_HACK:  Define to support memory
 *   allocation in constructors for static instances of C++ classes.
 *
 *   In C++, constructors for static objects are executed before user code
 *   (i.e., main()) has a chance to run.  If any such constructors attempt
 *   to allocate memory, the allocations will fail because the memory pools
 *   used by the PSP memory allocator (normally initialized by main()) have
 *   not yet been created.
 *
 *   When this symbol is defined, the malloc(), calloc(), and realloc()
 *   functions will check on each call whether the memory pools have been
 *   initialized, and perform initialization if not.  This comes with a
 *   small cost in execution time per memory allocation.
 *
 * - SIL_PLATFORM_PSP_MEMORY_POOL_SIZE:  Define to the desired size (in
 *   bytes) of the main memory pool used for memory allocations.  A
 *   positive value specifies that the memory pool must be allocated to
 *   that size; if the specified amount of memory is not available in a
 *   contiguous block at startup, the program will abort.  A zero or
 *   negative value means to allocate as much as possible while leaving at
 *   least the given amount of memory available to the system, for use in
 *   thread stacks (for example).  There is no default; this MUST be
 *   defined by the build system.
 *
 *  - SIL_PLATFORM_PSP_MEMORY_POOL_TEMP_SIZE:  Define to the desired size
 *    (in bytes) of the memory pool used for temporary (MEM_ALLOC_TEMP)
 *    memory allocations.  This value must be either positive or zero; if
 *    zero, no temporary pool will be created, and temporary allocations
 *    will go to the main pool.  There is no default; this MUST be defined
 *    by the build system.
 *
 * - SIL_PLATFORM_PSP_MODULE_NAME:  Define to the module name for the
 *   executable.  This is typically defined by the build environment.  The
 *   default is "user_module".
 *
 * - SIL_PLATFORM_PSP_REPLACE_MALLOC:  Define to replace the standard
 *   malloc() family of functions with versions that call the SIL mem_*()
 *   functions.
 *
 * - SIL_PLATFORM_PSP_STACK_SIZE:  Define to the desired size (in bytes) of
 *   the main thread's stack.  There is no default; this MUST be defined by
 *   the build system.
 *
 * - SIL_PLATFORM_PSP_USERDATA_MAX_SIZE:  Define to the maximum size (in
 *   bytes) for any save file or settings file stored by the program.
 *   Note that a buffer of this size will be allocated during each load
 *   operation, regardless of the actual size of the file being loaded.
 *   Defaults to 100000.
 *
 * - SIL_PLATFORM_PSP_USERDATA_*_FORMAT, SIL_PLATFORM_PSP_USERDATA_*_FILENAME:
 *   Define these strings to change the pathnames used for save files.  In
 *   format strings, "%s" is replaced by the program name passed to
 *   userdata_set_program_name(), and "%d" is replaced by the save file
 *   number.  All strings must be double-quoted like regular C strings.
 *   The individual strings (omitting the common SIL_PLATFORM_PSP_USERDATA_
 *   prefix on the symbol names) are:
 *      - SAVEFILE_DIR_FORMAT:  Format string giving the directory name for
 *        a save file.  This _must_ include both %s and %d tokens.
 *        Defaults to "%s_%03d".
 *      - SAVEFILE_FILENAME:  Data filename for a save file.  Defaults to
 *        "save.bin".
 *      - SETTINGS_DIR_FORMAT:  Format string giving the directory name for
 *        the settings file.  This may include a %s token.  Defaults to
 *        "%s_Settings".
 *      - SETTINGS_FILENAME:  Data filename for the settings file.
 *        Defaults to "settings.bin".
 *      - STATS_DIR_FORMAT:  Format string giving the directory name for
 *        the stats file.  This may include a %s token.  Defaults to
 *        "%s_Stats".
 *      - SETTINGS_FILENAME:  Data filename for the stats file.  Defaults
 *        to "stats.bin".
 */

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_CONFIGS_PSP_H
