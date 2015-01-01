/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2015 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include "include/nogg.h"
#include "include/test.h"


int main(void)
{
    vorbis_t *vorbis;
    EXPECT_TRUE(vorbis = vorbis_open_file("tests/data/thingy.ogg", 0, NULL));
    EXPECT_TRUE(vorbis_seek(vorbis, 2944));

    static const float expected_pcm[10] = {
         0.05026849,
         0.04001303,
         0.02807940,
         0.01561053,
         0.00312608,
        -0.01011315,
        -0.02482782,
        -0.04148402,
        -0.06053595,
        -0.08025991,
    };
    float pcm[10];
    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 10, &error), 10);
    EXPECT_EQ(error, VORBIS_NO_ERROR);
    COMPARE_PCM_FLOAT(pcm, expected_pcm, 10);

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
