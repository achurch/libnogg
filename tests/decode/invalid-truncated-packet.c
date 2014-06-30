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

#include "tests/data/6-mode-bits_last10_float.h"  // Defines expected_pcm[].


int main(void)
{
    vorbis_t *vorbis;
    EXPECT_TRUE(vorbis = vorbis_open_from_file(
                    "tests/data/6-mode-bits-short-packet.ogg", NULL));

    float pcm[1600];
    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 1600, &error), 448);
    EXPECT_EQ(error, VORBIS_ERROR_DECODE_RECOVERED);
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 1600, &error), 1600-448-128);
    EXPECT_EQ(error, VORBIS_ERROR_STREAM_END);
    COMPARE_PCM_FLOAT(&pcm[1600-448-128  // Samples read.
                           -128  // Beginning of the last packet.
                           +10], // Offset to the data to check.
                      expected_pcm, 10);

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
