/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/math/vector.h: Vector type and function definitions for
 * generic systems.
 */

#ifndef SIL_MATH_VECTOR_H
#define SIL_MATH_VECTOR_H

/*
 * This file defines the Vector2f, Vector3f, and Vector4f types and
 * associated functions (all inline).  The code is written so as to
 * function properly and reasonably quickly in any environment, but for
 * that reason does not take advantage of any architecture-specific
 * features.  To take advantage of architectures or compilers with support
 * for faster vector operations, make a copy of this file with appropriate
 * changes for the architecture or compiler, then define the preprocessor
 * symbol SIL_SYSTEM_VECTOR_HEADER in the system's configuration header
 * to point to the new file.
 */

/*************************************************************************/
/*************************************************************************/

/* Forward-declare the data types. */

#ifdef __cplusplus
struct Vector2f;
struct Vector3f;
struct Vector4f;
#else
typedef struct Vector2f Vector2f;
typedef struct Vector3f Vector3f;
typedef struct Vector4f Vector4f;
#endif

/*-----------------------------------------------------------------------*/

/* Declare the structures themselves.  The C++ methods will need to be
 * defined later because we need all three types defined first. */

#define DECLARE_VECTOR_METHODS(type)                    \
    inline type(const Vector2f &v);                     \
    inline type(const Vector3f &v);                     \
    inline type(const Vector4f &v);                     \
                                                        \
    inline type &operator=(const float k);              \
    inline type &operator=(const Vector2f &v);          \
    inline type &operator=(const Vector3f &v);          \
    inline type &operator=(const Vector4f &v);          \
                                                        \
    inline type operator+() const;                      \
    inline type operator-() const;                      \
                                                        \
    inline type operator+(const float k) const;         \
    inline type operator+(const Vector2f &v) const;     \
    inline type operator+(const Vector3f &v) const;     \
    inline type operator+(const Vector4f &v) const;     \
                                                        \
    inline type operator-(const float k) const;         \
    inline type operator-(const Vector2f &v) const;     \
    inline type operator-(const Vector3f &v) const;     \
    inline type operator-(const Vector4f &v) const;     \
                                                        \
    inline type operator*(const float k) const;         \
    inline type operator*(const Vector2f &v) const;     \
    inline type operator*(const Vector3f &v) const;     \
    inline type operator*(const Vector4f &v) const;     \
                                                        \
    inline type operator/(const float k) const;         \
    inline type operator/(const Vector2f &v) const;     \
    inline type operator/(const Vector3f &v) const;     \
    inline type operator/(const Vector4f &v) const;     \
                                                        \
    friend inline type operator+(const float k, const type &v); \
    friend inline type operator*(const float k, const type &v); \
                                                        \
    inline type &operator+=(const float k);             \
    inline type &operator+=(const Vector2f &v);         \
    inline type &operator+=(const Vector3f &v);         \
    inline type &operator+=(const Vector4f &v);         \
                                                        \
    inline type &operator-=(const float k);             \
    inline type &operator-=(const Vector2f &v);         \
    inline type &operator-=(const Vector3f &v);         \
    inline type &operator-=(const Vector4f &v);         \
                                                        \
    inline type &operator*=(const float k);             \
    inline type &operator*=(const Vector2f &v);         \
    inline type &operator*=(const Vector3f &v);         \
    inline type &operator*=(const Vector4f &v);         \
                                                        \
    inline type &operator/=(const float k);             \
    inline type &operator/=(const Vector2f &v);         \
    inline type &operator/=(const Vector3f &v);         \
    inline type &operator/=(const Vector4f &v);         \
                                                        \
    inline bool operator==(const type &v) const;        \
    inline bool operator!=(const type &v) const;        \
                                                        \
    inline float length() const;                        \
    inline float length2() const;                       \
    inline bool is_nonzero() const;                     \
    inline bool is_length_in(const float k) const;      \
    inline void normalize();                            \
    inline void set_length(const float k);              \
    inline void cap_length(const float k);              \
    inline float dot(const type &v) const;              \
    inline type lerp(const type &v, const float k) const; \

/*----------------------------------*/

struct Vector2f {
    float x, y;

#ifdef __cplusplus
    inline Vector2f(float _x = 0, float _y = 0)
        : x(_x), y(_y) {}
    DECLARE_VECTOR_METHODS(Vector2f)
#endif
};

struct Vector3f {
    float x, y, z;

#ifdef __cplusplus
    inline Vector3f(float _x = 0, float _y = 0, float _z = 0)
        : x(_x), y(_y), z(_z) {}
    DECLARE_VECTOR_METHODS(Vector3f)
    inline Vector3f cross(const Vector3f &v) const;
#endif
};

struct Vector4f {
    float x, y, z, w;

#ifdef __cplusplus
    inline Vector4f(float _x = 0, float _y = 0, float _z = 0, float _w = 0)
        : x(_x), y(_y), z(_z), w(_w) {}
    DECLARE_VECTOR_METHODS(Vector4f)
    inline Vector4f cross(const Vector4f &v2, const Vector4f &v3) const;
#endif  // __cplusplus
};

/* For C++, verify that the presence of class methods hasn't added any
 * hidden data to the class structure which would break compatibility
 * with C code. */
#ifdef __cplusplus
STATIC_ASSERT(sizeof(Vector2f) == sizeof(float)*2, "Vector2f is wrong size");
STATIC_ASSERT(sizeof(Vector3f) == sizeof(float)*3, "Vector3f is wrong size");
STATIC_ASSERT(sizeof(Vector4f) == sizeof(float)*4, "Vector4f is wrong size");
#endif

/*----------------------------------*/

#undef DECLARE_VECTOR_METHODS

/*************************************************************************/
/*************************************************************************/

/* Define the C operator functions. */

/*-----------------------------------------------------------------------*/

/**
 * vec[234]_add, vec[234]_sub, vec[234]_mul, vec[234]_div:  Add, subtract,
 * multiply, or divide two vectors, component by component.
 *
 * [Parameters]
 *     a, b: Vectors to add, subtract, multiply, or divide.
 * [Return value]
 *     Result of operation.
 */
static inline CONST_FUNCTION Vector2f vec2_add(Vector2f a, Vector2f b)
{
    /* This header may be included from C++ code, so we can't just return
     * a compound literal and we can't use designated initializers. */
    const Vector2f result = {
        /* .x = */ a.x + b.x,
        /* .y = */ a.y + b.y};
    return result;
}

static inline CONST_FUNCTION Vector3f vec3_add(Vector3f a, Vector3f b)
{
    const Vector3f result = {
        /* .x = */ a.x + b.x,
        /* .y = */ a.y + b.y,
        /* .z = */ a.z + b.z};
    return result;
}

static inline CONST_FUNCTION Vector4f vec4_add(Vector4f a, Vector4f b)
{
    const Vector4f result = {
        /* .x = */ a.x + b.x,
        /* .y = */ a.y + b.y,
        /* .z = */ a.z + b.z,
        /* .w = */ a.w + b.w};
    return result;
}

/*----------------------------------*/

static inline CONST_FUNCTION Vector2f vec2_sub(Vector2f a, Vector2f b)
{
    const Vector2f result = {
        /* .x = */ a.x - b.x,
        /* .y = */ a.y - b.y};
    return result;
}

static inline CONST_FUNCTION Vector3f vec3_sub(Vector3f a, Vector3f b)
{
    const Vector3f result = {
        /* .x = */ a.x - b.x,
        /* .y = */ a.y - b.y,
        /* .z = */ a.z - b.z};
    return result;
}

static inline CONST_FUNCTION Vector4f vec4_sub(Vector4f a, Vector4f b)
{
    const Vector4f result = {
        /* .x = */ a.x - b.x,
        /* .y = */ a.y - b.y,
        /* .z = */ a.z - b.z,
        /* .w = */ a.w - b.w};
    return result;
}

/*----------------------------------*/

static inline CONST_FUNCTION Vector2f vec2_mul(Vector2f a, Vector2f b)
{
    const Vector2f result = {
        /* .x = */ a.x * b.x,
        /* .y = */ a.y * b.y};
    return result;
}

static inline CONST_FUNCTION Vector3f vec3_mul(Vector3f a, Vector3f b)
{
    const Vector3f result = {
        /* .x = */ a.x * b.x,
        /* .y = */ a.y * b.y,
        /* .z = */ a.z * b.z};
    return result;
}

static inline CONST_FUNCTION Vector4f vec4_mul(Vector4f a, Vector4f b)
{
    const Vector4f result = {
        /* .x = */ a.x * b.x,
        /* .y = */ a.y * b.y,
        /* .z = */ a.z * b.z,
        /* .w = */ a.w * b.w};
    return result;
}

/*----------------------------------*/

static inline CONST_FUNCTION Vector2f vec2_div(Vector2f a, Vector2f b)
{
    const Vector2f result = {
        /* .x = */ a.x / b.x,
        /* .y = */ a.y / b.y};
    return result;
}

static inline CONST_FUNCTION Vector3f vec3_div(Vector3f a, Vector3f b)
{
    const Vector3f result = {
        /* .x = */ a.x / b.x,
        /* .y = */ a.y / b.y,
        /* .z = */ a.z / b.z};
    return result;
}

static inline CONST_FUNCTION Vector4f vec4_div(Vector4f a, Vector4f b)
{
    const Vector4f result = {
        /* .x = */ a.x / b.x,
        /* .y = */ a.y / b.y,
        /* .z = */ a.z / b.z,
        /* .w = */ a.w / b.w};
    return result;
}

/*-----------------------------------------------------------------------*/

/**
 * vec[234]_add_scalar, vec[234]_scale:  Add or multiply a scalar value to
 * each component of a vector.
 *
 * [Parameters]
 *     v: Vector to operate on.
 *     k: Scalar value to add or multiply.
 * [Return value]
 *     Result of operation.
 */
static inline CONST_FUNCTION Vector2f vec2_add_scalar(Vector2f v, float k)
{
    const Vector2f result = {
        /* .x = */ v.x + k,
        /* .y = */ v.y + k};
    return result;
}
static inline CONST_FUNCTION Vector3f vec3_add_scalar(Vector3f v, float k)
{
    const Vector3f result = {
        /* .x = */ v.x + k,
        /* .y = */ v.y + k,
        /* .z = */ v.z + k};
    return result;
}
static inline CONST_FUNCTION Vector4f vec4_add_scalar(Vector4f v, float k)
{
    const Vector4f result = {
        /* .x = */ v.x + k,
        /* .y = */ v.y + k,
        /* .z = */ v.z + k,
        /* .w = */ v.w + k};
    return result;
}

/*----------------------------------*/

static inline CONST_FUNCTION Vector2f vec2_scale(Vector2f v, float k)
{
    const Vector2f result = {
        /* .x = */ v.x * k,
        /* .y = */ v.y * k};
    return result;
}
static inline CONST_FUNCTION Vector3f vec3_scale(Vector3f v, float k)
{
    const Vector3f result = {
        /* .x = */ v.x * k,
        /* .y = */ v.y * k,
        /* .z = */ v.z * k};
    return result;
}
static inline CONST_FUNCTION Vector4f vec4_scale(Vector4f v, float k)
{
    const Vector4f result = {
        /* .x = */ v.x * k,
        /* .y = */ v.y * k,
        /* .z = */ v.z * k,
        /* .w = */ v.w * k};
    return result;
}

/*-----------------------------------------------------------------------*/

/**
 * vec[234]_dot:  Calculate the dot product of two vectors.
 *
 * [Parameters]
 *     a, b: Vectors to compute the dot product of.
 * [Return value]
 *     dot(a, b)
 */
static CONST_FUNCTION inline float vec2_dot(Vector2f a, Vector2f b)
{
    return a.x*b.x + a.y*b.y;
}

static CONST_FUNCTION inline float vec3_dot(Vector3f a, Vector3f b)
{
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

static CONST_FUNCTION inline float vec4_dot(Vector4f a, Vector4f b)
{
    return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
}

/*-----------------------------------------------------------------------*/

/**
 * vec[234]_length2:  Return the squared length of the given vector.
 *
 * [Parameters]
 *     v: Vector to compute the squared length of.
 * [Return value]
 *     ||v||^2
 */
static CONST_FUNCTION inline float vec2_length2(Vector2f v)
{
    return vec2_dot(v, v);
}

static CONST_FUNCTION inline float vec3_length2(Vector3f v)
{
    return vec3_dot(v, v);
}

static CONST_FUNCTION inline float vec4_length2(Vector4f v)
{
    return vec4_dot(v, v);
}

/*----------------------------------*/

/**
 * vec[234]_length:  Return the length of the given vector.
 *
 * [Parameters]
 *     v: Vector to compute the length of.
 * [Return value]
 *     ||v||
 */
static CONST_FUNCTION inline float vec2_length(Vector2f v)
{
    return sqrtf(vec2_length2(v));
}

static CONST_FUNCTION inline float vec3_length(Vector3f v)
{
    return sqrtf(vec3_length2(v));
}

static CONST_FUNCTION inline float vec4_length(Vector4f v)
{
    return sqrtf(vec4_length2(v));
}

/*----------------------------------*/

/**
 * vec[234]_is_nonzero:  Return whether the given vector is nonzero, i.e.
 * has at least one nonzero component.
 *
 * IMPORTANT: Even if this function returns false (not zero), the
 * vec*_length() and vec*_length2() functions may still return zero due to
 * floating-point underflow.  To catch division by zero when dividing by
 * the length of a vector, use "vec*_length(v) != 0" (or, equivalently,
 * "vec*_length2(v) != 0") rather than calling vec*_is_nonzero().
 *
 * [Parameters]
 *     v: Vector to check for zeroness.
 * [Return value]
 *     True if the vector is nonzero, false otherwise.
 */
static CONST_FUNCTION inline int vec2_is_nonzero(Vector2f v)
{
    return v.x != 0 || v.y != 0;
}

static CONST_FUNCTION inline int vec3_is_nonzero(Vector3f v)
{
    return v.x != 0 || v.y != 0 || v.z != 0;
}

static CONST_FUNCTION inline int vec4_is_nonzero(Vector4f v)
{
    return v.x != 0 || v.y != 0 || v.z != 0 || v.w != 0;
}

/*----------------------------------*/

/**
 * vec[234]_is_length_in:  Return whether the given vector's length is
 * within (less than or equal to) the given limit.
 *
 * [Parameters]
 *     v: Pointer to vector to check.
 *     k: Length limit to check against.
 * [Return value]
 *     True if the vector's length is within the limit, false otherwise.
 */
static CONST_FUNCTION inline int vec2_is_length_in(Vector2f v, float k)
{
    return vec2_length2(v) <= k*k;
}

static CONST_FUNCTION inline int vec3_is_length_in(Vector3f v, float k)
{
    return vec3_length2(v) <= k*k;
}

static CONST_FUNCTION inline int vec4_is_length_in(Vector4f v, float k)
{
    return vec4_length2(v) <= k*k;
}

/*----------------------------------*/

/**
 * vec[234]_normalize:  Normalize (adjust to unit length) the given vector.
 * If the input vector has length zero, the result is the zero vector.
 *
 * [Parameters]
 *     v: Vector to normalize.
 * [Return value]
 *     Normalized vector.
 */

static inline CONST_FUNCTION Vector2f vec2_normalize(Vector2f v)
{
    const float length2 = vec2_length2(v);
    if (LIKELY(length2 > 0)) {
        return vec2_scale(v, 1/sqrtf(length2));
    } else {
        const Vector2f result = {0, 0};
        return result;
    }
}

static inline CONST_FUNCTION Vector3f vec3_normalize(Vector3f v)
{
    const float length2 = vec3_length2(v);
    if (LIKELY(length2 > 0)) {
        return vec3_scale(v, 1/sqrtf(length2));
    } else {
        const Vector3f result = {0, 0, 0};
        return result;
    }
}

static inline CONST_FUNCTION Vector4f vec4_normalize(Vector4f v)
{
    const float length2 = vec4_length2(v);
    if (LIKELY(length2 > 0)) {
        return vec4_scale(v, 1/sqrtf(length2));
    } else {
        const Vector4f result = {0, 0, 0, 0};
        return result;
    }
}

/*----------------------------------*/

/**
 * vec[234]_set_length:  Scale the given vector to have the given length.
 *
 * [Parameters]
 *     v: Vector to scale.
 *     k: New length for vector.
 * [Return value]
 *     Scaled vector.
 */

static inline CONST_FUNCTION Vector2f vec2_set_length(Vector2f v, float k)
{
    const float length2 = vec2_length2(v);
    if (LIKELY(length2 > 0)) {
        return vec2_scale(v, k/sqrtf(length2));
    } else {
        const Vector2f result = {0, 0};
        return result;
    }
}

static inline CONST_FUNCTION Vector3f vec3_set_length(Vector3f v, float k)
{
    const float length2 = vec3_length2(v);
    if (LIKELY(length2 > 0)) {
        return vec3_scale(v, k/sqrtf(length2));
    } else {
        const Vector3f result = {0, 0, 0};
        return result;
    }
}

static inline CONST_FUNCTION Vector4f vec4_set_length(Vector4f v, float k)
{
    const float length2 = vec4_length2(v);
    if (LIKELY(length2 > 0)) {
        return vec4_scale(v, k/sqrtf(length2));
    } else {
        const Vector4f result = {0, 0, 0, 0};
        return result;
    }
}

/*-----------------------------------------------------------------------*/

/**
 * vec[234]_cap_length:  Scale the given vector if necessary to have a
 * length no greater than the given length.
 *
 * [Parameters]
 *     v: Vector to scale.
 *     k: Maximum length for vector.
 * [Return value]
 *     Scaled vector.
 */

static inline CONST_FUNCTION Vector2f vec2_cap_length(Vector2f v, float k)
{
    const float length2 = vec2_length2(v);
    if (length2 > k*k) {
        return vec2_scale(v, k/sqrtf(length2));
    } else {
        return v;
    }
}

static inline CONST_FUNCTION Vector3f vec3_cap_length(Vector3f v, float k)
{
    const float length2 = vec3_length2(v);
    if (length2 > k*k) {
        return vec3_scale(v, k/sqrtf(length2));
    } else {
        return v;
    }
}

static inline CONST_FUNCTION Vector4f vec4_cap_length(Vector4f v, float k)
{
    const float length2 = vec4_length2(v);
    if (length2 > k*k) {
        return vec4_scale(v, k/sqrtf(length2));
    } else {
        return v;
    }
}

/*-----------------------------------------------------------------------*/

/**
 * vec[234]_lerp:  Linearly interpolate between two vectors.
 *
 * [Parameters]
 *     a: Initial vector.
 *     b: Final vector.
 *     k: Interpolation factor.  A factor of 0 returns the initial vector
 *            (a), while a factor of 1 returns the final vector (b).
 * [Return value]
 *     a*(1-k) + b*k
 */
static inline CONST_FUNCTION Vector2f vec2_lerp(Vector2f a, Vector2f b, float k)
{
    return vec2_add(vec2_scale(a, 1-k), vec2_scale(b, k));
}

static inline CONST_FUNCTION Vector3f vec3_lerp(Vector3f a, Vector3f b, float k)
{
    return vec3_add(vec3_scale(a, 1-k), vec3_scale(b, k));
}

static inline CONST_FUNCTION Vector4f vec4_lerp(Vector4f a, Vector4f b, float k)
{
    return vec4_add(vec4_scale(a, 1-k), vec4_scale(b, k));
}

/*-----------------------------------------------------------------------*/

/**
 * vec3_cross:  Calculate the cross product of two 3-component vectors.
 *
 * [Parameters]
 *     a, b: Vctors to take the cross product of.
 * [Return value]
 *     Cross product of the vectors.
 */
static inline CONST_FUNCTION Vector3f vec3_cross(Vector3f a, Vector3f b)
{
    const Vector3f result = {
        /* .x = */ a.y * b.z - a.z * b.y,
        /* .y = */ a.z * b.x - a.x * b.z,
        /* .z = */ a.x * b.y - a.y * b.x};
    return result;
}

/*----------------------------------*/

/**
 * vec4_cross:  Calculate the cross product of three 4-component vectors.
 *
 * [Parameters]
 *     a, b, c: Vctors to take the cross product of.
 * [Return value]
 *     Cross product of the vectors.
 */
static inline CONST_FUNCTION Vector4f vec4_cross(Vector4f a, Vector4f b,
                                                 Vector4f c)
{
    const Vector4f result = {
        /* .x = */  (  a.y * (b.z * c.w - b.w * c.z)
                     + a.z * (b.w * c.y - b.y * c.w)
                     + a.w * (b.y * c.z - b.z * c.y)),
        /* .y = */ -(  a.x * (b.z * c.w - b.w * c.z)
                     + a.z * (b.w * c.x - b.x * c.w)
                     + a.w * (b.x * c.z - b.z * c.x)),
        /* .z = */  (  a.x * (b.y * c.w - b.w * c.y)
                     + a.y * (b.w * c.x - b.x * c.w)
                     + a.w * (b.x * c.y - b.y * c.x)),
        /* .w = */ -(  a.x * (b.y * c.z - b.z * c.y)
                     + a.y * (b.z * c.x - b.x * c.z)
                     + a.z * (b.x * c.y - b.y * c.x))};
    return result;
}

/*************************************************************************/
/*************************************************************************/

/* Define the C++ operator functions. */

#ifdef __cplusplus

/*-----------------------------------------------------------------------*/

Vector2f::Vector2f(const Vector2f &v) : x(v.x), y(v.y) {}
Vector2f::Vector2f(const Vector3f &v) : x(v.x), y(v.y) {}
Vector2f::Vector2f(const Vector4f &v) : x(v.x), y(v.y) {}

Vector2f &Vector2f::operator=(const float k)
    {x = k;   y = 0;   return *this;}
Vector2f &Vector2f::operator=(const Vector2f &v)
    {x = v.x; y = v.y; return *this;}
Vector2f &Vector2f::operator=(const Vector3f &v)
    {x = v.x; y = v.y; return *this;}
Vector2f &Vector2f::operator=(const Vector4f &v)
    {x = v.x; y = v.y; return *this;}

Vector2f Vector2f::operator+() const
    {return *this;}
Vector2f Vector2f::operator-() const
    {return Vector2f(-x, -y);}

Vector2f Vector2f::operator+(const float k) const
    {return Vector2f(x+k, y+k);}
Vector2f Vector2f::operator+(const Vector2f &v) const
    {return Vector2f(x+v.x, y+v.y);}
Vector2f Vector2f::operator+(const Vector3f &v) const
    {return Vector2f(x+v.x, y+v.y);}
Vector2f Vector2f::operator+(const Vector4f &v) const
    {return Vector2f(x+v.x, y+v.y);}

Vector2f Vector2f::operator-(const float k) const
    {return Vector2f(x-k, y-k);}
Vector2f Vector2f::operator-(const Vector2f &v) const
    {return Vector2f(x-v.x, y-v.y);}
Vector2f Vector2f::operator-(const Vector3f &v) const
    {return Vector2f(x-v.x, y-v.y);}
Vector2f Vector2f::operator-(const Vector4f &v) const
    {return Vector2f(x-v.x, y-v.y);}

Vector2f Vector2f::operator*(const float k) const
    {return Vector2f(x*k, y*k);}
Vector2f Vector2f::operator*(const Vector2f &v) const
    {return Vector2f(x*v.x, y*v.y);}
Vector2f Vector2f::operator*(const Vector3f &v) const
    {return Vector2f(x*v.x, y*v.y);}
Vector2f Vector2f::operator*(const Vector4f &v) const
    {return Vector2f(x*v.x, y*v.y);}

Vector2f Vector2f::operator/(const float k) const
    {return Vector2f(x/k, y/k);}
Vector2f Vector2f::operator/(const Vector2f &v) const
    {return Vector2f(x/v.x, y/v.y);}
Vector2f Vector2f::operator/(const Vector3f &v) const
    {return Vector2f(x/v.x, y/v.y);}
Vector2f Vector2f::operator/(const Vector4f &v) const
    {return Vector2f(x/v.x, y/v.y);}

Vector2f operator+(const float k, const Vector2f &v) {return v+k;}
Vector2f operator*(const float k, const Vector2f &v) {return v*k;}

Vector2f &Vector2f::operator+=(const float k)
    {x += k;   y += k;   return *this;}
Vector2f &Vector2f::operator+=(const Vector2f &v)
    {x += v.x; y += v.y; return *this;}
Vector2f &Vector2f::operator+=(const Vector3f &v)
    {x += v.x; y += v.y; return *this;}
Vector2f &Vector2f::operator+=(const Vector4f &v)
    {x += v.x; y += v.y; return *this;}

Vector2f &Vector2f::operator-=(const float k)
    {x -= k;   y -= k;   return *this;}
Vector2f &Vector2f::operator-=(const Vector2f &v)
    {x -= v.x; y -= v.y; return *this;}
Vector2f &Vector2f::operator-=(const Vector3f &v)
    {x -= v.x; y -= v.y; return *this;}
Vector2f &Vector2f::operator-=(const Vector4f &v)
    {x -= v.x; y -= v.y; return *this;}

Vector2f &Vector2f::operator*=(const float k)
    {x *= k;   y *= k;   return *this;}
Vector2f &Vector2f::operator*=(const Vector2f &v)
    {x *= v.x; y *= v.y; return *this;}
Vector2f &Vector2f::operator*=(const Vector3f &v)
    {x *= v.x; y *= v.y; return *this;}
Vector2f &Vector2f::operator*=(const Vector4f &v)
    {x *= v.x; y *= v.y; return *this;}

Vector2f &Vector2f::operator/=(const float k)
    {x /= k;   y /= k;   return *this;}
Vector2f &Vector2f::operator/=(const Vector2f &v)
    {x /= v.x; y /= v.y; return *this;}
Vector2f &Vector2f::operator/=(const Vector3f &v)
    {x /= v.x; y /= v.y; return *this;}
Vector2f &Vector2f::operator/=(const Vector4f &v)
    {x /= v.x; y /= v.y; return *this;}

bool Vector2f::operator==(const Vector2f &v) const
    {return x == v.x && y == v.y;}
bool Vector2f::operator!=(const Vector2f &v) const
    {return x != v.x || y != v.y;}

float Vector2f::length() const           {return sqrtf(length2());}
float Vector2f::length2() const          {return x*x + y*y;}
bool Vector2f::is_nonzero() const        {return x!=0 || y!=0;}
bool Vector2f::is_length_in(const float k) const {return length2() <= k*k;}
void Vector2f::normalize() {
    if (LIKELY(length2() != 0)) {
        *this /= length();
    } else {
        x = y = 0;
    }
}
void Vector2f::set_length(const float k) {
    if (LIKELY(length2() != 0)) {
        *this *= k/length();
    } else {
        x = y = 0;
    }
}
void Vector2f::cap_length(const float k)
    {if (length2() > k*k) *this *= k/length();}
float Vector2f::dot(const Vector2f &v) const
    {return x*v.x + y*v.y;}
Vector2f Vector2f::lerp(const Vector2f &v, const float k) const
    {return (*this)*(1-k) + v*k;}

/*-----------------------------------------------------------------------*/

Vector3f::Vector3f(const Vector2f &v) : x(v.x), y(v.y), z(0) {}
Vector3f::Vector3f(const Vector3f &v) : x(v.x), y(v.y), z(v.z) {}
Vector3f::Vector3f(const Vector4f &v) : x(v.x), y(v.y), z(v.z) {}

Vector3f &Vector3f::operator=(const float k)
    {x = k;   y = 0;   z = 0;   return *this;}
Vector3f &Vector3f::operator=(const Vector2f &v)
    {x = v.x; y = v.y; z = 0;   return *this;}
Vector3f &Vector3f::operator=(const Vector3f &v)
    {x = v.x; y = v.y; z = v.z; return *this;}
Vector3f &Vector3f::operator=(const Vector4f &v)
    {x = v.x; y = v.y; z = v.z; return *this;}

Vector3f Vector3f::operator+() const
    {return *this;}
Vector3f Vector3f::operator-() const
    {return Vector3f(-x, -y, -z);}

Vector3f Vector3f::operator+(const float k) const
    {return Vector3f(x+k, y+k, z+k);}
Vector3f Vector3f::operator+(const Vector2f &v) const
    {return Vector3f(x+v.x, y+v.y, z);}
Vector3f Vector3f::operator+(const Vector3f &v) const
    {return Vector3f(x+v.x, y+v.y, z+v.z);}
Vector3f Vector3f::operator+(const Vector4f &v) const
    {return Vector3f(x+v.x, y+v.y, z+v.z);}

Vector3f Vector3f::operator-(const float k) const
    {return Vector3f(x-k, y-k, z-k);}
Vector3f Vector3f::operator-(const Vector2f &v) const
    {return Vector3f(x-v.x, y-v.y, z);}
Vector3f Vector3f::operator-(const Vector3f &v) const
    {return Vector3f(x-v.x, y-v.y, z-v.z);}
Vector3f Vector3f::operator-(const Vector4f &v) const
    {return Vector3f(x-v.x, y-v.y, z-v.z);}

Vector3f Vector3f::operator*(const float k) const
    {return Vector3f(x*k, y*k, z*k);}
Vector3f Vector3f::operator*(const Vector2f &v) const
    {return Vector3f(x*v.x, y*v.y, z);}
Vector3f Vector3f::operator*(const Vector3f &v) const
    {return Vector3f(x*v.x, y*v.y, z*v.z);}
Vector3f Vector3f::operator*(const Vector4f &v) const
    {return Vector3f(x*v.x, y*v.y, z*v.z);}

Vector3f Vector3f::operator/(const float k) const
    {return Vector3f(x/k, y/k, z/k);}
Vector3f Vector3f::operator/(const Vector2f &v) const
    {return Vector3f(x/v.x, y/v.y, z);}
Vector3f Vector3f::operator/(const Vector3f &v) const
    {return Vector3f(x/v.x, y/v.y, z/v.z);}
Vector3f Vector3f::operator/(const Vector4f &v) const
    {return Vector3f(x/v.x, y/v.y, z/v.z);}

Vector3f operator+(const float k, const Vector3f &v) {return v+k;}
Vector3f operator*(const float k, const Vector3f &v) {return v*k;}

Vector3f &Vector3f::operator+=(const float k)
    {x += k;   y += k;   z += k;   return *this;}
Vector3f &Vector3f::operator+=(const Vector2f &v)
    {x += v.x; y += v.y;           return *this;}
Vector3f &Vector3f::operator+=(const Vector3f &v)
    {x += v.x; y += v.y; z += v.z; return *this;}
Vector3f &Vector3f::operator+=(const Vector4f &v)
    {x += v.x; y += v.y; z += v.z; return *this;}

Vector3f &Vector3f::operator-=(const float k)
    {x -= k;   y -= k;   z -= k;   return *this;}
Vector3f &Vector3f::operator-=(const Vector2f &v)
    {x -= v.x; y -= v.y;           return *this;}
Vector3f &Vector3f::operator-=(const Vector3f &v)
    {x -= v.x; y -= v.y; z -= v.z; return *this;}
Vector3f &Vector3f::operator-=(const Vector4f &v)
    {x -= v.x; y -= v.y; z -= v.z; return *this;}

Vector3f &Vector3f::operator*=(const float k)
    {x *= k;   y *= k;   z *= k;   return *this;}
Vector3f &Vector3f::operator*=(const Vector2f &v)
    {x *= v.x; y *= v.y;           return *this;}
Vector3f &Vector3f::operator*=(const Vector3f &v)
    {x *= v.x; y *= v.y; z *= v.z; return *this;}
Vector3f &Vector3f::operator*=(const Vector4f &v)
    {x *= v.x; y *= v.y; z *= v.z; return *this;}

Vector3f &Vector3f::operator/=(const float k)
    {x /= k;   y /= k;   z /= k;   return *this;}
Vector3f &Vector3f::operator/=(const Vector2f &v)
    {x /= v.x; y /= v.y;           return *this;}
Vector3f &Vector3f::operator/=(const Vector3f &v)
    {x /= v.x; y /= v.y; z /= v.z; return *this;}
Vector3f &Vector3f::operator/=(const Vector4f &v)
    {x /= v.x; y /= v.y; z /= v.z; return *this;}

bool Vector3f::operator==(const Vector3f &v) const
    {return x == v.x && y == v.y && z == v.z;}
bool Vector3f::operator!=(const Vector3f &v) const
    {return x != v.x || y != v.y || z != v.z;}

float Vector3f::length() const           {return sqrtf(length2());}
float Vector3f::length2() const          {return x*x + y*y + z*z;}
bool Vector3f::is_nonzero() const        {return x!=0 || y!=0 || z!=0;}
bool Vector3f::is_length_in(const float k) const {return length2() <= k*k;}
void Vector3f::normalize() {
    if (LIKELY(length2() != 0)) {
        *this /= length();
    } else {
        x = y = z = 0;
    }
}
void Vector3f::set_length(const float k) {
    if (LIKELY(length2() != 0)) {
        *this *= k/length();
    } else {
        x = y = z = 0;
    }
}
void Vector3f::cap_length(const float k)
    {if (length2() > k*k) *this *= k/length();}
float Vector3f::dot(const Vector3f &v) const
    {return x*v.x + y*v.y + z*v.z;}
Vector3f Vector3f::lerp(const Vector3f &v, const float k) const
    {return (*this)*(1-k) + v*k;}
Vector3f Vector3f::cross(const Vector3f &v) const
    {return Vector3f(y*v.z - z*v.y,
                     z*v.x - x*v.z,
                     x*v.y - y*v.x);}

/*-----------------------------------------------------------------------*/

Vector4f::Vector4f(const Vector2f &v) : x(v.x), y(v.y), z(0), w(0) {}
Vector4f::Vector4f(const Vector3f &v) : x(v.x), y(v.y), z(v.z), w(0) {}
Vector4f::Vector4f(const Vector4f &v) : x(v.x), y(v.y), z(v.z), w(v.w) {}

Vector4f &Vector4f::operator=(const float k)
    {x = k;   y = 0;   z = 0;   w = 0;   return *this;}
Vector4f &Vector4f::operator=(const Vector2f &v)
    {x = v.x; y = v.y; z = 0;   w = 0;   return *this;}
Vector4f &Vector4f::operator=(const Vector3f &v)
    {x = v.x; y = v.y; z = v.z; w = 0;   return *this;}
Vector4f &Vector4f::operator=(const Vector4f &v)
    {x = v.x; y = v.y; z = v.z; w = v.w; return *this;}

Vector4f Vector4f::operator+() const
    {return *this;}
Vector4f Vector4f::operator-() const
    {return Vector4f(-x, -y, -z, -w);}

Vector4f Vector4f::operator+(const float k) const
    {return Vector4f(x+k, y+k, z+k, w+k);}
Vector4f Vector4f::operator+(const Vector2f &v) const
    {return Vector4f(x+v.x, y+v.y, z, w);}
Vector4f Vector4f::operator+(const Vector3f &v) const
    {return Vector4f(x+v.x, y+v.y, z+v.z, w);}
Vector4f Vector4f::operator+(const Vector4f &v) const
    {return Vector4f(x+v.x, y+v.y, z+v.z, w+v.w);}

Vector4f Vector4f::operator-(const float k) const
    {return Vector4f(x-k, y-k, z-k, w-k);}
Vector4f Vector4f::operator-(const Vector2f &v) const
    {return Vector4f(x-v.x, y-v.y, z, w);}
Vector4f Vector4f::operator-(const Vector3f &v) const
    {return Vector4f(x-v.x, y-v.y, z-v.z, w);}
Vector4f Vector4f::operator-(const Vector4f &v) const
    {return Vector4f(x-v.x, y-v.y, z-v.z, w-v.w);}

Vector4f Vector4f::operator*(const float k) const
    {return Vector4f(x*k, y*k, z*k, w*k);}
Vector4f Vector4f::operator*(const Vector2f &v) const
    {return Vector4f(x*v.x, y*v.y, z, w);}
Vector4f Vector4f::operator*(const Vector3f &v) const
    {return Vector4f(x*v.x, y*v.y, z*v.z, w);}
Vector4f Vector4f::operator*(const Vector4f &v) const
    {return Vector4f(x*v.x, y*v.y, z*v.z, w*v.w);}

Vector4f Vector4f::operator/(const float k) const
    {return Vector4f(x/k, y/k, z/k, w/k);}
Vector4f Vector4f::operator/(const Vector2f &v) const
    {return Vector4f(x/v.x, y/v.y, z, w);}
Vector4f Vector4f::operator/(const Vector3f &v) const
    {return Vector4f(x/v.x, y/v.y, z/v.z, w);}
Vector4f Vector4f::operator/(const Vector4f &v) const
    {return Vector4f(x/v.x, y/v.y, z/v.z, w/v.w);}

Vector4f operator+(const float k, const Vector4f &v) {return v+k;}
Vector4f operator*(const float k, const Vector4f &v) {return v*k;}

Vector4f &Vector4f::operator+=(const float k)
    {x += k;   y += k;   z += k;   w += k;   return *this;}
Vector4f &Vector4f::operator+=(const Vector2f &v)
    {x += v.x; y += v.y;                     return *this;}
Vector4f &Vector4f::operator+=(const Vector3f &v)
    {x += v.x; y += v.y; z += v.z;           return *this;}
Vector4f &Vector4f::operator+=(const Vector4f &v)
    {x += v.x; y += v.y; z += v.z; w += v.w; return *this;}

Vector4f &Vector4f::operator-=(const float k)
    {x -= k;   y -= k;   z -= k;   w -= k;   return *this;}
Vector4f &Vector4f::operator-=(const Vector2f &v)
    {x -= v.x; y -= v.y;                     return *this;}
Vector4f &Vector4f::operator-=(const Vector3f &v)
    {x -= v.x; y -= v.y; z -= v.z;           return *this;}
Vector4f &Vector4f::operator-=(const Vector4f &v)
    {x -= v.x; y -= v.y; z -= v.z; w -= v.w; return *this;}

Vector4f &Vector4f::operator*=(const float k)
    {x *= k;   y *= k;   z *= k;   w *= k;   return *this;}
Vector4f &Vector4f::operator*=(const Vector2f &v)
    {x *= v.x; y *= v.y;                     return *this;}
Vector4f &Vector4f::operator*=(const Vector3f &v)
    {x *= v.x; y *= v.y; z *= v.z;           return *this;}
Vector4f &Vector4f::operator*=(const Vector4f &v)
    {x *= v.x; y *= v.y; z *= v.z; w *= v.w; return *this;}

Vector4f &Vector4f::operator/=(const float k)
    {x /= k;   y /= k;   z /= k;   w /= k;   return *this;}
Vector4f &Vector4f::operator/=(const Vector2f &v)
    {x /= v.x; y /= v.y;                     return *this;}
Vector4f &Vector4f::operator/=(const Vector3f &v)
    {x /= v.x; y /= v.y; z /= v.z;           return *this;}
Vector4f &Vector4f::operator/=(const Vector4f &v)
    {x /= v.x; y /= v.y; z /= v.z; w /= v.w; return *this;}

bool Vector4f::operator==(const Vector4f &v) const
    {return x == v.x && y == v.y && z == v.z && w == v.w;}
bool Vector4f::operator!=(const Vector4f &v) const
    {return x != v.x || y != v.y || z != v.z || w != v.w;}

float Vector4f::length() const           {return sqrtf(length2());}
float Vector4f::length2() const          {return x*x + y*y + z*z + w*w;}
bool Vector4f::is_nonzero() const        {return x!=0 || y!=0 || z!=0 || w!=0;}
bool Vector4f::is_length_in(const float k) const {return length2() <= k*k;}
void Vector4f::normalize() {
    if (LIKELY(length2() != 0)) {
        *this /= length();
    } else {
        x = y = z = w = 0;
    }
}
void Vector4f::set_length(const float k) {
    if (LIKELY(length2() != 0)) {
        *this *= k/length();
    } else {
        x = y = z = w = 0;
    }
}
void Vector4f::cap_length(const float k)
    {if (length2() > k*k) *this *= k/length();}
float Vector4f::dot(const Vector4f &v) const
    {return x*v.x + y*v.y + z*v.z + w*v.w;}
Vector4f Vector4f::lerp(const Vector4f &v, const float k) const
    {return (*this)*(1-k) + v*k;}
Vector4f Vector4f::cross(const Vector4f &v2, const Vector4f &v3) const
    {return Vector4f(+(  y * (v2.z*v3.w - v2.w*v3.z)
                       + z * (v2.w*v3.y - v2.y*v3.w)
                       + w * (v2.y*v3.z - v2.z*v3.y)),
                     -(  x * (v2.z*v3.w - v2.w*v3.z)
                       + z * (v2.w*v3.x - v2.x*v3.w)
                       + w * (v2.x*v3.z - v2.z*v3.x)),
                     +(  x * (v2.y*v3.w - v2.w*v3.y)
                       + y * (v2.w*v3.x - v2.x*v3.w)
                       + w * (v2.x*v3.y - v2.y*v3.x)),
                     -(  x * (v2.y*v3.z - v2.z*v3.y)
                       + y * (v2.z*v3.x - v2.x*v3.z)
                       + z * (v2.x*v3.y - v2.y*v3.x)));}

/*----------------------------------*/

#endif  // __cplusplus

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_MATH_VECTOR_H
