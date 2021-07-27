/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/opengl/shader-common.c: Shared OpenGL shader functions.
 */

#include "src/base.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/opengl/internal.h"
#include "src/sysdep/opengl/opengl.h"
#include "src/sysdep/opengl/shader-common.h"

/*************************************************************************/
/*************************** Library routines ****************************/
/*************************************************************************/

int opengl_compile_shader(GLuint shader, const char *source, int length,
                          ShaderType type, int is_separate)
{
    PRECOND(shader != 0, return 0);
    PRECOND(source != NULL, return 0);
    PRECOND(length > 0, return 0);

#ifdef DEBUG
    const char *type_str = NULL;
    switch (type) {
        case SHADER_TYPE_VERTEX:   type_str = "vertex";   break;
        case SHADER_TYPE_FRAGMENT: type_str = "fragment"; break;
    }
#endif

    int inout_style;  // in/out style (GLSL 1.30+) or attribute/varying style?
    int use_gl_PerVertex; // Use gl_PerVertex out block? (GLSL 1.50+, ES 3.20+)
    const char * const *version_header;
    int version_header_lines;
#ifdef SIL_OPENGL_ES
    /* In order to use integer vertex attributes on OpenGL ES 3.0+, we
     * have to explicitly request GLSL 3.00 or later (at least on iOS). */
    use_gl_PerVertex = 0;
    if (opengl_sl_version_is_at_least(3,0)) {
        ASSERT(!is_separate, return 0);  // See note in set_features().
        inout_style = 1;
        static const char * const version_300es_header[] = {
            "#version 300 es\n",
        };
        version_header = version_300es_header;
        version_header_lines = lenof(version_300es_header);
    } else {
        inout_style = 0;
        version_header = NULL;
        version_header_lines = 0;
    }
#else
    if (opengl_sl_version_is_at_least(1,50)) {
        inout_style = 1;
        use_gl_PerVertex = is_separate;
        static const char * const basic_version_150_header[] = {
            "#version 150\n",
        };
        static const char * const separate_version_150_header[] = {
            "#version 150\n",
            "#extension GL_ARB_separate_shader_objects : require\n",
        };
        if (is_separate) {
            version_header = separate_version_150_header;
            version_header_lines = lenof(separate_version_150_header);
        } else {
            version_header = basic_version_150_header;
            version_header_lines = lenof(basic_version_150_header);
        }
    } else if (opengl_sl_version_is_at_least(1,30)) {
        inout_style = 1;
        use_gl_PerVertex = 0;
        static const char * const basic_version_130_header[] = {
            "#version 130\n",
        };
        static const char * const separate_version_130_header[] = {
            "#version 130\n",
            "#extension GL_ARB_separate_shader_objects : require\n",
        };
        if (is_separate) {
            version_header = separate_version_130_header;
            version_header_lines = lenof(separate_version_130_header);
        } else {
            version_header = basic_version_130_header;
            version_header_lines = lenof(basic_version_130_header);
        }
    } else {
        inout_style = 0;
        use_gl_PerVertex = 0;
        version_header = NULL;
        version_header_lines = 0;
    }
#endif

    static const char * const precision_header[] = {
#ifdef SIL_OPENGL_ES
        /* Some reference materials recommend the use of mediump for texture
         * coordinates, but the precision of mediump is only 1 part in 1024,
         * which is insufficient when rendering textures from texture atlases
         * larger than 1024x1024 texels.  We thus default to using highp for
         * texture coordinates, if available in the fragment language.  To
         * avoid compilation failures when highp is not available, we define
         * a precision "texp" which is either highp or mediump depending on
         * the GL's fragment shader capabilities. */
        "#ifdef GL_FRAGMENT_PRECISION_HIGH\n",
        "#define texp highp\n",
        "#else\n",
        "#define texp mediump\n",
        "#endif\n",
#else  // !SIL_OPENGL_ES
        "#define lowp\n",
        "#define mediump\n",
        "#define highp\n",
        "#define texp\n",
#endif
    };  // precision_header[]

    const char * const *specific_header = NULL;
    int specific_header_lines = 0;
    switch (type) {
      case SHADER_TYPE_VERTEX: {
        static const char * const no_inout_vertex_header[] = {
            "#define in attribute\n",
            "#define out varying\n",
        };
        static const char * const separate_header[] = {
            "out highp vec4 gl_Position;\n",
        };
        static const char * const separate_pointsize_header[] = {
            "out highp vec4 gl_Position;\n",
            "out highp float gl_PointSize;\n",
        };
#ifdef SIL_OPENGL_ES
        /* GLES 2.0 doesn't allow redeclaration of gl_Position, so we
         * have to use different headers in that case. */
        static const char * const separate_pointsize_gles2_header[] = {
            "out highp float gl_PointSize;\n",
        };
#endif
        static const char * const gl_PerVertex_header[] = {
            "out gl_PerVertex {highp vec4 gl_Position;};\n",
        };
        static const char * const gl_PerVertex_pointsize_header[] = {
            "out gl_PerVertex {highp vec4 gl_Position; highp float gl_PointSize};\n",
        };
        if (!inout_style) {
            specific_header = no_inout_vertex_header;
            specific_header_lines = lenof(no_inout_vertex_header);
        } else {
            const char *wordchars =
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_";
            int has_pointsize = 0;
            for (int i = 1; i < length-13; i++) {
                if (!strchr(wordchars, source[i-1])
                 && strncmp(&source[i], "gl_PointSize", 12) == 0
                 && !strchr(wordchars, source[i+12])) {
                    has_pointsize = 1;
                    break;
                }
            }
            if (use_gl_PerVertex) {
                if (has_pointsize) {
                    specific_header = gl_PerVertex_pointsize_header;
                    specific_header_lines = lenof(gl_PerVertex_pointsize_header);
                } else {
                    specific_header = gl_PerVertex_header;
                    specific_header_lines = lenof(gl_PerVertex_header);
                }
            } else if (is_separate) {
#ifdef SIL_OPENGL_ES
                if (opengl_major_version() == 2) {
                    if (has_pointsize) {
                        specific_header = separate_pointsize_gles2_header;
                        specific_header_lines = lenof(separate_pointsize_gles2_header);
                    } else {
                        specific_header = NULL;
                        specific_header_lines = 0;
                    }
                } else
#endif
                if (has_pointsize) {
                    specific_header = separate_pointsize_header;
                    specific_header_lines = lenof(separate_pointsize_header);
                } else {
                    specific_header = separate_header;
                    specific_header_lines = lenof(separate_header);
                }
            } else {
                specific_header = NULL;
                specific_header_lines = 0;
            }
        }
        break;
      }
      case SHADER_TYPE_FRAGMENT: {
        static const char * const inout_fragment_header[] = {
            "#define texture2D texture\n",
            "out lowp vec4 color_out;\n"
        };
        static const char * const no_inout_fragment_header[] = {
            "#define in varying\n",
            "#define color_out gl_FragColor\n",
        };
        if (inout_style) {
            specific_header = inout_fragment_header;
            specific_header_lines = lenof(inout_fragment_header);
        } else {
            specific_header = no_inout_fragment_header;
            specific_header_lines = lenof(no_inout_fragment_header);
        }
        break;
      }
    }

    int source_lines = 1;
    for (const char *s = memchr(source, '\n', length);
         s != NULL && s+1 < source + length;
         s = memchr(s+1, '\n', length - ((s+1)-source)))
    {
        source_lines++;
    }

    const int total_lines =
        version_header_lines + lenof(precision_header) + specific_header_lines
        + source_lines;
    const char **lines = mem_alloc(sizeof(*lines) * total_lines,
                                   sizeof(*lines), MEM_ALLOC_TEMP);
    if (UNLIKELY(!lines)) {
        DLOG("Failed to compile %s shader: out of memory", type_str);
        return 0;
    }
    GLint *lengths = mem_alloc(sizeof(*lengths) * total_lines,
                               sizeof(*lengths), MEM_ALLOC_TEMP);
    if (UNLIKELY(!lengths)) {
        DLOG("Failed to compile %s shader: out of memory", type_str);
        mem_free(lines);
        return 0;
    }

    int line = 0;
    for (int i = 0; i < version_header_lines; i++, line++) {
        lines[line] = version_header[i];
        lengths[line] = strlen(lines[line]);
    }
    for (int i = 0; i < lenof(precision_header); i++, line++) {
        lines[line] = precision_header[i];
        lengths[line] = strlen(lines[line]);
    }
    for (int i = 0; i < specific_header_lines; i++, line++) {
        lines[line] = specific_header[i];
        lengths[line] = strlen(lines[line]);
    }
    const char *s = source;
    for (int i = 0; i < source_lines; i++, line++) {
        ASSERT(s - source < length);
        const char *eol = memchr(s, '\n', length - (s-source));
        int linelen;
        if (eol) {
            linelen = (eol - s) + 1;
        } else {
            linelen = length - (s-source);
        }
        lines[line] = s;
        lengths[line] = linelen;
        s += linelen;
    }
    ASSERT(s - source == length);

    opengl_clear_error();

    glShaderSource(shader, total_lines, lines, lengths);
    mem_free(lengths);
    mem_free(lines);

    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (UNLIKELY(!ok)) {
#ifdef DEBUG
        char *info = opengl_get_shader_string(GET_SHADER_INFO, shader);
        /* We could just reuse the source code buffer passed to us along
         * with the header we prepended to it, but we ask the GL for the
         * source instead so we see what it actually tried to compile. */
        char *gl_source = opengl_get_shader_string(GET_SHADER_SOURCE, shader);
        DLOG("Failed to compile %s shader!  Log follows:\n%s\n"
             "Shader source code follows:\n"
             "================\n%s================",
             type_str, info, gl_source);
        mem_free(info);
        mem_free(gl_source);
#endif
        return 0;
    }

    ASSERT(glGetError() == GL_NO_ERROR, return 0);
    return 1;
}

/*-----------------------------------------------------------------------*/

int opengl_link_shader(GLuint program, ...)
{
    opengl_clear_error();

    va_list args;
    va_start(args, program);
    for (GLuint shader; (shader = va_arg(args, GLuint)) != 0; ) {
        glAttachShader(program, shader);
        const GLenum error = glGetError();
        if (UNLIKELY(error != GL_NO_ERROR)) {
            DLOG("Failed to attach shader %u to program %u: 0x%04X",
                 shader, program, error);
            return 0;
        }
    }
    va_end(args);

    glLinkProgram(program);

    /*
     * A common OpenGL pattern for saving memory is to immediately detach
     * (and then delete) shader objects after linking them into a program,
     * and indeed, the definition of glCreateShaderProgramv() is defined
     * as the result of an algorithm including that pattern.  However, on
     * at least some OpenGL ES 2 devices (confirmed with SIL on the PowerVR
     * SGX 540 [Galaxy Nexus, Android 4.2.2], and reportedly seen on the
     * Tegra 2: https://code.google.com/p/android/issues/detail?id=61832),
     * detaching and deleting the shader causes the linked shader program
     * to misbehave, as if it was attempting to use memory which belonged
     * to the deleted shader objects.
     *
     * To work around this, we skip the glDetachShader() call on renderers
     * which do not support separate shader objects, letting OpenGL clean
     * up the shader objects when the programs are deleted.  We assume
     * that renderers which _do_ support SSOs also support detaching
     * shader objects from linked programs, since the definition of
     * glCreateShaderProgramv() uses that pattern.
     *
     * Technically speaking, the OpenGL specification only requires that
     * detach operations on a program object that is "in use" (presumably
     * meaning "has been installed with glUseProgram()") have no effect on
     * the executable code, so this might not be a driver bug in the most
     * literal sense of the word.
     */
    if (opengl_has_features(OPENGL_FEATURE_SEPARATE_SHADERS)) {
        va_start(args, program);
        for (GLuint shader; (shader = va_arg(args, GLuint)) != 0; ) {
            glDetachShader(program, shader);
        }
        va_end(args);
    }

    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (UNLIKELY(!ok)) {
#ifdef DEBUG
        char *info = opengl_get_shader_string(GET_PROGRAM_INFO, program);
        DLOG("Failed to link shader program!  Log follows:\n%s", info);
        mem_free(info);
#endif
        return 0;
    }

    ASSERT(glGetError() == GL_NO_ERROR, return 0);
    return 1;
}

/*************************************************************************/
/************************* Debug logging helpers *************************/
/*************************************************************************/

#ifdef DEBUG

/*-----------------------------------------------------------------------*/

const char *opengl_uniform_type_name(UniformType type)
{
    static const char * const names[] = {
        [UNIFORM_TYPE_UNKNOWN] = "<unset>",
        [UNIFORM_TYPE_INT    ] = "int",
        [UNIFORM_TYPE_FLOAT  ] = "float",
        [UNIFORM_TYPE_VEC2   ] = "vec2",
        [UNIFORM_TYPE_VEC3   ] = "vec3",
        [UNIFORM_TYPE_VEC4   ] = "vec4",
        [UNIFORM_TYPE_MAT4   ] = "mat4",
    };
    ASSERT(type < lenof(names));
    return names[type];
}

/*-----------------------------------------------------------------------*/

char *opengl_get_shader_string(ShaderStringType type, GLuint object)
{
    GLint string_length = -1;
    switch (type) {
      case GET_SHADER_SOURCE:
        glGetShaderiv(object, GL_SHADER_SOURCE_LENGTH, &string_length);
        break;
      case GET_SHADER_INFO:
        glGetShaderiv(object, GL_INFO_LOG_LENGTH, &string_length);
        break;
      case GET_PROGRAM_INFO:
        glGetProgramiv(object, GL_INFO_LOG_LENGTH, &string_length);
        break;
    }
    if (UNLIKELY(string_length < 0)) {
        return mem_strdup("(unavailable)", MEM_ALLOC_TEMP);
    }

    char *buf = mem_alloc(string_length+1, 1, MEM_ALLOC_TEMP);
    if (UNLIKELY(!buf)) {
        /* This will probably fail too, but just in case it works: */
        return mem_strdup("(out of memory)", MEM_ALLOC_TEMP);
    }

    switch (type) {
      case GET_SHADER_SOURCE:
        glGetShaderSource(object, string_length+1, NULL, buf);
        break;
      case GET_SHADER_INFO:
        glGetShaderInfoLog(object, string_length+1, NULL, buf);
        break;
      case GET_PROGRAM_INFO:
        glGetProgramInfoLog(object, string_length+1, NULL, buf);
        break;
    }
    return buf;
}

/*-----------------------------------------------------------------------*/

#endif  // DEBUG

/*************************************************************************/
/*************************************************************************/
