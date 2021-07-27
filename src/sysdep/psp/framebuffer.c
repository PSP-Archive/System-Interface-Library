/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/framebuffer.c: Framebuffer management functionality for the
 * PSP.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/framebuffer.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/psp/framebuffer.h"
#include "src/sysdep/psp/ge-util.h"
#include "src/sysdep/psp/internal.h"

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int sys_framebuffer_supported(void)
{
    return 1;
}

/*-----------------------------------------------------------------------*/

SysFramebuffer *sys_framebuffer_create(
    int width, int height, FramebufferColorType color_type,
    int depth_bits, int stencil_bits)
{
    if (color_type != FBCOLOR_RGB8 || depth_bits > 16 || stencil_bits > 8) {
        DLOG("Unsupported framebuffer configuration: %d %d %d",
             color_type, depth_bits, stencil_bits);
        return NULL;
    }

    SysFramebuffer *framebuffer = mem_alloc(sizeof(*framebuffer), 0, 0);
    if (UNLIKELY(!framebuffer)) {
        DLOG("Failed to allocate SysFramebuffer");
        return NULL;
    }

    int stride;
    if (depth_bits > 0) {
        stride = align_up(width, 8);
    } else {
        stride = align_up(width, 4);
    }

    framebuffer->width  = width;
    framebuffer->height = height;
    framebuffer->stride = stride;
    const uint32_t size = stride * height * 4;
    framebuffer->pixels = psp_vram_alloc(size, 64);
    if (UNLIKELY(!framebuffer->pixels)) {
        DLOG("Failed to allocate VRAM for %dx%d framebuffer (%u bytes)",
             width, height, size);
        mem_free(framebuffer);
        return NULL;
    }

    if (depth_bits > 0) {
        /*
         * The GE's depth buffer circuitry flips some address lines around
         * when reading and writing depth values; in particular, address
         * line 13 (bit 0x2000) is inverted, which implies that depth
         * buffer addresses and sizes must be aligned to a multiple of
         * 16k(!) to avoid clobbering other data in VRAM.  Note that, for
         * depth buffers associated with 32-bit color buffers, reading
         * from (VRAM address + 0x600000) triggers a hardware swizzle
         * which allows viewing the depth buffer data in linear format.
         * (VRAM address + 0x200000) is reported to do the same for depth
         * buffers associated with 16-bit color buffers.  See, e.g.:
         * http://lukasz.dk/mirror/forums.ps2dev.org/viewtopic27f9.html?t=4421
         */
        const uint32_t depth_size = align_up(stride * height * 2, 16384);
        framebuffer->depth = psp_vram_alloc(depth_size, 16384);
        if (UNLIKELY(!framebuffer->depth)) {
            DLOG("Failed to allocate VRAM for %dx%d depth buffer (%u bytes)",
                 width, height, depth_size);
            psp_vram_free(framebuffer->pixels);
            mem_free(framebuffer);
            return NULL;
        }
    } else {
        framebuffer->depth = NULL;
    }

    framebuffer->texture.width     = width;
    framebuffer->texture.height    = height;
    framebuffer->texture.stride    = stride;
    framebuffer->texture.format    = GE_TEXFMT_8888;
    framebuffer->texture.swizzled  = 0;
    framebuffer->texture.mipmaps   = 0;
    framebuffer->texture.pixels[0] = framebuffer->pixels;
    framebuffer->texture.palette   = NULL;
    framebuffer->texture.repeat_u  = 0;
    framebuffer->texture.repeat_v  = 0;
    framebuffer->texture.antialias = 1;
    framebuffer->texture.lock_buf  = NULL;

    return framebuffer;
}

/*-----------------------------------------------------------------------*/

void sys_framebuffer_destroy(SysFramebuffer *framebuffer)
{
    if (psp_current_framebuffer() == framebuffer) {
        psp_use_framebuffer(NULL);
    }
    if (psp_current_texture() == &framebuffer->texture) {
        sys_texture_apply(0, NULL);
    }

    /* Make sure we sync before freeing the VRAM, so the GE doesn't stomp
     * on subsequently-allocated buffers. */
    ge_sync();

    psp_vram_free(framebuffer->depth);
    psp_vram_free(framebuffer->pixels);
    mem_free(framebuffer);
}

/*-----------------------------------------------------------------------*/

void sys_framebuffer_bind(SysFramebuffer *framebuffer)
{
    if (framebuffer) {
        psp_use_framebuffer(framebuffer);
    } else {
        psp_use_framebuffer(NULL);
    }
}

/*-----------------------------------------------------------------------*/

SysTexture *sys_framebuffer_get_texture(SysFramebuffer *framebuffer)
{
    return &framebuffer->texture;
}

/*-----------------------------------------------------------------------*/

void sys_framebuffer_set_antialias(SysFramebuffer *framebuffer, int on)
{
    sys_texture_set_antialias(&framebuffer->texture, on);
}

/*-----------------------------------------------------------------------*/

void sys_framebuffer_discard_data(UNUSED SysFramebuffer *framebuffer)
{
    /* Ignore (we can't make use of the hint). */
}

/*************************************************************************/
/*************************************************************************/
