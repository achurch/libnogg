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
#include "src/util/decode-frame.h"


int vorbis_seek(vorbis_t *handle, int64_t position)
{
    if (position < 0) {
        return 0;
    }

    (void) stb_vorbis_get_error(handle->decoder);
    const int offset = stb_vorbis_seek(handle->decoder, position);
    if (stb_vorbis_get_error(handle->decoder) != VORBIS__no_error) {
        return 0;
    }

    decode_frame(handle);
    handle->decode_buf_pos += offset;
    handle->decode_pos = position;
    return 1;
}


int64_t vorbis_tell(const vorbis_t *handle)
{
    return handle->decode_pos;
}
