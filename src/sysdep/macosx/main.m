/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/macosx/main.m: Program entry point for Mac OS X builds.
 */

#define IN_SYSDEP

#undef SIL_MEMORY_FORBID_MALLOC  // See below.

#import "src/base.h"
#import "src/main.h"
#import "src/math/fpu.h"
#import "src/sysdep.h"
#import "src/sysdep/macosx/dialog.h"
#import "src/sysdep/macosx/graphics.h"
#import "src/sysdep/macosx/input.h"
#import "src/sysdep/macosx/util.h"
#import "src/thread.h"
#import "src/utility/misc.h"

#import <signal.h>
#import <sys/sysctl.h>
#import <time.h>

#import "src/sysdep/macosx/osx-headers.h"
#import <AppKit/NSApplication.h>
#import <AppKit/NSMenu.h>

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Thread in which sil_main() runs. */
static SysThreadID sil_main_thread;

/* Path of the directory containing the package's resource files. */
static char *resource_dir;

/* argc/argv to pass to sil_main(). */
static int client_argc;
static const char **client_argv;

/* Return value from sil__main(). */
static int exit_code;

/* Flag indicating whether -[terminate:] should actually terminate the
 * program.  Set by the app delegate after sil__main() returns. */
static uint8_t really_terminate = 0;

/*************************************************************************/
/**************************** Signal handlers ****************************/
/*************************************************************************/

/**
 * term_signal_handler:  Signal handler for ordinary termination signals
 * (SIGTERM, SIGINT, and SIGHUP).  Sets the quit-requested flag and
 * discards the signal.
 *
 * [Parameters]
 *     signum: Signal number (unused).
 */
static void term_signal_handler(UNUSED int signum)
{
    macosx_quit_requested = 1;
}

/*-----------------------------------------------------------------------*/

/**
 * fatal_signal_handler:  Signal handler for fatal signals (SIGSEGV,
 * SIGQUIT, etc).  Attempts to reset the video mode if it was changed,
 * then terminates the program.
 *
 * [Parameters]
 *     signum: Signal number.
 */
static void fatal_signal_handler(int signum)
{
    macosx_reset_video_mode();
    signal(signum, SIG_DFL);
    kill(getpid(), signum);
}

/*************************************************************************/
/*********************** sil_main() thread runner ************************/
/*************************************************************************/

/**
 * call_sil_main:  Wrapper for the sil__main() function, started by the
 * application delegate as a separate thread.  Calls sil__main(), then
 * sends a terminate message to the global application instance.
 *
 * [Parameters]
 *     unused: Thread parameter (unused).
 * [Return value]
 *     0
 */
static int call_sil_main(UNUSED void *unused)
{
    exit_code = sil__main(client_argc, client_argv);

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    [[NSApplication sharedApplication] stop:nil];
    /* -[stop] doesn't take effect until the event loop receives a message,
     * so send it one. */
    NSEvent *event = [NSEvent otherEventWithType:NSApplicationDefined
                              location:(NSPoint){0,0} modifierFlags:0
                              timestamp:0 windowNumber:0 context:0 subtype:0
                              data1:0 data2:0];
    [[NSApplication sharedApplication] postEvent:event atStart:NO];
    [pool release];

    return 0;
}

/*************************************************************************/
/************************** Menu setup routines **************************/
/*************************************************************************/

/**
 * create_app_menu:  Create the application menu for the program.  This
 * function must be called before any other menus are created.
 *
 * This function assumes that an autorelease pool is in place.
 */
static void create_app_menu(void)
{
    NSString *app_name =
        [NSString stringWithUTF8String:macosx_get_application_name()];

    /* Create the menu and its items. */

    NSMenu *menu = [[NSMenu alloc] initWithTitle:@""];

    [menu addItemWithTitle:[@"About " stringByAppendingString:app_name]
             action:@selector(orderFrontStandardAboutPanel:)
             keyEquivalent:@""];

    [menu addItem:[NSMenuItem separatorItem]];

    [menu addItemWithTitle:[@"Hide " stringByAppendingString:app_name]
             action:@selector(hide:)
             keyEquivalent:@"h"];

    [[menu addItemWithTitle:@"Hide Others"
              action:@selector(hideOtherApplications:)
              keyEquivalent:@"h"]
        setKeyEquivalentModifierMask:(NSAlternateKeyMask|NSCommandKeyMask)];

    [menu addItemWithTitle:@"Show All"
             action:@selector(unhideAllApplications:)
             keyEquivalent:@""];

    [menu addItem:[NSMenuItem separatorItem]];

    [menu addItemWithTitle:[@"Quit " stringByAppendingString:app_name]
             action:@selector(terminate:)
             keyEquivalent:@"q"];

    /* Add the menu to the menu bar.  Since (by contract) this is the
     * first menu in the menu bar, it will be taken as the application
     * menu; see "Nibless apps and the application menu" at
     * <https://developer.apple.com/library/mac/releasenotes/AppKit/RN-AppKitOlderNotes/>. */

    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:@""
                                           action:nil keyEquivalent:@""];
    [item setSubmenu:menu];
    [[NSApp mainMenu] addItem:item];

    /* Release our local copies of the menu objects. */

    [menu release];
    [item release];
}

/*-----------------------------------------------------------------------*/

/**
 * create_window_menu:  Create a simple "Window" menu for the program.
 *
 * This function assumes that an autorelease pool is in place.
 */
static void create_window_menu(void)
{
    NSMenu *menu;

    /* Create the menu and its solitary item. */

    menu = [[NSMenu alloc] initWithTitle:@"Window"];

    [menu addItemWithTitle:@"Minimize"
          action:@selector(performMiniaturize:)
          keyEquivalent:@"m"];

    /* Add the menu to the menu bar. */

    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:@"Window"
                                           action:nil keyEquivalent:@""];
    [item setSubmenu:menu];
    [[NSApp mainMenu] addItem:item];

    /* Register this menu as the application's window menu. */

    [NSApp setWindowsMenu:menu];

    /* Release our local copies of the menu objects. */

    [menu release];
    [item release];
}

/*************************************************************************/
/******************* Application and delegate classes ********************/
/*************************************************************************/

/**
 * SILApplication:  Subclass of NSApplication which implements SIL-specific
 * behaviors.
 */

@interface SILApplication: NSApplication
@end

@implementation SILApplication

/* This is called when the Quit menu item is selected; we translate that
 * into a quit request. */
- (void)terminate:(id)sender
{
    if (really_terminate) {
        [super terminate:sender];
    } else {
        macosx_quit_requested = 1;
    }
}

@end

/*-----------------------------------------------------------------------*/

/**
 * SILAppDelegate:  Application delegate.
 */

@interface SILAppDelegate : NSObject <NSApplicationDelegate>
@end

@implementation SILAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *) UNUSED aNotification
{
#ifdef DEBUG
    /* Log the hardware type and OS version, for reference. */
    size_t machine_size;
    size_t model_size;
    sysctlbyname("hw.machine", NULL, &machine_size, NULL, 0);
    sysctlbyname("hw.model", NULL, &model_size, NULL, 0);
    /* Note that we can't use mem_alloc() here since the memory subsystem
     * hasn't been set up yet. */
    char *machine = malloc(machine_size);
    if (LIKELY(machine)) {
        sysctlbyname("hw.machine", machine, &machine_size, NULL, 0);
    }
    char *model = malloc(model_size);
    if (LIKELY(model)) {
        sysctlbyname("hw.model", model, &model_size, NULL, 0);
    }
    DLOG("Running on: %s (%s), OS X %d.%d.%d", model ? model : "<unknown>",
         machine ? machine : "<unknown>", macosx_version_major(),
         macosx_version_minor(), macosx_version_bugfix());
    free(machine);
    free(model);
#endif

    /* Mark ourselves as the "current" application.  When launched from
     * the command line, this is needed for windows to get properly focused
     * on creation and for the event loop to receive mouse motion events. */
    NSApplication *app = [SILApplication sharedApplication];
    [app setActivationPolicy:NSApplicationActivationPolicyRegular];
    [app activateIgnoringOtherApps:YES];

    /* Look up the directory for resource files. */
    const char *resource_path =
        [[[NSBundle mainBundle] resourcePath] UTF8String];
    if (!resource_path) {
        DLOG("resourcePath not found!");
        resource_dir = NULL;
    } else if (!(resource_dir = strdup(resource_path))) {
        DLOG("Failed to copy resourcePath!");
    }

    /* Start sil_main() in a separate thread (since this one will become
     * the event loop). */
    static const ThreadAttributes attr;  // All zero.
    sil_main_thread = sys_thread_create(&attr, call_sil_main, NULL);
    if (!sil_main_thread) {
        DLOG("sys_thread_create(call_sil_main) failed!");
        macosx_show_dialog_formatted("MACOSX_FRIENDLY_ERROR_TITLE",
                                     "MACOSX_INIT_ERROR_TEXT",
                                     macosx_get_application_name());
        exit(1);
    }
}

@end

/*************************************************************************/
/************************** Program entry point **************************/
/*************************************************************************/

int main(int argc, char **argv_)
{
    const char **argv = (const char **)argv_;

    /* Install signal handlers to avoid being terminated with the display
     * in an unusable state. */
    signal(SIGHUP,    term_signal_handler);
    signal(SIGINT,    term_signal_handler);
    signal(SIGQUIT,   fatal_signal_handler);
    signal(SIGILL,    fatal_signal_handler);
    signal(SIGTRAP,   fatal_signal_handler);
    signal(SIGABRT,   fatal_signal_handler);
    signal(SIGEMT,    fatal_signal_handler);
    signal(SIGFPE,    fatal_signal_handler);
    signal(SIGBUS,    fatal_signal_handler);
    signal(SIGSEGV,   fatal_signal_handler);
    signal(SIGSYS,    fatal_signal_handler);
    signal(SIGPIPE,   SIG_IGN);
    signal(SIGALRM,   SIG_IGN);
    signal(SIGTERM,   term_signal_handler);
    signal(SIGURG,    SIG_IGN);
    signal(SIGTTIN,   SIG_IGN);
    signal(SIGTTOU,   SIG_IGN);
    signal(SIGIO,     SIG_IGN);
    signal(SIGXCPU,   fatal_signal_handler);
    signal(SIGXFSZ,   fatal_signal_handler);
    signal(SIGVTALRM, SIG_IGN);
    signal(SIGPROF,   SIG_IGN);
    signal(SIGWINCH,  SIG_IGN);
    signal(SIGINFO,   SIG_IGN);
    signal(SIGUSR1,   SIG_IGN);
    signal(SIGUSR2,   SIG_IGN);

    /* Configure a consistent floating-point environment. */
    fpu_configure();

    /* Sanity-check program arguments. */
    const char *argv_dummy[2];
    if (argc == 0) {
        DLOG("argc is zero, OS bug?");
        argc = 1;
        argv = argv_dummy;
        argv[0] = "SIL";
        argv[1] = NULL;
    } else if (!argv[0]) {
        DLOG("argv[0] is null, OS bug?");
        argc = 1;
        argv = argv_dummy;
        argv[0] = "SIL";
        argv[1] = NULL;
    }

    /* Check whether we were started from the Finder or the command
     * line.  If we were started from the Finder, null out all
     * command line parameters except the program name. */
    if (argc > 1 && strncmp(argv[1], "-psn", 4) == 0) {
        argc = 1;
        argv[1] = NULL;
    }

    /* Allocate/initialize basic application objects. */
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSApplication *app = [SILApplication sharedApplication];
    SILAppDelegate *appDelegate = [[SILAppDelegate alloc] init];

    /* Set up a basic menu bar. */
    [app setMainMenu:[[NSMenu alloc] init]];
    create_app_menu();
    create_window_menu();

    /* Start the app running. */
    exit_code = 2;  // In case we don't get as far as calling sil_main().
    client_argc = argc;
    client_argv = (const char **)argv;
    [app setDelegate:appDelegate];
    [app run];

    /* Release application objects and other local data. */
    free(resource_dir);
    [appDelegate release];
    [pool release];

    return exit_code;
}

/*************************************************************************/
/******************** OSX-internal exported routines *********************/
/*************************************************************************/

const char *macosx_resource_dir(void)
{
    return resource_dir ? resource_dir : ".";
}

/*************************************************************************/
/*************************************************************************/
