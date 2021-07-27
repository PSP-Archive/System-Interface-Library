/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/main.c: Global initialization and cleanup functions.
 */

#include "src/base.h"
#include "src/debug.h"
#include "src/graphics.h"
#include "src/input.h"
#include "src/main.h"
#include "src/resource.h"
#include "src/sound.h"
#include "src/sysdep.h"
#include "src/test.h"
#include "src/thread.h"
#include "src/time.h"
#include "src/userdata.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Override function for sil_main(). */

static int (*
#ifndef SIL_INCLUDE_TESTS
            const
#endif
            p_sil_main)(int, const char **) = sil_main;

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int init_all(void)
{
    /* Call thread_init() and time_init() first because other functions
     * may rely on them. */
    if (!thread_init()) {
        DLOG("thread_init() failed!");
        return 0;
    }
    time_init();

#ifdef DEBUG
    debug_init();
#endif
    if (sys_file_init()) {
        if (graphics_init()) {
            if (input_init()) {
                resource_init();
                sound_init();
                if (userdata_init()) {
                    return 1;
                } else {
                    DLOG("userdata_init() failed!");
                }
                sound_cleanup();
                resource_cleanup();
                input_cleanup();
            } else {
                DLOG("input_init() failed!");
            }
            graphics_cleanup();
        } else {
            DLOG("graphics_init() failed!");
        }
        sys_file_cleanup();
    } else {
        DLOG("sys_file_init() failed!");
    }
#ifdef DEBUG
    debug_cleanup();
#endif
    thread_cleanup();
    return 0;
}

/*-----------------------------------------------------------------------*/

void cleanup_all(void)
{
    userdata_cleanup();
    sound_cleanup();
    resource_cleanup();
    input_cleanup();
    graphics_cleanup();
    sys_file_cleanup();
#ifdef DEBUG
    debug_cleanup();
#endif
    thread_cleanup();
}

/*-----------------------------------------------------------------------*/

int sil__main(int argc, const char **argv)
{
    if (!init_all()) {
        return 2;
    }
    int exit_code = (*p_sil_main)(argc, argv);
    ASSERT(exit_code == EXIT_SUCCESS || exit_code == EXIT_FAILURE,
           exit_code = EXIT_FAILURE);
    cleanup_all();
    return exit_code == EXIT_SUCCESS ? 0 : 1;
}

/*************************************************************************/
/************************* Test control routines *************************/
/*************************************************************************/

#ifdef SIL_INCLUDE_TESTS

extern void TEST_override_sil_main(int (*function)(int, const char **))
{
    if (function) {
        p_sil_main = function;
    } else {
        p_sil_main = sil_main;
    }
}

#endif

/*************************************************************************/
/*************************************************************************/
