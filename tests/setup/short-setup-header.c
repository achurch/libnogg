/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2015 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include "include/nogg.h"
#include "tests/common.h"


static const char data[107] =
    "OggS\0\2\0\0\0\0\0\0\0\0\1\0\0\0\0\0\0\0\0\0\0\0\1\x1E"
    "\1vorbis\0\0\0\0\1\xA0\x0F\0\0\0\0\0\0\xFF\xFF\xFF\xFF"
    "\0\0\0\0\x99\x01"
    "OggS\0\0\0\0\0\0\0\0\0\0\2\0\0\0\0\0\0\0\0\0\0\0\2\x10\4"
    "\3vorbis\0\0\0\0\0\0\0\0\1"
    "\5vor";

int main(void)
{
    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_FALSE(vorbis_open_buffer(data, sizeof(data), 0, &error));
    EXPECT_EQ(error, VORBIS_ERROR_STREAM_INVALID);

    return EXIT_SUCCESS;
}
