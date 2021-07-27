/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/ios/log.m: Log writing routines for iOS.
 */

#define IN_SYSDEP

#import "src/base.h"
#import "src/sysdep.h"
#import "src/sysdep/ios/util.h"
#import "src/sysdep/posix/fileutil.h"
#import "src/userdata.h"

#import <stdio.h>

#import <Foundation/NSObjCRuntime.h>

#ifdef __IPHONE_10_0
# import <os/log.h>
#endif

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

void *sys_log_open(const char *name)
{
    const char *dir = ios_get_application_support_path();
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
#ifdef __IPHONE_10_0
# if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_10_0
        if (ios_version_is_at_least("10.0"))
# endif
        {
            os_log_debug(OS_LOG_DEFAULT, "%{public}.*s", len, message);
            return;
        }
#endif
        NSLog(@"%.*s", len, message);
    }
}

/*-----------------------------------------------------------------------*/

void sys_log_close(void *logfile)
{
    fclose(logfile);
}

/*************************************************************************/
/*************************************************************************/
