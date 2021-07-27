/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/utility/log.c: Implementation of the DLOG() macro.
 */

#include "src/base.h"
#include "src/sysdep.h"
#include "src/utility/log.h"

/*************************************************************************/
/****************** Global data (only used for testing) ******************/
/*************************************************************************/

#ifdef SIL_INCLUDE_TESTS
char test_DLOG_last_message[SIL_DLOG_MAX_SIZE];
#endif

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

#ifdef DEBUG

/* Log filename, or empty for no log file. */
static char log_filename[64];

/* Has the log file been opened yet? */
static uint8_t log_file_opened = 0;

/* Did the pending log message buffer overflow? */
static uint8_t log_file_pending_overflow = 0;

/* File handle for the log file. */
static void *logfh = NULL;

/* Buffer for log messages written before the log file is opened. */
static char log_file_pending[16384];
static int log_file_pending_len;

#endif

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

void log_to_file(DEBUG_USED const char *name)
{
#ifdef DEBUG
    if (logfh) {
        sys_log_close(logfh);
        logfh = NULL;
        log_file_opened = 0;
    }

    if (name) {
        ASSERT(strformat_check(log_filename, sizeof(log_filename),
                               "%s", name));
    } else {
        *log_filename = '\0';
    }
#endif
}

/*************************************************************************/
/********************* Debug-only interface routines *********************/
/*************************************************************************/

#ifdef DEBUG

/*-----------------------------------------------------------------------*/

void do_DLOG(const char *file, unsigned int line, const char *function,
             const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vdo_DLOG(file, line, function, format, args);
    va_end(args);
}

/*-----------------------------------------------------------------------*/

void vdo_DLOG(const char *file, unsigned int line, const char *function,
             const char *format, va_list args)
{
    char buf[SIL_DLOG_MAX_SIZE];
    int len;

    /* Generate the message header. */
    if (file) {
#ifdef SIL_DLOG_STRIP_PATH
        const unsigned int striplen = strlen(SIL_DLOG_STRIP_PATH);
        if (strncmp(file, SIL_DLOG_STRIP_PATH, striplen) == 0) {
            file += striplen;
        }
#endif
        len = strformat(buf, sizeof(buf), "%s:%u(%s): ", file, line, function);
        len = ubound(len, (int)sizeof(buf)-1);
    } else {
        /* No line header requested, so proceed with an empty buffer. */
        len = 0;
    }

    /* Append the caller-specified message. */
    len += vstrformat(buf + len, sizeof(buf) - len, format, args);
    len = ubound(len, (int)sizeof(buf)-1);
    if (len > 0 && buf[len-1] == '\n') {
        buf[--len] = '\0';
    }
    buf[len] = '\n';  // Safe because it was previously a null terminator.

#ifdef SIL_INCLUDE_TESTS
    memcpy(test_DLOG_last_message, buf, len);
    test_DLOG_last_message[len] = '\0';
#endif

    sys_log_write(NULL, buf, len);

    if (*log_filename && !log_file_opened) {
        /* Set this immediately to avoid infinite recursion in case
         * sys_log_open() tries to log anything. */
        log_file_opened = 1;
        logfh = sys_log_open(log_filename);
        if (logfh) {
            char *s = log_file_pending;
            char *end = log_file_pending + log_file_pending_len;
            while (s < end) {
                char *eol = memchr(s, '\n', end-s);
                if (!eol) {
                    break;
                }
                sys_log_write(logfh, s, eol-s);
                s = eol+1;
            }
            if (log_file_pending_overflow) {
                static const char lost[] = "[some log messages lost]\n";
                sys_log_write(logfh, lost, strlen(lost) - 1);
            }
            log_file_pending_len = 0;
            log_file_pending_overflow = 0;
        } else {
            log_file_opened = 0;
        }
    }
    if (logfh) {
        sys_log_write(logfh, buf, len);
    } else {
        const int available = sizeof(log_file_pending) - log_file_pending_len;
        const int to_copy = ubound(len+1, available);  // +1 for the newline.
        memcpy(log_file_pending + log_file_pending_len, buf, to_copy);
        log_file_pending_len += to_copy;
        log_file_pending_overflow |= (available < len+1);
    }
}

/*-----------------------------------------------------------------------*/

#endif  // DEBUG

/*************************************************************************/
/*************************************************************************/
