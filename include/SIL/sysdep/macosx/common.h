/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/sysdep/macosx/common.h: Global declarations specific to
 * Mac OS X.
 */

#ifndef SIL_SYSDEP_MACOSX_COMMON_H
#define SIL_SYSDEP_MACOSX_COMMON_H

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
 * macosx_version_major, macosx_version_minor, macosx_version_bugfix:
 * Return the major, minor, or bugfix version (respectively, the first,
 * second, or third dotted component of the version number) of the
 * operating system on which the program is running.
 *
 * [Return value]
 *     Major, minor, or bugfix version of the operating system.
 */
extern int macosx_version_major(void);
extern int macosx_version_minor(void);
extern int macosx_version_bugfix(void);

/**
 * macosx_version_is_at_least:  Return whether the version of the operating
 * system on which the program is running is at least the given version.
 *
 * [Parameters]
 *     major: Major version to test against.
 *     minor: Minor version to test against.
 *     bugfix: Bugfix version to test against.
 * [Return value]
 *     True if the operating system is version <major>.<minor>.<bugfix> or
 *     later, false if not.
 */
extern int macosx_version_is_at_least(int major, int minor, int bugfix);

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_SYSDEP_MACOSX_COMMON_H
