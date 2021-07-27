/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/ios/main.m: Program entry point for iOS.
 */

#undef SIL_MEMORY_FORBID_MALLOC  // See below.

#define IN_SYSDEP

#import "src/base.h"
#import "src/input.h"
#import "src/main.h"
#import "src/math/fpu.h"
#import "src/sysdep.h"
#import "src/thread.h"
#import "src/time.h"
#import "src/sysdep/darwin/meminfo.h"
#import "src/sysdep/ios/input.h"
#import "src/sysdep/ios/sound.h"
#import "src/sysdep/ios/util.h"
#import "src/sysdep/ios/view.h"
#import "src/sysdep/opengl/gl-headers.h"
#import "src/utility/misc.h"

#import <pthread.h>
#import <sys/sysctl.h>
#import <time.h>
#import <unistd.h>

#import <UIKit/UIScreen.h>
#import <UIKit/UIWindow.h>

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Thread in which sil_main() runs. */
static SysThreadID sil_main_thread;

/* Path of the directory containing the package's resource files. */
static char *resource_dir;

/*************************************************************************/
/*********************** sil_main() thread runner ************************/
/*************************************************************************/

/**
 * call_sil_main:  Wrapper for the sil__main() function, started by the
 * application delegate as a separate thread.  Calls exit() after
 * sil__main() returns, unless a termination request has been received
 * from the system (in which case the function returns normally and lets
 * the system terminate the process).
 *
 * [Parameters]
 *     unused: Thread parameter (unused).
 * [Return value]
 *     0 (if it returns).
 */
static int call_sil_main(UNUSED void *unused)
{
    /* iOS on arm64 resets FPCR in new threads, so we have to reconfigure. */
    fpu_configure();

    /* Set up an OpenGL rendering context for the main thread. */
    [global_view createGLContext:TRUE];

    /* Actually run the game (or tests). */
    const char *argv[3];
    int argc = 0;
    argv[argc++] = ios_get_application_name();
#if defined(DEBUG) && defined(SIL_INCLUDE_TESTS) && defined(RUN_TESTS)
    argv[argc++] = "-test";
#endif
    argv[argc] = NULL;
    const int exitcode = sil__main(argc, argv);
    BARRIER();
    if (ios_application_is_terminating) {
        return 0;
    } else {
        exit(exitcode);
    }
}

/*-----------------------------------------------------------------------*/

/**
 * do_suspend:  Send a suspend request to the sil_main() thread and wait
 * until the request is acknowledged.
 */
static void do_suspend(void)
{
    /* The semaphore will have a value of 1 after the program first starts
     * up, so clear that out to prevent the main thread from trying to
     * resume immediately. */
    while (sys_semaphore_wait(ios_resume_semaphore, 0)) {/* spin */}

    ios_application_is_suspending = 1;
    sys_semaphore_wait(ios_suspend_semaphore, -1);

    /* Explicitly glFlush() because if anything is still pending in the
     * command buffer, we'll get killed when it goes out to the GPU
     * (according to Apple's GLES programming guide). */
    glFlush();
}

/*************************************************************************/
/********************* UIApplication delegate class **********************/
/*************************************************************************/

@interface SILAppDelegate: NSObject <UIApplicationDelegate>
@end

@implementation SILAppDelegate

/*-----------------------------------------------------------------------*/

- (BOOL)application:(UIApplication *)application
        didFinishLaunchingWithOptions:(NSDictionary *) UNUSED options
{
#ifdef DEBUG
    /* Make sure coverage data is written to an accessible location. */
    char gcov_prefix_buf[1000];
    ASSERT(strformat_check(gcov_prefix_buf, sizeof(gcov_prefix_buf),
                           "%s/coverage", ios_get_application_support_path()));
    ASSERT(setenv("GCOV_PREFIX", gcov_prefix_buf, 1) == 0);

    /* Log the hardware name and OS version, for reference. */
    size_t size;
    sysctlbyname("hw.machine", NULL, &size, NULL, 0);
    /* Note that we can't use mem_alloc() here since the memory subsystem
     * hasn't been set up yet. */
    char *machine = malloc(size);
    if (LIKELY(machine)) {
        sysctlbyname("hw.machine", machine, &size, NULL, 0);
    }
    const char *architecture = "???";
#if defined(SIL_ARCH_ARM_32)
    architecture = "armv7";
#elif  defined(SIL_ARCH_ARM_64)
    architecture = "arm64";
#endif
    DLOG("Running on: %s, iOS %s (%s)", machine ? machine : "<unknown>",
         ios_version_string(), architecture);
    free(machine);
#endif

    /* Look up the directory for resource files. */
    const char *resource_path =
        [[[NSBundle mainBundle] resourcePath] UTF8String];
    if (!resource_path) {
        DLOG("resourcePath not found!");
        resource_dir = NULL;
    } else if (!(resource_dir = strdup(resource_path))) {
        DLOG("Failed to copy resourcePath!");
    }

    /* In some versions of iOS (observed in 6.1 and 7.1.2, but not 5.1.1 or
     * 8.0.2), the main thread's priority is set to the highest priority
     * possible, preventing us from creating any higher priority threads
     * such as for audio.  Find the middle of the allowed priority range
     * and set our priority to that if it's currently higher. */
    {
        int policy;
        struct sched_param sp;
        int error = pthread_getschedparam(pthread_self(), &policy, &sp);
        if (UNLIKELY(error)) {
            DLOG("pthread_getschedparam(): %s", strerror(error));
        } else {
            const int pri_min = sched_get_priority_min(policy);
            const int pri_max = sched_get_priority_max(policy);
            const int pri_mid = (pri_min + pri_max + 1) / 2;
            if (sp.sched_priority > pri_mid) {
                sp.sched_priority = pri_mid;
                error = pthread_setschedparam(pthread_self(), policy, &sp);
                if (UNLIKELY(error)) {
                    DLOG("pthread_setschedparam(priority=%d): %s",
                         pri_mid, strerror(error));
                }
            }
        }
    }

    /* Configure a consistent floating-point environment. */
    fpu_configure();

    /* Enable the idle timer by default.  The idle timer is temporarily
     * disabled when sys_reset_idle_timer() is called. */
    application.idleTimerDisabled = NO;

    /* Create semaphores for signaling suspend/resume events. */
    ios_suspend_semaphore = sys_semaphore_create(0, 1);
    if (UNLIKELY(!ios_suspend_semaphore)) {
        DLOG("Failed to create suspend semaphore");
        exit(1);
    }
    ios_resume_semaphore = sys_semaphore_create(0, 1);
    if (UNLIKELY(!ios_resume_semaphore)) {
        DLOG("Failed to create resume semaphore");
        exit(1);
    }

    /* Create the window and view for the application. */
    CGRect rect = [[UIScreen mainScreen] bounds];
    UIWindow *window = [[UIWindow alloc] initWithFrame:rect];
    /* As of iOS 8.0 (and only for apps built with SDK 8.0 or later), if
     * we're in landscape mode then UIScreen reports its size in landscape
     * rather than portrait orientation, so we don't need to swap the width
     * and height. */
#ifdef __IPHONE_8_0
    if (!ios_version_is_at_least("8.0"))
#endif
    {
        CGFloat temp = rect.size.width;
        rect.size.width = rect.size.height;
        rect.size.height = temp;
    }
    SILView *view = [[SILView alloc] initWithFrame:rect];
    if (!window || !view) {
        DLOG("Failed to create window or view, aborting");
        exit(1);
    }

    /* Create a view controller with the view so we get autorotation.
     * While UIViewController is documented not to take ownership of its
     * views, our SILViewController implementation does in fact take
     * ownership, so we release the view here. */
    SILViewController *vc = [[SILViewController alloc] initWithView:view];
    global_vc = vc;
    [view release];

    /* Install the view controller as the window's root view controller,
     * which will display the view in the window and enable receipt of
     * rotation events. */
    window.rootViewController = vc;

    /* Start the game's main routine in a separate thread (since this one
     * will become the event loop). */
    static const ThreadAttributes attr;  // All zero.
    sil_main_thread = sys_thread_create(&attr, call_sil_main, NULL);
    if (!sil_main_thread) {
        DLOG("sys_thread_create(call_sil_main) failed!");
        /* We can't display an error here because we'd deadlock waiting for
         * this very function to return, so just give up.  We intentionally
         * cause a memory access error so we can track whether this actually
         * happens to anyone. */
        *(int *)0xFA11FA11 = 0;  // Fail! Fail!
        exit(-1);  // Just in case.
    }

    /* Wait until the first present() call, then show the window. */
    [view waitForPresent];
    [window makeKeyAndVisible];

    return YES;
}

/*-----------------------------------------------------------------------*/

- (void)applicationWillResignActive:(UIApplication *) UNUSED application
{
    /* Technically, this is also called when alerts pop up (and while the
     * window is being resized in iOS 9+), but for simplicity (and also
     * because there's no WillEnterBackground hook, only a DidEnterBackground
     * one) we treat WillResignActive as a suspend event; likewise for
     * DidBecomeActive vs. WillEnterForeground. */
    DLOG("Preparing to suspend...");
    do_suspend();
    DLOG("Suspending.");
}

/*-----------------------------------------------------------------------*/

- (void)applicationDidBecomeActive:(UIApplication *) UNUSED application
{
    DLOG("Resuming.");
    ios_application_is_suspending = 0;
    ios_ignore_audio_route_change_until = time_now() + 1.0;
    sys_semaphore_signal(ios_resume_semaphore);
}

/*-----------------------------------------------------------------------*/

- (void)applicationWillTerminate:(UIApplication *) UNUSED application
{
    /* This should normally never be called, but (perhaps due to a bug?)
     * iOS will call this method if the user double-taps the Home button
     * while the program is running and force-quits the program.  Since
     * the program is already suspended in that case, we just call exit()
     * as if the process had been killed by the OS. */

    if (!ios_application_is_suspending) {  // Just in case.
        DLOG("Suspending for termination request.");
        do_suspend();
    }

    DLOG("Terminating program.");
    exit(0);
}

/*-----------------------------------------------------------------------*/

- (void)applicationDidReceiveMemoryWarning:(UIApplication *) UNUSED application
{
    const int64_t selfmem = darwin_get_process_size();
    const int64_t avail = darwin_get_free_memory();
    DLOG("Memory warning: total=%ldk self=%ldk avail=%ldk",
         (long)(darwin_get_total_memory()/1024),
         (long)(selfmem/1024), (long)(avail/1024));
    ios_forward_input_event(&(InputEvent){
        .type = INPUT_EVENT_MEMORY, .detail = INPUT_MEMORY_LOW,
        .timestamp = time_now(),
        {.memory = {.used_bytes = selfmem, .free_bytes = avail}}});
}

/*-----------------------------------------------------------------------*/

@end

/*************************************************************************/
/************************** Program entry point **************************/
/*************************************************************************/

int main(int argc, char **argv)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    int retval = UIApplicationMain(argc, argv,
                                   @"UIApplication", @"SILAppDelegate");
    [pool release];
    return retval;
}

/*************************************************************************/
/******************** iOS-internal exported routines *********************/
/*************************************************************************/

const char *ios_resource_dir(void)
{
    return resource_dir ? resource_dir : ".";
}

/*************************************************************************/
/*************************************************************************/
