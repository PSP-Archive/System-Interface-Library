/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/ios/misc.c: Miscellaneous interface functions for iOS.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/sysdep.h"
#include "src/sysdep/ios/dialog.h"
#include "src/sysdep/ios/main.h"
#include "src/sysdep/ios/util.h"
#include "src/sysdep/macosx/strings.h"
#include "src/thread.h"
#include "src/time.h"

#include <CoreFoundation/CoreFoundation.h>

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Time for which to keep the idle timer disabled after a call to
 * sys_reset_idle_timer(), in seconds.  If the system idle timeout has
 * expired, the system will suspend about 15 seconds after the idle timer
 * is re-enabled. */
#define IDLE_TIMER_RESTART_DELAY  60

/* Thread ID for the idle timer thread. */
static int idle_timer_thread_id;

/* Semaphore used to trigger an idle timer reset. */
static SysSemaphoreID idle_reset_trigger;

/* Shared flag used to signal the idle timer thread to stop. */
static uint8_t idle_timer_thread_stop;

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * idle_timer_thread:  Thread which toggles the system idle timer on and
 * off based on signals from sys_reset_idle_timer().
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
    CFStringRef title = CopyStringResource("IOS_ERROR_TITLE");
    CFStringRef text = CFStringCreateWithCStringNoCopy(
        NULL, buf, kCFStringEncodingUTF8, kCFAllocatorNull);
    ios_dialog(title, text);
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
    return strformat(prefix_buf, bufsize, "%s/", ios_resource_dir());
}

/*-----------------------------------------------------------------------*/

int sys_open_file(UNUSED const char *path)
{
    return 0;  // Not supported on iOS.
}

/*-----------------------------------------------------------------------*/

int sys_open_url(const char *url)
{
    if (!url) {
        return 1;
    }

    ios_open_url(url);
    return 1;
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
/*********************** Library-internal routines ***********************/
/*************************************************************************/

void ios_stop_idle_timer_thread(void)
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
    while (!idle_timer_thread_stop) {
        sys_semaphore_wait(idle_reset_trigger, -1);

        DLOG("Disabling idle timer");
        ios_enable_idle_timer(0);

        while (sys_semaphore_wait(
                   idle_reset_trigger, IDLE_TIMER_RESTART_DELAY)) { /*spin*/ }

        DLOG("Enabling idle timer");
        ios_enable_idle_timer(1);
    }

    return 0;
}

/*************************************************************************/
/*************************************************************************/
