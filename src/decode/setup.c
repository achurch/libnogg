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
#include "src/decode/inlines.h"
#include "src/decode/io.h"
#include "src/decode/packet.h"
#include "src/decode/setup.h"
#include "src/util/memory.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

//FIXME: not reviewed

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

#define M_PIf  3.14159265f

enum
{
   VORBIS_packet_id = 1,
   VORBIS_packet_comment = 3,
   VORBIS_packet_setup = 5,
};

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

static float float32_unpack(uint32_t x)
{
   // from the specification
   uint32_t mantissa = x & 0x1fffff;
   uint32_t sign = x & 0x80000000;
   uint32_t exp = (x & 0x7fe00000) >> 21;
   float res = sign ? -(float)mantissa : (float)mantissa;
   return ldexpf(res, exp-788);
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

static bool compute_codewords(Codebook *c, uint8_t *len, int n, uint32_t *values)
{
   int k,m=0;
   uint32_t available[32];

   memset(available, 0, sizeof(available));
   // find the first entry
   for (k=0; k < n; ++k) if (len[k] < NO_CODE) break;
   if (k == n) { assert(c->sorted_entries == 0); return true; }
   // add to the list
   add_entry(c, 0, k, m++, len[k], values);
   // add all available leaves
   for (int i=1; i <= len[k]; ++i)
      available[i] = 1 << (32-i);
   // note that the above code treats the first case specially,
   // but it's really the same as the following code, so they
   // could probably be combined (except the initial code is 0,
   // and I use 0 in available[] to mean 'empty')
   for (int i=k+1; i < n; ++i) {
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
      if (z == 0) { assert(0); return false; }
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
   return true;
}

// accelerated huffman table allows fast O(1) match of all symbols
// of length <= STB_VORBIS_FAST_HUFFMAN_LENGTH
static void compute_accelerated_huffman(Codebook *c)
{
   for (int i=0; i < FAST_HUFFMAN_TABLE_SIZE; ++i)
      c->fast_huffman[i] = -1;

   int len = c->sparse ? c->sorted_entries : c->entries;
   #ifdef STB_VORBIS_FAST_HUFFMAN_SHORT
   if (len > 32767) len = 32767; // largest possible value we can encode!
   #endif
   for (int i=0; i < len; ++i) {
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

static bool include_in_sort(Codebook *c, uint8_t len)
{
   if (c->sparse) { assert(len != NO_CODE); return true; }
   if (len == NO_CODE) return false;
   if (len > STB_VORBIS_FAST_HUFFMAN_LENGTH) return true;
   return false;
}

// if the fast table above doesn't work, we want to binary
// search them... need to reverse the bits
static void compute_sorted_huffman(Codebook *c, uint8_t *lengths, uint32_t *values)
{
   // build a list of all the entries
   // OPTIMIZATION: don't include the short ones, since they'll be caught by FAST_HUFFMAN.
   // this is kind of a frivolous optimization--I don't see any performance improvement,
   // but it's like 4 extra lines of code, so.
   if (!c->sparse) {
      int k = 0;
      for (int i=0; i < c->entries; ++i)
         if (include_in_sort(c, lengths[i])) 
            c->sorted_codewords[k++] = bit_reverse(c->codewords[i]);
      assert(k == c->sorted_entries);
   } else {
      for (int i=0; i < c->sorted_entries; ++i)
         c->sorted_codewords[i] = bit_reverse(c->codewords[i]);
   }

   qsort(c->sorted_codewords, c->sorted_entries, sizeof(c->sorted_codewords[0]), uint32_t_compare);
   c->sorted_codewords[c->sorted_entries] = 0xffffffff;

   int len = c->sparse ? c->sorted_entries : c->entries;
   // now we need to indicate how they correspond; we could either
   //   #1: sort a different data structure that says who they correspond to
   //   #2: for each sorted entry, search the original list to find who corresponds
   //   #3: for each original entry, find the sorted entry
   // #1 requires extra storage, #2 is slow, #3 can use binary search!
   for (int i=0; i < len; ++i) {
      int huff_len = c->sparse ? lengths[values[i]] : lengths[i];
      if (include_in_sort(c,huff_len)) {
         uint32_t code = bit_reverse(c->codewords[i]);
         int x=0, n=c->sorted_entries;
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
static bool vorbis_validate(uint8_t *data)
{
   static uint8_t vorbis[6] = { 'v', 'o', 'r', 'b', 'i', 's' };
   return memcmp(data, vorbis, 6) == 0;
}

// called from setup only, once per code book
// (formula implied by specification)
static int lookup1_values(int entries, int dim)
{
   int r = (int) floorf(expf(logf(entries) / dim));
   if ((int) floorf(powf(r+1, dim)) <= entries)   // (int) cast for MinGW warning;
      ++r;                                              // floorf() to avoid _ftol() when non-CRT
   assert(powf(r+1, dim) > entries);
   assert((int) floorf(powf(r, dim)) <= entries); // (int),floorf() as above
   return r;
}

// called twice per file
static void compute_twiddle_factors(const int n, float *A, float *B, float *C)
{
   for (int k=0; k < n/4; ++k) {
      A[k*2  ] =  cosf(4*k*M_PIf/n);
      A[k*2+1] = -sinf(4*k*M_PIf/n);
      B[k*2  ] =  cosf((k*2+1)*M_PIf/n/2) * 0.5f;
      B[k*2+1] =  sinf((k*2+1)*M_PIf/n/2) * 0.5f;
   }
   for (int k=0; k < n/8; ++k) {
      C[k*2  ] =  cosf(2*(k*2+1)*M_PIf/n);
      C[k*2+1] = -sinf(2*(k*2+1)*M_PIf/n);
   }
}

static void compute_window(const int n, float *window)
{
   for (int i=0; i < n/2; ++i)
      window[i] = sinf(0.5 * M_PIf * square(sinf((i - 0 + 0.5) / (n/2) * 0.5 * M_PIf)));
}

static void compute_bitreverse(const int n, uint16_t *rev)
{
   const int ld = ilog(n) - 1; // ilog is off-by-one from normal definitions
   for (int i=0; i < n/8; ++i)
      rev[i] = (bit_reverse(i) >> (32-ld+3)) << 2;
}

static bool init_blocksize(stb_vorbis *f, const int b, const int n)
{
   f->A[b] = (float *) mem_alloc(f->opaque, sizeof(float) * (n/2));
   f->B[b] = (float *) mem_alloc(f->opaque, sizeof(float) * (n/2));
   f->C[b] = (float *) mem_alloc(f->opaque, sizeof(float) * (n/4));
   if (!f->A[b] || !f->B[b] || !f->C[b]) return error(f, VORBIS_outofmem);
   compute_twiddle_factors(n, f->A[b], f->B[b], f->C[b]);
   f->window[b] = (float *) mem_alloc(f->opaque, sizeof(float) * (n/2));
   if (!f->window[b]) return error(f, VORBIS_outofmem);
   compute_window(n, f->window[b]);
   f->bit_reverse[b] = (uint16_t *) mem_alloc(f->opaque, sizeof(uint16_t) * (n/8));
   if (!f->bit_reverse[b]) return error(f, VORBIS_outofmem);
   compute_bitreverse(n, f->bit_reverse[b]);
   return true;
}

static void neighbors(uint16_t *x, int n, int *plow, int *phigh)
{
   int low = -1;
   int high = 65536;
   for (int i=0; i < n; ++i) {
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

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

bool start_decoder(stb_vorbis *f)
{
   uint8_t header[6], x,y;
   int max_submaps = 0;
   int longest_floorlist=0;

   // first page, first packet

   if (!start_page(f))                              return false;
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
   f->blocksize[0] = 1 << log0;
   f->blocksize[1] = 1 << log1;
   if (log0 < 6 || log0 > 13)                       return error(f, VORBIS_invalid_setup);
   if (log1 < 6 || log1 > 13)                       return error(f, VORBIS_invalid_setup);
   if (log0 > log1)                                 return error(f, VORBIS_invalid_setup);
   }

   // framing_flag
   x = get8(f);
   if (!(x & 1))                                    return error(f, VORBIS_invalid_first_page);

   f->imdct_temp_buf = (float *) mem_alloc(f->opaque, (f->blocksize[1] / 2) * sizeof(*f->imdct_temp_buf));
   if (f->imdct_temp_buf == NULL)                   return error(f, VORBIS_outofmem);

   // second packet!
   if (!start_page(f))                              return false;

   if (!start_packet(f))                            return false;
   flush_packet(f);

   // third packet!
   if (!start_packet(f))                            return false;

   crc32_init(); // always init it, to avoid multithread race conditions

   if (get8_packet(f) != VORBIS_packet_setup)       return error(f, VORBIS_invalid_setup);
   for (int i=0; i < 6; ++i) header[i] = get8_packet(f);
   if (!vorbis_validate(header))                    return error(f, VORBIS_invalid_setup);

   // codebooks

   f->codebook_count = get_bits(f,8) + 1;
   f->codebooks = (Codebook *) mem_alloc(f->opaque, sizeof(*f->codebooks) * f->codebook_count);
   if (f->codebooks == NULL)                        return error(f, VORBIS_outofmem);
   memset(f->codebooks, 0, sizeof(*f->codebooks) * f->codebook_count);
   for (int i=0; i < f->codebook_count; ++i) {
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
      c->sparse = ordered ? false : get_bits(f,1);

      if (c->sparse)
         lengths = (uint8_t *) mem_alloc(f->opaque, c->entries);
      else
         lengths = c->codeword_lengths = (uint8_t *) mem_alloc(f->opaque, c->entries);

      if (!lengths) return error(f, VORBIS_outofmem);

      if (ordered) {
         int current_entry = 0;
         int current_length = get_bits(f,5) + 1;
         while (current_entry < c->entries) {
            int limit = c->entries - current_entry;
            int n = get_bits(f, ilog(limit));
            if (current_entry + n > (int) c->entries) { mem_free(f->opaque, lengths); return error(f, VORBIS_invalid_setup); }
            memset(lengths + current_entry, current_length, n);
            current_entry += n;
            ++current_length;
         }
      } else {
         for (int j=0; j < c->entries; ++j) {
            int present = c->sparse ? get_bits(f,1) : 1;
            if (present) {
               lengths[j] = get_bits(f, 5) + 1;
               ++total;
            } else {
               lengths[j] = NO_CODE;
            }
         }
      }

      if (c->sparse && total >= c->entries/4) {
         // convert sparse items to non-sparse!
         c->codeword_lengths = (uint8_t *) mem_alloc(f->opaque, c->entries);
         if (!c->codeword_lengths) { mem_free(f->opaque, lengths); return error(f, VORBIS_outofmem); }
         memcpy(c->codeword_lengths, lengths, c->entries);
         mem_free(f->opaque, lengths);
         lengths = c->codeword_lengths;
         c->sparse = false;
      }

      // compute the size of the sorted tables
      if (c->sparse) {
         sorted_count = total;
         //assert(total != 0);
      } else {
         sorted_count = 0;
         #ifndef STB_VORBIS_NO_HUFFMAN_BINARY_SEARCH
         for (int j=0; j < c->entries; ++j)
            if (lengths[j] > STB_VORBIS_FAST_HUFFMAN_LENGTH && lengths[j] != NO_CODE)
               ++sorted_count;
         #endif
      }

      c->sorted_entries = sorted_count;
      values = NULL;

      if (!c->sparse) {
         c->codewords = (uint32_t *) mem_alloc(f->opaque, sizeof(c->codewords[0]) * c->entries);
         if (!c->codewords) return error(f, VORBIS_outofmem);
      } else {
         if (c->sorted_entries) {
            c->codeword_lengths = (uint8_t *) mem_alloc(f->opaque, c->sorted_entries);
            if (!c->codeword_lengths) { mem_free(f->opaque, lengths); return error(f, VORBIS_outofmem); }
            c->codewords = (uint32_t *) mem_alloc(f->opaque, sizeof(*c->codewords) * c->sorted_entries);
            if (!c->codewords) { mem_free(f->opaque, lengths); return error(f, VORBIS_outofmem); }
            values = (uint32_t *) mem_alloc(f->opaque, sizeof(*values) * c->sorted_entries);
            if (!values) { mem_free(f->opaque, lengths); return error(f, VORBIS_outofmem); }
         }
      }

      if (!compute_codewords(c, lengths, c->entries, values)) {
         if (c->sparse) {
             mem_free(f->opaque, values);
             mem_free(f->opaque, lengths);
         }
         return error(f, VORBIS_invalid_setup);
      }

      if (c->sorted_entries) {
         // allocate an extra slot for sentinels
         c->sorted_codewords = (uint32_t *) mem_alloc(f->opaque, sizeof(*c->sorted_codewords) * (c->sorted_entries+1));
         if (!c->sorted_codewords) {
             if (c->sparse) {
                 mem_free(f->opaque, values);
                 mem_free(f->opaque, lengths);
             }
             return error(f, VORBIS_outofmem);
         }
         // allocate an extra slot at the front so that c->sorted_values[-1] is defined
         // so that we can catch that case without an extra if
         c->sorted_values    = ( int   *) mem_alloc(f->opaque, sizeof(*c->sorted_values   ) * (c->sorted_entries+1));
         if (!c->sorted_values) {
             if (c->sparse) {
                 mem_free(f->opaque, values);
                 mem_free(f->opaque, lengths);
             }
             return error(f, VORBIS_outofmem);
         }
         ++c->sorted_values; c->sorted_values[-1] = -1;
         compute_sorted_huffman(c, lengths, values);
      }

      if (c->sparse) {
         mem_free(f->opaque, values);
         mem_free(f->opaque, c->codewords);
         mem_free(f->opaque, lengths);
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
         mults = (uint16_t *) mem_alloc(f->opaque, sizeof(mults[0]) * c->lookup_values);
         if (mults == NULL) return error(f, VORBIS_outofmem);
         for (int j=0; j < (int) c->lookup_values; ++j) {
            int q = get_bits(f, c->value_bits);
            if (q == EOP) { mem_free(f->opaque, mults); return error(f, VORBIS_invalid_setup); }
            mults[j] = q;
         }

#ifndef STB_VORBIS_DIVIDES_IN_CODEBOOK
         if (c->lookup_type == 1) {
            int len;
            // pre-expand the lookup1-style multiplicands, to avoid a divide in the inner loop
            if (c->sparse) {
               if (c->sorted_entries == 0) goto skip;
               c->multiplicands = (codetype *) mem_alloc(f->opaque, sizeof(c->multiplicands[0]) * c->sorted_entries * c->dimensions);
            } else
               c->multiplicands = (codetype *) mem_alloc(f->opaque, sizeof(c->multiplicands[0]) * c->entries        * c->dimensions);
            if (c->multiplicands == NULL) { mem_free(f->opaque, mults); return error(f, VORBIS_outofmem); }
            len = c->sparse ? c->sorted_entries : c->entries;
            for (int j=0; j < len; ++j) {
               int z = c->sparse ? c->sorted_values[j] : j, div=1;
               for (int k=0; k < c->dimensions; ++k) {
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
            mem_free(f->opaque, mults);
            c->lookup_type = 2;
         }
         else
#endif
         {
            c->multiplicands = (codetype *) mem_alloc(f->opaque, sizeof(c->multiplicands[0]) * c->lookup_values);
            if (c->multiplicands == NULL) {
               mem_free(f->opaque, mults);
               return error(f, VORBIS_outofmem);
            }
            #ifndef STB_VORBIS_CODEBOOK_FLOATS
            memcpy(c->multiplicands, mults, sizeof(c->multiplicands[0]) * c->lookup_values);
            #else
            for (int j=0; j < (int) c->lookup_values; ++j)
               c->multiplicands[j] = mults[j] * c->delta_value + c->minimum_value;
            mem_free(f->opaque, mults);
            #endif
         }
#ifndef STB_VORBIS_DIVIDES_IN_CODEBOOK
        skip:;
#endif

         #ifdef STB_VORBIS_CODEBOOK_FLOATS
         if (c->lookup_type == 2 && c->sequence_p) {
            for (int j=1; j < (int) c->lookup_values; ++j)
               c->multiplicands[j] = c->multiplicands[j-1];
            c->sequence_p = 0;
         }
         #endif
      }
   }

   // time domain transfers (notused)

   x = get_bits(f, 6) + 1;
   for (int i=0; i < x; ++i) {
      uint32_t z = get_bits(f, 16);
      if (z != 0) return error(f, VORBIS_invalid_setup);
   }

   // Floors
   f->floor_count = get_bits(f, 6)+1;
   f->floor_config = (Floor *)  mem_alloc(f->opaque, f->floor_count * sizeof(*f->floor_config));
   if (!f->floor_config) return error(f, VORBIS_outofmem);
   for (int i=0; i < f->floor_count; ++i) {
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
         for (int j=0; j < g->number_of_books; ++j)
            g->book_list[j] = get_bits(f,8);
         return error(f, VORBIS_feature_not_supported);
      } else {
         Point p[31*8+2];
         Floor1 *g = &f->floor_config[i].floor1;
         int max_class = -1; 
         g->partitions = get_bits(f, 5);
         for (int j=0; j < g->partitions; ++j) {
            g->partition_class_list[j] = get_bits(f, 4);
            if (g->partition_class_list[j] > max_class)
               max_class = g->partition_class_list[j];
         }
         for (int j=0; j <= max_class; ++j) {
            g->class_dimensions[j] = get_bits(f, 3)+1;
            g->class_subclasses[j] = get_bits(f, 2);
            if (g->class_subclasses[j]) {
               g->class_masterbooks[j] = get_bits(f, 8);
               if (g->class_masterbooks[j] >= f->codebook_count) return error(f, VORBIS_invalid_setup);
            }
            for (int k=0; k < 1 << g->class_subclasses[j]; ++k) {
               g->subclass_books[j][k] = get_bits(f,8)-1;
               if (g->subclass_books[j][k] >= f->codebook_count) return error(f, VORBIS_invalid_setup);
            }
         }
         g->floor1_multiplier = get_bits(f,2)+1;
         g->rangebits = get_bits(f,4);
         g->Xlist[0] = 0;
         g->Xlist[1] = 1 << g->rangebits;
         g->values = 2;
         for (int j=0; j < g->partitions; ++j) {
            int c = g->partition_class_list[j];
            for (int k=0; k < g->class_dimensions[c]; ++k) {
               g->Xlist[g->values] = get_bits(f, g->rangebits);
               ++g->values;
            }
         }
         // precompute the sorting
         for (int j=0; j < g->values; ++j) {
            p[j].x = g->Xlist[j];
            p[j].y = j;
         }
         qsort(p, g->values, sizeof(p[0]), point_compare);
         for (int j=0; j < g->values; ++j)
            g->sorted_order[j] = (uint8_t) p[j].y;
         // precompute the neighbors
         for (int j=2; j < g->values; ++j) {
            int low = 0, hi = 0;  // Initialized to avoid a warning.
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
   f->residue_config = (Residue *) mem_alloc(f->opaque, f->residue_count * sizeof(*f->residue_config));
   if (!f->residue_config) return error(f, VORBIS_outofmem);
   memset(f->residue_config, 0, f->residue_count * sizeof(*f->residue_config));
   for (int i=0; i < f->residue_count; ++i) {
      uint8_t residue_cascade[64];
      Residue *r = f->residue_config+i;
      f->residue_types[i] = get_bits(f, 16);
      if (f->residue_types[i] > 2) return error(f, VORBIS_invalid_setup);
      r->begin = get_bits(f, 24);
      r->end = get_bits(f, 24);
      r->part_size = get_bits(f,24)+1;
      r->classifications = get_bits(f,6)+1;
      r->classbook = get_bits(f,8);
      for (int j=0; j < r->classifications; ++j) {
         uint8_t high_bits=0;
         uint8_t low_bits=get_bits(f,3);
         if (get_bits(f,1))
            high_bits = get_bits(f,5);
         residue_cascade[j] = high_bits*8 + low_bits;
      }
      r->residue_books = (short (*)[8]) mem_alloc(f->opaque, sizeof(r->residue_books[0]) * r->classifications);
      if (!r->residue_books) return error(f, VORBIS_outofmem);
      for (int j=0; j < r->classifications; ++j) {
         for (int k=0; k < 8; ++k) {
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
      r->classdata = (uint8_t **) mem_alloc(f->opaque, sizeof(*r->classdata) * f->codebooks[r->classbook].entries);
      if (!r->classdata) return error(f, VORBIS_outofmem);
      memset(r->classdata, 0, sizeof(*r->classdata) * f->codebooks[r->classbook].entries);
      int classwords = f->codebooks[r->classbook].dimensions;
      r->classdata[0] = (uint8_t *) mem_alloc(f->opaque, f->codebooks[r->classbook].entries * sizeof(r->classdata[0][0]) * classwords);
      if (!r->classdata[0]) return error(f, VORBIS_outofmem);
      for (int j=0; j < f->codebooks[r->classbook].entries; ++j) {
         int temp = j;
         r->classdata[j] = r->classdata[0] + j*classwords;
         for (int k=classwords-1; k >= 0; --k) {
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
   f->classifications = (int **) alloc_channel_array(f->opaque, f->channels, residue_max_alloc * sizeof(**f->classifications));
   if (!f->classifications) return error(f, VORBIS_outofmem);
#else
   f->part_classdata = (uint8_t ***) alloc_channel_array(f->opaque, f->channels, residue_max_alloc * sizeof(**f->part_classdata));
   if (!f->part_classdata) return error(f, VORBIS_outofmem);
#endif

   f->mapping_count = get_bits(f,6)+1;
   f->mapping = (Mapping *) mem_alloc(f->opaque, f->mapping_count * sizeof(*f->mapping));
   if (!f->mapping) return error(f, VORBIS_outofmem);
   memset(f->mapping, 0, f->mapping_count * sizeof(*f->mapping));
   f->mapping[0].chan = (MappingChannel *) mem_alloc(f->opaque, f->mapping_count * f->channels * sizeof(*f->mapping[0].chan));
   if (!f->mapping[0].chan) return error(f, VORBIS_outofmem);
   for (int i=0; i < f->mapping_count; ++i) {
      Mapping *m = f->mapping + i;      
      int mapping_type = get_bits(f,16);
      if (mapping_type != 0) return error(f, VORBIS_invalid_setup);
      m->chan = f->mapping[0].chan + (i * f->channels);
      if (get_bits(f,1))
         m->submaps = get_bits(f,4);
      else
         m->submaps = 1;
      if (m->submaps > max_submaps)
         max_submaps = m->submaps;
      if (get_bits(f,1)) {
         m->coupling_steps = get_bits(f,8)+1;
         for (int k=0; k < m->coupling_steps; ++k) {
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
         for (int j=0; j < f->channels; ++j) {
            m->chan[j].mux = get_bits(f, 4);
            if (m->chan[j].mux >= m->submaps)                return error(f, VORBIS_invalid_setup);
         }
      } else
         // @SPECIFICATION: this case is missing from the spec
         for (int j=0; j < f->channels; ++j)
            m->chan[j].mux = 0;

      for (int j=0; j < m->submaps; ++j) {
         get_bits(f,8); // discard
         m->submap_floor[j] = get_bits(f,8);
         m->submap_residue[j] = get_bits(f,8);
         if (m->submap_floor[j] >= f->floor_count)      return error(f, VORBIS_invalid_setup);
         if (m->submap_residue[j] >= f->residue_count)  return error(f, VORBIS_invalid_setup);
      }
   }

   // Modes
   f->mode_count = get_bits(f, 6)+1;
   for (int i=0; i < f->mode_count; ++i) {
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

   f->channel_buffers = (float **) alloc_channel_array(f->opaque, f->channels, sizeof(float) * f->blocksize[1]);
   f->outputs         = (float **) mem_alloc(f->opaque, f->channels * sizeof(float *));
   f->previous_window = (float **) alloc_channel_array(f->opaque, f->channels, sizeof(float) * f->blocksize[1]/2);
   f->finalY          = (int16_t **) alloc_channel_array(f->opaque, f->channels, sizeof(int16_t) * longest_floorlist);
   if (!f->channel_buffers) return error(f, VORBIS_outofmem);
   if (!f->outputs)         return error(f, VORBIS_outofmem);
   if (!f->previous_window) return error(f, VORBIS_outofmem);
   if (!f->finalY)          return error(f, VORBIS_outofmem);
   for (int i=0; i < f->channels; ++i) {
       memset(f->channel_buffers[i], 0, sizeof(float) * f->blocksize[1]);
   }

   if (!init_blocksize(f, 0, f->blocksize[0])) return false;
   if (!init_blocksize(f, 1, f->blocksize[1])) return false;

   f->first_decode = true;

   f->first_audio_page_offset = get_file_offset(f);

   return true;
}

/*************************************************************************/
/*************************************************************************/

