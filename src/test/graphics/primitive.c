/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/graphics/primitive.c: Tests for graphics primitive functionality.
 */

#include "src/base.h"
#include "src/graphics.h"
#include "src/graphics/internal.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/test/base.h"
#include "src/test/graphics/internal.h"
#include "src/texture.h"

/*************************************************************************/
/*************************** Common test data ****************************/
/*************************************************************************/

/* Size unit by which the immediate vertex buffer is expanded (copied from
 * $TOPDIR/graphics/primitive.c). */
#define PRIMITIVE_DATA_EXPAND  1024

/*-----------------------------------------------------------------------*/

/* Common vertex and index buffers for basic primitive tests. */
typedef struct BasicVertex {float x,y,z;} BasicVertex;
static BasicVertex  // Not const, because we tweak the positions.
    point_vertices[] = {{-1,-1,0}},
    point_indexed_vertices[] = {{-0.5,-0.5,0}, {-1,-1,0}};
static const BasicVertex
    quad_vertices[] = {{-1,0,0}, {-1,1,0}, {0,1,0}, {0,0,0}},
    quad_indexed_vertices[] = {{-1,0,0}, {-1,1,0}, {1,1,0}, {0,0,0}, {0,1,0}};
static const uint16_t point_indices[] = {1};
static const uint16_t quad_indices[] = {0,1,4,3};
static const uint32_t basic_vertex_format[] = {
    GRAPHICS_VERTEX_FORMAT(POSITION_3F, offsetof(BasicVertex,x)),
    0
};

/* Common vertex buffer for textured quad tests. */
typedef struct TexturedVertex {float x,y,z,u,v;} TexturedVertex;
static const TexturedVertex tex_quad_vertices[] = {
    {-1,0,0,0,0}, {-1,1,0,0,1}, {0,1,0,1,1}, {0,0,0,1,0}
};
static const uint32_t textured_vertex_format[] = {
    GRAPHICS_VERTEX_FORMAT(POSITION_3F, offsetof(TexturedVertex,x)),
    GRAPHICS_VERTEX_FORMAT(TEXCOORD_2F, offsetof(TexturedVertex,u)),
    0
};

/* Texture data for mipmap testing. */

static const ALIGNED(4) uint8_t tex_data_mipmaps[] = {
    'T','E','X', 10,  2,  0,  2,  0,  0,  4,  0,  4,  0,  1,  0,  0,
      0,  0,  0, 32,  0,  0,  0, 84,  0,  0,  0,  0,  0,  0,  0,  0,
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

#ifdef SIL_PLATFORM_PSP
static const ALIGNED(4) uint8_t tex_data_mipmaps_rgb565_psp[] = {
    'T','E','X', 10,  2,113,  2,  0,  0,  2,  0,  4,  0,  1,  0,  0,
      0,  0,  0, 64,  0,  0,  0,112,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    /* Level 0: red=8 */
      1,  0,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      1,  0,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      1,  0,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      1,  0,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    /* Level 1: green=8 */
     64,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     64,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    /* Level 2: blue=8 */
      0,  8,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};
#else
static const ALIGNED(4) uint8_t tex_data_mipmaps_bgr565[] = {
    'T','E','X', 10,  2,  9,  2,  0,  0,  2,  0,  4,  0,  1,  0,  0,
      0,  0,  0, 32,  0,  0,  0, 22,  0,  0,  0,  0,  0,  0,  0,  0,
    /* Level 0: red=8 */
#ifdef IS_LITTLE_ENDIAN
      0,  8,  0,  8,  0,  8,  0,  8,  0,  8,  0,  8,  0,  8,  0,  8,
#else
      8,  0,  8,  0,  8,  0,  8,  0,  8,  0,  8,  0,  8,  0,  8,  0,
#endif
    /* Level 1: green=8 */
#ifdef IS_LITTLE_ENDIAN
     64,  0, 64,  0,
#else
      0, 64,  0, 64,
#endif
    /* Level 2: blue=8 */
#ifdef IS_LITTLE_ENDIAN
      1,  0,
#else
      0,  1,
#endif
};
#endif  // SIL_PLATFORM_PSP

static const ALIGNED(4) uint8_t tex_data_mipmaps_l8[] = {
    'T','E','X', 10,  2, 65,  2,  0,  0,  2,  0,  4,  0,  1,  0,  0,
      0,  0,  0, 32,  0,  0,  0, 11,  0,  0,  0,  0,  0,  0,  0,  0,
    /* Level 0: value=64 */
     64, 64, 64, 64, 64, 64, 64, 64,
    /* Level 1: value=128 */
    128,128,
    /* Level 2: value=192 */
    192,
};

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

static int wrap_test_graphics_primitive(void);
static int do_test_graphics_primitive(void);
int test_graphics_primitive(void)
{
    return run_tests_in_window(wrap_test_graphics_primitive);
}

static int wrap_test_graphics_primitive(void)
{
    /* Behavior for points right on pixel boundaries varies between
     * renderers, so put the point in the middle of a pixel (but not at
     * the exact center, so the coordinate rounds downward). */
    const float px = -1 + 0.5f/TESTW, py = -1 + 0.5f/TESTH;
    point_vertices[0].x = px;
    point_vertices[0].y = py;
    point_indexed_vertices[0].x = px + 0.5f;
    point_indexed_vertices[0].y = py + 0.5f;
    point_indexed_vertices[1].x = px;
    point_indexed_vertices[1].y = py;

    return do_test_graphics_primitive();
}

DEFINE_GENERIC_TEST_RUNNER(do_test_graphics_primitive)

TEST_INIT(init)
{
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    return 1;
}

TEST_CLEANUP(cleanup)
{
    graphics_finish_frame();
    graphics_flush_resources();
    return 1;
}

/*************************************************************************/
/******************** Basic immediate primitive tests ********************/
/*************************************************************************/

/* The ARM Mali OpenGL driver in Android 6.0 mysteriously fails to draw
 * the very first primitive if that primitive is GL_POINTS.  This appears
 * to be a bug in the driver, so we avoid a "spurious" test failure by
 * drawing a non-point primitive first here. */

TEST(test_immediate_quad)
{
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    uint8_t *pixels;
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x < TESTW/2 && y >= TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_immediate_point)
{
    const float px = -1 + 0.5f/TESTW, py = -1 + 0.5f/TESTH;
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){px,py,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());

    uint8_t *pixels;
    ASSERT(pixels = grab_display());
    CHECK_PIXEL(&pixels[0], 255,255,255,255, 0, 0);
    for (int i = 4; i < TESTW*TESTH*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 0,0,0,255, (i/4) % TESTW, (i/4) / TESTW);
    }
    mem_free(pixels);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_add_vertex_expand_buffer)
{
    const float px = -1 + 0.5f/TESTW, py = -1 + 0.5f/TESTH;
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    for (int i = 0; i < PRIMITIVE_DATA_EXPAND / (3*4); i++) {
        CHECK_TRUE(graphics_add_vertex(&(Vector3f){px,py,0}, NULL, NULL));
    }
    for (int i = 0; i < 1 + (PRIMITIVE_DATA_EXPAND / (3*4)); i++) {
        CHECK_TRUE(graphics_add_vertex(&(Vector3f){px+1,py,0}, NULL, NULL));
    }
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){px,py+1,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());

    uint8_t *pixels;
    ASSERT(pixels = grab_display());
    CHECK_PIXEL(pixels, 255,255,255,255, 0, 0);
    CHECK_PIXEL(&pixels[(TESTW/2)*4], 255,255,255,255, TESTW/2, 0);
    CHECK_PIXEL(&pixels[(TESTH/2)*TESTW*4], 255,255,255,255, 0, TESTH/2);
    mem_free(pixels);

    return 1;
}

/*************************************************************************/
/********************* Basic stored primitive tests **********************/
/*************************************************************************/

TEST(test_stored_immediate_point)
{
    int primitive;
    uint8_t *pixels;

    const float px = -1 + 0.5f/TESTW, py = -1 + 0.5f/TESTH;
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){px,py,0}, NULL, NULL));
    CHECK_TRUE(primitive = graphics_end_primitive());

    /* The primitive should not have been drawn yet. */
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 0,0,0,255, (i/4) % TESTW, (i/4) / TESTW);
    }
    mem_free(pixels);

    graphics_draw_primitive(primitive);
    graphics_destroy_primitive(primitive);

    ASSERT(pixels = grab_display());
    CHECK_PIXEL(&pixels[0], 255,255,255,255, 0, 0);
    for (int i = 4; i < TESTW*TESTH*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 0,0,0,255, (i/4) % TESTW, (i/4) / TESTW);
    }
    mem_free(pixels);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_stored_immediate_quad)
{
    int primitive;
    uint8_t *pixels;

    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,0,0}, NULL, NULL));
    CHECK_TRUE(primitive = graphics_end_primitive());

    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 0,0,0,255, (i/4) % TESTW, (i/4) / TESTW);
    }
    mem_free(pixels);

    graphics_draw_primitive(primitive);
    graphics_destroy_primitive(primitive);

    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x < TESTW/2 && y >= TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_stored_immediate_reverse_order)
{
    int primitive1, primitive2;
    uint8_t *pixels;

    const float px = -1 + 0.5f/TESTW, py = -1 + 0.5f/TESTH;
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){px,py,0}, NULL, NULL));
    CHECK_TRUE(primitive1 = graphics_end_primitive());

    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,0,0}, NULL, NULL));
    CHECK_TRUE(primitive2 = graphics_end_primitive());

    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 0,0,0,255, (i/4) % TESTW, (i/4) / TESTW);
    }
    mem_free(pixels);

    graphics_draw_primitive(primitive2);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x < TESTW/2 && y >= TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    graphics_draw_primitive(primitive1);
    ASSERT(pixels = grab_display());
    CHECK_PIXEL(&pixels[0], 255,255,255,255, 0, 0);
    for (int i = 4; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x < TESTW/2 && y >= TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    graphics_destroy_primitive(primitive1);
    graphics_destroy_primitive(primitive2);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_primitive)
{
    int primitive1, primitive2;
    uint8_t *pixels;

    CHECK_TRUE(primitive1 = graphics_create_primitive(
                   GRAPHICS_PRIMITIVE_POINTS,
                   point_vertices, basic_vertex_format,
                   sizeof(*point_vertices), lenof(point_vertices)));
    CHECK_TRUE(primitive2 = graphics_create_primitive(
                   GRAPHICS_PRIMITIVE_QUADS,
                   quad_vertices, basic_vertex_format,
                   sizeof(*quad_vertices), lenof(quad_vertices)));

    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 0,0,0,255, (i/4) % TESTW, (i/4) / TESTW);
    }
    mem_free(pixels);

    graphics_draw_primitive(primitive2);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x < TESTW/2 && y >= TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    graphics_draw_primitive(primitive1);
    ASSERT(pixels = grab_display());
    CHECK_PIXEL(&pixels[0], 255,255,255,255, 0, 0);
    for (int i = 4; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x < TESTW/2 && y >= TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    graphics_destroy_primitive(primitive1);
    graphics_destroy_primitive(primitive2);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_indexed_primitive)
{
    int primitive1, primitive2;
    uint8_t *pixels;

    CHECK_TRUE(primitive1 = graphics_create_indexed_primitive(
                   GRAPHICS_PRIMITIVE_POINTS, point_indexed_vertices,
                   basic_vertex_format, sizeof(*point_indexed_vertices),
                   lenof(point_indexed_vertices), point_indices,
                   sizeof(*point_indices), lenof(point_indices)));
    CHECK_TRUE(primitive2 = graphics_create_indexed_primitive(
                   GRAPHICS_PRIMITIVE_QUADS, quad_indexed_vertices,
                   basic_vertex_format, sizeof(*quad_indexed_vertices),
                   lenof(quad_indexed_vertices), quad_indices,
                   sizeof(*quad_indices), lenof(quad_indices)));

    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 0,0,0,255, (i/4) % TESTW, (i/4) / TESTW);
    }
    mem_free(pixels);

    graphics_draw_primitive(primitive1);
    ASSERT(pixels = grab_display());
    CHECK_PIXEL(&pixels[0], 255,255,255,255, 0, 0);
    for (int i = 4; i < TESTW*TESTH*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 0,0,0,255, (i/4) % TESTW, (i/4) / TESTW);
    }
    mem_free(pixels);

    graphics_draw_primitive(primitive2);
    ASSERT(pixels = grab_display());
    CHECK_PIXEL(&pixels[0], 255,255,255,255, 0, 0);
    for (int i = 4; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x < TESTW/2 && y >= TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    graphics_destroy_primitive(primitive1);
    graphics_destroy_primitive(primitive2);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_cleanup_destroys_primitives)
{
    int primitive;

    const float px = -1 + 0.5f/TESTW, py = -1 + 0.5f/TESTH;
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){px,py,0}, NULL, NULL));
    CHECK_TRUE(primitive = graphics_end_primitive());

    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){px,py,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_primitive()); //To be freed by primitive_cleanup().

    /* These don't return values; we use the memory leak check to determine
     * whether the test has passed. */
    graphics_destroy_primitive(primitive);
    primitive_cleanup();

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_primitive_array_hole)
{
    int primitive1, primitive2, primitive3;

    CHECK_TRUE(primitive1 = graphics_create_primitive(
                   GRAPHICS_PRIMITIVE_POINTS,
                   point_vertices, basic_vertex_format,
                   sizeof(*point_vertices), lenof(point_vertices)));
    CHECK_TRUE(primitive2 = graphics_create_primitive(
                   GRAPHICS_PRIMITIVE_QUADS,
                   quad_vertices, basic_vertex_format,
                   sizeof(*quad_vertices), lenof(quad_vertices)));
    CHECK_TRUE(primitive2 > primitive1);

    graphics_destroy_primitive(primitive1);
    /* Deliberately == (comparison) instead of = (assignment), because
     * this should reuse the ID we freed above. */
    CHECK_TRUE(primitive1 == graphics_create_primitive(
                   GRAPHICS_PRIMITIVE_POINTS,
                   point_vertices, basic_vertex_format,
                   sizeof(*point_vertices), lenof(point_vertices)));
    CHECK_TRUE(primitive3 = graphics_create_primitive(
                   GRAPHICS_PRIMITIVE_POINTS,
                   point_vertices, basic_vertex_format,
                   sizeof(*point_vertices), lenof(point_vertices)));
    CHECK_TRUE(primitive3 > primitive2);

    graphics_draw_primitive(primitive1);
    graphics_draw_primitive(primitive2);
    uint8_t *pixels;
    ASSERT(pixels = grab_display());
    CHECK_PIXEL(&pixels[0], 255,255,255,255, 0, 0);
    for (int i = 4; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x < TESTW/2 && y >= TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    graphics_destroy_primitive(primitive1);
    graphics_destroy_primitive(primitive2);
    graphics_destroy_primitive(primitive3);
    return 1;
}

/*************************************************************************/
/******************** Partial primitive drawing tests ********************/
/*************************************************************************/

TEST(test_point_partial)
{
    int primitive;
    uint8_t *pixels;

    const float px = -1 + 0.5f/TESTW, py = -1 + 0.5f/TESTH;
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){px,py,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){px+0.5f,py+0.5f,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){px+1,py+1,0}, NULL, NULL));
    CHECK_TRUE(primitive = graphics_end_primitive());

    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_draw_primitive_partial(primitive, 0, -1);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = ((x==0 && y==0)
                       || (x==TESTW/4 && y==TESTH/4)
                       || (x==TESTW/2 && y==TESTH/2)) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_draw_primitive_partial(primitive, 1, 1);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x==TESTW/4 && y==TESTH/4) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* The draw count should be truncated to the number of vertices
     * remaining. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_draw_primitive_partial(primitive, 2, 2);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x==TESTW/2 && y==TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* An out-of-range starting vertex or zero count should cause nothing
     * to be drawn. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_draw_primitive_partial(primitive, 3, 1);
    graphics_draw_primitive_partial(primitive, 0, 0);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        CHECK_PIXEL(&pixels[i], 0,0,0,255, x, y);
    }
    mem_free(pixels);

    graphics_destroy_primitive(primitive);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_quad_partial)
{
    int primitive;
    uint8_t *pixels;

    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,-0.5,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0.5,-0.5,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0.5,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0.5,-0.5,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0.5,-1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){1,-1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){1,-0.5,0}, NULL, NULL));
    CHECK_TRUE(primitive = graphics_end_primitive());

    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_draw_primitive_partial(primitive, 0, -1);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = ((x < TESTW/2 && y >= TESTH/2)
                       || (x >= TESTW/2 && x < TESTW*3/4
                           && y >= TESTH/4 && y < TESTH/2)
                       || (x >= TESTW*3/4 && y < TESTH/4)) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_draw_primitive_partial(primitive, 4, 4);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x >= TESTW/2 && x < TESTW*3/4
                       && y >= TESTH/4 && y < TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_draw_primitive_partial(primitive, 8, 8);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x >= TESTW*3/4 && y < TESTH/4) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_draw_primitive_partial(primitive, 12, 4);
    graphics_draw_primitive_partial(primitive, 0, 0);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        CHECK_PIXEL(&pixels[i], 0,0,0,255, x, y);
    }
    mem_free(pixels);

    /* Incomplete primitives should be truncated. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_draw_primitive_partial(primitive, 4, 7);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x >= TESTW/2 && x < TESTW*3/4
                       && y >= TESTH/4 && y < TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    graphics_destroy_primitive(primitive);
    return 1;
}

/*************************************************************************/
/***************** Vertex buffer immediate drawing tests *****************/
/*************************************************************************/

TEST(test_draw_vertices)
{
    uint8_t *pixels;

    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, quad_vertices, basic_vertex_format,
        sizeof(*quad_vertices), lenof(quad_vertices));
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x < TESTW/2 && y >= TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_POINTS, point_vertices, basic_vertex_format,
        sizeof(*point_vertices), lenof(point_vertices));
    ASSERT(pixels = grab_display());
    CHECK_PIXEL(&pixels[0], 255,255,255,255, 0, 0);
    for (int i = 4; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x < TESTW/2 && y >= TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_draw_indexed_vertices)
{
    uint8_t *pixels;

    graphics_draw_indexed_vertices(
        GRAPHICS_PRIMITIVE_QUADS, quad_indexed_vertices, basic_vertex_format,
        sizeof(*quad_indexed_vertices), lenof(quad_indexed_vertices),
        quad_indices, sizeof(*quad_indices), lenof(quad_indices));
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x < TESTW/2 && y >= TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    graphics_draw_indexed_vertices(
        GRAPHICS_PRIMITIVE_POINTS, point_indexed_vertices, basic_vertex_format,
        sizeof(*point_indexed_vertices), lenof(point_indexed_vertices),
        point_indices, sizeof(*point_indices), lenof(point_indices));
    ASSERT(pixels = grab_display());
    CHECK_PIXEL(&pixels[0], 255,255,255,255, 0, 0);
    for (int i = 4; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x < TESTW/2 && y >= TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    return 1;
}

/*************************************************************************/
/**************************** Texturing tests ****************************/
/*************************************************************************/

TEST(test_solid_texture)
{
    int texture;
    CHECK_TRUE(texture = texture_create_with_data(
                   1, 1, "\x33\x66\x99\xFF", TEX_FORMAT_RGBA8888, 2, 0, 0));
    texture_unlock(texture);
    texture_apply(0, texture);

    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, tex_quad_vertices, textured_vertex_format,
        sizeof(*tex_quad_vertices), lenof(tex_quad_vertices));
    uint8_t *pixels;
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int r = (x < TESTW/2 && y >= TESTH/2) ? 0x33 : 0;
        const int g = (x < TESTW/2 && y >= TESTH/2) ? 0x66 : 0;
        const int b = (x < TESTW/2 && y >= TESTH/2) ? 0x99 : 0;
        CHECK_PIXEL(&pixels[i], r,g,b,255, x, y);
    }
    mem_free(pixels);

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_alpha_texture)
{
    int texture;
    CHECK_TRUE(texture = texture_create_with_data(
                   1, 1, "\x33\x66\x99\xAA", TEX_FORMAT_RGBA8888, 1, 0, 0));
    texture_apply(0, texture);

    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, tex_quad_vertices, textured_vertex_format,
        sizeof(*tex_quad_vertices), lenof(tex_quad_vertices));
    uint8_t *pixels;
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int r = (x < TESTW/2 && y >= TESTH/2) ? 0x22 : 0;
        const int g = (x < TESTW/2 && y >= TESTH/2) ? 0x44 : 0;
        const int b = (x < TESTW/2 && y >= TESTH/2) ? 0x66 : 0;
        CHECK_PIXEL_NEAR(&pixels[i], r,g,b,255, 1, x, y);
    }
    mem_free(pixels);

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_alpha_only_texture)
{
    static const ALIGNED(4) uint8_t alpha_tex_data[] = {
        'T','E','X', 10,  2, 64,  0,  0,  0,  1,  0,  1,  0,  1,  0,  0,
          0,  0,  0, 32,  0,  0,  0,  1,  0,  0,  0,  0,  0,  0,  0,  0,
        170,
    };
    int texture;
    /* Safe to de-const the input buffer since we're not reusing it.  Note
     * that we don't assert success here because we haven't tested alpha
     * texture support yet (that's in graphics_texture_formats which
     * indirectly depends on this set of tests). */
    CHECK_TRUE(texture = texture_parse((void *)alpha_tex_data,
                                       sizeof(alpha_tex_data), 0, 0, 0));
    texture_apply(0, texture);

    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, tex_quad_vertices, textured_vertex_format,
        sizeof(*tex_quad_vertices), lenof(tex_quad_vertices));
    uint8_t *pixels;
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x < TESTW/2 && y >= TESTH/2) ? 0xAA : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_texture_antialias)
{
    /* Note that wraparound is enabled by default, so we mirror the texture
     * to get a solid border. */
    static const uint8_t texture_data[] = {
        0x40,0x80,0xC0,0xFF, 0x50,0x90,0xD0,0xFF, 0x50,0x90,0xD0,0xFF, 0x40,0x80,0xC0,0xFF,
        0x60,0xA0,0xE0,0xFF, 0x70,0xB0,0xF0,0xFF, 0x70,0xB0,0xF0,0xFF, 0x60,0xA0,0xE0,0xFF,
        0x60,0xA0,0xE0,0xFF, 0x70,0xB0,0xF0,0xFF, 0x70,0xB0,0xF0,0xFF, 0x60,0xA0,0xE0,0xFF,
        0x40,0x80,0xC0,0xFF, 0x50,0x90,0xD0,0xFF, 0x50,0x90,0xD0,0xFF, 0x40,0x80,0xC0,0xFF,
    };
    int texture;
    CHECK_TRUE(texture = texture_create_with_data(
                   4, 4, texture_data, TEX_FORMAT_RGBA8888, 4, 0, 0));
    texture_apply(0, texture);

    /* For this test, we draw a quad of exactly 64 pixels in each dimension,
     * resulting in an expansion by 16x (which we can easily check because
     * the RGB values are all multiples of 16).  We also shift the texture
     * coordinates slightly to adjust for differing antialias algorithms on
     * different systems, to ensure that all systems give us the same output
     * values. */
    const float x0 = -1, y0 = -1;
    const float x1 = -1 + 2*(64.0f/TESTW);
    const float y1 = -1 + 2*(64.0f/TESTH);
#ifdef SIL_PLATFORM_PSP
    const float u0 = 0, v0 = 0;
#else
    const float u0 = -0.5f/64, v0 = -0.5f/64;
#endif
    const float u1 = u0+1, v1 = v0+1;
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y0,0},
                                   &(Vector2f){u0,v0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y1,0},
                                   &(Vector2f){u0,v1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y1,0},
                                   &(Vector2f){u1,v1}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y0,0},
                                   &(Vector2f){u1,v0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    uint8_t *pixels;
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        int r, g, b;
        if (x < 64 && y < 64) {
            int p = 0;
            if (x >= 24 && x < 40) {
                p += 0x10;
            } else if (x >= 8 && x < 24) {
                p += x - 8;
            } else if (x >= 40 && x < 56) {
                p += 56 - x;
            }
            if (y >= 24 && y < 40) {
                p += 0x20;
            } else if (y >= 8 && y < 24) {
                p += 2 * (y - 8);
            } else if (y >= 40 && y < 56) {
                p += 2 * (56 - y);
            }
            r = p | 0x40;
            g = p | 0x80;
            b = p | 0xC0;
        } else {
            r = g = b = 0;
        }
        CHECK_PIXEL(&pixels[i], r,g,b,255, x, y);
    }
    mem_free(pixels);

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_texture_set_antialias)
{
    static const uint8_t texture_data[] = {
        0x40,0x80,0xC0,0xFF, 0x50,0x90,0xD0,0xFF,
        0x60,0xA0,0xE0,0xFF, 0x70,0xB0,0xF0,0xFF,
    };
    int texture;
    CHECK_TRUE(texture = texture_create_with_data(
                   2, 2, texture_data, TEX_FORMAT_RGBA8888, 2, 0, 0));
    texture_apply(0, texture);

    uint8_t *pixels;

    /* On -> off and off -> off transition. */
    for (int try = 0; try < 2; try++) {
        texture_set_antialias(texture, 0);
        graphics_draw_vertices(
            GRAPHICS_PRIMITIVE_QUADS, tex_quad_vertices, textured_vertex_format,
            sizeof(*tex_quad_vertices), lenof(tex_quad_vertices));
        ASSERT(pixels = grab_display());
        for (int i = 0; i < TESTW*TESTH*4; i += 4) {
            const int x = (i/4) % TESTW;
            const int y = (i/4) / TESTW;
            const int p = (x < TESTW/4 ? 0 : 0x10) | (y < TESTH*3/4 ? 0 : 0x20);
            const int r = (x < TESTW/2 && y >= TESTH/2) ? p|0x40 : 0;
            const int g = (x < TESTW/2 && y >= TESTH/2) ? p|0x80 : 0;
            const int b = (x < TESTW/2 && y >= TESTH/2) ? p|0xC0 : 0;
            CHECK_PIXEL(&pixels[i], r,g,b,255, x, y);
        }
        mem_free(pixels);
    }

    /* Off -> on and on -> on transition. */
    for (int try = 0; try < 2; try++) {
        texture_set_antialias(texture, 1);
        graphics_draw_vertices(
            GRAPHICS_PRIMITIVE_QUADS, tex_quad_vertices, textured_vertex_format,
            sizeof(*tex_quad_vertices), lenof(tex_quad_vertices));
        ASSERT(pixels = grab_display());
        /* We've already checked detailed antialiasing behavior, so just check
         * here that antialiasing is in fact being performed. */
        CHECK_INTRANGE(pixels[((TESTH*3/4)*TESTW + TESTW/4) * 4 + 0],
                       0x50, 0x60);
        CHECK_INTRANGE(pixels[((TESTH*3/4)*TESTW + TESTW/4) * 4 + 1],
                       0x90, 0xA0);
        CHECK_INTRANGE(pixels[((TESTH*3/4)*TESTW + TESTW/4) * 4 + 2],
                       0xD0, 0xE0);
        CHECK_INTEQUAL(pixels[((TESTH*3/4)*TESTW + TESTW/4) * 4 + 3], 255);
        mem_free(pixels);
    }

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_texture_set_antialias_readonly)
{
    static const uint8_t texture_data[] = {
        0x40,0x80,0xC0,0xFF, 0x50,0x90,0xD0,0xFF,
        0x60,0xA0,0xE0,0xFF, 0x70,0xB0,0xF0,0xFF,
    };
    /* Safe to de-const the input buffer since we're not reusing it. */
    SysTexture *systex;
    ASSERT(systex = sys_texture_create(
               2, 2, TEX_FORMAT_RGBA8888, 1, (void *)texture_data, 2,
               (int32_t[]){0}, (int32_t[]){sizeof(texture_data)}, 0, 0, 0));
    sys_texture_set_antialias(systex, 0);
    int texture;
    ASSERT(texture = texture_import_readonly(systex));
    texture_apply(0, texture);

    uint8_t *pixels;

    /* Make sure the texture was correctly set to non-antialiased. */
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, tex_quad_vertices, textured_vertex_format,
        sizeof(*tex_quad_vertices), lenof(tex_quad_vertices));
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x < TESTW/4 ? 0 : 0x10) | (y < TESTH*3/4 ? 0 : 0x20);
        const int r = (x < TESTW/2 && y >= TESTH/2) ? p|0x40 : 0;
        const int g = (x < TESTW/2 && y >= TESTH/2) ? p|0x80 : 0;
        const int b = (x < TESTW/2 && y >= TESTH/2) ? p|0xC0 : 0;
        CHECK_PIXEL(&pixels[i], r,g,b,255, x, y);
    }
    mem_free(pixels);

    /* This should fail, leaving the texture non-antialiased. */
    texture_set_antialias(texture, 1);

    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, tex_quad_vertices, textured_vertex_format,
        sizeof(*tex_quad_vertices), lenof(tex_quad_vertices));
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x < TESTW/4 ? 0 : 0x10) | (y < TESTH*3/4 ? 0 : 0x20);
        const int r = (x < TESTW/2 && y >= TESTH/2) ? p|0x40 : 0;
        const int g = (x < TESTW/2 && y >= TESTH/2) ? p|0x80 : 0;
        const int b = (x < TESTW/2 && y >= TESTH/2) ? p|0xC0 : 0;
        CHECK_PIXEL(&pixels[i], r,g,b,255, x, y);
    }
    mem_free(pixels);

    texture_forget_readonly(texture);
    sys_texture_destroy(systex);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_texture_set_repeat)
{
    static const uint8_t texture_data[] = {
        0x40,0x80,0xC0,0xFF, 0x50,0x90,0xD0,0xFF,
        0x60,0xA0,0xE0,0xFF, 0x70,0xB0,0xF0,0xFF,
    };
    int texture;
    CHECK_TRUE(texture = texture_create_with_data(
                   2, 2, texture_data, TEX_FORMAT_RGBA8888, 2, 0, 0));
    texture_set_antialias(texture, 0);
    texture_apply(0, texture);

    static const TexturedVertex vertices[] = {
        {-1,0,0,0,0}, {-1,1,0,0,2}, {0,1,0,2,2}, {0,0,0,2,0}
    };

    uint8_t *pixels;

    /* Check the default state first (U+V repeat). */
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, vertices, textured_vertex_format,
        sizeof(*vertices), lenof(vertices));
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int x8 = x / (TESTW/8);
        const int y8 = y / (TESTH/8);
        const int p = (x8==1 || x8==3 ? 0x10 : 0) | (y8==5 || y8==7 ? 0x20 : 0);
        const int r = (x < TESTW/2 && y >= TESTH/2) ? p|0x40 : 0;
        const int g = (x < TESTW/2 && y >= TESTH/2) ? p|0x80 : 0;
        const int b = (x < TESTW/2 && y >= TESTH/2) ? p|0xC0 : 0;
        CHECK_PIXEL(&pixels[i], r,g,b,255, x, y);
    }
    mem_free(pixels);

    texture_set_repeat(texture, 0, 1);
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, vertices, textured_vertex_format,
        sizeof(*vertices), lenof(vertices));
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int x8 = x / (TESTW/8);
        const int y8 = y / (TESTH/8);
        const int p = (x8>=1 && x8<=3 ? 0x10 : 0) | (y8==5 || y8==7 ? 0x20 : 0);
        const int r = (x < TESTW/2 && y >= TESTH/2) ? p|0x40 : 0;
        const int g = (x < TESTW/2 && y >= TESTH/2) ? p|0x80 : 0;
        const int b = (x < TESTW/2 && y >= TESTH/2) ? p|0xC0 : 0;
        CHECK_PIXEL(&pixels[i], r,g,b,255, x, y);
    }
    mem_free(pixels);

    texture_set_repeat(texture, 1, 0);
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, vertices, textured_vertex_format,
        sizeof(*vertices), lenof(vertices));
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int x8 = x / (TESTW/8);
        const int y8 = y / (TESTH/8);
        const int p = (x8==1 || x8==3 ? 0x10 : 0) | (y8>=5 && y8<=7 ? 0x20 : 0);
        const int r = (x < TESTW/2 && y >= TESTH/2) ? p|0x40 : 0;
        const int g = (x < TESTW/2 && y >= TESTH/2) ? p|0x80 : 0;
        const int b = (x < TESTW/2 && y >= TESTH/2) ? p|0xC0 : 0;
        CHECK_PIXEL(&pixels[i], r,g,b,255, x, y);
    }
    mem_free(pixels);

    texture_set_repeat(texture, 0, 0);
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, vertices, textured_vertex_format,
        sizeof(*vertices), lenof(vertices));
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int x8 = x / (TESTW/8);
        const int y8 = y / (TESTH/8);
        const int p = (x8>=1 && x8<=3 ? 0x10 : 0) | (y8>=5 && y8<=7 ? 0x20 : 0);
        const int r = (x < TESTW/2 && y >= TESTH/2) ? p|0x40 : 0;
        const int g = (x < TESTW/2 && y >= TESTH/2) ? p|0x80 : 0;
        const int b = (x < TESTW/2 && y >= TESTH/2) ? p|0xC0 : 0;
        CHECK_PIXEL(&pixels[i], r,g,b,255, x, y);
    }
    mem_free(pixels);

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_texture_set_repeat_readonly)
{
    static const uint8_t texture_data[] = {
        0x40,0x80,0xC0,0xFF, 0x50,0x90,0xD0,0xFF,
        0x60,0xA0,0xE0,0xFF, 0x70,0xB0,0xF0,0xFF,
    };
    /* Safe to de-const the input buffer since we're not reusing it. */
    SysTexture *systex;
    ASSERT(systex = sys_texture_create(
               2, 2, TEX_FORMAT_RGBA8888, 1, (void *)texture_data, 2,
               (int32_t[]){0}, (int32_t[]){sizeof(texture_data)}, 0, 0, 0));
    sys_texture_set_antialias(systex, 0);
    sys_texture_set_repeat(systex, 0, 0);
    int texture;
    ASSERT(texture = texture_import_readonly(systex));
    texture_apply(0, texture);

    static const TexturedVertex vertices[] = {
        {-1,0,0,0,0}, {-1,1,0,0,2}, {0,1,0,2,2}, {0,0,0,2,0}
    };

    uint8_t *pixels;

    /* Make sure the texture was correctly set to non-repeating. */
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, vertices, textured_vertex_format,
        sizeof(*vertices), lenof(vertices));
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int x8 = x / (TESTW/8);
        const int y8 = y / (TESTH/8);
        const int p = (x8>=1 && x8<=3 ? 0x10 : 0) | (y8>=5 && y8<=7 ? 0x20 : 0);
        const int r = (x < TESTW/2 && y >= TESTH/2) ? p|0x40 : 0;
        const int g = (x < TESTW/2 && y >= TESTH/2) ? p|0x80 : 0;
        const int b = (x < TESTW/2 && y >= TESTH/2) ? p|0xC0 : 0;
        CHECK_PIXEL(&pixels[i], r,g,b,255, x, y);
    }
    mem_free(pixels);

    /* This should fail, leaving the texture non-repeating. */
    texture_set_repeat(texture, 1, 1);
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, vertices, textured_vertex_format,
        sizeof(*vertices), lenof(vertices));
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int x8 = x / (TESTW/8);
        const int y8 = y / (TESTH/8);
        const int p = (x8>=1 && x8<=3 ? 0x10 : 0) | (y8>=5 && y8<=7 ? 0x20 : 0);
        const int r = (x < TESTW/2 && y >= TESTH/2) ? p|0x40 : 0;
        const int g = (x < TESTW/2 && y >= TESTH/2) ? p|0x80 : 0;
        const int b = (x < TESTW/2 && y >= TESTH/2) ? p|0xC0 : 0;
        CHECK_PIXEL(&pixels[i], r,g,b,255, x, y);
    }
    mem_free(pixels);

    texture_forget_readonly(texture);
    sys_texture_destroy(systex);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_texture_npot)
{
    static const uint8_t texture_data[] = {
        0x33,0x66,0x99,0xFF, 0x33,0x66,0x99,0xFF, 0x33,0x66,0x99,0xFF,
        0x33,0x66,0x99,0xFF, 0x33,0x66,0x99,0xFF, 0x33,0x66,0x99,0xFF,
        0x33,0x66,0x99,0xFF, 0x33,0x66,0x99,0xFF, 0x33,0x66,0x99,0xFF,
        0x33,0x66,0x99,0xFF, 0x33,0x66,0x99,0xFF, 0x33,0x66,0x99,0xFF,
        0x33,0x66,0x99,0xFF, 0x33,0x66,0x99,0xFF, 0x33,0x66,0x99,0xFF,
    };
    int texture;
    CHECK_TRUE(texture = texture_create_with_data(
                   3, 5, texture_data, TEX_FORMAT_RGBA8888, 3, 0, 0));
#if defined(SIL_OPENGL_ES) || defined(SIL_PLATFORM_PSP)
    /* OpenGL ES and the PSP don't support wraparound for NPOT textures. */
    texture_set_repeat(texture, 0, 0);
#endif
#if defined(SIL_PLATFORM_PSP)
    /* The PSP also doesn't support antialiasing (because the hardware
     * treats it as a power-of-two texture and we get leakage from the
     * borders). */
    texture_set_antialias(texture, 0);
#endif
    texture_apply(0, texture);

    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, tex_quad_vertices, textured_vertex_format,
        sizeof(*tex_quad_vertices), lenof(tex_quad_vertices));
    uint8_t *pixels;
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int r = (x < TESTW/2 && y >= TESTH/2) ? 0x33 : 0;
        const int g = (x < TESTW/2 && y >= TESTH/2) ? 0x66 : 0;
        const int b = (x < TESTW/2 && y >= TESTH/2) ? 0x99 : 0;
        CHECK_PIXEL(&pixels[i], r,g,b,255, x, y);
    }
    mem_free(pixels);

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_texture_mipmaps)
{
    int texture;
    /* Safe to de-const the input buffer since we're not reusing it. */
    CHECK_TRUE(texture = texture_parse((void *)tex_data_mipmaps,
                                       sizeof(tex_data_mipmaps), 0, 1, 0));
    texture_apply(0, texture);

    const float x0 = -1, y0 = -1;
    const float x1 = -1 + 2*(4.0f/TESTW);
    const float y1 = -1 + 2*(4.0f/TESTH);
#if defined(SIL_PLATFORM_PSP)
    /* We need slightly offset constants to get the desired exact output. */
    const float u_base = 1.0625f, v_base = 1.0625f;
#else
    const float u_base = 1, v_base = 1;
#endif
    uint8_t *pixels;

    /* Check that all mipmap levels are selected properly. */

    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y0,0},
                                   &(Vector2f){0,0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y1,0},
                                   &(Vector2f){0,v_base}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y1,0},
                                   &(Vector2f){u_base,v_base}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y0,0},
                                   &(Vector2f){u_base,0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x < 4 && y < 4) ? 20 : 0;
        CHECK_PIXEL(&pixels[i], p,0,0,255, x, y);
    }
    mem_free(pixels);

    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y0,0},
                                   &(Vector2f){0,0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y1,0},
                                   &(Vector2f){0,2*v_base}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y1,0},
                                   &(Vector2f){2*u_base,2*v_base}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y0,0},
                                   &(Vector2f){2*u_base,0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x < 4 && y < 4) ? 20 : 0;
        CHECK_PIXEL(&pixels[i], 0,p,0,255, x, y);
    }
    mem_free(pixels);

    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y0,0},
                                   &(Vector2f){0,0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y1,0},
                                   &(Vector2f){0,4*v_base}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y1,0},
                                   &(Vector2f){4*u_base,4*v_base}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y0,0},
                                   &(Vector2f){4*u_base,0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x < 4 && y < 4) ? 20 : 0;
        CHECK_PIXEL(&pixels[i], 0,0,p,255, x, y);
    }
    mem_free(pixels);

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_texture_mipmaps_limit)
{
    int texture;
    /* Safe to de-const the input buffer since we're not reusing it. */
    CHECK_TRUE(texture = texture_parse((void *)tex_data_mipmaps,
                                       sizeof(tex_data_mipmaps), 0, 1, 0));
    texture_apply(0, texture);

    const float x0 = -1, y0 = -1;
    const float x1 = -1 + 2*(4.0f/TESTW);
    const float y1 = -1 + 2*(4.0f/TESTH);
#if defined(SIL_PLATFORM_PSP)
    const float u_base = 1.0625f, v_base = 1.0625f;
#else
    const float u_base = 1, v_base = 1;
#endif
    uint8_t *pixels;

    /* Check that level of detail is capped at the mipmap level bounds. */

    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y0,0},
                                   &(Vector2f){0,0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y1,0},
                                   &(Vector2f){0,0.5f*v_base}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y1,0},
                                   &(Vector2f){0.5f*u_base,0.5f*v_base}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y0,0},
                                   &(Vector2f){0.5f*u_base,0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x < 4 && y < 4) ? 20 : 0;
        CHECK_PIXEL(&pixels[i], p,0,0,255, x, y);
    }
    mem_free(pixels);

    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y0,0},
                                   &(Vector2f){0,0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y1,0},
                                   &(Vector2f){0,8*v_base}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y1,0},
                                   &(Vector2f){8*u_base,8*v_base}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y0,0},
                                   &(Vector2f){8*u_base,0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x < 4 && y < 4) ? 20 : 0;
        CHECK_PIXEL(&pixels[i], 0,0,p,255, x, y);
    }
    mem_free(pixels);

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_texture_mipmaps_antialias)
{
    int texture;
    /* Safe to de-const the input buffer since we're not reusing it. */
    CHECK_TRUE(texture = texture_parse((void *)tex_data_mipmaps,
                                       sizeof(tex_data_mipmaps), 0, 1, 0));
    texture_apply(0, texture);

    const float x0 = -1, y0 = -1;
    const float x1 = -1 + 2*(4.0f/TESTW);
    const float y1 = -1 + 2*(4.0f/TESTH);
#if defined(SIL_PLATFORM_PSP)
    const float u_base = 1.0625f, v_base = 1.0625f;
#else
    const float u_base = 1, v_base = 1;
#endif
    uint8_t *pixels;

    /* Check that mipmap levels are blended properly. */

    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y0,0},
                                   &(Vector2f){0,0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y1,0},
                                   &(Vector2f){0,1.4f*v_base}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y1,0},
                                   &(Vector2f){1.4f*u_base,1.4f*v_base}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y0,0},
                                   &(Vector2f){1.4f*u_base,0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        if (x < 4 && y < 4) {
            if ((pixels[i+0] == 0 || pixels[i+0] >= 20)
             || (pixels[i+1] == 0 || pixels[i+1] >= 20)
             || pixels[i+2] != 0
             || pixels[i+3] != 255) {
                FAIL("Pixel (%d,%d) was RGBA (%u,%u,%u,%u) but should have"
                     " been between (%u,%u,%u,%u) and (%u,%u,%u,%u)", x, y,
                     pixels[i+0], pixels[i+1], pixels[i+2], pixels[i+3],
                     0, 0, 0, 255, 20, 20, 0, 255);
            }
        } else {
            CHECK_PIXEL(&pixels[i], 0,0,0,255, x, y);
        }
    }
    mem_free(pixels);

    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y0,0},
                                   &(Vector2f){0,0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y1,0},
                                   &(Vector2f){0,2.8f*v_base}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y1,0},
                                   &(Vector2f){2.8f*u_base,2.8f*v_base}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y0,0},
                                   &(Vector2f){2.8f*u_base,0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        if (x < 4 && y < 4) {
            if (pixels[i+0] != 0
             || (pixels[i+1] == 0 || pixels[i+1] >= 20)
             || (pixels[i+2] == 0 || pixels[i+2] >= 20)
             || pixels[i+3] != 255) {
                FAIL("Pixel (%d,%d) was RGBA (%u,%u,%u,%u) but should have"
                     " been between (%u,%u,%u,%u) and (%u,%u,%u,%u)", x, y,
                     pixels[i+0], pixels[i+1], pixels[i+2], pixels[i+3],
                     0, 0, 0, 255, 0, 20, 20, 255);
            }
        } else {
            CHECK_PIXEL(&pixels[i], 0,0,0,255, x, y);
        }
    }
    mem_free(pixels);

    /* Check that if antialiasing is disabled, mipmap blending is also
     * disabled (but mipmaps themselves are still used). */

    texture_set_antialias(texture, 0);

    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y0,0},
                                   &(Vector2f){0,0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y1,0},
                                   &(Vector2f){0,1.4f*v_base}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y1,0},
                                   &(Vector2f){1.4f*u_base,1.4f*v_base}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y0,0},
                                   &(Vector2f){1.4f*u_base,0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x < 4 && y < 4) ? 20 : 0;
        CHECK_PIXEL(&pixels[i], p,0,0,255, x, y);
    }
    mem_free(pixels);

    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y0,0},
                                   &(Vector2f){0,0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y1,0},
                                   &(Vector2f){0,2.8f*v_base}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y1,0},
                                   &(Vector2f){2.8f*u_base,2.8f*v_base}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y0,0},
                                   &(Vector2f){2.8f*u_base,0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x < 4 && y < 4) ? 20 : 0;
        CHECK_PIXEL(&pixels[i], 0,p,0,255, x, y);
    }
    mem_free(pixels);

    /* Check that if antialiasing is re-enabled, mipmap blending is also
     * re-enabled. */

    texture_set_antialias(texture, 1);

    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y0,0},
                                   &(Vector2f){0,0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y1,0},
                                   &(Vector2f){0,1.4f*v_base}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y1,0},
                                   &(Vector2f){1.4f*u_base,1.4f*v_base}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y0,0},
                                   &(Vector2f){1.4f*u_base,0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        if (x < 4 && y < 4) {
            if ((pixels[i+0] == 0 || pixels[i+0] >= 20)
             || (pixels[i+1] == 0 || pixels[i+1] >= 20)
             || pixels[i+2] != 0
             || pixels[i+3] != 255) {
                FAIL("Pixel (%d,%d) was RGBA (%u,%u,%u,%u) but should have"
                     " been between (%u,%u,%u,%u) and (%u,%u,%u,%u)", x, y,
                     pixels[i+0], pixels[i+1], pixels[i+2], pixels[i+3],
                     0, 0, 0, 255, 20, 20, 0, 255);
            }
        } else {
            CHECK_PIXEL(&pixels[i], 0,0,0,255, x, y);
        }
    }
    mem_free(pixels);

    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y0,0},
                                   &(Vector2f){0,0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y1,0},
                                   &(Vector2f){0,2.8f*v_base}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y1,0},
                                   &(Vector2f){2.8f*u_base,2.8f*v_base}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y0,0},
                                   &(Vector2f){2.8f*u_base,0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        if (x < 4 && y < 4) {
            if (pixels[i+0] != 0
             || (pixels[i+1] == 0 || pixels[i+1] >= 20)
             || (pixels[i+2] == 0 || pixels[i+2] >= 20)
             || pixels[i+3] != 255) {
                FAIL("Pixel (%d,%d) was RGBA (%u,%u,%u,%u) but should have"
                     " been between (%u,%u,%u,%u) and (%u,%u,%u,%u)", x, y,
                     pixels[i+0], pixels[i+1], pixels[i+2], pixels[i+3],
                     0, 0, 0, 255, 0, 20, 20, 255);
            }
        } else {
            CHECK_PIXEL(&pixels[i], 0,0,0,255, x, y);
        }
    }
    mem_free(pixels);

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_texture_mipmaps_16bpp)
{
    int texture;
    /* Safe to de-const the input buffer since we're not reusing it. */
#ifdef SIL_PLATFORM_PSP
    CHECK_TRUE(texture = texture_parse((void *)tex_data_mipmaps_rgb565_psp,
                                       sizeof(tex_data_mipmaps_rgb565_psp),
                                       0, 1, 0));
#else
    CHECK_TRUE(texture = texture_parse((void *)tex_data_mipmaps_bgr565,
                                       sizeof(tex_data_mipmaps_bgr565),
                                       0, 1, 0));
#endif
    texture_apply(0, texture);

    const float x0 = -1, y0 = -1;
    const float x1 = -1 + 2*(2.0f/TESTW);
    const float y1 = -1 + 2*(4.0f/TESTH);
#if defined(SIL_PLATFORM_PSP)
    const float u_base = 1.0625f, v_base = 1.0625f;
#else
    const float u_base = 1, v_base = 1;
#endif
    uint8_t *pixels;

    /* Check that alignment of narrow levels is handled correctly. */

    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y0,0},
                                   &(Vector2f){0,0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y1,0},
                                   &(Vector2f){0,v_base}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y1,0},
                                   &(Vector2f){u_base,v_base}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y0,0},
                                   &(Vector2f){u_base,0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x < 2 && y < 4) ? 8 : 0;
        CHECK_PIXEL(&pixels[i], p,0,0,255, x, y);
    }
    mem_free(pixels);

    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y0,0},
                                   &(Vector2f){0,0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y1,0},
                                   &(Vector2f){0,2*v_base}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y1,0},
                                   &(Vector2f){2*u_base,2*v_base}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y0,0},
                                   &(Vector2f){2*u_base,0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x < 2 && y < 4) ? 8 : 0;
        CHECK_PIXEL(&pixels[i], 0,p,0,255, x, y);
    }
    mem_free(pixels);

    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y0,0},
                                   &(Vector2f){0,0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y1,0},
                                   &(Vector2f){0,4*v_base}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y1,0},
                                   &(Vector2f){4*u_base,4*v_base}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y0,0},
                                   &(Vector2f){4*u_base,0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x < 2 && y < 4) ? 8 : 0;
        CHECK_PIXEL(&pixels[i], 0,0,p,255, x, y);
    }
    mem_free(pixels);

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_texture_mipmaps_8bpp)
{
    int texture;
    /* Safe to de-const the input buffer since we're not reusing it. */
    CHECK_TRUE(texture = texture_parse((void *)tex_data_mipmaps_l8,
                                       sizeof(tex_data_mipmaps_l8), 0, 1, 0));
    texture_apply(0, texture);

    const float x0 = -1, y0 = -1;
    const float x1 = -1 + 2*(2.0f/TESTW);
    const float y1 = -1 + 2*(4.0f/TESTH);
#if defined(SIL_PLATFORM_PSP)
    const float u_base = 1.0625f, v_base = 1.0625f;
#else
    const float u_base = 1, v_base = 1;
#endif
    uint8_t *pixels;

    /* Check that alignment of narrow levels is handled correctly. */

    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y0,0},
                                   &(Vector2f){0,0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y1,0},
                                   &(Vector2f){0,v_base}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y1,0},
                                   &(Vector2f){u_base,v_base}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y0,0},
                                   &(Vector2f){u_base,0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x < 2 && y < 4) ? 64 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y0,0},
                                   &(Vector2f){0,0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y1,0},
                                   &(Vector2f){0,2*v_base}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y1,0},
                                   &(Vector2f){2*u_base,2*v_base}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y0,0},
                                   &(Vector2f){2*u_base,0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x < 2 && y < 4) ? 128 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y0,0},
                                   &(Vector2f){0,0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y1,0},
                                   &(Vector2f){0,4*v_base}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y1,0},
                                   &(Vector2f){4*u_base,4*v_base}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y0,0},
                                   &(Vector2f){4*u_base,0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x < 2 && y < 4) ? 192 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_texture_alloc_clear_with_mipmaps)
{
    if (!auto_mipmaps_supported()) {
        SKIP("Automatic mipmap generation not supported on this platform.");
    }

    int texture;
    CHECK_TRUE(texture = texture_create(2, 4, MEM_ALLOC_CLEAR, 1));
    texture_apply(0, texture);

    graphics_clear(0, 1, 0, 0, 1, 0);
    const float x0 = -1, y0 = -1;
    const float x1 = -1 + 2*(2.0f/TESTW);
    const float y1 = -1 + 2*(4.0f/TESTH);
    uint8_t *pixels;

    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y0,0},
                                   &(Vector2f){0,0}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y1,0},
                                   &(Vector2f){0,4}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y1,0},
                                   &(Vector2f){4,4}, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y0,0},
                                   &(Vector2f){4,0}, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        CHECK_PIXEL(&pixels[i], 0,255,0,255, x, y);
    }
    mem_free(pixels);

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_texcoords_without_texture)
{
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, tex_quad_vertices, textured_vertex_format,
        sizeof(*tex_quad_vertices), lenof(tex_quad_vertices));
    uint8_t *pixels;
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int r = (x < TESTW/2 && y >= TESTH/2) ? 255 : 0;
        const int g = (x < TESTW/2 && y >= TESTH/2) ? 255 : 0;
        const int b = (x < TESTW/2 && y >= TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], r,g,b,255, x, y);
    }
    mem_free(pixels);

    return 1;
}

/*************************************************************************/
/************************* Memory failure tests **************************/
/*************************************************************************/

TEST(test_add_vertex_memory_failure_on_first_expand)
{
    const float px = -1 + 0.5f/TESTW, py = -1 + 0.5f/TESTH;
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    for (int i = 0; i < PRIMITIVE_DATA_EXPAND / (3*4); i++) {
        CHECK_TRUE(graphics_add_vertex(&(Vector3f){px,py,0}, NULL, NULL));
    }
    TEST_mem_fail_after(0, 0, 0);
    CHECK_FALSE(graphics_add_vertex(&(Vector3f){px+1,py,0}, NULL, NULL));
    TEST_mem_fail_after(-1, 0, 0);
    CHECK_FALSE(graphics_add_vertex(&(Vector3f){px,py+1,0}, NULL, NULL));
    CHECK_FALSE(graphics_end_and_draw_primitive());

    uint8_t *pixels;
    ASSERT(pixels = grab_display());
    CHECK_PIXEL(pixels, 0,0,0,255, 0, 0);
    CHECK_PIXEL(&pixels[(TESTW/2)*4], 0,0,0,255, TESTW/2, 0);
    CHECK_PIXEL(&pixels[(TESTH/2)*TESTW*4], 0,0,0,255, 0, TESTH/2);
    mem_free(pixels);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_add_vertex_memory_failure_on_second_expand)
{
    const float px = -1 + 0.5f/TESTW, py = -1 + 0.5f/TESTH;
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    for (int i = 0; i < PRIMITIVE_DATA_EXPAND / (3*4); i++) {
        CHECK_TRUE(graphics_add_vertex(&(Vector3f){px,py,0}, NULL, NULL));
    }
    for (int i = 0; i < 1 + (PRIMITIVE_DATA_EXPAND / (3*4)); i++) {
        CHECK_TRUE(graphics_add_vertex(&(Vector3f){px+1,py,0}, NULL, NULL));
    }
    TEST_mem_fail_after(0, 0, 0);
    CHECK_FALSE(graphics_add_vertex(&(Vector3f){px,py+1,0}, NULL, NULL));
    TEST_mem_fail_after(-1, 0, 0);
    CHECK_FALSE(graphics_add_vertex(&(Vector3f){px,py+1,0}, NULL, NULL));
    CHECK_FALSE(graphics_end_and_draw_primitive());

    uint8_t *pixels;
    ASSERT(pixels = grab_display());
    CHECK_PIXEL(pixels, 0,0,0,255, 0, 0);
    CHECK_PIXEL(&pixels[(TESTW/2)*4], 0,0,0,255, TESTW/2, 0);
    CHECK_PIXEL(&pixels[(TESTH/2)*TESTW*4], 0,0,0,255, 0, TESTH/2);
    mem_free(pixels);

    return 1;
}

/*-----------------------------------------------------------------------*/

#ifdef IMMEDIATE_RENDER_ALLOCS_MEMORY

TEST(test_end_primitive_memory_failure)
{
    const float px = -1 + 0.5f/TESTW, py = -1 + 0.5f/TESTH;
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){px,py,0}, NULL, NULL));
    TEST_mem_fail_after(0, 0, 0);
    CHECK_FALSE(graphics_end_and_draw_primitive());
    TEST_mem_fail_after(-1, 0, 0);

    uint8_t *pixels;
    ASSERT(pixels = grab_display());
    CHECK_PIXEL(pixels, 0,0,0,255, 0, 0);
    mem_free(pixels);

    return 1;
}

#endif  // IMMEDIATE_RENDER_ALLOCS_MEMORY

/*-----------------------------------------------------------------------*/

TEST(test_create_primitive_memory_failure)
{
    int primitive;

    CHECK_MEMORY_FAILURES(primitive = graphics_create_primitive(
                              GRAPHICS_PRIMITIVE_POINTS,
                              point_vertices, basic_vertex_format,
                              sizeof(*point_vertices), lenof(point_vertices)));

    graphics_draw_primitive(primitive);
    uint8_t *pixels;
    ASSERT(pixels = grab_display());
    CHECK_PIXEL(&pixels[0], 255,255,255,255, 0, 0);
    for (int i = 4; i < TESTW*TESTH*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 0,0,0,255, (i/4) % TESTW, (i/4) / TESTW);
    }
    mem_free(pixels);

    graphics_destroy_primitive(primitive);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_indexed_primitive_memory_failure)
{
    int primitive;

    CHECK_MEMORY_FAILURES(primitive = graphics_create_indexed_primitive(
        GRAPHICS_PRIMITIVE_POINTS, point_indexed_vertices,
        basic_vertex_format, sizeof(*point_indexed_vertices),
        lenof(point_indexed_vertices), point_indices,
        sizeof(*point_indices), lenof(point_indices)));

    graphics_draw_primitive(primitive);
    uint8_t *pixels;
    ASSERT(pixels = grab_display());
    CHECK_PIXEL(&pixels[0], 255,255,255,255, 0, 0);
    for (int i = 4; i < TESTW*TESTH*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 0,0,0,255, (i/4) % TESTW, (i/4) / TESTW);
    }
    mem_free(pixels);

    graphics_destroy_primitive(primitive);
    return 1;
}

/*-----------------------------------------------------------------------*/

#ifdef IMMEDIATE_RENDER_ALLOCS_MEMORY

TEST(test_draw_vertices_memory_failure)
{
    /* graphics_draw_[indexed_]vertices() doesn't return a value, so just
     * deny all allocations and check that nothing got drawn (and that the
     * call doesn't crash). */
    TEST_mem_fail_after(0, 1, 0);
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_POINTS, point_vertices, basic_vertex_format,
        sizeof(*point_vertices), lenof(point_vertices));
    TEST_mem_fail_after(-1, 0, 0);
    uint8_t *pixels;
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 0,0,0,255, (i/4) % TESTW, (i/4) / TESTW);
    }
    mem_free(pixels);

    return 1;
}

#endif  // IMMEDIATE_RENDER_ALLOCS_MEMORY

/*-----------------------------------------------------------------------*/

#ifdef IMMEDIATE_RENDER_ALLOCS_MEMORY

TEST(test_draw_indexed_vertices_memory_failure)
{
    TEST_mem_fail_after(0, 1, 0);
    graphics_draw_indexed_vertices(
        GRAPHICS_PRIMITIVE_POINTS, point_indexed_vertices, basic_vertex_format,
        sizeof(*point_indexed_vertices), lenof(point_indexed_vertices),
        point_indices, sizeof(*point_indices), lenof(point_indices));
    TEST_mem_fail_after(-1, 0, 0);
    uint8_t *pixels;
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 0,0,0,255, (i/4) % TESTW, (i/4) / TESTW);
    }
    mem_free(pixels);

    return 1;
}

#endif  // IMMEDIATE_RENDER_ALLOCS_MEMORY

/*************************************************************************/
/************************** Invalid call tests ***************************/
/*************************************************************************/

TEST(test_begin_primitive_double_call)
{
    const float px = -1 + 0.5f/TESTW, py = -1 + 0.5f/TESTH;
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){px,py,0}, NULL, NULL));

    CHECK_FALSE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));

    /* The failure should also abort the current primitive. */
    CHECK_FALSE(graphics_add_vertex(&(Vector3f){-1,-1,0}, NULL, NULL));
    CHECK_FALSE(graphics_end_and_draw_primitive());
    uint8_t *pixels;
    ASSERT(pixels = grab_display());
    CHECK_PIXEL(pixels, 0,0,0,255, 0, 0);
    mem_free(pixels);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_begin_primitive_invalid_type)
{
    CHECK_FALSE(graphics_begin_primitive((GraphicsPrimitiveType)0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_add_vertex_not_in_primitive)
{
    CHECK_FALSE(graphics_add_vertex(&(Vector3f){0,0,0}, NULL, NULL));

    /* The failure should not affect subsequent primitives. */
    const float px = -1 + 0.5f/TESTW, py = -1 + 0.5f/TESTH;
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){px,py,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    uint8_t *pixels;
    ASSERT(pixels = grab_display());
    CHECK_PIXEL(pixels, 255,255,255,255, 0, 0);
    CHECK_PIXEL(&pixels[(TESTW/2)*4], 0,0,0,255, TESTW/2, 0);
    mem_free(pixels);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_add_vertex_missing_position)
{
    const float px = -1 + 0.5f/TESTW, py = -1 + 0.5f/TESTH;
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){px,py,0}, NULL, NULL));
    CHECK_FALSE(graphics_add_vertex(NULL, NULL, NULL));
    CHECK_FALSE(graphics_end_and_draw_primitive());
    uint8_t *pixels;
    ASSERT(pixels = grab_display());
    CHECK_PIXEL(pixels, 0,0,0,255, 0, 0);
    mem_free(pixels);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_add_basic_vertex_format_change)
{
    uint8_t *pixels;

    const float px = -1 + 0.5f/TESTW, py = -1 + 0.5f/TESTH;
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){px,py,0}, NULL, NULL));
    CHECK_FALSE(graphics_add_vertex(&(Vector3f){px+1,py+1,0}, &(Vector2f){0,0},
                                    NULL));
    CHECK_FALSE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    CHECK_PIXEL(pixels, 0,0,0,255, 0, 0);
    mem_free(pixels);

    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){px,py,0}, &(Vector2f){0,0},
                                   NULL));
    CHECK_FALSE(graphics_add_vertex(&(Vector3f){px+1,py+1,0}, NULL, NULL));
    CHECK_FALSE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    CHECK_PIXEL(pixels, 0,0,0,255, 0, 0);
    mem_free(pixels);

    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){px,py,0}, NULL, NULL));
    CHECK_FALSE(graphics_add_vertex(&(Vector3f){px+1,py+1,0}, NULL,
                                    &(Vector4f){1,0,0,1}));
    CHECK_FALSE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    CHECK_PIXEL(pixels, 0,0,0,255, 0, 0);
    mem_free(pixels);

    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){px,py,0}, NULL,
                                   &(Vector4f){1,0,0,1}));
    CHECK_FALSE(graphics_add_vertex(&(Vector3f){px+1,py+1,0}, NULL, NULL));
    CHECK_FALSE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    CHECK_PIXEL(pixels, 0,0,0,255, 0, 0);
    mem_free(pixels);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_end_primitive_not_in_primitive)
{
    /* graphics_end_primitive() and graphics_end_and_draw_primitive()
     * share the same error-checking logic, so we just check once to make
     * sure graphics_end_primitive() properly fails on error.  Other
     * checks continue to use graphics_end_and_draw_primitive() so we
     * don't have to mess with stored primitives for the moment. */
    CHECK_FALSE(graphics_end_primitive());

    CHECK_FALSE(graphics_end_and_draw_primitive());
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_end_primitive_no_vertices)
{
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_FALSE(graphics_end_and_draw_primitive());
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_primitive_invalid_format_entry)
{
    uint32_t vertex_format[lenof(basic_vertex_format)+1];
    memcpy(vertex_format, basic_vertex_format, sizeof(basic_vertex_format));
    vertex_format[lenof(basic_vertex_format)-1] = 1;  // Invalid.
    vertex_format[lenof(basic_vertex_format)] = 0;
    CHECK_FALSE(graphics_create_primitive(
        GRAPHICS_PRIMITIVE_POINTS, point_vertices, vertex_format,
        sizeof(*point_vertices), lenof(point_vertices)));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_primitive_invalid)
{
    CHECK_FALSE(graphics_create_primitive(
                   (GraphicsPrimitiveType)0,
                   point_vertices, basic_vertex_format,
                   sizeof(*point_vertices), lenof(point_vertices)));
    CHECK_FALSE(graphics_create_primitive(
                   GRAPHICS_PRIMITIVE_POINTS, NULL, basic_vertex_format,
                   sizeof(*point_vertices), lenof(point_vertices)));
    CHECK_FALSE(graphics_create_primitive(
                   GRAPHICS_PRIMITIVE_POINTS, point_vertices, NULL,
                   sizeof(*point_vertices), lenof(point_vertices)));
    CHECK_FALSE(graphics_create_primitive(
                   GRAPHICS_PRIMITIVE_POINTS, point_vertices,
                   basic_vertex_format, 0, lenof(point_vertices)));
    CHECK_FALSE(graphics_create_primitive(
                   GRAPHICS_PRIMITIVE_POINTS, point_vertices,
                   basic_vertex_format, sizeof(*point_vertices), 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_indexed_primitive_invalid)
{
    CHECK_FALSE(graphics_create_indexed_primitive(
                   (GraphicsPrimitiveType)0, point_indexed_vertices,
                   basic_vertex_format, sizeof(*point_indexed_vertices),
                   lenof(point_indexed_vertices), point_indices,
                   sizeof(*point_indices), lenof(point_indices)));
    CHECK_FALSE(graphics_create_indexed_primitive(
                   GRAPHICS_PRIMITIVE_POINTS, NULL,
                   basic_vertex_format, sizeof(*point_indexed_vertices),
                   lenof(point_indexed_vertices), point_indices,
                   sizeof(*point_indices), lenof(point_indices)));
    CHECK_FALSE(graphics_create_indexed_primitive(
                   GRAPHICS_PRIMITIVE_POINTS, point_indexed_vertices,
                   NULL, sizeof(*point_indexed_vertices),
                   lenof(point_indexed_vertices), point_indices,
                   sizeof(*point_indices), lenof(point_indices)));
    CHECK_FALSE(graphics_create_indexed_primitive(
                   GRAPHICS_PRIMITIVE_POINTS, point_indexed_vertices,
                   basic_vertex_format, 0,
                   lenof(point_indexed_vertices), point_indices,
                   sizeof(*point_indices), lenof(point_indices)));
    CHECK_FALSE(graphics_create_indexed_primitive(
                   GRAPHICS_PRIMITIVE_POINTS, point_indexed_vertices,
                   basic_vertex_format, sizeof(*point_indexed_vertices),
                   0, point_indices,
                   sizeof(*point_indices), lenof(point_indices)));
    CHECK_FALSE(graphics_create_indexed_primitive(
                   GRAPHICS_PRIMITIVE_POINTS, point_indexed_vertices,
                   basic_vertex_format, sizeof(*point_indexed_vertices),
                   lenof(point_indexed_vertices), NULL,
                   sizeof(*point_indices), lenof(point_indices)));
    CHECK_FALSE(graphics_create_indexed_primitive(
                   GRAPHICS_PRIMITIVE_POINTS, point_indexed_vertices,
                   basic_vertex_format, sizeof(*point_indexed_vertices),
                   lenof(point_indexed_vertices), point_indices,
                   0, lenof(point_indices)));
    CHECK_FALSE(graphics_create_indexed_primitive(
                   GRAPHICS_PRIMITIVE_POINTS, point_indexed_vertices,
                   basic_vertex_format, sizeof(*point_indexed_vertices),
                   lenof(point_indexed_vertices), point_indices,
                   3, lenof(point_indices)));
    CHECK_FALSE(graphics_create_indexed_primitive(
                   GRAPHICS_PRIMITIVE_POINTS, point_indexed_vertices,
                   basic_vertex_format, sizeof(*point_indexed_vertices),
                   lenof(point_indexed_vertices), point_indices,
                   sizeof(*point_indices), 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_draw_primitive_invalid)
{
    int primitive;

    const float px = -1 + 0.5f/TESTW, py = -1 + 0.5f/TESTH;
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){px,py,0}, NULL, NULL));
    CHECK_TRUE(primitive = graphics_end_primitive());
    graphics_destroy_primitive(primitive);

    graphics_draw_primitive(0);
    graphics_draw_primitive(primitive);
    graphics_draw_primitive(INT_MAX);

    uint8_t *pixels;
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 0,0,0,255, (i/4) % TESTW, (i/4) / TESTW);
    }
    mem_free(pixels);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_draw_primitive_partial_invalid)
{
    int primitive;
    uint8_t *pixels;

    const float px = -1 + 0.5f/TESTW, py = -1 + 0.5f/TESTH;
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){px,py,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){px+0.5f,py+0.5f,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){px+1,py+1,0}, NULL, NULL));
    CHECK_TRUE(primitive = graphics_end_primitive());

    graphics_draw_primitive_partial(0, 0, -1);
    graphics_draw_primitive_partial(primitive, -1, -1);
    graphics_draw_primitive_partial(INT_MAX, 0, -1);

    graphics_destroy_primitive(primitive);
    graphics_draw_primitive_partial(primitive, 0, -1);

    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 0,0,0,255, (i/4) % TESTW, (i/4) / TESTW);
    }
    mem_free(pixels);

    graphics_destroy_primitive(primitive);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_draw_vertices_invalid)
{
    /* None of these return values, so just make sure they don't crash and
     * don't draw anything. */
    graphics_draw_vertices(
        (GraphicsPrimitiveType)0, point_vertices, basic_vertex_format,
        sizeof(*point_vertices), lenof(point_vertices));
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_POINTS, NULL, basic_vertex_format,
        sizeof(*point_vertices), lenof(point_vertices));
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_POINTS, point_vertices, NULL,
        sizeof(*point_vertices), lenof(point_vertices));
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_POINTS, point_vertices, basic_vertex_format,
        0, lenof(point_vertices));
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_POINTS, point_vertices, basic_vertex_format,
        sizeof(*point_vertices), 0);

    uint8_t *pixels;
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 0,0,0,255, (i/4) % TESTW, (i/4) / TESTW);
    }
    mem_free(pixels);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_draw_indexed_vertices_invalid)
{
    /* None of these return values, so just make sure they don't crash and
     * don't draw anything. */
    graphics_draw_indexed_vertices(
        (GraphicsPrimitiveType)0, point_indexed_vertices, basic_vertex_format,
        sizeof(*point_indexed_vertices), lenof(point_indexed_vertices),
        point_indices, sizeof(*point_indices), lenof(point_indices));
    graphics_draw_indexed_vertices(
        GRAPHICS_PRIMITIVE_POINTS, NULL, basic_vertex_format,
        sizeof(*point_indexed_vertices), lenof(point_indexed_vertices),
        point_indices, sizeof(*point_indices), lenof(point_indices));
    graphics_draw_indexed_vertices(
        GRAPHICS_PRIMITIVE_POINTS, point_indexed_vertices, NULL,
        sizeof(*point_indexed_vertices), lenof(point_indexed_vertices),
        point_indices, sizeof(*point_indices), lenof(point_indices));
    graphics_draw_indexed_vertices(
        GRAPHICS_PRIMITIVE_POINTS, point_indexed_vertices, basic_vertex_format,
        0, lenof(point_indexed_vertices),
        point_indices, sizeof(*point_indices), lenof(point_indices));
    graphics_draw_indexed_vertices(
        GRAPHICS_PRIMITIVE_POINTS, point_indexed_vertices, basic_vertex_format,
        sizeof(*point_indexed_vertices), 0,
        point_indices, sizeof(*point_indices), lenof(point_indices));
    graphics_draw_indexed_vertices(
        GRAPHICS_PRIMITIVE_POINTS, point_indexed_vertices, basic_vertex_format,
        sizeof(*point_indexed_vertices), lenof(point_indexed_vertices),
        NULL, sizeof(*point_indices), lenof(point_indices));
    graphics_draw_indexed_vertices(
        GRAPHICS_PRIMITIVE_POINTS, point_indexed_vertices, basic_vertex_format,
        sizeof(*point_indexed_vertices), lenof(point_indexed_vertices),
        point_indices, 0, lenof(point_indices));
    graphics_draw_indexed_vertices(
        GRAPHICS_PRIMITIVE_POINTS, point_indexed_vertices, basic_vertex_format,
        sizeof(*point_indexed_vertices), lenof(point_indexed_vertices),
        point_indices, 3, lenof(point_indices));
    graphics_draw_indexed_vertices(
        GRAPHICS_PRIMITIVE_POINTS, point_indexed_vertices, basic_vertex_format,
        sizeof(*point_indexed_vertices), lenof(point_indexed_vertices),
        point_indices, sizeof(*point_indices), 0);

    uint8_t *pixels;
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 0,0,0,255, (i/4) % TESTW, (i/4) / TESTW);
    }
    mem_free(pixels);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_destroy_primitive_invalid)
{
    int primitive;

    const float px = -1 + 0.5f/TESTW, py = -1 + 0.5f/TESTH;
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){px,py,0}, NULL, NULL));
    CHECK_TRUE(primitive = graphics_end_primitive());
    graphics_destroy_primitive(primitive);

    /* Just make sure these don't crash. */
    graphics_destroy_primitive(0);  // Allowed by design (no-op).
    graphics_destroy_primitive(-1);
    graphics_destroy_primitive(primitive);
    graphics_destroy_primitive(INT_MAX);

    return 1;
}

/*************************************************************************/
/********************** Exhaustive primitive tests ***********************/
/*************************************************************************/

TEST(test_primitive_points)
{
    int primitive;
    uint8_t *pixels;

    const float px = -1 + 0.5f/TESTW, py = -1 + 0.5f/TESTH;

    /* 1 point. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){px,py,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x == 0 && y == 0) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* 2 points (should be unconnected). */
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){px,py+1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){px+1,py,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = ((x==0 && y==TESTH/2) || (x==TESTW/2 && y==0)) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* 3 points, just for completeness. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){px,py,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){px,py+1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){px+1,py,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = ((x==0 && y==0) || (x==0 && y==TESTH/2)
                       || (x==TESTW/2 && y==0)) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* Partial drawing. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){px,py,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){px,py+1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){px+1,py,0}, NULL, NULL));
    CHECK_TRUE(primitive = graphics_end_primitive());
    graphics_draw_primitive_partial(primitive, 1, 1);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x==0 && y==TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    graphics_destroy_primitive(primitive);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_primitive_lines)
{
#ifdef SIL_PLATFORM_WINDOWS
    /* Some Windows graphics drivers don't render line primitives correctly;
     * for example, the driver used when running Windows as a guest under
     * the VMware virtualizer has an off-by-one bug which breaks these
     * tests.  If the SIL_TEST_SKIP_LINE_PRIMITIVES environment variable is
     * set to 1, these tests will be skipped so as not to disable dependent
     * tests. */
    const char *skip = testutil_getenv("SIL_TEST_SKIP_LINE_PRIMITIVES");
    if (skip && strcmp(skip, "1") == 0) {
        SKIP("Skipped due to user request (SIL_TEST_SKIP_LINE_PRIMITIVES).");
    }
#endif

    int primitive;
    uint8_t *pixels;

    /* As for points, ensure the line goes through the middle of pixel
     * squares rather than the edges. */
    const float x0 = -1.0f + (0.5f/TESTW);
    const float x1 =  0.0f + (0.5f/TESTW);
    const float x2 =  1.0f + (0.5f/TESTW);
    const float y0 = -1.0f + (0.5f/TESTH);
    const float y1 =  0.0f + (0.5f/TESTH);

    /* 1 vertex (should draw nothing). */
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_LINES));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y0,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* 1 line. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_LINES));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y0,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        /* The OpenGL spec (which we follow) dictates that the fragment in
         * which the line's second vertex lies is not drawn. */
        const int p = (x < TESTW/2 && y == 0) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* 1 line + 1 vertex (the extra vertex should be ignored). */
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_LINES));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y1,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x < TESTW/2 && y == 0) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* 2 lines (should be unconnected). */
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_LINES));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x2,y1,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = ((x < TESTW/2 && y == 0)
                       || (x >= TESTW/2 && x < TESTW && y == TESTH/2))
                      ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* Partial drawing. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_LINES));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x2,y1,0}, NULL, NULL));
    CHECK_TRUE(primitive = graphics_end_primitive());
    graphics_draw_primitive_partial(primitive, 2, 2);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x >= TESTW/2 && x < TESTW && y == TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* Partial drawing with unaligned start and count.  On all current
     * systems, this should reliably draw a line between the second and
     * third points, so we check for it. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_draw_primitive_partial(primitive, 1, 3);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x == TESTW/2 && y < TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* Partial drawing with unaligned start and infinite count. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_draw_primitive_partial(primitive, 1, -1);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x == TESTW/2 && y < TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    graphics_destroy_primitive(primitive);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_primitive_line_strip)
{
#ifdef SIL_PLATFORM_WINDOWS
    /* As for test_primitive_lines() above. */
    const char *skip = testutil_getenv("SIL_TEST_SKIP_LINE_PRIMITIVES");
    if (skip && strcmp(skip, "1") == 0) {
        SKIP("Skipped due to user request (SIL_TEST_SKIP_LINE_PRIMITIVES).");
    }
#endif

    int primitive;
    uint8_t *pixels;

    const float x0 = -1.0f + (0.5f/TESTW);
    const float x1 =  0.0f + (0.5f/TESTW);
    const float x2 =  1.0f + (0.5f/TESTW);
    const float y0 = -1.0f + (0.5f/TESTH);
    const float y1 =  0.0f + (0.5f/TESTH);

    /* 1 vertex (should draw nothing). */
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_LINE_STRIP));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y0,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* 1 line. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_LINE_STRIP));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y0,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x < TESTW/2 && y == 0) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* 2 connected lines. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_LINE_STRIP));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y1,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = ((x < TESTW/2 && y == 0)
                       || (x == TESTW/2 && y < TESTH/2)) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* 3 connected lines. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_LINE_STRIP));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x2,y1,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = ((x < TESTW/2 && y == 0)
                       || (x == TESTW/2 && y < TESTH/2)
                       || (x >= TESTW/2 && y == TESTH/2)) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* Partial drawing. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_LINE_STRIP));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x0,y0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x1,y1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){x2,y1,0}, NULL, NULL));
    CHECK_TRUE(primitive = graphics_end_primitive());
    graphics_draw_primitive_partial(primitive, 1, 3);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = ((x == TESTW/2 && y < TESTH/2)
                       || (x >= TESTW/2 && y == TESTH/2)) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    graphics_destroy_primitive(primitive);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_primitive_triangles)
{
    int primitive;
    uint8_t *pixels;

    /* 1-2 vertices (should draw nothing). */
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_TRIANGLES));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,-1,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_TRIANGLES));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,-1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* 1 triangle. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_TRIANGLES));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,-1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,-1,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const float xf = (float)x / (float)TESTW;
        const float yf = 0.5f - ((float)y / (float)TESTH);
        if (ifloorf(fabsf(yf - xf) * TESTW) <= 1) {
            /* Ignore pixels on a diagonal border, so as not to rely on
             * any specific rendering behavior. */
            continue;
        }
        const int p = (xf < yf && y < TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* 1 triangle + 2 vertices (the extra vertices should be ignored). */
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_TRIANGLES));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,-1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,-1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){1,-1,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const float xf = (float)x / (float)TESTW;
        const float yf = 0.5f - ((float)y / (float)TESTH);
        if (ifloorf(fabsf(yf - xf) * TESTW) <= 1) {
            continue;
        }
        const int p = (xf < yf && y < TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* 2 triangles (should be unconnected). */
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_TRIANGLES));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,-1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,-1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){1,-1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){1,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const float xf = (float)x / (float)TESTW;
        const float yf = 0.5f - ((float)y / (float)TESTH);
        if (ifloorf(fabsf(yf - xf) * TESTW) <= 1
         || ifloorf(fabsf(yf - (xf-0.5f)) * TESTW) <= 1) {
            continue;
        }
        const int p = ((xf < yf || (xf-0.5f) > yf) && y < TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* Partial drawing. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_TRIANGLES));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,-1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,-1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){1,-1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){1,0,0}, NULL, NULL));
    CHECK_TRUE(primitive = graphics_end_primitive());
    graphics_draw_primitive_partial(primitive, 3, 3);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const float xf = (float)x / (float)TESTW;
        const float yf = 0.5f - ((float)y / (float)TESTH);
        if (ifloorf(fabsf(yf - (xf-0.5f)) * TESTW) <= 1) {
            continue;
        }
        const int p = ((xf-0.5f) > yf && y < TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* Partial drawing with unaligned start and count. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_draw_primitive_partial(primitive, 1, 5);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const float xf = (float)x / (float)TESTW;
        const float yf = 0.5f - ((float)y / (float)TESTH);
        if (ifloorf(fabsf(yf - xf) * TESTW) <= 1) {
            continue;
        }
        const int p = (x < TESTW/2 && xf > yf && y < TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* Partial drawing with unaligned start and infinite count. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_draw_primitive_partial(primitive, 1, -1);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const float xf = (float)x / (float)TESTW;
        const float yf = 0.5f - ((float)y / (float)TESTH);
        if (ifloorf(fabsf(yf - xf) * TESTW) <= 1) {
            continue;
        }
        const int p = (x < TESTW/2 && xf > yf && y < TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    graphics_destroy_primitive(primitive);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_primitive_triangle_strip)
{
    int primitive;
    uint8_t *pixels;

    /* 1-2 vertices (should draw nothing). */
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_TRIANGLE_STRIP));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,-1,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_TRIANGLE_STRIP));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,-1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* 1 triangle. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_TRIANGLE_STRIP));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,-1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,-1,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const float xf = (float)x / (float)TESTW;
        const float yf = 0.5f - ((float)y / (float)TESTH);
        if (ifloorf(fabsf(yf - xf) * TESTW) <= 1) {
            /* Ignore pixels on a diagonal border, so as not to rely on
             * any specific rendering behavior. */
            continue;
        }
        const int p = (xf < yf && y < TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* 2 connected triangles forming a square. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_TRIANGLE_STRIP));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,-1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,-1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x < TESTW/2 && y < TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* 3 connected triangles. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_TRIANGLE_STRIP));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,-1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,-1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){1,-1,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const float xf = (float)x / (float)TESTW;
        const float yf = 0.5f - ((float)y / (float)TESTH);
        if (ifloorf(fabsf(yf - (xf-0.5f)) * TESTW) <= 1) {
            continue;
        }
        const int p = ((xf-0.5f) < yf && y < TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* Partial drawing. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_TRIANGLE_STRIP));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,-1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,-1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){1,-1,0}, NULL, NULL));
    CHECK_TRUE(primitive = graphics_end_primitive());
    graphics_draw_primitive_partial(primitive, 2, 3);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const float xf = (float)x / (float)TESTW;
        const float yf = 0.5f - ((float)y / (float)TESTH);
        if (ifloorf(fabsf(yf - (xf-0.5f)) * TESTW) <= 1) {
            continue;
        }
        const int p = (x >= TESTW/2 && (xf-0.5f) < yf && y < TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    graphics_destroy_primitive(primitive);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_primitive_quads)
{
    int primitive;
    uint8_t *pixels;

    /* 1-3 vertices (should draw nothing). */
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* 1 quad. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x < TESTW/2 && y >= TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* 1 quad + 3 vertices (the extra vertices should be ignored). */
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0.5,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0.5,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){1,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x < TESTW/2 && y >= TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* 2 quads (should be unconnected). */
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0.5,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0.5,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){1,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){1,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = ((x < TESTW/2 || x >= TESTW*3/4) && y >= TESTH/2)
                      ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* Partial drawing. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0.5,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0.5,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){1,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){1,0,0}, NULL, NULL));
    CHECK_TRUE(primitive = graphics_end_primitive());
    graphics_draw_primitive_partial(primitive, 4, 4);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x >= TESTW*3/4 && y >= TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* Partial drawing with unaligned start and count.  The exact behavior
     * is undefined, but _something_ should be drawn. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_draw_primitive_partial(primitive, 2, 6);
    ASSERT(pixels = grab_display());
    {
        int found_white_pixel = 0;
        for (int i = 0; i < TESTW*TESTH*4; i += 4) {
            if (pixels[i+0] == 255 && pixels[i+1] == 255 && pixels[i+2] == 255
             && pixels[i+3] == 255) {
                found_white_pixel = 1;
                break;
            }
        }
        CHECK_TRUE(found_white_pixel);
    }

    /* Partial drawing with unaligned start and infinite count.  The
     * result should be the same as the above test. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_draw_primitive_partial(primitive, 2, -1);
    uint8_t *pixels2;
    ASSERT(pixels2 = grab_display());
    CHECK_MEMEQUAL(pixels2, pixels, TESTW*TESTH*4);
    mem_free(pixels2);
    mem_free(pixels);

    graphics_destroy_primitive(primitive);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_primitive_quad_strip)
{
    int primitive;
    uint8_t *pixels;

    /* 1-3 vertices (should draw nothing). */
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUAD_STRIP));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUAD_STRIP));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUAD_STRIP));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* 1 quad. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUAD_STRIP));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x < TESTW/2 && y >= TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* 1 quad + 1 vertex (the extra vertex should be ignored). */
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUAD_STRIP));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0.5,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x < TESTW/2 && y >= TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* 2 connected quads. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUAD_STRIP));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0.5,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0.5,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x < TESTW*3/4 && y >= TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* 3 connected quads. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUAD_STRIP));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0.5,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0.5,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){1,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){1,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (y >= TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* Partial drawing. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUAD_STRIP));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0.5,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0.5,1,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){1,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){1,1,0}, NULL, NULL));
    CHECK_TRUE(primitive = graphics_end_primitive());
    graphics_draw_primitive_partial(primitive, 2, 6);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int p = (x >= TESTW/2 && y >= TESTH/2) ? 255 : 0;
        CHECK_PIXEL(&pixels[i], p,p,p,255, x, y);
    }
    mem_free(pixels);

    /* Partial drawing with unaligned start and count.  The exact behavior
     * is undefined, but _something_ should be drawn. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_draw_primitive_partial(primitive, 3, 5);
    ASSERT(pixels = grab_display());
    {
        int found_white_pixel = 0;
        for (int i = 0; i < TESTW*TESTH*4; i += 4) {
            if (pixels[i+0] == 255 && pixels[i+1] == 255 && pixels[i+2] == 255
             && pixels[i+3] == 255) {
                found_white_pixel = 1;
                break;
            }
        }
        CHECK_TRUE(found_white_pixel);
    }

    /* Partial drawing with unaligned start and infinite count.  The
     * result should be the same as the above test. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_draw_primitive_partial(primitive, 3, -1);
    uint8_t *pixels2;
    ASSERT(pixels2 = grab_display());
    CHECK_MEMEQUAL(pixels2, pixels, TESTW*TESTH*4);
    mem_free(pixels2);
    mem_free(pixels);

    graphics_destroy_primitive(primitive);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_all_vertex_types)
{
    /*
     * This test checks all combinations of position, texture coordinate,
     * and color data for immediate primitives, using a single square quad,
     * a solid texture of 2/3 white, and vertex colors of 80% opaque white.
     */

    static const Vector3f positions[] = {{-1,0,0}, {-1,1,0}, {0,1,0}, {0,0,0}};
    static const Vector2f texcoords[] = {{0,0}, {0,1}, {1,1}, {1,0}};
    static const Vector4f color = {0.8, 0.8, 0.8, 1};
    int texture;
    CHECK_TRUE(texture = texture_create_with_data(
                   1, 1, "\xAA\xAA\xAA\xFF", TEX_FORMAT_RGBA8888, 1, 0, 0));

    for (int use_texture = 0; use_texture <= 1; use_texture++) {
        for (int use_color = 0; use_color <= 1; use_color++) {
            DLOG("Testing texture %s, color %s", use_texture ? "on" : "off",
                 use_color ? "on" : "off");
            graphics_clear(0, 0, 0, 0, 1, 0);
            texture_apply(0, use_texture ? texture : 0);
            CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
            for (int i = 0; i < 4; i++) {
                CHECK_TRUE(graphics_add_vertex(
                               &positions[i],
                               use_texture ? &texcoords[i] : NULL,
                               use_color ? &color : NULL));
            }
            CHECK_TRUE(graphics_end_and_draw_primitive());
            uint8_t *pixels;
            ASSERT(pixels = grab_display());
            uint8_t level = 255;
            if (use_texture) {
                level = level*2/3;
            }
            if (use_color) {
                level = level*4/5;
            }
            for (int i = 0; i < TESTW*TESTH*4; i += 4) {
                const int x = (i/4) % TESTW;
                const int y = (i/4) / TESTW;
                const int p = (x < TESTW/2 && y >= TESTH/2) ? level : 0;
                if (pixels[i+0] != p
                 || pixels[i+1] != p
                 || pixels[i+2] != p
                 || pixels[i+3] != 255) {
                    FAIL("(%d,%d): Pixel (%d,%d) was RGBA (%u,%u,%u,%u)"
                         " but should have been RGBA (%u,%u,%u,%u)",
                         use_texture, use_color, x, y,
                         pixels[i+0], pixels[i+1], pixels[i+2], pixels[i+3],
                         p, p, p, 255);
                }
            }
            mem_free(pixels);
        }
    }

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_all_vertex_formats)
{
    /*
     * This test checks all combinations of position, texture coordinate,
     * and color data formats for vertices, using a single square quad, a
     * solid texture of 2/3 white, and vertex colors of 80% opaque white.
     * The test checks all vertex format specifiers (GRAPHICS_VERTEX_*)
     * in all orders as well as behavior in the presence of padding.
     */

    static const Vector4f positions[] = {{-1,0,0,1}, {-1,1,0,1},
                                         {0,1,0,1}, {0,0,0,1}};
    static const Vector2f texcoords[] = {{0,0}, {0,1}, {1,1}, {1,0}};
    static const Vector4f color = {0.8, 0.8, 0.8, 1};
    int texture;
    CHECK_TRUE(texture = texture_create_with_data(
                   1, 1, "\xAA\xAA\xAA\xFF", TEX_FORMAT_RGBA8888, 1, 0, 0));
    texture_apply(0, texture);

    struct FormatData {
        uint32_t format;
        uint8_t is_float;
        uint8_t size;
        uint8_t count;
    };
    static const struct FormatData position_formats[] = {
        {GRAPHICS_VERTEX_FORMAT(POSITION_2S, 0), 0, 2, 2},
        {GRAPHICS_VERTEX_FORMAT(POSITION_2F, 0), 1, 4, 2},
        {GRAPHICS_VERTEX_FORMAT(POSITION_3F, 0), 1, 4, 3},
#ifndef SIL_PLATFORM_PSP  // Not supported on the PSP.
        {GRAPHICS_VERTEX_FORMAT(POSITION_4F, 0), 1, 4, 4},
#endif
    };
    static const struct FormatData texcoord_formats[] = {
        {GRAPHICS_VERTEX_FORMAT(TEXCOORD_2F, 0), 1, 4, 2},
    };
    static const struct FormatData color_formats[] = {
        {GRAPHICS_VERTEX_FORMAT(COLOR_4NUB, 0), 0, 1, 4},
        {GRAPHICS_VERTEX_FORMAT(COLOR_4F,  0), 1, 4, 4},
    };
    static const char * const type_abbrev[2][5] = {
        {"", "NUB", "S", "", "I"},
        {"", "", "", "", "F"},
    };

    /* Make room for 5*4 bytes (4 data elements + 1 padding element) for
     * each of the 3 vertex data types, for 4 vertices. */
    uint8_t vertex_buffer[5*4*3*4];

    for (int pos_index = 0; pos_index < lenof(position_formats); pos_index++) {
      const struct FormatData * const posfmt = &position_formats[pos_index];
      for (int tex_index = 0; tex_index < lenof(texcoord_formats); tex_index++) {
        const struct FormatData * const texfmt = &texcoord_formats[tex_index];
        for (int col_index = 0; col_index < lenof(color_formats); col_index++) {
          const struct FormatData * const colfmt = &color_formats[col_index];
          for (int use_padding = 0; use_padding <= 1; use_padding++) {

            DLOG("Testing with POSITION_%d%s, TEXCOORD_%d%s,"
                 " COLOR_%d%s, %s padding",
                 posfmt->count, type_abbrev[posfmt->is_float][posfmt->size],
                 texfmt->count, type_abbrev[texfmt->is_float][texfmt->size],
                 colfmt->count, type_abbrev[colfmt->is_float][colfmt->size],
                 use_padding ? "with" : "without");

            uint32_t vertex_format[4];
            size_t vertex_size = 0;
            if (use_padding) {
                vertex_size += posfmt->size;
            }
            vertex_format[0] = posfmt->format + vertex_size;
            vertex_size += posfmt->size * posfmt->count;
            vertex_size = align_up(vertex_size, texfmt->size);
            if (use_padding) {
                vertex_size += texfmt->size;
            }
            vertex_format[1] = texfmt->format + vertex_size;
            vertex_size += texfmt->size * texfmt->count;
            vertex_size = align_up(vertex_size, colfmt->size);
            if (use_padding) {
                vertex_size += colfmt->size;
            }
            vertex_format[2] = colfmt->format + vertex_size;
            vertex_size += colfmt->size * colfmt->count;
            vertex_size = align_up(vertex_size, 4);
            vertex_format[3] = 0;

            #define STORE_VALUE(value,ptr,format)  do { \
                if ((format)->is_float) {               \
                    *(float *)((void *)ptr) = (value);  \
                } else if ((format)->size == 1) {       \
                    *(uint8_t *)((void *)ptr) = iroundf((value)*255); \
                } else if ((format)->size == 2) {       \
                    *(int16_t *)((void *)ptr) = (value);\
                } else {                                \
                    *(int32_t *)((void *)ptr) = (value);\
                }                                       \
            } while (0)
            for (int i = 0; i < 4; i++) {
                uint8_t *ptr = vertex_buffer + i * vertex_size;
                uint32_t offset = 0;
                if (use_padding) {
                    offset += posfmt->size;
                }
                for (unsigned int j = 0; j < posfmt->count; j++) {
                    STORE_VALUE((&positions[i].x)[j], &ptr[offset], posfmt);
                    offset += posfmt->size;
                }
                offset = align_up(offset, texfmt->size);
                if (use_padding) {
                    offset += texfmt->size;
                }
                for (unsigned int j = 0; j < texfmt->count; j++) {
                    STORE_VALUE((&texcoords[i].x)[j], &ptr[offset], texfmt);
                    offset += texfmt->size;
                }
                offset = align_up(offset, colfmt->size);
                if (use_padding) {
                    offset += colfmt->size;
                }
                for (unsigned int j = 0; j < colfmt->count; j++) {
                    STORE_VALUE((&color.x)[j], &ptr[offset], colfmt);
                    offset += colfmt->size;
                }
            }
            #undef STORE_VALUE

            graphics_clear(0, 0, 0, 0, 1, 0);
            graphics_draw_vertices(GRAPHICS_PRIMITIVE_QUADS, vertex_buffer,
                                   vertex_format, vertex_size, 4);
            uint8_t *pixels;
            ASSERT(pixels = grab_display());
            for (int i = 0; i < TESTW*TESTH*4; i += 4) {
                const int x = (i/4) % TESTW;
                const int y = (i/4) / TESTW;
                const int p = (x < TESTW/2 && y >= TESTH/2) ? 136 : 0;
                if (pixels[i+0] != p
                 || pixels[i+1] != p
                 || pixels[i+2] != p
                 || pixels[i+3] != 255) {
                    FAIL("(%d,%d,%d,%d): Pixel (%d,%d) was RGBA (%u,%u,%u,%u)"
                         " but should have been RGBA (%u,%u,%u,%u)",
                         pos_index, tex_index, col_index, use_padding, x, y,
                         pixels[i+0], pixels[i+1], pixels[i+2], pixels[i+3],
                         p, p, p, 255);
                }
            }
            mem_free(pixels);
          }  // for (use_padding)
        }  // for (col_index)
      }  // for (tex_index)
    }  // for (pos_index)

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_all_index_formats)
{
    /*
     * This test checks all supported index types (1, 2, and 4 byte
     * integers) using a single square quad with position-only vertices.
     * The test also checks that indices up to 65535 work properly.
     */

    static const BasicVertex vertices[] =
        {{-1,0,0}, {-1,1,0}, {0,0,0}, {0,1,0}};
    uint8_t index_buffer[4*4];
    void *index_pointer = index_buffer;

    const int has_32bit_indices =
#ifdef USES_GL
        opengl_has_formats(OPENGL_FORMAT_INDEX32);
#else
        0;
#endif
    const int max_index_size = has_32bit_indices ? 4 : 2;
    for (int index_size = 1; index_size <= max_index_size; index_size *= 2) {
        const unsigned int index_limit = (index_size == 1) ? 256 : 65536;
        BasicVertex *large_buf;
        ASSERT(large_buf = mem_alloc(sizeof(*large_buf) * index_limit, 0,
                                     MEM_ALLOC_CLEAR | MEM_ALLOC_TEMP));
        memcpy(&large_buf[index_limit - 4], vertices, sizeof(vertices));
        for (int use_big_indices = 0; use_big_indices <= 1; use_big_indices++){
            const unsigned int index_base = use_big_indices ? index_limit-4 : 0;
            if (index_size == 1) {
                ((uint8_t *)index_pointer)[0] = index_base + 0;
                ((uint8_t *)index_pointer)[1] = index_base + 1;
                ((uint8_t *)index_pointer)[2] = index_base + 3;
                ((uint8_t *)index_pointer)[3] = index_base + 2;
            } else if (index_size == 2) {
                ((uint16_t *)index_pointer)[0] = index_base + 0;
                ((uint16_t *)index_pointer)[1] = index_base + 1;
                ((uint16_t *)index_pointer)[2] = index_base + 3;
                ((uint16_t *)index_pointer)[3] = index_base + 2;
            } else {  // index_size == 4
                ((uint32_t *)index_pointer)[0] = index_base + 0;
                ((uint32_t *)index_pointer)[1] = index_base + 1;
                ((uint32_t *)index_pointer)[2] = index_base + 3;
                ((uint32_t *)index_pointer)[3] = index_base + 2;
            }

            DLOG("Testing index size %d, base %u", index_size, index_base);

            const void * const vertex_pointer =
                use_big_indices ? large_buf : vertices;
            const unsigned int num_vertices = use_big_indices ? index_limit : 4;
            uint8_t *pixels;

            graphics_clear(0, 0, 0, 0, 1, 0);
            int primitive;
            CHECK_TRUE(primitive = graphics_create_indexed_primitive(
                GRAPHICS_PRIMITIVE_QUADS, vertex_pointer, basic_vertex_format,
                sizeof(BasicVertex), num_vertices, index_pointer, index_size,
                4));
            graphics_draw_primitive(primitive);
            graphics_destroy_primitive(primitive);
            ASSERT(pixels = grab_display());
            for (int i = 0; i < TESTW*TESTH*4; i += 4) {
                const int x = (i/4) % TESTW;
                const int y = (i/4) / TESTW;
                const int p = (x < TESTW/2 && y >= TESTH/2) ? 255 : 0;
                if (pixels[i+0] != p
                 || pixels[i+1] != p
                 || pixels[i+2] != p
                 || pixels[i+3] != 255) {
                    FAIL("(%d,%d): Pixel (%d,%d) was RGBA (%u,%u,%u,%u)"
                         " but should have been RGBA (%u,%u,%u,%u)",
                         index_size, use_big_indices, x, y,
                         pixels[i+0], pixels[i+1], pixels[i+2], pixels[i+3],
                         p, p, p, 255);
                }
            }
            mem_free(pixels);

            graphics_clear(0, 0, 0, 0, 1, 0);
            graphics_draw_indexed_vertices(
                GRAPHICS_PRIMITIVE_QUADS, vertex_pointer, basic_vertex_format,
                sizeof(BasicVertex), num_vertices, index_pointer, index_size,
                4);
            ASSERT(pixels = grab_display());
            for (int i = 0; i < TESTW*TESTH*4; i += 4) {
                const int x = (i/4) % TESTW;
                const int y = (i/4) / TESTW;
                const int p = (x < TESTW/2 && y >= TESTH/2) ? 255 : 0;
                if (pixels[i+0] != p
                 || pixels[i+1] != p
                 || pixels[i+2] != p
                 || pixels[i+3] != 255) {
                    FAIL("(%d,%d): Pixel (%d,%d) was RGBA (%u,%u,%u,%u)"
                         " but should have been RGBA (%u,%u,%u,%u)",
                         index_size, use_big_indices, x, y,
                         pixels[i+0], pixels[i+1], pixels[i+2], pixels[i+3],
                         p, p, p, 255);
                }
            }
            mem_free(pixels);
        }
        mem_free(large_buf);
    }

    return 1;
}

/*************************************************************************/
/*********************** Other miscellaneous tests ***********************/
/*************************************************************************/

TEST(test_reuse_primitive)
{
    int primitive;
    uint8_t *pixels;

    int texture;
    CHECK_TRUE(texture = texture_create_with_data(
                   1, 1, "\x33\x66\x99\xAA", TEX_FORMAT_RGBA8888, 1, 0, 0));
    texture_apply(0, texture);

    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,0,0}, &(Vector2f){0,0},
                                   &(Vector4f){0.5,1,0.333,1}));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){-1,1,0}, &(Vector2f){0,1},
                                   &(Vector4f){0.5,1,0.333,1}));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,1,0}, &(Vector2f){1,1},
                                   &(Vector4f){0.5,1,0.333,1}));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,0,0}, &(Vector2f){1,0},
                                   &(Vector4f){0.5,1,0.333,1}));
    CHECK_TRUE(primitive = graphics_end_primitive());
    graphics_draw_primitive(primitive);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int r = (x < TESTW/2 && y >= TESTH/2) ? 0x11 : 0;
        const int g = (x < TESTW/2 && y >= TESTH/2) ? 0x44 : 0;
        const int b = (x < TESTW/2 && y >= TESTH/2) ? 0x22 : 0;
        CHECK_PIXEL_NEAR(&pixels[i], r,g,b,255, 1, x, y);
    }
    mem_free(pixels);

    /* Make sure the stored primitive can be redrawn correctly after
     * another primitive with different parameters is drawn. */
    CHECK_TRUE(graphics_begin_primitive(GRAPHICS_PRIMITIVE_POINTS));
    CHECK_TRUE(graphics_add_vertex(&(Vector3f){0,0,0}, NULL, NULL));
    CHECK_TRUE(graphics_end_and_draw_primitive());
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_draw_primitive(primitive);
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        const int x = (i/4) % TESTW;
        const int y = (i/4) / TESTW;
        const int r = (x < TESTW/2 && y >= TESTH/2) ? 0x11 : 0;
        const int g = (x < TESTW/2 && y >= TESTH/2) ? 0x44 : 0;
        const int b = (x < TESTW/2 && y >= TESTH/2) ? 0x22 : 0;
        CHECK_PIXEL_NEAR(&pixels[i], r,g,b,255, 1, x, y);
    }
    mem_free(pixels);

    texture_destroy(texture);
    graphics_destroy_primitive(primitive);
    return 1;
}

/*************************************************************************/
/*************************************************************************/
