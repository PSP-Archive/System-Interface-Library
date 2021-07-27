/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/ios/dialog.m: Routine for displaying a dialog box on iOS.
 */

#define IN_SYSDEP

#import "src/base.h"
#import "src/sysdep.h"
#import "src/sysdep/ios/dialog.h"
#import "src/sysdep/ios/util.h"
#import "src/sysdep/ios/view.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Local routine declarations. */

/**
 * show_dialog():  Vertical sync helper for ios_dialog() which actually
 * creates and shows the UIAlertView for the dialog.
 *
 * [Parameters]
 *     delegate: DialogAlertViewDelegate instance for the dialog.
 */
static void show_dialog(void *delegate_);

/*-----------------------------------------------------------------------*/

/* Forward declaration of UIAlertView delegate class. */

@interface DialogAlertViewDelegate: NSObject <UIAlertViewDelegate> {
  @public
    int closed;  // True if the dialog has been closed.
    int result;  // True if "Yes" was selected, false if "No" or "OK".

    /* Parameters for the dialog (since the UIAlertView object must be
     * created in the main thread). */
    NSString *nstitle;
    NSString *nstext;
}
@end

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

void ios_dialog(CFStringRef title, CFStringRef text)
{
    /* Ensure the main thread is in the iOS run loop, so the dialog is
     * properly displayed (and the vertical-sync function gets called in
     * the first place). */
    [global_view abandonWaitForPresent];

    DialogAlertViewDelegate *delegate = [[DialogAlertViewDelegate alloc] init];
    delegate->nstitle = (NSString *)title;
    delegate->nstext = (NSString *)text;
    ios_register_vsync_function(show_dialog, delegate);

    while (!delegate->closed) {
        ios_vsync();
    }

    [delegate release];
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static void show_dialog(void *delegate_)
{
    DialogAlertViewDelegate *delegate = (DialogAlertViewDelegate *)delegate_;
    UIAlertView *alert = [[UIAlertView alloc] initWithTitle:delegate->nstitle
                                              message:delegate->nstext
                                              delegate:delegate
                                              cancelButtonTitle:@"OK"
                                              otherButtonTitles:nil];
    [alert show];
    [alert release];
}

/*************************************************************************/
/************************* UIAlertView delegate **************************/
/*************************************************************************/

@implementation DialogAlertViewDelegate

/*-----------------------------------------------------------------------*/

- (id)init
{
    self = [super init];
    closed = 0;
    return self;
}

/*-----------------------------------------------------------------------*/

- (void)alertView:(UIAlertView *)alertView didDismissWithButtonIndex:(NSInteger)buttonIndex
{
    closed = 1;
    result = (buttonIndex != alertView.cancelButtonIndex);
}

/*-----------------------------------------------------------------------*/

@end

/*************************************************************************/
/*************************************************************************/
