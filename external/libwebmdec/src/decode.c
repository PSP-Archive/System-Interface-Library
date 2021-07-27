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

#ifdef DECODE_VIDEO
# include <vpx/vpx_decoder.h>
#endif


int webmdec_decode_frame(
    webmdec_t *handle, const void **video_data_ret, double *video_time_ret,
    const float **audio_data_ret, int *audio_samples_ret,
    double *audio_time_ret)
{
    if (video_data_ret && handle->video_track < 0) {
        handle->last_error = WEBMDEC_ERROR_STREAM_NO_TRACKS;
        return 0;
    }
    if (audio_data_ret && !audio_samples_ret) {
        handle->last_error = WEBMDEC_ERROR_INVALID_ARGUMENT;
        return 0;
    }
    if (audio_data_ret && handle->audio_track < 0) {
        handle->last_error = WEBMDEC_ERROR_STREAM_NO_TRACKS;
        return 0;
    }
    if (video_data_ret && !handle->video_decoder) {
#ifdef DECODE_VIDEO
        handle->last_error = WEBMDEC_ERROR_DECODE_SETUP_FAILURE;
#else
        handle->last_error = WEBMDEC_ERROR_DISABLED_FUNCTION;
#endif
        return 0;
    }
    if (audio_data_ret && !handle->audio_decoder) {
#ifdef DECODE_AUDIO
        handle->last_error = WEBMDEC_ERROR_DECODE_SETUP_FAILURE;
#else
        handle->last_error = WEBMDEC_ERROR_DISABLED_FUNCTION;
#endif
        return 0;
    }

    const void *raw_video = NULL, *raw_audio = NULL;
    int raw_video_length = 0, raw_audio_length = 0;
    double video_time, audio_time;
    if (!webmdec_read_frame(handle,
                            video_data_ret ? &raw_video : NULL,
                            video_data_ret ? &raw_video_length : NULL,
                            &video_time,
                            audio_data_ret ? &raw_audio : NULL,
                            audio_data_ret ? &raw_audio_length : NULL,
                            &audio_time)) {
        return 0;
    }

#ifdef DECODE_VIDEO
    if (raw_video) {
        if (vpx_codec_decode(handle->video_decoder, raw_video,
                             raw_video_length, NULL, 0) != VPX_CODEC_OK) {
            handle->last_error = WEBMDEC_ERROR_DECODE_FAILURE;
            return 0;
        }
        free(handle->video_data);
        handle->video_data = NULL;
        vpx_codec_iter_t iter = NULL;
        vpx_image_t *image = vpx_codec_get_frame(handle->video_decoder, &iter);
        if (image) {
            if (image->fmt != VPX_IMG_FMT_I420) {
                handle->last_error = WEBMDEC_ERROR_UNSUPPORTED_PIXEL_FORMAT;
                return 0;
            }
            const int width = image->d_w, height = image->d_h;
            char *yuv_data = malloc(width*height + 2*((width/2)*(height/2)));
            if (!yuv_data) {
                handle->last_error = WEBMDEC_ERROR_INSUFFICIENT_RESOURCES;
                return 0;
            }
            handle->video_data = yuv_data;
            for (int y = 0; y < height; y++, yuv_data += width) {
                memcpy(yuv_data, (image->planes[VPX_PLANE_Y]
                                  + y*image->stride[VPX_PLANE_Y]), width);
            }
            for (int y = 0; y < height/2; y++, yuv_data += width/2) {
                memcpy(yuv_data, (image->planes[VPX_PLANE_U]
                                  + y*image->stride[VPX_PLANE_U]), width/2);
            }
            for (int y = 0; y < height/2; y++, yuv_data += width/2) {
                memcpy(yuv_data, (image->planes[VPX_PLANE_V]
                                  + y*image->stride[VPX_PLANE_V]), width/2);
            }
        }
    }
#endif

    int audio_samples = 0;
#ifdef DECODE_AUDIO
    if (raw_audio) {
        if (!vorbis_decoder_decode(
                handle->audio_decoder, raw_audio, raw_audio_length)) {
            handle->last_error = WEBMDEC_ERROR_DECODE_FAILURE;
            return 0;
        }
        audio_samples =
            vorbis_decoder_available_samples(handle->audio_decoder);
        if (audio_samples > 0) {
            const int sample_size = 4 * handle->audio_params.channels;
            free(handle->audio_data);
            handle->audio_data = malloc(audio_samples * sample_size);
            if (!handle->audio_data) {
                handle->last_error = WEBMDEC_ERROR_INSUFFICIENT_RESOURCES;
                return 0;
            }
            vorbis_decoder_get_pcm(
                handle->audio_decoder, handle->audio_data, audio_samples);
        }
    }
#endif

    if (video_data_ret) {
        *video_data_ret = handle->video_data;
    }
    if (video_time_ret) {
        *video_time_ret = video_time;
    }
    if (audio_data_ret) {
        *audio_data_ret = handle->audio_data;
    }
    if (audio_samples_ret) {
        *audio_samples_ret = audio_samples;
    }
    if (audio_time_ret) {
        *audio_time_ret = audio_time;
    }

    return 1;
}
