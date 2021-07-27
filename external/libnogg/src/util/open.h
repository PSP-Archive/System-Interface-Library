/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2019 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#ifndef NOGG_SRC_UTIL_OPEN_H
#define NOGG_SRC_UTIL_OPEN_H

/*************************************************************************/
/*************************************************************************/

/* Parameter structure passed to open_common(). */
typedef struct open_params_t {
    /* Callback set.  The callbacks will be copied into the decoder handle,
     * so this pointer does not need to remain live beyond the open call. */
    const vorbis_callbacks_t *callbacks;
    /* Opaque pointer to pass to callbacks. */
    void *callback_data;
    /* Decoder options (VORBIS_OPTION_*). */
    unsigned int options;
    /* Create a packet-mode decoder (true) or standard Ogg parser (false)? */
    bool packet_mode;
    /* Vorbis ID and setup packets, for packet mode decoders.  Ignored for
     * standard decoders. */
    const void *id_packet;
    int32_t id_packet_len;
    const void *setup_packet;
    int32_t setup_packet_len;
} open_params_t;

/**
 * open_common:  Common logic for creating a new decoder instance.
 *
 * [Parameters]
 *     params: Parameters for the open operation.
 *     error_ret: Pointer to variable to receive the error code from the
 *         operation (always VORBIS_NO_ERROR on success).  May be NULL if
 *         the error code is not needed.
 * [Return value]
 *     Newly-created handle, or NULL on error.
 */
#define open_common INTERNAL(open_common)
extern vorbis_t *open_common(const open_params_t *params,
                             vorbis_error_t *error_ret);

/*************************************************************************/
/*************************************************************************/

#endif  // NOGG_SRC_UTIL_OPEN_H
