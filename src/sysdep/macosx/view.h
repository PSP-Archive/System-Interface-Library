/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/macosx/view.h: Header for the SILView class.
 */

#ifndef SIL_SRC_SYSDEP_MACOSX_VIEW_H
#define SIL_SRC_SYSDEP_MACOSX_VIEW_H

#include "src/sysdep/macosx/osx-headers.h"
#include <AppKit/NSOpenGL.h>
#include <AppKit/NSOpenGLView.h>
#include <OpenGL/OpenGL.h>

/*************************************************************************/
/*************************************************************************/

/**
 * SILView:  NSView subclass implementing OpenGL support.
 */

@interface SILView: NSOpenGLView {
  @private
    NSOpenGLContext *context;
}

/**
 * createGLContext:  Create (and set as current) an OpenGL context for the
 * current thread which is associated with this view.
 *
 * Currently, only one thread may have an OpenGL rendering context for a view.
 * Any number of non-rendering contexts may be created.
 *
 * [Parameters]
 *     forRendering: True to create a context suitable for rendering;
 *         false to create a context suitable only for shader compilation.
 * [Return value]
 *     True on success, false on error.
 */
- (BOOL)createGLContext:(BOOL)forRendering;

/**
 * updateGLContext:  Update the current thread's OpenGL context to account
 * for any changes in the window size or position.  This function does
 * nothing if the OS has not signaled that an update is necessary.
 */
- (void)updateGLContext;

/**
 * destroyGLContext:  Destroy the OpenGL context (if any) for the current
 * thread.
 */
- (void)destroyGLContext;

@end

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_MACOSX_VIEW_H
