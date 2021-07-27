/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/android/internal.h: Common header for declarations internal
 * to Android-specific code.
 */

#ifndef SIL_SRC_SYSDEP_ANDROID_INTERNAL_H
#define SIL_SRC_SYSDEP_ANDROID_INTERNAL_H

struct InputEvent;
struct SysFile;

/*************************************************************************/
/*************************************************************************/

/* System headers. */

#include <android/api-level.h>
#include <android/input.h>
#include <android/keycodes.h>
#include <android/native_activity.h>
#include <android/native_window.h>
#include <jni.h>

/* Thread priority constants from android.os.Process.  These are Linux
 * "nice" values, and they should be arithmetically inverted for use with
 * the SIL thread routines. */

enum {
    THREAD_PRIORITY_AUDIO = -16,
    THREAD_PRIORITY_BACKGROUND = 10,
    THREAD_PRIORITY_DEFAULT = 0,
    THREAD_PRIORITY_DISPLAY = -4,
    THREAD_PRIORITY_FOREGROUND = -2,
    THREAD_PRIORITY_LOWEST = 19,
    THREAD_PRIORITY_URGENT_AUDIO = -19,
    THREAD_PRIORITY_URGENT_DISPLAY = -8,
};

/*-----------------------------------------------------------------------*/

/* Library-internal declarations. */


/******** SILActivity.java ********/

/**
 * BuildInfoID:  ID values for passing to getBuildInfo().
 */
typedef enum BuildInfoID {
    BUILD_INFO_BOARD        =  1,
    BUILD_INFO_BOOTLOADER   =  2,
    BUILD_INFO_BRAND        =  3,
    BUILD_INFO_CPU_ABI      =  4,
    BUILD_INFO_CPU_ABI2     =  5,
    BUILD_INFO_DEVICE       =  6,
    BUILD_INFO_DISPLAY      =  7,
    BUILD_INFO_FINGERPRINT  =  8,
    BUILD_INFO_HARDWARE     =  9,
    BUILD_INFO_HOST         = 10,
    BUILD_INFO_ID           = 11,
    BUILD_INFO_MANUFACTURER = 12,
    BUILD_INFO_MODEL        = 13,
    BUILD_INFO_PRODUCT      = 14,
    BUILD_INFO_RADIO        = 15,
    BUILD_INFO_SERIAL       = 16,
    BUILD_INFO_TAGS         = 17,
    BUILD_INFO_TYPE         = 18,
    BUILD_INFO_USER         = 19,
} BuildInfoID;

/******** activity.c ********/

/**
 * android_api_level:  API level of the runtime environment, one of the
 * Build.VERSION_CODES constants.
 */
extern int android_api_level;

/**
 * android_info_*:  Strings identifying the hardware on which the program
 * is running.
 */
extern const char *android_info_hardware;
extern const char *android_info_manufacturer;
extern const char *android_info_model;
extern const char *android_info_product;

/**
 * android_activity:  Activity object for this instance of the application.
 */
extern ANativeActivity *android_activity;

/**
 * android_window:  Window object created for us by the system.
 */
extern ANativeWindow *android_window;

/**
 * android_internal_data_path:  Path to the application's writable data
 * directory on the internal storage device.
 */
extern const char *android_internal_data_path;

/**
 * android_external_data_path:  Path to the application's writable data
 * directory on the external storage device, or NULL if no external storage
 * is available.
 */
extern const char *android_external_data_path;

/**
 * android_external_root_path:  Path to the root of the external storage
 * device, or NULL if no external storage is available.
 */
extern const char *android_external_root_path;

/**
 * android_suspend_semaphore:  Semaphore used to signal that the main thread
 * has acknowledged a suspend request and is ready for the process to be
 * suspended.  Waited on by the activity stop callback and signalled by
 * sys_input_acknowledge_suspend_request().
 */
SysSemaphoreID android_suspend_semaphore;

/**
 * android_resume_semaphore:  Semaphore used to signal that a suspend/resume
 * cycle has completed.  Waited on by sys_input_acknowledge_suspend_request()
 * and signalled in the activity resume callback (or in the window-created
 * callback, if the window was destroyed while the process was suspended).
 */
SysSemaphoreID android_resume_semaphore;

/**
 * android_suspend_requested:  Flag indicating whether the system has sent
 * a suspend request (via onStop()).  Once set, remains true until the next
 * onStart() call.
 */
extern uint8_t android_suspend_requested;

/**
 * android_quit_requested:  Flag indicating whether the system has asked us
 * to quit (via onDestroy()).  Once set, this is never cleared while the
 * android_main() thread is running.
 */
extern uint8_t android_quit_requested;

/**
 * get_jni_env:  Return the JNI environment for the current thread.
 * This function always succeeds.
 *
 * [Return value]
 *     JNI environment object.
 */
extern JNIEnv *get_jni_env(void);

/**
 * get_class:  Return a local reference to the Java class of the given name.
 * This function does not raise any Java exceptions.
 *
 * [Parameters]
 *     name: Class name.
 * [Return value]
 *     Class object reference, or zero if the class is unknown.
 */
extern jclass get_class(const char *name);

/**
 * get_method:  Return the method ID for the Java method of the given name
 * in the given class.  This function does not raise any Java exceptions.
 *
 * [Parameters]
 *     class: Java class object in which to look up the method, or zero to
 *         look up a method in the activity class.
 *     method: Method name.
 *     signature: JNI signature of method.
 * [Return value]
 *     JNI method ID, or zero if the method does not exist.
 */
extern jmethodID get_method(jclass class, const char *method,
                            const char *signature);

/**
 * get_static_method:  Return the method ID for the static Java method of
 * the given name in the given class.  This function does not raise any
 * Java exceptions.
 *
 * [Parameters]
 *     class: Java class object in which to look up the method, or zero to
 *         look up a method in the activity class.
 *     method: Method name.
 *     signature: JNI signature of method.
 * [Return value]
 *     JNI method ID, or zero if the method does not exist.
 */
extern jmethodID get_static_method(jclass class, const char *method,
                                   const char *signature);

/**
 * clear_exceptions:  If a Java exception is pending, report it to the
 * system error channel and clear the exception.
 *
 * [Parameters]
 *     env: JNI environment object.
 * [Return value]
 *     True if an exception was pending, false if not.
 */
extern int clear_exceptions(JNIEnv *env);

/**
 * check_for_expansion_files:  Check whether any APK expansion files are
 * present, and record their pathnames if so.
 *
 * [Return value]
 *     False if a fatal error occurred, true otherwise.
 */
extern int check_for_expansion_files(void);


/******** files.c ********/

/**
 * android_file_base_offset:  Return the base offset in the physical file
 * for the logical file referenced by the given file handle.  For normal
 * files, this is always zero; for files stored in the .apk package, this
 * is the offset of the file data within the package.
 *
 * [Parameters]
 *     fh: File handle.
 * [Return value]
 *     Base offset of file data.
 */
extern int64_t android_file_base_offset(const struct SysFile *fh);


/******** graphics.c ********/

/**
 * android_display_width, android_display_height:  Return the width or
 * height of the display device.  These values will never change while the
 * program is running.
 *
 * These functions assume that the program is running in landscape mode.
 *
 * [Return value]
 *     Width or height of display device, in pixels.
 */
extern CONST_FUNCTION int android_display_width(void);
extern CONST_FUNCTION int android_display_height(void);

/**
 * android_suspend_graphics:  Suspend the graphics subsystem, terminating
 * the EGL display connection.  Should be called when the window is
 * destroyed due to the activity being paused.
 *
 * This function does nothing if the graphics subsystem has not been
 * initialized.
 */
extern void android_suspend_graphics(void);

/**
 * android_resume_graphics:  Restore the graphics subsystem to its state
 * before being suspended.  (Texture data will _not_ be preserved.)  Should
 * be called after the window has been recreated following a pause/resume
 * cycle; must not be called without a window.
 *
 * This function does nothing if the graphics subsystem has not been
 * initialized or is not currently suspended.
 */
extern void android_resume_graphics(void);


/******** input.c ********/

/**
 * android_handle_input_event:  Process an input event.  Called from the
 * main (Java) thread when an input event arrives.
 *
 * [Parameters]
 *     event: Input event object.
 * [Return value]
 *     True if the event was processed, false to continue propagating the
 *     event.
 */
extern int32_t android_handle_input_event(AInputEvent *event);

/**
 * android_forward_input_event:  Pass the given InputEvent to the callback
 * function registered with sys_input_init().
 *
 * [Parameters]
 *     event: Event to forward.
 */
extern void android_forward_input_event(const struct InputEvent *event);


/******** main.c ********/

/**
 * android_main:  sil__main() wrapper for Android.  This routine will be
 * called in its own thread of the Android process.
 *
 * [Parameters]
 *     param: Thread parameter (unused).
 */
extern int android_main(void *param);


/******** misc.c ********/

/**
 * android_get_resource_string:  Return the string corresponding to the
 * given string resource name.  The returned string is stored in a buffer
 * allocated with mem_alloc(); the buffer should be freed with mem_free()
 * when no longer needed.
 *
 * [Parameters]
 *     name: Name of string resource to return.
 * [Return value]
 *     Corresponding string, or NULL if the resource is not found.
 */
extern char *android_get_resource_string(const char *name);

/**
 * android_lock_ui_thread, android_unlock_ui_thread:  Lock or unlock the
 * UI thread.  Required when modifying UI state.
 */
extern void android_lock_ui_thread(void);
extern void android_unlock_ui_thread(void);

/**
 * android_get_navigation_bar_state:  Return whether the system navigation
 * bar (with the Back/Home/Recent softkeys) is displayed on Android 3.0+
 * devices without physical navigation buttons.
 *
 * [Return value]
 *     True if the navigation bar is displayed, false if not.
 */
extern int android_get_navigation_bar_state(void);

/**
 * android_show_alert:  Display an alert dialog with the given title and
 * body text, and wait for the user to dismiss it.
 *
 * [Parameters]
 *     title_is_resource: True if "title" is a resource name, false if a
 *         literal string.
 *     title: Dialog title.
 *     text_is_resource: True if "text" is a resource name, false if a
 *         literal string.
 *     text: Dialog body text.
 */
extern void android_show_alert(int title_is_resource, const char *title,
                               int text_is_resource, const char *text);

/**
 * android_stop_idle_timer_thread:  Stop the background thread used to
 * handle resetting the system's idle timer.  This function does nothing
 * if the thread has not been started.
 */
extern void android_stop_idle_timer_thread(void);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_ANDROID_INTERNAL_H
