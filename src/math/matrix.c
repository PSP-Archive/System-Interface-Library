/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/math/matrix.c: Matrix function definitions for generic systems.
 */

#include "src/base.h"
#include "src/math.h"

/*************************************************************************/
/****************************** Global data ******************************/
/*************************************************************************/

const Matrix4f mat4_identity = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

/* Convenience macro to take the dot product of two 4-element vectors
 * specified by their components. */
#define DOT4(x1,y1,z1,w1,x2,y2,z2,w2) \
    (((x1)*(x2)) + ((y1)*(y2)) + ((z1)*(z2)) + ((w1)*(w2)))

/*-----------------------------------------------------------------------*/

void mat4_mul(Matrix4f *dest, const Matrix4f *src1, const Matrix4f *src2)
{
    const float dest_11 = DOT4(src1->_11, src1->_12, src1->_13, src1->_14,
                               src2->_11, src2->_21, src2->_31, src2->_41);
    const float dest_12 = DOT4(src1->_11, src1->_12, src1->_13, src1->_14,
                               src2->_12, src2->_22, src2->_32, src2->_42);
    const float dest_13 = DOT4(src1->_11, src1->_12, src1->_13, src1->_14,
                               src2->_13, src2->_23, src2->_33, src2->_43);
    const float dest_14 = DOT4(src1->_11, src1->_12, src1->_13, src1->_14,
                               src2->_14, src2->_24, src2->_34, src2->_44);

    const float dest_21 = DOT4(src1->_21, src1->_22, src1->_23, src1->_24,
                               src2->_11, src2->_21, src2->_31, src2->_41);
    const float dest_22 = DOT4(src1->_21, src1->_22, src1->_23, src1->_24,
                               src2->_12, src2->_22, src2->_32, src2->_42);
    const float dest_23 = DOT4(src1->_21, src1->_22, src1->_23, src1->_24,
                               src2->_13, src2->_23, src2->_33, src2->_43);
    const float dest_24 = DOT4(src1->_21, src1->_22, src1->_23, src1->_24,
                               src2->_14, src2->_24, src2->_34, src2->_44);

    const float dest_31 = DOT4(src1->_31, src1->_32, src1->_33, src1->_34,
                               src2->_11, src2->_21, src2->_31, src2->_41);
    const float dest_32 = DOT4(src1->_31, src1->_32, src1->_33, src1->_34,
                               src2->_12, src2->_22, src2->_32, src2->_42);
    const float dest_33 = DOT4(src1->_31, src1->_32, src1->_33, src1->_34,
                               src2->_13, src2->_23, src2->_33, src2->_43);
    const float dest_34 = DOT4(src1->_31, src1->_32, src1->_33, src1->_34,
                               src2->_14, src2->_24, src2->_34, src2->_44);

    const float dest_41 = DOT4(src1->_41, src1->_42, src1->_43, src1->_44,
                               src2->_11, src2->_21, src2->_31, src2->_41);
    const float dest_42 = DOT4(src1->_41, src1->_42, src1->_43, src1->_44,
                               src2->_12, src2->_22, src2->_32, src2->_42);
    const float dest_43 = DOT4(src1->_41, src1->_42, src1->_43, src1->_44,
                               src2->_13, src2->_23, src2->_33, src2->_43);
    const float dest_44 = DOT4(src1->_41, src1->_42, src1->_43, src1->_44,
                               src2->_14, src2->_24, src2->_34, src2->_44);

    dest->_11 = dest_11;
    dest->_12 = dest_12;
    dest->_13 = dest_13;
    dest->_14 = dest_14;
    dest->_21 = dest_21;
    dest->_22 = dest_22;
    dest->_23 = dest_23;
    dest->_24 = dest_24;
    dest->_31 = dest_31;
    dest->_32 = dest_32;
    dest->_33 = dest_33;
    dest->_34 = dest_34;
    dest->_41 = dest_41;
    dest->_42 = dest_42;
    dest->_43 = dest_43;
    dest->_44 = dest_44;
}

/*-----------------------------------------------------------------------*/

float mat4_det(const Matrix4f *src)
{
    const Vector4f minor = vec4_cross(
        (Vector4f){src->_11, src->_21, src->_31, src->_41},
        (Vector4f){src->_12, src->_22, src->_32, src->_42},
        (Vector4f){src->_13, src->_23, src->_33, src->_43});
    return -DOT4(minor.x, minor.y, minor.z, minor.w,
                 src->_14, src->_24, src->_34, src->_44);
}

/*-----------------------------------------------------------------------*/

void mat4_rotate(Matrix4f *dest, const Matrix4f *src,
                 const Vector3f *axis, float angle)
{
    const Vector3f unit_axis = vec3_normalize(*axis);
    const float x = unit_axis.x;
    const float y = unit_axis.y;
    const float z = unit_axis.z;
    float s, c;
    dsincosf(angle, &s, &c);

    const Matrix4f rotator = {
        x*x*(1-c) + c,   x*y*(1-c) - z*s, x*z*(1-c) + y*s, 0,
        y*x*(1-c) + z*s, y*y*(1-c) + c,   y*z*(1-c) - x*s, 0,
        z*x*(1-c) - y*s, z*y*(1-c) + x*s, z*z*(1-c) + c,   0,
        0,               0,               0,               1};

    mat4_mul(dest, &rotator, src);
}

/*-----------------------------------------------------------------------*/

float mat4_inv(Matrix4f *dest, const Matrix4f *src)
{
    const float det = mat4_det(src);
    if (det == 0) {
        return 0;
    }

    const Vector4f cols[4] = {
        vec4_cross((Vector4f){src->_21, src->_22, src->_23, src->_24},
                   (Vector4f){src->_31, src->_32, src->_33, src->_34},
                   (Vector4f){src->_41, src->_42, src->_43, src->_44}),
        vec4_cross((Vector4f){src->_11, src->_12, src->_13, src->_14},
                   (Vector4f){src->_41, src->_42, src->_43, src->_44},
                   (Vector4f){src->_31, src->_32, src->_33, src->_34}),
        vec4_cross((Vector4f){src->_11, src->_12, src->_13, src->_14},
                   (Vector4f){src->_21, src->_22, src->_23, src->_24},
                   (Vector4f){src->_41, src->_42, src->_43, src->_44}),
        vec4_cross((Vector4f){src->_11, src->_12, src->_13, src->_14},
                   (Vector4f){src->_31, src->_32, src->_33, src->_34},
                   (Vector4f){src->_21, src->_22, src->_23, src->_24}),
    };

    dest->_11 = cols[0].x / det;
    dest->_21 = cols[0].y / det;
    dest->_31 = cols[0].z / det;
    dest->_41 = cols[0].w / det;
    dest->_12 = cols[1].x / det;
    dest->_22 = cols[1].y / det;
    dest->_32 = cols[1].z / det;
    dest->_42 = cols[1].w / det;
    dest->_13 = cols[2].x / det;
    dest->_23 = cols[2].y / det;
    dest->_33 = cols[2].z / det;
    dest->_43 = cols[2].w / det;
    dest->_14 = cols[3].x / det;
    dest->_24 = cols[3].y / det;
    dest->_34 = cols[3].z / det;
    dest->_44 = cols[3].w / det;

    return det;
}

/*************************************************************************/
/*************************************************************************/
