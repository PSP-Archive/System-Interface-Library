/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/base.h: Base header file for all source code.
 */

#ifndef SIL_BASE_H  // Protect the header against double-inclusion.
#define SIL_BASE_H

/*
 * This header sets up the basic compilation environment shared by all
 * source code used with this engine.  It must be included (either
 * directly, or indirectly via another header) before any other code or
 * declarations.
 *
 * The environment set up by this header includes the following:
 *
 * - Preprocessor symbols for global configuration options.
 *
 * - Standard declarations from <errno.h>, <limits.h>, <stdarg.h>,
 *   <stddef.h>, <stdint.h>, <stdlib.h>, and <string.h>.  For C++ source
 *   files, if SIL_USE_STL_ALGORITHM is defined, <algorithm> is also
 *   included; this in turn includes <math.h> (and due to certain
 *   platform-specific issues, we explicitly include <SIL/math.h> before
 *   <algorithm>).  No other system headers are included by this file.
 *   Note that:
 *      - <stdio.h> is intentionally not included because its I/O
 *        interfaces are not supported on some platforms.
 *      - system() is intentionally hidden because it is not available
 *        on some platforms.
 *      - rand(), random(), srand(), and srandom() are intentionally
 *        hidden to help avoid ABI compatibility issues on Android (see
 *        README-android.txt), and to help avoid cross-platform
 *        behavioral differences due to differing implementations of the
 *        system random-number generator.  Use the functions declared in
 *        the <SIL/random.h> header to obtain random numbers.
 *
 * - stricmp() and strnicmp(), for case-insensitive string comparisons
 *   with a consistent name (equivalent to the functions variously named
 *   _str[n]icmp(), str[n]casecmp(), or _str[n]casecmp() on some platforms).
 *
 * - EXTERN_C, EXTERN_C_BEGIN and EXTERN_C_END, as shortcuts for
 *   'extern "C"' and 'extern "C" { ... }' that do not need to be
 *   protected by #ifdef __cplusplus.
 *
 * - The following macros for controlling compilation and providing hints:
 *      - IS_GCC() and IS_CLANG(), for testing compiler versions in
 *           preprocessor conditionals.
 *      - ALIGNED(), to force alignment of a variable or structure member.
 *      - CONST_FUNCTION, PURE_FUNCTION, FORMAT, NORETURN, and NOINLINE,
 *           to specify function attributes.
 *      - UNUSED and DEBUG_USED, to suppress warnings about unused variables.
 *      - LIKELY() and UNLIKELY(), to provide branch hints to the compiler.
 *      - BARRIER() and DEBUG_MATH_BARRIER(), to define memory barriers.
 *      - FALLTHROUGH, to explicitly mark the end of a switch case as
 *           falling through to the next case.
 *      - UNREACHABLE, to indicate that a code location is unreachable.
 *
 * - The following convenience macros and inline functions:
 *      - min() and max(), to take the minimum or maximum value.
 *      - lbound(), ubound(), and bound(), to bound a value within a range.
 *      - align_up() and align_down(), to round a value to a multiple of
 *           another value.
 *      - lenof(), to return the length of an array in elements.
 *      - mem_clear(), to clear a block of memory to zero.
 *      - mem_fill32(), to fill a block of memory with a 32-bit value.
 *           (mem_fill8() is also available for consistency of naming,
 *           though it is merely an alias for memset().)
 *      - strformat() and vstrformat(), as well as _check wrappers that
 *           return overflow status, for text formatting without stdio.
 *      - DLOG(), to output a formatted debug message.
 *      - ASSERT() and PRECOND(), to check for logic errors at runtime.
 *      - STATIC_ASSERT(), to check for logic errors when compiling.
 *
 * See the documentation within this file for details.
 *
 * Within this library, the values "true" and "false" in reference to a
 * value (such as a function parameter or return value) have the the same
 * semantics as the operation "value != 0".  Specifically:
 *
 * - "False" always means the value zero.  To pass a false value to a
 *   function, pass the integer 0; a function returning false will return
 *   the integer 0.
 *
 * - "True" means any nonzero value on input, and the value 1 on output.
 *   If a function expects a truth (Boolean) value for a parameter, any
 *   nonzero value is considered to be true; a function returning true
 *   will return the integer 1.  (However, it is of course bad practice to
 *   treat a truth value as an integer!  If you're tempted, say "premature
 *   optimization is the root of all evil" three times in a row.)
 */

/*************************************************************************/
/********************* System-specific configuration *********************/
/*************************************************************************/

/*
 * (The documentation in this section only covers internal implementation
 * details of the library; library users need not read it.)
 *
 * Each supported system must include an appropriate header from the
 * configs/ subdirectory to define the following symbols.  The header
 * filename should be specified by the build environment using the
 * SIL_SYSTEM_CONFIG_HEADER preprocessor symbol; for example, if building
 * with GCC, give -DSIL_SYSTEM_CONFIG_HEADER=\"configs/filename.h\" on
 * the GCC command line (the backslashes are necessary to ensure that the
 * quote characters are passed to GCC and not stripped by the shell).
 *
 * The configuration header should not include any standard C/C++ headers
 * on its own, but may include system-specific headers provided that they
 * do not interfere with the environment provided by this file.
 *
 * The following symbols are required to be either defined or undefined
 * as appropriate for the compilation environment:
 *
 * - HAVE_STRDUP:  Define this if the system has strdup() as defined by
 *   POSIX.
 *
 * - HAVE_STRICMP:  Define this if the system has stricmp() and strnicmp(),
 *   and they act like strcmp() and strncmp() on lowercased versions of
 *   their parameter strings.  (Only define this if _both_ functions are
 *   available and behave as specified.)
 *
 * - HAVE__STRICMP:  Define this if the system has _stricmp() and
 *   _strnicmp(), and they behave as described above for stricmp() and
 *   strnicmp().  (Note the double underscore in the preprocessor symbol.)
 *
 * - HAVE_STRCASECMP:  Define this if the system has strcasecmp() and
 *   strncasecmp(), and they behave as described above for stricmp() and
 *   strnicmp().  This implies the presence of the <strings.h> header.
 *
 * - STRNICMP_SIZE_T:  If any of HAVE_STRICMP, HAVE__STRICMP, or
 *   HAVE_STRCASECMP are defined, define this to the type of the third
 *   (length) parameter to strnicmp().  If none of the above are defined,
 *   this is optional, and the type of the length parameter will default
 *   to "unsigned int" if this is left undefined.
 *
 * - IS_BIG_ENDIAN, IS_LITTLE_ENDIAN:  Define _exactly one_ of these to
 *   indicate the endianness of the execution environment.
 *
 * The header may redefine any of the following macros if the default
 * definition in this file is inappropriate or missing: ALIGNED,
 * CONST_FUNCTION, PURE_FUNCTION, FORMAT, NORETURN, NOINLINE, LIKELY,
 * UNLIKELY, BARRIER, and DEBUG_MATH_BARRIER.
 */

#if !defined(SIL_SYSTEM_CONFIG_HEADER)
# error Please define SIL_SYSTEM_CONFIG_HEADER in the build environment.
#endif

#include SIL_SYSTEM_CONFIG_HEADER

#if !defined(STRNICMP_SIZE_T)
# if defined(HAVE_STRICMP) || defined(HAVE__STRICMP) || defined(HAVE_STRCASECMP)
#  error STRNICMP_SIZE_T not defined by system configuration header!
# else
#  define STRNICMP_SIZE_T  unsigned int
# endif
#endif

#if !defined(IS_BIG_ENDIAN) && !defined(IS_LITTLE_ENDIAN)
# error Endianness not defined by system configuration header!
#endif

/*************************************************************************/
/********************* Global configuration options **********************/
/*************************************************************************/

/*
 * The following preprocessor symbols may be defined (by either the build
 * system or the system-specific configuration header) to affect the
 * operation of SIL.  Note that many of these can be set by overriding
 * default values declared in build/common/config.mk or the system-specific
 * build/.../build.mk files.
 *
 * ==================== Program-wide behavior controls ====================
 *
 * - DEBUG:  If defined, SIL will be built in debugging mode.  This
 *   enables output of debugging messages via the DLOG() facility, and
 *   causes the ASSERT() and PRECOND() macros to abort the program if a
 *   check fails.
 *
 * - SIL_INCLUDE_TESTS:  If defined, internal test routines (under the
 *   "src/test" subdirectory) will be compiled into the program.  Tests
 *   are run by calling the run_internal_tests() function declared in
 *   <SIL/test.h>.  SIL_INCLUDE_TESTS must not be defined unless DEBUG is
 *   also defined (the SIL build system ensures that this is the case).
 *
 * ================= Platform and architecture detection ==================
 *
 * - SIL_PLATFORM_*:  All preprocessor symbols beginning with
 *   "SIL_PLATFORM_" are reserved for platform-specific use.  Typically,
 *   the configuration header will define SIL_PLATFORM_<platform-name> to
 *   allow external code to be enabled or disabled based on the platform
 *   in use.
 *
 * - SIL_ARCH_*:  Preprocessor symbols beginning with "SIL_ARCH_" may be
 *   defined to indicate the hardware architecture for which the program
 *   is built.  These symbols are optional, and the program will work
 *   (albeit possibly at reduced efficiency) even if none are defined.
 *   The following symbols are currently recognized:
 *      - ARM CPUs:
 *           - SIL_ARCH_ARM (all architectures)
 *           - SIL_ARCH_ARM_32 (32-bit ARM architecture, assumed to
 *                 support at least the ARMv7a instruction set)
 *           - SIL_ARCH_ARM_64 (64-bit ARM architecture)
 *           - SIL_ARCH_ARM_NEON (NEON instructions supported)
 *      - MIPS CPUs:
 *           - SIL_ARCH_MIPS (all architectures)
 *           - SIL_ARCH_MIPS_32 (all 32-bit architectures)
 *           - SIL_ARCH_MIPS_64 (all 64-bit architectures)
 *           - SIL_ARCH_MIPS_MIPS32R2 (MIPS32r2 instruction set supported)
 *      - x86 CPUs:
 *           - SIL_ARCH_X86 (all architectures)
 *           - SIL_ARCH_X86_32 (32-bit x86, assumed to support MMX, SSE, SSE2)
 *           - SIL_ARCH_X86_64 (64-bit x86)
 *           - SIL_ARCH_X86_SSE3 (SSE3 instructions supported)
 *
 * ======================== External library usage ========================
 *
 * - SIL_FONT_INCLUDE_FREETYPE:  If defined, support for rendering
 *   TrueType/OpenType fonts using the FreeType library will be included.
 *   If not defined, font_parse_freetype() will always fail.
 *
 * - SIL_MOVIE_INCLUDE_WEBM:  If defined, the movie_*() functions will
 *   support software decoding of WebM audio/video streams.  (This support
 *   will be used for all WebM streams regardless of whether the runtime
 *   environment also provides WebM decoding facilities.)  The build system
 *   is responsible for ensuring that the libwebmdec, nestegg, libvpx, and
 *   libnogg libraries are linked into the executable.
 *
 * - SIL_SOUND_INCLUDE_OGG:  If defined, support for Ogg Vorbis audio
 *   decoding will be included in the sound subsystem.  The build system is
 *   responsible for ensuring that libnogg is linked into the executable.
 *
 * - SIL_UTILITY_INCLUDE_PNG:  If defined, the png_parse() and png_create()
 *   functions in utility/png.c will be compiled in.  The build system is
 *   responsible for ensuring that libpng is linked into the executable.
 *
 * - SIL_UTILITY_INCLUDE_ZLIB:  If defined, the zlib_compress() and
 *   zlib_decompress() functions in utility/zlib.c will be compiled in.
 *   The build system is responsible for ensuring that zlib is linked into
 *   the executable.
 *
 * ==================== Feature-specific configuration ====================
 *
 * - SIL_DATA_PATH_ENV_VAR:  On systems which support environment variables
 *   (Linux, Mac OS X, and Windows), this can be defined to the name of an
 *   environment variable whose value will override the system default path
 *   for resource data files.  The value must be a quoted string, e.g.:
 *       #define SIL_DATA_PATH_ENV_VAR  \"MY_VARIABLE\"
 *   or
 *       SIL_EXTRA_FLAGS = $(CFLAG_DEFINE)SIL_DATA_PATH_ENV_VAR=\"MY_VARIABLE\"
 *
 * - SIL_DEBUG_USE_VALGRIND:  If defined, SIL will detect when the program
 *   is running under the Valgrind tool and alter certain debug behaviors
 *   to make better use of Valgrind.  The Valgrind headers must be
 *   installed on the build system if this symbol is defined.  Currently,
 *   the following two behavioral changes are made:
 *      - The memory allocation functions will call out to Valgrind to
 *        provide more information about allocated memory.  This avoids
 *        hiding accesses to uninitialized memory with the uninitialized-
 *        memory fill performed by mem_alloc() and mem_realloc().
 *      - The utility_id_array test suite will not perform the "thread
 *        torture" test to verify correctness under mutex collisions, since
 *        the scheduling properties of Valgrind make such collisions much
 *        less likely if not outright impossible.
 *   If DEBUG is not defined, defining this symbol has no effect.
 *
 * - SIL_DLOG_MAX_SIZE:  Sets the maximum size, in bytes, of a single
 *   message written via the DLOG() interface (including both the
 *   file/line/function prefix and the null terminator byte).  If not
 *   defined elsewhere, defaults to 4096.  Note that when creating threads
 *   with a specific stack size, the stack size must include space for at
 *   least this many bytes when running in debug mode.
 *
 * - SIL_DLOG_STRIP_PATH:  If defined, the given prefix will be stripped
 *   from pathnames of files when logging text via DLOG(), to avoid overly
 *   long line prefixes.  Filenames not beginning with this string are not
 *   modified.
 *
 * - SIL_MEMORY_CHECK_POINTERS:  If defined, pointers passed to
 *   mem_realloc() and mem_free() will be checked for validity by
 *   traversing the global allocation list.  This will naturally slow down
 *   such calls.
 *
 * - SIL_MEMORY_DEBUG_FILL_UNUSED:  If defined and DEBUG is also defined,
 *   newly allocated memory blocks will be filled with 0xBB (when
 *   MEM_ALLOC_CLEAR is not specified), and freed blocks will be filled
 *   with 0xDD.  If DEBUG is not defined, this symbol has no effect.
 *   Note that enabling this behavior can have a significant performance
 *   impact on slow systems.
 *
 * - SIL_MEMORY_FORBID_MALLOC:  If defined, the malloc() family of memory
 *   management functions (malloc(), aligned_alloc(), calloc(), realloc(),
 *   strdup(), and free()) will be made unavailable to client code.  This
 *   can be useful with a custom memory allocator to ensure that all
 *   allocations go through the mem_alloc() family of functions.
 *
 * - SIL_MEMORY_LOG_ALLOCS:  If defined, all memory allocation/free
 *   operations (whether successful or not) that go through the mem_*()
 *   interface will be logged via DLOG().
 *
 * - SIL_OPENGL_DISABLE_GETERROR:  Disables the use of glGetError(),
 *   instead assuming that all operations succeed.  This can improve
 *   performance when glGetError() needs to wait for the hardware, but it
 *   can also mask actual error conditions such as insufficient hardware
 *   resources.  Only activate this option when the target system is known
 *   to not generate any errors.
 *
 * - SIL_OPENGL_DUMP_SHADERS:  If defined, the source code and compilation
 *   logs for each shader will be dumped using DLOG() when the shader is
 *   created.
 *
 * - SIL_OPENGL_ES:  If defined, the source code will be built for an
 *   OpenGL ES target rather than a standard OpenGL target.  (Due to header
 *   incompatibilities, standard OpenGL and OpenGL ES cannot both be
 *   supported by the same executable.)
 *
 * - SIL_OPENGL_IMMEDIATE_VERTEX_BUFFERS:  Specifies the number of vertex
 *   buffer objects to allocate for immediate-mode primitives.  If not
 *   defined, defaults to 128.
 *
 * - SIL_OPENGL_NO_SYS_FUNCS:  If defined, the system-dependent graphics
 *   functions (sys_graphics_*, sys_texture_*, and so on) supplied by the
 *   OpenGL module will instead be renamed to opengl_sys_* (for example,
 *   sys_graphics_clear() becomes opengl_sys_graphics_clear()); the
 *   SysFramebuffer, SysPrimitive, SysShader, SysShaderPipeline, and
 *   SysTexture types will likewise be renamed to OpenGLSysFramebuffer, etc.
 *   This allows the OpenGL module to coexist alongside another graphics
 *   backend.
 *
 * - SIL_OPENGL_TEXTURE_BUFFER_ALIGNMENT:  Specifies the alignment in bytes
 *   for texture pixel buffers allocated when locking a texture.  If not
 *   defined, the system default alignment is used.
 *
 * - SIL_OPENGL_VALIDATE_SHADERS:  If defined, a glValidateProgram() call
 *   will be made for each call to sys_graphics_draw_primitive() to
 *   validate the shader to be used for the primitive against the current
 *   OpenGL state.  This will naturally slow down rendering, and thus it
 *   should be used only for debugging.
 *
 * - SIL_RESOURCE_SYNC_IN_REVERSE:  If defined, resources will be synced by
 *   resource_sync() or resource_wait() in the reverse order they were
 *   loaded.  On systems without a virtual memory manager, this can help
 *   reduce memory fragmentation, since compressed data buffers for
 *   foreground decompression are allocated from the opposite end of the
 *   memory pool than the uncompressed data buffers.
 *
 * - SIL_STRFORMAT_USE_FLOATS:  If defined, the [v]strformat() functions
 *   in utility/strformat.c will use single-precision instead of double-
 *   precision arithmetic when printing floating point values.  This can
 *   reduce code size and improve execution speed in environments without
 *   native double-precision support.
 *
 * - SIL_TEST_THREAD_PERFORMANCE:  Test that priority values are properly
 *   respected, i.e. that higher-priority threads are given more run time
 *   than lower-priority threads.  On typical multitasking systems, these
 *   tests are heavily affected by system load and can easily fail even if
 *   the code itself is correct, so the tests are disabled by default.
 *
 * - SIL_TEST_VERBOSE_LOGGING:  Log debug messages for each test routine
 *   (including initialization and cleanup routines) called by the generic
 *   test runner.  Can be useful in pinpointing memory errors, but
 *   generates huge amounts of DLOG() output.
 *
 * - SIL_TEXTURE_ALIGNMENT:  If defined, indicates the preferred alignment
 *   in bytes for texture data.  The resource management code will align
 *   data buffers to a multiple of this value when creating or loading
 *   textures, and other callers of the texture management functions may
 *   also take advantage of this value to improve performance, but its use
 *   is not required for correct behavior.  (Conversely, system-dependent
 *   code is required to behave correctly even if texture data is not
 *   aligned to a multiple of this value.)  If not defined, texture data
 *   will be assumed to have no preferred alignment.
 *
 * - SIL_USE_STL_ALGORITHM:  If defined, the C++ <algorithm> header will
 *   be automatically included when compiling C++ source files.  In this
 *   case, std::min() and std::max() will be imported into the current
 *   namespace rather than defining custom versions of the functions.
 *
 * - SIL_UTILITY_INCLUDE_STRTOF:  If defined, the strtof() function in
 *   utility/strtof.c will be compiled in.  This function uses only
 *   single-precision arithmetic, and on some platforms it can run
 *   significantly faster than system implementations which simply call the
 *   double-precision strtod().  By default, the function is named strtof()
 *   so that it can override any definition in the system library; if the
 *   system does not allow library functions to be overridden,
 *   SIL_UTILITY_RENAME_STRTOF (see below) should also be defined.
 *
 * - SIL_UTILITY_MEMORY_TRANSPOSE_BLOCK_SIZE:  Specifies the blocking unit
 *   for transpose operations, i.e. the size (width and height, in data
 *   elements) of the blocks into which a memory region is subdivided for
 *   transposing; must be a positive number.  Setting this too high will
 *   result in cache thrashing, causing serious performance degradation.
 *   If not defined, a default of 16 will be used.
 *      This symbol may be defined as a function call or other non-constant
 *   expression which dynamically computes and returns the block size based
 *   on the runtime environment.  In this case, the expression will be
 *   evaluated once per transpose operation.
 *
 * - SIL_UTILITY_NOISY_ERRORS:  If defined, the display_error() function
 *   in utility/misc.c will use dialog boxes or other program-interrupting
 *   methods (if available on the system) to report error messages.  Note
 *   that the precise behavior of this option depends on the platform.
 *
 * - SIL_UTILITY_PNG_ALLOC_CHUNK:  Sets the block size for memory
 *   allocation operations performed by png_create().  Larger values
 *   reduce overhead from buffer reallocations, but png_create() may
 *   require up to this amount of extra memory over the final data size
 *   while creating the PNG data.  (The buffer returned by png_create()
 *   will be shrunk to exactly the required size, so any wasted space is
 *   freed before return.)  If not defined, defaults to 65536.
 *
 * - SIL_UTILITY_PNG_COMPRESSION_LEVEL:  If defined, sets the default
 *   compression level for compressing PNG images, such as save file
 *   screenshots.  The default value of -1 means "use the zlib default".
 *
 * - SIL_UTILITY_PNG_MAX_SIZE:  Sets the maximum pixel size (width or
 *   height) accepted by the png_parse() function.  Images with a size
 *   larger than this value in either dimension will be rejected.  If not
 *   defined, defaults to 16384.
 *
 * - SIL_UTILITY_RENAME_STRTOF:  If defined, the strtof() function in
 *   utility/strtof.c will be renamed to strtof_SIL() by means of a
 *   #define.  This can be used to avoid collision with a system-defined
 *   strtof() if the system does not allow library functions to be
 *   overridden, but this naturally means that uses of strtof() in
 *   external libraries will use the system version of strtof() rather
 *   than the SIL version.
 *
 * ======================== Internal configuration ========================
 *
 * (Library users need not read this section.)
 *
 * - SIL_MEMORY_CUSTOM:  If defined, the sys_mem_*() functions will be
 *   assumed to have system-specific definitions, and the default
 *   implementations in memory.c (which just call malloc(), realloc(), and
 *   free()) will be omitted.
 *
 * - SIL_SYSTEM_COMMON_HEADER:  If defined, base.h will include this file
 *   after all other declarations.
 *
 * - SIL_SYSTEM_MATH_HEADER:  If defined, math.h will include this file
 *   after all standard definitions, but before vector and matrix types
 *   and functions are defined.
 *
 * - SIL_SYSTEM_MATRIX_HEADER:  If defined, math.h will include this file
 *   instead of the generic "math/matrix.h" for defining matrix types and
 *   functions.
 *
 * - SIL_SYSTEM_MEMORY_HEADER:  If defined, memory.h will include this
 *   file instead of using the default implementations for the mem_*()
 *   functions.
 *
 * - SIL_SYSTEM_VECTOR_HEADER:  If defined, math.h will include this file
 *   instead of the generic "math/vector.h" for defining vector types and
 *   functions.
 */

/*-----------------------------------------------------------------------*/

/*
 * Sanity-check configuration options and set defaults where needed.
 */

#if defined(SIL_INCLUDE_TESTS) && !defined(DEBUG)
# error SIL_INCLUDE_TESTS requires DEBUG to be defined.
#endif

#ifndef SIL_DLOG_MAX_SIZE
# define SIL_DLOG_MAX_SIZE  4096
#endif

#ifndef SIL_OPENGL_IMMEDIATE_VERTEX_BUFFERS
# define SIL_OPENGL_IMMEDIATE_VERTEX_BUFFERS  128
#endif

#ifndef SIL_OPENGL_TEXTURE_BUFFER_ALIGNMENT
# define SIL_OPENGL_TEXTURE_BUFFER_ALIGNMENT  0
#endif

#ifndef SIL_TEXTURE_ALIGNMENT
# define SIL_TEXTURE_ALIGNMENT  0
#endif

#ifndef SIL_UTILITY_MEMORY_TRANSPOSE_BLOCK_SIZE
# define SIL_UTILITY_MEMORY_TRANSPOSE_BLOCK_SIZE  16
#endif

#ifdef SIL_UTILITY_PNG_ALLOC_CHUNK
# if SIL_UTILITY_PNG_ALLOC_CHUNK < 1
#  error Invalid value for SIL_UTILITY_PNG_ALLOC_CHUNK (must be positive).
# endif
#else
# define SIL_UTILITY_PNG_ALLOC_CHUNK  65536
#endif

#ifndef SIL_UTILITY_PNG_COMPRESSION_LEVEL
# define SIL_UTILITY_PNG_COMPRESSION_LEVEL  (-1)
#endif

#ifdef SIL_UTILITY_PNG_MAX_SIZE
# if SIL_UTILITY_PNG_MAX_SIZE < 1
#  error Invalid value for SIL_UTILITY_PNG_MAX_SIZE (must be positive).
# endif
#else
# define SIL_UTILITY_PNG_MAX_SIZE  16384
#endif

/*************************************************************************/
/********************** Compilation control macros ***********************/
/*************************************************************************/

/**
 * EXTERN_C:  Mark a single declaration as "extern" with C linkage.  This
 * expands to 'extern "C"' for C++ and 'extern' by itself for C.
 */
#ifdef __cplusplus
# define EXTERN_C  extern "C"
#else
# define EXTERN_C  extern
#endif

/**
 * EXTERN_C_BEGIN, EXTERN_C_END:  Mark the beginning or end of a C-linkage
 * interface section for C++ code.  These macros expand to 'extern "C" {'
 * and '};' respectively for C++, and to nothing for plain C.  These are
 * primarily for internal use.
 */
#ifdef __cplusplus
# define EXTERN_C_BEGIN  extern "C" {
# define EXTERN_C_END    }
#else
# define EXTERN_C_BEGIN  /*nothing*/
# define EXTERN_C_END    /*nothing*/
#endif

/*-----------------------------------------------------------------------*/

/**
 * IS_GCC, IS_CLANG:  Check whether the compiler is GCC (resp. Clang) and
 * is at least as new as the specified version.  These macros can only be
 * used as conditions in #if and #elif directives.
 */
#ifdef __GNUC__
# define IS_GCC(major,minor)    \
    (__GNUC__ > major           \
     || (__GNUC__ == major && __GNUC_MINOR__ >= minor))
#else
# define IS_GCC(major,minor)  0
#endif

#ifdef __clang__
# define IS_CLANG(major,minor)  \
    (__clang_major__ > major    \
     || (__clang_major__ == major && __clang_minor__ >= minor))
#else
# define IS_CLANG(major,minor)  0
#endif

/*-----------------------------------------------------------------------*/

/**
 * ALIGNED:  Force the alignment of a variable or structure member to the
 * given number of bytes.  For example:
 *     ALIGNED(64) int32_t buf[1000];
 * would align the starting address of "buf" to a multiple of 64 bytes.
 * The maximum possible alignment depends on the target architecture.
 */
#if !defined(__cplusplus) && defined(__STDC__) && __STDC_VERSION__ >= 201112L
# define ALIGNED(n)  _Alignas((n))
#elif IS_GCC(2,95) || IS_CLANG(1,0)
# define ALIGNED(n)  __attribute__((aligned((n))))
#elif defined(_MSC_VER)
# define ALIGNED(n)  __declspec(align(n))
#else
# error ALIGNED() is not defined for this compiler.
#endif

/*-----------------------------------------------------------------------*/

/**
 * CONST_FUNCTION:  Declare a function as being constant.  A constant
 * function's return value must depend only on the values of the parameters
 * passed to it, and in particular must not depend on the contents of
 * writable memory (even memory pointed to by parameters); compare
 * PURE_FUNCTION below.  For example:
 *     CONST_FUNCTION float square(float x);
 * indicates that for any given value of the parameter "x" the function
 * square() will always return the same result, regardless of any other
 * system state.
 *
 * Note that a constant function _may_ read from read-only memory, such as
 * lookup tables which have been declared constant.
 */
#if IS_GCC(2,95) || IS_CLANG(1,0)
# define CONST_FUNCTION  __attribute__((const))
#else
# define CONST_FUNCTION  /*nothing*/
#endif

/*-----------------------------------------------------------------------*/

/**
 * PURE_FUNCTION:  Declare a function as being pure.  A pure function's
 * return value must depend only on the values of the parameters and the
 * current program state; thus a pure function, unlike a constant function,
 * is permitted to read through pointer parameters.  Functions which rely
 * on volatile data (data which may change for reasons other than being
 * written by the current thread of the program) should not be declared pure.
 */
#if IS_GCC(3,0) || IS_CLANG(1,0)
# define PURE_FUNCTION  __attribute__((pure))
#else
# define PURE_FUNCTION  /*nothing*/
#endif

/* GCC 4.6 for armeabi-v7a has an optimizer bug that can completely drop
 * calls to pure functions (thus not calling them at all), so disable the
 * optimization in that case.  GCC 4.8 does not have the bug; the state of
 * GCC 4.7 is unknown.  See: https://issuetracker.google.com/issues/36980664
 * (formerly: https://code.google.com/p/android/issues/detail?id=60924) */
#if IS_GCC(2,95) && !IS_GCC(4,8) && !IS_CLANG(1,0) && defined(SIL_ARCH_ARM)
# undef PURE_FUNCTION
# define PURE_FUNCTION  /*nothing*/
#endif

/*-----------------------------------------------------------------------*/

/**
 * FORMAT:  Declare a function as taking a printf()-type format string.
 * The "fmt" argument specifies the index of the function parameter
 * containing the format string, while the "firstarg" argument specifies
 * the index of the first format argument in the function parameter list
 * (in other words, the index of the "...").  See the declaration of
 * do_DLOG() for an example.
 */
#if defined(__MINGW32__)
/* The MinGW build of GCC spits out tons of bogus warnings, so disable. */
# define FORMAT(fmt,firstarg)  /*nothing*/
#elif IS_GCC(2,95) || IS_CLANG(1,0)
# define FORMAT(fmt,firstarg)  __attribute__((format(printf,fmt,firstarg)))
#else
# define FORMAT(fmt,firstarg)  /*nothing*/
#endif

/*-----------------------------------------------------------------------*/

/**
 * NORETURN:  Declare a function to be non-returning (for example, because
 * it calls exit()).  For example:
 *     NORETURN void exit(int code);
 */
#if !defined(__cplusplus) && defined(__STDC__) && __STDC_VERSION__ >= 201112L
# define NORETURN  _Noreturn
#elif IS_GCC(2,95) || IS_CLANG(1,0)
# define NORETURN  __attribute__((noreturn))
#elif defined(_MSC_VER)
# define NORETURN  __declspec(noreturn)
#else
# define NORETURN  /*nothing*/
#endif

/*-----------------------------------------------------------------------*/

/**
 * NOINLINE:  Prevent the compiler from inlining a function.  For example:
 *     NOINLINE void function(void);
 */
#if IS_GCC(3,1) || IS_CLANG(1,0)
# define NOINLINE  __attribute__((noinline))
#elif defined(_MSC_VER)
# define NOINLINE  __declspec(noinline)
#else
# define NOINLINE  /*nothing*/
#endif

/*-----------------------------------------------------------------------*/

/**
 * UNUSED:  Mark a variable or function parameter as being unused.  This
 * has no effect on code generation, but it can be used to suppress
 * compiler warnings about unused variables or parameters.
 */
#if IS_GCC(2,95) || IS_CLANG(1,0)
# define UNUSED  __attribute__((unused))
#elif defined(_MSC_VER)
# define UNUSED  __pragma(warning(suppress:4100 4189))
#else
# define UNUSED  /*nothing*/
#endif

/*-----------------------------------------------------------------------*/

/**
 * DEBUG_USED:  Mark a variable or function parameter as being unused when
 * not building in debug mode.
 */
#ifdef DEBUG
# define DEBUG_USED  /*nothing*/
#else
# define DEBUG_USED  UNUSED
#endif

/*-----------------------------------------------------------------------*/

/**
 * LIKELY, UNLIKELY:  Indicate the expected truth value of a condition in
 * an if() test.  LIKELY(x) means "x is expected to be true (nonzero)",
 * while UNLIKELY(x) means "x is expected to be false (zero)".  The macros
 * themselves evaluate the parameter "x" and return a boolean value
 * suitable for use in an if() statement.
 */
#if IS_GCC(3,0) || IS_CLANG(1,0)
# define LIKELY(x)    __builtin_expect(!!(x), 1)
# define UNLIKELY(x)  __builtin_expect(!!(x), 0)
#else
# define LIKELY(x)    (x)
# define UNLIKELY(x)  (x)
#endif

/*-----------------------------------------------------------------------*/

/**
 * BARRIER:  Set a memory barrier at the current location, preventing the
 * compiler and CPU from moving memory loads or stores across the barrier.
 * Typically used when accessing shared variables from multiple threads
 * using a lock-free algorithm.
 *
 * Note that cache coherency is assumed throughout the code; this library
 * will not work on a noncoherent multiprocessor system.
 */

#if IS_GCC(2,95) || IS_CLANG(1,0)

# if defined(SIL_ARCH_X86_32)
/* This instruction sequence accomplishes the same thing as SSE2's MFENCE
 * instruction, but runs more quickly on currently common architectures
 * (as of 2016). */
#  define BARRIER()  __asm__ volatile("lock addl $0,(%%esp)" : : : "memory")
# elif defined(SIL_ARCH_X86_64)
/* On x86-64, we take advantage of the "red zone" to try and avoid
 * introducing false data dependencies on data in the current function's
 * stack frame.  The -64 offset ensures that the word we touch is in a
 * separate cache line from anything in the stack frame (64 is the cache
 * line size used by all current x86 chips; note that a change in cache
 * line size in future processors will at worst nullify this performance
 * boost and will not result in incorrect behavior).  This may not apply
 * to leaf functions, which are allowed to put their stack frames in the
 * red zone, but we assume most memory barriers will not be in leaf
 * functions. */
#  ifdef SIL_PLATFORM_WINDOWS
/* Windows, of course, has to do things its own way and doesn't have a red
 * zone... */
#   define BARRIER()  __asm__ volatile("lock addl $0,(%%rsp)" : : : "memory")
#  else
#   define BARRIER()  __asm__ volatile("lock addl $0,-64(%%rsp)" : : : "memory")
#  endif
# elif defined(SIL_ARCH_MIPS)
#  define BARRIER()  __asm__ volatile("sync" : : : "memory")
# elif defined(SIL_ARCH_ARM_32)
#  define BARRIER()  __asm__ volatile("dmb" : : : "memory")
# elif defined(SIL_ARCH_ARM_64)
#  define BARRIER()  __asm__ volatile("dmb sy" : : : "memory")
# else
#  error Memory barrier CPU instruction unknown!
# endif

#elif defined(_MSC_VER)

# include <intrin.h>
# if defined(_M_X64)
#  define BARRIER()  __faststorefence()
# else
/* This is an SSE2-specific instruction, but we assume the presence of SSE2. */
#  define BARRIER()  _mm_mfence()
# endif

#else  // !IS_GCC && !IS_CLANG && !_MSC_VER

# error Please define BARRIER() for this compiler.

#endif

/*-----------------------------------------------------------------------*/

/**
 * DEBUG_MATH_BARRIER:  Sets an optimization barrier on the given variable
 * at the given location.  This is useful in certain environments if
 * floating-point exceptions are enabled when debugging, to prevent
 * compiler optimization from triggering an exception when there is in
 * fact no error.  For example:
 *
 *     dist = sqrtf(x*x + y*y);
 *     if (dist > 0) {
 *         // Insert an optimization barrier to prevent optimization that
 *         // would result in division by zero.
 *         DEBUG_MATH_BARRIER(dist);
 *         x /= dist;
 *         y /= dist;
 *     }
 *
 * This macro does nothing when not in debugging mode.
 */

#ifdef DEBUG

# if IS_GCC(2,95) || IS_CLANG(1,0)
#  if defined(SIL_ARCH_ARM_64)
#   define DEBUG_MATH_BARRIER(var) \
        __asm__ volatile("" : "=w" (var) : "0" (var))
#  elif defined(SIL_ARCH_MIPS)
#   define DEBUG_MATH_BARRIER(var) \
        __asm__ volatile("" : "=f" (var) : "0" (var))
#  elif defined(SIL_ARCH_X86)
#   define DEBUG_MATH_BARRIER(var) \
        __asm__ volatile("" : "=x" (var) : "0" (var))
#  else
#   define DEBUG_MATH_BARRIER(var) \
        do {volatile __typeof__(var) _temp = var; var = _temp;} while (0)
#  endif
# else
#  define DEBUG_MATH_BARRIER(var)
# endif

#else  // !DEBUG

# define DEBUG_MATH_BARRIER(var)

#endif

/*-----------------------------------------------------------------------*/

/**
 * FALLTHROUGH:  Indicate to the compiler that the end of a switch case
 * deliberately falls through to the nexet case.
 */
#if IS_GCC(7,1)
# define FALLTHROUGH  __attribute__((fallthrough))
#else
# define FALLTHROUGH  /*nothing*/
#endif

/*-----------------------------------------------------------------------*/

/**
 * UNREACHABLE:  Indicate to the compiler that the current code location
 * can never be reached.  If the code location is in fact reached, program
 * behavior becomes undefined.
 */
#if IS_GCC(4,5) || IS_CLANG(2,7)
# define UNREACHABLE  __builtin_unreachable()
#elif defined(_MSC_VER)
# define UNREACHABLE  __assume(0)
#else
# define UNREACHABLE  abort()
#endif

/*-----------------------------------------------------------------------*/

/**
 * STATIC_ASSERT:  Check whether the specified condition is true.  If
 * false, abort compilation of the program with the specified error
 * message.  The condition must be a compile-time constant.
 *
 * Unlike ASSERT and PRECOND (defined below), the behavior of this macro
 * does not change whether DEBUG is or is not defined.
 */
#if defined(__cplusplus) || defined(_MSC_VER)
# define STATIC_ASSERT(condition,message)  static_assert(condition, message)
#elif (defined(__STDC__) && __STDC__ >= 201112) || IS_GCC(4,6) || IS_CLANG(6,0)
/* GCC 4.6+ and (at least) Clang 6.0+ support _Static_assert even in
 * vanilla C99 mode. */
# define STATIC_ASSERT(condition,message)  _Static_assert(condition, message)
#else
# define STATIC_ASSERT__(line)  _static_assert_##line
# define STATIC_ASSERT_(line,expr,message) \
    struct STATIC_ASSERT__(line) { \
        int _error_if_negative : 1 - 2 * ((expr) == 0); \
    }
# define STATIC_ASSERT(condition,message) \
    STATIC_ASSERT_(__LINE__, condition, message)
#endif

/*************************************************************************/
/************* Common system headers and related definitions *************/
/*************************************************************************/

/* Avoid environmental pollution and hide some non-portable functions. */
#define initstate _sil__HIDDEN_initstate
#define index     _sil__HIDDEN_index
#define random    _sil__HIDDEN_random
#define random_r  _sil__HIDDEN_random_r
#define setstate  _sil__HIDDEN_setstate
#define srandom   _sil__HIDDEN_srandom
/* Can't hide these in C++, sadly, because some compilers' headers want to
 * "use" them into the std namespace. */
#ifndef __cplusplus
# define rand      _sil__HIDDEN_rand
# define srand     _sil__HIDDEN_srand
# define system    _sil__HIDDEN_system
#endif

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(__cplusplus) && defined(SIL_USE_STL_ALGORITHM)
/* <algorithm> includes <math.h> via <cmath>, but we need to deal with
 * some platform-specific issues around that, so include <math.h> on our
 * own terms. */
# include <SIL/math.h>
# include <algorithm>
#endif

#undef initstate
#undef index
#undef random
#undef random_r
#undef setstate
#undef srandom
#if defined(__cplusplus) && defined(SIL_USE_STL_ALGORITHM)
# include <cstdlib>
# define rand _sil__DO_NOT_USE_rand
# define srand _sil__DO_NOT_USE_srand
# define system _sil__DO_NOT_USE_system
#else
# undef rand
# undef srand
# undef system
#endif

/*-----------------------------------------------------------------------*/

/* Bracket the remainder of the file with extern "C". */
EXTERN_C_BEGIN

/*-----------------------------------------------------------------------*/

/* Disable the malloc() family of functions if requested, by defining them
 * to names which do not exist. */

#ifdef SIL_MEMORY_FORBID_MALLOC

# undef aligned_alloc
# undef calloc
# undef free
# undef malloc
# undef realloc
# undef strdup

# define aligned_alloc  _sil__DO_NOT_USE_aligned_alloc
# define calloc         _sil__DO_NOT_USE_calloc
# define free           _sil__DO_NOT_USE_free
# define malloc         _sil__DO_NOT_USE_malloc
# define realloc        _sil__DO_NOT_USE_realloc
# define strdup         _sil__DO_NOT_USE_strdup

#endif

/*-----------------------------------------------------------------------*/

/* This is checked by utility/strdup.c to see whether to define strdup(). */
#undef NEED_STRDUP

/* These are used below and by test/utility/strdup.c to enable testing of
 * the local versions even if the system uses a built-in function. */
#undef STRDUP_IS_RENAMED
#undef RENAME_STRDUP

#if !defined(HAVE_STRDUP) && !defined(SIL_MEMORY_FORBID_MALLOC)
# define NEED_STRDUP
# define RENAME_STRDUP(name)  name
#endif

/* If tests are being compiled in and we don't otherwise need strdup(),
 * build it with a different name so we can test it. */
#if defined(SIL_INCLUDE_TESTS) && !defined(NEED_STRDUP) && !defined(SIL_MEMORY_FORBID_MALLOC)
# define NEED_STRDUP
# define STRDUP_IS_RENAMED
# define RENAME_STRDUP(name)  name##_SIL
# ifndef STRNICMP_SIZE_T
#  define STRNICMP_SIZE_T  unsigned int
# endif
#endif

#ifdef NEED_STRDUP

/**
 * strdup:  Duplicate the given string.  The returned string should be
 * freed with free() when it is no longer needed.
 *
 * [Parameters]
 *     s: String to duplicate.
 * [Return value]
 *     Copy of string, or NULL if out of memory.
 */
extern char *RENAME_STRDUP(strdup)(const char *s);

#endif

/*-----------------------------------------------------------------------*/

#undef NEED_STRICMP
#undef STRICMP_IS_RENAMED
#undef RENAME_STRICMP

#ifndef HAVE_STRICMP

# if defined(HAVE__STRICMP)
#  define stricmp  _stricmp
#  define strnicmp  _strnicmp
# elif defined(HAVE_STRCASECMP)
#  include <strings.h>
#  define stricmp  strcasecmp
#  define strnicmp  strncasecmp
# else
#  define NEED_STRICMP
#  define RENAME_STRICMP(name)  name
# endif
#endif  // !HAVE_STRICMP

#if defined(SIL_INCLUDE_TESTS) && !defined(NEED_STRICMP)
# define NEED_STRICMP
# define STRICMP_IS_RENAMED
# define RENAME_STRICMP(name)  name##_SIL
# ifndef STRNICMP_SIZE_T
#  define STRNICMP_SIZE_T  unsigned int
# endif
#endif

#ifdef NEED_STRICMP

/**
 * stricmp, strnicmp:  Compare two strings case-insensitively.  Equivalent
 * to strcmp() and strncmp() performed on lower-case versions of the
 * string parameters.  Non-ASCII bytes are all treated as distinct.
 *
 * Characters are treated as unsigned values (0-255) for ordering purposes.
 *
 * [Parameters]
 *     s1, s2: Strings to compare.
 *     n: Maximum number of bytes to compare.
 * [Return value]
 *     <0 if lowercase(s1) <  lowercase(s2)
 *      0 if lowercase(s1) == lowercase(s2)
 *     >0 if lowercase(s1) >  lowercase(s2)
 */
extern PURE_FUNCTION int RENAME_STRICMP(stricmp)(
    const char *s1, const char *s2);
extern PURE_FUNCTION int RENAME_STRICMP(strnicmp)(
    const char *s1, const char *s2, STRNICMP_SIZE_T n);

#endif

/*-----------------------------------------------------------------------*/

/* Rename our custom strtof() if requested.  This must be done _after_
 * including <stdlib.h> to avoid potential declaration conflicts.  If
 * tests are being compiled in, we also declare the function so we can
 * test it; in that case, renaming occurs in the strtof.c source file
 * (and its test source) so that the rest of the program continues to
 * use the system strtof() function. */

#if defined(SIL_UTILITY_INCLUDE_STRTOF) && defined(SIL_UTILITY_RENAME_STRTOF)
# undef strtof
# define strtof strtof_SIL
extern float strtof_SIL(const char *s, char **endptr);
#elif !defined(SIL_UTILITY_INCLUDE_STRTOF) && defined(SIL_INCLUDE_TESTS)
extern float strtof_SIL(const char *s, char **endptr);
#endif

/*************************************************************************/
/******************* Convenience functions and macros ********************/
/*************************************************************************/

/**
 * min, max:  Return the minimum or maximum value of the two arguments.
 *
 * When used in plain C (not C++) code, these are defined as macros which
 * may evaluate their arguments multiple times, so do not use expressions
 * with side effects as arguments.
 */
#ifdef __cplusplus
EXTERN_C_END
# ifdef SIL_USE_STL_ALGORITHM
using std::min;
using std::max;
# else
template <typename T> inline const T &min(const T &a, const T &b)
    {return a < b ? a : b;}
template <typename T> inline const T &max(const T &a, const T &b)
    {return a > b ? a : b;}
# endif
EXTERN_C_BEGIN
#else
# undef min
# undef max
# define min(a,b)  ((a) < (b) ? (a) : (b))
# define max(a,b)  ((a) > (b) ? (a) : (b))
#endif

/*-----------------------------------------------------------------------*/

/**
 * lbound, ubound, bound:  Bound the value "x" on the lower side, upper
 * side, or both sides, respectively.  These are functionally equivalent
 * to appropriate uses of min() and max(), but are provided to help
 * improve code readability when the conceptually desired result is the
 * bounding of a value within given limits.
 *
 * As with min() and max(), these may evaluate their arguments multiple
 * times when called from plain C code, so do not use expressions with
 * side effects as arguments.
 */
#define lbound(x,lower)       max((x), (lower))
#define ubound(x,upper)       min((x), (upper))
#define bound(x,lower,upper)  ubound(lbound((x), (lower)), (upper))

/*-----------------------------------------------------------------------*/

/**
 * align_up, align_down:  Round the unsigned value "x" up or down to a
 * multiple of "align".  (Note that GCC 4.2 and later are capable of
 * optimizing "a/b*b" to "a&-b" when b is a constant power of 2, so there
 * is no need to obfuscate these functions with special cases.)
 */
static CONST_FUNCTION inline uintptr_t align_up(uintptr_t x, unsigned int align)
    {return (x + (align-1)) / align * align;}
static CONST_FUNCTION inline uintptr_t align_down(uintptr_t x, unsigned int align)
    {return x / align * align;}

/*-----------------------------------------------------------------------*/

/**
 * lenof:  Return the length of the given array in elements.  The argument
 * must be an actual array, not a pointer.  Note that the return value is
 * cast to (signed) int, as opposed to the unsigned value returned by sizeof.
 */
#define lenof(array)  ((int)(sizeof(array) / sizeof(*(array))))

/*-----------------------------------------------------------------------*/

/**
 * mem_clear:  Clear a region of memory to zero.  Exactly equivalent to
 * memset(ptr,0,size), but is easier to read and avoids the risk of
 * accidentally reversing the last two parameters.
 */
#define mem_clear(ptr,size)  memset((ptr), 0, (size))

/**
 * mem_fill8:  Fill a region of memory with an 8-bit value.  Exactly
 * equivalent to memset(ptr,val,size).
 */
#define mem_fill8(ptr,val,size)  memset((ptr), (val), (size))

/**
 * mem_fill32:  Fill a region of memory with a 32-bit value.  The region is
 * assumed to be 32-bit aligned, and any fractional part of a 32-bit unit
 * specified by "size" is ignored (the function fills size/4 32-bit words).
 *
 * [Parameters]
 *     ptr: Pointer to memory region to fill.
 *     val: 32-bit value to fill with.
 *     size: Number of bytes to fill.
 */
extern void mem_fill32(void *ptr, uint32_t val, size_t size);

/*-----------------------------------------------------------------------*/

/**
 * strformat, vstrformat:  Format a string according to the embedded format
 * tokens and associated arguments, and store the formatted string into the
 * given buffer.  Format tokens are specified as for the printf() family of
 * functions, using a "%" followed by optional modifiers and a format
 * specifier.
 *
 * Note that both the size parameter and the return value are given in
 * bytes, not characters.  All strings are interpreted as UTF-8, so a
 * single character may occupy two or more bytes.
 *
 * Most C99 format tokens are supported; see utility/strformat.c for details.
 *
 * [Parameters]
 *     buf: Output buffer.
 *     size: Output buffer size, in bytes.
 *     format: Format string.
 *     ... / args: Format arguments.
 * [Return value]
 *     Length of the output string in bytes, not including the null
 *     terminator.  If this value is greater than size-1, then part of the
 *     output string was truncated.  Regardless of any truncation, the
 *     string stored in the output buffer will always be null-terminated
 *     and will always be a well-formed UTF-8 string.
 */
extern int strformat(char *buf, int size, const char *format, ...)
    FORMAT(3,4);
extern int vstrformat(char *buf, int size, const char *format, va_list args);

/**
 * strformat_check, vstrformat_check:  Format a string and return whether
 * the formatted result fits within the output buffer.  Equivalent to
 * ([v]strformat(buf, size, format, ...) < size).
 *
 * [Parameters]
 *     buf: Output buffer.
 *     size: Output buffer size, in bytes.
 *     format: Format string.
 *     ... / args: Format arguments.
 * [Return value]
 *     True if the formatted string fits within the output buffer;
 *     false if the full formatted string would overflow the buffer.
 */
extern int strformat_check(char *buf, int size, const char *format, ...)
    FORMAT(3,4);
extern int vstrformat_check(char *buf, int size, const char *format,
                            va_list args);

/**
 * strformat_append, vstrformat_append:  Format a string and append it to
 * a dynamically-allocated buffer.  The buffer will be (re)allocated with
 * mem_realloc(), and can be freed with mem_free() when no longer needed.
 *
 * To allocate a new buffer for the formatted string, set the variables
 * pointed to by bufptr and lenptr to NULL and 0, respectively.
 *
 * [Parameters]
 *     bufptr: Pointer to the output buffer pointer.  Updated on return
 *         to point to the reallocated buffer.
 *     lenptr: Pointer to the current length of the output string (not
 *         including the trailing null byte).  Updated on return to the
 *         length of the resulting string.
 *     mem_flags: Allocation flags for mem_realloc().
 *     format: Format string.
 *     ... / args: Format arguments.
 * [Return value]
 *     True on success; false on out-of-memory or invalid argument.
 */
extern int strformat_append(char **bufptr, int *lenptr, int mem_flags,
                            const char *format, ...);
extern int vstrformat_append(char **bufptr, int *lenptr, int mem_flags,
                             const char *format, va_list args);

/**
 * strformat_alloc, vstrformat_alloc:  Format a string and return the
 * result in a newly-allocated buffer.  Equivalent to calling
 * [v]strformat_append() with a NULL buffer and zero for mem_flags.
 *
 * [Parameters]
 *     format: Format string.
 *     ... / args: Format arguments.
 * [Return value]
 *     Newly allocated buffer containing the formatted string, or NULL on
 *     error.
 */
extern char *strformat_alloc(const char *format, ...);
extern char *vstrformat_alloc(const char *format, va_list args);

/*************************************************************************/
/*************************** Debugging macros ****************************/
/*************************************************************************/

/**
 * DLOG:  Output a debugging message.  The message is prefixed with the
 * source file, line number, and function from which the macro was called,
 * and a newline is automatically appended if the formatted result does not
 * end with a newline.
 *
 * This macro does nothing if not in debugging mode.
 *
 * [Parameters]
 *     message: Message to output (must be a string literal).  May include
 *         printf()-style format tokens.
 *     ...: Format arguments for message.
 */

#ifdef DEBUG
# define DLOG(...)  do_DLOG(__FILE__, __LINE__, __FUNCTION__, "" __VA_ARGS__)
#else
# define DLOG(...)  ((void)0)  // Avoid "empty if body" warnings.
#endif

/**
 * do_DLOG, vdo_DLOG:  Helper function for the DLOG() macro, used to output
 * debug messages, and a va_list equivalent for generating custom debug
 * output.  If file == NULL, the file, line, and function arguments are
 * ignored, and the message is printed without any line header.
 *
 * These functions are only defined in debugging mode.
 *
 * [Parameters]
 *     file: Name of source file from which the message originated.
 *     line: Source line number from which the message originated.
 *     function: Name of function from which the message originated.
 *     format: Message text to output.  May include printf()-style format
 *         tokens).
 *     ... / args: Format arguments for message text.
 */
#ifdef DEBUG
extern void do_DLOG(const char *file, unsigned int line, const char *function,
                    const char *format, ...) FORMAT(4,5);
extern void vdo_DLOG(const char *file, unsigned int line, const char *function,
                     const char *format, va_list args);
#endif

/*-----------------------------------------------------------------------*/

/**
 * ASSERT, PRECOND:  Check whether the specified condition is true.  If
 * the condition is false:
 *    - In debugging mode, the program is aborted with an appropriate
 *      error message.
 *    - Otherwise, the fallback action (optional macro argument) is
 *      executed.  If no fallback action is present, the failure is
 *      ignored; if additionally the condition expression has no side
 *      effects, the entire check will normally be optimized out by the
 *      compiler.
 *
 * Aside from the text of the error message ("ASSERTION" vs.
 * "PRECONDITION"), these two macros are identical in function.  The
 * distinct names are intended to help code readability, by explicitly
 * marking function precondition checks as such.
 *
 * [Parameters]
 *     condition: Condition to check.
 *     ...: Optional statement(s) to execute if the condition is false.
 */

/* Note that these macros do _not_ use the traditional do {...} while (0)
 * construct, since doing so would change the meaning of a "break" or
 * "continue" statement in the fallback action.  Instead, we use an
 * if-else statement with a no-op but non-empty statement in the else
 * branch. */

#ifdef DEBUG

# define ASSERT(condition,...)                                          \
    if (UNLIKELY(!(condition))) {                                       \
        do_DLOG(__FILE__, __LINE__, __FUNCTION__,                       \
                "\n\n*** ALERT *** ASSERTION FAILED:\n%s\n\n",          \
                #condition);                                            \
        /* The fallback action is not actually executed, but we include \
         * it inside an if(0) so we can check for compilation errors in \
         * debug mode. */                                               \
        if (0) {__VA_ARGS__;}                                           \
        abort();                                                        \
    } else (void)0
# define PRECOND(condition,...)                                         \
    if (UNLIKELY(!(condition))) {                                       \
        do_DLOG(__FILE__, __LINE__, __FUNCTION__,                       \
                "\n\n*** ALERT *** PRECONDITION FAILED:\n%s\n\n",       \
                #condition);                                            \
        if (0) {__VA_ARGS__;}                                           \
        abort();                                                        \
    } else (void)0

#else  // !DEBUG

# define ASSERT(condition,...)          \
    if (UNLIKELY(!(condition))) {       \
        __VA_ARGS__;                    \
    } else (void)0
# define PRECOND ASSERT

#endif

/*************************************************************************/
/************** Program entry point for the library client ***************/
/*************************************************************************/

/**
 * sil_main:  Entry point for the client of this library.  This function
 * is _not_ defined by the library itself, but is called by the system-
 * specific program entry point (e.g., main() on Unix-like systems) after
 * any system-specific initialization has been performed.
 *
 * If the C stdio library (or the C++ stream interface) is used to access
 * data files located in the same directory as the program's executable
 * file, the program can assume that such files are accessible from the
 * current directory when this function is called.
 *
 * This function can be considered as the equivalent of main() for the
 * platform-independent interface provided by this library.  As with main(),
 * argv[0] will contain the program name as provided by the system; if the
 * system does not provide such a name, argv[0] will be the empty string,
 * so that any program parameters start at argv[1] regardless of the system
 * on which the program is running.  In particular, SIL guarantees that
 * argc > 0 and argv[0] != NULL when this function is called.
 *
 * Note that unlike a standard C main() function, the strings contained in
 * argv[] may not be modified (note the "const char *" declaration).
 *
 * [Parameters]
 *     argc: Number of program parameters (including program name), always > 0.
 *     argv: Program parameter array; argv[0] is always non-NULL.
 * [Return value]
 *     Program exit code (either EXIT_SUCCESS or EXIT_FAILURE).
 */
extern int sil_main(int argc, const char *argv[]);

/*************************************************************************/

EXTERN_C_END

/*************************************************************************/
/***************** System-specific declarations (if any) *****************/
/*************************************************************************/

#ifdef SIL_SYSTEM_COMMON_HEADER
# include SIL_SYSTEM_COMMON_HEADER
#endif

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_BASE_H
