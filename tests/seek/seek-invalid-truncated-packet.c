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
    EXPECT_TRUE(f = fopen("tests/data/6-mode-bits-multipage.ogg", "rb"));
    EXPECT_EQ(fseek(f, 0, SEEK_END), 0);
    EXPECT_EQ(size = ftell(f), 0xE9B);
    EXPECT_EQ(fseek(f, 0, SEEK_SET), 0);
    EXPECT_TRUE(data = malloc(size));
    EXPECT_EQ(fread(data, 1, size, f), size);
    fclose(f);
    MODIFY(data[0xE0F], 0x21, 0xB4);
    MODIFY(data[0xE10], 0xFD, 0x2A);
    MODIFY(data[0xE11], 0xFB, 0x85);
    MODIFY(data[0xE12], 0x9F, 0xB5);
    MODIFY(data[0xE1B], 0x00, 0x02);

    vorbis_t *vorbis;
    EXPECT_TRUE(vorbis = vorbis_open_from_buffer(data, size, NULL));

    EXPECT_TRUE(vorbis_seek(vorbis, 700));
    float pcm[345];
    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 345, &error), 344);
    EXPECT_EQ(error, VORBIS_ERROR_STREAM_END);
    COMPARE_PCM_FLOAT(&pcm[334], expected_pcm, 10);

    vorbis_close(vorbis);
    free(data);
    return EXIT_SUCCESS;
}
