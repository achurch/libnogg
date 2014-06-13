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


int decode_frame(vorbis_t *handle)
{
    handle->decode_buf_pos = 0;
    handle->decode_buf_len = 0;

    (void) stb_vorbis_get_error(handle->decoder);  // Clear any pending error.
    int channels;
    float **outputs;
    const int samples = stb_vorbis_get_frame_float(
        handle->decoder, &channels, &outputs);

    for (int i = 0; i < samples; i++) {
        for (int c = 0; c < channels; c++) {
            handle->decode_buf[i*channels + c] = outputs[c][i];
        }
    }

    handle->decode_buf_len = samples;
    return handle->decode_buf_len > 0
        && stb_vorbis_peek_error(handle->decoder) == VORBIS__no_error;
}
