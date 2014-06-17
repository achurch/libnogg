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

#include <math.h>
#include <stdlib.h>
#include <string.h>

//FIXME: not fully reviewed

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

#define M_PIf  3.14159265f

enum
{
   VORBIS_packet_ident = 1,
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
   if (k == n) { ASSERT(c->sorted_entries == 0); return true; }
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
      if (z == 0) { ASSERT(0); return false; }
      res = available[z];
      available[z] = 0;
      add_entry(c, bit_reverse(res), i, m++, len[i], values);
      // propogate availability up the tree
      if (z != len[i]) {
         for (y=len[i]; y > z; --y) {
            ASSERT(available[y] == 0);
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
   if (c->sparse) { ASSERT(len != NO_CODE); return true; }
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
      ASSERT(k == c->sorted_entries);
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
         ASSERT(c->sorted_codewords[x] == code);
         if (c->sparse) {
            c->sorted_values[x] = values[i];
            c->codeword_lengths[x] = huff_len;
         } else {
            c->sorted_values[x] = i;
         }
      }
   }
}

// called from setup only, once per code book
// (formula implied by specification)
static int lookup1_values(int entries, int dim)
{
   int r = (int) floorf(expf(logf(entries) / dim));
   if ((int) floorf(powf(r+1, dim)) <= entries)   // (int) cast for MinGW warning;
      ++r;                                              // floorf() to avoid _ftol() when non-CRT
   ASSERT(powf(r+1, dim) > entries);
   ASSERT((int) floorf(powf(r, dim)) <= entries); // (int),floorf() as above
   return r;
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

/*-----------------------------------------------------------------------*/

/**
 * validate_header_packet:  Validate the Vorbis header packet at the
 * current stream read position and return its type.
 *
 * [Parameters]
 *     handle: Stream handle.
 * [Return value]
 *     Packet type (nonzero), or zero on error or invalid packet.
 */
static int validate_header_packet(stb_vorbis *handle)
{
    uint8_t buf[7];
    if (!getn_packet(handle, buf, sizeof(buf))) {
        return 0;
    }
    if (!(buf[0] & 1) || memcmp(buf+1, "vorbis", 6) != 0) {
        return 0;
    }
    return buf[0];
}

/*************************************************************************/
/************** Setup data parsing/initialization routines ***************/
/*************************************************************************/

/**
 * init_blocksize:  Allocate and initialize lookup tables used for each
 * audio block size.  handle->blocksize[] is assumed to have been initialized.
 *
 * On error, handle->error will be set appropriately.
 *
 * [Parameters]
 *     handle: Stream handle.
 *     index: Block size index (0 or 1).
 * [Return value]
 *     True on success, false on error.
 */
static bool init_blocksize(stb_vorbis *handle, const int index)
{
    const int blocksize = handle->blocksize[index];

    handle->A[index] =
        mem_alloc(handle->opaque, sizeof(float) * (blocksize/2));
    handle->B[index] =
        mem_alloc(handle->opaque, sizeof(float) * (blocksize/2));
    handle->C[index] =
        mem_alloc(handle->opaque, sizeof(float) * (blocksize/4));
    if (!handle->A[index] || !handle->B[index] || !handle->C[index]) {
        return error(handle, VORBIS_outofmem);
    }
    for (int i = 0; i < blocksize/4; i++) {
        handle->A[index][i*2  ] =  cosf(4*i*M_PIf / blocksize);
        handle->A[index][i*2+1] = -sinf(4*i*M_PIf / blocksize);
        handle->B[index][i*2  ] =  cosf((i*2+1)*(M_PIf/2) / blocksize) * 0.5f;
        handle->B[index][i*2+1] =  sinf((i*2+1)*(M_PIf/2) / blocksize) * 0.5f;
    }
    for (int i = 0; i < blocksize/8; i++) {
        handle->C[index][i*2  ] =  cosf(2*(i*2+1)*M_PIf / blocksize);
        handle->C[index][i*2+1] = -sinf(2*(i*2+1)*M_PIf / blocksize);
    }

    handle->window[index] =
        mem_alloc(handle->opaque, sizeof(float) * (blocksize/2));
    if (!handle->window[index]) {
        return error(handle, VORBIS_outofmem);
    }
    for (int i = 0; i < blocksize/2; i++) {
        const float x = sinf((i+0.5f)*(M_PIf/2) / (blocksize/2));
        handle->window[index][i] = sinf(0.5 * M_PIf * (x*x));
    }

    handle->bit_reverse[index] =
        mem_alloc(handle->opaque, sizeof(uint16_t) * (blocksize/8));
    if (!handle->bit_reverse[index]) {
        return error(handle, VORBIS_outofmem);
    }
    /* ilog(n) gives log2(n)+1, so we need to subtract 1 for real log2. */
    const int bits = ilog(blocksize) - 1;
    for (int i = 0; i < blocksize/8; i++) {
        handle->bit_reverse[index][i] = (bit_reverse(i) >> (32-bits+3)) << 2;
    }

   return true;
}

/*-----------------------------------------------------------------------*/

/**
 * parse_codebooks:  Parse codebook data from the setup header.
 *
 * [Parameters]
 *     handle: Stream handle.
 */
static bool parse_codebooks(stb_vorbis *handle)
{
   uint8_t x,y;

   handle->codebook_count = get_bits(handle,8) + 1;
   handle->codebooks = (Codebook *) mem_alloc(handle->opaque, sizeof(*handle->codebooks) * handle->codebook_count);
   if (handle->codebooks == NULL)                        return error(handle, VORBIS_outofmem);
   memset(handle->codebooks, 0, sizeof(*handle->codebooks) * handle->codebook_count);
   for (int i=0; i < handle->codebook_count; ++i) {
      uint32_t *values;
      int ordered, sorted_count;
      int total=0;
      uint8_t *lengths;
      Codebook *c = handle->codebooks+i;
      x = get_bits(handle, 8); if (x != 0x42)            return error(handle, VORBIS_invalid_setup);
      x = get_bits(handle, 8); if (x != 0x43)            return error(handle, VORBIS_invalid_setup);
      x = get_bits(handle, 8); if (x != 0x56)            return error(handle, VORBIS_invalid_setup);
      x = get_bits(handle, 8);
      c->dimensions = (get_bits(handle, 8)<<8) + x;
      x = get_bits(handle, 8);
      y = get_bits(handle, 8);
      c->entries = (get_bits(handle, 8)<<16) + (y<<8) + x;
      ordered = get_bits(handle,1);
      c->sparse = ordered ? false : get_bits(handle,1);

      if (c->sparse)
         lengths = (uint8_t *) mem_alloc(handle->opaque, c->entries);
      else
         lengths = c->codeword_lengths = (uint8_t *) mem_alloc(handle->opaque, c->entries);

      if (!lengths) return error(handle, VORBIS_outofmem);

      if (ordered) {
         int current_entry = 0;
         int current_length = get_bits(handle,5) + 1;
         while (current_entry < c->entries) {
            int limit = c->entries - current_entry;
            int n = get_bits(handle, ilog(limit));
            if (current_entry + n > (int) c->entries) { mem_free(handle->opaque, lengths); return error(handle, VORBIS_invalid_setup); }
            memset(lengths + current_entry, current_length, n);
            current_entry += n;
            ++current_length;
         }
      } else {
         for (int j=0; j < c->entries; ++j) {
            int present = c->sparse ? get_bits(handle,1) : 1;
            if (present) {
               lengths[j] = get_bits(handle, 5) + 1;
               ++total;
            } else {
               lengths[j] = NO_CODE;
            }
         }
      }

      if (c->sparse && total >= c->entries/4) {
         // convert sparse items to non-sparse!
         c->codeword_lengths = (uint8_t *) mem_alloc(handle->opaque, c->entries);
         if (!c->codeword_lengths) { mem_free(handle->opaque, lengths); return error(handle, VORBIS_outofmem); }
         memcpy(c->codeword_lengths, lengths, c->entries);
         mem_free(handle->opaque, lengths);
         lengths = c->codeword_lengths;
         c->sparse = false;
      }

      // compute the size of the sorted tables
      if (c->sparse) {
         sorted_count = total;
         //ASSERT(total != 0);
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
         c->codewords = (uint32_t *) mem_alloc(handle->opaque, sizeof(c->codewords[0]) * c->entries);
         if (!c->codewords) return error(handle, VORBIS_outofmem);
      } else {
         if (c->sorted_entries) {
            c->codeword_lengths = (uint8_t *) mem_alloc(handle->opaque, c->sorted_entries);
            if (!c->codeword_lengths) { mem_free(handle->opaque, lengths); return error(handle, VORBIS_outofmem); }
            c->codewords = (uint32_t *) mem_alloc(handle->opaque, sizeof(*c->codewords) * c->sorted_entries);
            if (!c->codewords) { mem_free(handle->opaque, lengths); return error(handle, VORBIS_outofmem); }
            values = (uint32_t *) mem_alloc(handle->opaque, sizeof(*values) * c->sorted_entries);
            if (!values) { mem_free(handle->opaque, lengths); return error(handle, VORBIS_outofmem); }
         }
      }

      if (!compute_codewords(c, lengths, c->entries, values)) {
         if (c->sparse) {
             mem_free(handle->opaque, values);
             mem_free(handle->opaque, lengths);
         }
         return error(handle, VORBIS_invalid_setup);
      }

      if (c->sorted_entries) {
         // allocate an extra slot for sentinels
         c->sorted_codewords = (uint32_t *) mem_alloc(handle->opaque, sizeof(*c->sorted_codewords) * (c->sorted_entries+1));
         if (!c->sorted_codewords) {
             if (c->sparse) {
                 mem_free(handle->opaque, values);
                 mem_free(handle->opaque, lengths);
             }
             return error(handle, VORBIS_outofmem);
         }
         // allocate an extra slot at the front so that c->sorted_values[-1] is defined
         // so that we can catch that case without an extra if
         c->sorted_values    = ( int   *) mem_alloc(handle->opaque, sizeof(*c->sorted_values   ) * (c->sorted_entries+1));
         if (!c->sorted_values) {
             if (c->sparse) {
                 mem_free(handle->opaque, values);
                 mem_free(handle->opaque, lengths);
             }
             return error(handle, VORBIS_outofmem);
         }
         ++c->sorted_values; c->sorted_values[-1] = -1;
         compute_sorted_huffman(c, lengths, values);
      }

      if (c->sparse) {
         mem_free(handle->opaque, values);
         mem_free(handle->opaque, c->codewords);
         mem_free(handle->opaque, lengths);
         c->codewords = NULL;
      }

      compute_accelerated_huffman(c);

      c->lookup_type = get_bits(handle, 4);
      if (c->lookup_type > 2) return error(handle, VORBIS_invalid_setup);
      if (c->lookup_type > 0) {
         uint16_t *mults;
         c->minimum_value = float32_unpack(get_bits(handle, 32));
         c->delta_value = float32_unpack(get_bits(handle, 32));
         c->value_bits = get_bits(handle, 4)+1;
         c->sequence_p = get_bits(handle,1);
         if (c->lookup_type == 1) {
            c->lookup_values = lookup1_values(c->entries, c->dimensions);
         } else {
            c->lookup_values = c->entries * c->dimensions;
         }
         mults = (uint16_t *) mem_alloc(handle->opaque, sizeof(mults[0]) * c->lookup_values);
         if (mults == NULL) return error(handle, VORBIS_outofmem);
         for (int j=0; j < (int) c->lookup_values; ++j) {
            int q = get_bits(handle, c->value_bits);
            if (q == EOP) { mem_free(handle->opaque, mults); return error(handle, VORBIS_invalid_setup); }
            mults[j] = q;
         }

#ifndef STB_VORBIS_DIVIDES_IN_CODEBOOK
         if (c->lookup_type == 1) {
            int len;
            // pre-expand the lookup1-style multiplicands, to avoid a divide in the inner loop
            if (c->sparse) {
               if (c->sorted_entries == 0) goto skip;
               c->multiplicands = (codetype *) mem_alloc(handle->opaque, sizeof(c->multiplicands[0]) * c->sorted_entries * c->dimensions);
            } else
               c->multiplicands = (codetype *) mem_alloc(handle->opaque, sizeof(c->multiplicands[0]) * c->entries        * c->dimensions);
            if (c->multiplicands == NULL) { mem_free(handle->opaque, mults); return error(handle, VORBIS_outofmem); }
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
            mem_free(handle->opaque, mults);
            c->lookup_type = 2;
         }
         else
#endif
         {
            c->multiplicands = (codetype *) mem_alloc(handle->opaque, sizeof(c->multiplicands[0]) * c->lookup_values);
            if (c->multiplicands == NULL) {
               mem_free(handle->opaque, mults);
               return error(handle, VORBIS_outofmem);
            }
            #ifndef STB_VORBIS_CODEBOOK_FLOATS
            memcpy(c->multiplicands, mults, sizeof(c->multiplicands[0]) * c->lookup_values);
            #else
            for (int j=0; j < (int) c->lookup_values; ++j)
               c->multiplicands[j] = mults[j] * c->delta_value + c->minimum_value;
            mem_free(handle->opaque, mults);
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

   return true;
}

/*-----------------------------------------------------------------------*/

/**
 * parse_time_domain_transforms:  Parse time domain transform data from the
 * setup header.  The spec declares that these are all placeholders in the
 * current Vorbis version, so we don't actually store any data.
 *
 * [Parameters]
 *     handle: Stream handle.
 */
static bool parse_time_domain_transforms(stb_vorbis *handle)
{
    const int vorbis_time_count = get_bits(handle, 6) + 1;
    for (int i = 0; i < vorbis_time_count; i++) {
        const uint16_t value = get_bits(handle, 16);
        if (value != 0) {
            return error(handle, VORBIS_invalid_setup);
        }
    }

    return true;
}

/*-----------------------------------------------------------------------*/

/**
 * parse_floors:  Parse floor data from the setup header.
 *
 * [Parameters]
 *     handle: Stream handle.
 */
static bool parse_floors(stb_vorbis *handle)
{
   int longest_floorlist = 0;

   handle->floor_count = get_bits(handle, 6)+1;
   handle->floor_config = (Floor *)  mem_alloc(handle->opaque, handle->floor_count * sizeof(*handle->floor_config));
   if (!handle->floor_config) return error(handle, VORBIS_outofmem);
   for (int i=0; i < handle->floor_count; ++i) {
      handle->floor_types[i] = get_bits(handle, 16);
      if (handle->floor_types[i] > 1) return error(handle, VORBIS_invalid_setup);
      if (handle->floor_types[i] == 0) {
         Floor0 *g = &handle->floor_config[i].floor0;
         g->order = get_bits(handle,8);
         g->rate = get_bits(handle,16);
         g->bark_map_size = get_bits(handle,16);
         g->amplitude_bits = get_bits(handle,6);
         g->amplitude_offset = get_bits(handle,8);
         g->number_of_books = get_bits(handle,4) + 1;
         for (int j=0; j < g->number_of_books; ++j)
            g->book_list[j] = get_bits(handle,8);
         return error(handle, VORBIS_feature_not_supported);
      } else {
         Point p[31*8+2];
         Floor1 *g = &handle->floor_config[i].floor1;
         int max_class = -1; 
         g->partitions = get_bits(handle, 5);
         for (int j=0; j < g->partitions; ++j) {
            g->partition_class_list[j] = get_bits(handle, 4);
            if (g->partition_class_list[j] > max_class)
               max_class = g->partition_class_list[j];
         }
         for (int j=0; j <= max_class; ++j) {
            g->class_dimensions[j] = get_bits(handle, 3)+1;
            g->class_subclasses[j] = get_bits(handle, 2);
            if (g->class_subclasses[j]) {
               g->class_masterbooks[j] = get_bits(handle, 8);
               if (g->class_masterbooks[j] >= handle->codebook_count) return error(handle, VORBIS_invalid_setup);
            }
            for (int k=0; k < 1 << g->class_subclasses[j]; ++k) {
               g->subclass_books[j][k] = get_bits(handle,8)-1;
               if (g->subclass_books[j][k] >= handle->codebook_count) return error(handle, VORBIS_invalid_setup);
            }
         }
         g->floor1_multiplier = get_bits(handle,2)+1;
         g->rangebits = get_bits(handle,4);
         g->Xlist[0] = 0;
         g->Xlist[1] = 1 << g->rangebits;
         g->values = 2;
         for (int j=0; j < g->partitions; ++j) {
            int c = g->partition_class_list[j];
            for (int k=0; k < g->class_dimensions[c]; ++k) {
               g->Xlist[g->values] = get_bits(handle, g->rangebits);
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

    handle->finalY = alloc_channel_array(
        handle->opaque, handle->channels, sizeof(int16_t) * longest_floorlist);
    if (!handle->finalY) {
        return error(handle, VORBIS_outofmem);
    }

   return true;
}

/**
 * parse_residues:  Parse residue data from the setup header.
 *
 * [Parameters]
 *     handle: Stream handle.
 */
static bool parse_residues(stb_vorbis *handle)
{
   int residue_max_alloc = 0;
   handle->residue_count = get_bits(handle, 6)+1;
   handle->residue_config = (Residue *) mem_alloc(handle->opaque, handle->residue_count * sizeof(*handle->residue_config));
   if (!handle->residue_config) return error(handle, VORBIS_outofmem);
   memset(handle->residue_config, 0, handle->residue_count * sizeof(*handle->residue_config));
   for (int i=0; i < handle->residue_count; ++i) {
      uint8_t residue_cascade[64];
      Residue *r = handle->residue_config+i;
      handle->residue_types[i] = get_bits(handle, 16);
      if (handle->residue_types[i] > 2) return error(handle, VORBIS_invalid_setup);
      r->begin = get_bits(handle, 24);
      r->end = get_bits(handle, 24);
      r->part_size = get_bits(handle,24)+1;
      r->classifications = get_bits(handle,6)+1;
      r->classbook = get_bits(handle,8);
      for (int j=0; j < r->classifications; ++j) {
         uint8_t high_bits=0;
         uint8_t low_bits=get_bits(handle,3);
         if (get_bits(handle,1))
            high_bits = get_bits(handle,5);
         residue_cascade[j] = high_bits*8 + low_bits;
      }
      r->residue_books = (short (*)[8]) mem_alloc(handle->opaque, sizeof(r->residue_books[0]) * r->classifications);
      if (!r->residue_books) return error(handle, VORBIS_outofmem);
      for (int j=0; j < r->classifications; ++j) {
         for (int k=0; k < 8; ++k) {
            if (residue_cascade[j] & (1 << k)) {
               r->residue_books[j][k] = get_bits(handle, 8);
               if (r->residue_books[j][k] >= handle->codebook_count) return error(handle, VORBIS_invalid_setup);
            } else {
               r->residue_books[j][k] = -1;
            }
         }
      }
      // precompute the classifications[] array to avoid inner-loop mod/divide
      // call it 'classdata' since we already have r->classifications
      r->classdata = (uint8_t **) mem_alloc(handle->opaque, sizeof(*r->classdata) * handle->codebooks[r->classbook].entries);
      if (!r->classdata) return error(handle, VORBIS_outofmem);
      memset(r->classdata, 0, sizeof(*r->classdata) * handle->codebooks[r->classbook].entries);
      int classwords = handle->codebooks[r->classbook].dimensions;
      r->classdata[0] = (uint8_t *) mem_alloc(handle->opaque, handle->codebooks[r->classbook].entries * sizeof(r->classdata[0][0]) * classwords);
      if (!r->classdata[0]) return error(handle, VORBIS_outofmem);
      for (int j=0; j < handle->codebooks[r->classbook].entries; ++j) {
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
   handle->classifications = (int **) alloc_channel_array(handle->opaque, handle->channels, residue_max_alloc * sizeof(**handle->classifications));
   if (!handle->classifications) return error(handle, VORBIS_outofmem);
#else
   handle->part_classdata = (uint8_t ***) alloc_channel_array(handle->opaque, handle->channels, residue_max_alloc * sizeof(**handle->part_classdata));
   if (!handle->part_classdata) return error(handle, VORBIS_outofmem);
#endif

   return true;
}

/**
 * parse_mappings:  Parse mapping data from the setup header.
 *
 * [Parameters]
 *     handle: Stream handle.
 */
static bool parse_mappings(stb_vorbis *handle)
{
   int max_submaps = 0;

   handle->mapping_count = get_bits(handle,6)+1;
   handle->mapping = (Mapping *) mem_alloc(handle->opaque, handle->mapping_count * sizeof(*handle->mapping));
   if (!handle->mapping) return error(handle, VORBIS_outofmem);
   memset(handle->mapping, 0, handle->mapping_count * sizeof(*handle->mapping));
   handle->mapping[0].chan = (MappingChannel *) mem_alloc(handle->opaque, handle->mapping_count * handle->channels * sizeof(*handle->mapping[0].chan));
   if (!handle->mapping[0].chan) return error(handle, VORBIS_outofmem);
   for (int i=0; i < handle->mapping_count; ++i) {
      Mapping *m = handle->mapping + i;      
      int mapping_type = get_bits(handle,16);
      if (mapping_type != 0) return error(handle, VORBIS_invalid_setup);
      m->chan = handle->mapping[0].chan + (i * handle->channels);
      if (get_bits(handle,1))
         m->submaps = get_bits(handle,4);
      else
         m->submaps = 1;
      if (m->submaps > max_submaps)
         max_submaps = m->submaps;
      if (get_bits(handle,1)) {
         m->coupling_steps = get_bits(handle,8)+1;
         for (int k=0; k < m->coupling_steps; ++k) {
            m->chan[k].magnitude = get_bits(handle, ilog(handle->channels)-1);
            m->chan[k].angle = get_bits(handle, ilog(handle->channels)-1);
            if (m->chan[k].magnitude >= handle->channels)        return error(handle, VORBIS_invalid_setup);
            if (m->chan[k].angle     >= handle->channels)        return error(handle, VORBIS_invalid_setup);
            if (m->chan[k].magnitude == m->chan[k].angle)   return error(handle, VORBIS_invalid_setup);
         }
      } else
         m->coupling_steps = 0;

      // reserved field
      if (get_bits(handle,2)) return error(handle, VORBIS_invalid_setup);
      if (m->submaps > 1) {
         for (int j=0; j < handle->channels; ++j) {
            m->chan[j].mux = get_bits(handle, 4);
            if (m->chan[j].mux >= m->submaps)                return error(handle, VORBIS_invalid_setup);
         }
      } else
         // @SPECIFICATION: this case is missing from the spec
         for (int j=0; j < handle->channels; ++j)
            m->chan[j].mux = 0;

      for (int j=0; j < m->submaps; ++j) {
         get_bits(handle,8); // discard
         m->submap_floor[j] = get_bits(handle,8);
         m->submap_residue[j] = get_bits(handle,8);
         if (m->submap_floor[j] >= handle->floor_count)      return error(handle, VORBIS_invalid_setup);
         if (m->submap_residue[j] >= handle->residue_count)  return error(handle, VORBIS_invalid_setup);
      }
   }

   return true;
}

/*-----------------------------------------------------------------------*/

/**
 * parse_modes:  Parse mode data from the setup header.
 *
 * [Parameters]
 *     handle: Stream handle.
 */
static bool parse_modes(stb_vorbis *handle)
{
   handle->mode_count = get_bits(handle, 6)+1;
   for (int i=0; i < handle->mode_count; ++i) {
      Mode *m = handle->mode_config+i;
      m->blockflag = get_bits(handle,1);
      m->windowtype = get_bits(handle,16);
      m->transformtype = get_bits(handle,16);
      m->mapping = get_bits(handle,8);
      if (m->windowtype != 0)                 return error(handle, VORBIS_invalid_setup);
      if (m->transformtype != 0)              return error(handle, VORBIS_invalid_setup);
      if (m->mapping >= handle->mapping_count)     return error(handle, VORBIS_invalid_setup);
   }

   return true;
}

/*************************************************************************/
/*********************** Top-level packet parsers ************************/
/*************************************************************************/

/**
 * parse_ident_header:  Parse the Vorbis identification header packet.
 * The packet length is assumed to have been validated as 30 bytes, and
 * the 7-byte packet header is assumed to have already been read.
 *
 * [Parameters]
 *     handle: Stream handle.
 * [Return value]
 *     True on success, false on error.
 */
static bool parse_ident_header(stb_vorbis *handle)
{
    const  uint32_t vorbis_version    = get32_packet(handle);
    const  int      audio_channels    = get8_packet(handle);
    const  uint32_t audio_sample_rate = get32_packet(handle);
    UNUSED int32_t  bitrate_maximum   = get32_packet(handle);
    UNUSED int32_t  bitrate_nominal   = get32_packet(handle);
    UNUSED int32_t  bitrate_minimum   = get32_packet(handle);
    const  int      log2_blocksize    = get8_packet(handle);
    const  int      framing_flag      = get8_packet(handle);
    ASSERT(framing_flag != EOP);
    ASSERT(get8_packet(handle) == EOP);

    const int log2_blocksize_0 = (log2_blocksize >> 0) & 15;
    const int log2_blocksize_1 = (log2_blocksize >> 4) & 15;

    if (vorbis_version != 0
     || audio_channels == 0
     || audio_sample_rate == 0
     || !(framing_flag & 1)) {
        return error(handle, VORBIS_invalid_first_page);
    }
    if (log2_blocksize_0 < 6
     || log2_blocksize_0 > 13
     || log2_blocksize_1 < log2_blocksize_0
     || log2_blocksize_1 > 13) {
        return error(handle, VORBIS_invalid_setup);
    }

    handle->channels = audio_channels;
    handle->sample_rate = audio_sample_rate;
    handle->blocksize[0] = 1 << log2_blocksize_0;
    handle->blocksize[1] = 1 << log2_blocksize_1;
    return true;
}

/*-----------------------------------------------------------------------*/

/**
 * parse_comment_header:  Parse the Vorbis comment header packet.  The
 * 7-byte packet header is assumed to have already been read.
 *
 * [Parameters]
 *     handle: Stream handle.
 * [Return value]
 *     True on success, false on error.
 */
static bool parse_comment_header(stb_vorbis *handle)
{
    /* Currently, we don't attempt to parse anything out of the comment
     * header. */
    flush_packet(handle);
    return true;
}

/*-----------------------------------------------------------------------*/

/**
 * parse_setup_header:  Parse the Vorbis setup header packet.  The 7-byte
 * packet header is assumed to have already been read.
 *
 * [Parameters]
 *     handle: Stream handle.
 * [Return value]
 *     True on success, false on error.
 */
static bool parse_setup_header(stb_vorbis *handle)
{
    if (!parse_codebooks(handle)) {
        return false;
    }
    if (!parse_time_domain_transforms(handle)) {
        return false;
    }
    if (!parse_floors(handle)) {
        return false;
    }
    if (!parse_residues(handle)) {
        return false;
    }
    if (!parse_mappings(handle)) {
        return false;
    }
    if (!parse_modes(handle)) {
        return false;
    }

    if (get_bits(handle, 1) != 1) {
        return error(handle, VORBIS_invalid_setup);
    }
    flush_packet(handle);

    return true;
}

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

bool start_decoder(stb_vorbis *handle)
{
    /* Initialize the CRC lookup table, if necessary.  This function
     * modifies global state but is multithread-safe, so we just call it
     * unconditionally. */
    crc32_init();

    /* Check for and parse the identification header packet.  The spec
     * requires that this packet is the only packet in the first Ogg page
     * of the stream.  We follow this requirement to ensure that we're
     * looking at a valid Ogg Vorbis stream. */
    if (!start_page(handle)) {
        return false;
    }
    if (handle->page_flag != PAGEFLAG_first_page
     || handle->segment_count != 1
     || handle->segments[0] != 30) {
        return error(handle, VORBIS_invalid_first_page);
    }
    start_packet(handle);
    if (validate_header_packet(handle) != VORBIS_packet_ident) {
        return error(handle, VORBIS_invalid_first_page);
    }
    if (!parse_ident_header(handle)) {
        return false;
    }

    /* Set up stream parameters and allocate buffers based on the stream
     * format. */
    for (int i = 0; i < 2; i++) {
        if (!init_blocksize(handle, i)) {
            return false;
        }
    }
    handle->channel_buffers = alloc_channel_array(
        handle->opaque, handle->channels,
        sizeof(float) * handle->blocksize[1]);
    handle->outputs = mem_alloc(
        handle->opaque, handle->channels * sizeof(float *));
    handle->previous_window = alloc_channel_array(
        handle->opaque, handle->channels,
        sizeof(float) * handle->blocksize[1]/2);
    handle->imdct_temp_buf = mem_alloc(
        handle->opaque,
        (handle->blocksize[1] / 2) * sizeof(*handle->imdct_temp_buf));
    if (!handle->channel_buffers
     || !handle->outputs
     || !handle->previous_window
     || !handle->imdct_temp_buf) {
        return error(handle, VORBIS_outofmem);
    }
    for (int i = 0; i < handle->channels; i++) {
        memset(handle->channel_buffers[i], 0,
               sizeof(float) * handle->blocksize[1]);
    }

    /* The second Ogg page (and possibly subsequent pages) should contain
     * the comment and setup headers.  The spec requires both headers to
     * exist in that order, but in the spirit of being liberal with what
     * we accept, we also allow the comment header to be missing. */
    if (!start_page(handle)) {
        return false;
    }
    bool got_setup_header = false;
    while (!got_setup_header) {
        if (!start_packet(handle)) {
            return false;
        }
        const int type = validate_header_packet(handle);
        switch (type) {
          case VORBIS_packet_comment:
            if (!parse_comment_header(handle)) {
                return false;
            }
            break;
          case VORBIS_packet_setup:
            if (!parse_setup_header(handle)) {
                return false;
            }
            got_setup_header = true;
            break;
          default:
            return error(handle, VORBIS_invalid_stream);
        }
    }

    /* Set up remaining state parameters for decoding. */
    handle->previous_length = 0;
    handle->first_decode = true;
    handle->p_first.page_start = get_file_offset(handle);

    return true;
}

/*************************************************************************/
/*************************************************************************/

