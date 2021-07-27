/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/math/internal.h: Internal utility header for math function tests.
 */

#ifndef SIL_SRC_TEST_MATH_INTERNAL_H
#define SIL_SRC_TEST_MATH_INTERNAL_H

EXTERN_C_BEGIN

/*************************************************************************/
/************************** Convenience macros ***************************/
/*************************************************************************/

/**
 * TINY:  A value slightly less than half the value of the lowest-order
 * mantissa bit in 1.0f.  In other words, if the rounding mode is set to
 * "round to nearest", the following will all hold:
 *    1.0f + TINY   == 1.0f
 *    1.0f + TINY*2 >  1.0f
 *    1.0f - TINY/2 == 1.0f
 *    1.0f - TINY   <  1.0f
 * This is used in checking for single-precision semantics.
 */
#define TINY  (0.999f / (1<<24))

/**
 * CLOSE_ENOUGH:  Checks whether two floating-point values are "close
 * enough" to be considered equal for the purposes of these tests.
 */
#define CLOSE_ENOUGH(a,b) \
    ((b) == 0 ? (a) == 0 : fabsf((a) - (b)) / (b) < 1/(float)(1<<20))

/*-----------------------------------------------------------------------*/

/**
 * DEFINE_MATH_TEST_RUNNER:  Variant of DEFINE_GENERIC_TEST_RUNNER which
 * skips the contained tests entirely (returning success) on x86 platforms
 * if the denormals-are-zero FPU flag is not set.  This typically indicates
 * that the program is running under the Valgrind memory checker, which (at
 * least through version 3.7.0) does not emulate denormals-are-zero; since
 * Valgrind is still useful in checking for other problems, we skip over
 * tests which would fail without DAZ rather than returning an error and
 * potentially suppressing the execution of other tests.
 */
#if defined(SIL_ARCH_X86) && defined(__GNUC__)
# define DEFINE_MATH_TEST_RUNNER(name)    \
static int do_##name(void);               \
DEFINE_GENERIC_TEST_RUNNER(do_##name)     \
int name(void) {                          \
    uint32_t mxcsr;                       \
    __asm__("stmxcsr %0" : "=m" (mxcsr)); \
    if (!(mxcsr & (1<<6))) {              \
        SKIP("*** mxcsr.DAZ is not set.  (Are you running under Valgrind?)"); \
    }                                     \
    return do_##name();                   \
}
#else  // not x86 or not GCC/Clang
# define DEFINE_MATH_TEST_RUNNER  DEFINE_GENERIC_TEST_RUNNER
#endif

/*************************************************************************/
/***************** Test data types and list declarations *****************/
/*************************************************************************/

/* Vector/matrix operation types. */
enum TestOp {PLUS, NEG, ADD, SUB, MUL, DIV, S_ADD, S_SUB, SCALE, S_DIV, LEN,
             LEN2, NON_0, LEN_IN, NORM, SETLEN, CAPLEN, DOT, LERP, CROSS,
             TRANS /*matrix transpose*/, INV /*matrix inverse*/,
             XLATE /*matrix translate*/, ROT, ROT_X, ROT_Y, ROT_Z,
             END /*fencepost*/
};

/* Test data for vector operations other than vec_cross4(). */
typedef struct VectorTest VectorTest;
struct VectorTest {
    enum TestOp type;
    int size;    // Size of vector argument "a" and result.
    int size_b;  // Size of vector argument "b" (for C++ tests, if relevant).
    float a[4], b[4], k;    // Vector and scalar arguments.
    float res[4];           // Expected result.
    int allow_approximate;  // True to allow a slight deviation.
};
extern const VectorTest vector_tests[];
extern CONST_FUNCTION int lenof_vector_tests(void);

/* Test data for vec_cross4() operations, which take three vector arguments
 * instead of two. */
typedef struct VectorCross4Test VectorCross4Test;
struct VectorCross4Test {
    float a[4], b[4], c[4], res[4];
};
extern const VectorCross4Test vector_cross4_tests[];
extern CONST_FUNCTION int lenof_vector_cross4_tests(void);

/* Test data for matrix operations. */
typedef struct MatrixTest MatrixTest;
struct MatrixTest {
    enum TestOp type;
    int size;
    Matrix4f a, b, res;
};
extern const MatrixTest matrix_tests[];
extern CONST_FUNCTION int lenof_matrix_tests(void);

/* Test data for vector transformation operations. */
typedef struct TransformTest TransformTest;
struct TransformTest {
    /* Arrange the fields so we can make the definitions line up nicely. */
    int size;
    float coord[4], m1[4], res[4], m2[4], m3[4], m4[4];
};
extern const TransformTest transform_tests[];
extern CONST_FUNCTION int lenof_transform_tests(void);

/*************************************************************************/
/*********************** Internal helper functions ***********************/
/*************************************************************************/

/**
 * check_matrix_error:  Check whether a matrix operation succeeded, and
 * print an error message if not.  Helper function for test_matrix().
 *
 * [Parameters]
 *     result: Pointer to result of operation.
 *     expect: Pointer to expected result.
 *     errmsg: Message to log on error.  printf() format tokens may be used.
 *     ...: Format arguments for errmsg.
 * [Return value]
 *     True if the operation succeeded, false if it failed.
 */
extern int check_matrix_error(const Matrix4f *result, const Matrix4f *expect,
                              const char *errmsg, ...);

/**
 * check_matrix_error_inexact:  Check whether a matrix operation succeeded,
 * and print an error message if not, allowing a deviation of up to 1e-6 in
 * each matrix element.
 *
 * [Parameters]
 *     result: Pointer to result of operation.
 *     expect: Pointer to expected result.
 *     errmsg: Message to log on error.  printf() format tokens may be used.
 *     ...: Format arguments for errmsg.
 * [Return value]
 *     True if the operation succeeded, false if it failed.
 */
extern int check_matrix_error_inexact(const Matrix4f *result,
                                      const Matrix4f *expect,
                                      const char *errmsg, ...);

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_SRC_TEST_MATH_INTERNAL_H
