/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/ge-util/texture.c: Texture manipulation routines for the
 * GE utility library.
 */

#include "src/base.h"
#include "src/math.h"
#include "src/sysdep/psp/ge-util.h"
#include "src/sysdep/psp/ge-util/ge-const.h"
#include "src/sysdep/psp/ge-util/ge-local.h"
#include "src/sysdep/psp/internal.h"

/*************************************************************************/
/*************************************************************************/

void ge_set_colortable(const void *table, int count, GEPixelFormat pixfmt,
                       int shift, uint8_t mask)
{
    CHECK_GELIST(4);
    internal_add_command(GECMD_CLUT_MODE, pixfmt | (shift & 31)<<2 | mask<<8);
    internal_add_command(GECMD_CLUT_ADDRESS_L,
                         ((uint32_t)table & 0x00FFFFFF));
    internal_add_command(GECMD_CLUT_ADDRESS_H,
                         ((uint32_t)table & 0xFF000000) >> 8);
    internal_add_command(GECMD_CLUT_LOAD,
                         (pixfmt==GE_PIXFMT_8888) ? count/8 : count/16);
}

/*-----------------------------------------------------------------------*/

void ge_flush_texture_cache(void)
{
    CHECK_GELIST(1);
    internal_add_command(GECMD_TEXTURE_FLUSH, 0);
}

/*-----------------------------------------------------------------------*/

void ge_set_texture_data(int index, const void *data,
                         int width, int height, int stride)
{
    CHECK_GELIST(3);

    const int log2_width  =
        width ==1 ? 0 : ubound(32 - __builtin_clz(width -1), 9);
    const int log2_height =
        height==1 ? 0 : ubound(32 - __builtin_clz(height-1), 9);

    internal_add_command(GECMD_TEX0_ADDRESS + index,
                         ((uint32_t)data & 0x00FFFFFF));
    internal_add_command(GECMD_TEX0_STRIDE + index,
                         ((uint32_t)data & 0xFF000000)>>8 | stride);
    internal_add_command(GECMD_TEX0_SIZE + index, log2_height<<8 | log2_width);
}

/*-----------------------------------------------------------------------*/

void ge_set_texture_draw_mode(GETextureDrawMode mode, int alpha)
{
    CHECK_GELIST(1);
    internal_add_command(GECMD_TEXTURE_FUNC, mode | (alpha ? 1<<8 : 0));
}

/*-----------------------------------------------------------------------*/

void ge_set_texture_color(uint32_t color)
{
    CHECK_GELIST(1);
    internal_add_command(GECMD_TEXTURE_COLOR, color & 0xFFFFFF);
}

/*-----------------------------------------------------------------------*/

void ge_set_texture_filter(GETextureFilter mag_filter,
                           GETextureFilter min_filter,
                           GETextureMipFilter mip_filter)
{
    CHECK_GELIST(1);
    internal_add_command(GECMD_TEXTURE_FILTER, (mag_filter | mip_filter) << 8
                                             | (min_filter | mip_filter));
}

/*-----------------------------------------------------------------------*/

void ge_set_texture_format(int levels, int swizzled, GETexelFormat format)
{
    CHECK_GELIST(3);
    internal_add_command(GECMD_TEXTURE_MODE, (bound(levels,1,8) - 1) << 16
                                           | (swizzled ? 1 : 0));
    internal_add_command(GECMD_TEXTURE_PIXFMT, format);
    internal_add_command(GECMD_TEXTURE_FLUSH, 0);
}

/*-----------------------------------------------------------------------*/

void ge_set_texture_map_mode(GETextureMapMode mode, GETextureMapSource source,
                             int row1, int row2)
{
    if (mode != GETEXMAPMODE_ENVIRONMENT_MAP) {
        source = 0;
    }
    if (mode == GETEXMAPMODE_TEXTURE_COORDS) {
        row1 = row2 = 0;
    }
    CHECK_GELIST(2);
    internal_add_command(GECMD_TEXTURE_MAP, mode | source<<8);
    internal_add_command(GECMD_TEXTURE_MATSEL, row1 | row2<<8);
}

/*-----------------------------------------------------------------------*/

void ge_set_texture_mipmap_mode(GEMipmapMode mode, float bias)
{
    CHECK_GELIST(1);
    const int bias_int = iroundf(bound(bias*16, -128, +127)) & 0xFF;
    internal_add_command(GECMD_TEXTURE_BIAS, bias_int<<16 | mode);
}

/*-----------------------------------------------------------------------*/

void ge_set_texture_mipmap_slope(float slope)
{
    CHECK_GELIST(1);
    internal_add_commandf(GECMD_TEXTURE_SLOPE, slope);
}

/*-----------------------------------------------------------------------*/

void ge_set_texture_wrap_mode(GETextureWrapMode u_mode,
                              GETextureWrapMode v_mode)
{
    CHECK_GELIST(1);
    internal_add_command(GECMD_TEXTURE_WRAP, u_mode | v_mode<<8);
}

/*-----------------------------------------------------------------------*/

void ge_set_texture_scale(float u_scale, float v_scale)
{
    CHECK_GELIST(2);
    internal_add_commandf(GECMD_USCALE, u_scale);
    internal_add_commandf(GECMD_VSCALE, v_scale);
}

/*-----------------------------------------------------------------------*/

void ge_set_texture_offset(float u_offset, float v_offset)
{
    CHECK_GELIST(2);
    internal_add_commandf(GECMD_UOFFSET, u_offset);
    internal_add_commandf(GECMD_VOFFSET, v_offset);
}

/*************************************************************************/
/*************************************************************************/
