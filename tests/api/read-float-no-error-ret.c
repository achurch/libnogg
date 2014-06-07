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

#include "tests/data/square_float.h"  // Defines expected_pcm[].


int main(void)
{
    vorbis_t *vorbis;
    EXPECT_TRUE(vorbis = vorbis_open_from_file("tests/data/square.ogg", NULL));

    float pcm[40];
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 40, NULL), 40);
    COMPARE_PCM_FLOAT(pcm, expected_pcm, 40);
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 40, NULL), 0);

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}