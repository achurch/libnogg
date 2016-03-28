/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2016 Andrew Church <achurch@achurch.org>
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
                    "tests/data/thingy-floor0.ogg",
                    VORBIS_OPTION_DIVIDES_IN_CODEBOOK, NULL));
    EXPECT_TRUE(vorbis_seek(vorbis, 1000000));

    static const float expected_pcm[10] = {
         0.38940716,
         0.39152467,
         0.39281490,
         0.39335734,
         0.39319378,
         0.39232188,
         0.39069581,
         0.38823414,
         0.38483572,
         0.38039783,
    };
    float pcm[10];
    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 10, &error), 10);
    EXPECT_EQ(error, VORBIS_NO_ERROR);
    COMPARE_PCM_FLOAT(pcm, expected_pcm, 10);

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
