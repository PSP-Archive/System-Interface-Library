/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/math/vector.c: Tests for vector functions.
 */

#include "src/base.h"
#include "src/math.h"
#include "src/test/base.h"
#include "src/test/math/internal.h"

/*************************************************************************/
/*************************************************************************/

DEFINE_MATH_TEST_RUNNER(test_math_vector)

/*-----------------------------------------------------------------------*/

/* Continue through failures.  (This must come after the test runner is
 * defined.) */
#undef FAIL_ACTION
#define FAIL_ACTION  failed = 1

/*-----------------------------------------------------------------------*/

TEST(test_fpu_mode)
{
    /* Check rounding mode, floating-point precision, and denormal handling
     * separately, so we can report them as likely causes of error. */

    int failed = 0;
    volatile float temp;  // Volatile temporary to prevent constant folding.
    Vector4f a, b, res;
    const union {uint32_t i; float f;} five_thirds = {.i = 0x3FD55555};

    temp = five_thirds.f;
    a.x = a.y = a.z = a.w = temp;
    temp = 1.5f;
    b.x = b.y = b.z = b.w = temp;
    res = vec4_mul(a, b);
    if (res.x != 2.5f || res.y != 2.5f || res.z != 2.5f || res.w != 2.5f) {
        FAIL("vec4_mul(5/3,3/2) failed -- rounding mode bug?");
    }

    temp = -(five_thirds.f);
    a.x = a.y = a.z = a.w = temp;
    temp = 1.5f;
    b.x = b.y = b.z = b.w = temp;
    res = vec4_mul(a, b);
    if (res.x != -2.5f || res.y != -2.5f || res.z != -2.5f || res.w != -2.5f) {
        FAIL("vec4_mul(-5/3,3/2) failed -- rounding mode bug?");
    }

    temp = 1.0f;
    a.x = a.y = a.z = a.w = temp;
    temp = TINY;
    b.x = b.y = b.z = b.w = temp;
    res = vec4_add(a, b);
    if (res.x != a.x || res.y != a.y || res.z != a.z || res.w != a.w) {
        FAIL("vec4_add(1.0,TINY) failed -- precision or rounding mode bug?"
#ifdef SIL_ARCH_X86
             " (Did you forget to build with -msse -mfpmath=sse on x86?)"
#endif
            );
    }

    temp = 1.0f;
    a.x = a.y = a.z = a.w = temp;
    temp = TINY*2;
    b.x = b.y = b.z = b.w = temp;
    res = vec4_add(a, b);
    if (res.x == a.x || res.y == a.y || res.z == a.z || res.w == a.w) {
        FAIL("vec4_add(1.0,TINY*2) failed -- precision or rounding mode bug?");
    }

    temp = 1.0f;
    a.x = a.y = a.z = a.w = temp;
    temp = TINY/2;
    b.x = b.y = b.z = b.w = temp;
    res = vec4_sub(a, b);
    if (res.x != a.x || res.y != a.y || res.z != a.z || res.w != a.w) {
        FAIL("vec4_sub(1.0,TINY/2) failed -- precision or rounding mode bug?"
#ifdef SIL_ARCH_X86
             " (Did you forget to build with -msse -mfpmath=sse on x86?)"
#endif
            );
    }

    temp = 1.0f;
    a.x = a.y = a.z = a.w = temp;
    temp = TINY;
    b.x = b.y = b.z = b.w = temp;
    res = vec4_sub(a, b);
    if (res.x == a.x || res.y == a.y || res.z == a.z || res.w == a.w) {
        FAIL("vec4_sub(1.0,TINY/2) failed -- precision or rounding mode bug?");
    }

    temp = 1.0f;
    a.x = a.y = a.z = a.w = temp;
    res = vec4_add_scalar(a, TINY);
    if (res.x != a.x || res.y != a.y || res.z != a.z || res.w != a.w) {
        FAIL("vec4_add_scalar(1.0,TINY) failed -- precision or rounding mode"
             " bug?"
#ifdef SIL_ARCH_X86
             " (Did you forget to build with -msse -mfpmath=sse on x86?)"
#endif
            );
    }

    temp = 1.0f;
    a.x = a.y = a.z = a.w = temp;
    res = vec4_add_scalar(a, TINY*2);
    if (res.x == a.x || res.y == a.y || res.z == a.z || res.w == a.w) {
        FAIL("vec4_add_scalar(1.0,TINY) failed -- precision or rounding mode"
             " bug?");
    }

    temp = 1.0f;
    a.x = a.y = a.z = a.w = temp;
    res = vec4_add_scalar(a, -TINY/2);
    if (res.x != a.x || res.y != a.y || res.z != a.z || res.w != a.w) {
        FAIL("vec4_add_scalar(1.0,-TINY/2) failed -- precision or rounding"
             " mode bug?"
#ifdef SIL_ARCH_X86
             " (Did you forget to build with -msse -mfpmath=sse on x86?)"
#endif
            );
    }

    temp = 1.0f;
    a.x = a.y = a.z = a.w = temp;
    res = vec4_add_scalar(a, -TINY);
    if (res.x == a.x || res.y == a.y || res.z == a.z || res.w == a.w) {
        FAIL("vec4_add_scalar(1.0,-TINY) failed -- precision or rounding mode"
             " bug?");
    }

    temp = 1.0e-20f;
    a.x = temp;
    a.y = temp;  // Separate assignment to block constant folding.
    temp = a.x * a.y;
    a.z = temp;
    if (a.z != 0) {
        FAIL("1e-20 * 1e-20 != 0 -- denormal bug? (Denormals must be"
             " flushed to zero.)");
    }

    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_basic)
{
    int failed = 0;

    for (int i = 0; i < lenof_vector_tests(); i++) {
        if (vector_tests[i].type == PLUS
         || vector_tests[i].type == NEG
         || vector_tests[i].type == S_SUB
         || vector_tests[i].type == S_DIV
         || vector_tests[i].size_b != vector_tests[i].size) {
            continue;  // Skip C++-specific tests.
        }

        if (vector_tests[i].size == 2) {
            const Vector2f a = {vector_tests[i].a[0], vector_tests[i].a[1]};
            const Vector2f b = {vector_tests[i].b[0], vector_tests[i].b[1]};
            const float k = vector_tests[i].k;
            Vector2f res = {0, 0};
            switch (vector_tests[i].type) {
                case ADD:    res   = vec2_add(a, b);            break;
                case SUB:    res   = vec2_sub(a, b);            break;
                case MUL:    res   = vec2_mul(a, b);            break;
                case DIV:    res   = vec2_div(a, b);            break;
                case S_ADD:  res   = vec2_add_scalar(a, k);     break;
                case SCALE:  res   = vec2_scale(a, k);          break;
                case LEN:    res.x = vec2_length(a);            break;
                case LEN2:   res.x = vec2_length2(a);           break;
                case NON_0:  res.x = vec2_is_nonzero(a);        break;
                case LEN_IN: res.x = vec2_is_length_in(a, k);   break;
                case NORM:   res   = vec2_normalize(a);         break;
                case SETLEN: res   = vec2_set_length(a, k);     break;
                case CAPLEN: res   = vec2_cap_length(a, k);     break;
                case DOT:    res.x = vec2_dot(a, b);            break;
                case LERP:   res   = vec2_lerp(a, b, k);        break;
                case CROSS:  FAIL("test %u: CROSS(2) invalid", i); break;
                default:     FAIL("test %u: invalid type", i);  break;
            }
            if (vector_tests[i].allow_approximate
             && CLOSE_ENOUGH(res.x, vector_tests[i].res[0])
             && CLOSE_ENOUGH(res.y, vector_tests[i].res[1])) {
                continue;
            }
            if (res.x != vector_tests[i].res[0]
             || res.y != vector_tests[i].res[1]) {
                FAIL("test %u: result <%f,%f> != expect <%f,%f>",
                     i, res.x, res.y,
                     vector_tests[i].res[0], vector_tests[i].res[1]);
            }

        } else if (vector_tests[i].size == 3) {
            const Vector3f a = {vector_tests[i].a[0], vector_tests[i].a[1],
                                vector_tests[i].a[2]};
            const Vector3f b = {vector_tests[i].b[0], vector_tests[i].b[1],
                                vector_tests[i].b[2]};
            const float k = vector_tests[i].k;
            Vector3f res = {0, 0, 0};
            switch (vector_tests[i].type) {
                case ADD:    res   = vec3_add(a, b);            break;
                case SUB:    res   = vec3_sub(a, b);            break;
                case MUL:    res   = vec3_mul(a, b);            break;
                case DIV:    res   = vec3_div(a, b);            break;
                case S_ADD:  res   = vec3_add_scalar(a, k);     break;
                case SCALE:  res   = vec3_scale(a, k);          break;
                case LEN:    res.x = vec3_length(a);            break;
                case LEN2:   res.x = vec3_length2(a);           break;
                case NON_0:  res.x = vec3_is_nonzero(a);        break;
                case LEN_IN: res.x = vec3_is_length_in(a, k);   break;
                case NORM:   res   = vec3_normalize(a);         break;
                case SETLEN: res   = vec3_set_length(a, k);     break;
                case CAPLEN: res   = vec3_cap_length(a, k);     break;
                case DOT:    res.x = vec3_dot(a, b);            break;
                case LERP:   res   = vec3_lerp(a, b, k);        break;
                case CROSS:  res   = vec3_cross(a, b);          break;
                default:     FAIL("test %u: invalid type", i);  break;
            }
            if (vector_tests[i].allow_approximate
             && CLOSE_ENOUGH(res.x, vector_tests[i].res[0])
             && CLOSE_ENOUGH(res.y, vector_tests[i].res[1])
             && CLOSE_ENOUGH(res.z, vector_tests[i].res[2])) {
                continue;
            }
            if (res.x != vector_tests[i].res[0]
             || res.y != vector_tests[i].res[1]
             || res.z != vector_tests[i].res[2]) {
                FAIL("test %u: result <%f,%f,%f> != expect <%f,%f,%f>",
                     i, res.x, res.y, res.z,
                     vector_tests[i].res[0], vector_tests[i].res[1],
                     vector_tests[i].res[2]);
            }

        } else if (vector_tests[i].size == 4) {
            const Vector4f a = {vector_tests[i].a[0], vector_tests[i].a[1],
                                vector_tests[i].a[2], vector_tests[i].a[3]};
            const Vector4f b = {vector_tests[i].b[0], vector_tests[i].b[1],
                                vector_tests[i].b[2], vector_tests[i].b[3]};
            const float k = vector_tests[i].k;
            Vector4f res = {0, 0, 0, 0};
            switch (vector_tests[i].type) {
                case ADD:    res   = vec4_add(a, b);            break;
                case SUB:    res   = vec4_sub(a, b);            break;
                case MUL:    res   = vec4_mul(a, b);            break;
                case DIV:    res   = vec4_div(a, b);            break;
                case S_ADD:  res   = vec4_add_scalar(a, k);     break;
                case SCALE:  res   = vec4_scale(a, k);          break;
                case LEN:    res.x = vec4_length(a);            break;
                case LEN2:   res.x = vec4_length2(a);           break;
                case NON_0:  res.x = vec4_is_nonzero(a);        break;
                case LEN_IN: res.x = vec4_is_length_in(a, k);   break;
                case NORM:   res   = vec4_normalize(a);         break;
                case SETLEN: res   = vec4_set_length(a, k);     break;
                case CAPLEN: res   = vec4_cap_length(a, k);     break;
                case DOT:    res.x = vec4_dot(a, b);            break;
                case LERP:   res   = vec4_lerp(a, b, k);        break;
                case CROSS:  FAIL("test %u: CROSS(4) invalid", i); break;
                default:     FAIL("test %u: invalid type", i);  break;
            }
            if (vector_tests[i].allow_approximate
             && CLOSE_ENOUGH(res.x, vector_tests[i].res[0])
             && CLOSE_ENOUGH(res.y, vector_tests[i].res[1])
             && CLOSE_ENOUGH(res.z, vector_tests[i].res[2])
             && CLOSE_ENOUGH(res.w, vector_tests[i].res[3])) {
                continue;
            }
            if (res.x != vector_tests[i].res[0]
             || res.y != vector_tests[i].res[1]
             || res.z != vector_tests[i].res[2]
             || res.w != vector_tests[i].res[3]) {
                FAIL("test %u: result <%f,%f,%f,%f> != expect <%f,%f,%f,%f>",
                     i, res.x, res.y, res.z, res.w,
                     vector_tests[i].res[0], vector_tests[i].res[1],
                     vector_tests[i].res[2], vector_tests[i].res[3]);
            }

        } else {
            FAIL("test %u: bad vector size %d", i, vector_tests[i].size);
        }
    }

    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_cross_overwrite)
{
    /* Ensure that vec3_cross() and vec4_cross() behave properly when the
     * output variable is the same as one of the input variables.  (This
     * should never be a problem in theory, but this guards against errors
     * introduced by buggy optimizers.) */

    int failed = 0;
    Vector3f a3, b3;

    a3.x = a3.y = a3.z = 1;
    b3.x = 2; b3.y = 4; b3.z = 7;
    a3 = vec3_cross(a3, b3);
    if (a3.x != 3 || a3.y != -5 || a3.z != 2) {
        FAIL("vec3_cross(dest == src1): result=<%.2f,%.2f,%.2f>"
             " expect=<3.00,-5.00,2.00>", a3.x, a3.y, a3.z);
    }

    a3.x = a3.y = a3.z = 1;
    b3 = vec3_cross(a3, b3);
    if (b3.x != 3 || b3.y != -5 || b3.z != 2) {
        FAIL("vec3_cross(dest == src2): result=<%.2f,%.2f,%.2f>"
             " expect=<3.00,-5.00,2.00>", b3.x, b3.y, b3.z);
    }

    Vector4f a4, b4, c4;

    a4.x = a4.y = a4.z = a4.w = 1;
    b4.x = 2; b4.y = 4; b4.z = 7; b4.w = 11;
    c4.x = -5; c4.y = -11; c4.z = -18; c4.w = -26;
    a4 = vec4_cross(a4, b4, c4);
    if (a4.x != 4 || a4.y != -12 || a4.z != 12 || a4.w != -4) {
        FAIL("vec4_cross(dest == src1): result=<%.2f,%.2f,%.2f,%.2f>"
             " expect=<4.00,-12.00,12.00,-4.00>", a4.x, a4.y, a4.z, a4.w);
    }

    a4.x = a4.y = a4.z = a4.w = 1;
    b4 = vec4_cross(a4, b4, c4);
    if (b4.x != 4 || b4.y != -12 || b4.z != 12 || b4.w != -4) {
        FAIL("vec4_cross(dest == src2): result=<%.2f,%.2f,%.2f,%.2f>"
             " expect=<4.00,-12.00,12.00,-4.00>", b4.x, b4.y, b4.z, b4.w);
    }

    b4.x = 2; b4.y = 4; b4.z = 7; b4.w = 11;
    c4 = vec4_cross(a4, b4, c4);
    if (c4.x != 4 || c4.y != -12 || c4.z != 12 || c4.w != -4) {
        FAIL("vec4_cross(dest == src3): result=<%.2f,%.2f,%.2f,%.2f>"
             " expect=<4.00,-12.00,12.00,-4.00>", c4.x, c4.y, c4.z, c4.w);
    }

    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_vec4_cross)
{
    int failed = 0;

    for (int i = 0; i < lenof_vector_cross4_tests(); i++) {
        Vector4f a, b, c, res;
        a.x = vector_cross4_tests[i].a[0];
        a.y = vector_cross4_tests[i].a[1];
        a.z = vector_cross4_tests[i].a[2];
        a.w = vector_cross4_tests[i].a[3];
        b.x = vector_cross4_tests[i].b[0];
        b.y = vector_cross4_tests[i].b[1];
        b.z = vector_cross4_tests[i].b[2];
        b.w = vector_cross4_tests[i].b[3];
        c.x = vector_cross4_tests[i].c[0];
        c.y = vector_cross4_tests[i].c[1];
        c.z = vector_cross4_tests[i].c[2];
        c.w = vector_cross4_tests[i].c[3];
        res.x = res.y = res.z = res.w = 0;
        res = vec4_cross(a, b, c);
        if (res.x != vector_cross4_tests[i].res[0]
         || res.y != vector_cross4_tests[i].res[1]
         || res.z != vector_cross4_tests[i].res[2]
         || res.w != vector_cross4_tests[i].res[3]) {
            FAIL("test %u: result <%f,%f,%f,%f> != expect <%f,%f,%f,%f>",
                 i, res.x, res.y, res.z, res.w,
                 vector_cross4_tests[i].res[0], vector_cross4_tests[i].res[1],
                 vector_cross4_tests[i].res[2], vector_cross4_tests[i].res[3]);
        }
    }

    return !failed;
}

/*************************************************************************/
/*************************************************************************/
