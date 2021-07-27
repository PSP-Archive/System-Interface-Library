/*
 * libwebmdec: a decoder library for WebM audio/video streams
 * Copyright (c) 2014-2019 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include "include/webmdec.h"
#include "include/internal.h"

#include <stdlib.h>
#include <string.h>

/*-----------------------------------------------------------------------*/

/* Data structure encapsulating the buffer, its length, and the current
 * read position, used as the opaque argument to callback functions. */
typedef struct buffer_data_t {
    const char *buffer;  /* char instead of void so we can index it. */
    long length;
    long position;
} buffer_data_t;

static long buffer_length(void *opaque)
{
    buffer_data_t *buffer_data = (buffer_data_t *)opaque;
    return buffer_data->length;
}

static long buffer_tell(void *opaque)
{
    buffer_data_t *buffer_data = (buffer_data_t *)opaque;
    return buffer_data->position;
}

static void buffer_seek(void *opaque, long offset)
{
    buffer_data_t *buffer_data = (buffer_data_t *)opaque;
    buffer_data->position = offset;
}

static long buffer_read(void *opaque, void *buffer, long length)
{
    buffer_data_t *buffer_data = (buffer_data_t *)opaque;
    memcpy(buffer, buffer_data->buffer + buffer_data->position, length);
    buffer_data->position += length;
    return length;
}

static void buffer_close(void *opaque)
{
    buffer_data_t *buffer_data = (buffer_data_t *)opaque;
    free(buffer_data);
}

static const webmdec_callbacks_t buffer_callbacks = {
    .length = buffer_length,
    .tell   = buffer_tell,
    .seek   = buffer_seek,
    .read   = buffer_read,
    /* We could just use "free" instead of defining our own function, but
     * this makes the intent clearer. */
    .close  = buffer_close,
};

/*-----------------------------------------------------------------------*/

extern webmdec_t *webmdec_open_from_buffer(
    const void *buffer, long length, webmdec_open_mode_t open_mode,
    webmdec_error_t *error_ret)
{
    if (!buffer || length < 0) {
        if (error_ret) {
            *error_ret = WEBMDEC_ERROR_INVALID_ARGUMENT;
        }
        return NULL;
    }

    buffer_data_t *buffer_data = malloc(sizeof(*buffer_data));
    if (!buffer_data) {
        if (error_ret) {
            *error_ret = WEBMDEC_ERROR_INSUFFICIENT_RESOURCES;
        }
        return NULL;
    }
    buffer_data->buffer = buffer;
    buffer_data->length = length;
    buffer_data->position = 0;

    webmdec_t *handle = webmdec_open_from_callbacks(
        buffer_callbacks, buffer_data, open_mode, error_ret);
    if (!handle) {
        free(buffer_data);
    }
    return handle;
}
