/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/macosx/util.m: Miscellaneous utility functions specific to
 * Mac OS X.
 */

/*
 * Note that we use the standard library's memory management functions
 * (malloc() and friends) rather than mem_*() in macosx_get_application_name()
 * below, since it may be called before the memory management subsystem has
 * been initialized.
 */
#undef SIL_MEMORY_FORBID_MALLOC

#include "src/base.h"
#include "src/sysdep/macosx/dialog.h"
#include "src/sysdep/macosx/util.h"

#include "src/sysdep/macosx/osx-headers.h"
#import <AppKit/AppKit.h>

/*************************************************************************/
/*********************** Exported utility routines ***********************/
/*************************************************************************/

/*
 * Gestalt() was deprecated in OS X 10.8, but according to one Apple
 * engineer (see https://twitter.com/Catfish_Man/status/373277120408997889),
 * the recommended way as of August 2013 to retrieve the OSX version is to
 * call Gestalt() and use a #pragma to suppress the deprecation warning:
 *
 * <Catfish_Man> just use gestalt. It's fine for that key
 * <Catfish_Man> use #pragma to turn that [compiler warning] off for that
 * one spot. I'm completely serious.
 *
 * Bug 15543376 has been filed with Apple to request a non-deprecated
 * replacement for this usage of Gestalt().
 */

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

int macosx_version_major(void)
{
    static SInt32 version = -1;
    if (version == -1) {
        OSErr error = Gestalt(gestaltSystemVersionMajor, &version);
        if (error != 0) {
            DLOG("Failed to get version: %d", error);
            return 0;
        }
        ASSERT(version != -1, version = 0);
    }
    return version;
}

int macosx_version_minor(void)
{
    static SInt32 version = -1;
    if (version == -1) {
        OSErr error = Gestalt(gestaltSystemVersionMinor, &version);
        if (error != 0) {
            DLOG("Failed to get version: %d", error);
            return 0;
        }
        ASSERT(version != -1, version = 0);
    }
    return version;
}

int macosx_version_bugfix(void)
{
    static SInt32 version = -1;
    if (version == -1) {
        OSErr error = Gestalt(gestaltSystemVersionBugFix, &version);
        if (error != 0) {
            DLOG("Failed to get version: %d", error);
            return 0;
        }
        ASSERT(version != -1, version = 0);
    }
    return version;
}

#pragma clang diagnostic pop

/*-----------------------------------------------------------------------*/

int macosx_version_is_at_least(int major, int minor, int bugfix)
{
    const int os_major  = macosx_version_major();
    const int os_minor  = macosx_version_minor();
    const int os_bugfix = macosx_version_bugfix();
    return (os_major > major
            || (os_major == major
                && (os_minor > minor
                    || (os_minor == minor
                        && os_bugfix >= bugfix))));
}

/*************************************************************************/
/*********************** Internal utility routines ***********************/
/*************************************************************************/

const char *macosx_get_application_name(void)
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
        NSString *name_string = [dict objectForKey: @"CFBundleName"];
        if (![name_string length]) {
            name_string = [[NSProcessInfo processInfo] processName];
        }
        const char *name = [name_string UTF8String];
        if (name) {
            saved_name = strdup(name);
        }
    }

    [pool release];

    return saved_name ? saved_name : "The application";
}

/*-----------------------------------------------------------------------*/

const char *macosx_get_application_support_path(void)
{
    static char *saved_path = NULL;

    /* If we already looked it up, return the looked-up value. */
    if (saved_path) {
        return saved_path;
    }

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    /* Get the directory path from the system. */
    const char *path = NULL;
    NSURL *url = [[NSFileManager defaultManager]
                     URLForDirectory:NSApplicationSupportDirectory
                     inDomain:NSUserDomainMask
                     appropriateForURL:nil
                     create:TRUE
                     error:nil];
    if (UNLIKELY(!url)) {
        DLOG("WARNING: URLForDirectory failed");
    } else if (UNLIKELY(![url isFileURL])) {
        DLOG("WARNING: NSURL for NSApplicationSupportDirectory is not a file"
             " URL, ignoring");
    } else {
        path = [url.path UTF8String];
    }

    /* Save a copy of the path, or generate it if we hit an error (which
     * should never occur, but play it safe). */
    if (LIKELY(path)) {
        saved_path = strdup(path);
        if (UNLIKELY(!saved_path)) {
            DLOG("Out of memory copying Application Support path: %s", path);
            return NULL;
        }
    } else {
        const char *home = getenv("HOME");
        if (!home || !*home) {
            home = ".";  // Fall back to the current directory if necessary.
        }
        const int path_size = strlen(home) // $HOME
                            + 29;          // "/Library/Application Support\0";
        saved_path = malloc(path_size);
        if (UNLIKELY(!saved_path)) {
            DLOG("Out of memory generating Application Support path (%u bytes)",
                 path_size);
            return NULL;
        }
        ASSERT(strformat_check(saved_path, path_size,
                               "%s/Library/Application Support", home),
               return NULL);
    }

    [pool release];

    /* Return the saved path. */
    return saved_path;
}

/*-----------------------------------------------------------------------*/

void *macosx_create_image(const void *data, int width, int height)
{
    PRECOND(data != NULL, return NULL);
    PRECOND(width > 0, return NULL);
    PRECOND(height > 0, return NULL);

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    NSBitmapImageRep *bitmap = [[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:NULL pixelsWide:width pixelsHigh:height
        bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO
        colorSpaceName:NSDeviceRGBColorSpace
        bitmapFormat:NSAlphaNonpremultipliedBitmapFormat
        bytesPerRow:width*4 bitsPerPixel:32];
    if (UNLIKELY(!bitmap)) {
        DLOG("Failed to create NSBitmapImageRep");
        [pool release];
        return NULL;
    }

    memcpy([bitmap bitmapData], data, width*height*4);

    NSImage *image = [[NSImage alloc] initWithSize:(NSSize){width, height}];
    if (UNLIKELY(!bitmap)) {
        DLOG("Failed to create NSImage");
        [pool release];
        return NULL;
    }
    [image addRepresentation:bitmap];  // Retains bitmap.
    [bitmap release];

    [pool release];
    return image;
}

/*-----------------------------------------------------------------------*/

CGRect macosx_window_frame_to_CGRect(NSRect frame)
{
    CGRect screen_frame = [NSScreen mainScreen].frame;
    frame.origin.y =
        screen_frame.size.height - (frame.origin.y + frame.size.height);
    return frame;
}

/*-----------------------------------------------------------------------*/

void macosx_show_dialog_formatted(const char *title_id,
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

    macosx_dialog(title, text);
    CFRelease(text);
    CFRelease(title);
}

/*************************************************************************/
/*************************************************************************/
