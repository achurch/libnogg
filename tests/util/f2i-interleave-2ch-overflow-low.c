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

    /* Also read one sample at a time to check the unoptimized code path. */
    EXPECT(vorbis_seek(vorbis, 528886));
    for (int i = 0; i < 5; i++) {
        error = (vorbis_error_t)-1;
        EXPECT_EQ(vorbis_read_int16(vorbis, pcm, 1, &error), 1);
        EXPECT_EQ(error, VORBIS_NO_ERROR);
        COMPARE_PCM_INT16(pcm, &expected_pcm_r[i*2], 2);
    }
    EXPECT(vorbis_seek(vorbis, 644810));
    for (int i = 0; i < 5; i++) {
        error = (vorbis_error_t)-1;
        EXPECT_EQ(vorbis_read_int16(vorbis, pcm, 1, &error), 1);
        EXPECT_EQ(error, VORBIS_NO_ERROR);
        COMPARE_PCM_INT16(pcm, &expected_pcm_l[i*2], 2);
    }

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
