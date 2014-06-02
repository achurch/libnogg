#ifndef STB_VORBIS_INCLUDE_STB_VORBIS_H
#define STB_VORBIS_INCLUDE_STB_VORBIS_H

#ifdef __cplusplus
extern "C" {
#endif

///////////   THREAD SAFETY

// Individual stb_vorbis* handles are not thread-safe; you cannot decode from
// them from multiple threads at the same time. However, you can have multiple
// stb_vorbis* handles and decode from them independently in multiple thrads.


typedef void stb_vorbis_alloc;


///////////   FUNCTIONS USEABLE WITH ALL INPUT MODES

typedef struct stb_vorbis stb_vorbis;

typedef struct
{
   unsigned int sample_rate;
   int channels;

   int max_frame_size;
} stb_vorbis_info;

// get general information about the file
extern stb_vorbis_info stb_vorbis_get_info(stb_vorbis *f);

// get the last error detected (clears it, too)
extern int stb_vorbis_get_error(stb_vorbis *f);

// get the last error detected (without clearing it)
extern int stb_vorbis_peek_error(stb_vorbis *f);

// close an ogg vorbis file and free all memory in use
extern void stb_vorbis_close(stb_vorbis *f);

//////////   PULLING INPUT API

// This API assumes stb_vorbis is allowed to pull data from a source--
// either a block of memory containing the _entire_ vorbis stream, or a
// FILE * that you or it create, or possibly some other reading mechanism
// if you go modify the source to replace the FILE * case with some kind
// of callback to your code. (But if you don't support seeking, you may
// just want to go ahead and use pushdata.)

extern stb_vorbis * stb_vorbis_open_callbacks(
   long (*read_callback)(void *opaque, void *buf, long len),
   void (*seek_callback)(void *opaque, long offset),
   long (*tell_callback)(void *opaque),
   void *opaque, int64_t length, int *error, stb_vorbis_alloc *alloc_buffer);

extern int stb_vorbis_seek(stb_vorbis *f, unsigned int sample_number);
// NOT WORKING YET
// these functions seek in the Vorbis file to (approximately) 'sample_number'.
// after calling seek_frame(), the next call to get_frame_*() will include
// the specified sample. after calling stb_vorbis_seek(), the next call to
// stb_vorbis_get_samples_* will start with the specified sample. If you
// do not need to seek to EXACTLY the target sample when using get_samples_*,
// you can also use seek_frame().

extern unsigned int stb_vorbis_stream_length_in_samples(stb_vorbis *f);
// these functions return the total length of the vorbis stream

extern int stb_vorbis_get_frame_float(stb_vorbis *f, int *channels, float ***output);
// decode the next frame and return the number of samples. the number of
// channels returned are stored in *channels (which can be NULL--it is always
// the same as the number of channels reported by get_info). *output will
// contain an array of float* buffers, one per channel. These outputs will
// be overwritten on the next call to stb_vorbis_get_frame_*.
//
// You generally should not intermix calls to stb_vorbis_get_frame_*()
// and stb_vorbis_get_samples_*(), since the latter calls the former.

////////   ERROR CODES

enum STBVorbisError
{
   VORBIS__no_error,

   VORBIS_need_more_data=1,             // not a real error

   VORBIS_invalid_api_mixing,           // can't mix API modes
   VORBIS_outofmem,                     // not enough memory
   VORBIS_feature_not_supported,        // uses floor 0
   VORBIS_too_many_channels,            // STB_VORBIS_MAX_CHANNELS is too small
   VORBIS_file_open_failure,            // fopen() failed
   VORBIS_seek_without_length,          // can't seek in unknown-length file

   VORBIS_unexpected_eof=10,            // file is truncated?
   VORBIS_seek_invalid,                 // seek past EOF

   // decoding errors (corrupt/invalid stream) -- you probably
   // don't care about the exact details of these

   // vorbis errors:
   VORBIS_invalid_setup=20,
   VORBIS_invalid_stream,

   // ogg errors:
   VORBIS_missing_capture_pattern=30,
   VORBIS_invalid_stream_structure_version,
   VORBIS_continued_packet_flag_invalid,
   VORBIS_incorrect_stream_serial_number,
   VORBIS_invalid_first_page,
   VORBIS_bad_packet_type,
   VORBIS_cant_find_last_page,
   VORBIS_seek_failed,
};


#ifdef __cplusplus
}
#endif

#endif // STB_VORBIS_INCLUDE_STB_VORBIS_H
