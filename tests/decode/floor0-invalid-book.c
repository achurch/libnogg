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


int main(void)
{
    FILE *f;
    uint8_t *data;
    long size;
    EXPECT_TRUE(f = fopen("tests/data/thingy-floor0.ogg", "rb"));
    EXPECT_EQ(fseek(f, 0, SEEK_END), 0);
    EXPECT_GT(size = ftell(f), 0);
    EXPECT_EQ(fseek(f, 0, SEEK_SET), 0);
    EXPECT_TRUE(data = malloc(size));
    EXPECT_EQ(fread(data, 1, size, f), size);
    fclose(f);
    MODIFY(data[0x2C89], 0x51, 0x71);

    vorbis_t *vorbis;
    EXPECT_TRUE(vorbis = vorbis_open_buffer(data, size, 0, NULL));

    static const float expected_pcm[] = {0.000019934998};
    float pcm[1];
    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 1, &error), 0);
    EXPECT_EQ(error, VORBIS_ERROR_DECODE_RECOVERED);
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 1, &error), 1);
    EXPECT_EQ(error, VORBIS_NO_ERROR);
    COMPARE_PCM_FLOAT(pcm, expected_pcm, 1);

    vorbis_close(vorbis);
    free(data);
    return EXIT_SUCCESS;
}
