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

//FIXME not reviewed

//////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void vorbis_deinit(stb_vorbis *p)
{
   int i,j;
   for (i=0; i < p->residue_count; ++i) {
      Residue *r = p->residue_config+i;
      if (r->classdata) {
         for (j=0; j < p->codebooks[r->classbook].entries; ++j)
            free(r->classdata[j]);
         free(r->classdata);
      }
      free(r->residue_books);
   }

   if (p->codebooks) {
      for (i=0; i < p->codebook_count; ++i) {
         Codebook *c = p->codebooks + i;
         free(c->codeword_lengths);
         free(c->multiplicands);
         free(c->codewords);
         free(c->sorted_codewords);
         // c->sorted_values[-1] is the first entry in the array
         free(c->sorted_values ? c->sorted_values-1 : NULL);
      }
      free(p->codebooks);
   }
   free(p->floor_config);
   free(p->residue_config);
   for (i=0; i < p->mapping_count; ++i)
      free(p->mapping[i].chan);
   free(p->mapping);
   free(p->channel_buffers);
   free(p->outputs);
   free(p->previous_window);
   free(p->finalY);
   for (i=0; i < 2; ++i) {
      free(p->A[i]);
      free(p->B[i]);
      free(p->C[i]);
      free(p->window[i]);
   }
   free(p->imdct_temp_buf);
}

void stb_vorbis_close(stb_vorbis *p)
{
   if (p == NULL) return;
   vorbis_deinit(p);
   free(p);
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

   if (channels) *channels = f->channels;
   if (output)   *output = f->outputs;
   return len;
}

extern stb_vorbis * stb_vorbis_open_callbacks(
    long (*read_callback)(void *opaque, void *buf, long len),
    void (*seek_callback)(void *opaque, long offset),
    long (*tell_callback)(void *opaque),
    void *opaque, int64_t length, int *error_ret)
{
    stb_vorbis *handle = malloc(sizeof(*handle));
    if (!handle) {
        if (error_ret) {
            *error_ret = VORBIS_outofmem;
        }
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
        if (error_ret) {
            *error_ret = handle->error;
        }
        vorbis_deinit(handle);
        free(handle);
        return NULL;
    }
    vorbis_pump_first_frame(handle);
    return handle;
}
