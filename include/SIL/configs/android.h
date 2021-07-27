/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/configs/android.h: Configuration header for Android.
 */

#ifndef SIL_CONFIGS_ANDROID_H
#define SIL_CONFIGS_ANDROID_H

/*************************************************************************/
/*************************************************************************/

#define SIL_PLATFORM_ANDROID
#define SIL_SYSTEM_COMMON_HEADER  "sysdep/android/common.h"

/* Architecture and endianness defines are set by the build rules. */

#define HAVE_STRDUP
#define HAVE_STRCASECMP
#define STRNICMP_SIZE_T  size_t

/* Android devices use OpenGL ES. */
#define SIL_OPENGL_ES

/* stdlib.h defines strtof as an inline function, so we have to rename it
 * to make use of our own version.  We undo this #define in
 * sysdep/android/common.h. */
#ifdef SIL_UTILITY_INCLUDE_STRTOF
# define strtof __android_strtof
#endif

/* Similarly, rename bogus inline functions from sysmacros.h that we have
 * absolutely no use for. */
#define major __android_major
#define minor __android_minor

/*-----------------------------------------------------------------------*/

/*
 * The following Android-specific configuration options are available and
 * may be enabled by defining the corresponding preprocessor symbol:
 *
 * - SIL_PLATFORM_ANDROID_DLOG_LOG_TAG:  Define this to the tag string to
 *   include in debug log messages.  Only meaningful when DEBUG is defined.
 *   Defaults to the string "SIL".
 */

#ifndef SIL_PLATFORM_ANDROID_DLOG_LOG_TAG
# define SIL_PLATFORM_ANDROID_DLOG_LOG_TAG  "SIL"
#endif

/*
 * The following Android-specific symbols are defined by the build system:
 *
 * - SIL_PLATFORM_ANDROID_MIN_SDK_VERSION:  Defined to the SDK level of
 *   the oldest Android OS version which the program will support (equal
 *   to the $(TARGET_OS_VERSION) build variable).
 */

#ifndef SIL_PLATFORM_ANDROID_MIN_SDK_VERSION
# error SIL_PLATFORM_ANDROID_MIN_SDK_VERSION was not defined by the build system!
#endif

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_CONFIGS_ANDROID_H
