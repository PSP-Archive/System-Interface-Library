/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/ge-util/state.c: Render state manipulation routines for
 * the GE utility library.
 */

#include "src/base.h"
#include "src/sysdep/psp/ge-util.h"
#include "src/sysdep/psp/ge-util/ge-const.h"
#include "src/sysdep/psp/ge-util/ge-local.h"
#include "src/sysdep/psp/internal.h"

/*************************************************************************/
/*************************************************************************/

void ge_enable(GEState state)
{
    CHECK_GELIST(1);
    switch (state) {
      case GE_STATE_LIGHTING:
        internal_add_command(GECMD_ENA_LIGHTING,   1);  return;
      case GE_STATE_CLIP_PLANES:
        internal_add_command(GECMD_ENA_ZCLIP,      1);  return;
      case GE_STATE_TEXTURE:
        internal_add_command(GECMD_ENA_TEXTURE,    1);  return;
      case GE_STATE_FOG:
        internal_add_command(GECMD_ENA_FOG,        1);  return;
      case GE_STATE_DITHER:
        internal_add_command(GECMD_ENA_DITHER,     1);  return;
      case GE_STATE_BLEND:
        internal_add_command(GECMD_ENA_BLEND,      1);  return;
      case GE_STATE_ALPHA_TEST:
        internal_add_command(GECMD_ENA_ALPHA_TEST, 1);  return;
      case GE_STATE_DEPTH_TEST:
        internal_add_command(GECMD_ENA_DEPTH_TEST, 1);  return;
      case GE_STATE_DEPTH_WRITE:
        internal_add_command(GECMD_DEPTH_MASK,     0);  return;
      case GE_STATE_STENCIL_TEST:
        internal_add_command(GECMD_ENA_STENCIL,    1);  return;
      case GE_STATE_ANTIALIAS:
        internal_add_command(GECMD_ENA_ANTIALIAS,  1);  return;
      case GE_STATE_PATCH_CULL_FACE:
        internal_add_command(GECMD_ENA_PATCH_CULL, 1);  return;
      case GE_STATE_COLOR_TEST:
        internal_add_command(GECMD_ENA_COLOR_TEST, 1);  return;
      case GE_STATE_COLOR_LOGIC_OP:
        internal_add_command(GECMD_ENA_LOGIC_OP,   1);  return;
      case GE_STATE_REVERSE_NORMALS:
        internal_add_command(GECMD_REV_NORMALS,    1);  return;
    }
    DLOG("Unknown state %d", state);
}

/*-----------------------------------------------------------------------*/

void ge_disable(GEState state)
{
    CHECK_GELIST(1);
    switch (state) {
      case GE_STATE_LIGHTING:
        internal_add_command(GECMD_ENA_LIGHTING,   0);  return;
      case GE_STATE_CLIP_PLANES:
        internal_add_command(GECMD_ENA_ZCLIP,      0);  return;
      case GE_STATE_TEXTURE:
        internal_add_command(GECMD_ENA_TEXTURE,    0);  return;
      case GE_STATE_FOG:
        internal_add_command(GECMD_ENA_FOG,        0);  return;
      case GE_STATE_DITHER:
        internal_add_command(GECMD_ENA_DITHER,     0);  return;
      case GE_STATE_BLEND:
        internal_add_command(GECMD_ENA_BLEND,      0);  return;
      case GE_STATE_ALPHA_TEST:
        internal_add_command(GECMD_ENA_ALPHA_TEST, 0);  return;
      case GE_STATE_DEPTH_TEST:
        internal_add_command(GECMD_ENA_DEPTH_TEST, 0);  return;
      case GE_STATE_DEPTH_WRITE:
        internal_add_command(GECMD_DEPTH_MASK,     1);  return;
      case GE_STATE_STENCIL_TEST:
        internal_add_command(GECMD_ENA_STENCIL,    0);  return;
      case GE_STATE_ANTIALIAS:
        internal_add_command(GECMD_ENA_ANTIALIAS,  0);  return;
      case GE_STATE_PATCH_CULL_FACE:
        internal_add_command(GECMD_ENA_PATCH_CULL, 0);  return;
      case GE_STATE_COLOR_TEST:
        internal_add_command(GECMD_ENA_COLOR_TEST, 0);  return;
      case GE_STATE_COLOR_LOGIC_OP:
        internal_add_command(GECMD_ENA_LOGIC_OP,   0);  return;
      case GE_STATE_REVERSE_NORMALS:
        internal_add_command(GECMD_REV_NORMALS,    0);  return;
    }
    DLOG("Unknown state %d", state);
}

/*-----------------------------------------------------------------------*/

void ge_set_alpha_mask(uint8_t mask)
{
    CHECK_GELIST(1);
    ge_add_command(GECMD_ALPHA_MASK, mask);
}

/*-----------------------------------------------------------------------*/

void ge_set_alpha_test(GETestFunc test, uint8_t ref)
{
    CHECK_GELIST(1);
    ge_add_command(GECMD_ALPHATEST, test | ref<<8 | 0xFF<<16);
}

/*-----------------------------------------------------------------------*/

void ge_set_ambient_color(uint32_t color)
{
    CHECK_GELIST(2);
    internal_add_command(GECMD_AMBIENT_COLOR, color & 0xFFFFFF);
    internal_add_command(GECMD_AMBIENT_ALPHA, color >> 24);
}

/*-----------------------------------------------------------------------*/

void ge_set_ambient_light(uint32_t color)
{
    CHECK_GELIST(2);
    internal_add_command(GECMD_LIGHT_AMBCOLOR, color & 0xFFFFFF);
    internal_add_command(GECMD_LIGHT_AMBALPHA, color >> 24);
}

/*-----------------------------------------------------------------------*/

void ge_set_blend_mode(GEBlendFunc func,
                       GEBlendParam src_param, GEBlendParam dst_param,
                       uint32_t src_fix, uint32_t dst_fix)
{
    CHECK_GELIST(3);
    internal_add_command(GECMD_BLEND_FUNC, func<<8 | dst_param<<4 | src_param);
    if (src_param == GE_BLEND_FIX) {
        internal_add_command(GECMD_BLEND_SRCFIX, src_fix);
    }
    if (dst_param == GE_BLEND_FIX) {
        internal_add_command(GECMD_BLEND_DSTFIX, dst_fix);
    }
}

/*-----------------------------------------------------------------------*/

void ge_set_clip_area(int x0, int y0, int x1, int y1)
{
    x0 = bound(x0, 0, 1023);
    y0 = bound(y0, 0, 1023);
    x1 = bound(x1, 0, 1023);
    y1 = bound(y1, 0, 1023);
    if (x1 < x0) {
        int tmp = x0; x0 = x1; x1 = tmp;
    }
    if (y1 < x0) {
        int tmp = y0; y0 = y1; y1 = tmp;
    }
    CHECK_GELIST(2);
    internal_add_command(GECMD_CLIP_MIN, x0 | y0<<10);
    internal_add_command(GECMD_CLIP_MAX, x1 | y1<<10);
}

/*-----------------------------------------------------------------------*/

void ge_unset_clip_area(void)
{
    ge_set_clip_area(0, 0, DISPLAY_WIDTH-1, DISPLAY_HEIGHT-1);
}

/*-----------------------------------------------------------------------*/

void ge_set_color_mask(uint32_t mask)
{
    CHECK_GELIST(1);
    ge_add_command(GECMD_COLOR_MASK, mask);
}

/*-----------------------------------------------------------------------*/

void ge_set_cull_mode(GECullMode mode)
{
    CHECK_GELIST(2);
    switch (mode) {
      case GE_CULL_NONE:
        internal_add_command(GECMD_ENA_FACE_CULL, 0);
        return;
      case GE_CULL_CW:
        internal_add_command(GECMD_ENA_FACE_CULL, 1);
        internal_add_command(GECMD_FACE_ORDER, GEVERT_CCW);
        return;
      case GE_CULL_CCW:
        internal_add_command(GECMD_ENA_FACE_CULL, 1);
        internal_add_command(GECMD_FACE_ORDER, GEVERT_CW);
        return;
    }
    DLOG("Unknown mode %d", mode);
}

/*-----------------------------------------------------------------------*/

void ge_set_depth_test(GETestFunc test)
{
    CHECK_GELIST(1);
    ge_add_command(GECMD_DEPTHTEST, test);
}

/*-----------------------------------------------------------------------*/

void ge_set_depth_range(uint16_t near, uint16_t far)
{
    CHECK_GELIST(4);
    internal_add_commandf(GECMD_ZSCALE, ((int32_t)far - (int32_t)near) / 2.0f);
    internal_add_commandf(GECMD_ZPOS,   ((int32_t)far + (int32_t)near) / 2.0f);
    internal_add_command (GECMD_CLIP_NEAR, min(near, far));
    internal_add_command (GECMD_CLIP_FAR,  max(near, far));
}

/*-----------------------------------------------------------------------*/

void ge_set_fog(float near, float far, int z_sign, uint32_t color)
{
    if (UNLIKELY(z_sign == 0)) {
        DLOG("WARNING: z_sign == 0, treating as positive");
        z_sign = 1;
    }

    if (z_sign > 0) {
        near = -near;
        far = -far;
    }

    const float range = far - near;

    CHECK_GELIST(3);
    internal_add_commandf(GECMD_FOG_LIMIT, far);
    internal_add_commandf(GECMD_FOG_RANGE, (range != 0) ? 1/range : 0);
    internal_add_command (GECMD_FOG_COLOR, color & 0xFFFFFF);
}

/*-----------------------------------------------------------------------*/

void ge_set_shade_mode(GEShadeMode mode)
{
    CHECK_GELIST(1);
    internal_add_command(GECMD_SHADE_MODE, mode);
}

/*-----------------------------------------------------------------------*/

void ge_set_stencil_func(GETestFunc func, uint8_t ref, uint8_t mask)
{
    CHECK_GELIST(1);
    internal_add_command(GECMD_STENCILTEST, func | ref<<8 | mask<<16);
}

/*-----------------------------------------------------------------------*/

extern void ge_set_stencil_op(
    GEStencilOp sfail, GEStencilOp dfail, GEStencilOp dpass)
{
    CHECK_GELIST(1);
    internal_add_command(GECMD_STENCIL_OP, sfail | dfail<<8 | dpass<<16);
}

/*-----------------------------------------------------------------------*/

void ge_set_viewport(int x, int y, int width, int height)
{
    CHECK_GELIST(4);
    internal_add_commandf(GECMD_XSCALE,   width /2);
    internal_add_commandf(GECMD_YSCALE,  -height/2);
    internal_add_command (GECMD_XOFFSET, (2048 - width /2 - x) << 4);
    internal_add_command (GECMD_YOFFSET, (2048 - height/2 - y) << 4);
}

/*************************************************************************/
/*************************************************************************/
