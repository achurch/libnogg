/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2020 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include "include/nogg.h"
#include "tests/common.h"

#include "tests/data/square_int16.h"  // Defines expected_pcm[].


int main(void)
{
    vorbis_t *vorbis;
    EXPECT(vorbis = TEST___open_file("tests/data/square.ogg", 0, NULL));

    int16_t pcm[40];
    EXPECT_EQ(vorbis_read_int16(vorbis, pcm, 40, NULL), 40);
    COMPARE_PCM_INT16(pcm, expected_pcm, 40);
    EXPECT_EQ(vorbis_read_int16(vorbis, pcm, 40, NULL), 0);

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
