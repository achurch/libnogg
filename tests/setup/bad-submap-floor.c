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
    EXPECT_TRUE(f = fopen("tests/data/square.ogg", "rb"));
    EXPECT_EQ(fseek(f, 0, SEEK_END), 0);
    EXPECT_GT(size = ftell(f), 0);
    EXPECT_EQ(fseek(f, 0, SEEK_SET), 0);
    EXPECT_TRUE(data = malloc(size));
    EXPECT_EQ(fread(data, 1, size, f), size);
    fclose(f);
    MODIFY(data[0xA5C], 0x00, 0xF8);
    MODIFY(data[0xA5D], 0x00, 0x07);

    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_FALSE(vorbis_open_from_buffer(data, size, &error));
    EXPECT_EQ(error, VORBIS_ERROR_DECODE_SETUP_FAILED);

    free(data);
    return EXIT_SUCCESS;
}