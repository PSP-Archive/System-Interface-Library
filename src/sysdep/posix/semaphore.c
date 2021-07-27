/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/posix/semaphore.c: System-level semaphore routines for
 * POSIX-compatible systems.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/posix/util.h"

#include <semaphore.h>
#include <time.h>

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int sys_semaphore_max_value(void)
{
    int max_value = INT_MAX;

#ifdef _SC_SEM_VALUE_MAX
    errno = 0;
    const long sem_value_max = sysconf(_SC_SEM_VALUE_MAX);
    if (UNLIKELY(errno != 0)) {
        DLOG("Warning: sysconf(_SC_SEM_VALUE_MAX) failed: %s",
             strerror(errno));
    } else {
        max_value = min(max_value, sem_value_max);
    }
#endif

#ifdef SEM_VALUE_MAX
    max_value = min(max_value, SEM_VALUE_MAX);
#endif

    return max_value;
}

/*-----------------------------------------------------------------------*/

SysSemaphoreID sys_semaphore_create(int initial_value, int required_max)
{
    if (required_max > sys_semaphore_max_value()) {
        DLOG("required_max %d exceeds system limit %d", required_max,
             sys_semaphore_max_value());
        return 0;
    }

    sem_t *semaphore = mem_alloc(sizeof(*semaphore), 0, MEM_ALLOC_CLEAR);
    if (UNLIKELY(!semaphore)) {
        DLOG("No memory for semaphore");
        return 0;
    }
    if (UNLIKELY(sem_init(semaphore, 0, initial_value) != 0)) {
        DLOG("Failed to initialize semaphore: %s", strerror(errno));
        mem_free(semaphore);
        return 0;
    }
    return (SysSemaphoreID)semaphore;
}

/*-----------------------------------------------------------------------*/

void sys_semaphore_destroy(SysSemaphoreID semaphore_)
{
    sem_t *semaphore = (sem_t *)semaphore_;
    sem_destroy(semaphore);
    mem_free(semaphore);
}

/*-----------------------------------------------------------------------*/

int sys_semaphore_wait(SysSemaphoreID semaphore_, float timeout)
{
    sem_t *semaphore = (sem_t *)semaphore_;
    if (timeout < 0) {
        sem_wait(semaphore);
        return 1;
    } else {
        struct timespec ts = timeout_to_ts(timeout);
        return sem_timedwait(semaphore, &ts) == 0;
    }
}

/*-----------------------------------------------------------------------*/

void sys_semaphore_signal(SysSemaphoreID semaphore_)
{
    sem_t *semaphore = (sem_t *)semaphore_;
    sem_post(semaphore);
}

/*************************************************************************/
/*************************************************************************/
