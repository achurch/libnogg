/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2019 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include "include/nogg.h"
#include "tests/common.h"

#include "tests/data/square_float.h"  // Defines expected_pcm[].


static int64_t length(void *opaque)
{
    return -1;
}

static int64_t tell(void *opaque)
{
    return -1;
}

static void seek(void *opaque, int64_t offset)
{
}

static int32_t read(void *opaque, void *buf, int32_t len)
{
    return fread(buf, 1, len, (FILE *)opaque);
}


int main(void)
{
    FILE *f;
    EXPECT(f = fopen("tests/data/square-interleaved.ogg", "rb"));

    vorbis_t *vorbis;
    vorbis_error_t error;
    float pcm[41];

    /* Check both implicitly-unseekable (no length callback) and
     * explicitly-unseekable (length callback returns -1) cases. */

    EXPECT(vorbis = vorbis_open_callbacks(
               ((const vorbis_callbacks_t){.read = read}),
               f, 0, NULL));
    error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 41, &error), 40);
    EXPECT_EQ(error, VORBIS_ERROR_STREAM_END);
    COMPARE_PCM_FLOAT(pcm, expected_pcm, 40);
    vorbis_close(vorbis);

    fseek(f, 0, SEEK_SET);

    EXPECT(vorbis = vorbis_open_callbacks(
               ((const vorbis_callbacks_t){.length = length, .tell = tell,
                                           .seek = seek, .read = read}),
               f, 0, NULL));
    error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 41, &error), 40);
    EXPECT_EQ(error, VORBIS_ERROR_STREAM_END);
    COMPARE_PCM_FLOAT(pcm, expected_pcm, 40);
    vorbis_close(vorbis);

    fclose(f);
    return EXIT_SUCCESS;
}
