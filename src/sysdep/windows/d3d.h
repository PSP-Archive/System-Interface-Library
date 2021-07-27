/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/windows/d3d.h: Windows-internal interface to the Direct3D
 * implementation of the sysdep graphics functions.
 */

#ifndef SIL_SRC_SYSDEP_WINDOWS_D3D_H
#define SIL_SRC_SYSDEP_WINDOWS_D3D_H

#ifndef SIL_SRC_SYSDEP_WINDOWS_INTERNAL_H
# include "src/sysdep/windows/internal.h"
#endif

/*************************************************************************/
/*************************************************************************/

/**
 * d3d_open_library:  Open the Direct3D DLL.
 *
 * [Return value]
 *     True on success, false on error.
 */
extern int d3d_open_library(void);

/**
 * d3d_close_library:  Close the Direct3D DLL.  Must not be called while a
 * context is open.
 */
extern void d3d_close_library(void);

/**
 * d3d_create_context:  Create a Direct3D context for a new window.
 *
 * [Parameters]
 *     window: Window for which to create a context.
 *     width, height: Size of the window's client area.
 *     depth_bits: Minimum number of bits per pixel for the depth buffer.
 *     stencil_bits: Minimum number of bits per pixel for the stencil buffer.
 *     samples: Number of samples per pixel (1 to disable multisampling).
 * [Return value]
 *     True on success, false on error.
 */
extern int d3d_create_context(HWND window, int width, int height,
                              int depth_bits, int stencil_bits, int samples);

/**
 * d3d_destroy_context:  Destroy any existing Direct3D context.
 */
extern void d3d_destroy_context(void);

/**
 * d3d_resize_window:  Update Direct3D window-related resources following
 * a window size change.  Must be called between frames.
 */
extern void d3d_resize_window(void);

/**
 * d3d_start_frame:  Set up for rendering a new frame.
 */
extern void d3d_start_frame(void);

/**
 * d3d_finish_frame:  End rendering of the current frame and push it to
 * the display device.
 */
extern void d3d_finish_frame(void);

/**
 * d3d_sync:  Wait for all pending graphics operations to complete.
 */
extern void d3d_sync(void);

/**
 * d3d_strerror:  Return a string describing the given Direct3D error code.
 */
extern const char *d3d_strerror(HRESULT result);

/**
 * d3d_framebuffer_set_depth_stencil_size:  Set the depth and stencil sizes
 * for subsequently created framebuffers.
 */
extern void d3d_framebuffer_set_depth_stencil_size(int depth, int stencil);

/**
 * d3d_shader_set_debug_info:  Set whether to include debugging information
 * in compiled shader data.
 */
extern void d3d_shader_set_debug_info(int enable);

/**
 * d3d_shader_set_opt_level:  Set the optimization level (0-3) for shader
 * compilation.
 */
extern void d3d_shader_set_opt_level(int level);

/**
 * d3d_compile_default_shaders:  Compile all default shaders and return a
 * newly-allocated string buffer containing C source code which defines
 * the compiled bytecode for each shader.  Intended for building the
 * d3d-defshaders.i source file.
 */
extern char *d3d_compile_default_shaders(void);

/*-----------------------------------------------------------------------*/

/* The remainder of the declarations implement the sysdep graphics interface,
 * much like the OpenGL versions of these types and functions. */

typedef struct D3DSysFramebuffer D3DSysFramebuffer;
typedef struct D3DSysPrimitive D3DSysPrimitive;
typedef struct D3DSysShader D3DSysShader;
typedef struct D3DSysShaderPipeline D3DSysShaderPipeline;
typedef struct D3DSysTexture D3DSysTexture;

struct Vector2f;
struct Vector3f;
struct Vector4f;
struct Matrix4f;

enum FramebufferColorType;

extern int d3d_sys_framebuffer_supported(void);
extern D3DSysFramebuffer *d3d_sys_framebuffer_create(int width, int height, enum FramebufferColorType color_type, int depth_bits, int stencil_bits);
extern void d3d_sys_framebuffer_destroy(D3DSysFramebuffer *framebuffer);
extern void d3d_sys_framebuffer_bind(D3DSysFramebuffer *framebuffer);
extern D3DSysTexture *d3d_sys_framebuffer_get_texture(D3DSysFramebuffer *framebuffer);
extern void d3d_sys_framebuffer_set_antialias(D3DSysFramebuffer *framebuffer, int on);
extern void d3d_sys_framebuffer_discard_data(D3DSysFramebuffer *framebuffer);

extern const char *d3d_sys_graphics_renderer_info(void);
extern void d3d_sys_graphics_clear(const struct Vector4f *color, const float *depth, unsigned int stencil);
extern int d3d_sys_graphics_read_pixels(int x, int y, int w, int h, int stride, void *buffer);
extern void d3d_sys_graphics_set_viewport(int left, int bottom, int width, int height);
extern void d3d_sys_graphics_set_clip_region(int left, int bottom, int width, int height);
extern void d3d_sys_graphics_set_depth_range(float near, float far);
extern int d3d_sys_graphics_set_blend(int operation, int src_factor, int dest_factor);
extern int d3d_sys_graphics_set_blend_alpha(int enable, int src_factor, int dest_factor);
extern void d3d_sys_graphics_set_int_param(SysGraphicsParam id, int value);
extern void d3d_sys_graphics_set_float_param(SysGraphicsParam id, float value);
extern void d3d_sys_graphics_set_vec2_param(SysGraphicsParam id, const struct Vector2f *value);
extern void d3d_sys_graphics_set_vec4_param(SysGraphicsParam id, const struct Vector4f *value);
extern void d3d_sys_graphics_set_matrix_param(SysGraphicsParam id, const struct Matrix4f *value);
extern void d3d_sys_graphics_get_matrix_param(SysGraphicsParam id, struct Matrix4f *value_ret);
extern float d3d_sys_graphics_max_point_size(void);
extern D3DSysPrimitive *d3d_sys_graphics_create_primitive(enum GraphicsPrimitiveType type, const void *data, const uint32_t *format, int size, int count, const void *index_data, int index_size, int index_count, int immediate);
extern void d3d_sys_graphics_draw_primitive(D3DSysPrimitive *primitive, int start, int count);
extern void d3d_sys_graphics_destroy_primitive(D3DSysPrimitive *primitive);
extern int d3d_sys_graphics_enable_shader_objects(void);
extern int d3d_sys_graphics_disable_shader_objects(void);

extern PURE_FUNCTION int d3d_sys_shader_background_compilation_supported(void);
extern void d3d_sys_shader_enable_get_binary(int enable);
extern PURE_FUNCTION int d3d_sys_shader_max_attributes(void);
extern int d3d_sys_shader_set_attribute(int index, const char *name);
extern void d3d_sys_shader_bind_standard_attribute(enum ShaderAttribute attribute, int index);
extern void d3d_sys_shader_clear_attributes(void);
extern D3DSysShader *d3d_sys_shader_create(enum ShaderType type, const void *data, int size, int is_binary);
extern void d3d_sys_shader_destroy(D3DSysShader *shader);
extern void *d3d_sys_shader_get_binary(D3DSysShader *shader, int *size_ret);
extern void *d3d_sys_shader_compile(enum ShaderType type, const char *source, int length, int *size_ret);
extern int d3d_sys_shader_get_uniform_id(D3DSysShader *shader, const char *name);
extern void d3d_sys_shader_set_uniform_int(D3DSysShader *shader, int uniform, int value);
extern void d3d_sys_shader_set_uniform_float(D3DSysShader *shader, int uniform, float value);
extern void d3d_sys_shader_set_uniform_vec2(D3DSysShader *shader, int uniform, const struct Vector2f *value);
extern void d3d_sys_shader_set_uniform_vec3(D3DSysShader *shader, int uniform, const struct Vector3f *value);
extern void d3d_sys_shader_set_uniform_vec4(D3DSysShader *shader, int uniform, const struct Vector4f *value);
extern void d3d_sys_shader_set_uniform_mat4(D3DSysShader *shader, int uniform, const struct Matrix4f *value);
extern D3DSysShaderPipeline *d3d_sys_shader_pipeline_create(D3DSysShader *vertex_shader, D3DSysShader *fragment_shader);
extern void d3d_sys_shader_pipeline_destroy(D3DSysShaderPipeline *pipeline);
extern void d3d_sys_shader_pipeline_apply(D3DSysShaderPipeline *pipeline);

extern D3DSysTexture *d3d_sys_texture_create(int width, int height, enum TextureFormat data_format, int num_levels, void *data, int stride, const int32_t *level_offsets, const int32_t *level_sizes, int mipmaps, int mem_flags, int reuse);
extern void d3d_sys_texture_destroy(D3DSysTexture *texture);
extern int d3d_sys_texture_width(D3DSysTexture *texture);
extern int d3d_sys_texture_height(D3DSysTexture *texture);
extern int d3d_sys_texture_has_mipmaps(D3DSysTexture *texture);
extern D3DSysTexture *d3d_sys_texture_grab(int x, int y, int w, int h, int readable, int mipmaps, int mem_flags);
extern void *d3d_sys_texture_lock(D3DSysTexture *texture, SysTextureLockMode lock_mode, int x, int y, int w, int h);
extern void d3d_sys_texture_unlock(D3DSysTexture *texture, int update);
extern void d3d_sys_texture_flush(D3DSysTexture *texture);
extern void d3d_sys_texture_set_repeat(D3DSysTexture *texture, int repeat_u, int repeat_v);
extern void d3d_sys_texture_set_antialias(D3DSysTexture *texture, int on);
extern void d3d_sys_texture_apply(int unit, D3DSysTexture *texture);
extern int d3d_sys_texture_num_units(void);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_WINDOWS_D3D_H
