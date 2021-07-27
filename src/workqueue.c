/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/workqueue.c: Work queue handling.
 */

#include "src/base.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/mutex.h"
#include "src/semaphore.h"
#include "src/sysdep.h"
#include "src/thread.h"
#include "src/utility/id-array.h"
#include "src/workqueue.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

typedef struct WorkQueue WorkQueue;
typedef struct WorkerThread WorkerThread;
typedef struct WorkerThreadData WorkerThreadData;
typedef struct WorkUnit WorkUnit;

/* Data structure for a work queue. */
struct WorkQueue {
    /* Maximum number of work units to execute simultaneously. */
    int max_concurrency;
    /* Semaphore used to limit simultaneous work unit execution. */
    int concurrency_semaphore;
    /* Thread ID of the dispatcher thread. */
    int dispatcher_thread;
    /* Semaphore for signaling the dispatcher thread. */
    int dispatcher_semaphore;
    /* Flag to tell the dispatcher thread to terminate. */
    uint8_t dispatcher_terminate;
    /* Convenience flag set by the dispatcher thread to indicate whether
     * the work queue is currently busy. */
    uint8_t busy;
    /* Flag set by workqueue_wait_all() to request a signal on
     * idle_semaphore when the work queue becomes idle. */
    uint8_t send_idle_signal;
    /* Semaphore signalled for send_idle_signal. */
    int idle_semaphore;

    /* Mutex controlling access to the structure.  This mutex must be
     * locked when accessing any of the data below. */
    int mutex;

    /* Table of worker threads for this work queue, of length
     * max_concurrency. */
    WorkerThread *worker_threads;
    /* Array holding the work unit queue, and its current size. */
    WorkUnit *work_queue;
    int work_queue_size;
    /* Index of the first pending work unit in the work queue, or -1 if no
     * work units are pending. */
    int first_pending;
    /* Index of the last pending work unit in the work queue, or -1 if no
     * work units are pending. */
    int last_pending;
    /* Index of the first free entry in the work queue, or -1 if no entries
     * are free. */
    int first_free;
};

/* Data structure shared between the dispatcher thread and each worker
 * thread. */
struct WorkerThreadData {
    /* Work queue to which this thread belongs. */
    WorkQueue *wq;
    /* Semaphore used to signal between the dispatcher and worker threads. */
    int semaphore;
    /* Flag set by interface routines to tell this thread to terminate. */
    uint8_t terminate;
    /* Index in work_queue of the work unit currently being processed, or
     * -1 if the thread is idle.  This is set by the dispatcher thread
     * prior to signaling the worker thread, and is cleared by the worker
     * thread prior to signaling the dispatcher thread when it finishes
     * processing the work unit. */
    int wu_index;
};

/* Data structure describing a worker thread in a work queue. */
struct WorkerThread {
    /* Thread ID of the worker thread. */
    int thread;
    /* Data structure passed to the thread on creation. */
    WorkerThreadData data;
};

/* Data structure for a work unit. */
struct WorkUnit {
    /* Flag indicating whether this entry is in use. */
    uint8_t in_use;
    /* Flag indicating whether processing has started for this work unit. */
    uint8_t started;
    /* Flag indicating whether processing has completed for this work unit. */
    uint8_t completed;
    /* Semaphore to signal when the work unit completes, or 0 if none. */
    int semaphore;

    /* The function to call. */
    WorkUnitFunction *function;
    /* The argument to pass to "function". */
    void *argument;
    /* The result of the function. */
    int result;

    /* For in-use but not-yet-started entries, index of the next work unit
     * submitted to the queue.  For free entries, index of the next free
     * entry in the queue array.  In both cases, -1 signals the end of the
     * list. */
    int next;
};

/*-----------------------------------------------------------------------*/

/* Array of created work queues. */
static IDArray workqueues = ID_ARRAY_INITIALIZER(10);

/**
 * VALIDATE_WORKQUEUE:  Validate the work queue ID passed to a workqueue
 * handling routine, and store the corresponding pointer in the variable
 * "wq".  If the workqueue ID is invalid, the "error_return" statement is
 * executed; this may consist of multiple statements, but must include a
 * "return" to exit the function.
 */
#define VALIDATE_WORKQUEUE(id,wq,error_return) \
    ID_ARRAY_VALIDATE(&workqueues, (id), WorkQueue *, wq, \
                      DLOG("Work queue ID %d is invalid", _id); error_return)

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * release_work_unit_locked:  Release entry "index" in the work queue array
 * for use by new work units.
 *
 * This function must be called with wq->mutex locked.
 *
 * [Parameters]
 *     wq: Work queue.
 *     index: Index in wq->work_queue[] of entry to release.
 */
static void release_work_unit_locked(WorkQueue *wq, int index);

/**
 * dispatcher:  Dispatcher thread routine.  One dispatcher thread is
 * created for each work queue; the dispatcher takes care of sending work
 * units out to individual worker threads.
 *
 * [Parameters]
 *     wq_: Pointer to the WorkQueue on which to operate (as "void *").
 * [Return value]
 *     0
 */
static int dispatcher(void *wq_);

/**
 * worker:  Worker thread routine.  One worker thread is created for each
 * potential concurrent work unit in a work queue (thus, the number of
 * worker threads is equal to the max_concurrency value passed to
 * workqueue_create()).  Worker threads are long-lived, executing work
 * units as the dispatcher sends them.
 *
 * [Parameters]
 *     data_: WorkerThreadData pointer for this thread (as "void *").
 * [Return value]
 *     0
 */
static int worker(void *data_);

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int workqueue_create(int max_concurrency)
{
    if (UNLIKELY(max_concurrency <= 0)) {
        DLOG("Invalid max_concurrency value %d (must be positive)",
             max_concurrency);
        goto error_return;
    }

    WorkQueue *wq = mem_alloc(sizeof(*wq), 0, 0);
    if (UNLIKELY(!wq)) {
        DLOG("Failed to allocate WorkQueue");
        goto error_return;
    }

    /* As in thread_create() and friends, we can't easily clean up a thread
     * after creation if the ID array store fails, so allocate an ID now. */
    const int id = id_array_register(&workqueues, wq);
    if (UNLIKELY(!id)) {
        DLOG("Failed to allocate new ID for work queue");
        goto error_free_wq;
    }

    wq->max_concurrency      = max_concurrency;
    wq->dispatcher_terminate = 0;
    wq->busy                 = 0;
    wq->send_idle_signal     = 0;
    wq->work_queue_size      = max_concurrency;
    wq->first_pending        = -1;
    wq->last_pending         = -1;
    wq->first_free           = 0;

    wq->concurrency_semaphore =
        semaphore_create(wq->max_concurrency, wq->max_concurrency);
    if (UNLIKELY(!wq->concurrency_semaphore)) {
        DLOG("Failed to create concurrency semaphore");
        goto error_release_id;
    }

    wq->dispatcher_semaphore = semaphore_create(0, semaphore_max_value());
    if (UNLIKELY(!wq->dispatcher_semaphore)) {
        DLOG("Failed to create dispatcher semaphore");
        goto error_destroy_concurrency_semaphore;
    }

    wq->idle_semaphore = semaphore_create(0, 1);
    if (UNLIKELY(!wq->idle_semaphore)) {
        DLOG("Failed to create idle semaphore");
        goto error_destroy_dispatcher_semaphore;
    }

    wq->mutex = mutex_create(MUTEX_SIMPLE, MUTEX_UNLOCKED);
    if (UNLIKELY(!wq->mutex)) {
        DLOG("Failed to create mutex");
        goto error_destroy_idle_semaphore;
    }

    wq->worker_threads =
        mem_alloc(sizeof(*wq->worker_threads) * wq->max_concurrency, 0, 0);
    if (UNLIKELY(!wq->worker_threads)) {
        DLOG("Failed to allocate worker thread array");
        goto error_destroy_mutex;
    }

    wq->work_queue =
        mem_alloc(sizeof(*wq->work_queue) * wq->work_queue_size, 0, 0);
    if (UNLIKELY(!wq->work_queue)) {
        DLOG("Failed to allocate work unit queue");
        goto error_free_worker_threads;
    }
    for (int i = 0; i < wq->work_queue_size; i++) {
        wq->work_queue[i].in_use = 0;
        if (i == wq->work_queue_size - 1) {
            wq->work_queue[i].next = -1;
        } else {
            wq->work_queue[i].next = i+1;
        }
    }

    for (int i = 0; i < wq->max_concurrency; i++) {
        wq->worker_threads[i].data.wq        = wq;
        wq->worker_threads[i].data.terminate = 0;
        wq->worker_threads[i].data.wu_index  = -1;
        wq->worker_threads[i].data.semaphore = 0;
        wq->worker_threads[i].thread         = 0;
    }
    for (int i = 0; i < wq->max_concurrency; i++) {
        wq->worker_threads[i].data.semaphore = semaphore_create(0, 1);
        if (UNLIKELY(!wq->worker_threads[i].data.semaphore)) {
            DLOG("Failed to create worker thread %d semaphore", i);
            goto error_cleanup_worker_threads;
        }
        wq->worker_threads[i].thread =
            thread_create(worker, &wq->worker_threads[i].data);
        if (UNLIKELY(!wq->worker_threads[i].thread)) {
            DLOG("Failed to create worker thread %d", i);
            goto error_cleanup_worker_threads;
        }
    }

    static const ThreadAttributes dispatcher_attr = {.stack_size = 4096};
    wq->dispatcher_thread =
        thread_create_with_attr(&dispatcher_attr, dispatcher, wq);
    if (UNLIKELY(!wq->dispatcher_thread)) {
        DLOG("Failed to create dispatcher thread");
        goto error_cleanup_worker_threads;
    }

    return id;

  error_cleanup_worker_threads:
    for (int i = 0; i < wq->max_concurrency; i++) {
        if (wq->worker_threads[i].thread) {
            wq->worker_threads[i].data.terminate = 1;
            semaphore_signal(wq->worker_threads[i].data.semaphore);
            thread_wait(wq->worker_threads[i].thread);
        }
        if (wq->worker_threads[i].data.semaphore) {
            semaphore_destroy(wq->worker_threads[i].data.semaphore);
        }
    }
  error_free_worker_threads:
    mem_free(wq->worker_threads);
    mem_free(wq->work_queue);
  error_destroy_mutex:
    mutex_destroy(wq->mutex);
  error_destroy_idle_semaphore:
    semaphore_destroy(wq->idle_semaphore);
  error_destroy_dispatcher_semaphore:
    semaphore_destroy(wq->dispatcher_semaphore);
  error_destroy_concurrency_semaphore:
    semaphore_destroy(wq->concurrency_semaphore);
  error_release_id:
    id_array_release(&workqueues, id);
  error_free_wq:
    mem_free(wq);
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

void workqueue_destroy(int workqueue)
{
    if (!workqueue) {
        return;
    }

    WorkQueue *wq;
    VALIDATE_WORKQUEUE(workqueue, wq, return);

    /* Lock the mutex for just an instant to ensure that any previous call
     * to an interface function has completed before we proceed. */
    mutex_lock(wq->mutex);
    mutex_unlock(wq->mutex);

    wq->dispatcher_terminate = 1;
    semaphore_signal(wq->dispatcher_semaphore);
    thread_wait(wq->dispatcher_thread);

    for (int i = 0; i < wq->max_concurrency; i++) {
        wq->worker_threads[i].data.terminate = 1;
        semaphore_signal(wq->worker_threads[i].data.semaphore);
        thread_wait(wq->worker_threads[i].thread);
        semaphore_destroy(wq->worker_threads[i].data.semaphore);
    }

    mem_free(wq->work_queue);
    mem_free(wq->worker_threads);
    mutex_destroy(wq->mutex);
    semaphore_destroy(wq->idle_semaphore);
    semaphore_destroy(wq->dispatcher_semaphore);
    semaphore_destroy(wq->concurrency_semaphore);
    mem_free(wq);
    id_array_release(&workqueues, workqueue);
}

/*-----------------------------------------------------------------------*/

int workqueue_is_busy(int workqueue)
{
    WorkQueue *wq;
    VALIDATE_WORKQUEUE(workqueue, wq, return 0);

    BARRIER();
    return wq->busy;
}

/*-----------------------------------------------------------------------*/

void workqueue_wait_all(int workqueue)
{
    WorkQueue *wq;
    VALIDATE_WORKQUEUE(workqueue, wq, return);

    wq->send_idle_signal = 1;
    semaphore_signal(wq->dispatcher_semaphore);
    semaphore_wait(wq->idle_semaphore);

    /* Reap any completed work units. */
    mutex_lock(wq->mutex);
    for (int i = 0; i < wq->work_queue_size; i++) {
        WorkUnit *wu = &wq->work_queue[i];
        /* Testing note: The only way to get (wu->in_use && !wu->completed)
         * is to have another thread submit a work unit in the instant
         * between the dispatcher unlocking the mutex after signalling the
         * idle semaphore and this function taking the mutex with the
         * mutex_lock() call above.  This is a difficult race to win, so
         * we take it on faith that the code works correctly under that
         * circumstance. */
        if (wu->in_use && wu->completed) {
            release_work_unit_locked(wq, i);
        }
    }
    mutex_unlock(wq->mutex);
}

/*-----------------------------------------------------------------------*/

int workqueue_submit(int workqueue, WorkUnitFunction *function,
                     void *argument)
{
    WorkQueue *wq;
    VALIDATE_WORKQUEUE(workqueue, wq, return 0);
    if (UNLIKELY(!function)) {
        DLOG("function == NULL");
        return 0;
    }

    mutex_lock(wq->mutex);

    if (wq->first_free < 0) {
        const int increment =
            lbound((wq->work_queue_size + 4) / 5, wq->max_concurrency);
        /* We assume this will not overflow, but if it does, mem_realloc()
         * will fail due to the negative size and nothing bad will happen.
         * (This is true for a 32-bit int; the queue size cannot realistically
         * overflow if int is 64 bits, so the issue is moot.) */
        const int new_size = wq->work_queue_size + increment;
        WorkUnit *new_queue =
            mem_realloc(wq->work_queue, sizeof(*wq->work_queue) * new_size, 0);
        if (UNLIKELY(!new_queue)) {
            DLOG("Failed to expand work queue size from %d to %d units",
                 wq->work_queue_size, new_size);
            mutex_unlock(wq->mutex);
            return 0;
        }
        for (int i = wq->work_queue_size; i < new_size; i++) {
            new_queue[i].in_use = 0;
            if (i == new_size - 1) {
                new_queue[i].next = -1;
            } else {
                new_queue[i].next = i+1;
            }
        }
        wq->first_free = wq->work_queue_size;
        wq->work_queue = new_queue;
        wq->work_queue_size = new_size;
    }

    const int index = wq->first_free;
    WorkUnit *wu = &wq->work_queue[index];
    wq->first_free = wu->next;

    wu->in_use    = 1;
    wu->started   = 0;
    wu->completed = 0;
    wu->semaphore = 0;
    wu->function  = function;
    wu->argument  = argument;

    if (wq->first_pending < 0) {
        wq->first_pending = index;
    }
    if (wq->last_pending >= 0) {
        wq->work_queue[wq->last_pending].next = index;
    }
    wq->last_pending = index;
    wu->next = -1;

    wq->busy = 1;
    semaphore_signal(wq->dispatcher_semaphore);

    mutex_unlock(wq->mutex);
    return index+1;
}

/*-----------------------------------------------------------------------*/

int workqueue_poll(int workqueue, int unit)
{
    WorkQueue *wq;
    VALIDATE_WORKQUEUE(workqueue, wq, return 1);

    mutex_lock(wq->mutex);

    const int index = unit-1;
    if (UNLIKELY(index < 0)
     || UNLIKELY(index >= wq->work_queue_size)
     || UNLIKELY(!wq->work_queue[index].in_use)) {
        DLOG("Work unit ID %d is invalid for work queue %d", unit, workqueue);
        mutex_unlock(wq->mutex);
        return 1;
    }
    WorkUnit *wu = &wq->work_queue[index];

    const int completed = wu->completed;

    mutex_unlock(wq->mutex);
    return completed;
}

/*-----------------------------------------------------------------------*/

int workqueue_wait(int workqueue, int unit)
{
    WorkQueue *wq;
    VALIDATE_WORKQUEUE(workqueue, wq, return 0);

    mutex_lock(wq->mutex);

    const int index = unit-1;
    if (UNLIKELY(index < 0)
     || UNLIKELY(index >= wq->work_queue_size)
     || UNLIKELY(!wq->work_queue[index].in_use)) {
        DLOG("Work unit ID %d is invalid for work queue %d", unit, workqueue);
        mutex_unlock(wq->mutex);
        return 0;
    }
    WorkUnit *wu = &wq->work_queue[index];

    if (!wu->completed) {
        const int semaphore = semaphore_create(0, 1);
        if (UNLIKELY(!semaphore)) {
            DLOG("WARNING: semaphore creation failed, will busy-wait!");
        }
        wu->semaphore = semaphore;

        while (!wu->completed) {
            /* Let go of the mutex while we wait, so we don't block other
             * operations on the work queue. */
            mutex_unlock(wq->mutex);
            if (semaphore) {
                semaphore_wait(semaphore);
            } else {
                thread_yield();
            }
            mutex_lock(wq->mutex);
            /* The value of index is guaranteed to still be valid, since
             * this is the only function that removes work units, but the
             * array itself may have been reallocated in the interim, so
             * update the WorkUnit pointer. */
            wu = &wq->work_queue[index];
        }

        semaphore_destroy(semaphore);
    }

    const int result = wu->result;
    release_work_unit_locked(wq, index);

    mutex_unlock(wq->mutex);
    return result;
}

/*-----------------------------------------------------------------------*/

int workqueue_cancel(int workqueue, int unit)
{
    WorkQueue *wq;
    VALIDATE_WORKQUEUE(workqueue, wq, return 0);

    mutex_lock(wq->mutex);

    const int index = unit-1;
    if (UNLIKELY(index < 0)
     || UNLIKELY(index >= wq->work_queue_size)
     || UNLIKELY(!wq->work_queue[index].in_use)) {
        DLOG("Work unit ID %d is invalid for work queue %d", unit, workqueue);
        mutex_unlock(wq->mutex);
        return 0;
    }
    WorkUnit *wu = &wq->work_queue[index];

    int cancelled;
    if (wu->started) {
        cancelled = 0;  // Can't cancel once it's running.
    } else {
        /* This is the only case in which we need to do a linear search
         * through one of the work unit lists.  We accept the O(n) runtime
         * since cancellation is assumed to be relatively rare. */
        int prev = -1;
        int *prev_nextptr = &wq->first_pending;
        while (*prev_nextptr != index) {
            /* Since the work unit hasn't been started yet, it must be on
             * the pending list.  Bail if we don't find it. */
            ASSERT(*prev_nextptr != -1, mutex_unlock(wq->mutex); return 0);
            prev = *prev_nextptr;
            prev_nextptr = &wq->work_queue[prev].next;
        }
        *prev_nextptr = wu->next;
        if (wu->next == -1) {
            wq->last_pending = prev;
        }
        release_work_unit_locked(wq, index);
        cancelled = 1;
    }

    mutex_unlock(wq->mutex);
    return cancelled;
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static void release_work_unit_locked(WorkQueue *wq, int index)
{
    WorkUnit *wu = &wq->work_queue[index];
    wu->in_use = 0;
    wu->next = wq->first_free;
    wq->first_free = index;
}

/*-----------------------------------------------------------------------*/

static int dispatcher(void *wq_)
{
    WorkQueue *wq = (WorkQueue *)wq_;
    PRECOND(wq != NULL, return 0);
    PRECOND(wq->dispatcher_semaphore != 0, return 0);

    while (!wq->dispatcher_terminate) {
        semaphore_wait(wq->dispatcher_semaphore);

        /* The entire loop body (except for the wait above) runs with the
         * mutex locked. */
        mutex_lock(wq->mutex);

        /* Keep track of whether we had any work to do for this iteration.
         * We don't update wq->busy until the end of the loop, since
         * workqueue_is_busy() reads it without locking the mutex. */
        int busy = 0;

        /* Check the state of each worker thread.  If a thread is idle and
         * there are pending work units, dequeue a work unit and assign it
         * to the thread. */
        for (int i = 0; i < wq->max_concurrency; i++) {
            if (wq->worker_threads[i].data.wu_index >= 0) {
                busy = 1;
            } else {
                const int index = wq->first_pending;
                if (index >= 0) {
                    WorkUnit *wu = &wq->work_queue[index];
                    wq->first_pending = wu->next;
                    if (wq->first_pending < 0) {
                        wq->last_pending = -1;
                    }
                    wu->started = 1;
                    wq->worker_threads[i].data.wu_index = index;
                    semaphore_signal(wq->worker_threads[i].data.semaphore);
                    busy = 1;
                }
            }
        }

        /* Report the busy/idle state back to the interface routines, and
         * send an idle signal if one was requested by workqueue_wait_all(). */
        wq->busy = busy;
        if (!busy && wq->send_idle_signal) {
            semaphore_signal(wq->idle_semaphore);
            wq->send_idle_signal = 0;
        }

        mutex_unlock(wq->mutex);
    }

    return 0;
}

/*-----------------------------------------------------------------------*/

static int worker(void *data_)
{
    WorkerThreadData *data = (WorkerThreadData *)data_;
    PRECOND(data != NULL, return 0);
    PRECOND(data->wq != NULL, return 0);
    PRECOND(data->semaphore != 0, return 0);

    WorkQueue *wq = data->wq;

    while (!data->terminate) {
        semaphore_wait(data->semaphore);

        if (data->wu_index >= 0) {
            WorkUnitFunction *function;
            void *argument;

            /* Pull the function and its argument into local variables so
             * we can call it without the mutex locked. */
            mutex_lock(wq->mutex);
            WorkUnit *wu = &wq->work_queue[data->wu_index];
            function = wu->function;
            argument = wu->argument;
            mutex_unlock(wq->mutex);

            /* Perform the actual work. */
            const int result = (*function)(argument);

            /* Store the result and mark the work unit completed.  As in
             * workqueue_wait(), the work_queue[] array may have moved, so
             * we need to get a new WorkUnit pointer. */
            mutex_lock(wq->mutex);
            wu = &wq->work_queue[data->wu_index];
            wu->result = result;
            wu->completed = 1;
            if (wu->semaphore) {
                semaphore_signal(wu->semaphore);
            }
            data->wu_index = -1;
            mutex_unlock(wq->mutex);

            /* Let the dispatcher know we're ready for more work. */
            semaphore_signal(wq->dispatcher_semaphore);
        }
    }

    return 0;
}

/*************************************************************************/
/*************************************************************************/
