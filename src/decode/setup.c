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

// FIXME: note from spec -- do we handle this properly? do we have a test?
// "Take special care that a codebook with a single used entry is handled
// properly; it consists of a single codework of zero bits and 'reading' a
// value out of such a codebook always returns the single used value and
// sinks zero bits."

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
 * lookup1_values:  Return the length of the value index for a codebook
 * vector lookup table of lookup type 1.  This is defined in the Vorbis
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
 * compute_codewords:  Generate the Huffman code tables for the given codebook.
 *
 * [Parameters]
 *     book: Codebook to operate on.
 *     lengths: Array of codeword lengths per symbol.
 *     count: Number of symbols.
 *     values: Array into which the symbol for each code will be written.
 *         Used only for sparse codebooks.
 * [Return value]
 *     True on success, false on error (underspecified or overspecified tree).
 */
static bool compute_codewords(Codebook *book, const uint8_t *lengths,
                              int count, uint32_t *values)
{
    /* Current index in the codeword list. */
    int index = 0;
    /* Next code available at each codeword length. */
    uint32_t available[32];
    memset(available, 0, sizeof(available));

    for (int symbol = 0; symbol < count; symbol++) {
        if (lengths[symbol] == NO_CODE) {
            continue;
        }

        /* Determine the code for this value, which will always be the
         * lowest available leaf.  (stb_vorbis note: "this property, and
         * the fact we can never have more than one free leaf at a given
         * level, isn't totally trivial to prove, but it seems true and
         * the assert never fires, so!") */
        uint32_t code;
        int bitpos;  // Position of the bit toggled for this code, plus one.
        if (index == 0) {
            /* This is the first value, so it always gets an all-zero code. */
            code = 0;
            bitpos = 0;
        } else {
            bitpos = lengths[symbol];
            while (bitpos > 0 && !available[bitpos]) {
                bitpos--;
            }
            if (bitpos == 0) {
                return false;  // Overspecified tree.
            }
            code = available[bitpos];
            available[bitpos] = 0;
        }

        /* Add the code and corresponding symbol to the tree. */
        if (book->sparse) {
            book->codewords       [index] = bit_reverse(code);
            book->codeword_lengths[index] = lengths[symbol];
            values                [index] = symbol;
        } else {
            book->codewords      [symbol] = bit_reverse(code);
        }
        index++;

        /* If the code's final bit isn't a 1, propagate availability down
         * the tree. */
        for (int i = bitpos+1; i <= lengths[symbol]; i++) {
            ASSERT(!available[i]);
            available[i] = code | (1 << (32-i));
        }
    }

    for (int i = 0; i < 32; i++) {
        if (available[i]) {
            return false;  // Underspecified tree.
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
    handle->codebook_count = get_bits(handle, 8) + 1;
    handle->codebooks = mem_alloc(
        handle->opaque, sizeof(*handle->codebooks) * handle->codebook_count);
    if (!handle->codebooks) {
        return error(handle, VORBIS_outofmem);
    }
    memset(handle->codebooks, 0,
           sizeof(*handle->codebooks) * handle->codebook_count);

    for (int i = 0; i < handle->codebook_count; i++) {
        Codebook *book = &handle->codebooks[i];

        /* Verify the codebook sync pattern and read basic parameters. */
        if (get_bits(handle, 24) != 0x564342) {
            return error(handle, VORBIS_invalid_setup);
        }
        book->dimensions = get_bits(handle, 16);
        book->entries = get_bits(handle, 24);
        const bool ordered = get_bits(handle, 1);

        /* Read in the code lengths for each entry. */
        uint8_t *lengths = mem_alloc(handle->opaque, book->entries);
        if (!lengths) {
            return error(handle, VORBIS_outofmem);
        }
        int32_t code_count = 0;  // Only used for non-ordered codebooks.
        if (ordered) {
            book->sparse = false;
            int32_t current_entry = 0;
            int current_length = get_bits(handle, 5) + 1;
            while (current_entry < book->entries) {
                const int32_t limit = book->entries - current_entry;
                const int32_t count = get_bits(handle, ilog(limit));
                if (current_entry + count > book->entries) {
                    mem_free(handle->opaque, lengths);
                    return error(handle, VORBIS_invalid_setup);
                }
                memset(lengths + current_entry, current_length, count);
                current_entry += count;
                current_length++;
            }
        } else {
            book->sparse = get_bits(handle, 1);
            for (int32_t j = 0; j < book->entries; j++) {
                const bool present = book->sparse ? get_bits(handle,1) : true;
                if (present) {
                    lengths[j] = get_bits(handle, 5) + 1;
                    code_count++;
                } else {
                    lengths[j] = NO_CODE;
                }
            }
            /* If the codebook is marked as sparse but is more than 25% full,
             * converting it to non-sparse will generally give us a better
             * space/time tradeoff. */
            if (book->sparse && code_count >= book->entries/4) {
                book->sparse = false;
            }
        }

        /* Count the number of used entries in the codebook, so we don't
         * waste space with unused entries. */
        if (book->sparse) {
            book->sorted_entries = code_count;
        } else {
            book->sorted_entries = 0;
#ifndef STB_VORBIS_NO_HUFFMAN_BINARY_SEARCH
            for (int32_t j = 0; j < book->entries; j++) {
                if (lengths[j] > STB_VORBIS_FAST_HUFFMAN_LENGTH
                 && lengths[j] != NO_CODE) {
                    book->sorted_entries++;
                }
            }
#endif
        }

        /* Allocate and generate the codeword tables. */
        uint32_t *values = NULL;  // Used only for sparse codebooks.
        if (book->sparse) {
            if (book->sorted_entries > 0) {
                book->codeword_lengths = mem_alloc(
                    handle->opaque, book->sorted_entries);
                book->codewords = mem_alloc(
                    handle->opaque,
                    sizeof(*book->codewords) * book->sorted_entries);
                values = mem_alloc(
                    handle->opaque, sizeof(*values) * book->sorted_entries);
                if (!book->codeword_lengths || !book->codewords || !values) {
                    mem_free(handle->opaque, lengths);
                    return error(handle, VORBIS_outofmem);
                }
            }
        } else {
            book->codeword_lengths = lengths;
            book->codewords = mem_alloc(
                handle->opaque, sizeof(*book->codewords) * book->entries);
            if (!book->codewords) {
                return error(handle, VORBIS_outofmem);
            }
        }
        if (!compute_codewords(book, lengths, book->entries, values)) {
            if (book->sparse) {
                mem_free(handle->opaque, values);
                mem_free(handle->opaque, lengths);
            }
            return error(handle, VORBIS_invalid_setup);
        }

        /* Build the code lookup tables used in decoding. */
        if (book->sorted_entries > 0) {
            /* Include an extra slot at the end for a sentinel. */
            book->sorted_codewords = mem_alloc(
                handle->opaque,
                sizeof(*book->sorted_codewords) * (book->sorted_entries+1));
            /* Include an extra slot before the beginning so we can access
             * sorted_values[-1] instead of needing a guard on the index. */
            book->sorted_values = mem_alloc(
                handle->opaque,
                sizeof(*book->sorted_values) * (book->sorted_entries+1));
            if (!book->sorted_codewords || !book->sorted_values) {
                if (book->sparse) {
                    mem_free(handle->opaque, values);
                    mem_free(handle->opaque, lengths);
                }
                return error(handle, VORBIS_outofmem);
            }
            book->sorted_codewords[book->sorted_entries] = 0xFFFFFFFFU;
            book->sorted_values++;
            book->sorted_values[-1] = -1;
            compute_sorted_huffman(book, lengths, values);
        }
        compute_accelerated_huffman(book);

        /* For sparse codebooks, we've now compressed the data into our
         * target arrays so we no longer need the original buffers. */
        if (book->sparse) {
            mem_free(handle->opaque, lengths);
            mem_free(handle->opaque, values);
            mem_free(handle->opaque, book->codewords);
            book->codewords = NULL;
        }

        /* Read the vector lookup table. */
        book->lookup_type = get_bits(handle, 4);
        if (book->lookup_type == 0) {
            /* No lookup data to be read. */
        } else if (book->lookup_type <= 2) {
            book->minimum_value = float32_unpack(get_bits(handle, 32));
            book->delta_value = float32_unpack(get_bits(handle, 32));
            book->value_bits = get_bits(handle, 4)+1;
            book->sequence_p = get_bits(handle, 1);
            // FIXME: need a test file with sequence_p set
            if (book->lookup_type == 1) {
                book->lookup_values =
                    lookup1_values(book->entries, book->dimensions);
            } else {
                book->lookup_values = book->entries * book->dimensions;
            }

            uint16_t *mults = (uint16_t *) mem_alloc(handle->opaque, sizeof(mults[0]) * book->lookup_values);
            if (!mults) {
                return error(handle, VORBIS_outofmem);
            }
            for (int32_t j = 0; j < book->lookup_values; j++) {
                mults[j] = get_bits(handle, book->value_bits);
            }

            /* Precompute and/or pre-expand multiplicands depending on
             * build options. */
            enum {EXPAND_LOOKUP1, COPY, NONE} precompute_type = COPY;
#ifndef STB_VORBIS_DIVIDES_IN_CODEBOOK
            if (book->lookup_type == 1) {
                if (book->sparse && book->sorted_entries == 0) {
                    precompute_type = NONE;
                } else {
                    precompute_type = EXPAND_LOOKUP1;
                }
            }
#endif
            if (precompute_type == EXPAND_LOOKUP1) {
                /* Pre-expand multiplicands to avoid a divide in an inner
                 * decode loop. */
                if (book->sparse) {
                    book->multiplicands = mem_alloc(
                        handle->opaque,
                        (sizeof(*book->multiplicands)
                         * book->sorted_entries * book->dimensions));
                } else {
                    book->multiplicands = mem_alloc(
                        handle->opaque,
                        (sizeof(book->multiplicands[0])
                         * book->entries * book->dimensions));
                }
                if (!book->multiplicands) {
                    mem_free(handle->opaque, mults);
                    return error(handle, VORBIS_outofmem);
                }
                const int32_t len =
                    book->sparse ? book->sorted_entries : book->entries;
                for (int32_t j = 0; j < len; j++) {
                    const uint32_t index =
                        book->sparse ? book->sorted_values[j] : (uint32_t)j;
                    int divisor = 1;
                    for (int k = 0; k < book->dimensions; k++) {
                        const int offset =
                            (index / divisor) % book->lookup_values;
                        book->multiplicands[j*book->dimensions + k] =
#ifdef STB_VORBIS_CODEBOOK_FLOATS
                            mults[offset]*book->delta_value + book->minimum_value;
                            // FIXME: stb_vorbis note --
                            // in this case (and this case only) we could pre-expand book->sequence_p,
                            // and throw away the decode logic for it; have to ALSO do
                            // it in the case below, but it can only be done if
                            //    STB_VORBIS_CODEBOOK_FLOATS
                            //   !STB_VORBIS_DIVIDES_IN_CODEBOOK
#else
                            mults[offset];
#endif
                            divisor *= book->lookup_values;
                    }
                }
                mem_free(handle->opaque, mults);
                book->lookup_type = 2;
            } else if (precompute_type == COPY) {
                book->multiplicands = mem_alloc(
                    handle->opaque,
                    sizeof(*book->multiplicands) * book->lookup_values);
                if (!book->multiplicands) {
                    mem_free(handle->opaque, mults);
                    return error(handle, VORBIS_outofmem);
                }
#ifndef STB_VORBIS_CODEBOOK_FLOATS
                memcpy(book->multiplicands, mults,
                       sizeof(*book->multiplicands) * book->lookup_values);
#else
                for (int j = 0; j < (int) book->lookup_values; j++) {
                    book->multiplicands[j] =
                        mults[j] * book->delta_value + book->minimum_value;
                }
#endif
                mem_free(handle->opaque, mults);
            }  // else precompute_type == NONE, so we don't do anything.
        } else {  // book->lookup_type > 2
            return error(handle, VORBIS_invalid_setup);
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

