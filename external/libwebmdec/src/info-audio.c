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


int webmdec_audio_channels(const webmdec_t *handle)
{
    return handle->audio_params.channels;
}


int webmdec_audio_rate(const webmdec_t *handle)
{
    return (int)handle->audio_params.rate;
}
