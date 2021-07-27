/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/macosx/dialog.m: Routine for displaying a dialog box on Mac OS X.
 */

#import "src/base.h"
#import "src/sysdep/macosx/dialog.h"

#import "src/sysdep/macosx/osx-headers.h"
#import <AppKit/NSAlert.h>
#import <Foundation/NSAutoreleasePool.h>
#import <Foundation/NSString.h>
#import <Foundation/NSThread.h>  // For declaration of -[performSelector...]

/*************************************************************************/
/*************************************************************************/

void macosx_dialog(CFStringRef title, CFStringRef text)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:(NSString *)title];
    [alert setInformativeText:(NSString *)text];
    [alert addButtonWithTitle:@"OK"];
    [alert performSelectorOnMainThread:@selector(runModal) withObject:nil
           waitUntilDone:YES];
    [alert release];
    [pool release];
}

/*************************************************************************/
/*************************************************************************/
