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

// used in setup, and for huffman that doesn't go fast path
static unsigned int bit_reverse(unsigned int n)
{
  n = ((n & 0xAAAAAAAA) >>  1) | ((n & 0x55555555) << 1);
  n = ((n & 0xCCCCCCCC) >>  2) | ((n & 0x33333333) << 2);
  n = ((n & 0xF0F0F0F0) >>  4) | ((n & 0x0F0F0F0F) << 4);
  n = ((n & 0xFF00FF00) >>  8) | ((n & 0x00FF00FF) << 8);
  return (n >> 16) | (n << 16);
}

static float square(float x)
{
   return x*x;
}

// this is a weird definition of log2() for which log2(1) = 1, log2(2) = 2, log2(4) = 3
// as required by the specification. fast(?) implementation from stb.h
// @OPTIMIZE: called multiple times per-packet with "constants"; move to setup
static int ilog(int32_t n)
{
   static signed char log2_4[16] = { 0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4 };

   // 2 compares if n < 16, 3 compares otherwise (4 if signed or n > 1<<29)
   if (n < (1 << 14))
        if (n < (1 <<  4))        return     0 + log2_4[n      ];
        else if (n < (1 <<  9))      return  5 + log2_4[n >>  5];
             else                    return 10 + log2_4[n >> 10];
   else if (n < (1 << 24))
             if (n < (1 << 19))      return 15 + log2_4[n >> 15];
             else                    return 20 + log2_4[n >> 20];
        else if (n < (1 << 29))      return 25 + log2_4[n >> 25];
             else if (n < (1 << 31)) return 30 + log2_4[n >> 30];
                  else               return 0; // signed n returns 0
}

/*************************************************************************/
/*************************************************************************/

#endif  // NOGG_SRC_DECODE_INLINES_H
