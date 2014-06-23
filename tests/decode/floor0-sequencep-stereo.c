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
    vorbis_t *vorbis;
    EXPECT_TRUE(vorbis = vorbis_open_from_file(
                    "tests/data/sketch008-floor0.ogg", NULL));

    vorbis_seek(vorbis, 1000000);

    static const float expected_pcm[10] = {
         -0.31620693,
         -0.36503690,
         -0.31044254,
         -0.39839125,
         -0.31632990,
         -0.43871310,
         -0.34002236,
         -0.46303105,
         -0.36960804,
         -0.45965692,
    };
    float pcm[10];
    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 5, &error), 5);
    EXPECT_EQ(error, VORBIS_NO_ERROR);
    COMPARE_PCM_FLOAT(pcm, expected_pcm, 10);

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
