/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/opengl/version.c: Tests for OpenGL context version control.
 */

#include "src/base.h"
#undef SIL_OPENGL_NO_SYS_FUNCS  // Avoid type renaming.
#include "src/graphics.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/opengl/dyngl.h"
#include "src/sysdep/opengl/internal.h"
#include "src/sysdep/opengl/opengl.h"
#include "src/test/base.h"
#include "src/test/graphics/internal.h"
#include "src/thread.h"

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

/* glGetString() string ID to override, and value to override to. */
static GLenum glGetString_override_name;
static const char *glGetString_override_value;

/**
 * glGetString_override:  Override function for glGetString().  When name
 * is the value set in glGetString_override_name, this function returns the
 * string set in glGetString_override_value instead of the actual string
 * from the GL itself.
 */
static const GLubyte *glGetString_override(
    GLenum name, TEST_glGetString_type *original_glGetString)
{
    if (name == glGetString_override_name) {
        return (const GLubyte *)glGetString_override_value;
    }
    return (*original_glGetString)(name);
}

/*-----------------------------------------------------------------------*/

/* Current graphics_init() state, to avoid double init. */
static uint8_t graphics_initted;

/**
 * set_display_mode:  Call graphics_init() and graphics_set_display_mode()
 * with appropriate size parameters for the runtime environment.
 *
 * [Parameters]
 *     error_ret: Pointer to variable to receive the error code on failure.
 *         May be NULL if the error code is not needed.
 * [Return value]
 *     True on success, false on error.
 */
static int set_display_mode(GraphicsError *error_ret)
{
    if (!graphics_initted) {
        if (!graphics_init()) {
            *error_ret = GRAPHICS_ERROR_UNKNOWN;
            return 0;
        }
        graphics_initted = 1;
    }

    int width, height;
    if (graphics_has_windowed_mode()) {
        width = TESTW;
        height = TESTH;
    } else {
        width = graphics_device_width();
        height = graphics_device_height();
    }
    return graphics_set_display_mode(width, height, error_ret);
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_opengl_version)

TEST_INIT(init)
{
    CHECK_TRUE(thread_init());
    /* We don't call graphics_init() here because some platforms (notably
     * iOS) perform OpenGL setup in sys_graphics_init(), so we need to
     * configure the test-specific glGetString() override before that call. */
    graphics_initted = 0;
    return 1;
}

TEST_CLEANUP(cleanup)
{
    if (graphics_initted) {
        graphics_cleanup();
        graphics_initted = 0;
    }
    thread_cleanup();
    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_gl_version)
{
    /* Use a version string which is syntactically valid and will pass
     * version checks but doesn't actually exist, so we don't spuriously
     * pass because we happen to be running on the same version of OpenGL
     * as we specify for the test. */
    glGetString_override_name = GL_VERSION;
#ifdef SIL_OPENGL_ES
    glGetString_override_value = "OpenGL ES 2.9";
#else
    glGetString_override_value = "2.9";
#endif
    TEST_dyngl_override_glGetString(glGetString_override);
    const int result = set_display_mode(NULL);
    TEST_dyngl_override_glGetString(NULL);
    CHECK_TRUE(result);
    CHECK_INTEQUAL(opengl_major_version(), 2);
    CHECK_INTEQUAL(opengl_minor_version(), 9);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_gl_version_es_cm)
{
#ifndef SIL_OPENGL_ES
    SKIP("Irrelevant on non-ES OpenGL.");
#endif

    glGetString_override_name = GL_VERSION;
    glGetString_override_value = "OpenGL ES-CM 2.9";
    TEST_dyngl_override_glGetString(glGetString_override);
    const int result = set_display_mode(NULL);
    TEST_dyngl_override_glGetString(NULL);
    CHECK_TRUE(result);
    CHECK_INTEQUAL(opengl_major_version(), 2);
    CHECK_INTEQUAL(opengl_minor_version(), 9);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_gl_version_es_cl)
{
#ifndef SIL_OPENGL_ES
    SKIP("Irrelevant on non-ES OpenGL.");
#endif

    glGetString_override_name = GL_VERSION;
    glGetString_override_value = "OpenGL ES-CL 2.9";
    TEST_dyngl_override_glGetString(glGetString_override);
    const int result = set_display_mode(NULL);
    TEST_dyngl_override_glGetString(NULL);
    CHECK_TRUE(result);
    CHECK_INTEQUAL(opengl_major_version(), 2);
    CHECK_INTEQUAL(opengl_minor_version(), 9);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_old_gl_version)
{
#ifdef SIL_OPENGL_ES
    SKIP("Irrelevant on OpenGL ES.");
#endif

    /* A GL_VERSION less than 2.0 should cause display mode initialization
     * to fail due to the GL version being too old. */
    glGetString_override_name = GL_VERSION;
    glGetString_override_value = "1.5";
    TEST_dyngl_override_glGetString(glGetString_override);
    GraphicsError error;
    const int result = set_display_mode(&error);
    TEST_dyngl_override_glGetString(NULL);
    CHECK_FALSE(result);
    CHECK_INTEQUAL(error, GRAPHICS_ERROR_BACKEND_TOO_OLD);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_missing_gl_version)
{
    /* A missing GL_VERSION string should be interpreted as 1.0 on non-ES,
     * causing display mode initialization to fail.  For GLES, we assume
     * 2.0 because we can't even compile the program under GLES 1.1, so
     * display mode initialization will succeed. */
    glGetString_override_name = GL_VERSION;
    glGetString_override_value = NULL;
    TEST_dyngl_override_glGetString(glGetString_override);
    GraphicsError error;
    const int result = set_display_mode(&error);
    TEST_dyngl_override_glGetString(NULL);
#ifdef SIL_OPENGL_ES
    CHECK_TRUE(result);
    CHECK_INTEQUAL(opengl_major_version(), 2);
    CHECK_INTEQUAL(opengl_minor_version(), 0);
#else
    CHECK_FALSE(result);
    CHECK_INTEQUAL(error, GRAPHICS_ERROR_BACKEND_TOO_OLD);
#endif
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_invalid_gl_version)
{
    glGetString_override_name = GL_VERSION;
#ifdef SIL_OPENGL_ES
    glGetString_override_value = "OpenGL ES x.y";
#else
    glGetString_override_value = "x.y";
#endif
    TEST_dyngl_override_glGetString(glGetString_override);
    const int result = set_display_mode(NULL);
    TEST_dyngl_override_glGetString(NULL);
    CHECK_TRUE(result);
    CHECK_INTEQUAL(opengl_major_version(), 2);
    CHECK_INTEQUAL(opengl_minor_version(), 0);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_glsl_version)
{
    glGetString_override_name = GL_SHADING_LANGUAGE_VERSION;
#ifdef SIL_OPENGL_ES
    glGetString_override_value = "OpenGL ES GLSL ES 1.90";
#else
    glGetString_override_value = "1.90";
#endif
    TEST_dyngl_override_glGetString(glGetString_override);
    const int result = set_display_mode(NULL);
    TEST_dyngl_override_glGetString(NULL);
    CHECK_TRUE(result);
    CHECK_INTEQUAL(opengl_sl_major_version(), 1);
    CHECK_INTEQUAL(opengl_sl_minor_version(), 90);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_missing_glsl_version)
{
    glGetString_override_name = GL_SHADING_LANGUAGE_VERSION;
    glGetString_override_value = NULL;
    TEST_dyngl_override_glGetString(glGetString_override);
    const int result = set_display_mode(NULL);
    TEST_dyngl_override_glGetString(NULL);
    CHECK_TRUE(result);
    CHECK_INTEQUAL(opengl_sl_major_version(), 1);
    CHECK_INTEQUAL(opengl_sl_minor_version(), 0);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_invalid_glsl_version)
{
    glGetString_override_name = GL_SHADING_LANGUAGE_VERSION;
#ifdef SIL_OPENGL_ES
    glGetString_override_value = "OpenGL ES GLSL ES x.y";
#else
    glGetString_override_value = "x.y";
#endif
    TEST_dyngl_override_glGetString(glGetString_override);
    const int result = set_display_mode(NULL);
    TEST_dyngl_override_glGetString(NULL);
    CHECK_TRUE(result);
    CHECK_INTEQUAL(opengl_sl_major_version(), 1);
    CHECK_INTEQUAL(opengl_sl_minor_version(), 0);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_request_invalid_version)
{
    /* Check that setting an absurdly high version requirement causes
     * mode setting to fail. */
    CHECK_TRUE(graphics_init());
    graphics_initted = 1;
    CHECK_TRUE(graphics_set_display_attr("opengl_version", 99, 99));
    CHECK_FALSE(set_display_mode(NULL));

    /* Check that setting back to the default causes mode setting to
     * succeed again. */
    CHECK_TRUE(graphics_set_display_attr("opengl_version", 0, 0));
    CHECK_TRUE(set_display_mode(NULL));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_requested_version_reset_on_init)
{
    CHECK_TRUE(graphics_init());
    graphics_initted = 1;
    CHECK_TRUE(graphics_set_display_attr("opengl_version", 99, 99));
    CHECK_FALSE(set_display_mode(NULL));

    /* Check that reinitializing the graphics subsystem resets the
     * requested OpenGL version.  This will never be an issue in real
     * code, but it ensures that a lack of initialization doesn't break
     * other tests. */
    graphics_cleanup();
    CHECK_TRUE(graphics_init());
    CHECK_TRUE(set_display_mode(NULL));

    return 1;
}

/*************************************************************************/
/*************************************************************************/
