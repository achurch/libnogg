/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#ifndef NOGG_SRC_DECODE_IO_H
#define NOGG_SRC_DECODE_IO_H

/*************************************************************************/
/*************************************************************************/

/**
 * get8:  Read a byte from the stream and return it as an 8-bit unsigned
 * integer.
 *
 * [Parameters]
 *     handle: Stream handle.
 * [Return value]
 *     Byte read, or 0 on EOF.
 */
extern uint8_t get8(stb_vorbis *handle);

/**
 * get32:  Read 4 bytes from the stream and return them as a 32-bit unsigned
 * integer.
 *
 * [Parameters]
 *     handle: Stream handle.
 * [Return value]
 *     Value read, or 0 on EOF.
 */
extern uint32_t get32(stb_vorbis *handle);

/**
 * get64:  Read 8 bytes from the stream and return them as a 64-bit unsigned
 * integer.
 *
 * [Parameters]
 *     handle: Stream handle.
 * [Return value]
 *     Value read, or 0 on EOF.
 */
extern uint64_t get64(stb_vorbis *handle);

/**
 * getn:  Read the given number of bytes from the stream and store them in
 * the given buffer.
 *
 * [Parameters]
 *     handle: Stream handle.
 *     buffer: Buffer into which to read data.
 *     count: Number of bytes to read.
 * [Return value]
 *     True on success, false on EOF.
 */
extern bool getn(stb_vorbis *handle, uint8_t *buffer, int count);

/**
 * skip:  Skip over the given number of bytes in the stream.
 *
 * [Parameters]
 *     handle: Stream handle.
 *     count: Number of bytes to skip.
 */
extern void skip(stb_vorbis *handle, int count);

/**
 * set_file_offset:  Set the stream read position to the given offset from
 * the beginning of the stream.  If the stream is not seekable, this
 * function does nothing.
 *
 * [Parameters]
 *     handle: Stream handle.
 *     offset: New stream read position, in bytes from the beginning of
 *         the stream.
 */
extern void set_file_offset(stb_vorbis *handle, int64_t offset);

/**
 * get_file_offset:  Return the current stream read position.
 *
 * [Parameters]
 *     handle: Stream handle.
 * [Return value]
 *     Current stream read position, in bytes from the beginning of the
 *     stream, or 0 if the stream is not seekable.
 */
extern int64_t get_file_offset(stb_vorbis *f);

/*************************************************************************/
/*************************************************************************/

#endif  // NOGG_SRC_DECODE_IO_H
