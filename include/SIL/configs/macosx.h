/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/configs/macosx.h: Configuration header for Mac OS X.
 */

#ifndef SIL_CONFIGS_MACOSX_H
#define SIL_CONFIGS_MACOSX_H

/*************************************************************************/
/*************************************************************************/

#define SIL_PLATFORM_MACOSX
#define SIL_SYSTEM_COMMON_HEADER  "sysdep/macosx/common.h"

#if defined(__x86_64__)
# define SIL_ARCH_X86
# define SIL_ARCH_X86_64
#else
# error Unsupported CPU architecture!
#endif

/* Pick up endianness from <machine/endian.h>.  This includes <sys/cdefs.h>
 * and <sys/_endian.h>, but the former will end up being included anyway,
 * and the latter just defines the ntoh*() group of macros, which we get
 * rid of below. */
#include <machine/endian.h>
#undef ntohs
#undef ntohl
#undef htons
#undef htonl
#undef NTOHS
#undef NTOHL
#undef HTONS
#undef HTONL
#if __DARWIN_BYTE_ORDER == __DARWIN_LITTLE_ENDIAN
# define IS_LITTLE_ENDIAN
#elif __DARWIN_BYTE_ORDER == __DARWIN_BIG_ENDIAN
# define IS_BIG_ENDIAN
#else
# error Unsupported endianness!
#endif

#define HAVE_STRDUP
#define HAVE_STRCASECMP
#define STRNICMP_SIZE_T  size_t

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_CONFIGS_MACOSX_H
