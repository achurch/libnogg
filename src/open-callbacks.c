/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

/*
 * This file implements vorbis_open_with_callbacks(), which is treated as
 * the "base" open function for the library; all other open functions are
 * implemented by calling this function with an appropriate set of
 * callbacks.
 */

#include "include/nogg.h"
#include "include/internal.h"

#include <stdlib.h>
#include <string.h>


vorbis_t *vorbis_open_from_callbacks(
    vorbis_callbacks_t callbacks, void *opaque, vorbis_error_t *error_ret)
{
    vorbis_t *handle = NULL;
    vorbis_error_t error = VORBIS_NO_ERROR;

    /* Validate the function arguments. */
    if (!callbacks.read) {
        error = VORBIS_ERROR_INVALID_ARGUMENT;
        goto exit;
    }
    if (callbacks.length && (!callbacks.tell || !callbacks.read)) {
        error = VORBIS_ERROR_INVALID_ARGUMENT;
        goto exit;
    }

    /* Allocate and initialize a handle structure. */
    handle = malloc(sizeof(*handle));
    if (!handle) {
        error = VORBIS_ERROR_INSUFFICIENT_RESOURCES;
        goto exit;
    }
    handle->callbacks = callbacks;
    handle->callback_data = opaque;
    if (handle->callbacks.length) {
        handle->data_length =
            (*handle->callbacks.length)(handle->callback_data);
    } else {
        handle->data_length = -1;
    }
    handle->read_error_flag = 0;
    handle->eos_flag = 0;
    handle->decode_pos = 0;

    /* Create an stb_vorbis handle for the stream. */
    // FIXME: we currently assume read_buf will always be big enough
    fill_read_buf(handle);
    int used, stb_error;
    handle->decoder = stb_vorbis_open_pushdata(
        handle->read_buf, handle->read_buf_len, &used, &stb_error, NULL);
    if (!handle->decoder) {
        if (stb_error == VORBIS_outofmem) {
            error = VORBIS_ERROR_INSUFFICIENT_RESOURCES;
        } else if (stb_error == VORBIS_need_more_data
                || stb_error == VORBIS_unexpected_eof
                || stb_error == VORBIS_invalid_setup
                || stb_error == VORBIS_invalid_stream
                || stb_error == VORBIS_missing_capture_pattern
                || stb_error == VORBIS_invalid_stream_structure_version
                || stb_error == VORBIS_continued_packet_flag_invalid
                || stb_error == VORBIS_incorrect_stream_serial_number
                || stb_error == VORBIS_invalid_first_page
                || stb_error == VORBIS_bad_packet_type) {
            error = VORBIS_ERROR_STREAM_INVALID;
        } else {
            error = VORBIS_ERROR_DECODE_SETUP_FAILURE;
        }
        goto error_free_handle;
    }

    /* Save the audio parameters. */
    stb_vorbis_info info = stb_vorbis_get_info(handle->decoder);
    handle->channels = info.channels;
    handle->rate = info.sample_rate;
    handle->length = -1;  // FIXME: not supported by stb_vorbis in push mode

    /* Allocate a decoding buffer based on the maximum decoded frame size. */
    handle->decode_buf = malloc(
        sizeof(*handle->decode_buf) * handle->channels * info.max_frame_size);
    if (!handle) {
        error = VORBIS_ERROR_INSUFFICIENT_RESOURCES;
        goto error_close_decoder;
    }

  exit:
    if (error_ret) {
        *error_ret = error;
    }
    return handle;

  error_close_decoder:
    stb_vorbis_close(handle->decoder);
  error_free_handle:
    free(handle);
    handle = NULL;
    goto exit;
}
