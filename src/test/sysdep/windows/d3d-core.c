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
#include "src/sysdep/windows/internal.h"
#include "src/test/base.h"
#include "src/test/graphics/internal.h"

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_windows_d3d_core)

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
    TEST_windows_force_direct3d = 0;
    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_d3d_backend_name)
{
    GraphicsError error;

    /* OpenGL should be used by default. */
    CHECK_TRUE(graphics_init());
    CHECK_TRUE(graphics_set_display_attr("window", 1));
    CHECK_TRUE(graphics_set_display_mode(TESTW, TESTH, &error));
    CHECK_INTEQUAL(error, GRAPHICS_ERROR_SUCCESS);
    CHECK_TRUE(windows_wgl_context());
    graphics_cleanup();

    /* Check that Direct3D can be selected. */
    CHECK_TRUE(graphics_init());
    CHECK_TRUE(graphics_set_display_attr("window", 1));
    CHECK_TRUE(graphics_set_display_attr("backend_name", "direct3d"));
    CHECK_TRUE(graphics_set_display_mode(TESTW, TESTH, &error));
    CHECK_INTEQUAL(error, GRAPHICS_ERROR_SUCCESS);
    CHECK_FALSE(windows_wgl_context());
    graphics_cleanup();

    /* Check that OpenGL can be re-selected. */
    CHECK_TRUE(graphics_init());
    CHECK_TRUE(graphics_set_display_attr("window", 1));
    CHECK_TRUE(graphics_set_display_attr("backend_name", "direct3d"));
    CHECK_TRUE(graphics_set_display_attr("backend_name", "opengl"));
    CHECK_TRUE(graphics_set_display_mode(TESTW, TESTH, &error));
    CHECK_INTEQUAL(error, GRAPHICS_ERROR_SUCCESS);
    CHECK_TRUE(windows_wgl_context());
    graphics_cleanup();

    /* Check that an invalid backend name doesn't affect the current
     * selection. */
    CHECK_TRUE(graphics_init());
    CHECK_TRUE(graphics_set_display_attr("window", 1));
    CHECK_TRUE(graphics_set_display_attr("backend_name", "direct3d"));
    CHECK_FALSE(graphics_set_display_attr("backend_name", "invalid"));
    CHECK_TRUE(graphics_set_display_mode(TESTW, TESTH, NULL));
    CHECK_INTEQUAL(error, GRAPHICS_ERROR_SUCCESS);
    CHECK_FALSE(windows_wgl_context());
    graphics_cleanup();

    /* Check changing backends at runtime. */
    CHECK_TRUE(graphics_init());
    CHECK_TRUE(graphics_set_display_attr("window", 1));
    CHECK_TRUE(graphics_set_display_mode(TESTW, TESTH, &error));
    CHECK_INTEQUAL(error, GRAPHICS_ERROR_SUCCESS);
    CHECK_TRUE(windows_wgl_context());
    CHECK_TRUE(graphics_set_display_attr("backend_name", "direct3d"));
    CHECK_TRUE(graphics_set_display_mode(TESTW, TESTH, &error));
    CHECK_INTEQUAL(error, GRAPHICS_ERROR_STATE_LOST);
    CHECK_FALSE(windows_wgl_context());
    CHECK_TRUE(graphics_set_display_attr("backend_name", "opengl"));
    CHECK_TRUE(graphics_set_display_mode(TESTW, TESTH, &error));
    CHECK_INTEQUAL(error, GRAPHICS_ERROR_STATE_LOST);
    CHECK_TRUE(windows_wgl_context());
    graphics_cleanup();

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_d3d_core_tests)
{
    DLOG("=== Running core graphics tests under Direct3D ===");

    TEST_windows_force_direct3d = 1;
    const int result = test_graphics_base()
                    && test_graphics_clear_grab()
                    && test_graphics_texture()
                    && test_graphics_primitive()
                    && test_graphics_state()
                    && test_graphics_framebuffer()
                    && test_graphics_misc()
                    && test_graphics_texture_formats();
    TEST_windows_force_direct3d = 0;

    DLOG("=== Finished running core graphics tests under Direct3D ===");
    if (!result) {
        FAIL("Preceding failure(s) occurred while using Direct3D");
    }

    return 1;
}

/*************************************************************************/
/*************************************************************************/
