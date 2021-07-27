/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/linux/userdata.c: User data access routines for Linux.
 */

/*
 * The default pathnames for this implementation are as follows:
 *
 *    - Save files: $USERDATA/<program-name>/save/save-NNNN.bin
 *         (NNNN is the save number, zero-padded to 4 digits)
 *    - Settings file: $USERDATA/<program-name>/settings.bin
 *    - Per-user statistics file: $USERDATA/<program-name>/stats.bin
 *    - Arbitrary data files: $USERDATA/<program-name>/<datafile-path>
 *
 * userdata_get_data_path() is supported, and returns
 * "$USERDATA/<program-name>/".
 *
 * "$USERDATA" in the above pathnames is replaced by:
 *    - the contents of the environment variable XDG_DATA_HOME, if that
 *      variable is not empty;
 *    - the contents of the environment variable HOME with "/.local/share"
 *      appended, if that variable is not empty;
 *    - otherwise, the string "." (i.e., the current directory).
 *
 * See ../posix/userdata.c for further details.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/memory.h"
#include "src/sysdep.h"

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

char *sys_userdata_get_data_path(const char *program_name)
{
    PRECOND(program_name != NULL, return NULL);

    const char *base_path = getenv("XDG_DATA_HOME");
    const char *base_append = "/";
    if (!base_path || !*base_path) {
        base_path = getenv("HOME");
        base_append = "/.local/share/";
    }
    if (!base_path || !*base_path) {
        base_path = ".";  // Fall back to the current directory if necessary.
        base_append = "/";
    }

    const int path_size = strlen(base_path)    // $HOME
                        + strlen(base_append)  // "/.local/share/"
                        + strlen(program_name) // program_name
                        + 2;                   // "/\0"
    char *path = mem_alloc(path_size, 0, 0);
    if (UNLIKELY(!path)) {
        DLOG("Out of memory generating user data directory path (%u bytes)",
             path_size);
        return NULL;
    }

    ASSERT(strformat_check(path, path_size, "%s%s%s/",
                           base_path, base_append, program_name),
           return NULL);
    return path;
}

/*************************************************************************/
/*************************************************************************/
