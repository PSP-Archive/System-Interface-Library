/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/opengl/state.c: Tests for OpenGL render state management.
 */

#include "src/base.h"
#undef SIL_OPENGL_NO_SYS_FUNCS  // Avoid type renaming.
#include "src/graphics.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/opengl/internal.h"
#include "src/sysdep/opengl/opengl.h"
#include "src/test/base.h"
#include "src/test/graphics/internal.h"
#include "src/test/sysdep/opengl/internal.h"
#include "src/texture.h"

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

/**
 * generate_vertex_shader_source, generate_fragment_shader_source:  Shader
 * source generator for testing error paths.  The vertex shader defines
 * the "transform" uniform incorrectly, so attempts to set it in
 * opengl_apply_shader() will raise GL errors.
 */
static char *generate_vertex_shader_source(
    UNUSED GraphicsPrimitiveType primitive_type, UNUSED int position_count,
    UNUSED int texcoord_count, UNUSED GraphicsTextureColorType texcolor_type,
    UNUSED int tex_offset, UNUSED int color_count, UNUSED int color_uniform,
    UNUSED int fog, UNUSED int alpha_test,
    UNUSED GraphicsComparisonType alpha_comparison)
{
    static const char vs_source[] =
        "uniform int transform;\n"
        "in highp vec4 position;\n"
        "void main() {\n"
        "    gl_Position = position * float(transform);\n"
        "}\n";

    char *shader;
    ASSERT(shader = mem_strdup(vs_source, 0));
    return shader;
}

static char *generate_fragment_shader_source(
    UNUSED GraphicsPrimitiveType primitive_type, UNUSED int position_count,
    UNUSED int texcoord_count, UNUSED GraphicsTextureColorType texcolor_type,
    UNUSED int tex_offset, UNUSED int color_count, UNUSED int color_uniform,
    UNUSED int fog, UNUSED int alpha_test,
    UNUSED GraphicsComparisonType alpha_comparison)
{
    static const char fs_source[] =
        "void main() {\n"
        "    color_out = vec4(1.0, 1.0, 1.0, 1.0);\n"
        "}\n";

    char *shader;
    ASSERT(shader = mem_strdup(fs_source, 0));
    return shader;
}

/*-----------------------------------------------------------------------*/

/**
 * generate_shader_key:  Shader key generator for testing error paths.
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

static int do_test_opengl_state(void);
int test_opengl_state(void)
{
    return run_tests_in_window(do_test_opengl_state);
}

DEFINE_GENERIC_TEST_RUNNER(do_test_opengl_state)

TEST_INIT(init)
{
    graphics_start_frame();
    graphics_set_viewport(0, 0, 64, 64);
    graphics_clear(0, 0, 0, 0, 1, 0);
    return 1;
}

TEST_CLEANUP(cleanup)
{
    graphics_enable_alpha_test(0);
    graphics_set_alpha_test_comparison(GRAPHICS_COMPARISON_GREATER_EQUAL);
    graphics_set_alpha_test_reference(0);
    graphics_set_blend(GRAPHICS_BLEND_ADD,
                       GRAPHICS_BLEND_SRC_ALPHA, GRAPHICS_BLEND_INV_SRC_ALPHA);
    graphics_set_blend_color(&(Vector4f){0,0,0,0});
    graphics_set_clip_region(0, 0, 0, 0);
    graphics_enable_depth_test(0);
    graphics_set_depth_test_comparison(GRAPHICS_COMPARISON_LESS);
    graphics_enable_depth_write(1);
    graphics_set_depth_range(0, 1);
    graphics_set_face_cull(GRAPHICS_FACE_CULL_NONE);
    graphics_enable_fog(0);
    graphics_set_fog_start(0);
    graphics_set_fog_end(0);
    graphics_set_fog_color(&(Vector4f){1,1,1,1});
    graphics_enable_stencil_test(0);
    graphics_set_stencil_comparison(GRAPHICS_COMPARISON_TRUE, 0, ~0U);
    graphics_set_stencil_operations(GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP,
                                    GRAPHICS_STENCIL_KEEP);
    texture_apply(0, 0);

    graphics_finish_frame();
    graphics_flush_resources();
    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_bind_texture_after_delete)
{
    int texture;
    ASSERT(texture = texture_create_with_data(1, 1, "\x33\x66\x99\xFF",
                                              TEX_FORMAT_RGBA8888, 1, 0, 0));

    struct TexturedVertex {float x,y,z,u,v;};
    static const struct TexturedVertex tex_quad_vertices[] = {
        {-0.5,-0.5,0,0,0}, {-0.5,0.5,0,0,1}, {0.5,0.5,0,1,1}, {0.5,-0.5,0,1,0}
    };
    static const uint32_t textured_vertex_format[] = {
        GRAPHICS_VERTEX_FORMAT(POSITION_3F, offsetof(struct TexturedVertex,x)),
        GRAPHICS_VERTEX_FORMAT(TEXCOORD_2F, offsetof(struct TexturedVertex,u)),
        0
    };

    graphics_set_blend(GRAPHICS_BLEND_ADD,
                       GRAPHICS_BLEND_ONE, GRAPHICS_BLEND_ONE);
    texture_apply(0, texture);
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, tex_quad_vertices, textured_vertex_format,
        sizeof(*tex_quad_vertices), lenof(tex_quad_vertices));
    CHECK_SQUARE(0.2, 0.4, 0.6);

    /* Cancel the OpenGL texture binding, as if the texture had been
     * deleted and recreated with the same ID. */
    opengl_bind_texture(GL_TEXTURE_2D, 0);
    glBindTexture(GL_TEXTURE_2D, 0);  // In case opengl_bind_texture is broken.

    texture_apply(0, texture);
    graphics_draw_vertices(
        GRAPHICS_PRIMITIVE_QUADS, tex_quad_vertices, textured_vertex_format,
        sizeof(*tex_quad_vertices), lenof(tex_quad_vertices));
    CHECK_SQUARE(0.4, 0.8, 1);

    texture_destroy(texture);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_apply_shader_error)
{
    graphics_set_shader_generator(generate_vertex_shader_source,
                                  generate_fragment_shader_source,
                                  generate_shader_key, 1, 0);

    SysPrimitive primitive;
    primitive.type = GL_TRIANGLES;
    primitive.position_size = 3;
    primitive.texcoord_size = 0;
    primitive.color_size = 0;
    CHECK_FALSE(opengl_apply_shader(&primitive));

    graphics_set_shader_generator(NULL, NULL, NULL, 0, 0);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_apply_shader_invalid)
{
    SysPrimitive primitive;
    primitive.type = GL_TRIANGLES;
    primitive.position_size = 3;
    primitive.texcoord_size = 0;
    primitive.color_size = 0;
    CHECK_TRUE(opengl_apply_shader(&primitive));

    primitive.position_size = 5;  // Deliberately invalid.
    CHECK_FALSE(opengl_apply_shader(&primitive));

    return 1;
}

/*************************************************************************/
/*************************************************************************/
