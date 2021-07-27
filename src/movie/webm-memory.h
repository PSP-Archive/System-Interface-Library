/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/webm-memory.h: Wrappers for malloc() and friends for use in the
 * WebM decoder libraries.
 */

/*
 * This header is not intended to be included in SIL sources; instead, it
 * is intended to be included from the command line when building sources
 * from the libvpx, libwebmdec, and nestegg libraries, allowing clean
 * replacement of calls to the malloc() family of functions without
 * invasive modification of the respective library sources.
 */

#ifndef SIL_SRC_MOVIE_WEBM_MEMORY_H
#define SIL_SRC_MOVIE_WEBM_MEMORY_H

/*************************************************************************/
/*************************************************************************/

#include "include/SIL/base.h"
#include "include/SIL/memory.h"

#undef malloc
#undef calloc
#undef realloc
#undef free
#undef strdup

#define malloc(size)        mem_alloc((size), 0, 0)
#define calloc(nmemb,size)  mem_alloc((nmemb)*(size), 0, MEM_ALLOC_CLEAR)
#define realloc(ptr,size)   mem_realloc((ptr), (size), 0)
#define free(ptr)           mem_free((ptr))
#define strdup(str)         mem_strdup((str))

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_MOVIE_WEBM_MEMORY_H
