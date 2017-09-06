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
    EXPECT(f = fopen("tests/data/square.ogg", "rb"));
    EXPECT_EQ(fseek(f, 0, SEEK_END), 0);
    EXPECT_GT(size = ftell(f), 0);
    EXPECT_EQ(fseek(f, 0, SEEK_SET), 0);
    EXPECT(data = malloc(size));
    EXPECT_EQ(fread(data, 1, size, f), size);
    fclose(f);

    /* Generate a fake codebook which is just valid enough to reach the
     * point at which code might fail due to multiplication overflow
     * (note that 641 * 6700417 == 0x1_0000_0001). */
    EXPECT(memcmp(data+0xC1, "BCV", 3) == 0);
    data[0xC4] = 641>>0 & 0xFF;
    data[0xC5] = 641>>8 & 0xFF;
    data[0xC6] = 6700417>>0 & 0xFF;
    data[0xC7] = 6700417>>8 & 0xFF;
    data[0xC8] = 6700417>>16 & 0xFF;
    /* The codebook will have (8388608-6700417) 22-bit entries and the
     * remainder as 23-bit entries. */
    uint32_t offset = 0xC9;
    unsigned int acc = 0x2B;  // ordered, current_length_1=21
    unsigned int bits = 6;
    acc |= (8388608-6700417) << bits;
    bits += 23;
    while (bits >= 8) {
        data[offset++] = acc & 0xFF;
        acc >>= 8;
        bits -= 8;
    }
    acc |= (6700417-(8388608-6700417)) << bits;
    bits += 23;
    while (bits >= 8) {
        data[offset++] = acc & 0xFF;
        acc >>= 8;
        bits -= 8;
    }
    /* Set a lookup type of 2 and fill out the rest of the header fields.
     * We don't worry about the actual vector values because any failure
     * will have happened before that point. */
    acc |= 2 << bits;
    bits += 4;  // lookup_type
    bits += 32;  // minimum_value
    bits += 32;  // delta_value
    bits += 4;  // value_bits
    while (bits >= 8) {
        data[offset++] = acc & 0xFF;
        acc >>= 8;
        bits -= 8;
    }
    acc |= 1 << bits;
    bits += 1;  // sequence_p
    if (bits >= 8) {
        data[offset++] = acc & 0xFF;
        acc >>= 8;
        bits -= 8;
    }
    data[offset++] = acc;
    EXPECT(offset <= size);

    /* Call the setup code.  This will fail in any case, but it should not
     * crash due to overrunning a buffer which has been allocated too small
     * due to multiplication overflow. */
    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_FALSE(vorbis_open_buffer(data, size, 0, &error));
    EXPECT_EQ(error, VORBIS_ERROR_DECODE_SETUP_FAILED);

    free(data);
    return EXIT_SUCCESS;
}
