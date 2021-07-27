/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/opengl/shader-common.h: Header for shared OpenGL shader
 * functions.
 */

#ifndef SIL_SRC_SYSDEP_OPENGL_SHADER_COMMON_H
#define SIL_SRC_SYSDEP_OPENGL_SHADER_COMMON_H

/*************************************************************************/
/*************************************************************************/

/* Constants for uniform data types. */
typedef enum UniformType UniformType;
#define UNIFORM_TYPE_UNKNOWN  0  // Hasn't been set yet.
enum UniformType {
    UNIFORM_TYPE_INT = 1,
    UNIFORM_TYPE_FLOAT,
    UNIFORM_TYPE_VEC2,
    UNIFORM_TYPE_VEC3,
    UNIFORM_TYPE_VEC4,
    UNIFORM_TYPE_MAT4,
};

/* Constants for the "type" parameter to get_shader_string(). */
typedef enum ShaderStringType ShaderStringType;
enum ShaderStringType {
    GET_SHADER_SOURCE,  // The shader's source code.
    GET_SHADER_INFO,    // The shader's information log.
    GET_PROGRAM_INFO,   // The shader program's information log.
};

/*-----------------------------------------------------------------------*/

/**
 * opengl_compile_shader:  Compile a shader from a source code string.
 *
 * On successful return, glGetError() is guaranteed to return GL_NO_ERROR.
 *
 * [Parameters]
 *     shader: Shader object.
 *     source: Shader source code to compile.
 *     length: Length of source, in bytes.
 *     type: Shader type (SHADER_TYPE_*).
 *     is_separate: True if the shader is being compiled into its own
 *         (separable) shader program; false otherwise.
 * [Return value]
 *     True on success, false on error.
 */
extern int opengl_compile_shader(GLuint shader, const char *source, int length,
                                 ShaderType type, int is_separate);

/**
 * opengl_link_shader:  Link one or more shader objects into a shader program.
 *
 * On successful return, glGetError() is guaranteed to return GL_NO_ERROR.
 *
 * [Parameters]
 *     program: Shader program object.
 *     ...: Shader objects (of type GLuint), terminated by a value of 0.
 * [Return value]
 *     True on success, false on error.
 */
extern int opengl_link_shader(GLuint program, ...);

/*-----------------------------------------------------------------------*/

/* These functions are only compiled in a debug build. */

#ifdef DEBUG

/**
 * opengl_uniform_type_name:  Return a string corresponding to the given
 * uniform data type code.  Only available when building in debug mode.
 *
 * [Parameters]
 *     type: Uniform data type code.
 * [Return value]
 *     Corresponding type name.
 */
extern const char *opengl_uniform_type_name(UniformType type);

/**
 * opengl_get_shader_string:  Retrieve string data for a shader or program
 * object.  Only available when building in debug mode.
 *
 * [Parameters]
 *     type: Type of data to retrieve (GET_SHADER_SOURCE, etc.).
 *     object: Shader or program object.
 * [Return value]
 *     String data in a mem_alloc()ed buffer, or NULL on error.
 */
extern char *opengl_get_shader_string(ShaderStringType type, GLuint object);

#endif  // DEBUG

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_OPENGL_SHADER_COMMON_H
