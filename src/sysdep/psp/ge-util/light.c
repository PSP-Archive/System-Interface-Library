/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/ge-util/light.c: Light source manipulation routines for
 * the GE utility library.
 */

#include "src/base.h"
#include "src/sysdep/psp/ge-util.h"
#include "src/sysdep/psp/ge-util/ge-const.h"
#include "src/sysdep/psp/ge-util/ge-local.h"
#include "src/sysdep/psp/internal.h"

/*************************************************************************/
/*************************************************************************/

void ge_set_light_mode(unsigned int mode)
{
    CHECK_GELIST(1);
    internal_add_command(GECMD_LIGHT_MODEL, mode);
}

/*-----------------------------------------------------------------------*/

void ge_enable_light(unsigned int light)
{
    if (light > 3) {
        DLOG("Invalid light source %u", light);
        return;
    }
    CHECK_GELIST(1);
    internal_add_command(GECMD_ENA_LIGHT0 + light, 1);
}

/*-----------------------------------------------------------------------*/

void ge_disable_light(unsigned int light)
{
    if (light > 3) {
        DLOG("Invalid light source %u", light);
        return;
    }
    CHECK_GELIST(1);
    internal_add_command(GECMD_ENA_LIGHT0 + light, 0);
}

/*-----------------------------------------------------------------------*/

void ge_set_light_type(unsigned int light, GELightType type,
                       int has_specular)
{
    if (light > 3) {
        DLOG("Invalid light source %u", light);
        return;
    }
    CHECK_GELIST(1);
    internal_add_command(GECMD_LIGHT0_TYPE + light,
                         (type & 3) << 8 | (has_specular ? 1 : 0));
}

/*-----------------------------------------------------------------------*/

void ge_set_light_position(unsigned int light, float x, float y, float z)
{
    if (light > 3) {
        DLOG("Invalid light source %u", light);
        return;
    }
    CHECK_GELIST(3);
    internal_add_commandf(GECMD_LIGHT0_XPOS + light*3, x);
    internal_add_commandf(GECMD_LIGHT0_YPOS + light*3, y);
    internal_add_commandf(GECMD_LIGHT0_ZPOS + light*3, z);
}

/*-----------------------------------------------------------------------*/

void ge_set_light_direction(unsigned int light, float x, float y, float z)
{
    if (light > 3) {
        DLOG("Invalid light source %u", light);
        return;
    }
    CHECK_GELIST(3);
    internal_add_commandf(GECMD_LIGHT0_XDIR + light*3, x);
    internal_add_commandf(GECMD_LIGHT0_YDIR + light*3, y);
    internal_add_commandf(GECMD_LIGHT0_ZDIR + light*3, z);
}

/*-----------------------------------------------------------------------*/

void ge_set_light_attenuation(unsigned int light, float constant,
                              float linear, float quadratic)
{
    if (light > 3) {
        DLOG("Invalid light source %u", light);
        return;
    }
    CHECK_GELIST(3);
    internal_add_commandf(GECMD_LIGHT0_CATT + light*3, constant);
    internal_add_commandf(GECMD_LIGHT0_LATT + light*3, linear);
    internal_add_commandf(GECMD_LIGHT0_QATT + light*3, quadratic);
}

/*-----------------------------------------------------------------------*/

void ge_set_light_color(unsigned int light, unsigned int component,
                        uint32_t color)
{
    if (light > 3) {
        DLOG("Invalid light source %u", light);
        return;
    }
    if (component > 2) {
        DLOG("Invalid component %u", component);
        return;
    }
    CHECK_GELIST(1);
    internal_add_command(GECMD_LIGHT0_ACOL + light*3 + component,
                         color & 0xFFFFFF);
}

/*-----------------------------------------------------------------------*/

void ge_set_spotlight_exponent(unsigned int light, float exponent)
{
    if (light > 3) {
        DLOG("Invalid light source %u", light);
        return;
    }
    CHECK_GELIST(1);
    internal_add_commandf(GECMD_LIGHT0_SPOTEXP + light, exponent);
}

/*-----------------------------------------------------------------------*/

void ge_set_spotlight_cutoff(unsigned int light, float cutoff)
{
    if (light > 3) {
        DLOG("Invalid light source %u", light);
        return;
    }
    CHECK_GELIST(1);
    internal_add_commandf(GECMD_LIGHT0_SPOTLIM + light, cutoff);
}

/*************************************************************************/
/*************************************************************************/
