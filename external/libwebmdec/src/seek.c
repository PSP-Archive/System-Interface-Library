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


int webmdec_seek(webmdec_t *handle, double timestamp)
{
    if (timestamp < 0.0) {
        handle->last_error = WEBMDEC_ERROR_INVALID_ARGUMENT;
        return 0;
    }
    if (handle->data_length < 0) {
        handle->last_error = WEBMDEC_ERROR_STREAM_NOT_SEEKABLE;
        return 0;
    }

    if (nestegg_track_seek(handle->demuxer, handle->video_track,
                           (uint64_t)(timestamp*1.0e9 + 0.5)) != 0) {
        handle->last_error = WEBMDEC_ERROR_STREAM_READ_FAILURE;
        return 0;
    }

    if (handle->audio_decoder) {
        vorbis_decoder_reset(handle->audio_decoder);
    }

    handle->current_timestamp = timestamp;
    handle->eos_flag = 0;
    return 1;
}
