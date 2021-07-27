/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/texture.c: Texture manipulation routines.
 */

#include "src/base.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/texture.h"
#include "src/utility/dds.h"
#include "src/utility/id-array.h"
#include "src/utility/png.h"
#include "src/utility/tex-file.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Largest number of mipmap levels (including the base image) to support
 * when parsing a texture file. */
#define MAX_TEXTURE_LEVELS  16

/*-----------------------------------------------------------------------*/

/* Structure used for storing texture data. */
typedef struct Texture Texture;
struct Texture {
    int width, height;   // Texture size, in pixels.

    float scale;         // Texture scale (texture size / display size).

    uint8_t readonly;    // Is the texture read-only? (for framebuffers)
    uint8_t locked;      // Is the texture locked?
    uint8_t sys_lock;    // Is it a system-level lock?
    uint8_t dirty;       // Is the pixel data dirty?

    SysTexture *systex;  // Low-level texture pointer.
    uint8_t *bitmap;     // Opaque bitmap pointer (NULL if none).
    uint8_t *lock_buf;   // Lock buffer for opaque bitmap reads.
};

/*-----------------------------------------------------------------------*/

/* Array of allocated textures. */
static IDArray textures = ID_ARRAY_INITIALIZER(100);

/**
 * VALIDATE_TEXTURE:  Validate the texture ID passed to a texture
 * manipulation routine, and store the corresponding pointer in the
 * variable "texture".  If the texture ID is invalid, the "error_return"
 * statement is executed; this may consist of multiple statements, but
 * must include a "return" to exit the function.
 */
#define VALIDATE_TEXTURE(id,texture,error_return) \
    ID_ARRAY_VALIDATE(&textures, (id), Texture *, texture, \
                      DLOG("Texture ID %d is invalid", _id); error_return)

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * dds_parse:  Parse a DDS-format texture file and return information
 * about the texture.
 *
 * All pointer parameters are guaranteed to be non-NULL.
 *
 * [Parameters]
 *     data: File data.
 *     size: File size, in bytes.
 *     format_ret: Pointer to variable to receive the texture format
 *         (TEX_FORMAT_*).
 *     width_ret: Pointer to variable to receive the texture width, in pixels.
 *     height_ret: Pointer to variable to receive the texture height, in
 *         pixels.
 *     stride_ret: Pointer to variable to receive the texture line size, in
 *         pixels.
 *     level_offsets_ret: Pointer to array of size MAX_TEXTURE_LEVELS to
 *         receive the offsets, in bytes, to each image level's data.
 *     level_sizes_ret: Pointer to array of size MAX_TEXTURE_LEVELS to
 *         receive the sizes, in bytes, of each image level's data.
 *     num_levels_ret: Pointer to variable to receive the number of image
 *         levels (including the base level).
 * [Return value]
 *     True if the data was successfully parsed, false if not.
 */
static int dds_parse(
    void *data, unsigned long size, TextureFormat *format_ret, int *width_ret,
    int *height_ret, int *stride_ret, int32_t *level_offsets_ret,
    int32_t *level_sizes_ret, int *num_levels_ret);

/**
 * tex_parse:  Parse a custom-format texture file and store its data in
 * the given texture.  If reuse is true, the data buffer is either reused
 * or freed on successful return.
 *
 * data, texture, and scale_ret are guaranteed to be non-NULL.
 *
 * [Parameters]
 *     data: File data.
 *     size: File size, in bytes.
 *     format_ret: Pointer to variable to receive the texture format
 *         (TEX_FORMAT_*).
 *     width_ret: Pointer to variable to receive the texture width, in pixels.
 *     height_ret: Pointer to variable to receive the texture height, in
 *         pixels.
 *     stride_ret: Pointer to variable to receive the texture line size, in
 *         pixels.
 *     scale_ret: Pointer to variable to receive the texture scale.
 *     level_offsets_ret: Pointer to array of size MAX_TEXTURE_LEVELS to
 *         receive the offsets, in bytes, to each image level's data.
 *     level_sizes_ret: Pointer to array of size MAX_TEXTURE_LEVELS to
 *         receive the sizes, in bytes, of each image level's data.
 *     num_levels_ret: Pointer to variable to receive the number of image
 *         levels (including the base level).
 *     bitmap_offset_ret: Pointer to variable to receive the offset, in
 *         bytes, of the opaque bitmap, or zero if none is included.
 * [Return value]
 *     True if the data was successfully parsed, false if not.
 */
static int tex_parse(
    void *data, unsigned long size, TextureFormat *format_ret, int *width_ret,
    int *height_ret, int *stride_ret, float *scale_ret,
    int32_t *level_offsets_ret, int32_t *level_sizes_ret, int *num_levels_ret,
    int32_t *bitmap_offset_ret);

/**
 * parse_common:  Common processing for dds_parse() and tex_parse().
 *
 * [Parameters]
 *     pixels_offset: Offset in file to beginning of pixel data, in bytes.
 *     pixels_size: Pixel data size, in bytes.
 *     format: Texture format (TEX_FORMAT_*).
 *     width, height: Texture size, in pixels.
 *     stride: Texture line size, in pixels.
 *     num_levels: Number of mipmap levels (including the base level).
 *     level_offsets_ret: Pointer to array of size MAX_TEXTURE_LEVELS to
 *         receive the offsets, in bytes, to each image level's data.
 *     level_sizes_ret: Pointer to array of size MAX_TEXTURE_LEVELS to
 *         receive the sizes, in bytes, of each image level's data.
 * [Return value]
 *     True if the data was successfully parsed, false if not.
 */
static int parse_common(
    int32_t pixels_offset, int32_t pixels_size, TextureFormat format,
    int width, int height, int stride, int num_levels,
    int32_t *level_offsets_ret, int32_t *level_sizes_ret);

/**
 * get_format_parameters:  Return various parameters for the given texture
 * format.  Return pointers are not modified on error.
 *
 * [Parameters]
 *     format: Texture format (TEX_FORMAT_*).
 *     bpp_ret: Pointer to variable to receive the number of bits per pixel.
 *     min_width_ret: Pointer to variable to receive the minimum texture
 *         width, in pixels.
 *     min_height_ret: Pointer to variable to receive the minimum texture
 *         height, in pixels.
 *     stride_align_ret: Pointer to variable to receive the stride alignment
 *         (the value the line stride must be a multiple of).
 *     palette_size_ret: Pointer to variable to receive the palette size,
 *         in bytes.
 * [Return value]
 *     True if the format is valid, false if not.
 */
static int get_format_parameters(
    TextureFormat format, int *bpp_ret, int *min_width_ret,
    int *min_height_ret, int *stride_align_ret, int *palette_size_ret);

/**
 * lock_opaque_bitmap:  Extract the given portion of the texture's opaque
 * bitmap into a newly-allocated (with mem_alloc()) buffer.
 *
 * [Parameters]
 *     texture: Texture from which to extract bitmap data.
 *     x, y: Base coordinate from which to extract, in pixels.
 *     w, h: Size of the region to extract, in pixels.
 * [Return value]
 *     RGBA pixel buffer containing the extracted data, or NULL on error.
 */
static uint8_t *lock_opaque_bitmap(const Texture *texture, int x, int y,
                                   int w, int h);

/*************************************************************************/
/*************** Interface: Texture creation and deletion ****************/
/*************************************************************************/

int texture_create(int width, int height, int mem_flags, int mipmaps)
{
    if (UNLIKELY(width <= 0) || UNLIKELY(height <= 0)) {
        DLOG("Invalid parameters: %d %d %d %d", width, height, mem_flags,
             mipmaps);
        goto error_return;
    }

    Texture *texture = debug_mem_alloc(
        sizeof(*texture), 0, mem_flags & ~MEM_ALLOC_CLEAR,
        __FILE__, __LINE__, MEM_INFO_TEXTURE);
    if (UNLIKELY(!texture)) {
        DLOG("No memory for texture");
        goto error_return;
    }

    texture->width    = width;
    texture->height   = height;
    texture->scale    = 1.0f;
    texture->readonly = 0;
    texture->locked   = 0;
    texture->sys_lock = 0;
    texture->dirty    = 0;
    texture->bitmap   = NULL;
    texture->lock_buf = NULL;
    texture->systex   = sys_texture_create(
        width, height, TEX_FORMAT_RGBA8888, 0, NULL, 0, NULL, NULL,
        mipmaps, mem_flags, 0);
    if (UNLIKELY(!texture->systex)) {
        DLOG("Failed to create %dx%d texture", width, height);
        goto error_free_texture;
    }

    const int id = id_array_register(&textures, texture);
    if (UNLIKELY(!id)) {
        DLOG("Failed to store new texture in array");
        goto error_destroy_systex;
    }

    return id;

  error_destroy_systex:
    sys_texture_destroy(texture->systex);
  error_free_texture:
    mem_free(texture);
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

int texture_create_with_data(
    int width, int height, const void *data, TextureFormat format,
    int stride, int mem_flags, int mipmaps)
{
    if (UNLIKELY(width <= 0)
     || UNLIKELY(height <= 0)
     || !data
     || UNLIKELY(stride < 0)) {
        DLOG("Invalid parameters: %d %d %p %d %d %d %d",
             width, height, data, format, stride, mem_flags, mipmaps);
        goto error_return;
    }

    int bpp, min_width, min_height, stride_align, palette_size;
    if (!get_format_parameters(format, &bpp, &min_width, &min_height,
                               &stride_align, &palette_size)) {
        goto error_return;
    }
    if (UNLIKELY(stride % stride_align != 0)) {
        DLOG("Stride %d invalid for format %d (must be a multiple of %d)",
             stride, format, stride_align);
        goto error_return;
    }

    Texture *texture = debug_mem_alloc(
        sizeof(*texture), 0, mem_flags & ~MEM_ALLOC_CLEAR,
        __FILE__, __LINE__, MEM_INFO_TEXTURE);
    if (UNLIKELY(!texture)) {
        DLOG("No memory for texture");
        goto error_return;
    }

    const int bounded_stride = lbound(stride, min_width);
    const int bounded_height = lbound(height, min_height);
    const int data_size =
        (bounded_stride * bounded_height * bpp + 7) / 8 + palette_size;

    texture->width    = width;
    texture->height   = height;
    texture->scale    = 1.0f;
    texture->readonly = 0;
    texture->locked   = 0;
    texture->sys_lock = 0;
    texture->dirty    = 0;
    texture->bitmap   = NULL;
    texture->lock_buf = NULL;
    texture->systex   = sys_texture_create(
        width, height, format, 1,
        (void *)data,  // Safe to de-const, since we pass 0 for the reuse flag.
        stride, (int32_t[]){0}, (int32_t[]){data_size}, mipmaps,
        mem_flags & ~MEM_ALLOC_CLEAR, 0);
    if (UNLIKELY(!texture->systex)) {
        DLOG("Failed to create %dx%d texture", width, height);
        goto error_free_texture;
    }

    const int id = id_array_register(&textures, texture);
    if (UNLIKELY(!id)) {
        DLOG("Failed to store new texture in array");
        goto error_destroy_systex;
    }

    return id;

  error_destroy_systex:
    sys_texture_destroy(texture->systex);
  error_free_texture:
    mem_free(texture);
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

int texture_create_from_display(
    int x, int y, int w, int h, int readable, int mem_flags, int mipmaps)
{
    if (UNLIKELY(w <= 0) || UNLIKELY(h <= 0)) {
        DLOG("Invalid parameters: %d %d %d %d %d 0x%X %d", x, y, w, h,
             readable, mem_flags, mipmaps);
        return 0;
    }

    /* Note that mipmaps and mem_flags are reversed in the sysdep interface
     * because reasons. */
    SysTexture *systex =
        sys_texture_grab(x, y, w, h, readable, mipmaps, mem_flags);
    if (!systex) {
        return 0;
    }

    const int id = texture_import(systex, mem_flags);
    if (!id) {
        DLOG("Failed to register texture");
        sys_texture_destroy(systex);
        return 0;
    }

    return id;
}

/*-----------------------------------------------------------------------*/

int texture_parse(void *data, int32_t len, int mem_flags, int mipmaps,
                  int reuse)
{
    if (UNLIKELY(!data) || UNLIKELY(len <= 0)) {
        DLOG("Invalid parameters: %p %d %u %d %d", data, len, mem_flags,
             mipmaps, reuse);
        goto error_return;
    }

    mem_flags &= ~MEM_ALLOC_CLEAR;

    TextureFormat format;
    int width, height, stride;
    float scale;
    int32_t level_offsets[MAX_TEXTURE_LEVELS], level_sizes[MAX_TEXTURE_LEVELS];
    int num_levels;
    int32_t opaque_bitmap_offset;

    if (len >= 8 && memcmp(data, "\x89PNG\x0D\x0A\x1A\x0A", 8) == 0) {
        void *pixels =
            png_parse(data, len, MEM_ALLOC_TEMP, &width, &height);
        if (!pixels) {
            DLOG("Failed to parse PNG file");
            goto error_return;
        }
        if (reuse) {  // We no longer need the original data.
            mem_free(data);
        }
        data = pixels;
        reuse = 1;  // Newly allocated buffer, so we can always reuse it.
        format = TEX_FORMAT_RGBA8888;
        stride = width;
        scale = 1;
        level_offsets[0] = 0;
        level_sizes[0] = width * height * 4;
        num_levels = 1;
        opaque_bitmap_offset = 0;

    } else if (len >= 4 && memcmp(data, "DDS ", 4) == 0) {
        if (!dds_parse(data, len, &format, &width, &height, &stride,
                       level_offsets, level_sizes, &num_levels)) {
            DLOG("Failed to parse DDS file");
            goto error_return;
        }
        scale = 1.0;
        opaque_bitmap_offset = 0;
        /* Mipmaps are stored with the texture file; if the file has none,
         * we assume mipmaps were intentionally disabled and don't generate
         * them even if we can. */
        mipmaps = 0;

    } else if (len >= 4 && memcmp(data, TEX_FILE_MAGIC, 4) == 0) {
        if (!tex_parse(data, len, &format, &width, &height, &stride, &scale,
                       level_offsets, level_sizes, &num_levels,
                       &opaque_bitmap_offset)) {
            DLOG("Failed to parse TEX file");
            goto error_return;
        }
        mipmaps = 0;  // As for DDS files.

    } else {
        DLOG("Unknown data format");
        goto error_return;
    }

    uint8_t *bitmap;
    if (opaque_bitmap_offset == 0) {
        bitmap = NULL;
    } else {
        const int32_t bitmap_stride = (width + 7) / 8;
        const int32_t bitmap_size = bitmap_stride * height;
        bitmap = debug_mem_alloc(bitmap_size, 0, mem_flags,
                                 __FILE__, __LINE__, MEM_INFO_TEXTURE);
        if (UNLIKELY(!bitmap)) {
            DLOG("No memory for opaque bitmap (%d bytes)", bitmap_size);
            goto error_return;
        }
        memcpy(bitmap, (const uint8_t *)data + opaque_bitmap_offset,
               bitmap_size);
    }

    Texture *texture = debug_mem_alloc(sizeof(*texture), 0, mem_flags,
                                       __FILE__, __LINE__, MEM_INFO_TEXTURE);
    if (UNLIKELY(!texture)) {
        DLOG("No memory for texture");
        goto error_free_bitmap;
    }

    texture->width    = width;
    texture->height   = height;
    texture->scale    = scale;
    texture->readonly = 0;
    texture->locked   = 0;
    texture->sys_lock = 0;
    texture->dirty    = 0;
    texture->bitmap   = bitmap;
    texture->lock_buf = NULL;
    texture->systex   =
        sys_texture_create(width, height, format, num_levels, data, stride,
                           level_offsets, level_sizes, mipmaps, mem_flags,
                           reuse);
    if (reuse) {
        data = NULL;  // Has now been freed or reused.
    }
    if (UNLIKELY(!texture->systex)) {
        DLOG("Failed to parse texture data");
        goto error_free_texture;
    }

    const int id = id_array_register(&textures, texture);
    if (UNLIKELY(!id)) {
        DLOG("Failed to store new texture in array");
        goto error_destroy_systex;
    }

    return id;

  error_destroy_systex:
    sys_texture_destroy(texture->systex);
  error_free_texture:
    mem_free(texture);
  error_free_bitmap:
    mem_free(bitmap);
  error_return:
    if (reuse) {
        mem_free(data);
    }
    return 0;
}

/*-----------------------------------------------------------------------*/

void texture_destroy(int texture_id)
{
    if (texture_id) {
        Texture *texture;
        VALIDATE_TEXTURE(texture_id, texture, return);
        if (texture->readonly) {
            DLOG("Attempt to destroy read-only texture %d", texture_id);
            return;
        }

        if (texture->locked && texture->sys_lock) {
            sys_texture_unlock(texture->systex, 0);
        }
        mem_free(texture->lock_buf);
        mem_free(texture->bitmap);
        sys_texture_destroy(texture->systex);
        mem_free(texture);
        id_array_release(&textures, texture_id);
    }
}

/*************************************************************************/
/*************** Interface: Texture information retrieval ****************/
/*************************************************************************/

int texture_width(int texture_id)
{
    const Texture *texture;
    VALIDATE_TEXTURE(texture_id, texture, return 0);

    return texture->width;
}

/*-----------------------------------------------------------------------*/

int texture_height(int texture_id)
{
    const Texture *texture;
    VALIDATE_TEXTURE(texture_id, texture, return 0);

    return texture->height;
}

/*-----------------------------------------------------------------------*/

float texture_scale(int texture_id)
{
    const Texture *texture;
    VALIDATE_TEXTURE(texture_id, texture, return 1.0f);

    return texture->scale;
}

/*-----------------------------------------------------------------------*/

int texture_has_mipmaps(int texture_id)
{
    const Texture *texture;
    VALIDATE_TEXTURE(texture_id, texture, return 0);

    return sys_texture_has_mipmaps(texture->systex);
}

/*************************************************************************/
/****************** Interface: Pixel data manipulation *******************/
/*************************************************************************/

void *texture_lock(int texture_id)
{
    Texture *texture;
    VALIDATE_TEXTURE(texture_id, texture, return NULL);
    if (texture->readonly) {
        DLOG("Attempt to modify read-only texture %d", texture_id);
        return NULL;
    }

    if (UNLIKELY(texture->locked)) {
        DLOG("Texture %d is already locked!", texture_id);
        return NULL;
    }

    if (texture->bitmap) {
        DLOG("Texture %d has an opaque bitmap and can't be locked read/write",
             texture_id);
        return NULL;
    }

    void *data = sys_texture_lock(texture->systex, SYS_TEXTURE_LOCK_NORMAL,
                                  0, 0, texture->width, texture->height);
    if (UNLIKELY(!data)) {
        return NULL;
    }

    texture->locked = 1;
    texture->sys_lock = 1;
    texture->dirty = 1;
    return data;
}

/*-----------------------------------------------------------------------*/

const void *texture_lock_readonly(int texture_id)
{
    Texture *texture;
    VALIDATE_TEXTURE(texture_id, texture, return NULL);

    if (UNLIKELY(texture->locked)) {
        DLOG("Texture %d is already locked!", texture_id);
        return NULL;
    }

    void *data;
    if (texture->bitmap) {
        data = lock_opaque_bitmap(texture,
                                  0, 0, texture->width, texture->height);
        texture->sys_lock = 0;
        texture->lock_buf = data;
    } else {
        data = sys_texture_lock(texture->systex, SYS_TEXTURE_LOCK_NORMAL,
                                0, 0, texture->width, texture->height);
        texture->sys_lock = 1;
    }
    if (UNLIKELY(!data)) {
        return NULL;
    }

    texture->locked = 1;
    return data;
}

/*-----------------------------------------------------------------------*/

const void *texture_lock_readonly_partial(int texture_id,
                                          int x, int y, int w, int h)
{
    Texture *texture;
    VALIDATE_TEXTURE(texture_id, texture, return NULL);

    if (UNLIKELY(x < 0) || UNLIKELY(x >= texture->width)
     || UNLIKELY(y < 0) || UNLIKELY(y >= texture->height)
        /* Careful about integer overflow!  Use a > c-b instead of a+b > c. */
     || UNLIKELY(w <= 0) || UNLIKELY(w > texture->width - x)
     || UNLIKELY(h <= 0) || UNLIKELY(h > texture->height - y)) {
        DLOG("Lock region %d,%d+%dx%d extends outside texture (size %dx%d)",
             x, y, w, h, texture->width, texture->height);
        return NULL;
    }

    if (UNLIKELY(texture->locked)) {
        DLOG("Texture %d is already locked!", texture_id);
        return NULL;
    }

    void *data;
    if (texture->bitmap) {
        data = lock_opaque_bitmap(texture, x, y, w, h);
        texture->sys_lock = 0;
        texture->lock_buf = data;
    } else {
        data = sys_texture_lock(texture->systex, SYS_TEXTURE_LOCK_NORMAL,
                                x, y, w, h);
        texture->sys_lock = 1;
    }
    if (UNLIKELY(!data)) {
        return NULL;
    }

    texture->locked = 1;
    return data;
}

/*-----------------------------------------------------------------------*/

void *texture_lock_writeonly(int texture_id)
{
    Texture *texture;
    VALIDATE_TEXTURE(texture_id, texture, return NULL);
    if (texture->readonly) {
        DLOG("Attempt to modify read-only texture %d", texture_id);
        return NULL;
    }

    if (UNLIKELY(texture->locked)) {
        DLOG("Texture %d is already locked!", texture_id);
        return NULL;
    }

    if (texture->bitmap) {
        DLOG("Note: discarding opaque bitmap for texture %d", texture_id);
        mem_free(texture->bitmap);
        texture->bitmap = NULL;
    }

    void *data = sys_texture_lock(texture->systex, SYS_TEXTURE_LOCK_DISCARD,
                                  0, 0, texture->width, texture->height);
    if (UNLIKELY(!data)) {
        return NULL;
    }

    texture->locked = 1;
    texture->sys_lock = 1;
    texture->dirty = 1;
    return data;
}

/*-----------------------------------------------------------------------*/

SysTexture *texture_lock_raw(int texture_id)
{
    Texture *texture;
    VALIDATE_TEXTURE(texture_id, texture, return NULL);

    if (UNLIKELY(texture->locked)) {
        DLOG("Texture %d is already locked!", texture_id);
        return NULL;
    }

    texture->locked = 1;
    texture->sys_lock = 0;
    texture->dirty = 1;
    return texture->systex;
}

/*-----------------------------------------------------------------------*/

void texture_unlock(int texture_id)
{
    Texture *texture;
    VALIDATE_TEXTURE(texture_id, texture, return);

    if (!texture->locked) {
        return;
    }

    if (texture->sys_lock) {
        sys_texture_unlock(texture->systex, texture->dirty);
        texture->sys_lock = 0;
    } else {
        if (texture->dirty) {
            sys_texture_flush(texture->systex);
        }
        mem_free(texture->lock_buf);
        texture->lock_buf = NULL;
    }
    texture->locked = 0;
    texture->dirty = 0;
}

/*************************************************************************/
/********************* Interface: Rendering control **********************/
/*************************************************************************/

void texture_set_repeat(int texture_id, int repeat_u, int repeat_v)
{
    Texture *texture;
    VALIDATE_TEXTURE(texture_id, texture, return);
    if (texture->readonly) {
        DLOG("Attempt to modify read-only texture %d", texture_id);
        return;
    }

    sys_texture_set_repeat(texture->systex, repeat_u, repeat_v);
}

/*-----------------------------------------------------------------------*/

void texture_set_antialias(int texture_id, int on)
{
    Texture *texture;
    VALIDATE_TEXTURE(texture_id, texture, return);
    if (texture->readonly) {
        DLOG("Attempt to modify read-only texture %d", texture_id);
        return;
    }

    sys_texture_set_antialias(texture->systex, on);
}

/*-----------------------------------------------------------------------*/

void texture_apply(int unit, int texture_id)
{
    if (UNLIKELY(unit < 0)) {
        DLOG("Invalid unit ID %d", unit);
        return;
    }

    if (texture_id) {
        const Texture *texture;
        VALIDATE_TEXTURE(texture_id, texture, return);
        sys_texture_apply(unit, texture->systex);
    } else {
        sys_texture_apply(unit, NULL);
    }
}

/*-----------------------------------------------------------------------*/

int texture_num_units(void)
{
    return sys_texture_num_units();
}

/*************************************************************************/
/********************** Internal interface routines **********************/
/*************************************************************************/

int texture_import(SysTexture *systex, int mem_flags)
{
    if (UNLIKELY(!systex)) {
        DLOG("Invalid parameters: %p 0x%X", systex, mem_flags);
        return 0;
    }

    Texture *texture = debug_mem_alloc(
        sizeof(*texture), 0, mem_flags & ~MEM_ALLOC_CLEAR,
        __FILE__, __LINE__, MEM_INFO_TEXTURE);
    if (UNLIKELY(!texture)) {
        DLOG("No memory for texture");
        return 0;
    }

    texture->width    = sys_texture_width(systex);
    texture->height   = sys_texture_height(systex);
    texture->scale    = 1.0f;
    texture->readonly = 0;
    texture->locked   = 0;
    texture->sys_lock = 0;
    texture->dirty    = 0;
    texture->systex   = systex;
    texture->bitmap   = NULL;
    texture->lock_buf = NULL;

    const int id = id_array_register(&textures, texture);
    if (UNLIKELY(!id)) {
        DLOG("Failed to store texture %p in array", systex);
        mem_free(texture);
        return 0;
    }

    return id;
}

/*-----------------------------------------------------------------------*/

int texture_import_readonly(SysTexture *systex)
{
    if (UNLIKELY(!systex)) {
        DLOG("systex == NULL");
        return 0;
    }

    Texture *texture = debug_mem_alloc(
        sizeof(*texture), 0, 0, __FILE__, __LINE__, MEM_INFO_TEXTURE);
    if (UNLIKELY(!texture)) {
        DLOG("No memory for texture");
        return 0;
    }

    texture->width    = sys_texture_width(systex);
    texture->height   = sys_texture_height(systex);
    texture->scale    = 1.0f;
    texture->readonly = 1;
    texture->locked   = 0;
    texture->sys_lock = 0;
    texture->dirty    = 0;
    texture->systex   = systex;
    texture->bitmap   = NULL;
    texture->lock_buf = NULL;

    const int id = id_array_register(&textures, texture);
    if (UNLIKELY(!id)) {
        DLOG("Failed to store texture %p in array", systex);
        mem_free(texture);
        return 0;
    }

    return id;
}

/*-----------------------------------------------------------------------*/

void texture_forget_readonly(int texture_id)
{
    if (texture_id) {
        Texture *texture;
        VALIDATE_TEXTURE(texture_id, texture, return);
        if (UNLIKELY(!texture->readonly)) {
            DLOG("Ignoring attempt to forget non-readonly texture %d",
                 texture_id);
            return;
        }

        if (texture->locked && texture->sys_lock) {
            sys_texture_unlock(texture->systex, 0);
        }
        mem_free(texture->lock_buf);
        mem_free(texture->bitmap);
        mem_free(texture);
        id_array_release(&textures, texture_id);
    }
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static int dds_parse(
    void *data, unsigned long size, TextureFormat *format_ret, int *width_ret,
    int *height_ret, int *stride_ret, int32_t *level_offsets_ret,
    int32_t *level_sizes_ret, int *num_levels_ret)
{
    DDSInfo info;
    if (!dds_get_info(data, size, &info)) {
        DLOG("Failed to parse DDS file");
        return 0;
    }
    ASSERT(info.width > 0, return 0);  // dds_get_info() guarantees these.
    ASSERT(info.height > 0, return 0);
    ASSERT(info.stride >= 0, return 0);

    *format_ret     = info.format;
    *width_ret      = info.width;
    *height_ret     = info.height;
    *stride_ret     = info.stride;
    *num_levels_ret = info.mipmaps + 1;
    return parse_common(0x80, size - 0x80, *format_ret, *width_ret,
                        *height_ret, *stride_ret, *num_levels_ret,
                        level_offsets_ret, level_sizes_ret);
}

/*-----------------------------------------------------------------------*/

static int tex_parse(
    void *data, unsigned long size, TextureFormat *format_ret, int *width_ret,
    int *height_ret, int *stride_ret, float *scale_ret,
    int32_t *level_offsets_ret, int32_t *level_sizes_ret, int *num_levels_ret,
    int32_t *bitmap_offset_ret)
{
    TexFileHeader header;
    if (!tex_parse_header(data, size, &header)) {
        return 0;
    }
    if (header.width <= 0 || header.height <= 0) {
        DLOG("TEX file has zero-size texture");
        return 0;
    }
    if (header.scale <= 0) {
        DLOG("TEX file has scale <= 0");
        return 0;
    }
    if (header.opaque_bitmap) {
        const int32_t bitmap_stride = (header.width + 7) / 8;
        /* tex_parse_header() guarantees this. */
        ASSERT(header.bitmap_size >= bitmap_stride * header.height, return 0);
    }

    int stride;
    switch (header.format) {
      case TEX_FORMAT_PSP_RGBA8888:
      case TEX_FORMAT_PSP_RGBA8888_SWIZZLED:
        stride = align_up(header.width, 4);
        break;
      case TEX_FORMAT_PSP_RGB565:
      case TEX_FORMAT_PSP_RGBA5551:
      case TEX_FORMAT_PSP_RGBA4444:
      case TEX_FORMAT_PSP_RGB565_SWIZZLED:
      case TEX_FORMAT_PSP_RGBA5551_SWIZZLED:
      case TEX_FORMAT_PSP_RGBA4444_SWIZZLED:
        stride = align_up(header.width, 8);
        break;
      case TEX_FORMAT_PSP_A8:
      case TEX_FORMAT_PSP_L8:
      case TEX_FORMAT_PSP_PALETTE8_RGBA8888:
      case TEX_FORMAT_PSP_A8_SWIZZLED:
      case TEX_FORMAT_PSP_L8_SWIZZLED:
      case TEX_FORMAT_PSP_PALETTE8_RGBA8888_SWIZZLED:
        stride = align_up(header.width, 16);
        break;
      default:
        stride = header.width;
        break;
    }

    *format_ret        = header.format;
    *width_ret         = header.width;
    *height_ret        = header.height;
    *stride_ret        = stride;
    *scale_ret         = header.scale;
    *num_levels_ret    = header.mipmaps + 1;
    *bitmap_offset_ret = header.opaque_bitmap ? header.bitmap_offset : 0;
    return parse_common(header.pixels_offset, header.pixels_size,
                        *format_ret, *width_ret, *height_ret, *width_ret,
                        *num_levels_ret, level_offsets_ret, level_sizes_ret);
}

/*-----------------------------------------------------------------------*/

static int parse_common(
    int32_t pixels_offset, int32_t pixels_size, TextureFormat format,
    UNUSED int width, int height, int stride, int num_levels,
    int32_t *level_offsets_ret, int32_t *level_sizes_ret)
{
    int bpp, min_width, min_height, stride_align, palette_size;
    if (!get_format_parameters(format, &bpp, &min_width, &min_height,
                               &stride_align, &palette_size)) {
        return 0;
    }

    for (int level = 0; level < num_levels; level++) {
        const int level_w = lbound(stride >> level, min_width);
        const int level_h = lbound(height >> level, min_height);
        const int level_s = align_up(level_w, stride_align);
        int32_t level_size = (level_s * level_h * bpp) / 8;
        if (level == 0) {
            level_size += palette_size;  // Include palette in 0th level data.
        }
        if (level_size > pixels_size) {
            DLOG("Not enough data for level %d (%dx%dx%d): need %d, have %d",
                 level, level_s, level_h, bpp, level_size, pixels_size);
            return 0;
        }
        level_offsets_ret[level] = pixels_offset;
        level_sizes_ret[level] = level_size;
        pixels_offset += level_size;
        pixels_size -= level_size;
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

static int get_format_parameters(
    TextureFormat format, int *bpp_ret, int *min_width_ret,
    int *min_height_ret, int *stride_align_ret, int *palette_size_ret)
{
    switch (format) {
      case TEX_FORMAT_RGBA8888:
      case TEX_FORMAT_BGRA8888:
        *bpp_ret = 32;
        *min_width_ret = 1;
        *min_height_ret = 1;
        *stride_align_ret = 1;
        *palette_size_ret = 0;
        return 1;

      case TEX_FORMAT_RGB565:
      case TEX_FORMAT_RGBA5551:
      case TEX_FORMAT_RGBA4444:
      case TEX_FORMAT_BGR565:
      case TEX_FORMAT_BGRA5551:
      case TEX_FORMAT_BGRA4444:
        *bpp_ret = 16;
        *min_width_ret = 1;
        *min_height_ret = 1;
        *stride_align_ret = 1;
        *palette_size_ret = 0;
        return 1;

      case TEX_FORMAT_A8:
      case TEX_FORMAT_L8:
        *bpp_ret = 8;
        *min_width_ret = 1;
        *min_height_ret = 1;
        *stride_align_ret = 1;
        *palette_size_ret = 0;
        return 1;

      case TEX_FORMAT_PALETTE8_RGBA8888:
        *bpp_ret = 8;
        *min_width_ret = 1;
        *min_height_ret = 1;
        *stride_align_ret = 1;
        *palette_size_ret = 256*4;
        return 1;

      case TEX_FORMAT_PSP_RGBA8888:
      case TEX_FORMAT_PSP_RGBA8888_SWIZZLED:
        *bpp_ret = 32;
        *min_width_ret = 1;
        *min_height_ret = 1;
        *stride_align_ret = 4;
        *palette_size_ret = 0;
        return 1;

      case TEX_FORMAT_PSP_RGB565:
      case TEX_FORMAT_PSP_RGB565_SWIZZLED:
      case TEX_FORMAT_PSP_RGBA5551:
      case TEX_FORMAT_PSP_RGBA5551_SWIZZLED:
      case TEX_FORMAT_PSP_RGBA4444:
      case TEX_FORMAT_PSP_RGBA4444_SWIZZLED:
        *bpp_ret = 16;
        *min_width_ret = 1;
        *min_height_ret = 1;
        *stride_align_ret = 8;
        *palette_size_ret = 0;
        return 1;

      case TEX_FORMAT_PSP_A8:
      case TEX_FORMAT_PSP_A8_SWIZZLED:
      case TEX_FORMAT_PSP_L8:
      case TEX_FORMAT_PSP_L8_SWIZZLED:
        *bpp_ret = 8;
        *min_width_ret = 1;
        *min_height_ret = 1;
        *stride_align_ret = 16;
        *palette_size_ret = 0;
        return 1;

      case TEX_FORMAT_PSP_PALETTE8_RGBA8888:
      case TEX_FORMAT_PSP_PALETTE8_RGBA8888_SWIZZLED:
        *bpp_ret = 8;
        *min_width_ret = 1;
        *min_height_ret = 1;
        *stride_align_ret = 16;
        *palette_size_ret = 256*4;
        return 1;

      case TEX_FORMAT_S3TC_DXT1:
        *bpp_ret = 4;
        *min_width_ret = 4;
        *min_height_ret = 4;
        *stride_align_ret = 1;
        *palette_size_ret = 0;
        return 1;

      case TEX_FORMAT_S3TC_DXT3:
      case TEX_FORMAT_S3TC_DXT5:
        *bpp_ret = 8;
        *min_width_ret = 4;
        *min_height_ret = 4;
        *stride_align_ret = 1;
        *palette_size_ret = 0;
        return 1;

      case TEX_FORMAT_PVRTC2_RGBA:
      case TEX_FORMAT_PVRTC2_RGB:
        *bpp_ret = 2;
        *min_width_ret = 16;
        *min_height_ret = 8;
        *stride_align_ret = 1;
        *palette_size_ret = 0;
        return 1;

      case TEX_FORMAT_PVRTC4_RGBA:
      case TEX_FORMAT_PVRTC4_RGB:
        *bpp_ret = 4;
        *min_width_ret = 8;
        *min_height_ret = 8;
        *stride_align_ret = 1;
        *palette_size_ret = 0;
        return 1;
    }

    /* Use this test instead of a default case so we get compiler warnings
     * if we forget to handle any data types. */
    DLOG("Pixel format %d unknown/unsupported", format);
    return 0;
}

/*-----------------------------------------------------------------------*/

static uint8_t *lock_opaque_bitmap(const Texture *texture, int x, int y,
                                   int w, int h)
{
    PRECOND(texture != NULL, return NULL);
    PRECOND(texture->bitmap != NULL, return NULL);
    PRECOND(x >= 0 && x < texture->width, return NULL);
    PRECOND(y >= 0 && y < texture->height, return NULL);
    PRECOND(w > 0 && w <= texture->width - x, return NULL);
    PRECOND(h > 0 && h <= texture->height - y, return NULL);

    uint32_t *outbuf = mem_alloc(w*h*4, 4, MEM_ALLOC_TEMP);
    if (UNLIKELY(!outbuf)) {
        DLOG("No memory for lock buffer (%d bytes)", w*h*4);
        return NULL;
    }

    const int32_t bitmap_stride = (texture->width + 7) / 8;
    const uint8_t *row_src = texture->bitmap + y*bitmap_stride;
    uint32_t *dest = outbuf;
    /* Use "unsigned int" here and below for better optimization. */
    const unsigned int col_limit = x+w;
    for (unsigned int row = 0; row < (unsigned int)h;
         row++, row_src += bitmap_stride)
    {
        unsigned int col = x;
        const uint8_t *src = row_src + col/8;
        /* Assume that most requests will be on 8-pixel boundaries, and
         * optimize the fast path by marking these UNLIKELY. */
        if (UNLIKELY((unsigned int)x % 8 != 0)) {
            uint8_t byte = *src;
            byte >>= col%8;
            for (; col < col_limit && col%8 != 0; col++, dest++, byte>>=1) {
                *dest = (byte & 1) ? 0xFFFFFFFF : 0;
            }
            src++;
        }
        for (; col+8 <= col_limit; col += 8, src++, dest += 8) {
            const uint8_t byte = *src;
            dest[0] = (byte & 1<<0) ? 0xFFFFFFFF : 0;
            dest[1] = (byte & 1<<1) ? 0xFFFFFFFF : 0;
            dest[2] = (byte & 1<<2) ? 0xFFFFFFFF : 0;
            dest[3] = (byte & 1<<3) ? 0xFFFFFFFF : 0;
            dest[4] = (byte & 1<<4) ? 0xFFFFFFFF : 0;
            dest[5] = (byte & 1<<5) ? 0xFFFFFFFF : 0;
            dest[6] = (byte & 1<<6) ? 0xFFFFFFFF : 0;
            dest[7] = (byte & 1<<7) ? 0xFFFFFFFF : 0;
        }
        /* Use a constant condition here (col_limit instead of col) to
         * help out the optimizer.  This has no effect on output; if
         * 8n < x < x+w < 8(n+1) for some integer n, then an extra read
         * will be performed from *src, but the for loop will exit
         * immediately. */
        if (UNLIKELY(col_limit % 8 != 0)) {
            uint8_t byte = *src;
            for (; col < col_limit; col++, dest++, byte>>=1) {
                *dest = (byte & 1) ? 0xFFFFFFFF : 0;
            }
        }
    }

    return (uint8_t *)outbuf;
}

/*************************************************************************/
/*************************************************************************/
