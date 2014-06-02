/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#ifndef NOGG_SRC_DECODE_IO_H
#define NOGG_SRC_DECODE_IO_H

/*************************************************************************/
/*************************************************************************/

/**
 * get8:  Read a byte from the stream and return it as an 8-bit unsigned
 * integer.
 */
extern uint8_t get8(stb_vorbis *z);

/**
 * get32:  Read 4 bytes from the stream and return them as a 32-bit unsigned
 * integer.
 */
extern uint32_t get32(stb_vorbis *f);

/**
 * getn:  Read the given number of bytes from the stream and store them in
 * the given buffer.
 */
extern int getn(stb_vorbis *z, uint8_t *data, int n);

/**
 * skip:  Skip over the given number of bytes in the stream.
 */
extern void skip(stb_vorbis *z, int n);

/**
 * set_file_offset:  Set the stream read position to the given offset from
 * the beginning of the stream.
 */
extern int set_file_offset(stb_vorbis *f, unsigned int loc);

/**
 * get_file_offset:  Return the current stream read position.
 */
extern unsigned int get_file_offset(stb_vorbis *f);

/*************************************************************************/
/*************************************************************************/

#endif  // NOGG_SRC_DECODE_IO_H
