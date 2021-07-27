/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/movie/webm.c: WebM decoding routines.
 */

#include "src/base.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/movie/webm.h"
#include "src/sysdep.h"
#include "src/utility/yuv2rgb.h"

#ifdef SIL_MOVIE_INCLUDE_WEBM
# include <webmdec.h>
# define UNUSED_IF_NO_WEBM  /*nothing*/
#else
# define UNUSED_IF_NO_WEBM  UNUSED
#endif

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

#ifdef SIL_MOVIE_INCLUDE_WEBM

/* File info for callbacks. */
typedef struct FileInfo FileInfo;
struct FileInfo {
    SysFile *fh;
    int64_t start;
    int64_t length;
};

/* Decoder handle structure. */
struct WebMDecodeHandle {
    /* Source file handle and data position/length.  We need separate
     * file handles for video and audio since we use separate decoders. */
    FileInfo file_video, file_audio;

    /* libwebmdec handles.  We use separate handles for video and audio
     * decoding so they can be done asynchronously. */
    webmdec_t *video_decoder;
    webmdec_t *audio_decoder;  // NULL if no audio in the stream.

    /* Buffered audio data not yet returned to the caller. */
    int16_t *audio_buffer;
    int audio_buffer_len;

    /* Should we smooth U/V planes when upsampling? */
    uint8_t smooth_uv;
};

#endif  // SIL_MOVIE_INCLUDE_WEBM

/*************************************************************************/
/************************* libwebmdec callbacks **************************/
/*************************************************************************/

#ifdef SIL_MOVIE_INCLUDE_WEBM

/*-----------------------------------------------------------------------*/

static long webmdec_length_cb(void *opaque)
{
    FileInfo *info = (FileInfo *)opaque;
    return (long)info->length;
}

/*-----------------------------------------------------------------------*/

static long webmdec_tell_cb(void *opaque)
{
    FileInfo *info = (FileInfo *)opaque;
    return (long)(sys_file_tell(info->fh) - info->start);
}

/*-----------------------------------------------------------------------*/

/* Currently never called. */
static void webmdec_seek_cb(void *opaque, long offset)            // NOTREACHED
{                                                                 // NOTREACHED
    FileInfo *info = (FileInfo *)opaque;                          // NOTREACHED
    sys_file_seek(info->fh, info->start + offset, FILE_SEEK_SET); // NOTREACHED
}                                                                 // NOTREACHED

/*-----------------------------------------------------------------------*/

static long webmdec_read_cb(void *opaque, void *buffer, long length)
{
    FileInfo *info = (FileInfo *)opaque;
    return sys_file_read(info->fh, buffer, length);
}

/*-----------------------------------------------------------------------*/

static const webmdec_callbacks_t webmdec_callbacks = {
    .length = webmdec_length_cb,
    .tell   = webmdec_tell_cb,
    .seek   = webmdec_seek_cb,
    .read   = webmdec_read_cb,
};

/*-----------------------------------------------------------------------*/

#endif  // SIL_MOVIE_INCLUDE_WEBM

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

WebMDecodeHandle *movie_webm_open(
    UNUSED_IF_NO_WEBM SysFile *fh,
    UNUSED_IF_NO_WEBM int64_t offset,
    UNUSED_IF_NO_WEBM int64_t length,
    UNUSED_IF_NO_WEBM int smooth_chroma)
{
#ifdef SIL_MOVIE_INCLUDE_WEBM

    WebMDecodeHandle *handle = mem_alloc(sizeof(*handle), 0, 0);
    if (!handle) {
        DLOG("No memory for decode handle");
        goto error_return;
    }
    handle->smooth_uv = smooth_chroma;

    webmdec_error_t error;

    handle->file_video.fh = fh;
    handle->file_video.start = offset;
    handle->file_video.length = length;
    sys_file_seek(handle->file_video.fh, handle->file_video.start,
                  FILE_SEEK_SET);
    handle->video_decoder = webmdec_open_from_callbacks(
        webmdec_callbacks, &handle->file_video, WEBMDEC_OPEN_VIDEO, &error);
    if (!handle->video_decoder) {
        DLOG("Failed to open video decoder (%d)", error);
        goto error_free_handle;
    }

    /* Only open an audio decoder if we'll need it. */
    if (webmdec_audio_channels(handle->video_decoder) > 0) {
        handle->file_audio.fh = sys_file_dup(fh);
        if (!handle->file_audio.fh) {
            DLOG("Failed to dup file handle for audio: %s", sys_last_errstr());
            goto error_close_video_decoder;
        }
        handle->file_audio.start = offset;
        handle->file_audio.length = length;
        sys_file_seek(handle->file_audio.fh, handle->file_audio.start,
                      FILE_SEEK_SET);
        handle->audio_decoder = webmdec_open_from_callbacks(
            webmdec_callbacks, &handle->file_audio, WEBMDEC_OPEN_AUDIO,
            &error);
        if (!handle->audio_decoder) {
            DLOG("Failed to open audio decoder (%d)", error);
            sys_file_close(handle->file_audio.fh);
            goto error_close_video_decoder;
        }
    } else {
        handle->file_audio.fh = NULL;
        handle->audio_decoder = NULL;
    }

    handle->audio_buffer = NULL;
    handle->audio_buffer_len = 0;

    return handle;

  error_close_video_decoder:
    webmdec_close(handle->video_decoder);
  error_free_handle:
    mem_free(handle);
  error_return:
    return NULL;

#else  // !SIL_MOVIE_INCLUDE_WEBM
    return NULL;
#endif
}

/*-----------------------------------------------------------------------*/

void movie_webm_close(WebMDecodeHandle *handle)
{
    PRECOND(handle != NULL);

#ifdef SIL_MOVIE_INCLUDE_WEBM
    mem_free(handle->audio_buffer);
    if (handle->audio_decoder) {
        webmdec_close(handle->audio_decoder);
        sys_file_close(handle->file_audio.fh);
    }
    webmdec_close(handle->video_decoder);
    sys_file_close(handle->file_video.fh);
    mem_free(handle);
#endif
}

/*-----------------------------------------------------------------------*/

double movie_webm_framerate(WebMDecodeHandle *handle)
{
    PRECOND(handle != NULL);

#ifdef SIL_MOVIE_INCLUDE_WEBM
    return webmdec_video_rate(handle->video_decoder);
#else
    return 0;
#endif
}

/*-----------------------------------------------------------------------*/

int movie_webm_width(WebMDecodeHandle *handle)
{
    PRECOND(handle != NULL);

#ifdef SIL_MOVIE_INCLUDE_WEBM
    return webmdec_video_width(handle->video_decoder);
#else
    return 0;
#endif
}

/*-----------------------------------------------------------------------*/

int movie_webm_height(WebMDecodeHandle *handle)
{
    PRECOND(handle != NULL);

#ifdef SIL_MOVIE_INCLUDE_WEBM
    return webmdec_video_height(handle->video_decoder);
#else
    return 0;
#endif
}

/*-----------------------------------------------------------------------*/

int movie_webm_audio_channels(WebMDecodeHandle *handle)
{
    PRECOND(handle != NULL);

#ifdef SIL_MOVIE_INCLUDE_WEBM
    if (handle->audio_decoder) {
        return webmdec_audio_channels(handle->audio_decoder);
    } else {
        return 0;
    }
#else
    return 0;
#endif
}

/*-----------------------------------------------------------------------*/

int movie_webm_audio_rate(WebMDecodeHandle *handle)
{
    PRECOND(handle != NULL);

#ifdef SIL_MOVIE_INCLUDE_WEBM
    if (handle->audio_decoder) {
        return webmdec_audio_rate(handle->audio_decoder);
    } else {
        return 0;
    }
#else
    return 0;
#endif
}

/*-----------------------------------------------------------------------*/

int movie_webm_get_video(WebMDecodeHandle *handle,
                         UNUSED_IF_NO_WEBM void *buffer)
{
    PRECOND(handle != NULL);

#ifdef SIL_MOVIE_INCLUDE_WEBM

    const void *video_data;
    if (!webmdec_decode_frame(handle->video_decoder, &video_data, NULL, NULL,
                              NULL, NULL)) {
        const webmdec_error_t error =
            webmdec_last_error(handle->video_decoder);
        if (error != WEBMDEC_ERROR_STREAM_END) {
            DLOG("Failed to decode video frame (%d)", error);
        }
        return 0;
    }

    const int width = webmdec_video_width(handle->video_decoder);
    const int height = webmdec_video_height(handle->video_decoder);
    const uint8_t *Y = video_data;
    const uint8_t *U = Y + width*height;
    const uint8_t *V = U + (width/2)*(height/2);
    const uint8_t *src_planes[3] = {Y, U, V};
    int src_strides[3] = {width, width/2, width/2};
    yuv2rgb(src_planes, src_strides, buffer, width, width, height,
            handle->smooth_uv);

    return 1;

#else  // !SIL_MOVIE_INCLUDE_WEBM
    return 0;
#endif
}

/*-----------------------------------------------------------------------*/

int movie_webm_get_audio(WebMDecodeHandle *handle,
                         UNUSED_IF_NO_WEBM void *buffer,
                         UNUSED_IF_NO_WEBM int num_samples)
{
    PRECOND(handle != NULL);

#ifdef SIL_MOVIE_INCLUDE_WEBM

    if (!handle->audio_decoder) {
        return 0;
    }

    const int num_channels = webmdec_audio_channels(handle->audio_decoder);
    int16_t *dest = (int16_t *)buffer;
    int samples_gotten = 0;

    if (handle->audio_buffer) {
        const int samples_to_take = min(handle->audio_buffer_len, num_samples);
        memcpy(dest, handle->audio_buffer, samples_to_take * num_channels * 2);
        samples_gotten += samples_to_take;
        dest += samples_to_take * num_channels;
        handle->audio_buffer_len -= samples_to_take;
        if (handle->audio_buffer_len > 0) {
            memmove(handle->audio_buffer,
                    handle->audio_buffer + (samples_to_take * num_channels),
                    handle->audio_buffer_len * num_channels * 2);
        } else {
            mem_free(handle->audio_buffer);
            handle->audio_buffer = NULL;
        }
    }

    while (samples_gotten < num_samples) {
        const float *audio_data;
        int audio_length;
        if (!webmdec_decode_frame(handle->audio_decoder, NULL, NULL,
                                  &audio_data, &audio_length, NULL)) {
            const webmdec_error_t error =
                webmdec_last_error(handle->audio_decoder);
            if (error != WEBMDEC_ERROR_STREAM_END) {
                DLOG("Failed to decode audio frame (%d)", error);
            }
            break;
        }

        const int samples_to_take =
            min(audio_length, num_samples - samples_gotten);
        for (int i = 0; i < samples_to_take * num_channels; i++, dest++) {
            *dest = iroundf(bound(audio_data[i], -1.0f, 1.0f) * 32767);
        }
        samples_gotten += samples_to_take;
        audio_data += samples_to_take * num_channels;
        audio_length -= samples_to_take;

        if (audio_length > 0) {
            handle->audio_buffer = mem_alloc(
                audio_length * num_channels * 2, 0, MEM_ALLOC_TEMP);
            if (UNLIKELY(!handle->audio_buffer)) {
                DLOG("No memory to save %d audio samples, audio will be lost",
                     audio_length);
            } else {
                for (int i = 0; i < audio_length * num_channels; i++) {
                    handle->audio_buffer[i] =
                        iroundf(bound(audio_data[i], -1.0f, 1.0f) * 32767);
                }
                handle->audio_buffer_len = audio_length;
            }
        }

    }  // while (samples_gotten < num_samples)

    return samples_gotten;

#else
    return 0;
#endif
}

/*************************************************************************/
/*************************************************************************/
