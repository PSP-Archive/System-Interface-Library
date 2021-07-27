/*
 * libwebmdec: a decoder library for WebM audio/video streams
 * Copyright (c) 2014-2019 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include "test/test.h"

#include <stdio.h>

/*************************************************************************/
/****************** Test callbacks and associated data *******************/
/*************************************************************************/

/* Number of times the close() callback was called. */
static int close_call_count;

/*-----------------------------------------------------------------------*/

/*
 * These are identical to the stdio callbacks used by webmdec_open_with_file(),
 * except that we track the number of calls to the close() callback to
 * verify that it is called exactly once.
 */

static long file_length(void *opaque)
{
    FILE *f = (FILE *)opaque;
    const long saved_offset = ftell(f);
    if (fseek(f, 0, SEEK_END) != 0) {
        return -1;
    }
    const long length = ftell(f);
    if (fseek(f, saved_offset, SEEK_SET) != 0) {
        return -1;
    }
    return length;
}

static long file_tell(void *opaque)
{
    FILE *f = (FILE *)opaque;
    return ftell(f);
}

static void file_seek(void *opaque, long offset)
{
    FILE *f = (FILE *)opaque;
    fseek(f, offset, SEEK_SET);
}

static long file_read(void *opaque, void *buffer, long length)
{
    FILE *f = (FILE *)opaque;
    return (long)fread(buffer, 1, (size_t)length, f);
}

static void file_close(void *opaque)
{
    FILE *f = (FILE *)opaque;
    fclose(f);
    close_call_count++;
}

/*************************************************************************/
/*********************** Individual test routines ************************/
/*************************************************************************/

static int test_open_callbacks_basic(void)
{
    static const webmdec_callbacks_t callbacks = {
        .length = file_length,
        .tell   = file_tell,
        .seek   = file_seek,
        .read   = file_read,
        .close  = file_close,
    };

    FILE *fp;
    assert_true(fp = fopen("test/data/stereo.webm", "rb"));

    webmdec_error_t error = (webmdec_error_t)-1;
    webmdec_t *handle;
    assert_true(handle = webmdec_open_from_callbacks(
                             callbacks, fp, WEBMDEC_OPEN_ANY, &error));
    assert_equal(error, WEBMDEC_NO_ERROR);
    webmdec_close(handle);

    return 1;
}

/*************************************************************************/
/****************************** Test runner ******************************/
/*************************************************************************/

int test_open_callbacks(void)
{
    int pass = 1;

    pass &= test_open_callbacks_basic();

    return pass;
}

/*************************************************************************/
/*************************************************************************/
