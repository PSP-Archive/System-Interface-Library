/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/posix/time.c: Tests for the POSIX implementation of the
 * system-level timekeeping functions.
 */

#include "src/base.h"
#define IN_SYSDEP  // So we get the real functions instead of the diversions.
#include "src/sysdep.h"
#include "src/sysdep/posix/thread.h"
#include "src/sysdep/posix/time.h"
#include "src/test/base.h"
#include "src/thread.h"
#include "src/time.h"

#include <signal.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

/**
 * do_clock_test:  Perform clock tests for a specific clock type.
 * Implementation for test_clock_gettime() and test_gettimeofday().
 *
 * [Parameters]
 *     use_clock_gettime: True to test clock_gettime(), false to test
 *         gettimeofday().
 * [Return value]
 *     True if all tests succeeded, false otherwise.
 */
static int do_clock_test(int use_clock_gettime)
{
    TEST_sys_posix_disable_clock_gettime = !use_clock_gettime;

    sys_time_init();
    uint32_t time_unit;
    CHECK_TRUE(time_unit = sys_time_unit());
    const uint64_t epoch = sys_time_now();  // Behave like time_init() does.

    /* Check that the time unit is correct for the chosen timing method. */
    CHECK_INTEQUAL(sys_time_unit(), use_clock_gettime ? 1000000000 : 1000000);

    /* Ensure that sys_time_now() does not decrease between successive
     * calls, and eventually increases. */
    const uint64_t t1 = sys_time_now();
    const uint64_t t2 = sys_time_now();
    CHECK_TRUE(t2 >= t1);
    if (t2 == t1) {
        /* Assume resolution of at least 1us and execution time per call
         * of at least 1ns, so 1000 calls should be enough to guarantee
         * an increase of at least one tick. */
        int tries = 1000;
        uint64_t t3;
        do {
            t3 = sys_time_now();
        } while (t3 == t1 && --tries > 0);
        if (tries == 0) {
            FAIL("sys_time_now() return value did not increase after"
                 " 1000 tries");
        }
    }

    /* Ensure that sys_time_delay() waits for at least as long as
     * specified. */
    const uint64_t delay = time_unit/10;
    const uint64_t t4 = sys_time_now();
    sys_time_delay(delay);
    const uint64_t t5 = sys_time_now();
    CHECK_TRUE(t5 >= t4 + delay);

    /* Check that the POSIX-specific helpers behave as documented. */
    CHECK_INTEQUAL(sys_posix_time_epoch(), epoch);

    return 1;
}

/*-----------------------------------------------------------------------*/

/**
 * dummy_signal_handler:  Signal handler which does nothing.  Used to
 * trigger EINTR by interrupting sys_time_delay() with a signal.
 */
static void dummy_signal_handler(UNUSED int signum)
{
}

/*-----------------------------------------------------------------------*/

/**
 * sleep_thread:  Thread which sleeps for 100 milliseconds using
 * sys_time_delay().
 */
static int sleep_thread(UNUSED void *unused)
{
    sys_time_delay(sys_time_unit()/10);
    return 0;
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_posix_time)

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
    TEST_sys_posix_disable_clock_gettime = 0;
    sys_time_init();
    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_clock_gettime)
{
    if (clock_gettime(CLOCK_REALTIME, (struct timespec[1]){{0,0}}) != 0) {
        SKIP("clock_gettime() is not available on this system.");
    }
    return do_clock_test(1);
}

/*-----------------------------------------------------------------------*/

TEST(test_gettimeofday)
{
    return do_clock_test(0);
}

/*-----------------------------------------------------------------------*/

TEST(test_delay_interrupt)
{
    const double time_unit = sys_time_unit();
    const double start = sys_time_now() / time_unit;

    /* Set up a signal handler so we can interrupt sys_time_delay().
     * (Ignored signals do not interrupt system calls.) */
    struct sigaction sa, old_sa;
    sa.sa_handler = dummy_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    ASSERT(sigaction(SIGUSR1, &sa, &old_sa) == 0);

    /* Start up a thread to sleep for exactly 100 milliseconds. */
    static const ThreadAttributes attr = {.name = "PosixTimeSleep"};
    const SysThreadID thread = sys_thread_create(&attr, sleep_thread, NULL);
    ASSERT(thread);
    SysThread *sys_thread = (SysThread *)thread;

    /* Wait 50 milliseconds, then interrupt the thread with a signal. */
    ASSERT(nanosleep(&(struct timespec){0, 50000000}, NULL) == 0);
    pthread_kill(sys_thread->handle, SIGUSR1);

    /* Wait for the thread to terminate, and measure how long it took.
     * The duration should be close to 100 milliseconds; a duration of
     * close to 150 milliseconds would indicate that sys_time_delay()
     * failed to resume nanosleep() properly. */
    sys_thread_wait(thread, (int[1]){0});
    const double end = sys_time_now() / time_unit;
    ASSERT(sigaction(SIGUSR1, &old_sa, NULL) == 0);
    CHECK_DOUBLERANGE(end - start, 0.08, 0.12);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_get_utc)
{
    /* Wait until the beginning of a new second to reduce the chance of
     * spurious failure due to slow subprocess spawning. */
    DLOG("Waiting for next second...");
    const time_t now = time(NULL);
    while (time(NULL) == now) {
        thread_yield();
    }

    char timebuf[32], tzbuf[8];
    FILE *pipe;
    CHECK_TRUE(pipe = popen("date -u '+%Y-%m-%d %w %H:%M:%S'; date '+%z'",
                            "r"));
    CHECK_TRUE(fgets(timebuf, sizeof(timebuf), pipe));
    int timelen = strlen(timebuf);
    CHECK_TRUE(timelen > 0);
    CHECK_TRUE(timebuf[timelen-1] == '\n');
    timebuf[timelen-1] = '\0';
    CHECK_TRUE(fgets(tzbuf, sizeof(tzbuf), pipe));
    int tzlen = strlen(tzbuf);
    CHECK_TRUE(tzlen > 0);
    CHECK_TRUE(tzbuf[tzlen-1] == '\n');
    tzbuf[tzlen-1] = '\0';
    pclose(pipe);

    DateTime utc_time;
    const int utc_offset = sys_time_get_utc(&utc_time);
    char buf[32];
    ASSERT(strformat_check(buf, sizeof(buf),
                           "%04d-%02d-%02d %d %02d:%02d:%02d",
                           utc_time.year, utc_time.month, utc_time.day,
                           utc_time.weekday, utc_time.hour, utc_time.minute,
                           utc_time.second));
    CHECK_STREQUAL(buf, timebuf);
    CHECK_TRUE(utc_time.nsec >= 0 && utc_time.nsec < 1000000000);
    CHECK_TRUE(utc_offset > -1440 && utc_offset < 1440);
    const int tz_hours = utc_offset / 60;
    const int tz_minutes = (utc_offset + 1440) % 60;
    ASSERT(strformat_check(buf, sizeof(buf),
                           "%+03d%02d", tz_hours, tz_minutes));
    CHECK_STREQUAL(buf, tzbuf);

    return 1;
}

/*************************************************************************/
/*************************************************************************/
