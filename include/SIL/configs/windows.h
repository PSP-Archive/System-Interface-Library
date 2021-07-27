/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/configs/windows.h: Configuration header for Windows.
 */

#ifndef SIL_CONFIGS_WINDOWS_H
#define SIL_CONFIGS_WINDOWS_H

/*************************************************************************/
/*************************************************************************/

#define SIL_PLATFORM_WINDOWS
#define SIL_SYSTEM_COMMON_HEADER  "sysdep/windows/common.h"

/* Windows only runs on little-endian platforms. */
#define IS_LITTLE_ENDIAN

#ifdef _MSC_VER

# if defined(_M_IX86)
#  define SIL_ARCH_X86
#  define SIL_ARCH_X86_32
# elif defined(_M_X64)
#  define SIL_ARCH_X86
#  define SIL_ARCH_X86_64
# elif defined(_M_ARM)
#  define SIL_ARCH_ARM
#  define SIL_ARCH_ARM_32
# elif defined(_M_ARM64)
#  define SIL_ARCH_ARM
#  define SIL_ARCH_ARM_64
#  define SIL_ARCH_ARM_NEON
# else
#  error Unable to determine target architecture!
# endif

# define HAVE__STRICMP
# define STRNICMP_SIZE_T  size_t

# undef _DEBUG  // Use in MS headers conflicts with some of our code.

#else  // !_MSC_VER

# if defined(__i386__)
#  define SIL_ARCH_X86
#  define SIL_ARCH_X86_32
# elif defined(__amd64__)
#  define SIL_ARCH_X86
#  define SIL_ARCH_X86_64
# elif defined(__arm__)
#  define SIL_ARCH_ARM
#  define SIL_ARCH_ARM_32
# else
#  error Unsupported CPU architecture!
# endif

/* MinGW's libc has strcasecmp() but not in <strings.h>, so we use our own
 * instead.  At least some versions of <string.h> declare str[n]icmp(),
 * though, so we need to use the correct type. */
//# define HAVE_STRCASECMP
# define STRNICMP_SIZE_T  size_t

#endif  // _MSC_VER

/* Don't let OpenGL define the sys_* graphics functions, so we can switch
 * between OpenGL and Direct3D. */
#define SIL_OPENGL_NO_SYS_FUNCS

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_CONFIGS_WINDOWS_H
