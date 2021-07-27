/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/opengl/shader.c: OpenGL-specific shader object tests.
 */

#include "src/base.h"
#undef SIL_OPENGL_NO_SYS_FUNCS  // Avoid type renaming.
#include "src/graphics.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/semaphore.h"
#include "src/shader.h"
#include "src/sysdep.h"
#include "src/sysdep/opengl/internal.h"
#include "src/sysdep/opengl/opengl.h"
#include "src/test/base.h"
#include "src/test/graphics/internal.h"
#include "src/test/sysdep/opengl/internal.h"
#include "src/texture.h"

#if defined(SIL_PLATFORM_LINUX)
# include "src/sysdep/linux/internal.h"
#elif defined(SIL_PLATFORM_MACOSX)
# include "src/sysdep/macosx/graphics.h"
#elif defined(SIL_PLATFORM_WINDOWS)
# include "src/sysdep/windows/internal.h"
#endif

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

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

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

static int do_test_opengl_shader(void);
int test_opengl_shader(void)
{
    return run_tests_in_window(do_test_opengl_shader);
}

DEFINE_GENERIC_TEST_RUNNER(do_test_opengl_shader)

/*-----------------------------------------------------------------------*/

TEST_INIT(init)
{
    graphics_set_viewport(0, 0, 64, 64);
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    CHECK_TRUE(graphics_use_shader_objects(1));
    shader_enable_get_binary(1);

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
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_create_binary_invalid_format)
{
    if (!opengl_has_features(OPENGL_FEATURE_SEPARATE_SHADERS)) {
        SKIP("System doesn't support separable shaders.");
    }
    if (!opengl_has_features(OPENGL_FEATURE_SHADER_BINARIES)) {
        SKIP("System doesn't support shader binaries.");
    }

    /* Invalid trailer version. */
    CHECK_FALSE(sys_shader_create(SHADER_TYPE_VERTEX, "\1", 1, 1));

    /* Valid trailer version but too small for size field. */
    CHECK_FALSE(sys_shader_create(SHADER_TYPE_VERTEX, "\0", 1, 1));

    /* Trailer larger than data size. */
    CHECK_FALSE(sys_shader_create(SHADER_TYPE_VERTEX, "\3\0", 2, 1));

    /* Invalid GL data format.  We assume GL_INVALID_ENUM (0x0500) is never
     * a valid format. */
    CHECK_FALSE(sys_shader_create(SHADER_TYPE_VERTEX, "\0\5\0\0\0\6\0", 7, 1));

    /* Valid format but missing data. */
    static const char source[] =
        "in highp vec4 position;\n"
        "void main() {\n"
        "    gl_Position = position;\n"
        "}\n";
    uint8_t *data;
    int size;
    CHECK_TRUE(data = sys_shader_compile(
                   SHADER_TYPE_VERTEX, source, strlen(source), &size));
    CHECK_TRUE(size > 6);
    CHECK_INTEQUAL(data[size-2], 6);
    CHECK_FALSE(sys_shader_create(SHADER_TYPE_VERTEX, data+size-6, 6, 1));
    mem_free(data);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_binary_no_separate_shaders)
{
    if (!opengl_has_features(OPENGL_FEATURE_SHADER_BINARIES)) {
        SKIP("System doesn't support shader binaries.");
    }
    if (opengl_has_features(OPENGL_FEATURE_SEPARATE_SHADERS)) {
        graphics_finish_frame();
        graphics_cleanup();
        TEST_opengl_force_feature_mask = OPENGL_FEATURE_SEPARATE_SHADERS;
        TEST_opengl_force_feature_flags = 0;
        ASSERT(graphics_init());
        ASSERT(graphics_set_display_attr("stencil_bits", 8));
        graphics_set_display_attr("vsync", 0);
        ASSERT(open_window(TESTW, TESTH));
        TEST_opengl_force_feature_mask = 0;
        TEST_opengl_force_feature_flags = 0;
        graphics_set_viewport(0, 0, TESTW, TESTH);
        graphics_start_frame();
    }

    SysShader *vertex_shader, *fragment_shader;
    SysShaderPipeline *pipeline;
    static const char vs_source[] =
        "in highp vec3 position;\n"
        "void main() {\n"
        "    gl_Position = vec4(position, 1.0);\n"
        "}\n";
    static const char fs_source[] =
        "void main() {\n"
        "    color_out = vec4(0.333, 0.667, 1.0, 0.6);\n"
        "}\n";
    CHECK_TRUE(vertex_shader = sys_shader_create(
                   SHADER_TYPE_VERTEX, vs_source, strlen(vs_source), 0));
    CHECK_TRUE(fragment_shader = sys_shader_create(
                   SHADER_TYPE_FRAGMENT, fs_source, strlen(fs_source), 0));
    CHECK_TRUE(pipeline = sys_shader_pipeline_create(
                   vertex_shader, fragment_shader));
    ASSERT(!pipeline->is_pipeline);
    GLint link_status = 0;
    glGetProgramiv(pipeline->program, GL_LINK_STATUS, &link_status);
    ASSERT(link_status);

    /* Set up a dummy SysShader which will trick sys_shader_get_binary()
     * into giving us the binary data (assuming the system supports it
     * at all). */
    SysShader binary_shader = {
        .shader = pipeline->program,
        .is_program = 1,
        .pipelines = NULL,
        .num_attributes = 1,
    };
    void *data;
    int size;
    if (!(data = sys_shader_get_binary(&binary_shader, &size))) {
        sys_shader_pipeline_destroy(pipeline);
        sys_shader_destroy(vertex_shader);
        sys_shader_destroy(fragment_shader);
        SKIP("System doesn't support shader binaries for the current"
             " display.");
    }

    CHECK_FALSE(sys_shader_create(SHADER_TYPE_VERTEX, data, size, 1));

    mem_free(data);
    sys_shader_pipeline_destroy(pipeline);
    sys_shader_destroy(vertex_shader);
    sys_shader_destroy(fragment_shader);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_binary_not_supported)
{
    if (!opengl_has_features(OPENGL_FEATURE_SEPARATE_SHADERS)) {
        SKIP("System doesn't support separable shaders.");
    }
    if (!opengl_has_features(OPENGL_FEATURE_SHADER_BINARIES)) {
        SKIP("System doesn't support shader binaries.");
    }

    SysShader *shader;
    static const char vs_source[] =
        "in highp vec3 position;\n"
        "void main() {\n"
        "    gl_Position = vec4(position, 1.0);\n"
        "}\n";
    CHECK_TRUE(shader = sys_shader_create(
                   SHADER_TYPE_VERTEX, vs_source, strlen(vs_source), 0));
    ASSERT(shader->is_program);

    void *data;
    int size;
    if (!(data = sys_shader_get_binary(shader, &size))) {
        sys_shader_destroy(shader);
        SKIP("System doesn't support shader binaries for the current"
             " display.");
    }

    graphics_finish_frame();
    graphics_cleanup();
    TEST_opengl_force_feature_mask = OPENGL_FEATURE_SHADER_BINARIES;
    TEST_opengl_force_feature_flags = 0;
    ASSERT(graphics_init());
    ASSERT(graphics_set_display_attr("stencil_bits", 8));
    graphics_set_display_attr("vsync", 0);
    ASSERT(open_window(TESTW, TESTH));
    TEST_opengl_force_feature_mask = 0;
    TEST_opengl_force_feature_flags = 0;
    graphics_set_viewport(0, 0, TESTW, TESTH);
    graphics_start_frame();

    CHECK_FALSE(sys_shader_create(SHADER_TYPE_VERTEX, data, size, 1));

    mem_free(data);
    sys_shader_destroy(shader);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_binary_no_separate_shaders)
{
    if (!opengl_has_features(OPENGL_FEATURE_SHADER_BINARIES)) {
        SKIP("System doesn't support shader binaries.");
    }
    if (opengl_has_features(OPENGL_FEATURE_SEPARATE_SHADERS)) {
        graphics_finish_frame();
        graphics_cleanup();
        TEST_opengl_force_feature_mask = OPENGL_FEATURE_SEPARATE_SHADERS;
        TEST_opengl_force_feature_flags = 0;
        ASSERT(graphics_init());
        ASSERT(graphics_set_display_attr("stencil_bits", 8));
        graphics_set_display_attr("vsync", 0);
        ASSERT(open_window(TESTW, TESTH));
        TEST_opengl_force_feature_mask = 0;
        TEST_opengl_force_feature_flags = 0;
        graphics_set_viewport(0, 0, TESTW, TESTH);
        graphics_start_frame();
    }

    static const char source[] =
        "in highp vec4 position;\n"
        "void main() {\n"
        "    gl_Position = position;\n"
        "}\n";
    SysShader *shader;
    CHECK_TRUE(shader = sys_shader_create(
                   SHADER_TYPE_VERTEX, source, strlen(source), 0));
    CHECK_FALSE(sys_shader_get_binary(shader, (int[1]){0}));

    sys_shader_destroy(shader);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_binary_not_supported)
{
    if (opengl_has_features(OPENGL_FEATURE_SHADER_BINARIES)) {
        graphics_finish_frame();
        graphics_cleanup();
        TEST_opengl_force_feature_mask = OPENGL_FEATURE_SHADER_BINARIES;
        TEST_opengl_force_feature_flags = 0;
        ASSERT(graphics_init());
        ASSERT(graphics_set_display_attr("stencil_bits", 8));
        graphics_set_display_attr("vsync", 0);
        ASSERT(open_window(TESTW, TESTH));
        TEST_opengl_force_feature_mask = 0;
        TEST_opengl_force_feature_flags = 0;
        graphics_set_viewport(0, 0, TESTW, TESTH);
        graphics_start_frame();
    }

    static const char source[] =
        "in highp vec4 position;\n"
        "void main() {\n"
        "    gl_Position = position;\n"
        "}\n";
    SysShader *shader;
    CHECK_TRUE(shader = sys_shader_create(
                   SHADER_TYPE_VERTEX, source, strlen(source), 0));
    CHECK_FALSE(sys_shader_get_binary(shader, (int[1]){0}));

    sys_shader_destroy(shader);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_compile_no_separate_shaders)
{
    if (!opengl_has_features(OPENGL_FEATURE_SHADER_BINARIES)) {
        SKIP("System doesn't support shader binaries.");
    }
    if (opengl_has_features(OPENGL_FEATURE_SEPARATE_SHADERS)) {
        graphics_finish_frame();
        graphics_cleanup();
        TEST_opengl_force_feature_mask = OPENGL_FEATURE_SEPARATE_SHADERS;
        TEST_opengl_force_feature_flags = 0;
        ASSERT(graphics_init());
        ASSERT(graphics_set_display_attr("stencil_bits", 8));
        graphics_set_display_attr("vsync", 0);
        ASSERT(open_window(TESTW, TESTH));
        TEST_opengl_force_feature_mask = 0;
        TEST_opengl_force_feature_flags = 0;
        graphics_set_viewport(0, 0, TESTW, TESTH);
        graphics_start_frame();
    }

    static const char source[] =
        "in highp vec4 position;\n"
        "void main() {\n"
        "    gl_Position = position;\n"
        "}\n";
    CHECK_FALSE(sys_shader_compile(SHADER_TYPE_VERTEX, source, strlen(source),
                                   (int[1]){0}));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_compile_binary_not_supported)
{
    if (opengl_has_features(OPENGL_FEATURE_SHADER_BINARIES)) {
        graphics_finish_frame();
        graphics_cleanup();
        TEST_opengl_force_feature_mask = OPENGL_FEATURE_SHADER_BINARIES;
        TEST_opengl_force_feature_flags = 0;
        ASSERT(graphics_init());
        ASSERT(graphics_set_display_attr("stencil_bits", 8));
        graphics_set_display_attr("vsync", 0);
        ASSERT(open_window(TESTW, TESTH));
        TEST_opengl_force_feature_mask = 0;
        TEST_opengl_force_feature_flags = 0;
        graphics_set_viewport(0, 0, TESTW, TESTH);
        graphics_start_frame();
    }

    static const char source[] =
        "in highp vec4 position;\n"
        "void main() {\n"
        "    gl_Position = position;\n"
        "}\n";
    CHECK_FALSE(sys_shader_compile(SHADER_TYPE_VERTEX, source, strlen(source),
                                   (int[1]){0}));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_local_uniforms)
{
    /* This test (for uniform lists stored in SysShader objects) is only
     * meaningful if separable shaders are not supported, but we run the
     * test regardless just to verify that it works. */

    shader_set_attribute(0, "position");

    int vertex_shader, fragment_shader, pipeline;
    static const char vs_source[] =
        "in highp vec4 position;\n"
        "void main() {\n"
        "    gl_Position = position;\n"
        "}\n";
    static const char fs_source[] =
        "uniform lowp vec4 test;\n"
        "uniform lowp vec4 long_uniform_name;\n"
        "void main() {\n"
        "    color_out = test + long_uniform_name;\n"
        "}\n";
    CHECK_TRUE(vertex_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_source, -1));
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT, fs_source, -1));
    CHECK_TRUE(pipeline = shader_pipeline_create(
                   vertex_shader, fragment_shader));

    int uniform_test, uniform_long_name, uniform_nonexistent;
    CHECK_TRUE(uniform_test = shader_get_uniform_id(fragment_shader, "test"));
    CHECK_TRUE(uniform_long_name = shader_get_uniform_id(
                   fragment_shader, "long_uniform_name"));
    /* This one doesn't exist, but we should still get an ID for it when
     * not using separable shaders; it should be ignored at draw time. */
    if (opengl_has_features(OPENGL_FEATURE_SEPARATE_SHADERS)) {
        CHECK_FALSE(uniform_nonexistent = shader_get_uniform_id(
                        fragment_shader, "nonexistent"));
    } else {
        CHECK_TRUE(uniform_nonexistent = shader_get_uniform_id(
                       fragment_shader, "nonexistent"));
    }
    shader_set_uniform_vec4(fragment_shader, uniform_test,
                            &(const Vector4f){1.0/3.0, 1.0/3.0, 0.6, 0.4});
    shader_set_uniform_vec4(fragment_shader, uniform_long_name,
                            &(const Vector4f){0, 1.0/3.0, 0.4, 0.2});
    shader_set_uniform_vec4(fragment_shader, uniform_nonexistent,
                            &(const Vector4f){1, 1, 1, 1});

    /* Looking up the uniforms again should not add them to the local array
     * again. */
    TEST_mem_fail_after(0, 1, 0);
    const int uniform_test_2 = shader_get_uniform_id(fragment_shader, "test");
    const int uniform_long_name_2 = shader_get_uniform_id(
        fragment_shader, "long_uniform_name");
    const int uniform_nonexistent_2 = shader_get_uniform_id(
        fragment_shader, "nonexistent");
    TEST_mem_fail_after(-1, 0, 0);
    CHECK_INTEQUAL(uniform_test_2, uniform_test);
    CHECK_INTEQUAL(uniform_long_name_2, uniform_long_name);
    CHECK_INTEQUAL(uniform_nonexistent_2, uniform_nonexistent);

    shader_pipeline_apply(pipeline);
    draw_shader_square(0, 1,1,1,1);
    CHECK_SQUARE(0.2, 0.4, 0.6);

    shader_pipeline_destroy(pipeline);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_local_uniforms_on_current_pipeline)
{
    shader_set_attribute(0, "position");

    int vertex_shader, fragment_shader, pipeline;
    static const char vs_source[] =
        "in highp vec4 position;\n"
        "out lowp vec4 color;\n"
        "uniform lowp vec4 test;\n"
        "void main() {\n"
        "    gl_Position = position;\n"
        "    color = test;\n"
        "}\n";
    static const char fs_source[] =
        "in lowp vec4 color;\n"
        "uniform lowp vec4 long_uniform_name;\n"
        "void main() {\n"
        "    color_out = color + long_uniform_name;\n"
        "}\n";
    CHECK_TRUE(vertex_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_source, -1));
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT, fs_source, -1));
    CHECK_TRUE(pipeline = shader_pipeline_create(
                   vertex_shader, fragment_shader));

    /* An extra pipeline just to ensure that uniform changes not on the
     * current pipeline have no effect. */
    int extra_vs, extra_fs, extra_pipeline;
    CHECK_TRUE(extra_vs = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_source, -1));
    CHECK_TRUE(extra_fs = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT, fs_source, -1));
    CHECK_TRUE(extra_pipeline = shader_pipeline_create(extra_vs, extra_fs));

    int uniform_test, uniform_long_name;
    int uniform_extra_test, uniform_extra_long_name;
    CHECK_TRUE(uniform_test = shader_get_uniform_id(vertex_shader, "test"));
    CHECK_TRUE(uniform_long_name = shader_get_uniform_id(
                   fragment_shader, "long_uniform_name"));
    CHECK_TRUE(uniform_extra_test = shader_get_uniform_id(extra_vs, "test"));
    CHECK_TRUE(uniform_extra_long_name = shader_get_uniform_id(
                   extra_fs, "long_uniform_name"));

    shader_pipeline_apply(pipeline);
    /* Check that uniform updates after the pipeline is current are
     * immediately passed to the GL. */
    shader_set_uniform_vec4(vertex_shader, uniform_test,
                            &(const Vector4f){1.0/3.0, 1.0/3.0, 0.6, 0.4});
    shader_set_uniform_vec4(fragment_shader, uniform_long_name,
                            &(const Vector4f){0, 1.0/3.0, 0.4, 0.2});
    /* These shaders are not on the current pipeline, so their uniforms
     * should not be sent to the GL. */
    shader_set_uniform_vec4(extra_vs, uniform_extra_test,
                            &(const Vector4f){1, 0.8, 1.0/3.0, 1});
    shader_set_uniform_vec4(extra_fs, uniform_extra_long_name,
                            &(const Vector4f){-1.0/3.0, 0.2, 0, -0.4});

    draw_shader_square(0, 1,1,1,1);
    CHECK_SQUARE(0.2, 0.4, 0.6);

    /* Switching to the other pipeline should apply the pending uniform
     * updates for its shaders. */
    shader_pipeline_apply(extra_pipeline);
    graphics_clear(0, 0, 0, 0, 1, 0);
    draw_shader_square(0, 1,1,1,1);
    CHECK_SQUARE(0.4, 0.6, 0.2);

    /* Check that there are no NULL dereferences on a pipeline which has
     * lost its shaders. */
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    shader_pipeline_apply(pipeline);
    shader_set_uniform_vec4(extra_vs, uniform_extra_test,
                            &(const Vector4f){1.0/3.0, 1.0/3.0, 0.6, 0.4});
    shader_set_uniform_vec4(extra_fs, uniform_extra_long_name,
                            &(const Vector4f){0, 1.0/3.0, 0.4, 0.2});
    shader_pipeline_apply(extra_pipeline);
    graphics_clear(0, 0, 0, 0, 1, 0);
    draw_shader_square(0, 1,1,1,1);
    CHECK_SQUARE(0.2, 0.4, 0.6);

    shader_pipeline_destroy(pipeline);
    shader_pipeline_destroy(extra_pipeline);
    shader_destroy(extra_vs);
    shader_destroy(extra_fs);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_local_uniforms_memory_failure)
{
    if (opengl_has_features(OPENGL_FEATURE_SEPARATE_SHADERS)) {
        /* With separable shaders, we never need to allocate memory for
         * uniforms so this test doesn't work at all. */
        SKIP("System supports separable shaders.");
    }

    shader_set_attribute(0, "position");

    int vertex_shader, fragment_shader, pipeline;
    static const char vs_source[] =
        "in highp vec4 position;\n"
        "void main() {\n"
        "    gl_Position = position;\n"
        "}\n";
    static const char fs_source[] =
        "uniform lowp vec4 test;\n"
        "uniform lowp vec4 long_uniform_name;\n"
        "void main() {\n"
        "    color_out = test + long_uniform_name;\n"
        "}\n";
    CHECK_TRUE(vertex_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_source, -1));
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT, fs_source, -1));
    CHECK_TRUE(pipeline = shader_pipeline_create(
                   vertex_shader, fragment_shader));

    int uniform_test, uniform_long_name;
    /* We don't use CHECK_MEMORY_FAILURES here because arrays will get
     * extended and not reverted on allocation failure.  The overall leak
     * check for the test will catch any leaks. */
    TEST_mem_fail_after(0, 1, 0);
    uniform_test = shader_get_uniform_id(fragment_shader, "test");
    uniform_long_name = shader_get_uniform_id(fragment_shader,
                                              "long_uniform_name");
    TEST_mem_fail_after(-1, 0, 0);
    CHECK_FALSE(uniform_test);
    CHECK_FALSE(uniform_long_name);
    for (int i = 0; !(uniform_test && uniform_long_name); i++) {
        if (i >= 100) {
            FAIL("Unable to look up uniforms after 100 tries");
        }
        /* Always allow exactly one allocation, since successful
         * allocations persist. */
        TEST_mem_fail_after(1, 1, 0);
        uniform_test = shader_get_uniform_id(fragment_shader, "test");
        uniform_long_name = shader_get_uniform_id(fragment_shader,
                                                  "long_uniform_name");
        TEST_mem_fail_after(-1, 0, 0);
    }

    TEST_mem_fail_after(0, 1, 0);
    /* These will fail because we can't allocate memory for the values. */
    shader_set_uniform_vec4(fragment_shader, uniform_test,
                            &(const Vector4f){1.0/3.0, 1.0/3.0, 0.6, 0.4});
    shader_set_uniform_vec4(fragment_shader, uniform_long_name,
                            &(const Vector4f){0, 1.0/3.0, 0.4, 0.2});
    TEST_mem_fail_after(-1, 0, 0);
    shader_pipeline_apply(pipeline);
    draw_shader_square(0, 1,1,1,1);
    CHECK_SQUARE(0, 0, 0);

    graphics_clear(0, 0, 0, 0, 1, 0);
    shader_set_uniform_vec4(fragment_shader, uniform_test,
                            &(const Vector4f){1.0/3.0, 1.0/3.0, 0.6, 0.4});
    shader_set_uniform_vec4(fragment_shader, uniform_long_name,
                            &(const Vector4f){0, 1.0/3.0, 0.4, 0.2});
    draw_shader_square(0, 1,1,1,1);
    CHECK_SQUARE(0.2, 0.4, 0.6);

    graphics_clear(0, 0, 0, 0, 1, 0);
    TEST_mem_fail_after(0, 1, 0);
    /* These should now succeed because there's no need to allocate any
     * more memory. */
    shader_set_uniform_vec4(fragment_shader, uniform_test,
                            &(const Vector4f){1, 0.8, 1.0/3.0, 1});
    shader_set_uniform_vec4(fragment_shader, uniform_long_name,
                            &(const Vector4f){-1.0/3.0, 0.2, 0, -0.4});
    TEST_mem_fail_after(-1, 0, 0);
    shader_pipeline_apply(pipeline);
    draw_shader_square(0, 1,1,1,1);
    CHECK_SQUARE(0.4, 0.6, 0.2);

    shader_pipeline_destroy(pipeline);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_local_uniforms_invalid)
{
    if (opengl_has_features(OPENGL_FEATURE_SEPARATE_SHADERS)) {
        SKIP("System supports separable shaders.");
    }

    shader_set_attribute(0, "position");

    int vertex_shader, fragment_shader, pipeline;
    static const char vs_source[] =
        "in highp vec4 position;\n"
        "void main() {\n"
        "    gl_Position = position;\n"
        "}\n";
    static const char fs_source[] =
        "uniform lowp vec4 test;\n"
        "uniform lowp vec4 long_uniform_name;\n"
        "void main() {\n"
        "    color_out = test + long_uniform_name;\n"
        "}\n";
    CHECK_TRUE(vertex_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_source, -1));
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT, fs_source, -1));
    CHECK_TRUE(pipeline = shader_pipeline_create(
                   vertex_shader, fragment_shader));

    int uniform_test, uniform_long_name;
    CHECK_TRUE(uniform_test = shader_get_uniform_id(fragment_shader, "test"));
    CHECK_TRUE(uniform_long_name = shader_get_uniform_id(
                   fragment_shader, "long_uniform_name"));
    shader_set_uniform_vec4(fragment_shader, uniform_test,
                            &(const Vector4f){1.0/3.0, 1.0/3.0, 0.6, 0.4});
    shader_set_uniform_vec4(fragment_shader, uniform_long_name,
                            &(const Vector4f){0, 1.0/3.0, 0.4, 0.2});
    /* These should do nothing. */
    shader_set_uniform_vec4(fragment_shader, -1,
                            &(const Vector4f){0, 0, 0, 0});
    shader_set_uniform_vec4(fragment_shader, uniform_long_name + 1,
                            &(const Vector4f){0, 0, 0, 0});

    shader_pipeline_apply(pipeline);
    draw_shader_square(0, 1,1,1,1);
    CHECK_SQUARE(0.2, 0.4, 0.6);

    shader_pipeline_destroy(pipeline);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_shader_in_multiple_pipelines)
{
    shader_set_attribute(0, "position");

    int vertex_shader, fragment_shader, pipeline1, pipeline2, pipeline3;
    static const char vs_source[] =
        "in highp vec4 position;\n"
        "void main() {\n"
        "    gl_Position = position;\n"
        "}\n";
    static const char fs_source[] =
        "uniform lowp vec4 test;\n"
        "uniform lowp vec4 long_uniform_name;\n"
        "void main() {\n"
        "    color_out = test + long_uniform_name;\n"
        "}\n";
    CHECK_TRUE(vertex_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_source, -1));
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT, fs_source, -1));
    CHECK_TRUE(pipeline1 = shader_pipeline_create(
                   vertex_shader, fragment_shader));
    CHECK_TRUE(pipeline2 = shader_pipeline_create(
                   vertex_shader, fragment_shader));
    CHECK_TRUE(pipeline3 = shader_pipeline_create(
                   vertex_shader, fragment_shader));

    int uniform_test, uniform_long_name;
    CHECK_TRUE(uniform_test = shader_get_uniform_id(fragment_shader, "test"));
    CHECK_TRUE(uniform_long_name = shader_get_uniform_id(
                   fragment_shader, "long_uniform_name"));

    shader_pipeline_apply(pipeline1);
    shader_set_uniform_vec4(fragment_shader, uniform_test,
                            &(const Vector4f){1.0/3.0, 1.0/3.0, 0.6, 0.4});
    shader_set_uniform_vec4(fragment_shader, uniform_long_name,
                            &(const Vector4f){0, 1.0/3.0, 0.4, 0.2});
    draw_shader_square(0, 1,1,1,1);
    CHECK_SQUARE(0.2, 0.4, 0.6);

    shader_pipeline_apply(pipeline2);
    graphics_clear(0, 0, 0, 0, 1, 0);
    draw_shader_square(0, 1,1,1,1);
    CHECK_SQUARE(0.2, 0.4, 0.6);

    shader_pipeline_apply(pipeline3);
    graphics_clear(0, 0, 0, 0, 1, 0);
    draw_shader_square(0, 1,1,1,1);
    CHECK_SQUARE(0.2, 0.4, 0.6);

#ifdef SIL_PLATFORM_IOS
    if (!ios_version_is_at_least("6.0")) {
        shader_pipeline_destroy(pipeline3);
        shader_pipeline_destroy(pipeline2);
        shader_pipeline_destroy(pipeline1);
        shader_destroy(fragment_shader);
        shader_destroy(vertex_shader);
        SKIP("Out-of-order destroy crashes on iOS 5.1.1 due to a bug in"
             " OpenGL.");
    }
#endif

    /* Check that various links are updated correctly if we delete things
     * not in the reverse order of creation. */
    shader_pipeline_destroy(pipeline2);
    shader_pipeline_destroy(pipeline1);
    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    shader_pipeline_destroy(pipeline3);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_num_attributes_no_current_pipeline)
{
    shader_pipeline_apply(0);
    CHECK_INTEQUAL(opengl_shader_num_attributes(), 0);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_standard_attribute_binding_no_current_pipeline)
{
    shader_pipeline_apply(0);
    CHECK_INTEQUAL(opengl_shader_standard_attribute_binding(
                       SHADER_ATTRIBUTE_POSITION), -1);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_standard_attribute_binding_shaderless_pipeline)
{
    int vertex_shader, fragment_shader, pipeline;
    static const char vs_source[] =
        "in highp vec4 position;\n"
        "void main() {\n"
        "    gl_Position = position;\n"
        "}\n";
    static const char fs_source[] =
        "uniform lowp vec4 test;\n"
        "uniform lowp vec4 long_uniform_name;\n"
        "void main() {\n"
        "    color_out = test + long_uniform_name;\n"
        "}\n";
    CHECK_TRUE(vertex_shader = shader_create_from_source(
                   SHADER_TYPE_VERTEX, vs_source, -1));
    CHECK_TRUE(fragment_shader = shader_create_from_source(
                   SHADER_TYPE_FRAGMENT, fs_source, -1));
    CHECK_TRUE(pipeline = shader_pipeline_create(
                   vertex_shader, fragment_shader));

    shader_destroy(vertex_shader);
    shader_destroy(fragment_shader);
    CHECK_INTEQUAL(opengl_shader_standard_attribute_binding(
                       SHADER_ATTRIBUTE_POSITION), -1);

    shader_pipeline_destroy(pipeline);
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
    CHECK_TRUE(shader_set_attribute(0, "position"));
    shader_bind_standard_attribute(SHADER_ATTRIBUTE_POSITION, 0);
    graphics_start_frame();

    SysShader *vs, *fs;
    SysShaderPipeline *pipeline;
    static const char vs_source[] =
        "in highp vec4 position;\n"
        "uniform highp int uni_i;\n"
        "uniform highp float uni_f;\n"
        "uniform highp vec2 uni_v2;\n"
        "uniform highp vec3 uni_v3;\n"
        "uniform highp vec4 uni_v4;\n"
        "uniform highp mat4 uni_m;\n"
        "void main() {\n"
        "    gl_Position = uni_m * (position + vec4(float(uni_i), uni_f, uni_v2.x, uni_v2.y) + vec4(uni_v3, 0) + uni_v4);\n"
        "}\n";
    static const char fs_source[] =
        "void main() {\n"
        "    color_out = vec4(1.0, 0.0, 0.0, 1.0);\n"
        "}\n";
    CHECK_TRUE(vs = sys_shader_create(
                   SHADER_TYPE_VERTEX, vs_source, strlen(vs_source), 0));
    CHECK_TRUE(fs = sys_shader_create(
                   SHADER_TYPE_FRAGMENT, fs_source, strlen(fs_source), 0));
    CHECK_TRUE(pipeline = sys_shader_pipeline_create(vs, fs));
    const int uni_i = sys_shader_get_uniform_id(vs, "uni_i");
    const int uni_f = sys_shader_get_uniform_id(vs, "uni_f");
    const int uni_v2 = sys_shader_get_uniform_id(vs, "uni_v2");
    const int uni_v3 = sys_shader_get_uniform_id(vs, "uni_v3");
    const int uni_v4 = sys_shader_get_uniform_id(vs, "uni_v4");
    const int uni_m = sys_shader_get_uniform_id(vs, "uni_m");
    sys_shader_set_uniform_mat4(vs, uni_m, &mat4_identity);

    graphics_finish_frame();
    force_close_window();
    ASSERT(open_window(TESTW, TESTH));
    CHECK_TRUE(shader_set_attribute(0, "position"));
    shader_bind_standard_attribute(SHADER_ATTRIBUTE_POSITION, 0);
    graphics_start_frame();

    SysShader *vs2, *fs2;
    SysShaderPipeline *pipeline2;
    static const char fs_source_2[] =
        "void main() {\n"
        "    color_out = vec4(0.0, 1.0, 0.0, 1.0);\n"
        "}\n";
    CHECK_TRUE(vs2 = sys_shader_create(
                   SHADER_TYPE_VERTEX, vs_source, strlen(vs_source), 0));
    CHECK_INTEQUAL(vs2->shader, vs->shader);
    CHECK_TRUE(fs2 = sys_shader_create(
                   SHADER_TYPE_FRAGMENT, fs_source_2, strlen(fs_source_2), 0));
    CHECK_INTEQUAL(fs2->shader, fs->shader);
    CHECK_TRUE(pipeline2 = sys_shader_pipeline_create(vs2, fs2));
    CHECK_INTEQUAL(pipeline2->program, pipeline->program);
    sys_shader_set_uniform_mat4(vs2, sys_shader_get_uniform_id(vs2, "uni_m"),
                                &mat4_identity);

    SysShader *vs3, *fs3;
    SysShaderPipeline *pipeline3;
    static const char fs_source_3[] =
        "void main() {\n"
        "    color_out = vec4(0.0, 0.0, 1.0, 1.0);\n"
        "}\n";
    CHECK_TRUE(vs3 = sys_shader_create(
                   SHADER_TYPE_VERTEX, vs_source, strlen(vs_source), 0));
    CHECK_TRUE(vs3->shader != vs2->shader);
    CHECK_TRUE(fs3 = sys_shader_create(
                   SHADER_TYPE_FRAGMENT, fs_source_3, strlen(fs_source_3), 0));
    CHECK_TRUE(fs3->shader != fs2->shader);
    CHECK_TRUE(pipeline3 = sys_shader_pipeline_create(vs3, fs3));
    CHECK_TRUE(pipeline3->program != pipeline2->program);
    sys_shader_set_uniform_mat4(vs3, sys_shader_get_uniform_id(vs3, "uni_m"),
                                &mat4_identity);

    /* Check that applying an invalidated shader pipeline fails. */
    sys_shader_pipeline_apply(pipeline3);
    sys_shader_pipeline_apply(pipeline);  // This call should fail.
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    graphics_add_vertex(&(Vector3f){-1,-1,0}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){-1,+1,0}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){+1,+1,0}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){+1,-1,0}, NULL, NULL);
    graphics_end_and_draw_primitive();
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 0,0,255,255, (i/4) % TESTW, (i/4) / TESTW);
    }
    mem_free(pixels);

    /* Check other calls that should fail. */
    CHECK_FALSE(sys_shader_pipeline_create(vs, fs2));
    CHECK_FALSE(sys_shader_pipeline_create(vs2, fs));
    CHECK_FALSE(sys_shader_get_uniform_id(vs, "uni_m"));
    /* If any of these go through, they will affect the coordinates of the
     * primitive drawn below. */
    sys_shader_set_uniform_int(vs, uni_i, 1);
    sys_shader_set_uniform_float(vs, uni_f, 1);
    sys_shader_set_uniform_vec2(vs, uni_v2, &(Vector2f){2, 2});
    sys_shader_set_uniform_vec3(vs, uni_v3, &(Vector3f){3, 3, 3});
    sys_shader_set_uniform_vec4(vs, uni_v4, &(Vector4f){4, 4, 4, 4});
    sys_shader_set_uniform_mat4(vs, uni_m,
                                &(Matrix4f){1,2,3,4, 5,6,7,8,
                                            9,10,11,12, 13,14,15,16});

    /* Check that destroying invalidated shaders and pipelines does not
     * affect existing (valid) shaders and pipelines. */
    sys_shader_pipeline_destroy(pipeline);
    sys_shader_destroy(vs);
    sys_shader_destroy(fs);
    sys_shader_pipeline_apply(pipeline2);
    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    graphics_add_vertex(&(Vector3f){-1,-1,0}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){-1,+1,0}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){+1,+1,0}, NULL, NULL);
    graphics_add_vertex(&(Vector3f){+1,-1,0}, NULL, NULL);
    graphics_end_and_draw_primitive();
    ASSERT(pixels = grab_display());
    for (int i = 0; i < TESTW*TESTH*4; i += 4) {
        CHECK_PIXEL(&pixels[i], 0,255,0,255, (i/4) % TESTW, (i/4) / TESTW);
    }
    mem_free(pixels);

    sys_shader_pipeline_destroy(pipeline2);
    sys_shader_pipeline_destroy(pipeline3);
    sys_shader_destroy(vs2);
    sys_shader_destroy(fs2);
    sys_shader_destroy(vs3);
    sys_shader_destroy(fs3);
    return 1;
}

/*************************************************************************/
/*************************************************************************/
