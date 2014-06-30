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

uint8_t get8(stb_vorbis *handle)
{
    uint8_t byte;
    if (UNLIKELY((*handle->read_callback)(handle->opaque, &byte, 1) != 1)) {
        handle->eof = true;
        return 0;
    }
    return byte;
}

/*-----------------------------------------------------------------------*/

uint32_t get32(stb_vorbis *handle)
{
    uint8_t buf[4];
    if (UNLIKELY((*handle->read_callback)(handle->opaque,
                                          buf, sizeof(buf)) != sizeof(buf))) {
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
    if (UNLIKELY((*handle->read_callback)(handle->opaque,
                                          buf, sizeof(buf)) != sizeof(buf))) {
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
    if (UNLIKELY((*handle->read_callback)(handle->opaque,
                                          buffer, count) != count)) {
        handle->eof = true;
        return false;
    }
    return true;
}

/*************************************************************************/
/*************************************************************************/
