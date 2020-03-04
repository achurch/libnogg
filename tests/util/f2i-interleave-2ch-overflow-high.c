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

/* See below for why we directly call internal functions. */
#include "src/common.h"
#include "src/util/float-to-int16.h"


int main(void)
{
    vorbis_t *vorbis;
    EXPECT(vorbis = TEST___open_file("tests/data/sketch008.ogg",
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

    /* The interleaving function is only called with the size of the
     * audio frame, which will (except on a final truncated frame) always
     * be a multiple of the SIMD block size, so we make an exception to
     * our usual rule of testing only through libnogg APIs and call the
     * internal utility function directly to test the unoptimized path. */
    static const float data[4] = {0.0f, 2.0f, 0.0f, 2.0f};
    float_to_int16_interleave_2(pcm, &data[0], &data[2], 2);
    EXPECT_EQ(pcm[0], 0);
    EXPECT_EQ(pcm[1], 0);
    EXPECT_EQ(pcm[2], 32767);
    EXPECT_EQ(pcm[3], 32767);

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
