/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/macosx/window.m: Implementation of SILWindow, a subclass of
 * NSWindow with fullscreen and OpenGL support.
 */

#define IN_SYSDEP

#import "src/base.h"
#import "src/input.h"
#import "src/math.h"
#import "src/memory.h"
#import "src/sysdep.h"
#import "src/sysdep/darwin/time.h"
#import "src/sysdep/opengl/opengl.h"
#import "src/sysdep/macosx/graphics.h"
#import "src/sysdep/macosx/input.h"
#import "src/sysdep/macosx/util.h"
#import "src/sysdep/macosx/view.h"
#import "src/sysdep/macosx/window.h"

#import "src/sysdep/macosx/osx-headers.h"
#import <AppKit/NSCursor.h>
#import <AppKit/NSImage.h>
#import <AppKit/NSMenu.h>
#import <AppKit/NSOpenGL.h>
#import <AppKit/NSScreen.h>
#import <AppKit/NSTrackingArea.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSWindowRestoration.h>
#import <OpenGL/OpenGL.h>
#import <QuartzCore/QuartzCore.h>

/*************************************************************************/
/************************ SILWindow implementation ***********************/
/*************************************************************************/

@interface SILWindow: NSWindow <NSWindowDelegate> {
  @public
    BOOL setup_ok;  // Did -[setup] succeed?
}

/**
 * setup:  Perform post-initialization setup of the window.  On success,
 * the setup_ok field will be set to YES.
 *
 * This function must be called on the main thread.
 *
 * [Parameters]
 *     format: Pixel format for the window.
 */
- (void)setup:(NSOpenGLPixelFormat *)format;

/**
 * setWindowTargetSize:  Set the target size for the window when changing
 * from fullscreen to windowed mode.  If set, the window will be resized to
 * this size during the next transition out of fullscreen mode.
 *
 * [Parameters]
 *     size: Desired window size.
 */
- (void)setWindowTargetSize:(NSSize)size;

/**
 * setFullscreen:  Set whether to display the window as a fullscreen window.
 *
 * [Parameters]
 *     fullscreen: True to display the window in fullscreen mode, false to
 *         display it as a normal window.
 *     screen: Screen on which the window is to be displayed.
 */
- (void)setFullscreen:(BOOL)fullscreen screen:(NSScreen *)screen;
/* Wrappers for calling via -[performSelector]. */
- (void)setFullscreen:(NSScreen *)screen;
- (void)setWindowed:(NSScreen *)screen;

/**
 * isFullscreen:  Return whether the window is currently in fullscreen mode.
 */
- (BOOL)isFullscreen;

/**
 * setShowPointer:  Set whether to display the mouse pointer.
 */
- (void)setShowPointer:(BOOL)show;

/**
 * hasFocus:  Return whether the window currently has input focus.
 */
- (BOOL)hasFocus;

/**
 * isMoving:  Return whether the window is currently moving.
 */
- (BOOL)isMoving;

/**
 * resize:  Resize the window to the given size.
 *
 * [Parameters]
 *     size: New size.
 * [Return value]
 *     True on success, false on error.
 */
- (BOOL)resize:(NSSize)size;

@end

/*-----------------------------------------------------------------------*/

@implementation SILWindow {
    NSUInteger base_style;   // Style flags passed to initializer.
    BOOL use_manual_fs;      // Use manual fullscreen instead of Spaces?
    BOOL is_manual_fs;       // Is the window in manual fullscreen mode?
    BOOL has_focus;          // Does the window currently have focus?
    BOOL is_moving;          // Is the window currently being moved?
    BOOL show_pointer;       // Should we show the mouse pointer?
    BOOL is_pointer_inside;  // Is the pointer currently inside the window?
    BOOL hide_state;         // Current state of +[NSCursor hide].
    NSCursor *empty_cursor;  // Empty image used to hide the mouse pointer.
    NSString *title;         // Window title string.
    NSSize target_size;      // Size from setTargetWindowSize or {0,0} if none.
    NSRect pending_resize;   // Target size/pos for windowDidExitFullScreen.
    int min_aspect_x, min_aspect_y;  // Minimum aspect ratio (0/0 = no limit).
    int max_aspect_x, max_aspect_y;  // Maximum aspect ratio (0/0 = no limit).
}

/*-----------------------------------------------------------------------*/

- (id)initWithContentRect:(NSRect)contentRect
#ifdef __MAC_10_12
                styleMask:(NSWindowStyleMask)windowStyle
#else
                styleMask:(NSUInteger)windowStyle
#endif
                  backing:(NSBackingStoreType)bufferingType
                    defer:(BOOL)deferCreation
{
    self = [super initWithContentRect:contentRect styleMask:windowStyle
                  backing:bufferingType defer:deferCreation];

    self.delegate = self;

    setup_ok = NO;
    base_style = windowStyle;
    has_focus = NO;
    hide_state = NO;
    is_manual_fs = NO;
    is_moving = NO;
    is_pointer_inside = NO;
    max_aspect_x = 0;
    max_aspect_y = 0;
    min_aspect_x = 0;
    min_aspect_y = 0;
    pending_resize.size.width = 0;
    pending_resize.size.height = 0;
    show_pointer = YES;
    target_size.width = 0;
    target_size.height = 0;
    title = NULL;

    /* We use Spaces-style fullscreen only for resizable windows, since
     * the associated APIs require the window to be resizable. */
    use_manual_fs = !(windowStyle & NSResizableWindowMask);

    /* We don't worry about failure of the operations below because
     * (1) it's not fatal (the worst that happens is the cursor doesn't
     * disappear), and (2) there's no good way to test failure anyway. */

    static const uint8_t empty_pixel[4] = {0,0,0,0};
    NSImage *empty_image = macosx_create_image(empty_pixel, 1, 1);
    empty_cursor = [[NSCursor alloc] initWithImage:empty_image
                                     hotSpot:(NSPoint){0,0}];
    [empty_image release];

    return self;
}

/*-----------------------------------------------------------------------*/

- (void)close
{
    if (is_manual_fs) {
        [NSMenu setMenuBarVisible:YES];
    }
    if (hide_state) {
        [NSCursor unhide];
        hide_state = NO;
    }
    [super close];
}

/*-----------------------------------------------------------------------*/

- (void)dealloc
{
    [title release];
    [empty_cursor release];

    [super dealloc];
}

/*-----------------------------------------------------------------------*/

/* OS X seems to make unbalanced calls to retain and release/autorelease
 * on NSWindow objects in a multithreaded environment, so we null out
 * Objective-C memory management to prevent the object from being destroyed
 * until we explicitly destroy it. */

- (id)retain {return self;}
- (oneway void)release {}
- (id)autorelease {return self;}

/* Called by SILWindow_destroy(). */
- (oneway void)destroy {[super release];}

/*-----------------------------------------------------------------------*/

- (void)setup:(NSOpenGLPixelFormat *)format
{
    /* This is required in order to get NSMouseMoved events. */
    self.acceptsMouseMovedEvents = YES;

    /* Prevent window resources from being destroyed when the window is
     * miniaturized. */
    self.oneShot = NO;

    /* Window restoration makes no sense for the type of programs with
     * which SIL is likely to be used.  Disable it so users don't get a
     * "do you want to restore this app's windows?" if the program crashes. */
    self.restorable = NO;

    /* Enable the fullscreen button if the window is resizable and if we're
     * using Spaces. */
    if (!use_manual_fs && (base_style & NSResizableWindowMask)) {
        [self setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
    } else {
        /* Apple doesn't document how to control the fullscreen button
         * (or, for that matter, much of anything regarding the fullscreen
         * collection behavior flags), but Qt bug 48759 suggests that we
         * can set auxiliary mode to disable button-invoked fullscreen
         * without any undesired side effects. */
        [self setCollectionBehavior:NSWindowCollectionBehaviorFullScreenAuxiliary];
    }

    CGRect content_rect = SILWindow_content_frame(self);
    content_rect.origin.x = 0;
    content_rect.origin.y = 0;
    SILView *view = [[SILView alloc] initWithFrame:content_rect
                                     pixelFormat:format];
    [format release];  // Retained by the view.
    if (UNLIKELY(!view)) {
        DLOG("Failed to create view");
        return;
    }
    [self setContentView:view];

    NSTrackingArea *tracking_area =
        [[NSTrackingArea alloc] initWithRect:[view frame]
                                options:NSTrackingCursorUpdate
                                        | NSTrackingMouseEnteredAndExited
                                        | NSTrackingActiveInKeyWindow
                                        | NSTrackingInVisibleRect
                                owner:self userInfo:nil];
    [view addTrackingArea:tracking_area];

    [self makeKeyAndOrderFront:self];
    [self makeMainWindow];

    setup_ok = YES;
}

/*-----------------------------------------------------------------------*/

- (void)setTitle:(NSString *)newTitle
{
    if (newTitle != title) {
        [title release];
        title = [newTitle retain];
    }
    if (!is_manual_fs) {
        [super setTitle:title];
    }
}

/*-----------------------------------------------------------------------*/

/* Windows need to respond to these selectors to become fullscreen. */

- (BOOL)canBecomeKeyWindow
{
    return YES;
}

- (BOOL)canBecomeMainWindow
{
    return YES;
}

/*-----------------------------------------------------------------------*/

/* Handle key window notifications to track focus (we could use -[isKeyWindow],
 * but Apple mysteriously deprecated that in OS X 10.10) and perform
 * appropriate state updates on focus change. */

- (void)becomeKeyWindow
{
    [super becomeKeyWindow];
    has_focus = YES;
    macosx_handle_focus_change(1);
    /* We apparently also need to refresh the cursor.  The argument here is
     * unused, but the OS X 10.11 SDK complains if we pass nil. */
    [self cursorUpdate:(void *)1];
}

- (void)resignKeyWindow
{
    macosx_handle_focus_change(0);
    [super resignKeyWindow];
    has_focus = NO;
}

/*-----------------------------------------------------------------------*/

/* NSTrackingArea callbacks.  We show or hide the pointer depending on the
 * current show state, and track whether the pointer is currently inside
 * the window. */

- (void)cursorUpdate:(NSEvent *) UNUSED event
{
    if ([self isFullscreen]) {
        /* In fullscreen mode, the tracking regions don't seem to work
         * properly, so we unconditionally hide the pointer (if requested)
         * for fullscreen windows. */
        if (show_pointer) {
            [[NSCursor arrowCursor] set];
        } else {
            [empty_cursor set];
        }
    } else if (!is_pointer_inside) {
        /* In most cases, cursorUpdate is not called on a mouseExited event
         * because the mouse has also moved outside the window itself, but
         * it is called if the mouse moves up from the client area to the
         * title bar, so we have to be sure to show the cursor in that case. */
        [[NSCursor arrowCursor] set];
    } else if (show_pointer) {
        [[NSCursor arrowCursor] set];
    } else {
        [empty_cursor set];
    }
}

- (void)mouseEntered:(NSEvent *) UNUSED event
{
    is_pointer_inside = YES;

    /* In OSX 10.7, cursor setting does not seem to work correctly, so we
     * manually hide and unhide the cursor globally. */
    if (!macosx_version_is_at_least(10,8,0)) {
        if (!show_pointer && !hide_state) {
            [NSCursor hide];
            hide_state = YES;
        }
    }
}

- (void)mouseExited:(NSEvent *) UNUSED event
{
    is_pointer_inside = NO;

    if (!macosx_version_is_at_least(10,8,0)) {
        if (hide_state) {
            [NSCursor unhide];
            hide_state = NO;
        }
    }
}

/* Helper for -[setShowPointer:] that only calls [NSCursor set] if the
 * pointer is currently inside the window. */
- (void)updateCursorIfInside
{
    if (is_pointer_inside) {
        /* The argument here is unused, but the OS X 10.11 SDK complains if
         * we pass nil. */
        [self cursorUpdate:(void *)1];

        if (!macosx_version_is_at_least(10,8,0)) {
            if (!show_pointer && !hide_state) {
                [NSCursor hide];
                hide_state = YES;
            } else if (show_pointer && hide_state) {
                [NSCursor unhide];
                hide_state = NO;
            }
        }
    }
}

/*-----------------------------------------------------------------------*/

/* Generic event processing routine.  We use this to detect the end of a
 * window move operation (see the init routine). */

- (void)sendEvent:(NSEvent *)event
{
    [super sendEvent:event];

    if (is_moving && [event type] == NSLeftMouseUp) {
        is_moving = NO;
    }
}

/*-----------------------------------------------------------------------*/

/* Mouse and keyboard input handlers.  Note that even if we didn't process
 * keyboard input, we'd have to define the keyboard input handlers as
 * no-ops to prevent the system from beeping whenever a key was pressed. */

- (void)mouseDown:        (NSEvent *)event {macosx_handle_mouse_event(event);}
- (void)otherMouseDown:   (NSEvent *)event {macosx_handle_mouse_event(event);}
- (void)rightMouseDown:   (NSEvent *)event {macosx_handle_mouse_event(event);}
- (void)mouseUp:          (NSEvent *)event {macosx_handle_mouse_event(event);}
- (void)otherMouseUp:     (NSEvent *)event {macosx_handle_mouse_event(event);}
- (void)rightMouseUp:     (NSEvent *)event {macosx_handle_mouse_event(event);}
- (void)mouseDragged:     (NSEvent *)event {macosx_handle_mouse_event(event);}
- (void)otherMouseDragged:(NSEvent *)event {macosx_handle_mouse_event(event);}
- (void)rightMouseDragged:(NSEvent *)event {macosx_handle_mouse_event(event);}
- (void)mouseMoved:       (NSEvent *)event {macosx_handle_mouse_event(event);}

- (void)scrollWheel:      (NSEvent *)event {macosx_handle_scroll_event(event);}

- (void)keyDown:          (NSEvent *)event {macosx_handle_key_event(event);}
- (void)keyUp:            (NSEvent *)event {macosx_handle_key_event(event);}
- (void)flagsChanged:     (NSEvent *)event {macosx_handle_key_event(event);}

/*-----------------------------------------------------------------------*/

/* Window delegate callbacks. */

- (BOOL)windowShouldClose:(id) UNUSED sender
{
    macosx_quit_requested = 1;
    return NO;
}

- (NSApplicationPresentationOptions)window:(NSWindow *) UNUSED window
      willUseFullScreenPresentationOptions:(NSApplicationPresentationOptions)proposedOptions
{
    /* The default (at least through OSX 10.13) seems to be to auto-hide
     * the menu bar and dock in fullscreen mode, which suits us just fine.
     * If it didn't, we could use this callback to modify the options. */
    return proposedOptions;
}

- (void)windowDidEnterFullScreen:(NSNotification *) UNUSED notification
{
    macosx_window_changed_fullscreen(self, true);
}

- (void)windowDidExitFullScreen:(NSNotification *) UNUSED notification
{
    if (pending_resize.size.width > 0 && pending_resize.size.height > 0) {
        [self resize:pending_resize.size];
        [self setFrameOrigin:pending_resize.origin];
        pending_resize.size.width = 0;
        pending_resize.size.height = 0;
    }
    macosx_window_changed_fullscreen(self, false);
}

- (void)windowWillMove:(NSNotification *) UNUSED notification
{
    is_moving = YES;
}

/* The windowDidMove notification is sadly useless for us because it only
 * tells us when the window has just moved, not when the user has finished
 * the move operation.  Instead, we do like SDL2 and treat a mouse-up event
 * while moving as the end of the move operation. */

- (NSSize)windowWillResize:(NSWindow *) UNUSED sender
                    toSize:(NSSize)frameSize
{
    /* Apply aspect ratio limits, since Cocoa doesn't support them natively
     * (it only supports fixed aspect ratios).  See the Windows resizing
     * implementation (WM_SIZING handler in src/sysdep/windows/graphics.c)
     * for details. */

    const NSSize current_size =
        [self contentRectForFrameRect:[self frame]].size;
    const int current_width = iroundf(current_size.width);
    const int current_height = iroundf(current_size.height);
    NSRect new_frame = [self frame];
    new_frame.size = frameSize;
    const NSSize new_size = [self contentRectForFrameRect:new_frame].size;
    int width = iroundf(new_size.width);
    int height = iroundf(new_size.height);

    if (width * min_aspect_y < height * min_aspect_x) {
        const int width_for_height =
            (height * min_aspect_x + min_aspect_y/2) / min_aspect_y;
        const int height_for_width =
            (width * min_aspect_y + min_aspect_x/2) / min_aspect_x;
        const int dw = width_for_height - current_width;
        const int dh = height_for_width - current_height;
        /* Cocoa doesn't tell us the type of sizing operation in progress,
         * so we have to guess based on the mouse position. */
        const NSPoint mouse = self.mouseLocationOutsideOfEventStream;
        if (mouse.x >= 50 && mouse.x < current_size.width-50) {
            width = width_for_height;
        } else if (mouse.y >= 50 && mouse.y < current_size.width-50) {
            height = height_for_width;
        } else if (abs(dw) < abs(dh)) {
            width = width_for_height;
        } else {
            height = height_for_width;
        }
    }
    if (width * max_aspect_y > height * max_aspect_x) {
        const int width_for_height =
            (height * max_aspect_x + max_aspect_y/2) / max_aspect_y;
        const int height_for_width =
            (width * max_aspect_y + max_aspect_x/2) / max_aspect_x;
        const int dw = width_for_height - current_width;
        const int dh = height_for_width - current_height;
        const NSPoint mouse = self.mouseLocationOutsideOfEventStream;
        if (mouse.x >= 50 && mouse.x < current_size.width-50) {
            width = width_for_height;
        } else if (mouse.y >= 50 && mouse.y < current_size.width-50) {
            height = height_for_width;
        } else if (abs(dw) < abs(dh)) {
            width = width_for_height;
        } else {
            height = height_for_width;
        }
    }

    new_frame.size.width = width;
    new_frame.size.height = height;
    return [self frameRectForContentRect:new_frame].size;
}

/*-----------------------------------------------------------------------*/

- (void)setWindowTargetSize:(NSSize)size
{
    target_size = size;
}

/*-----------------------------------------------------------------------*/

/* Must be called from the UI thread. */
- (void)setFullscreen:(BOOL)fullscreen screen:(NSScreen *)screen
{
    if (fullscreen == [self isFullscreen]) {
        return;  // No change.
    }

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    NSRect new_frame;
    BOOL have_target_size = NO;
    if (!fullscreen) {
        if (target_size.width > 0 && target_size.height > 0) {
            have_target_size = YES;
            new_frame.size = target_size;
            target_size.width = 0;
            target_size.height = 0;
        } else {
            new_frame.size.width = roundf([self frame].size.width);
            new_frame.size.height = roundf([self frame].size.height);
        }
        const CGRect screen_bounds = screen.visibleFrame;
        new_frame.origin.x =
            roundf(screen_bounds.origin.x
                   + (screen_bounds.size.width - new_frame.size.width) / 2);
        new_frame.origin.y =
            roundf(screen_bounds.origin.y
                   + (screen_bounds.size.height - new_frame.size.height) / 2);
    }
    if (!use_manual_fs) {
        [self toggleFullScreen:nil];
        if (fullscreen) {
            // FIXME: need to handle non-default screens
        } else {
            if (have_target_size) {
                [self resize:new_frame.size];
                [self setFrameOrigin:new_frame.origin];
                /* OSX 10.12 and earlier seem to ignore a resize operation
                 * here, so do it in the post-exit-fullscreen callback
                 * as well. */
                pending_resize = new_frame;
            }
        }
    } else {
        if (fullscreen) {
            [NSMenu setMenuBarVisible:NO];
            [self setStyleMask:NSBorderlessWindowMask];
            [self resize:[screen frame].size];
            [self setFrameOrigin:[screen frame].origin];
            [self setHidesOnDeactivate:YES];
            is_manual_fs = YES;
        } else {
            [NSMenu setMenuBarVisible:YES];
            [self setStyleMask:base_style];
            [self resize:new_frame.size];
            [self setFrameOrigin:new_frame.origin];
            if (title) {
                [super setTitle:title];
            }
            [self setHidesOnDeactivate:NO];
            is_manual_fs = NO;
        }
    }

    [pool release];
}

/* Wrappers for calling via -[performSelector]. */
- (void) setFullscreen:(NSScreen *)screen {
    [self setFullscreen:YES screen:screen];
}
- (void) setWindowed:(NSScreen *)screen {
    [self setFullscreen:NO screen:screen];
}

/*-----------------------------------------------------------------------*/

- (BOOL)isFullscreen
{
    if (use_manual_fs) {
        return is_manual_fs;
    } else {
        return (self.styleMask & NSFullScreenWindowMask) != 0;
    }
}

/*-----------------------------------------------------------------------*/

- (void)setResizable:(BOOL)resizable
{
    if (resizable) {
        base_style |= NSResizableWindowMask;
    } else {
        base_style &= ~NSResizableWindowMask;
    }
}

/*-----------------------------------------------------------------------*/

/* Must be called from the UI thread. */
- (void)setResizeLimitMin:(NSSize)minSize
                      max:(NSSize)maxSize
                      minAspectX:(int)minAspectX
                      minAspectY:(int)minAspectY
                      maxAspectX:(int)maxAspectX
                      maxAspectY:(int)maxAspectY
{
    if (minSize.width > 0 && minSize.height > 0) {
        self.contentMinSize = minSize;
    } else {
        self.contentMinSize = (CGSize){1, 1};
    }

    if (maxSize.width > 0 && maxSize.height > 0) {
        self.contentMaxSize = maxSize;
    } else {
        self.contentMaxSize = (CGSize){INT_MAX, INT_MAX};
    }

    if (minAspectX > 0 && minAspectY > 0) {
        min_aspect_x = minAspectX;
        min_aspect_y = minAspectY;
    } else {
        min_aspect_x = 0;
        min_aspect_y = 0;
    }

    if (maxAspectX > 0 && maxAspectY > 0) {
        max_aspect_x = maxAspectX;
        max_aspect_y = maxAspectY;
    } else {
        max_aspect_x = 0;
        max_aspect_y = 0;
    }
}

/*-----------------------------------------------------------------------*/

- (void)setShowPointer:(BOOL)show
{
    if (show != show_pointer) {
        show_pointer = show;
        /* This is probably safe from any thread, but we run it on the
         * main thread to avoid racing with enter/exit events. */
        [self performSelectorOnMainThread:@selector(updateCursorIfInside)
              withObject:nil waitUntilDone:NO];
    }
}

/*-----------------------------------------------------------------------*/

- (BOOL)hasFocus
{
    return has_focus;
}

/*-----------------------------------------------------------------------*/

- (BOOL)isMoving
{
    return is_moving;
}

/*-----------------------------------------------------------------------*/

/* Must be called from the UI thread. */
- (BOOL)resize:(NSSize)size
{
    [self setContentSize:size];

    NSRect content_frame = [self contentRectForFrameRect:[self frame]];
    return content_frame.size.width == size.width
        && content_frame.size.height == size.height;
}

/*-----------------------------------------------------------------------*/

@end

/*************************************************************************/
/*********************** Internal utility routines ***********************/
/*************************************************************************/

/* Helper to create a new window on the main thread. */
typedef struct CreateWindowParams {
    NSRect content_rect;
    unsigned int style_mask;
    NSScreen *screen;
    SILWindow *window_ret;
} CreateWindowParams;
static void do_create_window(void *params_)
{
    CreateWindowParams *params = params_;
    params->window_ret =
        [[SILWindow alloc] initWithContentRect:params->content_rect
                           styleMask:params->style_mask
                           backing:NSBackingStoreBuffered
                           defer:NO
                           screen:params->screen];
}

void *SILWindow_create(int x, int y, int width, int height, int screen,
                       int fullscreen, int resizable,
                       CGLPixelFormatObj pixel_format)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    [CATransaction begin];

    NSRect content_rect;
    content_rect.origin.x = x;
    content_rect.origin.y = y;
    content_rect.size.width = width;
    content_rect.size.height = height;
    unsigned int style_mask = NSTitledWindowMask
                            | NSClosableWindowMask
                            | NSMiniaturizableWindowMask;
    if (resizable) {
        style_mask |= NSResizableWindowMask;
    }
    NSScreen *screen_obj = [[NSScreen screens] objectAtIndex:screen];

    CreateWindowParams params = {
        .content_rect = content_rect,
        .style_mask = style_mask,
        .screen = screen_obj,
        .window_ret = NULL,
    };
    dispatch_sync_f(dispatch_get_main_queue(), &params, do_create_window);
    SILWindow *window = params.window_ret;
    if (UNLIKELY(!window)) {
        DLOG("Failed to create window");
        goto error_return;
    }

    if (fullscreen) {
        [window performSelectorOnMainThread:@selector(setFullscreen:)
                withObject:screen_obj waitUntilDone:YES];
    }

    NSOpenGLPixelFormat *format =
        [[NSOpenGLPixelFormat alloc] initWithCGLPixelFormatObj:pixel_format];
    if (UNLIKELY(!format)) {
        goto error_destroy_window;
    }

    [window performSelectorOnMainThread:@selector(setup:) withObject:format
            waitUntilDone:YES];
    if (UNLIKELY(!window->setup_ok)) {
        goto error_destroy_window;
    }

    /* Create an OpenGL context for the window. */
    ASSERT(![NSOpenGLContext currentContext]);
    if (UNLIKELY(![[window contentView] createGLContext:TRUE])) {
        DLOG("Failed to create OpenGL context");
        goto error_destroy_window;
    }

    [CATransaction commit];
    [pool release];
    return window;

  error_destroy_window:
    [window release];
  error_return:
    [CATransaction commit];
    [pool release];
    return NULL;
}

/*-----------------------------------------------------------------------*/

void SILWindow_destroy(void *window_)
{
    SILWindow *window = (SILWindow *)window_;
    [CATransaction begin];

    if (!window) {
        return;
    }

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    [[window contentView] destroyGLContext];
    [window performSelectorOnMainThread:@selector(close) withObject:nil
            waitUntilDone:YES];
    [window destroy];

    [CATransaction commit];
    [pool release];
}

/*-----------------------------------------------------------------------*/

CGRect SILWindow_frame(void *window_)
{
    PRECOND(window_ != NULL, return (CGRect){{0,0},{0,0}});
    SILWindow *window = (SILWindow *)window_;

    /* [window frame] gives us coordinates with the origin in the lower
     * left corner, but we want to return CoreGraphics-style coordinates
     * with the origin in the upper left corner. */
    return macosx_window_frame_to_CGRect([window frame]);
}

/*-----------------------------------------------------------------------*/

CGRect SILWindow_content_frame(void *window_)
{
    PRECOND(window_ != NULL, return (CGRect){{0,0},{0,0}});
    SILWindow *window = (SILWindow *)window_;

    return macosx_window_frame_to_CGRect(
        [window contentRectForFrameRect:[window frame]]);
}

/*-----------------------------------------------------------------------*/

int SILWindow_is_moving(void *window_)
{
    PRECOND(window_ != NULL, return 0);
    SILWindow *window = (SILWindow *)window_;

    return [window isMoving];
}

/*-----------------------------------------------------------------------*/

int SILWindow_has_focus(void *window_)
{
    PRECOND(window_ != NULL, return 0);
    SILWindow *window = (SILWindow *)window_;

    return [window hasFocus];
}

/*-----------------------------------------------------------------------*/

void SILWindow_set_title(void *window_, const char *title)
{
    PRECOND(window_ != NULL, return);
    PRECOND(title != NULL, return);
    SILWindow *window = (SILWindow *)window_;

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    [window performSelectorOnMainThread:@selector(setTitle:)
            withObject:[NSString stringWithUTF8String:title]
            waitUntilDone:YES];

    [pool release];
}

/*-----------------------------------------------------------------------*/

char *SILWindow_get_title(void *window_)
{
    PRECOND(window_ != NULL, return NULL);
    SILWindow *window = (SILWindow *)window_;

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    NSString *title_str = [window title];
    char *title = NULL;
    if (title_str) {
        const char *s = [title_str UTF8String];
        if (UNLIKELY(!s)) {
            DLOG("Failed to extract title from NSString");
        } else {
            title = mem_strdup(s, MEM_ALLOC_TEMP);
            if (UNLIKELY(!title)) {
                DLOG("No memory for copy of title string");
            }
        }
    }

    [pool release];
    return title;
}

/*-----------------------------------------------------------------------*/

int SILWindow_set_fullscreen(void *window_, int fullscreen, int screen,
                             int width, int height)
{
    PRECOND(window_ != NULL, return 0);
    SILWindow *window = (SILWindow *)window_;

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    [CATransaction begin];

    NSScreen *screen_obj = [[NSScreen screens] objectAtIndex:screen];
    if (!screen_obj) {
        DLOG("Invalid screen: %d", screen);
        screen_obj = [[NSScreen screens] objectAtIndex:0];
    }

    int success;
    if (fullscreen) {
        [window performSelectorOnMainThread:@selector(setFullscreen:)
                withObject:screen_obj waitUntilDone:YES];
        success = [window isFullscreen];
    } else {
        [window setWindowTargetSize:(NSSize){width, height}];
        [window performSelectorOnMainThread:@selector(setWindowed:)
                withObject:screen_obj waitUntilDone:YES];
        success = ![window isFullscreen];
    }

    [CATransaction commit];
    [pool release];

    return success;
}

/*-----------------------------------------------------------------------*/

int SILWindow_is_fullscreen(void *window_)
{
    PRECOND(window_ != NULL, return 0);
    SILWindow *window = (SILWindow *)window_;

    return [window isFullscreen];
}

/*-----------------------------------------------------------------------*/

void SILWindow_set_show_pointer(void *window_, int show)
{
    PRECOND(window_ != NULL, return);
    SILWindow *window = (SILWindow *)window_;

    [window setShowPointer:(show != 0)];
}

/*-----------------------------------------------------------------------*/

/* Helper for calling -[resize:] on the UI thread. */
typedef struct ResizeParams {
    SILWindow *window;
    NSSize size;
    BOOL result;
} ResizeParams;
static void do_resize(void *params_)
{
    ResizeParams *params = params_;
    params->result = [params->window resize:params->size];
}

int SILWindow_resize(void *window_, int width, int height)
{
    PRECOND(window_ != NULL, return 0);
    SILWindow *window = (SILWindow *)window_;

    ResizeParams params =
        {.window = window, .size = {width, height}, .result = NO};
    dispatch_sync_f(dispatch_get_main_queue(), &params, do_resize);
    return params.result;
}

/*-----------------------------------------------------------------------*/

void SILWindow_set_resizable(void *window_, int resizable)
{
    PRECOND(window_ != NULL, return);
    SILWindow *window = (SILWindow *)window_;

    [window setResizable:(resizable != 0)];
}

/*-----------------------------------------------------------------------*/

/* UI thread helper. */
typedef struct SetResizeLimitsParams {
    SILWindow *window;
    int min_width, min_height;
    int max_width, max_height;
    int min_aspect_x, min_aspect_y;
    int max_aspect_x, max_aspect_y;
} SetResizeLimitsParams;
static void do_set_resize_limits(void *params_)
{
    SetResizeLimitsParams *params = params_;
    [params->window setResizeLimitMin:(NSSize){params->min_width,
                                               params->min_height}
                                  max:(NSSize){params->max_width,
                                               params->max_height}
                           minAspectX:params->min_aspect_x
                           minAspectY:params->min_aspect_y
                           maxAspectX:params->max_aspect_x
                           maxAspectY:params->max_aspect_y];
}

void SILWindow_set_resize_limits(void *window_,
                                 int min_width, int min_height,
                                 int max_width, int max_height,
                                 int min_aspect_x, int min_aspect_y,
                                 int max_aspect_x, int max_aspect_y)
{
    PRECOND(window_ != NULL, return);
    SILWindow *window = (SILWindow *)window_;

    SetResizeLimitsParams params = {
        .window = window,
        .min_width = min_width, .min_height = min_height,
        .max_width = max_width, .max_height = max_height,
        .min_aspect_x = min_aspect_x, .min_aspect_y = min_aspect_y,
        .max_aspect_x = max_aspect_x, .max_aspect_y = max_aspect_y};
    dispatch_sync_f(dispatch_get_main_queue(), &params, do_set_resize_limits);
}

/*-----------------------------------------------------------------------*/

void SILWindow_update_gl_context(void *window_)
{
    PRECOND(window_ != NULL, return);
    SILWindow *window = (SILWindow *)window_;

    [[window contentView] updateGLContext];
}

/*-----------------------------------------------------------------------*/

int SILWindow_create_gl_shader_compilation_context(void *window_)
{
    PRECOND(window_ != NULL, return 0);
    SILWindow *window = (SILWindow *)window_;

    ASSERT(![NSOpenGLContext currentContext], return 0);
    return [[window contentView] createGLContext:FALSE];
}

/*************************************************************************/
/*************************************************************************/
