/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/opengl/texture.c: Tests for OpenGL texture code.
 */

#include "src/base.h"
#undef SIL_OPENGL_NO_SYS_FUNCS  // Avoid type renaming.
#include "src/graphics.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/opengl/internal.h"
#include "src/sysdep/opengl/opengl.h"
#include "src/test/base.h"
#include "src/test/graphics/internal.h"
#include "src/texture.h"

/*
 * Note that basic texture processing (including parse() for RGBA-format
 * TEX files, grab(), and lock() for non-empty textures) has already been
 * verified by the live texture handling in sysdep/test/texture.c, invoked
 * by the graphics_primitive tests.  We thus skip some basic tests in the
 * interest of brevity.
 */

/*************************************************************************/
/******************************* Test data *******************************/
/*************************************************************************/

static const uint8_t rgba_4x4[] = {
      0,  0,  0,  0,  4,  0,  4, 16,  8,  0,  8, 32, 12,  0, 12, 48,
      0, 64, 64, 64,  4, 64, 68, 80,  8, 64, 72, 96, 12, 64, 76,112,
      0,128,128,128,  4,128,132,144,  8,128,136,160, 12,128,140,176,
      0,192,192,192,  4,192,196,208,  8,192,200,224, 12,192,204,240,
};

static const uint8_t rgba_4x4_mipmaps[] = {
    /* Level 0: red=20 */
     20,  0,  0,255, 20,  0,  0,255, 20,  0,  0,255, 20,  0,  0,255,
     20,  0,  0,255, 20,  0,  0,255, 20,  0,  0,255, 20,  0,  0,255,
     20,  0,  0,255, 20,  0,  0,255, 20,  0,  0,255, 20,  0,  0,255,
     20,  0,  0,255, 20,  0,  0,255, 20,  0,  0,255, 20,  0,  0,255,
    /* Level 1: green=20 */
      0, 20,  0,255,  0, 20,  0,255,  0, 20,  0,255,  0, 20,  0,255,
    /* Level 2: blue=20 */
      0,  0, 20,255,
    };

static const uint8_t rgba_3x3_rgb565[] = {
      0,  0,  0,255,  8,  0,  8,255, 25,  0, 25,255,
      0, 65, 66,255,  8, 65, 74,255, 25, 65, 91,255,
      0,130,132,255,  8,130,140,255, 25,130,157,255,
};

static const uint16_t rgba_4x4_rgb565_data[] = {
    0x0000, 0x0801, 0x1803, 0x2805,
    0x4200, 0x4A01, 0x5A03, 0x6A05,
    0x8400, 0x8C01, 0x9C03, 0xAC05,
    0xC600, 0xCE01, 0xDE03, 0xEE05,
};

static const uint8_t alpha_data[] = {
      0, 16, 32, 48, 64, 80, 96,112,128,144,160,176,192,208,224,240,
};

static const uint8_t palette_4x4[] = {
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
     15,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
};

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

/**
 * draw_texture:  Draw the given texture to the display, and return a
 * mem_alloc()ed buffer containing the RGBA pixel data.
 *
 * [Parameters]
 *     texture: Texture to draw.
 *     w, h: Size at which to draw texture.
 * [Return value]
 *     Pixel data, or NULL on error.
 */
static uint8_t *draw_texture(SysTexture *texture, int w, int h)
{
    sys_texture_set_repeat(texture, 0, 0);
    sys_texture_set_antialias(texture, 0);
    sys_texture_apply(0, texture);

    graphics_set_viewport(0, 0, w, h);
    graphics_set_blend(GRAPHICS_BLEND_ADD,
                       GRAPHICS_BLEND_SRC_ALPHA, GRAPHICS_BLEND_INV_SRC_ALPHA);
    ASSERT(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    ASSERT(graphics_add_vertex(&(Vector3f){-1,-1,0}, &(Vector2f){0,0}, NULL));
    ASSERT(graphics_add_vertex(&(Vector3f){-1,+1,0}, &(Vector2f){0,1}, NULL));
    ASSERT(graphics_add_vertex(&(Vector3f){+1,+1,0}, &(Vector2f){1,1}, NULL));
    ASSERT(graphics_add_vertex(&(Vector3f){+1,-1,0}, &(Vector2f){1,0}, NULL));
    ASSERT(graphics_end_and_draw_primitive());

    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(w*h*4, 0, MEM_ALLOC_TEMP));
    ASSERT(graphics_read_pixels(0, 0, w, h, pixels));
    return pixels;
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

static int do_test_opengl_texture(void);
int test_opengl_texture(void)
{
    /* We don't technically need a window for these tests (since we don't
     * render anything), but some environments need an open window in
     * order to do anything in OpenGL. */
    return run_tests_in_window(do_test_opengl_texture);
}

DEFINE_GENERIC_TEST_RUNNER(do_test_opengl_texture)

/*-----------------------------------------------------------------------*/

TEST_INIT(init)
{
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
    TEST_opengl_always_convert_texture_data = 0;

    graphics_finish_frame();
    opengl_free_dead_resources(1);
    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_create)
{
    SysTexture *texture;
    CHECK_TRUE(texture = sys_texture_create(4, 4, TEX_FORMAT_RGBA8888, 0,
                                            NULL, 0, NULL, NULL, 0, 0, 0));
    CHECK_INTEQUAL(sys_texture_width(texture), 4);
    CHECK_INTEQUAL(sys_texture_height(texture), 4);
    CHECK_FALSE(texture->auto_mipmaps);
    CHECK_FALSE(texture->has_mipmaps);

    const uint8_t *pixels;
    CHECK_TRUE(pixels = sys_texture_lock(texture, SYS_TEXTURE_LOCK_NORMAL,
                                         0, 0, 4, 4));
    for (int i = 0; i < 4*4; i++) {
        CHECK_PIXEL(&pixels[i*4], 0,0,0,0, i%4, i/4);
    }

    sys_texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_16bpp_unaligned_size)
{
    SysTexture *texture;
    CHECK_TRUE(texture = sys_texture_create(
                   3, 3, TEX_FORMAT_RGB565, 1, (void *)rgba_4x4_rgb565_data, 4,
                   (int32_t[]){0}, (int32_t[]){sizeof(rgba_4x4_rgb565_data)},
                   0, 0, 0));
    CHECK_INTEQUAL(sys_texture_width(texture), 3);
    CHECK_INTEQUAL(sys_texture_height(texture), 3);
    CHECK_FALSE(texture->auto_mipmaps);
    CHECK_FALSE(texture->has_mipmaps);
    sys_texture_set_repeat(texture, 0, 0);

    uint8_t pixels[3*3*4];
    graphics_set_viewport(0, 0, 3, 3);
    sys_texture_apply(0, texture);
    ASSERT(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    ASSERT(graphics_add_vertex(&(Vector3f){-1,-1,0}, &(Vector2f){0,0}, NULL));
    ASSERT(graphics_add_vertex(&(Vector3f){-1,+1,0}, &(Vector2f){0,1}, NULL));
    ASSERT(graphics_add_vertex(&(Vector3f){+1,+1,0}, &(Vector2f){1,1}, NULL));
    ASSERT(graphics_add_vertex(&(Vector3f){+1,-1,0}, &(Vector2f){1,0}, NULL));
    ASSERT(graphics_end_and_draw_primitive());
    sys_texture_apply(0, NULL);
    ASSERT(graphics_read_pixels(0, 0, 3, 3, pixels));
    for (int i = 0; i < 3*3; i++) {
        const int r = rgba_3x3_rgb565[i*4+0];
        const int g = rgba_3x3_rgb565[i*4+1];
        const int b = rgba_3x3_rgb565[i*4+2];
        const int a = rgba_3x3_rgb565[i*4+3];
        CHECK_PIXEL_NEAR(&pixels[i*4], r,g,b,a, 4, i%4, i/4);
    }

    sys_texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_unknown_type)
{
    CHECK_FALSE(sys_texture_create(4, 4, -1, 0, NULL, 0, NULL, NULL, 0, 0, 0));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_mipmaps)
{
    SysTexture *texture;
    CHECK_TRUE(texture = sys_texture_create(4, 4, TEX_FORMAT_RGBA8888, 0,
                                            NULL, 0, NULL, NULL, 1, 0, 0));
    CHECK_TRUE(texture->auto_mipmaps);
    CHECK_TRUE(texture->has_mipmaps);

    const uint8_t *pixels;
    CHECK_TRUE(pixels = sys_texture_lock(texture, SYS_TEXTURE_LOCK_NORMAL,
                                         0, 0, 4, 4));
    for (int i = 0; i < 4*4; i++) {
        CHECK_PIXEL(&pixels[i*4], 0,0,0,0, i%4, i/4);
    }

    sys_texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_mipmaps_npot)
{
    SysTexture *texture;
    CHECK_TRUE(texture = sys_texture_create(5, 3, TEX_FORMAT_RGBA8888, 0,
                                            NULL, 0, NULL, NULL, 1, 0, 0));
#ifndef SIL_OPENGL_ES
    if (opengl_has_features(OPENGL_FEATURE_GENERATEMIPMAP)) {
        CHECK_TRUE(texture->auto_mipmaps);
        CHECK_TRUE(texture->has_mipmaps);
    } else {
#endif
        CHECK_FALSE(texture->auto_mipmaps);
        CHECK_FALSE(texture->has_mipmaps);
#ifndef SIL_OPENGL_ES
    }
#endif

    const uint8_t *pixels;
    CHECK_TRUE(pixels = sys_texture_lock(texture, SYS_TEXTURE_LOCK_NORMAL,
                                         0, 0, 5, 3));
    for (int i = 0; i < 5*3; i++) {
        CHECK_PIXEL(&pixels[i*4], 0,0,0,0, i%5, i/5);
    }

    sys_texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_mipmaps_npot_height_only)
{
    SysTexture *texture;
    CHECK_TRUE(texture = sys_texture_create(4, 3, TEX_FORMAT_RGBA8888, 0,
                                            NULL, 0, NULL, NULL, 1, 0, 0));
#ifndef SIL_OPENGL_ES
    if (opengl_has_features(OPENGL_FEATURE_GENERATEMIPMAP)) {
        CHECK_TRUE(texture->auto_mipmaps);
        CHECK_TRUE(texture->has_mipmaps);
    } else {
#endif
        CHECK_FALSE(texture->auto_mipmaps);
        CHECK_FALSE(texture->has_mipmaps);
#ifndef SIL_OPENGL_ES
    }
#endif

    const uint8_t *pixels;
    CHECK_TRUE(pixels = sys_texture_lock(texture, SYS_TEXTURE_LOCK_NORMAL,
                                         0, 0, 4, 3));
    for (int i = 0; i < 4*3; i++) {
        CHECK_PIXEL(&pixels[i*4], 0,0,0,0, i%4, i/4);
    }

    sys_texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_memory_failure)
{
    SysTexture *texture;
    CHECK_MEMORY_FAILURES(texture = sys_texture_create(
                              4, 4, TEX_FORMAT_RGBA8888, 0,
                              NULL, 0, NULL, NULL, 1, 0, 0));
    CHECK_INTEQUAL(sys_texture_width(texture), 4);
    CHECK_INTEQUAL(sys_texture_height(texture), 4);
    CHECK_TRUE(texture->auto_mipmaps);
    CHECK_TRUE(texture->has_mipmaps);

    const uint8_t *pixels;
    CHECK_TRUE(pixels = sys_texture_lock(texture, SYS_TEXTURE_LOCK_NORMAL,
                                         0, 0, 4, 4));
    for (int i = 0; i < 4*4; i++) {
        CHECK_PIXEL(&pixels[i*4], 0,0,0,0, i%4, i/4);
    }

    sys_texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_alpha)
{
    SysTexture *texture;
    CHECK_TRUE(texture = sys_texture_create(
                   4, 4, TEX_FORMAT_A8, 1, (void *)alpha_data, 4,
                   (int32_t[]){0}, (int32_t[]){sizeof(alpha_data)}, 0, 0, 0));
    CHECK_INTEQUAL(sys_texture_width(texture), 4);
    CHECK_INTEQUAL(sys_texture_height(texture), 4);
    CHECK_FALSE(texture->auto_mipmaps);
    CHECK_FALSE(texture->has_mipmaps);

    /* For this test (and many below), we draw the texture to the display
     * and read back the result because OpenGL ES limitations prevent us
     * from reading the texture data directly on ES platforms. */
    uint8_t *pixels;
    ASSERT(pixels = draw_texture(texture, 4, 4));
    for (int i = 0; i < 4*4; i++) {
        const int a = alpha_data[i];
        CHECK_PIXEL(&pixels[i*4], a,a,a,255, i%4, i/4);
    }
    mem_free(pixels);

    sys_texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_alpha_mipmaps_npot)
{
    SysTexture *texture;
    CHECK_TRUE(texture = sys_texture_create(
                   5, 3, TEX_FORMAT_A8, 1, (void *)alpha_data, 5,
                   (int32_t[]){0}, (int32_t[]){5 * 3}, 1, 0, 0));
#ifndef SIL_OPENGL_ES
    if (opengl_has_features(OPENGL_FEATURE_GENERATEMIPMAP)) {
        CHECK_TRUE(texture->auto_mipmaps);
        CHECK_TRUE(texture->has_mipmaps);
    } else {
#endif
        CHECK_FALSE(texture->auto_mipmaps);
        CHECK_FALSE(texture->has_mipmaps);
#ifndef SIL_OPENGL_ES
    }
#endif

    uint8_t *pixels;
    ASSERT(pixels = draw_texture(texture, 5, 3));
    for (int i = 0; i < 5*3; i++) {
        /* NPOT sizes/coordinates means there may be off-by-one issues. */
        CHECK_PIXEL(&pixels[i*4],
                    alpha_data[i], alpha_data[i], alpha_data[i], 255,
                    i%5, i/5);
    }
    mem_free(pixels);

    sys_texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_luminance_reuse)
{
    void *buffer;
    ASSERT(buffer = mem_alloc(sizeof(alpha_data), 0, 0));
    memcpy(buffer, alpha_data, sizeof(alpha_data));

    SysTexture *texture;
    CHECK_TRUE(texture = sys_texture_create(
                   4, 4, TEX_FORMAT_L8, 1, buffer, 4,
                   (int32_t[]){0}, (int32_t[]){sizeof(alpha_data)}, 0, 0, 1));
    CHECK_INTEQUAL(sys_texture_width(texture), 4);
    CHECK_INTEQUAL(sys_texture_height(texture), 4);
    CHECK_FALSE(texture->auto_mipmaps);
    CHECK_FALSE(texture->has_mipmaps);

    uint8_t *pixels;
    ASSERT(pixels = draw_texture(texture, 4, 4));
    for (int i = 0; i < 4*4; i++) {
        const int l = alpha_data[i];
        CHECK_PIXEL(&pixels[i*4], l,l,l,255, i%4, i/4);
    }
    mem_free(pixels);

    sys_texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_luminance_reuse_short_stride)
{
    void *buffer;
    ASSERT(buffer = mem_alloc(sizeof(alpha_data), 0, 0));
    memcpy(buffer, alpha_data, sizeof(alpha_data));

    SysTexture *texture;
    CHECK_TRUE(texture = sys_texture_create(
                   4, 4, TEX_FORMAT_L8, 1, buffer, 1,
                   (int32_t[]){0}, (int32_t[]){sizeof(alpha_data)}, 0, 0, 1));
    CHECK_INTEQUAL(sys_texture_width(texture), 4);
    CHECK_INTEQUAL(sys_texture_height(texture), 4);
    CHECK_FALSE(texture->auto_mipmaps);
    CHECK_FALSE(texture->has_mipmaps);

    uint8_t *pixels;
    ASSERT(pixels = draw_texture(texture, 4, 4));
    for (int i = 0; i < 4*4; i++) {
        const int x = i%4, y = i/4;
        const int l = alpha_data[y*1+x];
        CHECK_PIXEL(&pixels[i*4], l,l,l,255, x, y);
    }
    mem_free(pixels);

    sys_texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_palette_memory_failure)
{
    SysTexture *texture;
    CHECK_MEMORY_FAILURES(texture = sys_texture_create(
                              4, 4, TEX_FORMAT_PALETTE8_RGBA8888, 1,
                              (void *)palette_4x4, 4, (int32_t[]){0},
                              (int32_t[]){1040}, 1, 0, 0));
    CHECK_INTEQUAL(sys_texture_width(texture), 4);
    CHECK_INTEQUAL(sys_texture_height(texture), 4);
    /* If we're generating mipmaps locally, sys_texture_create() will give
     * up on mipmaps due to memory allocation failure. */
    if (opengl_has_features(OPENGL_FEATURE_GENERATEMIPMAP)) {
        CHECK_TRUE(texture->auto_mipmaps);
        CHECK_TRUE(texture->has_mipmaps);
    } else {
        CHECK_FALSE(texture->auto_mipmaps);
        CHECK_FALSE(texture->has_mipmaps);
    }

    const uint8_t *pixels;
    CHECK_TRUE(pixels = sys_texture_lock(texture, SYS_TEXTURE_LOCK_NORMAL,
                                         0, 0, 4, 4));
    for (int i = 0; i < 4*4; i++) {
        CHECK_PIXEL(&pixels[i*4], rgba_4x4[i*4+0], rgba_4x4[i*4+1],
                    rgba_4x4[i*4+2], rgba_4x4[i*4+3], i%4, i/4);
    }

    sys_texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_stride_fixup_unaligned_16bpp)
{
    static const uint16_t data[] = {0x0001, 0, 0, 0x0800, 0, 0};
    SysTexture *texture;
    CHECK_TRUE(texture = sys_texture_create(
                   1, 2, TEX_FORMAT_BGR565, 1, (void *)data, 3,
                   (int32_t[]){0}, (int32_t[]){sizeof(data)}, 1, 0, 0));
    CHECK_INTEQUAL(sys_texture_width(texture), 1);
    CHECK_INTEQUAL(sys_texture_height(texture), 2);
    /* generate_mipmaps() doesn't handle non-32bpp textures. */
    if (opengl_has_features(OPENGL_FEATURE_GENERATEMIPMAP)) {
        CHECK_TRUE(texture->auto_mipmaps);
        CHECK_TRUE(texture->has_mipmaps);
    } else {
        CHECK_FALSE(texture->auto_mipmaps);
        CHECK_FALSE(texture->has_mipmaps);
    }

    uint8_t *pixels;
    ASSERT(pixels = draw_texture(texture, 4, 4));
    for (int i = 0; i < 4*4; i++) {
        const int x = i%4;
        const int y = i/4;
        const int r = (y < 2 ? 0 : 8);
        const int b = (y < 2 ? 8 : 0);
        CHECK_PIXEL(&pixels[i*4], r,0,b,255, x, y);
    }
    mem_free(pixels);

    sys_texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_stride_fixup_unaligned_8bpp)
{
    static const uint8_t data[] = {0x55, 0, 0, 0xAA, 0, 0};
    SysTexture *texture;
    CHECK_TRUE(texture = sys_texture_create(
                   1, 2, TEX_FORMAT_A8, 1, (void *)data, 3,
                   (int32_t[]){0}, (int32_t[]){sizeof(data)}, 1, 0, 0));
    CHECK_INTEQUAL(sys_texture_width(texture), 1);
    CHECK_INTEQUAL(sys_texture_height(texture), 2);
    /* On desktop OpenGL with shaders but no ARB_texture_rg, we convert
     * alpha-only textures to RGBA, so we can generate mipmaps for them
     * with generate_mipmaps(). */
    if (opengl_has_features(OPENGL_FEATURE_GENERATEMIPMAP)
#ifndef SIL_OPENGL_ES
     || !opengl_has_formats(OPENGL_FORMAT_RG)
#endif
    ) {
        CHECK_TRUE(texture->auto_mipmaps);
        CHECK_TRUE(texture->has_mipmaps);
    } else {
        CHECK_FALSE(texture->auto_mipmaps);
        CHECK_FALSE(texture->has_mipmaps);
    }

    uint8_t *pixels;
    ASSERT(pixels = draw_texture(texture, 4, 4));
    for (int i = 0; i < 4*4; i++) {
        const int x = i%4;
        const int y = i/4;
        const int p = (y < 2 ? 0x55 : 0xAA);
        CHECK_PIXEL(&pixels[i*4], p,p,p,255, x, y);
    }
    mem_free(pixels);

    sys_texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_stride_fixup_unaligned_16bpp_reuse)
{
    static const uint16_t data[] = {0x0001, 0, 0, 0x0800, 0, 0};
    uint8_t *data_copy;
    ASSERT(data_copy = mem_alloc(sizeof(data), 0, 0));
    memcpy(data_copy, data, sizeof(data));
    SysTexture *texture;
    CHECK_TRUE(texture = sys_texture_create(
                   1, 2, TEX_FORMAT_BGR565, 1, data_copy, 3,
                   (int32_t[]){0}, (int32_t[]){sizeof(data)}, 1, 0, 1));
    CHECK_INTEQUAL(sys_texture_width(texture), 1);
    CHECK_INTEQUAL(sys_texture_height(texture), 2);
    if (opengl_has_features(OPENGL_FEATURE_GENERATEMIPMAP)) {
        CHECK_TRUE(texture->auto_mipmaps);
        CHECK_TRUE(texture->has_mipmaps);
    } else {
        CHECK_FALSE(texture->auto_mipmaps);
        CHECK_FALSE(texture->has_mipmaps);
    }

    uint8_t *pixels;
    ASSERT(pixels = draw_texture(texture, 4, 4));
    for (int i = 0; i < 4*4; i++) {
        const int x = i%4;
        const int y = i/4;
        const int r = (y < 2 ? 0 : 8);
        const int b = (y < 2 ? 8 : 0);
        CHECK_PIXEL(&pixels[i*4], r,0,b,255, x, y);
    }
    mem_free(pixels);

    sys_texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_stride_fixup_unaligned_8bpp_reuse)
{
    static const uint8_t data[] = {0x55, 0, 0, 0xAA, 0, 0};
    uint8_t *data_copy;
    ASSERT(data_copy = mem_alloc(sizeof(data), 0, 0));
    memcpy(data_copy, data, sizeof(data));
    SysTexture *texture;
    CHECK_TRUE(texture = sys_texture_create(
                   1, 2, TEX_FORMAT_A8, 1, data_copy, 3,
                   (int32_t[]){0}, (int32_t[]){sizeof(data)}, 1, 0, 1));
    CHECK_INTEQUAL(sys_texture_width(texture), 1);
    CHECK_INTEQUAL(sys_texture_height(texture), 2);
    if (opengl_has_features(OPENGL_FEATURE_GENERATEMIPMAP)
#ifndef SIL_OPENGL_ES  // As for the non-reuse case.
     || !opengl_has_formats(OPENGL_FORMAT_RG)
#endif
    ) {
        CHECK_TRUE(texture->auto_mipmaps);
        CHECK_TRUE(texture->has_mipmaps);
    } else {
        CHECK_FALSE(texture->auto_mipmaps);
        CHECK_FALSE(texture->has_mipmaps);
    }

    uint8_t *pixels;
    ASSERT(pixels = draw_texture(texture, 4, 4));
    for (int i = 0; i < 4*4; i++) {
        const int x = i%4;
        const int y = i/4;
        const int p = (y < 2 ? 0x55 : 0xAA);
        CHECK_PIXEL(&pixels[i*4], p,p,p,255, x, y);
    }
    mem_free(pixels);

    sys_texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_stride_fixup_memory_failure)
{
    uint8_t data[128];
    ASSERT(sizeof(data) == sizeof(rgba_4x4)*2);
    for (int y = 0; y < 4; y++) {
        memcpy(&data[y*32], &rgba_4x4[y*16], 16);
    }
    SysTexture *texture;
    CHECK_MEMORY_FAILURES(texture = sys_texture_create(
                              4, 4, TEX_FORMAT_RGBA8888, 1, data, 8,
                              (int32_t[]){0}, (int32_t[]){128}, 1, 0, 0));
    CHECK_INTEQUAL(sys_texture_width(texture), 4);
    CHECK_INTEQUAL(sys_texture_height(texture), 4);
    /* If we're generating mipmaps locally, sys_texture_create() will give
     * up on mipmaps due to memory allocation failure. */
    if (opengl_has_features(OPENGL_FEATURE_GENERATEMIPMAP)) {
        CHECK_TRUE(texture->auto_mipmaps);
        CHECK_TRUE(texture->has_mipmaps);
    } else {
        CHECK_FALSE(texture->auto_mipmaps);
        CHECK_FALSE(texture->has_mipmaps);
    }

    const uint8_t *pixels;
    CHECK_TRUE(pixels = sys_texture_lock(texture, SYS_TEXTURE_LOCK_NORMAL,
                                         0, 0, 4, 4));
    for (int i = 0; i < 4*4; i++) {
        CHECK_PIXEL(&pixels[i*4], rgba_4x4[i*4+0], rgba_4x4[i*4+1],
                    rgba_4x4[i*4+2], rgba_4x4[i*4+3], i%4, i/4);
    }

    sys_texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_convert_bgra8888)
{
    TEST_opengl_always_convert_texture_data = 1;

    static const uint8_t data[] = {0x33, 0x66, 0x99, 0xAA};
    SysTexture *texture;
    CHECK_TRUE(texture = sys_texture_create(
                   1, 1, TEX_FORMAT_BGRA8888, 1, (void *)data, 1,
                   (int32_t[]){0}, (int32_t[]){sizeof(data)}, 1, 0, 0));

    uint8_t *pixels;
    ASSERT(pixels = draw_texture(texture, 4, 4));
    for (int i = 0; i < 4*4; i++) {
        const int x = i%4;
        const int y = i/4;
        CHECK_PIXEL_NEAR(&pixels[i*4], 102,68,34,255, 1, x, y);
    }
    mem_free(pixels);

    sys_texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_convert_565rev)
{
    TEST_opengl_always_convert_texture_data = 1;

    static const uint16_t data[] = {0x0862};
    SysTexture *texture;
    CHECK_TRUE(texture = sys_texture_create(
                   1, 1, TEX_FORMAT_RGB565, 1, (void *)data, 1,
                   (int32_t[]){0}, (int32_t[]){sizeof(data)}, 1, 0, 0));

    uint8_t *pixels;
    ASSERT(pixels = draw_texture(texture, 4, 4));
    for (int i = 0; i < 4*4; i++) {
        const int x = i%4;
        const int y = i/4;
        CHECK_PIXEL_NEAR(&pixels[i*4], 16,12,8,255, 1, x, y);
    }
    mem_free(pixels);

    sys_texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_convert_rgba1555rev)
{
    TEST_opengl_always_convert_texture_data = 1;

    static const uint16_t data[] = {0x8462};
    SysTexture *texture;
    CHECK_TRUE(texture = sys_texture_create(
                   1, 1, TEX_FORMAT_RGBA5551, 1, (void *)data, 1,
                   (int32_t[]){0}, (int32_t[]){sizeof(data)}, 1, 0, 0));

    uint8_t *pixels;
    ASSERT(pixels = draw_texture(texture, 4, 4));
    for (int i = 0; i < 4*4; i++) {
        const int x = i%4;
        const int y = i/4;
        CHECK_PIXEL_NEAR(&pixels[i*4], 16,24,8,255, 1, x, y);
    }
    mem_free(pixels);

    sys_texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_convert_bgra1555rev)
{
    TEST_opengl_always_convert_texture_data = 1;

    static const uint16_t data[] = {0x8462};
    SysTexture *texture;
    CHECK_TRUE(texture = sys_texture_create(
                   1, 1, TEX_FORMAT_BGRA5551, 1, (void *)data, 1,
                   (int32_t[]){0}, (int32_t[]){sizeof(data)}, 1, 0, 0));

    uint8_t *pixels;
    ASSERT(pixels = draw_texture(texture, 4, 4));
    for (int i = 0; i < 4*4; i++) {
        const int x = i%4;
        const int y = i/4;
        CHECK_PIXEL_NEAR(&pixels[i*4], 8,24,16,255, 1, x, y);
    }
    mem_free(pixels);

    sys_texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_convert_rgba4444rev)
{
    TEST_opengl_always_convert_texture_data = 1;

    static const uint16_t data[] = {0x4321};
    SysTexture *texture;
    CHECK_TRUE(texture = sys_texture_create(
                   1, 1, TEX_FORMAT_RGBA4444, 1, (void *)data, 1,
                   (int32_t[]){0}, (int32_t[]){sizeof(data)}, 1, 0, 0));

    uint8_t *pixels;
    ASSERT(pixels = draw_texture(texture, 4, 4));
    for (int i = 0; i < 4*4; i++) {
        const int x = i%4;
        const int y = i/4;
        CHECK_PIXEL_NEAR(&pixels[i*4], 5,9,14,255, 1, x, y);
    }
    mem_free(pixels);

    sys_texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_convert_bgra4444rev)
{
    TEST_opengl_always_convert_texture_data = 1;

    static const uint16_t data[] = {0x4321};
    SysTexture *texture;
    CHECK_TRUE(texture = sys_texture_create(
                   1, 1, TEX_FORMAT_BGRA4444, 1, (void *)data, 1,
                   (int32_t[]){0}, (int32_t[]){sizeof(data)}, 1, 0, 0));

    uint8_t *pixels;
    ASSERT(pixels = draw_texture(texture, 4, 4));
    for (int i = 0; i < 4*4; i++) {
        const int x = i%4;
        const int y = i/4;
        CHECK_PIXEL_NEAR(&pixels[i*4], 14,9,5,255, 1, x, y);
    }
    mem_free(pixels);

    sys_texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_convert_reuse)
{
    TEST_opengl_always_convert_texture_data = 1;

    static const uint16_t data[] = {0x0001, 0x0800};
    uint8_t *data_copy;
    ASSERT(data_copy = mem_alloc(sizeof(data), 0, 0));
    memcpy(data_copy, data, sizeof(data));
    SysTexture *texture;
    CHECK_TRUE(texture = sys_texture_create(
                   1, 2, TEX_FORMAT_RGB565, 1, data_copy, 1,
                   (int32_t[]){0}, (int32_t[]){sizeof(data)}, 1, 0, 1));
    CHECK_INTEQUAL(sys_texture_width(texture), 1);
    CHECK_INTEQUAL(sys_texture_height(texture), 2);
    if (opengl_has_features(OPENGL_FEATURE_GENERATEMIPMAP)) {
        CHECK_TRUE(texture->auto_mipmaps);
        CHECK_TRUE(texture->has_mipmaps);
    } else {
        CHECK_FALSE(texture->auto_mipmaps);
        CHECK_FALSE(texture->has_mipmaps);
    }

    uint8_t *pixels;
    ASSERT(pixels = draw_texture(texture, 4, 4));
    for (int i = 0; i < 4*4; i++) {
        const int x = i%4;
        const int y = i/4;
        const int r = (y < 2 ? 8 : 0);
        const int b = (y < 2 ? 0 : 8);
        CHECK_PIXEL(&pixels[i*4], r,0,b,255, x, y);
    }
    mem_free(pixels);

    sys_texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_convert_reuse_stride_fixup)
{
    TEST_opengl_always_convert_texture_data = 1;

    static const uint16_t data[] = {0x0001, 0, 0, 0x0800, 0, 0};
    uint8_t *data_copy;
    ASSERT(data_copy = mem_alloc(sizeof(data), 0, 0));
    memcpy(data_copy, data, sizeof(data));
    SysTexture *texture;
    CHECK_TRUE(texture = sys_texture_create(
                   1, 2, TEX_FORMAT_RGB565, 1, data_copy, 3,
                   (int32_t[]){0}, (int32_t[]){sizeof(data)}, 1, 0, 1));
    CHECK_INTEQUAL(sys_texture_width(texture), 1);
    CHECK_INTEQUAL(sys_texture_height(texture), 2);
    if (opengl_has_features(OPENGL_FEATURE_GENERATEMIPMAP)) {
        CHECK_TRUE(texture->auto_mipmaps);
        CHECK_TRUE(texture->has_mipmaps);
    } else {
        CHECK_FALSE(texture->auto_mipmaps);
        CHECK_FALSE(texture->has_mipmaps);
    }

    uint8_t *pixels;
    ASSERT(pixels = draw_texture(texture, 4, 4));
    for (int i = 0; i < 4*4; i++) {
        const int x = i%4;
        const int y = i/4;
        const int r = (y < 2 ? 8 : 0);
        const int b = (y < 2 ? 0 : 8);
        CHECK_PIXEL(&pixels[i*4], r,0,b,255, x, y);
    }
    mem_free(pixels);

    sys_texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_auto_mipmaps_override_data_mipmaps)
{
    SysTexture *texture;
    /* Safe to de-const the input buffer since we're not reusing it. */
    CHECK_TRUE(texture = sys_texture_create(
                   4, 4, TEX_FORMAT_RGBA8888, 1, (void *)rgba_4x4_mipmaps, 4,
                   (int32_t[]){0, 64, 80}, (int32_t[]){64, 16, 4}, 1, 0, 0));
    sys_texture_apply(0, texture);

    uint8_t pixels[4*4*4];
    graphics_set_viewport(0, 0, 4, 4);
    ASSERT(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    ASSERT(graphics_add_vertex(&(Vector3f){-1,-1,0}, &(Vector2f){0,0}, NULL));
    ASSERT(graphics_add_vertex(&(Vector3f){-1,+1,0}, &(Vector2f){0,2}, NULL));
    ASSERT(graphics_add_vertex(&(Vector3f){+1,+1,0}, &(Vector2f){2,2}, NULL));
    ASSERT(graphics_add_vertex(&(Vector3f){+1,-1,0}, &(Vector2f){2,0}, NULL));
    ASSERT(graphics_end_and_draw_primitive());
    ASSERT(graphics_read_pixels(0, 0, 4, 4, pixels));
    for (int i = 0; i < 4*4; i++) {
        /* Theoretically this should be a solid color, but some renderers
         * (hi, PVR) seem to introduce slight inaccuracies in mipmap
         * generation or blending. */
        CHECK_PIXEL_NEAR(&pixels[i*4], 20,0,0,255, 1, i%4, i/4);
    }

    sys_texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_destroy_multiple)
{
    SysTexture *texture1, *texture2, *texture3;
    ASSERT(texture1 = sys_texture_create(4, 4, TEX_FORMAT_RGBA8888, 0,
                                         NULL, 0, NULL, NULL, 0, 0, 0));
    ASSERT(texture2 = sys_texture_create(4, 4, TEX_FORMAT_RGBA8888, 0,
                                         NULL, 0, NULL, NULL, 0, 0, 0));
    ASSERT(texture3 = sys_texture_create(4, 4, TEX_FORMAT_RGBA8888, 0,
                                         NULL, 0, NULL, NULL, 0, 0, 0));

    sys_texture_destroy(texture2);
    sys_texture_destroy(texture1);
    sys_texture_destroy(texture3);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_from_display_mipmaps)
{
    const int width = 4;
    const int height = 4;

    int texture;
    graphics_clear(0.2, 0.4, 0.6, 0, 1, 0);
    CHECK_TRUE(
        texture = texture_create_from_display(0, 0, width, height, 1, 0, 1));
    CHECK_TRUE(texture_has_mipmaps(texture));

    const uint8_t *pixels;
    CHECK_TRUE(pixels = texture_lock_readonly(texture));
    for (int i = 0; i < width*height*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 51,102,153,255, (i/4) % width, (i/4) / width);
    }

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_from_display_memory_failure)
{
    const int width = 4;
    const int height = 4;

    int texture;
    graphics_clear(0.2, 0.4, 0.6, 0, 1, 0);
    CHECK_MEMORY_FAILURES(
        texture = texture_create_from_display(0, 0, width, height, 0, 0, 1));
    CHECK_TRUE(texture_has_mipmaps(texture));

    const uint8_t *pixels;
    CHECK_TRUE(pixels = texture_lock_readonly(texture));
    for (int i = 0; i < width*height*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 51,102,153,255, (i/4) % width, (i/4) / width);
    }

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_from_display_link_to_all_textures)
{
    const int width = 4;
    const int height = 4;
    int texture1, texture2;

    CHECK_TRUE(texture1 = texture_create(width, height, 0, 0));
    graphics_clear(0.2, 0.4, 0.6, 0, 1, 0);
    CHECK_TRUE(
        texture2 = texture_create_from_display(0, 0, width, height, 0, 0, 0));

    const uint8_t *pixels;
    CHECK_TRUE(pixels = texture_lock_readonly(texture2));
    for (int i = 0; i < width*height*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 51,102,153,255, (i/4) % width, (i/4) / width);
    }

    texture_destroy(texture1);  // Do this first to validate the linked list.
    texture_destroy(texture2);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_update_over_gles_unreadable)
{
    const int width = 4;
    const int height = 4;
    int texture;
    uint8_t *pixels;

    ASSERT(pixels = mem_alloc(width*height*2, 0, MEM_ALLOC_CLEAR));
    CHECK_TRUE(texture = texture_create_with_data(
                   width, height, pixels, TEX_FORMAT_BGR565, width, 0, 0));
    mem_free(pixels);

    CHECK_TRUE(pixels = texture_lock_writeonly(texture));
    memset(pixels, 255, width*height*4);
    texture_unlock(texture);

    CHECK_TRUE(pixels = (uint8_t *)texture_lock_readonly(texture));
    for (int i = 0; i < width*height*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 255,255,255,255, (i/4) % width, (i/4) / width);
    }

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

#ifndef SIL_OPENGL_ES

TEST(test_lock_update_mipmaps)
{
    if (!opengl_has_features(OPENGL_FEATURE_GETTEXIMAGE)) {
        /* We call glGetTexImage() to verify the mipmap contents, so we
         * can't run this test if glGetTexImage() isn't available. */
        return 1;
    }

    /* We also use this test to check the "wide texture" case in
     * generate_mipamps(). */
    SysTexture *texture;
    ASSERT(texture = sys_texture_create(
               4, 2, TEX_FORMAT_RGBA8888, 1, (void *)rgba_4x4, 4,
               (int32_t[]){0}, (int32_t[]){2*4*4}, 1, 0, 0));

    uint8_t *pixels;
    ASSERT(pixels = sys_texture_lock(
               texture, SYS_TEXTURE_LOCK_NORMAL, 0, 0, 4, 2));
    for (int i = 0; i < 4*2*4; i += 4) {
        /* 0x10101010 ... 0x80808080 */
        pixels[i+0] = pixels[i+1] = pixels[i+2] = pixels[i+3] = (i+4)*4;
    }
    sys_texture_unlock(texture, 1);
    sys_texture_apply(0, texture);

    uint8_t read_pixels[4*2*4];
    opengl_clear_error();
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, read_pixels);
    ASSERT(glGetError() == GL_NO_ERROR);
    for (int i = 0; i < 4*2*4; i += 4) {
        const int p = (i+4)*4;
        CHECK_PIXEL(&read_pixels[i], p,p,p,p, (i/4)%4, (i/4)/4);
    }

    opengl_clear_error();
    glGetTexImage(GL_TEXTURE_2D, 1, GL_RGBA, GL_UNSIGNED_BYTE, read_pixels);
    ASSERT(glGetError() == GL_NO_ERROR);
    CHECK_PIXEL_NEAR(&read_pixels[0], 0x38,0x38,0x38,0x38, 1, 0, 0);
    CHECK_PIXEL_NEAR(&read_pixels[4], 0x58,0x58,0x58,0x58, 1, 1, 0);

    opengl_clear_error();
    glGetTexImage(GL_TEXTURE_2D, 2, GL_RGBA, GL_UNSIGNED_BYTE, read_pixels);
    ASSERT(glGetError() == GL_NO_ERROR);
    CHECK_PIXEL_NEAR(&read_pixels[0], 0x48,0x48,0x48,0x48, 1, 0, 0);

    sys_texture_destroy(texture);
    return 1;
}

#endif  // !SIL_OPENGL_ES

/*-----------------------------------------------------------------------*/

TEST(test_lock_partial)
{
    SysTexture *texture;
    ASSERT(texture = sys_texture_create(
               4, 4, TEX_FORMAT_RGBA8888, 1, (void *)rgba_4x4, 4,
               (int32_t[]){0}, (int32_t[]){sizeof(rgba_4x4)}, 0, 0, 0));

    uint8_t *pixels;
    ASSERT(pixels = sys_texture_lock(
               texture, SYS_TEXTURE_LOCK_NORMAL, 1, 1, 2, 2));
    for (int y = 0; y < 2; y++) {
        for (int x = 0; x < 2; x++) {
            CHECK_PIXEL(&pixels[(y*2+x)*4],
                        rgba_4x4[((y+1)*4+(x+1))*4+0],
                        rgba_4x4[((y+1)*4+(x+1))*4+1],
                        rgba_4x4[((y+1)*4+(x+1))*4+2],
                        rgba_4x4[((y+1)*4+(x+1))*4+3],
                        x, y);
        }
    }
    sys_texture_unlock(texture, 0);

    sys_texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_texture_id)
{
    SysTexture *texture;
    ASSERT(texture = sys_texture_create(
               4, 4, TEX_FORMAT_RGBA8888, 0, NULL, 0, NULL, NULL, 0, 0, 0));

    CHECK_INTEQUAL(opengl_texture_id(texture), texture->id);

    sys_texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_num_units)
{
    /*
     * OpenGL guarantees (for reference):
     *
     *  Version | TIU | CTIU | GTIU | VTIU | *TIU
     * ---------+-----+------+------+------+------
     *  GL 2.x  |   2 | ---- | ---- | ---- | ----
     *  GL 3.x  |  16 | ---- |  16  |  16  |  48
     *  GL 4.0-2|  16 | ---- |  16  |  16  |  48
     *  GL 4.3+ |  16 |  16  |  16  |  16  |  48
     *  ES 2.x  |   8 | ---- | ---- |   0  |   8
     *  ES 3.0  |  16 | ---- | ---- |  16  |  32
     *  ES 3.1+ |  16 |  16  | ---- |  16  |  32
     *
     * Legend:  TIU = MAX_TEXTURE_IMAGE_UNITS (for fragment shaders)
     *         CTIU = MAX_COMPUTE_TEXTURE_IMAGE_UNITS
     *         GTIU = MAX_GEOMETRY_TEXTURE_IMAGE_UNITS
     *         VTIU = MAX_VERTEX_TEXTURE_IMAGE_UNITS
     *         *TIU = MAX_COMBINED_TEXTURE_IMAGE_UNITS
     *
     * Note that OpenGL 1.[345] support multitexturing in the
     * fixed-function pipeline and guarantee at least two texture units,
     * but since we only support multitexturing in shaders, we don't worry
     * about that case.
     */

#ifdef SIL_OPENGL_ES
    if (opengl_version_is_at_least(3,0)) {
        CHECK_TRUE(sys_texture_num_units() >= 16);
    } else {
        CHECK_TRUE(texture_num_units() >= 8);
    }
#else
    if (opengl_version_is_at_least(3,0)) {
        CHECK_TRUE(sys_texture_num_units() >= 16);
    } else {
        CHECK_TRUE(sys_texture_num_units() >= 2);
    }
#endif

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_state_loss)
{
    if (!graphics_has_windowed_mode()) {
        SKIP("Not applicable to this platform.");
    }

    uint8_t *pixels;

    /* Reset the context once so we start with a consistent state. */
    graphics_finish_frame();
    force_close_window();
    ASSERT(open_window(TESTW, TESTH));
    graphics_start_frame();

    SysTexture *texture;
    static const uint8_t tex_data1[] = {51,102,153,255, 51,102,153,255,
                                        51,102,153,255};
    CHECK_TRUE(texture = sys_texture_create(
                   2, 1, TEX_FORMAT_RGBA8888, 2, (void *)tex_data1, 2,
                   (int32_t[]){0, 8}, (int32_t[]){8, 4}, 1, 0, 0));

    graphics_finish_frame();
    force_close_window();
    ASSERT(open_window(TESTW, TESTH));
    graphics_start_frame();

    SysTexture *texture2;
    static const uint8_t tex_data2[] = {204,153,102,255};
    CHECK_TRUE(texture2 = sys_texture_create(
                   1, 1, TEX_FORMAT_RGBA8888, 1, (void *)tex_data2, 1,
                   (int32_t[]){0}, (int32_t[]){sizeof(tex_data2)}, 0, 0, 0));
    CHECK_INTEQUAL(texture2->id, texture->id);

    /* Basic information calls should still succeed. */
    CHECK_INTEQUAL(sys_texture_width(texture), 2);
    CHECK_INTEQUAL(sys_texture_height(texture), 1);
    CHECK_TRUE(sys_texture_has_mipmaps(texture));

    /* These should be no-ops; we can't check the results, but we can at
     * least check that the calls don't cause a crash. */
    sys_texture_set_repeat(texture, 0, 0);
    sys_texture_set_antialias(texture, 0);

    graphics_clear(0, 1, 0, 0, 1, 0);
    sys_texture_apply(0, texture);  // This should fail.
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,-1,0},
                                   &(Vector2f){0,0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,+1,0},
                                   &(Vector2f){0,1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,+1,0},
                                   &(Vector2f){1,1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,-1,0},
                                   &(Vector2f){1,0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    sys_texture_apply(0, 0);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 255,255,255,255, (i/4) % TESTW, (i/4) / TESTW);
    }
    mem_free(pixels);

    CHECK_FALSE(sys_texture_lock(texture, SYS_TEXTURE_LOCK_NORMAL,
                                 0, 0, 2, 1));

    /* sys_texture_flush() is currently a no-op for OpenGL, but call it
     * anyway to verify safety against invalidated objects just in case we
     * add something later. */
    sys_texture_flush(texture);

    /* Check that destroying an invalidated texture doesn't affect a
     * second texture with the same OpenGL ID. */
    sys_texture_destroy(texture);
    sys_texture_apply(0, texture2);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,-1,0},
                                   &(Vector2f){0,0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,+1,0},
                                   &(Vector2f){0,1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,+1,0},
                                   &(Vector2f){1,1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+1,-1,0},
                                   &(Vector2f){1,0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    sys_texture_apply(0, 0);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 204,153,102,255, (i/4) % TESTW, (i/4) / TESTW);
    }
    mem_free(pixels);

    /* Check safety of texture_unlock() on a texture which was locked when
     * state loss occurred. */
    CHECK_TRUE(sys_texture_lock(texture2, SYS_TEXTURE_LOCK_NORMAL,
                                0, 0, 1, 1));
    graphics_finish_frame();
    force_close_window();
    ASSERT(open_window(TESTW, TESTH));
    graphics_start_frame();
    sys_texture_unlock(texture2, 1);

    sys_texture_destroy(texture2);
    return 1;
}

/*************************************************************************/
/*************************************************************************/

