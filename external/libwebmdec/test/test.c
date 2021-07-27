/*
 * libwebmdec: a decoder library for WebM audio/video streams
 * Copyright (c) 2014-2019 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include "test/test.h"
#include "include/internal.h"  /* For hack in open_test_file(). */

#include <errno.h>
#include <stdio.h>
#include <string.h>

/*
 * This file implements the overall test runner for libwebmdec tests,
 * along with shared test data and helper functions.  The test program
 * should be run from the top directory of the source tree in order for
 * the tests to find their data files.
 */

/*************************************************************************/
/************************* List of test runners **************************/
/*************************************************************************/

typedef int (*test_runner_t)(void);

static const test_runner_t tests[] = {
    test_open_callbacks,
    test_open_buffer,
    test_open_file,
    test_close,
    test_info,
    test_read,
    test_decode,
    test_tell,
    test_rewind,
    test_seek,
};

/*************************************************************************/
/***************************** Main routine ******************************/
/*************************************************************************/

int main(int argc, char **argv)
{
    int pass = 1;

    for (unsigned int i = 0; i < lenof(tests); i++) {
        test_runner_t test = tests[i];
        pass &= (*test)();
    }
    return pass ? 0 : 1;
}

/*************************************************************************/
/*************************** Shared test data ****************************/
/*************************************************************************/

const double timestamps[][2] = {
    /* Video, audio */
    { -1.000, 0.000 },
    { 0.003, -1.000 },
    { -1.000, 0.003 },
    { -1.000, 0.016 },
    { 0.036, -1.000 },
    { -1.000, 0.039 },
    { -1.000, 0.063 },
    { 0.070, -1.000 },
    { -1.000, 0.086 },
    { 0.103, -1.000 },
    { -1.000, 0.109 },
    { -1.000, 0.132 },
    { 0.136, -1.000 },
    { -1.000, 0.155 },
    { 0.170, -1.000 },
    { -1.000, 0.179 },
    { -1.000, 0.202 },
    { 0.203, -1.000 },
    { -1.000, 0.225 },
    { 0.236, -1.000 },
    { -1.000, 0.248 },
    { 0.270, -1.000 },
    { -1.000, 0.271 },
    { -1.000, 0.295 },
    { 0.303, -1.000 },
    { -1.000, 0.318 },
    { -1.000, 0.331 },
    { -1.000, 0.334 },
    /* Fencepost */
    { -1.000, -1.000 }
};

const double timestamps_no_audio[] = {
    0.000,
    0.033,
    0.067,
    0.100,
    0.133,
    0.167,
    0.200,
    0.233,
    0.267,
    0.300,
    /* Fencepost */
    -1.000
};

/*************************************************************************/
/************************* Test helper functions *************************/
/*************************************************************************/

/*
 * These are identical to the stdio callbacks used by webmdec_open_with_file().
 * We copy them here so we can use them regardless of whether stdio support
 * is compiled into the library.
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
}

/*-----------------------------------------------------------------------*/

webmdec_t *open_test_file(const char *path)
{
    static const webmdec_callbacks_t callbacks = {
        .length = file_length,
        .tell   = file_tell,
        .seek   = file_seek,
        .read   = file_read,
        .close  = file_close,
    };

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        perror(path);
        return NULL;
    }

    webmdec_t *handle = webmdec_open_from_callbacks(
        callbacks, fp, WEBMDEC_OPEN_ANY, NULL);
    if (!handle) {
        fclose(fp);
        return NULL;
    }

    return handle;
}

/*-----------------------------------------------------------------------*/

webmdec_t *open_test_file_unseekable(const char *path)
{
    static const webmdec_callbacks_t callbacks = {
        .read   = file_read,
        .close  = file_close,
    };

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        perror(path);
        return NULL;
    }

    webmdec_t *handle = webmdec_open_from_callbacks(
        callbacks, fp, WEBMDEC_OPEN_ANY, NULL);
    if (!handle) {
        fclose(fp);
        return NULL;
    }

    return handle;
}

/*************************************************************************/
/*************************************************************************/
