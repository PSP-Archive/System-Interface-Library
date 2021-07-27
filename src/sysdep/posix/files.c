/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/posix/files.c: Data file access interface for POSIX-compatible
 * systems.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/misc/ioqueue.h"
#include "src/sysdep/posix/files.h"
#include "src/sysdep/posix/path_max.h"

#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>

/*************************************************************************/
/***************************** Configuration *****************************/
/*************************************************************************/

/**
 * SIL_PLATFORM_POSIX_ESTIMATED_READ_SPEED:  The estimated speed at which
 * data can be read from files, in bytes per second.  This is used to
 * calculate the time by which a read request should be started in order
 * to have it finish by the specified deadline.
 */
#ifndef SIL_PLATFORM_POSIX_ESTIMATED_READ_SPEED
# define SIL_PLATFORM_POSIX_ESTIMATED_READ_SPEED  10000000  // 10MB/sec
#endif

/*************************************************************************/
/***************************** Exported data *****************************/
/*************************************************************************/

#ifdef SIL_INCLUDE_TESTS

uint8_t TEST_posix_file_fail_init;

#endif

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* File handle structure. */
struct SysFile {
    /* File descriptor used with system calls. */
    int fd;
    /* File size (discovered at open time). */
    int64_t filesize;
    /* Current synchronous read position. */
    int64_t filepos;

    /* Pathname with which this file was opened. */
    char *path;
};

/* Directory handle structure. */
struct SysDir {
    /* Path passed to sys_dir_open(). */
    char *path;
    /* Directory descriptor for readdir() calls. */
    DIR *d;
};

/*-----------------------------------------------------------------------*/

/* Asynchronous operation data. */
typedef struct AsyncInfo {
    SysFile *fh;  // File handle for this operation (NULL = unused entry).
    int ioqueue_request;  // Request ID for this operation.
    uint8_t aborted;  // True if the request has been aborted.
} AsyncInfo;

/* Array of async operation blocks.  (We use a static, fixed-size array
 * for simplicity.) */
static AsyncInfo async_info[MAX_ASYNC_READS];
static const int async_info_size = lenof(async_info);

/* Mutex for accessing the async_info array.  This only needs to be
 * locked when allocating a new async operation block; any array entry
 * with fh != NULL belongs to that filehandle. */
static pthread_mutex_t async_info_mutex = PTHREAD_MUTEX_INITIALIZER;

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * open_nocase:  Open the specified path case-insensitively, following the
 * rules for sys_file_open() and sys_dir_open().
 *
 * [Parameters]
 *     path: Pathname of file or directory to open.
 *     as_dir: True to open as a directory, false to open as a file.
 *     result_ret: Pointer to variable to receive result: pointer to int
 *         for file opens, pointer to DIR* for directory opens.
 *     path_ret: Buffer into which to store the actual path used for the
 *         operation (may be the same string as the "path" argument).
 *     path_ret_size: Size of path_ret buffer (must be >= strlen(path)+1).
 * [Return value]
 *     True on success, false on error.
 */
static int open_nocase(const char *path, int as_dir, void *retptr,
                       char *path_ret, unsigned int path_ret_size);

/*************************************************************************/
/******************* Interface: Initialization/cleanup *******************/
/*************************************************************************/

int sys_file_init(void)
{
#ifdef SIL_INCLUDE_TESTS
    if (TEST_posix_file_fail_init) {
        TEST_posix_file_fail_init = 0;
        return 0;
    }
#endif
    return ioq_init();
}

/*-----------------------------------------------------------------------*/

void sys_file_cleanup(void)
{
    ioq_reset();
}

/*************************************************************************/
/********************** Interface: File operations ***********************/
/*************************************************************************/

SysFile *sys_file_open(const char *path)
{
    if (UNLIKELY(!path)) {
        DLOG("path == NULL");
        errno = EINVAL;
        return NULL;
    }
    if (UNLIKELY(!*path)) {
        DLOG("path is empty");
        errno = ENOENT;
        return NULL;
    }

    SysFile *fh = mem_alloc(sizeof(*fh), 0, MEM_ALLOC_TEMP);
    if (UNLIKELY(!fh)) {
        errno = ENOMEM;
        return NULL;
    }
    const int path_size = strlen(path) + 1;
    fh->path = mem_alloc(path_size, 1, MEM_ALLOC_TEMP);
    if (UNLIKELY(!fh->path)) {
        mem_free(fh);
        errno = ENOMEM;
        return NULL;
    }
    if (UNLIKELY(!open_nocase(path, 0, &fh->fd, fh->path, path_size))) {
        int errno_save = errno;
        mem_free(fh->path);
        mem_free(fh);
        errno = errno_save;
        return NULL;
    }

    fh->filesize = lseek(fh->fd, 0, SEEK_END);
    if (UNLIKELY(fh->filesize < 0)) {
        int errno_save = errno;
        DLOG("%s: failed to get file size: %s", path, strerror(errno));
        close(fh->fd);
        mem_free(fh->path);
        mem_free(fh);
        errno = errno_save;
        return NULL;
    }

    fh->filepos = 0;

    return fh;
}

/*-----------------------------------------------------------------------*/

SysFile *sys_file_dup(SysFile *fh)
{
    if (UNLIKELY(!fh)) {
        DLOG("fh == NULL");
        errno = EINVAL;
        return NULL;
    }

    SysFile *newfh = mem_alloc(sizeof(*newfh), 0, MEM_ALLOC_TEMP);
    if (UNLIKELY(!newfh)) {
        errno = ENOMEM;
        return NULL;
    }

    newfh->path = mem_strdup(fh->path, MEM_ALLOC_TEMP);
    if (UNLIKELY(!newfh->path)) {
        DLOG("No memory to copy pathname");
        mem_free(newfh);
        errno = ENOMEM;
        return NULL;
    }

    /* IMPORTANT NOTE:  File descriptors created with the dup() system call
     * share file position pointers with the original descriptor.  Because
     * of this, attempting to perform simultaneous synchronous reads on
     * both the original and the duplicated file handle may lead to
     * incorrect results in a multithreaded environment.  To avoid this,
     * we implement synchronous reads for sys_file_read() using the
     * asynchronous I/O interface, which doesn't suffer from that problem.
     * (Another solution would be to call open() again using the stored
     * pathname, though that would incur the cost of directory traversal.) */

    newfh->fd = dup(fh->fd);
    if (UNLIKELY(newfh->fd < 0)) {
        int errno_save = errno;
        mem_free(newfh->path);
        mem_free(newfh);
        errno = errno_save;
        return NULL;
    }

    newfh->filesize = fh->filesize;
    newfh->filepos = fh->filepos;

    return newfh;
}

/*-----------------------------------------------------------------------*/

void sys_file_close(SysFile *fh)
{
    if (!fh) {
        return;
    }

    for (int i = 0; i < async_info_size; i++) {
        if (async_info[i].fh == fh) {
            sys_file_abort_async(i+1);
        }
    }

    close(fh->fd);
    mem_free(fh->path);
    mem_free(fh);
}

/*-----------------------------------------------------------------------*/

int64_t sys_file_size(SysFile *fh)
{
    if (UNLIKELY(!fh)) {
        DLOG("fh == NULL");
        errno = EINVAL;
        return 0;
    }

    return fh->filesize;
}

/*-----------------------------------------------------------------------*/

int sys_file_seek(SysFile *fh, int64_t pos, int how)
{
    if (UNLIKELY(!fh)) {
        DLOG("fh == NULL");
        errno = EINVAL;
        return 0;
    }

    if (how == FILE_SEEK_SET) {
        fh->filepos = pos;
    } else if (how == FILE_SEEK_CUR) {
        fh->filepos += pos;
    } else if (how == FILE_SEEK_END) {
        fh->filepos = fh->filesize + pos;
    } else {
        DLOG("Invalid how: %d", how);
        errno = EINVAL;
        return 0;
    }

    if (fh->filepos < 0) {
        fh->filepos = 0;
    } else if (fh->filepos > fh->filesize) {
        fh->filepos = fh->filesize;
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

int64_t sys_file_tell(SysFile *fh)
{
    if (UNLIKELY(!fh)) {
        DLOG("fh == NULL");
        errno = EINVAL;
        return 0;
    }

    return fh->filepos;
}

/*-----------------------------------------------------------------------*/

int32_t sys_file_read(SysFile *fh, void *buf, int32_t len)
{
    if (UNLIKELY(!fh) || UNLIKELY(!buf) || UNLIKELY(len < 0)) {
        DLOG("Invalid parameters: %p %p %d", fh, buf, len);
        errno = EINVAL;
        return -1;
    }

    if (len == 0) {
        return 0;
    }

    /* We funnel this through the asynchronous ioqueue interface to avoid
     * inter-thread collisions on the synchronous file pointer.  See
     * the IMPORTANT NOTE in sys_file_dup() for details. */
    const int request = ioq_read(fh->fd, buf, len, fh->filepos, 0);
    if (!request) {
        int errno_save = errno;
        DLOG("(%d,%p,%d): Failed to start read operation: %s",
             fh->fd, buf, len, strerror(errno));
        errno = errno_save;
        return -1;
    }
    int error;
    int32_t nread = ioq_wait(request, &error);
    if (nread < 0) {
        DLOG("(%d,%p,%d): Read operation failed: %s",
             fh->fd, buf, len, strerror(errno));
        errno = error;
        return -1;
    }

    fh->filepos += nread;
    return nread;
}

/*-----------------------------------------------------------------------*/

int32_t sys_file_read_at(SysFile *fh, void *buf, int32_t len, int64_t filepos)
{
    if (UNLIKELY(!fh)
     || UNLIKELY(!buf)
     || UNLIKELY(len < 0)
     || UNLIKELY(filepos < 0)) {
        DLOG("Invalid parameters: %p %p %d %lld", fh, buf, len,
             (long long)filepos);
        errno = EINVAL;
        return -1;
    }

    if (len == 0) {
        return 0;
    }

    const int request = ioq_read(fh->fd, buf, len, filepos, -1);
    if (!request) {
        int errno_save = errno;
        DLOG("(%d,%p,%d,%lld): Failed to start read operation: %s",
             fh->fd, buf, len, (long long)filepos, strerror(errno));
        errno = errno_save;
        return -1;
    }
    int error;
    int32_t nread = ioq_wait(request, &error);
    if (nread < 0) {
        DLOG("(%d,%p,%d,%lld): Read operation failed: %s",
             fh->fd, buf, len, (long long)filepos, strerror(error));
        errno = error;
        return -1;
    }

    return nread;
}

/*-----------------------------------------------------------------------*/

int sys_file_read_async(SysFile *fh, void *buf, int32_t len,
                        int64_t filepos, float deadline)
{
    if (UNLIKELY(!fh)
     || UNLIKELY(!buf)
     || UNLIKELY(len < 0)
     || UNLIKELY(filepos < 0)) {
        DLOG("Invalid parameters: %p %p %d %lld %g", fh, buf, len,
             (long long)filepos, deadline);
        errno = EINVAL;
        return 0;
    }

    if (deadline >= 0) {
        deadline -= (float)len / SIL_PLATFORM_POSIX_ESTIMATED_READ_SPEED;
        if (deadline < 0) {
            deadline = 0;
        }
    } else {
        deadline = -1;
    }

    pthread_mutex_lock(&async_info_mutex);

    int index;
    for (index = 0; index < async_info_size; index++) {
        if (!async_info[index].fh) {
            break;
        }
    }
    if (index >= async_info_size) {
        pthread_mutex_unlock(&async_info_mutex);
        errno = ENOEXEC;  // We'll never see this error elsewhere, so reuse it.
        return 0;
    }
    async_info[index].fh = fh;
    async_info[index].aborted = 0;

    pthread_mutex_unlock(&async_info_mutex);

    async_info[index].ioqueue_request =
        ioq_read(fh->fd, buf, len, filepos, deadline);
    if (UNLIKELY(async_info[index].ioqueue_request == 0)) {
        async_info[index].fh = NULL;
        BARRIER();
        return 0;
    }

    return index+1;
}

/*-----------------------------------------------------------------------*/

int sys_file_poll_async(int request)
{
    if (UNLIKELY(request <= 0 || request > async_info_size)) {
        DLOG("Request %d out of range", request);
        errno = ESRCH;  // As in ioqueue.c.
        return 1;
    }

    const unsigned int index = request-1;
    if (UNLIKELY(!async_info[index].fh)) {
        errno = ESRCH;
        return 1;
    }

    return ioq_poll(async_info[index].ioqueue_request);
}

/*-----------------------------------------------------------------------*/

int32_t sys_file_wait_async(int request)
{
    if (UNLIKELY(request <= 0 || request > async_info_size)) {
        DLOG("Request %d out of range", request);
        errno = ESRCH;
        return -1;
    }

    const unsigned int index = request-1;
    if (UNLIKELY(!async_info[index].fh)) {
        errno = ESRCH;
        return -1;
    }

    int error;
    int32_t retval = ioq_wait(async_info[index].ioqueue_request, &error);
    if (async_info[index].aborted) {
        retval = -1;
        error = ECANCELED;
    }

    async_info[index].fh = NULL;
    BARRIER();
    if (retval < 0) {
        errno = error;
    }
    return retval;
}

/*-----------------------------------------------------------------------*/

int sys_file_abort_async(int request)
{
    if (UNLIKELY(request <= 0 || request > async_info_size)) {
        DLOG("Request %d out of range", request);
        errno = ESRCH;
        return 0;
    }

    const int index = request-1;
    if (!async_info[index].fh) {
        errno = ESRCH;
        return 0;
    }

    ioq_cancel(async_info[index].ioqueue_request);
    async_info[index].aborted = 1;

    return 1;
}

/*************************************************************************/
/******************** Interface: Directory operations ********************/
/*************************************************************************/

SysDir *sys_dir_open(const char *path)
{
    int return_error;

    if (UNLIKELY(!path)) {
        DLOG("path == NULL");
        return_error = EINVAL;
        goto error_return;
    }
    if (UNLIKELY(!*path)) {
        DLOG("path is empty");
        return_error = ENOENT;
        goto error_return;
    }

    SysDir *dir = mem_alloc(sizeof(*dir), 0, MEM_ALLOC_TEMP);
    if (UNLIKELY(!dir)) {
        DLOG("No memory for SysDir structure");
        return_error = ENOMEM;
        goto error_return;
    }
    dir->path = mem_strdup(path, MEM_ALLOC_TEMP);
    if (UNLIKELY(!dir->path)) {
        DLOG("No memory for path copy: %s", path);
        return_error = ENOMEM;
        goto error_free_dir;
    }

    /* Remove any trailing slash from the pathname (unless it's just "/"). */
    const int len = strlen(dir->path);
    if (len > 1 && dir->path[len-1] == '/') {
        dir->path[len-1] = '\0';
    }

    if (!open_nocase(dir->path, 1, &dir->d, dir->path, strlen(dir->path)+1)) {
        return_error = errno;
        goto error_free_dir_path;
    }

    return dir;

  error_free_dir_path:
    mem_free(dir->path);
  error_free_dir:
    mem_free(dir);
  error_return:
    errno = return_error;
    return NULL;
}

/*-----------------------------------------------------------------------*/

const char *sys_dir_read(SysDir *dir, int *is_subdir_ret)
{
    if (UNLIKELY(!dir) || UNLIKELY(!is_subdir_ret)) {
        DLOG("Invalid parameters: %p %p", dir, is_subdir_ret);
        errno = EINVAL;
        return NULL;
    }

    struct dirent *de;
    while ((de = readdir(dir->d)) != NULL) {
        if (strncmp(de->d_name, "..", strlen(de->d_name)) == 0) {
            continue;
        }

        char pathbuf[PATH_MAX];
        if (!strformat_check(pathbuf, sizeof(pathbuf), "%s/%s", dir->path,
                             de->d_name)) {
            DLOG("Buffer overflow on path (skipping): %s/%s", dir->path,
                 de->d_name);
            continue;
        }

        struct stat st;
        if (stat(pathbuf, &st) == 0
         && (S_ISREG(st.st_mode) || S_ISDIR(st.st_mode))) {
            *is_subdir_ret = S_ISDIR(st.st_mode);
            break;
        }
    }

    if (de) {
        return de->d_name;
    } else {
        return NULL;
    }
}

/*-----------------------------------------------------------------------*/

void sys_dir_close(SysDir *dir)
{
    if (dir) {
        closedir(dir->d);
        mem_free(dir->path);
        mem_free(dir);
    }
}

/*************************************************************************/
/*********************** Exported utility routines ***********************/
/*************************************************************************/

int posix_fileno(const SysFile *fh)
{
    PRECOND(fh != NULL, return -1);
    return fh->fd;
}

/*-----------------------------------------------------------------------*/

const char *posix_file_path(const SysFile *fh)
{
    PRECOND(fh != NULL, return NULL);
    return fh->path;
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static int open_nocase(const char *path, int as_dir, void *result_ret,
                       char *path_ret, unsigned int path_ret_size)
{
    PRECOND(path != NULL, errno = EINVAL; return 0);
    PRECOND(*path != 0, errno = EINVAL; return 0);
    PRECOND(result_ret != NULL, errno = EINVAL; return 0);
    PRECOND(path_ret != NULL, errno = EINVAL; return 0);
    PRECOND(path_ret_size >= strlen(path)+1, errno = EINVAL; return 0);

    char pathbuf[PATH_MAX];   // Actual pathname determined so far.
    unsigned int pathlen;
#ifdef DEBUG
    const char *path_orig = path;  // Save original string for error messages.
#endif

    if (strcmp(path, "/") == 0) {
        /* This isn't handled properly by the loop below. */
        strcpy(pathbuf, "/");  // Safe.
        goto have_final_path;
    }

    /* Initialize the actual path buffer. */

    if (*path == '/') {
        pathlen = 0;
        path++;  // Skip the leading slash.
    } else {
        pathbuf[0] = '.';
        pathlen = 1;
    }
    pathbuf[pathlen] = '\0';

    /* Complete the pathname one component at a time. */

    while (*path) {

        /* Extract this path component. */
        const char *next = path + strcspn(path, "/");  // Next path component.
        if (next == path) {
            DLOG("Empty path element in: %s", path_orig);
            errno = ENOENT;
            return 0;
        }
        const unsigned int thislen = next - path;  // Length of this component.

        /* See if there's an exact match for this component, and skip the
         * directory search if so. */
        unsigned int res = strformat(pathbuf+pathlen, sizeof(pathbuf)-pathlen,
                                     "/%.*s", thislen, path);
        res += pathlen;
        if (UNLIKELY(res >= sizeof(pathbuf))) {
            DLOG("Buffer overflow on path element %.*s in: %s",
                 thislen, path, path_orig);
            errno = ENAMETOOLONG;
            return 0;
        }
        if (access(pathbuf, F_OK) == 0) {
            pathlen = res;
            goto next_component;
        }
        pathbuf[pathlen] = '\0';

        /* Look for a matching directory entry. */
        DIR *dir = opendir(pathbuf);
        if (!dir) {
            return 0;
        }
        struct dirent *de;
        while ((de = readdir(dir)) != NULL) {
            if (strnicmp(path, de->d_name, thislen) == 0
             && de->d_name[thislen] == '\0') {
                break;
            }
        }
        if (!de) {
            closedir(dir);
            errno = ENOENT;
            return 0;
        }

        /* Append the found entry to the actual path buffer. */
        res = strformat(pathbuf+pathlen, sizeof(pathbuf)-pathlen,
                        "/%s", de->d_name);
        closedir(dir);
        pathlen += res;
        /* This can never exceed the buffer size, since we already did the
         * append using the same-length path component from the original
         * string. */
        ASSERT(pathlen < sizeof(pathbuf), errno = ENAMETOOLONG; return 0);

        /* Advance past the slash to the next path component. */
      next_component:
        if (*next) {
            next++;
        }
        path = next;

    }

    /* Skip over the initial "./" for relative paths (so the path fits in
     * a buffer the same size as the original path). */

  have_final_path:;
    const char *final_path = pathbuf;
    if (strncmp(final_path, "./", 2) == 0) {
        final_path += 2;
    }

    /* Send back the modified path.  The destination buffer is guaranteed
     * by precondition to be large enough for the path. */

    ASSERT(strformat_check(path_ret, path_ret_size, "%s", final_path),
           errno = ENAMETOOLONG; return 0);

    /* We now have the actual path, so perform the requested open action. */

    if (as_dir) {
        DIR *d = opendir(final_path);
        if (!d) {
            return 0;
        }
        *(DIR **)result_ret = d;
    } else {
        int fd = open(final_path, O_RDONLY);
        if (fd < 0) {
            return 0;
        }
        struct stat st;
        /* Note that fstat() should never fail under normal circumstances. */
        if (fstat(fd, &st) == 0 && S_ISDIR(st.st_mode)) {
            close(fd);
            errno = EISDIR;
            return 0;
        }
        *(int *)result_ret = fd;
    }
    return 1;
}

/*************************************************************************/
/*************************************************************************/
