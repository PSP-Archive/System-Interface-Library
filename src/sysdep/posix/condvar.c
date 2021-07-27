/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/posix/condvar.c: System-level condition variable routines for
 * POSIX-compatible systems.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/posix/util.h"

#include <pthread.h>
#include <time.h>

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

SysCondVarID sys_condvar_create(void)
{
    pthread_cond_t *condvar = mem_alloc(sizeof(*condvar), 0, MEM_ALLOC_CLEAR);
    if (UNLIKELY(!condvar)) {
        DLOG("No memory for condition variable");
        return 0;
    }

    int error;
    if (UNLIKELY((error = pthread_cond_init(condvar, NULL)) != 0)) {
        DLOG("Failed to initialize condition variable: %s", strerror(error));
        mem_free(condvar);
        return 0;
    }

    return (SysCondVarID)condvar;
}

/*-----------------------------------------------------------------------*/

void sys_condvar_destroy(SysCondVarID condvar_)
{
    pthread_cond_t *condvar = (pthread_cond_t *)condvar_;
    pthread_cond_destroy(condvar);
    mem_free(condvar);
}

/*-----------------------------------------------------------------------*/

int sys_condvar_wait(SysCondVarID condvar_, SysMutexID mutex_, float timeout)
{
    pthread_cond_t *condvar = (pthread_cond_t *)condvar_;
    pthread_mutex_t *mutex = (pthread_mutex_t *)mutex_;
    if (timeout < 0) {
        pthread_cond_wait(condvar, mutex);
        return 1;
    } else {
        struct timespec ts = timeout_to_ts(timeout);
        return pthread_cond_timedwait(condvar, mutex, &ts) == 0;
    }
}

/*-----------------------------------------------------------------------*/

void sys_condvar_signal(SysCondVarID condvar_, int broadcast)
{
    pthread_cond_t *condvar = (pthread_cond_t *)condvar_;
    if (broadcast) {
        pthread_cond_broadcast(condvar);
    } else {
        pthread_cond_signal(condvar);
    }
}

/*************************************************************************/
/*************************************************************************/
