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

#include "tests/data/long-short_last10_float.h"  // Defines expected_pcm[].


int main(void)
{
    vorbis_t *vorbis;
    EXPECT(vorbis = TEST___open_file("tests/data/long-short.ogg", 0, NULL));

    float pcm[1482];
    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 1482, &error), 1482);
    EXPECT_EQ(error, VORBIS_NO_ERROR);
    error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 11, &error), 10);
    EXPECT_EQ(error, VORBIS_ERROR_STREAM_END);
    COMPARE_PCM_FLOAT(pcm, expected_pcm, 10);

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
