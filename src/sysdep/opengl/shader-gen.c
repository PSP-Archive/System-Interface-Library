/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/opengl/shader-gen.c: Shader generator functionality for
 * OpenGL-based platforms.
 */

#include "src/base.h"
#include "src/graphics.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/shader.h"
#include "src/sysdep.h"
#include "src/sysdep/opengl/internal.h"
#include "src/sysdep/opengl/opengl.h"
#include "src/sysdep/opengl/shader-common.h"
#include "src/sysdep/opengl/shader-table.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Value used in ShaderInfo.program meaning "shader creation failed".  This
 * is used to avoid trying to recreate the same shader over and over when
 * we already know it won't work.  We use (GLuint)-1 on the assumption
 * that the GL numbers program IDs from 1 up (as most seem to do), but
 * create_shader() checks for a collision with this ID and explicitly
 * leaks the program object and tries again if a collision occurs. */
#define INVALID_PROGRAM_ID  ((GLuint)-1)

/*-----------------------------------------------------------------------*/

/* Callback functions for generating shader source code and key values. */
static ShaderSourceCallback *vertex_shader_source_callback;
static ShaderSourceCallback *fragment_shader_source_callback;
static ShaderKeyCallback *shader_key_callback;

/* Shader program currently in use. */
static ShaderInfo *current_shader = NULL;

/* Information about user-specified uniforms. */
typedef struct UserUniformInfo UserUniformInfo;
struct UserUniformInfo {
    char *name;
    UniformType type;
    union {
        int value_int;
        float value_float;
        Vector2f value_vec2;
        Vector3f value_vec3;
        Vector4f value_vec4;
        Matrix4f value_mat4;  // Pre-transposed for glUniformMatrix4fv().
    };
};
static int num_user_uniforms;
static UserUniformInfo *user_uniforms;

/* Information about user-specified vertex attributes.  The data is
 * separated into two arrays so we can return the size array from
 * opengl_get_user_attrib_sizes(). */
static int num_user_attribs;
static char **user_attrib_names;
static int8_t *user_attrib_sizes; // int8_t to save space (size is always 1-4).

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * create_shader:  Create a new shader for the given vertex type.
 *
 * [Parameters]
 *     primitive_type: Primitive type (GL_TRIANGLES, etc.).
 *     position_count: Number of position elements per vertex (2-4).
 *     texcoord_count: Number of texture coordinate elements per vertex
 *         (0 or 2).
 *     texcolor_type: Texture color type (GRAPHICS_TEXCOLOR_*); ignored if
 *         texturing is disabled.
 *     tex_offset: True for an external texture offset passed as a uniform
 *         parameter, false for none.
 *     color_count: Number of color elements per vertex (0 or 4).
 *     color_uniform: True for an external fixed color passed as a uniform
 *         parameter, false for none.
 *     fog: True for linear fog, false for no fog.
 *     alpha_test: True for alpha testing (discard pixels with alpha less
 *         than the reference value), false for no alpha testing.
 *     alpha_comparison: Alpha test comparison type (GL_GEQUAL, etc.).
 * [Return value]
 *     OpenGL program object, or zero on error.
 */
static GLuint create_shader(
    GLenum primitive_type, int position_count, int texcoord_count,
    GraphicsTextureColorType texcolor_type, int tex_offset, int color_count,
    int color_uniform, int fog, int alpha_test, GLenum alpha_comparison);

/**
 * generate_vertex_shader_source, generate_fragment_shader_source:  Generate
 * vertex or fragment shader source code for the given parameters.
 *
 * [Parameters]
 *     primitive_type: Primitive type (GRAPHICS_PRIMITIVE_*).
 *     position_count: Number of position elements per vertex (2-4).
 *     texcoord_count: Number of texture coordinate elements per vertex
 *         (0 or 2).
 *     texcolor_type: Number and type of color components in the current
 *         texture's data (GRAPHICS_TEXCOLOR_*).
 *     tex_offset: True for an external texture offset passed as a uniform
 *         parameter, false for none.
 *     color_count: Number of color elements per vertex (0 or 4).
 *     color_uniform: True for an external fixed color passed as a uniform
 *         parameter, false for none.
 *     fog: True for linear fog, false for no fog.
 *     alpha_test: True for alpha testing (discard pixels with alpha less
 *         than the reference value), false for no alpha testing.
 *     alpha_comparison: Alpha test comparison type (GRAPHICS_COMPARISON_*).
 *     vertex_shader_ret: Pointer to variable in which to store the
 *         generated vertex shader source code.  Guaranteed non-NULL by
 *         caller.
 *     fragment_shader_ret: Pointer to variable in which to store the
 *         generated fragment shader source code.  Guaranteed non-NULL by
 *         caller.
 * [Return value]
 *     True on success, false on error.
 */
static char *generate_vertex_shader_source(
    GraphicsPrimitiveType primitive_type, int position_count,
    int texcoord_count, GraphicsTextureColorType texcolor_type, int tex_offset,
    int color_count, int color_uniform, int fog, int alpha_test,
    GraphicsComparisonType alpha_comparison);
static char *generate_fragment_shader_source(
    GraphicsPrimitiveType primitive_type, int position_count,
    int texcoord_count, GraphicsTextureColorType texcolor_type, int tex_offset,
    int color_count, int color_uniform, int fog, int alpha_test,
    GraphicsComparisonType alpha_comparison);

/**
 * generate_shader_key:  Return a unique 32-bit key for a shader with the
 * given parameters.
 *
 * [Parameters]
 *     As for generate_{vertex,fragment}_shader_source().
 * [Return value]
 *     Unique 32-bit shader key.
 */
static uint32_t generate_shader_key(
    GraphicsPrimitiveType primitive_type, int position_count,
    int texcoord_count, GraphicsTextureColorType texcolor_type, int tex_offset,
    int color_count, int color_uniform, int fog, int alpha_test,
    GraphicsComparisonType alpha_comparison);

/**
 * update_user_uniforms:  Send the values for all defined user uniforms to
 * the GL.  Called after changing the current shader.
 */
static void update_user_uniforms(void);

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int sys_(graphics_set_shader_generator)(
    void *vertex_source_callback, void *fragment_source_callback,
    void *key_callback, int hash_table_size, int dynamic_resize)
{
    opengl_clear_generated_shaders();

    if (vertex_source_callback) {
        vertex_shader_source_callback = vertex_source_callback;
        fragment_shader_source_callback = fragment_source_callback;
        shader_key_callback = key_callback;
        shader_table_init(hash_table_size, dynamic_resize);
    } else {
        vertex_shader_source_callback = generate_vertex_shader_source;
        fragment_shader_source_callback = generate_fragment_shader_source;
        shader_key_callback = generate_shader_key;
        shader_table_init(2 *  // primitive_type==POINTS ? 1 : 0
                          3 *  // lenof(position_counts)
                          2 *  // lenof(texcoord_counts)
                          4 *  // lenof(texcolor_types)
                          2 *  // tex_offset ? 1 : 0
                          2 *  // lenof(color_counts)
                          2 *  // color_uniform ? 1 : 0
                          2 *  // fog ? 1 : 0
                          5,   // 1 + lenof(alpha_comparison_types)
                          0);  // All shaders covered, so no need to resize.
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

int sys_(graphics_add_shader_uniform)(const char *name)
{
    if (strncmp(name, "gl_", 3) == 0) {
        DLOG("Invalid uniform name: %s", name);
        return 0;
    }
    for (int i = 0; i < num_user_uniforms; i++) {
        if (strcmp(name, user_uniforms[i].name) == 0) {
            DLOG("Duplicate uniform name: %s", name);
            return 0;
        }
    }
    for (int i = 0; i < num_user_attribs; i++) {
        if (strcmp(name, user_attrib_names[i]) == 0) {
            DLOG("Uniform name collides with attribute name: %s", name);
            return 0;
        }
    }

    char *name_copy = mem_strdup(name, 0);
    if (UNLIKELY(!name_copy)) {
        DLOG("No memory to copy uniform name %s", name);
        return 0;
    }
    UserUniformInfo *new_user_uniforms = mem_realloc(
        user_uniforms, sizeof(*user_uniforms) * (num_user_uniforms + 1), 0);
    if (UNLIKELY(!new_user_uniforms)) {
        DLOG("No memory to expand user uniform table to %d entries",
             num_user_uniforms + 1);
        mem_free(name_copy);
        return 0;
    }
    user_uniforms = new_user_uniforms;

    const int index = num_user_uniforms++;
    user_uniforms[index].name = name_copy;
    user_uniforms[index].type = UNIFORM_TYPE_UNKNOWN;

    return index + 1;
}

/*-----------------------------------------------------------------------*/

int sys_(graphics_add_shader_attribute)(const char *name, int size)
{
    /* The OpenGL ES spec mandates at least 8 vertex attributes (desktop GL
     * mandates 16). */
    const int MAX_VERTEX_ATTRIBS_MIN = 8;
    ASSERT(SHADER_ATTRIBUTE__NUM <= MAX_VERTEX_ATTRIBS_MIN, return 0);
    const int USER_ATTRIBS_MAX = 4095;  // Per documentation in sysdep.h.
    ASSERT(SHADER_ATTRIBUTE__NUM <= USER_ATTRIBS_MAX, return 0);
    GLint max_vertex_attribs = MAX_VERTEX_ATTRIBS_MIN;
    opengl_clear_error();
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &max_vertex_attribs);
    ASSERT(max_vertex_attribs >= MAX_VERTEX_ATTRIBS_MIN, return 0);
    max_vertex_attribs = ubound(
        max_vertex_attribs, SHADER_ATTRIBUTE__NUM + USER_ATTRIBS_MAX);

    if (SHADER_ATTRIBUTE__NUM + num_user_attribs >= max_vertex_attribs) {
        DLOG("Too many vertex attributes (limit %d)", max_vertex_attribs);
        return 0;
    }
    if (strncmp(name, "gl_", 3) == 0) {
        DLOG("Invalid attribute name: %s", name);
        return 0;
    }
    for (int i = 0; i < num_user_attribs; i++) {
        if (strcmp(name, user_attrib_names[i]) == 0) {
            DLOG("Duplicate attribute name: %s", name);
            return 0;
        }
    }
    for (int i = 0; i < num_user_uniforms; i++) {
        if (strcmp(name, user_uniforms[i].name) == 0) {
            DLOG("Attribute name collides with uniform name: %s", name);
            return 0;
        }
    }

    char *name_copy = mem_strdup(name, 0);
    if (UNLIKELY(!name_copy)) {
        DLOG("No memory to copy attribute name %s", name);
        return 0;
    }
    char **new_user_attrib_names =
        mem_realloc(user_attrib_names,
                    sizeof(*user_attrib_names) * (num_user_attribs + 1), 0);
    if (UNLIKELY(!new_user_attrib_names)) {
        DLOG("No memory to expand user attribute name table to %d entries",
             num_user_attribs + 1);
        mem_free(name_copy);
        return 0;
    }
    user_attrib_names = new_user_attrib_names;
    int8_t *new_user_attrib_sizes =
        mem_realloc(user_attrib_sizes,
                    sizeof(*user_attrib_sizes) * (num_user_attribs + 1), 0);
    if (UNLIKELY(!new_user_attrib_sizes)) {
        DLOG("No memory to expand user attribute size table to %d entries",
             num_user_attribs + 1);
        mem_free(name_copy);
        return 0;
    }
    user_attrib_sizes = new_user_attrib_sizes;

    const int index = num_user_attribs++;
    user_attrib_names[index] = name_copy;
    user_attrib_sizes[index] = size;

    return index + 1;
}

/*-----------------------------------------------------------------------*/

void sys_(graphics_set_shader_uniform_int)(int uniform, int value)
{
    if (uniform < 1 || uniform > num_user_uniforms) {
        DLOG("Invalid uniform ID: %d", uniform);
        return;
    }
    const int index = uniform - 1;

    if (user_uniforms[index].type == UNIFORM_TYPE_UNKNOWN) {
        user_uniforms[index].type = UNIFORM_TYPE_INT;
    } else if (user_uniforms[index].type != UNIFORM_TYPE_INT) {
        DLOG("Type mismatch: uniform %s is of type %s, not INT",
             user_uniforms[index].name,
             opengl_uniform_type_name(user_uniforms[index].type));
        return;
    }

    user_uniforms[index].value_int = value;
    if (current_shader && index < current_shader->num_user_uniforms) {
        glUniform1i(current_shader->user_uniforms[index], value);
    }
}

/*-----------------------------------------------------------------------*/

void sys_(graphics_set_shader_uniform_float)(int uniform, float value)
{
    if (uniform < 1 || uniform > num_user_uniforms) {
        DLOG("Invalid uniform ID: %d", uniform);
        return;
    }
    const int index = uniform - 1;

    if (user_uniforms[index].type == UNIFORM_TYPE_UNKNOWN) {
        user_uniforms[index].type = UNIFORM_TYPE_FLOAT;
    } else if (user_uniforms[index].type != UNIFORM_TYPE_FLOAT) {
        DLOG("Type mismatch: uniform %s is of type %s, not FLOAT",
             user_uniforms[index].name,
             opengl_uniform_type_name(user_uniforms[index].type));
        return;
    }

    user_uniforms[index].value_float = value;
    if (current_shader && index < current_shader->num_user_uniforms) {
        glUniform1f(current_shader->user_uniforms[index], value);
    }
}

/*-----------------------------------------------------------------------*/

void sys_(graphics_set_shader_uniform_vec2)(int uniform, const Vector2f *value)
{
    if (uniform < 1 || uniform > num_user_uniforms) {
        DLOG("Invalid uniform ID: %d", uniform);
        return;
    }
    const int index = uniform - 1;

    if (user_uniforms[index].type == UNIFORM_TYPE_UNKNOWN) {
        user_uniforms[index].type = UNIFORM_TYPE_VEC2;
    } else if (user_uniforms[index].type != UNIFORM_TYPE_VEC2) {
        DLOG("Type mismatch: uniform %s is of type %s, not VEC2",
             user_uniforms[index].name,
             opengl_uniform_type_name(user_uniforms[index].type));
        return;
    }

    user_uniforms[index].value_vec2 = *value;
    if (current_shader && index < current_shader->num_user_uniforms) {
        glUniform2fv(current_shader->user_uniforms[index], 1, &value->x);
    }
}

/*-----------------------------------------------------------------------*/

void sys_(graphics_set_shader_uniform_vec3)(int uniform, const Vector3f *value)
{
    if (uniform < 1 || uniform > num_user_uniforms) {
        DLOG("Invalid uniform ID: %d", uniform);
        return;
    }
    const int index = uniform - 1;

    if (user_uniforms[index].type == UNIFORM_TYPE_UNKNOWN) {
        user_uniforms[index].type = UNIFORM_TYPE_VEC3;
    } else if (user_uniforms[index].type != UNIFORM_TYPE_VEC3) {
        DLOG("Type mismatch: uniform %s is of type %s, not VEC3",
             user_uniforms[index].name,
             opengl_uniform_type_name(user_uniforms[index].type));
        return;
    }

    user_uniforms[index].value_vec3 = *value;
    if (current_shader && index < current_shader->num_user_uniforms) {
        glUniform3fv(current_shader->user_uniforms[index], 1, &value->x);
    }
}

/*-----------------------------------------------------------------------*/

void sys_(graphics_set_shader_uniform_vec4)(int uniform, const Vector4f *value)
{
    if (uniform < 1 || uniform > num_user_uniforms) {
        DLOG("Invalid uniform ID: %d", uniform);
        return;
    }
    const int index = uniform - 1;

    if (user_uniforms[index].type == UNIFORM_TYPE_UNKNOWN) {
        user_uniforms[index].type = UNIFORM_TYPE_VEC4;
    } else if (user_uniforms[index].type != UNIFORM_TYPE_VEC4) {
        DLOG("Type mismatch: uniform %s is of type %s, not VEC4",
             user_uniforms[index].name,
             opengl_uniform_type_name(user_uniforms[index].type));
        return;
    }

    user_uniforms[index].value_vec4 = *value;
    if (current_shader && index < current_shader->num_user_uniforms) {
        glUniform4fv(current_shader->user_uniforms[index], 1, &value->x);
    }
}

/*-----------------------------------------------------------------------*/

void sys_(graphics_set_shader_uniform_mat4)(int uniform, const Matrix4f *value)
{
    if (uniform < 1 || uniform > num_user_uniforms) {
        DLOG("Invalid uniform ID: %d", uniform);
        return;
    }
    const int index = uniform - 1;

    if (user_uniforms[index].type == UNIFORM_TYPE_UNKNOWN) {
        user_uniforms[index].type = UNIFORM_TYPE_MAT4;
    } else if (user_uniforms[index].type != UNIFORM_TYPE_MAT4) {
        DLOG("Type mismatch: uniform %s is of type %s, not MAT4",
             user_uniforms[index].name,
             opengl_uniform_type_name(user_uniforms[index].type));
        return;
    }

    mat4_transpose(&user_uniforms[index].value_mat4, value);
    if (current_shader && index < current_shader->num_user_uniforms) {
        glUniformMatrix4fv(current_shader->user_uniforms[index], 1, GL_FALSE,
                           &user_uniforms[index].value_mat4._11);
    }
}

/*************************************************************************/
/******************* Library-internal utility routines *******************/
/*************************************************************************/

int opengl_select_shader(const SysPrimitive *primitive,
                         const SysTexture *texture, int tex_offset,
                         int color_uniform, int fog, int alpha_test,
                         GraphicsComparisonType alpha_comparison)
{
    /* Extract data about the current primitive. */
    GraphicsPrimitiveType primitive_type;
    primitive_type = 0;
    switch (primitive->type) {
      case GL_POINTS:
        primitive_type = GRAPHICS_PRIMITIVE_POINTS;
        break;
      case GL_LINES:
        primitive_type = GRAPHICS_PRIMITIVE_LINES;
        break;
      case GL_LINE_STRIP:
        primitive_type = GRAPHICS_PRIMITIVE_LINE_STRIP;
        break;
      case GL_TRIANGLES:
        primitive_type = GRAPHICS_PRIMITIVE_TRIANGLES;
        break;
      case GL_TRIANGLE_STRIP:
        primitive_type = GRAPHICS_PRIMITIVE_TRIANGLE_STRIP;
        break;
#if GL_QUADS != GL_INVALID_ENUM
      case GL_QUADS:
        primitive_type = GRAPHICS_PRIMITIVE_QUADS;
        break;
#endif
#if GL_QUAD_STRIP != GL_INVALID_ENUM
      case GL_QUAD_STRIP:
        primitive_type = GRAPHICS_PRIMITIVE_QUAD_STRIP;
        break;
#endif
    }
    ASSERT(primitive_type != 0, return -1);

    const int position_count = primitive->position_size;
    const int texcoord_count = primitive->texcoord_size;
    const int color_count = primitive->color_size;

    GraphicsTextureColorType texcolor_type = GRAPHICS_TEXCOLOR_NONE;
    if (texture) {
        switch (texture->color_type) {
            case TEXCOLOR_RGBA: texcolor_type = GRAPHICS_TEXCOLOR_RGBA; break;
            case TEXCOLOR_RGB:  texcolor_type = GRAPHICS_TEXCOLOR_RGBA; break;
            case TEXCOLOR_A:    texcolor_type = GRAPHICS_TEXCOLOR_A;    break;
            case TEXCOLOR_L:    texcolor_type = GRAPHICS_TEXCOLOR_L;    break;
        }
        ASSERT(texcolor_type != GRAPHICS_TEXCOLOR_NONE);
    }

    /* Look up the ShaderInfo structure for this vertex data type. */
    const uint32_t key = (*shader_key_callback)(
        primitive_type, position_count, texcoord_count, texcolor_type,
        tex_offset, color_count, color_uniform, fog, alpha_test,
        alpha_comparison);
    if (key == INVALID_SHADER_KEY) {
        return -1;
    }
    int invalidate = 1; // Play it safe and null out current_shader by default.
    ShaderInfo * const shader_info = shader_table_lookup(key, &invalidate);
    if (invalidate) {
        current_shader = NULL;  // Old pointer is no longer valid.
    }
    if (UNLIKELY(!shader_info)) {
        DLOG("Failed to look up shader for key %u", key);
        return -1;
    }

    /* Create the shader program if necessary. */
    if (!shader_info->program) {
        GLint *shader_user_uniforms;
        if (num_user_uniforms > 0) {
            shader_user_uniforms = mem_alloc(
                sizeof(*shader_user_uniforms) * num_user_uniforms, 0, 0);
            if (UNLIKELY(!shader_user_uniforms)) {
                DLOG("No memory for %d user uniform locations",
                     num_user_uniforms);
                return -1;
            }
        } else {
            shader_user_uniforms = NULL;
        }
        const GLuint new_program = create_shader(
            primitive_type, position_count, texcoord_count, texcolor_type,
            tex_offset, color_count, color_uniform, fog, alpha_test,
            alpha_comparison);
        if (UNLIKELY(!new_program)) {
            DLOG("Failed to create shader for position=%d texcoord=%d/%d"
                 " texcolor=%d color=%d/%d", position_count, texcoord_count,
                 tex_offset, texcolor_type, color_count, color_uniform);
            shader_info->program = INVALID_PROGRAM_ID;
            mem_free(shader_user_uniforms);
            return -1;
        }
        shader_info->program = new_program;
        shader_info->uniforms[UNIFORM_TRANSFORM] =
            glGetUniformLocation(new_program, "transform");
        shader_info->uniforms[UNIFORM_TEXTURE] =
            glGetUniformLocation(new_program, "texture");
        shader_info->uniforms[UNIFORM_TEX_OFFSET] =
            glGetUniformLocation(new_program, "tex_offset");
        shader_info->uniforms[UNIFORM_FIXED_COLOR] =
            glGetUniformLocation(new_program, "fixed_color");
        shader_info->uniforms[UNIFORM_FOG_PARAMS] =
            glGetUniformLocation(new_program, "fog_params");
        shader_info->uniforms[UNIFORM_FOG_TRANSFORM] =
            glGetUniformLocation(new_program, "fog_transform");
        shader_info->uniforms[UNIFORM_FOG_COLOR] =
            glGetUniformLocation(new_program, "fog_color");
        shader_info->uniforms[UNIFORM_ALPHA_REF] =
            glGetUniformLocation(new_program, "alpha_ref");
        shader_info->uniforms[UNIFORM_POINT_SIZE] =
            glGetUniformLocation(new_program, "point_size");
        shader_info->num_user_uniforms = num_user_uniforms;
        shader_info->user_uniforms = shader_user_uniforms;
        for (int i = 0; i < num_user_uniforms; i++) {
            shader_user_uniforms[i] =
                glGetUniformLocation(new_program, user_uniforms[i].name);
        }
    }

    /* Activate the shader program if it's not already active. */
    int changed = 0;
    if (shader_info != current_shader) {
        if (shader_info->program == INVALID_PROGRAM_ID) {
            return -1;
        }
        current_shader = shader_info;
        changed = 1;
        glUseProgram(current_shader->program);
        update_user_uniforms();
    }

    return changed;
}

/*-----------------------------------------------------------------------*/

void opengl_deselect_shader(void)
{
    current_shader = NULL;
    glUseProgram(0);
}

/*-----------------------------------------------------------------------*/

void opengl_set_uniform_int(int uniform, int value)
{
    PRECOND(uniform >= 0 && uniform < UNIFORM__NUM, return);
    if (current_shader) {
        glUniform1i(current_shader->uniforms[uniform], value);
    }
}

/*-----------------------------------------------------------------------*/

void opengl_set_uniform_float(int uniform, float value)
{
    PRECOND(uniform >= 0 && uniform < UNIFORM__NUM, return);
    if (current_shader) {
        glUniform1f(current_shader->uniforms[uniform], value);
    }
}

/*-----------------------------------------------------------------------*/

void opengl_set_uniform_vec2(int uniform, const Vector2f *value)
{
    PRECOND(uniform >= 0 && uniform < UNIFORM__NUM, return);
    if (current_shader) {
        glUniform2fv(current_shader->uniforms[uniform], 1, &value->x);
    }
}

/*-----------------------------------------------------------------------*/

void opengl_set_uniform_vec4(int uniform, const Vector4f *value)
{
    PRECOND(uniform >= 0 && uniform < UNIFORM__NUM, return);
    if (current_shader) {
        glUniform4fv(current_shader->uniforms[uniform], 1, &value->x);
    }
}

/*-----------------------------------------------------------------------*/

void opengl_set_uniform_mat4(int uniform, const Matrix4f *value)
{
    PRECOND(uniform >= 0 && uniform < UNIFORM__NUM, return);
    if (current_shader) {
#ifdef SIL_OPENGL_ES
        if (opengl_major_version() < 3) {
            Matrix4f transpose;
            mat4_transpose(&transpose, value);
            glUniformMatrix4fv(current_shader->uniforms[uniform], 1, GL_FALSE,
                               &transpose._11);
            return;
        }
#endif
        glUniformMatrix4fv(current_shader->uniforms[uniform], 1, GL_TRUE,
                           &value->_11);
    }
}

/*-----------------------------------------------------------------------*/

const int8_t *opengl_get_user_attrib_sizes(int *num_user_attribs_ret)
{
    PRECOND(num_user_attribs_ret != NULL);
    *num_user_attribs_ret = num_user_attribs;
    return user_attrib_sizes;
}

/*-----------------------------------------------------------------------*/

void opengl_clear_generated_shaders(void)
{
    opengl_deselect_shader();
    shader_table_clear();

    for (int i = 0; i < num_user_uniforms; i++) {
        mem_free(user_uniforms[i].name);
    }
    mem_free(user_uniforms);
    user_uniforms = NULL;
    num_user_uniforms = 0;

    for (int i = 0; i < num_user_attribs; i++) {
        mem_free(user_attrib_names[i]);
        /* Make sure any previously-set attribute pointers are cleared.
         * When we draw a primitive, we only clear pointers up to the
         * current number of user attributes, so if a subsequent draw
         * operation has a smaller number of user primitives, the driver
         * might attempt to read from the (probably stale) pointers left
         * in the untouched attribute pointers. */
        glDisableVertexAttribArray(SHADER_ATTRIBUTE__NUM + i);
    }
    mem_free(user_attrib_names);
    user_attrib_names = NULL;
    mem_free(user_attrib_sizes);
    user_attrib_sizes = NULL;
    num_user_attribs = 0;
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static GLuint create_shader(
    GLenum primitive_type, int position_count, int texcoord_count,
    GraphicsTextureColorType texcolor_type, int tex_offset, int color_count,
    int color_uniform, int fog, int alpha_test, GLenum alpha_comparison)
{
    opengl_clear_error();

    /* Create the OpenGL program and shader resources. */
    const GLuint program = glCreateProgram();
    if (UNLIKELY(!program)) {
        DLOG("Failed to create new shader program");
        goto error_return;
    }
    if (UNLIKELY(program == INVALID_PROGRAM_ID)) {
        DLOG("Shader program collided with INVALID_PROGRAM_ID (%u),"
             " leaking it and generating another", INVALID_PROGRAM_ID);
        return create_shader(primitive_type, position_count, texcoord_count,
                             texcolor_type, tex_offset, color_count,
                             color_uniform, fog, alpha_test, alpha_comparison);
    }
    const GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    if (UNLIKELY(!vertex_shader)) {
        DLOG("Failed to create new vertex shader");
        goto error_delete_program;
    }
    const GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    if (UNLIKELY(!fragment_shader)) {
        DLOG("Failed to create new vertex shader");
        goto error_delete_vertex_shader;
    }
    ASSERT(glGetError() == GL_NO_ERROR, goto error_delete_fragment_shader);

    /* Bind attribute indices to their respective names. */
    glBindAttribLocation(program, SHADER_ATTRIBUTE_POSITION, "position");
    glBindAttribLocation(program, SHADER_ATTRIBUTE_TEXCOORD, "texcoord");
    glBindAttribLocation(program, SHADER_ATTRIBUTE_COLOR,    "color");
    for (int i = 0; i < num_user_attribs; i++) {
        glBindAttribLocation(program, SHADER_ATTRIBUTE__NUM + i,
                             user_attrib_names[i]);
    }
#if defined(SIL_OPENGL_SHADER_VERSION) && SIL_OPENGL_SHADER_VERSION >= 130
    glBindFragDataLocation(program, 0, "color_out");
#endif
    const GLenum error = glGetError();
    if (UNLIKELY(error != GL_NO_ERROR)) {
        DLOG("Failed to bind shader variables: 0x%04X", error);
        goto error_delete_fragment_shader;
    }

    /* Generate source code for the shaders. */
    char *vertex_shader_source = (*vertex_shader_source_callback)(
        primitive_type, position_count, texcoord_count, texcolor_type,
        tex_offset, color_count, color_uniform, fog, alpha_test,
        alpha_comparison);
    if (UNLIKELY(!vertex_shader_source)) {
        goto error_delete_fragment_shader;
    } else if (UNLIKELY(!*vertex_shader_source)) {
        DLOG("Vertex shader generator returned an empty string");
        mem_free(vertex_shader_source);
        goto error_delete_fragment_shader;
    }
    char *fragment_shader_source = (*fragment_shader_source_callback)(
        primitive_type, position_count, texcoord_count, texcolor_type,
        tex_offset, color_count, color_uniform, fog, alpha_test,
        alpha_comparison);
    if (UNLIKELY(!fragment_shader_source)) {
        mem_free(vertex_shader_source);
        goto error_delete_fragment_shader;
    } else if (UNLIKELY(!*fragment_shader_source)) {
        DLOG("Fragment shader generator returned an empty string");
        mem_free(fragment_shader_source);
        mem_free(vertex_shader_source);
        goto error_delete_fragment_shader;
    }

    /* Compile the shaders.  We compile both shaders before checking the
     * results so that (1) we can unconditionally free the source strings
     * and (2) if there are errors in both vertex and fragment shaders,
     * both error sets are logged at once. */
    const int vertex_ok = opengl_compile_shader(
        vertex_shader, vertex_shader_source,
        strlen(vertex_shader_source), SHADER_TYPE_VERTEX, 0);
    mem_free(vertex_shader_source);
    const int fragment_ok = opengl_compile_shader(
        fragment_shader, fragment_shader_source,
        strlen(fragment_shader_source), SHADER_TYPE_FRAGMENT, 0);
    mem_free(fragment_shader_source);
    if (UNLIKELY(!vertex_ok) || UNLIKELY(!fragment_ok)) {
        goto error_delete_fragment_shader;
    }

    /* Link the vertex and shader fragment together to create the shader
     * program. */
    if (UNLIKELY(!opengl_link_shader(program,
                                     vertex_shader, fragment_shader, 0))) {
        goto error_delete_fragment_shader;
    }
    ASSERT(glGetError() == GL_NO_ERROR, goto error_delete_fragment_shader);

    /* Success!  Dump the shader source and logs if requested, and return
     * the new program. */
#if defined(DEBUG) && defined(SIL_OPENGL_DUMP_SHADERS)
    char *vert_source =
        opengl_get_shader_string(GET_SHADER_SOURCE, vertex_shader);
    char *vert_info =
        opengl_get_shader_string(GET_SHADER_INFO, vertex_shader);
    char *frag_source =
        opengl_get_shader_string(GET_SHADER_SOURCE,fragment_shader);
    char *frag_info =
        opengl_get_shader_string(GET_SHADER_INFO, fragment_shader);
    char *link_info =
        opengl_get_shader_string(GET_PROGRAM_INFO, program);
    DLOG("\n"
         "******** BEGIN SHADER DUMP for position=%d texcoord=%d%s color=%d%s ********\n"
         "\n"
         "Vertex shader source:\n"
         "================\n"
         "%s"
         "================\n"
         "\n"
         "Vertex shader compile log:\n"
         "================\n"
         "%s"
         "================\n"
         "\n"
         "Fragment shader source:\n"
         "================\n"
         "%s"
         "================\n"
         "\n"
         "Fragment shader compile log:\n"
         "================\n"
         "%s"
         "================\n"
         "\n"
         "Program link log:\n"
         "================\n"
         "%s"
         "================\n"
         "\n"
         "********* END SHADER DUMP for position=%d texcoord=%d%s color=%d%s *********\n",
         position_count,
             texcoord_count, tex_offset ? "+offset" : "",
             color_count, color_uniform ? "*fixed" : "",
         vert_source ? vert_source : "",
         vert_info ? vert_info : "",
         frag_source ? frag_source : "",
         frag_info ? frag_info : "",
         link_info ? link_info : "",
         position_count,
             texcoord_count, tex_offset ? "+offset" : "",
             color_count, color_uniform ? "*fixed" : "");
    mem_free(vert_source);
    mem_free(vert_info);
    mem_free(frag_source);
    mem_free(frag_info);
    mem_free(link_info);
#endif

    return program;

    /* Avoid resource leaks on error. */
  error_delete_fragment_shader:
    opengl_delete_shader(fragment_shader);
  error_delete_vertex_shader:
    opengl_delete_shader(vertex_shader);
  error_delete_program:
    opengl_delete_program(program);
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

static void update_user_uniforms(void)
{
    PRECOND(current_shader != NULL, return);

    const int num_uniforms = current_shader->num_user_uniforms;
    for (int i = 0; i < num_uniforms; i++) {
        const GLint uniform = current_shader->user_uniforms[i];
        if (uniform == -1) {
            continue;
        }
        const UserUniformInfo *info = &user_uniforms[i];
        switch (info->type) {
          case UNIFORM_TYPE_INT:
            glUniform1i(uniform, info->value_int);
            break;
          case UNIFORM_TYPE_FLOAT:
            glUniform1f(uniform, info->value_float);
            break;
          case UNIFORM_TYPE_VEC2:
            glUniform2fv(uniform, 1, &info->value_vec2.x);
            break;
          case UNIFORM_TYPE_VEC3:
            glUniform3fv(uniform, 1, &info->value_vec3.x);
            break;
          case UNIFORM_TYPE_VEC4:
            glUniform4fv(uniform, 1, &info->value_vec4.x);
            break;
          case UNIFORM_TYPE_MAT4:
            /* Matrix data is pre-transposed into column-major order. */
            glUniformMatrix4fv(uniform, 1, GL_FALSE, &info->value_mat4._11);
            break;
        }
        const GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            DLOG("Failed to load uniform %s: 0x%04X", info->name, error);
        }
    }
}

/*************************************************************************/
/*********************** Default shader generator ************************/
/*************************************************************************/

#define ADD_LINE(s) \
    (source_len += strformat(source+source_len, \
                             sizeof(source)-source_len, "%s\n", (s)))

/*-----------------------------------------------------------------------*/

static char *generate_vertex_shader_source(
    GraphicsPrimitiveType primitive_type, int position_count,
    int texcoord_count, UNUSED GraphicsTextureColorType texcolor_type,
    int tex_offset, int color_count, int color_uniform, int fog,
    UNUSED int alpha_test, UNUSED GraphicsComparisonType alpha_comparison)
{
    char source[10000];
    int source_len = 0;

    ADD_LINE("uniform highp mat4 transform;");
    if (tex_offset) {
        ADD_LINE("uniform texp vec2 tex_offset;");
    }
    if (color_uniform && color_count) {
        ADD_LINE("uniform lowp vec4 fixed_color;");
    }
    if (primitive_type == GRAPHICS_PRIMITIVE_POINTS) {
        ADD_LINE("uniform mediump float point_size;");
    }
    switch (position_count) {
      case 2:
        ADD_LINE("in highp vec2 position;");
        break;
      case 3:
        ADD_LINE("in highp vec3 position;");
        break;
      case 4:
        ADD_LINE("in highp vec4 position;");
        break;
    }
    switch (texcoord_count) {
      case 2:
        ADD_LINE("in texp vec2 texcoord;");
        ADD_LINE("out texp vec2 texcoord_varying;");
        break;
    }
    switch (color_count) {
      case 4:
        ADD_LINE("in lowp vec4 color;");
        ADD_LINE("out lowp vec4 color_varying;");
        break;
    }
    if (fog) {
        ADD_LINE("uniform highp vec4 fog_transform;");
        ADD_LINE("out texp float fog_varying;");
    }

    ADD_LINE("void main() {");

    switch (position_count) {
      case 2:
        ADD_LINE("    gl_Position = vec4(position, 0.0, 1.0) * transform;");
        break;
      case 3:
        ADD_LINE("    gl_Position = vec4(position, 1.0) * transform;");
        break;
      case 4:
        ADD_LINE("    gl_Position = position * transform;");
        break;
    }

    switch (texcoord_count) {
      case 2:
        if (tex_offset) {
            ADD_LINE("    texcoord_varying = texcoord + tex_offset;");
        } else {
            ADD_LINE("    texcoord_varying = texcoord;");
        }
        break;
    }

    switch (color_count) {
      case 4:
        if (color_uniform) {
            ADD_LINE("    color_varying = color * fixed_color;");
        } else {
            ADD_LINE("    color_varying = color;");
        }
        break;
    }

    if (fog) {
        switch (position_count) {
          case 2:
            ADD_LINE("    fog_varying = dot(fog_transform, vec4(position, 0.0, 1.0));");
            break;
          case 3:
            ADD_LINE("    fog_varying = dot(fog_transform, vec4(position, 1.0));");
            break;
          case 4:
            ADD_LINE("    fog_varying = dot(fog_transform, position);");
            break;
        }
    }

    if (primitive_type == GRAPHICS_PRIMITIVE_POINTS) {
        ADD_LINE("    gl_PointSize = point_size;");
    }

    ADD_LINE("}");

    ASSERT(source_len <= (int)sizeof(source) - 1);
    char *shader = mem_strdup(source, MEM_ALLOC_TEMP);
    if (UNLIKELY(!shader)) {
        DLOG("No memory for copy of vertex shader source");
    }
    return shader;
}

/*-----------------------------------------------------------------------*/

static char *generate_fragment_shader_source(
    UNUSED GraphicsPrimitiveType primitive_type, UNUSED int position_count,
    int texcoord_count, GraphicsTextureColorType texcolor_type,
    UNUSED int tex_offset, int color_count, int color_uniform, int fog,
    int alpha_test, GraphicsComparisonType alpha_comparison)
{
    char source[10000];
    int source_len = 0;

    if (color_uniform && !color_count) {
        ADD_LINE("uniform lowp vec4 fixed_color;");
    }
    if (texcoord_count > 0) {
        ADD_LINE("uniform lowp sampler2D tex;");
        ADD_LINE("in texp vec2 texcoord_varying;");
    }
    if (color_count > 0) {
        ADD_LINE("in lowp vec4 color_varying;");
    }
    if (fog) {
        ADD_LINE("in texp float fog_varying;");
        ADD_LINE("uniform texp vec2 fog_params;");
        ADD_LINE("uniform lowp vec4 fog_color;");
    }
    if (alpha_test) {
        ADD_LINE("uniform lowp float alpha_ref;");
    }

    ADD_LINE("void main() {");

    if (texcoord_count == 0) {
        texcolor_type = GRAPHICS_TEXCOLOR_NONE;
    }
    switch (texcolor_type) {
      case GRAPHICS_TEXCOLOR_NONE:
        break;
      case GRAPHICS_TEXCOLOR_RGBA:
        ADD_LINE("    lowp vec4 sampleval = texture2D(tex, texcoord_varying);");
        break;
      case GRAPHICS_TEXCOLOR_A:
      case GRAPHICS_TEXCOLOR_L:
        ADD_LINE("    texp float sampleval = texture2D(tex, texcoord_varying).r;");
        break;
    }

    ADD_LINE("    lowp vec4 color =");
    if (color_count > 0) {
        switch (texcolor_type) {
          case GRAPHICS_TEXCOLOR_NONE:
            ADD_LINE("        color_varying;");
            break;
          case GRAPHICS_TEXCOLOR_RGBA:
            ADD_LINE("        sampleval * color_varying;");
            break;
          case GRAPHICS_TEXCOLOR_A:
            ADD_LINE("        vec4(color_varying.rgb, color_varying.a * sampleval);");
            break;
          case GRAPHICS_TEXCOLOR_L:
            ADD_LINE("        vec4(color_varying.rgb * sampleval, color_varying.a);");
            break;
        }
    } else if (color_uniform) {
        switch (texcolor_type) {
          case GRAPHICS_TEXCOLOR_NONE:
            ADD_LINE("        fixed_color;");
            break;
          case GRAPHICS_TEXCOLOR_RGBA:
            ADD_LINE("        sampleval * fixed_color;");
            break;
          case GRAPHICS_TEXCOLOR_A:
            ADD_LINE("        vec4(fixed_color.rgb, fixed_color.a * sampleval);");
            break;
          case GRAPHICS_TEXCOLOR_L:
            ADD_LINE("        vec4(fixed_color.rgb * sampleval, fixed_color.a);");
            break;
        }
    } else {
        switch (texcolor_type) {
          case GRAPHICS_TEXCOLOR_NONE:
            ADD_LINE("        vec4(1.0, 1.0, 1.0, 1.0);");
            break;
          case GRAPHICS_TEXCOLOR_RGBA:
            ADD_LINE("        sampleval;");
            break;
          case GRAPHICS_TEXCOLOR_A:
            ADD_LINE("        vec4(1.0, 1.0, 1.0, sampleval);");
            break;
          case GRAPHICS_TEXCOLOR_L:
            ADD_LINE("        vec4(sampleval, sampleval, sampleval, 1.0);");
            break;
        }
    }

    if (alpha_test) {
        switch (alpha_comparison) {
          case GRAPHICS_COMPARISON_LESS:
            ADD_LINE("    if (color.a >= alpha_ref) discard;");
            break;
          case GRAPHICS_COMPARISON_LESS_EQUAL:
            ADD_LINE("    if (color.a > alpha_ref) discard;");
            break;
          case GRAPHICS_COMPARISON_GREATER_EQUAL:
            ADD_LINE("    if (color.a < alpha_ref) discard;");
            break;
          case GRAPHICS_COMPARISON_GREATER:
            ADD_LINE("    if (color.a <= alpha_ref) discard;");
            break;
          default:
            DLOG("WARNING: Ignoring invalid alpha comparison type %d",
                 alpha_comparison);
            break;
        }
    }

    if (fog) {
        ADD_LINE("    texp float fog_factor = abs(fog_varying) * fog_params.x - fog_params.y;");
        ADD_LINE("    color_out = mix(color, vec4(fog_color.rgb, color.a), clamp(fog_factor, 0.0, 1.0));");
    } else {
        ADD_LINE("    color_out = color;");
    }

    ADD_LINE("}");

    char *shader = mem_strdup(source, MEM_ALLOC_TEMP);
    if (UNLIKELY(!shader)) {
        DLOG("No memory for copy of fragment shader source");
    }
    return shader;
}

/*-----------------------------------------------------------------------*/

static uint32_t generate_shader_key(
    GraphicsPrimitiveType primitive_type, int position_count,
    int texcoord_count, GraphicsTextureColorType texcolor_type, int tex_offset,
    int color_count, int color_uniform, int fog, int alpha_test,
    GraphicsComparisonType alpha_comparison)
{
    /* Lists of all valid vertex data counts and texture color counts, used
     * to create the shader key.  For data other than position (which is
     * required), a count of 0 indicates that the given field is not
     * present in the vertex data at all, or that no texture is applied. */
    static const int8_t position_counts[] = {2, 3, 4};
    static const int8_t texcoord_counts[] = {0, 2};
    static const uint8_t texcolor_types[] = {
        GRAPHICS_TEXCOLOR_NONE,
        GRAPHICS_TEXCOLOR_RGBA,
        GRAPHICS_TEXCOLOR_A,
        GRAPHICS_TEXCOLOR_L,
    };
    static const int8_t color_counts[] = {0, 4};
    static const GraphicsComparisonType alpha_comparison_types[] = {
        GRAPHICS_COMPARISON_LESS,
        GRAPHICS_COMPARISON_LESS_EQUAL,
        GRAPHICS_COMPARISON_GREATER_EQUAL,
        GRAPHICS_COMPARISON_GREATER,
    };

    /* Look up the position count, texture coordinate count, texture color
     * count, vertex color count, and alpha comparison type indices. */
    int position_index = -1, texcoord_index = -1, texcolor_index = -1,
        color_index = -1, alpha_comparison_index = -1;
    for (int i = 0; i < lenof(position_counts); i++) {
        if (position_count == position_counts[i]) {
            position_index = i;
            break;
        }
    }
    for (int i = 0; i < lenof(texcoord_counts); i++) {
        if (texcoord_count == texcoord_counts[i]) {
            texcoord_index = i;
            break;
        }
    }
    for (int i = 0; i < lenof(texcolor_types); i++) {
        if (texcolor_type == texcolor_types[i]) {
            texcolor_index = i;
            break;
        }
    }
    for (int i = 0; i < lenof(color_counts); i++) {
        if (color_count == color_counts[i]) {
            color_index = i;
            break;
        }
    }
    for (int i = 0; i < lenof(alpha_comparison_types); i++) {
        if (alpha_comparison == alpha_comparison_types[i]) {
            alpha_comparison_index = i;
            break;
        }
    }
    if (position_index < 0 || texcoord_index < 0 || texcolor_index < 0
     || color_index < 0 || (alpha_test && alpha_comparison_index < 0)) {
        DLOG("Invalid vertex type: position=%d texcoord=%d texcolor=%d"
             " color=%d alpha_comparison=%d", position_count, texcoord_count,
             texcolor_type, color_count, alpha_comparison);
        return INVALID_SHADER_KEY;
    }

    /* Generate a unique key for the shader by just treating each attribute
     * as a digit in a base-N value (where N varies by digit depending on
     * the number of options for each attribute). */
    uint32_t key = (primitive_type == GRAPHICS_PRIMITIVE_POINTS ? 1 : 0);
    key *= lenof(position_counts);
    key += position_index;
    key *= lenof(texcoord_counts);
    key += texcoord_index;
    key *= lenof(texcolor_types);
    key += texcolor_index;
    key *= 2;
    key += tex_offset ? 1 : 0;
    key *= lenof(color_counts);
    key += color_index;
    key *= 2;
    key += color_uniform ? 1 : 0;
    key *= 2;
    key += fog ? 1 : 0;
    key *= 1 + lenof(alpha_comparison_types);
    if (alpha_test) {
        key += 1 + alpha_comparison_index;
    }
    return key;
}

/*-----------------------------------------------------------------------*/

#undef ADD_LINE

/*************************************************************************/
/*************************************************************************/
