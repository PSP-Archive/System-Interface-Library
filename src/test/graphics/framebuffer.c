/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/graphics/framebuffer.c: Tests for framebuffer functionality.
 */

#include "src/base.h"
#include "src/framebuffer.h"
#include "src/graphics.h"
#include "src/graphics/internal.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/test/base.h"
#include "src/test/graphics/internal.h"
#include "src/texture.h"

/*************************************************************************/
/*************************** Common test data ****************************/
/*************************************************************************/

/* Vertex format for test data. */
typedef struct BasicVertex {float x,y,z;} BasicVertex;
static const uint32_t basic_vertex_format[] = {
    GRAPHICS_VERTEX_FORMAT(POSITION_3F, offsetof(BasicVertex,x)),
    0
};

/* White (RGB=255,255,255) quad over the horizontal center (-0.5<=x<0.5)
 * of the render target. */
static const BasicVertex white_quad_vertices[] = {
    {-0.5,-1,0}, {-0.5,1,0}, {0.5,1,0}, {0.5,-1,0}
};

/* Same as white_quad_vertices, but with z=-1. */
static const BasicVertex white_quad_vertices_minusZ[] = {
    {-0.5,-1,-1}, {-0.5,1,-1}, {0.5,1,-1}, {0.5,-1,-1}
};

#ifdef SIL_PLATFORM_WINDOWS
/* Allocated memory block used to avoid a spurious memory leak report after
 * the first clear operation which triggers d3d_state_safe_clear(), as in
 * state.c. */
#include "src/sysdep/windows/internal.h"  // For TEST_windows_force_direct3d.
#include "src/sysdep/windows/d3d-internal.h"  // For structure sizes.
static void *safe_clear_leak_cover;
#endif

/*************************************************************************/
/************************ Basic framebuffer tests ************************/
/*************************************************************************/

static int wrap_test_graphics_framebuffer(void);
static int do_test_graphics_framebuffer(void);
int test_graphics_framebuffer(void)
{
#ifdef SIL_PLATFORM_WINDOWS
    if (TEST_windows_force_direct3d) {
        ASSERT(safe_clear_leak_cover = mem_alloc(
                   2*sizeof(D3DSysShader) + sizeof(D3DSysShaderPipeline),
                   0, 0));
    }
#endif

    const int result = run_tests_in_window(wrap_test_graphics_framebuffer);

#ifdef SIL_PLATFORM_WINDOWS
    if (result) {
        CHECK_FALSE(safe_clear_leak_cover);
    } else {
        mem_free(safe_clear_leak_cover);
        safe_clear_leak_cover = NULL;
    }
#endif

    return result;
}

static int wrap_test_graphics_framebuffer(void)
{
    if (!framebuffer_supported()) {
        SKIP("Framebuffers not supported.");
    }
    return do_test_graphics_framebuffer();
}

DEFINE_GENERIC_TEST_RUNNER(do_test_graphics_framebuffer)

TEST_INIT(init)
{
    graphics_start_frame();
    framebuffer_bind(0);
    graphics_set_viewport(0, 0, TESTW, TESTH);
    graphics_enable_depth_test(0);
    graphics_enable_stencil_test(0);
    graphics_set_blend(GRAPHICS_BLEND_ADD,
                       GRAPHICS_BLEND_SRC_ALPHA, GRAPHICS_BLEND_INV_SRC_ALPHA);
    graphics_set_blend_alpha(0, 0, 0);
    graphics_set_blend_color(&(Vector4f){1,1,1,1});
    graphics_enable_color_write(1, 1, 1, 1);
    graphics_set_fixed_color(&(Vector4f){1,1,1,1});
    graphics_clear(0, 0, 0, 0, 1, 0);
    return 1;
}

TEST_CLEANUP(cleanup)
{
    graphics_finish_frame();
    graphics_flush_resources();
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create)
{
    int framebuffer;
    CHECK_TRUE(framebuffer = framebuffer_create(TESTW, TESTH,
                                                FBCOLOR_RGB8, 16, 8));

    CHECK_INTEQUAL(framebuffer_width(framebuffer), TESTW);
    CHECK_INTEQUAL(framebuffer_height(framebuffer), TESTH);

    framebuffer_destroy(framebuffer);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_invalid)
{
    CHECK_FALSE(framebuffer_create(0, TESTH, FBCOLOR_RGB8, 16, 8));
    CHECK_FALSE(framebuffer_create(TESTW, 0, FBCOLOR_RGB8, 16, 8));
    CHECK_FALSE(framebuffer_create(TESTW, TESTH, 0, 16, 8));
    CHECK_FALSE(framebuffer_create(TESTW, TESTH, FBCOLOR_RGB8, -1, 8));
    CHECK_FALSE(framebuffer_create(TESTW, TESTH, FBCOLOR_RGB8, 16, -1));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_destroy_invalid)
{
    int framebuffer;
    CHECK_TRUE(framebuffer = framebuffer_create(TESTW, TESTH,
                                                FBCOLOR_RGB8, 16, 8));
    framebuffer_destroy(framebuffer);

    framebuffer_destroy(0);  // Defined as a no-op.
    /* The rest of these are invalid calls, but check that they don't crash. */
    framebuffer_destroy(-1);
    framebuffer_destroy(framebuffer);
    framebuffer_destroy(INT_MAX);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_width_height_invalid)
{
    int framebuffer;
    CHECK_TRUE(framebuffer = framebuffer_create(TESTW, TESTH,
                                                FBCOLOR_RGB8, 16, 8));
    framebuffer_destroy(framebuffer);

    CHECK_INTEQUAL(framebuffer_width(0), 0);
    CHECK_INTEQUAL(framebuffer_width(framebuffer), 0);
    CHECK_INTEQUAL(framebuffer_width(INT_MAX), 0);
    CHECK_INTEQUAL(framebuffer_height(0), 0);
    CHECK_INTEQUAL(framebuffer_height(framebuffer), 0);
    CHECK_INTEQUAL(framebuffer_height(INT_MAX), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_bind)
{
    int texture;
    uint8_t *pixels;

    int framebuffer;
    CHECK_TRUE(framebuffer = framebuffer_create(TESTW, TESTH,
                                                FBCOLOR_RGB8, 16, 8));

    /* This should clear the display rather than the framebuffer. */
    graphics_clear(0, 0, 1, 0, 1, 0);

    framebuffer_bind(framebuffer);
    /* Neither of these should render to the display. */
    graphics_clear(1, 0, 0, 0, 1, 0);
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, white_quad_vertices, basic_vertex_format,
        sizeof(*white_quad_vertices), lenof(white_quad_vertices));

    /* graphics_read_pixels() and texture_create_from_display() should
     * read out of the currently bound framebuffer rather than the display
     * buffer. */
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW, y = (i/4) / TESTW;
        const int p = (x >= TESTW/4 && x < TESTW*3/4) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], 255,p,p,255, x, y);
    }
    mem_free(pixels);
    CHECK_TRUE(
        texture = texture_create_from_display(0, 0, TESTW, TESTH, 1, 0, 0));
    CHECK_INTEQUAL(texture_width(texture), TESTW);
    CHECK_INTEQUAL(texture_height(texture), TESTH);
    CHECK_TRUE(pixels = texture_lock(texture));
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW, y = (i/4) / TESTW;
        const int p = (x >= TESTW/4 && x < TESTW*3/4) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], 255,p,p,255, x, y);
    }
    texture_destroy(texture);

    /* This should break the framebuffer binding. */
    framebuffer_destroy(framebuffer);

    /* This should now read out of the display buffer. */
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 0,0,255,255, (i/4) % TESTW, (i/4) / TESTW);
    }
    mem_free(pixels);
    CHECK_TRUE(
        texture = texture_create_from_display(0, 0, TESTW, TESTH, 1, 0, 0));
    CHECK_INTEQUAL(texture_width(texture), TESTW);
    CHECK_INTEQUAL(texture_height(texture), TESTH);
    CHECK_TRUE(pixels = texture_lock(texture));
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 0,0,255,255, (i/4) % TESTW, (i/4) / TESTW);
    }
    texture_destroy(texture);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_bind_invalid)
{
    uint8_t *pixels;

    int framebuffer, framebuffer2;
    CHECK_TRUE(framebuffer = framebuffer_create(TESTW, TESTH,
                                                FBCOLOR_RGB8, 16, 8));
    CHECK_TRUE(framebuffer2 = framebuffer_create(TESTW, TESTH,
                                                 FBCOLOR_RGB8, 16, 8));
    framebuffer_destroy(framebuffer2);

    framebuffer_bind(framebuffer);
    graphics_clear(1, 0, 0, 0, 1, 0);
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, white_quad_vertices, basic_vertex_format,
        sizeof(*white_quad_vertices), lenof(white_quad_vertices));

    /* None of these should cause the binding to fall back to the display
     * buffer. */
    framebuffer_bind(-1);
    framebuffer_bind(framebuffer2);
    framebuffer_bind(INT_MAX);

    /* This should still be writing to the framebuffer. */
    graphics_clear(0, 0, 1, 0, 1, 0);

    framebuffer_bind(0);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 0,0,0,255, (i/4) % TESTW, (i/4) / TESTW);
    }
    mem_free(pixels);

    framebuffer_bind(framebuffer);
    int texture;
    CHECK_TRUE(
        texture = texture_create_from_display(0, 0, TESTW, TESTH, 1, 0, 0));
    CHECK_INTEQUAL(texture_width(texture), TESTW);
    CHECK_INTEQUAL(texture_height(texture), TESTH);
    CHECK_TRUE(pixels = texture_lock(texture));
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 0,0,255,255, (i/4) % TESTW, (i/4) / TESTW);
    }
    texture_destroy(texture);

    framebuffer_destroy(framebuffer);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_texture)
{
    uint8_t *pixels;

    graphics_clear(0, 0, 1, 0, 1, 0);

    int framebuffer;
    CHECK_TRUE(framebuffer = framebuffer_create(64, 64, FBCOLOR_RGB8, 16, 8));

    framebuffer_bind(framebuffer);
    graphics_set_viewport(0, 0, 64, 64);
    graphics_clear(1, 0, 0, 0, 1, 0);
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, white_quad_vertices, basic_vertex_format,
        sizeof(*white_quad_vertices), lenof(white_quad_vertices));

    framebuffer_bind(0);
    graphics_set_viewport(0, 0, TESTW, TESTH);
    texture_apply(0, framebuffer_get_texture(framebuffer));
    graphics_set_blend(GRAPHICS_BLEND_ADD, 1, 0);

    const float x0 = -1, y0 = -1;
    const float x1 = -1 + 2*(64.0f/TESTW);
    const float y1 = -1 + 2*(64.0f/TESTH);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y0,0},
                                   &(Vector2f){0,0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y1,0},
                                   &(Vector2f){0,1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y1,0},
                                   &(Vector2f){1,1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y0,0},
                                   &(Vector2f){1,0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        if ((x == 16-1 || x == 16 || x == 48-1 || x == 48) && y < 64) {
            continue;  // Skip possibly-antialiased pixels.
        }
        const int r = (x < 64 && y < 64) ? 255 : 0;
        const int g = ((x >= 16 && x < 48) && y < 64) ? 255 : 0;
        const int b = ((x >= 16 && x < 48) || x >= 64 || y >= 64) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], r,g,b,255, x, y);
    }
    mem_free(pixels);

    texture_apply(0, 0);

    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y0,0},
                                   &(Vector2f){0,0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y1,0},
                                   &(Vector2f){0,1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y1,0},
                                   &(Vector2f){1,1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y0,0},
                                   &(Vector2f){1,0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int r = (x < 64 && y < 64) ? 255 : 0;
        const int g = (x < 64 && y < 64) ? 255 : 0;
        const int b = 255;
        CHECK_PIXEL(&pixels[i], r,g,b,255, x, y);
    }
    mem_free(pixels);

    framebuffer_destroy(framebuffer);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_texture_invalid)
{
    int framebuffer;
    CHECK_TRUE(framebuffer = framebuffer_create(64, 64, FBCOLOR_RGB8, 16, 8));
    framebuffer_destroy(framebuffer);

    CHECK_FALSE(framebuffer_get_texture(0));
    CHECK_FALSE(framebuffer_get_texture(framebuffer));
    CHECK_FALSE(framebuffer_get_texture(INT_MAX));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_antialias)
{
    uint8_t *pixels;

    graphics_clear(0, 0, 1, 0, 1, 0);

    int framebuffer;
    CHECK_TRUE(framebuffer = framebuffer_create(32, 32, FBCOLOR_RGB8, 16, 8));

    framebuffer_bind(framebuffer);
    graphics_set_viewport(0, 0, 32, 32);
    graphics_clear(1, 0.2, 0.2, 0, 1, 0);
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, white_quad_vertices, basic_vertex_format,
        sizeof(*white_quad_vertices), lenof(white_quad_vertices));

    framebuffer_bind(0);
    graphics_set_viewport(0, 0, TESTW, TESTH);
    texture_apply(0, framebuffer_get_texture(framebuffer));
    graphics_set_blend(GRAPHICS_BLEND_ADD, 1, 0);

    /* Default (antialiasing on). */
    const float x0 = -1, y0 = -1;
    const float x1 = -1 + 2*(64.0f/TESTW);
    const float y1 = -1 + 2*(64.0f/TESTH);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y0,0},
                                   &(Vector2f){0,0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y1,0},
                                   &(Vector2f){0,1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y1,0},
                                   &(Vector2f){1,1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y0,0},
                                   &(Vector2f){1,0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        if ((x == 16-1 || x == 16 || x == 48-1 || x == 48) && y < 64) {
            if (pixels[i+0] != 255
             || (pixels[i+1] <= 51 || pixels[i+1] >= 255)
             || (pixels[i+2] <= 51 || pixels[i+2] >= 255)
             ||  pixels[i+3] != 255) {
                FAIL("Pixel (%d,%d) was RGBA (%u,%u,%u,%u) but should have"
                     " been between (%u,%u,%u,%u) and (%u,%u,%u,%u)", x, y,
                     pixels[i+0], pixels[i+1], pixels[i+2], pixels[i+3],
                     255, 51, 51, 255, 255, 255, 255, 255);
            }
        } else {
            const int r = (x < 64 && y < 64) ? 255 : 0;
            const int g = (x < 64 && y < 64
                           ? (x > 16 && x < 48-1 ? 255 : 51)
                           : 0);
            const int b = (x < 64 && y < 64
                           ? (x > 16 && x < 48-1 ? 255 : 51)
                           : 255);
            CHECK_PIXEL(&pixels[i], r,g,b,255, x, y);
        }
    }
    mem_free(pixels);

    /* On -> off and off -> off transition. */
    for (int try = 0; try < 2; try++) {
        framebuffer_set_antialias(framebuffer, 0);
        CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
        CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y0,0},
                                       &(Vector2f){0,0}, NULL));
        CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y1,0},
                                       &(Vector2f){0,1}, NULL));
        CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y1,0},
                                       &(Vector2f){1,1}, NULL));
        CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y0,0},
                                       &(Vector2f){1,0}, NULL));
        CHECK_TRUE(graphics_end_and_draw_primitive());
        ASSERT(pixels = grab_display());
        for (int i = 0; i < TESTW*TESTH*4; i += 4) {
            const int x = (i/4) % TESTW;
            const int y = (i/4) / TESTW;
            const int r = (x < 64 && y < 64) ? 255 : 0;
            const int g = ((x >= 16 && x < 48) && y < 64) ? 255 :
                          (x < 64 && y < 64) ? 51 : 0;
            const int b = ((x >= 16 && x < 48) || x >= 64 || y >= 64)
                          ? 255 : 51;
            CHECK_PIXEL(&pixels[i], r,g,b,255, x, y);
        }
        mem_free(pixels);
    }

    /* Off -> on and on -> on transition. */
    for (int try = 0; try < 2; try++) {
        framebuffer_set_antialias(framebuffer, 1);
        CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
        CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y0,0},
                                       &(Vector2f){0,0}, NULL));
        CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y1,0},
                                       &(Vector2f){0,1}, NULL));
        CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y1,0},
                                       &(Vector2f){1,1}, NULL));
        CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y0,0},
                                       &(Vector2f){1,0}, NULL));
        CHECK_TRUE(graphics_end_and_draw_primitive());
        ASSERT(pixels = grab_display());
        for (int i = 0; i < TESTW*TESTH*4; i += 4) {
            const int x = (i/4) % TESTW;
            const int y = (i/4) / TESTW;
            if ((x == 16-1 || x == 16 || x == 48-1 || x == 48) && y < 64) {
                if (pixels[i+0] != 255
                 || (pixels[i+1] <= 51 || pixels[i+1] >= 255)
                 || (pixels[i+2] <= 51 || pixels[i+2] >= 255)
                 ||  pixels[i+3] != 255) {
                    FAIL("Pixel (%d,%d) was RGBA (%u,%u,%u,%u) but should have"
                         " been between (%u,%u,%u,%u) and (%u,%u,%u,%u)", x, y,
                         pixels[i+0], pixels[i+1], pixels[i+2], pixels[i+3],
                         255, 51, 51, 255, 255, 255, 255, 255);
                }
            } else {
                const int r = (x < 64 && y < 64) ? 255 : 0;
                const int g = (x < 64 && y < 64
                               ? (x > 16 && x < 48-1 ? 255 : 51)
                               : 0);
                const int b = (x < 64 && y < 64
                               ? (x > 16 && x < 48-1 ? 255 : 51)
                               : 255);
                CHECK_PIXEL(&pixels[i], r,g,b,255, x, y);
            }
        }
        mem_free(pixels);
    }

    framebuffer_destroy(framebuffer);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_antialias_invalid)
{
    int framebuffer;
    CHECK_TRUE(framebuffer = framebuffer_create(64, 64, FBCOLOR_RGB8, 16, 8));
    framebuffer_destroy(framebuffer);

    framebuffer_set_antialias(0, 0);
    framebuffer_set_antialias(framebuffer, 0);
    framebuffer_set_antialias(INT_MAX, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_discard)
{
    uint8_t *pixels;

    int framebuffer;
    CHECK_TRUE(framebuffer = framebuffer_create(TESTW, TESTH,
                                                FBCOLOR_RGB8, 16, 8));
    framebuffer_bind(framebuffer);
    graphics_clear(1, 0, 0, 0, 1, 0);
    framebuffer_discard_data(framebuffer);
    graphics_clear(0, 1, 0, 0, 1, 0);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 0,255,0,255, (i/4) % TESTW, (i/4) / TESTW);
    }
    mem_free(pixels);
    framebuffer_destroy(framebuffer);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_discard_other_framebuffer)
{
    uint8_t *pixels;
    int framebuffer1, framebuffer2;
    CHECK_TRUE(framebuffer1 = framebuffer_create(TESTW, TESTH,
                                                 FBCOLOR_RGB8, 16, 8));
    CHECK_TRUE(framebuffer2 = framebuffer_create(TESTW, TESTH,
                                                 FBCOLOR_RGB8, 16, 8));

    framebuffer_bind(0);
    graphics_clear(1, 0, 0, 0, 1, 0);
    framebuffer_discard_data(framebuffer1);
    graphics_clear(0, 1, 0, 0, 1, 0);  // Should not draw to framebuffer1.
    framebuffer_bind(0);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 0,255,0,255, (i/4) % TESTW, (i/4) / TESTW);
    }
    mem_free(pixels);

    framebuffer_bind(framebuffer1);
    graphics_clear(0, 0, 1, 0, 1, 0);
    framebuffer_discard_data(framebuffer2); // Should not invalidate framebuffer1.
    framebuffer_bind(framebuffer1);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 0,0,255,255, (i/4) % TESTW, (i/4) / TESTW);
    }
    mem_free(pixels);

    framebuffer_bind(framebuffer1);
    graphics_clear(1, 0, 1, 0, 1, 0);
    framebuffer_discard_data(framebuffer2);
    graphics_clear(0, 1, 1, 0, 1, 0);  // Should not draw to framebuffer2.
    framebuffer_bind(framebuffer1);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 0,255,255,255, (i/4) % TESTW, (i/4) / TESTW);
    }
    mem_free(pixels);

    framebuffer_destroy(framebuffer1);
    framebuffer_destroy(framebuffer2);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_discard_invalid)
{
    int framebuffer;
    CHECK_TRUE(framebuffer = framebuffer_create(64, 64, FBCOLOR_RGB8, 16, 8));
    framebuffer_destroy(framebuffer);

    framebuffer_discard_data(0);
    framebuffer_discard_data(framebuffer);
    framebuffer_discard_data(INT_MAX);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_memory_failure)
{
    uint8_t *pixels;

    graphics_clear(0, 0, 1, 0, 1, 0);

    /* Prime any arrays that may be extended by the call (e.g., on PSP),
     * so we don't get a bogus memory leak error. */
    framebuffer_destroy(framebuffer_create(64, 64, FBCOLOR_RGB8, 16, 8));

    int framebuffer;
    CHECK_MEMORY_FAILURES(framebuffer = framebuffer_create(
                              64, 64, FBCOLOR_RGB8, 16, 8));

    CHECK_INTEQUAL(framebuffer_width(framebuffer), 64);
    CHECK_INTEQUAL(framebuffer_height(framebuffer), 64);

    framebuffer_bind(framebuffer);
    graphics_set_viewport(0, 0, 64, 64);
    graphics_clear(1, 0, 0, 0, 1, 0);
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, white_quad_vertices, basic_vertex_format,
        sizeof(*white_quad_vertices), lenof(white_quad_vertices));

    framebuffer_bind(0);
    graphics_set_viewport(0, 0, TESTW, TESTH);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 0,0,255,255, (i/4) % TESTW, (i/4) / TESTW);
    }
    mem_free(pixels);

    framebuffer_set_antialias(framebuffer, 0);
    texture_apply(0, framebuffer_get_texture(framebuffer));
    graphics_set_blend(GRAPHICS_BLEND_ADD, 1, 0);
    const float x0 = -1, y0 = -1;
    const float x1 = -1 + 2*(64.0f/TESTW);
    const float y1 = -1 + 2*(64.0f/TESTH);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y0,0},
                                   &(Vector2f){0,0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y1,0},
                                   &(Vector2f){0,1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y1,0},
                                   &(Vector2f){1,1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y0,0},
                                   &(Vector2f){1,0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int r = (x < 64 && y < 64) ? 255 : 0;
        const int g = ((x >= 16 && x < 48) && y < 64) ? 255 : 0;
        const int b = ((x >= 16 && x < 48) || x >= 64 || y >= 64) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], r,g,b,255, x, y);
    }
    mem_free(pixels);

    framebuffer_destroy(framebuffer);
    return 1;
}

/*************************************************************************/
/****************** Graphics operations on framebuffers ******************/
/*************************************************************************/

TEST(test_read_pixels)
{
    int framebuffer;
    uint8_t *pixels;
    int texture;

    CHECK_TRUE(framebuffer = framebuffer_create(64, 64, FBCOLOR_RGB8, 16, 8));
    CHECK_INTEQUAL(framebuffer_width(framebuffer), 64);
    CHECK_INTEQUAL(framebuffer_height(framebuffer), 64);
    framebuffer_bind(framebuffer);
    graphics_set_viewport(0, 0, 64, 64);
    graphics_clear(0, 0, 1, 0, 1, 0);
    graphics_set_blend(GRAPHICS_BLEND_ADD, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,-1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,-1,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());

    /* Check that the read region is properly located. */
    ASSERT(pixels = mem_alloc(16*16*4, 0, 0));
    CHECK_TRUE(graphics_read_pixels(8, 8, 16, 16, pixels));
    for (int i = 0; i < 16*16*4; i += 4) {
        const int x = (i/4) % 16 + 8;
        const int y = (i/4) / 16 + 8;
        CHECK_PIXEL(&pixels[i], 255,255,255,255, x, y);
    }
    mem_free(pixels);
    CHECK_TRUE(texture = texture_create_from_display(8, 8, 16, 16, 1, 0, 0));
    CHECK_INTEQUAL(texture_width(texture), 16);
    CHECK_INTEQUAL(texture_height(texture), 16);
    CHECK_TRUE(pixels = texture_lock(texture));
    for (int i = 0; i < 16*16*4; i += 4) {
        const int x = (i/4) % 16 + 8;
        const int y = (i/4) / 16 + 8;
        CHECK_PIXEL(&pixels[i], 255,255,255,255, x, y);
    }
    texture_destroy(texture);

    /* Check that the returned data is properly oriented. */
    ASSERT(pixels = mem_alloc(16*16*4, 0, 0));
    CHECK_TRUE(graphics_read_pixels(28, 28, 16, 16, pixels));
    for (int i = 0; i < 16*16*4; i += 4) {
        const int x = (i/4) % 16 + 28;
        const int y = (i/4) / 16 + 28;
        const int p = (x < 32 && y < 32) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,255,255, x, y);
    }
    mem_free(pixels);
    CHECK_TRUE(texture = texture_create_from_display(28, 28, 16, 16, 1, 0, 0));
    CHECK_INTEQUAL(texture_width(texture), 16);
    CHECK_INTEQUAL(texture_height(texture), 16);
    CHECK_TRUE(pixels = texture_lock(texture));
    for (int i = 0; i < 16*16*4; i += 4) {
        const int x = (i/4) % 16 + 28;
        const int y = (i/4) / 16 + 28;
        const int p = (x < 32 && y < 32) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,255,255, x, y);
    }
    texture_destroy(texture);

    framebuffer_destroy(framebuffer);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_clear_color)
{
    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(64*64*4, 0, 0));

    int framebuffer;
    CHECK_TRUE(framebuffer = framebuffer_create(64, 64, FBCOLOR_RGB8, 16, 8));
    framebuffer_bind(framebuffer);
    graphics_set_viewport(0, 0, 64, 64);
    graphics_enable_depth_test(1);
    graphics_enable_stencil_test(1);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_EQUAL, 0, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_INCR);

    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, white_quad_vertices, basic_vertex_format,
        sizeof(*white_quad_vertices), lenof(white_quad_vertices));
    CHECK_TRUE(graphics_read_pixels(0, 0, 64, 64, pixels));
    for (int i = 0; i < 64*64*4; i += 4) {
        const int x = (i/4) % 64, y = (i/4) / 64;
        const int p = (x >= 16 && x < 48) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }

    graphics_clear_color(0, 0, 1, 0);
    CHECK_TRUE(graphics_read_pixels(0, 0, 64, 64, pixels));
    for (int i = 0; i < 64*64*4; i += 4) {
        const int x = (i/4) % 64, y = (i/4) / 64;
        CHECK_PIXEL(&pixels[i], 0,0,255,255, x, y);
    }

    /* This should not be drawn because it fails the stencil test. */
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, white_quad_vertices_minusZ,
        basic_vertex_format, sizeof(*white_quad_vertices_minusZ),
        lenof(white_quad_vertices_minusZ));
    CHECK_TRUE(graphics_read_pixels(0, 0, 64, 64, pixels));
    for (int i = 0; i < 64*64*4; i += 4) {
        const int x = (i/4) % 64, y = (i/4) / 64;
        CHECK_PIXEL(&pixels[i], 0,0,255,255, x, y);
    }

    /* This should not be drawn because it fails the depth test. */
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_EQUAL, 1, 255);
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, white_quad_vertices, basic_vertex_format,
        sizeof(*white_quad_vertices), lenof(white_quad_vertices));
    CHECK_TRUE(graphics_read_pixels(0, 0, 64, 64, pixels));
    for (int i = 0; i < 64*64*4; i += 4) {
        const int x = (i/4) % 64, y = (i/4) / 64;
        CHECK_PIXEL(&pixels[i], 0,0,255,255, x, y);
    }

    /* This should be drawn. */
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, white_quad_vertices_minusZ,
        basic_vertex_format, sizeof(*white_quad_vertices_minusZ),
        lenof(white_quad_vertices_minusZ));
    CHECK_TRUE(graphics_read_pixels(0, 0, 64, 64, pixels));
    for (int i = 0; i < 64*64*4; i += 4) {
        const int x = (i/4) % 64, y = (i/4) / 64;
        const int p = (x >= 16 && x < 48) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,255,255, x, y);
    }

    /* Nothing should have been drawn to the display buffer. */
    framebuffer_bind(0);
    CHECK_TRUE(graphics_read_pixels(0, 0, 64, 64, pixels));
    for (int i = 0; i < 64*64*4; i += 4) {
        const int x = (i/4) % 64, y = (i/4) / 64;
        CHECK_PIXEL(&pixels[i], 0,0,0,255, x, y);
    }

    framebuffer_destroy(framebuffer);
    mem_free(pixels);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_clear_depth)
{
    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(64*64*4, 0, 0));

    int framebuffer;
    CHECK_TRUE(framebuffer = framebuffer_create(64, 64, FBCOLOR_RGB8, 16, 8));
    framebuffer_bind(framebuffer);
    graphics_set_viewport(0, 0, 64, 64);
    graphics_enable_depth_test(1);
    graphics_enable_stencil_test(1);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_EQUAL, 0, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_INCR);

    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_set_fixed_color(&(Vector4f){0,1,0,1});
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, white_quad_vertices, basic_vertex_format,
        sizeof(*white_quad_vertices), lenof(white_quad_vertices));
    CHECK_TRUE(graphics_read_pixels(0, 0, 64, 64, pixels));
    for (int i = 0; i < 64*64*4; i += 4) {
        const int x = (i/4) % 64, y = (i/4) / 64;
        const int p = (x >= 16 && x < 48) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], 0,p,0,255, x, y);
    }

    /* This should not be drawn because it fails the depth and stencil
     * tests. */
    graphics_set_fixed_color(&(Vector4f){1,0,0,1});
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, white_quad_vertices, basic_vertex_format,
        sizeof(*white_quad_vertices), lenof(white_quad_vertices));
    CHECK_TRUE(graphics_read_pixels(0, 0, 64, 64, pixels));
    for (int i = 0; i < 64*64*4; i += 4) {
        const int x = (i/4) % 64, y = (i/4) / 64;
        const int p = (x >= 16 && x < 48) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], 0,p,0,255, x, y);
    }

    /* This should not affect the color buffer. */
    graphics_clear_depth(1, 0);
    CHECK_TRUE(graphics_read_pixels(0, 0, 64, 64, pixels));
    for (int i = 0; i < 64*64*4; i += 4) {
        const int x = (i/4) % 64, y = (i/4) / 64;
        const int p = (x >= 16 && x < 48) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], 0,p,0,255, x, y);
    }

    /* This should be now drawn due to the graphics_clear_depth() call. */
    graphics_set_fixed_color(&(Vector4f){0,0,1,1});
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, white_quad_vertices, basic_vertex_format,
        sizeof(*white_quad_vertices), lenof(white_quad_vertices));
    CHECK_TRUE(graphics_read_pixels(0, 0, 64, 64, pixels));
    for (int i = 0; i < 64*64*4; i += 4) {
        const int x = (i/4) % 64, y = (i/4) / 64;
        const int p = (x >= 16 && x < 48) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], 0,0,p,255, x, y);
    }

    /* Nothing should have been drawn to the display buffer. */
    framebuffer_bind(0);
    CHECK_TRUE(graphics_read_pixels(0, 0, 64, 64, pixels));
    for (int i = 0; i < 64*64*4; i += 4) {
        const int x = (i/4) % 64, y = (i/4) / 64;
        CHECK_PIXEL(&pixels[i], 0,0,0,255, x, y);
    }

    framebuffer_destroy(framebuffer);
    mem_free(pixels);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_clear_depth_no_depth_buffer)
{
    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(64*64*4, 0, 0));

    int framebuffer;
    CHECK_TRUE(framebuffer = framebuffer_create(64, 64, FBCOLOR_RGB8, 0, 0));
    framebuffer_bind(framebuffer);
    graphics_set_viewport(0, 0, 64, 64);

    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, white_quad_vertices, basic_vertex_format,
        sizeof(*white_quad_vertices), lenof(white_quad_vertices));
    CHECK_TRUE(graphics_read_pixels(0, 0, 64, 64, pixels));
    for (int i = 0; i < 64*64*4; i += 4) {
        const int x = (i/4) % 64, y = (i/4) / 64;
        const int p = (x >= 16 && x < 48) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }

    /* This should not crash even though the framebuffer has no depth or
     * stencil buffer. */
    graphics_clear_depth(1, 0);
    CHECK_TRUE(graphics_read_pixels(0, 0, 64, 64, pixels));
    for (int i = 0; i < 64*64*4; i += 4) {
        const int x = (i/4) % 64, y = (i/4) / 64;
        const int p = (x >= 16 && x < 48) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }

    framebuffer_destroy(framebuffer);
    mem_free(pixels);
    return 1;
}

/*************************************************************************/
/******************* Framebuffers with alpha channels ********************/
/*************************************************************************/

TEST(test_alpha_basic)
{
#ifdef SIL_PLATFORM_PSP
    SKIP("Alpha framebuffers not supported on PSP.");
#endif

    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(64*64*4, 0, 0));

    int framebuffer;
    CHECK_TRUE(framebuffer = framebuffer_create(64, 64, FBCOLOR_RGBA8, 16, 8));
    framebuffer_bind(framebuffer);
    graphics_set_viewport(0, 0, 64, 64);

    graphics_clear(0.2, 0.4, 0.6, 0.8, 1, 0);
    CHECK_TRUE(graphics_read_pixels(0, 0, 64, 64, pixels));
    for (int i = 0; i < 64*64*4; i += 4) {
        const int x = (i/4) % 64, y = (i/4) / 64;
        CHECK_PIXEL(&pixels[i], 51,102,153,204, x, y);
    }

    framebuffer_destroy(framebuffer);
    mem_free(pixels);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_clear_color_alpha_bound)
{
#ifdef SIL_PLATFORM_PSP
    SKIP("Alpha framebuffers not supported on PSP.");
#endif

    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(64*64*4, 0, 0));

    int framebuffer;
    CHECK_TRUE(framebuffer = framebuffer_create(64, 64, FBCOLOR_RGBA8, 16, 8));
    framebuffer_bind(framebuffer);
    graphics_set_viewport(0, 0, 64, 64);

    graphics_clear(0.2, 0.4, 0.6, 2, 1, 0);
    CHECK_TRUE(graphics_read_pixels(0, 0, 64, 64, pixels));
    for (int i = 0; i < 64*64*4; i += 4) {
        const int x = (i/4) % 64, y = (i/4) / 64;
        CHECK_PIXEL(&pixels[i], 51,102,153,255, x, y);
    }

    graphics_clear(0.4, 0.6, 0.8, -1, 1, 0);
    CHECK_TRUE(graphics_read_pixels(0, 0, 64, 64, pixels));
    for (int i = 0; i < 64*64*4; i += 4) {
        const int x = (i/4) % 64, y = (i/4) / 64;
        CHECK_PIXEL(&pixels[i], 102,153,204,0, x, y);
    }

    framebuffer_destroy(framebuffer);
    mem_free(pixels);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_alpha_blend_dest)
{
#ifdef SIL_PLATFORM_PSP
    SKIP("Alpha framebuffers not supported on PSP.");
#endif

    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(64*64*4, 0, 0));

    int framebuffer;
    CHECK_TRUE(framebuffer = framebuffer_create(64, 64, FBCOLOR_RGBA8, 16, 8));
    framebuffer_bind(framebuffer);
    graphics_set_viewport(0, 0, 64, 64);

    graphics_clear(0.2, 0.4, 0.6, 0.8, 1, 0);
    graphics_set_fixed_color(
        &(Vector4f){1.0f/3.0f, 1.0f/3.0f, 1.0f/3.0f, 1.0f/3.0f});
    graphics_set_blend(GRAPHICS_BLEND_ADD,
                       GRAPHICS_BLEND_DEST_COLOR, GRAPHICS_BLEND_ZERO);
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, white_quad_vertices, basic_vertex_format,
        sizeof(*white_quad_vertices), lenof(white_quad_vertices));
    CHECK_TRUE(graphics_read_pixels(0, 0, 64, 64, pixels));
    for (int i = 0; i < 64*64*4; i += 4) {
        const int x = (i/4) % 64, y = (i/4) / 64;
        const int p = (x >= 16 && x < 48) ? 17 : 51;
        CHECK_PIXEL(&pixels[i], p*1,p*2,p*3,p*4, x, y);
    }

    graphics_set_fixed_color(&(Vector4f){1, 1, 1, 1});
    graphics_set_blend(GRAPHICS_BLEND_ADD,
                       GRAPHICS_BLEND_DEST_ALPHA, GRAPHICS_BLEND_ZERO);
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, white_quad_vertices, basic_vertex_format,
        sizeof(*white_quad_vertices), lenof(white_quad_vertices));
    CHECK_TRUE(graphics_read_pixels(0, 0, 64, 64, pixels));
    for (int i = 0; i < 64*64*4; i += 4) {
        const int x = (i/4) % 64, y = (i/4) / 64;
        if (x >= 16 && x < 48) {
            CHECK_PIXEL(&pixels[i], 68,68,68,68, x, y);
        } else {
            CHECK_PIXEL(&pixels[i], 51,102,153,204, x, y);
        }
    }

    graphics_clear(0.2, 0.4, 0.6, 0.8, 1, 0);
    graphics_set_blend(GRAPHICS_BLEND_ADD,
                       GRAPHICS_BLEND_ZERO, GRAPHICS_BLEND_DEST_COLOR);
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, white_quad_vertices, basic_vertex_format,
        sizeof(*white_quad_vertices), lenof(white_quad_vertices));
    CHECK_TRUE(graphics_read_pixels(0, 0, 64, 64, pixels));
    for (int i = 0; i < 64*64*4; i += 4) {
        const int x = (i/4) % 64, y = (i/4) / 64;
        if (x >= 16 && x < 48) {
            CHECK_PIXEL_NEAR(&pixels[i], 10,41,92,163, 1, x, y);
        } else {
            CHECK_PIXEL(&pixels[i], 51,102,153,204, x, y);
        }
    }

    graphics_clear(0.2, 0.4, 0.6, 0.8, 1, 0);
    graphics_set_blend(GRAPHICS_BLEND_ADD,
                       GRAPHICS_BLEND_ZERO, GRAPHICS_BLEND_DEST_ALPHA);
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, white_quad_vertices, basic_vertex_format,
        sizeof(*white_quad_vertices), lenof(white_quad_vertices));
    CHECK_TRUE(graphics_read_pixels(0, 0, 64, 64, pixels));
    for (int i = 0; i < 64*64*4; i += 4) {
        const int x = (i/4) % 64, y = (i/4) / 64;
        if (x >= 16 && x < 48) {
            CHECK_PIXEL_NEAR(&pixels[i], 41,82,122,163, 1, x, y);
        } else {
            CHECK_PIXEL(&pixels[i], 51,102,153,204, x, y);
        }
    }

    framebuffer_destroy(framebuffer);
    mem_free(pixels);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_alpha_blend_separate)
{
#ifdef SIL_PLATFORM_PSP
    SKIP("Alpha framebuffers not supported on PSP.");
#endif

    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(64*64*4, 0, 0));

    int framebuffer;
    CHECK_TRUE(framebuffer = framebuffer_create(64, 64, FBCOLOR_RGBA8, 16, 8));
    framebuffer_bind(framebuffer);
    graphics_set_viewport(0, 0, 64, 64);

    graphics_clear(0.2, 0.4, 0.6, 0.8, 1, 0);
    graphics_set_fixed_color(&(Vector4f){1, 1, 1, 1.0f/3.0f});
    graphics_set_blend_alpha(1, GRAPHICS_BLEND_ONE, GRAPHICS_BLEND_SRC_ALPHA);
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, white_quad_vertices, basic_vertex_format,
        sizeof(*white_quad_vertices), lenof(white_quad_vertices));
    CHECK_TRUE(graphics_read_pixels(0, 0, 64, 64, pixels));
    for (int i = 0; i < 64*64*4; i += 4) {
        const int x = (i/4) % 64, y = (i/4) / 64;
        if (x >= 16 && x < 48) {
            CHECK_PIXEL_NEAR(&pixels[i], 119,153,187,153, 1, x, y);
        } else {
            CHECK_PIXEL(&pixels[i], 51,102,153,204, x, y);
        }
    }

    graphics_clear(0.2, 0.4, 0.6, 0.8, 1, 0);
    /* This should not affect the alpha channel blend factors. */
    graphics_set_blend(GRAPHICS_BLEND_ADD,
                       GRAPHICS_BLEND_SRC_ALPHA, GRAPHICS_BLEND_DEST_ALPHA);
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, white_quad_vertices, basic_vertex_format,
        sizeof(*white_quad_vertices), lenof(white_quad_vertices));
    CHECK_TRUE(graphics_read_pixels(0, 0, 64, 64, pixels));
    for (int i = 0; i < 64*64*4; i += 4) {
        const int x = (i/4) % 64, y = (i/4) / 64;
        if (x >= 16 && x < 48) {
            CHECK_PIXEL_NEAR(&pixels[i], 126,167,207,153, 1, x, y);
        } else {
            CHECK_PIXEL(&pixels[i], 51,102,153,204, x, y);
        }
    }

    graphics_clear(0.2, 0.4, 0.6, 0.8, 1, 0);
    /* Color arguments to graphics_set_blend_alpha() should be treated as
     * alpha factors. */
    graphics_set_blend_alpha(1, GRAPHICS_BLEND_SRC_COLOR,
                             GRAPHICS_BLEND_DEST_COLOR);
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, white_quad_vertices, basic_vertex_format,
        sizeof(*white_quad_vertices), lenof(white_quad_vertices));
    CHECK_TRUE(graphics_read_pixels(0, 0, 64, 64, pixels));
    for (int i = 0; i < 64*64*4; i += 4) {
        const int x = (i/4) % 64, y = (i/4) / 64;
        if (x >= 16 && x < 48) {
            CHECK_PIXEL_NEAR(&pixels[i], 126,167,207,192, 1, x, y);
        } else {
            CHECK_PIXEL(&pixels[i], 51,102,153,204, x, y);
        }
    }

    graphics_clear(0.2, 0.4, 0.6, 0.8, 1, 0);
    /* This should not affect the alpha channel blend factors but should
     * change the alpha channel blend operation. */
    graphics_set_blend(GRAPHICS_BLEND_RSUB,
                       GRAPHICS_BLEND_CONSTANT, GRAPHICS_BLEND_INV_SRC_ALPHA);
    graphics_set_blend_color(&(Vector4f){1.0f/17.0f, 1.0f/17.0f, 1.0f/17.0f,
                                         1.0f/17.0f});
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, white_quad_vertices, basic_vertex_format,
        sizeof(*white_quad_vertices), lenof(white_quad_vertices));
    CHECK_TRUE(graphics_read_pixels(0, 0, 64, 64, pixels));
    for (int i = 0; i < 64*64*4; i += 4) {
        const int x = (i/4) % 64, y = (i/4) / 64;
        if (x >= 16 && x < 48) {
            CHECK_PIXEL_NEAR(&pixels[i], 19,53,87,135, 1, x, y);
        } else {
            CHECK_PIXEL(&pixels[i], 51,102,153,204, x, y);
        }
    }

    graphics_clear(0.2, 0.4, 0.6, 0.8, 1, 0);
    /* This should revert the alpha channel blend factors to the primary
     * (color channel) blend factors. */
    graphics_set_blend_alpha(0, 0, 0);
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, white_quad_vertices, basic_vertex_format,
        sizeof(*white_quad_vertices), lenof(white_quad_vertices));
    CHECK_TRUE(graphics_read_pixels(0, 0, 64, 64, pixels));
    for (int i = 0; i < 64*64*4; i += 4) {
        const int x = (i/4) % 64, y = (i/4) / 64;
        if (x >= 16 && x < 48) {
            CHECK_PIXEL_NEAR(&pixels[i], 19,53,87,131, 1, x, y);
        } else {
            CHECK_PIXEL(&pixels[i], 51,102,153,204, x, y);
        }
    }

    graphics_clear(0.2, 0.4, 0.6, 0.8, 1, 0);
    /* This should set the blend factors for both color and alpha channels. */
    graphics_set_blend(GRAPHICS_BLEND_ADD,
                       GRAPHICS_BLEND_ZERO, GRAPHICS_BLEND_SRC_ALPHA);
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, white_quad_vertices, basic_vertex_format,
        sizeof(*white_quad_vertices), lenof(white_quad_vertices));
    CHECK_TRUE(graphics_read_pixels(0, 0, 64, 64, pixels));
    for (int i = 0; i < 64*64*4; i += 4) {
        const int x = (i/4) % 64, y = (i/4) / 64;
        if (x >= 16 && x < 48) {
            CHECK_PIXEL_NEAR(&pixels[i], 17,34,51,68, 1, x, y);
        } else {
            CHECK_PIXEL(&pixels[i], 51,102,153,204, x, y);
        }
    }

    graphics_clear(0.2, 0.4, 0.6, 0.8, 1, 0);
    /* Invalid values should not change the current alpha blend state. */
    graphics_set_blend_alpha(1, -1, GRAPHICS_BLEND_ZERO);
    graphics_set_blend_alpha(1, GRAPHICS_BLEND_ZERO, -1);
    graphics_set_blend(GRAPHICS_BLEND_ADD,
                       GRAPHICS_BLEND_SRC_ALPHA, GRAPHICS_BLEND_ONE);
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, white_quad_vertices, basic_vertex_format,
        sizeof(*white_quad_vertices), lenof(white_quad_vertices));
    CHECK_TRUE(graphics_read_pixels(0, 0, 64, 64, pixels));
    for (int i = 0; i < 64*64*4; i += 4) {
        const int x = (i/4) % 64, y = (i/4) / 64;
        if (x >= 16 && x < 48) {
            CHECK_PIXEL_NEAR(&pixels[i], 136,187,238,232, 1, x, y);
        } else {
            CHECK_PIXEL(&pixels[i], 51,102,153,204, x, y);
        }
    }

    framebuffer_destroy(framebuffer);
    mem_free(pixels);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_alpha_color_write)
{
#ifdef SIL_PLATFORM_PSP
    SKIP("Alpha framebuffers not supported on PSP.");
#endif

    uint8_t *pixels;
    ASSERT(pixels = mem_alloc(64*64*4, 0, 0));

    int framebuffer;
    CHECK_TRUE(framebuffer = framebuffer_create(64, 64, FBCOLOR_RGBA8, 16, 8));
    framebuffer_bind(framebuffer);
    graphics_set_viewport(0, 0, 64, 64);

    graphics_clear(0.2, 0.4, 0.6, 0.8, 1, 0);
    graphics_enable_color_write(1, 1, 1, 0);
    graphics_clear(0.8, 0.6, 0.4, 0.2, 1, 0);
#ifdef SIL_PLATFORM_WINDOWS
    mem_free(safe_clear_leak_cover);
    safe_clear_leak_cover = NULL;
#endif
    CHECK_TRUE(graphics_read_pixels(0, 0, 64, 64, pixels));
    for (int i = 0; i < 64*64*4; i += 4) {
        const int x = (i/4) % 64, y = (i/4) / 64;
        CHECK_PIXEL(&pixels[i], 204,153,102,204, x, y);
    }

    framebuffer_destroy(framebuffer);
    mem_free(pixels);
    return 1;
}

/*************************************************************************/
/******************** Miscellaneous framebuffer tests ********************/
/*************************************************************************/

TEST(test_multiple_framebuffers)
{
    int framebuffer1, framebuffer2;
    uint8_t *pixels;
    int texture;

    graphics_clear(0, 0, 1, 0, 1, 0);

    CHECK_TRUE(framebuffer1 = framebuffer_create(64, 64, FBCOLOR_RGB8, 16, 8));
    CHECK_INTEQUAL(framebuffer_width(framebuffer1), 64);
    CHECK_INTEQUAL(framebuffer_height(framebuffer1), 64);
    framebuffer_set_antialias(framebuffer1, 0);
    framebuffer_bind(framebuffer1);

    CHECK_TRUE(framebuffer2 = framebuffer_create(32, 32, FBCOLOR_RGB8, 16, 8));
    CHECK_INTEQUAL(framebuffer_width(framebuffer2), 32);
    CHECK_INTEQUAL(framebuffer_height(framebuffer2), 32);
    framebuffer_set_antialias(framebuffer2, 0);

    graphics_set_viewport(0, 0, 64, 64);
    graphics_clear(1, 0, 0, 0, 1, 0);
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, white_quad_vertices, basic_vertex_format,
        sizeof(*white_quad_vertices), lenof(white_quad_vertices));

    framebuffer_bind(framebuffer2);
    graphics_set_viewport(0, 0, 32, 32);
    graphics_clear(0, 1, 0, 0, 1, 0);

    framebuffer_bind(framebuffer1);
    graphics_set_viewport(0, 0, 64, 64);
    texture_apply(0, framebuffer_get_texture(framebuffer2));
    graphics_set_blend(GRAPHICS_BLEND_ADD, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,-1,0},
                                   &(Vector2f){0,0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,0,0},
                                   &(Vector2f){0,1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,0,0},
                                   &(Vector2f){1,1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,-1,0},
                                   &(Vector2f){1,0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    framebuffer_discard_data(framebuffer2);
    ASSERT(pixels = mem_alloc(64*64*4, 0, 0));
    CHECK_TRUE(graphics_read_pixels(0, 0, 64, 64, pixels));
    for (int i = 0; i < 64*64*4; i += 4) {
        const int x = (i/4) % 64;
        const int y = (i/4) / 64;
        const int r = (x < 32 && y < 32) ? 0 : 255;
        const int g = (x < 32 && y < 32) ? 255 :
                      (x >= 16 && x < 48) ? 255 : 0;
        const int b = (x < 32 && y < 32) ? 0 :
                      (x >= 16 && x < 48) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], r,g,b,255, x, y);
    }
    mem_free(pixels);
    CHECK_TRUE(texture = texture_create_from_display(0, 0, 64, 64, 1, 0, 0));
    CHECK_INTEQUAL(texture_width(texture), 64);
    CHECK_INTEQUAL(texture_height(texture), 64);
    CHECK_TRUE(pixels = texture_lock(texture));
    for (int i = 0; i < 64*64*4; i += 4) {
        const int x = (i/4) % 64;
        const int y = (i/4) / 64;
        const int r = (x < 32 && y < 32) ? 0 : 255;
        const int g = (x < 32 && y < 32) ? 255 :
                      (x >= 16 && x < 48) ? 255 : 0;
        const int b = (x < 32 && y < 32) ? 0 :
                      (x >= 16 && x < 48) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], r,g,b,255, x, y);
    }
    texture_destroy(texture);

    framebuffer_bind(0);
    graphics_set_viewport(0, 0, TESTW, TESTH);
    texture_apply(0, framebuffer_get_texture(framebuffer1));
    graphics_set_blend(GRAPHICS_BLEND_ADD, 1, 0);
    const float x0 = -1, y0 = -1;
    const float x1 = -1 + 2*(64.0f/TESTW);
    const float y1 = -1 + 2*(64.0f/TESTH);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y0,0},
                                   &(Vector2f){0,0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y1,0},
                                   &(Vector2f){0,1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y1,0},
                                   &(Vector2f){1,1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y0,0},
                                   &(Vector2f){1,0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int r = (x < 32 && y < 32) ? 0 :
                      (x < 64 && y < 64) ? 255 : 0;
        const int g = (x < 32 && y < 32) ? 255 :
                      ((x >= 16 && x < 48) && y < 64) ? 255 : 0;
        const int b = (x < 32 && y < 32) ? 0 :
                      ((x >= 16 && x < 48) || x >= 64 || y >= 64) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], r,g,b,255, x, y);
    }
    mem_free(pixels);
    CHECK_TRUE(
        texture = texture_create_from_display(0, 0, TESTW, TESTH, 1, 0, 0));
    CHECK_INTEQUAL(texture_width(texture), TESTW);
    CHECK_INTEQUAL(texture_height(texture), TESTH);
    CHECK_TRUE(pixels = texture_lock(texture));
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int r = (x < 32 && y < 32) ? 0 :
                      (x < 64 && y < 64) ? 255 : 0;
        const int g = (x < 32 && y < 32) ? 255 :
                      ((x >= 16 && x < 48) && y < 64) ? 255 : 0;
        const int b = (x < 32 && y < 32) ? 0 :
                      ((x >= 16 && x < 48) || x >= 64 || y >= 64) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], r,g,b,255, x, y);
    }
    texture_destroy(texture);

    framebuffer_destroy(framebuffer1);
    framebuffer_destroy(framebuffer2);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_hole_in_array)
{
    int framebuffer1, framebuffer2, framebuffer3;

    CHECK_TRUE(framebuffer1 = framebuffer_create(64, 64, FBCOLOR_RGB8, 16, 8));
    CHECK_TRUE(framebuffer2 = framebuffer_create(32, 32, FBCOLOR_RGB8, 16, 8));

    framebuffer_destroy(framebuffer1);

    CHECK_INTEQUAL(framebuffer_create(16, 16, FBCOLOR_RGB8, 16, 8),
                   framebuffer1);
    CHECK_TRUE(framebuffer3 = framebuffer_create(8, 8, FBCOLOR_RGB8, 16, 8));

    CHECK_INTEQUAL(framebuffer_width(framebuffer1), 16);
    CHECK_INTEQUAL(framebuffer_height(framebuffer1), 16);
    CHECK_INTEQUAL(framebuffer_width(framebuffer2), 32);
    CHECK_INTEQUAL(framebuffer_height(framebuffer2), 32);
    CHECK_INTEQUAL(framebuffer_width(framebuffer3), 8);
    CHECK_INTEQUAL(framebuffer_height(framebuffer3), 8);

    framebuffer_destroy(framebuffer2);
    framebuffer_cleanup();

    return 1;
}

/*************************************************************************/
/*************************************************************************/
