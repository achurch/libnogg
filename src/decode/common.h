/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#ifndef NOGG_SRC_DECODE_COMMON_H
#define NOGG_SRC_DECODE_COMMON_H

// FIXME: not reviewed

/*************************************************************************/
/*************************************************************************/

#if STB_VORBIS_FAST_HUFFMAN_LENGTH > 24
#error "Value of STB_VORBIS_FAST_HUFFMAN_LENGTH outside of allowed range"
#endif


#define MAX_BLOCKSIZE_LOG  13   // from specification
#define MAX_BLOCKSIZE      (1 << MAX_BLOCKSIZE_LOG)


#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#ifdef STB_VORBIS_CODEBOOK_FLOATS
typedef float codetype;
#else
typedef uint16_t codetype;
#endif


// @NOTE
//
// Some arrays below are tagged "//varies", which means it's actually
// a variable-sized piece of data, but rather than malloc I assume it's
// small enough it's better to just allocate it all together with the
// main thing
//
// Most of the variables are specified with the smallest size I could pack
// them into. It might give better performance to make them all full-sized
// integers. It should be safe to freely rearrange the structures or change
// the sizes larger--nothing relies on silently truncating etc., nor the
// order of variables.

#define FAST_HUFFMAN_TABLE_SIZE   (1 << STB_VORBIS_FAST_HUFFMAN_LENGTH)
#define FAST_HUFFMAN_TABLE_MASK   (FAST_HUFFMAN_TABLE_SIZE - 1)

typedef struct
{
   int dimensions, entries;
   uint8_t *codeword_lengths;
   float  minimum_value;
   float  delta_value;
   uint8_t  value_bits;
   uint8_t  lookup_type;
   uint8_t  sequence_p;
   uint8_t  sparse;
   uint32_t lookup_values;
   codetype *multiplicands;
   uint32_t *codewords;
   #ifdef STB_VORBIS_FAST_HUFFMAN_SHORT
    int16_t  fast_huffman[FAST_HUFFMAN_TABLE_SIZE];
   #else
    int32_t  fast_huffman[FAST_HUFFMAN_TABLE_SIZE];
   #endif
   uint32_t *sorted_codewords;
   int    *sorted_values;
   int     sorted_entries;
} Codebook;

typedef struct
{
   uint8_t order;
   uint16_t rate;
   uint16_t bark_map_size;
   uint8_t amplitude_bits;
   uint8_t amplitude_offset;
   uint8_t number_of_books;
   uint8_t book_list[16]; // varies
} Floor0;

typedef struct
{
   uint8_t partitions;
   uint8_t partition_class_list[32]; // varies
   uint8_t class_dimensions[16]; // varies
   uint8_t class_subclasses[16]; // varies
   uint8_t class_masterbooks[16]; // varies
   int16_t subclass_books[16][8]; // varies
   uint16_t Xlist[31*8+2]; // varies
   uint8_t sorted_order[31*8+2];
   uint8_t neighbors[31*8+2][2];
   uint8_t floor1_multiplier;
   uint8_t rangebits;
   int values;
} Floor1;

typedef union
{
   Floor0 floor0;
   Floor1 floor1;
} Floor;

typedef struct
{
   uint32_t begin, end;
   uint32_t part_size;
   uint8_t classifications;
   uint8_t classbook;
   uint8_t **classdata;
   int16_t (*residue_books)[8];
} Residue;

typedef struct
{
   uint8_t magnitude;
   uint8_t angle;
   uint8_t mux;
} MappingChannel;

typedef struct
{
   uint16_t coupling_steps;
   MappingChannel *chan;
   uint8_t  submaps;
   uint8_t  submap_floor[15]; // varies
   uint8_t  submap_residue[15]; // varies
} Mapping;

typedef struct
{
   uint8_t blockflag;
   uint8_t mapping;
   uint16_t windowtype;
   uint16_t transformtype;
} Mode;

typedef struct
{
   uint32_t  goal_crc;    // expected crc if match
   int     bytes_left;  // bytes left in packet
   uint32_t  crc_so_far;  // running crc
   int     bytes_done;  // bytes processed in _current_ chunk
   uint32_t  sample_loc;  // granule pos encoded in page
} CRCscan;

typedef struct
{
   uint32_t page_start, page_end;
   uint32_t after_previous_page_start;
   uint32_t first_decoded_sample;
   uint32_t last_decoded_sample;
} ProbedPage;

struct stb_vorbis
{
  // user-accessible info
   unsigned int sample_rate;
   int channels;

  // input config
   long (*read_callback)(void *opaque, void *buf, long len);
   void (*seek_callback)(void *opaque, long offset);  // only used if stream_len >= 0
   long (*tell_callback)(void *opaque);  // same
   void *opaque;

   uint8_t *stream;
   uint8_t *stream_start;
   uint8_t *stream_end;

   uint32_t stream_len;

   uint32_t first_audio_page_offset;

   ProbedPage p_first, p_last;

  // run-time results
   int eof;
   enum STBVorbisError error;

  // user-useful data

  // header info
   int blocksize[2];
   int blocksize_0, blocksize_1;
   int codebook_count;
   Codebook *codebooks;
   int floor_count;
   uint16_t floor_types[64]; // varies
   Floor *floor_config;
   int residue_count;
   uint16_t residue_types[64]; // varies
   Residue *residue_config;
   int mapping_count;
   Mapping *mapping;
   int mode_count;
   Mode mode_config[64];  // varies

   uint32_t total_samples;

  // decode buffer
   float **channel_buffers;
   float **outputs;

   float **previous_window;
   int previous_length;

   int16_t **finalY;

#ifdef STB_VORBIS_DIVIDES_IN_RESIDUE
   int **classifications;
#else
   uint8_t ***part_classdata;
#endif

   uint32_t current_loc; // sample location of next frame to decode
   int    current_loc_valid;

  // temporary buffer for IMDCT
   float *imdct_temp_buf;

  // per-blocksize precomputed data
   
   // twiddle factors
   float *A[2],*B[2],*C[2];
   float *window[2];
   uint16_t *bit_reverse[2];

  // current page/packet/segment streaming info
   uint32_t serial; // stream serial number for verification
   int last_page;
   int segment_count;
   uint8_t segments[255];
   uint8_t page_flag;
   uint8_t bytes_in_seg;
   uint8_t first_decode;
   int next_seg;
   int last_seg;  // flag that we're on the last segment
   int last_seg_which; // what was the segment number of the last seg?
   uint32_t acc;
   int valid_bits;
   int packet_bytes;
   int end_seg_with_known_loc;
   uint32_t known_loc_for_packet;
   int discard_samples_deferred;
   uint32_t samples_output;

  // sample-access
   int channel_buffer_start;
   int channel_buffer_end;
};


#ifndef M_PI
  #define M_PI  3.14159265358979323846264f  // from CRC
#endif

// code length assigned to a value with no huffman encoding
#define NO_CODE   255

/*************************************************************************/
/*************************************************************************/

#endif  // NOGG_SRC_DECODE_COMMON_H
