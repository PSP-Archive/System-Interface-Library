/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/ge-util/misc.c: Miscellaneous graphics functions for the
 * GE utility library.
 */

#include "src/base.h"
#include "src/sysdep/psp/ge-util.h"
#include "src/sysdep/psp/ge-util/ge-const.h"
#include "src/sysdep/psp/ge-util/ge-local.h"
#include "src/sysdep/psp/internal.h"

/*************************************************************************/
/**************************** Helper routine *****************************/
/*************************************************************************/

/**
 * internal_add_color_xyz_vertex:  Add a vertex with color and 3D integer
 * position data.  This function does not check whether the buffer is full.
 *
 * [Parameters]
 *     color: Vertex color (0xAABBGGRR).
 *     x, y, z: Vertex coordinates.
 */
static inline void internal_add_color_xyz_vertex(uint32_t color, int16_t x,
                                                 int16_t y, int16_t z)
{
    *vertlist_ptr++ = color;
    *vertlist_ptr++ = INT16_PAIR(x, y);
    *vertlist_ptr++ = INT16_PAIR(z, 0);
}

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

void ge_clear(int clear_screen, int clear_depth, int clear_stencil,
              uint32_t color, uint16_t depth, int width, int height)
{
    CHECK_GELIST(6);
    CHECK_VERTLIST(6);

    const uint32_t clear_flags = (clear_screen  ? GECLEAR_DRAW    : 0)
                               | (clear_depth   ? GECLEAR_DEPTH   : 0)
                               | (clear_stencil ? GECLEAR_STENCIL : 0);
    internal_add_command(GECMD_CLEAR_MODE, GECLEAR_ON | clear_flags);
    ge_set_vertex_format(GE_VERTEXFMT_TRANSFORM_2D
                       | GE_VERTEXFMT_COLOR_8888
                       | GE_VERTEXFMT_VERTEX_16BIT);
    ge_set_vertex_pointer(NULL);
    internal_add_color_xyz_vertex(color, 0, 0, depth);
    internal_add_color_xyz_vertex(color, width, height, depth);
    ge_draw_primitive(GE_PRIMITIVE_SPRITES, 2);
    internal_add_command(GECMD_CLEAR_MODE, GECLEAR_OFF);
    ge_commit();
}

/*-----------------------------------------------------------------------*/

void ge_copy(const uint32_t *src, uint32_t src_stride, uint32_t *dest,
             uint32_t dest_stride, int width, int height, GECopyMode mode)
{
    CHECK_GELIST(8);

    const int Bpp = (mode==GE_COPY_16BIT) ? 2 : 4;
    internal_add_command(GECMD_COPY_S_ADDRESS, ((uint32_t)src &0x00FFFFC0));
    internal_add_command(GECMD_COPY_S_STRIDE,  ((uint32_t)src &0xFF000000) >> 8
                                               | src_stride);
    internal_add_command(GECMD_COPY_S_POS, ((uint32_t)src &0x0000003F) / Bpp);
    internal_add_command(GECMD_COPY_D_ADDRESS, ((uint32_t)dest&0x00FFFFC0));
    internal_add_command(GECMD_COPY_D_STRIDE,  ((uint32_t)dest&0xFF000000) >> 8
                                               | dest_stride);
    internal_add_command(GECMD_COPY_D_POS, ((uint32_t)dest&0x0000003F) / Bpp);
    internal_add_command(GECMD_COPY_SIZE, (width-1) | (height-1)<<10);
    internal_add_command(GECMD_COPY, mode);
    ge_commit();
}

/*************************************************************************/
/*************************************************************************/
