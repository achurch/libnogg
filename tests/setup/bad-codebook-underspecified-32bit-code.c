/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2022 Andrew Church <achurch@achurch.org>
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
    /* Reconfigure codebook 4 so it's underspecified by one 32-bit code. */
    MODIFY(data[0x1A9], 0x49, 0x39);  // 33: 5 -> 4
    MODIFY(data[0x1AA], 0x92, 0x8E);  // 33: 5 -> 4, 34: 5 -> 4
    MODIFY(data[0x1AB], 0x64, 0x23);  // 35: 5 -> 4, 36: 6 -> 5
    MODIFY(data[0x1AD], 0x96, 0x9A);  // 38: 6 -> 7
    MODIFY(data[0x1AE], 0x65, 0x27);  // 39: 6 -> 8, 40: 6 -> 9
    MODIFY(data[0x1AF], 0x59, 0x9A);  // 40: 6 -> 9, 41: 6 -> 10
    MODIFY(data[0x1B0], 0x96, 0xAA);  // 41: 6 -> 10, 42: 6 -> 11
    MODIFY(data[0x1B1], 0xA6, 0x2B);  // 43: 7 -> 12, 44: 7 -> 13
    MODIFY(data[0x1B2], 0x79, 0xDB);  // 44: 7 -> 13, 45: 8 -> 14
    MODIFY(data[0x1B3], 0x96, 0xBA);  // 45: 8 -> 14, 46: 6 -> 15
    MODIFY(data[0x1B4], 0xA8, 0x2F);  // 47: 9 -> 16, 48: 7 -> 17
    MODIFY(data[0x1B5], 0xB9, 0x1C);  // 48: 7 -> 17, 49: 12 -> 18
    MODIFY(data[0x1B6], 0xAA, 0xCB);  // 49: 12 -> 18, 50: 11 -> 19
    MODIFY(data[0x1B7], 0x2F, 0x33);  // 51: 16 -> 20, 52: 13 -> 21
    MODIFY(data[0x1B8], 0xFB, 0x5D);  // 52: 13 -> 21, 53: 16 -> 22
    MODIFY(data[0x1B9], 0xAE, 0xDB);  // 53: 16 -> 22, 54: 12 -> 23
    MODIFY(data[0x1BA], 0x2E, 0x37);  // 55: 15 -> 24, 56: 13 -> 25
    MODIFY(data[0x1BB], 0xEB, 0x9E);  // 56: 13 -> 25, 57: 15 -> 26
    MODIFY(data[0x1BC], 0xAE, 0xEB);  // 57: 15 -> 26, 58: 12 -> 27
    MODIFY(data[0x1BD], 0xED, 0x3B);  // 59: 14 -> 28, 60: 12 -> 29
    MODIFY(data[0x1BE], 0xEA, 0xDF);  // 60: 12 -> 29, 61: 15 -> 30
    MODIFY(data[0x1BF], 0xBA, 0xFB);  // 61: 15 -> 30, 62: 15 -> 31
    MODIFY(data[0x1C0], 0x0E, 0x1F);  // 63: 15 -> 32

    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_FALSE(vorbis_open_buffer(data, size, 0, &error));
    EXPECT_EQ(error, VORBIS_ERROR_DECODE_SETUP_FAILED);

    free(data);
    return EXIT_SUCCESS;
}
