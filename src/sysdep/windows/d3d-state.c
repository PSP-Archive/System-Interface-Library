/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/windows/d3d-state.c: Direct3D rendering state management
 * functionality.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/graphics.h"
#include "src/math.h"
#include "src/sysdep.h"
#include "src/sysdep/windows/d3d.h"
#include "src/sysdep/windows/d3d-internal.h"
#include "src/sysdep/windows/internal.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Size of the default render target (output window). */
static int default_rt_width, default_rt_height;

/* Current Direct3D state settings. */
static D3D11_VIEWPORT viewport;
static D3D11_RECT clip_region;
static D3D11_RASTERIZER_DESC raster;
static uint8_t depth_write;
static D3D11_BLEND_DESC blend;
static uint8_t blend_separate_alpha;
static Vector4f blend_color;
static D3D11_DEPTH_STENCIL_DESC depthstencil;
static uint8_t stencil_ref;
static uint8_t viewport_dirty;
static uint8_t clip_region_dirty;
static uint8_t raster_dirty;
static uint8_t blend_dirty;
static uint8_t depthstencil_dirty;

/* Coordinate transformation matrices. */
static uint8_t transform_matrix_dirty;
static uint8_t model_matrix_is_identity;
static Matrix4f projection_matrix;
static Matrix4f view_matrix;
static Matrix4f model_matrix;

/* Data blocks for shaders. */
static uint8_t vs_data_dirty;
static uint8_t ps_data_dirty;
static D3DVertexUniformBlock vs_data;
static D3DPixelUniformBlock ps_data;

/* Miscellaneous state. */
static uint8_t alpha_test_enabled;
static GraphicsComparisonType alpha_comparison;
static uint8_t fixed_color_used;  // Is it something other than (1,1,1,1)?
static uint8_t fog_enabled;
static uint8_t fog_range_dirty;
static float fog_start, fog_end;
static uint8_t tex_offset_used;  // Is it something other than (0,0)?

/* Shader pipeline for d3d_state_safe_clear(). */
static D3DSysShaderPipeline *safe_clear_pipeline;
static int safe_clear_color_uniform;

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * convert_blend_factor:  Convert a SIL blend factor constant to the
 * corresponding Direct3D constant.  If alpha_only is true, convert
 * *_COLOR factors to *_ALPHA (for setting alpha channel blend state).
 */
static CONST_FUNCTION D3D11_BLEND convert_blend_factor(
    GraphicsBlendFactor factor, int alpha_only);

/**
 * convert_comparison:  Convert a SIL graphics comparison constant to the
 * corresponding Direct3D constant.
 */
static CONST_FUNCTION D3D11_COMPARISON_FUNC convert_comparison(
    GraphicsComparisonType type);

/**
 * convert_stencil_op:  Convert a SIL stencil operation constant to the
 * corresponding Direct3D constant.
 */
static CONST_FUNCTION D3D11_STENCIL_OP convert_stencil_op(
    GraphicsStencilOp operation);

/*************************************************************************/
/*********************** sysdep interface routines ***********************/
/*************************************************************************/

void d3d_sys_graphics_set_viewport(int left, int bottom, int width, int height)
{
    /* For the default render target, we have to flip the Y coordinate
     * because Direct3D measures from the top of the window, rather than
     * from the bottom like everybody else does. */
    int y_base;
    if (d3d_get_current_framebuffer()) {
        y_base = bottom;
    } else {
        y_base = default_rt_height - (bottom + height);
    }

    if (viewport.TopLeftX != left
     || viewport.TopLeftY != y_base
     || viewport.Width != width
     || viewport.Height != height) {
        viewport.TopLeftX = left;
        viewport.TopLeftY = y_base;
        viewport.Width = width;
        viewport.Height = height;
        viewport_dirty = 1;
    }
}

/*-----------------------------------------------------------------------*/

void d3d_sys_graphics_set_clip_region(int left, int bottom,
                                      int width, int height)
{
    /* As for viewport setting. */
    int y_base;
    if (d3d_get_current_framebuffer()) {
        y_base = bottom;
    } else {
        y_base = default_rt_height - (bottom + height);
    }

    if (clip_region.left != left
     || clip_region.top != y_base
     || clip_region.right != left + width
     || clip_region.bottom != y_base + height) {
        clip_region.left = left;
        clip_region.top = y_base;
        clip_region.right = left + width;
        clip_region.bottom = y_base + height;
        clip_region_dirty = 1;
    }
}

/*-----------------------------------------------------------------------*/

void d3d_sys_graphics_set_depth_range(float near, float far)
{
    if (viewport.MinDepth != near
     || viewport.MaxDepth != far) {
        viewport.MinDepth = near;
        viewport.MaxDepth = far;
        viewport_dirty = 1;
    }
}

/*-----------------------------------------------------------------------*/

int d3d_sys_graphics_set_blend(int operation, int src_factor, int dest_factor)
{
    if (operation == GRAPHICS_BLEND_ADD
     && src_factor == GRAPHICS_BLEND_ONE
     && dest_factor == GRAPHICS_BLEND_ZERO) {
        if (blend.RenderTarget[0].BlendEnable) {
            blend.RenderTarget[0].BlendEnable = FALSE;
            blend_dirty = 1;
        }
        return 1;
    }

    D3D11_BLEND_OP d3d_op = 0;
    switch ((GraphicsBlendOperation)operation) {
        case GRAPHICS_BLEND_ADD:  d3d_op = D3D11_BLEND_OP_ADD;          break;
        case GRAPHICS_BLEND_SUB:  d3d_op = D3D11_BLEND_OP_SUBTRACT;     break;
        case GRAPHICS_BLEND_RSUB: d3d_op = D3D11_BLEND_OP_REV_SUBTRACT; break;
    }
    ASSERT(d3d_op != 0, return 0);
    const D3D11_BLEND d3d_src = convert_blend_factor(src_factor, 0);
    const D3D11_BLEND d3d_dest = convert_blend_factor(dest_factor, 0);
    if (!blend.RenderTarget[0].BlendEnable
     || blend.RenderTarget[0].BlendOp != d3d_op
     || blend.RenderTarget[0].SrcBlend != d3d_src
     || blend.RenderTarget[0].DestBlend != d3d_dest) {
        blend.RenderTarget[0].BlendEnable = TRUE;
        blend.RenderTarget[0].SrcBlend = d3d_src;
        blend.RenderTarget[0].DestBlend = d3d_dest;
        blend.RenderTarget[0].BlendOp = d3d_op;
        if (!blend_separate_alpha) {
            blend.RenderTarget[0].SrcBlendAlpha =
                convert_blend_factor(src_factor, 1);
            blend.RenderTarget[0].DestBlendAlpha =
                convert_blend_factor(dest_factor, 1);
        }
        blend.RenderTarget[0].BlendOpAlpha = d3d_op;
        blend_dirty = 1;
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

int d3d_sys_graphics_set_blend_alpha(int enable, int src_factor, int dest_factor)
{
    blend_separate_alpha = enable;

    D3D11_BLEND src, dest;
    if (enable) {
        src = convert_blend_factor(src_factor, 1);
        dest = convert_blend_factor(dest_factor, 1);
    } else {
        src = blend.RenderTarget[0].SrcBlend;
        dest = blend.RenderTarget[0].DestBlend;
    }
    if (blend.RenderTarget[0].SrcBlendAlpha != src
     || blend.RenderTarget[0].DestBlendAlpha != dest) {
        blend.RenderTarget[0].SrcBlendAlpha = src;
        blend.RenderTarget[0].DestBlendAlpha = dest;
        blend_dirty = 1;
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

void d3d_sys_graphics_set_int_param(SysGraphicsParam id, int value)
{
    switch (id) {

      case SYS_GRAPHICS_PARAM_ALPHA_TEST:
        alpha_test_enabled = (value != 0);
        return;

      case SYS_GRAPHICS_PARAM_ALPHA_TEST_COMPARISON:
        alpha_comparison = value;
        return;

      case SYS_GRAPHICS_PARAM_CLIP:
        value = (value != 0) ? TRUE : FALSE;
        if (value != raster.ScissorEnable) {
            raster.ScissorEnable = value;
            raster_dirty = 1;
        }
        return;

      case SYS_GRAPHICS_PARAM_COLOR_WRITE:
        value &= 15;
        if (value != blend.RenderTarget[0].RenderTargetWriteMask) {
            blend.RenderTarget[0].RenderTargetWriteMask = value;
            blend_dirty = 1;
        }
        return;

      case SYS_GRAPHICS_PARAM_DEPTH_TEST:
        value = (value != 0) ? TRUE : FALSE;
        if (value != depthstencil.DepthEnable) {
            depthstencil.DepthEnable = value;
            depthstencil_dirty = 1;
        }
        return;

      case SYS_GRAPHICS_PARAM_DEPTH_TEST_COMPARISON:
        value = convert_comparison(value);
        if ((D3D11_COMPARISON_FUNC)value != depthstencil.DepthFunc) {
            depthstencil.DepthFunc = value;
            depthstencil_dirty = 1;
        }
        return;

      case SYS_GRAPHICS_PARAM_DEPTH_WRITE:
        value = (value != 0);
        if (value != depth_write) {
            depth_write = value;
            depthstencil_dirty = 1;
        }
        return;

      case SYS_GRAPHICS_PARAM_FACE_CULL:
        value = (value != 0) ? D3D11_CULL_BACK : D3D11_CULL_NONE;
        if ((D3D11_CULL_MODE)value != raster.CullMode) {
            raster.CullMode = value;
            raster_dirty = 1;
        }
        return;

      case SYS_GRAPHICS_PARAM_FACE_CULL_CW:
        value = (value != 0) ? TRUE : FALSE;
        if (value != raster.FrontCounterClockwise) {
            raster.FrontCounterClockwise = value;
            raster_dirty = 1;
        }
        return;

      case SYS_GRAPHICS_PARAM_FOG:
        fog_enabled = (value != 0);
        return;

      case SYS_GRAPHICS_PARAM_STENCIL_TEST:
        value = (value != 0) ? TRUE : FALSE;
        if (value != depthstencil.StencilEnable) {
            depthstencil.StencilEnable = value;
            depthstencil_dirty = 1;
        }
        return;

      case SYS_GRAPHICS_PARAM_STENCIL_COMPARISON: {
        D3D11_COMPARISON_FUNC func;
        /* Direct3D has the relational ones backwards, so fix them up. */
        switch (value) {
          case GRAPHICS_COMPARISON_LESS:
            func = D3D11_COMPARISON_GREATER;
            break;
          case GRAPHICS_COMPARISON_LESS_EQUAL:
            func = D3D11_COMPARISON_GREATER_EQUAL;
            break;
          case GRAPHICS_COMPARISON_GREATER_EQUAL:
            func = D3D11_COMPARISON_LESS_EQUAL;
            break;
          case GRAPHICS_COMPARISON_GREATER:
            func = D3D11_COMPARISON_LESS;
            break;
          default:
            func = convert_comparison(value);
            break;
        }
        ASSERT(func != 0);
        if (func != depthstencil.FrontFace.StencilFunc) {
            depthstencil.FrontFace.StencilFunc = func;
            depthstencil.BackFace.StencilFunc = func;
            depthstencil_dirty = 1;
        }
        return;
      }  // case SYS_GRAPHICS_PARAM_STENCIL_COMPARISON

      case SYS_GRAPHICS_PARAM_STENCIL_REFERENCE:
        if ((uint8_t)value != stencil_ref) {
            stencil_ref = (uint8_t)value;
            depthstencil_dirty = 1;
        }
        return;

      case SYS_GRAPHICS_PARAM_STENCIL_MASK:
        if ((unsigned int)value != depthstencil.StencilReadMask) {
            depthstencil.StencilReadMask = (unsigned int)value;
            depthstencil.StencilWriteMask = (unsigned int)value;
            depthstencil_dirty = 1;
        }
        return;

      case SYS_GRAPHICS_PARAM_STENCIL_OP_SFAIL:
        value = convert_stencil_op(value);
        if ((D3D11_STENCIL_OP)value != depthstencil.FrontFace.StencilFailOp) {
            depthstencil.FrontFace.StencilFailOp = value;
            depthstencil.BackFace.StencilFailOp = value;
            depthstencil_dirty = 1;
        }
        return;

      case SYS_GRAPHICS_PARAM_STENCIL_OP_DFAIL:
        value = convert_stencil_op(value);
        if ((D3D11_STENCIL_OP)value != depthstencil.FrontFace.StencilDepthFailOp) {
            depthstencil.FrontFace.StencilDepthFailOp = value;
            depthstencil.BackFace.StencilDepthFailOp = value;
            depthstencil_dirty = 1;
        }
        return;

      case SYS_GRAPHICS_PARAM_STENCIL_OP_DPASS:
        value = convert_stencil_op(value);
        if ((D3D11_STENCIL_OP)value != depthstencil.FrontFace.StencilPassOp) {
            depthstencil.FrontFace.StencilPassOp = value;
            depthstencil.BackFace.StencilPassOp = value;
            depthstencil_dirty = 1;
        }
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

void d3d_sys_graphics_set_float_param(SysGraphicsParam id, float value)
{
    switch (id) {

      case SYS_GRAPHICS_PARAM_ALPHA_REFERENCE:
        value = bound(value, 0.0f, 1.0f);
        if (value != ps_data.alpha_ref) {
            ps_data.alpha_ref = value;
            ps_data_dirty = 1;
        }
        return;

      case SYS_GRAPHICS_PARAM_FOG_START:
        if (value != fog_start) {
            fog_start = value;
            fog_range_dirty = 1;
        }
        return;

      case SYS_GRAPHICS_PARAM_FOG_END:
        if (value != fog_end) {
            fog_end = value;
            fog_range_dirty = 1;
        }
        return;

      case SYS_GRAPHICS_PARAM_POINT_SIZE:
        return;  // Not supported in Direct3D.

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

void d3d_sys_graphics_set_vec2_param(SysGraphicsParam id,
                                     const Vector2f *value)
{
    switch (id) {

      case SYS_GRAPHICS_PARAM_TEXTURE_OFFSET:
        if (value->x != vs_data.tex_offset.x
         || value->y != vs_data.tex_offset.y) {
            vs_data.tex_offset = *value;
            vs_data_dirty = 1;
            tex_offset_used = vec2_is_nonzero(vs_data.tex_offset);
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

void d3d_sys_graphics_set_vec4_param(SysGraphicsParam id,
                                     const Vector4f *value)
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
            blend_dirty = 1;
        }
        return;

      case SYS_GRAPHICS_PARAM_FIXED_COLOR:
        if (value->x != vs_data.fixed_color.x
         || value->y != vs_data.fixed_color.y
         || value->z != vs_data.fixed_color.z
         || value->w != vs_data.fixed_color.w) {
            vs_data.fixed_color.x = bound(value->x, 0.0f, 1.0f);
            vs_data.fixed_color.y = bound(value->y, 0.0f, 1.0f);
            vs_data.fixed_color.z = bound(value->z, 0.0f, 1.0f);
            vs_data.fixed_color.w = bound(value->w, 0.0f, 1.0f);
            vs_data_dirty = 1;
            ps_data.fixed_color = vs_data.fixed_color;
            ps_data_dirty = 1;
            fixed_color_used = (vs_data.fixed_color.x != 1.0f
                             || vs_data.fixed_color.y != 1.0f
                             || vs_data.fixed_color.z != 1.0f
                             || vs_data.fixed_color.w != 1.0f);
        }
        return;

      case SYS_GRAPHICS_PARAM_FOG_COLOR:
        if (value->x != ps_data.fog_color.x
         || value->y != ps_data.fog_color.y
         || value->z != ps_data.fog_color.z
         || value->w != ps_data.fog_color.w) {
            ps_data.fog_color.x = bound(value->x, 0.0f, 1.0f);
            ps_data.fog_color.y = bound(value->y, 0.0f, 1.0f);
            ps_data.fog_color.z = bound(value->z, 0.0f, 1.0f);
            ps_data.fog_color.w = bound(value->w, 0.0f, 1.0f);
            ps_data_dirty = 1;
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

void d3d_sys_graphics_set_matrix_param(SysGraphicsParam id,
                                       const Matrix4f *value)
{
    switch (id) {

      case SYS_GRAPHICS_PARAM_PROJECTION_MATRIX:
        projection_matrix = *value;
        transform_matrix_dirty = 1;
        return;

      case SYS_GRAPHICS_PARAM_VIEW_MATRIX:
        view_matrix = *value;
        transform_matrix_dirty = 1;
        fog_range_dirty = 1;
        return;

      case SYS_GRAPHICS_PARAM_MODEL_MATRIX: {
        model_matrix = *value;
        transform_matrix_dirty = 1;
        model_matrix_is_identity =
            (memcmp(&model_matrix, &mat4_identity, sizeof(Matrix4f)) == 0);
        fog_range_dirty = 1;
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

void d3d_sys_graphics_get_matrix_param(SysGraphicsParam id,
                                       Matrix4f *value_ret)
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

float d3d_sys_graphics_max_point_size(void)
{
    return 1;
}

/*************************************************************************/
/******** Internal interface routines (private to Direct3D code) *********/
/*************************************************************************/

void d3d_state_init(int width, int height)
{
    default_rt_width = width;
    default_rt_height = height;

    viewport.Width = width;
    viewport.Height = height;
    viewport.MinDepth = 0;
    viewport.MaxDepth = 1;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport_dirty = 1;

    clip_region_dirty = 0;

    raster.FillMode = D3D11_FILL_SOLID;
    raster.CullMode = D3D11_CULL_NONE;
    raster.FrontCounterClockwise = TRUE;
    raster.DepthBias = 0;
    raster.DepthBiasClamp = 0;
    raster.SlopeScaledDepthBias = 0;
    raster.DepthClipEnable = TRUE;
    raster.ScissorEnable = FALSE;
    raster.MultisampleEnable = TRUE;
    raster.AntialiasedLineEnable = FALSE;
    raster_dirty = 1;

    blend.AlphaToCoverageEnable = FALSE;
    blend.IndependentBlendEnable = FALSE;
    blend.RenderTarget[0].BlendEnable = TRUE;
    blend.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blend.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
    blend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    blend_separate_alpha = 0;
    blend_color.x = 0;
    blend_color.y = 0;
    blend_color.z = 0;
    blend_color.w = 0;
    blend_dirty = 1;

    depthstencil.DepthEnable = FALSE;
    depthstencil.DepthFunc = D3D11_COMPARISON_LESS;
    depthstencil.StencilEnable = FALSE;
    depthstencil.StencilReadMask = 0xFF;
    depthstencil.StencilWriteMask = 0xFF;
    depthstencil.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    depthstencil.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    depthstencil.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    depthstencil.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    depthstencil.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    depthstencil.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    depthstencil.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    depthstencil.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    depth_write = 1;
    depthstencil_dirty = 1;

    d3d_state_apply();

    projection_matrix = mat4_identity;
    view_matrix = mat4_identity;
    model_matrix = mat4_identity;
    transform_matrix_dirty = 0;
    model_matrix_is_identity = 1;

    vs_data.transform = mat4_identity;
    vs_data.fixed_color.x = 1;
    vs_data.fixed_color.y = 1;
    vs_data.fixed_color.z = 1;
    vs_data.fixed_color.w = 1;
    vs_data.tex_offset.x = 0;
    vs_data.tex_offset.y = 0;
    vs_data_dirty = 0;

    ps_data.fixed_color.x = 1;
    ps_data.fixed_color.y = 1;
    ps_data.fixed_color.z = 1;
    ps_data.fixed_color.w = 1;
    ps_data.fog_color.x = 1;
    ps_data.fog_color.y = 1;
    ps_data.fog_color.z = 1;
    ps_data.fog_color.w = 1;
    ps_data.alpha_ref = 0;
    ps_data_dirty = 0;

    alpha_test_enabled = 0;
    alpha_comparison = GRAPHICS_COMPARISON_GREATER_EQUAL;
    fixed_color_used = 0;
    fog_enabled = 0;
    fog_range_dirty = 0;
    fog_start = 0.0f;
    fog_end = 1.0f;
    tex_offset_used = 0;
}

/*-----------------------------------------------------------------------*/

void d3d_state_cleanup(void)
{
    if (safe_clear_pipeline) {
        d3d_sys_shader_destroy(safe_clear_pipeline->vertex_shader);
        d3d_sys_shader_destroy(safe_clear_pipeline->pixel_shader);
        d3d_sys_shader_pipeline_destroy(safe_clear_pipeline);
        safe_clear_pipeline = NULL;
        safe_clear_color_uniform = 0;
    }
}

/*-----------------------------------------------------------------------*/

void d3d_state_handle_resize(int width, int height)
{
    default_rt_width = width;
    default_rt_height = height;
}

/*-----------------------------------------------------------------------*/

void d3d_state_apply(void)
{
    if (viewport_dirty) {
        ID3D11DeviceContext_RSSetViewports(d3d_context, 1, &viewport);
        viewport_dirty = 0;
    }

    if (clip_region_dirty) {
        ID3D11DeviceContext_RSSetScissorRects(d3d_context, 1, &clip_region);
        clip_region_dirty = 0;
    }

    if (raster_dirty) {
        ID3D11RasterizerState *rs;
        HRESULT result;
        if ((result = ID3D11Device_CreateRasterizerState(
                 d3d_device, &raster, &rs)) == S_OK) {
            ID3D11DeviceContext_RSSetState(d3d_context, rs);
            ID3D11RasterizerState_Release(rs);
            raster_dirty = 0;
        } else {
            DLOG("Failed to create rasterizer state object: %s",
                 d3d_strerror(result));
        }
    }

    if (blend_dirty) {
        ID3D11BlendState *bs;
        HRESULT result;
        if ((result = ID3D11Device_CreateBlendState(
                 d3d_device, &blend, &bs)) == S_OK) {
            ID3D11DeviceContext_OMSetBlendState(
                d3d_context, bs, &blend_color.x, ~0u);
            ID3D11BlendState_Release(bs);
            blend_dirty = 0;
        } else {
            DLOG("Failed to create blend state object: %s",
                 d3d_strerror(result));
        }
    }

    if (depthstencil_dirty) {
        depthstencil.DepthWriteMask =
            (depthstencil.DepthEnable && depth_write
             ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO);
        ID3D11DepthStencilState *ds;
        HRESULT result;
        if ((result = ID3D11Device_CreateDepthStencilState(
                 d3d_device, &depthstencil, &ds)) == S_OK) {
            ID3D11DeviceContext_OMSetDepthStencilState(
                d3d_context, ds, stencil_ref);
            ID3D11DepthStencilState_Release(ds);
            depthstencil_dirty = 0;
        } else {
            DLOG("Failed to create depth/stencil state object: %s",
                 d3d_strerror(result));
        }
    }
}

/*-----------------------------------------------------------------------*/

void d3d_state_set_shader(const D3DSysPrimitive *primitive)
{
    if (transform_matrix_dirty) {
        if (model_matrix_is_identity) {
            mat4_mul(&vs_data.transform, &view_matrix, &projection_matrix);
            vs_data.fog_transform.x = view_matrix._13;
            vs_data.fog_transform.y = view_matrix._23;
            vs_data.fog_transform.z = view_matrix._33;
            vs_data.fog_transform.w = view_matrix._43;
        } else {
            Matrix4f modelview_matrix;
            mat4_mul(&modelview_matrix, &model_matrix, &view_matrix);
            mat4_mul(&vs_data.transform, &modelview_matrix, &projection_matrix);
            vs_data.fog_transform.x = modelview_matrix._13;
            vs_data.fog_transform.y = modelview_matrix._23;
            vs_data.fog_transform.z = modelview_matrix._33;
            vs_data.fog_transform.w = modelview_matrix._43;
        }
        vs_data_dirty = 1;
        transform_matrix_dirty = 0;
    }

    if (fog_range_dirty) {
        /* See src/sysdep/opengl/state.c:update_fog_params() for details. */
        ps_data.fog_params.x = 1.0f / (fog_end - fog_start);
        ps_data.fog_params.y = fog_start / (fog_end - fog_start);
        ps_data_dirty = 1;
        fog_range_dirty = 0;
    }

    const int shader_changed = d3d_apply_default_shader(
        primitive, tex_offset_used, fixed_color_used, fog_enabled,
        alpha_test_enabled, alpha_comparison);

    if (shader_changed || vs_data_dirty) {
        d3d_set_default_vs_uniforms(&vs_data);
        vs_data_dirty = 0;
    }

    if (shader_changed || ps_data_dirty) {
        d3d_set_default_ps_uniforms(&ps_data);
        ps_data_dirty = 0;
    }
}

/*-----------------------------------------------------------------------*/

int d3d_state_can_clear(void)
{
    return !raster.ScissorEnable
        && blend.RenderTarget[0].RenderTargetWriteMask == D3D11_COLOR_WRITE_ENABLE_ALL;
}

/*-----------------------------------------------------------------------*/

void d3d_state_safe_clear(const Vector4f *color, const float *depth,
                          uint8_t stencil)
{
#if 0  // For reference.
    static const char safe_clear_vs_hlsl[] =
        "float4 main(float4 position: POSITION): SV_Position {\n"
        "    return position;\n"
        "}\n"
        ;
    static const char safe_clear_ps_hlsl[] =
        "cbuffer uniforms {float4 color;};\n"
        "float4 main(float4 position: SV_Position): SV_Target {\n"
        "    return color;\n"
        "}\n"
        ;
#endif

    static const char safe_clear_vs_bytecode[] =
        "\x44\x58\x42\x43\xB3\x58\xF4\xD0\xAA\x9E\x6D\xB1\xCE\xC7\x63\xFF"
        "\x0E\x70\x2E\xDA\x01\x00\x00\x00\x1C\x02\x00\x00\x06\x00\x00\x00"
        "\x38\x00\x00\x00\xA8\x00\x00\x00\xEC\x00\x00\x00\x68\x01\x00\x00"
        "\xB4\x01\x00\x00\xE8\x01\x00\x00\x41\x6F\x6E\x39\x68\x00\x00\x00"
        "\x68\x00\x00\x00\x00\x02\xFE\xFF\x40\x00\x00\x00\x28\x00\x00\x00"
        "\x00\x00\x24\x00\x00\x00\x24\x00\x00\x00\x24\x00\x00\x00\x24\x00"
        "\x01\x00\x24\x00\x00\x00\x00\x00\x00\x02\xFE\xFF\x1F\x00\x00\x02"
        "\x05\x00\x00\x80\x00\x00\x0F\x90\x05\x00\x00\x03\x00\x00\x03\x80"
        "\x00\x00\xFF\x90\x00\x00\xE4\xA0\x02\x00\x00\x03\x00\x00\x03\xC0"
        "\x00\x00\xE4\x80\x00\x00\xE4\x90\x01\x00\x00\x02\x00\x00\x0C\xC0"
        "\x00\x00\xE4\x90\xFF\xFF\x00\x00\x53\x48\x44\x52\x3C\x00\x00\x00"
        "\x40\x00\x01\x00\x0F\x00\x00\x00\x5F\x00\x00\x03\xF2\x10\x10\x00"
        "\x00\x00\x00\x00\x67\x00\x00\x04\xF2\x20\x10\x00\x00\x00\x00\x00"
        "\x01\x00\x00\x00\x36\x00\x00\x05\xF2\x20\x10\x00\x00\x00\x00\x00"
        "\x46\x1E\x10\x00\x00\x00\x00\x00\x3E\x00\x00\x01\x53\x54\x41\x54"
        "\x74\x00\x00\x00\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x52\x44\x45\x46\x44\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x1C\x00\x00\x00"
        "\x00\x04\xFE\xFF\x08\x29\x04\x00\x1C\x00\x00\x00\x4D\x69\x63\x72"
        "\x6F\x73\x6F\x66\x74\x20\x28\x52\x29\x20\x48\x4C\x53\x4C\x20\x53"
        "\x68\x61\x64\x65\x72\x20\x43\x6F\x6D\x70\x69\x6C\x65\x72\x20\x31"
        "\x30\x2E\x31\x00\x49\x53\x47\x4E\x2C\x00\x00\x00\x01\x00\x00\x00"
        "\x08\x00\x00\x00\x20\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x03\x00\x00\x00\x00\x00\x00\x00\x0F\x0F\x00\x00\x50\x4F\x53\x49"
        "\x54\x49\x4F\x4E\x00\xAB\xAB\xAB\x4F\x53\x47\x4E\x2C\x00\x00\x00"
        "\x01\x00\x00\x00\x08\x00\x00\x00\x20\x00\x00\x00\x00\x00\x00\x00"
        "\x01\x00\x00\x00\x03\x00\x00\x00\x00\x00\x00\x00\x0F\x00\x00\x00"
        "\x53\x56\x5F\x50\x6F\x73\x69\x74\x69\x6F\x6E\x00"
        ;
    static const char safe_clear_ps_bytecode[] =
        "\x44\x58\x42\x43\x2D\x9B\x6D\x76\x7F\x6A\x2E\x3C\x8E\x9B\x09\xC4"
        "\x10\xDD\x91\x88\x01\x00\x00\x00\x70\x02\x00\x00\x06\x00\x00\x00"
        "\x38\x00\x00\x00\x84\x00\x00\x00\xCC\x00\x00\x00\x48\x01\x00\x00"
        "\x08\x02\x00\x00\x3C\x02\x00\x00\x41\x6F\x6E\x39\x44\x00\x00\x00"
        "\x44\x00\x00\x00\x00\x02\xFF\xFF\x14\x00\x00\x00\x30\x00\x00\x00"
        "\x01\x00\x24\x00\x00\x00\x30\x00\x00\x00\x30\x00\x00\x00\x24\x00"
        "\x00\x00\x30\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x02\xFF\xFF\x01\x00\x00\x02\x00\x08\x0F\x80\x00\x00\xE4\xA0"
        "\xFF\xFF\x00\x00\x53\x48\x44\x52\x40\x00\x00\x00\x40\x00\x00\x00"
        "\x10\x00\x00\x00\x59\x00\x00\x04\x46\x8E\x20\x00\x00\x00\x00\x00"
        "\x01\x00\x00\x00\x65\x00\x00\x03\xF2\x20\x10\x00\x00\x00\x00\x00"
        "\x36\x00\x00\x06\xF2\x20\x10\x00\x00\x00\x00\x00\x46\x8E\x20\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x3E\x00\x00\x01\x53\x54\x41\x54"
        "\x74\x00\x00\x00\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x52\x44\x45\x46\xB8\x00\x00\x00"
        "\x01\x00\x00\x00\x48\x00\x00\x00\x01\x00\x00\x00\x1C\x00\x00\x00"
        "\x00\x04\xFF\xFF\x08\x29\x04\x00\x90\x00\x00\x00\x3C\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x75\x6E\x69\x66"
        "\x6F\x72\x6D\x73\x00\xAB\xAB\xAB\x3C\x00\x00\x00\x01\x00\x00\x00"
        "\x60\x00\x00\x00\x10\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x78\x00\x00\x00\x00\x00\x00\x00\x10\x00\x00\x00\x02\x00\x00\x00"
        "\x80\x00\x00\x00\x00\x00\x00\x00\x63\x6F\x6C\x6F\x72\x00\xAB\xAB"
        "\x01\x00\x03\x00\x01\x00\x04\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x4D\x69\x63\x72\x6F\x73\x6F\x66\x74\x20\x28\x52\x29\x20\x48\x4C"
        "\x53\x4C\x20\x53\x68\x61\x64\x65\x72\x20\x43\x6F\x6D\x70\x69\x6C"
        "\x65\x72\x20\x31\x30\x2E\x31\x00\x49\x53\x47\x4E\x2C\x00\x00\x00"
        "\x01\x00\x00\x00\x08\x00\x00\x00\x20\x00\x00\x00\x00\x00\x00\x00"
        "\x01\x00\x00\x00\x03\x00\x00\x00\x00\x00\x00\x00\x0F\x00\x00\x00"
        "\x53\x56\x5F\x50\x6F\x73\x69\x74\x69\x6F\x6E\x00\x4F\x53\x47\x4E"
        "\x2C\x00\x00\x00\x01\x00\x00\x00\x08\x00\x00\x00\x20\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x03\x00\x00\x00\x00\x00\x00\x00"
        "\x0F\x00\x00\x00\x53\x56\x5F\x54\x61\x72\x67\x65\x74\x00\xAB\xAB"
        ;

    if (!safe_clear_pipeline) {
        D3DSysShader *vs = d3d_sys_shader_create(
            SHADER_TYPE_VERTEX, safe_clear_vs_bytecode,
            sizeof(safe_clear_vs_bytecode) - 1, 1);
        if (UNLIKELY(!vs)) {
            DLOG("Failed to create vertex shader for safe clear");
            return;
        }
        D3DSysShader *ps = d3d_sys_shader_create(
            SHADER_TYPE_FRAGMENT, safe_clear_ps_bytecode,
            sizeof(safe_clear_ps_bytecode) - 1, 1);
        if (UNLIKELY(!ps)) {
            DLOG("Failed to create pixel shader for safe clear");
            d3d_sys_shader_destroy(vs);
            return;
        }
        safe_clear_pipeline = d3d_sys_shader_pipeline_create(vs, ps);
        if (UNLIKELY(!safe_clear_pipeline)) {
            DLOG("Failed to create shader pipeline for safe clear");
            d3d_sys_shader_destroy(ps);
            d3d_sys_shader_destroy(vs);
            return;
        }
        safe_clear_color_uniform = d3d_sys_shader_get_uniform_id(ps, "color");
        ASSERT(safe_clear_color_uniform != 0, return);
    }

    if (color) {
        d3d_sys_shader_set_uniform_vec4(safe_clear_pipeline->pixel_shader,
                                        safe_clear_color_uniform, color);
    }

    const D3D11_VIEWPORT saved_viewport = viewport;
    const D3D11_RASTERIZER_DESC saved_raster = raster;
    const D3D11_DEPTH_STENCIL_DESC saved_depthstencil = depthstencil;
    const int saved_blend_enable = blend.RenderTarget[0].BlendEnable;

    viewport.MinDepth = 0;
    viewport.MaxDepth = 0;
    viewport_dirty = 1;
    raster.FillMode = D3D11_FILL_SOLID;
    raster.CullMode = D3D11_CULL_NONE;
    raster_dirty = 1;
    depthstencil.DepthEnable = depth ? TRUE : FALSE;
    depthstencil.DepthWriteMask =
        depth ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
    depthstencil.DepthFunc = D3D11_COMPARISON_ALWAYS;
    depthstencil.StencilEnable = depth ? TRUE : FALSE;
    depthstencil.StencilReadMask = depth ? 0xFF : 0;
    depthstencil.StencilWriteMask = depth ? 0xFF : 0;
    depthstencil.FrontFace.StencilFailOp = D3D11_STENCIL_OP_REPLACE;
    depthstencil.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_REPLACE;
    depthstencil.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
    depthstencil.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    depthstencil.BackFace.StencilFailOp = D3D11_STENCIL_OP_REPLACE;
    depthstencil.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_REPLACE;
    depthstencil.BackFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
    depthstencil.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    stencil_ref = stencil;
    depthstencil_dirty = 1;
    blend.RenderTarget[0].BlendEnable = FALSE;
    if (!color) {
        blend.RenderTarget[0].RenderTargetWriteMask = 0;
    }
    blend_dirty = 1;
    d3d_state_apply();
    d3d_sys_shader_pipeline_apply(safe_clear_pipeline);

    const float z = depth ? (bound(*depth, 0.0f, 1.0f) * 2.0f - 1.0f) : 1.0f;
    Vector4f vertices[6];
    for (int i = 0; i < 6; i++) {
        vertices[i].x = (i == 0 || i == 2 || i == 3) ? -1.0f : 1.0f;
        vertices[i].y = (i == 0 || i == 1 || i == 4) ? -1.0f : 1.0f;
        vertices[i].z = z;
        vertices[i].w = 1.0f;
    }
    static const uint32_t vertex_format[] = {
        GRAPHICS_VERTEX_FORMAT(POSITION_4F, 0),
        0
    };
    D3DSysPrimitive *primitive = d3d_sys_graphics_create_primitive(
        GRAPHICS_PRIMITIVE_TRIANGLES, vertices, vertex_format,
        sizeof(*vertices), lenof(vertices), NULL, 0, 0, 1);
    if (UNLIKELY(!primitive)) {
        DLOG("Failed to create primitive for safe clear");
    } else {
        const int saved_shader_obj_enable = d3d_shader_objects_enabled;
        d3d_shader_objects_enabled = 1;
        d3d_sys_graphics_draw_primitive(primitive, 0, -1);
        d3d_shader_objects_enabled = saved_shader_obj_enable;
        d3d_sys_graphics_destroy_primitive(primitive);
    }

    viewport = saved_viewport;
    viewport_dirty = 1;
    raster = saved_raster;
    raster_dirty = 1;
    depthstencil = saved_depthstencil;
    depthstencil_dirty = 1;
    blend.RenderTarget[0].BlendEnable = saved_blend_enable;
    blend_dirty = 1;
    d3d_state_apply();
    d3d_sys_shader_pipeline_apply(NULL);
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static D3D11_BLEND convert_blend_factor(GraphicsBlendFactor factor,
                                        int alpha_only)
{
    switch (factor) {
      case GRAPHICS_BLEND_ZERO:
        return D3D11_BLEND_ZERO;
      case GRAPHICS_BLEND_ONE:
        return D3D11_BLEND_ONE;
      case GRAPHICS_BLEND_SRC_COLOR:
        return alpha_only ? D3D11_BLEND_SRC_ALPHA : D3D11_BLEND_SRC_COLOR;
      case GRAPHICS_BLEND_SRC_ALPHA:
        return D3D11_BLEND_SRC_ALPHA;
      case GRAPHICS_BLEND_INV_SRC_ALPHA:
        return D3D11_BLEND_INV_SRC_ALPHA;
      case GRAPHICS_BLEND_DEST_COLOR:
        return alpha_only ? D3D11_BLEND_DEST_ALPHA : D3D11_BLEND_DEST_COLOR;
      case GRAPHICS_BLEND_DEST_ALPHA:
        return D3D11_BLEND_DEST_ALPHA;
      case GRAPHICS_BLEND_INV_DEST_ALPHA:
        return D3D11_BLEND_INV_DEST_ALPHA;
      case GRAPHICS_BLEND_CONSTANT:
        return D3D11_BLEND_BLEND_FACTOR;
      case GRAPHICS_BLEND_INV_CONSTANT:
        return D3D11_BLEND_INV_BLEND_FACTOR;
    }
    ASSERT(!"Invalid blend factor", return 0);  // NOTREACHED
}

/*-----------------------------------------------------------------------*/

static D3D11_COMPARISON_FUNC convert_comparison(GraphicsComparisonType type)
{
    switch (type) {
      case GRAPHICS_COMPARISON_TRUE:
        return D3D11_COMPARISON_ALWAYS;
      case GRAPHICS_COMPARISON_FALSE:
        return D3D11_COMPARISON_NEVER;
      case GRAPHICS_COMPARISON_EQUAL:
        return D3D11_COMPARISON_EQUAL;
      case GRAPHICS_COMPARISON_NOT_EQUAL:
        return D3D11_COMPARISON_NOT_EQUAL;
      case GRAPHICS_COMPARISON_LESS:
        return D3D11_COMPARISON_LESS;
      case GRAPHICS_COMPARISON_LESS_EQUAL:
        return D3D11_COMPARISON_LESS_EQUAL;
      case GRAPHICS_COMPARISON_GREATER_EQUAL:
        return D3D11_COMPARISON_GREATER_EQUAL;
      case GRAPHICS_COMPARISON_GREATER:
        return D3D11_COMPARISON_GREATER;
    }
    ASSERT(!"Invalid comparison type", return 0);  // NOTREACHED
}

/*-----------------------------------------------------------------------*/

static D3D11_STENCIL_OP convert_stencil_op(GraphicsStencilOp operation)
{
    switch (operation) {
        case GRAPHICS_STENCIL_KEEP:    return D3D11_STENCIL_OP_KEEP;
        case GRAPHICS_STENCIL_CLEAR:   return D3D11_STENCIL_OP_ZERO;
        case GRAPHICS_STENCIL_REPLACE: return D3D11_STENCIL_OP_REPLACE;
        case GRAPHICS_STENCIL_INCR:    return D3D11_STENCIL_OP_INCR_SAT;
        case GRAPHICS_STENCIL_DECR:    return D3D11_STENCIL_OP_DECR_SAT;
        case GRAPHICS_STENCIL_INVERT:  return D3D11_STENCIL_OP_INVERT;
    }
    ASSERT(!"Invalid stencil op", return 0);  // NOTREACHED
}

/*************************************************************************/
/*************************************************************************/
