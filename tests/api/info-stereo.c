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


int main(void)
{
    vorbis_t *vorbis;
    EXPECT_TRUE(vorbis = vorbis_open_file(
                    "tests/data/square-stereo.ogg", 0, NULL));

    EXPECT_EQ(vorbis_channels(vorbis), 2);
    EXPECT_EQ(vorbis_rate(vorbis), 4000);
    EXPECT_EQ(vorbis_length(vorbis), 20);

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
