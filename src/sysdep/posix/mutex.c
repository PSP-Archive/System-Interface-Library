/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/posix/mutex.c: Mutex routines for POSIX-compatible systems.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/posix/util.h"

#include <pthread.h>
#include <time.h>

/* Android and Darwin are both missing pthread_mutex_timedlock(), so we
 * roll our own. */
#if defined(SIL_PLATFORM_ANDROID) || defined(SIL_PLATFORM_IOS) || defined(SIL_PLATFORM_MACOSX)
# if defined(SIL_PLATFORM_IOS) || defined(SIL_PLATFORM_MACOSX)
#  include <sys/time.h>
# endif
# define pthread_mutex_timedlock  SIL_pthread_mutex_timedlock
static int SIL_pthread_mutex_timedlock(
    pthread_mutex_t *mutex, const struct timespec *abs_timeout);
#endif

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

SysMutexID sys_mutex_create(int recursive, int initially_locked)
{
    pthread_mutex_t *mutex = mem_alloc(sizeof(*mutex), 0, MEM_ALLOC_CLEAR);
    if (UNLIKELY(!mutex)) {
        DLOG("No memory for mutex");
        goto error_return;
    }

    int error;
    pthread_mutexattr_t attr;
    if (UNLIKELY((error = pthread_mutexattr_init(&attr)) != 0)) {
        DLOG("Failed to initialize mutex attributes: %s", strerror(error));
        goto error_free_mutex;
    }
    if (recursive) {
        error = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        if (error != 0) {
            DLOG("Failed to set recursive attribute: %s", strerror(error));
            goto error_free_mutex;
        }
    }
    if (UNLIKELY((error = pthread_mutex_init(mutex, &attr)) != 0)) {
        DLOG("Failed to initialize mutex: %s", strerror(error));
        goto error_free_mutex;
    }
    pthread_mutexattr_destroy(&attr);

    if (initially_locked) {
        pthread_mutex_lock(mutex);
    }

    return (SysMutexID)mutex;

  error_free_mutex:
    mem_free(mutex);
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

void sys_mutex_destroy(SysMutexID mutex_)
{
    pthread_mutex_t *mutex = (pthread_mutex_t *)mutex_;
    pthread_mutex_destroy(mutex);
    mem_free(mutex);
}

/*-----------------------------------------------------------------------*/

int sys_mutex_lock(SysMutexID mutex_, float timeout)
{
    pthread_mutex_t *mutex = (pthread_mutex_t *)mutex_;
    if (timeout < 0) {
        pthread_mutex_lock(mutex);
        return 1;
    } else {
        struct timespec ts = timeout_to_ts(timeout);
        return pthread_mutex_timedlock(mutex, &ts) == 0;
    }
}

/*-----------------------------------------------------------------------*/

void sys_mutex_unlock(SysMutexID mutex_)
{
    pthread_mutex_t *mutex = (pthread_mutex_t *)mutex_;
    pthread_mutex_unlock(mutex);
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

#if defined(SIL_PLATFORM_ANDROID) || defined(SIL_PLATFORM_IOS) || defined(SIL_PLATFORM_MACOSX)

static int SIL_pthread_mutex_timedlock(
    pthread_mutex_t *mutex, const struct timespec *abs_timeout)
{
    for (;;) {
        const int error = pthread_mutex_trylock(mutex);
        if (error == 0) {
            return 0;
        } else if (error != EBUSY) {
            return error;
        }

        struct timespec now;
# if defined(SIL_PLATFORM_IOS) || defined(SIL_PLATFORM_MACOSX)
        struct timeval tv;
        gettimeofday(&tv, NULL);
        now.tv_sec = tv.tv_sec;
        now.tv_nsec = tv.tv_usec * 1000;
# else
        clock_gettime(CLOCK_REALTIME, &now);
# endif
        struct timespec time_left = {
            .tv_sec = abs_timeout->tv_sec - now.tv_sec,
            .tv_nsec = abs_timeout->tv_nsec - now.tv_nsec,
        };
        if (time_left.tv_nsec < 0) {
            time_left.tv_sec--;
            time_left.tv_nsec += 1000000000;
        }

        if (time_left.tv_sec < 0) {
            return ETIMEDOUT;
        } else {
            /* Don't sleep for more than 1 msec at a time, as a balance
             * between frequent checks and accurate wakeups. */
            if (time_left.tv_sec > 0 || time_left.tv_nsec > 1000000) {
                time_left.tv_sec = 0;
                time_left.tv_nsec = 1000000;
            }
            nanosleep(&time_left, NULL);
        }
    }
}

#endif  // SIL_PLATFORM_ANDROID etc.

/*************************************************************************/
/*************************************************************************/
