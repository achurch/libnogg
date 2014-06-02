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
#include "src/decode/inlines.h"
#include "src/decode/io.h"
#include "src/decode/packet.h"
#include "src/util/memory.h"


//////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

/////////////////////// LEAF SETUP FUNCTIONS //////////////////////////
//
// these functions are only called at setup, and only a few times
// per file

static float float32_unpack(uint32_t x)
{
   // from the specification
   uint32_t mantissa = x & 0x1fffff;
   uint32_t sign = x & 0x80000000;
   uint32_t exp = (x & 0x7fe00000) >> 21;
   double res = sign ? -(double)mantissa : (double)mantissa;
   return (float) ldexp((float)res, exp-788);
}


// zlib & jpeg huffman tables assume that the output symbols
// can either be arbitrarily arranged, or have monotonically
// increasing frequencies--they rely on the lengths being sorted;
// this makes for a very simple generation algorithm.
// vorbis allows a huffman table with non-sorted lengths. This
// requires a more sophisticated construction, since symbols in
// order do not map to huffman codes "in order".
static void add_entry(Codebook *c, uint32_t huff_code, int symbol, int count, int len, uint32_t *values)
{
   if (!c->sparse) {
      c->codewords      [symbol] = huff_code;
   } else {
      c->codewords       [count] = huff_code;
      c->codeword_lengths[count] = len;
      values             [count] = symbol;
   }
}

static int compute_codewords(Codebook *c, uint8_t *len, int n, uint32_t *values)
{
   int i,k,m=0;
   uint32_t available[32];

   memset(available, 0, sizeof(available));
   // find the first entry
   for (k=0; k < n; ++k) if (len[k] < NO_CODE) break;
   if (k == n) { assert(c->sorted_entries == 0); return TRUE; }
   // add to the list
   add_entry(c, 0, k, m++, len[k], values);
   // add all available leaves
   for (i=1; i <= len[k]; ++i)
      available[i] = 1 << (32-i);
   // note that the above code treats the first case specially,
   // but it's really the same as the following code, so they
   // could probably be combined (except the initial code is 0,
   // and I use 0 in available[] to mean 'empty')
   for (i=k+1; i < n; ++i) {
      uint32_t res;
      int z = len[i], y;
      if (z == NO_CODE) continue;
      // find lowest available leaf (should always be earliest,
      // which is what the specification calls for)
      // note that this property, and the fact we can never have
      // more than one free leaf at a given level, isn't totally
      // trivial to prove, but it seems true and the assert never
      // fires, so!
      while (z > 0 && !available[z]) --z;
      if (z == 0) { assert(0); return FALSE; }
      res = available[z];
      available[z] = 0;
      add_entry(c, bit_reverse(res), i, m++, len[i], values);
      // propogate availability up the tree
      if (z != len[i]) {
         for (y=len[i]; y > z; --y) {
            assert(available[y] == 0);
            available[y] = res + (1 << (32-y));
         }
      }
   }
   return TRUE;
}

// accelerated huffman table allows fast O(1) match of all symbols
// of length <= STB_VORBIS_FAST_HUFFMAN_LENGTH
static void compute_accelerated_huffman(Codebook *c)
{
   int i, len;
   for (i=0; i < FAST_HUFFMAN_TABLE_SIZE; ++i)
      c->fast_huffman[i] = -1;

   len = c->sparse ? c->sorted_entries : c->entries;
   #ifdef STB_VORBIS_FAST_HUFFMAN_SHORT
   if (len > 32767) len = 32767; // largest possible value we can encode!
   #endif
   for (i=0; i < len; ++i) {
      if (c->codeword_lengths[i] <= STB_VORBIS_FAST_HUFFMAN_LENGTH) {
         uint32_t z = c->sparse ? bit_reverse(c->sorted_codewords[i]) : c->codewords[i];
         // set table entries for all bit combinations in the higher bits
         while (z < FAST_HUFFMAN_TABLE_SIZE) {
             c->fast_huffman[z] = i;
             z += 1 << c->codeword_lengths[i];
         }
      }
   }
}

static int uint32_t_compare(const void *p, const void *q)
{
   uint32_t x = * (uint32_t *) p;
   uint32_t y = * (uint32_t *) q;
   return x < y ? -1 : x > y;
}

static int include_in_sort(Codebook *c, uint8_t len)
{
   if (c->sparse) { assert(len != NO_CODE); return TRUE; }
   if (len == NO_CODE) return FALSE;
   if (len > STB_VORBIS_FAST_HUFFMAN_LENGTH) return TRUE;
   return FALSE;
}

// if the fast table above doesn't work, we want to binary
// search them... need to reverse the bits
static void compute_sorted_huffman(Codebook *c, uint8_t *lengths, uint32_t *values)
{
   int i, len;
   // build a list of all the entries
   // OPTIMIZATION: don't include the short ones, since they'll be caught by FAST_HUFFMAN.
   // this is kind of a frivolous optimization--I don't see any performance improvement,
   // but it's like 4 extra lines of code, so.
   if (!c->sparse) {
      int k = 0;
      for (i=0; i < c->entries; ++i)
         if (include_in_sort(c, lengths[i])) 
            c->sorted_codewords[k++] = bit_reverse(c->codewords[i]);
      assert(k == c->sorted_entries);
   } else {
      for (i=0; i < c->sorted_entries; ++i)
         c->sorted_codewords[i] = bit_reverse(c->codewords[i]);
   }

   qsort(c->sorted_codewords, c->sorted_entries, sizeof(c->sorted_codewords[0]), uint32_t_compare);
   c->sorted_codewords[c->sorted_entries] = 0xffffffff;

   len = c->sparse ? c->sorted_entries : c->entries;
   // now we need to indicate how they correspond; we could either
   //   #1: sort a different data structure that says who they correspond to
   //   #2: for each sorted entry, search the original list to find who corresponds
   //   #3: for each original entry, find the sorted entry
   // #1 requires extra storage, #2 is slow, #3 can use binary search!
   for (i=0; i < len; ++i) {
      int huff_len = c->sparse ? lengths[values[i]] : lengths[i];
      if (include_in_sort(c,huff_len)) {
         uint32_t code = bit_reverse(c->codewords[i]);
         int x=0, n=c->sorted_entries;
         while (n > 1) {
            // invariant: sc[x] <= code < sc[x+n]
            int m = x + (n >> 1);
            if (c->sorted_codewords[m] <= code) {
               x = m;
               n -= (n>>1);
            } else {
               n >>= 1;
            }
         }
         assert(c->sorted_codewords[x] == code);
         if (c->sparse) {
            c->sorted_values[x] = values[i];
            c->codeword_lengths[x] = huff_len;
         } else {
            c->sorted_values[x] = i;
         }
      }
   }
}

// only run while parsing the header (3 times)
static int vorbis_validate(uint8_t *data)
{
   static uint8_t vorbis[6] = { 'v', 'o', 'r', 'b', 'i', 's' };
   return memcmp(data, vorbis, 6) == 0;
}

// called from setup only, once per code book
// (formula implied by specification)
static int lookup1_values(int entries, int dim)
{
   int r = (int) floor(exp((float) log((float) entries) / dim));
   if ((int) floor(pow((float) r+1, dim)) <= entries)   // (int) cast for MinGW warning;
      ++r;                                              // floor() to avoid _ftol() when non-CRT
   assert(pow((float) r+1, dim) > entries);
   assert((int) floor(pow((float) r, dim)) <= entries); // (int),floor() as above
   return r;
}

// called twice per file
static void compute_twiddle_factors(int n, float *A, float *B, float *C)
{
   int n4 = n >> 2, n8 = n >> 3;
   int k,k2;

   for (k=k2=0; k < n4; ++k,k2+=2) {
      A[k2  ] = (float)  cos(4*k*M_PI/n);
      A[k2+1] = (float) -sin(4*k*M_PI/n);
      B[k2  ] = (float)  cos((k2+1)*M_PI/n/2) * 0.5f;
      B[k2+1] = (float)  sin((k2+1)*M_PI/n/2) * 0.5f;
   }
   for (k=k2=0; k < n8; ++k,k2+=2) {
      C[k2  ] = (float)  cos(2*(k2+1)*M_PI/n);
      C[k2+1] = (float) -sin(2*(k2+1)*M_PI/n);
   }
}

static void compute_window(int n, float *window)
{
   int n2 = n >> 1, i;
   for (i=0; i < n2; ++i)
      window[i] = (float) sin(0.5 * M_PI * square((float) sin((i - 0 + 0.5) / n2 * 0.5 * M_PI)));
}

static void compute_bitreverse(int n, uint16_t *rev)
{
   int ld = ilog(n) - 1; // ilog is off-by-one from normal definitions
   int i, n8 = n >> 3;
   for (i=0; i < n8; ++i)
      rev[i] = (bit_reverse(i) >> (32-ld+3)) << 2;
}

static int init_blocksize(stb_vorbis *f, int b, int n)
{
   int n2 = n >> 1, n4 = n >> 2, n8 = n >> 3;
   f->A[b] = (float *) malloc(sizeof(float) * n2);
   f->B[b] = (float *) malloc(sizeof(float) * n2);
   f->C[b] = (float *) malloc(sizeof(float) * n4);
   if (!f->A[b] || !f->B[b] || !f->C[b]) return error(f, VORBIS_outofmem);
   compute_twiddle_factors(n, f->A[b], f->B[b], f->C[b]);
   f->window[b] = (float *) malloc(sizeof(float) * n2);
   if (!f->window[b]) return error(f, VORBIS_outofmem);
   compute_window(n, f->window[b]);
   f->bit_reverse[b] = (uint16_t *) malloc(sizeof(uint16_t) * n8);
   if (!f->bit_reverse[b]) return error(f, VORBIS_outofmem);
   compute_bitreverse(n, f->bit_reverse[b]);
   return TRUE;
}

static void neighbors(uint16_t *x, int n, int *plow, int *phigh)
{
   int low = -1;
   int high = 65536;
   int i;
   for (i=0; i < n; ++i) {
      if (x[i] > low  && x[i] < x[n]) { *plow  = i; low = x[i]; }
      if (x[i] < high && x[i] > x[n]) { *phigh = i; high = x[i]; }
   }
}

// this has been repurposed so y is now the original index instead of y
typedef struct
{
   uint16_t x,y;
} Point;

static int point_compare(const void *p, const void *q)
{
   Point *a = (Point *) p;
   Point *b = (Point *) q;
   return a->x < b->x ? -1 : a->x > b->x;
}

//
/////////////////////// END LEAF SETUP FUNCTIONS //////////////////////////


static uint8_t ogg_page_header[4] = { 0x4f, 0x67, 0x67, 0x53 };

// @OPTIMIZE: primary accumulator for huffman
// expand the buffer to as many bits as possible without reading off end of packet
// it might be nice to allow f->valid_bits and f->acc to be stored in registers,
// e.g. cache them locally and decode locally
static inline void prep_huffman(stb_vorbis *f)
{
   if (f->valid_bits <= 24) {
      if (f->valid_bits == 0) f->acc = 0;
      do {
         int z;
         if (f->last_seg && !f->bytes_in_seg) return;
         z = get8_packet_raw(f);
         if (z == EOP) return;
         f->acc += z << f->valid_bits;
         f->valid_bits += 8;
      } while (f->valid_bits <= 24);
   }
}

enum
{
   VORBIS_packet_id = 1,
   VORBIS_packet_comment = 3,
   VORBIS_packet_setup = 5,
};

static int codebook_decode_scalar_raw(stb_vorbis *f, Codebook *c)
{
   int i;
   prep_huffman(f);

   assert(c->sorted_codewords || c->codewords);
   // cases to use binary search: sorted_codewords && !c->codewords
   //                             sorted_codewords && c->entries > 8
   if (c->sorted_codewords && (c->entries > 8 || !c->codewords)) {
      // binary search
      uint32_t code = bit_reverse(f->acc);
      int x=0, n=c->sorted_entries, len;

      while (n > 1) {
         // invariant: sc[x] <= code < sc[x+n]
         int m = x + (n >> 1);
         if (c->sorted_codewords[m] <= code) {
            x = m;
            n -= (n>>1);
         } else {
            n >>= 1;
         }
      }
      // x is now the sorted index
      if (!c->sparse) x = c->sorted_values[x];
      // x is now sorted index if sparse, or symbol otherwise
      len = c->codeword_lengths[x];
      if (f->valid_bits >= len) {
         f->acc >>= len;
         f->valid_bits -= len;
         return x;
      }

      f->valid_bits = 0;
      return -1;
   }

   // if small, linear search
   assert(!c->sparse);
   for (i=0; i < c->entries; ++i) {
      if (c->codeword_lengths[i] == NO_CODE) continue;
      if (c->codewords[i] == (f->acc & ((1 << c->codeword_lengths[i])-1))) {
         if (f->valid_bits >= c->codeword_lengths[i]) {
            f->acc >>= c->codeword_lengths[i];
            f->valid_bits -= c->codeword_lengths[i];
            return i;
         }
         f->valid_bits = 0;
         return -1;
      }
   }

   error(f, VORBIS_invalid_stream);
   f->valid_bits = 0;
   return -1;
}

static int codebook_decode_scalar(stb_vorbis *f, Codebook *c)
{
   int i;
   if (f->valid_bits < STB_VORBIS_FAST_HUFFMAN_LENGTH)
      prep_huffman(f);
   // fast huffman table lookup
   i = f->acc & FAST_HUFFMAN_TABLE_MASK;
   i = c->fast_huffman[i];
   if (i >= 0) {
      int n = c->codeword_lengths[i];
      f->acc >>= n;
      f->valid_bits -= n;
      if (f->valid_bits < 0) { f->valid_bits = 0; return -1; }
      return i;
   }
   return codebook_decode_scalar_raw(f,c);
}

#define DECODE(var,f,c)   do {                                \
   var = codebook_decode_scalar(f, c);                        \
   if (c->sparse) var = c->sorted_values[var];                \
} while (0)

#ifndef STB_VORBIS_DIVIDES_IN_CODEBOOK
  #define DECODE_VQ(var,f,c)   var = codebook_decode_scalar(f, c);
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

static int codebook_decode_start(stb_vorbis *f, Codebook *c, int len)
{
   int z = -1;

   // type 0 is only legal in a scalar context
   if (c->lookup_type == 0)
      error(f, VORBIS_invalid_stream);
   else {
      DECODE_VQ(z,f,c);
      if (c->sparse) assert(z < c->sorted_entries);
      if (z < 0) {  // check for EOP
         if (!f->bytes_in_seg)
            if (f->last_seg)
               return z;
         error(f, VORBIS_invalid_stream);
      }
   }
   return z;
}

static int codebook_decode(stb_vorbis *f, Codebook *c, float *output, int len)
{
   int i,z = codebook_decode_start(f,c,len);
   if (z < 0) return FALSE;
   if (len > c->dimensions) len = c->dimensions;

#ifdef STB_VORBIS_DIVIDES_IN_CODEBOOK
   if (c->lookup_type == 1) {
      float last = CODEBOOK_ELEMENT_BASE(c);
      int div = 1;
      for (i=0; i < len; ++i) {
         int off = (z / div) % c->lookup_values;
         float val = CODEBOOK_ELEMENT_FAST(c,off) + last;
         output[i] += val;
         if (c->sequence_p) last = val + c->minimum_value;
         div *= c->lookup_values;
      }
      return TRUE;
   }
#endif

   z *= c->dimensions;
   if (c->sequence_p) {
      float last = CODEBOOK_ELEMENT_BASE(c);
      for (i=0; i < len; ++i) {
         float val = CODEBOOK_ELEMENT_FAST(c,z+i) + last;
         output[i] += val;
         last = val + c->minimum_value;
      }
   } else {
      float last = CODEBOOK_ELEMENT_BASE(c);
      for (i=0; i < len; ++i) {
         output[i] += CODEBOOK_ELEMENT_FAST(c,z+i) + last;
      }
   }

   return TRUE;
}

static int codebook_decode_step(stb_vorbis *f, Codebook *c, float *output, int len, int step)
{
   int i,z = codebook_decode_start(f,c,len);
   float last = CODEBOOK_ELEMENT_BASE(c);
   if (z < 0) return FALSE;
   if (len > c->dimensions) len = c->dimensions;

#ifdef STB_VORBIS_DIVIDES_IN_CODEBOOK
   if (c->lookup_type == 1) {
      int div = 1;
      for (i=0; i < len; ++i) {
         int off = (z / div) % c->lookup_values;
         float val = CODEBOOK_ELEMENT_FAST(c,off) + last;
         output[i*step] += val;
         if (c->sequence_p) last = val;
         div *= c->lookup_values;
      }
      return TRUE;
   }
#endif

   z *= c->dimensions;
   for (i=0; i < len; ++i) {
      float val = CODEBOOK_ELEMENT_FAST(c,z+i) + last;
      output[i*step] += val;
      if (c->sequence_p) last = val;
   }

   return TRUE;
}

static int codebook_decode_deinterleave_repeat(stb_vorbis *f, Codebook *c, float **outputs, int ch, int *c_inter_p, int *p_inter_p, int len, int total_decode)
{
   int c_inter = *c_inter_p;
   int p_inter = *p_inter_p;
   int i,z, effective = c->dimensions;

   // type 0 is only legal in a scalar context
   if (c->lookup_type == 0)   return error(f, VORBIS_invalid_stream);

   while (total_decode > 0) {
      float last = CODEBOOK_ELEMENT_BASE(c);
      DECODE_VQ(z,f,c);
      #ifndef STB_VORBIS_DIVIDES_IN_CODEBOOK
      assert(!c->sparse || z < c->sorted_entries);
      #endif
      if (z < 0) {
         if (!f->bytes_in_seg)
            if (f->last_seg) return FALSE;
         return error(f, VORBIS_invalid_stream);
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
         for (i=0; i < effective; ++i) {
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
            for (i=0; i < effective; ++i) {
               float val = CODEBOOK_ELEMENT_FAST(c,z+i) + last;
               outputs[c_inter][p_inter] += val;
               if (++c_inter == ch) { c_inter = 0; ++p_inter; }
               last = val;
            }
         } else {
            for (i=0; i < effective; ++i) {
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
   return TRUE;
}

#ifndef STB_VORBIS_DIVIDES_IN_CODEBOOK
static int codebook_decode_deinterleave_repeat_2(stb_vorbis *f, Codebook *c, float **outputs, int *c_inter_p, int *p_inter_p, int len, int total_decode)
{
   int c_inter = *c_inter_p;
   int p_inter = *p_inter_p;
   int i,z, effective = c->dimensions;

   // type 0 is only legal in a scalar context
   if (c->lookup_type == 0)   return error(f, VORBIS_invalid_stream);

   while (total_decode > 0) {
      float last = CODEBOOK_ELEMENT_BASE(c);
      DECODE_VQ(z,f,c);

      if (z < 0) {
         if (!f->bytes_in_seg)
            if (f->last_seg) return FALSE;
         return error(f, VORBIS_invalid_stream);
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
            for (i=0; i < effective; ++i) {
               float val = CODEBOOK_ELEMENT_FAST(c,z+i) + last;
               outputs[c_inter][p_inter] += val;
               if (++c_inter == 2) { c_inter = 0; ++p_inter; }
               last = val;
            }
         } else {
            i=0;
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
   return TRUE;
}
#endif

static int predict_point(int x, int x0, int x1, int y0, int y1)
{
   int dy = y1 - y0;
   int adx = x1 - x0;
   // @OPTIMIZE: force int division to round in the right direction... is this necessary on x86?
   int err = abs(dy) * (x - x0);
   int off = err / adx;
   return dy < 0 ? y0 - off : y0 + off;
}

// the following table is block-copied from the specification
static float inverse_db_table[256] =
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


// @OPTIMIZE: if you want to replace this bresenham line-drawing routine,
// note that you must produce bit-identical output to decode correctly;
// this specific sequence of operations is specified in the spec (it's
// drawing integer-quantized frequency-space lines that the encoder
// expects to be exactly the same)
//     ... also, isn't the whole point of Bresenham's algorithm to NOT
// have to divide in the setup? sigh.

static inline void draw_line(float *output, int x0, int y0, int x1, int y1, int n)
{
   int dy = y1 - y0;
   int adx = x1 - x0;
   int ady = abs(dy);
   int base;
   int x=x0,y=y0;
   int err = 0;
   int sy;

   base = dy / adx;
   if (dy < 0)
      sy = base - 1;
   else
      sy = base+1;
   ady -= abs(base) * adx;
   if (x1 > n) x1 = n;
   output[x] *= inverse_db_table[y];
   for (++x; x < x1; ++x) {
      err += ady;
      if (err >= adx) {
         err -= adx;
         y += sy;
      } else
         y += base;
      output[x] *= inverse_db_table[y];
   }
}

static int residue_decode(stb_vorbis *f, Codebook *book, float *target, int offset, int n, int rtype)
{
   int k;
   if (rtype == 0) {
      int step = n / book->dimensions;
      for (k=0; k < step; ++k)
         if (!codebook_decode_step(f, book, target+offset+k, n-offset-k, step))
            return FALSE;
   } else {
      for (k=0; k < n; ) {
         if (!codebook_decode(f, book, target+offset, n-k))
            return FALSE;
         k += book->dimensions;
         offset += book->dimensions;
      }
   }
   return TRUE;
}

static void decode_residue(stb_vorbis *f, float *residue_buffers[], int ch, int n, int rn, uint8_t *do_not_decode)
{
   int i,j,pass;
   Residue *r = f->residue_config + rn;
   int rtype = f->residue_types[rn];
   int classbook = r->classbook;
   int classwords = f->codebooks[classbook].dimensions;
   int n_read = r->end - r->begin;
   int part_read = n_read / r->part_size;
   #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
   uint8_t ***part_classdata = f->part_classdata;
   #else
   int **classifications = f->classifications;
   #endif

   for (i=0; i < ch; ++i)
      if (!do_not_decode[i])
         memset(residue_buffers[i], 0, sizeof(float) * n);

   if (rtype == 2 && ch != 1) {
      for (j=0; j < ch; ++j)
         if (!do_not_decode[j])
            break;
      if (j == ch)
         goto done;

      for (pass=0; pass < 8; ++pass) {
         int pcount = 0, class_set = 0;
         if (ch == 2) {
            while (pcount < part_read) {
               int z = r->begin + pcount*r->part_size;
               int c_inter = (z & 1), p_inter = z>>1;
               if (pass == 0) {
                  Codebook *codebook = f->codebooks+r->classbook;
                  int q;
                  DECODE(q,f,codebook);
                  if (q == EOP) goto done;
                  #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
                  part_classdata[0][class_set] = r->classdata[q];
                  #else
                  for (i=classwords-1; i >= 0; --i) {
                     classifications[0][i+pcount] = q % r->classifications;
                     q /= r->classifications;
                  }
                  #endif
               }
               for (i=0; i < classwords && pcount < part_read; ++i, ++pcount) {
                  int z2 = r->begin + pcount*r->part_size;
                  #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
                  int c = part_classdata[0][class_set][i];
                  #else
                  int c = classifications[0][pcount];
                  #endif
                  int b = r->residue_books[c][pass];
                  if (b >= 0) {
                     Codebook *book = f->codebooks + b;
                     #ifdef STB_VORBIS_DIVIDES_IN_CODEBOOK
                     if (!codebook_decode_deinterleave_repeat(f, book, residue_buffers, ch, &c_inter, &p_inter, n, r->part_size))
                        goto done;
                     #else
                     // saves 1%
                     if (!codebook_decode_deinterleave_repeat_2(f, book, residue_buffers, &c_inter, &p_inter, n, r->part_size))
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
                  Codebook *c = f->codebooks+r->classbook;
                  int q;
                  DECODE(q,f,c);
                  if (q == EOP) goto done;
                  #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
                  part_classdata[0][class_set] = r->classdata[q];
                  #else
                  for (i=classwords-1; i >= 0; --i) {
                     classifications[0][i+pcount] = q % r->classifications;
                     q /= r->classifications;
                  }
                  #endif
               }
               for (i=0; i < classwords && pcount < part_read; ++i, ++pcount) {
                  int z2 = r->begin + pcount*r->part_size;
                  #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
                  int c = part_classdata[0][class_set][i];
                  #else
                  int c = classifications[0][pcount];
                  #endif
                  int b = r->residue_books[c][pass];
                  if (b >= 0) {
                     Codebook *book = f->codebooks + b;
                     if (!codebook_decode_deinterleave_repeat(f, book, residue_buffers, ch, &c_inter, &p_inter, n, r->part_size))
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
                  Codebook *c = f->codebooks+r->classbook;
                  int q;
                  DECODE(q,f,c);
                  if (q == EOP) goto done;
                  #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
                  part_classdata[0][class_set] = r->classdata[q];
                  #else
                  for (i=classwords-1; i >= 0; --i) {
                     classifications[0][i+pcount] = q % r->classifications;
                     q /= r->classifications;
                  }
                  #endif
               }
               for (i=0; i < classwords && pcount < part_read; ++i, ++pcount) {
                  int z2 = r->begin + pcount*r->part_size;
                  #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
                  int c = part_classdata[0][class_set][i];
                  #else
                  int c = classifications[0][pcount];
                  #endif
                  int b = r->residue_books[c][pass];
                  if (b >= 0) {
                     Codebook *book = f->codebooks + b;
                     if (!codebook_decode_deinterleave_repeat(f, book, residue_buffers, ch, &c_inter, &p_inter, n, r->part_size))
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

   for (pass=0; pass < 8; ++pass) {
      int pcount = 0, class_set=0;
      while (pcount < part_read) {
         if (pass == 0) {
            for (j=0; j < ch; ++j) {
               if (!do_not_decode[j]) {
                  Codebook *c = f->codebooks+r->classbook;
                  int temp;
                  DECODE(temp,f,c);
                  if (temp == EOP) goto done;
                  #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
                  part_classdata[j][class_set] = r->classdata[temp];
                  #else
                  for (i=classwords-1; i >= 0; --i) {
                     classifications[j][i+pcount] = temp % r->classifications;
                     temp /= r->classifications;
                  }
                  #endif
               }
            }
         }
         for (i=0; i < classwords && pcount < part_read; ++i, ++pcount) {
            for (j=0; j < ch; ++j) {
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
                     Codebook *book = f->codebooks + b;
                     if (!residue_decode(f, book, target, offset, size, rtype))
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


#if 0
// slow way for debugging
static void inverse_mdct_slow(float *buffer, int n)
{
   int i,j;
   int n2 = n >> 1;
   float *x = (float *) malloc(sizeof(*x) * n2);
   memcpy(x, buffer, sizeof(*x) * n2);
   for (i=0; i < n; ++i) {
      float acc = 0;
      for (j=0; j < n2; ++j)
         // formula from paper:
         //acc += n/4.0f * x[j] * (float) cos(M_PI / 2 / n * (2 * i + 1 + n/2.0)*(2*j+1));
         // formula from wikipedia
         //acc += 2.0f / n2 * x[j] * (float) cos(M_PI/n2 * (i + 0.5 + n2/2)*(j + 0.5));
         // these are equivalent, except the formula from the paper inverts the multiplier!
         // however, what actually works is NO MULTIPLIER!?!
         //acc += 64 * 2.0f / n2 * x[j] * (float) cos(M_PI/n2 * (i + 0.5 + n2/2)*(j + 0.5));
         acc += x[j] * (float) cos(M_PI / 2 / n * (2 * i + 1 + n/2.0)*(2*j+1));
      buffer[i] = acc;
   }
   free(x);
}
#elif 0
// same as above, but just barely able to run in real time on modern machines
static void inverse_mdct_slow(float *buffer, int n, stb_vorbis *f, int blocktype)
{
   float mcos[16384];
   int i,j;
   int n2 = n >> 1, nmask = (n << 2) -1;
   float *x = (float *) malloc(sizeof(*x) * n2);
   memcpy(x, buffer, sizeof(*x) * n2);
   for (i=0; i < 4*n; ++i)
      mcos[i] = (float) cos(M_PI / 2 * i / n);

   for (i=0; i < n; ++i) {
      float acc = 0;
      for (j=0; j < n2; ++j)
         acc += x[j] * mcos[(2 * i + 1 + n2)*(2*j+1) & nmask];
      buffer[i] = acc;
   }
   free(x);
}
#else
// transform to use a slow dct-iv; this is STILL basically trivial,
// but only requires half as many ops
static void dct_iv_slow(float *buffer, int n)
{
   float mcos[16384];
   float x[2048];
   int i,j;
   int nmask = (n << 3) - 1;
   memcpy(x, buffer, sizeof(*x) * n);
   for (i=0; i < 8*n; ++i)
      mcos[i] = (float) cos(M_PI / 4 * i / n);
   for (i=0; i < n; ++i) {
      float acc = 0;
      for (j=0; j < n; ++j)
         acc += x[j] * mcos[((2 * i + 1)*(2*j+1)) & nmask];
         //acc += x[j] * cos(M_PI / n * (i + 0.5) * (j + 0.5));
      buffer[i] = acc;
   }
   free(x);
}

static void inverse_mdct_slow(float *buffer, int n, stb_vorbis *f, int blocktype)
{
   int i, n4 = n >> 2, n2 = n >> 1, n3_4 = n - n4;
   float temp[4096];

   memcpy(temp, buffer, n2 * sizeof(float));
   dct_iv_slow(temp, n2);  // returns -c'-d, a-b'

   for (i=0; i < n4  ; ++i) buffer[i] = temp[i+n4];            // a-b'
   for (   ; i < n3_4; ++i) buffer[i] = -temp[n3_4 - i - 1];   // b-a', c+d'
   for (   ; i < n   ; ++i) buffer[i] = -temp[i - n3_4];       // c'+d
}
#endif


// the following were split out into separate functions while optimizing;
// they could be pushed back up but eh. inline showed no change;
// they're probably already being inlined.
static void imdct_step3_iter0_loop(int n, float *e, int i_off, int k_off, float *A)
{
   float *ee0 = e + i_off;
   float *ee2 = ee0 + k_off;
   int i;

   assert((n & 3) == 0);
   for (i=(n>>2); i > 0; --i) {
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

static void imdct_step3_inner_r_loop(int lim, float *e, int d0, int k_off, float *A, int k1)
{
   int i;
   float k00_20, k01_21;

   float *e0 = e + d0;
   float *e2 = e0 + k_off;

   for (i=lim >> 2; i > 0; --i) {
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
   int i;
   float A0 = A[0];
   float A1 = A[0+1];
   float A2 = A[0+a_off];
   float A3 = A[0+a_off+1];
   float A4 = A[0+a_off*2+0];
   float A5 = A[0+a_off*2+1];
   float A6 = A[0+a_off*3+0];
   float A7 = A[0+a_off*3+1];

   float k00,k11;

   float *ee0 = e  +i_off;
   float *ee2 = ee0+k_off;

   for (i=n; i > 0; --i) {
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
   float k00,k11,k22,k33;
   float y0,y1,y2,y3;

   k00  = z[ 0] - z[-4];
   y0   = z[ 0] + z[-4];
   y2   = z[-2] + z[-6];
   k22  = z[-2] - z[-6];

   z[-0] = y0 + y2;      // z0 + z4 + z2 + z6
   z[-2] = y0 - y2;      // z0 + z4 - z2 - z6

   // done with y0,y2

   k33  = z[-3] - z[-7];

   z[-4] = k00 + k33;    // z0 - z4 + z3 - z7
   z[-6] = k00 - k33;    // z0 - z4 - z3 + z7

   // done with k33

   k11  = z[-1] - z[-5];
   y1   = z[-1] + z[-5];
   y3   = z[-3] + z[-7];

   z[-1] = y1 + y3;      // z1 + z5 + z3 + z7
   z[-3] = y1 - y3;      // z1 + z5 - z3 - z7
   z[-5] = k11 - k22;    // z1 - z5 + z2 - z6
   z[-7] = k11 + k22;    // z1 - z5 - z2 + z6
}

static void imdct_step3_inner_s_loop_ld654(int n, float *e, int i_off, float *A, int base_n)
{
   int a_off = base_n >> 3;
   float A2 = A[0+a_off];
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

static void inverse_mdct(float *buffer, int n, stb_vorbis *f, int blocktype)
{
   int n2 = n >> 1, n4 = n >> 2, n8 = n >> 3, l;
   int ld;
   // @OPTIMIZE: reduce register pressure by using fewer variables?
   float *buf2 = f->imdct_temp_buf;
   float *u=NULL,*v=NULL;
   // twiddle factors
   float *A = f->A[blocktype];

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
      d = &buf2[n2-2];
      AA = A;
      e = &buffer[0];
      e_stop = &buffer[n2];
      while (e != e_stop) {
         d[1] = (e[0] * AA[0] - e[2]*AA[1]);
         d[0] = (e[0] * AA[1] + e[2]*AA[0]);
         d -= 2;
         AA += 2;
         e += 4;
      }

      e = &buffer[n2-3];
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
      float *AA = &A[n2-8];
      float *d0,*d1, *e0, *e1;

      e0 = &v[n4];
      e1 = &v[0];

      d0 = &u[n4];
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
   ld = ilog(n) - 1; // ilog is off-by-one from normal definitions

   // optimized step 3:

   // the original step3 loop can be nested r inside s or s inside r;
   // it's written originally as s inside r, but this is dumb when r
   // iterates many times, and s few. So I have two copies of it and
   // switch between them halfway.

   // this is iteration 0 of step 3
   imdct_step3_iter0_loop(n >> 4, u, n2-1-n4*0, -(n >> 3), A);
   imdct_step3_iter0_loop(n >> 4, u, n2-1-n4*1, -(n >> 3), A);

   // this is iteration 1 of step 3
   imdct_step3_inner_r_loop(n >> 5, u, n2-1 - n8*0, -(n >> 4), A, 16);
   imdct_step3_inner_r_loop(n >> 5, u, n2-1 - n8*1, -(n >> 4), A, 16);
   imdct_step3_inner_r_loop(n >> 5, u, n2-1 - n8*2, -(n >> 4), A, 16);
   imdct_step3_inner_r_loop(n >> 5, u, n2-1 - n8*3, -(n >> 4), A, 16);

   l=2;
   for (; l < (ld-3)>>1; ++l) {
      int k0 = n >> (l+2), k0_2 = k0>>1;
      int lim = 1 << (l+1);
      int i;
      for (i=0; i < lim; ++i)
         imdct_step3_inner_r_loop(n >> (l+4), u, n2-1 - k0*i, -k0_2, A, 1 << (l+3));
   }

   for (; l < ld-6; ++l) {
      int k0 = n >> (l+2), k1 = 1 << (l+3), k0_2 = k0>>1;
      int rlim = n >> (l+6), r;
      int lim = 1 << (l+1);
      int i_off;
      float *A0 = A;
      i_off = n2-1;
      for (r=rlim; r > 0; --r) {
         imdct_step3_inner_s_loop(lim, u, i_off, -k0_2, A0, k1, k0);
         A0 += k1*4;
         i_off -= 8;
      }
   }

   // iterations with count:
   //   ld-6,-5,-4 all interleaved together
   //       the big win comes from getting rid of needless flops
   //         due to the constants on pass 5 & 4 being all 1 and 0;
   //       combining them to be simultaneous to improve cache made little difference
   imdct_step3_inner_s_loop_ld654(n >> 5, u, n2-1, A, n);

   // output is u

   // step 4, 5, and 6
   // cannot be in-place because of step 5
   {
      uint16_t *bitrev = f->bit_reverse[blocktype];
      // weirdly, I'd have thought reading sequentially and writing
      // erratically would have been better than vice-versa, but in
      // fact that's not what my testing showed. (That is, with
      // j = bitreverse(i), do you read i and write j, or read j and write i.)

      float *d0 = &v[n4-4];
      float *d1 = &v[n2-4];
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
   assert(v == buf2);

   // step 7   (paper output is v, now v)
   // this is now in place
   {
      float *C = f->C[blocktype];
      float *d, *e;

      d = v;
      e = v + n2 - 4;

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

      float *B = f->B[blocktype] + n2 - 8;
      float *e = buf2 + n2 - 8;
      d0 = &buffer[0];
      d1 = &buffer[n2-4];
      d2 = &buffer[n2];
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
void inverse_mdct_naive(float *buffer, int n)
{
   float s;
   float A[1 << 12], B[1 << 12], C[1 << 11];
   int i,k,k2,k4, n2 = n >> 1, n4 = n >> 2, n8 = n >> 3, l;
   int n3_4 = n - n4, ld;
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
   ld = ilog(n) - 1; // ilog is off-by-one from normal definitions
   for (l=0; l < ld-3; ++l) {
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
      if (l+1 < ld-3) {
         // paper bug: ping-ponging of u&w here is omitted
         memcpy(w, u, sizeof(u));
      }
   }

   // step 4
   for (i=0; i < n8; ++i) {
      int j = bit_reverse(i) >> (32-ld+3);
      assert(j < n8);
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

static float *get_window(stb_vorbis *f, int len)
{
   len <<= 1;
   if (len == f->blocksize_0) return f->window[0];
   if (len == f->blocksize_1) return f->window[1];
   assert(0);
   return NULL;
}

static int do_floor(stb_vorbis *f, Mapping *map, int i, int n, float *target, int16_t *finalY, uint8_t *step2_flag)
{
   int n2 = n >> 1;
   int s = map->chan[i].mux, floor;
   floor = map->submap_floor[s];
   if (f->floor_types[floor] == 0) {
      return error(f, VORBIS_invalid_stream);
   } else {
      Floor1 *g = &f->floor_config[floor].floor1;
      int j,q;
      int lx = 0, ly = finalY[0] * g->floor1_multiplier;
      for (q=1; q < g->values; ++q) {
         j = g->sorted_order[q];
         if (finalY[j] >= 0)
         {
            int hy = finalY[j] * g->floor1_multiplier;
            int hx = g->Xlist[j];
            draw_line(target, lx,ly, hx,hy, n2);
            lx = hx, ly = hy;
         }
      }
      if (lx < n2)
         // optimization of: draw_line(target, lx,ly, n,ly, n2);
         for (j=lx; j < n2; ++j)
            target[j] *= inverse_db_table[ly];
   }
   return TRUE;
}

static int vorbis_decode_initial(stb_vorbis *f, int *p_left_start, int *p_left_end, int *p_right_start, int *p_right_end, int *mode)
{
   Mode *m;
   int i, n, prev, next, window_center;
   f->channel_buffer_start = f->channel_buffer_end = 0;

  retry:
   if (f->eof) return FALSE;
   if (!maybe_start_packet(f))
      return FALSE;
   // check packet type
   if (get_bits(f,1) != 0) {
      while (EOP != get8_packet(f));
      goto retry;
   }

   i = get_bits(f, ilog(f->mode_count-1));
   if (i == EOP) return FALSE;
   if (i >= f->mode_count) return FALSE;
   *mode = i;
   m = f->mode_config + i;
   if (m->blockflag) {
      n = f->blocksize_1;
      prev = get_bits(f,1);
      next = get_bits(f,1);
   } else {
      prev = next = 0;
      n = f->blocksize_0;
   }

// WINDOWING

   window_center = n >> 1;
   if (m->blockflag && !prev) {
      *p_left_start = (n - f->blocksize_0) >> 2;
      *p_left_end   = (n + f->blocksize_0) >> 2;
   } else {
      *p_left_start = 0;
      *p_left_end   = window_center;
   }
   if (m->blockflag && !next) {
      *p_right_start = (n*3 - f->blocksize_0) >> 2;
      *p_right_end   = (n*3 + f->blocksize_0) >> 2;
   } else {
      *p_right_start = window_center;
      *p_right_end   = n;
   }
   return TRUE;
}

static int vorbis_decode_packet_rest(stb_vorbis *f, int *len, Mode *mode, int left_start, int left_end, int right_start, int right_end, int *p_left)
{
   Mapping *map;
   int i,j,k,n,n2;
   int zero_channel[256];
   int really_zero_channel[256];

// WINDOWING

   n = f->blocksize[mode->blockflag];

   map = &f->mapping[mode->mapping];

// FLOORS
   n2 = n >> 1;

   for (i=0; i < f->channels; ++i) {
      int s = map->chan[i].mux, floor;
      zero_channel[i] = FALSE;
      floor = map->submap_floor[s];
      if (f->floor_types[floor] == 0) {
         return error(f, VORBIS_invalid_stream);
      } else {
         Floor1 *g = &f->floor_config[floor].floor1;
         if (get_bits(f, 1)) {
            short *finalY;
            uint8_t step2_flag[256];
            static int range_list[4] = { 256, 128, 86, 64 };
            int range = range_list[g->floor1_multiplier-1];
            int offset = 2;
            finalY = f->finalY[i];
            finalY[0] = get_bits(f, ilog(range)-1);
            finalY[1] = get_bits(f, ilog(range)-1);
            for (j=0; j < g->partitions; ++j) {
               int pclass = g->partition_class_list[j];
               int cdim = g->class_dimensions[pclass];
               int cbits = g->class_subclasses[pclass];
               int csub = (1 << cbits)-1;
               int cval = 0;
               if (cbits) {
                  Codebook *c = f->codebooks + g->class_masterbooks[pclass];
                  DECODE(cval,f,c);
               }
               for (k=0; k < cdim; ++k) {
                  int book = g->subclass_books[pclass][cval & csub];
                  cval = cval >> cbits;
                  if (book >= 0) {
                     int temp;
                     Codebook *c = f->codebooks + book;
                     DECODE(temp,f,c);
                     finalY[offset++] = temp;
                  } else
                     finalY[offset++] = 0;
               }
            }
            if (f->valid_bits == INVALID_BITS) goto error; // behavior according to spec
            step2_flag[0] = step2_flag[1] = 1;
            for (j=2; j < g->values; ++j) {
               int low, high, pred, highroom, lowroom, room, val;
               low = g->neighbors[j][0];
               high = g->neighbors[j][1];
               //neighbors(g->Xlist, j, &low, &high);
               pred = predict_point(g->Xlist[j], g->Xlist[low], g->Xlist[high], finalY[low], finalY[high]);
               val = finalY[j];
               highroom = range - pred;
               lowroom = pred;
               if (highroom < lowroom)
                  room = highroom * 2;
               else
                  room = lowroom * 2;
               if (val) {
                  step2_flag[low] = step2_flag[high] = 1;
                  step2_flag[j] = 1;
                  if (val >= room)
                     if (highroom > lowroom)
                        finalY[j] = val - lowroom + pred;
                     else
                        finalY[j] = pred - val + highroom - 1;
                  else
                     if (val & 1)
                        finalY[j] = pred - ((val+1)>>1);
                     else
                        finalY[j] = pred + (val>>1);
               } else {
                  step2_flag[j] = 0;
                  finalY[j] = pred;
               }
            }

            // defer final floor computation until _after_ residue
            for (j=0; j < g->values; ++j) {
               if (!step2_flag[j])
                  finalY[j] = -1;
            }
         } else {
           error:
            zero_channel[i] = TRUE;
         }
         // So we just defer everything else to later

         // at this point we've decoded the floor into buffer
      }
   }
   // at this point we've decoded all floors

   // re-enable coupled channels if necessary
   memcpy(really_zero_channel, zero_channel, sizeof(really_zero_channel[0]) * f->channels);
   for (i=0; i < map->coupling_steps; ++i)
      if (!zero_channel[map->chan[i].magnitude] || !zero_channel[map->chan[i].angle]) {
         zero_channel[map->chan[i].magnitude] = zero_channel[map->chan[i].angle] = FALSE;
      }

// RESIDUE DECODE
   for (i=0; i < map->submaps; ++i) {
      // FIXME(libnogg): 256 pointers on stack is a bit much
      float *residue_buffers[256];
      int r;
      uint8_t do_not_decode[256];
      int ch = 0;
      for (j=0; j < f->channels; ++j) {
         if (map->chan[j].mux == i) {
            if (zero_channel[j]) {
               do_not_decode[ch] = TRUE;
               residue_buffers[ch] = NULL;
            } else {
               do_not_decode[ch] = FALSE;
               residue_buffers[ch] = f->channel_buffers[j];
            }
            ++ch;
         }
      }
      r = map->submap_residue[i];
      decode_residue(f, residue_buffers, ch, n2, r, do_not_decode);
   }

// INVERSE COUPLING
   for (i = map->coupling_steps-1; i >= 0; --i) {
      float *m = f->channel_buffers[map->chan[i].magnitude];
      float *a = f->channel_buffers[map->chan[i].angle    ];
      for (j=0; j < n2; ++j) {
         float a2,m2;
         if (m[j] > 0)
            if (a[j] > 0)
               m2 = m[j], a2 = m[j] - a[j];
            else
               a2 = m[j], m2 = m[j] + a[j];
         else
            if (a[j] > 0)
               m2 = m[j], a2 = m[j] + a[j];
            else
               a2 = m[j], m2 = m[j] - a[j];
         m[j] = m2;
         a[j] = a2;
      }
   }

   // finish decoding the floors
   for (i=0; i < f->channels; ++i) {
      if (really_zero_channel[i]) {
         memset(f->channel_buffers[i], 0, sizeof(*f->channel_buffers[i]) * n2);
      } else {
         do_floor(f, map, i, n, f->channel_buffers[i], f->finalY[i], NULL);
      }
   }

// INVERSE MDCT
   for (i=0; i < f->channels; ++i)
      inverse_mdct(f->channel_buffers[i], n, f, mode->blockflag);

   // this shouldn't be necessary, unless we exited on an error
   // and want to flush to get to the next packet
   flush_packet(f);

   if (f->first_decode) {
      // assume we start so first non-discarded sample is sample 0
      // this isn't to spec, but spec would require us to read ahead
      // and decode the size of all current frames--could be done,
      // but presumably it's not a commonly used feature
      f->current_loc = -n2; // start of first frame is positioned for discard
      // we might have to discard samples "from" the next frame too,
      // if we're lapping a large block then a small at the start?
      f->discard_samples_deferred = n - right_end;
      f->current_loc_valid = TRUE;
      f->first_decode = FALSE;
   } else if (f->discard_samples_deferred) {
      left_start += f->discard_samples_deferred;
      *p_left = left_start;
      f->discard_samples_deferred = 0;
   } else if (f->previous_length == 0 && f->current_loc_valid) {
      // we're recovering from a seek... that means we're going to discard
      // the samples from this packet even though we know our position from
      // the last page header, so we need to update the position based on
      // the discarded samples here
      // but wait, the code below is going to add this in itself even
      // on a discard, so we don't need to do it here...
   }

   // check if we have ogg information about the sample # for this packet
   if (f->last_seg_which == f->end_seg_with_known_loc) {
      // if we have a valid current loc, and this is final:
      if (f->current_loc_valid && (f->page_flag & PAGEFLAG_last_page)) {
         uint32_t current_end = f->known_loc_for_packet - (n-right_end);
         // then let's infer the size of the (probably) short final frame
         if (current_end < f->current_loc + right_end) {
            if (current_end < f->current_loc) {
               // negative truncation, that's impossible!
               *len = 0;
            } else {
               *len = current_end - f->current_loc;
            }
            *len += left_start;
            f->current_loc += *len;
            return TRUE;
         }
      }
      // otherwise, just set our sample loc
      // guess that the ogg granule pos refers to the _middle_ of the
      // last frame?
      // set f->current_loc to the position of left_start
      f->current_loc = f->known_loc_for_packet - (n2-left_start);
      f->current_loc_valid = TRUE;
   }
   if (f->current_loc_valid)
      f->current_loc += (right_start - left_start);

   *len = right_end;  // ignore samples after the window goes to 0
   return TRUE;
}

static int vorbis_decode_packet(stb_vorbis *f, int *len, int *p_left, int *p_right)
{
   int mode, left_end, right_end;
   if (!vorbis_decode_initial(f, p_left, &left_end, p_right, &right_end, &mode)) return 0;
   return vorbis_decode_packet_rest(f, len, f->mode_config + mode, *p_left, left_end, *p_right, right_end, p_left);
}

static int vorbis_finish_frame(stb_vorbis *f, int len, int left, int right)
{
   int prev,i,j;
   // we use right&left (the start of the right- and left-window sin()-regions)
   // to determine how much to return, rather than inferring from the rules
   // (same result, clearer code); 'left' indicates where our sin() window
   // starts, therefore where the previous window's right edge starts, and
   // therefore where to start mixing from the previous buffer. 'right'
   // indicates where our sin() ending-window starts, therefore that's where
   // we start saving, and where our returned-data ends.

   // mixin from previous window
   if (f->previous_length) {
      int n = f->previous_length;
      float *w = get_window(f, n);
      for (i=0; i < f->channels; ++i) {
         for (j=0; j < n; ++j)
            f->channel_buffers[i][left+j] =
               f->channel_buffers[i][left+j]*w[    j] +
               f->previous_window[i][     j]*w[n-1-j];
      }
   }

   prev = f->previous_length;

   // last half of this data becomes previous window
   f->previous_length = len - right;

   // @OPTIMIZE: could avoid this copy by double-buffering the
   // output (flipping previous_window with channel_buffers), but
   // then previous_window would have to be 2x as large, and
   // channel_buffers couldn't be temp mem (although they're NOT
   // currently temp mem, they could be (unless we want to level
   // performance by spreading out the computation))
   for (i=0; i < f->channels; ++i)
      for (j=0; right+j < len; ++j)
         f->previous_window[i][j] = f->channel_buffers[i][right+j];

   if (!prev)
      // there was no previous packet, so this data isn't valid...
      // this isn't entirely true, only the would-have-overlapped data
      // isn't valid, but this seems to be what the spec requires
      return 0;

   // truncate a short frame
   if (len < right) right = len;

   f->samples_output += right-left;

   return right - left;
}

static void vorbis_pump_first_frame(stb_vorbis *f)
{
   int len, right, left;
   if (vorbis_decode_packet(f, &len, &left, &right))
      vorbis_finish_frame(f, len, left, right);
}

static int start_decoder(stb_vorbis *f)
{
   uint8_t header[6], x,y;
   int seg_len,i,j,k, max_submaps = 0;
   int longest_floorlist=0;

   // first page, first packet

   if (!start_page(f))                              return FALSE;
   // validate page flag
   if (!(f->page_flag & PAGEFLAG_first_page))       return error(f, VORBIS_invalid_first_page);
   if (f->page_flag & PAGEFLAG_last_page)           return error(f, VORBIS_invalid_first_page);
   if (f->page_flag & PAGEFLAG_continued_packet)    return error(f, VORBIS_invalid_first_page);
   // check for expected packet length
   if (f->segment_count != 1)                       return error(f, VORBIS_invalid_first_page);
   if (f->segments[0] != 30)                        return error(f, VORBIS_invalid_first_page);
   // read packet
   // check packet header
   if (get8(f) != VORBIS_packet_id)                 return error(f, VORBIS_invalid_first_page);
   if (!getn(f, header, 6))                         return error(f, VORBIS_unexpected_eof);
   if (!vorbis_validate(header))                    return error(f, VORBIS_invalid_first_page);
   // vorbis_version
   if (get32(f) != 0)                               return error(f, VORBIS_invalid_first_page);
   f->channels = get8(f); if (!f->channels)         return error(f, VORBIS_invalid_first_page);
   f->sample_rate = get32(f); if (!f->sample_rate)  return error(f, VORBIS_invalid_first_page);
   get32(f); // bitrate_maximum
   get32(f); // bitrate_nominal
   get32(f); // bitrate_minimum
   x = get8(f);
   { int log0,log1;
   log0 = x & 15;
   log1 = x >> 4;
   f->blocksize_0 = 1 << log0;
   f->blocksize_1 = 1 << log1;
   if (log0 < 6 || log0 > 13)                       return error(f, VORBIS_invalid_setup);
   if (log1 < 6 || log1 > 13)                       return error(f, VORBIS_invalid_setup);
   if (log0 > log1)                                 return error(f, VORBIS_invalid_setup);
   }

   // framing_flag
   x = get8(f);
   if (!(x & 1))                                    return error(f, VORBIS_invalid_first_page);

   f->imdct_temp_buf = (float *) malloc((f->blocksize_1 / 2) * sizeof(*f->imdct_temp_buf));
   if (f->imdct_temp_buf == NULL)                   return error(f, VORBIS_outofmem);

   // second packet!
   if (!start_page(f))                              return FALSE;

   if (!start_packet(f))                            return FALSE;
   do {
      seg_len = next_segment(f);
      skip(f, seg_len);
      f->bytes_in_seg = 0;
   } while (seg_len);

   // third packet!
   if (!start_packet(f))                            return FALSE;

   crc32_init(); // always init it, to avoid multithread race conditions

   if (get8_packet(f) != VORBIS_packet_setup)       return error(f, VORBIS_invalid_setup);
   for (i=0; i < 6; ++i) header[i] = get8_packet(f);
   if (!vorbis_validate(header))                    return error(f, VORBIS_invalid_setup);

   // codebooks

   f->codebook_count = get_bits(f,8) + 1;
   f->codebooks = (Codebook *) malloc(sizeof(*f->codebooks) * f->codebook_count);
   if (f->codebooks == NULL)                        return error(f, VORBIS_outofmem);
   memset(f->codebooks, 0, sizeof(*f->codebooks) * f->codebook_count);
   for (i=0; i < f->codebook_count; ++i) {
      uint32_t *values;
      int ordered, sorted_count;
      int total=0;
      uint8_t *lengths;
      Codebook *c = f->codebooks+i;
      x = get_bits(f, 8); if (x != 0x42)            return error(f, VORBIS_invalid_setup);
      x = get_bits(f, 8); if (x != 0x43)            return error(f, VORBIS_invalid_setup);
      x = get_bits(f, 8); if (x != 0x56)            return error(f, VORBIS_invalid_setup);
      x = get_bits(f, 8);
      c->dimensions = (get_bits(f, 8)<<8) + x;
      x = get_bits(f, 8);
      y = get_bits(f, 8);
      c->entries = (get_bits(f, 8)<<16) + (y<<8) + x;
      ordered = get_bits(f,1);
      c->sparse = ordered ? 0 : get_bits(f,1);

      if (c->sparse)
         lengths = (uint8_t *) malloc(c->entries);
      else
         lengths = c->codeword_lengths = (uint8_t *) malloc(c->entries);

      if (!lengths) return error(f, VORBIS_outofmem);

      if (ordered) {
         int current_entry = 0;
         int current_length = get_bits(f,5) + 1;
         while (current_entry < c->entries) {
            int limit = c->entries - current_entry;
            int n = get_bits(f, ilog(limit));
            if (current_entry + n > (int) c->entries) { return error(f, VORBIS_invalid_setup); }
            memset(lengths + current_entry, current_length, n);
            current_entry += n;
            ++current_length;
         }
      } else {
         for (j=0; j < c->entries; ++j) {
            int present = c->sparse ? get_bits(f,1) : 1;
            if (present) {
               lengths[j] = get_bits(f, 5) + 1;
               ++total;
            } else {
               lengths[j] = NO_CODE;
            }
         }
      }

      if (c->sparse && total >= c->entries >> 2) {
         // convert sparse items to non-sparse!
         c->codeword_lengths = (uint8_t *) malloc(c->entries);
         memcpy(c->codeword_lengths, lengths, c->entries);
         free(lengths);
         lengths = c->codeword_lengths;
         c->sparse = 0;
      }

      // compute the size of the sorted tables
      if (c->sparse) {
         sorted_count = total;
         //assert(total != 0);
      } else {
         sorted_count = 0;
         #ifndef STB_VORBIS_NO_HUFFMAN_BINARY_SEARCH
         for (j=0; j < c->entries; ++j)
            if (lengths[j] > STB_VORBIS_FAST_HUFFMAN_LENGTH && lengths[j] != NO_CODE)
               ++sorted_count;
         #endif
      }

      c->sorted_entries = sorted_count;
      values = NULL;

      if (!c->sparse) {
         c->codewords = (uint32_t *) malloc(sizeof(c->codewords[0]) * c->entries);
         if (!c->codewords)                  return error(f, VORBIS_outofmem);
      } else {
         if (c->sorted_entries) {
            c->codeword_lengths = (uint8_t *) malloc(c->sorted_entries);
            if (!c->codeword_lengths)           return error(f, VORBIS_outofmem);
            c->codewords = (uint32_t *) malloc(sizeof(*c->codewords) * c->sorted_entries);
            if (!c->codewords)                  return error(f, VORBIS_outofmem);
            values = (uint32_t *) malloc(sizeof(*values) * c->sorted_entries);
            if (!values)                        return error(f, VORBIS_outofmem);
         }
      }

      if (!compute_codewords(c, lengths, c->entries, values)) {
         if (c->sparse) free(values);
         return error(f, VORBIS_invalid_setup);
      }

      if (c->sorted_entries) {
         // allocate an extra slot for sentinels
         c->sorted_codewords = (uint32_t *) malloc(sizeof(*c->sorted_codewords) * (c->sorted_entries+1));
         // allocate an extra slot at the front so that c->sorted_values[-1] is defined
         // so that we can catch that case without an extra if
         c->sorted_values    = ( int   *) malloc(sizeof(*c->sorted_values   ) * (c->sorted_entries+1));
         if (c->sorted_values) { ++c->sorted_values; c->sorted_values[-1] = -1; }
         compute_sorted_huffman(c, lengths, values);
      }

      if (c->sparse) {
         free(values);
         free(c->codewords);
         free(lengths);
         c->codewords = NULL;
      }

      compute_accelerated_huffman(c);

      c->lookup_type = get_bits(f, 4);
      if (c->lookup_type > 2) return error(f, VORBIS_invalid_setup);
      if (c->lookup_type > 0) {
         uint16_t *mults;
         c->minimum_value = float32_unpack(get_bits(f, 32));
         c->delta_value = float32_unpack(get_bits(f, 32));
         c->value_bits = get_bits(f, 4)+1;
         c->sequence_p = get_bits(f,1);
         if (c->lookup_type == 1) {
            c->lookup_values = lookup1_values(c->entries, c->dimensions);
         } else {
            c->lookup_values = c->entries * c->dimensions;
         }
         mults = (uint16_t *) malloc(sizeof(mults[0]) * c->lookup_values);
         if (mults == NULL) return error(f, VORBIS_outofmem);
         for (j=0; j < (int) c->lookup_values; ++j) {
            int q = get_bits(f, c->value_bits);
            if (q == EOP) { free(mults); return error(f, VORBIS_invalid_setup); }
            mults[j] = q;
         }

#ifndef STB_VORBIS_DIVIDES_IN_CODEBOOK
         if (c->lookup_type == 1) {
            int len, sparse = c->sparse;
            // pre-expand the lookup1-style multiplicands, to avoid a divide in the inner loop
            if (sparse) {
               if (c->sorted_entries == 0) goto skip;
               c->multiplicands = (codetype *) malloc(sizeof(c->multiplicands[0]) * c->sorted_entries * c->dimensions);
            } else
               c->multiplicands = (codetype *) malloc(sizeof(c->multiplicands[0]) * c->entries        * c->dimensions);
            if (c->multiplicands == NULL) { free(mults); return error(f, VORBIS_outofmem); }
            len = sparse ? c->sorted_entries : c->entries;
            for (j=0; j < len; ++j) {
               int z = sparse ? c->sorted_values[j] : j, div=1;
               for (k=0; k < c->dimensions; ++k) {
                  int off = (z / div) % c->lookup_values;
                  c->multiplicands[j*c->dimensions + k] =
                         #ifndef STB_VORBIS_CODEBOOK_FLOATS
                            mults[off];
                         #else
                            mults[off]*c->delta_value + c->minimum_value;
                            // in this case (and this case only) we could pre-expand c->sequence_p,
                            // and throw away the decode logic for it; have to ALSO do
                            // it in the case below, but it can only be done if
                            //    STB_VORBIS_CODEBOOK_FLOATS
                            //   !STB_VORBIS_DIVIDES_IN_CODEBOOK
                         #endif
                  div *= c->lookup_values;
               }
            }
            free(mults);
            c->lookup_type = 2;
         }
         else
#endif
         {
            c->multiplicands = (codetype *) malloc(sizeof(c->multiplicands[0]) * c->lookup_values);
            #ifndef STB_VORBIS_CODEBOOK_FLOATS
            memcpy(c->multiplicands, mults, sizeof(c->multiplicands[0]) * c->lookup_values);
            #else
            for (j=0; j < (int) c->lookup_values; ++j)
               c->multiplicands[j] = mults[j] * c->delta_value + c->minimum_value;
            free(mults);
            #endif
         }
        skip:;

         #ifdef STB_VORBIS_CODEBOOK_FLOATS
         if (c->lookup_type == 2 && c->sequence_p) {
            for (j=1; j < (int) c->lookup_values; ++j)
               c->multiplicands[j] = c->multiplicands[j-1];
            c->sequence_p = 0;
         }
         #endif
      }
   }

   // time domain transfers (notused)

   x = get_bits(f, 6) + 1;
   for (i=0; i < x; ++i) {
      uint32_t z = get_bits(f, 16);
      if (z != 0) return error(f, VORBIS_invalid_setup);
   }

   // Floors
   f->floor_count = get_bits(f, 6)+1;
   f->floor_config = (Floor *)  malloc(f->floor_count * sizeof(*f->floor_config));
   for (i=0; i < f->floor_count; ++i) {
      f->floor_types[i] = get_bits(f, 16);
      if (f->floor_types[i] > 1) return error(f, VORBIS_invalid_setup);
      if (f->floor_types[i] == 0) {
         Floor0 *g = &f->floor_config[i].floor0;
         g->order = get_bits(f,8);
         g->rate = get_bits(f,16);
         g->bark_map_size = get_bits(f,16);
         g->amplitude_bits = get_bits(f,6);
         g->amplitude_offset = get_bits(f,8);
         g->number_of_books = get_bits(f,4) + 1;
         for (j=0; j < g->number_of_books; ++j)
            g->book_list[j] = get_bits(f,8);
         return error(f, VORBIS_feature_not_supported);
      } else {
         Point p[31*8+2];
         Floor1 *g = &f->floor_config[i].floor1;
         int max_class = -1; 
         g->partitions = get_bits(f, 5);
         for (j=0; j < g->partitions; ++j) {
            g->partition_class_list[j] = get_bits(f, 4);
            if (g->partition_class_list[j] > max_class)
               max_class = g->partition_class_list[j];
         }
         for (j=0; j <= max_class; ++j) {
            g->class_dimensions[j] = get_bits(f, 3)+1;
            g->class_subclasses[j] = get_bits(f, 2);
            if (g->class_subclasses[j]) {
               g->class_masterbooks[j] = get_bits(f, 8);
               if (g->class_masterbooks[j] >= f->codebook_count) return error(f, VORBIS_invalid_setup);
            }
            for (k=0; k < 1 << g->class_subclasses[j]; ++k) {
               g->subclass_books[j][k] = get_bits(f,8)-1;
               if (g->subclass_books[j][k] >= f->codebook_count) return error(f, VORBIS_invalid_setup);
            }
         }
         g->floor1_multiplier = get_bits(f,2)+1;
         g->rangebits = get_bits(f,4);
         g->Xlist[0] = 0;
         g->Xlist[1] = 1 << g->rangebits;
         g->values = 2;
         for (j=0; j < g->partitions; ++j) {
            int c = g->partition_class_list[j];
            for (k=0; k < g->class_dimensions[c]; ++k) {
               g->Xlist[g->values] = get_bits(f, g->rangebits);
               ++g->values;
            }
         }
         // precompute the sorting
         for (j=0; j < g->values; ++j) {
            p[j].x = g->Xlist[j];
            p[j].y = j;
         }
         qsort(p, g->values, sizeof(p[0]), point_compare);
         for (j=0; j < g->values; ++j)
            g->sorted_order[j] = (uint8_t) p[j].y;
         // precompute the neighbors
         for (j=2; j < g->values; ++j) {
            int low,hi;
            neighbors(g->Xlist, j, &low,&hi);
            g->neighbors[j][0] = low;
            g->neighbors[j][1] = hi;
         }

         if (g->values > longest_floorlist)
            longest_floorlist = g->values;
      }
   }

   // Residue
   int residue_max_alloc = 0;
   f->residue_count = get_bits(f, 6)+1;
   f->residue_config = (Residue *) malloc(f->residue_count * sizeof(*f->residue_config));
   for (i=0; i < f->residue_count; ++i) {
      uint8_t residue_cascade[64];
      Residue *r = f->residue_config+i;
      f->residue_types[i] = get_bits(f, 16);
      if (f->residue_types[i] > 2) return error(f, VORBIS_invalid_setup);
      r->begin = get_bits(f, 24);
      r->end = get_bits(f, 24);
      r->part_size = get_bits(f,24)+1;
      r->classifications = get_bits(f,6)+1;
      r->classbook = get_bits(f,8);
      for (j=0; j < r->classifications; ++j) {
         uint8_t high_bits=0;
         uint8_t low_bits=get_bits(f,3);
         if (get_bits(f,1))
            high_bits = get_bits(f,5);
         residue_cascade[j] = high_bits*8 + low_bits;
      }
      r->residue_books = (short (*)[8]) malloc(sizeof(r->residue_books[0]) * r->classifications);
      for (j=0; j < r->classifications; ++j) {
         for (k=0; k < 8; ++k) {
            if (residue_cascade[j] & (1 << k)) {
               r->residue_books[j][k] = get_bits(f, 8);
               if (r->residue_books[j][k] >= f->codebook_count) return error(f, VORBIS_invalid_setup);
            } else {
               r->residue_books[j][k] = -1;
            }
         }
      }
      // precompute the classifications[] array to avoid inner-loop mod/divide
      // call it 'classdata' since we already have r->classifications
      r->classdata = (uint8_t **) malloc(sizeof(*r->classdata) * f->codebooks[r->classbook].entries);
      if (!r->classdata) return error(f, VORBIS_outofmem);
      memset(r->classdata, 0, sizeof(*r->classdata) * f->codebooks[r->classbook].entries);
      for (j=0; j < f->codebooks[r->classbook].entries; ++j) {
         int classwords = f->codebooks[r->classbook].dimensions;
         int temp = j;
         r->classdata[j] = (uint8_t *) malloc(sizeof(r->classdata[j][0]) * classwords);
         for (k=classwords-1; k >= 0; --k) {
            r->classdata[j][k] = temp % r->classifications;
            temp /= r->classifications;
         }
      }
      // calculate how many temporary array entries we need
      int alloc = (r->end - r->begin) / r->part_size;
      if (alloc > residue_max_alloc) {
         residue_max_alloc = alloc;
      }
   }
#ifdef STB_VORBIS_DIVIDES_IN_RESIDUE
   f->classifications = (int **) malloc_channel_array(f->channels, residue_max_alloc * sizeof(**f->classifications));
   if (!f->classifications) return FALSE;
#else
   f->part_classdata = (uint8_t ***) malloc_channel_array(f->channels, residue_max_alloc * sizeof(**f->part_classdata));
   if (!f->part_classdata) return FALSE;
#endif

   f->mapping_count = get_bits(f,6)+1;
   f->mapping = (Mapping *) malloc(f->mapping_count * sizeof(*f->mapping));
   for (i=0; i < f->mapping_count; ++i) {
      Mapping *m = f->mapping + i;      
      int mapping_type = get_bits(f,16);
      if (mapping_type != 0) return error(f, VORBIS_invalid_setup);
      m->chan = (MappingChannel *) malloc(f->channels * sizeof(*m->chan));
      if (get_bits(f,1))
         m->submaps = get_bits(f,4);
      else
         m->submaps = 1;
      if (m->submaps > max_submaps)
         max_submaps = m->submaps;
      if (get_bits(f,1)) {
         m->coupling_steps = get_bits(f,8)+1;
         for (k=0; k < m->coupling_steps; ++k) {
            m->chan[k].magnitude = get_bits(f, ilog(f->channels)-1);
            m->chan[k].angle = get_bits(f, ilog(f->channels)-1);
            if (m->chan[k].magnitude >= f->channels)        return error(f, VORBIS_invalid_setup);
            if (m->chan[k].angle     >= f->channels)        return error(f, VORBIS_invalid_setup);
            if (m->chan[k].magnitude == m->chan[k].angle)   return error(f, VORBIS_invalid_setup);
         }
      } else
         m->coupling_steps = 0;

      // reserved field
      if (get_bits(f,2)) return error(f, VORBIS_invalid_setup);
      if (m->submaps > 1) {
         for (j=0; j < f->channels; ++j) {
            m->chan[j].mux = get_bits(f, 4);
            if (m->chan[j].mux >= m->submaps)                return error(f, VORBIS_invalid_setup);
         }
      } else
         // @SPECIFICATION: this case is missing from the spec
         for (j=0; j < f->channels; ++j)
            m->chan[j].mux = 0;

      for (j=0; j < m->submaps; ++j) {
         get_bits(f,8); // discard
         m->submap_floor[j] = get_bits(f,8);
         m->submap_residue[j] = get_bits(f,8);
         if (m->submap_floor[j] >= f->floor_count)      return error(f, VORBIS_invalid_setup);
         if (m->submap_residue[j] >= f->residue_count)  return error(f, VORBIS_invalid_setup);
      }
   }

   // Modes
   f->mode_count = get_bits(f, 6)+1;
   for (i=0; i < f->mode_count; ++i) {
      Mode *m = f->mode_config+i;
      m->blockflag = get_bits(f,1);
      m->windowtype = get_bits(f,16);
      m->transformtype = get_bits(f,16);
      m->mapping = get_bits(f,8);
      if (m->windowtype != 0)                 return error(f, VORBIS_invalid_setup);
      if (m->transformtype != 0)              return error(f, VORBIS_invalid_setup);
      if (m->mapping >= f->mapping_count)     return error(f, VORBIS_invalid_setup);
   }

   flush_packet(f);

   f->previous_length = 0;

   f->channel_buffers = (float **) malloc_channel_array(f->channels, sizeof(float) * f->blocksize_1);
   f->outputs         = (float **) malloc(f->channels * sizeof(float *));
   f->previous_window = (float **) malloc_channel_array(f->channels, sizeof(float) * f->blocksize_1/2);
   f->finalY          = (int16_t **) malloc_channel_array(f->channels, sizeof(int16_t) * longest_floorlist);
   if (!f->channel_buffers) return FALSE;
   if (!f->outputs)         return FALSE;
   if (!f->previous_window) return FALSE;
   if (!f->finalY)          return FALSE;

   if (!init_blocksize(f, 0, f->blocksize_0)) return FALSE;
   if (!init_blocksize(f, 1, f->blocksize_1)) return FALSE;
   f->blocksize[0] = f->blocksize_0;
   f->blocksize[1] = f->blocksize_1;

   f->first_decode = TRUE;

   f->first_audio_page_offset = get_file_offset(f);

   return TRUE;
}

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
   d.max_frame_size = f->blocksize_1 >> 1;
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

//
// DATA-PULLING API
//

static uint32_t vorbis_find_page(stb_vorbis *f, int64_t *end, uint32_t *last)
{
   if (f->stream_len < 0) return error(f, VORBIS_cant_find_last_page);
   for(;;) {
      int n;
      if (f->eof) return 0;
      n = get8(f);
      if (n == 0x4f) { // page header
         int64_t retry_loc = get_file_offset(f);
         uint32_t i;
         // check if we're off the end of a file_section stream
         if (retry_loc - 25 > f->stream_len)
            return 0;
         // check the rest of the header
         for (i=1; i < 4; ++i)
            if (get8(f) != ogg_page_header[i])
               break;
         if (f->eof) return 0;
         if (i == 4) {
            uint8_t header[27];
            uint32_t crc, goal, len;
            for (i=0; i < 4; ++i)
               header[i] = ogg_page_header[i];
            for (; i < 27; ++i)
               header[i] = get8(f);
            if (f->eof) return 0;
            if (header[4] != 0) goto invalid;
            goal = header[22] + (header[23] << 8) + (header[24]<<16) + (header[25]<<24);
            for (i=22; i < 26; ++i)
               header[i] = 0;
            crc = 0;
            for (i=0; i < 27; ++i)
               crc = crc32_update(crc, header[i]);
            len = 0;
            for (i=0; i < header[26]; ++i) {
               int s = get8(f);
               crc = crc32_update(crc, s);
               len += s;
            }
            if (len && f->eof) return 0;
            for (i=0; i < len; ++i)
               crc = crc32_update(crc, get8(f));
            // finished parsing probable page
            if (crc == goal) {
               // we could now check that it's either got the last
               // page flag set, OR it's followed by the capture
               // pattern, but I guess TECHNICALLY you could have
               // a file with garbage between each ogg page and recover
               // from it automatically? So even though that paranoia
               // might decrease the chance of an invalid decode by
               // another 2^32, not worth it since it would hose those
               // invalid-but-useful files?
               if (end)
                  *end = get_file_offset(f);
               if (last) {
                  if (header[5] & 0x04)
                     *last = 1;
                  else
                     *last = 0;
               }
               set_file_offset(f, retry_loc-1);
               return 1;
            }
         }
        invalid:
         // not a valid page, so rewind and look for next one
         set_file_offset(f, retry_loc);
      }
   }
}

// seek is implemented with 'interpolation search'--this is like
// binary search, but we use the data values to estimate the likely
// location of the data item (plus a bit of a bias so when the
// estimation is wrong we don't waste overly much time)

#define SAMPLE_unknown  0xffffffff


// ogg vorbis, in its insane infinite wisdom, only provides
// information about the sample at the END of the page.
// therefore we COULD have the data we need in the current
// page, and not know it. we could just use the end location
// as our only knowledge for bounds, seek back, and eventually
// the binary search finds it. or we can try to be smart and
// not waste time trying to locate more pages. we try to be
// smart, since this data is already in memory anyway, so
// doing needless I/O would be crazy!
static int vorbis_analyze_page(stb_vorbis *f, ProbedPage *z)
{
   uint8_t header[27], lacing[255];
   uint8_t packet_type[255];
   int num_packet, packet_start;
   int i,len;
   uint32_t samples;

   // record where the page starts
   z->page_start = get_file_offset(f);

   // parse the header
   getn(f, header, 27);
   assert(header[0] == 'O' && header[1] == 'g' && header[2] == 'g' && header[3] == 'S');
   getn(f, lacing, header[26]);

   // determine the length of the payload
   len = 0;
   for (i=0; i < header[26]; ++i)
      len += lacing[i];

   // this implies where the page ends
   z->page_end = z->page_start + 27 + header[26] + len;

   // read the last-decoded sample out of the data
   z->last_decoded_sample = header[6] + (header[7] << 8) + (header[8] << 16) + (header[9] << 16);

   if (header[5] & 4) {
      // if this is the last page, it's not possible to work
      // backwards to figure out the first sample! whoops! fuck.
      z->first_decoded_sample = SAMPLE_unknown;
      set_file_offset(f, z->page_start);
      return 1;
   }

   // scan through the frames to determine the sample-count of each one...
   // our goal is the sample # of the first fully-decoded sample on the
   // page, which is the first decoded sample of the 2nd page

   num_packet=0;

   packet_start = ((header[5] & 1) == 0);

   for (i=0; i < header[26]; ++i) {
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

   for (i=num_packet-2; i >= 1; --i) {
      // now, for this packet, how many samples do we have that
      // do not overlap the following packet?
      if (packet_type[i] == 1)
         if (packet_type[i+1] == 1)
            samples += f->blocksize_1 >> 1;
         else
            samples += ((f->blocksize_1 - f->blocksize_0) >> 2) + (f->blocksize_0 >> 1);
      else
         samples += f->blocksize_0 >> 1;
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

static int vorbis_seek_frame_from_page(stb_vorbis *f, uint64_t page_start, uint32_t first_sample, uint32_t target_sample)
{
   int left_start, left_end, right_start, right_end, mode;
   int frame=0;
   uint32_t frame_start;
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

      if (frame == 0)
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
      assert(frames_to_skip >= 0);
      data_to_skip = -1;      
   }

   set_file_offset(f, page_start);
   f->next_seg = - 1; // force page resync

   {
      int i;
      for (i=0; i < frames_to_skip; ++i) {
         maybe_start_packet(f);
         flush_packet(f);
      }
   }

   if (data_to_skip >= 0) {
      int i,j,n = f->blocksize_0 >> 1;
      f->discard_samples_deferred = data_to_skip;
      for (i=0; i < f->channels; ++i)
         for (j=0; j < n; ++j)
            f->previous_window[i][j] = 0;
      f->previous_length = n;
      frame_start += data_to_skip;
   } else {
      f->previous_length = 0;
      vorbis_pump_first_frame(f);
   }

   // at this point, the NEXT decoded frame will generate the desired sample
   return target_sample - frame_start;
}

int stb_vorbis_seek(stb_vorbis *f, unsigned int sample_number)
{
   ProbedPage p[2],q;
   if (f->stream_len < 0) return error(f, VORBIS_cant_find_last_page);

   // do we know the location of the last page?
   if (f->p_last.page_start == 0) {
      uint32_t z = stb_vorbis_stream_length_in_samples(f);
      if (z == 0) return error(f, VORBIS_cant_find_last_page);
   }

   p[0] = f->p_first;
   p[1] = f->p_last;

   //FIXME(libnogg): allow seeking to 1 past the last sample (true EOF)
   if (sample_number >= f->p_last.last_decoded_sample)
      sample_number = f->p_last.last_decoded_sample-1;

   if (sample_number < f->p_first.last_decoded_sample) {
      return vorbis_seek_frame_from_page(f, p[0].page_start, 0, sample_number);
   } else {
      int attempts=0;
      while (p[0].page_end < p[1].page_start) {
         uint64_t probe;
         uint64_t start_offset, end_offset;
         uint32_t start_sample, end_sample;

         // copy these into local variables so we can tweak them
         // if any are unknown
         start_offset = p[0].page_end;
         end_offset   = p[1].after_previous_page_start; // an address known to seek to page p[1]
         start_sample = p[0].last_decoded_sample;
         end_sample   = p[1].last_decoded_sample;

         // currently there is no such tweaking logic needed/possible?
         if (start_sample == SAMPLE_unknown || end_sample == SAMPLE_unknown)
            return error(f, VORBIS_seek_failed);

         // now we want to lerp between these for the target samples...
      
         // step 1: we need to bias towards the page start...
         if (start_offset + 4000 < end_offset)
            end_offset -= 4000;

         // now compute an interpolated search loc
         probe = start_offset + (int) floor((float) (end_offset - start_offset) / (end_sample - start_sample) * (sample_number - start_sample));

         // next we need to bias towards binary search...
         // code is a little wonky to allow for full 32-bit unsigned values
         if (attempts >= 4) {
            uint32_t probe2 = start_offset + ((end_offset - start_offset) >> 1);
            if (attempts >= 8)
               probe = probe2;
            else if (probe < probe2)
               probe = probe + ((probe2 - probe) >> 1);
            else
               probe = probe2 + ((probe - probe2) >> 1);
         }
         ++attempts;

         set_file_offset(f, probe);
         if (!vorbis_find_page(f, NULL, NULL))   return error(f, VORBIS_seek_failed);
         if (!vorbis_analyze_page(f, &q))        return error(f, VORBIS_seek_failed);
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
         return vorbis_seek_frame_from_page(f, p[1].page_start, p[0].last_decoded_sample, sample_number);
      }
      return error(f, VORBIS_seek_failed);
   }
}

unsigned int stb_vorbis_stream_length_in_samples(stb_vorbis *f)
{
   int64_t restore_offset, previous_safe;
   int64_t end, last_page_loc;

   if (!f->total_samples) {
      int last;
      uint32_t lo,hi;
      char header[6];

      // first, store the current decode position so we can restore it
      restore_offset = get_file_offset(f);

      // now we want to seek back 64K from the end (the last page must
      // be at most a little less than 64K, but let's allow a little slop)
      if (f->stream_len >= 65536 && f->stream_len-65536 >= f->first_audio_page_offset)
         previous_safe = f->stream_len - 65536;
      else
         previous_safe = f->first_audio_page_offset;

      set_file_offset(f, previous_safe);
      // previous_safe is now our candidate 'earliest known place that seeking
      // to will lead to the final page'

      if (!vorbis_find_page(f, &end, (int unsigned *)&last)) {
         // if we can't find a page, we're hosed!
         f->error = VORBIS_cant_find_last_page;
         f->total_samples = 0xffffffff;
         goto done;
      }

      // check if there are more pages
      last_page_loc = get_file_offset(f);

      // stop when the last_page flag is set, not when we reach eof;
      // this allows us to stop short of a 'file_section' end without
      // explicitly checking the length of the section
      while (!last) {
         set_file_offset(f, end);
         if (!vorbis_find_page(f, &end, (int unsigned *)&last)) {
            // the last page we found didn't have the 'last page' flag
            // set. whoops!
            break;
         }
         previous_safe = last_page_loc+1;
         last_page_loc = get_file_offset(f);
      }

      set_file_offset(f, last_page_loc);

      // parse the header
      getn(f, (unsigned char *)header, 6);
      // extract the absolute granule position
      lo = get32(f);
      hi = get32(f);
      if (lo == 0xffffffff && hi == 0xffffffff) {
         f->error = VORBIS_cant_find_last_page;
         f->total_samples = SAMPLE_unknown;
         goto done;
      }
      if (hi)
         lo = 0xfffffffe; // saturate
      f->total_samples = lo;

      f->p_last.page_start = last_page_loc;
      f->p_last.page_end   = end;
      f->p_last.last_decoded_sample = lo;
      f->p_last.first_decoded_sample = SAMPLE_unknown;
      f->p_last.after_previous_page_start = previous_safe;

     done:
      set_file_offset(f, restore_offset);
   }
   return f->total_samples == SAMPLE_unknown ? 0 : f->total_samples;
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
    stb_vorbis *handle = calloc(1, sizeof(*handle));
    if (!handle) {
        if (error_ret) {
            *error_ret = VORBIS_outofmem;
        }
        return NULL;
    }
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
