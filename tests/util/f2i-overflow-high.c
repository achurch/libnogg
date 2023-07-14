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


int main(void)
{
    vorbis_t *vorbis;
    EXPECT(vorbis = TEST___open_file("tests/data/sketch008.ogg", 0, NULL));
    EXPECT(vorbis_seek(vorbis, 803595));

    static const int16_t expected_pcm[10] = {
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
    int16_t pcm[10];
    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_int16(vorbis, pcm, 5, &error), 5);
    EXPECT_EQ(error, VORBIS_NO_ERROR);
    COMPARE_PCM_INT16(pcm, expected_pcm, 10);

    /* Also read one sample at a time to check the unoptimized code path. */
    EXPECT(vorbis_seek(vorbis, 803595));
    for (int i = 0; i < 5; i++) {
        error = (vorbis_error_t)-1;
        EXPECT_EQ(vorbis_read_int16(vorbis, pcm, 1, &error), 1);
        EXPECT_EQ(error, VORBIS_NO_ERROR);
        COMPARE_PCM_INT16(pcm, &expected_pcm[i*2], 2);
    }

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
