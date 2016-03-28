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
    EXPECT_TRUE(f = fopen("tests/data/square.ogg", "rb"));
    EXPECT_EQ(fseek(f, 0, SEEK_END), 0);
    EXPECT_GT(size = ftell(f), 0);
    EXPECT_EQ(fseek(f, 0, SEEK_SET), 0);
    const long buffer_size = size + 31;
    EXPECT_TRUE(data = malloc(buffer_size));
    EXPECT_EQ(fread(data, 1, size, f), size);
    fclose(f);

    /* Insert 6 additional dummy modes to push the framing bit to a byte
     * boundary. */
    EXPECT_TRUE(memcmp(&data[0xA65], "OggS", 4) == 0);
    memmove(&data[0xA84], &data[0xA65], size-0xA65);
    memset(&data[0xA65], 0, 31);
    MODIFY(data[0x5F], 0xB5, 0xD4);
    MODIFY(data[0xA5E], 0, 6<<3);
    MODIFY(data[0xA64], 0x04, 0x00);
    MODIFY(data[0xA83], 0x00, 0x01);

    /* Doublecheck that we didn't accidentally break the file. */
    vorbis_t *vorbis;
    EXPECT_TRUE(vorbis = vorbis_open_buffer(data, buffer_size, 0, NULL));
    vorbis_close(vorbis);

    /* Shorten the setup packet by one byte, which will cause initialization
     * to fail. */
    MODIFY(data[0x5F], 0xD4, 0xD3);
    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_FALSE(vorbis_open_buffer(data, buffer_size, 0, &error));
    EXPECT_EQ(error, VORBIS_ERROR_DECODE_SETUP_FAILED);

    free(data);
    return EXIT_SUCCESS;
}
