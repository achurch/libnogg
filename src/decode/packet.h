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

#define EOP    (-1)
#define INVALID_BITS  (-1)

#define PAGEFLAG_continued_packet   1
#define PAGEFLAG_first_page         2
#define PAGEFLAG_last_page          4

extern int start_page(stb_vorbis *f);
extern int start_packet(stb_vorbis *f);
extern int maybe_start_packet(stb_vorbis *f);
extern int next_segment(stb_vorbis *f);
extern int get8_packet_raw(stb_vorbis *f);
extern int get8_packet(stb_vorbis *f);
extern void flush_packet(stb_vorbis *f);
extern uint32_t get_bits(stb_vorbis *f, int n);

/*************************************************************************/
/*************************************************************************/

#endif  // NOGG_SRC_DECODE_PACKET_H
