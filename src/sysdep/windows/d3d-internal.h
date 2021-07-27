/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/windows/d3d-internal.h: Internal declarations for the
 * Direct3D implementation of the sysdep graphics functions.
 */

#ifndef SIL_SRC_SYSDEP_WINDOWS_D3D_INTERNAL_H
#define SIL_SRC_SYSDEP_WINDOWS_D3D_INTERNAL_H

#ifndef SIL_SRC_math_H
# include "src/math.h"
#endif
#ifndef SIL_SRC_SHADER_H
# include "src/shader.h"
#endif
#ifndef SIL_SRC_SYSDEP_WINDOWS_D3D_H
# include "src/sysdep/windows/d3d.h"
#endif
#ifndef SIL_SRC_SYSDEP_WINDOWS_INTERNAL_H
# include "src/sysdep/windows/internal.h"
#endif
#ifndef SIL_SRC_UTILITY_PIXFORMAT_H
# include "src/utility/pixformat.h"
#endif

#define COBJMACROS  // Required for declaration of C interface.
#include <d3d11.h>
#include <d3d11shader.h>
#include <d3dcompiler.h>

/* These defines are currently missing from the MinGW headers: */
#ifdef __MINGW32__
# define D3D11_VS_INPUT_REGISTER_COUNT  (32)
# define D3D11_ERROR_TOO_MANY_UNIQUE_STATE_OBJECTS  ((HRESULT)0x887C0001)
# define D3D11_ERROR_FILE_NOT_FOUND  ((HRESULT)0x887C0002)
# define D3D11_ERROR_TOO_MANY_UNIQUE_VIEW_OBJECTS  ((HRESULT)0x887C0003)
# define D3D11_ERROR_DEFERRED_CONTEXT_MAP_WITHOUT_INITIAL_DISCARD  ((HRESULT)0x887C0004)
# define D3DERR_INVALIDCALL  ((HRESULT)0x8876086C)
# define D3DERR_WASSTILLDRAWING  ((HRESULT)0x8876021C)
#endif

/* These declarations are missing from d3d11shader.h, even in Microsoft's
 * SDK headers: */
#define ID3D11ShaderReflection_GetConstantBufferByIndex(this,index)  (this)->lpVtbl->GetConstantBufferByIndex(this, index)
#define ID3D11ShaderReflection_Release(this)  (this)->lpVtbl->Release(this)
#define ID3D11ShaderReflectionConstantBuffer_GetDesc(this,desc_ret)  (this)->lpVtbl->GetDesc(this, desc_ret)
#define ID3D11ShaderReflectionConstantBuffer_GetVariableByName(this,name)  (this)->lpVtbl->GetVariableByName(this, name)
#define ID3D11ShaderReflectionVariable_GetDesc(this,desc_ret)  (this)->lpVtbl->GetDesc(this, desc_ret)

/*************************************************************************/
/******************* Shared internal data declarations *******************/
/*************************************************************************/

/**
 * d3d_context:  The ID3D11DeviceContext instance for the current window.
 */
extern ID3D11DeviceContext *d3d_context;

/**
 * d3d_device:  The ID3D11Device instance for the current window.
 */
extern ID3D11Device *d3d_device;

/**
 * d3d_device_generation:  The current Direct3D device generation number.
 * Resources associated with objects that have a different generation
 * number are invalid.
 */
extern unsigned int d3d_device_generation;

/**
 * d3d_feature_level:  The feature level reported by the device at
 * context creation time.
 */
extern D3D_FEATURE_LEVEL d3d_feature_level;

/**
 * d3d_compiler_name:  The name of the d3dcompiler_*.dll library which
 * was loaded at runtime, or NULL if no compiler library could be loaded.
 */
extern const char *d3dcompiler_name;


/* Pointers to dynamically looked-up functions from d3dcompiler_*.dll. */
extern HRESULT (WINAPI *p_D3DCompile)(
    LPCVOID pSrcData, SIZE_T SrcDataSize, LPCSTR pSourceName,
    const D3D_SHADER_MACRO *pDefines, ID3DInclude *pInclude,
    LPCSTR pEntrypoint, LPCSTR pTarget, UINT Flags1, UINT Flags2,
    ID3DBlob **ppCode, ID3DBlob **ppErrorMsgs);
extern HRESULT (WINAPI *p_D3DReflect)(
    LPCVOID pSrcData, SIZE_T SrcDataSize, REFIID pInterface,
    void **ppReflector);

/*************************************************************************/
/***************** Data structures used by Direct3D code *****************/
/*************************************************************************/

/* Texture color types. */
enum {
    TEXCOLOR_RGBA = 1,
    TEXCOLOR_RGB,
    TEXCOLOR_A,
    TEXCOLOR_L,
};

/* Texture data structure. */

struct D3DSysTexture {
    /* Direct3D device generation for this texture. */
    unsigned int generation;

    /* Direct3D texture, view, and sampler objects for this texture. */
    ID3D11Texture2D *d3d_tex;
    ID3D11ShaderResourceView *d3d_srv;
    ID3D11SamplerState *d3d_sampler;

    /* Size of this texture, in pixels. */
    int width, height;

    /* Color type of pixel data (TEXCOLOR_*). */
    uint8_t color_type;

    /* Is this texture a framebuffer texture? */
    uint8_t is_framebuffer;

    /* Should we automatically generate mipmaps for this texture? */
    uint8_t auto_mipmaps;

    /* Does this texture currently have mipmaps stored? */
    uint8_t has_mipmaps;

    /* Texture coordinate repeat and antialiasing flags. */
    uint8_t repeat_u, repeat_v;
    uint8_t antialias;

    /* Is the texture empty (newly created and not yet modified)? */
    uint8_t empty;

    /* Texture unit to which this texture is currently bound, or -1 if none. */
    int bound_unit;

    /* Pixel data buffer returned from the last sys_texture_lock(), or
     * NULL if not locked. */
    void *lock_buf;

    /* Lock mode used with the last sys_texture_lock().  Only valid when
     * lock_buf != NULL. */
    SysTextureLockMode lock_mode;
};

/*-----------------------------------------------------------------------*/

/* Framebuffer data structure. */

struct D3DSysFramebuffer {
    /* Direct3D device generation for this framebuffer. */
    unsigned int generation;

    /* Direct3D texture objects for color and depth buffers. */
    ID3D11Texture2D *color_buffer;
    ID3D11Texture2D *depth_buffer;  // NULL if no depth buffer present.

    /* View objects for binding the framebuffer to the output merger. */
    ID3D11RenderTargetView *color_view;
    ID3D11DepthStencilView *depth_view;

    /* Size of this framebuffer, in pixels. */
    int width, height;

    /* D3DSysTexture structure for applying the framebuffer as a texture. */
    D3DSysTexture texture;
};

/*-----------------------------------------------------------------------*/

/* Graphics primitive data structure. */

struct D3DSysPrimitive {
    /* Direct3D device generation for this primitive. */
    unsigned int generation;

    /* Primitive type. */
    D3D11_PRIMITIVE_TOPOLOGY type;

    /* Is this a high-level QUADS or QUAD_STRIP primitive which was
     * converted to GL_TRIANGLES or GL_TRIANGLE_STRIP? */
    uint8_t converted_quads;

    /* Was this primitive using the shared single-quad index buffer? */
    uint8_t is_single_quad;

    /* Is this primitive using immediate-mode vertex/index buffers? */
    uint8_t is_immediate_vbo;

    /* Flags indicating which of the standard attributes are included in
     * the vertex data. */
    uint8_t has_position;
    uint8_t has_texcoord;
    uint8_t has_color;

    /* Number of components in the position attribute, if present. */
    uint8_t position_count;

    /* Vertex buffer object. */
    ID3D11Buffer *vertex_buffer;

    /* Index buffer object, or NULL if this primitive has no associated
     * index buffer. */
    ID3D11Buffer *index_buffer;

    /* Input layout object. */
    ID3D11InputLayout *input_layout;

    /* Number of vertices (or indices, if index_buffer is non-NULL) in the
     * primitive. */
    int render_count;

    /* Size of a single vertex structure. */
    int vertex_size;

    /* Data type of index buffer.  Unused if index_buffer is NULL. */
    DXGI_FORMAT index_type;
};

/*-----------------------------------------------------------------------*/

/* Shader data structure. */

struct D3DSysShader {
    /* Direct3D device generation for this shader. */
    unsigned int generation;

    /* Shader type (SHADER_TYPE_*). */
    ShaderType type;

    /* Direct3D shader object. */
    union {
        ID3D11VertexShader *vs;
        ID3D11PixelShader *ps;
    };

    /* ShaderReflection interfaces for looking up uniform locations. */
    ID3D11ShaderReflection *reflect;
    ID3D11ShaderReflectionConstantBuffer *uniforms_reflect;

    /* Uniform buffer, or NULL if no uniforms are defined. */
    ID3D11Buffer *uniforms;
    int uniforms_index;  // Buffer binding point.

    /* Linked lists of shader pipelines with which this shader is
     * associated.  This field also serves as the head of the list. */
    D3DSysShaderPipeline *pipelines;

    /* Saved shader data (for shader_get_binary()), or NULL/0 if not saved. */
    void *binary_data;
    int binary_size;
};

/*-----------------------------------------------------------------------*/

/* Shader pipeline data structure. */

struct D3DSysShaderPipeline {
    /* Direct3D device generation for this shader pipeline. */
    unsigned int generation;

    /* Pointers to the vertex and fragment shaders attached to this
     * pipeline. */
    D3DSysShader *vertex_shader, *pixel_shader;

    /* Linked list pointers for associating vertex and pixel shaders
     * with shader pipelines.  prev_ptr points to the appropriate "next"
     * field in the previous pipeline in the list, or the "pipelines" field
     * of the shader object if this is the first entry in the list. */
    D3DSysShaderPipeline *vertex_next, **vertex_prev_ptr;
    D3DSysShaderPipeline *pixel_next, **pixel_prev_ptr;
};

/*-----------------------------------------------------------------------*/

/* Uniform block structures for vertex and pixel shaders. */

typedef struct D3DVertexUniformBlock {
    Matrix4f transform;
    Vector4f fixed_color;
    Vector4f fog_transform;
    Vector2f tex_offset;
} D3DVertexUniformBlock;

typedef struct D3DPixelUniformBlock {
    Vector4f fixed_color;
    Vector4f fog_color;
    Vector2f fog_params;
    float alpha_ref;
} D3DPixelUniformBlock;

/*************************************************************************/
/*********************** Internal utility routines ***********************/
/*************************************************************************/

/******** d3d-base.c ********/

/**
 * d3d_check_format_support:  Return whether the given pixel format is
 * supported for the given usage.
 *
 * [Parameters]
 *     format: Format to check (DXGI_FORMAT_*).
 *     usage: Usage to check for (D3D11_FORMAT_SUPPORT_*).
 * [Return value]
 *     True if the format is supported for the given usage, false if not.
 *     If multiple usage bits are set, this function only returns true if
 *     all such usages are supported.
 */
extern int d3d_check_format_support(DXGI_FORMAT format,
                                    D3D11_FORMAT_SUPPORT usage);

/**
 * d3d_format_bpp:  Return the number of bits per pixel for the given
 * pixel format, or 0 if unknown.
 */
extern CONST_FUNCTION int d3d_format_bpp(DXGI_FORMAT format);

/**
 * d3d_get_depth_stencil_format:  Return the smallest pixel format
 * corresponding to the given depth and stencil data sizes, or
 * DXGI_FORMAT_UNKNOWN if no such format is available.
 */
extern DXGI_FORMAT d3d_depth_stencil_format(int depth_bits, int stencil_bits);

/**
 * d3d_get_pixel_converter:  Return the pixel conversion function to use
 * to convert the given Direct3D pixel format to RGBA8888.  Returns NULL
 * if the format is already RGBA8888 or if no conversion function is
 * available.
 */
extern CONST_FUNCTION PixelConvertFunc *d3d_get_pixel_converter(
    DXGI_FORMAT format);

/**
 * d3d_set_render_target:  Set the render target for subsequent draw
 * operations.
 *
 * [Parameters]
 *     rtv: RenderTargetView for the color buffer, or NULL to select the
 *         default render target.
 *     dsv: DepthStencilView for the depth buffer, or NULL if none.
 *         Ignored if rtv is NULL.
 */
extern void d3d_set_render_target(ID3D11RenderTargetView *rtv,
                                  ID3D11DepthStencilView *dsv);

/**
 * d3d_get_render_target:  Return the current render target, or NULL if
 * none.  This function does _not_ add a reference to the returned object.
 */
extern ID3D11Texture2D *d3d_get_render_target(void);

/**
 * d3d_read_texture:  Read data from the given texture into the given
 * RGBA8888 pixel buffer.  Portions of the specified region which lie
 * outside the texture are undefined in the output buffer.
 *
 * [Parameters]
 *     texture: Texture to read from.
 *     flip_y: True to invert all Y coordinates.  This should be set for
 *         framebuffers and cleared for normal textures.
 *     r8_is_alpha: True if the R8 format should be interpreted as alpha
 *         instead of luminance.
 *     x, y, w, h: Region of texture to read.  x and y must be nonnegative.
 *     stride: Line stride of output buffer, in pixels.
 *     buffer: Output buffer.
 * [Return value]
 *     True on success, false on error.
 */
extern int d3d_read_texture(ID3D11Texture2D *texture, int flip_y,
                            int r8_is_alpha, int x, int y, int w, int h,
                            int stride, void *buffer);


/******** d3d-framebuffer.c ********/

/**
 * d3d_framebuffer_init:  Initialize framebuffer management data.
 */
extern void d3d_framebuffer_init(void);

/**
 * d3d_get_current_texture:  Return a pointer to the currently bound
 * framebuffer, or NULL if none.
 */
extern D3DSysFramebuffer *d3d_get_current_framebuffer(void);


/******** d3d-inputlayout.c ********/

/**
 * d3d_inputlayout_get:  Return an InputLayout object for the given
 * vertex format, or NULL on error.
 */
extern ID3D11InputLayout *d3d_inputlayout_get(
    const D3D11_INPUT_ELEMENT_DESC *input_elements, int num_inputs);

/**
 * d3d_inputlayout_free_all:  Free all cached InputLayout objects and
 * reinitialize cache state.
 */
extern void d3d_inputlayout_free_all(void);


/******** d3d-primitive.c ********/

/**
 * d3d_primitive_cleanup:  Clean up resources used in primitive rendering.
 */
extern void d3d_primitive_cleanup(void);


/******** d3d-shader.c ********/

/**
 * d3d_shader_objects_enabled:  True if shader objects are enabled, false
 * if not.
 */
extern uint8_t d3d_shader_objects_enabled;

/**
 * d3d_shader_init:  Initialize shader management data.
 */
extern void d3d_shader_init(void);

/**
 * d3d_shader_cleanup:  Clean up shader management data.
 */
extern void d3d_shader_cleanup(void);

/**
 * d3d_apply_default_shader:  Select and apply an appropriate default
 * shader for drawing the given primitive object with the given state.
 *
 * [Parameters]
 *     primitive: Primitive for which to apply a shader.
 *     tex_offset: True if a texture coordinate offset is applied.
 *     color_uniform: True if a fixed color multiplier is applied.
 *     fog: True if fog is applied.
 *     alpha_test: True if alpha testing is enabled.
 *     alpha_comparison: Comparison function for alpha testing.
 * [Return value]
 *     True if the current shader was changed, false if not (or on error).
 */
extern int d3d_apply_default_shader(
    const D3DSysPrimitive *primitive, int tex_offset, int color_uniform,
    int fog, int alpha_test, GraphicsComparisonType alpha_comparison);

/**
 * d3d_set_default_vs_uniforms:  Copy the given data into the uniform buffer
 * for the currently selected default vertex shader.
 */
extern void d3d_set_default_vs_uniforms(const D3DVertexUniformBlock *vs_data);

/**
 * d3d_set_default_ps_uniforms:  Copy the given data into the uniform buffer
 * for the currently selected default pixel shader.
 */
extern void d3d_set_default_ps_uniforms(const D3DPixelUniformBlock *ps_data);


/******** d3d-state.c ********/

/**
 * d3d_state_init:  Initialize rendering state.
 *
 * [Parameters]
 *     width, height: Size of the default render target, in pixels.
 */
extern void d3d_state_init(int width, int height);

/**
 * d3d_state_cleanup:  Clean up resources used by rendering state management.
 */
extern void d3d_state_cleanup(void);

/**
 * d3d_state_handle_resize:  Update rendering state in response to a window
 * resize event.
 *
 * [Parameters]
 *     width, height: New size of the default render target, in pixels.
 */
extern void d3d_state_handle_resize(int width, int height);

/**
 * d3d_state_apply:  Apply any pending state changes to the Direct3D device.
 */
extern void d3d_state_apply(void);

/**
 * d3d_state_set_shader:  Install an appropriate shader and update uniform
 * blocks for the given primitive and current rendering state.
 */
extern void d3d_state_set_shader(const D3DSysPrimitive *primitive);

/**
 * d3d_state_can_clear():  Return whether the Direct3D Clear* functions
 * can safely be used given the current rendering state.
 *
 * Direct3D 11 specifies that ClearRenderTargetView() and
 * ClearDepthStencilView() ignore scissor state, while SIL (following
 * OpenGL) requires that graphics_clear() honor the scissor state.  The
 * interaction of color masks with clearing is not documented, but tests
 * suggest that ClearRenderTargetView() ignores the color mask (which
 * makes a certain amount of sense given that Direct3D treats the color
 * mask as part of blend state rather than output state), while SIL
 * requires graphics_clear() to honor the color mask.  So if scissoring
 * or color masking is enabled, we have to clear the buffers manually by
 * drawing a render-target-sized quad.
 *
 * [Return value]
 *     True if ClearRenderTargetView() and ClearDepthStencilView() can be
 *     safely used; false if d3d_state_safe_clear() must be called instead.
 */
extern int d3d_state_can_clear(void);

/**
 * d3d_state_safe_clear:  Clear the current render target and depth/stencil
 * buffer by drawing a quad instead of calling the ID3D11DeviceContext
 * clearing functions.  For use when d3d_state_can_clear() returns false.
 *
 * [Parameters]
 *     color: Color to store in the render buffer, or NULL to not clear
 *         the color buffer.
 *     depth: Value to store in the depth buffer, or NULL to not clear the
 *         depth/stencil buffer.
 *     stencil: Value to store in the stencil buffer.  Ignored if depth is
 *         NULL.
 */
extern void d3d_state_safe_clear(const Vector4f *color, const float *depth,
                                 uint8_t stencil);


/******** d3d-texture.c ********/

/**
 * d3d_texture_init:  Initialize texture management data.
 */
extern void d3d_texture_init(void);

/**
 * d3d_get_current_texture:  Return a pointer to the texture currently
 * bound to texture unit 0, or NULL if no texture is bound to unit 0.
 */
extern D3DSysTexture *d3d_get_current_texture(void);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_WINDOWS_D3D_INTERNAL_H
