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
#include "src/decode/io.h"

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

//FIXME: inline?
uint8_t get8(stb_vorbis *handle)
{
    uint8_t byte;
    if ((*handle->read_callback)(handle->opaque, &byte, 1) != 1) {
        handle->eof = true;
        return 0;
    }
    return byte;
}

/*-----------------------------------------------------------------------*/

//FIXME: inline?
uint32_t get32(stb_vorbis *handle)
{
    uint32_t value;
    value  = get8(handle) <<  0;
    value |= get8(handle) <<  8;
    value |= get8(handle) << 16;
    value |= get8(handle) << 24;
    return value;
}

/*-----------------------------------------------------------------------*/

int getn(stb_vorbis *handle, uint8_t *buffer, int count)
{
    if ((*handle->read_callback)(handle->opaque, buffer, count) == count) {
        return 1;
    } else {
        handle->eof = true;
        return 0;
    }
}

/*-----------------------------------------------------------------------*/

void skip(stb_vorbis *handle, int count)
{
    if (handle->stream_len >= 0) {
        const int64_t current = (*handle->tell_callback)(handle->opaque);
        if (count > handle->stream_len - current) {
            count = handle->stream_len - current;
            handle->eof = true;
        }
        (*handle->seek_callback)(handle->opaque, current + count);
    } else {
        while (count > 0) {
            /* This is an arbitrary size to balance between number of
             * read calls and stack usage. */
            uint8_t buf[64];
            int n;
            if (count > (int)sizeof(buf)) {
                n = sizeof(buf);
            } else {
                n = count;
            }
            if (!getn(handle, buf, n)) {
                return;
            }
            count -= n;
        }
    }
}

/*-----------------------------------------------------------------------*/

void set_file_offset(stb_vorbis *handle, int64_t offset)
{
    if (handle->stream_len >= 0) {
        handle->eof = false;
        (*handle->seek_callback)(handle->opaque, offset);
    }
}

/*-----------------------------------------------------------------------*/

int64_t get_file_offset(stb_vorbis *handle)
{
    if (handle->stream_len >= 0) {
        return (*handle->tell_callback)(handle->opaque);
    } else {
        return 0;
    }
}

/*************************************************************************/
/*************************************************************************/
