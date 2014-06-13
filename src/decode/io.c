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

uint32_t get32(stb_vorbis *handle)
{
    uint8_t buf[4];
    if ((*handle->read_callback)(handle->opaque,
                                 buf, sizeof(buf)) != sizeof(buf)) {
        handle->eof = true;
        return 0;
    }
    return (uint32_t)buf[0] <<  0
         | (uint32_t)buf[1] <<  8
         | (uint32_t)buf[2] << 16
         | (uint32_t)buf[3] << 24;
}

/*-----------------------------------------------------------------------*/

uint64_t get64(stb_vorbis *handle)
{
    uint8_t buf[8];
    if ((*handle->read_callback)(handle->opaque,
                                 buf, sizeof(buf)) != sizeof(buf)) {
        handle->eof = true;
        return 0;
    }
    return (uint32_t)buf[0] <<  0
         | (uint32_t)buf[1] <<  8
         | (uint32_t)buf[2] << 16
         | (uint32_t)buf[3] << 24
         | (uint64_t)buf[4] << 32
         | (uint64_t)buf[5] << 40
         | (uint64_t)buf[6] << 48
         | (uint64_t)buf[7] << 56;
}

/*-----------------------------------------------------------------------*/

bool getn(stb_vorbis *handle, uint8_t *buffer, int count)
{
    if ((*handle->read_callback)(handle->opaque, buffer, count) == count) {
        return true;
    } else {
        handle->eof = true;
        return false;
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
