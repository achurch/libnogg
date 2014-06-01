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
#include "src/util/read.h"

#include <string.h>


int fill_read_buf(vorbis_t *handle)
{
    if (handle->read_buf_pos > 0) {
        memmove(handle->read_buf, handle->read_buf + handle->read_buf_pos,
                handle->read_buf_len - handle->read_buf_pos);
        handle->read_buf_len -= handle->read_buf_pos;
        handle->read_buf_pos = 0;
    }

    if (!handle->read_error_flag) {
        int length = sizeof(handle->read_buf) - handle->read_buf_len;
        if (handle->data_length >= 0) {
            const int64_t current_pos =
                (*handle->callbacks.tell)(handle->callback_data);
            if (length > handle->data_length - current_pos) {
                length = handle->data_length - current_pos;
            }
        }
        if (length > 0) {
            char *ptr = handle->read_buf + handle->read_buf_len;
            const int bytes_read =
                (*handle->callbacks.read)(handle->callback_data, ptr, length);
            if (bytes_read != length && handle->data_length >= 0) {
                /* This should never fail; treat failure as a fatal error. */
                handle->read_error_flag = 1;
            }
            handle->read_buf_len += length;
        }
    }

    return handle->read_buf_len > 0;
}
