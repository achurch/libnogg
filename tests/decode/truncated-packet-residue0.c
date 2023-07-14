/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2023 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include "include/nogg.h"
#define PCM_FLOAT_ERROR  (1.0e-4f)  // Significant variation with this stream.
#include "tests/common.h"

#include "tests/data/6ch-moving-sine-floor0_float.h"  // Defines expected_pcm[].


int main(void)
{
    FILE *f;
    uint8_t *data;
    long size;
    EXPECT(f = fopen("tests/data/6ch-moving-sine-floor0.ogg", "rb"));
    EXPECT_EQ(fseek(f, 0, SEEK_END), 0);
    EXPECT_GT(size = ftell(f), 0);
    EXPECT_EQ(fseek(f, 0, SEEK_SET), 0);
    EXPECT(data = malloc(size));
    EXPECT_EQ(fread(data, 1, size, f), size);
    fclose(f);
    MODIFY(data[0x1A8B], 0x0E, 0x0D);
    size -= data[0x1A99];
    memmove(&data[0x1A99], &data[0x1A9A], size-0x1A9A);
    size--;
    MODIFY(data[0x1A98], 0x22, 0x18);
    size -= 10;

    vorbis_t *vorbis;
    EXPECT(vorbis = vorbis_open_buffer(data, size, 0, NULL));

    static float pcm[2817*6];  // Might be too big for the stack.
    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 2817, &error), 2816);
    EXPECT_EQ(error, VORBIS_ERROR_STREAM_END);
    COMPARE_PCM_FLOAT(pcm, expected_pcm, 2560*6);

    vorbis_close(vorbis);
    free(data);
    return EXIT_SUCCESS;
}
