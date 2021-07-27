/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/ge-util/vertex.c: Vertex manipulation routines for the
 * GE utility library.
 */

#include "src/base.h"
#include "src/sysdep/psp/ge-util.h"
#include "src/sysdep/psp/ge-util/ge-const.h"
#include "src/sysdep/psp/ge-util/ge-local.h"
#include "src/sysdep/psp/internal.h"

/*************************************************************************/
/*************************************************************************/

void ge_add_color_xy_vertex(uint32_t color, int16_t x, int16_t y)
{
    CHECK_VERTLIST(3);
    *vertlist_ptr++ = color;
    *vertlist_ptr++ = INT16_PAIR(x, y);
    *vertlist_ptr++ = INT16_PAIR(0 /*z*/, 0 /*pad*/);
}

/*-----------------------------------------------------------------------*/

void ge_add_color_xy_vertexf(uint32_t color, float x, float y)
{
    CHECK_VERTLIST(4);
    *vertlist_ptr++ = color;
    *vertlist_ptr++ = FLOAT(x);
    *vertlist_ptr++ = FLOAT(y);
    *vertlist_ptr++ = 0;
}

/*-----------------------------------------------------------------------*/

void ge_add_color_xyz_vertexf(uint32_t color, float x, float y, float z)
{
    CHECK_VERTLIST(4);
    *vertlist_ptr++ = color;
    *vertlist_ptr++ = FLOAT(x);
    *vertlist_ptr++ = FLOAT(y);
    *vertlist_ptr++ = FLOAT(z);
}

/*-----------------------------------------------------------------------*/

void ge_add_uv_xy_vertex(int16_t u, int16_t v, int16_t x, int16_t y)
{
    CHECK_VERTLIST(3);
    static int which = 0;
    if (which == 0) {  // First vertex of a pair.
        *vertlist_ptr++ = INT16_PAIR(u, v);
        *vertlist_ptr++ = INT16_PAIR(x, y);
    } else {  // Second vertex of a pair.
        *vertlist_ptr++ = INT16_PAIR(0 /*z*/, u);
        *vertlist_ptr++ = INT16_PAIR(v, x);
        *vertlist_ptr++ = INT16_PAIR(y, 0 /*z*/);
    }
    which = !which;
}

/*-----------------------------------------------------------------------*/

void ge_add_uv_xyz_vertexf(float u, float v, float x, float y, float z)
{
    CHECK_VERTLIST(5);
    *vertlist_ptr++ = FLOAT(u);
    *vertlist_ptr++ = FLOAT(v);
    *vertlist_ptr++ = FLOAT(x);
    *vertlist_ptr++ = FLOAT(y);
    *vertlist_ptr++ = FLOAT(z);
}

/*-----------------------------------------------------------------------*/

void ge_add_uv_color_xy_vertex(int16_t u, int16_t v, uint32_t color,
                               int16_t x, int16_t y)
{
    CHECK_VERTLIST(4);
    *vertlist_ptr++ = INT16_PAIR(u, v);
    *vertlist_ptr++ = color;
    *vertlist_ptr++ = INT16_PAIR(x, y);
    *vertlist_ptr++ = INT16_PAIR(0 /*z*/, 0 /*pad*/);
}

/*-----------------------------------------------------------------------*/

void ge_add_uv_color_xyz_vertexf(float u, float v, uint32_t color,
                                 float x, float y, float z)
{
    CHECK_VERTLIST(6);
    *vertlist_ptr++ = FLOAT(u);
    *vertlist_ptr++ = FLOAT(v);
    *vertlist_ptr++ = color;
    *vertlist_ptr++ = FLOAT(x);
    *vertlist_ptr++ = FLOAT(y);
    *vertlist_ptr++ = FLOAT(z);
}

/*-----------------------------------------------------------------------*/

void *ge_reserve_vertexbytes(int size)
{
    if (UNLIKELY(size <= 0)) {
        DLOG("Invalid size %d", size);
        return NULL;
    }

    const int nwords = (size+3) / 4;
    if (UNLIKELY(vertlist_ptr + nwords > vertlist_limit)) {
        DLOG("No memory for %d vertex bytes", size);
        return NULL;
    }
    void *retval = vertlist_ptr;
    vertlist_ptr += nwords;
    return retval;
}

/*************************************************************************/
/*************************************************************************/
