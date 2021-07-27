/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/posix/fileutil.c: Miscellaneous file utility functions.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/posix/fileutil.h"
#include "src/sysdep/posix/path_max.h"

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>  // For rename().
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>

/*************************************************************************/
/*************************************************************************/

/**
 * SAFE_DLOG:  Call DLOG(), preserving the value of errno across the call.
 */
#ifdef DEBUG
# define SAFE_DLOG(...)  do { \
    int _errno_save = errno;  \
    DLOG(__VA_ARGS__);        \
    errno = _errno_save;      \
} while (0)
#else
# define SAFE_DLOG(...)  /*nothing*/
#endif

/*-----------------------------------------------------------------------*/

void *posix_read_file(const char *path, ssize_t *size_ret,
                      unsigned int mem_flags)
{
    PRECOND(path != NULL, errno = EINVAL; return NULL);
    PRECOND(size_ret != NULL, errno = EINVAL; return NULL);

    char *buffer = NULL;

    /* Open the file and get its size. */
    const int fd = open(path, O_RDONLY);
    if (fd < 0) {
        if (errno != ENOENT) {  // Don't complain if it's just a missing file.
            SAFE_DLOG("open(%s) failed: %s", path, strerror(errno));
        }
        goto fail;
    }

    ssize_t size = lseek(fd, 0, SEEK_END);
    if (size < 0) {
        SAFE_DLOG("lseek(%s,0,SEEK_END) failed: %s", path, strerror(errno));
        goto fail_close_fd;
    }
    /* The second seek will always succeed if the first one did. */
    ASSERT(lseek(fd, 0, SEEK_SET) == 0, goto fail_close_fd);

    /* Allocate a buffer for loading the file. */
    buffer = mem_alloc(size > 0 ? size : 1, 0, mem_flags);
    if (!buffer) {
        DLOG("%s: Out of memory (unable to allocate %lld bytes)", path,
             (long long)size);
        errno = ENOMEM;
        goto fail_close_fd;
    }

    /* Read the file contents into the buffer. */
    ssize_t bytes_read = 0;
    while (bytes_read < size) {
        const ssize_t count = read(fd, buffer + bytes_read, size - bytes_read);
        if (count > 0) {
            bytes_read += count;
        } else {
            SAFE_DLOG("read(%s) failed: %s", path,
                      count < 0 ? strerror(errno) : "Short read");
            break;
        }
    }
    if (bytes_read != size) {
        goto fail_close_fd;
    }

    /* Close the file, and return the buffer and its size. */
    close(fd);
    *size_ret = size;
    errno = 0;
    return buffer;

    /* Error cases jump down here. */
  fail_close_fd:
    {
        int errno_save = errno;
        close(fd);
        errno = errno_save;
    }
  fail:
    mem_free(buffer);
    return NULL;
}

/*-----------------------------------------------------------------------*/

int posix_write_file(const char *path, const void *data, ssize_t size,
                     int sync)
{
    PRECOND(path != NULL, errno = EINVAL; return 0);
    PRECOND(data != NULL, errno = EINVAL; return 0);
    PRECOND(size >= 0, errno = EINVAL; return 0);

    /* See if we can write to the target path in the first place. */
    if (access(path, F_OK) == 0 && access(path, W_OK) != 0) {
        DLOG("%s is not writable, failing", path);
        errno = EACCES;
        goto fail;
    }

    /* Generate a temporary filename to use for writing, so we don't
     * destroy the original if a write error occurs. */
    char temppath[PATH_MAX];
    if (!strformat_check(temppath, sizeof(temppath), "%s~", path)) {
        SAFE_DLOG("Buffer overflow generating temporary pathname for %s",
                  path);
        goto fail;
    }

    /* Create any necessary parent directories. */
    char *s = strrchr(temppath, '/');
    if (s) {
        *s = '\0';
        if (!posix_mkdir_p(temppath)) {
            SAFE_DLOG("Failed to create parent directory %s of %s: %s",
                      temppath, path, strerror(errno));
            goto fail;
        }
        *s++ = '/';
    }

    /* Write the data to the temporary file. */
    int fd = open(temppath, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        SAFE_DLOG("open(%s) failed: %s", temppath, strerror(errno));
        goto fail;
    }
    ssize_t bytes_written = 0;
    while (bytes_written < size) {
        const ssize_t count =
            write(fd, (const char *)data + bytes_written, size - bytes_written);
        if (count > 0) {
            bytes_written += count;
        } else {
            /* POSIX forbids a return value of zero for a positive write
             * count, so this must be an error condition. */
            ASSERT(count < 0);
            SAFE_DLOG("write(%s) failed: %s", temppath, strerror(errno));
            goto fail_unlink;
        }
    }
    if (sync) {
#if defined(SIL_PLATFORM_MACOSX) || defined(SIL_PLATFORM_IOS)
        const int sync_result = fcntl(fd, F_FULLFSYNC);
#else
        const int sync_result = fdatasync(fd);
#endif
        if (sync_result != 0) {
            SAFE_DLOG("fdatasync(%s) failed: %s", temppath, strerror(errno));
            goto fail_unlink;
        }
    }
    const int close_res = close(fd);
    fd = -1;
    if (close_res != 0) {
        SAFE_DLOG("close(%s) failed: %s", temppath, strerror(errno));
        goto fail_unlink;
    }

    /* Rename the temporary file to the final filename.  (POSIX specifies
     * that the rename operation is atomic, so if the rename fails, the
     * original file will not be lost.) */
    if (rename(temppath, path) != 0) {
        SAFE_DLOG("rename(%s, %s) failed: %s", temppath, path,
                  strerror(errno));
        goto fail_unlink;
    }

    /* Success! */
    errno = 0;
    return 1;

    /* Error cases jump down here. */
  fail_unlink:
    {
        int errno_save = errno;
        if (fd >= 0) {
            close(fd);
        }
        unlink(temppath);
        errno = errno_save;
    }
  fail:
    return 0;
}

/*-----------------------------------------------------------------------*/

int posix_copy_file(const char *from, const char *to,
                    int preserve_times, ssize_t buffer_size)
{
    PRECOND(from != NULL, errno = EINVAL; return 0);
    PRECOND(to != NULL, errno = EINVAL; return 0);

    if (buffer_size == 0) {
        buffer_size = 65536;  // Default buffer size.
    }

    /* Retrieve the source file's timestamps if copying is requested.  (We
     * do this first (1) so we don't update the atime before we read it,
     * and (2) so if the source and destination happen to be the same, we
     * still use the correct timestamps.) */
    struct utimbuf ut;
    if (preserve_times) {
        struct stat st;
        if (stat(from, &st) != 0) {
            SAFE_DLOG("stat(%s) failed: %s", from, strerror(errno));
            goto fail;
        }
        ut.actime = st.st_atime;
        ut.modtime = st.st_mtime;
    }

    /* See if we can write to the target path in the first place. */
    if (access(to, F_OK) == 0 && access(to, W_OK) != 0) {
        DLOG("%s is not writable, failing", to);
        errno = EACCES;
        goto fail;
    }

    /* Allocate a temporary buffer for copying. */
    char *buffer = mem_alloc(buffer_size, 0, MEM_ALLOC_TEMP);
    if (!buffer) {
        DLOG("Out of memory (unable to allocate %lld bytes)",
             (long long)buffer_size);
        errno = ENOMEM;
        goto fail;
    }

    /* Open the source file. */
    int from_fd = open(from, O_RDONLY);
    if (from_fd < 0) {
        if (errno != ENOENT) {  // Don't complain if it's just a missing file.
            SAFE_DLOG("open(%s) failed: %s", from, strerror(errno));
        }
        goto fail_free_buffer;
    }

    /* Generate a temporary filename to use for writing and open the
     * temporary file, creating any necessary parent directories. */
    char temppath[PATH_MAX];
    if (!strformat_check(temppath, sizeof(temppath), "%s~", to)) {
        SAFE_DLOG("Buffer overflow generating temporary pathname for %s", to);
        goto fail_close_from_fd;
    }
    char *s = strrchr(temppath, '/');
    if (s) {
        *s = '\0';
        if (!posix_mkdir_p(temppath)) {
            SAFE_DLOG("Failed to create parent directory %s of %s: %s",
                      temppath, to, strerror(errno));
            goto fail_close_from_fd;
        }
        *s++ = '/';
    }
    int to_fd = open(temppath, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (to_fd < 0) {
        SAFE_DLOG("open(%s) failed: %s", temppath, strerror(errno));
        goto fail_close_from_fd;
    }

    /* Copy the file contents. */
    for (ssize_t bytes_read;
         (bytes_read = read(from_fd, buffer, buffer_size)) != 0; )
    {
        if (bytes_read < 0) {
            SAFE_DLOG("read(%s) failed: %s", from, strerror(errno));
            goto fail_unlink;
        }
        ssize_t bytes_written = 0;
        while (bytes_written < bytes_read) {
            const ssize_t count = write(to_fd, buffer + bytes_written,
                                        bytes_read - bytes_written);
            if (count > 0) {
                bytes_written += count;
            } else {
                ASSERT(count < 0);
                SAFE_DLOG("write(%s) failed: %s", to, strerror(errno));
                goto fail_unlink;
            }
        }
    }

    /* Free the copy buffer and close the files. */
    mem_free(buffer);
    buffer = NULL;
    close(from_fd);
    /* close() failure on the input file is meaningless to us. */
    from_fd = -1;
    const int close_res = close(to_fd);
    to_fd = -1;
    if (close_res != 0) {
        SAFE_DLOG("close(%s) failed: %s", temppath, strerror(errno));
        goto fail_unlink;
    }

    /* Update the output file's timestamps, if requested. */
    if (preserve_times) {
        if (utime(temppath, &ut) != 0) {
            SAFE_DLOG("utime(%s) failed: %s", temppath, strerror(errno));
            goto fail_unlink;
        }
    }

    /* Move the temporary output file to its final name. */
    if (rename(temppath, to) != 0) {
        SAFE_DLOG("rename(%s, %s) failed: %s", temppath, to, strerror(errno));
        goto fail_unlink;
    }

    /* Success! */
    errno = 0;
    return 1;

    /* Error cases jump down here. */
  fail_unlink:
    {
        int errno_save = errno;
        unlink(temppath);
        if (to_fd >= 0) {
            close(to_fd);
        }
        errno = errno_save;
    }
  fail_close_from_fd:
    if (from_fd >= 0) {
        int errno_save = errno;
        close(from_fd);
        errno = errno_save;
    }
  fail_free_buffer:
    mem_free(buffer);
  fail:
    return 0;
}

/*-----------------------------------------------------------------------*/

int posix_mkdir_p(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            errno = 0;
            return 1;
        } else {
            errno = EEXIST;
            return 0;
        }
    }

    char temppath[PATH_MAX];
    if (!strformat_check(temppath, sizeof(temppath), "%s", path)) {
        DLOG("Buffer overflow working on pathname %s", path);
        errno = ENAMETOOLONG;
        return 0;
    }
    char *s = temppath+1;
    while (s += strcspn(s, "/"), *s != '\0') {
        *s = '\0';
        if (stat(temppath, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                errno = 0;
            } else {
                errno = ENOTDIR;
            }
        } else {
            if (mkdir(temppath, 0777) == 0) {
                errno = 0;
            }
        }
        if (errno != 0) {
            SAFE_DLOG("Failed to create parent directory %s of %s: %s",
                      temppath, path, strerror(errno));
            return 0;
        }
        *s++ = '/';
    }

    if (mkdir(path, 0777) == 0) {
        errno = 0;
    } else if (errno == EEXIST) {
        /* If the path was something like "foo/bar/." then the directory
         * will exist now even though it didn't exist when the function
         * was called, so don't treat that as an error. */
        if (stat(temppath, &st) == 0 && S_ISDIR(st.st_mode)) {
            errno = 0;
        } else {
            /* This can only be reached if another process or thread is
             * racing with this one on the same path and created a file at
             * the target location immediately before the final mkdir()
             * call above.  The stat() call in particular can only fail if
             * the target was subsequently removed or a parent directory
             * was removed or made unreadable/unsearchable between the
             * mkdir() and stat() calls. */
            errno = EEXIST;  // Restore errno in case stat() overwrote it.
        }
    }
    return errno == 0;
}

/*-----------------------------------------------------------------------*/

int posix_rmdir_r(const char *path)
{
    int ok = 1;

    DIR *d = opendir(path);
    if (!d) {
        SAFE_DLOG("Failed to scan directory %s: %s", path, strerror(errno));
        return 0;
    }

    int errno_save = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (strcmp(de->d_name, ".") != 0 && strcmp(de->d_name, "..") != 0) {
            char temppath[PATH_MAX];
            if (!strformat_check(temppath, sizeof(temppath),
                                 "%s/%s", path, de->d_name)) {
                DLOG("Failed to remove %s/%s: Buffer overflow",
                     path, de->d_name);
                errno_save = ENAMETOOLONG;
                ok = 0;
            } else {
                /* We have to call lstat() first because POSIX specifies
                 * unlink() as having undefined behavior when given a path
                 * naming a directory.  lstat() will never fail unless we
                 * lose a race with another process that's also removing
                 * the file (or making a parent directory inaccessible). */
                struct stat st;
                if (lstat(temppath, &st) == 0 && S_ISDIR(st.st_mode)) {
                    if (!posix_rmdir_r(temppath)) {
                        errno_save = errno;
                        ok = 0;
                    }
                } else if (unlink(temppath) != 0) {
                    SAFE_DLOG("Failed to remove %s: %s", temppath,
                              strerror(errno));
                    errno_save = errno;
                    ok = 0;
                }
            }
        }
    }
    closedir(d);
    errno = errno_save;

    if (ok && rmdir(path) != 0) {
        SAFE_DLOG("Failed to remove directory %s: %s", path, strerror(errno));
        ok = 0;
    }

    return ok;
}

/*************************************************************************/
/*************************************************************************/
