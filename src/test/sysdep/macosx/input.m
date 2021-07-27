/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/macosx/input.m: Tests for Mac OS X input handling.
 */

#import "src/base.h"
#import "src/graphics.h"
#import "src/input.h"
#define IN_SYSDEP  // So we get the real functions instead of the diversions.
#import "src/sysdep.h"
#import "src/sysdep/darwin/time.h"
#import "src/sysdep/macosx/graphics.h"
#import "src/sysdep/test.h"
#import "src/test/base.h"
#import "src/test/graphics/internal.h"
#import "src/time.h"

#import "src/sysdep/macosx/osx-headers.h"
#import <AppKit/NSApplication.h>
#import <AppKit/NSEvent.h>
#import <AppKit/NSWindow.h>
#import <Carbon/Carbon.h>  // For kVK_*.
#import <time.h>  // For nanosleep().

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Buffer of events received from the sys_input module. */
static InputEvent events[10];
static int num_events;

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

/**
 * event_callback:  Callback which receives input events from the sys_input
 * module.
 */
static void event_callback(const InputEvent *event)
{
    ASSERT(num_events < lenof(events));
    events[num_events++] = *event;
}

/*-----------------------------------------------------------------------*/

/**
 * send_key_event:  Synthesize a key event.
 *
 * [Parameters]
 *     type: One of the constants NSKeyDown, NSKeyUp, or NSFlagsChanged.
 *     key: Keycode (kVK_*).
 *     modifiers: Modifier flags (NSAlphaShiftKeyMask, etc).
 *     text: Corresponding text.
 *     timestamp: SIL timestamp for event, in seconds.
 */
static void send_key_event(NSEventType type, int key, int modifiers,
                           const char *text, double timestamp)
{
    NSWindow *window = macosx_window();
    NSString *str = [NSString stringWithUTF8String:text];
    NSEvent *event = [NSEvent keyEventWithType:type location:(NSPoint){0,0}
                              modifierFlags:modifiers
                              timestamp:timestamp + darwin_time_epoch()
                              windowNumber:[window windowNumber] context:NULL
                              characters:str charactersIgnoringModifiers:str
                              isARepeat:NO keyCode:key];
    [[NSApplication sharedApplication]
        performSelectorOnMainThread:@selector(sendEvent:) withObject:event
        waitUntilDone:YES];
}

/*-----------------------------------------------------------------------*/

/**
 * send_mouse_event:  Synthesize a mouse movement (absolute) or button event.
 *
 * [Parameters]
 *     type: A mouse event type (NSLeftMouseDown, NSMouseMoved, etc).
 *     x, y: Mouse coordinates in the window's coordinate system.  Note
 *         that the Y coordinate origin is at the bottom (not the top) of
 *         the window.
 *     timestamp: SIL timestamp for event, in seconds.
 */
static void send_mouse_event(NSEventType type, int x, int y, double timestamp)
{
    int is_down = (type == NSLeftMouseDown ||
                   type == NSRightMouseDown ||
                   type == NSOtherMouseDown);
    NSWindow *window = macosx_window();
    NSEvent *event = [NSEvent mouseEventWithType:type location:(NSPoint){x,y}
                              modifierFlags:0
                              timestamp:timestamp + darwin_time_epoch()
                              windowNumber:[window windowNumber] context:NULL
                              eventNumber:0 clickCount:is_down
                              pressure:is_down];
    [[NSApplication sharedApplication]
        performSelectorOnMainThread:@selector(sendEvent:) withObject:event
        waitUntilDone:YES];
}

/*-----------------------------------------------------------------------*/

/**
 * send_mouse_rel_event:  Synthesize a relative mouse movement event.  The
 * absolute coordinates for the event will be set to (0,0).
 *
 * [Parameters]
 *     dx: Horizontal movement delta (positive = right).
 *     dy: Vertical movement delta (positive = up).
 *     timestamp: SIL timestamp for event, in seconds.
 */
static void send_mouse_rel_event(int dx, int dy, double timestamp)
{
    CGEventRef carbon_event = CGEventCreateMouseEvent(
        NULL, kCGEventMouseMoved, (CGPoint){0,0}, 0);
    CGEventSetIntegerValueField(carbon_event, kCGMouseEventDeltaX, dx);
    CGEventSetIntegerValueField(carbon_event, kCGMouseEventDeltaY, dy);
    CGEventSetTimestamp(
        carbon_event, (uint64_t)round((timestamp + darwin_time_epoch()) * 1e9));
    NSEvent *event = [NSEvent eventWithCGEvent:carbon_event];
    CFRelease(carbon_event);
    /* Using the application's -[sendEvent:] method doesn't work because we
     * have no way to set the window number in the NSEvent, so we cheat and
     * send the event directly to the window's mouse event handler. */
    NSWindow *window = macosx_window();
    [window performSelectorOnMainThread:@selector(mouseMoved:)
            withObject:event waitUntilDone:YES];
}

/*-----------------------------------------------------------------------*/

/**
 * send_scroll_event:  Synthesize a mouse scroll event.
 *
 * [Parameters]
 *     unit: Units for scroll deltas (kCGScrollEventUnit*).
 *     horiz_scroll: Horizontal scroll delta (positive = right).
 *     vert_scroll: Vertical scroll delta (positive = up).
 *     timestamp: SIL timestamp for event, in seconds.
 */
static void send_scroll_event(CGScrollEventUnit unit, int horiz_scroll,
                              int vert_scroll, double timestamp)
{
    CGEventRef carbon_event = CGEventCreateScrollWheelEvent(
        NULL, unit, 2, vert_scroll, horiz_scroll);
    CGEventSetTimestamp(
        carbon_event, (uint64_t)round((timestamp + darwin_time_epoch()) * 1e9));
    NSEvent *event = [NSEvent eventWithCGEvent:carbon_event];
    CFRelease(carbon_event);
    /* As above, we have to send the event directly to the window's scroll
     * event handler. */
    NSWindow *window = macosx_window();
    [window performSelectorOnMainThread:@selector(scrollWheel:)
            withObject:event waitUntilDone:YES];
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

static int do_test_macosx_input(void);
int test_macosx_input(void)
{
    /* Get the Darwin timekeeping state set up so we can handle event
     * timestamps correctly. */
    sys_time_init();
    (void) sys_time_now();

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    const int result = run_tests_in_window(do_test_macosx_input);
    [pool release];
    return result;
}

DEFINE_GENERIC_TEST_RUNNER(do_test_macosx_input)

TEST_INIT(init)
{
    time_init();
    CHECK_TRUE(sys_input_init(event_callback));
    num_events = 0;
    return 1;
}

TEST_CLEANUP(cleanup)
{
    sys_input_cleanup();
    return 1;
}

/*************************************************************************/
/*************************** Tests: Basic tests **************************/
/*************************************************************************/

TEST(test_init_memory_failure)
{
    sys_input_cleanup();
    CHECK_MEMORY_FAILURES(sys_input_init(event_callback));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_info)
{
    SysInputInfo info;
    sys_input_info(&info);
    /* Ignore joystick stuff since we don't know the physical system state. */
    CHECK_TRUE(info.has_keyboard);
    CHECK_TRUE(info.keyboard_is_full);
    CHECK_TRUE(info.has_mouse);
    CHECK_TRUE(info.has_text);
    CHECK_FALSE(info.text_uses_custom_interface);
    CHECK_FALSE(info.text_has_prompt);
    CHECK_FALSE(info.has_touch);

    return 1;
}

/*************************************************************************/
/************************* Tests: Keyboard input *************************/
/*************************************************************************/

TEST(test_key_down_up)
{
    num_events = 0;
    send_key_event(NSKeyDown, kVK_ANSI_1, 0, "1", 1.0);
    send_key_event(NSKeyUp, kVK_ANSI_1, 0, "1", 2.0);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_1);
    CHECK_INTEQUAL(events[0].keyboard.system_key, kVK_ANSI_1);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[1].detail, INPUT_KEYBOARD_KEY_UP);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 2.0);
    CHECK_INTEQUAL(events[1].keyboard.key, KEY_1);
    CHECK_INTEQUAL(events[1].keyboard.system_key, kVK_ANSI_1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_key_down_up_unsupported)
{
    num_events = 0;
    send_key_event(NSKeyDown, kVK_Function, 0, "", 1.0);
    send_key_event(NSKeyUp, kVK_Function, 0, "", 2.0);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_SYSTEM_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].keyboard.key, 0);
    CHECK_INTEQUAL(events[0].keyboard.system_key, kVK_Function);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[1].detail, INPUT_KEYBOARD_SYSTEM_KEY_UP);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 2.0);
    CHECK_INTEQUAL(events[1].keyboard.key, 0);
    CHECK_INTEQUAL(events[1].keyboard.system_key, kVK_Function);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_key_up_without_down)
{
    num_events = 0;
    /* This should be ignored because of no preceding key-down event. */
    send_key_event(NSKeyUp, kVK_ANSI_1, 0, "1", 1.0);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_key_modifiers)
{
    static const struct {int sil_key, osx_key, modifiers;} modifier_keys[] = {
        {KEY_LEFTSHIFT,    kVK_Shift,               0x0002},
        {KEY_RIGHTSHIFT,   kVK_RightShift,          0x0004},
        {KEY_LEFTCONTROL,  kVK_Control,             0x0001},
        {KEY_RIGHTCONTROL, kVK_RightControl,        0x2000},
        {KEY_LEFTALT,      kVK_Option,              0x0020},
        {KEY_RIGHTALT,     kVK_RightOption,         0x0040},
        {KEY_LEFTMETA,     kVK_Command,             0x0008},
        {KEY_RIGHTMETA,    54 /*kVK_RightCommand*/, 0x0010},
        {KEY_CAPSLOCK,     kVK_CapsLock,            NSAlphaShiftKeyMask},
    };

    for (int i = 0; i < lenof(modifier_keys); i++) {
        DLOG("Testing index %d (SIL key %d)", i, modifier_keys[i].sil_key);
        num_events = 0;
        send_key_event(NSFlagsChanged, modifier_keys[i].osx_key,
                       modifier_keys[i].modifiers, "", i*2+1);
        send_key_event(NSFlagsChanged, modifier_keys[i].osx_key, 0, "", i*2+2);
        sys_input_update();
        CHECK_INTEQUAL(num_events, 2);
        CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
        CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_DOWN);
        CHECK_DOUBLEEQUAL(events[0].timestamp, i*2+1);
        CHECK_INTEQUAL(events[0].keyboard.key, modifier_keys[i].sil_key);
        CHECK_INTEQUAL(events[0].keyboard.system_key, modifier_keys[i].osx_key);
        CHECK_INTEQUAL(events[1].type, INPUT_EVENT_KEYBOARD);
        CHECK_INTEQUAL(events[1].detail, INPUT_KEYBOARD_KEY_UP);
        CHECK_DOUBLEEQUAL(events[1].timestamp, i*2+2);
        CHECK_INTEQUAL(events[1].keyboard.key, modifier_keys[i].sil_key);
        CHECK_INTEQUAL(events[1].keyboard.system_key, modifier_keys[i].osx_key);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_key_unsupported_modifier)
{
    num_events = 0;
    send_key_event(NSFlagsChanged, kVK_Function, 0, "", 1.0);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_key_up_on_window_change)
{
    num_events = 0;
    send_key_event(NSKeyDown, kVK_ANSI_1, 0, "1", 1.0);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_1);
    CHECK_INTEQUAL(events[0].keyboard.system_key, kVK_ANSI_1);

    /* Forced-release is applied even if the window size doesn't actually
     * change. */
    num_events = 0;
    sys_test_time_set_seconds(2.0);
    ASSERT(graphics_set_display_mode(TESTW, TESTH, NULL));
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_UP);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_1);
    CHECK_INTEQUAL(events[0].keyboard.system_key, kVK_ANSI_1);

    /* Receiving the actual key-up shouldn't trigger another event. */
    num_events = 0;
    send_key_event(NSKeyUp, kVK_ANSI_1, 0, "1", 3.0);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);

    return 1;
}

/*************************************************************************/
/*************************** Tests: Mouse input **************************/
/*************************************************************************/

TEST(test_mouse_position)
{
    num_events = 0;
    send_mouse_event(NSMouseMoved, TESTW/8, TESTH/4-1, 1.0);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.125);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.75);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mouse_position_out_of_range)
{
    num_events = 0;
    send_mouse_event(NSMouseMoved, TESTW*5/4, -TESTH/2, 1.0);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, (float)(TESTW-1) / (float)TESTW);
    CHECK_FLOATEQUAL(events[0].mouse.y, (float)(TESTH-1) / (float)TESTH);

    num_events = 0;
    send_mouse_event(NSMouseMoved, -TESTW/4, TESTH*3/2, 2.0);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mouse_buttons)
{
    num_events = 0;
    send_mouse_event(NSMouseMoved, TESTW*5/4, -TESTH/2, 1.0);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, (float)(TESTW-1) / (float)TESTW);
    CHECK_FLOATEQUAL(events[0].mouse.y, (float)(TESTH-1) / (float)TESTH);

    static const struct {NSEventType type; int event;} event_map[] = {
        {NSLeftMouseDown,  INPUT_MOUSE_LMB_DOWN},
        {NSLeftMouseUp,    INPUT_MOUSE_LMB_UP},
        {NSOtherMouseDown, INPUT_MOUSE_MMB_DOWN},
        {NSOtherMouseUp,   INPUT_MOUSE_MMB_UP},
        {NSRightMouseDown, INPUT_MOUSE_RMB_DOWN},
        {NSRightMouseUp,   INPUT_MOUSE_RMB_UP},
    };
    double time = 1.0;

    for (int i = 0; i < lenof(event_map); i++, time += 1.0) {
        DLOG("Testing index %d (SIL event %d)", i, event_map[i].event);
        num_events = 0;
        send_mouse_event(event_map[i].type, i, (TESTH-1)-(i+1)*3, time);
        sys_input_update();
        CHECK_INTEQUAL(num_events, 1);
        CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
        CHECK_INTEQUAL(events[0].detail, event_map[i].event);
        CHECK_DOUBLEEQUAL(events[0].timestamp, time);
        CHECK_FLOATEQUAL(events[0].mouse.x, (float)i/TESTW);
        CHECK_FLOATEQUAL(events[0].mouse.y, (float)(i+1)*3/TESTH);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mouse_click_in_title_bar)
{
    num_events = 0;
    send_mouse_event(NSLeftMouseDown, TESTW/2, TESTH-1, 1.0);
    send_mouse_event(NSLeftMouseUp, TESTW/2, TESTH-1, 2.0);
    /* This click should be ignored because it's outside the window's
     * client area. */
    send_mouse_event(NSLeftMouseDown, TESTW/2, TESTH, 3.0);
    send_mouse_event(NSLeftMouseUp, TESTW/2, TESTH, 4.0);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_LMB_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.5f);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.0f);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[1].detail, INPUT_MOUSE_LMB_UP);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 2.0);
    CHECK_FLOATEQUAL(events[1].mouse.x, 0.5f);
    CHECK_FLOATEQUAL(events[1].mouse.y, 0.0f);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mouse_release_without_press)
{
    num_events = 0;
    /* These should be ignored because of no preceding button-down events. */
    send_mouse_event(NSLeftMouseUp, TESTW/2, TESTH/2-1, 1.0);
    send_mouse_event(NSOtherMouseUp, TESTW/2, TESTH/2-1, 1.0);
    send_mouse_event(NSRightMouseUp, TESTW/2, TESTH/2-1, 1.0);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mouse_position_drag)
{
    static const struct {NSEventType down, drag, up;} types[] = {
        {NSLeftMouseDown,  NSLeftMouseDragged,  NSLeftMouseUp},
        {NSOtherMouseDown, NSOtherMouseDragged, NSOtherMouseUp},
        {NSRightMouseDown, NSRightMouseDragged, NSRightMouseUp},
    };

    for (int i = 0; i < lenof(types); i++) {
        DLOG("Testing index %d (NSEventType %d)", i, (int)types[i].drag);
        num_events = 0;
        send_mouse_event(NSMouseMoved, TESTW/4, TESTH/2-1, i*10+1);
        send_mouse_event(types[i].down, TESTW/4, TESTH/2-1, i*10+2);
        send_mouse_event(types[i].drag, TESTW/2, TESTH/4-1, i*10+3);
        send_mouse_event(types[i].up, TESTW/2, TESTH/4-1, i*10+4);
        sys_input_update();
        CHECK_INTEQUAL(num_events, 4);
        CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
        CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
        CHECK_DOUBLEEQUAL(events[0].timestamp, i*10+1);
        CHECK_FLOATEQUAL(events[0].mouse.x, 0.25);
        CHECK_FLOATEQUAL(events[0].mouse.y, 0.5);
        CHECK_INTEQUAL(events[1].type, INPUT_EVENT_MOUSE);
        CHECK_DOUBLEEQUAL(events[1].timestamp, i*10+2);
        CHECK_FLOATEQUAL(events[1].mouse.x, 0.25);
        CHECK_FLOATEQUAL(events[1].mouse.y, 0.5);
        CHECK_INTEQUAL(events[2].type, INPUT_EVENT_MOUSE);
        CHECK_INTEQUAL(events[2].detail, INPUT_MOUSE_MOVE);
        CHECK_DOUBLEEQUAL(events[2].timestamp, i*10+3);
        CHECK_FLOATEQUAL(events[2].mouse.x, 0.5);
        CHECK_FLOATEQUAL(events[2].mouse.y, 0.75);
        CHECK_INTEQUAL(events[3].type, INPUT_EVENT_MOUSE);
        CHECK_DOUBLEEQUAL(events[3].timestamp, i*10+4);
        CHECK_FLOATEQUAL(events[3].mouse.x, 0.5);
        CHECK_FLOATEQUAL(events[3].mouse.y, 0.75);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mouse_scroll)
{
    /* Prep the saved mouse coordinates, since scroll events don't have
     * any of their own. */
    send_mouse_event(NSMouseMoved, TESTW/4, TESTH/2-1, 0.0);

    num_events = 0;
    send_scroll_event(kCGScrollEventUnitLine, 1, 0, 11.0);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_SCROLL_H);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 11.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.25);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.5);
    CHECK_FLOATEQUAL(events[0].mouse.scroll, 1);

    num_events = 0;
    send_scroll_event(kCGScrollEventUnitLine, -2, 0, 22.0);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_SCROLL_H);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 22.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.25);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.5);
    CHECK_FLOATEQUAL(events[0].mouse.scroll, -2);

    num_events = 0;
    send_scroll_event(kCGScrollEventUnitLine, 0, 3, 33.0);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_SCROLL_V);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 33.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.25);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.5);
    CHECK_FLOATEQUAL(events[0].mouse.scroll, 3);

    num_events = 0;
    send_scroll_event(kCGScrollEventUnitLine, 0, -4, 44.0);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_SCROLL_V);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 44.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.25);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.5);
    CHECK_FLOATEQUAL(events[0].mouse.scroll, -4);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mouse_scroll_subunit)
{
    send_mouse_event(NSMouseMoved, TESTW/4, TESTH/2-1, 0.0);

    num_events = 0;
    send_scroll_event(kCGScrollEventUnitPixel, 1, 0, 11.0);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_SCROLL_H);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 11.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.25);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.5);
    /* Technically this should be "between 0 and 1, exclusive", but we
     * don't currently have an exclusive CHECK_FLOATRANGE.  This should be
     * good enough. */
    CHECK_FLOATRANGE(events[0].mouse.scroll, 0.001, 0.999);

    num_events = 0;
    send_scroll_event(kCGScrollEventUnitPixel, -1, 0, 22.0);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_SCROLL_H);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 22.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.25);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.5);
    CHECK_FLOATRANGE(events[0].mouse.scroll, -0.999, -0.001);

    num_events = 0;
    send_scroll_event(kCGScrollEventUnitPixel, 0, 1, 33.0);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_SCROLL_V);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 33.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.25);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.5);
    CHECK_FLOATRANGE(events[0].mouse.scroll, 0.001, 0.999);

    num_events = 0;
    send_scroll_event(kCGScrollEventUnitPixel, 0, -1, 44.0);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_SCROLL_V);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 44.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.25);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.5);
    CHECK_FLOATRANGE(events[0].mouse.scroll, -0.999, -0.001);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mouse_set_position)
{
    /* This will set the real mouse pointer position, so save and restore
     * it to avoid interfering with whatever else the user may be doing. */
    int saved_x, saved_y;
    get_mouse_position(&saved_x, &saved_y);

    num_events = 0;
    sys_input_mouse_set_position(0.5, 0.75);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.5);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.75);

    /* Setting out-of-bounds coordinates should clamp the coordinates to
     * the window bounds. */
    num_events = 0;
    sys_input_mouse_set_position(-1, -1);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0);

    num_events = 0;
    sys_input_mouse_set_position(2, 2);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
    CHECK_FLOATEQUAL(events[0].mouse.x, (float)(TESTW-1)/TESTW);
    CHECK_FLOATEQUAL(events[0].mouse.y, (float)(TESTH-1)/TESTH);

    /* Set operations should do nothing (but not crash) if no window is
     * open. */
    sys_input_cleanup();
    graphics_cleanup();
    ASSERT(graphics_init());
    ASSERT(sys_input_init(event_callback));
    CHECK_FALSE(macosx_window());
    num_events = 0;
    sys_input_mouse_set_position(0.25, 0.5);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);

    ASSERT(graphics_set_display_mode(TESTW, TESTH, NULL));
    set_mouse_position(saved_x, saved_y);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* For this test, we want to be sure to clean up on return even if the
 * test fails, so that we don't leave input in a grabbed state. */
#undef FAIL_ACTION
#define FAIL_ACTION  result = 0; goto out

TEST(test_mouse_grab)
{
    int result = 1;

    int saved_x, saved_y;
    get_mouse_position(&saved_x, &saved_y);

    /* When grabbed, the mouse pointer should be warped to the center of
     * the window.  In order to figure out where that is, set the mouse
     * position via SIL and read it out via CoreGraphics. */
    int center_x, center_y;
    sys_input_mouse_set_position(0.5, 0.5);
    get_mouse_position(&center_x, &center_y);
    /* Move the pointer away from the window center so we can verify that
     * it moves back. */
    int x, y;
    sys_input_mouse_set_position(0.125, 0.25);
    get_mouse_position(&x, &y);
    CHECK_INTEQUAL(x, center_x - TESTW*3/8);
    CHECK_INTEQUAL(y, center_y - TESTH/4);

    /* Enable grabbing.  This should immediately warp the pointer to the
     * center of the window. */
    num_events = 0;
    sys_input_grab(1);
    get_mouse_position(&x, &y);
    CHECK_INTEQUAL(x, center_x);
    CHECK_INTEQUAL(y, center_y);
    /* But we should not see any mouse movement events. */
    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);

    /* While grabbed, mouse movement is converted into relative movement
     * events, and the system mouse pointer stays in the center of the
     * window (which we can't check because we can't inject events into the
     * system early enough). */
    send_mouse_rel_event(TESTW/2, TESTH/2, 1.0);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 1);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    if (events[0].mouse.x == 0.125f && events[0].mouse.y == 0.25f) {
        /* This was the first mouse event seen after grabbing, so it got
         * ignored.  Try again. */
        num_events = 0;
        send_mouse_rel_event(TESTW/2, TESTH/2, 1.5);
        sys_input_update();
        CHECK_INTEQUAL(num_events, 1);
        CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
        CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
        CHECK_DOUBLEEQUAL(events[0].timestamp, 1.5);
    }
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.625);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.75);

    /* The virtual mouse pointer should be bounded to the window size. */
    num_events = 0;
    send_mouse_rel_event(TESTW*2, TESTH*2, 3.0);
    send_mouse_rel_event(TESTW*-2, TESTH*-2, 4.0);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 3.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, (float)(TESTW-1)/(float)TESTW);
    CHECK_FLOATEQUAL(events[0].mouse.y, (float)(TESTH-1)/(float)TESTH);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[1].detail, INPUT_MOUSE_MOVE);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 4.0);
    CHECK_FLOATEQUAL(events[1].mouse.x, 0);
    CHECK_FLOATEQUAL(events[1].mouse.y, 0);

    /* Releasing the grab should reposition the system mouse pointer to
     * the current virtual pointer location within the window. */
    sys_input_grab(0);
    get_mouse_position(&x, &y);
    CHECK_INTEQUAL(x, center_x - TESTW/2);
    CHECK_INTEQUAL(y, center_y - TESTH/2);

    /* Mouse events should once again use and update the system pointer
     * position. */
    num_events = 0;
    send_mouse_event(NSMouseMoved, TESTW/4, TESTH/8-1, 5.0);
    send_mouse_event(NSLeftMouseDown, TESTW/8, TESTH/4-1, 6.0);
    send_mouse_event(NSLeftMouseUp, TESTW/8, TESTH/4-1, 7.0);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 3);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_MOVE);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 5.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.25);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.875);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[1].detail, INPUT_MOUSE_LMB_DOWN);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 6.0);
    CHECK_FLOATEQUAL(events[1].mouse.x, 0.125);
    CHECK_FLOATEQUAL(events[1].mouse.y, 0.75);
    CHECK_INTEQUAL(events[2].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[2].detail, INPUT_MOUSE_LMB_UP);
    CHECK_DOUBLEEQUAL(events[2].timestamp, 7.0);
    CHECK_FLOATEQUAL(events[2].mouse.x, 0.125);
    CHECK_FLOATEQUAL(events[2].mouse.y, 0.75);

    sys_input_mouse_set_position(0.75, 0.25);
    get_mouse_position(&x, &y);
    CHECK_INTEQUAL(x, center_x + TESTW/4);
    CHECK_INTEQUAL(y, center_y - TESTH/4);

  out:
    sys_input_grab(0);
    set_mouse_position(saved_x, saved_y);
    return result;
}

#undef FAIL_ACTION
#define FAIL_ACTION  return 0

/*-----------------------------------------------------------------------*/

TEST(test_mouse_up_on_window_change)
{
    num_events = 0;
    send_mouse_event(NSLeftMouseDown, TESTW/4, TESTH/2-1, 1.0);
    send_mouse_event(NSOtherMouseDown, TESTW/4, TESTH/2-1, 1.0);
    send_mouse_event(NSRightMouseDown, TESTW/4, TESTH/2-1, 1.0);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 3);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_LMB_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.25f);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.5f);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[1].detail, INPUT_MOUSE_MMB_DOWN);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[1].mouse.x, 0.25f);
    CHECK_FLOATEQUAL(events[1].mouse.y, 0.5f);
    CHECK_INTEQUAL(events[2].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[2].detail, INPUT_MOUSE_RMB_DOWN);
    CHECK_DOUBLEEQUAL(events[2].timestamp, 1.0);
    CHECK_FLOATEQUAL(events[2].mouse.x, 0.25f);
    CHECK_FLOATEQUAL(events[2].mouse.y, 0.5f);

    /* Forced-release is applied even if the window size doesn't actually
     * change. */
    num_events = 0;
    sys_test_time_set_seconds(2.0);
    ASSERT(graphics_set_display_mode(TESTW, TESTH, NULL));
    CHECK_INTEQUAL(num_events, 3);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[0].detail, INPUT_MOUSE_LMB_UP);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 2.0);
    CHECK_FLOATEQUAL(events[0].mouse.x, 0.25f);
    CHECK_FLOATEQUAL(events[0].mouse.y, 0.5f);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[1].detail, INPUT_MOUSE_MMB_UP);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 2.0);
    CHECK_FLOATEQUAL(events[1].mouse.x, 0.25f);
    CHECK_FLOATEQUAL(events[1].mouse.y, 0.5f);
    CHECK_INTEQUAL(events[2].type, INPUT_EVENT_MOUSE);
    CHECK_INTEQUAL(events[2].detail, INPUT_MOUSE_RMB_UP);
    CHECK_DOUBLEEQUAL(events[2].timestamp, 2.0);
    CHECK_FLOATEQUAL(events[2].mouse.x, 0.25f);
    CHECK_FLOATEQUAL(events[2].mouse.y, 0.5f);

    /* Receiving the actual button-ups shouldn't trigger more events. */
    num_events = 0;
    send_mouse_event(NSLeftMouseUp, TESTW/2, TESTH/2-1, 3.0);
    send_mouse_event(NSOtherMouseUp, TESTW/2, TESTH/2-1, 3.0);
    send_mouse_event(NSRightMouseUp, TESTW/2, TESTH/2-1, 3.0);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);

    return 1;
}

/*************************************************************************/
/*************************** Tests: Text input ***************************/
/*************************************************************************/

TEST(test_text_input_char)
{
    sys_input_text_set_state(1, NULL, NULL);
    num_events = 0;
    send_key_event(NSKeyDown, kVK_ANSI_1, 0, "1", 1.0);
    send_key_event(NSKeyUp, kVK_ANSI_1, 0, "1", 2.0);
    CHECK_INTEQUAL(num_events, 3);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 1.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_1);
    CHECK_INTEQUAL(events[0].keyboard.system_key, kVK_ANSI_1);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_TEXT);
    CHECK_INTEQUAL(events[1].detail, INPUT_TEXT_INPUT);
    CHECK_INTEQUAL(events[1].timestamp, 1.0);
    CHECK_INTEQUAL(events[1].text.ch, '1');
    CHECK_INTEQUAL(events[2].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[2].detail, INPUT_KEYBOARD_KEY_UP);
    CHECK_DOUBLEEQUAL(events[2].timestamp, 2.0);
    CHECK_INTEQUAL(events[2].keyboard.key, KEY_1);
    CHECK_INTEQUAL(events[2].keyboard.system_key, kVK_ANSI_1);

    /* Setting to "on" again should not change the state. */
    sys_input_text_set_state(1, NULL, NULL);
    num_events = 0;
    send_key_event(NSKeyDown, kVK_ANSI_2, 0, "2", 3.0);
    send_key_event(NSKeyUp, kVK_ANSI_2, 0, "2", 4.0);
    CHECK_INTEQUAL(num_events, 3);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 3.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_2);
    CHECK_INTEQUAL(events[0].keyboard.system_key, kVK_ANSI_2);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_TEXT);
    CHECK_INTEQUAL(events[1].detail, INPUT_TEXT_INPUT);
    CHECK_INTEQUAL(events[1].timestamp, 3.0);
    CHECK_INTEQUAL(events[1].text.ch, '2');
    CHECK_INTEQUAL(events[2].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[2].detail, INPUT_KEYBOARD_KEY_UP);
    CHECK_DOUBLEEQUAL(events[2].timestamp, 4.0);
    CHECK_INTEQUAL(events[2].keyboard.key, KEY_2);
    CHECK_INTEQUAL(events[2].keyboard.system_key, kVK_ANSI_2);

    sys_input_text_set_state(0, NULL, NULL);
    num_events = 0;
    send_key_event(NSKeyDown, kVK_ANSI_3, 0, "3", 5.0);
    send_key_event(NSKeyUp, kVK_ANSI_3, 0, "3", 6.0);
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 5.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_3);
    CHECK_INTEQUAL(events[0].keyboard.system_key, kVK_ANSI_3);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[1].detail, INPUT_KEYBOARD_KEY_UP);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 6.0);
    CHECK_INTEQUAL(events[1].keyboard.key, KEY_3);
    CHECK_INTEQUAL(events[1].keyboard.system_key, kVK_ANSI_3);

    sys_input_text_set_state(0, NULL, NULL);
    num_events = 0;
    send_key_event(NSKeyDown, kVK_ANSI_4, 0, "4", 7.0);
    send_key_event(NSKeyUp, kVK_ANSI_4, 0, "4", 8.0);
    CHECK_INTEQUAL(num_events, 2);
    CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_DOWN);
    CHECK_DOUBLEEQUAL(events[0].timestamp, 7.0);
    CHECK_INTEQUAL(events[0].keyboard.key, KEY_4);
    CHECK_INTEQUAL(events[0].keyboard.system_key, kVK_ANSI_4);
    CHECK_INTEQUAL(events[1].type, INPUT_EVENT_KEYBOARD);
    CHECK_INTEQUAL(events[1].detail, INPUT_KEYBOARD_KEY_UP);
    CHECK_DOUBLEEQUAL(events[1].timestamp, 8.0);
    CHECK_INTEQUAL(events[1].keyboard.key, KEY_4);
    CHECK_INTEQUAL(events[1].keyboard.system_key, kVK_ANSI_4);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_text_input_action)
{
    sys_input_text_set_state(1, NULL, NULL);

    const struct {
        int sil_key, osx_key;
        /* Text for characters and charactersIgnoringModifiers.  May be
         * U+F7xx (NSLeftArrowFunctionKey etc. from <AppKit/NSEvent.h>;
         * the character is apparently required for the text input system
         * to recognize special keys (kVK_* alone doesn't do it). */
        const char *text;
        InputEventDetail event;
    } event_map[] = {
        {KEY_BACKSPACE,    kVK_Delete,           "\x7F",
         INPUT_TEXT_BACKSPACE},
        {KEY_DELETE,       kVK_ForwardDelete,    "\xEF\x9C\xA8",
         INPUT_TEXT_DELETE},
        {KEY_LEFT,         kVK_LeftArrow,        "\xEF\x9C\x82",
         INPUT_TEXT_CURSOR_LEFT},
        {KEY_RIGHT,        kVK_RightArrow,       "\xEF\x9C\x83",
         INPUT_TEXT_CURSOR_RIGHT},
        {KEY_HOME,         kVK_Home,             "\xEF\x9C\xA9",
         INPUT_TEXT_CURSOR_HOME},
        {KEY_END,          kVK_End,              "\xEF\x9C\xAB",
         INPUT_TEXT_CURSOR_END},
        {KEY_ESCAPE,       kVK_Escape,           "\x1B",
         INPUT_TEXT_CANCELLED},
        {KEY_ENTER,        kVK_Return,           "\x0D",
         INPUT_TEXT_DONE},
        {KEY_NUMPAD_ENTER, kVK_ANSI_KeypadEnter, "\x0D",
         INPUT_TEXT_DONE},
        /* Also check one that calls an undefined selector just to make
         * sure nothing breaks. */
        {KEY_UP,           kVK_UpArrow,          "\xEF\x9C\x80",
         0},
    };
    double time = 1.0;

    for (int i = 0; i < lenof(event_map); i++, time += 1.0) {
        DLOG("Testing index %d (SIL key %d)", i, event_map[i].sil_key);
        num_events = 0;
        send_key_event(NSKeyDown, event_map[i].osx_key, 0, event_map[i].text,
                       time);
        send_key_event(NSKeyUp, event_map[i].osx_key, 0, event_map[i].text,
                       time + 0.5);
        const int count = event_map[i].event ? 3 : 2;
        CHECK_INTEQUAL(num_events, count);
        CHECK_INTEQUAL(events[0].type, INPUT_EVENT_KEYBOARD);
        CHECK_INTEQUAL(events[0].detail, INPUT_KEYBOARD_KEY_DOWN);
        CHECK_DOUBLEEQUAL(events[0].timestamp, time);
        CHECK_INTEQUAL(events[0].keyboard.key, event_map[i].sil_key);
        CHECK_INTEQUAL(events[0].keyboard.system_key, event_map[i].osx_key);
        if (count == 3) {
            CHECK_INTEQUAL(events[1].type, INPUT_EVENT_TEXT);
            CHECK_INTEQUAL(events[1].detail, event_map[i].event);
            CHECK_INTEQUAL(events[1].timestamp, time);
        }
        CHECK_INTEQUAL(events[count-1].type, INPUT_EVENT_KEYBOARD);
        CHECK_INTEQUAL(events[count-1].detail, INPUT_KEYBOARD_KEY_UP);
        CHECK_DOUBLEEQUAL(events[count-1].timestamp, time + 0.5);
        CHECK_INTEQUAL(events[count-1].keyboard.key, event_map[i].sil_key);
        CHECK_INTEQUAL(events[count-1].keyboard.system_key,
                       event_map[i].osx_key);
    }

    sys_input_text_set_state(0, NULL, NULL);
    return 1;
}

/*************************************************************************/
/************************** Tests: Miscellaneous *************************/
/*************************************************************************/

TEST(test_quit_by_signal)
{
    /* It seems that OSX can eat this signal in some circumstances, so
     * try several times. */
    num_events = 0;
    for (int try = 0; try < 10; try++) {
        raise(SIGINT);
        sys_input_update();
        if (sys_input_is_quit_requested()) {
            break;
        }
        nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = 10*1000*1000},
                  NULL);
    }
    CHECK_INTEQUAL(num_events, 0);
    CHECK_TRUE(sys_input_is_quit_requested());

    sys_input_cleanup();
    ASSERT(sys_input_init(event_callback));
    num_events = 0;
    raise(SIGTERM);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);
    CHECK_TRUE(sys_input_is_quit_requested());

    sys_input_cleanup();
    ASSERT(sys_input_init(event_callback));
    num_events = 0;
    raise(SIGHUP);
    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);
    CHECK_TRUE(sys_input_is_quit_requested());

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_quit_by_window)
{
    num_events = 0;
    NSWindow *window = macosx_window();
    [window performSelectorOnMainThread:@selector(performClose:)
            withObject:nil waitUntilDone:YES];
    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);
    CHECK_TRUE(sys_input_is_quit_requested());

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_quit_by_menu)
{
    num_events = 0;
    [[NSApplication sharedApplication] terminate:nil];
    sys_input_update();
    CHECK_INTEQUAL(num_events, 0);
    CHECK_TRUE(sys_input_is_quit_requested());

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_suspend)
{
    /* We don't support suspend/resume on OSX, so just check that the
     * associated functions behave properly. */
    CHECK_FALSE(sys_input_is_suspend_requested());
    sys_input_acknowledge_suspend_request();  // Should do nothing.

    return 1;
}

/*************************************************************************/
/*************************************************************************/
