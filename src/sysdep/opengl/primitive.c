/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/opengl/primitive.c: Primitive rendering functionality for
 * OpenGL-based platforms.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/graphics.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/opengl/dyngl.h"
#include "src/sysdep/opengl/internal.h"
#include "src/sysdep/opengl/opengl.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Static index buffer used for rendering a single quad on systems without
 * native quad support (generated only when needed). */
static GLuint single_quad_index_buffer;

/* Indices for the single quad buffer. */
static const uint16_t single_quad_indices[] = {0,1,3,3,1,2};

/* Vertex buffers used for immediate-mode primitives. */
static GLuint immediate_vertex_buffers[SIL_OPENGL_IMMEDIATE_VERTEX_BUFFERS];

/* Current buffer sizes of each buffer object, in bytes. */
static int immediate_vbo_sizes[lenof(immediate_vertex_buffers)];

/* Index of next immediate-mode vertex buffer to use. */
static int next_immediate_vbo = 0;

/* Current buffer objects bound to the GL vertex and index buffers (used
 * to avoid unnecessary glBindBuffer() calls). */
static GLuint current_vertex_buffer = 0;
static GLuint current_index_buffer = 0;

/* Current vertex attribute enable states. */
static uint8_t vertex_attrib_enabled[SHADER_ATTRIBUTE__NUM];

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * bind_vertex_buffer, bind_index_buffer:  Wrappers for glBindBuffer() on
 * GL_ARRAY_BUFFER and GL_ELEMENT_ARRAY_BUFFER, respectively, which ensure
 * that the appropriate current_*_buffer shadow variable is updated and
 * skip the glBindBuffer() call if it would have no effect.
 *
 * [Parameters]
 *     buffer: GL buffer object to bind.
 */
static inline void bind_vertex_buffer(const GLuint buffer);
static inline void bind_index_buffer(const GLuint buffer);

/**
 * enable_vertex_attrib, disable_vertex_attrib:  Wrappers for
 * gl{Enable,Disable}VertexAttribArray() which avoid calls that would
 * not cause a state change.  These functions should only be used when
 * modifying global GL state, not when a vertex array object is bound.
 *
 * [Parameters]
 *     attrib: Vertex attribute to enable or disable (SHADER_ATTRIBUTE_*).
 */
static inline void enable_vertex_attrib(int attrib);
static inline void disable_vertex_attrib(int attrib);

/**
 * load_primitive_data:  Load the data for a primitive into GL_ARRAY_BUFFER
 * (and GL_ELEMENT_ARRAY_BUFFER, if appropriate).  The index data parameters
 * are ignored if primitive->has_indices == 0.
 *
 * [Parameters]
 *     primitive: Primitive for which data is to be loaded.
 *     vertex_size: Total size of vertex data, in bytes.
 *     vertex_data: Pointer to vertex data.
 *     index_size: Total size of index data, in bytes.
 *     index_data: Pointer to index data.
 *     immediate_vbo_index: Index into immediate_vertex_buffers[] of the
 *         vertex buffer for immediate-mode primitives.
 * [Return value]
 *     True on success, false on error.
 */
static int load_primitive_data(
    SysPrimitive *primitive, int vertex_size, const void *vertex_data,
    int index_size, const void *index_data, int immediate_vbo_index);

/**
 * configure_standard_shader_attribute:  Configure one of the standard
 * vertex attributes for a shader.
 *
 * [Parameters]
 *     primitive: Primitive for which to configure.
 *     attribute: Attribute ID to configure (SHADER_ATTRIBUTE_*).
 *     size: Number of components in the attribute value.
 *     type: Type of the attribute value (GL_FLOAT, etc).
 *     normalized: GL_TRUE to normalize integer values, GL_FALSE to leave
 *         them as is.
 *     pointer: Data pointer or offset into buffer object for attribute data.
 * [Return value]
 *     Attribute index used for the attribute, or -1 if none.
 */
static int configure_standard_shader_attribute(
    const SysPrimitive *primitive, ShaderAttribute attribute, int size,
    GLenum type, GLboolean normalized, const void *pointer);

/**
 * configure_shader_attributes:  Configure shader attributes in preparation
 * for drawing a primitive.
 *
 * [Parameters]
 *     primitive: Primitive for which to configure attributes.
 *     vertex_base: Base address for glVertexAttribPointer() calls.
 */
static void configure_shader_attributes(const SysPrimitive *primitive,
                                        const uint8_t *vertex_base);

/*************************************************************************/
/************** Interface: Primitive creation and rendering **************/
/*************************************************************************/

SysPrimitive *sys_(graphics_create_primitive)(
    enum GraphicsPrimitiveType type, const void *data, const uint32_t *format,
    int size, int count, const void *index_data, int index_size,
    int index_count, int immediate)
{
    int need_quad_indices = 0;    // Set true if we need to generate indices.
    void *quad_index_data = NULL; // Freed (if not NULL) on return.


    GLenum gl_type = GL_INVALID_ENUM;
    int converted_quads = 0;
    switch (type) {
      case GRAPHICS_PRIMITIVE_POINTS:
        gl_type = GL_POINTS;
        break;
      case GRAPHICS_PRIMITIVE_LINES:
        gl_type = GL_LINES;
        break;
      case GRAPHICS_PRIMITIVE_LINE_STRIP:
        gl_type = GL_LINE_STRIP;
        break;
      case GRAPHICS_PRIMITIVE_TRIANGLES:
        gl_type = GL_TRIANGLES;
        break;
      case GRAPHICS_PRIMITIVE_TRIANGLE_STRIP:
        gl_type = GL_TRIANGLE_STRIP;
        break;
      case GRAPHICS_PRIMITIVE_QUADS:
        if (opengl_has_features(OPENGL_FEATURE_NATIVE_QUADS)) {
            gl_type = GL_QUADS;
        } else {
            gl_type = GL_TRIANGLES;
            converted_quads = 1;
            if ((index_data ? index_count : count) >= 4) {
                need_quad_indices = 1;
            } else {
                /* Continue constructing a primitive object for consistent
                 * behavior, but make sure we don't try to render a single
                 * triangle if we got passed 3 vertices. */
                if (index_data) {
                    index_count = 1;
                } else {
                    count = 1;
                }
            }
        }
        break;
      case GRAPHICS_PRIMITIVE_QUAD_STRIP:
        if (opengl_has_features(OPENGL_FEATURE_NATIVE_QUADS)) {
            gl_type = GL_QUAD_STRIP;
        } else {
            gl_type = GL_TRIANGLE_STRIP;
            converted_quads = 1;
            if ((index_data ? index_count : count) < 4) {
                if (index_data) {
                    index_count = 1;
                } else {
                    count = 1;
                }
            } else {
                /* In addition to the above check, make sure we don't draw
                 * half a quad if the vertex count is odd. */
                if (index_data) {
                    index_count &= ~1;
                } else {
                    count &= ~1;
                }
            }
        }
        break;
    }
    ASSERT(gl_type != GL_INVALID_ENUM, goto error_return);

    SysPrimitive *primitive = mem_alloc(sizeof(*primitive), 0, 0);
    if (UNLIKELY(!primitive)) {
        DLOG("No memory for SysPrimitive structure");
        goto error_return;
    }

    primitive->generation       = opengl_device_generation;
    primitive->type             = gl_type;
    primitive->converted_quads  = converted_quads;
    primitive->has_indices      = (index_data != NULL);
    primitive->is_immediate_vbo = 0;
    primitive->is_single_quad   = 0;
    primitive->vertex_buffer    = 0;
    primitive->vertex_data      = NULL;
    primitive->vertex_size      = size;
    primitive->vertex_count     = count;
    primitive->vertex_local     = 0;
    primitive->index_buffer     = 0;
    primitive->index_data       = NULL;
    primitive->index_size       = index_data ? index_size : 0;
    primitive->index_count      = index_data ? index_count : 0;
    primitive->index_local      = 0;
    primitive->position_size    = 0;
    primitive->texcoord_size    = 0;
    primitive->color_size       = 0;
    primitive->vertex_array     = 0;
    primitive->vao_configured   = 0;

    int user_attribs_used = 0;
    for (int i = 0; format[i] != 0; i++) {
        const int format_type = GRAPHICS_VERTEX_FORMAT_TYPE(format[i]);
        if (GRAPHICS_VERTEX_TYPE_IS_USER(format_type)
         || GRAPHICS_VERTEX_TYPE_IS_ATTRIB(format_type)) {
            user_attribs_used++;
        }
    }
    primitive->num_user_attribs = user_attribs_used;
    if (user_attribs_used > 0) {
        primitive->user_attribs = mem_alloc(
            sizeof(*primitive->user_attribs) * user_attribs_used, 0, 0);
        if (UNLIKELY(!primitive->user_attribs)) {
            DLOG("No memory for user attribute list (%d entries)",
                 user_attribs_used);
            goto error_free_primitive;
        }
    } else {
        primitive->user_attribs = NULL;
    }

    int user_attrib_index = 0;
    for (int i = 0; format[i] != 0; i++) {
        const int format_type = GRAPHICS_VERTEX_FORMAT_TYPE(format[i]);
        const int offset = GRAPHICS_VERTEX_FORMAT_OFFSET(format[i]);

        if (GRAPHICS_VERTEX_TYPE_IS_USER(format_type)
         || GRAPHICS_VERTEX_TYPE_IS_ATTRIB(format_type)) {
            ASSERT(user_attrib_index < primitive->num_user_attribs,
                   goto error_free_primitive);
            if (GRAPHICS_VERTEX_TYPE_IS_ATTRIB(format_type)) {
                const GraphicsVertexDataType data_type =
                    GRAPHICS_VERTEX_ATTRIB_TYPE(format_type);
                if (data_type == GRAPHICS_VERTEXDATA_UB
                 || data_type == GRAPHICS_VERTEXDATA_S
                 || data_type == GRAPHICS_VERTEXDATA_I) {
                    if (UNLIKELY(!opengl_has_features(
                                     OPENGL_FEATURE_VERTEX_ATTRIB_INT))) {
                        DLOG("Attempt to use integer vertex attributes on"
                             " OpenGL/ES 2.x, discarding primitive");
                        goto error_free_primitive;
                    }
                }
            }
            primitive->user_attribs[user_attrib_index++] = format[i];
        } else {
            int ok = 0;

            switch ((GraphicsVertexFormatType)format_type) {
              case GRAPHICS_VERTEX_POSITION_2S:
                ok = 1;
                primitive->position_size = 2;
                primitive->position_type = GL_SHORT;
                primitive->position_offset = offset;
                break;
              case GRAPHICS_VERTEX_POSITION_2F:
                ok = 1;
                primitive->position_size = 2;
                primitive->position_type = GL_FLOAT;
                primitive->position_offset = offset;
                break;
              case GRAPHICS_VERTEX_POSITION_3F:
                ok = 1;
                primitive->position_size = 3;
                primitive->position_type = GL_FLOAT;
                primitive->position_offset = offset;
                break;
              case GRAPHICS_VERTEX_POSITION_4F:
                ok = 1;
                primitive->position_size = 4;
                primitive->position_type = GL_FLOAT;
                primitive->position_offset = offset;
                break;

              case GRAPHICS_VERTEX_TEXCOORD_2F:
                ok = 1;
                primitive->texcoord_size = 2;
                primitive->texcoord_type = GL_FLOAT;
                primitive->texcoord_offset = offset;
                break;

              case GRAPHICS_VERTEX_COLOR_4NUB:
                ok = 1;
                primitive->color_size = 4;
                primitive->color_type = GL_UNSIGNED_BYTE;
                primitive->color_offset = offset;
                break;
              case GRAPHICS_VERTEX_COLOR_4F:
                ok = 1;
                primitive->color_size = 4;
                primitive->color_type = GL_FLOAT;
                primitive->color_offset = offset;
                break;
            }

            if (!ok) {
                DLOG("Unknown vertex data format 0x%08X, aborting", format[i]);
                goto error_free_primitive;
            }
        }  // if (GRAPHICS_VERTEX_TYPE_IS_{USER,ATTRIB}(format_type))
    }  // for (int i = 0; format[i] != 0; i++)
    ASSERT(user_attrib_index == user_attribs_used, goto error_free_primitive);

    /* If rendering quads on a platform that doesn't have native GL_QUADS
     * support, convert each quad to 2 triangles. */

    if (need_quad_indices) {
        const int num_points = (index_data != NULL) ? index_count : count;
        const int num_quads = num_points / 4;
        ASSERT(num_quads > 0);

        if (num_quads == 1 && !index_data) {
            /* If this is a single, non-indexed quad, use a common index
             * buffer to conserve resources. */

            if (opengl_has_features(OPENGL_FEATURE_FAST_STATIC_VBO)) {
                if (!single_quad_index_buffer) {
                    glGenBuffers(1, &single_quad_index_buffer);
                    if (UNLIKELY(!single_quad_index_buffer)) {
                        DLOG("Failed to generate single quad index buffer");
                        goto error_free_primitive;
                    }
                    bind_index_buffer(single_quad_index_buffer);
                    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                                 sizeof(single_quad_indices),
                                 single_quad_indices, GL_STATIC_DRAW);
                }
                primitive->index_buffer = single_quad_index_buffer;
            } else {
                /* Static vertex buffers disabled; we'll use the static
                 * index array when rendering.  It's safe to de-const the
                 * array here since we mark it as not locally allocated,
                 * so it will never be modified or freed. */
                primitive->index_data = (uint8_t *)single_quad_indices;
                primitive->index_local = 0;
            }
            primitive->has_indices = 1;
            primitive->is_single_quad = 1;
            primitive->index_size = index_size = 2;
            primitive->index_count = index_count = 6;

        } else {
            /* Multiple quads or primitive is indexed, so generate new data. */

            int quad_index_size;
            if (index_data) {
                quad_index_size = index_size;
            } else if (4*num_quads <= 65536) {
                /* Use 16-bit index values even if there are less than 256
                 * vertices since some GPUs handle them more efficiently
                 * and even the worst-case space cost is only 256*(6/4) =
                 * 384 bytes. */
                quad_index_size = 2;
            } else {
                if (!opengl_has_formats(OPENGL_FORMAT_INDEX32)) {
                    DLOG("Too many quads to render as triangles (%d, max %d)",
                         num_quads, 65536/4);
                    goto error_free_primitive;
                }
                quad_index_size = 4;
            }

            quad_index_data = mem_alloc(quad_index_size * (6*num_quads), 0, 0);
            if (UNLIKELY(!quad_index_data)) {
                DLOG("No memory for quad index list (%u*%u bytes)",
                     quad_index_size, 6*num_quads);
                goto error_free_primitive;
            }

            for (int i = 0; i < num_quads; i++) {
                uint32_t A, B, C, D;  // Vertex indices of this quad.
                if (index_data) {
                    if (index_size == 1) {
                        A = ((const uint8_t *)index_data)[4*i+0];
                        B = ((const uint8_t *)index_data)[4*i+1];
                        C = ((const uint8_t *)index_data)[4*i+2];
                        D = ((const uint8_t *)index_data)[4*i+3];
                    } else if (index_size == 2) {
                        A = ((const uint16_t *)index_data)[4*i+0];
                        B = ((const uint16_t *)index_data)[4*i+1];
                        C = ((const uint16_t *)index_data)[4*i+2];
                        D = ((const uint16_t *)index_data)[4*i+3];
                    } else {  // index_size == 4
                        A = ((const uint32_t *)index_data)[4*i+0];
                        B = ((const uint32_t *)index_data)[4*i+1];
                        C = ((const uint32_t *)index_data)[4*i+2];
                        D = ((const uint32_t *)index_data)[4*i+3];
                    }
                } else {  // !index_data
                    A = 4*i+0;
                    B = 4*i+1;
                    C = 4*i+2;
                    D = 4*i+3;
                }
                if (quad_index_size == 1) {
                    ((uint8_t *)quad_index_data)[6*i+0] = A;
                    ((uint8_t *)quad_index_data)[6*i+1] = B;
                    ((uint8_t *)quad_index_data)[6*i+2] = D;
                    ((uint8_t *)quad_index_data)[6*i+3] = D;
                    ((uint8_t *)quad_index_data)[6*i+4] = B;
                    ((uint8_t *)quad_index_data)[6*i+5] = C;
                } else if (quad_index_size == 2) {
                    ((uint16_t *)quad_index_data)[6*i+0] = A;
                    ((uint16_t *)quad_index_data)[6*i+1] = B;
                    ((uint16_t *)quad_index_data)[6*i+2] = D;
                    ((uint16_t *)quad_index_data)[6*i+3] = D;
                    ((uint16_t *)quad_index_data)[6*i+4] = B;
                    ((uint16_t *)quad_index_data)[6*i+5] = C;
                } else {  // quad_index_size == 4
                    ((uint32_t *)quad_index_data)[6*i+0] = A;
                    ((uint32_t *)quad_index_data)[6*i+1] = B;
                    ((uint32_t *)quad_index_data)[6*i+2] = D;
                    ((uint32_t *)quad_index_data)[6*i+3] = D;
                    ((uint32_t *)quad_index_data)[6*i+4] = B;
                    ((uint32_t *)quad_index_data)[6*i+5] = C;
                }
            }

            primitive->has_indices = 1;
            index_data = quad_index_data;
            primitive->index_size = index_size = quad_index_size;
            primitive->index_count = index_count = 6*num_quads;

        }  // if (single quad optimization)
    }  // if (need_quad_indices)

    if (primitive->has_indices) {
        if (index_size == 1) {
            primitive->index_type = GL_UNSIGNED_BYTE;
        } else if (index_size == 2) {
            primitive->index_type = GL_UNSIGNED_SHORT;
        } else {
            primitive->index_type = GL_UNSIGNED_INT;
        }
    }

    /* If vertex buffer usage is enabled, create vertex/index buffer
     * objects if possible (for immediate mode, reuse precreated buffer
     * objects) and load the data into them. */

    int immediate_vbo_index = 0;

    const int vbo_feature_flag = immediate ? OPENGL_FEATURE_FAST_DYNAMIC_VBO
                                           : OPENGL_FEATURE_FAST_STATIC_VBO;
    if (opengl_has_features(vbo_feature_flag)) {

        if (opengl_has_features(OPENGL_FEATURE_MANDATORY_VAO)
            || (!immediate
                && opengl_has_features(OPENGL_FEATURE_USE_STATIC_VAO)))
        {
            glGenVertexArrays(1, &primitive->vertex_array);
            if (UNLIKELY(!primitive->vertex_array)) {
                DLOG("Failed to create vertex array object: 0x%04X",
                     glGetError());
                goto error_free_primitive;
            }
        }

        if (immediate) {
            if (UNLIKELY(!immediate_vertex_buffers[0])) {
                glGenBuffers(lenof(immediate_vertex_buffers),
                             immediate_vertex_buffers);
            }
            primitive->is_immediate_vbo = 1;
            primitive->vertex_buffer =
                immediate_vertex_buffers[next_immediate_vbo];
            immediate_vbo_index = next_immediate_vbo;
            next_immediate_vbo++;
            if (next_immediate_vbo >= lenof(immediate_vertex_buffers)) {
                next_immediate_vbo = 0;
            }
            /* Careful not to overwrite the index buffer if already set!
             * (i.e., when optimizing a single quad) */
            if (primitive->has_indices && !primitive->index_buffer) {
                primitive->index_buffer =
                    immediate_vertex_buffers[next_immediate_vbo];
                next_immediate_vbo++;
                if (next_immediate_vbo >= lenof(immediate_vertex_buffers)) {
                    next_immediate_vbo = 0;
                }
            }
        } else {
            primitive->is_immediate_vbo = 0;
            glGenBuffers(1, &primitive->vertex_buffer);
            if (primitive->has_indices && !primitive->index_buffer) {
                glGenBuffers(1, &primitive->index_buffer);
                if (UNLIKELY(!primitive->index_buffer)) {
                    DLOG("Failed to create index buffer: 0x%04X", glGetError());
                    glDeleteBuffers(1, &primitive->vertex_buffer);
                    primitive->vertex_buffer = 0;
                }
            }
        }
        if (LIKELY(primitive->vertex_buffer)) {
            if (UNLIKELY(!load_primitive_data(
                             primitive, size * count, data,
                             index_size * index_count, index_data,
                             immediate_vbo_index))) {
                DLOG("Failed to load vertex data");
                if (!primitive->is_immediate_vbo) {
                    opengl_delete_buffer(primitive->vertex_buffer);
                    if (primitive->index_buffer
                     && primitive->index_buffer != single_quad_index_buffer) {
                        opengl_delete_buffer(primitive->index_buffer);
                    }
                    primitive->vertex_buffer = 0;
                }
            }
        } else {
            DLOG("Failed to create vertex buffer: 0x%04X", glGetError());
            if (primitive->vertex_array) {
                opengl_delete_vertex_array(primitive->vertex_array);
            }
            if (opengl_has_features(OPENGL_FEATURE_MANDATORY_VAO)) {
                goto error_free_primitive;
            }
        }

    }  // if (opengl_has_features(vbo_feature_flag))

    /* If we're not using VBOs (or we tried to get one and failed), store
     * the data in local buffers instead. */

    if (!primitive->vertex_buffer && !immediate) {

        primitive->vertex_data = mem_alloc(size * count, 0, 0);
        if (UNLIKELY(!primitive->vertex_data)) {
            DLOG("No memory for vertex data (%u*%u bytes)", size, count);
            goto error_free_primitive;
        }
        primitive->vertex_local = 1;
        memcpy(primitive->vertex_data, data, size * count);

        if (primitive->has_indices) {
            if (quad_index_data) {  // Just reuse the buffer in this case.
                primitive->index_data = quad_index_data;
                primitive->index_local = 1;
                quad_index_data = NULL;  // Don't free on return!
            } else if (index_data) {  // NULL if using the single quad buffer.
                primitive->index_data =
                    mem_alloc(index_size * index_count, 0, 0);
                if (UNLIKELY(!primitive->index_data)) {
                    DLOG("No memory for index data (%u*%u bytes)", index_size,
                         index_count);
                    mem_free(primitive->vertex_data);  // Was allocated above.
                    goto error_free_primitive;
                }
                primitive->index_local = 1;
                memcpy(primitive->index_data, index_data,
                       index_size * index_count);
            }
        }

    } else if (!primitive->vertex_buffer) {

        ASSERT(immediate);
        /* It's safe to drop the const -- we won't touch it if it's not
         * a local buffer. */
        primitive->vertex_data = (void *)data;
        if (quad_index_data) {  // Again, just reuse the buffer.
            primitive->index_data = quad_index_data;
            primitive->index_local = 1;
            quad_index_data = NULL;  // Don't free on return!
        } else if (primitive->index_data != (uint8_t *)single_quad_indices) {
            primitive->index_data = (void *)index_data;
        }

    }

    /* Return the new primitive. */

    mem_free(quad_index_data);
    return primitive;

  error_free_primitive:
    mem_free(primitive->user_attribs);
    mem_free(primitive);
  error_return:
    mem_free(quad_index_data);
    return NULL;
}

/*-----------------------------------------------------------------------*/

void sys_(graphics_draw_primitive)(SysPrimitive *primitive,
                                   int start, int count)
{
    if (UNLIKELY(primitive->generation != opengl_device_generation)) {
        DLOG("Attempt to draw invalidated primitive %p", primitive);
        return;
    }

    opengl_clear_error();

    if (!opengl_shader_objects_enabled) {
        opengl_apply_matrices(0);
        if (!opengl_apply_shader(primitive)) {
            DLOG("Failed to select shader, aborting render of primitive %p",
                 primitive);
            return;
        }
    }

    if (primitive->vertex_array) {
        glBindVertexArray(primitive->vertex_array);
        if (!primitive->vao_configured) {
            /* This modifies the VAO state, not global GL state, so it
             * needs to be executed unconditionally and must not modify
             * our cached value. */
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, primitive->index_buffer);
            /* Note that GL_ARRAY_BUFFER is _not_ part of VAO state,
             * but the vertex buffer is bound to each attribute by
             * glVertexAttribPointer() independently of the GL_ARRAY_BUFFER
             * binding, so we don't need to rebind the buffer if the VAO is
             * already configured. */
            bind_vertex_buffer(primitive->vertex_buffer);
        }
    } else {
        bind_vertex_buffer(primitive->vertex_buffer);
        bind_index_buffer(primitive->index_buffer);
    }

    const uint8_t * const vertex_base =
        primitive->vertex_buffer ? (const uint8_t *)0 : primitive->vertex_data;
    const uint8_t * const index_base =
        primitive->index_buffer ? (const uint8_t *)0 : primitive->index_data;

    if (!primitive->vao_configured) {
        configure_shader_attributes(primitive, vertex_base);
    }

#ifdef SIL_OPENGL_VALIDATE_SHADERS
    if (opengl_shader_objects_enabled
     && opengl_has_features(OPENGL_FEATURE_SEPARATE_SHADERS)) {

        GLint pipeline = 0;
        glGetIntegerv(GL_CURRENT_PROGRAM_PIPELINE, &pipeline);
        if (UNLIKELY(!pipeline)) {
            DLOG("Failed to get current program pipeline! (error=0x%X)",
                 glGetError());
        } else {
            glValidateProgramPipeline(pipeline);
            GLenum error = glGetError();
            if (error) {
                DLOG("glValidateProgramPipeline() failed! (error=0x%X)",
                     error);
            } else {
                GLint ok = 0;
                glGetProgramPipelineiv(program, GL_VALIDATE_STATUS, &ok);
                if (UNLIKELY(!ok)) {
                    GLint string_length = 0;
                    glGetProgramPipelineiv(program, GL_INFO_LOG_LENGTH,
                                           &string_length);
                    char *info = mem_alloc(string_length+1, 1, MEM_ALLOC_TEMP);
                    if (info) {
                        glGetProgramPipelineInfoLog(
                            program, string_length+1, NULL, info);
                    }
                    error = glGetError();
                    if (error) {
                        DLOG("Shader failed to validate, but failed to"
                             " retrieve shader log! (error=0x%X)", error);
                    } else {
                        DLOG("Shader failed to validate!  Log follows:\n%s",
                             info && *info ? info : "(no log)");
                    }
                    mem_free(info);
                }
            }
        }

    } else {  // not separate shaders

        GLint program = 0;
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        if (UNLIKELY(!program)) {
            DLOG("Failed to get current program! (error=0x%X)", glGetError());
        } else {
            glValidateProgram(program);
            GLenum error = glGetError();
            if (error) {
                DLOG("glValidateProgram() failed! (error=0x%X)", error);
            } else {
                GLint ok = 0;
                glGetProgramiv(program, GL_VALIDATE_STATUS, &ok);
                if (UNLIKELY(!ok)) {
                    GLint string_length = 0;
                    glGetProgramiv(program, GL_INFO_LOG_LENGTH,
                                   &string_length);
                    char *info = mem_alloc(string_length+1, 1, MEM_ALLOC_TEMP);
                    if (info) {
                        glGetProgramInfoLog(program, string_length+1, NULL,
                                            info);
                    }
                    error = glGetError();
                    if (error) {
                        DLOG("Shader failed to validate, but failed to"
                             " retrieve shader log! (error=0x%X)", error);
                    } else {
                        DLOG("Shader failed to validate!  Log follows:\n%s",
                             info && *info ? info : "(no log)");
                    }
                    mem_free(info);
                }
            }
        }

    }
#endif

    if (primitive->vertex_array) {
        primitive->vao_configured = 1;
    }

    int draw_unit = 1;
    if (primitive->converted_quads) {
        if (primitive->type == GL_TRIANGLES) {
            if (UNLIKELY(start % 4 != 0)) {
                DLOG("WARNING: unaligned partial draw of converted QUADS"
                     " primitive (start=%d count=%d)", start, count);
            }
            start = ((start/4) * 6) + (start % 4);
            if (count > 0) {
                count = (count/4) * 6;
            }
            draw_unit = 6;
        } else {  // Must be GL_TRIANGLE_STRIP, converted from QUAD_STRIP.
            if (UNLIKELY(start % 2 != 0)) {
                DLOG("WARNING: unaligned partial draw of converted QUAD_STRIP"
                     " primitive (start=%d count=%d)", start, count);
            }
            draw_unit = 2;
        }
    }
    if (primitive->has_indices) {
        if (start < primitive->index_count) {
            if (count < 0 || count > primitive->index_count - start) {
                count = primitive->index_count - start;
            }
            if (draw_unit > 1) {
                count -= count % draw_unit;
            }
            if (count > 0) {
                glDrawElements(primitive->type, count, primitive->index_type,
                               index_base + (start * primitive->index_size));
            }
        }
    } else {
        if (start < primitive->vertex_count) {
            if (count < 0 || count > primitive->vertex_count - start) {
                count = primitive->vertex_count - start;
            }
            if (draw_unit > 1) {
                count -= count % draw_unit;
            }
            if (count > 0) {
                glDrawArrays(primitive->type, start, count);
            }
        }
    }
    const int error = glGetError();
    if (error != GL_NO_ERROR) {
        DLOG("Error drawing primitive: 0x%04X", error);
    }

    if (primitive->vertex_array) {
        /* This is potentially redundant, but we call it anyway for
         * safety's sake on the assumption that it doesn't trigger any GPU
         * operations on its own. */
        glBindVertexArray(0);
    }
}

/*-----------------------------------------------------------------------*/

void sys_(graphics_destroy_primitive)(SysPrimitive *primitive)
{
    if (primitive->generation == opengl_device_generation) {
        if (primitive->vertex_buffer) {
            if (current_vertex_buffer == primitive->vertex_buffer) {
                bind_vertex_buffer(0);
            }
            if (primitive->index_buffer
             && current_index_buffer == primitive->index_buffer) {
                bind_index_buffer(0);
            }
            if (!primitive->is_immediate_vbo) {
                if (primitive->index_buffer && !primitive->is_single_quad) {
                    opengl_delete_buffer(primitive->index_buffer);
                }
                opengl_delete_buffer(primitive->vertex_buffer);
            }
        }
        if (primitive->vertex_array) {
            opengl_delete_vertex_array(primitive->vertex_array);
        }
    }

    if (primitive->index_local) {
        mem_free(primitive->index_data);
    }
    if (primitive->vertex_local) {
        mem_free(primitive->vertex_data);
    }
    mem_free(primitive->user_attribs);
    mem_free(primitive);
}

/*************************************************************************/
/******************* Library-internal utility routines *******************/
/*************************************************************************/

void opengl_primitive_reset_bindings(void)
{
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    current_vertex_buffer = current_index_buffer = 0;
    for (int i = 0; i < SHADER_ATTRIBUTE__NUM; i++) {
        glDisableVertexAttribArray(i);
    }
    /* glDisableVertexAttribArray() will raise errors if no shader has
     * been selected, so clear them. */
    opengl_clear_error();
    mem_clear(vertex_attrib_enabled, sizeof(vertex_attrib_enabled));
}

/*-----------------------------------------------------------------------*/

void opengl_primitive_cleanup(void)
{
    for (int i = 0; i < lenof(immediate_vertex_buffers); i++) {
        if (immediate_vertex_buffers[i] != 0) {
            opengl_delete_buffer(immediate_vertex_buffers[i]);
            immediate_vertex_buffers[i] = 0;
            immediate_vbo_sizes[i] = 0;
        }
    }
    next_immediate_vbo = 0;

    if (single_quad_index_buffer) {
        opengl_delete_buffer(single_quad_index_buffer);
        single_quad_index_buffer = 0;
    }

    bind_vertex_buffer(0);
    bind_index_buffer(0);
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static inline void bind_vertex_buffer(const GLuint buffer)
{
    if (current_vertex_buffer != buffer) {
        current_vertex_buffer = buffer;
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
    }
}

/*-----------------------------------------------------------------------*/

static inline void bind_index_buffer(const GLuint buffer)
{
    if (current_index_buffer != buffer) {
        current_index_buffer = buffer;
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);
    }
}

/*-----------------------------------------------------------------------*/

static inline void enable_vertex_attrib(int attrib)
{
    PRECOND(attrib >= 0 && attrib < lenof(vertex_attrib_enabled), return);
    if (!vertex_attrib_enabled[attrib]) {
        glEnableVertexAttribArray(attrib);
        vertex_attrib_enabled[attrib] = 1;
    }
}

/*-----------------------------------------------------------------------*/

static inline void disable_vertex_attrib(int attrib)
{
    PRECOND(attrib >= 0 && attrib < lenof(vertex_attrib_enabled), return);
    if (vertex_attrib_enabled[attrib]) {
        glDisableVertexAttribArray(attrib);
        vertex_attrib_enabled[attrib] = 0;
    }
}

/*-----------------------------------------------------------------------*/

static int load_primitive_data(
    SysPrimitive *primitive, int vertex_size, const void *vertex_data,
    int index_size, const void *index_data, int immediate_vbo_index)
{
    opengl_clear_error();
    bind_vertex_buffer(primitive->vertex_buffer);
    if (primitive->has_indices && index_data) {
        bind_index_buffer(primitive->index_buffer);
    }

    if (primitive->is_immediate_vbo) {
        if (vertex_size <= immediate_vbo_sizes[immediate_vbo_index]) {
            glBufferSubData(GL_ARRAY_BUFFER, 0, vertex_size, vertex_data);
        } else {
            glBufferData(GL_ARRAY_BUFFER, vertex_size, vertex_data,
                         GL_DYNAMIC_DRAW);
            immediate_vbo_sizes[immediate_vbo_index] = vertex_size;
        }
        if (primitive->has_indices && index_data) {
            immediate_vbo_index++;
            if (immediate_vbo_index >= lenof(immediate_vertex_buffers)) {
                immediate_vbo_index = 0;
            }
            if (index_size <= immediate_vbo_sizes[immediate_vbo_index]) {
                glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, index_size,
                                index_data);
            } else {
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_size, index_data,
                             GL_DYNAMIC_DRAW);
                immediate_vbo_sizes[immediate_vbo_index] = index_size;
            }
        }
    } else {
        glBufferData(GL_ARRAY_BUFFER, vertex_size, vertex_data, GL_STATIC_DRAW);
        if (primitive->has_indices && index_data) {
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_size, index_data,
                         GL_STATIC_DRAW);
        }
    }

    const int error = glGetError();
    if (UNLIKELY(error != 0)) {
        DLOG("Error loading vertex/index buffers: 0x%04X", error);
        return 0;
    } else {
        return 1;
    }
}

/*-----------------------------------------------------------------------*/

static int configure_standard_shader_attribute(
    const SysPrimitive *primitive, ShaderAttribute attribute, int size,
    GLenum type, GLboolean normalized, const void *pointer)
{
    if (size > 0) {
        int attr_index;
        if (opengl_shader_objects_enabled) {
            attr_index = opengl_shader_standard_attribute_binding(attribute);
        } else {
            attr_index = attribute;
        }
        if (attr_index >= 0) {
            if (opengl_shader_objects_enabled || primitive->vertex_array) {
                glEnableVertexAttribArray(attr_index);
            } else {
                enable_vertex_attrib(attr_index);
            }
            glVertexAttribPointer(attr_index, size, type, normalized,
                                  primitive->vertex_size, pointer);
        }
        return attr_index;
    } else {
        if (!opengl_shader_objects_enabled && !primitive->vertex_array) {
            disable_vertex_attrib(attribute);
        }
        return -1;
    }
}

/*-----------------------------------------------------------------------*/

static void configure_shader_attributes(const SysPrimitive *primitive,
                                        const uint8_t *vertex_base)
{
    const int position_attribute = configure_standard_shader_attribute(
        primitive, SHADER_ATTRIBUTE_POSITION, primitive->position_size,
        primitive->position_type, GL_FALSE,
        vertex_base + primitive->position_offset);
    const int texcoord_attribute = configure_standard_shader_attribute(
        primitive, SHADER_ATTRIBUTE_TEXCOORD, primitive->texcoord_size,
        primitive->texcoord_type, GL_TRUE,
        vertex_base + primitive->texcoord_offset);
    const int color_attribute = configure_standard_shader_attribute(
        primitive, SHADER_ATTRIBUTE_COLOR, primitive->color_size,
        primitive->color_type, GL_TRUE, vertex_base + primitive->color_offset);

    if (opengl_shader_objects_enabled) {

        const int num_attribs = opengl_shader_num_attributes();
        if (num_attribs > 0) {
            uint8_t attrib_used[256];
            ASSERT(num_attribs <= lenof(attrib_used), return);
            mem_clear(attrib_used, num_attribs);
            if (position_attribute >= 0 && position_attribute < num_attribs) {
                attrib_used[position_attribute] = 1;
            }
            if (texcoord_attribute >= 0 && texcoord_attribute < num_attribs) {
                attrib_used[texcoord_attribute] = 1;
            }
            if (color_attribute >= 0 && color_attribute < num_attribs) {
                attrib_used[color_attribute] = 1;
            }
            for (int i = 0; i < primitive->num_user_attribs; i++) {
                const uint32_t format = primitive->user_attribs[i];
                const int type = GRAPHICS_VERTEX_FORMAT_TYPE(format);
                const int offset = GRAPHICS_VERTEX_FORMAT_OFFSET(format);
                if (UNLIKELY(!GRAPHICS_VERTEX_TYPE_IS_ATTRIB(type))) {
                    continue;
                }
                const int index = GRAPHICS_VERTEX_ATTRIB_INDEX(type);
                const int data_count = GRAPHICS_VERTEX_ATTRIB_COUNT(type);
                const GraphicsVertexDataType data_type =
                    GRAPHICS_VERTEX_ATTRIB_TYPE(type);
                if (index >= num_attribs) {
                    continue;
                }
                attrib_used[index] = 1;
                static const uint8_t is_float[16] = {
                    [GRAPHICS_VERTEXDATA_NUB] = 1,
                    [GRAPHICS_VERTEXDATA_NS ] = 1,
                    [GRAPHICS_VERTEXDATA_F  ] = 1,
                };
                /* Undefined values here will become GL_ZERO, which
                 * will cause errors (as desired) if they're seen in
                 * the data. */
                static const GLenum gl_type[16] = {
                    [GRAPHICS_VERTEXDATA_UB ] = GL_UNSIGNED_BYTE,
                    [GRAPHICS_VERTEXDATA_S  ] = GL_SHORT,
                    [GRAPHICS_VERTEXDATA_I  ] = GL_INT,
                    [GRAPHICS_VERTEXDATA_NUB] = GL_UNSIGNED_BYTE,
                    [GRAPHICS_VERTEXDATA_NS ] = GL_SHORT,
                    [GRAPHICS_VERTEXDATA_F  ] = GL_FLOAT,
                };
                static const GLboolean gl_norm[16] = {
                    [GRAPHICS_VERTEXDATA_UB ] = GL_FALSE,
                    [GRAPHICS_VERTEXDATA_S  ] = GL_FALSE,
                    [GRAPHICS_VERTEXDATA_I  ] = GL_FALSE,
                    [GRAPHICS_VERTEXDATA_NUB] = GL_TRUE,
                    [GRAPHICS_VERTEXDATA_NS ] = GL_TRUE,
                    [GRAPHICS_VERTEXDATA_F  ] = GL_FALSE,
                };
                glEnableVertexAttribArray(index);
                if (is_float[data_type]) {
                    glVertexAttribPointer(
                        index, data_count, gl_type[data_type],
                        gl_norm[data_type], primitive->vertex_size,
                        vertex_base + offset);
                } else {
                    ASSERT(dyngl_has_vertex_attrib_int());
                    glVertexAttribIPointer(
                        index, data_count, gl_type[data_type],
                        primitive->vertex_size, vertex_base + offset);
                }
            }
            if (!primitive->vertex_array) {
                for (int i = 0; i < num_attribs; i++) {
                    if (!attrib_used[i]) {
                        glDisableVertexAttribArray(i);
                    }
                }
            }
        }

    } else {  // Generated shader.

        int num_user_attribs;
        const int8_t *user_attrib_sizes =
            opengl_get_user_attrib_sizes(&num_user_attribs);
        if (num_user_attribs > 0) {
            uint8_t attrib_used[0x1000];
            ASSERT(num_user_attribs <= lenof(attrib_used), return);
            mem_clear(attrib_used, num_user_attribs);
            for (int i = 0; i < primitive->num_user_attribs; i++) {
                const uint32_t format = primitive->user_attribs[i];
                const int type = GRAPHICS_VERTEX_FORMAT_TYPE(format);
                const int offset = GRAPHICS_VERTEX_FORMAT_OFFSET(format);
                if (UNLIKELY(!GRAPHICS_VERTEX_TYPE_IS_USER(type))) {
                    continue;
                }
                const int index = type - (GRAPHICS_VERTEX_USER(0) + 1);
                if (index >= num_user_attribs) {
                    continue;
                }
                attrib_used[index] = 1;
                const int attrib = SHADER_ATTRIBUTE__NUM + index;
                glEnableVertexAttribArray(attrib);
                glVertexAttribPointer(attrib,
                                      user_attrib_sizes[index],
                                      GL_FLOAT,
                                      GL_TRUE,
                                      primitive->vertex_size,
                                      vertex_base + offset);
            }
            if (!primitive->vertex_array) {
                for (int i = 0; i < num_user_attribs; i++) {
                    if (!attrib_used[i]) {
                        glDisableVertexAttribArray(SHADER_ATTRIBUTE__NUM + i);
                    }
                }
            }
        }
    }
}

/*************************************************************************/
/*************************************************************************/
