/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/random.h: Header for random number generation routines.
 */

/*
 * This header declares an interface to a linear congruential pseudo-random
 * number generator whose behavior is consistent for a given seed value
 * regardless of the runtime environment.  The exact generator formula is:
 *
 *     x[n+1] = ((x[n] * 6364136223846793005) + 1) mod 2^64
 *
 * where x[n] is the internal generator state at iteration n.  The actual
 * value returned to the caller is the upper B bits of the new state, where
 * B is the bit width of the value to be returned (for floating point
 * functions, B can vary depending on the floating point data format and
 * the value itself).  This generator is believed to be of reasonably good,
 * though not cryptographic, quality; in particular, urandom32() passes the
 * dieharder (http://www.phy.duke.edu/~rgb/General/dieharder.php) tests as
 * of dieharder version 3.31.1.
 *
 * Note that the low-end bits of random64() and urandom64() are less random
 * than the high-end bits.  If you need a 64-bit value with high randomness
 * in all bits, concatenate two 32-bit random numbers instead.
 *
 * Entropy can be injected into the random number stream by, for example:
 *    - Extracting a new random number from the stream at intervals defined
 *      by external events, such as when an input event is received.
 *    - Modifying the random number seed based on external data, such as
 *      the current state of input devices.  (For the non-reentrant
 *      interfaces, this entails retrieving the seed with urandom64(),
 *      modifying the seed, then applying it with srandom64().)
 *
 * This header includes declarations of both reentrant and non-reentrant
 * versions of the base random number functions.  The reentrant versions
 * (random32_r() and similar) accept a state buffer parameter, much like
 * the similarly named rand_r() in POSIX, allowing deterministic output
 * across multiple threads.  If determinism is not required, the
 * non-reentrant functions are safe to use in a multithreaded environment.
 *
 * In an environment with a 32-bit int type, the random32() and srandom32()
 * interfaces comply with the specifications of rand() and srand(),
 * respectively, in the C99 standard.  Thus, one can redefine the standard
 * functions as follows:
 *     #define rand      random32
 *     #define srand     srandom32
 *     #define RAND_MAX  0x7FFFFFFF
 * and still get expected behavior.  The random32_r() interface differs
 * slightly from the POSIX-specific rand_r() function in that random32_r()
 * takes a 64-bit state pointer rather than an "unsigned int" state
 * pointer.
 */

#ifndef SIL_RANDOM_H
#define SIL_RANDOM_H

EXTERN_C_BEGIN

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
# define INLINE_OR_EXTERN inline
# include <float.h>  // For FLT_RADIX, etc.
#else
# define INLINE_OR_EXTERN extern
#endif

/*************************************************************************/
/*************************************************************************/

/**
 * random32():  Return a pseudo-random integer from 0 to 0x7FFF'FFFF,
 * inclusive.
 *
 * Note that when called as an inline function, the compiler will typically
 * be able to determine that the return value is always nonnegative, so
 * there is no need to call the unsigned versions of these functions to
 * optimize a following modulo operation (for example).
 *
 * [Return value]
 *     Pseudo-random number.
 */
INLINE_OR_EXTERN int32_t random32(void);

/**
 * urandom32():  Return a pseudo-random integer from 0 to 0xFFFF'FFFF,
 * inclusive.
 *
 * [Return value]
 *     Pseudo-random number.
 */
INLINE_OR_EXTERN uint32_t urandom32(void);

/**
 * random64():  Return a pseudo-random integer from 0 to
 * 0x7FFF'FFFF'FFFF'FFFF, inclusive.
 *
 * [Return value]
 *     Pseudo-random number.
 */
INLINE_OR_EXTERN int64_t random64(void);

/**
 * urandom64():  Return a pseudo-random integer from 0 to
 * 0xFFFF'FFFF'FFFF'FFFF, inclusive.  See urandom64_r() for notes on the
 * relationship between the value returned and the current random seed.
 *
 * [Return value]
 *     Pseudo-random number.
 */
INLINE_OR_EXTERN uint64_t urandom64(void);

/**
 * frandom():  Return a pseudo-random double-precision floating point
 * number in the range [0.0,1.0).
 *
 * [Return value]
 *     Pseudo-random number.
 */
INLINE_OR_EXTERN double frandom(void);

/**
 * frandomf():  Return a pseudo-random single-precision floating point
 * number in the range [0.0,1.0).
 *
 * [Return value]
 *     Pseudo-random number.
 */
INLINE_OR_EXTERN float frandomf(void);

/*-----------------------------------------------------------------------*/

/**
 * random32_r():  Return a pseudo-random integer from 0 to 0x7FFF'FFFF,
 * inclusive.
 *
 * [Parameters]
 *     state: Pointer to random number generator state.
 * [Return value]
 *     Pseudo-random number.
 */
INLINE_OR_EXTERN int32_t random32_r(uint64_t *state);

/**
 * urandom32_r():  Return a pseudo-random integer from 0 to 0xFFFF'FFFF,
 * inclusive.
 *
 * [Parameters]
 *     state: Pointer to random number generator state.
 * [Return value]
 *     Pseudo-random number.
 */
INLINE_OR_EXTERN uint32_t urandom32_r(uint64_t *state);

/**
 * random64_r():  Return a pseudo-random integer from 0 to
 * 0x7FFF'FFFF'FFFF'FFFF, inclusive.
 *
 * [Parameters]
 *     state: Pointer to random number generator state.
 * [Return value]
 *     Pseudo-random number.
 */
INLINE_OR_EXTERN int64_t random64_r(uint64_t *state);

/**
 * urandom64_r():  Return a pseudo-random integer from 0 to
 * 0xFFFF'FFFF'FFFF'FFFF, inclusive.
 *
 * The value returned by this function is always equal to the value of the
 * state buffer after the function returns.  In other words, the following
 * assertions always hold (for (2), assuming that no other thread
 * concurrently calls one of the non-reentrant functions):
 *
 * (1) uint64_t state = <any 64-bit value>;
 *     uint64_t value = urandom64_r(&state);
 *     ASSERT(value == state);
 *
 * (2) uint64_t seed = urandom64();
 *     uint64_t value1 = urandom64();
 *     srandom64(seed);
 *     uint64_t value2 = urandom64();
 *     ASSERT(value2 == value1);
 *
 * [Parameters]
 *     state: Pointer to random number generator state.
 * [Return value]
 *     Pseudo-random number.
 */
INLINE_OR_EXTERN uint64_t urandom64_r(uint64_t *state);

/**
 * frandom_r():  Return a pseudo-random double-precision floating point
 * number in the range [0.0,1.0).
 *
 * [Parameters]
 *     state: Pointer to random number generator state.
 * [Return value]
 *     Pseudo-random number.
 */
INLINE_OR_EXTERN double frandom_r(uint64_t *state);

/**
 * frandomf_r():  Return a pseudo-random single-precision floating point
 * number in the range [0.0,1.0).
 *
 * [Parameters]
 *     state: Pointer to random number generator state.
 * [Return value]
 *     Pseudo-random number.
 */
INLINE_OR_EXTERN float frandomf_r(uint64_t *state);

/*-----------------------------------------------------------------------*/

/**
 * srandom32():  Set the seed value for the random number generator used by
 * the random number generator based on the given 32-bit value.
 *
 * If neither this function nor one of the other srandom*() functions is
 * called before obtaining random numbers, the program acts as though
 * srandom32(1) had been called before the first random number was obtained.
 *
 * [Parameters]
 *     seed: Seed value.
 */
extern void srandom32(uint32_t seed);

/**
 * srandom64():  Set the seed value for the random number generator used
 * the random number generator functions to the given 64-bit value.
 *
 * [Parameters]
 *     seed: Seed value.
 */
extern void srandom64(uint64_t seed);

/**
 * srandom_env():  Set the seed value for the random number generator used
 * the random number generator functions based on the current runtime
 * environment.  The exact method used to set the seed is system-dependent,
 * but typically includes factors such as the current time of day.
 */
extern void srandom_env(void);

/*-----------------------------------------------------------------------*/

/**
 * randlimit():  Return a random nonnegative integer strictly less than the
 * given value.
 *
 * [Parameters]
 *     limit: Upper bound (exclusive) for result.  Must be positive.
 * [Return value]
 *     Random number x such that 0 <= x < limit.
 */
static inline int32_t randlimit(int32_t limit) {
    return random32() % limit;
}

/**
 * randrange():  Return a random nonnegative integer in the given range.
 *
 * [Parameters]
 *     low: Lower bound (inclusive) for result.
 *     high: Upper bound (exclusive) for result.  Must satisfy high > low.
 * [Return value]
 *     Random number x such that low <= x < high.
 */
static inline int32_t randrange(int32_t low, int32_t high) {
    return (int32_t)(low + urandom32() % ((uint32_t)high - (uint32_t)low));
}

/*-----------------------------------------------------------------------*/

/*
 * The definitions of the above (non-static) inline functions follow.
 * These are also instantiated within SIL, so that client code which is
 * not compatible with C99 inline declarations can still make use of the
 * functions (albeit at a slight performance cost).
 */

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L

/* To avoid code duplication: */
#define _sil_RNG_FUNC(x)  (((x) * 6364136223846793005ULL) + 1)

inline int32_t random32(void) {
    extern uint64_t _sil_random_state;
    return random32_r(&_sil_random_state);
}

inline uint32_t urandom32(void) {
    extern uint64_t _sil_random_state;
    return urandom32_r(&_sil_random_state);
}

inline int64_t random64(void) {
    extern uint64_t _sil_random_state;
    return random64_r(&_sil_random_state);
}

inline uint64_t urandom64(void) {
    extern uint64_t _sil_random_state;
    return urandom64_r(&_sil_random_state);
}

inline double frandom(void) {
    extern uint64_t _sil_random_state;
    return frandom_r(&_sil_random_state);
}

inline float frandomf(void) {
    extern uint64_t _sil_random_state;
    return frandomf_r(&_sil_random_state);
}

inline int32_t random32_r(uint64_t *state) {
    *state = _sil_RNG_FUNC(*state);
    return *state >> 33;
}

inline uint32_t urandom32_r(uint64_t *state) {
    *state = _sil_RNG_FUNC(*state);
    return *state >> 32;
}

inline int64_t random64_r(uint64_t *state) {
    *state = _sil_RNG_FUNC(*state);
    return *state >> 1;
}

inline uint64_t urandom64_r(uint64_t *state) {
    *state = _sil_RNG_FUNC(*state);
    return *state;
}

inline double frandom_r(uint64_t *state) {
    *state = _sil_RNG_FUNC(*state);
    /* Care is needed here because simply converting the state value to
     * floating point and dividing by 2^64 will result in an output of 1.0
     * if the state value is greater than 2^64 * (1.0 - 0.5 ulp). */
    #if FLT_RADIX == 2
        #if DBL_MANT_DIG >= 64  // Typically never true (53 for IEEE 754).
             return (double)(*state) / ((double)(1ULL<<63) * 2);
        #else
             return (double)(*state >> (64 - DBL_MANT_DIG))
                    / (double)(1ULL << DBL_MANT_DIG);
        #endif
    #else
        double result = (double)(*state) / ((double)(1ULL<<63) * 2);
        if (UNLIKELY(result == 1.0)) {
            result = 0.0;
        }
        return result;
    #endif
}

inline float frandomf_r(uint64_t *state) {
    *state = _sil_RNG_FUNC(*state);
    #if FLT_RADIX == 2
        #if FLT_MANT_DIG >= 64  // Typically never true (24 for IEEE 754).
             return (float)(*state) / ((float)(1ULL<<63) * 2);
        #else
             return (float)(*state >> (64 - FLT_MANT_DIG))
                    / (float)(1ULL << FLT_MANT_DIG);
        #endif
    #else
        float result = (float)(*state) / ((float)(1ULL<<63) * 2);
        if (UNLIKELY(result == 1.0)) {
            result = 0.0;
        }
        return result;
    #endif
}

#undef _sil_RNG_FUNC

#endif  // defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L

/*************************************************************************/
/*************************************************************************/

#undef INLINE_OR_EXTERN

EXTERN_C_END

#endif  // SIL_RANDOM_H
