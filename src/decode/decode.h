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
 *     left_start_ret, left_end_ret, right_start_ret, right_end_ret: Pointers
 *         to variables to receive the frame's window parameters.
 *     mode_ret: Pointer to variable to receive the frame's mode index.
 * [Return value]
 *     True on success, false on error.
 */
extern bool vorbis_decode_initial(stb_vorbis *handle, int *left_start_ret,
                                  int *left_end_ret, int *right_start_ret,
                                  int *right_end_ret, int *mode_ret);

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
 * vorbis_pump_frame:  Decode and discard a frame of data.
 *
 * [Parameters]
 *     handle: Stream handle.
 */
extern void vorbis_pump_frame(stb_vorbis *handle);

/*************************************************************************/
/*************************************************************************/

#endif  // NOGG_SRC_DECODE_DECODE_H
