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


void *malloc_channel_array(int channels, int size)
{
    char **array = malloc(channels * (sizeof(*array) + size));
    if (array) {
        char * const subarray_base = (char *)&array[channels];
        for (int channel = 0; channel < channels; channel++) {
            array[channel] = subarray_base + channel*size;
        }
    }
    return array;
}
