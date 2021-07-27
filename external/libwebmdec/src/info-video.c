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


int webmdec_video_width(const webmdec_t *handle)
{
    return handle->video_params.width;
}


int webmdec_video_height(const webmdec_t *handle)
{
    return handle->video_params.height;
}


double webmdec_video_rate(const webmdec_t *handle)
{
    return handle->video_rate;
}
