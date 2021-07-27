/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/android/activity.c: Android NativeActivity callbacks.
 */

#define IN_SYSDEP
#undef SIL_MEMORY_FORBID_MALLOC  // We need to allocate memory before init.

#include "src/base.h"
#include "src/input.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/android/internal.h"
#include "src/sysdep/linux/meminfo.h"
#include "src/thread.h"
#include "src/time.h"

#include <fcntl.h>
#include <time.h>
#include <unistd.h>

/*************************************************************************/
/******************* Exported data (library-internal) ********************/
/*************************************************************************/

int android_api_level;
const char *android_info_hardware;
const char *android_info_manufacturer;
const char *android_info_model;
const char *android_info_product;
ANativeActivity *android_activity;
ANativeWindow *android_window;
const char *android_internal_data_path;
const char *android_external_data_path;
const char *android_external_root_path;
SysSemaphoreID android_suspend_semaphore;
SysSemaphoreID android_resume_semaphore;
uint8_t android_suspend_requested;
uint8_t android_quit_requested;

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Global reference to the activity's Java class, for looking up method IDs. */
jclass activity_class;

/* Pathnames for downloaded expansion files (NULL if none). */
static const char *expansion_file_path[2];

/* Thread handle for main game thread. */
static SysThreadID main_thread;

/* Thread handle for input loop thread. */
static SysThreadID input_thread;

/* The ALooper object created for input handling.  Mostly private to the
 * input loop thread, but exported to the main thread so it can call wake(). */
static ALooper *input_looper;

/* Stop flag for input loop thread. */
static uint8_t input_thread_stop;

/*-----------------------------------------------------------------------*/

/* Callback declarations. */

static void onConfigurationChanged(ANativeActivity *activity);
static void onDestroy(ANativeActivity *activity);
static void onInputQueueCreated(ANativeActivity *activity,
                                AInputQueue *queue);
static void onInputQueueDestroyed(ANativeActivity *activity,
                                  AInputQueue *queue);
static void onLowMemory(ANativeActivity *activity);
static void onNativeWindowCreated(ANativeActivity *activity,
                                  ANativeWindow *window);
static void onNativeWindowDestroyed(ANativeActivity *activity,
                                    ANativeWindow *window);
static void onPause(ANativeActivity *activity);
static void onResume(ANativeActivity *activity);
static void *onSaveInstanceState(ANativeActivity *activity, size_t *out_len);
static void onStart(ANativeActivity *activity);
static void onStop(ANativeActivity *activity);
static void onWindowFocusChanged(ANativeActivity *activity, int focused);


/* Other local routine declarations. */

/**
 * do_quit:  Set android_quit_requested and wait for the sil_main() thread
 * to exit (if it's running), then terminate the process.
 */
static void do_quit(void);

/**
 * input_loop:  Loop used to pass input events to the input subsystem.
 * Called as a separate thread when the input queue is established.
 *
 * [Parameters]
 *     param: Thread parameter (event queue for this thread).
 * [Return value]
 *     0
 */
static int input_loop(void *param);

/**
 * throw:  Throw a Java exception to force the JVM to terminate.
 *
 * [Parameters]
 *     message: Message to include in exception.
 */
void throw(const char *message);

/*************************************************************************/
/************************** Program entry point **************************/
/*************************************************************************/

/**
 * ANativeActivity_onCreate:  Program entry point for Android.  This
 * function is called (indirectly, hence the lack of JNI decorators) from
 * Java when the program's activity is created, and serves as the rough
 * equivalent of main() for Android applications.
 *
 * Since this function is called from the main application thread, the
 * program should only do the minimum necessary setup before returning,
 * and should perform all primary processing either in a separate thread
 * or in response to NativeActivity callbacks.
 *
 * [Parameters]
 *     activity: NativeActivity object.
 *     savedState: Activity's saved state data.
 *     savedStateSize: Size of data stored in savedState, in bytes.
 */
__attribute__((visibility("default")))
void ANativeActivity_onCreate(ANativeActivity *activity,
                              UNUSED void *savedState,
                              UNUSED size_t savedStateSize)
{
    DLOG("called");

#ifdef GCOV_PREFIX
    ASSERT(setenv("GCOV_PREFIX", GCOV_PREFIX, 1) == 0);
#endif

    android_activity = activity;

    /* Set up NativeActivity callbacks.  We define all possible callbacks,
     * even though several of them are no-ops, just to have a convenient
     * record of what's available. */
    activity->callbacks->onConfigurationChanged  = onConfigurationChanged;
    activity->callbacks->onDestroy               = onDestroy;
    activity->callbacks->onInputQueueCreated     = onInputQueueCreated;
    activity->callbacks->onInputQueueDestroyed   = onInputQueueDestroyed;
    activity->callbacks->onLowMemory             = onLowMemory;
    activity->callbacks->onNativeWindowCreated   = onNativeWindowCreated;
    activity->callbacks->onNativeWindowDestroyed = onNativeWindowDestroyed;
    activity->callbacks->onPause                 = onPause;
    activity->callbacks->onResume                = onResume;
    activity->callbacks->onSaveInstanceState     = onSaveInstanceState;
    activity->callbacks->onStart                 = onStart;
    activity->callbacks->onStop                  = onStop;
    activity->callbacks->onWindowFocusChanged    = onWindowFocusChanged;

    /* Save a reference to the activity's Java class so we can use it in
     * get_method() without having to explicitly take a reference each time. */
    JNIEnv *env = activity->env;
    jobject activity_obj = activity->clazz;
    jclass activity_class_local = (*env)->GetObjectClass(env, activity_obj);
    ASSERT(activity_class_local != 0,
           throw("Activity class not found"); return);
    activity_class = (*env)->NewGlobalRef(env, activity_class_local);
    (*env)->DeleteLocalRef(env, activity_class_local);
    ASSERT(activity_class != 0,
           throw("Failed to take a reference to the activity class"); return);

    /* Grab Java object pointers and method IDs for making JNI calls. */
    jmethodID getAPILevel = get_method(0, "getAPILevel", "()I");
    jmethodID getBuildInfo = get_method(
        0, "getBuildInfo", "(I)Ljava/lang/String;");

    /* ASSERT() that all method IDs are found to avoid mysterious crashes
     * inside the VM when we try to call them. */
    #define ASSERT_METHOD_FOUND(name) \
        ASSERT(name != 0, throw("Method not found: " #name); return)
    ASSERT_METHOD_FOUND(getAPILevel);
    ASSERT_METHOD_FOUND(getBuildInfo);
    #undef ASSERT_METHOD_FOUND

    /* Save the API level and hardware information strings for later
     * reference by other code.  Also log the information to the debug log. */
    static const struct {
        BuildInfoID id;
        const char **string_ptr;
        const char *log_header;
    } info_strings[] = {
        {BUILD_INFO_MANUFACTURER, &android_info_manufacturer, "Manufacturer"},
        {BUILD_INFO_MODEL,        &android_info_model,        "       Model"},
        {BUILD_INFO_PRODUCT,      &android_info_product,      "     Product"},
        {BUILD_INFO_HARDWARE,     &android_info_hardware,     "    Hardware"},
    };
    android_api_level = (*env)->CallIntMethod(
        env, activity_obj, getAPILevel);
    DLOG("Android API level: %d", android_api_level);
    DLOG("Device information:");
    for (int i = 0; i < lenof(info_strings); i++) {
        jstring j_info = (*env)->CallObjectMethod(
            env, activity_obj, getBuildInfo, info_strings[i].id);
        const char *info = (*env)->GetStringUTFChars(env, j_info, NULL);
        DLOG("   %s: %s", info_strings[i].log_header, info);
        if (info) {
            *(info_strings[i].string_ptr) = strdup(info);
            (*env)->ReleaseStringUTFChars(env, j_info, info);
        }
        (*env)->DeleteLocalRef(env, j_info);
        if (!*(info_strings[i].string_ptr)) {
            *(info_strings[i].string_ptr) = "<unknown>";
        }
    }

    /* We should never get any exceptions in the above code, but check
     * anyway since it's good practice. */
    ASSERT(!clear_exceptions(env));

    /* Create semaphores for signaling activity stop/resume events. */
    android_suspend_semaphore = sys_semaphore_create(0, 1);
    if (UNLIKELY(!android_suspend_semaphore)) {
        throw("Failed to create suspend semaphore");
        return;
    }
    android_resume_semaphore = sys_semaphore_create(0, 1);
    if (UNLIKELY(!android_resume_semaphore)) {
        throw("Failed to create resume semaphore");
        return;
    }

    /* Set the window color depth to 24bpp, since the default is
     * ugly-looking 16bpp. */
    ANativeActivity_setWindowFormat(activity, WINDOW_FORMAT_RGBX_8888);

    /* We don't start the main thread until the window has been created,
     * so just return here. */
}

/*************************************************************************/
/*********************** NativeActivity callbacks ************************/
/*************************************************************************/

static void onConfigurationChanged(UNUSED ANativeActivity *activity)
{
    DLOG("called");

    /* Nothing to do. */
}

/*-----------------------------------------------------------------------*/

static void onDestroy(UNUSED ANativeActivity *activity)
{
    DLOG("called");

    do_quit();
}

/*-----------------------------------------------------------------------*/

static void onInputQueueCreated(UNUSED ANativeActivity *activity,
                                AInputQueue *queue)
{
    DLOG("called");

    static const ThreadAttributes attr;  // All zero.
    input_thread = sys_thread_create(&attr, input_loop, queue);
}

/*-----------------------------------------------------------------------*/

static void onInputQueueDestroyed(UNUSED ANativeActivity *activity,
                                  UNUSED AInputQueue *queue)
{
    DLOG("called");

    input_thread_stop = 1;
    BARRIER();
    /* Here, input_looper can only be NULL if (1) the input thread has not
     * yet entered its loop (and thus the loop will be skipped entirely),
     * or (2) the thread has already detected input_thread_stop and exited. */
    if (input_looper) {
        ALooper_wake(input_looper);
    }
    int32_t dummy;
    sys_thread_wait(input_thread, &dummy);
    input_thread = 0;
    input_thread_stop = 0;
}

/*-----------------------------------------------------------------------*/

static void onLowMemory(UNUSED ANativeActivity *activity)
{
    DLOG("called");

    const int64_t self = linux_get_process_size();
    const int64_t avail = linux_get_free_memory();
#ifdef DEBUG
    DLOG("Memory warning: total=%ldk self=%ldk avail=%ldk",
         (long)(linux_get_total_memory()/1024),
         (long)(self/1024), (long)(avail/1024));
#endif
    android_forward_input_event(&(InputEvent){
        .type = INPUT_EVENT_MEMORY, .detail = INPUT_MEMORY_LOW,
        .timestamp = time_now(),
        {.memory = {.used_bytes = self, .free_bytes = avail}}});
}

/*-----------------------------------------------------------------------*/

static void onNativeWindowCreated(UNUSED ANativeActivity *activity,
                                  ANativeWindow *window)
{
    DLOG("called");

    /* If the sil_main() thread is already running, it should be sleeping
     * in sys_input_acknowledge_suspend_request().  Ensure android_window
     * is set before we release the thread, so that android_resume_graphics()
     * can properly set up the drawing surface. */
    android_window = window;
    android_suspend_requested = 0;
    sys_semaphore_signal(android_resume_semaphore);

    /* If this is the first time a window was created for this run of the
     * program, start up sil_main() on a separate thread. */
    if (!main_thread) {
        static const ThreadAttributes attr;  // All zero.
        main_thread = sys_thread_create(&attr, android_main, NULL);
    }
}

/*-----------------------------------------------------------------------*/

static void onNativeWindowDestroyed(UNUSED ANativeActivity *activity,
                                    UNUSED ANativeWindow *window)
{
    DLOG("called");

    int is_finishing;
    JNIEnv *env = activity->env;
    jobject activity_obj = activity->clazz;
    jmethodID isFinishing = get_method(0, "isFinishing", "()Z");
    ASSERT(isFinishing != 0, throw("Method not found: isFinishing"); return);
    is_finishing = (*env)->CallBooleanMethod(env, activity_obj, isFinishing);
    ASSERT(!clear_exceptions(env));

    /* If this happens without a preceding onPause() or finish(), things
     * will break horribly, so force-quit in that case. */
    ASSERT(android_suspend_requested || is_finishing, do_quit());

    BARRIER();
    android_window = NULL;
}

/*-----------------------------------------------------------------------*/

static void onPause(ANativeActivity *activity)
{
    DLOG("called");

    int is_finishing;
    JNIEnv *env = activity->env;
    jobject activity_obj = activity->clazz;
    jmethodID isFinishing = get_method(0, "isFinishing", "()Z");
    ASSERT(isFinishing != 0, throw("Method not found: isFinishing"); return);
    is_finishing = (*env)->CallBooleanMethod(env, activity_obj, isFinishing);
    ASSERT(!clear_exceptions(env));

    if (is_finishing) {
        do_quit();
    } else {  // !is_finishing
        /* Paranoia: spin to clear semaphores. */
        while (sys_semaphore_wait(android_suspend_semaphore, 0)) {}
        while (sys_semaphore_wait(android_resume_semaphore, 0)) {}
        android_suspend_requested = 1;
        if (main_thread) {
            sys_semaphore_wait(android_suspend_semaphore, -1);
        }
    }
}

/*-----------------------------------------------------------------------*/

static void onResume(UNUSED ANativeActivity *activity)
{
    DLOG("called");

    if (android_window) {
        /* The window was preserved over the pause/resume sequence. */
        android_suspend_requested = 0;
        sys_semaphore_signal(android_resume_semaphore);
    } else {
        /* Nothing to do until the window is (re-)created. */
    }
}

/*-----------------------------------------------------------------------*/

static void *onSaveInstanceState(UNUSED ANativeActivity *activity,
                                 size_t *out_len)
{
    DLOG("called");

    /* We handle state saving (i.e., game autosave) separately. */
    *out_len = 0;
    return NULL;
}

/*-----------------------------------------------------------------------*/

static void onStart(UNUSED ANativeActivity *activity)
{
    DLOG("called");

    /* Nothing to do until the window is created. */
}

/*-----------------------------------------------------------------------*/

static void onStop(UNUSED ANativeActivity *activity)
{
    DLOG("called");

    /* Nothing to do. */
}

/*-----------------------------------------------------------------------*/

static void onWindowFocusChanged(UNUSED ANativeActivity *activity,
                                 UNUSED int focused)
{
    DLOG("called");

    /* Nothing to do. */
}

/*************************************************************************/
/*********************** Exported utility routines ***********************/
/*************************************************************************/

const char *android_expansion_file_path(int index)
{
    if (index < 0 || index >= lenof(expansion_file_path)) {
        DLOG("Invalid index: %d", index);
        return NULL;
    }
    return expansion_file_path[index];
}

/*-----------------------------------------------------------------------*/

const char *android_external_mount_point(void)
{
    return android_external_root_path;
}

/*-----------------------------------------------------------------------*/

int android_request_permission(AndroidPermission permission)
{
    static const char * const permission_names[] = {
        #define DECLARE_PERMISSION(name) [ANDROID_PERMISSION_##name] = #name
        DECLARE_PERMISSION(READ_EXTERNAL_STORAGE),
        DECLARE_PERMISSION(WRITE_EXTERNAL_STORAGE),
        #undef DECLARE_PERMISSION
    };

    PRECOND((int)permission >= 0 && (int)permission <= lenof(permission_names),
            return 0);
    ASSERT(permission_names[permission], return 0);
    const char *name = permission_names[permission];

    JNIEnv *env = get_jni_env();
    jobject activity_obj = android_activity->clazz;
    jmethodID requestPermission = get_method(
        0, "requestPermission", "(Ljava/lang/String;)I");
    ASSERT(requestPermission, return 0);
    jstring j_name = (*env)->NewStringUTF(env, name);
    ASSERT(j_name != 0, clear_exceptions(env); return 0);
    const int result = (*env)->CallIntMethod(env, activity_obj,
                                             requestPermission, j_name);
    (*env)->DeleteLocalRef(env, j_name);
    return result;
}

/*************************************************************************/
/******************* Library-internal utility routines *******************/
/*************************************************************************/

JNIEnv *get_jni_env(void)
{
    JavaVM *vm = android_activity->vm;
    JNIEnv *env = NULL;
    (*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_6);
    ASSERT(env);
    return env;
}

/*-----------------------------------------------------------------------*/

jclass get_class(const char *name)
{
    PRECOND(name != NULL, return 0);

    char namebuf[1000];
    if (*name == '.') {
        if (!strformat_check(namebuf, sizeof(namebuf), "%s%s",
                             SIL_PLATFORM_ANDROID_PACKAGE_NAME, name)) {
            DLOG("Buffer overflow on class name: %s%s",
                 SIL_PLATFORM_ANDROID_PACKAGE_NAME, name);
            return NULL;
        }
        name = namebuf;
    }

    JNIEnv *env = get_jni_env();

    /* If an exception is pending, no calls to Java code (such as the
     * getClass() call below) will work.  Normally, any routine which
     * calls Java code is responsible for handling exceptions; if an
     * exception falls through the cracks, catch it here so the getClass()
     * call doesn't spuriously fail.  We ASSERT() in an effort to abort
     * as close to the missed exception as possible in debug mode. */
    ASSERT(!clear_exceptions(env));

    jobject activity_obj = android_activity->clazz;
    jmethodID getClass = get_method(
        0, "getClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    ASSERT(getClass != 0, return 0);
    jstring j_name = (*env)->NewStringUTF(env, name);
    ASSERT(j_name != 0, clear_exceptions(env); return 0);
    jclass class =
        (*env)->CallObjectMethod(env, activity_obj, getClass, j_name);
    /* This should never fail (we're just getting a reference to the class)
     * but play it safe. */
    if (UNLIKELY(clear_exceptions(env))) {
        DLOG("Failed to get reference to class %s", name);
        class = 0;
    }
    (*env)->DeleteLocalRef(env, j_name);
    return class;
}

/*-----------------------------------------------------------------------*/

jmethodID get_method(jclass class, const char *method, const char *signature)
{
    JNIEnv *env = get_jni_env();
    ASSERT(!clear_exceptions(env));  // As in get_class().

    if (!class) {
        class = activity_class;
    }
    jmethodID id = (*env)->GetMethodID(env, class, method, signature);
    if (clear_exceptions(env)) {
        id = 0;
    }
    return id;
}

/*-----------------------------------------------------------------------*/

jmethodID get_static_method(jclass class, const char *method,
                            const char *signature)
{
    JNIEnv *env = get_jni_env();
    ASSERT(!clear_exceptions(env));  // As in get_class().

    if (!class) {
        class = activity_class;
    }
    jmethodID id = (*env)->GetStaticMethodID(env, class, method, signature);
    if (clear_exceptions(env)) {
        id = 0;
    }
    return id;
}

/*-----------------------------------------------------------------------*/

int clear_exceptions(JNIEnv *env)
{
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
        return 1;
    }
    return 0;
}

/*-----------------------------------------------------------------------*/

int check_for_expansion_files(void)
{
    JNIEnv *env = get_jni_env();
    jobject activity_obj = android_activity->clazz;

    jmethodID getExpansionFilePath = get_method(
        0, "getExpansionFilePath", "(I)Ljava/lang/String;");
    ASSERT(getExpansionFilePath, return 0);

    for (int i = 0; i < 2; i++) {
        jstring j_path = (*env)->CallObjectMethod(
            env, activity_obj, getExpansionFilePath, i);
        if (!j_path) {
            DLOG("Expansion file %d does not exist", i);
            expansion_file_path[i] = NULL;
            continue;
        }
        const char *path = (*env)->GetStringUTFChars(env, j_path, NULL);
        if (!path || !*path) {
            DLOG("Failed to get expansion file %d path", i);
            if (path) {
                (*env)->ReleaseStringUTFChars(env, j_path, path);
            }
            continue;
        }
        DLOG("Expansion file %d path: %s", i, path);
        expansion_file_path[i] = mem_strdup(path, 0);
        if (UNLIKELY(!expansion_file_path[i])) {
            DLOG("Out of memory saving expansion file path %d: %s", i, path);
            android_show_alert(1, "SIL_error_title",
                               1, "SIL_error_out_of_memory");
            return 0;
        }
        if (access(expansion_file_path[i], R_OK) != 0) {
            DLOG("Failed to access expansion file %d (%s): %s", i,
                 expansion_file_path[i], strerror(errno));
            /* On pre-ICS devices, external storage could potentially be
             * located on a physically removable device such as an SD card.
             * If we can't access external storage, give the user a hint as
             * to why the program won't start up. */
            if (!android_external_data_path) {
                android_show_alert(1, "SIL_error_title",
                                   1, "SIL_error_no_external_data");
            } else {
                android_show_alert(1, "SIL_error_title",
                                   1, "SIL_error_no_app_data");
            }
            return 0;
        }
        (*env)->ReleaseStringUTFChars(env, j_path, path);
        (*env)->DeleteLocalRef(env, j_path);
    }

    ASSERT(!clear_exceptions(env));
    return 1;
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static void do_quit(void)
{
    android_quit_requested = 1;
    if (main_thread) {
        DLOG("waiting for main thread");
        int32_t dummy;
        sys_thread_wait(main_thread, &dummy);
        DLOG("main thread exited");
        main_thread = 0;
    }

    /* If the app is restarted, we can't just recreate the main thread and
     * call sil_main() again, since static data in the client code might
     * have been modified during the previous run; we need to ensure that
     * at least the native code is reloaded.  However, Android doesn't
     * provide a guaranteed way to accomplish this; Activity.finish()
     * doesn't necessarily result in the activity being destroyed, and
     * even if the activity is destroyed, there's no guarantee the native
     * object will be unloaded.  So we do things the old-fashioned way.
     * As a bonus, this ensures that coverage data will be properly
     * written out for builds with coverage analysis enabled. */
    exit(0);
}

/*-----------------------------------------------------------------------*/

static int input_loop(void *param)
{
    AInputQueue *queue = (AInputQueue *)param;

    input_looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
    BARRIER();
    AInputQueue_attachLooper(queue, input_looper, 0, NULL, NULL);

    while (!input_thread_stop) {
        int result, events;
        void *data;
        /*
         * Apparently there's a race condition in event processing that can
         * result in events queued without any notification to pollAll(),
         * so we call getEvent() in a loop until it returns failure (no
         * events left).  See:
         * https://code.google.com/p/android/issues/detail?id=41755
         *
         * On versions of Android before Jelly Bean (4.1), this method
         * spams "Failed to receive dispatch signal.  status=-11" errors to
         * the system log on every input event, but unfortunately we can't
         * use hasEvents() to check for pending events, since it seems to
         * suffer from the same problem as pollAll() (verified through
         * Android 4.2.1).
         */
        result = ALooper_pollAll(-1, NULL, &events, &data);
        if (result >= 0) {
            AInputEvent *event;
            while (AInputQueue_getEvent(queue, &event) >= 0) {
                if (!AInputQueue_preDispatchEvent(queue, event)) {
                    const int handled = android_handle_input_event(event);
                    AInputQueue_finishEvent(queue, event, handled);
                }
            }
        }
        BARRIER();
    }

    input_looper = NULL;
    return 0;
}

/*-----------------------------------------------------------------------*/

void throw(const char *message)
{
    JNIEnv *env = android_activity->env;

    clear_exceptions(env);
    jclass exception = (*env)->FindClass(env, "java/lang/Exception");
    if (exception) {
        (*env)->ThrowNew(env, exception, message);
    } else {
        /* FindClass() will have thrown an exception, so let that
         * terminate the JVM instead. */
    }
}

/*************************************************************************/
/*************************************************************************/
