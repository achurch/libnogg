/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2024 Andrew Church <achurch@achurch.org>
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
    EXPECT(vorbis = TEST___open_file("tests/data/noise-6ch.ogg",
                                     VORBIS_OPTION_READ_INT16_ONLY, NULL));

    static const int16_t expected_pcm[18] = {
        23482,
        25768,
        30349,
        32767,  // 33211
        32767,  // 34224
        1422,
        -16848,
        -3985,
        -6110,
        73,
        -1233,
        1406,
        -32767,  // -36226
        -13660,
        -21048,
        -15671,
        -17939,
        1388,
    };
    int16_t pcm[18];
    vorbis_error_t error;

    EXPECT(vorbis_seek(vorbis, 3473));
    error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_int16(vorbis, pcm, 3, &error), 3);
    EXPECT_EQ(error, VORBIS_NO_ERROR);
    COMPARE_PCM_INT16(pcm, expected_pcm, 18);

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
