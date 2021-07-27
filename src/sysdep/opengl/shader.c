/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/opengl/shader.c: Shader object and shader pipeline handling
 * for OpenGL-based platforms.
 */

#include "src/base.h"
#include "src/graphics.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/opengl/internal.h"
#include "src/sysdep/opengl/opengl.h"
#include "src/sysdep/opengl/shader-common.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Shader uniform data structure.  This is used to store information about
 * uniforms for OpenGL shaders when ARB_separate_shader_objects is not
 * available, since the uniforms can't be looked up until the shader is
 * linked into a program (and changes to the uniform values must then be
 * propagated to all programs into which the shader is linked). */
typedef struct ShaderUniform ShaderUniform;
struct ShaderUniform {
    /* Uniform name.  If it fits within the buffer, is_local (which overlays
     * the first byte of buf[]) is nonzero; otherwise, is_local is zero and
     * ptr.ptr points into the shared uniform data buffer. */
    union {
        char is_local;  // Treated as a Boolean flag.
        char buf[16];
        struct {
            char _pad;
            char *ptr;
        } ptr;
    } name;
    /* Data type. */
    UniformType type;
    /* Pointer to data in the shared uniform data buffer. */
    union {
        void *value_ptr;
        int *int_ptr;
        float *float_ptr;
        Vector2f *vec2_ptr;
        Vector3f *vec3_ptr;
        Vector4f *vec4_ptr;
        Matrix4f *mat4_ptr;
    } value;
};

/* Should shader binary data be readable? */
static uint8_t binary_retrievable_hint = 0;

/* Current attribute bindings (strings are owned by us). */
static char *attribute_bindings[256];
/* Number of bound attributes (highest bound index + 1). */
static int num_attribute_bindings;

/* Current standard attribute bindings + 1 (so we don't need an
 * initialization routine to set them all to -1). */
static int standard_attribute_bindings[SHADER_ATTRIBUTE__NUM];

/* Currently active shader pipeline, or NULL if none. */
static SysShaderPipeline *current_pipeline;

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * create_program:  Create an OpenGL shader program containing the given
 * OpenGL shader objects and attribute bindings.
 *
 * [Parameters]
 *     shader1, shader2: Shaders to link into the program.  Set shader2 to
 *         zero if there is only one shader.
 *     attributes: Attribute index-to-name mapping.
 *     num_attributes: Length of the attribute mapping.
 *     separable: True to set GL_PROGRAM_SEPARABLE on the program.
 *     readable: True to set GL_PROGRAM_BINARY_RETRIEVABLE_HINT on the program.
 * [Return value]
 *     Newly created OpenGL shader program (nonzero), or zero on error.
 */
static GLuint create_program(GLuint shader1, GLuint shader2,
                             const char **attributes, int num_attributes,
                             int separable, int readable);

/**
 * set_uniform_common:  Common implementation for sys_shader_set_uniform_*()
 * functions.
 *
 * [Parameters]
 *     shader: Shader object.
 *     uniform: Uniform ID.  Guaranteed to be nonzero, but _not_ guaranteed
 *         to be valid for the given shader.
 *     value: Pointer to the value to set (int *, float *, Vector2 *, etc).
 *     type: Data type (UNIFORM_TYPE_*).
 */
static inline void set_uniform_common(
    SysShader *shader, int uniform, const void *value, UniformType type);

/**
 * expand_uniform_data:  Expand the size of the uniform_data buffer in the
 * given shader by the given amount.
 *
 * [Parameters]
 *     shader: Shader to operate on.
 *     amount: Number of bytes by which to expand the uniform_data buffer.
 * [Return value]
 *     True on success, false on error.
 */
static int expand_uniform_data(SysShader *shader, int amount);

/**
 * update_uniforms:  Update all GL uniforms in the given (non-separable)
 * pipeline program for the given (non-linked) shader.
 *
 * [Parameters]
 *     pipeline: Pipeline object containing GL shader program to update.
 *     shader: Shader object containing updated uniforms.
 */
static void update_uniforms(SysShaderPipeline *pipeline,
                            const SysShader *shader);

/*************************************************************************/
/****************** Interface: Shader object management ******************/
/*************************************************************************/

int sys_(shader_background_compilation_supported)(void)
{
    return opengl_can_ensure_compile_context();
}

/*-----------------------------------------------------------------------*/

void sys_(shader_enable_get_binary)(int enable)
{
    binary_retrievable_hint = (enable != 0);
}

/*-----------------------------------------------------------------------*/

int sys_(shader_max_attributes)(void)
{
    GLint max_attributes = 0;
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &max_attributes);
    /* OpenGL (desktop) requires MAX_VERTEX_ATTRIBS >= 16; OpenGL ES only
     * requires MAX_VERTEX_ATTRIBS >= 8.  Use the lower value as our panic
     * cutoff for broken libraries. */
    ASSERT(max_attributes >= 8, max_attributes = 8);
    return max_attributes;
}

/*-----------------------------------------------------------------------*/

int sys_(shader_set_attribute)(int index, const char *name)
{
    int retval = 1;
    mem_free(attribute_bindings[index]);
    attribute_bindings[index] = NULL;
    if (name) {
        for (int i = 0; i < num_attribute_bindings; i++) {
            if (attribute_bindings[i]
             && strcmp(attribute_bindings[i], name) == 0) {
                DLOG("Attempt to rebind name %s (attribute %d) to attribute"
                     " %d", name, i, index);
                retval = 0;
                break;
            }
        }
        if (retval) {
            attribute_bindings[index] = mem_strdup(name, 0);
            if (UNLIKELY(!attribute_bindings[index])) {
                DLOG("Failed to copy name for attribute %d: %s", index, name);
                retval = 0;
            }
        }
    }

    if (attribute_bindings[index]) {
        if (index >= num_attribute_bindings) {
            num_attribute_bindings = index + 1;
        }
    } else {
        if (index == num_attribute_bindings - 1) {
            while (--num_attribute_bindings > 0) {
                if (attribute_bindings[num_attribute_bindings - 1]) {
                    break;
                }
            }
        }
    }

    return retval;
}

/*-----------------------------------------------------------------------*/

void sys_(shader_bind_standard_attribute)(ShaderAttribute attribute, int index)
{
    if (index >= 0 && index < 256) {
        standard_attribute_bindings[attribute] = index + 1;
    } else {
        standard_attribute_bindings[attribute] = 0;
    }
}

/*-----------------------------------------------------------------------*/

void sys_(shader_clear_attributes)(void)
{
    for (int i = 0; i < num_attribute_bindings; i++) {
        mem_free(attribute_bindings[i]);
        attribute_bindings[i] = NULL;
    }
    num_attribute_bindings = 0;

    for (int i = 0; i < lenof(standard_attribute_bindings); i++) {
        standard_attribute_bindings[i] = 0;
    }
}

/*-----------------------------------------------------------------------*/

SysShader *sys_(shader_create)(ShaderType type, const void *data, int size,
                             int is_binary)
{
    GLenum gl_type = GL_INVALID_ENUM;
    switch (type) {
        case SHADER_TYPE_VERTEX:   gl_type = GL_VERTEX_SHADER;   break;
        case SHADER_TYPE_FRAGMENT: gl_type = GL_FRAGMENT_SHADER; break;
    }
    ASSERT(gl_type != GL_INVALID_ENUM, goto error_return);

    if (is_binary && !opengl_has_features(OPENGL_FEATURE_SHADER_BINARIES)) {
        DLOG("Shader binaries not supported on this system");
        return 0;
    }

    SysShader *shader = mem_alloc(sizeof(*shader), 0, MEM_ALLOC_CLEAR);
    if (UNLIKELY(!shader)) {
        DLOG("No memory for shader object");
        goto error_return;
    }
    shader->generation = opengl_device_generation;
    shader->type = type;
    if (type == SHADER_TYPE_VERTEX) {
        shader->num_attributes = num_attribute_bindings;
        for (int i = 0; i < lenof(standard_attribute_bindings); i++) {
            shader->standard_attributes[i] =
                standard_attribute_bindings[i] - 1;
        }
    }

    opengl_clear_error();

    GLuint gl_shader = 0;
    if (!is_binary) {
        gl_shader = glCreateShader(gl_type);
        if (UNLIKELY(!gl_shader)) {
            DLOG("Failed to create OpenGL shader object");
            goto error_free_shader;
        }
        const int is_separate =
            opengl_has_features(OPENGL_FEATURE_SEPARATE_SHADERS);
        if (!opengl_compile_shader(gl_shader, data, size, type, is_separate)) {
            goto error_delete_gl_shader;
        }
    }

    if (opengl_has_features(OPENGL_FEATURE_SEPARATE_SHADERS)) {
        shader->is_program = 1;

        if (is_binary) {
            shader->shader = glCreateProgram();
            if (UNLIKELY(!shader->shader)) {
                DLOG("Failed to create OpenGL program object");
                /* Skip glDeleteShader() call since we didn't create a
                 * shader above. */
                goto error_free_shader;
            }
            const int version = ((const uint8_t *)data)[size-1];
            if (version != 0) {
                DLOG("Unknown binary data version %d", version);
                glDeleteProgram(shader->shader);
                goto error_free_shader;
            }
            if (size < 2 || size < ((const uint8_t *)data)[size-2]) {
                DLOG("Binary data too short");
                glDeleteProgram(shader->shader);
                goto error_free_shader;
            }
            const int trailer_size = ((const uint8_t *)data)[size-2];
            const uint8_t *trailer =
                (const uint8_t *)data + size - trailer_size;
            const GLenum format = trailer[0]<<8 | trailer[1];
            shader->num_attributes = trailer[2]<<8 | trailer[3];
            glProgramBinary(shader->shader, format, data, size - trailer_size);
            GLint ok;
            glGetProgramiv(shader->shader, GL_LINK_STATUS, &ok);
            const GLenum error = glGetError();
            if (UNLIKELY(error != GL_NO_ERROR)) {
                DLOG("Failed to load binary program: 0x%04X%s", error,
                     error==GL_INVALID_ENUM ? " (binary format not supported)"
                                            : "");
                glDeleteProgram(shader->shader);
                goto error_free_shader;
            } else if (UNLIKELY(!ok)) {
#ifdef DEBUG
                char *info =
                    opengl_get_shader_string(GET_PROGRAM_INFO, shader->shader);
                DLOG("Failed to load binary program!  Log follows:\n%s", info);
                mem_free(info);
#endif
                glDeleteProgram(shader->shader);
                goto error_free_shader;
            }
        } else {  // !is_binary
            shader->shader = create_program(
                gl_shader, 0, (const char **)attribute_bindings,
                num_attribute_bindings, 1, binary_retrievable_hint);
            glDeleteShader(gl_shader);  // No longer needed.
            if (UNLIKELY(!shader->shader)) {
                goto error_free_shader;
            }
        }

    } else {  // !opengl_has_features(OPENGL_FEATURE_SEPARATE_SHADERS)
        if (is_binary) {
            DLOG("Binary loading not supported for non-separable shaders");
            goto error_delete_gl_shader;
        }
        shader->is_program = 0;
        shader->shader = gl_shader;
        if (type == SHADER_TYPE_VERTEX && num_attribute_bindings > 0) {
            const int array_size =
                sizeof(*shader->attributes) * num_attribute_bindings;
            int buffer_size = array_size;
            for (int i = 0; i < num_attribute_bindings; i++) {
                if (attribute_bindings[i]) {
                    const int name_size = strlen(attribute_bindings[i]) + 1;
                    buffer_size += name_size;
                }
            }
            void *buffer = mem_alloc(buffer_size, 0, 0);
            if (UNLIKELY(!buffer)) {
                DLOG("No memory for %d shader attributes (%d bytes)",
                     num_attribute_bindings, size);
                goto error_delete_gl_shader;
            }
            shader->attributes = (const char **)buffer;
            char *s = (char *)buffer + array_size;
            for (int i = 0; i < num_attribute_bindings; i++) {
                if (attribute_bindings[i]) {
                    shader->attributes[i] = s;
                    const int name_size = strlen(attribute_bindings[i]) + 1;
                    memcpy(s, attribute_bindings[i], name_size);
                    s += name_size;
                } else {
                    shader->attributes[i] = NULL;
                }
            }
            ASSERT(s == (char *)buffer + buffer_size);
        }
    }

    shader->pipelines = NULL;
    return shader;

  error_delete_gl_shader:
    glDeleteShader(gl_shader);
  error_free_shader:
    mem_free(shader);
  error_return:
    return NULL;
}

/*-----------------------------------------------------------------------*/

void sys_(shader_destroy)(SysShader *shader)
{
    const int is_current = (shader->generation == opengl_device_generation);

    for (SysShaderPipeline *pipeline = shader->pipelines, *next; pipeline;
         pipeline = next)
    {
        if (shader->type == SHADER_TYPE_VERTEX) {
            next = pipeline->vertex_next;
            pipeline->vertex_next = NULL;
            pipeline->vertex_prev_ptr = NULL;
            pipeline->vertex_shader = NULL;
            ASSERT(pipeline->generation == shader->generation, continue);
            if (is_current && shader->is_program) {
                glUseProgramStages(pipeline->program, GL_VERTEX_SHADER_BIT, 0);
            }
        } else {
            ASSERT(shader->type == SHADER_TYPE_FRAGMENT, return);
            next = pipeline->fragment_next;
            pipeline->fragment_next = NULL;
            pipeline->fragment_prev_ptr = NULL;
            pipeline->fragment_shader = NULL;
            ASSERT(pipeline->generation == shader->generation, continue);
            if (is_current && shader->is_program) {
                glUseProgramStages(pipeline->program,
                                   GL_FRAGMENT_SHADER_BIT, 0);
            }
        }
        if (is_current && !shader->is_program) {
            glDetachShader(pipeline->program, shader->shader);
        }
    }

    if (is_current) {
        if (shader->is_program) {
            opengl_delete_program(shader->shader);
        } else {  // !shader->is_program
            opengl_delete_shader(shader->shader);
        }
    }

    mem_free(shader->attributes);
    mem_free(shader->uniforms);
    mem_free(shader->uniform_data);
    mem_free(shader);
}

/*-----------------------------------------------------------------------*/

void *sys_(shader_get_binary)(SysShader *shader, int *size_ret)
{
    if (!opengl_has_features(OPENGL_FEATURE_SHADER_BINARIES)) {
        DLOG("Shader binaries not supported on this system");
        return 0;
    }

    if (UNLIKELY(shader->generation != opengl_device_generation)) {
        DLOG("Attempt to use invalidated shader %p", shader);
        return NULL;
    }

    if (!shader->is_program) {
        DLOG("Binary retrieval not supported for non-separable shaders");
        return NULL;
    }

    GLint size = -1;
    glGetProgramiv(shader->shader, GL_PROGRAM_BINARY_LENGTH, &size);
    if (UNLIKELY(size < 0)) {
        DLOG("Failed to get program binary size: 0x%04X", glGetError());
        return NULL;
    } else if (size == 0) {
        DLOG("Program binary size is zero, assuming not supported");
        return NULL;
    }
    ASSERT(glGetError() == GL_NO_ERROR, return NULL);

    size += 6;  // Include space for the data trailer.
    uint8_t *data = mem_alloc(size, 0, 0);
    if (UNLIKELY(!data)) {
        DLOG("No memory for program binary (%d bytes)", size);
        return NULL;
    }

    GLsizei returned_size = -1;
    GLenum format = 0;
    glGetProgramBinary(shader->shader, size-6, &returned_size, &format, data);
    const GLenum error = glGetError();
    if (UNLIKELY(error != GL_NO_ERROR) || UNLIKELY(returned_size == -1)) {
        DLOG("Failed to get program binary: 0x%04X", error);
        mem_free(data);
        return NULL;
    }
    ASSERT(returned_size == size-6, return NULL);

    data[size-6] = (format >> 8) & 0xFF;
    data[size-5] = (format >> 0) & 0xFF;
    data[size-4] = (shader->num_attributes >> 8) & 0xFF;
    data[size-3] = (shader->num_attributes >> 0) & 0xFF;
    data[size-2] = 6;  // Trailer size.
    data[size-1] = 0;  // Trailer format version.
    *size_ret = size;
    return data;
}

/*-----------------------------------------------------------------------*/

void *sys_(shader_compile)(ShaderType type, const char *source, int length,
                           int *size_ret)
{
    GLenum gl_type = GL_INVALID_ENUM;
    switch (type) {
        case SHADER_TYPE_VERTEX:   gl_type = GL_VERTEX_SHADER;   break;
        case SHADER_TYPE_FRAGMENT: gl_type = GL_FRAGMENT_SHADER; break;
    }
    ASSERT(gl_type != GL_INVALID_ENUM, return NULL);

    if (!opengl_has_features(OPENGL_FEATURE_SEPARATE_SHADERS
                             | OPENGL_FEATURE_SHADER_BINARIES)) {
        DLOG("Binary retrieval not supported on this system");
        return NULL;
    }

    if (!opengl_ensure_compile_context()) {
        DLOG("Unable to create subthread GL context");
        return NULL;
    }

    const GLuint shader = glCreateShader(gl_type);
    if (UNLIKELY(!shader)) {
        DLOG("Failed to create OpenGL shader object");
        goto error_return;
    }
    if (!opengl_compile_shader(shader, source, length, type, 1)) {
        goto error_delete_shader;
    }

    const GLuint program = create_program(
        shader, 0, (const char **)attribute_bindings,
        num_attribute_bindings, 1, 1);
    if (UNLIKELY(!program)) {
        goto error_delete_shader;
    }

    /* Set up a dummy SysShader object so we can reuse
     * sys_shader_get_binary(). */
    SysShader sys_shader = {
        .generation = opengl_device_generation,
        .type = type,
        .shader = program,
        .is_program = 1,
        .num_attributes =
            type==SHADER_TYPE_VERTEX ? num_attribute_bindings : 0,
    };
    void *data = sys_(shader_get_binary)(&sys_shader, size_ret);
    if (UNLIKELY(!data)) {
        goto error_delete_program;
    }

    glDeleteProgram(program);
    glDeleteShader(shader);
    return data;

  error_delete_program:
    glDeleteProgram(program);
  error_delete_shader:
    glDeleteShader(shader);
  error_return:
    return NULL;
}

/*-----------------------------------------------------------------------*/

int sys_(shader_get_uniform_id)(SysShader *shader, const char *name)
{
    if (UNLIKELY(shader->generation != opengl_device_generation)) {
        DLOG("Attempt to use invalidated shader %p", shader);
        return 0;
    }

    if (shader->is_program) {

        /* OpenGL uses -1 as the "does not exist" value, so add 1 to the
         * GL uniform location to get our uniform ID. */
        return glGetUniformLocation(shader->shader, name) + 1;

    } else {

        /* Use a simple linear search on the assumption that most shaders
         * will have few enough uniforms (and this function will be called
         * infrequently enough) that the extra complexity of a hash table
         * or sorted list outweighs the benefit. */
        for (int i = 0; i < shader->num_uniforms; i++) {
            const ShaderUniform *uniform = &shader->uniforms[i];
            const char *uniform_name =
                (uniform->name.is_local
                 ? uniform->name.buf : uniform->name.ptr.ptr);
            if (strcmp(uniform_name, name) == 0) {
                return i + 1;
            }
        }

        ShaderUniform *new_uniforms = mem_realloc(
            shader->uniforms,
            sizeof(*shader->uniforms) * (shader->num_uniforms+1), 0);
        if (UNLIKELY(!new_uniforms)) {
            DLOG("Failed to expand uniform list to %d entries for uniform %s",
                 shader->num_uniforms+1, name);
            return 0;
        }
        shader->uniforms = new_uniforms;
        ShaderUniform *uniform = &shader->uniforms[shader->num_uniforms];
        const int name_size = strlen(name) + 1;
        if (name_size <= (int)sizeof(uniform->name.buf)) {
            memcpy(uniform->name.buf, name, name_size);
        } else {
            uniform->name.is_local = 0;
            const int offset = shader->uniform_data_size;
            if (UNLIKELY(!expand_uniform_data(shader, name_size))) {
                DLOG("Failed to expand uniform data buffer to %d bytes for"
                     " uniform %s", offset+name_size, name);
                return 0;
            }
            uniform->name.ptr.ptr = (char *)shader->uniform_data + offset;
            memcpy(uniform->name.ptr.ptr, name, name_size);
        }
        uniform->type = UNIFORM_TYPE_UNKNOWN;
        uniform->value.value_ptr = NULL;

        const int index = shader->num_uniforms++;
        return index+1;

    }  // if (shader->is_program)
}

/*-----------------------------------------------------------------------*/

void sys_(shader_set_uniform_int)(SysShader *shader, int uniform, int value)
{
    if (UNLIKELY(shader->generation != opengl_device_generation)) {
        DLOG("Attempt to use invalidated shader %p", shader);
        return;
    }
    set_uniform_common(shader, uniform, &value, UNIFORM_TYPE_INT);
}

/*-----------------------------------------------------------------------*/

void sys_(shader_set_uniform_float)(
    SysShader *shader, int uniform, float value)
{
    if (UNLIKELY(shader->generation != opengl_device_generation)) {
        DLOG("Attempt to use invalidated shader %p", shader);
        return;
    }
    set_uniform_common(shader, uniform, &value, UNIFORM_TYPE_FLOAT);
}

/*-----------------------------------------------------------------------*/

void sys_(shader_set_uniform_vec2)(
    SysShader *shader, int uniform, const Vector2f *value)
{
    if (UNLIKELY(shader->generation != opengl_device_generation)) {
        DLOG("Attempt to use invalidated shader %p", shader);
        return;
    }
    set_uniform_common(shader, uniform, value, UNIFORM_TYPE_VEC2);
}

/*-----------------------------------------------------------------------*/

void sys_(shader_set_uniform_vec3)(
    SysShader *shader, int uniform, const Vector3f *value)
{
    if (UNLIKELY(shader->generation != opengl_device_generation)) {
        DLOG("Attempt to use invalidated shader %p", shader);
        return;
    }
    set_uniform_common(shader, uniform, value, UNIFORM_TYPE_VEC3);
}

/*-----------------------------------------------------------------------*/

void sys_(shader_set_uniform_vec4)(
    SysShader *shader, int uniform, const Vector4f *value)
{
    if (UNLIKELY(shader->generation != opengl_device_generation)) {
        DLOG("Attempt to use invalidated shader %p", shader);
        return;
    }
    set_uniform_common(shader, uniform, value, UNIFORM_TYPE_VEC4);
}

/*-----------------------------------------------------------------------*/

void sys_(shader_set_uniform_mat4)(
    SysShader *shader, int uniform, const Matrix4f *value)
{
    if (UNLIKELY(shader->generation != opengl_device_generation)) {
        DLOG("Attempt to use invalidated shader %p", shader);
        return;
    }
    set_uniform_common(shader, uniform, value, UNIFORM_TYPE_MAT4);
}

/*************************************************************************/
/***************** Interface: Shader pipeline management *****************/
/*************************************************************************/

SysShaderPipeline *sys_(shader_pipeline_create)(
    SysShader *vertex_shader, SysShader *fragment_shader)
{
    if (UNLIKELY(vertex_shader->type != SHADER_TYPE_VERTEX)) {
        DLOG("Invalid type %d for vertex shader", vertex_shader->type);
        goto error_return;
    }
    if (UNLIKELY(fragment_shader->type != SHADER_TYPE_FRAGMENT)) {
        DLOG("Invalid type %d for fragment shader", fragment_shader->type);
        goto error_return;
    }
    if (UNLIKELY(vertex_shader->generation != opengl_device_generation)) {
        DLOG("Attempt to use invalidated vertex shader %p", vertex_shader);
        goto error_return;
    }
    if (UNLIKELY(fragment_shader->generation != opengl_device_generation)) {
        DLOG("Attempt to use invalidated fragment shader %p", fragment_shader);
        goto error_return;
    }

    SysShaderPipeline *pipeline =
        mem_alloc(sizeof(*pipeline), 0, MEM_ALLOC_CLEAR);
    if (UNLIKELY(!pipeline)) {
        DLOG("No memory for pipeline object");
        goto error_return;
    }
    pipeline->generation = opengl_device_generation;
    pipeline->vertex_shader = vertex_shader;
    pipeline->fragment_shader = fragment_shader;

    opengl_clear_error();

    if (opengl_has_features(OPENGL_FEATURE_SEPARATE_SHADERS)) {
        ASSERT(vertex_shader->is_program, goto error_free_pipeline);
        ASSERT(fragment_shader->is_program, goto error_free_pipeline);
        pipeline->is_pipeline = 1;
        pipeline->program = 0;
        glCreateProgramPipelines(1, &pipeline->program);
        if (UNLIKELY(!pipeline->program)) {
            DLOG("Failed to create OpenGL pipeline object");
            goto error_free_pipeline;
        }
        glUseProgramStages(pipeline->program,
                           GL_VERTEX_SHADER_BIT, vertex_shader->shader);
        glUseProgramStages(pipeline->program,
                           GL_FRAGMENT_SHADER_BIT, fragment_shader->shader);
        const GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            DLOG("Failed to initialize OpenGL pipeline object: 0x%04X", error);
            glDeleteProgramPipelines(1, &pipeline->program);
            goto error_free_pipeline;
        }

    } else {  // !opengl_has_features(OPENGL_FEATURE_SEPARATE_SHADERS)
        ASSERT(!vertex_shader->is_program, goto error_free_pipeline);
        ASSERT(!fragment_shader->is_program, goto error_free_pipeline);
        pipeline->is_pipeline = 0;
        pipeline->program = create_program(
            vertex_shader->shader, fragment_shader->shader,
            (const char **)vertex_shader->attributes,
            vertex_shader->num_attributes, 0, 0);
        if (UNLIKELY(!pipeline->program)) {
            goto error_free_pipeline;
        }
    }

    pipeline->num_inputs = vertex_shader->num_attributes;
    pipeline->vertex_next = vertex_shader->pipelines;
    pipeline->vertex_prev_ptr = &vertex_shader->pipelines;
    if (vertex_shader->pipelines) {
        vertex_shader->pipelines->vertex_prev_ptr = &pipeline->vertex_next;
    }
    vertex_shader->pipelines = pipeline;
    pipeline->fragment_next = fragment_shader->pipelines;
    pipeline->fragment_prev_ptr = &fragment_shader->pipelines;
    if (fragment_shader->pipelines) {
        fragment_shader->pipelines->fragment_prev_ptr =
            &pipeline->fragment_next;
    }
    fragment_shader->pipelines = pipeline;

    return pipeline;

  error_free_pipeline:
    mem_free(pipeline);
  error_return:
    return NULL;
}

/*-----------------------------------------------------------------------*/

void sys_(shader_pipeline_destroy)(SysShaderPipeline *pipeline)
{
    if (current_pipeline == pipeline) {
        sys_(shader_pipeline_apply)(NULL);
    }

    if (pipeline->generation == opengl_device_generation) {
        if (pipeline->is_pipeline) {
            opengl_delete_program_pipeline(pipeline->program);
        } else {
            opengl_delete_program(pipeline->program);
        }
    }

    if (pipeline->vertex_next) {
        pipeline->vertex_next->vertex_prev_ptr = pipeline->vertex_prev_ptr;
    }
    if (pipeline->vertex_prev_ptr) {
        *pipeline->vertex_prev_ptr = pipeline->vertex_next;
    }
    if (pipeline->fragment_next) {
        pipeline->fragment_next->fragment_prev_ptr =
            pipeline->fragment_prev_ptr;
    }
    if (pipeline->fragment_prev_ptr) {
        *pipeline->fragment_prev_ptr = pipeline->fragment_next;
    }

    mem_free(pipeline);
}

/*-----------------------------------------------------------------------*/

void sys_(shader_pipeline_apply)(SysShaderPipeline *pipeline)
{
    if (pipeline
     && UNLIKELY(pipeline->generation != opengl_device_generation)) {
        DLOG("Attempt to use invalidated shader pipeline %p", pipeline);
        return;
    }

    if (!pipeline) {
        if (opengl_has_features(OPENGL_FEATURE_SEPARATE_SHADERS)) {
            glBindProgramPipeline(0);
        }
        glUseProgram(0);
    } else if (pipeline->is_pipeline) {
        ASSERT(opengl_has_features(OPENGL_FEATURE_SEPARATE_SHADERS), return);
        glBindProgramPipeline(pipeline->program);
    } else {
        glUseProgram(pipeline->program);
        const SysShader *vs = pipeline->vertex_shader;
        const SysShader *fs = pipeline->fragment_shader;
        if (vs && pipeline->vertex_generation != vs->uniform_generation) {
            pipeline->vertex_generation = vs->uniform_generation;
            update_uniforms(pipeline, vs);
        }
        if (fs && pipeline->fragment_generation != fs->uniform_generation) {
            pipeline->fragment_generation = fs->uniform_generation;
            update_uniforms(pipeline, fs);
        }
    }

    current_pipeline = pipeline;
}

/*************************************************************************/
/******************* Library-internal utility routines *******************/
/*************************************************************************/

void opengl_shader_init(void)
{
    binary_retrievable_hint = 0;
}

/*-----------------------------------------------------------------------*/

int opengl_shader_num_attributes(void)
{
    if (current_pipeline) {
        return current_pipeline->num_inputs;
    } else {
        return 0;
    }
}

/*-----------------------------------------------------------------------*/

int opengl_shader_standard_attribute_binding(ShaderAttribute attribute)
{
    PRECOND(attribute >= 0 && attribute < SHADER_ATTRIBUTE__NUM, return -1);

    if (current_pipeline && current_pipeline->vertex_shader) {
        return current_pipeline->vertex_shader->standard_attributes[attribute];
    } else {
        return -1;
    }
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static GLuint create_program(GLuint shader1, GLuint shader2,
                             const char **attributes, int num_attributes,
                             int separable, int readable)
{
    const GLuint program = glCreateProgram();
    if (UNLIKELY(!program)) {
        DLOG("Failed to create OpenGL program object");
        goto error_return;
    }

    if (separable) {
        glProgramParameteri(program, GL_PROGRAM_SEPARABLE, GL_TRUE);
    }
    if (readable) {
#ifndef SIL_OPENGL_ES
        glProgramParameteri(program, GL_PROGRAM_BINARY_RETRIEVABLE_HINT,
                            GL_TRUE);
#endif
    }
    GLenum error = glGetError();
    if (UNLIKELY(error != GL_NO_ERROR)) {
        DLOG("Failed to set program status: 0x%04X", error);
        goto error_delete_program;
    }

    for (int i = 0; i < num_attributes; i++) {
        if (attributes[i]) {
            glBindAttribLocation(program, i, attributes[i]);
            error = glGetError();
            if (UNLIKELY(error != GL_NO_ERROR)) {
                DLOG("Failed to bind attribute %d (%s): 0x%04X",
                     i, attribute_bindings[i], error);
                goto error_delete_program;
            }
        }
    }

    if (UNLIKELY(!opengl_link_shader(program, shader1, shader2, 0))) {
        goto error_delete_program;
    }

    return program;

  error_delete_program:
    glDeleteProgram(program);
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

static inline void set_uniform_common(
    SysShader *shader, int uniform, const void *value, UniformType type)
{
    const int location = uniform - 1;

    if (shader->is_program) {

        switch (type) {
          case UNIFORM_TYPE_INT:
            glProgramUniform1i(shader->shader, location, *(const int *)value);
            break;
          case UNIFORM_TYPE_FLOAT:
            glProgramUniform1f(shader->shader, location,
                               *(const float *)value);
            break;
          case UNIFORM_TYPE_VEC2:
            glProgramUniform2fv(shader->shader, location, 1, value);
            break;
          case UNIFORM_TYPE_VEC3:
            glProgramUniform3fv(shader->shader, location, 1, value);
            break;
          case UNIFORM_TYPE_VEC4:
            glProgramUniform4fv(shader->shader, location, 1, value);
            break;
          case UNIFORM_TYPE_MAT4:
#ifdef SIL_OPENGL_ES
            /* GLES 2.0 doesn't allow transpose == TRUE, so we have to
             * transpose the matrix manually. */
            if (opengl_major_version() < 3) {
                Matrix4f transpose;
                mat4_transpose(&transpose, (const Matrix4f *)value);
                glProgramUniformMatrix4fv(shader->shader, location, 1,
                                          GL_FALSE, &transpose._11);
                break;
            }
#endif
            glProgramUniformMatrix4fv(shader->shader, location, 1, GL_TRUE,
                                      value);
            break;
        }

        const GLenum error = glGetError();
        if (UNLIKELY(error != GL_NO_ERROR)) {
            DLOG("Failed to set uniform %d (type %s): 0x%04X", location,
                 opengl_uniform_type_name(type), error);
        }

    } else {  // !shader->is_program

        if (UNLIKELY(location < 0)
         || UNLIKELY(location >= shader->num_uniforms)) {
            DLOG("Uniform index out of range: %d", uniform);
            return;
        }
        ShaderUniform *uni = &shader->uniforms[location];

        int value_size = 0;
        switch (type) {
            case UNIFORM_TYPE_INT:   value_size = sizeof(int);      break;
            case UNIFORM_TYPE_FLOAT: value_size = sizeof(float);    break;
            case UNIFORM_TYPE_VEC2:  value_size = sizeof(Vector2f); break;
            case UNIFORM_TYPE_VEC3:  value_size = sizeof(Vector3f); break;
            case UNIFORM_TYPE_VEC4:  value_size = sizeof(Vector4f); break;
            case UNIFORM_TYPE_MAT4:  value_size = sizeof(Matrix4f); break;
        }
        ASSERT(value_size > 0);

        if (uni->type == UNIFORM_TYPE_UNKNOWN) {
            const int offset = shader->uniform_data_size;
            if (UNLIKELY(!expand_uniform_data(shader, value_size))) {
                DLOG("Failed to expand uniform data buffer to %d bytes for"
                     " uniform %s value (type %s)", offset+value_size,
                     uni->name.is_local ? uni->name.buf : uni->name.ptr.ptr,
                     opengl_uniform_type_name(type));
                return;
            }
            uni->value.value_ptr = (uint8_t *)shader->uniform_data + offset;
            uni->type = type;
        }

        ASSERT(uni->value.value_ptr != NULL, return);
        memcpy(uni->value.value_ptr, value, value_size);

        shader->uniform_generation++;
        if (current_pipeline) {
            if (current_pipeline->vertex_shader == shader) {
                current_pipeline->vertex_generation =
                    shader->uniform_generation;
                update_uniforms(current_pipeline, shader);
            } else if (current_pipeline->fragment_shader == shader) {
                current_pipeline->fragment_generation =
                    shader->uniform_generation;
                update_uniforms(current_pipeline, shader);
            }
        }

    }
}

/*-----------------------------------------------------------------------*/

static int expand_uniform_data(SysShader *shader, int amount)
{
    const int new_size = shader->uniform_data_size + align_up(amount, 4);
    void *new_data = mem_realloc(shader->uniform_data, new_size, 0);
    if (UNLIKELY(!new_data)) {
        return 0;
    }
    const intptr_t data_delta =
        (intptr_t)new_data - (intptr_t)shader->uniform_data;
    shader->uniform_data = new_data;
    shader->uniform_data_size = new_size;

    if (data_delta != 0) {
        for (int i = 0; i < shader->num_uniforms; i++) {
            ShaderUniform *uniform = &shader->uniforms[i];
            if (!uniform->name.is_local) {
                uniform->name.ptr.ptr += data_delta;
            }
            if (uniform->value.value_ptr) {
                uniform->value.value_ptr =
                    (void *)((uintptr_t)uniform->value.value_ptr + data_delta);
            }
        }
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

static void update_uniforms(SysShaderPipeline *pipeline,
                            const SysShader *shader)
{
    PRECOND(pipeline, return);
    PRECOND(!pipeline->is_pipeline, return);
    PRECOND(shader, return);
    PRECOND(!shader->is_program, return);

    for (int i = 0; i < shader->num_uniforms; i++) {
        const ShaderUniform *uniform = &shader->uniforms[i];
        const void *value = uniform->value.value_ptr;
        if (!value) {
            continue;
        }
        const char *name = (uniform->name.is_local ? uniform->name.buf
                                                   : uniform->name.ptr.ptr);
        const GLint location = glGetUniformLocation(pipeline->program, name);
        if (UNLIKELY(location == -1)) {
            continue;
        }
        switch (uniform->type) {
          case UNIFORM_TYPE_INT:
            glUniform1iv(location, 1, value);
            break;
          case UNIFORM_TYPE_FLOAT:
            glUniform1fv(location, 1, value);
            break;
          case UNIFORM_TYPE_VEC2:
            glUniform2fv(location, 1, value);
            break;
          case UNIFORM_TYPE_VEC3:
            glUniform3fv(location, 1, value);
            break;
          case UNIFORM_TYPE_VEC4:
            glUniform4fv(location, 1, value);
            break;
          case UNIFORM_TYPE_MAT4:
#ifdef SIL_OPENGL_ES
            if (opengl_major_version() < 3) {  // See in set_uniform_common().
                Matrix4f transpose;
                mat4_transpose(&transpose, (const Matrix4f *)value);
                glUniformMatrix4fv(location, 1, GL_FALSE, &transpose._11);
                break;
            }
#endif
            glUniformMatrix4fv(location, 1, GL_TRUE, value);
            break;
        }
    }
}

/*************************************************************************/
/*************************************************************************/
