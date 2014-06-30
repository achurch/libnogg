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

#include <math.h>
#include <stdlib.h>
#include <string.h>

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
/******************* Codebook decoding: scalar context *******************/
/*************************************************************************/

/**
 * codebook_decode_scalar_raw:  Read a Huffman code from the packet and
 * decode it in scalar context using the given codebook, returning the
 * symbol for non-sparse codebooks and the sorted_values[] index for sparse
 * codebooks.
 *
 * [Parameters]
 *     handle: Stream handle.
 *     book: Codebook to use.
 * [Return value]
 *     Value read, or -1 if the end of the packet is reached.
 */
static int32_t codebook_decode_scalar_raw(stb_vorbis *handle,
                                          const Codebook *book)
{
    /* First try the O(1) table.  We only fill the bit accumulator if we
     * don't have enough bits for the fast table, to avoid overhead from
     * repeatedly reading single bytes from the packet. */
    if (handle->valid_bits < handle->fast_huffman_length) {
        fill_bits(handle);
    }
    const int fast_code =
        book->fast_huffman[handle->acc & handle->fast_huffman_mask];
    if (fast_code >= 0) {
        const int bits = book->codeword_lengths[fast_code];
        handle->acc >>= bits;
        handle->valid_bits -= bits;
        if (UNLIKELY(handle->valid_bits < 0)) {
            handle->valid_bits = 0;
            return -1;
        }
        return fast_code;
    }

    /* Fill the bit accumulator far enough to read any valid code. */
    if (handle->valid_bits < 24) {
        fill_bits(handle);
    }

    /* Find the code using binary search if we can, linear search otherwise. */
    if (book->sorted_codewords) {
        const uint32_t code = bit_reverse(handle->acc);
        int32_t low = 0, high = book->sorted_entries;
        /* Invariant: sorted_codewords[low] <= code < sorted_codewords[high] */
        while (low+1 < high) {
            const int32_t mid = (low + high) / 2;
            if (book->sorted_codewords[mid] <= code) {
                low = mid;
            } else {
                high = mid;
            }
        }
        const int32_t result =
            book->sparse ? low : (int32_t)book->sorted_values[low];
        const int len = book->codeword_lengths[result];
        handle->acc >>= len;
        handle->valid_bits -= len;
        if (UNLIKELY(handle->valid_bits < 0)) {
            handle->valid_bits = 0;
            return -1;
        }
        return result;
    } else {
        for (int i = 0; i < book->entries; i++) {
            if (book->codeword_lengths[i] == NO_CODE) {
                continue;
            }
            if (book->codewords[i] ==
                (handle->acc & ((UINT32_C(1) << book->codeword_lengths[i])-1)))
            {
                handle->acc >>= book->codeword_lengths[i];
                handle->valid_bits -= book->codeword_lengths[i];
                if (UNLIKELY(handle->valid_bits < 0)) {
                    handle->valid_bits = 0;
                    return -1;
                }
                return i;
            }
        }
    }

    /* We should never get here because we ensure that all Huffman trees
     * are completely specified during setup. */
    ASSERT(!"impossible");
    handle->valid_bits = 0;
    return -1;
}

/*-----------------------------------------------------------------------*/

/**
 * codebook_decode_scalar:  Read a Huffman code from the packet and decode
 * it in scalar context using the given codebook.
 *
 * [Parameters]
 *     handle: Stream handle.
 *     book: Codebook to use.
 * [Return value]
 *     Symbol read, or -1 if the end of the packet is reached.
 */
static int32_t codebook_decode_scalar(stb_vorbis *handle, const Codebook *book)
{
    const int32_t value = codebook_decode_scalar_raw(handle, book);
    return book->sparse ? (int32_t)book->sorted_values[value] : value;
}

/*************************************************************************/
/********************* Codebook decoding: VQ context *********************/
/*************************************************************************/

/**
 * codebook_decode_scalar_for_vq:  Read a Huffman code from the packet and
 * return a value appropriate for decoding the code in VQ context.  Helper
 * function for codebook_decode() and related functions.
 *
 * [Parameters]
 *     handle: Stream handle.
 *     book: Codebook to use.
 * [Return value]
 *     Symbol or value read (depending on library configuration and
 *     codebook type), or -1 if the end of the packet is reached.
 */
static inline int32_t codebook_decode_scalar_for_vq(stb_vorbis *handle,
                                                    const Codebook *book)
{
    if (handle->divides_in_codebook) {
        return codebook_decode_scalar(handle, book);
    } else {
        return codebook_decode_scalar_raw(handle, book);
    }
}

/*-----------------------------------------------------------------------*/

/**
 * codebook_element:  Return the "offset"th value of the given codebook's
 * VQ element table.  Helper function for codebook_decode() and related
 * functions.
 *
 * [Parameters]
 *     book: Codebook to use.
 *     offset: Index of multiplicand to return.
 * [Return value]
 *     Element value.
 */
static inline float codebook_element(const Codebook *book, int32_t offset)
{
    return book->multiplicands[offset];
}

/*-----------------------------------------------------------------------*/

/**
 * codebook_element_fast:  Return the "offset"th value of the given
 * codebook's VQ element table minus the value of codebook_element_base(book).
 * Helper function for codebook_decode() and related functions; used in
 * place of codebook_element() to save an addition in certain cases.
 *
 * [Parameters]
 *     book: Codebook to use.
 *     offset: Index of multiplicand to return.
 * [Return value]
 *     Element value minus the value of codebook_element_base(book).
 */
static inline float codebook_element_fast(const Codebook *book, int32_t offset)
{
    return book->multiplicands[offset];
}

/*-----------------------------------------------------------------------*/

/**
 * codebook_element_base:  Return the base value for codebook_element_fast()
 * for the given codebook.  Helper function for codebook_decode() and
 * related functions.
 *
 * [Parameters]
 *     book: Codebook to use.
 * [Return value]
 *     Base value for codebook_element_fast().
 */
static inline float codebook_element_base(const Codebook *book)
{
    return 0;
}

/*-----------------------------------------------------------------------*/

/**
 * codebook_decode:  Read a Huffman code from the packet and decode it in
 * VQ context using the given codebook.  Decoded vector components are
 * added componentwise to the values in the output vector.
 *
 * If an error occurs, the current packet is flushed.
 *
 * [Parameters]
 *     handle: Stream handle.
 *     book: Codebook to use.
 *     output: Output vector.
 *     len: Number of elements to read.  Automatically capped at
 *         book->dimensions.
 * [Return value]
 *     True on success, false on error or end-of-packet.
 */
static bool codebook_decode(stb_vorbis *handle, const Codebook *book,
                            float *output, int len)
{
    if (UNLIKELY(book->lookup_type == 0)) {
        /* Lookup type 0 is only valid in a scalar context. */
        flush_packet(handle);
        return error(handle, VORBIS_invalid_packet);
    }
    if (len > book->dimensions) {
        len = book->dimensions;
    }

    const int32_t code = codebook_decode_scalar_for_vq(handle, book);
    if (UNLIKELY(code < 0)) {
        return false;
    }

    if (book->lookup_type == 1) {
        int div = 1;
        if (book->sequence_p) {
            float last = codebook_element_base(book);
            for (int i = 0; i < len; i++) {
                const int32_t offset = (code / div) % book->lookup_values;
                const float val = codebook_element_fast(book, offset) + last;
                output[i] += val;
                last = val;
                div *= book->lookup_values;
            }
        } else {
            for (int i = 0; i < len; i++) {
                const int32_t offset = (code / div) % book->lookup_values;
                output[i] += codebook_element(book, offset);
                div *= book->lookup_values;
            }
        }
    } else {
        const int32_t offset = code * book->dimensions;
        for (int i = 0; i < len; i++) {
            output[i] += codebook_element(book, offset+i);
        }
    }

    return true;
}

/*-----------------------------------------------------------------------*/

/**
 * codebook_decode_step:  Read a Huffman code from the packet and decode it
 * in VQ context using the given codebook.  Decoded vector components are
 * added componentwise to values in the output vector at intervals of "step"
 * array elements.  (codebook_decode() is effectively a specialization of
 * this function with step==1.)
 *
 * If an error occurs, the current packet is flushed.
 *
 * [Parameters]
 *     handle: Stream handle.
 *     book: Codebook to use.
 *     output: Output vector.
 *     len: Number of elements to read.  Automatically capped at
 *         book->dimensions.
 *     step: Interval between consecutive output elements: the second
 *         element is output[step], the third is output[2*step], etc.
 * [Return value]
 *     True on success, false on error or end-of-packet.
 */
static bool codebook_decode_step(stb_vorbis *handle, const Codebook *book,
                                 float *output, int len, int step)
{
    if (UNLIKELY(book->lookup_type == 0)) {
        flush_packet(handle);
        return error(handle, VORBIS_invalid_packet);
    }
    if (len > book->dimensions) {
        len = book->dimensions;
    }

    const int32_t code = codebook_decode_scalar_for_vq(handle, book);
    if (UNLIKELY(code < 0)) {
        return false;
    }

    if (book->lookup_type == 1) {
        int div = 1;
        if (book->sequence_p) {
            float last = codebook_element_base(book);
            for (int i = 0; i < len; i++) {
                const int32_t offset = (code / div) % book->lookup_values;
                const float val = codebook_element_fast(book, offset) + last;
                output[i*step] += val;
                last = val;
                div *= book->lookup_values;
            }
        } else {
            for (int i = 0; i < len; i++) {
                const int32_t offset = (code / div) % book->lookup_values;
                output[i*step] += codebook_element(book, offset);
                div *= book->lookup_values;
            }
        }
    } else {
        const int32_t offset = code * book->dimensions;
        for (int i = 0; i < len; i++) {
            output[i*step] += codebook_element(book, offset+i);
        }
    }

    return true;
}

/*-----------------------------------------------------------------------*/

/**
 * codebook_decode_deinterleave_repeat:  Read Huffman codes from the packet
 * and decode them in VQ context using the given codebook, deinterleaving
 * across "ch" output channels, and repeat until total_decode vector
 * elements have been decoded.  Decoded vector components are added
 * componentwise to the values in the output vectors.
 *
 * If an error occurs, the current packet is flushed.
 *
 * [Parameters]
 *     handle: Stream handle.
 *     book: Codebook to use.
 *     outputs: Output vectors for each channel.
 *     ch: Number of channels.
 *     n: Length of each output vector.
 *     total_offset: Offset within the interleaved output vector at which
 *         to start writing output values.
 *     total_decode: Total number of elements to read.
 * [Return value]
 *     True on success, false on error or end-of-packet.
 */
static bool codebook_decode_deinterleave_repeat(
    stb_vorbis *handle, const Codebook *book, float **outputs, int ch, int n,
    int32_t total_offset, int32_t total_decode)
{
    if (UNLIKELY(book->lookup_type == 0)) {
        flush_packet(handle);
        return error(handle, VORBIS_invalid_packet);
    }
    if (total_decode > ch*n - total_offset) {
        total_decode = ch*n - total_offset;
    }
    int c_inter = total_offset % ch;
    int p_inter = total_offset / ch;
    int len = book->dimensions;

    while (total_decode > 0) {
        /* Make sure we don't run off the end of the output vectors. */
        if (len > total_decode) {
            len = total_decode;
        }

        const int32_t code = codebook_decode_scalar_for_vq(handle, book);
        if (UNLIKELY(code < 0)) {
            return false;
        }

        if (book->lookup_type == 1) {
            int div = 1;
            if (book->sequence_p) {
                float last = codebook_element_base(book);
                for (int i = 0; i < len; i++) {
                    const int32_t offset = (code / div) % book->lookup_values;
                    const float val =
                        codebook_element_fast(book, offset) + last;
                    outputs[c_inter][p_inter] += val;
                    if (++c_inter == ch) {
                        c_inter = 0;
                        p_inter++;
                    }
                    last = val;
                    div *= book->lookup_values;
                }
            } else {
                for (int i = 0; i < len; i++) {
                    const int32_t offset = (code / div) % book->lookup_values;
                    outputs[c_inter][p_inter] +=
                        codebook_element(book, offset);
                    if (++c_inter == ch) {
                        c_inter = 0;
                        p_inter++;
                    }
                    div *= book->lookup_values;
                }
            }
        } else {  // book->lookup_type == 2
            const int32_t offset = code * book->dimensions;
            for (int i = 0; i < len; i++) {
                outputs[c_inter][p_inter] += codebook_element(book, offset+i);
                if (++c_inter == ch) {
                    c_inter = 0;
                    p_inter++;
                }
            }
        }

        total_decode -= len;
    }

    return true;
}

/*-----------------------------------------------------------------------*/

/**
 * codebook_decode_deinterleave_repeat_2:  Read Huffman codes from the
 * packet and decode them in VQ context using the given codebook,
 * deinterleaving across 2 output channels.  This function is a
 * specialization of codebook_decode_deinterleave_repeat() for ch==2 and
 * VORBIS_OPTION_DIVIDES_IN_CODEBOOK disabled.
 *
 * If an error occurs, the current packet is flushed.
 *
 * [Parameters]
 *     handle: Stream handle.
 *     book: Codebook to use.
 *     outputs: Output vectors for each channel.
 *     len: Length of each output vector.
 *     total_decode: Total number of elements to read.
 * [Return value]
 *     True on success, false on error or end-of-packet.
 */
static bool codebook_decode_deinterleave_repeat_2(
    stb_vorbis *handle, const Codebook *book, float **outputs, int n,
    int32_t total_offset, int32_t total_decode)
{
    if (UNLIKELY(book->lookup_type == 0)) {
        flush_packet(handle);
        return error(handle, VORBIS_invalid_packet);
    }
    if (total_decode > 2*n - total_offset) {
        total_decode = 2*n - total_offset;
    }
    float * const output0 = outputs[0];
    float * const output1 = outputs[1];
    int c_inter = total_offset % 2;
    int p_inter = total_offset / 2;
    int len = book->dimensions;

    while (total_decode > 0) {
        /* Make sure we don't run off the end of the output vectors. */
        if (len > total_decode) {
            len = total_decode;
        }

        const int32_t code = codebook_decode_scalar_for_vq(handle, book);
        if (UNLIKELY(code < 0)) {
            return false;
        }

        const int32_t offset = code * book->dimensions;
        int i = 0;
        if (c_inter == 1) {
            output1[p_inter] += codebook_element(book, offset);
            c_inter = 0;
            p_inter++;
            i++;
        }
        for (; i+1 < len; i += 2) {
            output0[p_inter] += codebook_element(book, offset+i);
            output1[p_inter] += codebook_element(book, offset+i+1);
            p_inter++;
        }
        if (i < len) {
            output0[p_inter] += codebook_element(book, offset+i);
            c_inter++;
        }

        total_decode -= len;
    }

    return true;
}

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
static inline void render_line(int x0, int y0, int x1, int y1, float *output,
                               int n)
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
 * decode_floor0:  Perform type 0 floor decoding.
 *
 * [Parameters]
 *     handle: Stream handle.
 *     floor: Floor configuration.
 *     coefficients: Floor coefficient buffer for the current channel.
 * [Return value]
 *     The floor amplitude (positive), 0 to indicate "unused" status, or
 *     -1 to indicate an undecodable packet.
 */
static int64_t decode_floor0(stb_vorbis *handle, const Floor0 *floor,
                             float *coefficients)
{
    const int amplitude = get_bits(handle, floor->amplitude_bits);
    if (amplitude == 0) {
        return 0;
    }

    const int booknumber = get_bits(handle, floor->book_bits);
    if (booknumber >= floor->number_of_books) {
        return -1;
    }
    if (handle->valid_bits < 0) {
        /* The spec says we should treat EOP during floor decode as an
         * all-zero channel.  We check here because otherwise we could
         * attempt to use book number -1 below. */
        return 0;
    }
    const Codebook *book = &handle->codebooks[floor->book_list[booknumber]];

    float last = 0;
    int coef_count = 0;
    while (coef_count < floor->order) {
        const int len = min(book->dimensions, floor->order - coef_count);
        for (int i = 0; i < len; i++) {
            coefficients[coef_count + i] = last;
        }
        if (!codebook_decode(handle, book, &coefficients[coef_count], len)) {
            return 0;
        }
        coef_count += len;
        last = coefficients[coef_count - 1];
    }

    return amplitude;
}

/*-----------------------------------------------------------------------*/

/**
 * decode_floor1:  Perform type 1 floor decoding.
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
        if (cbits > 0) {
            const Codebook *book =
                &handle->codebooks[floor->class_masterbooks[class]];
            cval = codebook_decode_scalar(handle, book);
        }
        for (int j = 0; j < cdim; j++) {
            const int book_index = floor->subclass_books[class][cval & csub];
            cval >>= cbits;
            if (book_index >= 0) {
                const Codebook *book = &handle->codebooks[book_index];
                final_Y[offset++] = codebook_decode_scalar(handle, book);
            } else {
                final_Y[offset++] = 0;
            }
        }
    }
    if (handle->valid_bits < 0) {
        return false;
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
        const int val = final_Y[i];  // Value read from packet.
        const int highroom = range - predicted;
        const int lowroom = predicted;
        int room;
        if (highroom > lowroom) {
            room = lowroom * 2;
        } else {
            room = highroom * 2;
        }
        if (val) {
            step2_flag[low] = true;
            step2_flag[high] = true;
            step2_flag[i] = true;
            if (val >= room) {
                if (highroom > lowroom) {
                    /* The spec (7.2.4) suggests that "decoder implementations
                     * guard the values in vector [floor1_final_Y] by clamping
                     * each element to [0, [range])" because "it is possible
                     * to abuse the setup and codebook machinery to produce
                     * negative or over-range results".  That can only happen
                     * in these two cases, so we insert tests here.  This has
                     * about a 1% performance penalty, but that's better than
                     * corrupt data causing the library to crash. */
                    if (UNLIKELY(val > range - 1)) {
                        final_Y[i] = range - 1;
                    } else {
                        final_Y[i] = val;
                    }
                } else {
                    if (UNLIKELY(val > range - 1)) {
                        final_Y[i] = 0;
                    } else {
                        final_Y[i] = (range - 1) - val;
                    }
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
 * do_floor0_final:  Perform the floor curve computation and residue
 * product for type 0 floors.
 *
 * [Parameters]
 *     handle: Stream handle.
 *     floor: Floor configuration.
 *     ch: Channel to operate on.
 *     n: Frame window size.
 *     amplitude: Floor amplitude.
 */
static void do_floor0_final(stb_vorbis *handle, const Floor0 *floor,
                            const int ch, const int n, const int64_t amplitude)
{
    float *output = handle->channel_buffers[handle->cur_channel_buffer][ch];
    float *coefficients = handle->coefficients[ch];
    const int16_t *map = floor->map[(n == handle->blocksize[1])];
    const float omega_base = M_PIf / floor->bark_map_size;
    const float scaled_amplitude = (float)amplitude
        / (float)((UINT64_C(1) << floor->amplitude_bits) - 1);
    const float lfv_scale = 0.11512925f * floor->amplitude_offset;

    for (int i = 0; i < floor->order; i++) {
        coefficients[i] = 2 * cosf(coefficients[i]);
    }

    int i = 0;
    do {
        const float two_cos_omega = 2 * cosf(map[i] * omega_base);
        float p = 0.5, q = 0.5;
        int j;
        for (j = 0; j <= (floor->order - 2) / 2; j++) {
            p *= coefficients[2*j+1] - two_cos_omega;
            q *= coefficients[2*j] - two_cos_omega;
        }
        if (floor->order % 2 != 0) {
            q *= coefficients[2*j] - two_cos_omega;
            /* The spec gives this as "4 - cos^2(omega)", but we use the
             * equality (a^2-b^2) = (a+b)(a-b) to give us constants
             * common between both branches of the if statement. */
            p *= p * (2 + two_cos_omega) * (2 - two_cos_omega);
            q *= q;
        } else {
            p *= p * (2 - two_cos_omega);
            q *= q * (2 + two_cos_omega);
        }
        const float linear_floor_value =
            expf(lfv_scale * ((scaled_amplitude / sqrtf(p + q)) - 1));
        const int iteration_condition = map[i];
        do {
            output[i] *= linear_floor_value;
            i++;
        } while (map[i] == iteration_condition);
    } while (i < n/2);
}

/*-----------------------------------------------------------------------*/

/**
 * do_floor1_final:  Perform the final floor curve computation and residue
 * product for type 1 floors.
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
    float *output = handle->channel_buffers[handle->cur_channel_buffer][ch];
    const int16_t *final_Y = handle->final_Y[ch];
    int lx = 0;
    int ly = final_Y[0] * floor->floor1_multiplier;
    for (int i = 1; i < floor->values; i++) {
        int j = floor->sorted_order[i];
        if (final_Y[j] >= 0) {
            const int hx = floor->X_list[j];
            const int hy = final_Y[j] * floor->floor1_multiplier;
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

/**
 * decode_residue_partition_0:  Decode a single residue partition for
 * residue type 0.
 *
 * [Parameters]
 *     handle: Stream handle.
 *     book: Codebook to use for decoding.
 * [Return value]
 *     True on success, false on end of packet.
 */
static bool decode_residue_partition_0(
    stb_vorbis *handle, const Codebook *book, int size, int offset,
    float **output_ptr, UNUSED int n, UNUSED int ch)
{
    float *output = *output_ptr;
    const int dimensions = book->dimensions;
    const int step = size / dimensions;
    for (int i = 0; i < step; i++) {
        if (!codebook_decode_step(handle, book, &output[offset+i], size-i,
                                  step)) {
            return false;
        }
    }
    return true;
}

/*-----------------------------------------------------------------------*/

/**
 * decode_residue_partition_1:  Decode a single residue partition for
 * residue type 1.
 *
 * [Parameters]
 *     handle: Stream handle.
 *     book: Codebook to use for decoding.
 * [Return value]
 *     True on success, false on end of packet.
 */
static bool decode_residue_partition_1(
    stb_vorbis *handle, const Codebook *book, int size, int offset,
    float **output_ptr, UNUSED int n, UNUSED int ch)
{
    float *output = *output_ptr;
    const int dimensions = book->dimensions;
    for (int i = 0; i < size; i += dimensions) {
        if (!codebook_decode(handle, book, &output[offset+i], size-i)) {
            return false;
        }
    }
    return true;
}

/*-----------------------------------------------------------------------*/

/**
 * decode_residue_partition_2:  Decode a single residue partition for
 * residue type 2.
 *
 * [Parameters]
 *     handle: Stream handle.
 *     book: Codebook to use for decoding.
 * [Return value]
 *     True on success, false on end of packet.
 */
static bool decode_residue_partition_2(
    stb_vorbis *handle, const Codebook *book, int size, int offset,
    float **outputs, int n, int ch)
{
    return codebook_decode_deinterleave_repeat(
        handle, book, outputs, ch, n, offset, size);
}

/*-----------------------------------------------------------------------*/

/**
 * decode_residue_partition_2_2ch:  Decode a single residue partition for
 * residue type 2 with two channels.
 *
 * [Parameters]
 *     handle: Stream handle.
 *     book: Codebook to use for decoding.
 * [Return value]
 *     True on success, false on end of packet.
 */
static bool decode_residue_partition_2_2ch(
    stb_vorbis *handle, const Codebook *book, int size, int offset,
    float **outputs, int n, UNUSED int ch)
{
    return codebook_decode_deinterleave_repeat_2(
        handle, book, outputs, n, offset, size);
}

/*-----------------------------------------------------------------------*/

/**
 * decode_residue_common:  Common decode processing for all residue vector
 * types.
 *
 * [Parameters]
 *     handle: Stream handle.
 *     res: Residue configuration.
 *     type: Residue type.
 *     n: Length of residue vectors (half of window size).
 *     ch: Number of channels of residue data to decode.
 *     do_not_decode: Array of "do not decode" flags for each channel.
 *     residue_buffers: Pointers to the buffers for each channel in which
 *         the residue vectors should be stored.  If the do_not_decode[]
 *         flag for a channel is true, the corresponding pointer in this
 *         array will not be reference.
 */
static void decode_residue_common(
    stb_vorbis *handle, Residue *res, int type, int n, int ch,
    const bool *do_not_decode, float *residue_buffers[],
    bool (*do_partition)(
        stb_vorbis *handle, const Codebook *book, int size, int offset,
        float **outputs, int n, int ch))
{
    const Codebook *classbook = &handle->codebooks[res->classbook];
    const int classwords = classbook->dimensions;
    const int n_to_read = res->end - res->begin;
    const int partitions_to_read = n_to_read / res->part_size;
    const int ch_to_read = (type == 2 ? 1 : ch);

    if (handle->divides_in_residue) {

        int **classifications = handle->classifications;

        for (int pass = 0; pass < 8; pass++) {
            int partition_count = 0;
            while (partition_count < partitions_to_read) {
                if (pass == 0) {
                    for (int j = 0; j < ch_to_read; j++) {
                        if (!do_not_decode[j]) {
                            int temp =
                                codebook_decode_scalar(handle, classbook);
                            if (temp == EOP) {
                                return;
                            }
                            for (int i = classwords-1; i >= 0; i--) {
                                classifications[j][i+partition_count] =
                                    temp % res->classifications;
                                temp /= res->classifications;
                            }
                        }
                    }
                }
                for (int i = 0;
                     i < classwords && partition_count < partitions_to_read;
                     i++, partition_count++)
                {
                    for (int j = 0; j < ch_to_read; j++) {
                        if (!do_not_decode[j]) {
                            const int vqclass =
                                classifications[j][partition_count];
                            const int vqbook =
                                res->residue_books[vqclass][pass];
                            if (vqbook >= 0) {
                                const int32_t offset = res->begin
                                    + partition_count * res->part_size;
                                if (!(*do_partition)(
                                        handle, &handle->codebooks[vqbook],
                                        res->part_size, offset,
                                        &residue_buffers[j], n, ch)) {
                                    return;
                                }
                            }
                        }
                    }
                }
            }
        }

    } else {  // !handle->divides_in_residue

        uint8_t ***part_classdata = handle->classifications;

        for (int pass = 0; pass < 8; pass++) {
            int partition_count = 0;
            int class_set = 0;
            while (partition_count < partitions_to_read) {
                if (pass == 0) {
                    for (int j = 0; j < ch_to_read; j++) {
                        if (!do_not_decode[j]) {
                            int temp =
                                codebook_decode_scalar(handle, classbook);
                            if (temp == EOP) {
                                return;
                            }
                            part_classdata[j][class_set] =
                                res->classdata[temp];
                        }
                    }
                }
                for (int i = 0;
                     i < classwords && partition_count < partitions_to_read;
                     i++, partition_count++)
                {
                    for (int j = 0; j < ch_to_read; j++) {
                        if (!do_not_decode[j]) {
                            const int vqclass =
                                part_classdata[j][class_set][i];
                            const int vqbook =
                                res->residue_books[vqclass][pass];
                            if (vqbook >= 0) {
                                const int32_t offset = res->begin
                                    + partition_count * res->part_size;
                                if (!(*do_partition)(
                                        handle, &handle->codebooks[vqbook],
                                        res->part_size, offset,
                                        &residue_buffers[j], n, ch)) {
                                    return;
                                }
                            }
                        }
                    }
                }
                class_set++;
            }
        }

    }
}

/*-----------------------------------------------------------------------*/

/**
 * decode_residue:  Decode the residue vectors for the current frame.
 *
 * [Parameters]
 *     handle: Stream handle.
 *     residue_index: Index of residue configuration to use.
 *     n: Length of residue vectors (half of window size).
 *     ch: Number of channels of residue data to decode.
 *     do_not_decode: Array of "do not decode" flags for each channel.
 *     residue_buffers: Pointers to the buffers for each channel in which
 *         the residue vectors should be stored.  If the do_not_decode[]
 *         flag for a channel is true, the corresponding pointer in this
 *         array will not be reference.
 */
static void decode_residue(stb_vorbis *handle, int residue_index, int n,
                           int ch, const bool *do_not_decode,
                           float *residue_buffers[])
{
    Residue *res = &handle->residue_config[residue_index];
    const int type = handle->residue_types[residue_index];

    for (int i = 0; i < ch; i++) {
        if (!do_not_decode[i]) {
            memset(residue_buffers[i], 0, sizeof(*residue_buffers[i]) * n);
        }
    }

    /* For residue type 2, if there are multiple channels, we need to
     * deinterleave the data after decoding it. */
    if (type == 2 && ch > 1) {
        if (ch == 2 && !handle->divides_in_residue) {
            /* We have slightly optimized handling for this case. */
            decode_residue_common(
                handle, res, type, n, ch, do_not_decode, residue_buffers,
                decode_residue_partition_2_2ch);
        } else {
            decode_residue_common(
                handle, res, type, n, ch, do_not_decode, residue_buffers,
                decode_residue_partition_2);
        }
    } else {
        decode_residue_common(
            handle, res, type, n, ch, do_not_decode, residue_buffers,
            type==1 ? decode_residue_partition_1 : decode_residue_partition_0);
    }
}

/*************************************************************************/
/************************ Inverse MDCT processing ************************/
/*************************************************************************/

/**
 * imdct_setup_step1:  Setup and step 1 of the IMDCT.
 *
 * [Parameters]
 *     n: Window size.
 *     A: Twiddle factor A.
 *     Y: Input buffer (length n/2).
 *     v: Output buffer (length n/2).  Must be distinct from the input buffer.
 */
static void imdct_setup_step1(const unsigned int n, const float *A,
                              const float *Y, float *v)
{
#if 0  // Roughly literal implementation.
    for (unsigned int i = 0; i < n/4; i += 2) {
        v[(n/2)-i-1] = Y[i*2]*A[i+0] - Y[(i+1)*2]*A[i+1];
        v[(n/2)-i-2] = Y[i*2]*A[i+1] + Y[(i+1)*2]*A[i+0];
    }
    for (unsigned int i = 0; i < n/4; i += 2) {
        v[(n/4)-i-1] =
            -Y[(n/2-1)-i*2]*A[(n/4)+i+0] - -Y[(n/2-1)-(i+1)*2]*A[(n/4)+i+1];
        v[(n/4)-i-2] =
            -Y[(n/2-1)-i*2]*A[(n/4)+i+1] + -Y[(n/2-1)-(i+1)*2]*A[(n/4)+i+0];
    }
#else  // Optimized implementation (reduces register pressure).
    v += n/2;
    for (int i = 0, j = -2; i < (int)(n/4); i += 2, j -= 2) {
        v[j+1] = Y[i*2]*A[i+0] - Y[(i+1)*2]*A[i+1];
        v[j+0] = Y[i*2]*A[i+1] + Y[(i+1)*2]*A[i+0];
    }
    Y += n/2+1;
    A += n/4;
    v -= n/4;
    for (int i = 0, j = -2; i < (int)(n/4); i += 2, j -= 2) {
        v[j+1] = -Y[(j+1)*2]*A[i+0] - -Y[j*2]*A[i+1];
        v[j+0] = -Y[(j+1)*2]*A[i+1] + -Y[j*2]*A[i+0];
    }
#endif
}

/*-----------------------------------------------------------------------*/

/**
 * imdct_step2:  Step 2 of the IMDCT.
 *
 * [Parameters]
 *     n: Window size.
 *     A: Twiddle factor A.
 *     v: Input buffer (length n/2).
 *     w: Output buffer (length n/2).  May be the same as the input buffer.
 */
static void imdct_step2(const unsigned int n, const float *A,
                        const float *v, float *w)
{
    const float *v0 = &v[(n/4)];
    const float *v1 = &v[0];
    float *w0 = &w[(n/4)];
    float *w1 = &w[0];
    const float *AA = &A[(n/2)-8];
    for (int i = 0; AA >= A; i += 4, AA -= 8) {
        float v40_20, v41_21;

        v41_21  = v0[i+1] - v1[i+1];
        v40_20  = v0[i+0] - v1[i+0];
        w0[i+1] = v0[i+1] + v1[i+1];
        w0[i+0] = v0[i+0] + v1[i+0];
        w1[i+1] = v41_21*AA[4] - v40_20*AA[5];
        w1[i+0] = v40_20*AA[4] + v41_21*AA[5];

        v41_21  = v0[i+3] - v1[i+3];
        v40_20  = v0[i+2] - v1[i+2];
        w0[i+3] = v0[i+3] + v1[i+3];
        w0[i+2] = v0[i+2] + v1[i+2];
        w1[i+3] = v41_21*AA[0] - v40_20*AA[1];
        w1[i+2] = v40_20*AA[0] + v41_21*AA[1];
    }
}

/*-----------------------------------------------------------------------*/

/**
 * imdct_step3_iter0_loop:  Step 3 of the IMDCT for l=0.
 *
 * [Parameters]
 *     lim: Window size / 16.
 *     A: Twiddle factor A.
 *     e: Input/output buffer (length n/2, modified in place).
 *     i_off: Initial offset, calculated as (n/2-1 - k0*s).
 */
static void imdct_step3_iter0_loop(const unsigned int lim, const float *A,
                                   float *e, int i_off)
{
    float *ee0 = e + i_off;
    float *ee2 = ee0 - 2*lim;

    ASSERT((lim & 3) == 0);
    for (int i = lim/4; i > 0; i--) {
        float k00_20, k01_21;
        k00_20 = ee0[ 0] - ee2[ 0];
        k01_21 = ee0[-1] - ee2[-1];
        ee0[ 0] += ee2[ 0];
        ee0[-1] += ee2[-1];
        ee2[ 0] = k00_20 * A[0] - k01_21 * A[1];
        ee2[-1] = k01_21 * A[0] + k00_20 * A[1];
        A += 8;

        k00_20 = ee0[-2] - ee2[-2];
        k01_21 = ee0[-3] - ee2[-3];
        ee0[-2] += ee2[-2];
        ee0[-3] += ee2[-3];
        ee2[-2] = k00_20 * A[0] - k01_21 * A[1];
        ee2[-3] = k01_21 * A[0] + k00_20 * A[1];
        A += 8;

        k00_20  = ee0[-4] - ee2[-4];
        k01_21  = ee0[-5] - ee2[-5];
        ee0[-4] += ee2[-4];
        ee0[-5] += ee2[-5];
        ee2[-4] = k00_20 * A[0] - k01_21 * A[1];
        ee2[-5] = k01_21 * A[0] + k00_20 * A[1];
        A += 8;

        k00_20  = ee0[-6] - ee2[-6];
        k01_21  = ee0[-7] - ee2[-7];
        ee0[-6] += ee2[-6];
        ee0[-7] += ee2[-7];
        ee2[-6] = k00_20 * A[0] - k01_21 * A[1];
        ee2[-7] = k01_21 * A[0] + k00_20 * A[1];
        A += 8;
        ee0 -= 8;
        ee2 -= 8;
    }
}

/*-----------------------------------------------------------------------*/

/**
 * imdct_step3_inner_r_loop:  Step 3 of the IMDCT for a single iteration
 * of the l and s loops.
 *
 * [Parameters]
 *     lim: Iteration limit for the r loop.
 *     A: Twiddle factor A.
 *     e: Input/output buffer (length n/2, modified in place).
 *     i_off: Initial offset, calculated as (n/2-1 - k0*s).
 *     k0: Constant k0.
 *     k1: Constant k1.
 */
static void imdct_step3_inner_r_loop(const unsigned int lim, const float *A,
                                     float *e, int i_off, int k0, int k1)
{
    float *e0 = e + i_off;
    float *e2 = e0 - k0/2;

    for (int i=lim/4; i > 0; --i) {
        float k00_20, k01_21;

        k00_20 = e0[-0] - e2[-0];
        k01_21 = e0[-1] - e2[-1];
        e0[-0] += e2[-0];
        e0[-1] += e2[-1];
        e2[-0] = (k00_20)*A[0] - (k01_21) * A[1];
        e2[-1] = (k01_21)*A[0] + (k00_20) * A[1];
        A += k1;

        k00_20 = e0[-2] - e2[-2];
        k01_21 = e0[-3] - e2[-3];
        e0[-2] += e2[-2];
        e0[-3] += e2[-3];
        e2[-2] = (k00_20)*A[0] - (k01_21) * A[1];
        e2[-3] = (k01_21)*A[0] + (k00_20) * A[1];
        A += k1;

        k00_20 = e0[-4] - e2[-4];
        k01_21 = e0[-5] - e2[-5];
        e0[-4] += e2[-4];
        e0[-5] += e2[-5];
        e2[-4] = (k00_20)*A[0] - (k01_21) * A[1];
        e2[-5] = (k01_21)*A[0] + (k00_20) * A[1];
        A += k1;

        k00_20 = e0[-6] - e2[-6];
        k01_21 = e0[-7] - e2[-7];
        e0[-6] += e2[-6];
        e0[-7] += e2[-7];
        e2[-6] = (k00_20)*A[0] - (k01_21) * A[1];
        e2[-7] = (k01_21)*A[0] + (k00_20) * A[1];
        A += k1;

        e0 -= 8;
        e2 -= 8;
    }
}

/*-----------------------------------------------------------------------*/

/**
 * imdct_step3_inner_s_loop:  Step 3 of the IMDCT for a single iteration
 * of the l and r loops.
 *
 * [Parameters]
 *     lim: Iteration limit for the s loop.
 *     A: Twiddle factor A.
 *     e: Input/output buffer (length n/2, modified in place).
 *     i_off: Initial offset, calculated as (n/2-1 - k0*s).
 *     k0: Constant k0.
 *     k1: Constant k1.
 */
static void imdct_step3_inner_s_loop(const unsigned int lim, const float *A,
                                     float *e, int i_off, int k0, int k1)
{
    const float A0 = A[0];
    const float A1 = A[0+1];
    const float A2 = A[0+k1];
    const float A3 = A[0+k1+1];
    const float A4 = A[0+k1*2+0];
    const float A5 = A[0+k1*2+1];
    const float A6 = A[0+k1*3+0];
    const float A7 = A[0+k1*3+1];

    float *ee0 = e + i_off;
    float *ee2 = ee0 - k0/2;

    for (int i = lim; i > 0; i--) {
        float k00, k11;

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

/*-----------------------------------------------------------------------*/

/**
 * iter_54:  Step 3 of the IMDCT for l=ld(n)-5 and l=ld(n)-4 for a sequence
 * of 8 elements.  Helper function for imdct_step3_inner_s_loop_ld654().
 *
 * [Parameters]
 *     z: Pointer to elements to operate on.
 */
static inline void iter_54(float *z)
{
    const float k00  = z[ 0] - z[-4];
    const float y0   = z[ 0] + z[-4];
    const float y2   = z[-2] + z[-6];
    const float k22  = z[-2] - z[-6];

    z[-0] = y0 + y2;      // z0 + z4 + z2 + z6
    z[-2] = y0 - y2;      // z0 + z4 - z2 - z6

    const float k33  = z[-3] - z[-7];

    z[-4] = k00 + k33;    // z0 - z4 + z3 - z7
    z[-6] = k00 - k33;    // z0 - z4 - z3 + z7

    const float k11  = z[-1] - z[-5];
    const float y1   = z[-1] + z[-5];
    const float y3   = z[-3] + z[-7];

    z[-1] = y1 + y3;      // z1 + z5 + z3 + z7
    z[-3] = y1 - y3;      // z1 + z5 - z3 - z7
    z[-5] = k11 - k22;    // z1 - z5 + z2 - z6
    z[-7] = k11 + k22;    // z1 - z5 - z2 + z6
}

/*-----------------------------------------------------------------------*/

/**
 * imdct_step3_inner_s_loop:  Step 3 of the IMDCT for a single iteration
 * of the l and r loops.
 *
 * [Parameters]
 *     n: Window size.
 *     A: Twiddle factor A.
 *     e: Input/output buffer (length n/2, modified in place).
 *     i_off: Initial offset, calculated as (n/2-1 - k0*s).
 *     k0: Constant k0.
 *     k1: Constant k1.
 */
static void imdct_step3_inner_s_loop_ld654(
    const unsigned int n, const float *A, float *e)
{
    const int a_off = n/8;
    const float A2 = A[0+a_off];
    float *z = e + (n/2-1);
    float *base = z - n/2;

    while (z > base) {
        float k00, k11;

        k00   = z[-0] - z[-8];
        k11   = z[-1] - z[-9];
        z[-0] = z[-0] + z[-8];
        z[-1] = z[-1] + z[-9];
        z[-8] = k00;
        z[-9] = k11;

        k00    = z[ -2] - z[-10];
        k11    = z[ -3] - z[-11];
        z[ -2] = z[ -2] + z[-10];
        z[ -3] = z[ -3] + z[-11];
        z[-10] = (k00+k11) * A2;
        z[-11] = (k11-k00) * A2;

        k00    = z[-12] - z[ -4];  // Reversed to avoid a unary negation.
        k11    = z[ -5] - z[-13];
        z[ -4] = z[ -4] + z[-12];
        z[ -5] = z[ -5] + z[-13];
        z[-12] = k11;
        z[-13] = k00;

        k00    = z[-14] - z[ -6];  // Reversed to avoid a unary negation.
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

/*-----------------------------------------------------------------------*/

/**
 * imdct_step456:  Steps 4, 5, and 6 of the IMDCT.
 *
 * [Parameters]
 *     n: Window size.
 *     bitrev: Bit-reverse lookup table.
 *     u: Input buffer (length n/2).
 *     U: Output buffer (length n/2).  Must be distinct from the input buffer.
 */
static void imdct_step456(const unsigned int n, const uint16_t *bitrev,
                          const float *u, float *U)
{
    /* stb_vorbis note: "weirdly, I'd have thought reading sequentially and
     * writing erratically would have been better than vice-versa, but in fact
     * that's not what my testing showed. (That is, with j = bitreverse(i),
     * do you read i and write j, or read j and write i.)" */

    float *U0 = &U[n/4];
    float *U1 = &U[n/2];

    for (int i = 0, j = -4; i < (int)(n/8); i += 2, j -= 4) {
        int k4;

        k4 = bitrev[i+0];
        U1[j+3] = u[k4+0];
        U1[j+2] = u[k4+1];
        U0[j+3] = u[k4+2];
        U0[j+2] = u[k4+3];

        k4 = bitrev[i+1];
        U1[j+1] = u[k4+0];
        U1[j+0] = u[k4+1];
        U0[j+1] = u[k4+2];
        U0[j+0] = u[k4+3];
    }
}

/*-----------------------------------------------------------------------*/

/**
 * imdct_step7:  Step 7 of the IMDCT.
 *
 * [Parameters]
 *     n: Window size.
 *     C: Twiddle factor C.
 *     buffer: Input/output buffer (length n/2, modified in place).
 */
static void imdct_step7(const unsigned int n, const float *C, float *buffer)
{
    float *d = buffer;
    float *e = buffer + (n/2) - 4;

    while (d < e) {
        float a02, a11, b0, b1, b2, b3;

        a02 = d[0] - e[2];
        a11 = d[1] + e[3];

        b0 = C[1]*a02 + C[0]*a11;
        b1 = C[1]*a11 - C[0]*a02;

        b2 = d[0] + e[2];
        b3 = d[1] - e[3];

        d[0] = b2 + b0;
        d[1] = b3 + b1;
        e[2] = b2 - b0;
        e[3] = b1 - b3;

        a02 = d[2] - e[0];
        a11 = d[3] + e[1];

        b0 = C[3]*a02 + C[2]*a11;
        b1 = C[3]*a11 - C[2]*a02;

        b2 = d[2] + e[0];
        b3 = d[3] - e[1];

        d[2] = b2 + b0;
        d[3] = b3 + b1;
        e[0] = b2 - b0;
        e[1] = b1 - b3;

        C += 4;
        d += 4;
        e -= 4;
    }
}

/*-----------------------------------------------------------------------*/

/**
 * imdct_step8_decode:  Step 8 and final decoding for the IMDCT.
 *
 * [Parameters]
 *     n: Window size.
 *     B: Twiddle factor B.
 *     in: Input buffer (length n/2).
 *     out: Output buffer (length n).
 */
static void imdct_step8_decode(const unsigned int n, const float *B,
                               const float *in, float *out)
{
    /* stb_vorbis note: "this generates pairs of data a la 8 and pushes
     * them directly through the decode kernel (pushing rather than
     * pulling) to avoid having to make another pass later" */

    B += (n/2) - 8;
    const float *e = in + (n/2) - 8;
    float *d0 = &out[0];
    float *d1 = &out[(n/2)-4];
    float *d2 = &out[(n/2)];
    float *d3 = &out[n-4];

    while (e >= in) {
        float p0, p1, p2, p3;

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
        d1 -= 4;
        d2 += 4;
        d3 -= 4;
    }
}

/*-----------------------------------------------------------------------*/

/**
 * inverse_mdct:  Perform the inverse MDCT operation on the given buffer.
 * The algorithm is taken from "The use of multirate filter banks for
 * coding of high quality digital audio", Th. Sporer et. al. (1992), with
 * corrections for errors in that paper.
 *
 * [Parameters]
 *     handle: Stream handle.
 *     buffer: Input/output buffer.
 *     blocktype: 0 if the current frame is a short block, 1 if a long block.
 */
static void inverse_mdct(stb_vorbis *handle, float *buffer, int blocktype)
{
    const unsigned int n = handle->blocksize[blocktype];
    const int log2_n = handle->blocksize_bits[blocktype];
    const float *A = handle->A[blocktype];
    float *buf2 = handle->imdct_temp_buf;

    /* Setup and step 1.  Note that step 1 involves subtracting pairs of
     * spectral coefficients, but the two items subtracted are actually
     * arithmetic inverses of each other(!), e.g.
     *     u[4*k] - u[n-4*k-1] == 2*u[4*k]
     * We omit the factor of 2 here; that propagates linearly through to
     * the final step, and it is compensated for by "*0.5f" on the B[]
     * values computed during setup. */
    imdct_setup_step1(n, A, buffer, buf2);

    /* Step 2.
     * stb_vorbis note: "this could be in place, but the data ends up in
     * the wrong place... _somebody_'s got to swap it, so this is nominated" */
    imdct_step2(n, A, buf2, buffer);

    /* Step 3.
     * stb_vorbis note: "the original step3 loop can be nested r inside s
     * or s inside r; it's written originally as s inside r, but this is
     * dumb when r iterates many times, and s few. So I have two copies of
     * it and switch between them halfway." */

    imdct_step3_iter0_loop(n/16, A, buffer, (n/2)-1-(n/4)*0);
    imdct_step3_iter0_loop(n/16, A, buffer, (n/2)-1-(n/4)*1);

    imdct_step3_inner_r_loop(n/32, A, buffer, (n/2)-1 - (n/8)*0, n/8, 16);
    imdct_step3_inner_r_loop(n/32, A, buffer, (n/2)-1 - (n/8)*1, n/8, 16);
    imdct_step3_inner_r_loop(n/32, A, buffer, (n/2)-1 - (n/8)*2, n/8, 16);
    imdct_step3_inner_r_loop(n/32, A, buffer, (n/2)-1 - (n/8)*3, n/8, 16);

    int l = 2;
    for (; l < (log2_n-3)/2; l++) {
        const int k0 = n >> (l+2);
        const int lim = 1 << (l+1);
        for (int i = 0; i < lim; i++) {
            imdct_step3_inner_r_loop(n >> (l+4), A, buffer, (n/2)-1 - k0*i,
                                     k0, 1 << (l+3));
        }
    }

    for (; l < log2_n-6; l++) {
        const int k0 = n >> (l+2), k1 = 1 << (l+3);
        const int rlim = n >> (l+6);
        const int lim = 1 << (l+1);
        const float *A0 = A;
        int i_off = (n/2)-1;
        for (int r = rlim; r > 0; r--) {
            imdct_step3_inner_s_loop(lim, A0, buffer, i_off, k0, k1);
            A0 += k1*4;
            i_off -= 8;
        }
    }

    /* stb_vorbis note: "log2_n-6,-5,-4 all interleaved together - the big
     * win comes from getting rid of needless flops due to the constants on
     * pass 5 & 4 being all 1 and 0; combining them to be simultaneous to
     * improve cache made little difference" */
    imdct_step3_inner_s_loop_ld654(n, A, buffer);

    /* Steps 4, 5, and 6. */
    imdct_step456(n, handle->bit_reverse[blocktype], buffer, buf2);

    /* Step 7. */
    imdct_step7(n, handle->C[blocktype], buf2);

    /* Step 8 and final decoding. */
    imdct_step8_decode(n, handle->B[blocktype], buf2, buffer);
}

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
 *     len_ret: Pointer to variable to receive the frame length (offset
 *         within the window of just past the final fully-decoded sample).
 * [Return value]
 *     True on success, false on error.
 */
static bool vorbis_decode_packet_rest(
    stb_vorbis *handle, const Mode *mode, int *left_start_ptr, int left_end,
    int right_start, int right_end, int *len_ret)
{
    const int n = handle->blocksize[mode->blockflag];
    const Mapping *map = &handle->mapping[mode->mapping];
    float ** const channel_buffers =
        handle->channel_buffers[handle->cur_channel_buffer];

    /**** Floor processing (4.3.2). ****/

    int64_t floor0_amplitude[256];
    bool zero_channel[256];
    for (int ch = 0; ch < handle->channels; ch++) {
        const int floor_index = map->submap_floor[map->mux[ch]];
        if (handle->floor_types[floor_index] == 0) {
            Floor0 *floor = &handle->floor_config[floor_index].floor0;
            const int64_t amplitude =
                decode_floor0(handle, floor, handle->coefficients[ch]);
            if (amplitude < 0) {
                flush_packet(handle);
                return error(handle, VORBIS_invalid_packet);
            }
            zero_channel[ch] = (amplitude == 0);
            floor0_amplitude[ch] = amplitude;
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
                    residue_buffers[ch] = channel_buffers[j];
                }
                ch++;
            }
        }
        decode_residue(handle, map->submap_residue[i], n/2, ch,
                       do_not_decode, residue_buffers);
    }

    /**** Inverse coupling (4.3.5). ****/
    for (int i = map->coupling_steps-1; i >= 0; i--) {
        float *magnitude = channel_buffers[map->coupling[i].magnitude];
        float *angle = channel_buffers[map->coupling[i].angle];
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

    /**** Floor curve synthesis and residue product (4.3.6).  The spec ****
     **** uses the term "dot product", but the actual operation is     ****
     **** component-by-component vector multiplication.                ****/
    for (int i = 0; i < handle->channels; i++) {
        if (really_zero_channel[i]) {
            memset(channel_buffers[i], 0, sizeof(*channel_buffers[i]) * (n/2));
        } else {
            const int floor_index = map->submap_floor[map->mux[i]];
            if (handle->floor_types[floor_index] == 0) {
                Floor0 *floor = &handle->floor_config[floor_index].floor0;
                do_floor0_final(handle, floor, i, n, floor0_amplitude[i]);
            } else {  // handle->floor_types[floor_index] == 1
                Floor1 *floor = &handle->floor_config[floor_index].floor1;
                do_floor1_final(handle, floor, i, n);
            }
        }
    }

    /**** Inverse MDCT (4.3.7). ****/
    for (int i = 0; i < handle->channels; i++) {
        inverse_mdct(handle, channel_buffers[i], mode->blockflag);
    }

    /**** Frame length, sample position, and other miscellany. ****/

    /* Flush any leftover data in the current packet so the next packet
     * doesn't try to read it. */
    flush_packet(handle);

    /* Deal with discarding samples from the first packet. */
    if (handle->first_decode) {
        /* We don't support files with nonzero start positions (what the
         * spec describes as "The granule (PCM) position of the first page
         * need not indicate that the stream started at position zero..."
         * in A.2), so we just omit the left half of the window. */
        handle->current_loc = -(n/2 - *left_start_ptr);
        handle->current_loc_valid = true;
        handle->first_decode = false;
    }

    /* If this is the last complete frame in a non-final Ogg page, update
     * the current sample position based on the Ogg granule position.
     * Normally this should have no effect, but it's important for the
     * first page in case that page indicates a positive initial offset,
     * and it helps resync if a page is missing in the stream. */
    if (handle->last_seg_index == handle->end_seg_with_known_loc
     && !(handle->page_flag & PAGEFLAG_last_page)) {
        /* For non-final pages, the granule position should be the "last
         * complete sample returned by decode" (A.2), thus right_start.
         * However, it appears that the reference encoder blindly uses the
         * middle of the window (n/2) even on a long/short frame boundary,
         * and based on <https://trac.xiph.org/ticket/1169>, the window
         * center appears to have been the original intention. */
        handle->current_loc =
            handle->known_loc_for_packet - (n/2 - *left_start_ptr);
        handle->current_loc_valid = true;
    }

    /* Update the current sample position for samples returned from this
     * frame. */
    if (handle->current_loc_valid) {
        handle->current_loc += (right_start - *left_start_ptr);
    }

    /* Return the amount of data stored in the window, including the
     * right-side overlap region.*/
    *len_ret = right_end;

    /* The granule position on the final page of the Ogg stream specifies
     * the length of the stream, thus indicating how many samples to
     * return from the final frame.  However, some streams attempt to
     * truncate to within the _second-to-last_ frame instead, so we have
     * to check for end-of-stream truncation on every packet in the last
     * page.  (The specification is ambiguous about whether this is
     * actually permitted; the wording suggests that the truncation point
     * must lie within the last frame, but there is no explicit language
     * specifying that limitation.  Since such streams actually exist,
     * however, we go ahead and support them.) */
    if (handle->current_loc_valid
     && (handle->page_flag & PAGEFLAG_last_page)) {
        const uint64_t end_loc = handle->known_loc_for_packet;
        if (end_loc <= handle->current_loc
            || (handle->last_seg_index == handle->end_seg_with_known_loc
                && end_loc < handle->current_loc + (right_end - right_start))) {
            const uint64_t packet_begin_loc =
                handle->current_loc - (right_start - *left_start_ptr);
            if (end_loc < packet_begin_loc) {
                /* We can't truncate to the second-to-last frame if that
                 * frame is in a different page!  This almost certainly
                 * violates the spec because the Ogg granule position
                 * would go backward between pages, so we just treat it
                 * as if the last frame was truncated to zero length. */
                *len_ret = *left_start_ptr;
            } else {
                *len_ret = end_loc - (handle->current_loc - right_start);
            }
            handle->current_loc = end_loc;
        }
    }

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

bool vorbis_decode_packet(stb_vorbis *handle, int *len_ret)
{
    const bool first_decode = handle->first_decode;

    int mode, left_start, left_end, right_start, right_end, len;
    if (!vorbis_decode_initial(handle, &left_start, &left_end,
                               &right_start, &right_end, &mode)
     || !vorbis_decode_packet_rest(handle, &handle->mode_config[mode],
                                   &left_start, left_end, right_start,
                                   right_end, &len)) {
        return false;
    }

    float ** const channel_buffers =
        handle->channel_buffers[handle->cur_channel_buffer];
    const int prev = handle->previous_length;

    /* We deliberately (though harmlessly) deviate from the spec with
     * respect to the portion of data to return for a given frame.  The
     * spec says that we should return the region from the middle of the
     * previous frame to the middle of the current frame (4.8), but that
     * would require a buffer copy.  Instead we return all finalized
     * samples from the current frame, i.e., up to right_start.  This
     * unfortunately requires a bit of fudging around when decoding the
     * first frame of a stream. */

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
                channel_buffers[i][left_start+j] =
                    channel_buffers[i][left_start+j] * weights[j] +
                    handle->previous_window[i][j] * weights[prev-1-j];
            }
        }
    }

    /* Point the previous_window pointers at the right side of this window. */
    handle->previous_length = right_end - right_start;
    for (int i = 0; i < handle->channels; i++) {
        handle->previous_window[i] = channel_buffers[i] + right_start;
    }

    /* If this is the first frame, push left_start (the beginning of data
     * to return) to the center of the window, since the left half of the
     * window contains garbage. */
    if (first_decode) {
        left_start = right_start - (int)handle->current_loc;
    }

    /* Save this channel's output pointers. */
    for (int i = 0; i < handle->channels; i++) {
        handle->outputs[i] = channel_buffers[i] + left_start;
    }

    /* Advance to the next set of channel buffers for the next frame's data. */
    handle->cur_channel_buffer =
        (handle->cur_channel_buffer + 1) % lenof(handle->channel_buffers);

    /* Return the final frame length, if requested. */
    if (len_ret) {
        if (len < right_start) {
            *len_ret = len - left_start;
        } else {
            *len_ret = right_start - left_start;
        }
    }
    return true;
}

/*************************************************************************/
/*************************************************************************/
