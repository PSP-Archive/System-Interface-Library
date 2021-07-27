/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/posix/misc.c: Tests for the POSIX implementation of the
 * miscellaneous system-level functions.
 */

#include "src/base.h"
#include "src/sysdep.h"
#include "src/sysdep/misc/ioqueue.h"
#include "src/sysdep/posix/time.h"
#include "src/test/base.h"

#include <time.h>

/*************************************************************************/
/*************************************************************************/

DEFINE_GENERIC_TEST_RUNNER(test_posix_misc)

TEST_INIT(init)
{
    CHECK_TRUE(sys_file_init());
    return 1;
}

TEST_CLEANUP(cleanup)
{
    sys_file_cleanup();
    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_error_async_invalid)
{
    int error;
    CHECK_INTEQUAL(ioq_wait(10000, &error), -1);
    CHECK_INTEQUAL(error, 0);
    CHECK_INTEQUAL(sys_last_error(), SYSERR_FILE_ASYNC_INVALID);
    CHECK_STREQUAL(sys_last_errstr(), "Invalid asynchronous read ID");

    return 1;
}

/*-----------------------------------------------------------------------*/

TEST(test_error_async_full)
{
    SysFile *fh;
    char path[4096+30];
    ASSERT(sys_get_resource_path_prefix(path, sizeof(path))
           < (int)sizeof(path));
    ASSERT(strformat_check(path+strlen(path), sizeof(path)-strlen(path),
                           "testdata/dir1/dir2/file.txt"));
    ASSERT(fh = sys_file_open(path));
    char buf[8];

    int reqlist[1000];
    int i;
    for (i = 0; i < lenof(reqlist); i++) {
        if (!(reqlist[i] = sys_file_read_async(fh, buf, 1, 0, -1))) {
            break;
        }
    }
    if (i >= lenof(reqlist)) {
        FAIL("Unable to force sys_file_read_async() failure by running out"
             " of async read handles");
    }

    CHECK_INTEQUAL(sys_last_error(), SYSERR_FILE_ASYNC_FULL);
    CHECK_STREQUAL(sys_last_errstr(), "Asynchronous read table full");

    for (i--; i >= 0; i--) {
        ASSERT(sys_file_wait_async(reqlist[i]) == 1);
    }
    sys_file_close(fh);
    return 1;
}

/*************************************************************************/
/*************************************************************************/
