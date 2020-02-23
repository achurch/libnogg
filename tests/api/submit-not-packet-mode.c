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

#include "tests/data/square_float.h"  // Defines expected_pcm[].


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

    const int ofs_data0 = 0xA82;
    const int len_data0 = 0x3E;
    EXPECT_MEMEQ(data+ofs_data0-0x1D, "OggS", 4);
    EXPECT_EQ(data[ofs_data0-3], 2);
    EXPECT_EQ(data[ofs_data0-2], len_data0);

    vorbis_t *vorbis;
    EXPECT(vorbis = vorbis_open_file("tests/data/square.ogg", 0, NULL));

    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_FALSE(vorbis_submit_packet(vorbis, data+ofs_data0, len_data0,
                                      &error));
    EXPECT_EQ(error, VORBIS_ERROR_INVALID_OPERATION);

    vorbis_close(vorbis);
    free(data);
    return EXIT_SUCCESS;
}
