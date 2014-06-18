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

/* Vorbis header packet IDs. */
enum {
   VORBIS_packet_ident = 1,
   VORBIS_packet_comment = 3,
   VORBIS_packet_setup = 5,
};

/* Data structure mapping array values to array indices.  Used in
 * parse_floors() for sorting data. */
typedef struct ArrayElement {
    uint16_t value;
    uint8_t index;
} ArrayElement;

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

/**
 * float32_unpack:  Convert a 32-bit floating-point bit pattern read from
 * the bitstream to a native floating-point value.
 *
 * [Parameters]
 *     bits: Raw data read from the bitstream (32 bits).
 * [Return value]
 *     Corresponding floating-point value.
 */
static CONST_FUNCTION float float32_unpack(uint32_t bits)
{
    const float mantissa =  bits & 0x001FFFFF;
    const int   exponent = (bits & 0x7FE00000) >> 21;
    const bool  sign     = (bits & 0x80000000) != 0;
    return ldexpf(sign ? -mantissa : mantissa, exponent-788);
}

/*-----------------------------------------------------------------------*/

/**
 * uint32_compare:  Compare two 32-bit unsigned integer values.  Comparison
 * function for qsort().
 */
static int uint32_compare(const void *p, const void *q)
{
    const uint32_t x = *(uint32_t *)p;
    const uint32_t y = *(uint32_t *)q;
    return x < y ? -1 : x > y;
}

/*-----------------------------------------------------------------------*/

/**
 * array_element_compare:  Compare two ArrayElement values.  Comparison
 * function for qsort().
 */
static int array_element_compare(const void *p, const void *q)
{
    ArrayElement *a = (ArrayElement *)p;
    ArrayElement *b = (ArrayElement *)q;
    return a->value < b->value ? -1 : a->value > b->value;
}

/*-----------------------------------------------------------------------*/

/**
 * lookup1_values:  Return the length of the value index for a codebook VQ
 * lookup table of lookup type 1.  This is defined in the Vorbis
 * specification as "the greatest integer value for which [return_value]
 * to the power of [codebook_dimensions] is less than or equal to
 * [codebook_entries]".
 *
 * [Parameters]
 *     entries: Number of codebook entries.
 *     dimensions: Number of codebook dimensions.
 * [Return value]
 *     Value index length.
 */
static CONST_FUNCTION int lookup1_values(int entries, int dimensions)
{
    /* Conceptually, we want to calculate floor(entries ^ (1/dimensions)).
     * Generic exponentiation is a slow operation on some systems, so we
     * use log() and exp() instead: a^(1/b) == e^(log(a) * 1/b) */
    int retval = (int)floorf(expf(logf(entries) / dimensions));
    /* Rounding could conceivably cause us to end up with the wrong value,
     * so check retval+1 just in case. */
    if ((int)floorf(powf(retval+1, dimensions)) <= entries) {
        retval++;
        ASSERT((int)floorf(powf(retval+1, dimensions)) > entries);
    } else {
        ASSERT((int)floorf(powf(retval, dimensions)) <= entries);
    }
    return retval;
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

/*-----------------------------------------------------------------------*/

/**
 * add_codebook_entry:  Add an entry to the given codebook's codeword table.
 *
 * [Parameters]
 *     book: Codebook to operate on.
 *     code: Huffman code to add.
 *     length: Length of the code, in bits.
 *     symbol: Corresponding symbol.
 *     index: Index of this symbol in the codeword and value tables.  Used
 *         only for sparse codebooks.
 *     values: Table of values for each Huffman code (updated by this
 *         function).  Used only for sparse codebooks.
 */
static void add_codebook_entry(Codebook *book, uint32_t code, int length,
                               uint32_t symbol, int index, uint32_t *values)
{
    if (book->sparse) {
        book->codewords       [index] = code;
        book->codeword_lengths[index] = length;
        values                [index] = symbol;
    } else {
        book->codewords      [symbol] = code;
    }
}

/*-----------------------------------------------------------------------*/

/**
 * compute_codewords:  Generate the Huffman code tables for the given codebook.
 *
 * [Parameters]
 *     book: Codebook to operate on.
 *     lengths: Array of codeword lengths per symbol.
 *     count: Number of symbols.
 *     values: Array into which the symbol for each code will be written.
 *         Used only for sparse codebooks.
 * [Return value]
 *     True on success, false on error.
 */
static bool compute_codewords(Codebook *book, const uint8_t *lengths,
                              int count, uint32_t *values)
{
   int k,m=0;
   uint32_t available[32];

   memset(available, 0, sizeof(available));
   // find the first entry
   for (k=0; k < count; ++k) if (lengths[k] < NO_CODE) break;
   if (k == count) { ASSERT(book->sorted_entries == 0); return true; }
   // add to the list
   add_codebook_entry(book, 0, lengths[k], k, m++, values);
   // add all available leaves
   for (int i=1; i <= lengths[k]; ++i)
      available[i] = 1 << (32-i);
   // note that the above code treats the first case specially,
   // but it's really the same as the following code, so they
   // could probably be combined (except the initial code is 0,
   // and I use 0 in available[] to mean 'empty')
   for (int i=k+1; i < count; ++i) {
      uint32_t res;
      int z = lengths[i], y;
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
      add_codebook_entry(book, bit_reverse(res), lengths[i], i, m++, values);
      // propogate availability up the tree
      if (z != lengths[i]) {
         for (y=lengths[i]; y > z; --y) {
            ASSERT(available[y] == 0);
            available[y] = res + (1 << (32-y));
         }
      }
   }
   return true;
}

/*-----------------------------------------------------------------------*/

// accelerated huffman table allows fast O(1) match of all symbols
// of length <= STB_VORBIS_FAST_HUFFMAN_LENGTH
static void compute_accelerated_huffman(Codebook *book)
{
   for (int i=0; i < FAST_HUFFMAN_TABLE_SIZE; ++i)
      book->fast_huffman[i] = -1;

   int len = book->sparse ? book->sorted_entries : book->entries;
   #ifdef STB_VORBIS_FAST_HUFFMAN_SHORT
   if (len > 32767) len = 32767; // largest possible value we can encode!
   #endif
   for (int i=0; i < len; ++i) {
      if (book->codeword_lengths[i] <= STB_VORBIS_FAST_HUFFMAN_LENGTH) {
         uint32_t z = book->sparse ? bit_reverse(book->sorted_codewords[i]) : book->codewords[i];
         // set table entries for all bit combinations in the higher bits
         while (z < FAST_HUFFMAN_TABLE_SIZE) {
             book->fast_huffman[z] = i;
             z += 1 << book->codeword_lengths[i];
         }
      }
   }
}

/*-----------------------------------------------------------------------*/

static bool include_in_sort(Codebook *book, uint8_t len)
{
   if (book->sparse) { ASSERT(len != NO_CODE); return true; }
   if (len == NO_CODE) return false;
   if (len > STB_VORBIS_FAST_HUFFMAN_LENGTH) return true;
   return false;
}

/*-----------------------------------------------------------------------*/

// if the fast table above doesn't work, we want to binary
// search them... need to reverse the bits
static void compute_sorted_huffman(Codebook *book, uint8_t *lengths, uint32_t *values)
{
   // build a list of all the entries
   // OPTIMIZATION: don't include the short ones, since they'll be caught by FAST_HUFFMAN.
   // this is kind of a frivolous optimization--I don't see any performance improvement,
   // but it's like 4 extra lines of code, so.
   if (!book->sparse) {
      int k = 0;
      for (int i=0; i < book->entries; ++i)
         if (include_in_sort(book, lengths[i])) 
            book->sorted_codewords[k++] = bit_reverse(book->codewords[i]);
      ASSERT(k == book->sorted_entries);
   } else {
      for (int i=0; i < book->sorted_entries; ++i)
         book->sorted_codewords[i] = bit_reverse(book->codewords[i]);
   }

   qsort(book->sorted_codewords, book->sorted_entries, sizeof(book->sorted_codewords[0]), uint32_compare);
   book->sorted_codewords[book->sorted_entries] = 0xffffffff;

   int len = book->sparse ? book->sorted_entries : book->entries;
   // now we need to indicate how they correspond; we could either
   //   #1: sort a different data structure that says who they correspond to
   //   #2: for each sorted entry, search the original list to find who corresponds
   //   #3: for each original entry, find the sorted entry
   // #1 requires extra storage, #2 is slow, #3 can use binary search!
   for (int i=0; i < len; ++i) {
      int huff_len = book->sparse ? lengths[values[i]] : lengths[i];
      if (include_in_sort(book,huff_len)) {
         uint32_t code = bit_reverse(book->codewords[i]);
         int x=0, n=book->sorted_entries;
         while (n > 1) {
            // invariant: sc[x] <= code < sc[x+n]
            int m = x + n/2;
            if (book->sorted_codewords[m] <= code) {
               x = m;
               n -= n/2;
            } else {
               n /= 2;
            }
         }
         ASSERT(book->sorted_codewords[x] == code);
         if (book->sparse) {
            book->sorted_values[x] = values[i];
            book->codeword_lengths[x] = huff_len;
         } else {
            book->sorted_values[x] = i;
         }
      }
   }
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
               c->multiplicands = mem_alloc(handle->opaque, sizeof(c->multiplicands[0]) * c->sorted_entries * c->dimensions);
            } else
               c->multiplicands = mem_alloc(handle->opaque, sizeof(c->multiplicands[0]) * c->entries        * c->dimensions);
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
            c->multiplicands = mem_alloc(handle->opaque, sizeof(c->multiplicands[0]) * c->lookup_values);
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

    handle->floor_count = get_bits(handle, 6) + 1;
    handle->floor_config = mem_alloc(
        handle->opaque, handle->floor_count * sizeof(*handle->floor_config));
    if (!handle->floor_config) {
        return error(handle, VORBIS_outofmem);
    }

    for (int i = 0; i < handle->floor_count; i++) {
        handle->floor_types[i] = get_bits(handle, 16);

        if (handle->floor_types[i] == 0) {
            Floor0 *floor = &handle->floor_config[i].floor0;
            floor->order = get_bits(handle, 8);
            floor->rate = get_bits(handle, 16);
            floor->bark_map_size = get_bits(handle, 16);
            floor->amplitude_bits = get_bits(handle, 6);
            floor->amplitude_offset = get_bits(handle, 8);
            floor->number_of_books = get_bits(handle, 4) + 1;
            for (int j = 0; j < floor->number_of_books; j++) {
                floor->book_list[j] = get_bits(handle, 8);
                if (floor->book_list[j] >= handle->codebook_count) {
                    return error(handle, VORBIS_invalid_setup);
                }
            }
            // FIXME: floor 0 not yet implemented
            return error(handle, VORBIS_feature_not_supported);

        } else if (handle->floor_types[i] == 1) {
            Floor1 *floor = &handle->floor_config[i].floor1;
            floor->partitions = get_bits(handle, 5);
            int max_class = -1; 
            for (int j = 0; j < floor->partitions; j++) {
                floor->partition_class_list[j] = get_bits(handle, 4);
                if (floor->partition_class_list[j] > max_class) {
                    max_class = floor->partition_class_list[j];
                }
            }
            for (int j = 0; j <= max_class; j++) {
                floor->class_dimensions[j] = get_bits(handle, 3) + 1;
                floor->class_subclasses[j] = get_bits(handle, 2);
                if (floor->class_subclasses[j]) {
                    floor->class_masterbooks[j] = get_bits(handle, 8);
                    if (floor->class_masterbooks[j] >= handle->codebook_count) {
                        return error(handle, VORBIS_invalid_setup);
                    }
                }
                for (int k = 0; k < (1 << floor->class_subclasses[j]); k++) {
                    floor->subclass_books[j][k] = get_bits(handle, 8) - 1;
                    if (floor->subclass_books[j][k] >= handle->codebook_count) {
                        return error(handle, VORBIS_invalid_setup);
                    }
                }
            }
            floor->floor1_multiplier = get_bits(handle, 2) + 1;
            floor->rangebits = get_bits(handle, 4);
            floor->X_list[0] = 0;
            floor->X_list[1] = 1 << floor->rangebits;
            floor->values = 2;
            for (int j = 0; j < floor->partitions; j++) {
                int c = floor->partition_class_list[j];
                for (int k = 0; k < floor->class_dimensions[c]; k++) {
                    if (floor->values >= 65) {
                        return error(handle, VORBIS_invalid_setup);
                    }
                    floor->X_list[floor->values] =
                        get_bits(handle, floor->rangebits);
                    floor->values++;
                }
            }

            /* We'll need to process the floor curve in X value order, so
             * precompute a sorted list. */
            ArrayElement elements[65];
            for (int j = 0; j < floor->values; j++) {
                elements[j].value = floor->X_list[j];
                elements[j].index = j;
            }
            qsort(elements, floor->values, sizeof(*elements),
                  array_element_compare);
            for (int j = 0; j < floor->values; j++) {
                floor->sorted_order[j] = elements[j].index;
            }

            /* Find the low and high neighbors for each element of X_list,
             * following the Vorbis definition (which only looks at the
             * preceding elements of the list). */
            for (int j = 2; j < floor->values; j++) {
                const unsigned int X_j = floor->X_list[j];
                int low_index = 0, high_index = 1;
                unsigned int low = floor->X_list[0], high = floor->X_list[1];
                for (int k = 2; k < j; k++) {
                    if (floor->X_list[k] > low && floor->X_list[k] < X_j) {
                        low = floor->X_list[k];
                        low_index = k;
                    }
                    if (floor->X_list[k] < high && floor->X_list[k] > X_j) {
                        high = floor->X_list[k];
                        high_index = k;
                    }
                }
                floor->neighbors[j].low = low_index;
                floor->neighbors[j].high = high_index;
            }

            /* Remember the longest X_list length we've seen for allocating
             * the final_Y buffer later. */
            if (floor->values > longest_floorlist) {
                longest_floorlist = floor->values;
            }

        } else {  // handle->floor_types[i] > 1
            return error(handle, VORBIS_invalid_setup);
        }
    }

    handle->final_Y = alloc_channel_array(
        handle->opaque, handle->channels, sizeof(int16_t) * longest_floorlist);
    if (!handle->final_Y) {
        return error(handle, VORBIS_outofmem);
    }

    return true;
}

/*-----------------------------------------------------------------------*/

/**
 * parse_residues:  Parse residue data from the setup header.
 *
 * [Parameters]
 *     handle: Stream handle.
 */
static bool parse_residues(stb_vorbis *handle)
{
    /* Maximum number of temporary array entries needed by any residue
     * configuration. */
    int residue_max_temp = 0;

    handle->residue_count = get_bits(handle, 6) + 1;
    handle->residue_config = mem_alloc(
        handle->opaque,
        handle->residue_count * sizeof(*handle->residue_config));
    if (!handle->residue_config) {
        return error(handle, VORBIS_outofmem);
    }
    memset(handle->residue_config, 0,
           handle->residue_count * sizeof(*handle->residue_config));

    for (int i = 0; i < handle->residue_count; i++) {
        Residue *r = &handle->residue_config[i];
        handle->residue_types[i] = get_bits(handle, 16);
        if (handle->residue_types[i] > 2) {
            return error(handle, VORBIS_invalid_setup);
        }

        r->begin = get_bits(handle, 24);
        r->end = get_bits(handle, 24);
        r->part_size = get_bits(handle, 24) + 1;
        r->classifications = get_bits(handle, 6) + 1;
        r->classbook = get_bits(handle, 8);

        uint8_t residue_cascade[64];
        for (int j = 0; j < r->classifications; j++) {
            const uint8_t low_bits = get_bits(handle,3);
            const uint8_t high_bits = (get_bits(handle, 1)
                                       ? get_bits(handle, 5) : 0);
            residue_cascade[j] = high_bits<<3 | low_bits;
        }

        r->residue_books = mem_alloc(
            handle->opaque, sizeof(*(r->residue_books)) * r->classifications);
        if (!r->residue_books) {
            return error(handle, VORBIS_outofmem);
        }
        for (int j = 0; j < r->classifications; j++) {
            for (int k = 0; k < 8; k++) {
                if (residue_cascade[j] & (1 << k)) {
                    r->residue_books[j][k] = get_bits(handle, 8);
                    if (r->residue_books[j][k] >= handle->codebook_count) {
                        return error(handle, VORBIS_invalid_setup);
                    }
                } else {
                    r->residue_books[j][k] = -1;
                }
            }
        }

        const int classwords = handle->codebooks[r->classbook].dimensions;
        r->classdata = mem_alloc(
            handle->opaque,
            sizeof(*r->classdata) * handle->codebooks[r->classbook].entries);
        if (!r->classdata) {
            return error(handle, VORBIS_outofmem);
        }
        r->classdata[0] = mem_alloc(
            handle->opaque, (handle->codebooks[r->classbook].entries
                             * sizeof(**r->classdata) * classwords));
        if (!r->classdata[0]) {
            return error(handle, VORBIS_outofmem);
        }
        for (int j = 0; j < handle->codebooks[r->classbook].entries; j++) {
            r->classdata[j] = r->classdata[0] + (j * classwords);
            int temp = j;
            for (int k = classwords-1; k >= 0; k--) {
                r->classdata[j][k] = temp % r->classifications;
                temp /= r->classifications;
            }
        }

        /* Calculate how many temporary array entries we need, and update
         * the maximum across all residue configurations. */
        const int temp_required = (r->end - r->begin) / r->part_size;
        if (temp_required > residue_max_temp) {
            residue_max_temp = temp_required;
        }
   }

#ifdef STB_VORBIS_DIVIDES_IN_RESIDUE
    handle->classifications = alloc_channel_array(
        handle->opaque, handle->channels,
        residue_max_temp * sizeof(**handle->classifications));
    if (!handle->classifications) {
        return error(handle, VORBIS_outofmem);
    }
#else
    handle->part_classdata = alloc_channel_array(
        handle->opaque, handle->channels,
        residue_max_temp * sizeof(**handle->part_classdata));
    if (!handle->part_classdata) {
        return error(handle, VORBIS_outofmem);
    }
#endif

   return true;
}

/*-----------------------------------------------------------------------*/

/**
 * parse_mappings:  Parse mapping data from the setup header.
 *
 * [Parameters]
 *     handle: Stream handle.
 */
static bool parse_mappings(stb_vorbis *handle)
{
    handle->mapping_count = get_bits(handle, 6) + 1;
    handle->mapping = mem_alloc(
        handle->opaque, handle->mapping_count * sizeof(*handle->mapping));
    if (!handle->mapping) {
        return error(handle, VORBIS_outofmem);
    }
    memset(handle->mapping, 0, handle->mapping_count * sizeof(*handle->mapping));
    handle->mapping[0].mux = mem_alloc(
        handle->opaque, (handle->mapping_count * handle->channels
                         * sizeof(*(handle->mapping[0].mux))));
    if (!handle->mapping[0].mux) {
        return error(handle, VORBIS_outofmem);
    }
    for (int i = 1; i < handle->mapping_count; i++) {
        handle->mapping[i].mux =
            handle->mapping[0].mux + (i * handle->channels);
    }

    for (int i = 0; i < handle->mapping_count; i++) {
        Mapping *m = &handle->mapping[i];
        int mapping_type = get_bits(handle, 16);
        if (mapping_type != 0) {
            return error(handle, VORBIS_invalid_setup);
        }
        if (get_bits(handle, 1)) {
            m->submaps = get_bits(handle, 4);
        } else {
            m->submaps = 1;
        }

        if (get_bits(handle, 1)) {
            m->coupling_steps = get_bits(handle, 8) + 1;
            m->coupling = mem_alloc(
                handle->opaque, m->coupling_steps * sizeof(*m->coupling));
            if (!m->coupling) {
                return error(handle, VORBIS_outofmem);
            }
            for (int j = 0; j < m->coupling_steps; j++) {
                m->coupling[j].magnitude = get_bits(handle, ilog(handle->channels - 1));
                m->coupling[j].angle = get_bits(handle, ilog(handle->channels - 1));
                if (m->coupling[j].magnitude >= handle->channels
                 || m->coupling[j].angle >= handle->channels
                 || m->coupling[j].magnitude == m->coupling[j].angle) {
                    return error(handle, VORBIS_invalid_setup);
                }
            }
        } else {
            m->coupling_steps = 0;
            m->coupling = NULL;
        }

        if (get_bits(handle, 2)) {  // Reserved field, must be zero.
            return error(handle, VORBIS_invalid_setup);
        }

        if (m->submaps > 1) {
            for (int j = 0; j < handle->channels; j++) {
                m->mux[j] = get_bits(handle, 4);
                if (m->mux[j] >= m->submaps) {
                    return error(handle, VORBIS_invalid_setup);
                }
            }
        } else {
            /* The specification doesn't explicitly say what to store in
             * the mux array for this case, but since the values are used
             * to index the submap list, it should be safe to assume that
             * all values are set to zero (the only valid submap index). */
            for (int j = 0; j < handle->channels; j++) {
                m->mux[j] = 0;
            }
        }

        for (int j = 0; j < m->submaps; j++) {
            get_bits(handle, 8);  // Unused time configuration placeholder.
            m->submap_floor[j] = get_bits(handle, 8);
            m->submap_residue[j] = get_bits(handle, 8);
            if (m->submap_floor[j] >= handle->floor_count
             || m->submap_residue[j] >= handle->residue_count) {
                return error(handle, VORBIS_invalid_setup);
            }
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
    handle->mode_count = get_bits(handle, 6) + 1;
    for (int i=0; i < handle->mode_count; i++) {
        Mode *m = &handle->mode_config[i];
        m->blockflag = get_bits(handle, 1);
        m->windowtype = get_bits(handle, 16);
        m->transformtype = get_bits(handle, 16);
        m->mapping = get_bits(handle, 8);
        if (m->windowtype != 0
         || m->transformtype != 0
         || m->mapping >= handle->mapping_count) {
            return error(handle, VORBIS_invalid_setup);
        }
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
    if (handle->valid_bits < 0) {
        return error(handle, VORBIS_unexpected_eof);
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

