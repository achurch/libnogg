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
    EXPECT(f = fopen("tests/data/square.ogg", "rb"));
    EXPECT_EQ(fseek(f, 0, SEEK_END), 0);
    EXPECT_GT(size = ftell(f), 0);
    EXPECT_EQ(fseek(f, 0, SEEK_SET), 0);
    EXPECT(data = malloc(size));
    EXPECT_EQ(fread(data, 1, size, f), size);
    fclose(f);

    const int ofs_id = 0x1C;
    const int len_id = 0x1E;
    const int ofs_setup = 0xB9;
    const int len_setup = 0x9AC;
    EXPECT_MEMEQ(data+ofs_id, "\x01vorbis", 7);
    EXPECT_MEMEQ(data+ofs_setup, "\x05vorbis", 7);

    vorbis_t *vorbis;
    vorbis_callbacks_t callbacks = {.malloc = NULL, .free = NULL};
    EXPECT(vorbis = vorbis_open_packet(data+ofs_id, len_id,
                                       data+ofs_setup, len_setup,
                                       callbacks, NULL, 0, NULL));

    EXPECT_EQ(vorbis_channels(vorbis), 1);
    EXPECT_EQ(vorbis_rate(vorbis), 4000);
    EXPECT_EQ(vorbis_length(vorbis), -1);
    EXPECT_EQ(vorbis_bitrate(vorbis), 0);

    vorbis_close(vorbis);
    free(data);
    return EXIT_SUCCESS;
}
