/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/math/dtrig.c: Tests for degree-based trigonometric functions.
 */

#include "src/base.h"
#include "src/math.h"
#include "src/test/base.h"
#include "src/test/math/internal.h"

/*************************************************************************/
/*************************************************************************/

DEFINE_MATH_TEST_RUNNER(test_math_dtrig)

/* Continue through failures.  (This must come after the test runner is
 * defined.) */
#undef FAIL_ACTION
#define FAIL_ACTION  failed = 1

/*-----------------------------------------------------------------------*/

TEST(test_exact)
{
    static const struct {
        float deg;
        double expect_sin;  // Defined as double-precision just for reference.
        double expect_cos;
        double expect_tan;
    } testlist[] = {
#define SQRT_2 1.4142135623730951
#define SQRT_3 1.7320508075688772
        {   0,  0.0,       1.0,       0.0},
        {  30,  0.5,       SQRT_3/2,  1/SQRT_3},
        {  45,  SQRT_2/2,  SQRT_2/2,  1.0},
        {  60,  SQRT_3/2,  0.5,       SQRT_3},
        {  90,  1.0,       0.0,       1.0/0.0},
        { 120,  SQRT_3/2, -0.5,      -SQRT_3},
        { 135,  SQRT_2/2, -SQRT_2/2, -1.0},
        { 150,  0.5,      -SQRT_3/2, -1/SQRT_3},
        { 180,  0.0,      -1.0,       0.0},
        { 210, -0.5,      -SQRT_3/2,  1/SQRT_3},
        { 225, -SQRT_2/2, -SQRT_2/2,  1.0},
        { 240, -SQRT_3/2, -0.5,       SQRT_3},
        { 270, -1.0,       0.0,       1.0/0.0},
        { 300, -SQRT_3/2,  0.5,      -SQRT_3},
        { 315, -SQRT_2/2,  SQRT_2/2, -1.0},
        { 330, -0.5,       SQRT_3/2, -1/SQRT_3},
        { 360,  0.0,       1.0,       0.0},
        { 390,  0.5,       SQRT_3/2,  1/SQRT_3},
        { 720,  0.0,       1.0,       0.0},
        { 750,  0.5,       SQRT_3/2,  1/SQRT_3},
        {  -0,  0.0,       1.0,       0.0},
        { -30, -0.5,       SQRT_3/2, -1/SQRT_3},
        {-390, -0.5,       SQRT_3/2, -1/SQRT_3},
        {(float)0x3FFFFFC0L, 0.0, 1.0, 0.0},
        {(float)-0x7FFFFF80L, 0.0, 1.0, 0.0},
#undef SQRT_2
#undef SQRT_3
    };

    int failed = 0;

    for (int i = 0; i < lenof(testlist); i++) {
        const float sinf_res  = dsinf(testlist[i].deg);
        const float cosf_res  = dcosf(testlist[i].deg);
        const float tanf_res  = dtanf(testlist[i].deg);
        const float asinf_res = dasinf(testlist[i].expect_sin);
        const float acosf_res = dacosf(testlist[i].expect_cos);
        const float atanf_res = datan2f(testlist[i].expect_sin,
                                        testlist[i].expect_cos);
        float sincosf_sin, sincosf_cos;
        dsincosf(testlist[i].deg, &sincosf_sin, &sincosf_cos);

        /* Only check the datan2f() result if it's expected to be exact. */
        const int atan_exact = (fmodf(testlist[i].deg, 45) == 0);

        double expect_asin =
            fmod(fmod(testlist[i].deg, 360) + 360 + 180, 360) - 180;
        if (expect_asin > 90) {
            expect_asin = 180 - expect_asin;
        } else if (expect_asin < -90) {
            expect_asin = -180 - expect_asin;
        }
        /* Tweak the outer fmod() parameter to allow +180. */
        double expect_acos =
            fmod(fmod(testlist[i].deg, 360) + 360 + 179, 360) - 179;
        if (expect_acos < 0) {
            expect_acos = -expect_acos;
        }
        const double expect_atan =
            fmod(fmod(testlist[i].deg, 360) + 360 + 179, 360) - 179;

        if (sinf_res != (float)testlist[i].expect_sin) {
            FAIL("dsinf(%g) = %g (d=%g)", testlist[i].deg, sinf_res,
                sinf_res - testlist[i].expect_sin);
        }
        if (cosf_res != (float)testlist[i].expect_cos) {
            FAIL("dcosf(%g) = %g (d=%g)", testlist[i].deg, cosf_res,
                cosf_res - testlist[i].expect_cos);
        }
        if (isinf(testlist[i].expect_tan)
            ? !isinf(tanf_res)
            : (tanf_res != (float)testlist[i].expect_tan))
        {
            FAIL("dtanf(%g) = %g (d=%g)", testlist[i].deg, tanf_res,
                 tanf_res - testlist[i].expect_tan);
        }
        if (asinf_res != (float)expect_asin) {
            FAIL("dasinf(%g) = %g (d=%g)", testlist[i].expect_sin,
                 asinf_res, asinf_res - expect_asin);
        }
        if (acosf_res != (float)expect_acos) {
            FAIL("dacosf(%g) = %g (d=%g)", testlist[i].expect_cos,
                 acosf_res, acosf_res - expect_acos);
        }
        if (atan_exact && atanf_res != (float)expect_atan) {
            FAIL("datan2f(%g,%g) = %g (d=%g)", testlist[i].expect_sin,
                 testlist[i].expect_cos, atanf_res, atanf_res - expect_atan);
        }
        if (sincosf_sin != (float)testlist[i].expect_sin) {
            FAIL("dsincosf(%g).sin = %g (d=%g)", testlist[i].deg,
                 sincosf_sin, sincosf_sin - testlist[i].expect_sin);
        }
        if (sincosf_cos != (float)testlist[i].expect_cos) {
            FAIL("dsincosf(%g).cos = %g (d=%g)", testlist[i].deg,
                 sincosf_cos, sincosf_cos - testlist[i].expect_cos);
        }
    }

    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_inexact)
{
    static const struct {
        float deg;
        double expect_sin;
        double expect_cos;
        double expect_tan;
    } testlist[] = {
        { 12.5, +0.21643961393810288, +0.97629600711993336, +0.22169466264293991},
        {167.5, +0.21643961393810288, -0.97629600711993336, -0.22169466264293991},
        {192.5, -0.21643961393810288, -0.97629600711993336, +0.22169466264293991},
        {347.5, -0.21643961393810288, +0.97629600711993336, -0.22169466264293991},
        {372.5, +0.21643961393810288, +0.97629600711993336, +0.22169466264293991},
        {-12.5, -0.21643961393810288, +0.97629600711993336, -0.22169466264293991},
    };

    int failed = 0;

    for (int i = 0; i < lenof(testlist); i++) {
        const float sinf_res  = dsinf(testlist[i].deg);
        const float cosf_res  = dcosf(testlist[i].deg);
        const float tanf_res  = dtanf(testlist[i].deg);
        const float asinf_res = dasinf(testlist[i].expect_sin);
        const float acosf_res = dacosf(testlist[i].expect_cos);
        const float atanf_res = datan2f(testlist[i].expect_sin,
                                        testlist[i].expect_cos);
        float sincosf_sin, sincosf_cos;
        dsincosf(testlist[i].deg, &sincosf_sin, &sincosf_cos);

        double expect_asin =
            fmod(fmod(testlist[i].deg, 360) + 360 + 180, 360) - 180;
        if (expect_asin > 90) {
            expect_asin = 180 - expect_asin;
        } else if (expect_asin < -90) {
            expect_asin = -180 - expect_asin;
        }
        double expect_acos =
            fmod(fmod(testlist[i].deg, 360) + 360 + 180, 360) - 180;
        if (expect_acos < 0) {
            expect_acos = -expect_acos;
        }
        const double expect_atan =
            fmod(fmod(testlist[i].deg, 360) + 360 + 180, 360) - 180;

        if (fabsf(sinf_res / (float)testlist[i].expect_sin - 1) > 1.0e-6f) {
            FAIL("dsinf(%g) = %g (d=%g)", testlist[i].deg, sinf_res,
                sinf_res - testlist[i].expect_sin);
        }
        if (fabsf(cosf_res / (float)testlist[i].expect_cos - 1) > 1.0e-6f) {
            FAIL("dcosf(%g) = %g (d=%g)", testlist[i].deg, cosf_res,
                cosf_res - testlist[i].expect_cos);
        }
        if (fabsf(tanf_res / (float)testlist[i].expect_tan - 1) > 1.0e-6f) {
            FAIL("dtanf(%g) = %g (d=%g)", testlist[i].deg, tanf_res,
                 tanf_res - testlist[i].expect_tan);
        }
        if (fabsf(asinf_res / (float)expect_asin - 1) > 1.0e-6f) {
            FAIL("dasinf(%g) = %g (d=%g)", testlist[i].expect_sin,
                 asinf_res, asinf_res - expect_asin);
        }
        if (fabsf(acosf_res / (float)expect_acos - 1) > 1.0e-6f) {
            FAIL("dacosf(%g) = %g (d=%g)", testlist[i].expect_sin,
                 acosf_res, acosf_res - expect_acos);
        }
        if (fabsf(atanf_res / (float)expect_atan - 1) > 1.0e-6f) {
            FAIL("datan2f(%g,%g) = %g (d=%g)", testlist[i].expect_sin,
                 testlist[i].expect_cos, atanf_res, atanf_res - expect_atan);
        }
        if (fabsf(sincosf_sin / (float)testlist[i].expect_sin - 1) > 1.0e-6f) {
            FAIL("dsincosf(%g).sin = %g (d=%g)", testlist[i].deg,
                 sincosf_sin, sincosf_sin - testlist[i].expect_sin);
        }
        if (fabsf(sincosf_cos / (float)testlist[i].expect_cos - 1) > 1.0e-6f) {
            FAIL("dsincosf(%g).cos = %g (d=%g)", testlist[i].deg,
                 sincosf_cos, sincosf_cos - testlist[i].expect_cos);
        }
    }

    return !failed;
}

/*************************************************************************/
/*************************************************************************/
