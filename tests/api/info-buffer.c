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
    void *data;
    long size;
    EXPECT_TRUE(f = fopen("tests/data/square.ogg", "rb"));
    EXPECT_EQ(fseek(f, 0, SEEK_END), 0);
    EXPECT_GT(size = ftell(f), 0);
    EXPECT_EQ(fseek(f, 0, SEEK_SET), 0);
    EXPECT_TRUE(data = malloc(size));
    EXPECT_EQ(fread(data, 1, size, f), size);
    fclose(f);

    vorbis_t *vorbis;
    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_TRUE(vorbis = vorbis_open_from_buffer(data, size, &error));
    EXPECT_EQ(error, VORBIS_NO_ERROR);

    EXPECT_EQ(vorbis_channels(vorbis), 1);
    EXPECT_EQ(vorbis_rate(vorbis), 4000);
    EXPECT_EQ(vorbis_length(vorbis), 40);

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}