/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2015 Andrew Church <achurch@achurch.org>
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
                    "tests/data/sketch008.ogg",
                    VORBIS_OPTION_READ_INT16_ONLY, NULL));
    EXPECT_TRUE(vorbis_seek(vorbis, 6298410));

    static const int16_t expected_pcm[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0,
                                             0, 0, 0, 0, 0, 0, 0, 0, 0,
                                             -1, -1};
    int16_t pcm[20];
    for (int i = 0; i < 20; i++) {
        pcm[i] = -1;
    }
    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_int16(vorbis, pcm, 10, &error), 9);
    EXPECT_EQ(error, VORBIS_ERROR_STREAM_END);
    COMPARE_PCM_INT16(pcm, expected_pcm, 20);

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
