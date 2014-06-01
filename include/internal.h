/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

/*
 * This header defines internal structures and other common data for the
 * libnogg library.  This header is not a public interface, and client code
 * MUST NOT make use of any declarations or definitions in this header.
 */

#ifndef NOGG_INTERNAL_H
#define NOGG_INTERNAL_H

struct stb_vorbis;

/*************************************************************************/
/****************************** Data types *******************************/
/*************************************************************************/

/*
 * Definition of the vorbis_t stream handle structure.
 */

struct vorbis_t {

    /******** Stream callbacks and related data. ********/

    /* Callbacks used to access the stream data. */
    vorbis_callbacks_t callbacks;
    /* Opaque data pointer for callbacks. */
    void *callback_data;
    /* Length of stream data in bytes, or -1 if not a seekable stream. */
    int64_t data_length;
    /* Flag: has an I/O error occurred on the stream? */
    unsigned char read_error_flag;

    /******** Decoder handle and related data. ********/

    /* stb_vorbis decode handle. */
    struct stb_vorbis *decoder;
    /* Read buffer for stb_vorbis. */
    char read_buf[65536];
    /* Number of bytes of valid data in read_buf. */
    int32_t read_buf_len;
    /* Index of next byte in read_buf to consume. */
    int32_t read_buf_pos;

    /******** Audio parameters. ********/

    /* Number of channels. */
    int channels;
    /* Audio sampling rate, in Hz. */
    int32_t rate;
    /* Length of stream, in samples, or -1 if unknown. */
    int64_t length;

    /******** Decoding state. ********/

    /* Flag: have we hit the end of the stream? */
    unsigned char eos_flag;
    /* Current read/decode position, in seconds. */
    int64_t decode_pos;
    /* Buffer holding decoded audio data for the current frame. */
    float *decode_buf;
    /* Number of samples (per channel) of valid data in decode_buf. */
    int32_t decode_buf_len;
    /* Index of next sample (per channel) in decode_buf to consume. */
    int32_t decode_buf_pos;

};  /* struct vorbis_t */

/*************************************************************************/
/*************************************************************************/

#endif  /* NOGG_INTERNAL_H */
