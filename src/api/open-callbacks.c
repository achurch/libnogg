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
#include "src/util/memory.h"

#include <stdlib.h>
#include <string.h>


/* Callback wrappers for stb_vorbis. */
static int32_t stb_read(void *opaque, void *buf, int32_t len) {
    vorbis_t *handle = (vorbis_t *)opaque;
    return (*handle->callbacks.read)(handle->callback_data, buf, len);
}
static void stb_seek(void *opaque, int64_t offset) {
    vorbis_t *handle = (vorbis_t *)opaque;
    return (*handle->callbacks.seek)(handle->callback_data, offset);
}
static int64_t stb_tell(void *opaque) {
    vorbis_t *handle = (vorbis_t *)opaque;
    return (*handle->callbacks.tell)(handle->callback_data);
}


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
    if (callbacks.length && (!callbacks.tell || !callbacks.seek)) {
        error = VORBIS_ERROR_INVALID_ARGUMENT;
        goto exit;
    }
    if ((callbacks.malloc != NULL) != (callbacks.free != NULL)) {
        error = VORBIS_ERROR_INVALID_ARGUMENT;
        goto exit;
    }

    /* Allocate and initialize a handle structure. */
    if (callbacks.malloc) {
        handle = (*callbacks.malloc)(opaque, sizeof(*handle));
    } else {
        handle = malloc(sizeof(*handle));
    }
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
    handle->decode_buf_len = 0;
    handle->decode_buf_pos = 0;

    /* Create an stb_vorbis handle for the stream. */
    int stb_error;
    handle->decoder = stb_vorbis_open_callbacks(
        stb_read, stb_seek, stb_tell, handle, handle->data_length,
        &stb_error);
    if (!handle->decoder) {
        if (stb_error == VORBIS_outofmem) {
            error = VORBIS_ERROR_INSUFFICIENT_RESOURCES;
        } else if (stb_error == VORBIS_unexpected_eof
                || stb_error == VORBIS_missing_capture_pattern
                || stb_error == VORBIS_missing_capture_pattern_or_eof
                || stb_error == VORBIS_invalid_stream_structure_version
                || stb_error == VORBIS_invalid_first_page) {
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
    /* stb_vorbis doesn't differentiate between "empty file" and "error" in
     * the return value here, so use the error flag to tell the difference. */
    (void) stb_vorbis_get_error(handle->decoder);
    handle->length = stb_vorbis_stream_length_in_samples(handle->decoder);
    if (stb_vorbis_get_error(handle->decoder) != VORBIS__no_error) {
        handle->length = -1;
    }
    

    /* Allocate a decoding buffer based on the maximum decoded frame size. */
    handle->decode_buf = mem_alloc(
        handle,
        sizeof(*handle->decode_buf) * handle->channels * info.max_frame_size);
    if (!handle->decode_buf) {
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
    mem_free(handle, handle);
    handle = NULL;
    goto exit;
}
