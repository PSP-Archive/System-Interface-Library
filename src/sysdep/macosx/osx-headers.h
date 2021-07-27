/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/macosx/osx-headers.h: Header which wraps base OSX headers to
 * avoid compiler errors.
 */

/*
 * The system headers CoreFoundation/CFBase.h and CoreServices/CoreServices.h
 * make their own use of the DEBUG symbol (the latter through an internal
 * Debugging.h header), so we include them here with our definition of DEBUG
 * hidden.  All sources which include OSX-specific headers should include
 * this header first to ensure that these headers are properly processed.
 */

#ifndef SIL_SRC_SYSDEP_MACOSX_OSX_HEADERS_H
#define SIL_SRC_SYSDEP_MACOSX_OSX_HEADERS_H

/*************************************************************************/
/*************************************************************************/

#ifdef DEBUG
# define SIL_DEBUG
# undef DEBUG
#endif

#include <CoreFoundation/CFBase.h>
#include <CoreServices/CoreServices.h>

#ifdef SIL_DEBUG
# undef SIL_DEBUG
# define DEBUG
#endif

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_MACOSX_OSX_HEADERS_H
