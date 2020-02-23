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
    EXPECT(vorbis = vorbis_open_file("tests/data/sketch008.ogg",
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
    static const float data[4] = {0.0f, -2.0f, 0.0f, -2.0f};
    float_to_int16_interleave_2(pcm, &data[0], &data[2], 2);
    EXPECT_EQ(pcm[0], 0);
    EXPECT_EQ(pcm[1], 0);
    EXPECT_EQ(pcm[2], -32767);
    EXPECT_EQ(pcm[3], -32767);

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
