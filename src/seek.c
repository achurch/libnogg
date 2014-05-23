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


int vorbis_seek(vorbis_t *handle, int64_t position)
{
    if (position < 0) {
        return 0;
    }
    if (handle->data_length < 0) {
        return 0;
    }

    // FIXME: seeking not supported by stb_vorbis in push mode
    return 0;
}
