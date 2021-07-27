/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/android/thread.c: POSIX thread helpers for Android.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/sysdep.h"
#include "src/sysdep/android/internal.h"
#include "src/sysdep/posix/thread.h"
#include "src/thread.h"

#include <jni.h>

/*************************************************************************/
/*************************************************************************/

void posix_thread_runner_init(SysThread *thread)
{
    JavaVM *vm = android_activity->vm;
    JNIEnv *env;
    (*vm)->AttachCurrentThread(vm, &env, NULL);

    jclass android_os_Process = get_class("android.os.Process");
    ASSERT(android_os_Process, return);
    jmethodID setThreadPriority = (*env)->GetStaticMethodID(
        env, android_os_Process, "setThreadPriority", "(I)V");
    ASSERT(setThreadPriority != 0, return);
    (*env)->CallStaticVoidMethod(env, android_os_Process,
                                 setThreadPriority, thread->initial_priority);
    if (UNLIKELY(clear_exceptions(env))) {
        DLOG("Failed to set thread priority to %d", thread->initial_priority);
    }

    if (thread->initial_affinity) {
        if (UNLIKELY(!thread_set_affinity(thread->initial_affinity))) {
            DLOG("Failed to set thread affinity mask to 0x%llX",
                 (long long)thread->initial_affinity);
        }
    }
}

/*-----------------------------------------------------------------------*/

void posix_thread_runner_cleanup(UNUSED SysThread *thread)
{
    JavaVM *vm = android_activity->vm;
    (*vm)->DetachCurrentThread(vm);
}

/*************************************************************************/
/*************************************************************************/
