/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/psp/graphics.c: PSP-specific graphics functionality tests.
 */

#include "src/base.h"
#include "src/framebuffer.h"
#include "src/graphics.h"
#include "src/math.h"
#include "src/sysdep.h"
#include "src/sysdep/psp/internal.h"
#include "src/sysdep/psp/texture.h"
#include "src/test/base.h"
#include "src/texture.h"

/*************************************************************************/
/***************** Test runner and init/cleanup routines *****************/
/*************************************************************************/

static int do_test_psp_graphics(void);
int test_psp_graphics(void)
{
    ASSERT(graphics_init());
    ASSERT(graphics_set_display_mode(480, 272, NULL));
    const int result = do_test_psp_graphics();
    graphics_cleanup();
    return result;
}

DEFINE_GENERIC_TEST_RUNNER(do_test_psp_graphics)

/*-----------------------------------------------------------------------*/

/* To avoid unnecessary delays from reinitializing the graphics framework
 * for every test, we allow tests to specify initialization behavior by
 * including specific tokens in the function name: "_REINIT" to force a
 * cleanup/init sequence before starting the test, and "_NOFRAME" to
 * suppress the graphics_start_frame()/graphics_finish_frame() calls
 * before and after the test. */

TEST_INIT(init)
{
    if (strstr(CURRENT_TEST_NAME(), "_REINIT")) {
        graphics_cleanup();
        ASSERT(graphics_init());
        ASSERT(graphics_set_display_mode(480, 272, NULL));
    }
    if (!strstr(CURRENT_TEST_NAME(), "_NOFRAME")) {
        graphics_start_frame();
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
    if (!strstr(CURRENT_TEST_NAME(), "_NOFRAME")) {
        graphics_finish_frame();
        graphics_flush_resources();
    }
    return 1;
}

/*************************************************************************/
/******************** Test routines: VRAM allocation *********************/
/*************************************************************************/

TEST(test_vram_alloc)
{
    /* Of the PSP's 2MB (0x200000 bytes) of VRAM, the two display
     * framebuffers take up 512*272*4 = 0x88000 bytes each, and the depth
     * buffer takes up 512*272*2 = 0x44000 bytes, for a total of 0x154000
     * bytes used. */
    const int vram_used = 0x154000;
    const int vram_free = 0x200000 - vram_used;

    /* Check that we can't allocate more than available. */
    CHECK_FALSE(psp_vram_alloc(vram_free+1, 0));

    /* Check that we can allocate exactly the available amount. */
    void *ptr;
    CHECK_TRUE(ptr = psp_vram_alloc(vram_free, 0));

    /* Check that we can free and reallocate the memory. */
    psp_vram_free(ptr);
    CHECK_TRUE(ptr = psp_vram_alloc(vram_free, 0));

    psp_vram_free(ptr);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_vram_alloc_partial)
{
    const int vram_used = 0x154000;
    const int vram_free = 0x200000 - vram_used;

    /* Check that we can make multiple partial allocations of VRAM. */
    void *ptr1, *ptr2, *ptr3;
    CHECK_TRUE(ptr1 = psp_vram_alloc(0x10000, 0));
    CHECK_TRUE(ptr2 = psp_vram_alloc(0x20000, 0));
    CHECK_TRUE(ptr3 = psp_vram_alloc(vram_free - 0x30000, 0));

    /* Check that there's no memory left to allocate. */
    CHECK_FALSE(psp_vram_alloc(1, 0));

    /* Check that we can free (in any order) and reallocate the memory. */
    psp_vram_free(ptr3);
    psp_vram_free(ptr1);
    psp_vram_free(ptr2);
    CHECK_TRUE(ptr1 = psp_vram_alloc(0x18000, 0));
    CHECK_TRUE(ptr2 = psp_vram_alloc(0x28000, 0));
    CHECK_TRUE(ptr3 = psp_vram_alloc(vram_free - 0x40000, 0));

    /* Check that we can free the memory and reallocate it as a single block. */
    psp_vram_free(ptr2);
    psp_vram_free(ptr3);
    psp_vram_free(ptr1);
    CHECK_TRUE(ptr1 = psp_vram_alloc(vram_free, 0));

    psp_vram_free(ptr1);

    /* Since we will have expanded the VRAM block list with the allocations
     * above, we need to explicitly flush resources before returning to
     * avoid the test framework detecting a memory leak. */
    graphics_flush_resources();
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_vram_alloc_align_NOFRAME)
{
    /* Check that the requested alignment is honored. */
    void *ptr;
    CHECK_TRUE(ptr = psp_vram_alloc(0x10000, 0x10000));
    CHECK_PTREQUAL(ptr, (void *)0x4160000);

    /* Check that subsequent allocations can still use the part of VRAM
     * skipped over by the allocated allocation. */
    void *ptr2;
    CHECK_TRUE(ptr2 = psp_vram_alloc(1, 0));
    CHECK_PTREQUAL(ptr2, (void *)0x4154000);
    psp_vram_free(ptr2);

    /* Check that after freeing, the entire spare VRAM area can still be
     * allocated. */
    psp_vram_free(ptr);
    CHECK_TRUE(ptr = psp_vram_alloc(0x200000 - 0x154000, 0));
    CHECK_FALSE(psp_vram_alloc(1, 0));
    psp_vram_free(ptr);

    graphics_flush_resources();
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_vram_alloc_align_invalid_NOFRAME)
{
    CHECK_FALSE(psp_vram_alloc(1, 65));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_vram_alloc_size_zero_NOFRAME)
{
    const int vram_used = 0x154000;
    const int vram_free = 0x200000 - vram_used;

    void *ptr;
    CHECK_TRUE(ptr = psp_vram_alloc(0, 0));
    psp_vram_free(ptr);
    CHECK_TRUE(ptr = psp_vram_alloc(vram_free, 0));
    psp_vram_free(ptr);

    graphics_flush_resources();
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_vram_alloc_array_expand_failure_NOFRAME)
{
    TEST_mem_fail_after(0, 1, 0);
    const void *result = psp_vram_alloc(0, 0);
    TEST_mem_fail_after(-1, 0, 0);
    CHECK_FALSE(result);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_vram_free_null_NOFRAME)
{
    psp_vram_free(NULL);  // Just make sure it doesn't crash.
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_vram_free_double_NOFRAME)
{
    const int vram_used = 0x154000;
    const int vram_free = 0x200000 - vram_used;

    void *ptr;
    CHECK_TRUE(ptr = psp_vram_alloc(vram_free, 0));
    psp_vram_free(ptr);
    psp_vram_free(ptr);  // Make sure it doesn't crash.
    /* Make sure we can't now allocate more memory than is really available. */
    CHECK_TRUE(ptr = psp_vram_alloc(vram_free, 0));
    CHECK_FALSE(psp_vram_alloc(1, 0));

    psp_vram_free(ptr);
    graphics_flush_resources();
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_vram_free_invalid_NOFRAME)
{
    char ch;
    psp_vram_free(&ch);  // Just make sure it doesn't crash.
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_free_depth_buffer_NOFRAME)
{
    /* If we set the depth_bits display attribute to zero, the depth buffer
     * should be immediately freed, leaving the associated VRAM available
     * for use by client code. */
    CHECK_TRUE(graphics_set_display_attr("depth_bits", 0));
    const int vram_used = 0x110000;
    const int vram_free = 0x200000 - vram_used;

    CHECK_FALSE(psp_vram_alloc(vram_free+1, 0));

    void *ptr;
    CHECK_TRUE(ptr = psp_vram_alloc(vram_free, 0));

    psp_vram_free(ptr);

    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(480*272*4, 0, 0));

    /* Rendering which doesn't use the depth buffer should still work. */
    graphics_set_viewport(0, 0, 4, 4);
    graphics_start_frame();
    graphics_clear(0, 0, 1, 0, 1, 0);
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    graphics_add_vertex(&(Vector3f){-1,-1,0}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){+1,-1,0}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){+1,+1,0}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){-1,+1,0}, NULL, NULL);
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_TRUE(graphics_read_pixels(0, 0, 480, 272, pixels));
    for (int y = 0; y < 272; y++) {
        for (int x = 0; x < 480; x++) {
            if (x < 4 && y < 4) {
                CHECK_PIXEL(&pixels[(y*480+x)*4], 255,255,255,255, x, y);
            } else {
                CHECK_PIXEL(&pixels[(y*480+x)*4], 0,0,255,255, x, y);
            }
        }
    }
    graphics_finish_frame();

    /* Attempts to use the depth buffer should be ignored. */
    graphics_start_frame();
    graphics_enable_depth_test(1);
    graphics_enable_depth_write(1);
    graphics_clear(0, 0, 1, 0, 1, 0);
    graphics_set_depth_test_comparison(GRAPHICS_COMPARISON_LESS);
    graphics_set_fixed_color(&(Vector4f){1,0,0,1});
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    graphics_add_vertex(&(Vector3f){-1,-1,0}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){+1,-1,0}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){+1,+1,0}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){-1,+1,0}, NULL, NULL);
    CHECK_TRUE(graphics_end_and_draw_primitive());
    graphics_set_depth_test_comparison(GRAPHICS_COMPARISON_GREATER);
    graphics_set_fixed_color(&(Vector4f){1,1,0,1});
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    graphics_add_vertex(&(Vector3f){-1,-1,-1}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){+1,-1,-1}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){+1,+1,-1}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){-1,+1,-1}, NULL, NULL);
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_TRUE(graphics_read_pixels(0, 0, 4, 4, pixels));
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            CHECK_PIXEL(&pixels[(y*4+x)*4], 255,255,0,255, x, y);
        }
    }
    graphics_set_depth_test_comparison(GRAPHICS_COMPARISON_LESS);
    graphics_set_fixed_color(&(Vector4f){0,1,0,1});
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    graphics_add_vertex(&(Vector3f){-1,-1,+1}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){+1,-1,+1}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){+1,+1,+1}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){-1,+1,+1}, NULL, NULL);
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_TRUE(graphics_read_pixels(0, 0, 4, 4, pixels));
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            CHECK_PIXEL(&pixels[(y*4+x)*4], 0,255,0,255, x, y);
        }
    }
    graphics_finish_frame();

    /* Re-disabling the depth buffer should have no effect (and not crash). */
    CHECK_TRUE(graphics_set_display_attr("depth_bits", 0));
    graphics_start_frame();
    graphics_enable_depth_test(1);
    graphics_enable_depth_write(1);
    graphics_clear(0, 0, 1, 0, 1, 0);
    graphics_set_depth_test_comparison(GRAPHICS_COMPARISON_LESS);
    graphics_set_fixed_color(&(Vector4f){1,0,0,1});
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    graphics_add_vertex(&(Vector3f){-1,-1,0}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){+1,-1,0}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){+1,+1,0}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){-1,+1,0}, NULL, NULL);
    CHECK_TRUE(graphics_end_and_draw_primitive());
    graphics_set_depth_test_comparison(GRAPHICS_COMPARISON_GREATER);
    graphics_set_fixed_color(&(Vector4f){1,1,0,1});
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    graphics_add_vertex(&(Vector3f){-1,-1,-1}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){+1,-1,-1}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){+1,+1,-1}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){-1,+1,-1}, NULL, NULL);
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_TRUE(graphics_read_pixels(0, 0, 4, 4, pixels));
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            CHECK_PIXEL(&pixels[(y*4+x)*4], 255,255,0,255, x, y);
        }
    }
    graphics_set_depth_test_comparison(GRAPHICS_COMPARISON_LESS);
    graphics_set_fixed_color(&(Vector4f){0,1,0,1});
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    graphics_add_vertex(&(Vector3f){-1,-1,+1}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){+1,-1,+1}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){+1,+1,+1}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){-1,+1,+1}, NULL, NULL);
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_TRUE(graphics_read_pixels(0, 0, 4, 4, pixels));
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            CHECK_PIXEL(&pixels[(y*4+x)*4], 0,255,0,255, x, y);
        }
    }
    graphics_finish_frame();

    /* Re-enabling the depth buffer should immediately allow depth testing
     * to work. */
    CHECK_TRUE(graphics_set_display_attr("depth_bits", 16));
    graphics_start_frame();
    graphics_clear(0, 0, 1, 0, 1, 0);
    graphics_set_depth_test_comparison(GRAPHICS_COMPARISON_LESS);
    graphics_set_fixed_color(&(Vector4f){1,0,0,1});
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    graphics_add_vertex(&(Vector3f){-1,-1,0}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){+1,-1,0}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){+1,+1,0}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){-1,+1,0}, NULL, NULL);
    CHECK_TRUE(graphics_end_and_draw_primitive());
    graphics_set_depth_test_comparison(GRAPHICS_COMPARISON_GREATER);
    graphics_set_fixed_color(&(Vector4f){1,1,0,1});
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    graphics_add_vertex(&(Vector3f){-1,-1,-1}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){+1,-1,-1}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){+1,+1,-1}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){-1,+1,-1}, NULL, NULL);
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_TRUE(graphics_read_pixels(0, 0, 4, 4, pixels));
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            CHECK_PIXEL(&pixels[(y*4+x)*4], 255,0,0,255, x, y);
        }
    }
    graphics_set_depth_test_comparison(GRAPHICS_COMPARISON_LESS);
    graphics_set_fixed_color(&(Vector4f){0,1,0,1});
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    graphics_add_vertex(&(Vector3f){-1,-1,+1}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){+1,-1,+1}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){+1,+1,+1}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){-1,+1,+1}, NULL, NULL);
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_TRUE(graphics_read_pixels(0, 0, 4, 4, pixels));
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            CHECK_PIXEL(&pixels[(y*4+x)*4], 255,0,0,255, x, y);
        }
    }
    graphics_finish_frame();

    graphics_set_fixed_color(&(Vector4f){1,1,1,1});
    graphics_enable_depth_test(0);
    mem_free(pixels);
    graphics_flush_resources();
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_realloc_depth_buffer_NOFRAME)
{
    CHECK_TRUE(graphics_set_display_attr("depth_bits", 0));
    const int vram_used = 0x110000;
    const int vram_free = 0x200000 - vram_used;

    CHECK_FALSE(psp_vram_alloc(vram_free+1, 0));

    void *ptr;
    CHECK_TRUE(ptr = psp_vram_alloc(vram_free, 0));

    /* If we try to re-enable the depth buffer when no VRAM is available,
     * the attempt should fail. */
    CHECK_FALSE(graphics_set_display_attr("depth_bits", 16));

    /* If we free up enough memory for the depth buffer, the re-enable call
     * should succeed. */
    psp_vram_free(ptr);
    ptr = psp_vram_alloc(vram_free - 0x44000, 0);
    CHECK_TRUE(graphics_set_display_attr("depth_bits", 16));

    /* VRAM should now be full. */
    CHECK_FALSE(psp_vram_alloc(1, 0));

    psp_vram_free(ptr);
    graphics_flush_resources();
    return 1;
}

/*************************************************************************/
/****************** Test routines: Fast-blit primitives ******************/
/*************************************************************************/

TEST(test_blit_image)
{
    graphics_set_viewport(0, 0, 480, 272);

    int texture;
    ASSERT(texture = texture_create(256, 256, 0, 0));
    uint8_t *pixels;
    ASSERT(pixels = texture_lock(texture));
    for (int y = 0; y < 256; y++) {
        for (int x = 0; x < 256; x++) {
            pixels[(y*256+x)*4+0] = x;
            pixels[(y*256+x)*4+1] = y;
            pixels[(y*256+x)*4+2] = 0;
            pixels[(y*256+x)*4+3] = 255;
        }
    }
    texture_unlock(texture);
    texture_set_antialias(texture, 0);
    texture_apply(0, texture);

    const int x_base = (480-256)/2;
    const int y_base = (272-256)/2;
    const float x0 = (x_base/480.0f)*2 - 1;
    const float y0 = 1 - (y_base/272.0f)*2;
    const float x1 = -x0;
    const float y1 = -y0;
    graphics_clear(0, 0, 1, 0, 1, 0);
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    graphics_add_vertex(&(Vector3f){x0,y0,0}, &(Vector2f){0,0}, NULL);
    graphics_add_vertex(&(Vector3f){x1,y0,0}, &(Vector2f){1,0}, NULL);
    graphics_add_vertex(&(Vector3f){x1,y1,0}, &(Vector2f){1,1}, NULL);
    graphics_add_vertex(&(Vector3f){x0,y1,0}, &(Vector2f){0,1}, NULL);
    CHECK_TRUE(graphics_end_and_draw_primitive());

    ASSERT(pixels = mem_alloc(480*272*4, 0, 0));
    CHECK_TRUE(graphics_read_pixels(0, 0, 480, 272, pixels));
    for (int y = 0; y < 272; y++) {
        for (int x = 0; x < 480; x++) {
            if (x >= x_base && x < x_base+256
             && y >= y_base && y < y_base+256) {
                CHECK_PIXEL(&pixels[((271-y)*480+x)*4],
                            (x-x_base),(y-y_base),0,255, x, y);
            } else {
                CHECK_PIXEL(&pixels[((271-y)*480+x)*4], 0,0,255,255, x, y);
            }
        }
    }
    mem_free(pixels);

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_blit_image_8bpp)
{
    graphics_set_viewport(0, 0, 480, 272);

    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(256*4 + 256*256, 0, 0));
    for (int y = 0; y < 256; y++) {
        pixels[y*4+0] = 128;
        pixels[y*4+1] = y;
        pixels[y*4+2] = 0;
        pixels[y*4+3] = 255;
        memset(pixels + 256*4 + y*256, y, 256);
    }
    int texture;
    ASSERT(texture = texture_create_with_data(
               256, 256, pixels, TEX_FORMAT_PSP_PALETTE8_RGBA8888, 256, 0, 0));
    mem_free(pixels);
    texture_set_antialias(texture, 0);
    texture_apply(0, texture);

    const int x_base = (480-256)/2;
    const int y_base = (272-256)/2;
    const float x0 = (x_base/480.0f)*2 - 1;
    const float y0 = 1 - (y_base/272.0f)*2;
    const float x1 = x0 + (256/480.0f)*2;
    const float y1 = y0 - (256/272.0f)*2;
    graphics_clear(0, 0, 1, 0, 1, 0);
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    graphics_add_vertex(&(Vector3f){x0,y0,0}, &(Vector2f){0,0}, NULL);
    graphics_add_vertex(&(Vector3f){x1,y0,0}, &(Vector2f){1,0}, NULL);
    graphics_add_vertex(&(Vector3f){x1,y1,0}, &(Vector2f){1,1}, NULL);
    graphics_add_vertex(&(Vector3f){x0,y1,0}, &(Vector2f){0,1}, NULL);
    CHECK_TRUE(graphics_end_and_draw_primitive());

    ASSERT(pixels = mem_alloc(480*272*4, 0, 0));
    CHECK_TRUE(graphics_read_pixels(0, 0, 480, 272, pixels));
    for (int y = 0; y < 272; y++) {
        for (int x = 0; x < 480; x++) {
            if (x >= x_base && x < x_base+256
             && y >= y_base && y < y_base+256) {
                CHECK_PIXEL(&pixels[((271-y)*480+x)*4],
                            128,(y-y_base),0,255, x, y);
            } else {
                CHECK_PIXEL(&pixels[((271-y)*480+x)*4], 0,0,255,255, x, y);
            }
        }
    }
    mem_free(pixels);

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fill_box)
{
    graphics_set_viewport(0, 0, 480, 272);

    const int x_base = (480-256)/2;
    const int y_base = (272-256)/2;
    const float x0 = (x_base/480.0f)*2 - 1;
    const float y0 = 1 - (y_base/272.0f)*2;
    const float x1 = x0 + (256/480.0f)*2;
    const float y1 = y0 - (256/272.0f)*2;
    graphics_clear(0, 0, 1, 0, 1, 0);
    graphics_set_fixed_color(&(Vector4f){0.8, 0.4, 0.6, 0.5});
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    graphics_add_vertex(&(Vector3f){x0,y0,0}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){x1,y0,0}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){x1,y1,0}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){x0,y1,0}, NULL, NULL);
    CHECK_TRUE(graphics_end_and_draw_primitive());
    graphics_set_fixed_color(&(Vector4f){1, 1, 1, 1});

    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(480*272*4, 0, 0));
    CHECK_TRUE(graphics_read_pixels(0, 0, 480, 272, pixels));
    for (int y = 0; y < 272; y++) {
        for (int x = 0; x < 480; x++) {
            if (x >= x_base && x < x_base+256
             && y >= y_base && y < y_base+256) {
                CHECK_PIXEL(&pixels[((271-y)*480+x)*4], 102,51,204,255, x, y);
            } else {
                CHECK_PIXEL(&pixels[((271-y)*480+x)*4], 0,0,255,255, x, y);
            }
        }
    }
    mem_free(pixels);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fill_box_with_texture_applied)
{
    graphics_set_viewport(0, 0, 480, 272);

    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(256*4 + 256*256, 0, 0));
    for (int y = 0; y < 256; y++) {
        pixels[y*4+0] = 128;
        pixels[y*4+1] = y;
        pixels[y*4+2] = 0;
        pixels[y*4+3] = 255;
        memset(pixels + 256*4 + y*256, y, 256);
    }
    int texture;
    ASSERT(texture = texture_create_with_data(
               256, 256, pixels, TEX_FORMAT_PSP_PALETTE8_RGBA8888, 256, 0, 0));
    mem_free(pixels);
    texture_set_antialias(texture, 0);
    texture_apply(0, texture);

    const int x_base = (480-256)/2;
    const int y_base = (272-256)/2;
    const float x0 = (x_base/480.0f)*2 - 1;
    const float y0 = 1 - (y_base/272.0f)*2;
    const float x1 = x0 + (256/480.0f)*2;
    const float y1 = y0 - (256/272.0f)*2;
    graphics_clear(0, 0, 1, 0, 1, 0);
    graphics_set_fixed_color(&(Vector4f){0.8, 0.4, 0.6, 0.5});
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    graphics_add_vertex(&(Vector3f){x0,y0,0}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){x1,y0,0}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){x1,y1,0}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){x0,y1,0}, NULL, NULL);
    CHECK_TRUE(graphics_end_and_draw_primitive());
    graphics_set_fixed_color(&(Vector4f){1, 1, 1, 1});

    ASSERT(pixels = mem_alloc(480*272*4, 0, 0));
    CHECK_TRUE(graphics_read_pixels(0, 0, 480, 272, pixels));
    for (int y = 0; y < 272; y++) {
        for (int x = 0; x < 480; x++) {
            if (x >= x_base && x < x_base+256
             && y >= y_base && y < y_base+256) {
                CHECK_PIXEL(&pixels[((271-y)*480+x)*4], 102,51,204,255, x, y);
            } else {
                CHECK_PIXEL(&pixels[((271-y)*480+x)*4], 0,0,255,255, x, y);
            }
        }
    }
    mem_free(pixels);

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_blit_inverted_coord_order)
{
    graphics_set_viewport(0, 0, 480, 272);

    int texture;
    ASSERT(texture = texture_create(256, 256, 0, 0));
    uint8_t *pixels;
    ASSERT(pixels = texture_lock(texture));
    for (int y = 0; y < 256; y++) {
        for (int x = 0; x < 256; x++) {
            pixels[(y*256+x)*4+0] = x;
            pixels[(y*256+x)*4+1] = y;
            pixels[(y*256+x)*4+2] = 0;
            pixels[(y*256+x)*4+3] = 255;
        }
    }
    texture_unlock(texture);
    texture_set_antialias(texture, 0);
    texture_apply(0, texture);

    const int x_base = (480-256)/2;
    const int y_base = (272-256)/2;
    const float x0 = (x_base/480.0f)*2 - 1;
    const float y0 = 1 - (y_base/272.0f)*2;
    const float x1 = -x0;
    const float y1 = -y0;
    ASSERT(pixels = mem_alloc(480*272*4, 0, 0));

    graphics_clear(0, 0, 1, 0, 1, 0);
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    graphics_add_vertex(&(Vector3f){x1,y0,0}, &(Vector2f){1,0}, NULL);
    graphics_add_vertex(&(Vector3f){x0,y0,0}, &(Vector2f){0,0}, NULL);
    graphics_add_vertex(&(Vector3f){x0,y1,0}, &(Vector2f){0,1}, NULL);
    graphics_add_vertex(&(Vector3f){x1,y1,0}, &(Vector2f){1,1}, NULL);
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_TRUE(graphics_read_pixels(0, 0, 480, 272, pixels));
    for (int y = 0; y < 272; y++) {
        for (int x = 0; x < 480; x++) {
            if (x >= x_base && x < x_base+256
             && y >= y_base && y < y_base+256) {
                CHECK_PIXEL(&pixels[((271-y)*480+x)*4],
                            (x-x_base),(y-y_base),0,255, x, y);
            } else {
                CHECK_PIXEL(&pixels[((271-y)*480+x)*4], 0,0,255,255, x, y);
            }
        }
    }

    graphics_clear(0, 0, 1, 0, 1, 0);
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    graphics_add_vertex(&(Vector3f){x0,y1,0}, &(Vector2f){0,1}, NULL);
    graphics_add_vertex(&(Vector3f){x1,y1,0}, &(Vector2f){1,1}, NULL);
    graphics_add_vertex(&(Vector3f){x1,y0,0}, &(Vector2f){1,0}, NULL);
    graphics_add_vertex(&(Vector3f){x0,y0,0}, &(Vector2f){0,0}, NULL);
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_TRUE(graphics_read_pixels(0, 0, 480, 272, pixels));
    for (int y = 0; y < 272; y++) {
        for (int x = 0; x < 480; x++) {
            if (x >= x_base && x < x_base+256
             && y >= y_base && y < y_base+256) {
                CHECK_PIXEL(&pixels[((271-y)*480+x)*4],
                            (x-x_base),(y-y_base),0,255, x, y);
            } else {
                CHECK_PIXEL(&pixels[((271-y)*480+x)*4], 0,0,255,255, x, y);
            }
        }
    }

    graphics_clear(0, 0, 1, 0, 1, 0);
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    graphics_add_vertex(&(Vector3f){x1,y1,0}, &(Vector2f){1,1}, NULL);
    graphics_add_vertex(&(Vector3f){x0,y1,0}, &(Vector2f){0,1}, NULL);
    graphics_add_vertex(&(Vector3f){x0,y0,0}, &(Vector2f){0,0}, NULL);
    graphics_add_vertex(&(Vector3f){x1,y0,0}, &(Vector2f){1,0}, NULL);
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_TRUE(graphics_read_pixels(0, 0, 480, 272, pixels));
    for (int y = 0; y < 272; y++) {
        for (int x = 0; x < 480; x++) {
            if (x >= x_base && x < x_base+256
             && y >= y_base && y < y_base+256) {
                CHECK_PIXEL(&pixels[((271-y)*480+x)*4],
                            (x-x_base),(y-y_base),0,255, x, y);
            } else {
                CHECK_PIXEL(&pixels[((271-y)*480+x)*4], 0,0,255,255, x, y);
            }
        }
    }

    mem_free(pixels);
    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fill_box_inverted_coord_order)
{
    graphics_set_viewport(0, 0, 480, 272);

    const int x_base = (480-256)/2;
    const int y_base = (272-256)/2;
    const float x0 = (x_base/480.0f)*2 - 1;
    const float y0 = 1 - (y_base/272.0f)*2;
    const float x1 = x0 + (256/480.0f)*2;
    const float y1 = y0 - (256/272.0f)*2;
    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(480*272*4, 0, 0));

    graphics_clear(0, 0, 1, 0, 1, 0);
    graphics_set_fixed_color(&(Vector4f){0.8, 0.4, 0.6, 0.5});
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    graphics_add_vertex(&(Vector3f){x1,y0,0}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){x0,y0,0}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){x0,y1,0}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){x1,y1,0}, NULL, NULL);
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_TRUE(graphics_read_pixels(0, 0, 480, 272, pixels));
    for (int y = 0; y < 272; y++) {
        for (int x = 0; x < 480; x++) {
            if (x >= x_base && x < x_base+256
             && y >= y_base && y < y_base+256) {
                CHECK_PIXEL(&pixels[((271-y)*480+x)*4], 102,51,204,255, x, y);
            } else {
                CHECK_PIXEL(&pixels[((271-y)*480+x)*4], 0,0,255,255, x, y);
            }
        }
    }

    graphics_clear(0, 0, 1, 0, 1, 0);
    graphics_set_fixed_color(&(Vector4f){0.8, 0.4, 0.6, 0.5});
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    graphics_add_vertex(&(Vector3f){x0,y1,0}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){x1,y1,0}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){x1,y0,0}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){x0,y0,0}, NULL, NULL);
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_TRUE(graphics_read_pixels(0, 0, 480, 272, pixels));
    for (int y = 0; y < 272; y++) {
        for (int x = 0; x < 480; x++) {
            if (x >= x_base && x < x_base+256
             && y >= y_base && y < y_base+256) {
                CHECK_PIXEL(&pixels[((271-y)*480+x)*4], 102,51,204,255, x, y);
            } else {
                CHECK_PIXEL(&pixels[((271-y)*480+x)*4], 0,0,255,255, x, y);
            }
        }
    }

    graphics_clear(0, 0, 1, 0, 1, 0);
    graphics_set_fixed_color(&(Vector4f){0.8, 0.4, 0.6, 0.5});
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    graphics_add_vertex(&(Vector3f){x1,y1,0}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){x0,y1,0}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){x0,y0,0}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){x1,y0,0}, NULL, NULL);
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_TRUE(graphics_read_pixels(0, 0, 480, 272, pixels));
    for (int y = 0; y < 272; y++) {
        for (int x = 0; x < 480; x++) {
            if (x >= x_base && x < x_base+256
             && y >= y_base && y < y_base+256) {
                CHECK_PIXEL(&pixels[((271-y)*480+x)*4], 102,51,204,255, x, y);
            } else {
                CHECK_PIXEL(&pixels[((271-y)*480+x)*4], 0,0,255,255, x, y);
            }
        }
    }

    mem_free(pixels);
    graphics_set_fixed_color(&(Vector4f){1, 1, 1, 1});
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_blit_image_off_screen_edge_high)
{
    graphics_set_viewport(0, 0, 480, 272);

    int texture;
    ASSERT(texture = texture_create(256, 256, 0, 0));
    uint8_t *pixels;
    ASSERT(pixels = texture_lock(texture));
    for (int y = 0; y < 256; y++) {
        for (int x = 0; x < 256; x++) {
            pixels[(y*256+x)*4+0] = x;
            pixels[(y*256+x)*4+1] = y;
            pixels[(y*256+x)*4+2] = 0;
            pixels[(y*256+x)*4+3] = 255;
        }
    }
    texture_unlock(texture);
    texture_set_antialias(texture, 0);
    texture_apply(0, texture);

    const int x_base = (480-256)/2 + 256;
    const int y_base = (272-256)/2 + 128;
    const float x0 = (x_base/480.0f)*2 - 1;
    const float y0 = 1 - (y_base/272.0f)*2;
    const float x1 = x0 + (256/480.0f)*2;
    const float y1 = y0 - (256/272.0f)*2;
    graphics_clear(0, 0, 1, 0, 1, 0);
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    graphics_add_vertex(&(Vector3f){x0,y0,0}, &(Vector2f){0,0}, NULL);
    graphics_add_vertex(&(Vector3f){x1,y0,0}, &(Vector2f){1,0}, NULL);
    graphics_add_vertex(&(Vector3f){x1,y1,0}, &(Vector2f){1,1}, NULL);
    graphics_add_vertex(&(Vector3f){x0,y1,0}, &(Vector2f){0,1}, NULL);
    CHECK_TRUE(graphics_end_and_draw_primitive());

    ASSERT(pixels = mem_alloc(480*272*4, 0, 0));
    CHECK_TRUE(graphics_read_pixels(0, 0, 480, 272, pixels));
    for (int y = 0; y < 272; y++) {
        for (int x = 0; x < 480; x++) {
            if (x >= x_base && x < x_base+256
             && y >= y_base && y < y_base+256) {
                CHECK_PIXEL(&pixels[((271-y)*480+x)*4],
                            (x-x_base),(y-y_base),0,255, x, y);
            } else {
                CHECK_PIXEL(&pixels[((271-y)*480+x)*4], 0,0,255,255, x, y);
            }
        }
    }
    mem_free(pixels);

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_blit_image_off_screen_edge_low)
{
    graphics_set_viewport(0, 0, 480, 272);

    int texture;
    ASSERT(texture = texture_create(256, 256, 0, 0));
    uint8_t *pixels;
    ASSERT(pixels = texture_lock(texture));
    for (int y = 0; y < 256; y++) {
        for (int x = 0; x < 256; x++) {
            pixels[(y*256+x)*4+0] = x;
            pixels[(y*256+x)*4+1] = y;
            pixels[(y*256+x)*4+2] = 0;
            pixels[(y*256+x)*4+3] = 255;
        }
    }
    texture_unlock(texture);
    texture_set_antialias(texture, 0);
    texture_apply(0, texture);

    const int x_base = (480-256)/2 - 256;
    const int y_base = (272-256)/2 - 128;
    const float x0 = (x_base/480.0f)*2 - 1;
    const float y0 = 1 - (y_base/272.0f)*2;
    const float x1 = x0 + (256/480.0f)*2;
    const float y1 = y0 - (256/272.0f)*2;
    graphics_clear(0, 0, 1, 0, 1, 0);
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    graphics_add_vertex(&(Vector3f){x0,y0,0}, &(Vector2f){0,0}, NULL);
    graphics_add_vertex(&(Vector3f){x1,y0,0}, &(Vector2f){1,0}, NULL);
    graphics_add_vertex(&(Vector3f){x1,y1,0}, &(Vector2f){1,1}, NULL);
    graphics_add_vertex(&(Vector3f){x0,y1,0}, &(Vector2f){0,1}, NULL);
    CHECK_TRUE(graphics_end_and_draw_primitive());

    ASSERT(pixels = mem_alloc(480*272*4, 0, 0));
    CHECK_TRUE(graphics_read_pixels(0, 0, 480, 272, pixels));
    for (int y = 0; y < 272; y++) {
        for (int x = 0; x < 480; x++) {
            if (x >= x_base && x < x_base+256
             && y >= y_base && y < y_base+256) {
                CHECK_PIXEL(&pixels[((271-y)*480+x)*4],
                            (x-x_base),(y-y_base),0,255, x, y);
            } else {
                CHECK_PIXEL(&pixels[((271-y)*480+x)*4], 0,0,255,255, x, y);
            }
        }
    }
    mem_free(pixels);

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_blit_image_partly_clipped)
{
    graphics_set_viewport(0, 0, 480, 272);

    int texture;
    ASSERT(texture = texture_create(256, 256, 0, 0));
    uint8_t *pixels;
    ASSERT(pixels = texture_lock(texture));
    for (int y = 0; y < 256; y++) {
        for (int x = 0; x < 256; x++) {
            pixels[(y*256+x)*4+0] = x;
            pixels[(y*256+x)*4+1] = y;
            pixels[(y*256+x)*4+2] = 0;
            pixels[(y*256+x)*4+3] = 255;
        }
    }
    texture_unlock(texture);
    texture_set_antialias(texture, 0);
    texture_apply(0, texture);

    const int x_base = (480-256)/2;
    const int y_base = (272-256)/2;
    const float x0 = (x_base/480.0f)*2 - 1;
    const float y0 = 1 - (y_base/272.0f)*2;
    const float x1 = x0 + (256/480.0f)*2;
    const float y1 = y0 - (256/272.0f)*2;
    graphics_clear(0, 0, 1, 0, 1, 0);
    graphics_set_clip_region(0, 32, 240, 240);
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    graphics_add_vertex(&(Vector3f){x0,y0,0}, &(Vector2f){0,0}, NULL);
    graphics_add_vertex(&(Vector3f){x1,y0,0}, &(Vector2f){1,0}, NULL);
    graphics_add_vertex(&(Vector3f){x1,y1,0}, &(Vector2f){1,1}, NULL);
    graphics_add_vertex(&(Vector3f){x0,y1,0}, &(Vector2f){0,1}, NULL);
    CHECK_TRUE(graphics_end_and_draw_primitive());

    ASSERT(pixels = mem_alloc(480*272*4, 0, 0));
    CHECK_TRUE(graphics_read_pixels(0, 0, 480, 272, pixels));
    for (int y = 0; y < 272; y++) {
        for (int x = 0; x < 480; x++) {
            if (x >= x_base && x < 240
             && y >= y_base && y < 240) {
                CHECK_PIXEL(&pixels[((271-y)*480+x)*4],
                            (x-x_base),(y-y_base),0,255, x, y);
            } else {
                CHECK_PIXEL(&pixels[((271-y)*480+x)*4], 0,0,255,255, x, y);
            }
        }
    }
    mem_free(pixels);

    texture_destroy(texture);
    graphics_set_clip_region(0, 0, 0, 0);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_blit_image_fully_clipped)
{
    graphics_set_viewport(0, 0, 480, 272);

    int texture;
    ASSERT(texture = texture_create(256, 256, 0, 0));
    uint8_t *pixels;
    ASSERT(pixels = texture_lock(texture));
    for (int y = 0; y < 256; y++) {
        for (int x = 0; x < 256; x++) {
            pixels[(y*256+x)*4+0] = x;
            pixels[(y*256+x)*4+1] = y;
            pixels[(y*256+x)*4+2] = 0;
            pixels[(y*256+x)*4+3] = 255;
        }
    }
    texture_unlock(texture);
    texture_set_antialias(texture, 0);
    texture_apply(0, texture);

    const int x_base = (480-256)/2;
    const int y_base = (272-256)/2;
    const float x0 = (x_base/480.0f)*2 - 1;
    const float y0 = 1 - (y_base/272.0f)*2;
    const float x1 = x0 + (256/480.0f)*2;
    const float y1 = y0 - (256/272.0f)*2;
    graphics_clear(0, 0, 1, 0, 1, 0);
    /* Both of these should completely clip the quad. */
    graphics_set_clip_region(0, 32, 64, 240);
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    graphics_add_vertex(&(Vector3f){x0,y0,0}, &(Vector2f){0,0}, NULL);
    graphics_add_vertex(&(Vector3f){x1,y0,0}, &(Vector2f){1,0}, NULL);
    graphics_add_vertex(&(Vector3f){x1,y1,0}, &(Vector2f){1,1}, NULL);
    graphics_add_vertex(&(Vector3f){x0,y1,0}, &(Vector2f){0,1}, NULL);
    CHECK_TRUE(graphics_end_and_draw_primitive());
    graphics_set_clip_region(0, 268, 240, 4);
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    graphics_add_vertex(&(Vector3f){x0,y0,0}, &(Vector2f){0,0}, NULL);
    graphics_add_vertex(&(Vector3f){x1,y0,0}, &(Vector2f){1,0}, NULL);
    graphics_add_vertex(&(Vector3f){x1,y1,0}, &(Vector2f){1,1}, NULL);
    graphics_add_vertex(&(Vector3f){x0,y1,0}, &(Vector2f){0,1}, NULL);
    CHECK_TRUE(graphics_end_and_draw_primitive());

    ASSERT(pixels = mem_alloc(480*272*4, 0, 0));
    CHECK_TRUE(graphics_read_pixels(0, 0, 480, 272, pixels));
    for (int y = 0; y < 272; y++) {
        for (int x = 0; x < 480; x++) {
            CHECK_PIXEL(&pixels[((271-y)*480+x)*4], 0,0,255,255, x, y);
        }
    }
    mem_free(pixels);

    texture_destroy(texture);
    graphics_set_clip_region(0, 0, 0, 0);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_blit_conditions_shape)
{
    graphics_set_viewport(0, 0, 480, 272);
    graphics_set_fixed_color(&(Vector4f){0.8, 0.4, 0.6, 0.5});

    uint8_t pixels[48*48*4];

    const int x_base = 16;
    const int y_base = 16;
    const float x0 = (x_base/480.0f)*2 - 1;
    const float y0 = (y_base/272.0f)*2 - 1;
    const float x1 = x0 + (16/480.0f)*2;
    const float y1 = y0 + (16/272.0f)*2;
    const float base_vertices[8] = {x0,y0, x0,y1, x1,y1, x1,y0};
    float vertices[8];
    static const uint32_t format[] =
        {GRAPHICS_VERTEX_FORMAT(POSITION_2F, 0), 0};

    for (int i = 0; i < 8; i++) {
        DLOG("Altering coordinate %d", i);
        memcpy(vertices, base_vertices, sizeof(vertices));
        const int y_axis = (i%2 != 0);
        const int add = (i >= 3 && i <= 6);
        vertices[i] += (add ? 1 : -1) * ((16.0f / (y_axis ? 272 : 480)) * 2);
        graphics_clear(0, 0, 1, 0, 1, 0);
        graphics_draw_vertices(
            GRAPHICS_PRIMITIVE_QUADS, vertices, format, 2*sizeof(float), 4);
        CHECK_TRUE(graphics_read_pixels(0, 0, 48, 48, pixels));
        for (int y = 0; y < 48; y++) {
            for (int x = 0; x < 48; x++) {
                if ((x >= 16 && x < 32 && y >= 16 && y < 32)
                 || (i == 0 && x < 16 && y >= 16 && y < 17+x)
                 || (i == 1 && y < 16 && x >= 16 && x < 17+y)
                 || (i == 2 && x < 16 && y >= 32-x && y < 32)
                 || (i == 3 && y >= 32 && x >= 16 && x < 64-y)
                 || (i == 4 && x >= 32 && y >= x-15 && y < 32)
                 || (i == 5 && y >= 32 && x >= y-16 && x < 32)
                 || (i == 6 && x >= 32 && y >= 16 && y < 64-x)
                 || (i == 7 && y < 16 && x >= 32-y && x < 32)
                ) {
                    CHECK_PIXEL(&pixels[(y*48+x)*4], 102,51,204,255, x, y);
                } else {
                    CHECK_PIXEL(&pixels[(y*48+x)*4], 0,0,255,255, x, y);
                }
            }
        }

        DLOG("Reversing coordinate order");
        float temp;
        temp = vertices[2]; vertices[2] = vertices[6]; vertices[6] = temp;
        temp = vertices[3]; vertices[3] = vertices[7]; vertices[7] = temp;
        graphics_clear(0, 0, 1, 0, 1, 0);
        graphics_draw_vertices(
            GRAPHICS_PRIMITIVE_QUADS, vertices, format, 2*sizeof(float), 4);
        CHECK_TRUE(graphics_read_pixels(0, 0, 48, 48, pixels));
        for (int y = 0; y < 48; y++) {
            for (int x = 0; x < 48; x++) {
                if ((x >= 16 && x < 32 && y >= 16 && y < 32)
                 || (i == 0 && x < 16 && y >= 16 && y < 17+x)
                 || (i == 1 && y < 16 && x >= 16 && x < 17+y)
                 || (i == 2 && x < 16 && y >= 32-x && y < 32)
                 || (i == 3 && y >= 32 && x >= 16 && x < 64-y)
                 || (i == 4 && x >= 32 && y >= x-15 && y < 32)
                 || (i == 5 && y >= 32 && x >= y-16 && x < 32)
                 || (i == 6 && x >= 32 && y >= 16 && y < 64-x)
                 || (i == 7 && y < 16 && x >= 32-y && x < 32)
                ) {
                    CHECK_PIXEL(&pixels[(y*48+x)*4], 102,51,204,255, x, y);
                } else {
                    CHECK_PIXEL(&pixels[(y*48+x)*4], 0,0,255,255, x, y);
                }
            }
        }
    }

    graphics_clear(0, 0, 1, 0, 1, 0);
    memcpy(vertices, base_vertices, sizeof(vertices));
    vertices[2] -= (16/480.0f)*2;
    vertices[4] = vertices[0];
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, vertices, format, 2*sizeof(float), 4);
    CHECK_TRUE(graphics_read_pixels(0, 0, 48, 48, pixels));
    for (int y = 0; y < 48; y++) {
        for (int x = 0; x < 48; x++) {
            if (y >= 16 && y < 32 && x >= 32-y && x < 48-y) {
                CHECK_PIXEL(&pixels[(y*48+x)*4], 102,51,204,255, x, y);
            } else {
                CHECK_PIXEL(&pixels[(y*48+x)*4], 0,0,255,255, x, y);
            }
        }
    }

    graphics_clear(0, 0, 1, 0, 1, 0);
    memcpy(vertices, base_vertices, sizeof(vertices));
    vertices[5] = vertices[1];
    vertices[7] -= (16/272.0f)*2;
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, vertices, format, 2*sizeof(float), 4);
    CHECK_TRUE(graphics_read_pixels(0, 0, 48, 48, pixels));
    for (int y = 0; y < 48; y++) {
        for (int x = 0; x < 48; x++) {
            if (x >= 16 && x < 32 && y >= 32-x && y < 48-x) {
                CHECK_PIXEL(&pixels[(y*48+x)*4], 102,51,204,255, x, y);
            } else {
                CHECK_PIXEL(&pixels[(y*48+x)*4], 0,0,255,255, x, y);
            }
        }
    }

    graphics_set_fixed_color(&(Vector4f){1, 1, 1, 1});
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_blit_conditions_texture_coords)
{
    graphics_set_viewport(0, 0, 480, 272);

    uint8_t pixels[48*48*4];

    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            pixels[(y*16+x)*4+0] = x*16;
            pixels[(y*16+x)*4+1] = y*16;
            pixels[(y*16+x)*4+2] = 0;
            pixels[(y*16+x)*4+3] = 255;
        }
    }
    int texture;
    ASSERT(texture = texture_create_with_data(
               16, 16, pixels, TEX_FORMAT_RGBA8888, 16, 0, 0));
    texture_set_antialias(texture, 0);
    texture_apply(0, texture);

    const int x_base = 16;
    const int y_base = 16;
    const float x0 = (x_base/480.0f)*2 - 1;
    const float y0 = (y_base/272.0f)*2 - 1;
    const float x1 = x0 + (16/480.0f)*2;
    const float y1 = y0 + (16/272.0f)*2;
    const float base_vertices[16] =
        {x0,y0,0,0, x0,y1,0,1, x1,y1,1,1, x1,y0,1,0};
    float vertices[16];
    static const uint8_t adjacent[8] = {6,3,4,1,2,7,0,5};
    static const uint32_t format[] = {
        GRAPHICS_VERTEX_FORMAT(POSITION_2F, 0),
        GRAPHICS_VERTEX_FORMAT(TEXCOORD_2F, 2*sizeof(float)),
        0
    };

    for (int push = 1; push <= 2; push++) {
        DLOG("Pushing coordinates by %d", push);
        for (int i = 0; i < 8; i++) {
            DLOG("Altering coordinate %d", i);
            memcpy(vertices, base_vertices, sizeof(vertices));
            const int add = (i >= 3 && i <= 6);
            vertices[(i/2)*4 + (2+(i%2))] += (add ? push : -push);
            const int j = adjacent[i];
            vertices[(j/2)*4 + (2+(j%2))] += (add ? push : -push);
            graphics_clear(0, 0, 1, 0, 1, 0);
            graphics_draw_vertices(
                GRAPHICS_PRIMITIVE_QUADS, vertices, format, 4*sizeof(float), 4);
            CHECK_TRUE(graphics_read_pixels(0, 0, 48, 48, pixels));
            for (int y = 0; y < 48; y++) {
                for (int x = 0; x < 48; x++) {
                    if (x >= 16 && x < 32 && y >= 16 && y < 32) {
                        int u = x-16, v = y-16;
                        switch (i%4) {
                            case 0: u += v*push+(push-1); break;
                            case 1: v += u*push+(push-1); break;
                            case 2: u -= v*push+(push-1); break;
                            case 3: v -= u*push+(push-1); break;
                        }
                        CHECK_PIXEL(&pixels[(y*48+x)*4],
                                    (u&15)*16, (v&15)*16, 0, 255, x, y);
                    } else {
                        CHECK_PIXEL(&pixels[(y*48+x)*4], 0,0,255,255, x, y);
                    }
                }
            }
        }
    }

    const float x_5 = x0 + (8/480.0f)*2;
    const float y_5 = y0 + (8/272.0f)*2;
    const float half_vertices[16] =
        {x0,y0,0,0, x0,y_5,0,0.5, x_5,y_5,0.5,0.5, x_5,y0,0.5,0};
    DLOG("Pushing coordinates by 0.5");
    for (int i = 3; i < 7; i++) {
        DLOG("Altering coordinate %d", i);
        memcpy(vertices, half_vertices, sizeof(vertices));
        vertices[(i/2)*4 + (2+(i%2))] += 0.5f;
        const int j = adjacent[i];
        vertices[(j/2)*4 + (2+(j%2))] += 0.5f;
        graphics_clear(0, 0, 1, 0, 1, 0);
        graphics_draw_vertices(
            GRAPHICS_PRIMITIVE_QUADS, vertices, format, 4*sizeof(float), 4);
        CHECK_TRUE(graphics_read_pixels(0, 0, 32, 32, pixels));
        for (int y = 0; y < 32; y++) {
            for (int x = 0; x < 32; x++) {
                if (x >= 16 && x < 24 && y >= 16 && y < 24) {
                    int u = x-16, v = y-16;
                    switch (i%4) {
                        case 0: u += v; break;
                        case 1: v += u; break;
                        case 2: u -= v+8; break;
                        case 3: v -= u+8; break;
                    }
                    CHECK_PIXEL(&pixels[(y*32+x)*4],
                                (u&15)*16, (v&15)*16, 0, 255, x, y);
                } else {
                    CHECK_PIXEL(&pixels[(y*32+x)*4], 0,0,255,255, x, y);
                }
            }
        }

        DLOG("Reversing coordinate order");
        float temp;
        temp = vertices[4]; vertices[4] = vertices[12]; vertices[12] = temp;
        temp = vertices[5]; vertices[5] = vertices[13]; vertices[13] = temp;
        temp = vertices[6]; vertices[6] = vertices[14]; vertices[14] = temp;
        temp = vertices[7]; vertices[7] = vertices[15]; vertices[15] = temp;
        graphics_clear(0, 0, 1, 0, 1, 0);
        graphics_draw_vertices(
            GRAPHICS_PRIMITIVE_QUADS, vertices, format, 4*sizeof(float), 4);
        CHECK_TRUE(graphics_read_pixels(0, 0, 32, 32, pixels));
        for (int y = 0; y < 32; y++) {
            for (int x = 0; x < 32; x++) {
                if (x >= 16 && x < 24 && y >= 16 && y < 24) {
                    int u = x-16, v = y-16;
                    switch (i%4) {
                        case 0: u += v; break;
                        case 1: v += u; break;
                        case 2: u -= v+8; break;
                        case 3: v -= u+8; break;
                    }
                    CHECK_PIXEL(&pixels[(y*32+x)*4],
                                (u&15)*16, (v&15)*16, 0, 255, x, y);
                } else {
                    CHECK_PIXEL(&pixels[(y*32+x)*4], 0,0,255,255, x, y);
                }
            }
        }
    }

    {
        memcpy(vertices, half_vertices, sizeof(vertices));
        vertices[14] -= 0.25f;
        graphics_clear(0, 0, 1, 0, 1, 0);
        graphics_draw_vertices(
            GRAPHICS_PRIMITIVE_QUADS, vertices, format, 4*sizeof(float), 4);
        CHECK_TRUE(graphics_read_pixels(0, 0, 32, 32, pixels));
        for (int y = 0; y < 32; y++) {
            for (int x = 0; x < 32; x++) {
                if (x >= 16 && x < 24 && y >= 16 && y < 24) {
                    int u = x-16, v = y-16;
                    u -= (u>=1 && v<7) + (u>=3 && v<5) + (u>=5 && v<3) + (u>=7 && v<1);
                    CHECK_PIXEL(&pixels[(y*32+x)*4], u*16,v*16,0,255, x, y);
                } else {
                    CHECK_PIXEL(&pixels[(y*32+x)*4], 0,0,255,255, x, y);
                }
            }
        }
    }

    {
        memcpy(vertices, half_vertices, sizeof(vertices));
        vertices[6] += 0.25f;
        float temp;
        temp = vertices[4]; vertices[4] = vertices[12]; vertices[12] = temp;
        temp = vertices[5]; vertices[5] = vertices[13]; vertices[13] = temp;
        temp = vertices[6]; vertices[6] = vertices[14]; vertices[14] = temp;
        temp = vertices[7]; vertices[7] = vertices[15]; vertices[15] = temp;
        graphics_clear(0, 0, 1, 0, 1, 0);
        graphics_draw_vertices(
            GRAPHICS_PRIMITIVE_QUADS, vertices, format, 4*sizeof(float), 4);
        CHECK_TRUE(graphics_read_pixels(0, 0, 32, 32, pixels));
        for (int y = 0; y < 32; y++) {
            for (int x = 0; x < 32; x++) {
                if (x >= 16 && x < 24 && y >= 16 && y < 24) {
                    int u = x-16, v = y-16;
                    u += (v>=1 && u<7) + (v>=3 && u<5) + (v>=5 && u<3) + (v>=7 && u<1);
                    CHECK_PIXEL(&pixels[(y*32+x)*4], u*16,v*16,0,255, x, y);
                } else {
                    CHECK_PIXEL(&pixels[(y*32+x)*4], 0,0,255,255, x, y);
                }
            }
        }
    }

    {
        memcpy(vertices, half_vertices, sizeof(vertices));
        vertices[15] += 0.25f;
        graphics_clear(0, 0, 1, 0, 1, 0);
        graphics_draw_vertices(
            GRAPHICS_PRIMITIVE_QUADS, vertices, format, 4*sizeof(float), 4);
        CHECK_TRUE(graphics_read_pixels(0, 0, 32, 32, pixels));
        for (int y = 0; y < 32; y++) {
            for (int x = 0; x < 32; x++) {
                if (x >= 16 && x < 24 && y >= 16 && y < 24) {
                    int u = x-16, v = y-16;
                    v += (u>=1 && v<7) + (u>=3 && v<5) + (u>=5 && v<3) + (u>=7 && v<1);
                    CHECK_PIXEL(&pixels[(y*32+x)*4], u*16,v*16,0,255, x, y);
                } else {
                    CHECK_PIXEL(&pixels[(y*32+x)*4], 0,0,255,255, x, y);
                }
            }
        }
    }

    {
        memcpy(vertices, half_vertices, sizeof(vertices));
        vertices[7] -= 0.25f;
        float temp;
        temp = vertices[4]; vertices[4] = vertices[12]; vertices[12] = temp;
        temp = vertices[5]; vertices[5] = vertices[13]; vertices[13] = temp;
        temp = vertices[6]; vertices[6] = vertices[14]; vertices[14] = temp;
        temp = vertices[7]; vertices[7] = vertices[15]; vertices[15] = temp;
        graphics_clear(0, 0, 1, 0, 1, 0);
        graphics_draw_vertices(
            GRAPHICS_PRIMITIVE_QUADS, vertices, format, 4*sizeof(float), 4);
        CHECK_TRUE(graphics_read_pixels(0, 0, 32, 32, pixels));
        for (int y = 0; y < 32; y++) {
            for (int x = 0; x < 32; x++) {
                if (x >= 16 && x < 24 && y >= 16 && y < 24) {
                    int u = x-16, v = y-16;
                    v -= (v>=1 && u<7) + (v>=3 && u<5) + (v>=5 && u<3) + (v>=7 && u<1);
                    CHECK_PIXEL(&pixels[(y*32+x)*4], u*16,v*16,0,255, x, y);
                } else {
                    CHECK_PIXEL(&pixels[(y*32+x)*4], 0,0,255,255, x, y);
                }
            }
        }
    }

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_blit_conditions_projection_matrix)
{
    graphics_set_viewport(0, 0, 480, 272);
    graphics_set_fixed_color(&(Vector4f){0.8, 0.4, 0.6, 0.5});

    uint8_t pixels[48*48*4];

    const int x_base = 16;
    const int y_base = 16;
    const float x0 = (x_base/480.0f)*2 - 1;
    const float y0 = (y_base/272.0f)*2 - 1;
    const float x1 = x0 + (16/480.0f)*2;
    const float y1 = y0 + (16/272.0f)*2;
    const float vertices[12] = {x0,y0,1, x0,y1,1, x1,y1,1, x1,y0,1};
    static const uint32_t format[] =
        {GRAPHICS_VERTEX_FORMAT(POSITION_3F, 0), 0};

    for (int i = 0; i < 16; i++) {
        DLOG("Setting matrix element %d", i);
        Matrix4f M = mat4_identity;
        (&M._11)[i] = 42;

        graphics_clear(0, 0, 1, 0, 1, 0);
        graphics_set_projection_matrix(&M);
        graphics_draw_vertices(
            GRAPHICS_PRIMITIVE_QUADS, vertices, format, 3*sizeof(float), 4);
        graphics_set_projection_matrix(&mat4_identity);
        CHECK_TRUE(graphics_read_pixels(0, 0, 48, 48, pixels));
        for (int y = 0; y < 48; y++) {
            for (int x = 0; x < 48; x++) {
                if ((i==10 || i==14) && x>=16 && x<32 && y>=16 && y<32) {
                    CHECK_PIXEL(&pixels[(y*48+x)*4], 102,51,204,255, x, y);
                } else {
                    CHECK_PIXEL(&pixels[(y*48+x)*4], 0,0,255,255, x, y);
                }
            }
        }
    }

    graphics_clear(0, 0, 1, 0, 1, 0);
    graphics_set_projection_matrix(
        &(Matrix4f){2,0,0,0, 0,2,0,0, 0,0,2,0, 1,1,1,1});
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, vertices, format, 3*sizeof(float), 4);
    graphics_set_projection_matrix(&mat4_identity);
    CHECK_TRUE(graphics_read_pixels(24, 24, 48, 48, pixels));
    for (int y = 0; y < 48; y++) {
        for (int x = 0; x < 48; x++) {
            if (x >= 8 && x < 40 && y >= 8 && y < 40) {
                CHECK_PIXEL(&pixels[(y*48+x)*4], 102,51,204,255, x, y);
            } else {
                CHECK_PIXEL(&pixels[(y*48+x)*4], 0,0,255,255, x, y);
            }
        }
    }

    graphics_set_fixed_color(&(Vector4f){1, 1, 1, 1});
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_blit_conditions_view_matrix)
{
    graphics_set_viewport(0, 0, 480, 272);
    graphics_set_fixed_color(&(Vector4f){0.8, 0.4, 0.6, 0.5});

    uint8_t pixels[48*48*4];

    const int x_base = 16;
    const int y_base = 16;
    const float x0 = (x_base/480.0f)*2 - 1;
    const float y0 = (y_base/272.0f)*2 - 1;
    const float x1 = x0 + (16/480.0f)*2;
    const float y1 = y0 + (16/272.0f)*2;
    const float vertices[12] = {x0,y0,1, x0,y1,1, x1,y1,1, x1,y0,1};
    static const uint32_t format[] =
        {GRAPHICS_VERTEX_FORMAT(POSITION_3F, 0), 0};

    for (int i = 0; i < 16; i++) {
        DLOG("Setting matrix element %d", i);
        Matrix4f M = mat4_identity;
        (&M._11)[i] = 42;

        graphics_clear(0, 0, 1, 0, 1, 0);
        graphics_set_view_matrix(&M);
        graphics_draw_vertices(
            GRAPHICS_PRIMITIVE_QUADS, vertices, format, 3*sizeof(float), 4);
        graphics_set_view_matrix(&mat4_identity);
        CHECK_TRUE(graphics_read_pixels(0, 0, 48, 48, pixels));
        for (int y = 0; y < 48; y++) {
            for (int x = 0; x < 48; x++) {
                if (i%4 == 3 && x >= 16 && x < 32 && y >= 16 && y < 32) {
                    CHECK_PIXEL(&pixels[(y*48+x)*4], 102,51,204,255, x, y);
                } else {
                    CHECK_PIXEL(&pixels[(y*48+x)*4], 0,0,255,255, x, y);
                }
            }
        }
    }

    graphics_clear(0, 0, 1, 0, 1, 0);
    graphics_set_view_matrix(
        &(Matrix4f){2,0,0,0, 0,2,0,0, 0,0,2,0, 1,1,-1,1});
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, vertices, format, 3*sizeof(float), 4);
    graphics_set_view_matrix(&mat4_identity);
    CHECK_TRUE(graphics_read_pixels(24, 24, 48, 48, pixels));
    for (int y = 0; y < 48; y++) {
        for (int x = 0; x < 48; x++) {
            if (x >= 8 && x < 40 && y >= 8 && y < 40) {
                CHECK_PIXEL(&pixels[(y*48+x)*4], 102,51,204,255, x, y);
            } else {
                CHECK_PIXEL(&pixels[(y*48+x)*4], 0,0,255,255, x, y);
            }
        }
    }

    graphics_set_fixed_color(&(Vector4f){1, 1, 1, 1});
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_blit_conditions_model_matrix)
{
    graphics_set_viewport(0, 0, 480, 272);
    graphics_set_fixed_color(&(Vector4f){0.8, 0.4, 0.6, 0.5});

    uint8_t pixels[48*48*4];

    const int x_base = 16;
    const int y_base = 16;
    const float x0 = (x_base/480.0f)*2 - 1;
    const float y0 = (y_base/272.0f)*2 - 1;
    const float x1 = x0 + (16/480.0f)*2;
    const float y1 = y0 + (16/272.0f)*2;
    const float vertices[12] = {x0,y0,1, x0,y1,1, x1,y1,1, x1,y0,1};
    static const uint32_t format[] =
        {GRAPHICS_VERTEX_FORMAT(POSITION_3F, 0), 0};

    for (int i = 0; i < 16; i++) {
        DLOG("Setting matrix element %d", i);
        Matrix4f M = mat4_identity;
        (&M._11)[i] = 42;

        graphics_clear(0, 0, 1, 0, 1, 0);
        graphics_set_model_matrix(&M);
        graphics_draw_vertices(
            GRAPHICS_PRIMITIVE_QUADS, vertices, format, 3*sizeof(float), 4);
        graphics_set_model_matrix(&mat4_identity);
        CHECK_TRUE(graphics_read_pixels(0, 0, 48, 48, pixels));
        for (int y = 0; y < 48; y++) {
            for (int x = 0; x < 48; x++) {
                if (i%4 == 3 && x >= 16 && x < 32 && y >= 16 && y < 32) {
                    CHECK_PIXEL(&pixels[(y*48+x)*4], 102,51,204,255, x, y);
                } else {
                    CHECK_PIXEL(&pixels[(y*48+x)*4], 0,0,255,255, x, y);
                }
            }
        }
    }

    graphics_clear(0, 0, 1, 0, 1, 0);
    graphics_set_model_matrix(
        &(Matrix4f){2,0,0,0, 0,2,0,0, 0,0,2,0, 1,1,-1,1});
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, vertices, format, 3*sizeof(float), 4);
    graphics_set_model_matrix(&mat4_identity);
    CHECK_TRUE(graphics_read_pixels(24, 24, 48, 48, pixels));
    for (int y = 0; y < 48; y++) {
        for (int x = 0; x < 48; x++) {
            if (x >= 8 && x < 40 && y >= 8 && y < 40) {
                CHECK_PIXEL(&pixels[(y*48+x)*4], 102,51,204,255, x, y);
            } else {
                CHECK_PIXEL(&pixels[(y*48+x)*4], 0,0,255,255, x, y);
            }
        }
    }

    graphics_set_fixed_color(&(Vector4f){1, 1, 1, 1});
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_blit_conditions_viewport)
{
    graphics_set_viewport(0, 0, 480, 272);
    graphics_set_fixed_color(&(Vector4f){0.8, 0.4, 0.6, 0.5});

    uint8_t pixels[48*48*4];

    const int x_base = 16;
    const int y_base = 16;
    const float x0 = (x_base/480.0f)*2 - 1;
    const float y0 = (y_base/272.0f)*2 - 1;
    const float x1 = x0 + (16/480.0f)*2;
    const float y1 = y0 + (16/272.0f)*2;
    const float vertices[12] = {x0,y0, x0,y1, x1,y1, x1,y0};
    static const uint32_t format[] =
        {GRAPHICS_VERTEX_FORMAT(POSITION_2F, 0), 0};

    graphics_clear(0, 0, 1, 0, 1, 0);
    graphics_set_viewport(0, 0, 240, 272);
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, vertices, format, 2*sizeof(float), 4);
    CHECK_TRUE(graphics_read_pixels(0, 0, 48, 48, pixels));
    for (int y = 0; y < 48; y++) {
        for (int x = 0; x < 48; x++) {
            if (x >= 8 && x < 16 && y >= 16 && y < 32) {
                CHECK_PIXEL(&pixels[(y*48+x)*4], 102,51,204,255, x, y);
            } else {
                CHECK_PIXEL(&pixels[(y*48+x)*4], 0,0,255,255, x, y);
            }
        }
    }

    graphics_clear(0, 0, 1, 0, 1, 0);
    graphics_set_viewport(0, 0, 480, 136);
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, vertices, format, 2*sizeof(float), 4);
    CHECK_TRUE(graphics_read_pixels(0, 0, 48, 48, pixels));
    for (int y = 0; y < 48; y++) {
        for (int x = 0; x < 48; x++) {
            if (x >= 16 && x < 32 && y >= 8 && y < 16) {
                CHECK_PIXEL(&pixels[(y*48+x)*4], 102,51,204,255, x, y);
            } else {
                CHECK_PIXEL(&pixels[(y*48+x)*4], 0,0,255,255, x, y);
            }
        }
    }

    graphics_set_fixed_color(&(Vector4f){1, 1, 1, 1});
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_blit_conditions_texture_fixed_color)
{
    graphics_set_viewport(0, 0, 480, 272);

    uint8_t pixels[48*48*4];

    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            pixels[(y*16+x)*4+0] = x*16;
            pixels[(y*16+x)*4+1] = y*16;
            pixels[(y*16+x)*4+2] = 0;
            pixels[(y*16+x)*4+3] = 255;
        }
    }
    int texture;
    ASSERT(texture = texture_create_with_data(
               16, 16, pixels, TEX_FORMAT_RGBA8888, 16, 0, 0));
    texture_set_antialias(texture, 0);
    texture_apply(0, texture);

    const int x_base = 16;
    const int y_base = 16;
    const float x0 = (x_base/480.0f)*2 - 1;
    const float y0 = (y_base/272.0f)*2 - 1;
    const float x1 = x0 + (16/480.0f)*2;
    const float y1 = y0 + (16/272.0f)*2;
    const float vertices[16] =
        {x0,y0,0,0, x0,y1,0,1, x1,y1,1,1, x1,y0,1,0};
    static const uint32_t format[] = {
        GRAPHICS_VERTEX_FORMAT(POSITION_2F, 0),
        GRAPHICS_VERTEX_FORMAT(TEXCOORD_2F, 2*sizeof(float)),
        0
    };

    graphics_clear(0, 0, 0.8, 0, 1, 0);
    graphics_set_fixed_color(&(Vector4f){0.25, 0.5, 0.75, 0.5});
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, vertices, format, 4*sizeof(float), 4);
    graphics_set_fixed_color(&(Vector4f){1, 1, 1, 1});
    CHECK_TRUE(graphics_read_pixels(0, 0, 48, 48, pixels));
    for (int y = 0; y < 48; y++) {
        for (int x = 0; x < 48; x++) {
            if (x >= 16 && x < 32 && y >= 16 && y < 32) {
                const int u = x-16, v = y-16;
                CHECK_PIXEL_NEAR(&pixels[(y*48+x)*4],
                                 u*2,v*4,102,255, 1, x, y);
            } else {
                CHECK_PIXEL(&pixels[(y*48+x)*4], 0,0,204,255, x, y);
            }
        }
    }

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_blit_conditions_texture_apply_0)
{
    graphics_set_viewport(0, 0, 480, 272);

    uint8_t pixels[48*48*4];

    texture_apply(0, 0);

    const int x_base = 16;
    const int y_base = 16;
    const float x0 = (x_base/480.0f)*2 - 1;
    const float y0 = (y_base/272.0f)*2 - 1;
    const float x1 = x0 + (16/480.0f)*2;
    const float y1 = y0 + (16/272.0f)*2;
    const float vertices[16] =
        {x0,y0,0,0, x0,y1,0,1, x1,y1,1,1, x1,y0,1,0};
    static const uint32_t format[] = {
        GRAPHICS_VERTEX_FORMAT(POSITION_2F, 0),
        GRAPHICS_VERTEX_FORMAT(TEXCOORD_2F, 2*sizeof(float)),
        0
    };

    graphics_clear(0, 0, 1, 0, 1, 0);
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, vertices, format, 4*sizeof(float), 4);
    CHECK_TRUE(graphics_read_pixels(0, 0, 48, 48, pixels));
    for (int y = 0; y < 48; y++) {
        for (int x = 0; x < 48; x++) {
            if (x >= 16 && x < 32 && y >= 16 && y < 32) {
                CHECK_PIXEL(&pixels[(y*48+x)*4], 255,255,255,255, x, y);
            } else {
                CHECK_PIXEL(&pixels[(y*48+x)*4], 0,0,255,255, x, y);
            }
        }
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_blit_conditions_texture_antialias)
{
    graphics_set_viewport(0, 0, 480, 272);

    uint8_t pixels[48*48*4];

    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            /* In this case we set a solid color because otherwise we get
             * smearing, which is annoying to test. */
            pixels[(y*16+x)*4+0] = 255;
            pixels[(y*16+x)*4+1] = 255;
            pixels[(y*16+x)*4+2] = 0;
            pixels[(y*16+x)*4+3] = 255;
        }
    }
    int texture;
    ASSERT(texture = texture_create_with_data(
               16, 16, pixels, TEX_FORMAT_RGBA8888, 16, 0, 0));
    texture_set_antialias(texture, 1);
    texture_apply(0, texture);

    const int x_base = 16;
    const int y_base = 16;
    const float x0 = (x_base/480.0f)*2 - 1;
    const float y0 = (y_base/272.0f)*2 - 1;
    const float x1 = x0 + (16/480.0f)*2;
    const float y1 = y0 + (16/272.0f)*2;
    const float vertices[16] =
        {x0,y0,0,0, x0,y1,0,1, x1,y1,1,1, x1,y0,1,0};
    static const uint32_t format[] = {
        GRAPHICS_VERTEX_FORMAT(POSITION_2F, 0),
        GRAPHICS_VERTEX_FORMAT(TEXCOORD_2F, 2*sizeof(float)),
        0
    };

    graphics_clear(0, 0, 1, 0, 1, 0);
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, vertices, format, 4*sizeof(float), 4);
    graphics_set_texture_offset(&(Vector2f){0, 0});
    CHECK_TRUE(graphics_read_pixels(0, 0, 48, 48, pixels));
    for (int y = 0; y < 48; y++) {
        for (int x = 0; x < 48; x++) {
            if (x >= 16 && x < 32 && y >= 16 && y < 32) {
                CHECK_PIXEL(&pixels[(y*48+x)*4], 255,255,0,255, x, y);
            } else {
                CHECK_PIXEL(&pixels[(y*48+x)*4], 0,0,255,255, x, y);
            }
        }
    }

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_blit_conditions_texture_format)
{
    graphics_set_viewport(0, 0, 480, 272);

    uint8_t pixels[48*48*4];

    uint16_t tex_data[16][16];
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            tex_data[y][x] = x<<1 | y<<7;
        }
    }
    int texture;
    ASSERT(texture = texture_create_with_data(
               16, 16, tex_data, TEX_FORMAT_RGB565, 16, 0, 0));
    texture_set_antialias(texture, 0);
    texture_apply(0, texture);

    const int x_base = 16;
    const int y_base = 16;
    const float x0 = (x_base/480.0f)*2 - 1;
    const float y0 = (y_base/272.0f)*2 - 1;
    const float x1 = x0 + (16/480.0f)*2;
    const float y1 = y0 + (16/272.0f)*2;
    const float vertices[16] =
        {x0,y0,0,0, x0,y1,0,1, x1,y1,1,1, x1,y0,1,0};
    static const uint32_t format[] = {
        GRAPHICS_VERTEX_FORMAT(POSITION_2F, 0),
        GRAPHICS_VERTEX_FORMAT(TEXCOORD_2F, 2*sizeof(float)),
        0
    };

    graphics_clear(0, 0, 1, 0, 1, 0);
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, vertices, format, 4*sizeof(float), 4);
    CHECK_TRUE(graphics_read_pixels(0, 0, 48, 48, pixels));
    for (int y = 0; y < 48; y++) {
        for (int x = 0; x < 48; x++) {
            if (x >= 16 && x < 32 && y >= 16 && y < 32) {
                const int u = x-16, v = y-16;
                CHECK_PIXEL(&pixels[(y*48+x)*4],
                            u<<4 | u>>1, v<<4 | v>>2, 0, 255, x, y);
            } else {
                CHECK_PIXEL(&pixels[(y*48+x)*4], 0,0,255,255, x, y);
            }
        }
    }

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_blit_conditions_texture_rect_size)
{
    graphics_set_viewport(0, 0, 480, 272);

    uint8_t pixels[48*48*4];

    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            pixels[(y*16+x)*4+0] = x*16;
            pixels[(y*16+x)*4+1] = y*16;
            pixels[(y*16+x)*4+2] = 0;
            pixels[(y*16+x)*4+3] = 255;
        }
    }
    int texture;
    ASSERT(texture = texture_create_with_data(
               16, 16, pixels, TEX_FORMAT_RGBA8888, 16, 0, 0));
    texture_set_antialias(texture, 0);
    texture_apply(0, texture);

    const int x_base = 16;
    const int y_base = 16;
    const float x0 = (x_base/480.0f)*2 - 1;
    const float y0 = (y_base/272.0f)*2 - 1;
    const float x1 = x0 + (16/480.0f)*2;
    const float y1 = y0 + (16/272.0f)*2;
    const float base_vertices[16] =
        {x0,y0,0,0, x0,y1,0,1, x1,y1,1,1, x1,y0,1,0};
    float vertices[16];
    static const uint32_t format[] = {
        GRAPHICS_VERTEX_FORMAT(POSITION_2F, 0),
        GRAPHICS_VERTEX_FORMAT(TEXCOORD_2F, 2*sizeof(float)),
        0
    };

    memcpy(vertices, base_vertices, sizeof(vertices));
    vertices[10] = vertices[14] = 0.5;
    graphics_clear(0, 0, 1, 0, 1, 0);
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, vertices, format, 4*sizeof(float), 4);
    graphics_set_texture_offset(&(Vector2f){0, 0});
    CHECK_TRUE(graphics_read_pixels(0, 0, 48, 48, pixels));
    for (int y = 0; y < 48; y++) {
        for (int x = 0; x < 48; x++) {
            if (x >= 16 && x < 32 && y >= 16 && y < 32) {
                int u = (x-16)/2, v = y-16;
                CHECK_PIXEL(&pixels[(y*48+x)*4], u*16,v*16,0,255, x, y);
            } else {
                CHECK_PIXEL(&pixels[(y*48+x)*4], 0,0,255,255, x, y);
            }
        }
    }

    memcpy(vertices, base_vertices, sizeof(vertices));
    vertices[7] = vertices[11] = 0.5;
    graphics_clear(0, 0, 1, 0, 1, 0);
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, vertices, format, 4*sizeof(float), 4);
    graphics_set_texture_offset(&(Vector2f){0, 0});
    CHECK_TRUE(graphics_read_pixels(0, 0, 48, 48, pixels));
    for (int y = 0; y < 48; y++) {
        for (int x = 0; x < 48; x++) {
            if (x >= 16 && x < 32 && y >= 16 && y < 32) {
                int u = x-16, v = (y-16)/2;
                CHECK_PIXEL(&pixels[(y*48+x)*4], u*16,v*16,0,255, x, y);
            } else {
                CHECK_PIXEL(&pixels[(y*48+x)*4], 0,0,255,255, x, y);
            }
        }
    }

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_blit_conditions_texture_offset)
{
    graphics_set_viewport(0, 0, 480, 272);

    uint8_t pixels[48*48*4];

    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            pixels[(y*16+x)*4+0] = x*16;
            pixels[(y*16+x)*4+1] = y*16;
            pixels[(y*16+x)*4+2] = 0;
            pixels[(y*16+x)*4+3] = 255;
        }
    }
    int texture;
    ASSERT(texture = texture_create_with_data(
               16, 16, pixels, TEX_FORMAT_RGBA8888, 16, 0, 0));
    texture_set_antialias(texture, 0);
    texture_apply(0, texture);

    const int x_base = 16;
    const int y_base = 16;
    const float x0 = (x_base/480.0f)*2 - 1;
    const float y0 = (y_base/272.0f)*2 - 1;
    const float x1 = x0 + (16/480.0f)*2;
    const float y1 = y0 + (16/272.0f)*2;
    const float vertices[16] =
        {x0,y0,0,0, x0,y1,0,1, x1,y1,1,1, x1,y0,1,0};
    static const uint32_t format[] = {
        GRAPHICS_VERTEX_FORMAT(POSITION_2F, 0),
        GRAPHICS_VERTEX_FORMAT(TEXCOORD_2F, 2*sizeof(float)),
        0
    };

    graphics_clear(0, 0, 1, 0, 1, 0);
    graphics_set_texture_offset(&(Vector2f){0.5, 0});
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, vertices, format, 4*sizeof(float), 4);
    graphics_set_texture_offset(&(Vector2f){0, 0});
    CHECK_TRUE(graphics_read_pixels(0, 0, 48, 48, pixels));
    for (int y = 0; y < 48; y++) {
        for (int x = 0; x < 48; x++) {
            if (x >= 16 && x < 32 && y >= 16 && y < 32) {
                int u = (x-16)+8, v = y-16;
                CHECK_PIXEL(&pixels[(y*48+x)*4], u*16,v*16,0,255, x, y);
            } else {
                CHECK_PIXEL(&pixels[(y*48+x)*4], 0,0,255,255, x, y);
            }
        }
    }

    graphics_clear(0, 0, 1, 0, 1, 0);
    graphics_set_texture_offset(&(Vector2f){0, 0.5});
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, vertices, format, 4*sizeof(float), 4);
    graphics_set_texture_offset(&(Vector2f){0, 0});
    CHECK_TRUE(graphics_read_pixels(0, 0, 48, 48, pixels));
    for (int y = 0; y < 48; y++) {
        for (int x = 0; x < 48; x++) {
            if (x >= 16 && x < 32 && y >= 16 && y < 32) {
                int u = x-16, v = (y-16)+8;
                CHECK_PIXEL(&pixels[(y*48+x)*4], u*16,v*16,0,255, x, y);
            } else {
                CHECK_PIXEL(&pixels[(y*48+x)*4], 0,0,255,255, x, y);
            }
        }
    }

    texture_destroy(texture);
    return 1;
}

/*************************************************************************/
/********************* Test routines: Miscellaneous **********************/
/*************************************************************************/

TEST(test_set_wrong_display_size)
{
    GraphicsError error;

    CHECK_FALSE(graphics_set_display_mode(448, 272, &error));
    CHECK_INTEQUAL(error, GRAPHICS_ERROR_MODE_NOT_SUPPORTED);
    CHECK_FALSE(graphics_set_display_mode(480, 256, &error));
    CHECK_INTEQUAL(error, GRAPHICS_ERROR_MODE_NOT_SUPPORTED);

    return 1;
}

/*-----------------------------------------------------------------------*/

/* Test for a former bug in which the very first primitive drawn after
 * graphics_init() would fail to render because the ambient alpha was set
 * to zero. */
TEST(test_first_frame_fixed_color_without_vertex_colors_REINIT)
{
    graphics_set_viewport(0, 0, 32, 32);

    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_set_fixed_color(&(Vector4f){0.8, 0.4, 0.667, 0.5});
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_TRIANGLES);
    graphics_add_vertex(&(Vector3f){-1,-1,0}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){+1,-1,0}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){-1,+1,0}, NULL, NULL);
    CHECK_TRUE(graphics_end_and_draw_primitive());

    uint8_t pixels[32*32*4];
    CHECK_TRUE(graphics_read_pixels(0, 0, 32, 32, pixels));
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            if (x+y < 31) {
                CHECK_PIXEL(&pixels[(y*32+x)*4], 102,51,85,255, x, y);
            } else {
                CHECK_PIXEL(&pixels[(y*32+x)*4], 0,0,0,255, x, y);
            }
        }
    }

    graphics_set_fixed_color(&(Vector4f){1, 1, 1, 1});
    return 1;
}

/*-----------------------------------------------------------------------*/

/* Make sure graphics_clear() honors the clipping region for user
 * framebuffers. */
TEST(test_framebuffer_clipped_clear)
{
    int framebuffer;
    CHECK_TRUE(framebuffer = framebuffer_create(128, 96, FBCOLOR_RGB8, 16, 8));

    framebuffer_bind(framebuffer);
    graphics_set_viewport(0, 0, 128, 96);
    graphics_clear(0, 0, 1, 0, 1, 0);
    graphics_set_clip_region(32, 16, 48, 32);
    graphics_clear(1, 0, 0, 0, 1, 0);

    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(128*96*4, 0, MEM_ALLOC_TEMP));
    CHECK_TRUE(graphics_read_pixels(0, 0, 128, 96, pixels));
    for (int i = 0; i < 128*96*4; i += 4) {
        const int x = (i/4) % 128, y = (i/4) / 128;
        if (x >= 32 && x < 80 && y >= 16 && y < 48) {
            CHECK_PIXEL(&pixels[i], 255,0,0,255, x, y);
        } else {
            CHECK_PIXEL(&pixels[i], 0,0,255,255, x, y);
        }
    }
    mem_free(pixels);

    framebuffer_destroy(framebuffer);
    graphics_set_clip_region(0, 0, 0, 0);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_vertex_and_fixed_colors)
{
    graphics_set_viewport(0, 0, 4, 4);

    int texture;
    ASSERT(texture = texture_create_with_data(
               1, 1, "\xFF\xFF\xFF\xFF", TEX_FORMAT_RGBA8888, 1, 0, 0));
    texture_apply(0, texture);

    int primitive;
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    Vector4f vertex_color = {0.4, 0.6, 0.8, 1};
    graphics_add_vertex(&(Vector3f){-1,-1,0}, &(Vector2f){0,0}, &vertex_color);
    graphics_add_vertex(&(Vector3f){+1,-1,0}, &(Vector2f){1,0}, &vertex_color);
    graphics_add_vertex(&(Vector3f){+1,+1,0}, &(Vector2f){1,1}, &vertex_color);
    graphics_add_vertex(&(Vector3f){-1,+1,0}, &(Vector2f){0,1}, &vertex_color);
    CHECK_TRUE(primitive = graphics_end_primitive());

    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_set_fixed_color(&(Vector4f){0.5, 0.667, 0.75, 1});
    graphics_draw_primitive(primitive);
    uint8_t pixels[4*4*4];
    CHECK_TRUE(graphics_read_pixels(0, 0, 4, 4, pixels));
    for (int i = 0; i < 4*4; i++) {
        const int x = i/4, y = i/4;
        CHECK_PIXEL_NEAR(&pixels[i*4], 51,102,153,255, 1, x,y);
    }

    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_set_fixed_color(&(Vector4f){1, 1, 1, 1});
    graphics_draw_primitive(primitive);
    CHECK_TRUE(graphics_read_pixels(0, 0, 4, 4, pixels));
    for (int i = 0; i < 4*4; i++) {
        const int x = i/4, y = i/4;
        CHECK_PIXEL_NEAR(&pixels[i*4], 102,153,204,255, 1, x,y);
    }

    graphics_destroy_primitive(primitive);
    texture_destroy(texture);
    graphics_set_fixed_color(&(Vector4f){1, 1, 1, 1});
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_vertex_and_fixed_colors_tall_texture)
{
    graphics_set_viewport(0, 0, 4, 4);

    int texture;
    CHECK_TRUE(texture = texture_create(1, 1024, 0, 0));
    uint8_t *tex_pixels;
    CHECK_TRUE(tex_pixels = texture_lock(texture));
    for (int y = 0; y < 256; y++) {
        tex_pixels[y*4+0] = 0;
        tex_pixels[y*4+1] = 0;
        tex_pixels[y*4+2] = (y%15)*16;
        tex_pixels[y*4+3] = 255;
        tex_pixels[(256+y)*4+0] = 0;
        tex_pixels[(256+y)*4+1] = 240;
        tex_pixels[(256+y)*4+2] = (y%15)*16;
        tex_pixels[(256+y)*4+3] = 255;
        tex_pixels[(512+y)*4+0] = 240;
        tex_pixels[(512+y)*4+1] = 0;
        tex_pixels[(512+y)*4+2] = (y%15)*16;
        tex_pixels[(512+y)*4+3] = 255;
        tex_pixels[(768+y)*4+0] = 240;
        tex_pixels[(768+y)*4+1] = 240;
        tex_pixels[(768+y)*4+2] = (y%15)*16;
        tex_pixels[(768+y)*4+3] = 255;
    }
    texture_unlock(texture);
    texture_set_repeat(texture, 0, 0);
    texture_set_antialias(texture, 0);

    int primitive;
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    Vector4f vertex_color = {0.25, 0.5, 0.75, 1};
    graphics_add_vertex(
        &(Vector3f){-1,-1,0}, &(Vector2f){0, 512/1024.0f}, &vertex_color);
    graphics_add_vertex(
        &(Vector3f){+1,-1,0}, &(Vector2f){1, 512/1024.0f}, &vertex_color);
    graphics_add_vertex(
        &(Vector3f){+1,+1,0}, &(Vector2f){1, 516/1024.0f}, &vertex_color);
    graphics_add_vertex(
        &(Vector3f){-1,+1,0}, &(Vector2f){0, 516/1024.0f}, &vertex_color);
    CHECK_TRUE(primitive = graphics_end_primitive());

    uint8_t pixels[4*4*4];

    graphics_clear(0, 0, 0, 0, 1, 0);
    texture_apply(0, texture);
    graphics_set_fixed_color(&(Vector4f){0.5, 0.5, 0.5, 1});
    graphics_draw_primitive(primitive);
    CHECK_TRUE(graphics_read_pixels(0, 0, 4, 4, pixels));
    for (int i = 0; i < 4*4; i++) {
        const int x = i/4, y = i/4;
        CHECK_PIXEL(&pixels[i*4], 30,0,y*6,255, x,y);
    }

    graphics_clear(0, 0, 0, 0, 1, 0);
    texture_apply(0, texture);
    graphics_set_fixed_color(&(Vector4f){1, 1, 1, 1});
    graphics_draw_primitive(primitive);
    CHECK_TRUE(graphics_read_pixels(0, 0, 4, 4, pixels));
    for (int i = 0; i < 4*4; i++) {
        const int x = i/4, y = i/4;
        CHECK_PIXEL(&pixels[i*4], 60,0,y*12,255, x,y);
    }

    graphics_destroy_primitive(primitive);
    texture_destroy(texture);
    graphics_set_fixed_color(&(Vector4f){1, 1, 1, 1});
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_point_size)
{
    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(480*272*4, 0, MEM_ALLOC_TEMP));

    graphics_set_viewport(0, 0, 480, 272);
    graphics_set_point_size(1);
    graphics_set_point_size(2);  // Should do nothing.

    const float px = 0.5f/480, py = 0.5f/272;

    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){px,py,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_TRUE(graphics_read_pixels(0, 0, 480, 272, pixels));
    for (int i = 0; i < 480*272*4; i += 4) {
        const int x = (i/4) % 480;
        const int y = (i/4) / 480;
        const int p = (x == 480/2 && y == 272/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_primitive_immediate_index_buffer_overflow)
{
    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(480*272*4, 0, MEM_ALLOC_TEMP));
    graphics_set_viewport(0, 0, 480, 272);

    static const float vertices[] = {-1,0, -1,1, 0,0, 0,1};
    static const uint32_t format[] =
        {GRAPHICS_VERTEX_FORMAT(POSITION_2F, 0), 0};
    /* Make this large enough that it will always overflow the GE vertex
     * data buffer. */
    static const uint16_t indices[] = {0,1,2,3, 0,1,2,3};

    /* Have to do this first since it allocates some vertices! */
    graphics_clear(0, 0, 0, 0, 1, 0);

    extern uint32_t *vertlist_ptr, *vertlist_limit;  // From ge-util/base.c.
    const int floats_free = vertlist_limit - vertlist_ptr;
    const int vertex_count = floats_free / 3;
    float *vertex_buffer;
    ASSERT(vertex_buffer = mem_alloc((2*sizeof(float)) * vertex_count, 0, 0));
    memcpy(vertex_buffer, vertices, sizeof(vertices));

    graphics_draw_indexed_vertices(GRAPHICS_PRIMITIVE_QUAD_STRIP,
                                   vertex_buffer, format, 2*sizeof(float),
                                   vertex_count, indices, 2, lenof(indices));
    CHECK_TRUE(graphics_read_pixels(0, 0, 480, 272, pixels));
    for (int i = 0; i < 480*272*4; i += 4) {
        const int x = (i/4) % 480;
        const int y = (i/4) / 480;
        const int p = (x < 480/2 && y >= 272/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }

    mem_free(vertex_buffer);
    mem_free(pixels);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_no_position)
{
    uint32_t dummy = 0xFFFFFFFF;
    static const uint32_t format[] =
        {GRAPHICS_VERTEX_FORMAT(COLOR_4NUB, 0), 0};

    CHECK_FALSE(graphics_create_primitive(GRAPHICS_PRIMITIVE_POINTS,
                                          &dummy, format, 4, 1));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_oversize_primitives)
{
    /* These should fail without touching the data, so we don't bother
     * allocating and initializing data buffers. */
    uint32_t dummy;
    CHECK_FALSE(graphics_create_primitive(
                    GRAPHICS_PRIMITIVE_POINTS, &dummy, &dummy, 4, 65536));
    CHECK_FALSE(graphics_create_indexed_primitive(
                    GRAPHICS_PRIMITIVE_POINTS, &dummy, &dummy, 4, 1,
                    &dummy, 2, 65536));
    CHECK_FALSE(graphics_create_indexed_primitive(
                    GRAPHICS_PRIMITIVE_POINTS, &dummy, &dummy, 4, 1,
                    &dummy, 4, 1));

    static const uint32_t pos4_format[] =
        {GRAPHICS_VERTEX_FORMAT(POSITION_4F, 0), 0};
    CHECK_FALSE(graphics_create_primitive(
                    GRAPHICS_PRIMITIVE_POINTS, &dummy, pos4_format, 16, 1));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_misaligned_primitives)
{
    uint8_t buf[32];  // Will be at least 4-byte aligned.
    mem_clear(buf, sizeof(buf));
    static const uint32_t misaligned_pos_s[] =
        {GRAPHICS_VERTEX_FORMAT(POSITION_2S, 1), 0};
    static const uint32_t misaligned_pos_f[] =
        {GRAPHICS_VERTEX_FORMAT(POSITION_2F, 1), 0};
    static const uint32_t misaligned_tex[] =
        {GRAPHICS_VERTEX_FORMAT(POSITION_2F, 0),
         GRAPHICS_VERTEX_FORMAT(TEXCOORD_2F, 9), 0};
    static const uint32_t misaligned_color[] =
        {GRAPHICS_VERTEX_FORMAT(POSITION_2F, 0),
         GRAPHICS_VERTEX_FORMAT(COLOR_4F, 9), 0};

    CHECK_FALSE(graphics_create_primitive(GRAPHICS_PRIMITIVE_POINTS,
                                          buf, misaligned_pos_s, 32, 1));
    CHECK_FALSE(graphics_create_primitive(GRAPHICS_PRIMITIVE_POINTS,
                                          buf, misaligned_pos_f, 32, 1));
    CHECK_FALSE(graphics_create_primitive(GRAPHICS_PRIMITIVE_POINTS,
                                          buf, misaligned_tex, 32, 1));
    CHECK_FALSE(graphics_create_primitive(GRAPHICS_PRIMITIVE_POINTS,
                                          buf, misaligned_color, 32, 1));

    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(64*64*4, 0, MEM_ALLOC_TEMP));
    graphics_set_viewport(0, 0, 64, 64);
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_draw_vertices(GRAPHICS_PRIMITIVE_POINTS,
                           buf, misaligned_pos_s, 32, 1);
    graphics_draw_vertices(GRAPHICS_PRIMITIVE_POINTS,
                           buf, misaligned_pos_f, 32, 1);
    graphics_draw_vertices(GRAPHICS_PRIMITIVE_POINTS,
                           buf, misaligned_tex, 32, 1);
    graphics_draw_vertices(GRAPHICS_PRIMITIVE_POINTS,
                           buf, misaligned_color, 32, 1);
    CHECK_TRUE(graphics_read_pixels(0, 0, 64, 64, pixels));
    for (int i = 0; i < 64*64*4; i += 4) {
        const int x = (i/4) % 64;
        const int y = (i/4) / 64;
        CHECK_PIXEL(&pixels[i], 0,0,0,255, x, y);
    }
    mem_free(pixels);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_indexed_quad_strip_odd_index_count)
{
    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(480*272*4, 0, MEM_ALLOC_TEMP));
    graphics_set_viewport(0, 0, 480, 272);

    static const float vertices[] = {-1,0, -1,1, 0,0, 0,1, 0.5,0};
    static const uint32_t format[] =
        {GRAPHICS_VERTEX_FORMAT(POSITION_2F, 0), 0};
    static const uint8_t indices[] = {0,1,2,3,4};

    for (int count = 1; count <= 5; count += 2) {
        graphics_clear(0, 0, 0, 0, 1, 0);
        graphics_draw_indexed_vertices(GRAPHICS_PRIMITIVE_QUAD_STRIP,
                                       vertices, format, 2*sizeof(float), 5,
                                       indices, 1, count);
        CHECK_TRUE(graphics_read_pixels(0, 0, 480, 272, pixels));
        for (int i = 0; i < 480*272*4; i += 4) {
            const int x = (i/4) % 480;
            const int y = (i/4) / 480;
            const int p = (count == 5 && x < 480/2 && y >= 272/2) ? 255 : 0;
            CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
        }
    }

    mem_free(pixels);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_draw_between_frames_REINIT)
{
    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(480*272*4, 0, MEM_ALLOC_TEMP));

    /* Clear both display framebuffers to zero. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();

    /* Rendering between frames should have no effect. */
    graphics_clear(1, 0, 0, 0, 1, 0);
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    graphics_add_vertex(&(Vector3f){0,0,0}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){0,8,0}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){8,8,0}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){8,0,0}, NULL, NULL);
    /* This will return true because the primitive was successfully
     * created, even though it won't be drawn. */
    CHECK_TRUE(graphics_end_and_draw_primitive());

    /* Verify that nothing was drawn in either display framebuffer. */
    graphics_start_frame();
    CHECK_TRUE(graphics_read_pixels(0, 0, 480, 272, pixels));
    for (int y = 0; y < 272; y++) {
        for (int x = 0; x < 480; x++) {
            CHECK_PIXEL(&pixels[((271-y)*480+x)*4], 0,0,0,255, x,y);
        }
    }
    graphics_finish_frame();
    graphics_start_frame();
    CHECK_TRUE(graphics_read_pixels(0, 0, 480, 272, pixels));
    for (int y = 0; y < 272; y++) {
        for (int x = 0; x < 480; x++) {
            CHECK_PIXEL(&pixels[((271-y)*480+x)*4], 0,0,0,255, x,y);
        }
    }

    mem_free(pixels);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_cleanup_graphics_during_frame_NOFRAME)
{
    /* This should normally never happen in a real program, but it can
     * occur during tests if a test fails before finishing the frame it is
     * drawing. */
    graphics_start_frame();
    /* Let the test cleanup routine call graphics_cleanup() with the
     * current frame still open. */
    return 1;
}

/*************************************************************************/
/*************************************************************************/
