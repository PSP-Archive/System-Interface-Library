/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/configs/linux.h: Configuration header for Linux.
 */

#ifndef SIL_CONFIGS_LINUX_H
#define SIL_CONFIGS_LINUX_H

/*************************************************************************/
/*************************************************************************/

#define SIL_PLATFORM_LINUX

#if defined(__i386__)
# define SIL_ARCH_X86
# define SIL_ARCH_X86_32
#elif defined(__x86_64__)
# define SIL_ARCH_X86
# define SIL_ARCH_X86_64
#elif defined(__arm__)
# define SIL_ARCH_ARM
# if defined(__aarch64__)
#  define SIL_ARCH_ARM_64
#  define SIL_ARCH_ARM_NEON
# else
#  define SIL_ARCH_ARM_32
# endif
#else
# error Unsupported CPU architecture!
#endif

/* Pick up endianness from <endian.h>.  This is (at least with glibc 2.x)
 * safe in that it does not include any other standard headers. */
#include <endian.h>
#if __BYTE_ORDER == __LITTLE_ENDIAN
# define IS_LITTLE_ENDIAN
#elif __BYTE_ORDER == __BIG_ENDIAN
# define IS_BIG_ENDIAN
#else
# error Unsupported endianness!
#endif

#define HAVE_STRDUP
#define HAVE_STRCASECMP
#define STRNICMP_SIZE_T  size_t

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_CONFIGS_LINUX_H
