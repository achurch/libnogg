/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include "include/nogg.h"
#include "include/test.h"

#include "tests/data/long-short_last10_float.h"  // Defines expected_pcm[].


int main(void)
{
    FILE *f;
    uint8_t *data;
    long size;
    EXPECT_TRUE(f = fopen("tests/data/split-packet.ogg", "rb"));
    EXPECT_EQ(fseek(f, 0, SEEK_END), 0);
    EXPECT_EQ(size = ftell(f), 0xEF4);
    EXPECT_EQ(fseek(f, 0, SEEK_SET), 0);
    EXPECT_TRUE(data = malloc(size));
    EXPECT_EQ(fread(data, 1, size, f), size);
    fclose(f);
    EXPECT_TRUE(memcmp(&data[0xE52], "OggS"/*\0*/, 5) == 0);

    /* Make sure the broken page header doesn't block decoding. */
    vorbis_set_options(VORBIS_OPTION_SCAN_FOR_NEXT_PAGE);

    for (int i = 0xE52; i <= 0xE56; i++) {
        const uint8_t old_byte = data[i];
        data[i] = 0xFF;

        vorbis_t *vorbis;
        EXPECT_TRUE(vorbis = vorbis_open_from_buffer(data, size, NULL));
        EXPECT_TRUE(vorbis_seek(vorbis, 1482-128));
        float pcm[10];
        EXPECT_EQ(vorbis_read_float(vorbis, pcm, 10, NULL), 10);
        COMPARE_PCM_FLOAT(pcm, expected_pcm, 10);
        vorbis_close(vorbis);

        data[i] = old_byte;
    }

    free(data);
    return EXIT_SUCCESS;
}