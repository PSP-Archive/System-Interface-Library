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


void webmdec_close(webmdec_t *handle)
{
    if (!handle) {
        return;
    }

#ifdef DECODE_VIDEO
    if (handle->video_decoder) {
        vpx_codec_destroy(handle->video_decoder);
        free(handle->video_decoder);
    }
#endif

#ifdef DECODE_AUDIO
    if (handle->audio_decoder) {
        vorbis_decoder_destroy(handle->audio_decoder);
    }
#endif

    nestegg_destroy(handle->demuxer);

    if (handle->callbacks.close) {
        (*handle->callbacks.close)(handle->callback_data);
    }

    free(handle->video_data);
    free(handle->audio_data);
    free(handle);
}
