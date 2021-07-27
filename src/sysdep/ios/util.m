/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/ios/util.m: Utility routines and shared data for iOS.
 */

/*
 * Note that we use the standard library's memory management functions
 * (malloc() and friends) rather than mem_*() in several utility functions
 * below, since they may be called before the memory management subsystem
 * has been initialized.
 */
#undef SIL_MEMORY_FORBID_MALLOC

#define IN_SYSDEP

#import "src/base.h"
#import "src/math.h"
#import "src/sysdep.h"
#import "src/sysdep/ios/dialog.h"
#import "src/sysdep/ios/util.h"
#import "src/sysdep/ios/view.h"
#import "src/sysdep/macosx/strings.h"

#import <pthread.h>
#import <sys/sysctl.h>

#import <OpenGLES/EAGL.h>
#import <UIKit/UIScreen.h>
#import <UIKit/UIWindow.h>

/*************************************************************************/
/****************** Shared data (internal to iOS code) *******************/
/*************************************************************************/

uint8_t ios_application_is_terminating;
uint8_t ios_application_is_suspending;
SysSemaphoreID ios_suspend_semaphore;
SysSemaphoreID ios_resume_semaphore;

/*************************************************************************/
/******************** Local data (local to this file) ********************/
/*************************************************************************/

/* Frame interval for vsync handling. */
static uint8_t ios_frame_interval = 1;

/* Array of functions to be called on vertical sync, and associated mutex. */
static struct {
    iOSVSyncFunction *function;
    void *userdata;
} vsync_functions[10];
static pthread_mutex_t vsync_functions_mutex = PTHREAD_MUTEX_INITIALIZER;

/*************************************************************************/
/*********************** Exported utility routines ***********************/
/*************************************************************************/

iOSDevice ios_get_device(void)
{
    return ios_get_model() >> 8;  // See MAKE_MODEL() in export.h.
}

/*-----------------------------------------------------------------------*/

iOSModel ios_get_model(void)
{
    size_t size;
    sysctlbyname("hw.machine", NULL, &size, NULL, 0);
    char *machine = malloc(size);
    if (UNLIKELY(!machine)) {
        DLOG("No memory for machine name");
        return IOS_MODEL_UNKNOWN;
    }
    sysctlbyname("hw.machine", machine, &size, NULL, 0);

    const iOSModel model = ios_get_model_for(machine);

    free(machine);
    return model;
}

/*-----------------------------------------------------------------------*/

iOSModel ios_get_model_for(const char *machine)
{
    if (strcmp(machine, "iPhone1,2") == 0) {
        return IOS_MODEL_IPHONE_3G;
    } else if (strcmp(machine, "iPhone1,2*") == 0) {
        return IOS_MODEL_IPHONE_3G;
    } else if (strncmp(machine, "iPhone1,", 8) == 0) {
        return IOS_MODEL_IPHONE_1;
    } else if (strncmp(machine, "iPhone2,", 8) == 0) {
        return IOS_MODEL_IPHONE_3GS;
    } else if (strncmp(machine, "iPhone3,", 8) == 0) {
        return IOS_MODEL_IPHONE_4;
    } else if (strncmp(machine, "iPhone4,", 8) == 0) {
        return IOS_MODEL_IPHONE_4S;
    } else if (strncmp(machine, "iPhone5,", 8) == 0) {
        if ((machine[8] >= '1' && machine[8] <= '2') && machine[9] == 0) {
            return IOS_MODEL_IPHONE_5;
        } else {
            return IOS_MODEL_IPHONE_5C;
        }
    } else if (strncmp(machine, "iPhone6,", 8) == 0) {
        return IOS_MODEL_IPHONE_5S;
    } else if (strncmp(machine, "iPhone7,", 8) == 0) {
        if (machine[8] == '1' && machine[9] == 0) {
            return IOS_MODEL_IPHONE_6_PLUS;
        } else {
            return IOS_MODEL_IPHONE_6;
        }
    } else if (strncmp(machine, "iPhone8,", 8) == 0) {
        /* Yup, Apple reversed the minor numbers from the last generation. */
        if (machine[8] == '2' && machine[9] == 0) {
            return IOS_MODEL_IPHONE_6S_PLUS;
        } else if (machine[8] == '4' && machine[9] == 0) {
            return IOS_MODEL_IPHONE_SE;
        } else {
            return IOS_MODEL_IPHONE_6S;
        }
    } else if (strncmp(machine, "iPhone9,", 8) == 0) {
        if ((machine[8] == '2' || machine[8] == '4') && machine[9] == 0) {
            return IOS_MODEL_IPHONE_7_PLUS;
        } else {
            return IOS_MODEL_IPHONE_7;
        }
    } else if (strncmp(machine, "iPhone10,", 9) == 0) {
        if ((machine[9] == '2' || machine[9] == '5') && machine[10] == 0) {
            return IOS_MODEL_IPHONE_8_PLUS;
        } else if ((machine[9] == '3' || machine[9] == '6') && machine[10] == 0) {
            return IOS_MODEL_IPHONE_X;
        } else {
            return IOS_MODEL_IPHONE_8;
        }
    } else if (strncmp(machine, "iPhone11,", 9) == 0) {
        if (machine[9] == '6' && machine[10] == 0) {
            return IOS_MODEL_IPHONE_XS_MAX;
        } else if (machine[9] == '8' && machine[10] == 0) {
            return IOS_MODEL_IPHONE_XR;
        } else {
            return IOS_MODEL_IPHONE_XS;
        }
    } else if (strncmp(machine, "iPhone12,", 9) == 0) {
        if (machine[9] == '3' && machine[10] == 0) {
            return IOS_MODEL_IPHONE_11_PRO;
        } else if (machine[9] == '5' && machine[10] == 0) {
            return IOS_MODEL_IPHONE_11_MAX;
        } else {
            return IOS_MODEL_IPHONE_11;
        }
    } else if (strncmp(machine, "iPhone", 6) == 0) {
        if (atoi(machine+6) > 12) {
            DLOG("Unknown iPhone model %s with version %d > 12, returning"
                 " IPHONE_11", machine, atoi(machine+6));
            return IOS_MODEL_IPHONE_11;
        } else {
            DLOG("Unknown iPhone model %s with unknown version, returning"
                 " IPHONE_1", machine);
            return IOS_MODEL_IPHONE_1;
        }

    } else if (strncmp(machine, "iPod1,", 6) == 0) {
        return IOS_MODEL_IPOD_1;
    } else if (strncmp(machine, "iPod2,", 6) == 0) {
        return IOS_MODEL_IPOD_2;
    } else if (strncmp(machine, "iPod3,", 6) == 0) {
        return IOS_MODEL_IPOD_3;
    } else if (strncmp(machine, "iPod4,", 6) == 0) {
        return IOS_MODEL_IPOD_4;
    } else if (strncmp(machine, "iPod5,", 6) == 0) {
        return IOS_MODEL_IPOD_5;
    } else if (strncmp(machine, "iPod7,", 6) == 0) {
        return IOS_MODEL_IPOD_6;
    } else if (strncmp(machine, "iPod9,", 6) == 0) {
        return IOS_MODEL_IPOD_7;
    } else if (strncmp(machine, "iPod", 4) == 0) {
        if (atoi(machine+4) > 9) {
            DLOG("Unknown iPod model %s with version %d > 9, returning"
                 " IPOD_7", machine, atoi(machine+4));
            return IOS_MODEL_IPOD_7;
        } else {
            DLOG("Unknown iPod model %s with unknown version, returning"
                 " IPOD_1G", machine);
            return IOS_MODEL_IPOD_1;
        }

    } else if (strncmp(machine, "iPad1,", 6) == 0) {
        return IOS_MODEL_IPAD_1;
    } else if (strncmp(machine, "iPad2,", 6) == 0) {
        if ((machine[6] >= '1' && machine[6] <= '4') && machine[7] == 0) {
            return IOS_MODEL_IPAD_2;
        } else {
            return IOS_MODEL_IPAD_MINI_1;
        }
    } else if (strncmp(machine, "iPad3,", 6) == 0) {
        if ((machine[6] >= '1' && machine[6] <= '3') && machine[7] == 0) {
            return IOS_MODEL_IPAD_3;
        } else {
            return IOS_MODEL_IPAD_4;
        }
    } else if (strncmp(machine, "iPad4,", 6) == 0) {
        if ((machine[6] >= '1' && machine[6] <= '3') && machine[7] == 0) {
            return IOS_MODEL_IPAD_AIR;
        } else if ((machine[6] >= '4' && machine[6] <= '6') && machine[7] == 0) {
            return IOS_MODEL_IPAD_MINI_2;
        } else {
            return IOS_MODEL_IPAD_MINI_3;
        }
    } else if (strncmp(machine, "iPad5,", 6) == 0) {
        if ((machine[6] >= '1' && machine[6] <= '2') && machine[7] == 0) {
            return IOS_MODEL_IPAD_MINI_4;
        } else {
            return IOS_MODEL_IPAD_AIR_2;
        }
    } else if (strncmp(machine, "iPad6,", 6) == 0) {
        if (machine[6] <= '4' && machine[7] == 0) {
            return IOS_MODEL_IPAD_PRO_9IN;
        } else if (machine[6] == '1'
                   && (machine[7] == '1' || machine[7] == '2')
                   && machine[8] == 0) {
            return IOS_MODEL_IPAD_5;
        } else {
            return IOS_MODEL_IPAD_PRO;
        }
    } else if (strncmp(machine, "iPad7,", 6) == 0) {
        if ((machine[6] == '3' || machine[6] == '4') && machine[7] == 0) {
            return IOS_MODEL_IPAD_PRO_2G_10IN;
        } else if ((machine[6] == '5' || machine[6] == '6')
                   && machine[7] == 0) {
            return IOS_MODEL_IPAD_6;
        } else if (machine[6] == '1'
                   && (machine[7] == '1' || machine[7] == '2')
                   && machine[8] == 0) {
            return IOS_MODEL_IPAD_7;
        } else {
            return IOS_MODEL_IPAD_PRO_2G;
        }
    } else if (strncmp(machine, "iPad8,", 6) == 0) {
        if ((machine[6] >= '5' && machine[6] <= '8') && machine[7] == 0) {
            return IOS_MODEL_IPAD_PRO_3G;
        } else {
            return IOS_MODEL_IPAD_PRO_3G_11IN;
        }
    } else if (strncmp(machine, "iPad11,", 7) == 0) {
        if ((machine[7] == '3' || machine[7] == '4') && machine[8] == 0) {
            return IOS_MODEL_IPAD_AIR_3;
        } else {
            return IOS_MODEL_IPAD_MINI_5;
        }
    } else if (strncmp(machine, "iPad", 4) == 0) {
        if (atoi(machine+4) > 11) {
            DLOG("Unknown iPad model %s with version %d > 11, returning"
                 " IPAD_AIR_3", machine, atoi(machine+4));
            return IOS_MODEL_IPAD_AIR_3;
        } else {
            DLOG("Unknown iPod model %s with unknown version, returning"
                 " IPAD_1", machine);
            return IOS_MODEL_IPAD_1;
        }

    } else if (strncmp(machine, "AppleTV5,", 9) == 0) {
        return IOS_MODEL_APPLE_TV_4;
    } else if (strncmp(machine, "AppleTV6,", 9) == 0) {
        return IOS_MODEL_APPLE_TV_4K;
    } else if (strncmp(machine, "AppleTV", 7) == 0) {
        if (atoi(machine+7) > 6) {
            DLOG("Unknown Apple TV model %s with version %d > 6, returning"
                 " APPLE_TV_4K", machine, atoi(machine+7));
            return IOS_MODEL_APPLE_TV_4K;
        } else {
            DLOG("Unknown Apple TV model %s with unknown version, returning"
                 " APPLE_TV_4", machine);
            return IOS_MODEL_APPLE_TV_4;
        }

    } else {
        DLOG("Unknown model %s", machine);
        return IOS_MODEL_UNKNOWN;
    }
}

/*-----------------------------------------------------------------------*/

float ios_get_display_size(void)
{
    switch (ios_get_model()) {
      case IOS_MODEL_IPHONE_1:
      case IOS_MODEL_IPHONE_3G:
      case IOS_MODEL_IPHONE_3GS:
      case IOS_MODEL_IPHONE_4:
      case IOS_MODEL_IPHONE_4S:
      case IOS_MODEL_IPOD_1:
      case IOS_MODEL_IPOD_2:
      case IOS_MODEL_IPOD_3:
      case IOS_MODEL_IPOD_4:
        return 3.5;
      case IOS_MODEL_IPHONE_5:
      case IOS_MODEL_IPHONE_5C:
      case IOS_MODEL_IPHONE_5S:
      case IOS_MODEL_IPHONE_SE:
      case IOS_MODEL_IPOD_5:
      case IOS_MODEL_IPOD_6:
      case IOS_MODEL_IPOD_7:
        return 4;
      case IOS_MODEL_IPHONE_6:
      case IOS_MODEL_IPHONE_6S:
      case IOS_MODEL_IPHONE_7:
      case IOS_MODEL_IPHONE_8:
        return 4.7;
      case IOS_MODEL_IPHONE_6_PLUS:
      case IOS_MODEL_IPHONE_6S_PLUS:
      case IOS_MODEL_IPHONE_7_PLUS:
      case IOS_MODEL_IPHONE_8_PLUS:
        return 5.5;
      case IOS_MODEL_IPHONE_X:
      case IOS_MODEL_IPHONE_XS:
      case IOS_MODEL_IPHONE_11_PRO:
        return 5.8;
      case IOS_MODEL_IPHONE_XR:
      case IOS_MODEL_IPHONE_11:
        return 6.1;
      case IOS_MODEL_IPHONE_XS_MAX:
      case IOS_MODEL_IPHONE_11_MAX:
        return 6.5;
      case IOS_MODEL_IPAD_MINI_1:
      case IOS_MODEL_IPAD_MINI_2:
      case IOS_MODEL_IPAD_MINI_3:
      case IOS_MODEL_IPAD_MINI_4:
      case IOS_MODEL_IPAD_MINI_5:
        return 7.9;
      case IOS_MODEL_IPAD_1:
      case IOS_MODEL_IPAD_2:
      case IOS_MODEL_IPAD_3:
      case IOS_MODEL_IPAD_4:
      case IOS_MODEL_IPAD_5:
      case IOS_MODEL_IPAD_6:
      case IOS_MODEL_IPAD_AIR:
      case IOS_MODEL_IPAD_AIR_2:
      case IOS_MODEL_IPAD_PRO_9IN:
        return 9.7;
      case IOS_MODEL_IPAD_7:
        return 10.2;
      case IOS_MODEL_IPAD_PRO_2G_10IN:
      case IOS_MODEL_IPAD_AIR_3:
        return 10.5;
      case IOS_MODEL_IPAD_PRO_3G_11IN:
        return 11;
      case IOS_MODEL_IPAD_PRO:
      case IOS_MODEL_IPAD_PRO_2G:
      case IOS_MODEL_IPAD_PRO_3G:
        return 12.9;
      case IOS_MODEL_APPLE_TV_4:
      case IOS_MODEL_APPLE_TV_4K:
      case IOS_MODEL_UNKNOWN:
        return 0;
    }
    ASSERT(!"unreachable", return 0);
}

/*-----------------------------------------------------------------------*/

int ios_get_native_refresh_rate(void)
{
    if (ios_version_is_at_least("10.3")) {
        int refresh_rate =
#ifdef __IPHONE_10_3
            [UIScreen mainScreen].maximumFramesPerSecond
#else
            (int) [[UIScreen mainScreen] performSelector:@selector(maximumFramesPerSecond)]
#endif
            ;
        ASSERT(refresh_rate > 0, refresh_rate = 60);
        return refresh_rate;
    } else {
        return 60;
    }
}

/*-----------------------------------------------------------------------*/

const char *ios_version_string(void)
{
    static char *saved_version;

    /* If we already looked it up, return the looked-up value. */
    if (saved_version) {
        return saved_version;
    }

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    const char *version =
        [[[UIDevice currentDevice] systemVersion] UTF8String];
    if (version) {
        saved_version = strdup(version);
    }
    [pool release];

    return saved_version ? saved_version : "0.0";
}

/*-----------------------------------------------------------------------*/

int ios_version_is_at_least(const char *version)
{
    if (!version) {
        DLOG("version == NULL");
        return 1;
    }
    if (!*version) {
        DLOG("version is empty");
        return 1;
    }
    return ios_compare_versions(ios_version_string(), version) >= 0;
}

/*-----------------------------------------------------------------------*/

/* Vertical sync helper for ios_toggle_status_bar() that does the actual
 * iOS call. */
static void do_status_bar(void *userdata)
{
    const BOOL hidden = (BOOL)(intptr_t)userdata;
    [[UIApplication sharedApplication] setStatusBarHidden:hidden
                                       withAnimation:UIStatusBarAnimationFade];
}

void ios_toggle_status_bar(int state)
{
    ios_register_vsync_function(do_status_bar,
                                (void *)(intptr_t)(state ? NO : YES));
}

/*-----------------------------------------------------------------------*/

int ios_status_bar_visible_height(void)
{
    UIApplication *application = [UIApplication sharedApplication];

    /* The size is presumably integral, but we round it just in case.
     * (The statusBarFrame docs say that the frame size is zero when the
     * status bar is hidden, so we don't need to check separately.) */
#ifdef __IPHONE_8_0
    if (ios_version_is_at_least("8.0")) {
        return iroundf(application.statusBarFrame.size.height
                       * [UIScreen mainScreen].nativeScale);
    }
#endif
    /* For iOS <8.0 or when building with SDK <8.0, statusBarFrame is
     * always returned with respect to portrait orientation, so we look
     * at the width rather than the height. */
    return iroundf(application.statusBarFrame.size.width
                   * [UIScreen mainScreen].scale);
}

/*************************************************************************/
/******* Internal utility routines: Device/application parameters ********/
/*************************************************************************/

int ios_display_width(void)
{
    return (int)([global_view bounds].size.width
                 * global_view.contentScaleFactor);
}

/*-----------------------------------------------------------------------*/

int ios_display_height(void)
{
    return (int)([global_view bounds].size.height
                 * global_view.contentScaleFactor);
}

/*-----------------------------------------------------------------------*/

extern CONST_FUNCTION float ios_display_scale(void)
{
#ifdef __IPHONE_8_0
    if (ios_version_is_at_least("8.0")) {
        return [UIScreen mainScreen].nativeScale;
    }
#endif
    return [UIScreen mainScreen].scale;
}

/*-----------------------------------------------------------------------*/

const char *ios_get_application_name(void)
{
    static char *saved_name;

    /* If we already looked it up, return the looked-up value. */
    if (saved_name) {
        return saved_name;
    }

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    const NSDictionary *dict =
        (const NSDictionary *)CFBundleGetInfoDictionary(CFBundleGetMainBundle());
    if (dict) {
        const char *name = [[dict objectForKey: @"CFBundleName"] UTF8String];
        if (name) {
            saved_name = strdup(name);
        }
    }

    [pool release];

    return saved_name ? saved_name : "The application";
}

/*-----------------------------------------------------------------------*/

const char *ios_get_application_support_path(void)
{
    static char *saved_path = NULL;

    /* If we already looked it up, return the looked-up value. */
    if (saved_path) {
        return saved_path;
    }

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    /* Get the directory path from the system. */
    NSArray *paths = NSSearchPathForDirectoriesInDomains(
        NSApplicationSupportDirectory, NSUserDomainMask, YES);
    const char *path = [[paths objectAtIndex:0] UTF8String];
    if (UNLIKELY(!path)) {  // Should never happen.
        /* If we default to a relative path, we may try to write within
         * the *.app directory, which would (according to Apple docs)
         * break the code signature and prevent iOS from starting the
         * program again.  This should never happen anyway, but let's
         * try and minimize user frustration if it does. */
        DLOG("WARNING: NSSearchPathForDirectoriesInDomains() failed,"
             " falling back to /tmp");
        path = "/tmp";
    }

    /* Save a copy of the path. */
    saved_path = strdup(path);
    if (UNLIKELY(!saved_path)) {
        DLOG("WARNING: Out of memory copying pathname: %s", path);
        /* We never free it, so this is safe. */
        saved_path = (char *)"/tmp";
    }

    [pool release];

    /* Return the saved path. */
    return saved_path;
}

/*-----------------------------------------------------------------------*/

const char *ios_get_documents_path(void)
{
    static char *saved_path = NULL;

    /* If we already looked it up, return the looked-up value. */
    if (saved_path) {
        return saved_path;
    }

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    /* Get the directory path from the system. */
    NSArray *paths = NSSearchPathForDirectoriesInDomains(
        NSDocumentDirectory, NSUserDomainMask, YES);
    const char *path = [[paths objectAtIndex:0] UTF8String];
    if (UNLIKELY(!path)) {  // Should never happen.
        DLOG("WARNING: NSSearchPathForDirectoriesInDomains() failed,"
             " falling back to /tmp");
        path = "/tmp";
    }

    /* Save a copy of the path. */
    saved_path = strdup(path);
    if (UNLIKELY(!saved_path)) {
        DLOG("WARNING: Out of memory copying pathname: %s", path);
        /* We never free it, so this is safe. */
        saved_path = (char *)"/tmp";
    }

    [pool release];

    /* Return the saved path. */
    return saved_path;
}

/*************************************************************************/
/******** Internal utility routines: Frame presentation/counting *********/
/*************************************************************************/

void ios_set_refresh_rate(int rate)
{
    [global_view setRefreshRate:rate];
}

/*-----------------------------------------------------------------------*/

void ios_present_view(void)
{
    [global_view present];
}

/*-----------------------------------------------------------------------*/

void ios_vsync(void)
{
    [global_view vsync];
}

/*-----------------------------------------------------------------------*/

extern int ios_get_frame_counter(void)
{
    return [global_view getFrameCounter];
}

/*-----------------------------------------------------------------------*/

int ios_get_frame_interval(void)
{
    return ios_frame_interval;
}

/*-----------------------------------------------------------------------*/

void ios_set_frame_interval(int interval)
{
    ios_frame_interval = interval;
}

/*************************************************************************/
/********* Internal utility routines: V-sync function management *********/
/*************************************************************************/

void ios_register_vsync_function(iOSVSyncFunction *function, void *userdata)
{
    pthread_mutex_lock(&vsync_functions_mutex);
    for (int i = 0; i < lenof(vsync_functions); i++) {
        if (vsync_functions[i].function == NULL) {
            vsync_functions[i].function = function;
            vsync_functions[i].userdata = userdata;
            pthread_mutex_unlock(&vsync_functions_mutex);
            return;
        }
    }
    pthread_mutex_unlock(&vsync_functions_mutex);
    DLOG("No room for vsync function %p (userdata %p), dropping",
         function, userdata);
}

/*-----------------------------------------------------------------------*/

void ios_call_vsync_functions(void)
{
    __typeof__(vsync_functions) vsync_functions_copy;

    pthread_mutex_lock(&vsync_functions_mutex);
    for (int i = 0; i < lenof(vsync_functions); i++) {
        vsync_functions_copy[i] = vsync_functions[i];
        vsync_functions[i].function = NULL;
    }
    pthread_mutex_unlock(&vsync_functions_mutex);

    for (int i = 0; i < lenof(vsync_functions_copy); i++) {
        iOSVSyncFunction * const function = vsync_functions_copy[i].function;
        void * const userdata = vsync_functions_copy[i].userdata;
        if (function) {
            (*function)(userdata);
        }
    }
}

/*************************************************************************/
/********** Internal utility routines: Miscellaneous functions ***********/
/*************************************************************************/

int ios_compare_versions(const char *version1, const char *version2)
{
    PRECOND(version1 != NULL, return 0);
    PRECOND(version2 != NULL, return 0);
    PRECOND(*version1 != '\0', return 0);
    PRECOND(*version2 != '\0', return 0);

    /* Compare the version numbers, component by component. */
    while (*version1 && *version2) {
        /* Extract the next component from each version number. */
        const char *component1_str = version1;
        const char *component2_str = version2;
        version1 += strcspn(version1, ".");
        version2 += strcspn(version2, ".");

        /* Compare the numeric portion of each string. */
        const int component1 = strtol(
            component1_str, (char **)&component1_str, 10);
        const int component2 = strtol(
            component2_str, (char **)&component2_str, 10);
        if (component1 < component2) {
            return -1;
        } else if (component1 > component2) {
            return +1;
        }

        /* Check for version suffixes.  For iOS, suffixes always indicate
         * prerelease versions, so a plain numeric component is greater
         * than a component with a suffix.  Within suffixes:
         *    - The initial non-numeric portion is compared lexically, thus
         *      1.2a9 < 1.2b1;
         *    - the following numeric portion (0 if there is no numeric
         *      portion) is compared numerically, thus 1.2b2 < 1.2b10 and
         *      1.2b < 1.2b1;
         *    - any characters remaining after the numeric portion are
         *      compared lexically (thus 1.2b3a45 < 1.2b3a5), though iOS
         *      version numbers do not normally contain such components. */
        if (component1_str < version1 && component2_str == version2) {
            return -1;
        } else if (component1_str == version1 && component2_str < version2) {
            return +1;
        }
        while (component1_str < version1 && component2_str < version2) {
            if (*component1_str != *component2_str) {
                break;
            }
            const int is_digit1 =
                (*component1_str >= '0' && *component1_str < '9');
            const int is_digit2 =
                (*component2_str >= '0' && *component2_str < '9');
            if (is_digit1 || is_digit2) {
                if (!is_digit1) {
                    return -1;
                } else if (!is_digit2) {
                    return +1;
                } else {
                    break;  // Compare numeric portions of suffixes.
                }
            }
            component1_str++;
            component2_str++;
        }
        const int suffix1 = strtol(
            component1_str, (char **)&component1_str, 10);
        const int suffix2 = strtol(
            component2_str, (char **)&component2_str, 10);
        if (suffix1 < suffix2) {
            return -1;
        } else if (suffix1 > suffix2) {
            return +1;
        }
        while (component1_str < version1 && component2_str < version2) {
            if (*component1_str < *component2_str) {
                return -1;
            } else if (*component1_str > *component2_str) {
                return +1;
            }
            component1_str++;
            component2_str++;
        }
        if (component1_str == version1 && component2_str < version2) {
            return -1;
        } else if (component1_str < version1 && component2_str == version2) {
            return +1;
        }

        /* Advance to the next component, if any. */
        if (*version1) {
            version1++;
        }
        if (*version2) {
            version2++;
        }
    }

    /* If one of the version numbers has a component left and the other
     * doesn't, the one with the leftover component is greater (e.g.,
     * 1.2.3 > 1.2). */
    return (*version1 != '\0') - (*version2 != '\0');
}

/*-----------------------------------------------------------------------*/

/* Vertical sync helper for ios_enable_idle_timer(). */
static void do_enable_idle_timer(void *userdata)
{
    const int enable = (int)(intptr_t)userdata;

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    [UIApplication sharedApplication].idleTimerDisabled = !enable;
    [pool release];
}

void ios_enable_idle_timer(int enable)
{
    ios_register_vsync_function(do_enable_idle_timer,
                                (void *)(intptr_t)(enable != 0));
}

/*-----------------------------------------------------------------------*/

void ios_open_url(const char *url)
{
    PRECOND(url != NULL, return);

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSString *nsurl = [NSString stringWithUTF8String:url];
    NSURL *urlobject = [NSURL URLWithString:nsurl];
    [[UIApplication sharedApplication]
        performSelectorOnMainThread:@selector(openURL:) withObject:urlobject
        waitUntilDone:YES];
    [pool release];
}

/*-----------------------------------------------------------------------*/

void ios_show_dialog_formatted(const char *title_id,
                               const char *format_id, ...)
{
    PRECOND(title_id != NULL, return);
    PRECOND(format_id != NULL, return);

    /* Slightly roundabout since we can't use CopyStringResource() with
     * variables. */
    CFStringRef title_id_cfstr = CFStringCreateWithCStringNoCopy(
        NULL, title_id, kCFStringEncodingUTF8, kCFAllocatorNull);
    if (UNLIKELY(!title_id_cfstr)) {
        DLOG("Failed to copy title ID!");
        return;
    }
    CFStringRef format_id_cfstr = CFStringCreateWithCStringNoCopy(
        NULL, format_id, kCFStringEncodingUTF8, kCFAllocatorNull);
    if (UNLIKELY(!format_id_cfstr)) {
        DLOG("Failed to copy text ID!");
        CFRelease(title_id_cfstr);
        return;
    }
    CFStringRef title =
        CFBundleCopyLocalizedString(CFBundleGetMainBundle(), title_id_cfstr,
                                    title_id_cfstr, CFSTR("SIL"));
    CFStringRef format =
        CFBundleCopyLocalizedString(CFBundleGetMainBundle(), format_id_cfstr,
                                    format_id_cfstr, CFSTR("SIL"));
    CFRelease(title_id_cfstr);
    CFRelease(format_id_cfstr);

    if (!title) {
        DLOG("String resource \"%s\" not found", title_id);
        title = CFStringCreateWithCStringNoCopy(
            NULL, title_id, kCFStringEncodingUTF8, kCFAllocatorNull);
        if (UNLIKELY(!title_id)) {
            DLOG("Out of memory!");
            CFRelease(format);
            return;
        }
    }
    if (!format) {
        DLOG("String resource \"%s\" not found", format_id);
        format = CFStringCreateWithCStringNoCopy(
            NULL, format_id, kCFStringEncodingUTF8, kCFAllocatorNull);
        if (UNLIKELY(!format_id)) {
            DLOG("Out of memory!");
            CFRelease(title);
            return;
        }
    }

    CFStringRef text;
    char format_buf[10000];
    char text_buf[10000];
    if (CFStringGetCString(format, format_buf, sizeof(format_buf),
                           kCFStringEncodingUTF8)) {
        va_list args;
        va_start(args, format_id);
        vstrformat(text_buf, sizeof(text_buf), format_buf, args);
        va_end(args);
        text = CFStringCreateWithCStringNoCopy(
            NULL, text_buf, kCFStringEncodingUTF8, kCFAllocatorNull);
        if (text) {
            CFRelease(format);
        } else {
            DLOG("Failed to create CFString for formatted text!");
            text = format;  // Try to at least display _something_.
        }
    } else {
        DLOG("Failed to convert format string to UTF-8!");
        text = format;
    }

    ios_dialog(title, text);
    CFRelease(text);
    CFRelease(title);
}

/*************************************************************************/
/*************************************************************************/
