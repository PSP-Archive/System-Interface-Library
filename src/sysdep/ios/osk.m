/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/ios/osk.m: Interface to the iOS on-screen keyboard.
 */

#define IN_SYSDEP

#import "src/base.h"
#import "src/memory.h"
#import "src/sysdep.h"
#import "src/sysdep/ios/osk.h"
#import "src/sysdep/ios/view.h"
#import "src/sysdep/ios/util.h"
#import "src/utility/utf8.h"

#import <UIKit/UILabel.h>

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Text field delegate object for text entry. */
@interface OSKTextFieldDelegate: NSObject <UITextFieldDelegate>
@end
static OSKTextFieldDelegate *delegate;

/* View for the currently displayed input field, or nil if the on-screen
 * keyboard is not displayed. */
static UIView *current_osk = nil;

/* Flag indicating whether an OSK open is pending. */
static uint8_t osk_is_coming_up;

/* Flag indicating whether the user is currently entering text. */
static uint8_t osk_is_running;

/* Text entered by the user, terminated by a zero value. */
static int *osk_text;

/* Copy of default text and prompt strings (freed when the OSK is closed). */
static NSString *text_copy, *prompt_copy;

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * create_osk_view:  Create and display the view used for input from the
 * on-screen keyboard.  Called from the run loop thread as a vertical-sync
 * callback.
 *
 * [Parameters]
 *     unused: Callback parameter (unused).
 */
static void create_osk_view(void *unused);

/**
 * destroy_osk_view:  Destroy a view structure for the on-screen keyboard.
 * Called from the run loop thread as a vertical-sync callback.
 *
 * [Parameters]
 *     userdata: Top-level view used for the OSK.
 */
static void destroy_osk_view(void *userdata);

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

void ios_osk_open(const char *text, const char *prompt)
{
    if (current_osk) {
        return;  // Can't open more than one at once.
    }

    if (!delegate) {
        delegate = [[OSKTextFieldDelegate alloc] init];
        if (UNLIKELY(!delegate)) {
            DLOG("Failed to create text field delegate");
            return;
        }
    }

    osk_is_coming_up = 1;
    BARRIER();

    text_copy = [[NSString alloc] initWithUTF8String:text];
    if (UNLIKELY(!text_copy)) {
        DLOG("No memory for default text copy, field will start empty");
    }
    if (prompt) {
        prompt_copy = [[NSString alloc] initWithUTF8String:prompt];
        if (UNLIKELY(!prompt_copy)) {
            DLOG("No memory for prompt string copy, no prompt will be shown");
        }
    }
    ios_register_vsync_function(create_osk_view, NULL);
}

/*-----------------------------------------------------------------------*/

int ios_osk_is_running(void)
{
    return osk_is_coming_up || (current_osk != NULL && osk_is_running);
}

/*-----------------------------------------------------------------------*/

const int *ios_osk_get_text(void)
{
    return osk_text;
}

/*-----------------------------------------------------------------------*/

void ios_osk_close(void)
{
    if (!osk_is_coming_up && current_osk) {
        ios_register_vsync_function(destroy_osk_view, current_osk);
        current_osk = nil;
        mem_free(osk_text);
        osk_text = NULL;
        [text_copy release];
        text_copy = nil;
        [prompt_copy release];
        prompt_copy = nil;
    }
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static void create_osk_view(UNUSED void *unused)
{
    const int border = 5;
    const int width = ios_display_width();
    const int label_height =
        prompt_copy != nil && [prompt_copy length] > 0 ? 17 : 0;
    const int textfield_height = 27;
    const int total_height = (label_height>0 ? label_height+border : 0)
                           + textfield_height + border*2;
    current_osk = [[UIView alloc] initWithFrame:CGRectMake(
        0, 0, width, total_height)];
    if (UNLIKELY(!current_osk)) {
        DLOG("Failed to create OSK view");
        goto out;
    }
    [global_vc.view addSubview:current_osk];
    [current_osk release];  // Retained by the superview.
    current_osk.backgroundColor = [UIColor colorWithWhite:0.0 alpha:0.75];

    if (label_height > 0) {
        UILabel *label = [[UILabel alloc] initWithFrame:CGRectMake(
            border, border, width - 2*border, label_height)];
        [current_osk addSubview:label];
        [label release];
        label.text = prompt_copy;
        label.textAlignment = NSTextAlignmentLeft;
        label.textColor = [UIColor whiteColor];
        label.backgroundColor = [UIColor colorWithWhite:0 alpha:0];
    }

    UITextField *textfield = [[UITextField alloc] initWithFrame:CGRectMake(
        border, total_height - border - textfield_height,
        width - 2*border, textfield_height)];
    if (UNLIKELY(!textfield)) {
        DLOG("Failed to create OSK text field");
        [current_osk removeFromSuperview];
        current_osk = nil;
        goto out;
    }
    [current_osk addSubview:textfield];
    [textfield release];
    textfield.delegate = delegate;
    if (text_copy) {
        textfield.text = text_copy;
    }
    textfield.borderStyle = UITextBorderStyleRoundedRect;
    textfield.keyboardType = UIKeyboardTypeDefault;

    [[global_view window] bringSubviewToFront:global_vc.view];
    [textfield becomeFirstResponder];
    osk_is_running = 1;
    BARRIER();

  out:
    osk_is_coming_up = 0;
}

/*-----------------------------------------------------------------------*/

static void destroy_osk_view(void *userdata)
{
    UIView *view = (UIView *)userdata;
    [view removeFromSuperview];
    [[global_view window] sendSubviewToBack:global_vc.view];
}

/*-----------------------------------------------------------------------*/

@implementation OSKTextFieldDelegate

/**
 * -[textFieldShouldReturn]:  Callback method called when the Return key
 * is pressed.  We use Return to end text entry.
 */
- (BOOL)textFieldShouldReturn:(UITextField *)textfield
{
    [textfield resignFirstResponder];
    return NO;
}

/**
 * -[textFieldDidEndEditing]:  Callback method called after the text field
 * has become inactive.
 */
- (void)textFieldDidEndEditing:(UITextField *)textfield
{
    mem_free(osk_text);
    osk_text = NULL;

    const char *utf8 = [[textfield text] UTF8String];
    if (UNLIKELY(!utf8)) {
        DLOG("Failed to get UTF-8 string");
        goto out;
    }

    unsigned int text_size = 100;
    unsigned int text_length = 0;
    osk_text = mem_alloc(sizeof(*osk_text) * text_size, sizeof(int), 0);
    if (UNLIKELY(!osk_text)) {
        DLOG("No memory for text");
        goto out;
    }
    int ch;
    while ((ch = utf8_read(&utf8)) != 0) {
        if (ch == -1) {
            continue;
        }
        if (text_length+1 >= text_size) {  // Leave room for the trailing null.
            text_size += 100;
            int *new_text =
                mem_realloc(osk_text, sizeof(*osk_text) * text_size, 0);
            if (UNLIKELY(!new_text)) {
                DLOG("No memory for text");
                mem_free(osk_text);
                osk_text = NULL;
                goto out;
            }
            osk_text = new_text;
        }
        osk_text[text_length++] = ch;
    }
    osk_text[text_length] = 0;

  out:
    osk_is_running = 0;
}

@end

/*************************************************************************/
/*************************************************************************/
