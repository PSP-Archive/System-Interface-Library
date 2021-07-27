/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/opengl/internal.c: Internal helper routines for OpenGL
 * tests.
 */

#include "src/base.h"
#undef SIL_OPENGL_NO_SYS_FUNCS  // Avoid type renaming.
#include "src/graphics.h"
#include "src/sysdep.h"
#include "src/sysdep/opengl/opengl.h"
#include "src/test/base.h"
#include "src/test/sysdep/opengl/internal.h"
#include "src/thread.h"

/*************************************************************************/
/*************************************************************************/

/* Helper function to initialize the graphics subsystem. */
static void init_graphics(void)
{
    /* This wrapper code is taken from run_tests_in_window() and its helper
     * functions in test/graphics/internal.c. */
    int width = 64, height = 64;
    ASSERT(graphics_init());
    if (!graphics_set_display_attr("window", 1)) {
        const GraphicsDisplayModeList *mode_list;
        ASSERT(mode_list = graphics_list_display_modes(0));
        ASSERT(mode_list->num_modes > 0);
        width = mode_list->modes[0].width;
        height = mode_list->modes[0].height;
    }
    ASSERT(graphics_set_display_mode(width, height, NULL));
}

/*-----------------------------------------------------------------------*/

int opengl_has_features_uninitted(unsigned int features)
{
    ASSERT(thread_init());
    init_graphics();
    const int result = opengl_has_features(features);
    graphics_cleanup();
    thread_cleanup();
    return result;
}

/*-----------------------------------------------------------------------*/

int opengl_has_formats_uninitted(unsigned int formats)
{
    ASSERT(thread_init());
    init_graphics();
    const int result = opengl_has_formats(formats);
    graphics_cleanup();
    thread_cleanup();
    return result;
}

/*************************************************************************/
/*************************************************************************/
