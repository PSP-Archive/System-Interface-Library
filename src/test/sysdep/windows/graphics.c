/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/windows/graphics.c: Windows-specific graphics-related tests.
 */

#include "src/base.h"
#include "src/graphics.h"
#include "src/input.h"
#include "src/sysdep.h"
#include "src/sysdep/opengl/dyngl.h"
#include "src/sysdep/opengl/opengl.h"
#include "src/sysdep/windows/internal.h"
#include "src/test/base.h"
#include "src/test/graphics/internal.h"

/* From src/sysdep/windows/graphics.c: */
#define SIL_WM_APP_FLUSH  0x464C4953  // 'SILF'

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

static int do_test_windows_graphics(void);
int test_windows_graphics(void)
{
    return run_tests_in_window(do_test_windows_graphics);
}

DEFINE_GENERIC_TEST_RUNNER(do_test_windows_graphics)

TEST_INIT(init)
{
    CHECK_TRUE(input_init());
    graphics_start_frame();
    graphics_clear(0, 0, 0, 0, 1, 0);
    return 1;
}

TEST_CLEANUP(cleanup)
{
    graphics_finish_frame();
    graphics_flush_resources();
    input_cleanup();
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_windows_window)
{
    CHECK_TRUE(windows_window());
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_windows_flush_message_queue)
{
    DLOG("test");
    /* This message will cause a warning to be logged; we check the most
     * recent log message to determine whether the WM_APP message was
     * received by the window. */
    PostMessage(windows_window(), WM_APP, 0, 0);
    CHECK_STREQUAL(strrchr(test_DLOG_last_message,':'), ": test");

    windows_flush_message_queue();
    CHECK_STREQUAL(strrchr(test_DLOG_last_message,':'),
                   ": Unexpected WM_APP message with wParam 0x0");

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_single_threaded)
{
    /* By default, windows should be opened in multithreaded mode.  We can
     * verify this by manually sending a SIL_WM_APP_FLUSH message and
     * waiting for the event to be signaled, which cannot happen in
     * single-threaded mode because we have to explicitly process messages
     * in that case. */
    HANDLE event;
    ASSERT(event = CreateEvent(NULL, FALSE, FALSE, NULL));
    PostMessage(windows_window(), WM_APP, SIL_WM_APP_FLUSH, (LPARAM)event);
    WaitForSingleObject(event, INFINITE);
    CloseHandle(event);

    input_cleanup();
    graphics_cleanup();
    ASSERT(graphics_init());
    ASSERT(input_init());
    graphics_set_display_attr("window_thread", 0);
    ASSERT(open_window(TESTW, TESTH));

    /* In single-threaded mode, the window should not be able to respond
     * asynchronously to a flush message.  We assume that 1/4 second is
     * long enough for the message to be handled if the window is
     * (incorrectly) in multithreaded mode. */
    ASSERT(event = CreateEvent(NULL, FALSE, FALSE, NULL));
    PostMessage(windows_window(), WM_APP, SIL_WM_APP_FLUSH, (LPARAM)event);
    CHECK_TRUE(WaitForSingleObject(event, 250) == WAIT_TIMEOUT);
    /* Calling input_update() should process all pending window messages. */
    input_update();
    CHECK_TRUE(WaitForSingleObject(event, 250) == WAIT_OBJECT_0);
    CloseHandle(event);

    /* windows_flush_message_queue() should not block indefinitely in
     * single-threaded mode. */
    windows_flush_message_queue();

    /* Check that basic graphics operations still work correctly in
     * single-threaded mode. */
    uint8_t pixel[4];
    graphics_start_frame();
    graphics_clear_color(0.2, 0.4, 0.6, 1);
    mem_clear(pixel, sizeof(pixel));
    graphics_read_pixels(0, 0, 1, 1, pixel);
    CHECK_PIXEL(pixel, 51,102,153,255, 0, 0);
    graphics_finish_frame();
    input_update();
    graphics_start_frame();
    graphics_clear_color(0.8, 0.2, 0.4, 1);
    mem_clear(pixel, sizeof(pixel));
    graphics_read_pixels(0, 0, 1, 1, pixel);
    CHECK_PIXEL(pixel, 204,51,102,255, 0, 0);
    graphics_finish_frame();
    input_update();

    /* Check that the window can be resized. */
    CHECK_TRUE(graphics_set_display_mode(TESTW*2, TESTH*2, NULL));
    input_update();
    /* Give the window manager a bit of time to respond. */
    Sleep(100);
    graphics_start_frame();
    graphics_clear_color(0.6, 0.8, 0.2, 1);
    mem_clear(pixel, sizeof(pixel));
    graphics_read_pixels(TESTW*2-1, TESTH*2-1, 1, 1, pixel);
    CHECK_PIXEL(pixel, 153,204,51,255, 0, 0);
    graphics_finish_frame();
    input_update();

    return 1;
}

/*************************************************************************/
/*************************************************************************/
