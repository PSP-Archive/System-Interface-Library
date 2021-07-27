/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/mutex.c: Tests for the mutex functions.
 */

#include "src/base.h"
#include "src/memory.h"
#include "src/mutex.h"
#include "src/test/base.h"
#include "src/thread.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Mutex ID guaranteed to be invalid across all tests. */
#define INVALID_MUTEX  10000

/* Number of iterations to spin on thread_yield() while waiting for the
 * mutex thread. */
#define SPIN_COUNT  10000

/*-----------------------------------------------------------------------*/

/* Data structure passed to mutex_thread(). */
typedef struct MutexData MutexData;
struct MutexData {
    int mutex;
    uint32_t counter;  // O: Incremented each loop while lock is held.
    float timeout;     // I: >0 to use a timeout on the lock call.
    uint8_t hold_lock; // I: Set on entry to hold lock until clear.
    uint8_t stop;      // I: Set to make the thread exit.
    uint8_t start_ok;  // O: Set by the thread when it starts up.
    uint8_t lock_ok;   // O: Set by the thread on the first lock.
};

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * mutex_thread:  Thread routine that counts endlessly until its
 * parameter block's stop flag is set, locking the mutex around each
 * counter increment, then returns the final count.
 *
 * [Parameters]
 *     param: Pointer to parameter block (MutexData).
 * [Return value]
 *     Number of iterations executed (== MutexData.counter).
 */
static int mutex_thread(void *param);

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_mutex)

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

TEST(test_mutex_basic)
{
    int mutex;

    /* Make sure a mutex can be created, locked, and unlocked. */
    CHECK_TRUE(mutex = mutex_create(MUTEX_SIMPLE, MUTEX_UNLOCKED));
    /* Lock and unlock don't return values, so just check that they return
     * properly (instead of blocking). */
    mutex_lock(mutex);
    mutex_unlock(mutex);
    mutex_destroy(mutex);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mutex_create_memory_failure)
{
    int mutex;

    /* Make sure the semaphore ID array is empty before running this test. */
    thread_cleanup();

    CHECK_MEMORY_FAILURES(
        (mutex = mutex_create(MUTEX_SIMPLE, MUTEX_UNLOCKED)) != 0
        || (thread_init(), thread_cleanup(), 0));
    mutex_lock(mutex);
    mutex_unlock(mutex);
    mutex_destroy(mutex);

    ASSERT(thread_init());
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mutex_create_invalid)
{
    CHECK_FALSE(mutex_create(2, MUTEX_UNLOCKED));
    CHECK_FALSE(mutex_create(MUTEX_SIMPLE, 2));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mutex_destroy_null)
{
    mutex_destroy(0);  // Just make sure it doesn't crash.
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mutex_destroy_invalid)
{
    int mutex;
    CHECK_TRUE(mutex = mutex_create(MUTEX_SIMPLE, MUTEX_UNLOCKED));
    mutex_destroy(mutex);

    /* Just make sure these don't crash. */
    mutex_destroy(mutex);
    mutex_destroy(INVALID_MUTEX);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mutex_lock_invalid)
{
    int mutex;
    CHECK_TRUE(mutex = mutex_create(MUTEX_SIMPLE, MUTEX_UNLOCKED));
    mutex_destroy(mutex);

    /* Just make sure these don't crash. */
    mutex_lock(0);
    mutex_lock(mutex);
    mutex_lock(INVALID_MUTEX);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mutex_lock_timeout_invalid)
{
    int mutex;
    CHECK_TRUE(mutex = mutex_create(MUTEX_SIMPLE, MUTEX_UNLOCKED));
    CHECK_FALSE(mutex_lock_timeout(mutex, -1));
    CHECK_FALSE(mutex_lock_timeout(mutex, FLOAT_NAN()));
    mutex_destroy(mutex);

    CHECK_FALSE(mutex_lock_timeout(0, 0));
    CHECK_FALSE(mutex_lock_timeout(mutex, 0));
    CHECK_FALSE(mutex_lock_timeout(INVALID_MUTEX, 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mutex_unlock_invalid)
{
    int mutex;
    CHECK_TRUE(mutex = mutex_create(MUTEX_SIMPLE, MUTEX_UNLOCKED));
    mutex_destroy(mutex);

    /* Just make sure these don't crash. */
    mutex_unlock(0);
    mutex_unlock(mutex);
    mutex_unlock(INVALID_MUTEX);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mutex_multithread)
{
    int thread;
    MutexData mutex_data;
    uint32_t saved_counter;

    mem_clear(&mutex_data, sizeof(mutex_data));
    mutex_data.mutex = mutex_create(MUTEX_SIMPLE, MUTEX_UNLOCKED);
    if (!mutex_data.mutex) {
        FAIL("Could not create mutex");
    }
    thread = thread_create(mutex_thread, &mutex_data);
    if (!thread) {
        FAIL("Could not create mutex testing thread");
    }
    do {
        thread_yield();
    } while (mutex_data.counter == 0);
    mutex_lock(mutex_data.mutex);
    saved_counter = mutex_data.counter;
    for (int i = 0; i < SPIN_COUNT; i++) {
        thread_yield();
    }
    if (mutex_data.counter != saved_counter) {
        mutex_data.stop = 1;
        thread_wait(thread);
        FAIL("Mutex did not block other thread");
    }
    mutex_data.stop = 1;
    mutex_unlock(mutex_data.mutex);
    /* If mutex_unlock() does not work correctly, this call will
     * never return. */
    thread_wait(thread);
    mutex_destroy(mutex_data.mutex);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mutex_multithread_recursive)
{
    int thread;
    MutexData mutex_data;
    uint32_t saved_counter;

    mem_clear(&mutex_data, sizeof(mutex_data));
    mutex_data.mutex = mutex_create(MUTEX_RECURSIVE, MUTEX_UNLOCKED);
    if (!mutex_data.mutex) {
        FAIL("Could not create mutex");
    }
    thread = thread_create(mutex_thread, &mutex_data);
    if (!thread) {
        FAIL("Could not create mutex testing thread");
    }
    do {
        thread_yield();
    } while (mutex_data.counter == 0);
    mutex_lock(mutex_data.mutex);
    mutex_lock(mutex_data.mutex);
    mutex_unlock(mutex_data.mutex);
    saved_counter = mutex_data.counter;
    for (int i = 0; i < SPIN_COUNT; i++) {
        thread_yield();
    }
    if (mutex_data.counter != saved_counter) {
        mutex_data.stop = 1;
        thread_wait(thread);
        FAIL("Mutex did not block other thread");
    }
    mutex_data.stop = 1;
    mutex_unlock(mutex_data.mutex);
    /* If mutex_unlock() does not work correctly, this call will
     * never return. */
    thread_wait(thread);
    mutex_destroy(mutex_data.mutex);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mutex_create_locked)
{
    int thread;
    MutexData mutex_data;

    mem_clear(&mutex_data, sizeof(mutex_data));
    mutex_data.mutex = mutex_create(MUTEX_SIMPLE, MUTEX_LOCKED);
    if (!mutex_data.mutex) {
        FAIL("Could not create mutex");
    }
    thread = thread_create(mutex_thread, &mutex_data);
    if (!thread) {
        FAIL("Could not create mutex testing thread");
    }
    do {
        thread_yield();
    } while (!mutex_data.start_ok);
    for (int i = 0; i < SPIN_COUNT; i++) {
        thread_yield();
    }
    if (mutex_data.lock_ok) {
        mutex_data.stop = 1;
        thread_wait(thread);
        FAIL("Mutex did not start locked");
    }
    mutex_data.stop = 1;
    mutex_unlock(mutex_data.mutex);
    thread_wait(thread);
    mutex_destroy(mutex_data.mutex);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_mutex_timeout)
{
    int thread, thread2;
    MutexData mutex_data, mutex_data2;

    mem_clear(&mutex_data, sizeof(mutex_data));
    mutex_data.mutex = mutex_create(MUTEX_SIMPLE, MUTEX_UNLOCKED);
    if (!mutex_data.mutex) {
        FAIL("Could not create mutex");
    }
    mem_clear(&mutex_data2, sizeof(mutex_data2));
    mutex_data2.mutex = mutex_create(MUTEX_SIMPLE, MUTEX_LOCKED);
    if (!mutex_data2.mutex) {
        FAIL("Could not create mutex");
    }

    mutex_data.hold_lock = 1;
    thread = thread_create(mutex_thread, &mutex_data);
    if (!thread) {
        FAIL("Could not create mutex testing thread 1");
    }
    mutex_data2.timeout = 1.0;
    thread2 = thread_create(mutex_thread, &mutex_data2);
    if (!thread2) {
        FAIL("Could not create mutex testing thread 2");
    }

    mutex_unlock(mutex_data2.mutex);
    do {
        thread_yield();
    } while (!mutex_data.lock_ok);
    if (mutex_lock_timeout(mutex_data.mutex, 0.01)) {
        FAIL("Mutex thread 1 did not lock mutex");
    }
    mutex_data.hold_lock = 0;
    mutex_data.stop = 1;
    thread_wait(thread);
    if (!mutex_lock_timeout(mutex_data.mutex, 0)) {
        FAIL("Mutex thread 1 did not unlock mutex");
    }

    do {
        thread_yield();
    } while (thread_is_running(thread2) && !mutex_data2.lock_ok);
    if (!mutex_data2.lock_ok) {
        FAIL("Mutex thread 2 failed to lock mutex with timeout");
    }
    mutex_data2.stop = 1;
    thread_wait(thread2);
    if (!mutex_lock_timeout(mutex_data2.mutex, 0)) {
        FAIL("Mutex thread 2 did not unlock mutex");
    }

    mutex_destroy(mutex_data.mutex);
    mutex_destroy(mutex_data2.mutex);
    return 1;
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static int mutex_thread(void *param)
{
    MutexData *data = (MutexData *)param;
    data->start_ok = 1;
    if (data->timeout > 0) {
        if (!mutex_lock_timeout(data->mutex, data->timeout)) {
            return 0;
        }
    } else {
        mutex_lock(data->mutex);
    }
    data->lock_ok = 1;
    while (data->hold_lock) {
        thread_yield();
    }
    mutex_unlock(data->mutex);
    while (!data->stop) {
        mutex_lock(data->mutex);
        data->counter++;
        thread_yield();
        mutex_unlock(data->mutex);
    }
    return (int)data->counter;
}

/*************************************************************************/
/*************************************************************************/
