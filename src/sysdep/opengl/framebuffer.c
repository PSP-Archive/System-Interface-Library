/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/opengl/framebuffer.c: Framebuffer management functionality
 * for OpenGL-based platforms.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/framebuffer.h"
#include "src/graphics.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/opengl/dyngl.h"
#include "src/sysdep/opengl/internal.h"
#include "src/sysdep/opengl/opengl.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Default framebuffer ID.  This is applied when rendering to the display. */
static GLuint default_framebuffer = 0;

/* Currently bound framebuffer, or NULL if none. */
static SysFramebuffer *current_framebuffer = NULL;

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int sys_(framebuffer_supported)(void)
{
    return opengl_has_features(OPENGL_FEATURE_FRAMEBUFFERS);
}

/*-----------------------------------------------------------------------*/

SysFramebuffer *sys_(framebuffer_create)(
    int width, int height, FramebufferColorType color_type,
    int depth_bits, int stencil_bits)
{
    /* If the OpenGL framebuffer extension is not available, we can't do
     * anything. */

    if (!opengl_has_features(OPENGL_FEATURE_FRAMEBUFFERS)) {
        return NULL;
    }

    /* Determine the proper texture data type and associated texcolor_type. */

    GLenum tex_format = GL_INVALID_ENUM;
    int texcolor_type;
    switch (color_type) {
      case FBCOLOR_RGB8:
        tex_format = GL_RGBA8;
        texcolor_type = TEXCOLOR_RGB;
        break;
      case FBCOLOR_RGBA8:
        tex_format = GL_RGBA8;
        texcolor_type = TEXCOLOR_RGBA;
        break;
    }
    if (tex_format == GL_INVALID_ENUM) {
        DLOG("Invalid framebuffer color type: %d", tex_format);
        return NULL;
    }

    /* Determine the proper depth and (if applicable) stencil buffer format
     * based on the requested bit depths.  We prefer a packed depth/stencil
     * buffer if such a format is available. */

    GLenum depth_format = 0, stencil_format = 0;
    const int has_packed_24_8 = (
        opengl_major_version() >= 3  // Mandatory in GL 3.0 and GLES 3.0.
        || opengl_has_extension("GL_EXT_packed_depth_stencil")
        || opengl_has_extension("GL_OES_packed_depth_stencil"));
    const int has_packed_32_8 = (opengl_major_version() >= 3);
    GLenum depth32_format = GL_DEPTH_COMPONENT32F;
#ifdef SIL_OPENGL_ES
    if (opengl_major_version() == 2) {
        depth_format = 0x81A7;  // DEPTH_COMPONENT32_OES
    }
#endif
    if (depth_bits <= 16 && stencil_bits == 0) {
        depth_format = GL_DEPTH_COMPONENT16;
    } else if (depth_bits <= 24 && stencil_bits == 0) {
        depth_format = GL_DEPTH_COMPONENT24;
    } else if (depth_bits <= 24 && stencil_bits <= 8 && has_packed_24_8) {
        depth_format = GL_DEPTH24_STENCIL8;
    } else if (depth_bits <= 32 && stencil_bits == 0) {
        depth_format = depth32_format;
    } else if (depth_bits <= 32 && stencil_bits <= 8 && has_packed_32_8) {
        depth_format = GL_DEPTH32F_STENCIL8;
    } else if (depth_bits <= 16 && stencil_bits <= 8) {
        depth_format = GL_DEPTH_COMPONENT16;
        stencil_format = GL_STENCIL_INDEX8;
    } else if (depth_bits <= 24 && stencil_bits <= 8) {
        depth_format = GL_DEPTH_COMPONENT24;
        stencil_format = GL_STENCIL_INDEX8;
    } else if (depth_bits <= 32 && stencil_bits <= 8) {
        depth_format = depth32_format;
        stencil_format = GL_STENCIL_INDEX8;
    }
    if (!depth_format) {
        DLOG("Unsupported depth/stencil bit depths: %d/%d",
             depth_bits, stencil_bits);
        return NULL;
    }

    /* Allocate and initialize the SysFramebuffer structure. */

    SysFramebuffer *framebuffer = mem_alloc(sizeof(*framebuffer), 0, 0);
    if (UNLIKELY(!framebuffer)) {
        DLOG("Failed to allocate SysFramebuffer");
        goto error_return;
    }

    framebuffer->generation       = opengl_device_generation;
    framebuffer->width            = width;
    framebuffer->height           = height;
    framebuffer->depth_format     = depth_format;
    framebuffer->stencil_format   = stencil_format;
    framebuffer->has_stencil      = (stencil_bits > 0);
    framebuffer->separate_stencil = (stencil_format != 0);

    framebuffer->texture.generation   = opengl_device_generation;
    framebuffer->texture.width        = width;
    framebuffer->texture.height       = height;
    framebuffer->texture.color_type   = texcolor_type;
    framebuffer->texture.auto_mipmaps = 0;
    framebuffer->texture.has_mipmaps  = 0;
    framebuffer->texture.repeat_u     = 0;
    framebuffer->texture.repeat_v     = 0;
    framebuffer->texture.antialias    = 1;
    framebuffer->texture.empty        = 1;
    framebuffer->texture.readable     = 1;
    framebuffer->texture.lock_buf     = NULL;

    /* Allocate OpenGL resources.  We try to avoid allocating a depth
     * buffer if none is requested, but when using a packed depth/stencil
     * format, we need to allocate a depth buffer even if only a stencil
     * buffer is requested. */

    opengl_clear_error();
    framebuffer->framebuffer = 0;
    framebuffer->depth_buffer = 0;
    framebuffer->stencil_buffer = 0;
    framebuffer->texture.id = 0;

    glCreateFramebuffers(1, &framebuffer->framebuffer);
    if (UNLIKELY(!framebuffer->framebuffer)) {
        DLOG("Failed to generate an OpenGL framebuffer ID (0x%X)",
             glGetError());
        goto error_free_framebuffer;
    }
    if (depth_bits != 0
     || (stencil_bits != 0 && !framebuffer->separate_stencil)) {
        glCreateRenderbuffers(1, &framebuffer->depth_buffer);
        if (UNLIKELY(!framebuffer->depth_buffer)) {
            DLOG("Failed to generate an OpenGL renderbuffer ID (0x%X)",
                 glGetError());
            goto error_delete_gl_framebuffer;
        }
    }
    if (stencil_format != 0) {
        glCreateRenderbuffers(1, &framebuffer->stencil_buffer);
        if (UNLIKELY(!framebuffer->stencil_buffer)) {
            DLOG("Failed to generate an OpenGL renderbuffer ID (0x%X)",
                 glGetError());
            goto error_delete_gl_depth_buffer;
        }
    }
    glCreateTextures(GL_TEXTURE_2D, 1, &framebuffer->texture.id);
    if (UNLIKELY(!framebuffer->texture.id)) {
        DLOG("Failed to generate an OpenGL texture ID (0x%X)", glGetError());
        goto error_delete_gl_stencil_buffer;
    }

    if (framebuffer->depth_buffer) {
        glNamedRenderbufferStorage(framebuffer->depth_buffer,
                                   depth_format, width, height);
    }
    if (framebuffer->separate_stencil) {
        glNamedRenderbufferStorage(framebuffer->stencil_buffer,
                                   stencil_format, width, height);
    }
    glTextureParameteri(framebuffer->texture.id,
                        GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(framebuffer->texture.id,
                        GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(framebuffer->texture.id,
                        GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(framebuffer->texture.id,
                        GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureStorage2D(framebuffer->texture.id, 1, tex_format,
                       width, height);

    if (UNLIKELY(glGetError() != GL_NO_ERROR)) {
        DLOG("Failed to initialize framebuffer resources (%dx%d)",
             width, height);
        goto error_delete_gl_texture;
    }

    /* Bind the texture and depth buffer objects to the framebuffer, and
     * make sure the system accepts the result. */

    glNamedFramebufferTexture(
        framebuffer->framebuffer, GL_COLOR_ATTACHMENT0,
        framebuffer->texture.id, 0);
    if (framebuffer->depth_buffer) {
        glNamedFramebufferRenderbuffer(
            framebuffer->framebuffer, GL_DEPTH_ATTACHMENT,
            GL_RENDERBUFFER, framebuffer->depth_buffer);
    }
    if (stencil_bits > 0) {
        glNamedFramebufferRenderbuffer(
            framebuffer->framebuffer, GL_STENCIL_ATTACHMENT,
            GL_RENDERBUFFER, (framebuffer->separate_stencil
                              ? framebuffer->stencil_buffer
                              : framebuffer->depth_buffer));
    }
    const GLenum status = glCheckNamedFramebufferStatus(
        framebuffer->framebuffer, GL_FRAMEBUFFER);
    if (UNLIKELY(status != GL_FRAMEBUFFER_COMPLETE)) {
        if (status == GL_FRAMEBUFFER_UNSUPPORTED) {
            DLOG("Framebuffer not supported by system (size %dx%d)",
                 width, height);
        } else if (status == GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT) {
            DLOG("Framebuffer reported to be incomplete");
        } else {
            DLOG("Unknown error 0x%X while setting up framebuffer (size"
                 " %dx%d)", status, width, height);
        }
        goto error_delete_gl_texture;
    }

    /* Return the new framebuffer. */

    return framebuffer;

    /* Error handling. */

  error_delete_gl_texture:
    opengl_delete_texture(framebuffer->texture.id);
  error_delete_gl_stencil_buffer:
    if (stencil_format != 0) {
        opengl_delete_renderbuffer(framebuffer->stencil_buffer);
    }
  error_delete_gl_depth_buffer:
    opengl_delete_renderbuffer(framebuffer->depth_buffer);
  error_delete_gl_framebuffer:
    opengl_delete_framebuffer(framebuffer->framebuffer);
  error_free_framebuffer:
    mem_free(framebuffer);
  error_return:
    return NULL;
}

/*-----------------------------------------------------------------------*/

void sys_(framebuffer_destroy)(SysFramebuffer *framebuffer)
{
    if (current_framebuffer == framebuffer) {
        sys_(framebuffer_bind)(NULL);
    }
    if (opengl_current_texture == &framebuffer->texture) {
        sys_(texture_apply)(0, NULL);
    }

    if (framebuffer->generation == opengl_device_generation) {
        opengl_delete_texture(framebuffer->texture.id);
        if (framebuffer->separate_stencil) {
            opengl_delete_renderbuffer(framebuffer->stencil_buffer);
        }
        opengl_delete_renderbuffer(framebuffer->depth_buffer);
        opengl_delete_framebuffer(framebuffer->framebuffer);
    }

    mem_free(framebuffer);
}

/*-----------------------------------------------------------------------*/

void sys_(framebuffer_bind)(SysFramebuffer *framebuffer)
{
    if (opengl_has_features(OPENGL_FEATURE_FRAMEBUFFERS)) {
        if (framebuffer) {
            if (LIKELY(framebuffer->generation == opengl_device_generation)) {
                glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->framebuffer);
            } else {
                DLOG("Attempt to use invalidated framebuffer %p", framebuffer);
            }
        } else {
            glBindFramebuffer(GL_FRAMEBUFFER, default_framebuffer);
        }
        current_framebuffer = framebuffer;
        opengl_framebuffer_changed = 1;
        opengl_apply_viewport();
    }
}

/*-----------------------------------------------------------------------*/

SysTexture *sys_(framebuffer_get_texture)(SysFramebuffer *framebuffer)
{
    return &framebuffer->texture;
}

/*-----------------------------------------------------------------------*/

void sys_(framebuffer_set_antialias)(SysFramebuffer *framebuffer, int on)
{
    if (UNLIKELY(framebuffer->generation != opengl_device_generation)) {
        DLOG("Attempt to use invalidated framebuffer %p", framebuffer);
        return;
    }

    if (on && !framebuffer->texture.antialias) {
        glTextureParameteri(framebuffer->texture.id,
                            GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(framebuffer->texture.id,
                            GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        framebuffer->texture.antialias = 1;
    } else if (!on && framebuffer->texture.antialias) {
        glTextureParameteri(framebuffer->texture.id,
                            GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(framebuffer->texture.id,
                            GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        framebuffer->texture.antialias = 0;
    }
}

/*-----------------------------------------------------------------------*/

void sys_(framebuffer_discard_data)(SysFramebuffer *framebuffer)
{
    if (UNLIKELY(framebuffer->generation != opengl_device_generation)) {
        DLOG("Attempt to use invalidated framebuffer %p", framebuffer);
        return;
    }

    if (!opengl_has_features(OPENGL_FEATURE_DISCARD_FRAMEBUFFER)) {
        return;
    }

    static const GLenum attachments[] = {
        GL_COLOR_ATTACHMENT0,
        GL_DEPTH_ATTACHMENT,
        GL_STENCIL_ATTACHMENT,
    };
#ifdef SIL_OPENGL_ES
    if (opengl_major_version() >= 3)
#endif
    {
        glInvalidateNamedFramebufferData(framebuffer->framebuffer,
                                         lenof(attachments), attachments);
    }
#ifdef SIL_OPENGL_ES
    else {
        if (current_framebuffer != framebuffer) {
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->framebuffer);
        }
        glDiscardFramebufferEXT(GL_FRAMEBUFFER, lenof(attachments), attachments);
        if (current_framebuffer != framebuffer) {
            if (current_framebuffer) {
                glBindFramebuffer(GL_FRAMEBUFFER,
                                  current_framebuffer->framebuffer);
            } else {
                glBindFramebuffer(GL_FRAMEBUFFER, default_framebuffer);
            }
        }
    }
#endif
}

/*************************************************************************/
/*********************** Exported utility routines ***********************/
/*************************************************************************/

void opengl_set_default_framebuffer(GLuint default_fb)
{
    default_framebuffer = default_fb;
}

/*-----------------------------------------------------------------------*/

GLuint opengl_get_default_framebuffer(void)
{
    return default_framebuffer;
}

/*************************************************************************/
/******************* Library-internal utility routines *******************/
/*************************************************************************/

SysFramebuffer *opengl_current_framebuffer(void)
{
    return current_framebuffer;
}

/*************************************************************************/
/*************************************************************************/
