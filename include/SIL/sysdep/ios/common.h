/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/sysdep/ios/common.h: Global declarations specific to iOS.
 */

#ifndef SIL_SYSDEP_IOS_COMMON_H
#define SIL_SYSDEP_IOS_COMMON_H

EXTERN_C_BEGIN

/*************************************************************************/
/*************************************************************************/

/* A few Mach kernel function names unfortunately collide with our own
 * functions.  We can't rename the system functions, so we grudgingly
 * #define the SIL functions to different names. */

/* Make sure the relevant system headers are #included first, so the
 * system functions are declared with the proper names. */
#include <mach/semaphore.h>
#include <mach/task.h>

#define semaphore_create   sil_semaphore_create
#define semaphore_destroy  sil_semaphore_destroy
#define semaphore_wait     sil_semaphore_wait
#define semaphore_signal   sil_semaphore_signal
#define thread_create      sil_thread_create

/*-----------------------------------------------------------------------*/

/**
 * ios_allow_autorotation:  If true, the display will autorotate to match
 * the device orientation; if false, the display orientation will remain
 * fixed regardless of device orientation.
 *
 * The default value is true (autorotation enabled).
 */
extern uint8_t ios_allow_autorotation;

/**
 * ios_current_player:  Return the player ID of the current player.  If
 * GameKit support is enabled and the user is signed into Game Center,
 * this is the current authenticated player ID from GameKit; otherwise, it
 * is the authenticated player ID most recently seen, or NULL if no player
 * ID has ever been seen.
 *
 * [Return value]
 *     Current player ID, or NULL if none.
 */
extern const char *ios_current_player(void);

/**
 * ios_get_device:  Return the hardware device type (iPhone, iPad, ...) on
 * which the program is running.
 *
 * [Return value]
 *     Device type constant (IOS_DEVICE_*), or zero if the device is unknown.
 */
typedef enum iOSDevice {
    IOS_DEVICE_UNKNOWN   = 0,
    IOS_DEVICE_IPHONE    = 1,
    IOS_DEVICE_IPOD      = 2,
    IOS_DEVICE_IPAD      = 3,
    IOS_DEVICE_APPLE_TV  = 4,
} iOSDevice;
extern iOSDevice ios_get_device(void);

/**
 * ios_get_model:  Return the hardware model on which the program is running.
 *
 * [Return value]
 *     Hardware model constant (IOS_MODEL_*), or zero if the model is unknown.
 */
#define MAKE_MODEL(device,version) (IOS_DEVICE_##device << 8 | (version))
typedef enum iOSModel {
    IOS_MODEL_UNKNOWN        = 0,                       // Unknown model

    IOS_MODEL_IPHONE_1       = MAKE_MODEL(IPHONE,1),    // Original iPhone
    IOS_MODEL_IPHONE_3G      = MAKE_MODEL(IPHONE,2),    // iPhone 3G
    IOS_MODEL_IPHONE_3GS     = MAKE_MODEL(IPHONE,3),    // iPhone 3GS
    IOS_MODEL_IPHONE_4       = MAKE_MODEL(IPHONE,4),    // iPhone 4
    IOS_MODEL_IPHONE_4S      = MAKE_MODEL(IPHONE,5),    // iPhone 4S
    IOS_MODEL_IPHONE_5       = MAKE_MODEL(IPHONE,6),    // iPhone 5
    IOS_MODEL_IPHONE_5C      = MAKE_MODEL(IPHONE,7),    // iPhone 5C
    IOS_MODEL_IPHONE_5S      = MAKE_MODEL(IPHONE,8),    // iPhone 5S
    IOS_MODEL_IPHONE_6       = MAKE_MODEL(IPHONE,9),    // iPhone 6
    IOS_MODEL_IPHONE_6_PLUS  = MAKE_MODEL(IPHONE,10),   // iPhone 6 Plus
    IOS_MODEL_IPHONE_6S      = MAKE_MODEL(IPHONE,11),   // iPhone 6s
    IOS_MODEL_IPHONE_6S_PLUS = MAKE_MODEL(IPHONE,12),   // iPhone 6s Plus
    IOS_MODEL_IPHONE_SE      = MAKE_MODEL(IPHONE,13),   // iPhone SE
    IOS_MODEL_IPHONE_7       = MAKE_MODEL(IPHONE,14),   // iPhone 7
    IOS_MODEL_IPHONE_7_PLUS  = MAKE_MODEL(IPHONE,15),   // iPhone 7 Plus
    IOS_MODEL_IPHONE_8       = MAKE_MODEL(IPHONE,16),   // iPhone 8
    IOS_MODEL_IPHONE_8_PLUS  = MAKE_MODEL(IPHONE,17),   // iPhone 8 Plus
    IOS_MODEL_IPHONE_X       = MAKE_MODEL(IPHONE,18),   // iPhone X
    IOS_MODEL_IPHONE_XS      = MAKE_MODEL(IPHONE,19),   // iPhone XS
    IOS_MODEL_IPHONE_XS_MAX  = MAKE_MODEL(IPHONE,20),   // iPhone XS Max
    IOS_MODEL_IPHONE_XR      = MAKE_MODEL(IPHONE,21),   // iPhone XR
    IOS_MODEL_IPHONE_11      = MAKE_MODEL(IPHONE,22),   // iPhone 11
    IOS_MODEL_IPHONE_11_PRO  = MAKE_MODEL(IPHONE,23),   // iPhone 11 Pro
    IOS_MODEL_IPHONE_11_MAX  = MAKE_MODEL(IPHONE,24),   // iPhone 11 Pro Max

    IOS_MODEL_IPOD_1         = MAKE_MODEL(IPOD,1),      // iPod touch 1st gen
    IOS_MODEL_IPOD_2         = MAKE_MODEL(IPOD,2),      // iPod touch 2nd gen
    IOS_MODEL_IPOD_3         = MAKE_MODEL(IPOD,3),      // iPod touch 3rd gen
    IOS_MODEL_IPOD_4         = MAKE_MODEL(IPOD,4),      // iPod touch 4th gen
    IOS_MODEL_IPOD_5         = MAKE_MODEL(IPOD,5),      // iPod touch 5th gen
    IOS_MODEL_IPOD_6         = MAKE_MODEL(IPOD,6),      // iPod touch 6th gen
    IOS_MODEL_IPOD_7         = MAKE_MODEL(IPOD,7),      // iPod touch 7th gen

    IOS_MODEL_IPAD_1         = MAKE_MODEL(IPAD,1),      // Original iPad
    IOS_MODEL_IPAD_2         = MAKE_MODEL(IPAD,2),      // iPad 2
    IOS_MODEL_IPAD_3         = MAKE_MODEL(IPAD,3),      // iPad 3rd generation
    IOS_MODEL_IPAD_4         = MAKE_MODEL(IPAD,4),      // iPad 4th generation
    IOS_MODEL_IPAD_5         = MAKE_MODEL(IPAD,5),      // iPad 5th gen (2017)
    IOS_MODEL_IPAD_6         = MAKE_MODEL(IPAD,6),      // iPad 6th gen (2018)
    IOS_MODEL_IPAD_7         = MAKE_MODEL(IPAD,7),      // iPad 7th gen (2019)
    IOS_MODEL_IPAD_AIR       = MAKE_MODEL(IPAD,31),     // iPad Air
    IOS_MODEL_IPAD_AIR_2     = MAKE_MODEL(IPAD,32),      // iPad Air 2
    IOS_MODEL_IPAD_AIR_3     = MAKE_MODEL(IPAD,33),     // iPad Air 3rd gen
    IOS_MODEL_IPAD_MINI_1    = MAKE_MODEL(IPAD,51),      // iPad mini
    IOS_MODEL_IPAD_MINI_2    = MAKE_MODEL(IPAD,52),      // iPad mini 2
    IOS_MODEL_IPAD_MINI_3    = MAKE_MODEL(IPAD,53),      // iPad mini 3
    IOS_MODEL_IPAD_MINI_4    = MAKE_MODEL(IPAD,54),     // iPad mini 4
    IOS_MODEL_IPAD_MINI_5    = MAKE_MODEL(IPAD,55),     // iPad mini 5th gen
    IOS_MODEL_IPAD_PRO       = MAKE_MODEL(IPAD,71),     // iPad Pro (12.9")
    IOS_MODEL_IPAD_PRO_9IN   = MAKE_MODEL(IPAD,72),     // iPad Pro (9.7")
    IOS_MODEL_IPAD_PRO_2G    = MAKE_MODEL(IPAD,73),     // iPad Pro 2nd gen (12.9")
    IOS_MODEL_IPAD_PRO_2G_10IN = MAKE_MODEL(IPAD,74),   // iPad Pro 2nd gen (10.5")
    IOS_MODEL_IPAD_PRO_3G    = MAKE_MODEL(IPAD,75),     // iPad Pro 3rd gen (12.9")
    IOS_MODEL_IPAD_PRO_3G_11IN = MAKE_MODEL(IPAD,76),   // iPad Pro 3rd gen (11")

    IOS_MODEL_APPLE_TV_4     = MAKE_MODEL(APPLE_TV,4),  // Apple TV 4th gen
    IOS_MODEL_APPLE_TV_4K    = MAKE_MODEL(APPLE_TV,5),  // Apple TV 4K
} iOSModel;
#undef MAKE_MODEL
extern iOSModel ios_get_model(void);

/**
 * ios_get_display_size:  Return the approximate diagonal size of the
 * display, in inches, or 0 if the display size is unknown (as for Apple
 * TV).  This value will never change while the program is running.
 *
 * [Return value]
 *     Approximate display size (diagonal), in inches, or 0 if unknown.
 */
extern CONST_FUNCTION float ios_get_display_size(void);

/**
 * ios_get_native_refresh_rate:  Return the native (maximum) refresh rate
 * of the display device.  The system API wrapped by this function returns
 * an integer (presumably the rounded value of the true refresh rate), so
 * this function also returns an integer rather than a float.
 *
 * [Return value]
 *     Display device refresh rate, in Hz.
 */
extern CONST_FUNCTION int ios_get_native_refresh_rate(void);

/**
 * ios_sound_set_category:  Set the iOS audio session category parameters.
 * The default is IOS_SOUND_BACKGROUND.
 *
 * This function may be called after audio output has been started (with
 * sound_open_device()).  However, if audio from other apps is playing,
 * this function may cause that audio to be silenced and may block until
 * the fadeout is complete, even if the new category allows mixing with
 * other audio.  These caveats do not apply when setting the category
 * before calling sound_open_device().
 *
 * Note that the descriptions of the sound categories are correct as of iOS
 * 7.1, but the behavior of iOS may change in future versions.
 *
 * [Parameters]
 *     new_category: New category to set (one of IOS_SOUND_*).
 * [Return value]
 *     True if the category was successfully set, false if not.
 */
typedef enum iOSSoundCategory {
    /* Audio output is never affected by the "Mute" (also known as "Silent"
     * or "Ring/Silent") switch.  Any other apps playing audio will be
     * silenced while this app is in the foreground. */
    IOS_SOUND_FOREGROUND,
    /* Audio output is never affected by the "Mute" switch.  Audio from
     * other apps will mix with audio from this app. */
    IOS_SOUND_FOREGROUND_MIX,
    /* Audio output through the device's built-in speaker is silenced when
     * the "Mute" switch is on.  Audio from other apps will mix with audio
     * audio from this app. */
    IOS_SOUND_BACKGROUND,
} iOSSoundCategory;
extern int ios_sound_set_category(iOSSoundCategory new_category);

/**
 * ios_status_bar_visible_height:  Return the visible height of the status
 * bar, or zero if the status bar is hidden.
 *
 * [Return value]
 *     Visible height of status bar, in pixels.
 */
extern int ios_status_bar_visible_height(void);

/**
 * ios_toggle_status_bar:  Toggle the system status bar on or off.
 *
 * [Parameters]
 *     state: True to show the status bar, false to hide it.
 */
extern void ios_toggle_status_bar(int state);

/**
 * ios_version_is_at_least:  Return whether the version of iOS on which
 * the program is running is at least the given version.  Prerelease (beta)
 * OS versions are treated as "less than" (earlier than) the release
 * version with the same version code; for example, "6.0b1" is less than
 * "6.0".
 *
 * The version passed in for comparison must be a release version, not a
 * prerelease version; any suffix (like "b1") on a version component will
 * be ignored.
 *
 * [Parameters]
 *     version: Version number to compare against, as a string.
 * [Return value]
 *     True if the runtime OS version is equal to or later than the given
 *     version, false if not.
 */
extern int ios_version_is_at_least(const char *version);

/**
 * ios_version_string:  Return the version of iOS on which the program is
 * running.  This string will never change while the program is running.
 *
 * If the version number cannot be obtained from the system, "0.0" is
 * returned instead.
 *
 * [Return value]
 *     Runtime iOS version number.
 */
extern const char *ios_version_string(void);

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_SYSDEP_IOS_COMMON_H
