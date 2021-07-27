/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/ge-util.h: Header for the GE utility library.
 */

/*
 * This library provides an interface to the PSP's rendering hardware (GE)
 * which meshes more closely with the actual hardware functionality than
 * the standard sceGu library.  This library is also optimized for high
 * performance, particularly when dynamically generating display lists.
 */

#ifndef SIL_SRC_SYSDEP_PSP_GE_UTIL_H
#define SIL_SRC_SYSDEP_PSP_GE_UTIL_H

struct Matrix4f;

/*************************************************************************/
/****************** GE-related data types and constants ******************/
/*************************************************************************/

/**
 * GEBlendFunc:  Constants for blend functions.
 */
typedef enum GEBlendFunc {
    GE_BLEND_ADD              = 0,  // Cs*Bs + Cd*Bd
    GE_BLEND_SUBTRACT         = 1,  // Cs*Bs - Cd*Bd
    GE_BLEND_REVERSE_SUBTRACT = 2,  // Cd*Bd - Cs*Bs
    GE_BLEND_MIN              = 3,  // min(Cs,Cd)
    GE_BLEND_MAX              = 4,  // max(Cs,Cd)
    GE_BLEND_ABS              = 5,  // |Cs-Cd|
} GEBlendFunc;

/**
 * GEBlendParam:  Constants for blend function parameters (source/destination).
 */
typedef enum GEBlendParam {
    GE_BLEND_COLOR               = 0,
    GE_BLEND_ONE_MINUS_COLOR     = 1,
    GE_BLEND_SRC_ALPHA           = 2,
    GE_BLEND_ONE_MINUS_SRC_ALPHA = 3,
    GE_BLEND_DST_ALPHA           = 4,
    GE_BLEND_ONE_MINUS_DST_ALPHA = 5,
    GE_BLEND_FIX                 = 10, // Fixed value (i.e., constant)
} GEBlendParam;

/**
 * GECopyMode:  Copy unit size for ge_copy().
 */
typedef enum GECopyMode {
    GE_COPY_16BIT = 0,
    GE_COPY_32BIT = 1,
} GECopyMode;

/**
 * GECullMode:  Face culling mode for ge_set_cull_mode().
 */
typedef enum GECullMode {
    GE_CULL_NONE = 0,   // Don't cull anything.
    GE_CULL_CW,         // Cull faces whose vertices are in clockwise order.
    GE_CULL_CCW,        // Cull faces whose vertices are counterclockwise.
} GECullMode;

/**
 * GELightComponent:  Light component types.
 */
typedef enum GELightComponent {
    GE_LIGHT_COMPONENT_AMBIENT = 0,
    GE_LIGHT_COMPONENT_DIFFUSE,
    GE_LIGHT_COMPONENT_SPECULAR,
} GELightComponent;

/**
 * GELightMode:  Light color modes for ge_set_light_mode().
 */
typedef enum GELightMode {
    GE_LIGHT_MODE_SINGLE_COLOR = 0,
    GE_LIGHT_MODE_SEPARATE_SPECULAR_COLOR,
} GELightMode;

/**
 * GELightType:  Light types for ge_set_light_type().
 */
typedef enum GELightType {
    GE_LIGHT_TYPE_DIRECTIONAL = 0,
    GE_LIGHT_TYPE_POINT_LIGHT,
    GE_LIGHT_TYOE_SPOTLIGHT,
} GELightType;

/**
 * GEMipmapMode:  Mipmap selection modes.
 *
 * Note: Due to an apparent hardware bug in the PSP, using AUTO mode can
 * cause some triangles to use a higher mipmap level (lower resolution)
 * than appropriate, particularly as the triangle approaches perpendicular
 * to the plane of the screen.  For this reason, it is recommended to use
 * a negative mipmap bias with AUTO mode.
 */
typedef enum GEMipmapMode {
    GE_MIPMAPMODE_AUTO  = 0,  // Automatic selection (see note above)
    GE_MIPMAPMODE_CONST = 1,  // Constant level (bias value)
    GE_MIPMAPMODE_SLOPE = 2,  // Select by distance from and angle to screen
} GEMipmapMode;

/**
 * GEPixelFormat:  Pixel formats for display data.
 */
typedef enum GEPixelFormat {
    GE_PIXFMT_5650 = 0,  // 16bpp (R:5 G:6 B:5 A:0)
    GE_PIXFMT_5551 = 1,  // 16bpp (R:5 G:5 B:5 A:1)
    GE_PIXFMT_4444 = 2,  // 16bpp (R:4 G:4 B:4 A:4)
    GE_PIXFMT_8888 = 3,  // 32bpp (R:8 G:8 B:8 A:8)
} GEPixelFormat;

/**
 * GEPrimitive:  Primitive types.
 */
typedef enum GEPrimitive {
    GE_PRIMITIVE_POINTS         = 0, // Individual points
    GE_PRIMITIVE_LINES          = 1, // Individual lines (vertices 01, 23...)
    GE_PRIMITIVE_LINE_STRIP     = 2, // Connected lines (vertices 01, 12...)
    GE_PRIMITIVE_TRIANGLES      = 3, // Individual triangles (012, 345, 678...)
    GE_PRIMITIVE_TRIANGLE_STRIP = 4, // Connected triangles (012, 213, 234...)
    GE_PRIMITIVE_TRIANGLE_FAN   = 5, // Connected triangles (012, 123, 234...)
    GE_PRIMITIVE_SPRITES        = 6, // Axis-aligned rectangles (2 verts each)
} GEPrimitive;

/**
 * GEShadeMode:  Shading modes.
 */
typedef enum GEShadeMode {
    GE_SHADE_FLAT    = 0,  // Single-color (no shading)
    GE_SHADE_GOURAUD = 1,  // Smooth shading between vertices
} GEShadeMode;

/**
 * GEState:  State constants for ge_enable() and ge_disable().
 */
typedef enum GEState {
    GE_STATE_LIGHTING,
    GE_STATE_CLIP_PLANES,
    GE_STATE_TEXTURE,
    GE_STATE_FOG,
    GE_STATE_DITHER,
    GE_STATE_BLEND,
    GE_STATE_ALPHA_TEST,
    GE_STATE_DEPTH_TEST,
    GE_STATE_DEPTH_WRITE,
    GE_STATE_STENCIL_TEST,
    GE_STATE_ANTIALIAS,
    GE_STATE_PATCH_CULL_FACE,
    GE_STATE_COLOR_TEST,
    GE_STATE_COLOR_LOGIC_OP,
    GE_STATE_REVERSE_NORMALS,
} GEState;

/**
 * GEStencilOp:  Stencil operations.
 */
typedef enum GEStencilOp {
    GE_STENCIL_KEEP    = 0,
    GE_STENCIL_ZERO    = 1,
    GE_STENCIL_REPLACE = 2,
    GE_STENCIL_INVERT  = 3,
    GE_STENCIL_INCR    = 4,
    GE_STENCIL_DECR    = 5,
} GEStencilOp;

/**
 * GETestFunc:  Comparison functions for rendering test operations.
 */
typedef enum GETestFunc {
    GE_TEST_NEVER    = 0,
    GE_TEST_ALWAYS   = 1,
    GE_TEST_EQUAL    = 2,
    GE_TEST_NOTEQUAL = 3,
    GE_TEST_LESS     = 4,
    GE_TEST_LEQUAL   = 5,
    GE_TEST_GREATER  = 6,
    GE_TEST_GEQUAL   = 7,
} GETestFunc;

/**
 * GETexelFormat:  Texture data formats.
 */
typedef enum GETexelFormat {
    GE_TEXFMT_5650 = GE_PIXFMT_5650,
    GE_TEXFMT_5551 = GE_PIXFMT_5551,
    GE_TEXFMT_4444 = GE_PIXFMT_4444,
    GE_TEXFMT_8888 = GE_PIXFMT_8888,
    GE_TEXFMT_T4   = 4,  // 4bpp CLUT
    GE_TEXFMT_T8   = 5,  // 8bpp CLUT
    GE_TEXFMT_T16  = 6,  // 16bpp CLUT
    GE_TEXFMT_T32  = 7,  // 32bpp CLUT
    GE_TEXFMT_DXT1 = 8,  // DXT1-compressed
    GE_TEXFMT_DXT3 = 9,  // DXT3-compressed
    GE_TEXFMT_DXT5 = 10, // DXT5-compressed
} GETexelFormat;

/**
 * GETextureDrawMode:  Texture drawing modes.  The symbols below mean:
 *   Cv, Av: Color and alpha value of the result.
 *   Cf, Af: Color and alpha value of the pre-texturing fragment.
 *   Ct, At: Color and alpha value of the texture data to be applied.
 *   Cc    : Constant color value set with ge_set_texture_color().
 */
typedef enum GETextureDrawMode {
    GE_TEXDRAWMODE_MODULATE = 0,  // Cv = Cf*Ct           | Av = Af*At
    GE_TEXDRAWMODE_DECAL    = 1,  // Cv = Cf*(1-At)+Ct*At | Av = Af
    GE_TEXDRAWMODE_BLEND    = 2,  // Cv = Cf*(1-Ct)+Cc*Ct | Av = Af*At
    GE_TEXDRAWMODE_REPLACE  = 3,  // Cv = Ct              | Av = At
    GE_TEXDRAWMODE_ADD      = 4,  // Cv = Cf+Ct           | Av = Af*At
} GETextureDrawMode;

/**
 * GETextureFilter:  Texture minification/magnification filter types.
 */
typedef enum GETextureFilter {
    GE_TEXFILTER_NEAREST = 0,
    GE_TEXFILTER_LINEAR  = 1,
} GETextureFilter;

/**
 * GETextureMipFilter:  Texture mipmap minification filter types.
 */
typedef enum GETextureMipFilter {
    GE_TEXMIPFILTER_NONE    = 0,
    GE_TEXMIPFILTER_NEAREST = 4,
    GE_TEXMIPFILTER_LINEAR  = 6,
} GETextureMipFilter;

/**
 * GETextureMapMode: Texture coordinate mapping modes.
 */
typedef enum GETextureMapMode {
    GETEXMAPMODE_TEXTURE_COORDS  = 0,  // Texture matrix disabled
    GETEXMAPMODE_TEXTURE_MATRIX  = 1,  // Texture matrix enabled
    GETEXMAPMODE_ENVIRONMENT_MAP = 2,  // Environment mapping mode
} GETextureMapMode;

/**
 * GETextureMapSource: Sources for texture coordinate mapping.
 */
typedef enum GETextureMapSource {
    GETEXMAPSRC_POSITION    = 0,  // Vertex coordinates
    GETEXMAPSRC_TEXCOORD    = 1,  // Texture coordinates
    GETEXMAPSRC_NORMAL_UNIT = 2,  // Normalized normal vector
    GETEXMAPSRC_NORMAL      = 3,  // Unmodified normal vector
} GETextureMapSource;

/**
 * GETextureWrapMode:  Texture coordinate wrapping modes.
 */
typedef enum GETextureWrapMode {
    GE_TEXWRAPMODE_REPEAT = 0,
    GE_TEXWRAPMODE_CLAMP  = 1,
} GETextureWrapMode;

/*-----------------------------------------------------------------------*/

/**
 * GE_BLENDSET_*:  ge_set_blend_mode() parameter sets for commonly used
 * blending modes.  Use as: ge_set_blend_mode(GE_BLENDSET_*)
 */

/* Ordinary alpha blending. */
#define GE_BLENDSET_SRC_ALPHA \
    GE_BLEND_ADD, GE_BLEND_SRC_ALPHA, GE_BLEND_ONE_MINUS_SRC_ALPHA, 0, 0

/* Alpha blending with a fixed alpha value applied to the source.
 * The alpha parameter must be between 0 and 255 inclusive and must not
 * have any side effects. */
#define GE_BLENDSET_FIXED_ALPHA(alpha) \
    GE_BLEND_ADD, GE_BLEND_FIX, GE_BLEND_FIX, \
    (alpha) * 0x010101, (255-(alpha)) * 0x010101

/*-----------------------------------------------------------------------*/

/**
 * GE_VERTEXFMT_*:  Format constants for vertex data.  Choose exactly one
 * vertex coordinate type (GE_VERTEXFMT_VERTEX_*) and at most one value
 * from each other group, and bitwise-OR the values together to create the
 * vertex format value.
 *
 * The order of data for each vertex is:
 *    - Texture coordinates (U, V)
 *    - Vertex color
 *    - Normal vector (X, Y, Z)
 *    - Vertex coordinates (X, Y, Z)
 * Vertex alignment follows standard C rules, treating the color as a
 * single 16-bit or 32-bit integer.
 */

/* Texture coordinate formats. */
#define GE_VERTEXFMT_TEXTURE_8BIT   (1<<0)  // Signed 8-bit integer
#define GE_VERTEXFMT_TEXTURE_16BIT  (2<<0)  // Signed 16-bit integer
#define GE_VERTEXFMT_TEXTURE_32BITF (3<<0)  // Single-precision floating point
#define GE_VERTEXFMT_TEXTURE_MASK   (3<<0)

/* Color formats (encoded identically to the corresponding pixel data
 * formats). */
#define GE_VERTEXFMT_COLOR_5650     (4<<2)
#define GE_VERTEXFMT_COLOR_5551     (5<<2)
#define GE_VERTEXFMT_COLOR_4444     (6<<2)
#define GE_VERTEXFMT_COLOR_8888     (7<<2)
#define GE_VERTEXFMT_COLOR_MASK     (7<<2)

/* Normal vector component formats. */
#define GE_VERTEXFMT_NORMAL_8BIT    (1<<5)  // Signed 8-bit integer
#define GE_VERTEXFMT_NORMAL_16BIT   (2<<5)  // Signed 16-bit integer
#define GE_VERTEXFMT_NORMAL_32BITF  (3<<5)  // Single-precision floating point
#define GE_VERTEXFMT_NORMAL_MASK    (3<<5)

/* Vertex coordinate formats. */
#define GE_VERTEXFMT_VERTEX_8BIT    (1<<7)  // Signed 8-bit integer
#define GE_VERTEXFMT_VERTEX_16BIT   (2<<7)  // Signed 16-bit integer
#define GE_VERTEXFMT_VERTEX_32BITF  (3<<7)  // Single-precision floating point
#define GE_VERTEXFMT_VERTEX_MASK    (3<<7)

/* Index data formats (index values are stored in a separate buffer). */
#define GE_VERTEXFMT_INDEX_8BIT     (1<<11) // Unsigned 8-bit integer
#define GE_VERTEXFMT_INDEX_16BIT    (2<<11) // Unsigned 16-bit integer
#define GE_VERTEXFMT_INDEX_MASK     (3<<11)

/* Vertex coordinate transformation modes. */
#define GE_VERTEXFMT_TRANSFORM_3D   (0<<23) // Standard 3D transformation
#define GE_VERTEXFMT_TRANSFORM_2D   (1<<23) // No transformation

/*************************************************************************/
/************************* Function declarations *************************/
/*************************************************************************/

/*------------------ Basic operations (ge-util/base.c) ------------------*/

/**
 * ge_init:  Initialize the GE.
 *
 * [Return value]
 *     True on success, false on error.
 */
extern int ge_init(void);

/**
 * ge_start_frame:  Set up for drawing a new frame.
 *
 * [Parameters]
 *     display_mode: Pixel format for display (GE_PIXFMT_*, or a negative
 *         value to leave unchanged).
 */
extern void ge_start_frame(int display_mode);

/**
 * ge_end_frame:  Finish drawing the current frame.
 */
extern void ge_end_frame(void);

/**
 * ge_commit:  Tell the GE to begin execution of all commands entered into
 * the display list before this call.
 */
extern void ge_commit(void);

/**
 * ge_sync:  Wait until all currently executing commands have completed.
 */
extern void ge_sync(void);

#ifdef DEBUG
/**
 * ge_get_debug_info:  Retrieve debugging information (list usage data).
 *
 * [Parameters]
 *     gelist_used_ret, gelist_used_max_ret, gelist_size_ret,
 *     vertlist_used_ret, vertlist_used_max_ret, vertlist_size_ret:
 *         Pointers to variables to receive debug data.
 */
extern void ge_get_debug_info(int *gelist_used_ret, int *gelist_used_max_ret,
                              int *gelist_size_ret, int *vertlist_used_ret,
                              int *vertlist_used_max_ret, int *vertlist_size_ret);
#endif

/*----------------- Drawing functions (ge-util/draw.c) ------------------*/

/**
 * ge_set_draw_buffer:  Set the buffer into which to draw.  buffer == NULL
 * selects the current frame's work buffer.
 *
 * [Parameters]
 *     buffer: Draw buffer pointer (must be 64-byte aligned).
 *     stride: Draw buffer stride, in pixels.
 */
extern void ge_set_draw_buffer(void *buffer, int stride);

/**
 * ge_set_depth_buffer:  Set the depth buffer.  buffer == NULL selects
 * the default (screen-sized) depth buffer.
 *
 * [Parameters]
 *     buffer: Depth buffer pointer (must be 64-byte aligned).
 *     stride: Depth buffer stride, in pixels.
 */
extern void ge_set_depth_buffer(void *buffer, int stride);

/**
 * ge_set_index_pointer:  Set the index buffer pointer for primitive
 * rendering.  The buffer is only used if an index data format
 * (GE_VERTEXFMT_INDEX_*) is specified in the vertex format value, so
 * there is no need to set this buffer if index data is not being used.
 *
 * [Parameters]
 *     ptr: Index data pointer (must not be NULL).
 */
extern void ge_set_index_pointer(const void *ptr);

/**
 * ge_set_vertex_format:  Set the vertex data format for primitive rendering.
 *
 * [Parameters]
 *     format: Vertex data format (GE_VERTEXFMT_*).
 */
extern void ge_set_vertex_format(uint32_t format);

/**
 * ge_set_vertex_pointer:  Set the vertex buffer pointer for primitive
 * rendering.  ptr == NULL selects the internal buffer used by the
 * ge_add_*_vertex() functions; to use this buffer, call this function
 * _before_ adding any vertices.
 *
 * [Parameters]
 *     ptr: Vertex buffer pointer.
 */
extern void ge_set_vertex_pointer(const void *ptr);

/**
 * ge_draw_primitive:  Render primitives as specified by the parameters and
 * the current vertex/index buffer settings.  After this function returns,
 * the vertex buffer pointer for non-indexed vertices or the index buffer
 * pointer for indexed vertices are updated to point to the vertex or index
 * immediately following the last one rendered.
 *
 * [Parameters]
 *     primitive: Primitive type (GE_PRIMITIVE_*).
 *     num_vertices: Number of vertices to render.
 */
extern void ge_draw_primitive(GEPrimitive primitive, uint16_t num_vertices);

/*---------- Light source control functions (ge-util/light.c) -----------*/

/**
 * ge_set_light_mode:  Set the light effect mode.
 *
 * [Parameters]
 *     mode: Light effect mode (GU_LIGHT_MODE_*).
 */
extern void ge_set_light_mode(unsigned int mode);

/**
 * ge_enable_light:  Enable the given light source.
 *
 * [Parameters]
 *     light: Index of light source to enable (0-3).
 */
extern void ge_enable_light(unsigned int light);

/**
 * ge_disable_light:  Disable the given light source.
 *
 * [Parameters]
 *     light: Index of light source to disable (0-3).
 */
extern void ge_disable_light(unsigned int light);

/**
 * ge_set_light_type:  Set the lighting type for the given light source.
 *
 * [Parameters]
 *     light: Light source index (0-3).
 *     type: Lighting type (GE_LIGHT_TYPE_*).
 *     has_specular: True to enable specular lighting.
 */
extern void ge_set_light_type(unsigned int light, GELightType type,
                              int has_specular);

/**
 * ge_set_light_position:  Set the position of the given light source.
 *
 * [Parameters]
 *     light: Light source index (0-3).
 *     x, y, z: Light source position.
 */
extern void ge_set_light_position(unsigned int light, float x, float y, float z);

/**
 * ge_set_light_direction:  Set the direction of the given light source.
 *
 * [Parameters]
 *     light: Light source index (0-3).
 *     x, y, z: Light direction.
 */
extern void ge_set_light_direction(unsigned int light, float x, float y, float z);

/**
 * ge_set_light_attenuation:  Set the attenuation parameters for the given
 * light source.
 *
 * [Parameters]
 *     light: Light source index (0-3).
 *     constant: Constant attenuation factor.
 *     linear: Linear attenuation factor.
 *     quadratic: Quadratic attenuation factor.
 */
extern void ge_set_light_attenuation(unsigned int light, float constant,
                                     float linear, float quadratic);

/**
 * ge_set_light_color:  Set the color of the given light source.
 *
 * [Parameters]
 *     light: Light source index (0-3).
 *     component: Light component to modify (GE_LIGHT_COMPONENT_*).
 *     color: Light color (0xBBGGRR).
 */
extern void ge_set_light_color(unsigned int light, unsigned int component,
                               uint32_t color);

/**
 * ge_set_spotlight_exponent:  Set the spotlight exponent for the given
 * light source.
 *
 * [Parameters]
 *     light: Light source index (0-3).
 *     exponent: Spotlight exponent.
 */
extern void ge_set_spotlight_exponent(unsigned int light, float exponent);

/**
 * ge_set_spotlight_cutoff:  Set the spotlight cutoff value for the given
 * light source.
 *
 * [Parameters]
 *     light: Light source index (0-3).
 *     cutoff: Spotlight cutoff value.
 */
extern void ge_set_spotlight_cutoff(unsigned int light, float cutoff);

/*--------- Display list management functions (ge-util/list.c) ----------*/

/**
 * ge_add_command, ge_add_commandf:  Append an arbitrary GE command to the
 * current display list.  Use ge_add_command() for a command that takes an
 * integer parameter, and ge_add_commandf() for a command that takes a
 * floating-point parameter.
 *
 * [Parameters]
 *     command: GE opcode (0-255).
 *     parameter: Command parameter (24-bit integer or floating point value).
 */
extern void ge_add_command(uint8_t command, uint32_t parameter);
extern void ge_add_commandf(uint8_t command, float parameter);

/**
 * ge_start_sublist:  Start creating a display sublist.
 *
 * [Parameters]
 *     list: Sublist buffer pointer.
 *     size: Size of sublist buffer, in 32-bit words.
 * [Return value]
 *     True on success, false on error (i.e., a sublist is already selected).
 */
extern int ge_start_sublist(uint32_t *list, int size);

/**
 * ge_replace_sublist:  Replace the buffer pointer for the current sublist
 * with the given pointer.  The current position in the list relative to
 * the start of the buffer is maintained.  This function is intended to be
 * used when the list buffer has been reallocated to a larger size and
 * consequently moved to a different address in memory.
 *
 * This function does nothing if no sublist is currently being created.
 *
 * [Parameters]
 *     list: New sublist buffer pointer.
 *     size: New size of sublist buffer, in 32-bit words.
 */
extern void ge_replace_sublist(uint32_t *list, int size);

/**
 * ge_finish_sublist:  Terminate the current sublist and reselect the main
 * display list as the current list.  This function does nothing and
 * returns NULL if no sublist is currently being created.)
 *
 * [Return value]
 *     Pointer to one word past the last command in the list.
 */
extern uint32_t *ge_finish_sublist(void);

/**
 * ge_call_sublist:  Call a sublist.
 *
 * [Parameters]
 *     list: Sublist pointer.
 */
extern void ge_call_sublist(const uint32_t *list);

/**
 * ge_sublist_free:  Return the number of free words in the current sublist.
 *
 * [Return value]
 *     Number of free words, or zero if no sublist is currently being created.
 */
extern uint32_t ge_sublist_free(void);

/*---- Coordinate transformation matrix functions (ge-util/matrix.c) ----*/

/**
 * ge_set_projection_matrix:  Set the projection transformation matrix.
 *
 * [Parameters]
 *     matrix: New projection transformation matrix.
 */
extern void ge_set_projection_matrix(const struct Matrix4f *matrix);

/**
 * ge_set_view_matrix:  Set the view transformation matrix.  The fourth
 * column of the matrix is fixed at [0,0,0,1].
 *
 * [Parameters]
 *     matrix: New view transformation matrix.
 */
extern void ge_set_view_matrix(const struct Matrix4f *matrix);

/**
 * ge_set_model_matrix:  Set the model transformation matrix.  The fourth
 * column of the matrix is fixed at [0,0,0,1].
 *
 * [Parameters]
 *     matrix: New model transformation matrix.
 */
extern void ge_set_model_matrix(const struct Matrix4f *matrix);

/**
 * ge_set_texture_matrix:  Set the texture transformation matrix.  The
 * fourth column of the matrix is fixed at [0,0,0,1].
 *
 * [Parameters]
 *     matrix: New texture transformation matrix.
 */
extern void ge_set_texture_matrix(const struct Matrix4f *matrix);

/*---------- Render state control functions (ge-util/state.c) -----------*/

/**
 * ge_enable:  Enable the given rendering state.
 *
 * [Parameters]
 *     state: State to enable (GE_STATE_*).
 */
extern void ge_enable(GEState state);

/**
 * ge_disable:  Disable the given rendering state.
 *
 * [Parameters]
 *     state: State to disable (GE_STATE_*).
 */
extern void ge_disable(GEState state);

/**
 * ge_set_alpha_mask:  Set the bitmask for writing alpha data.
 *
 * [Parameters]
 *     mask: Write mask (0xFF to disable writing to all bits).
 */
extern void ge_set_alpha_mask(uint8_t mask);

/**
 * ge_set_alpha_test:  Set the alpha test function and reference value.
 *
 * [Parameters]
 *     test: Alpha test function (GE_TEST_*).
 *     ref: Reference value (0-255).
 */
extern void ge_set_alpha_test(GETestFunc test, uint8_t ref);

/**
 * ge_set_ambient_color:  Set the ambient color for rendering.
 *
 * [Parameters]
 *     color: Ambient color (0xAABBGGRR).
 */
extern void ge_set_ambient_color(uint32_t color);

/**
 * ge_set_ambient_light:  Set the ambient light color for rendering..
 *
 * [Parameters]
 *     color: Ambient light color (0xAABBGGRR).
 */
extern void ge_set_ambient_light(uint32_t color);

/**
 * ge_set_blend_mode:  Set the blend mode and associated parameters.
 *
 * Note that this function only updates the GE register for src_fix (resp.
 * dst_fix) if src_param (resp. dst_param) is GE_BLEND_FIX.
 *
 * [Parameters]
 *     func: Blend function (GE_BLEND_ADD, etc.).
 *     src_param: Source blend parameter (GE_BLEND_COLOR, etc.).
 *     dst_param: Destination blend parameter (GE_BLEND_COLOR, etc.).
 *     src_fix: Constant value for src when src_param == GE_BLEND_FIX.
 *     dst_fix: Constant value for dst when dst_param == GE_BLEND_FIX.
 */
extern void ge_set_blend_mode(GEBlendFunc func,
                              GEBlendParam src_param, GEBlendParam dst_param,
                              uint32_t src_fix, uint32_t dst_fix);

/**
 * ge_set_clip_area, ge_unset_clip_area:  Set or unset the clipping area.
 * Note that (x1,y1) is included within the clipping area.
 *
 * [Parameters]
 *     x0, y0: Upper-left coordinates of clipping region, in pixels.
 *     x1, y1: Lower-right coordinates of clipping region, in pixels.
 */
extern void ge_set_clip_area(int x0, int y0, int x1, int y1);
extern void ge_unset_clip_area(void);

/**
 * ge_set_color_mask:  Set the bitmask for writing color data.
 *
 * [Parameters]
 *     mask: Write mask (0xFFFFFF to disable writing to all bits).
 */
extern void ge_set_color_mask(uint32_t mask);

/**
 * ge_set_cull_mode:  Set the face culling mode.
 *
 * [Parameters]
 *     mode: Face culling mode (GE_CULL_*).
 */
extern void ge_set_cull_mode(GECullMode mode);

/**
 * ge_set_depth_test:  Set the depth test function.
 *
 * [Parameters]
 *     test: Depth test function (GE_TEST_*).
 */
extern void ge_set_depth_test(GETestFunc test);

/**
 * ge_set_depth_range:  Set the range of depth buffer values to map to
 * transformed depth coordinates.  The default is near==65535, far==0
 * (note that the GE treats larger values as nearer).
 *
 * [Parameters]
 *     near: Depth buffer value for the nearest possible depth (0-65535).
 *     far: Depth buffer value for the farthest possible depth (0-65535).
 */
extern void ge_set_depth_range(uint16_t near, uint16_t far);

/**
 * ge_set_fog:  Set fog parameters.
 *
 * [Parameters]
 *     near: Depth at which fog begins.
 *     far: Depth at which fog reaches full density.
 *     z_sign: -1 if the projection matrix inverts the sign of Z values,
 *         +1 otherwise.
 *     color: Fog color (0xBBGGRR).
 */
extern void ge_set_fog(float near, float far, int z_sign, uint32_t color);

/**
 * ge_set_shade_mode:  Set the primitive shading mode.
 *
 * [Parameters]
 *     mode: Shading mode (GE_SHADE_*).
 */
extern void ge_set_shade_mode(GEShadeMode mode);

/**
 * ge_set_stencil_func:  Set the stencil test function and related values.
 *
 * [Parameters]
 *     func: Stencil comparison function (GE_TEST_*).
 *     ref: Stencil reference value.
 *     mask: Stencil mask value.
 */
extern void ge_set_stencil_func(GETestFunc func, uint8_t ref, uint8_t mask);

/**
 * ge_set_stencil_op:  Set the stencil operations for stencil-fail,
 * depth-fail, and depth-pass pixels.
 *
 * [Parameters]
 *     sfail: Stencil operation for stencil test failure (GE_STENCIL_*).
 *     dfail: Stencil operation for depth test failure (GE_STENCIL_*).
 *     dpass: Stencil operation for depth test pass (GE_STENCIL_*).
 */
extern void ge_set_stencil_op(
    GEStencilOp sfail, GEStencilOp dfail, GEStencilOp dpass);

/**
 * ge_set_viewport:  Set the rendering region.
 *
 * [Parameters]
 *     x, y: Coordinates of lower-left corner of viewport, in pixels.
 *     width, height: Size of viewport, in pixels.
 */
extern void ge_set_viewport(int x, int y, int width, int height);

/*--------- Texture manipulation functions (ge-util/texture.c) ----------*/

/**
 * ge_set_colortable:  Set the color lookup table for CLUT-format textures.
 *
 * [Parameters]
 *     table: Pointer to the color table (must be 64-byte aligned).
 *     count: Number of colors in the table.
 *     format: Color data format (GE_PIXFMT_*).
 *     shift: Right shift count for values read from the texture (0-31).
 *     mask: Mask for values read from the texture (0-255).
 */
extern void ge_set_colortable(const void *table, int count, GEPixelFormat pixfmt,
                              int shift, uint8_t mask);

/**
 * ge_flush_texture_cache:  Clear all data from the texture cache.  If you
 * change texture data pointers without calling ge_texture_set_format(),
 * you must call this function before rendering any primitives with the
 * new texture data.
 */
extern void ge_flush_texture_cache(void);

/**
 * ge_set_texture_data:  Set the data pointer and size parameters for a
 * single mipmap level of a texture.
 *
 * [Parameters]
 *     index: Mipmap index (0-7, 0 = base image).
 *     data: Texture data pointer.
 *     width: Texture width, in pixels (must be a power of 2).
 *     height: Texture height, in pixels (must be a power of 2).
 *     stride: Texture data line size, in pixels.
 */
extern void ge_set_texture_data(int index, const void *data,
                                int width, int height, int stride);

/**
 * ge_set_texture_draw_mode:  Set the drawing mode for textures.
 *
 * [Parameters]
 *     mode: Drawing mode (GE_TEXDRAWMODE_*).
 *     alpha: True to use texture alpha values, false to ignore them.
 */
extern void ge_set_texture_draw_mode(GETextureDrawMode mode, int alpha);

/**
 * ge_set_texture_color:  Set the texture color used with GE_TEXDRAWMODE_BLEND.
 *
 * [Parameters]
 *     color: Texture color (0xBBGGRR).
 */
extern void ge_set_texture_color(uint32_t color);

/**
 * ge_set_texture_filter:  Set the magnification and minification filters
 * to use with textures.
 *
 * [Parameters]
 *     mag_filter: Magnification filter (GE_TEXFILTER_*).
 *     min_filter: Minification filter (GE_TEXFILTER_*).
 *     mip_filter: Mipmap filtering mode for minification (GE_TEXMIPFILTER_*).
 */
extern void ge_set_texture_filter(GETextureFilter mag_filter,
                                  GETextureFilter min_filter,
                                  GETextureMipFilter mip_filter);

/**
 * ge_set_texture_format:  Set the texture data format.
 *
 * [Parameters]
 *     levels: Number of mipmap levels.
 *     swizzled: True if the texture data is swizzled, false if not.
 *     format: Texture data format (GE_TEXFMT_*).
 */
extern void ge_set_texture_format(int levels, int swizzled, GETexelFormat format);

/**
 * ge_set_texture_map_mode:  Set the texture coordinate mapping mode.
 *
 * [Parameters]
 *     mode: Mapping mode (GE_TEXMAPMODE_*).
 *     source: Data source for environment mapping (GE_TEXMAPSRC_*).
 *     row1: Matrix row for U coordinate transformation (0-3).
 *     row2: Matrix row for V coordinate transformation (0-3).
 */
extern void ge_set_texture_map_mode(GETextureMapMode mode,
                                    GETextureMapSource source,
                                    int row1, int row2);

/**
 * ge_set_texture_mipmap_mode:  Set the texture mipmap selection mode and
 * bias.  The bias is a constant in the range [-8.0,8.0) added to the
 * value derived for the particular primitive; the biased result is then
 * used directly to select the texture level, so an increase of 1.0 halves
 * the texture resolution as rendered.
 *
 * [Parameters]
 *     mode: Mipmap selection mode (GE_MIPMAPMODE_*).
 *     bias: Mipmap bias.
 */
extern void ge_set_texture_mipmap_mode(GEMipmapMode mode, float bias);

/**
 * ge_set_texture_mipmap_slope:  Set the slope constant used for
 * GE_MIPMAPMODE_SLOPE mode mipmap level calculation.
 *
 * If the distance from the camera to the primitive being textured is d,
 * then the mipmap level L in slope mode is given by L = 1 + log2(d / slope).
 * If slope is 1.0, for example, then a distance of 0.5 or less gives mipmap
 * level 0, 1.0 gives level 1, 2.0 gives level 2, 4.0 gives level 3, and so
 * on up to 64.0 which gives the maximum level of 7 (minimum resolution).
 *
 * [Parameters]
 *     slope: Slope constant.
 */
extern void ge_set_texture_mipmap_slope(float slope);

/**
 * ge_set_texture_wrap_mode:  Set the wrap mode for texture coordinates.
 *
 * [Parameters]
 *     u_mode: Wrap mode for U (horizontal) coordinates (GE_TEXWRAPMODE_*).
 *     v_mode: Wrap mode for V (vertical) coordinates (GE_TEXWRAPMODE_*).
 */
extern void ge_set_texture_wrap_mode(GETextureWrapMode u_mode,
                                     GETextureWrapMode v_mode);

/**
 * ge_set_texture_scale:  Set the scale factors for texture coordinates.
 *
 * [Parameters]
 *     u_scale: Scale factor for U coordinates.
 *     v_scale: Scale factor for V coordinates.
 */
extern void ge_set_texture_scale(float u_scale, float v_scale);

/**
 * ge_set_texture_offset:  Set the offset values for texture coordinates.
 *
 * [Parameters]
 *     u_offset: Offset value for U coordinates.
 *     v_offset: Offset value for V coordinates.
 */
extern void ge_set_texture_offset(float u_offset, float v_offset);

/*---------- Vertex registration functions (ge-util/vertex.c) -----------*/

/**
 * ge_add_color_xy_vertex:  Add a vertex with color and 2D integer position
 * data.
 *
 * [Parameters]
 *     color: Vertex color (0xAABBGGRR).
 *     x, y: Vertex position.
 */
extern void ge_add_color_xy_vertex(uint32_t color, int16_t x, int16_t y);

/**
 * ge_add_color_xy_vertexf:  Add a vertex with color and 2D floating-point
 * position data.
 *
 * [Parameters]
 *     color: Vertex color (0xAABBGGRR).
 *     x, y: Vertex position.
 */
extern void ge_add_color_xy_vertexf(uint32_t color, float x, float y);

/**
 * ge_add_color_xyz_vertexf:  Add a vertex with color and 3D floating-point
 * position data.
 *
 * [Parameters]
 *     color: Vertex color (0xAABBGGRR).
 *     x, y, z: Vertex coordinates.
 */
extern void ge_add_color_xyz_vertexf(uint32_t color, float x, float y, float z);

/**
 * ge_add_uv_xy_vertex:  Add a vertex with integer texture coordinates and
 * integer 2D position data.  For alignment reasons, this function must
 * always be called an even number of times; for an odd number of vertices,
 * use ge_add_uv_xy_vertexf() instead.
 *
 * [Parameters]
 *     u, v: Texture coordinates.
 *     x, y: Vertex position.
 */
extern void ge_add_uv_xy_vertex(int16_t u, int16_t v, int16_t x, int16_t y);

/**
 * ge_add_uv_xyz_vertexf:  Add a vertex with integer texture coordinates
 * and 3D floating-point position data.
 *
 * [Parameters]
 *     u, v: Texture coordinates.
 *     x, y, z: Vertex coordinates.
 */
extern void ge_add_uv_xyz_vertexf(float u, float v, float x, float y, float z);

/**
 * ge_add_uv_color_xy_vertex:  Add a vertex with integer texture coordinates,
 * color, and 2D integer position data.
 *
 * [Parameters]
 *     u, v: Texture coordinates.
 *     color: Vertex color (0xAABBGGRR).
 *     x, y: Vertex position.
 */
extern void ge_add_uv_color_xy_vertex(int16_t u, int16_t v, uint32_t color,
                                      int16_t x, int16_t y);

/**
 * ge_add_uv_color_xyz_vertexf:  Add a vertex with floating-point texture
 * coordinates, color, and 3D floating-point position data.
 *
 * [Parameters]
 *     u, v: Texture coordinates.
 *     color: Vertex color (0xAABBGGRR).
 *     x, y, z: Vertex coordinates.
 */
extern void ge_add_uv_color_xyz_vertexf(float u, float v, uint32_t color,
                                        float x, float y, float z);

/**
 * ge_reserve_vertexbytes:  Reserve the given number of bytes in the
 * internal vertex buffer, returning a pointer to the allocated area.
 *
 * [Parameters]
 *     size: Number of bytes to reserve.
 * [Return value]
 *     Pointer to allocated region, or NULL if not enough space was available.
 */
extern void *ge_reserve_vertexbytes(int size);

/*-------------- Miscellaneous functions (ge-util/misc.c) ---------------*/

/**
 * ge_clear:  Clear the draw and/or depth buffers.
 *
 * [Parameters]
 *     clear_screen: True to clear the draw buffer, false to leave it alone.
 *     clear_depth: True to clear the depth buffer, false to leave it alone.
 *     clear_stencil: True to clear the stencil byte, false to leave it alone.
 *     color: Clear color and stencil value for the draw buffer (0xSSBBGGRR).
 *     depth: Depth value to store to the depth buffer.
 *     width, height: Size of the framebuffer, in pixels.
 */
extern void ge_clear(int clear_screen, int clear_depth, int clear_stencil,
                     uint32_t color, uint16_t depth, int width, int height);

/**
 * ge_copy:  Copy image data from src to dest using the GE.
 *
 * This copy is a GE operation, so it will take place in the background
 * and will generally not be complete when this function returns.
 *
 * [Parameters]
 *     src: Copy source.
 *     src_stride: Source line size, in pixels (must be <2048 and a
 *         multiple of 8).
 *     dest: Copy destination.
 *     dest_stride: Destination line size, in pixels (must be <2048 and a
 *         multiple of 8).
 *     width, height: Copy size (width must be <=512).
 *     mode: Copy mode (GE_COPY_*).
 */
extern void ge_copy(const uint32_t *src, uint32_t src_stride, uint32_t *dest,
                    uint32_t dest_stride, int width, int height, GECopyMode mode);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_PSP_GE_UTIL_H
