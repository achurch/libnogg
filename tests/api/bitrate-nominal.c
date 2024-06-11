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


static int32_t read(void *opaque, void *buf, int32_t len)
{
    return fread(buf, 1, len, (FILE *)opaque);
}


int main(void)
{
    FILE *f;
    vorbis_t *vorbis;
    vorbis_error_t error = (vorbis_error_t)-1;

    EXPECT(f = fopen("tests/data/bitrate-123.ogg", "rb"));
    EXPECT(vorbis = vorbis_open_callbacks(
               ((const vorbis_callbacks_t){.read = read}), f, 0, &error));
    EXPECT_EQ(error, VORBIS_NO_ERROR);

    EXPECT_EQ(vorbis_channels(vorbis), 1);
    EXPECT_EQ(vorbis_rate(vorbis), 4000);
    EXPECT_EQ(vorbis_length(vorbis), -1);
    EXPECT_EQ(vorbis_bitrate(vorbis), 123);

    vorbis_close(vorbis);
    fclose(f);
    return EXIT_SUCCESS;
}
