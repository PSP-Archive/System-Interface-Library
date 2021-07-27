/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/endian.c: Tests for endian-conversion functions.
 */

#include "src/base.h"
#include "src/endian.h"
#include "src/test/base.h"

/*************************************************************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_endian)

/*-----------------------------------------------------------------------*/

TEST(test_endian_detection)
{
    union {
        char c[4];
        uint32_t i;
    } buf = {.c = {0x12, 0x34, 0x56, 0x78}};
    CHECK_INTEQUAL(buf.i, is_little_endian() ? 0x78563412 : 0x12345678);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_s16)
{
    const int16_t s16_native  = -0x1234;
    const int16_t s16_be      =  is_little_endian() ? -0x3313 : -0x1234;
    const int16_t s16_le      = !is_little_endian() ? -0x3313 : -0x1234;

    /* We explicitly sign-extend values to the next larger integer size
     * in order to check the return value's signedness. */
    CHECK_INTEQUAL((int32_t)be_to_s16(s16_be), (int32_t)s16_native);
    CHECK_INTEQUAL((int32_t)s16_to_be(s16_native), (int32_t)s16_be);
    CHECK_INTEQUAL((int32_t)le_to_s16(s16_le), (int32_t)s16_native);
    CHECK_INTEQUAL((int32_t)s16_to_le(s16_native), (int32_t)s16_le);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_u16)
{
    const uint16_t u16_native = 0xCDEF;
    const uint16_t u16_be     =  is_little_endian() ? 0xEFCD : 0xCDEF;
    const uint16_t u16_le     = !is_little_endian() ? 0xEFCD : 0xCDEF;

    CHECK_INTEQUAL((int32_t)be_to_u16(u16_be), (int32_t)u16_native);
    CHECK_INTEQUAL((int32_t)u16_to_be(u16_native), (int32_t)u16_be);
    CHECK_INTEQUAL((int32_t)le_to_u16(u16_le), (int32_t)u16_native);
    CHECK_INTEQUAL((int32_t)u16_to_le(u16_native), (int32_t)u16_le);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_s32)
{
    const int32_t s32_native  = -0x12345678;
    const int32_t s32_be      =  is_little_endian() ? -0x77563413 : -0x12345678;
    const int32_t s32_le      = !is_little_endian() ? -0x77563413 : -0x12345678;

    CHECK_INTEQUAL((int64_t)be_to_s32(s32_be), (int64_t)s32_native);
    CHECK_INTEQUAL((int64_t)s32_to_be(s32_native), (int64_t)s32_be);
    CHECK_INTEQUAL((int64_t)le_to_s32(s32_le), (int64_t)s32_native);
    CHECK_INTEQUAL((int64_t)s32_to_le(s32_native), (int64_t)s32_le);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_u32)
{
    const uint32_t u32_native = 0x89ABCDEF;
    const uint32_t u32_be     =  is_little_endian() ? 0xEFCDAB89 : 0x89ABCDEF;
    const uint32_t u32_le     = !is_little_endian() ? 0xEFCDAB89 : 0x89ABCDEF;

    CHECK_INTEQUAL((int64_t)be_to_u32(u32_be), (int64_t)u32_native);
    CHECK_INTEQUAL((int64_t)u32_to_be(u32_native), (int64_t)u32_be);
    CHECK_INTEQUAL((int64_t)le_to_u32(u32_le), (int64_t)u32_native);
    CHECK_INTEQUAL((int64_t)u32_to_le(u32_native), (int64_t)u32_le);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_s64)
{
    const int64_t s64_native  = INT64_C(-0x123456789ABCDEF0);
    const int64_t s64_be      = (is_little_endian()
                                 ? INT64_C(0x1021436587A9CBED)
                                 : INT64_C(-0x123456789ABCDEF0));
    const int64_t s64_le      = (!is_little_endian()
                                 ? INT64_C(0x1021436587A9CBED)
                                 : INT64_C(-0x123456789ABCDEF0));

    CHECK_INTEQUAL(be_to_s64(s64_be), s64_native);
    CHECK_INTEQUAL(s64_to_be(s64_native), s64_be);
    CHECK_INTEQUAL(le_to_s64(s64_le), s64_native);
    CHECK_INTEQUAL(s64_to_le(s64_native), s64_le);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_u64)
{
    const uint64_t u64_native = UINT64_C(0x89ABCDEF01234567);
    const uint64_t u64_be     = (is_little_endian()
                                 ? UINT64_C(0x67452301EFCDAB89)
                                 : UINT64_C(0x89ABCDEF01234567));
    const uint64_t u64_le     = (!is_little_endian()
                                 ? UINT64_C(0x67452301EFCDAB89)
                                 : UINT64_C(0x89ABCDEF01234567));

    CHECK_INTEQUAL(be_to_u64(u64_be), u64_native);
    CHECK_INTEQUAL(u64_to_be(u64_native), u64_be);
    CHECK_INTEQUAL(le_to_u64(u64_le), u64_native);
    CHECK_INTEQUAL(u64_to_le(u64_native), u64_le);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_float)
{
    const float float_native = 257.0f;
    const union {uint32_t i; float f;} float_be =
        {.i = (is_little_endian() ? 0x00808043 : 0x43808000)};
    const union {uint32_t i; float f;} float_le =
        {.i = (!is_little_endian() ? 0x00808043 : 0x43808000)};
    const union {float f; uint32_t i;} float_be_test =
        {.f = float_to_be(float_native)};
    const union {float f; uint32_t i;} float_le_test =
        {.f = float_to_le(float_native)};

    CHECK_INTEQUAL(float_be_test.i, float_be.i);
    CHECK_FLOATEQUAL(be_to_float(float_be.f), float_native);
    CHECK_INTEQUAL(float_le_test.i, float_le.i);
    CHECK_FLOATEQUAL(le_to_float(float_le.f), float_native);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_double)
{
    const double double_native = 137438953473.0;
    const union {uint64_t i; double f;} double_be =
        {.i = (is_little_endian() ? UINT64_C(0x0080000000004042)
                                  : UINT64_C(0x4240000000008000))};
    const union {uint64_t i; double f;} double_le =
        {.i = (!is_little_endian() ? UINT64_C(0x0080000000004042)
                                   : UINT64_C(0x4240000000008000))};
    const union {double f; uint64_t i;} double_be_test =
        {.f = double_to_be(double_native)};
    const union {double f; uint64_t i;} double_le_test =
        {.f = double_to_le(double_native)};

    CHECK_INTEQUAL(double_be_test.i, double_be.i);
    CHECK_DOUBLEEQUAL(be_to_double(double_be.f), double_native);
    CHECK_INTEQUAL(double_le_test.i, double_le.i);
    CHECK_DOUBLEEQUAL(le_to_double(double_le.f), double_native);

    return 1;
}

/*************************************************************************/
/*************************************************************************/
