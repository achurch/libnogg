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
    EXPECT_TRUE(f = fopen("tests/data/split-packet.ogg", "rb"));
    EXPECT_EQ(fseek(f, 0, SEEK_END), 0);
    EXPECT_GT(size = ftell(f), 0);
    EXPECT_EQ(fseek(f, 0, SEEK_SET), 0);
    EXPECT_EQ(size, 0xEF4);
    EXPECT_TRUE(data = malloc(size));
    EXPECT_EQ(fread(data, 1, size, f), size);
    fclose(f);
    MODIFY(data[0xE76], 0x04, 0x00);
    MODIFY(data[0xE87], 0xFC, 0x28);
    MODIFY(data[0xE88], 0x1E, 0x28);
    MODIFY(data[0xE89], 0xB8, 0x88);
    MODIFY(data[0xE8A], 0x9D, 0xEA);

    vorbis_t *vorbis;
    EXPECT_TRUE(vorbis = vorbis_open_from_buffer(data, size, NULL));

    EXPECT_EQ(vorbis_length(vorbis), 1492);

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
