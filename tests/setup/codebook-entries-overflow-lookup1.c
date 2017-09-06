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
    const uint32_t codebook_size = 5000000;
    uint8_t *codebook;
    EXPECT(codebook = malloc(codebook_size));
    memcpy(codebook, "BCV", 3);
    codebook[3] = 641>>0 & 0xFF;
    codebook[4] = 641>>8 & 0xFF;
    codebook[5] = 6700417>>0 & 0xFF;
    codebook[6] = 6700417>>8 & 0xFF;
    codebook[7] = 6700417>>16 & 0xFF;
    /* The codebook will have (8388608-6700417) 22-bit entries and the
     * remainder as 23-bit entries. */
    uint32_t offset = 8;
    unsigned int acc = 0x00;  // !ordered, !sparse
    unsigned int bits = 2;
    for (int i = 0; i < 8388608-6700417; i++) {
        acc |= (22-1) << bits;
        bits += 5;
        if (bits >= 8) {
            codebook[offset++] = acc & 0xFF;
            acc >>= 8;
            bits -= 8;
        }
    }
    for (int i = 8388608-6700417; i < 6700417; i++) {
        acc |= (23-1) << bits;
        bits += 5;
        if (bits >= 8) {
            codebook[offset++] = acc & 0xFF;
            acc >>= 8;
            bits -= 8;
        }
    }
    /* Set a lookup type of 1 and fill out the rest of the data with zeroes. */
    acc |= 1 << bits;
    bits += 4;
    if (bits >= 8) {
        codebook[offset++] = acc & 0xFF;
        acc >>= 8;
        bits -= 8;
    }
    codebook[offset++] = acc;
    EXPECT(offset <= codebook_size);
    memset(codebook + offset, 0, codebook_size - offset);

    /* Copy the bogus codebook into the file, filling in Ogg pages as
     * appropriate.  We don't bother with checksums because the relevant
     * code doesn't check them. */
    size = codebook_size*5/4;  // Make room for page headers.
    EXPECT(data = realloc(data, size));
    EXPECT(memcmp(data+0x3A, "OggS", 4) == 0);
    EXPECT(memcmp(data+0x54,
                  "\x0B\x59\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xB5", 12) == 0);
    data[0x5F] = 0xFF;
    EXPECT(memcmp(data+0xB9, "\x05vorbis\x12", 8) == 0);
    int page_left = (0x59 + 0xFF*10) - (0xC1 - 0x60);
    memcpy(data+0xC1, codebook, page_left);
    uint8_t *outptr = data + (0xC1 + page_left);
    int page = 1;
    for (offset = page_left; offset+255 <= codebook_size; offset += 255) {
        memcpy(outptr, "OggS\0\1\0\0\0\0\0\0\0\0", 14);
        memcpy(outptr+14, data+0x48, 4);  // Copy bitstream ID.
        page++;
        outptr[18] = page>>0 & 0xFF;
        outptr[19] = page>>8 & 0xFF;
        outptr[20] = page>>16 & 0xFF;
        outptr[21] = page>>24 & 0xFF;
        memcpy(outptr+22, "\0\0\0\0\x01\xFF", 6);
        memcpy(outptr+28, codebook+offset, 255);
        outptr += 28+255;
    }

    /* Call the setup code.  This will fail in any case, but it should not
     * crash due to overrunning a buffer which has been allocated too small
     * due to multiplication overflow. */
    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_FALSE(vorbis_open_buffer(data, size, 0, &error));
    EXPECT_EQ(error, VORBIS_ERROR_DECODE_SETUP_FAILED);

    free(data);
    return EXIT_SUCCESS;
}
