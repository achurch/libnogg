/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2015 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

/* Tests handling of a codebook containing a single symbol encoded in
 * non-ordered sparse mode. */

#include "include/nogg.h"
#include "include/test.h"

#include "tests/data/noise-6ch_float.h"  // Defines expected_pcm[].


int main(void)
{
    vorbis_t *vorbis;
    EXPECT_TRUE(vorbis = vorbis_open_file("tests/data/1bit-1code-sparse.ogg",
                                          0, NULL));

    float pcm[8501*6];
    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 8501, &error), 8500);
    EXPECT_EQ(error, VORBIS_ERROR_STREAM_END);
    COMPARE_PCM_FLOAT(pcm, expected_pcm, 8500*6);

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
