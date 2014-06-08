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
    EXPECT_FALSE(vorbis_open_from_buffer(
                     "OggS\0\2\0\0\0\0\0\0\0\0\1\0\0\0\0\0\0\0\0\0\0\0\1\x1E"
                     "\1vorbis\0\0\0\0\1\xA0\x0F\0\0\0\0\0\0\xFF\xFF\xFF\xFF"
                     "\0\0\0\0\x90\x01", 52, &error));
    EXPECT_EQ(error, VORBIS_ERROR_DECODE_SETUP_FAILURE);

    return EXIT_SUCCESS;
}
