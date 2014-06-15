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
            handle->last_seg_index = handle->segment_count - 1;
            return false;
        }
        if (!(handle->page_flag & PAGEFLAG_continued_packet)) {
            return error(handle, VORBIS_continued_packet_flag_invalid);
        }
    }
    int len = handle->segments[handle->next_seg++];
    if (len < 255) {
        handle->last_seg = true;
        handle->last_seg_index = handle->next_seg - 1;
    }
    if (handle->next_seg >= handle->segment_count) {
        handle->next_seg = -1;
    }
    if (len > 0 && !getn(handle, handle->segment_data, len)) {
        return error(handle, VORBIS_unexpected_eof);
    }
    handle->segment_size = len;
    handle->segment_pos = 0;
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
    if (handle->segment_pos >= handle->segment_size) {
       if (handle->last_seg || !next_segment(handle)) {
           return EOP;
       }
    }
    return handle->segment_data[handle->segment_pos++];
}

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

void reset_page(stb_vorbis *handle)
{
    handle->next_seg = -1;
}

/*-----------------------------------------------------------------------*/

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

    /* Check the Ogg bitstream version. */
    if (get8(handle) != 0) {
        return error(handle, VORBIS_invalid_stream_structure_version);
    }
    /* Read the page header. */
    handle->page_flag = get8(handle);
    const uint64_t sample_pos = get64(handle);
    get32(handle);  // Bitstream ID (ignored).
    get32(handle);  // Page sequence number (ignored).
    get32(handle);  // CRC32 (ignored).
    handle->segment_count = get8(handle);  // FIXME: check for zero (invalid, I assume?)
    getn(handle, handle->segments, handle->segment_count);
    if (handle->eof) {
        return error(handle, VORBIS_unexpected_eof);
    }

    /* If this page has a sample position, find the packet it belongs to,
     * which will be the complete packet in this page. */
    handle->end_seg_with_known_loc = -2;  // Assume no packet with sample pos.
    if (sample_pos != ~UINT64_C(0)) {
        for (int i = (int)handle->segment_count - 1; i >= 0; i--) {
            /* An Ogg segment with size < 255 indicates the end of a packet. */
            if (handle->segments[i] < 255) {
                handle->end_seg_with_known_loc = i;
                handle->known_loc_for_packet = sample_pos;
                break;
            }
        }
    }

    /* If we haven't started decoding yet, record this page as the first
     * page containing audio.  We'll keep updating the data until we
     * actually start decoding, at which point first_decode will be cleared. */
    if (handle->first_decode) {
        int len = 27 + handle->segment_count;
        for (int i = 0; i < handle->segment_count; i++) {
            len += handle->segments[i];
        }
        /* p_first.page_start is set by start_decoder(). */
        handle->p_first.page_end = handle->p_first.page_start + len;
        handle->p_first.after_previous_page_start = handle->p_first.page_start;
        handle->p_first.first_decoded_sample = 0;
        handle->p_first.last_decoded_sample = sample_pos;
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
            handle->segment_size = 0;
            return error(handle, VORBIS_continued_packet_flag_invalid);
        }
    }
    handle->last_seg = false;
    handle->valid_bits = 0;
    handle->segment_size = 0;
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
            if (UNLIKELY(byte == EOP)) {
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
    while (next_segment(handle)) { /*loop*/ }
    handle->segment_size = 0;
}

/*-----------------------------------------------------------------------*/

void fill_bits(stb_vorbis *handle)
{
    if (handle->valid_bits <= 24) {
        if (handle->valid_bits == 0) {
            handle->acc = 0;
        }
        do {
            const int byte = get8_packet_raw(handle);
            if (UNLIKELY(byte == EOP)) {
                break;
            }
            handle->acc |= byte << handle->valid_bits;
            handle->valid_bits += 8;
        } while (handle->valid_bits <= 24);
    }
}


/*************************************************************************/
/*************************************************************************/
