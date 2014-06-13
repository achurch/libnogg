/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#ifndef NOGG_SRC_UTIL_DECODE_H
#define NOGG_SRC_UTIL_DECODE_H

/*************************************************************************/
/*************************************************************************/

/**
 * decode_frame:  Decode the next frame from the stream and store the
 * decoded data in decode_buf.
 *
 * On return from this function, the handle's decode_buf_pos field will
 * always be set to zero.
 *
 * [Parameters]
 *     handle: Handle to operate on.
 * [Return value]
 *     True if the decode buffer has at least one sample of valid data,
 *     false otherwise.
 */
extern int decode_frame(vorbis_t *handle);

/*************************************************************************/
/*************************************************************************/

#endif  // NOGG_SRC_UTIL_DECODE_H
