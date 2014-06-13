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
#include "src/decode/inlines.h"
#include "src/decode/io.h"
#include "src/decode/packet.h"
#include "src/util/memory.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

//FIXME: not fully reviewed

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
 * analyze_page:
 */
// ogg vorbis, in its insane infinite wisdom, only provides
// information about the sample at the END of the page.
// therefore we COULD have the data we need in the current
// page, and not know it. we could just use the end location
// as our only knowledge for bounds, seek back, and eventually
// the binary search finds it. or we can try to be smart and
// not waste time trying to locate more pages. we try to be
// smart, since this data is already in memory anyway, so
// doing needless I/O would be crazy!
static int analyze_page(stb_vorbis *f, ProbedPage *z)
{
   uint8_t header[27], lacing[255];
   uint8_t packet_type[255];
   int num_packet, packet_start;
   int len;
   uint32_t samples;

   // record where the page starts
   z->page_start = get_file_offset(f);

   // parse the header
   getn(f, header, 27);
   assert(header[0] == 'O' && header[1] == 'g' && header[2] == 'g' && header[3] == 'S');
   getn(f, lacing, header[26]);

   // determine the length of the payload
   len = 0;
   for (int i=0; i < header[26]; ++i)
      len += lacing[i];

   // this implies where the page ends
   z->page_end = z->page_start + 27 + header[26] + len;

   // read the last-decoded sample out of the data
   z->last_decoded_sample = header[6] + (header[7] << 8) + (header[8] << 16) + (header[9] << 24) + ((uint64_t)header[10] << 32) + ((uint64_t)header[11] << 40) + ((uint64_t)header[12] << 48) + ((uint64_t)header[13] << 56);

   if (header[5] & 4) {
      // if this is the last page, it's not possible to work
      // backwards to figure out the first sample! whoops! fuck.
      z->first_decoded_sample = -1;
      set_file_offset(f, z->page_start);
      return 1;
   }

   // scan through the frames to determine the sample-count of each one...
   // our goal is the sample # of the first fully-decoded sample on the
   // page, which is the first decoded sample of the 2nd page

   num_packet=0;

   packet_start = ((header[5] & 1) == 0);

   for (int i=0; i < header[26]; ++i) {
      if (packet_start) {
         uint8_t n,b;
         if (lacing[i] == 0) goto bail; // trying to read from zero-length packet
         n = get8(f);
         // if bottom bit is non-zero, we've got corruption
         if (n & 1) goto bail;
         n >>= 1;
         b = ilog(f->mode_count-1);
         n &= (1 << b)-1;
         if (n >= f->mode_count) goto bail;
         packet_type[num_packet++] = f->mode_config[n].blockflag;
         skip(f, lacing[i]-1);
      } else
         skip(f, lacing[i]);
      packet_start = (lacing[i] < 255);
   }

   // now that we know the sizes of all the pages, we can start determining
   // how much sample data there is.

   samples = 0;

   // for the last packet, we step by its whole length, because the definition
   // is that we encoded the end sample loc of the 'last packet completed',
   // where 'completed' refers to packets being split, and we are left to guess
   // what 'end sample loc' means. we assume it means ignoring the fact that
   // the last half of the data is useless without windowing against the next
   // packet... (so it's not REALLY complete in that sense)
   if (num_packet > 1)
      samples += f->blocksize[packet_type[num_packet-1]];

   for (int i=num_packet-2; i >= 1; --i) {
      // now, for this packet, how many samples do we have that
      // do not overlap the following packet?
      if (packet_type[i] == 1)
         if (packet_type[i+1] == 1)
            samples += f->blocksize[1] / 2;
         else
            samples += ((f->blocksize[1] - f->blocksize[0]) / 4) + (f->blocksize[0] / 2);
      else
         samples += f->blocksize[0] / 2;
   }
   // now, at this point, we've rewound to the very beginning of the
   // _second_ packet. if we entirely discard the first packet after
   // a seek, this will be exactly the right sample number. HOWEVER!
   // we can't as easily compute this number for the LAST page. The
   // only way to get the sample offset of the LAST page is to use
   // the end loc from the previous page. But what that returns us
   // is _exactly_ the place where we get our first non-overlapped
   // sample. (I think. Stupid spec for being ambiguous.) So for
   // consistency it's better to do that here, too. However, that
   // will then require us to NOT discard all of the first frame we
   // decode, in some cases, which means an even weirder frame size
   // and extra code. what a fucking pain.
   
   // we're going to discard the first packet if we
   // start the seek here, so we don't care about it. (we could actually
   // do better; if the first packet is long, and the previous packet
   // is short, there's actually data in the first half of the first
   // packet that doesn't need discarding... but not worth paying the
   // effort of tracking that of that here and in the seeking logic)
   // except crap, if we infer it from the _previous_ packet's end
   // location, we DO need to use that definition... and we HAVE to
   // infer the start loc of the LAST packet from the previous packet's
   // end location. fuck you, ogg vorbis.

   z->first_decoded_sample = z->last_decoded_sample - samples;

   // restore file state to where we were
   set_file_offset(f, z->page_start);
   return 1;

   // restore file state to where we were
  bail:
   set_file_offset(f, z->page_start);
   return 0;
}

/*-----------------------------------------------------------------------*/

/**
 * seek_frame_from_page:
 */
static int seek_frame_from_page(stb_vorbis *f, int64_t page_start, uint64_t first_sample, uint64_t target_sample)
{
   int left_start, left_end, right_start, right_end, mode;
   int frame=0;
   uint64_t frame_start;
   int frames_to_skip, data_to_skip;

   // first_sample is the sample # of the first sample that doesn't
   // overlap the previous page... note that this requires us to
   // _partially_ discard the first packet! bleh.
   set_file_offset(f, page_start);

   f->next_seg = -1;  // force page resync

   frame_start = first_sample;
   // frame start is where the previous packet's last decoded sample
   // was, which corresponds to left_end... EXCEPT if the previous
   // packet was long and this packet is short? Probably a bug here.


   // now, we can start decoding frames... we'll only FAKE decode them,
   // until we find the frame that contains our sample; then we'll rewind,
   // and try again
   for (;;) {
      int start;

      if (!vorbis_decode_initial(f, &left_start, &left_end, &right_start, &right_end, &mode))
         return error(f, VORBIS_seek_failed);

      if (frame == 0 && first_sample == 0)
         start = left_end;
      else
         start = left_start;

      // the window starts at left_start; the last valid sample we generate
      // before the next frame's window start is right_start-1
      if (target_sample < frame_start + right_start-start)
         break;

      flush_packet(f);
      if (f->eof)
         return error(f, VORBIS_seek_failed);

      frame_start += right_start - start;

      ++frame;
   }

   // ok, at this point, the sample we want is contained in frame #'frame'

   // to decode frame #'frame' normally, we have to decode the
   // previous frame first... but if it's the FIRST frame of the page
   // we can't. if it's the first frame, it means it falls in the part
   // of the first frame that doesn't overlap either of the other frames.
   // so, if we have to handle that case for the first frame, we might
   // as well handle it for all of them, so:
   if (target_sample > frame_start + (left_end - left_start)) {
      // so what we want to do is go ahead and just immediately decode
      // this frame, but then make it so the next get_frame_float() uses
      // this already-decoded data? or do we want to go ahead and rewind,
      // and leave a flag saying to skip the first N data? let's do that
      frames_to_skip = frame;  // if this is frame #1, skip 1 frame (#0)
      data_to_skip = left_end - left_start;
   } else {
      // otherwise, we want to skip frames 0, 1, 2, ... frame-2
      // (which means frame-2+1 total frames) then decode frame-1,
      // then leave frame pending
      frames_to_skip = frame - 1;
      // libnogg fix: if this is the first frame of the page, we need to
      // jump back a page to decode the previous frame; but we don't have
      // the location of the previous page, so we just do the seek all
      // over again for the _previous_ sample and then decode that frame
      // (UGLY HACK)
      if (frames_to_skip < 0) {
         (void) stb_vorbis_seek(f, target_sample - 1);
         frames_to_skip = 0;
         vorbis_pump_first_frame(f);
         f->current_loc = frame_start;
         return target_sample - frame_start;
      }
      data_to_skip = -1;      
   }

   set_file_offset(f, page_start);
   f->next_seg = - 1; // force page resync

   for (int i=0; i < frames_to_skip; ++i) {
      start_packet(f);
      flush_packet(f);
   }

   if (data_to_skip >= 0) {
      const int n = f->blocksize[0] / 2;
      f->discard_samples_deferred = data_to_skip;
      for (int i=0; i < f->channels; ++i)
         for (int j=0; j < n; ++j)
            f->previous_window[i][j] = 0;
      f->previous_length = n;
      frame_start += data_to_skip;
   } else {
      f->previous_length = 0;
      vorbis_pump_first_frame(f);
   }

   // at this point, the NEXT decoded frame will generate the desired sample
   f->current_loc = frame_start;
   return target_sample - frame_start;
}

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

// seek is implemented with 'interpolation search'--this is like
// binary search, but we use the data values to estimate the likely
// location of the data item (plus a bit of a bias so when the
// estimation is wrong we don't waste overly much time)
int stb_vorbis_seek(stb_vorbis *f, uint64_t sample_number)
{
    /* Fail early for unseekable streams. */
    if (f->stream_len < 0) {
        return error(f, VORBIS_cant_find_last_page);
    }

   ProbedPage p[2],q;

   // do we know the location of the last page?
   if (f->p_last.page_start == 0) {
      uint64_t z = stb_vorbis_stream_length_in_samples(f);
      if (z == 0) return error(f, VORBIS_cant_find_last_page);
   }

   p[0] = f->p_first;
   p[1] = f->p_last;

   const int orig_sample_number = sample_number;
   if (sample_number >= f->p_last.last_decoded_sample)
      sample_number = f->p_last.last_decoded_sample-1;

   if (sample_number < f->p_first.last_decoded_sample) {
      return seek_frame_from_page(f, p[0].page_start, 0, orig_sample_number);
   } else {
      int attempts=0;
      while (p[0].page_end < p[1].page_start) {
         int64_t probe;
         int64_t start_offset, end_offset;
         uint64_t start_sample, end_sample;

         // copy these into local variables so we can tweak them
         // if any are unknown
         start_offset = p[0].page_end;
         end_offset   = p[1].after_previous_page_start; // an address known to seek to page p[1]
         start_sample = p[0].last_decoded_sample;
         end_sample   = p[1].last_decoded_sample;

         // currently there is no such tweaking logic needed/possible?
         if (start_sample == (uint64_t)-1 || end_sample == (uint64_t)-1)
            return error(f, VORBIS_seek_failed);

         // now we want to lerp between these for the target samples...
      
         // step 1: we need to bias towards the page start...
         if (start_offset + 4000 < end_offset)
            end_offset -= 4000;

         // now compute an interpolated search loc
         probe = start_offset + (int) floorf((float) (end_offset - start_offset) / (end_sample - start_sample) * (sample_number - start_sample));

         // next we need to bias towards binary search...
         // code is a little wonky to allow for full 32-bit unsigned values
         if (attempts >= 4) {
            int64_t probe2 = start_offset + ((end_offset - start_offset) / 2);
            if (attempts >= 8)
               probe = probe2;
            else if (probe < probe2)
               probe = probe + ((probe2 - probe) / 2);
            else
               probe = probe2 + ((probe - probe2) / 2);
         }
         ++attempts;

         set_file_offset(f, probe);
         if (!find_page(f, NULL, NULL))   return error(f, VORBIS_seek_failed);
         if (!analyze_page(f, &q))        return error(f, VORBIS_seek_failed);
         q.after_previous_page_start = probe;

         // it's possible we've just found the last page again
         if (q.page_start == p[1].page_start) {
            p[1] = q;
            continue;
         }

         if (sample_number < q.last_decoded_sample)
            p[1] = q;
         else
            p[0] = q;
      }

      if (p[0].last_decoded_sample <= sample_number && sample_number < p[1].last_decoded_sample) {
         return seek_frame_from_page(f, p[1].page_start, p[0].last_decoded_sample, orig_sample_number);
      }
      return error(f, VORBIS_seek_failed);
   }
}

/*-----------------------------------------------------------------------*/

int64_t stb_vorbis_stream_length_in_samples(stb_vorbis *handle)
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
            handle->error = VORBIS_cant_find_last_page;
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
        // FIXME: stb comment -- what's a "file section"?
        // stop when the last_page flag is set, not when we reach eof;
        // this allows us to stop short of a 'file_section' end without
        // explicitly checking the length of the section
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
            handle->error = VORBIS_cant_find_last_page;
            goto done;
        }

        handle->p_last.page_start = last_page_loc;
        handle->p_last.page_end = page_end;
        handle->p_last.last_decoded_sample = handle->total_samples;
        handle->p_last.first_decoded_sample = -1;
        handle->p_last.after_previous_page_start = in_prev_page;

      done:
        set_file_offset(handle, restore_offset);
    }  // if (!handle->total_samples)

    return handle->total_samples == (uint64_t)-1 ? 0 : handle->total_samples;
}

/*************************************************************************/
/*************************************************************************/
