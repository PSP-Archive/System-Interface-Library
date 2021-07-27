/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/profile.h: Header for PSP-specific profiling functions.
 */

#ifndef SIL_SRC_SYSDEP_PSP_PROFILE_H
#define SIL_SRC_SYSDEP_PSP_PROFILE_H

/*************************************************************************/
/*************************************************************************/

#ifdef DEBUG  // These are only defined when debugging.

/**
 * _profile_start:  Mark the start of a function.  This should be called
 * via the _PROFILE_START macro at the beginning of every function to be
 * profiled.
 *
 * [Parameters]
 *     funcname: Name of function being profiled.
 *     index_ptr: Pointer to a static variable unique to the caller which is
 *         initialized to zero (the contents are private to this function).
 */
extern void _profile_start(const char *funcname, int *index_ptr);

/**
 * _profile_end:  Mark the end of a function.  This should be called via
 * the _PROFILE_END macro at the end of every function to be profiled (as
 * well as any other return points).
 *
 * [Parameters]
 *     index_ptr: Pointer to the static variable passed to _profile_start().
 */
extern void _profile_end(int *index_ptr);

/**
 * _profile_pause:  Pause profiling of the current function.  This may be
 * called via the _PROFILE_PAUSE macro at any time while profiling is
 * running.
 *
 * [Parameters]
 *     index_ptr: Pointer to the static variable passed to _profile_start().
 */
extern void _profile_pause(int *index_ptr);

/**
 * _profile_resume:  Resume profiling of the current function.  This may be
 * be called via the _PROFILE_RESUME macro at any time while profiling is
 * paused.
 *
 * [Parameters]
 *     index_ptr: Pointer to the static variable passed to _profile_start().
 */
extern void _profile_resume(int *index_ptr);

/**
 * _profile_dump:  Print out all current profiling statistics, and reset
 * accumulated call counts and times.  This function may be called at any
 * time, but should be called via the _PROFILE_DUMP macro.
 */
extern void _profile_dump(void);

#endif  // DEBUG

/*-----------------------------------------------------------------------*/

/*
 * Macros for profiling a function.  These should be used as:
 *
 * void function(...)
 * {
 *     _PROFILE_START;
 *     // Function body
 *     // If there are any intermediate "return"s:
 *     if (...) {
 *         _PROFILE_END;
 *         return;
 *     }
 *     // More function body
 *     _PROFILE_END;
 * }
 */

#ifdef DEBUG

# ifdef __GNUC__
#  define _PROFILE_FUNCTION __PRETTY_FUNCTION__
# else
#  define _PROFILE_FUNCTION __FUNCTION__
# endif

# define _PROFILE_START \
    static int _profile_index; \
    _profile_start(_PROFILE_FUNCTION, &_profile_index)
# define _PROFILE_PAUSE \
    _profile_pause(&_profile_index)
# define _PROFILE_RESUME \
    _profile_resume(&_profile_index)
# define _PROFILE_END \
    _profile_end(&_profile_index)
# define _PROFILE_DUMP \
    _profile_dump()

#else  // !DEBUG

# define _PROFILE_START  /*nothing*/
# define _PROFILE_END    /*nothing*/
# define _PROFILE_DUMP   /*nothing*/

#endif

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_PSP_PROFILE_H
