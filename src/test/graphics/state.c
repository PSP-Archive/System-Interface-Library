/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/graphics/state.c: Tests for graphics render state management.
 */

#include "src/base.h"
#include "src/graphics.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/test/base.h"
#include "src/test/graphics/internal.h"
#include "src/texture.h"

/*************************************************************************/
/*************************** Common test data ****************************/
/*************************************************************************/

/* Flags for whether each of the rendering parameters has been modified.
 * We use these to avoid resetting a parameter in cleanup() until it has
 * actually been used, so we can test default behavior for each of the
 * settings. */
static uint8_t used_viewport;
static uint8_t used_clip_region;
static uint8_t used_color_write;
static uint8_t used_depth_test;
static uint8_t used_depth_test_comparison;
static uint8_t used_depth_write;
static uint8_t used_depth_range;
static uint8_t used_blend;
static uint8_t used_projection_matrix;
static uint8_t used_view_matrix;
static uint8_t used_model_matrix;
static uint8_t used_alpha_test;
static uint8_t used_alpha_test_comparison;
static uint8_t used_face_cull;
static uint8_t used_fixed_color;
static uint8_t used_fog;
static uint8_t used_fog_range;
static uint8_t used_fog_color;
static uint8_t used_point_size;
static uint8_t used_stencil;
static uint8_t used_texture_offset;

#ifdef SIL_PLATFORM_WINDOWS
/* Allocated memory block used to avoid a spurious memory leak report after
 * the first clear operation which triggers d3d_state_safe_clear(). */
#include "src/sysdep/windows/internal.h"  // For TEST_windows_force_direct3d.
#include "src/sysdep/windows/d3d-internal.h"  // For structure sizes.
static void *safe_clear_leak_cover;
#endif

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

/**
 * CHECK_SET_MATRIX:  Set the given matrix, then read it back and make sure
 * the read-back value matches what was set.
 *
 * [Parameters]
 *     id: Matrix ID (GRAPHICS_MATRIX_*).
 *     matrix: Pointer to matrix to set.
 * [Return value]
 *     True if the matrix was properly set, false if not.
 */
typedef enum GraphicsMatrixID {
    GRAPHICS_MATRIX_PROJECTION,
    GRAPHICS_MATRIX_VIEW,
    GRAPHICS_MATRIX_MODEL,
} GraphicsMatrixID;
static int check_set_matrix(GraphicsMatrixID id, const Matrix4f *matrix)
{
    Matrix4f M;

    switch (id) {
      case GRAPHICS_MATRIX_PROJECTION:
        graphics_set_projection_matrix(matrix);
        graphics_get_projection_matrix(&M);
        break;
      case GRAPHICS_MATRIX_VIEW:
        graphics_set_view_matrix(matrix);
        graphics_get_view_matrix(&M);
        break;
      case GRAPHICS_MATRIX_MODEL:
        graphics_set_model_matrix(matrix);
        graphics_get_model_matrix(&M);
        break;
    }
    if (memcmp(matrix, &M, sizeof(M)) != 0) {
        FAIL("Matrix ID %d did not read back correctly:"
             "\n    Expected:"
             " [%g %g %g %g] [%g %g %g %g] [%g %g %g %g] [%g %g %g %g]"
             "\n   Read back:"
             " [%g %g %g %g] [%g %g %g %g] [%g %g %g %g] [%g %g %g %g]",
             id,
             matrix->_11, matrix->_12, matrix->_13, matrix->_14,
             matrix->_21, matrix->_22, matrix->_23, matrix->_24,
             matrix->_31, matrix->_32, matrix->_33, matrix->_34,
             matrix->_41, matrix->_42, matrix->_43, matrix->_44,
             M._11, M._12, M._13, M._14, M._21, M._22, M._23, M._24,
             M._31, M._32, M._33, M._34, M._41, M._42, M._43, M._44);
    }

    return 1;
}

#define CHECK_SET_MATRIX(...)  CHECK_TRUE(check_set_matrix(__VA_ARGS__))

/*-----------------------------------------------------------------------*/

/**
 * blend_color_squaring_supported:  Return whether the system's graphics
 * implementation supports multiplying a color by itself when blending
 * (e.g., using SRC_COLOR as the source blend factor).
 *
 * [Return value]
 *     True if the system supports color squaring, false if not.
 */
static int blend_color_squaring_supported(void)
{
#if defined(SIL_PLATFORM_PSP)
    return 0;
#else
    return 1;
#endif
}

/*-----------------------------------------------------------------------*/

/**
 * blend_dest_alpha_supported:  Return whether the system's graphics
 * implementation supports using the destination alpha channel as a
 * blend factor.
 *
 * [Return value]
 *     True if the system supports destination alpha as a blend factor,
 *     false if not.
 */
static int blend_dest_alpha_supported(void)
{
#ifdef SIL_PLATFORM_PSP
    return 0;
#else
    return 1;
#endif
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

static int do_test_graphics_state(void);

int test_graphics_state(void)
{
    used_viewport = 0;
    used_clip_region = 0;
    used_depth_test = 0;
    used_depth_test_comparison = 0;
    used_depth_write = 0;
    used_depth_range = 0;
    used_blend = 0;
    used_projection_matrix = 0;
    used_view_matrix = 0;
    used_model_matrix = 0;
    used_alpha_test = 0;
    used_alpha_test_comparison = 0;
    used_face_cull = 0;
    used_fixed_color = 0;
    used_fog = 0;
    used_fog_range = 0;
    used_fog_color = 0;
    used_point_size = 0;
    used_stencil = 0;
    used_texture_offset = 0;

#ifdef SIL_PLATFORM_WINDOWS
    if (TEST_windows_force_direct3d) {
        ASSERT(safe_clear_leak_cover = mem_alloc(
                   2*sizeof(D3DSysShader) + sizeof(D3DSysShaderPipeline),
                   0, 0));
    }
#endif

    const int result = run_tests_in_window(do_test_graphics_state);

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

DEFINE_GENERIC_TEST_RUNNER(do_test_graphics_state)

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
    if (used_viewport) {
        graphics_set_viewport(0, 0, graphics_display_width(),
                              graphics_display_height());
    }
    if (used_clip_region) {
        graphics_set_clip_region(0, 0, 0, 0);
    }
    if (used_color_write) {
        graphics_enable_color_write(1, 1, 1, 1);
    }
    if (used_depth_test) {
        graphics_enable_depth_test(0);
    }
    if (used_depth_test_comparison) {
        graphics_set_depth_test_comparison(GRAPHICS_COMPARISON_LESS);
    }
    if (used_depth_write) {
        graphics_enable_depth_write(1);
    }
    if (used_depth_range) {
        graphics_set_depth_range(0, 1);
    }
    if (used_blend) {
        CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD,
                                      GRAPHICS_BLEND_SRC_ALPHA,
                                      GRAPHICS_BLEND_INV_SRC_ALPHA));
        graphics_set_blend_color(&(Vector4f){0,0,0,0});
    }
    if (used_projection_matrix) {
        graphics_set_projection_matrix(&mat4_identity);
    }
    if (used_view_matrix) {
        graphics_set_view_matrix(&mat4_identity);
    }
    if (used_model_matrix) {
        graphics_set_model_matrix(&mat4_identity);
    }
    if (used_alpha_test) {
        graphics_enable_alpha_test(0);
        graphics_set_alpha_test_reference(0);
    }
    if (used_alpha_test_comparison) {
        graphics_set_alpha_test_comparison(GRAPHICS_COMPARISON_GREATER_EQUAL);
    }
    if (used_face_cull) {
        graphics_set_face_cull(GRAPHICS_FACE_CULL_NONE);
    }
    if (used_fixed_color) {
        graphics_set_fixed_color(&(Vector4f){1,1,1,1});
    }
    if (used_fog) {
        graphics_enable_fog(0);
    }
    if (used_fog_range) {
        graphics_set_fog_start(0);
        graphics_set_fog_end(1);
    }
    if (used_fog_color) {
        graphics_set_fog_color(&(Vector4f){1,1,1,1});
    }
    if (used_point_size) {
        graphics_set_point_size(1);
    }
    if (used_stencil) {
        graphics_enable_stencil_test(0);
        graphics_set_stencil_comparison(GRAPHICS_COMPARISON_TRUE, 0, ~0U);
        graphics_set_stencil_operations(GRAPHICS_STENCIL_KEEP,
                                        GRAPHICS_STENCIL_KEEP,
                                        GRAPHICS_STENCIL_KEEP);
    }
    if (used_texture_offset) {
        graphics_set_texture_offset(&(Vector2f){0,0});
    }

    graphics_finish_frame();
    graphics_flush_resources();
    return 1;
}

/*************************************************************************/
/******************** Rendering/clipping region tests ********************/
/*************************************************************************/

TEST(test_viewport)
{
    used_viewport = 1;

    uint8_t *pixels;

    /* By default, the viewport should match the display size, but our test
     * wrapper presets the viewport to a fixed size, so we can't check the
     * default here.  However, in typical applications the client code will
     * set the viewport manually before doing any rendering, so we don't
     * bother checking the default. */

    graphics_set_viewport(64, 40, 48, 32);
    CHECK_INTEQUAL(graphics_viewport_width(), 48);
    CHECK_INTEQUAL(graphics_viewport_height(), 32);

    /* Check that graphics_clear() ignores the viewport. */
    graphics_clear(0, 0, 1, 0, 1, 0);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 0,0,255,255, (i/4) % TESTW, (i/4) / TESTW);
    }
    mem_free(pixels);

    /* Check that primitive rendering honors the viewport. */
    draw_square(0, 1,1,1,1);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = ((x >= 76 && x < 100) && (y >= 48 && y < 64)) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,255,255, x, y);
    }
    mem_free(pixels);

    /* Check that attempts to render outside the viewport are clipped. */
    ASSERT(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    ASSERT(graphics_add_vertex(&(Vector3f){-2,-2,0}, NULL, NULL));
    ASSERT(graphics_add_vertex(&(Vector3f){-2,+2,0}, NULL, NULL));
    ASSERT(graphics_add_vertex(&(Vector3f){+2,+2,0}, NULL, NULL));
    ASSERT(graphics_add_vertex(&(Vector3f){+2,-2,0}, NULL, NULL));
    ASSERT(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = ((x >= 64 && x < 112) && (y >= 40 && y < 72)) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,255,255, x, y);
    }
    mem_free(pixels);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_viewport_invalid)
{
    uint8_t *pixels;

    graphics_set_viewport(64, 40, 48, 32);

    /* None of these should alter the rendering viewport. */
    graphics_set_viewport(-1, 10, 100, 50);
    graphics_set_viewport(10, -1, 100, 50);
    graphics_set_viewport(10, 10, 0, 50);
    graphics_set_viewport(10, 10, 100, 0);

    draw_square(0, 1,1,1,1);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = ((x >= 76 && x < 100) && (y >= 48 && y < 64)) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_clip_region)
{
    used_clip_region = 1;

    uint8_t pixels[64*64*4];

    /* From here on down, we force the viewport to exactly 64x64 to
     * simplify coordinate tests (and save time in screen grabs). */
    graphics_set_viewport(0, 0, 64, 64);

    /* Check that no clip region is active by default. */
    graphics_clear(0, 0, 1, 0, 1, 0);
    ASSERT(graphics_read_pixels(0, 0, 64, 64, pixels));
    for (int i = 0; i < 64*64*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 0,0,255,255, (i/4) % 64, (i/4) / 64);
    }

    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_set_clip_region(20, 24, 8, 12);

    /* Check that primitive rendering honors the clipping region. */
    draw_square(0, 1,1,1,1);
    ASSERT(graphics_read_pixels(0, 0, 64, 64, pixels));
    for (int i = 0; i < 64*64*4; i += 4) {
        const int x = (i/4) % 64;
        const int y = (i/4) / 64;
        const int p = ((x >= 20 && x < 28) && (y >= 24 && y < 36)) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }

    /* Check that graphics_clear() also honors the clipping region. */
    graphics_clear(0, 0, 1, 0, 1, 0);
#ifdef SIL_PLATFORM_WINDOWS
    mem_free(safe_clear_leak_cover);
    safe_clear_leak_cover = NULL;
#endif
    ASSERT(graphics_read_pixels(0, 0, 64, 64, pixels));
    for (int i = 0; i < 64*64*4; i += 4) {
        const int x = (i/4) % 64;
        const int y = (i/4) / 64;
        const int p = ((x >= 20 && x < 28) && (y >= 24 && y < 36)) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], 0,0,p,255, x, y);
    }

    /* Check that setting a clipping region outside the viewport results
     * in nothing being rendered. */
    graphics_set_clip_region(64, 64, 16, 16);
    graphics_clear(1, 0, 0, 0, 1, 0);
    draw_square(0, 1,1,1,1);
    ASSERT(graphics_read_pixels(0, 0, 64, 64, pixels));
    for (int i = 0; i < 64*64*4; i += 4) {
        const int x = (i/4) % 64;
        const int y = (i/4) / 64;
        const int p = ((x >= 20 && x < 28) && (y >= 24 && y < 36)) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], 0,0,p,255, x, y);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_clip_region_invalid)
{
    uint8_t pixels[64*64*4];

    graphics_set_viewport(0, 0, 64, 64);

    graphics_set_clip_region(20, 24, 8, 12);
    /* None of these should alter the clipping region. */
    graphics_set_clip_region(-1, 10, 100, 50);
    graphics_set_clip_region(10, -1, 100, 50);
    graphics_set_clip_region(10, 10, -1, 50);
    graphics_set_clip_region(10, 10, 100, -1);
    graphics_set_clip_region(10, 10, 0, 50);
    graphics_set_clip_region(10, 10, 100, 0);

    draw_square(0, 1,1,1,1);
    ASSERT(graphics_read_pixels(0, 0, 64, 64, pixels));
    for (int i = 0; i < 64*64*4; i += 4) {
        const int x = (i/4) % 64;
        const int y = (i/4) / 64;
        const int p = ((x >= 20 && x < 28) && (y >= 24 && y < 36)) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }

    return 1;
}

/*************************************************************************/
/********************** Color buffer masking tests ***********************/
/*************************************************************************/

TEST(test_color_write)
{
    used_color_write = 1;

    graphics_set_viewport(0, 0, 64, 64);

    /* Previous calls have ensured that color writing is enabled by default
     * for all components.  Check that we can disable each component
     * independently.  (Alpha masking is tested in the framebuffer tests.) */
    graphics_enable_color_write(1, 1, 1, 1);
    draw_square(0, 0.6,0.6,0.6,1);
    graphics_enable_color_write(0, 1, 1, 1);
    draw_square(0, 1,1,1,1);
    CHECK_SQUARE(0.6,1,1);

    graphics_enable_color_write(1, 1, 1, 1);
    draw_square(0, 0.6,0.6,0.6,1);
    graphics_enable_color_write(1, 0, 1, 1);
    draw_square(0, 1,1,1,1);
    CHECK_SQUARE(1,0.6,1);

    graphics_enable_color_write(1, 1, 1, 1);
    draw_square(0, 0.6,0.6,0.6,1);
    graphics_enable_color_write(1, 1, 0, 1);
    draw_square(0, 1,1,1,1);
    CHECK_SQUARE(1,1,0.6);

    /* Check disabling all components at once. */
    graphics_enable_color_write(1, 1, 1, 1);
    draw_square(0, 0.6,0.6,0.6,1);
    graphics_enable_color_write(0, 0, 0, 1);
    draw_square(0, 1,1,1,1);
    CHECK_SQUARE(0.6,0.6,0.6);

    /* Check that disabled components are not cleared by graphics_clear(). */
    graphics_enable_color_write(1, 1, 1, 1);
    draw_square(0, 1,1,1,1);
    graphics_enable_color_write(1, 0, 1, 1);
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_SQUARE(0,1,0);

    return 1;
}

/*************************************************************************/
/************************** Depth buffer tests ***************************/
/*************************************************************************/

TEST(test_depth_test)
{
    used_depth_test = 1;

    graphics_set_viewport(0, 0, 64, 64);

    /* Check that depth testing is disabled by default. */
    draw_square(-1, 1,1,1,1);
    draw_square(0, 1,0,0,1);
    CHECK_SQUARE(1,0,0);

    /* Check that depth testing works. */
    graphics_enable_depth_test(1);
    /* This should be drawn since depth writing is automatically disabled
     * when depth testing is disabled. */
    draw_square(0.5, 0,1,0,1);
    CHECK_SQUARE(0,1,0);
    /* This should be drawn because it's in front of the last one. */
    draw_square(-1, 0,0,1,1);
    CHECK_SQUARE(0,0,1);
    /* This should _not_ be drawn because it's behind the last one. */
    draw_square(0, 1,1,1,1);
    CHECK_SQUARE(0,0,1);

    /* Check that graphics_clear() clears the depth buffer when depth
     * testing is enabled. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    draw_square(0, 1,1,1,1);
    CHECK_SQUARE(1,1,1);

    /* Check that graphics_clear() clears the depth buffer even when depth
     * testing is disabled. */
    graphics_enable_depth_test(0);
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_enable_depth_test(1);
    draw_square(0.5, 1,0,1,1);
    CHECK_SQUARE(1,0,1);

    /* Check that depth testing can be disabled again. */
    graphics_enable_depth_test(0);
    draw_square(-1, 1,1,1,1);
    draw_square(0, 1,0,0,1);
    CHECK_SQUARE(1,0,0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_depth_test_comparison)
{
    used_depth_test_comparison = 1;

    graphics_set_viewport(0, 0, 64, 64);
    graphics_enable_depth_test(1);

    /* Check that only lesser depths are not drawn by default. */
    draw_square(0, 0,1,0,1);
    draw_square(0, 0,0,1,1);
    draw_square(1, 1,0,0,1);
    CHECK_SQUARE(0,1,0);

    /* Check each of the comparison methods. */
    graphics_set_depth_test_comparison(GRAPHICS_COMPARISON_LESS_EQUAL);
    draw_square(-1, 0,1,0,1);
    CHECK_SQUARE(0,1,0);
    draw_square(-1, 0,0,1,1);
    draw_square(0, 1,0,0,1);
    CHECK_SQUARE(0,0,1);

    graphics_set_depth_test_comparison(GRAPHICS_COMPARISON_GREATER_EQUAL);
    draw_square(0, 0,1,0,1);
    CHECK_SQUARE(0,1,0);
    draw_square(0, 0,0,1,1);
    draw_square(-1, 1,0,0,1);
    CHECK_SQUARE(0,0,1);

    graphics_set_depth_test_comparison(GRAPHICS_COMPARISON_GREATER);
    draw_square(1, 0,1,0,1);
    draw_square(1, 0,0,1,1);
    draw_square(0, 1,0,0,1);
    CHECK_SQUARE(0,1,0);

    /* Check that disabling and enabling the depth test preserves the
     * comparison type. */
    graphics_enable_depth_test(0);
    graphics_enable_depth_test(1);
    draw_square(0, 1,0,0,1);
    CHECK_SQUARE(0,1,0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_depth_test_comparison_invalid)
{
    graphics_set_viewport(0, 0, 64, 64);
    graphics_enable_depth_test(1);
    draw_square(-1, 1,1,1,1);

    graphics_set_depth_test_comparison(GRAPHICS_COMPARISON_GREATER);
    /* These should not change the current comparison function. */
    graphics_set_depth_test_comparison(-1);
    graphics_set_depth_test_comparison(GRAPHICS_COMPARISON_FALSE);
    graphics_set_depth_test_comparison(GRAPHICS_COMPARISON_TRUE);
    graphics_set_depth_test_comparison(GRAPHICS_COMPARISON_EQUAL);
    graphics_set_depth_test_comparison(GRAPHICS_COMPARISON_NOT_EQUAL);
    draw_square(0, 0,1,0,1);
    draw_square(0, 0,0,1,1);
    draw_square(-1, 1,0,0,1);
    CHECK_SQUARE(0,1,0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_depth_precision)
{
    graphics_set_viewport(0, 0, 64, 64);
    graphics_enable_depth_test(1);
    graphics_set_depth_test_comparison(GRAPHICS_COMPARISON_LESS);

    /* Check that we have at least 16 bits of precision. */
    draw_square(0, 1,1,1,1);
    draw_square(-1.1/32768.0, 1,0,0,1);
    CHECK_SQUARE(1,0,0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_depth_write)
{
    used_depth_write = 1;

    graphics_set_viewport(0, 0, 64, 64);
    graphics_enable_depth_test(1);

    /* Check that depth writing is enabled by default. */
    draw_square(0, 1,1,1,1);
    draw_square(1, 1,0,0,1);
    CHECK_SQUARE(1,1,1);

    /* Check that depth writing can be disabled. */
    graphics_enable_depth_write(0);
    draw_square(-1, 0,1,0,1);
    draw_square(-0.5, 0,0,1,1);
    CHECK_SQUARE(0,0,1);
    /* Check that depth testing is still enabled. */
    draw_square(1, 1,1,1,1);
    CHECK_SQUARE(0,0,1);

    /* Check that graphics_clear() does not clear the depth buffer when
     * depth writing is disabled. */
    graphics_clear(0,0,0,0, 1, 0);
    draw_square(1, 1,1,1,1);
    CHECK_SQUARE(0,0,0);

    /* Check that depth writing can be enabled again. */
    graphics_enable_depth_write(1);
    draw_square(-0.5, 1,0,0,1);
    draw_square(1, 0,1,0,1);
    CHECK_SQUARE(1,0,0);

    return 1;
}

/*************************************************************************/
/**************************** Blending tests *****************************/
/*************************************************************************/

TEST(test_blend)
{
    used_blend = 1;

    graphics_set_viewport(0, 0, 64, 64);

    /* Check that the default blend mode is alpha blending. */
    draw_square(0, 1,1,1,1);
    draw_square(0, 0,0,0,0.2);
    CHECK_SQUARE(0.8, 0.8, 0.8);

    /* Check that blend-disabled rendering works. */
    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD, 1, 0));
    draw_square(0, 0.2, 0.2, 0.2, 0.2);
    CHECK_SQUARE(0.2, 0.2, 0.2);

    /* Check that the default blend color is (0,0,0,0). */
    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD,
                                  GRAPHICS_BLEND_CONSTANT, 0));
    draw_square(0, 1,1,1,1);
    CHECK_SQUARE(0, 0, 0);

    /* Check the graphics_set_no_blend() convenience function. */
    graphics_set_no_blend();
    draw_square(0, 0.2, 0.2, 0.2, 0.2);
    CHECK_SQUARE(0.2, 0.2, 0.2);

    /* Check all blend values for src with dest==0. */

    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD, 1, 0));
    draw_square(0, 0.2, 0.4, 0.6, 1);
    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD, 0, 0));
    draw_square(0, 1.0, 0.8, 0.2, 0.4);
    CHECK_SQUARE(0, 0, 0);

    if (blend_color_squaring_supported()) {
        CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD, 1, 0));
        draw_square(0, 0.2, 0.4, 0.6, 1);
        CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD,
                                      GRAPHICS_BLEND_SRC_COLOR, 0));
        draw_square(0, 1.0, 0.8, 0.2, 0.4);
        CHECK_SQUARE(1.0, 0.64, 0.04);
    }

    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD, 1, 0));
    draw_square(0, 0.2, 0.4, 0.6, 1);
    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD,
                                  GRAPHICS_BLEND_SRC_ALPHA, 0));
    draw_square(0, 1.0, 0.8, 0.2, 0.4);
    CHECK_SQUARE(0.4, 0.32, 0.08);

    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD, 1, 0));
    draw_square(0, 0.2, 0.4, 0.6, 1);
    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD,
                                  GRAPHICS_BLEND_INV_SRC_ALPHA, 0));
    draw_square(0, 1.0, 0.8, 0.2, 0.4);
    CHECK_SQUARE(0.6, 0.48, 0.12);

    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD, 1, 0));
    draw_square(0, 0.2, 0.4, 0.6, 1);
    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD,
                                  GRAPHICS_BLEND_DEST_COLOR, 0));
    draw_square(0, 1.0, 0.8, 0.2, 0.4);
    CHECK_SQUARE(0.2, 0.32, 0.12);

    if (blend_dest_alpha_supported()) {
        CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD, 1, 0));
        draw_square(0, 0.2, 0.4, 0.6, 1);
        CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD,
                                      GRAPHICS_BLEND_DEST_ALPHA, 0));
        draw_square(0, 1.0, 0.8, 0.2, 0.4);
        CHECK_SQUARE(1.0, 0.8, 0.2);

        CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD, 1, 0));
        draw_square(0, 0.2, 0.4, 0.6, 1);
        CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD,
                                      GRAPHICS_BLEND_INV_DEST_ALPHA, 0));
        draw_square(0, 1.0, 0.8, 0.2, 0.4);
        CHECK_SQUARE(0, 0, 0);
    }

    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD, 1, 0));
    draw_square(0, 0.2, 0.4, 0.6, 1);
    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD,
                                  GRAPHICS_BLEND_CONSTANT, 0));
    graphics_set_blend_color(&(Vector4f){0.4, 0.6, 0.2, 1});
    draw_square(0, 1.0, 0.8, 0.2, 0.4);
    CHECK_SQUARE(0.4, 0.48, 0.04);

    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD, 1, 0));
    draw_square(0, 0.2, 0.4, 0.6, 1);
    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD,
                                  GRAPHICS_BLEND_INV_CONSTANT, 0));
    graphics_set_blend_color(&(Vector4f){0.4, 0.6, 0.2, 1});
    draw_square(0, 1.0, 0.8, 0.2, 0.4);
    CHECK_SQUARE(0.6, 0.32, 0.16);

    /* Check all blend values for dest with src==0. */

    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD, 1, 0));
    draw_square(0, 0.2, 0.8, 0.6, 1);
    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD, 0, 1));
    draw_square(0, 1.0, 0.4, 0.2, 0.4);
    CHECK_SQUARE(0.2, 0.8, 0.6);

    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD, 1, 0));
    draw_square(0, 0.2, 0.8, 0.6, 1);
    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD,
                                  0, GRAPHICS_BLEND_SRC_COLOR));
    draw_square(0, 1.0, 0.4, 0.2, 0.4);
    CHECK_SQUARE(0.2, 0.32, 0.12);

    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD, 1, 0));
    draw_square(0, 0.2, 0.8, 0.6, 1);
    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD,
                                  0, GRAPHICS_BLEND_SRC_ALPHA));
    draw_square(0, 1.0, 0.4, 0.2, 0.4);
    CHECK_SQUARE(0.08, 0.32, 0.24);

    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD, 1, 0));
    draw_square(0, 0.2, 0.8, 0.6, 1);
    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD,
                                  0, GRAPHICS_BLEND_INV_SRC_ALPHA));
    draw_square(0, 1.0, 0.4, 0.2, 0.4);
    CHECK_SQUARE(0.12, 0.48, 0.36);

    if (blend_color_squaring_supported()) {
        CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD, 1, 0));
        draw_square(0, 0.2, 0.8, 0.6, 1);
        CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD,
                                      0, GRAPHICS_BLEND_DEST_COLOR));
        draw_square(0, 1.0, 0.4, 0.2, 0.4);
        CHECK_SQUARE(0.04, 0.64, 0.36);
    }

    if (blend_dest_alpha_supported()) {
        CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD, 1, 0));
        draw_square(0, 0.2, 0.4, 0.6, 1);
        CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD,
                                      0, GRAPHICS_BLEND_DEST_ALPHA));
        draw_square(0, 1.0, 0.8, 0.2, 0.4);
        CHECK_SQUARE(0.2, 0.4, 0.6);

        CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD, 1, 0));
        draw_square(0, 0.2, 0.4, 0.6, 1);
        CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD,
                                      0, GRAPHICS_BLEND_INV_DEST_ALPHA));
        draw_square(0, 1.0, 0.8, 0.2, 0.4);
        CHECK_SQUARE(0, 0, 0);
    }

    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD, 1, 0));
    draw_square(0, 0.2, 0.4, 0.6, 1);
    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD,
                                  0, GRAPHICS_BLEND_CONSTANT));
    graphics_set_blend_color(&(Vector4f){0.4, 0.6, 0.2, 1});
    draw_square(0, 1.0, 0.8, 0.2, 0.4);
    CHECK_SQUARE(0.08, 0.24, 0.12);

    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD, 1, 0));
    draw_square(0, 0.2, 0.4, 0.6, 1);
    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD,
                                  0, GRAPHICS_BLEND_INV_CONSTANT));
    graphics_set_blend_color(&(Vector4f){0.4, 0.6, 0.2, 1});
    draw_square(0, 1.0, 0.8, 0.2, 0.4);
    CHECK_SQUARE(0.12, 0.16, 0.48);

    /* blend(1,0) may be handled specially, so check blend(1,1) to ensure
     * the src==1 case is covered. */
    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD, 1, 0));
    draw_square(0, 0.2, 0.4, 0.6, 1);
    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD, 1, 1));
    draw_square(0, 0.4, 0.8, 0.2, 0.5);
    CHECK_SQUARE(0.6, 1.0, 0.8);

    /* Check blend operations other than ADD. */

    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD, 1, 0));
    draw_square(0, 0.2, 0.4, 0.6, 1);
    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_SUB,
                                  GRAPHICS_BLEND_SRC_ALPHA, 1));
    draw_square(0, 0.8, 0.6, 1.0, 0.8);
    CHECK_SQUARE(0.44, 0.08, 0.2);

    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD, 1, 0));
    draw_square(0, 0.4, 0.6, 0.8, 1);
    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_RSUB,
                                  GRAPHICS_BLEND_SRC_ALPHA, 1));
    draw_square(0, 0.8, 0.2, 0.6, 0.4);
    CHECK_SQUARE(0.08, 0.52, 0.56);

    /* Check that changing any single component of the blend color results
     * in rendering differences (as might happen in the presence of
     * optimization bugs). */

    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD,
                                  GRAPHICS_BLEND_CONSTANT, 0));
    graphics_set_blend_color(&(Vector4f){0.6, 0.4, 0.2, 1.0});
    draw_square(0, 1, 1, 1, 1);
    CHECK_SQUARE(0.6, 0.4, 0.2);

    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD,
                                  GRAPHICS_BLEND_CONSTANT, 0));
    graphics_set_blend_color(&(Vector4f){0.4, 0.4, 0.2, 1.0});
    draw_square(0, 1, 1, 1, 1);
    CHECK_SQUARE(0.4, 0.4, 0.2);

    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD,
                                  GRAPHICS_BLEND_CONSTANT, 0));
    graphics_set_blend_color(&(Vector4f){0.4, 0.2, 0.2, 1.0});
    draw_square(0, 1, 1, 1, 1);
    CHECK_SQUARE(0.4, 0.2, 0.2);

    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD,
                                  GRAPHICS_BLEND_CONSTANT, 0));
    graphics_set_blend_color(&(Vector4f){0.4, 0.2, 1.0, 1.0});
    draw_square(0, 1, 1, 1, 1);
    CHECK_SQUARE(0.4, 0.2, 1.0);

    /* We currently can't see the result of setting the alpha component,
     * but we run the test anyway so we can check branch coverage. */
    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD,
                                  GRAPHICS_BLEND_CONSTANT, 0));
    graphics_set_blend_color(&(Vector4f){0.4, 0.2, 1.0, 0.4});
    draw_square(0, 1, 1, 1, 1);
    CHECK_SQUARE(0.4, 0.2, 1.0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_blend_unsupported)
{
    used_blend = 1;

    graphics_set_viewport(0, 0, 64, 64);
    graphics_set_blend_color(&(Vector4f){0.2, 0.4, 0.6, 1});

    /* Attempting to set an unsupported blend factor should fail and should
     * leave the current settings unchanged. */

    if (!blend_color_squaring_supported()) {
        CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD,
                                      GRAPHICS_BLEND_SRC_ALPHA,
                                      GRAPHICS_BLEND_CONSTANT));
        draw_square(0, 1,1,1,1);
        CHECK_FALSE(graphics_set_blend(GRAPHICS_BLEND_RSUB,
                                       GRAPHICS_BLEND_SRC_COLOR, 0));
        CHECK_FALSE(graphics_set_blend(GRAPHICS_BLEND_SUB,
                                       0, GRAPHICS_BLEND_DEST_COLOR));
        draw_square(0, 1,1,1,0.2);
        CHECK_SQUARE(0.4, 0.6, 0.8);
    }

    if (!blend_dest_alpha_supported()) {
        CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD,
                                      GRAPHICS_BLEND_SRC_ALPHA,
                                      GRAPHICS_BLEND_CONSTANT));
        draw_square(0, 1,1,1,1);
        CHECK_FALSE(graphics_set_blend(GRAPHICS_BLEND_RSUB,
                                       GRAPHICS_BLEND_DEST_ALPHA, 0));
        CHECK_FALSE(graphics_set_blend(GRAPHICS_BLEND_RSUB,
                                       GRAPHICS_BLEND_INV_DEST_ALPHA, 0));
        CHECK_FALSE(graphics_set_blend(GRAPHICS_BLEND_SUB,
                                       0, GRAPHICS_BLEND_DEST_ALPHA));
        CHECK_FALSE(graphics_set_blend(GRAPHICS_BLEND_SUB,
                                       0, GRAPHICS_BLEND_INV_DEST_ALPHA));
        draw_square(0, 1,1,1,0.2);
        CHECK_SQUARE(0.4, 0.6, 0.8);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_blend_invalid)
{
    used_blend = 1;

    graphics_set_viewport(0, 0, 64, 64);
    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD,
                                  GRAPHICS_BLEND_SRC_ALPHA,
                                  GRAPHICS_BLEND_CONSTANT));
    graphics_set_blend_color(&(Vector4f){0.2, 0.4, 0.6, 1});

    /* None of these should change the blend state. */
    CHECK_FALSE(graphics_set_blend(GRAPHICS_BLEND_ADD, -1, 0));
    CHECK_FALSE(graphics_set_blend(GRAPHICS_BLEND_ADD, INT_MAX, 0));
    CHECK_FALSE(graphics_set_blend(GRAPHICS_BLEND_ADD, 1, -1));
    CHECK_FALSE(graphics_set_blend(GRAPHICS_BLEND_ADD, 1, INT_MAX));
    CHECK_FALSE(graphics_set_blend(0, 1, 0));
    CHECK_FALSE(graphics_set_blend(INT_MAX, 1, 0));
    graphics_set_blend_color(NULL);

    draw_square(0, 1,1,1,1);
    draw_square(0, 1,1,1,0.2);
    CHECK_SQUARE(0.4, 0.6, 0.8);

    /* The blend color should be clamped to [0,1]. */
    graphics_set_blend_color(&(Vector4f){-1, -1, -1, -1});
    draw_square(0, 1,1,1,1);
    draw_square(0, 1,1,1,0.2);
    CHECK_SQUARE(0.2, 0.2, 0.2);
    graphics_set_blend_color(&(Vector4f){2, 2, 2, 2});
    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD,
                                  GRAPHICS_BLEND_ONE, GRAPHICS_BLEND_ZERO));
    draw_square(0, 0.2,0.4,0.6,1);
    CHECK_TRUE(graphics_set_blend(GRAPHICS_BLEND_ADD,
                                  GRAPHICS_BLEND_SRC_ALPHA,
                                  GRAPHICS_BLEND_CONSTANT));
    draw_square(0, 1,1,1,0.2);
    CHECK_SQUARE(0.4, 0.6, 0.8);

    return 1;
}

/*************************************************************************/
/********************** Transformation matrix tests **********************/
/*************************************************************************/

TEST(test_projection_matrix)
{
    used_projection_matrix = 1;

    graphics_set_viewport(0, 0, 64, 64);
    graphics_enable_depth_test(1);

    /* The default state of all matrices has been checked by just about
     * every graphics test so far, so we only check that changes to the
     * matrix are handled properly. */

    /* X scaling and translation. */
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_SET_MATRIX(GRAPHICS_MATRIX_PROJECTION,
                     &(Matrix4f){0.5,0,0,0, 0,1,0,0, 0,0,1,0, 0.5,0,0,1});
    draw_square(0, 1,1,1,1);
    CHECK_RECTANGLE(16,32, 48,32);

    /* Y scaling and translation. */
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_SET_MATRIX(GRAPHICS_MATRIX_PROJECTION,
                     &(Matrix4f){1,0,0,0, 0,0.5,0,0, 0,0,1,0, 0,0.5,0,1});
    draw_square(0, 1,1,1,1);
    CHECK_RECTANGLE(32,16, 32,48);

    /* Z scaling and translation.  For this case, we set things up so a
     * square with a nearer Z coordinate than a second square (in object
     * space) ends up with a lower (farther) depth value, and check that
     * it's properly obscured by the second square. */
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_SET_MATRIX(GRAPHICS_MATRIX_PROJECTION,
                     &(Matrix4f){1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,-0.5,1});
    draw_square(0, 1,1,1,1);  // Window Z coordinate is -0.5.
    CHECK_SET_MATRIX(GRAPHICS_MATRIX_PROJECTION,
                     &(Matrix4f){1,0,0,0, 0,1,0,0, 0,0,0.25,0, 0,0,0,1});
    draw_square(-1, 1,1,1,1);  // Window Z coordinate is -0.25.
    CHECK_SQUARE(1,1,1);

    /* W normalization with a constant value. */
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_SET_MATRIX(GRAPHICS_MATRIX_PROJECTION,
                     &(Matrix4f){1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,2});
    draw_square(0, 1,1,1,1);
    CHECK_RECTANGLE(16,16, 32,32);

    /* W normalization based on Z coordinate. */
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_SET_MATRIX(GRAPHICS_MATRIX_PROJECTION,
                     &(Matrix4f){1,0,0,0, 0,1,0,0, 0,0,1,2, 0,0,0,1});
    draw_square(0, 1,1,1,1);
    CHECK_RECTANGLE(32,32, 32,32);
    graphics_clear(0,0,0,0, 1, 0);
    draw_square(0.5, 1,1,1,1);
    CHECK_RECTANGLE(16,16, 32,32);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_make_parallel_projection)
{
    Matrix4f M;
    mem_clear(&M, sizeof(M));

    graphics_make_parallel_projection(0, 1024, 768, 256, -128, 128, &M);
    CHECK_FLOATEQUAL(M._11, 2.0/1024.0);
    CHECK_FLOATEQUAL(M._12, 0.0);
    CHECK_FLOATEQUAL(M._13, 0.0);
    CHECK_FLOATEQUAL(M._14, 0.0);
    CHECK_FLOATEQUAL(M._21, 0.0);
    CHECK_FLOATEQUAL(M._22, -2.0/512.0);
    CHECK_FLOATEQUAL(M._23, 0.0);
    CHECK_FLOATEQUAL(M._24, 0.0);
    CHECK_FLOATEQUAL(M._31, 0.0);
    CHECK_FLOATEQUAL(M._32, 0.0);
    CHECK_FLOATEQUAL(M._33, 2.0/256.0);
    CHECK_FLOATEQUAL(M._34, 0.0);
    CHECK_FLOATEQUAL(M._41, -1.0);
    CHECK_FLOATEQUAL(M._42, 2.0);
    CHECK_FLOATEQUAL(M._43, 0.0);
    CHECK_FLOATEQUAL(M._44, 1.0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_parallel_projection)
{
    Matrix4f M;
    mem_clear(&M, sizeof(M));

    graphics_set_parallel_projection(0, 1024, 768, 256, -128, 128);
    graphics_get_projection_matrix(&M);
    CHECK_FLOATEQUAL(M._11, 2.0/1024.0);
    CHECK_FLOATEQUAL(M._12, 0.0);
    CHECK_FLOATEQUAL(M._13, 0.0);
    CHECK_FLOATEQUAL(M._14, 0.0);
    CHECK_FLOATEQUAL(M._21, 0.0);
    CHECK_FLOATEQUAL(M._22, -2.0/512.0);
    CHECK_FLOATEQUAL(M._23, 0.0);
    CHECK_FLOATEQUAL(M._24, 0.0);
    CHECK_FLOATEQUAL(M._31, 0.0);
    CHECK_FLOATEQUAL(M._32, 0.0);
    CHECK_FLOATEQUAL(M._33, 2.0/256.0);
    CHECK_FLOATEQUAL(M._34, 0.0);
    CHECK_FLOATEQUAL(M._41, -1.0);
    CHECK_FLOATEQUAL(M._42, 2.0);
    CHECK_FLOATEQUAL(M._43, 0.0);
    CHECK_FLOATEQUAL(M._44, 1.0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_make_perspective_projection)
{
    Matrix4f M;
    mem_clear(&M, sizeof(M));

    graphics_make_perspective_projection(90, 2, 1, 1025, 0, &M);
    CHECK_FLOATEQUAL(M._11, 0.5);
    CHECK_FLOATEQUAL(M._12, 0.0);
    CHECK_FLOATEQUAL(M._13, 0.0);
    CHECK_FLOATEQUAL(M._14, 0.0);
    CHECK_FLOATEQUAL(M._21, 0.0);
    CHECK_FLOATEQUAL(M._22, 1.0);
    CHECK_FLOATEQUAL(M._23, 0.0);
    CHECK_FLOATEQUAL(M._24, 0.0);
    CHECK_FLOATEQUAL(M._31, 0.0);
    CHECK_FLOATEQUAL(M._32, 0.0);
    CHECK_FLOATEQUAL(M._33, 1026.0/1024.0);
    CHECK_FLOATEQUAL(M._34, 1.0);
    CHECK_FLOATEQUAL(M._41, 0.0);
    CHECK_FLOATEQUAL(M._42, 0.0);
    CHECK_FLOATEQUAL(M._43, -2050.0/1024.0);
    CHECK_FLOATEQUAL(M._44, 0.0);

    graphics_make_perspective_projection(90, 2, 1, 1025, 1, &M);
    CHECK_FLOATEQUAL(M._11, 0.5);
    CHECK_FLOATEQUAL(M._12, 0.0);
    CHECK_FLOATEQUAL(M._13, 0.0);
    CHECK_FLOATEQUAL(M._14, 0.0);
    CHECK_FLOATEQUAL(M._21, 0.0);
    CHECK_FLOATEQUAL(M._22, 1.0);
    CHECK_FLOATEQUAL(M._23, 0.0);
    CHECK_FLOATEQUAL(M._24, 0.0);
    CHECK_FLOATEQUAL(M._31, 0.0);
    CHECK_FLOATEQUAL(M._32, 0.0);
    CHECK_FLOATEQUAL(M._33, -1026.0/1024.0);
    CHECK_FLOATEQUAL(M._34, -1.0);
    CHECK_FLOATEQUAL(M._41, 0.0);
    CHECK_FLOATEQUAL(M._42, 0.0);
    CHECK_FLOATEQUAL(M._43, -2050.0/1024.0);
    CHECK_FLOATEQUAL(M._44, 0.0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_perspective_projection)
{
    Matrix4f M;
    mem_clear(&M, sizeof(M));

    graphics_set_perspective_projection(90, 2, 1, 1025, 0);
    graphics_get_projection_matrix(&M);
    CHECK_FLOATEQUAL(M._11, 0.5);
    CHECK_FLOATEQUAL(M._12, 0.0);
    CHECK_FLOATEQUAL(M._13, 0.0);
    CHECK_FLOATEQUAL(M._14, 0.0);
    CHECK_FLOATEQUAL(M._21, 0.0);
    CHECK_FLOATEQUAL(M._22, 1.0);
    CHECK_FLOATEQUAL(M._23, 0.0);
    CHECK_FLOATEQUAL(M._24, 0.0);
    CHECK_FLOATEQUAL(M._31, 0.0);
    CHECK_FLOATEQUAL(M._32, 0.0);
    CHECK_FLOATEQUAL(M._33, 1026.0/1024.0);
    CHECK_FLOATEQUAL(M._34, 1.0);
    CHECK_FLOATEQUAL(M._41, 0.0);
    CHECK_FLOATEQUAL(M._42, 0.0);
    CHECK_FLOATEQUAL(M._43, -2050.0/1024.0);
    CHECK_FLOATEQUAL(M._44, 0.0);

    graphics_set_perspective_projection(90, 2, 1, 1025, 1);
    graphics_get_projection_matrix(&M);
    CHECK_FLOATEQUAL(M._11, 0.5);
    CHECK_FLOATEQUAL(M._12, 0.0);
    CHECK_FLOATEQUAL(M._13, 0.0);
    CHECK_FLOATEQUAL(M._14, 0.0);
    CHECK_FLOATEQUAL(M._21, 0.0);
    CHECK_FLOATEQUAL(M._22, 1.0);
    CHECK_FLOATEQUAL(M._23, 0.0);
    CHECK_FLOATEQUAL(M._24, 0.0);
    CHECK_FLOATEQUAL(M._31, 0.0);
    CHECK_FLOATEQUAL(M._32, 0.0);
    CHECK_FLOATEQUAL(M._33, -1026.0/1024.0);
    CHECK_FLOATEQUAL(M._34, -1.0);
    CHECK_FLOATEQUAL(M._41, 0.0);
    CHECK_FLOATEQUAL(M._42, 0.0);
    CHECK_FLOATEQUAL(M._43, -2050.0/1024.0);
    CHECK_FLOATEQUAL(M._44, 0.0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_view_matrix)
{
    used_view_matrix = 1;

    graphics_set_viewport(0, 0, 64, 64);
    graphics_enable_depth_test(1);

    /* Identical to the projection matrix tests except for the matrix used. */

    /* X scaling and translation. */
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_SET_MATRIX(GRAPHICS_MATRIX_VIEW,
                     &(Matrix4f){0.5,0,0,0, 0,1,0,0, 0,0,1,0, 0.5,0,0,1});
    draw_square(0, 1,1,1,1);
    CHECK_RECTANGLE(16,32, 48,32);

    /* Y scaling and translation. */
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_SET_MATRIX(GRAPHICS_MATRIX_VIEW,
                     &(Matrix4f){1,0,0,0, 0,0.5,0,0, 0,0,1,0, 0,0.5,0,1});
    draw_square(0, 1,1,1,1);
    CHECK_RECTANGLE(32,16, 32,48);

    /* Z scaling and translation. */
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_SET_MATRIX(GRAPHICS_MATRIX_VIEW,
                     &(Matrix4f){1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,-0.5,1});
    draw_square(0, 1,1,1,1);  // Window Z coordinate is -0.5.
    CHECK_SET_MATRIX(GRAPHICS_MATRIX_VIEW,
                     &(Matrix4f){1,0,0,0, 0,1,0,0, 0,0,0.25,0, 0,0,0,1});
    draw_square(-1, 1,1,1,1);  // Window Z coordinate is -0.25.
    CHECK_SQUARE(1,1,1);

#ifndef SIL_PLATFORM_PSP  // Not supported on the PSP.
    /* W normalization with a constant value. */
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_SET_MATRIX(GRAPHICS_MATRIX_VIEW,
                     &(Matrix4f){1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,2});
    draw_square(0, 1,1,1,1);
    CHECK_RECTANGLE(16,16, 32,32);

    /* W normalization based on Z coordinate. */
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_SET_MATRIX(GRAPHICS_MATRIX_VIEW,
                     &(Matrix4f){1,0,0,0, 0,1,0,0, 0,0,1,2, 0,0,0,1});
    draw_square(0, 1,1,1,1);
    CHECK_RECTANGLE(32,32, 32,32);
    graphics_clear(0,0,0,0, 1, 0);
    draw_square(0.5, 1,1,1,1);
    CHECK_RECTANGLE(16,16, 32,32);
#endif

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_model_matrix)
{
    used_model_matrix = 1;

    graphics_set_viewport(0, 0, 64, 64);
    graphics_enable_depth_test(1);

    /* Identical to the projection matrix tests except for the matrix used. */

    /* X scaling and translation. */
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_SET_MATRIX(GRAPHICS_MATRIX_MODEL,
                     &(Matrix4f){0.5,0,0,0, 0,1,0,0, 0,0,1,0, 0.5,0,0,1});
    draw_square(0, 1,1,1,1);
    CHECK_RECTANGLE(16,32, 48,32);

    /* Y scaling and translation. */
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_SET_MATRIX(GRAPHICS_MATRIX_MODEL,
                     &(Matrix4f){1,0,0,0, 0,0.5,0,0, 0,0,1,0, 0,0.5,0,1});
    draw_square(0, 1,1,1,1);
    CHECK_RECTANGLE(32,16, 32,48);

    /* Z scaling and translation. */
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_SET_MATRIX(GRAPHICS_MATRIX_MODEL,
                     &(Matrix4f){1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,-0.5,1});
    draw_square(0, 1,1,1,1);  // Window Z coordinate is -0.5.
    CHECK_SET_MATRIX(GRAPHICS_MATRIX_MODEL,
                     &(Matrix4f){1,0,0,0, 0,1,0,0, 0,0,0.25,0, 0,0,0,1});
    draw_square(-1, 1,1,1,1);  // Window Z coordinate is -0.25.
    CHECK_SQUARE(1,1,1);

#ifndef SIL_PLATFORM_PSP  // Not supported on the PSP.
    /* W normalization with a constant value. */
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_SET_MATRIX(GRAPHICS_MATRIX_MODEL,
                     &(Matrix4f){1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,2});
    draw_square(0, 1,1,1,1);
    CHECK_RECTANGLE(16,16, 32,32);

    /* W normalization based on Z coordinate. */
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_SET_MATRIX(GRAPHICS_MATRIX_MODEL,
                     &(Matrix4f){1,0,0,0, 0,1,0,0, 0,0,1,2, 0,0,0,1});
    draw_square(0, 1,1,1,1);
    CHECK_RECTANGLE(32,32, 32,32);
    graphics_clear(0,0,0,0, 1, 0);
    draw_square(0.5, 1,1,1,1);
    CHECK_RECTANGLE(16,16, 32,32);
#endif

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_matrix_combined_and_invalid)
{
    graphics_set_viewport(0, 0, 64, 64);

    static const uint8_t texture_data[16] =
        {0x60,0x90,0xC0,0xAA, 0,0,0,0, 0,0,0,0, 0,0,0,0};
    int texture;
    ASSERT(texture = texture_create_with_data(2, 2, texture_data,
                                              TEX_FORMAT_RGBA8888, 2, 0, 0));
    texture_set_antialias(texture, 0);
    texture_apply(0, texture);

    CHECK_SET_MATRIX(GRAPHICS_MATRIX_PROJECTION,
                     &(Matrix4f){0.5,0,0,0, 0,1,0,0, 0,0,1,0, 0.5,0,0,1});
    CHECK_SET_MATRIX(GRAPHICS_MATRIX_VIEW,
                     &(Matrix4f){1,0,0,0, 0,0.25,0,0, 0,0,1,0, 0,-0.5,0,1});
    CHECK_SET_MATRIX(GRAPHICS_MATRIX_MODEL,
                     &(Matrix4f){1,0,0,0, 0,1,0,0, 0,0,1,0, 0,1,0,1});

    /* None of these should change the effective matrices. */
    graphics_set_projection_matrix(NULL);
    graphics_set_view_matrix(NULL);
    graphics_set_model_matrix(NULL);

    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0},
                                   &(Vector2f){0, 0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0},
                                   &(Vector2f){0, 0.5}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0},
                                   &(Vector2f){0.5, 0.5}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0},
                                   &(Vector2f){0.5, 0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_COLORED_RECTANGLE(16,8, 48,24, 0x40/255.0, 0x60/255.0, 0x80/255.0);

    /* Check that these calls don't crash. */
    graphics_get_projection_matrix(NULL);
    graphics_get_view_matrix(NULL);
    graphics_get_model_matrix(NULL);

    texture_destroy(texture);
    return 1;
}

/*************************************************************************/
/************************** Alpha-testing tests **************************/
/*************************************************************************/

TEST(test_alpha_test)
{
    used_alpha_test = 1;

    uint8_t *pixels;

    int texture;
    static const ALIGNED(4) uint8_t alpha_tex_data[] = {
        'T','E','X', 10,  2, 64,  0,  0,  0,  2,  0,  2,  0,  1,  0,  0,
          0,  0,  0, 32,  0,  0,  0,  4,  0,  0,  0,  0,  0,  0,  0,  0,
          1, 51,254,255
    };
    /* Safe to de-const the input buffer since we're not reusing it. */
    ASSERT(texture = texture_parse((void *)alpha_tex_data,
                                   sizeof(alpha_tex_data), 0, 0, 0));
    texture_set_antialias(texture, 0);
    texture_apply(0, texture);

    graphics_set_viewport(0, 0, 64, 64);

    /* By default, the alpha test should be disabled. */
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0},
                                   &(Vector2f){0, 0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0},
                                   &(Vector2f){0, 1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0},
                                   &(Vector2f){1, 1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0},
                                   &(Vector2f){1, 0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int x4 = x/16, y4 = y/16;
        const int p = (x4==1 && y4==1 ?   1 :
                       x4==2 && y4==1 ?  51 :
                       x4==1 && y4==2 ? 254 :
                       x4==2 && y4==2 ? 255 : 0);
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* The default reference ("discard less than") value should be zero,
     * so enabling alpha testing without changing the reference value
     * should still pass everything. */
    graphics_enable_alpha_test(1);
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0},
                                   &(Vector2f){0, 0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0},
                                   &(Vector2f){0, 1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0},
                                   &(Vector2f){1, 1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0},
                                   &(Vector2f){1, 0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int x4 = x/16, y4 = y/16;
        const int p = (x4==1 && y4==1 ?   1 :
                       x4==2 && y4==1 ?  51 :
                       x4==1 && y4==2 ? 254 :
                       x4==2 && y4==2 ? 255 : 0);
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* Check that alpha testing discards pixels with alpha less than
     * (but not equal to) the reference value. */
    graphics_set_alpha_test_reference(0.2);
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0},
                                   &(Vector2f){0, 0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0},
                                   &(Vector2f){0, 1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0},
                                   &(Vector2f){1, 1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0},
                                   &(Vector2f){1, 0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int x4 = x/16, y4 = y/16;
        const int p = (x4==2 && y4==1 ?  51 :
                       x4==1 && y4==2 ? 254 :
                       x4==2 && y4==2 ? 255 : 0);
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* Check with a reference value < 0.0 (should be clamped to 0.0). */
    graphics_set_alpha_test_reference(-1.0);
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0},
                                   &(Vector2f){0, 0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0},
                                   &(Vector2f){0, 1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0},
                                   &(Vector2f){1, 1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0},
                                   &(Vector2f){1, 0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int x4 = x/16, y4 = y/16;
        const int p = (x4==1 && y4==1 ?   1 :
                       x4==2 && y4==1 ?  51 :
                       x4==1 && y4==2 ? 254 :
                       x4==2 && y4==2 ? 255 : 0);
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* Check with a reference value > 1.0 (should be clamped to 1.0). */
    graphics_set_alpha_test_reference(2.0);
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0},
                                   &(Vector2f){0, 0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0},
                                   &(Vector2f){0, 1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0},
                                   &(Vector2f){1, 1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0},
                                   &(Vector2f){1, 0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int x4 = x/16, y4 = y/16;
        const int p = (x4==2 && y4==2 ? 255 : 0);
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* Check that alpha testing can be disabled. */
    graphics_enable_alpha_test(0);
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0},
                                   &(Vector2f){0, 0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0},
                                   &(Vector2f){0, 1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0},
                                   &(Vector2f){1, 1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0},
                                   &(Vector2f){1, 0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int x4 = x/16, y4 = y/16;
        const int p = (x4==1 && y4==1 ?   1 :
                       x4==2 && y4==1 ?  51 :
                       x4==1 && y4==2 ? 254 :
                       x4==2 && y4==2 ? 255 : 0);
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* Check that re-enabling alpha testing preserves the current reference
     * value. */
    graphics_enable_alpha_test(1);
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0},
                                   &(Vector2f){0, 0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0},
                                   &(Vector2f){0, 1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0},
                                   &(Vector2f){1, 1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0},
                                   &(Vector2f){1, 0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int x4 = x/16, y4 = y/16;
        const int p = (x4==2 && y4==2 ? 255 : 0);
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_alpha_test_comparison)
{
    used_alpha_test_comparison = 1;

    uint8_t *pixels;

    int texture;
    static const ALIGNED(4) uint8_t alpha_tex_data[] = {
        'T','E','X', 10,  2, 64,  0,  0,  0,  2,  0,  2,  0,  1,  0,  0,
          0,  0,  0, 32,  0,  0,  0,  4,  0,  0,  0,  0,  0,  0,  0,  0,
          1, 51,254,255
    };
    /* Safe to de-const the input buffer since we're not reusing it. */
    ASSERT(texture = texture_parse((void *)alpha_tex_data,
                                   sizeof(alpha_tex_data), 0, 0, 0));
    texture_set_antialias(texture, 0);
    texture_apply(0, texture);

    graphics_set_viewport(0, 0, 64, 64);

    /* By default, the alpha test should pass pixels greater than or
     * equal to the reference value.  We tested that above, so now check
     * that different comparison types work. */
    graphics_enable_alpha_test(1);
    graphics_set_alpha_test_reference(0.2);
    graphics_set_alpha_test_comparison(GRAPHICS_COMPARISON_LESS);
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0},
                                   &(Vector2f){0, 0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0},
                                   &(Vector2f){0, 1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0},
                                   &(Vector2f){1, 1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0},
                                   &(Vector2f){1, 0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int x4 = x/16, y4 = y/16;
        const int p = (x4==1 && y4==1 ?   1 : 0);
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    graphics_set_alpha_test_comparison(GRAPHICS_COMPARISON_LESS_EQUAL);
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0},
                                   &(Vector2f){0, 0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0},
                                   &(Vector2f){0, 1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0},
                                   &(Vector2f){1, 1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0},
                                   &(Vector2f){1, 0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int x4 = x/16, y4 = y/16;
        const int p = (x4==1 && y4==1 ?   1 :
                       x4==2 && y4==1 ?  51 : 0);
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    graphics_set_alpha_test_comparison(GRAPHICS_COMPARISON_GREATER);
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0},
                                   &(Vector2f){0, 0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0},
                                   &(Vector2f){0, 1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0},
                                   &(Vector2f){1, 1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0},
                                   &(Vector2f){1, 0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int x4 = x/16, y4 = y/16;
        const int p = (x4==1 && y4==2 ? 254 :
                       x4==2 && y4==2 ? 255 : 0);
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_alpha_test_new_frame)
{
    uint8_t *pixels;

    int texture;
    static const ALIGNED(4) uint8_t alpha_tex_data[] = {
        'T','E','X', 10,  2, 64,  0,  0,  0,  2,  0,  2,  0,  1,  0,  0,
          0,  0,  0, 32,  0,  0,  0,  4,  0,  0,  0,  0,  0,  0,  0,  0,
          1, 51,254,255
    };
    /* Safe to de-const the input buffer since we're not reusing it. */
    ASSERT(texture = texture_parse((void *)alpha_tex_data,
                                   sizeof(alpha_tex_data), 0, 0, 0));
    texture_set_antialias(texture, 0);
    texture_apply(0, texture);

    graphics_set_viewport(0, 0, 64, 64);

    /* Check that alpha test state is retained across a frame change. */
    graphics_enable_alpha_test(1);
    graphics_set_alpha_test_reference(0.2);
    graphics_set_alpha_test_comparison(GRAPHICS_COMPARISON_LESS);
    graphics_set_alpha_test_comparison(GRAPHICS_COMPARISON_LESS_EQUAL);
    graphics_finish_frame();
    graphics_start_frame();
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0},
                                   &(Vector2f){0, 0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0},
                                   &(Vector2f){0, 1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0},
                                   &(Vector2f){1, 1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0},
                                   &(Vector2f){1, 0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int x4 = x/16, y4 = y/16;
        const int p = (x4==1 && y4==1 ?   1 :
                       x4==2 && y4==1 ?  51 : 0);
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_alpha_test_comparison_invalid)
{
    uint8_t *pixels;

    int texture;
    static const ALIGNED(4) uint8_t alpha_tex_data[] = {
        'T','E','X', 10,  2, 64,  0,  0,  0,  2,  0,  2,  0,  1,  0,  0,
          0,  0,  0, 32,  0,  0,  0,  4,  0,  0,  0,  0,  0,  0,  0,  0,
          1, 51,254,255
    };
    /* Safe to de-const the input buffer since we're not reusing it. */
    ASSERT(texture = texture_parse((void *)alpha_tex_data,
                                   sizeof(alpha_tex_data), 0, 0, 0));
    texture_set_antialias(texture, 0);
    texture_apply(0, texture);

    graphics_set_viewport(0, 0, 64, 64);

    graphics_enable_alpha_test(1);
    graphics_set_alpha_test_reference(0.2);
    graphics_set_alpha_test_comparison(GRAPHICS_COMPARISON_LESS);
    /* This should not change the current comparison function. */
    /* These should not change the current comparison function. */
    graphics_set_alpha_test_comparison(-1);
    graphics_set_alpha_test_comparison(GRAPHICS_COMPARISON_FALSE);
    graphics_set_alpha_test_comparison(GRAPHICS_COMPARISON_TRUE);
    graphics_set_alpha_test_comparison(GRAPHICS_COMPARISON_EQUAL);
    graphics_set_alpha_test_comparison(GRAPHICS_COMPARISON_NOT_EQUAL);
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0},
                                   &(Vector2f){0, 0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0},
                                   &(Vector2f){0, 1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0},
                                   &(Vector2f){1, 1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0},
                                   &(Vector2f){1, 0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int x4 = x/16, y4 = y/16;
        const int p = (x4==1 && y4==1 ?   1 : 0);
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    texture_destroy(texture);
    return 1;
}

/*************************************************************************/
/************************** Face culling tests ***************************/
/*************************************************************************/

TEST(test_face_cull)
{
    used_face_cull = 1;

    graphics_set_viewport(0, 0, 64, 64);

    /* Face culling should be disabled by default. */
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_SQUARE(1,1,1);
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_SQUARE(1,1,1);

    /* Check that clockwise culling works. */
    graphics_set_face_cull(GRAPHICS_FACE_CULL_CW);
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_SQUARE(0,0,0);  // Not drawn.
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_SQUARE(1,1,1);

    /* Check that counterclockwise culling works. */
    graphics_set_face_cull(GRAPHICS_FACE_CULL_CCW);
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_SQUARE(1,1,1);
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_SQUARE(0,0,0);  // Not drawn.

    /* Check that culling can be disabled again. */
    graphics_set_face_cull(GRAPHICS_FACE_CULL_NONE);
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_SQUARE(1,1,1);
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_SQUARE(1,1,1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_face_cull_new_frame)
{
    graphics_set_viewport(0, 0, 64, 64);

    /* Check that face cull state is retained across a frame change. */
    graphics_set_face_cull(GRAPHICS_FACE_CULL_CW);
    graphics_finish_frame();
    graphics_start_frame();
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_SQUARE(0,0,0);  // Not drawn.
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_SQUARE(1,1,1);

    return 1;
}

/*************************************************************************/
/********************** Fixed primitive color tests **********************/
/*************************************************************************/

TEST(test_fixed_color)
{
    used_fixed_color = 1;

    graphics_set_viewport(0, 0, 64, 64);

    /* We've already checked that the default is (1,1,1,1), since earlier
     * tests depended on seeing quads with the proper colors.  Check that
     * changing the color results in rendering differences.  Note that
     * draw_square() makes use of vertex colors, so we roll our own quad
     * to verify behavior with position-only vertices. */

    graphics_set_fixed_color(&(Vector4f){0.2, 0.4, 0.6, 2.0/3.0});
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_SQUARE(0.2/1.5, 0.4/1.5, 0.6/1.5);

    /* Check that changing any single component of the color results in
     * rendering differences (in case of optimization bugs). */

    graphics_set_fixed_color(&(Vector4f){0.2, 0.4, 0.6, 1.0});
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_SQUARE(0.2, 0.4, 0.6);

    graphics_set_fixed_color(&(Vector4f){0.4, 0.4, 0.6, 1.0});
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_SQUARE(0.4, 0.4, 0.6);

    graphics_set_fixed_color(&(Vector4f){0.4, 0.6, 0.6, 1.0});
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_SQUARE(0.4, 0.6, 0.6);

    graphics_set_fixed_color(&(Vector4f){0.4, 0.6, 1.0, 1.0});
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_SQUARE(0.4, 0.6, 1.0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fixed_color_vertex_color)
{
    graphics_set_viewport(0, 0, 64, 64);

    graphics_set_fixed_color(&(Vector4f){0.2, 0.4, 0.6, 2.0/3.0});
    draw_square(0, 0.6, 0.7, 0.8, 2.0/3.0);
    CHECK_SQUARE(0.48/9.0, 1.12/9.0, 1.92/9.0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fixed_color_texture)
{
    graphics_set_viewport(0, 0, 64, 64);

    int texture;
    ASSERT(texture = texture_create_with_data(1, 1, "\x60\x90\xC0\xAA",
                                              TEX_FORMAT_RGBA8888, 2, 0, 0));
    texture_apply(0, texture);

    graphics_set_fixed_color(&(Vector4f){0.75, 1.0, 0.25, 0.5});
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0},
                                   &(Vector2f){0, 0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0},
                                   &(Vector2f){0, 1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0},
                                   &(Vector2f){1, 1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0},
                                   &(Vector2f){1, 0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_SQUARE(0x18/255.0, 0x30/255.0, 0x10/255.0);

    /* Also check the combination of texture and per-vertex color data. */
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0},
                                   &(Vector2f){0, 0},
                                   &(Vector4f){0.5, 1.0/3.0, 2.0/3.0, 0.75}));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0},
                                   &(Vector2f){0, 1},
                                   &(Vector4f){0.5, 1.0/3.0, 2.0/3.0, 0.75}));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0},
                                   &(Vector2f){1, 1},
                                   &(Vector4f){0.5, 1.0/3.0, 2.0/3.0, 0.75}));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0},
                                   &(Vector2f){1, 0},
                                   &(Vector4f){0.5, 1.0/3.0, 2.0/3.0, 0.75}));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_SQUARE(0x09/255.0, 0x0C/255.0, 0x08/255.0);

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fixed_color_alpha_texture)
{
    int texture;
    static const ALIGNED(4) uint8_t alpha_tex_data[] = {
        'T','E','X', 10,  2, 64,  0,  0,  0,  1,  0,  1,  0,  1,  0,  0,
          0,  0,  0, 32,  0,  0,  0,  1,  0,  0,  0,  0,  0,  0,  0,  0,
        170,
    };
    /* Safe to de-const the input buffer since we're not reusing it. */
    ASSERT(texture = texture_parse((void *)alpha_tex_data,
                                   sizeof(alpha_tex_data), 0, 0, 0));
    texture_apply(0, texture);

    graphics_set_viewport(0, 0, 64, 64);

    graphics_set_fixed_color(&(Vector4f){0.8, 0.4, 0.6, 0.5});
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0},
                                   &(Vector2f){0, 0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0},
                                   &(Vector2f){0, 1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0},
                                   &(Vector2f){1, 1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0},
                                   &(Vector2f){1, 0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_SQUARE(0.8/3.0, 0.4/3.0, 0.6/3.0);

    graphics_clear(0,0,0,0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0},
                                   &(Vector2f){0, 0},
                                   &(Vector4f){0.5, 1.0, 2.0/3.0, 0.5}));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0},
                                   &(Vector2f){0, 1},
                                   &(Vector4f){0.5, 1.0, 2.0/3.0, 0.5}));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0},
                                   &(Vector2f){1, 1},
                                   &(Vector4f){0.5, 1.0, 2.0/3.0, 0.5}));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0},
                                   &(Vector2f){1, 0},
                                   &(Vector4f){0.5, 1.0, 2.0/3.0, 0.5}));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_SQUARE(0.2/3.0, 0.2/3.0, 0.2/3.0);

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fixed_color_caching)
{
    graphics_set_viewport(0, 0, 64, 64);

    graphics_set_fixed_color(&(Vector4f){1, 1, 1, 0.2});
    draw_square(0, 1, 1, 1, 1);
    CHECK_SQUARE(0.2, 0.2, 0.2);

    /* Change one color component at a time to ensure that previous colors
     * are not incorrectly cached. */

    graphics_set_fixed_color(&(Vector4f){1, 1, 1, 1});
    draw_square(0, 1, 1, 1, 1);
    CHECK_SQUARE(1, 1, 1);

    graphics_set_fixed_color(&(Vector4f){1, 1, 0.8, 1});
    draw_square(0, 1, 1, 1, 1);
    CHECK_SQUARE(1, 1, 0.8);

    graphics_set_fixed_color(&(Vector4f){1, 0.6, 0.8, 1});
    draw_square(0, 1, 1, 1, 1);
    CHECK_SQUARE(1, 0.6, 0.8);

    graphics_set_fixed_color(&(Vector4f){0.4, 0.6, 0.8, 1});
    draw_square(0, 1, 1, 1, 1);
    CHECK_SQUARE(0.4, 0.6, 0.8);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fixed_color_new_frame)
{
    graphics_set_viewport(0, 0, 64, 64);

    /* Check that the fixed color is retained across a frame change. */
    graphics_set_fixed_color(&(Vector4f){0.2, 0.4, 0.6, 2.0/3.0});
    graphics_finish_frame();
    graphics_start_frame();
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_SQUARE(0.2/1.5, 0.4/1.5, 0.6/1.5);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fixed_color_invalid)
{
    graphics_set_viewport(0, 0, 64, 64);

    graphics_set_fixed_color(&(Vector4f){0.6, 0.2, 0.4, 1.0/3.0});
    /* This should not change the current color. */
    graphics_set_fixed_color(NULL);

    draw_square(0, 1,1,1,1);
    CHECK_SQUARE(0.6/3.0, 0.2/3.0, 0.4/3.0);

    /* The color should be clamped to [0,1]. */
    graphics_set_fixed_color(&(Vector4f){-1, -1, -1, -1});
    draw_square(0, 1,1,1,1);
    CHECK_SQUARE(0.6/3.0, 0.2/3.0, 0.4/3.0);
    graphics_set_fixed_color(&(Vector4f){2, 2, 2, 2});
    draw_square(0, 0.6, 0.6, 0.6, 1);
    CHECK_SQUARE(0.6, 0.6, 0.6);

    return 1;
}

/*************************************************************************/
/******************************* Fog tests *******************************/
/*************************************************************************/

TEST(test_fog)
{
    used_fog = 1;

    uint8_t *pixels;

    /* This is intentionally 65, not 64, so that coordinates (0,0) lie
     * in the center of a pixel rather than on the boundary. */
    graphics_set_viewport(0, 0, 65, 65);
    graphics_set_fixed_color(&(Vector4f){0.2, 0.2, 0.2, 1});

    /* Fog should be disabled by default.  Note that we use a single point
     * at the origin for these tests, since whether X and Y coordinates are
     * taken into account for fog distance is implementation-dependent. */
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0, 0, 0.5}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x==32 && y==32 ? 51 : 0);
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* Check that fog can be enabled. */
    graphics_enable_fog(1);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0, 0, 0.5}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x==32 && y==32 ? 153 : 0);
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* Check that fog can be disabled again. */
    graphics_enable_fog(0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0, 0, 0.5}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x==32 && y==32 ? 51 : 0);
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fog_non_centered_pixel)
{
    used_fog = 1;

    uint8_t *pixels;

    graphics_set_viewport(0, 0, 65, 65);  // Intentionally 65.
    graphics_set_fixed_color(&(Vector4f){0.2, 0.2, 0.2, 1});

    graphics_enable_fog(1);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0.25}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x==16 && y==16 ? 102 : 0);
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fog_range)
{
    used_fog_range = 1;

    uint8_t *pixels;

    graphics_set_viewport(0, 0, 65, 65);  // Intentionally 65.
    graphics_set_fixed_color(&(Vector4f){0.2, 0.2, 0.2, 1});
    graphics_enable_fog(1);

    /* The default fog range is from 0.0 to 1.0.  We checked the midpoint
     * (0.5) above, so check a second point to ensure the range is correct. */
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0, 0, 0.25}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x==32 && y==32 ? 102 : 0);
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* Check that the range can be altered. */
    graphics_set_fog_start(0.25);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0, 0, 0.5}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x==32 && y==32 ? 119 : 0);
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);
    graphics_set_fog_end(0.75);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0, 0, 0.5}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x==32 && y==32 ? 153 : 0);
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* Check that distance values outside the fog range get clamped to
     * either no or full fog. */
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0, 0, 0.1}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x==32 && y==32 ? 51 : 0);
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0, 0, 0.9}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x==32 && y==32 ? 255 : 0);
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fog_across_eye_plane)
{
    used_fog = 1;

    uint8_t *pixels;

    graphics_set_viewport(0, 0, 64, 64);
    graphics_set_fixed_color(&(Vector4f){0.2, 0.2, 0.2, 1});
    graphics_enable_fog(1);
    graphics_set_fog_start(0.25);
    graphics_set_fog_end(0.5);

    /* Check that (particularly in shader-computed fog) distance is
     * interpolated correctly for primitives which cross the eye plane.
     * If the absolute value of the distance is taken at each vertex and
     * then interpolated, incorrect values will result.
     * NOTE: A number of real-world drivers seem to fail this test when
     * using the driver's fixed-function pipeline. */
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){2, 0, -0.5}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0, 0, 0.25}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0, 1, 0.25}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){2, 1, -0.5}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        if (x >= 32 && x < 64 && y >= 32 && y < 64) {
            CHECK_PIXEL(&pixels[i], 51,51,51,255, x, y);
        } else {
            CHECK_PIXEL(&pixels[i], 0,0,0,255, x, y);
        }
    }
    mem_free(pixels);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fog_with_transform)
{
    uint8_t *pixels;

    graphics_set_viewport(0, 0, 65, 65);  // Intentionally 65.
    graphics_set_fixed_color(&(Vector4f){0.2, 0.2, 0.2, 1});
    graphics_enable_fog(1);
    graphics_set_fog_start(1);
    graphics_set_fog_end(5);

    /* Projection matrix Z scaling/offset (should not affect fog distance). */
    CHECK_SET_MATRIX(GRAPHICS_MATRIX_PROJECTION,
                     &(Matrix4f){1,0,0,0, 0,1,0,0, 0,0,0.125,0, 0,0,-0.125,1});
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0, 0, 3}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x==32 && y==32 ? 153 : 0);
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* View matrix Z scaling/offset. */
    CHECK_SET_MATRIX(GRAPHICS_MATRIX_VIEW,
                     &(Matrix4f){1,0,0,0, 0,1,0,0, 0,0,2,0, 0,0,1,1});
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0, 0, 1}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x==32 && y==32 ? 153 : 0);
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* Model matrix Z scaling/offset. */
    CHECK_SET_MATRIX(GRAPHICS_MATRIX_MODEL,
                     &(Matrix4f){1,0,0,0, 0,1,0,0, 0,0,0.25,0, 0,0,-0.5,1});
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0, 0, 6}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x==32 && y==32 ? 153 : 0);
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* Z-inverted projection matrix. */
    CHECK_SET_MATRIX(GRAPHICS_MATRIX_PROJECTION,
                     &(Matrix4f){1,0,0,0, 0,1,0,0, 0,0,-0.125,0, 0,0,0.125,1});
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0, 0, -6}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x==32 && y==32 ? 153 : 0);
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* View matrix rotation. */
    CHECK_SET_MATRIX(GRAPHICS_MATRIX_PROJECTION, &mat4_identity);
    CHECK_SET_MATRIX(GRAPHICS_MATRIX_VIEW,
                     &(Matrix4f){0,0,1,0, 0,1,0,0, 1,0,0,0, 0,0,0,1});
    CHECK_SET_MATRIX(GRAPHICS_MATRIX_MODEL, &mat4_identity);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){3, 0, 0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x==32 && y==32 ? 153 : 0);
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fog_color)
{
    used_fog_color = 1;

    uint8_t *pixels;

    graphics_set_viewport(0, 0, 65, 65);  // Intentionally 65.
    graphics_enable_fog(1);

    /* The default fog color has already been checked by previous tests.
     * Check that we can change it. */
    graphics_set_fog_color(&(Vector4f){0.2, 0.4, 0.6, 1});
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0, 0, 2.0/3.0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int r = (x==32 && y==32 ? 119 : 0);
        const int g = (x==32 && y==32 ? 153 : 0);
        const int b = (x==32 && y==32 ? 187 : 0);
        CHECK_PIXEL_NEAR(&pixels[i], r,g,b,255, 1, x, y);
    }
    mem_free(pixels);

    /* Check that changing alpha in the fog color has no effect. */
    graphics_set_fog_color(&(Vector4f){0.2, 0.4, 0.6, 0.8});
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0, 0, 1.0/3.0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int r = (x==32 && y==32 ? 187 : 0);
        const int g = (x==32 && y==32 ? 204 : 0);
        const int b = (x==32 && y==32 ? 221 : 0);
        CHECK_PIXEL_NEAR(&pixels[i], r,g,b,255, 1, x, y);
    }
    mem_free(pixels);

    /* Check that changing any single component of the fog color results
     * in rendering differences (in case of optimization bugs). */

    graphics_set_fog_color(&(Vector4f){0.4, 0.4, 0.6, 0.8});
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0, 0, 2.0/3.0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int r = (x==32 && y==32 ? 153 : 0);
        const int g = (x==32 && y==32 ? 153 : 0);
        const int b = (x==32 && y==32 ? 187 : 0);
        CHECK_PIXEL_NEAR(&pixels[i], r,g,b,255, 1, x, y);
    }
    mem_free(pixels);

    graphics_set_fog_color(&(Vector4f){0.4, 0.6, 0.6, 0.8});
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0, 0, 2.0/3.0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int r = (x==32 && y==32 ? 153 : 0);
        const int g = (x==32 && y==32 ? 187 : 0);
        const int b = (x==32 && y==32 ? 187 : 0);
        CHECK_PIXEL_NEAR(&pixels[i], r,g,b,255, 1, x, y);
    }
    mem_free(pixels);

    graphics_set_fog_color(&(Vector4f){0.4, 0.6, 0.8, 0.8});
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0, 0, 2.0/3.0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int r = (x==32 && y==32 ? 153 : 0);
        const int g = (x==32 && y==32 ? 187 : 0);
        const int b = (x==32 && y==32 ? 221 : 0);
        CHECK_PIXEL_NEAR(&pixels[i], r,g,b,255, 1, x, y);
    }
    mem_free(pixels);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fog_color_invalid)
{
    uint8_t *pixels;

    graphics_set_viewport(0, 0, 65, 65);  // Intentionally 65.
    graphics_enable_fog(1);

    /* A NULL value should not change the fog color. */
    graphics_set_fog_color(&(Vector4f){0.2, 0.4, 0.6, 1});
    graphics_set_fog_color(NULL);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0, 0, 2.0/3.0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int r = (x==32 && y==32 ? 119 : 0);
        const int g = (x==32 && y==32 ? 153 : 0);
        const int b = (x==32 && y==32 ? 187 : 0);
        CHECK_PIXEL_NEAR(&pixels[i], r,g,b,255, 1, x, y);
    }
    mem_free(pixels);

    /* The fog color should be clamped to [0,1]. */
    graphics_set_fog_color(&(Vector4f){-1, -1, -1, -1});
    graphics_set_fixed_color(&(Vector4f){0.4, 0.4, 0.4, 1});
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0, 0, 0.5}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x==32 && y==32 ? 51 : 0);
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    graphics_set_fog_color(&(Vector4f){2, 2, 2, 2});
    graphics_set_fixed_color(&(Vector4f){0.6, 0.6, 0.6, 1});
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0, 0, 0.5}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x==32 && y==32 ? 204 : 0);
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fog_new_frame)
{
    uint8_t *pixels;

    graphics_set_viewport(0, 0, 65, 65);  // Intentionally 65.

    /* Check that fog state is retained across a frame change. */
    graphics_enable_fog(1);
    graphics_set_fog_start(0.25);
    graphics_set_fog_end(0.625);
    graphics_set_fog_color(&(Vector4f){0.2, 0.4, 0.6, 1});
    graphics_finish_frame();
    graphics_start_frame();
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0, 0, 0.5}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int r = (x==32 && y==32 ? 119 : 0);
        const int g = (x==32 && y==32 ? 153 : 0);
        const int b = (x==32 && y==32 ? 187 : 0);
        CHECK_PIXEL_NEAR(&pixels[i], r,g,b,255, 1, x, y);
    }
    mem_free(pixels);

    return 1;
}

/*************************************************************************/
/************************ Depth buffer range tests ***********************/
/*************************************************************************/

/* Note: We run these tests separately from the other depth-buffer-related
 * tests so we can detect a failure to properly initialize internal state
 * on OpenGL, which manifests as fog range not being properly applied when
 * shaders are enabled. */

TEST(test_depth_range)
{
    used_depth_range = 1;

    graphics_set_viewport(0, 0, 64, 64);
    graphics_enable_depth_test(1);

    /* The default depth buffer range is [0,1] (full range), so this should
     * result in a depth value of 0.5. */
    draw_square(0, 1,1,1,1);

    /* Check that the depth range is properly applied.  To avoid spurious
     * failures due to depth value rounding, we set ranges and Z values to
     * give depth values slightly greater or less than 0.5 (depending on
     * which way we're testing). */
    graphics_set_depth_range(0.1, 0.6);
    draw_square(0.601, 1,0,0,1);
    CHECK_SQUARE(1,1,1);
    draw_square(0.599, 1,0,1,1);
    CHECK_SQUARE(1,0,1);
    graphics_set_depth_range(0.4, 0.9);
    draw_square(-0.600, 0,1,0,1);
    CHECK_SQUARE(1,0,1);
    draw_square(-0.602, 0,1,1,1);
    CHECK_SQUARE(0,1,1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_depth_range_new_frame)
{
    graphics_set_viewport(0, 0, 64, 64);
    graphics_enable_depth_test(1);

    /* Check that the depth range is retained across a frame change. */
    graphics_set_depth_range(0.1, 0.6);
    graphics_finish_frame();
    graphics_start_frame();
    graphics_clear(0,0,0,0, 1, 0);
    draw_square(0.601, 1,0,0,1);
    CHECK_SQUARE(1,0,0);
    draw_square(0.599, 0,1,0,1);
    CHECK_SQUARE(0,1,0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_depth_range_invalid)
{
    graphics_set_viewport(0, 0, 64, 64);
    graphics_enable_depth_test(1);

    graphics_set_depth_range(0, 0.7);
    draw_square(1, 1,0,0,1);  // depth = 0.7
    CHECK_SQUARE(1,0,0);

    graphics_set_depth_range(0.5, 1);
    draw_square(0, 0,1,0,1);  // depth = 0.75
    CHECK_SQUARE(1,0,0);

    /* None of these should change the depth range. */
    graphics_set_depth_range(-1, 0.5);
    draw_square(0, 0,0,1,1);
    graphics_set_depth_range(0, -1);
    draw_square(0, 0,0,1,1);
    graphics_set_depth_range(1.1, 0.5);
    draw_square(0, 0,0,1,1);
    graphics_set_depth_range(0, 1.1);
    draw_square(0, 0,0,1,1);
    CHECK_SQUARE(1,0,0);

    return 1;
}

/*************************************************************************/
/*************************** Point size tests ****************************/
/*************************************************************************/

TEST(test_point_size)
{
    if (graphics_max_point_size() == 1.0f) {
        SKIP("Non-unit line widths not supported on this system.");
    }

    used_point_size = 1;

    uint8_t *pixels;

    graphics_set_viewport(0, 0, 64, 64);

    /* The default point size should be 1 pixel. */
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){1.0/64, 1.0/64, 0},
                                   NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x == 32 && y == 32) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* Check that we can change the point size. */
    graphics_set_point_size(2);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0, 0, 0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x>=32-1 && x<32+1 && y>=32-1 && y<32+1) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_point_size_new_frame)
{
    if (graphics_max_point_size() == 1.0f) {
        SKIP("Non-unit line widths not supported on this system.");
    }

    uint8_t *pixels;

    graphics_set_viewport(0, 0, 64, 64);

    /* Check that the point size is retained across a frame change. */
    graphics_set_point_size(2);
    graphics_finish_frame();
    graphics_start_frame();
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0, 0, 0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x>=32-1 && x<32+1 && y>=32-1 && y<32+1) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    return 1;
}

/*************************************************************************/
/***************************** Stencil tests *****************************/
/*************************************************************************/

TEST(test_stencil)
{
    used_stencil = 1;

    graphics_set_viewport(0, 0, 64, 64);

    /* Set a value of 200 in the stencil buffer. */
    graphics_enable_stencil_test(1);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_TRUE, 200, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_REPLACE,
                                    GRAPHICS_STENCIL_REPLACE,
                                    GRAPHICS_STENCIL_REPLACE);
    draw_square(0, 1,1,1,1);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP);

    /* Check that all comparisons work as expected.  Note that the use of
     * 127 will cause ordered comparisons to fail if the stencil buffer
     * has less than 8 bits (we explicitly request 8 bits for these tests). */

    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_TRUE, 127, 255);
    draw_square(0, 1,0,0,1);
    CHECK_SQUARE(1,0,0);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_TRUE, 200, 255);
    draw_square(0, 0,1,0,1);
    CHECK_SQUARE(0,1,0);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_TRUE, 255, 255);
    draw_square(0, 0,0,1,1);
    CHECK_SQUARE(0,0,1);

    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_FALSE, 127, 255);
    draw_square(0, 1,1,1,1);
    CHECK_SQUARE(0,0,1);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_FALSE, 200, 255);
    draw_square(0, 1,1,1,1);
    CHECK_SQUARE(0,0,1);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_FALSE, 255, 255);
    draw_square(0, 1,1,1,1);
    CHECK_SQUARE(0,0,1);

    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_EQUAL, 127, 255);
    draw_square(0, 1,0,0,1);
    CHECK_SQUARE(0,0,1);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_EQUAL, 200, 255);
    draw_square(0, 0,1,0,1);
    CHECK_SQUARE(0,1,0);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_EQUAL, 255, 255);
    draw_square(0, 0,0,1,1);
    CHECK_SQUARE(0,1,0);

    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_NOT_EQUAL, 127, 255);
    draw_square(0, 0,1,1,1);
    CHECK_SQUARE(0,1,1);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_NOT_EQUAL, 200, 255);
    draw_square(0, 1,0,1,1);
    CHECK_SQUARE(0,1,1);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_NOT_EQUAL, 255, 255);
    draw_square(0, 1,1,0,1);
    CHECK_SQUARE(1,1,0);

    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_LESS, 127, 255);
    draw_square(0, 1,0,0,1);
    CHECK_SQUARE(1,1,0);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_LESS, 200, 255);
    draw_square(0, 0,1,0,1);
    CHECK_SQUARE(1,1,0);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_LESS, 255, 255);
    draw_square(0, 0,0,1,1);
    CHECK_SQUARE(0,0,1);

    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_LESS_EQUAL, 127, 255);
    draw_square(0, 0,1,1,1);
    CHECK_SQUARE(0,0,1);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_LESS_EQUAL, 200, 255);
    draw_square(0, 1,0,1,1);
    CHECK_SQUARE(1,0,1);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_LESS_EQUAL, 255, 255);
    draw_square(0, 1,1,0,1);
    CHECK_SQUARE(1,1,0);

    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_GREATER_EQUAL, 127, 255);
    draw_square(0, 1,0,0,1);
    CHECK_SQUARE(1,0,0);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_GREATER_EQUAL, 200, 255);
    draw_square(0, 0,1,0,1);
    CHECK_SQUARE(0,1,0);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_GREATER_EQUAL, 255, 255);
    draw_square(0, 0,0,1,1);
    CHECK_SQUARE(0,1,0);

    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_GREATER, 127, 255);
    draw_square(0, 0,1,1,1);
    CHECK_SQUARE(0,1,1);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_GREATER, 200, 255);
    draw_square(0, 1,0,1,1);
    CHECK_SQUARE(0,1,1);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_GREATER, 255, 255);
    draw_square(0, 1,1,0,1);
    CHECK_SQUARE(0,1,1);

    /* Check that the mask is handled correctly. */

    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_EQUAL, 9, 8);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP);
    draw_square(0, 0,1,0,1);
    CHECK_SQUARE(0,1,0);

    /* Check that all operations (other than REPLACE and KEEP, which we've
     * already used) work as expected.  We can't read the stencil buffer
     * directly, so we use an EQUAL comparison to check whether we got the
     * expected value. */

    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_TRUE, 0, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_INCR,
                                    GRAPHICS_STENCIL_INCR,
                                    GRAPHICS_STENCIL_INCR);
    draw_square(0, 1,0,0,1);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_EQUAL, 201, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP);
    draw_square(0, 0,1,0,1);
    CHECK_SQUARE(0,1,0);

    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_TRUE, 0, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_DECR,
                                    GRAPHICS_STENCIL_DECR,
                                    GRAPHICS_STENCIL_DECR);
    draw_square(0, 1,0,0,1);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_EQUAL, 200, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP);
    draw_square(0, 0,1,0,1);
    CHECK_SQUARE(0,1,0);

    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_TRUE, 0, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_INVERT,
                                    GRAPHICS_STENCIL_INVERT,
                                    GRAPHICS_STENCIL_INVERT);
    draw_square(0, 1,0,0,1);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_EQUAL, 55, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP);
    draw_square(0, 0,1,0,1);
    CHECK_SQUARE(0,1,0);

    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_TRUE, 0, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_CLEAR,
                                    GRAPHICS_STENCIL_CLEAR,
                                    GRAPHICS_STENCIL_CLEAR);
    draw_square(0, 1,0,0,1);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_EQUAL, 0, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP);
    draw_square(0, 0,1,0,1);
    CHECK_SQUARE(0,1,0);

    /* DECR and INCR should clamp to the range of the stencil buffer. */

    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_TRUE, 0, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_DECR,
                                    GRAPHICS_STENCIL_DECR,
                                    GRAPHICS_STENCIL_DECR);
    draw_square(0, 1,0,0,1);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_EQUAL, 0, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP);
    draw_square(0, 0,1,0,1);
    CHECK_SQUARE(0,1,0);

    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_TRUE, 255, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_REPLACE,
                                    GRAPHICS_STENCIL_REPLACE,
                                    GRAPHICS_STENCIL_REPLACE);
    draw_square(0, 1,0,0,1);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_INCR,
                                    GRAPHICS_STENCIL_INCR,
                                    GRAPHICS_STENCIL_INCR);
    draw_square(0, 1,0,0,1);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_EQUAL, 255, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP);
    draw_square(0, 0,1,0,1);
    CHECK_SQUARE(0,1,0);

    /* graphics_clear() should clear the stencil buffer. */
    graphics_clear(0,0,0,0, 1, 0);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_EQUAL, 0, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP);
    draw_square(0, 0,0,1,1);
    CHECK_SQUARE(0,0,1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_stencil_depth_test)
{
    graphics_set_viewport(0, 0, 64, 64);
    graphics_enable_stencil_test(1);

    graphics_enable_depth_test(1);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_TRUE, 200, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_REPLACE,
                                    GRAPHICS_STENCIL_REPLACE,
                                    GRAPHICS_STENCIL_REPLACE);
    draw_square(0, 1,1,1,1);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_TRUE, 100, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_REPLACE,
                                    GRAPHICS_STENCIL_DECR,
                                    GRAPHICS_STENCIL_INCR);
    draw_square(0.1, 0,0,1,1);
    CHECK_SQUARE(1,1,1);
    graphics_enable_depth_test(0);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_EQUAL, 199, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP);
    draw_square(0, 0,1,0,1);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_NOT_EQUAL, 199, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP);
    draw_square(0, 1,0,0,1);
    CHECK_SQUARE(0,1,0);

    graphics_enable_depth_test(1);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_TRUE, 200, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_REPLACE,
                                    GRAPHICS_STENCIL_REPLACE,
                                    GRAPHICS_STENCIL_REPLACE);
    draw_square(-0.1, 1,1,1,1);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_TRUE, 100, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_REPLACE,
                                    GRAPHICS_STENCIL_DECR,
                                    GRAPHICS_STENCIL_INCR);
    draw_square(-0.2, 0,0,1,1);
    CHECK_SQUARE(0,0,1);
    graphics_enable_depth_test(0);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_EQUAL, 201, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP);
    draw_square(0, 0,1,0,1);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_NOT_EQUAL, 201, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP);
    draw_square(0, 1,0,0,1);
    CHECK_SQUARE(0,1,0);

    graphics_enable_depth_test(1);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_TRUE, 200, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_REPLACE,
                                    GRAPHICS_STENCIL_REPLACE,
                                    GRAPHICS_STENCIL_REPLACE);
    draw_square(-0.3, 1,1,1,1);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_FALSE, 100, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_REPLACE,
                                    GRAPHICS_STENCIL_DECR,
                                    GRAPHICS_STENCIL_INCR);
    draw_square(-0.2, 0,0,1,1);
    CHECK_SQUARE(1,1,1);
    graphics_enable_depth_test(0);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_EQUAL, 100, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP);
    draw_square(0, 0,1,0,1);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_NOT_EQUAL, 100, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP);
    draw_square(0, 1,0,0,1);
    CHECK_SQUARE(0,1,0);

    graphics_enable_depth_test(1);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_TRUE, 200, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_REPLACE,
                                    GRAPHICS_STENCIL_REPLACE,
                                    GRAPHICS_STENCIL_REPLACE);
    draw_square(-0.4, 1,1,1,1);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_FALSE, 150, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_REPLACE,
                                    GRAPHICS_STENCIL_DECR,
                                    GRAPHICS_STENCIL_INCR);
    draw_square(-0.5, 0,0,1,1);
    CHECK_SQUARE(1,1,1);
    graphics_enable_depth_test(0);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_EQUAL, 150, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP);
    draw_square(0, 0,1,0,1);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_NOT_EQUAL, 150, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP);
    draw_square(0, 1,0,0,1);
    CHECK_SQUARE(0,1,0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_stencil_alpha_test)
{
    graphics_set_viewport(0, 0, 64, 64);
    graphics_enable_stencil_test(1);

    /* Check that pixels dropped by the alpha test do not affect the
     * stencil buffer. */

    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_TRUE, 200, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_REPLACE,
                                    GRAPHICS_STENCIL_REPLACE,
                                    GRAPHICS_STENCIL_REPLACE);
    draw_square(0, 1,1,1,1);

    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_TRUE, 0, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_INCR,
                                    GRAPHICS_STENCIL_INCR,
                                    GRAPHICS_STENCIL_INCR);
    graphics_enable_alpha_test(1);
    graphics_set_alpha_test_reference(0.5);
    draw_square(0, 0,0,1,0.2);  // Will not be drawn.

    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_EQUAL, 200, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP);
    draw_square(0, 0,1,0,1);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_NOT_EQUAL, 200, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP);
    draw_square(0, 1,0,0,1);
    CHECK_SQUARE(0,1,0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_stencil_new_frame)
{
    graphics_set_viewport(0, 0, 64, 64);

    /* Check that stencil state is retained across a frame change. */
    graphics_enable_stencil_test(1);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_EQUAL, 0, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_INCR,
                                    GRAPHICS_STENCIL_INCR,
                                    GRAPHICS_STENCIL_INCR);
    graphics_finish_frame();
    graphics_start_frame();
    graphics_clear(0,0,0,0, 1, 0);
    draw_square(0, 0,1,0,1);
    draw_square(0, 1,0,0,1);
    CHECK_SQUARE(0,1,0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_stencil_invalid)
{
    graphics_set_viewport(0, 0, 64, 64);

    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_TRUE, 200, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_REPLACE,
                                    GRAPHICS_STENCIL_REPLACE,
                                    GRAPHICS_STENCIL_REPLACE);
    draw_square(0, 1,0,0,1);

    graphics_set_stencil_operations(GRAPHICS_STENCIL_INCR,
                                    GRAPHICS_STENCIL_INCR,
                                    GRAPHICS_STENCIL_INCR);
    /* None of these should affect the stencil state. */
    graphics_set_stencil_comparison(-1, 200, 255);
    graphics_set_stencil_operations(-1,
                                    GRAPHICS_STENCIL_REPLACE,
                                    GRAPHICS_STENCIL_REPLACE);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_REPLACE,
                                    -1,
                                    GRAPHICS_STENCIL_REPLACE);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_REPLACE,
                                    GRAPHICS_STENCIL_REPLACE,
                                    -1);
    draw_square(0, 1,0,0,1);

    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_EQUAL, 201, 255);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP);
    draw_square(0, 0,1,0,1);
    CHECK_SQUARE(0,1,0);

    return 1;
}

/*************************************************************************/
/************************* Texture offset tests **************************/
/*************************************************************************/

TEST(test_texture_offset)
{
    used_texture_offset = 1;

    uint8_t *pixels;

    int texture;
    static const ALIGNED(4) uint8_t alpha_tex_data[] = {
        'T','E','X', 10,  2, 64,  0,  0,  0,  2,  0,  2,  0,  1,  0,  0,
          0,  0,  0, 32,  0,  0,  0,  4,  0,  0,  0,  0,  0,  0,  0,  0,
          1, 51,254,255
    };
    /* Safe to de-const the input buffer since we're not reusing it. */
    ASSERT(texture = texture_parse((void *)alpha_tex_data,
                                   sizeof(alpha_tex_data), 0, 0, 0));
    texture_set_antialias(texture, 0);
    texture_apply(0, texture);

    graphics_set_viewport(0, 0, 64, 64);

    /* By default, there should be no texture offset.  (This has already
     * been checked indirectly by numerous other tests, but check it
     * explicitly here just for completeness.) */
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0},
                                   &(Vector2f){0, 0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0},
                                   &(Vector2f){0, 1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0},
                                   &(Vector2f){1, 1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0},
                                   &(Vector2f){1, 0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int x4 = x/16, y4 = y/16;
        const int p = (x4==1 && y4==1 ?   1 :
                       x4==2 && y4==1 ?  51 :
                       x4==1 && y4==2 ? 254 :
                       x4==2 && y4==2 ? 255 : 0);
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* Check U (horizontal) coordinate offset. */
    graphics_set_texture_offset(&(Vector2f){0.5, 0});
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0},
                                   &(Vector2f){0, 0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0},
                                   &(Vector2f){0, 1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0},
                                   &(Vector2f){1, 1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0},
                                   &(Vector2f){1, 0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int x4 = x/16, y4 = y/16;
        const int p = (x4==1 && y4==1 ?  51 :
                       x4==2 && y4==1 ?   1 :
                       x4==1 && y4==2 ? 255 :
                       x4==2 && y4==2 ? 254 : 0);
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* Check V (vertical) coordinate offset. */
    graphics_set_texture_offset(&(Vector2f){0.5, 0.5});
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0},
                                   &(Vector2f){0, 0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0},
                                   &(Vector2f){0, 1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0},
                                   &(Vector2f){1, 1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0},
                                   &(Vector2f){1, 0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int x4 = x/16, y4 = y/16;
        const int p = (x4==1 && y4==1 ? 255 :
                       x4==2 && y4==1 ? 254 :
                       x4==1 && y4==2 ?  51 :
                       x4==2 && y4==2 ?   1 : 0);
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_texture_offset_new_frame)
{
    uint8_t *pixels;

    int texture;
    static const ALIGNED(4) uint8_t alpha_tex_data[] = {
        'T','E','X', 10,  2, 64,  0,  0,  0,  2,  0,  2,  0,  1,  0,  0,
          0,  0,  0, 32,  0,  0,  0,  4,  0,  0,  0,  0,  0,  0,  0,  0,
          1, 51,254,255
    };
    /* Safe to de-const the input buffer since we're not reusing it. */
    ASSERT(texture = texture_parse((void *)alpha_tex_data,
                                   sizeof(alpha_tex_data), 0, 0, 0));
    texture_set_antialias(texture, 0);
    texture_apply(0, texture);

    graphics_set_viewport(0, 0, 64, 64);

    /* Check that the texture offset is retained across a frame change. */
    graphics_set_texture_offset(&(Vector2f){0.5, 0.5});
    graphics_finish_frame();
    graphics_start_frame();
    graphics_clear(0,0,0,0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0},
                                   &(Vector2f){0, 0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0},
                                   &(Vector2f){0, 1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0},
                                   &(Vector2f){1, 1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0},
                                   &(Vector2f){1, 0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int x4 = x/16, y4 = y/16;
        const int p = (x4==1 && y4==1 ? 255 :
                       x4==2 && y4==1 ? 254 :
                       x4==1 && y4==2 ?  51 :
                       x4==2 && y4==2 ?   1 : 0);
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_texture_offset_invalid)
{
    uint8_t *pixels;

    int texture;
    static const ALIGNED(4) uint8_t alpha_tex_data[] = {
        'T','E','X', 10,  2, 64,  0,  0,  0,  2,  0,  2,  0,  1,  0,  0,
          0,  0,  0, 32,  0,  0,  0,  4,  0,  0,  0,  0,  0,  0,  0,  0,
          1, 51,254,255
    };
    /* Safe to de-const the input buffer since we're not reusing it. */
    ASSERT(texture = texture_parse((void *)alpha_tex_data,
                                   sizeof(alpha_tex_data), 0, 0, 0));
    texture_set_antialias(texture, 0);
    texture_apply(0, texture);

    graphics_set_viewport(0, 0, 64, 64);

    graphics_set_texture_offset(&(Vector2f){0.5, 0.5});
    /* This should not alter the current texture offset. */
    graphics_set_texture_offset(NULL);

    graphics_clear(0,0,0,0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0},
                                   &(Vector2f){0, 0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0},
                                   &(Vector2f){0, 1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0},
                                   &(Vector2f){1, 1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0},
                                   &(Vector2f){1, 0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int x4 = x/16, y4 = y/16;
        const int p = (x4==1 && y4==1 ? 255 :
                       x4==2 && y4==1 ? 254 :
                       x4==1 && y4==2 ?  51 :
                       x4==2 && y4==2 ?   1 : 0);
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    texture_destroy(texture);
    return 1;
}

/*************************************************************************/
/********************** Exhaustive rendering tests ***********************/
/*************************************************************************/

#undef FAIL_ACTION
#define FAIL_ACTION  failed = 1; break

TEST(test_all_render_combinations)
{
    /*
     * This test checks all combinations of:
     *    - number of position components (2, 3, 4)
     *    - texture type (none, RGB+alpha, alpha only, luminance)
     *    - per-vertex color (absent, present)
     *    - fixed color (absent, present)
     *    - fog (disabled, enabled)
     *    - alpha test (disabled, greater than low/high, less than high)
     * While not truly exhaustive, this should cover all common
     * interactions between various rendering parameters.
     */

    int failed = 0;

    graphics_set_viewport(0, 0, 64, 64);
    graphics_set_projection_matrix(&(Matrix4f){ 0.5, 0.0, 0.0, 0.0,
                                                0.0, 1.0, 0.0, 0.0,
                                                0.0, 0.0, 1.0, 0.0,
                                               0.25,-0.25,0.0, 1.0});
    graphics_set_view_matrix(&(Matrix4f){ 1.0, 0.0, 0.0, 0.0,
                                          0.0, 1.0, 0.0, 0.0,
                                          0.0, 0.0, 2.0, 0.0,
                                          0.0, 0.0, 0.8, 1.0});

    /**** Data for rendering. ****/

    static const Vector2f positions_2[] = {{-1,-0.5}, {-1,+0.5},
                                           {+1,+0.5}, {+1,-0.5}};
    static const Vector3f positions_3[] = {{-1,-0.5,-0.2}, {-1,+0.5,-0.2},
                                           {+1,+0.5,-0.2}, {+1,-0.5,-0.2}};
#ifndef SIL_PLATFORM_PSP  // 4 components not supported on PSP.
    static const Vector4f positions_4[] = {{-1,-0.5,-0.2,1}, {-1,+0.5,-0.2,1},
                                           {+1,+0.5,-0.2,1}, {+1,-0.5,-0.2,1}};
#endif
    static const Vector2f texcoords[] = {{0,0}, {0,1}, {1,1}, {1,0}};
    static const Vector4f vertex_color = {1.0, 1.0, 1.0, 2.0/3.0};
    static const Vector4f fixed_color = {0.8, 0.6, 0.4, 2.0/3.0};
    static const Vector4f fog_color = {0.9, 0.3, 0.7, 1.0};
    static const ALIGNED(4) uint8_t rgba_tex_data[] = {
        'T','E','X', 10,  2,  0,  0,  0,  0,  2,  0,  2,  0,  1,  0,  0,
          0,  0,  0, 32,  0,  0,  0, 16,  0,  0,  0,  0,  0,  0,  0,  0,
        255,255,255,102,255,255,255,153,255,255,255,204,255,255,255,255,
    };
    static const ALIGNED(4) uint8_t alpha_tex_data[] = {
        'T','E','X', 10,  2, 64,  0,  0,  0,  2,  0,  2,  0,  1,  0,  0,
          0,  0,  0, 32,  0,  0,  0,  4,  0,  0,  0,  0,  0,  0,  0,  0,
        102,153,204,255,
    };
    static const ALIGNED(4) uint8_t luminance_tex_data[] = {
        'T','E','X', 10,  2, 65,  0,  0,  0,  2,  0,  2,  0,  1,  0,  0,
          0,  0,  0, 32,  0,  0,  0,  4,  0,  0,  0,  0,  0,  0,  0,  0,
        102,153,204,255,
    };

    int rgba_texture, alpha_texture, luminance_texture;
    ASSERT(rgba_texture = texture_parse(
               (void *)rgba_tex_data, sizeof(rgba_tex_data), 0, 0, 0));
    texture_set_antialias(rgba_texture, 0);
    ASSERT(alpha_texture = texture_parse(
               (void *)alpha_tex_data, sizeof(alpha_tex_data), 0, 0, 0));
    texture_set_antialias(alpha_texture, 0);
    ASSERT(luminance_texture = texture_parse(
               (void *)luminance_tex_data, sizeof(luminance_tex_data),
               0, 0, 0));
    texture_set_antialias(luminance_texture, 0);

    /**** Data sets to test. ****/

    const struct {
        int count;
        const float *data;
        uint32_t format;
    } positions[] = {
        {2, &positions_2[0].x, GRAPHICS_VERTEX_FORMAT(POSITION_2F, 0)},
        {3, &positions_3[0].x, GRAPHICS_VERTEX_FORMAT(POSITION_3F, 0)},
#ifndef SIL_PLATFORM_PSP  // 4 components not supported on PSP.
        {4, &positions_4[0].x, GRAPHICS_VERTEX_FORMAT(POSITION_4F, 0)},
#endif
    };
    const int textures[] = {0, rgba_texture, alpha_texture, luminance_texture};
    static const float alpha_refs[] = {0.0, 0.15, 0.65, -0.65};

    /**** Nested test loops. ****/

    for (int i_position = 0; i_position < lenof(positions); i_position++) {
     for (int i_texture = 0; i_texture < lenof(textures); i_texture++) {
      for (int use_tex_offset = 0; use_tex_offset <= 1; use_tex_offset++) {

       /* Insert a frame break here, in case the repeated write/read cycles
        * confuse the hardware or libraries. */
       graphics_finish_frame();
       graphics_start_frame();

       for (int use_vertex_color = 0; use_vertex_color <= 1;
            use_vertex_color++) {
        for (int use_fixed_color = 0; use_fixed_color <= 1;
             use_fixed_color++) {
         for (int use_fog = 0; use_fog <= 1; use_fog++) {
          for (int i_alpha_ref = 0; i_alpha_ref < lenof(alpha_refs);
               i_alpha_ref++) {

           /**** Single test implementation. ****/

           /* Set rendering parameters for this test. */
           texture_apply(0, textures[i_texture]);
           if (use_fixed_color) {
               graphics_set_fixed_color(&fixed_color);
           } else {
               graphics_set_fixed_color(&(Vector4f){1,1,1,1});
           }
           if (use_fog) {
               graphics_enable_fog(1);
               graphics_set_fog_color(&fog_color);
           } else {
               graphics_enable_fog(0);
           }
           const float alpha_ref = alpha_refs[i_alpha_ref];
           if (alpha_ref > 0) {
               graphics_enable_alpha_test(1);
               graphics_set_alpha_test_comparison(GRAPHICS_COMPARISON_GREATER);
               graphics_set_alpha_test_reference(alpha_ref);
           } else if (alpha_ref < 0) {
               graphics_enable_alpha_test(1);
               graphics_set_alpha_test_comparison(GRAPHICS_COMPARISON_LESS);
               graphics_set_alpha_test_reference(-alpha_ref);
           } else {
               graphics_enable_alpha_test(0);
           }
           if (use_tex_offset) {
               graphics_set_texture_offset(&(Vector2f){0.5, 0.5});
           } else {
               graphics_set_texture_offset(&(Vector2f){0, 0});
           }

           /* Determine the vertex format and size for the quad to be
            * rendered (a square). */
           uint32_t vertex_format[4];
           unsigned int i_vertex_format = 0;
           unsigned int vertex_size = 0;
           vertex_format[i_vertex_format++] =
               positions[i_position].format + vertex_size;
           vertex_size += 4 * positions[i_position].count;
           if (textures[i_texture] != 0) {
               vertex_format[i_vertex_format++] =
                   GRAPHICS_VERTEX_FORMAT(TEXCOORD_2F, vertex_size);
               vertex_size += 4 * 2;
           }
           if (use_vertex_color) {
               vertex_format[i_vertex_format++] =
                   GRAPHICS_VERTEX_FORMAT(COLOR_4F, vertex_size);
               vertex_size += 4 * 4;
           }
           vertex_format[i_vertex_format] = 0;

           /* Create the actual vertex data. */
           float vertex_buffer[(4+2+4)*4];  // Enough for all vertex types.
           unsigned int i_vertex_buffer = 0;
           for (unsigned int vertex = 0; vertex < 4; vertex++) {
               const unsigned int pos_count = positions[i_position].count;
               for (unsigned int i = 0; i < pos_count; i++) {
                   vertex_buffer[i_vertex_buffer++] =
                       positions[i_position].data[vertex*pos_count + i];
               }
               if (textures[i_texture] != 0) {
                   vertex_buffer[i_vertex_buffer++] = texcoords[vertex].x;
                   vertex_buffer[i_vertex_buffer++] = texcoords[vertex].y;
               }
               if (use_vertex_color) {
                   vertex_buffer[i_vertex_buffer++] = vertex_color.x;
                   vertex_buffer[i_vertex_buffer++] = vertex_color.y;
                   vertex_buffer[i_vertex_buffer++] = vertex_color.z;
                   vertex_buffer[i_vertex_buffer++] = vertex_color.w;
               }
           }
           ASSERT(i_vertex_buffer = 4*(vertex_size/4));

           /* Draw the square. */
           graphics_clear(0, 0, 0, 0, 1, 0);
           graphics_draw_vertices(GRAPHICS_PRIMITIVE_QUADS, vertex_buffer,
                                  vertex_format, vertex_size, 4);

           /* Determine what colors to expect for each quadrant of the
            * square. */
           Vector4f color00, color01, color10, color11;
           Vector4f base_color = {1,1,1,1};
           if (use_vertex_color) {
               base_color = vec4_mul(base_color, vertex_color);
           }
           if (use_fixed_color) {
               base_color = vec4_mul(base_color, fixed_color);
           }
           color00 = base_color;
           color01 = base_color;
           color10 = base_color;
           color11 = base_color;
           if (textures[i_texture] != 0) {
               float factor00, factor01, factor10, factor11;
               if (use_tex_offset) {
                   factor00 = 1.0f;
                   factor01 = 0.8f;
                   factor10 = 0.6f;
                   factor11 = 0.4f;
               } else {
                   factor00 = 0.4f;
                   factor01 = 0.6f;
                   factor10 = 0.8f;
                   factor11 = 1.0f;
               }
               if (textures[i_texture] == luminance_texture) {
                   color00.x *= factor00;
                   color00.y *= factor00;
                   color00.z *= factor00;
                   color01.x *= factor01;
                   color01.y *= factor01;
                   color01.z *= factor01;
                   color10.x *= factor10;
                   color10.y *= factor10;
                   color10.z *= factor10;
                   color11.x *= factor11;
                   color11.y *= factor11;
                   color11.z *= factor11;
               } else {
                   color00.w *= factor00;
                   color01.w *= factor01;
                   color10.w *= factor10;
                   color11.w *= factor11;
               }
           }
           if (use_fog) {
               const float factor = (positions[i_position].count == 2
                                     ? 0.8 : 0.4);
               const Vector4f fog_add = vec4_mul(
                   fog_color, (Vector4f){factor, factor, factor, 0});
               const Vector4f color_scale = {1-factor, 1-factor, 1-factor, 1};
               color00 = vec4_add(fog_add, vec4_mul(color00, color_scale));
               color01 = vec4_add(fog_add, vec4_mul(color01, color_scale));
               color10 = vec4_add(fog_add, vec4_mul(color10, color_scale));
               color11 = vec4_add(fog_add, vec4_mul(color11, color_scale));
           }
           if (alpha_ref < 0) {
               if (color00.w > -alpha_ref) {
                   color00.w = 0;
               }
               if (color01.w > -alpha_ref) {
                   color01.w = 0;
               }
               if (color10.w > -alpha_ref) {
                   color10.w = 0;
               }
               if (color11.w > -alpha_ref) {
                   color11.w = 0;
               }
           } else {
               if (color00.w < alpha_ref) {
                   color00.w = 0;
               }
               if (color01.w < alpha_ref) {
                   color01.w = 0;
               }
               if (color10.w < alpha_ref) {
                   color10.w = 0;
               }
               if (color11.w < alpha_ref) {
                   color11.w = 0;
               }
           }
           color00.x *= color00.w;
           color00.y *= color00.w;
           color00.z *= color00.w;
           color01.x *= color01.w;
           color01.y *= color01.w;
           color01.z *= color01.w;
           color10.x *= color10.w;
           color10.y *= color10.w;
           color10.z *= color10.w;
           color11.x *= color11.w;
           color11.y *= color11.w;
           color11.z *= color11.w;

           /* Check that the rendered square matches what we expect.  To
            * handle rounding error, we accept a color component value if
            * it is within 2 of the rounded and scaled actual value --
            * unless the actual value is 0.0 or 1.0, in which case we
            * require exactly 0 or 255. */
           uint8_t pixels[64*64*4];
           ASSERT(graphics_read_pixels(0, 0, 64, 64, pixels));
           for (int i = 0; i < 64*64; i++) {
               const int x = i % 64;
               const int y = i / 64;
               const uint8_t render_r = pixels[i*4+0];
               const uint8_t render_g = pixels[i*4+1];
               const uint8_t render_b = pixels[i*4+2];
               const uint8_t render_a = pixels[i*4+3];
               int expect_r0, expect_g0, expect_b0;
               int expect_r1, expect_g1, expect_b1;
               const uint8_t expect_a = 255;
               if (x >= 24 && x < 40 && y >= 8 && y < 24) {
                   expect_r0 = iroundf(color00.x * 255);
                   expect_g0 = iroundf(color00.y * 255);
                   expect_b0 = iroundf(color00.z * 255);
               } else if (x >= 40 && x < 56 && y >= 8 && y < 24) {
                   expect_r0 = iroundf(color01.x * 255);
                   expect_g0 = iroundf(color01.y * 255);
                   expect_b0 = iroundf(color01.z * 255);
               } else if (x >= 24 && x < 40 && y >= 24 && y < 40) {
                   expect_r0 = iroundf(color10.x * 255);
                   expect_g0 = iroundf(color10.y * 255);
                   expect_b0 = iroundf(color10.z * 255);
               } else if (x >= 40 && x < 56 && y >= 24 && y < 40) {
                   expect_r0 = iroundf(color11.x * 255);
                   expect_g0 = iroundf(color11.y * 255);
                   expect_b0 = iroundf(color11.z * 255);
               } else {
                   expect_r0 = expect_g0 = expect_b0 = 0;
               }
               if (expect_r0 == 0 || expect_r0 == 255) {
                   expect_r1 = expect_r0;
               } else {
                   expect_r1 = ubound(expect_r0 + 2, 255);
                   expect_r0 = lbound(expect_r0 - 2, 0);
               }
               if (expect_g0 == 0 || expect_g0 == 255) {
                   expect_g1 = expect_g0;
               } else {
                   expect_g1 = ubound(expect_g0 + 2, 255);
                   expect_g0 = lbound(expect_g0 - 2, 0);
               }
               if (expect_b0 == 0 || expect_b0 == 255) {
                   expect_b1 = expect_b0;
               } else {
                   expect_b1 = ubound(expect_b0 + 2, 255);
                   expect_b0 = lbound(expect_b0 - 2, 0);
               }
               if (render_r < expect_r0 || render_r > expect_r1
                || render_g < expect_g0 || render_g > expect_g1
                || render_b < expect_b0 || render_b > expect_b1
                || render_a != expect_a) {
                   FAIL("(%u,%u,%u,%u,%u,%u,%u):"
                        " Pixel %d,%d was %02X%02X%02X%02X,"
                        " expected %02X%02X%02X%02X ... %02X%02X%02X%02X",
                        i_position, i_texture, use_tex_offset,
                        use_vertex_color, use_fixed_color, use_fog,
                        i_alpha_ref, x, y,
                        render_r, render_g, render_b, render_a,
                        expect_r0, expect_g0, expect_b0, render_a,
                        expect_r1, expect_g1, expect_b1, render_a);
               }
           }

          }  // i_alpha_ref
         }  // use_fog
        }  // use_fixed_color
       }  // use_vertex_color
      }  // use_tex_offset
     }  // i_texture
    }  // i_position

    /**** Clean up and return the test result. ****/

    texture_destroy(rgba_texture);
    texture_destroy(alpha_texture);
    texture_destroy(luminance_texture);

    return !failed;
}

/*************************************************************************/
/*************************************************************************/
