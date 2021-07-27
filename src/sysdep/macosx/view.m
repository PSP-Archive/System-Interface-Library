/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/macosx/view.m: Implementation of SILView, a subclass of
 * NSView with OpenGL support.
 */

#define IN_SYSDEP

#import "src/base.h"
#import "src/input.h"
#import "src/memory.h"
#import "src/sysdep.h"
#import "src/sysdep/darwin/time.h"
#import "src/sysdep/macosx/graphics.h"
#import "src/sysdep/macosx/view.h"
#import "src/sysdep/opengl/opengl.h"

#import "src/sysdep/macosx/osx-headers.h"
#import <AppKit/NSOpenGL.h>
#import <AppKit/NSView.h>
#import <OpenGL/OpenGL.h>

/*************************************************************************/
/***************************** Public methods ****************************/
/*************************************************************************/

@implementation SILView
{
    BOOL update_pending;  // Do we need to update the underlying GL context?
}

/*-----------------------------------------------------------------------*/

+ (SILView *)alloc
{
    SILView *view = [super alloc];
    view->update_pending = FALSE;
    return view;
}

/*-----------------------------------------------------------------------*/

- (void)update
{
    [super update];
    __atomic_store_n(&update_pending, TRUE, __ATOMIC_SEQ_CST);
}

/*-----------------------------------------------------------------------*/

- (BOOL)createGLContext:(BOOL)forRendering
{
    if (context && forRendering) {
        /* We don't currently support rendering from multiple threads. */
        return FALSE;
    }

    /* Create the OpenGL context. */
    NSOpenGLContext *new_context;
    if (forRendering) {
        new_context = self.openGLContext;
    } else {
        new_context = [[NSOpenGLContext alloc] initWithFormat:self.pixelFormat
                                               shareContext:nil];
    }
    if (!new_context) {
        return FALSE;
    }
    [new_context makeCurrentContext];

    if (forRendering) {
        /* Save this as the primary context. */
        context = new_context;
        /* Associate the context with the view. */
        [context setView:self];
        /* Ensure that the context's drawable references are appropriately
         * updated.  (One assumes -[setView:] would do this, but in rare
         * cases, the context starts raising GL_INVALID_FRAMEBUFFER_OPERATION
         * on draw operations.  SDL2 includes an explicit -[update] after
         * -[setView:], though without documentation of why, so we do the
         * same in an attempt to mitigate the problem.) */
        [context update];
    }

    return TRUE;
}

/*-----------------------------------------------------------------------*/

- (void)updateGLContext
{
    if (!__atomic_compare_exchange_n(&update_pending, (BOOL[]){TRUE}, FALSE,
                                     FALSE, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
        return;  // No update was pending.
    }

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    [context update];
    [pool release];
}

/*-----------------------------------------------------------------------*/

- (void)destroyGLContext
{
    NSOpenGLContext *this_context = [NSOpenGLContext currentContext];
    [NSOpenGLContext clearCurrentContext];
    if (this_context) {
        if (this_context == context) {
            context = nil;
        } else {
            [this_context release];
        }
    }
}

/*-----------------------------------------------------------------------*/

@end

/*************************************************************************/
/*************************************************************************/
