/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/psp/texture.c: PSP-specific texture tests.
 */

#include "src/base.h"
#include "src/graphics.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/psp/ge-util.h"
#include "src/sysdep/psp/internal.h"
#include "src/test/base.h"
#include "src/test/graphics/internal.h"
#include "src/texture.h"

/*************************************************************************/
/******************************* Test data *******************************/
/*************************************************************************/

/* These are borrowed from src/test/graphics/texture.c, with a few
 * modifications. */

static const uint8_t rgba_8x4_rgb565[] = {
      0,  0,  0,255,  8,  0,  8,255, 25,  0, 25,255, 41,  0, 41,255,
      0,  0,  0,255,  0,  0,  0,255,  0,  0,  0,255,  0,  0,  0,255,
      0, 65, 66,255,  8, 65, 74,255, 25, 65, 91,255, 41, 65,107,255,
      0,  0,  0,255,  0,  0,  0,255,  0,  0,  0,255,  0,  0,  0,255,
      0,130,132,255,  8,130,140,255, 25,130,157,255, 41,130,173,255,
      0,  0,  0,255,  0,  0,  0,255,  0,  0,  0,255,  0,  0,  0,255,
      0,195,198,255,  8,195,206,255, 25,195,223,255, 41,195,239,255,
      0,  0,  0,255,  0,  0,  0,255,  0,  0,  0,255,  0,  0,  0,255,
};

static const uint8_t rgba_16x4_alpha[] = {
    255,255,255,  0,255,255,255, 16,255,255,255, 32,255,255,255, 48,
    255,255,255,  0,255,255,255,  0,255,255,255,  0,255,255,255,  0,
    255,255,255,  0,255,255,255,  0,255,255,255,  0,255,255,255,  0,
    255,255,255,  0,255,255,255,  0,255,255,255,  0,255,255,255,  0,
    255,255,255, 64,255,255,255, 80,255,255,255, 96,255,255,255,112,
    255,255,255,  0,255,255,255,  0,255,255,255,  0,255,255,255,  0,
    255,255,255,  0,255,255,255,  0,255,255,255,  0,255,255,255,  0,
    255,255,255,  0,255,255,255,  0,255,255,255,  0,255,255,255,  0,
    255,255,255,128,255,255,255,144,255,255,255,160,255,255,255,176,
    255,255,255,  0,255,255,255,  0,255,255,255,  0,255,255,255,  0,
    255,255,255,  0,255,255,255,  0,255,255,255,  0,255,255,255,  0,
    255,255,255,  0,255,255,255,  0,255,255,255,  0,255,255,255,  0,
    255,255,255,192,255,255,255,208,255,255,255,224,255,255,255,240,
    255,255,255,  0,255,255,255,  0,255,255,255,  0,255,255,255,  0,
    255,255,255,  0,255,255,255,  0,255,255,255,  0,255,255,255,  0,
    255,255,255,  0,255,255,255,  0,255,255,255,  0,255,255,255,  0,
};

static const ALIGNED(4) uint8_t tex_8x4_psp_rgb565[] = {
    'T','E','X', 10,  2,113,  0,  0,  0,  8,  0,  4,  0,  1,  0,  0,
      0,  0,  0, 64,  0,  0,  0, 64,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  1,  8,  3, 24,  5, 40,  0,  0,  0,  0,  0,  0,  0,  0,
      0, 66,  1, 74,  3, 90,  5,106,  0,  0,  0,  0,  0,  0,  0,  0,
      0,132,  1,140,  3,156,  5,172,  0,  0,  0,  0,  0,  0,  0,  0,
      0,198,  1,206,  3,222,  5,238,  0,  0,  0,  0,  0,  0,  0,  0,
};

static const ALIGNED(4) uint8_t tex_16x4_psp_alpha[] = {
    'T','E','X', 10,  2,116,  0,  0,  0, 16,  0,  4,  0,  1,  0,  0,
      0,  0,  0, 64,  0,  0,  0, 64,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0, 16, 32, 48,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     64, 80, 96,112,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    128,144,160,176,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    192,208,224,240,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};

static const ALIGNED(4) uint8_t tex_4x4_psp_palette8_rgba8888[] = {
    'T','E','X', 10,  2,117,  0,  0,  0,  4,  0,  4,  0,  1,  0,  0,
      0,  0,  0, 64,  0,  0,  4, 64,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      4,  0,  4, 16,  8,  0,  8, 32, 12,  0, 12, 48,  0, 64, 64, 64,
      4, 64, 68, 80,  8, 64, 72, 96, 12, 64, 76,112,  0,128,128,128,
      4,128,132,144,  8,128,136,160, 12,128,140,176,  0,192,192,192,
      4,192,196,208,  8,192,200,224, 12,192,204,240,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     15,  0,  1,  2,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      3,  4,  5,  6,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      7,  8,  9, 10,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     11, 12, 13, 14,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};

/*************************************************************************/
/***************** Test runner and init/cleanup routines *****************/
/*************************************************************************/

static int do_test_psp_texture(void);
int test_psp_texture(void)
{
    CHECK_TRUE(graphics_init());
    const int result = do_test_psp_texture();
    graphics_cleanup();
    return result;
}

DEFINE_GENERIC_TEST_RUNNER(do_test_psp_texture)

/*-----------------------------------------------------------------------*/

TEST_INIT(init)
{
    graphics_start_frame();
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
    graphics_finish_frame();
    graphics_flush_resources();
    return 1;
}

/*************************************************************************/
/*************** Test routines: Exported utility routines ****************/
/*************************************************************************/

TEST(test_get_pixel_data)
{
    uint8_t *data;
    ASSERT(data = mem_alloc(sizeof(tex_4x4_psp_palette8_rgba8888), 64, 0));
    memcpy(data, tex_4x4_psp_palette8_rgba8888,
           sizeof(tex_4x4_psp_palette8_rgba8888));

    int texture;
    ASSERT(texture = texture_parse((void *)data,
                                   sizeof(tex_4x4_psp_palette8_rgba8888),
                                   0, 0, 1));
    SysTexture *systex = texture_lock_raw(texture);
    ASSERT((void *)systex == (void *)data);

    CHECK_PTREQUAL(psp_texture_get_pixel_data(systex), data + 64 + 256*4);

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_write_pixels_while_loaded)
{
    uint8_t *data;
    ASSERT(data = mem_alloc(sizeof(tex_4x4_psp_palette8_rgba8888), 64, 0));
    memcpy(data, tex_4x4_psp_palette8_rgba8888,
           sizeof(tex_4x4_psp_palette8_rgba8888));
    int texture;
    ASSERT(texture = texture_parse((void *)data,
                                   sizeof(tex_4x4_psp_palette8_rgba8888),
                                   0, 0, 1));
    texture_set_antialias(texture, 0);
    texture_apply(0, texture);

    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_set_viewport(0, 0, 4, 4);
    graphics_set_blend(GRAPHICS_BLEND_ADD,
                       GRAPHICS_BLEND_ONE, GRAPHICS_BLEND_ZERO);
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,-1,0}, &(Vector2f){0,0},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,+1,0}, &(Vector2f){0,1},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,+1,0}, &(Vector2f){1,1},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,-1,0}, &(Vector2f){1,0},
                                   NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    graphics_set_blend(GRAPHICS_BLEND_ADD,
                       GRAPHICS_BLEND_SRC_ALPHA, GRAPHICS_BLEND_INV_SRC_ALPHA);
    uint8_t readbuf[4*4*4];
    CHECK_TRUE(graphics_read_pixels(0, 0, 4, 4, readbuf));
    for (int i = 0; i < 4*4*4; i += 4) {
        const int x = (i/4) % 4;
        const int y = (i/4) / 4;
        const int r = tex_4x4_psp_palette8_rgba8888[i+60];
        const int g = tex_4x4_psp_palette8_rgba8888[i+61];
        const int b = tex_4x4_psp_palette8_rgba8888[i+62];
        CHECK_PIXEL(&readbuf[i], r,g,b,255, x, y);
    }

    SysTexture *systex = texture_lock_raw(texture);
    ASSERT((void *)systex == (void *)data);
    uint8_t *pixels = psp_texture_get_pixel_data(systex);
    CHECK_PTREQUAL(pixels, data + 0x440);
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            pixels[y*16+x] = y*4+x;
        }
    }
    texture_unlock(texture);

    graphics_set_blend(GRAPHICS_BLEND_ADD,
                       GRAPHICS_BLEND_ONE, GRAPHICS_BLEND_ZERO);
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,-1,0}, &(Vector2f){0,0},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,+1,0}, &(Vector2f){0,1},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,+1,0}, &(Vector2f){1,1},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,-1,0}, &(Vector2f){1,0},
                                   NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    graphics_set_blend(GRAPHICS_BLEND_ADD,
                       GRAPHICS_BLEND_SRC_ALPHA, GRAPHICS_BLEND_INV_SRC_ALPHA);
    CHECK_TRUE(graphics_read_pixels(0, 0, 4, 4, readbuf));
    for (int i = 0; i < 4*4*4; i += 4) {
        const int x = (i/4) % 4;
        const int y = (i/4) / 4;
        const int r = tex_4x4_psp_palette8_rgba8888[i+64];
        const int g = tex_4x4_psp_palette8_rgba8888[i+65];
        const int b = tex_4x4_psp_palette8_rgba8888[i+66];
        CHECK_PIXEL(&readbuf[i], r,g,b,255, x, y);
    }

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_palette)
{
    uint8_t *data;
    ASSERT(data = mem_alloc(sizeof(tex_4x4_psp_palette8_rgba8888), 64, 0));
    memcpy(data, tex_4x4_psp_palette8_rgba8888,
           sizeof(tex_4x4_psp_palette8_rgba8888));

    int texture;
    ASSERT(texture = texture_parse((void *)data,
                                   sizeof(tex_4x4_psp_palette8_rgba8888),
                                   0, 0, 1));
    SysTexture *systex = texture_lock_raw(texture);
    ASSERT((void *)systex == (void *)data);

    CHECK_PTREQUAL(psp_texture_get_palette(systex), data + 64);

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_palette)
{
    uint8_t *data;
    ASSERT(data = mem_alloc(sizeof(tex_4x4_psp_palette8_rgba8888), 64, 0));
    memcpy(data, tex_4x4_psp_palette8_rgba8888,
           sizeof(tex_4x4_psp_palette8_rgba8888));

    int texture;
    ASSERT(texture = texture_parse((void *)data,
                                   sizeof(tex_4x4_psp_palette8_rgba8888),
                                   0, 0, 1));
    SysTexture *systex = texture_lock_raw(texture);
    ASSERT((void *)systex == (void *)data);

    uint8_t *dummy_palette;
    ASSERT(dummy_palette = mem_alloc(256*4, 64, 0));
    psp_texture_set_palette(systex, dummy_palette);
    CHECK_PTREQUAL(psp_texture_get_palette(systex), dummy_palette);

    psp_texture_set_palette(systex, NULL);
    CHECK_PTREQUAL(psp_texture_get_palette(systex), data + 64);

    mem_free(dummy_palette);
    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_palette_while_loaded)
{
    uint8_t *data;
    ASSERT(data = mem_alloc(sizeof(tex_4x4_psp_palette8_rgba8888), 64, 0));
    memcpy(data, tex_4x4_psp_palette8_rgba8888,
           sizeof(tex_4x4_psp_palette8_rgba8888));
    int texture;
    ASSERT(texture = texture_parse((void *)data,
                                   sizeof(tex_4x4_psp_palette8_rgba8888),
                                   0, 0, 1));

    texture_set_antialias(texture, 0);
    texture_apply(0, texture);

    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_set_viewport(0, 0, 4, 4);
    graphics_set_blend(GRAPHICS_BLEND_ADD,
                       GRAPHICS_BLEND_ONE, GRAPHICS_BLEND_ZERO);
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,-1,0}, &(Vector2f){0,0},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,+1,0}, &(Vector2f){0,1},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,+1,0}, &(Vector2f){1,1},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,-1,0}, &(Vector2f){1,0},
                                   NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    graphics_set_blend(GRAPHICS_BLEND_ADD,
                       GRAPHICS_BLEND_SRC_ALPHA, GRAPHICS_BLEND_INV_SRC_ALPHA);
    uint8_t readbuf[4*4*4];
    CHECK_TRUE(graphics_read_pixels(0, 0, 4, 4, readbuf));
    for (int i = 0; i < 4*4*4; i += 4) {
        const int x = (i/4) % 4;
        const int y = (i/4) / 4;
        const int r = tex_4x4_psp_palette8_rgba8888[i+60];
        const int g = tex_4x4_psp_palette8_rgba8888[i+61];
        const int b = tex_4x4_psp_palette8_rgba8888[i+62];
        CHECK_PIXEL(&readbuf[i], r,g,b,255, x, y);
    }

    uint8_t *new_palette;
    ASSERT(new_palette = mem_alloc(256*4, 64, 0));
    for (int i = 0; i < 16; i++) {
        new_palette[i*4+0] = 0;
        new_palette[i*4+1] = 0;
        new_palette[i*4+2] = ((i+1)%16) * 4;
        new_palette[i*4+3] = 255;
    }
    SysTexture *systex = texture_lock_raw(texture);
    ASSERT((void *)systex == (void *)data);
    psp_texture_set_palette(systex, new_palette);
    CHECK_PTREQUAL(psp_texture_get_palette(systex), new_palette);
    texture_unlock(texture);

    graphics_set_blend(GRAPHICS_BLEND_ADD,
                       GRAPHICS_BLEND_ONE, GRAPHICS_BLEND_ZERO);
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,-1,0}, &(Vector2f){0,0},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,+1,0}, &(Vector2f){0,1},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,+1,0}, &(Vector2f){1,1},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,-1,0}, &(Vector2f){1,0},
                                   NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    graphics_set_blend(GRAPHICS_BLEND_ADD,
                       GRAPHICS_BLEND_SRC_ALPHA, GRAPHICS_BLEND_INV_SRC_ALPHA);
    CHECK_TRUE(graphics_read_pixels(0, 0, 4, 4, readbuf));
    for (int i = 0; i < 4*4*4; i += 4) {
        const int x = (i/4) % 4;
        const int y = (i/4) / 4;
        CHECK_PIXEL(&readbuf[i], 0,0,i,255, x, y);
    }

    texture_destroy(texture);
    mem_free(new_palette);
    return 1;
}

/*************************************************************************/
/*********** Test routines: sys_texture_create() special cases ***********/
/*************************************************************************/

TEST(test_create_swizzle_rgba8888)
{
    uint8_t data[16][16][4];
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            data[y][x][0] = x<<4;
            data[y][x][1] = y<<4;
            data[y][x][2] = x<<4 | y;
            data[y][x][3] = 255;
        }
    }
    SysTexture *systex;
    CHECK_TRUE(systex = sys_texture_create(
                   16, 16, TEX_FORMAT_RGBA8888, 1, data, lenof(data[0]),
                   (int32_t[]){0}, (int32_t[]){sizeof(data)}, 0, 0, 0));

    int texture;
    ASSERT(texture = texture_import(systex, 0));
    texture_set_antialias(texture, 0);
    texture_apply(0, texture);

    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_set_viewport(0, 0, 16, 16);
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,-1,0}, &(Vector2f){0,0},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,+1,0}, &(Vector2f){0,1},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,+1,0}, &(Vector2f){1,1},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,-1,0}, &(Vector2f){1,0},
                                   NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    uint8_t readbuf[16*16*4];
    CHECK_TRUE(graphics_read_pixels(0, 0, 16, 16, readbuf));
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            const int r = data[y][x][0];
            const int g = data[y][x][1];
            const int b = data[y][x][2];
            CHECK_PIXEL(&readbuf[(y*16+x)*4], r,g,b,0xFF, x, y);
        }
    }

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_swizzle_rgb565)
{
    uint16_t data[16][32];
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 32; x++) {
            const int r = x;
            const int g = (x>>2)<<3 | y>>1;
            const int b = y<<2;
            data[y][x] = b<<11 | g<<5 | r;
        }
    }
    SysTexture *systex;
    CHECK_TRUE(systex = sys_texture_create(
                   32, 16, TEX_FORMAT_RGB565, 1, data, lenof(data[0]),
                   (int32_t[]){0}, (int32_t[]){sizeof(data)}, 0, 0, 0));

    int texture;
    ASSERT(texture = texture_import(systex, 0));
    texture_set_antialias(texture, 0);
    texture_apply(0, texture);

    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_set_viewport(0, 0, 32, 16);
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,-1,0}, &(Vector2f){0,0},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,+1,0}, &(Vector2f){0,1},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,+1,0}, &(Vector2f){1,1},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,-1,0}, &(Vector2f){1,0},
                                   NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    uint8_t readbuf[32*16*4];
    CHECK_TRUE(graphics_read_pixels(0, 0, 32, 16, readbuf));
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 32; x++) {
            const int r = ((data[y][x] & 0x1F) << 3) + 4;
            const int g = ((data[y][x]>>5 & 0x3F) << 2) + 2;
            const int b = ((data[y][x]>>11 & 0x1F) << 3) + 4;
            CHECK_PIXEL_NEAR(&readbuf[(y*32+x)*4], r,g,b,0xFF, 4, x, y);
        }
    }

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_swizzle_l8)
{
    uint8_t data[16][64];
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 64; x++) {
            data[y][x] = ((x&15) ^ (x>>4)) << 4 | y;
        }
    }
    SysTexture *systex;
    CHECK_TRUE(systex = sys_texture_create(
                   64, 16, TEX_FORMAT_L8, 1, data, lenof(data[0]),
                   (int32_t[]){0}, (int32_t[]){sizeof(data)}, 0, 0, 0));

    int texture;
    ASSERT(texture = texture_import(systex, 0));
    texture_set_antialias(texture, 0);
    texture_apply(0, texture);

    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_set_viewport(0, 0, 64, 16);
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,-1,0}, &(Vector2f){0,0},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,+1,0}, &(Vector2f){0,1},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,+1,0}, &(Vector2f){1,1},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,-1,0}, &(Vector2f){1,0},
                                   NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    uint8_t readbuf[64*16*4];
    CHECK_TRUE(graphics_read_pixels(0, 0, 64, 16, readbuf));
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 64; x++) {
            const int l = data[y][x];
            CHECK_PIXEL(&readbuf[(y*64+x)*4], l,l,l,0xFF, x, y);
        }
    }

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_swizzle_mipmaps)
{
    struct {
        uint8_t l0[8][8][4];
        uint8_t l1[4][4][4];
        uint8_t l2[2][2][4];
        uint8_t l3[1][1][4];
    } data;
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            data.l0[y][x][0] = x<<4 | y;
            data.l0[y][x][1] = 0;
            data.l0[y][x][2] = 0;
            data.l0[y][x][3] = 255;
        }
    }
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            data.l1[y][x][0] = 0;
            data.l1[y][x][1] = x<<5 | y<<1 | 0x11;
            data.l1[y][x][2] = 0;
            data.l1[y][x][3] = 255;
        }
    }
    for (int y = 0; y < 2; y++) {
        for (int x = 0; x < 2; x++) {
            data.l2[y][x][0] = 0;
            data.l2[y][x][1] = 0;
            data.l2[y][x][2] = x<<6 | y<<2 | 0x22;
            data.l2[y][x][3] = 255;
        }
    }
    data.l3[0][0][0] = 51;
    data.l3[0][0][1] = 102;
    data.l3[0][0][2] = 153;
    data.l3[0][0][3] = 255;
    SysTexture *systex;
    CHECK_TRUE(systex = sys_texture_create(
                   8, 8, TEX_FORMAT_RGBA8888, 4, &data, lenof(data.l0[0]),
                   (int32_t[]){0, sizeof(data.l0),
                               sizeof(data.l0) + sizeof(data.l1),
                               sizeof(data.l0) + sizeof(data.l1) + sizeof(data.l2)},
                   (int32_t[]){sizeof(data.l0), sizeof(data.l1),
                               sizeof(data.l2), sizeof(data.l3)},
                   0, 0, 0));

    int texture;
    ASSERT(texture = texture_import(systex, 0));
    texture_set_antialias(texture, 0);
    texture_apply(0, texture);

    for (int level = 0; level < 4; level++) {
        graphics_clear(0, 0, 0, 0, 1, 0);
        const int size = 8 >> level;
        graphics_set_viewport(0, 0, size, size);
        graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
        if (level == 3) {
            /* The GE seems to choke on drawing to a single-pixel viewport. */
            graphics_set_viewport(0, 0, 2, 2);
            CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,-1,0},
                                           &(Vector2f){0,0}, NULL));
            CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,+0,0},
                                           &(Vector2f){0,1}, NULL));
            CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0,+0,0},
                                           &(Vector2f){1,1}, NULL));
            CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0,-1,0},
                                           &(Vector2f){1,0}, NULL));
        } else {
            CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,-1,0},
                                           &(Vector2f){0,0}, NULL));
            CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,+1,0},
                                           &(Vector2f){0,1}, NULL));
            CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,+1,0},
                                           &(Vector2f){1,1}, NULL));
            CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,-1,0},
                                           &(Vector2f){1,0}, NULL));
        }
        CHECK_TRUE(graphics_end_and_draw_primitive());
        uint8_t readbuf[8*8*4];
        CHECK_TRUE(graphics_read_pixels(0, 0, size, size, readbuf));
        const uint8_t *src = (level==0 ? (void *)data.l0 :
                              level==1 ? (void *)data.l1 :
                              level==2 ? (void *)data.l2 : (void *)data.l3);
        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                const int r = src[(y*size+x)*4+0];
                const int g = src[(y*size+x)*4+1];
                const int b = src[(y*size+x)*4+2];
                /* "100*level" in the X coordinate is to get the level into
                 * the failure message without having to write the test
                 * manually. */
                CHECK_PIXEL(&readbuf[(y*size+x)*4], r,g,b,0xFF,
                            100*level+x, y);
            }
        }
    }

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_too_many_mipmaps)
{
    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(256*256*4/3, 0, 0));
    int32_t offsets[9], sizes[9];
    for (int level = 0, offset = 0, size = 256; level < 9;
         level++, offset += size*size, size /= 2)
    {
        offsets[level] = offset;
        sizes[level] = size*size;
        memset(pixels+offset, (level+1)*16, size*size);
    }
    SysTexture *systex;
    CHECK_TRUE(systex = sys_texture_create(
                   256, 256, TEX_FORMAT_L8, 9, pixels, 256, offsets, sizes,
                   0, 0, 0));
    mem_free(pixels);

    int texture;
    ASSERT(texture = texture_import(systex, 0));
    texture_set_antialias(texture, 0);
    texture_apply(0, texture);

    graphics_clear(0, 0, 0, 0, 1, 0);
    /* Going all the way down to a 1x1 viewport seems to confuse the GE and
     * cause it to not draw anything at all, so stick with 2x2 and double
     * the texture coordinates so we're still scaling by a factor of 256. */
    graphics_set_viewport(0, 0, 2, 2);
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,-1,0}, &(Vector2f){0,0},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,+1,0}, &(Vector2f){0,2},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,+1,0}, &(Vector2f){2,2},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,-1,0}, &(Vector2f){2,0},
                                   NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    uint8_t readbuf[2*2*4];
    CHECK_TRUE(graphics_read_pixels(0, 0, 2, 2, readbuf));
    CHECK_PIXEL(&readbuf[ 0], 0x80,0x80,0x80,0xFF, 0, 0);
    CHECK_PIXEL(&readbuf[ 4], 0x80,0x80,0x80,0xFF, 1, 0);
    CHECK_PIXEL(&readbuf[ 8], 0x80,0x80,0x80,0xFF, 0, 1);
    CHECK_PIXEL(&readbuf[12], 0x80,0x80,0x80,0xFF, 1, 1);

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_reuse_unaligned_format_bad_stride)
{
    const void *rgba = tex_4x4_psp_palette8_rgba8888 + 64;
    void *pixels;
    ASSERT(pixels = mem_alloc(64, 64, 0));
    memcpy(pixels, rgba, 64);

    SysTexture *texture;
    CHECK_TRUE(texture = sys_texture_create(
                   2, 8, TEX_FORMAT_RGBA8888, 1, pixels, 2, (int32_t[]){0},
                   (int32_t[]){64}, 0, 0, 1));
    CHECK_FALSE(texture == pixels);

    CHECK_TRUE(pixels = sys_texture_lock(
                   texture, SYS_TEXTURE_LOCK_NORMAL, 0, 0, 2, 8));
    CHECK_MEMEQUAL(pixels, rgba, 64);
    sys_texture_unlock(texture, 0);

    sys_texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_reuse_unaligned_format_mipmaps)
{
    const void *rgba = tex_4x4_psp_palette8_rgba8888 + 64;
    void *pixels;
    ASSERT(pixels = mem_alloc(80, 64, 0));
    memcpy(pixels, rgba, 64);

    SysTexture *texture;
    CHECK_TRUE(texture = sys_texture_create(
                   4, 4, TEX_FORMAT_RGBA8888, 2, pixels, 4, (int32_t[]){0, 64},
                   (int32_t[]){64, 16}, 0, 0, 1));
    CHECK_FALSE(texture == pixels);

    CHECK_TRUE(pixels = sys_texture_lock(
                   texture, SYS_TEXTURE_LOCK_NORMAL, 0, 0, 4, 4));
    CHECK_MEMEQUAL(pixels, rgba, 64);
    sys_texture_unlock(texture, 0);

    sys_texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* Check that a PALETTE8 texture with no initial data gets the luminance
 * (not alpha) palette. */
TEST(test_create_8bpp_no_data)
{
    SysTexture *systex;
    CHECK_TRUE(systex = sys_texture_create(
                   2, 2, TEX_FORMAT_PALETTE8_RGBA8888, 0, NULL, 0, NULL, NULL,
                   0, 0, 0));

    uint8_t *pixels;
    CHECK_TRUE(pixels = psp_texture_get_pixel_data(systex));
    pixels[0] = 0x10;
    pixels[1] = 0x20;
    pixels[16] = 0x30;
    pixels[17] = 0x40;
    sceKernelDcacheWritebackRange(pixels, 32);

    int texture;
    ASSERT(texture = texture_import(systex, 0));
    texture_set_antialias(texture, 0);
    texture_apply(0, texture);

    graphics_clear(0, 0, 1, 0, 1, 0);  // So we can distinguish luminance and alpha.
    graphics_set_viewport(0, 0, 2, 2);
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,-1,0}, &(Vector2f){0,0},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,+1,0}, &(Vector2f){0,1},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,+1,0}, &(Vector2f){1,1},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,-1,0}, &(Vector2f){1,0},
                                   NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    uint8_t readbuf[2*2*4];
    CHECK_TRUE(graphics_read_pixels(0, 0, 2, 2, readbuf));
    CHECK_PIXEL(&readbuf[ 0], 0x10,0x10,0x10,0xFF, 0, 0);
    CHECK_PIXEL(&readbuf[ 4], 0x20,0x20,0x20,0xFF, 1, 0);
    CHECK_PIXEL(&readbuf[ 8], 0x30,0x30,0x30,0xFF, 0, 1);
    CHECK_PIXEL(&readbuf[12], 0x40,0x40,0x40,0xFF, 1, 1);

    texture_destroy(texture);
    return 1;
}

/*************************************************************************/
/************ Test routines: sys_texture_lock() special cases ************/
/*************************************************************************/

TEST(test_lock_16bpp_full_stride)
{
    int texture;
    /* Safe to de-const the input buffer since we're not reusing it. */
    CHECK_TRUE(texture = texture_parse((void *)tex_8x4_psp_rgb565,
                                       sizeof(tex_8x4_psp_rgb565), 0, 0, 0));

    const uint8_t *pixels;
    CHECK_TRUE(pixels = texture_lock_readonly(texture));
    for (int i = 0; i < 8*4*4; i += 4) {
        const int x = (i/4) % 8;
        const int y = (i/4) / 8;
        const int r = rgba_8x4_rgb565[i+0];
        const int g = rgba_8x4_rgb565[i+1];
        const int b = rgba_8x4_rgb565[i+2];
        const int a = rgba_8x4_rgb565[i+3];
        CHECK_PIXEL_NEAR(&pixels[i], r,g,b,a, 1, x,y);
    }

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_lock_8bpp_full_stride)
{
    int texture;
    /* Safe to de-const the input buffer since we're not reusing it. */
    CHECK_TRUE(texture = texture_parse((void *)tex_16x4_psp_alpha,
                                       sizeof(tex_16x4_psp_alpha), 0, 0, 0));

    const uint8_t *pixels;
    CHECK_TRUE(pixels = texture_lock_readonly(texture));
    for (int i = 0; i < 16*4*4; i += 4) {
        const int x = (i/4) % 16;
        const int y = (i/4) / 16;
        const int r = rgba_16x4_alpha[i+0];
        const int g = rgba_16x4_alpha[i+1];
        const int b = rgba_16x4_alpha[i+2];
        const int a = rgba_16x4_alpha[i+3];
        CHECK_PIXEL(&pixels[i], r,g,b,a, x,y);
    }

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_lock_deswizzle_partial_full_width)
{
    int texture;
    ASSERT(texture = texture_create_with_data(
               8, 8, tex_4x4_psp_palette8_rgba8888 + 64,
               TEX_FORMAT_PSP_RGBA8888_SWIZZLED, 8, 0, 0));

    const uint8_t *pixels;
    CHECK_TRUE(pixels = texture_lock_readonly_partial(texture, 0, 0, 8, 4));
    for (int i = 0; i < 8*4*4; i += 4) {
        const int x = (i/4) % 8;
        const int y = (i/4) / 8;
        int r, g, b, a;
        if (x < 4) {
            const int index = (y*4 + x) * 4;
            r = tex_4x4_psp_palette8_rgba8888[64 + index + 0];
            g = tex_4x4_psp_palette8_rgba8888[64 + index + 1];
            b = tex_4x4_psp_palette8_rgba8888[64 + index + 2];
            a = tex_4x4_psp_palette8_rgba8888[64 + index + 3];
        } else {
            r = g = b = a = 0;
        }
        CHECK_PIXEL(&pixels[i], r,g,b,a, x,y);
    }

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_lock_deswizzle_partial_nonfull_width)
{
    int texture;
    ASSERT(texture = texture_create_with_data(
               8, 8, tex_4x4_psp_palette8_rgba8888 + 64,
               TEX_FORMAT_PSP_RGBA8888_SWIZZLED, 8, 0, 0));

    const uint8_t *pixels;
    CHECK_TRUE(pixels = texture_lock_readonly_partial(texture, 0, 0, 4, 4));
    for (int i = 0; i < 4*4*4; i += 4) {
        const int x = (i/4) % 4;
        const int y = (i/4) / 4;
        const int index = (y*4 + x) * 4;
        const int r = tex_4x4_psp_palette8_rgba8888[64 + index + 0];
        const int g = tex_4x4_psp_palette8_rgba8888[64 + index + 1];
        const int b = tex_4x4_psp_palette8_rgba8888[64 + index + 2];
        const int a = tex_4x4_psp_palette8_rgba8888[64 + index + 3];
        CHECK_PIXEL(&pixels[i], r,g,b,a, x,y);
    }

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_lock_deswizzle_partial_memory_failure)
{
    int texture;
    ASSERT(texture = texture_create_with_data(
               8, 8, tex_4x4_psp_palette8_rgba8888 + 64,
               TEX_FORMAT_PSP_RGBA8888_SWIZZLED, 8, 0, 0));

    const uint8_t *pixels;
    CHECK_MEMORY_FAILURES(
        pixels = texture_lock_readonly_partial(texture, 0, 0, 4, 4));
    for (int i = 0; i < 4*4*4; i += 4) {
        const int x = (i/4) % 4;
        const int y = (i/4) / 4;
        const int index = (y*4 + x) * 4;
        const int r = tex_4x4_psp_palette8_rgba8888[64 + index + 0];
        const int g = tex_4x4_psp_palette8_rgba8888[64 + index + 1];
        const int b = tex_4x4_psp_palette8_rgba8888[64 + index + 2];
        const int a = tex_4x4_psp_palette8_rgba8888[64 + index + 3];
        CHECK_PIXEL(&pixels[i], r,g,b,a, x,y);
    }

    texture_destroy(texture);
    return 1;
}

/*************************************************************************/
/********************* Test routines: VRAM textures **********************/
/*************************************************************************/

TEST(test_create_vram)
{
    SysTexture *systex;
    CHECK_TRUE(systex = psp_create_vram_texture(4, 4));

    int texture;
    ASSERT(texture = texture_import(systex, 0));
    texture_set_antialias(texture, 0);
    texture_apply(0, texture);

    uint8_t *pixels;
    CHECK_TRUE(pixels = texture_lock_writeonly(texture));
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            pixels[(y*4+x)*4+0] = x*16;
            pixels[(y*4+x)*4+1] = y*16;
            pixels[(y*4+x)*4+2] = (x+y)*16;
            pixels[(y*4+x)*4+3] = 255;
        }
    }
    texture_unlock(texture);

    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_set_viewport(0, 0, 4, 4);
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,-1,0}, &(Vector2f){0,0},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,+1,0}, &(Vector2f){0,1},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,+1,0}, &(Vector2f){1,1},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,-1,0}, &(Vector2f){1,0},
                                   NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    uint8_t readbuf[4*4*4];
    CHECK_TRUE(graphics_read_pixels(0, 0, 4, 4, readbuf));
    for (int i = 0; i < 4*4*4; i += 4) {
        const int x = (i/4) % 4;
        const int y = (i/4) / 4;
        CHECK_PIXEL(&pixels[i], x*16,y*16,(x+y)*16,255, x, y);
    }

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_vram_memory_failure)
{
    /* Pre-expand the VRAM block list to avoid bogus memory leak errors. */
    void *ptr;
    ASSERT(ptr = psp_vram_alloc(1, 0));
    psp_vram_free(ptr);

    SysTexture *systex;
    CHECK_MEMORY_FAILURES(systex = psp_create_vram_texture(4, 4));

    int texture;
    ASSERT(texture = texture_import(systex, 0));
    texture_set_antialias(texture, 0);
    texture_apply(0, texture);

    uint8_t *pixels;
    CHECK_TRUE(pixels = texture_lock_writeonly(texture));
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            pixels[(y*4+x)*4+0] = x*16;
            pixels[(y*4+x)*4+1] = y*16;
            pixels[(y*4+x)*4+2] = (x+y)*16;
            pixels[(y*4+x)*4+3] = 255;
        }
    }
    texture_unlock(texture);

    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_set_viewport(0, 0, 4, 4);
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,-1,0}, &(Vector2f){0,0},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,+1,0}, &(Vector2f){0,1},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,+1,0}, &(Vector2f){1,1},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,-1,0}, &(Vector2f){1,0},
                                   NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    uint8_t readbuf[4*4*4];
    CHECK_TRUE(graphics_read_pixels(0, 0, 4, 4, readbuf));
    for (int i = 0; i < 4*4*4; i += 4) {
        const int x = (i/4) % 4;
        const int y = (i/4) / 4;
        CHECK_PIXEL(&pixels[i], x*16,y*16,(x+y)*16,255, x, y);
    }

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_vram_vram_full)
{
    void *ptr;
    ASSERT(ptr = psp_vram_alloc(0x200000 - 0x154000, 0));
    ASSERT(!psp_vram_alloc(1, 0));

    CHECK_FALSE(psp_create_vram_texture(4, 4));

    psp_vram_free(ptr);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_vram_invalid)
{
    CHECK_FALSE(psp_create_vram_texture(0, 4));
    CHECK_FALSE(psp_create_vram_texture(-1, 4));
    CHECK_FALSE(psp_create_vram_texture(4, 0));
    CHECK_FALSE(psp_create_vram_texture(4, -1));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_destroy_vram_immediate)
{
    graphics_finish_frame();
    sceDisplayWaitVblank();

    SysTexture *systex;
    CHECK_TRUE(systex = psp_create_vram_texture(4, 4));
    sys_texture_destroy(systex);

    /* Verify that the VRAM was freed. */
    void *ptr;
    CHECK_TRUE(ptr = psp_vram_alloc(0x200000 - 0x154000, 0));
    ASSERT(!psp_vram_alloc(1, 0));
    psp_vram_free(ptr);

    graphics_start_frame();
    return 1;
}

/*************************************************************************/
/********************* Test routines: Tall textures **********************/
/*************************************************************************/

TEST(test_tall_texture)
{
    int texture;
    CHECK_TRUE(texture = texture_create(1, 1024, 0, 0));
    uint8_t *pixels;
    CHECK_TRUE(pixels = texture_lock(texture));
    for (int y = 0; y < 512; y++) {
        pixels[y*4+0] = 0;
        pixels[y*4+1] = lbound(y-256, 0);
        pixels[y*4+2] = ubound(y, 255);
        pixels[y*4+3] = 255;
        pixels[(512+y)*4+0] = 255;
        pixels[(512+y)*4+1] = lbound(y-256, 0);
        pixels[(512+y)*4+2] = ubound(y, 255);
        pixels[(512+y)*4+3] = 255;
    }
    texture_unlock(texture);
    texture_set_repeat(texture, 0, 0);
    texture_set_antialias(texture, 0);
    texture_apply(0, texture);

    uint8_t readbuf[4*4*4];
    graphics_set_viewport(0, 0, 4, 4);

    graphics_clear(0, 1, 0, 0, 1, 0);
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    graphics_add_vertex(
        &(Vector3f){-1,-1,0}, &(Vector2f){0, 508/1024.0f}, NULL);
    graphics_add_vertex(
        &(Vector3f){+1,-1,0}, &(Vector2f){1, 508/1024.0f}, NULL);
    graphics_add_vertex(
        &(Vector3f){+1,+1,0}, &(Vector2f){1, 512/1024.0f}, NULL);
    graphics_add_vertex(
        &(Vector3f){-1,+1,0}, &(Vector2f){0, 512/1024.0f}, NULL);
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_TRUE(graphics_read_pixels(0, 0, 4, 4, readbuf));
    for (int i = 0; i < lenof(readbuf); i += 4) {
        const int x = (i/4)%4, y = (i/4)/4;
        CHECK_PIXEL(&readbuf[i], 0,252+y,255,255, x,y);
    }

    graphics_clear(0, 1, 0, 0, 1, 0);
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    graphics_add_vertex(
        &(Vector3f){-1,-1,0}, &(Vector2f){0, 512/1024.0f}, NULL);
    graphics_add_vertex(
        &(Vector3f){+1,-1,0}, &(Vector2f){1, 512/1024.0f}, NULL);
    graphics_add_vertex(
        &(Vector3f){+1,+1,0}, &(Vector2f){1, 516/1024.0f}, NULL);
    graphics_add_vertex(
        &(Vector3f){-1,+1,0}, &(Vector2f){0, 516/1024.0f}, NULL);
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_TRUE(graphics_read_pixels(0, 0, 4, 4, readbuf));
    for (int i = 0; i < lenof(readbuf); i += 4) {
        const int x = (i/4)%4, y = (i/4)/4;
        CHECK_PIXEL(&readbuf[i], 255,0,y,255, x,y);
    }

    graphics_clear(0, 1, 0, 0, 1, 0);
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    graphics_add_vertex(
        &(Vector3f){-1,-1,0}, &(Vector2f){0, 508/1024.0f}, NULL);
    graphics_add_vertex(
        &(Vector3f){ 0,-1,0}, &(Vector2f){1, 508/1024.0f}, NULL);
    graphics_add_vertex(
        &(Vector3f){ 0,+1,0}, &(Vector2f){1, 512/1024.0f}, NULL);
    graphics_add_vertex(
        &(Vector3f){-1,+1,0}, &(Vector2f){0, 512/1024.0f}, NULL);
    graphics_add_vertex(
        &(Vector3f){ 0,-1,0}, &(Vector2f){0, 512/1024.0f}, NULL);
    graphics_add_vertex(
        &(Vector3f){+1,-1,0}, &(Vector2f){1, 512/1024.0f}, NULL);
    graphics_add_vertex(
        &(Vector3f){+1,+1,0}, &(Vector2f){1, 516/1024.0f}, NULL);
    graphics_add_vertex(
        &(Vector3f){ 0,+1,0}, &(Vector2f){0, 516/1024.0f}, NULL);
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_TRUE(graphics_read_pixels(0, 0, 4, 4, readbuf));
    for (int i = 0; i < lenof(readbuf); i += 4) {
        const int x = (i/4)%4, y = (i/4)/4;
        if (x < 2) {
            CHECK_PIXEL(&readbuf[i], 0,252+y,255,255, x,y);
        } else {
            CHECK_PIXEL(&readbuf[i], 255,0,y,255, x,y);
        }
    }

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_tall_texture_indexed)
{
    int texture;
    CHECK_TRUE(texture = texture_create(512, 1024, 0, 0));
    uint8_t *pixels;
    CHECK_TRUE(pixels = texture_lock(texture));
    mem_fill32(pixels,           0xFFFF0000, 512*512*4);
    mem_fill32(pixels+512*512*4, 0xFF00FF00, 512*512*4);
    texture_unlock(texture);
    texture_set_repeat(texture, 0, 0);
    texture_set_antialias(texture, 0);
    texture_apply(0, texture);

    uint8_t readbuf[4*4*4];
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_set_viewport(0, 0, 4, 4);
    static const struct {Vector2f pos; Vector2f tex;} vertices[] = {
        {{-1,-1}, {0/512.0f, 508/1024.0f}},
        {{-1,-1}, {0/512.0f, 512/1024.0f}},
        {{+1,-1}, {4/512.0f, 508/1024.0f}},
        {{+1,-1}, {4/512.0f, 512/1024.0f}},
        {{+1,+1}, {4/512.0f, 516/1024.0f}},
        {{+1,+1}, {4/512.0f, 512/1024.0f}},
        {{-1,+1}, {0/512.0f, 516/1024.0f}},
        {{-1,+1}, {0/512.0f, 512/1024.0f}},
    };
    static const uint32_t format[] = {
        GRAPHICS_VERTEX_FORMAT(POSITION_2F, 0),
        GRAPHICS_VERTEX_FORMAT(TEXCOORD_2F, sizeof(Vector2f)),
        0
    };

    static const uint8_t indices8[] = {1,3,4,6};
    graphics_draw_indexed_vertices(
        GRAPHICS_PRIMITIVE_QUADS,
        vertices, format, sizeof(*vertices), lenof(vertices),
        indices8, sizeof(*indices8), lenof(indices8));
    CHECK_TRUE(graphics_read_pixels(0, 0, 4, 4, readbuf));
    for (int i = 0; i < lenof(readbuf); i += 4) {
        const int x = (i/4)%4, y = (i/4)/4;
        CHECK_PIXEL(&readbuf[i], 0,255,0,255, x,y);
    }

    static const uint16_t indices16[] = {0,2,5,7};
    graphics_draw_indexed_vertices(
        GRAPHICS_PRIMITIVE_QUADS,
        vertices, format, sizeof(*vertices), lenof(vertices),
        indices16, sizeof(*indices16), lenof(indices16));
    CHECK_TRUE(graphics_read_pixels(0, 0, 4, 4, readbuf));
    for (int i = 0; i < lenof(readbuf); i += 4) {
        const int x = (i/4)%4, y = (i/4)/4;
        CHECK_PIXEL(&readbuf[i], 0,0,255,255, x,y);
    }

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_tall_texture_8bpp)
{
    uint8_t *pixels = mem_alloc(256*4 + 512*1024, 0, 0);
    memset(pixels, 0, 256*4);
    memcpy(pixels, "\0\0\xFF\xFF\0\xFF\0\xFF", 8);
    memset(pixels + 256*4, 0, 512*512);
    memset(pixels + 256*4 + 512*512, 1, 512*512);
    int texture;
    CHECK_TRUE(texture = texture_create_with_data(
                   512, 1024, pixels, TEX_FORMAT_PALETTE8_RGBA8888, 512, 0, 0));
    mem_free(pixels);
    texture_set_repeat(texture, 0, 0);
    texture_set_antialias(texture, 0);
    texture_apply(0, texture);

    uint8_t readbuf[4*4*4];
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_set_viewport(0, 0, 4, 4);

    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    graphics_add_vertex(
        &(Vector3f){-1,-1,0}, &(Vector2f){0/512.0f, 508/1024.0f}, NULL);
    graphics_add_vertex(
        &(Vector3f){ 0,-1,0}, &(Vector2f){2/512.0f, 508/1024.0f}, NULL);
    graphics_add_vertex(
        &(Vector3f){ 0,+1,0}, &(Vector2f){2/512.0f, 512/1024.0f}, NULL);
    graphics_add_vertex(
        &(Vector3f){-1,+1,0}, &(Vector2f){0/512.0f, 512/1024.0f}, NULL);
    graphics_add_vertex(
        &(Vector3f){ 0,-1,0}, &(Vector2f){0/512.0f, 512/1024.0f}, NULL);
    graphics_add_vertex(
        &(Vector3f){+1,-1,0}, &(Vector2f){2/512.0f, 512/1024.0f}, NULL);
    graphics_add_vertex(
        &(Vector3f){+1,+1,0}, &(Vector2f){2/512.0f, 516/1024.0f}, NULL);
    graphics_add_vertex(
        &(Vector3f){ 0,+1,0}, &(Vector2f){0/512.0f, 516/1024.0f}, NULL);
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_TRUE(graphics_read_pixels(0, 0, 4, 4, readbuf));
    for (int i = 0; i < lenof(readbuf); i += 4) {
        const int x = (i/4)%4, y = (i/4)/4;
        if (x < 2) {
            CHECK_PIXEL(&readbuf[i], 0,0,255,255, x,y);
        } else {
            CHECK_PIXEL(&readbuf[i], 0,255,0,255, x,y);
        }
    }

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_tall_texture_apply_0)
{
    int texture;
    CHECK_TRUE(texture = texture_create(512, 1024, 0, 0));
    uint8_t *pixels;
    CHECK_TRUE(pixels = texture_lock(texture));
    mem_fill32(pixels,           0xFFFF0000, 512*512*4);
    mem_fill32(pixels+512*512*4, 0xFF00FF00, 512*512*4);
    texture_unlock(texture);
    texture_set_repeat(texture, 0, 0);
    texture_set_antialias(texture, 0);
    texture_apply(0, texture);

    uint8_t readbuf[4*4*4];
    graphics_set_viewport(0, 0, 4, 4);

    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    graphics_add_vertex(
        &(Vector3f){-1,-1,0}, &(Vector2f){0/512.0f, 508/1024.0f}, NULL);
    graphics_add_vertex(
        &(Vector3f){+1,-1,0}, &(Vector2f){4/512.0f, 508/1024.0f}, NULL);
    graphics_add_vertex(
        &(Vector3f){+1,+1,0}, &(Vector2f){4/512.0f, 512/1024.0f}, NULL);
    graphics_add_vertex(
        &(Vector3f){-1,+1,0}, &(Vector2f){0/512.0f, 512/1024.0f}, NULL);
    int primitive;
    CHECK_TRUE(primitive = graphics_end_primitive());

    /* Draw it once to do the texture coordinate adjustment. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_draw_primitive(primitive);

    /* Make sure a subsequent draw doesn't crash trying to choose
     * subtextures. */
    texture_apply(0, 0);
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_draw_primitive(primitive);
    CHECK_TRUE(graphics_read_pixels(0, 0, 4, 4, readbuf));
    for (int i = 0; i < lenof(readbuf); i += 4) {
        const int x = (i/4)%4, y = (i/4)/4;
        CHECK_PIXEL(&readbuf[i], 255,255,255,255, x,y);
    }

    graphics_destroy_primitive(primitive);
    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_tall_texture_apply_short)
{
    int texture;
    CHECK_TRUE(texture = texture_create(512, 1024, 0, 0));
    /* For this test, we won't actually try to read back any data, so we
     * don't need to initialize the texture. */

    int texture2;
    CHECK_TRUE(texture2 = texture_create(512, 1, 0, 0));

    graphics_set_viewport(0, 0, 4, 4);

    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    graphics_add_vertex(
        &(Vector3f){-1,-1,0}, &(Vector2f){0/512.0f, 512/1024.0f}, NULL);
    graphics_add_vertex(
        &(Vector3f){+1,-1,0}, &(Vector2f){4/512.0f, 512/1024.0f}, NULL);
    graphics_add_vertex(
        &(Vector3f){+1,+1,0}, &(Vector2f){4/512.0f, 516/1024.0f}, NULL);
    graphics_add_vertex(
        &(Vector3f){-1,+1,0}, &(Vector2f){0/512.0f, 516/1024.0f}, NULL);
    int primitive;
    CHECK_TRUE(primitive = graphics_end_primitive());

    /* Draw it once to do the texture coordinate adjustment. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    texture_apply(0, texture);
    graphics_draw_primitive(primitive);

    /* Make sure a subsequent draw with a short (height <= 512) texture
     * doesn't cause a crash, even though it violates the documented
     * requirements for tall texture rendering. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    texture_apply(0, texture2);
    graphics_draw_primitive(primitive);

    graphics_destroy_primitive(primitive);
    texture_destroy(texture2);
    texture_destroy(texture);
    return 1;
}

/*************************************************************************/
/********************* Test routines: Miscellaneous **********************/
/*************************************************************************/

TEST(test_delayed_delete)
{
    const int64_t bytes_used = mem_debug_bytes_allocated();

    int texture;
    ASSERT(texture = texture_create(4, 2, 0, 0));

    texture_destroy(texture);
    /* The memory should still be allocated so that the GE could draw from
     * it if the texture had been used in a drawing operation. */
    CHECK_TRUE(mem_debug_bytes_allocated() >= bytes_used + 4*2*4);

    /* A subsequent flush operation should free the texture. */
    graphics_flush_resources();
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), bytes_used);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_apply_invalid)
{
    int texture0, texture1;
    ASSERT(texture0 = texture_create_with_data(
               1, 1, "\0\xFF\0\xFF", TEX_FORMAT_RGBA8888, 1, 0, 0));
    ASSERT(texture1 = texture_create_with_data(
               1, 1, "\xFF\0\0\xFF", TEX_FORMAT_RGBA8888, 1, 0, 0));
    texture_apply(0, texture0);
    texture_apply(1, texture1);

    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_set_viewport(0, 0, 2, 2);
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,-1,0}, &(Vector2f){0,0},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,+1,0}, &(Vector2f){0,1},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,+1,0}, &(Vector2f){1,1},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,-1,0}, &(Vector2f){1,0},
                                   NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    uint8_t readbuf[2*2*4];
    CHECK_TRUE(graphics_read_pixels(0, 0, 2, 2, readbuf));
    CHECK_PIXEL(&readbuf[ 0], 0,255,0,255, 0, 0);
    CHECK_PIXEL(&readbuf[ 4], 0,255,0,255, 1, 0);
    CHECK_PIXEL(&readbuf[ 8], 0,255,0,255, 0, 1);
    CHECK_PIXEL(&readbuf[12], 0,255,0,255, 1, 1);

    texture_destroy(texture0);
    texture_destroy(texture1);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_apply_same_palette)
{
    int texture1, texture2;
    ASSERT(texture1 = texture_create_with_data(
               1, 1, "\x33", TEX_FORMAT_L8, 1, 0, 0));
    ASSERT(texture2 = texture_create_with_data(
               1, 1, "\x55", TEX_FORMAT_L8, 1, 0, 0));

    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_set_viewport(0, 0, 2, 2);
    texture_apply(0, texture1);
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,-1,0}, &(Vector2f){0,0},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,+1,0}, &(Vector2f){0,1},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,+1,0}, &(Vector2f){1,1},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,-1,0}, &(Vector2f){1,0},
                                   NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    uint8_t readbuf[2*2*4];
    CHECK_TRUE(graphics_read_pixels(0, 0, 2, 2, readbuf));
    CHECK_PIXEL(&readbuf[ 0], 51,51,51,255, 0, 0);
    CHECK_PIXEL(&readbuf[ 4], 51,51,51,255, 1, 0);
    CHECK_PIXEL(&readbuf[ 8], 51,51,51,255, 0, 1);
    CHECK_PIXEL(&readbuf[12], 51,51,51,255, 1, 1);

    /* Manually apply an alpha palette.  This should be used instead of
     * the correct luminance palette because psp_set_texture_state()
     * should see that the palette pointer is unchanged and skip loading
     * the palette. */
    uint8_t alpha_palette_buf[256*4+64];
    uint8_t *alpha_palette =
        (uint8_t *)align_up((uintptr_t)alpha_palette_buf, 64);
    for (int i = 0; i < 256; i++) {
        alpha_palette[i*4+0] = 255;
        alpha_palette[i*4+1] = 255;
        alpha_palette[i*4+2] = 255;
        alpha_palette[i*4+3] = i;
    }
    sceKernelDcacheWritebackRange(alpha_palette, 256*4);
    ge_set_colortable(alpha_palette, 256, GE_PIXFMT_8888, 0, 0xFF);

    texture_apply(0, texture2);
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,-1,0}, &(Vector2f){0,0},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,+1,0}, &(Vector2f){0,1},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,+1,0}, &(Vector2f){1,1},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,-1,0}, &(Vector2f){1,0},
                                   NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_TRUE(graphics_read_pixels(0, 0, 2, 2, readbuf));
    CHECK_PIXEL(&readbuf[ 0], 119,119,119,255, 0, 0);
    CHECK_PIXEL(&readbuf[ 4], 119,119,119,255, 1, 0);
    CHECK_PIXEL(&readbuf[ 8], 119,119,119,255, 0, 1);
    CHECK_PIXEL(&readbuf[12], 119,119,119,255, 1, 1);

    texture_destroy(texture1);
    texture_destroy(texture2);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_apply_change_scale_v)
{
    int texture1, texture2;
    uint8_t *pixels;
    ASSERT(texture1 = texture_create(4, 5, 0, 0));
    ASSERT(pixels = texture_lock_writeonly(texture1));
    for (int i = 0; i < 4*5; i++) {
        pixels[i*4+0] = (i%4)*16;
        pixels[i*4+1] = (i/4)*16;
        pixels[i*4+2] = 0;
        pixels[i*4+3] = 255;
    }
    texture_unlock(texture1);
    texture_set_antialias(texture1, 0);
    ASSERT(texture2 = texture_create(4, 4, 0, 0));
    ASSERT(pixels = texture_lock_writeonly(texture2));
    for (int i = 0; i < 4*4; i++) {
        pixels[i*4+0] = (i%4)*16;
        pixels[i*4+1] = (i/4)*16;
        pixels[i*4+2] = 0;
        pixels[i*4+3] = 255;
    }
    texture_unlock(texture2);
    texture_set_antialias(texture2, 0);

    texture_apply(0, texture1);
    graphics_clear(0, 0, 0, 0, 1, 0);
    /* The GE doesn't get things quite right with a 4x5 viewport, so we use
     * 4x8 instead and massage vertex coordinates appropriately.  We're not
     * too worried about this particular pattern working in all conceivable
     * cases -- we just want to verify that the texture coordinate scale
     * register has in fact been changed. */
    graphics_set_viewport(0, 0, 4, 8);
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,-1,0}, &(Vector2f){0,0},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,+0.25,0}, &(Vector2f){0,1},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,+0.25,0}, &(Vector2f){1,1},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,-1,0}, &(Vector2f){1,0},
                                   NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    uint8_t readbuf[4*5*4];
    CHECK_TRUE(graphics_read_pixels(0, 0, 4, 5, readbuf));
    for (int i = 0; i < 4*5*4; i += 4) {
        const int x = (i/4) % 4;
        const int y = (i/4) / 4;
        CHECK_PIXEL(&readbuf[i], x*16,y*16,0,255, x, y);
    }

    texture_apply(0, texture2);
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_set_viewport(0, 0, 4, 4);
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,-1,0}, &(Vector2f){0,0},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,+1,0}, &(Vector2f){0,1},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,+1,0}, &(Vector2f){1,1},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,-1,0}, &(Vector2f){1,0},
                                   NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_TRUE(graphics_read_pixels(0, 0, 4, 4, readbuf));
    for (int i = 0; i < 4*4*4; i += 4) {
        const int x = (i/4) % 4;
        const int y = (i/4) / 4;
        CHECK_PIXEL(&readbuf[i], x*16,y*16,0,255, x, y);
    }

    texture_destroy(texture1);
    texture_destroy(texture2);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_apply_change_repeat_v)
{
    int texture1, texture2;
    uint8_t *pixels;
    ASSERT(texture1 = texture_create(4, 4, 0, 0));
    ASSERT(pixels = texture_lock_writeonly(texture1));
    for (int i = 0; i < 4*4; i++) {
        pixels[i*4+0] = (i%4)*16;
        pixels[i*4+1] = (i/4)*16;
        pixels[i*4+2] = 0;
        pixels[i*4+3] = 255;
    }
    texture_unlock(texture1);
    texture_set_antialias(texture1, 0);
    texture_set_repeat(texture1, 0, 1);
    ASSERT(texture2 = texture_create(4, 4, 0, 0));
    ASSERT(pixels = texture_lock_writeonly(texture2));
    for (int i = 0; i < 4*4; i++) {
        pixels[i*4+0] = (i%4)*16;
        pixels[i*4+1] = (i/4)*16;
        pixels[i*4+2] = 0;
        pixels[i*4+3] = 255;
    }
    texture_unlock(texture2);
    texture_set_antialias(texture2, 0);
    texture_set_repeat(texture2, 0, 0);

    texture_apply(0, texture1);
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_set_viewport(0, 0, 4, 8);
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,-1,0}, &(Vector2f){0,0},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,+1,0}, &(Vector2f){0,2},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,+1,0}, &(Vector2f){1,2},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,-1,0}, &(Vector2f){1,0},
                                   NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    uint8_t readbuf[4*8*4];
    CHECK_TRUE(graphics_read_pixels(0, 0, 4, 8, readbuf));
    for (int i = 0; i < 4*8*4; i += 4) {
        const int x = (i/4) % 4;
        const int y = (i/4) / 4;
        CHECK_PIXEL(&readbuf[i], x*16,(y%4)*16,0,255, x, y);
    }

    texture_apply(0, texture2);
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,-1,0}, &(Vector2f){0,0},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,+1,0}, &(Vector2f){0,2},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,+1,0}, &(Vector2f){1,2},
                                   NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,-1,0}, &(Vector2f){1,0},
                                   NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_TRUE(graphics_read_pixels(0, 0, 4, 8, readbuf));
    for (int i = 0; i < 4*8*4; i += 4) {
        const int x = (i/4) % 4;
        const int y = (i/4) / 4;
        CHECK_PIXEL(&readbuf[i], x*16,ubound(y,3)*16,0,255, x, y);
    }

    texture_destroy(texture1);
    texture_destroy(texture2);
    return 1;
}

/*************************************************************************/
/*************************************************************************/
