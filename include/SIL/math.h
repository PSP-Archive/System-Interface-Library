/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/math.h: Mathematical type and function declarations.
 */

#ifndef SIL_MATH_H
#define SIL_MATH_H

/*
 * This file includes the system <math.h> header, thereby providing all
 * relevant declarations from that header (and specifically HUGE_VALF and
 * HUGE_VAL even if not provided by the system), and also provides the
 * following facilities:
 *
 * - Single- and double- precision declarations of pi (as M_PIf and M_PI,
 *   respectively).
 *
 * - frac() and fracf(), which return the fractional part of their
 *   argument.  frac(x) is roughly equivalent to fmod(x,1.0), but returns
 *   a positive value even for negative arguments.
 *
 * - ifloor[f](), iceil[f](), itrunc[f](), and iround[f](), which convert
 *   their floating-point argument to "int" type after rounding.  On some
 *   systems, this may be faster than typecasting the result of the
 *   corresponding floating-point function.  (In particular, itrunc() and
 *   itruncf() are translated to a single instruction for x86 targets.)
 *
 * - Single-precision trigonometric functions which use degrees instead of
 *   radians as units: dsinf(), dcosf(), dtanf(), dsincosf(), dasinf(),
 *   dacosf(), and datan2f().
 *
 * - 2-, 3-, and 4-component single-precision floating point vector and
 *   4x4-element matrix types.  For C++, vector operation functions are
 *   also provided as class methods or operator overloads.
 *
 * Note that the following traditional but nonstandard functions (and their
 * ...f()/...l() versions) are explicitly _not_ available, in order to
 * avoid identifier clashes with common variable names:
 *    gamma(), j0(), j1(), y0(), y1()
 */

/*************************************************************************/
/********** Standard library declarations and related constants **********/
/*************************************************************************/

/* Avoid environmental pollution. */
#define gamma   _sil__HIDDEN_gamma
#define gammaf  _sil__HIDDEN_gammaf
#define gammal  _sil__HIDDEN_gammal
#define j0      _sil__HIDDEN_j0
#define j0f     _sil__HIDDEN_j0f
#define j0l     _sil__HIDDEN_j0l
#define j1      _sil__HIDDEN_j1
#define j1f     _sil__HIDDEN_j1f
#define j1l     _sil__HIDDEN_j1l
#define y0      _sil__HIDDEN_y0
#define y0f     _sil__HIDDEN_y0f
#define y0l     _sil__HIDDEN_y0l
#define y1      _sil__HIDDEN_y1
#define y1f     _sil__HIDDEN_y1f
#define y1l     _sil__HIDDEN_y1l

#include <math.h>

#undef gamma
#undef gammaf
#undef gammal
#undef j0
#undef j0f
#undef j0l
#undef j1
#undef j1f
#undef j1l
#undef y0
#undef y0f
#undef y0l
#undef y1
#undef y1f
#undef y1l

#ifndef HUGE_VALF  // In case <math.h> didn't define it.
# define HUGE_VALF  3.4028234e+38f  // Maximum value of a 32-bit IEEE float.
#endif
#ifndef HUGE_VAL
# define HUGE_VAL  1.7976931348623157e+308  // Max value of a 64-bit IEEE float.
#endif

#undef M_PI  // Old <math.h> may define this; let's consistently use ours.
#define M_PI  3.1415926535897932

#define M_PIf  3.14159265f

/*************************************************************************/
/*********************** Extra rounding functions ************************/
/*************************************************************************/

/**
 * frac, fracf:  Return the fractional part of the floating-point argument.
 * The value returned is always positive, such that x = floor(x) + frac(x).
 * (For negative x very close to zero, this function may return 1.0.)
 */
static CONST_FUNCTION inline double frac(double x) {return x - floor(x);}
static CONST_FUNCTION inline float fracf(float x) {return x - floorf(x);}

/*-----------------------------------------------------------------------*/

/**
 * iceil[f], ifloor[f], iround[f], itrunc[f]:  Convenience functions for
 * floating-point rounding which convert the result of rounding to type
 * "int".  If the result does not fit in a value of type "int", the return
 * value is undefined.
 *
 * Note that itrunc() and itruncf() are exactly equivalent to (and are
 * implemented as) typecasting the floating-point input value to "int";
 * these functions are provided for parallelism with the other rounding types.
 */

#undef SIL__IROUND_DEFINED

#if defined(__GNUC__) && defined(SIL_ARCH_X86) && defined(SIL_MATH_ASSUME_ROUND_TO_NEAREST)
    #if defined(__clang__)
        /* If we offer the m alternative to Clang, it will load a memory
         * argument, store it to a local stack slot, and then run the
         * instruction on the stack slot instead of just running the insn
         * on the original value (true at least as of Clang 6.0), so we
         * suppress the "m" alternative when building with Clang. */
        #define SIL__IN_CONSTRAINT "x"
    #else
        #define SIL__IN_CONSTRAINT "xm"
    #endif
    #define SIL__DEFINE_IFUNC(name, type, insn) \
        static CONST_FUNCTION inline int name(type x) { \
            int result; \
            __asm__(#insn " %1, %0" : "=r" (result) : SIL__IN_CONSTRAINT (x));\
            return result; \
        }
    SIL__DEFINE_IFUNC(iround, double, cvtsd2si)
    SIL__DEFINE_IFUNC(iroundf, float, cvtss2si)
    #define SIL__IROUND_DEFINED
    #undef SIL__DEFINE_IFUNC
    #undef SIL__IN_CONSTRAINT
#endif

static CONST_FUNCTION inline int iceil(double x)  {return (int)ceil(x);}
static CONST_FUNCTION inline int iceilf(float x)  {return (int)ceilf(x);}
static CONST_FUNCTION inline int ifloor(double x) {return (int)floor(x);}
static CONST_FUNCTION inline int ifloorf(float x) {return (int)floorf(x);}
#ifndef SIL__IROUND_DEFINED
static CONST_FUNCTION inline int iround(double x) {return (int)round(x);}
static CONST_FUNCTION inline int iroundf(float x) {return (int)roundf(x);}
#endif
static CONST_FUNCTION inline int itrunc(double x) {return (int)x;}
static CONST_FUNCTION inline int itruncf(float x) {return (int)x;}

#undef SIL__IROUND_DEFINED

/*************************************************************************/
/********************* Extra trigonometric functions *********************/
/*************************************************************************/

/**
 * dsinf, dcosf, dtanf, dsincosf, dtanf:  Trigonometric functions which
 * take their angle argument or return an angle result in units of degrees
 * rather than radians.
 *
 * For angles whose true sine or cosine is 0, 0.5, or 1, or whose true
 * tangent is 0, 1, or infinity, these functions are guaranteed to return
 * the relevant value exactly.  For example, dsin(180) will return exactly
 * 0.0, rather than the very small but nonzero value returned by sin(M_PI),
 * and datan2() will return exact results for multiples of 45 degrees, i.e.
 * when either x or y is 0 or when x == y.
 *
 * Aside from the units of the angle parameter or return value, the
 * parameters and return values for these functions are exactly as for the
 * associated radian-unit functions in the standard math library, except
 * that the sign of zero is not preserved in cases such as dsin(0).
 */

EXTERN_C_BEGIN

extern CONST_FUNCTION float dsinf(const float angle);
extern CONST_FUNCTION float dcosf(const float angle);
extern CONST_FUNCTION float dtanf(const float angle);
extern void dsincosf(const float angle, float *sin_ret, float *cos_ret);
extern CONST_FUNCTION float dasinf(const float x);
extern CONST_FUNCTION float dacosf(const float x);
extern CONST_FUNCTION float datan2f(const float y, const float x);

EXTERN_C_END

/*************************************************************************/
/*********************** System-specific overrides ***********************/
/*************************************************************************/

#ifdef SIL_SYSTEM_MATH_HEADER
# include SIL_SYSTEM_MATH_HEADER
#endif

/*************************************************************************/
/************** Vector and matrix data types and functions ***************/
/*************************************************************************/

/**
 * Vector2f, Vector3f, Vector4f:  2-, 3-, and 4-component single-precision
 * floating point vector types.
 *
 * The following functions are available for operating on vectors.  All
 * functions with a vector result return it in a user-supplied return
 * variable; functions with scalar results return them via the function's
 * return value.
 *     vec[234]_add(dest,src1,src2)    // Add by components
 *     vec[234]_sub(dest,src1,src2)    // Subtract by components
 *     vec[234]_mul(dest,src1,src2)    // Multiply by components
 *     vec[234]_div(dest,src1,src2)    // Divide by components
 *     vec[234]_add_scalar(dest,src,k) // Add to all components
 *     vec[234]_scale(dest,src,k)      // Multiply into all components
 *     vec[234]_length(v)     // Return vector length: sqrtf(x*x + y*y + ...)
 *     vec[234]_length2(v)    // Return square of vector length
 *     vec[234]_normalize(dest,src)    // Normalize a vector
 *     vec[234]_is_nonzero(v)          // Return whether vector is nonzero
 *     vec[234]_is_length_in(v,k)      // Return whether length is <= k
 *     vec[234]_set_length(dest,src,k) // Set vector length
 *     vec[234]_cap_length(dest,src,k) // Set vector length if greater than k
 *     vec[234]_dot(a,b)               // Return dot product
 *     vec3_cross(dest,a,b)   // Return cross product (for 3-component vectors)
 *     vec4_cross(dest,a,b,c) // Return cross product (for 4-component vectors)
 *
 * For C++, the above vector operations can also be performed using
 * operators or object methods, as follows.  For assignment and component
 * arithmetic, the second vector may be any size; extra components in the
 * second vector will be discarded, and missing components will be treated
 * as 0 (for assignment/addition/subtraction) or 1 (for multiplication or
 * division).  For scalar addition and multiplication, the scalar operand
 * may come either first or second.
 *     vec1 = vec2            // Copy a vector
 *     +vec1                  // Unary + (return vector as is)
 *     -vec1                  // Negation of all components
 *     vec1 + scalar          // Add to all components
 *     vec1 - scalar          // Subtract from all components
 *     vec1 * scalar          // Multiply into all components
 *     vec1 / scalar          // Divide into all components
 *     vec1 + vec2            // Add by components
 *     vec1 - vec2            // Subtract by components
 *     vec1 * vec2            // Multiply by components
 *     vec1 / vec2            // Divide by components
 *     vec1 += -= *= /= scalar
 *     vec1 += -= *= /= vec2
 *     vec1.length()          // Return vector length: sqrtf(x*x + y*y + ...)
 *     vec1.length2()         // Return square of vector length
 *     vec1.is_nonzero()      // Return whether vector is nonzero
 *     vec1.is_length_in(k)   // Return whether vector's length is <= k
 *     vec1.normalize()       // Normalize this vector
 *     vec1.set_length(k)     // Set vector length to k
 *     vec1.cap_length(k)     // Set vector length to k if greater than k
 *     vec1.dot(vec2)         // Return dot product
 *     vec1.cross(vec2)       // Return cross product (for 3-component vectors)
 *     vec1.cross(vec2,vec3)  // Return cross product (for 4-component vectors)
 * The C functions can also be called from C++.
 *
 * The actual type and function definitions are located in a separate file,
 * to simplify the use of compiler- or system-specific optimizations for
 * vector processing.
 */

#ifdef SIL_SYSTEM_VECTOR_HEADER
# include SIL_SYSTEM_VECTOR_HEADER
#else
# include "math/vector.h"
#endif

/*-----------------------------------------------------------------------*/

/**
 * Matrix4f:  4x4-element single-precision floating point matrix type.
 *
 * The following functions are available for operating on matrices.  All
 * functions with a matrix result return it in a user-supplied return
 * variable; functions with scalar results return them via the function's
 * return value.
 *     mat4_make_identity(dest)  // Set dest to the identity matrix
 *     mat4_add(dest,src1,src2)  // Add matrices
 *     mat4_sub(dest,src1,src2)  // Subtract matrices
 *     mat4_mul(dest,src1,src2)  // Multiply matrices
 *     mat4_transpose(dest,src)  // Transpose matrix
 *     mat4_det(src)             // Return determinant of matrix
 *     mat4_inv(dest, src)       // Calculate inverse of matrix
 *     mat4_vec[234]_transform(dest, mat, src)  // Transform vector by matrix
 *
 * For C++, the above matrix operations can also be performed using
 * operators or object methods, as follows.
 *     identity()                // Return an identity matrix
 *     mat1 = mat2               // Copy a matrix
 *     +mat1                     // Unary + (return matrix as is)
 *     -mat1                     // Negation of all components
 *     mat1 + mat2               // Add matrices
 *     mat1 - mat2               // Subtract matrices
 *     mat1 * mat2               // Multiply matrices
 *     mat1 += -= *= mat2
 *     mat1.transpose()          // Return transpose of matrix
 *     mat1.det()                // Return determinant of matrix
 *     mat1.inv()                // Return inverse of matrix
 *     mat1.inv(&det)            // Return inverse and determinant of matrix
 *     mat1.transform(vec)       // Transform vector by matrix
 * The C functions can also be called from C++.
 *
 * All SIL functions treat matrices as stored in row-major order; thus,
 * for example, the first row of the matrix is composed of elements _11,
 * _12, _13, and _14.
 *
 * As for vectors, the actual type and function definitions are located in
 * a separate file, to simplify the use of compiler- or system-specific
 * optimizations for matrix processing.
 */

#ifdef SIL_SYSTEM_MATRIX_HEADER
# include SIL_SYSTEM_MATRIX_HEADER
#else
# include "math/matrix.h"
#endif

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_MATH_H
