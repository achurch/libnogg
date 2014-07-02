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

#include "tests/data/long-short_last10_float.h"  // Defines expected_pcm[].


int main(void)
{
    FILE *f;
    uint8_t *data;
    long size;
    EXPECT_TRUE(f = fopen("tests/data/split-packet.ogg", "rb"));
    EXPECT_EQ(fseek(f, 0, SEEK_END), 0);
    EXPECT_EQ(size = ftell(f), 0xEF4);
    EXPECT_EQ(fseek(f, 0, SEEK_SET), 0);
    EXPECT_TRUE(data = malloc(size));
    EXPECT_EQ(fread(data, 1, size, f), size);
    fclose(f);
    MODIFY(data[0xE58], 0x40, 0xFF);
    MODIFY(data[0xE59], 0x03, 0xFF);
    MODIFY(data[0xE5A], 0x00, 0xFF);
    MODIFY(data[0xE5B], 0x00, 0xFF);
    MODIFY(data[0xE5C], 0x00, 0xFF);
    MODIFY(data[0xE5D], 0x00, 0xFF);
    MODIFY(data[0xE5E], 0x00, 0xFF);
    MODIFY(data[0xE5F], 0x00, 0xFF);
    MODIFY(data[0xE68], 0x0B, 0xC1);
    MODIFY(data[0xE69], 0xB4, 0xA7);
    MODIFY(data[0xE6A], 0x51, 0x16);
    MODIFY(data[0xE6B], 0xE2, 0x83);

    vorbis_t *vorbis;
    EXPECT_TRUE(vorbis = vorbis_open_from_buffer(data, size, NULL));

    float pcm[1493];
    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 1493, &error), 1492);
    EXPECT_EQ(error, VORBIS_ERROR_STREAM_END);
    COMPARE_PCM_FLOAT(&pcm[1482], expected_pcm, 10);

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
