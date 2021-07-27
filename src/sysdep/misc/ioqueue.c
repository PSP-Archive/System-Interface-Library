/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/misc/ioqueue.c: Asynchronous I/O functions.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/misc/ioqueue.h"
#include "src/thread.h"

#if defined(SIL_PLATFORM_ANDROID) || defined(SIL_PLATFORM_IOS) || defined(SIL_PLATFORM_LINUX) || defined(SIL_PLATFORM_MACOSX)
# define IS_POSIX  // For convenience.
#endif

#if defined(IS_POSIX)
# include <fcntl.h>
# include <sys/types.h>
# include <unistd.h>
#elif defined(SIL_PLATFORM_WINDOWS)
# include "src/sysdep/windows/internal.h"
# include "src/sysdep/windows/utf8-wrappers.h"
#endif

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Maximum number of bytes to read in a single read operation (to avoid
 * undue delay in responding to a deadline-enabled request). */
#define DEFAULT_READ_LIMIT  1048576
static int64_t read_limit = DEFAULT_READ_LIMIT;

/* Structure holding data for an I/O request. */
typedef struct IORequest IORequest;
struct IORequest {
    int index;             // Array index (constant for each element).
    int id;                // Request ID (currently always equal to index+1).
    int next_pending;      // Array index of next pending request.
    uint8_t in_use;        // True if this entry is in use.
    uint8_t complete;      // True if this request has completed.
    uint8_t cancelled;     // True if this request was cancelled.
    uint8_t has_deadline;  // True if this request has a start deadline.
    uint64_t deadline;     // Start deadline, in sys_time_now() units.

    /* Type of request and request details. */
    enum {IOQ_OPEN, IOQ_READ} type;
    union {
        struct {
            char *path;  // Locally-allocated copy.
            int flags;
        } open;
        struct {
            IOQHandle fd;
            /* These are modified during processing for large reads which
             * are broken into multiple operations. */
            void *buf;
            int64_t count;
            int64_t pos;
        } read;
    };

    /* Request result.
     * - For caller: Valid only when .complete is true.
     * - For I/O thread: The number of bytes read is accumulated here
     *   when performing split reads (see process_request()). */
    int64_t result;
    /* Error code (errno) from request.  Only valid if the request failed. */
    int error;
    /* Condition variable used to signal completion of the request. */
    SysCondVarID completion_event;
};

/* Array of IORequest structures, dynamically resized as necessary. */
static IORequest *requests;
static int requests_size;

/* Index of last used entry in requests[], or -1 if no entries are used. */
static int requests_last_used;

/* Index of first and last pending requests in the queue.  If the queue is
 * empty, both are set to -1. */
static int first_pending = -1, last_pending = -1;

/* Mutex for accessing the requests array and queue pointers.  The mutex
 * must be held as long a pointer to any array element is in use (since
 * the array may be reallocated by another thread at any time). */
static SysMutexID requests_mutex;


/* Thread ID for background I/O thread, or 0 if the thread has not yet
 * been started. */
static SysThreadID io_thread_id;

/* Condition variable used to signal that a new requests has been enqueued. */
static SysCondVarID enqueue_event;

/* Flag set by ioq_reset() to stop the background I/O thread. */
static uint8_t thread_stop_flag;

/*-----------------------------------------------------------------------*/

#ifdef SIL_INCLUDE_TESTS

/* Flags set by test control routines. */
static uint8_t move_on_realloc = 0;
static uint8_t block_io_thread = 0;
static uint8_t unblock_on_wait = 0;
static uint8_t step_io_thread = 0;
static uint8_t permfail_next_read = 0;
static uint8_t tempfail_next_read = 0;
static uint8_t iofail_next_read = 0;
static int block_io_thread_after = -1;

#endif

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * get_new_request:  Return the index of a currently unused request in the
 * request array.
 *
 * On entry, requests_mutex is assumed to be unlocked.  On successful
 * return, requests_mutex is locked.
 *
 * [Parameters]
 *     has_deadline: True if the request has a start deadline, false if not.
 *     deadline: Start deadline timestamp (ignored if has_deadline is false).
 * [Return value]
 *     requests[] array index of request, or -1 on error.
 */
static int get_new_request(int has_deadline, uint64_t deadline);

/**
 * get_request_by_id:  Return the index of the request with the given ID.
 *
 * On entry, requests_mutex is assumed to be unlocked.  On successful
 * return, requests_mutex is locked.
 *
 * [Parameters]
 *     id: Request ID (must be an in-use request block).
 * [Return value]
 *     requests[] array index of request, or -1 on error or invalid ID.
 */
static int get_request_by_id(int id);

/**
 * release_request:  Release (mark unused) the request with the given index.
 *
 * On entry, requests_mutex is assumed to be locked.
 *
 * [Parameters]
 *     index: requests[] index of request to release.
 */
static void release_request(int index);

/**
 * resize_requests:  Resize the requests[] array to the given size.
 *
 * On entry, requests_mutex is assumed to be locked.
 *
 * [Parameters]
 *     new_size: New size (number of entries) for requests[] array.
 */
static int resize_requests(int new_size);

/**
 * enqueue_request:  Enqueue the given request for background processing,
 * starting the background processing thread if necessary.  If the thread
 * cannot be started, the request is processed synchronously.
 *
 * On entry, requests_mutex is assumed to be locked.
 *
 * [Parameters]
 *     index: requests[] index of request to enqueue.
 */
static void enqueue_request(int index);

/**
 * dequeue_request:  Dequeue the first queued request and return its index.
 *
 * On entry, requests_mutex is assumed to be locked.
 *
 * [Return value]
 *     requests[] index of dequeued request, or -1 if no requests were queued.
 */
static int dequeue_request(void);

/**
 * process_request:  Process the given request.
 *
 * [Parameters]
 *     request: Request to process.
 * [Return value]
 *     False if the request is incomplete (a read request which has been
 *     split into multiple operations), true otherwise.
 */
static int process_request(IORequest *request);

/**
 * cancel_request:  Cancel the given request.
 *
 * [Parameters]
 *     request: Request to cancel.
 */
static void cancel_request(IORequest *request);

/**
 * deadline_to_timestamp:  Return the timestamp corresponding to the given
 * relative deadline (in seconds).
 */
static uint64_t deadline_to_timestamp(double deadline);

/**
 * io_thread:  Background thread which loops until thread_stop_flag becomes
 * true, dequeueing and performing I/O requests as they are queued.
 *
 * [Parameters]
 *     thread_start_event_ptr: Pointer to condition variable used to signal
 *         that the thread has started and locked requests_mutex.
 * [Return value]
 *     0
 */
static int io_thread(void *thread_start_event_ptr);

/**
 * start_io_thread:  Start the I/O thread running.
 *
 * On entry, requests_mutex is assumed to be locked.
 *
 * [Return value]
 *     True on success, false on error.
 */
static int start_io_thread(void);

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int ioq_init(void)
{
    requests_mutex = sys_mutex_create(0, 0);
    if (UNLIKELY(!requests_mutex)) {
        DLOG("Failed to create requests_mutex");
        goto error_return;
    }

    enqueue_event = sys_condvar_create();
    if (UNLIKELY(!enqueue_event)) {
        DLOG("Failed to create enqueue_event");
        goto error_destroy_requests_mutex;
    }

    return 1;

  error_destroy_requests_mutex:
    sys_mutex_destroy(requests_mutex);
    requests_mutex = 0;
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

void ioq_set_read_limit(int64_t limit)
{
    PRECOND(limit > 0, return);
    read_limit = limit;
}

/*-----------------------------------------------------------------------*/

int ioq_open(const char *path, int flags, double deadline)
{
    const int has_deadline = (deadline >= 0);
    const uint64_t deadline_ts = deadline_to_timestamp(deadline);

    if (!path) {
        errno = EINVAL;
        return 0;
    }
#ifdef IS_POSIX
    if ((flags & O_ACCMODE) != O_RDONLY) {
        errno = EINVAL;
        return 0;
    }
#endif

    char * const path_copy = mem_strdup(path, MEM_ALLOC_TEMP);
    if (UNLIKELY(!path_copy)) {
        DLOG("Failed to copy path %s", path);
        errno = ENOMEM;
        return 0;
    }

    const int index = get_new_request(has_deadline, deadline_ts);
    if (UNLIKELY(index < 0)) {
        DLOG("Failed to get a request block");
        mem_free(path_copy);
        errno = ENOMEM;
        return 0;
    }

    requests[index].type       = IOQ_OPEN;
    requests[index].open.path  = path_copy;
    requests[index].open.flags = flags;
    enqueue_request(index);

    const int id = requests[index].id;
    sys_mutex_unlock(requests_mutex);
    return id;
}

/*-----------------------------------------------------------------------*/

int ioq_read(IOQHandle fd, void *buf, int64_t count, int64_t pos,
             double deadline)
{
    const int has_deadline = (deadline >= 0);
    const uint64_t deadline_ts = deadline_to_timestamp(deadline);

    if (fd == IOQHANDLE_INVALID || buf == NULL || count < 0 || pos < 0) {
        errno = EINVAL;
        return 0;
    }

#ifdef SIL_INCLUDE_TESTS
    if (permfail_next_read) {
        DLOG("Force-failing request with ENOMEM");
        errno = ENOMEM;
        permfail_next_read = 0;
        return 0;
    }
    if (tempfail_next_read) {
        DLOG("Force-failing request with EAGAIN");
        errno = EAGAIN;
        tempfail_next_read = 0;
        return 0;
    }
#endif

    const int index = get_new_request(has_deadline, deadline_ts);
    if (UNLIKELY(index < 0)) {
        DLOG("Failed to get a request block");
        errno = ENOMEM;
        return 0;
    }

    requests[index].type       = IOQ_READ;
    requests[index].read.fd    = fd;
    requests[index].read.buf   = buf;
    requests[index].read.count = count;
    requests[index].read.pos   = pos;
    enqueue_request(index);

    const int id = requests[index].id;
    sys_mutex_unlock(requests_mutex);
    return id;
}

/*-----------------------------------------------------------------------*/

int ioq_poll(int request)
{
    const int index = get_request_by_id(request);
    if (UNLIKELY(index < 0)) {
        DLOG("Invalid request ID: %d", request);
        errno = ESRCH;
        return 1;
    }

    const int complete = requests[index].complete;

    sys_mutex_unlock(requests_mutex);
    return complete;
}

/*-----------------------------------------------------------------------*/

int64_t ioq_wait(int request, int *error_ret)
{
    const int saved_errno = errno;

    const int index = get_request_by_id(request);
    if (UNLIKELY(index < 0)) {
        DLOG("Invalid request ID: %d", request);
        errno = ESRCH;
        if (error_ret) {
            *error_ret = 0;
        }
        return -1;
    }

#ifdef SIL_INCLUDE_TESTS
    if (unblock_on_wait && block_io_thread) {
        block_io_thread_after = index;
        BARRIER();
        block_io_thread = 0;
        BARRIER();
    }
#endif

    while (!requests[index].complete) {
        sys_condvar_wait(requests[index].completion_event, requests_mutex, -1);
    }

    if (requests[index].type == IOQ_OPEN) {
        mem_free(requests[index].open.path);
        requests[index].open.path = NULL;
    }

    const int64_t result = requests[index].result;
    if (error_ret) {
        *error_ret = requests[index].error;
    }
    release_request(index);

    sys_mutex_unlock(requests_mutex);
    errno = saved_errno;
    return result;
}

/*-----------------------------------------------------------------------*/

void ioq_cancel(int request)
{
    const int index = get_request_by_id(request);
    if (UNLIKELY(index < 0)) {
        DLOG("Invalid request ID: %d", request);
        errno = ESRCH;
        return;
    }

    cancel_request(&requests[index]);

    sys_mutex_unlock(requests_mutex);
}

/*-----------------------------------------------------------------------*/

void ioq_cancel_fd(IOQHandle fd)
{
    sys_mutex_lock(requests_mutex, -1);

    for (int index = 0; index < requests_size; index++) {
        if (requests[index].type == IOQ_READ
         && requests[index].read.fd == fd) {
            cancel_request(&requests[index]);
        }
    }

    sys_mutex_unlock(requests_mutex);
}

/*-----------------------------------------------------------------------*/

void ioq_reset(void)
{
    if (!requests_mutex) {
        return;  // ioq_init() never succeeded.
    }

#ifdef SIL_INCLUDE_TESTS
    sys_mutex_lock(requests_mutex, -1);
    block_io_thread = 0;
    unblock_on_wait = 0;
    step_io_thread = 0;
    block_io_thread_after = -1;
    sys_mutex_unlock(requests_mutex);
#endif

    if (io_thread_id) {
        sys_mutex_lock(requests_mutex, -1);
        thread_stop_flag = 1;
        sys_condvar_signal(enqueue_event, 1);
        sys_mutex_unlock(requests_mutex);
        sys_thread_wait(io_thread_id, (int[1]){0});
        thread_stop_flag = 0;
        io_thread_id = 0;
    }

    resize_requests(0);
    first_pending = last_pending = -1;
    read_limit = DEFAULT_READ_LIMIT;
    sys_condvar_destroy(enqueue_event);
    enqueue_event = 0;
    sys_mutex_destroy(requests_mutex);
    requests_mutex = 0;
}

/*************************************************************************/
/************************* Test control routines *************************/
/*************************************************************************/

#ifdef SIL_INCLUDE_TESTS

/*-----------------------------------------------------------------------*/

void TEST_misc_ioqueue_move_on_realloc(int enable)
{
    move_on_realloc = (enable != 0);
}

/*-----------------------------------------------------------------------*/

void TEST_misc_ioqueue_block_io_thread(int enable)
{
    if (enable) {
        block_io_thread = 1;
    } else {
        /* Need to lock the mutex to ensure these are changed atomically
         * with respect to the I/O thread. */
        sys_mutex_lock(requests_mutex, -1);
        block_io_thread_after = -1;
        block_io_thread = 0;
        sys_mutex_unlock(requests_mutex);
    }
}

/*-----------------------------------------------------------------------*/

void TEST_misc_ioqueue_unblock_on_wait(int enable)
{
    unblock_on_wait = (enable != 0);
}

/*-----------------------------------------------------------------------*/

void TEST_misc_ioqueue_step_io_thread(void)
{
    step_io_thread = 1;
    while (step_io_thread) {
        BARRIER();
        sys_thread_yield();
    }
}

/*-----------------------------------------------------------------------*/

void TEST_misc_ioqueue_permfail_next_read(int enable)
{
    permfail_next_read = (enable != 0);
}

/*-----------------------------------------------------------------------*/

void TEST_misc_ioqueue_tempfail_next_read(int enable)
{
    tempfail_next_read = (enable != 0);
}

/*-----------------------------------------------------------------------*/

void TEST_misc_ioqueue_iofail_next_read(int enable)
{
    iofail_next_read = (enable != 0);
}

/*-----------------------------------------------------------------------*/

#endif  // SIL_INCLUDE_TESTS

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static int get_new_request(int has_deadline, uint64_t deadline)
{
    sys_mutex_lock(requests_mutex, -1);

    int index;
    for (index = 0; index < requests_size; index++) {
        if (!requests[index].in_use) {
            break;
        }
    }
    if (index >= requests_size) {
        if (!resize_requests(index + 1)) {
            sys_mutex_unlock(requests_mutex);
            return -1;
        }
    }
    if (index > requests_last_used) {
        requests_last_used = index;
    }

    requests[index].in_use       = 1;
    requests[index].complete     = 0;
    requests[index].cancelled    = 0;
    requests[index].has_deadline = has_deadline;
    requests[index].deadline     = deadline;
    requests[index].result       = 0;
    requests[index].error        = 0;
    return index;
}

/*-----------------------------------------------------------------------*/

static int get_request_by_id(int id)
{
    if (id < 1 || id > requests_size) {
        return -1;
    }

    sys_mutex_lock(requests_mutex, -1);

    const int index = id - 1;
    if (!requests[index].in_use) {
        DLOG("Request ID %d is valid but not in use", id);
        sys_mutex_unlock(requests_mutex);
        return -1;
    }

    return index;
}

/*-----------------------------------------------------------------------*/

static void release_request(int index)
{
    PRECOND(index >= 0 && index < requests_size, return);

    requests[index].in_use = 0;
    if (index == requests_last_used) {
        for (; index >= 0; index--) {
            if (requests[index].in_use) {
                break;
            }
        }
        requests_last_used = index;
        const int required_size = index + 1;
        /* Shrink the array if a significant portion of it is unused, but
         * always leave a few entries allocated so we're not repeatedly
         * allocating and freeing for solitary requests. */
        if (required_size + 5 <= requests_size/2) {
            resize_requests(required_size + 5);
        }
    }
}

/*-----------------------------------------------------------------------*/

static int resize_requests(int new_size)
{
    PRECOND(new_size >= 0);

    /* Free resources associated with request blocks that are about to be
     * deallocated (when shrinking or freeing the array). */
    for (int index = new_size; index < requests_size; index++) {
        if (requests[index].in_use) {
            if (requests[index].type == IOQ_OPEN) {
                if (requests[index].complete) {
                    IOQHandle fd = RESULT_TO_IOQHANDLE(requests[index].result);
                    if (fd != IOQHANDLE_INVALID) {
#if defined(IS_POSIX)
                        close(fd);
#elif defined(SIL_PLATFORM_WINDOWS)
                        CloseHandle(fd);
#endif
                    }
                }
                mem_free(requests[index].open.path);
            }
        }
        sys_condvar_destroy(requests[index].completion_event);
    }

    /* If reallocating to zero length, just free the array and return. */
    if (new_size == 0) {
        mem_free(requests);
        requests = NULL;
        requests_size = 0;
        return 1;
    }

    /* Reallocate the array to its new size.  If reallocation fails when
     * shrinking the array, we proceed with the original pointer and
     * pretend the shrink succeeded. */
    IORequest *new_requests =
#ifdef SIL_INCLUDE_TESTS
        move_on_realloc ? mem_alloc(sizeof(*requests) * new_size, 0, 0) :
#endif
        mem_realloc(requests, sizeof(*requests) * new_size, 0);
    if (!new_requests) {
        if (UNLIKELY(new_size > requests_size)) {
            DLOG("Failed to expand requests array to %d entries", new_size);
            return 0;
        } else {
            new_requests = requests;
        }
    }
#ifdef SIL_INCLUDE_TESTS
    else if (move_on_realloc) {
        memcpy(new_requests, requests,
               sizeof(*requests) * min(requests_size, new_size));
        mem_free(requests);
    }
#endif
    requests = new_requests;
    const int old_size = requests_size;
    requests_size = new_size;

    /* Initialize and allocate resources for new array entries (when
     * expanding the array). */
    for (int i = old_size; i < requests_size; i++) {
        requests[i].index = i;
        requests[i].id = requests[i].index + 1;
        requests[i].next_pending = -1;
        requests[i].in_use = 0;
        requests[i].completion_event = sys_condvar_create();
        if (UNLIKELY(!requests[i].completion_event)) {
            DLOG("Failed to create completion condvar for index %d", i);
            requests_size = i;
            if (requests_size == 0) {
                mem_free(requests);
                requests = NULL;
            } else {
                new_requests = mem_realloc(
                    requests, sizeof(*requests) * requests_size, 0);
                if (new_requests) {
                    requests = new_requests;
                }
            }
            return 0;
        }
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

static void enqueue_request(int index)
{
    PRECOND(index >= 0 && index < requests_size, return);

    if (!io_thread_id) {
        if (UNLIKELY(!start_io_thread())) {
            while (!process_request(&requests[index])) { /*spin*/ }
            requests[index].complete = 1;
            sys_condvar_signal(requests[index].completion_event, 1);
            return;
        }
    }

    if (last_pending < 0) {
        first_pending = index;
    } else {
        requests[last_pending].next_pending = index;
    }
    last_pending = index;
    requests[index].next_pending = -1;

    sys_condvar_signal(enqueue_event, 1);
}

/*-----------------------------------------------------------------------*/

static int dequeue_request(void)
{
    int dequeued = first_pending;
    int dequeued_prev = -1;
    /* We assume the time required to iterate over the entire array is
     * insignificant compared to the time actually spent on I/O, so we go
     * with a simple linear search here to look for requests to prioritize. */
    const uint64_t now = sys_time_now();
    int64_t best_diff = INT64_MAX;
    for (int i = first_pending, prev = -1; i != -1;
         prev = i, i = requests[i].next_pending)
    {
        if (requests[i].has_deadline) {
            int64_t diff;
            if (requests[i].deadline - now > INT64_MAX) {
                /* The deadline has already passed!  Compute the time
                 * difference separately to avoid implementation-defined
                 * behavior on signed integer overflow. */
                diff = -(int64_t)(now - requests[i].deadline);
            } else {
                diff = requests[i].deadline - now;
            }
            if (diff < best_diff) {
                best_diff = diff;
                dequeued = i;
                dequeued_prev = prev;
            }
        }
    }

    if (dequeued >= 0) {
        if (dequeued_prev >= 0) {
            requests[dequeued_prev].next_pending =
                requests[dequeued].next_pending;
        } else {
            first_pending = requests[dequeued].next_pending;
        }
        if (last_pending == dequeued) {
            last_pending = dequeued_prev;
        }
        requests[dequeued].next_pending = -1;
    }

    return dequeued;
}

/*-----------------------------------------------------------------------*/

static int process_request(IORequest *request)
{
    PRECOND(request != NULL, return -1);

    switch (request->type) {

      case IOQ_OPEN:
#if defined(IS_POSIX)
        request->result = open(request->open.path, request->open.flags, 0666);
        request->error = errno;
#elif defined(SIL_PLATFORM_WINDOWS)
        /* Note: open.flags ignored for now, since we don't use this anyway. */
        request->result = (int64_t)(intptr_t)CreateFile(
            request->open.path, GENERIC_READ, FILE_SHARE_READ, NULL,
            OPEN_EXISTING, 0, NULL);
        request->error = GetLastError();
#endif
        return 1;

      case IOQ_READ: {
        const int64_t this_count = ubound(request->read.count, read_limit);
        int64_t this_result;
#ifdef SIL_INCLUDE_TESTS
        if (iofail_next_read) {
            this_result = -1;
# if defined(IS_POSIX)
            request->error = EIO;
# elif defined(SIL_PLATFORM_WINDOWS)
            request->error = ERROR_GEN_FAILURE;
# endif
            iofail_next_read = 0;
        } else
#endif
        {
#if defined(IS_POSIX)
            this_result = pread(request->read.fd, request->read.buf,
                                this_count, request->read.pos);
            request->error = errno;
#elif defined(SIL_PLATFORM_WINDOWS)
            OVERLAPPED overlapped;
            mem_clear(&overlapped, sizeof(overlapped));
            overlapped.Offset = (DWORD)(request->read.pos & 0xFFFFFFFF);
            overlapped.OffsetHigh = (DWORD)(request->read.pos >> 32);
            DWORD nread;
            if (ReadFile(request->read.fd, request->read.buf, this_count,
                         &nread, &overlapped)) {
                this_result = nread;
            } else {
                const DWORD error = GetLastError();
                if (error == ERROR_HANDLE_EOF) {
                    this_result = 0;
                } else {
                    this_result = -1;
                    request->error = error;
                }
            }
#endif
        }
        if (this_result < 0) {
            if (request->result == 0) {
                request->result = -1;
            }
        } else {
            request->result += this_result;
            if (this_result == this_count && this_count < request->read.count) {
                request->read.buf = (char *)request->read.buf + this_count;
                request->read.pos += this_count;
                request->read.count -= this_count;
                return 0;
            }
        }
        return 1;
      }  // case IOQ_READ

    }  // switch (request->type)

    UNREACHABLE;
}

/*-----------------------------------------------------------------------*/

static void cancel_request(IORequest *request)
{
    if (request->complete) {
        if (request->type == IOQ_OPEN) {
            IOQHandle fd = RESULT_TO_IOQHANDLE(request->result);
            if (fd != IOQHANDLE_INVALID) {
#if defined(IS_POSIX)
                close(fd);
#elif defined(SIL_PLATFORM_WINDOWS)
                CloseHandle(fd);
#endif
            }
        }
    } else {
        /* Skip over this request when it's dequeued. */
        request->cancelled = 1;
    }
    request->result = -1;
#if defined(IS_POSIX)
    request->error = ECANCELED;
#elif defined(SIL_PLATFORM_WINDOWS)
    request->error = ERROR_OPERATION_ABORTED;
#endif
}

/*-----------------------------------------------------------------------*/

static uint64_t deadline_to_timestamp(double deadline)
{
    return sys_time_now() + (uint64_t)(deadline * sys_time_unit());
}

/*************************************************************************/
/************************* Background I/O thread *************************/
/*************************************************************************/

static int io_thread(void *thread_start_event_ptr)
{
    sys_mutex_lock(requests_mutex, -1);
    sys_condvar_signal(*(SysCondVarID *)thread_start_event_ptr, 1);

    while (!thread_stop_flag) {
#ifdef SIL_INCLUDE_TESTS
        BARRIER();
        if (block_io_thread && !step_io_thread) {
            sys_mutex_unlock(requests_mutex);
            sys_time_delay(sys_time_unit() / 1000);  // 1 msec
            sys_mutex_lock(requests_mutex, -1);
            continue;
        }
#endif
        const int index = dequeue_request();
        if (index < 0) {
            sys_condvar_wait(enqueue_event, requests_mutex, -1);
        } else {
            int complete = 1;
            if (!requests[index].cancelled) {
                /* Copy the request so we can run it safely without the mutex
                 * held.  (process_request() doesn't modify the request's
                 * in-use or completion status so it wouldn't conflict with
                 * any API calls, but the request array itself might get
                 * moved in memory if it has to be expanded, so we can't
                 * touch the array without the mutex locked.) */
                IORequest request = requests[index];
                sys_mutex_unlock(requests_mutex);
                complete = process_request(&request);
                sys_mutex_lock(requests_mutex, -1);
                requests[index].read.buf = request.read.buf;
                requests[index].read.pos = request.read.pos;
                requests[index].read.count = request.read.count;
                requests[index].result = request.result;
                requests[index].error = request.error;
            }
            if (complete) {
                requests[index].complete = 1;
                sys_condvar_signal(requests[index].completion_event, 1);
#ifdef SIL_INCLUDE_TESTS
                if (index == block_io_thread_after) {
                    block_io_thread_after = -1;
                    block_io_thread = 1;
                }
#endif
            } else {
                /* Stick it back in the front of the queue for next time. */
                requests[index].next_pending = first_pending;
                first_pending = index;
                if (last_pending < 0) {
                    last_pending = index;
                }
            }
        }
#ifdef SIL_INCLUDE_TESTS
        if (step_io_thread) {
            step_io_thread = 0;
        }
#endif
    }

    sys_mutex_unlock(requests_mutex);
    return 0;
}

/*-----------------------------------------------------------------------*/

static int start_io_thread(void)
{
    int ok = 0;

    SysCondVarID thread_start_event = sys_condvar_create();
    if (LIKELY(thread_start_event)) {
        static const ThreadAttributes attr;  // All zero.
        io_thread_id = sys_thread_create(&attr, io_thread, &thread_start_event);
        if (LIKELY(io_thread_id)) {
            sys_condvar_wait(thread_start_event, requests_mutex, -1);
            ok = 1;
        }
        sys_condvar_destroy(thread_start_event);
    }

    if (ok) {
        return 1;
    } else {
        static int warned = 0;
        if (!warned) {
            DLOG("Failed to create background I/O thread");
            warned = 1;
        }
        return 0;
    }
}

/*************************************************************************/
/*************************************************************************/
