/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/graphics.h: Header for graphics functionality.
 */

/*
 * Coordinate transformation is performed as:
 *
 *     [X Y Z W] = (([x y z w] * M_model) * M_view) * M_projection
 *
 * (this is the same as the OpenGL fixed-function pipeline transformation,
 * rewritten for row-major matrices).  [x y z w] is a row vector containing
 * the input (model space) coordinates, and [X Y Z W] is a row vector
 * containing the output (device space) coordinates.  Device space is in
 * [-1,+1] for all three dimensions, with the positive axes pointing right,
 * up, and away from the viewer (a left-handed coordinate system).
 *
 * All color parameters to graphics functions are floating-point values
 * between 0.0 (minimum value) and 1.0 (maximum value) inclusive; on an
 * 8-bit color display, for example, 0.0 maps to color value 0 and 1.0
 * maps to color value 255.  Colors are specified by their red, green,
 * and blue components (usually named "r", "g", and "b"), sometimes with
 * an alpha ("a") component as well.  When specified using a vector value,
 * the red, green, blue, and alpha components map to x, y, z, and w
 * respectively.
 *
 * Unless otherwise specified, all graphics-related functions (including
 * the framebuffer_*(), font_*(), shader_*(), and texture_*() functions)
 * must be called from the program's main thread (the thread on which
 * sil_main() is called).
 *
 * Some functions which take vector or matrix input parameters can also
 * be called from C++ using an immediate constructor call rather than a
 * pointer; see the "C++ wrappers" section near the end of this file.
 */

#ifndef SIL_GRAPHICS_H
#define SIL_GRAPHICS_H

EXTERN_C_BEGIN

struct Matrix4f;
struct Vector2f;
struct Vector3f;
struct Vector4f;

/*************************************************************************/
/*********************** Data types and constants ************************/
/*************************************************************************/

/**
 * GraphicsDisplayModeEntry:  Structure describing a single display mode.
 */
typedef struct GraphicsDisplayModeEntry GraphicsDisplayModeEntry;
struct GraphicsDisplayModeEntry {
    /* Display device to which this entry applies. */
    int device;
    /* Name of the display device, or NULL if not known. */
    const char *device_name;
    /* Size of the display, in pixels. */
    int width, height;
    /* Refresh rate, in frames per second, or 0 if not known. */
    float refresh;
};

/**
 * GraphicsDisplayModeList:  Structure containing an array of display modes.
 * Returned from graphics_list_display_modes().
 */
typedef struct GraphicsDisplayModeList GraphicsDisplayModeList;
struct GraphicsDisplayModeList {
    /* Number of entries in the modes[] array. */
    int num_modes;
    /* Array of display modes. */
    GraphicsDisplayModeEntry modes[1];  // True length given by num_modes.
};

/**
 * GraphicsError:  Error codes for graphics operations.  Currently, only
 * graphics_set_display_mode() returns error codes.
 */
enum GraphicsError {
    /**** Successful conditions. ****/

    /* No error occured. */
    GRAPHICS_ERROR_SUCCESS = 0,
    /* A display mode change was successful, but graphics state was lost.
     * The caller must destroy and recreate all graphics resources. */
    GRAPHICS_ERROR_STATE_LOST,

    /**** Failure conditions. ****/

    /* Unknown or unspecified error condition. */
    GRAPHICS_ERROR_UNKNOWN,
    /* The system does not support the requested video mode. */
    GRAPHICS_ERROR_MODE_NOT_SUPPORTED,
    /* OpenGL is required but was not found on the system. */
    GRAPHICS_ERROR_BACKEND_NOT_FOUND,
    /* The OpenGL version on the system is older than required, or (for
     * desktop OpenGL 1.x) does not support required extensions. */
    GRAPHICS_ERROR_BACKEND_TOO_OLD,
};
typedef enum GraphicsError GraphicsError;

/*-----------------------------------------------------------------------*/

/**
 * GraphicsBlendFactor:  Constants identifying a pixel multiplication
 * factor for blend operations.  The literal values "0" and "1" can be used
 * in place of GRAPHICS_BLEND_ZERO and GRAPHICS_BLEND_ONE respectively.
 *
 * In blending operations, the "source" pixel value refers to the new pixel
 * value being drawn to the framebuffer; for example, if a half-transparent
 * orange polygon is being drawn, the source pixel value would be
 * (R=1, G=0.5, B=0, A=0.5).  The "destination" pixel value refers to the
 * current value of the pixel in the framebuffer which will be affected by
 * the draw operation.
 *
 * Not all blend factors or combinations thereof are supported on all
 * systems.  In particular, the following may not be supported:
 *    - Color squaring (for example, using SRC_COLOR as the source factor)
 *    - Constant blend factors (CONSTANT, INV_CONSTANT)
 *
 * Note that, while destination alpha factors are included for completeness,
 * it is currently undefined whether framebuffers (including both the
 * display buffer and user-created framebuffers) include an alpha channel,
 * and thus the effect of using DEST_ALPHA and INV_DEST_ALPHA is undefined.
 */
enum GraphicsBlendFactor {
    GRAPHICS_BLEND_ZERO = 0,       // out = 0
    GRAPHICS_BLEND_ONE,            // out = in
    GRAPHICS_BLEND_SRC_COLOR,      // out = in * src
    GRAPHICS_BLEND_SRC_ALPHA,      // out = in * src.a
    GRAPHICS_BLEND_INV_SRC_ALPHA,  // out = in * (1-src.a)
    GRAPHICS_BLEND_DEST_COLOR,     // out = in * dest
    GRAPHICS_BLEND_DEST_ALPHA,     // out = in * dest.a
    GRAPHICS_BLEND_INV_DEST_ALPHA, // out = in * (1-dest.a)
    GRAPHICS_BLEND_CONSTANT,       // out = in * blend_color
    GRAPHICS_BLEND_INV_CONSTANT,   // out = in * (1-blend_color)
};
typedef enum GraphicsBlendFactor GraphicsBlendFactor;

/**
 * GraphicsBlendOperation:  Constants identifying a pixel combination
 * function for blend operations.
 *
 * Note that some (very old) systems may not support functions other than
 * GRAPHICS_BLEND_ADD.
 */
enum GraphicsBlendOperation {
    GRAPHICS_BLEND_ADD = 1,     // out = src*srcFactor + dest*destFactor
    GRAPHICS_BLEND_SUB,         // out = src*srcFactor - dest*destFactor
    GRAPHICS_BLEND_RSUB,        // out = dest*destFactor - src*srcFactor
};
typedef enum GraphicsBlendOperation GraphicsBlendOperation;

/**
 * GraphicsComparisonType:  Constants identifying comparison types for
 * rendering test operations.  See the documentation of each function
 * using these constants for details.
 */
enum GraphicsComparisonType {
    GRAPHICS_COMPARISON_FALSE = 0,
    GRAPHICS_COMPARISON_TRUE,
    GRAPHICS_COMPARISON_EQUAL,
    GRAPHICS_COMPARISON_NOT_EQUAL,
    GRAPHICS_COMPARISON_LESS,
    GRAPHICS_COMPARISON_LESS_EQUAL,
    GRAPHICS_COMPARISON_GREATER_EQUAL,
    GRAPHICS_COMPARISON_GREATER,
};
typedef enum GraphicsComparisonType GraphicsComparisonType;

/**
 * GraphicsFaceCullMode:  Constants identifying modes for face (polygon)
 * culling, used with graphics_set_face_cull().
 */
enum GraphicsFaceCullMode {
    GRAPHICS_FACE_CULL_NONE = 0,  // Do not cull any polygons.
    GRAPHICS_FACE_CULL_CW,        // Cull when vertices are in clockwise order.
    GRAPHICS_FACE_CULL_CCW,       // Cull when vertices are counterclockwise.
};
typedef enum GraphicsFaceCullMode GraphicsFaceCullMode;

/**
 * GraphicsPrimitiveType:  Constants identifying primitive types.  The
 * types defined here are treated identically to the corresponding OpenGL
 * primitives of the same names.  On platforms which do not natively
 * support quadrilateral primitives (QUADS and QUAD_STRIP), such primitives
 * are internally remapped to triangle-based primitives before drawing, so
 * client code can use QUADS and QUAD_STRIP unconditionally, though it may
 * still be more efficient to use triangles on some platforms.
 */
enum GraphicsPrimitiveType {
    GRAPHICS_PRIMITIVE_POINTS = 1,
    GRAPHICS_PRIMITIVE_LINES,
    GRAPHICS_PRIMITIVE_LINE_STRIP,
    GRAPHICS_PRIMITIVE_TRIANGLES,
    GRAPHICS_PRIMITIVE_TRIANGLE_STRIP,
    GRAPHICS_PRIMITIVE_QUADS,
    GRAPHICS_PRIMITIVE_QUAD_STRIP,
};
typedef enum GraphicsPrimitiveType GraphicsPrimitiveType;

/**
 * GraphicsStencilOp:  Constants identifying stencil operations, used with
 * graphics_set_stencil_operations().
 */
enum GraphicsStencilOp {
    GRAPHICS_STENCIL_KEEP = 1,  // Leave the current value unchanged.
    GRAPHICS_STENCIL_CLEAR,     // Clear the value to zero.
    GRAPHICS_STENCIL_REPLACE,   // Set the value to the comparison reference.
    GRAPHICS_STENCIL_INCR,      // Increment the value (clamped to maximum).
    GRAPHICS_STENCIL_DECR,      // Decrement the value (clamped to zero).
    GRAPHICS_STENCIL_INVERT,    // Bitwise invert the value.
};
typedef enum GraphicsStencilOp GraphicsStencilOp;

/**
 * GRAPHICS_VERTEX_FORMAT:  Macro for constructing values to store into a
 * "format" array for passing to the graphics_create[_indexed]_primitive()
 * and graphics_draw[_indexed]_primitive().  The "format" array should
 * consist of a sequence of values generated by this macro, followed by
 * the value zero to terminate the array.
 *
 * [Parameters]
 *     type: Data type (a GRAPHICS_VERTEX_* constant, but with
 *         "GRAPHICS_VERTEX_" omitted from the constant name).
 *     offset: Offset of data element within vertex data structure, in
 *         bytes (must be nonnegative and <=65535).
 * [Return value]
 *     Value to store in "format" array.
 */
#define GRAPHICS_VERTEX_FORMAT(type,offset) \
    ((uint32_t)(GRAPHICS_VERTEX_##type << 16 | ((offset) & 0xFFFF)))

/**
 * GRAPHICS_VERTEX_*:  Constants identifying various kinds of data that
 * can be stored in vertex data streams.  The constant names take the
 * following form:
 *     <data>_<count><format>
 * where <data> is the data content (POSITION, TEXCOORD, etc.), <count> is
 * the number of data elements (e.g. 3 for a 3D vertex position), and
 * <format> identifies the numeric format of the data:
 *     F = 32-bit float, I = 32-bit integer, S = 16-bit integer,
 *     UB = 8-bit unsigned integer
 * much like the suffixes used in OpenGL function names.
 *
 * Note that when rendering using shader objects, only the ATTRIB_* types
 * are valid; all other formats are ignored.
 *
 * For ease of debugging, the constant values are defined using the
 * following scheme:
 *     0xABCD
 *       |||`------- Number of components (e.g. "3" for a 3-component value)
 *       ||`------ Data type: (is_float ? 8 : 0) | (log2(num_bits/8))
 *       ||           e.g. 32-bit float = 8 | log2(32/8) = 0xA
 *       |`----- Minor grouping (e.g. different kinds of color values)
 *       `---- Major grouping (e.g. position, texture coordinate, color...)
 * However, both callers and implementations MUST NOT rely on this format
 * as it may change in the future.
 *
 * Note that callers of graphics_create_primitive() and similar functions
 * should _not_ use these constants directly, but should instead use the
 * GRAPHICS_VERTEX_FORMAT() macro (see above) to generate the values to
 * store in the "format" array.
 */
enum GraphicsVertexFormatType {
    GRAPHICS_VERTEX_POSITION_2S = 0x0012,  // Not normalized.
    GRAPHICS_VERTEX_POSITION_2F = 0x00A2,
    GRAPHICS_VERTEX_POSITION_3F = 0x00A3,
    GRAPHICS_VERTEX_POSITION_4F = 0x00A4,

    GRAPHICS_VERTEX_TEXCOORD_2F = 0x10A2,

    GRAPHICS_VERTEX_COLOR_4NUB  = 0x2004,  // Normalized to [0.0,+1.0].
    GRAPHICS_VERTEX_COLOR_4F    = 0x20A4,
};
typedef enum GraphicsVertexFormatType GraphicsVertexFormatType;

/* Type value for a given custom attribute ID. */
#define GRAPHICS_VERTEX_USER(n)  (0xB000 + (n))

/*
 * Type values for a given attribute ID and data type used with a shader
 * object.  The "N" integer types (NUB and NS) are "normalized", so that
 * the maximum value becomes +1.0 and (for signed types) the minimum value
 * becomes -1.0, and passed to the shader as floating-point values; the
 * regular integer types (UB, S, I) are not normalized and are passed as
 * integer values.
 *
 * For normalized signed attributes (type NS), whether the integer value 0
 * converts to exactly 0.0 in floating point depends on the environment.
 * In particular, versions of OpenGL before 4.2 use a conversion which maps
 * integer 0 to a slightly positive value.
 *
 * Note that integer types (UB, S, I) are not supported before version 3.0
 * of OpenGL and OpenGL ES.  When using these types on an OpenGL platform,
 * be sure to request an OpenGL 3.0 context by setting the "opengl_version"
 * display attribute; see graphics_set_display_attr() for details.
 */
#define GRAPHICS_VERTEX_ATTRIB_1UB(n)   GRAPHICS_VERTEX_ATTRIB((n),1,UB)
#define GRAPHICS_VERTEX_ATTRIB_1S(n)    GRAPHICS_VERTEX_ATTRIB((n),1,S)
#define GRAPHICS_VERTEX_ATTRIB_1I(n)    GRAPHICS_VERTEX_ATTRIB((n),1,I)
#define GRAPHICS_VERTEX_ATTRIB_1NUB(n)  GRAPHICS_VERTEX_ATTRIB((n),1,NUB)
#define GRAPHICS_VERTEX_ATTRIB_1NS(n)   GRAPHICS_VERTEX_ATTRIB((n),1,NS)
#define GRAPHICS_VERTEX_ATTRIB_1F(n)    GRAPHICS_VERTEX_ATTRIB((n),1,F)
#define GRAPHICS_VERTEX_ATTRIB_2UB(n)   GRAPHICS_VERTEX_ATTRIB((n),2,UB)
#define GRAPHICS_VERTEX_ATTRIB_2S(n)    GRAPHICS_VERTEX_ATTRIB((n),2,S)
#define GRAPHICS_VERTEX_ATTRIB_2I(n)    GRAPHICS_VERTEX_ATTRIB((n),2,I)
#define GRAPHICS_VERTEX_ATTRIB_2NUB(n)  GRAPHICS_VERTEX_ATTRIB((n),2,NUB)
#define GRAPHICS_VERTEX_ATTRIB_2NS(n)   GRAPHICS_VERTEX_ATTRIB((n),2,NS)
#define GRAPHICS_VERTEX_ATTRIB_2F(n)    GRAPHICS_VERTEX_ATTRIB((n),2,F)
#define GRAPHICS_VERTEX_ATTRIB_3UB(n)   GRAPHICS_VERTEX_ATTRIB((n),3,UB)
#define GRAPHICS_VERTEX_ATTRIB_3S(n)    GRAPHICS_VERTEX_ATTRIB((n),3,S)
#define GRAPHICS_VERTEX_ATTRIB_3I(n)    GRAPHICS_VERTEX_ATTRIB((n),3,I)
#define GRAPHICS_VERTEX_ATTRIB_3NUB(n)  GRAPHICS_VERTEX_ATTRIB((n),3,NUB)
#define GRAPHICS_VERTEX_ATTRIB_3NS(n)   GRAPHICS_VERTEX_ATTRIB((n),3,NS)
#define GRAPHICS_VERTEX_ATTRIB_3F(n)    GRAPHICS_VERTEX_ATTRIB((n),3,F)
#define GRAPHICS_VERTEX_ATTRIB_4UB(n)   GRAPHICS_VERTEX_ATTRIB((n),4,UB)
#define GRAPHICS_VERTEX_ATTRIB_4S(n)    GRAPHICS_VERTEX_ATTRIB((n),4,S)
#define GRAPHICS_VERTEX_ATTRIB_4I(n)    GRAPHICS_VERTEX_ATTRIB((n),4,I)
#define GRAPHICS_VERTEX_ATTRIB_4NUB(n)  GRAPHICS_VERTEX_ATTRIB((n),4,NUB)
#define GRAPHICS_VERTEX_ATTRIB_4NS(n)   GRAPHICS_VERTEX_ATTRIB((n),4,NS)
#define GRAPHICS_VERTEX_ATTRIB_4F(n)    GRAPHICS_VERTEX_ATTRIB((n),4,F)

/* Internal implementation of the attribute formats.  Callers should use
 * the specific ATTRIB_* formats listed above. */
#define GRAPHICS_VERTEX_ATTRIB(index,count,type) \
    (0xC000 | ((count)-1)<<12 | GRAPHICS_VERTEXDATA_##type<<8 | ((index)&0xFF))
enum GraphicsVertexDataType {
    GRAPHICS_VERTEXDATA_UB  = 0x0,
    GRAPHICS_VERTEXDATA_S   = 0x1,
    GRAPHICS_VERTEXDATA_I   = 0x2,
    GRAPHICS_VERTEXDATA_NUB = 0x4,
    GRAPHICS_VERTEXDATA_NS  = 0x5,
    GRAPHICS_VERTEXDATA_F   = 0xA,
};
typedef enum GraphicsVertexDataType GraphicsVertexDataType;

/**
 * GRAPHICS_TEXCOLOR_*:  Texture color types passed to a
 * ShaderSourceCallback function.
 */
enum GraphicsTextureColorType {
    GRAPHICS_TEXCOLOR_NONE = 0,  // No texture applied.
    GRAPHICS_TEXCOLOR_RGBA,      // 4-component RGBA data.
    GRAPHICS_TEXCOLOR_A,         // 1-component alpha data.
    GRAPHICS_TEXCOLOR_L,         // 1-component luminance data.
};
typedef enum GraphicsTextureColorType GraphicsTextureColorType;

/**
 * INVALID_SHADER_KEY:  Value returned from a ShaderKeyCallback function
 * which indicates failure.
 */
#define INVALID_SHADER_KEY (~(uint32_t)0)

/*************************************************************************/
/************************ Callback function types ************************/
/*************************************************************************/

/**
 * ShaderSourceCallback:  Function type for the shader source generation
 * functions passed to graphics_set_shader_generator().  The function
 * should generate source code for the vertex or fragment shader specified
 * by the parameters to the function, which indicate the types of data
 * included in each vertex and other relevant render state, and return the
 * source code as a single (though typically multi-line) string stored in a
 * buffer allocated by mem_alloc().
 *
 * On success, the returned buffer becomes owned by the caller, who will
 * free it when it is no longer needed.
 *
 * See the graphics_set_shader_generator() documentation for details.
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
 * [Return value]
 *     Generated shader source code, or NULL on error.
 */
typedef char *ShaderSourceCallback(
    GraphicsPrimitiveType primitive_type, int position_count,
    int texcoord_count, GraphicsTextureColorType texcolor_type, int tex_offset,
    int color_count, int color_uniform, int fog, int alpha_test,
    GraphicsComparisonType alpha_comparison);

/**
 * ShaderKeyCallback:  Function type for the shader key generation function
 * passed to graphics_set_shader_generator().  The function must return a
 * 32-bit value, unique among all possible shader programs, identifying the
 * shader specified by the function parameters (which are the same values
 * as would be passed to the shader source callbacks to generate the shader
 * source code).
 *
 * If the shader source generator cannot handle the requested parameters,
 * this function should return INVALID_SHADER_KEY; in this case, the
 * primitive will be unable to be rendered.
 *
 * [Parameters]
 *     As for ShaderSourceCallback.
 * [Return value]
 *     Unique 32-bit shader key, or INVALID_SHADER_KEY if the requested
 *     parameters are not supported.
 */
typedef uint32_t ShaderKeyCallback(
    GraphicsPrimitiveType primitive_type, int position_count,
    int texcoord_count, GraphicsTextureColorType texcolor_type, int tex_offset,
    int color_count, int color_uniform, int fog, int alpha_test,
    GraphicsComparisonType alpha_comparison);

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

/*-------- Display mode configuration and related functionality ---------*/

/**
 * graphics_num_devices:  Return the number of display devices available
 * in the system.
 *
 * This function may be called from any thread.
 *
 * [Return value]
 *     Number of display devices available (always a positive value).
 */
extern int graphics_num_devices(void);

/**
 * graphics_device_width, graphics_device_height:  Return the current width
 * or height of the current display device.  This will typically be
 * constant for a particular runtime environment, but on systems which
 * allow the device resolution to be changed at runtime (such as PC
 * operating systems), the return values from these functions will change
 * to reflect the current resolution at the time the functions are called.
 *
 * The "current" display device is the device which was used in the last
 * successful graphics_set_display_mode() call; if no such call has been
 * made, the current device is that specified by the "device" attribute
 * (see graphics_set_display_attr()).
 *
 * Note that when using windowed mode, passing these values to
 * graphics_set_display_mode() will not necessarily result in a window
 * which covers the entire display device!  For example, if the system
 * has a working area (such as a desktop) which is larger than the
 * current display resolution, the system may place the window partly or
 * entirely offscreen.  On the flip side, setting a fullscreen display
 * mode using these values will always succeed (assuming no runtime
 * failures such as insufficient memory).
 *
 * These functions may be called from any thread.
 *
 * [Return value]
 *     Display device width or height, in pixels.
 */
extern int graphics_device_width(void);
extern int graphics_device_height(void);

/**
 * graphics_has_windowed_mode:  Return whether the system supports the
 * concept of a windowed mode in which a display region of arbitrary size
 * (up to the screen size) can be used.
 *
 * This function may be called from any thread.
 *
 * [Return value]
 *     True if windowed mode is supported, false if not.
 */
extern int graphics_has_windowed_mode(void);

/**
 * graphics_list_display_modes:  Return an array containing display modes
 * supported by the system.  For systems supporting a windowed
 * (non-fullscreen) mode, this is the list of modes supported for
 * fullscreen display.
 *
 * Returned values are sorted by the following rules:
 *    - By display device number, in increasing order.
 *    - For modes on the same display deivce, from the smallest to the
 *         largest number of total pixels in the mode.
 *    - For modes with the same total number of pixels, from smallest to
 *         largest width.
 *    - For modes of the same size, from lowest to highest refresh rate.
 *
 * [Parameters]
 *     include_refresh: True to return all display modes, including modes
 *         with the same size and different refresh rates; false to return
 *         only distinct sizes, treating all refresh rates as zero.
 * [Return value]
 *     List of supported display modes.  The return value will never be NULL.
 */
extern const GraphicsDisplayModeList *graphics_list_display_modes(
    int include_refresh);

/**
 * graphics_set_display_attr:  Set a system-specific display attribute.
 *
 * The following attributes are defined for all systems (though some
 * attributes may have no effect on some systems):
 *
 * "center_window": 1 value of type int
 *     Sets whether to center newly created windows.  A nonzero value
 *     causes graphics_set_display_mode() to open windows such that they
 *     are centered on the display device, if possible; a value of zero
 *     indicates that the window position should be left to the underlying
 *     system.  On systems without a windowed mode, this value may be set
 *     but will have no effect.  The default value is 0 (windows will not
 *     be explicitly centered).
 *
 * "depth_bits": 1 value of type int
 *     Sets the minimum number of bits per pixel to use for the depth
 *     buffer in subsequent calls to graphics_set_display_mode().  The
 *     actual depth buffer resolution may be greater than this value,
 *     depending on available hardware display capabilities.  Even if this
 *     value is successfully set, graphics_set_display_mode() will fail if
 *     the renderer does not support at least the requested number of bits.
 *     A value of zero indicates that a depth buffer is not required, and
 *     the system may choose not to allocate a depth buffer (in which case
 *     any operations involving the depth buffer are undefined).  The
 *     default value is 16.
 *
 * "device": 1 value of type int
 *     Sets the display device to be used when setting a display mode.
 *     Valid values are from 0 through graphics_num_devices()-1 inclusive;
 *     when setting a full-screen mode, the value should be taken from the
 *     GraphicsDisplayModeList entry for the mode to be used.  The default
 *     value is system-dependent.
 *
 * "multisample": 1 value of type int
 *     Sets the number of samples to use for multisample antialiasing
 *     (MSAA) in subsequent calls to graphics_set_display_mode().  A value
 *     of 1 disables MSAA.  The default value is 1.
 *
 *     This value is NOT applied to user-created framebuffers; there is
 *     currently no way to enable multisampling on such framebuffers.
 *
 * "refresh_rate": 1 value of type float
 *     Sets the desired refresh rate, in frames per second.  If a display
 *     device supports multiple refresh rates for a given display size,
 *     graphics_set_display_mode() will choose the one which is closest to
 *     this value.  A value of 0 means "don't care" and selects the "best"
 *     available refresh rate for the requested display size; this is
 *     normally the highest refresh rate, but may be a lower rate in some
 *     cases (for example, if the display is already using a mode of the
 *     requested size, the mode will not be changed even if there is a
 *     higher-refresh-rate mode available).  The default value is 0.
 *
 * "stencil_bits": 1 value of type int
 *     Sets the minimum number of bits per pixel to use for the stencil
 *     buffer in subsequent calls to graphics_set_display_mode().  The
 *     actual stencil buffer resolution may be greater than this value,
 *     depending on available hardware display capabilities.  Even if this
 *     value is successfully set, graphics_set_display_mode() will fail if
 *     the renderer does not support at least the requested number of bits.
 *     A value of zero indicates that a stencil buffer is not required, and
 *     the system may choose not to allocate a stencil buffer (in which
 *     case any operations involving the stencil buffer are undefined).
 *     The default value is 0.
 *
 * "vsync": 1 value of type int
 *     Sets whether to synchronize the beginning of each frame with a
 *     periodic external signal such as the display refresh.  A nonzero
 *     value enables synchronization, meaning that graphics_start_frame()
 *     may block; zero disables synchronization, allowing frames to be
 *     drawn as fast as possible but increasing the possibility of tearing
 *     or other visual artifacts.  Attempting to set a nonzero value on
 *     systems which do not support synchronization, or attempting to set
 *     zero on systems which require synchronization, will fail.  The
 *     default is to enable synchronization if supported.
 *
 *     Changes to this attribute take immediate effect.
 *
 * "window": 1 value of type int
 *     Sets whether graphics_set_display_mode() will use a windowed or
 *     full-screen mode.  Passing a nonzero value selects windowed mode;
 *     zero selects full-screen mode.  On systems which do not support the
 *     concept of a windowed mode, attempting to set a nonzero value will
 *     fail (setting zero will always succeed on such systems).  The
 *     default is to use windowed mode if available, else full-screen mode.
 *
 * "window_resizable": 1 value of type int
 *     Sets whether to allow a (non-fullscreen) window to be resized by the
 *     window system.  Passing a nonzero value enables resizing; passing
 *     zero disables resizing.  On systems which do not support the concept
 *     of a windowed mode, this attribute is ignored.  The default value is
 *     0 (resizing is not allowed).
 *
 *     When resizing is enabled, client code should obtain the current
 *     window size by calling graphics_display_{width,height}() at the
 *     beginning of each frame.  See graphics_set_window_resize_limits()
 *     for how to constrain resize operations.
 *
 * On platforms using OpenGL, the following attributes are also available:
 *
 * "opengl_debug": 1 value of type int
 *     Enables or disables OpenGL debugging mode, if supported by the
 *     system's OpenGL implementation.  When enabled (any nonzero value),
 *     newly created OpenGL contexts (for example, when opening a new
 *     window) are set to debug mode, and OpenGL debug messages are logged
 *     via the DLOG() facility.  The default value is 0 (debug mode is
 *     disabled).
 *
 *     This attribute has no effect if DEBUG is not defined or if the
 *     system's OpenGL implementation does not support debug contexts or
 *     debug messages.
 *
 *     Changes to this attribute take immediate effect with respect to
 *     logging of debug messages, but some types of debug messages may not
 *     be logged if the OpenGL context was not created in debug mode.
 *
 * "opengl_version": 2 values of type int
 *     Specifies the minimum version of OpenGL (or OpenGL ES, as
 *     appropriate to the platform) required when setting a display mode.
 *     graphics_set_display_mode() will fail if it cannot create an OpenGL
 *     context of at least the requested version.  The first value gives
 *     the major part of the version, and the second value gives the minor
 *     part; thus graphics_set_display_attr("opengl_version", 3, 1)
 *     declares that OpenGL 3.1 or later is required.  The default value
 *     is 2.0 (the minimum version required by SIL itself).
 *
 *     In most cases, programs do not need to explicitly set this
 *     attribute, since SIL transparently covers most differences between
 *     OpenGL versions, but see the note at GRAPHICS_VERTEX_ATTRIB_*
 *     regarding integer vertex attributes and OpenGL 3.0.
 *
 * Other attribute names and values are system-dependent; it is the
 * responsibility of the caller to pass correct values for the chosen
 * attribute.
 *
 * [Parameters]
 *     name: Attribute name.
 *     ...: Attribute value(s).
 * [Return value]
 *     True if the display attribute was successfully set; false if the
 *     attribute is unknown or an error occurred.
 */
extern int graphics_set_display_attr(const char *name, ...);

/**
 * graphics_set_display_mode:  Initialize the display to the given size,
 * using display attributes set through graphics_set_display_attr().  This
 * function must not be called between graphics_start_frame() and
 * graphics_finish_frame().
 *
 * The graphics state after a successful display change is
 * implementation-dependent.  If this function fails, the state of the
 * display is left undefined.
 *
 * Note that the caller is responsible for handling a STATE_LOST error;
 * attempting to make use of existing graphics objects after a loss of
 * state results in undefined behavior and may crash the program.  Callers
 * should only pass NULL for error_ret if the program never changes the
 * display mode after startup or if the program will not be run under
 * environments in which state loss can occur (currently Linux, Mac OS X,
 * and Windows platforms).
 *
 * Even on a successful mode change without a STATE_LOST error, some
 * renderers may return incorrect pixel data for pixel-read operations
 * (graphics_read_pixels() and texture_create_from_display()) on the
 * first frame after the mode change.  Callers should avoid relying on
 * pixel data read during that first frame.
 *
 * [Parameters]
 *     width, height: Display size, in pixels.
 *     error_ret: Pointer to variable to retrieve the cause of a failed
 *         call or caveats for a succeeded call (GRAPHICS_ERROR_*).  May
 *         be NULL if the value is not needed.
 * [Return value]
 *     True if the display mode was successfully set; false on error.
 */
extern int graphics_set_display_mode(int width, int height,
                                     GraphicsError *error_ret);

/**
 * graphics_renderer_info:  Return a string describing the low-level
 * rendering backend.  In environments which use OpenGL, this will
 * typically be an OpenGL version string.
 *
 * This function may only be called after a display mode has been set.
 * Calling this function at other times results in undefined behavior.
 *
 * [Return value]
 *     Renderer information string.
 */
extern const char *graphics_renderer_info(void);

/**
 * graphics_display_is_window:  Return whether the current display mode
 * is a windowed mode.  On systems which do not support the concept of a
 * windowed mode, this function will always return false.
 *
 * [Return value]
 *     True if the display mode is a windowed mode; false if the display
 *     mode is a full-screen mode or if no display mode is set.
 */
extern int graphics_display_is_window(void);

/**
 * graphics_set_window_title:  Set the title to be displayed in the window
 * title bar (when in windowed mode) or any equivalent system facility,
 * such as a taskbar or application list.
 *
 * [Parameters]
 *     title: Title string.
 */
extern void graphics_set_window_title(const char *title);

/**
 * graphics_set_window_icon:  Set the icon to be associated with the
 * window, if relevant for the system.  The texture must not be locked,
 * and its pixel data must be readable.  The pixel data is copied out of
 * the texture, so the texture may be safely modified or destroyed after
 * this function returns.
 *
 * On Microsoft Windows, passing a texture of size 16x16 or smaller will
 * set only the "small" icon (displayed in the window title bar and the
 * task bar); passing a larger texture will set both the "large" icon
 * (displayed in the Alt-Tab task switcher) and the "small" icon.
 *
 * Note that it is not currently possible to set an icon before opening a
 * window, since OpenGL (which is used for texture management) will not be
 * initialized.  Attempting to call this function before calling
 * graphics_set_display_mode() may cause the program to crash.
 *
 * [Parameters]
 *     texture: Texture to use as icon image.
 */
extern void graphics_set_window_icon(int texture);

/**
 * graphics_set_window_resize_limits:  Set constraints for window resize
 * operations.
 *
 * When the "window_resizable" display attribute is enabled, be default
 * there are no constraints placed on the size to which the user can
 * resize the window.  This function allows the caller to specify
 * constraints on both the size and the aspect (width-to-height) ratio of
 * the window in a resize operation.
 *
 * If any pair of values is zero, that attribute is not considered for
 * resize constraints.  For example, setting max_width and max_height to
 * zero allows the user to make the window as big as they want.
 *
 * The aspect ratio pairs are treated as rational numbers in the form x/y.
 * For example, the parameter set:
 *     min_aspect_x = 4, min_aspect_y = 3
 *     max_aspect_x = 16, max_aspect_y = 9
 * allows the window to be resized to any size with a width-to-height
 * ratio between 4:3 and 16:9, but prevents resizing the window to a
 * square (1:1) or to a width of twice the window height (2:1).
 *
 * If multiple constraints are specified, all are enforced (as in the
 * aspect ratio example above).  Defining multiple constraints which
 * cannot be simultaneously satisfied, such as a maximum size smaller than
 * the minimum size, results in undefined behavior.
 *
 * Using graphics_set_display_mode() to set a window size that violates
 * the constraints set with this function results in undefined behavior.
 *
 * [Parameters]
 *     min_width, min_height: Minimum allowable size for the window.
 *     max_width, max_height: Maximum allowable size for the window.
 *     min_aspect_x, min_aspect_y: Minimum allowable aspect ratio for the
 *         window.
 *     max_aspect_x, max_aspect_y: Maximum allowable aspect ratio for the
 *         window.
 */
extern void graphics_set_window_resize_limits(
    int min_width, int min_height, int max_width, int max_height,
    int min_aspect_x, int min_aspect_y, int max_aspect_x, int max_aspect_y);

/**
 * graphics_show_mouse_pointer:  Set whether to display a system-level
 * visible mouse pointer on top of the display, for systems which have
 * such a concept.  Some systems may not allow the pointer to be hidden.
 *
 * By default, the mouse pointer (if any) is hidden if possible.
 *
 * [Parameters]
 *     on: True to display the pointer, false to hide it.
 */
extern void graphics_show_mouse_pointer(int on);

/**
 * graphics_get_mouse_pointer_state:  Return whether the system-level
 * mouse pointer display is enabled.
 *
 * [Return value]
 *     True if the pointer is displayed, false if not.
 */
extern int graphics_get_mouse_pointer_state(void);

/**
 * graphics_display_width, graphics_display_height:  Return the current
 * width or height of the display.  These values will never change while
 * a frame is being rendered (between graphics_start_frame() and
 * graphics_finish_frame()).
 *
 * These functions may be called from any thread.
 *
 * [Return value]
 *     Display width or height, in pixels, or zero if the display mode
 *     has not been set.
 */
extern PURE_FUNCTION int graphics_display_width(void);
extern PURE_FUNCTION int graphics_display_height(void);

/**
 * graphics_frame_period:  Return the nominal frame period of the current
 * display mode, in seconds per frame, or zero if the frame period is
 * unknown.
 *
 * [Return value]
 *     Frame period, in seconds per frame, or zero if unknown.
 */
extern double graphics_frame_period(void);

/**
 * graphics_has_focus:  For systems with a concept of input focus (typical
 * PC windowing systems, for example), return whether this program
 * currently has the input focus.  On other systems, this function always
 * returns true.
 *
 * [Return value]
 *     True if this program has the input focus, false if not.
 */
extern int graphics_has_focus(void);

/*--------------- Frame start/finish and synchronization ----------------*/

/**
 * graphics_start_frame:  Start rendering a frame.  This function must be
 * called before any functions which operate on the rendering target,
 * specifically:
 *    - graphics_clear()
 *    - graphics_clear_color()
 *    - graphics_clear_depth()
 *    - graphics_draw_indexed_vertices()
 *    - graphics_draw_primitive()
 *    - graphics_draw_primitive_partial()
 *    - graphics_draw_vertices()
 *    - graphics_end_and_draw_primitive()
 *    - graphics_read_pixels()
 *    - texture_create_from_display()
 *
 * If the "vsync" display attribute is enabled, this function may block
 * for up to graphics_frame_period() seconds.
 *
 * After this call, the display framebuffer is bound to the rendering
 * target, and the contents of the display framebuffer are undefined.
 * No other graphics state is modified.
 */
extern void graphics_start_frame(void);

/**
 * graphics_finish_frame:  Finish rendering a frame.  No rendering functions
 * (see graphics_start_frame()) may be called after this function until the
 * next call to graphics_start_frame().
 */
extern void graphics_finish_frame(void);

/**
 * graphics_sync:  Wait for any background rendering operations to complete.
 *
 * This function may be useful in measuring performance, but it is not
 * required for correct behavior of any graphics operations, including
 * graphics_read_pixels().  Calling this function will always slow down
 * the program.
 */
extern void graphics_sync(void);

/**
 * graphics_flush_resources:  Immediately release any resources which are
 * pending deletion.
 *
 * This function is not required for correct behavior of any graphics
 * operations, but it may help relieve memory pressure when swapping
 * between large sets of resources.
 *
 * This function implicitly performs a graphics_sync() operation, so it
 * should not be called from performance-critical code.
 */
extern void graphics_flush_resources(void);

/**
 * graphics_enable_debug_sync:  Enable or disable explicit sync on frame
 * start.
 *
 * If enabled and the program was built in debug mode (with DEBUG defined),
 * graphics_start_frame() will explicitly perform a sync operation (like
 * graphics_sync()) before starting a new frame, allowing the GPU rendering
 * time for the previous frame to be accurately recorded by the debugging
 * interface (see <SIL/debug.h>).  Note that this may change execution
 * timing and consequently mask timing-related bugs.
 *
 * If disabled (the default), the explicit sync is omitted, which may
 * result in inconsistent timing data shown in the debugging interface if
 * GPU processing takes longer than a single frame.
 *
 * This function has no effect if DEBUG is not defined; no explicit sync
 * is performed in non-debug builds.
 *
 * [Parameters]
 *     enable: True to enable sync on frame start, false to disable.
 */
extern void graphics_enable_debug_sync(int enable);

/*-------------------- Display clearing and reading ---------------------*/

/**
 * graphics_clear:  Clear the display or currently bound framebuffer to
 * the specified color, depth, and stencil values.  The depth buffer is
 * only modified if depth writing is enabled.
 *
 * The passed-in color and depth values will be clamped to the range
 * [0,1].  If the stencil value is larger than will fit in a stencil buffer
 * element, the high bits of the value are discarded.
 *
 * If clearing the display buffer or if the currently bound framebuffer
 * does not have an alpha channel, the alpha value is ignored.
 *
 * This function ignores the viewport region setting, but honors the clip
 * region setting.
 *
 * The behavior of this function is undefined if it is not called between
 * graphics_start_frame() and graphics_finish_frame().
 *
 * [Parameters]
 *     r, g, b, a: Clear color.
 *     depth: Depth value to store.
 *     stencil: Stencil value to store.
 */
extern void graphics_clear(float r, float g, float b, float a,
                           float depth, unsigned int stencil);

/**
 * graphics_clear_color:  Clear only the color buffer of the display or
 * currently bound framebuffer.  The depth and stencil buffers are left
 * unmodified.
 *
 * The passed-in color values will be clamped to the range [0,1].
 *
 * If clearing the display buffer or if the currently bound framebuffer
 * does not have an alpha channel, the alpha value is ignored.
 *
 * This function ignores the viewport region setting, but honors the clip
 * region setting.
 *
 * The behavior of this function is undefined if it is not called between
 * graphics_start_frame() and graphics_finish_frame().
 *
 * [Parameters]
 *     r, g, b, a: Clear color.
 */
extern void graphics_clear_color(float r, float g, float b, float a);

/**
 * graphics_clear_depth:  Clear only the depth and stencil buffers of the
 * display or currently bound framebuffer.  The color buffer is left
 * unmodified; the depth buffer is only modified if depth writing is
 * enabled.
 *
 * The passed-in depth value will be clamped to the range [0,1].  If the
 * stencil value is larger than will fit in a stencil buffer element, the
 * high bits of the value are discarded.
 *
 * It is not possible to clear the depth buffer independently of the
 * stencil buffer using this function.  However, the same effect can be
 * achieved by drawing a full-screen quad at the desired depth with color
 * buffer writing disabled (see graphics_enable_color_write()), depth
 * testing enabled with the comparison GRAPHICS_COMPARISON_TRUE, and
 * stencil testing disabled.
 *
 * This function ignores the viewport region setting, but honors the clip
 * region setting.
 *
 * The behavior of this function is undefined if it is not called between
 * graphics_start_frame() and graphics_finish_frame().
 *
 * [Parameters]
 *     depth: Depth value to store.
 *     stencil: Stencil value to store.
 */
extern void graphics_clear_depth(float depth, unsigned int stencil);

/**
 * graphics_read_pixels:  Read pixel data from the display or currently
 * bound framebuffer.  When reading from the display, this function
 * returns pixels from the current (in-progress) frame, not the previous
 * (displayed) frame.
 *
 * It is not an error to specify a region partly or completely outside
 * the display or framebuffer, but the out-of-bounds portion of the
 * returned buffer will be undefined.
 *
 * The returned pixel data will be in bottom-to-top, left-to-right order,
 * using RGBA format with 1 byte per pixel (the alpha byte will always be
 * 255).
 *
 * Reading framebuffer data is a slow operation on many systems, and it
 * also forces the program to wait until any rendering operations on the
 * framebuffer have completed, so do not call this function from
 * performance-critical code.
 *
 * The behavior of this function is undefined if it is not called between
 * graphics_start_frame() and graphics_finish_frame().
 *
 * [Parameters]
 *     x, y: Pixel coordinates of lower-left corner of region to read
 *         (0,0 is the bottom-left corner of the display).
 *     w, h: Size of region to read, in pixels.
 *     buffer: Buffer into which pixel data will be stored.  The buffer
 *         must have room for at least w*h*4 bytes.
 * [Return value]
 *     True on success, false on error.
 */
extern int graphics_read_pixels(int x, int y, int w, int h, void *buffer);

/*------------------ Primitive creation and rendering -------------------*/

/**
 * graphics_begin_primitive:  Create a new graphics primitive object with
 * no attached vertex data.  Vertices can be subsequently added with
 * graphics_add_vertex(); the parameters supplied for the first vertex
 * will determine the vertex data format.  The primitive cannot be drawn
 * until it has been finalized with graphics_end_primitive().
 *
 * Callers may safely ignore the return value of this function and use
 * the return value of graphics_{end,end_and_draw}_primitive() to check
 * the success or failure of the primitive creation operation.  If this
 * function fails, all subsequent graphics_add_vertex() calls will be
 * ignored, and graphics_{end,end_and_draw}_primitive() will return zero
 * to indicate failure.
 *
 * [Parameters]
 *     type: Primitive type (GRAPHICS_PRIMITIVE_*).
 * [Return value]
 *     True on success, false on error.
 */
extern int graphics_begin_primitive(GraphicsPrimitiveType type);

/**
 * graphics_add_vertex:  Add a vertex to the vertex data stream for a
 * primitive object created with graphics_begin_primitive().  If this is
 * the first vertex for the primitive, the presence or absence of the
 * vector parameters to this function will determine the data format;
 * otherwise, the presence or absence of the parameters must match that
 * of the first vertex, or an error will be signalled and the primitive
 * aborted.
 *
 * The format of vertex data which can be constructed through this function
 * is limited, and may not be optimized for fast rendering.  If the vertex
 * data is known in advance, it is generally preferable to create the
 * primitive object ahead of time with graphics_create_primitive().
 *
 * Position data for the vertex is required (the function will fail if
 * position == NULL).  If a texture has been applied with texture_apply()
 * but texture coordinates are not included in the vertex data, the
 * primitive will still be rendered but texturing behavior is undefined.
 *
 * As with graphics_begin_primitive(), callers may safely ignore the
 * return value of this function.  If this function fails for any reason
 * (including missing or extraneous data on the second or a later vertex),
 * all subsequent graphics_add_vertex() calls will be ignored, and
 * graphics_{end,end_and_draw}_primitive() will return zero to indicate
 * failure.
 *
 * When using shader objects instead of the standard rendering pipeline,
 * the effect of each of these attributes depends on the particular shader
 * in use.  By default, these attributes are ignored, but they may be bound
 * to specific shader inputs using shader_set_standard_attribute().  Note
 * that position data is required by this function even if it will not be
 * used by the shader.
 *
 * [Parameters]
 *     position: Pointer to vector containing vertex position (required).
 *     texcoord: Pointer to vector containing texture coordinates (may be
 *         NULL).
 *     color: Pointer to vector containing color value (may be NULL).
 * [Return value]
 *     True on success, false on error.
 */
extern int graphics_add_vertex(const struct Vector3f *position,
                               const struct Vector2f *texcoord,
                               const struct Vector4f *color);

/**
 * graphics_end_primitive:  Mark the end of vertices for a primitive
 * object created with graphics_begin_primitive(), and return an ID for
 * the primitive object.
 *
 * [Return value]
 *     Primitive ID (nonzero), or zero on error.
 */
extern int graphics_end_primitive(void);

/**
 * graphics_end_and_draw_primitive:  Mark the end of vertices for a
 * primitive object created with graphics_begin_primitive(), then
 * immediately draw and destroy the primitive object.  On some systems,
 * this may result in better performance than separately creating and
 * drawing the primitive.
 *
 * The behavior of this function is undefined if it is not called between
 * graphics_start_frame() and graphics_finish_frame().
 *
 * [Return value]
 *     True on success, false on error.
 */
extern int graphics_end_and_draw_primitive(void);

/**
 * graphics_create_primitive:  Create a new graphics primitive object from
 * a preexisting vertex data buffer.
 *
 * The "format" parameter describes the format of the vertex data and points
 * to an array of uint32_t values generated by the GRAPHICS_VERTEX_FORMAT()
 * macro, terminated by a value of zero.  Unrecognized or unsupported
 * format values will cause an error.  When using the standard rendering
 * pipeline (as opposed to shader objects), the following limitations are
 * imposed upon the vertex format:
 *    - If the format does not include a POSITION_* parameter, behavior is
 *      undefined except in that the program will not crash: primitive
 *      creation may fail, or if it succeeds, attempting to draw the
 *      primitive may or may not result in output to the render target.
 *    - If a texture has been applied with texture_apply() but the format
 *      does not include a TEXCOORD_* parameter, the primitive will be
 *      rendered according to the other format parameters but texturing
 *      behavior is undefined (as with graphics_add_vertex()).
 *
 * [Parameters]
 *     type: Primitive type (GRAPHICS_PRIMITIVE_*).
 *     data: Vertex data pointer.
 *     format: Vertex data format description.
 *     size: Size of a single vertex, in bytes.
 *     count: Number of vertices.
 * [Return value]
 *     Primitive ID (nonzero), or zero on error.
 */
extern int graphics_create_primitive(
    GraphicsPrimitiveType type, const void *data, const uint32_t *format,
    int size, int count);

/**
 * graphics_create_indexed_primitive:  Create a new graphics primitive
 * object from preexisting vertex and index data buffers.  The result of
 * rendering an indexed primitive is identical to that of rendering a
 * non-indexed primitive with the vertices ordered as listed by the index
 * array, but using an indexed primitive may be faster if there are a lot
 * of repeated vertices.
 *
 * The "vertex_format" parameter is treated identically to the "format"
 * parameter to graphics_create_primitive().  "index_size" specifies the
 * size of each element of the index array in bytes, and must be 1, 2, or
 * 4 (indicating 8-bit, 16-bit, or 32-bit unsigned integers, respectively).
 * Some platforms do not support 32-bit indices.
 *
 * [Parameters]
 *     type: Primitive type (GRAPHICS_PRIMITIVE_*).
 *     vertex_data: Vertex data pointer.
 *     vertex_format: Vertex data format description.
 *     vertex_size: Size of a single vertex, in bytes.
 *     vertex_count: Number of vertices in vertex data buffer.
 *     index_data: Index data pointer.
 *     index_size: Size of an index value, in bytes.
 *     index_count: Number of elements in index array (= number of
 *         vertices to render).
 * [Return value]
 *     Primitive ID (nonzero), or zero on error.
 */
extern int graphics_create_indexed_primitive(
    GraphicsPrimitiveType type, const void *vertex_data,
    const uint32_t *vertex_format, int vertex_size, int vertex_count,
    const void *index_data, int index_size, int index_count);

/**
 * graphics_draw_primitive:  Draw a previously-created primitive.
 *
 * The behavior of this function is undefined if it is not called between
 * graphics_start_frame() and graphics_finish_frame().
 *
 * [Parameters]
 *     primitive: ID of primitive to draw.
 */
extern void graphics_draw_primitive(int primitive);

/**
 * graphics_draw_primitive_partial:  Draw a subset of vertices from a
 * previously-created primitive.
 *
 * For primitives of type LINES, TRIANGLES, and QUADS, the caller is
 * responsible for ensuring that the values passed to the start and count
 * parameters are aligned to a multiple of the basic primitive unit (for
 * example, start and count should be multiples of 3 for a TRIANGLES
 * primitive).  The result of rendering with unaligned start or count
 * values is implementation-dependent and is not necessarily the same as
 * the result of fully rendering a primitive object containing the data
 * specified by start and count; in particular, on systems which internally
 * convert QUADS primitives to triangle primitives, passing a start value
 * which is not a multiple of 4 will behave differently from a non-partial
 * draw call with the same vertex data.
 *
 * The behavior of this function is undefined if it is not called between
 * graphics_start_frame() and graphics_finish_frame().
 *
 * [Parameters]
 *     primitive: ID of primitive to draw.
 *     start: Zero-based index of first vertex to draw.
 *     count: Number of vertices to draw, or a negative value to draw all
 *         remaining vertices in the primitive.
 */
extern void graphics_draw_primitive_partial(int primitive, int start,
                                            int count);

/**
 * graphics_draw_vertices:  Render a graphics primitive from a preexisting
 * vertex data buffer.  Analogous to graphics_create_primitive(), but this
 * function renders the primitive immediately rather than creating a new
 * primitive object for later rendering.
 *
 * The behavior of this function is undefined if it is not called between
 * graphics_start_frame() and graphics_finish_frame().
 *
 * [Parameters]
 *     type: Primitive type (GRAPHICS_PRIMITIVE_*).
 *     data: Vertex data pointer.
 *     format: Vertex data format description.
 *     size: Size of a single vertex, in bytes.
 *     count: Number of vertices.
 */
extern void graphics_draw_vertices(
    GraphicsPrimitiveType type, const void *data, const uint32_t *format,
    int size, int count);

/**
 * graphics_draw_indexed_vertices:  Render a graphics primitive from
 * preexisting vertex and index data buffers.  Analogous to
 * graphics_create_indexed_primitive(), but this function renders the
 * primitive immediately rather than creating a new primitive object for
 * later rendering.
 *
 * The behavior of this function is undefined if it is not called between
 * graphics_start_frame() and graphics_finish_frame().
 *
 * [Parameters]
 *     type: Primitive type (GRAPHICS_PRIMITIVE_*).
 *     vertex_data: Vertex data pointer.
 *     vertex_format: Vertex data format description.
 *     vertex_size: Size of a single vertex, in bytes.
 *     vertex_count: Number of vertices in vertex data buffer.
 *     index_data: Index data pointer.
 *     index_size: Size of an index value, in bytes.
 *     index_count: Number of elements in index array (= number of
 *         vertices to render.
 */
extern void graphics_draw_indexed_vertices(
    GraphicsPrimitiveType type, const void *vertex_data,
    const uint32_t *vertex_format, int vertex_size, int vertex_count,
    const void *index_data, int index_size, int index_count);

/**
 * graphics_destroy_primitive:  Destroy a primitive.  Does nothing if
 * primitive == 0.
 *
 * [Parameters]
 *     primitive: ID of primitive to destroy.
 */
extern void graphics_destroy_primitive(int primitive);

/*--------------------------- Shader control ----------------------------*/

/**
 * graphics_use_shader_objects:  Set whether to use shader objects or
 * automatically generated shaders when drawing primitives.
 *
 * When shader objects are enabled (enable is nonzero), primitive drawing
 * will be controlled by the shader pipeline installed with the
 * shader_pipeline_apply() function (see <SIL/shader.h> for details).  In
 * this case, all of the shader functions below, as well as many of the
 * render state manipulation functions (see the documentation for each
 * function), have no effect on draw operations; also, unless otherwise
 * configured with shader_set_standard_attribute(), the standard position,
 * texture coordinate, and color attributes used by graphics_add_vertex()
 * will not be passed to vertex shaders.  The caller is responsible for
 * configuring shader parameters and vertex formats accordingly.
 *
 * When shader objects are disabled (enable is zero), SIL's standard
 * pipeline is used to draw primitives.  On systems which support shaders,
 * SIL will automatically generate shaders appropriate to the primitive
 * type and render state; the default shader generator can be overridden
 * using graphics_set_shader_generator() and the other shader functions
 * below.
 *
 * Calling this function clears all user-defined shader attributes
 * registered with graphics_add_shader_attribute() (for the shader
 * generator API) and shader_set_attribute()/shader_bind_standard_attribute()
 * (for the shader object API), as well as the shader generator callbacks
 * set with graphics_set_shader_generator().  This is true even if the
 * call to this function does not change the shader object enable state.
 *
 * The default is to use the standard rendering pipeline, equivalent to
 * graphics_use_shader_objects(0).
 *
 * [Parameters]
 *     True to use shader objects, false to use the standard rendering
 *     pipeline.
 * [Return value]
 *     True on success, false on error (including if enable is true and
 *     the system does not support shader objects).
 */
extern int graphics_use_shader_objects(int enable);

/**
 * graphics_set_shader_generator:  Set the callback functions and
 * associated parameters to be used when generating shaders for primitive
 * rendering.  On success, this function invalidates and destroys any
 * shaders which were previously created.  If the runtime environment does
 * not support shaders or cannot compile GLSL shader source code, this
 * function always fails.
 *
 * Calling graphics_use_shader_objects() has the implicit effect of calling
 * this function with NULL for all generator function pointers.
 *
 * Overview
 * --------
 *
 * On systems which support shaders, SIL automatically generates shaders
 * for each primitive type and associated render state when the primitive
 * is drawn.  The default shader generator renders primitives in the same
 * manner as the classic OpenGL fixed-function pipeline.  If a program
 * desires more complex shader behavior but does not want to manage all
 * render state on its own (as with graphics_use_shader_objects(1)), it
 * can call this function to define its own shader generator functions,
 * which will then be used in place of the default generator when a new
 * shader is required.
 *
 * To determine the proper shader for a given primitive and render state,
 * the renderer calls the key_callback function.  This function should
 * return a key value which uniquely identifies the shader to be used to
 * render the primitive.  (The value itself has no special meaning, except
 * that INVALID_SHADER_KEY is used to indicate that a particular primitive
 * cannot be rendered.)  SIL relies solely on this key to select a shader;
 * thus, for example, the same primitive can be rendered using two
 * different shaders even if SIL's internal render state is identical, by
 * returning different keys for the two render calls (perhaps based on the
 * value of a state variable specific to the program).
 *
 * If the key_callback function returns a key which has not been seen
 * before, the renderer then calls the vertex_source_callback and
 * fragment_source_callback function to generate the source code for the
 * vertex and fragment shaders to be used.  There are no restrictions on
 * the behavior of the generated shaders, but it is the responsibility of
 * the shader generator to ensure that the proper shaders are generated
 * for each render state, particularly if the generator uses external
 * state to select a shader.
 *
 * When a custom shader generator is installed using this function, it
 * completely replaces the default generator used by SIL, and it therefore
 * must generate shaders for every combination of primitive and render
 * state used by the program, even for those cases where the default
 * behavior is desired.  If the program requires only small adjustments to
 * the default generator, it may be easiest to copy those functions (the
 * generate_vertex_shader_source(), generate_fragment_shader_source(), and
 * generate_shader_key() functions in src/sysdep/opengl/shader.c) and make
 * the desired modifications.
 *
 * Source code format
 * ------------------
 *
 * Shader source code should be written in GLSL ES 1.00 style, using
 * precision specifiers ("highp", "lowp", etc.) as needed, except that
 * "attribute" and "varying" should be replaced with "in" and "out" as
 * appropriate, and the fragment shader's color output should be written
 * to the "color_out" variable (which will be predeclared when the shader
 * is compiled).  The code will be preprocessed according to the particular
 * OpenGL variant and shading language version in use.
 *
 * When specifying precision for texture coordinates, shader code may use
 * the precision "texp", which will be "highp" if the graphics hardware
 * supports it in fragment shaders and "mediump" otherwise.  This allows
 * texel-precise coordinates in large textures on modern hardware while
 * safely falling back (with a loss of image quality) on older hardware.
 *
 * The following identifiers must be used for the standard uniforms and
 * vertex attributes:
 *
 *     transform: (uniform mat4) Model-view-projection transformation matrix.
 *     tex: (uniform sampler2D) Texture sampler.
 *     tex_offset: (uniform vec2) Texture offset for all vertices.
 *     fixed_color: (uniform vec4) Fixed color multiplier for all vertices.
 *     fog_params: (uniform vec2) Fog parameters; see default generator in
 *         src/sysdep/opengl/shader.c for details.
 *     fog_transform: (uniform mat4) Z column of the model-view transformation
 *         matrix for computing fog coordinates.
 *     fog_color: (uniform vec4) Fog color.
 *     alpha_ref: (uniform float) Reference value for alpha testing.
 *     position: (in vec[234]) Vertex position.
 *     texcoord: (in vec2) Texture coordinates for vertex.
 *     color: (in vec4) Color for vertex.
 *
 * Custom uniforms and vertex attributes can be defined by calling
 * graphics_add_shader_uniform() or graphics_add_shader_attribute(),
 * respectively.  Calling graphics_set_shader_generator() clears all
 * custom uniforms and vertex attributes which were previously added.
 *
 * Performance notes
 * -----------------
 *
 * Since each shader must be compiled the first time it is used, changing
 * to a new render state may cause a slight delay in program execution.
 * If this is a problem, it can be avoided by pre-rendering primitives of
 * the appropriate type to an offscreen framebuffer as part of an
 * initialization process, for example.  Once a shader has been used, it
 * is cached in memory indefinitely (unless this function is called again
 * at a later point in time).
 *
 * When choosing an initial hash table size, keep in mind that dynamically
 * resizing the hash table can be an expensive operation.  If memory
 * permits (32 bytes per table entry), set the initial table size large
 * enough to hold all possible permutations of shader parameters.  (The
 * default generator uses such a fixed table size.)
 *
 * [Parameters]
 *     vertex_source_callback: Source code generation function to call
 *         for vertex shaders, or NULL to use the the default generator.
 *     fragment_source_callback: Source code generation function to call
 *         for fragment shaders, or NULL to use the the default generator.
 *         Mandatory if vertex_source_callback is not NULL; must be NULL
 *         otherwise.
 *     key_callback: Key generation function to call.  Mandatory if
 *         vertex_source_callback is not NULL; must be NULL otherwise.
 *     hash_table_size: Initial size of the shader hash table.  Ignored if
 *         vertex_source_callback is NULL.
 *     dynamic_resize: True to allow dynamic resizing of the hash table,
 *         false to flush old shaders when the table is full.  Ignored if
 *         vertex_source_callback is NULL.
 * [Return value]
 *     True on success, false on error or if the system does not support
 *     shaders.
 */
extern int graphics_set_shader_generator(
    ShaderSourceCallback *vertex_source_callback,
    ShaderSourceCallback *fragment_source_callback,
    ShaderKeyCallback *key_callback, int hash_table_size, int dynamic_resize);

/**
 * graphics_add_shader_uniform:  Define a custom uniform (constant) to be
 * used with a shader generator.  Once defined, the uniform's value can be
 * set by calling the graphics_set_shader_uniform_*() function appropriate
 * to the value type.
 *
 * This function may be called to add new uniforms after one or more
 * shaders have been generated, but only those uniforms defined at the
 * time a particular shader was generated will be available in that shader.
 *
 * Calling graphics_set_shader_generator() clears all custom uniforms
 * defined with this function.
 *
 * See shader_get_uniform_id() in <SIL/shader.h> for accessing uniforms
 * defined in shader objects.
 *
 * [Parameters]
 *     name: Uniform name, as used in shader source code.
 * [Return value]
 *     Uniform identifier (nonzero), or zero on error.
 */
extern int graphics_add_shader_uniform(const char *name);

/**
 * graphics_add_shader_attribute:  Define a custom vertex attribute to be
 * used with a shader generator.  Once defined, data can be passed to the
 * attribute by using the returned value in a USER() vertex format type.
 * For example:
 *
 *     typedef struct Vertex {
 *         Vector3f position;
 *         Vector3f normal;
 *     } Vertex;
 *     uint32_t format[] = {
 *         GRAPHICS_VERTEX_FORMAT(POSITION_3F, offsetof(Vertex,position)),
 *         GRAPHICS_VERTEX_FORMAT(USER(0),  // Rewritten below.
 *                                offsetof(Vertex,normal)),
 *         0
 *     };
 *     const int normal_id = graphics_add_shader_attribute("normal", 3);
 *     format[1] = GRAPHICS_VERTEX_FORMAT(USER(normal_id),
 *                                        offsetof(Vertex,normal));
 *
 * All custom vertex attributes must be either scalar floating-point values
 * or floating-point vectors of 2-4 components.
 *
 * SIL imposes a limit of 4095 custom vertex attributes across all shaders.
 * This function will fail if defining a new vertex attribute would exceed
 * that limit.
 *
 * This function may be called to add new vertex attributes after one or
 * more shaders have been generated, but only those attributes defined at
 * the time a particular shader was generated will be available in that
 * shader.
 *
 * Calling graphics_set_shader_generator() clears all custom vertex
 * attributes defined with this function.
 *
 * See shader_set_attribute() and shader_bind_standard_attribute() in
 * <SIL/shader.h> for specifying attributes in shader objects.
 *
 * [Parameters]
 *     name: Attribute name, as used in shader source code.
 *     size: Number of values in attribute (1-4).
 * [Return value]
 *     Attribute identifier (nonzero), or zero on error.
 */
extern int graphics_add_shader_attribute(const char *name, int size);

/**
 * graphics_set_shader_uniform_*:  Set the value of a custom uniform for
 * shaders created with a custom shader generator.
 *
 * The data type of a custom uniform is fixed when the uniform's value is
 * set for the first time; subsequent set operations will fail if they do
 * not use the same data type.  Behavior when the data type is incompatible
 * with the uniform's declaration in the shader is undefined.
 *
 * The value of a texture sampler uniform is the index of the texture unit
 * it accesses (as in the unit parameter to texture_apply()), and the data
 * type is int for the purpose of these functions.
 *
 * See shader_set_uniform() in <SIL/shader.h> for setting uniforms defined
 * in shader objects.
 *
 * [Parameters]
 *     uniform: Uniform identifier.
 *     value: Value to set.
 */
extern void graphics_set_shader_uniform_int(int uniform, int value);
extern void graphics_set_shader_uniform_float(int uniform, float value);
extern void graphics_set_shader_uniform_vec2(int uniform,
                                             const struct Vector2f *value);
extern void graphics_set_shader_uniform_vec3(int uniform,
                                             const struct Vector3f *value);
extern void graphics_set_shader_uniform_vec4(int uniform,
                                             const struct Vector4f *value);
extern void graphics_set_shader_uniform_mat4(int uniform,
                                             const struct Matrix4f *value);

/*--------------- Coordinate transformation and clipping ----------------*/

/**
 * graphics_set_viewport:  Set the render viewport position and size.
 * This controls the mapping of transformed (device space) coordinates to
 * window coordinates, but does not affect calls which operate directly on
 * the display or framebuffer (such as graphics_clear() or
 * texture_create_from_display()).
 *
 * All parameters are in units of framebuffer pixels.
 *
 * [Parameters]
 *     left: Leftmost X coordinate of the viewport.
 *     bottom: Bottommost Y coordinate of the viewport.
 *     width: Width of the viewport.
 *     height: Height of the viewport.
 */
extern void graphics_set_viewport(int left, int bottom, int width, int height);

/**
 * graphics_viewport_width, graphics_viewport_height:  Return the width or
 * height of the render viewport.
 *
 * [Return value]
 *     Render viewport width or height, in framebuffer pixels.
 */
extern int graphics_viewport_width(void);
extern int graphics_viewport_height(void);

/**
 * graphics_set_clip_region:  Set the clipping region for rendering and
 * clearing.  If both width and height are zero, the clipping region is
 * cleared (allowing rendering to the entire display); otherwise, both
 * width and height must be nonzero.  The default is to have no clipping
 * region.
 *
 * All parameters are in units of framebuffer pixels and are independent
 * of the viewport setting.  If the clip region and viewport region do not
 * intersect, nothing will be drawn.
 *
 * [Parameters]
 *     left: Leftmost X coordinate of the clipping region.
 *     bottom: Bottommost Y coordinate of the clipping region.
 *     width: Width of the clipping region.
 *     height: Height of the clipping region.
 */
extern void graphics_set_clip_region(int left, int bottom, int width, int height);

/**
 * graphics_set_depth_range:  Set the depth range mapping, which is used
 * to convert device-space depth coordinates to depth buffer values.  The
 * default setting is equivalent to graphics_set_depth_range(0, 1), which
 * maps depth values to the full range of the depth buffer.
 *
 * The values passed to this function must satisfy 0 <= near < far <= 1.
 *
 * [Parameters]
 *     near: Window coordinate of the near clipping plane.
 *     far: Window coordinate of the far clipping plane.
 */
extern void graphics_set_depth_range(float near, float far);

/**
 * graphics_set_projection_matrix:  Set the projection transformation
 * matrix to the given data.  The default is the identity matrix.
 *
 * This function has no direct effect on draw operations when shader
 * objects are enabled, though the matrix can still be retrieved by
 * calling graphics_get_projection_matrix().
 *
 * [Parameters]
 *     matrix: Pointer to matrix data.
 */
extern void graphics_set_projection_matrix(const struct Matrix4f *matrix);

/**
 * graphics_set_view_matrix:  Set the view transformation matrix to the
 * given data.  The default is the identity matrix.
 *
 * This function has no direct effect on draw operations when shader
 * objects are enabled, though the matrix can still be retrieved by
 * calling graphics_get_view_matrix().
 *
 * [Parameters]
 *     matrix: Pointer to matrix data.
 */
extern void graphics_set_view_matrix(const struct Matrix4f *matrix);

/**
 * graphics_set_model_matrix:  Set the model transformation matrix to the
 * given data.  The default is the identity matrix.
 *
 * This function has no direct effect on draw operations when shader
 * objects are enabled, though the matrix can still be retrieved by
 * calling graphics_get_model_matrix().
 *
 * [Parameters]
 *     matrix: Pointer to matrix data.
 */
extern void graphics_set_model_matrix(const struct Matrix4f *matrix);

/**
 * graphics_get_projection_matrix:  Retrieve the current contents of the
 * projection transformation matrix.
 *
 * [Parameters]
 *     matrix_ret: Pointer to variable to receive matrix data.
 */
extern void graphics_get_projection_matrix(struct Matrix4f *matrix_ret);

/**
 * graphics_get_view_matrix:  Retrieve the current contents of the view
 * transformation matrix.
 *
 * [Parameters]
 *     matrix_ret: Pointer to variable to receive matrix data.
 */
extern void graphics_get_view_matrix(struct Matrix4f *matrix_ret);

/**
 * graphics_get_model_matrix:  Retrieve the current contents of the view
 * transformation matrix.
 *
 * [Parameters]
 *     matrix_ret: Pointer to variable to receive matrix data.
 */
extern void graphics_get_model_matrix(struct Matrix4f *matrix_ret);

/**
 * graphics_make_parallel_projection:  Sets the given matrix to a parallel
 * projection matrix defined by the parameters to the function.  The result
 * is undefined if any of the three coordinate ranges (right - left,
 * top - bottom, or far - near) is zero.
 *
 * [Parameters]
 *     left: X coordinate for the left edge of the display.
 *     right: X coordinate for the right edge of the display.
 *     bottom: Y coordinate for the bottom edge of the display.
 *     top: Y coordinate for the top edge of the display.
 *     near: Z coordinate for the near clipping plane.
 *     far: Z coordinate for the far clipping plane.
 *     matrix_ret: Pointer to variable into which to store the result.
 */
extern void graphics_make_parallel_projection(
    float left, float right, float bottom, float top, float near, float far,
    struct Matrix4f *matrix_ret);

/**
 * graphics_set_parallel_projection:  Sets the projection matrix to a
 * parallel transformation matrix defined by the parameters to the
 * function.  Exactly equivalent to:
 *     Matrix4f matrix;
 *     graphics_make_parallel_projection(
 *         left, right, bottom, top, near, far, &matrix);
 *     graphics_set_projection_matrix(&matrix);
 *
 * [Parameters]
 *     left: X coordinate for the left edge of the display.
 *     right: X coordinate for the right edge of the display.
 *     bottom: Y coordinate for the bottom edge of the display.
 *     top: Y coordinate for the top edge of the display.
 *     near: Z coordinate for the near clipping plane.
 *     far: Z coordinate for the far clipping plane.
 */
extern void graphics_set_parallel_projection(
    float left, float right, float bottom, float top, float near, float far);

/**
 * graphics_make_perspective_projection:  Sets the given matrix to a
 * perspective projection matrix defined by the parameters to the function.
 * The result is undefined if any parameter's value (other than
 * right_handed) is zero or if far is equal to near.
 *
 * [Parameters]
 *     y_fov: Field-of-view angle in the Y (vertical) direction, in degrees.
 *     aspect: Display aspect ratio (width divided by height).
 *     near: Z-axis distance from the viewer to the near clipping plane.
 *     far: Z-axis distance from the viewer to the far clipping plane.
 *     right_handed: True if the world coordinate system is right-handed
 *         (positive Z extends out of display, toward viewer); false if
 *         the world coordinate system is left-handed (positive Z extends
 *         into display, away from viewer).
 *     matrix_ret: Pointer to variable into which to store the result.
 */
extern void graphics_make_perspective_projection(
    float y_fov, float aspect, float near, float far, int right_handed,
    struct Matrix4f *matrix_ret);

/**
 * graphics_set_perspective_projection:  Sets the projection matrix to a
 * perspective transformation matrix defined by the parameters to the
 * function.  Exactly equivalent to:
 *     Matrix4f matrix;
 *     graphics_make_perspective_projection(
 *         y_fov, aspect, near, far, right_handed, &matrix);
 *     graphics_set_projection_matrix(&matrix);
 *
 * [Parameters]
 *     y_fov: Field-of-view angle in the Y (vertical) direction, in degrees.
 *     aspect: Display aspect ratio (width divided by height).
 *     near: Z-axis distance from the viewer to the near clipping plane.
 *     far: Z-axis distance from the viewer to the far clipping plane.
 *     right_handed: True if the world coordinate system is right-handed
 *         (positive Z extends out of display, toward viewer); false if
 *         the world coordinate system is left-handed (positive Z extends
 *         into display, away from viewer).
 */
extern void graphics_set_perspective_projection(
    float y_fov, float aspect, float near, float far, int right_handed);

/*------------------- Other render state manipulation -------------------*/

/**
 * graphics_enable_alpha_test:  Enable or disable alpha testing.  The
 * default is disabled (alpha testing is not performed).
 *
 * This function has no effect on draw operations when shader objects are
 * enabled.
 *
 * [Parameters]
 *     on: True to enable, false to disable.
 */
extern void graphics_enable_alpha_test(int on);

/**
 * graphics_set_alpha_test_comparison:  Set the type of comparison to use
 * for alpha testing.  The default is GRAPHICS_COMPARISON_GREATER_EQUAL,
 * meaning that pixels pass the alpha test if their alpha values are greater
 * than or equal to the reference value.
 *
 * The only permitted comparisons are LESS, LESS_EQUAL, GREATER, and
 * GREATER_EQUAL.
 *
 * This function has no effect on draw operations when shader objects are
 * enabled.
 *
 * [Parameters]
 *     type: Comparison type to use.
 */
extern void graphics_set_alpha_test_comparison(GraphicsComparisonType type);

/**
 * graphics_set_alpha_test_reference:  Set the reference value for alpha
 * testing.  The value is clamped to the inclusive range [0,1].  The
 * default is 0.
 *
 * This function has no effect on draw operations when shader objects are
 * enabled.
 *
 * [Parameters]
 *     alpha: Alpha reference value.
 */
extern void graphics_set_alpha_test_reference(float alpha);

/**
 * graphics_set_blend:  Set the pixel blending operation and pixel factors.
 * The default is to perform standard alpha blending, equivalent to:
 *     graphics_set_blend(GRAPHICS_BLEND_ADD,
 *                        GRAPHICS_BLEND_SRC_ALPHA,
 *                        GRAPHICS_BLEND_INV_SRC_ALPHA)
 *
 * Note that on some systems, opaque images can be rendered more
 * efficiently if blending is disabled, which is accomplished by calling
 * graphics_set_blend(GRAPHICS_BLEND_ADD, 1, 0) or graphics_set_no_blend().
 *
 * [Parameters]
 *     operation: Blending operation (GRAPHICS_BLEND_ADD, etc.).
 *     src_factor: Source (new) pixel blending factor (GRAPHICS_BLEND_*).
 *     dest_factor: Destination (old) pixel blending factor (GRAPHICS_BLEND_*).
 * [Return value]
 *     True on success, false if the requested parameters cannot be set.
 */
extern int graphics_set_blend(GraphicsBlendOperation operation,
                              GraphicsBlendFactor src_factor,
                              GraphicsBlendFactor dest_factor);

/**
 * graphics_set_blend_alpha:  Set alternate blend functions for rendering
 * operations on the alpha channel of a framebuffer.
 *
 * By default, independent blending of the alpha channel is disabled.
 *
 * [Parameters]
 *     enable: True to enable independent alpha blending; false to use the
 *         color blend factors for the alpha channel.
 *     src_factor: Source (new) pixel blending factor for the alpha
 *         channel (GRAPHICS_BLEND_*).  Ignored if enable is false.
 *     dest_factor: Destination (old) pixel blending factor for the alpha
 *         channel (GRAPHICS_BLEND_*).  Ignored if enable is false.
 * [Return value]
 *     True on success, false if the requested parameters cannot be set.
 */
extern int graphics_set_blend_alpha(int enable, GraphicsBlendFactor src_factor,
                                    GraphicsBlendFactor dest_factor);

/**
 * graphics_set_no_blend:  Disable pixel blending.  Exactly equivalent to:
 *     graphics_set_blend(GRAPHICS_BLEND_ADD,
 *                        GRAPHICS_BLEND_ONE, GRAPHICS_BLEND_ZERO);
 *     graphics_set_blend_alpha(0, 0, 0);
 *
 * This function always succeeds.
 */
extern void graphics_set_no_blend(void);

/**
 * graphics_set_blend_color:  Set the color used with the BLEND_CONSTANT
 * and BLEND_INV_CONSTANT blending modes.  Color components are clamped to
 * [0,1].  The default value is (0,0,0,0), equivalent to GRAPHICS_BLEND_ZERO.
 *
 * [Parameters]
 *     color: Blend color.
 */
extern void graphics_set_blend_color(const struct Vector4f *color);

/**
 * graphics_enable_color_write:  Enable or disable writing to the color
 * buffer.  Each color component can be enabled or disabled independently.
 * The default is enabled for all components.
 *
 * [Parameters]
 *     red: True to enable writing to the red component, false to disable.
 *     green: True to enable writing to the green component, false to disable.
 *     blue: True to enable writing to the blue component, false to disable.
 *     alpha: True to enable writing to the alpha component, false to disable.
 */
extern void graphics_enable_color_write(int red, int green, int blue,
                                        int alpha);

/**
 * graphics_enable_depth_test:  Enable or disable depth testing.  The
 * default is disabled.
 *
 * [Parameters]
 *     on: True to enable, false to disable.
 */
extern void graphics_enable_depth_test(int on);

/**
 * graphics_set_depth_test_comparison:  Set the type of comparison to use
 * for depth testing.  The default is GRAPHICS_COMPARISON_LESS, meaning
 * that pixels pass the depth test if their depth values are strictly less
 * than (closer to the viewer, under the default coordinate transformations)
 * the corresponding value stored in the depth buffer.
 *
 * The only permitted comparisons are LESS, LESS_EQUAL, GREATER, and
 * GREATER_EQUAL.
 *
 * [Parameters]
 *     type: Comparison type to use.
 */
extern void graphics_set_depth_test_comparison(GraphicsComparisonType type);

/**
 * graphics_enable_depth_write:  Enable or disable updating of the depth
 * buffer when rendering.  Regardless of this setting, depth values are not
 * updated if depth testing is disabled.  The default is enabled.
 *
 * [Parameters]
 *     on: True to enable, false to disable.
 */
extern void graphics_enable_depth_write(int on);

/**
 * graphics_set_face_cull:  Set the face culling mode.  The default is
 * GRAPHICS_FACE_CULL_NONE (face culling is not performed).
 *
 * [Parameters]
 *     mode: Face culling mode.
 */
extern void graphics_set_face_cull(GraphicsFaceCullMode mode);

/**
 * graphics_set_fixed_color:  Set the fixed color value to apply to
 * primitives.  Each vertex's color is multiplied by this color when
 * rendering.  Color components are clamped to the range [0,1].  The
 * default value is (1,1,1,1) -- i.e., no color change.
 *
 * This function has no effect on draw operations when shader objects are
 * enabled.
 *
 * [Parameters]
 *     color: Fixed primitive color.
 */
extern void graphics_set_fixed_color(const struct Vector4f *color);

/**
 * graphics_enable_fog:  Enable or disable fog.  Only linear fog is
 * currently supported; the behavior of fog on fragments behind the eye
 * origin is undefined.  The default is disabled.
 *
 * This function has no effect on draw operations when shader objects are
 * enabled.
 *
 * [Parameters]
 *     on: True to enable, false to disable.
 */
extern void graphics_enable_fog(int on);

/**
 * graphics_set_fog_start:  Set the distance at which linear fog is first
 * applied.  The default is 0.
 *
 * This function has no effect on draw operations when shader objects are
 * enabled.
 *
 * [Parameters]
 *     distance: Fog start distance.
 */
extern void graphics_set_fog_start(float distance);

/**
 * graphics_set_fog_end:  Set the distance at which linear fog is fully
 * applied.  The default is 1.
 *
 * Attempting to render a primitive with fog enabled and start == end
 * results in undefined behavior.
 *
 * This function has no effect on draw operations when shader objects are
 * enabled.
 *
 * [Parameters]
 *     distance: Fog end distance.
 */
extern void graphics_set_fog_end(float distance);

/**
 * graphics_set_fog_color:  Set the fog color (the alpha component is
 * ignored).  Color component values are clamped to the range [0,1].
 * The default is (1,1,1,1).
 *
 * This function has no effect on draw operations when shader objects are
 * enabled.
 *
 * [Parameters]
 *     color: Fog color.
 */
extern void graphics_set_fog_color(const struct Vector4f *color);

/**
 * graphics_set_point_size:  Set the point diameter used for point
 * primitives.  The default is 1.
 *
 * This function has no effect on draw operations when shader objects are
 * enabled.
 *
 * [Parameters]
 *     size: Point diameter, in pixels.
 */
extern void graphics_set_point_size(float size);

/**
 * graphics_max_point_size:  Return the maximum point size supported for
 * point primitives.  The returned value is always at least 1.
 *
 * [Return value]
 *     Maximum point size, in pixels.
 */
extern float graphics_max_point_size(void);

/**
 * graphics_enable_stencil_test:  Enable or disable stencil testing.  The
 * default is disabled.
 *
 * [Parameters]
 *     on: True to enable, false to disable.
 */
extern void graphics_enable_stencil_test(int on);

/**
 * graphics_set_stencil_comparison:  Set the stencil comparison type and
 * reference value.  The defaults are:
 *    type = GRAPHICS_COMPARISON_TRUE (stencil test always passes)
 *    reference = 0
 *    mask = ~0
 *
 * Comparisons are performed as:
 *    (stencil_buffer_value & mask) <=> (reference & mask)
 * so that, for example, GRAPHICS_COMPARISON_LESS passes pixels for which
 * the masked stencil buffer value at that pixel is less than the masked
 * reference value.  Note that this is the converse of OpenGL, which puts
 * the reference value on the left side of the comparison; this order was
 * chosen because it matches the order used for depth and alpha tests.
 *
 * [Parameters]
 *     type: Comparison type to use.
 *     reference: Reference value for comparison.
 *     mask: Bitmask to apply to input stencil buffer value and
 *         reference value.
 */
extern void graphics_set_stencil_comparison(
    GraphicsComparisonType type, unsigned int reference, unsigned int mask);

/**
 * graphics_set_stencil_operations:  Set the stencil operations to be
 * performed based on whether each pixel passes the stencil and depth tests.
 * The default for all operations is GRAPHICS_STENCIL_KEEP.
 *
 * [Parameters]
 *     sfail: Operation to perform when the stencil test fails.
 *     dfail: Operation to perform when the stencil test passes but the
 *         depth test fails.
 *     dpass: Operation to perform when the stencil test passes and the
 *         depth test passes or is disabled.
 */
extern void graphics_set_stencil_operations(
    GraphicsStencilOp sfail, GraphicsStencilOp dfail, GraphicsStencilOp dpass);

/**
 * graphics_set_texture_offset:  Set the offset to add to texture
 * coordinates when rendering textured primitives.  The default is no
 * offset (both components zero).
 *
 * This function has no effect on draw operations when shader objects are
 * enabled.
 *
 * [Parameters]
 *     offset: Texture coordinate offset.
 */
extern void graphics_set_texture_offset(const struct Vector2f *offset);

/*************************************************************************/
/***************************** C++ wrappers ******************************/
/*************************************************************************/

#ifdef __cplusplus
EXTERN_C_END

/*
 * The following macro wraps a function taking an input pointer parameter
 * such that the function can also be called (from C++ only) using a
 * non-pointer value of the same type.  For example:
 *    void graphics_set_projection_matrix(const struct Matrix4f *matrix)
 * is wrapped by:
 *    void graphics_set_projection_matrix(const struct Matrix4f &matrix)
 * allowing a call of the form:
 *    graphics_set_projection_matrix(Matrix4f(...));
 *
 * This is intended mainly to work around the lack of C99-style compound
 * literals in C++.
 */

#define WRAP_FUNCTION_0(NAME,TYPE) \
    static inline void NAME(const struct TYPE &value) {NAME(&value);}
#define WRAP_FUNCTION_1(NAME,TYPE0,TYPE) \
    static inline void NAME(TYPE0 arg0, const struct TYPE &value) {NAME(arg0, &value);}

WRAP_FUNCTION_0(graphics_set_blend_color,         Vector4f)
WRAP_FUNCTION_0(graphics_set_fixed_color,         Vector4f)
WRAP_FUNCTION_0(graphics_set_fog_color,           Vector4f)
WRAP_FUNCTION_0(graphics_set_model_matrix,        Matrix4f)
WRAP_FUNCTION_0(graphics_set_projection_matrix,   Matrix4f)
WRAP_FUNCTION_1(graphics_set_shader_uniform_vec2, int, Vector2f)
WRAP_FUNCTION_1(graphics_set_shader_uniform_vec3, int, Vector3f)
WRAP_FUNCTION_1(graphics_set_shader_uniform_vec4, int, Vector4f)
WRAP_FUNCTION_1(graphics_set_shader_uniform_mat4, int, Matrix4f)
WRAP_FUNCTION_0(graphics_set_texture_offset,      Vector2f)
WRAP_FUNCTION_0(graphics_set_view_matrix,         Matrix4f)

#undef WRAP_FUNCTION_0
#undef WRAP_FUNCTION_1

/* The graphics_add_vertex() wrappers are written such that arguments
 * which would be NULL in the C interface are omitted from the C++ call. */
static inline void graphics_add_vertex(const Vector3f &position) {
    graphics_add_vertex(&position, NULL, NULL);
}
static inline void graphics_add_vertex(const Vector3f &position,
                                       const Vector2f &texcoord) {
    graphics_add_vertex(&position, &texcoord, NULL);
}
static inline void graphics_add_vertex(const Vector3f &position,
                                       const Vector4f &color) {
    graphics_add_vertex(&position, NULL, &color);
}
static inline void graphics_add_vertex(const Vector3f &position,
                                       const Vector2f &texcoord,
                                       const Vector4f &color) {
    graphics_add_vertex(&position, &texcoord, &color);
}

EXTERN_C_BEGIN
#endif  // __cplusplus

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_GRAPHICS_H
