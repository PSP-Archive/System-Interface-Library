/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/random.c: Random number generation routines.
 */

#include "src/base.h"
#include "src/random.h"
#include "src/sysdep.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Global seed (state) value for random32() and other non-reentrant
 * functions.  Initialized to 1 to match the C99 standard. */
uint64_t _sil_random_state = 1;

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

/*
 * These functions are declared inline in random.h.  The "extern inline"
 * declarations here tell the compiler to emit standalone copies of the
 * functions, in case non-inlined versions are needed by other code (such
 * as if other code takes a pointer to the function or if a non-C99 source
 * file includes <SIL/random.h>).
 */

extern inline int32_t random32(void);
extern inline uint32_t urandom32(void);
extern inline int64_t random64(void);
extern inline uint64_t urandom64(void);
extern inline double frandom(void);
extern inline float frandomf(void);
extern inline int32_t random32_r(uint64_t *state);
extern inline uint32_t urandom32_r(uint64_t *state);
extern inline int64_t random64_r(uint64_t *state);
extern inline uint64_t urandom64_r(uint64_t *state);
extern inline double frandom_r(uint64_t *state);
extern inline float frandomf_r(uint64_t *state);

/*-----------------------------------------------------------------------*/

void srandom32(uint32_t seed)
{
    _sil_random_state = (uint64_t)seed;
}

/*-----------------------------------------------------------------------*/

void srandom64(uint64_t seed)
{
    _sil_random_state = seed;
}

/*-----------------------------------------------------------------------*/

void srandom_env(void)
{
    /* Add rather than simply assigning, so that (for example) if
     * sys_random_seed() returns the same value for calls within the same
     * second, we still get a different random sequence. */
    _sil_random_state += sys_random_seed();
}

/*************************************************************************/
/*************************************************************************/
