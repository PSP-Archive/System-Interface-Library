/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/posix/thread.c: Tests for the POSIX implementation of
 * the system-level thread handling functions.
 */

#include "src/base.h"
#include "src/semaphore.h"
#include "src/sysdep.h"
#include "src/sysdep/posix/thread.h"
#include "src/test/base.h"
#include "src/thread.h"

#include <pthread.h>
/* glibc hides this behind _GNU_SOURCE even though it's standard on Linux: */
#ifdef __linux__
extern int pthread_getname_np(pthread_t thread, const char *name, size_t len);
#endif

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

/**
 * priority_check_thread:  Simple thread routine which returns its own
 * POSIX priority value.
 *
 * [Parameters]
 *     unused: Opaque thread parameter (unused).
 * [Return value]
 *     Thread's POSIX priority value.
 */
static int priority_check_thread(UNUSED void *unused)
{
    int policy;
    struct sched_param sched_param;
    ASSERT(pthread_getschedparam(pthread_self(), &policy, &sched_param) == 0);
    return sched_param.sched_priority;
}

/*-----------------------------------------------------------------------*/

/**
 * name_check_thread:  Simple thread routine which checks its name.
 *
 * [Parameters]
 *     expected_name: String containing the expected thread name.
 * [Return value]
 *     True if the thread name was correct, false if not.
 */
static int name_check_thread(void *expected_name_)
{
    const char *expected_name = expected_name_;
    PRECOND(expected_name != NULL, return 0);
    PRECOND(strlen(expected_name) <= 15, return 0);

    char name[16];
    /* This is technically a "nonportable" (_np) call, but it's implemented
     * on all POSIX platforms we currently support (Linux and Darwin). */
    CHECK_INTEQUAL(pthread_getname_np(pthread_self(), name, sizeof(name)), 0);
    CHECK_STREQUAL(name, expected_name);

    return 1;
}

/*-----------------------------------------------------------------------*/

/**
 * detached_thread:  Simple thread routine, intended to run as a detached
 * thread, which signals the semaphore passed in and then exits.
 *
 * [Parameters]
 *     semaphore: ID of semaphore to signal.
 */
static void detached_thread(void *semaphore)
{
    semaphore_signal((int)(intptr_t)semaphore);
    thread_exit(0);
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_posix_thread)

/*-----------------------------------------------------------------------*/

TEST_INIT(init)
{
    CHECK_TRUE(thread_init());
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST_CLEANUP(cleanup)
{
    thread_cleanup();
    return 1;
}

/*************************************************************************/
/***************************** Test routines *****************************/
/*************************************************************************/

TEST(test_priority_negative_bound)
{
    int policy;
    struct sched_param sched_param;
    ASSERT(pthread_getschedparam(pthread_self(), &policy, &sched_param) == 0);
    const int base_priority = sched_param.sched_priority;

    int thread;
    CHECK_TRUE(thread = thread_create_with_priority(
                   -999999999, priority_check_thread, NULL));
    const int thread_priority = thread_wait(thread);
    if (!(thread_priority <= base_priority)) {
        FAIL("thread_priority (%d) >= base_priority (%d) was not true as"
             " expected", thread_priority, base_priority);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_priority_positive_bound)
{
    int policy;
    struct sched_param sched_param;
    ASSERT(pthread_getschedparam(pthread_self(), &policy, &sched_param) == 0);
    const int base_priority = sched_param.sched_priority;

    int thread;
    CHECK_TRUE(thread = thread_create_with_priority(
                   999999999, priority_check_thread, NULL));
    const int thread_priority = thread_wait(thread);
    if (!(thread_priority >= base_priority)) {
        FAIL("thread_priority (%d) >= base_priority (%d) was not true as"
             " expected", thread_priority, base_priority);
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_thread_name)
{
    static const ThreadAttributes attr = {.name = "ThreadNameTest"};
    const int thread =
        thread_create_with_attr(&attr, name_check_thread, (void *)attr.name);
    CHECK_TRUE(thread_wait(thread));

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_create_detached)
{
    int semaphore;
    ASSERT(semaphore = semaphore_create(0, 1));

    CHECK_TRUE(posix_thread_create_detached(
                   detached_thread, (void *)(intptr_t)semaphore));
    semaphore_wait(semaphore);

    semaphore_destroy(semaphore);
    return 1;
}

/*************************************************************************/
/*************************************************************************/
