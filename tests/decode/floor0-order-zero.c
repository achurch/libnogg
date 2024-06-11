/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2024 Andrew Church <achurch@achurch.org>
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
    EXPECT(f = fopen("tests/data/6ch-moving-sine-floor0.ogg", "rb"));
    EXPECT_EQ(fseek(f, 0, SEEK_END), 0);
    EXPECT_GT(size = ftell(f), 0);
    EXPECT_EQ(fseek(f, 0, SEEK_SET), 0);
    EXPECT(data = malloc(size));
    EXPECT_EQ(fread(data, 1, size, f), size);
    fclose(f);
    /* Zero the "order" field of both floor definitions. */
    MODIFY(data[0x1A14], 0x40, 0x00);
    MODIFY(data[0x1A15], 0x02, 0x00);
    MODIFY(data[0x1A20], 0x1E, 0x00);

    vorbis_t *vorbis;
    EXPECT(vorbis = vorbis_open_buffer(data, size, 0, NULL));

    float pcm[257*6];
    vorbis_error_t error;
    for (int i = 0; i < 9; i++) {
        error = (vorbis_error_t)-1;
        const int expected_len = (i==3 || i==6) ? 256 : 0;
        EXPECT_EQ(vorbis_read_float(vorbis, pcm, 257, &error), expected_len);
        EXPECT_EQ(error, VORBIS_ERROR_DECODE_RECOVERED);
    }
    error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 257, &error), 256);
    EXPECT_EQ(error, VORBIS_ERROR_STREAM_END);

    vorbis_close(vorbis);
    free(data);
    return EXIT_SUCCESS;
}
