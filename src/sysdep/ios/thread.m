/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/ios/thread.m: POSIX thread helpers for iOS.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/math/fpu.h"
#include "src/sysdep.h"
#include "src/sysdep/ios/view.h"
#include "src/sysdep/posix/thread.h"

/*************************************************************************/
/*************************************************************************/

void posix_thread_runner_init(UNUSED SysThread *thread)
{
    /* iOS on arm64 resets FPCR in new threads, so we have to reconfigure. */
    fpu_configure();
}

/*-----------------------------------------------------------------------*/

void posix_thread_runner_cleanup(UNUSED SysThread *thread)
{
    [global_view destroyGLContext];
}

/*************************************************************************/
/*************************************************************************/
