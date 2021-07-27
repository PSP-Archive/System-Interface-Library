/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/utility/png.c: Functions for parsing and creating PNG files.
 */

#include "src/base.h"
#include "src/memory.h"
#include "src/utility/png.h"

#ifndef SIL_UTILITY_INCLUDE_PNG

/* Stub functions so linking doesn't fail. */

uint8_t *png_parse(UNUSED const void *data, UNUSED uint32_t size,
                   UNUSED int mem_flags, UNUSED int *width_ret,
                   UNUSED int *height_ret)
{
    DLOG("PNG support not compiled in");
    return NULL;
}
void *png_create(UNUSED const void *pixels, UNUSED int width,
                 UNUSED int height, UNUSED int keep_alpha,
                 UNUSED int comp_level, UNUSED int flush_interval,
                 UNUSED int mem_flags, UNUSED uint32_t *size_ret)
{
    DLOG("PNG support not compiled in");
    return NULL;
}

#else  // defined(SIL_UTILITY_INCLUDE_PNG), to the end of the file.

#define PNG_USER_MEM_SUPPORTED
#include <zlib.h>
#include <png.h>

/*************************************************************************/
/****************** Global data (used only for testing) ******************/
/*************************************************************************/

#ifdef SIL_INCLUDE_TESTS
int TEST_png_create_num_allocs;
#endif

/*************************************************************************/
/********************* Callback routines for libpng **********************/
/*************************************************************************/

/* Read callback and associated data structure. */

typedef struct pngRFILE pngRFILE;
struct pngRFILE {
    const uint8_t *data;
    uint32_t size;
    uint32_t pos;
};

static void png_read(png_structp png, png_bytep data, png_size_t length)
{
    pngRFILE *f = (pngRFILE *)png_get_io_ptr(png);
    if (length > f->size - f->pos) {
        png_error(png, "Out of data");
        return;
    }
    memcpy(data, f->data + f->pos, length);
    f->pos += length;
}

/*-----------------------------------------------------------------------*/

/* Write callbacks and associated data structure. */

typedef struct pngWFILE pngWFILE;
struct pngWFILE {
    uint8_t *data;
    uint32_t size;  // Allocated size of the buffer.
    uint32_t len;   // Amount of data actually stored in the buffer.
    int mem_flags;
};

static void png_write(png_structp png, png_bytep data, png_size_t length)
{
    pngWFILE *f = (pngWFILE *)png_get_io_ptr(png);
    if (f->len + length > f->size) {
        const uint32_t new_size =
            align_up(f->len + length, SIL_UTILITY_PNG_ALLOC_CHUNK);
        uint8_t *new_data = mem_realloc(f->data, new_size, f->mem_flags);
        if (!new_data) {
            png_error(png, "Out of memory");
            return;
        }
#ifdef SIL_INCLUDE_TESTS
        TEST_png_create_num_allocs++;
#endif
        f->data = new_data;
        f->size = new_size;
    }
    memcpy(f->data + f->len, data, length);
    f->len += length;
}

static void png_flush(UNUSED png_structp png) { /* Nothing to do. */ }

/*-----------------------------------------------------------------------*/

/* Memory allocation callbacks.  To avoid collisions with libpng functions,
 * we append "_callback" to the function names. */

static png_voidp png_malloc_callback(UNUSED png_structp png,
                                     png_alloc_size_t size)
{
    return mem_alloc(size, 0, MEM_ALLOC_TEMP);
}

static void png_free_callback(UNUSED png_structp png, png_voidp ptr)
{
    mem_free(ptr);
}

/*-----------------------------------------------------------------------*/

/* Warning/error callbacks.  Again, we have to avoid libpng name collisions
 * so we append _callback to the function names. */

static void png_warning_callback(UNUSED png_structp png,
                                 DEBUG_USED const char *message)
{
    DLOG("libpng warning: %s", message);
}

static void png_error_callback(png_structp png, DEBUG_USED const char *message)
{
    DLOG("libpng error: %s", message);
    longjmp(*(jmp_buf *)png_get_error_ptr(png), 1);
}

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

uint8_t *png_parse(const void *data, uint32_t size, int mem_flags,
                   int *width_ret, int *height_ret)
{
    PRECOND(data != NULL, return NULL);
    PRECOND(width_ret != NULL, return NULL);
    PRECOND(height_ret != NULL, return NULL);

    /* Jump target for libpng errors. */
    jmp_buf png_jmpbuf;

    /* We have to be able to free these on error, so we need volatile
     * declarations. */
    png_structp volatile png_volatile = NULL;
    png_infop volatile info_volatile = NULL;
    uint8_t * volatile pixels_volatile = NULL;
    uint8_t ** volatile row_pointers_volatile = NULL;

    if (setjmp(png_jmpbuf) != 0) {
        /* libpng jumped back here with an error, so return the error. */
      error:  // Let's reuse it for our own error handling, too.
        mem_free(row_pointers_volatile);
        mem_free(pixels_volatile);
        png_structp png = png_volatile;
        png_infop info = info_volatile;
        png_destroy_read_struct(&png, &info, NULL);
        return NULL;
    }


    /* Set up the PNG reader instance. */

    png_structp png = png_create_read_struct_2(
        PNG_LIBPNG_VER_STRING,
        &png_jmpbuf, png_error_callback, png_warning_callback,
        NULL, png_malloc_callback, png_free_callback);
    png_volatile = png;
    png_set_user_limits(png,
                        SIL_UTILITY_PNG_MAX_SIZE, SIL_UTILITY_PNG_MAX_SIZE);
    png_infop info = png_create_info_struct(png);
    info_volatile = info;
    pngRFILE in = {.data = data, .size = size, .pos = 0};
    png_set_read_fn(png, &in, png_read);

    /* Read the image information. */

    png_read_info(png, info);
    const unsigned int width      = png_get_image_width(png, info);
    const unsigned int height     = png_get_image_height(png, info);
    const unsigned int bit_depth  = png_get_bit_depth(png, info);
    const unsigned int color_type = png_get_color_type(png, info);
    if (UNLIKELY(png_get_interlace_type(png, info) != PNG_INTERLACE_NONE)) {
        DLOG("Interlaced images not supported");
        goto error;
    }

    /* Set up image transformation parameters. */

    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png);
    } else if (color_type == PNG_COLOR_TYPE_GRAY
            || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png);
    }
    if (bit_depth < 8) {
        png_set_expand(png);
    } else if (bit_depth == 16) {
        png_set_scale_16(png);
    }
    if (png_get_valid(png, info, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png);
    } else if (color_type == PNG_COLOR_TYPE_RGB
            || color_type == PNG_COLOR_TYPE_PALETTE
            || color_type == PNG_COLOR_TYPE_GRAY) {
        png_set_add_alpha(png, 0xFF, PNG_FILLER_AFTER);
    }
    png_read_update_info(png, info);

    /* Allocate memory for the pixel data. */

    const uint32_t row_size = width * 4;
    const uint32_t pixels_size = height * row_size;
    uint8_t *pixels = mem_alloc(pixels_size, 0, mem_flags & ~MEM_ALLOC_CLEAR);
    if (UNLIKELY(!pixels)) {
        DLOG("Out of memory for pixel data (%ux%u, %u bytes)",
             width, height, pixels_size);
        goto error;
    }
    pixels_volatile = pixels;

    /* Read the image into the pixel buffer. */

    uint8_t **row_pointers =
        mem_alloc(sizeof(*row_pointers) * height, 0, MEM_ALLOC_TEMP);
    if (UNLIKELY(!row_pointers)) {
        DLOG("Out of memory for pixel row pointers (%u bytes)",
             (int)(sizeof(*row_pointers) * height));
        goto error;
    }
    row_pointers_volatile = row_pointers;

    row_pointers[0] = pixels;
    for (unsigned int row = 1; row < height; row++) {
        row_pointers[row] = row_pointers[row-1] + row_size;
    }

    png_read_rows(png, row_pointers, NULL, height);

    row_pointers_volatile = NULL;
    mem_free(row_pointers);

    /* Done!  Close down the PNG reader and return the image data. */

    png_read_end(png, NULL);
    png_destroy_read_struct(&png, &info, NULL);
    *width_ret = width;
    *height_ret = height;
    return pixels;
}

/*-----------------------------------------------------------------------*/

void *png_create(const void *pixels, int width, int height, int keep_alpha,
                 int comp_level, int flush_interval, int mem_flags,
                 uint32_t *size_ret)
{
    PRECOND(pixels != NULL, return NULL);
    PRECOND(width > 0, return NULL);
    PRECOND(height > 0, return NULL);
    PRECOND(comp_level >= -1 && comp_level <= 9, return NULL);
    PRECOND(size_ret != NULL, return NULL);

#ifdef SIL_INCLUDE_TESTS
    TEST_png_create_num_allocs = 0;
#endif

    volatile int real_comp_level;
    if (comp_level == -1) {
        real_comp_level = SIL_UTILITY_PNG_COMPRESSION_LEVEL;
    } else {
        real_comp_level = comp_level;
    }

    /* Jump target for libpng errors. */
    jmp_buf png_jmpbuf;

    /* As with png_parse(), these need to be volatile. */
    png_structp volatile png_volatile = NULL;
    png_infop volatile info_volatile = NULL;
    pngWFILE volatile out_volatile =
        {.data = NULL, .size = 0, .len = 0, .mem_flags = mem_flags};
    uint8_t ** volatile row_pointers_volatile = NULL;

    if (setjmp(png_jmpbuf) != 0) {
        /* libpng (or we) jumped back with an error, so return the error. */
      error:
        mem_free(row_pointers_volatile);
        mem_free(out_volatile.data);
        png_structp png = png_volatile;
        png_infop info = info_volatile;
        png_destroy_write_struct(&png, &info);
        return NULL;
    }


    /* Set up the PNG writer instance. */

    png_structp png = png_create_write_struct_2(
        PNG_LIBPNG_VER_STRING,
        &png_jmpbuf, png_error_callback, png_warning_callback,
        NULL, png_malloc_callback, png_free_callback);
    png_volatile = png;
    png_infop info = png_create_info_struct(png);
    info_volatile = info;
    /* This cast is safe; we declare out_volatile as volatile only to
     * guarantee that out.data is valid after a longjmp. */
    png_set_write_fn(png, (pngWFILE *)&out_volatile, png_write, png_flush);

    /* Set up image encoding parameters. */

    png_set_IHDR(png, info, width, height, 8,
                 keep_alpha ? PNG_COLOR_TYPE_RGB_ALPHA : PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_set_compression_level(png, real_comp_level);
    png_set_compression_mem_level(png, 9);
    png_set_compression_window_bits(png, 15);

    /* Create the PNG data. */

    uint8_t **row_pointers =
        mem_alloc(height * sizeof(uint8_t *), 0, MEM_ALLOC_TEMP);
    if (!row_pointers) {
        DLOG("Out of memory for %d row pointers", height);
        goto error;
    }
    row_pointers_volatile = row_pointers;
    for (int y = 0; y < height; y++) {
        /* These have to be de-consted because png_set_rows() takes a
         * non-const pointer.  We assume libpng behaves nicely and
         * doesn't stomp on anything. */
        row_pointers[y] = &((uint8_t *)pixels)[y * width * 4];
    }
    png_set_rows(png, info, row_pointers);
    if (flush_interval > 0) {
        png_set_flush(png, flush_interval);
    }

    png_write_png(png, info, (keep_alpha
                              ? PNG_TRANSFORM_IDENTITY
                              : PNG_TRANSFORM_STRIP_FILLER_AFTER), NULL);

    row_pointers_volatile = NULL;
    mem_free(row_pointers);

    /* Done!  Close down the PNG writer, trim any unused space from the
     * data buffer, and return the buffer. */

    png_destroy_write_struct(&png, &info);
    void *data = out_volatile.data;
    uint32_t size = out_volatile.len;
    void *new_data = mem_realloc(data, size, mem_flags);
    if (LIKELY(new_data)) {
        data = new_data;
    }
    *size_ret = size;
    return data;
}

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_UTILITY_INCLUDE_PNG
