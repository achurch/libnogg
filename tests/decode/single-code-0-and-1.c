/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2016 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

/* Tests that for a stream with a codebook containing a single symbol,
 * both possible codes (0 and 1) decode to that symbol. */

#include "include/nogg.h"
#include "tests/common.h"

#include "tests/data/noise-6ch_float.h"  // Defines expected_pcm[].


int main(void)
{
    FILE *f;
    uint8_t *data;
    long size;
    EXPECT_TRUE(f = fopen("tests/data/single-code-sparse.ogg", "rb"));
    EXPECT_EQ(fseek(f, 0, SEEK_END), 0);
    EXPECT_EQ(size = ftell(f), 0x3AE6);
    EXPECT_EQ(fseek(f, 0, SEEK_SET), 0);
    EXPECT_TRUE(data = malloc(size));
    EXPECT_EQ(fread(data, 1, size, f), size);
    fclose(f);
    MODIFY(data[0x3117], 0x8E, 0x8E);  // Encoded as 0 (bit 4).
    MODIFY(data[0x3402], 0x16, 0x56);  // Encoded as 1 (bit 6).

    vorbis_t *vorbis;
    EXPECT_TRUE(vorbis = vorbis_open_buffer(data, size, 0, NULL));

    float pcm[8501*6];
    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 8501, &error), 8500);
    EXPECT_EQ(error, VORBIS_ERROR_STREAM_END);
    COMPARE_PCM_FLOAT(pcm, expected_pcm, 8500*6);

    vorbis_close(vorbis);
    free(data);
    return EXIT_SUCCESS;
}
