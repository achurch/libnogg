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

#include "tests/data/6-mode-bits_last10_float.h"  // Defines expected_pcm[].


int main(void)
{
    FILE *f;
    uint8_t *data;
    long size;
    EXPECT(f = fopen("tests/data/6-mode-bits-multipage.ogg", "rb"));
    EXPECT_EQ(fseek(f, 0, SEEK_END), 0);
    EXPECT_GT(size = ftell(f), 0);
    EXPECT_EQ(fseek(f, 0, SEEK_SET), 0);
    EXPECT(data = malloc(size));
    EXPECT_EQ(fread(data, 1, size, f), size);
    fclose(f);
    /* Set the stream length to 0x6D4 (1748), which is 20 samples past the
     * end of the last packet's window. */
    MODIFY(data[0xE56], 0x05, 0x06);

    vorbis_t *vorbis;
    EXPECT(vorbis = vorbis_open_buffer(data, size, 0, NULL));

    float pcm[1749];
    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 1749, &error), 1600);
    EXPECT_EQ(error, VORBIS_ERROR_STREAM_END);
    COMPARE_PCM_FLOAT(&pcm[1482], expected_pcm, 10);

    vorbis_close(vorbis);
    free(data);
    return EXIT_SUCCESS;
}
