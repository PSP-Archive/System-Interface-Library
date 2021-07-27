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


/**
 * append_data:  Helper function that appends data to a buffer.
 *
 * [Parameters]
 *     buffer_ptr: Pointer to buffer pointer.
 *     buflen_ptr: Pointer to buffer length, in bytes.
 *     data: Data to append to *buffer_ptr.
 *     length: Length of data, in bytes.
 * [Return value]
 *     True (nonzero) on success, false (zero) if out of memory.
 */
static int append_data(void **buffer_ptr, int *buflen_ptr,
                       const void *data, int length)
{
    void *new_buffer = realloc(*buffer_ptr, *buflen_ptr + length);
    if (!new_buffer) {
        return 0;
    }
    memcpy((char *)new_buffer + *buflen_ptr, data, length);
    *buffer_ptr = new_buffer;
    *buflen_ptr += length;
    return 1;
}


int webmdec_read_frame(
    webmdec_t *handle,
    const void **video_data_ret, int *video_length_ret, double *video_time_ret,
    const void **audio_data_ret, int *audio_length_ret, double *audio_time_ret)
{
    if (video_data_ret && !video_length_ret) {
        handle->last_error = WEBMDEC_ERROR_INVALID_ARGUMENT;
        return 0;
    }
    if (video_length_ret && handle->video_track < 0) {
        handle->last_error = WEBMDEC_ERROR_STREAM_NO_TRACKS;
        return 0;
    }
    if (audio_data_ret && !audio_length_ret) {
        handle->last_error = WEBMDEC_ERROR_INVALID_ARGUMENT;
        return 0;
    }
    if (audio_length_ret && handle->audio_track < 0) {
        handle->last_error = WEBMDEC_ERROR_STREAM_NO_TRACKS;
        return 0;
    }

    free(handle->video_data);
    free(handle->audio_data);
    handle->video_data = NULL;
    handle->audio_data = NULL;

    void *video_data = NULL;
    int video_length = 0;
    double video_timestamp = -1;  /* -1 indicates we haven't seen it yet. */
    void *audio_data = NULL;
    int audio_length = 0;
    double audio_timestamp = -1;

    /* Loop over packets in the stream until we've collected a video or
     * audio frame. */
    while ((video_timestamp < 0 && audio_timestamp < 0)
           && !handle->read_error_flag && !handle->eos_flag)
    {

        /* Pull in the next packet from the stream. */
        nestegg_packet *packet;
        const int result = nestegg_read_packet(handle->demuxer, &packet);
        if (result <= 0) {
            if (result == 0) {
                handle->eos_flag = 1;
            } else {
                handle->read_error_flag = 1;
            }
            break;
        }

        /* Update the current stream timestamp. */
        uint64_t tstamp_ns;
        if (nestegg_packet_tstamp(packet, &tstamp_ns) != 0) {
            tstamp_ns = 0;  /* Uh, I dunno?  Should be impossible. */
        }
        handle->current_timestamp = tstamp_ns * 1.0e-9;

        /* See if this packet is on a track we're interested in. */
        int is_video = 0, is_audio = 0;
        int track;
        if (nestegg_packet_track(packet, (unsigned int *)&track) != 0) {
            track = -1;
        }
        if (track == handle->video_track) {
            is_video = 1;
        } else if (handle->audio_track >= 0 && track == handle->audio_track) {
            is_audio = 1;
        }

        /* If it's a packet we want, copy out all relevant information. */
        if ((is_video && video_data_ret) || (is_audio && audio_data_ret)) {
            void **data_ptr = is_video ? &video_data : &audio_data;
            int *length_ptr = is_video ? &video_length : &audio_length;
            double *timestamp_ptr =
                is_video ? &video_timestamp : &audio_timestamp;

            *timestamp_ptr = handle->current_timestamp;

            unsigned int chunks;
            if (nestegg_packet_count(packet, &chunks) != 0) {
                chunks = 0;  /* How could this possibly fail?! */
            }
            for (unsigned int i = 0; i < chunks; i++) {
                unsigned char *chunk_data;
                size_t chunk_length;
                if (nestegg_packet_data(packet, i,
                                        &chunk_data, &chunk_length) != 0) {
                    continue;
                }
                if (!append_data(data_ptr, length_ptr,
                                 chunk_data, (int)chunk_length)) {
                    nestegg_free_packet(packet);
                    goto error_oom;
                }
            }
        }

        /* Free the packet's data. */
        nestegg_free_packet(packet);

    }  /* while (!handle->read_error_flag && !handle->eos_flag) */

    if (handle->read_error_flag) {
        handle->last_error = WEBMDEC_ERROR_STREAM_READ_FAILURE;
        return 0;
    }
    if (handle->eos_flag && video_timestamp < 0 && audio_timestamp < 0) {
        handle->last_error = WEBMDEC_ERROR_STREAM_END;
        return 0;
    }

    if (video_data_ret) {
        *video_data_ret = video_data;
    }
    if (video_length_ret) {
        *video_length_ret = video_length;
    }
    if (video_time_ret) {
        *video_time_ret = video_timestamp;
    }
    if (audio_data_ret) {
        *audio_data_ret = audio_data;
    }
    if (audio_length_ret) {
        *audio_length_ret = audio_length;
    }
    if (audio_time_ret) {
        *audio_time_ret = audio_timestamp;
    }

    handle->video_data = video_data;
    handle->audio_data = audio_data;
    return 1;

  error_oom:
    free(video_data);
    free(audio_data);
    handle->last_error = WEBMDEC_ERROR_INSUFFICIENT_RESOURCES;
    return 0;
}
