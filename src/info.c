/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include "include/nogg.h"
#include "include/internal.h"


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
    return handle->length;
}
