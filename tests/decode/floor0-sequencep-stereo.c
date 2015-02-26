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


int main(void)
{
    vorbis_t *vorbis;
    EXPECT_TRUE(vorbis = vorbis_open_file(
                    "tests/data/sketch008-floor0.ogg", 0, NULL));
    EXPECT_TRUE(vorbis_seek(vorbis, 1000000));

    static const float expected_pcm[10] = {
        -0.026770104,
         0.10745968,
        -0.029321522,
         0.11029746,
        -0.019971853,
         0.11230789,
        -0.0012615155,
         0.11379874,
         0.018812388,
         0.11551876,
    };
    float pcm[10];
    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 5, &error), 5);
    EXPECT_EQ(error, VORBIS_NO_ERROR);
    COMPARE_PCM_FLOAT(pcm, expected_pcm, 10);

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
