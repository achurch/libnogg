/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include "include/nogg.h"
#include "include/test.h"


int main(void)
{
    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_FALSE(vorbis_open_buffer(
                     "OggS\0\0\0\0\0\0\0\0\0\0\1\0\0\0\0\0\0\0\0\0\0\0\1\x1E",
                     28, 0, &error));
    EXPECT_EQ(error, VORBIS_ERROR_STREAM_INVALID);

    return EXIT_SUCCESS;
}
