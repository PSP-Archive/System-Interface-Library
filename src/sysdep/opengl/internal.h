/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/opengl/internal.h: Data types, constants, and utility
 * functions exported for use by other OpenGL source files.  These should
 * not be used externally.
 */

#ifndef SIL_SRC_SYSDEP_OPENGL_INTERNAL_H
#define SIL_SRC_SYSDEP_OPENGL_INTERNAL_H

#include "src/graphics.h"
#include "src/math.h"
#include "src/shader.h"
#include "src/sysdep/opengl/gl-headers.h"
#include "src/sysdep/opengl/opengl.h"

/*************************************************************************/
/******************* Internal-use constants and macros *******************/
/*************************************************************************/

/* Array expansion increment for the delayed-delete buffer. */
#define OPENGL_DELETE_INFO_EXPAND  100

/* Function and type renamers for sys_* functions and Sys* types (see the
 * SIL_OPENGL_NO_SYS_FUNCS documentation in include/SIL/base.h). */
#ifdef SIL_OPENGL_NO_SYS_FUNCS
typedef struct OpenGLSysFramebuffer     OpenGLSysFramebuffer;
typedef struct OpenGLSysPrimitive       OpenGLSysPrimitive;
typedef struct OpenGLSysShader          OpenGLSysShader;
typedef struct OpenGLSysShaderPipeline  OpenGLSysShaderPipeline;
typedef struct OpenGLSysTexture         OpenGLSysTexture;
# define SysFramebuffer     OpenGLSysFramebuffer
# define SysPrimitive       OpenGLSysPrimitive
# define SysShader          OpenGLSysShader
# define SysShaderPipeline  OpenGLSysShaderPipeline
# define SysTexture         OpenGLSysTexture
# define sys_(name)  opengl_sys_##name
#else
# define sys_(name)  sys_##name
#endif

/*************************************************************************/
/****************** Data structures used by OpenGL code ******************/
/*************************************************************************/

/* Texture color types. */
enum {
    TEXCOLOR_RGBA = 1,
    TEXCOLOR_RGB,
    TEXCOLOR_A,  // Loaded as GL_RED if shaders are in use.
    TEXCOLOR_L,  // Loaded as GL_RED if shaders are in use.
};

/* Texture data structure. */

struct SysTexture {

    /* OpenGL device generation for this texture. */
    unsigned int generation;

    /* OpenGL texture ID for this texture. */
    GLuint id;

    /* Size of this texture, in pixels. */
    int width, height;

    /* Color type of pixel data (TEXCOLOR_*). */
    uint8_t color_type;

    /* Should we automatically generate mipmaps for this texture? */
    uint8_t auto_mipmaps;

    /* Does this texture currently have mipmaps stored? */
    uint8_t has_mipmaps;

    /* Texture coordinate repeat and antialiasing flags. */
    uint8_t repeat_u, repeat_v;
    uint8_t antialias;

    /* Is the texture empty (newly created and not yet modified)? */
    uint8_t empty;

    /* Is the pixel data readable by attaching the texture to a framebuffer?
     * If false and glGetTexImage() is not available, attempts to lock
     * compressed textures for anything except a complete overwrite
     * (SYS_TEXTURE_LOCK_DISCARD) will fail. */
    uint8_t readable;

    /* Pixel data buffer returned from the last sys_texture_lock(), or
     * NULL if not locked. */
    void *lock_buf;

    /* Lock mode used with the last sys_texture_lock().  Only valid when
     * lock_buf != NULL. */
    SysTextureLockMode lock_mode;

};

/*-----------------------------------------------------------------------*/

/* Framebuffer data structure. */

struct SysFramebuffer {

    /* OpenGL device generation for this framebuffer. */
    unsigned int generation;

    /* OpenGL framebuffer ID for this framebuffer. */
    GLuint framebuffer;

    /* OpenGL depth buffer (renderbuffer) ID for this framebuffer. */
    GLuint depth_buffer;

    /* OpenGL stencil buffer (renderbuffer) ID for this framebuffer.  Only
     * valid if separate_stencil is true. */
    GLuint stencil_buffer;

    /* SysTexture structure for applying the framebuffer as a texture. */
    SysTexture texture;

    /* Size of this framebuffer, in pixels. */
    int width, height;

    /* Depth and stencil buffer formats for this framebuffer. */
    GLenum depth_format;
    GLenum stencil_format;  // Only valid if separate_stencil is true.

    /* Stencil buffer presence flag. */
    uint8_t has_stencil;
    /* Separate depth/stencil buffer flag: true = separate, false = packed.
     * Always false if has_stencil is false. */
    uint8_t separate_stencil;

};

/*-----------------------------------------------------------------------*/

/* Graphics primitive data structure. */

struct SysPrimitive {

    /* OpenGL device generation for this primitive. */
    unsigned int generation;

    /* Primitive type (GL_TRIANGLES, etc.). */
    GLenum type;

    /* Is this a high-level QUADS or QUAD_STRIP primitive which was
     * converted to GL_TRIANGLES or GL_TRIANGLE_STRIP? */
    uint8_t converted_quads;

    /* Does this primitive have a separate index list? */
    uint8_t has_indices;

    /* Is this primitive using an immediate-mode vertex buffer? */
    uint8_t is_immediate_vbo;

    /* Was this primitive using the shared single-quad index buffer? */
    uint8_t is_single_quad;

    /* Are vertex_data and index_data locally-allocated buffers, which
     * should be freed when the primitive is destroyed? */
    uint8_t vertex_local, index_local;

    /* Has the vertex array object been configured?  (If no vertex array is
     * in use, this is always false.) */
    uint8_t vao_configured;

    /* Vertex buffer object ID, or zero if this primitive has no associated
     * vertex buffer. */
    GLuint vertex_buffer;

    /* Index buffer object ID, or zero if this primitive has no associated
     * index buffer. */
    GLuint index_buffer;

    /* Vertex array object ID, used if the MANDATORY_VAO feature is set. */
    GLuint vertex_array;

    /* Local vertex data buffer.  Always NULL if vertex_buffer != 0. */
    uint8_t *vertex_data;

    /* Vertex data structure size (in bytes per vertex) and count. */
    int vertex_size, vertex_count;

    /* Vertex position information. */
    GLint position_size;        // Number of components (2, 3, or 4).
    GLenum position_type;       // GL_* data type.
    GLsizei position_offset;    // Byte offset from base of array.

    /* Texture coordinate information. */
    GLint texcoord_size;        // 0 if texture coordinates are not used.
    GLenum texcoord_type;
    GLsizei texcoord_offset;

    /* Color information. */
    GLint color_size;
    GLenum color_type;
    GLsizei color_offset;

    /* Custom attribute information.  Used for both generated shaders and
     * shader objects. */
    int num_user_attribs;
    uint32_t *user_attribs;  // 32-bit vertex format entries.

    /* Local index data buffer.  Always NULL if index_buffer != 0, and
     * also NULL if the primitive has no indices. */
    uint8_t *index_data;

    /* Index data size (in bytes: 1, 2, or 4) and count.  These values are
     * both set to zero if the primitive has no indices. */
    int index_size, index_count;

    /* GL data type of index data. */
    GLenum index_type;

};

/*-----------------------------------------------------------------------*/

/* Shader data structure. */

struct ShaderUniform;  // Defined in shader.c.

struct SysShader {

    /* OpenGL device generation for this shader. */
    unsigned int generation;

    /* OpenGL shader object or program. */
    GLuint shader;

    /* Is this an ARB_separate_shader_objects separable program (true) or
     * a shader object (false)? */
    uint8_t is_program;

    /* Shader type (SHADER_TYPE_*). */
    ShaderType type;

    /* Linked lists of shader pipelines with which this shader is
     * associated.  This field also serves as the head of the list. */
    SysShaderPipeline *pipelines;

    /* Number of attributes used by the shader, if a vertex shader (0
     * otherwise). */
    int num_attributes;

    /**** The fields below are only used for non-program shader objects  ****
     **** (for which is_program is false), and are all 0/NULL otherwise. ****/

    /* Attribute bindings for vertex shaders.  Allocated as a single
     * buffer, with the strings stored after the array. */
    const char **attributes;

    /* Index bindings for standard vertex attributes, -1 if not bound. */
    int standard_attributes[SHADER_ATTRIBUTE__NUM];

    /* List of uniforms seen so far. */
    int num_uniforms;
    struct ShaderUniform *uniforms;

    /* Name and value buffer for uniforms.  This is expanded as needed
     * when adding uniforms or setting their values. */
    void *uniform_data;
    int uniform_data_size;

    /* Uniform data generation number.  Used to detect when linked programs
     * need their uniforms updated. */
    uint32_t uniform_generation;
};

/*-----------------------------------------------------------------------*/

/* Shader pipeline data structure. */

struct SysShaderPipeline {

    /* OpenGL device generation for this shader pipeline. */
    unsigned int generation;

    /* Shader program or program pipeline. */
    GLuint program;

    /* Is this an ARB_separate_shader_objects program pipeline (true) or a
     * linked program (false)? */
    uint8_t is_pipeline;

    /* Number of vertex shader inputs (attributes). */
    uint16_t num_inputs;

    /* Linked list pointers for associating vertex and fragment shaders
     * with shader pipelines.  prev_ptr points to the appropriate "next"
     * field in the previous pipeline in the list, or the "pipelines" field
     * of the shader object if this is the first entry in the list. */
    SysShaderPipeline *vertex_next, **vertex_prev_ptr;
    SysShaderPipeline *fragment_next, **fragment_prev_ptr;

    /* Pointers to the vertex and fragment shaders themselves (only used
     * if !is_pipeline). */
    SysShader *vertex_shader, *fragment_shader;

    /* Generation numbers for vertex and fragment shaders (only used if
     * !is_pipeline). */
    uint32_t vertex_generation, fragment_generation;

};

/*-----------------------------------------------------------------------*/

/* Standard uniform indices for generated shaders (used with
 * opengl_set_uniform_*()). */
enum {
    UNIFORM_TRANSFORM = 0,  // mat4 transform;
    UNIFORM_TEXTURE,        // sampler texture;
    UNIFORM_TEX_OFFSET,     // vec2 tex_offset;
    UNIFORM_FIXED_COLOR,    // vec4 fixed_color;
    UNIFORM_FOG_PARAMS,     // vec2 fog_params;
    UNIFORM_FOG_TRANSFORM,  // vec4 fog_transform;
    UNIFORM_FOG_COLOR,      // vec4 fog_color;
    UNIFORM_ALPHA_REF,      // float alpha_ref;
    UNIFORM_POINT_SIZE,     // float point_size;
    UNIFORM__NUM            // Total number of standard uniforms.
};

/*************************************************************************/
/*************** Library-internal shared data and routines ***************/
/*************************************************************************/

/******** framebuffer.c ********/

/**
 * opengl_current_framebuffer:  Return the framebuffer currently bound as
 * the rendering target, or NULL if the default framebuffer is the current
 * rendering target.
 *
 * [Return value]
 *     Currently bound framebuffer, or NULL if none.
 */
extern SysFramebuffer *opengl_current_framebuffer(void);


/******** graphics.c ********/

/**
 * opengl_device_generation:  The current OpenGL device generation number.
 * GL resources associated with objects that have a different generation
 * number are invalid.
 */
extern unsigned int opengl_device_generation;

/**
 * opengl_window_width, opengl_window_height:  Width and height of the
 * OpenGL output window, in pixels.
 */
extern int opengl_window_width, opengl_window_height;

/**
 * opengl_can_ensure_compile_context:  Return whether a callback function
 * has been supplied for setting up shader compilation GL contexts (via
 * the exported function opengl_set_compile_context_callback()).
 *
 * [Return value]
 *     True if shader compilation GL contexts can be created, false if not.
 */
extern int opengl_can_ensure_compile_context(void);

/**
 * opengl_ensure_compile_context:  Ensure that the current thread has an
 * active GL context suitable for shader compilation.
 *
 * [Return value]
 *     True on success, false if a context could not be created.
 */
extern int opengl_ensure_compile_context(void);

/**
 * opengl_delete_*:  Delete the given GL object.  The type of object is
 * identified by the function name; for example, opengl_delete_texture()
 * deletes texture objects.  If the DELAYED_DELETE feature is in effect,
 * the object is not deleted immediately, but is instead added to a list
 * for deletion at the next opengl_free_dead_resources() call.
 *
 * [Parameters]
 *     object: GL object to delete.
 */
extern void opengl_delete_buffer(GLuint object);
extern void opengl_delete_framebuffer(GLuint object);
extern void opengl_delete_program(GLuint object);
extern void opengl_delete_program_pipeline(GLuint object);
extern void opengl_delete_renderbuffer(GLuint object);
extern void opengl_delete_shader(GLuint object);
extern void opengl_delete_texture(GLuint object);
extern void opengl_delete_vertex_array(GLuint object);


/******** primitive.c ********/

/**
 * opengl_primitive_reset_bindings:  Reset the vertex and index buffer
 * bindings, ensuring that the cached values are in sync with the GL state.
 * Called at the beginning of each frame to avoid glitches resulting from
 * GL operations performed by system libraries.
 *
 * This routine does nothing if vertex buffers are not enabled (i.e.,
 * neither of the OPENGL_FEATURE_FAST_{STATIC,DYNAMIC}_VBO flags are set).
 */
extern void opengl_primitive_reset_bindings(void);

/**
 * opengl_primitive_cleanup:  Clear all static buffer objects for
 * immediate-mode primitives.  Called by opengl_cleanup().
 */
extern void opengl_primitive_cleanup(void);


/******** shader.c ********/

/**
 * opengl_shader_init:  Initialize internal shader object management state.
 */
extern void opengl_shader_init(void);

/**
 * opengl_shader_num_attributes:  Return the number of vertex attributes
 * used by the currently active shader pipeline.  Only meaningful when
 * shader objects are enabled.
 *
 * [Return value]
 *     Number of attributes used by the currently active shader pipeline,
 *     or 0 if no shader pipeline is active.
 */
extern int opengl_shader_num_attributes(void);

/**
 * opengl_shader_standard_attribute_binding:  Return the attribute index
 * to which the given standard attribute is bound for the currently active
 * shader pipeline, or -1 if the standard attribute is unbound.
 *
 * [Parameters]
 *     attribute: Standard attribute to look up (SHADER_ATTRIBUTE_*).
 * [Return value]
 *     Vertex shader attribute index, or -1 if the standard attrbiute is
 *     unbound.
 */
extern int opengl_shader_standard_attribute_binding(ShaderAttribute attribute);


/******** shader-gen.c ********/

/**
 * opengl_select_shader:  Activate the appropriate shader for the vertex
 * format used by the given primitive, first creating the shader if
 * necessary.  Called by sys_graphics_draw_primitive().
 *
 * [Parameters]
 *     primitive: Primitive from which to take vertex format.
 *     texture: Texture to use for rendering, or NULL if none.
 *     tex_offset: True to apply the texture offset.
 *     color_uniform: True to apply the fixed color parameter.
 *     fog: True to apply fog.
 *     alpha_test: True to perform alpha testing.
 *     alpha_comparison: Alpha test comparison type (GRAPHICS_COMPARISON_*).
 * [Return value]
 *     1 if the active shader was changed, 0 if not, -1 if an error occurred.
 */
extern int opengl_select_shader(const SysPrimitive *primitive,
                                const SysTexture *texture, int tex_offset,
                                int color_uniform, int fog, int alpha_test,
                                GraphicsComparisonType alpha_comparison);

/**
 * opengl_deselect_shader:  Deactivate any currently active shader.
 */
extern void opengl_deselect_shader(void);

/**
 * opengl_set_uniform_*:  Set the value of a standard uniform parameter to
 * a shader.  No type-checking is performed; undefined behavior will result
 * from attempting to load a value of incorrect type into a uniform
 * parameter (for example, trying to load a vector into a matrix).
 *
 * [Parameters]
 *     uniform: ID of uniform parameter to set (UNIFORM_*).
 *     value: Value to set (type depends on function).
 */
extern void opengl_set_uniform_int(int uniform, int value);
extern void opengl_set_uniform_float(int uniform, float value);
extern void opengl_set_uniform_vec2(int uniform, const Vector2f *value);
extern void opengl_set_uniform_vec4(int uniform, const Vector4f *value);
extern void opengl_set_uniform_mat4(int uniform, const Matrix4f *value);

/**
 * opengl_get_user_attrib_sizes:  Return an array of the sizes (number of
 * float components) of each user-specified vertex attribute.  The first
 * entry in the array maps to user attribute ID 1 and OpenGL vertex
 * attribute SHADER_ATTRIBUTE__NUM.
 *
 * [Parameters]
 *     num_user_attribs_ret: Pointer to variable to receive the number of
 *         entries in the array.
 * [Return value]
 *     Array containing user-specified vertex attribute sizes.
 */
extern const int8_t *opengl_get_user_attrib_sizes(int *num_user_attribs_ret);

/**
 * opengl_clear_generated_shaders:  Clear all generated shader objects.
 * Called by opengl_cleanup() (and also internally).
 */
extern void opengl_clear_generated_shaders(void);


/******** state.c ********/

/**
 * opengl_current_texture:  Pointer to the currently active texture.
 */
extern SysTexture *opengl_current_texture;

/**
 * opengl_current_texture_id:  Texture ID currently bound to GL_TEXTURE_2D,
 * or zero if no texture is bound.
 */
extern GLuint opengl_current_texture_id;

/**
 * opengl_primitive_color:  Color parameter to apply to primitives.
 */
extern Vector4f opengl_primitive_color;

/**
 * opengl_primitive_color_used:  Flag indicating whether the current
 * primitive color is not the identity and should therefore be applied.
 * This is merely a shortcut to avoid needing to compare the entire vector
 * against (1,1,1,1).
 */
extern uint8_t opengl_primitive_color_used;

/**
 * opengl_framebuffer_changed:  Flag indicating whether the framebuffer set
 * as the render target has been changed.
 */
extern uint8_t opengl_framebuffer_changed;

/**
 * opengl_shader_objects_enabled:  Flag indicating whether shader objects
 * (true) or generated shaders / fixed-function pipeline (false) are to be
 * used for primitive rendering.  This is never set to true if the SHADERS
 * feature is not available.
 */
extern uint8_t opengl_shader_objects_enabled;

/*----------------------------------*/

/**
 * opengl_state_init:  Initialize internal state data for the OpenGL
 * interface.  Called by opengl_init().
 */
extern void opengl_state_init(void);

/**
 * opengl_bind_texture:  Bind the given texture to the given target on
 * texture unit 0.  Any currently bound texture (on any target) is unbound.
 *
 * For other texture units, call glBindTextureUnit() directly.
 *
 * [Parameters]
 *     target: OpenGL texture target (e.g., GL_TEXTURE_2D).
 *     id: OpenGL texture ID.
 */
extern void opengl_bind_texture(GLenum target, GLuint id);

/**
 * opengl_apply_viewport:  Apply the current viewport parameters.
 * Implements the OpenGL side of sys_graphics_set_viewport(), and is also
 * called on framebuffer change.
 */
extern void opengl_apply_viewport(void);

/**
 * opengl_apply_clip_region:  Apply the current clipping region parameters.
 * Implements the OpenGL side of sys_graphics_set_clip_region(), and is
 * also called on framebuffer change.
 */
extern void opengl_apply_clip_region(void);

/**
 * opengl_apply_matrices:  Apply the current coordinate transformation
 * matrices.  Called by sys_graphics_draw_primitive().
 *
 * [Parameters]
 *     force: True to reapply all matrices regardless of current state;
 *         false to only apply changed matrices.
 */
extern void opengl_apply_matrices(int force);

/**
 * opengl_apply_shader:  Select an appropriate shader for rendering the
 * given primitive based on the current rendering state.  Called by
 * sys_graphics_draw_primitive() if shaders are enabled.
 *
 * [Parameters]
 *     primitive: Primitive to be rendered.
 * [Return value]
 *     True on success, false on error.
 */
extern int opengl_apply_shader(SysPrimitive *primitive);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_OPENGL_INTERNAL_H
