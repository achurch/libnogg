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

#include <math.h>


int64_t vorbis_read_int16(
    vorbis_t *handle, int16_t *buf, int64_t len, vorbis_error_t *error_ret)
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
            error = decode_frame(handle);
            if (error) {
                break;
            }
        }
        int64_t copy = len - count;
        if (copy > handle->decode_buf_len - handle->decode_buf_pos) {
            copy = handle->decode_buf_len - handle->decode_buf_pos;
        }
        const float *src =
            handle->decode_buf + handle->decode_buf_pos * channels;
        for (int i = 0; i < copy * channels; i++) {
            const int32_t sample = (int32_t)roundf(src[i] * 32767.0f);
            if (sample < -32767) {
                buf[i] = -32767;
            } else if (sample < 32767) {
                buf[i] = sample;
            } else {
                buf[i] = 32767;
            }
        }
        buf += copy * channels;
        count += copy;
        handle->decode_pos += copy;
        handle->decode_buf_pos += copy;
    }

  out:
    if (error_ret) {
        *error_ret = error;
    }
    return count;
}
