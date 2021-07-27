/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/graphics/shader-gen.c: Shader generator tests.
 */

#include "src/base.h"
#include "src/graphics.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/test/base.h"
#include "src/test/graphics/internal.h"
#include "src/texture.h"

/*************************************************************************/
/****************** Shader generator functions and data ******************/
/*************************************************************************/

/*
 * For these tests, we write shader code specific to each test primitive,
 * so our "generator" simply copies that code to the caller's return
 * variables, and we return a constant key value (0) for all rendering
 * states.  This implies that the caller must clear out the old shader
 * (by calling graphics_set_shader_generator() again) to render with a
 * second shader in the same test routine.
 */

/* Store pointers to the source code strings in these variables. */
static const char *vertex_shader_source;
static const char *fragment_shader_source;

/*-----------------------------------------------------------------------*/

/**
 * generate_vertex_shader_source, generate_fragment_shader_source:  Shader
 * source generators.  Each function returns a copy of the appropriate
 * source code string (vertex_shader_source or fragment_shader_source).
 */
static char *generate_vertex_shader_source(
    UNUSED GraphicsPrimitiveType primitive_type, UNUSED int position_count,
    UNUSED int texcoord_count, UNUSED GraphicsTextureColorType texcolor_type,
    UNUSED int tex_offset, UNUSED int color_count, UNUSED int color_uniform,
    UNUSED int fog, UNUSED int alpha_test,
    UNUSED GraphicsComparisonType alpha_comparison)
{
    char *shader;
    ASSERT(shader = mem_strdup(vertex_shader_source, 0));
    return shader;
}

static char *generate_fragment_shader_source(
    UNUSED GraphicsPrimitiveType primitive_type, UNUSED int position_count,
    UNUSED int texcoord_count, UNUSED GraphicsTextureColorType texcolor_type,
    UNUSED int tex_offset, UNUSED int color_count, UNUSED int color_uniform,
    UNUSED int fog, UNUSED int alpha_test,
    UNUSED GraphicsComparisonType alpha_comparison)
{
    char *shader;
    ASSERT(shader = mem_strdup(fragment_shader_source, 0));
    return shader;
}

/*-----------------------------------------------------------------------*/

/**
 * generate_shader_key:  Shader key generator.  Always returns 0.
 */
static uint32_t generate_shader_key(
    UNUSED GraphicsPrimitiveType primitive_type, UNUSED int position_count,
    UNUSED int texcoord_count, UNUSED GraphicsTextureColorType texcolor_type,
    UNUSED int tex_offset, UNUSED int color_count, UNUSED int color_uniform,
    UNUSED int fog, UNUSED int alpha_test,
    UNUSED GraphicsComparisonType alpha_comparison)
{
    return 0;
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

static int wrap_test_graphics_shader_gen(void);
static int do_test_graphics_shader_gen(void);

int test_graphics_shader_gen(void)
{
    return run_tests_in_window(wrap_test_graphics_shader_gen);
}

static int wrap_test_graphics_shader_gen(void)
{
    if (!graphics_set_shader_generator(NULL, NULL, NULL, 0, 0)) {
        CHECK_FALSE(graphics_add_shader_uniform("test_uniform"));
        CHECK_FALSE(graphics_add_shader_attribute("test_attribute", 1));
        SKIP("Shaders not supported on this system.");
    }
    return do_test_graphics_shader_gen();
}

DEFINE_GENERIC_TEST_RUNNER(do_test_graphics_shader_gen)

/*-----------------------------------------------------------------------*/

TEST_INIT(init)
{
    CHECK_TRUE(graphics_set_shader_generator(generate_vertex_shader_source,
                                             generate_fragment_shader_source,
                                             generate_shader_key, 1, 0));
    graphics_set_viewport(0, 0, 64, 64);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
    graphics_finish_frame();
    CHECK_TRUE(graphics_set_shader_generator(NULL, NULL, NULL, 0, 0));
    graphics_flush_resources();
    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_set_shader_generator_invalid)
{
    CHECK_FALSE(graphics_set_shader_generator(NULL, (void *)1, NULL, 0, 1));
    CHECK_FALSE(graphics_set_shader_generator(NULL, NULL, (void *)1, 0, 1));
    CHECK_FALSE(graphics_set_shader_generator((void *)1, NULL, (void *)1,
                                              0, 1));
    CHECK_FALSE(graphics_set_shader_generator((void *)1, (void *)1, NULL,
                                              0, 1));
    CHECK_FALSE(graphics_set_shader_generator((void *)1, (void *)1, (void *)1,
                                              -1, 1));
    CHECK_FALSE(graphics_set_shader_generator((void *)1, (void *)1, (void *)1,
                                              0, 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_basic_shader)
{
    vertex_shader_source =
        "in highp vec3 position;\n"
        "void main() {\n"
        "    gl_Position = vec4(position, 1.0);\n"
        "}\n";
    fragment_shader_source =
        "void main() {\n"
        "    color_out = vec4(0.333, 0.667, 1.0, 0.6);\n"
        "}\n";

    draw_square(0, 1,1,1,1);
    CHECK_SQUARE(0.2, 0.4, 0.6);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_clear_generator_funcs)
{
    vertex_shader_source =
        "in highp vec3 position;\n"
        "void main() {\n"
        "    gl_Position = vec4(position, 1.0);\n"
        "}\n";
    fragment_shader_source =
        "void main() {\n"
        "    color_out = vec4(0.333, 0.667, 1.0, 0.6);\n"
        "}\n";

    /* This should restore the standard generator functions. */
    CHECK_TRUE(graphics_set_shader_generator(NULL, NULL, NULL, 0, 0));

    draw_square(0, 1,1,1,1);
    CHECK_SQUARE(1, 1, 1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_clear_generator_funcs_implicit)
{
    vertex_shader_source =
        "in highp vec3 position;\n"
        "void main() {\n"
        "    gl_Position = vec4(position, 1.0);\n"
        "}\n";
    fragment_shader_source =
        "void main() {\n"
        "    color_out = vec4(0.333, 0.667, 1.0, 0.6);\n"
        "}\n";

    /* This should implicitly restore the standard generator functions. */
    CHECK_TRUE(graphics_use_shader_objects(0));

    draw_square(0, 1,1,1,1);
    CHECK_SQUARE(1, 1, 1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_basic_shader_no_trailing_newline)
{
    vertex_shader_source =
        "in highp vec3 position;\n"
        "void main() {\n"
        "    gl_Position = vec4(position, 1.0);\n"
        "}";
    fragment_shader_source =
        "void main() {\n"
        "    color_out = vec4(0.333, 0.667, 1.0, 0.6);\n"
        "}";

    draw_square(0, 1,1,1,1);
    CHECK_SQUARE(0.2, 0.4, 0.6);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_common_vertex_attributes)
{
    vertex_shader_source =
        "in highp vec3 position;\n"
        "in texp vec2 texcoord;\n"
        "in lowp vec4 color;\n"
        "out texp vec2 texcoord_varying;\n"
        "out lowp vec4 color_varying;\n"
        "void main() {\n"
        "    gl_Position = vec4(position, 1.0);\n"
        "    texcoord_varying = texcoord;\n"
        "    color_varying = color;\n"
        "}\n";
    fragment_shader_source =
        "uniform sampler2D tex;\n"
        "in texp vec2 texcoord_varying;\n"
        "in lowp vec4 color_varying;\n"
        "void main() {\n"
        "    color_out = texture2D(tex, texcoord_varying) * color_varying;\n"
        "}\n";

    static const uint8_t texture_data[] = {
        0xFF,0x00,0x00,0xFF, 0x00,0xFF,0x00,0xFF,
        0x00,0x00,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF
    };
    int texture;
    ASSERT(texture = texture_create_with_data(2, 2, texture_data,
                                              TEX_FORMAT_RGBA8888, 2, 0, 0));
    texture_set_antialias(texture, 0);
    texture_apply(0, texture);

    ASSERT(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    ASSERT(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0}, &(Vector2f){0, 0},
                               &(Vector4f){0.333, 0.667, 1.0, 0.2}));
    ASSERT(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0}, &(Vector2f){0, 1},
                               &(Vector4f){0.333, 0.667, 1.0, 0.2}));
    ASSERT(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0}, &(Vector2f){1, 1},
                               &(Vector4f){0.333, 0.667, 1.0, 0.2}));
    ASSERT(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0}, &(Vector2f){1, 0},
                               &(Vector4f){0.333, 0.667, 1.0, 0.2}));
    ASSERT(graphics_end_and_draw_primitive());
    uint8_t pixels[64*64*4];
    ASSERT(graphics_read_pixels(0, 0, 64, 64, pixels));
    for (int i = 0; i < 64*64*4; i += 4) {
        const int x = (i/4) % 64;
        const int y = (i/4) / 64;
        if (x >= 16 && x < 48 && y >= 16 && y < 48) {
            const int r = ((x<32 && y<32) || (x>=32 && y>=32)) ? 0x11 : 0;
            const int g = (x >= 32) ? 0x22 : 0;
            const int b = (y >= 32) ? 0x33 : 0;
            CHECK_PIXEL(&pixels[i], r,g,b,255, x, y);
        } else {
            CHECK_PIXEL(&pixels[i], 0,0,0,255, x, y);
        }
    }

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_coordinate_transform)
{
    vertex_shader_source =
        "uniform highp mat4 transform;\n"
        "in highp vec3 position;\n"
        "void main() {\n"
        "    gl_Position = vec4(position, 1.0) * transform;\n"
        "}\n";
    fragment_shader_source =
        "void main() {\n"
        "    color_out = vec4(0.333, 0.667, 1.0, 0.6);\n"
        "}\n";

    graphics_set_projection_matrix(
                     &(Matrix4f){0.5,0,0,0, 0,1,0,0, 0,0,1,0, 0.5,0,0,1});
    graphics_set_view_matrix(
                     &(Matrix4f){1,0,0,0, 0,0.25,0,0, 0,0,1,0, 0,-0.5,0,1});
    graphics_set_model_matrix(
                     &(Matrix4f){1,0,0,0, 0,1,0,0, 0,0,1,0, 0,1,0,1});
    draw_square(0, 1,1,1,1);
    CHECK_COLORED_RECTANGLE(16,8, 48,24, 0.2, 0.4, 0.6);

    graphics_set_projection_matrix(&mat4_identity);
    graphics_set_view_matrix(&mat4_identity);
    graphics_set_model_matrix(&mat4_identity);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_texture_offset)
{
    vertex_shader_source =
        "uniform texp vec2 tex_offset;\n"
        "in highp vec3 position;\n"
        "in texp vec2 texcoord;\n"
        "in lowp vec4 color;\n"
        "out texp vec2 texcoord_varying;\n"
        "out lowp vec4 color_varying;\n"
        "void main() {\n"
        "    gl_Position = vec4(position, 1.0);\n"
        "    texcoord_varying = texcoord + tex_offset;\n"
        "    color_varying = color;\n"
        "}\n";
    fragment_shader_source =
        "uniform sampler2D tex;\n"
        "in texp vec2 texcoord_varying;\n"
        "in lowp vec4 color_varying;\n"
        "void main() {\n"
        "    color_out = texture2D(tex, texcoord_varying) * color_varying;\n"
        "}\n";

    static const uint8_t texture_data[] = {
        0xFF,0x00,0x00,0xFF, 0x00,0xFF,0x00,0xFF,
        0x00,0x00,0xFF,0xFF, 0xFF,0xFF,0xFF,0xFF
    };
    int texture;
    ASSERT(texture = texture_create_with_data(2, 2, texture_data,
                                              TEX_FORMAT_RGBA8888, 2, 0, 0));
    texture_set_antialias(texture, 0);
    texture_apply(0, texture);
    graphics_set_texture_offset(&(Vector2f){0.25, 0.5});

    ASSERT(graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS));
    ASSERT(graphics_add_vertex(&(Vector3f){-0.5, -0.5, 0}, &(Vector2f){0, 0},
                               &(Vector4f){0.333, 0.667, 1.0, 0.2}));
    ASSERT(graphics_add_vertex(&(Vector3f){-0.5, +0.5, 0}, &(Vector2f){0, 1},
                               &(Vector4f){0.333, 0.667, 1.0, 0.2}));
    ASSERT(graphics_add_vertex(&(Vector3f){+0.5, +0.5, 0}, &(Vector2f){1, 1},
                               &(Vector4f){0.333, 0.667, 1.0, 0.2}));
    ASSERT(graphics_add_vertex(&(Vector3f){+0.5, -0.5, 0}, &(Vector2f){1, 0},
                               &(Vector4f){0.333, 0.667, 1.0, 0.2}));
    ASSERT(graphics_end_and_draw_primitive());
    uint8_t pixels[64*64*4];
    ASSERT(graphics_read_pixels(0, 0, 64, 64, pixels));
    for (int i = 0; i < 64*64*4; i += 4) {
        const int x = (i/4) % 64;
        const int y = (i/4) / 64;
        if (x >= 16 && x < 48 && y >= 16 && y < 48) {
            const int r = (((x < 24 || x >= 40) && y >= 32)
                           || ((x >= 24 && x < 40) && y < 32)) ? 0x11 : 0;
            const int g = (x >= 24 && x < 40) ? 0x22 : 0;
            const int b = (y < 32) ? 0x33 : 0;
            CHECK_PIXEL(&pixels[i], r,g,b,255, x, y);
        } else {
            CHECK_PIXEL(&pixels[i], 0,0,0,255, x, y);
        }
    }

    graphics_set_texture_offset(&(Vector2f){0, 0});
    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fixed_color)
{
    vertex_shader_source =
        "in highp vec3 position;\n"
        "void main() {\n"
        "    gl_Position = vec4(position, 1.0);\n"
        "}\n";
    fragment_shader_source =
        "uniform lowp vec4 fixed_color;\n"
        "void main() {\n"
        "    color_out = fixed_color;\n"
        "}\n";

    graphics_set_fixed_color(&(Vector4f){0.667, 1.0, 0.333, 0.6});
    draw_square(0, 1,1,1,1);
    CHECK_SQUARE(0.4, 0.6, 0.2);

    graphics_set_fixed_color(&(Vector4f){1, 1, 1, 1});
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_fog)
{
    /* Rather than performing the actual fog computations, we just modify
     * the vertex positions to reflect the parameter values. */
    vertex_shader_source =
        "uniform highp vec2 fog_params;\n"
        "uniform highp vec4 fog_transform;\n"
        "in highp vec3 position;\n"
        "void main() {\n"
        "    gl_Position = vec4(position.xy * fog_transform.zw + fog_params.xy, position.z, 1.0);\n"
        "}\n";
    fragment_shader_source =
        "uniform lowp vec4 fog_color;\n"
        "void main() {\n"
        "    color_out = fog_color;\n"
        "}\n";

    graphics_set_view_matrix(
        &(Matrix4f){1,0,0,0, 0,1,0,0, 0,0,0.5,0, 0,0,0.75,1});
    graphics_enable_fog(1);
    graphics_set_fog_start(0.25);  // fog_params.x = 1/(2.25-0.25) = 0.5
    graphics_set_fog_end(2.25);    // fog_params.y = 0.25/(2.25-0.25) = 0.125
    graphics_set_fog_color(&(Vector4f){0.667, 1.0, 0.333, 0.6});
    draw_square(0, 1,1,1,1);
    CHECK_COLORED_RECTANGLE(16,24, 48,36, 0.4, 0.6, 0.2);

    graphics_set_view_matrix(&mat4_identity);
    graphics_enable_fog(0);
    graphics_set_fog_start(0);
    graphics_set_fog_end(1);
    graphics_set_fog_color(&(Vector4f){1, 1, 1, 1});
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_alpha_reference)
{
    vertex_shader_source =
        "in highp vec3 position;\n"
        "void main() {\n"
        "    gl_Position = vec4(position, 1.0);\n"
        "}\n";
    fragment_shader_source =
        "uniform lowp float alpha_ref;\n"
        "void main() {\n"
        "    color_out = vec4(0.333, 0.667, 1.0, alpha_ref);\n"
        "}\n";

    graphics_enable_alpha_test(1);
    graphics_set_alpha_test_reference(0.3);
    draw_square(0, 1,1,1,1);
    CHECK_SQUARE(0.1, 0.2, 0.3);

    graphics_enable_alpha_test(0);
    graphics_set_alpha_test_reference(0);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_custom_vertex_attributes)
{
    vertex_shader_source =
        "in highp vec3 position;\n"
        "in lowp vec4 test;\n"
        "out lowp vec4 test_varying;\n"
        "void main() {\n"
        "    gl_Position = vec4(position, 1.0);\n"
        "    test_varying = test;\n"
        "}\n";
    fragment_shader_source =
        "in lowp vec4 test_varying;\n"
        "void main() {\n"
        "    color_out = test_varying;\n"
        "}\n";

    int attrib_test;
    CHECK_TRUE(attrib_test = graphics_add_shader_attribute("test", 4));
    CHECK_TRUE(attrib_test > 0);
    CHECK_TRUE(attrib_test < 0x1000);

    static const float vertex_data[][7] = {
        {-0.5, -0.5, 0, 1.0, 0.333, 0.667, 0.6},
        {-0.5, +0.5, 0, 1.0, 0.333, 0.667, 0.6},
        {+0.5, +0.5, 0, 1.0, 0.333, 0.667, 0.6},
        {+0.5, -0.5, 0, 1.0, 0.333, 0.667, 0.6},
    };
    uint32_t vertex_format[] = {
        GRAPHICS_VERTEX_FORMAT(POSITION_3F, 0),
        GRAPHICS_VERTEX_FORMAT(USER(attrib_test), 3*sizeof(float)),
        /* These should be ignored. */
        GRAPHICS_VERTEX_FORMAT(USER(attrib_test+1), 3*sizeof(float)),
        GRAPHICS_VERTEX_FORMAT(ATTRIB_4F(attrib_test), 0),
        0
    };
    int primitive;
    CHECK_TRUE(primitive = graphics_create_primitive(
                   GRAPHICS_PRIMITIVE_QUADS, vertex_data, vertex_format,
                   sizeof(vertex_data[0]), lenof(vertex_data)));
    graphics_draw_primitive(primitive);
    CHECK_SQUARE(0.6, 0.2, 0.4);

    /* Draw it again to make sure vertex attribute pointers are properly
     * stored in VAO mode. */
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_draw_primitive(primitive);
    CHECK_SQUARE(0.6, 0.2, 0.4);

    graphics_destroy_primitive(primitive);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_custom_vertex_attributes_multiple)
{
    vertex_shader_source =
        "in highp vec3 position;\n"
        "in highp float test1;\n"
        "in highp vec2 test2;\n"
        "in highp vec3 test3;\n"
        "in highp vec4 test5;\n"
        "out lowp vec4 test_varying;\n"
        "void main() {\n"
        "    gl_Position = vec4(position, 1.0);\n"
        "    test_varying = vec4(test1, 0.0, test2) + vec4(test3, 0.0) + test5;\n"
        "}\n";
    fragment_shader_source =
        "in lowp vec4 test_varying;\n"
        "void main() {\n"
        "    color_out = test_varying;\n"
        "}\n";

    int attrib_test1, attrib_test2, attrib_test3, attrib_test4, attrib_test5;
    CHECK_TRUE(attrib_test1 = graphics_add_shader_attribute("test1", 1));
    CHECK_TRUE(attrib_test2 = graphics_add_shader_attribute("test2", 2));
    CHECK_TRUE(attrib_test3 = graphics_add_shader_attribute("test3", 3));
    CHECK_TRUE(attrib_test4 = graphics_add_shader_attribute("test4", 4));
    CHECK_TRUE(attrib_test5 = graphics_add_shader_attribute("test5", 4));

    static const float vertex_data[][13] = {
        {-0.5, -0.5, 0, 0.3, 0.033, 0.067, 0.2, 0.2, 0.1, 0.1, 0.6, 0.3, 0.4},
        {-0.5, +0.5, 0, 0.3, 0.033, 0.067, 0.2, 0.2, 0.1, 0.1, 0.6, 0.3, 0.4},
        {+0.5, +0.5, 0, 0.3, 0.033, 0.067, 0.2, 0.2, 0.1, 0.1, 0.6, 0.3, 0.4},
        {+0.5, -0.5, 0, 0.3, 0.033, 0.067, 0.2, 0.2, 0.1, 0.1, 0.6, 0.3, 0.4},
    };
    uint32_t vertex_format[] = {
        GRAPHICS_VERTEX_FORMAT(POSITION_3F, 0),
        GRAPHICS_VERTEX_FORMAT(USER(attrib_test5), 3*sizeof(float)),
        GRAPHICS_VERTEX_FORMAT(USER(attrib_test2), 7*sizeof(float)),
        GRAPHICS_VERTEX_FORMAT(USER(attrib_test1), 9*sizeof(float)),
        GRAPHICS_VERTEX_FORMAT(USER(attrib_test3), 10*sizeof(float)),
        0
    };
    int primitive;
    CHECK_TRUE(primitive = graphics_create_primitive(
                   GRAPHICS_PRIMITIVE_QUADS, vertex_data, vertex_format,
                   sizeof(vertex_data[0]), lenof(vertex_data)));
    graphics_draw_primitive(primitive);
    CHECK_SQUARE(0.3, 0.1, 0.2);

    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_draw_primitive(primitive);
    CHECK_SQUARE(0.3, 0.1, 0.2);

    graphics_destroy_primitive(primitive);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_custom_vertex_attributes_memory_failure)
{
    vertex_shader_source =
        "in highp vec3 position;\n"
        "in lowp vec4 test;\n"
        "out lowp vec4 test_varying;\n"
        "void main() {\n"
        "    gl_Position = vec4(position, 1.0);\n"
        "    test_varying = test;\n"
        "}\n";
    fragment_shader_source =
        "in lowp vec4 test_varying;\n"
        "void main() {\n"
        "    color_out = test_varying;\n"
        "}\n";

    int attrib_test;
    CHECK_MEMORY_FAILURES(
        (attrib_test = graphics_add_shader_attribute("test", 4))
        /* Avoid false leak reports from persistent arrays being expanded. */
        || (TEST_mem_fail_after(-1, 0, 0),
            graphics_set_shader_generator(generate_vertex_shader_source,
                                          generate_fragment_shader_source,
                                          generate_shader_key, 1, 0),
            0));

    static const float vertex_data[][7] = {
        {-0.5, -0.5, 0, 1.0, 0.333, 0.667, 0.6},
        {-0.5, +0.5, 0, 1.0, 0.333, 0.667, 0.6},
        {+0.5, +0.5, 0, 1.0, 0.333, 0.667, 0.6},
        {+0.5, -0.5, 0, 1.0, 0.333, 0.667, 0.6},
    };
    uint32_t vertex_format[] = {
        GRAPHICS_VERTEX_FORMAT(POSITION_3F, 0),
        GRAPHICS_VERTEX_FORMAT(USER(attrib_test), 3*sizeof(float)),
        0
    };
    int primitive;
    CHECK_MEMORY_FAILURES(primitive = graphics_create_primitive(
                              GRAPHICS_PRIMITIVE_QUADS,
                              vertex_data, vertex_format,
                              sizeof(vertex_data[0]), lenof(vertex_data)));
    graphics_draw_primitive(primitive);
    CHECK_SQUARE(0.6, 0.2, 0.4);

    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_draw_primitive(primitive);
    CHECK_SQUARE(0.6, 0.2, 0.4);

    graphics_destroy_primitive(primitive);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_custom_vertex_attributes_invalid)
{
    CHECK_FALSE(graphics_add_shader_attribute(NULL, 1));
    CHECK_FALSE(graphics_add_shader_attribute("", 1));
    CHECK_FALSE(graphics_add_shader_attribute("test", 0));
    CHECK_FALSE(graphics_add_shader_attribute("test", 5));

    CHECK_FALSE(graphics_add_shader_attribute("transform", 1));
    CHECK_FALSE(graphics_add_shader_attribute("tex", 1));
    CHECK_FALSE(graphics_add_shader_attribute("tex_offset", 1));
    CHECK_FALSE(graphics_add_shader_attribute("fixed_color", 1));
    CHECK_FALSE(graphics_add_shader_attribute("fog_params", 1));
    CHECK_FALSE(graphics_add_shader_attribute("fog_color", 1));
    CHECK_FALSE(graphics_add_shader_attribute("alpha_ref", 1));
    CHECK_FALSE(graphics_add_shader_attribute("position", 1));
    CHECK_FALSE(graphics_add_shader_attribute("texcoord", 1));
    CHECK_FALSE(graphics_add_shader_attribute("color", 1));
#ifdef USES_GL
    CHECK_FALSE(graphics_add_shader_attribute("gl_test", 1));
#endif

    CHECK_TRUE(graphics_add_shader_attribute("test", 1));
    CHECK_FALSE(graphics_add_shader_attribute("test", 1));

    const int USER_ATTRIBS_MAX = 4095;
    for (int i = 2; i <= USER_ATTRIBS_MAX + 1; i++) {
        char buf[10];
        ASSERT(strformat_check(buf, sizeof(buf), "test%d", i));
        const int attrib = graphics_add_shader_attribute(buf, 1);
        if (!attrib) {
            break;
        }
        CHECK_TRUE(i <= USER_ATTRIBS_MAX);
        CHECK_TRUE(attrib > 0);
        CHECK_TRUE(attrib < 0x1000);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_custom_vertex_attributes_clear)
{
    CHECK_TRUE(graphics_add_shader_attribute("test", 1));

    /* Calling graphics_set_shader_generator() should implicitly clear all
     * defined attributes. */
    CHECK_TRUE(graphics_set_shader_generator(generate_vertex_shader_source,
                                             generate_fragment_shader_source,
                                             generate_shader_key, 1, 0));
    /* This should now succeed since the previously defined attribute has
     * been cleared. */
    CHECK_TRUE(graphics_add_shader_attribute("test", 1));

    /* Similarly, calling graphics_use_shader_objects() should clear all
     * defined attributes. */
    CHECK_TRUE(graphics_use_shader_objects(0));
    CHECK_TRUE(graphics_add_shader_attribute("test", 1));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_custom_uniforms)
{
    vertex_shader_source =
        "uniform highp vec4 test_vertex_uniform;\n"
        "in highp vec3 position;\n"
        "void main() {\n"
        "    gl_Position = vec4(position, 1.0) + test_vertex_uniform;\n"
        "}\n";
    fragment_shader_source =
        "uniform lowp vec4 test_fragment_uniform;\n"
        "void main() {\n"
        "    color_out = test_fragment_uniform;\n"
        "}\n";

    int uniform_vertex, uniform_fragment;
    CHECK_TRUE(uniform_vertex =
                   graphics_add_shader_uniform("test_vertex_uniform"));
    CHECK_TRUE(uniform_fragment =
                   graphics_add_shader_uniform("test_fragment_uniform"));

    graphics_set_shader_uniform_vec4(
        uniform_vertex, &(Vector4f){0.25, 0.5, 0, 0});
    graphics_set_shader_uniform_vec4(
        uniform_fragment, &(Vector4f){0.333, 1.0, 0.667, 0.6});
    draw_square(0, 1,1,1,1);
    CHECK_COLORED_RECTANGLE(32,32, 40,48, 0.2, 0.6, 0.4);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_custom_uniforms_all_types)
{
    vertex_shader_source =
        "uniform highp float test_float;\n"
        "uniform highp vec2 test_vec2;\n"
        "uniform highp vec3 test_vec3;\n"
        "uniform highp mat4 test_mat4;\n"
        "in highp vec3 position;\n"
        "void main() {\n"
        "    highp vec3 final_position = position + (vec3(test_vec2, 0.0) * test_float) + test_vec3;\n"
        "    gl_Position = vec4(final_position, 1.0) * test_mat4;\n"
        "}\n";
    fragment_shader_source =
        "uniform lowp int test_int;\n"
        "uniform lowp vec4 test_vec4;\n"
        "void main() {\n"
        "    if (test_int > 0) {\n"
        "        color_out = test_vec4;\n"
        "    } else {\n"
        "        color_out = vec4(1.0, 0.0, 1.0, 0.8);\n"
        "    }\n"
        "}\n";

    int uniform_int, uniform_float, uniform_vec2, uniform_vec3;
    int uniform_vec4, uniform_mat4;
    CHECK_TRUE(uniform_int = graphics_add_shader_uniform("test_int"));
    CHECK_TRUE(uniform_float = graphics_add_shader_uniform("test_float"));
    CHECK_TRUE(uniform_vec2 = graphics_add_shader_uniform("test_vec2"));
    CHECK_TRUE(uniform_vec3 = graphics_add_shader_uniform("test_vec3"));
    CHECK_TRUE(uniform_vec4 = graphics_add_shader_uniform("test_vec4"));
    CHECK_TRUE(uniform_mat4 = graphics_add_shader_uniform("test_mat4"));

    graphics_set_shader_uniform_int(uniform_int, 0);
    graphics_set_shader_uniform_float(uniform_float, -0.5);
    graphics_set_shader_uniform_vec2(uniform_vec2, &(Vector2f){1.0, 1.25});
    graphics_set_shader_uniform_vec3(uniform_vec3, &(Vector3f){0.25, 0.5, 0});
    graphics_set_shader_uniform_vec4(
        uniform_vec4, &(Vector4f){0.333, 1.0, 0.667, 0.3});
    graphics_set_shader_uniform_mat4(
        uniform_mat4, &(Matrix4f){0.5,0,0,0, 0,0.5,0,0, 0,0,1,0, 0.5,0.5,0,1});
    draw_square(0, 1,1,1,1);
    CHECK_COLORED_RECTANGLE(16,16, 44,46, 0.8, 0.0, 0.8);

    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_set_shader_uniform_int(uniform_int, 1);
    graphics_set_shader_uniform_float(uniform_float, 0.5);
    graphics_set_shader_uniform_vec2(uniform_vec2, &(Vector2f){-1.5, -1.75});
    graphics_set_shader_uniform_vec3(uniform_vec3,
                                     &(Vector3f){0.125, 0.375, 0});
    graphics_set_shader_uniform_vec4(
        uniform_vec4, &(Vector4f){1.0, 0.667, 0.333, 0.6});
    graphics_set_shader_uniform_mat4(
        uniform_mat4, &(Matrix4f){0.5,0,0,0, 0,0.5,0,0, 0,0,1,0, 0,0,0,1});
    draw_square(0, 1,1,1,1);
    CHECK_COLORED_RECTANGLE(16,16, 22,24, 0.6, 0.4, 0.2);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_custom_uniforms_wrong_type)
{
    vertex_shader_source =
        "uniform highp vec3 test_vertex_uniform;\n"
        "in highp vec3 position;\n"
        "void main() {\n"
        "    gl_Position = vec4(position + test_vertex_uniform, 1.0);\n"
        "}\n";
    fragment_shader_source =
        "uniform lowp vec4 test_fragment_uniform;\n"
        "void main() {\n"
        "    color_out = test_fragment_uniform;\n"
        "}\n";

    int uniform_vertex, uniform_fragment;
    CHECK_TRUE(uniform_vertex =
                   graphics_add_shader_uniform("test_vertex_uniform"));
    CHECK_TRUE(uniform_fragment =
                   graphics_add_shader_uniform("test_fragment_uniform"));

    graphics_set_shader_uniform_vec3(
        uniform_vertex, &(Vector3f){0.25, 0.5, 0});
    graphics_set_shader_uniform_vec4(
        uniform_fragment, &(Vector4f){0.333, 1.0, 0.667, 0.6});

    /* None of these should succeed. */
    graphics_set_shader_uniform_int(uniform_vertex, 1);
    graphics_set_shader_uniform_float(uniform_vertex, 1);
    graphics_set_shader_uniform_vec2(uniform_vertex, &(Vector2f){1,1});
    graphics_set_shader_uniform_vec4(uniform_vertex, &(Vector4f){1,1,1,1});
    graphics_set_shader_uniform_mat4(uniform_vertex, &mat4_identity);
    graphics_set_shader_uniform_int(uniform_fragment, 1);
    graphics_set_shader_uniform_float(uniform_fragment, 1);
    graphics_set_shader_uniform_vec2(uniform_fragment, &(Vector2f){1,1});
    graphics_set_shader_uniform_vec3(uniform_fragment, &(Vector3f){1,1,1});
    graphics_set_shader_uniform_mat4(uniform_fragment, &mat4_identity);

    draw_square(0, 1,1,1,1);
    CHECK_COLORED_RECTANGLE(32,32, 40,48, 0.2, 0.6, 0.4);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_custom_uniforms_defined_late)
{
    vertex_shader_source =
        "uniform highp vec4 test_vertex_uniform;\n"
        "in highp vec3 position;\n"
        "void main() {\n"
        "    gl_Position = vec4(position, 1.0) + test_vertex_uniform;\n"
        "}\n";
    fragment_shader_source =
        "uniform lowp vec4 test_fragment_uniform;\n"
        "void main() {\n"
        "    color_out = vec4(\n"
        "        (test_fragment_uniform.xyz * test_fragment_uniform.w)\n"
        "        + vec3(0.2, 0.2, 0.2), 1.0);\n"
        "}\n";

    int uniform_vertex, uniform_fragment;

    CHECK_TRUE(uniform_vertex =
                   graphics_add_shader_uniform("test_vertex_uniform"));
    graphics_set_shader_uniform_vec4(
        uniform_vertex, &(Vector4f){0.25, 0.5, 0, 0});
    draw_square(0, 1,1,1,1);
    CHECK_COLORED_RECTANGLE(32,32, 40,48, 0.2, 0.2, 0.2);

    /* This definition should have no effect on the existing shader. */
    CHECK_TRUE(uniform_fragment =
                   graphics_add_shader_uniform("test_fragment_uniform"));
    graphics_set_shader_uniform_vec4(
        uniform_fragment, &(Vector4f){0.333, 1.0, 0.667, 0.6});
    ASSERT(graphics_set_blend(GRAPHICS_BLEND_ADD, 1, 1));
    draw_square(0, 1,1,1,1);
    ASSERT(graphics_set_blend(GRAPHICS_BLEND_ADD,
                              GRAPHICS_BLEND_SRC_ALPHA,
                              GRAPHICS_BLEND_INV_SRC_ALPHA));
    CHECK_COLORED_RECTANGLE(32,32, 40,48, 0.4, 0.4, 0.4);

    /* Call all other type functions as well to make sure they don't crash. */
    int uniform_int, uniform_float, uniform_vec2, uniform_vec3;
    int uniform_mat4;
    CHECK_TRUE(uniform_int = graphics_add_shader_uniform("test_int"));
    CHECK_TRUE(uniform_float = graphics_add_shader_uniform("test_float"));
    CHECK_TRUE(uniform_vec2 = graphics_add_shader_uniform("test_vec2"));
    CHECK_TRUE(uniform_vec3 = graphics_add_shader_uniform("test_vec3"));
    CHECK_TRUE(uniform_mat4 = graphics_add_shader_uniform("test_mat4"));
    graphics_set_shader_uniform_int(uniform_int, 1);
    graphics_set_shader_uniform_float(uniform_float, 1);
    graphics_set_shader_uniform_vec2(uniform_vec2, &(Vector2f){1,1});
    graphics_set_shader_uniform_vec3(uniform_vec3, &(Vector3f){1,1,1});
    graphics_set_shader_uniform_mat4(uniform_mat4, &mat4_identity);
    ASSERT(graphics_set_blend(GRAPHICS_BLEND_ADD, 1, 1));
    draw_square(0, 1,1,1,1);
    ASSERT(graphics_set_blend(GRAPHICS_BLEND_ADD,
                              GRAPHICS_BLEND_SRC_ALPHA,
                              GRAPHICS_BLEND_INV_SRC_ALPHA));
    CHECK_COLORED_RECTANGLE(32,32, 40,48, 0.6, 0.6, 0.6);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_custom_uniforms_memory_failure)
{
    vertex_shader_source =
        "uniform highp vec4 test_vertex_uniform;\n"
        "in highp vec3 position;\n"
        "void main() {\n"
        "    gl_Position = vec4(position, 1.0) + test_vertex_uniform;\n"
        "}\n";
    fragment_shader_source =
        "uniform lowp vec4 test_fragment_uniform;\n"
        "void main() {\n"
        "    color_out = test_fragment_uniform;\n"
        "}\n";

    int uniform_vertex, uniform_fragment;
    CHECK_MEMORY_FAILURES(
        uniform_vertex = graphics_add_shader_uniform("test_vertex_uniform"));
    CHECK_MEMORY_FAILURES(
        uniform_fragment = graphics_add_shader_uniform("test_fragment_uniform"));

    graphics_set_shader_uniform_vec4(
        uniform_vertex, &(Vector4f){0.25, 0.5, 0, 0});
    graphics_set_shader_uniform_vec4(
        uniform_fragment, &(Vector4f){0.333, 1.0, 0.667, 0.6});
    draw_square(0, 1,1,1,1);
    CHECK_COLORED_RECTANGLE(32,32, 40,48, 0.2, 0.6, 0.4);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_custom_uniforms_invalid)
{
    vertex_shader_source =
        "uniform highp vec3 test_vertex_uniform;\n"
        "in highp vec3 position;\n"
        "void main() {\n"
        "    gl_Position = vec4(position + test_vertex_uniform, 1.0);\n"
        "}\n";
    fragment_shader_source =
        "uniform lowp vec4 test_fragment_uniform;\n"
        "void main() {\n"
        "    color_out = test_fragment_uniform;\n"
        "}\n";

    CHECK_FALSE(graphics_add_shader_uniform(NULL));
    CHECK_FALSE(graphics_add_shader_uniform(""));

    CHECK_FALSE(graphics_add_shader_uniform("transform"));
    CHECK_FALSE(graphics_add_shader_uniform("tex"));
    CHECK_FALSE(graphics_add_shader_uniform("tex_offset"));
    CHECK_FALSE(graphics_add_shader_uniform("fixed_color"));
    CHECK_FALSE(graphics_add_shader_uniform("fog_params"));
    CHECK_FALSE(graphics_add_shader_uniform("fog_color"));
    CHECK_FALSE(graphics_add_shader_uniform("alpha_ref"));
    CHECK_FALSE(graphics_add_shader_uniform("position"));
    CHECK_FALSE(graphics_add_shader_uniform("texcoord"));
    CHECK_FALSE(graphics_add_shader_uniform("color"));
#ifdef USES_GL
    CHECK_FALSE(graphics_add_shader_uniform("gl_test"));
#endif

    CHECK_TRUE(graphics_add_shader_uniform("test"));
    CHECK_FALSE(graphics_add_shader_uniform("test"));

    int uniform_vertex, uniform_fragment;
    CHECK_TRUE(uniform_vertex =
                   graphics_add_shader_uniform("test_vertex_uniform"));
    CHECK_TRUE(uniform_fragment =
                   graphics_add_shader_uniform("test_fragment_uniform"));
    graphics_set_shader_uniform_vec3(
        uniform_vertex, &(Vector3f){0.25, 0.5, 0});
    graphics_set_shader_uniform_vec4(
        uniform_fragment, &(Vector4f){0.333, 1.0, 0.667, 0.6});

    /* None of these should succeed. */
    const int uniform_invalid = max(uniform_vertex, uniform_fragment) + 1;
    ASSERT(uniform_invalid != 0);
    graphics_set_shader_uniform_int(0, 1);
    graphics_set_shader_uniform_float(0, 1);
    graphics_set_shader_uniform_vec2(0, &(Vector2f){1,1});
    graphics_set_shader_uniform_vec3(0, &(Vector3f){1,1,1});
    graphics_set_shader_uniform_vec4(0, &(Vector4f){1,1,1,1});
    graphics_set_shader_uniform_mat4(0, &mat4_identity);
    graphics_set_shader_uniform_int(-1, 1);
    graphics_set_shader_uniform_float(-1, 1);
    graphics_set_shader_uniform_vec2(-1, &(Vector2f){1,1});
    graphics_set_shader_uniform_vec3(-1, &(Vector3f){1,1,1});
    graphics_set_shader_uniform_vec4(-1, &(Vector4f){1,1,1,1});
    graphics_set_shader_uniform_mat4(-1, &mat4_identity);
    graphics_set_shader_uniform_int(uniform_invalid, 1);
    graphics_set_shader_uniform_float(uniform_invalid, 1);
    graphics_set_shader_uniform_vec2(uniform_invalid, &(Vector2f){1,1});
    graphics_set_shader_uniform_vec3(uniform_invalid, &(Vector3f){1,1,1});
    graphics_set_shader_uniform_vec4(uniform_invalid, &(Vector4f){1,1,1,1});
    graphics_set_shader_uniform_mat4(uniform_invalid, &mat4_identity);
    graphics_set_shader_uniform_vec2(uniform_vertex, NULL);
    graphics_set_shader_uniform_vec3(uniform_vertex, NULL);
    graphics_set_shader_uniform_vec4(uniform_vertex, NULL);
    graphics_set_shader_uniform_mat4(uniform_vertex, NULL);

    draw_square(0, 1,1,1,1);
    CHECK_COLORED_RECTANGLE(32,32, 40,48, 0.2, 0.6, 0.4);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_custom_uniform_attribute_collision)
{
    CHECK_TRUE(graphics_add_shader_uniform("test_uniform"));
    CHECK_TRUE(graphics_add_shader_attribute("test_attribute", 1));

    CHECK_FALSE(graphics_add_shader_uniform("test_attribute"));
    CHECK_FALSE(graphics_add_shader_attribute("test_uniform", 1));

    return 1;
}

/*************************************************************************/
/*************************************************************************/
