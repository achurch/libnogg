/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2024 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include "include/nogg.h"
#define PCM_FLOAT_ERROR  (1.0e-4f)  // Significant variation with this stream.
#include "tests/common.h"

#include "tests/data/6ch-moving-sine-floor0_float.h"  // Defines expected_pcm[].


int main(void)
{
    vorbis_t *vorbis;
    EXPECT(vorbis = TEST___open_file("tests/data/6ch-moving-sine-floor0.ogg",
                                     0, NULL));

    static float pcm[3073*6];  // Might be too big for the stack.
    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 3073, &error), 3072);
    EXPECT_EQ(error, VORBIS_ERROR_STREAM_END);
    COMPARE_PCM_FLOAT(pcm, expected_pcm, 3072*6);

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
