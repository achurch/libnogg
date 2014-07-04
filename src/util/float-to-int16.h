/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#ifndef NOGG_SRC_UTIL_FLOAT_TO_INT16_H
#define NOGG_SRC_UTIL_FLOAT_TO_INT16_H

/*************************************************************************/
/*************************************************************************/

/**
 * float_to_int16:  Convert floating-point data in 'src' to 16-bit integer
 * data in 'dest'.
 *
 * [Parameters]
 *     dest: Destination (int16) buffer pointer.
 *     src: Source (float) buffer pointer.
 *     count: Number of values to convert.
 */
extern void float_to_int16(int16_t *dest, const float *src, int count);

/*************************************************************************/
/*************************************************************************/

#endif  // NOGG_SRC_UTIL_FLOAT_TO_INT16_H
