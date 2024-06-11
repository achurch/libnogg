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

    /* Change codebook 3 to an empty lookup-type-1 codebook.  The
     * fixed-size data for a vector codebook requires 115 bits, while the
     * original codebook 3 has 146 bits; that leaves 31 bits to be divided
     * among the codebook entries.  31 is inconveniently prime, so we also
     * expand codebook 2 by one bit to give us 30 bits for entry data.
     * Just for kicks, we also make codebook 2 empty (though that doesn't
     * test any additional code paths). */
    MODIFY(data[0x17A], 0x0A, 0x29);  // Codebook 2: 10 -> 41 entries
    MODIFY(data[0x17D], 0xC2, 0x02);  // !ordered, sparse, all not present
    MODIFY(data[0x17E], 0x50, 0x00);
    MODIFY(data[0x17F], 0x0C, 0x00);
    MODIFY(data[0x180], 0x45, 0x00);
    MODIFY(data[0x181], 0x51, 0x00);
    MODIFY(data[0x182], 0x80, 0x00);  // Lookup type 0
    MODIFY(data[0x183], 0xD0, 0xA1);  // Codebook 3: magic number (shifted)
    MODIFY(data[0x184], 0x90, 0x21);
    MODIFY(data[0x185], 0x55, 0xAB);
    MODIFY(data[0x186], 0x00, 0x00);
    MODIFY(data[0x187], 0x40, 0x80);  // 25 -> 15 entries
    MODIFY(data[0x188], 0x06, 0x07);
    MODIFY(data[0x189], 0x00, 0x00);
    MODIFY(data[0x18A], 0x80, 0x00);  // Sparse flag moves to next byte
    MODIFY(data[0x18B], 0x00, 0x01);  // New sparse flag, entries not present
    MODIFY(data[0x18C], 0x14, 0x00);
    MODIFY(data[0x18D], 0x45, 0x01);  // Lookup type 1 (floats are don't-cares)
    MODIFY(data[0x195], 0x24, 0x00);  // 1 bit per lookup entry
    MODIFY(data[0x196], 0xCB, 0x01);  // sequence_p set (to test assertion)

    /* Call the setup code.  The stream will not decode correctly, but
     * codebook processing during setup should should not fail the
     * !(!handle->divides_in_codebook && book->lookup_type != 2) assertion. */
    vorbis_t *vorbis;
    EXPECT(vorbis = vorbis_open_buffer(data, size, 0, NULL));

    vorbis_close(vorbis);
    free(data);
    return EXIT_SUCCESS;
}
