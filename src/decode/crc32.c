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
#include "src/decode/crc32.h"


#define CRC32_POLY    0x04c11db7   // from spec

uint32_t crc_table[256];
void crc32_init(void)
{
   int i,j;
   uint32_t s;
   for(i=0; i < 256; i++) {
      for (s=i<<24, j=0; j < 8; ++j)
         s = (s << 1) ^ (s >= (1U<<31) ? CRC32_POLY : 0);
      crc_table[i] = s;
   }
}
