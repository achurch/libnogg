/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2023 Andrew Church <achurch@achurch.org>
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
         -20415,
         -31346,
         -22146,
         -32767,  // -32924
         -23438,
         -32767,  // -33455
         -22443,
         -30839,
         -20226,
         -26650,
    };
    static const int16_t expected_pcm_l[10] = {
         -32630,
         -30023,
         -32767,  // -32977
         -29675,
         -32767,  // -33095
         -29306,
         -32767,  // -32828
         -28879,
         -32237,
         -28537,
    };
    int16_t pcm[10];
    vorbis_error_t error;

    EXPECT(vorbis_seek(vorbis, 528886));
    error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_int16(vorbis, pcm, 5, &error), 5);
    EXPECT_EQ(error, VORBIS_NO_ERROR);
    COMPARE_PCM_INT16(pcm, expected_pcm_r, 10);
    EXPECT(vorbis_seek(vorbis, 644810));
    error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_int16(vorbis, pcm, 5, &error), 5);
    EXPECT_EQ(error, VORBIS_NO_ERROR);
    COMPARE_PCM_INT16(pcm, expected_pcm_l, 10);

    /* The interleaving function is only called with the size of the
     * audio frame, which will (except on a final truncated frame) always
     * be a multiple of the SIMD block size, so we make an exception to
     * our usual rule of testing only through libnogg APIs and call the
     * internal utility function directly to test the unoptimized path. */
    ALIGN(64) static const float data0[3] = {0.0f, -1.0f, -2.0f};
    ALIGN(64) static const float data1[3] = {-2.0f, 0.0f, -1.0f};
    ALIGN(64) static int16_t pcm_aligned[6];
    float_to_int16_interleave_2(pcm_aligned, data0, data1, 3);
    EXPECT_EQ(pcm_aligned[0], 0);
    EXPECT_EQ(pcm_aligned[1], -32767);
    EXPECT_EQ(pcm_aligned[2], -32767);
    EXPECT_EQ(pcm_aligned[3], 0);
    EXPECT_EQ(pcm_aligned[4], -32767);
    EXPECT_EQ(pcm_aligned[5], -32767);

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
