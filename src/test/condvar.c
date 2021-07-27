/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/condvar.c: Tests for the condition variable functions.
 */

#include "src/base.h"
#include "src/condvar.h"
#include "src/memory.h"
#include "src/mutex.h"
#include "src/semaphore.h"
#include "src/test/base.h"
#include "src/thread.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Condition variable and mutex IDs guaranteed to be invalid across all
 * tests. */
#define INVALID_CONDVAR  10000
#define INVALID_MUTEX    10000

/* Data structure passed to condvar_wait_thread(). */
typedef struct CondVarThreadData CondVarThreadData;
struct CondVarThreadData {
    int condvar;    // Condition variable to wait on.
    int mutex;      // Mutex for waiting.
    int start_sem;  // Semaphore to signal after initially locking the mutex.
    float timeout;  // Wait timeout, or 0 to wait indefinitely.
    int counter;    // Incremented after initial lock of the mutex.
};

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * condvar_wait_thread:  Thread routine that simply waits on a condition
 * variable and returns.
 *
 * [Parameters]
 *     data: Thread data pointer (CondVarThreadData *).
 * [Return value]
 *     False if the timeout was nonzero and condvar_wait_timeout()
 *     returned false; true otherwise.
 */
static int condvar_wait_thread(void *data);

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_condvar)

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

TEST(test_condvar_create)
{
    int condvar;

    CHECK_TRUE(condvar = condvar_create());
    condvar_destroy(condvar);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_condvar_create_memory_failure)
{
    int condvar;

    /* Make sure the condition variable ID array is empty before running
     * this test. */
    thread_cleanup();

    CHECK_MEMORY_FAILURES(
        (condvar = condvar_create()) != 0
        || (thread_init(), thread_cleanup(), 0));
    condvar_destroy(condvar);

    ASSERT(thread_init());
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_condvar_create_forced_failure)
{
    TEST_condvar_fail_create(1);

    CHECK_FALSE(condvar_create());

    TEST_condvar_fail_create(0);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_condvar_destroy_null)
{
    condvar_destroy(0);  // Just make sure it doesn't crash.
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_condvar_destroy_invalid)
{
    int condvar;
    CHECK_TRUE(condvar = condvar_create());
    condvar_destroy(condvar);

    /* Just make sure these don't crash. */
    condvar_destroy(condvar);
    condvar_destroy(INVALID_CONDVAR);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_condvar_wait)
{
    int condvar, mutex, sem, thread;
    CHECK_TRUE(condvar = condvar_create());
    CHECK_TRUE(mutex = mutex_create(MUTEX_SIMPLE, MUTEX_LOCKED));
    CHECK_TRUE(sem = semaphore_create(0, 1));

    CondVarThreadData data =
        {.condvar = condvar, .mutex = mutex, .start_sem = sem,
         .timeout = 0, .counter = 0};
    CHECK_TRUE(thread = thread_create(condvar_wait_thread, &data));
    for (int i = 0; i < 1000; i++) {
        thread_yield();
    }
    CHECK_TRUE(thread_is_running(thread));

    /* Unlocking the mutex should not by itself allow the thread to
     * proceed past the wait. */
    mutex_unlock(mutex);
    semaphore_wait(sem);

    /* We should be able to lock the mutex while the thread is waiting in
     * condvar_wait(). */
    mutex_lock(mutex);
    CHECK_INTEQUAL(data.counter, 1);

    /* Signal the condition variable and allow the thread to complete.  If
     * condvar_wait() fails to resume on a signal, the thread_wait() call
     * will block forever. */
    condvar_signal(condvar);
    mutex_unlock(mutex);
    CHECK_TRUE(thread_wait(thread));

    condvar_destroy(condvar);
    mutex_destroy(mutex);
    semaphore_destroy(sem);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_condvar_wait_invalid)
{
    int condvar, mutex;
    CHECK_TRUE(condvar = condvar_create());

    /* Just make sure these don't crash. */
    CHECK_TRUE(mutex = mutex_create(MUTEX_SIMPLE, MUTEX_LOCKED));
    mutex_destroy(mutex);
    condvar_wait(condvar, 0);
    condvar_wait(condvar, mutex);
    condvar_wait(condvar, INVALID_MUTEX);

    condvar_destroy(condvar);
    CHECK_TRUE(mutex = mutex_create(MUTEX_SIMPLE, MUTEX_LOCKED));
    condvar_wait(0, mutex);
    condvar_wait(condvar, mutex);
    condvar_wait(INVALID_CONDVAR, mutex);

    mutex_destroy(mutex);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_condvar_wait_timeout)
{
    int condvar, mutex, sem, thread;
    CHECK_TRUE(condvar = condvar_create());
    CHECK_TRUE(mutex = mutex_create(MUTEX_SIMPLE, MUTEX_LOCKED));
    CHECK_TRUE(sem = semaphore_create(0, 1));

    /* Attempting to wait on the condition variable with nobody to signal
     * us should fail. */
    CHECK_FALSE(condvar_wait_timeout(condvar, mutex, 0));
    CHECK_FALSE(condvar_wait_timeout(condvar, mutex, 0.01));

    CondVarThreadData data =
        {.condvar = condvar, .mutex = mutex, .start_sem = sem,
         .timeout = 999, .counter = 0};
    CHECK_TRUE(thread = thread_create(condvar_wait_thread, &data));
    for (int i = 0; i < 1000; i++) {
        thread_yield();
    }
    CHECK_TRUE(thread_is_running(thread));

    mutex_unlock(mutex);
    semaphore_wait(sem);

    mutex_lock(mutex);
    CHECK_INTEQUAL(data.counter, 1);

    condvar_signal(condvar);
    mutex_unlock(mutex);
    CHECK_TRUE(thread_wait(thread));

    condvar_destroy(condvar);
    mutex_destroy(mutex);
    semaphore_destroy(sem);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_condvar_wait_timeout_invalid)
{
    int condvar, mutex;
    CHECK_TRUE(condvar = condvar_create());

    CHECK_TRUE(mutex = mutex_create(MUTEX_SIMPLE, MUTEX_LOCKED));
    CHECK_FALSE(condvar_wait_timeout(condvar, mutex, -1));
    CHECK_FALSE(condvar_wait_timeout(condvar, mutex, FLOAT_NAN()));

    mutex_destroy(mutex);
    CHECK_FALSE(condvar_wait_timeout(condvar, 0, 0));
    CHECK_FALSE(condvar_wait_timeout(condvar, mutex, 0));
    CHECK_FALSE(condvar_wait_timeout(condvar, INVALID_MUTEX, 0));

    condvar_destroy(condvar);
    CHECK_TRUE(mutex = mutex_create(MUTEX_SIMPLE, MUTEX_LOCKED));
    CHECK_FALSE(condvar_wait_timeout(0, mutex, 0));
    CHECK_FALSE(condvar_wait_timeout(condvar, mutex, 0));
    CHECK_FALSE(condvar_wait_timeout(INVALID_CONDVAR, mutex, 0));

    mutex_destroy(mutex);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_condvar_signal_no_waiters)
{
    int condvar, mutex, sem, thread;
    CHECK_TRUE(condvar = condvar_create());
    CHECK_TRUE(mutex = mutex_create(MUTEX_SIMPLE, MUTEX_LOCKED));
    CHECK_TRUE(sem = semaphore_create(0, 1));

    CondVarThreadData data =
        {.condvar = condvar, .mutex = mutex, .start_sem = sem,
         .timeout = 0, .counter = 0};
    CHECK_TRUE(thread = thread_create(condvar_wait_thread, &data));
    for (int i = 0; i < 1000; i++) {
        thread_yield();
    }
    CHECK_TRUE(thread_is_running(thread));

    /* Signaling the condition variable before any threads are waiting on
     * it should have no effect. */
    condvar_signal(condvar);

    /* The signal should not be seen by the thread's wait call. */
    mutex_unlock(mutex);
    semaphore_wait(sem);
    for (int i = 0; i < 1000; i++) {
        thread_yield();
    }
    CHECK_TRUE(thread_is_running(thread));

    mutex_lock(mutex);
    condvar_signal(condvar);
    mutex_unlock(mutex);
    CHECK_TRUE(thread_wait(thread));

    condvar_destroy(condvar);
    mutex_destroy(mutex);
    semaphore_destroy(sem);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_condvar_signal_invalid)
{
    int condvar;
    CHECK_TRUE(condvar = condvar_create());
    condvar_destroy(condvar);

    /* Just make sure these don't crash. */
    condvar_signal(0);
    condvar_signal(condvar);
    condvar_signal(INVALID_CONDVAR);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_condvar_broadcast)
{
    int condvar, mutex, thread1, thread2;
    CHECK_TRUE(condvar = condvar_create());
    CHECK_TRUE(mutex = mutex_create(MUTEX_SIMPLE, MUTEX_UNLOCKED));

    CondVarThreadData data =
        {.condvar = condvar, .mutex = mutex, .start_sem = 0,
         .timeout = 0, .counter = 0};
    CHECK_TRUE(thread1 = thread_create(condvar_wait_thread, &data));
    CHECK_TRUE(thread2 = thread_create(condvar_wait_thread, &data));

    mutex_lock(mutex);
    while (data.counter != 2) {
        mutex_unlock(mutex);
        thread_yield();
        mutex_lock(mutex);
    }
    condvar_broadcast(condvar);
    mutex_unlock(mutex);
    CHECK_TRUE(thread_wait(thread1));
    CHECK_TRUE(thread_wait(thread2));

    condvar_destroy(condvar);
    mutex_destroy(mutex);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_condvar_broadcast_no_waiters)
{
    int condvar, mutex, thread1, thread2;
    CHECK_TRUE(condvar = condvar_create());
    CHECK_TRUE(mutex = mutex_create(MUTEX_SIMPLE, MUTEX_LOCKED));

    /* This broadcast should not be seen by the threads. */
    condvar_broadcast(condvar);
    mutex_unlock(mutex);

    CondVarThreadData data =
        {.condvar = condvar, .mutex = mutex, .start_sem = 0,
         .timeout = 0, .counter = 0};
    CHECK_TRUE(thread1 = thread_create(condvar_wait_thread, &data));
    CHECK_TRUE(thread2 = thread_create(condvar_wait_thread, &data));

    mutex_lock(mutex);
    while (data.counter != 2) {
        mutex_unlock(mutex);
        thread_yield();
        mutex_lock(mutex);
    }
    condvar_broadcast(condvar);
    mutex_unlock(mutex);
    CHECK_TRUE(thread_wait(thread1));
    CHECK_TRUE(thread_wait(thread2));

    condvar_destroy(condvar);
    mutex_destroy(mutex);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_condvar_broadcast_invalid)
{
    int condvar;
    CHECK_TRUE(condvar = condvar_create());
    condvar_destroy(condvar);

    /* Just make sure these don't crash. */
    condvar_broadcast(0);
    condvar_broadcast(condvar);
    condvar_broadcast(INVALID_CONDVAR);

    return 1;
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static int condvar_wait_thread(void *data_)
{
    CondVarThreadData *data = data_;

    mutex_lock(data->mutex);
    data->counter++;
    if (data->start_sem) {
        semaphore_signal(data->start_sem);
    }

    int result;
    if (data->timeout > 0) {
        result =
            condvar_wait_timeout(data->condvar, data->mutex, data->timeout);
    } else {
        condvar_wait(data->condvar, data->mutex);
        result = 1;
    }

    mutex_unlock(data->mutex);
    return result;
}

/*************************************************************************/
/*************************************************************************/
