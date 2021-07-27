/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/macosx/thread.m: POSIX thread helpers for Mac OS X.
 */

#define IN_SYSDEP

#import "src/base.h"
#import "src/sysdep.h"
#import "src/sysdep/macosx/graphics.h"
#import "src/sysdep/macosx/window.h"
#import "src/sysdep/posix/thread.h"

#import <AppKit/NSOpenGL.h>

/*************************************************************************/
/*************************************************************************/

void posix_thread_runner_init(UNUSED SysThread *thread)
{
    /* Nothing to do. */
}

/*-----------------------------------------------------------------------*/

void posix_thread_runner_cleanup(UNUSED SysThread *thread)
{
    /* Destroy any GL context which may have been created (for shader
     * compilation) on this thread. */
    NSOpenGLContext *context = [NSOpenGLContext currentContext];
    [NSOpenGLContext clearCurrentContext];
    if (context) {
        [context release];
    }
}

/*************************************************************************/
/*************************************************************************/
