// Ogg Vorbis I audio decoder  -- version 0.99996
//
// Written in April 2007 by Sean Barrett, sponsored by RAD Game Tools.
//
// Placed in the public domain April 2007 by the author: no copyright is
// claimed, and you may use it for any purpose you like.
//
// No warranty for any purpose is expressed or implied by the author (nor
// by RAD Game Tools). Report bugs and send enhancements to the author.
//
// Get the latest version and other information at:
//     http://nothings.org/stb_vorbis/


// Todo:
//
//   - seeking (note you can seek yourself using the pushdata API)
//
// Limitations:
//
//   - floor 0 not supported (used in old ogg vorbis files)
//   - lossless sample-truncation at beginning ignored
//   - cannot concatenate multiple vorbis streams
//   - sample positions are 32-bit, limiting seekable 192Khz
//       files to around 6 hours (Ogg supports 64-bit)
// 
// All of these limitations may be removed in future versions.


#include "include/nogg.h"
#include "include/internal.h"
#include "include/stb_vorbis.h"
#include "src/decode/common.h"
#include "src/decode/crc32.h"
#include "src/decode/decode.h"
#include "src/decode/io.h"
#include "src/decode/packet.h"
#include "src/decode/setup.h"
#include "src/util/memory.h"

//FIXME not reviewed

//////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void vorbis_deinit(stb_vorbis *p)
{
   if (p->residue_config) {
      for (int i=0; i < p->residue_count; ++i) {
         Residue *r = p->residue_config+i;
         if (r->classdata) {
            mem_free(p->opaque, r->classdata[0]);
            mem_free(p->opaque, r->classdata);
         }
         mem_free(p->opaque, r->residue_books);
      }
   }

   if (p->codebooks) {
      for (int i=0; i < p->codebook_count; ++i) {
         Codebook *c = p->codebooks + i;
         mem_free(p->opaque, c->codeword_lengths);
         mem_free(p->opaque, c->multiplicands);
         mem_free(p->opaque, c->codewords);
         mem_free(p->opaque, c->sorted_codewords);
         // c->sorted_values[-1] is the first entry in the array
         mem_free(p->opaque, c->sorted_values ? c->sorted_values-1 : NULL);
      }
      mem_free(p->opaque, p->codebooks);
   }
   mem_free(p->opaque, p->floor_config);
   mem_free(p->opaque, p->residue_config);
   if (p->mapping) {
      mem_free(p->opaque, p->mapping[0].chan);
   }
   mem_free(p->opaque, p->mapping);
   mem_free(p->opaque, p->channel_buffers);
   mem_free(p->opaque, p->outputs);
   mem_free(p->opaque, p->previous_window);
   mem_free(p->opaque, p->finalY);
#ifdef STB_VORBIS_DIVIDES_IN_RESIDUE
   mem_free(p->opaque, p->classifications);
#else
   mem_free(p->opaque, p->part_classdata);
#endif
   for (int i=0; i < 2; ++i) {
      mem_free(p->opaque, p->A[i]);
      mem_free(p->opaque, p->B[i]);
      mem_free(p->opaque, p->C[i]);
      mem_free(p->opaque, p->window[i]);
      mem_free(p->opaque, p->bit_reverse[i]);
   }
   mem_free(p->opaque, p->imdct_temp_buf);
}

void stb_vorbis_close(stb_vorbis *p)
{
   vorbis_deinit(p);
   mem_free(p->opaque, p);
}

stb_vorbis_info stb_vorbis_get_info(stb_vorbis *f)
{
   stb_vorbis_info d;
   d.channels = f->channels;
   d.sample_rate = f->sample_rate;
   d.max_frame_size = f->blocksize_1;
   return d;
}

int stb_vorbis_get_error(stb_vorbis *f)
{
   int e = f->error;
   f->error = VORBIS__no_error;
   return e;
}

int stb_vorbis_peek_error(stb_vorbis *f)
{
   return f->error;
}

int stb_vorbis_get_frame_float(stb_vorbis *f, int *channels, float ***output)
{
   int len, right,left,i;

   if (!vorbis_decode_packet(f, &len, &left, &right)) {
      f->channel_buffer_start = f->channel_buffer_end = 0;
      return 0;
   }

   len = vorbis_finish_frame(f, len, left, right);
   for (i=0; i < f->channels; ++i)
      f->outputs[i] = f->channel_buffers[i] + left;

   f->channel_buffer_start = left;
   f->channel_buffer_end   = left+len;

   *channels = f->channels;
   *output = f->outputs;
   return len;
}

extern stb_vorbis * stb_vorbis_open_callbacks(
    long (*read_callback)(void *opaque, void *buf, long len),
    void (*seek_callback)(void *opaque, long offset),
    long (*tell_callback)(void *opaque),
    void *opaque, int64_t length, int *error_ret)
{
    stb_vorbis *handle = mem_alloc(opaque, sizeof(*handle));
    if (!handle) {
        *error_ret = VORBIS_outofmem;
        return NULL;
    }
    memset(handle, 0, sizeof(*handle));
    handle->read_callback = read_callback;
    handle->seek_callback = seek_callback;
    handle->tell_callback = tell_callback;
    handle->opaque = opaque;
    handle->stream_len = length;
    handle->error = VORBIS__no_error;
    if (!start_decoder(handle)) {
        *error_ret = handle->error;
        vorbis_deinit(handle);
        mem_free(opaque, handle);
        return NULL;
    }
    vorbis_pump_first_frame(handle);
    return handle;
}
