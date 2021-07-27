/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/windows/files.c: Data file access interface for Windows.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/misc/ioqueue.h"
#include "src/sysdep/windows/internal.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Estimated read speed (low bound), for setting ioqueue deadlines. */
#define ESTIMATED_READ_SPEED  10000000  // 10MB/sec

/* File handle structure. */
struct SysFile {
    HANDLE handle;     // System file handle.
    int64_t filesize;  // File size (discovered at open time).
    int64_t filepos;   // Current synchronous read position.
};

/* Directory handle structure. */
struct SysDir {
    HANDLE find_handle;         // Handle for FindNextFile().
    WIN32_FIND_DATA find_data;  // Data buffer for Find{First,Next}File().

    /* Flag set if we haven't yet returned the first entry.  This is
     * needed because Windows encapsulates the start-search and return-
     * first-match functions into a single system call. */
    uint8_t is_first;
};

/*-----------------------------------------------------------------------*/

/* Asynchronous operation data. */
typedef struct AsyncInfo {
    SysFile *fh;      // File handle for this operation (NULL = unused entry).
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
static SysMutexID async_info_mutex = 0;

/*************************************************************************/
/******************* Interface: Initialization/cleanup *******************/
/*************************************************************************/

int sys_file_init(void)
{
    async_info_mutex = sys_mutex_create(0, 0);
    if (!async_info_mutex) {
        DLOG("Failed to create mutex for async info block: %s",
             sys_last_errstr());
        goto error_return;
    }

    if (!ioq_init()) {
        DLOG("ioq_init() failed");
        goto error_destroy_async_info_mutex;
    }

    return 1;

  error_destroy_async_info_mutex:
    sys_mutex_destroy(async_info_mutex);
    async_info_mutex = 0;
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

void sys_file_cleanup(void)
{
    ioq_reset();
    sys_mutex_destroy(async_info_mutex);
    async_info_mutex = 0;
}

/*************************************************************************/
/********************** Interface: File operations ***********************/
/*************************************************************************/

SysFile *sys_file_open(const char *path)
{
    if (UNLIKELY(!path)) {
        DLOG("path == NULL");
        windows_set_error(SYSERR_INVALID_PARAMETER, 0);
        goto error_return;
    }
    if (UNLIKELY(!*path)) {
        DLOG("path is empty");
        windows_set_error(SYSERR_FILE_NOT_FOUND, 0);
        goto error_return;
    }

    SysFile *fh = mem_alloc(sizeof(*fh), 0, MEM_ALLOC_TEMP);
    if (UNLIKELY(!fh)) {
        windows_set_error(SYSERR_OUT_OF_MEMORY, 0);
        goto error_return;
    }

    /* Convert the pathname to Windows format. */
    char pathbuf[4096];
    int pathlen = 0;
    *pathbuf = 0;
    const char *s = path;
    while (*s) {
        const unsigned int partlen = strcspn(s, "/");
        const unsigned int i =
            strformat(pathbuf+pathlen, sizeof(pathbuf)-pathlen,
                      "%.*s%s", partlen, s, s[partlen]=='/' ? "\\" : "");
        if (UNLIKELY(pathlen+i >= sizeof(pathbuf))) {
            DLOG("Path buffer overflow on path %s", path);
            windows_set_error(SYSERR_BUFFER_OVERFLOW, 0);
            goto error_free_fh;
        }
        pathlen += i;
        s += partlen;
        s += strspn(s, "/");
    }

    fh->handle = CreateFile(pathbuf, GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, 0, NULL);
    if (fh->handle == INVALID_HANDLE_VALUE) {
        windows_set_error(0, 0);
        if (GetLastError() == ERROR_ACCESS_DENIED) {
            /* Windows returns "access denied" if you try to open a
             * directory as a file.  Return a more useful error code
             * in that case. */
            const DWORD attr = GetFileAttributes(pathbuf);
            if (attr != INVALID_FILE_ATTRIBUTES
             && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
                windows_set_error(SYSERR_FILE_WRONG_TYPE, 0);
            }
        }
        goto error_free_fh;
    }

    DWORD filesize = GetFileSize(fh->handle, NULL);
    if (UNLIKELY(filesize == INVALID_FILE_SIZE)) {
        DLOG("Failed to get file size for %s", pathbuf);
        windows_set_error(0, 0);
        goto error_close_handle;
    }
    fh->filesize = filesize;

    fh->filepos = 0;

    return fh;

  error_close_handle:
    CloseHandle(fh->handle);
  error_free_fh:
    mem_free(fh);
  error_return:
    return NULL;
}

/*-----------------------------------------------------------------------*/

SysFile *sys_file_dup(SysFile *fh)
{
    if (UNLIKELY(!fh)) {
        DLOG("fh == NULL");
        windows_set_error(SYSERR_INVALID_PARAMETER, 0);
        return NULL;
    }

    SysFile *newfh = mem_alloc(sizeof(*newfh), 0, MEM_ALLOC_TEMP);
    if (UNLIKELY(!newfh)) {
        windows_set_error(SYSERR_OUT_OF_MEMORY, 0);
        return NULL;
    }

    int res = DuplicateHandle(GetCurrentProcess(), fh->handle,
                              GetCurrentProcess(), &newfh->handle,
                              0, FALSE, DUPLICATE_SAME_ACCESS);
    if (UNLIKELY(!res)) {
        windows_set_error(0, 0);
        mem_free(newfh);
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

    CloseHandle(fh->handle);
    mem_free(fh);
}

/*-----------------------------------------------------------------------*/

int64_t sys_file_size(SysFile *fh)
{
    if (UNLIKELY(!fh)) {
        DLOG("fh == NULL");
        windows_set_error(SYSERR_INVALID_PARAMETER, 0);
        return 0;
    }

    return fh->filesize;
}

/*-----------------------------------------------------------------------*/

int sys_file_seek(SysFile *fh, int64_t pos, int how)
{
    if (UNLIKELY(!fh)) {
        DLOG("fh == NULL");
        windows_set_error(SYSERR_INVALID_PARAMETER, 0);
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
        windows_set_error(SYSERR_INVALID_PARAMETER, 0);
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
        windows_set_error(SYSERR_INVALID_PARAMETER, 0);
        return 0;
    }

    return fh->filepos;
}

/*-----------------------------------------------------------------------*/

int32_t sys_file_read(SysFile *fh, void *buf, int32_t len)
{
    if (UNLIKELY(!fh) || UNLIKELY(!buf) || UNLIKELY(len < 0)) {
        DLOG("Invalid parameters: %p %p %d", fh, buf, len);
        windows_set_error(SYSERR_INVALID_PARAMETER, 0);
        return -1;
    }

    if (len == 0) {
        return 0;
    }

    int32_t nread = sys_file_read_at(fh, buf, len, fh->filepos);
    if (nread < 0) {
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
        windows_set_error(SYSERR_INVALID_PARAMETER, 0);
        return -1;
    }

    if (len == 0) {
        return 0;
    }

    DWORD nread = 0;
    OVERLAPPED overlapped;
    mem_clear(&overlapped, sizeof(overlapped));
    overlapped.Offset = (DWORD)(filepos & 0xFFFFFFFFU);
    overlapped.OffsetHigh = (DWORD)(filepos >> 32);
    int ok = ReadFile(fh->handle, buf, len, &nread, &overlapped);
    if (!ok && GetLastError() != ERROR_HANDLE_EOF) {
        windows_set_error(0, 0);
        return -1;
    }

    return (int32_t)nread;
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
        windows_set_error(SYSERR_INVALID_PARAMETER, 0);
        return 0;
    }

    int64_t deadline_ns;
    if (deadline >= 0) {
        deadline -= len / ESTIMATED_READ_SPEED;
        if (deadline < 0) {
            deadline = 0;
        }
        deadline_ns = (int64_t)deadline; // Overflows at ~292 years, thus safe.
    } else {
        deadline_ns = -1;
    }

    sys_mutex_lock(async_info_mutex, -1);

    int index;
    for (index = 0; index < async_info_size; index++) {
        if (!async_info[index].fh) {
            break;
        }
    }
    if (index >= async_info_size) {
        sys_mutex_unlock(async_info_mutex);
        windows_set_error(SYSERR_FILE_ASYNC_FULL, 0);
        return 0;
    }
    async_info[index].fh = fh;
    async_info[index].aborted = 0;

    sys_mutex_unlock(async_info_mutex);

    errno = 0;
    async_info[index].ioqueue_request =
        ioq_read(fh->handle, buf, len, filepos, deadline_ns);
    if (UNLIKELY(async_info[index].ioqueue_request == 0)) {
        if (errno == 0) {
            windows_set_error(0, 0);
        } else if (errno == EAGAIN) {
            windows_set_error(SYSERR_TRANSIENT_FAILURE, 0);
        } else {
            windows_set_error(SYSERR_UNKNOWN_ERROR, 0);
        }
        async_info[index].fh = NULL;
        return 0;
    }

    return index+1;
}

/*-----------------------------------------------------------------------*/

int sys_file_poll_async(int request)
{
    if (UNLIKELY(request <= 0 || request > async_info_size)) {
        DLOG("Request %d out of range", request);
        windows_set_error(SYSERR_FILE_ASYNC_INVALID, 0);
        return 1;
    }

    const unsigned int index = request-1;
    if (UNLIKELY(!async_info[index].fh)) {
        windows_set_error(SYSERR_FILE_ASYNC_INVALID, 0);
        return 1;
    }

    return ioq_poll(async_info[index].ioqueue_request);
}

/*-----------------------------------------------------------------------*/

int32_t sys_file_wait_async(int request)
{
    if (UNLIKELY(request <= 0 || request > async_info_size)) {
        DLOG("Request %d out of range", request);
        windows_set_error(SYSERR_FILE_ASYNC_INVALID, 0);
        return -1;
    }

    const unsigned int index = request-1;
    if (UNLIKELY(!async_info[index].fh)) {
        windows_set_error(SYSERR_FILE_ASYNC_INVALID, 0);
        return -1;
    }

    int error;
    int32_t retval = ioq_wait(async_info[index].ioqueue_request, &error);
    if (async_info[index].aborted) {
        retval = -1;
        windows_set_error(SYSERR_FILE_ASYNC_ABORTED, 0);
    } else if (retval < 0) {
        windows_set_error(0, 0);
    }

    async_info[index].fh = NULL;
    return retval;
}

/*-----------------------------------------------------------------------*/

int sys_file_abort_async(int request)
{
    if (UNLIKELY(request <= 0 || request > async_info_size)) {
        DLOG("Request %d out of range", request);
        windows_set_error(SYSERR_FILE_ASYNC_INVALID, 0);
        return 0;
    }

    const int index = request-1;
    if (!async_info[index].fh) {
        windows_set_error(SYSERR_FILE_ASYNC_INVALID, 0);
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
    if (UNLIKELY(!path)) {
        DLOG("path == NULL");
        windows_set_error(SYSERR_INVALID_PARAMETER, 0);
        goto error_return;
    }
    if (UNLIKELY(!*path)) {
        DLOG("path is empty");
        windows_set_error(SYSERR_FILE_NOT_FOUND, 0);
        goto error_return;
    }

    SysDir *dir = mem_alloc(sizeof(*dir), 0, MEM_ALLOC_TEMP);
    if (UNLIKELY(!dir)) {
        DLOG("No memory for SysDir structure");
        windows_set_error(SYSERR_OUT_OF_MEMORY, 0);
        goto error_return;
    }

    /* Sanitize the path name, and convert to a Windows file pattern. */
    static const char suffix[] = "\\*.*";
    const int pattern_bufsize = strlen(path) + sizeof(suffix);
    char *pattern = mem_alloc(pattern_bufsize, 1, MEM_ALLOC_TEMP);
    if (UNLIKELY(!pattern)) {
        DLOG("No memory for path copy: %s", path);
        windows_set_error(SYSERR_OUT_OF_MEMORY, 0);
        goto error_free_dir;
    }
    strcpy(pattern, path);  // Safe.
    char *s = pattern + strcspn(pattern, "/");
    while (*s) {
        *s++ = '\\';
        char *slash_end = s + strspn(s, "/");
        if (slash_end > s) {
            memmove(s, slash_end, strlen(slash_end) + 1);
        }
        s += strcspn(s, "/");
    }
    /* The pattern can never be empty here -- it either contains one or
     * more backslashes or is a nonempty path component ("." if the
     * original path was empty). */
    ASSERT(s > pattern, goto error_free_dir);
    if (s[-1] == '\\') {
        *--s = 0;
    }
    /* We never increase the length of the string, so it will always fit
     * within our buffer. */
    ASSERT((s+sizeof(suffix))-pattern <= pattern_bufsize, goto error_free_dir);
    memcpy(s, suffix, sizeof(suffix));

    /* Start the search. */
    dir->find_handle = FindFirstFile(pattern, &dir->find_data);
    mem_free(pattern);
    if (dir->find_handle == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_NOT_OWNER) {  // Work around a Wine bug.
            SetLastError(ERROR_FILE_NOT_FOUND);
        }
        if (GetLastError() == ERROR_DIRECTORY) {
            windows_set_error(SYSERR_FILE_WRONG_TYPE, 0);
        } else {
            windows_set_error(0, 0);
        }
        goto error_free_dir;
    }

    dir->is_first = 1;
    return dir;

  error_free_dir:
    mem_free(dir);
  error_return:
    return NULL;
}

/*-----------------------------------------------------------------------*/

const char *sys_dir_read(SysDir *dir, int *is_subdir_ret)
{
    if (UNLIKELY(!dir) || UNLIKELY(!is_subdir_ret)) {
        DLOG("Invalid parameters: %p %p", dir, is_subdir_ret);
        windows_set_error(SYSERR_INVALID_PARAMETER, 0);
        return NULL;
    }

    int result;
    do {
        if (dir->is_first) {
            dir->is_first = 0;
            result = 1;
        } else {
            result = FindNextFile(dir->find_handle, &dir->find_data);
        }
    } while (result && strncmp(dir->find_data.cFileName, "..",
                               strlen(dir->find_data.cFileName)) == 0);
    if (!result && GetLastError() != ERROR_NO_MORE_FILES) {
        DLOG("FindNextFile() failed: %s", windows_strerror(GetLastError()));
    }

    if (result) {
        *is_subdir_ret =
            (dir->find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        return dir->find_data.cFileName;
    } else {
        return NULL;
    }
}

/*-----------------------------------------------------------------------*/

void sys_dir_close(SysDir *dir)
{
    if (dir) {
        FindClose(dir->find_handle);
        mem_free(dir);
    }
}

/*************************************************************************/
/*************************************************************************/
