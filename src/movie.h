/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/movie.h: Internal header for movie playback functionality.
 */

#ifndef SIL_SRC_MOVIE_H
#define SIL_SRC_MOVIE_H

#include "SIL/movie.h"  // Include the public header.

struct SysMovieHandle;

/*************************************************************************/
/*************************************************************************/

/**
 * movie_import:  Assign a movie ID to the given system-level movie handle
 * (SysMovieHandle object).  After a successful return from this function,
 * the SysMovieHandle object belongs to the high-level movie manager and
 * should not be destroyed or otherwise manipulated by the caller.
 *
 * [Parameters]
 *     sysmovie: System-level movie handle (SysMovieHandle object) to import.
 * [Return value]
 *     Newly assigned movie ID (nonzero), or zero on error.
 */
extern int movie_import(struct SysMovieHandle *sysmovie);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_MOVIE_H
