/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/graphics/state.c: Render state manipulation.
 */

#include "src/base.h"
#include "src/graphics.h"
#include "src/graphics/internal.h"
#include "src/math.h"
#include "src/sysdep.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Current size of the viewport, or zero if the viewport has never been set. */
static int viewport_width, viewport_height;

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

void graphics_set_viewport(int left, int bottom, int width, int height)
{
    if (UNLIKELY(left < 0)
     || UNLIKELY(bottom < 0)
     || UNLIKELY(width <= 0)
     || UNLIKELY(height <= 0)) {
        DLOG("Invalid parameters: %d %d %d %d", left, bottom, width, height);
        return;
    }

    sys_graphics_set_viewport(left, bottom, width, height);
    viewport_width = width;
    viewport_height = height;
}

/*-----------------------------------------------------------------------*/

int graphics_viewport_width(void)
{
    return viewport_width;
}

/*-----------------------------------------------------------------------*/

int graphics_viewport_height(void)
{
    return viewport_height;
}

/*-----------------------------------------------------------------------*/

void graphics_set_clip_region(int left, int bottom, int width, int height)
{
    if (UNLIKELY(left < 0)
     || UNLIKELY(bottom < 0)
     || UNLIKELY(width < 0)
     || UNLIKELY(height < 0)) {
        DLOG("Invalid parameters: %d %d %d %d", left, bottom, width, height);
        return;
    }

    if (width == 0 && height == 0) {
        sys_graphics_set_int_param(SYS_GRAPHICS_PARAM_CLIP, 0);
    } else if (width == 0 || height == 0) {
        DLOG("Invalid parameters: %d %d %d %d", left, bottom, width, height);
        return;
    } else {
        sys_graphics_set_int_param(SYS_GRAPHICS_PARAM_CLIP, 1);
        sys_graphics_set_clip_region(left, bottom, width, height);
    }
}

/*-----------------------------------------------------------------------*/

void graphics_set_depth_range(float near, float far)
{
    if (UNLIKELY(near < 0)
     || UNLIKELY(far <= near)
     || UNLIKELY(far > 1)) {
        DLOG("Invalid parameters: %g %g", near, far);
        return;
    }

    sys_graphics_set_depth_range(near, far);
}

/*-----------------------------------------------------------------------*/

int graphics_set_blend(GraphicsBlendOperation operation,
                       GraphicsBlendFactor src_factor,
                       GraphicsBlendFactor dest_factor)
{
    /* These switch statements are just checks for validity. */
    switch (operation) {
      case GRAPHICS_BLEND_ADD:
      case GRAPHICS_BLEND_SUB:
      case GRAPHICS_BLEND_RSUB:
        switch (src_factor) {
          case GRAPHICS_BLEND_ZERO:
          case GRAPHICS_BLEND_ONE:
          case GRAPHICS_BLEND_SRC_COLOR:
          case GRAPHICS_BLEND_SRC_ALPHA:
          case GRAPHICS_BLEND_INV_SRC_ALPHA:
          case GRAPHICS_BLEND_DEST_COLOR:
          case GRAPHICS_BLEND_DEST_ALPHA:
          case GRAPHICS_BLEND_INV_DEST_ALPHA:
          case GRAPHICS_BLEND_CONSTANT:
          case GRAPHICS_BLEND_INV_CONSTANT:
            switch (dest_factor) {
              case GRAPHICS_BLEND_ZERO:
              case GRAPHICS_BLEND_ONE:
              case GRAPHICS_BLEND_SRC_COLOR:
              case GRAPHICS_BLEND_SRC_ALPHA:
              case GRAPHICS_BLEND_INV_SRC_ALPHA:
              case GRAPHICS_BLEND_DEST_COLOR:
              case GRAPHICS_BLEND_DEST_ALPHA:
              case GRAPHICS_BLEND_INV_DEST_ALPHA:
              case GRAPHICS_BLEND_CONSTANT:
              case GRAPHICS_BLEND_INV_CONSTANT:
                return sys_graphics_set_blend(operation,
                                              src_factor, dest_factor);
            }
        }
    }

    DLOG("Invalid parameters: %d %d %d", operation, src_factor, dest_factor);
    return 0;
}

/*-----------------------------------------------------------------------*/

extern int graphics_set_blend_alpha(int enable, GraphicsBlendFactor src_factor,
                                    GraphicsBlendFactor dest_factor)
{
    switch (src_factor) {
      case GRAPHICS_BLEND_ZERO:
      case GRAPHICS_BLEND_ONE:
      case GRAPHICS_BLEND_SRC_COLOR:
      case GRAPHICS_BLEND_SRC_ALPHA:
      case GRAPHICS_BLEND_INV_SRC_ALPHA:
      case GRAPHICS_BLEND_DEST_COLOR:
      case GRAPHICS_BLEND_DEST_ALPHA:
      case GRAPHICS_BLEND_INV_DEST_ALPHA:
      case GRAPHICS_BLEND_CONSTANT:
      case GRAPHICS_BLEND_INV_CONSTANT:
        switch (dest_factor) {
          case GRAPHICS_BLEND_ZERO:
          case GRAPHICS_BLEND_ONE:
          case GRAPHICS_BLEND_SRC_COLOR:
          case GRAPHICS_BLEND_SRC_ALPHA:
          case GRAPHICS_BLEND_INV_SRC_ALPHA:
          case GRAPHICS_BLEND_DEST_COLOR:
          case GRAPHICS_BLEND_DEST_ALPHA:
          case GRAPHICS_BLEND_INV_DEST_ALPHA:
          case GRAPHICS_BLEND_CONSTANT:
          case GRAPHICS_BLEND_INV_CONSTANT:
            return sys_graphics_set_blend_alpha(enable != 0,
                                                src_factor, dest_factor);
        }
    }

    DLOG("Invalid parameters: %d %d %d", enable, src_factor, dest_factor);
    return 0;
}

/*-----------------------------------------------------------------------*/

void graphics_set_no_blend(void)
{
    ASSERT(graphics_set_blend(GRAPHICS_BLEND_ADD,
                              GRAPHICS_BLEND_ONE, GRAPHICS_BLEND_ZERO));
    ASSERT(graphics_set_blend_alpha(0, 0, 0));
}

/*-----------------------------------------------------------------------*/

void graphics_set_projection_matrix(const Matrix4f *matrix)
{
    if (UNLIKELY(!matrix)) {
        DLOG("matrix == NULL");
        return;
    }

    sys_graphics_set_matrix_param(SYS_GRAPHICS_PARAM_PROJECTION_MATRIX, matrix);
}

/*-----------------------------------------------------------------------*/

void graphics_set_view_matrix(const Matrix4f *matrix)
{
    if (UNLIKELY(!matrix)) {
        DLOG("matrix == NULL");
        return;
    }

    sys_graphics_set_matrix_param(SYS_GRAPHICS_PARAM_VIEW_MATRIX, matrix);
}

/*-----------------------------------------------------------------------*/

void graphics_set_model_matrix(const Matrix4f *matrix)
{
    if (UNLIKELY(!matrix)) {
        DLOG("matrix == NULL");
        return;
    }

    sys_graphics_set_matrix_param(SYS_GRAPHICS_PARAM_MODEL_MATRIX, matrix);
}

/*-----------------------------------------------------------------------*/

void graphics_get_projection_matrix(Matrix4f *matrix_ret)
{
    if (UNLIKELY(!matrix_ret)) {
        DLOG("matrix_ret == NULL");
        return;
    }

    sys_graphics_get_matrix_param(
        SYS_GRAPHICS_PARAM_PROJECTION_MATRIX, matrix_ret);
}

/*-----------------------------------------------------------------------*/

void graphics_get_view_matrix(Matrix4f *matrix_ret)
{
    if (UNLIKELY(!matrix_ret)) {
        DLOG("matrix_ret == NULL");
        return;
    }

    sys_graphics_get_matrix_param(SYS_GRAPHICS_PARAM_VIEW_MATRIX, matrix_ret);
}

/*-----------------------------------------------------------------------*/

void graphics_get_model_matrix(Matrix4f *matrix_ret)
{
    if (UNLIKELY(!matrix_ret)) {
        DLOG("matrix_ret == NULL");
        return;
    }

    sys_graphics_get_matrix_param(SYS_GRAPHICS_PARAM_MODEL_MATRIX, matrix_ret);
}

/*-----------------------------------------------------------------------*/

void graphics_make_parallel_projection(
    float left, float right, float bottom, float top, float near, float far,
    Matrix4f *matrix_ret)
{
    const float dx = right - left;
    const float dy = top - bottom;
    const float dz = far - near;

    *matrix_ret = mat4_identity;
    matrix_ret->_11 = 2.0f / dx;
    matrix_ret->_22 = 2.0f / dy;
    matrix_ret->_33 = 2.0f / dz;
    matrix_ret->_41 = -(right + left) / dx;
    matrix_ret->_42 = -(top + bottom) / dy;
    matrix_ret->_43 = -(far + near) / dz;
}

/*-----------------------------------------------------------------------*/

void graphics_set_parallel_projection(
    float left, float right, float bottom, float top, float near, float far)
{
    Matrix4f m;
    graphics_make_parallel_projection(left, right, bottom, top, near, far, &m);
    sys_graphics_set_matrix_param(SYS_GRAPHICS_PARAM_PROJECTION_MATRIX, &m);
}

/*-----------------------------------------------------------------------*/

void graphics_make_perspective_projection(
    float y_fov, float aspect, float near, float far, int right_handed,
    Matrix4f *matrix_ret)
{
    const float cotangent = dcosf(y_fov/2) / dsinf(y_fov/2);
    const float dz = near - far;

    *matrix_ret = mat4_identity;
    matrix_ret->_11 = cotangent / aspect;
    matrix_ret->_22 = cotangent;
    matrix_ret->_33 = -(near + far) / dz;
    matrix_ret->_34 = 1;
    matrix_ret->_43 = (2 * near * far) / dz;
    matrix_ret->_44 = 0;
    if (right_handed) {
        matrix_ret->_33 = -matrix_ret->_33;
        matrix_ret->_34 = -matrix_ret->_34;
    }
}

/*-----------------------------------------------------------------------*/

void graphics_set_perspective_projection(
    float y_fov, float aspect, float near, float far, int right_handed)
{
    Matrix4f m;
    graphics_make_perspective_projection(y_fov, aspect, near, far,
                                         right_handed, &m);
    sys_graphics_set_matrix_param(SYS_GRAPHICS_PARAM_PROJECTION_MATRIX, &m);
}

/*-----------------------------------------------------------------------*/

void graphics_enable_alpha_test(int on)
{
    sys_graphics_set_int_param(SYS_GRAPHICS_PARAM_ALPHA_TEST, (on != 0));
}

/*-----------------------------------------------------------------------*/

void graphics_set_alpha_test_comparison(GraphicsComparisonType type)
{
    switch (type) {
      case GRAPHICS_COMPARISON_LESS:
      case GRAPHICS_COMPARISON_LESS_EQUAL:
      case GRAPHICS_COMPARISON_GREATER_EQUAL:
      case GRAPHICS_COMPARISON_GREATER:
        sys_graphics_set_int_param(
            SYS_GRAPHICS_PARAM_ALPHA_TEST_COMPARISON, type);
        return;
      case GRAPHICS_COMPARISON_FALSE:
      case GRAPHICS_COMPARISON_TRUE:
      case GRAPHICS_COMPARISON_EQUAL:
      case GRAPHICS_COMPARISON_NOT_EQUAL:
        break;  // Fall through to error message.
    }
    DLOG("Invalid comparison type: %d", type);
}

/*-----------------------------------------------------------------------*/

void graphics_set_alpha_test_reference(float alpha)
{
    sys_graphics_set_float_param(SYS_GRAPHICS_PARAM_ALPHA_REFERENCE, alpha);
}

/*-----------------------------------------------------------------------*/

void graphics_set_blend_color(const Vector4f *color)
{
    if (UNLIKELY(!color)) {
        DLOG("color == NULL");
        return;
    }

    sys_graphics_set_vec4_param(SYS_GRAPHICS_PARAM_BLEND_COLOR, color);
}

/*-----------------------------------------------------------------------*/

void graphics_enable_color_write(int red, int green, int blue, int alpha)
{
    sys_graphics_set_int_param(
        SYS_GRAPHICS_PARAM_COLOR_WRITE,
        (red ? 1<<0 : 0) | (green ? 1<<1 : 0) | (blue ? 1<<2 : 0)
            | (alpha ? 1<<3 : 0));
}

/*-----------------------------------------------------------------------*/

void graphics_enable_depth_test(int on)
{
    sys_graphics_set_int_param(SYS_GRAPHICS_PARAM_DEPTH_TEST, (on != 0));
}

/*-----------------------------------------------------------------------*/

void graphics_set_depth_test_comparison(GraphicsComparisonType type)
{
    switch (type) {
      case GRAPHICS_COMPARISON_LESS:
      case GRAPHICS_COMPARISON_LESS_EQUAL:
      case GRAPHICS_COMPARISON_GREATER_EQUAL:
      case GRAPHICS_COMPARISON_GREATER:
        sys_graphics_set_int_param(
            SYS_GRAPHICS_PARAM_DEPTH_TEST_COMPARISON, type);
        return;
      case GRAPHICS_COMPARISON_FALSE:
      case GRAPHICS_COMPARISON_TRUE:
      case GRAPHICS_COMPARISON_EQUAL:
      case GRAPHICS_COMPARISON_NOT_EQUAL:
        break;  // Fall through to error message.
    }
    DLOG("Invalid comparison type: %d", type);
}

/*-----------------------------------------------------------------------*/

void graphics_enable_depth_write(int on)
{
    sys_graphics_set_int_param(SYS_GRAPHICS_PARAM_DEPTH_WRITE, (on != 0));
}

/*-----------------------------------------------------------------------*/

void graphics_set_face_cull(GraphicsFaceCullMode mode)
{
    sys_graphics_set_int_param(SYS_GRAPHICS_PARAM_FACE_CULL,
                               (mode != GRAPHICS_FACE_CULL_NONE));
    sys_graphics_set_int_param(SYS_GRAPHICS_PARAM_FACE_CULL_CW,
                               (mode == GRAPHICS_FACE_CULL_CW));
}

/*-----------------------------------------------------------------------*/

void graphics_set_fixed_color(const Vector4f *color)
{
    if (UNLIKELY(!color)) {
        DLOG("color == NULL");
        return;
    }

    sys_graphics_set_vec4_param(SYS_GRAPHICS_PARAM_FIXED_COLOR, color);
}

/*-----------------------------------------------------------------------*/

void graphics_enable_fog(int on)
{
    sys_graphics_set_int_param(SYS_GRAPHICS_PARAM_FOG, (on != 0));
}

/*-----------------------------------------------------------------------*/

void graphics_set_fog_start(float distance)
{
    sys_graphics_set_float_param(SYS_GRAPHICS_PARAM_FOG_START, distance);
}

/*-----------------------------------------------------------------------*/

void graphics_set_fog_end(float distance)
{
    sys_graphics_set_float_param(SYS_GRAPHICS_PARAM_FOG_END, distance);
}

/*-----------------------------------------------------------------------*/

void graphics_set_fog_color(const Vector4f *color)
{
    if (UNLIKELY(!color)) {
        DLOG("color == NULL");
        return;
    }

    sys_graphics_set_vec4_param(SYS_GRAPHICS_PARAM_FOG_COLOR, color);
}

/*-----------------------------------------------------------------------*/

void graphics_set_point_size(float size)
{
    sys_graphics_set_float_param(SYS_GRAPHICS_PARAM_POINT_SIZE, size);
}

/*-----------------------------------------------------------------------*/

float graphics_max_point_size(void)
{
    return sys_graphics_max_point_size();
}

/*-----------------------------------------------------------------------*/

void graphics_enable_stencil_test(int on)
{
    sys_graphics_set_int_param(SYS_GRAPHICS_PARAM_STENCIL_TEST, (on != 0));
}

/*-----------------------------------------------------------------------*/

void graphics_set_stencil_comparison(
    GraphicsComparisonType type, unsigned int reference, unsigned int mask)
{
    /* This switch statement is just a check for validity. */
    switch (type) {
      case GRAPHICS_COMPARISON_FALSE:
      case GRAPHICS_COMPARISON_TRUE:
      case GRAPHICS_COMPARISON_EQUAL:
      case GRAPHICS_COMPARISON_NOT_EQUAL:
      case GRAPHICS_COMPARISON_LESS:
      case GRAPHICS_COMPARISON_LESS_EQUAL:
      case GRAPHICS_COMPARISON_GREATER_EQUAL:
      case GRAPHICS_COMPARISON_GREATER:
        sys_graphics_set_int_param(
            SYS_GRAPHICS_PARAM_STENCIL_COMPARISON, type);
        sys_graphics_set_int_param(
            SYS_GRAPHICS_PARAM_STENCIL_REFERENCE, reference);
        sys_graphics_set_int_param(
            SYS_GRAPHICS_PARAM_STENCIL_MASK, mask);
        return;
    }
    DLOG("Invalid comparison type: %d", type);
}

/*-----------------------------------------------------------------------*/

void graphics_set_stencil_operations(
    GraphicsStencilOp sfail, GraphicsStencilOp dfail, GraphicsStencilOp dpass)
{
    /* These switch statements are just checks for validity. */
    switch (sfail) {
      case GRAPHICS_STENCIL_KEEP:
      case GRAPHICS_STENCIL_CLEAR:
      case GRAPHICS_STENCIL_REPLACE:
      case GRAPHICS_STENCIL_INCR:
      case GRAPHICS_STENCIL_DECR:
      case GRAPHICS_STENCIL_INVERT:
        switch (dfail) {
          case GRAPHICS_STENCIL_KEEP:
          case GRAPHICS_STENCIL_CLEAR:
          case GRAPHICS_STENCIL_REPLACE:
          case GRAPHICS_STENCIL_INCR:
          case GRAPHICS_STENCIL_DECR:
          case GRAPHICS_STENCIL_INVERT:
            switch (dpass) {
              case GRAPHICS_STENCIL_KEEP:
              case GRAPHICS_STENCIL_CLEAR:
              case GRAPHICS_STENCIL_REPLACE:
              case GRAPHICS_STENCIL_INCR:
              case GRAPHICS_STENCIL_DECR:
              case GRAPHICS_STENCIL_INVERT:
                sys_graphics_set_int_param(
                    SYS_GRAPHICS_PARAM_STENCIL_OP_SFAIL, sfail);
                sys_graphics_set_int_param(
                    SYS_GRAPHICS_PARAM_STENCIL_OP_DFAIL, dfail);
                sys_graphics_set_int_param(
                    SYS_GRAPHICS_PARAM_STENCIL_OP_DPASS, dpass);
                return;
            }
        }
    }
    DLOG("Invalid operations: %d %d %d", sfail, dfail, dpass);
}

/*-----------------------------------------------------------------------*/

void graphics_set_texture_offset(const Vector2f *offset)
{
    if (UNLIKELY(!offset)) {
        DLOG("offset == NULL");
        return;
    }

    sys_graphics_set_vec2_param(SYS_GRAPHICS_PARAM_TEXTURE_OFFSET, offset);
}

/*************************************************************************/
/*************************************************************************/
