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

static const float expected_pcm_mid[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

#define expected_pcm expected_pcm_end
#include "tests/data/long-short_last10_float.h"  // Defines expected_pcm_end[].
#undef expected_pcm


int main(void)
{
    FILE *f;
    uint8_t *data;
    long size;
    EXPECT(f = fopen("tests/data/split-packet-non-audio.ogg", "rb"));
    EXPECT_EQ(fseek(f, 0, SEEK_END), 0);
    EXPECT_EQ(size = ftell(f), 0xFF6);
    EXPECT_EQ(fseek(f, 0, SEEK_SET), 0);
    EXPECT(data = malloc(size));
    EXPECT_EQ(fread(data, 1, size, f), size);
    fclose(f);
    /* Instead of a non-audio packet split across two pages, create a
     * non-audio packet of two segments in a single page.  The parser
     * should not treat the second segment as a new packet. */
    MODIFY(data[0xE58], 0xC0, 0x40);
    MODIFY(data[0xE68], 0x57, 0xE2);
    MODIFY(data[0xE69], 0xB6, 0x76);
    MODIFY(data[0xE6A], 0xAB, 0x67);
    MODIFY(data[0xE6B], 0x00, 0x4F);
    MODIFY(data[0xE6F], 0x01, 0xFF);
    MODIFY(data[0xE70], 0xFF, 0x01);
    MODIFY(data[0xE73], 0x00, 0xFF);
    MODIFY(data[0xE74], 0xFF, 0x00);
    MODIFY(data[0xF78], 0x05, 0x04);
    MODIFY(data[0xF89], 0x49, 0xFC);
    MODIFY(data[0xF8A], 0x13, 0x1E);
    MODIFY(data[0xF8B], 0x74, 0xB8);
    MODIFY(data[0xF8C], 0x80, 0x9D);

    vorbis_t *vorbis;
    EXPECT(vorbis = vorbis_open_buffer(data, size, 0, NULL));
    EXPECT(vorbis_seek(vorbis, 704));
    float pcm[789];
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 789, NULL), 788);
    COMPARE_PCM_FLOAT(pcm, expected_pcm_mid, 10);
    COMPARE_PCM_FLOAT(pcm+778, expected_pcm_end, 10);
    vorbis_close(vorbis);

    free(data);
    return EXIT_SUCCESS;
}
