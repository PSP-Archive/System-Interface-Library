/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/workqueue.c: Tests for the work queue functions.
 */

#include "src/base.h"
#include "src/memory.h"
#include "src/semaphore.h"
#include "src/test/base.h"
#include "src/thread.h"
#include "src/workqueue.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Work queue and work unit IDs guaranteed to be invalid across all tests. */
#define INVALID_WORKQUEUE  10000
#define INVALID_WORKUNIT  10000

/* Number of iterations to spin on thread_yield() while waiting for threads
 * to do something. */
#ifdef SIL_PLATFORM_PSP
# define SPIN_COUNT  1000
#else
# define SPIN_COUNT  100000
#endif

/*-----------------------------------------------------------------------*/

/* Data structure passed to work_function(). */
typedef struct WorkData WorkData;
struct WorkData {
    int started;           // Set to 1 when called.
    int result;            // Value to return from the function.
    int semaphore_start;   // Semaphore to signal when called.
    int semaphore_finish;  // Semaphore to wait for before returning.
};

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * work_function:  Function passed as to workqueue_submit() as a work
 * function.  Signals the start semaphore (if any), then waits for the
 * finish semaphore (if any) before returning.
 *
 * [Parameters]
 *     data_: Pointer to parameter block (WorkData).
 * [Return value]
 *     WorkData.retval
 */
static int work_function(void *data_);

/**
 * empty_work_function:  Function passed as to workqueue_submit() as a
 * work function.  Does nothing, returning immediately.
 *
 * [Return value]
 *     1
 */
static int empty_work_function(UNUSED void *unused);

/**
 * delayed_work_function:  Function passed as to workqueue_submit() as a
 * work function.  Busy-waits for SPIN_COUNT cycles, then increments the
 * int variable pointed to by the parameter and returns.
 *
 * [Parameters]
 *     data_: Pointer to an int variable which is incremented after a delay.
 * [Return value]
 *     1
 */
static int delayed_work_function(void *data_);

/**
 * waiter_thread:  Simple function to call workqueue_wait() on the work
 * unit with ID 1 in the given work queue.  Used to test the behavior of
 * workqueue_wait() when it needs to wait for work unit completion.
 *
 * [Parameters]
 *     wq: Work queue ID (cast to a pointer).
 * [Return value]
 *     Return value of workqueue_wait(wq, 1).
 */
static int waiter_thread(void *wq_);

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_workqueue)

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
/*********************** Basic functionality tests ***********************/
/*************************************************************************/

TEST(test_workqueue_basic)
{
    int wq;

    /* Check that a basic work queue can be created. */
    CHECK_TRUE(wq = workqueue_create(1));

    /* Check that work can be submitted and waited for. */
    WorkData data = {.started = 0, .result = 123, .semaphore_start = 0,
                     .semaphore_finish = 0};
    int wu;
    CHECK_TRUE(wu = workqueue_submit(wq, work_function, &data));
    CHECK_INTEQUAL(workqueue_wait(wq, wu), 123);
    CHECK_TRUE(data.started);

    /* Check that destroying the work queue doesn't crash. */
    workqueue_destroy(wq);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_workqueue_create_memory_failure)
{
    int wq;

    CHECK_MEMORY_FAILURES(wq = workqueue_create(1));

    WorkData data = {.started = 0, .result = 123, .semaphore_start = 0,
                     .semaphore_finish = 0};
    int wu;
    CHECK_TRUE(wu = workqueue_submit(wq, work_function, &data));
    CHECK_INTEQUAL(workqueue_wait(wq, wu), 123);

    workqueue_destroy(wq);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_workqueue_create_invalid)
{
    CHECK_FALSE(workqueue_create(0));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_workqueue_destroy_null)
{
    workqueue_destroy(0);  // Just make sure it doesn't crash.
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_workqueue_destroy_invalid)
{
    int wq;
    CHECK_TRUE(wq = workqueue_create(1));
    workqueue_destroy(wq);

    /* Just make sure these don't crash. */
    workqueue_destroy(wq);
    workqueue_destroy(INVALID_WORKQUEUE);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_workqueue_is_busy)
{
    int wq;
    CHECK_TRUE(wq = workqueue_create(1));
    CHECK_FALSE(workqueue_is_busy(wq));

    WorkData data = {.started = 0, .result = 123};
    ASSERT(data.semaphore_start = semaphore_create(0, 1));
    ASSERT(data.semaphore_finish = semaphore_create(0, 1));
    int wu;
    CHECK_TRUE(wu = workqueue_submit(wq, work_function, &data));
    CHECK_TRUE(workqueue_is_busy(wq));
    semaphore_wait(data.semaphore_start);
    CHECK_TRUE(data.started);
    CHECK_TRUE(workqueue_is_busy(wq));

    semaphore_signal(data.semaphore_finish);
    CHECK_INTEQUAL(workqueue_wait(wq, wu), 123);
    for (int i = 0; i < SPIN_COUNT; i++) {
        thread_yield();
    }
    CHECK_FALSE(workqueue_is_busy(wq));

    semaphore_destroy(data.semaphore_start);
    semaphore_destroy(data.semaphore_finish);
    workqueue_destroy(wq);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_workqueue_is_busy_invalid)
{
    int wq;
    CHECK_TRUE(wq = workqueue_create(1));
    workqueue_destroy(wq);

    CHECK_FALSE(workqueue_is_busy(0));
    CHECK_FALSE(workqueue_is_busy(wq));
    CHECK_FALSE(workqueue_is_busy(INVALID_WORKQUEUE));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_workqueue_wait_all)
{
    int wq;
    CHECK_TRUE(wq = workqueue_create(1));

    WorkData data = {.started = 0, .result = 123, .semaphore_start = 0,
                     .semaphore_finish = 0};
    int wu;
    CHECK_TRUE(wu = workqueue_submit(wq, work_function, &data));
    workqueue_wait_all(wq);
    CHECK_FALSE(workqueue_is_busy(wq));
    CHECK_TRUE(data.started);

    /* A second call should do nothing. */
    workqueue_wait_all(wq);
    CHECK_FALSE(workqueue_is_busy(wq));

    /* The work unit should have been reaped by workqueue_wait_all(), so
     * we shouldn't be able to retrieve its result here. */
    CHECK_FALSE(workqueue_wait(wq, wu));

    workqueue_destroy(wq);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_workqueue_wait_all_invalid)
{
    int wq;
    CHECK_TRUE(wq = workqueue_create(1));
    workqueue_destroy(wq);

    /* Just check that these don't hang or crash. */
    workqueue_wait_all(0);
    workqueue_wait_all(wq);
    workqueue_wait_all(INVALID_WORKQUEUE);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_workqueue_submit_multiple)
{
    int wq;
    CHECK_TRUE(wq = workqueue_create(1));

    WorkData data1 = {.started = 0, .result = 123, .semaphore_start = 0};
    WorkData data2 = {.started = 0, .result = 456, .semaphore_start = 0};
    ASSERT(data1.semaphore_finish = semaphore_create(0, 1));
    ASSERT(data2.semaphore_finish = semaphore_create(0, 1));
    int wu1, wu2;
    CHECK_TRUE(wu1 = workqueue_submit(wq, work_function, &data1));
    CHECK_TRUE(wu2 = workqueue_submit(wq, work_function, &data2));

    semaphore_signal(data1.semaphore_finish);
    CHECK_INTEQUAL(workqueue_wait(wq, wu1), 123);

    semaphore_signal(data2.semaphore_finish);
    CHECK_INTEQUAL(workqueue_wait(wq, wu2), 456);

    semaphore_destroy(data1.semaphore_finish);
    semaphore_destroy(data2.semaphore_finish);
    workqueue_destroy(wq);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_workqueue_submit_multiple_memory_failure)
{
    int wq;
    CHECK_TRUE(wq = workqueue_create(1));

    WorkData data1 = {.started = 0, .result = 123, .semaphore_start = 0};
    WorkData data2 = {.started = 0, .result = 456, .semaphore_start = 0};
    ASSERT(data1.semaphore_finish = semaphore_create(0, 1));
    ASSERT(data2.semaphore_finish = semaphore_create(0, 1));
    int wu1, wu2;
    CHECK_TRUE(wu1 = workqueue_submit(wq, work_function, &data1));
    CHECK_MEMORY_FAILURES(wu2 = workqueue_submit(wq, work_function, &data2));

    semaphore_signal(data1.semaphore_finish);
    CHECK_INTEQUAL(workqueue_wait(wq, wu1), 123);

    semaphore_signal(data2.semaphore_finish);
    CHECK_INTEQUAL(workqueue_wait(wq, wu2), 456);

    semaphore_destroy(data1.semaphore_finish);
    semaphore_destroy(data2.semaphore_finish);
    workqueue_destroy(wq);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_workqueue_submit_invalid)
{
    int wq;
    CHECK_TRUE(wq = workqueue_create(1));
    CHECK_FALSE(workqueue_submit(wq, NULL, NULL));
    workqueue_destroy(wq);

    WorkData data;
    CHECK_FALSE(workqueue_submit(0, work_function, &data));
    CHECK_FALSE(workqueue_submit(wq, work_function, &data));
    CHECK_FALSE(workqueue_submit(INVALID_WORKQUEUE, work_function, &data));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_workqueue_poll_invalid)
{
    int wq;
    CHECK_TRUE(wq = workqueue_create(1));

    WorkData data = {.started = 0, .result = 123, .semaphore_start = 0,
                     .semaphore_finish = 0};
    int wu;
    CHECK_TRUE(wu = workqueue_submit(wq, work_function, &data));

    CHECK_TRUE(workqueue_poll(0, wu));
    CHECK_TRUE(workqueue_poll(INVALID_WORKQUEUE, wu));

    CHECK_INTEQUAL(workqueue_wait(wq, wu), 123);

    CHECK_TRUE(workqueue_poll(wq, 0));
    CHECK_TRUE(workqueue_poll(wq, wu));
    CHECK_TRUE(workqueue_poll(wq, INVALID_WORKUNIT));

    workqueue_destroy(wq);

    CHECK_TRUE(workqueue_poll(wq, wu));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_workqueue_wait_wait)
{
    int wq;
    CHECK_TRUE(wq = workqueue_create(1));

    WorkData data = {.started = 0, .result = 123, .semaphore_start = 0};
    ASSERT(data.semaphore_finish = semaphore_create(0, 1));
    int wu;
    CHECK_TRUE(wu = workqueue_submit(wq, work_function, &data));
    ASSERT(wu == 1);  // For simplicity in passing data to waiter_thread().

    /* Check that workqueue_wait() works properly when it needs to wait
     * for the work unit to complete.  Since we can't let it block the
     * test itself, we spawn a separate thread to call workqueue_wait(),
     * then release the work unit after spinning for a bit to ensure that
     * workqueue_wait() is in fact waiting. */
    int thread;
    ASSERT(thread = thread_create(waiter_thread, (void *)(intptr_t)wq));
    for (int i = 0; i < SPIN_COUNT; i++) {
        thread_yield();
    }
    semaphore_signal(data.semaphore_finish);
    CHECK_INTEQUAL(thread_wait(thread), 123);
    CHECK_TRUE(data.started);

    semaphore_destroy(data.semaphore_finish);
    workqueue_destroy(wq);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_workqueue_wait_semaphore_failure)
{
    int wq;
    CHECK_TRUE(wq = workqueue_create(1));

    WorkData data = {.started = 0, .result = 123, .semaphore_start = 0};
    ASSERT(data.semaphore_finish = semaphore_create(0, 1));
    int wu;
    CHECK_TRUE(wu = workqueue_submit(wq, work_function, &data));
    ASSERT(wu == 1);  // For simplicity in passing data to waiter_thread().

    TEST_semaphore_fail_create(1);
    int thread;
    ASSERT(thread = thread_create(waiter_thread, (void *)(intptr_t)wq));
    for (int i = 0; i < SPIN_COUNT; i++) {
        thread_yield();
    }
    semaphore_signal(data.semaphore_finish);
    CHECK_INTEQUAL(thread_wait(thread), 123);
    TEST_semaphore_fail_create(0);
    CHECK_TRUE(data.started);

    semaphore_destroy(data.semaphore_finish);
    workqueue_destroy(wq);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_workqueue_wait_invalid)
{
    int wq;
    CHECK_TRUE(wq = workqueue_create(1));

    WorkData data = {.started = 0, .result = 123, .semaphore_start = 0,
                     .semaphore_finish = 0};
    int wu;
    CHECK_TRUE(wu = workqueue_submit(wq, work_function, &data));

    CHECK_FALSE(workqueue_wait(0, wu));
    CHECK_FALSE(workqueue_wait(INVALID_WORKQUEUE, wu));

    CHECK_INTEQUAL(workqueue_wait(wq, wu), 123);

    CHECK_FALSE(workqueue_wait(wq, 0));
    CHECK_FALSE(workqueue_wait(wq, wu));
    CHECK_FALSE(workqueue_wait(wq, INVALID_WORKUNIT));

    workqueue_destroy(wq);

    CHECK_FALSE(workqueue_wait(wq, wu));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_workqueue_cancel)
{
    int wq;
    CHECK_TRUE(wq = workqueue_create(1));

    WorkData data1 = {.started = 0, .result = 123};
    WorkData data2 = {.started = 0, .result = 456, .semaphore_start = 0};
    WorkData data3 = {.started = 0, .result = 789, .semaphore_start = 0};
    WorkData data4 = {.started = 0, .result = 555, .semaphore_start = 0};
    ASSERT(data1.semaphore_start = semaphore_create(0, 1));
    ASSERT(data1.semaphore_finish = semaphore_create(0, 1));
    ASSERT(data2.semaphore_finish = semaphore_create(0, 1));
    ASSERT(data3.semaphore_finish = semaphore_create(0, 1));
    ASSERT(data4.semaphore_finish = semaphore_create(0, 1));
    int wu1, wu2, wu3, wu4;
    CHECK_TRUE(wu1 = workqueue_submit(wq, work_function, &data1));
    CHECK_TRUE(wu2 = workqueue_submit(wq, work_function, &data2));
    CHECK_TRUE(wu3 = workqueue_submit(wq, work_function, &data3));
    CHECK_TRUE(wu4 = workqueue_submit(wq, work_function, &data4));

    /* We shouldn't be able to cancel a work unit already in progress. */
    semaphore_wait(data1.semaphore_start);
    CHECK_FALSE(workqueue_cancel(wq, wu1));

    /* But we should be able to cancel a work unit that hasn't started yet.
     * We do the cancels in this order to test handling of list pointers at
     * the middle, end, and beginning of the pending list, respectively. */
    CHECK_TRUE(workqueue_cancel(wq, wu3));
    CHECK_TRUE(workqueue_cancel(wq, wu4));
    CHECK_TRUE(workqueue_cancel(wq, wu2));

    /* Make sure the cancelled work units really didn't get executed. */
    semaphore_signal(data1.semaphore_finish);
    CHECK_INTEQUAL(workqueue_wait(wq, wu1), 123);
    CHECK_FALSE(workqueue_wait(wq, wu2));  // Will hang if wu2 is running.
    workqueue_wait_all(wq);  // Will hang if wu3 or wu4 are running.
    CHECK_TRUE(data1.started);
    CHECK_FALSE(data2.started);
    CHECK_FALSE(data3.started);
    CHECK_FALSE(data4.started);

    semaphore_destroy(data1.semaphore_start);
    semaphore_destroy(data1.semaphore_finish);
    semaphore_destroy(data2.semaphore_finish);
    semaphore_destroy(data3.semaphore_finish);
    semaphore_destroy(data4.semaphore_finish);
    workqueue_destroy(wq);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_workqueue_cancel_invalid)
{
    int wq;
    CHECK_TRUE(wq = workqueue_create(1));

    WorkData data1 = {.started = 0, .result = 123, .semaphore_start = 0};
    WorkData data2 = {.started = 0, .result = 456, .semaphore_start = 0};
    ASSERT(data1.semaphore_finish = semaphore_create(0, 1));
    ASSERT(data2.semaphore_finish = semaphore_create(0, 1));
    int wu1, wu2;
    CHECK_TRUE(wu1 = workqueue_submit(wq, work_function, &data1));
    CHECK_TRUE(wu2 = workqueue_submit(wq, work_function, &data2));

    CHECK_FALSE(workqueue_cancel(0, wu2));
    CHECK_FALSE(workqueue_cancel(INVALID_WORKQUEUE, wu2));

    semaphore_signal(data1.semaphore_finish);
    semaphore_signal(data2.semaphore_finish);
    workqueue_wait_all(wq);
    CHECK_TRUE(data1.started);
    CHECK_TRUE(data2.started);

    CHECK_FALSE(workqueue_cancel(wq, 0));
    CHECK_FALSE(workqueue_cancel(wq, wu1));
    CHECK_FALSE(workqueue_cancel(wq, wu2));
    CHECK_FALSE(workqueue_cancel(wq, INVALID_WORKUNIT));

    workqueue_destroy(wq);

    CHECK_FALSE(workqueue_cancel(wq, wu1));
    CHECK_FALSE(workqueue_cancel(wq, wu2));

    semaphore_destroy(data1.semaphore_finish);
    semaphore_destroy(data2.semaphore_finish);
    return 1;
}

/*************************************************************************/
/************************ Complex behavior tests *************************/
/*************************************************************************/

/* Essentially the same as test_workqueue_submit_multiple(), but checking
 * more details of behavior. */
TEST(test_workqueue_submit_multiple_2)
{
    int wq;
    CHECK_TRUE(wq = workqueue_create(1));

    WorkData data1 = {.started = 0, .result = 123};
    WorkData data2 = {.started = 0, .result = 456};
    ASSERT(data1.semaphore_start = semaphore_create(0, 1));
    ASSERT(data2.semaphore_start = semaphore_create(0, 1));
    ASSERT(data1.semaphore_finish = semaphore_create(0, 1));
    ASSERT(data2.semaphore_finish = semaphore_create(0, 1));
    int wu1, wu2;
    CHECK_TRUE(wu1 = workqueue_submit(wq, work_function, &data1));
    CHECK_TRUE(wu2 = workqueue_submit(wq, work_function, &data2));

    /* Only the first work unit should have started.  Assume that if the
     * second work unit had started, it would have signalled the start
     * semaphore within 10 milliseconds. */
    semaphore_wait(data1.semaphore_start);
    CHECK_FALSE(semaphore_wait_timeout(data2.semaphore_start, 0.01));
    CHECK_TRUE(data1.started);
    CHECK_FALSE(data2.started);
    CHECK_FALSE(workqueue_poll(wq, wu1));
    CHECK_FALSE(workqueue_poll(wq, wu2));

    /* Let the first work unit finish, which should allow the second unit
     * to proceed. */
    semaphore_signal(data1.semaphore_finish);
    for (int i = 0; i < SPIN_COUNT; i++) {
        thread_yield();
    }
    CHECK_TRUE(workqueue_poll(wq, wu1));
    CHECK_INTEQUAL(workqueue_wait(wq, wu1), 123);
    semaphore_wait(data2.semaphore_start);
    CHECK_TRUE(data2.started);
    CHECK_FALSE(workqueue_poll(wq, wu2));
    CHECK_TRUE(workqueue_is_busy(wq));

    /* Let the second work unit finish and check that the work queue is no
     * longer reported as busy. */
    semaphore_signal(data2.semaphore_finish);
    CHECK_INTEQUAL(workqueue_wait(wq, wu2), 456);
    for (int i = 0; i < SPIN_COUNT; i++) {
        thread_yield();
    }
    CHECK_FALSE(workqueue_is_busy(wq));

    semaphore_destroy(data1.semaphore_start);
    semaphore_destroy(data2.semaphore_start);
    semaphore_destroy(data1.semaphore_finish);
    semaphore_destroy(data2.semaphore_finish);
    workqueue_destroy(wq);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_workqueue_multiple_threads)
{
    int wq;
    CHECK_TRUE(wq = workqueue_create(2));

    WorkData data1 = {.started = 0, .result = 123};
    WorkData data2 = {.started = 0, .result = 456};
    WorkData data3 = {.started = 0, .result = 789};
    ASSERT(data1.semaphore_start = semaphore_create(0, 1));
    ASSERT(data2.semaphore_start = semaphore_create(0, 1));
    ASSERT(data3.semaphore_start = semaphore_create(0, 1));
    ASSERT(data1.semaphore_finish = semaphore_create(0, 1));
    ASSERT(data2.semaphore_finish = semaphore_create(0, 1));
    ASSERT(data3.semaphore_finish = semaphore_create(0, 1));
    int wu1, wu2, wu3;
    CHECK_TRUE(wu1 = workqueue_submit(wq, work_function, &data1));
    CHECK_TRUE(wu2 = workqueue_submit(wq, work_function, &data2));
    CHECK_TRUE(wu3 = workqueue_submit(wq, work_function, &data3));

    /* The first two work units should have started.  Assume that if the
     * third work unit had started, it would have signalled the start
     * semaphore within 10 milliseconds. */
    semaphore_wait(data1.semaphore_start);
    semaphore_wait(data2.semaphore_start);
    CHECK_FALSE(semaphore_wait_timeout(data3.semaphore_start, 0.01));
    CHECK_TRUE(data1.started);
    CHECK_TRUE(data2.started);
    CHECK_FALSE(data3.started);
    CHECK_FALSE(workqueue_poll(wq, wu1));
    CHECK_FALSE(workqueue_poll(wq, wu2));
    CHECK_FALSE(workqueue_poll(wq, wu3));

    /* Let the second work unit finish, which should allow the third unit
     * to proceed even though the first is still running. */
    semaphore_signal(data2.semaphore_finish);
    CHECK_INTEQUAL(workqueue_wait(wq, wu2), 456);
    semaphore_wait(data3.semaphore_start);
    CHECK_TRUE(data3.started);
    CHECK_TRUE(workqueue_is_busy(wq));

    /* Let the first work unit finish and check that the work queue is
     * still reported as busy (since the third work unit is running). */
    semaphore_signal(data1.semaphore_finish);
    CHECK_INTEQUAL(workqueue_wait(wq, wu1), 123);
    CHECK_TRUE(workqueue_is_busy(wq));

    /* Let the third work unit finish and check that the work queue is no
     * longer reported as busy. */
    semaphore_signal(data3.semaphore_finish);
    CHECK_INTEQUAL(workqueue_wait(wq, wu3), 789);
    for (int i = 0; i < SPIN_COUNT; i++) {
        thread_yield();
    }
    CHECK_FALSE(workqueue_is_busy(wq));

    semaphore_destroy(data1.semaphore_start);
    semaphore_destroy(data2.semaphore_start);
    semaphore_destroy(data3.semaphore_start);
    semaphore_destroy(data1.semaphore_finish);
    semaphore_destroy(data2.semaphore_finish);
    semaphore_destroy(data3.semaphore_finish);
    workqueue_destroy(wq);
    return 1;
}

/*-----------------------------------------------------------------------*/

/* This test checks for a former bug in which the send_idle_signal flag was
 * never cleared after being set by workqueue_wait_all().  Subsequently, if
 * the dispatcher encountered an idle state (such as on completion of a new
 * work unit), it would signal the idle semaphore even without
 * workqueue_wait_all() having been called.  A later workqueue_wait_all()
 * call would then skip over the wait-for-idle step even if additional work
 * units were executing at that time. */
TEST(test_send_idle_signal_cleared)
{
    int wq;
    CHECK_TRUE(wq = workqueue_create(1));

    /* Submit a work unit and wait for it with workqueue_wait_all(), which
     * will set the send_idle_signal flag. */
    CHECK_TRUE(workqueue_submit(wq, empty_work_function, NULL));
    workqueue_wait_all(wq);

    /* Submit another work unit, delay long enough for the dispatcher to
     * receive the completion signal from the worker thread, then wait for
     * the work unit with workqueue_wait().  This will not set the
     * send_idle_signal flag, but if the flag was not cleared by the
     * dispatcher, the idle semaphore will be signalled when the work unit
     * completes. */
    int wu;
    CHECK_TRUE(wu = workqueue_submit(wq, empty_work_function, NULL));
    for (int i = 0; i < SPIN_COUNT; i++) {
        thread_yield();
    }
    CHECK_TRUE(workqueue_wait(wq, wu));

    /* Submit a work unit which takes a long time to complete, and wait for
     * it with workqueue_wait_all().  If the bug is present, the function
     * will return immediately. */
    int test = 0;
    CHECK_TRUE(workqueue_submit(wq, delayed_work_function, &test));
    workqueue_wait_all(wq);

    /* Verify that the work unit has in fact completed (i.e., that
     * workqueue_wait_all() did not return early). */
    CHECK_INTEQUAL(test, 1);

    workqueue_destroy(wq);
    return 1;
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static int work_function(void *data_)
{
    WorkData *data = (WorkData *)data_;
    data->started = 1;
    if (data->semaphore_start) {
        semaphore_signal(data->semaphore_start);
    }
    if (data->semaphore_finish) {
        semaphore_wait(data->semaphore_finish);
    }
    return data->result;
}

/*-----------------------------------------------------------------------*/

static int empty_work_function(UNUSED void *unused)
{
    return 1;
}

/*-----------------------------------------------------------------------*/

static int delayed_work_function(void *data_)
{
    int *data = (int *)data_;
    for (int i = 0; i < SPIN_COUNT; i++) {
        thread_yield();
    }
    (*data)++;
    return 1;
}

/*-----------------------------------------------------------------------*/

static int waiter_thread(void *wq)
{
    return workqueue_wait((int)(intptr_t)wq, 1);
}

/*************************************************************************/
/*************************************************************************/
