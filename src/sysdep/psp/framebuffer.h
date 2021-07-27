/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/framebuffer.h: Framebuffer data structure for the PSP.
 */

#ifndef SIL_SRC_SYSDEP_PSP_FRAMEBUFFER_H
#define SIL_SRC_SYSDEP_PSP_FRAMEBUFFER_H

#include "src/sysdep/psp/texture.h"  // Needed for SysTexture definition.

/*************************************************************************/
/*************************************************************************/

/* PSP-internal data structure used for framebuffers. */

struct SysFramebuffer {
    /* VRAM pointers for this framebuffer. */
    void *pixels;
    void *depth;  // NULL if no depth component in the framebuffer.

    /* Framebuffer size and line stride (all in pixels). */
    unsigned int width, height, stride;

    /* Texture data for applying this framebuffer as a texture. */
    SysTexture texture;
};

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_PSP_FRAMEBUFFER_H
