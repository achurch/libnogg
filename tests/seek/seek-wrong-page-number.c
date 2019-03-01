/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2019 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include "include/nogg.h"
#include "tests/common.h"

#include "tests/data/6-mode-bits_last10_float.h"  // Defines expected_pcm[].


int main(void)
{
    FILE *f;
    uint8_t *data;
    long size;
    EXPECT(f = fopen("tests/data/6-mode-bits-multipage.ogg", "rb"));
    EXPECT_EQ(fseek(f, 0, SEEK_END), 0);
    EXPECT_EQ(size = ftell(f), 0xE9B);
    EXPECT_EQ(fseek(f, 0, SEEK_SET), 0);
    EXPECT(data = malloc(size));
    EXPECT_EQ(fread(data, 1, size, f), size);
    fclose(f);
    MODIFY(data[0xE0B], 0x03, 0x04);
    MODIFY(data[0xE0F], 0x21, 0x0F);
    MODIFY(data[0xE10], 0xFD, 0xE7);
    MODIFY(data[0xE11], 0xFB, 0x16);
    MODIFY(data[0xE12], 0x9F, 0x12);
    MODIFY(data[0xE61], 0x04, 0x05);
    MODIFY(data[0xE65], 0xBE, 0x8E);
    MODIFY(data[0xE66], 0x3D, 0x97);
    MODIFY(data[0xE67], 0xD6, 0x40);
    MODIFY(data[0xE68], 0x18, 0x54);

    vorbis_t *vorbis;
    EXPECT(vorbis = vorbis_open_buffer(data, size, 0, NULL));

    EXPECT(vorbis_seek(vorbis, 700));
    float pcm[793];
    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 793, &error), 792);
    EXPECT_EQ(error, VORBIS_ERROR_STREAM_END);
    COMPARE_PCM_FLOAT(&pcm[782], expected_pcm, 10);

    vorbis_close(vorbis);
    free(data);
    return EXIT_SUCCESS;
}
