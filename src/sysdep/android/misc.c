/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/android/misc.c: Miscellaneous interface functions for Android.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/android/internal.h"
#include "src/thread.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Idle timeout, in seconds.  We make this just long enough to avoid
 * repeated calls to keepScreenOn() while client code is keeping the
 * screen active.  Once the lock is released, the device will restart its
 * idle timer. */
#define IDLE_TIMEOUT  3

/* Thread ID for the idle timer thread, or 0 if the thread is not running. */
static int idle_timer_thread_id;

/* Semaphore used to trigger an idle timer reset. */
static SysSemaphoreID idle_reset_trigger;

/* Shared flag used to signal the idle timer thread to stop. */
static uint8_t idle_timer_thread_stop;

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * idle_timer_thread:  Thread which implements an idle timer using a wake
 * lock.  Needed because Android denies non-system applications access to
 * PowerManager.userActivity().
 *
 * [Parameters]
 *     unused: Thread parameter (unused).
 * [Return value]
 *     0
 */
static int idle_timer_thread(void *unused);

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

void sys_display_error(const char *message, va_list args)
{
    char buf[1000];
    vstrformat(buf, sizeof(buf), message, args);
    DLOG("Error: %s", buf);
#ifdef SIL_UTILITY_NOISY_ERRORS
    android_show_alert(1, "SIL_error_title", 0, buf);
#endif
}

/*-----------------------------------------------------------------------*/

int sys_get_language(int index, char *language_ret, char *dialect_ret)
{
    int retval;

    if (index > 0) {
        return 0;
    }

    JNIEnv *env = get_jni_env();
    jobject activity_obj = android_activity->clazz;
    jmethodID getUserLocale = get_method(
        NULL, "getUserLocale", "()Ljava/lang/String;");
    ASSERT(getUserLocale != 0, return 0);
    jstring j_locale = (*env)->CallObjectMethod(env, activity_obj,
                                                getUserLocale);
    ASSERT(!clear_exceptions(env), return 0);
    ASSERT(j_locale != 0, return 0);
    const char *locale = (*env)->GetStringUTFChars(env, j_locale, NULL);
    if (!locale || !*locale) {
        static uint8_t warned = 0;
        if (!warned) {
            DLOG("Failed to get user locale, or locale not set");
            warned = 1;
        }
        retval = 0;
    } else if (strlen(locale) < 2) {
        DLOG("Invalid locale: %s", locale);
        retval = 0;
    } else {
        language_ret[0] = locale[0];
        language_ret[1] = locale[1];
        language_ret[2] = 0;
        if (locale[2] == '_'
         && (locale[3] >= 'A' && locale[3] <= 'Z')
         && (locale[4] >= 'A' && locale[4] <= 'Z')) {
            dialect_ret[0] = locale[3];
            dialect_ret[1] = locale[4];
            dialect_ret[2] = 0;
        } else {
            *dialect_ret = 0;
        }
        retval = 1;
    }
    if (locale) {
        (*env)->ReleaseStringUTFChars(env, j_locale, locale);
    }
    (*env)->DeleteLocalRef(env, j_locale);
    return retval;
}

/*-----------------------------------------------------------------------*/

int sys_get_resource_path_prefix(char *prefix_buf, UNUSED int bufsize)
{
    *prefix_buf = '\0';
    return 0;
}

/*-----------------------------------------------------------------------*/

int sys_open_file(UNUSED const char *path)
{
    return 0;  // Not supported on Android.
}

/*-----------------------------------------------------------------------*/

int sys_open_url(const char *url)
{
    if (!url) {
        return 1;
    }

    JNIEnv *env = get_jni_env();
    jobject activity_obj = android_activity->clazz;
    jmethodID openURL = get_method(0, "openURL", "(Ljava/lang/String;)V");
    ASSERT(openURL != 0, return 0);
    jstring j_url = (*env)->NewStringUTF(env, url);
    ASSERT(j_url != 0, clear_exceptions(env); return 0);
    (*env)->CallVoidMethod(env, activity_obj, openURL, j_url);
    (*env)->DeleteLocalRef(env, j_url);
    return !clear_exceptions(env);
}

/*-----------------------------------------------------------------------*/

void sys_reset_idle_timer(void)
{
    if (!idle_timer_thread_id) {
        if (!(idle_reset_trigger = sys_semaphore_create(0, 1))) {
            DLOG("Failed to create idle reset trigger");
            return;
        }
        if (!(idle_timer_thread_id = thread_create(idle_timer_thread, NULL))) {
            DLOG("Failed to create idle timer thread");
            sys_semaphore_destroy(idle_reset_trigger);
            idle_reset_trigger = 0;
            return;
        }
    }

    sys_semaphore_signal(idle_reset_trigger);
}

/*-----------------------------------------------------------------------*/

int sys_set_performance_level(int level)
{
    return level == 0;  // Alternate performance levels not supported.
}

/*************************************************************************/
/*********************** Exported utility routines ***********************/
/*************************************************************************/

void android_toggle_navigation_bar(int state)
{
    if (android_api_level < 11) {
        return;
    }

    const int has_immersive = (android_api_level >= 19);

    JNIEnv *env = get_jni_env();
    jobject activity_obj = android_activity->clazz;
    jmethodID setSystemUiVisible = get_method(
        0, "setSystemUiVisible", "(Z)V");
    ASSERT(setSystemUiVisible != 0, return);
    (*env)->CallVoidMethod(env, activity_obj, setSystemUiVisible,
                           state && !has_immersive);
    ASSERT(!clear_exceptions(env));
}

/*-----------------------------------------------------------------------*/

int android_get_api_level(void)
{
    return android_api_level;
}

/*-----------------------------------------------------------------------*/

const char *android_get_hardware(void)
{
    return android_info_hardware;
}

/*-----------------------------------------------------------------------*/

const char *android_get_manufacturer(void)
{
    return android_info_manufacturer;
}

/*-----------------------------------------------------------------------*/

const char *android_get_model(void)
{
    return android_info_model;
}

/*-----------------------------------------------------------------------*/

const char *android_get_product(void)
{
    return android_info_product;
}

/*************************************************************************/
/*********************** Library-internal routines ***********************/
/*************************************************************************/

char *android_get_resource_string(const char *name)
{
    JNIEnv *env = get_jni_env();
    jobject activity_obj = android_activity->clazz;
    jmethodID getResourceString = get_method(
        NULL, "getResourceString", "(Ljava/lang/String;)Ljava/lang/String;");
    ASSERT(getResourceString != 0, return NULL);

    jstring j_name = (*env)->NewStringUTF(env, name);
    ASSERT(!clear_exceptions(env) && j_name != 0, return NULL;);

    jstring j_text = (*env)->CallObjectMethod(
        env, activity_obj, getResourceString, j_name);
    (*env)->DeleteLocalRef(env, j_name);
    ASSERT(!clear_exceptions(env), return NULL);
    if (!j_text) {
        DLOG("String resource \"%s\" not found", name);
        return NULL;
    }
    const char *const_text = (*env)->GetStringUTFChars(env, j_text, NULL);
    char *text = NULL;
    if (clear_exceptions(env) || !const_text) {
        DLOG("Failed to retrieve string resource \"%s\"", name);
    } else {
        text = mem_strdup(const_text, 0);
        if (!text) {
            DLOG("No memory for copy of string resource \"%s\"", name);
        }
        (*env)->ReleaseStringUTFChars(env, j_text, const_text);
    }
    (*env)->DeleteLocalRef(env, j_text);
    return text;
}

/*-----------------------------------------------------------------------*/

void android_lock_ui_thread(void)
{
    JNIEnv *env = get_jni_env();
    jobject activity_obj = android_activity->clazz;
    jmethodID lockUiThread = get_method(0, "lockUiThread", "()V");
    ASSERT(lockUiThread != 0, return);
    (*env)->CallVoidMethod(env, activity_obj, lockUiThread);
    ASSERT(!clear_exceptions(env));
}

/*-----------------------------------------------------------------------*/

void android_unlock_ui_thread(void)
{
    JNIEnv *env = get_jni_env();
    jobject activity_obj = android_activity->clazz;
    jmethodID unlockUiThread = get_method(0, "unlockUiThread", "()V");
    ASSERT(unlockUiThread != 0, return);
    (*env)->CallVoidMethod(env, activity_obj, unlockUiThread);
    ASSERT(!clear_exceptions(env));
}

/*-----------------------------------------------------------------------*/

int android_get_navigation_bar_state(void)
{
    JNIEnv *env = get_jni_env();
    jobject activity_obj = android_activity->clazz;
    jmethodID getSystemUiVisible = get_method(0, "getSystemUiVisible", "()Z");
    ASSERT(getSystemUiVisible != 0, return 1);
    return (*env)->CallBooleanMethod(env, activity_obj, getSystemUiVisible);
    ASSERT(!clear_exceptions(env));
}

/*-----------------------------------------------------------------------*/

void android_show_alert(int title_is_resource, const char *title,
                        int text_is_resource, const char *text)
{
    char *alloced_title = NULL, *alloced_text = NULL;
    if (title_is_resource) {
        alloced_title = android_get_resource_string(title);
        if (alloced_title) {
            title = alloced_title;
        }
    }
    if (text_is_resource) {
        alloced_text = android_get_resource_string(text);
        if (alloced_text) {
            text = alloced_text;
        }
    }

    JNIEnv *env = get_jni_env();
    jobject activity_obj = android_activity->clazz;
    jmethodID showAlert = get_method(0, "showAlert",
                                     "(Ljava/lang/String;"
                                     "Ljava/lang/String;"
                                     "Ljava/lang/String;"
                                     "Ljava/lang/String;"
                                     "Ljava/lang/String;"
                                     ")I");
    ASSERT(showAlert != 0, goto error_return);
    jstring j_title = (*env)->NewStringUTF(env, title);
    ASSERT(!clear_exceptions(env) && j_title != 0, goto error_free_title);
    jstring j_text = (*env)->NewStringUTF(env, text);
    ASSERT(!clear_exceptions(env) && j_text != 0, goto error_free_text);
    jstring j_button = (*env)->NewStringUTF(env, "OK");
    ASSERT(!clear_exceptions(env) && j_button != 0, goto error_free_button);
    (*env)->CallIntMethod(env, activity_obj, showAlert,
                          j_title, j_text, j_button, NULL, NULL);
    ASSERT(!clear_exceptions(env));
  error_free_button:
    (*env)->DeleteLocalRef(env, j_button);
  error_free_text:
    (*env)->DeleteLocalRef(env, j_text);
  error_free_title:
    (*env)->DeleteLocalRef(env, j_title);
  error_return:
    mem_free(alloced_title);
    mem_free(alloced_text);
    return;
}

/*-----------------------------------------------------------------------*/

void android_stop_idle_timer_thread(void)
{
    if (idle_timer_thread_id) {
        idle_timer_thread_stop = 1;
        thread_wait(idle_timer_thread_id);
        idle_timer_thread_id = 0;
        sys_semaphore_destroy(idle_reset_trigger);
        idle_reset_trigger = 0;
    }
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static int idle_timer_thread(UNUSED void *unused)
{
    JNIEnv *env = get_jni_env();
    jobject activity_obj = android_activity->clazz;
    jmethodID keepScreenOn = get_method(0, "keepScreenOn", "(Z)V");
    ASSERT(keepScreenOn != 0, return 0);

    while (!idle_timer_thread_stop) {
        sys_semaphore_wait(idle_reset_trigger, -1);

        DLOG("Acquiring screen lock");
        (*env)->CallVoidMethod(env, activity_obj, keepScreenOn, 1);
        ASSERT(!clear_exceptions(env));

        while (sys_semaphore_wait(idle_reset_trigger, IDLE_TIMEOUT)) {/*spin*/}

        DLOG("Releasing screen lock");
        (*env)->CallVoidMethod(env, activity_obj, keepScreenOn, 0);
        ASSERT(!clear_exceptions(env));
    }

    return 0;
}

/*************************************************************************/
/*************************************************************************/
