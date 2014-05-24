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

#include <string.h>


int64_t vorbis_read_float(
    vorbis_t *handle, float *buf, int64_t len, vorbis_error_t *error_ret)
{
    int64_t count = 0;
    int error = VORBIS_NO_ERROR;

    if (!buf || len < 0) {
        error = VORBIS_ERROR_INVALID_ARGUMENT;
        goto out;
    }

    const int channels = handle->channels;
    while (count < len) {
        if (handle->decode_buf_pos >= handle->decode_buf_len) {
            if (!decode_frame(handle)) {
                error = VORBIS_ERROR_DECODE_FAILURE;
                break;
            }
        }
        int64_t copy = len - count;
        if (copy > handle->decode_buf_len - handle->decode_buf_pos)
            copy = handle->decode_buf_len - handle->decode_buf_pos;
        memcpy(buf, handle->decode_buf + handle->decode_buf_pos * channels,
               copy * channels);
        buf += copy * channels;
        count += copy;
    }

  out:
    if (error_ret) {
        *error_ret = error;
    }
    return count;
}
