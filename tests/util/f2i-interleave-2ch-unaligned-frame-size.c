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
    vorbis_set_options(VORBIS_OPTION_READ_INT16_ONLY);

    vorbis_t *vorbis;
    EXPECT_TRUE(vorbis = vorbis_open_from_file(
                    "tests/data/sketch008.ogg", NULL));
    EXPECT_TRUE(vorbis_seek(vorbis, 6298410));

    static const int16_t expected_pcm[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    int16_t pcm[20];
    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_int16(vorbis, pcm, 10, &error), 9);
    EXPECT_EQ(error, VORBIS_ERROR_STREAM_END);
    COMPARE_PCM_INT16(pcm, expected_pcm, 9);

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}