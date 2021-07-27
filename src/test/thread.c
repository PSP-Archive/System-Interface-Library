/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/thread.c: Tests for the thread management functions.
 */

#include "src/base.h"
#include "src/math.h"
#include "src/sysdep.h"
#include "src/test/base.h"

/* We clear any definition of NORETURN here because we want to ensure that
 * the "return" in exit_thread() is executed if thread_exit() does not work
 * correctly. */
#undef NORETURN
#define NORETURN  /*nothing*/
#include "src/thread.h"

#ifdef SIL_PLATFORM_LINUX
# include <sys/resource.h>  // Used in test_thread_priority_increase().
#endif

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Number of iterations to spin on thread_yield() while waiting for a
 * subthread.  We set a fairly high value by default since it may take
 * some time for a thread to start up on some systems. */
#ifdef SIL_PLATFORM_PSP
# define SPIN_COUNT  1000  // The PSP is well-behaved.
#else
# define SPIN_COUNT  100000
#endif

/* Number of iterations to spin on thread_yield() for performance testing. */
#define TEST_PERFORMANCE_SPIN_COUNT  (SPIN_COUNT * 5)

/* Number of threads to start in test_thread_priority() to compete for CPU
 * time. */
#define MAX_SPIN_THREADS  16

#ifdef SIL_TEST_THREAD_PERFORMANCE

/* Positive and negative priority values to use for performance measurement.
 * These should be of sufficient magnitude to ensure that differences will
 * be properly measured on all systems which support thread priorities. */
# define POSITIVE_PRIORITY  +10
# define NEGATIVE_PRIORITY  -10

/* Tolerance for comparing performance values.  For equality tests, the
 * test succeeds if either value is within this fraction of the other.
 * For inequality tests, a > b succeeds if a > b*(1-TOLERANCE) or
 * a*(1+TOLERANCE) > b. */
# define TOLERANCE  0.25

#endif  // SIL_TEST_THREAD_PERFORMANCE

/*-----------------------------------------------------------------------*/

/* Data structure passed to counter_thread(). */
typedef struct CounterData CounterData;
struct CounterData {
    uint32_t counter;
    uint8_t stop;
};

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * run_counter_thread:  Run the counter thread for a while and return its
 * result.
 *
 * A successful run is defined as successfully creating the thread and
 * getting a return value from thread_wait() equal to the counter value in
 * the parameter block.  In particular, a return value of zero (meaning no
 * iterations were executed inside the thread) is still considered successful.
 *
 * [Parameters]
 *     use_priority: If true, call create_thread_with_priority(); if
 *         false, call create_thread().
 *     priority: Priority value to use (when use_priority != 0).
 *     spin_count: Number of times to spin on the thread_yield() before
 *         terminating the thread; if zero, spins until the thread's
 *         counter variable becomes nonzero.
 *     result_ret: Pointer to variable to receive sum of thread return values.
 * [Return value]
 *     True if the thread ran successfully, false if not.
 */
static int run_counter_thread(int use_priority, int priority, int spin_count,
                              int *result_ret);

/**
 * counter_thread:  Thread routine that counts endlessly until its
 * parameter block's stop flag is set, then returns the final count.
 *
 * [Parameters]
 *     param: Pointer to parameter block (CounterData).
 * [Return value]
 *     Number of iterations executed (== CounterData.counter).
 */
static int counter_thread(void *param);

/**
 * large_stack_thread:  Thread routine that allocates a 124*1024-byte array
 * on the stack, writes values to the entire array buffer, and performs
 * arithmetic on those values to ensure that the entire array is accessible.
 *
 * [Parameters]
 *     param_unused: Unused.
 * [Return value]
 *     503824896
 */
static int large_stack_thread(void *param_unused);

/**
 * exit_thread:  Thread routine that returns 1 via thread_exit().
 *
 * [Parameters]
 *     param: Unused.
 * [Return value]
 *     Does not return (unless thread_exit() is broken, in which case the
 *     function returns 0).
 */
static int exit_thread(UNUSED void *param);

/**
 * get_id_thread:  Thread routine that returns the return value of
 * thread_get_id().
 *
 * [Parameters]
 *     param: Unused.
 * [Return value]
 *     thread_get_id() return value.
 */
static int get_id_thread(UNUSED void *param);

/**
 * get_priority_thread:  Thread routine that returns the return value of
 * thread_get_priority().
 *
 * [Parameters]
 *     param: Unused.
 * [Return value]
 *     thread_get_priority() return value.
 */
static int get_priority_thread(UNUSED void *param);

/**
 * get_priority_caller_thread:  Thread routine that creates a new thread
 * with thread_create() and returns the priority of that thread.
 *
 * [Parameters]
 *     param: Unused.
 * [Return value]
 *     thread_get_priority() return value in the subthread.
 */
static int get_priority_caller_thread(UNUSED void *param);

/**
 * run_low_priority_thread:  Thread routine that starts the function
 * passed as the thread argument in a thread of priority -2 and returns
 * the result of that thread.
 *
 * [Parameters]
 *     function: Pointer to function to start in a low-priority thread.
 * [Return value]
 *     Function's return value as returned from the low-priority thread.
 */
static int run_low_priority_thread(void *function);

/**
 * run_default_priority_thread:  Thread routine that starts the function
 * passed as the thread argument in a thread of priority 0 and returns
 * the result of that thread.
 *
 * [Parameters]
 *     function: Pointer to function to start in a default-priority thread.
 * [Return value]
 *     Function's return value as returned from the default-priority thread.
 */
static int run_default_priority_thread(void *function);

/**
 * get_affinity_thread:  Thread routine that optionally sets the thread's
 * CPU affinity and then returns the (low bits of the) return value of
 * thread_get_affinity().
 *
 * [Parameters]
 *     param: Pointer to a uint64_t argument to thread_set_affinity(), or
 *         NULL to not call thread_set_affinity().
 * [Return value]
 *     thread_get_affinity() return value, truncated to an int.
 */
static int get_affinity_thread(void *param);

/**
 * waiter_thread:  Thread routine that waits for the value pointed to by
 * its parameter to go nonzero, calls thread_wait() on that value, sets
 * the value pointed to by its parameter to zero, and returns the result
 * of thread_wait().
 *
 * [Parameters]
 *     param: Pointer to int variable (see function description).
 * [Return value]
 *     Result of thread_wait().
 */
static int waiter_thread(void *param);

/**
 * waiter2_thread:  Alternate version of waiter_thread() which calls
 * thread_wait2() instead of thread_wait().
 *
 * [Parameters]
 *     param: Pointer to int variable (see waiter_thread() function
 *         description).
 * [Return value]
 *     Result of thread_wait2().
 */
static int waiter2_thread(void *param);

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_thread)

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

/*-----------------------------------------------------------------------*/

TEST(test_thread_create)
{
    int result;
    if (!run_counter_thread(0, 0, 0, &result)) {
        FAIL("Thread run failed");
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_thread_create_memory_failure)
{
    int result;
    CHECK_MEMORY_FAILURES(run_counter_thread(0, 0, 0, &result));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_thread_create_with_priority)
{
    int result;
    if (!run_counter_thread(1, 0, 0, &result)) {
        FAIL("Thread run failed");
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_thread_create_with_stack_size)
{
    int thread;
    static const ThreadAttributes attr =
        {.stack_size = 131072, .name = "StackSize"};
    CHECK_TRUE(thread = thread_create_with_attr(
                   &attr, large_stack_thread, NULL));
    CHECK_INTEQUAL(thread_wait(thread), 503824896);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_thread_create_default_affinity)
{
    if (thread_get_num_cores() < 2) {
        SKIP("Only one core on this system.");
    }

    const uint64_t default_affinity = thread_get_affinity();
    int thread;
    CHECK_TRUE(thread = thread_create(get_affinity_thread, NULL));
    CHECK_INTEQUAL(thread_wait(thread), (int)default_affinity);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_thread_create_with_affinity)
{
    if (thread_get_num_cores() < 2) {
        SKIP("Only one core on this system.");
    }

    const uint64_t default_affinity = thread_get_affinity();
    int thread;
    /* Make sure the new thread is spawned with a different affinity. */
    ThreadAttributes attr = {
        .priority = 0,
        .stack_size = 0,
        .affinity = (default_affinity==1<<0 ? 1<<1 : 1<<0),
        .name = "GetAffinity"};
    CHECK_TRUE(thread = thread_create_with_attr(
                   &attr, get_affinity_thread, NULL));
#if defined(SIL_PLATFORM_MACOSX) || defined(SIL_PLATFORM_IOS)
    /* Affinity is not supported on Darwin platforms. */
    CHECK_INTEQUAL(thread_wait(thread), -1);
#else
    CHECK_INTEQUAL(thread_wait(thread), attr.affinity);
#endif

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_thread_create_invalid)
{
    ThreadAttributes attr =
        {.priority = 0, .stack_size = 0, .affinity = 0, .name = NULL};

    CHECK_FALSE(thread_create(NULL, NULL));
    CHECK_FALSE(thread_create_with_priority(0, NULL, NULL));
    CHECK_FALSE(thread_create_with_attr(NULL, counter_thread, NULL));
    CHECK_FALSE(thread_create_with_attr(&attr, NULL, NULL));
    attr.stack_size = -1;
    CHECK_FALSE(thread_create_with_attr(&attr, counter_thread, NULL));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_thread_create_forced_failure)
{
    TEST_thread_fail_create(1);

    CHECK_FALSE(thread_create(counter_thread, NULL));
    CHECK_FALSE(thread_create_with_priority(0, counter_thread, NULL));
    static const ThreadAttributes attr;  // All zero.
    CHECK_FALSE(thread_create_with_attr(&attr, counter_thread, NULL));

    TEST_thread_fail_create(0);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_thread_exit)
{
    int thread;

    CHECK_TRUE(thread = thread_create(exit_thread, NULL));
    CHECK_INTEQUAL(thread_wait(thread), 1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_thread_get_id)
{
    int thread;

    CHECK_TRUE(thread = thread_create(get_id_thread, NULL));
    CHECK_INTEQUAL(thread_wait(thread), thread);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_thread_get_id_main_thread)
{
    /* On systems where the main thread is not a SIL thread,
     * sys_thread_get_id() will return zero.  Ensure that thread_get_id()
     * doesn't treat this as matching an unused entry in the ID array. */

    if (sys_thread_get_id()) {
        SKIP("Main thread is a SIL thread.");
    }

    /* Create a thread so the ID array has some unused entries in it
     * when we call thread_get_id(). */
    int thread;
    CHECK_TRUE(thread = thread_create(get_id_thread, NULL));
    const int my_id = thread_get_id();
    thread_wait(thread);

    CHECK_INTEQUAL(my_id, 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_thread_get_priority)
{
    int thread;

    CHECK_INTEQUAL(thread_get_priority(), 0);

    CHECK_TRUE(thread = thread_create_with_priority(
                   -1, get_priority_thread, NULL));
    /* At the moment, all systems we support will successfully set a
     * priority of -1. */
    CHECK_INTEQUAL(thread_wait(thread), -1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_thread_create_uses_current_priority)
{
    int thread;

    CHECK_TRUE(thread = thread_create_with_priority(
                   -1, get_priority_caller_thread, NULL));
    CHECK_INTEQUAL(thread_wait(thread), -1);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_thread_priority_is_absolute)
{
    int thread;

    CHECK_TRUE(thread = thread_create_with_priority(
                   -1, run_low_priority_thread, get_priority_thread));
    CHECK_INTEQUAL(thread_wait(thread), -2);  // Should not be -3.

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_thread_priority_increase)
{
#ifdef SIL_PLATFORM_LINUX
    struct rlimit rlim;
    if (getrlimit(RLIMIT_NICE, &rlim) == 0 && rlim.rlim_cur != RLIM_INFINITY) {
        const int pri_min = 20 - bound(rlim.rlim_cur, 1, 40);
        if (pri_min > 0) {
            WARN("Process resource limits are not currently configured to"
                 " allow creation of threads with increased priority."
                 "  This can usually be fixed by adding the following two"
                 " lines to /etc/security/limits.conf and logging out and"
                 " back in (note that the \"*\" is part of the text to be"
                 " added):\n"
                 "    * hard nice -10\n"
                 "    * soft nice -10\n"
                 "See the Linux-specific notes in the documentation for"
                 " details.");
            return 1;
        }
    }
#endif

    int thread;

    CHECK_TRUE(thread = thread_create_with_priority(
                   -1, run_default_priority_thread, get_priority_thread));
    CHECK_INTEQUAL(thread_wait(thread), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_thread_set_affinity)
{
    if (thread_get_num_cores() < 2) {
        SKIP("Only one core on this system.");
    }

    int thread;

    static const ThreadAttributes attr =
        {.affinity = 1<<0, .name = "SetAffinity"};
    uint64_t new_affinity = 1<<1;
    CHECK_TRUE(thread = thread_create_with_attr(
                   &attr, get_affinity_thread, &new_affinity));
#if defined(SIL_PLATFORM_MACOSX) || defined(SIL_PLATFORM_IOS)
    CHECK_INTEQUAL(thread_wait(thread), -1);
#else
    CHECK_INTEQUAL(thread_wait(thread), new_affinity);
#endif

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_thread_set_affinity_zero)
{
    if (thread_get_num_cores() < 2) {
        SKIP("Only one core on this system.");
    }

    int thread;

    static const ThreadAttributes attr =
        {.affinity = 1<<0, .name = "SetAffinityZero"};
    uint64_t new_affinity = 0;
    CHECK_TRUE(thread = thread_create_with_attr(
                   &attr, get_affinity_thread, &new_affinity));
#if defined(SIL_PLATFORM_MACOSX) || defined(SIL_PLATFORM_IOS)
    CHECK_INTEQUAL(thread_wait(thread), -1);
#else
    const int num_cores = thread_get_num_cores();
    const uint64_t valid_cpu_mask =
        num_cores >= 64 ? ~UINT64_C(0) : (UINT64_C(1) << num_cores) - 1;
    CHECK_INTEQUAL(thread_wait(thread), (int)valid_cpu_mask);
#endif

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_thread_is_running)
{
    int thread;
    CounterData counter_data;

    counter_data.counter = 0;
    counter_data.stop = 0;
    thread = thread_create(counter_thread, &counter_data);
    if (!thread) {
        FAIL("Failed to create counter thread");
    }
    if (!thread_is_running(thread)) {
        FAIL("thread_is_running() reported running thread as stopped");
    }
    counter_data.stop = 1;
    for (int try = 0; thread_is_running(thread) && try < SPIN_COUNT; try++) {
        thread_yield();
    }
    if (thread_is_running(thread)) {
        thread_wait(thread);
        FAIL("thread_is_running() reported stopped thread as running (or"
             " system is too loaded for this test)");
    }
    thread_wait(thread);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_thread_wait2)
{
    int thread;
    int result = 0;

    static const ThreadAttributes attr =
        {.stack_size = 131072, .name = "ThreadWait2"};
    CHECK_TRUE(thread = thread_create_with_attr(
                   &attr, large_stack_thread, NULL));
    CHECK_TRUE(thread_wait2(thread, &result));
    CHECK_INTEQUAL(result, 503824896);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_thread_wait_self)
{
    int thread;
    int param = 0;

    CHECK_TRUE(thread = thread_create(waiter_thread, &param));
    param = thread;
    do {
        thread_yield();
    } while (param);
    CHECK_INTEQUAL(thread_wait(thread), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_thread_wait2_self)
{
    int thread;
    int param = 0;

    CHECK_TRUE(thread = thread_create(waiter2_thread, &param));
    param = thread;
    do {
        thread_yield();
    } while (param);
    CHECK_INTEQUAL(thread_wait(thread), 0);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_thread_invalid)
{
    int thread, result;

    CHECK_FALSE(thread_create(NULL, NULL));
    CHECK_FALSE(thread_create_with_priority(0, NULL, NULL));
    CHECK_FALSE(thread_is_running(0));
    CHECK_FALSE(thread_is_running(INT_MAX));
    CHECK_FALSE(thread_wait(0));
    CHECK_FALSE(thread_wait(INT_MAX));
    CHECK_FALSE(thread_wait2(0, &result));
    CHECK_FALSE(thread_wait2(INT_MAX, &result));

    static const ThreadAttributes attr =
        {.stack_size = 131072, .name = "Wait2Invalid"};
    CHECK_TRUE(thread = thread_create_with_attr(
                   &attr, large_stack_thread, NULL));
    CHECK_FALSE(thread_wait2(thread, NULL));
    CHECK_INTEQUAL(thread_wait(thread), 503824896);
    CHECK_FALSE(thread_wait(thread));
    CHECK_FALSE(thread_wait2(thread, &result));

    return 1;
}

/*-----------------------------------------------------------------------*/

#ifdef SIL_TEST_THREAD_PERFORMANCE

/* Make sure we clean up all leftover threads before returning from this
 * function. */
#undef FAIL_ACTION
#define FAIL_ACTION  goto cleanup_threads

TEST(test_thread_priority)
{
    int result = 0;

    DLOG("Testing thread performance...");

    /* Start some threads to compete with our test threads for CPU time.
     * For these to have any effect, we need to start up at least one
     * thread per independent processing unit (e.g., CPU core), but we
     * don't have a cross-platform way to get the number of CPUs in the
     * system, and we can't measure performance because the system may
     * artificially limit performance based on the number of threads in
     * the process separately from thread priorities, so we just blindly
     * start a fixed number of threads.  If it's not enough, increase the
     * value of MAX_SPIN_THREADS. */
    CounterData spin_data[MAX_SPIN_THREADS];
    int spin_threads[MAX_SPIN_THREADS];
    int num_spin_threads;
    for (num_spin_threads = 0; num_spin_threads < MAX_SPIN_THREADS;
         num_spin_threads++)
    {
        spin_data[num_spin_threads].stop = 0;
        spin_threads[num_spin_threads] =
            thread_create(counter_thread, &spin_data[num_spin_threads]);
        if (!spin_threads[num_spin_threads]) {
            FAIL("Failed to create spin thread %d", num_spin_threads);
        }
    }

    /* Check that a thread run explicitly at priority zero performs the
     * same as a thread run without an explicit priority. */
    int sum_nopri;
    if (!run_counter_thread(0, 0, TEST_PERFORMANCE_SPIN_COUNT, &sum_nopri)) {
        FAIL("Thread run failed");
    }
    DLOG("    Default priority: %d", sum_nopri);
    int sum_pri0;
    if (!run_counter_thread(1, 0, TEST_PERFORMANCE_SPIN_COUNT, &sum_pri0)) {
        FAIL("Thread run failed");
    }
    DLOG("    Priority 0: %d", sum_pri0);
    if (fabs(sum_pri0 - sum_nopri) > sum_nopri * TOLERANCE
     && fabs(sum_pri0 - sum_nopri) > sum_pri0 * TOLERANCE) {
        FAIL("Non-priority thread and priority 0 thread perform differently"
             " (non-priority: %d, priority 0: %d).  NOTE: This test may be"
             " affected by system load.", sum_nopri, sum_pri0);
    }

    /* Check that threads at positive and negative priorities run for
     * respectively no less and no more time than a priority-zero thread. */
    int sum_pos;
    if (!run_counter_thread(1, POSITIVE_PRIORITY, TEST_PERFORMANCE_SPIN_COUNT,
                            &sum_pos)) {
        FAIL("Thread run failed");
    }
    DLOG("    Priority %d: %d", POSITIVE_PRIORITY, sum_pos);
    if (sum_pos < sum_pri0*(1-TOLERANCE) && sum_pos*(1+TOLERANCE) < sum_pri0) {
        FAIL("Positive-priority thread runs shorter then priority 0 thread"
             " (priority %d: %d, priority 0: %d).  NOTE: This test may be"
             " affected by system load.", POSITIVE_PRIORITY, sum_pos,
             sum_pri0);
    }
    int sum_neg;
    if (!run_counter_thread(1, NEGATIVE_PRIORITY, TEST_PERFORMANCE_SPIN_COUNT,
                            &sum_neg)) {
        FAIL("Thread run failed");
    }
    DLOG("    Priority %d: %d", NEGATIVE_PRIORITY, sum_neg);
    if (sum_pri0 < sum_neg*(1-TOLERANCE) && sum_pri0*(1+TOLERANCE) < sum_neg) {
        FAIL("Negative-priority thread runs longer then priority 0 thread"
             " (priority %d: %d, priority 0: %d).  NOTE: This test may be"
             " affected by system load.", NEGATIVE_PRIORITY, sum_neg,
             sum_pri0);
    }

    result = 1;

  cleanup_threads:
    for (int i = 0; i < num_spin_threads; i++) {
        spin_data[i].stop = 1;
        thread_wait(spin_threads[i]);
    }
    return result;
}

#undef FAIL_ACTION
#define FAIL_ACTION  return 0

#endif  // SIL_TEST_THREAD_PERFORMANCE

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static int run_counter_thread(int use_priority, int priority, int spin_count,
                              int *result_ret)
{
    PRECOND(result_ret != NULL, return 0);

    *result_ret = 0;

    CounterData counter_data;
    int thread;
    int result;

    counter_data.counter = 0;
    counter_data.stop = 0;
    if (use_priority) {
        thread = thread_create_with_priority(
            priority, counter_thread, &counter_data);
    } else {
        thread = thread_create(counter_thread, &counter_data);
    }
    if (!thread) {
        DLOG("Failed to create counter thread");
        return 0;
    }

    if (spin_count > 0) {
        for (int spin = 0; spin < spin_count; spin++) {
            thread_yield();
        }
    } else {
        while (counter_data.counter == 0) {
            thread_yield();
        }
    }

    counter_data.stop = 1;
    result = thread_wait(thread);
    if (result != (int)counter_data.counter) {
        DLOG("Wrong return value from thread_wait (got %d, should be %d)",
             result, (int)counter_data.counter);
        return 0;
    }

    *result_ret = result;
    return 1;
}

/*-----------------------------------------------------------------------*/

static int counter_thread(void *param)
{
    CounterData *data = (CounterData *)param;
    while (!data->stop) {
        data->counter++;
#ifdef SIL_TEST_THREAD_PERFORMANCE
        BARRIER();
#else
        thread_yield();
#endif
    }
    return (int)data->counter;
}

/*-----------------------------------------------------------------------*/

static int large_stack_thread(UNUSED void *param_unused)
{
    /* Declare the array volatile to ensure that the memory accesses
     * actually occur, since otherwise the compiler could theoretically
     * optimize this entire routine down to a single return statement. */
    volatile uint32_t array[(124*1024)/4];

    for (int i = 0; i < lenof(array); i++) {
        array[i] = i;
    }
    int sum = 0;
    for (int i = 0; i < lenof(array); i++) {
        sum += array[i];
    }
    return sum;
}

/*-----------------------------------------------------------------------*/

static int exit_thread(UNUSED void *param)
{
    thread_exit(1);
    return 0;
}

/*-----------------------------------------------------------------------*/

static int get_id_thread(UNUSED void *param)
{
    return thread_get_id();
}

/*-----------------------------------------------------------------------*/

static int get_priority_thread(UNUSED void *param)
{
    return thread_get_priority();
}

/*-----------------------------------------------------------------------*/

static int get_priority_caller_thread(UNUSED void *param)
{
    const int thread = thread_create(get_priority_thread, NULL);
    if (!thread) {
        return -999999999;
    }
    return thread_wait(thread);
}

/*-----------------------------------------------------------------------*/

static int run_low_priority_thread(void *function)
{
    int thread;
    CHECK_TRUE(thread = thread_create_with_priority(-2, function, NULL));
    return thread_wait(thread);
}

/*-----------------------------------------------------------------------*/

static int run_default_priority_thread(void *function)
{
    int thread;
    CHECK_TRUE(thread = thread_create_with_priority(0, function, NULL));
    return thread_wait(thread);
}

/*-----------------------------------------------------------------------*/

static int get_affinity_thread(void *param)
{
    if (param) {
        thread_set_affinity(*(uint64_t *)param);
    }
    return (int)thread_get_affinity();
}

/*-----------------------------------------------------------------------*/

static int waiter_thread(void *param)
{
    int *thread_ptr = (int *)param;
    while (!*thread_ptr) {
        thread_yield();
    }
    const int result = thread_wait(*thread_ptr);
    *thread_ptr = 0;
    BARRIER();
    return result;
}

/*-----------------------------------------------------------------------*/

static int waiter2_thread(void *param)
{
    int *thread_ptr = (int *)param;
    while (!*thread_ptr) {
        thread_yield();
    }
    int thread_result;
    const int result = thread_wait2(*thread_ptr, &thread_result);
    *thread_ptr = 0;
    BARRIER();
    return result;
}

/*************************************************************************/
/*************************************************************************/
