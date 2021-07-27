/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/ios/view.h: Header for the SILView class.
 */

#ifndef SIL_SRC_SYSDEP_IOS_VIEW_H
#define SIL_SRC_SYSDEP_IOS_VIEW_H

#import <OpenGLES/EAGL.h>
#import <UIKit/UIView.h>
#import <UIKit/UIViewController.h>

/*************************************************************************/
/*************************************************************************/

/**
 * SILViewController:  Simple view controller, with a dummy view which is
 * added behind the main game view (and therefore invisible).  This kludge
 * is apparently required for the UI orientation to be passed down to the
 * application.
 */

@interface SILViewController: UIViewController
#if defined(__GNUC__) && !defined(__clang__)
{  // This is private, but GCC complains if it's not in the interface.
    UIView *saved_view;
}
#endif

/**
 * initWithView:  Initialize this instance to manage the given view.
 *
 * [Parameters]
 *     view: View to manage.
 * [Return value]
 *     The initialized SILViewController instance.
 */
- (id)initWithView:(UIView *)view;

@end

/*-----------------------------------------------------------------------*/

/**
 * SILView:  UIView subclass implementing OpenGL support.
 */

@interface SILView: UIView

/**
 * createGLContext:  Create (and set as current) an OpenGL context for the
 * current thread which shares GL objects with all other threads.  Must be
 * called exactly once from each thread that performs rendering.
 *
 * Note that because all threads share the same GL objects, care must be
 * taken to avoid interference between threads.
 *
 * [Parameters]
 *     forRendering: True to create a context suitable for rendering;
 *         false to create a context suitable only for shader compilation.
 * [Return value]
 *     True on success, false on error.
 */
- (BOOL)createGLContext:(BOOL)forRendering;

/**
 * destroyGLContext:  Destroy the OpenGL context for the current thread.
 */
- (void)destroyGLContext;

/**
 * setRefreshRate:  Set the desired refresh rate.  The actual rate chosen
 * may differ if the requested rate is not a factor of the display's
 * native refresh rate.
 *
 * [Parameters]
 *     rate: Desired refresh rate, in Hz.
 */
- (void)setRefreshRate:(int)rate;

/**
 * present:  Present the current contents of the OpenGL framebuffer to the
 * display device.  Must be called from a thread with a valid OpenGL context.
 */
- (void)present;

/**
 * waitForPresent:  Wait until the -[present] method has been called at
 * least once.
 */
- (void)waitForPresent;

/**
 * abandonWaitForPresent:  If -[waitForPresent] is still waiting, force it
 * to terminated.
 */
- (void)abandonWaitForPresent;

/**
 * vsync:  Wait for the next vertical sync event.
 */
- (void)vsync;

/**
 * getFrameCounter:  Return the current frame counter.  This is incremented
 * by 1 for each frame (typically 1/60 second) that passes, rolling over
 * from INT_MAX to INT_MIN as necessary.
 *
 * [Return value]
 *     Frame counter.
 */
- (int)getFrameCounter;

@end

/*************************************************************************/

/**
 * global_vc:  Exported pointer to the view controller used to manage
 * device rotation.  (This is set in main.m.)
 */
extern SILViewController *global_vc;

/**
 * global_view:  Exported pointer to the singleton SILView object created
 * for this program.
 */
extern SILView *global_view;

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_IOS_VIEW_H
