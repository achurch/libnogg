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

#include "tests/data/square_float.h"  // Defines expected_pcm[].


static int32_t read(void *opaque, void *buf, int32_t len)
{
    return fread(buf, 1, len, (FILE *)opaque);
}


int main(void)
{
    FILE *f;
    vorbis_t *vorbis;
    vorbis_error_t error = (vorbis_error_t)-1;

    EXPECT_TRUE(f = fopen("tests/data/square.ogg", "rb"));
    EXPECT_TRUE(vorbis = vorbis_open_from_callbacks(
                    ((const vorbis_callbacks_t){.read = read}), f, &error));
    EXPECT_EQ(error, VORBIS_NO_ERROR);

    float pcm[40];
    error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 40, &error), 40);
    EXPECT_EQ(error, VORBIS_NO_ERROR);
    COMPARE_PCM_FLOAT(pcm, expected_pcm, 40);

    EXPECT_FALSE(vorbis_seek(vorbis, 0));
    error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 1, &error), 0);
    EXPECT_EQ(error, VORBIS_ERROR_STREAM_END);

    vorbis_close(vorbis);

    fclose(f);
    return EXIT_SUCCESS;
}
