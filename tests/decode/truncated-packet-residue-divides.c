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


int main(void)
{
    FILE *f;
    uint8_t *data;
    long size;
    EXPECT_TRUE(f = fopen("tests/data/long-short.ogg", "rb"));
    EXPECT_EQ(fseek(f, 0, SEEK_END), 0);
    EXPECT_GT(size = ftell(f), 0);
    EXPECT_EQ(fseek(f, 0, SEEK_SET), 0);
    EXPECT_TRUE(data = malloc(size));
    EXPECT_EQ(fread(data, 1, size, f), size);
    fclose(f);

    vorbis_set_options(VORBIS_OPTION_DIVIDES_IN_RESIDUE);

    vorbis_t *vorbis;
    float pcm[1493];
    vorbis_error_t error;

    /* Truncate to a point inside a residue partition. */
    MODIFY(data[0xD56], 0x2E, 0x11);
    memmove(&data[0xD73], &data[0xD90], size-0xD90);
    size -= 0x1D;
    EXPECT_TRUE(vorbis = vorbis_open_from_buffer(data, size, NULL));
    error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 1493, &error), 1492);
    EXPECT_EQ(error, VORBIS_ERROR_STREAM_END);
    vorbis_close(vorbis);

    /* Truncate to a classification code. */
    MODIFY(data[0xD56], 0x11, 0x10);
    memmove(&data[0xD72], &data[0xD73], size-0xD73);
    size -= 1;
    EXPECT_TRUE(vorbis = vorbis_open_from_buffer(data, size, NULL));
    error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 1493, &error), 1492);
    EXPECT_EQ(error, VORBIS_ERROR_STREAM_END);
    vorbis_close(vorbis);

    free(data);
    return EXIT_SUCCESS;
}
