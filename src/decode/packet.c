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
#include "src/decode/common.h"
#include "src/decode/io.h"
#include "src/decode/packet.h"

#include <assert.h>

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

/**
 * next_segment:  Advance to the next segment in the current packet.
 *
 * [Parameters]
 *     handle: Stream handle.
 * [Return value]
 *     True on success, false on error or end of packet.
 */
static bool next_segment(stb_vorbis *handle)
{
    if (handle->last_seg) {
        return false;
    }
    if (handle->next_seg == -1) {
        if (!start_page(handle)) {
            handle->last_seg = true;
            handle->last_seg_index = handle->segment_count-1;
            return false;
        }
        if (!(handle->page_flag & PAGEFLAG_continued_packet)) {
            return error(handle, VORBIS_continued_packet_flag_invalid);
        }
    }
    int len = handle->segments[handle->next_seg++];
    if (len < 255) {
        handle->last_seg = true;
        handle->last_seg_index = handle->next_seg-1;
    }
    if (handle->next_seg >= handle->segment_count) {
        handle->next_seg = -1;
    }
    handle->bytes_in_seg = len;
    return len > 0;
}

/*-----------------------------------------------------------------------*/

/**
 * get8_packet_raw:  Read one byte from the current packet.
 *
 * [Parameters]
 *     handle: Stream handle.
 * [Return value]
 *     Byte read, or EOP on end of packet or error.
 */
static int get8_packet_raw(stb_vorbis *handle)
{
    if (!handle->bytes_in_seg) {
       if (handle->last_seg || !next_segment(handle)) {
           return EOP;
       }
    }
    assert(handle->bytes_in_seg > 0);
    handle->bytes_in_seg--;
    const int byte = get8(handle);
    if (handle->eof) {
        handle->error = VORBIS_unexpected_eof;
        handle->bytes_in_seg = 0;
        return EOP;
    }
    return byte;
}

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

bool start_page(stb_vorbis *handle)
{
    if (get8(handle) != 0x4F) {
        return error(handle, VORBIS_missing_capture_pattern_or_eof);
    }
    if (get8(handle) != 0x67
     || get8(handle) != 0x67
     || get8(handle) != 0x53) {
        return error(handle, VORBIS_missing_capture_pattern);
    }

//FIXME: not reviewed
   uint32_t loc0,loc1,n;
   // stream structure version
   if (0 != get8(handle)) return error(handle, VORBIS_invalid_stream_structure_version);
   // header flag
   handle->page_flag = get8(handle);
   // absolute granule position
   loc0 = get32(handle); 
   loc1 = get32(handle);
   // @TODO: validate loc0,loc1 as valid positions?
   // stream serial number -- vorbis doesn't interleave, so discard
   get32(handle);
   // page sequence number
   n = get32(handle);
   handle->last_page = n;
   // CRC32
   get32(handle);
   // page_segments
   handle->segment_count = get8(handle);
   if (!getn(handle, handle->segments, handle->segment_count))
      return error(handle, VORBIS_unexpected_eof);
   // assume we _don't_ know any the sample position of any segments
   handle->end_seg_with_known_loc = -2;
   if (loc0 != ~0U || loc1 != ~0U) {
      // determine which packet is the last one that will complete
      int32_t i;
      for (i=handle->segment_count-1; i >= 0; --i)
         if (handle->segments[i] < 255)
            break;
      // 'i' is now the index of the _last_ segment of a packet that ends
      if (i >= 0) {
         handle->end_seg_with_known_loc = i;
         handle->known_loc_for_packet   = loc0;
      }
   }
   if (handle->first_decode) {
      int i,len;
      ProbedPage p;
      len = 0;
      for (i=0; i < handle->segment_count; ++i)
         len += handle->segments[i];
      len += 27 + handle->segment_count;
      p.page_start = handle->first_audio_page_offset;
      p.page_end = p.page_start + len;
      p.after_previous_page_start = p.page_start;
      p.first_decoded_sample = 0;
      p.last_decoded_sample = loc0;
      handle->p_first = p;
   }
   handle->next_seg = 0;
   return true;
}

/*-----------------------------------------------------------------------*/

bool start_packet(stb_vorbis *handle)
{
    while (handle->next_seg == -1) {
        if (!start_page(handle)) {
            return false;
        }
        if (handle->page_flag & PAGEFLAG_continued_packet) {
            /* Set up enough state that we can read this packet if we want,
             * e.g. during recovery. */
            handle->last_seg = false;
            handle->bytes_in_seg = 0;
            return error(handle, VORBIS_continued_packet_flag_invalid);
        }
    }
    handle->last_seg = false;
    handle->valid_bits = 0;
    handle->bytes_in_seg = 0;
    return true;
}

/*-----------------------------------------------------------------------*/

int get8_packet(stb_vorbis *handle)
{
    int byte = get8_packet_raw(handle);
    handle->valid_bits = 0;
    return byte;
}

/*-----------------------------------------------------------------------*/

uint32_t get_bits(stb_vorbis *handle, int count)
{
    if (handle->valid_bits < 0) {
        return 0;
    }
    if (handle->valid_bits < count) {
        if (count > 24) {
            /* We have to handle this case specially to avoid overflow of
             * the bit accumulator. */
            uint32_t value = get_bits(handle, 24);
            value |= get_bits(handle, count-24) << 24;
            return value;
        }
        if (handle->valid_bits == 0) {
            handle->acc = 0;
        }
        while (handle->valid_bits < count) {
            int byte = get8_packet_raw(handle);
            if (byte == EOP) {
                handle->valid_bits = -1;
                return 0;
            }
            handle->acc |= byte << handle->valid_bits;
            handle->valid_bits += 8;
        }
    }
    const uint32_t value = handle->acc & ((1 << count) - 1);
    handle->acc >>= count;
    handle->valid_bits -= count;
    return value;
}

/*-----------------------------------------------------------------------*/

void flush_packet(stb_vorbis *handle)
{
    if (handle->bytes_in_seg > 0) {
        skip(handle, handle->bytes_in_seg);
    }
    while (next_segment(handle)) {
        skip(handle, handle->bytes_in_seg);
    }
    handle->bytes_in_seg = 0;
}

/*-----------------------------------------------------------------------*/

// @OPTIMIZE: primary accumulator for huffman
// expand the buffer to as many bits as possible without reading off end of packet
// it might be nice to allow f->valid_bits and f->acc to be stored in registers,
// e.g. cache them locally and decode locally
void fill_bits(stb_vorbis *handle)
{
    if (handle->valid_bits <= 24) {
        if (handle->valid_bits == 0) {
            handle->acc = 0;
        }
        do {
            if (handle->last_seg && !handle->bytes_in_seg) {
                break;
            }
            const int byte = get8_packet_raw(handle);
            if (byte == EOP) {
                break;
            }
            handle->acc |= byte << handle->valid_bits;
            handle->valid_bits += 8;
        } while (handle->valid_bits <= 24);
    }
}


/*************************************************************************/
/*************************************************************************/
