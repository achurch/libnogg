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
#include "src/util/memory.h"


void vorbis_close(vorbis_t *handle)
{
    if (!handle) {
        return;
    }

    mem_free(handle, handle->decode_buf_base);
    stb_vorbis_close(handle->decoder);
    if (handle->callbacks.close) {
        (*handle->callbacks.close)(handle->callback_data);
    }
    mem_free(handle, handle);
}
