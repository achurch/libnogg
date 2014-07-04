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
#include "src/util/float-to-int16.h"

#include <math.h>


void float_to_int16(int16_t *dest, const float *src, int count)
{
    for (int i = 0; i < count; i++) {
        const float sample = src[i];
        if (sample <= -1.0f) {
            dest[i] = -32767;
        } else if (sample < 1.0f) {
            dest[i] = (int16_t)roundf(sample * 32767.0f);
        } else {
            dest[i] = 32767;
        }
    }
}
