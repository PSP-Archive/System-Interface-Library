/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/windows/thread.c: Thread management for Windows.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/windows/internal.h"
#include "src/thread.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Structure to hold thread data.  A pointer to this structure is returned
 * as the thread handle.  (If not for CALLBACK, i.e. __stdcall, we could
 * just pass the function pointer directly to CreateThread().  Sigh...) */

typedef struct SysThread SysThread;
struct SysThread {
    /* Windows thread handle. */
    HANDLE handle;
    /* Windows thread ID, as returned by GetCurrentThreadId().  Set by the
     * thread runner.  (This is needed because GetThreadId() is missing
     * from Windows XP.) */
    DWORD id;

    /* Function to call, and its parameter. */
    int (*function)(void *param);
    void *param;
};


/* Local storage key for storing the current thread's SysThread pointer
 * (so we can return it from sys_thread_get_id()). */
static DWORD sys_thread_key;

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * create_key:  Create the local storage key for storing the current
 * thread's SysThread pointer if it has not already been created.
 *
 * This function may be safely called from multiple threads, and the local
 * storage key will only be created once.
 *
 * [Return value]
 *     True if the key was successfully created, false if not.
 */
static int create_key(void);

/**
 * get_core_mask:  Return a Windows-format affinity bitmask containing all
 * available processor cores.
 */
static DWORD_PTR get_core_mask(void);

/**
 * affinity_to_windows:  Convert a 64-bit affinity mask to a Windows
 * thread affinity mask.
 */
static DWORD_PTR affinity_to_windows(uint64_t affinity);

/**
 * affinity_from_windows:  Convert a Windows thread affinity mask to a
 * 64-bit affinity mask.
 */
static uint64_t affinity_from_windows(DWORD_PTR affinity);

/**
 * thread_runner:  Wrapper for threads started with sys_thread_create()
 * which handles translating between standard and Windows calling conventions.
 *
 * [Parameters]
 *     param: Thread parameter (points to a SysThread structure).
 * [Return value]
 *     Thread function return value.
 */
static DWORD CALLBACK thread_runner(LPVOID param);

/**
 * cleanup_thread:  Perform any necessary cleanup before a thread exits.
 */
static void cleanup_thread(void);

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int sys_thread_get_num_cores(void)
{
    DWORD_PTR all_cores = get_core_mask();
    int num_cores = 0;
    for (; all_cores; all_cores >>= 1) {
        num_cores += all_cores & 1;
    }
    ASSERT(num_cores > 0, num_cores = 1);
    return num_cores;
}

/*-----------------------------------------------------------------------*/

SysThreadID sys_thread_create(
    const ThreadAttributes *attr, int (*function)(void *), void *param)
{
    int priority = attr->priority;
    if (priority != THREAD_PRIORITY_IDLE
     && priority != THREAD_PRIORITY_TIME_CRITICAL) {
        if (priority < THREAD_PRIORITY_LOWEST) {
            DLOG("Thread priority %d not supported, clamping to %d",
                 priority, THREAD_PRIORITY_LOWEST);
            priority = THREAD_PRIORITY_LOWEST;
        } else if (priority > THREAD_PRIORITY_HIGHEST) {
            DLOG("Thread priority %d not supported, clamping to %d",
                 priority, THREAD_PRIORITY_HIGHEST);
            priority = THREAD_PRIORITY_HIGHEST;
        }
    }

    if (!create_key()) {
        DLOG("Unable to create TLS key for thread ID");
        return 0;
    }

    SysThread *thread = mem_alloc(sizeof(*thread), 0, 0);
    if (!thread) {
        DLOG("No memory for thread structure");
        return 0;
    }
    thread->function = function;
    thread->param = param;
    /* Set thread->id to an invalid value to avoid spurious wait errors if
     * the caller tries to wait for the thread before the thread sets
     * thread->id.  Microsoft's documentation explicitly says that thread
     * IDs will never be zero. */
    thread->id = 0;
    thread->handle = CreateThread(NULL, attr->stack_size, thread_runner,
                                  thread, CREATE_SUSPENDED, NULL);
    if (!thread->handle) {
        DLOG("Failed to create thread for %p(%p): %s", function, param,
             windows_strerror(GetLastError()));
        mem_free(thread);
        return 0;
    }
    if (!SetThreadPriority(thread->handle, priority)) {
        DLOG("Failed to set thread priority for %p(%p) to %d (running"
             " anyway): %s", function, param, priority,
             windows_strerror(GetLastError()));
    }
    if (attr->affinity) {
        if (!SetThreadAffinityMask(thread->handle,
                                   affinity_to_windows(attr->affinity))) {
            DLOG("Failed to set thread affinity for %p(%p) to 0x%llX (running"
                 " anyway): %s", function, param, (long long)attr->affinity,
                 windows_strerror(GetLastError()));
        }
    }
    if (!ResumeThread(thread->handle)) {
        DLOG("Failed to run thread for %p(%p): %s", function, param,
             windows_strerror(GetLastError()));
        CloseHandle(thread->handle);
        mem_free(thread);
        return 0;
    }
    return (SysThreadID)thread;
}

/*-----------------------------------------------------------------------*/

void sys_thread_exit(int exit_code)
{
    cleanup_thread();
    ExitThread(exit_code);
}

/*-----------------------------------------------------------------------*/

SysThreadID sys_thread_get_id(void)
{
    return (SysThreadID)TlsGetValue(sys_thread_key);
}

/*-----------------------------------------------------------------------*/

int sys_thread_get_priority(void)
{
    return GetThreadPriority(GetCurrentThread());
}

/*-----------------------------------------------------------------------*/

int sys_thread_set_affinity(uint64_t affinity)
{
    if (UNLIKELY(!SetThreadAffinityMask(GetCurrentThread(),
                                        affinity_to_windows(affinity)))) {
        DLOG("Failed to set thread affinity: %s",
             windows_strerror(GetLastError()));
        return 0;
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

uint64_t sys_thread_get_affinity(void)
{
    /* Windows is missing GetThreadAffinityMask(), so we have to fake it. */
    DWORD_PTR process_affinity = 0;
    ASSERT(GetProcessAffinityMask(GetCurrentProcess(), &process_affinity,
                                  (DWORD_PTR[1]){0}));
    if (UNLIKELY(!process_affinity)) {
        DLOG("Failed to get process affinity mask: %s",
             windows_strerror(GetLastError()));
        return ~UINT64_C(0);
    }
    const HANDLE thread = GetCurrentThread();
    const DWORD_PTR affinity = SetThreadAffinityMask(thread, process_affinity);
    if (UNLIKELY(!affinity)) {
        DLOG("Failed to get thread affinity mask: %s",
             windows_strerror(GetLastError()));
        return ~UINT64_C(0);
    }
    ASSERT(SetThreadAffinityMask(thread, affinity) != 0);
    return affinity_from_windows(affinity);
}

/*-----------------------------------------------------------------------*/

int sys_thread_is_running(SysThreadID thread_)
{
    SysThread *thread = (SysThread *)thread_;
    return WaitForSingleObject(thread->handle, 0) != WAIT_OBJECT_0;
}

/*-----------------------------------------------------------------------*/

int sys_thread_wait(SysThreadID thread_, int *result_ret)
{
    SysThread *thread = (SysThread *)thread_;

    /* WaitForSingleObject() won't protect against trying to wait for the
     * current thread, so we need to check manually whether the target
     * thread is the same as the current thread.  Note that while we don't
     * wait for thread->id to be updated before returning from
     * sys_thread_create(), this condition can only be true when called
     * from the thread that set thread->id in the first place, so locking
     * isn't necessary to get correct behavior. */
    if (thread->id == GetCurrentThreadId()) {
        DLOG("Attempted to wait for current thread!");
        return 0;
    }

    if (WaitForSingleObject(thread->handle, INFINITE) == WAIT_FAILED) {
        DLOG("Failed to wait for thread: %s",
             windows_strerror(GetLastError()));
        return 0;
    }

    DWORD result = 0;
    if (!GetExitCodeThread(thread->handle, &result)) {
        DLOG("Failed to get thread exit code for thread %p: %s",
             thread->handle, windows_strerror(GetLastError()));
    }
    CloseHandle(thread->handle);
    mem_free(thread);
    *result_ret = (int)result;
    return 1;
}

/*-----------------------------------------------------------------------*/

void sys_thread_yield()
{
    SwitchToThread();
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static int create_key(void)
{
    static uint8_t sys_thread_key_created = 0;
    static SysMutexID create_mutex = 0;  // This will leak, unfortunately.

    if (sys_thread_key_created) {
        return 1;
    }

    if (!create_mutex) {
        const SysMutexID new_mutex = sys_mutex_create(0, 0);
        /* SysMutexID is uintptr_t, so we use ...Pointer() on it. */
        if (InterlockedCompareExchangePointer(
                (void **)&create_mutex, (void *)new_mutex, 0) != 0) {
            /* Somebody else already created the mutex, so use that one. */
            sys_mutex_destroy(new_mutex);
        }
    }

    sys_mutex_lock(create_mutex, -1);
    if (!sys_thread_key_created) {
        sys_thread_key = TlsAlloc();
        if (sys_thread_key == TLS_OUT_OF_INDEXES) {
            DLOG("Failed to create local storage key: No slots available");
        } else {
            sys_thread_key_created = 1;
        }
    }
    sys_mutex_unlock(create_mutex);

    return sys_thread_key_created;
}

/*-----------------------------------------------------------------------*/

static DWORD_PTR get_core_mask(void)
{
    DWORD_PTR core_mask = 0;
    ASSERT(GetProcessAffinityMask(GetCurrentProcess(), &core_mask,
                                  (DWORD_PTR[1]){0}));
    ASSERT(core_mask != 0, core_mask = 1);
    return core_mask;
}

/*-----------------------------------------------------------------------*/

DWORD_PTR affinity_to_windows(uint64_t affinity)
{
    const DWORD_PTR core_mask = get_core_mask();
    DWORD_PTR core_bit = 1;
    DWORD_PTR windows_affinity = 0;
    for (; affinity; affinity >>= 1, core_bit <<= 1) {
        for (; !(core_mask & core_bit); core_bit <<= 1) {
            if (!core_bit) {
                break;
            }
        }
        if (affinity & 1) {
            windows_affinity |= core_bit;
        }
    }
    return windows_affinity;
}

/*-----------------------------------------------------------------------*/

uint64_t affinity_from_windows(DWORD_PTR affinity)
{
    const DWORD_PTR core_mask = get_core_mask();
    DWORD_PTR core_bit = 1;
    uint64_t u64_affinity = 0;
    uint64_t u64_bit = 1;
    for (; affinity; affinity >>= 1, core_bit <<= 1, u64_bit <<= 1) {
        for (; !(core_mask & core_bit); affinity >>= 1, core_bit <<= 1) {
            if (!core_bit) {
                break;
            }
        }
        if (affinity & 1) {
            u64_affinity |= u64_bit;
        }
    }
    return u64_affinity;
}

/*-----------------------------------------------------------------------*/

static DWORD CALLBACK thread_runner(LPVOID param)
{
    SysThread *thread = (SysThread *)param;
    thread->id = GetCurrentThreadId();

    if (!TlsSetValue(sys_thread_key, thread)) {
        DLOG("Failed to store thread ID: %s",
             windows_strerror(GetLastError()));
        return 0;
    }

    int result = (*thread->function)(thread->param);

    cleanup_thread();
    return (DWORD)result;
}

/*-----------------------------------------------------------------------*/

static void cleanup_thread(void)
{
    /* Destroy the GL context if one exists and it's not the main
     * rendering context for the window (which should never be the case
     * in a subthread, but it can't hurt to play it safe). */
    HGLRC gl_context = wglGetCurrentContext();
    if (gl_context && gl_context != windows_wgl_context()) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(gl_context);
    }
}

/*************************************************************************/
/*************************************************************************/
