/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2023 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include "include/nogg.h"
#include "tests/common.h"


int main(void)
{
    vorbis_t *vorbis;
    EXPECT(vorbis = TEST___open_file("tests/data/zero-length.ogg", 0, NULL));

    EXPECT_EQ(vorbis_channels(vorbis), 2);
    EXPECT_EQ(vorbis_rate(vorbis), 44100);
    EXPECT_EQ(vorbis_length(vorbis), 0);
    EXPECT_EQ(vorbis_bitrate(vorbis), 112000);

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
