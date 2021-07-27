/*
 * libwebmdec: a decoder library for WebM audio/video streams
 * Copyright (c) 2014-2019 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

/*
 * This header defines internal structures and other common data for the
 * libwebmdec library.  This header is not a public interface, and client
 * code MUST NOT make use of any declarations or definitions in this header.
 */

#ifndef WEBMDEC_INTERNAL_H
#define WEBMDEC_INTERNAL_H

#include <stddef.h>  // nestegg.h needs size_t
#include <nestegg/nestegg.h>

#ifdef DECODE_VIDEO
# include <vpx/vpx_codec.h>
#else
typedef void vpx_codec_ctx_t;
#endif

#ifdef DECODE_AUDIO
typedef struct vorbis_decoder_t vorbis_decoder_t;
#else
typedef void vorbis_decoder_t;
#endif

/*************************************************************************/
/****************************** Data types *******************************/
/*************************************************************************/

/*
 * Definition of the webmdec_t stream handle structure.
 */

struct webmdec_t {

    /******** Common state information. ********/

    /* Error code from the most recent failing operation. */
    webmdec_error_t last_error;

    /******** Stream callbacks and related data. ********/

    /* Callbacks used to access the stream data. */
    webmdec_callbacks_t callbacks;
    /* Opaque data pointer for callbacks. */
    void *callback_data;
    /* Length of stream data in bytes, or -1 if not a seekable stream. */
    long data_length;
    /* Flag: has an I/O error occurred on the stream? */
    unsigned char read_error_flag;

    /******** Decoding state. ********/

    /* Flag: have we hit the end of the stream? */
    unsigned char eos_flag;
    /* Current read/decode position, in seconds. */
    double current_timestamp;
    /* Video and audio data buffers returned from the last read or decode
     * operation.  These are saved only so that the buffers can be freed
     * on the next read/decode operation or when the stream is closed. */
    void *video_data;
    void *audio_data;

    /******** Handles for demuxer/decoder libraries. ********/

    /* Demuxer handle (libnestegg). */
    nestegg *demuxer;

    /* Video decoder handle (libvpx). */
    vpx_codec_ctx_t *video_decoder;

    /* Audio decoder handle (libnogg). */
    vorbis_decoder_t *audio_decoder;

    /******** Video and audio parameters. ********/

    /* Video and audio track numbers. */
    int video_track;
    int audio_track;  /* -1 if no audio in the stream. */
    /* Video and audio parameters. */
    nestegg_video_params video_params;
    nestegg_audio_params audio_params;
    /* Video frame rate, or 0 if unknown. */
    double video_rate;

};  /* struct webmdec_t */

/*************************************************************************/
/*********************** Vorbis decoding interface ***********************/
/*************************************************************************/

/*
 * These functions are used to implement Vorbis audio decoding; they can be
 * found in src/vorbis.c.
 */

/**
 * vorbis_decoder_create:  Create a new Vorbis decoder handle.
 *
 * [Parameters]
 *     id_header: Pointer to identification header data.
 *     id_length: Length of identification header data, in bytes.
 *     setup_header: Pointer to setup header data.
 *     setup_length: Length of setup header data, in bytes.
 * [Return value]
 *     Newly created decoder handle, or NULL on error.
 */
extern vorbis_decoder_t *vorbis_decoder_create(
    const unsigned char *id_header, int id_length,
    const unsigned char *setup_header, int setup_length);

/**
 * vorbis_decoder_destroy:  Destroy a Vorbis decoder handle.
 *
 * [Parameters]
 *     decoder: Decoder handle to destroy.  If NULL, this funciton does
 *         nothing.
 */
extern void vorbis_decoder_destroy(vorbis_decoder_t *decoder);

/**
 * vorbis_decoder_reset:  Reset a Vorbis decoder handle to its initial
 * state.  This function should be called after seeking to a new position.
 *
 * [Parameters]
 *     decoder: Decoder handle.
 */
extern void vorbis_decoder_reset(vorbis_decoder_t *decoder);

/**
 * vorbis_decoder_decode:  Decode Vorbis audio data.  Decoded data is
 * stored in the decoder handle and can be retrieved with
 * vorbis_decoder_get_pcm().
 *
 * [Parameters]
 *     decoder: Decoder handle.
 *     data: Vorbis data to decode.
 *     length: Length of Vorbis data, in bytes.
 * [Return value]
 *     True (nonzero) on successful decode, false (zero) on error.
 */
extern int vorbis_decoder_decode(
    vorbis_decoder_t *decoder, const void *data, int length);

/**
 * vorbis_decoder_available_samples:  Return the number of samples
 * available to be retrieved with vorbis_decoder_get_pcm().
 *
 * [Parameters]
 *     decoder: Decoder handle.
 * [Return value]
 *     Number of available PCM samples.
 */
extern int vorbis_decoder_available_samples(const vorbis_decoder_t *decoder);

/**
 * vorbis_decoder_get_pcm:  Retrieve decoded PCM samples.  The data is
 * returned as single-precision floating point values with interleaved
 * channels.
 *
 * If this function is called immediately after a call to
 * vorbis_decoder_available_samples(), a request for the number of samples
 * returned by that function (or less) is guaranteed to succeed.
 *
 * [Parameters]
 *     decoder: Decoder handle.
 *     buffer: Pointer to buffer into which to store PCM data.
 *     samples: Number of samples to retrieve.
 * [Return value]
 *     Number of samples returned.
 */
extern int vorbis_decoder_get_pcm(
    vorbis_decoder_t *decoder, float *buffer, int samples);

/*************************************************************************/
/*************************************************************************/

#endif  /* WEBMDEC_INTERNAL_H */
