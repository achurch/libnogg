/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2019 Andrew Church <achurch@achurch.org>
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
    EXPECT(vorbis = vorbis_open_file("tests/data/sketch008.ogg",
                                     VORBIS_OPTION_READ_INT16_ONLY, NULL));

    static const int16_t expected_pcm_r[10] = {
         28373,
         32767,  // 32845
         28313,
         32767,  // 33293
         27718,
         32767,  // 33231
         26736,
         32712,
         25696,
         32028,
    };
    static const int16_t expected_pcm_l[10] = {
         32103,
         26106,
         32629,
         26270,
         32767,  // 32825
         26261,
         32748,
         26295,
         32363,
         26335,
    };
    int16_t pcm[10];
    vorbis_error_t error;

    EXPECT(vorbis_seek(vorbis, 803595));
    error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_int16(vorbis, pcm, 5, &error), 5);
    EXPECT_EQ(error, VORBIS_NO_ERROR);
    COMPARE_PCM_INT16(pcm, expected_pcm_r, 10);
    EXPECT(vorbis_seek(vorbis, 1623272));
    error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_int16(vorbis, pcm, 5, &error), 5);
    EXPECT_EQ(error, VORBIS_NO_ERROR);
    COMPARE_PCM_INT16(pcm, expected_pcm_l, 10);

    /* Also read one sample at a time to check the unoptimized code path. */
    EXPECT(vorbis_seek(vorbis, 803595));
    for (int i = 0; i < 5; i++) {
        error = (vorbis_error_t)-1;
        EXPECT_EQ(vorbis_read_int16(vorbis, pcm, 1, &error), 1);
        EXPECT_EQ(error, VORBIS_NO_ERROR);
        COMPARE_PCM_INT16(pcm, &expected_pcm_r[i*2], 2);
    }
    EXPECT(vorbis_seek(vorbis, 1623272));
    for (int i = 0; i < 5; i++) {
        error = (vorbis_error_t)-1;
        EXPECT_EQ(vorbis_read_int16(vorbis, pcm, 1, &error), 1);
        EXPECT_EQ(error, VORBIS_NO_ERROR);
        COMPARE_PCM_INT16(pcm, &expected_pcm_l[i*2], 2);
    }

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
