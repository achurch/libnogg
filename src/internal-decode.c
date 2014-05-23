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


int decode_frame(vorbis_t *handle)
{
    handle->decode_buf_pos = 0;
    handle->decode_buf_len = 0;

    int channels;
    float **outputs;
    int samples;
    int used = stb_vorbis_decode_frame_pushdata(
        handle->decoder, handle->read_buf + handle->read_buf_pos,
        handle->read_buf_len - handle->read_buf_pos,
        &channels, &outputs, &samples);
    if (!used) {
        fill_read_buf(handle);
        used = stb_vorbis_decode_frame_pushdata(
            handle->decoder, handle->read_buf, handle->read_buf_len,
            &channels, &outputs, &samples);
        if (!used) {
            return 0;
        }
    }
    if (!samples || channels != handle->channels) {
        return 0;
    }

    for (int i = 0; i < samples; i++) {
        for (int c = 0; c < channels; c++) {
            handle->decode_buf[i*channels + c] = outputs[c][i];
        }
    }

    handle->decode_buf_len = samples;
    return handle->decode_buf_len > 0;
}
