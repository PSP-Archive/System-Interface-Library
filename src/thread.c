/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sync.c: Thread and synchronization primitive handling.
 */

#include "src/base.h"
#include "src/condvar.h"
#include "src/mutex.h"
#include "src/semaphore.h"
#include "src/sysdep.h"
#include "src/thread.h"
#include "src/utility/id-array.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Array of created thread objects.  We don't make this THREADSAFE because
 * we do our own mutual exclusion. */
static IDArray threads = ID_ARRAY_INITIALIZER(10);

/* Mutex for accessing the threads[] array.  Needed so that a thread which
 * calls thread_get_id() immediately doesn't race with thread_create()'s
 * update of the array. */
SysMutexID threads_mutex;

/* Arrays of synchronization primitive objects. */
static IDArray condvars = ID_ARRAY_THREADSAFE_INITIALIZER(100);
static IDArray mutexes = ID_ARRAY_THREADSAFE_INITIALIZER(100);
static IDArray semaphores = ID_ARRAY_THREADSAFE_INITIALIZER(100);


/**
 * VALIDATE_THREAD:  Validate the thread ID passed to a thread handling
 * routine, and store the corresponding pointer in the variable "thread".
 * If the thread ID is invalid, the "error_return" statement is executed;
 * this may consist of multiple statements, but must include a "return" to
 * exit the function.
 */
#define VALIDATE_THREAD(id,thread,error_return) \
    ID_ARRAY_VALIDATE(&threads, (id), SysThreadID, thread, \
                      DLOG("Thread ID %d is invalid", _id); error_return)

/**
 * VALIDATE_CONDVAR:  Validate the condition variable ID passed to a
 * condition variable routine, and store the corresponding low-level
 * condition variable ID in the variable "condvar".  If the condition
 * variable ID is invalid, the "error_return" statement is executed; this
 * may consist of multiple statements, but must include a "return" to exit
 * the function.
 */
#define VALIDATE_CONDVAR(id,condvar,error_return) \
    ID_ARRAY_VALIDATE(&condvars, (id), SysCondVarID, condvar, \
                      DLOG("Condition variable ID %d is invalid", _id); \
                      error_return)

/**
 * VALIDATE_MUTEX:  Validate the mutex ID passed to a mutex
 * manipulation routine, and store the corresponding pointer in the
 * variable "mutex".  If the mutex ID is invalid, the "error_return"
 * statement is executed; this may consist of multiple statements, but
 * must include a "return" to exit the function.
 */
#define VALIDATE_MUTEX(id,mutex,error_return) \
    ID_ARRAY_VALIDATE(&mutexes, (id), SysMutexID, mutex, \
                      DLOG("Mutex ID %d is invalid", _id); error_return)

/**
 * VALIDATE_SEMAPHORE:  Validate the semaphore ID passed to a semaphore
 * routine, and store the corresponding low-level semaphore ID in the
 * variable "sem".  If the semaphore ID is invalid, the "error_return"
 * statement is executed; this may consist of multiple statements, but must
 * include a "return" to exit the function.
 */
#define VALIDATE_SEMAPHORE(id,sem,error_return) \
    ID_ARRAY_VALIDATE(&semaphores, (id), SysSemaphoreID, sem, \
                      DLOG("Semaphore ID %d is invalid", _id); error_return)

/*-----------------------------------------------------------------------*/

/* Flags to force thread, condition variable, or semaphore creation failure
 * (for testing purposes). */
static
#ifndef SIL_INCLUDE_TESTS
    const
#endif
    uint8_t TEST_fail_create_thread = 0;
static
#ifndef SIL_INCLUDE_TESTS
    const
#endif
    uint8_t TEST_fail_create_condvar = 0;
#ifndef SIL_INCLUDE_TESTS
    const
#endif
    uint8_t TEST_fail_create_semaphore = 0;

/*************************************************************************/
/********************** Interface: Thread routines ***********************/
/*************************************************************************/

int thread_init(void)
{
    threads_mutex = sys_mutex_create(MUTEX_SIMPLE, MUTEX_UNLOCKED);
    if (!threads_mutex) {
        DLOG("Failed to create mutex for threads array");
        return 0;
    }

    /* Register dummy IDs to get the ID array mutexes created (in order to
     * avoid spurious memory leak errors during tests). */
    id_array_release(&condvars, id_array_register(&condvars, &condvars));
    id_array_release(&mutexes, id_array_register(&mutexes, &mutexes));
    id_array_release(&semaphores, id_array_register(&semaphores, &semaphores));

    return 1;
}

/*-----------------------------------------------------------------------*/

void thread_cleanup(void)
{
    if (threads_mutex) {
        sys_mutex_destroy(threads_mutex);
        threads_mutex = 0;
    }

    id_array_clean(&condvars);
    id_array_clean(&mutexes);
    id_array_clean(&semaphores);
}

/*-----------------------------------------------------------------------*/

int thread_get_num_cores(void)
{
    int num_cores = sys_thread_get_num_cores();
    ASSERT(num_cores >= 1, num_cores = 1);
    return num_cores;
}

/*-----------------------------------------------------------------------*/

int thread_create(ThreadFunction *function, void *param)
{
    if (UNLIKELY(!function)) {
        DLOG("function == NULL");
        return 0;
    }
    return thread_create_with_priority(thread_get_priority(), function, param);
}

/*-----------------------------------------------------------------------*/

int thread_create_with_priority(
    int priority, ThreadFunction *function, void *param)
{
    if (UNLIKELY(!function)) {
        DLOG("function == NULL");
        return 0;
    }
    ThreadAttributes attr = {.priority = priority,
                             .stack_size = 0,
                             .affinity = 0};
    return thread_create_with_attr(&attr, function, param);
}

/*-----------------------------------------------------------------------*/

int thread_create_with_attr(
    const ThreadAttributes *attr, ThreadFunction *function, void *param)
{
    if (UNLIKELY(!attr) || UNLIKELY(!function)) {
        DLOG("Invalid parameters: %p %p %p", attr, function, param);
        return 0;
    }
    if (UNLIKELY(attr->stack_size < 0)) {
        DLOG("Invalid stack size: %d", attr->stack_size);
        return 0;
    }

    if (TEST_fail_create_thread) {
        DLOG("Failing due to TEST_thread_fail_create()");
        return 0;
    }

    sys_mutex_lock(threads_mutex, -1);

    /* We can't easily clean up a thread after creation if the ID array
     * store fails, so allocate an ID first and set its value properly
     * once the thread has been successfully created.  The dummy value
     * is arbitrary (but must be non-NULL as required by the function
     * preconditions). */
    const int id = id_array_register(&threads, &threads);
    if (UNLIKELY(!id)) {
        DLOG("Failed to allocate new ID for thread");
        sys_mutex_unlock(threads_mutex);
        return 0;
    }

    SysThreadID thread = sys_thread_create(attr, function, param);
    if (!thread) {
        id_array_release(&threads, id);
        sys_mutex_unlock(threads_mutex);
        return 0;
    }

    id_array_set(&threads, id, (void *)thread);
    sys_mutex_unlock(threads_mutex);
    return id;
}

/*-----------------------------------------------------------------------*/

void thread_exit(int exit_code)
{
    sys_thread_exit(exit_code);
}

/*-----------------------------------------------------------------------*/

int thread_get_id(void)
{
    const SysThreadID sys_thread = sys_thread_get_id();
    if (sys_thread) {
        sys_mutex_lock(threads_mutex, -1);
        const int id = id_array_find(&threads, (void *)sys_thread);
        sys_mutex_unlock(threads_mutex);
        return id;
    } else {
        return 0;
    }
}

/*-----------------------------------------------------------------------*/

int thread_get_priority(void)
{
    return sys_thread_get_priority();
}

/*-----------------------------------------------------------------------*/

int thread_set_affinity(uint64_t affinity)
{
    const int num_cores = thread_get_num_cores();
    const uint64_t valid_cpu_mask =
        num_cores >= 64 ? ~UINT64_C(0) : (UINT64_C(1) << num_cores) - 1;
    affinity &= valid_cpu_mask;
    if (affinity == 0) {
        affinity = valid_cpu_mask;
    }
    return sys_thread_set_affinity(affinity);
}

/*-----------------------------------------------------------------------*/

uint64_t thread_get_affinity(void)
{
    return sys_thread_get_affinity();
}

/*-----------------------------------------------------------------------*/

int thread_is_running(int thread)
{
    SysThreadID sys_thread;
    sys_mutex_lock(threads_mutex, -1);
    VALIDATE_THREAD(thread, sys_thread, sys_mutex_unlock(threads_mutex); return 0);
    const int result = sys_thread_is_running(sys_thread);
    sys_mutex_unlock(threads_mutex);
    return result;
}

/*-----------------------------------------------------------------------*/

int thread_wait(int thread)
{
    SysThreadID sys_thread;
    sys_mutex_lock(threads_mutex, -1);
    VALIDATE_THREAD(thread, sys_thread, sys_mutex_unlock(threads_mutex); return 0);
    sys_mutex_unlock(threads_mutex);

    int result;
    if (!sys_thread_wait(sys_thread, &result)) {
        return 0;
    }
    sys_mutex_lock(threads_mutex, -1);
    id_array_release(&threads, thread);
    sys_mutex_unlock(threads_mutex);
    return result;
}

/*-----------------------------------------------------------------------*/

int thread_wait2(int thread, int *result_ret)
{
    if (UNLIKELY(!result_ret)) {
        DLOG("result_ret == NULL");
        return 0;
    }
    SysThreadID sys_thread;
    sys_mutex_lock(threads_mutex, -1);
    VALIDATE_THREAD(thread, sys_thread, sys_mutex_unlock(threads_mutex); return 0);
    sys_mutex_unlock(threads_mutex);

    if (!sys_thread_wait(sys_thread, result_ret)) {
        return 0;
    }
    sys_mutex_lock(threads_mutex, -1);
    id_array_release(&threads, thread);
    sys_mutex_unlock(threads_mutex);
    return 1;
}

/*-----------------------------------------------------------------------*/

void thread_yield(void)
{
    sys_thread_yield();
}

/*************************************************************************/
/**************** Interface: Condition variable routines *****************/
/*************************************************************************/

int condvar_create(void)
{
    if (TEST_fail_create_condvar) {
        DLOG("Failing due to TEST_condvar_fail_create()");
        return 0;
    }

    const SysCondVarID condvar = sys_condvar_create();
    if (!condvar) {
        return 0;
    }

    const int id = id_array_register(&condvars, (void *)condvar);
    if (UNLIKELY(!id)) {
        DLOG("Failed to store new condition variable in array");
        sys_condvar_destroy(condvar);
        return 0;
    }

    return id;
}

/*-----------------------------------------------------------------------*/

void condvar_destroy(int condvar)
{
    if (condvar) {
        SysCondVarID sys_condvar;
        VALIDATE_CONDVAR(condvar, sys_condvar, return);
        sys_condvar_destroy(sys_condvar);
        id_array_release(&condvars, condvar);
    }
}

/*-----------------------------------------------------------------------*/

void condvar_wait(int condvar, int mutex)
{
    SysCondVarID sys_condvar;
    SysMutexID sys_mutex;
    VALIDATE_CONDVAR(condvar, sys_condvar, return);
    VALIDATE_MUTEX(mutex, sys_mutex, return);
    sys_condvar_wait(sys_condvar, sys_mutex, -1);
}

/*-----------------------------------------------------------------------*/

int condvar_wait_timeout(int condvar, int mutex, float timeout)
{
    SysCondVarID sys_condvar;
    SysMutexID sys_mutex;
    VALIDATE_CONDVAR(condvar, sys_condvar, return 0);
    VALIDATE_MUTEX(mutex, sys_mutex, return 0);

    /* We check !(timeout >= 0) instead of (timeout < 0) so that NaNs will
     * fail the test. */
    if (UNLIKELY(!(timeout >= 0))) {
        DLOG("Invalid timeout: %g", timeout);
        return 0;
    }

    return sys_condvar_wait(sys_condvar, sys_mutex, timeout);
}

/*-----------------------------------------------------------------------*/

void condvar_signal(int condvar)
{
    SysCondVarID sys_condvar;
    VALIDATE_CONDVAR(condvar, sys_condvar, return);
    sys_condvar_signal(sys_condvar, 0);
}

/*-----------------------------------------------------------------------*/

void condvar_broadcast(int condvar)
{
    SysCondVarID sys_condvar;
    VALIDATE_CONDVAR(condvar, sys_condvar, return);
    sys_condvar_signal(sys_condvar, 1);
}

/*************************************************************************/
/*********************** Interface: Mutex routines ***********************/
/*************************************************************************/

int mutex_create(MutexType type, MutexState initial_state)
{
    if (UNLIKELY(type != MUTEX_SIMPLE && type != MUTEX_RECURSIVE)) {
        DLOG("Invalid type: %d", (int)type);
        return 0;
    }
    if (UNLIKELY(initial_state != MUTEX_LOCKED
                 && initial_state != MUTEX_UNLOCKED)) {
        DLOG("Invalid initial_state: %d", (int)initial_state);
        return 0;
    }

    const SysMutexID mutex = sys_mutex_create((int)type, (int)initial_state);
    if (!mutex) {
        return 0;
    }

    const int id = id_array_register(&mutexes, (void *)mutex);
    if (UNLIKELY(!id)) {
        DLOG("Failed to store new mutex in array");
        sys_mutex_destroy(mutex);
        return 0;
    }

    return id;
}

/*-----------------------------------------------------------------------*/

void mutex_destroy(int mutex)
{
    if (mutex) {
        SysMutexID sys_mutex;
        VALIDATE_MUTEX(mutex, sys_mutex, return);
        sys_mutex_destroy(sys_mutex);
        id_array_release(&mutexes, mutex);
    }
}

/*-----------------------------------------------------------------------*/

void mutex_lock(int mutex)
{
    SysMutexID sys_mutex;
    VALIDATE_MUTEX(mutex, sys_mutex, return);
    sys_mutex_lock(sys_mutex, -1);
}

/*-----------------------------------------------------------------------*/

int mutex_lock_timeout(int mutex, float timeout)
{
    SysMutexID sys_mutex;
    VALIDATE_MUTEX(mutex, sys_mutex, return 0);

    /* We check !(timeout >= 0) instead of (timeout < 0) so that NaNs will
     * fail the test. */
    if (UNLIKELY(!(timeout >= 0))) {
        DLOG("Invalid timeout: %g", timeout);
        return 0;
    }

    return sys_mutex_lock(sys_mutex, timeout);
}

/*-----------------------------------------------------------------------*/

void mutex_unlock(int mutex)
{
    SysMutexID sys_mutex;
    VALIDATE_MUTEX(mutex, sys_mutex, return);
    sys_mutex_unlock(sys_mutex);
}

/*************************************************************************/
/********************* Interface: Semaphore routines *********************/
/*************************************************************************/

int semaphore_create(int initial_value, int required_max)
{
    if (UNLIKELY(initial_value < 0)
     || UNLIKELY(required_max < 1)
     || UNLIKELY(required_max < initial_value)) {
        DLOG("Invalid parameters: %d %d", initial_value, required_max);
        return 0;
    }

    if (TEST_fail_create_semaphore) {
        DLOG("Failing due to TEST_semaphore_fail_create()");
        return 0;
    }

    const SysSemaphoreID semaphore =
        sys_semaphore_create(initial_value, required_max);
    if (!semaphore) {
        return 0;
    }

    const int id = id_array_register(&semaphores, (void *)semaphore);
    if (UNLIKELY(!id)) {
        DLOG("Failed to store new semaphore in array");
        sys_semaphore_destroy(semaphore);
        return 0;
    }

    return id;
}

/*-----------------------------------------------------------------------*/

void semaphore_destroy(int semaphore)
{
    if (semaphore) {
        SysSemaphoreID sem;
        VALIDATE_SEMAPHORE(semaphore, sem, return);
        sys_semaphore_destroy(sem);
        id_array_release(&semaphores, semaphore);
    }
}

/*-----------------------------------------------------------------------*/

void semaphore_wait(int semaphore)
{
    SysSemaphoreID sem;
    VALIDATE_SEMAPHORE(semaphore, sem, return);
    sys_semaphore_wait(sem, -1);
}

/*-----------------------------------------------------------------------*/

int semaphore_wait_timeout(int semaphore, float timeout)
{
    SysSemaphoreID sem;
    VALIDATE_SEMAPHORE(semaphore, sem, return 0);

    /* We check !(timeout >= 0) instead of (timeout < 0) so that NaNs will
     * fail the test. */
    if (UNLIKELY(!(timeout >= 0))) {
        DLOG("Invalid timeout: %g", timeout);
        return 0;
    }

    return sys_semaphore_wait(sem, timeout);
}

/*-----------------------------------------------------------------------*/

void semaphore_signal(int semaphore)
{
    SysSemaphoreID sem;
    VALIDATE_SEMAPHORE(semaphore, sem, return);
    sys_semaphore_signal(sem);
}

/*-----------------------------------------------------------------------*/

int semaphore_max_value(void)
{
    return sys_semaphore_max_value();
}

/*************************************************************************/
/************************* Test control routines *************************/
/*************************************************************************/

#ifdef SIL_INCLUDE_TESTS

/*-----------------------------------------------------------------------*/

void TEST_thread_fail_create(int fail)
{
    TEST_fail_create_thread = (fail != 0);
}

/*-----------------------------------------------------------------------*/

void TEST_condvar_fail_create(int fail)
{
    TEST_fail_create_condvar = (fail != 0);
}

/*-----------------------------------------------------------------------*/

void TEST_semaphore_fail_create(int fail)
{
    TEST_fail_create_semaphore = (fail != 0);
}

/*-----------------------------------------------------------------------*/

#endif  // SIL_INCLUDE_TESTS

/*************************************************************************/
/*************************************************************************/
