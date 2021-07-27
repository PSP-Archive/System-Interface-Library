/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/opengl/framebuffer.c: OpenGL-specific framebuffer tests.
 */

#include "src/base.h"
#undef SIL_OPENGL_NO_SYS_FUNCS  // Avoid type renaming.
#include "src/framebuffer.h"
#include "src/graphics.h"
#include "src/math.h"
#include "src/sysdep.h"
#include "src/sysdep/opengl/internal.h"
#include "src/sysdep/opengl/opengl.h"
#include "src/test/base.h"
#include "src/test/graphics/internal.h"
#include "src/texture.h"

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

static int wrap_test_opengl_framebuffer(void);
static int do_test_opengl_framebuffer(void);
int test_opengl_framebuffer(void)
{
    return run_tests_in_window(wrap_test_opengl_framebuffer);
}

static int wrap_test_opengl_framebuffer(void)
{
    if (!opengl_has_features(OPENGL_FEATURE_FRAMEBUFFERS)) {
        /* We can still test the framebuffers-disabled path. */
        if (!test_no_framebuffer_support()) {
            return 0;
        }
        SKIP("Framebuffers not available.");
    }
    return do_test_opengl_framebuffer();
}

DEFINE_GENERIC_TEST_RUNNER(do_test_opengl_framebuffer)

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
    opengl_free_dead_resources(1);
    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_no_framebuffer_support)
{
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    graphics_cleanup();
    TEST_opengl_force_feature_mask = OPENGL_FEATURE_FRAMEBUFFERS;
    TEST_opengl_force_feature_flags = 0;
    ASSERT(graphics_init());
    ASSERT(open_window(TESTW, TESTH));
    graphics_start_frame();
    TEST_opengl_force_feature_mask = 0;
    TEST_opengl_force_feature_flags = 0;

    CHECK_FALSE(sys_framebuffer_create(16, 16, FBCOLOR_RGB8, 16, 8));

    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_finish_frame();
    graphics_cleanup();
    ASSERT(graphics_init());
    ASSERT(open_window(TESTW, TESTH));
    graphics_start_frame();
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_failure)
{
    CHECK_FALSE(sys_framebuffer_create(0, 0, FBCOLOR_RGB8, 16, 8));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_bind_and_get)
{
    SysFramebuffer *framebuffer;
    CHECK_TRUE(framebuffer = sys_framebuffer_create(16, 16,
                                                    FBCOLOR_RGB8, 16, 8));
    sys_framebuffer_bind(framebuffer);

    GLint binding;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &binding);
    CHECK_INTEQUAL(binding, framebuffer->framebuffer);

    CHECK_PTREQUAL(opengl_current_framebuffer(), framebuffer);

    sys_framebuffer_bind(NULL);
    sys_framebuffer_destroy(framebuffer);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_destroy_unbinds)
{
    SysFramebuffer *framebuffer;
    CHECK_TRUE(framebuffer = sys_framebuffer_create(16, 16,
                                                    FBCOLOR_RGB8, 16, 8));

    GLint binding;
    sys_framebuffer_bind(framebuffer);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &binding);
    CHECK_INTEQUAL(binding, framebuffer->framebuffer);

    sys_framebuffer_destroy(framebuffer);
    opengl_free_dead_resources(0);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &binding);
    CHECK_INTEQUAL(binding, opengl_get_default_framebuffer());

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_default_framebuffer)
{
    const GLuint saved_default_framebuffer = opengl_get_default_framebuffer();

    SysFramebuffer *framebuffer1, *framebuffer2;
    CHECK_TRUE(framebuffer1 = sys_framebuffer_create(16, 16,
                                                     FBCOLOR_RGB8, 16, 8));
    CHECK_TRUE(framebuffer2 = sys_framebuffer_create(32, 32,
                                                     FBCOLOR_RGB8, 16, 8));

    GLint binding;
    sys_framebuffer_bind(framebuffer1);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &binding);
    CHECK_INTEQUAL(binding, framebuffer1->framebuffer);

    opengl_set_default_framebuffer(framebuffer2->framebuffer);
    sys_framebuffer_bind(NULL);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &binding);
    /* Make sure we restore the default even if we're about to fail out. */
    opengl_set_default_framebuffer(saved_default_framebuffer);
    CHECK_INTEQUAL(binding, framebuffer2->framebuffer);

    sys_framebuffer_destroy(framebuffer1);
    sys_framebuffer_destroy(framebuffer2);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_depth_stencil_config)
{
    /* 255 is included to check various failure cases. */
    static const uint8_t depth_list[] = {0, 16, 24, 32, 255};
    static const uint8_t stencil_list[] = {0, 8, 255};

    for (int i = 0; i < lenof(depth_list); i++) {
        const int depth_bits = depth_list[i];
        for (int j = 0; j < lenof(stencil_list); j++) {
            const int stencil_bits = stencil_list[j];
            SysFramebuffer *framebuffer = sys_framebuffer_create(
                16, 16, FBCOLOR_RGB8, depth_bits, stencil_bits);
            if (!framebuffer) {
                if (depth_bits <= 16 && stencil_bits <= 8) {
                    FAIL("sys_framebuffer_create() failed for depth=%d"
                         " stencil=%d", depth_bits, stencil_bits);
                } else {
                    continue;  // Let it slide.
                }
            }
            sys_framebuffer_bind(framebuffer);
            graphics_clear(0, 0, 0, 0, 1, 0);
            graphics_set_viewport(0, 0, 16, 16);
            graphics_enable_depth_test(depth_bits > 0);
            graphics_set_depth_test_comparison(GRAPHICS_COMPARISON_LESS);
            graphics_enable_stencil_test(stencil_bits > 0);
            graphics_set_stencil_comparison(GRAPHICS_COMPARISON_TRUE, 0, ~0U);
            graphics_set_stencil_operations(GRAPHICS_STENCIL_REPLACE,
                                            GRAPHICS_STENCIL_REPLACE,
                                            GRAPHICS_STENCIL_REPLACE);

            Vector4f color;

            color = (Vector4f){0,0,1,1};
            graphics_set_fixed_color(&color);
            graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
            graphics_add_vertex(&(Vector3f){-1,-1,0}, NULL, NULL);
            graphics_add_vertex(&(Vector3f){-1,+1,0}, NULL, NULL);
            graphics_add_vertex(&(Vector3f){+1,+1,0}, NULL, NULL);
            graphics_add_vertex(&(Vector3f){+1,-1,0}, NULL, NULL);
            graphics_end_and_draw_primitive();

            if (depth_bits > 0) {
                /* Test depth buffer resolution by rendering two quads with
                 * a Z-separation small enough that the Z values would be
                 * treated as equal at a smaller bit depth.  Ideally, we
                 * could use 2^(1-depth_bits), but some renderers lose a
                 * few bits of precision along the rendering path.  Note
                 * that we have to be careful in the 32-bit case because of
                 * the limited resolution of floats -- when the Z value is
                 * converted to a depth value, it'll be 0.5 +/- a small
                 * amount, so that small amount needs to still be
                 * significant in the floating-point mantissa. */
                const float z = -powf(2, lbound(4 - depth_bits, -24));

                color = (Vector4f){0,1,0,1};
                graphics_set_fixed_color(&color);
                graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
                graphics_add_vertex(&(Vector3f){-1,-1,z}, NULL, NULL);
                graphics_add_vertex(&(Vector3f){-1,+1,z}, NULL, NULL);
                graphics_add_vertex(&(Vector3f){+1,+1,z}, NULL, NULL);
                graphics_add_vertex(&(Vector3f){+1,-1,z}, NULL, NULL);
                graphics_end_and_draw_primitive();

                /* Double-check that depth testing is actually functioning. */
                graphics_set_fixed_color(&(Vector4f){1,0,0,1});
                graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
                graphics_add_vertex(&(Vector3f){-1,-1,z}, NULL, NULL);
                graphics_add_vertex(&(Vector3f){-1,+1,z}, NULL, NULL);
                graphics_add_vertex(&(Vector3f){+1,+1,z}, NULL, NULL);
                graphics_add_vertex(&(Vector3f){+1,-1,z}, NULL, NULL);
                graphics_end_and_draw_primitive();
            }

            if (stencil_bits > 0) {
                const unsigned int value = 1U << (stencil_bits-1);
                const unsigned int mask = value | (value-1);

                graphics_enable_depth_test(0);
                graphics_set_stencil_comparison(GRAPHICS_COMPARISON_TRUE,
                                                value, mask);
                color = (Vector4f){0,1,1,1};
                graphics_set_fixed_color(&color);
                graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
                graphics_add_vertex(&(Vector3f){-1,-1,0}, NULL, NULL);
                graphics_add_vertex(&(Vector3f){-1,+1,0}, NULL, NULL);
                graphics_add_vertex(&(Vector3f){+1,+1,0}, NULL, NULL);
                graphics_add_vertex(&(Vector3f){+1,-1,0}, NULL, NULL);
                graphics_end_and_draw_primitive();

                graphics_set_stencil_comparison(GRAPHICS_COMPARISON_LESS,
                                                value-1, mask);
                graphics_set_fixed_color(&(Vector4f){1,0,1,1});
                graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
                graphics_add_vertex(&(Vector3f){-1,-1,0}, NULL, NULL);
                graphics_add_vertex(&(Vector3f){-1,+1,0}, NULL, NULL);
                graphics_add_vertex(&(Vector3f){+1,+1,0}, NULL, NULL);
                graphics_add_vertex(&(Vector3f){+1,-1,0}, NULL, NULL);
                graphics_end_and_draw_primitive();
            }

            const int expect_r = iroundf(color.x*255);
            const int expect_g = iroundf(color.y*255);
            const int expect_b = iroundf(color.z*255);
            uint8_t pixels[16][16][4];
            graphics_read_pixels(0, 0, 16, 16, pixels);
            for (int y = 0; y < 16; y++) {
                for (int x = 0; x < 16; x++) {
                    if (pixels[y][x][0] != expect_r
                     || pixels[y][x][1] != expect_g
                     || pixels[y][x][2] != expect_b
                     || pixels[y][x][3] != 255) {
                        FAIL("Pixel (%d,%d) was RGBA (%d,%d,%d,%d) but"
                             " should have been (%d,%d,%d,255) for depth=%d"
                             " stencil=%d", x, y, pixels[y][x][0],
                             pixels[y][x][1], pixels[y][x][2], pixels[y][x][3],
                             expect_r, expect_g, expect_b, depth_bits,
                             stencil_bits);
                    }
                    CHECK_PIXEL(pixels[y][x],
                                expect_r, expect_g, expect_b, 255, x, y);
                }
            }

            sys_framebuffer_destroy(framebuffer);
        }
    }

    graphics_enable_depth_test(0);
    graphics_enable_stencil_test(0);
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

    SysFramebuffer *framebuffer;
    CHECK_TRUE(framebuffer = sys_framebuffer_create(TESTW, TESTH,
                                                    FBCOLOR_RGB8, 16, 8));
    sys_framebuffer_bind(framebuffer);
    graphics_clear(1, 0, 0, 0, 1, 0);
    sys_framebuffer_bind(NULL);

    graphics_finish_frame();
    force_close_window();
    ASSERT(open_window(TESTW, TESTH));
    graphics_start_frame();

    SysFramebuffer *framebuffer2 =
        sys_framebuffer_create(TESTW, TESTH, FBCOLOR_RGB8, 16, 8);
    CHECK_INTEQUAL(framebuffer2->framebuffer, framebuffer->framebuffer);
    CHECK_INTEQUAL(framebuffer2->depth_buffer, framebuffer->depth_buffer);
    CHECK_INTEQUAL(framebuffer2->stencil_buffer, framebuffer->stencil_buffer);
    CHECK_INTEQUAL(framebuffer2->texture.id, framebuffer->texture.id);

    sys_framebuffer_bind(framebuffer);  // This should fail.
    graphics_clear(0, 1, 0, 0, 1, 0);
    draw_square(0, 0,0,1,1);
    sys_framebuffer_bind(NULL);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW, y = (i/4) / TESTW;
        if (x >= TESTW/4 && x < TESTW*3/4 && y >= TESTH/4 && y < TESTH*3/4) {
            CHECK_PIXEL(&pixels[i], 0,0,255,255, x, y);
        } else {
            CHECK_PIXEL(&pixels[i], 0,255,0,255, x, y);
        }
    }
    mem_free(pixels);

    /* These should be no-ops; we can't check the results, but we can at
     * least check that the calls don't cause a crash. */
    sys_framebuffer_set_antialias(framebuffer, 0);
    sys_framebuffer_discard_data(framebuffer);

    /* The framebuffer/texture association is constant, so this will
     * succeed even on an invalidated framebuffer object (though the
     * texture object will be invalid). */
    SysTexture *texture;
    CHECK_TRUE(texture = sys_framebuffer_get_texture(framebuffer));
    sys_texture_apply(0, texture);
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
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 255,255,255,255, (i/4) % TESTW, (i/4) / TESTW);
    }
    mem_free(pixels);

    /* Check that destroying an invalidated framebuffer doesn't affect a
     * second framebuffer with the same OpenGL IDs. */
    sys_framebuffer_bind(framebuffer2);
    graphics_clear(0, 1, 1, 0, 1, 0);
    sys_framebuffer_bind(NULL);
    sys_framebuffer_destroy(framebuffer);  // Should not destroy framebuffer2.
    sys_texture_apply(0, sys_framebuffer_get_texture(framebuffer2));
    graphics_clear(1, 0, 0, 0, 1, 0);
    graphics_set_blend(GRAPHICS_BLEND_ADD, 1, 0);
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
        CHECK_PIXEL(&pixels[i], 0,255,255,255, (i/4) % TESTW, (i/4) / TESTW);
    }
    mem_free(pixels);
    sys_framebuffer_destroy(framebuffer2);

    return 1;
}

/*************************************************************************/
/*************************************************************************/
