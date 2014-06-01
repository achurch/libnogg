/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#ifndef NOGG_SRC_UTIL_MEMORY_H
#define NOGG_SRC_UTIL_MEMORY_H

/*************************************************************************/
/*************************************************************************/

/**
 * malloc_channel_array:  Allocate an array of "channels" sub-arrays, with
 * each sub-array having "size" bytes of storage.  The entire set of arrays
 * can be freed by simply calling free() on the returned pointer.
 *
 * The return value is conceptually "<T> **", but the function is typed
 * as "void *" so the return value does not need an explicit cast to the
 * target data type.
 *
 * [Parameters]
 *     channels: Number of channels (sub-arrays) required.
 *     size: Number of bytes of storage to allocate for each sub-array.
 * [Return value]
 *     Pointer to the top-level array, or NULL on allocation failure.
 */
extern void *malloc_channel_array(int channels, int size);

/*************************************************************************/
/*************************************************************************/

#endif  // NOGG_SRC_UTIL_MEMORY_H
