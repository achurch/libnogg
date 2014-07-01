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
                    "tests/data/thingy-floor0.ogg", NULL));
    EXPECT_TRUE(vorbis_seek(vorbis, 1000000));

    static const float expected_pcm[10] = {
         0.39523366,
         0.39807969,
         0.39972407,
         0.40024868,
         0.39974174,
         0.39833862,
         0.39622924,
         0.39359701,
         0.39052618,
         0.38695550,
    };
    float pcm[10];
    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 10, &error), 10);
    EXPECT_EQ(error, VORBIS_NO_ERROR);
    COMPARE_PCM_FLOAT(pcm, expected_pcm, 10);

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
