/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/ge-util/draw.c: Drawing functions for the GE utility library.
 */

#include "src/base.h"
#include "src/sysdep/psp/ge-util.h"
#include "src/sysdep/psp/ge-util/ge-const.h"
#include "src/sysdep/psp/ge-util/ge-local.h"
#include "src/sysdep/psp/internal.h"

/*************************************************************************/
/*************************************************************************/

void ge_set_draw_buffer(void *buffer, int stride)
{
    if (!buffer) {
        buffer = psp_draw_buffer();
        stride = DISPLAY_STRIDE;
    }
    CHECK_GELIST(2);
    internal_add_command(GECMD_DRAW_ADDRESS, ((uint32_t)buffer & 0x00FFFFC0));
    internal_add_command(GECMD_DRAW_STRIDE,  ((uint32_t)buffer & 0xFF000000)>>8
                                             | stride);
}

/*-----------------------------------------------------------------------*/

void ge_set_depth_buffer(void *buffer, int stride)
{
    if (!buffer) {
        buffer = psp_depth_buffer();
        if (!buffer) {
            return;
        }
        stride = DISPLAY_STRIDE;
    }
    CHECK_GELIST(2);
    internal_add_command(GECMD_DEPTH_ADDRESS,((uint32_t)buffer & 0x00FFFFC0));
    internal_add_command(GECMD_DEPTH_STRIDE, ((uint32_t)buffer & 0xFF000000)>>8
                                             | stride);
}

/*-----------------------------------------------------------------------*/

void ge_set_index_pointer(const void *ptr)
{
    CHECK_GELIST(2);
    internal_add_command(GECMD_ADDRESS_BASE, ((uint32_t)ptr & 0xFF000000)>>8);
    internal_add_command(GECMD_INDEX_POINTER, (uint32_t)ptr & 0x00FFFFFF);
}

/*-----------------------------------------------------------------------*/

void ge_set_vertex_format(uint32_t format)
{
    CHECK_GELIST(1);
    internal_add_command(GECMD_VERTEX_FORMAT, format & 0xFFFFFF);
}

/*-----------------------------------------------------------------------*/

void ge_set_vertex_pointer(const void *ptr)
{
    if (!ptr) {
        ptr = vertlist_ptr;
    }
    CHECK_GELIST(2);
    internal_add_command(GECMD_ADDRESS_BASE, ((uint32_t)ptr & 0xFF000000)>>8);
    internal_add_command(GECMD_VERTEX_POINTER, (uint32_t)ptr & 0x00FFFFFF);
}

/*-----------------------------------------------------------------------*/

void ge_draw_primitive(GEPrimitive primitive, uint16_t num_vertices)
{
    CHECK_GELIST(1);
    internal_add_command(GECMD_DRAW_PRIMITIVE, primitive<<16 | num_vertices);
}

/*************************************************************************/
/*************************************************************************/
