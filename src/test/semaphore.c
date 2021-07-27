/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/semaphore.c: Tests for the semaphore functions.
 */

#include "src/base.h"
#include "src/memory.h"
#include "src/semaphore.h"
#include "src/test/base.h"
#include "src/thread.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Semaphore ID guaranteed to be invalid across all tests. */
#define INVALID_SEMAPHORE  10000

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_semaphore)

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

TEST(test_semaphore_basic)
{
    int semaphore;

    /* Make sure a semaphore can be created, waited for, and signalled. */
    CHECK_TRUE(semaphore = semaphore_create(1, 1));

    /* These functions don't return values, so just check that they return
     * properly (instead of blocking). */
    semaphore_wait(semaphore);
    semaphore_signal(semaphore);
    /* Do another iteration to make sure semaphore_signal() worked. */
    semaphore_wait(semaphore);
    semaphore_signal(semaphore);

    semaphore_destroy(semaphore);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_semaphore_create_memory_failure)
{
    int semaphore;

    /* Make sure the semaphore ID array is empty before running this test. */
    thread_cleanup();

    CHECK_MEMORY_FAILURES(
        (semaphore = semaphore_create(1, 1)) != 0
        || (thread_init(), thread_cleanup(), 0));
    semaphore_wait(semaphore);
    semaphore_signal(semaphore);
    semaphore_destroy(semaphore);

    ASSERT(thread_init());
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_semaphore_create_invalid)
{
    CHECK_FALSE(semaphore_create(-1, 1));
    CHECK_FALSE(semaphore_create(0, -1));
    CHECK_FALSE(semaphore_create(0, 0));
    CHECK_FALSE(semaphore_create(2, 1));
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_semaphore_create_forced_failure)
{
    TEST_semaphore_fail_create(1);

    CHECK_FALSE(semaphore_create(1, 1));

    TEST_semaphore_fail_create(0);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_semaphore_destroy_null)
{
    semaphore_destroy(0);  // Just make sure it doesn't crash.
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_semaphore_destroy_invalid)
{
    int semaphore;
    CHECK_TRUE(semaphore = semaphore_create(1, 1));
    semaphore_destroy(semaphore);

    /* Just make sure these don't crash. */
    semaphore_destroy(semaphore);
    semaphore_destroy(INVALID_SEMAPHORE);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_semaphore_wait_invalid)
{
    int semaphore;
    CHECK_TRUE(semaphore = semaphore_create(1, 1));
    semaphore_destroy(semaphore);

    /* Just make sure these don't crash. */
    semaphore_wait(0);
    semaphore_wait(semaphore);
    semaphore_wait(INVALID_SEMAPHORE);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_semaphore_wait_timeout)
{
    int semaphore;
    CHECK_TRUE(semaphore = semaphore_create(1, 1));

    /* We should be able to get the semaphore immediately on the first
     * iteration. */
    CHECK_TRUE(semaphore_wait_timeout(semaphore, 999));
    /* We should no longer be able to get the semaphore, whether we wait
     * or not. */
    CHECK_FALSE(semaphore_wait_timeout(semaphore, 0));
    CHECK_FALSE(semaphore_wait_timeout(semaphore, 0.01));

    semaphore_destroy(semaphore);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_semaphore_wait_timeout_invalid)
{
    int semaphore;
    CHECK_TRUE(semaphore = semaphore_create(1, 1));
    CHECK_FALSE(semaphore_wait_timeout(semaphore, -1));
    CHECK_FALSE(semaphore_wait_timeout(semaphore, FLOAT_NAN()));
    semaphore_destroy(semaphore);

    CHECK_FALSE(semaphore_wait_timeout(0, 0));
    CHECK_FALSE(semaphore_wait_timeout(semaphore, 0));
    CHECK_FALSE(semaphore_wait_timeout(INVALID_SEMAPHORE, 0));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_semaphore_signal_invalid)
{
    int semaphore;
    CHECK_TRUE(semaphore = semaphore_create(1, 1));
    semaphore_destroy(semaphore);

    /* Just make sure these don't crash. */
    semaphore_signal(0);
    semaphore_signal(semaphore);
    semaphore_signal(INVALID_SEMAPHORE);

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_semaphore_create_value_zero)
{
    int semaphore;
    CHECK_TRUE(semaphore = semaphore_create(0, 1));

    /* We should not be able to get the semaphore yet. */
    CHECK_FALSE(semaphore_wait_timeout(semaphore, 0));

    /* Signaling the semaphore should allow us to get it. */
    semaphore_signal(semaphore);
    CHECK_TRUE(semaphore_wait_timeout(semaphore, 0));

    semaphore_destroy(semaphore);
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_semaphore_create_value_greater_than_one)
{
    int semaphore;
    CHECK_TRUE(semaphore = semaphore_create(2, 3));

    /* We should be able to get the semaphore exactly twice. */
    CHECK_TRUE(semaphore_wait_timeout(semaphore, 0));
    CHECK_TRUE(semaphore_wait_timeout(semaphore, 0));
    CHECK_FALSE(semaphore_wait_timeout(semaphore, 0));

    /* Signaling once should only allow us to get it once more. */
    semaphore_signal(semaphore);
    CHECK_TRUE(semaphore_wait_timeout(semaphore, 0));
    CHECK_FALSE(semaphore_wait_timeout(semaphore, 0));

    /* We should be able to signal up to the required_max value, even if
     * that's greater than the initial value. */
    semaphore_signal(semaphore);
    semaphore_signal(semaphore);
    semaphore_signal(semaphore);
    CHECK_TRUE(semaphore_wait_timeout(semaphore, 0));
    CHECK_TRUE(semaphore_wait_timeout(semaphore, 0));
    CHECK_TRUE(semaphore_wait_timeout(semaphore, 0));
    CHECK_FALSE(semaphore_wait_timeout(semaphore, 0));

    semaphore_destroy(semaphore);
    return 1;
}

/*************************************************************************/
/*************************************************************************/
