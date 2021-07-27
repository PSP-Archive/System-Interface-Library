/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/opengl/features.c: Tests for feature-specific code paths
 * in the OpenGL graphics code.
 */

#include "src/base.h"
#undef SIL_OPENGL_NO_SYS_FUNCS  // Avoid type renaming.
#include "src/graphics.h"
#include "src/sysdep.h"
#include "src/sysdep/opengl/dyngl.h"
#include "src/sysdep/opengl/opengl.h"
#include "src/test/base.h"
#include "src/test/sysdep/opengl/internal.h"
#include "src/thread.h"

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

int test_opengl_features_delayed_delete(void)
{
    if (opengl_has_features_uninitted(OPENGL_FEATURE_DELAYED_DELETE)) {
        SKIP("System uses delayed delete by default.");
    }

    DLOG("=== Testing OpenGL code with delayed-delete enabled ===");
    TEST_opengl_force_feature_mask = OPENGL_FEATURE_DELAYED_DELETE;
    TEST_opengl_force_feature_flags = OPENGL_FEATURE_DELAYED_DELETE;
    const int result = test_graphics_texture()
                    && test_graphics_primitive()
                    && test_graphics_shader_obj()
                    && test_graphics_state()
                    && test_graphics_framebuffer()
                    && test_opengl_graphics(); // Includes delete buffer tests.
    TEST_opengl_force_feature_mask = 0;
    TEST_opengl_force_feature_flags = 0;
    DLOG("=== Finished testing OpenGL code with delayed-delete enabled ===");
    if (!result) {
        FAIL("Preceding failure(s) occurred with delayed-delete enabled");
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

int test_opengl_features_delayed_delete_vao(void)
{
    if (opengl_has_features_uninitted(OPENGL_FEATURE_DELAYED_DELETE)) {
        SKIP("System uses delayed delete by default.");
    }
    if (opengl_has_features_uninitted(OPENGL_FEATURE_MANDATORY_VAO)) {
        SKIP("VAOs are mandatory on this GL.");
    }
#if defined(SIL_OPENGL_ES) && !defined(SIL_PLATFORM_IOS)
    /* OpenGL ES 2 doesn't specify VAOs, but Apple includes them as an
     * extension, so we can run this test on iOS. */
    SKIP("VAOs are unavailable on this GL.");
#endif

    DLOG("=== Testing OpenGL code with delayed-delete and VAOs enabled ===");
    TEST_opengl_force_feature_mask = OPENGL_FEATURE_DELAYED_DELETE
                                   | OPENGL_FEATURE_MANDATORY_VAO;
    TEST_opengl_force_feature_flags = OPENGL_FEATURE_DELAYED_DELETE
                                    | OPENGL_FEATURE_MANDATORY_VAO;
    const int result = test_graphics_primitive();
    TEST_opengl_force_feature_mask = 0;
    TEST_opengl_force_feature_flags = 0;
    DLOG("=== Finished testing OpenGL code with delayed-delete and VAOs enabled ===");
    if (!result) {
        FAIL("Preceding failure(s) occurred with delayed-delete and VAOs enabled");
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

int test_opengl_features_no_genmipmap(void)
{
    if (!opengl_has_features_uninitted(OPENGL_FEATURE_GENERATEMIPMAP)) {
        SKIP("System doesn't support server-side mipmap generation.");
    }

    DLOG("=== Testing OpenGL code with server mipmap generation disabled ===");
    TEST_opengl_force_feature_mask = OPENGL_FEATURE_GENERATEMIPMAP;
    TEST_opengl_force_feature_flags = 0;
    const int result = test_graphics_texture()
                    && test_graphics_primitive()
                    && test_opengl_texture();
    TEST_opengl_force_feature_mask = 0;
    TEST_opengl_force_feature_flags = 0;
    DLOG("=== Finished testing OpenGL code with server mipmap generation"
         " disabled ===");
    if (!result) {
        FAIL("Preceding failure(s) occurred with server mipmap generation"
             " disabled");
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

int test_opengl_features_no_getteximage(void)
{
    if (!opengl_has_features_uninitted(OPENGL_FEATURE_GETTEXIMAGE)) {
        SKIP("System doesn't support glGetTexImage().");
    }

    DLOG("=== Testing OpenGL code with glGetTexImage() disabled ===");
    TEST_opengl_force_feature_mask = OPENGL_FEATURE_GETTEXIMAGE;
    TEST_opengl_force_feature_flags = 0;
    const int result = test_graphics_texture()
                    && test_opengl_texture();
    TEST_opengl_force_feature_mask = 0;
    TEST_opengl_force_feature_flags = 0;
    DLOG("=== Finished testing OpenGL code with glGetTexImage() disabled ===");
    if (!result) {
        FAIL("Preceding failure(s) occurred with glGetTexImage() disabled");
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

int test_opengl_features_no_int_attrib(void)
{
    if (!opengl_has_features_uninitted(OPENGL_FEATURE_VERTEX_ATTRIB_INT)) {
        SKIP("System doesn't support integer vertex attributes.");
    }

    DLOG("=== Testing OpenGL code with integer vertex attributes disabled ===");
    TEST_opengl_force_feature_mask = OPENGL_FEATURE_VERTEX_ATTRIB_INT;
    TEST_opengl_force_feature_flags = 0;
    const int result = test_graphics_shader_obj();
    TEST_opengl_force_feature_mask = 0;
    TEST_opengl_force_feature_flags = 0;
    DLOG("=== Finished testing OpenGL code with quads disabled ===");
    if (!result) {
        FAIL("Preceding failure(s) occurred with integer vertex attributes"
             " disabled");
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

int test_opengl_features_no_quads(void)
{
    if (!opengl_has_features_uninitted(OPENGL_FEATURE_NATIVE_QUADS)) {
        SKIP("System doesn't support native quads.");
    }

    DLOG("=== Testing OpenGL code with quads disabled ===");
    TEST_opengl_force_feature_mask = OPENGL_FEATURE_NATIVE_QUADS;
    TEST_opengl_force_feature_flags = 0;
    const int result = test_graphics_primitive()
                    && test_graphics_state()
                    && test_opengl_primitive();
    TEST_opengl_force_feature_mask = 0;
    TEST_opengl_force_feature_flags = 0;
    DLOG("=== Finished testing OpenGL code with quads disabled ===");
    if (!result) {
        FAIL("Preceding failure(s) occurred with quads disabled");
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

int test_opengl_features_no_rg(void)
{
    int width = 64, height = 64;
    ASSERT(thread_init());
    ASSERT(graphics_init());
    if (!graphics_set_display_attr("window", 1)) {
        const GraphicsDisplayModeList *mode_list;
        ASSERT(mode_list = graphics_list_display_modes(0));
        ASSERT(mode_list->num_modes > 0);
        width = mode_list->modes[0].width;
        height = mode_list->modes[0].height;
    }
    ASSERT(graphics_set_display_mode(width, height, NULL));
    if (!opengl_has_formats(OPENGL_FORMAT_RG)) {
        graphics_cleanup();
        thread_cleanup();
        SKIP("System doesn't support the RG texture format.");
    }
#ifdef SIL_OPENGL_ES
    if (opengl_version_is_at_least(3,0)) {
        graphics_cleanup();
        thread_cleanup();
        SKIP("Can't use RG format fallback (LUMINANCE) in OpenGL 3.0+.");
    }
#endif
    graphics_cleanup();
    thread_cleanup();

    DLOG("=== Testing OpenGL code with no RG texture format ===");
    TEST_opengl_force_format_mask = OPENGL_FORMAT_RG;
    TEST_opengl_force_format_flags = 0;
    const int result = test_graphics_primitive()
                    && test_graphics_texture()
                    && test_graphics_texture_formats()
                    && test_opengl_texture();
    TEST_opengl_force_format_mask = 0;
    TEST_opengl_force_format_flags = 0;
    DLOG("=== Finished testing OpenGL code with no RG texture format ===");
    if (!result) {
        FAIL("Preceding failure(s) occurred with RG texture format disabled");
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

int test_opengl_features_no_separate_shaders(void)
{
    if (!opengl_has_features_uninitted(OPENGL_FEATURE_SEPARATE_SHADERS)) {
        SKIP("System doesn't support separable shaders.");
    }

    DLOG("=== Testing OpenGL code with separable shaders disabled ===");
    TEST_opengl_force_feature_mask = OPENGL_FEATURE_SEPARATE_SHADERS;
    TEST_opengl_force_feature_flags = 0;
    const int result = test_graphics_shader_obj()
                    && test_opengl_shader();
    TEST_opengl_force_feature_mask = 0;
    TEST_opengl_force_feature_flags = 0;
    DLOG("=== Finished testing OpenGL code with separable shaders disabled ===");
    if (!result) {
        FAIL("Preceding failure(s) occurred with separable shaders disabled");
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

int test_opengl_features_vao_mandatory(void)
{
    if (opengl_has_features_uninitted(OPENGL_FEATURE_MANDATORY_VAO)) {
        SKIP("VAOs are mandatory on this GL.");
    }
#if defined(SIL_OPENGL_ES) && !defined(SIL_PLATFORM_IOS)
    /* OpenGL ES 2 doesn't specify VAOs, but Apple includes them as an
     * extension, so we can run this test on iOS. */
    SKIP("VAOs are unavailable on this GL.");
#endif

    DLOG("=== Testing OpenGL code with mandatory VAOs enabled ===");
    TEST_opengl_force_feature_mask = OPENGL_FEATURE_MANDATORY_VAO;
    TEST_opengl_force_feature_flags = OPENGL_FEATURE_MANDATORY_VAO;
    const int result = test_graphics_primitive()
                    && test_graphics_shader_gen()
                    && test_graphics_shader_obj()
                    && test_opengl_primitive();
    TEST_opengl_force_feature_mask = 0;
    TEST_opengl_force_feature_flags = 0;
    DLOG("=== Finished testing OpenGL code with mandatory VAOs enabled ===");
    if (!result) {
        FAIL("Preceding failure(s) occurred with mandatory VAOs enabled");
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

int test_opengl_features_vao_static(void)
{
    if (opengl_has_features_uninitted(OPENGL_FEATURE_MANDATORY_VAO)) {
        SKIP("VAOs are mandatory on this GL.");
    }
#if defined(SIL_OPENGL_ES) && !defined(SIL_PLATFORM_IOS)
    SKIP("VAOs are unavailable on this GL.");
#endif

    const int default_static_vao =
        opengl_has_features_uninitted(OPENGL_FEATURE_USE_STATIC_VAO);
    DLOG("=== Testing OpenGL code with static VAOs %sabled ===",
         default_static_vao ? "dis" : "en");
    TEST_opengl_force_feature_mask = OPENGL_FEATURE_USE_STATIC_VAO;
    TEST_opengl_force_feature_flags =
        default_static_vao ? 0 : OPENGL_FEATURE_USE_STATIC_VAO;
    const int result = test_graphics_primitive()
                    && test_graphics_shader_gen()
                    && test_graphics_shader_obj()
                    && test_opengl_primitive();
    TEST_opengl_force_feature_mask = 0;
    TEST_opengl_force_feature_flags = 0;
    DLOG("=== Finished testing OpenGL code with static VAOs %sabled ===",
         default_static_vao ? "dis" : "en");
    if (!result) {
        FAIL("Preceding failure(s) occurred with static VAOs %sabled ===",
             default_static_vao ? "dis" : "en");
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

int test_opengl_features_wrap_dsa(void)
{
    int width = 64, height = 64;
    ASSERT(thread_init());
    ASSERT(graphics_init());
    if (!graphics_set_display_attr("window", 1)) {
        const GraphicsDisplayModeList *mode_list;
        ASSERT(mode_list = graphics_list_display_modes(0));
        ASSERT(mode_list->num_modes > 0);
        int i;
        for (i = 0; i < mode_list->num_modes; i++) {
            if (mode_list->modes[i].width >= width
             && mode_list->modes[i].height >= height) {
                break;
            }
        }
        if (i >= mode_list->num_modes) {
            graphics_cleanup();
            thread_cleanup();
            FAIL("No available display mode of size at least %dx%d",
                 width, height);
        }
        width = mode_list->modes[i].width;
        height = mode_list->modes[i].height;
    }
    ASSERT(graphics_set_display_mode(width, height, NULL));
    const int has_dsa = dyngl_has_dsa();
    graphics_cleanup();
    thread_cleanup();

    if (!has_dsa) {
        SKIP("This GL does not support direct state access.");
    }

    DLOG("=== Testing OpenGL code with DSA function wrappers ===");
    TEST_opengl_always_wrap_dsa = 1;
    const int result = test_graphics_texture()
                    && test_graphics_primitive()
                    && test_graphics_state()
                    && test_graphics_framebuffer()
                    && test_graphics_shader_gen()
                    && test_graphics_shader_obj()
                    && test_opengl_framebuffer()
                    && test_opengl_primitive()
                    && test_opengl_shader()
                    && test_opengl_shader_gen()
                    && test_opengl_state()
                    && test_opengl_texture()
                    && test_graphics_texture_formats();
    TEST_opengl_always_wrap_dsa = 0;
    DLOG("=== Finished testing OpenGL code with DSA function wrappers ===");
    if (!result) {
        FAIL("Preceding failure(s) occurred with DSA function wrappers"
             " installed");
    }

    return 1;
}

/*************************************************************************/
/*************************************************************************/
