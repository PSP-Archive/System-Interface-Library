/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/android/log.c: Log writing routines for Android.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/sysdep.h"
#include "src/sysdep/android/internal.h"
#include "src/sysdep/posix/fileutil.h"
#include "src/userdata.h"

#include <stdio.h>

#include <android/log.h>

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

void *sys_log_open(const char *name)
{
    const char *dir = android_external_data_path;
    if (!posix_mkdir_p(dir)) {
        return NULL;
    }

    char pathbuf[1000];
    if (strformat_check(pathbuf, sizeof(pathbuf), "%s/%s", dir, name)) {
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
    if (logfile) {
        fwrite(message, 1, len+1, logfile);
    } else {
        __android_log_print(ANDROID_LOG_DEBUG,
                            SIL_PLATFORM_ANDROID_DLOG_LOG_TAG,
                            "%.*s", len, message);
    }
}

/*-----------------------------------------------------------------------*/

void sys_log_close(void *logfile)
{
    fclose(logfile);
}

/*************************************************************************/
/*************************************************************************/
