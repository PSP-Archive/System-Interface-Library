/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/graphics/shader.c: Shader management.
 */

#include "src/base.h"
#include "src/graphics.h"
#include "src/graphics/internal.h"
#include "src/math.h"
#include "src/shader.h"
#include "src/sysdep.h"
#include "src/utility/id-array.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Arrays of allocated shaders and shader pipelines. */
static IDArray shaders = ID_ARRAY_INITIALIZER(100);
static IDArray shader_pipelines = ID_ARRAY_INITIALIZER(100);

/**
 * VALIDATE_SHADER, VALIDATE_SHADER_OPTIONAL:  Validate the shader ID
 * passed to a shader manipulation routine, and store the corresponding
 * pointer in the variable "shader".  If the shader ID is invalid, the
 * "error_return" statement is executed; this may consist of multiple
 * statements, but must include a "return" to exit the function.
 */
#define VALIDATE_SHADER(id,shader,error_return) \
    ID_ARRAY_VALIDATE(&shaders, (id), SysShader *, shader, \
                      DLOG("Shader ID %d is invalid", _id); error_return)

/**
 * VALIDATE_SHADER_PIPELINE:  Validate the shader pipeline ID passed to a
 * shader pipeline manipulation routine, and store the corresponding
 * pointer in the variable "pipeline".  If the shader pipeline ID is
 * invalid, the "error_return" statement is executed; this may consist of
 * multiple statements, but must include a "return" to exit the function.
 */
#define VALIDATE_SHADER_PIPELINE(id,pipeline,error_return) \
    ID_ARRAY_VALIDATE(&shader_pipelines, (id), SysShaderPipeline *, pipeline, \
                      DLOG("Shader pipeline ID %d is invalid", _id); \
                      error_return)

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * is_reserved_name:  Return whether the given string matches a uniform or
 * vertex attribute name defined by SIL.
 *
 * [Parameters]
 *     name: Name to check.
 * [Return value]
 *     True if the name is a reserved name, false if not.
 */
static int is_reserved_name(const char *name);

/*************************************************************************/
/**************** Interface: Generated shader management *****************/
/*************************************************************************/

int graphics_set_shader_generator(
    ShaderSourceCallback *vertex_source_callback,
    ShaderSourceCallback *fragment_source_callback,
    ShaderKeyCallback *key_callback, int hash_table_size, int dynamic_resize)
{
    if (UNLIKELY(vertex_source_callback
                 && (!fragment_source_callback || !key_callback))
     || UNLIKELY(!vertex_source_callback
                 && (fragment_source_callback || key_callback))
     || UNLIKELY(vertex_source_callback && hash_table_size < 0)
     || UNLIKELY(vertex_source_callback
                 && hash_table_size == 0 && !dynamic_resize)) {
        DLOG("Invalid parameters: %p %p %p %d %d", vertex_source_callback,
             fragment_source_callback, key_callback, hash_table_size,
             dynamic_resize);
        return 0;
    }

    return sys_graphics_set_shader_generator(
        vertex_source_callback, fragment_source_callback, key_callback,
        hash_table_size, dynamic_resize);
}

/*-----------------------------------------------------------------------*/

int graphics_add_shader_uniform(const char *name)
{
    if (UNLIKELY(!name) || UNLIKELY(!*name)) {
        DLOG("Invalid parameters: %p[%s]", name, name ? name : "");
        return 0;
    }

    if (is_reserved_name(name)) {
        DLOG("%s is a reserved name", name);
        return 0;
    }
    return sys_graphics_add_shader_uniform(name);
}

/*-----------------------------------------------------------------------*/

int graphics_add_shader_attribute(const char *name, int size)
{
    if (UNLIKELY(!name) || UNLIKELY(!*name)
     || UNLIKELY(size < 1) || UNLIKELY(size > 4)) {
        DLOG("Invalid parameters: %p[%s] %d", name, name ? name : "", size);
        return 0;
    }

    if (is_reserved_name(name)) {
        DLOG("%s is a reserved name", name);
        return 0;
    }
    const int attribute_id = sys_graphics_add_shader_attribute(name, size);
    if (attribute_id) {
        ASSERT(attribute_id > 0, return 0);
        ASSERT(attribute_id < 4096, return 0);
    }
    return attribute_id;
}

/*-----------------------------------------------------------------------*/

void graphics_set_shader_uniform_int(int uniform, int value)
{
    if (UNLIKELY(!uniform)) {
        DLOG("Invalid arguments: %d %d", uniform, value);
        return;
    }
    sys_graphics_set_shader_uniform_int(uniform, value);
}

/*-----------------------------------------------------------------------*/

void graphics_set_shader_uniform_float(int uniform, float value)
{
    if (UNLIKELY(!uniform)) {
        DLOG("Invalid arguments: %d %g", uniform, value);
        return;
    }
    sys_graphics_set_shader_uniform_float(uniform, value);
}

/*-----------------------------------------------------------------------*/

void graphics_set_shader_uniform_vec2(int uniform, const Vector2f *value)
{
    if (UNLIKELY(!uniform) || UNLIKELY(!value)) {
        DLOG("Invalid arguments: %d %p", uniform, value);
        return;
    }
    sys_graphics_set_shader_uniform_vec2(uniform, value);
}

/*-----------------------------------------------------------------------*/

void graphics_set_shader_uniform_vec3(int uniform, const Vector3f *value)
{
    if (UNLIKELY(!uniform) || UNLIKELY(!value)) {
        DLOG("Invalid arguments: %d %p", uniform, value);
        return;
    }
    sys_graphics_set_shader_uniform_vec3(uniform, value);
}

/*-----------------------------------------------------------------------*/

void graphics_set_shader_uniform_vec4(int uniform, const Vector4f *value)
{
    if (UNLIKELY(!uniform) || UNLIKELY(!value)) {
        DLOG("Invalid arguments: %d %p", uniform, value);
        return;
    }
    sys_graphics_set_shader_uniform_vec4(uniform, value);
}

/*-----------------------------------------------------------------------*/

void graphics_set_shader_uniform_mat4(int uniform, const Matrix4f *value)
{
    if (UNLIKELY(!uniform) || UNLIKELY(!value)) {
        DLOG("Invalid arguments: %d %p", uniform, value);
        return;
    }
    sys_graphics_set_shader_uniform_mat4(uniform, value);
}

/*************************************************************************/
/****************** Interface: Shader object management ******************/
/*************************************************************************/

int shader_background_compilation_supported(void)
{
    return sys_shader_background_compilation_supported();
}

/*-----------------------------------------------------------------------*/

void shader_enable_get_binary(int enable)
{
    sys_shader_enable_get_binary(enable);
}

/*-----------------------------------------------------------------------*/

int shader_max_attributes(void)
{
    return sys_shader_max_attributes();
}

/*-----------------------------------------------------------------------*/

int shader_set_attribute(int index, const char *name)
{
    const int limit = ubound(sys_shader_max_attributes(), 256);
    if (UNLIKELY(index < 0 || index >= limit)) {
        DLOG("Invalid parameters: %d %p[%s] (limit = %d)", index,
             name, name ? name : "", limit);
        return 0;
    }
    return sys_shader_set_attribute(index, name);
}

/*-----------------------------------------------------------------------*/

void shader_bind_standard_attribute(ShaderAttribute attribute, int index)
{
    int valid = 0;
    switch (attribute) {
      case SHADER_ATTRIBUTE_POSITION:
      case SHADER_ATTRIBUTE_TEXCOORD:
      case SHADER_ATTRIBUTE_COLOR:
        valid = 1;
        break;
    }
    if (UNLIKELY(!valid)) {
        DLOG("Invalid parameters: %d %d", attribute, index);
        return;
    }

    sys_shader_bind_standard_attribute(attribute, index);
}

/*-----------------------------------------------------------------------*/

void shader_clear_attributes(void)
{
    sys_shader_clear_attributes();
}

/*-----------------------------------------------------------------------*/

int shader_create_from_source(ShaderType type, const char *source, int length)
{
    int valid = 0;
    switch (type) {
      case SHADER_TYPE_VERTEX:
      case SHADER_TYPE_FRAGMENT:
        valid = 1;
        break;
    }
    if (UNLIKELY(!valid) || UNLIKELY(!source) || UNLIKELY(!length)
     || UNLIKELY(!*source)) {
        DLOG("Invalid parameters: %d %p[%.*s] %d", type, source,
             length < 0 ? INT_MAX : length, source ? source : "", length);
        return 0;
    }

    if (length < 0) {
        length = strlen(source);
    }
    SysShader *shader = sys_shader_create(type, source, length, 0);
    if (!shader) {
        return 0;
    }

    const int id = id_array_register(&shaders, shader);
    if (UNLIKELY(!id)) {
        DLOG("Failed to store new shader in array");
        sys_shader_destroy(shader);
        return 0;
    }

    return id;
}

/*-----------------------------------------------------------------------*/

int shader_create_from_binary(ShaderType type, const void *data, int size)
{
    int valid = 0;
    switch (type) {
      case SHADER_TYPE_VERTEX:
      case SHADER_TYPE_FRAGMENT:
        valid = 1;
        break;
    }
    if (UNLIKELY(!valid) || UNLIKELY(!data) || UNLIKELY(size <= 0)) {
        DLOG("Invalid parameters: %d %p %d", type, data, size);
        return 0;
    }

    SysShader *shader = sys_shader_create(type, data, size, 1);
    if (!shader) {
        return 0;
    }

    const int id = id_array_register(&shaders, shader);
    if (UNLIKELY(!id)) {
        DLOG("Failed to store new shader in array");
        sys_shader_destroy(shader);
        return 0;
    }

    return id;
}

/*-----------------------------------------------------------------------*/

void shader_destroy(int shader_id)
{
    if (shader_id) {
        SysShader *shader;
        VALIDATE_SHADER(shader_id, shader, return);
        sys_shader_destroy(shader);
        id_array_release(&shaders, shader_id);
    }
}

/*-----------------------------------------------------------------------*/

void *shader_get_binary(int shader_id, int *size_ret)
{
    SysShader *shader;
    VALIDATE_SHADER(shader_id, shader, return NULL);
    if (UNLIKELY(!size_ret)) {
        DLOG("Invalid parameters: %d %p", shader_id, size_ret);
        return NULL;
    }

    return sys_shader_get_binary(shader, size_ret);
}

/*-----------------------------------------------------------------------*/

void *shader_compile_to_binary(ShaderType type, const char *source,
                               int length, int *size_ret)
{
    int valid = 0;
    switch (type) {
      case SHADER_TYPE_VERTEX:
      case SHADER_TYPE_FRAGMENT:
        valid = 1;
        break;
    }
    if (UNLIKELY(!valid) || UNLIKELY(!source) || UNLIKELY(!length)
     || UNLIKELY(!*source) || UNLIKELY(!size_ret)) {
        DLOG("Invalid parameters: %d %p[%.*s] %d %p", type, source,
             length < 0 ? INT_MAX : length, source ? source : "", length,
             size_ret);
        return 0;
    }

    if (length < 0) {
        length = strlen(source);
    }
    return sys_shader_compile(type, source, length, size_ret);
}

/*-----------------------------------------------------------------------*/

int shader_get_uniform_id(int shader_id, const char *name)
{
    SysShader *shader;
    VALIDATE_SHADER(shader_id, shader, return 0);
    if (UNLIKELY(!name) || UNLIKELY(!*name)) {
        DLOG("Invalid parameters: %d %p[%s]", shader_id,
             name, name ? name : "");
        return 0;
    }

    return sys_shader_get_uniform_id(shader, name);
}

/*-----------------------------------------------------------------------*/

void shader_set_uniform_int(int shader_id, int uniform, int value)
{
    SysShader *shader;
    VALIDATE_SHADER(shader_id, shader, return);
    if (UNLIKELY(!uniform)) {
        DLOG("Invalid parameters: %d %d %d", shader_id, uniform, value);
        return;
    }

    sys_shader_set_uniform_int(shader, uniform, value);
}

/*-----------------------------------------------------------------------*/

extern void shader_set_uniform_float(int shader_id, int uniform, float value)
{
    SysShader *shader;
    VALIDATE_SHADER(shader_id, shader, return);
    if (UNLIKELY(!uniform)) {
        DLOG("Invalid parameters: %d %d %g", shader_id, uniform, value);
        return;
    }

    sys_shader_set_uniform_float(shader, uniform, value);
}

/*-----------------------------------------------------------------------*/

extern void shader_set_uniform_vec2(
    int shader_id, int uniform, const struct Vector2f *value)
{
    SysShader *shader;
    VALIDATE_SHADER(shader_id, shader, return);
    if (UNLIKELY(!uniform) || UNLIKELY(!value)) {
        DLOG("Invalid parameters: %d %d %p", shader_id, uniform, value);
        return;
    }

    sys_shader_set_uniform_vec2(shader, uniform, value);
}

/*-----------------------------------------------------------------------*/

extern void shader_set_uniform_vec3(
    int shader_id, int uniform, const struct Vector3f *value)
{
    SysShader *shader;
    VALIDATE_SHADER(shader_id, shader, return);
    if (UNLIKELY(!uniform) || UNLIKELY(!value)) {
        DLOG("Invalid parameters: %d %d %p", shader_id, uniform, value);
        return;
    }

    sys_shader_set_uniform_vec3(shader, uniform, value);
}

/*-----------------------------------------------------------------------*/

extern void shader_set_uniform_vec4(
    int shader_id, int uniform, const struct Vector4f *value)
{
    SysShader *shader;
    VALIDATE_SHADER(shader_id, shader, return);
    if (UNLIKELY(!uniform) || UNLIKELY(!value)) {
        DLOG("Invalid parameters: %d %d %p", shader_id, uniform, value);
        return;
    }

    sys_shader_set_uniform_vec4(shader, uniform, value);
}

/*-----------------------------------------------------------------------*/

extern void shader_set_uniform_mat4(
    int shader_id, int uniform, const struct Matrix4f *value)
{
    SysShader *shader;
    VALIDATE_SHADER(shader_id, shader, return);
    if (UNLIKELY(!uniform) || UNLIKELY(!value)) {
        DLOG("Invalid parameters: %d %d %p", shader_id, uniform, value);
        return;
    }

    sys_shader_set_uniform_mat4(shader, uniform, value);
}

/*************************************************************************/
/***************** Interface: Shader pipeline management *****************/
/*************************************************************************/

int shader_pipeline_create(int vertex_shader_id, int fragment_shader_id)
{
    SysShader *vertex_shader, *fragment_shader;
    VALIDATE_SHADER(vertex_shader_id, vertex_shader, return 0);
    VALIDATE_SHADER(fragment_shader_id, fragment_shader, return 0);

    SysShaderPipeline *pipeline =
        sys_shader_pipeline_create(vertex_shader, fragment_shader);
    if (!pipeline) {
        return 0;
    }

    const int id = id_array_register(&shader_pipelines, pipeline);
    if (UNLIKELY(!id)) {
        DLOG("Failed to store new shader pipeline in array");
        sys_shader_pipeline_destroy(pipeline);
        return 0;
    }

    return id;
}

/*-----------------------------------------------------------------------*/

void shader_pipeline_destroy(int pipeline_id)
{
    if (pipeline_id) {
        SysShaderPipeline *pipeline;
        VALIDATE_SHADER_PIPELINE(pipeline_id, pipeline, return);
        sys_shader_pipeline_destroy(pipeline);
        id_array_release(&shader_pipelines, pipeline_id);
    }
}

/*-----------------------------------------------------------------------*/

void shader_pipeline_apply(int pipeline_id)
{
    SysShaderPipeline *pipeline;
    if (pipeline_id) {
        VALIDATE_SHADER_PIPELINE(pipeline_id, pipeline, return);
    } else {
        pipeline = NULL;
    }
    sys_shader_pipeline_apply(pipeline);
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static int is_reserved_name(const char *name)
{
    PRECOND(name != NULL, return 0);

    static const char * const RESERVED_NAMES[] = {
        "transform",
        "tex",
        "tex_offset",
        "fixed_color",
        "fog_params",
        "fog_color",
        "alpha_ref",
        "position",
        "texcoord",
        "color",
    };

    for (int i = 0; i < lenof(RESERVED_NAMES); i++) {
        if (strcmp(name, RESERVED_NAMES[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

/*************************************************************************/
/*************************************************************************/
