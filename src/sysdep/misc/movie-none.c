/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/misc/movie-none.c: sys_movie_*() implementation for systems
 * with no native movie playback support.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/sysdep.h"

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

SysMovieHandle *sys_movie_open(struct SysFile *fh, UNUSED int64_t offset,
                               UNUSED int length, UNUSED int smooth_chroma)
{
    sys_file_close(fh);
    return NULL;
}

void sys_movie_close(UNUSED SysMovieHandle *movie) {}  // NOTREACHED

int sys_movie_width(UNUSED SysMovieHandle *movie) {return 0;}  // NOTREACHED

int sys_movie_height(UNUSED SysMovieHandle *movie) {return 0;}  // NOTREACHED

double sys_movie_framerate(UNUSED SysMovieHandle *movie) {return 0;}  // NOTREACHED

void sys_movie_set_volume(UNUSED SysMovieHandle *movie, UNUSED float volume) {}  // NOTREACHED

int sys_movie_play(UNUSED SysMovieHandle *movie) {return 0;}  // NOTREACHED

void sys_movie_stop(UNUSED SysMovieHandle *movie) {}  // NOTREACHED

int sys_movie_get_texture(  // NOTREACHED
    UNUSED SysMovieHandle *movie,
    UNUSED float *left_ret, UNUSED float *right_ret,
    UNUSED float *top_ret, UNUSED float *bottom_ret) {return 0;}  // NOTREACHED

int sys_movie_draw_frame(UNUSED SysMovieHandle *movie) {return 0;}  // NOTREACHED

/*************************************************************************/
/*************************************************************************/
