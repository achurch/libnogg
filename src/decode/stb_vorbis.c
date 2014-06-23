/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

/*
 * This decoder implementation is based on the public-domain stb_vorbis.c
 * decoder (version 0.99996) from <http://nothings.org/stb_vorbis/>.
 * It shares the following limitations with that implementation:
 *    - Lossless sample truncation at the beginning of the stream
 *         (negative initial sample position) is ignored.
 *    - Only a single Ogg bitstream per stream is supported.
 */

#include "include/nogg.h"
#include "include/internal.h"
#include "src/decode/common.h"
#include "src/decode/crc32.h"
#include "src/decode/decode.h"
#include "src/decode/io.h"
#include "src/decode/packet.h"
#include "src/decode/setup.h"
#include "src/util/memory.h"

#include <string.h>

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

extern stb_vorbis * stb_vorbis_open_callbacks(
    int32_t (*read_callback)(void *opaque, void *buf, int32_t len),
    void (*seek_callback)(void *opaque, int64_t offset),
    int64_t (*tell_callback)(void *opaque),
    void *opaque, int64_t length, int *error_ret)
{
    stb_vorbis *handle = mem_alloc(opaque, sizeof(*handle));
    if (!handle) {
        *error_ret = VORBIS_outofmem;
        return NULL;
    }
    memset(handle, 0, sizeof(*handle));
    handle->read_callback = read_callback;
    handle->seek_callback = seek_callback;
    handle->tell_callback = tell_callback;
    handle->opaque = opaque;
    handle->stream_len = length;
    handle->error = VORBIS__no_error;

    if (!start_decoder(handle)) {
        *error_ret = handle->error;
        stb_vorbis_close(handle);
        return NULL;
    }

    vorbis_pump_frame(handle);
    return handle;
}

/*-----------------------------------------------------------------------*/

void stb_vorbis_close(stb_vorbis *handle)
{
    if (handle->codebooks) {
        for (int i = 0; i < handle->codebook_count; i++) {
            Codebook *book = &handle->codebooks[i];
            mem_free(handle->opaque, book->codeword_lengths);
            mem_free(handle->opaque, book->multiplicands);
            mem_free(handle->opaque, book->codewords);
            mem_free(handle->opaque, book->sorted_codewords);
            /* book->sorted_values points one entry past the allocated
             * address (see notes in setup.c). */
            if (book->sorted_values) {
                mem_free(handle->opaque, book->sorted_values-1);
            }
        }
        mem_free(handle->opaque, handle->codebooks);
    }

    if (handle->floor_config) {
        for (int i = 0; i < handle->floor_count; i++) {
            Floor *floor = &handle->floor_config[i];
            if (handle->floor_types[i] == 0) {
                mem_free(handle->opaque, floor->floor0.map[0]);
            }
        }
        mem_free(handle->opaque, handle->floor_config);
    }

    if (handle->residue_config) {
        for (int i = 0; i < handle->residue_count; i++) {
            Residue *res = &handle->residue_config[i];
            if (res->classdata) {
                mem_free(handle->opaque, res->classdata[0]);
                mem_free(handle->opaque, res->classdata);
            }
            mem_free(handle->opaque, res->residue_books);
        }
        mem_free(handle->opaque, handle->residue_config);
    }

    if (handle->mapping) {
        for (int i = 0; i < handle->mapping_count; i++) {
            mem_free(handle->opaque, handle->mapping[i].coupling);
        }
        mem_free(handle->opaque, handle->mapping[0].mux);
        mem_free(handle->opaque, handle->mapping);
    }

    for (int i = 0; i < 2; i++) {
        mem_free(handle->opaque, handle->A[i]);
        mem_free(handle->opaque, handle->B[i]);
        mem_free(handle->opaque, handle->C[i]);
        mem_free(handle->opaque, handle->bit_reverse[i]);
        mem_free(handle->opaque, handle->window_weights[i]);
    }

    mem_free(handle->opaque, handle->channel_buffers);
    mem_free(handle->opaque, handle->outputs);
    mem_free(handle->opaque, handle->previous_window);
    mem_free(handle->opaque, handle->coefficients);
    mem_free(handle->opaque, handle->final_Y);
#ifdef STB_VORBIS_DIVIDES_IN_RESIDUE
    mem_free(handle->opaque, handle->classifications);
#else
    mem_free(handle->opaque, handle->part_classdata);
#endif
    mem_free(handle->opaque, handle->imdct_temp_buf);

    mem_free(handle->opaque, handle);
}

/*-----------------------------------------------------------------------*/

STBVorbisError stb_vorbis_get_error(stb_vorbis *handle)
{
    const STBVorbisError last_error = handle->error;
    handle->error = VORBIS__no_error;
    return last_error;
}

/*-----------------------------------------------------------------------*/

stb_vorbis_info stb_vorbis_get_info(stb_vorbis *handle)
{
    return ((stb_vorbis_info){
        .channels = handle->channels,
        .sample_rate = handle->sample_rate,
        .max_frame_size = handle->blocksize[1],
    });
}

/*-----------------------------------------------------------------------*/

int stb_vorbis_get_frame_float(stb_vorbis *handle, float ***output_ret)
{
    int len, left, right;

    if (!vorbis_decode_packet(handle, &len, &left, &right)) {
        return 0;
    }
    len = vorbis_finish_frame(handle, len, left, right);

    for (int i = 0; i < handle->channels; i++)
        handle->outputs[i] = handle->channel_buffers[i] + left;

    *output_ret = handle->outputs;
    return len;
}

/*************************************************************************/
/*************************************************************************/
