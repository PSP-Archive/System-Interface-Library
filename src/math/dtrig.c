/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/math/dtrig.c: Generic implementations of degree-unit trigonometric
 * functions.
 */

#include "src/base.h"
#include "src/math.h"

/*************************************************************************/
/*************************************************************************/

/* Trigonometric function lookup tables for each multiple of 15 degrees.
 * The values are given in double precision primarily for reference, but
 * they are of course compiled to single-precision constants. */

static const float dsinf_table[24] = {
    0.0, 0.258819045102521, 0.5,
    0.707106781186548, 0.866025403784439, 0.965925826289068,
    1.0, 0.965925826289068, 0.866025403784439,
    0.707106781186548, 0.5, 0.258819045102521,
    0.0, -0.258819045102521, -0.5,
    -0.707106781186548, -0.866025403784439, -0.965925826289068,
    -1.0, -0.965925826289068, -0.866025403784439,
    -0.707106781186548, -0.5, -0.258819045102521,
};
static const float dtanf_table[12] = {
    0.0, 0.267949192431123, 0.577350269189626,
    1.0, 1.73205080756888, 3.73205080756888,
    1.0/0.0, -3.73205080756888, -1.73205080756888,
    -1.0, -0.577350269189626, -0.267949192431123,
};

/*************************************************************************/

CONST_FUNCTION float dsinf(const float angle)
{
    const float angle_15 = roundf(angle / 15);
    if (angle == 15 * angle_15) {
        /* If this conversion would overflow a 32-bit int, the value is far
         * too large to expect any sort of accuracy in the result anyway,
         * so we use an integer modulo to avoid a potentially expensive
         * fmodf() call. */
        int32_t index = (int32_t)angle_15 % 24;  // 24 = 360/15
        if (index < 0) {
            index += 24;
        }
        return dsinf_table[index];
    }
    return sinf(angle * (M_PIf/180));
}

/*-----------------------------------------------------------------------*/

CONST_FUNCTION float dcosf(const float angle)
{
    const float angle_15 = roundf(angle / 15);
    if (angle == 15 * angle_15) {
        int32_t index = (int32_t)angle_15 % 24;
        if (index < 0) {
            index += 24;
        }
        return dsinf_table[(index+6) % 24];
    }
    return cosf(angle * (M_PIf/180));
}

/*-----------------------------------------------------------------------*/

CONST_FUNCTION float dtanf(const float angle)
{
    const float angle_15 = roundf(angle / 15);
    if (angle == 15 * angle_15) {
        int32_t index = (int32_t)angle_15 % 12;
        if (index < 0) {
            index += 12;
        }
        return dtanf_table[index];
    }
    return tanf(angle * (M_PIf/180));
}

/*-----------------------------------------------------------------------*/

void dsincosf(const float angle, float *sin_ret, float *cos_ret)
{
    const float angle_15 = roundf(angle / 15);
    if (angle == 15 * angle_15) {
        int32_t index = (int32_t)angle_15 % 24;
        if (index < 0) {
            index += 24;
        }
        *sin_ret = dsinf_table[index];
        *cos_ret = dsinf_table[(index+6) % 24];
        return;
    }
    const float sin_val = sinf(angle * (M_PIf/180));
    const float cos_val = sqrtf(1 - (sin_val*sin_val));
    *sin_ret = sin_val;
    const int test = ifloorf(fmodf(fabsf(angle), 360));
    *cos_ret = (test >= 90 && test < 270) ? -cos_val : cos_val;
}

/*-----------------------------------------------------------------------*/

CONST_FUNCTION float dasinf(const float x)
{
    for (int i = 0; i <= 12; i++) {
        if (x == dsinf_table[i+6]) {
            return 90 - i*15;
        }
    }
    return asinf(x) * (180/M_PIf);
}

/*-----------------------------------------------------------------------*/

CONST_FUNCTION float dacosf(const float x)
{
    for (int i = 0; i <= 12; i++) {
        if (x == dsinf_table[i+6]) {
            return i*15;
        }
    }
    return acosf(x) * (180/M_PIf);
}

/*-----------------------------------------------------------------------*/

CONST_FUNCTION float datan2f(const float y, const float x)
{
    if (y == 0) {
        return (x < 0) ? 180 : 0;
    } else if (x == 0) {
        return (y < 0) ? -90 : 90;
    } else if (x == y) {
        return (x < 0) ? -135 : 45;
    } else if (x == -y) {
        return (x < 0) ? 135 : -45;
    } else {
        return atan2f(y, x) * (180/M_PIf);
    }
}

/*************************************************************************/
/*************************************************************************/
