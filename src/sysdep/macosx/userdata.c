/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/macosx/userdata.c: User data access routines for Mac OS X.
 */

/*
 * The default pathnames for this implementation are as follows:
 *
 *    - Save files: <user-data-path>/save/save-NNNN.bin
 *         (NNNN is the save number, zero-padded to 4 digits)
 *    - Settings file: <user-data-path>/settings.bin
 *    - Per-user statistics file: <user-data-path>/stats.bin
 *    - Arbitrary data files: <user-data-path>/<datafile-path>
 *
 * where <user-data-path> is the path returned by userdata_get_data_path()
 * minus the trailing slash.
 *
 * userdata_get_data_path() is supported, and returns
 * "$HOME/Library/Application Support/<program-name>/".
 *
 * "$HOME" in the above pathnames is replaced by the user's home directory
 * as found in the environment variable $HOME, or "." if that variable is
 * missing or empty.
 *
 * See ../posix/userdata.c for further details.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/macosx/util.h"

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

char *sys_userdata_get_data_path(const char *program_name)
{
    PRECOND(program_name != NULL, return NULL);

    const char *as_path = macosx_get_application_support_path();
    const int size = strlen(as_path) + strlen(program_name) + 3;
    char *path = mem_alloc(size, 1, 0);
    if (!path) {
        DLOG("Out of memory generating user data directory path (%s/%s/)",
             as_path, program_name);
        return NULL;
    }
    ASSERT(strformat_check(path, size, "%s/%s/", as_path, program_name),
           return NULL);
    return path;
}

/*************************************************************************/
/*************************************************************************/
