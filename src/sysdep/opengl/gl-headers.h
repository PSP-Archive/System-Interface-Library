/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/opengl/gl-headers.h: Header which wraps the individual OpenGL
 * headers, defining compatibility constants if necessary.
 */

#ifndef SIL_SRC_SYSDEP_OPENGL_GL_HEADERS_H
#define SIL_SRC_SYSDEP_OPENGL_GL_HEADERS_H

/*************************************************************************/
/*************************************************************************/

#define GL_GLEXT_PROTOTYPES

#ifdef SIL_OPENGL_ES

# ifdef SIL_PLATFORM_ANDROID
/* Android uses dlsym(...,RTLD_DEFAULT) (not RTLD_NEXT, which doesn't work
 * because of the load order) to search for symbols, so we need to declare
 * the GL functions as non-exported to prevent the stub functions defined
 * in dyngl.c from being found in the search and causing infinite recursion.
 * Note that the default definition of GL_APICALL (KHRONOS_APICALL) already
 * includes a visibility attribute, so we have to rewrite the definition
 * from scratch.  Also note that __NDK_FPABI__ is only present through NDK
 * r12. */
#  ifdef __NDK_FPABI__
#   define GL_APICALL  __attribute__((visibility("hidden"))) __NDK_FPABI__
#  else
#   define GL_APICALL  __attribute__((visibility("hidden")))
#  endif
# endif

# if defined(SIL_PLATFORM_IOS)

/* The iOS SDK (at least through version 9.0) doesn't include GLES 3.1, so
 * we rename all the EXT_separate_shader_objects declarations to the proper
 * GLES 3.1 names. */
#  define glBindProgramPipelineEXT  glBindProgramPipeline
#  define glDeleteProgramPipelinesEXT  glDeleteProgramPipelines
#  define glGetProgramPipelineivEXT  glGetProgramPipelineiv
#  define glGetProgramPipelineInfoLogEXT  glGetProgramPipelineInfoLog
#  define glGenProgramPipelinesEXT  glGenProgramPipelines
#  define glProgramParameteriEXT  glProgramParameteri
#  define glProgramUniform1iEXT  glProgramUniform1i
#  define glProgramUniform1fEXT  glProgramUniform1f
#  define glProgramUniform2fvEXT  glProgramUniform2fv
#  define glProgramUniform3fvEXT  glProgramUniform3fv
#  define glProgramUniform4fvEXT  glProgramUniform4fv
#  define glProgramUniformMatrix4fvEXT  glProgramUniformMatrix4fv
#  define glUseProgramStagesEXT  glUseProgramStages
#  define glValidateProgramPipelineEXT  glValidateProgramPipeline

#  include <OpenGLES/ES3/gl.h>
/* We need Apple's glext.h for Apple-specific extensions. */
#  include <OpenGLES/ES3/glext.h>

#  undef glBindProgramPipelineEXT
#  undef glDeleteProgramPipelinesEXT
#  undef glGetProgramPipelineivEXT
#  undef glGetProgramPipelineInfoLogEXT
#  undef glGenProgramPipelinesEXT
#  undef glProgramParameteriEXT
#  undef glProgramUniform1iEXT
#  undef glProgramUniform1fEXT
#  undef glProgramUniform2fvEXT
#  undef glProgramUniform3fvEXT
#  undef glProgramUniform4fvEXT
#  undef glProgramUniformMatrix4fvEXT
#  undef glUseProgramStagesEXT
#  undef glValidateProgramPipelineEXT

#  define GL_APICALL GL_API
/* Some iOS headers blindly include <ES2/gl.h>, which causes breakage after
 * including <ES3/gl.h>. */
#  define __gl_es20_h_

# else  // not iOS
#  include <GLES3/gl31.h>
# endif

# include "external/opengl-headers/GLES2/gl2ext.h"

# define GLAPI GL_APICALL
# define GLAPIENTRY GL_APIENTRY

/* Declare GLES 2.0-specific extension functions so dyngl doesn't spout
 * warnings. */
extern void glDiscardFramebufferEXT(GLenum, GLsizei, const GLenum *);

/* The direct state access functions aren't (yet?) in GLES, so we need to
 * declare them ourselves. */
GLAPI void GLAPIENTRY glBindTextureUnit(GLuint unit, GLuint texture);
GLAPI GLenum GLAPIENTRY glCheckNamedFramebufferStatus(GLuint framebuffer, GLenum target);
GLAPI void GLAPIENTRY glCompressedTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data);
GLAPI void GLAPIENTRY glCopyTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
GLAPI void GLAPIENTRY glCreateFramebuffers(GLsizei n, GLuint *framebuffers);
GLAPI void GLAPIENTRY glCreateProgramPipelines(GLsizei n, GLuint *pipelines);
GLAPI void GLAPIENTRY glCreateRenderbuffers(GLsizei n, GLuint *renderbuffers);
GLAPI void GLAPIENTRY glCreateTextures(GLenum target, GLsizei n, GLuint *textures);
GLAPI void GLAPIENTRY glGenerateTextureMipmap(GLuint texture);
GLAPI void GLAPIENTRY glGetNamedBufferSubData(GLuint buffer, GLintptr offset, GLsizeiptr size, void *data);
GLAPI void GLAPIENTRY glGetTextureImage(GLuint texture, GLint level, GLenum format, GLenum type, GLsizei bufSize, void *pixels);
GLAPI void GLAPIENTRY glInvalidateNamedFramebufferData(GLuint framebuffer, GLsizei numAttachments, const GLenum *attachments);
GLAPI void GLAPIENTRY glNamedFramebufferRenderbuffer(GLuint framebuffer, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
GLAPI void GLAPIENTRY glNamedFramebufferTexture(GLuint framebuffer, GLenum attachment, GLuint texture, GLint level);
GLAPI void GLAPIENTRY glNamedRenderbufferStorage(GLuint renderbuffer, GLenum internalformat, GLsizei width, GLsizei height);
GLAPI void GLAPIENTRY glTextureParameteri(GLuint texture, GLenum pname, GLint param);
GLAPI void GLAPIENTRY glTextureStorage2D(GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
GLAPI void GLAPIENTRY glTextureSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels);

/* Debug message functions all have the KHR suffix. */
# define glDebugMessageCallback            glDebugMessageCallbackKHR
# define glDebugMessageControl             glDebugMessageControlKHR
# define glDebugMessageInsert              glDebugMessageInsertKHR
# define GL_DEBUG_OUTPUT                   GL_DEBUG_OUTPUT_KHR
# define GL_DEBUG_OUTPUT_SYNCHRONOUS       GL_DEBUG_OUTPUT_SYNCHRONOUS_KHR
# define GL_DEBUG_SOURCE_API               GL_DEBUG_SOURCE_API_KHR
# define GL_DEBUG_SOURCE_WINDOW_SYSTEM     GL_DEBUG_SOURCE_WINDOW_SYSTEM_KHR
# define GL_DEBUG_SOURCE_SHADER_COMPILER   GL_DEBUG_SOURCE_SHADER_COMPILER_KHR
# define GL_DEBUG_SOURCE_THIRD_PARTY       GL_DEBUG_SOURCE_THIRD_PARTY_KHR
# define GL_DEBUG_SOURCE_APPLICATION       GL_DEBUG_SOURCE_APPLICATION_KHR
# define GL_DEBUG_SOURCE_OTHER             GL_DEBUG_SOURCE_OTHER_KHR
# define GL_DEBUG_TYPE_ERROR               GL_DEBUG_TYPE_ERROR_KHR
# define GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_KHR
# define GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR  GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_KHR
# define GL_DEBUG_TYPE_PORTABILITY         GL_DEBUG_TYPE_PORTABILITY_KHR
# define GL_DEBUG_TYPE_PERFORMANCE         GL_DEBUG_TYPE_PERFORMANCE_KHR
# define GL_DEBUG_TYPE_MARKER              GL_DEBUG_TYPE_MARKER_KHR
# define GL_DEBUG_TYPE_PUSH_GROUP          GL_DEBUG_TYPE_PUSH_GROUP_KHR
# define GL_DEBUG_TYPE_POP_GROUP           GL_DEBUG_TYPE_POP_GROUP_KHR
# define GL_DEBUG_TYPE_OTHER               GL_DEBUG_TYPE_OTHER_KHR
# define GL_DEBUG_SEVERITY_HIGH            GL_DEBUG_SEVERITY_HIGH_KHR
# define GL_DEBUG_SEVERITY_MEDIUM          GL_DEBUG_SEVERITY_MEDIUM_KHR
# define GL_DEBUG_SEVERITY_LOW             GL_DEBUG_SEVERITY_LOW_KHR
# define GL_DEBUG_SEVERITY_NOTIFICATION    GL_DEBUG_SEVERITY_NOTIFICATION_KHR

#else  // !SIL_OPENGL_ES

# if defined(SIL_PLATFORM_WINDOWS)
/* Native Windows <GL/gl.h> requires <windows.h> to be included beforehand. */
#  include "src/sysdep/windows/internal.h"
/* We look up all symbols dynamically and define the wrappers statically,
 * so drop the dllimport attribute. */
#  undef DECLSPEC_IMPORT
#  define DECLSPEC_IMPORT  /*nothing*/
# endif

# define __gl_glext_h_  // Prevent SDK-provided <glext.h> from being included.
# define __glext_h_  // Some header versions use this symbol instead.
# if defined(SIL_PLATFORM_MACOSX)
#  include <OpenGL/gl3.h>
# else
/* Work around a typo in old Mesa gl.h (#if should be #ifdef). */
#  define GL_ARB_shader_objects 0
#  include <GL/gl.h>
#  undef GL_ARB_shader_objects
# endif
# undef __gl_glext_h_
# undef __glext_h_
# include "external/opengl-headers/GL/glext.h"

# ifndef GLAPIENTRY
#  define GLAPIENTRY APIENTRY
# endif

# if defined(SIL_PLATFORM_WINDOWS)
#  undef DECLSPEC_IMPORT
#  define DECLSPEC_IMPORT  __declspec(dllimport)
# endif

/* Map glDepthRangef() to glDepthRange() for the benefit of old OpenGL
 * implementations that don't have the former. */
# define glDepthRangef  glDepthRange

#endif  // SIL_OPENGL_ES

/* Equivalent of GLAPI for local functions. */
#ifdef SIL_PLATFORM_ANDROID
# define GLAPI_STATIC  static __NDK_FPABI__
#else
# define GLAPI_STATIC  static
#endif

/* Define SIL_OPENGL_LOG_CALLS to log all OpenGL calls (requires GCC
 * extensions).  The log will be stored in an in-memory buffer to
 * minimize impact on timing, and dumped via the DLOG interface when
 * opengl_cleanup() is called.  _opengl_log() is defined in graphics.c.
 * Note that the logging code is not thread-safe; using background shader
 * compilation may lead to loss of some log messages. */
#if defined(DEBUG) && defined(SIL_OPENGL_LOG_CALLS)
#ifndef SUPPRESS_GLLOG  // Used to avoid problems in dyngl.c.
extern void _opengl_log(const char *s, int s_size, const char *file, int line);
# define GLLOG(call) \
    __extension__({_opengl_log(#call, strlen(#call)+1, __FILE__, __LINE__); \
                   (call);})
// grep ^FUNC src/sysdep/opengl/dyngl-funcs.h | cut -d, -f2 | sed 's/[ )]//g' | sort -u | sed 's/\(.*\)/# define \1(...) GLLOG(\1(__VA_ARGS__))/'
# define glActiveTexture(...) GLLOG(glActiveTexture(__VA_ARGS__))
# define glAttachShader(...) GLLOG(glAttachShader(__VA_ARGS__))
# define glBindAttribLocation(...) GLLOG(glBindAttribLocation(__VA_ARGS__))
# define glBindBuffer(...) GLLOG(glBindBuffer(__VA_ARGS__))
# define glBindFragDataLocation(...) GLLOG(glBindFragDataLocation(__VA_ARGS__))
# define glBindFramebuffer(...) GLLOG(glBindFramebuffer(__VA_ARGS__))
# define glBindProgramPipeline(...) GLLOG(glBindProgramPipeline(__VA_ARGS__))
# define glBindProgramPipeline(...) GLLOG(glBindProgramPipeline(__VA_ARGS__))
# define glBindRenderbuffer(...) GLLOG(glBindRenderbuffer(__VA_ARGS__))
# define glBindTexture(...) GLLOG(glBindTexture(__VA_ARGS__))
# define glBindTextureUnit(...) GLLOG(glBindTextureUnit(__VA_ARGS__))
# define glBindVertexArray(...) GLLOG(glBindVertexArray(__VA_ARGS__))
# define glBlendColor(...) GLLOG(glBlendColor(__VA_ARGS__))
# define glBlendEquation(...) GLLOG(glBlendEquation(__VA_ARGS__))
# define glBlendFunc(...) GLLOG(glBlendFunc(__VA_ARGS__))
# define glBlendFuncSeparate(...) GLLOG(glBlendFuncSeparate(__VA_ARGS__))
# define glBufferData(...) GLLOG(glBufferData(__VA_ARGS__))
# define glBufferSubData(...) GLLOG(glBufferSubData(__VA_ARGS__))
# define glCheckFramebufferStatus(...) GLLOG(glCheckFramebufferStatus(__VA_ARGS__))
# define glCheckNamedFramebufferStatus(...) GLLOG(glCheckNamedFramebufferStatus(__VA_ARGS__))
# define glClear(...) GLLOG(glClear(__VA_ARGS__))
# define glClearColor(...) GLLOG(glClearColor(__VA_ARGS__))
# define glClearDepth(...) GLLOG(glClearDepth(__VA_ARGS__))
# define glClearDepthf(...) GLLOG(glClearDepthf(__VA_ARGS__))
# define glClearStencil(...) GLLOG(glClearStencil(__VA_ARGS__))
# define glColorMask(...) GLLOG(glColorMask(__VA_ARGS__))
# define glCompileShader(...) GLLOG(glCompileShader(__VA_ARGS__))
# define glCompressedTexImage2D(...) GLLOG(glCompressedTexImage2D(__VA_ARGS__))
# define glCompressedTexSubImage2D(...) GLLOG(glCompressedTexSubImage2D(__VA_ARGS__))
# define glCompressedTextureSubImage2D(...) GLLOG(glCompressedTextureSubImage2D(__VA_ARGS__))
# define glCopyTexImage2D(...) GLLOG(glCopyTexImage2D(__VA_ARGS__))
# define glCopyTexSubImage2D(...) GLLOG(glCopyTexSubImage2D(__VA_ARGS__))
# define glCopyTextureSubImage2D(...) GLLOG(glCopyTextureSubImage2D(__VA_ARGS__))
# define glCreateFramebuffers(...) GLLOG(glCreateFramebuffers(__VA_ARGS__))
# define glCreateProgram(...) GLLOG(glCreateProgram(__VA_ARGS__))
# define glCreateProgramPipelines(...) GLLOG(glCreateProgramPipelines(__VA_ARGS__))
# define glCreateRenderbuffers(...) GLLOG(glCreateRenderbuffers(__VA_ARGS__))
# define glCreateShader(...) GLLOG(glCreateShader(__VA_ARGS__))
# define glCreateTextures(...) GLLOG(glCreateTextures(__VA_ARGS__))
# define glCullFace(...) GLLOG(glCullFace(__VA_ARGS__))
# ifdef SIL_OPENGL_ES
#  define glDebugMessageCallbackKHR(...) GLLOG(glDebugMessageCallbackKHR(__VA_ARGS__))
#  define glDebugMessageControlKHR(...) GLLOG(glDebugMessageControlKHR(__VA_ARGS__))
#  define glDebugMessageInsertKHR(...) GLLOG(glDebugMessageInsertKHR(__VA_ARGS__))
# else
#  define glDebugMessageCallback(...) GLLOG(glDebugMessageCallback(__VA_ARGS__))
#  define glDebugMessageControl(...) GLLOG(glDebugMessageControl(__VA_ARGS__))
#  define glDebugMessageInsert(...) GLLOG(glDebugMessageInsert(__VA_ARGS__))
# endif
# define glDeleteBuffers(...) GLLOG(glDeleteBuffers(__VA_ARGS__))
# define glDeleteFramebuffers(...) GLLOG(glDeleteFramebuffers(__VA_ARGS__))
# define glDeleteProgram(...) GLLOG(glDeleteProgram(__VA_ARGS__))
# define glDeleteProgramPipelines(...) GLLOG(glDeleteProgramPipelines(__VA_ARGS__))
# define glDeleteProgramPipelines(...) GLLOG(glDeleteProgramPipelines(__VA_ARGS__))
# define glDeleteRenderbuffers(...) GLLOG(glDeleteRenderbuffers(__VA_ARGS__))
# define glDeleteShader(...) GLLOG(glDeleteShader(__VA_ARGS__))
# define glDeleteTextures(...) GLLOG(glDeleteTextures(__VA_ARGS__))
# define glDeleteVertexArrays(...) GLLOG(glDeleteVertexArrays(__VA_ARGS__))
# define glDepthFunc(...) GLLOG(glDepthFunc(__VA_ARGS__))
# define glDepthMask(...) GLLOG(glDepthMask(__VA_ARGS__))
# define glDepthRange(...) GLLOG(glDepthRange(__VA_ARGS__))
# ifdef SIL_OPENGL_ES
#  define glDepthRangef(...) GLLOG(glDepthRangef(__VA_ARGS__))
# endif
# define glDetachShader(...) GLLOG(glDetachShader(__VA_ARGS__))
# define glDisable(...) GLLOG(glDisable(__VA_ARGS__))
# define glDisableVertexAttribArray(...) GLLOG(glDisableVertexAttribArray(__VA_ARGS__))
# define glDiscardFramebufferEXT(...) GLLOG(glDiscardFramebufferEXT(__VA_ARGS__))
# define glDrawArrays(...) GLLOG(glDrawArrays(__VA_ARGS__))
# define glDrawElements(...) GLLOG(glDrawElements(__VA_ARGS__))
# define glEnable(...) GLLOG(glEnable(__VA_ARGS__))
# define glEnableVertexAttribArray(...) GLLOG(glEnableVertexAttribArray(__VA_ARGS__))
# define glFinish(...) GLLOG(glFinish(__VA_ARGS__))
# define glFlush(...) GLLOG(glFlush(__VA_ARGS__))
# define glFramebufferRenderbuffer(...) GLLOG(glFramebufferRenderbuffer(__VA_ARGS__))
# define glFramebufferTexture2D(...) GLLOG(glFramebufferTexture2D(__VA_ARGS__))
# define glFrontFace(...) GLLOG(glFrontFace(__VA_ARGS__))
# define glGenBuffers(...) GLLOG(glGenBuffers(__VA_ARGS__))
# define glGenFramebuffers(...) GLLOG(glGenFramebuffers(__VA_ARGS__))
# define glGenProgramPipelines(...) GLLOG(glGenProgramPipelines(__VA_ARGS__))
# define glGenProgramPipelines(...) GLLOG(glGenProgramPipelines(__VA_ARGS__))
# define glGenRenderbuffers(...) GLLOG(glGenRenderbuffers(__VA_ARGS__))
# define glGenTextures(...) GLLOG(glGenTextures(__VA_ARGS__))
# define glGenVertexArrays(...) GLLOG(glGenVertexArrays(__VA_ARGS__))
# define glGenerateMipmap(...) GLLOG(glGenerateMipmap(__VA_ARGS__))
# define glGenerateTextureMipmap(...) GLLOG(glGenerateTextureMipmap(__VA_ARGS__))
# define glGetError(...) GLLOG(glGetError(__VA_ARGS__))
# define glGetFloatv(...) GLLOG(glGetFloatv(__VA_ARGS__))
# define glGetFramebufferAttachmentParameteriv(...) GLLOG(glGetFramebufferAttachmentParameteriv(__VA_ARGS__))
# define glGetIntegerv(...) GLLOG(glGetIntegerv(__VA_ARGS__))
# define glGetNamedBufferSubData(...) GLLOG(glGetNamedBufferSubData(__VA_ARGS__))
# define glGetProgramBinary(...) GLLOG(glGetProgramBinary(__VA_ARGS__))
# define glGetProgramBinary(...) GLLOG(glGetProgramBinary(__VA_ARGS__))
# define glGetProgramInfoLog(...) GLLOG(glGetProgramInfoLog(__VA_ARGS__))
# define glGetProgramPipelineInfoLog(...) GLLOG(glGetProgramPipelineInfoLog(__VA_ARGS__))
# define glGetProgramPipelineInfoLog(...) GLLOG(glGetProgramPipelineInfoLog(__VA_ARGS__))
# define glGetProgramPipelineiv(...) GLLOG(glGetProgramPipelineiv(__VA_ARGS__))
# define glGetProgramPipelineiv(...) GLLOG(glGetProgramPipelineiv(__VA_ARGS__))
# define glGetProgramiv(...) GLLOG(glGetProgramiv(__VA_ARGS__))
# define glGetShaderInfoLog(...) GLLOG(glGetShaderInfoLog(__VA_ARGS__))
# define glGetShaderSource(...) GLLOG(glGetShaderSource(__VA_ARGS__))
# define glGetShaderiv(...) GLLOG(glGetShaderiv(__VA_ARGS__))
# define glGetString(...) GLLOG(glGetString(__VA_ARGS__))
# define glGetStringi(...) GLLOG(glGetStringi(__VA_ARGS__))
# define glGetTexImage(...) GLLOG(glGetTexImage(__VA_ARGS__))
# define glGetTexLevelParameterfv(...) GLLOG(glGetTexLevelParameterfv(__VA_ARGS__))
# define glGetTextureImage(...) GLLOG(glGetTextureImage(__VA_ARGS__))
# define glGetUniformLocation(...) GLLOG(glGetUniformLocation(__VA_ARGS__))
# define glInvalidateFramebuffer(...) GLLOG(glInvalidateFramebuffer(__VA_ARGS__))
# define glInvalidateNamedFramebufferData(...) GLLOG(glInvalidateNamedFramebufferData(__VA_ARGS__))
# define glIsEnabled(...) GLLOG(glIsEnabled(__VA_ARGS__))
# define glIsProgram(...) GLLOG(glIsProgram(__VA_ARGS__))
# define glIsShader(...) GLLOG(glIsShader(__VA_ARGS__))
# define glLinkProgram(...) GLLOG(glLinkProgram(__VA_ARGS__))
# define glNamedFramebufferRenderbuffer(...) GLLOG(glNamedFramebufferRenderbuffer(__VA_ARGS__))
# define glNamedFramebufferTexture(...) GLLOG(glNamedFramebufferTexture(__VA_ARGS__))
# define glNamedRenderbufferStorage(...) GLLOG(glNamedRenderbufferStorage(__VA_ARGS__))
# define glPixelStorei(...) GLLOG(glPixelStorei(__VA_ARGS__))
# define glProgramBinary(...) GLLOG(glProgramBinary(__VA_ARGS__))
# define glProgramBinary(...) GLLOG(glProgramBinary(__VA_ARGS__))
# define glProgramParameteri(...) GLLOG(glProgramParameteri(__VA_ARGS__))
# define glProgramParameteri(...) GLLOG(glProgramParameteri(__VA_ARGS__))
# define glProgramUniform1f(...) GLLOG(glProgramUniform1f(__VA_ARGS__))
# define glProgramUniform1f(...) GLLOG(glProgramUniform1f(__VA_ARGS__))
# define glProgramUniform1i(...) GLLOG(glProgramUniform1i(__VA_ARGS__))
# define glProgramUniform1i(...) GLLOG(glProgramUniform1i(__VA_ARGS__))
# define glProgramUniform2fv(...) GLLOG(glProgramUniform2fv(__VA_ARGS__))
# define glProgramUniform2fv(...) GLLOG(glProgramUniform2fv(__VA_ARGS__))
# define glProgramUniform3fv(...) GLLOG(glProgramUniform3fv(__VA_ARGS__))
# define glProgramUniform3fv(...) GLLOG(glProgramUniform3fv(__VA_ARGS__))
# define glProgramUniform4fv(...) GLLOG(glProgramUniform4fv(__VA_ARGS__))
# define glProgramUniform4fv(...) GLLOG(glProgramUniform4fv(__VA_ARGS__))
# define glProgramUniformMatrix4fv(...) GLLOG(glProgramUniformMatrix4fv(__VA_ARGS__))
# define glProgramUniformMatrix4fv(...) GLLOG(glProgramUniformMatrix4fv(__VA_ARGS__))
# define glRasterPos2i(...) GLLOG(glRasterPos2i(__VA_ARGS__))
# define glReadPixels(...) GLLOG(glReadPixels(__VA_ARGS__))
# define glRenderbufferStorage(...) GLLOG(glRenderbufferStorage(__VA_ARGS__))
# define glScissor(...) GLLOG(glScissor(__VA_ARGS__))
# define glShaderSource(...) GLLOG(glShaderSource(__VA_ARGS__))
# define glStencilFunc(...) GLLOG(glStencilFunc(__VA_ARGS__))
# define glStencilOp(...) GLLOG(glStencilOp(__VA_ARGS__))
# define glTexImage2D(...) GLLOG(glTexImage2D(__VA_ARGS__))
# define glTexParameteri(...) GLLOG(glTexParameteri(__VA_ARGS__))
# define glTexStorage2D(...) GLLOG(glTexStorage2D(__VA_ARGS__))
# define glTexSubImage2D(...) GLLOG(glTexSubImage2D(__VA_ARGS__))
# define glTextureParameteri(...) GLLOG(glTextureParameteri(__VA_ARGS__))
# define glTextureStorage2D(...) GLLOG(glTextureStorage2D(__VA_ARGS__))
# define glTextureSubImage2D(...) GLLOG(glTextureSubImage2D(__VA_ARGS__))
# define glUniform1f(...) GLLOG(glUniform1f(__VA_ARGS__))
# define glUniform1fv(...) GLLOG(glUniform1fv(__VA_ARGS__))
# define glUniform1i(...) GLLOG(glUniform1i(__VA_ARGS__))
# define glUniform1iv(...) GLLOG(glUniform1iv(__VA_ARGS__))
# define glUniform2f(...) GLLOG(glUniform2f(__VA_ARGS__))
# define glUniform2fv(...) GLLOG(glUniform2fv(__VA_ARGS__))
# define glUniform2i(...) GLLOG(glUniform2i(__VA_ARGS__))
# define glUniform2iv(...) GLLOG(glUniform2iv(__VA_ARGS__))
# define glUniform3f(...) GLLOG(glUniform3f(__VA_ARGS__))
# define glUniform3fv(...) GLLOG(glUniform3fv(__VA_ARGS__))
# define glUniform3i(...) GLLOG(glUniform3i(__VA_ARGS__))
# define glUniform3iv(...) GLLOG(glUniform3iv(__VA_ARGS__))
# define glUniform4f(...) GLLOG(glUniform4f(__VA_ARGS__))
# define glUniform4fv(...) GLLOG(glUniform4fv(__VA_ARGS__))
# define glUniform4i(...) GLLOG(glUniform4i(__VA_ARGS__))
# define glUniform4iv(...) GLLOG(glUniform4iv(__VA_ARGS__))
# define glUniformMatrix2fv(...) GLLOG(glUniformMatrix2fv(__VA_ARGS__))
# define glUniformMatrix3fv(...) GLLOG(glUniformMatrix3fv(__VA_ARGS__))
# define glUniformMatrix4fv(...) GLLOG(glUniformMatrix4fv(__VA_ARGS__))
# define glUseProgram(...) GLLOG(glUseProgram(__VA_ARGS__))
# define glUseProgramStages(...) GLLOG(glUseProgramStages(__VA_ARGS__))
# define glUseProgramStages(...) GLLOG(glUseProgramStages(__VA_ARGS__))
# define glValidateProgram(...) GLLOG(glValidateProgram(__VA_ARGS__))
# define glValidateProgramPipeline(...) GLLOG(glValidateProgramPipeline(__VA_ARGS__))
# define glValidateProgramPipeline(...) GLLOG(glValidateProgramPipeline(__VA_ARGS__))
# define glVertexAttribIPointer(...) GLLOG(glVertexAttribIPointer(__VA_ARGS__))
# define glVertexAttribPointer(...) GLLOG(glVertexAttribPointer(__VA_ARGS__))
# define glViewport(...) GLLOG(glViewport(__VA_ARGS__))
#endif  // !SUPPRESS_GLLOG
#endif  // DEBUG && SIL_OPENGL_LOG_CALLS

/*-----------------------------------------------------------------------*/

/* Make sure optional constants are defined to _something_ to avoid
 * compilation errors.  (The constants aren't used if the respective
 * features are unavailable.)  We can't detect the presence or absence of
 * functions in this way, so system-specific code is responsible for
 * ensuring that all functions are available (even if no-ops) or defined
 * as macros. */

/* Constants for primitive type. */
#ifndef GL_QUAD_STRIP
# define GL_QUAD_STRIP  GL_INVALID_ENUM
#endif
#ifndef GL_QUADS
# define GL_QUADS  GL_INVALID_ENUM
#endif

/* Constants used by the fixed-function pipeline. */
#ifndef GL_ALIASED_LINE_SIZE_RANGE
# define GL_ALIASED_LINE_SIZE_RANGE  GL_INVALID_ENUM
#endif
#ifndef GL_ALIASED_POINT_SIZE_RANGE
# define GL_ALIASED_POINT_SIZE_RANGE  GL_INVALID_ENUM
#endif
#ifndef GL_ALPHA_TEST
# define GL_ALPHA_TEST  GL_INVALID_ENUM
#endif
#ifndef GL_COLOR_ARRAY
# define GL_COLOR_ARRAY  GL_INVALID_ENUM
#endif
#ifndef GL_FOG
# define GL_FOG  GL_INVALID_ENUM
#endif
#ifndef GL_FOG_MODE
# define GL_FOG_MODE  GL_INVALID_ENUM
#endif
#ifndef GL_MODELVIEW
# define GL_MODELVIEW  GL_INVALID_ENUM
#endif
#ifndef GL_PROJECTION
# define GL_PROJECTION  GL_INVALID_ENUM
#endif
#ifndef GL_TEXTURE_COORD_ARRAY
# define GL_TEXTURE_COORD_ARRAY  GL_INVALID_ENUM
#endif
#ifndef GL_VERTEX_ARRAY
# define GL_VERTEX_ARRAY  GL_INVALID_ENUM
#endif

/* Constants for pixel data types. */
#ifndef GL_ALPHA8
# define GL_ALPHA8  GL_INVALID_ENUM
#endif
#ifndef GL_BGRA
# ifdef GL_BGRA_EXT
#  define GL_BGRA  GL_BGRA_EXT
# else
#  define GL_BGRA  GL_INVALID_ENUM
# endif
#endif
#ifndef GL_LUMINANCE
# define GL_LUMINANCE  GL_INVALID_ENUM
#endif
#ifndef GL_LUMINANCE8
# define GL_LUMINANCE8  GL_INVALID_ENUM
#endif
#ifndef GL_UNSIGNED_SHORT_5_6_5_REV
# ifdef GL_UNSIGNED_SHORT_5_6_5_REV_EXT
#  define GL_UNSIGNED_SHORT_5_6_5_REV  GL_UNSIGNED_SHORT_5_6_5_REV_EXT
# else
#  define GL_UNSIGNED_SHORT_5_6_5_REV  GL_INVALID_ENUM
# endif
#endif
#ifndef GL_UNSIGNED_SHORT_1_5_5_5_REV
# ifdef GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT
#  define GL_UNSIGNED_SHORT_1_5_5_5_REV  GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT
# else
#  define GL_UNSIGNED_SHORT_1_5_5_5_REV  GL_INVALID_ENUM
# endif
#endif
#ifndef GL_UNSIGNED_SHORT_4_4_4_4_REV
# ifdef GL_UNSIGNED_SHORT_4_4_4_4_REV_EXT
#  define GL_UNSIGNED_SHORT_4_4_4_4_REV  GL_UNSIGNED_SHORT_4_4_4_4_REV_EXT
# else
#  define GL_UNSIGNED_SHORT_4_4_4_4_REV  GL_INVALID_ENUM
# endif
#endif

/* Constants for texture formats.  Some of these are missing from system
 * headers even when they should be present, so insert the real values. */
#ifndef GL_COMPRESSED_RGB_S3TC_DXT1_EXT
# define GL_COMPRESSED_RGB_S3TC_DXT1_EXT  0x83F0
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
# define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT  0x83F2
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
# define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT  0x83F3
#endif
#ifndef GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG
# define GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG  0x8C01
#endif
#ifndef GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG
# define GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG  0x8C03
#endif
#ifndef GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG
# define GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG  0x8C00
#endif
#ifndef GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG
# define GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG  0x8C02
#endif

/* Constants related to vertex/index buffers. */
#ifndef GL_WRITE_ONLY
# if defined(GL_WRITE_ONLY_ARB)
#  define GL_WRITE_ONLY  GL_WRITE_ONLY_ARB
# elif defined(GL_WRITE_ONLY_OES)
#  define GL_WRITE_ONLY  GL_WRITE_ONLY_OES
# else
#  define GL_WRITE_ONLY  GL_INVALID_ENUM
# endif
#endif

/* Constants related to shaders. */
#ifndef GL_ACTIVE_PROGRAM
# ifdef GL_ACTIVE_PROGRAM_EXT
#  define GL_ACTIVE_PROGRAM  GL_ACTIVE_PROGRAM_EXT
# else
#  define GL_ACTIVE_PROGRAM  GL_INVALID_ENUM
# endif
#endif
#ifndef GL_ALL_SHADER_BITS
# ifdef GL_ALL_SHADER_BITS_EXT
#  define GL_ALL_SHADER_BITS  GL_ALL_SHADER_BITS_EXT
# else
#  define GL_ALL_SHADER_BITS  GL_INVALID_ENUM
# endif
#endif
#ifndef GL_FRAGMENT_SHADER_BIT
# ifdef GL_FRAGMENT_SHADER_BIT_EXT
#  define GL_FRAGMENT_SHADER_BIT  GL_FRAGMENT_SHADER_BIT_EXT
# else
#  define GL_FRAGMENT_SHADER_BIT  GL_INVALID_ENUM
# endif
#endif
#ifndef GL_PROGRAM_PIPELINE_BINDING
# ifdef GL_PROGRAM_PIPELINE_BINDING_EXT
#  define GL_PROGRAM_PIPELINE_BINDING  GL_PROGRAM_PIPELINE_BINDING_EXT
# else
#  define GL_PROGRAM_PIPELINE_BINDING  GL_INVALID_ENUM
# endif
#endif
#ifndef GL_PROGRAM_POINT_SIZE
# define GL_PROGRAM_POINT_SIZE  GL_INVALID_ENUM
#endif
#ifndef GL_PROGRAM_SEPARABLE
# ifdef GL_PROGRAM_SEPARABLE_EXT
#  define GL_PROGRAM_SEPARABLE  GL_PROGRAM_SEPARABLE_EXT
# else
#  define GL_PROGRAM_SEPARABLE  GL_INVALID_ENUM
# endif
#endif
#ifndef GL_VERTEX_SHADER_BIT
# ifdef GL_VERTEX_SHADER_BIT_EXT
#  define GL_VERTEX_SHADER_BIT  GL_VERTEX_SHADER_BIT_EXT
# else
#  define GL_VERTEX_SHADER_BIT  GL_INVALID_ENUM
# endif
#endif

/*-----------------------------------------------------------------------*/

/* Disable glGetError() if requested by the system configuration. */

#ifdef SIL_OPENGL_DISABLE_GETERROR
# undef glGetError
# define glGetError() GL_NO_ERROR
#endif

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_OPENGL_GL_HEADERS_H
