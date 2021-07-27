/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/graphics/shader.c: Shader tests.
 */

#include "src/base.h"
#include "src/graphics.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/shader.h"
#include "src/test/base.h"
#include "src/test/graphics/internal.h"
#include "src/texture.h"
#include "src/thread.h"

#ifdef USES_GL
# include "src/sysdep/opengl/opengl.h"
#endif

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Shader ID guaranteed to be invalid across all tests. */
#define INVALID_SHADER  10000

/*-----------------------------------------------------------------------*/

/* Basic shaders with only a position attribute. */
static const char vs_position[] =
    "in highp vec3 position;\n"
    "void main() {\n"
    "    gl_Position = vec4(position, 1.0);\n"
    "}\n";
static const char fs_position[] =
    "void main() {\n"
    "    color_out = vec4(0.333, 0.667, 1.0, 0.6);\n"
    "}\n";

/* Basic shaders with position and color attributes. */
static const char vs_position_color[] =
    "in highp vec3 position;\n"
    "in lowp vec4 color;\n"
    "out lowp vec4 color_varying;\n"
    "void main() {\n"
    "    gl_Position = vec4(position, 1.0);\n"
    "    color_varying = color;\n"
    "}\n";
static const char fs_position_color[] =
    "in lowp vec4 color_varying;\n"
    "void main() {\n"
    "    color_out = color_varying;\n"
    "}\n";

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

/**
 * get_binary_supported:  Return whether the system supports retrieving
 * compiled binary data for a shader.
 */
static int get_binary_supported(void)
{
#ifdef USES_GL
    return opengl_has_features(OPENGL_FEATURE_SEPARATE_SHADERS
                             | OPENGL_FEATURE_SHADER_BINARIES);
#else
    return 0;
#endif
}

/*-----------------------------------------------------------------------*/

/**
 * draw_shader_square:  Draw a square from (-0.5,-0.5) to (+0.5,+0.5) at
 * the given Z coordinate with the given color, passing the vertex position
 * in attribute index 0 and the color in attribute index 2.
 *
 * [Parameters]
 *     z: Depth value (-1 through +1).
 *     r, g, b, a: Color components.
 */
static void draw_shader_square(float z, float r, float g, float b, float a)
{
    const struct {float x, y, z, r, g, b, a;} vertices[] = {
        {-0.5, -0.5, z, r, g, b, a},
        {-0.5, +0.5, z, r, g, b, a},
        {+0.5, +0.5, z, r, g, b, a},
        {+0.5, -0.5, z, r, g, b, a},
    };
    static const uint32_t vertex_format[] = {
        GRAPHICS_VERTEX_FORMAT(ATTRIB_3F(0), 0),
        GRAPHICS_VERTEX_FORMAT(ATTRIB_4F(2), 3*sizeof(float)),
        0
    };

    graphics_draw_vertices(GRAPHICS_PRIMITIVE_QUADS, vertices, vertex_format,
                           sizeof(*vertices), lenof(vertices));
}

/*-----------------------------------------------------------------------*/

/**
 * compile_thread:  Thread routine to compile a shader.
 *
 * [Parameters]
 *     data_ret: Pointer to variable to receive the allocated data pointer.
 * [Return value]
 *     Size of data, in bytes.
 */
static int compile_thread(void *data_ret_)
{
    void **data_ret = data_ret_;
    int size;
    CHECK_TRUE(*data_ret = shader_compile_to_binary(
                   SHADER_TYPE_VERTEX, vs_position_color, -1, &size));
    return size;
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

static int do_test_graphics_shader_obj(void);

int test_graphics_shader_obj(void)
{
#ifdef SIL_PLATFORM_PSP
    SKIP("Shaders not supported on PSP.");
#endif

    const int result = run_tests_in_window(do_test_graphics_shader_obj);
    return result;
}

DEFINE_GENERIC_TEST_RUNNER(do_test_graphics_shader_obj)

/*-----------------------------------------------------------------------*/

TEST_INIT(init)
{
    graphics_set_viewport(0, 0, 64, 64);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_use_shader_objects(1));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
    graphics_finish_frame();
    shader_clear_attributes();
    CHECK_TRUE(graphics_use_shader_objects(0));
    graphics_flush_resources();

    return 1;
}

/*************************************************************************/
/********************* Test routines: Shader objects *********************/
/*************************************************************************/

TEST(test_basic_shader)
{
    CHECK_TRUE(shader_set_attribute(0, "position"));

    int vertex_shader, fragment_shader, pipeline;
    /* Add a junk character at the end of the string to verify that an
     * explicit string length is respected. */
    char buf[1000];
    ASSERT(strformat_check(buf, sizeof(buf), "%s}", vs_position));
    CHECK_TRUE(vertex_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, buf, strlen(buf)-1));
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT, fs_position, -1));
    CHECK_TRUE(pipeline = shader_pipeline_create(
                   vertex_shader, fragment_shader));

    shader_pipeline_apply(pipeline);
    draw_shader_square(0, 1,1,1,1);
    CHECK_SQUARE(0.2, 0.4, 0.6);

    shader_pipeline_destroy(pipeline);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_basic_shader_no_trailing_newline)
{
    CHECK_TRUE(shader_set_attribute(0, "position"));

    int vertex_shader, fragment_shader, pipeline;
    CHECK_TRUE(vertex_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX,
                   "in highp vec3 position;\n"
                   "void main() {\n"
                   "    gl_Position = vec4(position, 1.0);\n"
                   "}", -1));
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT,
                   "void main() {\n"
                   "    color_out = vec4(0.333, 0.667, 1.0, 0.6);\n"
                   "}", -1));
    CHECK_TRUE(pipeline = shader_pipeline_create(
                   vertex_shader, fragment_shader));

    shader_pipeline_apply(pipeline);
    draw_shader_square(0, 1,1,1,1);
    CHECK_SQUARE(0.2, 0.4, 0.6);

    shader_pipeline_destroy(pipeline);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_destroy_invalid)
{
    int shader;
    CHECK_TRUE(shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT,
                   "void main() {\n"
                   "    color_out = vec4(0.333, 0.667, 1.0, 0.6);\n"
                   "}\n", -1));
    shader_destroy(shader);

    shader_destroy(shader);  // Should log an error but not crash.
    shader_destroy(INVALID_SHADER);  // Should log an error but not crash.
    shader_destroy(0);  // Should not crash (defined as a no-op).
    shader_destroy(-1);  // Should log an error but not crash.

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_multiple_attributes)
{
    CHECK_TRUE(shader_set_attribute(0, "position"));
    CHECK_TRUE(shader_set_attribute(2, "color"));

    int vertex_shader, fragment_shader, pipeline;
    CHECK_TRUE(vertex_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_position_color, -1));
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT, fs_position_color, -1));
    CHECK_TRUE(pipeline = shader_pipeline_create(
                   vertex_shader, fragment_shader));

    shader_pipeline_apply(pipeline);
    draw_shader_square(0, 0.667, 1.0, 0.333, 0.6);
    CHECK_SQUARE(0.4, 0.6, 0.2);

    shader_pipeline_destroy(pipeline);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_from_source_memory_failure)
{
    CHECK_TRUE(shader_set_attribute(0, "position"));
    CHECK_TRUE(shader_set_attribute(2, "color"));

    int vertex_shader, fragment_shader, pipeline;
    CHECK_MEMORY_FAILURES(vertex_shader = shader_create_from_source(
                              SHADER_TYPE_VERTEX, vs_position_color, -1));
    CHECK_MEMORY_FAILURES(fragment_shader = shader_create_from_source(
                              SHADER_TYPE_FRAGMENT, fs_position_color, -1));
    CHECK_TRUE(pipeline = shader_pipeline_create(
                   vertex_shader, fragment_shader));

    shader_pipeline_apply(pipeline);
    draw_shader_square(0, 0.667, 1.0, 0.333, 0.6);
    CHECK_SQUARE(0.4, 0.6, 0.2);

    shader_pipeline_destroy(pipeline);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_from_source_invalid)
{
    CHECK_FALSE(shader_create_from_source(-1, vs_position_color, -1));
    CHECK_FALSE(shader_create_from_source(SHADER_TYPE_VERTEX, NULL, -1));
    CHECK_FALSE(shader_create_from_source(SHADER_TYPE_FRAGMENT, "", -1));
    CHECK_FALSE(shader_create_from_source(SHADER_TYPE_FRAGMENT, "foo", 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_create_binary)
{
    if (!get_binary_supported()) {
        SKIP("Unable to retrieve compiled shader data on this system.");
    }

    CHECK_TRUE(shader_set_attribute(0, "position"));
    CHECK_TRUE(shader_set_attribute(2, "color"));

    int vertex_shader, fragment_shader, pipeline;
    CHECK_TRUE(vertex_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_position_color, -1));
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT, fs_position_color, -1));

    void *vs_data, *fs_data;
    int vs_size = 0, fs_size = 0;
    CHECK_TRUE(vs_data = shader_get_binary(vertex_shader, &vs_size));
    CHECK_TRUE(vs_size > 0);
    CHECK_TRUE(fs_data = shader_get_binary(fragment_shader, &fs_size));
    CHECK_TRUE(fs_size > 0);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);

    CHECK_TRUE(vertex_shader = shader_create_from_binary(
                   SHADER_TYPE_VERTEX, vs_data, vs_size));
    mem_free(vs_data);
    CHECK_TRUE(fragment_shader = shader_create_from_binary(
                   SHADER_TYPE_FRAGMENT, fs_data, fs_size));
    mem_free(fs_data);
    CHECK_TRUE(pipeline = shader_pipeline_create(
                   vertex_shader, fragment_shader));

    shader_pipeline_apply(pipeline);
    draw_shader_square(0, 0.667, 1.0, 0.333, 0.6);
    CHECK_SQUARE(0.4, 0.6, 0.2);

    shader_pipeline_destroy(pipeline);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_create_binary_memory_failures)
{
    if (!get_binary_supported()) {
        SKIP("Unable to retrieve compiled shader data on this system.");
    }

    CHECK_TRUE(shader_set_attribute(0, "position"));
    CHECK_TRUE(shader_set_attribute(2, "color"));

    int vertex_shader, fragment_shader, pipeline;
    CHECK_TRUE(vertex_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_position_color, -1));
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT, fs_position_color, -1));

    void *vs_data, *fs_data;
    int vs_size = 0, fs_size = 0;
    CHECK_MEMORY_FAILURES(vs_data = shader_get_binary(vertex_shader,
                                                      &vs_size));
    CHECK_TRUE(vs_size > 0);
    CHECK_MEMORY_FAILURES(fs_data = shader_get_binary(fragment_shader,
                                                      &fs_size));
    CHECK_TRUE(fs_size > 0);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);

    CHECK_MEMORY_FAILURES(vertex_shader = shader_create_from_binary(
                              SHADER_TYPE_VERTEX, vs_data, vs_size));
    mem_free(vs_data);
    CHECK_MEMORY_FAILURES(fragment_shader = shader_create_from_binary(
                              SHADER_TYPE_FRAGMENT, fs_data, fs_size));
    mem_free(fs_data);
    CHECK_TRUE(pipeline = shader_pipeline_create(
                   vertex_shader, fragment_shader));

    shader_pipeline_apply(pipeline);
    draw_shader_square(0, 0.667, 1.0, 0.333, 0.6);
    CHECK_SQUARE(0.4, 0.6, 0.2);

    shader_pipeline_destroy(pipeline);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_create_binary_invalid)
{
    if (!get_binary_supported()) {
        SKIP("Unable to retrieve compiled shader data on this system.");
    }

    CHECK_TRUE(shader_set_attribute(0, "position"));
    CHECK_TRUE(shader_set_attribute(2, "color"));

    int shader;
    CHECK_TRUE(shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_position_color, -1));

    void *vs_data;
    int vs_size = 0;
    CHECK_TRUE(vs_data = shader_get_binary(shader, &vs_size));
    CHECK_TRUE(vs_size > 0);
    CHECK_FALSE(shader_create_from_binary(-1, vs_data, vs_size));
    CHECK_FALSE(shader_create_from_binary(SHADER_TYPE_VERTEX, NULL, vs_size));
    CHECK_FALSE(shader_create_from_binary(SHADER_TYPE_VERTEX, vs_data, 0));
    CHECK_FALSE(shader_create_from_binary(SHADER_TYPE_VERTEX, vs_data, -1));
    mem_free(vs_data);

    vs_size = -123;
    CHECK_FALSE(shader_get_binary(shader, NULL));
    CHECK_FALSE(shader_get_binary(0, &vs_size));
    CHECK_FALSE(shader_get_binary(INVALID_SHADER, &vs_size));
    shader_destroy(shader);
    CHECK_FALSE(shader_get_binary(shader, &vs_size));
    CHECK_INTEQUAL(vs_size, -123);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_compile)
{
    if (!get_binary_supported()) {
        SKIP("Unable to compile shaders to binary data on this system.");
    }

    CHECK_TRUE(shader_set_attribute(0, "position"));
    CHECK_TRUE(shader_set_attribute(2, "color"));

    void *vs_data, *fs_data;
    int vs_size = 0, fs_size = 0;
    /* Add a junk character at the end of the string to verify that an
     * explicit string length is respected. */
    char buf[1000];
    ASSERT(strformat_check(buf, sizeof(buf), "%s}", vs_position_color));
    CHECK_TRUE(vs_data = shader_compile_to_binary(
                   SHADER_TYPE_VERTEX, buf, strlen(buf)-1, &vs_size));
    CHECK_TRUE(vs_size > 0);
    CHECK_TRUE(fs_data = shader_compile_to_binary(
                   SHADER_TYPE_FRAGMENT, fs_position_color, -1, &fs_size));
    CHECK_TRUE(fs_size > 0);

    int vertex_shader, fragment_shader, pipeline;
    CHECK_TRUE(vertex_shader = shader_create_from_binary(
                   SHADER_TYPE_VERTEX, vs_data, vs_size));
    mem_free(vs_data);
    CHECK_TRUE(fragment_shader = shader_create_from_binary(
                   SHADER_TYPE_FRAGMENT, fs_data, fs_size));
    mem_free(fs_data);
    CHECK_TRUE(pipeline = shader_pipeline_create(
                   vertex_shader, fragment_shader));

    shader_pipeline_apply(pipeline);
    draw_shader_square(0, 0.667, 1.0, 0.333, 0.6);
    CHECK_SQUARE(0.4, 0.6, 0.2);

    shader_pipeline_destroy(pipeline);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_compile_memory_failure)
{
    if (!get_binary_supported()) {
        SKIP("Unable to compile shaders to binary data on this system.");
    }

    CHECK_TRUE(shader_set_attribute(0, "position"));
    CHECK_TRUE(shader_set_attribute(2, "color"));

    void *vs_data, *fs_data;
    int vs_size = 0, fs_size = 0;
    CHECK_MEMORY_FAILURES(vs_data = shader_compile_to_binary(
                              SHADER_TYPE_VERTEX, vs_position_color, -1,
                              &vs_size));
    CHECK_TRUE(vs_size > 0);
    CHECK_MEMORY_FAILURES(fs_data = shader_compile_to_binary(
                              SHADER_TYPE_FRAGMENT, fs_position_color, -1,
                              &fs_size));
    CHECK_TRUE(fs_size > 0);

    int vertex_shader, fragment_shader, pipeline;
    CHECK_TRUE(vertex_shader = shader_create_from_binary(
                   SHADER_TYPE_VERTEX, vs_data, vs_size));
    mem_free(vs_data);
    CHECK_TRUE(fragment_shader = shader_create_from_binary(
                   SHADER_TYPE_FRAGMENT, fs_data, fs_size));
    mem_free(fs_data);
    CHECK_TRUE(pipeline = shader_pipeline_create(
                   vertex_shader, fragment_shader));

    shader_pipeline_apply(pipeline);
    draw_shader_square(0, 0.667, 1.0, 0.333, 0.6);
    CHECK_SQUARE(0.4, 0.6, 0.2);

    shader_pipeline_destroy(pipeline);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_compile_background)
{
    if (!get_binary_supported()) {
        SKIP("Unable to compile shaders to binary data on this system.");
    }
    if (!shader_background_compilation_supported()) {
        SKIP("Background compilation not supported on this system.");
    }

    CHECK_TRUE(shader_set_attribute(0, "position"));
    CHECK_TRUE(shader_set_attribute(2, "color"));

    void *vs_data, *fs_data;
    int vs_size = 0, fs_size = 0;
    int vs_thread;
    CHECK_TRUE(vs_thread = thread_create(compile_thread, &vs_data));
    CHECK_TRUE(fs_data = shader_compile_to_binary(
                   SHADER_TYPE_FRAGMENT, fs_position_color, -1, &fs_size));
    CHECK_TRUE(fs_size > 0);
    CHECK_TRUE(vs_size = thread_wait(vs_thread));
    CHECK_TRUE(vs_data);
    CHECK_TRUE(vs_size > 0);

    int vertex_shader, fragment_shader, pipeline;
    CHECK_TRUE(vertex_shader = shader_create_from_binary(
                   SHADER_TYPE_VERTEX, vs_data, vs_size));
    mem_free(vs_data);
    CHECK_TRUE(fragment_shader = shader_create_from_binary(
                   SHADER_TYPE_FRAGMENT, fs_data, fs_size));
    mem_free(fs_data);
    CHECK_TRUE(pipeline = shader_pipeline_create(
                   vertex_shader, fragment_shader));

    shader_pipeline_apply(pipeline);
    draw_shader_square(0, 0.667, 1.0, 0.333, 0.6);
    CHECK_SQUARE(0.4, 0.6, 0.2);

    shader_pipeline_destroy(pipeline);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_compile_invalid)
{
    if (!get_binary_supported()) {
        SKIP("Unable to compile shaders to binary data on this system.");
    }

    int vs_size = -123;
    CHECK_FALSE(shader_compile_to_binary(-1, vs_position_color, -1, &vs_size));
    CHECK_FALSE(shader_compile_to_binary(SHADER_TYPE_VERTEX, NULL, -1,
                                         &vs_size));
    CHECK_FALSE(shader_compile_to_binary(SHADER_TYPE_VERTEX, "", -1,
                                         &vs_size));
    CHECK_FALSE(shader_compile_to_binary(SHADER_TYPE_VERTEX, "foo", 0,
                                         &vs_size));
    CHECK_FALSE(shader_compile_to_binary(SHADER_TYPE_VERTEX, vs_position_color,
                                         -1, NULL));
    CHECK_INTEQUAL(vs_size, -123);

    return 1;
}

/*************************************************************************/
/**************** Test routines: Shader pipeline objects *****************/
/*************************************************************************/

TEST(test_pipeline_create_memory_failure)
{
    CHECK_TRUE(shader_set_attribute(0, "position"));
    CHECK_TRUE(shader_set_attribute(2, "color"));

    int vertex_shader, fragment_shader, pipeline;
    CHECK_TRUE(vertex_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_position_color, -1));
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT, fs_position_color, -1));
    CHECK_MEMORY_FAILURES(pipeline = shader_pipeline_create(
                              vertex_shader, fragment_shader));

    shader_pipeline_apply(pipeline);
    draw_shader_square(0, 0.667, 1.0, 0.333, 0.6);
    CHECK_SQUARE(0.4, 0.6, 0.2);

    shader_pipeline_destroy(pipeline);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_pipeline_create_invalid)
{
    CHECK_TRUE(shader_set_attribute(0, "position"));
    CHECK_TRUE(shader_set_attribute(2, "color"));

    int vertex_shader, fragment_shader;
    CHECK_TRUE(vertex_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_position_color, -1));
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT, fs_position_color, -1));

    /* Using wrong shader types should result in failure. */
    CHECK_FALSE(shader_pipeline_create(fragment_shader, fragment_shader));
    CHECK_FALSE(shader_pipeline_create(vertex_shader, vertex_shader));
    CHECK_FALSE(shader_pipeline_create(fragment_shader, vertex_shader));

    CHECK_FALSE(shader_pipeline_create(0, fragment_shader));
    CHECK_FALSE(shader_pipeline_create(vertex_shader, 0));
    CHECK_FALSE(shader_pipeline_create(INVALID_SHADER, fragment_shader));
    CHECK_FALSE(shader_pipeline_create(vertex_shader, INVALID_SHADER));

    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    CHECK_FALSE(shader_pipeline_create(vertex_shader, fragment_shader));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_pipeline_destroy_after_shaders)
{
#ifdef SIL_PLATFORM_IOS
    if (!ios_version_is_at_least("6.0")) {
        SKIP("Test crashes on iOS 5.1.1 due to a bug in OpenGL.");
    }
#endif

    CHECK_TRUE(shader_set_attribute(0, "position"));
    CHECK_TRUE(shader_set_attribute(2, "color"));

    int vertex_shader, fragment_shader, pipeline;
    CHECK_TRUE(vertex_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_position_color, -1));
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT, fs_position_color, -1));
    CHECK_MEMORY_FAILURES(pipeline = shader_pipeline_create(
                              vertex_shader, fragment_shader));

    shader_pipeline_apply(pipeline);
    draw_shader_square(0, 0.667, 1.0, 0.333, 0.6);
    CHECK_SQUARE(0.4, 0.6, 0.2);

    /* It should be safe to destroy the shaders first (turning the pipeline
     * into a valid but useless object). */
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    shader_pipeline_destroy(pipeline);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_pipeline_destroy_invalid)
{
    CHECK_TRUE(shader_set_attribute(0, "position"));
    CHECK_TRUE(shader_set_attribute(2, "color"));

    int vertex_shader, fragment_shader, pipeline;
    CHECK_TRUE(vertex_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_position_color, -1));
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT, fs_position_color, -1));
    CHECK_TRUE(pipeline = shader_pipeline_create(
                   vertex_shader, fragment_shader));
    shader_pipeline_destroy(pipeline);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);

    shader_pipeline_destroy(pipeline);  // Should log an error but not crash.
    shader_pipeline_destroy(INVALID_SHADER);  // Should log an error but not crash.
    shader_pipeline_destroy(0);  // Should not crash (defined as a no-op).
    shader_pipeline_destroy(-1);  // Should log an error but not crash.

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_pipeline_apply_invalid)
{
    CHECK_TRUE(shader_set_attribute(0, "position"));
    CHECK_TRUE(shader_set_attribute(2, "color"));

    int vertex_shader, fragment_shader, pipeline;
    CHECK_TRUE(vertex_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_position_color, -1));
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT, fs_position_color, -1));
    CHECK_TRUE(pipeline = shader_pipeline_create(
                   vertex_shader, fragment_shader));

    int vertex_shader_2, fragment_shader_2, pipeline_2;
    CHECK_TRUE(vertex_shader_2 = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_position, -1));
    CHECK_TRUE(fragment_shader_2 = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT, fs_position, -1));
    CHECK_TRUE(pipeline_2 = shader_pipeline_create(
                   vertex_shader_2, fragment_shader_2));
    shader_pipeline_destroy(pipeline_2);
    shader_destroy(vertex_shader_2);
    shader_destroy(fragment_shader_2);

    shader_pipeline_apply(pipeline);

    /* We have no way of verifying the result of applying invalid shader
     * pipelines, since the resulting state is that drawing behaviors have
     * undefined behavior.  Just check that these don't crash. */
    shader_pipeline_apply(0);
    shader_pipeline_apply(pipeline_2);
    shader_pipeline_apply(INVALID_SHADER);

    shader_pipeline_apply(pipeline);
    draw_shader_square(0, 0.667, 1.0, 0.333, 0.6);
    CHECK_SQUARE(0.4, 0.6, 0.2);

    shader_pipeline_destroy(pipeline);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    return 1;
}

/*************************************************************************/
/******************* Test routines: Vertex attributes ********************/
/*************************************************************************/

TEST(test_set_attribute_rebind_name_to_different_index)
{
    CHECK_TRUE(shader_set_attribute(0, "position"));
    CHECK_FALSE(shader_set_attribute(1, "position")); // Name is already bound.
    CHECK_TRUE(shader_set_attribute(2, "color"));

    int vertex_shader, fragment_shader, pipeline;
    CHECK_TRUE(vertex_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_position_color, -1));
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT, fs_position_color, -1));
    CHECK_TRUE(pipeline = shader_pipeline_create(
                   vertex_shader, fragment_shader));

    shader_pipeline_apply(pipeline);
    draw_shader_square(0, 0.667, 1.0, 0.333, 0.6);
    CHECK_SQUARE(0.4, 0.6, 0.2);

    shader_pipeline_destroy(pipeline);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_attribute_rebind_name_to_same_index)
{
    CHECK_TRUE(shader_set_attribute(0, "position"));
    CHECK_TRUE(shader_set_attribute(0, "position"));
    CHECK_TRUE(shader_set_attribute(2, "color"));

    int vertex_shader, fragment_shader, pipeline;
    CHECK_TRUE(vertex_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_position_color, -1));
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT, fs_position_color, -1));
    CHECK_TRUE(pipeline = shader_pipeline_create(
                   vertex_shader, fragment_shader));

    shader_pipeline_apply(pipeline);
    draw_shader_square(0, 0.667, 1.0, 0.333, 0.6);
    CHECK_SQUARE(0.4, 0.6, 0.2);

    shader_pipeline_destroy(pipeline);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_attribute_rebind_index_with_different_name)
{
    CHECK_TRUE(shader_set_attribute(0, "color"));
    CHECK_TRUE(shader_set_attribute(0, "position"));
    CHECK_TRUE(shader_set_attribute(2, "color"));  // color is not bound here.

    int vertex_shader, fragment_shader, pipeline;
    CHECK_TRUE(vertex_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_position_color, -1));
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT, fs_position_color, -1));
    CHECK_TRUE(pipeline = shader_pipeline_create(
                   vertex_shader, fragment_shader));

    shader_pipeline_apply(pipeline);
    draw_shader_square(0, 0.667, 1.0, 0.333, 0.6);
    CHECK_SQUARE(0.4, 0.6, 0.2);

    shader_pipeline_destroy(pipeline);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_attribute_unbind)
{
    CHECK_TRUE(shader_set_attribute(0, "position"));
    CHECK_TRUE(shader_set_attribute(1, "color"));
    CHECK_TRUE(shader_set_attribute(1, NULL));
    CHECK_TRUE(shader_set_attribute(2, "color"));

    int vertex_shader, fragment_shader, pipeline;
    CHECK_TRUE(vertex_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_position_color, -1));
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT, fs_position_color, -1));
    CHECK_TRUE(pipeline = shader_pipeline_create(
                   vertex_shader, fragment_shader));

    shader_pipeline_apply(pipeline);
    draw_shader_square(0, 0.667, 1.0, 0.333, 0.6);
    CHECK_SQUARE(0.4, 0.6, 0.2);

    shader_pipeline_destroy(pipeline);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_attribute_memory_failures)
{
    CHECK_MEMORY_FAILURES(shader_set_attribute(0, "position"));
    CHECK_MEMORY_FAILURES(shader_set_attribute(2, "color"));

    int vertex_shader, fragment_shader, pipeline;
    CHECK_TRUE(vertex_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_position_color, -1));
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT, fs_position_color, -1));
    CHECK_TRUE(pipeline = shader_pipeline_create(
                   vertex_shader, fragment_shader));

    shader_pipeline_apply(pipeline);
    draw_shader_square(0, 0.667, 1.0, 0.333, 0.6);
    CHECK_SQUARE(0.4, 0.6, 0.2);

    shader_pipeline_destroy(pipeline);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_attribute_invalid)
{
    CHECK_FALSE(shader_set_attribute(-1, NULL));
    CHECK_FALSE(shader_set_attribute(256, NULL));
    CHECK_FALSE(shader_set_attribute(-1, "position"));
    CHECK_FALSE(shader_set_attribute(shader_max_attributes(), "color"));
    /* The above binds failed, so these should succeed. */
    CHECK_TRUE(shader_set_attribute(0, "position"));
    CHECK_TRUE(shader_set_attribute(2, "color"));

    int vertex_shader, fragment_shader, pipeline;
    CHECK_TRUE(vertex_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_position_color, -1));
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT, fs_position_color, -1));
    CHECK_TRUE(pipeline = shader_pipeline_create(
                   vertex_shader, fragment_shader));

    shader_pipeline_apply(pipeline);
    draw_shader_square(0, 0.667, 1.0, 0.333, 0.6);
    CHECK_SQUARE(0.4, 0.6, 0.2);

    shader_pipeline_destroy(pipeline);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_bind_standard_attribute)
{
    CHECK_TRUE(shader_set_attribute(0, "position"));
    CHECK_TRUE(shader_set_attribute(2, "color"));
    shader_bind_standard_attribute(SHADER_ATTRIBUTE_POSITION, 0);

    int vertex_shader, fragment_shader, pipeline;
    CHECK_TRUE(vertex_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_position_color, -1));
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT, fs_position_color, -1));
    CHECK_TRUE(pipeline = shader_pipeline_create(
                   vertex_shader, fragment_shader));

    shader_pipeline_apply(pipeline);
    draw_shader_square(0, 0.667, 1.0, 0.333, 0.6);
    CHECK_SQUARE(0.4, 0.6, 0.2);

    const struct {float x, y, z, r, g, b, a;} vertices[] = {
        {-0.5, -0.5, 0, 0.333, 0.667, 1.0, 0.6},
        {-0.5, +0.5, 0, 0.333, 0.667, 1.0, 0.6},
        {+0.5, +0.5, 0, 0.333, 0.667, 1.0, 0.6},
        {+0.5, -0.5, 0, 0.333, 0.667, 1.0, 0.6},
    };
    static const uint32_t vertex_format[] = {
        GRAPHICS_VERTEX_FORMAT(POSITION_3F, 0),
        GRAPHICS_VERTEX_FORMAT(ATTRIB_4F(2), 3*sizeof(float)),
        0
    };
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_draw_vertices(GRAPHICS_PRIMITIVE_QUADS, vertices, vertex_format,
                           sizeof(*vertices), lenof(vertices));
    CHECK_SQUARE(0.2, 0.4, 0.6);

    shader_pipeline_destroy(pipeline);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_bind_standard_attribute_all)
{
    int texture;
    ASSERT(texture = texture_create_with_data(
               1, 1, "\xFF\xFF\xFF\x99", TEX_FORMAT_RGBA8888, 1, 0, 0));
    texture_apply(0, texture);

    CHECK_TRUE(shader_set_attribute(3, "position"));
    CHECK_TRUE(shader_set_attribute(4, "texcoord"));
    CHECK_TRUE(shader_set_attribute(5, "color"));
    shader_bind_standard_attribute(SHADER_ATTRIBUTE_POSITION, 3);
    shader_bind_standard_attribute(SHADER_ATTRIBUTE_TEXCOORD, 4);
    shader_bind_standard_attribute(SHADER_ATTRIBUTE_COLOR, 5);

    int vertex_shader, fragment_shader, pipeline;
    CHECK_TRUE(vertex_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX,
                   "in highp vec3 position;\n"
                   "in mediump vec2 texcoord;\n"
                   "in lowp vec4 color;\n"
                   "out mediump vec2 texcoord_varying;\n"
                   "out lowp vec4 color_varying;\n"
                   "void main() {\n"
                   "    gl_Position = vec4(position, 1.0);\n"
                   "    texcoord_varying = texcoord;\n"
                   "    color_varying = color;\n"
                   "}\n", -1));
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT,
                   "in mediump vec2 texcoord_varying;\n"
                   "in lowp vec4 color_varying;\n"
                   "uniform lowp sampler2D tex;\n"
                   "void main() {\n"
                   "    color_out = texture2D(tex, texcoord_varying) * color_varying;\n"
                   "}\n", -1));
    CHECK_TRUE(pipeline = shader_pipeline_create(
                   vertex_shader, fragment_shader));
    shader_set_uniform_int(fragment_shader,
                           shader_get_uniform_id(fragment_shader, "tex"), 0);

    const struct {float x, y, z, u, v; uint8_t r, g, b, a;} vertices[] = {
        {-0.5, -0.5, 0, 0, 0, 0x55, 0xAA, 0xFF, 0xFF},
        {-0.5, +0.5, 0, 0, 1, 0x55, 0xAA, 0xFF, 0xFF},
        {+0.5, +0.5, 0, 1, 1, 0x55, 0xAA, 0xFF, 0xFF},
        {+0.5, -0.5, 0, 1, 0, 0x55, 0xAA, 0xFF, 0xFF},
    };
    static const uint32_t vertex_format[] = {
        GRAPHICS_VERTEX_FORMAT(POSITION_3F, 0),
        GRAPHICS_VERTEX_FORMAT(TEXCOORD_2F, 3*sizeof(float)),
        GRAPHICS_VERTEX_FORMAT(COLOR_4NUB, 5*sizeof(float)),
        0
    };
    shader_pipeline_apply(pipeline);
    graphics_draw_vertices(GRAPHICS_PRIMITIVE_QUADS, vertices, vertex_format,
                           sizeof(*vertices), lenof(vertices));
    CHECK_SQUARE(0.2, 0.4, 0.6);

    shader_pipeline_destroy(pipeline);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_bind_standard_attribute_unbind)
{
    CHECK_TRUE(shader_set_attribute(0, "position"));
    CHECK_TRUE(shader_set_attribute(2, "color"));
    shader_bind_standard_attribute(SHADER_ATTRIBUTE_POSITION, 0);
    shader_bind_standard_attribute(SHADER_ATTRIBUTE_POSITION, -1);

    int vertex_shader, fragment_shader, pipeline;
    CHECK_TRUE(vertex_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_position_color, -1));
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT, fs_position_color, -1));
    CHECK_TRUE(pipeline = shader_pipeline_create(
                   vertex_shader, fragment_shader));

    shader_pipeline_apply(pipeline);
    draw_shader_square(0, 0.667, 1.0, 0.333, 0.6);
    CHECK_SQUARE(0.4, 0.6, 0.2);

    const struct {float x, y, z, r, g, b, a;} vertices[] = {
        {-0.5, -0.5, 0, 0.333, 0.667, 1.0, 0.6},
        {-0.5, +0.5, 0, 0.333, 0.667, 1.0, 0.6},
        {+0.5, +0.5, 0, 0.333, 0.667, 1.0, 0.6},
        {+0.5, -0.5, 0, 0.333, 0.667, 1.0, 0.6},
    };
    static const uint32_t vertex_format[] = {
        GRAPHICS_VERTEX_FORMAT(POSITION_3F, 0),
        GRAPHICS_VERTEX_FORMAT(ATTRIB_4F(2), 3*sizeof(float)),
        0
    };
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_draw_vertices(GRAPHICS_PRIMITIVE_QUADS, vertices, vertex_format,
                           sizeof(*vertices), lenof(vertices));
    CHECK_SQUARE(0, 0, 0);

    shader_pipeline_destroy(pipeline);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_bind_standard_attribute_invalid)
{
    CHECK_TRUE(shader_set_attribute(0, "position"));
    CHECK_TRUE(shader_set_attribute(2, "color"));
    shader_bind_standard_attribute(SHADER_ATTRIBUTE_POSITION, 0);
    shader_bind_standard_attribute(-1, 0);
    shader_bind_standard_attribute(SHADER_ATTRIBUTE__NUM, 0);
    shader_bind_standard_attribute(256, 0);

    int vertex_shader, fragment_shader, pipeline;
    CHECK_TRUE(vertex_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_position_color, -1));
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT, fs_position_color, -1));
    CHECK_TRUE(pipeline = shader_pipeline_create(
                   vertex_shader, fragment_shader));

    shader_pipeline_apply(pipeline);
    draw_shader_square(0, 0.667, 1.0, 0.333, 0.6);
    CHECK_SQUARE(0.4, 0.6, 0.2);

    const struct {float x, y, z, r, g, b, a;} vertices[] = {
        {-0.5, -0.5, 0, 0.333, 0.667, 1.0, 0.6},
        {-0.5, +0.5, 0, 0.333, 0.667, 1.0, 0.6},
        {+0.5, +0.5, 0, 0.333, 0.667, 1.0, 0.6},
        {+0.5, -0.5, 0, 0.333, 0.667, 1.0, 0.6},
    };
    static const uint32_t vertex_format[] = {
        GRAPHICS_VERTEX_FORMAT(POSITION_3F, 0),
        GRAPHICS_VERTEX_FORMAT(ATTRIB_4F(2), 3*sizeof(float)),
        0
    };
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_draw_vertices(GRAPHICS_PRIMITIVE_QUADS, vertices, vertex_format,
                           sizeof(*vertices), lenof(vertices));
    CHECK_SQUARE(0.2, 0.4, 0.6);

    shader_pipeline_destroy(pipeline);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_invalid_attributes_in_format)
{
    CHECK_TRUE(shader_set_attribute(0, "position"));
    CHECK_TRUE(shader_set_attribute(2, "color"));
    /* These won't have any effect; we just use them to check that the
     * code doesn't crash on seeing out-of-range values. */
    shader_bind_standard_attribute(SHADER_ATTRIBUTE_POSITION, 3);
    shader_bind_standard_attribute(SHADER_ATTRIBUTE_TEXCOORD,
                                   shader_max_attributes() - 2);
    shader_bind_standard_attribute(SHADER_ATTRIBUTE_COLOR, 255);

    int vertex_shader, fragment_shader, pipeline;
    CHECK_TRUE(vertex_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_position_color, -1));
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT, fs_position_color, -1));
    CHECK_TRUE(pipeline = shader_pipeline_create(
                   vertex_shader, fragment_shader));

    shader_pipeline_apply(pipeline);
    draw_shader_square(0, 0.667, 1.0, 0.333, 0.6);
    CHECK_SQUARE(0.4, 0.6, 0.2);

    const struct {float x, y, z, r, g, b, a;} vertices[] = {
        {-0.5, -0.5, 0, 0.333, 0.667, 1.0, 0.6},
        {-0.5, +0.5, 0, 0.333, 0.667, 1.0, 0.6},
        {+0.5, +0.5, 0, 0.333, 0.667, 1.0, 0.6},
        {+0.5, -0.5, 0, 0.333, 0.667, 1.0, 0.6},
    };
    static const uint32_t vertex_format[] = {
        GRAPHICS_VERTEX_FORMAT(ATTRIB_3F(0), 0),
        GRAPHICS_VERTEX_FORMAT(ATTRIB_4F(2), 3*sizeof(float)),
        /* These should be ignored. */
        GRAPHICS_VERTEX_FORMAT(USER(2), 0),
        GRAPHICS_VERTEX_FORMAT(POSITION_3F, 0),
        GRAPHICS_VERTEX_FORMAT(TEXCOORD_2F, 0),
        GRAPHICS_VERTEX_FORMAT(COLOR_4F, 3*sizeof(float)),
        0
    };
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_draw_vertices(GRAPHICS_PRIMITIVE_QUADS, vertices, vertex_format,
                           sizeof(*vertices), lenof(vertices));
    CHECK_SQUARE(0.2, 0.4, 0.6);

    shader_pipeline_destroy(pipeline);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_shader_attributes_override_standard)
{
    CHECK_TRUE(shader_set_attribute(0, "position"));
    CHECK_TRUE(shader_set_attribute(2, "color"));
    shader_bind_standard_attribute(SHADER_ATTRIBUTE_POSITION, 0);

    int vertex_shader, fragment_shader, pipeline;
    CHECK_TRUE(vertex_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_position_color, -1));
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT, fs_position_color, -1));
    CHECK_TRUE(pipeline = shader_pipeline_create(
                   vertex_shader, fragment_shader));

    shader_pipeline_apply(pipeline);
    draw_shader_square(0, 0.667, 1.0, 0.333, 0.6);
    CHECK_SQUARE(0.4, 0.6, 0.2);

    const struct {float x, y, z, X, Y, Z, r, g, b, a;} vertices[] = {
        {-0.5, -0.5, 0, 0, 0, 0, 0.333, 0.667, 1.0, 0.6},
        {-0.5, +0.5, 0, 0, 1, 0, 0.333, 0.667, 1.0, 0.6},
        {+0.5, +0.5, 0, 1, 1, 0, 0.333, 0.667, 1.0, 0.6},
        {+0.5, -0.5, 0, 1, 0, 0, 0.333, 0.667, 1.0, 0.6},
    };
    static const uint32_t vertex_format[] = {
        GRAPHICS_VERTEX_FORMAT(ATTRIB_3F(0), 0),
        GRAPHICS_VERTEX_FORMAT(ATTRIB_4F(2), 6*sizeof(float)),
        /* This should be ignored. */
        GRAPHICS_VERTEX_FORMAT(POSITION_3F, 3*sizeof(float)),
        0
    };
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_draw_vertices(GRAPHICS_PRIMITIVE_QUADS, vertices, vertex_format,
                           sizeof(*vertices), lenof(vertices));
    CHECK_SQUARE(0.2, 0.4, 0.6);

    shader_pipeline_destroy(pipeline);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_clear_attributes)
{
    CHECK_TRUE(shader_set_attribute(1, "position"));
    CHECK_TRUE(shader_set_attribute(3, "color"));
    shader_bind_standard_attribute(SHADER_ATTRIBUTE_POSITION, 0);
    shader_clear_attributes();
    CHECK_TRUE(shader_set_attribute(0, "position"));
    CHECK_TRUE(shader_set_attribute(2, "color"));

    int vertex_shader, fragment_shader, pipeline;
    CHECK_TRUE(vertex_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_position_color, -1));
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT, fs_position_color, -1));
    CHECK_TRUE(pipeline = shader_pipeline_create(
                   vertex_shader, fragment_shader));

    shader_pipeline_apply(pipeline);
    draw_shader_square(0, 0.667, 1.0, 0.333, 0.6);
    CHECK_SQUARE(0.4, 0.6, 0.2);

    const struct {float x, y, z, r, g, b, a;} vertices[] = {
        {-0.5, -0.5, 0, 0.333, 0.667, 1.0, 0.6},
        {-0.5, +0.5, 0, 0.333, 0.667, 1.0, 0.6},
        {+0.5, +0.5, 0, 0.333, 0.667, 1.0, 0.6},
        {+0.5, -0.5, 0, 0.333, 0.667, 1.0, 0.6},
    };
    static const uint32_t vertex_format[] = {
        GRAPHICS_VERTEX_FORMAT(POSITION_3F, 0),
        GRAPHICS_VERTEX_FORMAT(ATTRIB_4F(2), 3*sizeof(float)),
        0
    };
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_draw_vertices(GRAPHICS_PRIMITIVE_QUADS, vertices, vertex_format,
                           sizeof(*vertices), lenof(vertices));
    CHECK_SQUARE(0, 0, 0);

    shader_pipeline_destroy(pipeline);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_clear_attributes_implicit)
{
    CHECK_TRUE(shader_set_attribute(1, "position"));
    CHECK_TRUE(shader_set_attribute(3, "color"));
    shader_bind_standard_attribute(SHADER_ATTRIBUTE_POSITION, 0);
    /* This should implicitly clear all defined attributes. */
    CHECK_TRUE(graphics_use_shader_objects(1));
    CHECK_TRUE(shader_set_attribute(0, "position"));
    CHECK_TRUE(shader_set_attribute(2, "color"));

    int vertex_shader, fragment_shader, pipeline;
    CHECK_TRUE(vertex_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_position_color, -1));
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT, fs_position_color, -1));
    CHECK_TRUE(pipeline = shader_pipeline_create(
                   vertex_shader, fragment_shader));

    shader_pipeline_apply(pipeline);
    draw_shader_square(0, 0.667, 1.0, 0.333, 0.6);
    CHECK_SQUARE(0.4, 0.6, 0.2);

    const struct {float x, y, z, r, g, b, a;} vertices[] = {
        {-0.5, -0.5, 0, 0.333, 0.667, 1.0, 0.6},
        {-0.5, +0.5, 0, 0.333, 0.667, 1.0, 0.6},
        {+0.5, +0.5, 0, 0.333, 0.667, 1.0, 0.6},
        {+0.5, -0.5, 0, 0.333, 0.667, 1.0, 0.6},
    };
    static const uint32_t vertex_format[] = {
        GRAPHICS_VERTEX_FORMAT(POSITION_3F, 0),
        GRAPHICS_VERTEX_FORMAT(ATTRIB_4F(2), 3*sizeof(float)),
        0
    };
    graphics_clear(0, 0, 0, 0, 1, 0);
    graphics_draw_vertices(GRAPHICS_PRIMITIVE_QUADS, vertices, vertex_format,
                           sizeof(*vertices), lenof(vertices));
    CHECK_SQUARE(0, 0, 0);

    shader_pipeline_destroy(pipeline);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    return 1;
}

/*************************************************************************/
/******************** Test routines: Shader uniforms *********************/
/*************************************************************************/

TEST(test_set_uniform_int)
{
    CHECK_TRUE(shader_set_attribute(0, "position"));
    CHECK_TRUE(shader_set_attribute(2, "color"));

    int vertex_shader, fragment_shader, pipeline;
    CHECK_TRUE(vertex_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_position_color, -1));
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT,
                   "in lowp vec4 color_varying;\n"
                   "uniform lowp int test;\n"
                   "void main() {\n"
                   "    color_out = vec4(color_varying.xyz * color_varying.w, 1.0)\n"
                   "              + vec4(float(test)*0.1, float(test)*0.1,\n"
                   "                     float(test)*0.1, 0.0);\n"
                   "}\n", -1));
    CHECK_TRUE(pipeline = shader_pipeline_create(
                   vertex_shader, fragment_shader));

    int uniform_test;
    CHECK_TRUE(uniform_test = shader_get_uniform_id(fragment_shader, "test"));
    shader_set_uniform_int(fragment_shader, uniform_test, 2);

    /* Also check invalid calls. */
    int dummy_shader;
    CHECK_TRUE(dummy_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_position_color, -1));
    shader_destroy(dummy_shader);
    shader_set_uniform_int(0, uniform_test, 1);
    shader_set_uniform_int(dummy_shader, uniform_test, 1);
    shader_set_uniform_int(INVALID_SHADER, uniform_test, 1);
    shader_set_uniform_int(fragment_shader, 0, 1);

    shader_pipeline_apply(pipeline);
    draw_shader_square(0, 0.667, 1.0, 0.333, 0.6);
    CHECK_SQUARE(0.6, 0.8, 0.4);

    shader_pipeline_destroy(pipeline);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_uniform_invalid)
{
    int fragment_shader;
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT,
                   "in lowp vec4 color_varying;\n"
                   "uniform lowp int test;\n"
                   "void main() {\n"
                   "    color_out = vec4(color_varying.xyz * color_varying.w, 1.0)\n"
                   "              + vec4(float(test)*0.1, float(test)*0.1,\n"
                   "                     float(test)*0.1, 0.0);\n"
                   "}\n", -1));

    CHECK_FALSE(shader_get_uniform_id(fragment_shader, NULL));
    CHECK_FALSE(shader_get_uniform_id(fragment_shader, ""));

    shader_destroy(fragment_shader);
    CHECK_FALSE(shader_get_uniform_id(0, "test"));
    CHECK_FALSE(shader_get_uniform_id(fragment_shader, "test"));
    CHECK_FALSE(shader_get_uniform_id(INVALID_SHADER, "test"));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_uniform_float)
{
    CHECK_TRUE(shader_set_attribute(0, "position"));
    CHECK_TRUE(shader_set_attribute(2, "color"));

    int vertex_shader, fragment_shader, pipeline;
    CHECK_TRUE(vertex_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_position_color, -1));
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT,
                   "in lowp vec4 color_varying;\n"
                   "uniform lowp float test;\n"
                   "void main() {\n"
                   "    color_out = vec4(color_varying.xyz * color_varying.w, 1.0)\n"
                   "              + vec4(test, test, test, 0.0);\n"
                   "}\n", -1));
    CHECK_TRUE(pipeline = shader_pipeline_create(
                   vertex_shader, fragment_shader));

    int uniform_test;
    CHECK_TRUE(uniform_test = shader_get_uniform_id(fragment_shader, "test"));
    shader_set_uniform_float(fragment_shader, uniform_test, 0.2);

    /* Also check invalid calls. */
    int dummy_shader;
    CHECK_TRUE(dummy_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_position_color, -1));
    shader_destroy(dummy_shader);
    shader_set_uniform_float(0, uniform_test, 0.1);
    shader_set_uniform_float(dummy_shader, uniform_test, 0.1);
    shader_set_uniform_float(INVALID_SHADER, uniform_test, 0.1);
    shader_set_uniform_float(fragment_shader, 0, 0.1);

    shader_pipeline_apply(pipeline);
    draw_shader_square(0, 0.667, 1.0, 0.333, 0.6);
    CHECK_SQUARE(0.6, 0.8, 0.4);

    shader_pipeline_destroy(pipeline);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_uniform_vec2)
{
    CHECK_TRUE(shader_set_attribute(0, "position"));
    CHECK_TRUE(shader_set_attribute(2, "color"));

    int vertex_shader, fragment_shader, pipeline;
    CHECK_TRUE(vertex_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_position_color, -1));
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT,
                   "in lowp vec4 color_varying;\n"
                   "uniform lowp vec2 test;\n"
                   "void main() {\n"
                   "    color_out = vec4(color_varying.xyz * color_varying.w, 1.0)\n"
                   "              + vec4(test, 0.0, 0.0);\n"
                   "}\n", -1));
    CHECK_TRUE(pipeline = shader_pipeline_create(
                   vertex_shader, fragment_shader));

    int uniform_test;
    CHECK_TRUE(uniform_test = shader_get_uniform_id(fragment_shader, "test"));
    shader_set_uniform_vec2(fragment_shader, uniform_test,
                            &(const Vector2f){0.6, 0.2});

    /* Also check invalid calls. */
    int dummy_shader;
    CHECK_TRUE(dummy_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_position_color, -1));
    shader_destroy(dummy_shader);
    shader_set_uniform_vec2(0, uniform_test, &(const Vector2f){-0.1, -0.2});
    shader_set_uniform_vec2(dummy_shader, uniform_test,
                            &(const Vector2f){-0.1, -0.2});
    shader_set_uniform_vec2(INVALID_SHADER, uniform_test,
                            &(const Vector2f){-0.1, -0.2});
    shader_set_uniform_vec2(fragment_shader, 0, &(const Vector2f){-0.1, -0.2});
    shader_set_uniform_vec2(fragment_shader, uniform_test, NULL);

    shader_pipeline_apply(pipeline);
    draw_shader_square(0, 0.667, 1.0, 0.333, 0.6);
    CHECK_SQUARE(1.0, 0.8, 0.2);

    shader_pipeline_destroy(pipeline);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_uniform_vec3)
{
    CHECK_TRUE(shader_set_attribute(0, "position"));
    CHECK_TRUE(shader_set_attribute(2, "color"));

    int vertex_shader, fragment_shader, pipeline;
    CHECK_TRUE(vertex_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_position_color, -1));
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT,
                   "in lowp vec4 color_varying;\n"
                   "uniform lowp vec3 test;\n"
                   "void main() {\n"
                   "    color_out = vec4(color_varying.xyz * color_varying.w, 1.0)\n"
                   "              + vec4(test, 0.0);\n"
                   "}\n", -1));
    CHECK_TRUE(pipeline = shader_pipeline_create(
                   vertex_shader, fragment_shader));

    int uniform_test;
    CHECK_TRUE(uniform_test = shader_get_uniform_id(fragment_shader, "test"));
    shader_set_uniform_vec3(fragment_shader, uniform_test,
                            &(const Vector3f){0.6, 0.2, 0.4});

    /* Also check invalid calls. */
    int dummy_shader;
    CHECK_TRUE(dummy_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_position_color, -1));
    shader_destroy(dummy_shader);
    shader_set_uniform_vec3(0, uniform_test,
                            &(const Vector3f){-0.1, -0.2, -0.3});
    shader_set_uniform_vec3(dummy_shader, uniform_test,
                            &(const Vector3f){-0.1, -0.2, -0.3});
    shader_set_uniform_vec3(INVALID_SHADER, uniform_test,
                            &(const Vector3f){-0.1, -0.2, -0.3});
    shader_set_uniform_vec3(fragment_shader, 0,
                            &(const Vector3f){-0.1, -0.2, -0.3});
    shader_set_uniform_vec3(fragment_shader, uniform_test, NULL);

    shader_pipeline_apply(pipeline);
    draw_shader_square(0, 0.667, 1.0, 0.333, 0.6);
    CHECK_SQUARE(1.0, 0.8, 0.6);

    shader_pipeline_destroy(pipeline);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_uniform_vec4)
{
    CHECK_TRUE(shader_set_attribute(0, "position"));
    CHECK_TRUE(shader_set_attribute(2, "color"));

    int vertex_shader, fragment_shader, pipeline;
    CHECK_TRUE(vertex_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_position_color, -1));
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT,
                   "in lowp vec4 color_varying;\n"
                   "uniform lowp vec4 test;\n"
                   "void main() {\n"
                   "    color_out = vec4(color_varying.xyz * color_varying.w, 1.0) + test;\n"
                   "}\n", -1));
    CHECK_TRUE(pipeline = shader_pipeline_create(
                   vertex_shader, fragment_shader));

    int uniform_test;
    CHECK_TRUE(uniform_test = shader_get_uniform_id(fragment_shader, "test"));
    shader_set_uniform_vec4(fragment_shader, uniform_test,
                            &(const Vector4f){0.4, -0.2, 0.2, -0.5});

    /* Also check invalid calls. */
    int dummy_shader;
    CHECK_TRUE(dummy_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_position_color, -1));
    shader_destroy(dummy_shader);
    shader_set_uniform_vec4(0, uniform_test,
                            &(const Vector4f){-0.1, -0.2, -0.3, -0.4});
    shader_set_uniform_vec4(dummy_shader, uniform_test,
                            &(const Vector4f){-0.1, -0.2, -0.3, -0.4});
    shader_set_uniform_vec4(INVALID_SHADER, uniform_test,
                            &(const Vector4f){-0.1, -0.2, -0.3, -0.4});
    shader_set_uniform_vec4(fragment_shader, 0,
                            &(const Vector4f){-0.1, -0.2, -0.3, -0.4});
    shader_set_uniform_vec4(fragment_shader, uniform_test, NULL);

    shader_pipeline_apply(pipeline);
    draw_shader_square(0, 0.667, 1.0, 0.333, 0.6);
    CHECK_SQUARE(0.4, 0.2, 0.2);

    shader_pipeline_destroy(pipeline);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_uniform_mat4)
{
    CHECK_TRUE(shader_set_attribute(0, "position"));
    CHECK_TRUE(shader_set_attribute(2, "color"));

    int vertex_shader, fragment_shader, pipeline;
    CHECK_TRUE(vertex_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_position_color, -1));
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT,
                   "in lowp vec4 color_varying;\n"
                   "uniform lowp mat4 test;\n"
                   "uniform lowp int col;\n"
                   "void main() {\n"
                   /* Some OpenGL ES implementations, e.g. Android 4.4 on
                    * the 1st generation Nexus 7, reject non-constant
                    * array/matrix indices, so we need a more kludgey
                    * approach.  (Requiring constant indices doesn't seem
                    * to be permitted by the spec, but since when did
                    * vendors care about specs...) */
                   // color_out = vec4(color_varying.xyz * color_varying.w, 1.0) + test[col];
                   "    lowp vec4 temp;\n"
                   "    if (col == 0) temp = test[0];\n"
                   "    else if (col == 1) temp = test[1];\n"
                   "    else if (col == 2) temp = test[2];\n"
                   "    else if (col == 3) temp = test[3];\n"
                   "    else temp = vec4(0.0, 0.0, 0.0, 0.0);\n"
                   "    color_out = vec4(color_varying.xyz * color_varying.w, 1.0) + temp;\n"
                   "}\n", -1));
    CHECK_TRUE(pipeline = shader_pipeline_create(
                   vertex_shader, fragment_shader));

    int uniform_test, uniform_col;
    CHECK_TRUE(uniform_test = shader_get_uniform_id(fragment_shader, "test"));
    CHECK_TRUE(uniform_col = shader_get_uniform_id(fragment_shader, "col"));
    shader_set_uniform_mat4(fragment_shader, uniform_test,
                            &(const Matrix4f){ 0.4,  0.2,  0.6,  0.0,
                                              -0.2,  0.2, -0.4,  0.4,
                                               0.2,  0.8,  0.4,  0.6,
                                              -0.5,  0.0,  0.5,  1.0});

    /* Also check invalid calls. */
    int dummy_shader;
    CHECK_TRUE(dummy_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_position_color, -1));
    shader_destroy(dummy_shader);
    const Matrix4f dummy_matrix = {1,2,3,4, 5,6,7,8, 9,10,11,12, 13,14,15,16};
    shader_set_uniform_mat4(0, uniform_test, &dummy_matrix);
    shader_set_uniform_mat4(dummy_shader, uniform_test, &dummy_matrix);
    shader_set_uniform_mat4(INVALID_SHADER, uniform_test, &dummy_matrix);
    shader_set_uniform_mat4(fragment_shader, 0, &dummy_matrix);
    shader_set_uniform_mat4(fragment_shader, uniform_test, NULL);

    static const Vector3f expected_colors[4] = {
        {0.4, 0.2, 0.2},
        {0.6, 0.8, 1.0},
        {1.0, 0.2, 0.6},
        {0.4, 1.0, 0.8},
    };
    for (int col = 0; col < 4; col++) {
        shader_set_uniform_int(fragment_shader, uniform_col, col);
        shader_pipeline_apply(pipeline);
        draw_shader_square(0, 0.667, 1.0, 0.333, 0.6);
        CHECK_SQUARE(expected_colors[col].x, expected_colors[col].y,
                     expected_colors[col].z);
    }

    shader_pipeline_destroy(pipeline);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    return 1;
}

/*************************************************************************/
/***************** Test routines: Multiple texture units *****************/
/*************************************************************************/

TEST(test_multiple_texture_units)
{
    shader_set_attribute(0, "position");
    shader_set_attribute(1, "texcoord");

    int vertex_shader, fragment_shader, pipeline;
    static const char vs_source[] =
        "in highp vec4 position;\n"
        "in mediump vec2 texcoord;\n"
        "out mediump vec2 texcoord_varying;\n"
        "void main() {\n"
        "    gl_Position = position;\n"
        "    texcoord_varying = texcoord;\n"
        "}\n";
    static const char fs_source[] =
        "in mediump vec2 texcoord_varying;\n"
        /* Deliberately declare these in reverse order to ensure that the
         * uniforms are mapped correctly. */
        "uniform lowp sampler2D tex1;\n"
        "uniform lowp sampler2D tex0;\n"
        "void main() {\n"
        "    color_out = (texture2D(tex0, texcoord_varying)\n"
        "                 - texture2D(tex1, texcoord_varying));\n"
        "}\n";
    CHECK_TRUE(vertex_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_source, -1));
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT, fs_source, -1));
    CHECK_TRUE(pipeline = shader_pipeline_create(
                   vertex_shader, fragment_shader));

    int uniform_tex0, uniform_tex1;
    CHECK_TRUE(uniform_tex0 = shader_get_uniform_id(fragment_shader, "tex0"));
    CHECK_TRUE(uniform_tex1 = shader_get_uniform_id(fragment_shader, "tex1"));
    shader_set_uniform_int(fragment_shader, uniform_tex0, 0);
    shader_set_uniform_int(fragment_shader, uniform_tex1, 1);

    int texture0;
    ASSERT(texture0 = texture_create_with_data(1, 1, "\xAA\xCC\xFF\xCC",
                                               TEX_FORMAT_RGBA8888, 1, 0, 0));
    texture_apply(0, texture0);
    int texture1;
    ASSERT(texture1 = texture_create_with_data(1, 1, "\x55\x22\x00\x33",
                                               TEX_FORMAT_RGBA8888, 1, 0, 0));
    texture_apply(1, 0);  // Make sure this doesn't crash.
    texture_apply(1, texture1);

    const struct {float x, y, z, u, v;} vertices[] = {
        {-0.5, -0.5, 0, 0, 0},
        {-0.5, +0.5, 0, 0, 1},
        {+0.5, +0.5, 0, 1, 1},
        {+0.5, -0.5, 0, 1, 0},
    };
    static const uint32_t vertex_format[] = {
        GRAPHICS_VERTEX_FORMAT(ATTRIB_3F(0), 0),
        GRAPHICS_VERTEX_FORMAT(ATTRIB_2F(1), 3*sizeof(float)),
        0
    };
    shader_pipeline_apply(pipeline);
    graphics_draw_vertices(GRAPHICS_PRIMITIVE_QUADS, vertices, vertex_format,
                           sizeof(*vertices), lenof(vertices));
    CHECK_SQUARE(0.2, 0.4, 0.6);

    texture_destroy(texture0);
    texture_destroy(texture1);
    shader_pipeline_destroy(pipeline);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    return 1;
}

/*************************************************************************/
/***************** Test routines: Vertex attribute types *****************/
/*************************************************************************/

TEST(test_attribute_types)
{
    static const struct {
        uint8_t type_size;
        uint8_t is_float;
        uint32_t format[5];
        float mult0, mult1;
        Vector3f expected_color;
        union {
            char ptr[1];
            uint8_t i8[4][10];
            uint16_t i16[4][10];
            uint32_t i32[4][10];
            float f32[4][10];
        } vertices;
    } tests[] = {
        /* 0 */
        { 4, 1,
          {GRAPHICS_VERTEX_FORMAT(ATTRIB_1F(0), 0),
           GRAPHICS_VERTEX_FORMAT(ATTRIB_2F(1), 4),
           GRAPHICS_VERTEX_FORMAT(ATTRIB_3F(2), 12),
           GRAPHICS_VERTEX_FORMAT(ATTRIB_4F(3), 24), 0},
          0.5, 0.25, {0.2, 0.4, 0.6},
          {.f32 = { {0, 0,0, 2,2,0, 0.333,0.667,1.0,0.6},
                    {0, 1,0, 2,0,0, 0.333,0.667,1.0,0.6},
                    {1, 1,0, 0,0,0, 0.333,0.667,1.0,0.6},
                    {1, 0,0, 0,2,0, 0.333,0.667,1.0,0.6} } } },
        /* 1 */
        { 1, 1,
          {GRAPHICS_VERTEX_FORMAT(ATTRIB_1NUB(0), 0),
           GRAPHICS_VERTEX_FORMAT(ATTRIB_2NUB(1), 1),
           GRAPHICS_VERTEX_FORMAT(ATTRIB_3NUB(2), 3),
           GRAPHICS_VERTEX_FORMAT(ATTRIB_4NUB(3), 6), 0},
          0.5, 0.5, {0.4, 0.6, 0.2},
          {.i8 = { {  0,   0,0, 255,255,0, 0xAA,0xFF,0x55,0x99},
                   {  0, 255,0, 255,  0,0, 0xAA,0xFF,0x55,0x99},
                   {255, 255,0,   0,  0,0, 0xAA,0xFF,0x55,0x99},
                   {255,   0,0,   0,255,0, 0xAA,0xFF,0x55,0x99} } } },
        /* 2 */
        { 2, 1,
          {GRAPHICS_VERTEX_FORMAT(ATTRIB_1NS(0), 0),
           GRAPHICS_VERTEX_FORMAT(ATTRIB_2NS(1), 2),
           GRAPHICS_VERTEX_FORMAT(ATTRIB_3NS(2), 6),
           GRAPHICS_VERTEX_FORMAT(ATTRIB_4NS(3), 12), 0},
          0.5, 0.5, {0.2, 0.6, 0.4},
          {.i16 = { {    0, -32768,0, 32767,     0,0, 0x2AAA,0x7FFF,0x5555,0x4CCC},
                    {    0,      0,0, 32767,-32768,0, 0x2AAA,0x7FFF,0x5555,0x4CCC},
                    {32767,      0,0,     0,-32768,0, 0x2AAA,0x7FFF,0x5555,0x4CCC},
                    {32767, -32768,0,     0,     0,0, 0x2AAA,0x7FFF,0x5555,0x4CCC} } } },
        /* 3 */
        { 1, 0,
          {GRAPHICS_VERTEX_FORMAT(ATTRIB_1UB(0), 0),
           GRAPHICS_VERTEX_FORMAT(ATTRIB_2UB(1), 1),
           GRAPHICS_VERTEX_FORMAT(ATTRIB_3UB(2), 3),
           GRAPHICS_VERTEX_FORMAT(ATTRIB_4UB(3), 6), 0},
          0.5, 0.25, {0, 1, 0},
          {.i8 = { {0, 126,0, 2,254,0, 0,1,0,1},
                   {0,   1,0, 2,  0,0, 0,1,0,1},
                   {1,   1,0, 0,  0,0, 0,1,0,1},
                   {1, 126,0, 0,254,0, 0,1,0,1} } } },
        /* 4 */
        { 2, 0,
          {GRAPHICS_VERTEX_FORMAT(ATTRIB_1S(0), 0),
           GRAPHICS_VERTEX_FORMAT(ATTRIB_2S(1), 2),
           GRAPHICS_VERTEX_FORMAT(ATTRIB_3S(2), 6),
           GRAPHICS_VERTEX_FORMAT(ATTRIB_4S(3), 12), 0},
          0.5, 0.25, {1, 1, 0},
          {.i16 = { {0, -1,0, 2, 0,0, 1,1,0,1},
                    {0,  0,0, 2,-2,0, 1,1,0,1},
                    {1,  0,0, 0,-2,0, 1,1,0,1},
                    {1, -1,0, 0, 0,0, 1,1,0,1} } } },
        /* 5 */
        { 4, 0,
          {GRAPHICS_VERTEX_FORMAT(ATTRIB_1I(0), 0),
           GRAPHICS_VERTEX_FORMAT(ATTRIB_2I(1), 4),
           GRAPHICS_VERTEX_FORMAT(ATTRIB_3I(2), 12),
           GRAPHICS_VERTEX_FORMAT(ATTRIB_4I(3), 24), 0},
          0.5, 0.25, {0, 1, 1},
          {.i32 = { {0, -1,0, 2, 0,0, 0,1,1,1},
                    {0,  0,0, 2,-2,0, 0,1,1,1},
                    {1,  0,0, 0,-2,0, 0,1,1,1},
                    {1, -1,0, 0, 0,0, 0,1,1,1} } } },
    };


    shader_set_attribute(0, "in0");
    shader_set_attribute(1, "in1");
    shader_set_attribute(2, "in2");
    shader_set_attribute(3, "in3");

    static const char vs_source_float[] =
        "in highp float in0;\n"
        "in highp vec2 in1;\n"
        "in highp vec3 in2;\n"
        "in lowp vec4 in3;\n"
        "out lowp vec4 color_varying;\n"
        "uniform highp float mult0;\n"
        "uniform highp float mult1;\n"
        "void main() {\n"
        "    highp vec3 position = (mult0 * vec3(in0,in1)) - (mult1 * in2);\n"
        "    gl_Position = vec4(position, 1.0);\n"
        "    color_varying = in3;\n"
        "}\n";
    static const char vs_source_int[] =
        "in highp int in0;\n"
        "in highp ivec2 in1;\n"
        "in highp ivec3 in2;\n"
        "in highp ivec4 in3;\n"
        "out lowp vec4 color_varying;\n"
        "uniform highp float mult0;\n"
        "uniform highp float mult1;\n"
        "void main() {\n"
        "    highp vec3 position = (mult0 * vec3(in0,in1)) - (mult1 * vec3(in2));\n"
        "    gl_Position = vec4(position, 1.0);\n"
        "    color_varying = vec4(in3);\n"
        "}\n";

    int vertex_shader_float, fragment_shader, pipeline_float;
    int uniform_mult0_float, uniform_mult1_float;
    CHECK_TRUE(vertex_shader_float = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_source_float, -1));
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT, fs_position_color, -1));
    CHECK_TRUE(pipeline_float = shader_pipeline_create(
                   vertex_shader_float, fragment_shader));
    CHECK_TRUE(uniform_mult0_float = shader_get_uniform_id(
                   vertex_shader_float, "mult0"));
    CHECK_TRUE(uniform_mult1_float = shader_get_uniform_id(
                   vertex_shader_float, "mult1"));

    int vertex_shader_int, pipeline_int;
    int uniform_mult0_int, uniform_mult1_int;
    int has_int_attribs;
#ifdef USES_GL
    has_int_attribs = opengl_has_features(OPENGL_FEATURE_VERTEX_ATTRIB_INT);
#else
    has_int_attribs = 1;
#endif
    if (has_int_attribs) {
        CHECK_TRUE(vertex_shader_int = shader_create_from_source(
                       SHADER_TYPE_VERTEX, vs_source_int, -1));
        CHECK_TRUE(pipeline_int = shader_pipeline_create(
                       vertex_shader_int, fragment_shader));
        CHECK_TRUE(uniform_mult0_int = shader_get_uniform_id(
                       vertex_shader_int, "mult0"));
        CHECK_TRUE(uniform_mult1_int = shader_get_uniform_id(
                       vertex_shader_int, "mult1"));
    } else {
        vertex_shader_int = 0;
        pipeline_int = 0;
        uniform_mult0_int = 0;
        uniform_mult1_int = 0;
    }

    for (int test = 0; test < lenof(tests); test++) {
#ifdef USES_GL
        if (!tests[test].is_float
         && !opengl_has_features(OPENGL_FEATURE_VERTEX_ATTRIB_INT)) {
            CHECK_FALSE(graphics_create_primitive(
                            GRAPHICS_PRIMITIVE_QUADS, tests[test].vertices.ptr,
                            tests[test].format, 10*tests[test].type_size, 4));
            continue;
        }
#endif
        DLOG("Testing index %d", test);
        const int vertex_shader =
            tests[test].is_float ? vertex_shader_float : vertex_shader_int;
        const int pipeline =
            tests[test].is_float ? pipeline_float : pipeline_int;
        const int uniform_mult0 =
            tests[test].is_float ? uniform_mult0_float : uniform_mult0_int;
        const int uniform_mult1 =
            tests[test].is_float ? uniform_mult1_float : uniform_mult1_int;
        shader_set_uniform_float(vertex_shader, uniform_mult0,
                                 tests[test].mult0);
        shader_set_uniform_float(vertex_shader, uniform_mult1,
                                 tests[test].mult1);
        shader_pipeline_apply(pipeline);
        graphics_clear(0, 0, 0, 0, 1, 0);
        graphics_draw_vertices(GRAPHICS_PRIMITIVE_QUADS,
                               tests[test].vertices.ptr, tests[test].format,
                               10*tests[test].type_size, 4);
        CHECK_SQUARE(tests[test].expected_color.x,
                     tests[test].expected_color.y,
                     tests[test].expected_color.z);
    }

    shader_pipeline_destroy(pipeline_float);
    shader_pipeline_destroy(pipeline_int);
    shader_destroy(vertex_shader_float);
    shader_destroy(vertex_shader_int);
    shader_destroy(fragment_shader);
    return 1;
}

/*************************************************************************/
/*************************************************************************/
