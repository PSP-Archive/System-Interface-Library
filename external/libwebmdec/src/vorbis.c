/*
 * libwebmdec: a decoder library for WebM audio/video streams
 * Copyright (c) 2014-2019 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#ifdef DECODE_AUDIO  /* To the end of the file. */

#include "include/webmdec.h"
#include "include/internal.h"

#include <stdlib.h>
#include <string.h>

#include <nogg.h>

/*************************************************************************/
/************************ Decoder data structure *************************/
/*************************************************************************/

struct vorbis_decoder_t {
    /* libnogg handle. */
    vorbis_t *nogg;

    /* PCM data not yet returned to the caller. */
    float *pcm;
    int32_t num_samples;
};

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

vorbis_decoder_t *vorbis_decoder_create(
    const unsigned char *id_header, int id_length,
    const unsigned char *setup_header, int setup_length)
{
    if (id_header == NULL
     || id_length < 7
     || memcmp(id_header, "\1vorbis", 7) != 0
     || setup_header == NULL
     || setup_length < 7
     || memcmp(setup_header, "\5vorbis", 7) != 0)
    {
        goto error_return;
    }

    vorbis_decoder_t *decoder = malloc(sizeof(*decoder));
    if (!decoder) {
        goto error_return;
    }
    decoder->pcm = NULL;
    decoder->num_samples = 0;
    decoder->nogg = vorbis_open_packet(
        id_header, id_length, setup_header, setup_length,
        ((const vorbis_callbacks_t){.malloc = NULL, .free = NULL}), NULL,
        0, NULL);
    if (!decoder->nogg) {
        goto error_free_decoder;
    }

    return decoder;

  error_free_decoder:
    free(decoder);
  error_return:
    return NULL;
}

/*-----------------------------------------------------------------------*/

void vorbis_decoder_destroy(vorbis_decoder_t *decoder)
{
    free(decoder->pcm);
    vorbis_close(decoder->nogg);
    free(decoder);
}

/*-----------------------------------------------------------------------*/

void vorbis_decoder_reset(vorbis_decoder_t *decoder)
{
    /* Nothing to do. */
}

/*-----------------------------------------------------------------------*/

int vorbis_decoder_decode(
    vorbis_decoder_t *decoder, const void *data, int length)
{
    if (!vorbis_submit_packet(decoder->nogg, data, length, NULL)) {
        return 0;
    }

    const int channels = vorbis_channels(decoder->nogg);
    const int sample_size = 4 * channels;
    float read_buf[1024];
    int32_t new_samples;
    while ((new_samples = vorbis_read_float(
                decoder->nogg, read_buf, sizeof(read_buf) / sample_size,
                NULL)) > 0) {
        const int32_t total_samples = decoder->num_samples + new_samples;
        float *new_pcm = realloc(decoder->pcm, total_samples * sample_size);
        if (!new_pcm) {
            return 0;
        }
        memcpy(new_pcm + (decoder->num_samples * channels), read_buf,
               new_samples * sample_size);
        decoder->pcm = new_pcm;
        decoder->num_samples = total_samples;
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

int vorbis_decoder_available_samples(const vorbis_decoder_t *decoder)
{
    return decoder->num_samples;
}

/*-----------------------------------------------------------------------*/

int vorbis_decoder_get_pcm(
    vorbis_decoder_t *decoder, float *buffer, int samples)
{
    if (samples > decoder->num_samples) {
        samples = decoder->num_samples;
    }
    if (samples <= 0) {
        return 0;
    }

    const int channels = vorbis_channels(decoder->nogg);
    const int sample_size = 4 * channels;
    memcpy(buffer, decoder->pcm, samples * sample_size);
    decoder->num_samples -= samples;
    if (decoder->num_samples > 0) {
        memmove(decoder->pcm, decoder->pcm + (samples * channels),
                decoder->num_samples * sample_size);
        float *new_pcm =
            realloc(decoder->pcm, decoder->num_samples * sample_size);
        /* This realloc should never fail, but if it does, just leave the
         * larger buffer alone since it won't break anything. */
        if (new_pcm) {
            decoder->pcm = new_pcm;
        }
    } else {
        free(decoder->pcm);
        decoder->pcm = NULL;
    }

    return samples;
}

/*************************************************************************/
/*************************************************************************/

#endif  /* DECODE_AUDIO */
