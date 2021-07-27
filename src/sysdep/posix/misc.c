/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/posix/misc.c: Miscellaneous interface functions for
 * POSIX-compatible systems.
 */

/*
 * This source file defines the following functions:
 *
 * sys_last_error()
 * sys_last_errstr()
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/sysdep.h"

#include <stdio.h>
#include <sys/time.h>

/*************************************************************************/
/*************************************************************************/

void sys_console_vprintf(const char *format, va_list args)
{
    vprintf(format, args);
}

/*-----------------------------------------------------------------------*/

int sys_last_error(void)
{
    switch (errno) {
        case EINVAL:       return SYSERR_INVALID_PARAMETER;
        case ENOMEM:       return SYSERR_OUT_OF_MEMORY;
        case EMFILE:       return SYSERR_OUT_OF_MEMORY;
        case ENFILE:       return SYSERR_OUT_OF_MEMORY;
        case ENAMETOOLONG: return SYSERR_BUFFER_OVERFLOW;
        case EAGAIN:       return SYSERR_TRANSIENT_FAILURE;
        case ENOENT:       return SYSERR_FILE_NOT_FOUND;
        case EACCES:       return SYSERR_FILE_ACCESS_DENIED;
        case EISDIR:       return SYSERR_FILE_WRONG_TYPE;
        case ENOTDIR:      return SYSERR_FILE_WRONG_TYPE;
        case ECANCELED:    return SYSERR_FILE_ASYNC_ABORTED;
        case ESRCH:        return SYSERR_FILE_ASYNC_INVALID;  // See ioqueue.c.
        case ENOEXEC:      return SYSERR_FILE_ASYNC_FULL;     // See files.c.
    }
    return SYSERR_UNKNOWN_ERROR;
}

/*-----------------------------------------------------------------------*/

const char *sys_last_errstr(void)
{
    switch (errno) {
        case ESRCH:   return "Invalid asynchronous read ID";
        case ENOEXEC: return "Asynchronous read table full";
        default:      return strerror(errno);
    }
}

/*-----------------------------------------------------------------------*/

uint64_t sys_random_seed(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((uint64_t)tv.tv_sec)*1000000 + tv.tv_usec;
}

/*************************************************************************/
/*************************************************************************/
