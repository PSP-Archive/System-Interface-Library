/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/opengl/dsa.c: Wrappers used to simulate OpenGL direct state
 * access for platforms which lack it.
 */

/*
 * Note that many of the texture function wrappers assume that all textures
 * are of type GL_TEXTURE_2D, which is currently the only texture type we use.
 */

#include "src/base.h"
#include "src/sysdep.h"
#include "src/sysdep/opengl/dsa.h"
#include "src/sysdep/opengl/internal.h"
#include "src/sysdep/opengl/opengl.h"

#ifdef SIL_OPENGL_ES
# define UNUSED_IF_GLES  UNUSED
#else
# define UNUSED_IF_GLES  /*nothing*/
#endif

/*************************************************************************/
/************** Exported data (local to the OpenGL library) **************/
/*************************************************************************/

GLuint opengl_current_texture_unit;

/*************************************************************************/
/*************************** Wrapper functions ***************************/
/*************************************************************************/

GLAPI void GLAPIENTRY wrap_glBindTextureUnit(GLuint unit, GLuint texture)
{
    if (unit != opengl_current_texture_unit) {
        glActiveTexture(GL_TEXTURE0 + unit);
        opengl_current_texture_unit = unit;
    }
    glBindTexture(GL_TEXTURE_2D, texture);
}

/*-----------------------------------------------------------------------*/

GLAPI GLenum GLAPIENTRY wrap_glCheckNamedFramebufferStatus(
     GLuint framebuffer, GLenum target)
{
    GLuint current_framebuffer;
#ifdef SIL_OPENGL_ES
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, (GLint *)&current_framebuffer);
#else
    GLuint current_read_framebuffer;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&current_framebuffer);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,
                  (GLint *)&current_read_framebuffer);
#endif
    if (framebuffer != current_framebuffer) {
        glBindFramebuffer(target, framebuffer);
    }

    GLenum status = glCheckFramebufferStatus(target);

#ifdef SIL_OPENGL_ES
    glBindFramebuffer(GL_FRAMEBUFFER, current_framebuffer);
#else
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, current_framebuffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, current_read_framebuffer);
#endif

    return status;
}

/*-----------------------------------------------------------------------*/

GLAPI void GLAPIENTRY wrap_glCompressedTextureSubImage2D(
    GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLsizei imageSize, const void *data)
{
    GLuint current_texture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, (GLint *)&current_texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glCompressedTexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset,
                              width, height, format, imageSize, data);

    glBindTexture(GL_TEXTURE_2D, current_texture);
}

/*-----------------------------------------------------------------------*/

GLAPI void GLAPIENTRY wrap_glCopyTextureSubImage2D(
    GLuint texture, GLint level, GLint xoffset, GLint yoffset,
    GLint x, GLint y, GLsizei width, GLsizei height)
{
    GLuint current_texture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, (GLint *)&current_texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glCopyTexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset, x, y,
                        width, height);

    glBindTexture(GL_TEXTURE_2D, current_texture);
}

/*-----------------------------------------------------------------------*/

GLAPI void GLAPIENTRY wrap_glCreateFramebuffers(GLsizei n, GLuint *ids)
{
    GLuint current_framebuffer;
#ifdef SIL_OPENGL_ES
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, (GLint *)&current_framebuffer);
#else
    GLuint current_read_framebuffer;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&current_framebuffer);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,
                  (GLint *)&current_read_framebuffer);
#endif

    glGenFramebuffers(n, ids);
    for (GLsizei i = 0; i < n; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, ids[i]);
    }

#ifdef SIL_OPENGL_ES
    glBindFramebuffer(GL_FRAMEBUFFER, current_framebuffer);
#else
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, current_framebuffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, current_read_framebuffer);
#endif
}

/*-----------------------------------------------------------------------*/

GLAPI void GLAPIENTRY wrap_glCreateProgramPipelines(GLsizei n, GLuint *ids)
{
    GLuint current_pipeline;
    glGetIntegerv(GL_PROGRAM_PIPELINE_BINDING, (GLint *)&current_pipeline);

    glGenProgramPipelines(n, ids);
    for (GLsizei i = 0; i < n; i++) {
        glBindProgramPipeline(ids[i]);
    }

    glBindProgramPipeline(current_pipeline);
}

/*-----------------------------------------------------------------------*/

GLAPI void GLAPIENTRY wrap_glCreateRenderbuffers(GLsizei n, GLuint *ids)
{
    GLuint current_renderbuffer;
    glGetIntegerv(GL_RENDERBUFFER_BINDING, (GLint *)&current_renderbuffer);

    glGenRenderbuffers(n, ids);
    for (GLsizei i = 0; i < n; i++) {
        glBindRenderbuffer(GL_RENDERBUFFER, ids[i]);
    }

    glBindRenderbuffer(GL_RENDERBUFFER, current_renderbuffer);
}

/*-----------------------------------------------------------------------*/

GLAPI void GLAPIENTRY wrap_glCreateTextures(GLenum target, GLsizei n,
                                            GLuint *ids)
{
    ASSERT(target == GL_TEXTURE_2D, return);

    GLuint current_texture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, (GLint *)&current_texture);

    glGenTextures(n, ids);
    for (GLsizei i = 0; i < n; i++) {
        glBindTexture(target, ids[i]);
    }

    glBindTexture(target, current_texture);
}

/*-----------------------------------------------------------------------*/

GLAPI void GLAPIENTRY wrap_glGenerateTextureMipmap(GLuint texture)
{
    GLuint current_texture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, (GLint *)&current_texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, current_texture);
}

/*-----------------------------------------------------------------------*/

GLAPI void GLAPIENTRY wrap_glGetTextureImage(
    GLuint texture, UNUSED_IF_GLES GLint level, UNUSED_IF_GLES GLenum format,
    UNUSED_IF_GLES GLenum type, UNUSED GLsizei bufSize,
    UNUSED_IF_GLES void *pixels)
{
    GLuint current_texture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, (GLint *)&current_texture);
    glBindTexture(GL_TEXTURE_2D, texture);

#ifdef SIL_OPENGL_ES
    DLOG("Invalid call to glGetTexImage()");
    glEnable(GL_INVALID_ENUM);  // Force a GL error.
#else
    glGetTexImage(GL_TEXTURE_2D, level, format, type, pixels);
#endif

    glBindTexture(GL_TEXTURE_2D, current_texture);
}

/*-----------------------------------------------------------------------*/

GLAPI void GLAPIENTRY wrap_glInvalidateNamedFramebufferData(
    GLuint framebuffer, GLsizei numAttachments, const GLenum *attachments)
{
    GLuint current_framebuffer;
#ifdef SIL_OPENGL_ES
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, (GLint *)&current_framebuffer);
#else
    GLuint current_read_framebuffer;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&current_framebuffer);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,
                  (GLint *)&current_read_framebuffer);
#endif
    if (framebuffer != current_framebuffer) {
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    }

    glInvalidateFramebuffer(GL_FRAMEBUFFER, numAttachments, attachments);

#ifdef SIL_OPENGL_ES
    glBindFramebuffer(GL_FRAMEBUFFER, current_framebuffer);
#else
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, current_framebuffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, current_read_framebuffer);
#endif
}

/*-----------------------------------------------------------------------*/

GLAPI void GLAPIENTRY wrap_glNamedFramebufferRenderbuffer(
    GLuint framebuffer, GLenum attachment, GLenum renderbuffertarget,
    GLuint renderbuffer)
{
    GLuint current_framebuffer;
#ifdef SIL_OPENGL_ES
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, (GLint *)&current_framebuffer);
#else
    GLuint current_read_framebuffer;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&current_framebuffer);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,
                  (GLint *)&current_read_framebuffer);
#endif
    if (framebuffer != current_framebuffer) {
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    }

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment,
                              renderbuffertarget, renderbuffer);

#ifdef SIL_OPENGL_ES
    glBindFramebuffer(GL_FRAMEBUFFER, current_framebuffer);
#else
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, current_framebuffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, current_read_framebuffer);
#endif
}

/*-----------------------------------------------------------------------*/

GLAPI void GLAPIENTRY wrap_glNamedFramebufferTexture(
    GLuint framebuffer, GLenum attachment, GLuint texture, GLint level)
{
    GLuint current_framebuffer;
#ifdef SIL_OPENGL_ES
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, (GLint *)&current_framebuffer);
#else
    GLuint current_read_framebuffer;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, (GLint *)&current_framebuffer);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,
                  (GLint *)&current_read_framebuffer);
#endif
    if (framebuffer != current_framebuffer) {
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    }

    glFramebufferTexture2D(GL_FRAMEBUFFER, attachment,
                           GL_TEXTURE_2D, texture, level);

#ifdef SIL_OPENGL_ES
    glBindFramebuffer(GL_FRAMEBUFFER, current_framebuffer);
#else
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, current_framebuffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, current_read_framebuffer);
#endif
}

/*-----------------------------------------------------------------------*/

GLAPI void GLAPIENTRY wrap_glNamedRenderbufferStorage(
    GLuint renderbuffer, GLenum internalformat, GLsizei width, GLsizei height)
{
    GLuint current_renderbuffer;
    glGetIntegerv(GL_RENDERBUFFER_BINDING, (GLint *)&current_renderbuffer);
    if (renderbuffer != current_renderbuffer) {
        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
    }

    glRenderbufferStorage(GL_RENDERBUFFER, internalformat, width, height);

    glBindRenderbuffer(GL_RENDERBUFFER, current_renderbuffer);
}

/*-----------------------------------------------------------------------*/

GLAPI void GLAPIENTRY wrap_glTextureParameteri(GLuint texture, GLenum pname,
                                               GLint param)
{
    GLuint current_texture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, (GLint *)&current_texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, pname, param);

    glBindTexture(GL_TEXTURE_2D, current_texture);
}

/*-----------------------------------------------------------------------*/

GLAPI void GLAPIENTRY wrap_glTextureStorage2D(
    GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width,
    GLsizei height)
{
    GLuint current_texture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, (GLint *)&current_texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    if (opengl_has_features(OPENGL_FEATURE_TEXTURE_STORAGE)) {
        glTexStorage2D(GL_TEXTURE_2D, levels, internalformat, width, height);
    } else {
        GLenum format;
        if (!opengl_version_is_at_least(3,0)) {
            /* On GL/GLES 2.x, we're only called from sys_framebuffer_create(),
             * sys_texture_grab(), and (potentially update_texture();
             * sys_texture_create() has separate logic to handle the various
             * hacks needed to create textures of non-RGB8/RGBA8 formats. */
            if (internalformat == GL_RGB8) {
                internalformat = GL_RGB;
            } else {
                ASSERT(internalformat == GL_RGBA8);
                internalformat = GL_RGBA;
            }
            format = internalformat;
        } else {
            format = GL_RGBA;
        }
        for (GLsizei level = 0; level < levels; level++) {
            glTexImage2D(GL_TEXTURE_2D, level, internalformat, width,
                         height, 0, format, GL_UNSIGNED_BYTE, NULL);
            width = lbound(width/2, 1);
            height = lbound(height/2, 1);
        }
    }

    glBindTexture(GL_TEXTURE_2D, current_texture);
}

/*-----------------------------------------------------------------------*/

GLAPI void GLAPIENTRY wrap_glTextureSubImage2D(
    GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLenum type, const void *pixels)
{
    GLuint current_texture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, (GLint *)&current_texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset,
                    width, height, format, type, pixels);

    glBindTexture(GL_TEXTURE_2D, current_texture);
}

/*************************************************************************/
/*************************************************************************/
