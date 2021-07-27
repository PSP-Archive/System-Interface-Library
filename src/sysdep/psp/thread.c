/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/thread.c: Thread management routines for the PSP.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/psp/internal.h"
#include "src/sysdep/psp/thread.h"
#include "src/thread.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Data structure for a thread. */
typedef struct SysThread SysThread;
struct SysThread {
    /* Thread ID. */
    SceUID thread;
    /* Function to call and parameter value to pass. */
    int (*function)(void *param);
    void *param;
    /* Buffer for function's return value.  We need to use this instead of
     * just returning the value directly because the kernel interprets a
     * negative return value as an error and overwrites it with its own
     * error code. */
    int result;
};

/*-----------------------------------------------------------------------*/

/* Lock nesting count for psp_threads_lock(). */
static unsigned int lock_count;

/* Saved interrupt register value for thread locking. */
static unsigned int lock_intstatus;

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * thread_runner:  Wrapper for threads started with sys_thread_create()
 * which handles interface translation between sysdep and the PSP system
 * call.
 *
 * [Parameters]
 *     args: Thread argument size (== sizeof(ThreadRunParam)).
 *     argp: Thread argument pointer (points to a ThreadRunParam structure).
 * [Return value]
 *     Thread function return value.
 */
static int thread_runner(SceSize args, void *argp);

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int sys_thread_get_num_cores(void)
{
    return 1;
}

/*-----------------------------------------------------------------------*/

SysThreadID sys_thread_create(
    const ThreadAttributes *attr, int (*function)(void *), void *param)
{
    /* On the PSP, lower value = higher priority. */
    const int max_priority = THREADPRI_MAIN - THREADPRI_USER_MIN;
    const int min_priority = THREADPRI_MAIN - THREADPRI_USER_MAX;
    int priority = attr->priority;
    if (priority < min_priority) {
        DLOG("Thread priority %d out of range, forcing to %d",
             priority, min_priority);
        priority = min_priority;
    } else if (priority > max_priority) {
        DLOG("Thread priority %d out of range, forcing to %d",
             priority, max_priority);
        priority = max_priority;
    }

    /* We can't support custom thread names on PSP because we need to use
     * the thread name to carry the thread ID (due to a lack of TLS). */
    return psp_thread_create_named(NULL, priority, attr->stack_size,
                                   function, param);
}

/*-----------------------------------------------------------------------*/

void sys_thread_exit(int exit_code)
{
    SysThread *thread = (SysThread *)sys_thread_get_id();
    if (thread) {
        thread->result = exit_code;
    } else {
        DLOG("Calling sys_thread_exit() from unknown thread (ID 0x%X),"
             " exit code %d will be lost", sceKernelGetThreadId(), exit_code);
    }
    sceKernelExitThread(exit_code);
}

/*-----------------------------------------------------------------------*/

SysThreadID sys_thread_get_id(void)
{
    SceKernelThreadInfo thinfo;
    mem_clear(&thinfo, sizeof(thinfo));
    thinfo.size = sizeof(thinfo);
    int res = sceKernelReferThreadStatus(sceKernelGetThreadId(), &thinfo);
    if (UNLIKELY(res < 0)) {
        DLOG("sceKernelReferThreadStatus(0x%08X) failed: %s",
             sceKernelGetThreadId(), psp_strerror(res));
        return 0;
    }

    if (strncmp(thinfo.name, "SysThread_", 10) != 0) {
        return 0;
    }
    char *s;
    const uint32_t thread_address = strtoul(&thinfo.name[10], &s, 16);
    if (s != &thinfo.name[18] || *s != '\0') {
        return 0;
    }
    return (SysThreadID)thread_address;
}

/*-----------------------------------------------------------------------*/

int sys_thread_get_priority(void)
{
    return THREADPRI_MAIN - sceKernelGetThreadCurrentPriority();
}

/*-----------------------------------------------------------------------*/

int sys_thread_set_affinity(UNUSED uint64_t affinity)
{
    return 1;  // Affinity is meaningless.
}

/*-----------------------------------------------------------------------*/

uint64_t sys_thread_get_affinity(void)
{
    return 0x1;
}

/*-----------------------------------------------------------------------*/

int sys_thread_is_running(SysThreadID thread_)
{
    SysThread *thread = (SysThread *)thread_;

    SceKernelThreadInfo thinfo;
    mem_clear(&thinfo, sizeof(thinfo));
    thinfo.size = sizeof(thinfo);
    int res = sceKernelReferThreadStatus(thread->thread, &thinfo);
    if (UNLIKELY(res < 0)) {
        DLOG("sceKernelReferThreadStatus(0x%08X) failed: %s",
             thread->thread, psp_strerror(res));
        /* Return false (stopped) so the caller doesn't get stuck. */
        return 0;
    }

    return (thinfo.status & (PSP_THREAD_RUNNING | PSP_THREAD_READY
                             | PSP_THREAD_WAITING)) != 0;
}

/*-----------------------------------------------------------------------*/

int sys_thread_wait(SysThreadID thread_, int *result_ret)
{
    SysThread *thread = (SysThread *)thread_;

    if (thread->thread == sceKernelGetThreadId()) {
        DLOG("Attempted to wait for current thread!");
        return 0;
    }

    while (!psp_delete_thread_if_stopped(thread->thread, NULL)) {
        sceKernelDelayThread(100);  // 0.1ms
    }

    *result_ret = thread->result;
    mem_free(thread);
    return 1;
}

/*-----------------------------------------------------------------------*/

void sys_thread_yield()
{
    /* The PSP doesn't have a "yield" function, but this has the same
     * effect.  We can't pass 0 because if we do, the OS will sometimes
     * decide not to switch to a different thread even if one is ready. */
    sceKernelDelayThread(1);
}

/*************************************************************************/
/******************** PSP-specific interface routines ********************/
/*************************************************************************/

SysThreadID psp_thread_create_named(
    const char *name, int priority, int stack_size, int (*function)(void *),
    void *param)
{
    /* On the PSP, lower value = higher priority. */
    const int real_priority = THREADPRI_MAIN - priority;

    SysThread *thread = mem_alloc(sizeof(*thread), 0, 0);
    if (!thread) {
        DLOG("No memory for thread info structure");
        return 0;
    }
    thread->function = function;
    thread->param = param;

    char namebuf[32];
    if (!name) {
        strformat(namebuf, sizeof(namebuf), "SysThread_%08X",
                  (uint32_t)thread);
        name = namebuf;
    }
    thread->thread = psp_start_thread(
        namebuf, thread_runner, real_priority,
        stack_size ? stack_size : 65536, sizeof(thread), &thread);
    if (thread->thread < 0) {
        DLOG("%s: Failed to start thread: %s", name,
             psp_strerror(thread->thread));
        mem_free(thread);
        return 0;
    }
    return (SysThreadID)thread;
}

/*-----------------------------------------------------------------------*/

SceUID psp_start_thread(const char *name, int (*entry)(SceSize, void *),
                        int priority, int stacksize, SceSize args, void *argp)
{
    if (UNLIKELY(!name)
     || UNLIKELY(!entry)
     || UNLIKELY(priority < 0)
     || UNLIKELY(stacksize < 0)) {
        DLOG("Invalid parameters: %p[%s] %p %d %d %d %p",
             name, name ? name : "", entry, priority, stacksize, args, argp);
        return PSP_EINVAL;
    }

    SceUID handle = sceKernelCreateThread(name, entry, priority, stacksize,
                                          0, NULL);
    if (UNLIKELY(handle < 0)) {
        DLOG("Failed to create thread \"%s\": %s", name, psp_strerror(handle));
        return handle;
    }

    int32_t res = sceKernelStartThread(handle, args, argp);
    if (UNLIKELY(res < 0)) {
        DLOG("Failed to start thread \"%s\": %s", name, psp_strerror(res));
        sceKernelDeleteThread(handle);
        return res;
    }

    return handle;
}

/*-----------------------------------------------------------------------*/

int psp_delete_thread_if_stopped(SceUID thid, int *status_ret)
{
    SceKernelThreadInfo thinfo;
    mem_clear(&thinfo, sizeof(thinfo));
    thinfo.size = sizeof(thinfo);
    int res = sceKernelReferThreadStatus(thid, &thinfo);

    if (UNLIKELY(res < 0)) {
        DLOG("sceKernelReferThreadStatus(0x%08X) failed: %s",
             thid, psp_strerror(res));
        sceKernelTerminateThread(thid);

    } else if (thinfo.status & (PSP_THREAD_RUNNING | PSP_THREAD_READY
                                | PSP_THREAD_WAITING)) {
        return 0;

    } else if (thinfo.status & PSP_THREAD_STOPPED) {
        res = thinfo.exitStatus;

    } else {
        res = 0x80000000 | thinfo.status;
        sceKernelTerminateThread(thid);
    }

    sceKernelDeleteThread(thid);
    if (status_ret) {
        *status_ret = res;
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

void psp_threads_lock(void)
{
    const int intstatus = sceKernelCpuSuspendIntr();
    /* BARRIER() is technically unnecessary on the PSP as long as there's
     * a sequence point (since the PSP's CPU has only one core), but we
     * include one anyway for clarity. */
    BARRIER();
    if (lock_count == 0) {
        lock_intstatus = intstatus;
    }
    lock_count++;
}

/*-----------------------------------------------------------------------*/

void psp_threads_unlock(void)
{
    if (lock_count > 0) {
        lock_count--;
        if (lock_count == 0) {
            BARRIER();  // As above, unnecessary but included for clarity.
            sceKernelCpuResumeIntrWithSync(lock_intstatus);
        }
    }
}

/*-----------------------------------------------------------------------*/

int psp_threads_locked(void)
{
    return lock_count > 0;
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static int thread_runner(SceSize args, void *argp)
{
    ASSERT(args == sizeof(SysThread **), return 0);
    SysThread *thread = *(SysThread **)argp;
    thread->result = (*thread->function)(thread->param);
    return 0;
}

/*************************************************************************/
/*************************************************************************/
