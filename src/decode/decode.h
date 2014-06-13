/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#ifndef NOGG_SRC_DECODE_DECODE_H
#define NOGG_SRC_DECODE_DECODE_H

/*************************************************************************/
/*************************************************************************/

/**
 * vorbis_decode_initial:  Perform initial decoding steps for a packet.
 *
 * [Parameters]
 *     handle: Stream handle.
//FIXME: document other parameters
 * [Return value]
 *     True on success, false on error.
 */
extern bool vorbis_decode_initial(stb_vorbis *handle, int *p_left_start,
                                  int *p_left_end, int *p_right_start,
                                  int *p_right_end, int *mode);

/**
 * vorbis_decode_packet:  Decode a Vorbis packet into the internal PCM
 * buffers.
 *
 * [Parameters]
 *     handle: Stream handle.
 *     len, left, right: Pointers to variables to hold values to be
 *         passed to vorbis_finish_frame().
 * [Return value]
 *     True on success, false on error.
 */
extern bool vorbis_decode_packet(stb_vorbis *handle, int *len,
                                 int *p_left, int *p_right);

/**
 * vorbis_finish_frame:  Perform appropriate windowing operations to
 * extract the final data for the current frame into the channel buffers.
 *
 * [Parameters]
 *     handle: Stream handle.
 *     len, left, right: Values returned from vorbis_decode_packet().
 * [Return value]
 *     Number of samples in the frame.
 */
extern int vorbis_finish_frame(stb_vorbis *handle, int len,
                               int left, int right);

/**
 * vorbis_pump_first_frame:  Decode and discard a frame of data.
 *
 * [Parameters]
 *     handle: Stream handle.
 */
extern void vorbis_pump_first_frame(stb_vorbis *handle);

/*************************************************************************/
/*************************************************************************/

#endif  // NOGG_SRC_DECODE_DECODE_H
