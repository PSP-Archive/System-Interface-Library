/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/shader.h: Header for graphics shader management.
 */

/*
 * This header declares functions for creating and manipulating shader and
 * shader pipeline objects, which are used to render graphics when shader
 * objects have been enabled via graphics_use_shader_objects(1).
 *
 * ======== Overview =========
 *
 * Shader objects allow complete replacement of the standard SIL rendering
 * pipeline with OpenGL-style shader programs.  When the use of shader
 * objects has been enabled, client code is responsible for choosing an
 * appropriate shader before rendering each primitive (including text
 * rendering operations performed with font_render_text() and
 * text_display(); see <SIL/font.h> for details of how these functions
 * interact with shader objects).
 *
 * The SIL shader interface is loosely based on the shader pipeline design
 * introduced in OpenGL 4.1.  Each rendered primitive is passed to a
 * "shader pipeline object", which includes some number of "shader objects"
 * that process the primitive.  Currently, a valid shader pipeline must
 * include exactly two shaders: a vertex shader and a fragment shader.
 *
 * Each individual vertex and shader program is encapsulated by a "shader
 * object".  These objects are created by calling shader_create_from_source()
 * with the shader type (vertex or fragment) and source code; if the source
 * code compiles successfully, the function returns a shader object
 * containing that compiled program.  (On some systems, it is also possible
 * to load precompiled programs to avoid the overhead of compiling shaders
 * at runtime; see below.)  Shaders can be destroyed with shader_destroy()
 * when no longer needed.
 *
 * Once vertex and fragment shader objects have been created, they should
 * be linked into a shader pipeline object using shader_pipeline_create().
 * The returned shader pipeline ID can be passed to shader_pipeline_apply()
 * to enable the associated vertex and fragment shaders for rendering.
 * Shader pipelines can be destroyed with shader_pipeline_destroy() when
 * no longer needed (this does not destroy the associated shader objects
 * themselves).
 *
 * ======== Vertex attributes and uniforms =========
 *
 * A shader pipeline receives a sequence of vertices as input and produces
 * fragment (pixel) colors as output.  Each vertex is composed of one or
 * more attributes; for example, a basic vertex will consist of a single
 * vector-type attribute, namely the vertex's position.  The structure of
 * each vertex is defined in the same manner as for the standard SIL
 * rendering pipeline, by an array of vertex format declarations (see the
 * documentation for the GRAPHICS_VERTEX_* macros in <SIL/graphics.h>).
 * Since SIL has no way of knowing what sort of data is required by each
 * shader, all vertex attributes are declared using the generic ATTRIB_*
 * formats: ATTRIB_3F(n) for a 3-component vector or ATTRIB_4NUB(n) for a
 * 32-bit color value, for example.  The association between the attribute
 * index ("n" above) used by these format macros and attribute names used
 * in the shader's source code are defined with shader_set_attribute();
 * this function must be called for each attribute used by a vertex shader
 * before the shader is compiled (unless the association is defined in the
 * shader source code itself, such as with the GLSL 1.50 layout qualifier).
 *
 * It is also possible to bind specific attributes to the standard SIL
 * position, texture coordinate, and color attributes, by calling
 * shader_bind_standard_attribute(); this allows the use of SIL's
 * immediate-mode primitive specification (graphics_add_vertex()) and is
 * also required to render text using SIL's font system.
 *
 * Shaders can also include externally-specified constants, known as
 * "uniforms".  Unlike vertex attributes, uniforms do not need to be
 * declared before shader compilation; instead, the compiler will assign
 * each uniform an ID (OpenGL refers to this as "location"), and the ID
 * can be retrieved with shader_get_uniform_id() (as with attributes, this
 * step can be skipped if the shader uses layout qualifiers or similar
 * mechanisms to define explicit uniform IDs).  Uniform values can then be
 * set with the shader_set_uniform_*() function appropriate to the
 * uniform's type.
 *
 * ======== Textures =========
 *
 * With respect to shader objects, the texture formats TEX_FORMAT_A8 and
 * TEX_FORMAT_L8 are identical; in both cases, the single color value will
 * appear in the first ("r") component of the sampled texel.  The values of
 * the remaining components of the texel are undefined; shader programs
 * should _not_ rely on the default values specified by OpenGL.
 *
 * ======== Source code format ========
 *
 * Currently, all supported systems with shader functionality use OpenGL,
 * with the exception of Windows when the Direct3D backend is selected.
 *
 * Shader source code for OpenGL systems should be written in GLSL ES 1.00
 * style, using precision specifiers ("highp", "lowp", etc.) as needed,
 * with these two exceptions:
 *    - Code should use the "in"/"out" style of GLSL 1.30 / GLSL ES 3.00
 *      and later for declaring shader inputs and outputs, rather than
 *      "attribute" and "varying".
 *    - Fragment shader color output should be written to the "color_out"
 *      variable, rather than writing to gl_FragColor or declaring an
 *      output variable manually.
 * SIL will take care of covering the differences between standard OpenGL
 * and OpenGL ES, and between different GLSL versions, when compiling the
 * shader.
 *
 * When specifying precision for texture coordinates, shader code may use
 * the precision "texp", which will be defined to "highp" if the graphics
 * hardware supports it in fragment shaders and "mediump" otherwise.  This
 * allows texel-precise coordinates in large textures on modern hardware
 * while safely falling back (with a loss of image quality) on older
 * hardware.
 *
 * Shaders should not use a "#version" preprocessor directive; SIL will
 * add such a directive if appropriate.  (For desktop OpenGL, if the
 * runtime environment supports GLSL 1.50 or later, SIL will prepend
 * "#version 150" to the shader.)
 *
 * When using the Direct3D backend on Windows, shaders should be written
 * to target Shader Model 4.0, feature level 9_1.  It is not currently
 * possible to select a different feature level.  Vertex attributes are
 * passed using the semantic name "ATTRIBUTE<n>" (ATTRIBUTE0, ATTRIBUTE1,
 * and so on); the standard position, texture coordinate, and color
 * attributes (for shader_bind_standard_attribute()) are given the
 * semantic names POSITION, TEXCOORD, and COLOR.  Uniforms must be
 * declared in a cbuffer named "uniforms" in order to be detected by SIL.
 *
 * The following vertex data types are not natively supported by Direct3D:
 *    - GRAPHICS_VERTEX_POSITION_2S
 *    - GRAPHICS_VERTEX_ATTRIB_3UB
 *    - GRAPHICS_VERTEX_ATTRIB_3S
 *    - GRAPHICS_VERTEX_ATTRIB_3NUB
 *    - GRAPHICS_VERTEX_ATTRIB_3NS
 * SIL allows the use these data types with Direct3D, but if a primitive
 * uses any of these types, the vertex data must be copied and reformatted
 * when the primitive is created.  Similarly, Direct3D requires 4-byte
 * alignment for all data types, so 8- or 16-bit data which is not aligned
 * to a multiple of 4 bytes will trigger a vertex data copy.
 *
 * ======== Saving and reusing compiled shaders ========
 *
 * Since the cost of compiling shaders at runtime can be high enough to
 * cause visible impact such as dropped frames, SIL includes the ability to
 * compile shader programs to a binary representation which can then be
 * loaded into a shader object much more quickly.  (This requires support
 * from the runtime environment, and will not work on systems using older
 * versions of OpenGL, for example.)  To compile a shader into such a
 * binary representation, call shader_compile_to_binary() with the
 * shader's source code; the returned data can then be passed to
 * shader_create_from_binary() to create a shader object containing the
 * compiled program.  The compiled data can also be saved to external
 * storage (using userdata_save_data(), for example) and loaded on
 * subsequent runs to skip the compilation step altogether, provided the
 * runtime environment has not changed in a way that invalidates the format
 * of the compiled data.
 *
 * It is also possible to retrieve the compiled code for a shader after
 * the fact, by calling shader_get_binary().  However, some systems may
 * default to optimizing compiled shaders in a way that prevents this
 * function from working.  To ensure that compiled code is available, call
 * shader_enable_get_binary(1) before compiling any shaders for which you
 * want to retrieve the compiled code.
 *
 * On some systems, shaders can be compiled on a separate thread.  For
 * example, this can be used to hide the overhead of runtime shader
 * compilation by compiling shaders on a background thread while the
 * foreground thread displays a smooth animation.  The function
 * shader_background_compilation_supported() indicates whether this is
 * possible in the current runtime environment.  When using background
 * shader compilation, be aware of the following points:
 *
 * - SIL will _not_ explicitly prevent background compilation when not
 *   supported; it is the caller's responsibility to check whether
 *   background compilation is supported before attempting it.
 *
 * - Never attempt to compile a shader on a separate thread while the main
 *   thread is changing the display mode.
 *
 * - It is environment-dependent whether threads created before a display
 *   mode change can compile shaders after the change.  After changing the
 *   display mode, always terminate and recreate threads used for shader
 *   compilation.
 */

#ifndef SIL_SHADER_H
#define SIL_SHADER_H

EXTERN_C_BEGIN

/*************************************************************************/
/******************************* Constants *******************************/
/*************************************************************************/

/**
 * ShaderType:  Constants identifying types of shaders, used with the
 * shader creation functions.
 */
enum ShaderType {
    SHADER_TYPE_VERTEX = 0,
    SHADER_TYPE_FRAGMENT,
};
typedef enum ShaderType ShaderType;

/**
 * ShaderAttribute:  Constants identifying standard shader attributes used
 * by the default SIL rendering pipeline.
 */
enum ShaderAttribute {
    SHADER_ATTRIBUTE_POSITION = 0,
    SHADER_ATTRIBUTE_TEXCOORD,
    SHADER_ATTRIBUTE_COLOR,
};
typedef enum ShaderAttribute ShaderAttribute;

/*************************************************************************/
/****************** Interface: Shader object management ******************/
/*************************************************************************/

/**
 * shader_background_compilation_supported:  Return whether the platform
 * supports compiling shaders to binary code on alternate threads (see
 * the shader_compile_to_binary() documentation).
 *
 * Note that (like all other graphics parameters) the value returned by
 * this function may change after a call to graphics_set_display_mode().
 *
 * [Return value]
 *     True if background shader compilation is supported, false if not.
 */
extern PURE_FUNCTION int shader_background_compilation_supported(void);

/**
 * shader_enable_get_binary:  Indicate whether shader_get_binary() should
 * be supported for subsequently created shaders.  If a true value is
 * passed, shader_get_binary() will succeed on subsequently created shaders
 * (if the system supports it in the first place); otherwise, the behavior
 * of shader_get_binary() on such shaders is undefined.  Calling this
 * function has no effect on shaders created prior to the call.
 *
 * On some systems, the program must declare whether to make binary code
 * available for a shader program when the shader program is created.  By
 * not making this code available, the system may be able to save memory or
 * use a more efficient internal representation or storage method.  This
 * function allows the caller to make that declaration to the system.
 *
 * The default is that shader binary code will not be available, as if
 * shader_enable_get_binary(0) had been called at program startup time.
 *
 * [Parameters]
 *     enable: True to enable shader_get_binary() on subsequent shaders,
 *         false for system-default behavior.
 */
extern void shader_enable_get_binary(int enable);

/**
 * shader_max_attributes:  Return the maximum number of vertex attributes
 * that can be used in a single shader.  The maximum number of attributes
 * is always nonzero (positive) if shader objects are supported.
 *
 * [Return value]
 *     Maximum number of vertex attributes in a shader.
 */
extern PURE_FUNCTION int shader_max_attributes(void);

/**
 * shader_set_attribute:  Define an attribute index-to-name binding for
 * vertex shaders compiled with shader_create_from_source() or
 * shader_compile_to_binary(), such that using GRAPHICS_VERTEX_ATTRIB_*(index)
 * in a vertex format will cause the associated data to appear in the named
 * variable when the shader is executed.  The binding from index to name
 * remains active for all subsequent vertex shader compilations until the
 * index is either redefined (by calling this function with the same index
 * and a different name) or cleared (by calling shader_clear_attributes()).
 * It is an error to attempt to bind the same name to multiple attribute
 * indices.
 *
 * On failure, any previous binding of the given index is cleared.
 *
 * Attributes are saved as part of the compiled shader binary, so they do
 * not need to be set when loading a shader with shader_create_from_binary().
 *
 * It is not required to call this function before compiling a shader if
 * the shader source code defines attribute bindings itself (for example,
 * with the GLSL layout qualifier).  Note, however, that SIL only supports
 * 256 vertex attributes, and any attributes declared with an index of 256
 * or greater will not receive any data.
 *
 * [Parameters]
 *     index: Attribute index; must be in the range [0,255].
 *     name: Attribute name, or NULL to clear any existing binding.
 * [Return value]
 *     True on success, false on error.
 */
extern int shader_set_attribute(int index, const char *name);

/**
 * shader_bind_standard_attribute:  Define a binding from one of the
 * standard vertex attributes to a vertex shader attribute index for
 * vertex shaders created with shader_create_from_source() or
 * shader_create_from_binary().
 *
 * By calling this function, a program can associate the standard
 * attribute formats (GRAPHICS_VERTEX_FORMAT_POSITION_* and so on) with
 * specific vertex shader attribute indices, so that primitives containing
 * those format codes will have the corresponding data sent to the
 * specified shader attribute.  In particular, this allows the use of the
 * graphics_begin_primitive() / graphics_add_vertex() /
 * graphics_end_and_draw_primitive() set of functions for immediate
 * primitive drawing, since these functions are hardcoded to use the
 * standard attribute formats.
 *
 * For primitives created or drawn with graphics_create_primitive(),
 * graphics_draw_primitive(), and related functions, if the vertex format
 * contains both a standard vertex attribute and a shader vertex attribute
 * whose index is the same one to which the standard attribute is bound,
 * the data for the standard attribute is ignored and the shader vertex
 * attribute takes precedence.  For example, given the following vertex
 * format and attribute binding:
 *     shader_bind_standard_attribute(SHADER_ATTRIBUTE_POSITION, 0);
 *     typedef struct Vertex {
 *         Vector3f position1;
 *         Vector3f position2;
 *     } Vertex;
 *     uint32_t format[] = {
 *         GRAPHICS_VERTEX_FORMAT(ATTRIB_3F(0), offsetof(Vertex,position1)),
 *         GRAPHICS_VERTEX_FORMAT(POSITION_3F, offsetof(Vertex,position2)),
 *         0
 *     };
 * the vertex shader will receive the data from position1, and position2
 * will be ignored.
 *
 * Standard attribute bindings are stored in the created shader object, so
 * they do not need to be reapplied every time the shader is used.  Unlike
 * shader_set_attribute(), however, the bindings are not stored in compiled
 * binaries, so they must be active when creating the shader object with
 * shader_create_from_binary().
 *
 * This function always succeeds.  Passing an out-of-range index (negative
 * or greater than the maximum supported attribute index) disables any
 * existing binding and restores the default behavior of not passing data
 * for that standard attribute to vertex shaders.  Passing an invalid
 * attribute constant has no effect.
 *
 * [Parameters]
 *     attribute: Standard attribute to bind (SHADER_ATTRIBUTE_*).
 *     index: Vertex shader attribute index to bind to, or negative to
 *         cancel any existing binding.
 */
extern void shader_bind_standard_attribute(ShaderAttribute attribute,
                                           int index);

/**
 * shader_clear_attributes:  Clear all attributes previously defined with
 * shader_set_attribute() and all standard attribute bindings defined with
 * shader_set_standard_attribute().
 *
 * This function is implicitly called by graphics_use_shader_objects().
 */
extern void shader_clear_attributes(void);

/**
 * shader_create_from_source:  Create a new shader object by compiling it
 * from the given source code.  The format of the source code is
 * platform-dependent; for OpenGL-based platforms, it should be written as
 * for shader generator callbacks (see graphics_set_shader_generator()).
 *
 * If shader_compilation_supported() returns false, this function will
 * always fail.
 *
 * [Parameters]
 *     type: Type of shader to create (SHADER_TYPE_*).
 *     source: String containing shader source code.
 *     length: Length of source in bytes, or -1 if source is a
 *         null-terminated string.
 * [Return value]
 *     Shader ID (nonzero), or zero on error.
 */
extern int shader_create_from_source(ShaderType type, const char *source,
                                     int length);

/**
 * shader_create_from_binary:  Create a new shader object by loading
 * binary data representing the compiled shader code.  Typically, the
 * data will be obtained by calling shader_get_binary() or
 * shader_compile_to_binary(), but it can also be created using offline
 * shader compilers or similar tools; in that case, see the relevant
 * system-dependent source code, typically the sys_shder_get_binary()
 * implementation, for the proper format of the data buffer to pass to
 * this function.
 *
 * [Parameters]
 *     type: Type of shader to create (SHADER_TYPE_*).
 *     data: Buffer containing compiled shader program data.
 *     size: Size of data, in bytes.
 * [Return value]
 *     Shader ID (nonzero), or zero on error.
 */
extern int shader_create_from_binary(ShaderType type, const void *data,
                                     int size);

/**
 * shader_destroy:  Destroy a shader object.  Does nothing if shader == 0.
 *
 * If the given shader object is bound to a shader pipeline (see
 * shader_pipeline_create()), the shader pipeline object remains live,
 * but attempting to draw primitives with that pipeline applied results
 * in undefined behavior, as if no pipeline was applied.
 *
 * [Parameters]
 *     shader: ID of shader to destroy.
 */
extern void shader_destroy(int shader);

/**
 * shader_get_binary:  Return a buffer containing binary data which can
 * subsequently be passed to shader_create_from_binary().
 *
 * Note that there is generally no guarantee that a shader binary returned
 * by this function can be successfully loaded back into a new shader
 * object.  Even on the same machine, a change of display mode might
 * trigger a change in the display processing pipeline (a change from an
 * integrated GPU to a dedicated graphics card, for example).  Unless a
 * particular platform extrinsically guarantees binary compatibility,
 * callers should always be prepared for shader_create_from_binary() to
 * fail even when the data is valid.
 *
 * The returned buffer should be freed with mem_free() when it is no longer
 * needed.
 *
 * [Parameters]
 *     shader: ID of shader for which to retrieve compiled program data.
 *     size_ret: Pointer to variable to receive the size of the data, in bytes.
 * [Return value]
 *     Binary data representing the compiled shader program, or NULL on error.
 */
extern void *shader_get_binary(int shader, int *size_ret);

/**
 * shader_compile_to_binary:  Compile the given shader source code and
 * return a buffer containing binary data which can subsequently be passed
 * to shader_create_from_binary().  The returned buffer should be freed
 * with mem_free() when it is no longer needed.
 *
 * The caveats which apply to shader_get_binary() also apply to this
 * function.
 *
 * If shader_background_compilation_supported() returns true, then this
 * function may be safely called from any thread.  This can be used, for
 * example, to implement on-demand shader compilation without blocking the
 * rendering thread (as would happen with shader_create_from_source()).
 * Although it is safe to change the display mode while a background
 * compilation thread is live, the caller must ensure that the thread is
 * quiescent during the call to graphics_set_display_mode().  Changing the
 * display mode while a shader is actually being compiled results in
 * undefined behavior.
 *
 * If shader_background_compilation_supported() returns false, then (like
 * other graphics functions) calling this function from any thread other
 * than the main thread results in undefined behavior.
 *
 * [Parameters]
 *     type: Type of shader to create (SHADER_TYPE_*).
 *     source: String containing shader source code.
 *     length: Length of source in bytes, or -1 if source is a
 *         null-terminated string.
 *     size_ret: Pointer to variable to receive the size of the data, in bytes.
 * [Return value]
 *     Binary data representing the compiled shader program, or NULL on error.
 */
extern void *shader_compile_to_binary(ShaderType type, const char *source,
                                      int length, int *size_ret);

/**
 * shader_get_uniform_id:  Return a value identifying the given uniform in
 * the given shader, for use in shader_set_uniform_*() calls.
 *
 * On some platforms (notably OpenGL before 4.1 and OpenGL ES before 3.1),
 * it may not be possible to determine whether a uniform exists in a shader
 * before the shader is actually used for rendering.  In such cases, this
 * function will return a valid ID even for uniforms that do not exist, but
 * the values set for such uniforms will be ignored.  Note that each
 * uniform queried or set may take up additional memory even if the uniform
 * does not exist in the shader, since the names and values must be stored
 * until the uniforms can be looked up.
 *
 * [Parameters]
 *     shader: Shader ID.
 *     name: Name of uniform (e.g., variable name used in shader source code).
 * [Return value]
 *     Uniform ID, or zero if the uniform is not found.
 */
extern int shader_get_uniform_id(int shader, const char *name);

/**
 * shader_set_uniform_*:  Set the value of a shader uniform.  The data type
 * must match the type used in the shader, or undefined behavior results.
 *
 * The value of a texture sampler uniform is the index of the texture unit
 * it accesses (as in the unit parameter to texture_apply()), and the data
 * type is int for the purpose of these functions.
 *
 * For any two shaders used in a pipeline, if both shaders declare a
 * uniform of the same name, it is undefined whether they share storage
 * (and thus always have the same value) or have separate storage (and
 * thus independent values).  To be safe, always ensure that same-named
 * uniforms in a vertex/fragment shader pair are set to the same value.
 *
 * For shader_set_uniform_mat4(), SIL assumes that the matrix elements are
 * stored in row-major order (as for other SIL functions) and transfers
 * the data to the shader so as to preserve that order.  In some cases
 * (such as when accessing the individual rows of a matrix in GLSL), it
 * can be more convenient to transpose rows and columns; to do this, call
 * mat4_transpose() to transpose the matrix into a temporary variable and
 * pass the transposed matrix to shader_set_uniform_mat4().
 *
 * [Parameters]
 *     shader: Shader ID.
 *     uniform: Uniform ID, as returned from shader_get_uniform_id().
 *     value: Value to set.
 */
extern void shader_set_uniform_int(int shader, int uniform, int value);
extern void shader_set_uniform_float(int shader, int uniform, float value);
extern void shader_set_uniform_vec2(
    int shader, int uniform, const struct Vector2f *value);
extern void shader_set_uniform_vec3(
    int shader, int uniform, const struct Vector3f *value);
extern void shader_set_uniform_vec4(
    int shader, int uniform, const struct Vector4f *value);
extern void shader_set_uniform_mat4(
    int shader, int uniform, const struct Matrix4f *value);

/*************************************************************************/
/***************** Interface: Shader pipeline management *****************/
/*************************************************************************/

/**
 * shader_pipeline_create:  Create a new shader pipeline containing the
 * given shaders.  A shader pipeline encapsulates a vertex/fragment shader
 * pair used to render primitives, and is equivalent to (for example) an
 * OpenGL 4.1 program pipeline.
 *
 * On systems which do not support independent vertex and fragment shader
 * programs, this creates and links a new shader program containing the
 * given vertex and fragment shaders, so the same interface may be used
 * without regard to system-dependent details.  Note, however, that on
 * such systems, uniform updates can be expensive because each update must
 * be propagated to all shader programs which use the updated shader.
 *
 * [Parameters]
 *     vertex_shader: ID of vertex shader to use in pipeline.
 *     fragment_shader: ID of fragment shader to use in pipeline.
 * [Return value]
 *     Shader pipeline ID (nonzero), or zero on error.
 */
extern int shader_pipeline_create(int vertex_shader, int fragment_shader);

/**
 * shader_pipeline_destroy:  Destroy a shader pipeline.  Does nothing if
 * pipeline == 0.
 *
 * Destroying the current shader pipeline (as set with shader_pipeline_apply())
 * causes it to be unbound from the current render state, as if
 * shader_pipeline_apply(0) had been called.
 *
 * [Parameters]
 *     pipeline: ID of shader pipeline to destroy.
 */
extern void shader_pipeline_destroy(int pipeline);

/**
 * shader_pipeline_apply:  Use the given shader pipeline for subsequent
 * draw operations.
 *
 * It is permitted to pass zero, which removes any currently applied
 * shader pipeline, but attempting to draw primitives without a shader
 * pipeline applied (assuming shader objects have been enabled with
 * graphics_use_shader_objects()) results in undefined behavior.
 *
 * [Parameters]
 *     pipeline: ID of shader pipeline to apply, or zero to remove the
 *         currently applied shader pipeline.
 */
extern void shader_pipeline_apply(int pipeline);

/*************************************************************************/
/***************************** C++ wrappers ******************************/
/*************************************************************************/

#ifdef __cplusplus
EXTERN_C_END

/*
 * The following macro wraps a function taking an input pointer parameter
 * such that the function can also be called (from C++ only) using a
 * non-pointer value of the same type.  For example:
 *    void shader_set_uniform_mat4(const struct Matrix4f *matrix)
 * is wrapped by:
 *    void shader_set_uniform_mat4(const struct Matrix4f &matrix)
 * allowing a call of the format:
 *    shader_set_uniform_mat4(Matrix4f(...));
 *
 * This is intended mainly to work around the lack of C99-style compound
 * literals in C++.
 */

#define WRAP_FUNCTION_2(NAME,TYPE0,TYPE1,TYPE) \
    static inline void NAME(TYPE0 arg0, TYPE1 arg1, const struct TYPE &value) {NAME(arg0, arg1, &value);}

WRAP_FUNCTION_2(shader_set_uniform_vec2, int, int, Vector2f)
WRAP_FUNCTION_2(shader_set_uniform_vec3, int, int, Vector3f)
WRAP_FUNCTION_2(shader_set_uniform_vec4, int, int, Vector4f)
WRAP_FUNCTION_2(shader_set_uniform_mat4, int, int, Matrix4f)

#undef WRAP_FUNCTION_2

EXTERN_C_BEGIN
#endif  // __cplusplus

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_SHADER_H
