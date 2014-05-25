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

#include <stdlib.h>
#include <string.h>

/*************************************************************************/
/******************** Buffer data type and callbacks *********************/
/*************************************************************************/

/* Data structure encapsulating the buffer, its length, and the current
 * read position, used as the opaque argument to callback functions. */
typedef struct buffer_data_t {
    const char *buffer;  /* char instead of void so we can index it. */
    long length;
    long position;
} buffer_data_t;

static int64_t buffer_length(void *opaque)
{
    buffer_data_t *buffer_data = (buffer_data_t *)opaque;
    return buffer_data->length;
}

static int64_t buffer_tell(void *opaque)
{
    buffer_data_t *buffer_data = (buffer_data_t *)opaque;
    return buffer_data->position;
}

static void buffer_seek(void *opaque, int64_t offset)
{
    buffer_data_t *buffer_data = (buffer_data_t *)opaque;
    buffer_data->position = offset;
}

static int64_t buffer_read(void *opaque, void *buffer, int64_t length)
{
    buffer_data_t *buffer_data = (buffer_data_t *)opaque;
    memcpy(buffer, buffer_data->buffer + buffer_data->position, length);
    buffer_data->position += length;
    return length;
}

static void buffer_close(void *opaque)
{
    buffer_data_t *buffer_data = (buffer_data_t *)opaque;
    free(buffer_data);
}

static const vorbis_callbacks_t buffer_callbacks = {
    .length = buffer_length,
    .tell   = buffer_tell,
    .seek   = buffer_seek,
    .read   = buffer_read,
    /* We could just use "free" instead of defining our own function, but
     * this makes the intent clearer. */
    .close  = buffer_close,
};

/*************************************************************************/
/*************************** Interface routine ***************************/
/*************************************************************************/

extern vorbis_t *vorbis_open_from_buffer(
    const void *buffer, int64_t length, vorbis_error_t *error_ret)
{
    if (!buffer || length < 0) {
        if (error_ret) {
            *error_ret = VORBIS_ERROR_INVALID_ARGUMENT;
        }
        return NULL;
    }

    buffer_data_t *buffer_data = malloc(sizeof(*buffer_data));
    if (!buffer_data) {
        if (error_ret) {
            *error_ret = VORBIS_ERROR_INSUFFICIENT_RESOURCES;
        }
        return NULL;
    }
    buffer_data->buffer = buffer;
    buffer_data->length = length;
    buffer_data->position = 0;

    vorbis_t *handle =
        vorbis_open_from_callbacks(buffer_callbacks, buffer_data, error_ret);
    if (!handle) {
        free(buffer_data);
    }
    return handle;
}

/*************************************************************************/
/*************************************************************************/
