/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/ios/util.h: Header for iOS utility routines and shared data.
 */

#ifndef SIL_SRC_SYSDEP_IOS_UTIL_H
#define SIL_SRC_SYSDEP_IOS_UTIL_H

/*************************************************************************/
/*************************************************************************/

/*----------------------------- Shared data -----------------------------*/

/**
 * ios_application_is_terminating:  Indicates whether a termination request
 * has been received from the system.  Under current versions of iOS, this
 * is never set.
 */
extern uint8_t ios_application_is_terminating;

/**
 * ios_application_is_suspending:  Indicates whether a suspend request
 * has been received from the system (normally because the Home button was
 * pressed).  This flag is set when a suspend request is received and
 * cleared when the suspend finishes (i.e., when the program is
 * reactivated).
 */
extern uint8_t ios_application_is_suspending;

/**
 * ios_suspend_semaphore:  Semaphore used to signal that the main thread
 * has acknowledged a suspend request and is ready for the process to be
 * suspended.  Waited on by the -[applicationWillResignActive:] callback
 * and signalled by sys_input_acknowledge_suspend_request().
 */
SysSemaphoreID ios_suspend_semaphore;

/**
 * ios_resume_semaphore:  Semaphore used to signal that a suspend/resume
 * cycle has completed.  Waited on by sys_input_acknowledge_suspend_request()
 * and signalled by the -[applicationDidBecomeActive:] callback.
 */
SysSemaphoreID ios_resume_semaphore;

/*-------------------- Device/application parameters --------------------*/

/**
 * ios_get_model_for:  Return the hardware model ID for the given machine
 * identifier (a string like "iPhone1,1").
 *
 * This function implements the machine-ID-to-model-ID translation for
 * ios_get_model(); it is separated out for testing purposes.
 *
 * [Return value]
 *     Hardware model constant (IOS_MODEL_*), or zero if the model is unknown.
 */
extern iOSModel ios_get_model_for(const char *machine);

/**
 * ios_display_width, ios_display_height:  Return the width or height of
 * the display device.  These values will never change while the program is
 * running.
 *
 * These functions assume that the program is running in landscape mode.
 *
 * [Return value]
 *     Width or height of display device, in points.
 */
extern CONST_FUNCTION int ios_display_width(void);
extern CONST_FUNCTION int ios_display_height(void);

/**
 * ios_display_scale:  Return the display device's scale factor (the value
 * that must be stored in UIView.contentScaleFactor to avoid stretching).
 *
 * [Return value]
 *     Display device scale factor.
 */
extern CONST_FUNCTION float ios_display_scale(void);

/**
 * ios_get_application_name:  Return the name of the current application,
 * or "The application" (capitalized) if the application name cannot be
 * determined.
 *
 * [Return value]
 *     Application name.
 */
extern const char *ios_get_application_name(void);

/**
 * ios_get_application_support_path:  Return the pathname of the
 * "Application Support" directory for the current application.
 *
 * This function always succeeds.  The returned value is stored in a
 * static buffer.
 *
 * [Return value]
 *     "Application Support" directory pathname.
 */
extern const char *ios_get_application_support_path(void);

/**
 * ios_get_documents_path:  Return the pathname of the "Documents"
 * directory for the current application.
 *
 * This function always succeeds.  The returned value is stored in a
 * static buffer.
 *
 * [Return value]
 *     "Documents" directory pathname.
 */
extern const char *ios_get_documents_path(void);

/*--------------------- Frame presentation/counting ---------------------*/

/**
 * ios_set_refresh_rate:  Set the refresh rate for the CADisplayLink
 * instance used for timing.  If the argument is not a factor of the
 * display's native refresh rate, the refresh rate used by CADisplayLink
 * may differ.
 *
 * The "rate" parameter is of type int to match that of the associated
 * CADisplayLink method.
 *
 * [Parameters]
 *     rate: Desired refresh rate, in Hz.
 */
extern void ios_set_refresh_rate(int rate);

/**
 * ios_present_view:  Present the current contents of the view to the
 * display device.
 */
extern void ios_present_view(void);

/**
 * ios_vsync:  Wait for the next vertical sync event.
 */
extern void ios_vsync(void);

/**
 * ios_get_frame_counter:  Return the global frame counter (incremented
 * once per 1/60-second frame).
 *
 * [Return value]
 *     Frame counter.
 */
extern int ios_get_frame_counter(void);

/**
 * ios_get_frame_interval:  Return the minimum number of hardware frames
 * per application frame.
 *
 * [Return value]
 *     Application frame interval, in hardware frames.
 */
extern PURE_FUNCTION int ios_get_frame_interval(void);

/**
 * ios_set_frame_interval:  Set the minimum number of hardware frames per
 * application frame.
 *
 * [Parameters]
 *     interval: Application frame interval, in hardware frames.
 */
extern void ios_set_frame_interval(int interval);

/*--------------------- V-sync function management ----------------------*/

/**
 * iOSVsyncFunction:  Type of function passed to ios_register_vsync_function().
 *
 * [Parameters]
 *     userdata: Parameter value passed to ios_register_vsync_function().
 */
typedef void iOSVSyncFunction(void *userdata);

/**
 * ios_register_vsync_function:  Register a function to be called at the
 * next vertical sync.  The function will only be called once, but it may
 * re-add itself to the call list by calling ios_register_vsync_function()
 * again before it returns.
 *
 * Note that the function will be called from a separate thread, and thus
 * must lock (when appropriate) any shared data it needs to access.
 *
 * Due to apparent multithreading or locking issues, all iOS calls which
 * affect the GUI must be performed in vertical sync callback functions.
 *
 * [Parameters]
 *     function: Function to call.
 *     userdata: Opaque parameter to function.
 */
extern void ios_register_vsync_function(iOSVSyncFunction *function,
                                        void *userdata);

/**
 * ios_call_vsync_functions:  Call all functions registered to be called
 * at vertical sync, clearing each one as it is called.  Functions which
 * wish to be called repeatedly should re-add themselves before returning.
 *
 * This function should only be called from the vertical sync handler.
 */
extern void ios_call_vsync_functions(void);

/*----------------------- Miscellaneous functions -----------------------*/

/**
 * ios_compare_versions:  Compare version numbers in the iOS style.  Both
 * version numbers must be non-empty strings.
 *
 * [Parameters]
 *     version1, version2: Version numbers to compare, as strings.
 * [Return value]
 *     +1 if version1 > version2
 *      0 if version1 == version2
 *     -1 if version1 < version2
 */
extern int ios_compare_versions(const char *version1, const char *version2);

/**
 * ios_enable_idle_timer:  Enable or disable the system idle timer.
 *
 * [Parameters]
 *     enable: True to enable the idle timer, false to disable it.
 */
extern void ios_enable_idle_timer(int enable);

/**
 * ios_open_url:  Open the given URL in the system browser (Safari).  This
 * may cause the calling program to be terminated by the system.
 *
 * [Parameters]
 *     url: URL to open (must be non-NULL).
 */
extern void ios_open_url(const char *url);

/**
 * ios_show_dialog_formatted:  Call ios_dialog() with the localized strings
 * looked up from the given resource IDs.  The dialog text is treated as a
 * printf()-style format string.
 *
 * [Parameters]
 *     title_id: Resource ID for dialog title.
 *     format_id: Resource ID for dialog text format string.
 *     ...: Format arguments for dialog text.
 */
extern void ios_show_dialog_formatted(const char *title_id,
                                      const char *format_id, ...);

/**
 * ios_stop_idle_timer_thread:  Stop the background thread used to handle
 * resetting the system's idle timer.  This function does nothing if the
 * thread has not been started.
 *
 * This function is defined in misc.c.
 */
extern void ios_stop_idle_timer_thread(void);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_IOS_UTIL_H
