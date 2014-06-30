/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#ifndef NOGG_SRC_DECODE_PACKET_H
#define NOGG_SRC_DECODE_PACKET_H

/*************************************************************************/
/*************************************************************************/

/* Constant returned from get8_packet_raw() when trying to read past the
 * end of a packet. */
#define EOP  (-1)

/* Flag bits set in the page_flag field of the decoder handle based on the
 * status of the current page. */
#define PAGEFLAG_continued_packet   1
#define PAGEFLAG_first_page         2
#define PAGEFLAG_last_page          4

/*-----------------------------------------------------------------------*/

/**
 * reset_page:  Reset the current stream state to start reading from a new
 * page.  A call to this function should be followed by a call to start_page()
 * or start_packet().
 *
 * [Parameters]
 *     handle: Stream handle.
 */
extern void reset_page(stb_vorbis *handle);

/**
 * start_page:  Verify that the stream read position is at the beginning
 * of a valid Ogg page, and start reading from the page.
 *
 * On error, some bytes may have been consumed from the stream.
 *
 * [Parameters]
 *     handle: Stream handle.
 * [Return value]
 *     True on success, false on error.
 */
extern bool start_page(stb_vorbis *handle);

/**
 * start_packet:  Start reading a new packet at the current stream read
 * position, advancing to a new page if necessary.
 *
 * [Parameters]
 *     handle: Stream handle.
 * [Return value]
 *     True on success, false on error.
 */
extern bool start_packet(stb_vorbis *handle);

/**
 * get8_packet:  Read one byte from the current packet.  The bit accumulator
 * for bitstream reads is cleared.
 *
 * [Parameters]
 *     handle: Stream handle.
 * [Return value]
 *     Byte read, or EOP on end of packet or error.
 */
extern int get8_packet(stb_vorbis *handle);

/**
 * get32_packet:  Read a byte-aligned 32-bit signed integer from the
 * current packet.  The bit accumulator for bitstream reads is cleared.
 *
 * [Parameters]
 *     handle: Stream handle.
 * [Return value]
 *     Value read, or EOP on end of packet or error.
 */
extern int32_t get32_packet(stb_vorbis *handle);

/**
 * getn_packet:  Read a sequence of bytes from the current packet.  The bit
 * accumulator for bitstream reads is cleared.
 *
 * [Parameters]
 *     handle: Stream handle.
 * [Return value]
 *     True on success, false on end of packet or error.
 */
extern bool getn_packet(stb_vorbis *handle, void *buf, int len);

/**
 * get_bits:  Read a value of arbitrary bit length from the stream.
 *
 * [Parameters]
 *     handle: Stream handle.
 * [Return value]
 *     Value read, or 0 on end of packet or error.
 */
extern uint32_t get_bits(stb_vorbis *handle, int count);

/**
 * flush_packet:  Advance the stream read position to the end of the
 * current packet.
 *
 * [Parameters]
 *     handle: Stream handle.
 */
extern void flush_packet(stb_vorbis *handle);

/**
 * fill_bits:  Fill the bit accumulator with as much data as possible.
 *
 * [Parameters]
 *     handle: Stream handle.
 */
extern void fill_bits(stb_vorbis *handle);

/*************************************************************************/
/*************************************************************************/

#endif  // NOGG_SRC_DECODE_PACKET_H
