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

    const int ofs_id = 0x1C;
    const int len_id = 0x1E;
    const int ofs_setup = 0xB9;
    const int len_setup = 0x9AC;
    const int ofs_data0 = 0xA82;
    const int len_data0 = 0x3E;
    const int ofs_data1 = 0xAC0;
    const int len_data1 = 0x25;
    EXPECT_MEMEQ(data+ofs_id, "\x01vorbis", 7);
    EXPECT_MEMEQ(data+ofs_setup, "\x05vorbis", 7);
    EXPECT_MEMEQ(data+ofs_data0-0x1D, "OggS", 4);
    EXPECT_EQ(data[ofs_data0-3], 2);
    EXPECT_EQ(data[ofs_data0-2], len_data0);
    EXPECT_EQ(data[ofs_data0-1], len_data1);

    vorbis_t *vorbis;
    vorbis_callbacks_t callbacks = {.malloc = NULL, .free = NULL};
    EXPECT(vorbis = vorbis_open_packet(data+ofs_id, len_id,
                                       data+ofs_setup, len_setup,
                                       callbacks, NULL, 0, NULL));

    vorbis_error_t error = (vorbis_error_t)-1;
    float pcm[257];
    EXPECT(vorbis_submit_packet(vorbis, data+ofs_data0, len_data0, &error));
    EXPECT_EQ(error, VORBIS_NO_ERROR);
    error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 1, &error), 0);
    EXPECT_EQ(error, VORBIS_ERROR_STREAM_END);

    data[ofs_data1] |= 0x01;  // Set the "non-audio" flag to trigger an error.
    error = (vorbis_error_t)-1;
    EXPECT_FALSE(vorbis_submit_packet(vorbis, data+ofs_data1, len_data1,
                                      &error));
    EXPECT_EQ(error, VORBIS_ERROR_DECODE_RECOVERED);

    /* A decoding error in a single packet should not prevent subsequent
     * packets from being decoded. */
    data[ofs_data1] &= ~0x01;
    error = (vorbis_error_t)-1;
    EXPECT(vorbis_submit_packet(vorbis, data+ofs_data1, len_data1, &error));
    EXPECT_EQ(error, VORBIS_NO_ERROR);
    error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 257, &error), 256);
    EXPECT_EQ(error, VORBIS_ERROR_STREAM_END);
    COMPARE_PCM_FLOAT(pcm, expected_pcm, 40);

    vorbis_close(vorbis);
    free(data);
    return EXIT_SUCCESS;
}
