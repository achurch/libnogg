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
    MODIFY(data[0xE64], 0x03, 0x04);
    MODIFY(data[0xE83], 0x04, 0x05);

    vorbis_t *vorbis;
    EXPECT_TRUE(vorbis = vorbis_open_buffer(data, size, 0, NULL));

    float pcm[1621];
    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 1621, &error), 576);
    EXPECT_EQ(error, VORBIS_ERROR_DECODE_RECOVERED);
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 1621, &error), 1620-576);
    EXPECT_EQ(error, VORBIS_ERROR_STREAM_END);
    COMPARE_PCM_FLOAT(&pcm[1610-576], expected_pcm, 10);

    vorbis_close(vorbis);
    free(data);
    return EXIT_SUCCESS;
}
