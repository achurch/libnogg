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

#include "tests/data/6ch-moving-sine_int16.h"  // Defines expected_pcm[].


int main(void)
{
    vorbis_set_options(VORBIS_OPTION_READ_INT16_ONLY);

    vorbis_t *vorbis;
    EXPECT_TRUE(vorbis = vorbis_open_from_file(
                    "tests/data/6ch-moving-sine.ogg", NULL));

    int16_t pcm[3073*6];
    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_int16(vorbis, pcm, 3073, &error), 3072);
    EXPECT_EQ(error, VORBIS_ERROR_STREAM_END);
    COMPARE_PCM_INT16(pcm, expected_pcm, 3072);

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}