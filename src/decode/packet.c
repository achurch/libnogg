/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

//FIXME: not reviewed

#include "include/nogg.h"
#include "include/internal.h"
#include "include/stb_vorbis.h"
#include "src/decode/common.h"
#include "src/decode/io.h"
#include "src/decode/packet.h"

#include <assert.h>

/*************************************************************************/
/*************************************************************************/

static int capture_pattern(stb_vorbis *f)
{
   if (0x4f != get8(f)) return FALSE;
   if (0x67 != get8(f)) return FALSE;
   if (0x67 != get8(f)) return FALSE;
   if (0x53 != get8(f)) return FALSE;
   return TRUE;
}

static int start_page_no_capturepattern(stb_vorbis *f)
{
   uint32_t loc0,loc1,n;
   // stream structure version
   if (0 != get8(f)) return error(f, VORBIS_invalid_stream_structure_version);
   // header flag
   f->page_flag = get8(f);
   // absolute granule position
   loc0 = get32(f); 
   loc1 = get32(f);
   // @TODO: validate loc0,loc1 as valid positions?
   // stream serial number -- vorbis doesn't interleave, so discard
   get32(f);
   //if (f->serial != get32(f)) return error(f, VORBIS_incorrect_stream_serial_number);
   // page sequence number
   n = get32(f);
   f->last_page = n;
   // CRC32
   get32(f);
   // page_segments
   f->segment_count = get8(f);
   if (!getn(f, f->segments, f->segment_count))
      return error(f, VORBIS_unexpected_eof);
   // assume we _don't_ know any the sample position of any segments
   f->end_seg_with_known_loc = -2;
   if (loc0 != ~0U || loc1 != ~0U) {
      // determine which packet is the last one that will complete
      int32_t i;
      for (i=f->segment_count-1; i >= 0; --i)
         if (f->segments[i] < 255)
            break;
      // 'i' is now the index of the _last_ segment of a packet that ends
      if (i >= 0) {
         f->end_seg_with_known_loc = i;
         f->known_loc_for_packet   = loc0;
      }
   }
   if (f->first_decode) {
      int i,len;
      ProbedPage p;
      len = 0;
      for (i=0; i < f->segment_count; ++i)
         len += f->segments[i];
      len += 27 + f->segment_count;
      p.page_start = f->first_audio_page_offset;
      p.page_end = p.page_start + len;
      p.after_previous_page_start = p.page_start;
      p.first_decoded_sample = 0;
      p.last_decoded_sample = loc0;
      f->p_first = p;
   }
   f->next_seg = 0;
   return TRUE;
}

int start_page(stb_vorbis *f)
{
   if (!capture_pattern(f)) return error(f, VORBIS_missing_capture_pattern);
   return start_page_no_capturepattern(f);
}

int start_packet(stb_vorbis *f)
{
   while (f->next_seg == -1) {
      if (!start_page(f)) return FALSE;
      if (f->page_flag & PAGEFLAG_continued_packet)
         return error(f, VORBIS_continued_packet_flag_invalid);
   }
   f->last_seg = FALSE;
   f->valid_bits = 0;
   f->packet_bytes = 0;
   f->bytes_in_seg = 0;
   // f->next_seg is now valid
   return TRUE;
}

int maybe_start_packet(stb_vorbis *f)
{
   if (f->next_seg == -1) {
      int x = get8(f);
      if (f->eof) return FALSE; // EOF at page boundary is not an error!
      if (0x4f != x      ) return error(f, VORBIS_missing_capture_pattern);
      if (0x67 != get8(f)) return error(f, VORBIS_missing_capture_pattern);
      if (0x67 != get8(f)) return error(f, VORBIS_missing_capture_pattern);
      if (0x53 != get8(f)) return error(f, VORBIS_missing_capture_pattern);
      if (!start_page_no_capturepattern(f)) return FALSE;
      if (f->page_flag & PAGEFLAG_continued_packet) {
         // set up enough state that we can read this packet if we want,
         // e.g. during recovery
         f->last_seg = FALSE;
         f->bytes_in_seg = 0;
         return error(f, VORBIS_continued_packet_flag_invalid);
      }
   }
   return start_packet(f);
}

int next_segment(stb_vorbis *f)
{
   int len;
   if (f->last_seg) return 0;
   if (f->next_seg == -1) {
      f->last_seg_which = f->segment_count-1; // in case start_page fails
      if (!start_page(f)) { f->last_seg = 1; return 0; }
      if (!(f->page_flag & PAGEFLAG_continued_packet)) return error(f, VORBIS_continued_packet_flag_invalid);
   }
   len = f->segments[f->next_seg++];
   if (len < 255) {
      f->last_seg = TRUE;
      f->last_seg_which = f->next_seg-1;
   }
   if (f->next_seg >= f->segment_count)
      f->next_seg = -1;
   assert(f->bytes_in_seg == 0);
   f->bytes_in_seg = len;
   return len;
}

int get8_packet_raw(stb_vorbis *f)
{
   if (!f->bytes_in_seg) {
      if (f->last_seg) return EOP;
      else if (!next_segment(f)) return EOP;
   }
   assert(f->bytes_in_seg > 0);
   --f->bytes_in_seg;
   ++f->packet_bytes;
   return get8(f);
}

int get8_packet(stb_vorbis *f)
{
   int x = get8_packet_raw(f);
   f->valid_bits = 0;
   return x;
}

void flush_packet(stb_vorbis *f)
{
   while (get8_packet_raw(f) != EOP);
}

// @OPTIMIZE: this is the secondary bit decoder, so it's probably not as important
// as the huffman decoder?
uint32_t get_bits(stb_vorbis *f, int n)
{
   if (f->valid_bits < 0) return 0;
   if (f->valid_bits < n) {
      if (n > 24) {
         // the accumulator technique below would not work correctly in this case
         uint32_t z = get_bits(f, 24);
         z += get_bits(f, n-24) << 24;
         return z;
      }
      if (f->valid_bits == 0) f->acc = 0;
      while (f->valid_bits < n) {
         int z = get8_packet_raw(f);
         if (z == EOP) {
            f->valid_bits = INVALID_BITS;
            return 0;
         }
         f->acc += z << f->valid_bits;
         f->valid_bits += 8;
      }
   }
   if (f->valid_bits < 0) return 0;
   uint32_t z = f->acc & ((1 << n)-1);
   f->acc >>= n;
   f->valid_bits -= n;
   return z;
}

/*************************************************************************/
/*************************************************************************/
