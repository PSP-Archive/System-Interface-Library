/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/stdio.c: A minimal stdio implementation for the PSP.
 */

/*
 * The features/limitations of this stdio implementation are as follows:
 *
 * - Files opened for reading are buffered entirely in memory, and are (if
 *   possible) loaded from package files in preference to the filesystem.
 *
 * - There is no difference between text and binary mode (the "t" and "b"
 *   flags to fopen() and freopen() are ignored).
 *
 * - Opening files for both reading and writing ("r+"/"w+") is not supported.
 *
 * - freopen() is not supported, except when used on a file opened for
 *   reading to reopen the same file again for reading.  (This is used by
 *   Lua to switch a file from text to binary mode.)
 *
 * - File descriptors are not supported.  fdopen() will always return
 *   NULL; however, fileno() returns the file pointer cast to an integer,
 *   since the C++ STL implemented by GCC requires it for calls to read(),
 *   write(), and fstat().  Naturally, this will not work on 64-bit
 *   platforms, but it works on the PSP.
 *
 * - Files opened for writing are never buffered, except that calls to
 *   fprintf() will write the entire generated string at once.  (As a
 *   result, code which e.g. calls fwrite() with small buffer sizes
 *   repeatedly will have poor performance.  However, games normally do
 *   not write to the filesystem when built for the PSP, so this should
 *   not be a problem in practice.)
 *
 * - printf(), fprintf(), vprintf(), and vfprintf() will silently truncate
 *   output from a single call at 9999 bytes.
 *
 * - remove() is not supported.
 *
 * - scanf() and fscanf() are not supported; sscanf() is only supported to
 *   the extent required by the TinyXML library (namely, the two format
 *   strings "%d" and "%lf").
 *
 * This implementation assumes that the program is built with Newlib, and
 * sets up structures appropriately so that external libraries using the
 * macros in stdio.h still operate correctly:
 *
 * - For files in read mode, __sFILE._p points to the current position in
 *   the read buffer, and __sFILE._r contains the number of bytes still
 *   available for reading; these fields are updated appropriately on
 *   fread(), fseek(), and so on.
 *
 * - For files in write mode, __sFILE.{_p,_w,_lbfsize} are all NULL/zero.
 *
 * - __SEOF and __SERR are set in __sFILE._flags when end-of-file or an I/O
 *   error is detected, respectively.  We also set __SRD, __SWR, and __SAPP
 *   for internal reference, though these are not checked by stdio.h macros.
 *
 * - __sFILE._file is always set to -1.
 *
 * - __sFILE._bf and __sFILE._data are not used by stdio.h macros, and are
 *   cleared to zero at allocation time.
 */

/*************************************************************************/
/*************** Internal data structures and declarations ***************/
/*************************************************************************/

/* Include common headers. */

#ifdef DEBUG
# define saved_DEBUG
# undef DEBUG
#endif
#include <sys/reent.h>
#ifdef saved_DEBUG
# undef saved_DEBUG
# define DEBUG
#endif

#include "src/base.h"
#include "src/memory.h"
#include "src/resource.h"
#include "src/sysdep.h"
#include "src/sysdep/psp/internal.h"

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

/* Clear out Newlib macros from stdio.h so we can define the functions. */
#undef feof
#undef ferror

/*************************************************************************/

/* Replacement FILE structure for our use.  We provide the fields used by
 * Newlib's <stdio.h> macros at the top of the structure. */

typedef struct pspstdio_FILE_ {
    /* Newlib data. */
    struct __sFILE newlib;

    /* Resource ID, file data buffer, and size for files opened for reading,
     * and associated resource manager. */
    int res_id;
    uint8_t *data;
    uint32_t size;
    ResourceManager *resmgr;
    int mark;  // Sync mark for checking data availability.

    /* File descriptor for files opened for writing. */
    int fd;

    /* File pathname (used to detect freopen() to the same file; also used
     * in debug messages). */
    char path[256];
} pspstdio_FILE;

/*-----------------------------------------------------------------------*/

/* Definition of global stdio data.  The struct _reent stuff is needed
 * because Newlib's stdin/out/error are macros that reference _impure_ptr. */

static FILE _stdin, _stdout, _stderr;
static struct _reent _impure_data = {
    ._stdin = &_stdin, ._stdout = &_stdout, ._stderr = &_stderr};
struct _reent *_impure_ptr = &_impure_data;

/*-----------------------------------------------------------------------*/

/* Internal helper function declarations. */

/**
 * wait_for_data:  Wait for the given file's data to be loaded into memory,
 * if necessary.
 *
 * [Parameters]
 *     f: File pointer.
 * [Return value]
 *     True if the data was successfully read, false if not.
 */
static int wait_for_data(pspstdio_FILE *f);

/*************************************************************************/
/*********************** File management functions ***********************/
/*************************************************************************/

FILE *fopen(const char *name, const char *mode)
{
    pspstdio_FILE *f;

    if (strlen(name) > sizeof(f->path) - 1) {
        DLOG("Pathname %s overflows pspstdio_FILE.path", name);
        errno = ENAMETOOLONG;
        goto error_return;
    }

    unsigned int flags = 0;
    switch (*mode++) {
      case 'r':
        flags = __SRD;
        break;
      case 'w':
        flags = __SWR;
        break;
      case 'a':
        flags = __SWR | __SAPP;
        break;
      default:
        errno = EINVAL;
        goto error_return;
    }
    if (*mode != 0 && *mode != 'b' && *mode != 't') {
        errno = EINVAL;
        goto error_return;
    }

    f = mem_alloc(sizeof(*f), 0, MEM_ALLOC_TEMP | MEM_ALLOC_CLEAR);
    if (!f) {
        errno = ENOMEM;
        goto error_return;
    }
    f->newlib._flags = flags;
    f->newlib._file = -1;
    strcpy(f->path, name);  // Safe (length checked above).

    if (f->newlib._flags & __SRD) {

        /* We load the data using a resource manager, which we then
         * delete when the file is closed.  This allows us to access
         * data stored in package files at no extra cost. */
        if (!(f->resmgr = resource_create(1))) {
            DLOG("%s: Failed to create resource manager", f->path);
            goto error_free_file;
        }
        f->res_id = resource_load_data(f->resmgr, f->path, 0, RES_ALLOC_TEMP);
        if (!f->res_id) {
            DLOG("%s: Failed to open via resource manager", f->path);
            goto error_free_resmgr;
        }
        f->mark = resource_mark(f->resmgr);

        /* We don't actually wait for the data to be read until the first
         * read operation.  That may be just a few cycles away, of course,
         * but let's give the read thread as much time as we can. */

    } else {  // !(f->newlib._flags & __SRD) --> write mode

        f->fd = sceIoOpen(f->path,
                          PSP_O_WRONLY | PSP_O_CREAT
                              | (f->newlib._flags & __SAPP ? 0 : PSP_O_TRUNC),
                          0666);
        if (f->fd < 0) {
            DLOG("%s: Failed to open file: %s", f->path, psp_strerror(f->fd));
            errno = EIO;
            goto error_free_file;
        }

    }

    return (FILE *)f;

  error_free_resmgr:
    resource_destroy(f->resmgr);
  error_free_file:
    mem_free(f);
  error_return:
    return NULL;
}

/*-----------------------------------------------------------------------*/

FILE *freopen(const char *name, const char *mode, FILE *_f)
{
    if (_f == stdin || _f == stdout || _f == stderr) {
        DLOG("Attempt to reopen standard stream %s",
             _f==stdin ? "stdin" : _f==stdout ? "stdout" : "stderr");
        errno = EINVAL;
        return NULL;
    }

    pspstdio_FILE *f = (pspstdio_FILE *)_f;

    if (!(f->newlib._flags & __SRD)) {
        DLOG("%p (%s) is not open for reading", f, f->path);
        errno = EINVAL;
        return NULL;
    }
    if (strcmp(name, f->path) != 0) {
        DLOG("Attempt to reopen %p (%s) with different name %s",
             f, f->path, name);
        errno = EINVAL;
        return NULL;
    }
    if (mode[0] != 'r' || (mode[1] != 0 && mode[1] != 'b' && mode[1] != 't')) {
        DLOG("Attempt to reopen %p (%s) with different mode %s",
             f, f->path, mode);
        errno = EINVAL;
        return NULL;
    }

    f->newlib._p = f->data;
    f->newlib._r = f->size;
    return (FILE *)f;
}

/*-----------------------------------------------------------------------*/

int fclose(FILE *_f)
{
    if (_f == stdin || _f == stdout || _f == stderr) {
        DLOG("Attempt to close standard stream %s",
             _f==stdin ? "stdin" : _f==stdout ? "stdout" : "stderr");
        errno = EINVAL;
        return -1;
    }

    pspstdio_FILE *f = (pspstdio_FILE *)_f;

    if (f->newlib._flags & __SRD) {
        resource_destroy(f->resmgr);  // Also deletes the data buffer.
    } else {
        sceIoClose(f->fd);
    }
    mem_free(f);
    return 0;
}

/*-----------------------------------------------------------------------*/

int fileno(FILE *_f)
{
    return (int)_f;
}

/*-----------------------------------------------------------------------*/

FILE *fdopen(UNUSED int fd, UNUSED const char *mode)
{
    errno = EINVAL;
    return NULL;
}

/*-----------------------------------------------------------------------*/

int feof(FILE *_f)
{
    return _f->_flags & __SEOF;
}

/*-----------------------------------------------------------------------*/

int ferror(FILE *_f)
{
    return _f->_flags & __SERR;
}

/*-----------------------------------------------------------------------*/

int setvbuf(UNUSED FILE *_f, UNUSED char *buffer, UNUSED int mode,
            UNUSED size_t size)
{
    return 0;
}

/*-----------------------------------------------------------------------*/

int fflush(UNUSED FILE *_f)
{
    return 0;
}

/*-----------------------------------------------------------------------*/

int fstat(int fd, struct stat *st)
{
    if (fd < 0x8800000 || fd >= 0xC000000) {
        DLOG("Invalid file descriptor value 0x%X", fd);
        errno = EBADF;
        return -1;
    } else if (fd == (int)stdin || fd == (int)stdout || fd == (int)stderr) {
        DLOG("Attempt to fstat() standard stream %s",
             fd==(int)stdin ? "stdin" : fd==(int)stdout ? "stdout" : "stderr");
        errno = EBADF;
        return -1;
    }

    pspstdio_FILE *f = (pspstdio_FILE *)fd;

    /* This is only used by STL to retrieve the size of a file, so we don't
     * sweat the other details of the structure. */
    mem_clear(&st, sizeof(st));
    if (f->newlib._flags & __SRD) {
        st->st_size = f->size;
    } else {
        long saved_pos, filesize;
        if ((saved_pos = sceIoLseek(f->fd, 0, PSP_SEEK_CUR)) < 0
         || (filesize = sceIoLseek(f->fd, 0, PSP_SEEK_END)) < 0
         || sceIoLseek(f->fd, saved_pos, PSP_SEEK_SET) < 0) {
            errno = EIO;
            return -1;
        }
        st->st_size = filesize;
    }

    return 0;
}

/*************************************************************************/
/************************** File I/O functions ***************************/
/*************************************************************************/

off_t lseek(int fd, off_t pos, int whence)
{
    if (fd < 0x8800000 || fd >= 0xC000000) {
        DLOG("Invalid file descriptor value 0x%X", fd);
        errno = EBADF;
        return -1;
    } else if (fd == (int)stdin || fd == (int)stdout || fd == (int)stderr) {
        DLOG("Attempt to lseek() standard stream %s",
             fd==(int)stdin ? "stdin" : fd==(int)stdout ? "stdout" : "stderr");
        errno = EBADF;
        return -1;
    }

    if (fseek((FILE *)fd, pos, whence) != 0) {
        return -1;
    }
    return ftell((FILE *)fd);
}

/*-----------------------------------------------------------------------*/

int fseek(FILE *_f, long pos, int whence)
{
    if (_f == stdin || _f == stdout || _f == stderr) {
        DLOG("Attempt to seek in standard stream %s",
             _f==stdin ? "stdin" : _f==stdout ? "stdout" : "stderr");
        errno = EBADF;
        return -1;
    }

    pspstdio_FILE *f = (pspstdio_FILE *)_f;

    if (f->newlib._flags & __SRD) {

        if (!wait_for_data(f)) {
            return -1;
        }
        if (whence == SEEK_CUR) {
            pos += f->newlib._p - f->data;
        } else if (whence == SEEK_END) {
            pos += f->size;
        }
        pos = bound(pos, 0, f->size);
        f->newlib._p = f->data + pos;
        f->newlib._r = f->size - pos;

    } else {

        int res = sceIoLseek(f->fd, pos, whence);
        if (res < 0) {
            DLOG("sceIoLseek(%s, %ld, %d): %s", f->path, pos, whence,
                 psp_strerror(errno));
            errno = EIO;
            return -1;
        }

    }

    return 0;
}

/*-----------------------------------------------------------------------*/

long ftell(FILE *_f)
{
    if (_f == stdin || _f == stdout || _f == stderr) {
        DLOG("Attempt to tell standard stream %s",
             _f==stdin ? "stdin" : _f==stdout ? "stdout" : "stderr");
        errno = EBADF;
        return -1;
    }

    pspstdio_FILE *f = (pspstdio_FILE *)_f;

    if (f->newlib._flags & __SRD) {

        if (!wait_for_data(f)) {
            return -1;
        }
        return f->newlib._p - f->data;

    } else {

        int res = sceIoLseek(f->fd, 0, PSP_SEEK_CUR);
        if (res < 0) {
            DLOG("sceIoLseek(%s, 0, SEEK_CUR): %s", f->path,
                 psp_strerror(errno));
            errno = EIO;
            return -1;
        }
        return res;

    }
}

/*-----------------------------------------------------------------------*/

ssize_t read(int fd, void *ptr, size_t size)
{
    if (fd < 0x8800000 || fd >= 0xC000000) {
        DLOG("Invalid file descriptor value 0x%X", fd);
        errno = EBADF;
        return -1;
    } else if (fd == (int)stdin || fd == (int)stdout || fd == (int)stderr) {
        DLOG("Attempt to read() standard stream %s",
             fd==(int)stdin ? "stdin" : fd==(int)stdout ? "stdout" : "stderr");
        errno = EBADF;
        return -1;
    }

    return fread(ptr, 1, size, (FILE *)fd);
}

/*-----------------------------------------------------------------------*/

size_t fread(void *ptr, size_t size, size_t n, FILE *_f)
{
    if (_f == stdin || _f == stdout || _f == stderr) {
        DLOG("Attempt to read from standard stream %s",
             _f==stdin ? "stdin" : _f==stdout ? "stdout" : "stderr");
        errno = EBADF;
        return 0;
    }

    pspstdio_FILE *f = (pspstdio_FILE *)_f;

    if (!(f->newlib._flags & __SRD)) {
        DLOG("Attempt to read from %s opened for writing", f->path);
        errno = EBADF;
        return 0;
    }

    if (!wait_for_data(f)) {
        return 0;
    }

    const uint32_t total = size * n;  // Assume no overflow
    if (total <= (uint32_t)f->newlib._r) {
        memcpy(ptr, f->newlib._p, total);
        f->newlib._p += total;
        f->newlib._r -= total;
        return n;
    } else {
        size_t retval = f->newlib._r;
        memcpy(ptr, f->newlib._p, f->newlib._r);
        f->newlib._p += f->newlib._r;
        f->newlib._r = 0;
        f->newlib._flags |= __SEOF;
        return retval;
    }
}

/*-----------------------------------------------------------------------*/

int fgetc(FILE *_f)
{
    if (_f == stdin || _f == stdout || _f == stderr) {
        DLOG("Attempt to read from standard stream %s",
             _f==stdin ? "stdin" : _f==stdout ? "stdout" : "stderr");
        errno = EINVAL;
        return EOF;
    }

    pspstdio_FILE *f = (pspstdio_FILE *)_f;

    if (!(f->newlib._flags & __SRD)) {
        DLOG("Attempt to read from %s opened for writing", f->path);
        errno = EBADF;
        return EOF;
    }

    if (!wait_for_data(f)) {
        return EOF;
    }

    if (f->newlib._r > 0) {
        int c = *f->newlib._p;
        f->newlib._p++;
        f->newlib._r--;
        return c;
    } else {
        f->newlib._flags |= __SEOF;
        return EOF;
    }
}

#undef getc
int getc(FILE *_f) __attribute__((alias("fgetc")));

/*-----------------------------------------------------------------------*/

#undef getchar
int getchar(void)
{
    return getc(stdin);
}

/*-----------------------------------------------------------------------*/

int ungetc(int c, FILE *_f)
{
    if (_f == stdin || _f == stdout || _f == stderr) {
        DLOG("Attempt to ungetc() on standard stream %s",
             _f==stdin ? "stdin" : _f==stdout ? "stdout" : "stderr");
        errno = EINVAL;
        return EOF;
    }

    pspstdio_FILE *f = (pspstdio_FILE *)_f;

    if (!(f->newlib._flags & __SRD)) {
        DLOG("Attempt to ungetc() on %s opened for writing", f->path);
        errno = EBADF;
        return EOF;
    }

    if (!wait_for_data(f)) {
        return EOF;
    }

    if (f->newlib._p == f->data) {
        DLOG("%s is already at the beginning of the stream!", f->path);
        errno = EINVAL;
        return EOF;
    }
    if (c != f->newlib._p[-1]) {
        DLOG("%s: Put-back character 0x%02X doesn't match data (0x%02X) at"
             " 0x%X", f->path, c, f->newlib._p[-1],
             (int)(f->newlib._p - f->data - 1));
        errno = EINVAL;
        return EOF;
    }
    f->newlib._p--;
    f->newlib._r++;
    return c;
}

/*-----------------------------------------------------------------------*/

char *fgets(char *buffer, int size, FILE *_f)
{
    if (_f == stdin || _f == stdout || _f == stderr) {
        DLOG("Attempt to read from standard stream %s",
             _f==stdin ? "stdin" : _f==stdout ? "stdout" : "stderr");
        errno = EINVAL;
        return NULL;
    }

    pspstdio_FILE *f = (pspstdio_FILE *)_f;

    if (!(f->newlib._flags & __SRD)) {
        DLOG("Attempt to read from %s opened for writing", f->path);
        errno = EBADF;
        return NULL;
    }

    if (!wait_for_data(f)) {
        return NULL;
    }

    char *ptr = buffer;

    while (size > 1) {
        if (f->newlib._r == 0) {
            f->newlib._flags |= __SEOF;
            break;
        }
        *ptr++ = *f->newlib._p++;
        f->newlib._r--;
        size--;
        if (ptr[-1] == '\n') {
            break;
        }
    }

    if (size > 0) {
        *ptr = 0;
    }
    return buffer;
}

/*-----------------------------------------------------------------------*/

int __srget_r(UNUSED struct _reent *reent, FILE *_f)
{
    if (_f->_p == NULL) {
        if (_f == stdin || _f == stdout || _f == stderr
         || !wait_for_data((pspstdio_FILE *)_f)) {
            return EOF;
        }
    }

    if (_f->_r > 0) {
        _f->_r--;
        return *_f->_p++;
    } else {
        _f->_flags |= __SEOF;
        return EOF;
    }
}

/*-----------------------------------------------------------------------*/

ssize_t write(int fd, const void *ptr, size_t size)
{
    if (fd < 0x8800000 || fd >= 0xC000000) {
        DLOG("Invalid file descriptor value 0x%X", fd);
        errno = EBADF;
        return -1;
    } else if (fd == (int)stdin || fd == (int)stdout || fd == (int)stderr) {
        DLOG("Attempt to write() standard stream %s",
             fd==(int)stdin ? "stdin" : fd==(int)stdout ? "stdout" : "stderr");
        errno = EBADF;
        return -1;
    }

    return fwrite(ptr, 1, size, (FILE *)fd);
}

/*-----------------------------------------------------------------------*/

size_t fwrite(const void *ptr, size_t size, size_t n, FILE *_f)
{
    if (_f == stdin) {
        DLOG("Attempt to write to standard stream stdin");
        errno = EBADF;
        return EOF;
    }

    int fd;
#ifdef DEBUG
    const char *path;
#endif
    if (_f == stdout) {
        fd = 1;
#ifdef DEBUG
        path = "<stdout>";
#endif
    } else if (_f == stderr) {
        fd = 2;
#ifdef DEBUG
        path = "<stderr>";
#endif
    } else {
        pspstdio_FILE *f = (pspstdio_FILE *)_f;
        if (!(f->newlib._flags & __SWR)) {
            DLOG("Attempt to write to %s opened for reading", f->path);
            errno = EBADF;
            return EOF;
        }
        fd = f->fd;
#ifdef DEBUG
        path = f->path;
#endif
    }

    const uint32_t total = size * n;  // Assume no overflow
    const int32_t result = sceIoWrite(fd, ptr, total);
    if (result < 0) {
        DLOG("sceIoWrite(%s, %p, %u): %s", path, ptr, total,
             psp_strerror(result));
        _f->_flags |= __SERR;
        return EOF;
    }
    return result / size;
}

/*-----------------------------------------------------------------------*/

int fputc(int c, FILE *_f)
{
    char _c = (char)c;
    if (fwrite(&_c, 1, 1, _f) == 1) {
        return _c;
    } else {
        return EOF;
    }
}

#undef putc
int putc(int c, FILE *_f) __attribute__((alias("fputc")));

/*-----------------------------------------------------------------------*/

#undef putchar
int putchar(int c)
{
    return putc(c, stdout);
}

/*-----------------------------------------------------------------------*/

int fputs(const char *string, FILE *_f)
{
    return fwrite(string, 1, strlen(string), _f);
}

/*-----------------------------------------------------------------------*/

int puts(const char *string)
{
    if (fputs(string, stdout) < 0 || fputc('\n', stdout)) {
        return EOF;
    }
    return strlen(string) + 1;
}

/*************************************************************************/
/******************* printf()/scanf()-family functions *******************/
/*************************************************************************/

int printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    return vfprintf(stdout, format, args);
    va_end(args);
}

int fprintf(FILE *f, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    return vfprintf(f, format, args);
    va_end(args);
}

int sprintf(char *buffer, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    return vsnprintf(buffer, 0x7FFFFFFF, format, args);
    va_end(args);
}

int snprintf(char *buffer, size_t size, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    return vsnprintf(buffer, size, format, args);
    va_end(args);
}

/*-----------------------------------------------------------------------*/

int vprintf(const char *format, va_list args)
{
    return vfprintf(stdout, format, args);
}

int vfprintf(FILE *f, const char *format, va_list args)
{
    char buffer[10000];
    unsigned int length = vstrformat(buffer, sizeof(buffer), format, args);
    return fwrite(buffer, 1, ubound(length, sizeof(buffer)-1), f);
}

int vsprintf(char *buffer, const char *format, va_list args)
{
    return vsnprintf(buffer, 0x7FFFFFFF, format, args);
}

int vsnprintf(char *buffer, size_t size, const char *format, va_list args)
{
    return vstrformat(buffer, size, format, args);
}

/*-----------------------------------------------------------------------*/

/* Used by the assert() macro. */

int fiprintf(FILE *f, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    return vfprintf(f, format, args);
    va_end(args);
}

/*-----------------------------------------------------------------------*/

/* snprintf() and vsnprintf() are defined in util.c. */

/*-----------------------------------------------------------------------*/

int sscanf(const char *buffer, const char *format, ...)
{
    while (*buffer == ' '
        || *buffer == '\t'
        || *buffer == '\r'
        || *buffer == '\n'
        || *buffer == '\v')
    {
        buffer++;
    }

    if (strcmp(format, "%d") == 0) {

        char *end;
        long value = strtol(buffer, &end, 10);
        if (end == buffer) {
            return 0;
        }

        va_list args;
        va_start(args, format);
        int *ptr = va_arg(args, int *);
        va_end(args);
        *ptr = (int)value;
        return 1;

    } else if (strcmp(format, "%lf") == 0) {

        char *end;
        double value = strtod(buffer, &end);
        if (end == buffer) {
            return 0;
        }

        va_list args;
        va_start(args, format);
        double *ptr = va_arg(args, double *);
        va_end(args);
        *ptr = value;
        return 1;

    } else {

        DLOG("Invalid format string: %s", format);
        return 0;

    }
}

/*************************************************************************/
/*********************** Internal helper functions ***********************/
/*************************************************************************/

static int wait_for_data(pspstdio_FILE *f)
{
    if (f->mark) {
        resource_wait(f->resmgr, f->mark);
        f->mark = 0;
        int size;
        f->data = resource_get_data(f->resmgr, f->res_id, &size);
        f->size = size;
        f->newlib._p = f->data;
        f->newlib._r = f->size;
        if (!f->data) {
            DLOG("%s: Failed to load via resource manager", f->path);
        }
    }

    if (!f->data) {
        f->newlib._flags |= __SERR;
        return 0;
    }

    return 1;
}

/*************************************************************************/
/*************************************************************************/
