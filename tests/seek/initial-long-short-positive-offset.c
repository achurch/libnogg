/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2022 Andrew Church <achurch@achurch.org>
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
    EXPECT(f = fopen("tests/data/split-packet.ogg", "rb"));
    EXPECT_EQ(fseek(f, 0, SEEK_END), 0);
    EXPECT_EQ(size = ftell(f), 0xEF4);
    EXPECT_EQ(fseek(f, 0, SEEK_SET), 0);
    EXPECT(data = malloc(size));
    EXPECT_EQ(fread(data, 1, size, f), size);
    fclose(f);
    MODIFY(data[0xD39], 0x40, 0x60);
    MODIFY(data[0xD49], 0x0E, 0xDC);
    MODIFY(data[0xD4A], 0xC1, 0xD9);
    MODIFY(data[0xD4B], 0xAF, 0xA0);
    MODIFY(data[0xD4C], 0x9A, 0x8C);
    MODIFY(data[0xE58], 0x40, 0x60);
    MODIFY(data[0xE68], 0x0B, 0xD4);
    MODIFY(data[0xE69], 0xB4, 0x09);
    MODIFY(data[0xE6A], 0x51, 0x03);
    MODIFY(data[0xE6B], 0xE2, 0x3D);
    MODIFY(data[0xE77], 0xD4, 0xF4);
    MODIFY(data[0xE87], 0xFC, 0x4A);
    MODIFY(data[0xE88], 0x1E, 0x75);
    MODIFY(data[0xE89], 0xB8, 0xC9);
    MODIFY(data[0xE8A], 0x9D, 0x7D);

    vorbis_t *vorbis;
    EXPECT(vorbis = vorbis_open_buffer(data, size, 0, NULL));

    EXPECT(vorbis_seek(vorbis, 10));
    float pcm[1472];
    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 1472, &error), 1472);
    EXPECT_EQ(error, VORBIS_NO_ERROR);
    error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 11, &error), 10);
    EXPECT_EQ(error, VORBIS_ERROR_STREAM_END);
    COMPARE_PCM_FLOAT(pcm, expected_pcm, 10);

    vorbis_close(vorbis);
    free(data);
    return EXIT_SUCCESS;
}
