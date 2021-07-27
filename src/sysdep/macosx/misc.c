/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/macosx/misc.c: Miscellaneous interface functions for Mac OS X.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/sysdep.h"
#include "src/sysdep/macosx/dialog.h"
#include "src/sysdep/macosx/main.h"
#include "src/sysdep/macosx/osx-headers.h"
#include "src/sysdep/macosx/strings.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/pwr_mgt/IOPMLib.h>

/*************************************************************************/
/*************************************************************************/

#ifdef SIL_UTILITY_NOISY_ERRORS
# define USED_IF_NOISY  /*nothing*/
#else
# define USED_IF_NOISY  UNUSED
#endif

void sys_display_error(USED_IF_NOISY const char *message,
                       USED_IF_NOISY va_list args)
{
#ifdef SIL_UTILITY_NOISY_ERRORS
    char buf[1000];
    vstrformat(buf, sizeof(buf), message, args);
    CFStringRef title = CopyStringResource("MACOSX_ERROR_TITLE");
    CFStringRef text = CFStringCreateWithCStringNoCopy(
        NULL, buf, kCFStringEncodingUTF8, kCFAllocatorNull);
    macosx_dialog(title, text);
    CFRelease(text);
    CFRelease(title);
#endif
}

#undef USED_IF_NOISY

/*-----------------------------------------------------------------------*/

int sys_get_language(int index, char *language_ret, char *dialect_ret)
{
    CFArrayRef language_array = CFLocaleCopyPreferredLanguages();
    if (UNLIKELY(!language_array)) {
        return 0;
    }
    if (index >= (int)CFArrayGetCount(language_array)) {
        CFRelease(language_array);
        return 0;
    }
    CFStringRef language = CFArrayGetValueAtIndex(language_array, index);
    if (UNLIKELY(!language)) {
        CFRelease(language_array);
        return 0;
    }
    const int length = CFStringGetLength(language);
    if (UNLIKELY(length < 2)) {
        DLOG("Language %d string too short", index);
        CFRelease(language_array);
        return 0;
    }
    UniChar chars[6];
    mem_clear(chars, sizeof(chars));
    CFRange range = {.location = 0, .length = ubound(length, lenof(chars))};
    CFStringGetCharacters(language, range, chars);
    CFRelease(language_array);
    if ((chars[0] >= 'a' && chars[0] <= 'z')
     && (chars[1] >= 'a' && chars[1] <= 'z')
     && (chars[2] == 0 || chars[2] == '-')) {
        language_ret[0] = chars[0];
        language_ret[1] = chars[1];
        language_ret[2] = 0;
        if ((chars[3] >= 'A' && chars[3] <= 'Z')
         && (chars[4] >= 'A' && chars[4] <= 'Z')
         && (chars[5] == 0)) {
            dialect_ret[0] = chars[3];
            dialect_ret[1] = chars[4];
            dialect_ret[2] = 0;
        } else {
            if (chars[3] != 0) {
                char buf[4];
                buf[0] = chars[3];
                buf[1] = chars[4];
                buf[2] = chars[5];
                buf[3] = 0;
                DLOG("Invalid dialect code: %s", buf);
            }
            *dialect_ret = 0;
        }
    } else {
        if (chars[0] != 0) {
            char buf[7];
            buf[0] = chars[0];
            buf[1] = chars[1];
            buf[2] = chars[2];
            buf[3] = chars[3];
            buf[4] = chars[4];
            buf[5] = chars[5];
            buf[6] = 0;
            DLOG("Invalid language code: %s", buf);
        }
        strcpy(language_ret, "??");  // Safe by contract.
        *dialect_ret = 0;
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

int sys_get_resource_path_prefix(char *prefix_buf, int bufsize)
{
    const char *path = macosx_resource_dir();
#ifdef SIL_DATA_PATH_ENV_VAR
    const char *env_path = getenv(SIL_DATA_PATH_ENV_VAR);
    if (env_path && *env_path) {
        path = env_path;
    }
#endif
    return strformat(prefix_buf, bufsize, "%s/", path);
}

/*-----------------------------------------------------------------------*/

int sys_open_file(const char *path)
{
    if (!path) {
        return 1;
    }

    CFStringRef cfstr = CFStringCreateWithCString(
        NULL, path, kCFStringEncodingUTF8);
    CFURLRef cfurl = CFURLCreateWithFileSystemPath(
        NULL, cfstr, kCFURLPOSIXPathStyle, 0);
    LSOpenCFURLRef(cfurl, NULL);
    CFRelease(cfurl);
    CFRelease(cfstr);
    return 1;
}

/*-----------------------------------------------------------------------*/

int sys_open_url(const char *url)
{
    if (!url) {
        return 1;
    }

    CFStringRef cfstr = CFStringCreateWithCString(
        NULL, url, kCFStringEncodingUTF8);
    CFURLRef cfurl = CFURLCreateWithString(NULL, cfstr, NULL);
    LSOpenCFURLRef(cfurl, NULL);
    CFRelease(cfurl);
    CFRelease(cfstr);
    return 1;
}

/*-----------------------------------------------------------------------*/

void sys_reset_idle_timer(void)
{
    static IOPMAssertionID assertion = 0;
    IOReturn result = IOPMAssertionDeclareUserActivity(
        CFSTR("Application running"), kIOPMUserActiveLocal, &assertion);
    if (result != kIOReturnSuccess) {
        DLOG("IOPMAssertionDeclareUserActivity failed: %d", result);
    }
}

/*-----------------------------------------------------------------------*/

int sys_set_performance_level(int level)
{
    return level == 0;  // Alternate performance levels not supported.
}

/*************************************************************************/
/*************************************************************************/
