/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/opengl/shader-gen.c: OpenGL-specific shader generator tests.
 */

#include "src/base.h"
#undef SIL_OPENGL_NO_SYS_FUNCS  // Avoid type renaming.
#include "src/graphics.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/opengl/internal.h"
#include "src/sysdep/opengl/opengl.h"
#include "src/sysdep/opengl/shader-table.h"
#include "src/test/base.h"
#include "src/test/graphics/internal.h"
#include "src/test/sysdep/opengl/internal.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Number of calls made to generate_{vertex,fragment}_shader_source(). */
static int vertex_shader_source_calls;
static int fragment_shader_source_calls;

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

/**
 * generate_vertex_shader_source, generate_fragment_shader_source:  Shader
 * source generator for testing error paths.  Behavior is controlled by the
 * fog and alpha_test parameters:
 *
 * fog == 0, alpha_test == 0: Causes a compile error in the vertex shader.
 * fog == 0, alpha_test == 1: Causes a compile error in the fragment shader.
 * fog == 1, alpha_test == 0: Causes a link error.
 * fog == 1, alpha_test == 1, alpha_comparison == 0: Generates empty source
 *     code for the vertex shader.
 * fog == 1, alpha_test == 1, alpha_comparison == 1: Generates empty source
 *     code for the fragment shader.
 *
 * Setting color_uniform nonzero will cause the function to return an error
 * without generating any source.
 */
static char *generate_vertex_shader_source(
    UNUSED GraphicsPrimitiveType primitive_type, UNUSED int position_count,
    UNUSED int texcoord_count, UNUSED GraphicsTextureColorType texcolor_type,
    UNUSED int tex_offset, UNUSED int color_count, int color_uniform,
    int fog, int alpha_test, GraphicsComparisonType alpha_comparison)
{
    vertex_shader_source_calls++;

    char *shader;
    if (color_uniform) {
        shader = NULL;
    } else if (!fog && !alpha_test) {
        static const char bad_vs_source[] =
            "in highp vec4 position;\n"
            "void main() {\n"
            "    gl_Position = pos_ition;\n"
            "}\n";
        shader = mem_strdup(bad_vs_source, 0);
    } else if (fog && alpha_test && !alpha_comparison) {
        shader = mem_strdup("", 0);
    } else {
        static const char vs_source[] =
            "in highp vec4 position;\n"
            "void main() {\n"
            "    gl_Position = position;\n"
            "}\n";
        shader = mem_strdup(vs_source, 0);
    }

    return shader;
}

static char *generate_fragment_shader_source(
    UNUSED GraphicsPrimitiveType primitive_type, UNUSED int position_count,
    UNUSED int texcoord_count, UNUSED GraphicsTextureColorType texcolor_type,
    UNUSED int tex_offset, UNUSED int color_count, int color_uniform,
    int fog, int alpha_test, UNUSED GraphicsComparisonType alpha_comparison)
{
    fragment_shader_source_calls++;

    char *shader;
    if (color_uniform) {
        shader = NULL;
    } else if (!fog && alpha_test) {
        static const char bad_fs_source[] =
            "void main() {\n"
            "    color_out = v_ec4(1.0, 1.0, 1.0, 1.0);\n"
            "}\n";
        shader = mem_strdup(bad_fs_source, 0);
    } else if (fog && !alpha_test) {
        static const char bad_fs_source[] =
            "void ma_in() {\n"
            "    color_out = vec4(1.0, 1.0, 1.0, 1.0);\n"
            "}\n";
        shader = mem_strdup(bad_fs_source, 0);
    } else if (fog && alpha_test && alpha_comparison) {
        shader = mem_strdup("", 0);
    } else {
        static const char fs_source[] =
            "void main() {\n"
            "    color_out = vec4(1.0, 1.0, 1.0, 1.0);\n"
            "}\n";
        shader = mem_strdup(fs_source, 0);
    }

    return shader;
}

/*-----------------------------------------------------------------------*/

/**
 * generate_shader_key:  Shader key generator for testing error paths.
 * Setting color_count > 0 will result in an error; all other parameter
 * combinations are accepted.
 */
static uint32_t generate_shader_key(
    UNUSED GraphicsPrimitiveType primitive_type, UNUSED int position_count,
    UNUSED int texcoord_count, UNUSED GraphicsTextureColorType texcolor_type,
    UNUSED int tex_offset, int color_count, int color_uniform,
    int fog, int alpha_test, GraphicsComparisonType alpha_comparison)
{
    if (color_count > 0) {
        return INVALID_SHADER_KEY;
    } else {
        return ((color_uniform * 2 + fog) * 2 + alpha_test) * 2
            + alpha_comparison;
    }
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

static int do_test_opengl_shader_gen(void);
int test_opengl_shader_gen(void)
{
    return run_tests_in_window(do_test_opengl_shader_gen);
}

DEFINE_GENERIC_TEST_RUNNER(do_test_opengl_shader_gen)

/*-----------------------------------------------------------------------*/

TEST_INIT(init)
{
    graphics_set_shader_generator(NULL, NULL, NULL, 0, 0);
    graphics_start_frame();
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
    graphics_finish_frame();
    opengl_clear_generated_shaders();
    graphics_set_shader_generator(NULL, NULL, NULL, 0, 0);
    CHECK_TRUE(shader_table_init(0, 1));
    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

#undef FAIL_ACTION
#define FAIL_ACTION  failed = 1

TEST(test_shader_key)
{
    static const int texcolors[] = {
        TEXCOLOR_A, TEXCOLOR_RGBA,
    };
    static const GraphicsComparisonType comparisons[] = {
        GRAPHICS_COMPARISON_LESS,
        GRAPHICS_COMPARISON_LESS_EQUAL,
        GRAPHICS_COMPARISON_GREATER_EQUAL,
        GRAPHICS_COMPARISON_GREATER,
    };

    int failed = 0;

    /**** Nested test loops. ****/

    for (int is_point = 0; is_point <= 1; is_point++) {
     for (int position_count = 2; position_count <= 4; position_count++) {
      for (int use_texcoord = 0; use_texcoord <= 1; use_texcoord++) {
       DLOG("%d/%d...",
            ((is_point * 3 + (position_count-2)) * 2 + use_texcoord)
                * (1+lenof(texcolors)) * 2 * 2 * 2 * 2 * (1+lenof(comparisons)),
            (2 * 3 * 2)
                * (1+lenof(texcolors)) * 2 * 2 * 2 * 2 * (1+lenof(comparisons)));
       for (int i_texcolor = 0; i_texcolor < 1+lenof(texcolors); i_texcolor++) {
        for (int use_tex_offset = 0; use_tex_offset <= 1; use_tex_offset++) {
         for (int use_vertex_color = 0; use_vertex_color <= 1; use_vertex_color++) {
          for (int use_fixed_color = 0; use_fixed_color <= 1; use_fixed_color++) {
           for (int use_fog = 0; use_fog <= 1; use_fog++) {
            for (int alpha_test = 0; alpha_test < 1+lenof(comparisons); alpha_test++) {

                /**** Single test implementation. ****/

                /* Create dummy objects with just the information needed by
                 * the the shader routines. */
                SysPrimitive primitive;
                SysTexture texture;
                primitive.type = is_point ? GL_POINTS : GL_TRIANGLES;
                primitive.position_size = position_count;
                primitive.texcoord_size = use_texcoord ? 2 : 0;
                primitive.color_size = use_vertex_color ? 4 : 0;
                if (i_texcolor > 0) {
                    texture.color_type = texcolors[i_texcolor-1];
                }

                /* Check how many shaders are stored in the table. */
                const int shaders_loaded = shader_table_used();

                /* Select the shader (which should create it, regardless
                 * of whether it is successfully generated). */
                const int result = opengl_select_shader(
                    &primitive, i_texcolor > 0 ? &texture : NULL,
                    use_tex_offset, use_fixed_color,
                    use_fog, alpha_test != 0,
                    alpha_test ? comparisons[alpha_test-1] : 0);
                if (!(result > 0)) {
                    FAIL("Did not select a shader for"
                         " (%u,%u,%u,%u,%u,%u,%u,%u,%u)",
                         is_point, position_count, use_texcoord, i_texcolor,
                         use_tex_offset, use_vertex_color, use_fixed_color,
                         use_fog, alpha_test);
                    failed = 1;
                }

                /* Check that a new shader was in fact created. */
                if (shader_table_used() != shaders_loaded+1) {
                    FAIL("New shader was not created for"
                         " (%u,%u,%u,%u,%u,%u,%u,%u,%u)",
                         is_point, position_count, use_texcoord, i_texcolor,
                         use_tex_offset, use_vertex_color, use_fixed_color,
                         use_fog, alpha_test);
                    failed = 1;
                }

            }  // use_alpha_test
           }  // use_fog
          }  // use_fixed_color
         }  // use_vertex_color
        }  // use_tex_offset
       }  // i_texcolor
      }  // use_texcoord
     }  // position_count
    }  // is_point

    return !failed;
}

#undef FAIL_ACTION
#define FAIL_ACTION  return 0

/*-----------------------------------------------------------------------*/

TEST(test_select_shader_memory_failure)
{
    SysPrimitive primitive;
    primitive.type = GL_TRIANGLES;
    primitive.position_size = 3;
    primitive.texcoord_size = 0;
    primitive.color_size = 0;

    CHECK_TRUE(shader_table_init(0, 1));

    TEST_mem_fail_after(0, 1, 0);
    const int result = opengl_select_shader(&primitive, NULL, 0, 0, 0, 0, 0);
    TEST_mem_fail_after(-1, 0, 0);
    CHECK_TRUE(result < 0);
    CHECK_INTEQUAL(shader_table_used(), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_generate_shader_memory_failure)
{
    SysPrimitive primitive;
    primitive.type = GL_TRIANGLES;
    primitive.position_size = 3;
    primitive.texcoord_size = 0;
    primitive.color_size = 0;

    CHECK_MEMORY_FAILURES(
        opengl_select_shader(&primitive, NULL, 0, 0, 0, 0, 0) > 0
        || (shader_table_clear(), 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_select_shader_custom_uniforms_memory_failure)
{
    SysPrimitive primitive;
    primitive.type = GL_TRIANGLES;
    primitive.position_size = 3;
    primitive.texcoord_size = 0;
    primitive.color_size = 0;

    CHECK_TRUE(shader_table_init(0, 1));
    CHECK_TRUE(sys_graphics_add_shader_uniform("foo"));

    int result;
    CHECK_MEMORY_FAILURES(
        (result = opengl_select_shader(&primitive, NULL, 0, 0, 0, 0, 0)) >= 0
        || (shader_table_clear(), 0));
    CHECK_TRUE(result);
    CHECK_INTEQUAL(shader_table_used(), 1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_select_shader_unsupported_params)
{
    SysPrimitive primitive;
    primitive.type = GL_TRIANGLES;
    primitive.position_size = 3;
    primitive.texcoord_size = 0;
    primitive.color_size = 0;

    SysTexture texture;
    texture.color_type = TEXCOLOR_RGBA;

    primitive.position_size = 5;
    CHECK_TRUE(opengl_select_shader(&primitive, NULL, 0, 0, 0, 0, 0) < 0);
    primitive.position_size = 3;

    primitive.texcoord_size = 5;
    CHECK_TRUE(opengl_select_shader(&primitive, &texture, 0, 0, 0, 0, 0) < 0);
    primitive.texcoord_size = 0;

    primitive.color_size = 5;
    CHECK_TRUE(opengl_select_shader(&primitive, NULL, 0, 0, 0, 0, 0) < 0);
    primitive.color_size = 0;

    /* Invalid alpha comparison. */
    CHECK_TRUE(opengl_select_shader(&primitive, NULL, 0, 0, 0, 1, 0) < 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_set_uniform_with_no_shader)
{
    SysPrimitive primitive;
    primitive.type = GL_TRIANGLES;
    primitive.position_size = 5;  // Invalid value.
    primitive.texcoord_size = 0;
    primitive.color_size = 0;

    CHECK_TRUE(opengl_select_shader(&primitive, NULL, 0, 0, 0, 0, 0) < 0);

    /* Just make sure these don't crash. */
    opengl_set_uniform_int(UNIFORM_TEXTURE, 0);
    opengl_set_uniform_float(UNIFORM_ALPHA_REF, 0);
    opengl_set_uniform_vec2(UNIFORM_TEX_OFFSET, &(Vector2f){0,0});
    opengl_set_uniform_vec4(UNIFORM_FIXED_COLOR, &(Vector4f){0,0,0,0});
    opengl_set_uniform_mat4(UNIFORM_TRANSFORM, &mat4_identity);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_shader_table_fixed_size)
{
    /* Cycle through all 6 permutations of the 3 position count values,
     * to ensure that the oldest shader is always evicted first. */
    static const uint8_t position_counts[6][3] = {
        {2,3,4},
        {2,4,3},
        {3,2,4},
        {3,4,2},
        {4,2,3},
        {4,3,2},
    };

    for (int test = 0; test < lenof(position_counts); test++) {
        if (!shader_table_init(3, 0)) {
            FAIL("shader_table_init() failed for test %u", test);
        }
        opengl_shader_table_overflow_count = 0;

        SysPrimitive primitive;
        primitive.type = GL_TRIANGLES;
        primitive.color_size = 0;
        primitive.texcoord_size = 0;

        /* First generate shaders for each position count to fill the table. */
        for (int i = 0; i < 3; i++) {
            primitive.position_size = position_counts[test][i];
            if (opengl_select_shader(&primitive, NULL, 0, 0, 0, 0, 0) <= 0) {
                FAIL("Did not select a new shader for test %u index %d",
                     test, i);
            }
        }
        if (shader_table_used() != 3) {
            FAIL("Did not store 3 shaders for test %u", test);
        }

        /* Generate a fourth shader, and check that the table size remains
         * unchanged and an entry is evicted. */
        primitive.color_size = 4;
        if (opengl_select_shader(&primitive, NULL, 0, 0, 0, 0, 0) <= 0) {
            FAIL("Did not select a new shader for test %u", test);
        }
        if (shader_table_used() != 3) {
            FAIL("Stored shader count increased for test %u", test);
        }
        if (opengl_shader_table_overflow_count != 1) {
            FAIL("Did not record a shader table eviction for test %u", test);
        }

        /* Check that the last two shaders in the test set were not evicted. */
        primitive.color_size = 0;
        for (int i = 1; i < 3; i++) {
            primitive.position_size = position_counts[test][i];
            if (opengl_select_shader(&primitive, NULL, 0, 0, 0, 0, 0) <= 0) {
                FAIL("Did not select a new shader for test %u index %d",
                     test, i);
            }
            if (shader_table_used() != 3) {
                FAIL("Stored shader count increased for test %u index %d",
                     test, i);
            }
            if (opengl_shader_table_overflow_count != 1) {
                FAIL("Test %u index %d was incorrectly evicted", test, i);
            }
        }

        /* Check that the first shader in the test set was evicted. */
        primitive.position_size = position_counts[test][0];
        if (opengl_select_shader(&primitive, NULL, 0, 0, 0, 0, 0) <= 0) {
            FAIL("Did not select a new shader for test %u index 0", test);
        }
        if (shader_table_used() != 3) {
            FAIL("Stored shader count increased for test %u index 0", test);
        }
        if (opengl_shader_table_overflow_count != 2) {
            FAIL("Test %u index 0 should have been evicted but was not", test);
        }

        opengl_clear_generated_shaders();
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_shader_table_resize_memory_failure)
{
    SysPrimitive primitive;
    primitive.type = GL_TRIANGLES;
    primitive.position_size = 3;
    primitive.texcoord_size = 0;
    primitive.color_size = 0;

    CHECK_MEMORY_FAILURES(shader_table_init(1, 1));

    CHECK_TRUE(opengl_select_shader(&primitive, NULL, 0, 0, 0, 0, 0) > 0);
    CHECK_INTEQUAL(shader_table_used(), 1);

    TEST_mem_fail_after(0, 0, 0); // Allow the shader source copies to succeed.
    const int result = opengl_select_shader(&primitive, NULL, 0, 1, 0, 0, 0);
    TEST_mem_fail_after(-1, 0, 0);
    CHECK_TRUE(result > 0);
    CHECK_INTEQUAL(shader_table_used(), 1);  // Expect overwrite.

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_shader_table_clear)
{
    SysPrimitive primitive;
    primitive.type = GL_TRIANGLES;
    primitive.position_size = 3;
    primitive.texcoord_size = 0;
    primitive.color_size = 0;

    CHECK_TRUE(shader_table_init(0, 1));

    CHECK_TRUE(opengl_select_shader(&primitive, NULL, 0, 0, 0, 0, 0) > 0);
    CHECK_INTEQUAL(shader_table_used(), 1);

    CHECK_TRUE(opengl_select_shader(&primitive, NULL, 0, 1, 0, 0, 0) > 0);
    CHECK_INTEQUAL(shader_table_used(), 2);

    CHECK_TRUE(opengl_select_shader(&primitive, NULL, 0, 0, 0, 0, 0) > 0);
    CHECK_INTEQUAL(shader_table_used(), 2);

    opengl_clear_generated_shaders();
    CHECK_INTEQUAL(shader_table_used(), 0);

    CHECK_TRUE(opengl_select_shader(&primitive, NULL, 0, 0, 0, 0, 0) > 0);
    CHECK_INTEQUAL(shader_table_used(), 1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_shader_table_clear_fixed_size)
{
    SysPrimitive primitive;
    primitive.type = GL_TRIANGLES;
    primitive.position_size = 3;
    primitive.texcoord_size = 0;
    primitive.color_size = 0;

    CHECK_TRUE(shader_table_init(5, 0));

    CHECK_TRUE(opengl_select_shader(&primitive, NULL, 0, 0, 0, 0, 0) > 0);
    CHECK_INTEQUAL(shader_table_used(), 1);

    CHECK_TRUE(opengl_select_shader(&primitive, NULL, 0, 1, 0, 0, 0) > 0);
    CHECK_INTEQUAL(shader_table_used(), 2);

    CHECK_TRUE(opengl_select_shader(&primitive, NULL, 0, 0, 0, 0, 0) > 0);
    CHECK_INTEQUAL(shader_table_used(), 2);

    opengl_clear_generated_shaders();
    CHECK_INTEQUAL(shader_table_used(), 0);

    CHECK_TRUE(opengl_select_shader(&primitive, NULL, 0, 0, 0, 0, 0) > 0);
    CHECK_INTEQUAL(shader_table_used(), 1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_shader_custom_source_errors)
{
    graphics_set_shader_generator(generate_vertex_shader_source,
                                  generate_fragment_shader_source,
                                  generate_shader_key, 6, 0);

    SysPrimitive primitive;
    primitive.type = GL_TRIANGLES;
    primitive.position_size = 3;
    primitive.texcoord_size = 0;
    primitive.color_size = 0;

    vertex_shader_source_calls = 0;
    fragment_shader_source_calls = 0;

    /* Key generation fails. */
    primitive.color_size = 4;
    CHECK_TRUE(opengl_select_shader(&primitive, NULL, 0, 0, 1, 1, 0) < 0);
    CHECK_INTEQUAL(shader_table_used(), 0);
    CHECK_INTEQUAL(vertex_shader_source_calls, 0);
    CHECK_INTEQUAL(fragment_shader_source_calls, 0);
    primitive.color_size = 0;

    /* Shader source generation fails. */
    CHECK_TRUE(opengl_select_shader(&primitive, NULL, 0, 1, 0, 0, 0) < 0);
    CHECK_INTEQUAL(shader_table_used(), 1);
    CHECK_INTEQUAL(vertex_shader_source_calls, 1);
    CHECK_INTEQUAL(fragment_shader_source_calls, 0);

    /* Vertex shader compilation fails. */
    CHECK_TRUE(opengl_select_shader(&primitive, NULL, 0, 0, 0, 0, 0) < 0);
    CHECK_INTEQUAL(shader_table_used(), 2);
    CHECK_INTEQUAL(vertex_shader_source_calls, 2);
    CHECK_INTEQUAL(fragment_shader_source_calls, 1);

    /* Fragment shader compilation fails. */
    CHECK_TRUE(opengl_select_shader(&primitive, NULL, 0, 0, 0, 1, 0) < 0);
    CHECK_INTEQUAL(shader_table_used(), 3);
    CHECK_INTEQUAL(vertex_shader_source_calls, 3);
    CHECK_INTEQUAL(fragment_shader_source_calls, 2);

    /* Shader program link fails. */
    CHECK_TRUE(opengl_select_shader(&primitive, NULL, 0, 0, 1, 0, 0) < 0);
    CHECK_INTEQUAL(shader_table_used(), 4);
    CHECK_INTEQUAL(vertex_shader_source_calls, 4);
    CHECK_INTEQUAL(fragment_shader_source_calls, 3);

    /* Empty source for the vertex shader. */
    CHECK_TRUE(opengl_select_shader(&primitive, NULL, 0, 0, 1, 1, 0) < 0);
    CHECK_INTEQUAL(shader_table_used(), 5);
    CHECK_INTEQUAL(vertex_shader_source_calls, 5);
    CHECK_INTEQUAL(fragment_shader_source_calls, 3);

    /* Empty source for the fragment shader. */
    CHECK_TRUE(opengl_select_shader(&primitive, NULL, 0, 0, 1, 1, 1) < 0);
    CHECK_INTEQUAL(shader_table_used(), 6);
    CHECK_INTEQUAL(vertex_shader_source_calls, 6);
    CHECK_INTEQUAL(fragment_shader_source_calls, 4);

    /* Subsequent calls on failed parameter sets should not trigger
     * source generation again. */
    CHECK_TRUE(opengl_select_shader(&primitive, NULL, 0, 1, 0, 0, 0) < 0);
    CHECK_TRUE(opengl_select_shader(&primitive, NULL, 0, 0, 0, 0, 0) < 0);
    CHECK_TRUE(opengl_select_shader(&primitive, NULL, 0, 0, 0, 1, 0) < 0);
    CHECK_TRUE(opengl_select_shader(&primitive, NULL, 0, 0, 1, 0, 0) < 0);
    CHECK_TRUE(opengl_select_shader(&primitive, NULL, 0, 0, 1, 1, 0) < 0);
    CHECK_TRUE(opengl_select_shader(&primitive, NULL, 0, 0, 1, 1, 1) < 0);
    CHECK_INTEQUAL(shader_table_used(), 6);
    CHECK_INTEQUAL(vertex_shader_source_calls, 6);
    CHECK_INTEQUAL(fragment_shader_source_calls, 4);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_shader_custom_source_errors_memory_failure)
{
    graphics_set_shader_generator(generate_vertex_shader_source,
                                  generate_fragment_shader_source,
                                  generate_shader_key, 6, 0);

    SysPrimitive primitive;
    primitive.type = GL_TRIANGLES;
    primitive.position_size = 3;
    primitive.texcoord_size = 0;
    primitive.color_size = 0;

    /* This call will fail whether memory allocation succeeds or not, so
     * instead of CHECK_MEMORY_FAILURES(), just loop a bunch of times and
     * check that we don't crash or leak anywhere. */
    const char *expr_str =
        "opengl_select_shader(&primitive, NULL, 0, 0, 1, 0, 0)";
    for (int i = 0; i < 100; i++) {
        opengl_clear_generated_shaders();
        const int64_t used_before = mem_debug_bytes_allocated();
        TEST_mem_fail_after(i, 1, 0);
        if (opengl_select_shader(&primitive, NULL, 0, 0, 1, 0, 0) >= 0) {
            TEST_mem_fail_after(-1, 0, 0);
            FAIL("%s did not fail as expected", expr_str);
        }
        TEST_mem_fail_after(-1, 0, 0);
        const int64_t used_after = mem_debug_bytes_allocated();
        if (used_after > used_before) {
            testlog_log(
                __FILE__, __LINE__, __FUNCTION__, TESTLOG_FAIL,
                "FAIL: %s leaked memory for iteration %d (%lld bytes)",
                expr_str, i+1, (long long)(used_after - used_before));
            mem_debug_report_allocs();
            DLOG("End of leak report for %s", expr_str);
            FAIL_ACTION;
        }
    }

    return 1;
}

/*************************************************************************/
/*************************************************************************/
