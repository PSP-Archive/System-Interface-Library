/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/opengl/opengl.h: Interface to OpenGL for use by system-specific
 * graphics implementations.
 */

/*
 * IMPORTANT NOTE:  This is an internal SIL header, not intended for use
 * in client code.  Be aware that the interfaces and constants declared in
 * this header may change without warning.
 */

#ifndef SIL_SRC_SYSDEP_OPENGL_OPENGL_H
#define SIL_SRC_SYSDEP_OPENGL_OPENGL_H

#include "src/sysdep/opengl/gl-headers.h"

/*************************************************************************/
/*************** OpenGL feature flags (for opengl_init()) ****************/
/*************************************************************************/

/* GL object deletion should be delayed until opengl_free_dead_resources()
 * is called.  (Set this flag if delete operations tend to block for long
 * periods of time.) */
#define OPENGL_FEATURE_DELAYED_DELETE      (1<<0)

/* Vertex buffer objects for dynamic (immediate-mode) primitives are
 * faster than client-side data buffers, or client-side data buffers are
 * not available. */
#define OPENGL_FEATURE_FAST_DYNAMIC_VBO    (1<<1)

/* Vertex buffer objects for static (create-once, draw-many) primitives
 * are faster than client-side data buffers, or client-side data buffers
 * are not available. */
#define OPENGL_FEATURE_FAST_STATIC_VBO     (1<<2)

/* The glGenerateMipmap() function should be used for generating mipmaps,
 * instead of manually generating mipmaps from the pixel data.  This flag
 * is ignored if glGenerateMipmap() is not available. */
#define OPENGL_FEATURE_GENERATEMIPMAP      (1<<3)

/* Vertex array objects are required for primitive rendering.  This should
 * be set when using a core profile implementation of OpenGL 3.0 or later. */
#define OPENGL_FEATURE_MANDATORY_VAO       (1<<4)

/* Quads and quad strips should be drawn using GL_QUAD or GL_QUAD_STRIP
 * primitives.  If this flag is not set, quads will be drawn as indexed
 * triangles.  Note that this flag should not be used with an OpenGL 3.0
 * (or later) core profile renderer, since OpenGL 3.0 deprecates quads
 * entirely. */
#define OPENGL_FEATURE_NATIVE_QUADS        (1<<5)

/* Vertex array objects should be used for static, but not dynamic,
 * primitive rendering.  This flag has no meaning if the MANDATORY_VAO
 * feature flag is set. */
#define OPENGL_FEATURE_USE_STATIC_VAO      (1<<6)

/* Suppress the use of separate shader objects, even if available.  (This
 * is a hack for some Intel OpenGL drivers on Windows which improperly
 * change the bound pipeline object on a glProgramUniform*() call for a
 * shader on a non-current pipeline.) */
#define OPENGL_FEATURE_NO_SEPARATE_SHADERS (1<<13)

/* glCopyTexImage() is broken.  (This is a workaround for a bug in iOS
 * 8.{0,1,2} which causes the alpha byte to receive 0 instead of 255.) */
#define OPENGL_FEATURE_BROKEN_COPYTEXIMAGE (1<<14)

/* Integer vertex attributes are broken.  (This is a workaround for a bug
 * in iOS which causes shaders with integer vertex attributes to fail to
 * compile with an internal error.) */
#define OPENGL_FEATURE_BROKEN_ATTRIB_INT   (1<<15)


/* The following flags are set automatically based on the OpenGL version
 * and extensions reported by the renderer. */


/* The glDiscardFramebufferEXT() function is available (OpenGL ES 2.x with
 * EXT_discard_framebuffer). */
#define OPENGL_FEATURE_DISCARD_FRAMEBUFFER (1<<16)

/* The GL has framebuffer support (OpenGL 3.0 or EXT_framebuffer_object,
 * OpenGL ES 1.1). */
#define OPENGL_FEATURE_FRAMEBUFFERS        (1<<17)

/* The glGetTexImage() function is available (OpenGL but not ES). */
#define OPENGL_FEATURE_GETTEXIMAGE         (1<<18)

/* The GL supports separate vertex and fragment shader programs (OpenGL 4.1
 * or ARB_separate_shader_objects, OpenGL ES 3.1 or the GLES version of
 * EXT_separate_shader_objects). */
#define OPENGL_FEATURE_SEPARATE_SHADERS    (1<<19)

/* The GL has support for retrieving and loading shader binaries (OpenGL 4.1
 * or ARB_get_program_binary, OpenGL ES 3.0 or OES_get_program_binary). */
#define OPENGL_FEATURE_SHADER_BINARIES     (1<<20)

/* The GL supports the glTexStorage functions (OpenGL 4.2 or
 * ARB_texture_storage, OpenGL ES 3.0). */
#define OPENGL_FEATURE_TEXTURE_STORAGE     (1<<21)

/* The GL supports integer-type vertex attributes (OpenGL 3.0 or
 * EXT_gpu_shader4, OpenGL ES 3.0). */
#define OPENGL_FEATURE_VERTEX_ATTRIB_INT   (1<<22)

/* All automatically-set flags. */
#define OPENGL_AUTOCONFIG_FEATURE_MASK     (0xFFFF0000U)


/*************************************************************************/
/***** OpenGL texture/index format flags (for opengl_has_formats()) ******/
/*************************************************************************/

/* BGRA ordering is supported for RGB pixel data. */
#define OPENGL_FORMAT_BGRA      (1<<0)

/* Reversed bit ordering (GL_..._REV) is supported for packed pixel formats. */
#define OPENGL_FORMAT_BITREV    (1<<1)

/* 32-bit index values are supported for indexed primitives. */
#define OPENGL_FORMAT_INDEX32   (1<<2)

/* PVRTC compression formats are supported. */
#define OPENGL_FORMAT_PVRTC     (1<<3)

/* Single- and double-component color textures (RED and RG) are supported. */
#define OPENGL_FORMAT_RG        (1<<4)

/* S3TC compression formats (DXTn) are supported. */
#define OPENGL_FORMAT_S3TC      (1<<5)

/*************************************************************************/
/************************* Convenience functions *************************/
/*************************************************************************/

/**
 * opengl_clear_error:  Clear any pending GL error.  Equivalent to looping
 * over glGetError() until it returns GL_NO_ERROR, but makes the intent
 * clearer.
 */
static inline void opengl_clear_error(void)
{
    while (glGetError() != GL_NO_ERROR) { /*spin*/ }
}

/*************************************************************************/
/******************** Interface function declarations ********************/
/*************************************************************************/

/******** framebuffer.c ********/

/**
 * opengl_set_default_framebuffer:  Set the default framebuffer ID to be
 * used when rendering to the display.  If not set, this defaults to zero.
 * (This value is meaningless if framebuffers are not available.)
 *
 * [Parameters]
 *     default_fb: Default framebuffer ID.
 */
extern void opengl_set_default_framebuffer(GLuint default_fb);

/**
 * opengl_get_default_framebuffer:  Return the default framebuffer ID as
 * set by opengl_set_default_framebuffer().
 *
 * [Return value]
 *     Default framebuffer ID.
 */
extern GLuint opengl_get_default_framebuffer(void);


/******** graphics.c ********/

/**
 * opengl_lookup_functions:  Look up function pointers for all OpenGL
 * functions, if not already done.  This function must be called before
 * calling any OpenGL functions or opengl_init(), but after loading the
 * OpenGL library (if applicable to the platform).
 *
 * [Parameters]
 *     lookup_function: Function to call to look up an OpenGL function.
 *         Takes the GL function name (e.g., "glGetIntegerv") and returns
 *         the pointer to that function or NULL if the function is not found.
 */
extern void opengl_lookup_functions(void *(*lookup_function)(const char *));

/**
 * opengl_get_version:  Parse the OpenGL version strings to determine the
 * OpenGL and GLSL versions of the OpenGL context.  This function can be
 * called if the versions are needed after context creation but before
 * opengl_init() has been called.
 */
extern void opengl_get_version(void);

/**
 * opengl_enable_debug:  Enable or disable logging of OpenGL debug messages.
 * This function may be called at any time after opengl_lookup_functions(),
 * even before opengl_init().
 *
 * If DEBUG is not defined, this function does nothing.
 *
 * [Parameters]
 *     enable: True to enable logging of debug messages, false to disable.
 */
extern void opengl_enable_debug(int enable);

/**
 * opengl_debug_is_enabled:  Return whether logging of OpenGL debug messages
 * is enabled.
 *
 * [Return value]
 *      True if debug message logging is enabled, false if not.
 */
extern int opengl_debug_is_enabled(void);

/**
 * opengl_init:  Initialize the OpenGL interface.  This does _not_ set up
 * the display or GL context; those tasks are the responsibility of
 * system-dependent code, and must be completed before this function is
 * called.
 *
 * [Parameters]
 *     width, height: Size of the OpenGL output window, in pixels.
 *     features: Bitmask of available OpenGL features (OPENGL_FEATURE_*).
 * [Return value]
 *     True on success, false if the OpenGL version is too old.
 */
extern int opengl_init(int width, int height, uint32_t features);

/**
 * opengl_cleanup:  Clean up and shut down the OpenGL interface.
 *
 * After calling this function, all existing OpenGL objects are assumed to
 * have become invalid.  Attempting to use the corresponding SIL object
 * will will have no effect, and will log an error in debug mode;
 * destroying the corresponding SIL object will not destroy the OpenGL
 * object.  (This allows callers on systems with device loss to have a
 * single destroy-and-recreate routine for graphics objects called after
 * restoring the device, rather than requiring a separate destroy routine
 * done before restoring the device.)
 */
extern void opengl_cleanup(void);

/**
 * opengl_major_version:  Return the major version number of OpenGL
 * supported by the runtime environment.  For example:
 *    - OpenGL 4.3 => return value 4
 *    - OpenGL ES 2.0 => return value 2
 *
 * [Return value]
 *     OpenGL major version number.
 */
extern int opengl_major_version(void);

/**
 * opengl_minor_version:  Return the minor version number of OpenGL
 * supported by the runtime environment.  For example:
 *    - OpenGL 4.3 => return value 3
 *    - OpenGL ES 2.0 => return value 0
 *
 * [Return value]
 *     OpenGL minor version number.
 */
extern int opengl_minor_version(void);

/**
 * opengl_version_is_at_least:  Return whether the OpenGL version supported
 * by the runtime environment is at least the given version.
 *
 * [Parameters]
 *     major: OpenGL major version to test against.
 *     minor: OpenGL minor version to test against.
 * [Return value]
 *     opengl_major_version() > major ||
 *         (opengl_major_version == major && opengl_minor_version() >= minor)
 */
extern int opengl_version_is_at_least(int major, int minor);

/**
 * opengl_sl_major_version:  Return the major version number of OpenGL
 * Shading Language supported by the runtime environment.  For example:
 *    - GLSL 4.20 => return value 4
 *    - GLSL ES 1.00 => return value 1
 *
 * [Return value]
 *     OpenGL Shading Language major version number.
 */
extern int opengl_sl_major_version(void);

/**
 * opengl_sl_minor_version:  Return the minor version number of OpenGL
 * Shading Language supported by the runtime environment.  For example:
 *    - GLSL 4.20 => return value 20
 *    - GLSL ES 1.00 => return value 0
 *
 * [Return value]
 *     OpenGL Shading Language minor version number.
 */
extern int opengl_sl_minor_version(void);

/**
 * opengl_sl_version_is_at_least:  Return whether the OpenGL Shading
 * Language version supported by the runtime environment is at least the
 * given version.
 *
 * [Parameters]
 *     major: GLSL major version to test against.
 *     minor: GLSL minor version to test against.
 * [Return value]
 *     True if the GLSL version supported by the system is at least the
 *     given version.
 */
extern int opengl_sl_version_is_at_least(int major, int minor);

/**
 * opengl_has_extension:  Return whether the OpenGL implementation supports
 * the given extension.  Used internally by other OpenGL functions to check
 * feature availability, but may also be called from outside.
 *
 * [Parameters]
 *     name: Extension name, including the "GL_" prefix.
 * [Return value]
 *     True if the extension is supported, false if not.
 */
extern PURE_FUNCTION int opengl_has_extension(const char *name);

/**
 * opengl_has_features:  Return whether the given OpenGL features are all
 * available.
 *
 * [Parameters]
 *     features: Features to check.
 * [Return value]
 *     True if all of the specified features are available, false otherwise.
 */
extern PURE_FUNCTION int opengl_has_features(uint32_t features);

/**
 * opengl_has_formats:  Return whether the given OpenGL formats are all
 * supported.
 *
 * [Parameters]
 *     formats: Formats to check.
 * [Return value]
 *     True if all of the specified formats are supported, false otherwise.
 */
extern PURE_FUNCTION int opengl_has_formats(uint32_t formats);

/**
 * opengl_set_delete_buffer_size:  Set the size of the buffer used to hold
 * objects pending deletion.  The buffer will never change size; if more
 * objects are deleted without being flushed than can fit in the array,
 * the object which overflows the array will trigger an immediate flush of
 * all pending deletions.
 *
 * If size == 0, the buffer will instead be dynamically allocated and
 * expanded as necessary to hold objects pending deletion.  This is the
 * default behavior.
 *
 * Calling this function will flush all pending deletions and free the
 * existing delete buffer, if any.
 *
 * This function does nothing if the OPENGL_FEATURE_DELAYED_DELETE feature
 * flag was not passed to opengl_init().
 *
 * [Parameters]
 *     size: Maximum number of elements to store, or zero to use dynamic
 *         allocation.
 * [Return value]
 *     True on success, false on insufficient memory.
 */
int opengl_set_delete_buffer_size(int size);

/**
 * opengl_set_compile_context_callback:  Set a function to be called by
 * sys_shader_compile() to create a GL context for shader compilation on
 * the current thread.  The function should return true if the current
 * thread already has a context suitable for shader compilation or if
 * such a context is successfully created, false if a context could not
 * be created (which will cause shader complation on the current thread
 * to fail).
 *
 * [Parameters]
 *     function: Function to call to set up a GL context on the current
 *         thread, or NULL to cancel any existing callback.
 */
void opengl_set_compile_context_callback(int (*function)(void));

/**
 * opengl_set_display_size:  Set the size of the display (i.e., the
 * default framebuffer).  This function should be called after changing
 * the size of an open window when the size change does not cause the
 * existing OpenGL context to be destroyed.
 *
 * [Parameters]
 *     width, height: New display size, in pixels.
 */
extern void opengl_set_display_size(int width, int height);

/**
 * opengl_start_frame:  Perform OpenGL-specific setup at the beginning of
 * the frame.  (There is no corresponding opengl_finish_frame(), as it is
 * not currently required.)
 */
extern void opengl_start_frame(void);

/**
 * opengl_sync:  Wait for background rendering to complete.
 */
extern void opengl_sync(void);

/**
 * opengl_free_dead_resources:  Free all resources which were destroyed
 * since the last call to this function.  If the DELAYED_DELETE feature
 * flag is passed to opengl_init(), the system-dependent code must
 * periodically call this function (typically at the beginning of a frame)
 * to prevent resources from leaking.
 *
 * If the DELAYED_DELETE feature flag was not passed to opengl_init(),
 * this function does nothing.
 *
 * [Parameters]
 *     also_array: True to also free any memory allocated dynamically for
 *         storing pending deletions; false to leave such memory allocated.
 */
extern void opengl_free_dead_resources(int also_array);


/******** texture.c ********/

/**
 * opengl_texture_id:  Return the OpenGL texture ID for the given texture.
 *
 * [Parameters]
 *     texture: SysTexture object (must be non-NULL).
 * [Return value]
 *     OpenGL texture ID.
 */
#ifdef SIL_OPENGL_NO_SYS_FUNCS
struct OpenGLSysTexture;
extern int opengl_texture_id(const struct OpenGLSysTexture *texture);
#else
struct SysTexture;
extern int opengl_texture_id(const struct SysTexture *texture);
#endif

/*************************************************************************/
/****************** OpenGL subsystem function renaming *******************/
/*************************************************************************/

/* If requested, we rename all sys_* functions to opengl_sys_* (see the
 * documentation of SIL_OPENGL_NO_SYS_FUNCS in include/SIL/base.h); these
 * prototypes allow the platform's graphics manager to call the renamed
 * functions. */

#ifdef SIL_OPENGL_NO_SYS_FUNCS

struct OpenGLSysFramebuffer;
struct OpenGLSysPrimitive;
struct OpenGLSysShader;
struct OpenGLSysShaderPipeline;
struct OpenGLSysTexture;

struct Vector2f;
struct Vector3f;
struct Vector4f;
struct Matrix4f;

enum FramebufferColorType;

extern int opengl_sys_framebuffer_supported(void);
extern struct OpenGLSysFramebuffer *opengl_sys_framebuffer_create(int width, int height, enum FramebufferColorType color_type, int depth_bits, int stencil_bits);
extern void opengl_sys_framebuffer_destroy(struct OpenGLSysFramebuffer *framebuffer);
extern void opengl_sys_framebuffer_bind(struct OpenGLSysFramebuffer *framebuffer);
extern struct OpenGLSysTexture *opengl_sys_framebuffer_get_texture(struct OpenGLSysFramebuffer *framebuffer);
extern void opengl_sys_framebuffer_set_antialias(struct OpenGLSysFramebuffer *framebuffer, int on);
extern void opengl_sys_framebuffer_discard_data(struct OpenGLSysFramebuffer *framebuffer);

extern const char *opengl_sys_graphics_renderer_info(void);
extern void opengl_sys_graphics_clear(const struct Vector4f *color, const float *depth, unsigned int stencil);
extern int opengl_sys_graphics_read_pixels(int x, int y, int w, int h, int stride, void *buffer);
extern void opengl_sys_graphics_set_viewport(int left, int bottom, int width, int height);
extern void opengl_sys_graphics_set_clip_region(int left, int bottom, int width, int height);
extern void opengl_sys_graphics_set_depth_range(float near, float far);
extern int opengl_sys_graphics_set_blend(int operation, int src_factor, int dest_factor);
extern int opengl_sys_graphics_set_blend_alpha(int enable, int src_factor, int dest_factor);
extern void opengl_sys_graphics_set_int_param(SysGraphicsParam id, int value);
extern void opengl_sys_graphics_set_float_param(SysGraphicsParam id, float value);
extern void opengl_sys_graphics_set_vec2_param(SysGraphicsParam id, const struct Vector2f *value);
extern void opengl_sys_graphics_set_vec4_param(SysGraphicsParam id, const struct Vector4f *value);
extern void opengl_sys_graphics_set_matrix_param(SysGraphicsParam id, const struct Matrix4f *value);
extern void opengl_sys_graphics_get_matrix_param(SysGraphicsParam id, struct Matrix4f *value_ret);
extern float opengl_sys_graphics_max_point_size(void);
extern struct OpenGLSysPrimitive *opengl_sys_graphics_create_primitive(enum GraphicsPrimitiveType type, const void *data, const uint32_t *format, int size, int count, const void *index_data, int index_size, int index_count, int immediate);
extern void opengl_sys_graphics_draw_primitive(struct OpenGLSysPrimitive *primitive, int start, int count);
extern void opengl_sys_graphics_destroy_primitive(struct OpenGLSysPrimitive *primitive);
extern int opengl_sys_graphics_set_shader_generator(void *vertex_source_callback, void *fragment_source_callback, void *key_callback, int hash_table_size, int dynamic_resize);
extern int opengl_sys_graphics_add_shader_uniform(const char *name);
extern int opengl_sys_graphics_add_shader_attribute(const char *name, int size);
extern void opengl_sys_graphics_set_shader_uniform_int(int uniform, int value);
extern void opengl_sys_graphics_set_shader_uniform_float(int uniform, float value);
extern void opengl_sys_graphics_set_shader_uniform_vec2(int uniform, const struct Vector2f *value);
extern void opengl_sys_graphics_set_shader_uniform_vec3(int uniform, const struct Vector3f *value);
extern void opengl_sys_graphics_set_shader_uniform_vec4(int uniform, const struct Vector4f *value);
extern void opengl_sys_graphics_set_shader_uniform_mat4(int uniform, const struct Matrix4f *value);
extern int opengl_sys_graphics_enable_shader_objects(void);
extern int opengl_sys_graphics_disable_shader_objects(void);

extern PURE_FUNCTION int opengl_sys_shader_background_compilation_supported(void);
extern void opengl_sys_shader_enable_get_binary(int enable);
extern PURE_FUNCTION int opengl_sys_shader_max_attributes(void);
extern int opengl_sys_shader_set_attribute(int index, const char *name);
extern void opengl_sys_shader_bind_standard_attribute(enum ShaderAttribute attribute, int index);
extern void opengl_sys_shader_clear_attributes(void);
extern struct OpenGLSysShader *opengl_sys_shader_create(enum ShaderType type, const void *data, int size, int is_binary);
extern void opengl_sys_shader_destroy(struct OpenGLSysShader *shader);
extern void *opengl_sys_shader_get_binary(struct OpenGLSysShader *shader, int *size_ret);
extern void *opengl_sys_shader_compile(enum ShaderType type, const char *source, int length, int *size_ret);
extern int opengl_sys_shader_get_uniform_id(struct OpenGLSysShader *shader, const char *name);
extern void opengl_sys_shader_set_uniform_int(struct OpenGLSysShader *shader, int uniform, int value);
extern void opengl_sys_shader_set_uniform_float(struct OpenGLSysShader *shader, int uniform, float value);
extern void opengl_sys_shader_set_uniform_vec2(struct OpenGLSysShader *shader, int uniform, const struct Vector2f *value);
extern void opengl_sys_shader_set_uniform_vec3(struct OpenGLSysShader *shader, int uniform, const struct Vector3f *value);
extern void opengl_sys_shader_set_uniform_vec4(struct OpenGLSysShader *shader, int uniform, const struct Vector4f *value);
extern void opengl_sys_shader_set_uniform_mat4(struct OpenGLSysShader *shader, int uniform, const struct Matrix4f *value);
extern struct OpenGLSysShaderPipeline *opengl_sys_shader_pipeline_create(struct OpenGLSysShader *vertex_shader, struct OpenGLSysShader *fragment_shader);
extern void opengl_sys_shader_pipeline_destroy(struct OpenGLSysShaderPipeline *pipeline);
extern void opengl_sys_shader_pipeline_apply(struct OpenGLSysShaderPipeline *pipeline);

extern struct OpenGLSysTexture *opengl_sys_texture_create(int width, int height, enum TextureFormat data_format, int num_levels, void *data, int stride, const int32_t *level_offsets, const int32_t *level_sizes, int mipmaps, int mem_flags, int reuse);
extern void opengl_sys_texture_destroy(struct OpenGLSysTexture *texture);
extern int opengl_sys_texture_width(struct OpenGLSysTexture *texture);
extern int opengl_sys_texture_height(struct OpenGLSysTexture *texture);
extern int opengl_sys_texture_has_mipmaps(struct OpenGLSysTexture *texture);
extern struct OpenGLSysTexture *opengl_sys_texture_grab(int x, int y, int w, int h, int readable, int mipmaps, int mem_flags);
extern void *opengl_sys_texture_lock(struct OpenGLSysTexture *texture, SysTextureLockMode lock_mode, int x, int y, int w, int h);
extern void opengl_sys_texture_unlock(struct OpenGLSysTexture *texture, int update);
extern void opengl_sys_texture_flush(struct OpenGLSysTexture *texture);
extern void opengl_sys_texture_set_repeat(struct OpenGLSysTexture *texture, int repeat_u, int repeat_v);
extern void opengl_sys_texture_set_antialias(struct OpenGLSysTexture *texture, int on);
extern void opengl_sys_texture_apply(int unit, struct OpenGLSysTexture *texture);
extern int opengl_sys_texture_num_units(void);

#endif  // SIL_OPENGL_NO_SYS_FUNCS

/*************************************************************************/
/*************************** Test control data ***************************/
/*************************************************************************/

#ifdef SIL_INCLUDE_TESTS

/**
 * TEST_opengl_force_feature_flags, TEST_opengl_force_feature_mask:  These
 * variables operate in tandem to force certain OpenGL feature flags on or
 * off regardless of what the system-specific code requests or the OpenGL
 * library reports.  When opengl_init() is called, the features parameter
 * (after addition of autodetected flags) is modified as follows:
 *
 *     features = ((features & ~TEST_opengl_force_feature_mask)
 *                 | TEST_opengl_force_feature_flags);
 *
 * Either of the following cases will trigger an assertion failure:
 *
 * - (TEST_opengl_force_feature_flags & ~TEST_opengl_force_feature_mask) != 0
 *   i.e., trying to set a flag bit not included in the mask
 *
 * - ((features & ..._feature_mask) ^ ..._feature_flags) != ..._feature_mask
 *   i.e., trying to set or clear a bit that is already in the target state
 */
extern uint32_t TEST_opengl_force_feature_flags;
extern uint32_t TEST_opengl_force_feature_mask;

/**
 * TEST_opengl_force_format_flags, TEST_opengl_force_format_mask:  As for
 * the *_feature_* versions, these allow specific format flags to be forced
 * on or off.
 */
extern uint32_t TEST_opengl_force_format_flags;
extern uint32_t TEST_opengl_force_format_mask;

/**
 * TEST_opengl_always_wrap_dsa:  If true, opengl_init() will always install
 * the non-DSA wrappers for direct state access functions, even if the
 * system supports DSA.
 */
extern uint8_t TEST_opengl_always_wrap_dsa;

/**
 * TEST_opengl_always_convert_texture_data:  If true, sys_texture_create()
 * will always use a pixel format conversion function when one exists for
 * the input texture data format.
 */
extern uint8_t TEST_opengl_always_convert_texture_data;

#endif

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_OPENGL_OPENGL_H
