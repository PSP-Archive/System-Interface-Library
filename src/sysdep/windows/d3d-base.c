/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/windows/d3d-base.c: Basic Direct3D rendering functionality.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/graphics.h"
#include "src/math.h"
#include "src/sysdep.h"
#include "src/sysdep/windows/d3d.h"
#include "src/sysdep/windows/d3d-internal.h"
#include "src/sysdep/windows/internal.h"
#include "src/utility/pixformat.h"

#include <d3dx9.h>  // For D3DXERR_* error codes.

/*************************************************************************/
/**************** Shared data (private to Direct3D code) *****************/
/*************************************************************************/

ID3D11DeviceContext *d3d_context;
ID3D11Device *d3d_device;
unsigned int d3d_device_generation;
D3D_FEATURE_LEVEL d3d_feature_level;
const char *d3dcompiler_name;

HRESULT (WINAPI *p_D3DCompile)(
    LPCVOID pSrcData, SIZE_T SrcDataSize, LPCSTR pSourceName,
    const D3D_SHADER_MACRO *pDefines, ID3DInclude *pInclude,
    LPCSTR pEntrypoint, LPCSTR pTarget, UINT Flags1, UINT Flags2,
    ID3DBlob **ppCode, ID3DBlob **ppErrorMsgs);
HRESULT (WINAPI *p_D3DReflect)(
    LPCVOID pSrcData, SIZE_T SrcDataSize, REFIID pInterface,
    void **ppReflector);

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Module handles for d3d11.dll and d3dcompiler_4[367].dll. */
static HMODULE d3d11_handle;
static HMODULE d3dcompiler_handle;

/* Pointer to D3D11CreateDeviceAndSwapChain() obtained from d3d11.dll. */
static PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN p_D3D11CreateDeviceAndSwapChain;

/* Swap chain for the current context. */
static IDXGISwapChain *d3d_swapchain;

/* Depth buffer, format, and view for the current context, 0/NULL if none. */
static ID3D11Texture2D *d3d_depthbuffer;
static DXGI_FORMAT d3d_depthformat;
static ID3D11DepthStencilView *d3d_depthview;

/* Default render target and view for the current context. */
static ID3D11Texture2D *d3d_default_rendertarget;
static ID3D11RenderTargetView *d3d_default_rtview;

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * create_depth_buffer:  Create a depth buffer matching the size of the
 * current window output buffer with the format given by d3d_depthformat.
 *
 * [Return value]
 *     True on success, false on error.
 */
static int create_depth_buffer(void);

/*************************************************************************/
/****** Internal interface routines (exposed to other Windows code) ******/
/*************************************************************************/

int d3d_open_library(void)
{
    d3d11_handle = LoadLibrary("d3d11.dll");
    if (d3d11_handle) {
        p_D3D11CreateDeviceAndSwapChain = (void *)GetProcAddress(
            d3d11_handle, "D3D11CreateDeviceAndSwapChain");
        if (p_D3D11CreateDeviceAndSwapChain) {
            DLOG("d3d11.dll successfully loaded");
            /* But we also need to manually load the shader compiler... */
            static const char * const compiler_dlls[] = {
                "d3dcompiler_47.dll", "d3dcompiler_46.dll",
                "d3dcompiler_43.dll"};
            for (int i = 0; i < lenof(compiler_dlls); i++) {
                d3dcompiler_name = compiler_dlls[i];
                d3dcompiler_handle = LoadLibrary(d3dcompiler_name);
                if (d3dcompiler_handle) {
                    p_D3DCompile = (void *)GetProcAddress(
                        d3dcompiler_handle, "D3DCompile");
                    p_D3DReflect = (void *)GetProcAddress(
                        d3dcompiler_handle, "D3DReflect");
                    if (p_D3DCompile && p_D3DReflect) {
                        DLOG("%s successfully loaded", d3dcompiler_name);
                        d3d_context = NULL;
                        d3d_depthbuffer = NULL;
                        d3d_depthview = NULL;
                        d3d_device = NULL;
                        d3d_feature_level = 0;
                        d3d_swapchain = NULL;
                        return 1;
                    } else {
                        DLOG("Found invalid %s (missing D3DCompile/D3DReflect)",
                             d3dcompiler_name);
                        FreeLibrary(d3dcompiler_handle);
                        d3dcompiler_handle = NULL;
                    }
                } else {
                    DLOG("Failed to load %s", d3dcompiler_name);
                }
            }
            DLOG("Failed to load any shader compiler, Direct3D not available");
            d3dcompiler_name = NULL;
        } else {
            DLOG("Found invalid d3d11.dll (missing D3D11CreateDeviceAndSwapChain())");
            FreeLibrary(d3d11_handle);
            d3d11_handle = NULL;
        }
    } else {
        DLOG("Failed to load d3d11.dll");
    }
    return 0;
}

/*-----------------------------------------------------------------------*/

void d3d_close_library(void)
{
    ASSERT(!d3d_context);
    if (d3dcompiler_handle) {
        FreeLibrary(d3dcompiler_handle);
        d3dcompiler_handle = NULL;
    }
    if (d3d11_handle) {
        FreeLibrary(d3d11_handle);
        d3d11_handle = NULL;
    }
}

/*-----------------------------------------------------------------------*/

int d3d_create_context(HWND window, int width, int height, int depth_bits,
                       int stencil_bits, int samples)
{
    DXGI_SWAP_CHAIN_DESC swapchain_desc;
    mem_clear(&swapchain_desc, sizeof(swapchain_desc));
    swapchain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapchain_desc.SampleDesc.Count = samples;
    swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    // FIXME: 1 or 2 buffers for fullscreen? or do we have to recreate the
    // swap chain each time we switch between fullscreen and windowed?
    swapchain_desc.BufferCount = 1;
    swapchain_desc.OutputWindow = window;
    swapchain_desc.Windowed = TRUE;
    swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    swapchain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    UINT flags = 0;
    HRESULT result = (*p_D3D11CreateDeviceAndSwapChain)(
        NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags, NULL, 0,
        D3D11_SDK_VERSION, &swapchain_desc, &d3d_swapchain,
        &d3d_device, &d3d_feature_level, &d3d_context);
    if (result != S_OK) {
        DLOG("Failed to create Direct3D context: %s", d3d_strerror(result));
        goto error_return;
    }

    if ((result = IDXGISwapChain_GetBuffer(
             d3d_swapchain, 0, &IID_ID3D11Texture2D,
             (void **)&d3d_default_rendertarget)) != S_OK) {
        DLOG("Failed to get back buffer reference: %s", d3d_strerror(result));
        goto error_close_context;
    }
    result = ID3D11Device_CreateRenderTargetView(
        d3d_device, (ID3D11Resource *)d3d_default_rendertarget, NULL,
        &d3d_default_rtview);
    if (result != S_OK) {
        DLOG("Failed to create render target view: %s",
             d3d_strerror(result));
        goto error_release_rendertarget;
    }

    if (depth_bits == 0 && stencil_bits == 0) {
        d3d_depthbuffer = NULL;
        d3d_depthview = NULL;
    } else {
        d3d_depthformat = d3d_depth_stencil_format(depth_bits, stencil_bits);
        if (d3d_depthformat == DXGI_FORMAT_UNKNOWN) {
            DLOG("Unsupported depth/stencil size combination: %d/%d",
                 depth_bits, stencil_bits);
            goto error_release_rtview;
        }
        if (!create_depth_buffer()) {
            goto error_release_rtview;
        }
    }

    d3d_device_generation++;
    d3d_framebuffer_init();
    d3d_shader_init();
    d3d_state_init(width, height);
    d3d_texture_init();
    return 1;

  error_release_rtview:
    ID3D11RenderTargetView_Release(d3d_default_rtview);
    d3d_default_rtview = NULL;
  error_release_rendertarget:
    ID3D11Texture2D_Release(d3d_default_rendertarget);
    d3d_default_rendertarget = NULL;
  error_close_context:
    IDXGISwapChain_Release(d3d_swapchain);
    d3d_swapchain = NULL;
    ID3D11DeviceContext_Release(d3d_context);
    d3d_context = NULL;
    ID3D11Device_Release(d3d_device);
    d3d_device = NULL;
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

void d3d_destroy_context(void)
{
    if (!d3d_context) {
        return;
    }

    d3d_inputlayout_free_all();
    d3d_primitive_cleanup();
    d3d_shader_cleanup();
    d3d_state_cleanup();

    ID3D11DeviceContext_ClearState(d3d_context);
    if (d3d_depthview) {
        ID3D11DepthStencilView_Release(d3d_depthview);
        d3d_depthview = NULL;
    }
    if (d3d_depthbuffer) {
        ID3D11Texture2D_Release(d3d_depthbuffer);
        d3d_depthbuffer = NULL;
    }
    if (d3d_default_rtview) {
        ID3D11RenderTargetView_Release(d3d_default_rtview);
        d3d_default_rtview = NULL;
    }
    if (d3d_default_rendertarget) {
        ID3D11Texture2D_Release(d3d_default_rendertarget);
        d3d_default_rendertarget = NULL;
    }
    IDXGISwapChain_Release(d3d_swapchain);
    d3d_swapchain = NULL;
    ID3D11DeviceContext_Release(d3d_context);
    d3d_context = NULL;
    ID3D11Device_Release(d3d_device);
    d3d_device = NULL;
}

/*-----------------------------------------------------------------------*/

void d3d_resize_window(void)
{
    HRESULT result;

    if (d3d_default_rtview) {
        ASSERT(0 == ID3D11RenderTargetView_Release(d3d_default_rtview));
        d3d_default_rtview = NULL;
    }
    if (d3d_default_rendertarget) {
        ASSERT(0 == ID3D11Texture2D_Release(d3d_default_rendertarget));
        d3d_default_rendertarget = NULL;
    }
    if ((result = IDXGISwapChain_ResizeBuffers(
             d3d_swapchain, 0, 0, 0, 0, 0)) != S_OK) {
        DLOG("IDXGISwapChain::ResizeBuffers() failed: %s",
             d3d_strerror(result));
    }
    if ((result = IDXGISwapChain_GetBuffer(
             d3d_swapchain, 0, &IID_ID3D11Texture2D,
             (void **)&d3d_default_rendertarget)) != S_OK) {
        DLOG("Failed to get back buffer reference: %s", d3d_strerror(result));
    } else {
        result = ID3D11Device_CreateRenderTargetView(
            d3d_device, (ID3D11Resource *)d3d_default_rendertarget, NULL,
            &d3d_default_rtview);
        if (result != S_OK) {
            DLOG("Failed to create render target view: %s",
                 d3d_strerror(result));
        }
        D3D11_TEXTURE2D_DESC output_desc;
        ID3D11Texture2D_GetDesc(d3d_default_rendertarget, &output_desc);
        d3d_state_handle_resize(output_desc.Width, output_desc.Height);
    }

    if (d3d_depthformat) {
        if (d3d_depthview) {
            ID3D11DepthStencilView_Release(d3d_depthview);
            d3d_depthview = NULL;
        }
        if (d3d_depthbuffer) {
            ID3D11Texture2D_Release(d3d_depthbuffer);
            d3d_depthbuffer = NULL;
        }
        if (!create_depth_buffer()) {
            DLOG("Failed to resize depth buffer");
        }
    }
}

/*-----------------------------------------------------------------------*/

void d3d_start_frame(void)
{
    ID3D11DeviceContext_OMSetRenderTargets(
        d3d_context, 1, &d3d_default_rtview, d3d_depthview);
}

/*-----------------------------------------------------------------------*/

void d3d_finish_frame(void)
{
    ID3D11DeviceContext_OMSetRenderTargets(d3d_context, 0, NULL, NULL);
    IDXGISwapChain_Present(d3d_swapchain, windows_vsync_interval(), 0);
}

/*-----------------------------------------------------------------------*/

void d3d_sync(void)
{
    ID3D11DeviceContext_Flush(d3d_context);
    ID3D11Query *query;
    if (ID3D11Device_CreateQuery(
            d3d_device, &((D3D11_QUERY_DESC){D3D11_QUERY_EVENT, 0}),
            &query) != S_OK) {
        DLOG("Failed to create query event");
    } else {
        ID3D11Asynchronous *async = NULL;
        ASSERT(ID3D11Query_QueryInterface(query, &IID_ID3D11Asynchronous,
                                          (void **)&async) == S_OK);
        ASSERT(async);
        ID3D11DeviceContext_End(d3d_context, async);
        HRESULT result;
        while ((result = ID3D11DeviceContext_GetData(
                    d3d_context, async, NULL, 0, 0)) != S_OK) {
            if (result != S_FALSE) {
                DLOG("Error waiting for D3D11_QUERY_EVENT: %s",
                     d3d_strerror(result));
                break;
            }
            sys_thread_yield();
        }
        ID3D11Asynchronous_Release(async);
        ID3D11Query_Release(query);
    }
}

/*-----------------------------------------------------------------------*/

const char *d3d_strerror(HRESULT result)
{
    switch (result) {
      case S_OK:
        return "Success";
      case S_FALSE:
        return "Data not available";
      case E_FAIL:
        return "Operation failed";
      case E_INVALIDARG:
        return "Invalid argument";
      case E_NOINTERFACE:
        return "Interface not supported";
      case E_OUTOFMEMORY:
        return "Out of memory";
      case E_NOTIMPL:
        return "Not implemented";
      case D3D11_ERROR_FILE_NOT_FOUND:
        return "File not found";
      case D3D11_ERROR_TOO_MANY_UNIQUE_STATE_OBJECTS:
        return "Too many unique state objects";
      case D3D11_ERROR_TOO_MANY_UNIQUE_VIEW_OBJECTS:
        return "Too many unique view objects";
      case D3DERR_INVALIDCALL:
      case DXGI_ERROR_INVALID_CALL:
        return "Invalid method call";
      case D3DERR_WASSTILLDRAWING:
      case DXGI_ERROR_WAS_STILL_DRAWING:
        return "Draw operation still in progress";
      case D3DXERR_INVALIDDATA:
        return "Invalid data";
    }

    static char buf[10];
    strformat(buf, sizeof(buf), "%08X", result);
    return buf;
}

/*************************************************************************/
/******** Internal interface routines (private to Direct3D code) *********/
/*************************************************************************/

int d3d_check_format_support(DXGI_FORMAT format, D3D11_FORMAT_SUPPORT usage)
{
    UINT reported_usage = 0;
    return ID3D11Device_CheckFormatSupport(d3d_device, format,
                                           &reported_usage) == S_OK
        && (reported_usage & usage) == usage;
}

/*-----------------------------------------------------------------------*/

int d3d_format_bpp(DXGI_FORMAT format)
{
    switch (format) {
      case DXGI_FORMAT_R32G32B32A32_TYPELESS:
      case DXGI_FORMAT_R32G32B32A32_FLOAT:
      case DXGI_FORMAT_R32G32B32A32_UINT:
      case DXGI_FORMAT_R32G32B32A32_SINT:
        return 128;
      case DXGI_FORMAT_R32G32B32_TYPELESS:
      case DXGI_FORMAT_R32G32B32_FLOAT:
      case DXGI_FORMAT_R32G32B32_UINT:
      case DXGI_FORMAT_R32G32B32_SINT:
        return 96;
      case DXGI_FORMAT_R16G16B16A16_TYPELESS:
      case DXGI_FORMAT_R16G16B16A16_FLOAT:
      case DXGI_FORMAT_R16G16B16A16_UNORM:
      case DXGI_FORMAT_R16G16B16A16_UINT:
      case DXGI_FORMAT_R16G16B16A16_SNORM:
      case DXGI_FORMAT_R16G16B16A16_SINT:
      case DXGI_FORMAT_R32G32_TYPELESS:
      case DXGI_FORMAT_R32G32_FLOAT:
      case DXGI_FORMAT_R32G32_UINT:
      case DXGI_FORMAT_R32G32_SINT:
      case DXGI_FORMAT_R32G8X24_TYPELESS:
      case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
      case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
      case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        return 64;
      case DXGI_FORMAT_R10G10B10A2_TYPELESS:
      case DXGI_FORMAT_R10G10B10A2_UNORM:
      case DXGI_FORMAT_R10G10B10A2_UINT:
      case DXGI_FORMAT_R11G11B10_FLOAT:
      case DXGI_FORMAT_R8G8B8A8_TYPELESS:
      case DXGI_FORMAT_R8G8B8A8_UNORM:
      case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
      case DXGI_FORMAT_R8G8B8A8_UINT:
      case DXGI_FORMAT_R8G8B8A8_SNORM:
      case DXGI_FORMAT_R8G8B8A8_SINT:
      case DXGI_FORMAT_R16G16_TYPELESS:
      case DXGI_FORMAT_R16G16_FLOAT:
      case DXGI_FORMAT_R16G16_UNORM:
      case DXGI_FORMAT_R16G16_UINT:
      case DXGI_FORMAT_R16G16_SNORM:
      case DXGI_FORMAT_R16G16_SINT:
      case DXGI_FORMAT_R32_TYPELESS:
      case DXGI_FORMAT_D32_FLOAT:
      case DXGI_FORMAT_R32_FLOAT:
      case DXGI_FORMAT_R32_UINT:
      case DXGI_FORMAT_R32_SINT:
      case DXGI_FORMAT_R24G8_TYPELESS:
      case DXGI_FORMAT_D24_UNORM_S8_UINT:
      case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
      case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
      case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
      case DXGI_FORMAT_B8G8R8A8_UNORM:
      case DXGI_FORMAT_B8G8R8X8_UNORM:
      case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
      case DXGI_FORMAT_B8G8R8A8_TYPELESS:
      case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
      case DXGI_FORMAT_B8G8R8X8_TYPELESS:
      case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
        return 32;
      case DXGI_FORMAT_R8G8_TYPELESS:
      case DXGI_FORMAT_R8G8_UNORM:
      case DXGI_FORMAT_R8G8_UINT:
      case DXGI_FORMAT_R8G8_SNORM:
      case DXGI_FORMAT_R8G8_SINT:
      case DXGI_FORMAT_R16_TYPELESS:
      case DXGI_FORMAT_R16_FLOAT:
      case DXGI_FORMAT_D16_UNORM:
      case DXGI_FORMAT_R16_UNORM:
      case DXGI_FORMAT_R16_UINT:
      case DXGI_FORMAT_R16_SNORM:
      case DXGI_FORMAT_R16_SINT:
      case DXGI_FORMAT_R8G8_B8G8_UNORM:
      case DXGI_FORMAT_G8R8_G8B8_UNORM:
      case DXGI_FORMAT_B5G6R5_UNORM:
      case DXGI_FORMAT_B5G5R5A1_UNORM:
      case DXGI_FORMAT_B4G4R4A4_UNORM:
        return 16;
      case DXGI_FORMAT_R8_TYPELESS:
      case DXGI_FORMAT_R8_UNORM:
      case DXGI_FORMAT_R8_UINT:
      case DXGI_FORMAT_R8_SNORM:
      case DXGI_FORMAT_R8_SINT:
      case DXGI_FORMAT_A8_UNORM:
      case DXGI_FORMAT_BC2_TYPELESS:
      case DXGI_FORMAT_BC2_UNORM:
      case DXGI_FORMAT_BC2_UNORM_SRGB:
      case DXGI_FORMAT_BC3_TYPELESS:
      case DXGI_FORMAT_BC3_UNORM:
      case DXGI_FORMAT_BC3_UNORM_SRGB:
        return 8;
      case DXGI_FORMAT_BC1_TYPELESS:
      case DXGI_FORMAT_BC1_UNORM:
      case DXGI_FORMAT_BC1_UNORM_SRGB:
        return 4;
      case DXGI_FORMAT_R1_UNORM:
        return 1;
      default:
        return 0;
    }
}

/*-----------------------------------------------------------------------*/

DXGI_FORMAT d3d_depth_stencil_format(int depth_bits, int stencil_bits)
{
    if (depth_bits <= 16 && stencil_bits == 0) {
        return DXGI_FORMAT_D16_UNORM;
    } else if (depth_bits <= 24 && stencil_bits <= 8) {
        return DXGI_FORMAT_D24_UNORM_S8_UINT;
    } else if (depth_bits <= 32 && stencil_bits == 0) {
        return DXGI_FORMAT_D32_FLOAT;
    } else if (depth_bits <= 32 && stencil_bits <= 8) {
        return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
    } else {
        return DXGI_FORMAT_UNKNOWN;
    }
}

/*-----------------------------------------------------------------------*/

PixelConvertFunc *d3d_get_pixel_converter(DXGI_FORMAT format)
{
    switch (format) {
      case DXGI_FORMAT_B8G8R8A8_UNORM:
      case DXGI_FORMAT_B8G8R8X8_UNORM:
        return pixel_convert_bgra8888_rgba8888;
      case DXGI_FORMAT_B5G6R5_UNORM:
        return pixel_convert_bgr565_rgba8888;
      case DXGI_FORMAT_B5G5R5A1_UNORM:
        return pixel_convert_bgra5551_rgba8888;
      case DXGI_FORMAT_B4G4R4A4_UNORM:
        return pixel_convert_bgra4444_rgba8888;
      case DXGI_FORMAT_R8_UNORM:
        return pixel_convert_l8_rgba8888;
      default:
        return NULL;
    }
}

/*-----------------------------------------------------------------------*/

void d3d_set_render_target(ID3D11RenderTargetView *rtv,
                           ID3D11DepthStencilView *dsv)
{
    if (rtv) {
        ID3D11DeviceContext_OMSetRenderTargets(d3d_context, 1, &rtv, dsv);
    } else {
        ID3D11DeviceContext_OMSetRenderTargets(
            d3d_context, 1, &d3d_default_rtview, d3d_depthview);
    }
}

/*-----------------------------------------------------------------------*/

ID3D11Texture2D *d3d_get_render_target(void)
{
    ID3D11RenderTargetView *rtv = NULL;
    ID3D11DeviceContext_OMGetRenderTargets(d3d_context, 1, &rtv, NULL);
    if (!rtv) {
        return NULL;
    }
    ID3D11Texture2D *render_target = NULL;
    ID3D11RenderTargetView_GetResource(rtv, (ID3D11Resource **)&render_target);
    ID3D11RenderTargetView_Release(rtv);
    ASSERT(render_target != NULL, return NULL);
    ID3D11Texture2D_Release(render_target);
    return render_target;
}

/*-----------------------------------------------------------------------*/

int d3d_read_texture(ID3D11Texture2D *texture, int flip_y, int r8_is_alpha,
                     int x, int y, int w, int h, int stride, void *buffer)
{
    PRECOND(texture != NULL, return 0);
    PRECOND(x >= 0, return 0);
    PRECOND(y >= 0, return 0);
    PRECOND(buffer != NULL, return 0);

    if (w <= 0 || h <= 0) {
        return 1;  // Nothing to read!
    }

    HRESULT result;

    ID3D11Texture2D *staging_texture = NULL;
    PixelConvertFunc *convert_func;
    D3D11_TEXTURE2D_DESC staging_desc;
    ID3D11Texture2D_GetDesc(texture, &staging_desc);
    if (staging_desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM) {
        convert_func = NULL;
    } else if (staging_desc.Format == DXGI_FORMAT_R8_UNORM && r8_is_alpha) {
        convert_func = pixel_convert_a8_rgba8888;
    } else {
        convert_func = d3d_get_pixel_converter(staging_desc.Format);
        if (!convert_func) {
            DLOG("Unable to read from non-R8G8B8A8 surfaces (format: %d)",
                 staging_desc.Format);
            return 0;
        }
    }

    const int texture_w = staging_desc.Width;
    const int texture_h = staging_desc.Height;
    if (x >= texture_w || y >= texture_h) {
        return 1;
    }
    w = ubound(w, texture_w - x);
    h = ubound(h, texture_h - y);

    staging_desc.Width = w;
    staging_desc.Height = h;
    staging_desc.Usage = D3D11_USAGE_STAGING;
    staging_desc.BindFlags = 0;
    staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    staging_desc.MiscFlags = 0;
    if ((result = ID3D11Device_CreateTexture2D(
             d3d_device, &staging_desc, NULL, &staging_texture)) != S_OK) {
        DLOG("Failed to create staging texture: %s", d3d_strerror(result));
        return 0;
    }

    if (w == texture_w && h == texture_h) {
        ASSERT(x == 0 && y == 0);
        ID3D11DeviceContext_CopyResource(
            d3d_context, (ID3D11Resource *)staging_texture,
            (ID3D11Resource *)texture);
    } else {
        /* Direct3D render targets are flipped vertically (with Y=0 at the
         * top of the buffer), so adjust the copy coordinates accordingly. */
        if (flip_y) {
            y = texture_h - (y + h);
        }
        ID3D11DeviceContext_CopySubresourceRegion(
            d3d_context, (ID3D11Resource *)staging_texture, 0, 0, 0, 0,
            (ID3D11Resource *)texture, 0,
            &((D3D11_BOX){.left = x, .top = y, .front = 0,
                          .right = x+w, .bottom = y+h, .back = 1}));
    }

    D3D11_MAPPED_SUBRESOURCE resource;
    if ((result = ID3D11DeviceContext_Map(
             d3d_context, (ID3D11Resource *)staging_texture, 0,
             D3D11_MAP_READ, 0, &resource)) != S_OK) {
        DLOG("Failed to map staging texture: %s", d3d_strerror(result));
        ID3D11Texture2D_Release(staging_texture);
        return 0;
    }

    const uint8_t *src = resource.pData;
    int src_pitch = resource.RowPitch;
    if (flip_y) {
        /* Read input rows in reverse to undo vertical flipping. */
        src += (h-1) * src_pitch;
        src_pitch = -src_pitch;
    }
    const int staging_bpp = d3d_format_bpp(staging_desc.Format);
    uint8_t *dest = buffer;
    if (stride == w && src_pitch == w * staging_bpp / 8) {
        if (convert_func) {
            (*convert_func)(dest, src, h * w);
        } else {
            memcpy(dest, src, h * w * staging_bpp / 8);
        }
    } else {
        for (int yy = 0; yy < h; yy++, src += src_pitch, dest += stride * 4) {
            if (convert_func) {
                (*convert_func)(dest, src, w);
            } else {
                memcpy(dest, src, w * staging_bpp / 8);
            }
        }
    }

    ID3D11DeviceContext_Unmap(
        d3d_context, (ID3D11Resource *)staging_texture, 0);
    ID3D11Texture2D_Release(staging_texture);
    return 1;
}

/*************************************************************************/
/*********************** sysdep interface routines ***********************/
/*************************************************************************/

// FIXME: Workaround for old <d3dcommon.h> in Ubuntu 18.04 mingw-w64
// headers package which doesn't know about D3D_FEATURE_LEVEL_12_*.
#define D3D_FEATURE_LEVEL_12_0  0xC000
#define D3D_FEATURE_LEVEL_12_1  0xC100
#ifdef __GNUC__
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wswitch"
#endif

const char *d3d_sys_graphics_renderer_info(void)
{
    const char *level_str = "(unknown)";
    switch (d3d_feature_level) {
      case D3D_FEATURE_LEVEL_9_1:  level_str = "9_1";  break;
      case D3D_FEATURE_LEVEL_9_2:  level_str = "9_2";  break;
      case D3D_FEATURE_LEVEL_9_3:  level_str = "9_3";  break;
      case D3D_FEATURE_LEVEL_10_0: level_str = "10_0"; break;
      case D3D_FEATURE_LEVEL_10_1: level_str = "10_1"; break;
      case D3D_FEATURE_LEVEL_11_0: level_str = "11_0"; break;
      case D3D_FEATURE_LEVEL_11_1: level_str = "11_1"; break;
      case D3D_FEATURE_LEVEL_12_0: level_str = "12_0"; break;
      case D3D_FEATURE_LEVEL_12_1: level_str = "12_1"; break;
    }
    static char buf[40];
    strformat(buf, sizeof(buf), "Direct3D 11.0, feature level %s", level_str);
    return buf;
}

#ifdef __GNUC__  // FIXME: hack for Ubuntu-installed headers, see above
# pragma GCC diagnostic pop
#endif

/*-----------------------------------------------------------------------*/

void d3d_sys_graphics_clear(const Vector4f *color, const float *depth,
                            unsigned int stencil)
{
    if (d3d_state_can_clear()) {
        ID3D11RenderTargetView *rtv = NULL;
        ID3D11DepthStencilView *dsv = NULL;
        ID3D11DeviceContext_OMGetRenderTargets(d3d_context, 1, &rtv, &dsv);
        if (color && rtv) {
            ID3D11DeviceContext_ClearRenderTargetView(
                d3d_context, rtv, &color->x);
        }
        if (depth && dsv) {
            ID3D11DeviceContext_ClearDepthStencilView(
                d3d_context, dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
                *depth, stencil);
        }
        if (rtv) {
            ID3D11RenderTargetView_Release(rtv);
        }
        if (dsv) {
            ID3D11DepthStencilView_Release(dsv);
        }
    } else {
        d3d_state_safe_clear(color, depth, stencil);
    }
}

/*-----------------------------------------------------------------------*/

int d3d_sys_graphics_read_pixels(int x, int y, int w, int h, int stride,
                                 void *buffer)
{
    if (!d3d_read_texture(d3d_get_render_target(), 1, 0, x, y, w, h, stride,
                          buffer)) {
        return 0;
    }

    D3DSysFramebuffer *framebuffer = d3d_get_current_framebuffer();
    if (!framebuffer || framebuffer->texture.color_type == TEXCOLOR_RGB) {
        uint8_t *dest = buffer;
        for (int yy = 0; yy < h; yy++, dest += stride * 4) {
            for (int xx = 0; xx < w; xx++) {
                dest[xx*4+3] = 255;
            }
        }
    }

    return 1;
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static int create_depth_buffer(void)
{
    PRECOND(d3d_depthformat != DXGI_FORMAT_UNKNOWN);
    PRECOND(!d3d_depthbuffer);
    PRECOND(!d3d_depthview);

    HRESULT result;

    ID3D11Texture2D *back_buffer;
    if ((result = IDXGISwapChain_GetBuffer(
             d3d_swapchain, 0, &IID_ID3D11Texture2D,
             (void **)&back_buffer)) != S_OK) {
        DLOG("Failed to get back buffer reference: %s", d3d_strerror(result));
        return 0;
    }
    D3D11_TEXTURE2D_DESC back_desc;
    ID3D11Texture2D_GetDesc(back_buffer, &back_desc);
    ID3D11Texture2D_Release(back_buffer);

    D3D11_TEXTURE2D_DESC depth_desc;
    mem_clear(&depth_desc, sizeof(depth_desc));
    depth_desc.Width = back_desc.Width;
    depth_desc.Height = back_desc.Height;
    depth_desc.MipLevels = 1;
    depth_desc.ArraySize = 1;
    depth_desc.Format = d3d_depthformat;
    depth_desc.SampleDesc.Count = 1;
    depth_desc.Usage = D3D11_USAGE_DEFAULT;
    depth_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    if ((result = ID3D11Device_CreateTexture2D(
             d3d_device, &depth_desc, NULL, &d3d_depthbuffer)) != S_OK) {
        DLOG("Failed to create depth/stencil buffer: %s", d3d_strerror(result));
        return 0;
    }

    if ((result = ID3D11Device_CreateDepthStencilView(
             d3d_device, (ID3D11Resource *)d3d_depthbuffer, NULL,
             &d3d_depthview)) != S_OK) {
        DLOG("Failed to create depth/stencil view: %s", d3d_strerror(result));
        ID3D11Texture2D_Release(d3d_depthbuffer);
        d3d_depthbuffer = NULL;
        return 0;
    }

    return 1;
}

/*************************************************************************/
/*************************************************************************/
