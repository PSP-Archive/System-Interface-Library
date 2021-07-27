/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/android/main.c: sil_main() wrapper for Android.
 */

/*
 * Native Android apps start life as a Java "activity" (NativeActivity,
 * or in our case the custom SILActivity which extends it); the Java code
 * calls out to native functions to implement the Android activity life
 * cycle.  The android_main() function defined here is called on a
 * separate thread after the activity has started.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/main.h"
#include "src/math/fpu.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/android/internal.h"
#include "src/utility/misc.h"

#include <pthread.h>
#include <signal.h>

/*************************************************************************/
/*************************************************************************/

int android_main(UNUSED void *param)
{
    DLOG("Main thread: 0x%lX", (long)pthread_self());

    JNIEnv *env = get_jni_env();
    jobject activity_obj = android_activity->clazz;

    jmethodID getInternalDataPath = get_method(
        0, "getInternalDataPath", "()Ljava/lang/String;");
    ASSERT(getInternalDataPath, return -1);
    jmethodID getExternalDataPath = get_method(
        0, "getExternalDataPath", "()Ljava/lang/String;");
    ASSERT(getExternalDataPath, return -1);
    jclass Environment = get_class("android.os.Environment");
    ASSERT(Environment, return -1);
    jmethodID getExternalStorageDirectory = get_static_method(
        Environment, "getExternalStorageDirectory", "()Ljava/io/File;");
    ASSERT(getExternalStorageDirectory, return -1);
    jclass File = get_class("java.io.File");
    ASSERT(File, return -1);
    jmethodID getPath = get_method(File, "getPath", "()Ljava/lang/String;");
    ASSERT(getPath, return -1);

    /* Look up the data storage directories.  The paths themselves are
     * provided to us in the NativeActivity structure, but if a directory
     * doesn't already exist, it doesn't seem to get created until we call
     * the associated Java function. */
    jstring j_path = (*env)->CallObjectMethod(env, activity_obj,
                                              getInternalDataPath);
    const char *path = (*env)->GetStringUTFChars(env, j_path, NULL);
    if (!path || !*path) {  // Should always be available.
        DLOG("Failed to get internal data path");
        android_show_alert(1, "SIL_error_title",
                           1, "SIL_error_no_internal_data");
        if (path) {
            (*env)->ReleaseStringUTFChars(env, j_path, path);
        }
        (*env)->DeleteLocalRef(env, j_path);
        return -1;
    }
    DLOG("Internal data path: %s", path);
    android_internal_data_path = mem_strdup(path, 0);
    (*env)->ReleaseStringUTFChars(env, j_path, path);
    (*env)->DeleteLocalRef(env, j_path);

    j_path = (*env)->CallObjectMethod(env, activity_obj, getExternalDataPath);
    path = (*env)->GetStringUTFChars(env, j_path, NULL);
    if (!path || !*path) {
        DLOG("Failed to get external data path (continuing anyway)");
        if (path) {
            (*env)->ReleaseStringUTFChars(env, j_path, path);
        }
        android_external_data_path = NULL;
    } else {
        DLOG("External data path: %s", path);
        android_external_data_path = mem_strdup(path, 0);
        if (UNLIKELY(!android_external_data_path)) {
            DLOG("Out of memory saving external data path: %s", path);
            android_show_alert(1, "SIL_error_title",
                               1, "SIL_error_out_of_memory");
            return -1;
        }
        (*env)->ReleaseStringUTFChars(env, j_path, path);
    }
    (*env)->DeleteLocalRef(env, j_path);

    jobject j_file = (*env)->CallStaticObjectMethod(
        env, Environment, getExternalStorageDirectory);
    if (!j_file) {
        DLOG("Failed to get external storage directory (continuing anyway)");
        android_external_root_path = NULL;
    } else {
        j_path = (*env)->CallObjectMethod(env, j_file, getPath);
        (*env)->DeleteLocalRef(env, j_file);
        path = (*env)->GetStringUTFChars(env, j_path, NULL);
        if (!path || !*path) {
            DLOG("Failed to get external storage path (continuing anyway)");
            if (path) {
                (*env)->ReleaseStringUTFChars(env, j_path, path);
            }
            android_external_root_path = NULL;
        } else {
            DLOG("External storage mount point: %s", path);
            android_external_root_path = mem_strdup(path, 0);
            if (UNLIKELY(!android_external_root_path)) {
                DLOG("Out of memory saving external mount point: %s", path);
                android_show_alert(1, "SIL_error_title",
                                   1, "SIL_error_out_of_memory");
                return -1;
            }
            (*env)->ReleaseStringUTFChars(env, j_path, path);
        }
        (*env)->DeleteLocalRef(env, j_path);
    }

    ASSERT(!clear_exceptions(env));

    /* Check for expansion files.  On pre-ICS devices, this will fail if
     * external storage is unavailable. */
    if (!check_for_expansion_files()) {
        return -1;
    }

    /* Warn if we can't access external storage.  If we get this far, we
     * didn't have any expansion files to worry about, so treat the
     * condition as nonfatal. */
    if (!android_external_data_path) {
        android_show_alert(1, "SIL_error_title",
                           1, "SIL_error_no_external_data_nonfatal");
    }

    fpu_configure();

    char *args = NULL;

    jmethodID getArgs = get_method(0, "getArgs", "()Ljava/lang/String;");
    ASSERT(getArgs != 0, return -1);
    jstring j_args = (*env)->CallObjectMethod(env, activity_obj, getArgs);
    ASSERT(!clear_exceptions(env), return -1);
    ASSERT(j_args != 0, return -1);
    const char *c_args = (*env)->GetStringUTFChars(env, j_args, NULL);
    if (c_args) {
        args = mem_strdup(c_args, MEM_ALLOC_TEMP);
        if (!args) {
            DLOG("Out of memory copying arg string: %s", c_args);
        }
        (*env)->ReleaseStringUTFChars(env, j_args, c_args);
    }
    (*env)->DeleteLocalRef(env, j_args);

    int argc;
    const char **argv;
    const char *argv_dummy[2];
    if (!(args && split_args(args, 1, &argc, (char ***)&argv))) {
        argc = 1;
        argv = argv_dummy;
        argv[0] = SIL_PLATFORM_ANDROID_PACKAGE_NAME;
        argv[1] = NULL;
    }

    const int exitcode = sil__main(argc, argv);
    if (exitcode == 2) {
        /* Trigger the "Unfortunately, X has stopped." dialog. */
        DLOG("Aborting due to init failure");
        raise(SIGKILL);
    }

    if (argv != argv_dummy) {
        mem_free(argv);
    }
    mem_free(args);

    ANativeActivity_finish(android_activity);
    return 0;
}

/*************************************************************************/
/*************************************************************************/
