/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/windows/d3d-framebuffer.c: Framebuffer management functionality
 * for Direct3D.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/framebuffer.h"
#include "src/graphics.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/windows/d3d-internal.h"
#include "src/framebuffer.h"
#include "src/utility/pixformat.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Currently bound framebuffer, or NULL if the default render target is
 * bound. */
static D3DSysFramebuffer *current_framebuffer;

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int d3d_sys_framebuffer_supported(void)
{
    return 1;
}

/*-----------------------------------------------------------------------*/

D3DSysFramebuffer *d3d_sys_framebuffer_create(
    int width, int height, FramebufferColorType color_type,
    int depth_bits, int stencil_bits)
{
    HRESULT result;

    D3DSysFramebuffer *framebuffer = mem_alloc(sizeof(*framebuffer), 0, 0);
    if (UNLIKELY(!framebuffer)) {
        DLOG("Failed to allocate D3DSysFramebuffer");
        goto error_return;
    }

    DXGI_FORMAT tex_format = DXGI_FORMAT_UNKNOWN;
    int texcolor_type;
    switch (color_type) {
      case FBCOLOR_RGB8:
        tex_format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texcolor_type = TEXCOLOR_RGB;
        break;
      case FBCOLOR_RGBA8:
        tex_format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texcolor_type = TEXCOLOR_RGBA;
        break;
    }
    if (tex_format == DXGI_FORMAT_UNKNOWN) {
        DLOG("Invalid framebuffer color type: %d", tex_format);
        return NULL;
    }

    framebuffer->generation             = d3d_device_generation;
    framebuffer->width                  = width;
    framebuffer->height                 = height;
    framebuffer->texture.generation     = d3d_device_generation;
    framebuffer->texture.width          = width;
    framebuffer->texture.height         = height;
    framebuffer->texture.color_type     = texcolor_type;
    framebuffer->texture.is_framebuffer = 1;
    framebuffer->texture.auto_mipmaps   = 0;
    framebuffer->texture.has_mipmaps    = 0;
    framebuffer->texture.repeat_u       = 0;
    framebuffer->texture.repeat_v       = 0;
    framebuffer->texture.antialias      = 1;
    framebuffer->texture.empty          = 1;
    framebuffer->texture.bound_unit     = -1;
    framebuffer->texture.lock_buf       = NULL;

    D3D11_TEXTURE2D_DESC tex_desc = {
        .Width = width,
        .Height = height,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = tex_format,
        .SampleDesc = {.Count = 1, .Quality = 0},  // FIXME: multisampling?
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
        .CPUAccessFlags = 0,
        .MiscFlags = 0,
    };
    if ((result = ID3D11Device_CreateTexture2D(
             d3d_device, &tex_desc, NULL,
             &framebuffer->color_buffer)) != S_OK) {
        DLOG("Failed to create color buffer: %s", d3d_strerror(result));
        goto error_free_framebuffer;
    }
    if ((result != ID3D11Device_CreateRenderTargetView(
             d3d_device, (ID3D11Resource *)framebuffer->color_buffer, NULL,
             &framebuffer->color_view)) != S_OK) {
        DLOG("Failed to create RTV: %s", d3d_strerror(result));
        goto error_release_color_buffer;
    }
    if ((result != ID3D11Device_CreateShaderResourceView(
             d3d_device, (ID3D11Resource *)framebuffer->color_buffer, NULL,
             &framebuffer->texture.d3d_srv)) != S_OK) {
        DLOG("Failed to create SRV: %s", d3d_strerror(result));
        goto error_release_color_view;
    }

    if (depth_bits == 0 && stencil_bits == 0) {
        framebuffer->depth_buffer = NULL;
        framebuffer->depth_view = NULL;
    } else {
        const DXGI_FORMAT depth_format =
            d3d_depth_stencil_format(depth_bits, stencil_bits);
        if (depth_format == DXGI_FORMAT_UNKNOWN) {
            DLOG("Depth/stencil size %d/%d not supported",
                 depth_bits, stencil_bits);
            goto error_release_srv;
        }
        tex_desc.Format = depth_format;
        tex_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        if ((result = ID3D11Device_CreateTexture2D(
                 d3d_device, &tex_desc, NULL,
                 &framebuffer->depth_buffer)) != S_OK) {
            DLOG("Failed to create depth buffer: %s", d3d_strerror(result));
            goto error_release_srv;
        }
        if ((result = ID3D11Device_CreateDepthStencilView(
                 d3d_device, (ID3D11Resource *)framebuffer->depth_buffer, NULL,
                 &framebuffer->depth_view)) != S_OK) {
            DLOG("Failed to create DSV: %s", d3d_strerror(result));
            goto error_release_depth_buffer;
        }
    }

    /* Force a change in sampler state so the texture's sampler object
     * gets created. */
    framebuffer->texture.antialias = 0;
    framebuffer->texture.d3d_sampler = NULL;
    d3d_sys_texture_set_antialias(&framebuffer->texture, 1);

    return framebuffer;

  error_release_depth_buffer:
    if (framebuffer->depth_buffer) {
        ID3D11Texture2D_Release(framebuffer->depth_buffer);
    }
  error_release_srv:
    ID3D11ShaderResourceView_Release(framebuffer->texture.d3d_srv);
  error_release_color_view:
    ID3D11RenderTargetView_Release(framebuffer->color_view);
  error_release_color_buffer:
    ID3D11Texture2D_Release(framebuffer->color_buffer);
  error_free_framebuffer:
    mem_free(framebuffer);
  error_return:
    return NULL;
}

/*-----------------------------------------------------------------------*/

void d3d_sys_framebuffer_destroy(D3DSysFramebuffer *framebuffer)
{
    if (framebuffer->generation == d3d_device_generation) {
        if (current_framebuffer == framebuffer) {
            d3d_sys_framebuffer_bind(NULL);
        }
        if (framebuffer->texture.bound_unit >= 0) {
            d3d_sys_texture_apply(framebuffer->texture.bound_unit, NULL);
        }
        if (framebuffer->depth_view) {
            ID3D11DepthStencilView_Release(framebuffer->depth_view);
        }
        if (framebuffer->depth_buffer) {
            ID3D11Texture2D_Release(framebuffer->depth_buffer);
        }
        ID3D11SamplerState_Release(framebuffer->texture.d3d_sampler);
        ID3D11ShaderResourceView_Release(framebuffer->texture.d3d_srv);
        ID3D11RenderTargetView_Release(framebuffer->color_view);
        ID3D11Texture2D_Release(framebuffer->color_buffer);
    }

    mem_free(framebuffer);
}

/*-----------------------------------------------------------------------*/

void d3d_sys_framebuffer_bind(D3DSysFramebuffer *framebuffer)
{
    if (framebuffer) {
        if (UNLIKELY(framebuffer->generation != d3d_device_generation)) {
            DLOG("Attempt to use invalidated framebuffer %p", framebuffer);
            return;
        }
        d3d_set_render_target(framebuffer->color_view, framebuffer->depth_view);
    } else {
        d3d_set_render_target(NULL, NULL);
    }
    current_framebuffer = framebuffer;
}

/*-----------------------------------------------------------------------*/

D3DSysTexture *d3d_sys_framebuffer_get_texture(D3DSysFramebuffer *framebuffer)
{
    return &framebuffer->texture;
}

/*-----------------------------------------------------------------------*/

void d3d_sys_framebuffer_set_antialias(D3DSysFramebuffer *framebuffer, int on)
{
    if (UNLIKELY(framebuffer->generation != d3d_device_generation)) {
        DLOG("Attempt to use invalidated framebuffer %p", framebuffer);
        return;
    }

    d3d_sys_texture_set_antialias(&framebuffer->texture, on);
}

/*-----------------------------------------------------------------------*/

void d3d_sys_framebuffer_discard_data(D3DSysFramebuffer *framebuffer)
{
    if (UNLIKELY(framebuffer->generation != d3d_device_generation)) {
        DLOG("Attempt to use invalidated framebuffer %p", framebuffer);
        return;
    }

    /* Direct3D doesn't seem to support this operation. */
}

/*************************************************************************/
/******** Internal interface routines (private to Direct3D code) *********/
/*************************************************************************/

void d3d_framebuffer_init(void)
{
    current_framebuffer = NULL;
}

/*-----------------------------------------------------------------------*/

D3DSysFramebuffer *d3d_get_current_framebuffer(void)
{
    return current_framebuffer;
}

/*************************************************************************/
/*************************************************************************/
