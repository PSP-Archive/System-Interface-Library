/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/graphics/framebuffer.c: Framebuffer management.
 */

#include "src/base.h"
#include "src/framebuffer.h"
#include "src/graphics.h"
#include "src/graphics/internal.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/texture.h"
#include "src/utility/id-array.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Simple data structure for managing framebuffer data. */
typedef struct Framebuffer Framebuffer;
struct Framebuffer {
    SysFramebuffer *sysfb;
    int width, height;
    int texture;
};

/* Array of allocated framebuffers. */
static IDArray framebuffers = ID_ARRAY_INITIALIZER(10);

/*-----------------------------------------------------------------------*/

/**
 * VALIDATE_FRAMEBUFFER:  Validate the framebuffer ID passed to a
 * framebuffer management routine, and store the corresponding pointer in
 * the variable "framebuffer".  If the framebuffer ID is invalid, the
 * "error_return" statement is executed; this may consist of multiple
 * statements, but must include a "return" to exit the function.
 */
#define VALIDATE_FRAMEBUFFER(id,framebuffer,error_return) \
    ID_ARRAY_VALIDATE(&framebuffers, (id), Framebuffer *, framebuffer, \
                      DLOG("Framebuffer ID %d is invalid", _id); error_return)

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int framebuffer_supported(void)
{
    return sys_framebuffer_supported();
}

/*-----------------------------------------------------------------------*/

int framebuffer_create(int width, int height, FramebufferColorType color_type,
                       int depth_bits, int stencil_bits)
{
    if (UNLIKELY(width <= 0)
     || UNLIKELY(height <= 0)
     || UNLIKELY(color_type == 0)
     || UNLIKELY(depth_bits < 0)
     || UNLIKELY(stencil_bits < 0)) {
        DLOG("Invalid parameters: %d %d %d %d %d",
             width, height, color_type, depth_bits, stencil_bits);
        goto error_return;
    }

    Framebuffer *framebuffer = mem_alloc(sizeof(*framebuffer), 0, 0);
    if (UNLIKELY(!framebuffer)) {
        DLOG("No memory for framebuffer instance");
        goto error_return;
    }

    framebuffer->sysfb = sys_framebuffer_create(width, height, color_type,
                                                depth_bits, stencil_bits);
    if (!framebuffer->sysfb) {
        goto error_free_framebuffer;
    }
    framebuffer->width = width;
    framebuffer->height = height;

    SysTexture *systex = sys_framebuffer_get_texture(framebuffer->sysfb);
    framebuffer->texture = texture_import_readonly(systex);
    if (UNLIKELY(!framebuffer->texture)) {
        DLOG("Failed to register framebuffer texture");
        goto error_destroy_sysfb;
    }

    const int id = id_array_register(&framebuffers, framebuffer);
    if (UNLIKELY(!id)) {
        DLOG("Failed to register framebuffer");
        goto error_forget_texture;
    }

    return id;

  error_forget_texture:
    texture_forget_readonly(framebuffer->texture);
  error_destroy_sysfb:
    sys_framebuffer_destroy(framebuffer->sysfb);
  error_free_framebuffer:
    mem_free(framebuffer);
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

void framebuffer_destroy(int framebuffer_id)
{
    if (framebuffer_id) {
        Framebuffer *framebuffer;
        VALIDATE_FRAMEBUFFER(framebuffer_id, framebuffer, return);
        texture_forget_readonly(framebuffer->texture);
        sys_framebuffer_destroy(framebuffer->sysfb);
        mem_free(framebuffer);
        id_array_release(&framebuffers, framebuffer_id);
    }
}

/*-----------------------------------------------------------------------*/

int framebuffer_width(int framebuffer_id)
{
    Framebuffer *framebuffer;
    VALIDATE_FRAMEBUFFER(framebuffer_id, framebuffer, return 0);
    return framebuffer->width;
}

/*-----------------------------------------------------------------------*/

int framebuffer_height(int framebuffer_id)
{
    Framebuffer *framebuffer;
    VALIDATE_FRAMEBUFFER(framebuffer_id, framebuffer, return 0);
    return framebuffer->height;
}

/*-----------------------------------------------------------------------*/

void framebuffer_bind(int framebuffer_id)
{
    if (framebuffer_id) {
        Framebuffer *framebuffer;
        VALIDATE_FRAMEBUFFER(framebuffer_id, framebuffer, return);
        sys_framebuffer_bind(framebuffer->sysfb);
    } else {
        sys_framebuffer_bind(NULL);
    }
}

/*-----------------------------------------------------------------------*/

int framebuffer_get_texture(int framebuffer_id)
{
    Framebuffer *framebuffer;
    VALIDATE_FRAMEBUFFER(framebuffer_id, framebuffer, return 0);
    return framebuffer->texture;
}

/*-----------------------------------------------------------------------*/

void framebuffer_set_antialias(int framebuffer_id, int on)
{
    Framebuffer *framebuffer;
    VALIDATE_FRAMEBUFFER(framebuffer_id, framebuffer, return);
    sys_framebuffer_set_antialias(framebuffer->sysfb, on);
}

/*-----------------------------------------------------------------------*/

void framebuffer_discard_data(int framebuffer_id)
{
    Framebuffer *framebuffer;
    VALIDATE_FRAMEBUFFER(framebuffer_id, framebuffer, return);
    sys_framebuffer_discard_data(framebuffer->sysfb);
}

/*************************************************************************/
/*********************** Library-internal routines ***********************/
/*************************************************************************/

void framebuffer_cleanup(void)
{
    for (int i = 1; i < id_array_size(&framebuffers); i++) {
        if (id_array_get(&framebuffers, i)) {
            framebuffer_destroy(i);
        }
    }
}

/*************************************************************************/
/*************************************************************************/
