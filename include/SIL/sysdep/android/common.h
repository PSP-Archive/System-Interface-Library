/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/sysdep/android/common.h: Global declarations specific to
 * Android.
 */

#ifndef SIL_SYSDEP_ANDROID_COMMON_H
#define SIL_SYSDEP_ANDROID_COMMON_H

EXTERN_C_BEGIN

/*************************************************************************/
/*************************************************************************/

/* Undo #defines from configs/android.h that hide definitions of unneeded
 * system functions, and declare strtof() since we hid the original one. */

#undef major
#undef minor

#ifdef SIL_UTILITY_INCLUDE_STRTOF
# undef strtof
extern float strtof(const char *s, char **endptr);
#endif

/*-----------------------------------------------------------------------*/

/* Global utility functions. */

/*----------------------------------*/

/******** activity.c ********/

/**
 * android_expansion_file_path:  Return the pathname of the APK expansion
 * file for the given index.  Currently, the following index values are
 * supported:
 *    0: Google Play "main" expansion file
 *    1: Google Play "patch" expansion file
 *
 * [Parameters]
 *     index: Expansion file index.
 * [Return value]
 *     Expansion file pathname, or NULL if no such expansion file exists.
 */
extern const char *android_expansion_file_path(int index);

/**
 * android_external_mount_point:  Return the pathname where the external
 * storage tree is mounted, typically "/sdcard" or "/mnt/sdcard".
 */
extern const char *android_external_mount_point(void);

/**
 * android_request_permission:  Request a permission from the user, and
 * return whether it was granted.  The permission must also have been
 * listed in the AndroidManifest file.
 *
 * On Android, permission requests are asynchronous, and may cause the
 * program to be suspended or even terminated.  Accordingly, the caller
 * must loop over this function until it returns a nonnegative value,
 * checking for suspend and quit requests, in a manner such as the
 * following:
 *
 *     int quit = 0;
 *     int result;
 *     while ((result = android_request_permission(...)) < 0) {
 *         input_update();
 *         if (input_is_suspend_requested()) {
 *             input_acknowledge_suspend_request();
 *         } else if (input_is_quit_requested()) {
 *             quit = 1;
 *             break;
 *         }
 *     }
 *     if (quit) {
 *         // Program is restarting as part of the permission grant
 *     } else if (result) {
 *         // Permission was granted
 *     } else {
 *         // Permission was denied
 *     }
 *
 * Behavior is undefined if, after this function returns -1 for a given
 * permission, it is subsequently called with a different permission.
 *
 * [Parameters]
 *     permission: Permission to request (ANDROID_PERMISSION_*).
 * [Return value]
 *     1 if the permission was granted, 0 if it was denied, or -1 if the
 *     request is pending.
 */
typedef enum AndroidPermission {
    /* External storage access.  Note that these permissions are _not_
     * required to access the app's external data directory (the one used
     * by the userdata functions). */
    ANDROID_PERMISSION_READ_EXTERNAL_STORAGE,
    ANDROID_PERMISSION_WRITE_EXTERNAL_STORAGE,
} AndroidPermission;
extern int android_request_permission(AndroidPermission permission);

/******** graphics.c ********/

/**
 * android_using_immersive_mode:  Return whether the current display mode
 * is using the Android 4.4+ "immersive" mode (which hides the navigation
 * bar).  Immersive mode cannot be set directly; SIL always uses immersive
 * mode on devices which support it.
 *
 * [Return value]
 *     True if the current display mode uses immersive mode, false if not.
 */
extern PURE_FUNCTION int android_using_immersive_mode(void);

/**
 * android_display_size_inches:  Return the approximate diagonal size of
 * the display, in inches.  The accuracy of this value depends on whether
 * the underlying drivers report the correct dots-per-inch values to the OS.
 * This value will never change while the program is running.
 *
 * [Return value]
 *     Approximate display size (diagonal), in inches.
 */
extern CONST_FUNCTION float android_display_size_inches(void);


/******** misc.c ********/

/**
 * android_toggle_navigation_bar:  Set how the system navigation bar (with
 * the Back, Home, and Recent Apps softkeys) should be displayed on Android
 * versions 3.0 through 4.3.  If state is true, the navigation bar is
 * displayed; if state is false (the default), the softkey icons are dimmed.
 *
 * On devices with physical navigation buttons, the navigation bar is not
 * displayed in the first place, so this function has no effect.
 *
 * On devices running Android 4.4 or later, SIL uses "immersive mode", in
 * which the navigation bar is hidden and can be displayed by swiping
 * inward from the edge of the screen.  This function has no effect on
 * such devices.
 *
 * [Parameters]
 *     state: True to display navigation softkeys, false to hide them.
 */
extern void android_toggle_navigation_bar(int state);

/**
 * android_get_api_level:  Return the API level (version code, such as
 * 21 for Android 5.0 Lollipop) of the runtime environment.
 *
 * [Return value]
 *     Android API level.
 */
extern int android_get_api_level(void);

/**
 * android_get_hardware, android_get_manufacturer, android_get_model,
 * android_get_product:  Return the hardware, manufacturer, model, or
 * product string for the device on which the program is running.
 *
 * [Return value]
 *     Device information string.
 */
extern const char *android_get_hardware(void);
extern const char *android_get_manufacturer(void);
extern const char *android_get_model(void);
extern const char *android_get_product(void);

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_SYSDEP_ANDROID_COMMON_H
