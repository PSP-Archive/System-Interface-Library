/*
 * libwebmdec: a decoder library for WebM audio/video streams
 * Copyright (c) 2014-2019 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

/*
 * This file implements webmdec_open_with_callbacks(), which is treated as
 * the "base" open function for the library; all other open functions are
 * implemented by calling this function with an appropriate set of
 * callbacks.
 */

#include "include/webmdec.h"
#include "include/internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#ifdef DECODE_VIDEO
# include <vpx/vp8dx.h>
# include <vpx/vpx_decoder.h>
#endif

/*************************************************************************/
/********************* libnestegg callback functions *********************/
/*************************************************************************/

/*
 * libnestegg uses a slightly different I/O interface than our callbacks,
 * so we need to translate.
 */

static int nestegg_read(void *buffer, size_t length, void *userdata)
{
    webmdec_t *handle = (webmdec_t *)userdata;
    if (handle->data_length >= 0) {
        const long current_pos =
            (*handle->callbacks.tell)(handle->callback_data);
        if ((long)length > handle->data_length - current_pos) {
            return 0;
        }
    }
    const long bytes_read =
        (*handle->callbacks.read)(handle->callback_data, buffer, (long)length);
    if (bytes_read != (long)length) {
        if (handle->data_length >= 0) {
            /* This should never fail; treat failure as a fatal error. */
            handle->read_error_flag = 1;
            return -1;
        } else {
            /* We probably just hit the end of the stream. */
            return 0;
        }
    }
    return 1;
}

static int nestegg_seek(int64_t offset, int whence, void *userdata)
{
    webmdec_t *handle = (webmdec_t *)userdata;
    if (handle->data_length < 0) {
        return -1;
    }
    if (whence == NESTEGG_SEEK_CUR) {
        offset += (*handle->callbacks.tell)(handle->callback_data);
    } else if (whence == NESTEGG_SEEK_END) {
        offset += handle->data_length;
    }
    (*handle->callbacks.seek)(handle->callback_data, (long)offset);
    return 0;
}

static int64_t nestegg_tell(void *userdata)
{
    webmdec_t *handle = (webmdec_t *)userdata;
    return (int64_t)((*handle->callbacks.tell)(handle->callback_data));
}

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

#ifdef DECODE_VIDEO

/**
 * init_video_decoder:  Create a video decoder for the given stream,
 * returning whether the creation succeeded.
 */
static int init_video_decoder(webmdec_t *handle)
{
    vpx_codec_iface_t *interface;
    const int codec =
        nestegg_track_codec_id(handle->demuxer, handle->video_track);
    switch (codec) {
        case NESTEGG_CODEC_VP8: interface = vpx_codec_vp8_dx(); break;
        case NESTEGG_CODEC_VP9: interface = vpx_codec_vp9_dx(); break;
        default: return 0;  /* Not supported. */
    }

    vpx_codec_dec_cfg_t config = {
        .threads = 1,
        .w = handle->video_params.width,
        .h = handle->video_params.height,
    };

    handle->video_decoder = malloc(sizeof(*handle->video_decoder));
    if (!handle->video_decoder) {
        return 0;
    }
    int status = vpx_codec_dec_init(handle->video_decoder, interface, &config,
                                    VPX_CODEC_USE_ERROR_CONCEALMENT);
    if (status == VPX_CODEC_INCAPABLE) {
        status = vpx_codec_dec_init(handle->video_decoder, interface, &config,
                                    0);
    }
    if (status != VPX_CODEC_OK) {
        free(handle->video_decoder);
        handle->video_decoder = NULL;
        return 0;
    }

    return 1;
}

#endif  // DECODE_VIDEO

/*-----------------------------------------------------------------------*/

#ifdef DECODE_AUDIO

/**
 * init_audio_decoder:  Create a audio decoder for the given stream,
 * returning whether the creation succeeded.
 */
static int init_audio_decoder(webmdec_t *handle)
{
    const int codec =
        nestegg_track_codec_id(handle->demuxer, handle->audio_track);
    if (codec != NESTEGG_CODEC_VORBIS) {
        return 0;  /* Not supported. */
    }

    unsigned int header_count;
    if (nestegg_track_codec_data_count(handle->demuxer, handle->audio_track,
                                       &header_count) != 0) {
        header_count = 0;
    }
    if (header_count != 3) {  /* Vorbis has 3 header packets. */
        return 0;
    }
    unsigned char *id_data, *setup_data;
    size_t id_length, setup_length;
    if (nestegg_track_codec_data(handle->demuxer, handle->audio_track, 0,
                                 &id_data, &id_length) != 0) {
        return 0;
    }
    if (nestegg_track_codec_data(handle->demuxer, handle->audio_track, 2,
                                 &setup_data, &setup_length) != 0) {
        return 0;
    }

    handle->audio_decoder = vorbis_decoder_create(
        id_data, id_length, setup_data, setup_length);
    return handle->audio_decoder != NULL;
}

#endif  // DECODE_AUDIO

/*************************************************************************/
/*************************** Interface routine ***************************/
/*************************************************************************/

webmdec_t *webmdec_open_from_callbacks(
    webmdec_callbacks_t callbacks, void *opaque, webmdec_open_mode_t open_mode,
    webmdec_error_t *error_ret)
{
    webmdec_t *handle = NULL;
    webmdec_error_t error = WEBMDEC_NO_ERROR;

    /* Validate the function arguments. */
    if (!callbacks.read) {
        error = WEBMDEC_ERROR_INVALID_ARGUMENT;
        goto exit;
    }
    if (callbacks.length && (!callbacks.tell || !callbacks.read)) {
        error = WEBMDEC_ERROR_INVALID_ARGUMENT;
        goto exit;
    }
    int mode_ok = 0;
    switch (open_mode) {
      case WEBMDEC_OPEN_ANY:
      case WEBMDEC_OPEN_VIDEO:
      case WEBMDEC_OPEN_AUDIO:
        mode_ok = 1;
        break;
    }
    if (!mode_ok) {
        error = WEBMDEC_ERROR_INVALID_ARGUMENT;
        goto exit;
    }

    /* Allocate and initialize a handle structure. */
    handle = malloc(sizeof(*handle));
    if (!handle) {
        error = WEBMDEC_ERROR_INSUFFICIENT_RESOURCES;
        goto exit;
    }
    handle->callbacks = callbacks;
    handle->callback_data = opaque;
    if (handle->callbacks.length) {
        handle->data_length =
            (*handle->callbacks.length)(handle->callback_data);
    } else {
        handle->data_length = -1;
    }
    handle->read_error_flag = 0;
    handle->eos_flag = 0;
    handle->current_timestamp = 0.0;
    handle->video_data = NULL;
    handle->audio_data = NULL;

    /* Create a libnestegg handle for the stream. */
    const nestegg_io io = {
        .read = nestegg_read,
        .seek = nestegg_seek,
        .tell = nestegg_tell,
        .userdata = handle,
    };
    if (nestegg_init(&handle->demuxer, io, NULL, LONG_MAX) != 0) {
        error = WEBMDEC_ERROR_STREAM_INVALID;
        goto error_free_handle;
    }

    /* Look up the tracks in the file. */
    handle->video_track = -1;
    handle->audio_track = -1;
    unsigned int num_tracks;
    if (nestegg_track_count(handle->demuxer, &num_tracks) != 0) {
        num_tracks = 0;
    }
    for (unsigned int i = 0; i < num_tracks; i++) {
        const int type = nestegg_track_type(handle->demuxer, i);
        if (type == NESTEGG_TRACK_VIDEO) {
            if (handle->video_track < 0) {
                handle->video_track = (int)i;
            }
        } else if (type == NESTEGG_TRACK_AUDIO) {
            if (handle->audio_track < 0) {
                handle->audio_track = (int)i;
            }
        }
    }

    /* Make sure there's at least one track of the requested type(s). */
    switch (open_mode) {
      case WEBMDEC_OPEN_ANY:
        if (handle->video_track < 0 && handle->audio_track < 0) {
            error = WEBMDEC_ERROR_STREAM_NO_TRACKS;
            goto error_close_demuxer;
        }
        break;
      case WEBMDEC_OPEN_VIDEO:
        if (handle->video_track < 0) {
            error = WEBMDEC_ERROR_STREAM_NO_TRACKS;
            goto error_close_demuxer;
        }
        break;
      case WEBMDEC_OPEN_AUDIO:
        if (handle->audio_track < 0) {
            error = WEBMDEC_ERROR_STREAM_NO_TRACKS;
            goto error_close_demuxer;
        }
        break;
    }

    /* Look up the video and audio parameters. */
    if (handle->video_track >= 0) {
        if (nestegg_track_video_params(handle->demuxer, handle->video_track,
                                       &handle->video_params) != 0) {
            error = WEBMDEC_ERROR_STREAM_INVALID;
            goto error_close_demuxer;
        }
    } else {
        memset(&handle->video_params, 0, sizeof(handle->video_params));
    }
    uint64_t inv_framerate;
    if (nestegg_track_default_duration(handle->demuxer, handle->video_track,
                                       &inv_framerate) == 0
        && inv_framerate > 0)
    {
        double rate = 1.0e9 / inv_framerate;
        /* If it's very close to an integer or NTSC rate, round it off. */
        int rate_int = (int)rate;
        if (rate - rate_int > -0.001 && rate - rate_int < 0.001) {
            rate = rate_int;
        } else {
            rate_int = (int)(rate*1.001);
            if (rate*1.001 - rate_int > -0.001 && rate*1.001 - rate_int < 0.001) {
                rate = rate_int/1.001;
            }
        }
        handle->video_rate = rate;
    } else {
        handle->video_rate = 0;
    }
    if (handle->audio_track >= 0) {
        if (nestegg_track_audio_params(handle->demuxer, handle->audio_track,
                                       &handle->audio_params) != 0) {
            error = WEBMDEC_ERROR_STREAM_INVALID;
            goto error_close_demuxer;
        }
    } else {
        memset(&handle->audio_params, 0, sizeof(handle->audio_params));
    }

    /* Create decoder handles. */
    handle->video_decoder = NULL;
#ifdef DECODE_VIDEO
    if (open_mode != WEBMDEC_OPEN_AUDIO && handle->video_track >= 0) {
        if (!init_video_decoder(handle)) {
            error = WEBMDEC_ERROR_DECODE_SETUP_FAILURE;
            goto error_close_demuxer;
        }
    }
#endif

    /* Create an audio decoder handle for the audio stream (if one exists). */
    handle->audio_decoder = NULL;
#ifdef DECODE_AUDIO
    if (open_mode != WEBMDEC_OPEN_VIDEO && handle->audio_track >= 0) {
        if (!init_audio_decoder(handle)) {
            error = WEBMDEC_ERROR_DECODE_SETUP_FAILURE;
            goto error_close_video_decoder;
        }
    }
#endif

  exit:
    if (error_ret) {
        *error_ret = error;
    }
    return handle;

  error_close_video_decoder:
#ifdef DECODE_VIDEO
    if (handle->video_decoder) {
        vpx_codec_destroy(handle->video_decoder);
        free(handle->video_decoder);
    }
#endif
  error_close_demuxer:
    nestegg_destroy(handle->demuxer);
  error_free_handle:
    free(handle);
    handle = NULL;
    goto exit;
}

/*************************************************************************/
/*************************************************************************/
