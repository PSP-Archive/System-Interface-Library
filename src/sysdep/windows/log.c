/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/windows/log.c: Log writing routines for Windows, using
 * Windows system calls instead of stdio to avoid the risk of MinGW/MSVC
 * stdio library collisions.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/sysdep.h"
#include "src/sysdep/windows/internal.h"
#include "src/userdata.h"

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

void *sys_log_open(const char *name)
{
    const char *dir = userdata_get_data_path();
    if (!dir) {
        return NULL;
    }

    /* Get the directories created if necessary. */
    int id = userdata_save_data(name, "", 0);
    if (id) {
        userdata_wait(id);
        (void) userdata_get_result(id);
    }

    char pathbuf[1000];
    if (strformat_check(pathbuf, sizeof(pathbuf), "%s%s", dir, name)) {
        HANDLE fh = CreateFile(pathbuf, FILE_WRITE_DATA, 0, NULL,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
        if (fh != INVALID_HANDLE_VALUE) {
            return fh;
        }
    }

    return NULL;
}

/*-----------------------------------------------------------------------*/

void sys_log_write(void *logfile, const char *message, int len)
{
    HANDLE fh = logfile ? (HANDLE)logfile : GetStdHandle(STD_ERROR_HANDLE);

    /* We need to write the message using Windows newlines (CRLF), so we
     * have to process the string a line at a time.  If possible, we do
     * the processing ahead of time so we can write the final message in a
     * single write operation and avoid simultaneous messages from multiple
     * threads getting intermingled. */
    const HANDLE heap = GetProcessHeap();
    const int bufsize = 2*len+2;  // Worst case: "\n" x len
    char *buf = HeapAlloc(heap, 0, bufsize);
    int buflen = 0;

    while (len >= 0) {
        const char *s = memchr(message, '\n', len+1);
        ASSERT(s, return);  // The message is always terminated with \n.
        const int linelen = s - message;
        if (buf) {
            ASSERT(buflen + (linelen+2) <= bufsize, return);
            memcpy(buf+buflen, message, linelen);
            buflen += linelen;
            buf[buflen++] = '\r';
            buf[buflen++] = '\n';
        } else {
            WriteFile(fh, message, linelen, (DWORD[1]){0}, NULL);
            WriteFile(fh, "\r\n", 2, (DWORD[1]){0}, NULL);
        }
        len -= (s+1) - message;
        message = s+1;
    }

    if (buf) {
        ASSERT(buflen <= bufsize);
        WriteFile(fh, buf, buflen, (DWORD[1]){0}, NULL);
        HeapFree(heap, 0, buf);
    }
}

/*-----------------------------------------------------------------------*/

void sys_log_close(void *logfile)
{
    CloseHandle(logfile);
}

/*************************************************************************/
/*************************************************************************/
