/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/math/matrix.c: Tests for matrix functions.
 */

#include "src/base.h"
#include "src/math.h"
#include "src/test/base.h"
#include "src/test/math/internal.h"

/*************************************************************************/
/*************************************************************************/

DEFINE_MATH_TEST_RUNNER(test_math_matrix)

/*-----------------------------------------------------------------------*/

/* Continue through failures.  (This must come after the test runner is
 * defined.) */
#undef FAIL_ACTION
#define FAIL_ACTION  failed = 1

/*-----------------------------------------------------------------------*/

TEST(test_identity)
{
    int failed = 0;

    CHECK_FLOATEQUAL(mat4_identity._11, 1);
    CHECK_FLOATEQUAL(mat4_identity._12, 0);
    CHECK_FLOATEQUAL(mat4_identity._13, 0);
    CHECK_FLOATEQUAL(mat4_identity._14, 0);
    CHECK_FLOATEQUAL(mat4_identity._21, 0);
    CHECK_FLOATEQUAL(mat4_identity._22, 1);
    CHECK_FLOATEQUAL(mat4_identity._23, 0);
    CHECK_FLOATEQUAL(mat4_identity._24, 0);
    CHECK_FLOATEQUAL(mat4_identity._31, 0);
    CHECK_FLOATEQUAL(mat4_identity._32, 0);
    CHECK_FLOATEQUAL(mat4_identity._33, 1);
    CHECK_FLOATEQUAL(mat4_identity._34, 0);
    CHECK_FLOATEQUAL(mat4_identity._41, 0);
    CHECK_FLOATEQUAL(mat4_identity._42, 0);
    CHECK_FLOATEQUAL(mat4_identity._43, 0);
    CHECK_FLOATEQUAL(mat4_identity._44, 1);

    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_basic)
{
    int failed = 0;

    for (int i = 0; i < lenof_matrix_tests(); i++) {
        if (matrix_tests[i].type == PLUS || matrix_tests[i].type == NEG) {
            continue;  // Skip C++-specific tests.
        }

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
                if (mat4_inv(&res, &matrix_tests[i].a)) {
                    FAIL("test %u: inverted a non-invertible matrix", i);
                }
                continue;  // Skip regular processing.
            }

            /* Check operation of the function itself. */
            mem_clear(&res, sizeof(res));
            switch (matrix_tests[i].type) {
                case ADD:   mat4_add(&res,
                                     &matrix_tests[i].a, &matrix_tests[i].b);
                            break;
                case SUB:   mat4_sub(&res,
                                     &matrix_tests[i].a, &matrix_tests[i].b);
                            break;
                case MUL:   mat4_mul(&res,
                                     &matrix_tests[i].a, &matrix_tests[i].b);
                            break;
                case TRANS: mat4_transpose(&res, &matrix_tests[i].a); break;
                case INV:   mat4_inv(&res, &matrix_tests[i].a); break;
                case XLATE: mat4_translate(&res, &matrix_tests[i].a, &v); break;
                case ROT:   mat4_rotate(&res, &matrix_tests[i].a, &v, angle);
                            break;
                case SCALE: mat4_scale(&res, &matrix_tests[i].a, &v); break;
                default:    FAIL("test %u: invalid type", i); break;
            }
            if (!(*check_func)(&res, &matrix_tests[i].res,
                               "test %u: result != expect", i)) {
                failed = 1;
                /* Skip the dest==src tests since they won't give us any
                 * useful information. */
                goto skip_src2;
            }

            /* Check operation with dest == src1. */
            res = matrix_tests[i].a;
            switch (matrix_tests[i].type) {
                case ADD:   mat4_add(&res, &res, &matrix_tests[i].b); break;
                case SUB:   mat4_sub(&res, &res, &matrix_tests[i].b); break;
                case MUL:   mat4_mul(&res, &res, &matrix_tests[i].b); break;
                case TRANS: mat4_transpose(&res, &res); break;
                case INV:   mat4_inv(&res, &res); break;
                case XLATE: mat4_translate(&res, &res, &v); break;
                case ROT:   mat4_rotate(&res, &res, &v, angle); break;
                case SCALE: mat4_scale(&res, &res, &v); break;
                default:    FAIL("test %u: invalid type", i); break;
            }
            if (!(*check_func)(&res, &matrix_tests[i].res,
                               "test %u: fail on dest == src1", i)) {
                failed = 1;
            }

            /* Check operation with dest == src2 (if appropriate). */
            res = matrix_tests[i].b;
            switch (matrix_tests[i].type) {
                case ADD:   mat4_add(&res, &matrix_tests[i].a, &res); break;
                case SUB:   mat4_sub(&res, &matrix_tests[i].a, &res); break;
                case MUL:   mat4_mul(&res, &matrix_tests[i].a, &res); break;
                case TRANS: goto skip_src2;  // Only one operand.
                case INV:   goto skip_src2;  // Only one operand.
                case XLATE: goto skip_src2;  // Only one operand.
                case ROT:   goto skip_src2;  // Only one operand.
                case SCALE: goto skip_src2;  // Only one operand.
                default:    FAIL("test %u: invalid type", i); break;
            }
            if (!(*check_func)(&res, &matrix_tests[i].res,
                               "test %u: fail on dest == src2", i)) {
                failed = 1;
            }
          skip_src2:;

            /* Check mat4_rotate() specializations. */
            if (matrix_tests[i].type == ROT) {
                if (matrix_tests[i].b._11 == 1
                 && matrix_tests[i].b._12 == 0
                 && matrix_tests[i].b._13 == 0) {
                    mat4_rotate_x(&res, &matrix_tests[i].a, angle);
                    if (!(*check_func)(&res, &matrix_tests[i].res,
                                       "test %u: fail on rotate_x", i)) {
                        failed = 1;
                    } else {
                        res = matrix_tests[i].a;
                        mat4_rotate_x(&res, &res, angle);
                        if (!(*check_func)(&res, &matrix_tests[i].res,
                                           "test %u: fail on rotate_x,"
                                           " dest == src", i)) {
                            failed = 1;
                        }
                    }
                } else if (matrix_tests[i].b._11 == 0
                        && matrix_tests[i].b._12 == 1
                        && matrix_tests[i].b._13 == 0) {
                    mat4_rotate_y(&res, &matrix_tests[i].a, angle);
                    if (!(*check_func)(&res, &matrix_tests[i].res,
                                       "test %u: fail on rotate_y", i)) {
                        failed = 1;
                    } else {
                        res = matrix_tests[i].a;
                        mat4_rotate_y(&res, &res, angle);
                        if (!(*check_func)(&res, &matrix_tests[i].res,
                                           "test %u: fail on rotate_y,"
                                           " dest == src", i)) {
                            failed = 1;
                        }
                    }
                } else if (matrix_tests[i].b._11 == 0
                        && matrix_tests[i].b._12 == 0
                        && matrix_tests[i].b._13 == 1) {
                    mat4_rotate_z(&res, &matrix_tests[i].a, angle);
                    if (!(*check_func)(&res, &matrix_tests[i].res,
                                       "test %u: fail on rotate_z", i)) {
                        failed = 1;
                    } else {
                        res = matrix_tests[i].a;
                        mat4_rotate_z(&res, &res, angle);
                        if (!(*check_func)(&res, &matrix_tests[i].res,
                                           "test %u: fail on rotate_z,"
                                           " dest == src", i)) {
                            failed = 1;
                        }
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
        Matrix4f m;
        m._11 = transform_tests[i].m1[0];
        m._12 = transform_tests[i].m1[1];
        m._13 = transform_tests[i].m1[2];
        m._14 = transform_tests[i].m1[3];
        m._21 = transform_tests[i].m2[0];
        m._22 = transform_tests[i].m2[1];
        m._23 = transform_tests[i].m2[2];
        m._24 = transform_tests[i].m2[3];
        m._31 = transform_tests[i].m3[0];
        m._32 = transform_tests[i].m3[1];
        m._33 = transform_tests[i].m3[2];
        m._34 = transform_tests[i].m3[3];
        m._41 = transform_tests[i].m4[0];
        m._42 = transform_tests[i].m4[1];
        m._43 = transform_tests[i].m4[2];
        m._44 = transform_tests[i].m4[3];

        if (transform_tests[i].size == 2) {
            Vector2f vec, res;
            vec.x = transform_tests[i].coord[0];
            vec.y = transform_tests[i].coord[1];
            mat4_vec2_transform(&res, &vec, &m);
            if (res.x != transform_tests[i].res[0]
             || res.y != transform_tests[i].res[1]) {
                FAIL("transform test %u: result <%f,%f> != expect <%f,%f>",
                     i, res.x, res.y,
                     transform_tests[i].res[0], transform_tests[i].res[1]);
            }
            res = vec;
            mat4_vec2_transform(&res, &res, &m);
            if (res.x != transform_tests[i].res[0]
             || res.y != transform_tests[i].res[1]) {
                FAIL("transform test %u: result <%f,%f> != expect <%f,%f>"
                     " for dest == src", i, res.x, res.y,
                     transform_tests[i].res[0], transform_tests[i].res[1]);
            }

        } else if (transform_tests[i].size == 3) {
            Vector3f vec, res;
            vec.x = transform_tests[i].coord[0];
            vec.y = transform_tests[i].coord[1];
            vec.z = transform_tests[i].coord[2];
            mat4_vec3_transform(&res, &vec, &m);
            if (res.x != transform_tests[i].res[0]
             || res.y != transform_tests[i].res[1]
             || res.z != transform_tests[i].res[2]) {
                FAIL("transform test %u: result <%f,%f,%f> != expect"
                     " <%f,%f,%f>", i, res.x, res.y, res.z,
                     transform_tests[i].res[0], transform_tests[i].res[1],
                     transform_tests[i].res[2]);
            }
            res = vec;
            mat4_vec3_transform(&res, &res, &m);
            if (res.x != transform_tests[i].res[0]
             || res.y != transform_tests[i].res[1]
             || res.z != transform_tests[i].res[2]) {
                FAIL("transform test %u: result <%f,%f,%f> != expect"
                     " <%f,%f,%f> for dest == src", i, res.x, res.y, res.z,
                     transform_tests[i].res[0], transform_tests[i].res[1],
                     transform_tests[i].res[2]);
            }

        } else if (transform_tests[i].size == 4) {
            Vector4f vec, res;
            vec.x = transform_tests[i].coord[0];
            vec.y = transform_tests[i].coord[1];
            vec.z = transform_tests[i].coord[2];
            vec.w = transform_tests[i].coord[3];
            mat4_vec4_transform(&res, &vec, &m);
            if (res.x != transform_tests[i].res[0]
             || res.y != transform_tests[i].res[1]
             || res.z != transform_tests[i].res[2]
             || res.w != transform_tests[i].res[3]) {
                FAIL("transform test %u: result <%f,%f,%f,%f> != expect"
                     " <%f,%f,%f,%f>", i, res.x, res.y, res.z, res.w,
                     transform_tests[i].res[0], transform_tests[i].res[1],
                     transform_tests[i].res[2], transform_tests[i].res[3]);
            }
            res = vec;
            mat4_vec4_transform(&res, &res, &m);
            if (res.x != transform_tests[i].res[0]
             || res.y != transform_tests[i].res[1]
             || res.z != transform_tests[i].res[2]
             || res.w != transform_tests[i].res[3]) {
                FAIL("transform test %u: result <%f,%f,%f,%f> != expect"
                     " <%f,%f,%f,%f> for dest == src",
                     i, res.x, res.y, res.z, res.w,
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

    static const Matrix4f m = {9,9,9,9, 9,9,9,9, 9,9,9,9, 1,2,3,9};
    const Vector3f v = mat4_get_translation(&m);
    CHECK_FLOATEQUAL(v.x, 1);
    CHECK_FLOATEQUAL(v.y, 2);
    CHECK_FLOATEQUAL(v.z, 3);

    return !failed;
}

/*************************************************************************/
/*************************************************************************/
