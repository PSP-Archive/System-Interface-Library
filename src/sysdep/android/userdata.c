/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/android/userdata.c: User data access routines for Android.
 */

/*
 * The default pathnames for this implementation are as follows:
 *
 *    - Save files: $EXTERNAL/save/save-NNNN.bin
 *         (NNNN is the save number, zero-padded to 4 digits)
 *    - Settings file: $EXTERNAL/settings.bin
 *    - Per-user statistics file: $EXTERNAL/stats.bin
 *    - Arbitrary data files: $EXTERNAL/<datafile-path>
 *
 * userdata_get_data_path() is supported, and returns "$EXTERNAL".
 *
 * "$EXTERNAL" in the above pathnames is replaced by the external data path
 * assigned by Android.  (To avoid desynchronization of user data between
 * internal and external storage, data is always written to external
 * storage, and save operations will fail if no external storage is
 * available.)
 *
 * See ../posix/userdata.c for further details.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/android/internal.h"

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

char *sys_userdata_get_data_path(const char *program_name)
{
    PRECOND(program_name != NULL, return NULL);

    if (!android_external_data_path) {
        DLOG("No external storage available, user data disabled");
        return NULL;
    }

    const int path_size = strlen(android_external_data_path) + 2;
    char *path = mem_alloc(path_size, 0, 0);
    if (UNLIKELY(!path)) {
        DLOG("Out of memory generating user data directory path (%u bytes)",
             path_size);
        return NULL;
    }

    ASSERT(strformat_check(path, path_size, "%s/", android_external_data_path),
           return NULL);
    return path;
}

/*************************************************************************/
/*************************************************************************/
