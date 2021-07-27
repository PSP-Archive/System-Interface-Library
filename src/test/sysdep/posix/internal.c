/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/posix/internal.c: Helper functions for POSIX
 * file-related tests.
 */

#include "src/base.h"
#include "src/random.h"
#include "src/test/base.h"
#include "src/test/sysdep/posix/internal.h"

#ifdef SIL_PLATFORM_ANDROID
# include "src/sysdep.h"
# include "src/sysdep/android/internal.h"
#endif

#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/*************************************************************************/
/*************************************************************************/

const char *posix_get_tmpdir(void)
{
#ifdef SIL_PLATFORM_ANDROID
    /* $TMPDIR doesn't seem to be defined on Android, and /tmp doesn't
     * exist either, so fall back to the internal data directory. */
    ASSERT(android_internal_data_path != NULL);
    return android_internal_data_path;
#else
    const char *tmpdir = getenv("TMPDIR");
    if (tmpdir && *tmpdir) {
        return tmpdir;
    } else {
        return "/tmp";
    }
#endif
}

/*-----------------------------------------------------------------------*/

int posix_create_temporary_dir(const char *basename, char *pathbuf,
                               int pathbuf_size)
{
    PRECOND(basename != NULL);
    PRECOND(pathbuf != NULL);
    PRECOND(pathbuf_size > 0);

    srandom_env();
    const char *tmpdir = posix_get_tmpdir();
    for (int tries = 10; tries > 0; tries--) {
        if (!strformat_check(pathbuf, pathbuf_size - 1,
                             "%s%s%s_%u_%d",
                             tmpdir, tmpdir[strlen(tmpdir)-1]=='/' ? "" : "/",
                             basename, getpid(), random32())) {
            DLOG("Buffer overflow generating temporary path (tmpdir=%s)",
                 tmpdir);
            return 0;
        }
        if (mkdir(pathbuf, S_IRWXU) == 0) {
            return 1;
        }
        DLOG("Failed to create temporary directory %s: %s", pathbuf,
             strerror(errno));
    }
    DLOG("Unable to create temporary directory, giving up");
    return 0;
}

/*-----------------------------------------------------------------------*/

int posix_remove_temporary_dir(const char *path, int *had_temp_files_ret)
{
    *had_temp_files_ret = 0;

    DIR *d = opendir(path);
    if (!d) {
        if (errno == ENOENT) {
            return 1;
        } else {
            DLOG("opendir(%s): %s", path, strerror(errno));
            return 0;
        }
    }
    for (struct dirent *de; (de = readdir(d)) != NULL; ) {
        if (strncmp(de->d_name, "..", strlen(de->d_name)) == 0) {
            continue;
        }
        char buf[PATH_MAX];
        ASSERT(strformat_check(buf, sizeof(buf), "%s/%s", path, de->d_name));
        struct stat st;
        if (stat(buf, &st) == 0 && S_ISDIR(st.st_mode)) {
            int sub_had_temp_files;
            if (!posix_remove_temporary_dir(buf, &sub_had_temp_files)) {
                return 0;
            }
            *had_temp_files_ret |= sub_had_temp_files;
        } else {
            if (unlink(buf) != 0) {
                DLOG("unlink(%s): %s", buf, strerror(errno));
            }
            *had_temp_files_ret |= (de->d_name[strlen(de->d_name)-1] == '~');
        }
    }
    closedir(d);

    if (rmdir(path) != 0) {
        DLOG("rmdir(%s): %s", path, strerror(errno));
        return 0;
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

int posix_pipe_writer(void *path)
{
    void (*old_sigpipe)(int) = signal(SIGPIPE, SIG_IGN);
    nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = 10*1000*1000}, NULL);
    const int wfd = open((const char *)path, O_WRONLY);
    ASSERT(wfd >= 0);
    int bytes_written = write(wfd, "foo", 3);
    CHECK_INTEQUAL(close(wfd), 0);
    signal(SIGPIPE, old_sigpipe);
    return bytes_written > 0 ? bytes_written : 0;
}

/*************************************************************************/
/*************************************************************************/
