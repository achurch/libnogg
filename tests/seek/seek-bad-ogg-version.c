/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2020 Andrew Church <achurch@achurch.org>
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
    MODIFY(data[0xDFD], 0x00, 0x01);
    MODIFY(data[0xE0F], 0x21, 0xFA);
    MODIFY(data[0xE10], 0xFD, 0xE7);
    MODIFY(data[0xE11], 0xFB, 0xBD);
    MODIFY(data[0xE12], 0x9F, 0x06);

    vorbis_t *vorbis;
    EXPECT(vorbis = vorbis_open_buffer(data, size, 0, NULL));

    EXPECT_FALSE(vorbis_seek(vorbis, 700));

    vorbis_close(vorbis);
    free(data);
    return EXIT_SUCCESS;
}
