/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/windows/thread.c: Windows-specific thread tests.
 */

#include "src/base.h"
#include "src/sysdep/windows/internal.h"  // For <windows.h>.
#include "src/test/base.h"
#include "src/thread.h"

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

/**
 * get_priority_thread:  Simple thread routine that returns the thread's
 * priority.
 *
 * [Return value]
 *     Thread priority.
 */
static int get_priority_thread(UNUSED void *unused)
{
    return GetThreadPriority(GetCurrentThread());
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_windows_thread)

TEST_INIT(init)
{
    CHECK_TRUE(thread_init());
    return 1;
}

TEST_CLEANUP(cleanup)
{
    thread_cleanup();
    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_thread_priority_clamped_low)
{
    int thread;
    CHECK_TRUE(thread = thread_create_with_priority(
                   THREAD_PRIORITY_LOWEST - 1, get_priority_thread, NULL));
    CHECK_INTEQUAL(thread_wait(thread), THREAD_PRIORITY_LOWEST);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_thread_priority_clamped_high)
{
    int thread;
    CHECK_TRUE(thread = thread_create_with_priority(
                   THREAD_PRIORITY_HIGHEST + 1, get_priority_thread, NULL));
    CHECK_INTEQUAL(thread_wait(thread), THREAD_PRIORITY_HIGHEST);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_thread_priority_idle)
{
    int thread;
    CHECK_TRUE(thread = thread_create_with_priority(
                   THREAD_PRIORITY_IDLE, get_priority_thread, NULL));
    /* Should not be clamped even though it's less than LOWEST. */
    CHECK_INTEQUAL(thread_wait(thread), THREAD_PRIORITY_IDLE);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_thread_priority_time_critical)
{
    int thread;
    CHECK_TRUE(thread = thread_create_with_priority(
                   THREAD_PRIORITY_TIME_CRITICAL, get_priority_thread, NULL));
    /* Should not be clamped even though it's greater than HIGHEST. */
    CHECK_INTEQUAL(thread_wait(thread), THREAD_PRIORITY_TIME_CRITICAL);
    return 1;
}

/*************************************************************************/
/*************************************************************************/
