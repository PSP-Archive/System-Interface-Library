/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/ios/view.m: Implementation of SILView, a subclass of UIView
 * with OpenGL support.
 */

#define IN_SYSDEP

#import "src/base.h"
#import "src/input.h"
#import "src/memory.h"
#import "src/sysdep.h"
#import "src/sysdep/darwin/time.h"
#import "src/sysdep/opengl/opengl.h"
#import "src/sysdep/ios/input.h"
#import "src/sysdep/ios/util.h"
#import "src/sysdep/ios/view.h"

#import <dlfcn.h>  // For OpenGL function lookups.
#import <time.h>   // For nanosleep().

#import <CoreGraphics/CGGeometry.h>
#import <OpenGLES/EAGL.h>
#import <OpenGLES/EAGLDrawable.h>
#import <QuartzCore/CADisplayLink.h>
#import <QuartzCore/CAEAGLLayer.h>
#import <UIKit/UIScreen.h>
#import <UIKit/UITouch.h>

/* Declare preferredFramesPerSecond when building with a pre-iOS10 SDK.
 * We only reference it on iOS 10+, but we need the declaration
 * unconditionally.  We have to use 10_1 here even though the property
 * was introduced in 10.0 because a Quartz header in the 9.3 SDK (CABase.h)
 * defines 10_0. */
#ifndef __IPHONE_10_1
@interface CADisplayLink (ios10_compat)
@property(nonatomic) NSInteger preferredFramesPerSecond;
@end
#endif

/*************************************************************************/
/*************************************************************************/

/**
 * global_vc:  Exported pointer to the view controller used to manage
 * device rotation.
 */
SILViewController *global_vc;

/**
 * global_view:  Exported pointer to the singleton SILView object created
 * for this program.
 */
SILView *global_view;

/*-----------------------------------------------------------------------*/

/* Array mapping touch object pointers to touch IDs.  A NULL object pointer
 * indicates an unused entry. */
static struct {
    const UITouch *object;
    unsigned int id;
} touch_map[INPUT_MAX_TOUCHES];

/* Next touch ID to use for a new touch.  Incremented by 1 for each touch,
 * rolling over (and skipping zero) if necessary. */
static unsigned int next_touch_id = 1;

/*-----------------------------------------------------------------------*/

/* Context for OpenGL rendering. */
static EAGLContext *context;

/* Framebuffer, color buffer, and depth buffer for display. */
static GLuint framebuffer, color_buffer, depth_buffer;

/* CADisplayLink instance for timing. */
static CADisplayLink *displaylink;

/* Have we done at least one present call yet? */
static int presented;

/* Global frame counter, used by -[vsync] to wait for the next frame. */
static int frame_counter;

/* Semaphore for signaling vertical sync events to -[vsync]. */
static SysSemaphoreID vsync_event_sem;

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * ios_glGetProcAddress:  Replacement for eglGetProcAddress() on iOS.
 *
 * [Parameters]
 *     name: Name of function to look up.
 * [Return value]
 *     Function pointer, or NULL if the function is not found.
 */
static void *ios_glGetProcAddress(const char *name);

/*************************************************************************/
/***************************** Public methods ****************************/
/*************************************************************************/

@implementation SILViewController {
    UIView *saved_view;
}

- (id)initWithView:(UIView *)view
{
    self = [super initWithNibName:nil bundle:nil];

    /* iOS 7.0+ always use full-screen layout, so this is unnecessary. */
#if __IPHONE_OS_VERSION_MIN_REQUIRED < 70000  // __IPHONE_7_0
    if (!ios_version_is_at_least("7.0")) {
        self.wantsFullScreenLayout = YES;
    }
#endif

    /* Prior to iOS 6.0, the OS could force-unload our view.  Save the view
     * pointer and retain a reference to make sure that doesn't happen. */
    self->saved_view = view;
    [view retain];

    return self;
}

- (void)loadView
{
    self.view = self->saved_view;
}

#ifdef __IPHONE_9_0
- (UIInterfaceOrientationMask)supportedInterfaceOrientations
#else
- (NSUInteger)supportedInterfaceOrientations
#endif
{
    return UIInterfaceOrientationMaskLandscape;
}

/* For iOS <6.0. */
- (BOOL)shouldAutorotateToInterfaceOrientation:
    (UIInterfaceOrientation)orientation
{
    return ios_allow_autorotation
        && ([self supportedInterfaceOrientations] & (1 << orientation));
}

- (void)didReceiveMemoryWarning
{
    /* Handled at the app layer. */
}

- (void)dealloc
{
    [self->saved_view release];
    [super dealloc];
}

@end

/*************************************************************************/
/*************************************************************************/

@implementation SILView

/*-----------------------------------------------------------------------*/

+ (Class)layerClass
{
    return [CAEAGLLayer class];
}

/*-----------------------------------------------------------------------*/

- (id)initWithFrame:(CGRect)rect
{
    self = [super initWithFrame:rect];
    if (!self) {
        return nil;
    }

    /* Make sure we get the full resolution on "retina" displays. */
    self.contentScaleFactor = ios_display_scale();

    /* Enable multitouch support for the view.  (Without this, the OS will
     * only send events for the first touch of a multitouch set.) */
    self.multipleTouchEnabled = YES;

    /* Set up the Core Animation layer properties. */
    CAEAGLLayer *layer = (CAEAGLLayer *)self.layer;
    layer.opaque = TRUE;
    layer.drawableProperties = @{
        kEAGLDrawablePropertyRetainedBacking: @(FALSE),
        kEAGLDrawablePropertyColorFormat: kEAGLColorFormatRGBA8,
    };

    /* Create a CADisplayLink instance for vertical sync handling.  If the
     * instance is successfully created, also set up an event semaphore
     * for interaction with the -[vsync] function.  (We proceed even if
     * this fails; -[vsync] then effectively becomes a no-op, so timing
     * accuracy may suffer.) */
    displaylink = [CADisplayLink displayLinkWithTarget:self
                                 selector:@selector(vsync_callback)];
    if (displaylink) {
        [displaylink addToRunLoop:[NSRunLoop currentRunLoop]
                     forMode:NSDefaultRunLoopMode];
        vsync_event_sem = sys_semaphore_create(0, 1);
        if (UNLIKELY(!vsync_event_sem)) {
            DLOG("Failed to create vertical sync semaphore; discarding"
                 " CADisplayLink instance -- timing accuracy will suffer!");
            [displaylink release];
            displaylink = nil;
        }
    } else {
        DLOG("WARNING: Failed to create CADisplayLink instance; timing"
             " accuracy will suffer!");
    }

    /* Save our object pointer in global_view for external access.  (This
     * is done here rather than earlier so we don't have to worry about
     * clearing global_view on failure, though that's a mostly academic
     * issue since the program will terminate if it can't create a view.) */
    global_view = self;

    /* All done! */
    return self;
}

/*-----------------------------------------------------------------------*/

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *) UNUSED event
{
    NSEnumerator *i = [touches objectEnumerator];
    const UITouch *touch;
    while ((touch = [i nextObject]) != nil) {

        /* Compute the event timestamp in the time_now() time base. */
        const double timestamp = touch.timestamp - darwin_time_epoch();

        /* Convert the raw device coordinates into relative coordinates. */
        const CGPoint point = [touch locationInView:self];
        float x = point.x / self.bounds.size.width;
        float y = point.y / self.bounds.size.height;

        /* Make sure this object isn't already in the array.  A little
         * paranoia never hurt anybody... */
        for (int j = 0; j < lenof(touch_map); j++) {
            if (UNLIKELY(touch_map[j].object == touch)) {
                DLOG("WARNING: New touch object %p already in array with ID"
                     " %u, cancelling old touch", touch, touch_map[j].id);
                ios_forward_input_event(&(InputEvent){
                    .type = INPUT_EVENT_TOUCH, .detail = INPUT_TOUCH_CANCEL,
                    .timestamp = timestamp,
                    {.touch = {.id = touch_map[j].id, .x = x, .y = y}}});
                touch_map[j].object = NULL;
            }
        }

        /* Add this touch to the array and generate a SIL event for it. */
        for (int j = 0; j < lenof(touch_map); j++) {
            if (touch_map[j].object == NULL) {
                touch_map[j].object = touch;
                touch_map[j].id = next_touch_id;
                next_touch_id++;
                if (next_touch_id == 0) {
                    next_touch_id++;
                }
                ios_forward_input_event(&(InputEvent){
                    .type = INPUT_EVENT_TOUCH, .detail = INPUT_TOUCH_DOWN,
                    .timestamp = timestamp,
                    {.touch = {.id = touch_map[j].id, .x = x, .y = y}}});
                break;
            }
        }

    }  // while ((touch = [i nextObject]) != nil)
}

/*-----------------------------------------------------------------------*/

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *) UNUSED event
{
    NSEnumerator *i = [touches objectEnumerator];
    const UITouch *touch;
    while ((touch = [i nextObject]) != nil) {
        const double timestamp = touch.timestamp - darwin_time_epoch();
        const CGPoint point = [touch locationInView:self];
        const float x = point.x / self.bounds.size.width;
        const float y = point.y / self.bounds.size.height;
        for (int j = 0; j < lenof(touch_map); j++) {
            if (touch_map[j].object == touch) {
                ios_forward_input_event(&(InputEvent){
                    .type = INPUT_EVENT_TOUCH, .detail = INPUT_TOUCH_MOVE,
                    .timestamp = timestamp,
                    {.touch = {.id = touch_map[j].id, .x = x, .y = y}}});
                break;
            }
        }
    }
}

/*-----------------------------------------------------------------------*/

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *) UNUSED event
{
    NSEnumerator *i = [touches objectEnumerator];
    const UITouch *touch;
    while ((touch = [i nextObject]) != nil) {
        const double timestamp = touch.timestamp - darwin_time_epoch();
        const CGPoint point = [touch locationInView:self];
        const float x = point.x / self.bounds.size.width;
        const float y = point.y / self.bounds.size.height;
        for (int j = 0; j < lenof(touch_map); j++) {
            if (touch_map[j].object == touch) {
                ios_forward_input_event(&(InputEvent){
                    .type = INPUT_EVENT_TOUCH, .detail = INPUT_TOUCH_UP,
                    .timestamp = timestamp,
                    {.touch = {.id = touch_map[j].id, .x = x, .y = y}}});
                touch_map[j].object = NULL;
                break;
            }
        }
    }
}

/*-----------------------------------------------------------------------*/

- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *) UNUSED event
{
    NSEnumerator *i = [touches objectEnumerator];
    const UITouch *touch;
    while ((touch = [i nextObject]) != nil) {
        const double timestamp = touch.timestamp - darwin_time_epoch();
        const CGPoint point = [touch locationInView:self];
        const float x = point.x / self.bounds.size.width;
        const float y = point.y / self.bounds.size.height;
        for (int j = 0; j < lenof(touch_map); j++) {
            if (touch_map[j].object == touch) {
                ios_forward_input_event(&(InputEvent){
                    .type = INPUT_EVENT_TOUCH, .detail = INPUT_TOUCH_CANCEL,
                    .timestamp = timestamp,
                    {.touch = {.id = touch_map[j].id, .x = x, .y = y}}});
                touch_map[j].object = NULL;
                break;
            }
        }
    }
}

/*-----------------------------------------------------------------------*/

- (void)dealloc
{
    [displaylink invalidate];
    global_view = nil;
    [super dealloc];
}

/*************************************************************************/

- (BOOL)createGLContext:(BOOL)forRendering
{
    if (context) {
        /* We already have a primary context, so create a new one, sharing
         * the primary context's objects if rendering support is requested. */
        EAGLSharegroup *sharegroup = forRendering ? [context sharegroup] : nil;
        EAGLContext *sub_context = [[EAGLContext alloc]
                                       initWithAPI:[context API]
                                       sharegroup:sharegroup];
        if (!sub_context) {
            DLOG("Failed to create sub-context");
            return FALSE;
        }
        if (![EAGLContext setCurrentContext:sub_context]) {
            DLOG("Failed to set sub-context as current");
            return FALSE;
        }
        return TRUE;
    }

    /* The main context should always be a rendering context. */
    ASSERT(forRendering);

    /* Create the OpenGL ES context.  We require OpenGL ES 2.0+ for shader
     * support, but we take the most recent version available. */
    context = NULL;
#ifdef __IPHONE_7_0
    if (!context && ios_version_is_at_least("7.0")) {
        context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
    }
#endif
    if (!context) {
        context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
    }
    if (!context || ![EAGLContext setCurrentContext:context]) {
        return FALSE;
    }

    /* Look up all OpenGL symbols before we go any farther. */
    opengl_lookup_functions(ios_glGetProcAddress);

    /* iOS doesn't have a "default" framebuffer; instead, we need to create
     * one and attach the Core Animation layer's pixel buffer to it. */
    CAEAGLLayer *layer = (CAEAGLLayer *)self.layer;
    opengl_clear_error();
    glGenFramebuffers(1, &framebuffer);
    glGenRenderbuffers(1, &color_buffer);
    glGenRenderbuffers(1, &depth_buffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, color_buffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER, color_buffer);
    [context renderbufferStorage:GL_RENDERBUFFER fromDrawable:layer];
    glBindRenderbuffer(GL_RENDERBUFFER, depth_buffer);
    glRenderbufferStorage(
        GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
        (int)(self.bounds.size.width * self.contentScaleFactor),
        (int)(self.bounds.size.height * self.contentScaleFactor));
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, depth_buffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, depth_buffer);
    const int error = glGetError();
    if (error != GL_NO_ERROR) {
        DLOG("Failed to create display framebuffer (0x%X)", error);
        return FALSE;
    }
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        DLOG("Failed to validate display framebuffer (0x%X)",
             glCheckFramebufferStatus(GL_FRAMEBUFFER));
        return FALSE;
    }
    opengl_set_default_framebuffer(framebuffer);

    return TRUE;
}

/*-----------------------------------------------------------------------*/

- (void)destroyGLContext
{
    EAGLContext *this_context = [EAGLContext currentContext];
    [EAGLContext setCurrentContext:nil];
    if (this_context == context) {
        glDeleteRenderbuffers(1, &depth_buffer);
        glDeleteRenderbuffers(1, &color_buffer);
        glDeleteFramebuffers(1, &framebuffer);
        context = nil;
    }
    [this_context release];
}

/*-----------------------------------------------------------------------*/

- (void)setRefreshRate:(int)rate
{
    if (ios_version_is_at_least("10.0")) {
        displaylink.preferredFramesPerSecond = rate;
    }
}

/*-----------------------------------------------------------------------*/

- (void)present
{
    if (ios_application_is_terminating) {
        return;
    }

    if (opengl_has_features(OPENGL_FEATURE_DISCARD_FRAMEBUFFER)) {
        static const GLenum attachments[] = {
            GL_DEPTH_ATTACHMENT,
            GL_STENCIL_ATTACHMENT,
        };
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glDiscardFramebufferEXT(GL_FRAMEBUFFER,
                                lenof(attachments), attachments);
    }

    glBindRenderbuffer(GL_RENDERBUFFER, color_buffer);
    [context presentRenderbuffer:GL_RENDERBUFFER];

    presented = 1;
    BARRIER();
}

/*-----------------------------------------------------------------------*/

- (void)waitForPresent
{
    while (!presented && !ios_application_is_terminating) {
        /* We use a simple, relatively coarse wait here, since this
         * function is only called once (while starting up) and is not
         * time-critical. */
        nanosleep(&(struct timespec){0, 1000000}, NULL);
        BARRIER();
    }
}

/*-----------------------------------------------------------------------*/

- (void)abandonWaitForPresent
{
    presented = 1;
    BARRIER();  // Flush it out immediately.
}

/*-----------------------------------------------------------------------*/

- (void)vsync
{
    if (ios_application_is_terminating) {
        /* We need to quit as soon as possible, so just pretend a frame
         * has passed and return. */
        frame_counter++;
        return;
    }

    if (ios_application_is_suspending) {
        /* We may or may not get vertical sync events while transitioning
         * to the background (not checked), but we shouldn't rely on it.
         * This also avoids deadlocking while the run loop thread waits
         * for suspend acknowledgement. */
        frame_counter++;
        return;
    }

    if (!displaylink) {
        /* If we don't have a CADisplayLink, we'll never get vsync events,
         * so just return and let things fall out as they may.  But update
         * the frame counter so a caller waiting for multiple frames to
         * pass won't get stuck. */
        frame_counter++;
        return;
    }

    const int last_frame = frame_counter;
    do {
        /* Don't wait longer than 0.1 seconds (if the program is
         * terminating/suspending, we won't get any more events, so we
         * have to check ios_application_is_{terminating,suspending} on
         * our own). */
        sys_semaphore_wait(vsync_event_sem, 0.1);
        BARRIER();
    } while (frame_counter == last_frame
             && !ios_application_is_terminating
             && !ios_application_is_suspending);
}

/*-----------------------------------------------------------------------*/

- (int)getFrameCounter
{
    return frame_counter;
}

/*************************************************************************/
/**************************** Private methods ****************************/
/*************************************************************************/

/**
 * vsync_callback:  Private callback function passed to CADisplayLink to be
 * called on each vertical sync event.  Updates the frame counter, and also
 * calls any functions registered to be called on vertical sync.
 */
- (void)vsync_callback
{
    frame_counter++;
    BARRIER();

    /* Clear out any pending value on the semaphore before signaling it,
     * so the value doesn't build up over time.  (We don't need to loop on
     * sys_semaphore_wait() since we only ever signal the semaphore here.) */
    sys_semaphore_wait(vsync_event_sem, 0);
    sys_semaphore_signal(vsync_event_sem);

    ios_call_vsync_functions();
}

/*************************************************************************/

@end

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static void *ios_glGetProcAddress(const char *name)
{
    return dlsym(RTLD_DEFAULT, name);
}

/*************************************************************************/
/*************************************************************************/
