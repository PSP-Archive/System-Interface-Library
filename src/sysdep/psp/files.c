/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/files.c: PSP data file access interface.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/psp/file-read.h"
#include "src/sysdep/psp/internal.h"
#include "src/sysdep/psp/thread.h"

/*
 * Notes regarding file access on the PSP:
 *
 * - The implementation as a whole is multithread-aware, and will operate
 *   correctly under simultaneous calls from separate threads.
 *
 * - It is _not_ permitted to simultaneously perform multiple operations on
 *   a single file handle.  However, a file handle may be opened by one
 *   thread and then read from by another, as long as the calls from each
 *   thread do not overlap.
 *
 * - This implementation does not perform any caching (and the PSP's OS
 *   likewise has no I/O cache), so there is a significant overhead to
 *   small-sized read operations.  In general, it is preferable to read an
 *   entire file into memory and process it from the memory buffer rather
 *   than read small chunks directly from the storage device.
 */

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Maximum number of files that can be open at once.  (File handles are
 * maintained in a static table to avoid the risk of failure due to
 * insufficient memory.)  Note that the PSP seems to have a kernel-level
 * limit of 64 simultaneous open files. */
#define MAX_FILES  64

/*-----------------------------------------------------------------------*/

/* File handle structure. */
struct SysFile {
    /* Is this file handle in use? (0 = unused) */
    uint8_t inuse;
    /* Is the opened object a directory? (0 = file) */
    uint8_t isdir;
    /* File descriptor used with sceIo*() system calls. */
    int fd;
    /* Current synchronous read position.  For directories, this instead
     * gives the number of entries read from the directory. */
    int64_t filepos;
    /* File size (discovered at open time). */
    int64_t filesize;
    /* File pathname (used in recovery from suspend mode). */
    char path[256];
};

/* Directory handle structure. */
struct SysDir {
    /* File handle containing directory descriptor for sceIo*() calls. */
    SysFile *dirfh;
    /* Return buffer for sceIoDread(). */
    struct SceIoDirent psp_dirent;
};

/* File handle table. */
static SysFile filetable[MAX_FILES];

/* Table of file handle mutexes (created at startup time).  Kept separate
 * from the file handle structure so they are not accidentally cleared
 * when opening or closing a file. */
static SceUID file_mutex[MAX_FILES];

/* Asynchronous operation table. */
struct {
    /* File handle for this operation (NULL = unused entry). */
    SysFile *fh;
    /* Type of operation.  Asynchronous reads are not supported in the
     * common sysdep interface, but are used by internal PSP code. */
    enum {ASYNC_OPEN, ASYNC_READ} type;
    /* Low-level (file-read.c) read request for ASYNC_READ.  fh!=NULL and
     * request==0 indicates that the low-level request completed and this
     * entry is awaiting a sys_file_wait_async() call. */
    int request;
    /* Result of low-level request for ASYNC_READ. */
    int32_t res;
} async_info[MAX_ASYNC_READS];

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * lock_file:  Lock the mutex for the given file handle, waiting as necessary.
 *
 * [Parameters]
 *     fh: File handle to lock.
 */
static void lock_file(const SysFile *fp);

/**
 * unlock_file:  Unlock the mutex for the given file handle.
 *
 * [Parameters]
 *     fh: File handle to lock.
 */
static void unlock_file(const SysFile *fp);

/**
 * alloc_file:  Allocate an unused file handle.  If the function succeeds,
 * the returned file handle's mutex will be locked.
 *
 * [Return value]
 *     Allocated file handle, or NULL on error.
 */
static SysFile *alloc_file(void);

/**
 * alloc_async:  Allocate an entry from the async_info[] table and
 * associate it with the given file handle.
 *
 * [Parameters]
 *     fh: File handle to associate with async_info[] entry.
 * [Return value]
 *     Index of allocated entry in async_info[] table, or a negative value
 *     on error.
 */
static int alloc_async(SysFile *fp);

/**
 * free_async:  Free an async_info[] table entry.
 *
 * [Parameters]
 *     index: Index in async_info[] of entry to free.
 */
static void free_async(int index);

/**
 * check_async_request:  Check the status of an asynchronous open or read
 * operation (first waiting for completion if "wait" is true).  If the
 * operation has completed, store the result in async_info[index].res.
 *
 * [Parameters]
 *     index: Index in async_info[] of entry to check.
 *     wait: True to wait for the operation to complete.
 * [Return value]
 *     True if the operation has completed, false if not.
 */
static int check_async_request(int index, int wait);

/*************************************************************************/
/******************* Interface: Initialization/cleanup *******************/
/*************************************************************************/

int sys_file_init(void)
{
    for (int i = 0; i < lenof(filetable); i++) {
        char namebuf[32];
        strformat(namebuf, sizeof(namebuf), "File%dMutex", i);
        file_mutex[i] = sceKernelCreateSema(namebuf, 0, 1, 1, NULL);
        if (UNLIKELY(file_mutex[i] < 0)) {
            DLOG("Failed to create file %d mutex: %s",
                 i, psp_strerror(file_mutex[i]));
            while (--i >= 0) {
                sceKernelDeleteSema(file_mutex[i]);
                file_mutex[i] = 0;
            }
            return 0;
        }
    }

    if (UNLIKELY(!psp_file_read_init())) {
        DLOG("Failed to initialize file read thread");
        return 0;
    }

    mem_clear(async_info, sizeof(async_info));
    return 1;
}

/*-----------------------------------------------------------------------*/

void sys_file_cleanup(void)
{
    psp_file_read_cleanup();

    for (int i = 0; i < lenof(filetable); i++) {
        if (UNLIKELY(filetable[i].inuse)) {
            DLOG("WARNING: file %d (%s) still open at cleanup",
                 i, filetable[i].path);
            filetable[i].inuse = 0;
        }
        sceKernelDeleteSema(file_mutex[i]);
        file_mutex[i] = 0;
    }
}

/*************************************************************************/
/********************** Interface: File operations ***********************/
/*************************************************************************/

SysFile *sys_file_open(const char *path)
{
    if (UNLIKELY(!path)) {
        DLOG("path == NULL");
        psp_errno = PSP_EINVAL;
        goto error_return;
    }
    if (UNLIKELY(!*path)) {
        DLOG("path is empty");
        psp_errno = PSP_ENOENT;
        goto error_return;
    }

    /* Find a free file handle and allocate it. */
    SysFile *fh = alloc_file();
    if (UNLIKELY(!fh)) {
        psp_errno = PSP_EMFILE;
        goto error_return;
    }
    mem_clear(fh, sizeof(*fh));
    fh->inuse = 1;

    /* Generate the full pathname, making sure it fits in the path buffer. */
    unsigned int len;
    if (strchr(path, ':')) {
        len = strformat(fh->path, sizeof(fh->path), "%s", path);
    } else {
        len = strformat(fh->path, sizeof(fh->path), "%s/%s",
                        psp_executable_dir(), path);
    }
    if (UNLIKELY(len >= sizeof(fh->path))) {
        psp_errno = PSP_ENAMETOOLONG;
        goto error_unlock;
    }

    /* Open the physical file. */
    int fd = sceIoOpen(fh->path, PSP_O_RDONLY, 0);
    if (UNLIKELY(fd < 0)) {
        psp_errno = fd;
        if (psp_errno == PSP_ENOENT) {
            /* ENOENT may actually be EISDIR, so check explicitly. */
            SceIoStat st;
            if (sceIoGetstat(fh->path, &st) == 0 && FIO_S_ISDIR(st.st_mode)) {
                psp_errno = PSP_EISDIR;
            }
        }
        goto error_unlock;
    }

    /* Set up and return the file handle. */
    fh->isdir    = 0;
    fh->fd       = fd;
    fh->filepos  = 0;
    fh->filesize = sceIoLseek(fd, 0, PSP_SEEK_END);
    if (UNLIKELY(fh->filesize < 0)) {
        DLOG("Error getting file size for %s: %s",
             path, psp_strerror(fh->filesize));
        fh->filesize = 0;
    }

    unlock_file(fh);
    return fh;

  error_unlock:
    fh->inuse = 0;
    unlock_file(fh);
  error_return:
    return NULL;
}

/*-----------------------------------------------------------------------*/

SysFile *sys_file_dup(SysFile *fh)
{
    if (UNLIKELY(!fh)) {
        DLOG("fh == NULL");
        psp_errno = PSP_EINVAL;
        return NULL;
    }

    SysFile *newfh = alloc_file();
    if (UNLIKELY(!newfh)) {
        psp_errno = PSP_EMFILE;
        return NULL;
    }

    lock_file(fh);
    *newfh = *fh;
    unlock_file(fh);

    int newfd = sceIoOpen(newfh->path, PSP_O_RDONLY, 0);
    if (newfd < 0) {
        DLOG("Failed to reopen %s: %s", fh->path, psp_strerror(newfd));
        psp_errno = newfd;
        newfh->inuse = 0;
        unlock_file(newfh);
        return NULL;
    }
    newfh->fd = newfd;

    unlock_file(newfh);
    return newfh;
}

/*-----------------------------------------------------------------------*/

void sys_file_close(SysFile *fh)
{
    if (fh) {
        lock_file(fh);
        for (int i = 0; i < lenof(async_info); i++) {
            if (async_info[i].fh == fh) {
                if (async_info[i].request) {
                    check_async_request(i, 1);
                }
                async_info[i].res = PSP_ECANCELED;
            }
        }
        sceIoClose(fh->fd);
        fh->inuse = 0;
        unlock_file(fh);
    }
}

/*-----------------------------------------------------------------------*/

int64_t sys_file_size(SysFile *fh)
{
    if (UNLIKELY(!fh)) {
        DLOG("fh == NULL");
        psp_errno = PSP_EINVAL;
        return 0;
    }

    return fh->filesize;
}

/*-----------------------------------------------------------------------*/

int sys_file_seek(SysFile *fh, int64_t pos, int how)
{
    if (UNLIKELY(!fh)) {
        DLOG("fh == NULL");
        psp_errno = PSP_EINVAL;
        goto error_return;
    }
    lock_file(fh);

    if (how == FILE_SEEK_SET) {
        fh->filepos = pos;
    } else if (how == FILE_SEEK_CUR) {
        fh->filepos += pos;
    } else if (how == FILE_SEEK_END) {
        fh->filepos = fh->filesize + pos;
    } else {
        DLOG("Invalid how: %d", how);
        psp_errno = PSP_EINVAL;
        goto error_unlock_file;
    }

    if (fh->filepos < 0) {
        fh->filepos = 0;
    } else if (fh->filepos > fh->filesize) {
        fh->filepos = fh->filesize;
    }

    unlock_file(fh);
    return 1;

  error_unlock_file:
    unlock_file(fh);
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

int64_t sys_file_tell(SysFile *fh)
{
    if (UNLIKELY(!fh)) {
        DLOG("fh == NULL");
        psp_errno = PSP_EINVAL;
        return 0;
    }

    return fh->filepos;
}

/*-----------------------------------------------------------------------*/

int32_t sys_file_read(SysFile *fh, void *buf, int32_t len)
{
    if (UNLIKELY(!fh) || UNLIKELY(!buf) || UNLIKELY(len < 0)) {
        DLOG("Invalid parameters: %p %p %d", fh, buf, len);
        psp_errno = PSP_EINVAL;
        goto error_return;
    }
    lock_file(fh);

    if (len == 0) {
        unlock_file(fh);
        return 0;
    }

    int request = psp_file_read_submit(fh->fd, fh->filepos, len, buf, 0, 0);
    if (UNLIKELY(!request)) {
        DLOG("(%d,%p,%d): Read request submission failed", fh->fd, buf, len);
        psp_errno = PSP_EIO;
        goto error_unlock_file;
    }
    int32_t res = psp_file_read_wait(request);
    if (UNLIKELY(res < 0)) {
        DLOG("Read request failed");
        psp_errno = res;
        goto error_unlock_file;
    }

    fh->filepos += res;
    unlock_file(fh);
    return res;

  error_unlock_file:
    unlock_file(fh);
  error_return:
    return -1;
}

/*-----------------------------------------------------------------------*/

int32_t sys_file_read_at(SysFile *fh, void *buf, int32_t len, int64_t filepos)
{
    if (UNLIKELY(!fh)
     || UNLIKELY(!buf)
     || UNLIKELY(len < 0)
     || UNLIKELY(filepos < 0)) {
        DLOG("Invalid parameters: %p %p %d %lld", fh, buf, len, filepos);
        psp_errno = PSP_EINVAL;
        goto error_return;
    }
    lock_file(fh);

    if (len == 0) {
        unlock_file(fh);
        return 0;
    }

    int request = psp_file_read_submit(fh->fd, filepos, len, buf, 0, 0);
    if (UNLIKELY(!request)) {
        DLOG("(%d,%p,%d): Read request submission failed", fh->fd, buf, len);
        psp_errno = PSP_EIO;
        goto error_unlock_file;
    }
    int32_t res = psp_file_read_wait(request);
    if (UNLIKELY(res < 0)) {
        DLOG("Read request failed");
        psp_errno = res;
        goto error_unlock_file;
    }

    unlock_file(fh);
    return res;

  error_unlock_file:
    unlock_file(fh);
  error_return:
    return -1;
}

/*-----------------------------------------------------------------------*/

int sys_file_read_async(SysFile *fh, void *buf, int32_t len,
                        int64_t filepos, float deadline)
{
    if (UNLIKELY(!fh)
     || UNLIKELY(!buf)
     || UNLIKELY(len < 0)
     || UNLIKELY(filepos < 0)) {
        DLOG("Invalid parameters: %p %p %d %lld %g", fh, buf, len, filepos,
             deadline);
        psp_errno = PSP_EINVAL;
        goto error_return;
    }
    lock_file(fh);

    const int timed = (deadline >= 0 && deadline < 2147);
    int time_limit;
    if (timed) {
        /* Determine when the read operation needs to start, assuming the
         * data transfer rate of the UMD drive as a reasonable lower bound
         * on access speed.  We don't worry about seek delays since it
         * seems unlikely this library will ever be used for a disc-based
         * PSP game and since dealing properly with seeks requires an
         * entirely separate set of scheduling logic. */
        deadline = lbound(deadline - (len / 1375000.0f), 0.0f);
        time_limit = ifloorf(deadline * 1000000);
    } else {
        time_limit = 0;
    }

    const int index = alloc_async(fh);
    if (UNLIKELY(index < 0)) {
        psp_errno = PSP_ENOEXEC;  // Like sysdep/posix/files.c.
        goto error_unlock_file;
    }

    async_info[index].type = ASYNC_READ;
    async_info[index].request =
        psp_file_read_submit(fh->fd, filepos, len, buf, timed, time_limit);
    if (UNLIKELY(!async_info[index].request)) {
        DLOG("(%d,%p,%d): Read request submission failed", fh->fd, buf, len);
        psp_errno = PSP_EIO;
        goto error_free_async;
    }
    async_info[index].res = -1;

    unlock_file(fh);
    return index+1;

  error_free_async:
    free_async(index);
  error_unlock_file:
    unlock_file(fh);
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

int sys_file_poll_async(int request)
{
    if (UNLIKELY(request <= 0 || request > lenof(async_info))) {
        DLOG("Request %d out of range", request);
        psp_errno = SCE_KERNEL_ERROR_NOASYNC;
        return 1;
    }

    const int index = request-1;
    if (UNLIKELY(!async_info[index].fh)) {
        psp_errno = SCE_KERNEL_ERROR_NOASYNC;
        return 1;
    }
    if (!async_info[index].request) {  // Already completed.
        return 1;
    }
    return check_async_request(index, 0);
}

/*-----------------------------------------------------------------------*/

int32_t sys_file_wait_async(int request)
{
    if (UNLIKELY(request <= 0 || request > lenof(async_info))) {
        DLOG("Request %d out of range", request);
        psp_errno = SCE_KERNEL_ERROR_NOASYNC;
        return -1;
    }

    const int index = request-1;
    if (UNLIKELY(!async_info[index].fh)) {
        psp_errno = SCE_KERNEL_ERROR_NOASYNC;
        return -1;
    }
    SysFile *fh = async_info[index].fh;
    lock_file(fh);

    if (async_info[index].request) {
        /* Still in progress, so wait for it. */
        check_async_request(index, 1);
    }

    int32_t retval;
    if (UNLIKELY(async_info[index].res < 0)) {
        psp_errno = async_info[index].res;
        if (async_info[index].type == ASYNC_OPEN) {
            sceIoClose(fh->fd);
            fh->inuse = 0;
            retval = 0;
        } else {
            retval = -1;
        }
    } else {
        if (async_info[index].type == ASYNC_OPEN) {
            fh->filepos  = 0;
            fh->filesize = sceIoLseek(fh->fd, 0, PSP_SEEK_END);
            if (UNLIKELY(fh->filesize < 0)) {
                DLOG("Error getting file size for %s: %s",
                     fh->path, psp_strerror(fh->filesize));
                fh->filesize = 0;
            }
            retval = 1;
        } else {
            retval = async_info[index].res;
        }
    }
    free_async(index);
    unlock_file(fh);
    return retval;
}

/*-----------------------------------------------------------------------*/

int sys_file_abort_async(int request)
{
    if (UNLIKELY(request <= 0 || request > lenof(async_info))) {
        DLOG("Request %d out of range", request);
        psp_errno = SCE_KERNEL_ERROR_NOASYNC;
        return 0;
    }

    const int index = request-1;
    if (!async_info[index].fh) {
        psp_errno = SCE_KERNEL_ERROR_NOASYNC;
        return 0;
    }
    psp_file_read_abort(async_info[index].request);
    return 1;
}

/*************************************************************************/
/******************** Interface: Directory operations ********************/
/*************************************************************************/

SysDir *sys_dir_open(const char *path)
{
    if (UNLIKELY(!path)) {
        DLOG("path == NULL");
        psp_errno = PSP_EINVAL;
        goto error_return;
    }
    if (UNLIKELY(!*path)) {
        DLOG("path is empty");
        psp_errno = PSP_ENOENT;
        goto error_return;
    }

    /* Find a free file handle and allocate it. */
    SysFile *fh = alloc_file();
    if (UNLIKELY(!fh)) {
        psp_errno = PSP_EMFILE;
        goto error_return;
    }
    mem_clear(fh, sizeof(*fh));
    fh->inuse = 1;

    /* Generate the full pathname, making sure it fits in the path buffer. */
    int ok;
    if (strchr(path, ':')) {
        ok = strformat_check(fh->path, sizeof(fh->path), "%s", path);
    } else {
        ok = strformat_check(fh->path, sizeof(fh->path), "%s/%s",
                             psp_executable_dir(), path);
    }
    if (UNLIKELY(!ok)) {
        psp_errno = PSP_ENAMETOOLONG;
        goto error_unlock;
    }

    int len = strlen(fh->path);
    ASSERT(len > 0);

    /* Convert a trailing "/." in the pathname to just "/", since trying
     * to open "." fails on old firmware versions. */
    if (len >= 2 && strcmp(&fh->path[len-2], "/.") == 0) {
        fh->path[len-1] = 0;
        len--;
    }

    /* Remove any trailing slash from the pathname. */
    if (fh->path[len-1] == '/') {
        fh->path[len-1] = 0;
        len--;
    }

    /* Old firmware versions will happily open a file as a directory, so
     * explicitly check whether the object is a directory before proceeding. */
    SceIoStat st;
    if (sceIoGetstat(fh->path, &st) == 0 && !FIO_S_ISDIR(st.st_mode)) {
        psp_errno = PSP_ENOTDIR;
        goto error_unlock;
    }

    /* Allocate a SysDir object for returning to the caller. */
    SysDir *dir = mem_alloc(sizeof(*dir), 0, MEM_ALLOC_TEMP | MEM_ALLOC_CLEAR);
    if (UNLIKELY(!dir)) {
        DLOG("No memory for SysDir structure");
        psp_errno = PSP_ENOMEM;
        goto error_unlock;
    }

    /* Actually open the directory. */
    fh->fd = sceIoDopen(fh->path);
    if (fh->fd < 0) {
        psp_errno = fh->fd;
        goto error_free_dir;
    }

    /* Set up structures and return. */
    fh->isdir = 1;
    fh->filepos = 0;
    unlock_file(fh);
    dir->dirfh = fh;
    return dir;

  error_free_dir:
    mem_free(dir);
  error_unlock:
    fh->inuse = 0;
    unlock_file(fh);
  error_return:
    return NULL;
}

/*-----------------------------------------------------------------------*/

const char *sys_dir_read(SysDir *dir, int *is_subdir_ret)
{
    if (UNLIKELY(!dir) || UNLIKELY(!is_subdir_ret)) {
        DLOG("Invalid parameters: %p %p", dir, is_subdir_ret);
        psp_errno = PSP_EINVAL;
        return NULL;
    }
    ASSERT(dir->dirfh != NULL, return NULL);

    int res;
    while ((res = sceIoDread(dir->dirfh->fd, &dir->psp_dirent)) > 0) {
        dir->dirfh->filepos++;
        if (strncmp(dir->psp_dirent.d_name, "..",
                    strlen(dir->psp_dirent.d_name)) == 0) {
            continue;
        }
        if (FIO_S_ISREG(dir->psp_dirent.d_stat.st_mode)
         || FIO_S_ISDIR(dir->psp_dirent.d_stat.st_mode)) {
            break;
        }
    }
    if (res <= 0) {
        return NULL;
    }

    *is_subdir_ret = FIO_S_ISDIR(dir->psp_dirent.d_stat.st_mode);
    return dir->psp_dirent.d_name;
}

/*-----------------------------------------------------------------------*/

void sys_dir_close(SysDir *dir)
{
    if (dir) {
        ASSERT(dir->dirfh != NULL, mem_free(dir); return);
        lock_file(dir->dirfh);
        sceIoDclose(dir->dirfh->fd);
        dir->dirfh->inuse = 0;
        unlock_file(dir->dirfh);
        mem_free(dir);
    }
}

/*************************************************************************/
/************************* Internal-use routines *************************/
/*************************************************************************/

int psp_file_open_async(const char *path, SysFile **fh_ret)
{
    if (UNLIKELY(!path)) {
        DLOG("path == NULL");
        psp_errno = PSP_EINVAL;
        return 0;
    }
    if (UNLIKELY(!fh_ret)) {
        DLOG("fh_ret == NULL");
        psp_errno = PSP_EINVAL;
        return 0;
    }

    /* Find a free file handle and allocate it. */
    SysFile *fh = alloc_file();
    if (UNLIKELY(!fh)) {
        psp_errno = PSP_EMFILE;
        return 0;
    }
    mem_clear(fh, sizeof(*fh));
    fh->inuse = 1;

    /* Generate the full pathname, making sure it fits in the path buffer. */
    unsigned int len;
    if (strchr(path, ':')) {
        len = strformat(fh->path, sizeof(fh->path), "%s", path);
    } else {
        len = strformat(fh->path, sizeof(fh->path), "%s/%s",
                        psp_executable_dir(), path);
    }
    if (UNLIKELY(len >= sizeof(fh->path))) {
        psp_errno = PSP_ENAMETOOLONG;
        goto error_unlock;
    }

    /* Allocate an asynchronous operation slot. */
    const int req_index = alloc_async(fh);
    if (UNLIKELY(req_index < 0)) {
        DLOG("No free async blocks");
        psp_errno = PSP_ENOEXEC;  // Like sys_file_read_async().
        goto error_unlock;
    }
    async_info[req_index].type = ASYNC_OPEN;
    async_info[req_index].request = 1;  // Signal that it's in progress.
    const int request = req_index + 1;

    /* Start the open operation. */
    int fd = sceIoOpenAsync(fh->path, PSP_O_RDONLY, 0);
    if (UNLIKELY(fd < 0)) {
        psp_errno = fd;
        async_info[req_index].fh = NULL;
        goto error_unlock;
    }
    fh->fd = fd;

    /* Return the file handle and asynchronous request ID. */
    *fh_ret = fh;
    unlock_file(fh);
    return request;

  error_unlock:
    fh->inuse = 0;
    unlock_file(fh);
    return 0;
}

/*-----------------------------------------------------------------------*/

void psp_file_pause(void)
{
    for (int i = 0; i < lenof(async_info); i++) {
        if (async_info[i].fh && async_info[i].request) {
            check_async_request(i, 1);
        }
    }
    for (int i = 0; i < lenof(filetable); i++) {
        lock_file(&filetable[i]);
        if (filetable[i].inuse) {
            if (filetable[i].isdir) {
                sceIoDclose(filetable[i].fd);
            } else {
                sceIoClose(filetable[i].fd);
            }
            filetable[i].fd = -1;
        }
    }
}

/*-----------------------------------------------------------------------*/

void psp_file_unpause(void)
{
#ifdef DEBUG
    if (strncmp(psp_executable_dir(), "host", 4) == 0) {
        /* Wait for PSPlink's USB connection to recover. */
        sceKernelDelayThread(250000);
    }
#endif

    for (int i = 0; i < lenof(filetable); i++) {
        if (filetable[i].inuse) {
            if (filetable[i].isdir) {
                filetable[i].fd = sceIoDopen(filetable[i].path);
            } else {
                filetable[i].fd = sceIoOpen(filetable[i].path, PSP_O_RDONLY, 0);
            }
            if (UNLIKELY(filetable[i].fd < 0)) {
                DLOG("Unable to reopen %s: %s",
                     filetable[i].path, psp_strerror(filetable[i].fd));
                filetable[i].fd = -1;
            }
            if (filetable[i].isdir) {
                for (int64_t j = 0; j < filetable[i].filepos; j++) {
                    SceIoDirent dirent;
                    (void) sceIoDread(filetable[i].fd, &dirent);
                }
            }
        }
        unlock_file(&filetable[i]);
    }
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static void lock_file(const SysFile *fh)
{
    PRECOND(fh != NULL, return);
    const int index = fh - filetable;
    ASSERT(index >= 0 && index < lenof(filetable), return);
    ASSERT(fh == &filetable[index], return);
    sceKernelWaitSema(file_mutex[index], 1, NULL);
}

/*-----------------------------------------------------------------------*/

static void unlock_file(const SysFile *fh)
{
    PRECOND(fh != NULL, return);
    const int index = fh - filetable;
    ASSERT(index >= 0 && index < lenof(filetable), return);
    ASSERT(fh == &filetable[index], return);
    sceKernelSignalSema(file_mutex[index], 1);
}

/*-----------------------------------------------------------------------*/

static SysFile *alloc_file(void)
{
    unsigned int i;
    for (i = 0; i < lenof(filetable); i++) {
        /* If the file handle is already in use, don't try to lock it
         * (because it may be locked for an extended period of time,
         * e.g. for a synchronous read). */
        if (!filetable[i].inuse) {
            lock_file(&filetable[i]);
            /* Now that we've locked the handle, check that it's still
             * unused -- another thread may be calling this function
             * at the same time (race condition). */
            if (LIKELY(!filetable[i].inuse)) {
                break;
            } else {
                /* We lost the race, so unlock the handle and try the
                 * next one. */
                unlock_file(&filetable[i]);
            }
        }
    }
    if (UNLIKELY(i >= lenof(filetable))) {
        return NULL;
    }
    return &filetable[i];
}

/*-----------------------------------------------------------------------*/

static int alloc_async(SysFile *fh)
{
    int index;

    psp_threads_lock();
    for (index = 0; index < lenof(async_info); index++) {
        if (!async_info[index].fh) {
            async_info[index].fh = fh;
            break;
        }
    }
    psp_threads_unlock();
    return (index < lenof(async_info)) ? index : -1;
}

/*-----------------------------------------------------------------------*/

static void free_async(int index)
{
    PRECOND(index >= 0 && index < lenof(async_info), return);
    psp_threads_lock();
    mem_clear(&async_info[index], sizeof(async_info[index]));
    psp_threads_unlock();
}

/*-----------------------------------------------------------------------*/

static int check_async_request(int index, int wait)
{
    PRECOND(index >= 0 && index < lenof(async_info), return 1);
    PRECOND(async_info[index].request != 0, return 1);

    if (async_info[index].type == ASYNC_OPEN) {
        int64_t res = 0;
        int err;
        if (wait) {
            err = sceIoWaitAsync(async_info[index].fh->fd, &res);
        } else {
            err = sceIoPollAsync(async_info[index].fh->fd, &res);
            if (err > 0) {  // Still in progress.
                return 0;
            }
        }
        if (UNLIKELY(err < 0)) {
            async_info[index].res = err;
        } else {
            async_info[index].res = (int32_t)res;
        }
    } else {
        if (!wait && !psp_file_read_check(async_info[index].request)) {
            return 0;
        }
        async_info[index].res = psp_file_read_wait(async_info[index].request);
    }
    async_info[index].request = 0;
    return 1;
}

/*************************************************************************/
/*************************************************************************/
