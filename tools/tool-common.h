/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * tools/tool-common.h: Common includes and macro definitions for tool
 * programs.  Essentially a tweaked subset of base.h and endian.h.
 */

#ifndef SIL_TOOLS_TOOL_COMMON_H
#define SIL_TOOLS_TOOL_COMMON_H

/*************************************************************************/
/*************************************************************************/

#ifdef __i386__
# define SIL_ARCH_X86
# define SIL_ARCH_X86_32
# define USE_SSE2
#endif

#ifdef __amd64__
# define SIL_ARCH_X86
# define SIL_ARCH_X86_64
# define USE_SSE2
#endif

#ifndef __MINGW32__
# define _POSIX_C_SOURCE  201112L
#endif

/*-----------------------------------------------------------------------*/

#define index  _sil__HIDDEN_index
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#undef index

#ifdef _MSC_VER
# define stricmp _stricmp
# define strnicmp _strnicmp
#else
# include <strings.h>
# define stricmp strcasecmp
# define strnicmp strncasecmp
#endif

#if defined(__GNUC__) || defined(__clang__)
# define ALIGNED(n)             __attribute__((aligned(n)))
# define CONST_FUNCTION         __attribute__((const))
# define FORMAT(fmt,firstarg)   __attribute__((format(printf,fmt,firstarg)))
# define PURE_FUNCTION          __attribute__((pure))
#elif defined(_MSC_VER)
# define ALIGNED(n)             __declspec(align(n))
# define CONST_FUNCTION         /*nothing*/
# define FORMAT                 /*nothing*/
# define PURE_FUNCTION          /*nothing*/
#else
# error Please define ALIGNED() for this compiler.
#endif

#define LIKELY(x) (x)
#define UNLIKELY(x) (x)

#define EXTERN_C_BEGIN  /*nothing*/
#define EXTERN_C_END    /*nothing*/

/*-----------------------------------------------------------------------*/

#undef min
#undef max
#define min(a,b)  ((a) < (b) ? (a) : (b))
#define max(a,b)  ((a) > (b) ? (a) : (b))

#define lbound(x,lower)       max((x), (lower))
#define ubound(x,upper)       min((x), (upper))
#define bound(x,lower,upper)  ubound(lbound((x), (lower)), (upper))

static CONST_FUNCTION inline unsigned long align_up(unsigned long x, unsigned int align)
    {return (x + (align-1)) / align * align;}
static CONST_FUNCTION inline unsigned long align_down(unsigned long x, unsigned int align)
    {return x / align * align;}

#define lenof(array)  ((int)(sizeof(array) / sizeof(*(array))))

#define mem_clear(ptr,size)  memset((ptr), 0, (size))
#define mem_fill8(ptr,val,size)  memset((ptr), (val), (size))

/*-----------------------------------------------------------------------*/

#include "../src/endian.h"

/*-----------------------------------------------------------------------*/

#define DLOG(...)  do {                                                 \
    fprintf(stderr, "%s:%u(%s): ", __FILE__, __LINE__, __FUNCTION__);   \
    fprintf(stderr, "" __VA_ARGS__);                                    \
    fputc('\n', stderr);                                                \
} while (0)

#define ASSERT(condition,...) do {                                      \
    if (!(condition)) {                                                 \
        fprintf(stderr, "%s:%u(%s)"                                     \
                "\n\n*** ALERT *** ASSERTION FAILED\n%s\n\n",           \
                __FILE__, __LINE__, __FUNCTION__, #condition);          \
        /* We never execute failure actions in tools, but we retain the \
         * optional parameter for syntax consistency with the main code. */ \
        if (0) {__VA_ARGS__;}                                           \
        abort();                                                        \
    }                                                                   \
} while (0)

#define PRECOND(condition,...) do {                                     \
    if (!(condition)) {                                                 \
        fprintf(stderr, "%s:%u(%s)"                                     \
                "\n\n*** ALERT *** PRECONDITION FAILED\n%s\n\n",        \
                __FILE__, __LINE__, __FUNCTION__, #condition);          \
        if (0) {__VA_ARGS__;}                                           \
        abort();                                                        \
    }                                                                   \
} while (0)

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_TOOLS_TOOL_COMMON_H
