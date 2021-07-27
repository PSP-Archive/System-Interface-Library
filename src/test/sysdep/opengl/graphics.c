/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/opengl/graphics.c: Tests for general OpenGL functionality.
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
 * get_binding:  Return the GL object bound to the given target.
 *
 * [Parameters]
 *     target: GL target (GL_TEXTURE_BINDING_2D, etc.).
 * [Return value]
 *     Currently bound object, or ~0 on error.
 */
static GLuint get_binding(GLenum target)
{
    GLint object = -1;
    glGetIntegerv(target, &object);
    return (GLuint)object;
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

static int do_test_opengl_graphics(void);
int test_opengl_graphics(void)
{
    return run_tests_in_window(do_test_opengl_graphics);
}

DEFINE_GENERIC_TEST_RUNNER(do_test_opengl_graphics)

TEST_INIT(init)
{
    graphics_start_frame();
    return 1;
}

TEST_CLEANUP(cleanup)
{
    ASSERT(opengl_set_delete_buffer_size(0));
    graphics_finish_frame();
    graphics_flush_resources();
    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_enable_debug)
{
    if (!dyngl_has_debug_output()) {
        SKIP("GL debug output not implemented on this system.");
    }
    const int has_debug_output =
#ifdef SIL_OPENGL_ES
        opengl_has_extension("GL_KHR_debug");
#else
        opengl_version_is_at_least(4,3)
            || opengl_has_extension("GL_ARB_debug_output");
#endif
    if (!has_debug_output) {
        SKIP("GL debug output not available on this system.");
    }

    /* The default should be to not log debug messages. */
    do_DLOG(NULL, 0, NULL, "foo");
    glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_OTHER, 123,
                         GL_DEBUG_SEVERITY_HIGH, -1, "test message");
    CHECK_STREQUAL(test_DLOG_last_message, "foo");

    /* Check that debug messages can be enabled.  The GL implementation may
     * ignore glDebugMessageInsert() calls on a non-debug context, so force
     * context recreation if possible. */
    if (!graphics_set_display_attr("opengl_debug", 1)) {
        SKIP("GL debug output not supported on this system.");
    }
    const int width = graphics_display_width();
    const int height = graphics_display_height();
#if defined(SIL_PLATFORM_LINUX)
    linux_close_window();
#elif defined(SIL_PLATFORM_MACOSX)
    macosx_close_window();
#elif defined(SIL_PLATFORM_WINDOWS)
    windows_close_window();
#endif
    CHECK_TRUE(graphics_set_display_mode(width, height, 0));
    do_DLOG(NULL, 0, NULL, "bar");
    opengl_clear_error();
    glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_OTHER, 456,
                         GL_DEBUG_SEVERITY_LOW, -1, "test message");
    CHECK_INTEQUAL(glGetError(), GL_NO_ERROR);
    glFinish();
    const char *last_log_message = strstr(test_DLOG_last_message, "): ");
    if (last_log_message) {
        last_log_message += 3;
    } else {
        last_log_message = test_DLOG_last_message;
    }
    const char *expected =
        "GL message: [application other-type 456 low-severity] test message";
    CHECK_STREQUAL(last_log_message, expected);

    /* Check that debug messages can be disabled while the window is still
     * open. */
    graphics_set_display_attr("opengl_debug", 0);
    do_DLOG(NULL, 0, NULL, "quux");
    glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_OTHER, 789,
                         GL_DEBUG_SEVERITY_LOW, -1, "test message");
    CHECK_STREQUAL(test_DLOG_last_message, "quux");

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_delayed_delete)
{
    GLuint buffer = 0, framebuffer = 0, program = 0, renderbuffer = 0;
    GLuint shader = 0, texture = 0;

    opengl_clear_error();
    glGenBuffers(1, &buffer);
    ASSERT(buffer);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    ASSERT(program = glCreateProgram());
    ASSERT(shader = glCreateShader(GL_VERTEX_SHADER));
    if (opengl_has_features(OPENGL_FEATURE_FRAMEBUFFERS)) {
        glGenFramebuffers(1, &framebuffer);
        ASSERT(framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glGenRenderbuffers(1, &renderbuffer);
        ASSERT(renderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
    }
    glGenTextures(1, &texture);
    ASSERT(texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    CHECK_INTEQUAL(glGetError(), GL_NO_ERROR);

    if (opengl_has_features(OPENGL_FEATURE_DELAYED_DELETE)) {
        /* Delayed delete is enabled, so check that "deleted" objects are
         * still accessible until we force a delete. */

        opengl_delete_buffer(buffer);
        opengl_delete_program(program);
        opengl_delete_shader(shader);
        CHECK_INTEQUAL(get_binding(GL_ARRAY_BUFFER_BINDING), buffer);
        CHECK_TRUE(glIsProgram(program));
        CHECK_TRUE(glIsShader(shader));
        if (opengl_has_features(OPENGL_FEATURE_FRAMEBUFFERS)) {
            opengl_delete_framebuffer(framebuffer);
            opengl_delete_renderbuffer(renderbuffer);
            CHECK_INTEQUAL(get_binding(GL_FRAMEBUFFER_BINDING), framebuffer);
            CHECK_INTEQUAL(get_binding(GL_RENDERBUFFER_BINDING), renderbuffer);
        }
        opengl_delete_texture(texture);
        CHECK_INTEQUAL(get_binding(GL_TEXTURE_BINDING_2D), texture);

        opengl_free_dead_resources(1);

    } else {
        /* Delayed delete is disabled, so check that objects are actually
         * deleted as soon as we request the delete. */

        opengl_delete_buffer(buffer);
        opengl_delete_program(program);
        opengl_delete_shader(shader);
        if (opengl_has_features(OPENGL_FEATURE_FRAMEBUFFERS)) {
            opengl_delete_framebuffer(framebuffer);
            opengl_delete_renderbuffer(renderbuffer);
        }
        opengl_delete_texture(texture);
    }

    CHECK_INTEQUAL(get_binding(GL_ARRAY_BUFFER_BINDING), 0);
    CHECK_FALSE(glIsProgram(program));
    CHECK_FALSE(glIsShader(shader));
    if (opengl_has_features(OPENGL_FEATURE_FRAMEBUFFERS)) {
        CHECK_INTEQUAL(get_binding(GL_FRAMEBUFFER_BINDING), 0);
        CHECK_INTEQUAL(get_binding(GL_RENDERBUFFER_BINDING), 0);
    }
    CHECK_INTEQUAL(get_binding(GL_TEXTURE_BINDING_2D), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_delayed_delete_fixed_buffer_size)
{
    if (!opengl_has_features(OPENGL_FEATURE_DELAYED_DELETE)) {
        return 1;
    }

    GLuint texture[2];

    CHECK_TRUE(opengl_set_delete_buffer_size(1));
    const int64_t mem_used = mem_debug_bytes_allocated();

    opengl_clear_error();
    glGenTextures(2, texture);
    ASSERT(texture[0]);
    ASSERT(texture[1]);
    glBindTexture(GL_TEXTURE_2D, texture[0]);
    CHECK_INTEQUAL(glGetError(), GL_NO_ERROR);
    CHECK_INTEQUAL(get_binding(GL_TEXTURE_BINDING_2D), texture[0]);

    opengl_delete_texture(texture[0]);
    CHECK_INTEQUAL(get_binding(GL_TEXTURE_BINDING_2D), texture[0]);

    /* This should force texture[0] to be deleted. */
    opengl_delete_texture(texture[1]);
    CHECK_INTEQUAL(get_binding(GL_TEXTURE_BINDING_2D), 0);
    /* texture[1] should still be allocated. */
    glBindTexture(GL_TEXTURE_2D, texture[1]);
    CHECK_INTEQUAL(get_binding(GL_TEXTURE_BINDING_2D), texture[1]);

    opengl_free_dead_resources(1);
    CHECK_INTEQUAL(mem_debug_bytes_allocated(), mem_used);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_delayed_delete_fixed_buffer_size_memory_failure)
{
    if (!opengl_has_features(OPENGL_FEATURE_DELAYED_DELETE)) {
        return 1;
    }

    GLuint texture;

    TEST_mem_fail_after(0, 1, 0);
    const int result = opengl_set_delete_buffer_size(1);
    TEST_mem_fail_after(-1, 0, 0);
    CHECK_FALSE(result);

    /* Failure to set a fixed size shouldn't prevent delayed delete from
     * working otherwise. */
    opengl_clear_error();
    glGenTextures(1, &texture);
    ASSERT(texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    CHECK_INTEQUAL(glGetError(), GL_NO_ERROR);
    CHECK_INTEQUAL(get_binding(GL_TEXTURE_BINDING_2D), texture);

    opengl_delete_texture(texture);
    CHECK_INTEQUAL(get_binding(GL_TEXTURE_BINDING_2D), texture);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_delayed_delete_buffer_expand_memory_failure)
{
    if (!opengl_has_features(OPENGL_FEATURE_DELAYED_DELETE)) {
        return 1;
    }

    GLuint texture[OPENGL_DELETE_INFO_EXPAND + 1];

    opengl_clear_error();
    glGenTextures(lenof(texture), texture);
    for (int i = 0; i < lenof(texture); i++) {
        ASSERT(texture[i]);
    }
    glBindTexture(GL_TEXTURE_2D, texture[0]);
    CHECK_INTEQUAL(glGetError(), GL_NO_ERROR);
    CHECK_INTEQUAL(get_binding(GL_TEXTURE_BINDING_2D), texture[0]);

    for (int i = 0; i < lenof(texture) - 1; i++) {
        opengl_delete_texture(texture[i]);
    }
    CHECK_INTEQUAL(get_binding(GL_TEXTURE_BINDING_2D), texture[0]);

    /* This should force texture[0] to be deleted. */
    TEST_mem_fail_after(0, 1, 0);
    opengl_delete_texture(texture[lenof(texture) - 1]);
    TEST_mem_fail_after(-1, 0, 0);
    CHECK_INTEQUAL(get_binding(GL_TEXTURE_BINDING_2D), 0);
    glBindTexture(GL_TEXTURE_2D, texture[lenof(texture) - 1]);
    CHECK_INTEQUAL(get_binding(GL_TEXTURE_BINDING_2D),
                   texture[lenof(texture) - 1]);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_major_version)
{
    const char *version_str;
    ASSERT(version_str = (const char *)glGetString(GL_VERSION));
#ifdef SIL_OPENGL_ES
    ASSERT(strncmp(version_str, "OpenGL ES ", 10) == 0
        || strncmp(version_str, "OpenGL ES-CM ", 13) == 0
        || strncmp(version_str, "OpenGL ES-CL ", 13) == 0);
    if (version_str[9] == '-') {
        version_str += 13;
    } else {
        version_str += 10;
    }
#endif
    CHECK_INTEQUAL(opengl_major_version(), strtoul(version_str, NULL, 10));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_minor_version)
{
    const char *version_str;
    ASSERT(version_str = (const char *)glGetString(GL_VERSION));
#ifdef SIL_OPENGL_ES
    if (strncmp(version_str, "OpenGL ES ", 10) == 0) {
        version_str += 10;
    } else if (strncmp(version_str, "OpenGL ES-CM ", 13) == 0
            || strncmp(version_str, "OpenGL ES-CL ", 13) == 0) {
        version_str += 13;
    } else {
        FAIL("Invalid OpenGL ES version string: [%s]", version_str);
    }
#endif
    const char *s = version_str + strspn(version_str, "0123456789");
    if (*s != '.' || s[1] < '0' || s[1] > '9') {
        FAIL("Invalid OpenGL version number: [%s]", version_str);
    }
    CHECK_INTEQUAL(opengl_minor_version(), strtoul(s+1, NULL, 10));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_has_extension)
{
    CHECK_FALSE(opengl_has_extension("--invalid_extension_name!--"));
    CHECK_FALSE(opengl_has_extension("GL_--invalid_extension_name!--"));

    char *extensions;
    if (opengl_major_version() >= 3) {
        ASSERT(extensions = mem_strdup("", 0));
        GLint num_extensions = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);
        for (GLint i = 0; i < num_extensions; i++) {
            const char *extension =
                (const char *)glGetStringi(GL_EXTENSIONS, i);
            if (extension) {
                ASSERT(extensions = mem_realloc(
                           extensions,
                           strlen(extensions) + strlen(extension) + 2, 0));
                strcat(strcat(extensions, " "), extension);  // Safe.
            }
        }
    } else {
        const char *s = (const char *)glGetString(GL_EXTENSIONS);
        if (!s) {
            s = "";
        }
        ASSERT(extensions = mem_strdup(s, 0));
    }
    char *extension = extensions;
    extension += strspn(extension, " ");
    while (*extension) {
        char *eow = extension + strcspn(extension, " ");
        if (*eow) {
            *eow++ = 0;
        }
        /* opengl_has_extension() requires the extension name to start
         * with "GL_", so skip any that don't (e.g. WGL_* with some
         * Windows drivers). */
        if (strncmp(extension, "GL_", 3) == 0) {
            DLOG("Checking extension: %s", extension);
            CHECK_TRUE(opengl_has_extension(extension));
        }
        extension = eow + strspn(eow, " ");
    }
    mem_free(extensions);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_has_extension_empty_name)
{
    CHECK_FALSE(opengl_has_extension("GL_"));
    return 1;
}

/*************************************************************************/
/*************************************************************************/
