/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/posix/thread.c: Thread management for POSIX-compatible systems.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/thread.h"

/* Android hides a bunch of scheduling stuff behind _GNU_SOURCE/__USE_GNU.
 * Note that we have to include this before <pthread.h> in order to hack
 * around the bogosity. */
#ifdef SIL_PLATFORM_ANDROID
# undef _GNU_SOURCE
# define _GNU_SOURCE 1
# undef __USE_GNU
# define __USE_GNU 1
#endif
#include <sched.h>
#ifdef SIL_PLATFORM_ANDROID
# undef _GNU_SOURCE
# undef __USE_GNU
#endif

#include "src/sysdep/posix/thread.h"

#include <pthread.h>
/* glibc hides this behind _GNU_SOURCE even though it's standard on Linux: */
#ifdef __linux__
extern int pthread_setname_np(pthread_t thread, const char *name);
#endif

#include <unistd.h>

#ifdef __linux__  // Any Linux-based platform (including Android).
# include <sys/resource.h>
/* glibc hides these behind _GNU_SOURCE even though they're standard Linux
 * system calls: */
extern int sched_setaffinity(pid_t, size_t, const cpu_set_t *);
extern int sched_getaffinity(pid_t, size_t, cpu_set_t *);
#endif

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Priority of the initial thread, for adjusting priorities in
 * sys_thread_create(). */
static int base_priority;

/* Local storage key for storing the current thread's SysThread pointer
 * (so we can return it from sys_thread_get_id()). */
static pthread_key_t sys_thread_key;

/* Control object for creating the local storage key. */
static pthread_once_t key_once = PTHREAD_ONCE_INIT;

/* Flag indicating whether the key was successfully created (since we can't
 * return a value from the creation function). */
static uint8_t sys_thread_key_created = 0;

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * posix_thread_init:  Save the current thread's priority, and create the
 * local storage key for storing the current thread's SysThread pointer.
 */
static void posix_thread_init(void);

/**
 * get_raw_priority:  Retrieve the raw system-level priority value for the
 * current thread.  Wraps the difference between Linux and standard POSIX
 * priority handling (excluding the inverted priority sign).
 *
 * [Parameters]
 *     priority_ret: Pointer to variable to receive the current thread's
 *         raw priority value.
 * [Return value]
 *     True on success, false on error.
 */
static int get_raw_priority(int *priority_ret);

/**
 * thread_runner:  Wrapper for threads started with sys_thread_create()
 * which handles setting the "finished" flag when the thread terminates
 * and returning a pointer as pthreads expects.  On Android, this function
 * also attaches the thread to / detaches it from the Dalvik VM.
 *
 * [Parameters]
 *     param: Thread parameter (points to a SysThread structure).
 * [Return value]
 *     Thread function return value.
 */
static void *thread_runner(void *param);


#ifdef __linux__

/**
 * linux_cpumask_from_u64:  Store a 64-bit affinity bitmask into a
 * cpu_set_t variable.
 */
static void linux_cpumask_from_u64(cpu_set_t *cpumask, uint64_t u64);

/**
 * linux_cpumask_to_u64:  Return the first 64 bits of a cpu_set_t variable
 * as a 64-bit affinity bitmask.
 */
static uint64_t linux_cpumask_to_u64(const cpu_set_t *cpumask);

#endif  // __linux__

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int sys_thread_get_num_cores(void)
{
    int nproc = (int)sysconf(_SC_NPROCESSORS_CONF);
    if (nproc >= 0) {
        ASSERT(nproc > 0, nproc = 1);
        return (int)nproc;
    }

    DLOG("Number of processors unknown, returning 1");
    return 1;
}

/*-----------------------------------------------------------------------*/

SysThreadID sys_thread_create(
    const ThreadAttributes *attr, int (*function)(void *), void *param)
{
    #define CHECK(call)  do {                       \
        const int error = (call);                   \
        if (UNLIKELY(error != 0)) {                 \
            DLOG("%s: %s", #call, strerror(error)); \
            return 0;                               \
        }                                           \
    } while (0)

    CHECK(pthread_once(&key_once, posix_thread_init));
    if (UNLIKELY(!sys_thread_key_created)) {
        DLOG("Failed to create TLS key for thread ID");
        return 0;
    }

    int policy;
    struct sched_param sched_param;
    CHECK(pthread_getschedparam(pthread_self(), &policy, &sched_param));

#ifdef __linux__
    /* Linux (including Android) doesn't allow setting thread priorities
     * via pthreads, but it usefully deviates from POSIX in allowing
     * setpriority() to work on single threads, so we take that approach
     * instead. */
    int real_priority = base_priority - attr->priority;
# ifdef SIL_PLATFORM_ANDROID
    const int pri_min = -20;
# else  // SIL_PLATFORM_LINUX
    int pri_min;
    struct rlimit rlim;
    if (UNLIKELY(getrlimit(RLIMIT_NICE, &rlim) != 0)) {
        DLOG("%p(%p): getrlimit(RLIMIT_NICE): %s", function, param,
             strerror(errno));
    }
    if (rlim.rlim_cur == RLIM_INFINITY) {
        pri_min = -20;
    } else {
        pri_min = 20 - bound(rlim.rlim_cur, 1, 40);
    }
    /* We can always start a thread at the same or lower priority (higher
     * nice level) regardless of resource limit settings. */
    int my_priority;
    if (get_raw_priority(&my_priority) && my_priority < pri_min) {
        pri_min = my_priority;
    }
# endif
    const int pri_max = 19;
    if (real_priority < pri_min) {
        DLOG("%p(%p): Requested priority %d (actual %d) too high, using"
             " %d (%d)", function, param, attr->priority, real_priority,
             attr->priority - (pri_min - real_priority), pri_min);
        real_priority = pri_min;
    } else if (real_priority > pri_max) {
        DLOG("%p(%p): Requested priority %d (actual %d) too low, using"
             " %d (%d)", function, param, attr->priority, real_priority,
             attr->priority - (pri_max - real_priority), pri_max);
        real_priority = pri_max;
    }
#else  // !__linux__
    sched_param.sched_priority = base_priority + attr->priority;
    const int pri_min = sched_get_priority_min(policy);
    const int pri_max = sched_get_priority_max(policy);
    if (sched_param.sched_priority < pri_min) {
        DLOG("%p(%p): Requested priority %d (actual %d) too low, using %d (%d)",
             function, param, attr->priority, sched_param.sched_priority,
             attr->priority + (pri_min - sched_param.sched_priority), pri_min);
        sched_param.sched_priority = pri_min;
    } else if (sched_param.sched_priority > pri_max) {
        DLOG("%p(%p): Requested priority %d (actual %d) too high, using %d (%d)",
             function, param, attr->priority, sched_param.sched_priority,
             attr->priority + (pri_max - sched_param.sched_priority), pri_max);
        sched_param.sched_priority = pri_max;
    }
#endif  // __linux__

    pthread_attr_t pth_attr;
    CHECK(pthread_attr_init(&pth_attr));
    CHECK(pthread_attr_setschedpolicy(&pth_attr, policy));
    CHECK(pthread_attr_setschedparam(&pth_attr, &sched_param));
    if (attr->stack_size > 0) {
        int page_size = sysconf(_SC_PAGESIZE);
        ASSERT(page_size > 0, page_size = 4096);
        int stack_size = align_up(attr->stack_size, page_size);
        stack_size = lbound(stack_size, (int)PTHREAD_STACK_MIN);
#ifdef __linux__
        /* Linux doesn't let you actually access the lowest page of the
         * stack (WTF?), and we don't seem to get to use the highest page,
         * so add two extra pages to the requested stack size to ensure we
         * get the usable size we want. */
        stack_size += page_size*2;
#endif
#ifdef SIL_PLATFORM_IOS
        /* Add an extra page to cover stack usage by DLOG() -> NSLog(). */
        stack_size += page_size;
#endif
#ifdef COVERAGE
        /* Add extra space to account for thread-local storage for
         * coverage tracking. */
        stack_size += page_size*4;
#endif
        CHECK(pthread_attr_setstacksize(&pth_attr, stack_size));
    }

    SysThread *thread = mem_alloc(sizeof(*thread), 0, 0);
    if (UNLIKELY(!thread)) {
        DLOG("No memory for thread structure");
        return 0;
    }
    thread->name = attr->name;
    thread->function = function;
    thread->param = param;
#ifdef __linux__
    thread->initial_priority = real_priority;
    thread->initial_affinity = attr->affinity;
#endif
    thread->finished = 0;

    const int error = pthread_create(&thread->handle, &pth_attr,
                                     thread_runner, thread);
    pthread_attr_destroy(&pth_attr);
    if (UNLIKELY(error != 0)) {
        DLOG("Failed to create thread for %p(%p): %s", function, param,
             strerror(error));
        mem_free(thread);
        return 0;
    }

    return (SysThreadID)thread;

    #undef CHECK
}

/*-----------------------------------------------------------------------*/

void sys_thread_exit(int exit_code)
{
    posix_thread_runner_cleanup(pthread_getspecific(sys_thread_key));
    pthread_exit((void *)(intptr_t)exit_code);
}

/*-----------------------------------------------------------------------*/

SysThreadID sys_thread_get_id(void)
{
    return (SysThreadID)pthread_getspecific(sys_thread_key);
}

/*-----------------------------------------------------------------------*/

int sys_thread_get_priority(void)
{
    int my_priority;
    if (UNLIKELY(!get_raw_priority(&my_priority))) {
        return 0;
    }
#ifdef __linux__
    return base_priority - my_priority;
#else
    return my_priority - base_priority;
#endif
}

/*-----------------------------------------------------------------------*/

int sys_thread_set_affinity(uint64_t affinity)
{
    /* Affinity functions are strangely missing from POSIX, so we need
     * platform-specific code.  Sigh. */
#ifdef __linux__
    cpu_set_t cpuset;
    linux_cpumask_from_u64(&cpuset, affinity);
    if (UNLIKELY(sched_setaffinity(0, sizeof(cpuset), &cpuset) < 0)) {
        DLOG("sched_setaffinity() failed: %s", strerror(errno));
        return 0;
    }
    return 1;
#else
    (void) affinity;  // Avoid an unused-variable warning.
    return 0;
#endif
}

/*-----------------------------------------------------------------------*/

uint64_t sys_thread_get_affinity(void)
{
#ifdef __linux__
    cpu_set_t cpuset;
    if (UNLIKELY(sched_getaffinity(0, sizeof(cpuset), &cpuset) < 0)) {
        DLOG("sched_getaffinity() failed: %s", strerror(errno));
        return ~UINT64_C(0);
    }
    return linux_cpumask_to_u64(&cpuset);
#else
    return ~UINT64_C(0);
#endif
}

/*-----------------------------------------------------------------------*/

int sys_thread_is_running(SysThreadID thread_)
{
    SysThread *thread = (SysThread *)thread_;
    BARRIER();
    return !thread->finished;
}

/*-----------------------------------------------------------------------*/

int sys_thread_wait(SysThreadID thread_, int *result_ret)
{
    SysThread *thread = (SysThread *)thread_;
#ifdef SIL_PLATFORM_ANDROID
    /* Some older versions of Android fail to detect waiting on self. */
    if (UNLIKELY(thread->handle == pthread_self())) {
        DLOG("pthread_join(%p): %s", &thread->handle, strerror(EDEADLK));
        return 0;
    }
#endif
    void *retval = NULL;
    int error = pthread_join(thread->handle, &retval);
    if (UNLIKELY(error != 0)) {
        DLOG("pthread_join(%p): %s", &thread->handle, strerror(error));
        return 0;
    }
    mem_free(thread);
    /* Convert through intptr_t to silence compiler warnings about
     * converting from a pointer to a differently-sized integer. */
    *result_ret = (int)(intptr_t)retval;
    return 1;
}

/*-----------------------------------------------------------------------*/

void sys_thread_yield()
{
    sched_yield();
}

/*************************************************************************/
/*********************** Exported utility routines ***********************/
/*************************************************************************/

int posix_thread_create_detached(void (*function)(void *), void *param)
{
    PRECOND(function != NULL, return 0);

    #define CHECK(call)  do {                       \
        const int error = (call);                   \
        if (UNLIKELY(error != 0)) {                 \
            DLOG("%s: %s", #call, strerror(error)); \
            return 0;                               \
        }                                           \
    } while (0)

    CHECK(pthread_once(&key_once, posix_thread_init));
    if (UNLIKELY(!sys_thread_key_created)) {
        DLOG("Failed to create TLS key for thread ID");
        return 0;
    }

    pthread_attr_t attr;
    CHECK(pthread_attr_init(&attr));
    CHECK(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED));

    pthread_t handle;
    const int error = pthread_create(&handle, &attr, (void *)function, param);
    pthread_attr_destroy(&attr);
    if (LIKELY(error == 0)) {
        return 1;
    } else {
        DLOG("Failed to create thread for %p(%p): %s", function, param,
             strerror(error));
        return 0;
    }

    #undef CHECK
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static void posix_thread_init(void)
{
    errno = 0;
    if (UNLIKELY(!get_raw_priority(&base_priority))) {
        base_priority = 0;
    }

    int error;
    if (UNLIKELY((error = pthread_key_create(&sys_thread_key, NULL)) != 0)) {
        DLOG("pthread_key_create() failed: %s", strerror(error));
        return;
    }
    sys_thread_key_created = 1;
}

/*-----------------------------------------------------------------------*/

static int get_raw_priority(int *priority_ret)
{
    PRECOND(priority_ret != NULL, return 0);

#ifdef __linux__
    errno = 0;
    const int my_priority = getpriority(PRIO_PROCESS, 0);
    if (my_priority == -1 && UNLIKELY(errno != 0)) {
        DLOG("getpriority(PRIO_PROCESS, 0) failed: %s", strerror(errno));
        return 0;
    }
    *priority_ret = my_priority;
#else
    int policy;
    struct sched_param sched_param;
    int error = pthread_getschedparam(pthread_self(), &policy, &sched_param);
    if (UNLIKELY(error != 0)) {
        DLOG("pthread_getschedparam(self) failed: %s", strerror(error));
        return 0;
    }
    *priority_ret = sched_param.sched_priority;
#endif
    return 1;
}

/*-----------------------------------------------------------------------*/

static void *thread_runner(void *param)
{
    SysThread *thread = (SysThread *)param;
    PRECOND(thread != NULL, return NULL);

    int error;
    if (UNLIKELY((error = pthread_setspecific(sys_thread_key, thread)) != 0)) {
        DLOG("Failed to store thread ID");
        return NULL;
    }

    if (thread->name) {
#ifdef __linux__
        pthread_setname_np(thread->handle, thread->name);
#else  // Must be Darwin, at least at the moment.
        pthread_setname_np(thread->name);
#endif
    }

    posix_thread_runner_init(thread);
    const int retval = (*thread->function)(thread->param);
    posix_thread_runner_cleanup(thread);

    /* Make sure all actions from the thread function happen before the
     * thread->finished store is seen. */
    BARRIER();
    thread->finished = 1;

    return (void *)(intptr_t)retval;
}

/*-----------------------------------------------------------------------*/

#ifdef __linux__

STATIC_ASSERT(sizeof(cpu_set_t) >= sizeof(unsigned long),
              "Invalid cpu_set_t definition");
STATIC_ASSERT(sizeof(unsigned long) >= 8 || sizeof(unsigned long) == 4,
              "Invalid sizeof(unsigned long)");

static void linux_cpumask_from_u64(cpu_set_t *cpumask, uint64_t u64)
{
    mem_clear(cpumask, sizeof(*cpumask));
    if (sizeof(unsigned long) >= 8) {
        ((unsigned long *)cpumask)[0] = u64;
    } else {
        ((unsigned long *)cpumask)[0] = (uint32_t)u64;
        if (sizeof(*cpumask) >= 8) {
            ((unsigned long *)cpumask)[1] = (uint32_t)(u64 >> 32);
        }
    }
}

static uint64_t linux_cpumask_to_u64(const cpu_set_t *cpumask)
{
    if (sizeof(unsigned long) >= 8) {
        return ((const unsigned long *)cpumask)[0];
    } else {
        const uint32_t affinity_lo = ((const unsigned long *)cpumask)[0];
        const uint64_t affinity_hi =
            sizeof(*cpumask) >= 8 ? ((const unsigned long *)cpumask)[1] : 0;
        return affinity_lo | affinity_hi<<32;
    }
}

#endif  // __linux__

/*************************************************************************/
/*************************************************************************/
