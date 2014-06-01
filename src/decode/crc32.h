/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#ifndef NOGG_SRC_DECODE_CRC32_H
#define NOGG_SRC_DECODE_CRC32_H

/*************************************************************************/
/*************************************************************************/

#define CRC32_POLY    0x04c11db7   // from spec

extern uint32_t crc_table[256];
extern void crc32_init(void);

static inline UNUSED CONST_FUNCTION uint32_t crc32_update(uint32_t crc, uint8_t byte)
{
   return (crc << 8) ^ crc_table[byte ^ (crc >> 24)];
}

/*************************************************************************/
/*************************************************************************/

#endif  // NOGG_SRC_DECODE_CRC32_H
