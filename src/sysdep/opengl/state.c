/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/opengl/state.c: Render state management for OpenGL-based
 * platforms.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/graphics.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/opengl/internal.h"
#include "src/sysdep/opengl/opengl.h"

/*************************************************************************/
/************** Exported data (local to the OpenGL library) **************/
/*************************************************************************/

SysTexture *opengl_current_texture;
GLuint opengl_current_texture_id;

Vector4f opengl_primitive_color;
uint8_t opengl_primitive_color_used;

uint8_t opengl_framebuffer_changed;

uint8_t opengl_shader_objects_enabled;

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Current projection, view, and model transformation matrices. */
static Matrix4f projection_matrix, view_matrix, model_matrix;

/* Combined transformation matrix. */
static Matrix4f transformation_matrix;

/* Current texture offset. */
static Vector2f texture_offset;

/* Flags indicating whether various rendering parameters have changed since
 * they were last written to shader uniforms or GL state. */
static uint8_t projection_matrix_changed, view_matrix_changed,
               model_matrix_changed, transformation_matrix_changed,
               texture_offset_changed, primitive_color_changed;

/* Flag indicating whether the current model matrix is the identity matrix.
 * Used to avoid an extra matrix multiplication when not necessary. */
static uint8_t model_matrix_is_identity;

/* Flag indicating whether the current texture offset is (0,0). */
static uint8_t texture_offset_is_zero;

/* Current alpha test state. */
static uint8_t alpha_test_enabled;
static uint8_t alpha_reference_changed;
static GraphicsComparisonType alpha_test_comparison;
static float alpha_test_reference;

/* Current blend enable flags and parameters. */
static uint8_t blend_enabled;
static uint8_t blend_alpha_enabled;
static GLenum blend_op;
static GLenum blend_src, blend_dest, blend_alpha_src, blend_alpha_dest;
static Vector4f blend_color;

/* Current clipping (scissor) state (coordinates are as passed to
 * sys_graphics_set_clip_region()). */
static uint8_t clip_enabled;
static int clip_left, clip_bottom, clip_width, clip_height;

/* Current color buffer write flags. */
static uint8_t color_write_red;
static uint8_t color_write_green;
static uint8_t color_write_blue;
static uint8_t color_write_alpha;

/* Current depth range. */
static float depth_near, depth_far;

/* Current depth test and write flags. */
static uint8_t depth_test_enabled, depth_write_enabled;
static GLenum depth_test_comparison;

/* Current face culling state. */
static uint8_t face_cull_enabled;
static uint8_t face_cull_cw;  // Cull clockwise faces? (false = cull CCW faces)

/* Current fog state.  The start and end values are depths, not distances
 * (or more accurately, they are distances along the Z-axis in eye
 * coordinate space). */
static uint8_t fog_enabled, fog_changed, fog_transform_changed;
static float fog_start, fog_end;
static Vector4f fog_color;
static Vector2f fog_params;  // Precomputed fog constants for shaders.
static Vector4f fog_transform;  // Z column of model-view matrix for shaders.

/* Current point size. */
static uint8_t point_size_changed;
static float point_size;

/* Current stencil parameters. */
static uint8_t stencil_test_enabled;
static GLenum stencil_comparison;
static unsigned int stencil_reference, stencil_mask;
static GLenum stencil_op_sfail, stencil_op_dfail, stencil_op_dpass;

/* Current viewport parameters (as passed to sys_graphics_set_viewport()). */
static int viewport_left, viewport_bottom, viewport_width, viewport_height;

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * convert_blend_factor:  Return the OpenGL enumeration value corresponding
 * to the given SIL blend factor.
 *
 * [Parameters]
 *     factor: SIL blend factor constant (GRAPHICS_BLEND_*).
 * [Return value]
 *     Corresponding OpenGL enumeration value, or zero if factor is invalid.
 */
static GLenum convert_blend_factor(GraphicsBlendFactor factor);

/**
 * convert_comparison:  Return the OpenGL enumeration value corresponding
 * to the given SIL comparison type.
 *
 * [Parameters]
 *     type: SIL comparison type constant (GRAPHICS_COMPARISON_*).
 * [Return value]
 *     Corresponding OpenGL enumeration value, or zero if type is invalid.
 */
static GLenum convert_comparison(GraphicsComparisonType type);

/**
 * convert_stencil_op:  Return the OpenGL enumeration value corresponding
 * to the given SIL stencil operation.
 *
 * [Parameters]
 *     operation: SIL stencil operation constant (GRAPHICS_STENCIL_*).
 * [Return value]
 *     Corresponding OpenGL enumeration value, or zero if operation is invalid.
 */
static GLenum convert_stencil_op(GraphicsStencilOp operation);

/**
 * update_blend:  Update GL state for new blend parameters.
 */
static void update_blend(GLenum op, GLenum src, GLenum dest,
                         GLenum alpha_src, GLenum alpha_dest);

/**
 * update_shader_fog_params:  Update the exported fog parameter values
 * used by shaders based on the current fog settings.
 */
static void update_fog_params(void);

/*************************************************************************/
/***************** Interface: Render state manipulation ******************/
/*************************************************************************/

void sys_(graphics_set_viewport)(int left, int bottom, int width, int height)
{
    viewport_left   = left;
    viewport_bottom = bottom;
    viewport_width  = width;
    viewport_height = height;
    opengl_apply_viewport();
}

/*-----------------------------------------------------------------------*/

void sys_(graphics_set_clip_region)(int left, int bottom, int width, int height)
{
    clip_left   = left;
    clip_bottom = bottom;
    clip_width  = width;
    clip_height = height;
    opengl_apply_clip_region();
}

/*-----------------------------------------------------------------------*/

void sys_(graphics_set_depth_range)(float near, float far)
{
    depth_near = near;
    depth_far  = far;
#ifdef SIL_OPENGL_ES
    glDepthRangef(near, far);
#else
    glDepthRange(near, far);
#endif
}

/*-----------------------------------------------------------------------*/

int sys_(graphics_set_blend)(int operation, int src_factor, int dest_factor)
{
    GLenum op = GL_INVALID_ENUM;  // Detect invalid input.
    switch ((GraphicsBlendOperation)operation) {
        case GRAPHICS_BLEND_ADD:  op = GL_FUNC_ADD;              break;
        case GRAPHICS_BLEND_SUB:  op = GL_FUNC_SUBTRACT;         break;
        case GRAPHICS_BLEND_RSUB: op = GL_FUNC_REVERSE_SUBTRACT; break;
    }
    ASSERT(op != GL_INVALID_ENUM, return 0);

    const GLenum src = convert_blend_factor(src_factor);
    const GLenum dest = convert_blend_factor(dest_factor);
    ASSERT(src != GL_INVALID_ENUM, return 0);
    ASSERT(dest != GL_INVALID_ENUM, return 0);

    update_blend(op, src, dest,
                 blend_alpha_enabled ? blend_alpha_src : src,
                 blend_alpha_enabled ? blend_alpha_dest : dest);
    return 1;
}

/*-----------------------------------------------------------------------*/

int sys_(graphics_set_blend_alpha)(int enable, int src_factor, int dest_factor)
{
    const GLenum src = convert_blend_factor(src_factor);
    const GLenum dest = convert_blend_factor(dest_factor);
    ASSERT(src != GL_INVALID_ENUM, return 0);
    ASSERT(dest != GL_INVALID_ENUM, return 0);

    blend_alpha_enabled = enable;
    update_blend(blend_op, blend_src, blend_dest,
                 blend_alpha_enabled ? src : blend_src,
                 blend_alpha_enabled ? dest : blend_dest);
    return 1;
}

/*-----------------------------------------------------------------------*/

void sys_(graphics_set_int_param)(SysGraphicsParam id, int value)
{
    switch (id) {

      case SYS_GRAPHICS_PARAM_ALPHA_TEST:
        alpha_test_enabled = (value != 0);
        return;

      case SYS_GRAPHICS_PARAM_ALPHA_TEST_COMPARISON:
        alpha_test_comparison = value;
        return;

      case SYS_GRAPHICS_PARAM_CLIP:
        value = (value != 0);
        if (value != clip_enabled) {
            clip_enabled = value;
            if (clip_enabled) {
                glEnable(GL_SCISSOR_TEST);
            } else {
                glDisable(GL_SCISSOR_TEST);
            }
        }
        return;

      case SYS_GRAPHICS_PARAM_COLOR_WRITE: {
        const int red   = (value & 1<<0) != 0;
        const int green = (value & 1<<1) != 0;
        const int blue  = (value & 1<<2) != 0;
        const int alpha = (value & 1<<3) != 0;
        if (red != color_write_red || green != color_write_green
         || blue != color_write_blue || alpha != color_write_alpha) {
            color_write_red   = red;
            color_write_green = green;
            color_write_blue  = blue;
            color_write_alpha = alpha;
            glColorMask(color_write_red, color_write_green, color_write_blue,
                        color_write_alpha);
        }
        return;
      }

      case SYS_GRAPHICS_PARAM_DEPTH_TEST:
        value = (value != 0);
        if (value != depth_test_enabled) {
            depth_test_enabled = value;
            if (depth_test_enabled) {
                glEnable(GL_DEPTH_TEST);
            } else {
                glDisable(GL_DEPTH_TEST);
            }
        }
        return;

      case SYS_GRAPHICS_PARAM_DEPTH_TEST_COMPARISON:
        value = convert_comparison(value);
        if ((GLenum)value != depth_test_comparison) {
            depth_test_comparison = value;
            glDepthFunc(depth_test_comparison);
        }
        return;

      case SYS_GRAPHICS_PARAM_DEPTH_WRITE:
        value = (value != 0);
        if (value != depth_write_enabled) {
            depth_write_enabled = value;
            glDepthMask(depth_write_enabled);
        }
        return;

      case SYS_GRAPHICS_PARAM_FACE_CULL:
        value = (value != 0);
        if (value != face_cull_enabled) {
            face_cull_enabled = value;
            if (face_cull_enabled) {
                glEnable(GL_CULL_FACE);
            } else {
                glDisable(GL_CULL_FACE);
            }
        }
        return;

      case SYS_GRAPHICS_PARAM_FACE_CULL_CW:
        value = (value != 0);
        if (value != face_cull_cw) {
            face_cull_cw = value;
            glFrontFace(face_cull_cw ? GL_CCW : GL_CW);
        }
        return;

      case SYS_GRAPHICS_PARAM_FOG:
        fog_enabled = (value != 0);
        return;

      case SYS_GRAPHICS_PARAM_STENCIL_TEST:
        value = (value != 0);
        if (value != stencil_test_enabled) {
            stencil_test_enabled = value;
            if (stencil_test_enabled) {
                glEnable(GL_STENCIL_TEST);
            } else {
                glDisable(GL_STENCIL_TEST);
            }
        }
        return;

      case SYS_GRAPHICS_PARAM_STENCIL_COMPARISON:
        /* OpenGL has the relational ones backwards, so fix them up. */
        switch (value) {
          case GRAPHICS_COMPARISON_LESS:
            stencil_comparison = GL_GREATER;
            break;
          case GRAPHICS_COMPARISON_LESS_EQUAL:
            stencil_comparison = GL_GEQUAL;
            break;
          case GRAPHICS_COMPARISON_GREATER_EQUAL:
            stencil_comparison = GL_LEQUAL;
            break;
          case GRAPHICS_COMPARISON_GREATER:
            stencil_comparison = GL_LESS;
            break;
          default:
            stencil_comparison = convert_comparison(value);
            break;
        }
        ASSERT(stencil_comparison != GL_INVALID_ENUM);
        /* Value will be passed to OpenGL with STENCIL_MASK. */
        return;

      case SYS_GRAPHICS_PARAM_STENCIL_REFERENCE:
        stencil_reference = (unsigned int)value;
        /* Value will be passed to OpenGL with STENCIL_MASK. */
        return;

      case SYS_GRAPHICS_PARAM_STENCIL_MASK:
        stencil_mask = (unsigned int)value;
        glStencilFunc(stencil_comparison, stencil_reference, stencil_mask);
        return;

      case SYS_GRAPHICS_PARAM_STENCIL_OP_SFAIL:
        stencil_op_sfail = convert_stencil_op(value);
        ASSERT(stencil_op_sfail != GL_INVALID_ENUM);
        /* Value will be passed to OpenGL with STENCIL_OP_DPASS. */
        return;

      case SYS_GRAPHICS_PARAM_STENCIL_OP_DFAIL:
        stencil_op_dfail = convert_stencil_op(value);
        ASSERT(stencil_op_dfail != GL_INVALID_ENUM);
        /* Value will be passed to OpenGL with STENCIL_OP_DPASS. */
        return;

      case SYS_GRAPHICS_PARAM_STENCIL_OP_DPASS:
        stencil_op_dpass = convert_stencil_op(value);
        ASSERT(stencil_op_dpass != GL_INVALID_ENUM);
        glStencilOp(stencil_op_sfail, stencil_op_dfail, stencil_op_dpass);
        return;

      case SYS_GRAPHICS_PARAM_PROJECTION_MATRIX:
      case SYS_GRAPHICS_PARAM_VIEW_MATRIX:
      case SYS_GRAPHICS_PARAM_MODEL_MATRIX:
      case SYS_GRAPHICS_PARAM_ALPHA_REFERENCE:
      case SYS_GRAPHICS_PARAM_BLEND_COLOR:
      case SYS_GRAPHICS_PARAM_FIXED_COLOR:
      case SYS_GRAPHICS_PARAM_FOG_START:
      case SYS_GRAPHICS_PARAM_FOG_END:
      case SYS_GRAPHICS_PARAM_FOG_COLOR:
      case SYS_GRAPHICS_PARAM_POINT_SIZE:
      case SYS_GRAPHICS_PARAM_TEXTURE_OFFSET:
        ASSERT(!"wrong type", return);  // NOTREACHED

    }  // switch (id)

    ASSERT(!"unreachable");  // NOTREACHED
}

/*-----------------------------------------------------------------------*/

void sys_(graphics_set_float_param)(SysGraphicsParam id, float value)
{
    switch (id) {

      case SYS_GRAPHICS_PARAM_ALPHA_REFERENCE:
        value = bound(value, 0.0f, 1.0f);
        if (value != alpha_test_reference) {
            alpha_test_reference = value;
            alpha_reference_changed = 1;
        }
        return;

      case SYS_GRAPHICS_PARAM_FOG_START:
        if (value != fog_start) {
            fog_start = value;
            fog_changed = 1;
        }
        return;

      case SYS_GRAPHICS_PARAM_FOG_END:
        if (value != fog_end) {
            fog_end = value;
            fog_changed = 1;
        }
        return;

      case SYS_GRAPHICS_PARAM_POINT_SIZE:
        if (value != point_size) {
            point_size = value;
            point_size_changed = 1;
        }
        return;

      case SYS_GRAPHICS_PARAM_PROJECTION_MATRIX:
      case SYS_GRAPHICS_PARAM_VIEW_MATRIX:
      case SYS_GRAPHICS_PARAM_MODEL_MATRIX:
      case SYS_GRAPHICS_PARAM_ALPHA_TEST:
      case SYS_GRAPHICS_PARAM_ALPHA_TEST_COMPARISON:
      case SYS_GRAPHICS_PARAM_BLEND_COLOR:
      case SYS_GRAPHICS_PARAM_CLIP:
      case SYS_GRAPHICS_PARAM_COLOR_WRITE:
      case SYS_GRAPHICS_PARAM_DEPTH_TEST:
      case SYS_GRAPHICS_PARAM_DEPTH_TEST_COMPARISON:
      case SYS_GRAPHICS_PARAM_DEPTH_WRITE:
      case SYS_GRAPHICS_PARAM_FACE_CULL:
      case SYS_GRAPHICS_PARAM_FACE_CULL_CW:
      case SYS_GRAPHICS_PARAM_FIXED_COLOR:
      case SYS_GRAPHICS_PARAM_FOG:
      case SYS_GRAPHICS_PARAM_FOG_COLOR:
      case SYS_GRAPHICS_PARAM_STENCIL_TEST:
      case SYS_GRAPHICS_PARAM_STENCIL_COMPARISON:
      case SYS_GRAPHICS_PARAM_STENCIL_REFERENCE:
      case SYS_GRAPHICS_PARAM_STENCIL_MASK:
      case SYS_GRAPHICS_PARAM_STENCIL_OP_SFAIL:
      case SYS_GRAPHICS_PARAM_STENCIL_OP_DFAIL:
      case SYS_GRAPHICS_PARAM_STENCIL_OP_DPASS:
      case SYS_GRAPHICS_PARAM_TEXTURE_OFFSET:
        ASSERT(!"wrong type", return);  // NOTREACHED

    }  // switch (id)

    ASSERT(!"unreachable");  // NOTREACHED
}

/*-----------------------------------------------------------------------*/

void sys_(graphics_set_vec2_param)(SysGraphicsParam id, const Vector2f *value)
{
    switch (id) {

      case SYS_GRAPHICS_PARAM_TEXTURE_OFFSET:
        if (value->x != texture_offset.x || value->y != texture_offset.y) {
            texture_offset = *value;
            texture_offset_is_zero = !vec2_is_nonzero(texture_offset);
            texture_offset_changed = 1;
        }
        return;

      case SYS_GRAPHICS_PARAM_PROJECTION_MATRIX:
      case SYS_GRAPHICS_PARAM_VIEW_MATRIX:
      case SYS_GRAPHICS_PARAM_MODEL_MATRIX:
      case SYS_GRAPHICS_PARAM_ALPHA_TEST:
      case SYS_GRAPHICS_PARAM_ALPHA_TEST_COMPARISON:
      case SYS_GRAPHICS_PARAM_ALPHA_REFERENCE:
      case SYS_GRAPHICS_PARAM_BLEND_COLOR:
      case SYS_GRAPHICS_PARAM_CLIP:
      case SYS_GRAPHICS_PARAM_COLOR_WRITE:
      case SYS_GRAPHICS_PARAM_DEPTH_TEST:
      case SYS_GRAPHICS_PARAM_DEPTH_TEST_COMPARISON:
      case SYS_GRAPHICS_PARAM_DEPTH_WRITE:
      case SYS_GRAPHICS_PARAM_FACE_CULL:
      case SYS_GRAPHICS_PARAM_FACE_CULL_CW:
      case SYS_GRAPHICS_PARAM_FIXED_COLOR:
      case SYS_GRAPHICS_PARAM_FOG:
      case SYS_GRAPHICS_PARAM_FOG_START:
      case SYS_GRAPHICS_PARAM_FOG_END:
      case SYS_GRAPHICS_PARAM_FOG_COLOR:
      case SYS_GRAPHICS_PARAM_POINT_SIZE:
      case SYS_GRAPHICS_PARAM_STENCIL_TEST:
      case SYS_GRAPHICS_PARAM_STENCIL_COMPARISON:
      case SYS_GRAPHICS_PARAM_STENCIL_REFERENCE:
      case SYS_GRAPHICS_PARAM_STENCIL_MASK:
      case SYS_GRAPHICS_PARAM_STENCIL_OP_SFAIL:
      case SYS_GRAPHICS_PARAM_STENCIL_OP_DFAIL:
      case SYS_GRAPHICS_PARAM_STENCIL_OP_DPASS:
        ASSERT(!"wrong type", return);  // NOTREACHED

    }  // switch (id)

    ASSERT(!"unreachable");  // NOTREACHED
}

/*-----------------------------------------------------------------------*/

void sys_(graphics_set_vec4_param)(SysGraphicsParam id, const Vector4f *value)
{
    switch (id) {

      case SYS_GRAPHICS_PARAM_BLEND_COLOR:
        if (value->x != blend_color.x
         || value->y != blend_color.y
         || value->z != blend_color.z
         || value->w != blend_color.w) {
            blend_color.x = bound(value->x, 0.0f, 1.0f);
            blend_color.y = bound(value->y, 0.0f, 1.0f);
            blend_color.z = bound(value->z, 0.0f, 1.0f);
            blend_color.w = bound(value->w, 0.0f, 1.0f);
            glBlendColor(blend_color.x, blend_color.y, blend_color.z,
                         blend_color.w);
        }
        return;

      case SYS_GRAPHICS_PARAM_FIXED_COLOR:
        if (value->x != opengl_primitive_color.x
         || value->y != opengl_primitive_color.y
         || value->z != opengl_primitive_color.z
         || value->w != opengl_primitive_color.w) {
            opengl_primitive_color.x = bound(value->x, 0.0f, 1.0f);
            opengl_primitive_color.y = bound(value->y, 0.0f, 1.0f);
            opengl_primitive_color.z = bound(value->z, 0.0f, 1.0f);
            opengl_primitive_color.w = bound(value->w, 0.0f, 1.0f);
            opengl_primitive_color_used = (opengl_primitive_color.x != 1
                                        || opengl_primitive_color.y != 1
                                        || opengl_primitive_color.z != 1
                                        || opengl_primitive_color.w != 1);
            primitive_color_changed = 1;
        }
        return;

      case SYS_GRAPHICS_PARAM_FOG_COLOR:
        if (value->x != fog_color.x
         || value->y != fog_color.y
         || value->z != fog_color.z
         || value->w != fog_color.w) {
            fog_color.x = bound(value->x, 0.0f, 1.0f);
            fog_color.y = bound(value->y, 0.0f, 1.0f);
            fog_color.z = bound(value->z, 0.0f, 1.0f);
            fog_color.w = bound(value->w, 0.0f, 1.0f);
            fog_changed = 1;
        }
        return;

      case SYS_GRAPHICS_PARAM_PROJECTION_MATRIX:
      case SYS_GRAPHICS_PARAM_VIEW_MATRIX:
      case SYS_GRAPHICS_PARAM_MODEL_MATRIX:
      case SYS_GRAPHICS_PARAM_ALPHA_TEST:
      case SYS_GRAPHICS_PARAM_ALPHA_TEST_COMPARISON:
      case SYS_GRAPHICS_PARAM_ALPHA_REFERENCE:
      case SYS_GRAPHICS_PARAM_CLIP:
      case SYS_GRAPHICS_PARAM_COLOR_WRITE:
      case SYS_GRAPHICS_PARAM_DEPTH_TEST:
      case SYS_GRAPHICS_PARAM_DEPTH_TEST_COMPARISON:
      case SYS_GRAPHICS_PARAM_DEPTH_WRITE:
      case SYS_GRAPHICS_PARAM_FACE_CULL:
      case SYS_GRAPHICS_PARAM_FACE_CULL_CW:
      case SYS_GRAPHICS_PARAM_FOG:
      case SYS_GRAPHICS_PARAM_FOG_START:
      case SYS_GRAPHICS_PARAM_FOG_END:
      case SYS_GRAPHICS_PARAM_POINT_SIZE:
      case SYS_GRAPHICS_PARAM_STENCIL_TEST:
      case SYS_GRAPHICS_PARAM_STENCIL_COMPARISON:
      case SYS_GRAPHICS_PARAM_STENCIL_REFERENCE:
      case SYS_GRAPHICS_PARAM_STENCIL_MASK:
      case SYS_GRAPHICS_PARAM_STENCIL_OP_SFAIL:
      case SYS_GRAPHICS_PARAM_STENCIL_OP_DFAIL:
      case SYS_GRAPHICS_PARAM_STENCIL_OP_DPASS:
      case SYS_GRAPHICS_PARAM_TEXTURE_OFFSET:
        ASSERT(!"wrong type", return);  // NOTREACHED

    }  // switch (id)

    ASSERT(!"unreachable");  // NOTREACHED
}

/*-----------------------------------------------------------------------*/

void sys_(graphics_set_matrix_param)(SysGraphicsParam id, const Matrix4f *value)
{
    switch (id) {

      case SYS_GRAPHICS_PARAM_PROJECTION_MATRIX:
        projection_matrix = *value;
        projection_matrix_changed = 1;
        return;

      case SYS_GRAPHICS_PARAM_VIEW_MATRIX:
        view_matrix = *value;
        view_matrix_changed = 1;
        fog_changed = 1;
        return;

      case SYS_GRAPHICS_PARAM_MODEL_MATRIX: {
        model_matrix = *value;
        model_matrix_changed = 1;
        model_matrix_is_identity =
            (memcmp(&model_matrix, &mat4_identity, sizeof(Matrix4f)) == 0);
        fog_changed = 1;
        return;
      }

      case SYS_GRAPHICS_PARAM_ALPHA_TEST:
      case SYS_GRAPHICS_PARAM_ALPHA_TEST_COMPARISON:
      case SYS_GRAPHICS_PARAM_ALPHA_REFERENCE:
      case SYS_GRAPHICS_PARAM_BLEND_COLOR:
      case SYS_GRAPHICS_PARAM_CLIP:
      case SYS_GRAPHICS_PARAM_COLOR_WRITE:
      case SYS_GRAPHICS_PARAM_DEPTH_TEST:
      case SYS_GRAPHICS_PARAM_DEPTH_TEST_COMPARISON:
      case SYS_GRAPHICS_PARAM_DEPTH_WRITE:
      case SYS_GRAPHICS_PARAM_FACE_CULL:
      case SYS_GRAPHICS_PARAM_FACE_CULL_CW:
      case SYS_GRAPHICS_PARAM_FIXED_COLOR:
      case SYS_GRAPHICS_PARAM_FOG:
      case SYS_GRAPHICS_PARAM_FOG_START:
      case SYS_GRAPHICS_PARAM_FOG_END:
      case SYS_GRAPHICS_PARAM_FOG_COLOR:
      case SYS_GRAPHICS_PARAM_POINT_SIZE:
      case SYS_GRAPHICS_PARAM_STENCIL_TEST:
      case SYS_GRAPHICS_PARAM_STENCIL_COMPARISON:
      case SYS_GRAPHICS_PARAM_STENCIL_REFERENCE:
      case SYS_GRAPHICS_PARAM_STENCIL_MASK:
      case SYS_GRAPHICS_PARAM_STENCIL_OP_SFAIL:
      case SYS_GRAPHICS_PARAM_STENCIL_OP_DFAIL:
      case SYS_GRAPHICS_PARAM_STENCIL_OP_DPASS:
      case SYS_GRAPHICS_PARAM_TEXTURE_OFFSET:
        ASSERT(!"wrong type", return);  // NOTREACHED

    }  // switch (id)

    ASSERT(!"unreachable");  // NOTREACHED
}

/*-----------------------------------------------------------------------*/

void sys_(graphics_get_matrix_param)(SysGraphicsParam id, Matrix4f *value_ret)
{
    switch (id) {

      case SYS_GRAPHICS_PARAM_PROJECTION_MATRIX:
        *value_ret = projection_matrix;
        return;

      case SYS_GRAPHICS_PARAM_VIEW_MATRIX:
        *value_ret = view_matrix;
        return;

      case SYS_GRAPHICS_PARAM_MODEL_MATRIX:
        *value_ret = model_matrix;
        return;

      case SYS_GRAPHICS_PARAM_ALPHA_TEST:
      case SYS_GRAPHICS_PARAM_ALPHA_TEST_COMPARISON:
      case SYS_GRAPHICS_PARAM_ALPHA_REFERENCE:
      case SYS_GRAPHICS_PARAM_BLEND_COLOR:
      case SYS_GRAPHICS_PARAM_CLIP:
      case SYS_GRAPHICS_PARAM_COLOR_WRITE:
      case SYS_GRAPHICS_PARAM_DEPTH_TEST:
      case SYS_GRAPHICS_PARAM_DEPTH_TEST_COMPARISON:
      case SYS_GRAPHICS_PARAM_DEPTH_WRITE:
      case SYS_GRAPHICS_PARAM_FACE_CULL:
      case SYS_GRAPHICS_PARAM_FACE_CULL_CW:
      case SYS_GRAPHICS_PARAM_FIXED_COLOR:
      case SYS_GRAPHICS_PARAM_FOG:
      case SYS_GRAPHICS_PARAM_FOG_START:
      case SYS_GRAPHICS_PARAM_FOG_END:
      case SYS_GRAPHICS_PARAM_FOG_COLOR:
      case SYS_GRAPHICS_PARAM_POINT_SIZE:
      case SYS_GRAPHICS_PARAM_STENCIL_TEST:
      case SYS_GRAPHICS_PARAM_STENCIL_COMPARISON:
      case SYS_GRAPHICS_PARAM_STENCIL_REFERENCE:
      case SYS_GRAPHICS_PARAM_STENCIL_MASK:
      case SYS_GRAPHICS_PARAM_STENCIL_OP_SFAIL:
      case SYS_GRAPHICS_PARAM_STENCIL_OP_DFAIL:
      case SYS_GRAPHICS_PARAM_STENCIL_OP_DPASS:
      case SYS_GRAPHICS_PARAM_TEXTURE_OFFSET:
        ASSERT(!"wrong type", return);  // NOTREACHED

    }  // switch (id)

    ASSERT(!"unreachable");  // NOTREACHED
}

/*-----------------------------------------------------------------------*/

float sys_(graphics_max_point_size)(void)
{
#ifdef SIL_OPENGL_ES
    /* This seems to be missing from the OpenGL ES spec, so play it safe. */
    return 1;
#else
    GLfloat range[2];
    glGetFloatv(GL_POINT_SIZE_RANGE, range);
    return range[1];
#endif
}

/*************************************************************************/
/******** Interface: Shader object / generated shader mode switch ********/
/*************************************************************************/

int sys_(graphics_enable_shader_objects)(void)
{
    opengl_deselect_shader();
    opengl_shader_objects_enabled = 1;
    return 1;
}

/*-----------------------------------------------------------------------*/

int sys_(graphics_disable_shader_objects)(void)
{
    sys_(shader_pipeline_apply)(NULL);
    opengl_shader_objects_enabled = 0;
    return 1;
}

/*************************************************************************/
/******************* Library-internal utility routines *******************/
/*************************************************************************/

void opengl_state_init(void)
{
    opengl_primitive_color.x = 1;
    opengl_primitive_color.y = 1;
    opengl_primitive_color.z = 1;
    opengl_primitive_color.w = 1;
    opengl_primitive_color_used = 0;

    model_matrix_is_identity = 1;
    texture_offset_is_zero = 1;
    transformation_matrix_changed = 1;
    texture_offset_changed = 1;
    opengl_apply_matrices(1);

#ifndef SIL_OPENGL_ES
    /* We can use glPointSize() even in shader-enabled versions of OpenGL,
     * but set the point size in shaders instead so we have the same
     * behavior between regular OpenGL and OpenGL ES (the latter of which
     * doesn't have glPointSize()). */
    glEnable(GL_PROGRAM_POINT_SIZE);
#endif

    projection_matrix = mat4_identity;
    view_matrix = mat4_identity;
    model_matrix = mat4_identity;
    projection_matrix_changed = view_matrix_changed = model_matrix_changed = 1;

    alpha_test_enabled = 0;
    alpha_test_comparison = GRAPHICS_COMPARISON_GREATER_EQUAL;
    alpha_reference_changed = 0;
    alpha_test_reference = 0;

    blend_enabled = 1;
    blend_alpha_enabled = 0;
    blend_op = GL_FUNC_ADD;
    blend_src = GL_SRC_ALPHA;
    blend_dest = GL_ONE_MINUS_SRC_ALPHA;
    blend_color.x = 0;
    blend_color.y = 0;
    blend_color.z = 0;
    blend_color.w = 0;
    glEnable(GL_BLEND);
    glBlendEquation(blend_op);
    glBlendFunc(blend_src, blend_dest);
    glBlendColor(blend_color.x, blend_color.y, blend_color.z, blend_color.w);

    clip_enabled = 0;
    glDisable(GL_SCISSOR_TEST);
    opengl_apply_clip_region();

    color_write_red = 1;
    color_write_green = 1;
    color_write_blue = 1;
    color_write_alpha = 1;
    glColorMask(color_write_red, color_write_green,
                color_write_blue, color_write_alpha);

    depth_near = 0;
    depth_far = 1;
    depth_test_enabled = 0;
    depth_test_comparison = GL_LESS;
    depth_write_enabled = 1;
#ifdef SIL_OPENGL_ES
    glDepthRangef(depth_near, depth_far);
#else
    glDepthRange(depth_near, depth_far);
#endif
    glDisable(GL_DEPTH_TEST);
    glDepthFunc(depth_test_comparison);
    glDepthMask(depth_write_enabled);

    glDisable(GL_DITHER);

    face_cull_enabled = 0;
    face_cull_cw = 1;
    glDisable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    fog_enabled = 0;
    fog_changed = 0;
    fog_start = 0;
    fog_end = 1;
    fog_color.x = 1;
    fog_color.y = 1;
    fog_color.z = 1;
    fog_color.w = 1;
    fog_params.x = 1;
    fog_params.y = 1;

    point_size = 1;
    point_size_changed = 0;

    stencil_test_enabled = 0;
    stencil_comparison = GL_ALWAYS;
    stencil_reference = 0;
    stencil_mask = ~0U;
    stencil_op_sfail = GL_KEEP;
    stencil_op_dfail = GL_KEEP;
    stencil_op_dpass = GL_KEEP;
    glDisable(GL_STENCIL_TEST);
    glStencilFunc(stencil_comparison, stencil_reference, stencil_mask);
    glStencilOp(stencil_op_sfail, stencil_op_dfail, stencil_op_dpass);

    opengl_current_texture = NULL;
    opengl_current_texture_id = 0;
    texture_offset.x = 0;
    texture_offset.y = 0;
    glBindTextureUnit(0, 0);

    viewport_left = 0;
    viewport_bottom = 0;
    viewport_width = opengl_window_width;
    viewport_height = opengl_window_height;
    opengl_apply_viewport();
}

/*-----------------------------------------------------------------------*/

void opengl_bind_texture(GLenum target, GLuint id)
{
    ASSERT(target == GL_TEXTURE_2D, return);  // Only GL_TEXTURE_2D supported.

    if (id != opengl_current_texture_id) {
        glBindTextureUnit(0, id);
        opengl_current_texture_id = id;
    }
}

/*-----------------------------------------------------------------------*/

void opengl_apply_viewport(void)
{
    glViewport(viewport_left, viewport_bottom, viewport_width, viewport_height);
}

/*-----------------------------------------------------------------------*/

void opengl_apply_clip_region(void)
{
    glScissor(clip_left, clip_bottom, clip_width, clip_height);
}

/*-----------------------------------------------------------------------*/

void opengl_apply_matrices(int force)
{
    if (force
     || opengl_framebuffer_changed
     || projection_matrix_changed
     || view_matrix_changed
     || model_matrix_changed) {
        if (model_matrix_is_identity) {
            mat4_mul(&transformation_matrix,
                     &view_matrix, &projection_matrix);
            fog_transform.x = view_matrix._13;
            fog_transform.y = view_matrix._23;
            fog_transform.z = view_matrix._33;
            fog_transform.w = view_matrix._43;
        } else {
            Matrix4f modelview_matrix;
            mat4_mul(&modelview_matrix, &model_matrix, &view_matrix);
            mat4_mul(&transformation_matrix,
                     &modelview_matrix, &projection_matrix);
            fog_transform.x = modelview_matrix._13;
            fog_transform.y = modelview_matrix._23;
            fog_transform.z = modelview_matrix._33;
            fog_transform.w = modelview_matrix._43;
        }
        transformation_matrix_changed = 1;
        fog_transform_changed = 1;
        opengl_framebuffer_changed = 0;
        projection_matrix_changed = 0;
        view_matrix_changed = 0;
        model_matrix_changed = 0;
    }
}

/*-----------------------------------------------------------------------*/

int opengl_apply_shader(SysPrimitive *primitive)
{
    const int shader_changed = opengl_select_shader(
        primitive, opengl_current_texture,
        primitive->texcoord_size != 0 && !texture_offset_is_zero,
        opengl_primitive_color_used, fog_enabled,
        alpha_test_enabled, alpha_test_comparison);
    if (shader_changed < 0) {
        DLOG("Failed to select shader");
        return 0;
    }

    opengl_clear_error();
    if (shader_changed || transformation_matrix_changed) {
        opengl_set_uniform_mat4(UNIFORM_TRANSFORM,
                                &transformation_matrix);
        const int error = glGetError();
        if (UNLIKELY(error != GL_NO_ERROR)) {
            DLOG("Failed to configure shader (transform): 0x%04X", error);
            return 0;
        }
        transformation_matrix_changed = 0;
    }
    if (primitive->texcoord_size != 0 && shader_changed) {
        opengl_set_uniform_int(UNIFORM_TEXTURE, 0);
        const int error = glGetError();
        if (UNLIKELY(error != GL_NO_ERROR)) {
            DLOG("Failed to configure shader (texture): 0x%04X", error);
            return 0;
        }
    }
    if ((primitive->texcoord_size != 0 && !texture_offset_is_zero)
     && (shader_changed || texture_offset_changed)) {
        opengl_set_uniform_vec2(UNIFORM_TEX_OFFSET, &texture_offset);
        const int error = glGetError();
        if (UNLIKELY(error != GL_NO_ERROR)) {
            DLOG("Failed to configure shader (tex_offset): 0x%04X", error);
            return 0;
        }
        texture_offset_changed = 0;
    }
    if (opengl_primitive_color_used
     && (shader_changed || primitive_color_changed)) {
        opengl_set_uniform_vec4(UNIFORM_FIXED_COLOR, &opengl_primitive_color);
        const int error = glGetError();
        if (UNLIKELY(error != GL_NO_ERROR)) {
            DLOG("Failed to configure shader (fixed_color): 0x%04X", error);
            return 0;
        }
        primitive_color_changed = 0;
    }
    if (fog_enabled && (shader_changed || fog_changed)) {
        update_fog_params();
        opengl_set_uniform_vec2(UNIFORM_FOG_PARAMS, &fog_params);
        int error = glGetError();
        if (UNLIKELY(error != GL_NO_ERROR)) {
            DLOG("Failed to configure shader (fog_params): 0x%04X", error);
            return 0;
        }
        opengl_set_uniform_vec4(UNIFORM_FOG_COLOR, &fog_color);
        error = glGetError();
        if (UNLIKELY(error != GL_NO_ERROR)) {
            DLOG("Failed to configure shader (fog_color): 0x%04X", error);
            return 0;
        }
        fog_changed = 0;
    }
    if (fog_enabled && (shader_changed || fog_transform_changed)) {
        opengl_set_uniform_vec4(UNIFORM_FOG_TRANSFORM, &fog_transform);
        const int error = glGetError();
        if (UNLIKELY(error != GL_NO_ERROR)) {
            DLOG("Failed to configure shader (fog_transform): 0x%04X", error);
            return 0;
        }
        fog_transform_changed = 0;
    }
    if (alpha_test_enabled && (shader_changed || alpha_reference_changed)) {
        opengl_set_uniform_float(UNIFORM_ALPHA_REF, alpha_test_reference);
        const int error = glGetError();
        if (UNLIKELY(error != GL_NO_ERROR)) {
            DLOG("Failed to configure shader (alpha_ref): 0x%04X", error);
            return 0;
        }
        alpha_reference_changed = 0;
    }
    if (primitive->type == GL_POINTS
     && (shader_changed || point_size_changed)) {
        opengl_set_uniform_float(UNIFORM_POINT_SIZE, point_size);
        const int error = glGetError();
        if (UNLIKELY(error != GL_NO_ERROR)) {
            DLOG("Failed to configure shader (point_size): 0x%04X", error);
            return 0;
        }
        point_size_changed = 0;
    }

    return 1;
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static GLenum convert_blend_factor(GraphicsBlendFactor factor)
{
    switch (factor) {
        case GRAPHICS_BLEND_ZERO:           return GL_ZERO;
        case GRAPHICS_BLEND_ONE:            return GL_ONE;
        case GRAPHICS_BLEND_SRC_COLOR:      return GL_SRC_COLOR;
        case GRAPHICS_BLEND_SRC_ALPHA:      return GL_SRC_ALPHA;
        case GRAPHICS_BLEND_INV_SRC_ALPHA:  return GL_ONE_MINUS_SRC_ALPHA;
        case GRAPHICS_BLEND_DEST_COLOR:     return GL_DST_COLOR;
        case GRAPHICS_BLEND_DEST_ALPHA:     return GL_DST_ALPHA;
        case GRAPHICS_BLEND_INV_DEST_ALPHA: return GL_ONE_MINUS_DST_ALPHA;
        case GRAPHICS_BLEND_CONSTANT:       return GL_CONSTANT_COLOR;
        case GRAPHICS_BLEND_INV_CONSTANT:   return GL_ONE_MINUS_CONSTANT_COLOR;
    }
    ASSERT(!"Invalid blend factor", return GL_INVALID_ENUM);  // NOTREACHED
}

/*-----------------------------------------------------------------------*/

static GLenum convert_comparison(GraphicsComparisonType type)
{
    switch (type) {
        case GRAPHICS_COMPARISON_TRUE:          return GL_ALWAYS;
        case GRAPHICS_COMPARISON_FALSE:         return GL_NEVER;
        case GRAPHICS_COMPARISON_EQUAL:         return GL_EQUAL;
        case GRAPHICS_COMPARISON_NOT_EQUAL:     return GL_NOTEQUAL;
        case GRAPHICS_COMPARISON_LESS:          return GL_LESS;
        case GRAPHICS_COMPARISON_LESS_EQUAL:    return GL_LEQUAL;
        case GRAPHICS_COMPARISON_GREATER_EQUAL: return GL_GEQUAL;
        case GRAPHICS_COMPARISON_GREATER:       return GL_GREATER;
    }
    ASSERT(!"Invalid comparison type", return GL_INVALID_ENUM);  // NOTREACHED
}

/*-----------------------------------------------------------------------*/

static GLenum convert_stencil_op(GraphicsStencilOp operation)
{
    switch (operation) {
        case GRAPHICS_STENCIL_KEEP:    return GL_KEEP;
        case GRAPHICS_STENCIL_CLEAR:   return GL_ZERO;
        case GRAPHICS_STENCIL_REPLACE: return GL_REPLACE;
        case GRAPHICS_STENCIL_INCR:    return GL_INCR;
        case GRAPHICS_STENCIL_DECR:    return GL_DECR;
        case GRAPHICS_STENCIL_INVERT:  return GL_INVERT;
    }
    ASSERT(!"Invalid stencil op", return GL_INVALID_ENUM);  // NOTREACHED
}

/*-----------------------------------------------------------------------*/

static void update_blend(GLenum op, GLenum src, GLenum dest,
                         GLenum alpha_src, GLenum alpha_dest)
{
    if ((op == GL_FUNC_ADD || op == GL_FUNC_SUBTRACT)
     && src == GL_ONE && dest == GL_ZERO
     && alpha_src == GL_ONE && alpha_dest == GL_ZERO)
    {
        if (blend_enabled) {
            glDisable(GL_BLEND);
            blend_enabled = 0;
        }
    } else {
        if (!blend_enabled) {
            glEnable(GL_BLEND);
            blend_enabled = 1;
        }
    }
    if (op != blend_op) {
        glBlendEquation(op);
        blend_op = op;
    }
    if (src != blend_src
     || dest != blend_dest
     || alpha_src != blend_alpha_src
     || alpha_dest != blend_alpha_dest) {
        glBlendFuncSeparate(src, dest, alpha_src, alpha_dest);
        blend_src = src;
        blend_dest = dest;
        blend_alpha_src = alpha_src;
        blend_alpha_dest = alpha_dest;
    }
}

/*-----------------------------------------------------------------------*/

static void update_fog_params(void)
{
    /*
     * The function for linear fog (see section 3.10, "Fog", in the OpenGL
     * 2.1 specification) is f = (end - c) / (end - start), where c is the
     * eye-coordinate distance _along the Z-axis_ (the OpenGL spec is not
     * clear on this, but this is what real-world implementations do) from
     * the origin to the fragment in question.  Note that this "f" is the
     * scale factor for the original fragment color, not the fog color.
     *
     * The Z distance to the fragment can be calculated by just taking the
     * dot product of the local coordinate and the Z column of the
     * model-view transformation matrix, so we pass that column to the
     * shader in the fog_transform uniform.  To handle fog range, we invert
     * and rearrange the fog formula as follows:
     *
     * f' = (c - start) / (end - start)  [scale factor for fog color]
     *    = (c / (end - start)) - (start / (end - start))
     *
     * and set fog_params.x to (1 / (end - start)) and fog_params.y to
     * (start / (end - start)) to slightly reduce the complexity of
     * computations in the shader.
     */
    fog_params.x = 1.0f / (fog_end - fog_start);
    fog_params.y = fog_start / (fog_end - fog_start);
}

/*************************************************************************/
/*************************************************************************/
