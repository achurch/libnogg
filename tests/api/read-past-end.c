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

#define expected_pcm expected_pcm_float
#include "tests/data/square_float.h"
#undef expected_pcm

#define expected_pcm expected_pcm_int16
#include "tests/data/square_int16.h"
#undef expected_pcm


int main(void)
{
    vorbis_t *vorbis;
    float pcm_float[41];
    int16_t pcm_int16[41];
    vorbis_error_t error;

    EXPECT(vorbis = TEST___open_file("tests/data/square.ogg", 0, NULL));
    error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_float(vorbis, pcm_float, 41, &error), 40);
    EXPECT_EQ(error, VORBIS_ERROR_STREAM_END);
    COMPARE_PCM_FLOAT(pcm_float, expected_pcm_float, 40);
    vorbis_close(vorbis);

    EXPECT(vorbis = TEST___open_file("tests/data/square.ogg", 0, NULL));
    error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_int16(vorbis, pcm_int16, 41, &error), 40);
    EXPECT_EQ(error, VORBIS_ERROR_STREAM_END);
    COMPARE_PCM_INT16(pcm_int16, expected_pcm_int16, 40);
    vorbis_close(vorbis);

    return EXIT_SUCCESS;
}
