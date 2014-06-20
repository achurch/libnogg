/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

/*
 * This header defines a few simple inline functions used for various
 * purposes that don't fit anywhere else.  These functions are all
 * declared UNUSED to prevent warnings from compilers which complain
 * about unused static inline functions.
 */

#ifndef NOGG_SRC_DECODE_INLINES_H
#define NOGG_SRC_DECODE_INLINES_H

/*************************************************************************/
/*************************************************************************/

/**
 * bit_reverse:  Reverse the order of the bits in a 32-bit integer.
 */
static inline UNUSED CONST_FUNCTION uint32_t bit_reverse(uint32_t n)
{
    n = ((n & 0xAAAAAAAA) >>  1) | ((n & 0x55555555) << 1);
    n = ((n & 0xCCCCCCCC) >>  2) | ((n & 0x33333333) << 2);
    n = ((n & 0xF0F0F0F0) >>  4) | ((n & 0x0F0F0F0F) << 4);
    n = ((n & 0xFF00FF00) >>  8) | ((n & 0x00FF00FF) << 8);
    return (n >> 16) | (n << 16);
}

/*************************************************************************/
/*************************************************************************/

#endif  // NOGG_SRC_DECODE_INLINES_H
