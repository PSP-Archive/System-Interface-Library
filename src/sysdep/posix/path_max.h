/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/posix/path_max.h: Definition of PATH_MAX ("maximum" pathname
 * length) for POSIX systems.
 */

#ifndef SIL_SRC_SYSDEP_POSIX_PATH_MAX_H
#define SIL_SRC_SYSDEP_POSIX_PATH_MAX_H

/*************************************************************************/
/*************************************************************************/

/*
 * PATH_MAX was formerly the maximum length of a pathname.  In modern
 * environments, the limit is often dynamically configurable, or there
 * may not even be an OS-imposed limit.  However, since we don't need to
 * deal with arbitrary user-specified pathnames, we just define PATH_MAX
 * to a reasonable size if it's not defined by the system, and use that as
 * the size for various pathname buffers.
 */

#include <limits.h>

#ifndef PATH_MAX
# define PATH_MAX  4096  // Big enough for our purposes.
#endif

#if defined(SIL_PLATFORM_MACOSX) && PATH_MAX == 1024
/* On at least OSX 10.7, PATH_MAX is defined as 1024, but tests show that
 * opendir() fails at 1017 characters.  Work around the brokenness. */
# undef PATH_MAX
# define PATH_MAX  1016
#endif

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_POSIX_PATH_MAX_H
