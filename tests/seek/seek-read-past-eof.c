/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2016 Andrew Church <achurch@achurch.org>
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
    EXPECT_TRUE(f = fopen("tests/data/split-packet.ogg", "rb"));
    EXPECT_EQ(fseek(f, 0, SEEK_END), 0);
    EXPECT_EQ(size = ftell(f), 0xEF4);
    EXPECT_EQ(fseek(f, 0, SEEK_SET), 0);
    EXPECT_TRUE(data = malloc(size));
    EXPECT_EQ(fread(data, 1, size, f), size);
    fclose(f);
    MODIFY(data[0xE78], 0x05, 0x15);
    MODIFY(data[0xE87], 0xFC, 0x94);
    MODIFY(data[0xE88], 0x1E, 0xB5);
    MODIFY(data[0xE89], 0xB8, 0xA3);
    MODIFY(data[0xE8A], 0x9D, 0xC9);

    vorbis_t *vorbis;
    EXPECT_TRUE(vorbis = vorbis_open_buffer(data, size, 0, NULL));

    /* The wrong sample position we wrote to the last page will confuse
     * the seek code into trying to read from the second packet, and this
     * sample index will force it to read past the end of the file. */
    EXPECT_FALSE(vorbis_seek(vorbis, 3000));

    vorbis_close(vorbis);
    free(data);
    return EXIT_SUCCESS;
}
