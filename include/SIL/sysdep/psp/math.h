/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/sysdep/psp/math.h: Definitions of PSP-specific versions of
 * math.h functions.
 */

#ifndef SIL_SYSDEP_PSP_MATH_H
#define SIL_SYSDEP_PSP_MATH_H

EXTERN_C_BEGIN

/*************************************************************************/
/*************************************************************************/

/* Optimized float-to-int conversion and related mathematical functions.
 * The i*() functions (ifloorf(), etc.) do _not_ check for out-of-range
 * values. */

/*----------------------------------*/

/* These macros define a function which accepts a floating-point parameter
 * and returns a floating-point or integer value, respectively.  The
 * assembly instruction(s) implementing the function should be passed in
 * the insn parameter to the macro.  The assembly should read the function
 * parameter from %[x] and store the result to %[result].  A dummy register
 * of the type opposite the function's return value (an integer register
 * for floating-point functions, a floating-point register for integer
 * functions) is available in %[dummy]. */

#define DEFINE_FUNC(name,insn) \
static inline CONST_FUNCTION float name(const float x) { \
    int32_t dummy;                        \
    float result;                         \
    __asm__(                              \
        ".set push; .set noreorder\n"     \
        "mfc1 %[dummy], %[x]\n"           \
        "nop\n"                           \
        "ext %[dummy], %[dummy], 23, 8\n" \
        "sltiu %[dummy], %[dummy], 0x98\n"\
        "beqzl %[dummy], 0f\n"            \
        "mov.s %[result], %[x]\n"         \
        insn                              \
        "\n0: .set pop"                   \
        : [result] "=f" (result), [dummy] "=r" (dummy) : [x] "f" (x)); \
    return result;                        \
}

#define DEFINE_IFUNC(name,insn)                                         \
static inline CONST_FUNCTION int32_t name(const float x) {              \
    float dummy;                                                        \
    int32_t result;                                                     \
    __asm__(insn                                                        \
            : [result] "=r" (result), [dummy] "=f" (dummy) : [x] "f" (x)); \
    return result;                                                      \
}

/*----------------------------------*/

#undef floorf
#define floorf psp_floorf
DEFINE_FUNC(floorf, "floor.w.s %[result],%[x]; cvt.s.w %[result],%[result]")

#undef truncf
#define truncf psp_truncf
DEFINE_FUNC(truncf, "trunc.w.s %[result],%[x]; cvt.s.w %[result],%[result]")

#undef ceilf
#define ceilf psp_ceilf
DEFINE_FUNC(ceilf,  "ceil.w.s  %[result],%[x]; cvt.s.w %[result],%[result]")

#undef roundf
#define roundf psp_roundf
DEFINE_FUNC(roundf, "round.w.s %[result],%[x]; cvt.s.w %[result],%[result]")

/*----------------------------------*/

#undef ifloorf
#define ifloorf psp_ifloorf
DEFINE_IFUNC(ifloorf, "floor.w.s %[dummy],%[x]; mfc1 %[result],%[dummy]")

#undef itruncf
#define itruncf psp_itruncf
DEFINE_IFUNC(itruncf, "trunc.w.s %[dummy],%[x]; mfc1 %[result],%[dummy]")

#undef iceilf
#define iceilf psp_iceilf
DEFINE_IFUNC(iceilf,  "ceil.w.s  %[dummy],%[x]; mfc1 %[result],%[dummy]")

#undef iroundf
#define iroundf psp_iroundf
DEFINE_IFUNC(iroundf, "round.w.s %[dummy],%[x]; mfc1 %[result],%[dummy]")

/*----------------------------------*/

#undef fracf
#define fracf psp_fracf
static inline CONST_FUNCTION float psp_fracf(const float x) {
    float result;
    __asm__(
        "floor.w.s %[result],%[x];"
        "cvt.s.w %[result],%[result];"
        "sub.s %[result],%[x],%[result]"
        : [result] "=&f" (result) : [x] "f" (x)
    );
    return result;
}

/*----------------------------------*/

/* GCC's -ffast-math can turn isinf() and isnan() into constant false values
 * on MIPS, so we implement them manually. */

#undef isinf
#define isinf(x)  (__builtin_types_compatible_p(typeof(x), float)       \
    ? __extension__({                                                   \
        unsigned int bits;                                              \
        __asm__("mfc1 %0,%1; nop" : "=r" (bits) : "f" (x));             \
        bits==0x7F800000 ? 1 : bits==0xFF800000 ? -1 : 0;               \
    }) : __extension__({                                                \
        unsigned int lo_bits, hi_bits;                                  \
        __asm__("lw %0,%1" : "=r" (lo_bits) : "m" (x));                 \
        __asm__("lw %0,4+%1" : "=r" (hi_bits) : "m" (x));               \
        lo_bits != 0 ? 0 :                                              \
            hi_bits==0x7FF00000 ? 1 : hi_bits==0xFFF00000 ? -1 : 0;     \
    }))

#undef isnan
#define isnan(x)  (__builtin_types_compatible_p(typeof(x), float)       \
    ? __extension__({                                                   \
        unsigned int bits;                                              \
        __asm__("mfc1 %0,%1; nop" : "=r" (bits) : "f" (x));             \
        (bits & 0x7FFFFFFF) > 0x7F800000;                               \
    }) : __extension__({                                                \
        unsigned int lo_bits, hi_bits;                                  \
        __asm__("lw %0,%1" : "=r" (lo_bits) : "m" (x));                 \
        __asm__("lw %0,4+%1" : "=r" (hi_bits) : "m" (x));               \
        hi_bits &= 0x7FFFFFFF;                                          \
        hi_bits > 0x7FF00000 || (hi_bits == 0x7FF00000 && lo_bits != 0);\
    }))

/*----------------------------------*/

#undef DEFINE_FUNC
#undef DEFINE_IFUNC

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_SYSDEP_PSP_MATH_H
