/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/misc/sysfont-none.c: sys_sysfont_*() implementation for
 * systems with no native font support.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/sysdep.h"

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

SysFont *sys_sysfont_create(UNUSED const char *name, UNUSED float size,
                            UNUSED int mem_flags)
{
    DLOG("System fonts not supported");
    return NULL;
}

/*-----------------------------------------------------------------------*/

void sys_sysfont_destroy(UNUSED SysFont *font)  // NOTREACHED
{
    return;  // NOTREACHED
}

/*-----------------------------------------------------------------------*/

int sys_sysfont_native_size(UNUSED SysFont *font)  // NOTREACHED
{
    return 0;  // NOTREACHED
}

/*-----------------------------------------------------------------------*/

void sys_sysfont_get_metrics(  // NOTREACHED
    UNUSED SysFont *font, UNUSED float size, UNUSED float *height_ret,
    UNUSED float *baseline_ret, UNUSED float *ascent_ret,
    UNUSED float *descent_ret)
{
    return;  // NOTREACHED
}

/*-----------------------------------------------------------------------*/

float sys_sysfont_char_advance(UNUSED SysFont *font,  // NOTREACHED
                               UNUSED int32_t ch, UNUSED float size)
{
    return 0;  // NOTREACHED
}

/*-----------------------------------------------------------------------*/

float sys_sysfont_text_advance(UNUSED SysFont *font,  // NOTREACHED
                               UNUSED const char *str, UNUSED float size)
{
    return 0;  // NOTREACHED
}

/*-----------------------------------------------------------------------*/

void sys_sysfont_get_text_bounds(  // NOTREACHED
    UNUSED SysFont *font, UNUSED const char *str, UNUSED float size,
    UNUSED float *left_ret, UNUSED float *right_ret)
{
    return;  // NOTREACHED
}

/*-----------------------------------------------------------------------*/

struct SysTexture *sys_sysfont_render(  // NOTREACHED
    UNUSED SysFont *font, UNUSED const char *str, UNUSED float size,
    UNUSED float *origin_x_ret, UNUSED float *origin_y_ret,
    UNUSED float *advance_ret, UNUSED float *scale_ret)
{
    return NULL;  // NOTREACHED
}

/*************************************************************************/
/*************************************************************************/
