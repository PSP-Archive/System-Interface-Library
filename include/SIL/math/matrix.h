/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/math/matrix.h: Matrix type and function definitions for
 * generic systems.
 */

#ifndef SIL_MATH_MATRIX_H
#define SIL_MATH_MATRIX_H

/*
 * This file defines the Matrix4f type and associated functions (except
 * for some matrix functions which are defined separately in matrix.c).
 * The code is written so as to function properly and reasonably quickly
 * in any environment, but for that reason does not take advantage of any
 * architecture-specific features.  To take advantage of such features,
 * make a copy of this file and matrix.c with appropriate changes for the
 * architecture or compiler, then define the preprocessor symbol
 * SIL_SYSTEM_MATRIX_HEADER in the system's configuration header to point
 * to the new file, and change the build file set to include the custom
 * matrix.c instead of the one in this directory.
 */

/*************************************************************************/
/*************************************************************************/

/* Forward-declare the data types. */

#ifdef __cplusplus
struct Matrix4f;
#else
typedef struct Matrix4f Matrix4f;
#endif

/*-----------------------------------------------------------------------*/

/* Declare the structures themselves.  We only have one matrix type, so
 * forward declaration is not technically necessary, but we do so anyway
 * for parallelism with the vector code and also to simplify any future
 * additions of differently-sized matrix types. */

#define DECLARE_MATRIX_METHODS(type)                    \
    inline type(const Matrix4f &m);                     \
    static inline type identity();                      \
    inline type &operator=(const Matrix4f &m);          \
    inline type operator+() const;                      \
    inline type operator-() const;                      \
    inline type operator+(const Matrix4f &m) const;     \
    inline type operator-(const Matrix4f &m) const;     \
    inline type operator*(const Matrix4f &m) const;     \
    inline type &operator+=(const Matrix4f &m);         \
    inline type &operator-=(const Matrix4f &m);         \
    inline type &operator*=(const Matrix4f &m);         \
    inline bool operator==(const Matrix4f &m) const;    \
    inline bool operator!=(const Matrix4f &m) const;    \
    inline type transpose() const;                      \
    inline float det() const;                           \
    inline type inv() const;                            \
    inline type inv(float *det_ret) const;              \
    inline Vector2f transform(const Vector2f &v) const; \
    inline Vector3f transform(const Vector3f &v) const; \
    inline Vector4f transform(const Vector4f &v) const; \
    inline type translate(const Vector3f &v) const;     \
    inline type rotate(const Vector3f &axis, float angle) const; \
    inline type rotate_x(float angle) const;            \
    inline type rotate_y(float angle) const;            \
    inline type rotate_z(float angle) const;            \
    inline type scale(const Vector3f &v) const;         \
    inline Vector3f get_translation() const;

/*----------------------------------*/

struct Matrix4f {
    float _11, _12, _13, _14,
          _21, _22, _23, _24,
          _31, _32, _33, _34,
          _41, _42, _43, _44;

#ifdef __cplusplus
    inline Matrix4f() {}
    inline Matrix4f(float e_11, float e_12, float e_13, float e_14,
                    float e_21, float e_22, float e_23, float e_24,
                    float e_31, float e_32, float e_33, float e_34,
                    float e_41, float e_42, float e_43, float e_44)
        : _11 (e_11), _12 (e_12), _13 (e_13), _14 (e_14),
          _21 (e_21), _22 (e_22), _23 (e_23), _24 (e_24),
          _31 (e_31), _32 (e_32), _33 (e_33), _34 (e_34),
          _41 (e_41), _42 (e_42), _43 (e_43), _44 (e_44) {}
    DECLARE_MATRIX_METHODS(Matrix4f)
#endif  // __cplusplus
};

/* For C++, verify that the presence of class methods hasn't added any
 * hidden data to the class structure which would break compatibility
 * with C code. */
#ifdef __cplusplus
STATIC_ASSERT(sizeof(Matrix4f) == sizeof(float)*16, "Matrix4f is wrong size");
#endif

/*----------------------------------*/

#undef DECLARE_MATRIX_METHODS

/*************************************************************************/
/*************************************************************************/

/* Define global constants. */

/**
 * mat4_identity: The 4x4 identity matrix.
 */
extern const Matrix4f mat4_identity;

/*************************************************************************/
/*************************************************************************/

/* Define the C operator functions. */

EXTERN_C_BEGIN

/*-----------------------------------------------------------------------*/

/**
 * mat4_add, mat4_sub:  Add or subtract 4x4 matrices.
 *
 * [Parameters]
 *     dest: Pointer to matrix in which to store result.
 *     src1, src2: Pointers to matrices to add or subtract.
 */
static inline void mat4_add(Matrix4f *dest,
                            const Matrix4f *src1, const Matrix4f *src2)
{
    dest->_11 = src1->_11 + src2->_11;
    dest->_12 = src1->_12 + src2->_12;
    dest->_13 = src1->_13 + src2->_13;
    dest->_14 = src1->_14 + src2->_14;
    dest->_21 = src1->_21 + src2->_21;
    dest->_22 = src1->_22 + src2->_22;
    dest->_23 = src1->_23 + src2->_23;
    dest->_24 = src1->_24 + src2->_24;
    dest->_31 = src1->_31 + src2->_31;
    dest->_32 = src1->_32 + src2->_32;
    dest->_33 = src1->_33 + src2->_33;
    dest->_34 = src1->_34 + src2->_34;
    dest->_41 = src1->_41 + src2->_41;
    dest->_42 = src1->_42 + src2->_42;
    dest->_43 = src1->_43 + src2->_43;
    dest->_44 = src1->_44 + src2->_44;
}

static inline void mat4_sub(Matrix4f *dest,
                            const Matrix4f *src1, const Matrix4f *src2)
{
    dest->_11 = src1->_11 - src2->_11;
    dest->_12 = src1->_12 - src2->_12;
    dest->_13 = src1->_13 - src2->_13;
    dest->_14 = src1->_14 - src2->_14;
    dest->_21 = src1->_21 - src2->_21;
    dest->_22 = src1->_22 - src2->_22;
    dest->_23 = src1->_23 - src2->_23;
    dest->_24 = src1->_24 - src2->_24;
    dest->_31 = src1->_31 - src2->_31;
    dest->_32 = src1->_32 - src2->_32;
    dest->_33 = src1->_33 - src2->_33;
    dest->_34 = src1->_34 - src2->_34;
    dest->_41 = src1->_41 - src2->_41;
    dest->_42 = src1->_42 - src2->_42;
    dest->_43 = src1->_43 - src2->_43;
    dest->_44 = src1->_44 - src2->_44;
}

/*-----------------------------------------------------------------------*/

/**
 * mat4_mul:  Multiply 4x4 matrices.
 *
 * [Parameters]
 *     dest: Pointer to matrix in which to store result.
 *     src1, src2: Pointers to matrices to multiply.
 */
extern void mat4_mul(Matrix4f *dest,
                     const Matrix4f *src1, const Matrix4f *src2);

/*-----------------------------------------------------------------------*/

/**
 * mat4_transpose:  Transpose a 4x4 matrix.
 *
 * [Parameters]
 *     dest: Pointer to matrix in which to store result.
 *     src: Pointer to matrix to transpose.
 */
static inline void mat4_transpose(Matrix4f *dest, const Matrix4f *src)
{
    const float saved_12 = src->_12;
    const float saved_13 = src->_13;
    const float saved_14 = src->_14;
    const float saved_23 = src->_23;
    const float saved_24 = src->_24;
    const float saved_34 = src->_34;

    dest->_11 = src->_11;
    dest->_12 = src->_21;
    dest->_13 = src->_31;
    dest->_14 = src->_41;
    dest->_21 = saved_12;
    dest->_22 = src->_22;
    dest->_23 = src->_32;
    dest->_24 = src->_42;
    dest->_31 = saved_13;
    dest->_32 = saved_23;
    dest->_33 = src->_33;
    dest->_34 = src->_43;
    dest->_41 = saved_14;
    dest->_42 = saved_24;
    dest->_43 = saved_34;
    dest->_44 = src->_44;
}

/*-----------------------------------------------------------------------*/

/**
 * mat4_det:  Find the determinant of a 4x4 matrix.
 *
 * [Parameters]
 *     src: Pointer to matrix to calculate the determinant of.
 * [Return value]
 *     |src|
 */
extern PURE_FUNCTION float mat4_det(const Matrix4f *src);

/*-----------------------------------------------------------------------*/

/**
 * mat4_inv:  Calculate the inverse of a 4x4 matrix.  If the source matrix
 * is not invertible (has a determinant of zero), the destination matrix is
 * not modified.
 *
 * [Parameters]
 *     dest: Pointer to matrix in which to store result.
 *     src: Pointer to matrix to calculate the inverse of.
 * [Return value]
 *     |src|
 */
extern float mat4_inv(Matrix4f *dest, const Matrix4f *src);

/*-----------------------------------------------------------------------*/

/**
 * mat4_vec[234]_transform:  Transform a 2-, 3-, or 4-element vector by a
 * 4x4 matrix.  The vec2 version assumes z = 0; the vec2 and vec3 versions
 * assume w = 1.
 *
 * [Parameters]
 *     dest: Pointer to vector in which to store result.
 *     src: Pointer to vector to transform.
 *     m: Pointer to transformation matrix.
 */
static inline void mat4_vec2_transform(Vector2f *dest, const Vector2f *src,
                                       const Matrix4f *m)
{
    const float x = src->x * m->_11
                  + src->y * m->_21
                  +          m->_41;
    const float y = src->x * m->_12
                  + src->y * m->_22
                  +          m->_42;
    dest->x = x;
    dest->y = y;
}

static inline void mat4_vec3_transform(Vector3f *dest, const Vector3f *src,
                                       const Matrix4f *m)
{
    const float x = src->x * m->_11
                  + src->y * m->_21
                  + src->z * m->_31
                  +          m->_41;
    const float y = src->x * m->_12
                  + src->y * m->_22
                  + src->z * m->_32
                  +          m->_42;
    const float z = src->x * m->_13
                  + src->y * m->_23
                  + src->z * m->_33
                  +          m->_43;
    dest->x = x;
    dest->y = y;
    dest->z = z;
}

static inline void mat4_vec4_transform(Vector4f *dest, const Vector4f *src,
                                       const Matrix4f *m)
{
    const float x = src->x * m->_11
                  + src->y * m->_21
                  + src->z * m->_31
                  + src->w * m->_41;
    const float y = src->x * m->_12
                  + src->y * m->_22
                  + src->z * m->_32
                  + src->w * m->_42;
    const float z = src->x * m->_13
                  + src->y * m->_23
                  + src->z * m->_33
                  + src->w * m->_43;
    const float w = src->x * m->_14
                  + src->y * m->_24
                  + src->z * m->_34
                  + src->w * m->_44;
    dest->x = x;
    dest->y = y;
    dest->z = z;
    dest->w = w;
}

/*-----------------------------------------------------------------------*/

/**
 * mat4_translate:  Given a coordinate transformation matrix, apply a
 * translation by the given vector and return the resulting matrix.
 *
 * Ignoring rounding error, the following assertion holds for any v and m:
 *     Vector3f v2;
 *     mat4_vec3_transform(&v2, &v, &m);
 *     Matrix4f m2;
 *     mat4_translate(&m2, &m, &v);
 *     ASSERT(m2._41 == v2.x && m2._42 == v2.y && m2._43 == v2.z);
 *
 * [Parameters]
 *     dest: Pointer to matrix in which to store result.
 *     src: Pointer to input matrix.
 *     v: Pointer to vector to translate by.
 */
static inline void mat4_translate(Matrix4f *dest, const Matrix4f *src,
                                  const Vector3f *v)
{
    dest->_11 = src->_11;
    dest->_12 = src->_12;
    dest->_13 = src->_13;
    dest->_14 = src->_14;
    dest->_21 = src->_21;
    dest->_22 = src->_22;
    dest->_23 = src->_23;
    dest->_24 = src->_24;
    dest->_31 = src->_31;
    dest->_32 = src->_32;
    dest->_33 = src->_33;
    dest->_34 = src->_34;
    dest->_41 = src->_11*v->x + src->_21*v->y + src->_31*v->z + src->_41;
    dest->_42 = src->_12*v->x + src->_22*v->y + src->_32*v->z + src->_42;
    dest->_43 = src->_13*v->x + src->_23*v->y + src->_33*v->z + src->_43;
    dest->_44 = src->_14*v->x + src->_24*v->y + src->_34*v->z + src->_44;
}

/*-----------------------------------------------------------------------*/

/**
 * mat4_rotate:  Given a coordinate transformation matrix, apply a rotation
 * around the given axis by the given angle and return the resulting matrix.
 * The rotation follows the right-hand rule, so if the axis points out of
 * the display plane toward the viewer, the rotation will be counterclockwise.
 *
 * [Parameters]
 *     dest: Pointer to matrix in which to store result.
 *     src: Pointer to input matrix.
 *     axis: Pointer to axis vector (must be nonzero).
 *     angle: Angle of rotation, in degrees.
 */
extern void mat4_rotate(Matrix4f *dest, const Matrix4f *src,
                        const Vector3f *axis, float angle);

/**
 * mat4_rotate_x:  Given a coordinate transformation matrix, apply a rotation
 * around the X axis by the given angle and return the resulting matrix.
 * Specialization of mat4_rotate() for axis = {1,0,0}.
 *
 * [Parameters]
 *     dest: Pointer to matrix in which to store result.
 *     src: Pointer to input matrix.
 *     angle: Angle of rotation, in degrees.
 */
static inline void mat4_rotate_x(Matrix4f *dest, const Matrix4f *src,
                                 float angle)
{
    float s, c;
    dsincosf(angle, &s, &c);
    const float result_31 = s*src->_21 + c*src->_31;
    const float result_32 = s*src->_22 + c*src->_32;
    const float result_33 = s*src->_23 + c*src->_33;
    const float result_34 = s*src->_24 + c*src->_34;
    dest->_11 = src->_11;
    dest->_12 = src->_12;
    dest->_13 = src->_13;
    dest->_14 = src->_14;
    dest->_21 = c*src->_21 - s*src->_31;
    dest->_22 = c*src->_22 - s*src->_32;
    dest->_23 = c*src->_23 - s*src->_33;
    dest->_24 = c*src->_24 - s*src->_34;
    dest->_31 = result_31;
    dest->_32 = result_32;
    dest->_33 = result_33;
    dest->_34 = result_34;
    dest->_41 = src->_41;
    dest->_42 = src->_42;
    dest->_43 = src->_43;
    dest->_44 = src->_44;
}

/**
 * mat4_rotate_y:  Given a coordinate transformation matrix, apply a rotation
 * around the Y axis by the given angle and return the resulting matrix.
 * Specialization of mat4_rotate() for axis = {0,1,0}.
 *
 * [Parameters]
 *     dest: Pointer to matrix in which to store result.
 *     src: Pointer to input matrix.
 *     angle: Angle of rotation, in degrees.
 */
static inline void mat4_rotate_y(Matrix4f *dest, const Matrix4f *src,
                                 float angle)
{
    float s, c;
    dsincosf(angle, &s, &c);
    const float result_31 = c*src->_31 - s*src->_11;
    const float result_32 = c*src->_32 - s*src->_12;
    const float result_33 = c*src->_33 - s*src->_13;
    const float result_34 = c*src->_34 - s*src->_14;
    dest->_11 = c*src->_11 + s*src->_31;
    dest->_12 = c*src->_12 + s*src->_32;
    dest->_13 = c*src->_13 + s*src->_33;
    dest->_14 = c*src->_14 + s*src->_34;
    dest->_21 = src->_21;
    dest->_22 = src->_22;
    dest->_23 = src->_23;
    dest->_24 = src->_24;
    dest->_31 = result_31;
    dest->_32 = result_32;
    dest->_33 = result_33;
    dest->_34 = result_34;
    dest->_41 = src->_41;
    dest->_42 = src->_42;
    dest->_43 = src->_43;
    dest->_44 = src->_44;
}

/**
 * mat4_rotate_z:  Given a coordinate transformation matrix, apply a rotation
 * around the Z axis by the given angle and return the resulting matrix.
 * Specialization of mat4_rotate() for axis = {0,0,1}.
 *
 * [Parameters]
 *     dest: Pointer to matrix in which to store result.
 *     src: Pointer to input matrix.
 *     angle: Angle of rotation, in degrees.
 */
static inline void mat4_rotate_z(Matrix4f *dest, const Matrix4f *src,
                                 float angle)
{
    float s, c;
    dsincosf(angle, &s, &c);
    const float result_21 = s*src->_11 + c*src->_21;
    const float result_22 = s*src->_12 + c*src->_22;
    const float result_23 = s*src->_13 + c*src->_23;
    const float result_24 = s*src->_14 + c*src->_24;
    dest->_11 = c*src->_11 - s*src->_21;
    dest->_12 = c*src->_12 - s*src->_22;
    dest->_13 = c*src->_13 - s*src->_23;
    dest->_14 = c*src->_14 - s*src->_24;
    dest->_21 = result_21;
    dest->_22 = result_22;
    dest->_23 = result_23;
    dest->_24 = result_24;
    dest->_31 = src->_31;
    dest->_32 = src->_32;
    dest->_33 = src->_33;
    dest->_34 = src->_34;
    dest->_41 = src->_41;
    dest->_42 = src->_42;
    dest->_43 = src->_43;
    dest->_44 = src->_44;
}

/*-----------------------------------------------------------------------*/

/**
 * mat4_scale:  Given a coordinate transformation matrix, apply a scaling
 * operation by the given vector and return the resulting matrix.
 *
 * [Parameters]
 *     dest: Pointer to matrix in which to store result.
 *     src: Pointer to input matrix.
 *     v: Pointer to vector containing scale factors along each axis.
 */
static inline void mat4_scale(Matrix4f *dest, const Matrix4f *src,
                              const Vector3f *v)
{
    dest->_11 = src->_11 * v->x;
    dest->_12 = src->_12 * v->x;
    dest->_13 = src->_13 * v->x;
    dest->_14 = src->_14 * v->x;
    dest->_21 = src->_21 * v->y;
    dest->_22 = src->_22 * v->y;
    dest->_23 = src->_23 * v->y;
    dest->_24 = src->_24 * v->y;
    dest->_31 = src->_31 * v->z;
    dest->_32 = src->_32 * v->z;
    dest->_33 = src->_33 * v->z;
    dest->_34 = src->_34 * v->z;
    dest->_41 = src->_41;
    dest->_42 = src->_42;
    dest->_43 = src->_43;
    dest->_44 = src->_44;
}

/*-----------------------------------------------------------------------*/

/**
 * mat4_get_translation:  Return the translation applied by the given
 * coordinate transformation matrix.  Equivalent to mat4_vec3_transform()
 * on a source coordinate of {0,0,0}.
 *
 * [Parameters]
 *     m: Pointer to transformation matrix.
 * [Return value]
 *     Translation applied by matrix.
 */
static inline Vector3f mat4_get_translation(const Matrix4f *m)
{
    return (Vector3f){m->_41, m->_42, m->_43};
}

/*-----------------------------------------------------------------------*/

EXTERN_C_END

/*************************************************************************/
/*************************************************************************/

/* Define the C++ operator functions. */

#ifdef __cplusplus

/*-----------------------------------------------------------------------*/

Matrix4f::Matrix4f(const Matrix4f &m)
    : _11(m._11), _12(m._12), _13(m._13), _14(m._14),
      _21(m._21), _22(m._22), _23(m._23), _24(m._24),
      _31(m._31), _32(m._32), _33(m._33), _34(m._34),
      _41(m._41), _42(m._42), _43(m._43), _44(m._44) {}

/*static*/ Matrix4f Matrix4f::identity()
    {return Matrix4f(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1);}

Matrix4f &Matrix4f::operator=(const Matrix4f &m)
    {_11 = m._11; _12 = m._12; _13 = m._13; _14 = m._14;
     _21 = m._21; _22 = m._22; _23 = m._23; _24 = m._24;
     _31 = m._31; _32 = m._32; _33 = m._33; _34 = m._34;
     _41 = m._41; _42 = m._42; _43 = m._43; _44 = m._44; return *this;}

Matrix4f Matrix4f::operator+() const
    {return *this;}

Matrix4f Matrix4f::operator-() const
    {return Matrix4f(-_11, -_12, -_13, -_14,
                     -_21, -_22, -_23, -_24,
                     -_31, -_32, -_33, -_34,
                     -_41, -_42, -_43, -_44);}

Matrix4f Matrix4f::operator+(const Matrix4f &m) const
    {return Matrix4f(_11 + m._11, _12 + m._12, _13 + m._13, _14 + m._14,
                     _21 + m._21, _22 + m._22, _23 + m._23, _24 + m._24,
                     _31 + m._31, _32 + m._32, _33 + m._33, _34 + m._34,
                     _41 + m._41, _42 + m._42, _43 + m._43, _44 + m._44);}

Matrix4f Matrix4f::operator-(const Matrix4f &m) const
    {return Matrix4f(_11 - m._11, _12 - m._12, _13 - m._13, _14 - m._14,
                     _21 - m._21, _22 - m._22, _23 - m._23, _24 - m._24,
                     _31 - m._31, _32 - m._32, _33 - m._33, _34 - m._34,
                     _41 - m._41, _42 - m._42, _43 - m._43, _44 - m._44);}

Matrix4f Matrix4f::operator*(const Matrix4f &m) const
    {Matrix4f res; mat4_mul(&res, this, &m); return res;}

Matrix4f &Matrix4f::operator+=(const Matrix4f &m)
    {_11 += m._11; _12 += m._12; _13 += m._13; _14 += m._14;
     _21 += m._21; _22 += m._22; _23 += m._23; _24 += m._24;
     _31 += m._31; _32 += m._32; _33 += m._33; _34 += m._34;
     _41 += m._41; _42 += m._42; _43 += m._43; _44 += m._44; return *this;}

Matrix4f &Matrix4f::operator-=(const Matrix4f &m)
    {_11 -= m._11; _12 -= m._12; _13 -= m._13; _14 -= m._14;
     _21 -= m._21; _22 -= m._22; _23 -= m._23; _24 -= m._24;
     _31 -= m._31; _32 -= m._32; _33 -= m._33; _34 -= m._34;
     _41 -= m._41; _42 -= m._42; _43 -= m._43; _44 -= m._44; return *this;}

Matrix4f &Matrix4f::operator*=(const Matrix4f &m)
    {mat4_mul(this, this, &m); return *this;}

bool Matrix4f::operator==(const Matrix4f &m) const
    {return _11 == m._11 && _12 == m._12 && _13 == m._13 && _14 == m._14
         && _21 == m._21 && _22 == m._22 && _23 == m._23 && _24 == m._24
         && _31 == m._31 && _32 == m._32 && _33 == m._33 && _34 == m._34
         && _41 == m._41 && _42 == m._42 && _43 == m._43 && _44 == m._44;}

bool Matrix4f::operator!=(const Matrix4f &m) const
    {return _11 != m._11 || _12 != m._12 || _13 != m._13 || _14 != m._14
         || _21 != m._21 || _22 != m._22 || _23 != m._23 || _24 != m._24
         || _31 != m._31 || _32 != m._32 || _33 != m._33 || _34 != m._34
         || _41 != m._41 || _42 != m._42 || _43 != m._43 || _44 != m._44;}

Matrix4f Matrix4f::transpose() const
    {return Matrix4f(_11, _21, _31, _41,
                     _12, _22, _32, _42,
                     _13, _23, _33, _43,
                     _14, _24, _34, _44);}

float Matrix4f::det() const
    {return mat4_det(this);}
Matrix4f Matrix4f::inv() const
    {Matrix4f res = *this; mat4_inv(&res, this); return res;}
Matrix4f Matrix4f::inv(float *det_ret) const
    {Matrix4f res = *this; *det_ret = mat4_inv(&res, this); return res;}

Vector2f Matrix4f::transform(const Vector2f &v) const
    {return Vector2f(v.x*_11 + v.y*_21 + _41,
                     v.x*_12 + v.y*_22 + _42);}
Vector3f Matrix4f::transform(const Vector3f &v) const
    {return Vector3f(v.x*_11 + v.y*_21 + v.z*_31 + _41,
                     v.x*_12 + v.y*_22 + v.z*_32 + _42,
                     v.x*_13 + v.y*_23 + v.z*_33 + _43);}
Vector4f Matrix4f::transform(const Vector4f &v) const
    {return Vector4f(v.x*_11 + v.y*_21 + v.z*_31 + v.w*_41,
                     v.x*_12 + v.y*_22 + v.z*_32 + v.w*_42,
                     v.x*_13 + v.y*_23 + v.z*_33 + v.w*_43,
                     v.x*_14 + v.y*_24 + v.z*_34 + v.w*_44);}

Matrix4f Matrix4f::translate(const Vector3f &v) const
    {return Matrix4f(_11, _12, _13, _14,
                     _21, _22, _23, _24,
                     _31, _32, _33, _34,
                     _11*v.x + _21*v.y + _31*v.z + _41,
                         _12*v.x + _22*v.y + _32*v.z + _42,
                         _13*v.x + _23*v.y + _33*v.z + _43,
                         _14*v.x + _24*v.y + _34*v.z + _44);}
Matrix4f Matrix4f::rotate(const Vector3f &axis, float angle) const
    {Matrix4f res; mat4_rotate(&res, this, &axis, angle); return res;}
Matrix4f Matrix4f::rotate_x(float angle) const
    {float s, c;
     dsincosf(angle, &s, &c);
     return Matrix4f(_11,           _12,           _13,           _14,
                     _21*c - _31*s, _22*c - _32*s, _23*c - _33*s, _24*c - _34*s,
                     _21*s + _31*c, _22*s + _32*c, _23*s + _33*c, _24*s + _34*c,
                     _41,           _42,           _43,           _44);}
Matrix4f Matrix4f::rotate_y(float angle) const
    {float s, c;
     dsincosf(angle, &s, &c);
     return Matrix4f(_11*c + _31*s, _12*c + _32*s, _13*c + _33*s, _14*c + _34*s,
                     _21,           _22,           _23,           _24,
                     _31*c - _11*s, _32*c - _12*s, _33*c - _13*s, _34*c - _14*s,
                     _41,           _42,           _43,           _44);}
Matrix4f Matrix4f::rotate_z(float angle) const
    {float s, c;
     dsincosf(angle, &s, &c);
     return Matrix4f(_11*c - _21*s, _12*c - _22*s, _13*c - _23*s, _14*c - _24*s,
                     _11*s + _21*c, _12*s + _22*c, _13*s + _23*c, _14*s + _24*c,
                     _31,           _32,           _33,           _34,
                     _41,           _42,           _43,           _44);}
Matrix4f Matrix4f::scale(const Vector3f &v) const
    {return Matrix4f(_11 * v.x, _12 * v.x, _13 * v.x, _14 * v.x,
                     _21 * v.y, _22 * v.y, _23 * v.y, _24 * v.y,
                     _31 * v.z, _32 * v.z, _33 * v.z, _34 * v.z,
                     _41,       _42,       _43,       _44);}

Vector3f Matrix4f::get_translation() const
    {return Vector3f(_41, _42, _43);}

/*-----------------------------------------------------------------------*/

#endif  // __cplusplus

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_MATH_MATRIX_H
