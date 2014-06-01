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

#include <math.h>


int vorbis_seek_to_time(vorbis_t *handle, double timestamp)
{
    return vorbis_seek(handle, (int64_t)round(timestamp * handle->rate));
}
