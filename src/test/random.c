/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/random.c: Tests for the random number generation functions.
 */

#include "src/base.h"
#include "src/random.h"
#include "src/test/base.h"

/*************************************************************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_random)

/*-----------------------------------------------------------------------*/

TEST(test_initial_seed)
{
    CHECK_INTEQUAL(urandom64(), 0x5851F42D4C957F2EULL);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_srandom32)
{
    srandom32(1);
    CHECK_INTEQUAL(urandom64(), 0x5851F42D4C957F2EULL);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_srandom64)
{
    srandom64(1);
    CHECK_INTEQUAL(urandom64(), 0x5851F42D4C957F2EULL);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_srandom_env)
{
    srandom64(1);
    srandom_env();
    CHECK_TRUE(urandom64() != 0x5851F42D4C957F2EULL);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_random32)
{
    srandom64(1);
    CHECK_INTEQUAL(random32(), 0x2C28FA16);
    CHECK_TRUE(random32() != 0x2C28FA16);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_random32_r)
{
    uint64_t state = 1;
    CHECK_INTEQUAL(random32_r(&state), 0x2C28FA16);
    CHECK_INTEQUAL(state, 0x5851F42D4C957F2EULL);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_urandom32)
{
    srandom64(1);
    CHECK_INTEQUAL(urandom32(), 0x5851F42D);
    CHECK_TRUE(urandom32() != 0x5851F42D);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_urandom32_r)
{
    uint64_t state = 1;
    CHECK_INTEQUAL(urandom32_r(&state), 0x5851F42D);
    CHECK_INTEQUAL(state, 0x5851F42D4C957F2EULL);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_random64)
{
    srandom64(1);
    CHECK_INTEQUAL(random64(), 0x2C28FA16A64ABF97LL);
    CHECK_TRUE(random64() != 0x2C28FA16A64ABF97LL);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_random64_r)
{
    uint64_t state = 1;
    CHECK_INTEQUAL(random64_r(&state), 0x2C28FA16A64ABF97LL);
    CHECK_INTEQUAL(state, 0x5851F42D4C957F2EULL);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_urandom64)
{
    srandom64(1);
    CHECK_INTEQUAL(urandom64(), 0x5851F42D4C957F2EULL);
    CHECK_TRUE(urandom64() != 0x5851F42D4C957F2EULL);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_urandom64_r)
{
    uint64_t state = 1;
    CHECK_INTEQUAL(urandom64_r(&state), 0x5851F42D4C957F2EULL);
    CHECK_INTEQUAL(state, 0x5851F42D4C957F2EULL);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* Test the invariants described in the urandom64_r() documentation. */
TEST(test_urandom64_invariants)
{
    uint64_t state = 1;
    uint64_t value = urandom64_r(&state);
    CHECK_INTEQUAL(value, state);

    uint64_t seed = urandom64();
    uint64_t value1 = urandom64();
    srandom64(seed);
    uint64_t value2 = urandom64();
    CHECK_INTEQUAL(value2, value1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_frandom)
{
    srandom64(1);
    CHECK_DOUBLEEQUAL(frandom(), 0.34500051599441928);
    CHECK_TRUE(frandom() != 0.34500051599441928);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_frandom_r)
{
    uint64_t state = 1;
    CHECK_DOUBLEEQUAL(frandom_r(&state), 0.34500051599441928);
    CHECK_INTEQUAL(state, 0x5851F42D4C957F2EULL);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_frandomf)
{
    srandom64(1);
    CHECK_FLOATEQUAL(frandomf(), 0.34500051f);
    CHECK_TRUE(frandomf() != 0.34500051f);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_frandomf_r)
{
    uint64_t state = 1;
    CHECK_FLOATEQUAL(frandomf_r(&state), 0.34500051f);
    CHECK_INTEQUAL(state, 0x5851F42D4C957F2EULL);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_frandom_range)
{
    /* We pick an initial state such that applying the RNG function
     * (6364136223846793005*x + 1) to the state value results in a new
     * state value of 0xFFFF'FFFF'FFFF'FFFF (the maximum possible value).
     * If frandom() then rounds that value up when converting to floating
     * point, the result will be 1.0, outside the declared bounds of the
     * return value. */
    uint64_t state = 9137839865990459062;
    CHECK_TRUE(frandom_r(&state) < 1.0);
    CHECK_INTEQUAL(state, -1ULL);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_frandomf_range)
{
    uint64_t state = 9137839865990459062;  // As in test_frandom_range.
    CHECK_TRUE(frandomf_r(&state) < 1.0);
    CHECK_INTEQUAL(state, -1ULL);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_randlimit)
{
    srandom_env();
    const uint32_t seed = urandom32();
    DLOG("Random seed: %u", seed);
    srandom32(seed);

    for (int32_t i = 1; i <= 65536; i++) {
        CHECK_TRUE(randlimit(i) < i);
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_randrange)
{
    srandom_env();
    const uint32_t seed = urandom32();
    DLOG("Random seed: %u", seed);
    srandom32(seed);

    for (int32_t i = 1; i <= 65536; i++) {
        const int32_t x = randrange(i/2, i);
        CHECK_TRUE(x >= i/2);
        CHECK_TRUE(x < i);
    }
    return 1;
}

/*************************************************************************/
/*************************************************************************/
