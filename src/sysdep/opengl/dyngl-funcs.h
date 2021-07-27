/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/opengl/dyngl-funcs.h: Function table for dynamic loading of
 * OpenGL.
 */

/*
 * This file can be used to declare or define various sets of data for the
 * OpenGL functions used by the program.  The caller should define the
 * following preprocessor macro:
 *     DYNGL_FUNC(rettype,name,altname,params,args,required,category)
 * which will be called in the following manner:
 *     DYNGL_FUNC(void, glFunc, glFuncEXT, (GLenum a), (a), required, category)
 * for each OpenGL function to be processed.  "required" is a Boolean
 * expression evaluating to true if the function is required, 0 if it is
 * optional; for many functions, this is defined with reference to
 * variables "major" and "minor", which are expected to hold the major and
 * minor version of the OpenGL library.  (These variables do not need to be
 * defined if the "required" parameter is ignored.)  "category" is a token
 * which can be pasted onto an identifier to e.g. create a variable name
 * for use in checking the presence of that category of functions.  The
 * "altname" parameter gives an alternate name for the function (which may
 * be the same as its base name) to handle systems on which the base name
 * is not available.
 *
 * See dyngl.c for details of usage.
 *
 * Note that we intentionally do not protect this file against double
 * inclusion, since it may (and typically will) be included multiple times
 * with different definitions of DYNGL_FUNC.
 */

/*************************************************************************/
/*************************************************************************/

/* Shortcut macros to simplify the function list: */

/* Needed to dereference CATEGORY for token pasting. */
#define FUNC(a,b,c,d,e,f,g)  DYNGL_FUNC(a,b,c,d,e,f,g)

#define FUNC0(rettype,name) \
    FUNC(rettype, name, name, (void), (), REQUIRED_FLAG, CATEGORY)
#define FUNC1(rettype,name,type1) \
    FUNC(rettype, name, name, (type1 a), (a), REQUIRED_FLAG, CATEGORY)
#define FUNC2(rettype,name,type1,type2) \
    FUNC(rettype, name, name, (type1 a, type2 b), (a,b), REQUIRED_FLAG, \
         CATEGORY)
#define FUNC3(rettype,name,type1,type2,type3) \
    FUNC(rettype, name, name, (type1 a, type2 b, type3 c), (a,b,c), \
         REQUIRED_FLAG, CATEGORY)
#define FUNC4(rettype,name,type1,type2,type3,type4) \
    FUNC(rettype, name, name, (type1 a, type2 b, type3 c, type4 d), \
         (a,b,c,d), REQUIRED_FLAG, CATEGORY)
#define FUNC5(rettype,name,type1,type2,type3,type4,type5) \
    FUNC(rettype, name, name, (type1 a, type2 b, type3 c, type4 d, type5 e), \
         (a,b,c,d,e), REQUIRED_FLAG, CATEGORY)
#define FUNC6(rettype,name,type1,type2,type3,type4,type5,type6) \
    FUNC(rettype, name, name, (type1 a, type2 b, type3 c, type4 d, type5 e, \
                         type6 f), (a,b,c,d,e,f), REQUIRED_FLAG, CATEGORY)
#define FUNC7(rettype,name,type1,type2,type3,type4,type5,type6,type7) \
    FUNC(rettype, name, name, (type1 a, type2 b, type3 c, type4 d, type5 e, \
                               type6 f, type7 g), (a,b,c,d,e,f,g), \
         REQUIRED_FLAG, CATEGORY)
#define FUNC8(rettype,name,type1,type2,type3,type4,type5,type6,type7,type8) \
    FUNC(rettype, name, name, (type1 a, type2 b, type3 c, type4 d, type5 e, \
                               type6 f, type7 g, type8 h), (a,b,c,d,e,f,g,h), \
         REQUIRED_FLAG, CATEGORY)
#define FUNC9(rettype,name,type1,type2,type3,type4,type5,type6,type7,type8,type9) \
    FUNC(rettype, name, name, (type1 a, type2 b, type3 c, type4 d, type5 e, \
                               type6 f, type7 g, type8 h, type9 i), \
         (a,b,c,d,e,f,g,h,i), REQUIRED_FLAG, CATEGORY)

#define FUNC1_ARB(rettype,name,type1) \
    FUNC(rettype, name, name##ARB, (type1 a), (a), REQUIRED_FLAG, CATEGORY)
#define FUNC2_ARB(rettype,name,type1,type2) \
    FUNC(rettype, name, name##ARB, (type1 a, type2 b), (a,b), REQUIRED_FLAG, \
         CATEGORY)
#define FUNC3_ARB(rettype,name,type1,type2,type3) \
    FUNC(rettype, name, name##ARB, (type1 a, type2 b, type3 c), (a,b,c), \
         REQUIRED_FLAG, CATEGORY)
#define FUNC4_ARB(rettype,name,type1,type2,type3,type4) \
    FUNC(rettype, name, name##ARB, (type1 a, type2 b, type3 c, type4 d), \
         (a,b,c,d), REQUIRED_FLAG, CATEGORY)
#define FUNC5_ARB(rettype,name,type1,type2,type3,type4,type5) \
    FUNC(rettype, name, name##ARB, (type1 a, type2 b, type3 c, type4 d, \
                                    type5 e), (a,b,c,d,e), REQUIRED_FLAG, \
         CATEGORY)
#define FUNC6_ARB(rettype,name,type1,type2,type3,type4,type5,type6) \
    FUNC(rettype, name, name##ARB, (type1 a, type2 b, type3 c, type4 d, \
                                    type5 e, type6 f), (a,b,c,d,e,f), \
         REQUIRED_FLAG, CATEGORY)
#define FUNC8_ARB(rettype,name,type1,type2,type3,type4,type5,type6,type7,type8) \
    FUNC(rettype, name, name##ARB, (type1 a, type2 b, type3 c, type4 d, \
                                    type5 e, type6 f, type7 g, type8 h), \
         (a,b,c,d,e,f,g,h), REQUIRED_FLAG, CATEGORY)
#define FUNC9_ARB(rettype,name,type1,type2,type3,type4,type5,type6,type7,type8,type9) \
    FUNC(rettype, name, name##ARB, (type1 a, type2 b, type3 c, type4 d, \
                                    type5 e, type6 f, type7 g, type8 h, \
                                    type9 i), (a,b,c,d,e,f,g,h,i), \
         REQUIRED_FLAG, CATEGORY)

#define FUNC1_EXT(rettype,name,type1) \
    FUNC(rettype, name, name##EXT, (type1 a), (a), REQUIRED_FLAG, CATEGORY)
#define FUNC2_EXT(rettype,name,type1,type2) \
    FUNC(rettype, name, name##EXT, (type1 a, type2 b), (a,b), REQUIRED_FLAG, \
         CATEGORY)
#define FUNC3_EXT(rettype,name,type1,type2,type3) \
    FUNC(rettype, name, name##EXT, (type1 a, type2 b, type3 c), (a,b,c), \
         REQUIRED_FLAG, CATEGORY)
#define FUNC4_EXT(rettype,name,type1,type2,type3,type4) \
    FUNC(rettype, name, name##EXT, (type1 a, type2 b, type3 c, type4 d), \
         (a,b,c,d), REQUIRED_FLAG, CATEGORY)
#define FUNC5_EXT(rettype,name,type1,type2,type3,type4,type5) \
    FUNC(rettype, name, name##EXT, (type1 a, type2 b, type3 c, type4 d, \
                                    type5 e), (a,b,c,d,e), REQUIRED_FLAG, \
         CATEGORY)
#define FUNC6_EXT(rettype,name,type1,type2,type3,type4,type5,type6) \
    FUNC(rettype, name, name##EXT, (type1 a, type2 b, type3 c, type4 d, \
                                    type5 e, type6 f), (a,b,c,d,e,f), \
         REQUIRED_FLAG, CATEGORY)

#define FUNC2_KHR(rettype,name,type1,type2) \
    FUNC(rettype, name, name##KHR, (type1 a, type2 b), (a,b), REQUIRED_FLAG, \
         CATEGORY)
#define FUNC6_KHR(rettype,name,type1,type2,type3,type4,type5,type6) \
    FUNC(rettype, name, name##KHR, (type1 a, type2 b, type3 c, type4 d, \
                                    type5 e, type6 f), (a,b,c,d,e,f), \
         REQUIRED_FLAG, CATEGORY)

#define FUNC1_OES(rettype,name,type1) \
    FUNC(rettype, name, name##OES, (type1 a), (a), REQUIRED_FLAG, CATEGORY)
#define FUNC2_OES(rettype,name,type1,type2) \
    FUNC(rettype, name, name##OES, (type1 a, type2 b), (a,b), REQUIRED_FLAG, \
         CATEGORY)
#define FUNC4_OES(rettype,name,type1,type2,type3,type4) \
    FUNC(rettype, name, name##OES, (type1 a, type2 b, type3 c, type4 d), \
         (a,b,c,d), REQUIRED_FLAG, CATEGORY)
#define FUNC5_OES(rettype,name,type1,type2,type3,type4,type5) \
    FUNC(rettype, name, name##OES, (type1 a, type2 b, type3 c, type4 d, \
                                    type5 e), (a,b,c,d,e), REQUIRED_FLAG, \
         CATEGORY)

/*-----------------------------------------------------------------------*/

/* Base OpenGL/GLES */
#define REQUIRED_FLAG 1
#define CATEGORY /*nothing*/
FUNC2(void, glBindTexture, GLenum, GLuint)
FUNC2(void, glBlendFunc, GLenum, GLenum)
FUNC1(void, glClear, GLbitfield)
FUNC4(void, glClearColor, GLclampf, GLclampf, GLclampf, GLclampf)
FUNC1(void, glClearStencil, GLint)
FUNC4(void, glColorMask, GLboolean, GLboolean, GLboolean, GLboolean)
FUNC8(void, glCopyTexImage2D, GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLsizei, GLint)
FUNC8(void, glCopyTexSubImage2D, GLenum, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei)
FUNC1(void, glCullFace, GLenum)
FUNC2(void, glDeleteTextures, GLsizei, const GLuint *)
FUNC1(void, glDepthFunc, GLenum)
FUNC1(void, glDepthMask, GLboolean)
FUNC1(void, glDisable, GLenum)
FUNC3(void, glDrawArrays, GLenum, GLint, GLsizei)
FUNC4(void, glDrawElements, GLenum, GLsizei, GLenum, const GLvoid *)
FUNC1(void, glEnable, GLenum)
FUNC0(void, glFinish)
FUNC0(void, glFlush)
FUNC1(void, glFrontFace, GLenum)
FUNC2(void, glGenTextures, GLsizei, GLuint *)
#ifndef SIL_OPENGL_DISABLE_GETERROR
FUNC0(GLenum, glGetError)
#endif
FUNC2(void, glGetFloatv, GLenum, GLfloat *)
FUNC2(void, glGetIntegerv, GLenum, GLint *)
FUNC1(const GLubyte *, glGetString, GLenum)
FUNC1(GLboolean, glIsEnabled, GLenum)
FUNC2(void, glPixelStorei, GLenum, GLint)
FUNC7(void, glReadPixels, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, GLvoid *)
FUNC4(void, glScissor, GLint, GLint, GLsizei, GLsizei)
FUNC3(void, glStencilFunc, GLenum, GLint, GLuint)
FUNC3(void, glStencilOp, GLenum, GLenum, GLenum)
FUNC9(void, glTexImage2D, GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const GLvoid *)
FUNC3(void, glTexParameteri, GLenum, GLenum, GLint)
FUNC9(void, glTexSubImage2D, GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const GLvoid *)
FUNC4(void, glViewport, GLint, GLint, GLsizei, GLsizei)

/* Base OpenGL (not in GLES) */
#ifndef SIL_OPENGL_ES
FUNC1(void, glClearDepth, GLclampd)
FUNC2(void, glDepthRange, GLclampd, GLclampd)
FUNC5(void, glGetTexImage, GLenum, GLint, GLenum, GLenum, GLvoid *)
FUNC4(void, glGetTexLevelParameterfv, GLenum, GLint, GLenum, GLfloat *)
FUNC2(void, glRasterPos2i, GLint, GLint)
#endif

/* Base OpenGLES */
#ifdef SIL_OPENGL_ES
FUNC1(void, glClearDepthf, GLclampf)
FUNC2(void, glDepthRangef, GLclampf, GLclampf)
#endif

/* OpenGL 1.3 (formerly ARB_texture_compression) */
FUNC8_ARB(void, glCompressedTexImage2D, GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const GLvoid *)
FUNC9_ARB(void, glCompressedTexSubImage2D, GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei, const GLvoid *)

/* OpenGL 1.4 (formerly GL_EXT_blend_{color,subtract}) */
FUNC4(void, glBlendColor, GLclampf, GLclampf, GLclampf, GLclampf)
FUNC1(void, glBlendEquation, GLenum)

/* OpenGL 1.5 (formerly ARB_vertex_buffer_object) */
FUNC2_ARB(void, glBindBuffer, GLenum, GLuint)
FUNC4_ARB(void, glBufferData, GLenum, GLsizeiptr, const GLvoid *, GLenum)
FUNC4_ARB(void, glBufferSubData, GLenum, GLintptr, GLsizeiptr, const GLvoid *)
FUNC2_ARB(void, glDeleteBuffers, GLsizei, const GLuint *)
FUNC2_ARB(void, glGenBuffers, GLsizei, GLuint *)

/* OpenGL 2.0 (note that there were earlier ARBs for shaders, but they had
 * a different calling format so we don't allow fallback) */
FUNC1(void, glActiveTexture, GLenum)  // Actually OpenGL 1.3, but we only use it with shaders.
FUNC2(void, glAttachShader, GLuint, GLuint)
FUNC3(void, glBindAttribLocation, GLuint, GLuint, const GLchar *)
FUNC4(void, glBlendFuncSeparate, GLenum, GLenum, GLenum, GLenum)
FUNC1(void, glCompileShader, GLuint)
FUNC0(GLuint, glCreateProgram)
FUNC1(GLuint, glCreateShader, GLenum)
FUNC1(void, glDeleteProgram, GLuint)
FUNC1(void, glDeleteShader, GLuint)
FUNC2(void, glDetachShader, GLuint, GLuint)
FUNC1(void, glDisableVertexAttribArray, GLuint)
FUNC1(void, glEnableVertexAttribArray, GLuint)
FUNC3(void, glGetProgramiv, GLuint, GLenum, GLint *)
FUNC4(void, glGetProgramInfoLog, GLuint, GLsizei, GLsizei *, GLchar *)
FUNC3(void, glGetShaderiv, GLuint, GLenum, GLint *)
FUNC4(void, glGetShaderInfoLog, GLuint, GLsizei, GLsizei *, GLchar *)
FUNC4(void, glGetShaderSource, GLuint, GLsizei, GLsizei *, GLchar *)
FUNC2(GLint, glGetUniformLocation, GLuint, const GLchar *)
FUNC1(GLboolean, glIsProgram, GLuint)
FUNC1(GLboolean, glIsShader, GLuint)
FUNC1(void, glLinkProgram, GLuint)
FUNC4(void, glShaderSource, GLuint, GLsizei, const GLchar * const *, const GLint *)
FUNC2(void, glUniform1f, GLint, GLfloat)
FUNC3(void, glUniform1fv, GLint, GLsizei, const GLfloat *)
FUNC2(void, glUniform1i, GLint, GLint)
FUNC3(void, glUniform1iv, GLint, GLsizei, const GLint *)
FUNC3(void, glUniform2f, GLint, GLfloat, GLfloat)
FUNC3(void, glUniform2fv, GLint, GLsizei, const GLfloat *)
FUNC3(void, glUniform2i, GLint, GLint, GLint)
FUNC3(void, glUniform2iv, GLint, GLsizei, const GLint *)
FUNC4(void, glUniform3f, GLint, GLfloat, GLfloat, GLfloat)
FUNC3(void, glUniform3fv, GLint, GLsizei, const GLfloat *)
FUNC4(void, glUniform3i, GLint, GLint, GLint, GLint)
FUNC3(void, glUniform3iv, GLint, GLsizei, const GLint *)
FUNC5(void, glUniform4f, GLint, GLfloat, GLfloat, GLfloat, GLfloat)
FUNC3(void, glUniform4fv, GLint, GLsizei, const GLfloat *)
FUNC5(void, glUniform4i, GLint, GLint, GLint, GLint, GLint)
FUNC3(void, glUniform4iv, GLint, GLsizei, const GLint *)
FUNC4(void, glUniformMatrix2fv, GLint, GLsizei, GLboolean, const GLfloat *)
FUNC4(void, glUniformMatrix3fv, GLint, GLsizei, GLboolean, const GLfloat *)
FUNC4(void, glUniformMatrix4fv, GLint, GLsizei, GLboolean, const GLfloat *)
FUNC1(void, glUseProgram, GLuint)
FUNC1(void, glValidateProgram, GLuint)
FUNC6(void, glVertexAttribPointer, GLuint, GLint, GLenum, GLboolean, GLsizei, const GLvoid *)

/* OpenGL ES 2.0 (but not 3.0) */
#ifdef SIL_OPENGL_ES
#undef REQUIRED_FLAG
#define REQUIRED_FLAG (major == 2)
FUNC3(void, glDiscardFramebufferEXT, GLenum, GLsizei, const GLenum *)
#endif

/* OpenGL 3.0 */
#undef REQUIRED_FLAG
#define REQUIRED_FLAG (major >= 3)
FUNC2(const GLubyte *, glGetStringi, GLenum, GLuint)
/* These three are also available as ...OES() functions in iOS's
 * implementation of OpenGL ES 2.0. */
FUNC1_OES(void, glBindVertexArray, GLuint)
FUNC2_OES(void, glDeleteVertexArrays, GLsizei, const GLuint *)
FUNC2_OES(void, glGenVertexArrays, GLsizei, GLuint *)

/* OpenGL 3.0, not in GLES */
#ifndef SIL_OPENGL_ES
FUNC3(void, glBindFragDataLocation, GLuint, GLuint, const GLchar *)
#endif

/* OpenGL 4.3 / OpenGL ES 3.0 */
#undef REQUIRED_FLAG
#ifdef SIL_OPENGL_ES
# define REQUIRED_FLAG (major >= 3)
#else
# define REQUIRED_FLAG (major >= 5 || (major == 4 && minor >= 3))
#endif
FUNC3(void, glInvalidateFramebuffer, GLenum, GLsizei, const GLenum *)

/*----------------------------------*/

/* Functions in optional categories: */

/* Framebuffers (OpenGL 3.0 or EXT_framebuffer_object, OpenGL ES 2.0) */
#undef REQUIRED_FLAG
#ifdef SIL_OPENGL_ES
# define REQUIRED_FLAG (major >= 2)
#else
# define REQUIRED_FLAG (major >= 3)
#endif
#undef CATEGORY
#define CATEGORY framebuffers
FUNC2_EXT(void, glBindFramebuffer, GLenum, GLuint)
FUNC2_EXT(void, glBindRenderbuffer, GLenum, GLuint)
FUNC1_EXT(GLenum, glCheckFramebufferStatus, GLenum)
FUNC2_EXT(void, glDeleteFramebuffers, GLsizei, const GLuint *)
FUNC2_EXT(void, glDeleteRenderbuffers, GLsizei, const GLuint *)
FUNC4_EXT(void, glFramebufferRenderbuffer, GLenum, GLenum, GLenum, GLuint)
FUNC5_EXT(void, glFramebufferTexture2D, GLenum, GLenum, GLenum, GLuint, GLint)
FUNC2_EXT(void, glGenFramebuffers, GLsizei, GLuint *)
FUNC2_EXT(void, glGenRenderbuffers, GLsizei, GLuint *)
/* Despite being totally unrelated to framebuffers, glGenerateMipmap() is
 * mysteriously part of the EXT_framebuffer_object extension. */
FUNC1_EXT(void, glGenerateMipmap, GLenum)
FUNC4_EXT(void, glGetFramebufferAttachmentParameteriv, GLenum, GLenum, GLenum, GLint *)
FUNC4_EXT(void, glRenderbufferStorage, GLenum, GLenum, GLsizei, GLsizei)

/* Integer vertex attributes (OpenGL/ES 3.0 or EXT_gpu_shader4) */
#undef REQUIRED_FLAG
#define REQUIRED_FLAG (major >= 3)
#undef CATEGORY
#define CATEGORY vertex_attrib_int
FUNC5(void, glVertexAttribIPointer, GLuint, GLint, GLenum, GLsizei, const GLvoid *)

/* Shader program binary loading/retrieval (OpenGL 4.1 or
 * ARB_get_program_binary, OpenGL ES 3.0 or OES_get_program_binary) */
#undef REQUIRED_FLAG
#ifdef SIL_OPENGL_ES
# define REQUIRED_FLAG (major >= 3)
#else
# define REQUIRED_FLAG (major >= 5 || (major == 4 && minor >= 1))
#endif
#undef CATEGORY
#define CATEGORY shader_binaries
#ifdef SIL_OPENGL_ES
FUNC5_OES(void, glGetProgramBinary, GLuint, GLsizei, GLsizei *, GLenum *, void *)
FUNC4_OES(void, glProgramBinary, GLuint, GLenum, const void *, GLsizei)
#else
FUNC5_ARB(void, glGetProgramBinary, GLuint, GLsizei, GLsizei *, GLenum *, void *)
FUNC4_ARB(void, glProgramBinary, GLuint, GLenum, const void *, GLsizei)
#endif

/* Per-stage shader programs (OpenGL 4.1 or ARB_separate_shader_objects,
 * OpenGL ES 3.1 or the GLES version of EXT_separate_shader_objects) */
#undef REQUIRED_FLAG
#ifdef SIL_OPENGL_ES
# define REQUIRED_FLAG (major >= 4 || (major == 3 && minor >= 1))
#else
# define REQUIRED_FLAG (major >= 5 || (major == 4 && minor >= 1))
#endif
#undef CATEGORY
#define CATEGORY separate_shaders
#ifdef SIL_OPENGL_ES
FUNC1_EXT(void, glBindProgramPipeline, GLuint)
FUNC2_EXT(void, glDeleteProgramPipelines, GLsizei, const GLuint *)
FUNC3_EXT(void, glGetProgramPipelineiv, GLuint, GLenum, GLint *)
FUNC4_EXT(void, glGetProgramPipelineInfoLog, GLuint, GLsizei, GLsizei *, GLchar *)
FUNC2_EXT(void, glGenProgramPipelines, GLsizei, GLuint *)
FUNC3_EXT(void, glProgramParameteri, GLuint, GLenum, GLint)
FUNC3_EXT(void, glProgramUniform1f, GLuint, GLint, GLfloat)
FUNC3_EXT(void, glProgramUniform1i, GLuint, GLint, GLint)
FUNC4_EXT(void, glProgramUniform2fv, GLuint, GLint, GLsizei, const GLfloat *)
FUNC4_EXT(void, glProgramUniform3fv, GLuint, GLint, GLsizei, const GLfloat *)
FUNC4_EXT(void, glProgramUniform4fv, GLuint, GLint, GLsizei, const GLfloat *)
FUNC5_EXT(void, glProgramUniformMatrix4fv, GLuint, GLint, GLsizei, GLboolean, const GLfloat *)
FUNC3_EXT(void, glUseProgramStages, GLuint, GLbitfield, GLuint)
FUNC1_EXT(void, glValidateProgramPipeline, GLuint)
#else
FUNC1_ARB(void, glBindProgramPipeline, GLuint)
FUNC2_ARB(void, glDeleteProgramPipelines, GLsizei, const GLuint *)
FUNC2_ARB(void, glGenProgramPipelines, GLsizei, GLuint *)
FUNC3_ARB(void, glGetProgramPipelineiv, GLuint, GLenum, GLint *)
FUNC4_ARB(void, glGetProgramPipelineInfoLog, GLuint, GLsizei, GLsizei *, GLchar *)
FUNC3_ARB(void, glProgramParameteri, GLuint, GLenum, GLint)
FUNC3_ARB(void, glProgramUniform1f, GLuint, GLint, GLfloat)
FUNC3_ARB(void, glProgramUniform1i, GLuint, GLint, GLint)
FUNC4_ARB(void, glProgramUniform2fv, GLuint, GLint, GLsizei, const GLfloat *)
FUNC4_ARB(void, glProgramUniform3fv, GLuint, GLint, GLsizei, const GLfloat *)
FUNC4_ARB(void, glProgramUniform4fv, GLuint, GLint, GLsizei, const GLfloat *)
FUNC5_ARB(void, glProgramUniformMatrix4fv, GLuint, GLint, GLsizei, GLboolean, const GLfloat *)
FUNC3_ARB(void, glUseProgramStages, GLuint, GLbitfield, GLuint)
FUNC1_ARB(void, glValidateProgramPipeline, GLuint)
#endif

/* Texture storage allocation and immutable texture objects (OpenGL 4.2 or
 * ARB_texture_storage, OpenGL ES 3.0) */
#undef REQUIRED_FLAG
#ifdef SIL_OPENGL_ES
# define REQUIRED_FLAG (major >= 3)
#else
# define REQUIRED_FLAG (major >= 5 || (major == 4 && minor >= 2))
#endif
#undef CATEGORY
#define CATEGORY texture_storage
FUNC5(void, glTexStorage2D, GLenum, GLsizei, GLenum, GLsizei, GLsizei)

/* Debug output (OpenGL 4.3 or ARB_debug_output, OpenGL ES KHR_debug) */
#undef REQUIRED_FLAG
#ifdef SIL_OPENGL_ES
# define REQUIRED_FLAG 0
#else
# define REQUIRED_FLAG (major >= 5 || (major == 4 && minor >= 3))
#endif
#undef CATEGORY
#define CATEGORY debug_output
#ifdef SIL_OPENGL_ES
FUNC2_KHR(void, glDebugMessageCallback, GLDEBUGPROCKHR, const void *)
FUNC6_KHR(void, glDebugMessageControl, GLenum, GLenum, GLenum, GLsizei, const GLuint *, GLboolean)
FUNC6_KHR(void, glDebugMessageInsert, GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar *)
#else
FUNC2_ARB(void, glDebugMessageCallback, GLDEBUGPROC, const void *)
FUNC6_ARB(void, glDebugMessageControl, GLenum, GLenum, GLenum, GLsizei, const GLuint *, GLboolean)
FUNC6_ARB(void, glDebugMessageInsert, GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar *)
#endif

/* Stateless functions (OpenGL 4.5 or ARB_direct_state_access). */
#undef REQUIRED_FLAG
#ifdef SIL_OPENGL_ES
# define REQUIRED_FLAG 0
#else
# define REQUIRED_FLAG (major >= 5 || (major == 4 && minor >= 5))
#endif
#undef CATEGORY
#define CATEGORY dsa
FUNC2_ARB(void, glBindTextureUnit, GLuint, GLuint)
FUNC2_ARB(GLenum, glCheckNamedFramebufferStatus, GLuint, GLenum)
FUNC9_ARB(void, glCompressedTextureSubImage2D, GLuint, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei, const void *)
FUNC8_ARB(void, glCopyTextureSubImage2D, GLuint, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei)
FUNC2_ARB(void, glCreateFramebuffers, GLsizei, GLuint *)
FUNC2_ARB(void, glCreateProgramPipelines, GLsizei, GLuint *)
FUNC2_ARB(void, glCreateRenderbuffers, GLsizei, GLuint *)
FUNC3_ARB(void, glCreateTextures, GLenum, GLsizei, GLuint *)
FUNC1_ARB(void, glGenerateTextureMipmap, GLuint)
FUNC4_ARB(void, glGetNamedBufferSubData, GLuint, GLintptr, GLsizeiptr, void *)
FUNC6_ARB(void, glGetTextureImage, GLuint, GLint, GLenum, GLenum, GLsizei, void *)
FUNC3_ARB(void, glInvalidateNamedFramebufferData, GLuint, GLsizei, const GLenum *)
FUNC4_ARB(void, glNamedFramebufferRenderbuffer, GLuint, GLenum, GLenum, GLuint)
FUNC4_ARB(void, glNamedFramebufferTexture, GLuint, GLenum, GLuint, GLint)
FUNC4_ARB(void, glNamedRenderbufferStorage, GLuint, GLenum, GLsizei, GLsizei)
FUNC3_ARB(void, glTextureParameteri, GLuint, GLenum, GLint)
FUNC5_ARB(void, glTextureStorage2D, GLuint, GLsizei, GLenum, GLsizei, GLsizei)
FUNC9_ARB(void, glTextureSubImage2D, GLuint, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *)

/*-----------------------------------------------------------------------*/

#undef FUNC
#undef FUNC0
#undef FUNC1
#undef FUNC2
#undef FUNC3
#undef FUNC4
#undef FUNC5
#undef FUNC6
#undef FUNC7
#undef FUNC8
#undef FUNC9
#undef FUNC1_ARB
#undef FUNC2_ARB
#undef FUNC3_ARB
#undef FUNC4_ARB
#undef FUNC5_ARB
#undef FUNC6_ARB
#undef FUNC8_ARB
#undef FUNC9_ARB
#undef FUNC1_EXT
#undef FUNC2_EXT
#undef FUNC3_EXT
#undef FUNC4_EXT
#undef FUNC5_EXT
#undef FUNC6_EXT
#undef FUNC2_KHR
#undef FUNC6_KHR
#undef FUNC1_OES
#undef FUNC2_OES
#undef FUNC4_OES
#undef FUNC5_OES
#undef REQUIRED_FLAG
#undef CATEGORY

/*************************************************************************/
/*************************************************************************/
