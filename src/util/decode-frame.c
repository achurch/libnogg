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


vorbis_error_t decode_frame(vorbis_t *handle)
{
    handle->decode_buf_pos = 0;
    handle->decode_buf_len = 0;

    (void) stb_vorbis_get_error(handle->decoder);  // Clear any pending error.
    float **outputs;
    int samples = 0;
    do {
        if (!stb_vorbis_get_frame_float(handle->decoder, &outputs, &samples)) {
            break;
        }
    } while (samples == 0);

    const int channels = handle->channels;
    for (int i = 0; i < samples; i++) {
        for (int c = 0; c < channels; c++) {
            handle->decode_buf[i*channels + c] = outputs[c][i];
        }
    }
    handle->decode_buf_len = samples;

    const STBVorbisError stb_error = stb_vorbis_get_error(handle->decoder);
    if (samples == 0 && stb_error == VORBIS__no_error) {
        return VORBIS_ERROR_STREAM_END;
    } else if (stb_error == VORBIS_invalid_packet
            || stb_error == VORBIS_continued_packet_flag_invalid) {
        return VORBIS_ERROR_DECODE_RECOVERED;
    } else if (samples == 0 || stb_error != VORBIS__no_error) {
        return VORBIS_ERROR_DECODE_FAILURE;
    } else {
        return VORBIS_NO_ERROR;
    }
}
