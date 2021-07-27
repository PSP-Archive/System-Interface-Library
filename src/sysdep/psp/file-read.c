/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/file-read.c: Low-level file reading logic for the PSP.
 */

/*
 * This file manages all read requests made to the host filesystem, both to
 * allow prioritization of requests (for example, immediate requests should
 * be given priority over background read-ahead requests) and to prevent
 * thrashing (particularly on a non-random-access device like an optical
 * disc) when multiple requests are submitted simultaneously.
 *
 * After initializing this module by calling psp_file_read_init(), read
 * operations can be submitted by calling psp_file_read_submit().  In
 * addition to the standard parameters (file descriptor, file offset, read
 * length, and buffer pointer), the caller can also specify the read
 * priority, and can indicate whether the read should be fulfilled as soon
 * as possible or may be delayed a certain amount of time.  Once a request
 * has been submitted, the caller may wait for its completion by calling
 * psp_file_read_wait(), or may check asynchronously for completion with
 * psp_file_read_check().  Note that all read operations are asynchronous
 * in the sense that psp_file_read_submit() only starts the operation; the
 * caller must separately call psp_file_read_wait() to obtain the result.
 *
 * Read requests are normally queued on a first-come, first-served basis.
 * However, if the caller requests a specific start deadline for the
 * request, it will be delayed until either no immediate reads are pending
 * or the requested start time has passed.  Any request whose deadline has
 * passed is given priority over all other read requests, even immediate
 * reads.  (The use of a start deadline rather than a completion deadline
 * is because the time required for a request to complete is generally
 * unpredictable.)
 *
 * Internally, all requests are processed by a separate thread which loops
 * indefinitely, handling one request at a time.  Each read request is
 * broken down into chunks of at most BLOCKSIZE bytes, where the constant
 * BLOCKSIZE is defined such that the overhead from system calls is
 * negligible but the time spent reading a single block does not
 * significantly delay the read loop.  For each iteration of the loop:
 *
 * - If any new requests have been submitted, they are added to a global
 *   list of requests.  Separate lists are maintained for immediate and
 *   timed requests; the immediate list is maintained in FIFO order, while
 *   the timed-request list is sorted by start deadline (and in FIFO order
 *   for requests with the same start deadline).
 *
 * - If there are any timed read requests whose start deadlines have
 *   passed, those requests are processed immediately, starting from the
 *   request with the earliest deadline.  In this case, the request is
 *   fulfilled with a single system call, rather than being divided into
 *   blocks as usual.  This also triggers "deadline priority" mode (see
 *   below).
 *
 * - Otherwise, one block of at most BLOCKSIZE bytes is read for the
 *   highest-priority pending request:
 *      + If any immediate requests are pending, the oldest such request
 *        is used.
 *      + If there are no immediate requests but at least one timed request
 *        is pending, the timed request with the earliest deadline is used.
 *      + If there are no pending requests at all, the thread goes to
 *        sleep until signalled that new requests have arrived.
 *
 * When a timed request is given priority because its start deadline has
 * passed, there is generally a good chance that another timed request will
 * follow in short order; for example, if a streaming music player submits
 * read requests as its buffers empty, the delay of a request past its
 * start deadline may mean that another timed request has already been
 * submitted.  If we allowed immediate read requests to resume as soon as
 * the past-deadline request completed, we could easily end up in a cycle
 * of immediate reads causing deadline expiration over and over.  To
 * prevent this undesirable behavior, when a timed request is fulfilled
 * due to deadline expiration, the read loop switches to "deadline
 * priority" mode for a fixed amount of time (set by PRIORITY_TIME).
 * During this time, the loop will only process timed requests whose
 * timeouts have already expired; if no such requests are pending, the loop
 * delays for a short amount of time (set by PRIORITY_DELAY) instead of
 * processing other pending requests.  Once all such requests have been
 * processed and the priority mode timeout expires, the loop returns to
 * normal behavior.
 *
 * It is safe to perform read operations from multiple threads, but only
 * one thread may call psp_file_read_wait() for a specific request.
 */

#include "src/base.h"
#include "src/sysdep.h"  // For MAX_ASYNC_READS.
#include "src/sysdep/psp/file-read.h"
#include "src/sysdep/psp/internal.h"
#include "src/sysdep/psp/thread.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Block size for reads. */
#define BLOCKSIZE       65536

/* Maximum number of simultaneous reads to support.  We add a bit on top
 * of MAX_ASYNC_READS to leave room for synchronous reads. */
#define MAX_REQUESTS    (MAX_ASYNC_READS + 10)

/* Length of time to maintain deadline priority mode (in μsec). */
#define PRIORITY_TIME   50000

/* Loop spin interval while in deadline priority mode (in μsec). */
#define PRIORITY_DELAY  10000

/* Mutex lock timeout for psp_file_read_submit() (in μsec).  Set high so as
 * not to trigger errors if a higher-priority thread steals the CPU while
 * this is locked, but normally the lock will only be held for a few
 * microseconds at a time. */
#define SUBMIT_MUTEX_TIMEOUT  3000000

/*-----------------------------------------------------------------------*/

static struct request {
    int16_t next;       // Index of next request of the same type (-1 = end).
    uint8_t inuse:1,    // True if this entry is in use.
            new:1,      // True if this is a new request (not yet seen by the
                        //    read loop).  Once this flag is set, this request
                        //    may only be used by the read thread (except for
                        //    reading inuse or setting abort) until finished
                        //    becomes true. Conversely, the read thread will
                        //    never access the request after setting finished
                        //    to true.
            timed:1,    // True if the deadline field is valid.
            finished:1, // Completion flag; also serves as a lock (see above).
            abort:1;    // True to abort the request.
    int fd;             // File descriptor to read from.
    int64_t start;      // File offset at which to read the next block.
    int32_t len;        // Number of bytes remaining to be read.
    uint8_t *buf;       // Location at which to store the next block.
    int32_t deadline;   // Deadline (as an absolute timestamp).
    SceUID event_flag;  // Event flag used for synchronization.
    SceUID waiter;      // Thread waiting on this request (0 if none).
    int32_t res;        // Read result (number of bytes read or error code).
} requests[MAX_REQUESTS];

/* Index of the first request in the immediate and timed request lists.
 * The immediate list is ordered by request submission (new requests are
 * appended to the end of the list); the timed list is ordered by deadline
 * (with the earliest deadline at the head of the list). */
static int16_t first_immediate, first_timed;

/* Thread handle for the read loop thread. */
static SceUID file_read_thread_handle;

/* Event flag used to signal the read loop thread for newly submitted
 * requests. */
static SceUID file_read_submit_event;

/* Request creation mutex for psp_file_read_submit(). */
static SceUID file_read_submit_mutex;

/* Flag used to terminate the read thread. */
static uint8_t stop_thread;

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * file_read_thread:  Thread routine for the file-read loop.
 *
 * [Parameters]
 *     args: Argument size (unused).
 *     argp: Argument pointer (unused).
 * [Return value]
 *     0
 */
static int file_read_thread(SceSize args, void *argp);

/**
 * handle_request:  Process a single read request.
 *
 * [Parameters]
 *     req: Pointer to request block to process.
 *     all: True to read the entire request, false to only read a single block.
 * [Return value]
 *     True if request has completed (req->res and req->finished will be set),
 *     false if not.
 */
static int handle_request(struct request *req, int all);

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int psp_file_read_init(void)
{
    first_immediate = -1;
    first_timed = -1;

    file_read_submit_event = sceKernelCreateEventFlag("FileReadSubmitFlag",
                                                      0, 0, 0);
    if (file_read_submit_event < 0) {
        DLOG("Error creating submit event flag: %s",
             psp_strerror(file_read_submit_event));
        file_read_submit_event = 0;
        goto error_return;
    }

    file_read_submit_mutex = sceKernelCreateSema("FileReadSubmitMutex",
                                                 0, 1, 1, NULL);
    if (file_read_submit_mutex < 0) {
        DLOG("Error creating submit mutex: %s",
             psp_strerror(file_read_submit_mutex));
        file_read_submit_mutex = 0;
        goto error_delete_submit_event;
    }

    mem_clear(requests, sizeof(requests));
    for (int i = 0; i < lenof(requests); i++) {
        char namebuf[28];
        strformat(namebuf, sizeof(namebuf), "FileReadFlag%u", i);
        requests[i].event_flag = sceKernelCreateEventFlag(namebuf, 0, 0, 0);
        if (requests[i].event_flag < 0) {
            DLOG("Error creating event flag %u: %s", i,
                 psp_strerror(requests[i].event_flag));
            requests[i].event_flag = 0;
            goto error_delete_event_flags;
        }
    }

    stop_thread = 0;
    file_read_thread_handle = psp_start_thread(
        "FileReadThread", file_read_thread, THREADPRI_FILEIO, 0x1000, 0, NULL);
    if (file_read_thread_handle < 0) {
        goto error_delete_event_flags;
    }

    return 1;

  error_delete_event_flags:
    for (int i = 0; i < lenof(requests); i++) {
        if (requests[i].event_flag) {
            sceKernelDeleteEventFlag(requests[i].event_flag);
            requests[i].event_flag = 0;
        }
    }
    sceKernelDeleteSema(file_read_submit_mutex);
    file_read_submit_mutex = 0;
  error_delete_submit_event:
    sceKernelDeleteEventFlag(file_read_submit_event);
    file_read_submit_mutex = 0;
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

void psp_file_read_cleanup(void)
{
    stop_thread = 1;
    sceKernelSetEventFlag(file_read_submit_event, 1);
    while (!psp_delete_thread_if_stopped(file_read_thread_handle, NULL)) {
        sceKernelDelayThread(1000);
    }
    file_read_thread_handle = 0;

    for (int i = 0; i < lenof(requests); i++) {
        sceKernelDeleteEventFlag(requests[i].event_flag);
        requests[i].event_flag = 0;
    }

    sceKernelDeleteSema(file_read_submit_mutex);
    file_read_submit_mutex = 0;
}

/*-----------------------------------------------------------------------*/

int psp_file_read_submit(int fd, int64_t start, int32_t len, void *buf,
                         int timed, int32_t time_limit)
{
    PRECOND(fd >= 0, return 0);
    PRECOND(buf != NULL, return 0);
    PRECOND(!timed || time_limit >= 0, return 0);

    /* First calculate the absolute deadline for this request, so we use a
     * timestamp as close to the call time as possible.  (This value is
     * ignored for immediate requests.) */
    const int32_t deadline = sceKernelGetSystemTimeLow() + time_limit;

    /* Lock the mutex so we can safely allocate a request block.  (If
     * thread switching is locked, we skip this because (1) it's not
     * necessary and (2) the kernel will reset the device if we call this
     * function with threads locked, whether or not it would block.) */
    if (!psp_threads_locked()) {
        unsigned int timeout = SUBMIT_MUTEX_TIMEOUT;
        int res = sceKernelWaitSema(file_read_submit_mutex, 1, &timeout);
        if (res != 0) {
            DLOG("Failed to lock submit mutex: %s", psp_strerror(res));
            return 0;
        }
    }

    /* Find an unused request block for this request. */
    unsigned int index;
    for (index = 0; index < lenof(requests); index++) {
        if (!requests[index].inuse) {
            break;
        }
    }
    if (index >= lenof(requests)) {
        DLOG("No open request slots for: %d %lld %d %p %d %d",
             fd, start, len, buf, timed, time_limit);
        if (!psp_threads_locked()) {
            sceKernelSignalSema(file_read_submit_mutex, 1);
        }
        return 0;
    }

    /* Mark the request as used, and free the mutex immediately so other
     * threads can proceed.  (As long as the inuse flag is true, the
     * request block will not be reallocated by anyone else; however, we
     * make sure to clear the new flag so the read loop does not attempt
     * to process the request before all the data has been filled in.) */
    requests[index].inuse = 1;
    requests[index].new = 0;
    if (!psp_threads_locked()) {
        sceKernelSignalSema(file_read_submit_mutex, 1);
    }

    /* Initialize the request block with this request's data. */
    requests[index].timed    = (timed != 0);
    requests[index].finished = 0;
    requests[index].abort    = 0;
    requests[index].fd       = fd;
    requests[index].start    = start;
    requests[index].len      = len;
    requests[index].buf      = buf;
    requests[index].deadline = deadline;
    requests[index].waiter   = 0;
    sceKernelClearEventFlag(requests[index].event_flag, ~0);

    /* Flag the request for addition to the appropriate request list, and
     * signal the read loop thread in case it's asleep (due to having no
     * requests to process). */
    requests[index].new = 1;
    sceKernelSetEventFlag(file_read_submit_event, 1);

    /* Success!  Return the ID as one more than the array index (so the ID
     * is never zero). */
    return index+1;
}

/*-----------------------------------------------------------------------*/

int psp_file_read_check(int id)
{
    PRECOND(id > 0, return -1);
    const int index = id-1;
    if (index >= lenof(requests) || !requests[index].inuse) {
        return -1;
    }
    return requests[index].finished;
}

/*-----------------------------------------------------------------------*/

int psp_file_read_wait(int id)
{
    PRECOND(id > 0, return PSP_EINVAL);
    const int index = id-1;
    if (index >= lenof(requests) || !requests[index].inuse) {
        return PSP_EINVAL;
    }
    const SceUID this_thread = sceKernelGetThreadId();
    SceUID old_waiter;
    __asm__(".set push; .set noreorder; .set noat\n"
            "0: ll %0, %1\n"
            "move $at, %2\n"
            "sc $at, %1\n"
            "beqz $at, 0b\n"
            "nop\n"
            ".set pop"
            : "=&r" (old_waiter), "=m" (requests[index].waiter)
            : "r" (this_thread), "m" (requests[index].waiter)
            : "at");
    if (old_waiter) {
        DLOG("Two threads tried to sleep on request %d! old=%08X new=%08X",
             id, old_waiter, this_thread);
        /* We've overwritten the old waiter, but since other threads only
         * check whether the field is nonzero (and the waiting thread
         * doesn't care if the value changes after it enters its wait;
         * note that the waiter can't have already zeroed the field, since
         * that would have caused the store to fail), we don't need to try
         * and restore its original value. */
        return SCE_KERNEL_ERROR_ASYNC_BUSY;
    }
    requests[index].waiter = this_thread;
    sceKernelWaitEventFlag(requests[index].event_flag, 1,
                           PSP_EVENT_WAITCLEAR, NULL, NULL);
    int32_t retval = requests[index].res;
    BARRIER();
    requests[index].inuse = 0;
    return retval;
}

/*-----------------------------------------------------------------------*/

int psp_file_read_abort(int id)
{
    PRECOND(id > 0, return 0);
    const int index = id-1;
    if (index >= lenof(requests) || !requests[index].inuse) {
        return 0;
    }
    requests[index].abort = 1;
    return 1;
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static int file_read_thread(UNUSED SceSize args, UNUSED void *argp)
{
    /* Deadline priority mode flag and absolute timeout. */
    int priority_mode = 0;
    int32_t priority_timeout = 0;

    while (LIKELY(!stop_thread)) {

        /* Scan the list for new requests (which will be added to the
         * appropriate list).  We don't need a mutex because we lock with
         * the new and finished flags; see the documentation of the
         * request structure. */
        for (int i = 0; i < lenof(requests); i++) {
            if (UNLIKELY(requests[i].new)) {
                int16_t *nextptr;
                if (requests[i].timed) {
                    const int32_t this_deadline = requests[i].deadline;
                    nextptr = &first_timed;
                    while (*nextptr >= 0) {
                        /* We check the difference rather than comparing the
                         * values directly so that wraparound is handled
                         * correctly. */
                        const int32_t diff =
                            requests[*nextptr].deadline - this_deadline;
                        if (diff > 0) {  // next.deadline > this.deadline
                            break;
                        }
                        nextptr = &requests[*nextptr].next;
                    }
                } else {
                    nextptr = &first_immediate;
                    while (*nextptr >= 0) {
                        nextptr = &requests[*nextptr].next;
                    }
                }
                requests[i].next = *nextptr;
                *nextptr = i;
                requests[i].new = 0;
                requests[i].res = 0;
            }  // if (UNLIKELY(requests[i].new))
        }

        /* Save the current time (using a signed integer to avoid sign
         * issues in difference calculations). */
        const int32_t now = sceKernelGetSystemTimeLow();

        /* Check for timed requests whose deadlines have expired and
         * process them immediately. */
        while (first_timed >= 0 && (requests[first_timed].deadline - now) < 0){
            handle_request(&requests[first_timed], 1);
            first_timed = requests[first_timed].next;
            priority_timeout = sceKernelGetSystemTimeLow() + PRIORITY_TIME;
            priority_mode = 1;
        }

        /* If we're in deadline priority mode, don't attempt to process any
         * other requests (see algorithm documentation for details). */
        if (priority_mode) {
            if (priority_timeout - now > 0) {
                sceKernelDelayThread(PRIORITY_DELAY);
            } else {
                priority_mode = 0;
            }
            continue;
        }

        /* Find the highest-priority request and read a block for it. */
        if (first_immediate >= 0) {
            if (handle_request(&requests[first_immediate], 0)) {
                first_immediate = requests[first_immediate].next;
            }
        } else if (first_timed >= 0) {
            if (handle_request(&requests[first_timed], 0)) {
                first_timed = requests[first_timed].next;
            }
        } else {
            /* Nothing to do, so wait to be signalled. */
            sceKernelWaitEventFlag(file_read_submit_event, 1,
                                   PSP_EVENT_WAITCLEAR, NULL, NULL);
        }

        BARRIER();
    }  // while (!stop_thread)

    return 0;
}

/*-----------------------------------------------------------------------*/

static int handle_request(struct request *req, int all)
{
    if (req->abort) {
        req->res = PSP_ECANCELED;
        goto finish_request;
    }

    int32_t toread = req->len;
    if (!all) {
        toread = ubound(toread, BLOCKSIZE);
    }
    if (UNLIKELY(toread == 0)) {
        goto finish_request;
    }
    const int64_t seekpos = sceIoLseek(req->fd, req->start, PSP_SEEK_SET);
    if (UNLIKELY(seekpos != req->start)) {
        DLOG("Failed seeking to position %lld in file %d: %s",
             req->start, req->fd, psp_strerror((int32_t)seekpos));
        req->res = (int32_t)seekpos;
        goto finish_request;
    }
    const int32_t nread = sceIoRead(req->fd, req->buf, toread);
    if (UNLIKELY(nread != toread)) {
        if (nread < 0) {
            DLOG("Failed reading %d from position %lld in file %d: %s",
                 toread, req->start, req->fd, psp_strerror(nread));
            req->res = nread;
        } else {  // EOF
            req->res += nread;
        }
        goto finish_request;
    }
    req->start += toread;
    req->len   -= toread;
    req->buf   += toread;
    req->res   += toread;
    if (req->len == 0) {
        goto finish_request;
    }
    return 0;

  finish_request:;
    req->finished = 1;
    sceKernelSetEventFlag(req->event_flag, 1);
    return 1;
}

/*************************************************************************/
/*************************************************************************/
