/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2015 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include "include/nogg.h"
#include "src/common.h"


int vorbis_channels(const vorbis_t *handle)
{
    return handle->channels;
}


int32_t vorbis_rate(const vorbis_t *handle)
{
    return handle->rate;
}


int64_t vorbis_length(const vorbis_t *handle)
{
    /* stb_vorbis_stream_length_in_samples() doesn't differentiate between
     * "empty file" and "error" in the return value, so use the error flag
     * to tell the difference. */
    (void) stb_vorbis_get_error(handle->decoder);
    const int64_t length = stb_vorbis_stream_length_in_samples(handle->decoder);
    if (stb_vorbis_get_error(handle->decoder) != VORBIS__no_error) {
        return -1;
    }
    return length;
}
