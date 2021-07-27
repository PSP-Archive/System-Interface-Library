/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/windows/d3d-texture.c: Texture manipulation functionality
 * for Direct3D.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/graphics.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/windows/d3d-internal.h"
#include "src/texture.h"
#include "src/utility/pixformat.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Texture currently bound to each texture unit. */
static D3DSysTexture *current_texture[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * can_auto_mipmap:  Return whether the system supports automatic mipmap
 * generation for a texture of the given size and pixel format.
 */
static PURE_FUNCTION int can_auto_mipmap(int width, int height,
                                         DXGI_FORMAT format);

/**
 * create_d3d_texture:  Create Direct3D resources for the given texture.
 *
 * [Parameters]
 *     texture: Texture for which to create resources.
 *     format: Direct3D texture format identifier (DXGI_FORMAT_*).
 *     num_levels: Number of mipmap levels (including the base image).
 *     data_list: Initial data for the texture, or NULL for none.
 *     d3d_tex_ret: Pointer to variable to receive the created
 *         ID3D11Texture2D instance.
 *     d3d_srv_ret: Pointer to variable to receive the created
 *         ID3D11ShaderResourceView instance.
 * [Return value]
 *     True on success, false on error.
 */
static int create_d3d_texture(
    D3DSysTexture *texture, DXGI_FORMAT format,
    int num_levels, D3D11_SUBRESOURCE_DATA *data_list,
    ID3D11Texture2D **d3d_tex_ret, ID3D11ShaderResourceView **d3d_srv_ret);

/**
 * update_sampler:  Update the ID3D11SamplerState object for the given
 * texture based on the texture's current state.  On failure, the
 * texture's sampler state will be reset to default (the null object).
 *
 * [Parameters]
 *     texture: Texture to update.
 * [Return value]
 *     True on success, false on error.
 */
static int update_sampler(D3DSysTexture *texture);

/*************************************************************************/
/*************** Interface: Texture creation and deletion ****************/
/*************************************************************************/

D3DSysTexture *d3d_sys_texture_create(
    int width, int height, TextureFormat data_format, int num_levels,
    void *data, int stride, const int32_t *level_offsets,
    const int32_t *level_sizes, int mipmaps, int mem_flags, int reuse)
{
    /* Determine the format parameters for the texture. */

    DXGI_FORMAT d3d_format;
    int color_type, bpp, input_bpp = 0;
    int stride_unit = 1;  // For block-compression formats.
    int is_palette = 0;
    PixelConvertFunc *convert_func = NULL;

    /* This is used to detect an invalid format without needing a "default"
     * case (which would break the compiler's missing-case check). */
    bpp = 0;

color_type=-1;
    switch (data_format) {

      case TEX_FORMAT_RGBA8888:
        d3d_format = DXGI_FORMAT_R8G8B8A8_UNORM;
        color_type = TEXCOLOR_RGBA;
        bpp = 32;
        break;

      case TEX_FORMAT_RGB565:
        /* 16-bit types were mandatorily supported in DirectX 9 and are
         * once again supported in DirectX 11.1, but they were optional
         * in DirectX 10.x and 11.0, so we have to explicitly check for
         * support.  Sigh. */
        d3d_format = DXGI_FORMAT_B5G6R5_UNORM;
        if (d3d_check_format_support(d3d_format,
                                     D3D11_FORMAT_SUPPORT_TEXTURE2D)) {
            convert_func = pixel_convert_rgb565_bgr565;
            color_type = TEXCOLOR_RGB;
            bpp = 16;
        } else {
            d3d_format = DXGI_FORMAT_R8G8B8A8_UNORM;
            convert_func = pixel_convert_rgb565_rgba8888;
            color_type = TEXCOLOR_RGBA;
            bpp = 32;
            input_bpp = 16;
        }
        break;

      case TEX_FORMAT_RGBA5551:
        d3d_format = DXGI_FORMAT_B5G5R5A1_UNORM;
        color_type = TEXCOLOR_RGBA;
        if (d3d_check_format_support(d3d_format,
                                     D3D11_FORMAT_SUPPORT_TEXTURE2D)) {
            convert_func = pixel_convert_rgba5551_bgra5551;
            bpp = 16;
        } else {
            d3d_format = DXGI_FORMAT_R8G8B8A8_UNORM;
            convert_func = pixel_convert_rgba5551_rgba8888;
            bpp = 32;
            input_bpp = 16;
        }
        break;

      case TEX_FORMAT_RGBA4444:
        d3d_format = DXGI_FORMAT_B4G4R4A4_UNORM;
        color_type = TEXCOLOR_RGBA;
        if (d3d_check_format_support(d3d_format,
                                     D3D11_FORMAT_SUPPORT_TEXTURE2D)) {
            convert_func = pixel_convert_rgba4444_bgra4444;
            bpp = 16;
        } else {
            d3d_format = DXGI_FORMAT_R8G8B8A8_UNORM;
            convert_func = pixel_convert_rgba4444_rgba8888;
            bpp = 32;
            input_bpp = 16;
        }
        break;

      case TEX_FORMAT_BGRA8888:
        d3d_format = DXGI_FORMAT_B8G8R8A8_UNORM;
        color_type = TEXCOLOR_RGBA;
        bpp = 32;
        break;

      case TEX_FORMAT_BGR565:
        d3d_format = DXGI_FORMAT_B5G6R5_UNORM;
        if (d3d_check_format_support(d3d_format,
                                     D3D11_FORMAT_SUPPORT_TEXTURE2D)) {
            color_type = TEXCOLOR_RGB;
            bpp = 16;
        } else {
            d3d_format = DXGI_FORMAT_R8G8B8A8_UNORM;
            convert_func = pixel_convert_bgr565_rgba8888;
            color_type = TEXCOLOR_RGBA;
            bpp = 32;
            input_bpp = 16;
        }
        break;

      case TEX_FORMAT_BGRA5551:
        d3d_format = DXGI_FORMAT_B5G5R5A1_UNORM;
        color_type = TEXCOLOR_RGBA;
        if (d3d_check_format_support(d3d_format,
                                     D3D11_FORMAT_SUPPORT_TEXTURE2D)) {
            bpp = 16;
        } else {
            d3d_format = DXGI_FORMAT_R8G8B8A8_UNORM;
            convert_func = pixel_convert_bgra5551_rgba8888;
            bpp = 32;
            input_bpp = 16;
        }
        break;

      case TEX_FORMAT_BGRA4444:
        d3d_format = DXGI_FORMAT_B4G4R4A4_UNORM;
        color_type = TEXCOLOR_RGBA;
        if (d3d_check_format_support(d3d_format,
                                     D3D11_FORMAT_SUPPORT_TEXTURE2D)) {
            bpp = 16;
        } else {
            d3d_format = DXGI_FORMAT_R8G8B8A8_UNORM;
            convert_func = pixel_convert_bgra4444_rgba8888;
            bpp = 32;
            input_bpp = 16;
        }
        break;

      case TEX_FORMAT_A8:
      case TEX_FORMAT_L8:
        d3d_format = DXGI_FORMAT_R8_UNORM;
        bpp = 8;
        color_type = (data_format == TEX_FORMAT_L8) ? TEXCOLOR_L : TEXCOLOR_A;
        break;

      case TEX_FORMAT_PSP_RGBA8888:
      case TEX_FORMAT_PSP_RGB565:
      case TEX_FORMAT_PSP_RGBA5551:
      case TEX_FORMAT_PSP_RGBA4444:
      case TEX_FORMAT_PSP_A8:
      case TEX_FORMAT_PSP_L8:
      case TEX_FORMAT_PSP_PALETTE8_RGBA8888:
      case TEX_FORMAT_PSP_RGBA8888_SWIZZLED:
      case TEX_FORMAT_PSP_RGB565_SWIZZLED:
      case TEX_FORMAT_PSP_RGBA5551_SWIZZLED:
      case TEX_FORMAT_PSP_RGBA4444_SWIZZLED:
      case TEX_FORMAT_PSP_A8_SWIZZLED:
      case TEX_FORMAT_PSP_L8_SWIZZLED:
      case TEX_FORMAT_PSP_PALETTE8_RGBA8888_SWIZZLED:
        DLOG("Pixel format %u unsupported", data_format);
        goto error_return;

      case TEX_FORMAT_PALETTE8_RGBA8888:
        d3d_format = DXGI_FORMAT_R8G8B8A8_UNORM;
        color_type = TEXCOLOR_RGBA;
        bpp = 32;
        input_bpp = 8;
        is_palette = 1;
        break;

      case TEX_FORMAT_S3TC_DXT1:
        d3d_format = DXGI_FORMAT_BC1_UNORM;
        color_type = TEXCOLOR_RGB;
        bpp = 4;
        stride = width;
        stride_unit = 4;
        break;

      case TEX_FORMAT_S3TC_DXT3:
        d3d_format = DXGI_FORMAT_BC2_UNORM;
        color_type = TEXCOLOR_RGBA;
        bpp = 8;
        stride = width;
        stride_unit = 4;
        break;

      case TEX_FORMAT_S3TC_DXT5:
        d3d_format = DXGI_FORMAT_BC3_UNORM;
        color_type = TEXCOLOR_RGBA;
        bpp = 8;
        stride = width;
        stride_unit = 4;
        break;

      case TEX_FORMAT_PVRTC2_RGBA:
      case TEX_FORMAT_PVRTC2_RGB:
      case TEX_FORMAT_PVRTC4_RGBA:
      case TEX_FORMAT_PVRTC4_RGB:
        DLOG("Pixel format %u unsupported", data_format);
        goto error_return;

    }  // switch (data_format)

    if (UNLIKELY(bpp == 0)) {
        DLOG("Pixel format %u unknown", data_format);
        goto error_return;
    }

    if (input_bpp == 0) {
        input_bpp = bpp;
    }

    /* Allocate and set up the SysTexture structure. */

    D3DSysTexture *texture = debug_mem_alloc(
        sizeof(*texture), 0, mem_flags | MEM_ALLOC_CLEAR,
        __FILE__, __LINE__, MEM_INFO_TEXTURE);
    if (UNLIKELY(!texture)) {
        DLOG("Failed to allocate SysTexture");
        goto error_return;
    }

    texture->generation = d3d_device_generation;
    texture->width          = width;
    texture->height         = height;
    texture->color_type     = color_type;
    texture->is_framebuffer = 0;
    texture->repeat_u       = 1;
    texture->repeat_v       = 1;
    texture->antialias      = 1;
    texture->empty          = (num_levels == 0);
    texture->bound_unit     = -1;

    if (mipmaps) {
        texture->auto_mipmaps = can_auto_mipmap(width, height, d3d_format);
    } else {
        texture->auto_mipmaps = 0;
    }
    if (texture->auto_mipmaps && num_levels > 1) {
        num_levels = 1; // Ignore provided mipmap data since we'll generate it.
    }
    texture->has_mipmaps = texture->auto_mipmaps || (num_levels > 1);

    /* Prepare texture data for loading, if it was supplied. */

    int total_levels;
    if (texture->has_mipmaps) {
        if (texture->auto_mipmaps) {
            total_levels = 1;
            int w = width, h = height;
            while (w > 1 || h > 1) {
                w = lbound(w/2, 1);
                h = lbound(h/2, 1);
                total_levels++;
            }
        } else {
            total_levels = num_levels;
        }
    } else {
        total_levels = 1;
    }

    void *temp_data = NULL;
    if (num_levels > 0
     && (is_palette || convert_func)
     && (bpp > input_bpp || stride < width || !reuse)) {
        int64_t total_size = 0;
        for (int level = 0; level < total_levels; level++) {
            const int level_w = lbound(width >> level, 1);
            const int level_h = lbound(height >> level, 1);
            total_size += (align_up(level_w, stride_unit)
                           * align_up(level_h, stride_unit) * bpp / 8);
        }
        if (!(temp_data = mem_alloc(total_size, 0, MEM_ALLOC_TEMP))) {
            DLOG("Out of memory for texture load buffer (%lld bytes)",
                 (long long)total_size);
            goto error_free_texture;
        }
    }

    D3D11_SUBRESOURCE_DATA *data_list = NULL;
    if (num_levels > 0) {
        data_list = mem_alloc(sizeof(*data_list) * total_levels, 0,
                              MEM_ALLOC_TEMP);
        if (UNLIKELY(!data_list)) {
            DLOG("Out of memory while creating texture");
            goto error_free_temp_data;
        }

        uint8_t *temp_data_ptr = temp_data;
        uint32_t palette[256];

        for (int level = 0; level < num_levels; level++) {
            const int level_w = lbound(width >> level, 1);
            const int level_h = lbound(height >> level, 1);
            const int level_s = lbound(stride >> level, 1);

            uint8_t *level_data = (uint8_t *)data + level_offsets[level];
            int32_t level_size = level_sizes[level];
            if (is_palette && level == 0) {
                memcpy(palette, level_data, sizeof(palette));
                level_data += sizeof(palette);
                level_size -= sizeof(palette);
            }

            if (is_palette) {
                const uint8_t *src = level_data;
                uint32_t *dest = (uint32_t *)temp_data_ptr;
                data_list[level].pSysMem = dest;
                data_list[level].SysMemPitch =
                    align_up(level_w, stride_unit) * bpp / 8;
                for (int y = 0; y < level_h;
                     y++, src += level_s, dest += level_w)
                {
                    for (int x = 0; x < level_w; x++) {
                        dest[x] = palette[src[x]];
                    }
                }

            } else if (convert_func) {
                const uint8_t *src = level_data;
                uint8_t *dest;
                if (temp_data) {
                    dest = temp_data_ptr;
                } else {
                    dest = level_data;
                }
                data_list[level].pSysMem = dest;
                data_list[level].SysMemPitch =
                    align_up(level_w, stride_unit) * bpp / 8;
                ASSERT(stride_unit == 1);
                if (level_s == level_w) {
                    (*convert_func)(dest, src, level_w * level_h);
                } else {
                    for (int y = 0; y < level_h; y++) {
                        (*convert_func)(dest, src, level_w);
                        src += level_s * input_bpp / 8;
                        dest += data_list[level].SysMemPitch;
                    }
                }

            } else {
                data_list[level].pSysMem = level_data;
                data_list[level].SysMemPitch =
                    align_up(level_s, stride_unit) * bpp / 8;
            }

            data_list[level].SysMemSlicePitch =
                align_up(level_h, stride_unit) * data_list[level].SysMemPitch;
            temp_data_ptr += data_list[level].SysMemSlicePitch;
        }  // for (int level = 0; level < num_levels; level++) {

        for (int level = num_levels; level < total_levels; level++) {
            /* Just set bogus data; we'll generate the mipmaps below. */
            data_list[level] = data_list[0];
        }
    }  // if (num_levels > 0)

    /* Create the actual Direct3D objects for the texture. */

    if (!create_d3d_texture(texture, d3d_format, total_levels, data_list,
                            &texture->d3d_tex, &texture->d3d_srv)) {
        goto error_free_data_list;
    }

    texture->d3d_sampler = NULL;
    if (!update_sampler(texture)) {
        goto error_destroy_d3d_srv;
    }

    /* Success!  Return the new texture. */

    mem_free(data_list);
    mem_free(temp_data);
    if (reuse) {
        mem_free(data);
    }
    return texture;

    /* Error handling. */

  error_destroy_d3d_srv:
    ID3D11ShaderResourceView_Release(texture->d3d_srv);
    ID3D11Texture2D_Release(texture->d3d_tex);
  error_free_data_list:
    mem_free(data_list);
  error_free_temp_data:
    mem_free(temp_data);
  error_free_texture:
    mem_free(texture);
  error_return:
    if (reuse) {
        mem_free(data);
    }
    return NULL;
}

/*-----------------------------------------------------------------------*/

void d3d_sys_texture_destroy(D3DSysTexture *texture)
{
    if (texture->generation == d3d_device_generation) {
        if (texture->bound_unit >= 0) {
            d3d_sys_texture_apply(texture->bound_unit, NULL);
        }
        if (texture->d3d_sampler) {
            ID3D11SamplerState_Release(texture->d3d_sampler);
        }
        ID3D11ShaderResourceView_Release(texture->d3d_srv);
        ID3D11Texture2D_Release(texture->d3d_tex);
    }

    mem_free(texture->lock_buf);
    mem_free(texture);
}

/*************************************************************************/
/*************** Interface: Texture information retrieval ****************/
/*************************************************************************/

int d3d_sys_texture_width(D3DSysTexture *texture)
{
    return texture->width;
}

/*-----------------------------------------------------------------------*/

int d3d_sys_texture_height(D3DSysTexture *texture)
{
    return texture->height;
}

/*-----------------------------------------------------------------------*/

int d3d_sys_texture_has_mipmaps(D3DSysTexture *texture)
{
    return texture->has_mipmaps;
}

/*************************************************************************/
/****************** Interface: Pixel data manipulation *******************/
/*************************************************************************/

D3DSysTexture *d3d_sys_texture_grab(
    int x, int y, int w, int h, int readable, int mipmaps, int mem_flags)
{
    D3DSysTexture *texture = mem_alloc(sizeof(*texture), 0, mem_flags);
    if (UNLIKELY(!texture)) {
        DLOG("Failed to allocate D3DSysTexture");
        goto error_return;
    }
    mem_debug_set_info(texture, MEM_INFO_TEXTURE);
    texture->generation     = d3d_device_generation;
    texture->width          = w;
    texture->height         = h;
    texture->color_type     = TEXCOLOR_RGB;
    texture->is_framebuffer = 0;
    texture->auto_mipmaps   = 0;
    texture->has_mipmaps    = 0;
    texture->repeat_u       = 1;
    texture->repeat_v       = 1;
    texture->antialias      = 1;
    texture->empty          = 1;
    texture->lock_buf       = NULL;

    ID3D11Texture2D *rendertarget = d3d_get_render_target();
    if (!rendertarget) {
        DLOG("No render target bound");
        goto error_free_texture;
    }
    D3D11_TEXTURE2D_DESC rt_desc;
    ID3D11Texture2D_GetDesc(rendertarget, &rt_desc);
    /* At the moment we only render to RGBA8888 surfaces, so this check is
     * meaningless, but keep it around just in case we add more formats
     * later. */
    if (readable
     && rt_desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM
     && !d3d_get_pixel_converter(rt_desc.Format)) {
        DLOG("Render target is not in a readable format (format: %d)",
             rt_desc.Format);
        goto error_free_texture;
    }

    texture->auto_mipmaps = mipmaps && can_auto_mipmap(w, h, rt_desc.Format);
    texture->has_mipmaps = texture->auto_mipmaps;

    int num_levels = 1;
    if (texture->auto_mipmaps) {
        int w2 = texture->width, h2 = texture->height;
        while (w2 > 1 || h2 > 1) {
            w2 = lbound(w2/2, 1);
            h2 = lbound(h2/2, 1);
            num_levels++;
        }
    }
    D3D11_TEXTURE2D_DESC texture_desc = {
        .Width = w,
        .Height = h,
        .MipLevels = num_levels,
        .ArraySize = 1,
        .Format = rt_desc.Format,
        .SampleDesc = {.Count = 1, .Quality = 0},
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_SHADER_RESOURCE
                   | (mipmaps ? D3D11_BIND_RENDER_TARGET : 0),
        .CPUAccessFlags = 0,
        .MiscFlags = (mipmaps ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0),
    };

    int destx = 0, desty = 0;
    if (x < 0) {
        destx = -x;
        w += x;
        x = 0;
    }
    if (y < 0) {
        desty = -y;
        h += y;
        y = 0;
    }
    w = ubound(w, (int)rt_desc.Width - x);
    h = ubound(h, (int)rt_desc.Height - y);

    ID3D11Texture2D *new_tex;
    HRESULT result;

    if (w > 0 && h > 0) {
        int bufsize = texture_desc.Width * texture_desc.Height * 4;
        uint8_t *data = mem_alloc(bufsize, 0, MEM_ALLOC_TEMP | MEM_ALLOC_CLEAR);
        if (UNLIKELY(!data)) {
            DLOG("No memory for pixel read buffer (%d bytes)", bufsize);
            goto error_free_texture;
        }

        if (!d3d_read_texture(rendertarget, 1, 0, x, y, w, h,
                              texture_desc.Width,
                              data + (desty*texture_desc.Width + destx) * 4)) {
            DLOG("Failed to read data for texture");
            mem_free(data);
            goto error_free_texture;
        }

        D3D11_SUBRESOURCE_DATA *data_list = mem_alloc(
            sizeof(*data_list) * num_levels, 0, MEM_ALLOC_TEMP);
        if (UNLIKELY(!data_list)) {
            DLOG("Failed to allocate data list array (%d levels)", num_levels);
            mem_free(data);
            goto error_free_texture;
        }
        for (int i = 0; i < num_levels; i++) {
            data_list[i].pSysMem = data;
            data_list[i].SysMemPitch = texture_desc.Width * 4;
            data_list[i].SysMemSlicePitch =
                texture_desc.Height * data_list[i].SysMemPitch;
        }

        texture_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        if (!mipmaps) {
            texture_desc.Usage = D3D11_USAGE_IMMUTABLE;
        }
        result = ID3D11Device_CreateTexture2D(
            d3d_device, &texture_desc, data_list, &new_tex);
        mem_free(data_list);
        mem_free(data);
        if (UNLIKELY(result != S_OK)) {
            DLOG("Failed to create texture: %s", d3d_strerror(result));
            goto error_free_texture;
        }

    } else {  // Source region is empty.
        if ((result = ID3D11Device_CreateTexture2D(
                 d3d_device, &texture_desc, NULL, &new_tex)) != S_OK) {
            DLOG("Failed to create texture: %s", d3d_strerror(result));
            goto error_free_texture;
        }
    }

    ID3D11ShaderResourceView *new_srv;
    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {
        .Format = texture_desc.Format,
        .ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
        .Texture2D = {
            .MostDetailedMip = 0,
            .MipLevels = -1,
        },
    };
    if ((result != ID3D11Device_CreateShaderResourceView(
             d3d_device, (ID3D11Resource *)new_tex, &srv_desc,
             &new_srv)) != S_OK) {
        DLOG("Failed to create SRV: %s", d3d_strerror(result));
        ID3D11Texture2D_Release(new_tex);
        goto error_free_texture;
    }

    if (texture->auto_mipmaps) {
        ID3D11DeviceContext_GenerateMips(d3d_context, new_srv);
    }

    texture->d3d_tex = new_tex;
    texture->d3d_srv = new_srv;
    texture->d3d_sampler = NULL;
    update_sampler(texture);
    texture->color_type = TEXCOLOR_RGB;
    texture->empty = 0;
    return texture;

  error_free_texture:
    mem_free(texture);
  error_return:
    return NULL;
}

/*-----------------------------------------------------------------------*/

void *d3d_sys_texture_lock(
    D3DSysTexture *texture, SysTextureLockMode lock_mode,
    int x, int y, int w, int h)
{
    if (UNLIKELY(texture->generation != d3d_device_generation)) {
        DLOG("Attempt to use invalidated texture %p", texture);
        return NULL;
    }

    const int64_t size = w * h * 4;
    uint8_t *lock_buf = mem_alloc(size, SIL_OPENGL_TEXTURE_BUFFER_ALIGNMENT,
                                  MEM_ALLOC_TEMP);
    if (UNLIKELY(!lock_buf)) {
        DLOG("lock(%p): Failed to get a lock buffer (%u bytes)",
             texture, size);
        return NULL;
    }

    if (lock_mode == SYS_TEXTURE_LOCK_DISCARD) {
        /* Nothing else to do. */
    } else if (texture->empty) {
        mem_clear(lock_buf, size);
    } else {
        if (!d3d_read_texture(texture->d3d_tex, texture->is_framebuffer,
                              texture->color_type == TEXCOLOR_A,
                              x, y, w, h, w, lock_buf)) {
            DLOG("lock(%p): Failed to read texture data", texture);
            mem_free(lock_buf);
            return NULL;
        }
        if (texture->color_type == TEXCOLOR_RGB) {
            for (int i = 0; i < size; i += 4) {
                lock_buf[i+3] = 255;
            }
        }
    }

    texture->lock_buf  = lock_buf;
    texture->lock_mode = lock_mode;
    return texture->lock_buf;
}

/*-----------------------------------------------------------------------*/

void d3d_sys_texture_unlock(D3DSysTexture *texture, int update)
{
    if (UNLIKELY(texture->generation != d3d_device_generation)) {
        DLOG("Attempt to use invalidated texture %p", texture);
        return;
    }

    if (update) {
        /* For simplicity (and possibly speed), we just create a fresh
         * D3D texture with the new data and discard the old one. */

        D3D11_SUBRESOURCE_DATA data_list[16];
        int num_levels = 1;
        if (texture->auto_mipmaps) {
            int w = texture->width, h = texture->height;
            while (w > 1 || h > 1) {
                w = lbound(w/2, 1);
                h = lbound(h/2, 1);
                num_levels++;
            }
        }
        ASSERT(num_levels <= lenof(data_list));
        data_list[0].pSysMem = texture->lock_buf;
        data_list[0].SysMemPitch = texture->width * 4;
        data_list[0].SysMemSlicePitch =
            texture->height * data_list[0].SysMemPitch;
        for (int i = 1; i < num_levels; i++) {
            data_list[i] = data_list[0];
        }

        ID3D11Texture2D *new_tex;
        ID3D11ShaderResourceView *new_srv;
        if (!create_d3d_texture(texture, DXGI_FORMAT_R8G8B8A8_UNORM,
                                num_levels, data_list, &new_tex, &new_srv)) {
            DLOG("Failed to create replacement texture, discarding update");
        } else {
            ID3D11ShaderResourceView_Release(texture->d3d_srv);
            ID3D11Texture2D_Release(texture->d3d_tex);
            texture->d3d_tex = new_tex;
            texture->d3d_srv = new_srv;
        }

        texture->empty = 0;
        texture->has_mipmaps = texture->auto_mipmaps;
    }  // if (update)

    mem_free(texture->lock_buf);
    texture->lock_buf = NULL;
}

/*-----------------------------------------------------------------------*/

void d3d_sys_texture_flush(D3DSysTexture *texture)
{
    if (UNLIKELY(texture->generation != d3d_device_generation)) {
        DLOG("Attempt to use invalidated texture %p", texture);
        return;
    }

    /* Nothing to do for Direct3D. */
}

/*************************************************************************/
/********************* Interface: Rendering control **********************/
/*************************************************************************/

void d3d_sys_texture_set_repeat(D3DSysTexture *texture,
                                int repeat_u, int repeat_v)
{
    if (UNLIKELY(texture->generation != d3d_device_generation)) {
        DLOG("Attempt to use invalidated texture %p", texture);
        return;
    }

    int need_update = 0;
    if ((repeat_u != 0) != texture->repeat_u) {
        texture->repeat_u = (repeat_u != 0);
        need_update = 1;
    }
    if ((repeat_v != 0) != texture->repeat_v) {
        texture->repeat_v = (repeat_v != 0);
        need_update = 1;
    }
    if (need_update || !texture->d3d_sampler) {
        update_sampler(texture);
    }
}

/*-----------------------------------------------------------------------*/

void d3d_sys_texture_set_antialias(D3DSysTexture *texture, int on)
{
    if (UNLIKELY(texture->generation != d3d_device_generation)) {
        DLOG("Attempt to use invalidated texture %p", texture);
        return;
    }

    if (on && (!texture->antialias || !texture->d3d_sampler)) {
        texture->antialias = 1;
        update_sampler(texture);
    } else if (!on && (texture->antialias || !texture->d3d_sampler)) {
        texture->antialias = 0;
        update_sampler(texture);
    }
}

/*-----------------------------------------------------------------------*/

void d3d_sys_texture_apply(int unit, D3DSysTexture *texture)
{
    if (texture && UNLIKELY(texture->generation != d3d_device_generation)) {
        DLOG("Attempt to use invalidated texture %p", texture);
        return;
    }

    if (unit >= D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT) {
        DLOG("Attempt to bind to invalid unit %d (out of range)", unit);
        return;
    }

    if (current_texture[unit]) {
        current_texture[unit]->bound_unit = -1;
    }
    current_texture[unit] = texture;

    if (texture) {
        texture->bound_unit = unit;
        ID3D11DeviceContext_PSSetShaderResources(
            d3d_context, unit, 1, &texture->d3d_srv);
        ID3D11DeviceContext_PSSetSamplers(
            d3d_context, unit, 1, &texture->d3d_sampler);
    } else {
        ID3D11DeviceContext_PSSetShaderResources(
            d3d_context, unit, 1, ((ID3D11ShaderResourceView *[]){NULL}));
        ID3D11DeviceContext_PSSetSamplers(
            d3d_context, unit, 1, ((ID3D11SamplerState *[]){NULL}));
    }
}

/*-----------------------------------------------------------------------*/

int d3d_sys_texture_num_units(void)
{
    return D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;
}

/*************************************************************************/
/******** Internal interface routines (private to Direct3D code) *********/
/*************************************************************************/

void d3d_texture_init(void)
{
    mem_clear(current_texture, sizeof(current_texture));
}

/*-----------------------------------------------------------------------*/

D3DSysTexture *d3d_get_current_texture(void)
{
    return current_texture[0];
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static int can_auto_mipmap(int width, int height, DXGI_FORMAT format)
{
    if (d3d_feature_level < D3D_FEATURE_LEVEL_10_0) {
        /* Direct3D 9.x only supports mipmaps for power-of-two-sized
         * textures. */
        if ((width  & (width -1)) != 0
         || (height & (height-1)) != 0) {
            return 0;
        }
    }

    if (format == DXGI_FORMAT_R8G8B8A8_UNORM
     || format == DXGI_FORMAT_B8G8R8A8_UNORM
     || format == DXGI_FORMAT_B5G6R5_UNORM) {
        /* Supported with all feature levels >= 9_1. */
        return 1;
    } else if (format == DXGI_FORMAT_B4G4R4A4_UNORM) {
        return d3d_feature_level >= D3D_FEATURE_LEVEL_9_3;
    } else if (format == DXGI_FORMAT_B5G5R5A1_UNORM
            || format == DXGI_FORMAT_R8_UNORM) {
        return d3d_feature_level >= D3D_FEATURE_LEVEL_10_0;
    } else {
        return 0;
    }
}

/*-----------------------------------------------------------------------*/

static int create_d3d_texture(
    D3DSysTexture *texture, DXGI_FORMAT format,
    int num_levels, D3D11_SUBRESOURCE_DATA *data_list,
    ID3D11Texture2D **d3d_tex_ret, ID3D11ShaderResourceView **d3d_srv_ret)
{
    PRECOND(texture != NULL, return 0);
    PRECOND(d3d_tex_ret != NULL, return 0);
    PRECOND(d3d_srv_ret != NULL, return 0);

    HRESULT result;

    D3D11_TEXTURE2D_DESC tex_desc = {
        .Width = texture->width,
        .Height = texture->height,
        .MipLevels = num_levels,
        .ArraySize = 1,
        .Format = format,
        .SampleDesc = {.Count = 1, .Quality = 0},
        /* We never modify texture buffers once created, so as long as
         * the texture doesn't have auto-mipmaps (and initial data was
         * provided), we can declare it immutable. */
        .Usage = (texture->auto_mipmaps || !data_list
                  ? D3D11_USAGE_DEFAULT : D3D11_USAGE_IMMUTABLE),
        .BindFlags = D3D11_BIND_SHADER_RESOURCE
                   | (texture->auto_mipmaps ? D3D11_BIND_RENDER_TARGET : 0),
        .CPUAccessFlags = 0,
        .MiscFlags = (texture->auto_mipmaps
                      ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0),
    };
    if ((result = ID3D11Device_CreateTexture2D(
             d3d_device, &tex_desc, data_list, d3d_tex_ret)) != S_OK) {
        DLOG("Failed to create texture: %s", d3d_strerror(result));
        goto error_return;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {
        .Format = format,
        .ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
        .Texture2D = {
            .MostDetailedMip = 0,
            .MipLevels = -1,
        },
    };
    if ((result != ID3D11Device_CreateShaderResourceView(
             d3d_device, (ID3D11Resource *)*d3d_tex_ret, &srv_desc,
             d3d_srv_ret)) != S_OK) {
        DLOG("Failed to create SRV: %s", d3d_strerror(result));
        goto error_destroy_d3d_tex;
    }

    if (texture->auto_mipmaps) {
        ID3D11DeviceContext_GenerateMips(d3d_context, texture->d3d_srv);
    }

    return 1;

  error_destroy_d3d_tex:
    ID3D11Texture2D_Release(*d3d_tex_ret);
    *d3d_tex_ret = NULL;
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

static int update_sampler(D3DSysTexture *texture)
{
    D3D11_SAMPLER_DESC desc;
    mem_clear(&desc, sizeof(desc));
    if (texture->antialias) {
        desc.Filter = texture->has_mipmaps
            ? D3D11_FILTER_MIN_MAG_MIP_LINEAR
            : D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    } else {
        desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    }
    desc.AddressU = texture->repeat_u
        ? D3D11_TEXTURE_ADDRESS_WRAP
        : D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.AddressV = texture->repeat_v
        ? D3D11_TEXTURE_ADDRESS_WRAP
        : D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    desc.MaxLOD = D3D11_FLOAT32_MAX;

    /* The Direct3D API specified that creating a new sampler state object
     * with the same state as an existing object simply returns a new
     * reference to the existing object, so we don't have to worry about
     * a proliferation of device-side sampler states here.  We create the
     * new state before releasing the old one so the old one can be reused
     * in this manner if appropriate. */
    ID3D11SamplerState *new_sampler;
    HRESULT result = ID3D11Device_CreateSamplerState(
        d3d_device, &desc, &new_sampler);
    if (texture->d3d_sampler) {
        ID3D11SamplerState_Release(texture->d3d_sampler);
    }
    if (result == S_OK) {
        texture->d3d_sampler = new_sampler;
        if (texture->bound_unit >= 0) {
            ID3D11DeviceContext_PSSetSamplers(
                d3d_context, texture->bound_unit, 1, &texture->d3d_sampler);
        }
        return 1;
    } else {
        DLOG("Failed to create sampler state: %s", d3d_strerror(result));
        texture->d3d_sampler = NULL;
        return 0;
    }
}

/*************************************************************************/
/*************************************************************************/
