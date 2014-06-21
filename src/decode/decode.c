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
#include "src/decode/setup.h"
#include "src/util/memory.h"

#include <stdlib.h>
#include <string.h>

//FIXME: not fully reviewed

/*
 * Note on variable naming: Variables are generally named following usage
 * in the Vorbis spec, though some variables have been renamed for clarity.
 * In particular, the Vorbis spec consistently uses "n" for the window size
 * of a packet, and we follow that usage here.
 */

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Lookup table for type 1 floor curve generation, copied from the Vorbis
 * specification. */
static float floor1_inverse_db_table[256] =
{
    1.0649863e-07f, 1.1341951e-07f, 1.2079015e-07f, 1.2863978e-07f, 
    1.3699951e-07f, 1.4590251e-07f, 1.5538408e-07f, 1.6548181e-07f, 
    1.7623575e-07f, 1.8768855e-07f, 1.9988561e-07f, 2.1287530e-07f, 
    2.2670913e-07f, 2.4144197e-07f, 2.5713223e-07f, 2.7384213e-07f, 
    2.9163793e-07f, 3.1059021e-07f, 3.3077411e-07f, 3.5226968e-07f, 
    3.7516214e-07f, 3.9954229e-07f, 4.2550680e-07f, 4.5315863e-07f, 
    4.8260743e-07f, 5.1396998e-07f, 5.4737065e-07f, 5.8294187e-07f, 
    6.2082472e-07f, 6.6116941e-07f, 7.0413592e-07f, 7.4989464e-07f, 
    7.9862701e-07f, 8.5052630e-07f, 9.0579828e-07f, 9.6466216e-07f, 
    1.0273513e-06f, 1.0941144e-06f, 1.1652161e-06f, 1.2409384e-06f, 
    1.3215816e-06f, 1.4074654e-06f, 1.4989305e-06f, 1.5963394e-06f, 
    1.7000785e-06f, 1.8105592e-06f, 1.9282195e-06f, 2.0535261e-06f, 
    2.1869758e-06f, 2.3290978e-06f, 2.4804557e-06f, 2.6416497e-06f, 
    2.8133190e-06f, 2.9961443e-06f, 3.1908506e-06f, 3.3982101e-06f, 
    3.6190449e-06f, 3.8542308e-06f, 4.1047004e-06f, 4.3714470e-06f, 
    4.6555282e-06f, 4.9580707e-06f, 5.2802740e-06f, 5.6234160e-06f, 
    5.9888572e-06f, 6.3780469e-06f, 6.7925283e-06f, 7.2339451e-06f, 
    7.7040476e-06f, 8.2047000e-06f, 8.7378876e-06f, 9.3057248e-06f, 
    9.9104632e-06f, 1.0554501e-05f, 1.1240392e-05f, 1.1970856e-05f, 
    1.2748789e-05f, 1.3577278e-05f, 1.4459606e-05f, 1.5399272e-05f, 
    1.6400004e-05f, 1.7465768e-05f, 1.8600792e-05f, 1.9809576e-05f, 
    2.1096914e-05f, 2.2467911e-05f, 2.3928002e-05f, 2.5482978e-05f, 
    2.7139006e-05f, 2.8902651e-05f, 3.0780908e-05f, 3.2781225e-05f, 
    3.4911534e-05f, 3.7180282e-05f, 3.9596466e-05f, 4.2169667e-05f, 
    4.4910090e-05f, 4.7828601e-05f, 5.0936773e-05f, 5.4246931e-05f, 
    5.7772202e-05f, 6.1526565e-05f, 6.5524908e-05f, 6.9783085e-05f, 
    7.4317983e-05f, 7.9147585e-05f, 8.4291040e-05f, 8.9768747e-05f, 
    9.5602426e-05f, 0.00010181521f, 0.00010843174f, 0.00011547824f, 
    0.00012298267f, 0.00013097477f, 0.00013948625f, 0.00014855085f, 
    0.00015820453f, 0.00016848555f, 0.00017943469f, 0.00019109536f, 
    0.00020351382f, 0.00021673929f, 0.00023082423f, 0.00024582449f, 
    0.00026179955f, 0.00027881276f, 0.00029693158f, 0.00031622787f, 
    0.00033677814f, 0.00035866388f, 0.00038197188f, 0.00040679456f, 
    0.00043323036f, 0.00046138411f, 0.00049136745f, 0.00052329927f, 
    0.00055730621f, 0.00059352311f, 0.00063209358f, 0.00067317058f, 
    0.00071691700f, 0.00076350630f, 0.00081312324f, 0.00086596457f, 
    0.00092223983f, 0.00098217216f, 0.0010459992f,  0.0011139742f, 
    0.0011863665f,  0.0012634633f,  0.0013455702f,  0.0014330129f, 
    0.0015261382f,  0.0016253153f,  0.0017309374f,  0.0018434235f, 
    0.0019632195f,  0.0020908006f,  0.0022266726f,  0.0023713743f, 
    0.0025254795f,  0.0026895994f,  0.0028643847f,  0.0030505286f, 
    0.0032487691f,  0.0034598925f,  0.0036847358f,  0.0039241906f, 
    0.0041792066f,  0.0044507950f,  0.0047400328f,  0.0050480668f, 
    0.0053761186f,  0.0057254891f,  0.0060975636f,  0.0064938176f, 
    0.0069158225f,  0.0073652516f,  0.0078438871f,  0.0083536271f, 
    0.0088964928f,  0.009474637f,   0.010090352f,   0.010746080f, 
    0.011444421f,   0.012188144f,   0.012980198f,   0.013823725f, 
    0.014722068f,   0.015678791f,   0.016697687f,   0.017782797f, 
    0.018938423f,   0.020169149f,   0.021479854f,   0.022875735f, 
    0.024362330f,   0.025945531f,   0.027631618f,   0.029427276f, 
    0.031339626f,   0.033376252f,   0.035545228f,   0.037855157f, 
    0.040315199f,   0.042935108f,   0.045725273f,   0.048696758f, 
    0.051861348f,   0.055231591f,   0.058820850f,   0.062643361f, 
    0.066714279f,   0.071049749f,   0.075666962f,   0.080584227f, 
    0.085821044f,   0.091398179f,   0.097337747f,   0.10366330f, 
    0.11039993f,    0.11757434f,    0.12521498f,    0.13335215f, 
    0.14201813f,    0.15124727f,    0.16107617f,    0.17154380f, 
    0.18269168f,    0.19456402f,    0.20720788f,    0.22067342f, 
    0.23501402f,    0.25028656f,    0.26655159f,    0.28387361f, 
    0.30232132f,    0.32196786f,    0.34289114f,    0.36517414f, 
    0.38890521f,    0.41417847f,    0.44109412f,    0.46975890f, 
    0.50028648f,    0.53279791f,    0.56742212f,    0.60429640f, 
    0.64356699f,    0.68538959f,    0.72993007f,    0.77736504f, 
    0.82788260f,    0.88168307f,    0.9389798f,     1.0f
};

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

static int codebook_decode_scalar_raw(stb_vorbis *handle, const Codebook *c)
{
   fill_bits(handle);

   ASSERT(c->sorted_codewords || c->codewords);
   // cases to use binary search: sorted_codewords && !c->codewords
   //                             sorted_codewords && c->entries > 8
   if (c->sorted_codewords && (c->entries > 8 || !c->codewords)) {
      // binary search
      uint32_t code = bit_reverse(handle->acc);
      int x=0, n=c->sorted_entries, len;

      while (n > 1) {
         // invariant: sc[x] <= code < sc[x+n]
         int m = x + n/2;
         if (c->sorted_codewords[m] <= code) {
            x = m;
            n -= n/2;
         } else {
            n /= 2;
         }
      }
      // x is now the sorted index
      if (!c->sparse) x = c->sorted_values[x];
      // x is now sorted index if sparse, or symbol otherwise
      len = c->codeword_lengths[x];
      if (handle->valid_bits >= len) {
         handle->acc >>= len;
         handle->valid_bits -= len;
         return x;
      }

      handle->valid_bits = 0;
      return -1;
   }

   // if small, linear search
   ASSERT(!c->sparse);
   for (int i=0; i < c->entries; ++i) {
      if (c->codeword_lengths[i] == NO_CODE) continue;
      if (c->codewords[i] == (handle->acc & ((1 << c->codeword_lengths[i])-1))) {
         if (handle->valid_bits >= c->codeword_lengths[i]) {
            handle->acc >>= c->codeword_lengths[i];
            handle->valid_bits -= c->codeword_lengths[i];
            return i;
         }
         handle->valid_bits = 0;
         return -1;
      }
   }

   error(handle, VORBIS_invalid_stream);
   handle->valid_bits = 0;
   return -1;
}

static int codebook_decode_scalar(stb_vorbis *handle, const Codebook *c)
{
   int i;
   if (handle->valid_bits < STB_VORBIS_FAST_HUFFMAN_LENGTH)
      fill_bits(handle);
   // fast huffman table lookup
   i = handle->acc & FAST_HUFFMAN_TABLE_MASK;
   i = c->fast_huffman[i];
   if (i >= 0) {
      int n = c->codeword_lengths[i];
      handle->acc >>= n;
      handle->valid_bits -= n;
      if (handle->valid_bits < 0) { handle->valid_bits = 0; return -1; }
      return i;
   }
   return codebook_decode_scalar_raw(handle,c);
}

#define DECODE(var,f,c)   do {                                \
   var = codebook_decode_scalar(handle, c);                        \
   if (c->sparse) var = c->sorted_values[var];                \
} while (0)

#ifndef STB_VORBIS_DIVIDES_IN_CODEBOOK
  #define DECODE_VQ(var,f,c)   var = codebook_decode_scalar(handle, c);
#else
  #define DECODE_VQ(var,f,c)   DECODE(var,f,c)
#endif






// CODEBOOK_ELEMENT_FAST is an optimization for the CODEBOOK_FLOATS case
// where we avoid one addition
#ifndef STB_VORBIS_CODEBOOK_FLOATS
   #define CODEBOOK_ELEMENT(c,off)          (c->multiplicands[off] * c->delta_value + c->minimum_value)
   #define CODEBOOK_ELEMENT_FAST(c,off)     (c->multiplicands[off] * c->delta_value)
   #define CODEBOOK_ELEMENT_BASE(c)         (c->minimum_value)
#else
   #define CODEBOOK_ELEMENT(c,off)          (c->multiplicands[off])
   #define CODEBOOK_ELEMENT_FAST(c,off)     (c->multiplicands[off])
   #define CODEBOOK_ELEMENT_BASE(c)         (0)
#endif

static int codebook_decode_start(stb_vorbis *handle, Codebook *c, int len)
{
   int z = -1;

   // type 0 is only legal in a scalar context
   if (c->lookup_type == 0)
      error(handle, VORBIS_invalid_stream);
   else {
      DECODE_VQ(z,f,c);
      if (c->sparse) ASSERT(z < c->sorted_entries);
      if (z < 0) {  // check for EOP
         if (handle->segment_pos >= handle->segment_size)
            if (handle->last_seg)
               return z;
         error(handle, VORBIS_invalid_stream);
      }
   }
   return z;
}

static bool codebook_decode(stb_vorbis *handle, Codebook *c, float *output, int len)
{
   int z = codebook_decode_start(handle,c,len);
   if (z < 0) return false;
   if (len > c->dimensions) len = c->dimensions;

#ifdef STB_VORBIS_DIVIDES_IN_CODEBOOK
   if (c->lookup_type == 1) {
      float last = CODEBOOK_ELEMENT_BASE(c);
      int div = 1;
      for (int i=0; i < len; ++i) {
         int off = (z / div) % c->lookup_values;
         float val = CODEBOOK_ELEMENT_FAST(c,off) + last;
         output[i] += val;
         if (c->sequence_p) last = val + c->minimum_value;
         div *= c->lookup_values;
      }
      return true;
   }
#endif

   z *= c->dimensions;
   if (c->sequence_p) {
      float last = CODEBOOK_ELEMENT_BASE(c);
      for (int i=0; i < len; ++i) {
         float val = CODEBOOK_ELEMENT_FAST(c,z+i) + last;
         output[i] += val;
         last = val + c->minimum_value;
      }
   } else {
      float last = CODEBOOK_ELEMENT_BASE(c);
      for (int i=0; i < len; ++i) {
         output[i] += CODEBOOK_ELEMENT_FAST(c,z+i) + last;
      }
   }

   return true;
}

static bool codebook_decode_step(stb_vorbis *handle, Codebook *c, float *output, int len, int step)
{
   int z = codebook_decode_start(handle,c,len);
   float last = CODEBOOK_ELEMENT_BASE(c);
   if (z < 0) return false;
   if (len > c->dimensions) len = c->dimensions;

#ifdef STB_VORBIS_DIVIDES_IN_CODEBOOK
   if (c->lookup_type == 1) {
      int div = 1;
      for (int i=0; i < len; ++i) {
         int off = (z / div) % c->lookup_values;
         float val = CODEBOOK_ELEMENT_FAST(c,off) + last;
         output[i*step] += val;
         if (c->sequence_p) last = val;
         div *= c->lookup_values;
      }
      return true;
   }
#endif

   z *= c->dimensions;
   for (int i=0; i < len; ++i) {
      float val = CODEBOOK_ELEMENT_FAST(c,z+i) + last;
      output[i*step] += val;
      if (c->sequence_p) last = val;
   }

   return true;
}

static bool codebook_decode_deinterleave_repeat(stb_vorbis *handle, Codebook *c, float **outputs, int ch, int *c_inter_p, int *p_inter_p, int len, int total_decode)
{
   int c_inter = *c_inter_p;
   int p_inter = *p_inter_p;
   int z, effective = c->dimensions;

   // type 0 is only legal in a scalar context
   if (c->lookup_type == 0)   return error(handle, VORBIS_invalid_stream);

   while (total_decode > 0) {
      float last = CODEBOOK_ELEMENT_BASE(c);
      DECODE_VQ(z,f,c);
      #ifndef STB_VORBIS_DIVIDES_IN_CODEBOOK
      ASSERT(!c->sparse || z < c->sorted_entries);
      #endif
      if (z < 0) {
         if (handle->segment_pos >= handle->segment_size)
            if (handle->last_seg) return false;
         return error(handle, VORBIS_invalid_stream);
      }

      // if this will take us off the end of the buffers, stop short!
      // we check by computing the length of the virtual interleaved
      // buffer (len*ch), our current offset within it (p_inter*ch)+(c_inter),
      // and the length we'll be using (effective)
      if (c_inter + p_inter*ch + effective > len * ch) {
         effective = len*ch - (p_inter*ch - c_inter);
      }

   #ifdef STB_VORBIS_DIVIDES_IN_CODEBOOK
      if (c->lookup_type == 1) {
         int div = 1;
         for (int i=0; i < effective; ++i) {
            int off = (z / div) % c->lookup_values;
            float val = CODEBOOK_ELEMENT_FAST(c,off) + last;
            outputs[c_inter][p_inter] += val;
            if (++c_inter == ch) { c_inter = 0; ++p_inter; }
            if (c->sequence_p) last = val;
            div *= c->lookup_values;
         }
      } else
   #endif
      {
         z *= c->dimensions;
         if (c->sequence_p) {
            for (int i=0; i < effective; ++i) {
               float val = CODEBOOK_ELEMENT_FAST(c,z+i) + last;
               outputs[c_inter][p_inter] += val;
               if (++c_inter == ch) { c_inter = 0; ++p_inter; }
               last = val;
            }
         } else {
            for (int i=0; i < effective; ++i) {
               float val = CODEBOOK_ELEMENT_FAST(c,z+i) + last;
               outputs[c_inter][p_inter] += val;
               if (++c_inter == ch) { c_inter = 0; ++p_inter; }
            }
         }
      }

      total_decode -= effective;
   }
   *c_inter_p = c_inter;
   *p_inter_p = p_inter;
   return true;
}

#ifndef STB_VORBIS_DIVIDES_IN_CODEBOOK
static bool codebook_decode_deinterleave_repeat_2(stb_vorbis *handle, Codebook *c, float **outputs, int *c_inter_p, int *p_inter_p, int len, int total_decode)
{
   int c_inter = *c_inter_p;
   int p_inter = *p_inter_p;
   int z, effective = c->dimensions;

   // type 0 is only legal in a scalar context
   if (c->lookup_type == 0)   return error(handle, VORBIS_invalid_stream);

   while (total_decode > 0) {
      float last = CODEBOOK_ELEMENT_BASE(c);
      DECODE_VQ(z,f,c);

      if (z < 0) {
         if (handle->segment_pos >= handle->segment_size)
            if (handle->last_seg) return false;
         return error(handle, VORBIS_invalid_stream);
      }

      // if this will take us off the end of the buffers, stop short!
      // we check by computing the length of the virtual interleaved
      // buffer (len*ch), our current offset within it (p_inter*ch)+(c_inter),
      // and the length we'll be using (effective)
      if (c_inter + p_inter*2 + effective > len * 2) {
         effective = len*2 - (p_inter*2 - c_inter);
      }

      {
         z *= c->dimensions;
         if (c->sequence_p) {
            // haven't optimized this case because I don't have any examples
            for (int i=0; i < effective; ++i) {
               float val = CODEBOOK_ELEMENT_FAST(c,z+i) + last;
               outputs[c_inter][p_inter] += val;
               if (++c_inter == 2) { c_inter = 0; ++p_inter; }
               last = val;
            }
         } else {
            int i=0;
            if (c_inter == 1) {
               float val = CODEBOOK_ELEMENT_FAST(c,z+i) + last;
               outputs[c_inter][p_inter] += val;
               c_inter = 0; ++p_inter;
               ++i;
            }
            {
               float *z0 = outputs[0];
               float *z1 = outputs[1];
               for (; i+1 < effective;) {
                  z0[p_inter] += CODEBOOK_ELEMENT_FAST(c,z+i) + last;
                  z1[p_inter] += CODEBOOK_ELEMENT_FAST(c,z+i+1) + last;
                  ++p_inter;
                  i += 2;
               }
            }
            if (i < effective) {
               float val = CODEBOOK_ELEMENT_FAST(c,z+i) + last;
               outputs[c_inter][p_inter] += val;
               if (++c_inter == 2) { c_inter = 0; ++p_inter; }
            }
         }
      }

      total_decode -= effective;
   }
   *c_inter_p = c_inter;
   *p_inter_p = p_inter;
   return true;
}
#endif

/*************************************************************************/
/*************************** Floor processing ****************************/
/*************************************************************************/

/**
 * render_point:  Calculate the Y coordinate for point X on the line
 * (x0,y0)-(x1,y1).  Defined by section 9.2.6 in the Vorbis specification.
 *
 * [Parameters]
 *     x0, y0: First endpoint of the line.
 *     x1, y1: Second endpoint of the line.
 *     x: X coordinate of point to calculate.
 * [Return value]
 *     Y value for the given X coordinate.
 */
static CONST_FUNCTION int render_point(int x0, int y0, int x1, int y1, int x)
{
    /* The definition of this function in the spec has separate code
     * branches for positive and negative dy values, presumably to try and
     * avoid defining integer division as rounding negative values toward
     * zero (even though it ends up doing exactly that in the very next
     * section...), but C already defines integer division to round both
     * positive and negative values toward zero, so we just do things the
     * natural way. */
    return y0 + ((y1 - y0) * (x - x0) / (x1 - x0));
}

/*-----------------------------------------------------------------------*/

/**
 * render_line:  Render a line for a type 1 floor curve and perform the
 * dot-product operation with the residue vector.  Defined by section
 * 9.2.7 in the Vorbis specification.
 *
 * [Parameters]
 *     x0, y0: First endpoint of the line.
 *     x1, y1: Second endpoint of the line.
 *     output: Output buffer.  On entry, this buffer should contain the
 *         residue vector.
 *     n: Window length.
 */
static inline void render_line(int x0, int y0, int x1, int y1, float *output, int n)
{
    /* N.B.: The spec requires a very specific sequence of operations for
     * this function to ensure that both the encoder and the decoder
     * generate the same integer-quantized curve.  Take care that any
     * optimizations to this function do not change the output. */

    const int dy = y1 - y0;
    const int adx = x1 - x0;
    const int base = dy / adx;
    const int ady = abs(dy) - (abs(base) * adx);
    const int sy = (dy < 0 ? base-1 : base+1);
    int x = x0, y = y0;
    int err = 0;

    if (x1 > n) {
        if (x0 > n) {
            return;
        }
        x1 = n;
    }
    output[x] *= floor1_inverse_db_table[y];
    for (x++; x < x1; x++) {
        err += ady;
        if (err >= adx) {
            err -= adx;
            y += sy;
        } else {
            y += base;
        }
        output[x] *= floor1_inverse_db_table[y];
    }
}

/*-----------------------------------------------------------------------*/

/**
 * do_floor1_decode:  Perform type 1 floor decoding.
 *
 * [Parameters]
 *     handle: Stream handle.
 *     floor: Floor configuration.
 *     final_Y: final_Y buffer for the current channel.
 * [Return value]
 *     False to indicate "unused" status (4.2.3 step 6), true otherwise.
 */
static bool decode_floor1(stb_vorbis *handle, const Floor1 *floor,
                          int16_t *final_Y)
{
    static const int16_t range_list[4] = {256, 128, 86, 64};
    static const int8_t range_bits_list[4] = {8, 7, 7, 6};
    const int range = range_list[floor->floor1_multiplier - 1];
    const int range_bits = range_bits_list[floor->floor1_multiplier - 1];

    if (!get_bits(handle, 1)) {
        return false;  // Channel is zero.
    }

    /* Floor decode (7.2.3). */
    bool step2_flag[256];
    int offset = 2;
    final_Y[0] = get_bits(handle, range_bits);
    final_Y[1] = get_bits(handle, range_bits);
    for (int i = 0; i < floor->partitions; i++) {
        const int class = floor->partition_class_list[i];
        const int cdim = floor->class_dimensions[class];
        const int cbits = floor->class_subclasses[class];
        const int csub = (1 << cbits) - 1;
        int cval = 0;
        if (cbits) {
            const Codebook *book =
                &handle->codebooks[floor->class_masterbooks[class]];
            DECODE(cval, handle, book);
        }
        for (int j = 0; j < cdim; j++) {
            const int book_index = floor->subclass_books[class][cval & csub];
            cval = cval >> cbits;
            if (book_index >= 0) {
                const Codebook *book = &handle->codebooks[book_index];
                int temp;
                DECODE(temp, handle, book);
                final_Y[offset++] = temp;
            } else {
                final_Y[offset++] = 0;
            }
        }
    }
    if (handle->valid_bits < 0) {
        return false;  // The spec says we should treat EOP as a zero channel.
    }

    /* Amplitude value synthesis (7.2.4 step 1). */
    step2_flag[0] = true;
    step2_flag[1] = true;
    for (int i = 2; i < floor->values; i++) {
        const int low = floor->neighbors[i].low;
        const int high = floor->neighbors[i].high;
        const int predicted = render_point(floor->X_list[low], final_Y[low],
                                           floor->X_list[high], final_Y[high],
                                           floor->X_list[i]);
        const int val = final_Y[i];
        const int highroom = range - predicted;
        const int lowroom = predicted;
        int room;
        if (highroom < lowroom) {
            room = highroom * 2;
        } else {
            room = lowroom * 2;
        }
        if (val) {
            step2_flag[low] = true;
            step2_flag[high] = true;
            step2_flag[i] = true;
            if (val >= room) {
                if (highroom > lowroom) {
                    final_Y[i] = val - lowroom + predicted;
                } else {
                    final_Y[i] = predicted - val + highroom - 1;
                }
            } else {
                if (val % 2 != 0) {
                    final_Y[i] = predicted - (val+1)/2;
                } else {
                    final_Y[i] = predicted + val/2;
                }
            }
        } else {
            step2_flag[i] = false;
            final_Y[i] = predicted;
        }
    }
    // FIXME: spec suggests checking for out-of-range values; add
    // that as an option?

    /* Curve synthesis (7.2.4 step 2).  We defer final floor computation
     * until after synthesis; here, we just clear final_Y values which
     * need to be synthesized later. */
    for (int i = 0; i < floor->values; i++) {
        if (!step2_flag[i]) {
            final_Y[i] = -1;
        }
    }

    return true;
}

/*-----------------------------------------------------------------------*/

/**
 * do_floor1_final:  Perform the final floor computation and residue
 * dot-product for type 1 floors.
 *
 * [Parameters]
 *     handle: Stream handle.
 *     floor: Floor configuration.
 *     ch: Channel to operate on.
 *     n: Frame window size.
 */
static void do_floor1_final(stb_vorbis *handle, const Floor1 *floor,
                            const int ch, const int n)
{
    float *output = handle->channel_buffers[ch];
    const int16_t *final_Y = handle->final_Y[ch];
    int lx = 0;
    int ly = final_Y[0] * floor->floor1_multiplier;
    for (int i = 1; i < floor->values; i++) {
        int j = floor->sorted_order[i];
        if (final_Y[j] >= 0) {
            const int hy = final_Y[j] * floor->floor1_multiplier;
            const int hx = floor->X_list[j];
            render_line(lx, ly, hx, hy, output, n/2);
            lx = hx;
            ly = hy;
        }
    }
    if (lx < n/2) {
        /* Optimization of: render_line(lx, ly, hx, hy, output, n/2); */
        for (int i = lx; i < n/2; i++) {
            output[i] *= floor1_inverse_db_table[ly];
        }
    }
}

/*************************************************************************/
/*************************** Residue handling ****************************/
/*************************************************************************/

static bool residue_decode(stb_vorbis *handle, Codebook *book, float *target, int offset, int n, int rtype)
{
   if (rtype == 0) {
      int step = n / book->dimensions;
      for (int k=0; k < step; ++k)
         if (!codebook_decode_step(handle, book, target+offset+k, n-offset-k, step))
            return false;
   } else {
      for (int k=0; k < n; ) {
         if (!codebook_decode(handle, book, target+offset, n-k))
            return false;
         k += book->dimensions;
         offset += book->dimensions;
      }
   }
   return true;
}

static void decode_residue(stb_vorbis *handle, float *residue_buffers[], int ch, int n, int rn, const bool *do_not_decode)
{
   Residue *r = handle->residue_config + rn;
   int rtype = handle->residue_types[rn];
   int classbook = r->classbook;
   int classwords = handle->codebooks[classbook].dimensions;
   int n_read = r->end - r->begin;
   int part_read = n_read / r->part_size;
   #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
   uint8_t ***part_classdata = handle->part_classdata;
   #else
   int **classifications = handle->classifications;
   #endif

   for (int i=0; i < ch; ++i)
      if (!do_not_decode[i])
         memset(residue_buffers[i], 0, sizeof(float) * n);

   if (rtype == 2 && ch != 1) {
      bool found_ch = false;
      for (int j=0; j < ch; ++j) {
         if (!do_not_decode[j]) {
            found_ch = true;
            break;
         }
      }
      if (!found_ch)
         goto done;

      for (int pass=0; pass < 8; ++pass) {
         int pcount = 0;
         #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
         int class_set = 0;
         #endif
         if (ch == 2) {
            while (pcount < part_read) {
               int z = r->begin + pcount*r->part_size;
               int c_inter = (z & 1), p_inter = z>>1;
               if (pass == 0) {
                  Codebook *codebook = handle->codebooks+r->classbook;
                  int q;
                  DECODE(q,f,codebook);
                  if (q == EOP) goto done;
                  #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
                  part_classdata[0][class_set] = r->classdata[q];
                  #else
                  for (int i=classwords-1; i >= 0; --i) {
                     classifications[0][i+pcount] = q % r->classifications;
                     q /= r->classifications;
                  }
                  #endif
               }
               for (int i=0; i < classwords && pcount < part_read; ++i, ++pcount) {
                  int z2 = r->begin + pcount*r->part_size;
                  #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
                  int c = part_classdata[0][class_set][i];
                  #else
                  int c = classifications[0][pcount];
                  #endif
                  int b = r->residue_books[c][pass];
                  if (b >= 0) {
                     Codebook *book = handle->codebooks + b;
                     #ifdef STB_VORBIS_DIVIDES_IN_CODEBOOK
                     if (!codebook_decode_deinterleave_repeat(handle, book, residue_buffers, ch, &c_inter, &p_inter, n, r->part_size))
                        goto done;
                     #else
                     // saves 1%
                     if (!codebook_decode_deinterleave_repeat_2(handle, book, residue_buffers, &c_inter, &p_inter, n, r->part_size))
                        goto done;
                     #endif
                  } else {
                     z2 += r->part_size;
                     c_inter = z2 & 1;
                     p_inter = z2 >> 1;
                  }
               }
               #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
               ++class_set;
               #endif
            }
         } else if (ch == 1) {
            while (pcount < part_read) {
               int z = r->begin + pcount*r->part_size;
               int c_inter = 0, p_inter = z;
               if (pass == 0) {
                  Codebook *c = handle->codebooks+r->classbook;
                  int q;
                  DECODE(q,f,c);
                  if (q == EOP) goto done;
                  #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
                  part_classdata[0][class_set] = r->classdata[q];
                  #else
                  for (int i=classwords-1; i >= 0; --i) {
                     classifications[0][i+pcount] = q % r->classifications;
                     q /= r->classifications;
                  }
                  #endif
               }
               for (int i=0; i < classwords && pcount < part_read; ++i, ++pcount) {
                  int z2 = r->begin + pcount*r->part_size;
                  #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
                  int c = part_classdata[0][class_set][i];
                  #else
                  int c = classifications[0][pcount];
                  #endif
                  int b = r->residue_books[c][pass];
                  if (b >= 0) {
                     Codebook *book = handle->codebooks + b;
                     if (!codebook_decode_deinterleave_repeat(handle, book, residue_buffers, ch, &c_inter, &p_inter, n, r->part_size))
                        goto done;
                  } else {
                     z2 += r->part_size;
                     c_inter = 0;
                     p_inter = z2;
                  }
               }
               #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
               ++class_set;
               #endif
            }
         } else {
            while (pcount < part_read) {
               int z = r->begin + pcount*r->part_size;
               int c_inter = z % ch, p_inter = z/ch;
               if (pass == 0) {
                  Codebook *c = handle->codebooks+r->classbook;
                  int q;
                  DECODE(q,f,c);
                  if (q == EOP) goto done;
                  #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
                  part_classdata[0][class_set] = r->classdata[q];
                  #else
                  for (int i=classwords-1; i >= 0; --i) {
                     classifications[0][i+pcount] = q % r->classifications;
                     q /= r->classifications;
                  }
                  #endif
               }
               for (int i=0; i < classwords && pcount < part_read; ++i, ++pcount) {
                  int z2 = r->begin + pcount*r->part_size;
                  #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
                  int c = part_classdata[0][class_set][i];
                  #else
                  int c = classifications[0][pcount];
                  #endif
                  int b = r->residue_books[c][pass];
                  if (b >= 0) {
                     Codebook *book = handle->codebooks + b;
                     if (!codebook_decode_deinterleave_repeat(handle, book, residue_buffers, ch, &c_inter, &p_inter, n, r->part_size))
                        goto done;
                  } else {
                     z2 += r->part_size;
                     c_inter = z2 % ch;
                     p_inter = z2 / ch;
                  }
               }
               #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
               ++class_set;
               #endif
            }
         }
      }
      goto done;
   }

   for (int pass=0; pass < 8; ++pass) {
      int pcount = 0;
      #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
      int class_set = 0;
      #endif
      while (pcount < part_read) {
         if (pass == 0) {
            for (int j=0; j < ch; ++j) {
               if (!do_not_decode[j]) {
                  Codebook *c = handle->codebooks+r->classbook;
                  int temp;
                  DECODE(temp,f,c);
                  if (temp == EOP) goto done;
                  #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
                  part_classdata[j][class_set] = r->classdata[temp];
                  #else
                  for (int i=classwords-1; i >= 0; --i) {
                     classifications[j][i+pcount] = temp % r->classifications;
                     temp /= r->classifications;
                  }
                  #endif
               }
            }
         }
         for (int i=0; i < classwords && pcount < part_read; ++i, ++pcount) {
            for (int j=0; j < ch; ++j) {
               if (!do_not_decode[j]) {
                  #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
                  int c = part_classdata[j][class_set][i];
                  #else
                  int c = classifications[j][pcount];
                  #endif
                  int b = r->residue_books[c][pass];
                  if (b >= 0) {
                     float *target = residue_buffers[j];
                     int offset = r->begin + pcount * r->part_size;
                     int size = r->part_size;
                     Codebook *book = handle->codebooks + b;
                     if (!residue_decode(handle, book, target, offset, size, rtype))
                        goto done;
                  }
               }
            }
         }
         #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
         ++class_set;
         #endif
      }
   }
  done:;
}

/*************************************************************************/
/************************ Inverse MDCT processing ************************/
/*************************************************************************/

// the following were split out into separate functions while optimizing;
// they could be pushed back up but eh. inline showed no change;
// they're probably already being inlined.
static void imdct_step3_iter0_loop(const int n, float *e, int i_off, int k_off, float *A)
{
   float *ee0 = e + i_off;
   float *ee2 = ee0 + k_off;

   ASSERT((n & 3) == 0);
   for (int i=n/4; i > 0; --i) {
      float k00_20, k01_21;
      k00_20  = ee0[ 0] - ee2[ 0];
      k01_21  = ee0[-1] - ee2[-1];
      ee0[ 0] += ee2[ 0];//ee0[ 0] = ee0[ 0] + ee2[ 0];
      ee0[-1] += ee2[-1];//ee0[-1] = ee0[-1] + ee2[-1];
      ee2[ 0] = k00_20 * A[0] - k01_21 * A[1];
      ee2[-1] = k01_21 * A[0] + k00_20 * A[1];
      A += 8;

      k00_20  = ee0[-2] - ee2[-2];
      k01_21  = ee0[-3] - ee2[-3];
      ee0[-2] += ee2[-2];//ee0[-2] = ee0[-2] + ee2[-2];
      ee0[-3] += ee2[-3];//ee0[-3] = ee0[-3] + ee2[-3];
      ee2[-2] = k00_20 * A[0] - k01_21 * A[1];
      ee2[-3] = k01_21 * A[0] + k00_20 * A[1];
      A += 8;

      k00_20  = ee0[-4] - ee2[-4];
      k01_21  = ee0[-5] - ee2[-5];
      ee0[-4] += ee2[-4];//ee0[-4] = ee0[-4] + ee2[-4];
      ee0[-5] += ee2[-5];//ee0[-5] = ee0[-5] + ee2[-5];
      ee2[-4] = k00_20 * A[0] - k01_21 * A[1];
      ee2[-5] = k01_21 * A[0] + k00_20 * A[1];
      A += 8;

      k00_20  = ee0[-6] - ee2[-6];
      k01_21  = ee0[-7] - ee2[-7];
      ee0[-6] += ee2[-6];//ee0[-6] = ee0[-6] + ee2[-6];
      ee0[-7] += ee2[-7];//ee0[-7] = ee0[-7] + ee2[-7];
      ee2[-6] = k00_20 * A[0] - k01_21 * A[1];
      ee2[-7] = k01_21 * A[0] + k00_20 * A[1];
      A += 8;
      ee0 -= 8;
      ee2 -= 8;
   }
}

static void imdct_step3_inner_r_loop(const int lim, float *e, int d0, int k_off, float *A, int k1)
{
   float k00_20, k01_21;

   float *e0 = e + d0;
   float *e2 = e0 + k_off;

   for (int i=lim/4; i > 0; --i) {
      k00_20 = e0[-0] - e2[-0];
      k01_21 = e0[-1] - e2[-1];
      e0[-0] += e2[-0];//e0[-0] = e0[-0] + e2[-0];
      e0[-1] += e2[-1];//e0[-1] = e0[-1] + e2[-1];
      e2[-0] = (k00_20)*A[0] - (k01_21) * A[1];
      e2[-1] = (k01_21)*A[0] + (k00_20) * A[1];

      A += k1;

      k00_20 = e0[-2] - e2[-2];
      k01_21 = e0[-3] - e2[-3];
      e0[-2] += e2[-2];//e0[-2] = e0[-2] + e2[-2];
      e0[-3] += e2[-3];//e0[-3] = e0[-3] + e2[-3];
      e2[-2] = (k00_20)*A[0] - (k01_21) * A[1];
      e2[-3] = (k01_21)*A[0] + (k00_20) * A[1];

      A += k1;

      k00_20 = e0[-4] - e2[-4];
      k01_21 = e0[-5] - e2[-5];
      e0[-4] += e2[-4];//e0[-4] = e0[-4] + e2[-4];
      e0[-5] += e2[-5];//e0[-5] = e0[-5] + e2[-5];
      e2[-4] = (k00_20)*A[0] - (k01_21) * A[1];
      e2[-5] = (k01_21)*A[0] + (k00_20) * A[1];

      A += k1;

      k00_20 = e0[-6] - e2[-6];
      k01_21 = e0[-7] - e2[-7];
      e0[-6] += e2[-6];//e0[-6] = e0[-6] + e2[-6];
      e0[-7] += e2[-7];//e0[-7] = e0[-7] + e2[-7];
      e2[-6] = (k00_20)*A[0] - (k01_21) * A[1];
      e2[-7] = (k01_21)*A[0] + (k00_20) * A[1];

      e0 -= 8;
      e2 -= 8;

      A += k1;
   }
}

static void imdct_step3_inner_s_loop(int n, float *e, int i_off, int k_off, float *A, int a_off, int k0)
{
   const float A0 = A[0];
   const float A1 = A[0+1];
   const float A2 = A[0+a_off];
   const float A3 = A[0+a_off+1];
   const float A4 = A[0+a_off*2+0];
   const float A5 = A[0+a_off*2+1];
   const float A6 = A[0+a_off*3+0];
   const float A7 = A[0+a_off*3+1];

   float k00,k11;

   float *ee0 = e  +i_off;
   float *ee2 = ee0+k_off;

   for (int i=n; i > 0; --i) {
      k00     = ee0[ 0] - ee2[ 0];
      k11     = ee0[-1] - ee2[-1];
      ee0[ 0] =  ee0[ 0] + ee2[ 0];
      ee0[-1] =  ee0[-1] + ee2[-1];
      ee2[ 0] = (k00) * A0 - (k11) * A1;
      ee2[-1] = (k11) * A0 + (k00) * A1;

      k00     = ee0[-2] - ee2[-2];
      k11     = ee0[-3] - ee2[-3];
      ee0[-2] =  ee0[-2] + ee2[-2];
      ee0[-3] =  ee0[-3] + ee2[-3];
      ee2[-2] = (k00) * A2 - (k11) * A3;
      ee2[-3] = (k11) * A2 + (k00) * A3;

      k00     = ee0[-4] - ee2[-4];
      k11     = ee0[-5] - ee2[-5];
      ee0[-4] =  ee0[-4] + ee2[-4];
      ee0[-5] =  ee0[-5] + ee2[-5];
      ee2[-4] = (k00) * A4 - (k11) * A5;
      ee2[-5] = (k11) * A4 + (k00) * A5;

      k00     = ee0[-6] - ee2[-6];
      k11     = ee0[-7] - ee2[-7];
      ee0[-6] =  ee0[-6] + ee2[-6];
      ee0[-7] =  ee0[-7] + ee2[-7];
      ee2[-6] = (k00) * A6 - (k11) * A7;
      ee2[-7] = (k11) * A6 + (k00) * A7;

      ee0 -= k0;
      ee2 -= k0;
   }
}

static inline void iter_54(float *z)
{
   const float k00  = z[ 0] - z[-4];
   const float y0   = z[ 0] + z[-4];
   const float y2   = z[-2] + z[-6];
   const float k22  = z[-2] - z[-6];

   z[-0] = y0 + y2;      // z0 + z4 + z2 + z6
   z[-2] = y0 - y2;      // z0 + z4 - z2 - z6

   // done with y0,y2

   const float k33  = z[-3] - z[-7];

   z[-4] = k00 + k33;    // z0 - z4 + z3 - z7
   z[-6] = k00 - k33;    // z0 - z4 - z3 + z7

   // done with k33

   const float k11  = z[-1] - z[-5];
   const float y1   = z[-1] + z[-5];
   const float y3   = z[-3] + z[-7];

   z[-1] = y1 + y3;      // z1 + z5 + z3 + z7
   z[-3] = y1 - y3;      // z1 + z5 - z3 - z7
   z[-5] = k11 - k22;    // z1 - z5 + z2 - z6
   z[-7] = k11 + k22;    // z1 - z5 - z2 + z6
}

static void imdct_step3_inner_s_loop_ld654(int n, float *e, int i_off, float *A, int base_n)
{
   const int a_off = base_n/8;
   const float A2 = A[0+a_off];
   float *z = e + i_off;
   float *base = z - 16 * n;

   while (z > base) {
      float k00,k11;

      k00   = z[-0] - z[-8];
      k11   = z[-1] - z[-9];
      z[-0] = z[-0] + z[-8];
      z[-1] = z[-1] + z[-9];
      z[-8] =  k00;
      z[-9] =  k11 ;

      k00    = z[ -2] - z[-10];
      k11    = z[ -3] - z[-11];
      z[ -2] = z[ -2] + z[-10];
      z[ -3] = z[ -3] + z[-11];
      z[-10] = (k00+k11) * A2;
      z[-11] = (k11-k00) * A2;

      k00    = z[-12] - z[ -4];  // reverse to avoid a unary negation
      k11    = z[ -5] - z[-13];
      z[ -4] = z[ -4] + z[-12];
      z[ -5] = z[ -5] + z[-13];
      z[-12] = k11;
      z[-13] = k00;

      k00    = z[-14] - z[ -6];  // reverse to avoid a unary negation
      k11    = z[ -7] - z[-15];
      z[ -6] = z[ -6] + z[-14];
      z[ -7] = z[ -7] + z[-15];
      z[-14] = (k00+k11) * A2;
      z[-15] = (k00-k11) * A2;

      iter_54(z);
      iter_54(z-8);
      z -= 16;
   }
}

static void inverse_mdct(float *buffer, const int n, const int log2_n, stb_vorbis *handle, int blocktype)
{
   int l;
   float *buf2 = handle->imdct_temp_buf;
   float *u=NULL,*v=NULL;
   // twiddle factors
   float *A = handle->A[blocktype];

   // IMDCT algorithm from "The use of multirate filter banks for coding of high quality digital audio"
   // See notes about bugs in that paper in less-optimal implementation 'inverse_mdct_old' after this function.

   // kernel from paper


   // merged:
   //   copy and reflect spectral data
   //   step 0

   // note that it turns out that the items added together during
   // this step are, in fact, being added to themselves (as reflected
   // by step 0). inexplicable inefficiency! this became obvious
   // once I combined the passes.

   // so there's a missing 'times 2' here (for adding X to itself).
   // this propogates through linearly to the end, where the numbers
   // are 1/2 too small, and need to be compensated for.

   {
      float *d,*e, *AA, *e_stop;
      d = &buf2[(n/2)-2];
      AA = A;
      e = &buffer[0];
      e_stop = &buffer[(n/2)];
      while (e != e_stop) {
         d[1] = (e[0] * AA[0] - e[2]*AA[1]);
         d[0] = (e[0] * AA[1] + e[2]*AA[0]);
         d -= 2;
         AA += 2;
         e += 4;
      }

      e = &buffer[(n/2)-3];
      while (d >= buf2) {
         d[1] = (-e[2] * AA[0] - -e[0]*AA[1]);
         d[0] = (-e[2] * AA[1] + -e[0]*AA[0]);
         d -= 2;
         AA += 2;
         e -= 4;
      }
   }

   // now we use symbolic names for these, so that we can
   // possibly swap their meaning as we change which operations
   // are in place

   u = buffer;
   v = buf2;

   // step 2    (paper output is w, now u)
   // this could be in place, but the data ends up in the wrong
   // place... _somebody_'s got to swap it, so this is nominated
   {
      float *AA = &A[(n/2)-8];
      float *d0,*d1, *e0, *e1;

      e0 = &v[(n/4)];
      e1 = &v[0];

      d0 = &u[(n/4)];
      d1 = &u[0];

      while (AA >= A) {
         float v40_20, v41_21;

         v41_21 = e0[1] - e1[1];
         v40_20 = e0[0] - e1[0];
         d0[1]  = e0[1] + e1[1];
         d0[0]  = e0[0] + e1[0];
         d1[1]  = v41_21*AA[4] - v40_20*AA[5];
         d1[0]  = v40_20*AA[4] + v41_21*AA[5];

         v41_21 = e0[3] - e1[3];
         v40_20 = e0[2] - e1[2];
         d0[3]  = e0[3] + e1[3];
         d0[2]  = e0[2] + e1[2];
         d1[3]  = v41_21*AA[0] - v40_20*AA[1];
         d1[2]  = v40_20*AA[0] + v41_21*AA[1];

         AA -= 8;

         d0 += 4;
         d1 += 4;
         e0 += 4;
         e1 += 4;
      }
   }

   // step 3

   // optimized step 3:

   // the original step3 loop can be nested r inside s or s inside r;
   // it's written originally as s inside r, but this is dumb when r
   // iterates many times, and s few. So I have two copies of it and
   // switch between them halfway.

   // this is iteration 0 of step 3
   imdct_step3_iter0_loop(n/16, u, (n/2)-1-(n/4)*0, -(n/8), A);
   imdct_step3_iter0_loop(n/16, u, (n/2)-1-(n/4)*1, -(n/8), A);

   // this is iteration 1 of step 3
   imdct_step3_inner_r_loop(n/32, u, (n/2)-1 - (n/8)*0, -(n/16), A, 16);
   imdct_step3_inner_r_loop(n/32, u, (n/2)-1 - (n/8)*1, -(n/16), A, 16);
   imdct_step3_inner_r_loop(n/32, u, (n/2)-1 - (n/8)*2, -(n/16), A, 16);
   imdct_step3_inner_r_loop(n/32, u, (n/2)-1 - (n/8)*3, -(n/16), A, 16);

   l=2;
   for (; l < (log2_n-3)/2; ++l) {
      const int k0 = n >> (l+2);
      const int lim = 1 << (l+1);
      int i;
      for (i=0; i < lim; ++i)
         imdct_step3_inner_r_loop(n >> (l+4), u, (n/2)-1 - k0*i, -(k0/2), A, 1 << (l+3));
   }

   for (; l < log2_n-6; ++l) {
      const int k0 = n >> (l+2), k1 = 1 << (l+3);
      const int rlim = n >> (l+6);
      const int lim = 1 << (l+1);
      int i_off;
      float *A0 = A;
      i_off = (n/2)-1;
      for (int r=rlim; r > 0; --r) {
         imdct_step3_inner_s_loop(lim, u, i_off, -(k0/2), A0, k1, k0);
         A0 += k1*4;
         i_off -= 8;
      }
   }

   // iterations with count:
   //   log2_n-6,-5,-4 all interleaved together
   //       the big win comes from getting rid of needless flops
   //         due to the constants on pass 5 & 4 being all 1 and 0;
   //       combining them to be simultaneous to improve cache made little difference
   imdct_step3_inner_s_loop_ld654(n/32, u, (n/2)-1, A, n);

   // output is u

   // step 4, 5, and 6
   // cannot be in-place because of step 5
   {
      uint16_t *bitrev = handle->bit_reverse[blocktype];
      // weirdly, I'd have thought reading sequentially and writing
      // erratically would have been better than vice-versa, but in
      // fact that's not what my testing showed. (That is, with
      // j = bitreverse(i), do you read i and write j, or read j and write i.)

      float *d0 = &v[(n/4)-4];
      float *d1 = &v[(n/2)-4];
      while (d0 >= v) {
         int k4;

         k4 = bitrev[0];
         d1[3] = u[k4+0];
         d1[2] = u[k4+1];
         d0[3] = u[k4+2];
         d0[2] = u[k4+3];

         k4 = bitrev[1];
         d1[1] = u[k4+0];
         d1[0] = u[k4+1];
         d0[1] = u[k4+2];
         d0[0] = u[k4+3];
         
         d0 -= 4;
         d1 -= 4;
         bitrev += 2;
      }
   }
   // (paper output is u, now v)


   // data must be in buf2
   ASSERT(v == buf2);

   // step 7   (paper output is v, now v)
   // this is now in place
   {
      float *C = handle->C[blocktype];
      float *d, *e;

      d = v;
      e = v + (n/2) - 4;

      while (d < e) {
         float a02,a11,b0,b1,b2,b3;

         a02 = d[0] - e[2];
         a11 = d[1] + e[3];

         b0 = C[1]*a02 + C[0]*a11;
         b1 = C[1]*a11 - C[0]*a02;

         b2 = d[0] + e[ 2];
         b3 = d[1] - e[ 3];

         d[0] = b2 + b0;
         d[1] = b3 + b1;
         e[2] = b2 - b0;
         e[3] = b1 - b3;

         a02 = d[2] - e[0];
         a11 = d[3] + e[1];

         b0 = C[3]*a02 + C[2]*a11;
         b1 = C[3]*a11 - C[2]*a02;

         b2 = d[2] + e[ 0];
         b3 = d[3] - e[ 1];

         d[2] = b2 + b0;
         d[3] = b3 + b1;
         e[0] = b2 - b0;
         e[1] = b1 - b3;

         C += 4;
         d += 4;
         e -= 4;
      }
   }

   // data must be in buf2


   // step 8+decode   (paper output is X, now buffer)
   // this generates pairs of data a la 8 and pushes them directly through
   // the decode kernel (pushing rather than pulling) to avoid having
   // to make another pass later

   // this cannot POSSIBLY be in place, so we refer to the buffers directly

   {
      float *d0,*d1,*d2,*d3;

      float *B = handle->B[blocktype] + (n/2) - 8;
      float *e = buf2 + (n/2) - 8;
      d0 = &buffer[0];
      d1 = &buffer[(n/2)-4];
      d2 = &buffer[(n/2)];
      d3 = &buffer[n-4];
      while (e >= v) {
         float p0,p1,p2,p3;

         p3 =  e[6]*B[7] - e[7]*B[6];
         p2 = -e[6]*B[6] - e[7]*B[7]; 

         d0[0] =   p3;
         d1[3] = - p3;
         d2[0] =   p2;
         d3[3] =   p2;

         p1 =  e[4]*B[5] - e[5]*B[4];
         p0 = -e[4]*B[4] - e[5]*B[5]; 

         d0[1] =   p1;
         d1[2] = - p1;
         d2[1] =   p0;
         d3[2] =   p0;

         p3 =  e[2]*B[3] - e[3]*B[2];
         p2 = -e[2]*B[2] - e[3]*B[3]; 

         d0[2] =   p3;
         d1[1] = - p3;
         d2[2] =   p2;
         d3[1] =   p2;

         p1 =  e[0]*B[1] - e[1]*B[0];
         p0 = -e[0]*B[0] - e[1]*B[1]; 

         d0[3] =   p1;
         d1[0] = - p1;
         d2[3] =   p0;
         d3[0] =   p0;

         B -= 8;
         e -= 8;
         d0 += 4;
         d2 += 4;
         d1 -= 4;
         d3 -= 4;
      }
   }
}

#if 0
// this is the original version of the above code, if you want to optimize it from scratch
void inverse_mdct_naive(float *buffer, int n, int log2_n)
{
   float s;
   float A[1 << 12], B[1 << 12], C[1 << 11];
   int i,k,k2,k4, n2 = n >> 1, n4 = n >> 2, n8 = n >> 3, l;
   int n3_4 = n - n4;
   // how can they claim this only uses N words?!
   // oh, because they're only used sparsely, whoops
   float u[1 << 13], X[1 << 13], v[1 << 13], w[1 << 13];
   // set up twiddle factors

   for (k=k2=0; k < n4; ++k,k2+=2) {
      A[k2  ] = (float)  cos(4*k*M_PI/n);
      A[k2+1] = (float) -sin(4*k*M_PI/n);
      B[k2  ] = (float)  cos((k2+1)*M_PI/n/2);
      B[k2+1] = (float)  sin((k2+1)*M_PI/n/2);
   }
   for (k=k2=0; k < n8; ++k,k2+=2) {
      C[k2  ] = (float)  cos(2*(k2+1)*M_PI/n);
      C[k2+1] = (float) -sin(2*(k2+1)*M_PI/n);
   }

   // IMDCT algorithm from "The use of multirate filter banks for coding of high quality digital audio"
   // Note there are bugs in that pseudocode, presumably due to them attempting
   // to rename the arrays nicely rather than representing the way their actual
   // implementation bounces buffers back and forth. As a result, even in the
   // "some formulars corrected" version, a direct implementation fails. These
   // are noted below as "paper bug".

   // copy and reflect spectral data
   for (k=0; k < n2; ++k) u[k] = buffer[k];
   for (   ; k < n ; ++k) u[k] = -buffer[n - k - 1];
   // kernel from paper
   // step 1
   for (k=k2=k4=0; k < n4; k+=1, k2+=2, k4+=4) {
      v[n-k4-1] = (u[k4] - u[n-k4-1]) * A[k2]   - (u[k4+2] - u[n-k4-3])*A[k2+1];
      v[n-k4-3] = (u[k4] - u[n-k4-1]) * A[k2+1] + (u[k4+2] - u[n-k4-3])*A[k2];
   }
   // step 2
   for (k=k4=0; k < n8; k+=1, k4+=4) {
      w[n2+3+k4] = v[n2+3+k4] + v[k4+3];
      w[n2+1+k4] = v[n2+1+k4] + v[k4+1];
      w[k4+3]    = (v[n2+3+k4] - v[k4+3])*A[n2-4-k4] - (v[n2+1+k4]-v[k4+1])*A[n2-3-k4];
      w[k4+1]    = (v[n2+1+k4] - v[k4+1])*A[n2-4-k4] + (v[n2+3+k4]-v[k4+3])*A[n2-3-k4];
   }
   // step 3
   for (l=0; l < log2_n-3; ++l) {
      int k0 = n >> (l+2), k1 = 1 << (l+3);
      int rlim = n >> (l+4), r4, r;
      int s2lim = 1 << (l+2), s2;
      for (r=r4=0; r < rlim; r4+=4,++r) {
         for (s2=0; s2 < s2lim; s2+=2) {
            u[n-1-k0*s2-r4] = w[n-1-k0*s2-r4] + w[n-1-k0*(s2+1)-r4];
            u[n-3-k0*s2-r4] = w[n-3-k0*s2-r4] + w[n-3-k0*(s2+1)-r4];
            u[n-1-k0*(s2+1)-r4] = (w[n-1-k0*s2-r4] - w[n-1-k0*(s2+1)-r4]) * A[r*k1]
                                - (w[n-3-k0*s2-r4] - w[n-3-k0*(s2+1)-r4]) * A[r*k1+1];
            u[n-3-k0*(s2+1)-r4] = (w[n-3-k0*s2-r4] - w[n-3-k0*(s2+1)-r4]) * A[r*k1]
                                + (w[n-1-k0*s2-r4] - w[n-1-k0*(s2+1)-r4]) * A[r*k1+1];
         }
      }
      if (l+1 < log2_n-3) {
         // paper bug: ping-ponging of u&w here is omitted
         memcpy(w, u, sizeof(u));
      }
   }

   // step 4
   for (i=0; i < n8; ++i) {
      int j = bit_reverse(i) >> (32-log2_n+3);
      ASSERT(j < n8);
      if (i == j) {
         // paper bug: original code probably swapped in place; if copying,
         //            need to directly copy in this case
         int i8 = i << 3;
         v[i8+1] = u[i8+1];
         v[i8+3] = u[i8+3];
         v[i8+5] = u[i8+5];
         v[i8+7] = u[i8+7];
      } else if (i < j) {
         int i8 = i << 3, j8 = j << 3;
         v[j8+1] = u[i8+1], v[i8+1] = u[j8 + 1];
         v[j8+3] = u[i8+3], v[i8+3] = u[j8 + 3];
         v[j8+5] = u[i8+5], v[i8+5] = u[j8 + 5];
         v[j8+7] = u[i8+7], v[i8+7] = u[j8 + 7];
      }
   }
   // step 5
   for (k=0; k < n2; ++k) {
      w[k] = v[k*2+1];
   }
   // step 6
   for (k=k2=k4=0; k < n8; ++k, k2 += 2, k4 += 4) {
      u[n-1-k2] = w[k4];
      u[n-2-k2] = w[k4+1];
      u[n3_4 - 1 - k2] = w[k4+2];
      u[n3_4 - 2 - k2] = w[k4+3];
   }
   // step 7
   for (k=k2=0; k < n8; ++k, k2 += 2) {
      v[n2 + k2 ] = ( u[n2 + k2] + u[n-2-k2] + C[k2+1]*(u[n2+k2]-u[n-2-k2]) + C[k2]*(u[n2+k2+1]+u[n-2-k2+1]))/2;
      v[n-2 - k2] = ( u[n2 + k2] + u[n-2-k2] - C[k2+1]*(u[n2+k2]-u[n-2-k2]) - C[k2]*(u[n2+k2+1]+u[n-2-k2+1]))/2;
      v[n2+1+ k2] = ( u[n2+1+k2] - u[n-1-k2] + C[k2+1]*(u[n2+1+k2]+u[n-1-k2]) - C[k2]*(u[n2+k2]-u[n-2-k2]))/2;
      v[n-1 - k2] = (-u[n2+1+k2] + u[n-1-k2] + C[k2+1]*(u[n2+1+k2]+u[n-1-k2]) - C[k2]*(u[n2+k2]-u[n-2-k2]))/2;
   }
   // step 8
   for (k=k2=0; k < n4; ++k,k2 += 2) {
      X[k]      = v[k2+n2]*B[k2  ] + v[k2+1+n2]*B[k2+1];
      X[n2-1-k] = v[k2+n2]*B[k2+1] - v[k2+1+n2]*B[k2  ];
   }

   // decode kernel to output
   // determined the following value experimentally
   // (by first figuring out what made inverse_mdct_slow work); then matching that here
   // (probably vorbis encoder premultiplies by n or n/2, to save it on the decoder?)
   s = 0.5; // theoretically would be n4

   // [[[ note! the s value of 0.5 is compensated for by the B[] in the current code,
   //     so it needs to use the "old" B values to behave correctly, or else
   //     set s to 1.0 ]]]
   for (i=0; i < n4  ; ++i) buffer[i] = s * X[i+n4];
   for (   ; i < n3_4; ++i) buffer[i] = -s * X[n3_4 - i - 1];
   for (   ; i < n   ; ++i) buffer[i] = -s * X[i - n3_4];
}
#endif

/*************************************************************************/
/******************* Main decoding routine (internal) ********************/
/*************************************************************************/

/**
 * vorbis_decode_packet_rest:  Perform all decoding operations for a frame
 * except mode selection and windowing.
 *
 * [Parameters]
 *     handle: Stream handle.
 *     mode: Mode configuration for this frame.
 *     left_start_ptr: Pointer to variable holding the start position of
 *         the left overlap region.  May be modified on return.
 *     left_end: End position of the left overlap region.
 *     right_start: Start position of the right overlap region.
 *     right_end: End position of the right overlap region.
 *     len_ret: Pointer to variable to receive the frame length (including
 *         the right window).
 * [Return value]
 *     True on success, false on error.
 */
static bool vorbis_decode_packet_rest(
    stb_vorbis *handle, const Mode *mode, int *left_start_ptr, int left_end,
    int right_start, int right_end, int *len_ret)
{
    const int n = handle->blocksize[mode->blockflag];
    const Mapping *map = &handle->mapping[mode->mapping];

    /**** Floor processing (4.3.2). ****/

    bool zero_channel[256];
    for (int ch = 0; ch < handle->channels; ch++) {
        const int floor_index = map->submap_floor[map->mux[ch]];
        if (handle->floor_types[floor_index] == 0) {
            // FIXME: floor 0 not yet supported
            return error(handle, VORBIS_invalid_stream);
        } else {  // handle->floor_types[floor_index] == 1
            Floor1 *floor = &handle->floor_config[floor_index].floor1;
            zero_channel[ch] =
                !decode_floor1(handle, floor, handle->final_Y[ch]);
        }
    }

    /**** Nonzero vector propatation (4.3.3). ****/
    bool really_zero_channel[256];
    memcpy(really_zero_channel, zero_channel,
           sizeof(zero_channel[0]) * handle->channels);
    for (int i = 0; i < map->coupling_steps; i++) {
        if (!zero_channel[map->coupling[i].magnitude]
         || !zero_channel[map->coupling[i].angle]) {
            zero_channel[map->coupling[i].magnitude] = false;
            zero_channel[map->coupling[i].angle] = false;
        }
    }

    /**** Residue decoding (4.3.4). ****/
    for (int i = 0; i < map->submaps; i++) {
        // FIXME: 256 pointers on stack is a bit much
        float *residue_buffers[256];
        bool do_not_decode[256];
        int ch = 0;
        for (int j = 0; j < handle->channels; j++) {
            if (map->mux[j] == i) {
                if (zero_channel[j]) {
                    do_not_decode[ch] = true;
                    residue_buffers[ch] = NULL;
                } else {
                    do_not_decode[ch] = false;
                    residue_buffers[ch] = handle->channel_buffers[j];
                }
                ch++;
            }
        }
        decode_residue(handle, residue_buffers, ch, n/2,
                       map->submap_residue[i], do_not_decode);
    }

    /**** Inverse coupling (4.3.5). ****/
    for (int i = map->coupling_steps-1; i >= 0; i--) {
        float *magnitude = handle->channel_buffers[map->coupling[i].magnitude];
        float *angle = handle->channel_buffers[map->coupling[i].angle];
        for (int j = 0; j < n/2; j++) {
            const float M = magnitude[j];
            const float A = angle[j];
            float new_M, new_A;
            if (M > 0) {
                if (A > 0) {
                    new_M = M;
                    new_A = M - A;
                } else {
                    new_A = M;
                    new_M = M + A;
                }
            } else {
                if (A > 0) {
                    new_M = M;
                    new_A = M + A;
                } else {
                    new_A = M;
                    new_M = M - A;
                }
            }
            magnitude[j] = new_M;
            angle[j] = new_A;
        }
    }

    /**** Floor curve synthesis and dot product (4.3.6). ****/
    for (int i = 0; i < handle->channels; i++) {
        if (really_zero_channel[i]) {
            memset(handle->channel_buffers[i], 0,
                   sizeof(*handle->channel_buffers[i]) * (n/2));
        } else {
            const int floor_index = map->submap_floor[map->mux[i]];
            if (handle->floor_types[floor_index] == 0) {
                // FIXME: floor 0 not yet supported
            } else {  // handle->floor_types[floor_index] == 1
                Floor1 *floor = &handle->floor_config[floor_index].floor1;
                do_floor1_final(handle, floor, i, n);
            }
        }
    }

    /**** Inverse MDCT (4.3.7). ****/
    for (int i = 0; i < handle->channels; i++) {
        inverse_mdct(handle->channel_buffers[i], n,
                     handle->blocksize_bits[mode->blockflag], handle,
                     mode->blockflag);
    }

    /**** Frame length and other fixups. ****/

    /* Flush any leftover data in the current packet so the next packet
     * doesn't try to read it. */
    flush_packet(handle);

    /* Deal with discarding samples from the first packet. */
    if (handle->first_decode) {
        /* We don't support files with nonzero start positions (what the
         * spec describes as "The granule (PCM) position of the first page
         * need not indicate that the stream started at position zero..."
         * in A.2), so we just omit everything to the left of the right
         * overlap start. */
        // FIXME: find a test file with a long->short start and make sure
        // it still works
        /* Set up current_loc so the second frame (first frame returned)
         * has position zero. */
        handle->current_loc = -(right_start - *left_start_ptr);
        handle->current_loc_valid = true;
        handle->first_decode = false;
    }

    /* If this is the last complete frame in an Ogg page, update the sample
     * position and possibly the frame length based on the Ogg granule
     * position. */
    if (handle->last_seg_index == handle->end_seg_with_known_loc) {
        if (handle->current_loc_valid
         && (handle->page_flag & PAGEFLAG_last_page)) {
            /* For the very last frame in the stream, the Ogg granule
             * position specifies (indirectly) how many samples to return;
             * this may be less than the normal length, or it may include
             * part of the right overlap region which is not normally
             * returned until the next packet.  If the granule position
             * indicates a packet length longer than the window size, we
             * assume that the file is broken and just revert to our usual
             * processing. */
            const uint64_t current_end =
                handle->known_loc_for_packet - (n - right_end);
            if (current_end < handle->current_loc + right_end) {
                if (current_end < handle->current_loc) {
                    *len_ret = 0;  // Oops, tried to truncate to negative len!
                } else {
                    *len_ret = current_end - handle->current_loc;
                }
                *len_ret += *left_start_ptr;
                handle->current_loc += *len_ret;
                return true;
            }
        }
        /* For non-final pages, the granule position should be the "last
         * complete sample returned by decode" (A.2), thus right_start.
         * However, it appears that the reference encoder blindly uses the
         * middle of the window (n/2) even on a long/short frame boundary. */
        handle->current_loc =
            handle->known_loc_for_packet - (n/2 - *left_start_ptr);
        handle->current_loc_valid = true;
    }

    /* Update the current sample position for samples returned from this
     * frame. */
    if (handle->current_loc_valid) {
        handle->current_loc += (right_start - *left_start_ptr);
    }

    /* Set the frame length to the end of the right overlap region (limit
     * of valid data).  vorbis_finish_frame() will trim it as necessary. */
    *len_ret = right_end;

    return true;
}

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

bool vorbis_decode_initial(stb_vorbis *handle, int *left_start_ret,
                           int *left_end_ret, int *right_start_ret,
                           int *right_end_ret, int *mode_ret)
{
    /* Start reading the next packet, and verify that it's an audio packet. */
    if (UNLIKELY(handle->eof)) {
        return false;
    }
    if (UNLIKELY(!start_packet(handle))) {
        if (handle->eof
         && handle->error == VORBIS_missing_capture_pattern_or_eof) {
            /* EOF at page boundary is not an error! */
            handle->error = VORBIS__no_error;
        }
        return false;
    }
    if (UNLIKELY(get_bits(handle, 1) != 0)) {
        /* Not an audio packet, so skip it and try again. */
        flush_packet(handle);
        return vorbis_decode_initial(handle, left_start_ret, left_end_ret,
                                     right_start_ret, right_end_ret, mode_ret);
    }

    /* Read the mode number and window flags for this packet. */
    const int mode_index = get_bits(handle, handle->mode_bits);
    /* Still in the same byte, so we can't hit EOP here. */
    ASSERT(mode_index != EOP);
    if (mode_index >= handle->mode_count) {
        handle->error = VORBIS_invalid_packet;
        flush_packet(handle);
        return false;
    }
    *mode_ret = mode_index;
    Mode *mode = &handle->mode_config[mode_index];
    bool next, prev;
    if (mode->blockflag) {
        prev = get_bits(handle, 1);
        const int temp = get_bits(handle, 1);
        if (temp == EOP) {
            handle->error = VORBIS_invalid_packet;
            flush_packet(handle);
            return false;
        }
        next = temp;
    } else {
        prev = next = false;
    }

    /* Set up the window bounds for this frame. */
    const int n = handle->blocksize[mode->blockflag];
    if (mode->blockflag && !prev) {
        *left_start_ret = (n - handle->blocksize[0]) / 4;
        *left_end_ret   = (n + handle->blocksize[0]) / 4;
    } else {
        *left_start_ret = 0;
        *left_end_ret   = n/2;
    }
    if (mode->blockflag && !next) {
        *right_start_ret = (n*3 - handle->blocksize[0]) / 4;
        *right_end_ret   = (n*3 + handle->blocksize[0]) / 4;
    } else {
        *right_start_ret = n/2;
        *right_end_ret   = n;
    }

    return true;
}

/*-----------------------------------------------------------------------*/

bool vorbis_decode_packet(stb_vorbis *handle, int *len_ret,
                          int *left_ret, int *right_ret)
{
    int mode, left_end, right_end;
    return vorbis_decode_initial(handle, left_ret, &left_end,
                                 right_ret, &right_end, &mode)
        && vorbis_decode_packet_rest(handle, &handle->mode_config[mode],
                                     left_ret, left_end, *right_ret, right_end,
                                     len_ret);
}

/*-----------------------------------------------------------------------*/

int vorbis_finish_frame(stb_vorbis *handle, int len, int left, int right)
{
    /* The spec has some moderately complex rules for determining which
     * part of the window to return, but all those end up saying is that
     * we return the part of the window beginning with the start point of
     * the left overlap region and ending with the start point of the
     * right overlap region.  We already have those values from the
     * decoding process, so we just reuse them here. */

    const int prev = handle->previous_length;

    /* Mix in data from the previous window's right side. */
    if (prev > 0) {
        const float *weights;
        if (prev*2 == handle->blocksize[0]) {
            weights = handle->window_weights[0];
        } else {
            ASSERT(prev*2 == handle->blocksize[1]);
            weights = handle->window_weights[1];
        }
        for (int i = 0; i < handle->channels; i++) {
            for (int j = 0; j < prev; j++) {
                handle->channel_buffers[i][left+j] =
                    handle->channel_buffers[i][left+j] * weights[       j] +
                    handle->previous_window[i][     j] * weights[prev-1-j];
            }
        }
    }

    /* Copy the right side of this window into the previous_window buffer. */
    handle->previous_length = len - right;
    for (int i = 0; i < handle->channels; i++) {
        for (int j = 0; j < len - right; j++) {
            handle->previous_window[i][j] = handle->channel_buffers[i][right+j];
        }
    }

    /* Return the final frame length.  If this is the first frame in the
     * file, the data returned will be garbage; the spec says that we
     * should return a data length of zero in this case, but since we
     * always discard that frame anyway, we don't bother with special
     * handling. */
    if (len < right) {
        return len - left;
    } else {
        return right - left;
    }
}

/*-----------------------------------------------------------------------*/

void vorbis_pump_frame(stb_vorbis *handle)
{
   int len, right, left;
   if (vorbis_decode_packet(handle, &len, &left, &right))
      vorbis_finish_frame(handle, len, left, right);
}

/*************************************************************************/
/*************************************************************************/
