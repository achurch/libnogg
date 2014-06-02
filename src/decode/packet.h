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
extern int start_page(stb_vorbis *handle);

extern int start_packet(stb_vorbis *handle);
extern int next_segment(stb_vorbis *handle);
extern int get8_packet_raw(stb_vorbis *handle);
extern int get8_packet(stb_vorbis *handle);
extern void flush_packet(stb_vorbis *handle);
extern uint32_t get_bits(stb_vorbis *handle, int count);

/*************************************************************************/
/*************************************************************************/

#endif  // NOGG_SRC_DECODE_PACKET_H
