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
#include "src/decode/crc32.h"
#include "src/decode/decode.h"
#include "src/decode/io.h"
#include "src/decode/packet.h"
#include "src/util/memory.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

// FIXME: reimp this with storing page data as we decode?
// at the least, we need to test with:
// - 1st packet on non-continued page, continued page
// - 2nd packet on non-continued page, continued page
// - 2nd-last packet on page
// - last packet on page
// - packet split between two pages (sample positions on each page)
// - 1st packet on last page
// - 2nd packet on last page
// - last packet on last page
// - long packet with preceding short packet (1st, 2nd, last packet on page)
// - short packet with preceding long packet (1st, 2nd, last packet on page)
// - sample at left (unlapped) edge of first frame
// - sample at right (unlapped) edge of last frame
// - sample in [left_start,left_end)
// - sample in [left_end,window_center) (only short->long frames)
// - sample in [window_center,right_start) (only long->short frames)
// - sample in [right_start,right_end)
// - sample in [left_end,right_start) in first frame (requires long->short)
// - something that causes the page search to hit the high page over and over
// - sample found in probed page vs. sample between pages
// also test next_long flag reading for streams with 6 and <6 mode bits

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

/**
 * find_page:  Locate the first page starting at or after the current read
 * position in the stream.
 *
 * [Parameters]
 *     handle: Stream handle.
 *     end_ret: Pointer to variable to receive the file offset of one byte
 *         past the end of the page.  May be NULL if not needed.
 *     last_ret: Pointer to variable to receive the "last page" flag of
 *         the page.  May be NULL if not needed.
 * [Return value]
 *     True if a page was found, false if not.
 */
static bool find_page(stb_vorbis *handle, int64_t *end_ret, bool *last_ret)
{
    while (!handle->eof) {

        /* See if we have the first byte of an Ogg page. */
        const uint8_t byte = get8(handle);
        if (byte != 0x4F) {
            continue;
        }

        /* Read in the rest of the (possible) page header. */
        const int64_t page_start = get_file_offset(handle) - 1;
        uint8_t header[27];
        header[0] = byte;
        if (!getn(handle, &header[1], sizeof(header)-1)) {
            break;
        }

        /* See if this really is an Ogg page (as opposed to a random 0x4F
         * byte in the middle of audio data). */
        if (header[1] == 0x67 && header[2] == 0x67 && header[3] == 0x53
         && header[4] == 0 /*Ogg version*/) {

            /* Check the page CRC to make the final determination of whether
             * this is a valid page. */
            const uint32_t expected_crc = header[22] <<  0
                                        | header[23] <<  8
                                        | header[24] << 16
                                        | header[25] << 24;
            for (int i = 22; i < 26; i++) {
                header[i] = 0;
            }
            uint32_t crc = 0;
            for (int i = 0; i < 27; i++) {
                crc = crc32_update(crc, header[i]);
            }
            unsigned int len = 0;
            for (int i = 0; i < header[26]; i++) {
                const int seglen = get8(handle);
                crc = crc32_update(crc, seglen);
                len += seglen;
            }
            for (unsigned int i = 0; i < len; i++) {
                crc = crc32_update(crc, get8(handle));
            }
            if (handle->eof) {
                break;
            }
            if (crc == expected_crc) {
                /* We found a valid page! */
                if (end_ret) {
                    *end_ret = get_file_offset(handle);
                }
                if (last_ret) {
                    *last_ret = ((header[5] & 0x04) != 0);
                }
                set_file_offset(handle, page_start);
                return true;
            }
        }

        /* It wasn't a valid page, so seek back to the byte after the
         * first one we tested and try again. */
        set_file_offset(handle, page_start + 1);

   }  // while (!handle->eof)

    return false;
}

/*-----------------------------------------------------------------------*/

/**
 * analyze_page:  Scan an Ogg page starting at the current read position
 * to determine the page's file and sample offset bounds.
 *
 * [Parameters]
 *     handle: Stream handle.
 *     page_ret: Pointer to variable to receive the page data.
 * [Return value]
 *     True on success, false on error.
 */
static int analyze_page(stb_vorbis *handle, ProbedPage *page_ret)
{
    /* The page start position is easy. */
    page_ret->page_start = get_file_offset(handle);

    /* Parse the header to determine the page length. */
    uint8_t header[27], lacing[255];
    if (!getn(handle, header, 27) || memcmp(header, "OggS"/*\0*/, 5) != 0) {
        goto bail;
    }
    const int num_segments = header[26];
    if (!getn(handle, lacing, num_segments)) {
        goto bail;
    }
    unsigned int payload_len = 0;
    for (int i = 0; i < num_segments; i++) {
        payload_len += lacing[i];
    }
    page_ret->page_end = page_ret->page_start + 27 + num_segments + payload_len;

    /* The header contains the sample offset of the end of the page.
     * If this value is -1, there are no complete packets on the page. */
    page_ret->last_decoded_sample = (uint32_t)header[ 6] <<  0
                                  | (uint32_t)header[ 7] <<  8
                                  | (uint32_t)header[ 8] << 16
                                  | (uint32_t)header[ 9] << 24
                                  | (uint64_t)header[10] << 32
                                  | (uint64_t)header[11] << 40
                                  | (uint64_t)header[12] << 48
                                  | (uint64_t)header[13] << 56;
    if (page_ret->last_decoded_sample == (uint64_t)-1) {
        page_ret->first_decoded_sample = -1;
        goto done;
    }

    /* The header does _not_ contain the sample offset of the beginning of
     * the page, so we have to go through ridiculous contortions to figure
     * it out ourselves, if it's even possible for the page in question.
     * Presumably the Ogg format developers thought it so important to save
     * an extra 64 bits per page that they didn't care about the impact on
     * decoder implementations. */

    /* On the last page of a stream, the end sample position is overloaded
     * to also declare the true length of the final packet (to allow for a
     * stream length of an arbitrary number of samples).  This means we
     * have no way to work out the first sample on the page.  Sigh deeply
     * and give up. */
    if (header[5] & 4) {
        page_ret->first_decoded_sample = page_ret->last_decoded_sample;
        goto done;
    }

    /* Scan the page to find the number of (complete) packets and the type
     * of each one. */
    bool packet_long[255], next_long[255];
    int num_packets = 0;
    int packet_start = ((header[5] & PAGEFLAG_continued_packet) == 0);
    const int mode_bits = handle->mode_bits;
    for (int i = 0; i < num_segments; i++) {
        if (packet_start) {
            if (lacing[i] == 0) {
                goto bail;  // Assume a 0-length packet indicates corruption.
            }
            const uint8_t packet_header = get8(handle);
            if (packet_header & 0x01) {
                goto bail;  // Audio packets must have the LSB clear.
            }
            const int mode = (packet_header >> 1) & ((1 << mode_bits) - 1);
            if (mode >= handle->mode_count) {
                goto bail;
            }
            packet_long[num_packets] = handle->mode_config[mode].blockflag;
            if (mode_bits == 6) {
                next_long[num_packets] = get8(handle) & 1;
                skip(handle, lacing[i] - 1);
            } else {
                next_long[num_packets] = (packet_header >> (mode_bits+2)) & 1;
                skip(handle, lacing[i] - 1);
            }
            num_packets++;
        } else {
            skip(handle, lacing[i]);
        }
        packet_start = (lacing[i] < 255);
    }
    if (!packet_start) {
        /* The last packet is incomplete, so ignore it. */
        num_packets--;
    }

    /* Count backwards from the end of the page to find the beginning
     * sample offset of the first fully-decoded sample on this page (this
     * is the beginning of the _second_ packet, since the first packet will
     * overlap with the last packet of the previous page). */
    uint64_t sample_pos = page_ret->last_decoded_sample;
    for (int i = num_packets-1; i >= 1; i--) {
        if (packet_long[i]) {
            if (next_long[i]) {
                sample_pos -= handle->blocksize[1] / 2;
            } else {
                sample_pos -=
                    (((handle->blocksize[1] - handle->blocksize[0]) / 4)
                     + (handle->blocksize[0] / 2));
            }
        } else {
            sample_pos -= handle->blocksize[0] / 2;
        }
    }
    page_ret->first_decoded_sample = sample_pos;

  done:
    set_file_offset(handle, page_ret->page_start);
    return true;

   /* Error conditions come down here. */
  bail:
    set_file_offset(handle, page_ret->page_start);
    return false;
}

/*-----------------------------------------------------------------------*/

/**
 * seek_frame_from_page:  Seek to the frame containing the given target
 * sample in the page beginning at the given offset.
 *
 * [Parameters]
 *     handle: Stream handle.
 *     page_start: File offset of the beginning of the page.
 *     first_sample: Sample offset of the first frame on the page.
 *     target_sample: Sample to seek to.
 * [Return value]
 *     Number of samples that must be discarded from the beginning of the
 *     frame to reach the target sample.
 */
static int seek_frame_from_page(stb_vorbis *handle, int64_t page_start,
                                uint64_t first_sample, uint64_t target_sample)
{
    /* Reset the read position to the beginning of the page. */
    set_file_offset(handle, page_start);
    reset_page(handle);

    /* Find the frame which, when decoded, will generate the target sample.
     * To save time, we only examine the packet headers rather than
     * actually decoding the frames. */
    int frame = -1;
    /* frame_start is the position of the first fully decoded sample in
     * frame number "frame". */
    uint64_t frame_start = first_sample;
    int left_start = 0, left_end = 0, right_start = 0, decoded_start = 0;
    do {
        frame++;
        /* Advance the frame start position based on the previous frame's
         * values.  (This line is why we initialize everything to 0 above.) */
        frame_start += right_start - decoded_start;
        int right_end, mode;
        if (!vorbis_decode_initial(handle, &left_start, &left_end,
                                   &right_start, &right_end, &mode)) {
            return error(handle, VORBIS_seek_failed);
        }
        flush_packet(handle);
        /* The frame we just scanned will give fully decoded samples in
         * the window range [left_start,right_start).  However, for the
         * very first frame in the file, we need to skip the left side of
         * the window which is normally overlapped with the previous frame. */
        if (frame == 0 && first_sample == 0) {
            decoded_start = left_end;
        } else {
            decoded_start = left_start;
        }
    } while (target_sample >= frame_start + (right_start - decoded_start));

    /* Determine where we need to start decoding in order to get the
     * desired sample. */
    int frames_to_skip;
    bool do_seek, decode_one_frame;
    if (target_sample >= frame_start + (left_end - left_start)) {
        /* In this case, the sample is outside the overlapped window
         * segments and doesn't require the previous frame to decode.
         * This can only happen in a long frame preceded by a short frame. */
        frames_to_skip = frame;
        do_seek = true;
        decode_one_frame = false;
    } else {
        /* The sample is in the overlapped portion of the window, so we'll
         * need to decode the previous frame first. */
        if (frame > 0) {
            frames_to_skip = frame - 1;
            do_seek = true;
            decode_one_frame = true;
        } else {
            /* If the target frame is the first frame in the page, we have
             * to rewind to the previous page to get to the previous frame. */
            // FIXME: we don't have the location of the previous page, so
            // we have to redo the seek
            handle->error = VORBIS__no_error;
            stb_vorbis_seek(handle, first_sample - 1);
            if (handle->error != VORBIS__no_error) {
                return 0;
            }
            frames_to_skip = 0;
            do_seek = false;
            decode_one_frame = 1;
        }
    }

    /* Set up the stream state so the next decode call returns the frame
     * containing the target sample, and return the offset of that sample
     * within the frame. */
    if (do_seek) {
        set_file_offset(handle, page_start);
        reset_page(handle);
    }
    for (int i = 0; i < frames_to_skip; i++) {
        start_packet(handle);
        flush_packet(handle);
    }
    handle->previous_length = 0;
    if (decode_one_frame) {
        vorbis_decode_packet(handle, NULL);
    }
    handle->current_loc = frame_start;
    return target_sample - frame_start;
}

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

// seek is implemented with 'interpolation search'--this is like
// binary search, but we use the data values to estimate the likely
// location of the data item (plus a bit of a bias so when the
// estimation is wrong we don't waste overly much time)
int stb_vorbis_seek(stb_vorbis *handle, uint64_t sample_number)
{
    /* Fail early for unseekable streams. */
    if (handle->stream_len < 0) {
        return error(handle, VORBIS_cant_find_last_page);
    }

    /* Find the first and last pages if they have not yet been looked up. */
    if (handle->p_last.page_start == 0) {
        (void) stb_vorbis_stream_length_in_samples(handle);
        if (handle->total_samples == (uint64_t)-1) {
            return 0;
        }
    }

    /* If we're seeking to/past the end of the stream, just do that. */
    if (sample_number >= handle->total_samples) {
        set_file_offset(handle, handle->stream_len);
        reset_page(handle);
        return 0;
    }

    /* If the sample is known to be on the first page, we don't need to
     * search for the correct page. */
    if (sample_number < handle->p_first.last_decoded_sample) {
        return seek_frame_from_page(handle, handle->p_first.page_start, 0,
                                    sample_number);
    }

    /* Otherwise, perform an interpolated binary search (biased toward
     * where we expect the page to be located) for the page containing
     * the target sample. */
    ProbedPage low = handle->p_first;
    ProbedPage high = handle->p_last;
    while (low.page_end < high.page_start) {
        /* Bounds for the search.  We use low.page_start+1 as the lower
         * bound instead of low.page_end to bias the search toward the
         * beginning of the stream, since find_page() scans forward. */
        const int64_t low_offset = low.page_start + 1;
        const int64_t high_offset = high.after_previous_page_start;
        const uint64_t low_sample = low.last_decoded_sample;
        const uint64_t high_sample = high.first_decoded_sample;
        if (low_sample == (uint64_t)-1 || high_sample == (uint64_t)-1) {
            return error(handle, VORBIS_seek_failed);
        }

        /* Pick a probe point for the next iteration.  As we get closer to
         * the target, we reduce the amount of bias, since there may be up
         * to a page's worth of variance in the relative positions of the
         * low and high bounds within their respective pages. */
        float lerp_frac =
            (float)(sample_number - low_sample) / (high_sample - low_sample);
        if (high_offset - low_offset < 8192) {
            lerp_frac = 0.5f;
        } else if (high_offset - low_offset < 65536) {
            lerp_frac = 0.25f + lerp_frac/2;
        }
        const int64_t probe = low_offset
            + (int64_t)floorf(lerp_frac * (high_offset - low_offset));

        /* Look for the next page starting after the probe point. */
        set_file_offset(handle, probe);
        if (!find_page(handle, NULL, NULL)) {
            return error(handle, VORBIS_seek_failed);
        }
        ProbedPage page;
        if (!analyze_page(handle, &page)) {
            return error(handle, VORBIS_seek_failed);
        }
        page.after_previous_page_start = probe;

        /* Choose one or the other side of the range and iterate, unless
         * we happened to find the right page (in which case we just stop). */
        if (sample_number < page.last_decoded_sample) {
            high = page;
            if (sample_number >= page.first_decoded_sample) {
                break;  // Found it!
            }
        } else {
            low = page;
        }
    }

    if (low.last_decoded_sample <= sample_number
     && sample_number < high.last_decoded_sample) {
        return seek_frame_from_page(handle, high.page_start,
                                    low.last_decoded_sample, sample_number);
    } else {
        return error(handle, VORBIS_seek_failed);
    }
}

/*-----------------------------------------------------------------------*/

uint64_t stb_vorbis_stream_length_in_samples(stb_vorbis *handle)
{
    if (!handle->total_samples) {
        /* Fail early for unseekable streams. */
        if (handle->stream_len < 0) {
            handle->total_samples = -1;
            return error(handle, VORBIS_cant_find_last_page);
        }

        /* Save the current file position so we can restore it when done. */
        const int64_t restore_offset = get_file_offset(handle);

        /* We need to find the last Ogg page in the file.  An Ogg page can
         * have up to 255*255 bytes of data; for simplicity, we just seek
         * back 64k. */
        int64_t in_prev_page;
        if (handle->stream_len - handle->p_first.page_start >= 65536) {
            in_prev_page = handle->stream_len - 65536;
        } else {
            in_prev_page = handle->p_first.page_start;
        }
        set_file_offset(handle, in_prev_page);

        /* Check that we can actually find a page there. */
        int64_t page_end;
        bool last;
        if (!find_page(handle, &page_end, &last)) {
            handle->total_samples = -1;
            goto done;
        }

        /* Look for subsequent pages. */
        int64_t last_page_loc = get_file_offset(handle);
        if (in_prev_page == last_page_loc
         && in_prev_page > handle->p_first.page_start) {
            /* We happened to start exactly at a page boundary, so back up
             * one byte for the "in previous page" location. */
            in_prev_page--;
        }
        while (!last) {
            set_file_offset(handle, page_end);
            if (!find_page(handle, &page_end, &last)) {
                /* The last page didn't have the "last page" flag set.
                 * This probably means the file is truncated, but we go
                 * on anyway. */
                break;
            }
            in_prev_page = last_page_loc+1;
            last_page_loc = get_file_offset(handle);
        }

        /* Get the sample offset at the end of the last page. */
        set_file_offset(handle, last_page_loc+6);
        handle->total_samples = get64(handle);
        if (handle->total_samples == (uint64_t)-1) {
            /* Oops, the last page didn't have a sample offset! */
            goto done;
        }

        handle->p_last.page_start = last_page_loc;
        handle->p_last.page_end = page_end;
        handle->p_last.first_decoded_sample = handle->total_samples;
        handle->p_last.last_decoded_sample = handle->total_samples;
        handle->p_last.after_previous_page_start = in_prev_page;

      done:
        set_file_offset(handle, restore_offset);
    }  // if (!handle->total_samples)

    if (handle->total_samples == (uint64_t)-1) {
        return error(handle, VORBIS_cant_find_last_page);
    } else {
        return handle->total_samples;
    }
}

/*************************************************************************/
/*************************************************************************/
