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

#include <string.h>

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

/**
 * interleave:  Interleave source channels into a destination buffer.
 *
 * [Parameters]
 *     dest: Destination buffer pointer.
 *     src: Source buffer pointer array.
 *     channels: Number of channels.
 *     samples: Number of samples per channel.
 */
static void interleave(float *dest, float **src, int channels, int samples)
{
    for (int i = 0; i < samples; i++) {
        for (int c = 0; c < channels; c++) {
            *dest++ = src[c][i];
        }
    }
}

/*-----------------------------------------------------------------------*/

/**
 * interleave_2:  Interleave two source channels into a destination buffer.
 * Specialization of interleave() for channels==2.
 *
 * [Parameters]
 *     dest: Destination buffer pointer.
 *     src: Source buffer pointer array.
 *     samples: Number of samples per channel.
 */
static void interleave_2(float *dest, float **src, int samples)
{
    const float *src0 = src[0];
    const float *src1 = src[1];

#if defined(ENABLE_ASM_X86_SSE2) && defined(__GNUC__)
    if (samples >= 4) {
        const int loops = samples / 4;
        samples %= 4;
        __asm__(
            "0:\n"
            "movdqu (%[src0]), %%xmm0\n"
            "movdqu (%[src1]), %%xmm1\n"
            "add $16, %[src0]\n"
            "add $16, %[src1]\n"
            "add $32, %[dest]\n"
            "movdqa %%xmm0, %%xmm2\n"
            "punpckldq %%xmm1, %%xmm0\n"
            "punpckhdq %%xmm1, %%xmm2\n"
            "movdqu %%xmm0, -32(%[dest])\n"
            "movdqu %%xmm2, -16(%[dest])\n"
            "cmp %[src0], %[src0_limit]\n"
            "ja 0b\n"
            : [dest] "=r" (dest), [src0] "=r" (src0), [src1] "=r" (src1)
            : "0" (dest), "1" (src0), "2" (src1),
              [src0_limit] "r" (src0 + loops*4)
            : "xmm0", "xmm1", "xmm2", "xmm3"
        );
    }
#endif

    for (int i = 0; i < samples; i++) {
        dest[i*2+0] = src0[i];
        dest[i*2+1] = src1[i];
    }
}

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

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
    float *decode_buf = handle->decode_buf;
    if (channels == 1) {
        memcpy(decode_buf, outputs[0], sizeof(*decode_buf) * samples);
    } else if (channels == 2) {
        interleave_2(decode_buf, outputs, samples);
    } else {
        interleave(decode_buf, outputs, channels, samples);
    }
    handle->decode_buf_len = samples;

    const STBVorbisError stb_error = stb_vorbis_get_error(handle->decoder);
    if (samples == 0 && stb_error == VORBIS__no_error) {
        return VORBIS_ERROR_STREAM_END;
    } else if (stb_error == VORBIS_invalid_packet
            || stb_error == VORBIS_continued_packet_flag_invalid) {
        return VORBIS_ERROR_DECODE_RECOVERED;
    } else if (samples == 0 || stb_error != VORBIS__no_error) {
        return VORBIS_ERROR_DECODE_FAILED;
    } else {
        return VORBIS_NO_ERROR;
    }
}

/*************************************************************************/
/*************************************************************************/
