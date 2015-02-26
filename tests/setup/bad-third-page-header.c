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
    long size;
    EXPECT_TRUE(f = fopen("tests/data/square.ogg", "rb"));
    EXPECT_EQ(fseek(f, 0, SEEK_END), 0);
    EXPECT_GT(size = ftell(f), 0);
    EXPECT_EQ(fseek(f, 0, SEEK_SET), 0);
    const long buffer_size = size + 27;
    EXPECT_TRUE(data = malloc(buffer_size));
    EXPECT_EQ(fread(data, 1, size, f), size);
    fclose(f);

    /* Split the setup header onto a third page. */
    EXPECT_TRUE(memcmp(&data[0x3A], "OggS", 4) == 0);
    const int num_segments = data[0x54];
    const int first_seg_length = data[0x55];
    const int last_seg_length = data[0x54 + num_segments];
    memmove(&data[0x55 + num_segments + first_seg_length + 27],
            &data[0x55 + num_segments + first_seg_length],
            size - (0x55 + num_segments + first_seg_length));
    data[0x54] = 1;
    memmove(&data[0x56], &data[0x55 + num_segments], first_seg_length);
    memcpy(&data[0x56 + first_seg_length], &data[0x3A], 27);
    data[0x56 + first_seg_length + 18] = 2;  // Page number.
    data[0x56 + first_seg_length + 26] = num_segments-1;
    memset(&data[0x56 + first_seg_length + 27], 0xFF, num_segments-2);
    data[0x56 + first_seg_length + 27 + (num_segments-2)] = last_seg_length;

    /* Doublecheck that we didn't accidentally break the file. */
    vorbis_t *vorbis;
    EXPECT_TRUE(vorbis = vorbis_open_buffer(data, buffer_size, 0, NULL));
    vorbis_close(vorbis);

    /* Set the "continued packet" flag on the third page, which will cause
     * start_packet() to fail. */
    MODIFY(data[0x56 + first_seg_length + 5], 0x00, 0x01);
    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_FALSE(vorbis_open_buffer(data, buffer_size, 0, &error));
    EXPECT_EQ(error, VORBIS_ERROR_DECODE_SETUP_FAILED);

    free(data);
    return EXIT_SUCCESS;
}
