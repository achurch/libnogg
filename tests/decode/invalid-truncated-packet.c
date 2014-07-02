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

#include "tests/data/6-mode-bits_last10_float.h"  // Defines expected_pcm[].


int main(void)
{
    FILE *f;
    uint8_t *data;
    long size;
    EXPECT_TRUE(f = fopen("tests/data/6-mode-bits.ogg", "rb"));
    EXPECT_EQ(fseek(f, 0, SEEK_END), 0);
    EXPECT_GT(size = ftell(f), 0);
    EXPECT_EQ(fseek(f, 0, SEEK_SET), 0);
    EXPECT_EQ(size, 0xE65);
    EXPECT_TRUE(data = malloc(size));
    EXPECT_EQ(fread(data, 1, size, f), size);
    fclose(f);
    MODIFY(data[0xDEE], 0x8D, 0x5B);
    MODIFY(data[0xDEF], 0xEC, 0x31);
    MODIFY(data[0xDF0], 0x92, 0x9F);
    MODIFY(data[0xDF1], 0x29, 0xC5);
    MODIFY(data[0xE00], 0x00, 0x02);

    vorbis_t *vorbis;
    EXPECT_TRUE(vorbis = vorbis_open_from_buffer(data, size, NULL));

    float pcm[1600];
    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 1600, &error), 448);
    EXPECT_EQ(error, VORBIS_ERROR_DECODE_RECOVERED);
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 1600, &error), 1600-448-128);
    EXPECT_EQ(error, VORBIS_ERROR_STREAM_END);
    COMPARE_PCM_FLOAT(&pcm[1600-448-128  // Samples read.
                           -128  // Beginning of the last packet.
                           +10], // Offset to the data to check.
                      expected_pcm, 10);

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
