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
    long size, buffer_size;
    EXPECT_TRUE(f = fopen("tests/data/split-packet.ogg", "rb"));
    EXPECT_EQ(fseek(f, 0, SEEK_END), 0);
    EXPECT_EQ(size = ftell(f), 0xEF4);
    EXPECT_EQ(fseek(f, 0, SEEK_SET), 0);
    buffer_size = 0x10E72L;
    EXPECT_TRUE(data = calloc(buffer_size, 1));
    EXPECT_EQ(fread(data, 1, size, f), size);
    fclose(f);

    vorbis_t *vorbis;
    EXPECT_TRUE(vorbis = vorbis_open_buffer(data, buffer_size, 0, NULL));

    EXPECT_EQ(vorbis_length(vorbis), -1);

    vorbis_close(vorbis);
    free(data);
    return EXIT_SUCCESS;
}
