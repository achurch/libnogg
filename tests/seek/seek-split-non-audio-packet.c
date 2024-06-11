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

    vorbis_t *vorbis;
    EXPECT(vorbis = vorbis_open_buffer(data, size, 0, NULL));
    EXPECT(vorbis_seek(vorbis, 960));
    float pcm[533];
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 533, NULL), 532);
    COMPARE_PCM_FLOAT(pcm, expected_pcm_mid, 10);
    COMPARE_PCM_FLOAT(pcm+522, expected_pcm_end, 10);
    vorbis_close(vorbis);

    free(data);
    return EXIT_SUCCESS;
}
