/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/math/matrix-cxx.cc: Tests for C++-specific matrix functions.
 */

#include "src/base.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/test/base.h"
#include "src/test/math/internal.h"

#if defined(__GNUC__) && __GNUC__ >= 8
/* Work around spurious warning about mem_clear() on Matrix4f. */
# pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif

/*************************************************************************/
/*************************************************************************/

DEFINE_MATH_TEST_RUNNER(test_math_matrix_cxx)

/*-----------------------------------------------------------------------*/

/* Continue through failures.  (This must come after the test runner is
 * defined.) */
#undef FAIL_ACTION
#define FAIL_ACTION  failed = 1

/*-----------------------------------------------------------------------*/

TEST(test_constructor)
{
    int failed = 0;

    Matrix4f m(1,1.5,2,2.5, 3,4,5,6, 8,10,12,14, 17,20,23,26);
    CHECK_FLOATEQUAL(m._11,  1.0);
    CHECK_FLOATEQUAL(m._12,  1.5);
    CHECK_FLOATEQUAL(m._13,  2.0);
    CHECK_FLOATEQUAL(m._14,  2.5);
    CHECK_FLOATEQUAL(m._21,  3.0);
    CHECK_FLOATEQUAL(m._22,  4.0);
    CHECK_FLOATEQUAL(m._23,  5.0);
    CHECK_FLOATEQUAL(m._24,  6.0);
    CHECK_FLOATEQUAL(m._31,  8.0);
    CHECK_FLOATEQUAL(m._32, 10.0);
    CHECK_FLOATEQUAL(m._33, 12.0);
    CHECK_FLOATEQUAL(m._34, 14.0);
    CHECK_FLOATEQUAL(m._41, 17.0);
    CHECK_FLOATEQUAL(m._42, 20.0);
    CHECK_FLOATEQUAL(m._43, 23.0);
    CHECK_FLOATEQUAL(m._44, 26.0);

    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_identity)
{
    int failed = 0;

    Matrix4f m = Matrix4f::identity();
    CHECK_FLOATEQUAL(m._11, 1.0);
    CHECK_FLOATEQUAL(m._12, 0.0);
    CHECK_FLOATEQUAL(m._13, 0.0);
    CHECK_FLOATEQUAL(m._14, 0.0);
    CHECK_FLOATEQUAL(m._21, 0.0);
    CHECK_FLOATEQUAL(m._22, 1.0);
    CHECK_FLOATEQUAL(m._23, 0.0);
    CHECK_FLOATEQUAL(m._24, 0.0);
    CHECK_FLOATEQUAL(m._31, 0.0);
    CHECK_FLOATEQUAL(m._32, 0.0);
    CHECK_FLOATEQUAL(m._33, 1.0);
    CHECK_FLOATEQUAL(m._34, 0.0);
    CHECK_FLOATEQUAL(m._41, 0.0);
    CHECK_FLOATEQUAL(m._42, 0.0);
    CHECK_FLOATEQUAL(m._43, 0.0);
    CHECK_FLOATEQUAL(m._44, 1.0);

    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_comparison)
{
    int failed = 0;

    union {uint32_t i; float f;} u;
    u.i = 0x7FFFFFFF;
    const float nan = u.f;

    CHECK_TRUE (Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             == Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16));
    CHECK_FALSE(Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             == Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15, 0));
    CHECK_FALSE(Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             == Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14, 0,16));
    CHECK_FALSE(Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             == Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13, 0,15,16));
    CHECK_FALSE(Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             == Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12, 0,14,15,16));
    CHECK_FALSE(Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             == Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11, 0,13,14,15,16));
    CHECK_FALSE(Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             == Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10, 0,12,13,14,15,16));
    CHECK_FALSE(Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             == Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9, 0,11,12,13,14,15,16));
    CHECK_FALSE(Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             == Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 0,10,11,12,13,14,15,16));
    CHECK_FALSE(Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             == Matrix4f( 1, 2, 3, 4, 5, 6, 7, 0, 9,10,11,12,13,14,15,16));
    CHECK_FALSE(Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             == Matrix4f( 1, 2, 3, 4, 5, 6, 0, 8, 9,10,11,12,13,14,15,16));
    CHECK_FALSE(Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             == Matrix4f( 1, 2, 3, 4, 5, 0, 7, 8, 9,10,11,12,13,14,15,16));
    CHECK_FALSE(Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             == Matrix4f( 1, 2, 3, 4, 0, 6, 7, 8, 9,10,11,12,13,14,15,16));
    CHECK_FALSE(Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             == Matrix4f( 1, 2, 3, 0, 5, 6, 7, 8, 9,10,11,12,13,14,15,16));
    CHECK_FALSE(Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             == Matrix4f( 1, 2, 0, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16));
    CHECK_FALSE(Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             == Matrix4f( 1, 0, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16));
    CHECK_FALSE(Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             == Matrix4f( 0, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16));
    CHECK_FALSE(Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             == Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,nan));
    CHECK_FALSE(Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             == Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,nan,16));
    CHECK_FALSE(Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             == Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,nan,15,16));
    CHECK_FALSE(Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             == Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,nan,14,15,16));
    CHECK_FALSE(Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             == Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,nan,13,14,15,16));
    CHECK_FALSE(Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             == Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,nan,12,13,14,15,16));
    CHECK_FALSE(Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             == Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,nan,11,12,13,14,15,16));
    CHECK_FALSE(Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             == Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8,nan,10,11,12,13,14,15,16));
    CHECK_FALSE(Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             == Matrix4f( 1, 2, 3, 4, 5, 6, 7,nan, 9,10,11,12,13,14,15,16));
    CHECK_FALSE(Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             == Matrix4f( 1, 2, 3, 4, 5, 6,nan, 8, 9,10,11,12,13,14,15,16));
    CHECK_FALSE(Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             == Matrix4f( 1, 2, 3, 4, 5,nan, 7, 8, 9,10,11,12,13,14,15,16));
    CHECK_FALSE(Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             == Matrix4f( 1, 2, 3, 4,nan, 6, 7, 8, 9,10,11,12,13,14,15,16));
    CHECK_FALSE(Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             == Matrix4f( 1, 2, 3,nan, 5, 6, 7, 8, 9,10,11,12,13,14,15,16));
    CHECK_FALSE(Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             == Matrix4f( 1, 2,nan, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16));
    CHECK_FALSE(Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             == Matrix4f( 1,nan, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16));
    CHECK_FALSE(Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             == Matrix4f(nan, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16));

    CHECK_FALSE(Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             != Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16));
    CHECK_TRUE (Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             != Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15, 0));
    CHECK_TRUE (Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             != Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14, 0,16));
    CHECK_TRUE (Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             != Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13, 0,15,16));
    CHECK_TRUE (Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             != Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12, 0,14,15,16));
    CHECK_TRUE (Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             != Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11, 0,13,14,15,16));
    CHECK_TRUE (Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             != Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10, 0,12,13,14,15,16));
    CHECK_TRUE (Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             != Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9, 0,11,12,13,14,15,16));
    CHECK_TRUE (Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             != Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 0,10,11,12,13,14,15,16));
    CHECK_TRUE (Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             != Matrix4f( 1, 2, 3, 4, 5, 6, 7, 0, 9,10,11,12,13,14,15,16));
    CHECK_TRUE (Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             != Matrix4f( 1, 2, 3, 4, 5, 6, 0, 8, 9,10,11,12,13,14,15,16));
    CHECK_TRUE (Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             != Matrix4f( 1, 2, 3, 4, 5, 0, 7, 8, 9,10,11,12,13,14,15,16));
    CHECK_TRUE (Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             != Matrix4f( 1, 2, 3, 4, 0, 6, 7, 8, 9,10,11,12,13,14,15,16));
    CHECK_TRUE (Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             != Matrix4f( 1, 2, 3, 0, 5, 6, 7, 8, 9,10,11,12,13,14,15,16));
    CHECK_TRUE (Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             != Matrix4f( 1, 2, 0, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16));
    CHECK_TRUE (Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             != Matrix4f( 1, 0, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16));
    CHECK_TRUE (Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             != Matrix4f( 0, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16));
    CHECK_TRUE (Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             != Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,nan));
    CHECK_TRUE (Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             != Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,nan,16));
    CHECK_TRUE (Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             != Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,nan,15,16));
    CHECK_TRUE (Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             != Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,nan,14,15,16));
    CHECK_TRUE (Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             != Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,nan,13,14,15,16));
    CHECK_TRUE (Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             != Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,nan,12,13,14,15,16));
    CHECK_TRUE (Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             != Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,nan,11,12,13,14,15,16));
    CHECK_TRUE (Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             != Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8,nan,10,11,12,13,14,15,16));
    CHECK_TRUE (Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             != Matrix4f( 1, 2, 3, 4, 5, 6, 7,nan, 9,10,11,12,13,14,15,16));
    CHECK_TRUE (Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             != Matrix4f( 1, 2, 3, 4, 5, 6,nan, 8, 9,10,11,12,13,14,15,16));
    CHECK_TRUE (Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             != Matrix4f( 1, 2, 3, 4, 5,nan, 7, 8, 9,10,11,12,13,14,15,16));
    CHECK_TRUE (Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             != Matrix4f( 1, 2, 3, 4,nan, 6, 7, 8, 9,10,11,12,13,14,15,16));
    CHECK_TRUE (Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             != Matrix4f( 1, 2, 3,nan, 5, 6, 7, 8, 9,10,11,12,13,14,15,16));
    CHECK_TRUE (Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             != Matrix4f( 1, 2,nan, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16));
    CHECK_TRUE (Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             != Matrix4f( 1,nan, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16));
    CHECK_TRUE (Matrix4f( 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16)
             != Matrix4f(nan, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16));

    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_basic)
{
    int failed = 0;

    for (int i = 0; i < lenof_matrix_tests(); i++) {
        if (matrix_tests[i].size == 4) {
            Matrix4f res;
            const Vector3f v = {matrix_tests[i].b._11, matrix_tests[i].b._12,
                                matrix_tests[i].b._13};
            const float angle = matrix_tests[i].b._14;
            int (*check_func)(const Matrix4f *result, const Matrix4f *expect,
                              const char *errmsg, ...);
            if (matrix_tests[i].type == ROT && fmodf(angle, 90) != 0) {
                check_func = check_matrix_error_inexact;
            } else {
                check_func = check_matrix_error;
            }

            /* Handle noninvertible matrices specially. */
            if (matrix_tests[i].type == INV && matrix_tests[i].b._11 != 0) {
                float det;
                if (matrix_tests[i].a.inv(&det), det != 0) {
                    FAIL("test %u: inverted a non-invertible matrix", i);
                }
                continue;  // Skip regular processing.
            }

            /* Check operation of the function itself. */
            mem_clear(&res, sizeof(res));
            switch (matrix_tests[i].type) {
                case PLUS:  res = +matrix_tests[i].a; break;
                case NEG:   res = -matrix_tests[i].a; break;
                case ADD:   res = matrix_tests[i].a + matrix_tests[i].b; break;
                case SUB:   res = matrix_tests[i].a - matrix_tests[i].b; break;
                case MUL:   res = matrix_tests[i].a * matrix_tests[i].b; break;
                case TRANS: res = matrix_tests[i].a.transpose();     break;
                case INV:   res = matrix_tests[i].a.inv();           break;
                case XLATE: res = matrix_tests[i].a.translate(v);    break;
                case ROT:   res = matrix_tests[i].a.rotate(v, angle); break;
                case SCALE: res = matrix_tests[i].a.scale(v);        break;
                default:    FAIL("test %u: invalid type", i); break;
            }
            if (!(*check_func)(&res, &matrix_tests[i].res,
                               "test %u: result != expect", i)) {
                failed = 1;
                continue;
            }

            /* Check operation of assignment operators. */
            res = matrix_tests[i].a;
            switch (matrix_tests[i].type) {
                case PLUS:  goto skip_assign;  // No equivalent operator.
                case NEG:   goto skip_assign;  // No equivalent operator.
                case ADD:   res += matrix_tests[i].b; break;
                case SUB:   res -= matrix_tests[i].b; break;
                case MUL:   res *= matrix_tests[i].b; break;
                case TRANS: goto skip_assign;  // No equivalent operator.
                case INV:   goto skip_assign;  // No equivalent operator.
                case XLATE: goto skip_assign;  // No equivalent operator.
                case ROT:   goto skip_assign;  // No equivalent operator.
                case SCALE: goto skip_assign;  // No equivalent operator.
                default:    FAIL("test %u: invalid type", i); break;
            }
            if (!(*check_func)(&res, &matrix_tests[i].res,
                               "test %u: fail on assignment op", i)) {
                failed = 1;
            }
          skip_assign:;

            /* Check mat4_rotate() specializations. */
            if (matrix_tests[i].type == ROT) {
                if (matrix_tests[i].b._11 == 1
                 && matrix_tests[i].b._12 == 0
                 && matrix_tests[i].b._13 == 0) {
                    res = matrix_tests[i].a.rotate_x(angle);
                    if (!(*check_func)(&res, &matrix_tests[i].res,
                                       "test %u: fail on rotate_x", i)) {
                        failed = 1;
                    }
                } else if (matrix_tests[i].b._11 == 0
                        && matrix_tests[i].b._12 == 1
                        && matrix_tests[i].b._13 == 0) {
                    res = matrix_tests[i].a.rotate_y(angle);
                    if (!(*check_func)(&res, &matrix_tests[i].res,
                                       "test %u: fail on rotate_y", i)) {
                        failed = 1;
                    }
                } else if (matrix_tests[i].b._11 == 0
                        && matrix_tests[i].b._12 == 0
                        && matrix_tests[i].b._13 == 1) {
                    res = matrix_tests[i].a.rotate_z(angle);
                    if (!(*check_func)(&res, &matrix_tests[i].res,
                                       "test %u: fail on rotate_z", i)) {
                        failed = 1;
                    }
                }
            }  // if (matrix_tests[i].type == ROT)

        } else {
            FAIL("test %u: bad matrix size %d", i, matrix_tests[i].size);
        }
    }

    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_transform)
{
    int failed = 0;

    for (int i = 0; i < lenof_transform_tests(); i++) {
        Matrix4f m(transform_tests[i].m1[0],
                   transform_tests[i].m1[1],
                   transform_tests[i].m1[2],
                   transform_tests[i].m1[3],
                   transform_tests[i].m2[0],
                   transform_tests[i].m2[1],
                   transform_tests[i].m2[2],
                   transform_tests[i].m2[3],
                   transform_tests[i].m3[0],
                   transform_tests[i].m3[1],
                   transform_tests[i].m3[2],
                   transform_tests[i].m3[3],
                   transform_tests[i].m4[0],
                   transform_tests[i].m4[1],
                   transform_tests[i].m4[2],
                   transform_tests[i].m4[3]);

        if (transform_tests[i].size == 2) {
            Vector2f vec(transform_tests[i].coord[0],
                         transform_tests[i].coord[1]);
            Vector2f res = m.transform(vec);
            if (res.x != transform_tests[i].res[0]
             || res.y != transform_tests[i].res[1]) {
                FAIL("transform test %u: result <%f,%f> != expect <%f,%f>",
                     i, res.x, res.y,
                     transform_tests[i].res[0], transform_tests[i].res[1]);
            }

        } else if (transform_tests[i].size == 3) {
            Vector3f vec(transform_tests[i].coord[0],
                         transform_tests[i].coord[1],
                         transform_tests[i].coord[2]);
            Vector3f res = m.transform(vec);
            if (res.x != transform_tests[i].res[0]
             || res.y != transform_tests[i].res[1]
             || res.z != transform_tests[i].res[2]) {
                FAIL("transform test %u: result <%f,%f,%f> != expect"
                     " <%f,%f,%f>", i, res.x, res.y, res.z,
                     transform_tests[i].res[0], transform_tests[i].res[1],
                     transform_tests[i].res[2]);
            }

        } else if (transform_tests[i].size == 4) {
            Vector4f vec(transform_tests[i].coord[0],
                         transform_tests[i].coord[1],
                         transform_tests[i].coord[2],
                         transform_tests[i].coord[3]);
            Vector4f res = m.transform(vec);
            if (res.x != transform_tests[i].res[0]
             || res.y != transform_tests[i].res[1]
             || res.z != transform_tests[i].res[2]
             || res.w != transform_tests[i].res[3]) {
                FAIL("transform test %u: result <%f,%f,%f,%f> != expect"
                     " <%f,%f,%f,%f>", i, res.x, res.y, res.z, res.w,
                     transform_tests[i].res[0], transform_tests[i].res[1],
                     transform_tests[i].res[2], transform_tests[i].res[3]);
            }

        } else {
            FAIL("test %u: bad vector size %d", i, transform_tests[i].size);
        }
    }

    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_translation)
{
    int failed = 0;

    const Matrix4f m(9,9,9,9, 9,9,9,9, 9,9,9,9, 1,2,3,9);
    const Vector3f v = m.get_translation();
    CHECK_FLOATEQUAL(v.x, 1);
    CHECK_FLOATEQUAL(v.y, 2);
    CHECK_FLOATEQUAL(v.z, 3);

    return !failed;
}

/*************************************************************************/
/*************************************************************************/
