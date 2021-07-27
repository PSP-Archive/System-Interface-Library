/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/math/vector-cxx.cc: Tests for C++-specific vector functions.
 */

#include "src/base.h"
#include "src/math.h"
#include "src/test/base.h"
#include "src/test/math/internal.h"

/*************************************************************************/
/*************************************************************************/

DEFINE_MATH_TEST_RUNNER(test_math_vector_cxx)

/*-----------------------------------------------------------------------*/

/* Continue through failures.  (This must come after the test runner is
 * defined.) */
#undef FAIL_ACTION
#define FAIL_ACTION  failed = 1

/*-----------------------------------------------------------------------*/

TEST(test_constructor)
{
    int failed = 0;

    Vector2f v2;
    v2.x = v2.y = 2;
    Vector3f v3;
    v3.x = v3.y = v3.z = 3;
    Vector4f v4;
    v4.x = v4.y = v4.z = v4.w = 4;

    Vector2f test2_0;
    if (test2_0.x != 0 || test2_0.y != 0) {
        FAIL("Vector2f()\n");
    }
    Vector2f test2_1(1);
    if (test2_1.x != 1 || test2_1.y != 0) {
        FAIL("Vector2f(1)\n");
    }
    Vector2f test2_2(2,2);
    if (test2_2.x != 2 || test2_2.y != 2) {
        FAIL("Vector2f(2,2)\n");
    }
    Vector2f test2_v2(v2);
    if (test2_v2.x != 2 || test2_v2.y != 2) {
        FAIL("Vector2f(Vector2f)\n");
    }
    Vector2f test2_v3(v3);
    if (test2_v3.x != 3 || test2_v3.y != 3) {
        FAIL("Vector2f(Vector3f)\n");
    }
    Vector2f test2_v4(v4);
    if (test2_v4.x != 4 || test2_v4.y != 4) {
        FAIL("Vector2f(Vector4f)\n");
    }

    Vector3f test3_0;
    if (test3_0.x != 0 || test3_0.y != 0 || test3_0.z != 0) {
        FAIL("Vector3f()\n");
    }
    Vector3f test3_1(1);
    if (test3_1.x != 1 || test3_1.y != 0 || test3_1.z != 0) {
        FAIL("Vector3f(1)\n");
    }
    Vector3f test3_2(2,2);
    if (test3_2.x != 2 || test3_2.y != 2 || test3_2.z != 0) {
        FAIL("Vector3f(2,2)\n");
    }
    Vector3f test3_3(3,3,3);
    if (test3_3.x != 3 || test3_3.y != 3 || test3_3.z != 3) {
        FAIL("Vector3f(3,3,3)\n");
    }
    Vector3f test3_v2(v2);
    if (test3_v2.x != 2 || test3_v2.y != 2 || test3_v2.z != 0) {
        FAIL("Vector3f(Vector2f)\n");
    }
    Vector3f test3_v3(v3);
    if (test3_v3.x != 3 || test3_v3.y != 3 || test3_v3.z != 3) {
        FAIL("Vector3f(Vector3f)\n");
    }
    Vector3f test3_v4(v4);
    if (test3_v4.x != 4 || test3_v4.y != 4 || test3_v4.z != 4) {
        FAIL("Vector3f(Vector4f)\n");
    }

    Vector4f test4_0;
    if (test4_0.x != 0 || test4_0.y != 0 || test4_0.z != 0 || test4_0.w != 0) {
        FAIL("Vector4f()\n");
    }
    Vector4f test4_1(1);
    if (test4_1.x != 1 || test4_1.y != 0 || test4_1.z != 0 || test4_1.w != 0) {
        FAIL("Vector4f(1)\n");
    }
    Vector4f test4_2(2,2);
    if (test4_2.x != 2 || test4_2.y != 2 || test4_2.z != 0 || test4_2.w != 0) {
        FAIL("Vector4f(2,2)\n");
    }
    Vector4f test4_3(3,3,3);
    if (test4_3.x != 3 || test4_3.y != 3 || test4_3.z != 3 || test4_3.w != 0) {
        FAIL("Vector4f(3,3,3)\n");
    }
    Vector4f test4_4(4,4,4,4);
    if (test4_4.x != 4 || test4_4.y != 4 || test4_4.z != 4 || test4_4.w != 4) {
        FAIL("Vector4f(4,4,4,4)\n");
    }
    Vector4f test4_v2(v2);
    if (test4_v2.x != 2 || test4_v2.y != 2 || test4_v2.z != 0 || test4_v2.w != 0) {
        FAIL("Vector4f(Vector2f)\n");
    }
    Vector4f test4_v3(v3);
    if (test4_v3.x != 3 || test4_v3.y != 3 || test4_v3.z != 3 || test4_v3.w != 0) {
        FAIL("Vector4f(Vector3f)\n");
    }
    Vector4f test4_v4(v4);
    if (test4_v4.x != 4 || test4_v4.y != 4 || test4_v4.z != 4 || test4_v4.w != 4) {
        FAIL("Vector4f(Vector4f)\n");
    }

    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_assignment)
{
    int failed = 0;

    Vector2f v2;
    v2.x = v2.y = 2;
    Vector3f v3;
    v3.x = v3.y = v3.z = 3;
    Vector4f v4;
    v4.x = v4.y = v4.z = v4.w = 4;

    Vector2f test2_k = 1;
    if (test2_k.x != 1 || test2_k.y != 0) {
        FAIL("Vector2f = float\n");
    }
    Vector2f test2_v2 = v2;
    if (test2_v2.x != 2 || test2_v2.y != 2) {
        FAIL("Vector2f = Vector2f\n");
    }
    Vector2f test2_v3 = v3;
    if (test2_v3.x != 3 || test2_v3.y != 3) {
        FAIL("Vector2f = Vector3f\n");
    }
    Vector2f test2_v4 = v4;
    if (test2_v4.x != 4 || test2_v4.y != 4) {
        FAIL("Vector2f = Vector4f\n");
    }

    Vector3f test3_k = 1;
    if (test3_k.x != 1 || test3_k.y != 0 || test3_k.z != 0) {
        FAIL("Vector3f = float\n");
    }
    Vector3f test3_v2 = v2;
    if (test3_v2.x != 2 || test3_v2.y != 2 || test3_v2.z != 0) {
        FAIL("Vector3f = Vector2f\n");
    }
    Vector3f test3_v3 = v3;
    if (test3_v3.x != 3 || test3_v3.y != 3 || test3_v3.z != 3) {
        FAIL("Vector3f = Vector3f\n");
    }
    Vector3f test3_v4 = v4;
    if (test3_v4.x != 4 || test3_v4.y != 4 || test3_v4.z != 4) {
        FAIL("Vector3f = Vector4f\n");
    }

    Vector4f test4_k = 1;
    if (test4_k.x != 1 || test4_k.y != 0 || test4_k.z != 0 || test4_k.w != 0) {
        FAIL("Vector4f = float\n");
    }
    Vector4f test4_v2 = v2;
    if (test4_v2.x != 2 || test4_v2.y != 2 || test4_v2.z != 0 || test4_v2.w != 0) {
        FAIL("Vector4f = Vector2f\n");
    }
    Vector4f test4_v3 = v3;
    if (test4_v3.x != 3 || test4_v3.y != 3 || test4_v3.z != 3 || test4_v3.w != 0) {
        FAIL("Vector4f = Vector3f\n");
    }
    Vector4f test4_v4 = v4;
    if (test4_v4.x != 4 || test4_v4.y != 4 || test4_v4.z != 4 || test4_v4.w != 4) {
        FAIL("Vector4f = Vector4f\n");
    }

    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_comparison)
{
    int failed = 0;

    union {uint32_t i; float f;} u;
    u.i = 0x7FFFFFFF;
    const float nan = u.f;

    CHECK_TRUE (Vector2f(1,2) == Vector2f(1,2));
    CHECK_FALSE(Vector2f(1,2) == Vector2f(1,0));
    CHECK_FALSE(Vector2f(1,2) == Vector2f(0,2));
    CHECK_FALSE(Vector2f(1,2) == Vector2f(1,nan));
    CHECK_FALSE(Vector2f(1,2) == Vector2f(nan,2));

    CHECK_FALSE(Vector2f(1,2) != Vector2f(1,2));
    CHECK_TRUE (Vector2f(1,2) != Vector2f(1,0));
    CHECK_TRUE (Vector2f(1,2) != Vector2f(0,2));
    CHECK_TRUE (Vector2f(1,2) != Vector2f(1,nan));
    CHECK_TRUE (Vector2f(1,2) != Vector2f(nan,2));

    CHECK_TRUE (Vector3f(1,2,3) == Vector3f(1,2,3));
    CHECK_FALSE(Vector3f(1,2,3) == Vector3f(1,2,0));
    CHECK_FALSE(Vector3f(1,2,3) == Vector3f(1,0,3));
    CHECK_FALSE(Vector3f(1,2,3) == Vector3f(0,2,3));
    CHECK_FALSE(Vector3f(1,2,3) == Vector3f(1,2,nan));
    CHECK_FALSE(Vector3f(1,2,3) == Vector3f(1,nan,3));
    CHECK_FALSE(Vector3f(1,2,3) == Vector3f(nan,2,3));

    CHECK_FALSE(Vector3f(1,2,3) != Vector3f(1,2,3));
    CHECK_TRUE (Vector3f(1,2,3) != Vector3f(1,2,0));
    CHECK_TRUE (Vector3f(1,2,3) != Vector3f(1,0,3));
    CHECK_TRUE (Vector3f(1,2,3) != Vector3f(0,2,3));
    CHECK_TRUE (Vector3f(1,2,3) != Vector3f(1,2,nan));
    CHECK_TRUE (Vector3f(1,2,3) != Vector3f(1,nan,3));
    CHECK_TRUE (Vector3f(1,2,3) != Vector3f(nan,2,3));

    CHECK_TRUE (Vector4f(1,2,3,4) == Vector4f(1,2,3,4));
    CHECK_FALSE(Vector4f(1,2,3,4) == Vector4f(1,2,3,0));
    CHECK_FALSE(Vector4f(1,2,3,4) == Vector4f(1,2,0,4));
    CHECK_FALSE(Vector4f(1,2,3,4) == Vector4f(1,0,3,4));
    CHECK_FALSE(Vector4f(1,2,3,4) == Vector4f(0,2,3,4));
    CHECK_FALSE(Vector4f(1,2,3,4) == Vector4f(1,2,3,nan));
    CHECK_FALSE(Vector4f(1,2,3,4) == Vector4f(1,2,nan,4));
    CHECK_FALSE(Vector4f(1,2,3,4) == Vector4f(1,nan,3,4));
    CHECK_FALSE(Vector4f(1,2,3,4) == Vector4f(nan,2,3,4));

    CHECK_FALSE(Vector4f(1,2,3,4) != Vector4f(1,2,3,4));
    CHECK_TRUE (Vector4f(1,2,3,4) != Vector4f(1,2,3,0));
    CHECK_TRUE (Vector4f(1,2,3,4) != Vector4f(1,2,0,4));
    CHECK_TRUE (Vector4f(1,2,3,4) != Vector4f(1,0,3,4));
    CHECK_TRUE (Vector4f(1,2,3,4) != Vector4f(0,2,3,4));
    CHECK_TRUE (Vector4f(1,2,3,4) != Vector4f(1,2,3,nan));
    CHECK_TRUE (Vector4f(1,2,3,4) != Vector4f(1,2,nan,4));
    CHECK_TRUE (Vector4f(1,2,3,4) != Vector4f(1,nan,3,4));
    CHECK_TRUE (Vector4f(1,2,3,4) != Vector4f(nan,2,3,4));

    return !failed;
}

/*-----------------------------------------------------------------------*/

TEST(test_basic)
{
    int failed = 0;

    for (int i = 0; i < lenof_vector_tests(); i++) {
        if (vector_tests[i].size == 2) {
            const Vector2f a(vector_tests[i].a[0], vector_tests[i].a[1]);
            const Vector2f b2(vector_tests[i].b[0], vector_tests[i].b[1]);
            const Vector3f b3(vector_tests[i].b[0], vector_tests[i].b[1],
                              vector_tests[i].b[2]);
            const Vector4f b4(vector_tests[i].b[0], vector_tests[i].b[1],
                              vector_tests[i].b[2], vector_tests[i].b[3]);
            const float k = vector_tests[i].k;
            Vector2f res1;
            Vector2f res2(a);
            const bool have_res2 =
                (vector_tests[i].type > NEG && vector_tests[i].type < LEN);
            switch (vector_tests[i].type) {
                case PLUS:   res1 = +a; break;
                case NEG:    res1 = -a; break;
                case ADD:    if (vector_tests[i].size_b == 2) {
                                 res1 = a + b2; res2 += b2;
                             } else if (vector_tests[i].size_b == 3) {
                                 res1 = a + b3; res2 += b3;
                             } else { // vector_tests[i].size_b == 4
                                 res1 = a + b4; res2 += b4;
                             }
                             break;
                case SUB:    if (vector_tests[i].size_b == 2) {
                                 res1 = a - b2; res2 -= b2;
                             } else if (vector_tests[i].size_b == 3) {
                                 res1 = a - b3; res2 -= b3;
                             } else { // vector_tests[i].size_b == 4
                                 res1 = a - b4; res2 -= b4;
                             }
                             break;
                case MUL:    if (vector_tests[i].size_b == 2) {
                                 res1 = a * b2; res2 *= b2;
                             } else if (vector_tests[i].size_b == 3) {
                                 res1 = a * b3; res2 *= b3;
                             } else { // vector_tests[i].size_b == 4
                                 res1 = a * b4; res2 *= b4;
                             }
                             break;
                case DIV:    if (vector_tests[i].size_b == 2) {
                                 res1 = a / b2; res2 /= b2;
                             } else if (vector_tests[i].size_b == 3) {
                                 res1 = a / b3; res2 /= b3;
                             } else { // vector_tests[i].size_b == 4
                                 res1 = a / b4; res2 /= b4;
                             }
                             break;
                case S_ADD:  res1 = a + k; res2 += k; break;
                case S_SUB:  res1 = a - k; res2 -= k; break;
                case SCALE:  res1 = a * k; res2 *= k; break;
                case S_DIV:  res1 = a / k; res2 /= k; break;
                case LEN:    res1.x = a.length(); break;
                case LEN2:   res1.x = a.length2(); break;
                case NON_0:  res1.x = a.is_nonzero(); break;
                case LEN_IN: res1.x = a.is_length_in(k); break;
                case NORM:   res1 = a; res1.normalize(); break;
                case SETLEN: res1 = a; res1.set_length(k); break;
                case CAPLEN: res1 = a; res1.cap_length(k); break;
                case DOT:    res1.x = a.dot(b2); break;
                case LERP:   res1 = a.lerp(b2, k); break;
                case CROSS:  FAIL("test %u: CROSS(2) invalid", i); break;
                default:     FAIL("test %u: invalid type", i); break;
            }
            if (vector_tests[i].allow_approximate
             && CLOSE_ENOUGH(res1.x, vector_tests[i].res[0])
             && CLOSE_ENOUGH(res1.y, vector_tests[i].res[1])
             && (!have_res2
                 || (CLOSE_ENOUGH(res2.x, vector_tests[i].res[0])
                  && CLOSE_ENOUGH(res2.y, vector_tests[i].res[1]))))
            {
                continue;
            }
            if (res1.x != vector_tests[i].res[0]
             || res1.y != vector_tests[i].res[1])
            {
                FAIL("test %u: result <%f,%f> != expect <%f,%f>",
                     i, res1.x, res1.y,
                     vector_tests[i].res[0], vector_tests[i].res[1]);
            }
            if (have_res2
                && (res2.x != vector_tests[i].res[0]
                 || res2.y != vector_tests[i].res[1]))
            {
                FAIL("test %u: result 2 <%f,%f> != expect <%f,%f>",
                     i, res2.x, res2.y,
                     vector_tests[i].res[0], vector_tests[i].res[1]);
            }

        } else if (vector_tests[i].size == 3) {
            const Vector3f a(vector_tests[i].a[0], vector_tests[i].a[1],
                             vector_tests[i].a[2]);
            const Vector2f b2(vector_tests[i].b[0], vector_tests[i].b[1]);
            const Vector3f b3(vector_tests[i].b[0], vector_tests[i].b[1],
                              vector_tests[i].b[2]);
            const Vector4f b4(vector_tests[i].b[0], vector_tests[i].b[1],
                              vector_tests[i].b[2], vector_tests[i].b[3]);
            const float k = vector_tests[i].k;
            Vector3f res1;
            Vector3f res2(a);
            const bool have_res2 =
                (vector_tests[i].type > NEG && vector_tests[i].type < LEN);
            switch (vector_tests[i].type) {
                case PLUS:   res1 = +a; break;
                case NEG:    res1 = -a; break;
                case ADD:    if (vector_tests[i].size_b == 2) {
                                 res1 = a + b2; res2 += b2;
                             } else if (vector_tests[i].size_b == 3) {
                                 res1 = a + b3; res2 += b3;
                             } else { // vector_tests[i].size_b == 4
                                 res1 = a + b4; res2 += b4;
                             }
                             break;
                case SUB:    if (vector_tests[i].size_b == 2) {
                                 res1 = a - b2; res2 -= b2;
                             } else if (vector_tests[i].size_b == 3) {
                                 res1 = a - b3; res2 -= b3;
                             } else { // vector_tests[i].size_b == 4
                                 res1 = a - b4; res2 -= b4;
                             }
                             break;
                case MUL:    if (vector_tests[i].size_b == 2) {
                                 res1 = a * b2; res2 *= b2;
                             } else if (vector_tests[i].size_b == 3) {
                                 res1 = a * b3; res2 *= b3;
                             } else { // vector_tests[i].size_b == 4
                                 res1 = a * b4; res2 *= b4;
                             }
                             break;
                case DIV:    if (vector_tests[i].size_b == 2) {
                                 res1 = a / b2; res2 /= b2;
                             } else if (vector_tests[i].size_b == 3) {
                                 res1 = a / b3; res2 /= b3;
                             } else { // vector_tests[i].size_b == 4
                                 res1 = a / b4; res2 /= b4;
                             }
                             break;
                case S_ADD:  res1 = a + k; res2 += k; break;
                case S_SUB:  res1 = a - k; res2 -= k; break;
                case SCALE:  res1 = a * k; res2 *= k; break;
                case S_DIV:  res1 = a / k; res2 /= k; break;
                case LEN:    res1.x = a.length(); break;
                case LEN2:   res1.x = a.length2(); break;
                case NON_0:  res1.x = a.is_nonzero(); break;
                case LEN_IN: res1.x = a.is_length_in(k); break;
                case NORM:   res1 = a; res1.normalize(); break;
                case SETLEN: res1 = a; res1.set_length(k); break;
                case CAPLEN: res1 = a; res1.cap_length(k); break;
                case DOT:    res1.x = a.dot(b3); break;
                case LERP:   res1 = a.lerp(b3, k); break;
                case CROSS:  res1 = a.cross(b3); break;
                default:     FAIL("test %u: invalid type", i); break;
            }
            if (vector_tests[i].allow_approximate
             && CLOSE_ENOUGH(res1.x, vector_tests[i].res[0])
             && CLOSE_ENOUGH(res1.y, vector_tests[i].res[1])
             && CLOSE_ENOUGH(res1.z, vector_tests[i].res[2])
             && (!have_res2
                 || (CLOSE_ENOUGH(res2.x, vector_tests[i].res[0])
                  && CLOSE_ENOUGH(res2.y, vector_tests[i].res[1])
                  && CLOSE_ENOUGH(res2.z, vector_tests[i].res[2]))))
            {
                continue;
            }
            if (res1.x != vector_tests[i].res[0]
             || res1.y != vector_tests[i].res[1]
             || res1.z != vector_tests[i].res[2])
            {
                FAIL("test %u: result <%f,%f,%f> != expect <%f,%f,%f>",
                     i, res1.x, res1.y, res1.z,
                     vector_tests[i].res[0], vector_tests[i].res[1],
                     vector_tests[i].res[2]);
            }
            if (have_res2
                && (res2.x != vector_tests[i].res[0]
                 || res2.y != vector_tests[i].res[1]
                 || res2.z != vector_tests[i].res[2]))
            {
                FAIL("test %u: result 2 <%f,%f,%f> != expect <%f,%f,%f>",
                     i, res2.x, res2.y, res2.z,
                     vector_tests[i].res[0], vector_tests[i].res[1],
                     vector_tests[i].res[2]);
            }

        } else if (vector_tests[i].size == 4) {
            const Vector4f a(vector_tests[i].a[0], vector_tests[i].a[1],
                             vector_tests[i].a[2], vector_tests[i].a[3]);
            const Vector2f b2(vector_tests[i].b[0], vector_tests[i].b[1]);
            const Vector3f b3(vector_tests[i].b[0], vector_tests[i].b[1],
                              vector_tests[i].b[2]);
            const Vector4f b4(vector_tests[i].b[0], vector_tests[i].b[1],
                              vector_tests[i].b[2], vector_tests[i].b[3]);
            const float k = vector_tests[i].k;
            Vector4f res1;
            Vector4f res2(a);
            const bool have_res2 =
                (vector_tests[i].type > NEG && vector_tests[i].type < LEN);
            switch (vector_tests[i].type) {
                case PLUS:   res1 = +a; break;
                case NEG:    res1 = -a; break;
                case ADD:    if (vector_tests[i].size_b == 2) {
                                 res1 = a + b2; res2 += b2;
                             } else if (vector_tests[i].size_b == 3) {
                                 res1 = a + b3; res2 += b3;
                             } else { // vector_tests[i].size_b == 4
                                 res1 = a + b4; res2 += b4;
                             }
                             break;
                case SUB:    if (vector_tests[i].size_b == 2) {
                                 res1 = a - b2; res2 -= b2;
                             } else if (vector_tests[i].size_b == 3) {
                                 res1 = a - b3; res2 -= b3;
                             } else { // vector_tests[i].size_b == 4
                                 res1 = a - b4; res2 -= b4;
                             }
                             break;
                case MUL:    if (vector_tests[i].size_b == 2) {
                                 res1 = a * b2; res2 *= b2;
                             } else if (vector_tests[i].size_b == 3) {
                                 res1 = a * b3; res2 *= b3;
                             } else { // vector_tests[i].size_b == 4
                                 res1 = a * b4; res2 *= b4;
                             }
                             break;
                case DIV:    if (vector_tests[i].size_b == 2) {
                                 res1 = a / b2; res2 /= b2;
                             } else if (vector_tests[i].size_b == 3) {
                                 res1 = a / b3; res2 /= b3;
                             } else { // vector_tests[i].size_b == 4
                                 res1 = a / b4; res2 /= b4;
                             }
                             break;
                case S_ADD:  res1 = a + k; res2 += k; break;
                case S_SUB:  res1 = a - k; res2 -= k; break;
                case SCALE:  res1 = a * k; res2 *= k; break;
                case S_DIV:  res1 = a / k; res2 /= k; break;
                case LEN:    res1.x = a.length(); break;
                case LEN2:   res1.x = a.length2(); break;
                case NON_0:  res1.x = a.is_nonzero(); break;
                case LEN_IN: res1.x = a.is_length_in(k); break;
                case NORM:   res1 = a; res1.normalize(); break;
                case SETLEN: res1 = a; res1.set_length(k); break;
                case CAPLEN: res1 = a; res1.cap_length(k); break;
                case DOT:    res1.x = a.dot(b4); break;
                case LERP:   res1 = a.lerp(b4, k); break;
                case CROSS:  FAIL("test %u: CROSS(4) invalid", i); break;
                default:     FAIL("test %u: invalid type", i); break;
            }
            if (vector_tests[i].allow_approximate
             && CLOSE_ENOUGH(res1.x, vector_tests[i].res[0])
             && CLOSE_ENOUGH(res1.y, vector_tests[i].res[1])
             && CLOSE_ENOUGH(res1.z, vector_tests[i].res[2])
             && CLOSE_ENOUGH(res1.w, vector_tests[i].res[3])
             && (!have_res2
                 || (CLOSE_ENOUGH(res2.x, vector_tests[i].res[0])
                  && CLOSE_ENOUGH(res2.y, vector_tests[i].res[1])
                  && CLOSE_ENOUGH(res2.z, vector_tests[i].res[2])
                  && CLOSE_ENOUGH(res2.w, vector_tests[i].res[3]))))
            {
                continue;
            }
            if (res1.x != vector_tests[i].res[0]
             || res1.y != vector_tests[i].res[1]
             || res1.z != vector_tests[i].res[2]
             || res1.w != vector_tests[i].res[3])
            {
                FAIL("test %u: result <%f,%f,%f,%f> != expect <%f,%f,%f,%f>",
                     i, res1.x, res1.y, res1.z, res1.w,
                     vector_tests[i].res[0], vector_tests[i].res[1],
                     vector_tests[i].res[2], vector_tests[i].res[3]);
            }
            if (have_res2
                && (res2.x != vector_tests[i].res[0]
                 || res2.y != vector_tests[i].res[1]
                 || res2.z != vector_tests[i].res[2]
                 || res2.w != vector_tests[i].res[3]))
            {
                FAIL("test %u: result 2 <%f,%f,%f,%f> != expect <%f,%f,%f,%f>",
                     i, res2.x, res2.y, res2.z, res2.w,
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

TEST(test_vec4_cross)
{
    int failed = 0;

    for (int i = 0; i < lenof_vector_cross4_tests(); i++) {
        Vector4f a(vector_cross4_tests[i].a[0], vector_cross4_tests[i].a[1],
                   vector_cross4_tests[i].a[2], vector_cross4_tests[i].a[3]);
        Vector4f b(vector_cross4_tests[i].b[0], vector_cross4_tests[i].b[1],
                   vector_cross4_tests[i].b[2], vector_cross4_tests[i].b[3]);
        Vector4f c(vector_cross4_tests[i].c[0], vector_cross4_tests[i].c[1],
                   vector_cross4_tests[i].c[2], vector_cross4_tests[i].c[3]);
        Vector4f res = a.cross(b, c);
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
