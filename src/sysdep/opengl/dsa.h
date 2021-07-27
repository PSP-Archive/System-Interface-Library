/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/opengl/dsa.h: Header for direct state access wrappers.
 */

#ifndef SIL_SRC_SYSDEP_OPENGL_DSA_H
#define SIL_SRC_SYSDEP_OPENGL_DSA_H

#include "src/sysdep/opengl/gl-headers.h"

/*************************************************************************/
/*************************************************************************/

/**
 * opengl_current_texture_unit:  Index of the texture unit currently set
 * with glActiveTexture().  Used to avoid a library call on systems lacking
 * OpenGL DSA functions on consecutive calls to glBindTextureUnit() with
 * the same texture unit.
 */
extern GLuint opengl_current_texture_unit;

/*-----------------------------------------------------------------------*/

/* Each of these functions "wraps" the indicated OpenGL function in that
 * it provides the same behavior using pre-DSA functions.  When the DSA
 * functions are not available (or are explicitly disabled during testing),
 * these functions are installed in the dyngl.c function pointers so that
 * other code can call the DSA functions without needing to check for the
 * presence of DSA or include fallback code for non-DSA systems. */

GLAPI void GLAPIENTRY wrap_glBindTextureUnit(GLuint unit, GLuint texture);
GLAPI GLenum GLAPIENTRY wrap_glCheckNamedFramebufferStatus(
    GLuint framebuffer, GLenum target);
GLAPI void GLAPIENTRY wrap_glCompressedTextureSubImage2D(
    GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLsizei imageSize, const void *data);
GLAPI void GLAPIENTRY wrap_glCopyTextureSubImage2D(
    GLuint texture, GLint level, GLint xoffset, GLint yoffset,
    GLint x, GLint y, GLsizei width, GLsizei height);
GLAPI void GLAPIENTRY wrap_glCreateFramebuffers(GLsizei n, GLuint *ids);
GLAPI void GLAPIENTRY wrap_glCreateProgramPipelines(GLsizei n, GLuint *ids);
GLAPI void GLAPIENTRY wrap_glCreateRenderbuffers(GLsizei n, GLuint *ids);
GLAPI void GLAPIENTRY wrap_glCreateTextures(GLenum target, GLsizei n,
                                            GLuint *ids);
GLAPI void GLAPIENTRY wrap_glGenerateTextureMipmap(GLuint texture);
GLAPI void GLAPIENTRY wrap_glGetTextureImage(
    GLuint texture, GLint level, GLenum format, GLenum type,
    GLsizei bufSize, void *pixels);
GLAPI void GLAPIENTRY wrap_glInvalidateNamedFramebufferData(
    GLuint framebuffer, GLsizei numAttachments, const GLenum *attachments);
GLAPI void GLAPIENTRY wrap_glNamedFramebufferRenderbuffer(
    GLuint framebuffer, GLenum attachment, GLenum renderbuffertarget,
    GLuint renderbuffer);
GLAPI void GLAPIENTRY wrap_glNamedFramebufferTexture(
    GLuint framebuffer, GLenum attachment, GLuint texture, GLint level);
GLAPI void GLAPIENTRY wrap_glNamedRenderbufferStorage(
    GLuint renderbuffer, GLenum internalformat, GLsizei width, GLsizei height);
GLAPI void GLAPIENTRY wrap_glTextureParameteri(GLuint texture, GLenum pname,
                                               GLint param);
GLAPI void GLAPIENTRY wrap_glTextureStorage2D(
    GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width,
    GLsizei height);
GLAPI void GLAPIENTRY wrap_glTextureSubImage2D(
    GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLenum type, const void *pixels);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_OPENGL_DSA_H
