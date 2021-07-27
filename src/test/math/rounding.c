/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/math/rounding.c: Tests for SIL-specific rounding functions.
 */

#include "src/base.h"
#include "src/math.h"
#include "src/test/base.h"
#include "src/test/math/internal.h"

/*************************************************************************/
/*************************** Function wrappers ***************************/
/*************************************************************************/

/*
 * These functions and corresponding macros wrap the (inline) rounding
 * functions defined in <SIL/math.h> with a volatile variable load/store,
 * ensuring that the functions are actually called rather than being
 * optimized out at compile time.
 */

#define WRAP(func, type, rettype) \
    static inline rettype wrap_##func(type x) { \
        volatile type v = x; \
        return func(v); \
    }

WRAP(frac, double, double)
WRAP(fracf, float, float)
WRAP(iceil, double, int)
WRAP(iceilf, float, int)
WRAP(ifloor, double, int)
WRAP(ifloorf, float, int)
WRAP(iround, double, int)
WRAP(iroundf, float, int)
WRAP(itrunc, double, int)
WRAP(itruncf, float, int)

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_MATH_TEST_RUNNER(test_math_rounding)

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_wrap_frac)
{
    CHECK_DOUBLEEQUAL(wrap_frac(2147483645.25), 0.25);
    CHECK_DOUBLEEQUAL(wrap_frac(2147483645.625), 0.625);
    CHECK_DOUBLEEQUAL(wrap_frac(-2147483645.25), 0.75);
    CHECK_DOUBLEEQUAL(wrap_frac(-2147483645.625), 0.375);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_wrap_fracf)
{
    CHECK_FLOATEQUAL(wrap_fracf(1.25), 0.25);
    CHECK_FLOATEQUAL(wrap_fracf(1.625), 0.625);
    CHECK_FLOATEQUAL(wrap_fracf(-1.25), 0.75);
    CHECK_FLOATEQUAL(wrap_fracf(-1.625), 0.375);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_wrap_iceil)
{
    CHECK_INTEQUAL(wrap_iceil(2147483645.25), 2147483646);
    CHECK_INTEQUAL(wrap_iceil(2147483645.75), 2147483646);
    CHECK_INTEQUAL(wrap_iceil(-2147483645.25), -2147483645);
    CHECK_INTEQUAL(wrap_iceil(-2147483645.75), -2147483645);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_wrap_iceilf)
{
    CHECK_INTEQUAL(wrap_iceilf(1.25), 2);
    CHECK_INTEQUAL(wrap_iceilf(1.75), 2);
    CHECK_INTEQUAL(wrap_iceilf(-1.25), -1);
    CHECK_INTEQUAL(wrap_iceilf(-1.75), -1);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_wrap_ifloor)
{
    CHECK_INTEQUAL(wrap_ifloor(2147483645.25), 2147483645);
    CHECK_INTEQUAL(wrap_ifloor(2147483645.75), 2147483645);
    CHECK_INTEQUAL(wrap_ifloor(-2147483645.25), -2147483646);
    CHECK_INTEQUAL(wrap_ifloor(-2147483645.75), -2147483646);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_wrap_ifloorf)
{
    CHECK_INTEQUAL(wrap_ifloorf(1.25), 1);
    CHECK_INTEQUAL(wrap_ifloorf(1.75), 1);
    CHECK_INTEQUAL(wrap_ifloorf(-1.25), -2);
    CHECK_INTEQUAL(wrap_ifloorf(-1.75), -2);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_wrap_iround)
{
    CHECK_INTEQUAL(wrap_iround(2147483645.25), 2147483645);
    CHECK_INTEQUAL(wrap_iround(2147483645.75), 2147483646);
    CHECK_INTEQUAL(wrap_iround(-2147483645.25), -2147483645);
    CHECK_INTEQUAL(wrap_iround(-2147483645.75), -2147483646);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_wrap_iroundf)
{
    CHECK_INTEQUAL(wrap_iroundf(1.25), 1);
    CHECK_INTEQUAL(wrap_iroundf(1.75), 2);
    CHECK_INTEQUAL(wrap_iroundf(-1.25), -1);
    CHECK_INTEQUAL(wrap_iroundf(-1.75), -2);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_wrap_itrunc)
{
    CHECK_INTEQUAL(wrap_itrunc(2147483645.25), 2147483645);
    CHECK_INTEQUAL(wrap_itrunc(2147483645.75), 2147483645);
    CHECK_INTEQUAL(wrap_itrunc(-2147483645.25), -2147483645);
    CHECK_INTEQUAL(wrap_itrunc(-2147483645.75), -2147483645);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_wrap_itruncf)
{
    CHECK_INTEQUAL(wrap_itruncf(1.25), 1);
    CHECK_INTEQUAL(wrap_itruncf(1.75), 1);
    CHECK_INTEQUAL(wrap_itruncf(-1.25), -1);
    CHECK_INTEQUAL(wrap_itruncf(-1.75), -1);
    return 1;
}

/*************************************************************************/
/*************************************************************************/
