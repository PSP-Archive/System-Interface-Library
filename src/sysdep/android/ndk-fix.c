/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/android/ndk-fix.c: Fixes for NDK breakage.
 */

/*
 * In the android-21 (Android 5.0 Lollipop) SDK, the Android developers
 * made the boneheaded decision to break compatibility with all previous
 * versions of Android by changing the implementations of some standard
 * library functions, such that functions which were previously missing
 * from the shared object on the device but defined as inline functions
 * in the SDK headers are no longer defined as inline.  This naturally
 * requires the functions to be present on the device -- which they are not
 * before Android 5.0!  To work around this breakage, we define replacement
 * functions in this file, create a library containing only this file, and
 * add it last in the link order so that all references are resolved to
 * these functions rather than the (possibly nonexistent) functions in the
 * on-device library.
 *
 * See also: https://code.google.com/p/android/issues/detail?id=73725
 */

#define IN_SYSDEP

#include "src/base.h"

#include <assert.h>
#include <dlfcn.h>
#include <signal.h>
#include <stdint.h>
#include <sys/stat.h>

/*************************************************************************/
/*************************************************************************/

/* Used by Clang coverage backend. */
int abs(int x)
{
    return x >= 0 ? x : -x;
}

/*-----------------------------------------------------------------------*/

/* Used by libpng. */
double atof(const char *s)
{
    return strtod(s, NULL);
}

/*-----------------------------------------------------------------------*/

/* Used by strformat() and in the math tests. */
int isinf(double x)
{
    union {double f; uint64_t i;} u = {.f = x};
    return (u.i & ~(UINT64_C(1)<<63)) == UINT64_C(0x7FF0000000000000);
}
int __isinff(float x)
{
    union {float f; uint32_t i;} u = {.f = x};
    return (u.i & 0x7FFFFFFF) == 0x7F800000;
}

/*-----------------------------------------------------------------------*/

/* Used by Clang coverage backend. */
long long llabs(long long x)
{
    return x >= 0 ? x : -x;
}

/*-----------------------------------------------------------------------*/

/* Used by the POSIX tests. */
int mkfifo(const char *pathname, mode_t mode)
{
    return mknod(pathname, (mode & ~S_IFMT) | S_IFIFO, 0);
}

/*-----------------------------------------------------------------------*/

/* Used by the POSIX tests. */
int sigemptyset(sigset_t *set)
{
    memset(set, 0, sizeof(*set));
    return 0;
}

/*-----------------------------------------------------------------------*/

/* Used by the POSIX tests. */
sighandler_t signal(int signum, sighandler_t handler)
{
    /* Android 4.4 has bsd_signal() but not signal(); Android 5.0 has
     * signal() but not bsd_signal().  ARGH. */
    void *libc = dlopen("/system/lib/libc.so", RTLD_LAZY);
    assert(libc);
    sighandler_t (*function)(int, sighandler_t) = dlsym(libc, "signal");
    if (!function) {
        function = dlsym(libc, "bsd_signal");
        assert(function);
    }
    sighandler_t result = (*function)(signum, handler);
    dlclose(libc);
    return result;
}

/*-----------------------------------------------------------------------*/

/* Used by Clang coverage backend. */
char *__strncpy_chk2(char *dest, const char *src, size_t n,
                     size_t dest_size, size_t src_size)
{
    assert(n <= dest_size);
    assert(n <= src_size);
    return strncpy(dest, src, n);
}

/*************************************************************************/
/*************************************************************************/
