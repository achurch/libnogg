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
    EXPECT_TRUE(vorbis = vorbis_open_from_file("tests/data/thingy.ogg", NULL));
    EXPECT_TRUE(vorbis_seek(vorbis, 3320000));

    static const int16_t expected_pcm[10] = {
        -24686,
        -26632,
        -28562,
        -30290,
        -31668,
        -32588,
        -32767,  // -32994
        -32767,  // -32908
        -32423,
        -31689,
    };
    int16_t pcm[10];
    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_int16(vorbis, pcm, 10, &error), 10);
    EXPECT_EQ(error, VORBIS_NO_ERROR);
    COMPARE_PCM_INT16(pcm, expected_pcm, 10);

    /* Also read one sample at a time to check the unoptimized code path. */
    EXPECT_TRUE(vorbis_seek(vorbis, 3320000));
    for (int i = 0; i < 10; i++) {
        error = (vorbis_error_t)-1;
        EXPECT_EQ(vorbis_read_int16(vorbis, pcm, 1, &error), 1);
        EXPECT_EQ(error, VORBIS_NO_ERROR);
        COMPARE_PCM_INT16(pcm, &expected_pcm[i], 1);
    }

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
