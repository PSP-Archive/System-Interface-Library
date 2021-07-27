/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/endian.h: Inline functions for endian conversion.
 */

/*
 * This file defines the following inline functions, useful for converting
 * data to and from native byte order:
 *    - is_little_endian(), for checking the endianness of the environment.
 *    - be_to_*(), for converting values from big-endian to native format.
 *    - *_to_be(), for converting values from native format to big-endian.
 * (The *_to_be() functions are actually just macros defined to the
 * equivalent be_to_*() functions, since the same function can be used to
 * convert in either direction; the alternate names are supplied only for
 * convenience.)
 *
 * While these are low-level functions, they are not included as part of
 * base.h because they are typically only applicable to the limited case
 * of I/O on binary data, and should not be relied on by other code.
 */

#ifndef SIL_ENDIAN_H
#define SIL_ENDIAN_H

EXTERN_C_BEGIN

/*************************************************************************/
/*************************************************************************/

/**
 * is_little_endian:  Return true if the execution environment is a
 * little-endian environment, false if a big-endian environment.
 */
#if defined(IS_LITTLE_ENDIAN)
# define is_little_endian()  1
#elif defined(IS_BIG_ENDIAN)
# define is_little_endian()  0
#else
# error Endianness unknown!
#endif

/*-----------------------------------------------------------------------*/

/**
 * be_to_s16, be_to_u16, be_to_s32, be_to_u32, be_to_s64, be_to_u64,
 * be_to_float, be_to double:  Convert big-endian values to native byte
 * order (signed/unsigned 16-bit, signed/unsigned 32-bit, signed/unsigned
 * 64-bit, single-precision floating point, and double-precision floating
 * point respectively).
 */
static CONST_FUNCTION inline int16_t be_to_s16(const int16_t val) {
    if (!is_little_endian()) {return val;}
    return (val>>8 & 0xFF) | val<<8;
}

static CONST_FUNCTION inline uint16_t be_to_u16(const uint16_t val) {
    if (!is_little_endian()) {return val;}
    return val>>8 | val<<8;
}

static CONST_FUNCTION inline int32_t be_to_s32(const int32_t val) {
    if (!is_little_endian()) {return val;}
#ifdef __GNUC__
    return __builtin_bswap32(val);
#else
    return (val>>24 & 0xFF) | (val>>8 & 0xFF00) | (val & 0xFF00)<<8 | val<<24;
#endif
}

static CONST_FUNCTION inline uint32_t be_to_u32(const uint32_t val) {
    if (!is_little_endian()) {return val;}
#ifdef __GNUC__
    return __builtin_bswap32(val);
#else
    return val>>24 | (val>>8 & 0xFF00) | (val & 0xFF00)<<8 | val<<24;
#endif
}

static CONST_FUNCTION inline int64_t be_to_s64(const int64_t val) {
    if (!is_little_endian()) {return val;}
#ifdef __GNUC__
    return __builtin_bswap64(val);
#else
    return (val>>56 & 0xFF) | (val>>40 & 0xFF00) | (val>>24 & 0xFF0000)
         | (val>>8 & 0xFF000000U) | ((val & 0xFF000000U)<<8)
         | (val & 0xFF0000)<<24 | (val & 0xFF00)<<40 | val<<56;
#endif
}

static CONST_FUNCTION inline uint64_t be_to_u64(const uint64_t val) {
    if (!is_little_endian()) {return val;}
#ifdef __GNUC__
    return __builtin_bswap64(val);
#else
    return val>>56 | (val>>40 & 0xFF00) | (val>>24 & 0xFF0000)
         | (val>>8 & 0xFF000000U) | (val & 0xFF000000U)<<8
         | (val & 0xFF0000)<<24 | (val & 0xFF00)<<40 | val<<56;
#endif
}

static CONST_FUNCTION inline float be_to_float(const float val) {
    if (!is_little_endian()) {return val;}
    union {float f; uint32_t i;} u;
    u.f = val;
    u.i = be_to_u32(u.i);
    return u.f;
}

static CONST_FUNCTION inline double be_to_double(const double val) {
    if (!is_little_endian()) {return val;}
    union {double f; uint64_t i;} u;
    u.f = val;
    u.i = be_to_u64(u.i);
    return u.f;
}

/*-----------------------------------------------------------------------*/

/**
 * le_to_s16, le_to_u16, le_to_s32, le_to_u32, le_to_s64, le_to_u64,
 * le_to_float, le_to double:  Convert little-endian values to native byte
 * order (signed/unsigned 16-bit, signed/unsigned 32-bit, signed/unsigned
 * 64-bit, single-precision floating point, and double-precision floating
 * point respectively).
 */
static CONST_FUNCTION inline int16_t le_to_s16(const int16_t val) {
    if (is_little_endian()) {return val;}
    return (val>>8 & 0xFF) | val<<8;
}

static CONST_FUNCTION inline uint16_t le_to_u16(const uint16_t val) {
    if (is_little_endian()) {return val;}
    return val>>8 | val<<8;
}

static CONST_FUNCTION inline int32_t le_to_s32(const int32_t val) {
    if (is_little_endian()) {return val;}
#ifdef __GNUC__
    return __builtin_bswap32(val);
#else
    return (val>>24 & 0xFF) | (val>>8 & 0xFF00) | (val & 0xFF00)<<8 | val<<24;
#endif
}

static CONST_FUNCTION inline uint32_t le_to_u32(const uint32_t val) {
    if (is_little_endian()) {return val;}
#ifdef __GNUC__
    return __builtin_bswap32(val);
#else
    return val>>24 | (val>>8 & 0xFF00) | (val & 0xFF00)<<8 | val<<24;
#endif
}

static CONST_FUNCTION inline int64_t le_to_s64(const int64_t val) {
    if (is_little_endian()) {return val;}
#ifdef __GNUC__
    return __builtin_bswap64(val);
#else
    return (val>>56 & 0xFF) | (val>>40 & 0xFF00) | (val>>24 & 0xFF0000)
         | (val>>8 & 0xFF000000U) | (val & 0xFF000000U)<<8
         | (val & 0xFF0000)<<24 | (val & 0xFF00)<<40 | val<<56;
#endif
}

static CONST_FUNCTION inline uint64_t le_to_u64(const uint64_t val) {
    if (is_little_endian()) {return val;}
#ifdef __GNUC__
    return __builtin_bswap64(val);
#else
    return val>>56 | (val>>40 & 0xFF00) | (val>>24 & 0xFF0000)
         | (val>>8 & 0xFF000000U) | (val & 0xFF000000U)<<8
         | (val & 0xFF0000)<<24 | (val & 0xFF00)<<40 | val<<56;
#endif
}

static CONST_FUNCTION inline float le_to_float(const float val) {
    if (is_little_endian()) {return val;}
    union {float f; uint32_t i;} u;
    u.f = val;
    u.i = le_to_u32(u.i);
    return u.f;
}

static CONST_FUNCTION inline double le_to_double(const double val) {
    if (is_little_endian()) {return val;}
    union {double f; uint64_t i;} u;
    u.f = val;
    u.i = le_to_u64(u.i);
    return u.f;
}

/*-----------------------------------------------------------------------*/

/**
 * s16_to_be, u16_to_be, s32_to_be, u32_to_be, s64_to_be, u64_to_be,
 * float_to_be, double_to_be:  Convert native values to big-endian byte
 * order (signed/unsigned 16-bit, signed/unsigned 32-bit, signed/unsigned
 * 64-bit, single-precision floating point, and double-precision floating
 * point respectively).
 */
#define s16_to_be(val)     (be_to_s16(val))
#define u16_to_be(val)     (be_to_u16(val))
#define s32_to_be(val)     (be_to_s32(val))
#define u32_to_be(val)     (be_to_u32(val))
#define s64_to_be(val)     (be_to_s64(val))
#define u64_to_be(val)     (be_to_u64(val))
#define float_to_be(val)   (be_to_float(val))
#define double_to_be(val)  (be_to_double(val))

/*-----------------------------------------------------------------------*/

/**
 * s16_to_le, u16_to_le, s32_to_le, u32_to_le, s64_to_le, u64_to_le,
 * float_to_le, double_to_le:  Convert native values to little-endian byte
 * order (signed/unsigned 16-bit, signed/unsigned 32-bit, signed/unsigned
 * 64-bit, single-precision floating point, and double-precision floating
 * point respectively).
 */
#define s16_to_le(val)     (le_to_s16(val))
#define u16_to_le(val)     (le_to_u16(val))
#define s32_to_le(val)     (le_to_s32(val))
#define u32_to_le(val)     (le_to_u32(val))
#define s64_to_le(val)     (le_to_s64(val))
#define u64_to_le(val)     (le_to_u64(val))
#define float_to_le(val)   (le_to_float(val))
#define double_to_le(val)  (le_to_double(val))

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_ENDIAN_H
