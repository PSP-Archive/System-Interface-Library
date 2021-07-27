/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/misc/log-stdio.c: sys_log_*() implementation for systems
 * which can use stdio to write log messages and can create log files in
 * the user data directory.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/sysdep.h"
#include "src/userdata.h"

#include <stdio.h>

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
        FILE *fh = fopen(pathbuf, "w");
        if (fh) {
            setvbuf(fh, NULL, _IOLBF, 0);  // Line-buffered.
            return fh;
        }
    }

    return NULL;
}

/*-----------------------------------------------------------------------*/

void sys_log_write(void *logfile, const char *message, int len)
{
    FILE *fh = logfile ? (FILE *)logfile : stderr;
    fwrite(message, 1, len+1, fh);
}

/*-----------------------------------------------------------------------*/

void sys_log_close(void *logfile)
{
    fclose(logfile);
}

/*************************************************************************/
/*************************************************************************/
