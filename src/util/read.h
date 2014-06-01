/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#ifndef NOGG_SRC_UTIL_READ_H
#define NOGG_SRC_UTIL_READ_H

/*************************************************************************/
/*************************************************************************/

/**
 * fill_read_buf:  Fill the given handle's read buffer with as much data
 * from the stream as possible.
 *
 * On return from this function, the handle's read_buf_pos field will
 * always be set to zero.
 *
 * [Parameters]
 *     handle: Handle to operate on.
 * [Return value]
 *     True if the read buffer has at least one byte of valid data, false
 *     otherwise.
 */
extern int fill_read_buf(vorbis_t *handle);

/*************************************************************************/
/*************************************************************************/

#endif  // NOGG_SRC_UTIL_READ_H
