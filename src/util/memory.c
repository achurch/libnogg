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
#include "src/util/memory.h"

#include <stdlib.h>


void *mem_alloc(vorbis_t *handle, int32_t size)
{
    if (handle->callbacks.malloc) {
        return (*handle->callbacks.malloc)(handle->callback_data, size);
    } else {
        return malloc(size);
    }
}

void mem_free(vorbis_t *handle, void *ptr)
{
    if (handle->callbacks.free) {
        return (*handle->callbacks.free)(handle->callback_data, ptr);
    } else {
        return free(ptr);
    }
}

void *alloc_channel_array(vorbis_t *handle, int channels, int32_t size)
{
    char **array = mem_alloc(handle, channels * (sizeof(*array) + size));
    if (array) {
        char * const subarray_base = (char *)&array[channels];
        for (int channel = 0; channel < channels; channel++) {
            array[channel] = subarray_base + (channel * size);
        }
    }
    return array;
}
